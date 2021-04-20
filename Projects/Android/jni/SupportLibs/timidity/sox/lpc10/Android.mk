LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := lpc10

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
$(LOCAL_PATH)/../src \

LOCAL_SRC_FILES := analys.c bsynz.c chanwr.c dcbias.c \
                   decode.c deemp.c difmag.c dyptrk.c encode.c energy.c f2clib.c \
                   ham84.c hp100.c invert.c irc2pc.c ivfilt.c lpcdec.c lpcenc.c lpcini.c \
                   lpfilt.c median.c mload.c onset.c pitsyn.c placea.c placev.c preemp.c \
                   prepro.c random.c rcchk.c synths.c tbdm.c voicin.c vparms.c

LOCAL_CFLAGS           := -Wall
#LOCAL_LDLIBS := -ldl -llog

include $(BUILD_STATIC_LIBRARY)
