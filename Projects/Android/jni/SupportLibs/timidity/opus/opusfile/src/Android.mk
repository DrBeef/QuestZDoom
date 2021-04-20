LOCAL_PATH := $(call my-dir)

###########################
#
# SDL shared library
#
###########################
include $(CLEAR_VARS)



LOCAL_MODULE := opusfile

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
$(LOCAL_PATH)/../include/ \
$(LOCAL_PATH)/../../../libogg/include/ \
$(LOCAL_PATH)/../../include/ \

LOCAL_SRC_FILES := opusfile.c internal.c info.c stream.c http.c

#LOCAL_SHARED_LIBRARIES := liblpc10 libgsm libogg libvorbis	

#LOCAL_LDLIBS := -ldl -lGLESv1_CM -llog -L$(DIRECTORY_TO_OBJ)

include $(BUILD_STATIC_LIBRARY)


