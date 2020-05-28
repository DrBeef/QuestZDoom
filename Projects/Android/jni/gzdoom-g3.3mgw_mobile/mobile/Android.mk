
GZDOOM_TOP_PATH := $(call my-dir)/../

include $(GZDOOM_TOP_PATH)/mobile/Android_lzma.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_zlib.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_gdtoa.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_dumb.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_gme.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_bzip2.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_src.mk




include $(GZDOOM_TOP_PATH)/mobile/Android_oplsynth.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_wildmidi.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_timidity.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_timidityplus.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_opnmidi.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_adlmidi.mk

include $(GZDOOM_TOP_PATH)/mobile/Android_zmusic.mk