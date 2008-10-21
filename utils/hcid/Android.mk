LOCAL_PATH:= $(call my-dir)

#
# hcid
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, dbus) \
	$(call include-path-for, bluez-utils)/common/ \
	$(call include-path-for, bluez-utils)/eglib/ \
	$(call include-path-for, bluez-utils)/gdbus/ \
	$(call include-path-for, bluez-utils)/sdpd/ 

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\" \
	-DSTORAGEDIR=\"/data/misc/hcid\" \
	-DCONFIGDIR=\"/etc\" \
	-DSERVICEDIR=\"/system/bin\" \
	-DPLUGINDIR=\"\" \
	-DANDROID_SET_AID_AND_CAP \
	-DANDROID_EXPAND_NAME

LOCAL_SRC_FILES:= \
	adapter.c \
	agent.c \
	dbus-common.c \
	dbus-database.c \
	dbus-error.c \
	dbus-hci.c \
	dbus-sdp.c \
	dbus-security.c \
	dbus-service.c \
	device.c \
	kword.c \
	lexer.c \
	main.c \
	manager.c \
	parser.c \
	security.c \
	server.c \
	storage.c \
	plugin.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth \
	libdbus \
	libexpat \
	libcutils

LOCAL_STATIC_LIBRARIES := \
	libsdpserver_static \
	libeglib_static \
	libgdbus_static \
	libbluez-utils-common-static

LOCAL_MODULE:=hcid

include $(BUILD_EXECUTABLE)
