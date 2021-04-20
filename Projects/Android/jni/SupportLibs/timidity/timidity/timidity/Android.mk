LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := timidityplusplus

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/..
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../utils
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libarc
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libunimod
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../helper

LOCAL_CFLAGS = -DHAVE_CONFIG_H
LOCAL_CPP_EXTENSION := .cpp

LOCAL_SRC_FILES:= aiff_a.c
LOCAL_SRC_FILES+= aq.c
LOCAL_SRC_FILES+= au_a.c
LOCAL_SRC_FILES+= audio_cnv.c
LOCAL_SRC_FILES+= common.c
LOCAL_SRC_FILES+= controls.c
LOCAL_SRC_FILES+= droid_a.c
LOCAL_SRC_FILES+= effect.c
LOCAL_SRC_FILES+= filter.c
LOCAL_SRC_FILES+= freq.c
LOCAL_SRC_FILES+= instrum.c
LOCAL_SRC_FILES+= list_a.c
LOCAL_SRC_FILES+= loadtab.c
LOCAL_SRC_FILES+= m2m.c
LOCAL_SRC_FILES+= mfi.c
LOCAL_SRC_FILES+= miditrace.c
LOCAL_SRC_FILES+= mix.c
LOCAL_SRC_FILES+= mod.c
LOCAL_SRC_FILES+= mod2midi.c
LOCAL_SRC_FILES+= modmid_a.c
LOCAL_SRC_FILES+= mt19937ar.c
LOCAL_SRC_FILES+= optcode.c
LOCAL_SRC_FILES+= output.c
LOCAL_SRC_FILES+= playmidi.c
LOCAL_SRC_FILES+= quantity.c
LOCAL_SRC_FILES+= raw_a.c
LOCAL_SRC_FILES+= rcp.c
LOCAL_SRC_FILES+= readmidi.c
LOCAL_SRC_FILES+= recache.c
LOCAL_SRC_FILES+= resample.c
LOCAL_SRC_FILES+= reverb.c
LOCAL_SRC_FILES+= sbkconv.c
LOCAL_SRC_FILES+= sffile.c
LOCAL_SRC_FILES+= sfitem.c
#LOCAL_SRC_FILES+= sles_a.c
LOCAL_SRC_FILES+= smfconv.c
LOCAL_SRC_FILES+= smplfile.c
LOCAL_SRC_FILES+= sndfont.c
LOCAL_SRC_FILES+= tables.c
LOCAL_SRC_FILES+= timidity.c
LOCAL_SRC_FILES+= version.c
LOCAL_SRC_FILES+= wave_a.c
LOCAL_SRC_FILES+= wrd_read.c
LOCAL_SRC_FILES+= wrdt.c

#LOCAL_LDLIBS = -llog
#LOCAL_LDLIBS+= libOpenSLES

LOCAL_STATIC_LIBRARIES := arc unimod utils interface
LOCAL_SHARED_LIBRARIES := timidityhelper
include $(BUILD_SHARED_LIBRARY)
