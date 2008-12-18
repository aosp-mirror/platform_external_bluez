/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2006-2007  Nokia Corporation
 *  Copyright (C) 2004-2008  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/poll.h>

#include "ipc.h"
#include "sbc.h"
#include "rtp.h"
#include "liba2dp.h"

#define LOG_TAG "A2DP"
#include <utils/Log.h>

// #define ENABLE_DEBUG

#define BUFFER_SIZE 2048

#ifdef ENABLE_DEBUG
#define DBG LOGD
#else
#define DBG(fmt, arg...)
#endif


#ifndef MIN
# define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
# define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define MAX_BITPOOL 64
#define MIN_BITPOOL 2

#define SNDERR LOGE

struct bluetooth_a2dp {
	sbc_capabilities_t sbc_capabilities;
	sbc_t sbc;				/* Codec data */
	int sbc_initialized;			/* Keep track if the encoder is initialized */
	int codesize;				/* SBC codesize */
	int samples;				/* Number of encoded samples */
	uint8_t buffer[BUFFER_SIZE];		/* Codec transfer buffer */
	int count;				/* Codec transfer buffer counter */

	int nsamples;				/* Cumulative number of codec samples */
	uint16_t seq_num;			/* Cumulative packet sequence */
	int frame_count;			/* Current frames in buffer*/
};


struct bluetooth_data {
	volatile long hw_ptr;
	int transport;					/* chosen transport SCO or AD2P */
	int link_mtu;					/* MTU for selected transport channel */
	volatile struct pollfd stream;			/* Audio stream filedescriptor */
	struct pollfd server;				/* Audio daemon filedescriptor */
	uint8_t buffer[BUFFER_SIZE];		/* Encoded transfer buffer */
	int count;					/* Transfer buffer counter */
	struct bluetooth_a2dp a2dp;			/* A2DP data */

	sig_atomic_t reset;             /* Request XRUN handling */
	
	char    address[20];
};



static int audioservice_send(int sk, const bt_audio_msg_header_t *msg);
static int audioservice_expect(int sk, bt_audio_msg_header_t *outmsg,
				int expected_type);


static void bluetooth_exit(struct bluetooth_data *data)
{
	struct bluetooth_a2dp *a2dp = &data->a2dp;

	if (data->server.fd >= 0)
		bt_audio_service_close(data->server.fd);

	if (data->stream.fd >= 0)
		close(data->stream.fd);

	if (a2dp->sbc_initialized)
		sbc_finish(&a2dp->sbc);
}

static int bluetooth_prepare(struct bluetooth_data *data)
{
	char c = 'w';
	char buf[BT_AUDIO_IPC_PACKET_SIZE];
	struct bt_streamstart_req *start_req = (void*) buf;
	bt_audio_rsp_msg_header_t *rsp_hdr = (void*) buf;
	struct bt_streamfd_ind *streamfd_ind = (void*) buf;
	int opt_name, err;
	struct timeval t = { 100, 0 };
	int buffer;

	data->reset = 0;
	data->hw_ptr = 0;

	/* send start */
	memset(start_req, 0, BT_AUDIO_IPC_PACKET_SIZE);
	start_req->h.msg_type = BT_STREAMSTART_REQ;

	err = audioservice_send(data->server.fd, &start_req->h);
	if (err < 0)
		return err;

	err = audioservice_expect(data->server.fd, &rsp_hdr->msg_h,
					BT_STREAMSTART_RSP);
	if (err < 0)
		return err;

	if (rsp_hdr->posix_errno != 0) {
		SNDERR("BT_START failed : %s(%d)",
					strerror(rsp_hdr->posix_errno),
					rsp_hdr->posix_errno);
		return -rsp_hdr->posix_errno;
	}

	err = audioservice_expect(data->server.fd, &streamfd_ind->h,
					BT_STREAMFD_IND);
	if (err < 0)
		return err;

	if (data->stream.fd >= 0)
		close(data->stream.fd);

	data->stream.fd = bt_audio_service_get_data_fd(data->server.fd);
	if (data->stream.fd < 0) {
		return -errno;
	}

	if (setsockopt(data->stream.fd, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t)) < 0)
		return -errno;

	/* disable buffering to reduce latency when pausing or changing volume */
	buffer = 0;
	if (setsockopt(data->stream.fd, SOL_SOCKET, SO_SNDBUF, &buffer, sizeof(buffer)) < 0)
		return -errno;

	return 0;
}

static uint8_t default_bitpool(uint8_t freq, uint8_t mode)
{
	switch (freq) {
	case BT_SBC_SAMPLING_FREQ_16000:
	case BT_SBC_SAMPLING_FREQ_32000:
		return 53;
	case BT_SBC_SAMPLING_FREQ_44100:
		switch (mode) {
		case BT_A2DP_CHANNEL_MODE_MONO:
		case BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL:
			return 31;
		case BT_A2DP_CHANNEL_MODE_STEREO:
		case BT_A2DP_CHANNEL_MODE_JOINT_STEREO:
			return 53;
		default:
			DBG("Invalid channel mode %u", mode);
			return 53;
		}
	case BT_SBC_SAMPLING_FREQ_48000:
		switch (mode) {
		case BT_A2DP_CHANNEL_MODE_MONO:
		case BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL:
			return 29;
		case BT_A2DP_CHANNEL_MODE_STEREO:
		case BT_A2DP_CHANNEL_MODE_JOINT_STEREO:
			return 51;
		default:
			DBG("Invalid channel mode %u", mode);
			return 51;
		}
	default:
		DBG("Invalid sampling freq %u", freq);
		return 53;
	}
}

static int bluetooth_a2dp_init(struct bluetooth_data *data, int rate, int channels)
{
	sbc_capabilities_t *cap = &data->a2dp.sbc_capabilities;
	unsigned int max_bitpool, min_bitpool;
	int dir;

	switch (rate) {
	case 48000:
		cap->frequency = BT_SBC_SAMPLING_FREQ_48000;
		break;
	case 44100:
		cap->frequency = BT_SBC_SAMPLING_FREQ_44100;
		break;
	case 32000:
		cap->frequency = BT_SBC_SAMPLING_FREQ_32000;
		break;
	case 16000:
		cap->frequency = BT_SBC_SAMPLING_FREQ_16000;
		break;
	default:
		DBG("Rate %d not supported", rate);
		return -1;
	}

	if (channels == 2) {
		if (cap->channel_mode & BT_A2DP_CHANNEL_MODE_JOINT_STEREO)
			cap->channel_mode = BT_A2DP_CHANNEL_MODE_JOINT_STEREO;
		else if (cap->channel_mode & BT_A2DP_CHANNEL_MODE_STEREO)
			cap->channel_mode = BT_A2DP_CHANNEL_MODE_STEREO;
		else if (cap->channel_mode & BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL)
			cap->channel_mode = BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL;
	} else {
		if (cap->channel_mode & BT_A2DP_CHANNEL_MODE_MONO)
			cap->channel_mode = BT_A2DP_CHANNEL_MODE_MONO;
	}

	if (!cap->channel_mode) {
		DBG("No supported channel modes");
		return -1;
	}

	if (cap->block_length & BT_A2DP_BLOCK_LENGTH_16)
		cap->block_length = BT_A2DP_BLOCK_LENGTH_16;
	else if (cap->block_length & BT_A2DP_BLOCK_LENGTH_12)
		cap->block_length = BT_A2DP_BLOCK_LENGTH_12;
	else if (cap->block_length & BT_A2DP_BLOCK_LENGTH_8)
		cap->block_length = BT_A2DP_BLOCK_LENGTH_8;
	else if (cap->block_length & BT_A2DP_BLOCK_LENGTH_4)
		cap->block_length = BT_A2DP_BLOCK_LENGTH_4;
	else {
		DBG("No supported block lengths");
		return -1;
	}

	if (cap->subbands & BT_A2DP_SUBBANDS_8)
		cap->subbands = BT_A2DP_SUBBANDS_8;
	else if (cap->subbands & BT_A2DP_SUBBANDS_4)
		cap->subbands = BT_A2DP_SUBBANDS_4;
	else {
		DBG("No supported subbands");
		return -1;
	}

	if (cap->allocation_method & BT_A2DP_ALLOCATION_LOUDNESS)
		cap->allocation_method = BT_A2DP_ALLOCATION_LOUDNESS;
	else if (cap->allocation_method & BT_A2DP_ALLOCATION_SNR)
		cap->allocation_method = BT_A2DP_ALLOCATION_SNR;

		min_bitpool = MAX(MIN_BITPOOL, cap->min_bitpool);
		max_bitpool = MIN(default_bitpool(cap->frequency,
					cap->channel_mode),
					cap->max_bitpool);

	cap->min_bitpool = min_bitpool;
	cap->max_bitpool = max_bitpool;

DBG("bluetooth_a2dp_init bottom:\n  channel_mode: %d\n  frequency: %d\n  allocation_method: %d\n  subbands: %d\n  block_length: %d\n  min_bitpool: %d\n  max_bitpool: %d\n  ",
    cap->channel_mode, cap->frequency, cap->allocation_method, cap->subbands, 
    cap->block_length, cap->min_bitpool, cap->max_bitpool);


	return 0;
}

static void bluetooth_a2dp_setup(struct bluetooth_a2dp *a2dp)
{
	sbc_capabilities_t active_capabilities = a2dp->sbc_capabilities;

	if (a2dp->sbc_initialized)
		sbc_reinit(&a2dp->sbc, 0);
	else
		sbc_init(&a2dp->sbc, 0);
	a2dp->sbc_initialized = 1;

	if (active_capabilities.frequency & BT_SBC_SAMPLING_FREQ_16000)
		a2dp->sbc.frequency = SBC_FREQ_16000;

	if (active_capabilities.frequency & BT_SBC_SAMPLING_FREQ_32000)
		a2dp->sbc.frequency = SBC_FREQ_32000;

	if (active_capabilities.frequency & BT_SBC_SAMPLING_FREQ_44100)
		a2dp->sbc.frequency = SBC_FREQ_44100;

	if (active_capabilities.frequency & BT_SBC_SAMPLING_FREQ_48000)
		a2dp->sbc.frequency = SBC_FREQ_48000;

	if (active_capabilities.channel_mode & BT_A2DP_CHANNEL_MODE_MONO)
		a2dp->sbc.mode = SBC_MODE_MONO;

	if (active_capabilities.channel_mode & BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL)
		a2dp->sbc.mode = SBC_MODE_DUAL_CHANNEL;

	if (active_capabilities.channel_mode & BT_A2DP_CHANNEL_MODE_STEREO)
		a2dp->sbc.mode = SBC_MODE_STEREO;

	if (active_capabilities.channel_mode & BT_A2DP_CHANNEL_MODE_JOINT_STEREO)
		a2dp->sbc.mode = SBC_MODE_JOINT_STEREO;

	a2dp->sbc.allocation = active_capabilities.allocation_method
				== BT_A2DP_ALLOCATION_SNR ? SBC_AM_SNR
				: SBC_AM_LOUDNESS;

	switch (active_capabilities.subbands) {
	case BT_A2DP_SUBBANDS_4:
		a2dp->sbc.subbands = SBC_SB_4;
		break;
	case BT_A2DP_SUBBANDS_8:
		a2dp->sbc.subbands = SBC_SB_8;
		break;
	}

	switch (active_capabilities.block_length) {
	case BT_A2DP_BLOCK_LENGTH_4:
		a2dp->sbc.blocks = SBC_BLK_4;
		break;
	case BT_A2DP_BLOCK_LENGTH_8:
		a2dp->sbc.blocks = SBC_BLK_8;
		break;
	case BT_A2DP_BLOCK_LENGTH_12:
		a2dp->sbc.blocks = SBC_BLK_12;
		break;
	case BT_A2DP_BLOCK_LENGTH_16:
		a2dp->sbc.blocks = SBC_BLK_16;
		break;
	}

	a2dp->sbc.bitpool = active_capabilities.max_bitpool;
	a2dp->codesize = sbc_get_codesize(&a2dp->sbc);
	a2dp->count = sizeof(struct rtp_header) + sizeof(struct rtp_payload);
}

static int bluetooth_a2dp_hw_params(struct bluetooth_data *data, int rate, int channels)
{
	struct bluetooth_a2dp *a2dp = &data->a2dp;
	char buf[BT_AUDIO_IPC_PACKET_SIZE];
	bt_audio_rsp_msg_header_t *rsp_hdr = (void*) buf;
	struct bt_setconfiguration_req *setconf_req = (void*) buf;
	struct bt_setconfiguration_rsp *setconf_rsp = (void*) buf;
	int err;

	err = bluetooth_a2dp_init(data, rate, channels);
	if (err < 0)
		return err;

	memset(setconf_req, 0, BT_AUDIO_IPC_PACKET_SIZE);
	setconf_req->h.msg_type = BT_SETCONFIGURATION_REQ;
	strncpy(setconf_req->device, data->address, 18);
	setconf_req->transport = BT_CAPABILITIES_TRANSPORT_A2DP;
	setconf_req->sbc_capabilities = a2dp->sbc_capabilities;
	setconf_req->access_mode = BT_CAPABILITIES_ACCESS_MODE_WRITE;

	err = audioservice_send(data->server.fd, &setconf_req->h);
	if (err < 0)
		return err;

	err = audioservice_expect(data->server.fd, &rsp_hdr->msg_h,
					BT_SETCONFIGURATION_RSP);
	if (err < 0)
		return err;

	if (rsp_hdr->posix_errno != 0) {
		SNDERR("BT_SETCONFIGURATION failed : %s(%d)",
					strerror(rsp_hdr->posix_errno),
					rsp_hdr->posix_errno);
		return -rsp_hdr->posix_errno;
	}

	data->transport = setconf_rsp->transport;
	data->link_mtu = setconf_rsp->link_mtu;

	/* Setup SBC encoder now we agree on parameters */
	bluetooth_a2dp_setup(a2dp);

	DBG("\tallocation=%u\n\tsubbands=%u\n\tblocks=%u\n\tbitpool=%u\n",
		a2dp->sbc.allocation, a2dp->sbc.subbands, a2dp->sbc.blocks,
		a2dp->sbc.bitpool);

	return 0;
}



static int avdtp_write(struct bluetooth_data *data)
{
	int ret = 0;
	struct rtp_header *header;
	struct rtp_payload *payload;
	struct bluetooth_a2dp *a2dp = &data->a2dp;

	header = (void *) a2dp->buffer;
	payload = (void *) (a2dp->buffer + sizeof(*header));

	memset(a2dp->buffer, 0, sizeof(*header) + sizeof(*payload));

	payload->frame_count = a2dp->frame_count;
	header->v = 2;
	header->pt = 1;
	header->sequence_number = htons(a2dp->seq_num);
	header->timestamp = htonl(a2dp->nsamples);
	header->ssrc = htonl(1);

retry:
    ret = send(data->stream.fd, a2dp->buffer, a2dp->count, MSG_DONTWAIT);
	if (ret < 0) {
		DBG("send returned %d errno %s.", ret, strerror(errno));
		ret = -errno;
		if (ret == -EAGAIN) {
			struct timespec tv;
			DBG("retry");
			tv.tv_sec = 0;
			tv.tv_nsec = 100 * 1000;
			nanosleep(&tv, NULL);
			goto retry;
		}
	}

	/* Reset buffer of data to send */
	a2dp->count = sizeof(struct rtp_header) + sizeof(struct rtp_payload);
	a2dp->frame_count = 0;
	a2dp->samples = 0;
	a2dp->seq_num++;

	return ret;
}

static long bluetooth_a2dp_write(struct bluetooth_data *data,
                const char* buffer,
				long size)
{
	struct bluetooth_a2dp *a2dp = &data->a2dp;
	long ret = 0;
	long frames_to_read, frames_left = size;
	int encoded, written;
	const char *buff;

	while (frames_left > 0) {

		if ((data->count + frames_left) <= a2dp->codesize)
			frames_to_read = frames_left;
		else
			frames_to_read = a2dp->codesize - data->count;
        buff = buffer + ret;
 
		memcpy(data->buffer + data->count, buff, frames_to_read);
		/* Remember we have some frames in the pipe now */
		data->count += frames_to_read;
		if (data->count != a2dp->codesize) {
			ret = frames_to_read;
			goto done;
		}

		/* Enough data to encode (sbc wants 1k blocks) */
		encoded = sbc_encode(&(a2dp->sbc), data->buffer, a2dp->codesize,
					a2dp->buffer + a2dp->count,
					sizeof(a2dp->buffer) - a2dp->count,
					&written);
		if (encoded <= 0) {
			DBG("Encoding error %d", encoded);
			goto done;
		}

		data->count -= encoded;
		a2dp->count += written;
		a2dp->frame_count++;
		a2dp->samples += encoded;
		a2dp->nsamples += encoded;

		/* No space left for another frame then send */
		if (a2dp->count + written >= data->link_mtu) {
			DBG("sending packet %d, count %d, link_mtu %u",
					a2dp->seq_num, a2dp->count,
					data->link_mtu);
			avdtp_write(data);
		}

		ret += frames_to_read;
		frames_left -= frames_to_read;
	}

	/* note: some ALSA apps will get confused otherwise */
	if (ret > size)
		ret = size;

done:
	DBG("returning %ld", ret);
	return ret;
}


static int audioservice_send(int sk, const bt_audio_msg_header_t *msg)
{
	int err;

	DBG("sending %s", bt_audio_strmsg(msg->msg_type));
	if (send(sk, msg, BT_AUDIO_IPC_PACKET_SIZE, 0) > 0)
		err = 0;
	else {
		err = -errno;
		SNDERR("Error sending data to audio service: %s(%d)",
			strerror(errno), errno);
	}

	return err;
}

static int audioservice_recv(int sk, bt_audio_msg_header_t *inmsg)
{
	int err;
	const char *type;

	DBG("trying to receive msg from audio service...");
	if (recv(sk, inmsg, BT_AUDIO_IPC_PACKET_SIZE, 0) > 0) {
		type = bt_audio_strmsg(inmsg->msg_type);
		if (type) {
			DBG("Received %s", type);
			err = 0;
		} else {
			err = -EINVAL;
			SNDERR("Bogus message type %d "
					"received from audio service",
					inmsg->msg_type);
		}
	} else {
		err = -errno;
		SNDERR("Error receiving data from audio service: %s(%d)",
					strerror(errno), errno);
	}

	return err;
}

static int audioservice_expect(int sk, bt_audio_msg_header_t *rsp_hdr,
				int expected_type)
{
	int err = audioservice_recv(sk, rsp_hdr);
	if (err == 0) {
		if (rsp_hdr->msg_type != expected_type) {
			err = -EINVAL;
			SNDERR("Bogus message %s received while "
					"%s was expected",
					bt_audio_strmsg(rsp_hdr->msg_type),
					bt_audio_strmsg(expected_type));
		}
	}
	return err;
}

static int bluetooth_init(struct bluetooth_data *data)
{
	int sk, err;
	char buf[BT_AUDIO_IPC_PACKET_SIZE];
	bt_audio_rsp_msg_header_t *rsp_hdr = (void*) buf;
	struct bt_getcapabilities_req *getcaps_req = (void*) buf;
	struct bt_getcapabilities_rsp *getcaps_rsp = (void*) buf;

	memset(data, 0, sizeof(struct bluetooth_data));

	data->server.fd = -1;
	data->stream.fd = -1;

	sk = bt_audio_service_open();
	if (sk <= 0) {
		err = -errno;
		goto failed;
	}

	data->server.fd = sk;
	data->server.events = POLLIN;

	memset(getcaps_req, 0, BT_AUDIO_IPC_PACKET_SIZE);
	getcaps_req->h.msg_type = BT_GETCAPABILITIES_REQ;
	getcaps_req->flags = 0;
	getcaps_req->flags |= BT_FLAG_AUTOCONNECT;
	strncpy(getcaps_req->device, data->address, 18);
	getcaps_req->transport = BT_CAPABILITIES_TRANSPORT_A2DP;

	err = audioservice_send(data->server.fd, &getcaps_req->h);
	if (err < 0)
		goto failed;

	err = audioservice_expect(data->server.fd, &rsp_hdr->msg_h, BT_GETCAPABILITIES_RSP);
	if (err < 0)
		goto failed;

	if (rsp_hdr->posix_errno != 0) {
		SNDERR("BT_GETCAPABILITIES failed : %s(%d)",
					strerror(rsp_hdr->posix_errno),
					rsp_hdr->posix_errno);
		return -rsp_hdr->posix_errno;
	}

	data->transport = getcaps_rsp->transport;

	if (getcaps_rsp->transport == BT_CAPABILITIES_TRANSPORT_A2DP) {
		data->a2dp.sbc_capabilities = getcaps_rsp->sbc_capabilities;
	}

	return 0;

failed:
	bt_audio_service_close(sk);
	return err;
}

int a2dp_init(const char* address, int rate, int channels, a2dpData* dataPtr)
{
	int err;

    *dataPtr = NULL;
    struct bluetooth_data* data = malloc(sizeof(struct bluetooth_data));
    if (!data)
        return -1;

	strncpy(data->address, address, 18);

	err = bluetooth_init(data);
	if (err < 0)
		goto error;
		
    err = bluetooth_a2dp_hw_params(data, rate, channels);
	if (err < 0) {
	    printf("bluetooth_a2dp_hw_params failed");
		goto error;
	}
    
	err = bluetooth_prepare(data);
	if (err < 0) {
	    printf("bluetooth_prepare failed");
		goto error;
	}

    *dataPtr = data;
	return 0;
   
error:
	bluetooth_exit(data);
	free(data);

	return err;
}

int a2dp_write(a2dpData d, const void* buffer, int count)
{
    struct bluetooth_data* data = (struct bluetooth_data*)d;
    return bluetooth_a2dp_write(data, buffer, count); 
}

void a2dp_cleanup(a2dpData d)
{
    struct bluetooth_data* data = (struct bluetooth_data*)d;
	bluetooth_exit(data);
	free(data);
}
