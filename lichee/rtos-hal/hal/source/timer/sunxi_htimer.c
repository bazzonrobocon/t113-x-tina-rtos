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

#include "sunxi_htimer.h"
#include "aw_common.h"
#include <stdlib.h>
#include <hal_interrupt.h>
#include <hal_mem.h>

#ifdef CONFIG_HTIMER_CLK_200MHZ
#define HTIMER_CLK_DIV		(0)
#define HTIMER_CLK_BASE_VALUE	(200)
#define HTIMER_CLK_FREQ		(200000000)
#elif CONFIG_HTIMER_CLK_100MHZ
#define HTIMER_CLK_DIV		(1)
#define HTIMER_CLK_BASE_VALUE	(100)
#define HTIMER_CLK_FREQ		(100000000)
#elif CONFIG_HTIMER_CLK_50MHZ
#define HTIMER_CLK_DIV		(2)
#define HTIMER_CLK_BASE_VALUE	(50)
#define HTIMER_CLK_FREQ		(50000000)
#elif CONFIG_HTIMER_CLK_25MHZ
#define HTIMER_CLK_DIV		(4)
#define HTIMER_CLK_BASE_VALUE	(25)
#define HTIMER_CLK_FREQ		(25000000)
#endif

struct sunxi_htimer g_htimer[HAL_HRTIMER_NUM];

const struct sunxi_timer_params g_sunxi_timer_params[] = {
#ifdef TIMER0_PARAMS
    TIMER0_PARAMS,
#endif
#ifdef TIMER1_PARAMS
    TIMER1_PARAMS,
#endif
#ifdef TIMER2_PARAMS
    TIMER2_PARAMS,
#endif
#ifdef TIMER3_PARAMS
    TIMER3_PARAMS,
#endif
#ifdef TIMER4_PARAMS
    TIMER4_PARAMS,
#endif
#ifdef TIMER5_PARAMS
    TIMER5_PARAMS,
#endif
#ifdef TIMER6_PARAMS
    TIMER6_PARAMS,
#endif
#ifdef TIMER7_PARAMS
    TIMER7_PARAMS,
#endif
#ifdef TIMER8_PARAMS
    TIMER8_PARAMS,
#endif
#ifdef TIMER9_PARAMS
    TIMER9_PARAMS,
#endif
#ifdef TIMER10_PARAMS
    TIMER10_PARAMS,
#endif
#ifdef TIMER11_PARAMS
    TIMER11_PARAMS,
#endif
#ifdef TIMER12_PARAMS
    TIMER12_PARAMS,
#endif
#ifdef TIMER13_PARAMS
    TIMER13_PARAMS,
#endif
#ifdef TIMER14_PARAMS
    TIMER14_PARAMS,
#endif
#ifdef TIMER15_PARAMS
    TIMER15_PARAMS,
#endif
#ifdef TIMER16_PARAMS
    TIMER16_PARAMS,
#endif
#ifdef TIMER17_PARAMS
    TIMER17_PARAMS,
#endif
#ifdef TIMER18_PARAMS
    TIMER18_PARAMS,
#endif
#ifdef TIMER19_PARAMS
    TIMER19_PARAMS,
#endif
#ifdef TIMER20_PARAMS
    TIMER20_PARAMS,
#endif
#ifdef TIMER21_PARAMS
    TIMER21_PARAMS,
#endif
#ifdef TIMER22_PARAMS
    TIMER22_PARAMS,
#endif
#ifdef TIMER23_PARAMS
    TIMER23_PARAMS,
#endif
#ifdef TIMER24_PARAMS
    TIMER24_PARAMS,
#endif
#ifdef TIMER25_PARAMS
    TIMER25_PARAMS,
#endif
#ifdef TIMER26_PARAMS
    TIMER26_PARAMS,
#endif
#ifdef TIMER27_PARAMS
    TIMER27_PARAMS,
#endif

};

int sunxi_htimer_freq(void)
{
	return HTIMER_CLK_FREQ;
}

static hal_irqreturn_t sunxi_htimer_irq_handle(void *data)
{
	struct sunxi_htimer *timer = (struct sunxi_htimer *)data;
	unsigned int val = 0;

	val = PENDING_BIT(timer->timer_id);
	val &= readl(HTIMER_IRQ_ST_REG(timer->timer_id));
	if (val != PENDING_BIT(timer->timer_id)) {
		return 1;
	}

	/* clear pending */
	writel((PENDING_BIT(timer->timer_id)), HTIMER_IRQ_ST_REG(timer->timer_id));

	/*callback*/
	if (timer->callback != NULL)
	{
		timer->callback(timer->param);
	}

	return HAL_IRQ_OK;
}

uint32_t sunxi_htimer_read_cntlow(uint32_t timer)
{
	uint32_t value;

	value = readl(HTIMER_CNTVAL_LO_REG(timer));

	return value;
}

uint32_t sunxi_htimer_read_reglow(uint32_t timer)
{
	return ((uint32_t)HTIMER_CNTVAL_LO_REG(timer));
}

static void sunxi_htimer_sync(uint32_t timer)
{
	uint32_t old = readl(HTIMER_CNTVAL_LO_REG(timer));

	while ((old - readl(HTIMER_CNTVAL_LO_REG(timer))) < HTIMER_SYNC_TICKS)
	{
		int i = 10;
		while (i--);
		break;
	}
}

void sunxi_htimer_stop(uint32_t timer)
{
	uint32_t val = readl(HTIMER_CTL_REG(timer));

	writel(val & ~HTIMER_CTL_ENABLE, HTIMER_CTL_REG(timer));
	sunxi_htimer_sync(timer);
}

void sunxi_htimer_start(uint32_t timer, bool periodic)
{
	uint32_t val = readl(HTIMER_CTL_REG(timer));

	if (periodic)
	{
		val &= ~HTIMER_CTL_ONESHOT;
	}
	else
	{
		val |= HTIMER_CTL_ONESHOT;
	}

	val |= HTIMER_CTL_CLK_PRES(HTIMER_CLK_DIV);
	writel(val | HTIMER_CTL_ENABLE | HTIMER_CTL_RELOAD, HTIMER_CTL_REG(timer));
}

static void sunxi_htimer_setup(uint32_t tick, uint32_t timer)
{
	writel(tick, HTIMER_INTVAL_LO_REG(timer));
}

int sunxi_htimer_set_oneshot(uint32_t delay_us, uint32_t timer, timer_callback callback, void *callback_param)
{
	uint32_t tick = delay_us * HTIMER_CLK_BASE_VALUE;

	if (tick < g_htimer[timer].min_delta_ticks || tick > g_htimer[timer].max_delta_ticks)
	{
		HTIMER_INFO("not support!\n");
		return -1;
	}

	if (callback != NULL)
	{
		hal_mutex_lock(g_htimer[timer].lock);
		g_htimer[timer].callback = callback;
		g_htimer[timer].param = callback_param;
		hal_mutex_unlock(g_htimer[timer].lock);
	} else {
		HTIMER_INFO("no callback for timer%d!\n", timer);
	}

	sunxi_htimer_stop(timer);

	sunxi_htimer_setup(tick, timer);

	sunxi_htimer_start(timer, false);

	return 0;
}

int sunxi_htimer_set_periodic(uint32_t delay_us, uint32_t timer, timer_callback callback, void *callback_param)
{
	uint32_t tick = delay_us * HTIMER_CLK_BASE_VALUE;

	if (tick < g_htimer[timer].min_delta_ticks || tick > g_htimer[timer].max_delta_ticks)
	{
		HTIMER_INFO("not support!\n");
		return -1;
	}

	if (callback != NULL)
	{
		hal_mutex_lock(g_htimer[timer].lock);
		g_htimer[timer].callback = callback;
		g_htimer[timer].param = callback_param;
		hal_mutex_unlock(g_htimer[timer].lock);
	}

	sunxi_htimer_stop(timer);

	sunxi_htimer_setup(tick, timer);

	sunxi_htimer_start(timer, true);

	return 0;

}

int sunxi_htimer_clk_deinit(int id)
{
	int ret = 0;
	const struct sunxi_timer_params *para;

	para = &g_sunxi_timer_params[id];

	/* disable timer clk */
	struct reset_control *reset;
	reset = hal_reset_control_get(para->reset_type, para->reset_id);
	if (!reset) {
		HTIMER_ERR("fail to get hstimer rst clk!\n");
		return -1;
	}
	ret = hal_reset_control_assert(reset);
	if (ret) {
		HTIMER_ERR("fail to assert hstimer rst clk!\n");
		return ret;
	}

	hal_clk_t bus_clk;
	bus_clk = hal_clock_get(para->bus_type, para->bus_id);
	if (!bus_clk) {
		HTIMER_ERR("fail to get hstimer bus gate clk!\n");
		return -1;
	}

	ret = hal_clock_disable(bus_clk);
	if (ret) {
		HTIMER_ERR("fail to disable hstimer bus gate clk!\n");
		return ret;
	}

	hal_clk_t clk;
	clk = hal_clock_get(para->clk_type, para->clk_id);
	if (!clk) {
		HTIMER_ERR("fail to get hstimer %d gate clk!\n", id);
		return -1;
	}

	ret = hal_clock_disable(clk);
	if (ret) {
		HTIMER_ERR("fail to disable hstimer %d gate clk!\n", id);
		return ret;
	}

	return ret;
}

int sunxi_htimer_clk_init(int id)
{
	int ret = 0;
	const struct sunxi_timer_params *para;

	para = &g_sunxi_timer_params[id];

	struct reset_control *reset;
	reset = hal_reset_control_get(para->reset_type, para->reset_id);
	if (!reset) {
		HTIMER_ERR("fail to get hstimer rst clk!\n");
		return -1;
	}

	ret = hal_reset_control_deassert(reset);
	if (ret) {
		HTIMER_ERR("fail to deassrt hstimer rst clk!\n");
		return ret;
	}

	hal_clk_t bus_clk;
	bus_clk = hal_clock_get(para->bus_type, para->bus_id);
	if (!bus_clk) {
		HTIMER_ERR("fail to get hstimer bus gate clk!\n");
		return -1;
	}

	ret = hal_clock_enable(bus_clk);
	if (ret) {
		HTIMER_ERR("fail to enable hstimer bus gate clk!\n");
		return ret;
	}

	hal_clk_t clk;
	clk = hal_clock_get(para->clk_type, para->clk_id);
	if (!clk) {
		HTIMER_ERR("fail to get hstimer %d gate clk!\n", id);
		return -1;
	}

	ret = hal_clock_enable(clk);
	if (ret) {
		HTIMER_ERR("fail to enable hstimer %d gate clk!\n", id);
		return ret;
	}

	return ret;
}

int sunxi_htimer_init(int id)
{
	int ret;
	struct sunxi_htimer *timer;
	uint32_t val;
	char irqname[32];
	const struct sunxi_timer_params *para;

	timer = g_htimer;

	if (g_htimer[id].lock) {
		HTIMER_ERR("htimer %d already init\n", id);
		return -1;
	}

	g_htimer[id].lock = hal_mutex_create();
	if (!g_htimer[id].lock) {
		HTIMER_ERR("htimer mutex create failed\n");
		return -1;
	}

	/* disable hrtimer of id */
	val = readl(HTIMER_CTL_REG(id));
	writel(val & ~HTIMER_CTL_ENABLE, HTIMER_CTL_REG(id));

	/* clear pending */
	writel(PENDING_BIT(id), HTIMER_IRQ_ST_REG(id));

	para = &g_sunxi_timer_params[id];
	timer[id].timer_id = id;
	timer[id].clk_rate = 24000000;      //ahb1,should get form clk driver
	timer[id].irq = para->irq_num;
	timer[id].min_delta_ticks = HTIMER_SYNC_TICKS;
	timer[id].max_delta_ticks = 0xffffffff;
	timer[id].callback = NULL;
	timer[id].param = NULL;
	snprintf(irqname, 32, "htimer%d", id);
	ret = hal_request_irq((para->irq_num), sunxi_htimer_irq_handle, irqname, (void *)&timer[id]);
	if (ret < 0)
		HTIMER_ERR("timer%d hal_request_irq error!\n", id);

	/*enable timer irq*/
	val = readl(HTIMER_IRQ_EN_REG(id));
	val |= HTIMER_IRQ_EN(id);
	writel(val, HTIMER_IRQ_EN_REG(id));

	/* enable irq */
	para = &g_sunxi_timer_params[id];
	hal_enable_irq(para->irq_num);
	return 0;
}


int sunxi_htimer_deinit(int id)
{
	uint32_t val;
	const struct sunxi_timer_params *para;

	para = &g_sunxi_timer_params[id];
	/* disable hrtimer of id */
	val = readl(HTIMER_CTL_REG(id));
	writel(val & ~HTIMER_CTL_ENABLE, HTIMER_CTL_REG(id));

	/* clear pending */
	writel(PENDING_BIT(id), HTIMER_IRQ_ST_REG(id));

	hal_free_irq(para->irq_num);

	hal_disable_irq(para->irq_num);

	hal_mutex_delete(g_htimer[id].lock);
	g_htimer[id].lock = NULL;
	return 0;
}
