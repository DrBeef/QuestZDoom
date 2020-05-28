
LOCAL_PATH := $(call my-dir)/../libraries/opnmidi


include $(CLEAR_VARS)


LOCAL_MODULE    := opnmidi_lz

LOCAL_CFLAGS :=  -DOPNMIDI_DISABLE_MIDI_SEQUENCER -DOPNMIDI_DISABLE_GX_EMULATOR -fsigned-char

LOCAL_LDLIBS += -llog

LOCAL_C_INCLUDES :=


LOCAL_SRC_FILES =  	\
	chips/gens_opn2.cpp \
	chips/gens/Ym2612_Emu.cpp \
	chips/mame/mame_ym2612fm.c \
	chips/mame_opn2.cpp \
	chips/nuked_opn2.cpp \
	chips/nuked/ym3438.c \
	opnmidi.cpp \
	opnmidi_load.cpp \
	opnmidi_midiplay.cpp \
	opnmidi_opn2.cpp \
	opnmidi_private.cpp \
	wopn/wopn_file.c \


include $(BUILD_STATIC_LIBRARY)








