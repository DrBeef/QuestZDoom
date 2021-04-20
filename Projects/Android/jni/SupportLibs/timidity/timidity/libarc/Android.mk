LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := arc

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/..
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../timidity
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../utils

LOCAL_CFLAGS = -DHAVE_CONFIG_H

LOCAL_CPP_EXTENSION := .cpp

LOCAL_SRC_FILES:= arc.c
LOCAL_SRC_FILES+= arc_lzh.c
LOCAL_SRC_FILES+= arc_mime.c
LOCAL_SRC_FILES+= arc_tar.c
LOCAL_SRC_FILES+= arc_zip.c
LOCAL_SRC_FILES+= deflate.c
LOCAL_SRC_FILES+= explode.c
LOCAL_SRC_FILES+= inflate.c
LOCAL_SRC_FILES+= unlzh.c
LOCAL_SRC_FILES+= url.c
LOCAL_SRC_FILES+= url_b64decode.c
LOCAL_SRC_FILES+= url_buff.c
LOCAL_SRC_FILES+= url_cache.c
LOCAL_SRC_FILES+= url_dir.c
LOCAL_SRC_FILES+= url_file.c
LOCAL_SRC_FILES+= url_hqxdecode.c
LOCAL_SRC_FILES+= url_inflate.c
LOCAL_SRC_FILES+= url_mem.c
LOCAL_SRC_FILES+= url_pipe.c
LOCAL_SRC_FILES+= url_qsdecode.c
LOCAL_SRC_FILES+= url_uudecode.c

#LOCAL_STATIC_LIBRARIES := utils

include $(BUILD_STATIC_LIBRARY)
