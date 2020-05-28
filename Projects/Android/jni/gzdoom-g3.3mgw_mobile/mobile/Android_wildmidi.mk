
LOCAL_PATH := $(call my-dir)/../libraries/wildmidi


include $(CLEAR_VARS)


LOCAL_MODULE    := wildmidi_lz

LOCAL_CFLAGS := -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -Wall -Wextra -Wno-unused-parameter -fomit-frame-pointer -fsigned-char

LOCAL_LDLIBS += -llog

LOCAL_C_INCLUDES := $(LOCAL_PATH)/wildmidi



LOCAL_SRC_FILES =  	\
		file_io.cpp \
    	gus_pat.cpp \
    	reverb.cpp \
    	wildmidi_lib.cpp \
    	wm_error.cpp

include $(BUILD_STATIC_LIBRARY)








