/*
* This confidential and proprietary software should be used
* under the licensing agreement from Allwinner Technology.

* Copyright (C) 2020-2030 Allwinner Technology Limited
* All rights reserved.

* Author:zhengwanyu <zhengwanyu@allwinnertech.com>

* The entire notice above must be reproduced on all authorised
* copies and copies may only be made to the extent permitted
* by a licensing agreement from Allwinner Technology Limited.
*/
#ifndef _DIV3XX_UAPI_H
#define _DIV3XX_UAPI_H

#include <stdbool.h>

#define DI_MAX_PLANE_NUM 3

typedef void *DIDevice;

enum {
	DIV3X_MODE_BOB = 0,
	DIV3X_MODE_OUT2_TNR,
	DIV3X_MODE_OUT2,
	DIV3X_MODE_OUT1_TNR,
	DIV3X_MODE_OUT1,
	DIV3X_MODE_ONLY_TNR,
	DIV3X_MODE_NUM,
};

enum di_pixel_format {
	DI_FORMAT_YUV420_PLANNER = 0,
	DI_FORMAT_YUV420_NV21, /*UV UV UV*/
	DI_FORMAT_YUV420_NV12, /*VU VU VU*/

	DI_FORMAT_YUV422_PLANNER,
	DI_FORMAT_YUV422_NV61, /*UV UV UV*/
	DI_FORMAT_YUV422_NV16, /*VU VU VU*/

	DI_FORMAT_NUM,
};

enum di_tnr_out_format {
	DI_IN_FORMAT_YUV420P = DI_FORMAT_YUV420_PLANNER,
	DI_IN_FORMAT_YUV422P = DI_FORMAT_YUV422_PLANNER,
};

enum {
	TNR_MODE_ADAPTIVE = 1,
	TNR_MODE_FIX,
};

enum {
	TNR_LEVEL_HIGH = 0,
	TNR_LEVEL_MIDDLE,
	TNR_LEVEL_LOW,
};

enum {
       DI_FIELD_TYPE_COMBINE_FIELD = 0,
       DI_FIELD_TYPE_TOP_FIELD,
       DI_FIELD_TYPE_BOTTOM_FIELD,
};

struct reserve_8bytes {
	unsigned int reserve0;
	unsigned int reserve1;
};

struct reserve_12bytes {
	unsigned int reserve0;
	unsigned int reserve1;
	unsigned int reserve2;
};

struct reserve_16bytes {
	unsigned int reserve0;
	unsigned int reserve1;
	unsigned int reserve2;
	unsigned int reserve3;
};



struct di_tnr_work_mode {
	unsigned int mode;
	unsigned int level;
};

struct di_size {
	unsigned int width;
	unsigned int height;
};

struct di_rect {
	unsigned int left;
	unsigned int top;
	unsigned int right;
	unsigned int bottom;
};

struct di_buffer {
	int fd;
	union {
		unsigned long long phyaddr;
		unsigned long long offset;
	};
	unsigned char field_type; //details is in DI_FIELD_TYPE_*
};

struct init_para_out2_tnr {
	unsigned int src_w;
	unsigned int src_h;
	unsigned int dst_w;
	unsigned int dst_h;

	struct di_rect crop;
	struct di_rect democrop;

	struct di_buffer tnrBuffer[3];
	enum di_tnr_out_format tnrFormat;

	bool fmd_en;
	unsigned char debug_en;
};

struct init_para_out2 {
	unsigned int src_w;
	unsigned int src_h;
	unsigned int dst_w;
	unsigned int dst_h;

	struct di_rect crop;
	struct reserve_16bytes reserve0;

	struct reserve_12bytes reserve1[3];
	unsigned int resverve2;

	bool fmd_en;
	unsigned char debug_en;
};

struct init_para_out1_tnr {
	unsigned int src_w;
	unsigned int src_h;
	unsigned int dst_w;
	unsigned int dst_h;

	struct di_rect crop;
	struct di_rect democrop;

	struct di_buffer tnrBuffer[2];
	enum di_tnr_out_format tnrFormat;

	bool fmd_en;
	unsigned char debug_en;
};

struct init_para_out1 {
	unsigned int src_w;
	unsigned int src_h;
	unsigned int dst_w;
	unsigned int dst_h;

	struct di_rect crop;
	struct reserve_16bytes reserve0;

	struct reserve_12bytes reserve1[2];
	unsigned int reserve2;

	bool fmd_en;
	unsigned char debug_en;
};

struct init_para_only_tnr {
	unsigned int src_w;
	unsigned int src_h;
	unsigned int dst_w;
	unsigned int dst_h;

	struct di_rect crop;
	struct di_rect democrop;

	struct reserve_12bytes reserve0[2];
	unsigned int reserve1;

	bool reserve2;
	unsigned char debug_en;
};

struct init_para_bob {
	unsigned int src_w;
	unsigned int src_h;
	unsigned int dst_w;
	unsigned int dst_h;

	struct di_rect crop;
	struct reserve_16bytes reserve0;

	struct reserve_12bytes reserve1[2];
	unsigned int reserve2;

	bool reserve3;
	unsigned char debug_en;
};

struct fb_para_out2_tnr {
	bool topFieldFirst;
	unsigned char base_field;

	struct di_buffer inFb[3];
	struct di_buffer inFbNf[3];//NF, next field
	enum di_pixel_format inFormat[3];

	struct di_buffer outFb[2];
	enum di_tnr_out_format outFormat[2];

	struct di_tnr_work_mode tnrMode;
};

struct fb_para_out2 {
	bool topFieldFirst;
	unsigned char base_field;

	struct di_buffer inFb[3];
	struct di_buffer inFbNf[3];//NF, next field, NOT used if *_FIELD_COMBINE
	enum di_pixel_format inFormat[3];

	struct di_buffer outFb[2];
	enum di_pixel_format outFormat[2];

	struct reserve_8bytes reserve0;
};
struct fb_para_out1_tnr {
	bool topFieldFirst;
	unsigned char base_field;

	struct di_buffer inFb[2];
	struct di_buffer inFbNf[2];//NF, next field, NOT used if *_FIELD_COMBINE
	enum di_pixel_format inFormat[2];

	struct di_buffer outFb;
	enum di_tnr_out_format outFormat;

	struct di_tnr_work_mode tnrMode;
};

struct fb_para_out1 {
	bool topFieldFirst;
	unsigned char base_field;

	struct di_buffer inFb[2];
	struct di_buffer inFbNf[2];//NF, next field, NOT used if *_FIELD_COMBINE
	enum di_pixel_format inFormat[2];

	struct di_buffer outFb;
	enum di_pixel_format outFormat;

	struct reserve_8bytes reserve0;
};

struct fb_para_only_tnr {
	bool reserve0;
	unsigned char base_field;

	struct di_buffer inFb[2];
	struct di_buffer reserve1[2];
	enum di_pixel_format inFormat[2];

	struct di_buffer outTnrFb;
	enum di_tnr_out_format outTnrFormat;

	struct di_tnr_work_mode tnrMode;
};

struct fb_para_bob {
	bool topFieldFirst;
	unsigned char base_field;

	struct di_buffer inFb[2];
	struct di_buffer inFbNf[2];//NF, next field, NOT used if *_FIELD_COMBINE
	enum di_pixel_format inFormat[2];

	struct di_buffer outFb;
	enum di_pixel_format outFormat;

	struct reserve_8bytes reserve0;
};

static inline char *getDiPixelFormatName(unsigned int fmt)
{
	switch(fmt) {
	case DI_FORMAT_YUV420_PLANNER:
		return "yv12";
	case DI_FORMAT_YUV420_NV12:
		return "nv12";
	case DI_FORMAT_YUV420_NV21:
		return "nv21";
	case DI_FORMAT_YUV422_PLANNER:
		return "yv16";
	case DI_FORMAT_YUV422_NV16:
		return "nv16";
	case DI_FORMAT_YUV422_NV61:
		return "nv61";
	}
	printf("invalid pixel format:%d\n", fmt);

	return NULL;
}

static inline char *getDiModeName(unsigned int diMode)
{

	switch(diMode) {
	case DIV3X_MODE_BOB:
		return "bob";
	case DIV3X_MODE_OUT2_TNR:
		return "out2tnr";
	case DIV3X_MODE_OUT2:
		return "out2";
	case DIV3X_MODE_OUT1_TNR:
		return "out1tnr";
	case DIV3X_MODE_OUT1:
		return "out1";
	case DIV3X_MODE_ONLY_TNR:
		return "onlytnr";
	}
	printf("invalid di mode:%d\n", diMode);

	return NULL;
}

int diInit(DIDevice *deinterlace, unsigned int mode, void *init_para);
int diFrameBuffer(DIDevice deinterlace, void *fb_para);
int diExit(DIDevice deinterlace);

#endif