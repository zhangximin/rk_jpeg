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

SRC_CODE_CPP = \
	src/h264decapi.cpp \
        src/h264hwd_asic.cpp \
        src/h264hwd_decoder.cpp \
        src/h264hwd_dpb.cpp \
        src/h264hwd_storage.cpp \
        src/h264hwd_byte_stream.cpp \
        src/h264hwd_nal_unit.cpp \
        src/h264hwd_pic_order_cnt.cpp \
        src/h264hwd_pic_param_set.cpp \
        src/h264hwd_seq_param_set.cpp \
        src/h264hwd_slice_group_map.cpp \
        src/h264hwd_slice_header.cpp \
        src/h264hwd_stream.cpp \
        src/h264hwd_util.cpp \
        src/h264hwd_vlc.cpp \
        src/h264hwd_vui.cpp \
        src/pv_avcdec_api.cpp \
        src/refbuffer.cpp \
        src/regdrv.cpp
		
#INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include
INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include -I$(TOP_DIR)/../../common -I$(TOP_DIR)/../../common/include

# define generation

OBJ_C = $(SRC_CODE_C:.c=.o)
OBJ_CPP = $(SRC_CODE_CPP:.cpp=.o)

LIB_NAME = libvpu_avcdec.so

# define the build flag
CFLAGS += $(INCLUDE)
#CFLAGS += -fPIC
#LDFLAGS    

SHARED_LIB += -L$(TOP_DIR)/../../../sharelib/
SHARED_LIB += -lvpu -lion

#RUN_LIB_DIR = -rpath $(TOP_DIR)/../../../sharelib/libvpu.so $(TOP_DIR)/../../../sharelib/libion.so
#RUN_LIB_DIR = -rpath $(TOP_DIR)/../../../sharelib/libvpu.so 
RUN_LIB_DIR = 
#RUN_LIB_DIR = -rpath $(TOP_DIR)/../../../sharelib/libion.so
# $(TOP_DIR)/../../../sharelib/libvpu.so
# build steps

.PHONY : all all_src install clean

all : $(OBJ_C) $(OBJ_CPP)
#	$(CC) $(CFLAGS) $^ -shared -fPIC -o $(LIB_NAME)
	$(CC) $(CFLAGS) $(SRC_CODE_CPP) $(SHARED_LIB) -shared -fPIC -o $(LIB_NAME) $(RUN_LIB_DIR)

#$(OBJ_C) : %.o : %.c
#	$(CC) $(CFLAGS) -c $< -o $@

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
