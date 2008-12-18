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

LOCAL_CFLAGS+= \
	-O3 \
	-DNEED_DBUS_WATCH_GET_UNIX_FD

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/eglib \
	$(call include-path-for, bluez-utils)/gdbus \
	$(call include-path-for, dbus)

LOCAL_MODULE:=libbluez-utils-common-static

include $(BUILD_STATIC_LIBRARY)
