LOCAL_PATH := $(call my-dir)


include $(CLEAR_VARS)

LOCAL_MODULE := lame
#LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include

LOCAL_SRC_FILES := bitstream.c \
	encoder.c \
	fft.c \
	gain_analysis.c \
        id3tag.c \
        lame.c \
        newmdct.c \
	presets.c \
	psymodel.c \
	quantize.c \
	quantize_pvt.c \
	reservoir.c \
	set_get.c \
	tables.c \
	takehiro.c \
	util.c \
	vbrquantize.c \
	version.c \
	mpglib_interface.c \
        VbrTag.c \

LOCAL_CFLAGS           := -Wall -DSTDC_HEADERS
#LOCAL_LDFLAGS          := -Wl,-Map,xxx.map
#LOCAL_LDLIBS := -ldl -lGLESv1_CM -llog -L$(DIRECTORY_TO_OBJ)

include $(BUILD_STATIC_LIBRARY)
