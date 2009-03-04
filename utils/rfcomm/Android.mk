LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	kword.c \
	lexer.c \
	main.c \
	parser.c

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\" \
	-DCONFIGDIR=\"/etc/bluez\" \
	-DNEED_PPOLL

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/common

LOCAL_SHARED_LIBRARIES := \
	libbluetooth

LOCAL_STATIC_LIBRARIES := \
	libbluez-utils-common-static

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=rfcomm

include $(BUILD_EXECUTABLE)
