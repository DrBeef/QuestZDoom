LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := fmod
LOCAL_SRC_FILES := api/lowlevel/lib/$(TARGET_ARCH_ABI)/libfmod.so

include $(PREBUILT_SHARED_LIBRARY)

#Logging version
#include $(CLEAR_VARS)
#LOCAL_MODULE    := fmodL
#LOCAL_SRC_FILES := api/lowlevel/lib/$(TARGET_ARCH_ABI)/libfmodL.so
#include $(PREBUILT_SHARED_LIBRARY)
