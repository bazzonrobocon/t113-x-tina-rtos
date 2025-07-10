#ifndef __CONFIG_VIN_SUN8IW20P1_H_
#define __CONFIG_VIN_SUN8IW20P1_H_

#include <hal_gpio.h>
#include <hal_clk.h>
#if defined CONFIG_KERNEL_FREERTOS
#include "memheap.h"
#endif

#define TVD_MEMRESERVE 0x44200000
#define TVD_MEMRESERVE_SIZE 0x600000

#define SYSCFG_REGS_BASE			0x03000000
#define RTC_REGS_BASE				0x07090000

#define TVD_TOP_BASE				0x05c00000
#define TVD0_BASE					0x05c01000

#define SUNXI_CLK_TVD				CLK_BUS_TVD_TOP
#define SUNXI_CLK_BUS_TVD			CLK_MBUS_TVIN

#define SUNXI_CLK_TVD0				CLK_TVD
#define SUNXI_CLK_BUS_TVD0			CLK_BUS_TVD
#define SUNXI_CLK_TVD0_SRC			CLK_PLL_VIDEO1

#define SUNXI_CLK_RESET_TVD_TOP		RST_BUS_TVD_TOP
#define SUNXI_CLK_RESET_TVD			RST_BUS_TVD

#define SUNXIGIC_START 				16
#define SUNXI_IRQ_TVD				(139 - SUNXIGIC_START)

//USER CONFIG
#define AGC_AUTO_ENABLE 			1
#define AGC_MANUAL_VALUE 			64
#define CAGC_ENABLE 				1
#define FLITER_USERD				1


#define TVD_COUNT					1

enum {
	TVD_TOP_CLK = 0,
	TVD_MBUS_CLK,
	TVD0_TOP_CLK_SRC,
	TVD0_TOP_CLK,
	TVD0_MBUS_CLK,
	TVD_MAX_CLK,
};

enum {
	TVD_RET = 0,
	TVD_TOP_RET ,
	TVD_MAX_RET,
};

struct tvd_clk_info {
	hal_clk_t clock;
	unsigned int clock_id;
	unsigned long frequency;
};

struct tvd_rst_clk_info {
	struct reset_control *clock;
	unsigned int clock_id;
	unsigned int type;
};

#endif