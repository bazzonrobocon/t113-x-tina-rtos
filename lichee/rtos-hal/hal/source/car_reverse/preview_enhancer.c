/*
 * Fast car reverse preview enhancer process module
 *
 * Copyright (C) 2015-2023 AllwinnerTech, Inc.
 *
 * Authors:  Huangyongxing <huangyongxing@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
//#define DI_DEBUG

#include "preview_enhancer.h"
#if defined(CONFIG_DRIVERS_DI_V2X) || defined(CONFIG_DRIVERS_DI_V2X_MODULE)
#include "../../di/drv_div2x/sunxi-di.h"
#endif
//#if defined(CONFIG_DRIVERS_DI_V3XX) || defined(CONFIG_DRIVERS_DI_V3X_MODULE)
#if defined CONFIG_DRIVERS_DI_V3XX
#include "../sunxi-di/drv_div3xx/di_client.h"
#include "../sunxi-di/drv_div3xx/sunxi_di.h"
#include "../sunxi-di/include/drm_fourcc.h"

#include <hal_osal.h>
#include <hal_sem.h>
#include <hal_cache.h>
#include <hal_msgbox.h>
#include <openamp/sunxi_helper/openamp.h>
#include <hal_hwspinlock.h>
#include <sys/time.h>
#endif

#if defined CONFIG_DRIVERS_DI_V1XX
#include "../sunxi-di/drv_div1xx/di_client.h"
#include "../sunxi-di/drv_div1xx/sunxi_di.h"
#include "../sunxi-di/include/drm_fourcc.h"

#include <hal_osal.h>
#include <hal_sem.h>
#include <hal_cache.h>
#include <hal_msgbox.h>
#include <openamp/sunxi_helper/openamp.h>
#include <hal_hwspinlock.h>
#include <sys/time.h>
#endif

#define ALIGN_16B(x) (((x) + (15)) & ~(15))

#ifdef DI_DEBUG
#define debug(fmt, arg...) printf(fmt, ##arg)
#else
#define debug(fmt, arg...) do{}while(0);
#endif

extern int di_probe();

struct buffer_node pre_node;
struct buffer_node cur_node;
struct buffer_node ref_node;

bool rp_connecting = false;

enum __di_pixel_fmt_t {
	DI_FORMAT_NV12 = 0x00,	/* 2-plane */
	DI_FORMAT_NV21 = 0x01,	/* 2-plane */
	DI_FORMAT_MB32_12 = 0x02, /* NOT SUPPORTED, UV mapping like NV12 */
	DI_FORMAT_MB32_21 = 0x03, /* NOT SUPPORTED, UV mapping like NV21 */
	DI_FORMAT_YV12 = 0x04,	/* 3-plane */
	DI_FORMAT_YUV422_SP_UVUV = 0x08, /* 2-plane, New in DI_V2.2 */
	DI_FORMAT_YUV422_SP_VUVU = 0x09, /* 2-plane, New in DI_V2.2 */
	DI_FORMAT_YUV422P = 0x0c,	/* 3-plane, New in DI_V2.2 */
	DI_FORMAT_MAX,
};

//#if defined(CONFIG_DRIVERS_DI_V3X) || defined(CONFIG_DRIVERS_DI_V3X_MODULE)
#if defined CONFIG_DRIVERS_DI_V3XX || defined CONFIG_DRIVERS_DI_V1XX

#define RPMSG_SERVICE_NAME		"sunxi,rpmsg_deinterlace"
#define DEINTERLACE_PACKET_MAGIC		0x10244022

struct enhancer_packet {
	u32 magic;
	u32 type;
};

enum deinterlace_packet_type {
	DEINTERLACE_START,
	DEINTERLACE_START_ACK,
	DEINTERLACE_STOP,
	DEINTERLACE_STOP_ACK,
};

struct deinterlace_packet {
	u32 magic;
	u32 type;
};

enum deinterlace_control
{
	GET_BY_LINUX,
	GET_BY_RTOS,
};


struct rpmsg_enhancer_private {
	hal_sem_t finish;		/* notify remote call finish */
	hal_spinlock_t ack_lock;	/* protect ack */
	hal_mutex_t call_lock;		/* protect status and make sure remote call is exclusive */
	hal_workqueue *init_workqueue;
	hal_work init_work;
	struct enhancer_packet ack;
	int status;
	enum deinterlace_control control;
	struct rpmsg_endpoint *ept;
};

struct rpmsg_enhancer_private deinterlace;
static struct di_client *car_client;

static unsigned int get_di_fb_format(unsigned int fmt)
{
	switch (fmt) {
	case DI_FORMAT_YUV422P:
		return DRM_FORMAT_YUV422;
	case DI_FORMAT_YV12:
		return DRM_FORMAT_YUV420;
	case DI_FORMAT_YUV422_SP_UVUV:
		return DRM_FORMAT_NV16;
	case DI_FORMAT_YUV422_SP_VUVU:
		return DRM_FORMAT_NV61;
	case DI_FORMAT_NV12:
		return DRM_FORMAT_NV12;
	case DI_FORMAT_NV21:
		return DRM_FORMAT_NV21;
	default:
		return 0;
	}

	return 0;
}

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	struct deinterlace_packet *pack = data;

	if (pack->magic != DEINTERLACE_PACKET_MAGIC || len != sizeof(*pack)) {
		//printf("packet invalid magic or size %d %d %x\n", len, sizeof(*pack), pack->magic);
		return 0;
	}

	debug("ept callback %d \n", pack->type);
	if (pack->type == DEINTERLACE_START_ACK) {
		deinterlace.control = GET_BY_RTOS;
	} else if (pack->type ==DEINTERLACE_STOP_ACK) {
		deinterlace.control = GET_BY_LINUX;
	}

	return 0;
}

static void rpmsg_unbind_callback(struct rpmsg_endpoint *ept)
{
	//struct rpmsg_demo_private *demo_priv = ept->priv;

	debug("Remote endpoint is destroyed\n");
}

int rpmsg_deinterlace_remote_call(int type)
{
	int ret;
	struct deinterlace_packet packet;

	if (!deinterlace.ept) {
		debug("linux no ok trying to resigter \n");
		if (!deinterlace.ept)
			return 0;
	}

	packet.magic = DEINTERLACE_PACKET_MAGIC;
	packet.type = type;
	ret = openamp_rpmsg_send(deinterlace.ept, (void *)&packet, sizeof(packet));
	if (ret <  sizeof(packet)) {
		printf("%s rpmsg sent fail\r\n", __FUNCTION__);
		ret = -1;
	}

	debug("%s ok\n", __FUNCTION__);
	return ret;
}

int preview_enhancer_rpmsg_init()
{
	struct rpmsg_endpoint *ept;

	rp_connecting = true;
	debug("init deinterlace rpmsg \n");

	if (deinterlace.ept != NULL) {
		debug("rpmsg already init \n");
		return -1;
	}

	ept = openamp_ept_open(RPMSG_SERVICE_NAME, 0, RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
					&deinterlace, rpmsg_ept_callback, rpmsg_unbind_callback);
	if (ept == NULL) {
		printf("Failed to Create Endpoint\r\n");
		deinterlace.control = GET_BY_RTOS;
		return -1;
	}

	deinterlace.ept = ept;
	deinterlace.control = GET_BY_LINUX;
	rp_connecting = false;

	debug("%s ok\r\n", __FUNCTION__);
	return 0;
}
#else
int preview_enhancer_rpmsg_init()
{
	CAR_REVERSE_ERR("interface for DI_V2X is not support yet!\n");
	return 0;
}
#endif

#if defined CONFIG_DRIVERS_DI_V3XX

int preview_enhancer_init(struct preview_params *params)
{
	struct di_timeout_ns t;
	struct di_dit_mode dit_mode;
	struct di_fmd_enable fmd_en;
	struct di_size src_size;
	struct di_rect out_crop;
	struct timeval tv;
	int ret;
	unsigned int src_width, src_height;
	unsigned long long t0 = 0, t1 = 0, t2 = 0, t3 = 0;
	gettimeofday(&tv, NULL);
	t0 = tv.tv_usec;

	debug("preview_enhancer_init \n");
	if (car_client) {
		CAR_REVERSE_INFO("preview enhancer already init \n");
		return 0;
	}

	di_probe();

	gettimeofday(&tv, NULL);
	t1 = tv.tv_usec;

	car_client = (struct di_client *)di_client_create("car_reverse");
	if (!car_client) {
		CAR_REVERSE_ERR("di_client_create failed\n");
		return -1;
	}

	ret = di_client_reset(car_client, NULL);
	if (ret) {
		CAR_REVERSE_ERR("di_client_reset failed\n");
		return -1;
	}

	t.wait4start = 500000000ULL;
	t.wait4finish = 600000000ULL;
	ret = di_client_set_timeout(car_client, &t);
	if (ret) {
		CAR_REVERSE_ERR("di_client_set_timeout failed\n");
		return -1;
	}

	dit_mode.intp_mode = DI_DIT_INTP_MODE_BOB;
	dit_mode.out_frame_mode = DI_DIT_OUT_1FRAME;
	ret = di_client_set_dit_mode(car_client, &dit_mode);
	if (ret) {
		CAR_REVERSE_ERR("di_client_set_dit_mode failed\n");
		return -1;
	}
	fmd_en.en = 0;

	ret = di_client_set_fmd_enable(car_client, &fmd_en);
	if (ret) {
		CAR_REVERSE_ERR("di_client_set_fmd_enable failed\n");
		return -1;
	}

	src_height = params->src_height;
	src_width = params->src_width;
	src_size.width = src_width;
	src_size.height = src_height;
	ret = di_client_set_video_size(car_client,
					&src_size);
	if (ret) {
		CAR_REVERSE_ERR("di_client_set_video_size failed\n");
		return -1;
	}

	out_crop.left = 0;
	out_crop.top = 0;
	out_crop.right = src_size.width;
	out_crop.bottom = src_size.height;
	ret = di_client_set_video_crop(car_client, &out_crop);
	if (ret) {
		CAR_REVERSE_ERR("di_client_set_crop failed\n");
		return -1;
	}

	PREVIEW_ENHANCER_DBG("src_width:%d, src_height:%d\n", \
			     src_size.width, src_size.height);

	PREVIEW_ENHANCER_DBG("crop_left:%d, crop_right:%d, crop_top:%d, crop_bottom:%d\n",
		out_crop.left, out_crop.right, out_crop.top, out_crop.bottom);

	gettimeofday(&tv, NULL);
	ret = di_client_check_para(car_client, NULL);
	if (ret) {
		CAR_REVERSE_ERR("di_client_check_para failed\n");
		return -1;
	}

	t2 = tv.tv_usec;
	gettimeofday(&tv, NULL);

	memset(&pre_node, 0, sizeof(struct buffer_node));
	memset(&cur_node, 0, sizeof(struct buffer_node));
	memset(&ref_node, 0, sizeof(struct buffer_node));

	t3 = tv.tv_usec;

	if (t0 && t1 && t2 && t3) {
		debug("total:%lluus     t0~t1:%lluus  t1~t2:%lluus  t2~t3:%lluus\n",
			(t3 - t0),
			(t1 - t0),
			(t2 - t1),
			(t3 - t2));
	}
	return 0;
}

int preview_enhancer_exit(void)
{
	debug("preview enhancer exit \n");
	if (deinterlace.ept) {
		rpmsg_deinterlace_remote_call(DEINTERLACE_STOP);
	}
	if (car_client) {
		di_client_destroy(car_client);
		memset(&pre_node, 0, sizeof(struct buffer_node));
		memset(&cur_node, 0, sizeof(struct buffer_node));
		memset(&ref_node, 0, sizeof(struct buffer_node));
		car_client = NULL;
	}

	return 0;
}

int preview_image_enhance(struct buffer_node *frame, struct buffer_node **di_frame, int src_w,
			 int src_h, int format)
{
	struct buffer_node *pre_frame = &pre_node;
	struct buffer_node *cur_frame = &cur_node;
	struct buffer_node *zero_frame = &ref_node;
	unsigned int src_width, src_height;
	unsigned int dst_width, dst_height;
	int ret = 0;

	debug("preview_image_enhance \n");

	if (rp_connecting){
		CAR_REVERSE_ERR("RP connecting \n");
		return -1;
	}
	if (deinterlace.ept != NULL && deinterlace.control != GET_BY_RTOS) {
		CAR_REVERSE_INFO("DI is hold by Linux \n");
		rpmsg_deinterlace_remote_call(DEINTERLACE_START);
		return -1;
	}

	if (!frame || !di_frame) {
		CAR_REVERSE_ERR("di src/dst buffer is not available!\n");
		return -1;
	}

	memcpy(cur_frame, frame, sizeof(struct buffer_node));

	/* judge if frame has valid date */
	if ((memcmp(pre_frame, zero_frame, sizeof(struct buffer_node)) != 0) \
	    && (memcmp(cur_frame, zero_frame, sizeof(struct buffer_node)) != 0)) {
		struct di_process_fb_arg fb_arg;
		struct di_fb *fb;
		src_height = src_h;
		src_width = src_w;
		dst_width = src_width;
		dst_height = src_height;
		memset(&fb_arg, 0, sizeof(fb_arg));
		fb_arg.is_interlace = 1;
		fb_arg.is_pulldown = 0;
		fb_arg.top_field_first = 0;

		/* set fb0 */
		/*
		fb = &fb_arg.in_fb1;
		fb->size.width = src_width;
		fb->size.height = src_height;
		fb->buf.cstride = dst_width;
		fb->buf.ystride = ALIGN_16B(dst_width);
		if (format ==
			V4L2_PIX_FMT_NV61)
			fb->format =
			get_di_fb_format(DI_FORMAT_YUV422_SP_VUVU);
		else
			fb->format =
			get_di_fb_format(DI_FORMAT_NV21);
		//fb->buf.y_addr = (u32)(pre_frame->phy_address);
		//fb->buf.cb_addr = fb->buf.y_addr + ALIGN_16B(src_width) * src_height;
		fb->buf.y_addr = 0;
		fb->buf.cb_addr = 0;
		fb->buf.cr_addr = 0;

		PREVIEW_ENHANCER_DBG("pre_frame.height:%d, pre_frame.width:%d\n", \
				     fb->size.width, fb->size.height);
		PREVIEW_ENHANCER_DBG("pre_frame.fd = %d, pre_frame.cb_offset:0x%llx, pre_frame.cr_offset:0x%llx\n", \
			fb->dma_buf_fd, fb->buf.cb_addr, fb->buf.cr_addr);
		*/
		/* set fb1 */
		fb = &fb_arg.in_fb0;
		fb->size.width = src_width;
		fb->size.height = src_height;
		fb->buf.cstride = dst_width;
		fb->buf.ystride = ALIGN_16B(dst_width);
		if (format ==
			V4L2_PIX_FMT_NV61)
			fb->format =
			get_di_fb_format(DI_FORMAT_YUV422_SP_VUVU);
		else
			fb->format =
			get_di_fb_format(DI_FORMAT_NV21);
		fb->buf.y_addr = (u32)(cur_frame->phy_address);
		fb->buf.cb_addr = fb->buf.y_addr + ALIGN_16B(src_width) * src_height;
		fb->buf.cr_addr = 0;
		PREVIEW_ENHANCER_DBG("cur_frame.height:%d, cur_frame.width:%d\n", \
				     fb->size.width, fb->size.height);
		PREVIEW_ENHANCER_DBG("cur_frame.fd = %d, cur_frame.cb_offset:0x%llx, cur_frame.cr_offset:0x%llx\n", \
			fb->dma_buf_fd, fb->buf.cb_addr, fb->buf.cr_addr);

		/* set out_fb0 */
		fb = &fb_arg.out_dit_fb0;
		fb->size.width = dst_width;
		fb->size.height = dst_height;
		fb->buf.cstride = dst_width;
		fb->buf.ystride = ALIGN_16B(dst_width);
		if (format ==
			V4L2_PIX_FMT_NV61)
			fb->format =
			get_di_fb_format(DI_FORMAT_YUV422_SP_VUVU);
		else
			fb->format =
			get_di_fb_format(DI_FORMAT_NV21);
		fb->buf.y_addr = (u32)(*di_frame)->phy_address;
		fb->buf.cb_addr = fb->buf.y_addr +
			ALIGN_16B(dst_width) * dst_height;
		fb->buf.cr_addr = 0;
		PREVIEW_ENHANCER_DBG("next_frame.height:%d, next_frame.width:%d\n", \
				     fb->size.width, fb->size.height);
		PREVIEW_ENHANCER_DBG("next_frame.fd = %d, next_frame.cb_offset:0x%llx, next_frame.cr_offset:0x%llx\n", \
			fb->dma_buf_fd, fb->buf.cb_addr, fb->buf.cr_addr);

		ret = di_client_process_fb(car_client, &fb_arg);
		if (ret < 0) {
			CAR_REVERSE_ERR("di_client_process_fb fail\n");
			ret = -1;
		}
	} else {
		CAR_REVERSE_ERR("first frame from di is not ready!\n");
		ret = -1;
	}
	memcpy(pre_frame, cur_frame, sizeof(struct buffer_node));
	debug("preview_image_enhance end \n");

	return ret;
}

#elif defined CONFIG_DRIVERS_DI_V1XX
int preview_enhancer_init(struct preview_params *params)
{
	struct di_timeout_ns t;
	int ret;

	di_probe();

	car_client = (struct di_client *)di_client_create("car_reverse");
	if (!car_client) {
		CAR_REVERSE_ERR("di_client_create failed\n");
		return -1;
	}

	t.wait4start = 500000000ULL;
	t.wait4finish = 600000000ULL;
	ret = di_client_set_timeout(car_client, &t);
	if (ret) {
		CAR_REVERSE_ERR("di_client_set_timeout failed\n");
		return -1;
	}
	return ret;
}

int preview_enhancer_exit(void)
{
	debug("preview enhancer exit \n");
	if (deinterlace.ept) {
		rpmsg_deinterlace_remote_call(DEINTERLACE_STOP);
	}
	if (car_client) {
		di_client_destroy(car_client, NULL);
		memset(&pre_node, 0, sizeof(struct buffer_node));
		memset(&cur_node, 0, sizeof(struct buffer_node));
		memset(&ref_node, 0, sizeof(struct buffer_node));
		car_client = NULL;
	}

	return 0;
}

int preview_image_enhance(struct buffer_node *frame, struct buffer_node **di_frame, int src_w,
			 int src_h, int format)
{
	unsigned int src_width, src_height;
	unsigned int dst_width, dst_height;
	int ret = 0;

	debug("preview_image_enhance \n");

	if (rp_connecting){
		CAR_REVERSE_ERR("RP connecting \n");
		return -1;
	}
	if (deinterlace.ept != NULL && deinterlace.control != GET_BY_RTOS) {
		CAR_REVERSE_INFO("DI is hold by Linux \n");
		rpmsg_deinterlace_remote_call(DEINTERLACE_START);
		return -1;
	}

	if (!frame || !di_frame) {
		CAR_REVERSE_ERR("di src/dst buffer is not available!\n");
		return -1;
	}
	if (1) {
		struct di_process_fb_arg fb_arg;
		struct di_fb *fb;

		memset(&fb_arg, 0, sizeof(fb_arg));

		fb_arg.output_mode = DI_OUT_0FRAME;
		fb_arg.di_mode = DI_INTP_MODE_BOB;
		if (format ==
			V4L2_PIX_FMT_NV61)
			fb_arg.pixel_format = get_di_fb_format(DI_FORMAT_YUV422_SP_VUVU);
		else
			fb_arg.pixel_format = get_di_fb_format(DI_FORMAT_NV21);
		src_height = src_h;
		src_width = src_w;
		dst_width = src_width;
		dst_height = src_height;

		fb_arg.is_interlace = 1;

		fb_arg.size.width = src_width;
		fb_arg.size.height = src_height;

		fb = &fb_arg.in_fb1;
		fb->size.width = src_width;
		fb->size.height = src_height;

		fb->buf.addr.y_addr = (u64)frame->phy_address;
		fb->buf.addr.cb_addr = fb->buf.addr.y_addr + ALIGN_16B(src_width) * src_height;
		fb->buf.addr.cr_addr = 0;

		/* set out_fb0 */
		fb = &fb_arg.out_fb0;
		fb->size.width = dst_width;
		fb->size.height = dst_height;
		fb->buf.addr.y_addr = (u64)(*di_frame)->phy_address;
		fb->buf.addr.cb_addr = fb->buf.addr.y_addr +
			ALIGN_16B(dst_width) * dst_height;
		fb->buf.addr.cr_addr = 0;

		ret = di_client_process_fb(car_client, &fb_arg);
		if (ret < 0) {
			CAR_REVERSE_ERR("di_client_process_fb fail\n");
			ret = -1;
		}
	} else {
		CAR_REVERSE_ERR("first frame from di is not ready!\n");
		ret = -1;
	}

	debug("preview_image_enhance end \n");
	return ret;
}
#else
//#if defined(CONFIG_DRIVERS_DI_V2X) || defined(CONFIG_DRIVERS_DI_V2X_MODULE)
int preview_enhancer_init(struct preview_params *params)
{
	CAR_REVERSE_ERR("car reverse preview enhancer is not support yet!\n");
	return 0;
}

int preview_enhancer_exit(void)
{
	CAR_REVERSE_ERR("interface for DI_V2X is not support yet!\n");
	return 0;
}

int preview_image_enhance(struct buffer_node *frame, struct buffer_node **di_frame, int src_w,
			 int src_h, int format)
{
	CAR_REVERSE_ERR("interface for DI_V2X is not support yet!\n");
	return -1;
}
#endif
