/*
 * Fast car reverse image preview module
 *
 * Copyright (C) 2013-2022 AllwinnerTech, Inc.
 *
 * Authors:  Huangyongxing <huangyongxing@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __BUFFER_POOL_H__
#define __BUFFER_POOL_H__

#include "../vin/vin_video/vin_video.h"
#include "../tvd/tvd.h"

struct video_source_buffer {
	struct tvd_buffer tvd_buf;
	struct vin_buffer vin_buf;
	struct list_head list;
	struct buffer_node *node;
	struct device *dev;
	void *phy_address;
	unsigned int size;
};

struct buffer_node {
	struct list_head list;
	unsigned int size;
	void *phy_address;
	struct video_source_buffer vsbuf;
};

struct buffer_pool {
	hal_spinlock_t lock;
	struct list_head head;
	int depth;
	struct buffer_node **pool;
};

struct buffer_pool *alloc_buffer_pool(int depth, int buf_size);
struct buffer_node *buffer_node_alloc(int sizes);
struct buffer_node *buffer_pool_dequeue_buffer(struct buffer_pool *bp);
void buffer_pool_queue_buffer(struct buffer_pool *bp, struct buffer_node *node);
void buffer_pool_mem_init(void);
void buffer_mem_deinit(void);
void free_buffer_node(struct buffer_node *node);
void free_buffer_pool(struct buffer_pool *bp);
void *buffer_pool_mem_alloc_align(size_t size, size_t align);
void buffer_pool_mem_free_align(void *ptr);
#endif
