/*
 * tvd.c
 *
 * Copyright (c) 2018 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Jinkun Chen <chenjinkun@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <hal_mem.h>
#include <hal_cmd.h>
#include <hal_dma.h>
#include <hal_interrupt.h>
#include <hal_clk.h>
#include <hal_reset.h>
#include "tvd.h"
#include "tvd_config_sun8iw20p1.h"
#include "vin_common.h"
#include "tvd_rpmsg.h"


#define TVD_MODULE_NAME "sunxi_tvd"
#define MIN_WIDTH (32)
#define MIN_HEIGHT (32)
#define MAX_WIDTH (4096)
#define MAX_HEIGHT (4096)
#define MAX_BUFFER (32 * 1024 * 1024)
#define NUM_INPUTS 1

#define TVD_MAX_POWER_NUM 2
#define TVD_MAX_GPIO_NUM 2
#define TVD_MAJOR_VERSION 1
#define TVD_MINOR_VERSION 0
#define TVD_RELEASE 0
#define TVD_VERSION                                                            \
	KERNEL_VERSION(TVD_MAJOR_VERSION, TVD_MINOR_VERSION, TVD_RELEASE)

/*v4l2_format's raw_data index*/
#define RAW_DATA_INTERFACE   1
#define RAW_DATA_SYSTEM      2
#define RAW_DATA_FORMAT      3
#define RAW_DATA_PIXELFORMAT 4
#define RAW_DATA_ROW         5
#define RAW_DATA_COLUMN      6
#define RAW_DATA_CH0_INDEX   7
#define RAW_DATA_CH1_INDEX   8
#define RAW_DATA_CH2_INDEX   9
#define RAW_DATA_CH3_INDEX   10
#define RAW_DATA_CH0_STATUS  11
#define RAW_DATA_CH1_STATUS  12
#define RAW_DATA_CH2_STATUS  13
#define RAW_DATA_CH3_STATUS  14

/*todo:alloc dynamic*/
static struct tvd_dev tvd;
static unsigned long tvd_addr;
static unsigned long tvd_top;

static unsigned int tvd_row;
static unsigned int tvd_column;
static unsigned int tvd_total_num;

/* use for reversr special interfaces */
static hal_mutex_t fliter_lock;

static hal_irqreturn_t tvd_isr(void *priv);
static int tvd_probe(unsigned int id);
static int tvd_wait_done_buf(void);

//#define DISPLAY_FRAME

#ifdef CONFIG_KERNEL_FREERTOS
struct memheap tvd_mempool;
#else
struct rt_memheap tvd_mempool;
#endif

#if defined DISPLAY_FRAME
extern void preview_display_tmp_test(unsigned long addr);
#endif

static struct tvd_clk_info tvd_default_clk[TVD_MAX_CLK] = {
	[TVD_TOP_CLK] = {
		.clock_id = SUNXI_CLK_TVD,
	},

	[TVD_MBUS_CLK] = {
		.clock_id = SUNXI_CLK_BUS_TVD,
	},

	[TVD0_TOP_CLK_SRC] = {
		.clock_id = SUNXI_CLK_TVD0_SRC,
	},

	[TVD0_TOP_CLK] = {
		.clock_id = SUNXI_CLK_TVD0,
	},
	[TVD0_MBUS_CLK] = {
		.clock_id = SUNXI_CLK_BUS_TVD0,
	},
};

static struct tvd_rst_clk_info tvd_default_rst_clk[TVD_MAX_RET] = {
	[TVD_RET] = {
		.clock_id = SUNXI_CLK_RESET_TVD,
		.type = HAL_SUNXI_RESET,
	},
	[TVD_TOP_RET] = {
		.clock_id = SUNXI_CLK_RESET_TVD_TOP,
		.type = HAL_SUNXI_RESET,
	},
};

static struct tvd_fmt formats[] = {
	{
		.name = "planar UVUV",
		.fourcc = V4L2_PIX_FMT_NV12,
		.output_fmt = TVD_PL_YUV420,
		.depth = 12,
	},
	{
		.name = "planar VUVU",
		.fourcc = V4L2_PIX_FMT_NV21,
		.output_fmt = TVD_PL_YUV420,
		.depth = 12,
	},
	{
		.name = "planar UVUV",
		.fourcc = V4L2_PIX_FMT_NV16,
		.output_fmt = TVD_PL_YUV422,
		.depth = 16,
	},
	{
		.name = "planar VUVU",
		.fourcc = V4L2_PIX_FMT_NV61,
		.output_fmt = TVD_PL_YUV422,
		.depth = 16,
	},
	/* this format is not standard, just for allwinner. */
	{
		.name = "planar PACK",
		.fourcc = 0,
		.output_fmt = TVD_MB_YUV420,
		.depth = 12,
	},
};


static inline int tvd_is_generating(struct tvd_dev *dev)
{
	return dev->generating;
}

static inline void tvd_start_generating(struct tvd_dev *dev)
{
	tvd_here;

	dev->generating = 1;

	return;
}

static inline void tvd_stop_generating(struct tvd_dev *dev)
{
	tvd_here;
	dev->generating = 0;

	return;
}

static int tvd_is_opened(struct tvd_dev *dev)
{
	int ret;

	tvd_here;
	hal_mutex_lock(dev->opened_lock);
	ret = dev->opened;
	hal_mutex_unlock(dev->opened_lock);
	return ret;
}

static void tvd_start_opened(struct tvd_dev *dev)
{

	tvd_here;
	hal_mutex_lock(dev->opened_lock);
	dev->opened = 1;
	hal_mutex_unlock(dev->opened_lock);
}

static void tvd_stop_opened(struct tvd_dev *dev)
{

	tvd_here;
	hal_mutex_lock(dev->opened_lock);
	dev->opened = 0;
	hal_mutex_unlock(dev->opened_lock);
}

static int __tvd_clk_init(struct tvd_dev *dev)
{
	int div = 0, ret = 0;
	unsigned long p = 216000000;
	hal_clk_t parent;

	tvd_here;

	tvd_dbg("%s: dev->interface = %d, dev->system = %d\n", __func__,
			dev->interface, dev->system);

	hal_clk_set_parent(tvd_default_clk[TVD0_TOP_CLK].clock,
					   tvd_default_clk[TVD0_TOP_CLK_SRC].clock);

	parent = hal_clk_get_parent(tvd_default_clk[TVD0_TOP_CLK].clock);
	if (IS_ERR_OR_NULL(parent) ||
		IS_ERR_OR_NULL(tvd_default_clk[TVD0_TOP_CLK].clock)) {
		tvd_wrn("get parent clk fail!\n");
		return -EINVAL;
	} else
		tvd_dbg("parent:%s\n", parent->name);

	/* parent is 297M */
	ret = hal_clk_set_rate(parent, p);
	if (ret) {
		ret = -EINVAL;
		tvd_wrn("set tvd0 src clk rate fail!\n");
		goto out;
	}

	if (dev->interface == CVBS_INTERFACE || (dev->sel == 3)) {
		/* cvbs interface */
		div = 8;

	} else if (dev->interface == YPBPRI_INTERFACE) {
		/* ypbprI interface */
		div = 8;
	} else if (dev->interface == YPBPRP_INTERFACE) {
		/* ypbprP interface */
		div = 4;
	} else {
		tvd_wrn("%s: interface is err!\n", __func__);
		return -EINVAL;
	}

	tvd_dbg("div = %d\n", div);

	p /= div;

	ret = hal_clk_set_rate(tvd_default_clk[TVD0_TOP_CLK].clock, p);
	if (ret) {
		ret = -EINVAL;
		tvd_wrn("set tvd0 top clk fail!\n");
		goto out;
	}
	tvd_dbg("tvd%d: parent = %u, clk = %u\n", dev->sel,
			hal_clk_get_rate(parent), hal_clk_get_rate(tvd_default_clk[TVD0_TOP_CLK].clock));

out:
	return ret;
}

static int __tvd_clk_enable(struct tvd_dev *dev)
{
	int ret = 0;

	ret = hal_reset_control_deassert(tvd_default_rst_clk[TVD_TOP_RET].clock);
	if (ret) {
		tvd_wrn("reset clk_top_rst fail");
		return ret;
	}

	//(*(volatile unsigned int *)0x02001bdc) |= (1 < 17);

	ret = hal_clock_enable(tvd_default_clk[TVD_MBUS_CLK].clock);
	if (ret) {
		tvd_wrn("%s: tvd clk mbus enable err!", __func__);
		return ret;
	}

	ret = hal_clock_enable(tvd_default_clk[TVD_TOP_CLK].clock);
	if (ret) {
		tvd_wrn("%s: tvd top clk enable err!", __func__);
		hal_clock_disable(tvd_default_clk[TVD_TOP_CLK].clock);
		return ret;
	}

	ret = hal_reset_control_deassert(tvd_default_rst_clk[TVD_RET].clock);
	if (ret) {
		tvd_wrn("%s: reset tvd reset fail!", __func__);
		return ret;
	}

	//(*(volatile unsigned int *)0x02001bdc) |= (1 < 16);

	ret = hal_clock_enable(tvd_default_clk[TVD0_MBUS_CLK].clock);
	if (ret)
		tvd_wrn("enable tvd bus clk fail");

	ret = hal_clock_enable(tvd_default_clk[TVD0_TOP_CLK].clock);
	if (ret) {
		tvd_wrn("%s: tvd0 clk enable err!", __func__);
		hal_clock_disable(tvd_default_clk[TVD0_TOP_CLK].clock);
	}

	return ret;
}

static int tvd_put_clk(void)
{
	hal_reset_control_put(tvd_default_rst_clk[TVD_RET].clock);
	hal_reset_control_put(tvd_default_rst_clk[TVD_TOP_RET].clock);
	hal_clock_put(tvd_default_clk[TVD0_MBUS_CLK].clock);
	hal_clock_put(tvd_default_clk[TVD0_TOP_CLK].clock);
	hal_clock_put(tvd_default_clk[TVD0_TOP_CLK_SRC].clock);
	hal_clock_put(tvd_default_clk[TVD_MBUS_CLK].clock);
	hal_clock_put(tvd_default_clk[TVD_TOP_CLK].clock);

	return 0;
}

static int __tvd_clk_disable(struct tvd_dev *dev)
{
	int ret = 0;

	tvd_here;

	hal_clock_disable(tvd_default_clk[TVD0_TOP_CLK].clock);
	hal_clock_disable(tvd_default_clk[TVD0_MBUS_CLK].clock);
	hal_reset_control_assert(tvd_default_rst_clk[TVD_RET].clock);

	hal_clock_disable(tvd_default_clk[TVD_TOP_CLK].clock);
	hal_reset_control_assert(tvd_default_rst_clk[TVD_TOP_RET].clock);
	hal_clock_disable(tvd_default_clk[TVD_MBUS_CLK].clock);

	tvd_put_clk();

	return ret;
}

static int __tvd_init(struct tvd_dev *dev)
{
	tvd_here;

	tvd_top_set_reg_base((unsigned long)tvd_top);
	tvd_set_reg_base(dev->sel, (unsigned long)tvd_addr);

	return 0;
}

static int __tvd_config(struct tvd_dev *dev)
{
	tvd_here;

	//tvd_init(dev->sel, dev->interface);
	tvd_config(dev->sel,
			   (dev->sel == 3) ? 0 : dev->interface, dev->system);
	tvd_set_wb_width(dev->sel, dev->width);
	tvd_set_wb_width_jump(dev->sel, dev->width);
	if (dev->interface == YPBPRP_INTERFACE && (dev->sel != 3))
		tvd_set_wb_height(dev->sel, dev->height); /*P,no div*/
	else
		tvd_set_wb_height(dev->sel, dev->height / 2);

	/* pl_yuv420, mb_yuv420, pl_yuv422 */
	tvd_set_wb_fmt(dev->sel, dev->fmt->output_fmt);
	switch (dev->fmt->fourcc) {

	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV16:
		tvd_set_wb_uv_swap(dev->sel, 0);
		break;

	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV61:
		tvd_set_wb_uv_swap(dev->sel, 1);
		break;
	}

	return 0;
}

static int __tvd_3d_comp_mem_request(struct tvd_dev *dev, int size)
{
	tvd_here;

	dev->fliter.size = PAGE_ALIGN(size);
#ifdef CONFIG_KERNEL_FREERTOS
	dev->fliter.phy_address = memheap_alloc_align(&tvd_mempool, size, 0x1000);
#else
	dev->fliter.phy_address = rt_memheap_alloc_align(&tvd_mempool, size, 0x1000);
#endif

	if (IS_ERR_OR_NULL(dev->fliter.phy_address)) {
		tvd_wrn("%s: 3d fliter buf_alloc failed!\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static void __tvd_3d_comp_mem_free(struct tvd_dev *dev)
{
	tvd_here;

#ifdef CONFIG_KERNEL_FREERTOS
	memheap_free_align(dev->fliter.phy_address);
#else
	memheap_free_align_rt(dev->fliter.phy_address);
#endif
}

static int tvd_request_buf(void)
{
	unsigned long size;
	int buf_idx = 0;
	struct tvd_dmaqueue *vidq = &tvd.vidq;

	if (tvd.width < MIN_WIDTH || tvd.width > MAX_WIDTH ||
		tvd.height < MIN_HEIGHT || tvd.height > MAX_HEIGHT) {
		return -EINVAL;
	}

	tvd_here;

	switch (tvd.fmt->output_fmt) {
	case TVD_MB_YUV420:
	case TVD_PL_YUV420:
		size = tvd.width * tvd.height * 3 / 2;
		break;
	case TVD_PL_YUV422:
	default:
		size = tvd.width * tvd.height * 2;
		break;
	}

	tvd.frame_size = size;

	hal_mutex_lock(tvd.buf_lock);

	for (buf_idx = 0; buf_idx < TVD_BUF_NUM; buf_idx++) {
#ifdef CONFIG_KERNEL_FREERTOS
		tvd.tvd_buf[buf_idx].paddr = memheap_alloc_align(&tvd_mempool, size, 0x1000);
#else
		tvd.tvd_buf[buf_idx].paddr = memheap_alloc_align_rt(&tvd_mempool, size, 0x1000);
#endif
		if(tvd.tvd_buf[buf_idx].paddr) {
			tvd_wrn("%s buf_idx:%d paddr:%p size:%lu suc !\n", __func__,
					buf_idx, tvd.tvd_buf[buf_idx].paddr,
						size);

			list_add_tail(&tvd.tvd_buf[buf_idx].list, &vidq->active);
		} else {
			tvd_wrn("%s buf_idx:%d size:%lu fail!\n", __func__, buf_idx, size);
			return -1;
		}
	}

	hal_mutex_unlock(tvd.buf_lock);

	return 0;
}

static int tvd_release_buf(void)
{
	__attribute__((__unused__)) unsigned long size;
	int buf_idx = 0;

	if (tvd.width < MIN_WIDTH || tvd.width > MAX_WIDTH ||
		tvd.height < MIN_HEIGHT || tvd.height > MAX_HEIGHT) {
		return -EINVAL;
	}

	tvd_here;

	switch (tvd.fmt->output_fmt) {
	case TVD_MB_YUV420:
	case TVD_PL_YUV420:
		size = tvd.width * tvd.height * 3 / 2;
		break;
	case TVD_PL_YUV422:
	default:
		size = tvd.width * tvd.height * 2;
		break;
	}

	hal_mutex_lock(tvd.buf_lock);

	for (buf_idx = 0; buf_idx < TVD_BUF_NUM; buf_idx++) {
		if(tvd.tvd_buf[buf_idx].paddr) {
#ifdef CONFIG_KERNEL_FREERTOS
			memheap_free_align(tvd.tvd_buf[buf_idx].paddr);
#else
			memheap_free_align_rt(tvd.tvd_buf[buf_idx].paddr);
#endif
		}
	}

	hal_mutex_unlock(tvd.buf_lock);

	return 0;
}


/**
 * @name       __get_status
 * @brief      get specified tvd device's status
 * @param[IN]  dev
 * @param[OUT] locked: 1:signal locked, 0:signal not locked
 * @param[OUT] system: 1:pal, 0:ntsc
 * @return     none
 */
static void __get_status(struct tvd_dev *dev, unsigned int *locked,
						 unsigned int *system)
{
	int i = 0;

	tvd_here;

	if (dev->interface > 0) {
		/* ypbpr signal, search i/p */
		dev->interface = 1;
		for (i = 0; i < 2; i++) {
			__tvd_clk_init(dev);
			hal_mdelay(200);

			tvd_get_status(dev->sel, locked, system);
			if (*locked)
				break;

			if (dev->interface < 2)
				dev->interface++;
		}
	} else if (dev->interface == 0) {
		/* cvbs signal */
		hal_mdelay(200);

		tvd_get_status(dev->sel, locked, system);
		tvd_dbg("tvd%d locked:%d system:%d\n",
				dev->sel, *locked, *system);
	}
}

static struct tvd_fmt *__get_format(struct tvd_dev *dev, u32 pixelformat)
{
	struct tvd_fmt *fmt;
	u32 i;

	tvd_here;


	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		fmt = &formats[i];
		if (fmt->fourcc == pixelformat) {
			tvd_dbg("fourcc = %d\n", fmt->fourcc);
			break;
		}
	}
	if (i == ARRAY_SIZE(formats))
		return NULL;

	return &formats[i];
}

static int tvd_cagc_and_3d_config(struct tvd_dev *dev)
{
	tvd_here;


	if (AGC_AUTO_ENABLE == 0) {
		/* manual mode */
		tvd_agc_manual_config(dev->sel, (u32)AGC_MANUAL_VALUE);
		tvd_wrn("tvd%d agc manual:0x%x\n", dev->sel, AGC_MANUAL_VALUE);
	} else {
		/* auto mode */
		tvd_agc_auto_config(dev->sel);
		tvd_wrn("tvd%d agc auto mode\n", dev->sel);
	}

	if(CAGC_ENABLE) {
		tvd_cagc_config(dev->sel, 1);
		tvd_wrn("tvd%d CAGC enable\n", dev->sel);
	} else {
		tvd_cagc_config(dev->sel, 0);
		tvd_wrn("tvd%d CAGC disable\n", dev->sel);
	}

	dev->fliter.used = FLITER_USERD;

	if (dev->fliter.used) {
		hal_mutex_lock(fliter_lock);

		if (__tvd_3d_comp_mem_request(
				dev, (int)TVD_3D_COMP_BUFFER_SIZE)) {
			/* no mem support for 3d fliter */
			dev->fliter.used = 0;
			hal_mutex_unlock(fliter_lock);
			tvd_dbg(
				"no mem support for 3d fliter\n");
			goto OUT;
		}

		tvd_3d_mode(dev->sel, 1, (u64)dev->fliter.phy_address);
		tvd_wrn("tvd%d 3d enable :%p\n", dev->sel,
				dev->fliter.phy_address);

		hal_mutex_unlock(fliter_lock);
	}
OUT:
	tvd_dbg("%s:Enable _3D_FLITER,used:%d\n", __func__,
			dev->fliter.used);
	return 0;
}

int tvd_vidioc_s_input(unsigned int i)
{
	tvd_dbg("%s: input_num = %d\n", __func__, i);

	tvd.input = i;
	tvd_input_sel(tvd.input);
	return 0;
}


static int vidioc_s_parm(void)
{
	tvd.capture_mode = V4L2_MODE_VIDEO;

	return 0;
}

static void __tvd_set_addr(struct tvd_dev *dev,
						   struct tvd_buffer *buffer)
{
	unsigned long addr_org;
	unsigned int c_offset = 0;

	if (buffer == NULL || buffer->paddr == NULL) {
		tvd_wrn("vb_buf->priv is NULL!\n");
		return;
	}

	addr_org = (unsigned long)buffer->paddr;

	switch (dev->fmt->output_fmt) {
	case TVD_PL_YUV422:
	case TVD_PL_YUV420:
		c_offset = dev->width * dev->height;
		break;
	case TVD_MB_YUV420:
		c_offset = 0;
		break;
	default:
		break;
	}
	tvd_set_wb_addr(dev->sel, addr_org, addr_org + c_offset);
	tvd_dbg("%s: format:%d, addr_org = 0x%p, addr_org + c_offset = 0x%p\n",
			__func__, dev->format, (void *)addr_org,
			(void *)(addr_org + c_offset));
}

int tvd_info(void)
{
	return TVD_COUNT;
}

int tvd_open(int id)
{
	struct tvd_dev *dev = &tvd;
	int ret = -1;
	struct tvd_dmaqueue *active = &dev->vidq;
	struct tvd_dmaqueue *done = &dev->done;

	if (tvd_is_opened(dev)) {
		tvd_wrn("%s: device open busy\n", __func__);
		return -EBUSY;
	}

	/*device probe*/
	ret = tvd_probe(id);
	if (ret) {
		tvd_wrn("tvd driver probe failed\n");
		return -1;
	}

	dev->row = 1;
	dev->column = 1;
	dev->system = NTSC;
	dev->first_flag = 0;

	INIT_LIST_HEAD(&active->active);
	INIT_LIST_HEAD(&done->active);
	__tvd_init(dev);

	/* register irq */
	tvd_dbg("request_irq:%d\n", dev->irq);
	ret = hal_request_irq(dev->irq, tvd_isr, "sunxi-tvd", dev);
	if (__tvd_clk_init(dev))
		tvd_wrn("%s: clock init fail!\n", __func__);

	ret = __tvd_clk_enable(dev);

	tvd_init(dev->sel, dev->interface);

	vidioc_s_parm();

	tvd_start_opened(dev);

#ifdef CONFIG_KERNEL_FREERTOS
	memheap_init(&tvd_mempool, "tvd-mempool", (void *)TVD_MEMRESERVE, TVD_MEMRESERVE_SIZE);
#else
	rt_memheap_init(&tvd_mempool, "tvd-mempool", (void *)TVD_MEMRESERVE, TVD_MEMRESERVE_SIZE);
#endif

	return ret;
}

int tvd_close(int id)
{
	struct tvd_dev *dev = &tvd;
	int ret = 0;
	struct tvd_dmaqueue *active = &dev->vidq;
	struct tvd_dmaqueue *done = &dev->done;

	hal_mutex_lock(fliter_lock);
	tvd_dbg("%s:Enable _3D_FLITER:,used:%d\n", __func__,
			dev->fliter.used);
	if (dev->fliter.used) {
		__tvd_3d_comp_mem_free(dev);
		tvd_3d_mode(dev->sel, 0, 0);
	}
	hal_mutex_unlock(fliter_lock);

#ifdef CONFIG_KERNEL_FREERTOS
	memheap_detach(&tvd_mempool);
#else
	rt_memheap_detach(&tvd_mempool);
#endif

	tvd_deinit(id, dev->interface);

	tvd_stop_generating(dev);
	__tvd_clk_disable(dev);
	tvd_stop_opened(dev);
	tvd_dbg("Free tvd%d irq:%d\n", dev->sel, dev->irq);
	hal_free_irq(dev->irq);

	INIT_LIST_HEAD(&active->active);
	INIT_LIST_HEAD(&done->active);

	hal_mutex_delete(dev->stream_lock);
	hal_mutex_delete(dev->opened_lock);
	hal_mutex_delete(dev->buf_lock);
	hal_mutex_delete(fliter_lock);

	hal_waitqueue_head_deinit(&dev->tvd_waitqueue);

	return ret;
}

int vidioc_s_fmt_vid_cap(int id, struct vin_format *fmt)
{
	struct tvd_dev *dev = &tvd;
	int ret = 0;

	if (tvd_is_generating(dev)) {
		tvd_wrn("%s device busy\n", __func__);
		return -EBUSY;
	}

	if(!fmt->pixelformat) {
		fmt->pixelformat = V4L2_PIX_FMT_NV12;
	}

	dev->fmt = __get_format(dev, fmt->pixelformat);
	if (!dev->fmt) {
		tvd_wrn("Fourcc format (0x%08x) invalid.\n",
				fmt->pixelformat);
		return -EINVAL;
	}

	dev->width = fmt->win.width;
	dev->height = fmt->win.height;
	if (dev->height == 576) {
		dev->system = PAL;
		/* To solve the problem of PAL signal is not well.
		 * Accoding to the hardware designer, tvd need 29.7M
		 * clk input on PAL system, so here adjust clk again.
		 * Before this modify, PAL and NTSC have the same
		 * frequency which is 27M.
		 */
		__tvd_clk_init(dev);
	} else {
		dev->system = NTSC;
	}
	__tvd_config(dev);

	tvd_cagc_and_3d_config(dev);

	tvd_dbg("\ninterface=%d\nsystem=%s\nformat=%d\noutput_fmt=%s\n",
			dev->interface, (!dev->system) ? "NTSC" : "PAL", dev->format,
			(!dev->fmt->output_fmt) ? "YUV420" : "YUV422");
	tvd_dbg("\nwidth=%d\nheight=%d\ndev->sel=%d\n", dev->width, dev->height,
			dev->sel);

	return ret;
}

int vidioc_g_fmt_vid_cap(int id, struct vin_format *fmt)
{
	struct tvd_dev *dev = &tvd;
	u32 locked = 0, system = 2;

	__get_status(dev, &locked, &system);

	if (!locked) {
		tvd_dbg("%s: signal is not locked.\n", __func__);
		return -EAGAIN;
	} else {
		/* system: 1->pal, 0->ntsc */
		if (system == PAL) {
			dev->width = 720;
			dev->height = 576;
		} else if (system == NTSC) {
			dev->width = 720;
			dev->height = 480;
		} else {
			tvd_wrn("system is not sure.\n");
		}
	}

	fmt->win.width = dev->width;
	fmt->win.height = dev->height;

	tvd_wrn("system = %d, w = %d, h = %d\n", system, dev->width,
			dev->height);

	return 0;
}

int tvd_dqbuffer(int id, struct tvd_buffer **buf)
{
	int ret = 0;
	struct tvd_dev *dev = &tvd;
	struct tvd_dmaqueue *done = &dev->done;

	if (hal_wait_event_timeout(dev->tvd_waitqueue, list_empty(&done->active) == 0, 2000) <= 0) {
		tvd_wrn("dqbuf timeout!\n");
		return -1;
	}

	uint32_t __cpsr = hal_spin_lock_irqsave(&dev->slock);
	if (!list_empty(&done->active)) {
		*buf = list_first_entry(&done->active, struct tvd_buffer, list);
		list_del(&((*buf)->list));
	} else {
		ret = -1;
	}
	hal_spin_unlock_irqrestore(&dev->slock, __cpsr);

	return ret;
}

int tvd_qbuffer(int id, struct tvd_buffer *buf)
{
	struct tvd_dev *dev = &tvd;
	struct tvd_dmaqueue *vidq = &dev->vidq;
	int ret = 0;

	uint32_t __cpsr = hal_spin_lock_irqsave(&dev->slock);
	list_add_tail(&buf->list, &vidq->active);
	hal_spin_unlock_irqrestore(&dev->slock, __cpsr);

	return ret;
}

int tvd_vidioc_streamon(int video_id)
{
	struct tvd_dev *dev = &tvd;
	struct tvd_dmaqueue *dma_q = &dev->vidq;
	struct tvd_buffer *buf = NULL;
	int ret = 0;

	hal_mutex_lock(dev->stream_lock);
	if (tvd_is_generating(dev)) {
		tvd_wrn("stream has been already on\n");
		ret = -1;
		goto streamon_unlock;
	}

	dev->ms = 0;
	dma_q->frame = 0;

	if (!list_empty(&dma_q->active)) {
		buf = list_entry(dma_q->active.next, struct tvd_buffer, list);
	} else {
		tvd_wrn("stream on, but no buffer now.\n");
		goto streamon_unlock;
	}

	__tvd_set_addr(dev, buf);
	tvd_irq_status_clear(dev->sel, TVD_IRQ_FRAME_END);
	hal_enable_irq(dev->irq);
	tvd_irq_enable(dev->sel, TVD_IRQ_FRAME_END);
	tvd_capture_on(dev->sel);
	tvd_start_generating(dev);

streamon_unlock:
	hal_mutex_unlock(dev->stream_lock);

	return ret;
}

int tvd_vidioc_streamoff(int video_id)
{
	struct tvd_dev *dev = &tvd;
	struct tvd_dmaqueue *dma_q = &dev->vidq;
	struct tvd_dmaqueue *donelist = &dev->done;
	struct tvd_buffer *buffer;
	int ret = 0;
	uint32_t __cpsr;

	hal_mutex_lock(dev->stream_lock);

	if (!tvd_is_generating(dev)) {
		tvd_wrn("%s: stream has been already off\n", __func__);
		ret = 0;
		goto streamoff_unlock;
	}
	tvd_stop_generating(dev);
	dev->ms = 0;
	dma_q->frame = 0;
	donelist->frame = 0;

	tvd_irq_disable(dev->sel, TVD_IRQ_FRAME_END);
	tvd_irq_status_clear(dev->sel, TVD_IRQ_FRAME_END);
	tvd_capture_off(dev->sel);
	__cpsr = hal_spin_lock_irqsave(&dev->slock);

	while (!list_empty(&dma_q->active)) {
		buffer =
			list_first_entry(&dma_q->active, struct tvd_buffer, list);
		list_del(&buffer->list);
	}

	while (!list_empty(&donelist->active)) {
		buffer =
			list_first_entry(&donelist->active, struct tvd_buffer, list);
		list_del(&buffer->list);
	}

	hal_spin_unlock_irqrestore(&dev->slock, __cpsr);

streamoff_unlock:
	hal_mutex_unlock(dev->stream_lock);

	return ret;
}

static void (*tvd_buffer_done)(int id);
void tvd_register_buffer_done_callback(void *func)
{
	tvd_here;
	tvd_buffer_done = func;
}

// fix unused-function
__attribute__((__unused__)) static int tvd_wait_done_buf(void)
{
	int wait_cnt = 0;

	while(list_empty(&tvd.done.active)) {
		hal_msleep(1);
		wait_cnt++;
		if(wait_cnt >= 1000) {
			tvd_wrn("wait timeout!\n");
			return -1;
		}
	}

	return 0;
}

int tvd_force_reset_buffer(int video_id)
{
	int ret = 0;
	uint32_t __cpsr;
	struct tvd_dev *dev = &tvd;
	struct tvd_dmaqueue *dma_q = &dev->vidq;
	struct tvd_dmaqueue *donelist = &dev->done;
	struct tvd_buffer *buf;

	__cpsr = hal_spin_lock_irqsave(&dev->slock);

	while (!list_empty(&dma_q->active)) {
		buf =
			list_first_entry(&dma_q->active, struct tvd_buffer, list);
		list_del(&buf->list);
	}

	while (!list_empty(&donelist->active)) {
		buf =
			list_first_entry(&donelist->active, struct tvd_buffer, list);
		list_del(&buf->list);
	}

	hal_spin_unlock_irqrestore(&dev->slock, __cpsr);

	return ret;
}

static hal_irqreturn_t tvd_isr(void *priv)
{
	struct tvd_buffer *buf;
	struct tvd_dev *dev = (struct tvd_dev *)priv;
	struct tvd_dmaqueue *dma_q = &dev->vidq;
	struct tvd_dmaqueue *done = &dev->done;
	uint32_t __cpsr;
	u32 irq_status = 0;

	u32 err = (1 << TVD_IRQ_FIFO_C_O) | (1 << TVD_IRQ_FIFO_Y_O) |
		  (1 << TVD_IRQ_FIFO_C_U) | (1 << TVD_IRQ_FIFO_Y_U) |
		  (1 << TVD_IRQ_WB_ADDR_CHANGE_ERR);

	if (tvd_is_generating(dev) == 0) {
		tvd_irq_status_clear(dev->sel, TVD_IRQ_FRAME_END);
		return 0;
	}

	tvd_dma_irq_status_get(dev->sel, &irq_status);
	if ((irq_status & err) != 0) {
		tvd_dma_irq_status_clear_err_flag(dev->sel, err);
		tvd_wrn("tvd irq err!!!\n");
	}

	__cpsr = hal_spin_lock_irqsave(&dev->slock);

	if (0 == dev->first_flag) {
		dev->first_flag = 1;
		goto set_next_addr;
	}

	if (list_empty(&dma_q->active) ||
		dma_q->active.next->next == (&dma_q->active)) {
		tvd_dbg("No active queue to serve\n");
		goto unlock;
	}

	buf = list_entry(dma_q->active.next, struct tvd_buffer, list);
	list_del(&buf->list);
	list_add_tail(&buf->list, &done->active);
	hal_wake_up(&dev->tvd_waitqueue);

	if (list_empty(&dma_q->active)) {
		tvd_dbg("%s: No more free frame\n", __func__);
		goto unlock;
	}
	if ((&dma_q->active) == dma_q->active.next->next) {
		tvd_dbg("No more free frame on next time\n");
		goto unlock;
	}

set_next_addr:
	buf = list_entry(dma_q->active.next->next, struct tvd_buffer, list);
	__tvd_set_addr(dev, buf);

unlock:
	hal_spin_unlock_irqrestore(&dev->slock, __cpsr);
	tvd_irq_status_clear(dev->sel, TVD_IRQ_FRAME_END);

	return 0;
}


static int __tvd_probe_init(int sel)
{
	struct tvd_dev *dev = &tvd;
	int ret = 0;

	tvd_here;

	dev->id = sel;
	dev->sel = sel;
	dev->generating = 0;
	dev->opened = 0;
	dev->interface = CVBS_INTERFACE;
	dev->irq = SUNXI_IRQ_TVD;
	tvd_addr = TVD0_BASE;

	tvd_default_clk[TVD0_TOP_CLK].clock = hal_clock_get(HAL_SUNXI_CCU, tvd_default_clk[TVD0_TOP_CLK].clock_id);
	if (IS_ERR_OR_NULL(tvd_default_clk[TVD0_TOP_CLK].clock)) {
		tvd_wrn("get tvd0 clk error!\n");
		return -1;
	}

	tvd_default_clk[TVD0_MBUS_CLK].clock = hal_clock_get(HAL_SUNXI_CCU, tvd_default_clk[TVD0_MBUS_CLK].clock_id);
	if (IS_ERR_OR_NULL(tvd_default_clk[TVD0_MBUS_CLK].clock)) {
		tvd_wrn("get tvd0 bus clk error\n");
		return -1;
	}


	INIT_LIST_HEAD(&dev->vidq.active);

	dev->stream_lock = hal_mutex_create();
	dev->opened_lock = hal_mutex_create();
	dev->buf_lock = hal_mutex_create();
	hal_waitqueue_head_init(&dev->tvd_waitqueue);

	return ret;
}

static int tvd_probe(unsigned int id)
{
	int ret = 0;
	hal_clk_type_t clk_type = HAL_SUNXI_CCU;

	fliter_lock = hal_mutex_create();
	if (!fliter_lock) {
		tvd_wrn("fliter_lock mutex create fail\n");
		return -1;
	}

	tvd_top = TVD_TOP_BASE;

	tvd_default_clk[TVD_TOP_CLK].clock = hal_clock_get(clk_type, tvd_default_clk[TVD_TOP_CLK].clock_id);
	if (IS_ERR_OR_NULL(tvd_default_clk[TVD_TOP_CLK].clock)) {
		tvd_wrn("get tvd clk error!\n");
		return -1;
	}

	tvd_default_clk[TVD_MBUS_CLK].clock = hal_clock_get(clk_type, tvd_default_clk[TVD_MBUS_CLK].clock_id);
	if (IS_ERR_OR_NULL(tvd_default_clk[TVD_MBUS_CLK].clock)) {
		tvd_wrn("get tvd clk bus error\n");
		return -1;
	}

	tvd_default_rst_clk[TVD_TOP_RET].clock = hal_reset_control_get(tvd_default_rst_clk[TVD_TOP_RET].type,
																   tvd_default_rst_clk[TVD_TOP_RET].clock_id);
	if (IS_ERR_OR_NULL(tvd_default_rst_clk[TVD_TOP_RET].clock)) {
		tvd_wrn("get tvd top reset clk error\n");
		return -1;
	}


	tvd_default_rst_clk[TVD_RET].clock = hal_reset_control_get(tvd_default_rst_clk[TVD_RET].type,
															   tvd_default_rst_clk[TVD_RET].clock_id);
	if (IS_ERR_OR_NULL(tvd_default_rst_clk[TVD_RET].clock)) {
		tvd_wrn("get tvd top reset clk error\n");
		return -1;
	}

	tvd_default_clk[TVD0_TOP_CLK_SRC].clock =
		hal_clock_get(clk_type, tvd_default_clk[TVD0_TOP_CLK_SRC].clock_id);
	if (IS_ERR_OR_NULL(tvd_default_clk[TVD0_TOP_CLK_SRC].clock)) {
		tvd_wrn("get tvd0 src clk bus error\n");
		return -1;
	}

	//sun8iw20p1 only support 1ch input
	tvd_total_num = 1;
	tvd_row = 1;
	tvd_column = 1;

	ret = __tvd_probe_init(id);
	if (ret != 0) {
		/* one tvd may init fail because of the
			* sysconfig */
		tvd_dbg("tvd%d init is failed\n", id);
		ret = 0;
	}

//OUT:
	return ret;
}

// fix unused-function
__attribute__((__unused__)) static int tvd_release(void) /*fix*/
{
	return 0;
}

static void usage(void)
{
	printf("***************************************************\n");
	printf("** tvd_debug version  V1.0.0                     **\n");
	printf("** argv[1] test frame cnt                        **\n");
	printf("** argv[2] adc input chn 0: chn0 1: chn1         **\n");
	printf("** example: capture 10 frame on adc chn 0        **\n");
	printf("** tvd_debug 10 0                                **\n");
	printf("***************************************************\n");
}

int tvd_debug(int argc, const char **argv)
{
	int ret = 0, i = 0, input = 0;
	int test_cnt = 10;
	struct tvd_buffer *buf = NULL;
	struct vin_format fmt;

	usage();

	if(argv[1]) {
		test_cnt = atoi(argv[1]);
	}

	if(argv[2]) {
		input = atoi(argv[2]);
	}

	/*device open*/
	ret = tvd_open(0);
	if (ret) {
		tvd_wrn("tvd driver open failed\n");
		return -1;
	}

	/*device s_input*/
	ret = tvd_vidioc_s_input(input);
	if (ret) {
		tvd_wrn("tvd driver s_input failed\n");
		return -1;
	}

	/*device get fmt,judge siganl is lock?*/
	ret = vidioc_g_fmt_vid_cap(0, &fmt);
	if(ret) {
		tvd_wrn("g_fmt failed, signal not lock\n");
		tvd_close(0);
		return -1;
	} else {
		tvd_wrn("g_fmt success, width:%d height:%d \n", fmt.win.width,
			fmt.win.height);
	}

	/*device set fmt*/
	fmt.pixelformat = V4L2_PIX_FMT_NV12;
	ret = vidioc_s_fmt_vid_cap(0, &fmt);
	if (ret) {
		tvd_wrn("tvd s_fmt failed\n");
		return -1;
	}

	//alloc tvd buffer mem
	tvd_request_buf();

	tvd_vidioc_streamon(0);

	for(i = 0; i < test_cnt; i++) {
		tvd_dqbuffer(0, &buf);

		printf("get a frame cnt:%d \n", i);

		//do something
#if defined DISPLAY_FRAME
		preview_display_tmp_test((unsigned long)buf->paddr);
#endif
		tvd_qbuffer(0, buf);
	}

	tvd_vidioc_streamoff(0);

	//release tvd buffer mem
	tvd_release_buf();

	tvd_close(0);

	return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(tvd_debug, tvd_debug, Command);
