APP_PLATFORM := android-19

APP_CFLAGS += -Wl,--no-undefined, -march=armv8+crc

APPLICATIONMK_PATH = $(call my-dir)
NDK_MODULE_PATH := $(APPLICATIONMK_PATH)/../..

TOP_DIR			:= $(APPLICATIONMK_PATH)
SUPPORT_LIBS	:= $(APPLICATIONMK_PATH)/SupportLibs
GZDOOM_TOP_PATH := $(APPLICATIONMK_PATH)/gzdoom-g3.3mgw_mobile

APP_ALLOW_MISSING_DEPS=true

APP_SHORT_COMMANDS :=true

APP_MODULES := qzdoom 
APP_STL := c++_shared


