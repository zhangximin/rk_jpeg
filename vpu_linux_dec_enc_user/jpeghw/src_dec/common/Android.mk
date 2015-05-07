# Copyright 2006 The Android Open Source Project
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_DIR := $(LOCAL_PATH)/../../src_dec
LOCAL_VPU_DIR := $(LOCAL_PATH)/../../../libon2

LOCAL_MODULE := libOn2Dec_common
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := 

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
					$(LOCAL_VPU_DIR) \
					$(LOCAL_SRC_DIR) \
					$(LOCAL_SRC_DIR)/common \
					$(LOCAL_SRC_DIR)/config \
					$(LOCAL_SRC_DIR)/inc \

LOCAL_SRC_FILES := 	bqueue.c \
					refbuffer.c \
					regdrv.c \
					workaround.c

include $(BUILD_STATIC_LIBRARY)
