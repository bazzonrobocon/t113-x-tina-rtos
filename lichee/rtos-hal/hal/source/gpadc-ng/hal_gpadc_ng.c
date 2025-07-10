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
#include <hal_interrupt.h>
#include <sunxi_hal_gpadc_ng.h>
#include <hal_timer.h>
#define CONFIG_DRIVERS_GPADC_DEBUG

hal_gpadc_t hal_gpadc[GPADC_PORT];

static hal_gpadc_status_t hal_gpadc_clk_init(hal_gpadc_t *gpadc)
{
	hal_clk_type_t clk_type = HAL_SUNXI_CCU;
	hal_clk_id_t gpadc_clk_id = gpadc->bus_clk;
	hal_clk_id_t gpadc_mod_clk_id = gpadc->mod_clk;

	hal_reset_type_t reset_type = HAL_SUNXI_RESET;
	hal_reset_id_t gpadc_reset_id = gpadc->rst_clk;

	gpadc->reset = hal_reset_control_get(reset_type, gpadc_reset_id);
	if (hal_reset_control_reset(gpadc->reset))
	{
		GPADC_ERR("gpadc reset deassert failed!\n");
		return GPADC_ERROR;
	}

	gpadc->mbus_clk = hal_clock_get(clk_type, gpadc_clk_id);
	if (hal_clock_enable(gpadc->mbus_clk))
	{
		GPADC_ERR("gpadc clk enable failed!\n");
		goto err0;
	}

	gpadc->clk = hal_clock_get(clk_type, gpadc_mod_clk_id);
	if (hal_clock_enable(gpadc->clk))
	{
		GPADC_ERR("gpadc clk enable failed!\n");
		goto err1;
	}

	return GPADC_OK;
err1:
	hal_clock_disable(gpadc->mbus_clk);
err0:
	hal_reset_control_assert(gpadc->reset);
	return GPADC_ERROR;
}

static void hal_gpadc_clk_exit(hal_gpadc_t *gpadc)
{
	hal_clock_disable(gpadc->clk);
	hal_clock_disable(gpadc->mbus_clk);
	hal_reset_control_assert(gpadc->reset);
}

static int gpadc_channel_check_valid(int port, hal_gpadc_channel_t channal)
{
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	return channal < gpadc->channel_num ? 0 : -1 ;
}

static void gpadc_channel_select(int port, hal_gpadc_channel_t channal)
{
	uint32_t reg_val;
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	reg_val = readl((unsigned long)(gpadc->reg_base) + GP_CS_EN_REG);
	reg_val |= (0x01 << channal);
	writel(reg_val, (unsigned long)(gpadc->reg_base) + GP_CS_EN_REG);
}

static void gpadc_channel_deselect(int port, hal_gpadc_channel_t channal)
{
	uint32_t reg_val;
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	reg_val = readl((unsigned long)(gpadc->reg_base) + GP_CS_EN_REG);
	reg_val &= ~(0x01 << channal);
	writel(reg_val, (unsigned long)(gpadc->reg_base) + GP_CS_EN_REG);
}

static void gpadc_compare_select(int port, hal_gpadc_channel_t channal)
{
	uint32_t reg_val;
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	reg_val = readl((unsigned long)(gpadc->reg_base) + GP_CS_EN_REG);
	reg_val |= (GP_CH0_CMP_EN << channal);
	writel(reg_val, (unsigned long)(gpadc->reg_base) + GP_CS_EN_REG);
}

static void gpadc_compare_deselect(int port, hal_gpadc_channel_t channal)
{
	uint32_t reg_val;
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	reg_val = readl((unsigned long)(gpadc->reg_base) + GP_CTRL_REG);
	reg_val &= ~(GP_CH0_CMP_EN << channal);
	writel(reg_val, (unsigned long)(gpadc->reg_base) + GP_CTRL_REG);
}

static void gpadc_channel_enable_lowirq(int port, hal_gpadc_channel_t channal)
{
	uint32_t reg_val;
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	reg_val = readl((unsigned long)(gpadc->reg_base) + GP_DATAL_INTC_REG);
	reg_val |= (0x01 << channal);
	writel(reg_val, (unsigned long)(gpadc->reg_base) + GP_DATAL_INTC_REG);
}

static void gpadc_channel_disable_lowirq(int port, hal_gpadc_channel_t channal)
{
	uint32_t reg_val;
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	reg_val = readl((unsigned long)(gpadc->reg_base) + GP_DATAL_INTC_REG);
	reg_val &= ~(0x01 << channal);
	writel(reg_val, (unsigned long)(gpadc->reg_base) + GP_DATAL_INTC_REG);
}

static void gpadc_channel_compare_lowdata(int port, hal_gpadc_channel_t channal,
		uint32_t low_uv)
{
	uint32_t reg_val, low, unit;
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	/* analog voltage range 0~1.8v, 12bits sample rate, unit=1.8v/(2^12) */
	unit = VOL_RANGE / 4096; /* 12bits sample rate */
	low = low_uv / unit;
	if (low > VOL_VALUE_MASK)
	{
		low = VOL_VALUE_MASK;
	}

	reg_val = readl((unsigned long)(gpadc->reg_base) + GP_CH0_CMP_DATA_REG + 4 * channal);
	reg_val &= ~VOL_VALUE_MASK;
	reg_val |= (low & VOL_VALUE_MASK);
	writel(reg_val, (unsigned long)(gpadc->reg_base) + GP_CH0_CMP_DATA_REG + 4 * channal);
}

void gpadc_channel_enable_highirq(int port, hal_gpadc_channel_t channal)
{
	uint32_t reg_val;
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	reg_val = readl((unsigned long)(gpadc->reg_base) + GP_DATAH_INTC_REG);
	reg_val |= (1 << channal);
	writel(reg_val, (unsigned long)(gpadc->reg_base) + GP_DATAH_INTC_REG);
}

void gpadc_channel_disable_highirq(int port, hal_gpadc_channel_t channal)
{
	uint32_t reg_val;
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	reg_val = readl((unsigned long)(gpadc->reg_base) + GP_DATAH_INTC_REG);
	reg_val &= ~(1 << channal);
	writel(reg_val, (unsigned long)(gpadc->reg_base) + GP_DATAH_INTC_REG);
}

static void gpadc_channel_compare_highdata(int port, hal_gpadc_channel_t channal,
		uint32_t hig_uv)
{
	uint32_t reg_val, hig_val, unit_val;
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	/* anolog voltage range 0~1.8v, 12bits sample rate, unit=1.8v/(2^12) */
	unit_val = VOL_RANGE / 4096; /* 12bits sample rate */
	hig_val = hig_uv / unit_val;

	if (hig_val > VOL_VALUE_MASK)
	{
		hig_val = VOL_VALUE_MASK;
	}

	reg_val = readl((unsigned long)(gpadc->reg_base) + GP_CH0_CMP_DATA_REG + 4 * channal);
	reg_val &= ~(VOL_VALUE_MASK << 16);
	reg_val |= (hig_val & VOL_VALUE_MASK) << 16;
	writel(reg_val, (unsigned long)(gpadc->reg_base) + GP_CH0_CMP_DATA_REG + 4 * channal);
}

/* clk_in: source clock, round_clk: sample rate */
static void gpadc_sample_rate_set(uint32_t reg_base, uint32_t clk_in,
		uint32_t round_clk)
{
	uint32_t div, reg_val;
	if (round_clk > clk_in)
	{
		GPADC_ERR("invalid round clk!");
	}
	div = clk_in / round_clk - 1 ;
	reg_val = readl((unsigned long)(reg_base) + GP_SR_REG);
	reg_val &= ~GP_SR_CON;
	reg_val |= (div << 16);
	writel(reg_val, (unsigned long)(reg_base) + GP_SR_REG);
}

static void gpadc_calibration_enable(uint32_t reg_base)
{
	uint32_t reg_val;
	reg_val = readl((unsigned long)(reg_base) + GP_CTRL_REG);
	reg_val |= GP_CALI_EN;
	writel(reg_val, (unsigned long)(reg_base) + GP_CTRL_REG);
}

static void gpadc_mode_select(uint32_t reg_base,
		enum gp_select_mode mode)
{
	uint32_t reg_val;

	reg_val = readl((unsigned long)(reg_base) + GP_CTRL_REG);
	reg_val &= ~GP_MODE_SELECT;
	reg_val |= (mode << 18);
	writel(reg_val, (unsigned long)(reg_base) + GP_CTRL_REG);
}

/* enable gpadc function, true:enable, false:disable */
static void gpadc_enable(uint32_t reg_base)
{
	uint32_t reg_val;

	reg_val = readl((unsigned long)(reg_base) + GP_CTRL_REG);
	reg_val |= GP_ADC_EN;
	writel(reg_val, (unsigned long)(reg_base) + GP_CTRL_REG);
}

/* enable gpadc function, true:enable, false:disable */
static void gpadc_disable(uint32_t reg_base)
{
	uint32_t reg_val;

	reg_val = readl((unsigned long)(reg_base) + GP_CTRL_REG);
	reg_val &= ~GP_ADC_EN;
	writel(reg_val, (unsigned long)(reg_base) + GP_CTRL_REG);
}
// fix unused-function
__attribute__((__unused__)) static uint32_t gpadc_read_channel_irq_enable(uint32_t reg_base)
{
	return readl((unsigned long)(reg_base) + GP_DATA_INTC_REG);
}

static uint32_t gpadc_read_channel_lowirq_enable(uint32_t reg_base)
{
	return readl((unsigned long)(reg_base) + GP_DATAL_INTC_REG);
}

static uint32_t gpadc_read_channel_highirq_enable(uint32_t reg_base)
{
	return readl((unsigned long)(reg_base) + GP_DATAH_INTC_REG);
}

static uint32_t gpadc_channel_irq_status(uint32_t reg_base)
{
	return readl((unsigned long)(reg_base) + GP_DATA_INTS_REG);
}

static void gpadc_channel_clear_irq(uint32_t reg_base, uint32_t flags)
{
	writel(flags, (unsigned long)(reg_base) + GP_DATA_INTS_REG);
}

static uint32_t gpadc_channel_lowirq_status(uint32_t reg_base)
{
	return readl((unsigned long)(reg_base) + GP_DATAL_INTS_REG);
}

static void gpadc_channel_clear_lowirq(uint32_t reg_base, uint32_t flags)
{
	writel(flags, (unsigned long)(reg_base) + GP_DATAL_INTS_REG);
}

static uint32_t gpadc_channel_highirq_status(uint32_t reg_base)
{
	return readl((unsigned long)(reg_base) + GP_DATAH_INTS_REG);
}

static void gpadc_channel_clear_highirq(uint32_t reg_base, uint32_t flags)
{
	writel(flags, (unsigned long)(reg_base) + GP_DATAH_INTS_REG);
}

static int gpadc_read_data(uint32_t reg_base, hal_gpadc_channel_t channal)
{
	return readl((unsigned long)(reg_base) + GP_CH0_DATA_REG + 4 * channal) & GP_CH_DATA_MASK;
}

uint32_t gpadc_read_channel_data(int port, hal_gpadc_channel_t channel)
{
	int data;
	uint32_t vol_data;
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	data = gpadc_read_data(gpadc->reg_base, channel);
	data = ((VOL_RANGE / 4096) * data);
	vol_data = data / 1000;
	GPADC_INFO("channel %d vol data: %u\n", channel, vol_data);

	return vol_data;
}

int hal_gpadc_callback(uint32_t data_type, uint32_t data)
{
	GPADC_INFO("gpadc interrupt, data_type is %u", data_type);
	return 0;
}

static hal_irqreturn_t gpadc_handler(void *dev)
{
	hal_gpadc_t *gpadc = (hal_gpadc_t *)dev;

	uint32_t reg_val, reg_low, reg_high;
	uint32_t reg_enable_low, reg_enable_high;
	uint32_t i, data = 0;

	reg_enable_low = gpadc_read_channel_lowirq_enable(gpadc->reg_base);
	reg_enable_high = gpadc_read_channel_highirq_enable(gpadc->reg_base);

	reg_val = gpadc_channel_irq_status(gpadc->reg_base);
	gpadc_channel_clear_irq(gpadc->reg_base, reg_val);
	reg_low = gpadc_channel_lowirq_status(gpadc->reg_base);
	gpadc_channel_clear_lowirq(gpadc->reg_base, reg_val);
	reg_high = gpadc_channel_highirq_status(gpadc->reg_base);
	gpadc_channel_clear_highirq(gpadc->reg_base, reg_val);

	for (i = 0; i < gpadc->channel_num; i++)
	{
		if (reg_low & (1 << i) & reg_enable_low)
		{
			data = gpadc_read_data(gpadc->reg_base, i);
			//gpadc_channel_enable_highirq(i);

			if (gpadc->callback[i])
			{
				gpadc->callback[i](GPADC_DOWN, data);
			}
		}

		if (reg_high & (1 << i) & reg_enable_high)
		{
			//gpadc_channel_disable_highirq(i);
			gpadc->callback[i](GPADC_UP, data);
		}
	}

	return 0;
}

hal_gpadc_status_t hal_gpadc_register_callback(int port, hal_gpadc_channel_t channal,
		gpadc_callback_t user_callback)
{
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	if (gpadc_channel_check_valid(port, channal))
	{
		return GPADC_CHANNEL_ERROR;
	}

	if (user_callback == NULL)
	{
		return GPADC_ERROR;
	}

	hal_mutex_lock(gpadc->lock);
	gpadc->callback[channal] = user_callback;
	hal_mutex_unlock(gpadc->lock);

	return GPADC_OK;
}

hal_gpadc_status_t hal_gpadc_channel_init(int port, hal_gpadc_channel_t channal, bool keyadc)
{
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	if (gpadc_channel_check_valid(port, channal))
	{
		return GPADC_CHANNEL_ERROR;
	}

	hal_mutex_lock(gpadc->lock);
	gpadc_channel_select(port, channal);
	hal_mutex_unlock(gpadc->lock);

	/* use gpadc capture key */
	if (keyadc) {
		hal_mutex_lock(gpadc->lock);
		gpadc_compare_select(port, channal);
		gpadc_channel_enable_lowirq(port, channal);
		hal_mutex_unlock(gpadc->lock);
		gpadc_channel_compare_lowdata(port, channal, COMPARE_LOWDATA);
		gpadc_channel_compare_highdata(port, channal, COMPARE_HIGDATA);
	}
	hal_msleep(4);
	return GPADC_OK;
}

hal_gpadc_status_t hal_gpadc_channel_exit(int port, hal_gpadc_channel_t channal, bool keyadc)
{
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	if (gpadc_channel_check_valid(port, channal))
	{
		return GPADC_CHANNEL_ERROR;
	}

	if (keyadc) {
		hal_mutex_lock(gpadc->lock);
		gpadc_channel_deselect(port, channal);
		gpadc_compare_deselect(port, channal);
		gpadc_channel_disable_lowirq(port, channal);
		hal_mutex_unlock(gpadc->lock);
	}

	return GPADC_OK;
}

static int hal_gpadc_setup(hal_gpadc_t *gpadc)
{
	uint8_t i;

	switch (gpadc->port)
	{
		case 0:
			gpadc->reg_base = GPADC_BASE0;
			gpadc->mod_clk = SUNXI_CLK_GPADC0;
			gpadc->bus_clk = SUNXI_CLK_BUS_GPADC(0);
			gpadc->rst_clk = SUNXI_CLK_RST_GPADC(0);
			gpadc->irq_num = SUNXI_GPADC_IRQ0;
			break;
		case 1:
			gpadc->reg_base = GPADC_BASE1;
			gpadc->mod_clk = SUNXI_CLK_GPADC1;
			gpadc->bus_clk = SUNXI_CLK_BUS_GPADC(1);
			gpadc->rst_clk = SUNXI_CLK_RST_GPADC(1);
			gpadc->irq_num = SUNXI_GPADC_IRQ1;
			break;
		default:
			GPADC_ERR("gpadc%d is invalid\n", gpadc->port);
			return GPADC_ERROR;
	}

	gpadc->channel_num = CHANNEL_NUM;
	gpadc->sample_rate = DEFAULT_SR;
	gpadc->mode = GP_CONTINUOUS_MODE;

	for (i = 0; i < gpadc->channel_num; i++)
	{
		gpadc->callback[i] = hal_gpadc_callback;
	}

	if (hal_request_irq(gpadc->irq_num, gpadc_handler, "gpadc", gpadc) < 0)
	{
		return GPADC_IRQ_ERROR;
	}

	return GPADC_OK;
};

void hal_gpadc_fifo_config(int port)
{
	uint32_t reg_val;
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	/* set the working mode of gpadc to GP_BURST_MODE */
	gpadc_mode_select(gpadc->reg_base, GP_BURST_MODE);

	/* enable gpadc fifo drq */
	reg_val = readl((unsigned long)(gpadc->reg_base) + GP_FIFO_INTC_REG);
	reg_val |= (0x01 << 18);
	writel(reg_val, (unsigned long)(gpadc->reg_base) + GP_FIFO_INTC_REG);
}

static int hal_gpadc_hw_init(hal_gpadc_t *gpadc)
{
	if (hal_gpadc_clk_init(gpadc))
	{
		GPADC_ERR("gpadc init clk error");
		return GPADC_CLK_ERROR;
	}

	pr_err("gpadc set sample rate");

	gpadc_sample_rate_set(gpadc->reg_base, OSC_24MHZ, gpadc->sample_rate);
	pr_err("gpadc enable calibration");
	gpadc_calibration_enable(gpadc->reg_base);
	gpadc_mode_select(gpadc->reg_base, gpadc->mode);

	gpadc_enable(gpadc->reg_base);

	hal_enable_irq(gpadc->irq_num);

	return GPADC_OK;
}

static void hal_gpadc_hw_exit(hal_gpadc_t *gpadc)
{
	hal_disable_irq(gpadc->irq_num);
	gpadc_disable(gpadc->reg_base);
	hal_gpadc_clk_exit(gpadc);
}

int hal_gpadc_init(int port)
{
	hal_gpadc_t *gpadc = &hal_gpadc[port];
	int err;

	if (gpadc->already_init) {
		gpadc->already_init++;
		GPADC_INFO("gpadc has been inited, return ok\n");
		return GPADC_OK;
	}

	gpadc->port = port;
	err = hal_gpadc_setup(gpadc);
	if (err)
	{
		GPADC_ERR("gpadc setup failed\n");
		return GPADC_ERROR;
	}

	err = hal_gpadc_hw_init(gpadc);
	if (err)
	{
		GPADC_ERR("gpadc init hw failed\n");
		return GPADC_ERROR;
	}

	gpadc->lock = hal_mutex_create();
	if (!gpadc->lock) {
		GPADC_ERR("mutex create failed\n");
		goto err;
	}

	gpadc->already_init++;
	pr_err("gpadc init success\n");

	return GPADC_OK;
err:
	hal_gpadc_hw_exit(gpadc);

	GPADC_ERR("gpadc init failed\n");
	return GPADC_ERROR;

}

hal_gpadc_status_t hal_gpadc_deinit(int port)
{
	hal_gpadc_t *gpadc = &hal_gpadc[port];

	if (gpadc->already_init > 0) {
		if (--gpadc->already_init == 0) {
			hal_mutex_delete(gpadc->lock);
			gpadc->lock = NULL;
			hal_gpadc_hw_exit(gpadc);
		}
	}

	return GPADC_OK;
}
