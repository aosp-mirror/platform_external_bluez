LOCAL_PATH:= $(call my-dir)

#
# libhcid
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	adapter.c \
	agent.c \
	device.c \
	dbus-common.c \
	dbus-database.c \
	dbus-error.c \
	dbus-hci.c \
	dbus-sdp.c \
	dbus-security.c \
	dbus-service.c \
	kword.c \
	lexer.c \
	main.c \
	manager.c \
	parser.c \
	security.c \
	server.c \
	storage.c \
	plugin.c

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\" \
	-DSTORAGEDIR=\"/data/misc/hcid\" \
	-DCONFIGDIR=\"/etc/bluez\" \
	-DSERVICEDIR=\"/system/bin\" \
	-DPLUGINDIR=\"/system/lib/bluez-plugin\" \
	-DANDROID_SET_AID_AND_CAP \
	-DANDROID_EXPAND_NAME

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/common \
	$(call include-path-for, bluez-utils)/eglib \
	$(call include-path-for, bluez-utils)/gdbus \
	$(call include-path-for, bluez-utils)/sdpd \
	$(call include-path-for, dbus)

LOCAL_SHARED_LIBRARIES := \
	libdl \
	libbluetooth \
	libdbus \
	libcutils

LOCAL_STATIC_LIBRARIES := \
	libsdpserver_static \
	libeglib_static \
	libgdbus_static \
	libbluez-utils-common-static

LOCAL_MODULE:=libhcid

include $(BUILD_SHARED_LIBRARY)

#
# hcid
#

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
	libhcid

LOCAL_MODULE:=hcid

include $(BUILD_EXECUTABLE)
