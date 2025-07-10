// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2022 Allwinnertech
 */

#ifndef _RESET_SUN60IW1_DSP_H_
#define _RESET_SUN60IW1_DSP_H_

#define	RST_BUS_RV		0
#define	RST_BUS_RV_APB		1
#define	RST_BUS_RV_CFG		2
#define	RST_BUS_RV_MSG		3
#define	RST_BUS_RV_TIME		4
#define	RST_BUS_DSP_DMA		5
#define	RST_BUS_DSP_I2S		6
#define	RST_BUS_DSP_TIME	7
#define	RST_BUS_DSP_CORE	8
#define	RST_BUS_DSP_DBG		9
#define	RST_BUS_DSP_CFG		10
#define	RST_BUS_DSP_MSG		11
#define	RST_BUS_DSP_SPINLOCK	12
#define	RST_BUS_DSP_DMIC	13
#define	MRST_BUS_DSP_MSI	14
#define	HRST_BUS_DSP_MSI	15
#define	HRST_BUS_DSP_TBU	16
#define	HRST_BUS_DSP_VDD	17
#define	HRST_BUS_RV_SYS		18

#define RST_DSP_BUS_NUMBER (HRST_BUS_RV_SYS + 1)

#endif /* _RESET_SUN60IW1_DSP_H_ */
