# Copyright 2006 The Android Open Source Project
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

DEBUG_HWJPEGENC := true 

LOCAL_TOP_DIR := $(LOCAL_PATH)/../../
LOCAL_SRC_DIR := $(LOCAL_PATH)/../../src_enc
LOCAL_VPU_DIR := $(LOCAL_PATH)/../../../libon2

LOCAL_MODULE := libjpeghwenc
LOCAL_MODULE_TAGS := optional

GRAPHIC_USE := SWSCALE

ifeq ($(GRAPHIC_USE), PIXMAN)
LOCAL_CFLAGS := -DUSE_PIXMAN
LOCAL_CFLAGS += -DPIXMAN_NO_TLS -Wno-missing-field-initializers -DPACKAGE="android-pixman" -O2 -include "pixman-elf-fix.h"
endif

ifeq ($(GRAPHIC_USE), SWSCALE)
LOCAL_CFLAGS := -DUSE_SWSCALE
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := libon2jpegenc \
			On2enc_common

LOCAL_SHARED_LIBRARIES := libcutils \
			  libvpu			  

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
			$(LOCAL_VPU_DIR) \
			$(LOCAL_SRC_DIR) \
			$(LOCAL_SRC_DIR)/common \
			$(LOCAL_SRC_DIR)/inc \
			$(LOCAL_SRC_DIR)/jpeg
			
ifeq ($(GRAPHIC_USE), PIXMAN)
LOCAL_STATIC_LIBRARIES += libpixman
LOCAL_C_INCLUDES += external/pixman/pixman
else
LOCAL_SHARED_LIBRARIES += librkswscale
LOCAL_C_INCLUDES += external/libswscale
endif

LOCAL_SRC_FILES := hw_jpegenc.c

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

#ifeq ($(DEBUG_HWJPEGENC),true)
#LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_TOP_DIR := $(LOCAL_PATH)/../../
LOCAL_SRC_DIR := $(LOCAL_PATH)/../../src_enc
LOCAL_VPU_DIR := $(LOCAL_PATH)/../../../libon2

LOCAL_ARM_MODE := arm
LOCAL_STATIC_LIBRARIES := libon2jpegenc \
			On2enc_common

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
					$(LOCAL_VPU_DIR) \
					$(LOCAL_SRC_DIR) \
					$(LOCAL_SRC_DIR)/common \
					$(LOCAL_SRC_DIR)/inc \
					$(LOCAL_SRC_DIR)/jpeg \
					external/libswscale \
					external/skia/include/images \
					external/skia/include/core

LOCAL_SRC_FILES := jpegenctest.c

LOCAL_SHARED_LIBRARIES := libjpeghwenc \
				libcutils \
			  	libvpu \
				librkswscale \
				libskia
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := hwjpegenctest
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
#endif
