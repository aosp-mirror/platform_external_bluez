LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	gmain.c \
	gmodule.c

LOCAL_C_INCLUDES:= \
        $(LOCAL_PATH)

LOCAL_MODULE:=libeglib_static

LOCAL_CFLAGS+=-O3

include $(BUILD_STATIC_LIBRARY)
