/**
  ******************************************************************************
  * @file    usbh_rtlsdr.h
  * @author  Victor Pecanins <vpecanins@gmail.com>
  * @version V0.1
  * @date    25/09/2016
  * @brief   RTLSDR Driver for STM32F7 using ST's USBHost
  *
  *
  ******************************************************************************
  * @attention
  * 
  * This file can be considered a derived work from rtl-sdr.h, a part from
  * the original rtl-sdr package. The routines have been adapted to work in 
  * the STM32 USB Host environment, by incorporating them in a hierarchical
  * finite state machine.
  * 
  * 
  * It follows the original copyright notice from librtlsdr: 
  *
  * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
  * Copyright (C) 2012-2014 by Steve Markgraf <steve@steve-m.de>
  * Copyright (C) 2012 by Dimitri Stolnikov <horiz0n@gmx.net>
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  * 
  * 
  * In order to write the code for a specific Class for USBH, the code 
  * of CDC Class (found in Middlewares on the ST Cube F7 package) has been
  * modified. It follows the original notice from the ST Middleware code:
  * 
  *   * <h2><center>&copy; COPYRIGHT 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Define to prevent recursive  ----------------------------------------------*/
#ifndef __USBH_RTLSDR_H
#define __USBH_RTLSDR_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbh_core.h"
#include "stm32746g_discovery.h"
#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery_sdram.h"

/** @addtogroup USBH_LIB
* @{
*/

/** @addtogroup USBH_CLASS
* @{
*/

/** @addtogroup USBH_RTLSDR_CLASS
* @{
*/

/** @defgroup USBH_RTLSDR_CLASS
* @brief This file is the Header file for usbh_template.c
* @{
*/ 

/*USB Class codes*/
#define USB_RTLSDR_CLASS                                        0xFF

/*USB Sub class codes*/
#define USB_RTLSDR_SUBCLASS                                     0xFF

/*USB Control Protocol Codes*/
#define VENDOR_SPECIFIC                                         0xFF

/**
  * @}
  */ 

/** @defgroup USBH_RTLSDR_CLASS_Exported_Types
* @{
*/ 

/* States for RTLSDR State Machines */
/* Initialization FSM (USBH_ClassRequest) */
typedef enum
{
  RTLSDR_REQ_STARTWAIT= 0,
  RTLSDR_REQ_INC,
  RTLSDR_REQ_COMPLETE,
}
RTLSDR_ReqStateTypeDef;

/* Demod_write_reg FSM */
typedef enum
{
  RTLSDR_DEM_WRITE_WAIT= 0,
  RTLSDR_DEM_READ_WAIT,
}
RTLSDR_DemodStateTypeDef;

/* FIR Coefficients FSM */
typedef enum
{
  RTLSDR_FIR_CALC= 0,
  RTLSDR_FIR_WRITE_WAIT,
  RTLSDR_FIR_INC,
  RTLSDR_FIR_COMPLETE,
}
RTLSDR_FirStateTypeDef;

/* Probe tuners FSM */
typedef enum
{
  RTLSDR_PROBE_E4000= 0,
  RTLSDR_PROBE_FC0013,
  RTLSDR_PROBE_R820T,
  RTLSDR_PROBE_R828D,
  RTLSDR_PROBE_COMPLETE
}
RTLSDR_ProbeStateTypeDef;

/* I2C Read Reg FSM */
typedef enum
{
  RTLSDR_I2C_WRITE_WAIT= 0,
  RTLSDR_I2C_READ_WAIT,
}
RTLSDR_I2CStateTypeDef;

/* Bulk SDR data xfer FSM */
typedef enum
{
  RTLSDR_XFER_START= 0,
  RTLSDR_XFER_WAIT,
  RTLSDR_XFER_COMPLETE
}
RTLSDR_xferStateTypeDef;

/* Structure for RTLSDR Sample Stream EP */
typedef struct
{
  uint8_t              SdrPipe; 
  uint8_t              SdrEp;
  uint16_t             SdrEpSize;
  uint32_t             buffSize;
  uint8_t*             buff;
}
RTLSDR_CommItfTypedef ;

/* RTLSDR Tuner Interface */
/* All the tuner modules should implement these functions */
typedef struct 
{
  const char          *Name; 
  USBH_StatusTypeDef  (*Init)         (struct _USBH_HandleTypeDef *phost);
  USBH_StatusTypeDef  (*InitProcess)  (struct _USBH_HandleTypeDef *phost);
  USBH_StatusTypeDef  (*SetBW)        (struct _USBH_HandleTypeDef *phost);
  /*USBH_StatusTypeDef  (*DeInit)       (struct _USBH_HandleTypeDef *phost);
  USBH_StatusTypeDef  (*Requests)     (struct _USBH_HandleTypeDef *phost);  
  USBH_StatusTypeDef  (*BgndProcess)  (struct _USBH_HandleTypeDef *phost);
  USBH_StatusTypeDef  (*SOFProcess)   (struct _USBH_HandleTypeDef *phost);  
  void*                pData;*/
  void*               tunerData;
} RTLSDR_TunerTypeDef;


/* Structure for RTLSDR process */
typedef struct _RTLSDR_Process
{
  
  RTLSDR_CommItfTypedef             CommItf;
  RTLSDR_ReqStateTypeDef            reqState;
  uint8_t                           reqNumber;
  
  /* Demod */
  RTLSDR_DemodStateTypeDef          demodState;
  uint16_t                          demodRead;
  uint16_t                          demodReadIndex;
  uint16_t                          demodReadAddr;
  uint8_t                           demodReadData[4];
  
  uint16_t                          demodWriteIndex;
  uint16_t                          demodWriteAddr;
  uint8_t                           demodWriteData[4];
  
  /* FIR */
  RTLSDR_FirStateTypeDef            firState;
  uint8_t                           firNumber;
  uint8_t 													fir[20];
  
  /* RTL2830 raw read and write*/
  uint16_t            							regWriteIndex;
  uint8_t														regWriteData[4];
  uint16_t           								arrReadIndex;
  uint16_t            							arrWriteIndex;
  
  /* RTL2830 I2C read and write */
  uint16_t            i2cWriteAddress;
  uint8_t							i2cWriteData[4];
  
  uint16_t            i2cReadAddress;
  uint8_t							i2cReadReg;
  uint8_t							i2cReadVal;
  uint8_t							i2cReadData[4]; /* See Note */
  
  uint32_t						bw;
  uint8_t setSampleRateState;
  uint32_t rsamp_ratio;
  uint32_t real_rsamp_ratio;
  double real_rate;
  
  uint16_t xferWaitNo;
  
  RTLSDR_ProbeStateTypeDef          probeState;
  RTLSDR_I2CStateTypeDef            i2cState;
  RTLSDR_TunerTypeDef*              tuner;
  
  RTLSDR_xferStateTypeDef						xferState;
  
  /* TIM3 handle declaration */
  TIM_HandleTypeDef    TimHandle;

  /* Timer 3 Prescaler declaration */
  uint32_t uwPrescalerValue;
  
}
RTLSDR_HandleTypeDef;

/** Note on buffer sizes ** 
 * Apparently all buffers must have a length that is multiple of
 * 4 bytes (blocks of 32 bits). If not, USB will write outside the 
 * buffer and cause a buffer overflow. This has been physically tested,
 * if the buffers are less than 4 bytes, the next variable inside the
 * struct gets altered. Probably the cause is in stm32f7xx_ll_usb.c **/

/**
* @}
*/ 

/** @defgroup USBH_RTLSDR_CLASS_Exported_Defines
* @{
*/

typedef struct RTLSDR_DONGLE {
	uint16_t vid;
	uint16_t pid;
	const char *name;
} RTLSDR_DONGLE_T;

#define DEFAULT_BUF_NUMBER	15
#define DEFAULT_BUF_LENGTH	(16 * 32 * 512)

#define DEF_RTL_XTAL_FREQ	28800000
#define MIN_RTL_XTAL_FREQ	(DEF_RTL_XTAL_FREQ - 1000)
#define MAX_RTL_XTAL_FREQ	(DEF_RTL_XTAL_FREQ + 1000)

#define CTRL_TIMEOUT	300
#define BULK_TIMEOUT	0

#define EEPROM_ADDR	0xa0

enum RTLSDR_USB_REG {
	USB_SYSCTL		= 0x2000,
	USB_CTRL		= 0x2010,
	USB_STAT		= 0x2014,
	USB_EPA_CFG		= 0x2144,
	USB_EPA_CTL		= 0x2148,
	USB_EPA_MAXPKT		= 0x2158,
	USB_EPA_MAXPKT_2	= 0x215a,
	USB_EPA_FIFO_CFG	= 0x2160,
};


enum RTLSDR_SYS_REG {
	DEMOD_CTL		= 0x3000,
	GPO			= 0x3001,
	GPI			= 0x3002,
	GPOE			= 0x3003,
	GPD			= 0x3004,
	SYSINTE			= 0x3005,
	SYSINTS			= 0x3006,
	GP_CFG0			= 0x3007,
	GP_CFG1			= 0x3008,
	SYSINTE_1		= 0x3009,
	SYSINTS_1		= 0x300a,
	DEMOD_CTL_1		= 0x300b,
	IR_SUSPEND		= 0x300c,
};
 
enum RTLSDR_BLOCKS {
	DEMODB		= 0,
	USBB			= 1,
	SYSB			= 2,
	TUNB			= 3,
	ROMB			= 4,
	IRB			  = 5,
	IICB			= 6,
};

/*
 * FIR coefficients.
 *
 * The filter is running at XTal frequency. It is symmetric filter with 32
 * coefficients. Only first 16 coefficients are specified, the other 16
 * use the same values but in reversed order. The first coefficient in
 * the array is the outer one, the last, the last is the inner one.
 * First 8 coefficients are 8 bit signed integers, the next 8 coefficients
 * are 12 bit signed integers. All coefficients have the same weight.
 *
 * Default FIR coefficients used for DAB/FM by the Windows driver,
 * the DVB driver uses different ones
 */
#define RTLSDR_FIR_LEN 16

static const int RTLSDR_FIR[RTLSDR_FIR_LEN] = {
	-54, -36, -41, -40, -32, -14, 14, 53,	/* 8 bit signed */
	101, 156, 215, 273, 327, 372, 404, 421	/* 12 bit signed */
};


/**
* @}
*/ 

/** @defgroup USBH_RTLSDR_CLASS_Exported_Macros
* @{
*/ 


#define CTRL_IN		(USB_REQ_TYPE_VENDOR | USB_D2H) // D2H = 0x80 = LIBUSB_ENDPOINT_IN
#define CTRL_OUT	(USB_REQ_TYPE_VENDOR | USB_H2D) 


/**
* @}
*/ 

/** @defgroup USBH_RTLSDR_CLASS_Exported_Variables
* @{
*/ 
extern USBH_ClassTypeDef  RTLSDR_Class;
#define USBH_RTLSDR_CLASS    &RTLSDR_Class

/**
* @}
*/ 

/** @defgroup USBH_RTLSDR_CLASS_Exported_FunctionsPrototype
* @{
*/ 
USBH_StatusTypeDef USBH_RTLSDR_IOProcess (USBH_HandleTypeDef *phost);
USBH_StatusTypeDef USBH_RTLSDR_Init (USBH_HandleTypeDef *phost);

USBH_StatusTypeDef RTLSDR_read_reg (USBH_HandleTypeDef *phost, 
                               uint8_t block, 
                               uint16_t addr, 
                               uint8_t len);

USBH_StatusTypeDef RTLSDR_read_array(USBH_HandleTypeDef *phost, 
                      uint8_t block, 
                      uint16_t addr, 
                      uint8_t *array, 
                      uint8_t len);

USBH_StatusTypeDef RTLSDR_write_array(USBH_HandleTypeDef *phost, 
                       uint8_t block, 
                       uint16_t addr, 
                       uint8_t *array, 
                       uint8_t len);

USBH_StatusTypeDef RTLSDR_i2c_read_reg(USBH_HandleTypeDef *phost, 
                            uint8_t i2c_addr, 
                            uint8_t reg);
                            
USBH_StatusTypeDef RTLSDR_i2c_write_reg(USBH_HandleTypeDef *phost, 
                            uint8_t i2c_addr, 
                            uint8_t reg, 
                            uint8_t val);
                            
USBH_StatusTypeDef RTLSDR_i2c_write(USBH_HandleTypeDef *phost, uint8_t i2c_addr, uint8_t *buffer, uint8_t);

USBH_StatusTypeDef RTLSDR_i2c_read(USBH_HandleTypeDef *phost, uint8_t i2c_addr, uint8_t *buffer, uint8_t);

USBH_StatusTypeDef RTLSDR_open(USBH_HandleTypeDef *phost) ;

USBH_StatusTypeDef RTLSDR_demod_read_reg(USBH_HandleTypeDef *phost, uint8_t page, uint16_t addr, uint8_t len);

USBH_StatusTypeDef RTLSDR_demod_write_reg(USBH_HandleTypeDef *phost, uint8_t page, uint16_t addr, uint16_t val, uint8_t len);

USBH_StatusTypeDef RTLSDR_set_i2c_repeater(USBH_HandleTypeDef *phost, int on);

USBH_StatusTypeDef RTLSDR_write_array(USBH_HandleTypeDef *phost, 
                       uint8_t block, 
                       uint16_t addr, 
                       uint8_t *array, 
                       uint8_t len);

USBH_StatusTypeDef RTLSDR_write_reg(USBH_HandleTypeDef *phost, 
                     uint8_t block, 
                     uint16_t addr, 
                     uint16_t val, 
                     uint8_t len);

USBH_StatusTypeDef RTLSDR_set_fir(USBH_HandleTypeDef *phost);

USBH_StatusTypeDef RTLSDR_probe_tuners(USBH_HandleTypeDef *phost);

/**
* @}
*/ 

#ifdef __cplusplus
}
#endif

#endif /* __USBH_RTLSDR_H */

/**
* @}
*/ 

/**
* @}
*/ 

/**
* @}
*/ 

/**
* @}
*/ 
