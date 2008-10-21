# Note: this currently does not build due to a dependency on
# libhciserver, which the Android build of bluez-utils/hcid
# does not produce.

BUILD_HEADSETD:=0
ifeq ($(BUILD_HEADSETD),1)

LOCAL_PATH:= $(call my-dir)

#
# headsetd 
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, dbus) \
	$(call include-path-for, bluez-utils)/common \
	$(call include-path-for, bluez-utils)/sdpd \
	$(call include-path-for, bluez-utils)/eglib \
	$(call include-path-for, bluez-utils)/gdbus

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\" \
	-DSTORAGEDIR=\"/data\" \
	-DCONFIGDIR=\"/etc\" \
	-DENABLE_DEBUG \
	-D__S_IFREG=0100000  # missing from bionic stat.h

LOCAL_SRC_FILES:= \
	a2dp.c \
	avdtp.c \
	control.c \
	device.c \
	headset.c \
	ipc.c \
	sink.c \
	unix.c \
	manager.c \

LOCAL_SHARED_LIBRARIES := \
	libbluetooth \
	libdbus

LOCAL_STATIC_LIBRARIES := \
	libbluez-utils-common-static \
	libeglib_static \
	libsdpserver_static \
	libgdbus_static

LOCAL_MODULE:=headsetd

include $(BUILD_EXECUTABLE)

endif
