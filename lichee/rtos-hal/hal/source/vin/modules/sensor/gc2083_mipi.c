/*
 * A V4L2 driver for Raw cameras.
 *
 * Copyright (c) 2017 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zhao Wei <zhaowei@allwinnertech.com>
 *    Liang WeiJie <liangweijie@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <hal_timer.h>
#include "../../vin_mipi/combo_common.h"
#include "camera.h"
#include "../../utility/sunxi_camera_v2.h"
#include "../../utility/media-bus-format.h"
#include "../../utility/vin_supply.h"

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR  0x2083

/*
 * Our nominal (default) frame rate.
 */
#define ID_REG_HIGH		0x03f0
#define ID_REG_LOW		0x03f1
#define ID_VAL_HIGH		((V4L2_IDENT_SENSOR) >> 8)
#define ID_VAL_LOW		((V4L2_IDENT_SENSOR) & 0xff)
#define SENSOR_FRAME_RATE 20

/*
 * The GC2083 i2c address
 */
#define I2C_ADDR 0x6e

#define SENSOR_NUM 0x2
#define SENSOR_NAME "gc2083_mipi"
#define SENSOR_NAME_2 "gc2083_mipi_2"

static int sensor_power_count[2];
static int sensor_stream_count[2];
static struct sensor_format_struct *current_win[2];
static struct sensor_format_struct *current_switch_win[2];

#define GC2083_1920X1088_20FPS 1
#define GC2083_1920X1088_15FPS 0

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};


#if defined CONFIG_ISP_READ_THRESHOLD || defined CONFIG_ISP_ONLY_HARD_LIGHTADC//FULL_SIZE
/*
 * window_size=1920*1080 mipi@2lane
 * mclk=24mhz, mipi_clk=594Mbps
 * pixel_line_total=2640, line_frame_total=1125
 * row_time=35.55us, frame_rate=25fps
 */
#if GC2083_1920X1088_20FPS
static struct regval_list sensor_1080p20_regs[] = {
//version 1.0
//mclk 27Mhz
//mipiclk 594Mbps/lane
//framelength 1688
//framrate 30fps
//rowtime 29.616162us
//pattern rggb
	/****system****/
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x24},
	{0x03f7, 0x01},
	{0x03f8, 0x2c},
	{0x03f9, 0x43},
	{0x03fc, 0x8e},
	{0x0381, 0x07},
	{0x00d7, 0x29},
	/****CISCTL & ANALOG****/
	{0x0d6d, 0x18},
	{0x00d5, 0x03},
	{0x0082, 0x01},
	{0x0db3, 0xd4},
	{0x0db0, 0x0d},
	{0x0db5, 0x96},
	{0x0d03, 0x02},
	{0x0d04, 0x02},
	{0x0d05, 0x05},
	{0x0d06, 0xba},
	{0x0d07, 0x00},
	{0x0d08, 0x11},
	{0x0d09, 0x00},
	{0x0d0a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x02},
	{0x0d0d, 0x04},
	{0x0d0e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x90},
	{0x0017, 0x0c},
	{0x0d73, 0x92},
	{0x0076, 0x00},
	{0x0d76, 0x00},
	{0x0d41, 0x06},
	{0x0d42, 0x98},

	{0x0d7a, 0x10},
	{0x0d19, 0x31},
	{0x0d25, 0xcb},
	{0x0d20, 0x60},
	{0x0d27, 0x03},
	{0x0d29, 0x60},
	{0x0d43, 0x10},
	{0x0d49, 0x10},
	{0x0d55, 0x18},
	{0x0dc2, 0x44},
	{0x0058, 0x3c},
	{0x00d8, 0x68},
	{0x00d9, 0x14},
	{0x00da, 0xc1},
	{0x0050, 0x18},
	{0x0db6, 0x3d},
	{0x00d2, 0xbc},
	{0x0d66, 0x42},
	{0x008c, 0x07},
	{0x008d, 0xff},

	/*gain*/
	{0x007a, 0x50}, //global gain
	{0x00d0, 0x00},
	{0x0dc1, 0x00},

	/*isp*/
	{0x0102, 0xa9}, //89
	{0x0158, 0x00},
	{0x0107, 0xa6},
	{0x0108, 0xa9},
	{0x0109, 0xa8},
	{0x010a, 0xa7},
	{0x010b, 0xff},
	{0x010c, 0xff},

	{0x0428, 0x86}, //84 edge_th
	{0x0429, 0x86}, //84
	{0x042a, 0x86}, //84
	{0x042b, 0x68}, //84
	{0x042c, 0x68}, //84
	{0x042d, 0x68}, //84
	{0x042e, 0x68}, //83
	{0x042f, 0x68}, //82

	{0x0430, 0x4f}, //82 noise_th
	{0x0431, 0x68}, //82
	{0x0432, 0x67}, //82
	{0x0433, 0x66}, //82
	{0x0434, 0x66}, //82
	{0x0435, 0x66}, //82
	{0x0436, 0x66}, //64
	{0x0437, 0x66}, //68

	{0x0438, 0x62},
	{0x0439, 0x62},
	{0x043a, 0x62},
	{0x043b, 0x62},
	{0x043c, 0x62},
	{0x043d, 0x62},
	{0x043e, 0x62},
	{0x043f, 0x62},

	/*dark sun*/
	{0x0077, 0x01},
	{0x0078, 0x65},
	{0x0079, 0x04},
	{0x0067, 0xa0},
	{0x0054, 0xff},
	{0x0055, 0x02},
	{0x0056, 0x00},
	{0x0057, 0x04},
	{0x005a, 0xff},
	{0x005b, 0x07},

	/*blk*/
	{0x0026, 0x01},
	{0x0152, 0x02},
	{0x0153, 0x50},
	{0x0155, 0x93},
	{0x0410, 0x16},
	{0x0411, 0x16},
	{0x0412, 0x16},
	{0x0413, 0x16},
	{0x0414, 0x6f},
	{0x0415, 0x6f},
	{0x0416, 0x6f},
	{0x0417, 0x6f},
	{0x04e0, 0x18},

	/*window*/
	{0x0192, 0x00},
	{0x0194, 0x00},
	{0x0195, 0x04},
	{0x0196, 0x40},
	{0x0197, 0x07},
	{0x0198, 0x80},

	/****DVP & MIPI****/
	{0x0201, 0x27},
	{0x0202, 0x53},
	{0x0203, 0xce},
	{0x0204, 0x40},
	{0x0212, 0x07},
	{0x0213, 0x80},
	{0x0215, 0x10},
	{0x0229, 0x05},
	{0x0237, 0x03},
	//{0x023e, 0x99}
};
#endif

#if GC2083_1920X1088_15FPS
static struct regval_list sensor_1080p15_regs[] = {
//version 1.0
//mclk 27Mhz
//mipiclk 297Mbps/lane
//framelength 1125
//framrate 30fps
//rowtime 29.616162us
//pattern rggb
//window size:1920x1080
//mclk=24mhz,mipi_rate=318Mbps/lane
//rowtime=49.36us
//frame rate:15fps,
//vts=1350,
//bayer mode:RGGB
	/****system****/
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x24},
	{0x03f7, 0x01},
	{0x03f8, 0x35},
	{0x03f9, 0x41},
	{0x03fc, 0x8e},
	{0x0381, 0x07},
	{0x00d7, 0x29},
	/****CISCTL & ANALOG****/
	{0x0d6d, 0x18},
	{0x00d5, 0x03},
	{0x0082, 0x01},
	{0x0db3, 0xd4},
	{0x0db0, 0x0d},
	{0x0db5, 0x96},
	{0x0d03, 0x02},
	{0x0d04, 0x02},
	{0x0d05, 0x05},
	{0x0d06, 0x1c},
	{0x0d07, 0x00},
	{0x0d08, 0x11},
	{0x0d09, 0x00},
	{0x0d0a, 0x02},
	{0x000b, 0x00},
	{0x000c, 0x02},
	{0x0d0d, 0x04},
	{0x0d0e, 0x40},
	{0x000f, 0x07},
	{0x0010, 0x90},
	{0x0017, 0x0c},
	{0x0d73, 0x92},
	{0x0076, 0x00},
	{0x0d76, 0x00},
	{0x0d41, 0x05},
	{0x0d42, 0x46},

	{0x0d7a, 0x10},
	{0x0d19, 0x31},
	{0x0d25, 0x0b},
	{0x0d20, 0x60},
	{0x0d27, 0x03},
	{0x0d29, 0x60},
	{0x0d43, 0x10},
	{0x0d49, 0x10},
	{0x0d55, 0x18},
	{0x0dc2, 0x44},
	{0x0058, 0x3c},
	{0x00d8, 0x68},
	{0x00d9, 0x14},
	{0x00da, 0xc1},
	{0x0050, 0x18},
	{0x0db6, 0x3d},
	{0x00d2, 0xbc},
	{0x0d66, 0x42},
	{0x008c, 0x05},
	{0x008d, 0xff},

	/*gain*/
	{0x007a, 0x50}, //global gain
	{0x00d0, 0x00},
	{0x0dc1, 0x00},

	/*isp*/
	{0x0102, 0xa9}, //89
	{0x0158, 0x00},
	{0x0107, 0xa6},
	{0x0108, 0xa9},
	{0x0109, 0xa8},
	{0x010a, 0xa7},
	{0x010b, 0xff},
	{0x010c, 0xff},

	{0x0428, 0x86}, //84 edge_th
	{0x0429, 0x86}, //84
	{0x042a, 0x86}, //84
	{0x042b, 0x68}, //84
	{0x042c, 0x68}, //84
	{0x042d, 0x68}, //84
	{0x042e, 0x68}, //83
	{0x042f, 0x68}, //82

	{0x0430, 0x4f}, //82 noise_th
	{0x0431, 0x68}, //82
	{0x0432, 0x67}, //82
	{0x0433, 0x66}, //82
	{0x0434, 0x66}, //82
	{0x0435, 0x66}, //82
	{0x0436, 0x66}, //64
	{0x0437, 0x66}, //68

	{0x0438, 0x62},
	{0x0439, 0x62},
	{0x043a, 0x62},
	{0x043b, 0x62},
	{0x043c, 0x62},
	{0x043d, 0x62},
	{0x043e, 0x62},
	{0x043f, 0x62},

	/*dark sun*/
	{0x0077, 0x01},
	{0x0078, 0x65},
	{0x0079, 0x04},
	{0x0067, 0xa0},
	{0x0054, 0xff},
	{0x0055, 0x02},
	{0x0056, 0x00},
	{0x0057, 0x04},
	{0x005a, 0xff},
	{0x005b, 0x07},

	/*blk*/
	{0x0026, 0x01},
	{0x0152, 0x02},
	{0x0153, 0x50},
	{0x0155, 0x93},
	{0x0410, 0x16},
	{0x0411, 0x16},
	{0x0412, 0x16},
	{0x0413, 0x16},
	{0x0414, 0x6f},
	{0x0415, 0x6f},
	{0x0416, 0x6f},
	{0x0417, 0x6f},
	{0x04e0, 0x18},

	/*window*/
	{0x0192, 0x00},
	{0x0194, 0x04},
	{0x0195, 0x04},
	{0x0196, 0x40},
	{0x0197, 0x07},
	{0x0198, 0x80},

	/****DVP & MIPI****/
	{0x0201, 0x27},
	{0x0202, 0x53},
	{0x0203, 0xce},
	{0x0204, 0x40},
	{0x0212, 0x07},
	{0x0213, 0x80},
	{0x0215, 0x10},
	{0x0229, 0x05},
	{0x0237, 0x03},
	//{0x023e, 0x99}
};
#endif

#else //CONFIG_ISP_FAST_CONVERGENCE || CONFIG_ISP_HARD_LIGHTADC
static struct regval_list sensor_1920x360p120_regs[] = {
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0xf0},
	{0x03fe, 0x00},
	{0x03f2, 0x00},
	{0x03f3, 0x00},
	{0x03f4, 0x36},
	{0x03f5, 0xc0},
	{0x03f6, 0x14},
	{0x03f7, 0x01},
	{0x03f8, 0x30},
	{0x03f9, 0x43},
	{0x03fc, 0x8e},
	{0x0381, 0x07},
	{0x00d7, 0x29},
	{0x0d6d, 0x18},
	{0x00d5, 0x03},
	{0x0082, 0x01},
	{0x0db3, 0xd4},
	{0x0db0, 0x0d},
	{0x0db5, 0x96},
	{0x0d03, 0x01},
	{0x0d04, 0x51},
	{0x0d05, 0x04},
	{0x0d06, 0xd0},
	{0x0d07, 0x00},
	{0x0d08, 0x11},
	{0x0d09, 0x01},
	{0x0d0a, 0x68},
	{0x000b, 0x00},
	{0x000c, 0x02},
	{0x0d0d, 0x01},
	{0x0d0e, 0x70},//368
	{0x000f, 0x07},
	{0x0010, 0x90},
	{0x0017, 0x0c},
	{0x0d73, 0x92},
	{0x0076, 0x00},
	{0x0d76, 0x00},
	{0x0d41, 0x01},
	{0x0d42, 0x85},//780
	{0x0d7a, 0x10},
	{0x0d19, 0x31},
	{0x0d25, 0xcb},
	{0x0d20, 0x60},
	{0x0d27, 0x03},
	{0x0d29, 0x60},
	{0x0d43, 0x10},
	{0x0d49, 0x10},
	{0x0d55, 0x18},
	{0x0dc2, 0x44},
	{0x0058, 0x3c},
	{0x00d8, 0x68},
	{0x00d9, 0x14},
	{0x00da, 0xc1},
	{0x0050, 0x18},
	{0x0db6, 0x3d},
	{0x00d2, 0xbc},
	{0x0d66, 0x42},
	{0x008c, 0x03},//05
	{0x008d, 0xd8},//ff
	{0x007a, 0x68},
	{0x00d0, 0x00},
	{0x0dc1, 0x00},
	{0x0102, 0xa9},
	{0x0158, 0x00},
	{0x0107, 0xa6},
	{0x0108, 0xa9},
	{0x0109, 0xa8},
	{0x010a, 0xa7},
	{0x010b, 0xff},
	{0x010c, 0xff},
	{0x0428, 0x86},
	{0x0429, 0x86},
	{0x042a, 0x86},
	{0x042b, 0x68},
	{0x042c, 0x68},
	{0x042d, 0x68},
	{0x042e, 0x68},
	{0x042f, 0x68},
	{0x0430, 0x4f},
	{0x0431, 0x68},
	{0x0432, 0x67},
	{0x0433, 0x66},
	{0x0434, 0x66},
	{0x0435, 0x66},
	{0x0436, 0x66},
	{0x0437, 0x66},
	{0x0438, 0x62},
	{0x0439, 0x62},
	{0x043a, 0x62},
	{0x043b, 0x62},
	{0x043c, 0x62},
	{0x043d, 0x62},
	{0x043e, 0x62},
	{0x043f, 0x62},
	{0x0077, 0x01},
	{0x0078, 0x65},
	{0x0079, 0x04},
	{0x0067, 0xa0},
	{0x0054, 0xff},
	{0x0055, 0x02},
	{0x0056, 0x00},
	{0x0057, 0x04},
	{0x005a, 0xff},
	{0x005b, 0x07},
	{0x0026, 0x01},
	{0x0152, 0x02},
	{0x0153, 0x50},
	{0x0155, 0x93},
	{0x0410, 0x16},
	{0x0411, 0x16},
	{0x0412, 0x16},
	{0x0413, 0x16},
	{0x0414, 0x6f},
	{0x0415, 0x6f},
	{0x0416, 0x6f},
	{0x0417, 0x6f},
	{0x04e0, 0x18},
	{0x0192, 0x04},
	{0x0193, 0x00},
	{0x0194, 0x04},
	{0x0195, 0x01},
	{0x0196, 0x68},
	{0x0197, 0x07},
	{0x0198, 0x80},
	{0x0201, 0x27},
	{0x0202, 0x53}, //0x50
	{0x0203, 0xce}, //0xb6//0x8e
	{0x0204, 0x40},
	{0x0212, 0x07},
	{0x0213, 0x80},
	{0x0215, 0x12},
	{0x0229, 0x05},
	{0x0237, 0x03},
	//{0x023e, 0x99},
};

#if GC2083_1920X1088_20FPS
static struct regval_list sensor_1920x360p120fps_to_1080p20fps[] = {
	{0x023e, 0x00},

	{0x03f6, 0x24},
	{0x03f8, 0x2c},

	//{0x0d03, 0x02},
	//{0x0d04, 0x02},
	{0x0d05, 0x05},
	{0x0d06, 0xba},
	{0x0d09, 0x00},
	{0x0d0a, 0x02},

	{0x0d0d, 0x04},
	{0x0d0e, 0x40},

	{0x0d41, 0x06},
	{0x0d42, 0x98},

	{0x008c, 0x07},
	{0x008d, 0xff},

	{0x007a, 0x50},

	{0x0192, 0x00},
	{0x0193, 0x00},
	{0x0194, 0x00},
	{0x0195, 0x04},
	{0x0196, 0x40},

	{0x0215, 0x10},

	{0x03fe, 0x10},
	{0x03fe, 0x00},

	{0x023e, 0x99},
};
#endif

#if GC2083_1920X1088_15FPS
static struct regval_list sensor_1920x360p120fps_to_1080p15fps[] = {
	{0x023e, 0x00},

	{0x03f6, 0x24},
	{0x03f8, 0x35},
	{0x03f9, 0x41},

	//{0x0d03, 0x02},
	//{0x0d04, 0x02},
	{0x0d05, 0x05},
	{0x0d06, 0x1c},
	{0x0d09, 0x00},
	{0x0d0a, 0x02},
	{0x0d0d, 0x04},
	{0x0d0e, 0x40},
	{0x0d41, 0x05},
	{0x0d42, 0x46},

	{0x0d25, 0x0b},

	{0x008c, 0x05},
	{0x008d, 0xff},

	{0x007a, 0x50},

	{0x0192, 0x00},
	{0x0193, 0x00},
	{0x0194, 0x04},
	{0x0195, 0x04},
	{0x0196, 0x40},

	{0x0215, 0x10},

	{0x03fe, 0x10},
	{0x03fe, 0x00},

	{0x023e, 0x99},
};
#endif
#endif
/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {

};


/*
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function , retrun -EINVAL
 */
#if 0
static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}
static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->gain;
	sensor_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}
#endif

static int sensor_s_exp(int id, unsigned int exp_val)
{
	int tmp_exp_val = exp_val / 16;

	sensor_dbg("exp_val:%d\n", exp_val);
	sensor_write(id, 0x0d03, (tmp_exp_val >> 8) & 0xFF);
	sensor_write(id, 0x0d04, (tmp_exp_val & 0xFF));

	return 0;
}

unsigned char gc2083_regValTable[29][13] = {
	//0x00d0 0x0155 0x0410 0x0411 0x0412 0x0413 0x0414 0x0415 0x0416 0x0417 0x00b8 0x00b9 0x0dc1
	{0x00, 0x00, 0x01, 0x00, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x10, 0x00, 0x01, 0x0c, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x01, 0x00, 0x01, 0x1a, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x11, 0x00, 0x01, 0x2b, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x02, 0x00, 0x02, 0x00, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x12, 0x00, 0x02, 0x18, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x03, 0x00, 0x02, 0x33, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x13, 0x00, 0x03, 0x15, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x04, 0x00, 0x04, 0x00, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x14, 0x00, 0x04, 0xe0, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x05, 0x00, 0x05, 0x26, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x15, 0x00, 0x06, 0x2b, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x44, 0x00, 0x08, 0x00, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x54, 0x00, 0x09, 0x22, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x45, 0x00, 0x0b, 0x0d, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x55, 0x00, 0x0d, 0x16, 0x03, 0x11, 0x11, 0x11, 0x11, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x04, 0x01, 0x10, 0x00, 0x19, 0x16, 0x16, 0x16, 0x16, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x14, 0x01, 0x13, 0x04, 0x19, 0x16, 0x16, 0x16, 0x16, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x24, 0x01, 0x16, 0x1a, 0x19, 0x16, 0x16, 0x16, 0x16, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x34, 0x01, 0x1a, 0x2b, 0x19, 0x16, 0x16, 0x16, 0x16, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x44, 0x01, 0x20, 0x00, 0x36, 0x18, 0x18, 0x18, 0x18, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x54, 0x01, 0x26, 0x07, 0x36, 0x18, 0x18, 0x18, 0x18, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x64, 0x01, 0x2c, 0x33, 0x36, 0x18, 0x18, 0x18, 0x18, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x74, 0x01, 0x35, 0x17, 0x36, 0x18, 0x18, 0x18, 0x18, 0x6f, 0x6f, 0x6f, 0x6f},
	{0x84, 0x01, 0x35, 0x17, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72},
	{0x94, 0x01, 0x35, 0x17, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72},
	{0x85, 0x01, 0x35, 0x17, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72},
	{0x95, 0x01, 0x35, 0x17, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72},
	{0xa5, 0x01, 0x35, 0x17, 0x64, 0x16, 0x16, 0x16, 0x16, 0x72, 0x72, 0x72, 0x72},
};

unsigned int gc2083_gainLevelTable[29] = {
	 64,   77,   92,   110,  128,        154,
	186,   223,  269,  323,  381,        457,
	544,   653,  762,  914, 1078,       1293,
	1541, 1849, 2177, 2612, 3136,       3764,
	4710, 5652, 6656, 7988, 9474
};

static int total = sizeof(gc2083_gainLevelTable) / sizeof(unsigned int);
static int setSensorGain(int id, int gain)
{
	int i;
	unsigned int tol_dig_gain = 0;

	for(i = 0; (total > 1) && (i < (total - 1)); i++)
	{
		if((gc2083_gainLevelTable[i] <= gain) && (gain < gc2083_gainLevelTable[i+1]))
			break;
	}

	tol_dig_gain = (gain * 64) / gc2083_gainLevelTable[i];

	//if(tol_dig_gain>15*64) tol_dig_gain=15;

	sensor_write(id, 0x00d0, gc2083_regValTable[i][0]);
	sensor_write(id, 0x031d, 0x2e);
	sensor_write(id, 0x0dc1, gc2083_regValTable[i][1]);
	sensor_write(id, 0x031d, 0x28);
	sensor_write(id, 0x00b8, gc2083_regValTable[i][2]);
	sensor_write(id, 0x00b9, gc2083_regValTable[i][3]);
	sensor_write(id, 0x0155, gc2083_regValTable[i][4]);
	sensor_write(id, 0x0410, gc2083_regValTable[i][5]);
	sensor_write(id, 0x0411, gc2083_regValTable[i][6]);
	sensor_write(id, 0x0412, gc2083_regValTable[i][7]);
	sensor_write(id, 0x0413, gc2083_regValTable[i][8]);
	sensor_write(id, 0x0414, gc2083_regValTable[i][9]);
	sensor_write(id, 0x0415, gc2083_regValTable[i][10]);
	sensor_write(id, 0x0416, gc2083_regValTable[i][11]);

	sensor_write(id, 0x0417, gc2083_regValTable[i][12]);

	sensor_write(id, 0x00b1, (tol_dig_gain >> 6));
	sensor_write(id, 0x00b2, ((tol_dig_gain & 0x3f) << 2));

	return 0;
}

static int sensor_s_gain(int id, int gain_val)
{
	sensor_dbg("gain_val:%d\n", gain_val);
	setSensorGain(id, gain_val * 4);

	return 0;
}

static int gc2083_sensor_vts;
static int sensor_s_exp_gain(int id, struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val;
	int shutter = 0, frame_length = 0;

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	if (gain_val < (1 * 16)) {
		gain_val = 16;
	}

	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	shutter = exp_val >> 4;
	if (shutter > gc2083_sensor_vts - 16)
		frame_length = shutter + 16;
	else
		frame_length = gc2083_sensor_vts;
	sensor_dbg("frame_length = %d\n", frame_length);
	sensor_write(id, 0x0d42, frame_length & 0xff);
	sensor_write(id, 0x0d41, frame_length >> 8);

	sensor_s_exp(id, exp_val);
	sensor_s_gain(id, gain_val);

	sensor_print("gain_val:%d, exp_val:%d\n", gain_val, exp_val);

	return 0;
}

/*
* Stuff that knows about the sensor.
*/
static int sensor_power(int id, int on)
{
	if (on && (sensor_power_count[id])++ > 0)
		return 0;
	else if (!on && (sensor_power_count[id] == 0 || --(sensor_power_count[id]) > 0))
		return 0;

	switch (on) {
	case PWR_ON:
		sensor_dbg("PWR_ON!\n");
		vin_set_mclk_freq(id, MCLK);
		vin_set_mclk(id, 1);
		hal_usleep(1000);
		vin_gpio_set_status(id, PWDN, 1);
		vin_gpio_set_status(id, RESET, 1);
		//vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(id, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(id, RESET, CSI_GPIO_LOW);
		hal_usleep(1000);
		vin_gpio_write(id, PWDN, CSI_GPIO_HIGH);
		vin_gpio_write(id, RESET, CSI_GPIO_HIGH);
		hal_usleep(1000);
		break;
	case PWR_OFF:
		sensor_dbg("PWR_OFF!do nothing\n");
		vin_set_mclk(id, 0);
		hal_usleep(1000);
		vin_gpio_set_status(id, PWDN, 1);
		vin_gpio_set_status(id, RESET, 1);
		vin_gpio_write(id, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(id, RESET, CSI_GPIO_LOW);
		break;
	default:
		return -EINVAL;
	}

return 0;
}

static int sensor_set_ir(int id, int status)
{
	// hal_usleep(1000*1000);
	// rt_kprintf("sensor_set_ir %d\n",status);
	// return 0;
	vin_gpio_set_status(id, IR_CUT0, 1);
	vin_gpio_set_status(id, IR_CUT1, 1);
	vin_gpio_set_status(id, IR_LED, 1);
	switch (status) {
	case IR_DAY:
		vin_gpio_write(id, IR_CUT0, CSI_GPIO_HIGH);
		vin_gpio_write(id, IR_CUT1, CSI_GPIO_LOW);
		vin_gpio_write(id, IR_LED, CSI_GPIO_LOW);
		break;
	case IR_NIGHT:
		vin_gpio_write(id, IR_CUT0, CSI_GPIO_LOW);
		vin_gpio_write(id, IR_CUT1, CSI_GPIO_HIGH);
		vin_gpio_write(id, IR_LED, CSI_GPIO_HIGH);
		break;
	default:
		return -1;
	}

	return 0;
}

#if 0
static int sensor_reset(int id, u32 val)
{

	sensor_dbg("%s: val=%d\n", __func__);
	switch (val) {
	case 0:
		vin_gpio_write(id, RESET, CSI_GPIO_HIGH);
		hal_usleep(1000);
		break;
	case 1:
		vin_gpio_write(id, RESET, CSI_GPIO_LOW);
		hal_usleep(1000);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif

static int sensor_detect(int id)
{
	data_type rdval;
	/*int eRet;
	int times_out = 3;
	do {
		eRet = sensor_read(id, ID_REG_HIGH, &rdval);
		sensor_dbg("eRet:%d, ID_VAL_HIGH:0x%x, times_out:%d\n", eRet, rdval, times_out);
		hal_usleep(200);
		times_out--;
	} while (eRet < 0  &&  times_out > 0);*/

	sensor_read(id, ID_REG_HIGH, &rdval);
	sensor_dbg("ID_VAL_HIGH = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_HIGH)
		return -ENODEV;

	sensor_read(id, ID_REG_LOW, &rdval);
	sensor_dbg("ID_VAL_LOW = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_LOW)
		return -ENODEV;

	sensor_dbg("Done!\n");
	return 0;
}

static int sensor_init(int id)
{
	int ret;

	sensor_dbg("sensor_init\n");

	/*Make sure it is a target sensor */
	ret = sensor_detect(id);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	return 0;
}

/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
#if defined CONFIG_ISP_READ_THRESHOLD || defined CONFIG_ISP_ONLY_HARD_LIGHTADC // FULL_SIZE
#if GC2083_1920X1088_20FPS
	{
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width      = 1920,
		.height     = 1088,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2932,
		.vts        = 1688,
		.pclk       = 98955000,
		.mipi_bps   = 594 * 1000 * 1000,
		.fps_fixed  = 20,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1688 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.offs_h     = 0,
		.offs_v     = 0,
		.regs	    = sensor_1080p20_regs,
		.regs_size  = ARRAY_SIZE(sensor_1080p20_regs),
	},
#endif
#if GC2083_1920X1088_15FPS
	{
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width      = 1920,
		.height     = 1088,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 3140,
		.vts        = 1350,
		.pclk       = 63600000,
		.mipi_bps   = 318 * 1000 * 1000,
		.fps_fixed  = 15,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1350 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.offs_h     = 0,
		.offs_v     = 0,
		.regs	    = sensor_1080p15_regs,
		.regs_size  = ARRAY_SIZE(sensor_1080p15_regs),
	},
#endif

#else //CONFIG_ISP_FAST_CONVERGENCE || CONFIG_ISP_HARD_LIGHTADC
	{
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width      = 1920,
		.height     = 360,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2464,
		.vts        = 389,
		.pclk       = 115019520,
		.mipi_bps   = 576 * 1000 * 1000,
		.fps_fixed  = 120,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (389 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.offs_h     = 0,
		.offs_v     = 0,
		.regs	    = sensor_1920x360p120_regs,
		.regs_size  = ARRAY_SIZE(sensor_1920x360p120_regs),
	}
#endif
};

static struct sensor_format_struct *sensor_get_format(int id, int isp_id, int vinc_id)
{
#if defined CONFIG_ISP_READ_THRESHOLD || defined CONFIG_ISP_ONLY_HARD_LIGHTADC
	int ispid = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);
	struct sensor_format_struct *sensor_format = NULL;
	int wdr_on = isp_get_cfg[ispid].sensor_wdr_on;
	int fps = isp_get_cfg[ispid].sensor_get_fps;
	int i;

	if (current_win[id])
		return current_win[id];

	for (i = 0; i < ARRAY_SIZE(sensor_formats); i++) {
		if (sensor_formats[i].wdr_mode == wdr_on) {
			if (sensor_formats[i].fps_fixed == fps) {
				sensor_format = &sensor_formats[i];
				sensor_print("fine wdr is %d, fine fps is %d\n", wdr_on, fps);
				goto done;
			}
		}
	}

	if (sensor_format == NULL) {
		for (i = 0; i < ARRAY_SIZE(sensor_formats); i++) {
			if (sensor_formats[i].wdr_mode == wdr_on) {
				sensor_format = &sensor_formats[i];
				isp_get_cfg[ispid].sensor_get_fps = sensor_format->fps_fixed;
				sensor_print("fine wdr is %d, use fps is %d\n", wdr_on, sensor_format->fps_fixed);
				goto done;
			}
		}
	}

	if (sensor_format == NULL) {
		sensor_format = &sensor_formats[0];
		isp_get_cfg[ispid].sensor_wdr_on = sensor_format->wdr_mode;
		isp_get_cfg[ispid].sensor_get_fps = sensor_format->fps_fixed;
		sensor_print("use wdr is %d, use fps is %d\n", sensor_format->wdr_mode, sensor_format->fps_fixed);
	}

done:
	current_win[id] = sensor_format;
	return sensor_format;
#else //CONFIG_ISP_FAST_CONVERGENCE || CONFIG_ISP_HARD_LIGHTADC
	if (current_win[id])
		return current_win[id];

	current_win[id] = &sensor_formats[0];
	sensor_print("fine wdr is %d, fps is %d\n", sensor_formats[0].wdr_mode, sensor_formats[0].fps_fixed);
	return &sensor_formats[0];
#endif
}

static struct sensor_format_struct switch_sensor_formats[] = {
#if defined CONFIG_ISP_FAST_CONVERGENCE || defined CONFIG_ISP_HARD_LIGHTADC
#if GC2083_1920X1088_20FPS
	{
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width      = 1920,
		.height     = 1088,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2932,
		.vts        = 1688,
		.pclk       = 98955000,
		.mipi_bps   = 594 * 1000 * 1000,
		.fps_fixed  = 20,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1688 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.switch_regs	   = sensor_1920x360p120fps_to_1080p20fps,
		.switch_regs_size  = ARRAY_SIZE(sensor_1920x360p120fps_to_1080p20fps),
	},
#endif
#if GC2083_1920X1088_15FPS
	{
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width      = 1920,
		.height     = 1088,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 3140,
		.vts        = 1350,
		.pclk       = 63600000,
		.mipi_bps   = 318 * 1000 * 1000,
		.fps_fixed  = 15,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1350 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.switch_regs	   = sensor_1920x360p120fps_to_1080p15fps,
		.switch_regs_size  = ARRAY_SIZE(sensor_1920x360p120fps_to_1080p15fps),
	},
#endif

#endif
};

static struct sensor_format_struct *sensor_get_switch_format(int id, int isp_id)
{
#if defined CONFIG_ISP_FAST_CONVERGENCE || defined CONFIG_ISP_HARD_LIGHTADC
	int ispid = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);
	struct sensor_format_struct *sensor_format = NULL;
	int wdr_on = isp_get_cfg[ispid].sensor_wdr_on;
	int fps = isp_get_cfg[ispid].sensor_get_fps;
	int i;

	if (current_switch_win[id])
		return current_switch_win[id];

	for (i = 0; i < ARRAY_SIZE(switch_sensor_formats); i++) {
		if (switch_sensor_formats[i].wdr_mode == wdr_on) {
			if (switch_sensor_formats[i].fps_fixed == fps) {
				sensor_format = &switch_sensor_formats[i];
				sensor_print("switch fine wdr is %d, fine fps is %d\n", wdr_on, fps);
				goto done;
			}
		}
	}

	if (sensor_format == NULL) {
		for (i = 0; i < ARRAY_SIZE(switch_sensor_formats); i++) {
			if (switch_sensor_formats[i].wdr_mode == wdr_on) {
				sensor_format = &switch_sensor_formats[i];
				isp_get_cfg[ispid].sensor_get_fps = sensor_format->fps_fixed;
				sensor_print("switch fine wdr is %d, use fps is %d\n", wdr_on, sensor_format->fps_fixed);
				goto done;
			}
		}
	}

	if (sensor_format == NULL) {
		sensor_format = &switch_sensor_formats[0];
		isp_get_cfg[ispid].sensor_wdr_on = sensor_format->wdr_mode;
		isp_get_cfg[ispid].sensor_get_fps = sensor_format->fps_fixed;
		sensor_print("switch use wdr is %d, use fps is %d\n", sensor_format->wdr_mode, sensor_format->fps_fixed);
	}

done:
	current_switch_win[id] = sensor_format;
	return sensor_format;
#else
	return NULL;
#endif
}

static int sensor_g_mbus_config(int id, struct v4l2_mbus_config *cfg, struct mbus_framefmt_res *res)
{
	//struct sensor_info *info = to_state(sd);

	cfg->type  = V4L2_MBUS_CSI2;
	cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
	res->res_time_hs = 0x28;

	return 0;
}

static int sensor_reg_init(int id, int isp_id)
{
	int ret = 0;
	int ispid = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);
	struct sensor_exp_gain exp_gain;

	ret = sensor_write_array(id, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	if (current_win[id]->regs)
		ret = sensor_write_array(id, current_win[id]->regs, current_win[id]->regs_size);
	if (ret < 0)
		return ret;

	gc2083_sensor_vts = current_win[id]->vts;
//#if defined CONFIG_ISP_READ_THRESHOLD || defined CONFIG_ISP_FAST_CONVERGENCE
	if (ispid == 0) {
		exp_gain.exp_val = clamp(*((unsigned int *)ISP0_NORFLASH_SAVE + 2), 16, gc2083_sensor_vts << 4);
		exp_gain.gain_val = clamp(*((unsigned int *)ISP0_NORFLASH_SAVE + 1), 16, 110 << 4);
	} else {
		exp_gain.exp_val = clamp(*((unsigned int *)ISP1_NORFLASH_SAVE + 2), 16, gc2083_sensor_vts << 4);
		exp_gain.gain_val = clamp(*((unsigned int *)ISP1_NORFLASH_SAVE + 1), 16, 110 << 4);
	}

	sensor_s_exp_gain(id, &exp_gain);
	sensor_write(id, 0x023e, 0x99);
//#else  //CONFIG_ISP_HARD_LIGHTADC

//#endif

	return 0;
}

static int sensor_s_stream(int id, int isp_id, int enable)
{
	if (enable && sensor_stream_count[id]++ > 0)
		return 0;
	else if (!enable && (sensor_stream_count[id] == 0 || --sensor_stream_count[id] > 0))
		return 0;

	sensor_dbg("%s on = %d, 1920*1088 fps: 15\n", __func__, enable);

	if (!enable)
		return 0;

	return sensor_reg_init(id, isp_id);
}

static int sensor_s_switch(int id)
{
#if defined CONFIG_ISP_FAST_CONVERGENCE || defined CONFIG_ISP_HARD_LIGHTADC
	int ret = -1;
	gc2083_sensor_vts = current_switch_win[id]->vts;
	if (current_switch_win[id]->switch_regs)
		ret = sensor_write_array(id, current_switch_win[id]->switch_regs, current_switch_win[id]->switch_regs_size);
	else
		sensor_err("cannot find 1920x360p120fps to 1080p%dfps reg\n", current_switch_win[id]->fps_fixed);
	if (ret < 0)
		return ret;
#endif
	return 0;
}

static int sensor_test_i2c(int id)
{
	int ret;
	sensor_power(id, PWR_ON);
	ret = sensor_init(id);
	sensor_power(id, PWR_OFF);

	return ret;
}

struct sensor_fuc_core gc2083_core  = {
	.g_mbus_config = sensor_g_mbus_config,
	.sensor_test_i2c = sensor_test_i2c,
	.sensor_power = sensor_power,
	.s_ir_status = sensor_set_ir,
	.s_stream = sensor_s_stream,
	.s_switch = sensor_s_switch,
	.s_exp_gain = sensor_s_exp_gain,
	.sensor_g_format = sensor_get_format,
	.sensor_g_switch_format = sensor_get_switch_format,
};