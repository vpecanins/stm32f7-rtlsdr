#
# Makefile for STM32F7 Discovery projects, using a similar structure 
# that in STM32 Cube package.
#
# Victor Pecanins <vpecanins@gmail.com> 
#
# Commands:
#
#	make						Compile project
#	make program		Burn program to STM32 using OpenOCD
#	make clean			Remove compiled files
# make openocd  	Run openocd configured for the target
# make cleanusb 	Only clean compiled USBH libraries
# make debug			Run GDB for real time debugging (not reliable)
#

# Build environment
SHELL = /bin/bash
MAKE = make

# Space and comma trick
comma := ,
space :=
space +=

# Path to GCC_ARM_NONE_EABI toolchain
# GCC_ARM_NONE_EABI_PATH = ~/STM32Toolchain/gcc-arm-none-eabi-5_2-2015q4/bin
GCC_ARM_NONE_EABI_PATH = /usr/bin
CC = $(GCC_ARM_NONE_EABI_PATH)/arm-none-eabi-gcc
OBJCOPY = $(GCC_ARM_NONE_EABI_PATH)/arm-none-eabi-objcopy
OBJDUMP = $(GCC_ARM_NONE_EABI_PATH)/arm-none-eabi-objdump
DEBUGGER = $(GCC_ARM_NONE_EABI_PATH)/arm-none-eabi-gdb
SIZE = $(GCC_ARM_NONE_EABI_PATH)/arm-none-eabi-size

# OPENOCD Executable path
OPENOCD_DIR = /usr/local/bin

# Configuration (cfg) file containing programming directives for OpenOCD
OPENOCD_PROC_FILE=stm32f7.cfg

# Project parameters
PROJ_NAME = stm32f7-rtlsdr
BSP_NAME = STM32746G-Discovery
CPU_FAMILY = STM32F7xx
CPU_MODEL_GENERAL = STM32F746xx

INCLUDEDIRS =
INCLUDEDIRS += ./Inc
INCLUDEDIRS += ./Drivers/BSP/Inc
INCLUDEDIRS += ./Drivers/HAL/Inc
INCLUDEDIRS += ./Drivers/CMSIS/Include
INCLUDEDIRS += ./Drivers/CMSIS/Device/ST/$(CPU_FAMILY)/Include
#INCLUDEDIRS += $(wildcard ./Drivers/BSP/Components/*)
INCLUDEDIRS += ./Drivers/USB_Host/Core/Inc
INCLUDEDIRS += ./Drivers/USB_Host/Class/RTLSDR/Inc
INCLUDEDIRS += ./Drivers/Log

# Project specific sources
SOURCES = 
SOURCES += $(shell find ./Src -name *.c)

# Additional LCD Log and syscall stub sources
SOURCES += ./Drivers/Log/lcd_log.c
SOURCES += ./Drivers/Log/syscalls.c

# Startup/Init Files (c and assembly)
SOURCES += ./Drivers/Init/startup_stm32f746xx.s 
SOURCES += $(wildcard ./Drivers/Init/*.c)

# Generate object names
OBJ1 = $(SOURCES:.c=.o)
OBJ = $(OBJ1:.s=.o)

# LDscript for linking for STM32F7 (From STM32 CUBE)
LDSCRIPT = ./Config/ldscripts/STM32F746NGHx_FLASH.ld

# GCC flags
# -nostartfiles is needed for correct LCD Log operation.
CFLAGS = -Wall -g -std=c99 -Os -D $(CPU_MODEL_GENERAL) -include stm32f7xx_hal_conf.h -Werror-implicit-function-declaration
CFLAGS += -mlittle-endian -mcpu=cortex-m7 -mthumb -DARM_MATH_CM7
CFLAGS += -nostartfiles -fdata-sections -ffunction-sections 
CFLAGS += -Wl,--gc-sections -Wl,-Map=Build/$(PROJ_NAME).map 
CFLAGS +=  $(addprefix -I ,$(INCLUDEDIRS)) 

# Static libraries
STATIC_LIBRARIES =
STATIC_LIBRARIES += ./Drivers/HAL/libstm32f7-hal.a
STATIC_LIBRARIES += ./Drivers/BSP/libstm32f7-bsp.a
STATIC_LIBRARIES += ./Drivers/USB_Host/Core/libusbh-core.a
STATIC_LIBRARIES += ./Drivers/USB_Host/Class/RTLSDR/libusbh-rtlsdr.a

CLIBS = -Wl,--start-group $(STATIC_LIBRARIES) -lgcc -lc -lg -lm -Wl,--end-group

.PHONY: all 

all: proj

proj: $(STATIC_LIBRARIES) Build/$(PROJ_NAME).elf

%.a: FORCE
	@echo  $(wildcard $(dir %)/Src/*.c)
	@echo -e "\e[1;33mBuilding static library: $@\e[0m"
	make -C $(dir $@)

FORCE:

%.o: %.c
	@echo
	@echo -e "\e[1;32mBuilding source file: $<\e[0m"
	$(CC) -c -o $@ $< $(CFLAGS)

Build/$(PROJ_NAME).elf: $(SOURCES) $(STATIC_LIBRARIES)
	@echo
	@echo -e "\e[1;34mBuilding project: $@\e[0m"
	$(CC) -Os $(CFLAGS) $^ -o $@ $(CLIBS) -T $(LDSCRIPT) -MD 
	$(OBJCOPY) -O ihex Build/$(PROJ_NAME).elf Build/$(PROJ_NAME).hex
	$(OBJCOPY) -O binary Build/$(PROJ_NAME).elf Build/$(PROJ_NAME).bin
	$(OBJDUMP) -St Build/$(PROJ_NAME).elf >Build/$(PROJ_NAME).lst
	$(SIZE) Build/$(PROJ_NAME).elf

program: proj
	$(OPENOCD_DIR)/openocd -f board/stm32f7discovery.cfg -f $(OPENOCD_PROC_FILE) -s ./Config -c "stm_flash `pwd`/Build/$(PROJ_NAME).bin" -c shutdown

openocd:
	$(OPENOCD_DIR)/openocd -f board/stm32f7discovery.cfg -f $(OPENOCD_PROC_FILE) -s ./Config

debug:
	make openocd &
	mate-terminal -e "$(DEBUGGER) -ex \"target extended-remote :3333\" Build/$(PROJ_NAME).elf"
	
cleanusb:
	make -C ./Drivers/USB_Host/Class/RTLSDR clean
	make -C ./Drivers/USB_Host/Core clean

clean:
	$(foreach dir,$(dir $(STATIC_LIBRARIES)),make -C $(dir) clean;)
	rm Build/$(PROJ_NAME).elf || true
	rm Build/$(PROJ_NAME).hex || true
	rm Build/$(PROJ_NAME).bin || true
	rm Build/$(PROJ_NAME).map || true
	rm Build/$(PROJ_NAME).lst || true
	rm Build/$(PROJ_NAME).d || true
