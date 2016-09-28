#
# Makefile for USB Host RTLSDR Class
#
# The result of this is a static library that can be linked to your
# application.
#
# Commands:
#
#	make
#	make clean		Remove compiled files
#

# Build environment
SHELL = /bin/bash
MAKE = make

# Path to GCC_ARM_NONE_EABI toolchain
# GCC_ARM_NONE_EABI_PATH = ~/STM32Toolchain/gcc-arm-none-eabi-5_2-2015q4/bin
GCC_ARM_NONE_EABI_PATH = /usr/bin
CC = $(GCC_ARM_NONE_EABI_PATH)/arm-none-eabi-gcc
AR = $(GCC_ARM_NONE_EABI_PATH)/arm-none-eabi-ar

# Project parameters
LIB_NAME = libusbh-rtlsdr.a
CPU_FAMILY = STM32F7xx
CPU_MODEL_GENERAL = STM32F746xx

# Need to include: stm32f7xx.h, HAL headers, core_cm7.h, hal_conf, usbh_core
INCLUDEDIRS =
INCLUDEDIRS += ./Inc
INCLUDEDIRS += ../../../../Drivers/HAL/Inc
INCLUDEDIRS += ../../../../Drivers/BSP/Inc
INCLUDEDIRS += ../../../../Drivers/CMSIS/Device/ST/$(CPU_FAMILY)/Include
INCLUDEDIRS += ../../../../Drivers/CMSIS/Include
INCLUDEDIRS += ../../Core/Inc

# Library sources
SOURCES =  $(wildcard ./Src/*.c)
OBJ = $(SOURCES:.c=.o)

# GCC flags
CFLAGS = -Wall -g -std=c99 -Os -D $(CPU_MODEL_GENERAL) -include stm32f7xx_hal_conf.h -Werror-implicit-function-declaration
CFLAGS += -mlittle-endian -mcpu=cortex-m7 -mthumb -DARM_MATH_CM7
CFLAGS += -nostartfiles -fdata-sections -ffunction-sections 
CFLAGS += -Wl,--gc-sections -Wl,-Map=Build/$(PROJ_NAME).map 
CFLAGS +=  $(addprefix -I ,$(INCLUDEDIRS)) 

.PHONY: all

all: $(LIB_NAME)

$(LIB_NAME): $(OBJ)
	@echo
	@echo -e "\e[1;33mPacking objects into static library $@ ...\e[0m"
	$(AR) -r $@ $(OBJ)

%.o: %.c
	@echo
	@echo -e "\e[1;32mBuilding source file $< ...\e[0m"
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm $(OBJ) || true
	rm $(LIB_NAME) || true

