/*
* Allwinner GMAC driver.
*
* Copyright(c) 2022-2027 Allwinnertech Co., Ltd.
*
* This file is licensed under the terms of the GNU General Public
* License version 2.  This program is licensed "as is" without any
* warranty of any kind, whether express or implied.
*/

#ifndef __GMAC_SUN8IW20_H__
#define __GMAC_SUN8IW20_H__

#include <hal_gpio.h>

#define GMAC_BASE	0x04500000
#define SYSCFG_BASE	0x03000030

#define GMAC_CLK	CLK_BUS_EMAC0
#define GMAC25M_CLK	CLK_EMAC0_25M
#define GMAC_RST	RST_BUS_EMAC0

#define RMII_PIN(x)	GPIOE(x)
#define RMII_PINMUX	8
#define RMII_DRVLEVEL	GPIO_DRIVING_LEVEL1

#define PHYRST		GPIOE(13)
#define IRQ_GMAC	62
#endif /* __GMAC_SUN8IW20_H__ */
