BUILD_PAND:=1
ifeq ($(BUILD_PAND),1)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-utils)/common/ \
	$(call include-path-for, bluez-libs)

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\" \
	-DNEED_PPOLL

LOCAL_SRC_FILES:= \
	bnep.c    \
	main.c    \
	sdp.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=pand

include $(BUILD_EXECUTABLE)
endif
