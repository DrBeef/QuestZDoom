LOCAL_PATH := $(call my-dir)


include $(CLEAR_VARS)

LOCAL_MODULE := mad

LOCAL_C_INCLUDES := $(LOCAL_PATH) \

LOCAL_SRC_FILES := version.c fixed.c bit.c timer.c stream.c frame.c  \
                   synth.c decoder.c layer12.c layer3.c huffman.c 

LOCAL_CFLAGS           := -Wall -DHAVE_CONFIG_H -DFPM_DEFAULT

include $(BUILD_STATIC_LIBRARY)


