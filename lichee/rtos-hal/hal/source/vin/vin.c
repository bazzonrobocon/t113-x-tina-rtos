/*
 * vin.c
 *
 * Copyright (c) 2018 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zequn Zheng <zequnzhengi@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <hal_clk.h>
#include <hal_timer.h>
#include <hal_time.h>
#include <sunxi_hal_gpadc.h>
#include <openamp/sunxi_helper/openamp.h>
#include <hal_cfg.h>
#include <hal_gpio.h>
#include "vin.h"
#ifdef CONFIG_KERNEL_FREERTOS
#include <script.h>
#include <console.h>
#include <openamp/sunxi_helper/rpmsg_master.h>

#ifdef CONFIG_ARCH_RISCV_PMP
#include <pmp.h>
#endif
#if defined CONFIG_VIDEO_SUNXI_VIN_SPECIAL
#include "./vin_rpmsg/vin_rpmsg.h"
#endif

#define IS_ERR_OR_NULL(pointer) (pointer == NULL)
#define PTR_ERR(x)			((long)(x))
#endif
#ifdef CONFIG_KERNEL_FREERTOS
struct memheap isp_mempool;
extern int rpbuf_init(void);
#else
struct rt_memheap isp_mempool;
#endif

#ifdef CONFIG_VIN_WAIT_AMP_INIT
hal_sem_t vin_sem;
#endif

unsigned int vin_log_mask = 0;/* = 0xffff - VIN_LOG_ISP - VIN_LOG_STAT - VIN_LOG_STAT1; */
struct isp_get_cfg_t isp_get_cfg[ISP_GET_CFG_NUM] = {
	[0] = {
		.sensor_get_fps = 15,
	},
	[1] = {
		.sensor_get_fps = 15,
	},
};
extern struct csi_dev *global_csi[VIN_MAX_CSI];
extern struct vin_core *global_vinc[VIN_MAX_VIDEO];

void vin_set_from_partition(int isp_id, unsigned char *ir_en)
{
#if !defined NOT_USE_ISP
	SENSOR_ISP_CONFIG_S *sensor_isp_cfg = NULL;
	unsigned int check_sign = 0;
	int id = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);

#if defined CONFIG_ISP_HARD_LIGHTADC || defined CONFIG_ISP_ONLY_HARD_LIGHTADC
	unsigned int gpadc_ch = 0;
	unsigned int gpadc_value = 0;
	hal_gpadc_init();
	hal_gpadc_channel_init(gpadc_ch);
#endif

	/*read from ddr*/
	if (id == 0) {  /* isp0 */
		sensor_isp_cfg = (SENSOR_ISP_CONFIG_S *)VIN_SENSOR0_RESERVE_ADDR;
		check_sign = 0xAA66AA66;
	} else { /* isp1/isp2 */
		sensor_isp_cfg = (SENSOR_ISP_CONFIG_S *)VIN_SENSOR1_RESERVE_ADDR;
		check_sign = 0xBB66BB66;
	}
	if (sensor_isp_cfg->sign != check_sign) {
		vin_warn("sensor%d:sign is 0x%x\n", id, sensor_isp_cfg->sign);
		return;
	}

	isp_get_cfg[id].sensor_wdr_on = sensor_isp_cfg->wdr_mode;
	if (sensor_isp_cfg->fps)
		isp_get_cfg[id].sensor_get_fps = sensor_isp_cfg->fps;
	isp_get_cfg[id].sensor_deinit = sensor_isp_cfg->sensor_deinit;
	isp_get_cfg[id].get_yuv_en = sensor_isp_cfg->get_yuv_en;
	vin_print("isp%d:get wdr mode is %d, fps is %d\n", id, isp_get_cfg[id].sensor_wdr_on, isp_get_cfg[id].sensor_get_fps);

#if defined CONFIG_ISP_HARD_LIGHTADC || defined CONFIG_ISP_ONLY_HARD_LIGHTADC
	gpadc_value = gpadc_read_channel_data(gpadc_ch);
	/* boot0 set ir, melis set isp ir */
	if (sensor_isp_cfg->ircut_state) {
		if (sensor_isp_cfg->ircut_state == 1) {
			ir_mode = DAY_MODE;
			*ir_en = 1 + 2;
		} else if (sensor_isp_cfg->ircut_state == 2) {
			ir_mode = NIGHT_MODE;
			*ir_en = 2 + 2;
		}
		vin_print("boot0 set ir, ircut_state is %s, light_def is %d, gpadc_value is %d\n",
				(sensor_isp_cfg->ircut_state == 1 ? "day_state" : "night_state"),
				sensor_isp_cfg->light_def, gpadc_value);
	} else {
		/* force day or night*/
		if (sensor_isp_cfg->ir_mode == DAY_MODE || sensor_isp_cfg->ir_mode == NIGHT_MODE) {
			ir_mode = sensor_isp_cfg->ir_mode;
		} else { /* auto mode */
			if (sensor_isp_cfg->light_def == 0)
				sensor_isp_cfg->light_def = 1000;
			if (sensor_isp_cfg->adc_mode == 0) {
				if (sensor_isp_cfg->light_def > gpadc_value)
					ir_mode = NIGHT_MODE;
				else
					ir_mode = DAY_MODE;
			} else {
				if (sensor_isp_cfg->light_def > gpadc_value)
					ir_mode = DAY_MODE;
				else
					ir_mode = NIGHT_MODE;
			}
		}

		if (ir_mode == DAY_MODE) {
			sensor_isp_cfg->ircut_state = 1;
			*ir_en = 1;
		} else {
			sensor_isp_cfg->ircut_state = 2;
			*ir_en = 2;
		}
		vin_print("boot0 not set ir, ir_mode is %s, ircut_state is %s, light_def is %d, gpadc_value is %d\n",
				(sensor_isp_cfg->ir_mode == DAY_MODE ? "day_mode" : (sensor_isp_cfg->ir_mode == NIGHT_MODE ? "night_mode" : "auto_mode")),
				(sensor_isp_cfg->ircut_state == 1 ? "day_state" : "night_state"),
				sensor_isp_cfg->light_def, gpadc_value);
	}
	//hal_gpadc_channel_exit(gpadc_ch);
	//hal_gpadc_deinit();
#elif defined CONFIG_ISP_FAST_CONVERGENCE
	if (sensor_isp_cfg->ir_mode == NIGHT_MODE) {
		*ir_en = 1;
	} else {
		*ir_en = 0;
	}
#endif
#endif
}

void vin_set_to_partition(int isp_id)
{
#if !defined NOT_USE_ISP
	SENSOR_ISP_CONFIG_S *sensor_isp_cfg = NULL;
	unsigned int check_sign = 0;
	int id = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);

	/*read from ddr*/
	if (id == 0) {
		sensor_isp_cfg = (SENSOR_ISP_CONFIG_S *)VIN_SENSOR0_RESERVE_ADDR;
		check_sign = 0xAA66AA66;
	} else {
		sensor_isp_cfg = (SENSOR_ISP_CONFIG_S *)VIN_SENSOR1_RESERVE_ADDR;
		check_sign = 0xBB66BB66;
	}
	if (sensor_isp_cfg->sign != check_sign) {
		vin_warn("sensor%d:sign is 0x%x\n", id, sensor_isp_cfg->sign);
		return;
	}

	sensor_isp_cfg->wdr_mode = isp_get_cfg[id].sensor_wdr_on;
	sensor_isp_cfg->fps = isp_get_cfg[id].sensor_get_fps;
	vin_print("sensor%d:set wdr mode is %d, fps is %d\n", id, isp_get_cfg[id].sensor_wdr_on, isp_get_cfg[id].sensor_get_fps);
#endif
}

void vin_set_lightadc_from_partition(int isp_id, int sensor_id, unsigned char ir_en)
{
#if defined CONFIG_ISP_ONLY_HARD_LIGHTADC
	SENSOR_ISP_CONFIG_S *sensor_isp_cfg = NULL;
	unsigned int check_sign = 0;
	unsigned int gpadc_ch = 0;
	unsigned int gpadc_value = 0;
	unsigned short ae_idx = 0;
	unsigned char hdr_ratio;
	struct sensor_exp_gain exp_gain;
	int i;
	int id = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);

	/*read from ddr*/
	if (id == 0) {  /* isp0 */
		sensor_isp_cfg = (SENSOR_ISP_CONFIG_S *)VIN_SENSOR0_RESERVE_ADDR;
		check_sign = 0xAA66AA66;
	} else { /* isp1/isp2 */
		sensor_isp_cfg = (SENSOR_ISP_CONFIG_S *)VIN_SENSOR1_RESERVE_ADDR;
		check_sign = 0xBB66BB66;
	}
	if (sensor_isp_cfg->sign != check_sign) {
		vin_warn("sensor%d:sign is 0x%x\n", id, sensor_isp_cfg->sign);
		return;
	}

	sunxi_isp_set_lightadc_debug_en(isp_id, sensor_isp_cfg->lightadc_debug_en);

	memset(&exp_gain, 0, sizeof(exp_gain));

	hdr_ratio = sensor_isp_cfg->hdr_light_sensor_ratio == 0 ? 32 : sensor_isp_cfg->hdr_light_sensor_ratio;
	if (sensor_isp_cfg->light_sensor_en) {
		gpadc_value = gpadc_read_channel_data(gpadc_ch);
		if (ir_en == 1 || ir_en == 3) { //day mode
			if (isp_get_cfg[id].sensor_wdr_on == ISP_DOL_WDR_MODE) {
				if (gpadc_value < sensor_isp_cfg->hdr[0].u16LightValue)
						i = 0;
				else {
					for (i = 0; i < DAY_LIGNT_SENSOR_SIZE - 1; i++) {
						if (sensor_isp_cfg->hdr[i].u16LightValue == 0)
							break;
						if (sensor_isp_cfg->hdr[i].u16LightValue != 0 && sensor_isp_cfg->hdr[i + 1].u16LightValue == 0)
							break;
						if (gpadc_value >= sensor_isp_cfg->hdr[i].u16LightValue && gpadc_value < sensor_isp_cfg->hdr[i+1].u16LightValue)
							break;
					}
				}
				exp_gain.exp_val = sensor_isp_cfg->hdr[i].u32SnsExposure;
				exp_gain.exp_short_val = exp_gain.exp_val / hdr_ratio;
				exp_gain.gain_val = sensor_isp_cfg->hdr[i].u32SnsAgain >> 4;
				ae_idx = sensor_isp_cfg->hdr[i].ae_table_idx;
			} else {
				if (gpadc_value < sensor_isp_cfg->linear[0].u16LightValue)
						i = 0;
				else {
					for (i = 0; i < DAY_LIGNT_SENSOR_SIZE - 1; i++) {
						/*vin_print("table:gpadc_value is %d, exp&again is %d/%d, ae_table idx is %d\n",
											sensor_isp_cfg->linear[i].u16LightValue, sensor_isp_cfg->linear[i].u32SnsExposure,
											sensor_isp_cfg->linear[i+1].u16LightValue, sensor_isp_cfg->linear[i].ae_table_idx);*/
						if (sensor_isp_cfg->linear[i].u16LightValue == 0)
							break;
						if (sensor_isp_cfg->linear[i].u16LightValue != 0 && sensor_isp_cfg->linear[i + 1].u16LightValue == 0)
							break;
						if (gpadc_value >= sensor_isp_cfg->linear[i].u16LightValue && gpadc_value < sensor_isp_cfg->linear[i+1].u16LightValue)
							break;
					}
				}
				exp_gain.exp_val = sensor_isp_cfg->linear[i].u32SnsExposure;
				exp_gain.gain_val = sensor_isp_cfg->linear[i].u32SnsAgain >> 4;
				ae_idx = sensor_isp_cfg->linear[i].ae_table_idx;
			}
		} else { //night mode
			if (isp_get_cfg[id].sensor_wdr_on == ISP_DOL_WDR_MODE) {
				if (gpadc_value < sensor_isp_cfg->hdr_night[0].u16LightValue)
						i = 0;
				else {
					for (i = 0; i < NIGHT_LIGNT_SENSOR_SIZE - 1; i++) {
						if (sensor_isp_cfg->hdr_night[i].u16LightValue == 0)
							break;
						if (sensor_isp_cfg->hdr_night[i].u16LightValue != 0 && sensor_isp_cfg->hdr_night[i + 1].u16LightValue == 0)
							break;
						if (gpadc_value >= sensor_isp_cfg->hdr_night[i].u16LightValue && gpadc_value < sensor_isp_cfg->hdr_night[i+1].u16LightValue)
							break;
					}
				}
				exp_gain.exp_val = sensor_isp_cfg->hdr_night[i].u32SnsExposure;
				exp_gain.exp_short_val = exp_gain.exp_val / hdr_ratio;
				exp_gain.gain_val = sensor_isp_cfg->hdr_night[i].u32SnsAgain >> 4;
				ae_idx = sensor_isp_cfg->hdr_night[i].ae_table_idx;
			} else {
				if (gpadc_value < sensor_isp_cfg->linear_night[0].u16LightValue)
						i = 0;
				else {
					for (i = 0; i < NIGHT_LIGNT_SENSOR_SIZE - 1; i++) {
						if (sensor_isp_cfg->linear_night[i].u16LightValue == 0)
							break;
						if (sensor_isp_cfg->linear_night[i].u16LightValue != 0 && sensor_isp_cfg->linear_night[i + 1].u16LightValue == 0)
							break;
						if (gpadc_value >= sensor_isp_cfg->linear_night[i].u16LightValue && gpadc_value < sensor_isp_cfg->linear_night[i+1].u16LightValue)
							break;
					}
				}
				exp_gain.exp_val = sensor_isp_cfg->linear_night[i].u32SnsExposure;
				exp_gain.gain_val = sensor_isp_cfg->linear_night[i].u32SnsAgain >> 4;
				ae_idx = sensor_isp_cfg->linear_night[i].ae_table_idx;
			}
		}
		vin_print("gpadc_value is %d, exp&again is %d/%d, ae_table idx is %d\n", gpadc_value, exp_gain.exp_val, exp_gain.gain_val, ae_idx);
		if (exp_gain.exp_val == 0 || exp_gain.gain_val == 0)
			return;
		sunxi_isp_set_ae_idx(isp_id, ae_idx);
		if (global_sensors[sensor_id].sensor_core->s_exp_gain)
			global_sensors[sensor_id].sensor_core->s_exp_gain(sensor_id, &exp_gain);
	}
#endif
}

#if defined CONFIG_KERNEL_FREERTOS
static int vin_md_get_clk(void)
{
	vind_default_clk[VIN_TOP_CLK].clock = hal_clock_get(vind_default_clk[VIN_TOP_CLK].type, vind_default_clk[VIN_TOP_CLK].clock_id);
	if (IS_ERR_OR_NULL(vind_default_clk[VIN_TOP_CLK].clock)) {
		vin_err("get csi top clk fail\n");
		return PTR_ERR(vind_default_clk[VIN_TOP_CLK].clock);
	}

	vind_default_clk[VIN_TOP_CLK_SRC].clock = hal_clock_get(vind_default_clk[VIN_TOP_CLK_SRC].type, vind_default_clk[VIN_TOP_CLK_SRC].clock_id);
	if (IS_ERR_OR_NULL(vind_default_clk[VIN_TOP_CLK_SRC].clock)) {
		vin_err("get csi top clk src fail\n");
		return PTR_ERR(vind_default_clk[VIN_TOP_CLK_SRC].clock);
	}
#if defined CONFIG_ARCH_SUN55IW3
	vind_default_clk[VIN_TOP_CLK_SRC].frequency = 1800000000;
#else
	vind_default_clk[VIN_TOP_CLK_SRC].frequency = vind_default_clk[VIN_TOP_CLK].frequency;
#endif

	vind_default_isp_clk[VIN_ISP_CLK].clock = hal_clock_get(vind_default_isp_clk[VIN_ISP_CLK].type, vind_default_isp_clk[VIN_ISP_CLK].clock_id);
	if (IS_ERR_OR_NULL(vind_default_isp_clk[VIN_ISP_CLK].clock)) {
		vin_err("get csi isp clk fail\n");
		return PTR_ERR(vind_default_isp_clk[VIN_ISP_CLK].clock);
	}

	vind_default_isp_clk[VIN_ISP_CLK_SRC].clock = hal_clock_get(vind_default_isp_clk[VIN_ISP_CLK_SRC].type, vind_default_isp_clk[VIN_ISP_CLK_SRC].clock_id);
	if (IS_ERR_OR_NULL(vind_default_isp_clk[VIN_ISP_CLK_SRC].clock)) {
		vin_err("get csi isp src clk fail\n");
		return PTR_ERR(vind_default_isp_clk[VIN_ISP_CLK_SRC].clock);
	}

	vind_default_mbus_clk[VIN_CSI_BUS_CLK].clock = hal_clock_get(vind_default_mbus_clk[VIN_CSI_BUS_CLK].type, vind_default_mbus_clk[VIN_CSI_BUS_CLK].clock_id);
	if (IS_ERR_OR_NULL(vind_default_mbus_clk[VIN_CSI_BUS_CLK].clock)) {
		vin_err("get csi bus clk failed!\n");
		return PTR_ERR(vind_default_mbus_clk[VIN_CSI_BUS_CLK].clock);
	}

	vind_default_mbus_clk[VIN_CSI_MBUS_CLK].clock = hal_clock_get(vind_default_mbus_clk[VIN_CSI_MBUS_CLK].type, vind_default_mbus_clk[VIN_CSI_MBUS_CLK].clock_id);
	if (IS_ERR_OR_NULL(vind_default_mbus_clk[VIN_CSI_MBUS_CLK].clock)) {
		vin_err("get csi mbus clk failed!\n");
		return PTR_ERR(vind_default_mbus_clk[VIN_CSI_MBUS_CLK].clock);
	}

	vind_default_mbus_clk[VIN_ISP_MBUS_CLK].clock = hal_clock_get(vind_default_mbus_clk[VIN_ISP_MBUS_CLK].type, vind_default_mbus_clk[VIN_ISP_MBUS_CLK].clock_id);
	if (IS_ERR_OR_NULL(vind_default_mbus_clk[VIN_ISP_MBUS_CLK].clock)) {
		vin_err("get csi isp mbus clk fail!\n");
		return PTR_ERR(vind_default_mbus_clk[VIN_ISP_MBUS_CLK].clock);
	}

	vind_default_rst_clk[VIN_CSI_RET].clock = hal_reset_control_get(vind_default_rst_clk[VIN_CSI_RET].type, vind_default_rst_clk[VIN_CSI_RET].clock_id);
	if (IS_ERR_OR_NULL(vind_default_rst_clk[VIN_CSI_RET].clock)) {
		vin_err("get csi reset control fail\n");
		return PTR_ERR(vind_default_rst_clk[VIN_CSI_RET].clock);
	}

	vind_default_rst_clk[VIN_ISP_RET].clock = hal_reset_control_get(vind_default_rst_clk[VIN_ISP_RET].type, vind_default_rst_clk[VIN_CSI_RET].clock_id);
	if (IS_ERR_OR_NULL(vind_default_rst_clk[VIN_ISP_RET].clock)) {
		vin_warn("Get isp reset control fail!\n");
		vind_default_rst_clk[VIN_ISP_RET].clock = NULL;
	}

	return 0;
}

static int vin_md_put_clk(void)
{
       hal_reset_control_put(vind_default_rst_clk[VIN_ISP_RET].clock);
       hal_reset_control_put(vind_default_rst_clk[VIN_CSI_RET].clock);
       hal_clock_put(vind_default_mbus_clk[VIN_ISP_MBUS_CLK].clock);
       hal_clock_put(vind_default_mbus_clk[VIN_CSI_MBUS_CLK].clock);
       hal_clock_put(vind_default_mbus_clk[VIN_CSI_BUS_CLK].clock);
       hal_clock_put(vind_default_isp_clk[VIN_ISP_CLK_SRC].clock);
       hal_clock_put(vind_default_isp_clk[VIN_ISP_CLK].clock);
       hal_clock_put(vind_default_clk[VIN_TOP_CLK_SRC].clock);
       hal_clock_put(vind_default_clk[VIN_TOP_CLK].clock);

       return 0;
}

static int vin_md_clk_en(unsigned int en)
{
	int ret;

	if (en) {
		ret = vin_md_get_clk();
		if (ret < 0)
			vin_err("csic clk get fail\n");
	}

	if (en) {
		if (hal_clk_set_parent(vind_default_clk[VIN_TOP_CLK].clock, vind_default_clk[VIN_TOP_CLK_SRC].clock)) {
			vin_err("csi reset deassert fail!\n");
			return -1;
		}

		if (hal_clk_set_rate(vind_default_clk[VIN_TOP_CLK_SRC].clock, vind_default_clk[VIN_TOP_CLK_SRC].frequency)) {
			vin_warn("set top_clk src clock error\n");
			//return -1;
		}

		if (hal_clk_set_rate(vind_default_clk[VIN_TOP_CLK].clock, vind_default_clk[VIN_TOP_CLK].frequency)) {
			vin_err("set top_clk clock error\n");
			return -1;
		}
#if !defined NOT_USE_ISP
		if (hal_clk_set_parent(vind_default_isp_clk[VIN_ISP_CLK].clock, vind_default_isp_clk[VIN_ISP_CLK_SRC].clock)) {
			vin_err("set top_clk source failed!\n");
			return -1;
		}

		if (hal_clk_set_rate(vind_default_isp_clk[VIN_ISP_CLK_SRC].clock, vind_default_isp_clk[VIN_ISP_CLK].frequency)) {
			vin_warn("set top_clk src clock error\n");
			//return -1;
		}

		if (hal_clk_set_rate(vind_default_isp_clk[VIN_ISP_CLK].clock, vind_default_isp_clk[VIN_ISP_CLK].frequency)) {
			vin_err("set top_clk clock error\n");
			return -1;
		}
#endif
	}

	if (en) {
		if (hal_reset_control_deassert(vind_default_rst_clk[VIN_CSI_RET].clock)) {
			vin_err("csi_rst deassert failed\n");
			return -1;
		}
#if !defined NOT_USE_ISP
		if (hal_reset_control_deassert(vind_default_rst_clk[VIN_ISP_RET].clock)) {
			vin_err("isp_rst deassert failed\n");
			return -1;
		}
#endif
		if (hal_clock_enable(vind_default_mbus_clk[VIN_CSI_BUS_CLK].clock)) {
			vin_err("csi_bus_src clock enable error\n");
			return -1;
		}

		if (hal_clock_enable(vind_default_mbus_clk[VIN_CSI_MBUS_CLK].clock)) {
			vin_err("csi_mbus_clk clock enable error\n");
			return -1;
		}
#if !defined NOT_USE_ISP
		if (hal_clock_enable(vind_default_mbus_clk[VIN_ISP_MBUS_CLK].clock)) {
			vin_err("isp_mbus_clk clock enable error\n");
			return -1;
		}
#endif
		if (hal_clock_enable(vind_default_clk[VIN_TOP_CLK_SRC].clock)) {
			vin_err("top_clk_src clock enable error\n");
			return -1;
		}

		if (hal_clock_enable(vind_default_clk[VIN_TOP_CLK].clock)) {
			vin_err("top_clk clock enable error\n");
			return -1;
		}
#if !defined NOT_USE_ISP
		if (hal_clock_enable(vind_default_isp_clk[VIN_ISP_CLK_SRC].clock)) {
			vin_err("top_clk_src clock enable error\n");
			return -1;
		}

		if (hal_clock_enable(vind_default_isp_clk[VIN_ISP_CLK].clock)) {
			vin_err("isp clock enable error\n");
			return -1;
		}
#endif
	} else {
		if (hal_clock_disable(vind_default_clk[VIN_TOP_CLK].clock)) {
			vin_err("top_clk clock disable error\n");
			return -1;
		}

		if (hal_clock_disable(vind_default_mbus_clk[VIN_CSI_MBUS_CLK].clock)) {
			vin_err("csi_mbus_clk clock disable error\n");
			return -1;
		}

		if (hal_clock_disable(vind_default_mbus_clk[VIN_CSI_BUS_CLK].clock)) {
			vin_err("csi_bus_clk clock disable error\n");
			return -1;
		}

		if (hal_reset_control_assert(vind_default_rst_clk[VIN_CSI_RET].clock)) {
			vin_err("csi_ret_clk clock disable error\n");
			return -1;
		}

		if (hal_clock_disable(vind_default_isp_clk[VIN_ISP_CLK].clock)) {
			vin_err("isp clock disable error\n");
			return -1;
		}

		if (hal_clock_disable(vind_default_mbus_clk[VIN_ISP_MBUS_CLK].clock)) {
			vin_err("isp_mbus_clk clock disable error\n");
			return -1;
		}

		if (hal_reset_control_assert(vind_default_rst_clk[VIN_ISP_RET].clock)) {
			vin_warn("isp_ret_clk clock disable error\n");
			//return -1;
		}
	}
	if (!en)
		vin_md_put_clk();
	vin_print("set clk end\n");

	return 0;
}
#else
static int vin_md_clk_en(unsigned int en)
{
	struct vin_clk_info top_clk_src;

	hal_writel(0x00000001, 0x02001c2c);
	hal_writel(0x00010001, 0x02001c2c);

	if (vind_default_clk[VIN_TOP_CLK].frequency > 300000000) {
		top_clk_src.clock = vind_default_clk[VIN_TOP_CLK_SRC].clock;
		top_clk_src.frequency = VIN_PLL_CSI_RATE;
	} else {
		top_clk_src.clock = vind_default_clk[VIN_TOP_CLK_SRC1].clock;
		top_clk_src.frequency = vind_default_clk[VIN_TOP_CLK_SRC1].frequency;
	}

	if (en) {
		if (hal_clk_set_parent(vind_default_clk[VIN_TOP_CLK].clock, top_clk_src.clock)) {
			vin_err("set top_clk source failed!\n");
			return -1;
		}

		if (hal_clk_set_rate(top_clk_src.clock, top_clk_src.frequency)) {
			vin_warn("set top_clk src clock error\n");
			//return -1;
		}

		if (hal_clk_set_rate(vind_default_clk[VIN_TOP_CLK].clock, vind_default_clk[VIN_TOP_CLK].frequency)) {
			vin_err("set top_clk clock error\n");
			return -1;
		}
	}

	if (en) {
		if (hal_clock_enable(top_clk_src.clock)) {
			vin_err("top_clk_src clock enable error\n");
			return -1;
		}

		if (hal_clock_enable(vind_default_clk[VIN_TOP_CLK].clock)) {
			vin_err("top_clk clock enable error\n");
			return -1;
		}

		if (hal_clock_enable(vind_default_isp_clk[VIN_ISP_CLK].clock)) {
			vin_err("isp clock enable error\n");
			return -1;
		}

		/*if (vind_default_clk[VIN_TOP_CLK].frequency > 300000000) {
			hal_writel(0xc9006201, 0x02001048); //set pll_csi 2376M
			hal_writel(0xd1303333, 0x02001148);
			hal_writel(0x83000006, 0x02001c04); //set top_csi 340M
		}*/
	} else {
		if (hal_clock_disable(vind_default_clk[VIN_TOP_CLK].clock)) {
			vin_err("top_clk clock disable error\n");
			return -1;
		}

		if (hal_clock_disable(vind_default_isp_clk[VIN_ISP_CLK].clock)) {
			vin_err("isp clock disable error\n");
			return -1;
		}
	}

	hal_writel(0x00000001, 0x02001c2c);
	hal_writel(0x00010001, 0x02001c2c);

	vin_print("set clk end\n");

	return 0;
}
#endif
static void vin_ccu_clk_gating_en(unsigned int en)
{
	/*if (en) {
		csic_ccu_clk_gating_enable();
		csic_ccu_mcsi_clk_mode(1);
		csic_ccu_mcsi_post_clk_enable(0);
		csic_ccu_mcsi_post_clk_enable(1);
	} else {
		csic_ccu_mcsi_post_clk_disable(1);
		csic_ccu_mcsi_post_clk_disable(0);
		csic_ccu_mcsi_clk_mode(0);
		csic_ccu_clk_gating_disable();
	}*/

	csic_ccu_clk_gating_disable();
}

static void vin_md_set_power(int on)
{
	if (on) {
		vin_md_clk_en(on);
		vin_ccu_clk_gating_en(on);
		hal_usleep(120);
		csic_top_enable();
		csic_mbus_req_mex_set(0xf);
		csic_isp_bridge_enable();
	} else {
		csic_isp_bridge_disable();
		csic_top_disable();
		vin_ccu_clk_gating_en(on);
		vin_md_clk_en(on);
	}
}

static void vin_subdev_ccu_en(unsigned int id, unsigned int en)
{

}

int vin_pipeline_set_mbus_config(unsigned int id)
{
	struct vin_core *vinc = global_vinc[id];
	struct v4l2_mbus_config mcfg;
	struct mbus_framefmt_res res;
	int sensor_id = vinc->rear_sensor;

	memset(&res, 0, sizeof(struct mbus_framefmt_res));
	if (global_sensors[sensor_id].sensor_core->g_mbus_config)
		global_sensors[sensor_id].sensor_core->g_mbus_config(sensor_id, &mcfg, &res);

	/* s_mbus_config on all mipi and csi */
	if (vinc->mipi_sel != 0xff)
		sunxi_mipi_s_mbus_config(vinc->mipi_sel, &mcfg, &res);

	sunxi_csi_s_mbus_config(vinc->csi_sel, &mcfg);
	vinc->total_rx_ch = global_csi[vinc->csi_sel]->bus_info.ch_total_num;

	if (vinc->tdm_rx_sel != 0xff)
		sunxi_tdm_s_mbus_config(vinc->tdm_rx_sel, &res);
#if !defined NOT_USE_ISP
	sunxi_isp_s_mbus_config(vinc->isp_sel, &res);
#endif

	return 0;
}

int vin_s_stream(unsigned int id, int enable)
{
	struct vin_core *vinc = global_vinc[id];
	int sensor_id = vinc->rear_sensor;
#if defined CSIC_DMA_VER_140_000
	struct vin_core *logic_vinc = global_vinc[dma_virtual_find_logic[vinc->id]];
#endif
	int ret;

	vin_log(VIN_LOG_MD, "vinc%d:tdm_rx_sel = %d,  mipi_sel = %d, isp_sel = %d, vincid = %d, csi_sel = %d\n",
			id, vinc->tdm_rx_sel, vinc->mipi_sel, vinc->isp_sel, vinc->id, vinc->csi_sel);

	if (enable) {
		if (vinc->tdm_rx_sel != 0xff)
			sunxi_tdm_subdev_s_stream(vinc->tdm_rx_sel, vinc->id, enable);
		hal_usleep(120);
		if (vinc->mipi_sel != 0xff)
			sunxi_mipi_subdev_s_stream(vinc->mipi_sel, vinc->id, enable);
		hal_usleep(120);
#if !defined NOT_USE_ISP
		sunxi_isp_subdev_s_stream(vinc->isp_sel, vinc->id, enable);
		hal_usleep(120);
#endif
#if !defined NOT_USE_VIPP
		sunxi_scaler_subdev_s_stream(vinc->id, enable);
		hal_usleep(120);
#endif
		vin_subdev_s_stream(vinc->id, enable);
		hal_usleep(120);
		sunxi_csi_subdev_s_stream(vinc->csi_sel, vinc->id, enable);
		hal_usleep(120);
		if (global_sensors[sensor_id].sensor_core->s_stream) {
			ret = global_sensors[sensor_id].sensor_core->s_stream(sensor_id, vinc->isp_sel, enable);
			if (ret)
				return ret;
		}
	} else {
		vin_subdev_s_stream(vinc->id, enable);
		hal_usleep(120);
#if defined CSIC_DMA_VER_140_000
		if (logic_vinc->work_mode == BK_ONLINE) {
#if !defined NOT_USE_VIPP
			sunxi_scaler_subdev_s_stream(vinc->id, enable);
			hal_usleep(120);
#endif
#if !defined NOT_USE_ISP
			sunxi_isp_subdev_s_stream(vinc->isp_sel, vinc->id, enable);
			hal_usleep(120);
#endif
			if (vinc->tdm_rx_sel != 0xff)
				sunxi_tdm_subdev_s_stream(vinc->tdm_rx_sel, vinc->id, enable);
		} else {
			if (vinc->tdm_rx_sel != 0xff)
				sunxi_tdm_subdev_s_stream(vinc->tdm_rx_sel, vinc->id, enable);
			hal_usleep(120);
#if !defined NOT_USE_ISP
			sunxi_isp_subdev_s_stream(vinc->isp_sel, vinc->id, enable);
			hal_usleep(120);
#endif
#if !defined NOT_USE_VIPP
			sunxi_scaler_subdev_s_stream(vinc->id, enable);
#endif
		}
#else
#if !defined NOT_USE_VIPP
			sunxi_scaler_subdev_s_stream(vinc->id, enable);
			hal_usleep(120);
#endif
#if !defined NOT_USE_ISP
			sunxi_isp_subdev_s_stream(vinc->isp_sel, vinc->id, enable);
			hal_usleep(120);
#endif
#endif
		hal_usleep(120);
		sunxi_csi_subdev_s_stream(vinc->csi_sel, vinc->id, enable);
#if !defined ISP_SERVER_FASTINIT || defined CONFIG_VIDEO_SUNXI_VIN_SPECIAL
		hal_usleep(120);
		if (vinc->mipi_sel != 0xff)
			sunxi_mipi_subdev_s_stream(vinc->mipi_sel, vinc->id, enable);
		hal_usleep(120);
		if (global_sensors[sensor_id].sensor_core->s_stream) {
			ret = global_sensors[sensor_id].sensor_core->s_stream(sensor_id, vinc->isp_sel, enable);
			if (ret)
				return ret;
		}
#endif
	}
	return 0;
}

static void vin_probe(unsigned int id)
{
	struct vin_core *vinc = &global_video[id];

	if (vinc->mipi_sel != 0xff)
		mipi_probe(vinc->mipi_sel);
	if (vinc->sensor_lane == 4)
		mipi_probe(vinc->mipi_sel + 1);
	csi_probe(vinc->csi_sel);
	if (vinc->tdm_rx_sel != 0xff)
		tdm_probe(vinc->tdm_rx_sel);
#if !defined NOT_USE_ISP
	isp_probe(vinc->isp_sel, vinc->rear_sensor); /* isp0 must first probe */
#endif
#if !defined NOT_USE_VIPP
	scaler_probe(vinc->id);
#endif
	vin_core_probe(vinc->id);
}

static void vin_free(unsigned int id)
{
	struct vin_core *vinc = &global_video[id];

	if (vinc->mipi_sel != 0xff)
		mipi_remove(vinc->mipi_sel);
	if (vinc->sensor_lane == 4)
		mipi_remove(vinc->mipi_sel + 1);
	csi_remove(vinc->csi_sel);
	if (vinc->tdm_rx_sel != 0xff)
		tdm_remove(vinc->tdm_rx_sel);
#if !defined NOT_USE_ISP
	isp_remove(vinc->isp_sel);
#endif
#if !defined NOT_USE_VIPP
	scaler_remove(vinc->id);
#endif
	vin_core_remove(vinc->id);
}

int vin_g_config(void)
{
#if !defined CONFIG_ARCH_SUN55IW3
	int i, ret = -1;
	user_gpio_set_t gpio_cfg;
	char main_name[16];
	char sub_name[16];
	int ivalue = 0;
	for (i = 0; i< VIN_MAX_VIDEO; i++)
	{
		ivalue = 0;
		sprintf(main_name, "sensor%d", i);
		sprintf(sub_name, "used%d", i);
#ifdef CONFIG_KERNEL_FREERTOS
		ret = hal_cfg_get_keyvalue(main_name, sub_name, (void*)&ivalue, 1);
#else
		ret = hal_cfg_get_sub_keyvalue(main_name, sub_name, (void*)&ivalue, 1);
#endif
		if(0 != ret) {
			vin_err("%s is %d\n", sub_name, ivalue);
			break;
		}
		vin_log(VIN_LOG_MD, "%s is %d\n", sub_name, ivalue);
		global_video[i].used = ivalue;
		if(1 != ivalue)
			continue;

		sprintf(sub_name, "reset%d", i);
#ifdef CONFIG_KERNEL_FREERTOS
		ret = hal_cfg_get_keyvalue(main_name, sub_name, (void*)&gpio_cfg, 4);
#else
		ret = hal_cfg_get_sub_keyvalue(main_name, sub_name, (void*)&gpio_cfg, 4);
#endif
		if(0 != ret) {
			vin_err("%s is port %d, num %d\n", main_name, gpio_cfg.port, gpio_cfg.port_num);
			break;
		}
		global_sensors[i].reset_gpio = (gpio_cfg.port - 1) * 32 + gpio_cfg.port_num;
		sprintf(sub_name, "pwdn%d", i);
#ifdef CONFIG_KERNEL_FREERTOS
		ret = hal_cfg_get_keyvalue(main_name, sub_name, (void*)&gpio_cfg, 4);
#else
		ret = hal_cfg_get_sub_keyvalue(main_name, sub_name, (void*)&gpio_cfg, 4);
#endif
		if(0 != ret) {
			vin_err("%s is port %d, num %d\n", main_name, gpio_cfg.port, gpio_cfg.port_num);
			break;
		}
		global_sensors[i].pwdn_gpio = (gpio_cfg.port - 1) * 32 + gpio_cfg.port_num;
		sprintf(sub_name, "mclk%d", i);
#ifdef CONFIG_KERNEL_FREERTOS
		ret = hal_cfg_get_keyvalue(main_name, sub_name, (void*)&gpio_cfg, 4);
#else
		ret = hal_cfg_get_sub_keyvalue(main_name, sub_name, (void*)&gpio_cfg, 4);
#endif
		if(0 != ret) {
			vin_err("%s is port %d, num %d\n", main_name, gpio_cfg.port, gpio_cfg.port_num);
			break;
		}
		vind_default_mclk[i].pin = (gpio_cfg.port - 1) * 32 + gpio_cfg.port_num;
		vind_default_mclk[i].pin_func[0] = gpio_cfg.mul_sel;
		vind_default_mclk[i].pin_func[1] = 0xf;
	}
#endif
	return 0;
}

int vin_g_status(void)
{
	int i, ret = -1;

	for (i = 0; i < VIN_MAX_VIDEO; i++) {
		if (global_video[i].used == 1)
			ret = 0;
	}

	return ret;
}

void vin_detect_sensor_list(int sensor_id)
{
#if !defined NOT_USE_ISP
	int i, save_detect_id;
	struct isp_autoflash_config_s *isp_autoflash_cfg = NULL;
	unsigned int sign;
	int ret = -1;

	if (sensor_id == 0) {
		isp_autoflash_cfg = (struct isp_autoflash_config_s *)ISP0_NORFLASH_SAVE;
		sign = 0xAA22AA22;
	} else {
		isp_autoflash_cfg = (struct isp_autoflash_config_s *)ISP1_NORFLASH_SAVE;
		sign = 0xBB22BB22;
	}

	save_detect_id = min(isp_autoflash_cfg->sensor_detect_id, MAX_DETECT_SENSOR - 1);
	vin_print("sensor%d save_detect_id is %d\n", sensor_id, save_detect_id);
	memcpy(&global_sensors[sensor_id], &global_sensors_list[sensor_id][save_detect_id], sizeof(struct sensor_list));
	global_sensors[sensor_id].sensor_core = find_sensor_func(global_sensors[sensor_id].sensor_name);
	if (global_sensors[sensor_id].sensor_core->sensor_test_i2c) {
		vin_print("sensor %s to save detect\n", global_sensors[sensor_id].sensor_name);
		ret = global_sensors[sensor_id].sensor_core->sensor_test_i2c(sensor_id);
	}
	if (ret) {
		for (i = 0; i < MAX_DETECT_SENSOR; i++) {
			if (i == save_detect_id)
				continue;
			memcpy(&global_sensors[sensor_id], &global_sensors_list[sensor_id][i], sizeof(struct sensor_list));
			global_sensors[sensor_id].sensor_core = find_sensor_func(global_sensors[sensor_id].sensor_name);
			if (global_sensors[sensor_id].sensor_core->sensor_test_i2c) {
				vin_print("sensor %s to detect\n", global_sensors[sensor_id].sensor_name);
				ret = global_sensors[sensor_id].sensor_core->sensor_test_i2c(sensor_id);
				if (!ret) {
					vin_print("find sensor %s\n", global_sensors[sensor_id].sensor_name);
					isp_autoflash_cfg->sensorlist_sign_id = sign;
					strcpy((char *)isp_autoflash_cfg->sensor_name, global_sensors[sensor_id].sensor_name);
					isp_autoflash_cfg->sensor_twi_addr = global_sensors[sensor_id].sensor_twi_addr;
					isp_autoflash_cfg->sensor_detect_id = i;
					break;
				}
			}
		}
		if (ret)
			isp_autoflash_cfg->sensorlist_sign_id = 0xFFFFFFFF;
	}
#endif
}
// fix unused-function
__attribute__((__unused__)) static void __csic_dump_regs(unsigned long addr, unsigned long size)
{
	unsigned int val;
	int cnt = 0;

	do {
		if (cnt % 4 == 0)
			printk("0x%08lx:", addr + cnt * 4);
		val = hal_readl(addr + cnt * 4);
		printk(" 0x%08x ", val);
		cnt++;
		if (cnt % 4 == 0 && cnt != 0)
			printk("\n");
	} while (size > cnt * 4);
}

#ifdef CONFIG_VIN_WAIT_AMP_INIT
int vin_continue_init()
{
	int cnt = 0;

	while (cnt < CONFIG_VIN_CONTINUE_INIT_TRY_TIMES) {
		if (vin_sem) {
			hal_sem_post(vin_sem);
			return 0;
		}
		cnt++;
		hal_msleep(1);
	}

	vin_err("sem_post error, vin_sem is NULL\n");
	return -1;
}
#endif

#ifdef CONFIG_ARCH_RISCV_PMP
int set_pmp_for_isp_reserved_mem(void)
{
	int ret;
	uint32_t reserved_mem_start, reserved_mem_end;

	reserved_mem_start = MEMRESERVE;
#if defined CONFIG_NOT_FRAME_LOSS
	reserved_mem_end = reserved_mem_start + MEMRESERVE_SIZE + YUV_MEMRESERVE_SIZE;
#else
	reserved_mem_end = reserved_mem_start + MEMRESERVE_SIZE;
#endif
	//printf("add PMP config for ISP reserved mem, start: 0x%08x, end: 0x%08x, size: 0x%08x\n",
	//		reserved_mem_start, reserved_mem_end - 1, reserved_mem_end - reserved_mem_start);
	ret = pmp_add_region(reserved_mem_start, reserved_mem_end, PMP_R | PMP_W);
	if (ret) {
		//printf("add PMP config for ISP reserved mem failed, ret: %d\n", ret);
		return ret;
	}

	return 0;
}
#endif

#if defined CONFIG_VIDEO_SUNXI_VIN_SPECIAL
int vin_open_special(int id)
{
	int ret = 0;
	int sensor_id;
	unsigned long reg_base;
	unsigned long ccu_base;

	vin_log(VIN_LOG_MD, "CSI start!\n");

	if (global_video[id].used != 1) {
		vin_err("this video node is not in use!\n");
		return -1;
	}

#ifdef CONFIG_KERNEL_FREERTOS
	memheap_init(&isp_mempool, "isp-mempool", (void *)MEMRESERVE, MEMRESERVE_SIZE);
#else
	rt_memheap_init(&isp_mempool, "isp-mempool", (void *)MEMRESERVE, MEMRESERVE_SIZE);
#endif

	reg_base = sunxi_vin_get_top_base();
	csic_top_set_base_addr(reg_base);
	vin_log(VIN_LOG_MD, "reg is 0x%lx\n", reg_base);

	ccu_base = sunxi_vin_get_ccu_base();
	csic_ccu_set_base_addr(ccu_base);
	vin_log(VIN_LOG_MD, "reg is 0x%lx\n", ccu_base);

	vin_md_set_power(PWR_ON);

	sunxi_twi_init(0);
	sunxi_twi_init(1);

	sensor_id = global_video[id].rear_sensor;

	global_sensors[sensor_id].sensor_core = find_sensor_func(global_sensors[sensor_id].sensor_name);

	vin_probe(id);
	vin_subdev_ccu_en(id, PWR_ON);

	return ret;
}

int vin_close_special(int id)
{
	vin_subdev_ccu_en(id, PWR_OFF);
	vin_free(id);
	sunxi_twi_exit(0);
	sunxi_twi_exit(1);
	vin_md_set_power(PWR_OFF);

	return 0;
}

#define CAR_REVERSE_TEST 1
#if CAR_REVERSE_TEST
#if defined DISPLAY_FRAM
extern void preview_display_tmp_test(unsigned long addr);
#endif
static int read_buffer(int id, void *addr, int size)
{
	return 0;
}

/* car_reverse demo */
int csi_init_special(int argc, char *argv[])
{
	struct vin_buffer *buffer = NULL;
	struct vin_core *vinc = NULL;

	vin_open_special(0);
	vin_reqbuf_special(0, 3);
	vin_streamon_special(0);
	vinc = global_vinc[0];
	while (1) {
		vin_dqbuffer_special(0, &buffer);
		read_buffer(0, &buffer->phy_addr, buffer->size);
#if defined DISPLAY_FRAM
		preview_display_tmp_test((unsigned long)buffer->phy_addr);
#endif
		vin_qbuffer_special(0, buffer);
		if (vinc->frame_cnt > 100)
			break;
	}
	vin_streamoff_special(0);
	vin_close_special(0);

	return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(csi_init_special, vin, rtthread vin run fast reverse code);
#endif
#else
int csi_init(int argc, const char **argv)
{
	struct vin_core *vinc = NULL;
	unsigned long reg_base;
	unsigned long ccu_base;
	int ret = 0;
	int i, j;
	int sensor_id;
	int select_video_num = VIN_MAX_VIDEO; //VIN_MAX_VIDEO
	unsigned char ir_en = 0;
#ifdef CONFIG_NOT_FRAME_LOSS
	gpio_data_t gpio_data = 0;
#endif
	int sensor_enable = 1;

#ifdef CONFIG_VIN_WAIT_AMP_INIT
	vin_sem = hal_sem_create(0);
	if (vin_sem == NULL) {
		vin_err("creating vin sem failed.\n");
		return -1;
	}
#endif

	if (hal_readl(rtc_base + 0x100 + 0x4 * RTC_NUM) & 0x2) {
		vin_print("rtc%d is 0x%x, csi_init is already init once, reinit isp server\n", RTC_NUM, hal_readl(rtc_base + 0x100 + 0x4 * RTC_NUM));
#ifdef CONFIG_VIN_WAIT_AMP_INIT
	if (vin_sem != NULL) {
		ret = hal_sem_timedwait(vin_sem, CONFIG_VIN_WAIT_AMP_INIT_SEM_TIMEOUT);
		if (ret != 0) {
			vin_err("vin wait rpbuf init timeout.\n");
			return -1;
		}
	} else {
		vin_err("vin_sem is NULL.\n");
		return -1;
	}
#endif
#ifndef CONFIG_KERNEL_FREERTOS
	openamp_init(); //wait rpmsg init
#endif
		for (i = 0; i < select_video_num; i++) {
			if (global_video[i].used == 1) {
				ret = isp_reinit(global_video[i].isp_sel);
			}
		}
		return ret;
	}

	vin_log(VIN_LOG_MD, "CSI start!\n");

	ret = vin_g_config();
	if (ret != 0) {
		vin_err("vin get config error\n");
		return -1;
	}

	ret = vin_g_status();
	if (ret != 0) {
		vin_err("There is no open CSI\n");
		return -1;
	}
#ifdef CONFIG_KERNEL_FREERTOS
	memheap_init(&isp_mempool, "isp-mempool", (void *)MEMRESERVE, MEMRESERVE_SIZE);
#else
	rt_memheap_init(&isp_mempool, "isp-mempool", (void *)MEMRESERVE, MEMRESERVE_SIZE);
#endif

	reg_base = sunxi_vin_get_top_base();
	csic_top_set_base_addr(reg_base);
	vin_log(VIN_LOG_MD, "reg is 0x%lx\n", reg_base);

	ccu_base = sunxi_vin_get_ccu_base();
	csic_ccu_set_base_addr(ccu_base);
	vin_log(VIN_LOG_MD, "reg is 0x%lx\n", ccu_base);

	vin_md_set_power(PWR_ON);

	sunxi_twi_init(0);
	sunxi_twi_init(1);

#if 0
	i = 0;
	global_sensors[sensor_id].sensor_core = find_sensor_func(global_sensors[sensor_id].sensor_name);
	if (global_sensors[sensor_id].sensor_core->sensor_test_i2c) {
		ret = global_sensors[i].sensor_core->sensor_test_i2c(i);
		if (ret)
			return -1;
	}
#endif
	//vin_probe(0);
	for (i = 0; i < select_video_num; i++) {
		if (global_video[i].used == 1) {

			sensor_id = global_video[i].rear_sensor;

			global_sensors[sensor_id].sensor_core = find_sensor_func(global_sensors[sensor_id].sensor_name);
			if (global_video[i].use_sensor_list)
				vin_detect_sensor_list(sensor_id);

			vin_set_from_partition(global_video[i].isp_sel, &ir_en);
			vin_probe(i);
			vinc = global_vinc[i];
			vinc->get_yuv_en = isp_get_cfg[clamp(vinc->isp_sel, 0, ISP_GET_CFG_NUM - 1)].get_yuv_en;

			vin_subdev_ccu_en(i, PWR_ON);

			vin_pipeline_set_mbus_config(i);
			for (j = 0; j < vinc->total_rx_ch; j++) {
				csic_isp_input_select(vinc->isp_sel/ISP_VIRT_NUM, vinc->isp_sel%ISP_VIRT_NUM + j, vinc->csi_sel, j);
			}
			csic_vipp_input_select(vinc->vipp_sel/VIPP_VIRT_NUM, vinc->isp_sel/ISP_VIRT_NUM, vinc->isp_tx_ch);

#ifdef CONFIG_ISP_FAST_CONVERGENCE
			if (global_sensors[sensor_id].sensor_core->s_ir_status) {
				if (ir_en)
					global_sensors[sensor_id].sensor_core->s_ir_status(sensor_id, IR_NIGHT);
				else
					global_sensors[sensor_id].sensor_core->s_ir_status(sensor_id, IR_DAY);
			}
#elif defined CONFIG_ISP_HARD_LIGHTADC || defined CONFIG_ISP_ONLY_HARD_LIGHTADC
			if (global_sensors[sensor_id].sensor_core->s_ir_status) {
				if (ir_en == 1)
					global_sensors[sensor_id].sensor_core->s_ir_status(sensor_id, IR_DAY);
				else if (ir_en == 2)
					global_sensors[sensor_id].sensor_core->s_ir_status(sensor_id, IR_NIGHT);
			}
#endif
			if (global_sensors[sensor_id].sensor_core->sensor_power)
				global_sensors[sensor_id].sensor_core->sensor_power(sensor_id, PWR_ON);
			if (global_sensors[sensor_id].sensor_core->sensor_g_format)
				global_sensors[sensor_id].sensor_core->sensor_g_format(sensor_id, vinc->isp_sel, i);
			ret = vin_s_stream(i, PWR_ON);
			if (ret) {
				sensor_enable = 0;
				vin_err("find not sensor\n");
			}

			vin_set_lightadc_from_partition(vinc->isp_sel, sensor_id, ir_en);
#ifdef CONFIG_NOT_FRAME_LOSS
			if (global_sensors[sensor_id].not_frame_loss_gpio != 0xffff) {
				hal_gpio_set_pull(global_sensors[sensor_id].not_frame_loss_gpio, GPIO_PULL_UP);
				hal_gpio_set_direction(global_sensors[sensor_id].not_frame_loss_gpio, GPIO_DIRECTION_INPUT);
			}
#endif
		}
	}

	//__csic_dump_regs(0x05830000, 0x300);

	for (i = 0; i < select_video_num; i++) {
		if (global_video[i].used == 1) {
			vinc = global_vinc[i];

			if (vinc->get_yuv_en) {
#ifdef CONFIG_NOT_FRAME_LOSS
				while (vinc->frame_cnt < (YUV_MEMRESERVE_SIZE / vinc->buffer_size - 1)) {
					if (global_sensors[vinc->rear_sensor].not_frame_loss_gpio != 0xffff) {
						hal_gpio_get_data(global_sensors[vinc->rear_sensor].not_frame_loss_gpio, &gpio_data);
						if(gpio_data) {
							vin_print("detect button up, start notify\n");
							break;
						}
					}
					vin_log(VIN_LOG_MD, "wait frame cnt\n");
					hal_usleep(500);
				}
#else
				while (vinc->frame_cnt != 2)
					hal_usleep(1000);
#endif
			}

			if (global_sensors[i].use_isp && sensor_enable) {
				while (sunxi_isp_ae_done(vinc->isp_sel, 8) != 0)
					hal_usleep(1000);
			}
			vin_print("close video%d\n", vinc->id);

			vin_s_stream(i, PWR_OFF);
			csic_isp_bridge_disable();
		}
	}

	for (i = 0; i < select_video_num; i++) {
		if (global_video[i].used == 1) {
			vinc = global_vinc[i];

			vin_free(i);
		}
	}
	//vin_free(0);

#ifndef CONFIG_KERNEL_FREERTOS
	openamp_init(); //wait rpmsg init
#endif

#ifdef CONFIG_VIN_WAIT_AMP_INIT
	if (vin_sem != NULL) {
		ret = hal_sem_timedwait(vin_sem, CONFIG_VIN_WAIT_AMP_INIT_SEM_TIMEOUT);
		if (ret != 0) {
        		vin_err("vin wait rpbuf init timeout.\n");
			return -1;
		}
	} else {
		vin_err("vin_sem is NULL.\n");
		return -1;
	}
#endif

	for (i = 0; i < select_video_num; i++) {
		if (global_video[i].used == 1) {
			vinc = &global_video[i]; //global_vinc is free

			sensor_id = vinc->mipi_sel;
#if defined CONFIG_ISP_FAST_CONVERGENCE || defined CONFIG_ISP_HARD_LIGHTADC
			if (global_sensors[sensor_id].sensor_core->sensor_g_switch_format)
				global_sensors[sensor_id].sensor_core->sensor_g_switch_format(sensor_id, vinc->isp_sel);
			sunxi_isp_update_server(vinc->isp_sel);
			if (global_sensors[sensor_id].sensor_core->s_switch)
				global_sensors[sensor_id].sensor_core->s_switch(sensor_id);
#endif
			sunxi_isp_reset_server(vinc->isp_sel);

			vin_set_to_partition(vinc->isp_sel);

			if (isp_get_cfg[clamp(vinc->isp_sel, 0, ISP_GET_CFG_NUM - 1)].sensor_deinit) {
				if (vinc->mipi_sel != 0xff)
					sunxi_mipi_subdev_s_stream(vinc->mipi_sel, vinc->id, 0);
				hal_usleep(120);
				if (global_sensors[sensor_id].sensor_core->s_stream)
					global_sensors[sensor_id].sensor_core->s_stream(sensor_id, vinc->isp_sel, 0);
				if (global_sensors[sensor_id].sensor_core->sensor_power)
					global_sensors[sensor_id].sensor_core->sensor_power(sensor_id, PWR_OFF);
				vin_print("sensor%d is close\n", sensor_id);
			}
		}
	}

	sunxi_twi_exit(0);
	sunxi_twi_exit(1);
#ifdef CONFIG_KERNEL_FREERTOS
	memheap_detach(&isp_mempool);
#else
	rt_memheap_detach(&isp_mempool);
#endif

	hal_writel(hal_readl(rtc_base + 0x100 + 0x4 * RTC_NUM) | 0x2, rtc_base + 0x100 + 0x4 * RTC_NUM);
	vin_print("set rtc%d is 0x%x\n", RTC_NUM, hal_readl(rtc_base + 0x100 + 0x4 * RTC_NUM));
#ifdef CONFIG_VIN_WAIT_AMP_INIT
	if(vin_sem != NULL) {
		hal_sem_delete(vin_sem);
		vin_sem = NULL;
	}
#endif
	vin_log(VIN_LOG_MD, "GoodBye CSI!\n");
	return ret;
}
FINSH_FUNCTION_EXPORT_ALIAS(csi_init, csi_init, rtthread vin run code);
#endif
