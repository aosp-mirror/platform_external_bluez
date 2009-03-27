LOCAL_PATH:= $(call my-dir)

# A2DP plugin

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	a2dp.c \
	avdtp.c \
	control.c \
	device.c \
	headset.c \
	ipc.c \
	main.c \
	manager.c \
	sink.c \
	unix.c

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\" \
	-DSTORAGEDIR=\"/data/misc/hcid\" \
	-DCONFIGDIR=\"/etc/bluez\" \
	-DANDROID \
	-D__S_IFREG=0100000  # missing from bionic stat.h

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/common \
	$(call include-path-for, bluez-utils)/hcid \
	$(call include-path-for, bluez-utils)/sdpd \
	$(call include-path-for, bluez-utils)/eglib \
	$(call include-path-for, bluez-utils)/gdbus \
	$(call include-path-for, dbus)

LOCAL_SHARED_LIBRARIES := \
	libbluetooth \
	libhcid \
	libdbus

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/bluez-plugin
LOCAL_UNSTRIPPED_PATH := $(TARGET_OUT_SHARED_LIBRARIES_UNSTRIPPED)/bluez-plugin
LOCAL_MODULE := audio

include $(BUILD_SHARED_LIBRARY)

#
# liba2dp
# This is linked to Audioflinger so **LGPL only**

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	liba2dp.c \
	ipc.c \
	../sbc/sbc.c.arm \
	../sbc/sbc_primitives.c

# to improve SBC performance
LOCAL_CFLAGS:= -funroll-loops

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-utils)/sbc \

LOCAL_SHARED_LIBRARIES := \
	libcutils

LOCAL_MODULE := liba2dp

include $(BUILD_SHARED_LIBRARY)
