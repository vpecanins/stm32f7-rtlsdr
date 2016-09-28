/**
  ******************************************************************************
  * @file    tuner_e4k.c
  * @author  Victor Pecanins <vpecanins@gmail.com>
  * @version V0.1
  * @date    25/09/2016
  * @brief   RTLSDR Driver for STM32F7 using ST's USBHost
  *
  *
  ******************************************************************************
  * @attention
  * 
  * This file can be considered a derived work from tuner_e4k.c, a part from
  * the original rtl-sdr package. The routines have been adapted to work in 
  * the STM32 USB Host environment, by incorporating them in a hierarchical
  * finite state machine.
  * 
  * 
  * It follows the original copyright notice from librtlsdr: 
  *
  * Elonics E4000 tuner driver
	*
	* (C) 2011-2012 by Harald Welte <laforge@gnumonks.org>
	* (C) 2012 by Sylvain Munaut <tnt@246tNt.com>
	* (C) 2012 by Hoernchen <la@tfc-server.de>
	*
	* All Rights Reserved
	*
	* This program is free software; you can redistribute it and/or modify
	* it under the terms of the GNU General Public License as published by
	* the Free Software Foundation; either version 2 of the License, or
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

#include <tuner_e4k.h>

RTLSDR_TunerTypeDef  Tuner_E4K = 
{
  "E4K",
  E4K_Init,
  E4K_InitProcess,
  NULL,
};

/** Private defines **/


/* Convenience macros */

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MHZ(x)	((x)*1000*1000)
#define KHZ(x)	((x)*1000)

uint32_t unsigned_delta(uint32_t a, uint32_t b)
{
	if (a > b) return a - b;
	else return b - a;
}

/* look-up table bit-width -> mask */
static const uint8_t width2mask[] = {
	0, 1, 3, 7, 0xf, 0x1f, 0x3f, 0x7f, 0xff
};


/* Gain Control */

static const int8_t if_stage1_gain[] = {
	-3, 6
};

static const int8_t if_stage23_gain[] = {
	0, 3, 6, 9
};

static const int8_t if_stage4_gain[] = {
	0, 1, 2, 2
};

static const int8_t if_stage56_gain[] = {
	3, 6, 9, 12, 15, 15, 15, 15
};

static const int8_t *if_stage_gain[] = {
	0,
	if_stage1_gain,
	if_stage23_gain,
	if_stage23_gain,
	if_stage4_gain,
	if_stage56_gain,
	if_stage56_gain
};

static const uint8_t if_stage_gain_len[] = {
	0,
	ARRAY_SIZE(if_stage1_gain),
	ARRAY_SIZE(if_stage23_gain),
	ARRAY_SIZE(if_stage23_gain),
	ARRAY_SIZE(if_stage4_gain),
	ARRAY_SIZE(if_stage56_gain),
	ARRAY_SIZE(if_stage56_gain)
};

static const struct reg_field if_stage_gain_regs[] = {
	{ 0, 0, 0 },
	{ E4K_REG_GAIN3, 0, 1 },
	{ E4K_REG_GAIN3, 1, 2 },
	{ E4K_REG_GAIN3, 3, 2 },
	{ E4K_REG_GAIN3, 5, 2 },
	{ E4K_REG_GAIN4, 0, 3 },
	{ E4K_REG_GAIN4, 3, 3 }
};

static const int32_t lnagain[] = {
	-50,	0,
	-25,	1,
	0,	4,
	25,	5,
	50,	6,
	75,	7,
	100,	8,
	125,	9,
	150,	10,
	175,	11,
	200,	12,
	250,	13,
	300,	14,
};

static const int32_t enhgain[] = {
	10, 30, 50, 70
};

/* Mixer Filter */
static const uint32_t mix_filter_bw[] = {
	KHZ(27000), KHZ(27000), KHZ(27000), KHZ(27000),
	KHZ(27000), KHZ(27000), KHZ(27000), KHZ(27000),
	KHZ(4600), KHZ(4200), KHZ(3800), KHZ(3400),
	KHZ(3300), KHZ(2700), KHZ(2300), KHZ(1900)
};

/* IF RC Filter */
static const uint32_t ifrc_filter_bw[] = {
	KHZ(21400), KHZ(21000), KHZ(17600), KHZ(14700),
	KHZ(12400), KHZ(10600), KHZ(9000), KHZ(7700),
	KHZ(6400), KHZ(5300), KHZ(4400), KHZ(3400),
	KHZ(2600), KHZ(1800), KHZ(1200), KHZ(1000)
};

/* IF Channel Filter */
static const uint32_t ifch_filter_bw[] = {
	KHZ(5500), KHZ(5300), KHZ(5000), KHZ(4800),
	KHZ(4600), KHZ(4400), KHZ(4300), KHZ(4100),
	KHZ(3900), KHZ(3800), KHZ(3700), KHZ(3600),
	KHZ(3400), KHZ(3300), KHZ(3200), KHZ(3100),
	KHZ(3000), KHZ(2950), KHZ(2900), KHZ(2800),
	KHZ(2750), KHZ(2700), KHZ(2600), KHZ(2550),
	KHZ(2500), KHZ(2450), KHZ(2400), KHZ(2300),
	KHZ(2280), KHZ(2240), KHZ(2200), KHZ(2150)
};

static const uint32_t *if_filter_bw[] = {
	mix_filter_bw,
	ifch_filter_bw,
	ifrc_filter_bw,
};

static const uint32_t if_filter_bw_len[] = {
	ARRAY_SIZE(mix_filter_bw),
	ARRAY_SIZE(ifch_filter_bw),
	ARRAY_SIZE(ifrc_filter_bw),
};

static const struct reg_field if_filter_fields[] = {
	{
		E4K_REG_FILT2, 4, 4,
	},
	{
		E4K_REG_FILT3, 0, 5,
	},
	{
		E4K_REG_FILT2, 0, 4,
	}
};

static int closest_arr_idx(const uint32_t *arr, unsigned int arr_size, uint32_t freq)
{
	unsigned int i, bi = 0;
	uint32_t best_delta = 0xffffffff;

	/* iterate over the array containing a list of the center
	 * frequencies, selecting the closest one */
	for (i = 0; i < arr_size; i++) {
		uint32_t delta = unsigned_delta(freq, arr[i]);
		if (delta < best_delta) {
			best_delta = delta;
			bi = i;
		}
	}

	return bi;
}

static int find_if_bw(enum e4k_if_filter filter, uint32_t bw)
{
	if (filter >= ARRAY_SIZE(if_filter_bw)) {
		USBH_DbgLog("Usage error in find_if_bw()");
		return 0;
	}

	return closest_arr_idx(if_filter_bw[filter],
			       if_filter_bw_len[filter], bw);
}

/* Internal functions */

USBH_StatusTypeDef E4K_reg_read(USBH_HandleTypeDef *phost, uint8_t addr) {
  return RTLSDR_i2c_read_reg(phost, 
                            E4K_I2C_ADDR, 
                            addr);
}

USBH_StatusTypeDef E4K_reg_write(USBH_HandleTypeDef *phost, uint8_t addr, uint8_t data) {
  return RTLSDR_i2c_write_reg(phost, 
                              E4K_I2C_ADDR, 
                              addr, 
                              data);
}

USBH_StatusTypeDef E4K_reg_set_mask(USBH_HandleTypeDef *phost, uint8_t reg,
		     uint8_t mask, uint8_t val) {
  
  USBH_StatusTypeDef rStatus = USBH_FAIL;
  USBH_StatusTypeDef uStatus = USBH_FAIL;
  
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
  
  E4K_HandleTypeDef * E4K_Handle = 
    (E4K_HandleTypeDef*) RTLSDR_Handle->tuner->tunerData;
    
  switch (E4K_Handle->maskState) {
	  case E4K_MASK_READ:
		uStatus = E4K_reg_read(phost, reg);
		if (uStatus == USBH_OK) {
			rStatus = USBH_BUSY;
			E4K_Handle->maskState = E4K_MASK_WRITE;
		} else {
			rStatus = uStatus;
		}
	  break;
	  
	  case E4K_MASK_WRITE:
		
		if ((RTLSDR_Handle->i2cReadVal & mask) == val) {
			uStatus = USBH_OK;
		} else {
			uStatus = E4K_reg_write(phost, reg, (RTLSDR_Handle->i2cReadVal & ~mask) | (val & mask));
		}
		
		if (uStatus == USBH_OK) {
			rStatus = USBH_OK;
			E4K_Handle->maskState = E4K_MASK_READ;
		} else {
			rStatus = uStatus;
		}
	  break;
  }
  
  return rStatus;
}

USBH_StatusTypeDef E4K_enable_manual_gain(USBH_HandleTypeDef *phost, uint8_t manual) {
	
	USBH_StatusTypeDef rStatus = USBH_FAIL;
	USBH_StatusTypeDef uStatus = USBH_FAIL;

	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
	(RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;

	E4K_HandleTypeDef * E4K_Handle = 
	(E4K_HandleTypeDef*) RTLSDR_Handle->tuner->tunerData;

	if (manual) {
		switch (E4K_Handle->gainState) {
			case 0:
				/* Set LNA mode to manual */
				uStatus = E4K_reg_set_mask(phost, E4K_REG_AGC1, E4K_AGC1_MOD_MASK, E4K_AGC_MOD_SERIAL);
				if (uStatus == USBH_OK) {
					rStatus = USBH_BUSY;
					E4K_Handle->gainState=1;
				} else {
					rStatus = uStatus;
				}
			break;
			
			case 1:
				/* Set Mixer Gain Control to manual */
				uStatus = E4K_reg_set_mask(phost, E4K_REG_AGC7, E4K_AGC7_MIX_GAIN_AUTO, 0);
				if (uStatus == USBH_OK) {
					rStatus = USBH_OK;
					E4K_Handle->gainState=0;
				} else {
					rStatus = uStatus;
				}
			break;
		}
	} else {
		switch (E4K_Handle->gainState) {
			case 0:
				/* Set LNA mode to auto */
				uStatus = E4K_reg_set_mask(phost, E4K_REG_AGC1, E4K_AGC1_MOD_MASK, E4K_AGC_MOD_IF_SERIAL_LNA_AUTON);
				if (uStatus == USBH_OK) {
					rStatus = USBH_BUSY;
					E4K_Handle->gainState=1;
				} else {
					rStatus = uStatus;
				}
			break;
			
			case 1:
				/* Set Mixer Gain Control to auto */
				uStatus = E4K_reg_set_mask(phost, E4K_REG_AGC7, E4K_AGC7_MIX_GAIN_AUTO, 1);
				if (uStatus == USBH_OK) {
					rStatus = USBH_BUSY;
					E4K_Handle->gainState=2;
				} else {
					rStatus = uStatus;
				}
			break;
			
			case 2:
				/* Last step */
				uStatus = E4K_reg_set_mask(phost, E4K_REG_AGC11, 0x7, 0);
				if (uStatus == USBH_OK) {
					rStatus = USBH_OK;
					E4K_Handle->gainState=0;
				} else {
					rStatus = uStatus;
				}
			break;
		}
	}
	
	return rStatus;
};

/* IF GAIN SET Aux procedures */
static int find_stage_gain(uint8_t stage, int8_t val)
{
	const int8_t *arr;
	int i;

	if (stage >= ARRAY_SIZE(if_stage_gain)) {
		USBH_DbgLog("Usage error in find_stage_gain()");
		return 0;
	}

	arr = if_stage_gain[stage];

	for (i = 0; i < if_stage_gain_len[stage]; i++) {
		if (arr[i] == val)
			return i;
	}
	
	USBH_DbgLog("Usage error in find_stage_gain()");
	return 0;
}

USBH_StatusTypeDef E4K_if_gain_set(USBH_HandleTypeDef *phost, uint8_t stage, int8_t value) {
	USBH_StatusTypeDef rStatus = USBH_FAIL;
	USBH_StatusTypeDef uStatus = USBH_FAIL;

	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
	(RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;

	E4K_HandleTypeDef * E4K_Handle = 
	(E4K_HandleTypeDef*) RTLSDR_Handle->tuner->tunerData;
	
	switch (E4K_Handle->ifGainState) {
		case 0:
			E4K_Handle->ifg_rc = find_stage_gain(stage, value);

			// compute the bit-mask for the given gain field 
			E4K_Handle->ifg_field = &if_stage_gain_regs[stage];
			E4K_Handle->ifg_mask = width2mask[(E4K_Handle->ifg_field)->width] << (E4K_Handle->ifg_field)->shift;
			
			E4K_Handle->ifGainState = 1;
			rStatus = USBH_BUSY;
		break;
		
		case 1:
			uStatus = E4K_reg_set_mask(phost,
										(E4K_Handle->ifg_field)->reg, 
										E4K_Handle->ifg_mask, 
										E4K_Handle->ifg_rc << (E4K_Handle->ifg_field)->shift);
			if (uStatus == USBH_OK) {
				rStatus = USBH_OK;
				E4K_Handle->ifGainState=0;
			} else {
				rStatus = uStatus;
			}
		break;
	}
	
	return rStatus;
}


USBH_StatusTypeDef E4K_field_write(USBH_HandleTypeDef *phost, const struct reg_field *field, uint8_t val) {
	return E4K_reg_set_mask(phost, 
							field->reg, 
							(uint8_t)(width2mask[field->width] << field->shift), 
							val << field->shift);
}

USBH_StatusTypeDef E4K_if_filter_bw_set(USBH_HandleTypeDef *phost, enum e4k_if_filter filter,
		         uint32_t bandwidth) {
					 
	USBH_StatusTypeDef rStatus = USBH_FAIL;
	USBH_StatusTypeDef uStatus = USBH_FAIL;

	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
	(RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;

	E4K_HandleTypeDef * E4K_Handle = 
	(E4K_HandleTypeDef*) RTLSDR_Handle->tuner->tunerData;
	
	switch (E4K_Handle->iffiltState) {
		case 0:
			if (filter >= (ARRAY_SIZE(if_filter_bw))) {USBH_DbgLog("Usage error in if_filter_bw_set() %d", filter);}

			E4K_Handle->iff_bw_idx = find_if_bw(filter, bandwidth);
			E4K_Handle->iff_field = &if_filter_fields[filter];

			E4K_Handle->iffiltState = 1;
			rStatus = USBH_BUSY;
		break;
		
		case 1:
			uStatus = E4K_field_write(phost, E4K_Handle->iff_field, E4K_Handle->iff_bw_idx);
			if (uStatus == USBH_OK) {
				rStatus = USBH_OK;
				E4K_Handle->iffiltState=0;
			} else {
				rStatus = uStatus;
			}
		break;
	}
	
	return rStatus;
}

USBH_StatusTypeDef E4K_if_filter_chan_enable(USBH_HandleTypeDef *phost, uint8_t on) {
	return E4K_reg_set_mask(phost, E4K_REG_FILT3, E4K_FILT3_DISABLE,
	                        on ? 0 : E4K_FILT3_DISABLE);
}

/*
 * Simple prototype
 * 
 USBH_StatusTypeDef E4K_if_gain_set(USBH_HandleTypeDef *phost, uint8_t stage, int8_t value) {
	USBH_StatusTypeDef rStatus = USBH_FAIL;
	USBH_StatusTypeDef uStatus = USBH_FAIL;

	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
	(RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;

	E4K_HandleTypeDef * E4K_Handle = 
	(E4K_HandleTypeDef*) RTLSDR_Handle->tuner->tunerData;
	
	switch (E4K_Handle->gainState) {
		case 0:
	
		break;
	}
	
	return rStatus;
}
* 
* */

/* Functions to export */

USBH_StatusTypeDef E4K_Init(USBH_HandleTypeDef *phost) {
  
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
  
  RTLSDR_Handle->tuner->tunerData = 
      (E4K_HandleTypeDef *)USBH_malloc (sizeof(RTLSDR_HandleTypeDef));
      
  E4K_HandleTypeDef * E4K_Handle = 
    (E4K_HandleTypeDef*) RTLSDR_Handle->tuner->tunerData;
    
  E4K_Handle->initState = E4K_REQ_RUN;
  E4K_Handle->initNumber = 0;
  E4K_Handle->maskState = E4K_MASK_READ;
  E4K_Handle->gainState = 0;
  E4K_Handle->ifGainState = 0;
  E4K_Handle->iffiltState=0;
  return USBH_OK;
}

USBH_StatusTypeDef E4K_InitProcess(USBH_HandleTypeDef *phost) {
  USBH_StatusTypeDef uStatus = USBH_FAIL;
  USBH_StatusTypeDef retStatus = USBH_BUSY;
  
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
  
  E4K_HandleTypeDef * E4K_Handle = 
    (E4K_HandleTypeDef*) RTLSDR_Handle->tuner->tunerData;
  
  switch (E4K_Handle->initState) {
  
    /* Start or run the sub FSM for each init step */
    case E4K_REQ_RUN:
      switch (E4K_Handle->initNumber) {
      
        /* make a dummy i2c read or write command, will not be ACKed! */
	      case 0: uStatus = E4K_reg_read(phost, 0); break;

	      /* Make sure we reset everything and clear POR indicator */
	      case 1: uStatus = E4K_reg_write(phost, E4K_REG_MASTER1,
		      E4K_MASTER1_RESET |
		      E4K_MASTER1_NORM_STBY |
		      E4K_MASTER1_POR_DET
	      ); break;

	      /* Configure clock input */
	      case 2: uStatus = E4K_reg_write(phost, E4K_REG_CLK_INP, 0x00); break;

	      /* Disable clock output */
	      case 3: uStatus = E4K_reg_write(phost, E4K_REG_REF_CLK, 0x00); break;
	      case 4: uStatus = E4K_reg_write(phost, E4K_REG_CLKOUT_PWDN, 0x96); break;

	      /* Write some magic values into registers */
	      case 5: uStatus = E4K_reg_write(phost, 0x7e, 0x01); break;
	      case 6: uStatus = E4K_reg_write(phost, 0x7f, 0xfe); break;
	      case 7: uStatus = E4K_reg_write(phost, 0x82, 0x00); break;
	      case 8: uStatus = E4K_reg_write(phost, 0x86, 0x50); break; // polarity A 
	      case 9: uStatus = E4K_reg_write(phost, 0x87, 0x20); break;
	      case 10: uStatus = E4K_reg_write(phost, 0x88, 0x01); break;
	      case 11: uStatus = E4K_reg_write(phost, 0x9f, 0x7f); break;
	      case 12: uStatus = E4K_reg_write(phost, 0xa0, 0x07); break;
      #if 0
	      /* Set common mode voltage a bit higher for more margin 850 mv */
	      e4k_commonmode_set(phost, 4);

	      /* Initialize DC offset lookup tables */
	      e4k_dc_offset_gen_table(phost);

	      /* Enable time variant DC correction */
	      E4K_reg_write(phost, E4K_REG_DCTIME1, 0x01);
	      E4K_reg_write(phost, E4K_REG_DCTIME2, 0x01);
      #endif

	      /* Set LNA mode to manual */
	      case 13: uStatus = E4K_reg_write(phost, E4K_REG_AGC4, 0x10); break; /* High threshold */
	      case 14: uStatus = E4K_reg_write(phost, E4K_REG_AGC5, 0x04); break;	/* Low threshold */
	      case 15: uStatus = E4K_reg_write(phost, E4K_REG_AGC6, 0x1a); break;	/* LNA calib + loop rate */

	      case 16: uStatus = E4K_reg_set_mask(phost, E4K_REG_AGC1, E4K_AGC1_MOD_MASK,
		      E4K_AGC_MOD_SERIAL); break;

	      /* Set Mixer Gain Control to manual */
	      case 17: uStatus = E4K_reg_set_mask(phost, E4K_REG_AGC7, E4K_AGC7_MIX_GAIN_AUTO, 0); break;

      #if 0
	      /* Enable LNA Gain enhancement */
	      E4K_reg_set_mask(phost, E4K_REG_AGC11, 0x7,
			       E4K_AGC11_LNA_GAIN_ENH | (2 << 1));

	      /* Enable automatic IF gain mode switching */
	      E4K_reg_set_mask(phost, E4K_REG_AGC8, 0x1, E4K_AGC8_SENS_LIN_AUTO);
      #endif

	      /* Use auto-gain as default */
	      case 18: uStatus = E4K_enable_manual_gain(phost, 0); break;

	      /* Select moderate gain levels */
	      case 19: uStatus = E4K_if_gain_set(phost, 1, 6); break;
	      case 20: uStatus = E4K_if_gain_set(phost, 2, 0); break;
	      case 21: uStatus = E4K_if_gain_set(phost, 3, 0); break;
	      case 22: uStatus = E4K_if_gain_set(phost, 4, 0); break;
	      case 23: uStatus = E4K_if_gain_set(phost, 5, 9); break;
	      case 24: uStatus = E4K_if_gain_set(phost, 6, 9); break;

	      /* Set the most narrow filter we can possibly use */
	      case 25: uStatus = E4K_if_filter_bw_set(phost, E4K_IF_FILTER_MIX, KHZ(1900)); break;
	      case 26: uStatus = E4K_if_filter_bw_set(phost, E4K_IF_FILTER_RC, KHZ(1000)); break;
	      case 27: uStatus = E4K_if_filter_bw_set(phost, E4K_IF_FILTER_CHAN, KHZ(2150)); break;
	      case 28: uStatus = E4K_if_filter_chan_enable(phost, 1); break;

	      /* Disable time variant DC correction and LUT */
	      case 29: uStatus = E4K_reg_set_mask(phost, E4K_REG_DC5, 0x03, 0); break;
	      case 30: uStatus = E4K_reg_set_mask(phost, E4K_REG_DCTIME1, 0x03, 0); break;
	      case 31: uStatus = E4K_reg_set_mask(phost, E4K_REG_DCTIME2, 0x03, 0); break;
      
        default:
    
        break;
      }
      
      if (uStatus == USBH_OK) {
        //USBH_DbgLog("E4K OK %d", E4K_Handle->initNumber);
        E4K_Handle->initState = E4K_REQ_INC;
      } else if (uStatus != USBH_BUSY) { // TODO: Maybe handle USBH_UNRECOVERABLE_ERRORS here
        USBH_DbgLog("E4K Init Fail initNumber=%d, error=%d", E4K_Handle->initNumber, uStatus);
        //E4K_Handle->initNumber = 0;
      }
      
      retStatus = USBH_BUSY;
    break;
  
    /* Increment the initNumber pointing to the next initialization operation */
    case E4K_REQ_INC:
    
      if (E4K_Handle->initNumber == 31) {
        E4K_Handle->initState = E4K_REQ_COMPLETE;
        E4K_Handle->initNumber = 0;
      } else {
        E4K_Handle->initNumber++;
        E4K_Handle->initState = E4K_REQ_RUN;
      }
      retStatus = USBH_BUSY;
    break;
    
    /* Configuration complete, give back control to ClassRequest process */
    case E4K_REQ_COMPLETE:
      E4K_Handle->initState = E4K_REQ_RUN;
      retStatus = USBH_OK;
    break;
    
    default:
    
    break;
  }
  
  return retStatus;
}
