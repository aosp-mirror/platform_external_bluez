BUILD_HCICONFIG:=1
BUILD_HCITOOL:=1
BUILD_L2PING:=1
BUILD_HCIATTACH:=1

LOCAL_PATH:= $(call my-dir)

#
# sdptool
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/common/ 

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\" -fpermissive

LOCAL_SRC_FILES:= \
	sdptool.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth 

LOCAL_STATIC_LIBRARIES := \
	libbluez-utils-common-static

LOCAL_MODULE:=sdptool

include $(BUILD_EXECUTABLE)

ifeq ($(BUILD_HCICONFIG),1)

#
# hciconfig 
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/common/

LOCAL_CFLAGS:= \
	-DSTORAGEDIR=\"/tmp\" \
	-DVERSION=\"3.36\"

LOCAL_SRC_FILES:= \
	csr.c \
	csr_h4.c \
	hciconfig.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth 

LOCAL_STATIC_LIBRARIES := \
	libbluez-utils-common-static

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng

LOCAL_MODULE:=hciconfig

include $(BUILD_EXECUTABLE)

endif # BUILD_HCICONFIG

ifeq ($(BUILD_HCITOOL),1)

#
# hcitool 
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/common/

LOCAL_CFLAGS:= \
	-DSTORAGEDIR=\"/tmp\" \
	-DVERSION=\"3.36\"

LOCAL_SRC_FILES:= \
	hcitool.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth

LOCAL_STATIC_LIBRARIES := \
	libbluez-utils-common-static

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng

LOCAL_MODULE:=hcitool

include $(BUILD_EXECUTABLE)

endif # hcitool

ifeq ($(BUILD_L2PING),1)

#
# l2ping 
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs)

LOCAL_SRC_FILES:= \
	l2ping.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth 

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=l2ping

include $(BUILD_EXECUTABLE)

endif # l2ping 


#
# hciattach
#

ifeq ($(BUILD_HCIATTACH),1)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs) \
	$(call include-path-for, bluez-utils)/common/ 

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\" \
	-D__BSD_VISIBLE=1 

LOCAL_SRC_FILES:= \
	hciattach.c \
	hciattach_st.c \
	hciattach_tialt.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth 

LOCAL_STATIC_LIBRARIES := \
	libbluez-utils-common-static

LOCAL_MODULE:=hciattach

include $(BUILD_EXECUTABLE)

endif # hciattach
