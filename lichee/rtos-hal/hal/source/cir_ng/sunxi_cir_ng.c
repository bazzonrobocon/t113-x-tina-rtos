/* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.

 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.

 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY¡¯S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS¡¯SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY¡¯S TECHNOLOGY.


 * THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
 * PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
 * THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
 * OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>

#include <interrupt.h>
#include <hal_clk.h>
#include <hal_gpio.h>
#include <hal_reset.h>
#include <hal_cfg.h>
#include <script.h>
#include "common_cir_ng.h"
#include "platform_cir_ng.h"
#include "sunxi_hal_cir_ng.h"

/* define this macro when debugging is required */
/* #define CONFIG_DRIVERS_IR_DEBUG */
#ifdef CONFIG_DRIVERS_IR_DEBUG
#define CIR_INFO(fmt, arg...) printf("%s()%d " fmt, __func__, __LINE__, ##arg)
#else
#define CIR_INFO(fmt, arg...) do{}while(0);
#endif

#define CIR_ERR(fmt, arg...) printf("%s()%d " fmt, __func__, __LINE__, ##arg)

#define CIR_STR_SIZE 32

static sunxi_cir_t g_sunxi_irrx[CIR_MAX_NUM];
extern struct sunxi_irrx_params_t g_sunxi_irrx_params[CIR_MAX_NUM];

void sunxi_cir_callback_register(cir_port_t port, cir_callback_t callback)
{
	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	cir->callback = callback;
}

static hal_irqreturn_t sunxi_cir_handler(void *dev)
{
	uint32_t int_flag, count;
	uint32_t reg_data, i = 0;
	struct sunxi_irrx_params_t *para;

	sunxi_cir_t *cir = (sunxi_cir_t *)dev;
	para = &g_sunxi_irrx_params[cir->port];

	int_flag = readl(para->reg_base + CIR_RXSTA);
	
	count = (int_flag & RAC) >> RAC_OFFSET;
	for(i = 0; i < count; i++) {
		reg_data = readl(para->reg_base + CIR_RXFIFO);
		if (cir->callback) {
			cir->callback(cir->port, RA, reg_data);
		}
	}

	if ((int_flag & ROI) && cir->callback)
		cir->callback(cir->port, ROI, 0);

	if ((int_flag & RPE) && cir->callback) {
		cir->callback(cir->port, RA, 0);
		cir->callback(cir->port, RPE, 0);
	}

	writel(int_flag, para->reg_base + CIR_RXSTA);

	return 0;
}

void sunxi_cir_mode_enable(cir_port_t port, uint8_t enable)
{
	struct sunxi_irrx_params_t *para;
	int reg_val;

	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	para = &g_sunxi_irrx_params[port];

	if (!cir->status)
		return ;

	reg_val = readl(para->reg_base + CIR_CTRL);

	if (enable)
		reg_val |= CIR_ENABLE;
	else
		reg_val &= ~CIR_ENABLE;

	writel(reg_val, para->reg_base + CIR_CTRL);
}

void sunxi_cir_mode_config(cir_port_t port, cir_mode_t mode)
{
	int reg_val;
	struct sunxi_irrx_params_t *para;

	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	para = &g_sunxi_irrx_params[port];

	if (!cir->status)
		return ;

	reg_val = readl(para->reg_base + CIR_CTRL);

	reg_val &= ~CIR_MODE;
	reg_val |= (mode << CIR_MODE_OFFSET);

	writel(reg_val, para->reg_base + CIR_CTRL);
}

void sunxi_cir_sample_clock_select(cir_port_t port, cir_sample_clock_t div)
{
	struct sunxi_irrx_params_t *para;
	int reg_val;

	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	para = &g_sunxi_irrx_params[port];

	if (!cir->status)
		return ;

	reg_val = readl(para->reg_base + CIR_CONFIG);

	if (div == CIR_CLK) {
		reg_val &= ~SCS;
		reg_val |= SCS2;
	} else {
		reg_val &= ~SCS2;
		reg_val |= div;
	}

	writel(reg_val, para->reg_base + CIR_CONFIG);
}

void sunxi_cir_sample_noise_threshold(cir_port_t port, int8_t threshold)
{
	int reg_val = 0;
	struct sunxi_irrx_params_t *para;

	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	para = &g_sunxi_irrx_params[port];

	if (!cir->status || threshold > 0x3f)
		return ;

	reg_val = readl(para->reg_base + CIR_CONFIG);

	reg_val &= ~NTHR;
	reg_val |= (threshold << NTHR_OFFSET);

	writel(reg_val, para->reg_base + CIR_CONFIG);
}

void sunxi_cir_sample_idle_threshold(cir_port_t port, int8_t threshold)
{
	int reg_val = 0;
	struct sunxi_irrx_params_t *para;

	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	para = &g_sunxi_irrx_params[port];

	if (!cir->status || threshold > 0x3f)
		return ;

	reg_val = readl(para->reg_base + CIR_CONFIG);

	reg_val &= ~ITHR;
	reg_val |= (threshold << ITHR_OFFSET);

	writel(reg_val, para->reg_base + CIR_CONFIG);
}

void sunxi_cir_sample_active_threshold(cir_port_t port, int8_t threshold)
{
	int reg_val = 0;
	struct sunxi_irrx_params_t *para;

	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	para = &g_sunxi_irrx_params[port];

	if (!cir->status || threshold > 0x3f)
		return ;

	reg_val = readl(para->reg_base + CIR_CONFIG);

	reg_val &= ~ATHR;
	reg_val |= (threshold << ATHR_OFFSET);

	writel(reg_val, para->reg_base + CIR_CONFIG);
}

void sunxi_cir_fifo_level(cir_port_t port, int8_t size)
{
	int reg_val = 0;
	struct sunxi_irrx_params_t *para;

	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	para = &g_sunxi_irrx_params[port];

	if (!cir->status || size > 0x3f + 1)
		return ;

	reg_val = readl(para->reg_base + CIR_RXINT);

	reg_val &= ~RAL;
	reg_val |= ((size -1) << RAL_OFFSET);

	writel(reg_val, para->reg_base + CIR_RXINT);
}

void sunxi_cir_irq_enable(cir_port_t port, int enable)
{
	int reg_val = 0;
	struct sunxi_irrx_params_t *para;

	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	para = &g_sunxi_irrx_params[port];

	if (!cir->status)
		return ;

	reg_val = readl(para->reg_base + CIR_RXINT);

	reg_val &= ~IRQ_MASK;
	enable &= IRQ_MASK;
	reg_val |= enable;

	writel(reg_val, para->reg_base + CIR_RXINT);
}

void sunxi_cir_irq_disable(cir_port_t port)
{
	int reg_val = 0;
	struct sunxi_irrx_params_t *para;

	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	para = &g_sunxi_irrx_params[port];

	if (!cir->status)
		return;

	reg_val = readl(para->reg_base + CIR_RXINT);

	reg_val &= ~IRQ_MASK;

	writel(reg_val, para->reg_base + CIR_RXINT);
}

void sunxi_cir_signal_invert(cir_port_t port, uint8_t invert)
{
	int reg_val = 0;
	struct sunxi_irrx_params_t *para;

	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	para = &g_sunxi_irrx_params[port];

	if (!cir->status)
		return;

	reg_val = readl(para->reg_base + CIR_RXCTRL);

	if (invert)
		reg_val |= RPPI;
	else
		reg_val &= ~RPPI;

	writel(reg_val, para->reg_base + CIR_RXCTRL);
}

void sunxi_cir_module_enable(cir_port_t port, int8_t enable)
{
	int reg_val = 0;
	struct sunxi_irrx_params_t *para;

	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	para = &g_sunxi_irrx_params[port];

	if (!cir->status)
		return ;

	reg_val = readl(para->reg_base + CIR_CTRL);

	if (enable)
		reg_val |= (GEN | RXEN);
	else
		reg_val &= ~(GEN | RXEN);

	writel(reg_val, para->reg_base + CIR_CTRL);
}

static int sunxi_cir_sys_gpio_init(sunxi_cir_t *cir)
{
#if defined(CONFIG_DRIVER_SYSCONFIG) && defined(IRRX_USE_SYSCONFIG)
	user_gpio_set_t irpin = {0};
	cir_gpio_t pin_cir;

	hal_cfg_get_keyvalue("cir", "cir_pin", (int32_t *)&irpin, (sizeof(user_gpio_set_t) + 3) / sizeof(int));

	pin_cir.gpio = (irpin.port - 1) * PINS_PER_BANK + irpin.port_num;

	pin_cir.enable_mux = irpin.mul_sel;
	pin_cir.disable_mux = 0;

	return hal_gpio_pinmux_set_function(pin_cir.gpio, pin_cir.enable_mux);
#else
	CIR_INFO("not support sys_config format \n");
	return -1;
#endif
}

static int sunxi_cir_gpio_exit(struct sunxi_irrx_params_t *para)
{
	return hal_gpio_pinmux_set_function(para->gpio_name, para->disable_mux);
}

static int sunxi_cir_clk_init(sunxi_cir_t *cir)
{
	struct sunxi_irrx_params_t *para;
	int ret;

	para = &g_sunxi_irrx_params[cir->port];

	hal_reset_type_t cir_rx_reset_type = para->reset_type;
	hal_reset_id_t cir_rx_reset_id = para->reset_id;

	cir->cir_reset = hal_reset_control_get(cir_rx_reset_type, cir_rx_reset_id);
	if (hal_reset_control_deassert(cir->cir_reset)) {
		CIR_ERR("cir reset deassert failed\n");
		goto err1;
	}

	hal_clk_type_t	clk_type = para->clk_type;
	hal_clk_type_t  pclk_type = para->pclk_type;

	cir->b_clk_id = para->bus_clk_id;
	cir->m_clk_id = para->mclk_id;
	cir->p_clk_id = para->pclk_id;

	cir->bclk = hal_clock_get(clk_type, cir->b_clk_id);
	if (hal_clock_enable(cir->bclk)) {
		CIR_ERR("cir bclk enabled failed\n");
		goto err2;
	}

	cir->mclk = hal_clock_get(clk_type, cir->m_clk_id);
	if (hal_clock_enable(cir->mclk)) {
		CIR_ERR("cir mclk enabled failed\n");
		goto err3;
	}

	cir->pclk = hal_clock_get(pclk_type, cir->p_clk_id);
	if (hal_clock_enable(cir->pclk)) {
		CIR_ERR("cir pclk enabled failed\n");
		goto err4;
	}

	ret = hal_clk_set_parent(cir->mclk, cir->pclk);
	if (ret) {
		CIR_ERR("cir set parent failed\n");
		goto err5;
	}

	return 0;

err5:
	hal_clock_disable(cir->pclk);
err4:
	hal_clock_put(cir->pclk);
	hal_clock_disable(cir->mclk);
err3:
	hal_clock_put(cir->mclk);
	hal_clock_disable(cir->bclk);
err2:
	hal_clock_put(cir->bclk);
	hal_reset_control_assert(cir->cir_reset);
err1:
	hal_reset_control_put(cir->cir_reset);
	return -1;
}

static int sunxi_cir_clk_exit(sunxi_cir_t *cir)
{
	hal_clock_disable(cir->pclk);
	hal_clock_put(cir->pclk);

	hal_clock_disable(cir->mclk);
	hal_clock_put(cir->mclk);

	hal_clock_disable(cir->bclk);
	hal_clock_put(cir->bclk);


	hal_reset_control_assert(cir->cir_reset);
	hal_reset_control_put(cir->cir_reset);

	return 0;
}

static cir_status_t sunxi_cir_hw_init(sunxi_cir_t *cir)
{
	cir_status_t ret;
	struct sunxi_irrx_params_t *para;

	para = &g_sunxi_irrx_params[cir->port];
	if (sunxi_cir_clk_init(cir))
		return CIR_CLK_ERR;

	if (sunxi_cir_sys_gpio_init(cir)) {
		printf("para->enable_mux=%d\n", para->enable_mux);
		ret = hal_gpio_pinmux_set_function(para->gpio_name, para->enable_mux);
		if (ret) {
			CIR_ERR("pinctrl init error\n");
			return CIR_PIN_ERR;
		}
	}

	sunxi_cir_mode_enable(para->port, true);

	/* clear reg */
	writel(0x0, para->reg_base + CIR_CONFIG);
	sunxi_cir_sample_clock_select(para->port, CIR_CLK_DIV256);
	sunxi_cir_sample_idle_threshold(para->port, RXIDLE_VAL);
	sunxi_cir_sample_active_threshold(para->port, ACTIVE_T_SAMPLE);
	sunxi_cir_sample_noise_threshold(para->port, CIR_NOISE_THR_NEC);
	sunxi_cir_signal_invert(para->port, true);

	/* clear irq */
	writel(0xef, para->reg_base + CIR_RXSTA);
	sunxi_cir_irq_enable(para->port, true);
	sunxi_cir_fifo_level(para->port, 20);
	sunxi_cir_mode_config(para->port, CIR_BOTH_PULSE);
	sunxi_cir_module_enable(para->port, true);

	if (hal_request_irq(para->irq_num, sunxi_cir_handler, "cir", cir) < 0) {
		CIR_ERR("cir request irq err\n");
		return CIR_IRQ_ERR;
	}

	hal_enable_irq(para->irq_num);

	return CIR_OK;
}

static void sunxi_cir_hw_exit(sunxi_cir_t *cir)
{
	struct sunxi_irrx_params_t *para;

	para = &g_sunxi_irrx_params[cir->port];

	hal_disable_irq(para->irq_num);
	hal_free_irq(para->irq_num);
	sunxi_cir_gpio_exit(para);
	sunxi_cir_clk_exit(cir);
}

cir_status_t sunxi_cir_init(cir_port_t port)
{
	struct sunxi_irrx_params_t *para;

	para = &g_sunxi_irrx_params[port];
	sunxi_cir_t *cir = &g_sunxi_irrx[port];
	cir_status_t ret = 0;

	if (port >= CIR_MAX_NUM) {
		CIR_ERR("[irrx%d] invalid port\n", port);
		return CIR_PORT_ERR;
	}

	cir->port = para->port;
	cir->base = para->reg_base;
	cir->irq = para->irq_num;
	cir->status = 1;

	ret = sunxi_cir_hw_init(cir);
	if (ret) {
		CIR_ERR("cir[%d] hardware init error, ret:%d\n", port, ret);
		return ret;
	}

	return ret;
}

void sunxi_cir_deinit(cir_port_t port)
{
	sunxi_cir_t *cir = &g_sunxi_irrx[port];

	sunxi_cir_hw_exit(cir);
	cir->status = 0;
}
