#ifndef __IRRX_SUN55IW3_H__
#define __IRRX_SUN55IW3_H__

#include <hal_interrupt.h>
#include <hal_clk.h>

/* CPUX IRRX config */
#define IRRX_CPUX_BASE		0x02005000
#if defined(CONFIG_ARCH_RISCV)
#define IRRX_CPUX_IRQ		MAKE_IRQn(93, 4)
#endif

/* CPUS IRRX config */
#define IRRX_CPUS_BASE		0x07040000
#if defined(CONFIG_ARCH_RISCV)
#define IRRX_CPUS_IRQ		MAKE_IRQn(71, 0)
#endif

static struct sunxi_irrx_params_t g_sunxi_irrx_params[] = {
	/* CPUX */
	{	.port = 0,
		.reg_base = IRRX_CPUX_BASE, .irq_num = IRRX_CPUX_IRQ,
		.clk_type = HAL_SUNXI_CCU, .bus_clk_id = CLK_BUS_IRRX,
		.mclk_id = CLK_IRRX, .pclk_type = HAL_SUNXI_FIXED_CCU,
		.pclk_id = CLK_SRC_HOSC24M, .reset_type = HAL_SUNXI_RESET,
		.reset_id = RST_BUS_IRRX, .gpio_name = GPIOI(8),
		.enable_mux = 3, .disable_mux = 0,
	},
	/* CPUS */
	{	.port = 1,
		.reg_base = IRRX_CPUS_BASE, .irq_num = IRRX_CPUS_IRQ,
		.clk_type = HAL_SUNXI_R_CCU, .bus_clk_id = CLK_BUS_R_IRRX,
		.mclk_id = CLK_R_IRRX, .pclk_type = HAL_SUNXI_FIXED_CCU,
		.pclk_id = CLK_SRC_HOSC24M, .reset_type = HAL_SUNXI_R_RESET,
		.reset_id = RST_R_IRRX, .gpio_name = GPIOL(11),
		.enable_mux = 2, .disable_mux = 0,
	},
};

#define CIR_MAX_NUM ARRAY_SIZE(g_sunxi_irrx_params)

#endif /* __IRRX_SUN55IW3_H__ */
