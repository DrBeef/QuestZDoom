LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


LOCAL_MODULE := ogg

#LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
$(LOCAL_PATH)/../include/


LOCAL_SRC_FILES := framing.c bitwise.c

LOCAL_CFLAGS           := -Wall
#LOCAL_LDFLAGS          := -Wl,-Map,xxx.map
include $(BUILD_STATIC_LIBRARY)



