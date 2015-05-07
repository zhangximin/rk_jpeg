#!/bin/bash

# Makefile using libtool for vpu common library.

TOP_DIR = $(shell pwd)

# define compiler
#TOOLCHAIN ?= /user/bin
#export CROSS_COMPILER := arm-linux-gnueabihf-
#TOOLCHAIN ?= ../../toolschain/4.4.3/bin/
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
        src/vpu_api.cpp \
        src/vpu_api_private.cpp
		
#INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include
INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include -I$(TOP_DIR)/../common -I$(TOP_DIR)/../common/include \
	-I$(TOP_DIR)/..

# define generation

OBJ_C = $(SRC_CODE_C:.c=.o)
OBJ_CPP = $(SRC_CODE_CPP:.cpp=.o)

LIB_NAME = librk_on2.so

# define the build flag
CFLAGS += $(INCLUDE)
#CFLAGS += -fPIC
#LDFLAGS    

SHARED_LIB += -L../../sharelib/
SHARED_LIB += -lion -lvpu -lvpu_avcdec -lvpu_avcenc -lvpu_mjpegenc -lon2mjpeghwdec -lpost_deinterlace -lstdc++ -ldl

#RUN_LIB_DIR = -rpath $(TOP_DIR)/../../sharelib/libion.so $(TOP_DIR)/../../sharelib/libvpu.so \
	$(TOP_DIR)/../../sharelib/libvpu_avcdec.so $(TOP_DIR)/../../sharelib/libpost_deinterlace.so
RUN_LIB_DIR = 
# build steps

.PHONY : all all_src install clean

all : $(OBJ_C) $(OBJ_CPP)
#	$(CC) $(CFLAGS) $^ -shared -fPIC -o $(LIB_NAME)
	$(CXX) $(CFLAGS) $(SRC_CODE_CPP) -shared -fPIC -o $(LIB_NAME) $(SHARED_LIB) $(RUN_LIB_DIR)

#$(OBJ_C) : %.o : %.c
#	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_CPP) : %.o : %.cpp
	$(CXX) $(CFLAGS) $(SHARED_LIB) $(RUN_LIB_DIR) -c $< -o $@

#all_src :
#$(OBJ_CPP) : %.o : %.cpp
#	$(CXX) $(CFLAGS) -c $< -o $@

clean :
	@rm -rf $(OBJ_C) $(OBJ_CPP) $(LIB_NAME)

install : all
	@echo "cp "$(LIB_NAME)" to sharelib"
	@cp $(LIB_NAME) ../../sharelib/
