LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	mainloop.c object.c watch.c

LOCAL_CFLAGS+=-O3 -DNEED_DBUS_WATCH_GET_UNIX_FD

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/eglib \
	$(call include-path-for, bluez-utils)/common \
	$(call include-path-for, dbus)

LOCAL_SHARED_LIBRARIES := \
	libbluetooth \
	libdbus

LOCAL_MODULE:=libgdbus_static

include $(BUILD_STATIC_LIBRARY)
