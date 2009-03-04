LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	gmain.c \
	gmodule.c

LOCAL_CFLAGS+=-O3

LOCAL_MODULE:=libeglib_static

include $(BUILD_STATIC_LIBRARY)
