
LOCAL_PATH := $(call my-dir)/../libraries/timidityplus


include $(CLEAR_VARS)


LOCAL_MODULE    := timidityplus_lz

LOCAL_CFLAGS :=  -std=c++11 -fsigned-char

LOCAL_LDLIBS += -llog

LOCAL_C_INCLUDES := $(LOCAL_PATH)/timiditypp


LOCAL_SRC_FILES =  	\
			fft4g.cpp \
        	reverb.cpp \
        	common.cpp \
        	configfile.cpp \
        	effect.cpp \
        	filter.cpp \
        	freq.cpp \
        	instrum.cpp \
        	mblock.cpp \
        	mix.cpp \
        	playmidi.cpp \
        	quantity.cpp \
        	readmidic.cpp \
        	recache.cpp \
        	resample.cpp \
        	sbkconv.cpp \
        	sffile.cpp \
        	sfitem.cpp \
        	smplfile.cpp \
        	sndfont.cpp \
        	tables.cpp \



include $(BUILD_STATIC_LIBRARY)








