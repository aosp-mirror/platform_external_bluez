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

#define LOG_NDEBUG 0
#define LOG_TAG "A2DP"
#include <utils/Log.h>

#define ENABLE_DEBUG
/* #define ENABLE_VERBOSE */

#define BUFFER_SIZE 2048

#ifdef ENABLE_DEBUG
#define DBG LOGD
#else
#define DBG(fmt, arg...)
#endif

#ifdef ENABLE_VERBOSE
#define VDBG LOGV
#else
#define VDBG(fmt, arg...)
#endif

#ifndef MIN
# define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
# define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define MAX_BITPOOL 64
#define MIN_BITPOOL 2

#define ERR LOGE

/* Number of milliseconds worth of audio to buffer in our the data->stream.fd socket */
#define SOCK_BUFFER_MS		50

struct bluetooth_data {
	int link_mtu;					/* MTU for selected transport channel */
	struct pollfd stream;			/* Audio stream filedescriptor */
	struct pollfd server;			/* Audio daemon filedescriptor */

	sbc_capabilities_t sbc_capabilities;
	sbc_t sbc;				/* Codec data */
	int sbc_initialized;			/* Keep track if the encoder is initialized */
	int	frame_duration;			/* length of an SBC frame in microseconds */
	int codesize;				/* SBC codesize */
	int samples;				/* Number of encoded samples */
	uint8_t buffer[BUFFER_SIZE];		/* Codec transfer buffer */
	int count;				/* Codec transfer buffer counter */

	int nsamples;				/* Cumulative number of codec samples */
	uint16_t seq_num;			/* Cumulative packet sequence */
	int frame_count;			/* Current frames in buffer*/

	int		started;
	char	address[20];
	int	rate;
	int	channels;

	/* used for pacing our writes to the output socket */
	struct timeval	last_write;
	unsigned long	last_duration;

	/* true if we already set the buffer size on the data->stream.fd socket */
	int adjusted_sock_buffer;
};


static int audioservice_send(int sk, const bt_audio_msg_header_t *msg);
static int audioservice_expect(int sk, bt_audio_msg_header_t *outmsg,
				int expected_type);
static int bluetooth_a2dp_hw_params(struct bluetooth_data *data);


static void bluetooth_exit(struct bluetooth_data *data)
{
	if (data->server.fd >= 0)
		bt_audio_service_close(data->server.fd);

	if (data->stream.fd >= 0)
		close(data->stream.fd);

	if (data->sbc_initialized)
		sbc_finish(&data->sbc);
}

static int bluetooth_start(struct bluetooth_data *data)
{
	char c = 'w';
	char buf[BT_AUDIO_IPC_PACKET_SIZE];
	struct bt_streamstart_req *start_req = (void*) buf;
	bt_audio_rsp_msg_header_t *rsp_hdr = (void*) buf;
	struct bt_streamfd_ind *streamfd_ind = (void*) buf;
	int opt_name, err;
	int retry = 0;

retry:
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
		ERR("BT_START failed : %s(%d)",
					strerror(rsp_hdr->posix_errno),
					rsp_hdr->posix_errno);
		
		/* if the connection dropped, we may need to reset the configuration */
		if (!retry) {
			retry = 1;
			if (bluetooth_a2dp_hw_params(data) == 0)
				goto retry;
		}

		return -rsp_hdr->posix_errno;
	}

	err = audioservice_expect(data->server.fd, &streamfd_ind->h,
					BT_STREAMFD_IND);
	if (err < 0)
		return err;

	if (data->stream.fd >= 0) {
		close(data->stream.fd);
		data->stream.fd = -1;
		data->adjusted_sock_buffer = 0;
	}

	data->stream.fd = bt_audio_service_get_data_fd(data->server.fd);
	if (data->stream.fd < 0) {
		return -errno;
	}
	data->stream.events = POLLOUT;

	data->count = sizeof(struct rtp_header) + sizeof(struct rtp_payload);
	data->frame_count = 0;
	data->samples = 0;
	data->nsamples = 0;
	data->seq_num = 0;
	data->frame_count = 0;

	return 0;
}

static int bluetooth_stop(struct bluetooth_data *data)
{
	char buf[BT_AUDIO_IPC_PACKET_SIZE];
	struct bt_streamstop_req *stop_req = (void*) buf;
	bt_audio_rsp_msg_header_t *rsp_hdr = (void*) buf;
	int err;

	data->started = 0;

	if (data->stream.fd >= 0) {
		close(data->stream.fd);
		data->stream.fd = 0;
	}

	/* send stop request */
	memset(stop_req, 0, BT_AUDIO_IPC_PACKET_SIZE);
	stop_req->h.msg_type = BT_STREAMSTOP_REQ;

	err = audioservice_send(data->server.fd, &stop_req->h);
	if (err < 0)
		return err;

	err = audioservice_expect(data->server.fd, &rsp_hdr->msg_h,
					BT_STREAMSTOP_RSP);
	if (err < 0)
		return err;

	if (rsp_hdr->posix_errno != 0) {
		ERR("BT_STREAMSTOP failed : %s(%d)",
					strerror(rsp_hdr->posix_errno),
					rsp_hdr->posix_errno);
		return -rsp_hdr->posix_errno;
	}

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
			ERR("Invalid channel mode %u", mode);
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
			ERR("Invalid channel mode %u", mode);
			return 51;
		}
	default:
		ERR("Invalid sampling freq %u", freq);
		return 53;
	}
}

static int bluetooth_a2dp_init(struct bluetooth_data *data)
{
	sbc_capabilities_t *cap = &data->sbc_capabilities;
	unsigned int max_bitpool, min_bitpool;
	int dir;

	switch (data->rate) {
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
		ERR("Rate %d not supported", data->rate);
		return -1;
	}

	if (data->channels == 2) {
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
		ERR("No supported channel modes");
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
		ERR("No supported block lengths");
		return -1;
	}

	if (cap->subbands & BT_A2DP_SUBBANDS_8)
		cap->subbands = BT_A2DP_SUBBANDS_8;
	else if (cap->subbands & BT_A2DP_SUBBANDS_4)
		cap->subbands = BT_A2DP_SUBBANDS_4;
	else {
		ERR("No supported subbands");
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

	return 0;
}

static void bluetooth_a2dp_setup(struct bluetooth_data *data)
{
	sbc_capabilities_t active_capabilities = data->sbc_capabilities;

	if (data->sbc_initialized)
		sbc_reinit(&data->sbc, 0);
	else
		sbc_init(&data->sbc, 0);
	data->sbc_initialized = 1;

	if (active_capabilities.frequency & BT_SBC_SAMPLING_FREQ_16000)
		data->sbc.frequency = SBC_FREQ_16000;

	if (active_capabilities.frequency & BT_SBC_SAMPLING_FREQ_32000)
		data->sbc.frequency = SBC_FREQ_32000;

	if (active_capabilities.frequency & BT_SBC_SAMPLING_FREQ_44100)
		data->sbc.frequency = SBC_FREQ_44100;

	if (active_capabilities.frequency & BT_SBC_SAMPLING_FREQ_48000)
		data->sbc.frequency = SBC_FREQ_48000;

	if (active_capabilities.channel_mode & BT_A2DP_CHANNEL_MODE_MONO)
		data->sbc.mode = SBC_MODE_MONO;

	if (active_capabilities.channel_mode & BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL)
		data->sbc.mode = SBC_MODE_DUAL_CHANNEL;

	if (active_capabilities.channel_mode & BT_A2DP_CHANNEL_MODE_STEREO)
		data->sbc.mode = SBC_MODE_STEREO;

	if (active_capabilities.channel_mode & BT_A2DP_CHANNEL_MODE_JOINT_STEREO)
		data->sbc.mode = SBC_MODE_JOINT_STEREO;

	data->sbc.allocation = active_capabilities.allocation_method
				== BT_A2DP_ALLOCATION_SNR ? SBC_AM_SNR
				: SBC_AM_LOUDNESS;

	switch (active_capabilities.subbands) {
	case BT_A2DP_SUBBANDS_4:
		data->sbc.subbands = SBC_SB_4;
		break;
	case BT_A2DP_SUBBANDS_8:
		data->sbc.subbands = SBC_SB_8;
		break;
	}

	switch (active_capabilities.block_length) {
	case BT_A2DP_BLOCK_LENGTH_4:
		data->sbc.blocks = SBC_BLK_4;
		break;
	case BT_A2DP_BLOCK_LENGTH_8:
		data->sbc.blocks = SBC_BLK_8;
		break;
	case BT_A2DP_BLOCK_LENGTH_12:
		data->sbc.blocks = SBC_BLK_12;
		break;
	case BT_A2DP_BLOCK_LENGTH_16:
		data->sbc.blocks = SBC_BLK_16;
		break;
	}

	data->sbc.bitpool = active_capabilities.max_bitpool;
	data->codesize = sbc_get_codesize(&data->sbc);
	data->frame_duration = sbc_get_frame_duration(&data->sbc);
}

static int bluetooth_a2dp_hw_params(struct bluetooth_data *data)
{
	char buf[BT_AUDIO_IPC_PACKET_SIZE];
	bt_audio_rsp_msg_header_t *rsp_hdr = (void*) buf;
	struct bt_setconfiguration_req *setconf_req = (void*) buf;
	struct bt_setconfiguration_rsp *setconf_rsp = (void*) buf;
	int err;

	err = bluetooth_a2dp_init(data);
	if (err < 0)
		return err;

	memset(setconf_req, 0, BT_AUDIO_IPC_PACKET_SIZE);
	setconf_req->h.msg_type = BT_SETCONFIGURATION_REQ;
	strncpy(setconf_req->device, data->address, 18);
	setconf_req->transport = BT_CAPABILITIES_TRANSPORT_A2DP;
	setconf_req->sbc_capabilities = data->sbc_capabilities;
	setconf_req->access_mode = BT_CAPABILITIES_ACCESS_MODE_WRITE;

	DBG("bluetooth_a2dp_hw_params sending configuration:\n");
	switch (data->sbc_capabilities.channel_mode) {
		case BT_A2DP_CHANNEL_MODE_MONO:
			DBG("\tchannel_mode: MONO\n");
			break;
		case BT_A2DP_CHANNEL_MODE_DUAL_CHANNEL:
			DBG("\tchannel_mode: DUAL CHANNEL\n");
			break;
		case BT_A2DP_CHANNEL_MODE_STEREO:
			DBG("\tchannel_mode: STEREO\n");
			break;
		case BT_A2DP_CHANNEL_MODE_JOINT_STEREO:
			DBG("\tchannel_mode: JOINT STEREO\n");
			break;
		default:
			DBG("\tchannel_mode: UNKNOWN (%d)\n",
				data->sbc_capabilities.channel_mode);
	}
	switch (data->sbc_capabilities.frequency) {
		case BT_SBC_SAMPLING_FREQ_16000:
			DBG("\tfrequency: 16000\n");
			break;
		case BT_SBC_SAMPLING_FREQ_32000:
			DBG("\tfrequency: 32000\n");
			break;
		case BT_SBC_SAMPLING_FREQ_44100:
			DBG("\tfrequency: 44100\n");
			break;
		case BT_SBC_SAMPLING_FREQ_48000:
			DBG("\tfrequency: 48000\n");
			break;
		default:
			DBG("\tfrequency: UNKNOWN (%d)\n",
				data->sbc_capabilities.frequency);
	}
	switch (data->sbc_capabilities.allocation_method) {
		case BT_A2DP_ALLOCATION_SNR:
			DBG("\tallocation_method: SNR\n");
			break;
		case BT_A2DP_ALLOCATION_LOUDNESS:
			DBG("\tallocation_method: LOUDNESS\n");
			break;
		default:
			DBG("\tallocation_method: UNKNOWN (%d)\n",
				data->sbc_capabilities.allocation_method);
	}
	switch (data->sbc_capabilities.subbands) {
		case BT_A2DP_SUBBANDS_4:
			DBG("\tsubbands: 4\n");
			break;
		case BT_A2DP_SUBBANDS_8:
			DBG("\tsubbands: 8\n");
			break;
		default:
			DBG("\tsubbands: UNKNOWN (%d)\n",
				data->sbc_capabilities.subbands);
	}
	switch (data->sbc_capabilities.block_length) {
		case BT_A2DP_BLOCK_LENGTH_4:
			DBG("\tblock_length: 4\n");
			break;
		case BT_A2DP_BLOCK_LENGTH_8:
			DBG("\tblock_length: 8\n");
			break;
		case BT_A2DP_BLOCK_LENGTH_12:
			DBG("\tblock_length: 12\n");
			break;
		case BT_A2DP_BLOCK_LENGTH_16:
			DBG("\tblock_length: 16\n");
			break;
		default:
			DBG("\tblock_length: UNKNOWN (%d)\n",
				data->sbc_capabilities.block_length);
	}
	DBG("\tmin_bitpool: %d\n", data->sbc_capabilities.min_bitpool);
	DBG("\tmax_bitpool: %d\n", data->sbc_capabilities.max_bitpool);

	err = audioservice_send(data->server.fd, &setconf_req->h);
	if (err < 0)
		return err;

	err = audioservice_expect(data->server.fd, &rsp_hdr->msg_h,
					BT_SETCONFIGURATION_RSP);
	if (err < 0)
		return err;

	if (rsp_hdr->posix_errno != 0) {
		ERR("BT_SETCONFIGURATION failed : %s(%d)",
					strerror(rsp_hdr->posix_errno),
					rsp_hdr->posix_errno);
		return -rsp_hdr->posix_errno;
	}

	data->link_mtu = setconf_rsp->link_mtu;

	/* Setup SBC encoder now we agree on parameters */
	bluetooth_a2dp_setup(data);

	DBG("\tallocation=%u\n\tsubbands=%u\n\tblocks=%u\n\tbitpool=%u\n",
		data->sbc.allocation, data->sbc.subbands, data->sbc.blocks,
		data->sbc.bitpool);

	return 0;
}

static int avdtp_write(struct bluetooth_data *data, unsigned long duration)
{
	int ret = 0;
	struct rtp_header *header;
	struct rtp_payload *payload;
	unsigned long delta;
 	struct timeval now;
 	int microseconds, bytes;

	header = (struct rtp_header *)data->buffer;
	payload = (struct rtp_payload *)(data->buffer + sizeof(*header));

	memset(data->buffer, 0, sizeof(*header) + sizeof(*payload));

	payload->frame_count = data->frame_count;
	header->v = 2;
	header->pt = 1;
	header->sequence_number = htons(data->seq_num);
	header->timestamp = htonl(data->nsamples);
	header->ssrc = htonl(1);

	data->stream.revents = 0;
	ret = poll(&data->stream, 1, -1);
	if (ret == 1 && data->stream.revents == POLLOUT) {
		gettimeofday(&now, NULL);
		if (data->last_write.tv_sec || data->last_write.tv_usec) {
			delta = (now.tv_sec - data->last_write.tv_sec) * 1000000 +
					now.tv_usec - data->last_write.tv_usec;
			if (duration > delta) {
				VDBG("duration: %ld delta: %ld, delay %ld us",
					duration, delta, duration - delta);
				usleep(duration - delta);
			}
		}
 		data->last_write = now;
	
		ret = send(data->stream.fd, data->buffer, data->count, 0);
		if (ret < 0) {
			ERR("send returned %d errno %s.", ret, strerror(errno));
			ret = -errno;
		}
	} else {
		ret = -errno;
	}

	if (!data->adjusted_sock_buffer) {
		/* microseconds: number of microseconds of audio for this write */
		microseconds = data->frame_duration * data->frame_count;
		/* ret: number of bytes written */
		/* bytes: number of bytes corresponding to SOCK_BUFFER_MS milliseconds of audio playback */
		bytes = (ret * 1000 * SOCK_BUFFER_MS) / microseconds;
		
		VDBG("microseconds: %d, ret: %d, bytes: %d\n", microseconds, ret, bytes);
		setsockopt(data->stream.fd, SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes));
		data->adjusted_sock_buffer = 1;
	}

	/* Reset buffer of data to send */
	data->count = sizeof(struct rtp_header) + sizeof(struct rtp_payload);
	data->frame_count = 0;
	data->samples = 0;
	data->seq_num++;

	return ret;
}

static int audioservice_send(int sk, const bt_audio_msg_header_t *msg)
{
	int err;

	VDBG("sending %s", bt_audio_strmsg(msg->msg_type));
	if (send(sk, msg, BT_AUDIO_IPC_PACKET_SIZE, 0) > 0)
		err = 0;
	else {
		err = -errno;
		ERR("Error sending data to audio service: %s(%d)",
			strerror(errno), errno);
	}

	return err;
}

static int audioservice_recv(int sk, bt_audio_msg_header_t *inmsg)
{
	int err;
	const char *type;

	VDBG("trying to receive msg from audio service...");
	if (recv(sk, inmsg, BT_AUDIO_IPC_PACKET_SIZE, 0) > 0) {
		type = bt_audio_strmsg(inmsg->msg_type);
		if (type) {
			VDBG("Received %s", type);
			err = 0;
		} else {
			err = -EINVAL;
			ERR("Bogus message type %d "
					"received from audio service",
					inmsg->msg_type);
		}
	} else {
		err = -errno;
		ERR("Error receiving data from audio service: %s(%d)",
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
			ERR("Bogus message %s received while "
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
	data->adjusted_sock_buffer = 0;

	sk = bt_audio_service_open();
	if (sk <= 0) {
		ERR("bt_audio_service_open failed\n");
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
	if (err < 0) {
		ERR("audioservice_send failed for BT_GETCAPABILITIES_REQ\n");
		goto failed;
	}

	err = audioservice_expect(data->server.fd, &rsp_hdr->msg_h, BT_GETCAPABILITIES_RSP);
	if (err < 0) {
		ERR("audioservice_expect failed for BT_GETCAPABILITIES_RSP\n");
		goto failed;
	}
	if (rsp_hdr->posix_errno != 0) {
		ERR("BT_GETCAPABILITIES failed : %s(%d)",
					strerror(rsp_hdr->posix_errno),
					rsp_hdr->posix_errno);
		err = -rsp_hdr->posix_errno;
		goto failed;
	}

	if (getcaps_rsp->transport == BT_CAPABILITIES_TRANSPORT_A2DP)
		data->sbc_capabilities = getcaps_rsp->sbc_capabilities;

	return 0;

failed:
	ERR("bluetooth_init failed, err: %d\n", err);
	bt_audio_service_close(sk);
	data->server.fd = -1;
	return err;
}

int a2dp_init(const char* address, int rate, int channels, a2dpData* dataPtr)
{
	int err;

	DBG("a2dp_init");
	*dataPtr = NULL;
	struct bluetooth_data* data = malloc(sizeof(struct bluetooth_data));
	if (!data)
		return -1;

	strncpy(data->address, address, 18);

	err = bluetooth_init(data);
	if (err < 0)
		goto error;

	data->rate = rate;
	data->channels = channels;

	err = bluetooth_a2dp_hw_params(data);
	if (err < 0) {
		ERR("bluetooth_a2dp_hw_params failed");
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
	uint8_t* src = (uint8_t *)buffer;
	int codesize = data->codesize;
	long ret = 0;
	long frames_left = count;
	int encoded, written;
	const char *buff;
	unsigned long duration = 0;
	
	if (!data->started) {
		ret = bluetooth_start(data);
		if (ret < 0) {
			ERR("bluetooth_start failed");
			return ret;
		}
		data->started = 1;
	}

	while (frames_left >= codesize) {
		/* Enough data to encode (sbc wants 512 byte blocks) */
		encoded = sbc_encode(&(data->sbc), src, codesize,
					data->buffer + data->count,
					sizeof(data->buffer) - data->count,
					&written);
		if (encoded <= 0) {
			ERR("Encoding error %d", encoded);
			goto done;
		}
		VDBG("sbc_encode returned %d, codesize: %d, written: %d\n",
			encoded, codesize, written);

		src += encoded;
		data->count += written;
		data->frame_count++;
		data->samples += encoded;
		data->nsamples += encoded;
		duration += data->frame_duration;

		/* No space left for another frame then send */
		if (data->count + written >= data->link_mtu) {
			VDBG("sending packet %d, count %d, link_mtu %u",
					data->seq_num, data->count,
					data->link_mtu);
			avdtp_write(data, data->last_duration);
			data->last_duration = duration;
 			duration = 0;		
		}

		ret += encoded;
		frames_left -= encoded;
	}

	if (frames_left > 0)
		ERR("%ld bytes left at end of a2dp_write\n", frames_left);

done:
	VDBG("returning %ld", ret);
	return ret;
}

int a2dp_stop(a2dpData d)
{
	struct bluetooth_data* data = (struct bluetooth_data*)d;
	DBG("a2dp_stop\n");
	if (!data)
		return 0;
	
	return bluetooth_stop(data);
}

void a2dp_cleanup(a2dpData d)
{
	struct bluetooth_data* data = (struct bluetooth_data*)d;
	bluetooth_exit(data);
	free(data);
}
