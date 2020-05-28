
LOCAL_PATH := $(call my-dir)/../libraries/timidity


include $(CLEAR_VARS)


LOCAL_MODULE    := timidity_lz

LOCAL_CFLAGS := -fexceptions -std=c++11 -Wno-unused-function -Wno-unused-variable -fsigned-char

LOCAL_LDLIBS += -llog

LOCAL_C_INCLUDES := $(LOCAL_PATH)/timidity



LOCAL_SRC_FILES =  	\
			common.cpp \
        	instrum.cpp \
        	instrum_dls.cpp \
        	instrum_font.cpp \
        	instrum_sf2.cpp \
        	mix.cpp \
        	playmidi.cpp \
        	resample.cpp \
        	timidity.cpp \


include $(BUILD_STATIC_LIBRARY)








