// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2022 Allwinnertech
 */

#ifndef _RESET_SUN55IW3_MCU_H_
#define _RESET_SUN55IW3_MCU_H_

#define RST_BUS_MCU_I2S3	0
#define RST_BUS_MCU_I2S2	1
#define RST_BUS_MCU_I2S1	2
#define RST_BUS_MCU_I2S0	3
#define RST_BUS_MCU_SPDIF	4
#define RST_BUS_MCU_DMIC	5
#define RST_BUS_MCU_AUDIO_CODEC	6
#define RST_BUS_DSP_MSG		7
#define RST_BUS_DSP_CFG		8
#define RST_BUS_MCU_NPU		9
#define RST_BUS_MCU_TIMER	10
#define RST_BUS_DSP		11
#define RST_BUS_DSP_DBG		12
#define RST_BUS_MCU_DMA		13
#define RST_BUS_PUBSRAM		14
#define RST_BUS_RV		15
#define RST_BUS_RV_DBG		16
#define RST_BUS_RV_CFG		17
#define RST_BUS_MCU_RV_MSG	18
#define RST_BUS_MCU_PWM		19

#define RST_MCU_BUS_NUMBER (RST_BUS_MCU_PWM + 1)

#endif /* _RESET_SUN55IW3_MCU_H_ */
