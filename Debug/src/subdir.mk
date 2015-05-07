################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/JPEGHWDecTest.cpp \
../src/SkData.cpp \
../src/SkHwJpegUtility.cpp \
../src/SkMemory_malloc.cpp \
../src/SkOSFile.cpp \
../src/SkOSFile_none.cpp \
../src/SkOSFile_stdio.cpp \
../src/SkStream.cpp \
../src/SkString.cpp \
../src/SkThread_pthread.cpp \
../src/SkUtils.cpp \
../src/SkUtils_opts_none.cpp \
../src/rk_jpeg.cpp 

OBJS += \
./src/JPEGHWDecTest.o \
./src/SkData.o \
./src/SkHwJpegUtility.o \
./src/SkMemory_malloc.o \
./src/SkOSFile.o \
./src/SkOSFile_none.o \
./src/SkOSFile_stdio.o \
./src/SkStream.o \
./src/SkString.o \
./src/SkThread_pthread.o \
./src/SkUtils.o \
./src/SkUtils_opts_none.o \
./src/rk_jpeg.o 

CPP_DEPS += \
./src/JPEGHWDecTest.d \
./src/SkData.d \
./src/SkHwJpegUtility.d \
./src/SkMemory_malloc.d \
./src/SkOSFile.d \
./src/SkOSFile_none.d \
./src/SkOSFile_stdio.d \
./src/SkStream.d \
./src/SkString.d \
./src/SkThread_pthread.d \
./src/SkUtils.d \
./src/SkUtils_opts_none.d \
./src/rk_jpeg.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-linux-gnueabihf-g++ -I"/home/simon/currentWork/rk_jpeg/src/include" -I"/home/simon/currentWork/rk_jpeg/vpu_linux_dec_enc_user/jpeghw/src_dec/inc" -I"/home/simon/currentWork/rk_jpeg/vpu_linux_dec_enc_user/jpeghw/src_dec" -I"/home/simon/currentWork/rk_jpeg/vpu_linux_dec_enc_user/jpeghw/src_dec/common" -I"/home/simon/currentWork/rk_jpeg/vpu_linux_dec_enc_user/jpeghw/src_dec/jpeg" -I"/home/simon/currentWork/rk_jpeg/vpu_linux_dec_enc_user/jpeghw/libswscale" -I"/home/simon/currentWork/rk_jpeg/vpu_linux_dec_enc_user/jpeghw/release/decoder_release" -I"/home/simon/currentWork/rk_jpeg/vpu_linux_dec_enc_user/libvpu/common/include" -I"/home/simon/currentWork/rk_jpeg/vpu_linux_dec_enc_user/libvpu/common" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


