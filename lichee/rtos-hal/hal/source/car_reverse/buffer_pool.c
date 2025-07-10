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

static struct memheap car_buffer_mempool;

void buffer_pool_mem_init(void)
{
	memheap_init(&car_buffer_mempool, "car-buffer-mempool", \
		(void *)CONFIG_BUFFER_POOL_MEM_ADDR, CONFIG_BUFFER_POOL_MEM_LEN);
}

void buffer_mem_deinit(void)
{
	memheap_detach(&car_buffer_mempool);
}

void *buffer_pool_mem_alloc_align(size_t size, size_t align)
{
	return memheap_alloc_align(&car_buffer_mempool, size, align);
}

void buffer_pool_mem_free_align(void *ptr)
{
	memheap_free_align(ptr);
}

struct buffer_node *buffer_node_alloc(int sizes)
{
	struct buffer_node *node;
	unsigned int align_sizes;

	node = hal_malloc(sizeof(struct buffer_node));
	if (!node) {
		CAR_REVERSE_ERR("alloc failed\n");
		return ERR_PTR(-EINVAL);
	}

	align_sizes = PAGE_ALIGN(sizes);

	node->phy_address = memheap_alloc_align(&car_buffer_mempool, align_sizes, 0x1000);
	BUFFER_POOL_DBG("phy_addr 0x%lx, align_sizes 0x%x\n", (unsigned long)node->phy_address, align_sizes);
	if (!node->phy_address) {
		CAR_REVERSE_ERR("alloc failed\n");
		hal_free(node);
		return NULL;
	}
	node->size = align_sizes;

	return node;
}

void free_buffer_node(struct buffer_node *node)
{
	BUFFER_POOL_DBG("free phy_addr 0x%lx\n", (unsigned long)node->phy_address);
	memheap_free_align(node->phy_address);
	hal_free(node);
}

struct buffer_node *buffer_pool_dequeue_buffer(struct buffer_pool *bp)
{
	struct buffer_node *node = NULL;
	uint32_t __cpsr = 0;

	__cpsr = hal_spin_lock_irqsave(&bp->lock);
	if (!list_empty(&bp->head)) {
		node = list_entry(bp->head.next, struct buffer_node, list);
		list_del(&node->list);
	}
	hal_spin_unlock_irqrestore(&bp->lock, __cpsr);
	return node;
}

void buffer_pool_queue_buffer(struct buffer_pool *bp, struct buffer_node *node)
{
	uint32_t __cpsr = 0;

	__cpsr = hal_spin_lock_irqsave(&bp->lock);
	list_add_tail(&node->list, &bp->head);
	hal_spin_unlock_irqrestore(&bp->lock, __cpsr);
}

struct buffer_pool *alloc_buffer_pool(int depth, int buf_size)
{
	int i;
	struct buffer_pool *bp;
	struct buffer_node *node;

	bp = hal_malloc(sizeof(struct buffer_pool));
	if (!bp) {
		CAR_REVERSE_ERR("buffer pool alloc failed\n");
		goto _out;
	}

	bp->depth = depth;
	bp->pool = hal_malloc(sizeof(struct buffer_node *) * depth);
	if (!bp->pool) {
		CAR_REVERSE_ERR("buffer node alloc failed\n");
		hal_free(bp);
		bp = NULL;
		goto _out;
	}
	hal_spin_lock_init(&bp->lock);
	BUFFER_POOL_DBG("\n");

	/* alloc memory for buffer node */
	INIT_LIST_HEAD(&bp->head);
	for (i = 0; i < depth; i++) {
		node = buffer_node_alloc(buf_size);
		if (node) {
			list_add(&node->list, &bp->head);
			bp->pool[i] = node;
		}
	}

_out:

	return bp;
}

void free_buffer_pool(struct buffer_pool *bp)
{

	uint32_t __cpsr = 0;
	struct buffer_node *node;

	__cpsr = hal_spin_lock_irqsave(&bp->lock);
	BUFFER_POOL_DBG("\n");
	while (!list_empty(&bp->head)) {
		node = list_entry(bp->head.next, struct buffer_node, list);
		list_del(&node->list);
		free_buffer_node(node);
	}
	hal_spin_unlock_irqrestore(&bp->lock, __cpsr);

	hal_free(bp->pool);
	hal_free(bp);
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
