#!/bin/bash

# Makefile using libtool for vpu common library.

TOP_DIR = $(shell pwd)

# define compiler
#TOOLCHAIN ?= /user/bin
#export CROSS_COMPILER := arm-linux-gnueabihf-
#TOOLCHAIN ?= ../../toolschain/4.4.3/bin/
export CROSS_COMPILER := 
CC     := $(CROSS_COMPILER)gcc
CXX    := $(CROSS_COMPILER)gcc
AR     := $(CROSS_COMPILER)ar
AS     := $(CROSS_COMPILER)as
LD     := $(CROSS_COMPILER)ld
RANLIB := $(CROSS_COMPILER)ranlib
STRIP  := $(CROSS_COMPILER)strip

# define the source code
#SRC_CODE_C = $(wildcard *.c)
#SRC_CODE_CPP = $(wildcard *.cpp)
SRC_CODE_C = vpu_mem.c \
                   vpu_drv.c \
                       vpu.c 
SRC_CODE_CPP = rk_list.cpp \
		ppOp.cpp
#INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include
INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include 

# define generation

OBJ_C = $(SRC_CODE_C:.c=.o)
OBJ_CPP = $(SRC_CODE_CPP:.cpp=.o)

LIB_NAME = libvpu.so

# define the build flag
CFLAGS += $(INCLUDE)
#CFLAGS += -fPIC
#LDFLAGS    

SHARED_LIB += -L../../sharelib/
SHARED_LIB +=  -lpthread -lion

#RUN_LIB_DIR = -rpath $(TOP_DIR)/../../sharelib/libion.so
RUN_LIB_DIR = 
# build steps

.PHONY : all install clean

all : $(OBJ_C) $(OBJ_CPP)
#	$(CC) $(CFLAGS) $^ -shared -fPIC -o $(LIB_NAME)
	$(CXX) $(CFLAGS) $(SRC_CODE_C) $(SRC_CODE_CPP)  -shared -fPIC -o $(LIB_NAME) $(SHARED_LIB) $(RUN_LIB_DIR)

#$(OBJ_C) : %.o : %.c
#	$(CC) $(CFLAGS) $(SHARED_LIB) $(RUN_LIB_DIR) -c $< -o $@

#$(OBJ_CPP) : %.o : %.cpp
#	$(CXX) $(CFLAGS) $(SHARED_LIB) $(RUN_LIB_DIR) -c $< -o $@

clean :
	@rm -rf $(OBJ_C) $(OBJ_CPP) $(LIB_NAME)

install : all
	@echo "cp "$(LIB_NAME)" to sharelib"
	@cp $(LIB_NAME)  ../../sharelib/
