# stm32f7-rtlsdr

Portable SDR using STM32F7-Discovery board and RTLSDR USB receiver.

## Motivation

We want to make a low-cost RF spectrum analyzer / receiver using an RTL-SDR 
compatible dongle connected to an STM32F7-Discovery board. The goal is to 
demonstrate the DSP capabilities of the STM32F7 processor and the usage of the
USB Host Middleware in STM32 Cube to implement a custom class.

## Build system

The project is developed using the free, Eclipse-based IDE called 
*System Workbench for STM32* ([SW4STM32](http://www.openstm32.org/HomePage)). 
To set-up the IDE you should follow the instructions of the site. I used Eclipse
Neon for C/C++ developers under Ubuntu 16.04.

## Current status

- USBH driver is able to recognize and enumeate RTL-SDR
- Manipulating RTL2832 registers with USB Control Transfers working
- USBH driver was modifed to be able to deal with Stalled Control Transfers, that
  may arise while recognizing the RTL-SDR tuner chip by I2C. 
- Tuner chip recognition (probing) is working.
- Elonics E4000 tuner driver is working.
- The data samples from RTLSDR are successfully copied to a SDRAM buffer.

## Next tasks

- Use DMA for speeding up the data transfers from EP1 (samples)
- Perform some FFT on the samples to check what we are receiving.
- Check the correct operation of E4K_tune_freq
- Implement/Port other tuners from librtlsdr.

## Requirements

### Hardware

- STM32F7-Discovery Evaluation Board
- RTL-SDR compatible USB dongle
- Micro USB to USB OTG Adapter Cable (Look for in eBAY)
- External 5V power supply to STM32F7-Discovery.

### Software

- Linux standard build system (Make)
- GNU ARM Compiler Toolchain
- OpenOCD (properly patched for STM32F7)

## Power supply issues

The STM32F7-Discovery board connected to an RTL-SDR dongle was found to draw a total current of
approximately 650mA. This is beyond the limit of current that can be provided by a standard USB 2.0
port (500mA). Therefore an external 5V power supply must be attached to **JP2**. In addition, the 
**JP1** jumpers must be set as follows:

- *JP1 5Vext*: **ON**
- *JP1 5Vlink*: **OFF**
- *JP1 USB_FS*: **OFF**
- *JP1 USB_HS*: **OFF** 


## Screenshot

![Screenshot](/screenshot1.jpg "Screenshot of RTLSDR connected to STM32F7-Discovery board")
