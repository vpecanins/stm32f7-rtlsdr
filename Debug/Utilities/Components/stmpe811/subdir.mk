################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Utilities/Components/stmpe811/stmpe811.c 

OBJS += \
./Utilities/Components/stmpe811/stmpe811.o 

C_DEPS += \
./Utilities/Components/stmpe811/stmpe811.d 


# Each subdirectory must supply rules for building sources it contributes
Utilities/Components/stmpe811/%.o: ../Utilities/Components/stmpe811/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU GCC Compiler'
	@echo $(PWD)
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -DDEBUG -DSTM32F746G_DISCO -DSTM32F746NGHx -DSTM32F7 -DSTM32 -DUSE_HAL_DRIVER -DSTM32F746xx -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/inc" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/HAL_Driver/Inc" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/HAL_Driver/Inc/Legacy" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/STM32746G-Discovery" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Fonts" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Log" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/ft5336" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/otm8009a" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/ov9655" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/st7735" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/ampire480272" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/ft6x06" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/ampire640480" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/wm8994" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/Common" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/adv7533" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/s5k5cag" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/mx25l512" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/mfxstm32l152" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/n25q128a" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/n25q512a" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/exc7200" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/ts3510" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/rk043fn48h" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Utilities/Components/stmpe811" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/CMSIS/core" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/CMSIS/device" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Middlewares/ST/STM32_USB_Host_Library/Core/Inc" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Middlewares/ST/STM32_USB_Host_Library/Class/CDC/Inc" -I"/home/vp/Downloads/eclipse-neon/workspace/stm32-rtlsdr/Middlewares/ST/STM32_USB_Host_Library/Class/RTLSDR/Inc" -O0 -g3 -Wall -fmessage-length=0 -ffunction-sections -c -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


