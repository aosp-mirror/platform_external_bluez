LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	error.c \
	glib-helper.c \
	logging.c \
	oui.c \
	sdp-glib.c \
	sdp-xml.c \
	textfile.c \
	android_bluez.c

LOCAL_C_INCLUDES:= \
	$(LOCAL_PATH) \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/eglib/ \
	$(call include-path-for, bluez-utils)/gdbus/ \
	external/expat/lib/ \
	$(call include-path-for, dbus)

LOCAL_STATIC_LIBRARIES := \
	libeglib_static \
	libgdbus_static

LOCAL_MODULE:=libbluez-utils-common-static

LOCAL_CFLAGS+= \
	-O3 \
	-DNEED_DBUS_WATCH_GET_UNIX_FD

include $(BUILD_STATIC_LIBRARY)
