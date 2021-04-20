LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := flac
#LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES := $(LOCAL_PATH)/libFLAC/include/ \
$(LOCAL_PATH)/../include/ \
$(LOCAL_PATH)/../../libogg/include/ \
$(LOCAL_PATH)/../../vorbis/include/ \
$(LOCAL_PATH)/share/include/ \
$(LOCAL_PATH)/share/utf8/ \
$(LOCAL_PATH)/../include/ \
$(LOCAL_PATH)/../include/share/ \
$(LOCAL_PATH)/share/replaygain_synthesis/include/ \
$(LOCAL_PATH)/../../libogg/include/ \
$(LOCAL_PATH)/../../vorbis/include/ \
$(LOCAL_PATH)/metaflac/include/ \
$(LOCAL_PATH)/../include/ \
$(LOCAL_PATH)/../include/share \
$(LOCAL_PATH)/../../libogg/include/ \
$(LOCAL_PATH)/../../vorbis/include/ \
$(LOCAL_PATH)/flac/ \
$(LOCAL_PATH)/../include/ \
$(LOCAL_PATH)/../../libogg/include/ \
$(LOCAL_PATH)/../../vorbis/include/ \

LOCAL_SRC_FILES := libFLAC/bitmath.c \
	libFLAC/bitreader.c \
	libFLAC/bitwriter.c \
	libFLAC/cpu.c \
	libFLAC/crc.c \
	libFLAC/fixed.c \
	libFLAC/float.c \
	libFLAC/format.c \
	libFLAC/lpc.c \
	libFLAC/md5.c \
	libFLAC/memory.c \
	libFLAC/metadata_iterators.c \
	libFLAC/metadata_object.c \
	libFLAC/stream_decoder.c \
	libFLAC/stream_encoder.c \
	libFLAC/stream_encoder_framing.c \
	libFLAC/window.c \
        libFLAC/ogg_decoder_aspect.c \
	libFLAC/ogg_encoder_aspect.c \
	libFLAC/ogg_helper.c \
	libFLAC/ogg_mapping.c \
        share/getopt/getopt.c share/getopt/getopt1.c  \
        share/grabbag/cuesheet.c \
	share/grabbag/file.c \
	share/grabbag/picture.c \
	share/grabbag/replaygain.c \
	share/grabbag/seektable.c \
        share/replaygain_analysis/replaygain_analysis.c \
        share/replaygain_synthesis/replaygain_synthesis.c \
        share/utf8/charset.c share/utf8/iconvert.c share/utf8/utf8.c \
        metaflac/main.c \
	metaflac/operations.c \
	metaflac/operations_shorthand_cuesheet.c \
	metaflac/operations_shorthand_picture.c \
	metaflac/operations_shorthand_seektable.c \
	metaflac/operations_shorthand_streaminfo.c \
	metaflac/operations_shorthand_vorbiscomment.c \
	metaflac/options.c \
	metaflac/usage.c \
	metaflac/utils.c \
        flac/analyze.c \
	flac/decode.c \
	flac/encode.c \
	flac/foreign_metadata.c \
	flac/local_string_utils.c \
	flac/utils.c \
	flac/vorbiscomment.c \

#LOCAL_STATIC_LIBRARIES := ogg vorbis

#LOCAL_LDLIBS := -ldl -lGLESv1_CM -llog -L/$(DIRECTORY_TO_OBJ)
include $(BUILD_STATIC_LIBRARY)
