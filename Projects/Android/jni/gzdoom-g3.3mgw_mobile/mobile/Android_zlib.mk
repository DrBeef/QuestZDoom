
LOCAL_PATH := $(call my-dir)/../zlib


include $(CLEAR_VARS)


LOCAL_MODULE    := zlib_lz

LOCAL_CFLAGS = -Wall


LOCAL_LDLIBS += -llog

LOCAL_C_INCLUDES :=


LOCAL_SRC_FILES =  \
    adler32.c \
    compress.c \
    crc32.c \
    deflate.c \
    inflate.c \
    infback.c \
    inftrees.c \
    inffast.c \
    trees.c \
    uncompr.c \
    zutil.c \

LOCAL_LDLIBS :=  -ldl -llog

include $(BUILD_STATIC_LIBRARY)








