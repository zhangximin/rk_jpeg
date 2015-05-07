# Copyright 2006 The Android Open Source Project
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_TOP_DIR := $(LOCAL_PATH)/../../
LOCAL_SRC_DIR := $(LOCAL_PATH)/../../src_dec
LOCAL_VPU_DIR := $(LOCAL_PATH)/../../../libon2


LOCAL_CFLAGS := 

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := libOn2Dec_common

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
					$(LOCAL_VPU_DIR) \
					$(LOCAL_SRC_DIR) \
					$(LOCAL_SRC_DIR)/common \
					$(LOCAL_SRC_DIR)/config \
					$(LOCAL_SRC_DIR)/inc \
					$(LOCAL_SRC_DIR)/jpeg \
					
LOCAL_SRC_FILES := 	jpegdecapi.c \
					jpegdechdrs.c \
					jpegdecinternal.c \
					jpegdecscan.c \
					jpegdecutils.c \
					dwl_test.c \
					
LOCAL_MODULE := libon2jpegdec
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)
