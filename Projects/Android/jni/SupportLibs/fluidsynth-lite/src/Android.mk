LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE   := fluidsynth-static

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include/ \
                    $(LOCAL_PATH)/utils \
                    $(LOCAL_PATH)/synth \
                    $(LOCAL_PATH)/sfloader \
                    $(LOCAL_PATH)/rvoice \
                    $(LOCAL_PATH)/midi \

LOCAL_CFLAGS :=  -DDEFAULT_SOUNDFONT=\"gzdoom.sf2\" -DSUPPORTS_VLA -DHAVE_PTHREAD_H -DHAVE_SYS_STAT_H -DHAVE_STDLIB_H -DHAVE_STDIO_H -DHAVE_MATH_H -DHAVE_STRING_H -DHAVE_STDARG_H -DHAVE_SYS_SOCKET_H -DHAVE_NETINET_IN_H -DHAVE_ARPA_INET_H -DHAVE_NETINET_TCP_H -DHAVE_UNISTD_H -DHAVE_ERRNO_H -DHAVE_FCNTL_H -DVERSION=1.1.6


LOCAL_CPPFLAGS := $(LOCAL_CFLAGS)  -fexceptions -frtti


LOCAL_SRC_FILES :=    utils/fluid_conv.c \
                       utils/fluid_hash.c \
                       utils/fluid_list.c \
                       utils/fluid_ringbuffer.c \
                       utils/fluid_settings.c \
                       utils/fluid_sys.c \
                       sfloader/fluid_defsfont.c \
                       sfloader/fluid_ramsfont.c \
                       rvoice/fluid_adsr_env.c \
                       rvoice/fluid_chorus.c \
                       rvoice/fluid_iir_filter.c \
                       rvoice/fluid_lfo.c \
                       rvoice/fluid_rvoice.c \
                       rvoice/fluid_rvoice_dsp.c \
                       rvoice/fluid_rvoice_event.c \
                       rvoice/fluid_rvoice_mixer.c \
                       rvoice/fluid_rev.c \
                       synth/fluid_chan.c \
                       synth/fluid_event.c \
                       synth/fluid_gen.c \
                       synth/fluid_mod.c \
                       synth/fluid_synth.c \
                       synth/fluid_tuning.c \
                       synth/fluid_voice.c \
                       midi/fluid_midi.c \
                       midi/fluid_midi_router.c \
                       midi/fluid_seqbind.c \
                       midi/fluid_seq.c \
 
LOCAL_LDLIBS := -llog
include $(BUILD_STATIC_LIBRARY)

#include $(CLEAR_VARS)

#include $(BUILD_SHARED_LIBRARY)

