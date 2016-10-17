/**
  ******************************************************************************
  * @file    usbh_rtlsdr.c
  * @author  Victor Pecanins <vpecanins@gmail.com>
  * @version V0.1
  * @date    25/09/2016
  * @brief   RTLSDR Driver for STM32F7 using ST's USBHost
  *
  *
  ******************************************************************************
  * @attention
  * 
  * This file can be considered a derived work from librtlsdr.c, a part from
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

/* Includes ------------------------------------------------------------------*/
#include "usbh_rtlsdr.h"

#include "tuner_e4k.h"
#include "tuner_fc0012.h"
#include "tuner_fc0013.h"
#include "tuner_fc2580.h"
#include "tuner_r82xx.h"

/** @addtogroup USBH_LIB
* @{
*/

/** @addtogroup USBH_CLASS
* @{
*/

/** @addtogroup USBH_RTLSDR_CLASS
* @{
*/

/** @defgroup USBH_RTLSDR_CORE 
* @brief    This file includes RTLSDR Layer Handlers for USB Host RTLSDR class.
* @{
*/ 

/** @defgroup USBH_RTLSDR_CORE_Private_TypesDefinitions
* @{
*/ 
/**
* @}
*/ 


/** @defgroup USBH_RTLSDR_CORE_Private_Defines
* @{
*/ 
/**
* @}
*/ 


/** @defgroup USBH_RTLSDR_CORE_Private_Macros
* @{
*/ 

/* two raised to the power of n */
#define TWO_POW(n)		((double)(1ULL<<(n)))

/**
* @}
*/ 


/** @defgroup USBH_RTLSDR_CORE_Private_Variables
* @{
*/
/**
* @}
*/ 


/** @defgroup USBH_RTLSDR_CORE_Private_FunctionPrototypes
* @{
*/ 

static USBH_StatusTypeDef USBH_RTLSDR_InterfaceInit  (USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef USBH_RTLSDR_InterfaceDeInit  (USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef USBH_RTLSDR_Process(USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef USBH_RTLSDR_SOFProcess(USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef USBH_RTLSDR_ClassRequest (USBH_HandleTypeDef *phost);


USBH_ClassTypeDef  RTLSDR_Class = 
{
  "RTLSDR",
  USB_RTLSDR_CLASS,
  USBH_RTLSDR_InterfaceInit,
  USBH_RTLSDR_InterfaceDeInit,
  USBH_RTLSDR_ClassRequest,
  USBH_RTLSDR_Process, 
  USBH_RTLSDR_SOFProcess,
  NULL,
};
/**
* @}
*/ 


/** @defgroup USBH_RTLSDR_CORE_Private_Functions
* @{
*/ 

/**
  * @brief  USBH_RTLSDR_InterfaceInit 
  *         The function init the RTLSDR class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_RTLSDR_InterfaceInit (USBH_HandleTypeDef *phost)
{	
  USBH_StatusTypeDef status = USBH_OK ;
  uint8_t interface;
  RTLSDR_HandleTypeDef *RTLSDR_Handle;
  
  interface = USBH_FindInterface(phost, 
                                 USB_RTLSDR_CLASS, 
                                 USB_RTLSDR_SUBCLASS, 
                                 VENDOR_SPECIFIC);
   
	if(interface == 0xFF) {
		/* No Valid Interface */
		USBH_DbgLog ("Cannot Find the interface for class: %s", phost->pActiveClass->Name);         
	} else {
		/* Found valid interface */
		USBH_DbgLog ("Found interface for class: %s", phost->pActiveClass->Name);
		USBH_SelectInterface (phost, interface);
		
		phost->pActiveClass->pData = 
		  (RTLSDR_HandleTypeDef *)USBH_malloc (sizeof(RTLSDR_HandleTypeDef));
		
		RTLSDR_Handle =  (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
		  
		/* Initialize the FSM for writing the initialization registers */
		RTLSDR_Handle->demodState = RTLSDR_DEM_WRITE_WAIT;
		RTLSDR_Handle->reqState  = RTLSDR_REQ_STARTWAIT;
		RTLSDR_Handle->reqNumber = 0;
		RTLSDR_Handle->firState = RTLSDR_FIR_CALC;
		RTLSDR_Handle->firNumber = 0;
		RTLSDR_Handle->probeState = RTLSDR_PROBE_E4000;
		RTLSDR_Handle->i2cState = RTLSDR_I2C_WRITE_WAIT;
		RTLSDR_Handle->tuner = 0;
		RTLSDR_Handle->xferState = RTLSDR_XFER_START;
		RTLSDR_Handle->setSampleRateState=0;
		  
		/*Collect the SDR sample stream endpoint address and length*/
		if(phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress & 
		   0x80) 
		{	   
			RTLSDR_Handle->CommItf.SdrEp = 
			phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;

			RTLSDR_Handle->CommItf.SdrEpSize  = 
			phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
		}
    
		USBH_DbgLog ("Sdr EP: 0x%02X, Size: %d", 
					 RTLSDR_Handle->CommItf.SdrEp,
					 RTLSDR_Handle->CommItf.SdrEpSize);
    
		/*Allocate the length for host channel number in*/
		RTLSDR_Handle->CommItf.SdrPipe = 
		  USBH_AllocPipe(phost, RTLSDR_Handle->CommItf.SdrEp);
		
		/* Open pipe for SDR sample stream endpoint */
		USBH_OpenPipe  (phost,
						RTLSDR_Handle->CommItf.SdrPipe,
						RTLSDR_Handle->CommItf.SdrEp,                            
						phost->device.address,
						phost->device.speed,
						USB_EP_TYPE_BULK,
						RTLSDR_Handle->CommItf.SdrEpSize); 
						
		RTLSDR_Handle->CommItf.buff = (uint8_t*)((uint32_t)(LCD_FB_START_ADDRESS + (RK043FN48H_WIDTH * RK043FN48H_HEIGHT * 4)));
		
    /* This should be user configurable. It gives expected throughput values from 32..127 *512 */
    RTLSDR_Handle->CommItf.buffSize = 1*512;
    
		USBH_LL_SetToggle (phost, RTLSDR_Handle->CommItf.SdrPipe, 0);
		
		/* Timer3 is used for measuring real throughput */
		__HAL_RCC_TIM5_CLK_ENABLE();

    /* Compute the prescaler value to have TIMx counter clock equal to 100000 Hz */
    RTLSDR_Handle->uwPrescalerValue = (uint32_t)((SystemCoreClock / 2) / 100000) - 1;

    /* Set TIMx instance */
    RTLSDR_Handle->TimHandle.Instance = TIM5;

    /* Initialize TIMx peripheral */
    RTLSDR_Handle->TimHandle.Init.Period            = 100000 - 1;
    RTLSDR_Handle->TimHandle.Init.Prescaler         = RTLSDR_Handle->uwPrescalerValue;
    RTLSDR_Handle->TimHandle.Init.ClockDivision     = 0;
    RTLSDR_Handle->TimHandle.Init.CounterMode       = TIM_COUNTERMODE_UP;
    RTLSDR_Handle->TimHandle.Init.RepetitionCounter = 0;

    if (HAL_TIM_Base_Init(&(RTLSDR_Handle->TimHandle)) != HAL_OK) {
      USBH_DbgLog("Unable to init Timer 3");
    }

    /* Start Channel1 */
    if (HAL_TIM_Base_Start(&(RTLSDR_Handle->TimHandle)) != HAL_OK) {
      USBH_DbgLog("Unable to start Timer 3");
    }
	}
	return status;
}
/*
uint16_t RTLSDR_read_reg (USBH_HandleTypeDef *phost, 
                          uint8_t block, 
                          uint16_t addr, 
                          uint8_t len) 
{
  
}*/

USBH_StatusTypeDef RTLSDR_write_reg(USBH_HandleTypeDef *phost, 
                     uint8_t block, 
                     uint16_t addr, 
                     uint16_t val, 
                     uint8_t len)
{
	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
		(RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;

	if(phost->RequestState == CMD_SEND) {
		
		// "index"
		RTLSDR_Handle->regWriteIndex = (block << 8) | 0x10;
		
		// "data"
		if (len == 1)
		  RTLSDR_Handle->regWriteData[0] = val & 0xff;
		else
		  RTLSDR_Handle->regWriteData[0] = val >> 8;

		RTLSDR_Handle->regWriteData[1] = val & 0xff;

		// Setup packet parameters
		phost->Control.setup.b.bmRequestType = CTRL_OUT; 
		phost->Control.setup.b.bRequest = 0;
		phost->Control.setup.b.wValue.w = addr;
		phost->Control.setup.b.wIndex.w = RTLSDR_Handle->regWriteIndex;
		phost->Control.setup.b.wLength.w = len; 
    }
    
	return USBH_CtlReq(phost, &(RTLSDR_Handle->regWriteData[0]), len);
}


USBH_StatusTypeDef RTLSDR_read_array(USBH_HandleTypeDef *phost, 
                      uint8_t block, 
                      uint16_t addr, 
                      uint8_t *array, 
                      uint8_t len)
{
	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
		(RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
	
	if(phost->RequestState == CMD_SEND) {
		// "index"
		RTLSDR_Handle->arrReadIndex = (block << 8);

		// Setup packet parameters
		phost->Control.setup.b.bmRequestType = CTRL_IN;
		phost->Control.setup.b.bRequest = 0;
		phost->Control.setup.b.wValue.w = addr;
		phost->Control.setup.b.wIndex.w = RTLSDR_Handle->arrReadIndex;
		phost->Control.setup.b.wLength.w = len; 
	}

	return USBH_CtlReq(phost, array, len);
}

USBH_StatusTypeDef RTLSDR_write_array(USBH_HandleTypeDef *phost, 
                       uint8_t block, 
                       uint16_t addr, 
                       uint8_t *array, 
                       uint8_t len)
{
	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
		(RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
	
	if(phost->RequestState == CMD_SEND) {
		
		// "index"
		RTLSDR_Handle->arrWriteIndex = (block << 8) | 0x10;

		// Setup packet parameters
		phost->Control.setup.b.bmRequestType = CTRL_OUT;
		phost->Control.setup.b.bRequest = 0;
		phost->Control.setup.b.wValue.w = addr;
		phost->Control.setup.b.wIndex.w = RTLSDR_Handle->arrWriteIndex;
		phost->Control.setup.b.wLength.w = len;   
		
	}

	return USBH_CtlReq(phost, array, len);
}

uint8_t RTLSDR_i2c_read_reg(USBH_HandleTypeDef *phost, 
                            uint8_t i2c_addr, 
                            uint8_t reg)
{
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
    
	USBH_StatusTypeDef uStatus = USBH_FAIL;
	USBH_StatusTypeDef rStatus = USBH_BUSY;

	RTLSDR_Handle->i2cReadAddress = i2c_addr;
	RTLSDR_Handle->i2cReadReg = reg;
	RTLSDR_Handle->i2cReadVal = 0x00;

	switch (RTLSDR_Handle->i2cState) {
	case RTLSDR_I2C_WRITE_WAIT:
	
	  uStatus = RTLSDR_write_array(phost, 
									IICB, 
									RTLSDR_Handle->i2cReadAddress, 
									&(RTLSDR_Handle->i2cReadReg), 
									1);
	  
	  if (uStatus == USBH_OK) {
		rStatus = USBH_BUSY;
		RTLSDR_Handle->i2cState = RTLSDR_I2C_READ_WAIT;
	  } else if (uStatus == USBH_NOT_SUPPORTED) {
		rStatus = USBH_BUSY;
	  } else {
		rStatus = uStatus;
	  }
	break;

	case RTLSDR_I2C_READ_WAIT:
	
	  uStatus = RTLSDR_read_array(phost, 
								   IICB, 
								   RTLSDR_Handle->i2cReadAddress, 
								   &(RTLSDR_Handle->i2cReadData[0]), 
								   1);
	  
	  if (uStatus == USBH_OK) {
		RTLSDR_Handle->i2cReadVal = RTLSDR_Handle->i2cReadData[0];
		RTLSDR_Handle->i2cState = RTLSDR_I2C_WRITE_WAIT;
		rStatus = uStatus;
	  } else if (uStatus == USBH_NOT_SUPPORTED) {
		rStatus = USBH_BUSY;
	  } else {
		rStatus = uStatus;
		
	  }
	break;
	}

	return rStatus;
}

/* I2C routines */
USBH_StatusTypeDef RTLSDR_i2c_write_reg(USBH_HandleTypeDef *phost, uint8_t i2c_addr, uint8_t reg, uint8_t val)
{
	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
	
	RTLSDR_Handle->i2cWriteAddress = i2c_addr;

	RTLSDR_Handle->i2cWriteData[0] = reg;
	RTLSDR_Handle->i2cWriteData[1] = val;
	
	return RTLSDR_write_array(phost, IICB, RTLSDR_Handle->i2cWriteAddress, &(RTLSDR_Handle->i2cWriteData[0]), 2);
}

USBH_StatusTypeDef RTLSDR_i2c_write(USBH_HandleTypeDef *phost, uint8_t i2c_addr, uint8_t *buffer, uint8_t len)
{
	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
	
	RTLSDR_Handle->i2cWriteAddress = i2c_addr;

	return RTLSDR_write_array(phost, IICB, RTLSDR_Handle->i2cWriteAddress, buffer, len);
}

USBH_StatusTypeDef RTLSDR_i2c_read(USBH_HandleTypeDef *phost, uint8_t i2c_addr, uint8_t *buffer, uint8_t len)
{
	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
	
	RTLSDR_Handle->i2cReadAddress = i2c_addr;

	return RTLSDR_read_array(phost, IICB, RTLSDR_Handle->i2cReadAddress, buffer, len);
}

/* Demod routines */
USBH_StatusTypeDef RTLSDR_demod_read_reg(USBH_HandleTypeDef *phost, uint8_t page, uint16_t addr, uint8_t len)
{
	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
	(RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
		
	if ( phost->RequestState == CMD_SEND ) {
		
		RTLSDR_Handle->demodReadIndex = page;
		RTLSDR_Handle->demodReadAddr = (addr << 8) | 0x20;
		
		phost->Control.setup.b.bmRequestType = CTRL_IN;
		phost->Control.setup.b.bRequest = 0;
		phost->Control.setup.b.wValue.w = RTLSDR_Handle->demodReadAddr;
		phost->Control.setup.b.wIndex.w = RTLSDR_Handle->demodReadIndex;
		phost->Control.setup.b.wLength.w = len;
	}

	USBH_StatusTypeDef uStatus = USBH_CtlReq(phost, &(RTLSDR_Handle->demodReadData[0]), len);

	RTLSDR_Handle->demodRead = (RTLSDR_Handle->demodWriteData[1] << 8) | RTLSDR_Handle->demodWriteData[0];

	return uStatus;
} 

USBH_StatusTypeDef RTLSDR_demod_write_reg(USBH_HandleTypeDef *phost, uint8_t page, uint16_t addr, uint16_t val, uint8_t len)
{
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
    
  USBH_StatusTypeDef uStatus = USBH_FAIL;
  USBH_StatusTypeDef rStatus = USBH_BUSY;
  
  switch (RTLSDR_Handle->demodState) {
    case RTLSDR_DEM_WRITE_WAIT:
		
		if ( phost->RequestState == CMD_SEND ) {
			RTLSDR_Handle->demodWriteIndex = 0x10 | page;
			RTLSDR_Handle->demodWriteAddr = (addr << 8) | 0x20;

			if (len == 1)
				RTLSDR_Handle->demodWriteData[0] = val & 0xff;
			else
				RTLSDR_Handle->demodWriteData[0] = val >> 8;

			RTLSDR_Handle->demodWriteData[1] = val & 0xff;
		  
			phost->Control.setup.b.bmRequestType = CTRL_OUT;
			phost->Control.setup.b.bRequest = 0;
			phost->Control.setup.b.wValue.w = RTLSDR_Handle->demodWriteAddr;
			phost->Control.setup.b.wIndex.w = RTLSDR_Handle->demodWriteIndex;
			phost->Control.setup.b.wLength.w = len; 
		}
		
		uStatus = USBH_CtlReq(phost, &(RTLSDR_Handle->demodWriteData[0]), len);
		
		if (uStatus == USBH_OK) {
			RTLSDR_Handle->demodState = RTLSDR_DEM_READ_WAIT;
			rStatus = USBH_BUSY; 
		} else {
			rStatus = uStatus;
		}
      
    break;
    
    case RTLSDR_DEM_READ_WAIT:
      // You really need to do this read after writing? (Why?)
		uStatus = RTLSDR_demod_read_reg(phost, 0x0a, 0x01, 1);

		if (uStatus == USBH_OK) {
			RTLSDR_Handle->demodState = RTLSDR_DEM_WRITE_WAIT;
			rStatus = USBH_OK;
		} else {
			rStatus = uStatus;
		}
	    
    break;
  }

	return rStatus;
}

/* I2C Repeater control */
USBH_StatusTypeDef RTLSDR_set_i2c_repeater(USBH_HandleTypeDef *phost, int on)
{
	return RTLSDR_demod_write_reg(phost, 1, 0x01, on ? 0x18 : 0x10, 1);
}

/* FIR routine */
USBH_StatusTypeDef RTLSDR_set_fir(USBH_HandleTypeDef *phost)
{

	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
	(RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;

	USBH_StatusTypeDef uStatus = USBH_FAIL;
	USBH_StatusTypeDef rStatus = USBH_BUSY;

	
	int i;
	int val;
	int val0;
	int val1;
	
	//USBH_DbgLog("firState=%d, firNumber=%d", RTLSDR_Handle->firState, RTLSDR_Handle->firNumber);
	//USBH_DbgLog("firState=%d, firNumber=%d", RTLSDR_Handle->firState, 0);
	switch (RTLSDR_Handle->firState) {
	  case RTLSDR_FIR_CALC:
	    /* format: int8_t[8] */
	    for (i = 0; i < 8; ++i) {
		    val = RTLSDR_FIR[i];
		    if (val < -128 || val > 127) {
			    USBH_DbgLog("Invalid FIR coefficient!");
		    }
		    RTLSDR_Handle->fir[i] = val;
	    }
	    
	    /* format: int12_t[8] */
	    for (i = 0; i < 8; i += 2) {
		    val0 = RTLSDR_FIR[8+i];
		    val1 = RTLSDR_FIR[8+i+1];
		    if (val0 < -2048 || val0 > 2047 || val1 < -2048 || val1 > 2047) {
			    USBH_DbgLog("Invalid FIR coefficient!");
		    }
		    RTLSDR_Handle->fir[8+i*3/2] = val0 >> 4;
		    RTLSDR_Handle->fir[8+i*3/2+1] = (val0 << 4) | ((val1 >> 8) & 0x0f);
		    RTLSDR_Handle->fir[8+i*3/2+2] = val1;
	    }
	    RTLSDR_Handle->firState = RTLSDR_FIR_WRITE_WAIT;
	    RTLSDR_Handle->firNumber = 0;
	  break;
    
    case RTLSDR_FIR_WRITE_WAIT:
      uStatus = RTLSDR_demod_write_reg(phost, 
										1, 
                                        0x1c + RTLSDR_Handle->firNumber, 
                                        RTLSDR_Handle->fir[RTLSDR_Handle->firNumber], 
                                        1);
      
      if (uStatus == USBH_OK) RTLSDR_Handle->firState = RTLSDR_FIR_INC;
      
      rStatus = USBH_BUSY;
    break;
    
	  /* Increment the reqNumber pointing to the next FIR reg */
    case RTLSDR_FIR_INC:
      if (RTLSDR_Handle->firNumber == 19) {
        RTLSDR_Handle->firState = RTLSDR_FIR_COMPLETE;
        RTLSDR_Handle->firNumber = 0;
      } else {
        RTLSDR_Handle->firNumber++;
        RTLSDR_Handle->firState = RTLSDR_FIR_WRITE_WAIT;
      }
      rStatus = USBH_BUSY;
    break;
    
    /* FIR writing complete, exit sub FSM */
    case RTLSDR_FIR_COMPLETE:
      RTLSDR_Handle->firState = RTLSDR_FIR_CALC;
      rStatus = USBH_OK;
    break;
    
    default:
    
    break;
  }
  
	return rStatus;
}

/**
  * @brief  USBH_RTLSDR_InterfaceDeInit 
  *         The function DeInit the Pipes used for the RTLSDR class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_RTLSDR_InterfaceDeInit (USBH_HandleTypeDef *phost)
{
  
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
  
  
  if ( RTLSDR_Handle->CommItf.SdrPipe)
  {
    USBH_ClosePipe(phost, RTLSDR_Handle->CommItf.SdrPipe);
    USBH_FreePipe  (phost, RTLSDR_Handle->CommItf.SdrPipe);
    RTLSDR_Handle->CommItf.SdrPipe = 0;     /* Reset the Channel as Free */
  }
  
  if(phost->pActiveClass->pData) {
    USBH_free (phost->pActiveClass->pData);
    phost->pActiveClass->pData = 0;
  }
  
  return USBH_OK;
}


int usbh_wdt1_n = 0;
int usbh_wdt1_s = 0;

static void USBH_Wdt1(int s) {
  if (s == usbh_wdt1_s) {
    if (usbh_wdt1_n == 100000) {
      USBH_DbgLog("ClassRequest stopped at state: %d", s);
      usbh_wdt1_n = 0;
      
    } else {
      usbh_wdt1_n++;
    }
  } else {
    usbh_wdt1_n = 0;
  }
  usbh_wdt1_s = s;
}

USBH_StatusTypeDef RTLSDR_set_test_mode(USBH_HandleTypeDef *phost, uint8_t on) {
	return RTLSDR_demod_write_reg(phost, 0, 0x19, on ? 0x03 : 0x05, 1);
}

USBH_StatusTypeDef RTLSDR_set_sample_rate (USBH_HandleTypeDef *phost, uint32_t samp_rate)
{   
  USBH_StatusTypeDef rStatus = USBH_FAIL;  
  USBH_StatusTypeDef uStatus = USBH_FAIL;
  
	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData; 
  
  switch (RTLSDR_Handle->setSampleRateState) {
		case 0:
			/* check if the rate is supported by the resampler */
			if ((samp_rate <= 225000) || (samp_rate > 3200000) ||
				 ((samp_rate > 300000) && (samp_rate <= 900000))) {
				USBH_DbgLog("Invalid sample rate: %lu Hz", samp_rate);
				rStatus = USBH_FAIL;
			}
			
			RTLSDR_Handle->rsamp_ratio = (DEF_RTL_XTAL_FREQ * TWO_POW(22)) / samp_rate;
			RTLSDR_Handle->rsamp_ratio &= 0x0ffffffc;
			RTLSDR_Handle->real_rsamp_ratio = RTLSDR_Handle->rsamp_ratio | 
				((RTLSDR_Handle->rsamp_ratio & 0x08000000) << 1);
			
			RTLSDR_Handle->real_rate = (DEF_RTL_XTAL_FREQ * TWO_POW(22)) / 
				RTLSDR_Handle->real_rsamp_ratio;
				
			if ( ((double)samp_rate) != RTLSDR_Handle->real_rate ) {
				USBH_DbgLog("Exact sample rate is: %f Hz", RTLSDR_Handle->real_rate);
				USBH_DbgLog("Rsamp_ratio: %lu Hz", RTLSDR_Handle->rsamp_ratio);
				USBH_DbgLog("Real_rsamp_ratio: %lu Hz", RTLSDR_Handle->real_rsamp_ratio);
			}
			
			rStatus = USBH_BUSY;
			RTLSDR_Handle->setSampleRateState++;
		break;
		
		case 1:
			uStatus = RTLSDR_set_i2c_repeater(phost, 1);
			if (uStatus==USBH_OK) {
				rStatus=USBH_BUSY;
				RTLSDR_Handle->setSampleRateState++;
				RTLSDR_Handle->bw=(uint32_t)RTLSDR_Handle->real_rate; /* TODO: dev->bw > 0 ? dev->bw : dev->rate */
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 2:
			uStatus = RTLSDR_Handle->tuner->SetBW(phost);
			if (uStatus==USBH_OK) {
				rStatus=USBH_BUSY;
				RTLSDR_Handle->setSampleRateState++;
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 3:
			uStatus = RTLSDR_set_i2c_repeater(phost, 1);
			if (uStatus==USBH_OK) {
				rStatus=USBH_BUSY;
				RTLSDR_Handle->setSampleRateState++;
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 4:
			uStatus = RTLSDR_demod_write_reg(phost, 1, 0x9f, (uint16_t)(RTLSDR_Handle->rsamp_ratio >> 16), 2);
			if (uStatus==USBH_OK) {
				rStatus=USBH_BUSY;
				RTLSDR_Handle->setSampleRateState++;
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 5:
			uStatus = RTLSDR_demod_write_reg(phost, 1, 0xa1, (uint16_t)(RTLSDR_Handle->rsamp_ratio & 0xffff), 2);
			if (uStatus==USBH_OK) {
				rStatus=USBH_BUSY;
				RTLSDR_Handle->setSampleRateState++;
			} else {
				rStatus=uStatus;
			}
		break;
		
		/* TODO: Ignored frequency correction (ppm=0) */
		/* This should be implemented in rtlsdr_set_sample_freq_correction*/
		case 6:
			uStatus = RTLSDR_demod_write_reg(phost, 1, 0x3f, 0, 1);
			if (uStatus==USBH_OK) {
				rStatus=USBH_BUSY;
				RTLSDR_Handle->setSampleRateState++;
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 7:
			uStatus = RTLSDR_demod_write_reg(phost, 1, 0x3f, 0, 1);
			if (uStatus==USBH_OK) {
				rStatus=USBH_BUSY;
				RTLSDR_Handle->setSampleRateState++;
			} else {
				rStatus=uStatus;
			}
		break;
		
		/* reset demod (bit 3, soft_rst) */
		case 8:
			uStatus = RTLSDR_demod_write_reg(phost, 1, 0x01, 0x14, 1);
			if (uStatus==USBH_OK) {
				rStatus=USBH_BUSY;
				RTLSDR_Handle->setSampleRateState++;
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 9:
			uStatus = RTLSDR_demod_write_reg(phost,  1, 0x01, 0x10, 1);
			if (uStatus==USBH_OK) {
				rStatus=USBH_OK;
				RTLSDR_Handle->setSampleRateState=0;
			} else {
				rStatus=uStatus;
			}
		break;
		
			/* TODO recalculate offset frequency if offset tuning is enabled */
	/*if (dev->offs_freq)
		rtlsdr_set_offset_tuning(dev, 1);*/
	}
  return rStatus;
}

/**
  * @brief  USBH_RTLSDR_ClassRequest 
  *         The function is responsible for handling Standard requests
  *         for RTLSDR class.
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_RTLSDR_ClassRequest (USBH_HandleTypeDef *phost)
{   
  USBH_StatusTypeDef rStatus = USBH_FAIL;  
  USBH_StatusTypeDef uStatus = USBH_FAIL;
  
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;  
    
  USBH_Wdt1(RTLSDR_Handle->reqState);
    
  switch (RTLSDR_Handle->reqState) {
  
    /* Start or run the sub FSM for writing registers */
    case RTLSDR_REQ_STARTWAIT:
      
      switch (RTLSDR_Handle->reqNumber) {
			/* Dummy write */
			case 0: uStatus = RTLSDR_write_reg(phost, USBB, USB_SYSCTL, 0x09, 1); break;

			/* initialize USB */
			case 1: uStatus = RTLSDR_write_reg(phost, USBB, USB_SYSCTL, 0x09, 1); break;
			case 2: uStatus = RTLSDR_write_reg(phost, USBB, USB_EPA_MAXPKT, 0x0002, 2); break;
			case 3: uStatus = RTLSDR_write_reg(phost, USBB, USB_EPA_CTL, 0x1002, 2); break;

			/* poweron demod */
			case 4: uStatus = RTLSDR_write_reg(phost, SYSB, DEMOD_CTL_1, 0x22, 1); break;

			// Note: This one causes increase of power consumption
			// The STM32F7 board must be connected to an external power supply
			case 5: uStatus = RTLSDR_write_reg(phost, SYSB, DEMOD_CTL, 0xe8, 1); break;

			/* reset demod (bit 3, soft_rst) */
			case 6: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x01, 0x14, 1); break;
			case 7: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x01, 0x10, 1); break;

			/* disable spectrum inversion and adjacent channel rejection */
			case 8: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x15, 0x00, 1); break;
			case 9: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x16, 0x0000, 2); break;

			/* clear both DDC shift and IF frequency registers  */
			case 10: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x16 + 0, 0x00, 1); break;
			case 11: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x16 + 1, 0x00, 1); break;
			case 12: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x16 + 2, 0x00, 1); break;
			case 13: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x16 + 3, 0x00, 1); break;
			case 14: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x16 + 4, 0x00, 1); break;
			case 15: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x16 + 5, 0x00, 1); break;

			/* Set FIR coefficients (This is a SUB-FSM) */
			case 16: uStatus = RTLSDR_set_fir(phost); break;

			/* enable SDR mode, disable DAGC (bit 5) */
			case 17: uStatus = RTLSDR_demod_write_reg(phost, 0, 0x19, 0x05, 1); break;

			/* init FSM state-holding register */
			case 18: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x93, 0xf0, 1); break;
			case 19: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x94, 0x0f, 1); break;

			/* disable AGC (en_dagc, bit 0) (this seems to have no effect) */
			case 20: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x11, 0x00, 1); break;

			/* disable RF and IF AGC loop */
			case 21: uStatus = RTLSDR_demod_write_reg(phost, 1, 0x04, 0x00, 1); break;

			/* disable PID filter (enable_PID = 0) */
			case 22: uStatus = RTLSDR_demod_write_reg(phost, 0, 0x61, 0x60, 1); break;

			/* opt_adc_iq = 0, default ADC_I/ADC_Q datapath */
			case 23: uStatus = RTLSDR_demod_write_reg(phost, 0, 0x06, 0x80, 1); break;

			/* Enable Zero-IF mode (en_bbin bit), DC cancellation (en_dc_est),
			* IQ estimation/compensation (en_iq_comp, en_iq_est) */
			case 24: uStatus = RTLSDR_demod_write_reg(phost, 1, 0xb1, 0x1b, 1); break;

			/* disable 4.096 MHz clock output on pin TP_CK0 */
			case 25: uStatus = RTLSDR_demod_write_reg(phost, 0, 0x0d, 0x83, 1); break;

			/* set i2c repeater */
			case 26: uStatus = RTLSDR_set_i2c_repeater(phost, 1); break;

			/* probe tuners (This is a SUB-FSM) */
			case 27: uStatus = RTLSDR_probe_tuners(phost); break;

			/* initialize tuner variables */
			case 28: uStatus = RTLSDR_Handle->tuner->Init(phost); break;

			/* Tuner initialization process */
			case 29: uStatus = RTLSDR_Handle->tuner->InitProcess(phost); break;
			
			/* Set sample rate */
			case 30: uStatus = RTLSDR_set_sample_rate(phost, 240000); break;
			
			/* Set test mode */
			case 31: uStatus = RTLSDR_set_test_mode(phost, 1); break;
			
			/* Reset RTL2832 buffer,mandatory (1) */
			case 32: uStatus = RTLSDR_write_reg(phost, USBB, USB_EPA_CTL, 0x1002, 2); break;
			
			/* Reset RTL2832 buffer,mandatory (2) */
			case 33: uStatus = RTLSDR_write_reg(phost, USBB, USB_EPA_CTL, 0x0000, 2); break;
				
		}
      
	if (uStatus == USBH_OK) {
		RTLSDR_Handle->reqState = RTLSDR_REQ_INC;
		rStatus = USBH_BUSY;
	} else if (uStatus != USBH_BUSY) { 
		// TODO: Maybe handle USBH_UNRECOVERABLE_ERRORS here
		USBH_DbgLog("Write Fail reqNumber=%d, error=%d", RTLSDR_Handle->reqNumber, uStatus);
		rStatus = uStatus;
	}
      
    break;
      
    /* Increment the reqNumber pointing to the next config reg */
    case RTLSDR_REQ_INC:
    
      //USBH_DbgLog("Completed reqNumber: %d", RTLSDR_Handle->reqNumber);
    
      if (RTLSDR_Handle->reqNumber == 33) {
        RTLSDR_Handle->reqState = RTLSDR_REQ_COMPLETE;
        RTLSDR_Handle->reqNumber = 0;
      } else {
        RTLSDR_Handle->reqNumber++;
        RTLSDR_Handle->reqState = RTLSDR_REQ_STARTWAIT;
      }
      rStatus = USBH_BUSY;
    break;
    
    /* Configuration complete, proceed to class active */
    case RTLSDR_REQ_COMPLETE:
      USBH_DbgLog("RTLSDR Init Complete");
      RTLSDR_Handle->reqState = RTLSDR_REQ_STARTWAIT;
      rStatus = USBH_OK;
    break;
    
    default:
    
    break;
  }
  
  if(rStatus == USBH_OK)
  {
    phost->pUser(phost, HOST_USER_CLASS_ACTIVE); 
  }
  
  return rStatus; 
}

/**
  * @brief  RTLSDR_probe_tuners
  *         This is a sub-FSM to check what tuner we have.
  * @param  phost: Host handle

  * @retval USBH Status
  */

USBH_StatusTypeDef RTLSDR_probe_tuners(USBH_HandleTypeDef *phost) {
  
  USBH_StatusTypeDef rStatus = USBH_FAIL;  
  USBH_StatusTypeDef uStatus = USBH_FAIL;
  
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData; 
  
  switch (RTLSDR_Handle->probeState) {
    case RTLSDR_PROBE_E4000:
        uStatus = RTLSDR_i2c_read_reg(phost, E4K_I2C_ADDR, E4K_CHECK_ADDR);
        if (uStatus == USBH_OK || uStatus == USBH_NOT_SUPPORTED) {
          if (RTLSDR_Handle->i2cReadVal == E4K_CHECK_VAL) {
            USBH_DbgLog( "Found Elonics E4000 tuner");
            RTLSDR_Handle->tuner = &Tuner_E4K;
            RTLSDR_Handle->probeState = RTLSDR_PROBE_COMPLETE;
          } else {
            USBH_DbgLog( "E4000 not found: %02X", RTLSDR_Handle->i2cReadVal);
            RTLSDR_Handle->probeState = RTLSDR_PROBE_FC0013;
          }
        rStatus = USBH_BUSY;
        } else {
          rStatus = uStatus;
        }
    break;
    
    case RTLSDR_PROBE_FC0013:
        uStatus = RTLSDR_i2c_read_reg(phost, FC0013_I2C_ADDR, FC0013_CHECK_ADDR);
        if (uStatus == USBH_OK || uStatus == USBH_NOT_SUPPORTED) {
          if (RTLSDR_Handle->i2cReadVal == FC0013_CHECK_VAL) {
            USBH_DbgLog( "Found Fitipower FC0013 tuner");
          } else {
            USBH_DbgLog( "FC0013 not found: %02X", RTLSDR_Handle->i2cReadVal);
          }
        RTLSDR_Handle->probeState = RTLSDR_PROBE_R820T;
        rStatus = USBH_BUSY;
        } else {
          rStatus = uStatus;
        }
    break;
    
    case RTLSDR_PROBE_R820T:
      uStatus = RTLSDR_i2c_read_reg(phost, R820T_I2C_ADDR, R82XX_CHECK_ADDR);
      if (uStatus == USBH_OK || uStatus == USBH_NOT_SUPPORTED) {
        if (RTLSDR_Handle->i2cReadVal == R82XX_CHECK_VAL) {
          USBH_DbgLog( "Found Rafael Micro R820T tuner");
        } else {
          USBH_DbgLog( "R820T not found: %02X", RTLSDR_Handle->i2cReadVal);
        }
        RTLSDR_Handle->probeState = RTLSDR_PROBE_R828D;
        rStatus = USBH_BUSY;
      } else {
        rStatus = uStatus;
      }
    break;
    
    case RTLSDR_PROBE_R828D:
      uStatus = RTLSDR_i2c_read_reg(phost, R828D_I2C_ADDR, R82XX_CHECK_ADDR);
      if (uStatus == USBH_OK || uStatus == USBH_NOT_SUPPORTED) {
        if (RTLSDR_Handle->i2cReadVal == R82XX_CHECK_VAL) {
          USBH_DbgLog( "Found Rafael Micro R828D tuner");
        } else {
          USBH_DbgLog( "R828D not found: %02X", RTLSDR_Handle->i2cReadVal);
        }
        RTLSDR_Handle->probeState = RTLSDR_PROBE_COMPLETE;
        rStatus = USBH_BUSY;
      } else {
        rStatus = uStatus;
      }
    break;
    
    case RTLSDR_PROBE_COMPLETE:
      RTLSDR_Handle->probeState = RTLSDR_PROBE_E4000;
      rStatus = USBH_OK;
      
      if (RTLSDR_Handle->tuner == 0) {
        USBH_DbgLog("No tuner driver available!");
      } else {
        USBH_DbgLog("Tuner driver loaded: %s", RTLSDR_Handle->tuner->Name);
      }
    break;
  }
  
  return rStatus;
}


/**
  * @brief  USBH_RTLSDR_Process 
  *         The function is for managing state machine for RTLSDR data transfers 
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_RTLSDR_Process (USBH_HandleTypeDef *phost)
{
	USBH_StatusTypeDef rStatus = USBH_FAIL;  
  USBH_URBStateTypeDef urbStatus = USBH_URB_ERROR;
  //USBH_DbgLog("Enter RTLSDR_Process");
  
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData; 
  
  switch (RTLSDR_Handle->xferState) {
		case RTLSDR_XFER_START:
            //RTLSDR_Handle->TimHandle.Instance->CNT=0;
			rStatus = USBH_BulkReceiveData(phost, 
																			&(RTLSDR_Handle->CommItf.buff[0]), 
																			RTLSDR_Handle->CommItf.buffSize,
																			RTLSDR_Handle->CommItf.SdrPipe);
			
			RTLSDR_Handle->xferState = RTLSDR_XFER_WAIT;
			RTLSDR_Handle->xferWaitNo=0;
		break;
		
		case RTLSDR_XFER_WAIT:
			RTLSDR_Handle->xferWaitNo++;
			urbStatus = USBH_LL_GetURBState(phost , RTLSDR_Handle->CommItf.SdrPipe);
			if (urbStatus == USBH_URB_DONE) {
				//USBH_DbgLog("Xfer complete %02X %02X", RTLSDR_Handle->CommItf.buff[0], RTLSDR_Handle->CommItf.buff[1]);
				USBH_DbgLog("Xfer complete %d B, %d kB/s", USBH_LL_GetLastXferSize(phost,RTLSDR_Handle->CommItf.SdrPipe), USBH_LL_GetLastXferSize(phost,RTLSDR_Handle->CommItf.SdrPipe) * 100 / RTLSDR_Handle->TimHandle.Instance->CNT);
				RTLSDR_Handle->TimHandle.Instance->CNT=0;
				rStatus = USBH_OK;
				RTLSDR_Handle->xferState = RTLSDR_XFER_COMPLETE;
			} else if (urbStatus == USBH_URB_ERROR) {
				USBH_DbgLog("Xfer error");
				rStatus = USBH_FAIL;
			}			
		break;
		
		case RTLSDR_XFER_COMPLETE:
			RTLSDR_Handle->xferState = RTLSDR_XFER_START;
			rStatus = USBH_OK;
		break;
	}
  
  return rStatus;
}

/**
  * @brief  USBH_RTLSDR_SOFProcess 
  *         The function is for managing SOF callback 
  * @param  phost: Host handle
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_RTLSDR_SOFProcess (USBH_HandleTypeDef *phost)
{
  //USBH_DbgLog("Enter RTLSDR_SOFProcess");
  return USBH_OK;  
}

/**
  * @brief  USBH_RTLSDR_Init 
  *         The function Initialize the RTLSDR function
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_RTLSDR_Init (USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef Status = USBH_BUSY;
#if (USBH_USE_OS == 1)
  osEvent event;
  
  event = osMessageGet( phost->class_ready_event, osWaitForever );
  
  if( event.status == osEventMessage )      
  {
    if(event.value.v == USBH_CLASS_EVENT)
    {
#else 
      
  while ((Status == USBH_BUSY) || (Status == USBH_FAIL))
  {
    /* Host background process */
    USBH_Process(phost);
    if(phost->gState == HOST_CLASS)
    {
#endif        
      Status = USBH_OK;
    }
  }
  return Status;   
}

/**
  * @brief  USBH_RTLSDR_IOProcess 
  *         RTLSDR RTLSDR process
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_RTLSDR_IOProcess (USBH_HandleTypeDef *phost)
{
  if (phost->device.is_connected == 1)
  {
    if(phost->gState == HOST_CLASS)
    {
      USBH_RTLSDR_Process(phost);
    }
  }
  
  return USBH_OK;
}

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


/**
* @}
*/

