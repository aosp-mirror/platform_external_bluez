LOCAL_PATH:= $(call my-dir)

#
# hcid
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, dbus) \
	$(call include-path-for, bluez-utils)/common

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\" \
	-DSTORAGEDIR=\"/data\" \
	-DCONFIGDIR=\"/etc\" \

LOCAL_SRC_FILES:= \
	dun.c	  \
	main.c    \
	msdun.c   \
	sdp.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth 

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=dund

include $(BUILD_EXECUTABLE)
