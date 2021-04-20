LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := unimod

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/..
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../timidity
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../utils
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libarc

LOCAL_CFLAGS = -DHAVE_CONFIG_H

LOCAL_CPP_EXTENSION := .cpp

LOCAL_SRC_FILES:= load_669.c
LOCAL_SRC_FILES+= load_amf.c
LOCAL_SRC_FILES+= load_dsm.c
LOCAL_SRC_FILES+= load_far.c
LOCAL_SRC_FILES+= load_gdm.c
LOCAL_SRC_FILES+= load_imf.c
LOCAL_SRC_FILES+= load_it.c
LOCAL_SRC_FILES+= load_m15.c
LOCAL_SRC_FILES+= load_med.c
LOCAL_SRC_FILES+= load_mod.c
LOCAL_SRC_FILES+= load_mtm.c
LOCAL_SRC_FILES+= load_okt.c
LOCAL_SRC_FILES+= load_s3m.c
LOCAL_SRC_FILES+= load_stm.c
LOCAL_SRC_FILES+= load_stx.c
LOCAL_SRC_FILES+= load_ult.c
LOCAL_SRC_FILES+= load_uni.c
LOCAL_SRC_FILES+= load_xm.c
LOCAL_SRC_FILES+= mloader.c
LOCAL_SRC_FILES+= mlutil.c
LOCAL_SRC_FILES+= mmsup.c
LOCAL_SRC_FILES+= munitrk.c

LOCAL_STATIC_LIBRARIES := utils arc

include $(BUILD_STATIC_LIBRARY)
