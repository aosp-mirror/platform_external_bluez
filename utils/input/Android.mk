LOCAL_PATH:= $(call my-dir)

# HID plugin

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	device.c \
	fakehid.c \
	main.c \
	manager.c \
	server.c \
	storage.c

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\" \
	-DSTORAGEDIR=\"/data/misc/hcid\" \
	-DCONFIGDIR=\"/etc/bluez\"

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/common \
	$(call include-path-for, bluez-utils)/eglib \
	$(call include-path-for, bluez-utils)/hcid \
	$(call include-path-for, bluez-utils)/gdbus \
	$(call include-path-for, dbus)

LOCAL_SHARED_LIBRARIES := \
	libhcid \
	libbluetooth \
	libdbus \
	libexpat \
	libcutils

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/bluez-plugin
LOCAL_UNSTRIPPED_PATH := $(TARGET_OUT_SHARED_LIBRARIES_UNSTRIPPED)/bluez-plugin
LOCAL_MODULE := input

include $(BUILD_SHARED_LIBRARY)
