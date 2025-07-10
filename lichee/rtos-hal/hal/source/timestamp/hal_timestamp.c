#include "aw_common.h"
#include <hal_clk.h>
#include <stdlib.h>
#include <hal_interrupt.h>
#include "platform_timestamp.h"

#define DAY

u64 hal_timestamp_read(void)
{
	u64 current_time;
	u32 ts_low_time;
	u64 ts_high_time;
	u32 reg_val = 0;

	reg_val |= TS_RL_EN;
	hal_writel(reg_val, TS_CTRL_REG);

	ts_low_time = hal_readl(TS_LOW_REG);
	ts_high_time = hal_readl(TS_HIGH_REG);
	ts_high_time = ts_high_time << 32;
	current_time = ts_high_time + ts_low_time;
	return current_time;

}

void hal_timestamp_reset(void)
{
	u32 reg_val = 0;

	reg_val |= TS_CLR_EN;
	hal_writel(reg_val, TS_CTRL_REG);
}

void hal_timestamp_init(void) {
	/* when the rv init,the timestamp start counting , the ctrl reg default 0x0 */
	u32 reg_val = 0;

	hal_writel(reg_val, TS_CTRL_REG);

}

void hal_timestamp_unint(void) {
	u32 reg_val = 0;

	reg_val |= TS_CLK_SRC_SEL;
	hal_writel(reg_val, TS_CTRL_REG);
}

void hal_timestamp_printf_runtime(void) {
	unsigned long current_time = 0;
	unsigned long days, hours, minutes, seconds;

	current_time = hal_timestamp_read();
	seconds = current_time / TIMESTAMP_CLK_HZ; /* runtime s */
	minutes = seconds / 60;
	hours = minutes / 60;
	days = hours / 24;

	seconds %= 60;
	minutes %= 60;
	hours %= 24;

	printf("runtime is %ld d,%ld h,%ld m,%ld s\n"
			, days, hours, minutes, seconds);
}
