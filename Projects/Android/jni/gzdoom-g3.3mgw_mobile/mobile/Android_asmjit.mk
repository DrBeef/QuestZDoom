
LOCAL_PATH := $(call my-dir)/../asmjit

include $(CLEAR_VARS)


LOCAL_MODULE    := asmjit_lz

LOCAL_CFLAGS := -frtti -DASMJIT_BUILD_EMBED -DASMJIT_STATIC -DASMJIT_BUILD_X86 #-DASMJIT_ARCH_X86=1

LOCAL_LDLIBS += -llog


LOCAL_C_INCLUDES :=  $(LOCAL_PATH)/asmjit/


LOCAL_SRC_FILES =  	\
 	asmjit/core/arch.cpp \
 	asmjit/core/assembler.cpp \
 	asmjit/core/builder.cpp \
 	asmjit/core/callconv.cpp \
 	asmjit/core/codeholder.cpp \
 	asmjit/core/compiler.cpp \
 	asmjit/core/constpool.cpp \
 	asmjit/core/cpuinfo.cpp \
 	asmjit/core/emitter.cpp \
 	asmjit/core/func.cpp \
 	asmjit/core/globals.cpp \
 	asmjit/core/inst.cpp \
 	asmjit/core/jitallocator.cpp \
 	asmjit/core/jitruntime.cpp \
 	asmjit/core/logging.cpp \
 	asmjit/core/operand.cpp \
 	asmjit/core/osutils.cpp \
 	asmjit/core/ralocal.cpp \
 	asmjit/core/rapass.cpp \
 	asmjit/core/rastack.cpp \
 	asmjit/core/string.cpp \
 	asmjit/core/support.cpp \
 	asmjit/core/target.cpp \
 	asmjit/core/type.cpp \
 	asmjit/core/virtmem.cpp \
 	asmjit/core/zone.cpp \
 	asmjit/core/zonehash.cpp \
 	asmjit/core/zonelist.cpp \
 	asmjit/core/zonestack.cpp \
 	asmjit/core/zonetree.cpp \
 	asmjit/core/zonevector.cpp \
 	asmjit/x86/x86assembler.cpp \
 	asmjit/x86/x86builder.cpp \
 	asmjit/x86/x86callconv.cpp \
 	asmjit/x86/x86compiler.cpp \
 	asmjit/x86/x86features.cpp \
 	asmjit/x86/x86instapi.cpp \
 	asmjit/x86/x86instdb.cpp \
 	asmjit/x86/x86internal.cpp \
 	asmjit/x86/x86logging.cpp \
 	asmjit/x86/x86operand.cpp \
 	asmjit/x86/x86rapass.cpp \


LOCAL_LDLIBS :=  -ldl -llog

include $(BUILD_STATIC_LIBRARY)








