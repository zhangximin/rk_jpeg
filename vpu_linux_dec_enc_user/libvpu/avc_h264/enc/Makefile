#!/bin/bash

# Makefile using libtool for vpu common library.

TOP_DIR = $(shell pwd)

# define compiler
#TOOLCHAIN ?= /user/bin
#export CROSS_COMPILER := arm-linux-gnueabihf-
#TOOLCHAIN ?= ../../../toolschain/4.4.3/bin/
export CROSS_COMPILER := 
CC     := $(CROSS_COMPILER)gcc
CXX    := $(CROSS_COMPILER)g++
AR     := $(CROSS_COMPILER)ar
AS     := $(CROSS_COMPILER)as
LD     := $(CROSS_COMPILER)ld
RANLIB := $(CROSS_COMPILER)ranlib
STRIP  := $(CROSS_COMPILER)strip

# define the source code
#SRC_CODE_C = $(wildcard *.c)
#SRC_CODE_CPP = $(wildcard *.cpp)

SRC_CODE_C = \
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
            src/encpreprocess.c 

SRC_CODE_CPP = \
            src/pv_avcenc_api.cpp

#INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include
INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include -I$(TOP_DIR)/../../common -I$(TOP_DIR)/../../common/include

# define generation

OBJ_C = $(SRC_CODE_C:.c=.o)
OBJ_CPP = $(SRC_CODE_CPP:.cpp=.o)

LIB_NAME = libvpu_avcenc.so

# define the build flag
CFLAGS += $(INCLUDE)
#CFLAGS += -fPIC
#LDFLAGS    

SHARED_LIB += -L$(TOP_DIR)/../../../sharelib/
SHARED_LIB += -lvpu

#RUN_LIB_DIR = -rpath $(TOP_DIR)/../../../sharelib/libvpu.so $(TOP_DIR)/../../../sharelib/libion.so
#RUN_LIB_DIR = -rpath $(TOP_DIR)/../../../sharelib/libvpu.so 
RUN_LIB_DIR = 

# build steps

.PHONY : all all_src install clean

all : $(OBJ_C) $(OBJ_CPP)
#	$(CC) $(CFLAGS) $^ -shared -fPIC -o $(LIB_NAME)
	$(CC) $(CFLAGS) $(SRC_CODE_C)  $(SRC_CODE_CPP) $(SHARED_LIB) -shared -fPIC -o $(LIB_NAME) $(RUN_LIB_DIR)

$(OBJ_C) : %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_CPP) : %.o : %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

#all_src :
#$(OBJ_CPP) : %.o : %.cpp
#	$(CXX) $(CFLAGS) -c $< -o $@

clean :
	@rm -rf $(OBJ_C) $(OBJ_CPP) $(LIB_NAME)

install : all
	@echo "cp "$(LIB_NAME)" to sharelib"
	@cp $(LIB_NAME) ../../../sharelib/
