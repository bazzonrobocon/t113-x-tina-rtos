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

#include "../include/isp_ini_parse.h"
#include "../include/isp_manage.h"
#include "../include/isp_debug.h"
#include "../../../platform/platform_cfg.h"

#if (ISP_VERSION >= 600)
#if defined CONFIG_SENSOR_GC2053_MIPI || defined CONFIG_SENSOR_GC4663_MIPI || defined CONFIG_SENSOR_SC5336_MIPI || \
	defined CONFIG_SENSOR_IMX319_MIPI || defined CONFIG_SENSOR_SC035HGS_MIPI || defined CONFIG_SENSOR_SC3336_MIPI || \
	defined CONFIG_SENSOR_IMX335_MIPI || defined CONFIG_SENSOR_SC2336_MIPI

#ifdef CONFIG_SENSOR_GC4663_MIPI
#include "SENSOR_H/gc4663_mipi_default_ini_v853.h"
#include "SENSOR_H/gc4663_120fps_mipi_default_ini_v853.h"
#include "SENSOR_H/gc4663_120fps_mipi_linear_to_wdr_ini_v853.h"
//#include "SENSOR_H/gc4663_mipi_wdr_default_v853.h"
#include "SENSOR_H/gc4663_mipi_wdr_auto_ratio_v853.h"
#include "SENSOR_REG_H/gc4663_mipi_120fps_720p_day_reg.h"
#include "SENSOR_REG_H/gc4663_mipi_2560_1440_15fps_day_reg.h"
//#include "SENSOR_REG_H/gc4663_mipi_2560_1440_wdr_15fps_day_reg.h"
#include "SENSOR_REG_H/gc4663_mipi_2560_1440_wdr_auto_ratio_15fps_day_reg.h"
#endif // CONFIG_SENSOR_GC4663_MIPI

#ifdef CONFIG_SENSOR_GC2053_MIPI
#include "SENSOR_H/gc2053_120fps_mipi_default_ini_v853.h"
//#include "SENSOR_H/gc2053_mipi_default_ini_v853.h"
#include "SENSOR_H/gc2053_mipi_isp600_20231208_144940.h"
#include "SENSOR_H/gc2053_mipi_isp600_20231025_162813_353_dnr_gamma.h"
#include "SENSOR_REG_H/gc2053_mipi_120fps_480p_day_reg.h"
#include "SENSOR_REG_H/gc2053_mipi_isp600_20231208_144940_day_reg.h"
#include "SENSOR_REG_H/gc2053_mipi_isp600_20231025_162813_353_dnr_gamma_reg.h"
#endif //CONFIG_SENSOR_GC2053_MIPI

#ifdef CONFIG_SENSOR_SC2336_MIPI
#include "SENSOR_H/sc2336_120fps_mipi_default_ini_v853.h"
#include "SENSOR_H/sc2336_mipi_default_ini_v853.h"
#include "SENSOR_REG_H/sc2336_mipi_120fps_960_280_day_reg.h"
#include "SENSOR_REG_H/sc2336_mipi_1080p_day_reg.h"
#endif //CONFIG_SENSOR_SC2336_MIPI

#ifdef CONFIG_SENSOR_SC3336_MIPI
#include "SENSOR_H/sc3336_120fps_mipi_default_ini_v853.h"
#include "SENSOR_H/sc3336_mipi_default_ini_v853.h"
#include "SENSOR_REG_H/sc3336_mipi_120fps_1152_320_day_reg.h"
#include "SENSOR_REG_H/sc3336_mipi_2304_1296_day_reg.h"
#endif //CONFIG_SENSOR_SC3336_MIPI

#ifdef CONFIG_SENSOR_SC5336_MIPI
#include "SENSOR_H/sc5336_mipi_default_ini_v853.h"
#include "SENSOR_H/sc5336_130fps_mipi_default_ini_v853.h"
#include "SENSOR_REG_H/sc5336_mipi_2880_1620_day_reg.h"
#include "SENSOR_REG_H/sc5336_mipi_130fps_1440_400_day_reg.h"
#endif // CONFIG_SENSOR_SC5336_MIPI

#ifdef CONFIG_SENSOR_IMX335_MIPI
#include "SENSOR_REG_H/imx335_mipi_2592_1944_day_reg.h"
#include "SENSOR_REG_H/imx335_mipi_2592_1944_wdr_day_reg.h"
#include "SENSOR_H/imx335_mipi_default_ini_v853.h"
#include "SENSOR_H/imx335_mipi_wdr_default_v853.h"
#endif // CONFIG_SENSOR_IMX335_MIPI

#ifdef CONFIG_SENSOR_IMX319_MIPI
#include "SENSOR_H/imx319_mipi_default_ini_a523.h"
#endif // CONFIG_SENSOR_IMX319_MIPI

#ifdef CONFIG_SENSOR_SC035HGS_MIPI
#include "SENSOR_H/sc035hgs_mipi_isp601_20230626.h"
#endif // CONFIG_SENSOR_SC035HGS_MIPI

#else
#include "SENSOR_H/gc2053_mipi_default_ini_v853.h"
#endif
#endif //(ISP_VERSION >= 600)

unsigned int isp_cfg_log_param = ISP_LOG_CFG;

#define SIZE_OF_LSC_TBL     (12*768*2)
#define SIZE_OF_GAMMA_TBL   (5*1024*3*2)

struct isp_cfg_array cfg_arr[] = {
#if (ISP_VERSION >= 600)
#if defined CONFIG_SENSOR_GC2053_MIPI || defined CONFIG_SENSOR_GC4663_MIPI || defined CONFIG_SENSOR_SC5336_MIPI || \
	defined CONFIG_SENSOR_IMX319_MIPI || defined CONFIG_SENSOR_SC035HGS_MIPI || defined CONFIG_SENSOR_SC3336_MIPI || \
	defined CONFIG_SENSOR_IMX335_MIPI || defined CONFIG_SENSOR_SC2336_MIPI

#ifdef CONFIG_SENSOR_GC2053_MIPI
	//{"gc2053_mipi", "gc2053_mipi_default_ini_v853_day", 1920, 1088, 20, 0, 0, &gc2053_mipi_v853_isp_cfg},
	{"gc2053_mipi", "gc2053_mipi_isp600_20231208_144940", 1920, 1088, 20, 0, 0, &gc2053_mipi_v853_isp_cfg},
#ifdef CONFIG_ENABLE_AIISP
	{"gc2053_mipi", "gc2053_mipi_isp600_20231025_162813_353_dnr_gamma", 1920, 1088, 10, 0, 1, &gc2053_mipi_isp_cfg},
#else
	{"gc2053_mipi", "gc2053_mipi_default_ini_v853_night", 1920, 1088, 20, 0, 1, &gc2053_mipi_v853_isp_cfg},
#endif //CONFIG_ENABLE_AIISP
	{"gc2053_mipi", "gc2053_120fps_mipi_default_ini_v853_day", 640, 480, 120, 0, 0, &gc2053_mipi_120fps_v853_isp_cfg},
	{"gc2053_mipi", "gc2053_120fps_mipi_default_ini_v853_night", 640, 480, 120, 0, 1, &gc2053_mipi_120fps_v853_isp_cfg},
#endif //CONFIG_SENSOR_GC2053_MIPI

#ifdef CONFIG_SENSOR_GC4663_MIPI
	{"gc4663_mipi", "gc4663_mipi_default_ini_day", 2560, 1440, 15, 0, 0, &gc4663_mipi_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_mipi_default_ini_night", 2560, 1440, 15, 0, 1, &gc4663_mipi_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_mipi_wdr_v853_isp_cfg_day", 2560, 1440, 15, 1, 0, &gc4663_mipi_wdr_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_mipi_wdr_v853_isp_cfg_night", 2560, 1440, 15, 1, 1, &gc4663_mipi_wdr_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_120fps_mipi_default_ini_day", 1280, 720, 120, 0, 0, &gc4663_120fps_mipi_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_120fps_mipi_default_ini_night", 1280, 720, 120, 0, 1, &gc4663_120fps_mipi_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_120fps_linear_to_wdr_day", 1280, 720, 120, 1, 0, &gc4663_120fps_mipi_linear_to_wdr_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_120fps_linear_to_wdr_night", 1280, 720, 120, 1, 1, &gc4663_120fps_mipi_linear_to_wdr_v853_isp_cfg},
#endif // CONFIG_SENSOR_GC4663_MIPI

#ifdef CONFIG_SENSOR_SC2336_MIPI
	{"sc2336_mipi", "sc2336_120fps_mipi_default_ini_day", 960, 280, 120, 0, 0, &sc2336_mipi_120fps_isp_cfg},
	{"sc2336_mipi", "sc2336_120fps_mipi_default_ini_night", 960, 280, 120, 0, 1, &sc2336_mipi_120fps_isp_cfg},
	{"sc2336_mipi", "sc2336_mipi_default_ini_day", 1920, 1080, 20, 0, 0, &sc2336_mipi_isp_cfg},
	{"sc2336_mipi", "sc2336_mipi_default_ini_night", 1920, 1080, 20, 0, 1, &sc2336_mipi_isp_cfg},
#endif //CONFIG_SENSOR_SC2336_MIPI
#ifdef CONFIG_SENSOR_SC3336_MIPI
	{"sc3336_mipi", "sc3336_120fps_mipi_default_ini_day", 1152, 320, 120, 0, 0, &sc3336_mipi_120fps_isp_cfg},
	{"sc3336_mipi", "sc3336_120fps_mipi_default_ini_night", 1152, 320, 120, 0, 1, &sc3336_mipi_120fps_isp_cfg},
	{"sc3336_mipi", "sc3336_mipi_default_ini_day", 2304, 1296, 20, 0, 0, &sc3336_mipi_isp_cfg},
	{"sc3336_mipi", "sc3336_mipi_default_ini_night", 2304, 1296, 20, 0, 1, &sc3336_mipi_isp_cfg},
#endif //CONFIG_SENSOR_SC3336_MIPI

#ifdef CONFIG_SENSOR_SC5336_MIPI
	{"sc5336_mipi", "sc5336_130fps_mipi_default_ini_day", 1440, 400, 130, 0, 0, &sc5336_mipi_130fps_isp_cfg},
	{"sc5336_mipi", "sc5336_130fps_mipi_default_ini_night", 1440, 400, 130, 0, 1, &sc5336_mipi_130fps_isp_cfg},
	{"sc5336_mipi", "sc5336_mipi_default_ini_day", 2880, 1620, 20, 0, 0, &sc5336_mipi_isp_cfg},
	{"sc5336_mipi", "sc5336_mipi_default_ini_night", 2880, 1620, 20, 0, 1, &sc5336_mipi_isp_cfg},
#endif // CONFIG_SENSOR_SC5336_MIPI

#ifdef CONFIG_SENSOR_IMX335_MIPI
	{"imx335_mipi", "imx335_mipi_default_ini_v853", 2592, 1944, 25, 0, 0, &imx335_mipi_v853_isp_cfg},
	{"imx335_mipi", "imx335_mipi_wdr_default_v853", 2592, 1944, 25, 1, 0, &imx335_mipi_wdr_v853_isp_cfg},
#endif // CONFIG_SENSOR_IMX335_MIPI
#ifdef CONFIG_SENSOR_IMX319_MIPI
	{"imx319_mipi", "imx319_mipi_default_ini_a523", 3264, 2448, 30, 0, 0, &imx319_mipi_a523_isp_cfg},
#endif // CONFIG_SENSOR_IMX319_MIPI
#ifdef CONFIG_SENSOR_SC035HGS_MIPI
	{"sc035hgs_mipi", "sc035hgs_mipi_default_ini_a523", 640, 480, 120, 0, 0, &sc035hgs_mipi_isp_cfg},
#endif // CONFIG_SENSOR_SC035HGS_MIPI

#else
	{"gc2053_mipi", "gc2053_mipi_default_ini_v853", 1920, 1088, 20, 0, 0, &gc2053_mipi_v853_isp_cfg},
#endif
#endif //(ISP_VERSION >= 600)
};

int parser_ini_info(struct isp_param_config *param, char *isp_cfg_name, char *sensor_name,
			int w, int h, int fps, int wdr, int ir, int sync_mode, int isp_id)
{
	int i;
	struct isp_cfg_pt *cfg = NULL;

	//load header parameter
	for (i = 0; i < ARRAY_SIZE(cfg_arr); i++) {
		if (!strncmp(sensor_name, cfg_arr[i].sensor_name, 6) &&
		    (w == cfg_arr[i].width) && (h == cfg_arr[i].height) &&
		    (fps == cfg_arr[i].fps) && (wdr == cfg_arr[i].wdr) &&
		    (ir == cfg_arr[i].ir)) {
				cfg = cfg_arr[i].cfg;
				ISP_PRINT("find %s_%d_%d_%d_%d [%s] isp config\n", cfg_arr[i].sensor_name,
					cfg_arr[i].width, cfg_arr[i].height, cfg_arr[i].fps, cfg_arr[i].wdr, cfg_arr[i].isp_cfg_name);
				break;
		}
	}

	if (i == ARRAY_SIZE(cfg_arr)) {
		for (i = 0; i < ARRAY_SIZE(cfg_arr); i++) {
			if (!strncmp(sensor_name, cfg_arr[i].sensor_name, 6) && (wdr == cfg_arr[i].wdr)) {
				cfg = cfg_arr[i].cfg;
				ISP_WARN("cannot find %s_%d_%d_%d_%d_%d isp config, use %s_%d_%d_%d_%d_%d -> [%s]\n", sensor_name, w, h, fps, wdr, ir,
				         cfg_arr[i].sensor_name, cfg_arr[i].width, cfg_arr[i].height, cfg_arr[i].fps, cfg_arr[i].wdr,
				         cfg_arr[i].ir, cfg_arr[i].isp_cfg_name);
				break;
			}
		}
		if (i == ARRAY_SIZE(cfg_arr)) {
			for (i = 0; i < ARRAY_SIZE(cfg_arr); i++) {
				if (wdr == cfg_arr[i].wdr) {
					cfg = cfg_arr[i].cfg;
					ISP_WARN("cannot find %s_%d_%d_%d_%d_%d isp config, use %s_%d_%d_%d_%d_%d -> [%s]\n", sensor_name, w, h, fps, wdr, ir,
					         cfg_arr[i].sensor_name, cfg_arr[i].width, cfg_arr[i].height, cfg_arr[i].fps, cfg_arr[i].wdr,
					         cfg_arr[i].ir, cfg_arr[i].isp_cfg_name);
					break;
				}
			}
		}
		if (i == ARRAY_SIZE(cfg_arr)) {
			ISP_WARN("cannot find %s_%d_%d_%d_%d_%d isp config, use default config [%s]\n",
				sensor_name, w, h, fps, wdr, ir, cfg_arr[i-1].isp_cfg_name);
			cfg = cfg_arr[i-1].cfg;// use the last one
		}
	}

	if (cfg != NULL) {
		strcpy(isp_cfg_name, cfg_arr[i].isp_cfg_name);
		param->isp_test_settings = *cfg->isp_test_settings;
		param->isp_3a_settings = *cfg->isp_3a_settings;
		param->isp_iso_settings = *cfg->isp_iso_settings;
		param->isp_tunning_settings = *cfg->isp_tunning_settings;
	}

	return 0;
}

struct isp_reg_array reg_arr[] = {
#if (ISP_VERSION >= 600)
#ifdef CONFIG_SENSOR_GC2053_MIPI
	{"gc2053_mipi", "gc2053_mipi_120fps_480p_day_reg", 640, 480, 120, 0, 0, &gc2053_mipi_480p_isp_day_reg},
	{"gc2053_mipi", "gc2053_mipi_120fps_480p_night_reg", 640, 480, 120, 0, 1, &gc2053_mipi_480p_isp_day_reg},
	//{"gc2053_mipi", "gc2053_mipi_1080p_20fps_day_reg_day", 1920, 1088, 20, 0, 0, &gc2053_mipi_isp_day_reg},
	{"gc2053_mipi", "gc2053_mipi_isp600_20231208_144940_day_reg", 1920, 1088, 20, 0, 0, &gc2053_mipi_isp_day_reg},
#ifdef CONFIG_ENABLE_AIISP
	{"gc2053_mipi", "gc2053_mipi_isp600_20231025_162813_353_dnr_gamma_reg", 1920, 1088, 10, 0, 1, &gc2053_mipi_isp_aiisp_reg},
#else
	{"gc2053_mipi", "gc2053_mipi_1080p_20fps_day_reg_night", 1920, 1088, 20, 0, 1, &gc2053_mipi_isp_day_reg},
#endif //CONFIG_ENABLE_AIISP
#endif //CONFIG_SENSOR_GC2053_MIPI

#ifdef CONFIG_SENSOR_GC4663_MIPI
	{"gc4663_mipi", "gc4663_mipi_720p_120fps_day_reg", 1280, 720, 120, 0, 0, &gc4663_mipi_720p_isp_day_reg},
	{"gc4663_mipi", "gc4663_mipi_720p_120fps_night_reg", 1280, 720, 120, 0, 1, &gc4663_mipi_720p_isp_day_reg},
	{"gc4663_mipi", "gc4663_mipi_1440p_15fps_day_reg_day", 2560, 1440, 15, 0, 0, &gc4663_mipi_isp_day_reg},
	{"gc4663_mipi", "gc4663_mipi_1440p_15fps_day_reg_night", 2560, 1440, 15, 0, 1, &gc4663_mipi_isp_day_reg},
	{"gc4663_mipi", "gc4663_mipi_1440p_wdr_15fps_day_reg_day", 2560, 1440, 15, 1, 0, &gc4663_mipi_wdr_isp_day_reg},
	{"gc4663_mipi", "gc4663_mipi_1440p_wdr_15fps_day_reg_night", 2560, 1440, 15, 1, 1, &gc4663_mipi_wdr_isp_day_reg},
#endif //CONFIG_SENSOR_GC4663_MIPI

#ifdef CONFIG_SENSOR_SC2336_MIPI
	{"sc2336_mipi", "sc2336_mipi_120fps_960_280_day_reg", 960, 280, 120, 0, 0, &sc2336_mipi_960_280_isp_day_reg},
	{"sc2336_mipi", "sc2336_mipi_120fps_960_280_night_reg", 960, 280, 120, 0, 1, &sc2336_mipi_960_280_isp_day_reg},
	{"sc2336_mipi", "sc2336_mipi_1080p_day_reg_day", 1920, 1080, 20, 0, 0, &sc2336_mipi_isp_day_reg},
	{"sc2336_mipi", "sc2336_mipi_1080p_day_reg_night", 1920, 1080, 20, 0, 1, &sc2336_mipi_isp_day_reg},
#endif //CONFIG_SENSOR_SC2336_MIPI

#ifdef CONFIG_SENSOR_SC3336_MIPI
	{"sc3336_mipi", "sc3336_mipi_120fps_1152_320_day_reg", 1152, 320, 120, 0, 0, &sc3336_mipi_1152_320_isp_day_reg},
	{"sc3336_mipi", "sc3336_mipi_120fps_1152_320_night_reg", 1152, 320, 120, 0, 1, &sc3336_mipi_1152_320_isp_day_reg},
	{"sc3336_mipi", "sc3336_mipi_2304_1296_day_reg_day", 2304, 1296, 20, 0, 0, &sc3336_mipi_isp_day_reg},
	{"sc3336_mipi", "sc3336_mipi_2304_1296_day_reg_night", 2304, 1296, 20, 0, 1, &sc3336_mipi_isp_day_reg},
#endif //CONFIG_SENSOR_SC3336_MIPI

#ifdef CONFIG_SENSOR_SC5336_MIPI
	{"sc5336_mipi", "sc5336_mipi_130fps_1440_400_day_reg", 1440, 400, 130, 0, 0, &sc5336_mipi_1440_400_isp_day_reg},
	{"sc5336_mipi", "sc5336_mipi_130fps_1440_400_night_reg", 1440, 400, 130, 0, 1, &sc5336_mipi_1440_400_isp_day_reg},
	{"sc5336_mipi", "sc5336_mipi_2880_1620_day_reg_day", 2880, 1620, 20, 0, 0, &sc5336_mipi_isp_day_reg},
	{"sc5336_mipi", "sc5336_mipi_2880_1620_day_reg_night", 2880, 1620, 20, 0, 1, &sc5336_mipi_isp_day_reg},
#endif // CONFIG_SENSOR_SC5336_MIPI

#ifdef CONFIG_SENSOR_IMX335_MIPI
	{"imx335_mipi", "imx335_mipi_2592_1944_day_reg", 2592, 1944, 25, 0, 0, &imx335_mipi_isp_day_reg},
	{"imx335_mipi", "imx335_mipi_2592_1944_night_reg", 2592, 1944, 25, 0, 1, &imx335_mipi_isp_day_reg},
	{"imx335_mipi", "imx335_mipi_wdr_2592_1944_day_reg", 2592, 1944, 25, 1, 0, &imx335_mipi_wdr_isp_day_reg},
	{"imx335_mipi", "imx335_mipi_wdr_2592_1944_night_reg", 2592, 1944, 25, 1, 1, &imx335_mipi_wdr_isp_day_reg},
#endif //CONFIG_SENSOR_IMX335_MIPI
#endif
};

int parser_ini_regs_info(struct isp_lib_context *ctx, char *sensor_name,
			int w, int h, int fps, int wdr, int ir)
{
#ifdef CONFIG_ARCH_SUN20IW3
	int i;
	struct isp_reg_pt *reg = NULL;

	for (i = 0; i < ARRAY_SIZE(reg_arr); i++) {
		if (!strncmp(sensor_name, reg_arr[i].sensor_name, 6) &&
			(w == reg_arr[i].width) && (h == reg_arr[i].height) &&// (fps == reg_arr[i].fps) &&
			(wdr == reg_arr[i].wdr)) {

			if (reg_arr[i].ir == ir) {
				reg = reg_arr[i].reg;
				ISP_PRINT("find %s_%d_%d_%d_%d ---- [%s] isp reg\n", reg_arr[i].sensor_name,
					reg_arr[i].width, reg_arr[i].height, reg_arr[i].fps, reg_arr[i].wdr, reg_arr[i].isp_cfg_name);
				break;
			}
		}
	}

	if (i == ARRAY_SIZE(reg_arr)) {
		ISP_WARN("cannot find %s_%d_%d_%d_%d_%d isp reg!!!\n", sensor_name, w, h, fps, wdr, ir);
		return -1;
	}

	if (reg != NULL) {
		if (reg->isp_save_reg)
			memcpy(ctx->load_reg_base, reg->isp_save_reg, ISP_LOAD_REG_SIZE);
		if (reg->isp_save_fe_table)
			memcpy(ctx->module_cfg.fe_table, reg->isp_save_fe_table, ISP_FE_TABLE_SIZE);
		if (reg->isp_save_bayer_table)
			memcpy(ctx->module_cfg.bayer_table, reg->isp_save_bayer_table, ISP_BAYER_TABLE_SIZE);
		if (reg->isp_save_rgb_table)
			memcpy(ctx->module_cfg.rgb_table, reg->isp_save_rgb_table, ISP_RGB_TABLE_SIZE);
		if (reg->isp_save_yuv_table)
			memcpy(ctx->module_cfg.yuv_table, reg->isp_save_yuv_table, ISP_YUV_TABLE_SIZE);
	}
#else
	void *isp_save_reg, *isp_save_fe_table, *isp_save_bayer_table, *isp_save_rgb_table, *isp_save_yuv_table;

	isp_save_reg = (void *)ISP_PARA_READ;
	isp_save_fe_table = isp_save_reg + ISP_LOAD_REG_SIZE;
	isp_save_bayer_table = isp_save_fe_table + ISP_FE_TABLE_SIZE;
	isp_save_rgb_table = isp_save_bayer_table + ISP_BAYER_TABLE_SIZE;
	isp_save_yuv_table = isp_save_rgb_table + ISP_RGB_TABLE_SIZE;

	if (*((unsigned int *)isp_save_reg) != 0xAAEEBBEE) {
		ISP_WARN("cannot get reg&table from memory&reserved addr\n");
		return -1;
	} else
		ISP_PRINT("get reg&table from memory&reserved addr\n");

	memcpy(ctx->load_reg_base, isp_save_reg, ISP_LOAD_REG_SIZE);
	memcpy(ctx->module_cfg.fe_table, isp_save_fe_table, ISP_FE_TABLE_SIZE);
	memcpy(ctx->module_cfg.bayer_table, isp_save_bayer_table, ISP_BAYER_TABLE_SIZE);
	memcpy(ctx->module_cfg.rgb_table, isp_save_rgb_table, ISP_RGB_TABLE_SIZE);
	memcpy(ctx->module_cfg.yuv_table, isp_save_yuv_table, ISP_YUV_TABLE_SIZE);
#endif
	return 0;
}
