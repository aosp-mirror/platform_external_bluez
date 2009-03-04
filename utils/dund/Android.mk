LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	dun.c \
	main.c \
	msdun.c \
	sdp.c

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\" \
	-DSTORAGEDIR=\"/data/misc/hcid\" \
	-DCONFIGDIR=\"/etc/bluez\"

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/common \
	$(call include-path-for, dbus)

LOCAL_SHARED_LIBRARIES := \
	libbluetooth

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=dund

include $(BUILD_EXECUTABLE)
