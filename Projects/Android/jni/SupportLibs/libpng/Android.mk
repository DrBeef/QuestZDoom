
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libpng

LOCAL_SRC_FILES = \
    png.c \
	pngerror.c \
	pngget.c \
	pngmem.c \
	pngpread.c \
	pngread.c \
	pngrio.c \
	pngrtran.c \
	pngrutil.c \
	pngset.c \
	pngtrans.c \
	pngwio.c \
	pngwrite.c \
	pngwtran.c \
	pngwutil.c \
	arm/arm_init.c  \
	arm/filter_neon_intrinsics.c




LOCAL_LDLIBS := -lz
LOCAL_EXPORT_LDLIBS := -lz 
LOCAL_STATIC_LIBRARIES :=  
#include $(BUILD_SHARED_LIBRARY)
include $(BUILD_STATIC_LIBRARY)