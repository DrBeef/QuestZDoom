LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

#LOCAL_ARM_MODE := arm

LOCAL_MODULE := celt

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
$(LOCAL_PATH)/../ \
$(LOCAL_PATH)/../include \
$(LOCAL_PATH)/../../libogg/include

LOCAL_SRC_FILES := bands.c celt.c celt_decoder.c celt_encoder.c cwrs.c entcode.c \
	           entdec.c entenc.c kiss_fft.c laplace.c mathops.c \
		   mdct.c modes.c pitch.c celt_lpc.c quant_bands.c rate.c vq.c\

# TODO: Add Arch things.

#LOCAL_SHARED_LIBRARIES := liblpc10 libgsm libogg	
#LOCAL_STATIC_LIBRARIES := vorbisfile vorbisenc
LOCAL_CFLAGS           := -Wall -DHAVE_CONFIG_H
#LOCAL_LDFLAGS          := -Wl,-Map,xxx.map
#LOCAL_LDLIBS := -ldl -lGLESv1_CM -llog -L$(DIRECTORY_TO_OBJ)

include $(BUILD_STATIC_LIBRARY)



