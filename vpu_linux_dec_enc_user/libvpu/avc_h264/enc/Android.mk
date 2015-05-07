LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
						src/H264Cabac.c \
            src/H264CodeFrame.c \
            src/H264EncApi.c \
            src/h264encapi_ext.c \
            src/H264Init.c \
            src/H264Mad.c \
            src/H264NalUnit.c \
            src/H264PictureParameterSet.c \
            src/H264PutBits.c \
            src/H264RateControl.c \
            src/H264RateControl_new.c \
            src/H264Sei.c \
            src/H264SequenceParameterSet.c \
            src/H264Slice.c \
            src/H264TestId.c \
						src/vidstabalg.c \
            src/vidstabapi.c \
            src/vidstabcommon.c \
           	src/vidstabinternal.c \
           	src/encasiccontroller.c \
            src/encasiccontroller_v2.c \
            src/encpreprocess.c \
            src/pv_avcenc_api.cpp \
         	      
LOCAL_MODULE := libvpu_avcenc

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := libvpu

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/src \
 	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../../common \
 	$(LOCAL_PATH)/../../common/include \
 

include $(BUILD_STATIC_LIBRARY)
