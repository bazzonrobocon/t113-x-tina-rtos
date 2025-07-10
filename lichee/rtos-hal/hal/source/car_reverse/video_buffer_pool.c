/*
 * Fast car reverse buffer manager module
 *
 * Copyright (C) 2015-2023 AllwinnerTech, Inc.
 *
 * Authors:  wujiayi <wujiayi@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "fastboot_com.h"
#include "buffer_pool.h"
#include "video_source.h"
#include "memheap.h"

static struct memheap ve_buffer_mempool;

void ve_buffer_pool_mem_init(void)
{
	memheap_init(&ve_buffer_mempool, "car-buffer-mempool", \
		(void *)CONFIG_BUFFER_POOL_VE_MEM_ADDR, CONFIG_BUFFER_POOL_VE_MEM_LEN);
}

void ve_buffer_mem_deinit(void)
{
	memheap_detach(&ve_buffer_mempool);
}

void *ve_buffer_pool_mem_alloc_align(size_t size, size_t align)
{
	unsigned int align_sizes;
	align_sizes = PAGE_ALIGN(size);
	return memheap_alloc_align(&ve_buffer_mempool, align_sizes, 0x1000);
}

void ve_buffer_pool_mem_free_align(void *ptr)
{
	memheap_free_align(ptr);
}


void rest_buffer_pool(struct buffer_pool *bp)
{
	int i;
	struct buffer_node *node;

	INIT_LIST_HEAD(&bp->head);
	for (i = 0; i < bp->depth; i++) {
		node = bp->pool[i];
		if (node)
			list_add(&node->list, &bp->head);
	}
}

void dump_buffer_pool(struct buffer_pool *bp)
{
	struct buffer_node *node;
	uint32_t __cpsr = 0;

	__cpsr = hal_spin_lock_irqsave(&bp->lock);
	list_for_each_entry(node, &bp->head, list) {
		//CAR_REVERSE_PRINT("buffer[%d]->dmabuf_fd: %d\n", i++, node->dmabuf_fd);
	}
	hal_spin_unlock_irqrestore(&bp->lock, __cpsr);
}
