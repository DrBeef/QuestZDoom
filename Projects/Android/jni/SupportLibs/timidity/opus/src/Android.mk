LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

#LOCAL_ARM_MODE := arm

LOCAL_MODULE := opus

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
$(LOCAL_PATH)/../ \
$(LOCAL_PATH)/../include/ \
$(LOCAL_PATH)/../silk/ \
$(LOCAL_PATH)/../celt/ \
$(LOCAL_PATH)/../../libogg/include/

LOCAL_SRC_FILES := opus.c opus_decoder.c opus_encoder.c opus_multistream.c opus_multistream_encoder.c \
				   opus_multistream_decoder.c repacketizer.c analysis.c mlp.c mlp_data.c

#LOCAL_SHARED_LIBRARIES := liblpc10 libgsm libogg	
#LOCAL_STATIC_LIBRARIES := vorbisfile vorbisenc
LOCAL_CFLAGS           := -Wall -DHAVE_CONFIG_H
#LOCAL_LDFLAGS          := -Wl,-Map,xxx.map
#LOCAL_LDLIBS := -ldl -lGLESv1_CM -llog -L$(DIRECTORY_TO_OBJ)

include $(BUILD_STATIC_LIBRARY)



