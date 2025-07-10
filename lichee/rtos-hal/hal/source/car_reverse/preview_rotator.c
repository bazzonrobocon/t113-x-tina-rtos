/*
 * Fast car reverse preview rotation process module
 *
 * Copyright (C) 2015-2023 AllwinnerTech, Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <g2d_driver.h>
#include "fastboot_com.h"
#include "buffer_pool.h"

#ifdef CONFIG_DRIVERS_G2D
int sunxi_g2d_open(void);
int sunxi_g2d_close(void);
int sunxi_g2d_control(int cmd, void *arg);
#else
static inline int sunxi_g2d_open(void) {return 0;}
int sunxi_g2d_close(void) {return 0;}
int sunxi_g2d_control(int cmd, void *arg) {return 0;}
#endif
int preview_image_rotate(struct buffer_node *frame, struct buffer_node **rotate, int src_w,
			 int src_h, int angle, int format)
{
	g2d_blt_h blit_para;
	int ret = -1;

	sunxi_g2d_open();
	if (!frame || !rotate) {
		CAR_REVERSE_ERR("g2d src/dst buffer is not available!\n");
		return -1;
	}

	PREVIEW_ROTATOR_DBG("src_w = %d, src_h = %d, rotation = 0x%x, format = 0x%x\n", \
			src_w, src_h, angle, format);
	PREVIEW_ROTATOR_DBG("src_buf->addr:0x%lx, dst_buf->addr:0x%lx\n", \
			    (unsigned long)frame->phy_address, (unsigned long)rotate[0]->phy_address);

	blit_para.src_image_h.use_phy_addr = 1;
	blit_para.src_image_h.laddr[0] = (__u32)(unsigned long)frame->phy_address;
	blit_para.src_image_h.laddr[1] = (__u32)(unsigned long)frame->phy_address + src_w * src_h;
	blit_para.src_image_h.laddr[2] = (__u32)(unsigned long)frame->phy_address + src_w * src_h * 5 / 4;
	blit_para.src_image_h.mode = G2D_GLOBAL_ALPHA;
	blit_para.src_image_h.clip_rect.x = 0;
	blit_para.src_image_h.clip_rect.y = 0;
	blit_para.src_image_h.clip_rect.w = src_w;
	blit_para.src_image_h.clip_rect.h = src_h;
	blit_para.src_image_h.width = src_w;
	blit_para.src_image_h.height = src_h;
	blit_para.src_image_h.alpha = 0xff;
	blit_para.src_image_h.align[0] = 0;
	blit_para.src_image_h.align[1] = 0;
	blit_para.src_image_h.align[2] = 0;

	blit_para.dst_image_h.use_phy_addr = 1;
	blit_para.dst_image_h.laddr[0] = (__u32)(unsigned long)rotate[0]->phy_address;
	blit_para.dst_image_h.laddr[1] = (__u32)(unsigned long)rotate[0]->phy_address + src_w * src_h;
	blit_para.dst_image_h.laddr[2] = (__u32)(unsigned long)rotate[0]->phy_address + src_w * src_h * 5 / 4;
	blit_para.dst_image_h.mode = G2D_GLOBAL_ALPHA;
	blit_para.dst_image_h.clip_rect.x = 0;
	blit_para.dst_image_h.clip_rect.y = 0;
	blit_para.dst_image_h.alpha = 0xff;
	blit_para.dst_image_h.align[0] = 0;
	blit_para.dst_image_h.align[1] = 0;
	blit_para.dst_image_h.align[2] = 0;

	switch (format) {
	case -1:
		blit_para.src_image_h.format = G2D_FORMAT_ARGB8888;
		blit_para.dst_image_h.format = G2D_FORMAT_ARGB8888;
		break;
	case V4L2_PIX_FMT_NV21:
		blit_para.src_image_h.format = G2D_FORMAT_YUV420UVC_U1V1U0V0;
		blit_para.dst_image_h.format = G2D_FORMAT_YUV420UVC_U1V1U0V0;
		break;
	case V4L2_PIX_FMT_NV12:
		blit_para.src_image_h.format = G2D_FORMAT_YUV420UVC_V1U1V0U0;
		blit_para.dst_image_h.format = G2D_FORMAT_YUV420UVC_V1U1V0U0;
		break;
	case V4L2_PIX_FMT_NV61:
		blit_para.src_image_h.format = G2D_FORMAT_YUV422UVC_U1V1U0V0;
		blit_para.dst_image_h.format = G2D_FORMAT_YUV422UVC_U1V1U0V0;
		break;
	case V4L2_PIX_FMT_NV16:
		blit_para.src_image_h.format = G2D_FORMAT_YUV422UVC_V1U1V0U0;
		blit_para.dst_image_h.format = G2D_FORMAT_YUV422UVC_V1U1V0U0;
		break;
	case V4L2_PIX_FMT_YUV420:
		blit_para.src_image_h.format = G2D_FORMAT_YUV420_PLANAR;
		blit_para.dst_image_h.format = G2D_FORMAT_YUV420_PLANAR;
		break;
	case V4L2_PIX_FMT_YUV422P:
		blit_para.src_image_h.format = G2D_FORMAT_YUV422_PLANAR;
		blit_para.dst_image_h.format = G2D_FORMAT_YUV422_PLANAR;
		break;
	default:
		blit_para.src_image_h.format = G2D_FORMAT_YUV420UVC_U1V1U0V0;
		blit_para.dst_image_h.format = G2D_FORMAT_YUV420UVC_U1V1U0V0;
	}

	switch (angle) {
	case 0x1:
		blit_para.flag_h = G2D_ROT_90;
		blit_para.dst_image_h.width = src_h;
		blit_para.dst_image_h.height = src_w;
		blit_para.dst_image_h.clip_rect.w = src_h;
		blit_para.dst_image_h.clip_rect.h = src_w;
		break;
	case 0x2:
		blit_para.flag_h = G2D_ROT_180;
		blit_para.dst_image_h.width = src_w;
		blit_para.dst_image_h.height = src_h;
		blit_para.dst_image_h.clip_rect.w = src_w;
		blit_para.dst_image_h.clip_rect.h = src_h;
		break;
	case 0x3:
		blit_para.flag_h = G2D_ROT_270;
		blit_para.dst_image_h.width = src_h;
		blit_para.dst_image_h.height = src_w;
		blit_para.dst_image_h.clip_rect.w = src_h;
		blit_para.dst_image_h.clip_rect.h = src_w;
		break;
	case 0xe:
		blit_para.flag_h = G2D_ROT_H;
		blit_para.dst_image_h.width = src_w;
		blit_para.dst_image_h.height = src_h;
		blit_para.dst_image_h.clip_rect.w = src_w;
		blit_para.dst_image_h.clip_rect.h = src_h;
		break;
	case 0xf:
		blit_para.flag_h = G2D_ROT_V;
		blit_para.dst_image_h.width = src_w;
		blit_para.dst_image_h.height = src_h;
		blit_para.dst_image_h.clip_rect.w = src_w;
		blit_para.dst_image_h.clip_rect.h = src_h;
		break;
	default:
		blit_para.flag_h = G2D_ROT_0;
		blit_para.dst_image_h.width = src_w;
		blit_para.dst_image_h.height = src_h;
		blit_para.dst_image_h.clip_rect.w = src_w;
		blit_para.dst_image_h.clip_rect.h = src_h;
	}

	ret = sunxi_g2d_control(G2D_CMD_BITBLT_H, &blit_para);
	if (ret < 0)
		CAR_REVERSE_ERR("preview g2d_rotate failed!\n");

	sunxi_g2d_close();

	return ret;

}
