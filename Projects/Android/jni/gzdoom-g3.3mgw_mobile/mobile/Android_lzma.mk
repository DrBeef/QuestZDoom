
LOCAL_PATH := $(call my-dir)/../libraries/lzma


include $(CLEAR_VARS)


LOCAL_MODULE    := lzma_lz

LOCAL_CFLAGS = -Wall -fomit-frame-pointer -D_7ZIP_ST


LOCAL_LDLIBS += -llog

LOCAL_C_INCLUDES :=


LOCAL_SRC_FILES =  \
	C/7zBuf.c \
	C/7zCrc.c \
	C/7zCrcOpt.c \
	C/7zDec.c \
    C/7zArcIn.c \
	C/7zStream.c \
	C/7zDec.c \
	C/Delta.c \
	C/Bcj2.c \
	C/Bra.c \
	C/Bra86.c \
	C/BraIA64.c \
	C/CpuArch.c \
	C/LzFind.c \
	C/Lzma2Dec.c \
	C/LzmaDec.c \
	C/LzmaEnc.c \


include $(BUILD_STATIC_LIBRARY)








