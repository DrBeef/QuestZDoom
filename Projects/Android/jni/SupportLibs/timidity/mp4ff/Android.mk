#mp4ff
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CFLAGS := -DHAVE_CONFIG_H

LOCAL_SRC_FILES := \
  mp4atom.c \
  mp4ff.c \
  mp4meta.c \
  mp4sample.c \
  mp4tagupdate.c \
  mp4util.c 

#LOCAL_C_INCLUDES += \
#	$(LOCAL_PATH) 

LOCAL_MODULE := mp4ff

include $(BUILD_STATIC_LIBRARY)
