LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := timidityhelper

#LOCAL_C_INCLUDES := $(LOCAL_PATH)/..
#LOCAL_C_INCLUDES += $(LOCAL_PATH)/../timidity
#LOCAL_C_INCLUDES += $(LOCAL_PATH)/../utils

LOCAL_CPP_EXTENSION := .cpp

LOCAL_SRC_FILES:= helper.c
#CFLAGS = -Ofast
LOCAL_STATIC_LIBRARIES := sfark
#LOCAL_SHARED_LIBRARIES := timidityplusplus
LOCAL_LDLIBS = -llog -lz
include $(BUILD_SHARED_LIBRARY)
