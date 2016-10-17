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
  E4K_SetBW,
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

static const uint32_t rf_filt_center_uhf[] = {
	MHZ(360), MHZ(380), MHZ(405), MHZ(425),
	MHZ(450), MHZ(475), MHZ(505), MHZ(540),
	MHZ(575), MHZ(615), MHZ(670), MHZ(720),
	MHZ(760), MHZ(840), MHZ(890), MHZ(970)
};

static const uint32_t rf_filt_center_l[] = {
	MHZ(1300), MHZ(1320), MHZ(1360), MHZ(1410),
	MHZ(1445), MHZ(1460), MHZ(1490), MHZ(1530),
	MHZ(1560), MHZ(1590), MHZ(1640), MHZ(1660),
	MHZ(1680), MHZ(1700), MHZ(1720), MHZ(1750)
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

/* return 4-bit index as to which RF filter to select */
static int choose_rf_filter(enum e4k_band band, uint32_t freq)
{
	int rc;

	switch (band) {
		case E4K_BAND_VHF2:
		case E4K_BAND_VHF3:
			rc = 0;
			break;
		case E4K_BAND_UHF:
			rc = closest_arr_idx(rf_filt_center_uhf,
						 ARRAY_SIZE(rf_filt_center_uhf),
						 freq);
			break;
		case E4K_BAND_L:
			rc = closest_arr_idx(rf_filt_center_l,
						 ARRAY_SIZE(rf_filt_center_l),
						 freq);
			break;
		default:
			rc = 0;
			USBH_DbgLog("Usage error in choose_rf_filter()");
			break;
	}

	return rc;
}

/***********************************************************************
 * Frequency Control */

#define E4K_FVCO_MIN_KHZ	2600000	/* 2.6 GHz */
#define E4K_FVCO_MAX_KHZ	3900000	/* 3.9 GHz */
#define E4K_PLL_Y		65536

#define OUT_OF_SPEC

#ifdef OUT_OF_SPEC
#define E4K_FLO_MIN_MHZ		50
#define E4K_FLO_MAX_MHZ		2200UL
#else
#define E4K_FLO_MIN_MHZ		64
#define E4K_FLO_MAX_MHZ		1700
#endif

struct pll_settings {
	uint32_t freq;
	uint8_t reg_synth7;
	uint8_t mult;
};

static const struct pll_settings pll_vars[] = {
	{KHZ(72400),	(1 << 3) | 7,	48},
	{KHZ(81200),	(1 << 3) | 6,	40},
	{KHZ(108300),	(1 << 3) | 5,	32},
	{KHZ(162500),	(1 << 3) | 4,	24},
	{KHZ(216600),	(1 << 3) | 3,	16},
	{KHZ(325000),	(1 << 3) | 2,	12},
	{KHZ(350000),	(1 << 3) | 1,	8},
	{KHZ(432000),	(0 << 3) | 3,	8},
	{KHZ(667000),	(0 << 3) | 2,	6},
	{KHZ(1200000),	(0 << 3) | 1,	4}
};

static int is_fvco_valid(uint32_t fvco_z)
{
	/* check if the resulting fosc is valid */
	if (fvco_z/1000 < E4K_FVCO_MIN_KHZ ||
	    fvco_z/1000 > E4K_FVCO_MAX_KHZ) {
		USBH_DbgLog("Fvco %lu invalid\n", fvco_z);
		return 0;
	}

	return 1;
}

static int is_fosc_valid(uint32_t fosc)
{
	if (fosc < MHZ(16) || fosc > MHZ(30)) {
		USBH_DbgLog("Fosc %lu invalid\n", fosc);
		return 0;
	}

	return 1;
}

/* \brief compute Fvco based on Fosc, Z and X
 * \returns positive value (Fvco in Hz), 0 in case of error */
static uint64_t compute_fvco(uint32_t f_osc, uint8_t z, uint16_t x)
{
	uint64_t fvco_z, fvco_x, fvco;

	/* We use the following transformation in order to
	 * handle the fractional part with integer arithmetic:
	 *  Fvco = Fosc * (Z + X/Y) <=> Fvco = Fosc * Z + (Fosc * X)/Y
	 * This avoids X/Y = 0.  However, then we would overflow a 32bit
	 * integer, as we cannot hold e.g. 26 MHz * 65536 either.
	 */
	fvco_z = (uint64_t)f_osc * z;

	fvco_x = ((uint64_t)f_osc * x) / E4K_PLL_Y;

	fvco = fvco_z + fvco_x;

	return fvco;
}

static uint32_t compute_flo(uint32_t f_osc, uint8_t z, uint16_t x, uint8_t r)
{
	uint64_t fvco = compute_fvco(f_osc, z, x);
	return fvco / r;
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

USBH_StatusTypeDef E4K_SetBW(USBH_HandleTypeDef *phost) 
{
	USBH_StatusTypeDef rStatus = USBH_FAIL;
	USBH_StatusTypeDef uStatus = USBH_FAIL;

	RTLSDR_HandleTypeDef *RTLSDR_Handle =  
	(RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;

	E4K_HandleTypeDef * E4K_Handle = 
	(E4K_HandleTypeDef*) RTLSDR_Handle->tuner->tunerData;
	
	switch (E4K_Handle->setBWState) {
		case 0:
			uStatus=E4K_if_filter_bw_set(phost, E4K_IF_FILTER_MIX, RTLSDR_Handle->bw);
			if (uStatus==USBH_OK) {
				rStatus=USBH_BUSY;
				E4K_Handle->setBWState++;
			} else {
				rStatus = uStatus;
			}
		break;
		
		case 1:
			uStatus=E4K_if_filter_bw_set(phost, E4K_IF_FILTER_RC, RTLSDR_Handle->bw);
			if (uStatus==USBH_OK) {
				rStatus=USBH_BUSY;
				E4K_Handle->setBWState++;
			} else {
				rStatus = uStatus;
			}
		break;
		
		case 2:
			uStatus=E4K_if_filter_bw_set(phost, E4K_IF_FILTER_CHAN, RTLSDR_Handle->bw);
			if (uStatus==USBH_OK) {
				rStatus=USBH_OK;
				E4K_Handle->setBWState=0;
			} else {
				rStatus = uStatus;
			}
		break;
	}
	return rStatus;
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

/* Tune routines */

uint32_t E4K_compute_pll_params(struct e4k_pll_params *oscp, uint32_t fosc, uint32_t intended_flo)
{
	uint32_t i;
	uint8_t r = 2;
	uint64_t intended_fvco, remainder;
	uint64_t z = 0;
	uint32_t x;
	int flo;
	int three_phase_mixing = 0;
	oscp->r_idx = 0;

	if (!is_fosc_valid(fosc))
		return 0;

	for(i = 0; i < ARRAY_SIZE(pll_vars); ++i) {
		if(intended_flo < pll_vars[i].freq) {
			three_phase_mixing = (pll_vars[i].reg_synth7 & 0x08) ? 1 : 0;
			oscp->r_idx = pll_vars[i].reg_synth7;
			r = pll_vars[i].mult;
			break;
		}
	}
	
	/* flo(max) = 1700MHz, R(max) = 48, we need 64bit! */
	intended_fvco = (uint64_t)intended_flo * r;

	/* compute integral component of multiplier */
	z = intended_fvco / fosc;

	/* compute fractional part.  this will not overflow,
	* as fosc(max) = 30MHz and z(max) = 255 */
	remainder = intended_fvco - (fosc * z);
	/* remainder(max) = 30MHz, E4K_PLL_Y = 65536 -> 64bit! */
	x = (remainder * E4K_PLL_Y) / fosc;
	/* x(max) as result of this computation is 65536 */

	flo = compute_flo(fosc, z, x, r);

	oscp->fosc = fosc;
	oscp->flo = flo;
	oscp->intended_flo = intended_flo;
	oscp->r = r;
//	oscp->r_idx = pll_vars[i].reg_synth7 & 0x0;
	oscp->threephase = three_phase_mixing;
	oscp->x = x;
	oscp->z = z;

	return flo;
}

/* \brief Automatically select apropriate RF filter based on e4k state */
int E4K_rf_filter_set(USBH_HandleTypeDef *phost)
{
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
  
  E4K_HandleTypeDef * E4K_Handle = 
    (E4K_HandleTypeDef*) RTLSDR_Handle->tuner->tunerData;
  
	return E4K_reg_set_mask(phost, E4K_REG_FILT1, 0xF, choose_rf_filter(E4K_Handle->band, E4K_Handle->vco.flo));
}

USBH_StatusTypeDef E4K_band_set(USBH_HandleTypeDef *phost, enum e4k_band band)
{
	USBH_StatusTypeDef uStatus = USBH_FAIL;
  USBH_StatusTypeDef rStatus = USBH_BUSY;
  
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
  
  E4K_HandleTypeDef * E4K_Handle = 
    (E4K_HandleTypeDef*) RTLSDR_Handle->tuner->tunerData;
  
  switch (E4K_Handle->bandSetState) {
		case 0:
			/* Write band register */
			switch (band) {
			case E4K_BAND_VHF2:
			case E4K_BAND_VHF3:
			case E4K_BAND_UHF:
				uStatus=E4K_reg_write(phost, E4K_REG_BIAS, 3);
				break;
			case E4K_BAND_L:
				uStatus=E4K_reg_write(phost, E4K_REG_BIAS, 0);
				break;
			}
			
			if (uStatus==USBH_OK) {
				E4K_Handle->bandSetState++;
				rStatus=USBH_BUSY;
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 1:
			/* workaround: if we don't reset this register before writing to it,
			* we get a gap between 325-350 MHz (1) */
			uStatus = E4K_reg_set_mask(phost, E4K_REG_SYNTH1, 0x06, 0);
			if (uStatus==USBH_OK) {
				E4K_Handle->bandSetState++;
				rStatus=USBH_BUSY;
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 2:
			/* workaround: if we don't reset this register before writing to it,
			* we get a gap between 325-350 MHz (2) */
			uStatus = E4K_reg_set_mask(phost, E4K_REG_SYNTH1, 0x06, band << 1);
			if (uStatus==USBH_OK) {
				E4K_Handle->bandSetState=0;
				E4K_Handle->band = band;
				rStatus=USBH_OK;
			} else {
				rStatus=uStatus;
			}
		break;
	}
	
	return rStatus;
}

USBH_StatusTypeDef E4K_tune_params(USBH_HandleTypeDef *phost, struct e4k_pll_params *p)
{
	USBH_StatusTypeDef uStatus = USBH_FAIL;
  USBH_StatusTypeDef rStatus = USBH_BUSY;
  
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
  
  E4K_HandleTypeDef * E4K_Handle = 
    (E4K_HandleTypeDef*) RTLSDR_Handle->tuner->tunerData;
  
  switch (E4K_Handle->tuneParamsState) {
		case 0:
			/* program R + 3phase/2phase */
			uStatus = E4K_reg_write(phost, E4K_REG_SYNTH7, p->r_idx);
			if (uStatus==USBH_OK) {
				E4K_Handle->tuneFreqState++;
				rStatus=USBH_BUSY;
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 1:
			/* program Z */
			uStatus = E4K_reg_write(phost,  E4K_REG_SYNTH3, p->z);
			if (uStatus==USBH_OK) {
				E4K_Handle->tuneFreqState++;
				rStatus=USBH_BUSY;
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 2:
			/* program X (1) */
			uStatus = E4K_reg_write(phost, E4K_REG_SYNTH4, p->x & 0xff);
			if (uStatus==USBH_OK) {
				E4K_Handle->tuneFreqState++;
				rStatus=USBH_BUSY;
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 3:
			/* program X (2) */
			uStatus = E4K_reg_write(phost, E4K_REG_SYNTH5, p->x >> 8);
			if (uStatus==USBH_OK) {
				E4K_Handle->tuneFreqState++;
				rStatus=USBH_BUSY;
				/* we're in auto calibration mode, so there's no need to trigger it */
				memcpy(&(E4K_Handle->vco), p, sizeof(E4K_Handle->vco));
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 4:
			/* set the band */
			if (E4K_Handle->vco.flo < MHZ(140))
				uStatus=E4K_band_set(phost, E4K_BAND_VHF2);
			else if (E4K_Handle->vco.flo < MHZ(350))
				uStatus=E4K_band_set(phost, E4K_BAND_VHF3);
			else if (E4K_Handle->vco.flo < MHZ(1135))
				uStatus=E4K_band_set(phost, E4K_BAND_UHF);
			else
				uStatus=E4K_band_set(phost, E4K_BAND_L);
				
			if (uStatus==USBH_OK) {
				E4K_Handle->tuneFreqState++;
				rStatus=USBH_BUSY;
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 5:
			/* select and set proper RF filter */
			uStatus = E4K_rf_filter_set(phost);
			if (uStatus==USBH_OK) {
				E4K_Handle->tuneFreqState=0;
				rStatus=USBH_OK;
			} else {
				rStatus=uStatus;
			}
		break;
	}

	return rStatus;
}

USBH_StatusTypeDef E4K_tune_freq(USBH_HandleTypeDef *phost, uint32_t freq)
{
	USBH_StatusTypeDef uStatus = USBH_FAIL;
  USBH_StatusTypeDef rStatus = USBH_BUSY;
  
  RTLSDR_HandleTypeDef *RTLSDR_Handle =  
    (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
  
  E4K_HandleTypeDef * E4K_Handle = 
    (E4K_HandleTypeDef*) RTLSDR_Handle->tuner->tunerData;
  
  switch (E4K_Handle->tuneFreqState) {
		case 0:
			/* determine PLL parameters */								 
		  E4K_compute_pll_params(&(E4K_Handle->tuneParams),
				E4K_Handle->vco.fosc, freq);
	
			E4K_Handle->tuneFreqState = 1;
			rStatus=USBH_BUSY;
	
		break;
		
		case 1:
			/* actually tune to those parameters */
			uStatus = E4K_tune_params(phost, &(E4K_Handle->tuneParams));
			
			if (uStatus==USBH_OK) {
				E4K_Handle->tuneFreqState = 2;
				rStatus=USBH_BUSY;
			} else {
				rStatus=uStatus;
			}
		break;
		
		case 2:
			/* check PLL lock */
			uStatus = E4K_reg_read(phost, E4K_REG_SYNTH1);
			
			if (uStatus==USBH_OK) {
				E4K_Handle->tuneFreqState = 0;
				rStatus=USBH_OK;
				if (!(RTLSDR_Handle->i2cReadVal & 0x01)) {
					USBH_DbgLog("PLL not locked for %lu Hz", freq);
				} else {
					USBH_DbgLog("PLL locked at %lu Hz", freq);
				}
			} else {
				rStatus=uStatus;
			}
		break;
	}
	return rStatus;
}

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
  E4K_Handle->bandSetState=0;
  E4K_Handle->tuneFreqState=0;
  E4K_Handle->tuneParamsState=0;
  E4K_Handle->vco.fosc = DEF_RTL_XTAL_FREQ;
  E4K_Handle->setBWState=0;
  
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
				
	      /* Tune some frequency */
	      case 32: uStatus = E4K_tune_freq(phost, KHZ(99700)); break;
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
    
      if (E4K_Handle->initNumber == 32) {
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
	  USBH_DbgLog("E4K Init Complete");
      E4K_Handle->initState = E4K_REQ_RUN;
      retStatus = USBH_OK;
    break;
    
    default:
    
    break;
  }
  
  return retStatus;
}
