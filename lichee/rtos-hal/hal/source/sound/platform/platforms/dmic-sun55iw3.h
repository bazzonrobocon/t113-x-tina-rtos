/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
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
#ifndef	__SUN55IW3_DMIC_H_
#define	__SUN55IW3_DMIC_H_

#define	SUNXI_DMIC_MEMBASE (0x07111000)

/*------------------------ PIN CONFIG FOR NORMAL ---------------------------*/
dmic_gpio_t g_dmic_gpio = {
	.clk	= {GPIOF(9), 2},
	.din0	= {GPIOF(28), 5},
	.din1	= {GPIOG(30), 5},
	.din2	= {GPIOG(28), 5},
	.din3	= {GPIOG(26), 5},
};

struct sunxi_dmic_clk {
	/* dependent clk */
	hal_clk_t clk_peri0_2x;
	hal_clk_t clk_dsp_src;
	/* parent clk */
	hal_clk_t clk_pll_audio0_4x;
	hal_clk_t clk_pll_audio1_div2;
	hal_clk_t clk_pll_audio1_div5;
	/* module clk */
	hal_clk_t clk_dmic;

	hal_clk_t clk_bus;
	struct reset_control *clk_rst;
};

static inline int snd_sunxi_dmic_clk_enable(struct sunxi_dmic_clk *clk)
{
	snd_print("\n");

	if (hal_reset_control_deassert(clk->clk_rst)) {
		snd_err("clk_rst deassert failed\n");
		goto err_deassert_rst;
	}

	if (hal_clock_enable(clk->clk_peri0_2x)) {
		snd_err("clk_peri0_2x enable failed\n");
		goto err_enable_clk_peri0_2x;
	}
	if (hal_clock_enable(clk->clk_dsp_src)) {
		snd_err("clk_dsp_src enable failed\n");
		goto err_enable_clk_dsp_src;
	}

	if (hal_clock_enable(clk->clk_bus)) {
		snd_err("clk_bus enable failed\n");
		goto err_enable_clk_bus;
	}

	if (hal_clock_enable(clk->clk_pll_audio0_4x)) {
		snd_err("clk_pll_audio0_4x enable failed\n");
		goto err_enable_clk_pll_audio0_4x;
	}
	if (hal_clock_enable(clk->clk_pll_audio1_div2)) {
		snd_err("clk_pll_audio1_div2 enable failed\n");
		goto err_enable_clk_pll_audio1_div2;
	}
	if (hal_clock_enable(clk->clk_pll_audio1_div5)) {
		snd_err("clk_pll_audio1_div5 enable failed\n");
		goto err_enable_clk_pll_audio1_div5;
	}

	if (hal_clock_enable(clk->clk_dmic)) {
		snd_err("clk_dmic enable failed\n");
		goto err_enable_clk_dmic;
	}

	return HAL_CLK_STATUS_OK;

err_enable_clk_dmic:
	hal_clock_disable(clk->clk_pll_audio1_div5);
err_enable_clk_pll_audio1_div5:
	hal_clock_disable(clk->clk_pll_audio1_div2);
err_enable_clk_pll_audio1_div2:
	hal_clock_disable(clk->clk_pll_audio0_4x);
err_enable_clk_pll_audio0_4x:
	hal_clock_disable(clk->clk_bus);
err_enable_clk_bus:
	hal_clock_disable(clk->clk_dsp_src);
err_enable_clk_dsp_src:
	hal_clock_disable(clk->clk_peri0_2x);
err_enable_clk_peri0_2x:
	hal_reset_control_assert(clk->clk_rst);
err_deassert_rst:
	return HAL_CLK_STATUS_ERROR;
}

static inline void snd_sunxi_dmic_clk_disable(struct sunxi_dmic_clk *clk)
{
	snd_print("\n");

	hal_clock_disable(clk->clk_dmic);
	hal_clock_disable(clk->clk_pll_audio1_div5);
	hal_clock_disable(clk->clk_pll_audio1_div2);
	hal_clock_disable(clk->clk_pll_audio0_4x);
	hal_clock_disable(clk->clk_bus);
	hal_clock_disable(clk->clk_dsp_src);
	hal_clock_disable(clk->clk_peri0_2x);
	hal_reset_control_assert(clk->clk_rst);

	return;
}

static inline int snd_sunxi_dmic_clk_init(struct sunxi_dmic_clk *clk)
{
	int ret = 0;

	snd_print("\n");

	/* get rst clk */
	clk->clk_rst = hal_reset_control_get(HAL_SUNXI_MCU_RESET, RST_BUS_MCU_DMIC);
	if (!clk->clk_rst) {
		snd_err("dmic clk rst get failed\n");
		goto err_get_clk_rst;
	}

	/* get dependent clock */
	clk->clk_peri0_2x = hal_clock_get(HAL_SUNXI_CCU, CLK_PLL_PERI0_2X);
	if (!clk->clk_peri0_2x) {
		snd_err("clk_peri0_2x get failed\n");
		goto err_get_clk_peri0_2x;
	}

	clk->clk_dsp_src = hal_clock_get(HAL_SUNXI_CCU, CLK_DSP);
	if (!clk->clk_dsp_src) {
		snd_err("clk_dsp_src get failed\n");
		goto err_get_clk_dsp_src;
	}

	/* get bus clk */
	clk->clk_bus = hal_clock_get(HAL_SUNXI_MCU, CLK_BUS_MCU_DMIC);
	if (!clk->clk_bus) {
		snd_err("clk_bus get failed\n");
		goto err_get_clk_bus;
	}

	/* get parent clk */
	clk->clk_pll_audio0_4x = hal_clock_get(HAL_SUNXI_CCU, CLK_PLL_AUDIO0_4X);
	if (!clk->clk_pll_audio0_4x) {
		snd_err("clk_pll_audio0_4x get failed\n");
		goto err_get_clk_pll_audio0_4x;
	}
	clk->clk_pll_audio1_div2 = hal_clock_get(HAL_SUNXI_MCU, CLK_PLL_MCU_AUDIO1_DIV2);
	if (!clk->clk_pll_audio1_div2) {
		snd_err("clk_pll_audio1_div2 get failed\n");
		goto err_get_clk_pll_audio1_div2;
	}
	clk->clk_pll_audio1_div5 = hal_clock_get(HAL_SUNXI_MCU, CLK_PLL_MCU_AUDIO1_DIV5);
	if (!clk->clk_pll_audio1_div5) {
		snd_err("clk_pll_audio1_div5 get failed\n");
		goto err_get_clk_pll_audio1_div5;
	}

	/* get dmic clk */
	clk->clk_dmic = hal_clock_get(HAL_SUNXI_MCU, CLK_MCU_DMIC);
	if (!clk->clk_dmic) {
		snd_err("clk_dmic get failed\n");
		goto err_get_clk_dmic;
	}

	ret = snd_sunxi_dmic_clk_enable(clk);
	if (ret) {
		snd_err("clk enable failed\n");
		ret = -EINVAL;
		goto err_clk_enable;
	}

	return HAL_CLK_STATUS_OK;

err_clk_enable:
	hal_clock_put(clk->clk_dmic);
err_get_clk_dmic:
	hal_clock_put(clk->clk_pll_audio1_div5);
err_get_clk_pll_audio1_div5:
	hal_clock_put(clk->clk_pll_audio1_div2);
err_get_clk_pll_audio1_div2:
	hal_clock_put(clk->clk_pll_audio0_4x);
err_get_clk_pll_audio0_4x:
	hal_clock_put(clk->clk_bus);
err_get_clk_bus:
	hal_clock_put(clk->clk_dsp_src);
err_get_clk_dsp_src:
	hal_clock_put(clk->clk_peri0_2x);
err_get_clk_peri0_2x:
err_get_clk_rst:
	return HAL_CLK_STATUS_ERROR;
}

static inline void snd_sunxi_dmic_clk_exit(struct sunxi_dmic_clk *clk)
{
	snd_print("\n");

	snd_sunxi_dmic_clk_disable(clk);
	hal_clock_put(clk->clk_dmic);
	hal_clock_put(clk->clk_pll_audio1_div5);
	hal_clock_put(clk->clk_pll_audio1_div2);
	hal_clock_put(clk->clk_pll_audio0_4x);
	hal_clock_put(clk->clk_bus);
	hal_clock_put(clk->clk_dsp_src);
	hal_clock_put(clk->clk_peri0_2x);
	hal_reset_control_put(clk->clk_rst);

	return;
}

static inline int snd_sunxi_dmic_clk_set_rate(struct sunxi_dmic_clk *clk, int stream,
					      unsigned int freq_in, unsigned int freq_out)
{
	/* set dependent clk */
	if (hal_clk_set_parent(clk->clk_dsp_src, clk->clk_peri0_2x)) {
		snd_err("set clk_dsp_src parent clk failed\n");
		return -EINVAL;
	}

	if (hal_clk_set_rate(clk->clk_dsp_src, 600000000)) {
		snd_err("set clk_dsp_src rate failed, rate\n");
		return -EINVAL;
	}

	if (freq_in % 24576000 == 0) {
		/* If you want to use clk_pll_audio0_4x, must set it 1083801600Hz */
		if (hal_clk_set_parent(clk->clk_dmic, clk->clk_pll_audio0_4x)) {
			snd_err("set dmic parent clk failed\n");
			return -EINVAL;
		}
		if (hal_clk_set_rate(clk->clk_pll_audio0_4x, 1083801600)) {
			snd_err("set clk_pll_audio0_4x rate failed\n");
			return -EINVAL;
		}
	} else {
		if (hal_clk_set_parent(clk->clk_dmic, clk->clk_pll_audio1_div5)) {
			snd_err("set dmic parent clk failed\n");
			return -EINVAL;
		}
	}

	if (hal_clk_set_rate(clk->clk_dmic, freq_out)) {
		snd_err("set clk_dmic rate failed, rate: %u\n", freq_out);
		return -EINVAL;
	}

	return HAL_CLK_STATUS_OK;
}

#endif /* __SUN55IW3_DMIC_H_ */
