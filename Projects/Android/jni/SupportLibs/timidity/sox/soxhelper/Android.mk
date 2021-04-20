LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := soxhelper

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../src

LOCAL_SRC_FILES := helper.c
LOCAL_LDLIBS := -lz -ldl
LOCAL_STATIC_LIBRARIES := sox
include $(BUILD_SHARED_LIBRARY)
