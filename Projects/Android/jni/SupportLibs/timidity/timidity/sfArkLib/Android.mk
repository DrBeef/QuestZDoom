LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := sfark

LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_CPP_EXTENSION := .cpp

LOCAL_SRC_FILES:= sfklCoding.cpp
LOCAL_SRC_FILES+= sfklCrunch.cpp
LOCAL_SRC_FILES+= sfklDiff.cpp
LOCAL_SRC_FILES+= sfklFile.cpp
LOCAL_SRC_FILES+= sfklLPC.cpp
LOCAL_SRC_FILES+= sfklString.cpp
LOCAL_SRC_FILES+= sfklZip.cpp

include $(BUILD_STATIC_LIBRARY)
