LOCAL_PATH := $(call my-dir)

###########################
#
# SDL shared library
#
###########################
include $(CLEAR_VARS)



LOCAL_MODULE := vorbisfile

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
$(LOCAL_PATH)/../include \
$(LOCAL_PATH)/../../libogg/include \
$(LOCAL_PATH)/../lib \

LOCAL_SRC_FILES := vorbisfile.c

#LOCAL_SHARED_LIBRARIES := liblpc10 libgsm libogg libvorbis	

#LOCAL_LDLIBS := -ldl -lGLESv1_CM -llog -L$(DIRECTORY_TO_OBJ)

include $(BUILD_STATIC_LIBRARY)


