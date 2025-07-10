/*
 * drivers/media/platform/sunxi-tvd/tvd/tvd.h
 *
 * Copyright (c) 2007-2018 Allwinnertech Co., Ltd.
 * Author: zhengxiaobin <zhengxiaobin@allwinnertech.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __TVD__H__
#define __TVD__H__

#include "bsp_tvd.h"
#include "vin_common.h"
#include <aw_list.h>
#include <hal_osal.h>
#include <hal_queue.h>
#include <hal_sem.h>
#include <hal_mutex.h>
#include <hal_log.h>
#include <hal_workqueue.h>
#include <hal_atomic.h>
#include <hal_waitqueue.h>

#define MAX_CSI_CAM_NUM 2
#define dma_addr_t	unsigned long
#define PAGE_ALIGN(x) (((x) + (4095)) & ~(4095))

#define IS_ERR_OR_NULL(pointer) (pointer == NULL)

#define EAGAIN					11
#define	ENOMEM					12
#define EBUSY					16
#define EINVAL 					22


#define V4L2_MODE_VIDEO			0x0002	/*  For video capture */

enum hpd_status {
	STATUE_CLOSE = 0,
	STATUE_OPEN  = 1,
};

struct tvd_fmt {
	u8              name[32];
	u32           	fourcc;          /* v4l2 format id */
	TVD_FMT_T    	output_fmt;
	int   	        depth;
	int    bus_pix_code;
	unsigned char   planes_cnt;
};

enum tvd_interface {
	CVBS,
	YPBPRI,
	YPBPRP,
};

/* buffer for one video frame */
struct tvd_buffer {
	struct list_head	list;
	struct tvd_fmt		*fmt;
	void *paddr;
};

struct tvd_dmaqueue {
	struct list_head  active;
	/* Counters to control fps rate */
	int               frame;
};

struct writeback_addr {
	dma_addr_t	y;
	dma_addr_t	c;
};

struct tvd_status {
	int tvd_used;
	int tvd_opened;
	int tvd_if;
	int tvd_3d_used;
	int locked;
	int tvd_system;
	int tvd_clk_enabled;
};

struct tvd_3d_fliter {
	int used;
	int size;
	void *phy_address;
};

struct tvd_dev {
	int			sel;
	int			id;
	int			first_flag;
	hal_spinlock_t              slock;

	/* Several counters */
	unsigned     			ms;

	/* Input Number */
	int	                input;

	/* video capture */
	struct tvd_fmt          *fmt;
	unsigned int            width;
	unsigned int            height;
	unsigned int			frame_size;
	unsigned int            capture_mode;

	unsigned int			interface; /*0 cvbs,1 ypbprI,2 ypbprP*/
	unsigned int            system;	/*0 ntsc, 1 pal*/
	unsigned int		row;
	unsigned int		column;

	unsigned int            locked;	/* signal is stable or not */
	unsigned int            format;
	int			irq;

	/* working state */
	unsigned long 	        generating;
	unsigned long		opened;

	struct writeback_addr	wb_addr;

	/* tvd revelent para */
	unsigned int            luma;
	unsigned int            contrast;
	unsigned int            saturation;
	unsigned int            hue;

	unsigned int            uv_swap;
	struct tvd_3d_fliter fliter;
	//struct early_suspend early_suspend;
	unsigned int 		para_from;
#define TVD_BUF_NUM 3
	struct tvd_buffer tvd_buf[TVD_BUF_NUM];
	hal_mutex_t 	buf_lock;
	hal_mutex_t              stream_lock;
	hal_mutex_t		  		opened_lock;
	struct tvd_dmaqueue       vidq;
	struct tvd_dmaqueue       done;

	hal_waitqueue_head_t tvd_waitqueue;
};

#define SUNXI_TVD_DEBUG_LEVEL 0


#define tvd_wrn(fmt, ...)                                                     \
	printf("[tvd] %s:%d " fmt "", __func__, __LINE__, ##__VA_ARGS__)

#if SUNXI_TVD_DEBUG_LEVEL == 2
#define tvd_here \
	printf("[tvd] %s:%d", __func__, __LINE__);
#else
#define tvd_here
#endif

#if SUNXI_TVD_DEBUG_LEVEL >= 1
#define tvd_dbg(fmt, ...)                                                     \
	printf("[tvd] %s:%d " fmt "", __func__, __LINE__, ##__VA_ARGS__)
#else
#define tvd_dbg(fmt, ...)
#endif

#endif /* __TVD__H__ */

