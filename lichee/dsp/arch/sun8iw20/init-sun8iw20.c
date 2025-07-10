#include <stdio.h>
#include <xtensa/hal.h>
#include <xtensa/tie/xt_externalregisters.h>
#include <xtensa/config/core.h>
#include <xtensa/config/core-matmap.h>
#include <xtensa/xtruntime.h>
#ifdef CONFIG_DRIVERS_SOUND
#include <snd_core.h>
#endif
#ifdef CONFIG_COMPONENTS_AW_AUDIO_SYSTEM
#include <AudioSystem.h>
#endif

//int32_t console_uart = UART_UNVALID;
int32_t console_uart = 0xffff;
void cache_config(void) {
	/* 0x0~0x20000000-1 is non-cacheable */
	xthal_set_region_attribute((void *)0x00000000, 0x20000000, XCHAL_CA_WRITEBACK, 0);
	xthal_set_region_attribute((void *)0x00000000, 0x20000000, XCHAL_CA_BYPASS, 0);

	/* 0x20000000~0x40000000-1 is cacheable */
	xthal_set_region_attribute((void *)0x20000000, 0x20000000, XCHAL_CA_WRITEBACK, 0);

	/* 0x4000000~0x80000000-1 is non-cacheable */
	xthal_set_region_attribute((void *)0x40000000, 0x40000000, XCHAL_CA_WRITEBACK, 0);
	xthal_set_region_attribute((void *)0x40000000, 0x40000000, XCHAL_CA_BYPASS, 0);

	/* 0x80000000~0xC0000000-1 is non-cacheable */
	xthal_set_region_attribute((void *)0x80000000, 0x40000000, XCHAL_CA_WRITEBACK, 0);
	xthal_set_region_attribute((void *)0x80000000, 0x40000000, XCHAL_CA_BYPASS, 0);

	/* 0xC0000000~0xFFFFFFFF is  cacheable */
	xthal_set_region_attribute((void *)0xC0000000, 0x40000000, XCHAL_CA_WRITEBACK, 0);

	/* set prefetch level */
	xthal_set_cache_prefetch(XTHAL_PREFETCH_BLOCKS(8) |XTHAL_DCACHE_PREFETCH_HIGH | XTHAL_ICACHE_PREFETCH_HIGH |XTHAL_DCACHE_PREFETCH_L1);
}

unsigned int xtbsp_clock_freq_hz(void)
{
	unsigned int dsp_clk_freq_hz = 600000000;//must init value
	int       clk_rate = 0;

	extern int dsp_get_freq(void);
	clk_rate = dsp_get_freq();

	if (clk_rate <= 0) {
		/* return default freq.*/
		goto err_clk_get;
	}

	dsp_clk_freq_hz = clk_rate;

err_clk_get:
	return dsp_clk_freq_hz;
}

void print_banner(void)
{
	printf("\r\n");
	printf(" *******************************************\r\n");
	printf(" **      Welcome to %s DSP FreeRTOS     **\r\n", LICHEE_IC);
	printf(" ** Copyright (C) 2022-2024 AllwinnerTech **\r\n");
	printf(" **                                       **\r\n");
	printf(" **      starting xtensa FreeRTOS         **\r\n");
	printf(" *******************************************\r\n");
	printf("\r\n");
	printf("Date:%s, Time:%s\n", __DATE__, __TIME__);
}

#ifdef CONFIG_LINUX_DEBUG
extern struct dts_sharespace_t dts_sharespace;
static void _sharespace_init() {
	volatile struct spare_rtos_head_t *pstr = platform_head;
	volatile struct dts_msg_t *pdts = &pstr->rtos_img_hdr.dts_msg;
	int val = 0;
	val = pdts->dts_sharespace.status;
	if(val == DTS_OPEN) {
		dts_sharespace.dsp_write_addr = pdts->dts_sharespace.dsp_write_addr;
		dts_sharespace.dsp_write_size = pdts->dts_sharespace.dsp_write_size;
		dts_sharespace.arm_write_addr = pdts->dts_sharespace.arm_write_addr;
		dts_sharespace.arm_write_size = pdts->dts_sharespace.arm_write_size;
		dts_sharespace.dsp_log_addr = pdts->dts_sharespace.dsp_log_addr;
		dts_sharespace.dsp_log_size = pdts->dts_sharespace.dsp_log_size;
		debug_common_init();
	}
}
#endif

void app_init(void)
{
#ifdef CONFIG_DRIVERS_SOUND
	sunxi_soundcard_init();
#endif
#ifdef CONFIG_COMPONENTS_AW_AUDIO_SYSTEM
	AudioSystemInit();
#endif
}
