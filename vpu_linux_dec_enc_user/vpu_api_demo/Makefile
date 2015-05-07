#!/bin/bash

# Makefile using libtool for vpu common library.

TOP_DIR = $(shell pwd)

# define compiler
#TOOLCHAIN ?= /user/bin
#export CROSS_COMPILER := arm-linux-gnueabihf-
#TOOLCHAIN ?= ../toolschain/4.4.3/bin/
export CROSS_COMPILER := 
#$(TOOLCHAIN)arm-none-linux-gnueabi-
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

#SRC_CODE_C = \
#	main.c

SRC_CODE_CPP = \
	main.cpp
		
#INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include
INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include -I$(TOP_DIR)/../libvpu/common -I$(TOP_DIR)/../libvpu/common/include \
	-I$(TOP_DIR)/../libvpu

# define generation

#OBJ_C = $(SRC_CODE_C:.c=.o)
#OBJ_CPP = $(SRC_CODE_CPP:.cpp=.o)

#LIB_NAME = libvpu_avcdec.so

EXC_NAME = vpu_apiDemo

# define the build flag
CFLAGS += $(INCLUDE)
CFLAGS += -fpermissive
#CFLAGS += -fPIC
#LDFLAGS    

LIB_PATH = $(TOP_DIR)/../sharelib

SHARED_LIB += -L$(LIB_PATH)
#SHARED_LIB += -lvpu -lrk_on2 -lpost_deinterlace -lvpu_avcdec -lion
SHARED_LIB +=  -lvpu -lrk_on2 -lpost_deinterlace -lvpu_avcdec -lvpu_avcenc -lvpu_mjpegenc -lon2mjpeghwdec -lion

#RUN_LIB_DIR = -rpath $(LIB_PATH)/libion.so $(LIB_PATH)/libvpu.so \
        $(LIB_PATH)/libvpu_avcdec.so $(LIB_PATH)/libvpu_avcenc.so \
	$(LIB_PATH)/libpost_deinterlace.so $(LIB_PATH)/librk_on2.so 
RUN_LIB_DIR = 

# build steps

.PHONY : all all_src install clean

all : $(OBJ_C) $(OBJ_CPP)
#	$(CC) $(CFLAGS) $^ -shared -fPIC -o $(LIB_NAME)
	$(CXX) $(CFLAGS) $(SRC_CODE_CPP) $(SHARED_LIB) $(RUN_LIB_DIR)  -o  $(EXC_NAME) 
#	$(CC) $(CFLAGS) $(SRC_CODE_C) $(SHARED_LIB)  -o  $(EXC_NAME)

$(OBJ_C) : %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_CPP) : %.o : %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

#all_src :
#$(OBJ_CPP) : %.o : %.cpp
#	$(CXX) $(CFLAGS) -c $< -o $@

clean :
	@rm -rf $(OBJ_C) $(OBJ_CPP) $(LIB_NAME) $(EXC_NAME)

install : all
	@echo "cp to vpu_api"
