LOCAL_PATH:= $(call my-dir)

#
# libsdpserver
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	cstate.c \
	request.c \
	service.c \
	server.c \
	servicedb.c

LOCAL_CFLAGS:=-DVERSION=\"3.36\"

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/common \
	$(call include-path-for, bluez-utils)/eglib

LOCAL_MODULE:=libsdpserver_static

include $(BUILD_STATIC_LIBRARY)
