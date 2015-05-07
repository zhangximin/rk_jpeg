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
	src/deinter.cpp	

#INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include
INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include -I$(TOP_DIR)/../../common -I$(TOP_DIR)/../../common/include

# define generation

OBJ_C = $(SRC_CODE_C:.c=.o)
OBJ_CPP = $(SRC_CODE_CPP:.cpp=.o)

LIB_NAME = libpost_deinterlace.so

# define the build flag
CFLAGS += $(INCLUDE)
#CFLAGS += -fPIC
#LDFLAGS    

#SHARED_LIB += -L.
SHARED_LIB += -ldl

# build steps

.PHONY : all all_src install clean

all : $(OBJ_C) $(OBJ_CPP)
#	$(CC) $(CFLAGS) $^ -shared -fPIC -o $(LIB_NAME)
	$(CXX) $(CFLAGS) $(SRC_CODE_CPP) $(SHARED_LIB) -shared -fPIC -o $(LIB_NAME)

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
