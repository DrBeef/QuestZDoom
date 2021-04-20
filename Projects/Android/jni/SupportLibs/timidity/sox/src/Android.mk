LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)


#$(LOCAL_PATH)/../libogg/include/ \
#$(LOCAL_PATH)/../vorbis/include/ \
#$(LOCAL_PATH)/../flac/include/ \
#$(LOCAL_PATH)/../lame-3.98.4/include/ \
#$(LOCAL_PATH)/../libsndfile-1.0.24/src/ \
#$(LOCAL_PATH)/../wavpack-4.60.1/ \
#$(LOCAL_PATH)/../ffmpeg/



LOCAL_MODULE := sox
LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
$(LOCAL_PATH)/../../lame/include/ \
$(LOCAL_PATH)/../../libogg/include \
$(LOCAL_PATH)/../../vorbis/include \
$(LOCAL_PATH)/../../libmad/ \
$(LOCAL_PATH)/../../flac/include/ \
$(LOCAL_PATH)/../../wavpack/ \
$(LOCAL_PATH)/../../faad2/include/ \
$(LOCAL_PATH)/../../opus/include/ \
$(LOCAL_PATH)/../../opus/opusfile/include/ \
$(LOCAL_PATH)/../../mp4ff/ \
$(LOCAL_PATH)/../libgsm/ \
$(LOCAL_PATH)/../lpc10/ 

LOCAL_CFLAGS           := -Wall
#LOCAL_LDFLAGS          := -Wl,-Map,xxx.map

LOCAL_SRC_FILES := sox.c adpcms.c aiff.c cvsd.c \
	g711.c g721.c g723_24.c g723_40.c g72x.c vox.c \
        raw.c formats.c formats_i.c skelform.c \
	xmalloc.c getopt.c \
	util.c libsox.c libsox_i.c sox-fmt.c \
	hilbert.c upsample.c downsample.c \
        bend.c biquad.c biquads.c chorus.c compand.c \
	compandt.c contrast.c dcshift.c delay.c dft_filter.c \
	dither.c divide.c earwax.c echo.c \
	echos.c effects.c effects_i.c effects_i_dsp.c fade.c fft4g.c \
	fir.c firfit.c flanger.c gain.c input.c \
	loudness.c mcompand.c \
	noiseprof.c noisered.c output.c overdrive.c pad.c \
	phaser.c rate.c \
	remix.c repeat.c reverb.c reverse.c silence.c \
	sinc.c skeleff.c speed.c splice.c stat.c stats.c \
	stretch.c swap.c synth.c tempo.c tremolo.c trim.c vad.c vol.c \
        raw-fmt.c s1-fmt.c s2-fmt.c s3-fmt.c \
        s4-fmt.c u1-fmt.c u2-fmt.c u3-fmt.c u4-fmt.c al-fmt.c la-fmt.c ul-fmt.c \
        lu-fmt.c 8svx.c aiff-fmt.c aifc-fmt.c au.c avr.c cdr.c cvsd-fmt.c \
        dvms-fmt.c dat.c hcom.c htk.c maud.c prc.c sf.c smp.c \
        sounder.c soundtool.c sphere.c tx16w.c voc.c vox-fmt.c ima-fmt.c adpcm.c \
        ima_rw.c wav.c wve.c xa.c nulfile.c f4-fmt.c f8-fmt.c gsrt.c gsm.c lpc10.c caf.c fap.c mat4.c mat5.c paf.c pvf.c sd2.c aac-common.c m4a.c w64.c xi.c aac.c droid.c \
		mp3.c flac.c vorbis.c wavpack.c opus.c\

#LOCAL_SHARED_LIBRARIES :=
# Order is important here because static.
LOCAL_STATIC_LIBRARIES := mp4ff faad2 lpc10 gsm flac opusfile opus celt silk vorbisfile vorbisenc vorbis ogg lame mad wavpack
#LOCAL_LDLIBS := -ldl -llog

include $(BUILD_STATIC_LIBRARY)
