
GZDOOM_TOP_PATH := $(call my-dir)/../

include $(GZDOOM_TOP_PATH)/mobile/Android_lzma.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_zlib.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_gdtoa.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_dumb.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_gme.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_bzip2.mk
#include $(GZDOOM_TOP_PATH)/mobile/Android_asmjit.mk
include $(GZDOOM_TOP_PATH)/mobile/Android_src.mk




