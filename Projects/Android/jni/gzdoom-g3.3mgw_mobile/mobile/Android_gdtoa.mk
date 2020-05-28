
LOCAL_PATH := $(call my-dir)/../libraries/gdtoa


include $(CLEAR_VARS)


LOCAL_MODULE    := gdtoa_lz

LOCAL_CFLAGS :=

LOCAL_LDLIBS += -llog

LOCAL_C_INCLUDES :=   . $(GZDOOM_TOP_PATH)/mobile/src/extrafiles


LOCAL_SRC_FILES =  	\
	dmisc.c \
	dtoa.c \
	g_Qfmt.c \
	g__fmt.c \
	g_ddfmt.c \
	g_dfmt.c \
	g_ffmt.c \
	g_xLfmt.c \
	g_xfmt.c \
	gdtoa.c \
	gethex.c \
	gmisc.c \
	hd_init.c \
	hexnan.c \
	misc.c \
	smisc.c \
	strtoIQ.c \
	strtoId.c \
	strtoIdd.c \
	strtoIf.c \
	strtoIg.c \
	strtoIx.c \
	strtoIxL.c \
	strtod.c \
	strtodI.c \
	strtodg.c \
	strtopQ.c \
	strtopd.c \
	strtopdd.c \
	strtopf.c \
	strtopx.c \
	strtopxL.c \
	strtorQ.c \
	strtord.c \
	strtordd.c \
	strtorf.c \
	strtorx.c \
	strtorxL.c \
	sum.c \
	ulp.c \


include $(BUILD_STATIC_LIBRARY)








