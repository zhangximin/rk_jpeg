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

SRC_CODE_C = 
#\
                jpegenctest.c

SRC_CODE_CPP = \
	SkUtils_opts_none.cpp \
	SkThread_pthread.cpp \
	SkUtils.cpp \
	SkOSFile_none.cpp \
	SkOSFile_stdio.cpp \
	SkData.cpp \
	SkMemory_malloc.cpp \
	SkString.cpp \
	SkOSFile.cpp \
	SkStream.cpp \
	SkHwJpegUtility.cpp \
	JPEGHWDecTest.cpp

#INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include
INCLUDE = -I. -I$(TOP_DIR)/include -I$(TOP_DIR)/../../src_dec/inc -I$(TOP_DIR)/../../src_dec \
	-I$(TOP_DIR)/../../src_dec/common -I$(TOP_DIR)/../../src_dec/jpeg \
	-I$(TOP_DIR)/../../libswscale -I$(TOP_DIR)/../decoder_release \
	-I$(TOP_DIR)/../../../libvpu/common/include -I$(TOP_DIR)/../../../libvpu/common

# define generation

OBJ_C = $(SRC_CODE_C:.c=.o)
OBJ_CPP = $(SRC_CODE_CPP:.cpp=.o)

LIB_NAME = testhwjpegdec

# define the build flag
CFLAGS += $(INCLUDE)
CFLAGS += -fpermissive
#CFLAGS += -fPIC
#LDFLAGS    

SHARED_LIB += -L$(TOP_DIR)/../../../sharelib/
SHARED_LIB += -lvpu -lon2jpegdec -lOn2Dec_common -ljpeghwdec

#RUN_LIB_DIR = -rpath $(TOP_DIR)/../../../sharelib/libvpu.so $(TOP_DIR)/../../../sharelib/libion.so
#RUN_LIB_DIR = -rpath $(TOP_DIR)/../../../sharelib/libvpu.so 
RUN_LIB_DIR = 

# build steps

.PHONY : all all_src clean

all : $(OBJ_C) $(OBJ_CPP)
#	$(CC) $(CFLAGS) $^ -shared -fPIC -o $(LIB_NAME)
	$(CXX) $(CFLAGS) $(SRC_CODE_C)  $(SRC_CODE_CPP) $(SHARED_LIB) -fPIC -o $(LIB_NAME) $(RUN_LIB_DIR)

$(OBJ_C) : %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_CPP) : %.o : %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

#all_src :
#$(OBJ_CPP) : %.o : %.cpp
#	$(CXX) $(CFLAGS) -c $< -o $@

clean :
	@rm -rf $(OBJ_C) $(OBJ_CPP) $(LIB_NAME)

