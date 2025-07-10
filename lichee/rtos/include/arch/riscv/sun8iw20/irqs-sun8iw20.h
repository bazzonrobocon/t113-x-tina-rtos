/*
 * include/arch/riscv/mach/sun20iw2p1/irqs-sun20iw2p1.h
 *
 * Copyright (c) 2020-2021 Allwinnertech Co., Ltd.
 *      http://www.allwinnertech.com
 *
 * Author: huangshr <huangshr@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __IRQS_SUN8IW20_H
#define __IRQS_SUN8IW20_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	CPUX_MSGBOX_R	= 16,

	UART0		= 18,
	UART1		= 19,
	UART2		= 20,
	UART3		= 21,
	UART4		= 22,
	UART5		= 23,
	TWI0		= 25,
	TWI1		= 26,
	TWI2		= 27,
	TWI3		= 28,

	SPI0		= 31,
	SPI1		= 32,

	PWM		= 34,
	IR_TX		= 35,
	LEDC		= 36,
	CAN0		= 37,
	CAN1		= 38,
	SPDIF		= 39,
	MDIC		= 40,
	AUDIO_CODEC	= 41,
	I2S_PCM0	= 42,
	I2S_PCM1	= 43,
	I2S_PCM2	= 44,
	USB0_DEVICE	= 45,
	USB0_EHCI	= 46,
	USB0_OHCI	= 47,

	USB1_EHCI	= 49,
	USB1_OHCI	= 50,

	SD0		= 56,
	SD1		= 57,
	SD2		= 58,
	MSI		= 59,
	SMC		= 60,

	GMAC		= 62,
	TZMA_ERR	= 63,
	CCU_FREE	= 64,
	AHB_HREADY_TIME_OUT = 65,
	DMA_NS		= 66,
	DMA_S		= 67,
	CE_NS		= 68,
	CE_S		= 69,
	SPINLOCK	= 70,
	HSTIMER0	= 71,
	HSTIMER1	= 72,
	GPA		= 73,
	THS		= 74,
	TIMER0		= 75,
	TIMER1		= 76,
	LR		= 77,
	TPA		= 78,
	WATCHDOG	= 79,
	IOMMU		= 80,

	VE		= 82,

	GPIOB_NS	= 85,
	GPIOB_S		= 86,
	GPIOC_NS	= 87,
	GPIOC_S		= 88,
	GPIOD_NS	= 89,
	GPIOD_S		= 90,
	GPIOE_NS	= 91,
	GPIOE_S		= 92,
	GPIOF_NS	= 93,
	GPIOF_S		= 94,
	GPIOG_NS	= 95,
	GPIOG_S		= 96,

	CPUX_MSGBOX_DSP_W = 101,
	CPUX_MSGBOX_RISCV_W = 102,
	DE		= 103,
	DI		= 104,
	G2D		= 105,
	LCD		= 106,
	TV		= 107,
	DSI		= 108,
	HDMI		= 109,
	TVE		= 110,
	CSI_DMA0	= 111,
	CSI_DMA1	= 112,

	CSI_PARSER0	= 116,

	CSI_TOP_PKT	= 122,
	TVD		= 123,

	DSP_DEE		= 136,
	DSP_PFE		= 137,
	DSP_WDG		= 138,
	DSP_MBOX_CPUX_W	= 139,
	DSP_MBOX_RISCV_W = 140,
	DSP_TZMA	= 141,
	DMAC_IRQ_DSP_NS	= 142,
	DMAC_IRQ_DSP_S	= 143,
	RISCV_MBOX_RISCV = 144,
	RISCV_MBOX_DSP	= 145,
	RISCV_MBOX_CPUX	= 146,
	RISCV_WDG	= 147,
/* CPUS */
	NMI		= 152,
	PPU		= 153,
	TWD		= 154,

	R_TIMER0	= 156,
	R_TIMER1	= 157,
	R_TIMER2	= 158,
	R_TIMER3	= 159,
	ALARM0		= 160,
	IRRX		= 167,

	AHBS_HREADY_TIME_OUT = 170,
/* CPUX */
	C0_CTI0		= 176,
	C0_CTI1		= 177,

	CO_COMMTX0	= 180,
	C0_COMMTX1	= 181,

	C0_PMU0		= 188,
	C0_PMU1		= 189,

	C0_AXI_ERROR	= 192,

	AXI_WR_IRQ	= 194,
	AXI_RD_IRQ	= 195,
	DEGPWRUPREQ_OUT0= 196,
	DEGPWRUPREQ_OUT1= 197,
} IRQn_Type;

#define SUNXI_IRQ_MAX   (198)
#ifdef __cplusplus
}
#endif

#endif    /* __IRQS_SUN8IW20_H */
