LOCAL_PATH:= $(call my-dir)

#
# hstest
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs)

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\"

LOCAL_SRC_FILES:= \
	hstest.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=hstest

include $(BUILD_EXECUTABLE)

#
# l2test
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs)

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\"

LOCAL_SRC_FILES:= \
	l2test.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=l2test

include $(BUILD_EXECUTABLE)

#
# rctest
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs)

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\"

LOCAL_SRC_FILES:= \
	rctest.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=rctest

include $(BUILD_EXECUTABLE)

#
# passkey-agent
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, dbus)

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\"

LOCAL_SRC_FILES:= \
	passkey-agent.c

LOCAL_SHARED_LIBRARIES := \
	libdbus

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=passkey-agent

include $(BUILD_EXECUTABLE)

#
# scotest
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs)

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\"

LOCAL_SRC_FILES:= \
	scotest.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=scotest

include $(BUILD_EXECUTABLE)

#
# auth-agent
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, dbus)

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\"

LOCAL_SRC_FILES:= \
	auth-agent.c

LOCAL_SHARED_LIBRARIES := \
	libdbus

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=auth-agent

include $(BUILD_EXECUTABLE)

#
# attest
#

include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
	$(call include-path-for, bluez-libs)

LOCAL_CFLAGS:= \
	-DVERSION=\"3.36\"

LOCAL_SRC_FILES:= \
	attest.c

LOCAL_SHARED_LIBRARIES := \
	libbluetooth

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:=attest

include $(BUILD_EXECUTABLE)
