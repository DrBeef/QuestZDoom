LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

#LOCAL_ARM_MODE := arm

LOCAL_MODULE := wavpack
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include \

LOCAL_SRC_FILES := bits.c \
	           float.c \
	           metadata.c \
	           unpack.c \
	           unpack3.c \
	           wputils.c \
	           words.c \
	           extra1.c \
	           extra2.c \
	           pack.c \
	           tags.c

LOCAL_CFLAGS           := -Wall
#LOCAL_LDFLAGS          := -Wl,-Map,xxx.map
#LOCAL_LDLIBS := -ldl -lGLESv1_CM -llog -L$(DIRECTORY_TO_OBJ)

include $(BUILD_STATIC_LIBRARY)
