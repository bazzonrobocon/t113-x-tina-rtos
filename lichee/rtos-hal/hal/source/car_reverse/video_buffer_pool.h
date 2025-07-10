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

#ifndef __VE_BUFFER_POOL_H__
#define __VE_BUFFER_POOL_H__

void ve_buffer_pool_mem_init(void);
void ve_buffer_mem_deinit(void);
void *ve_buffer_pool_mem_alloc_align(size_t size, size_t align);
void ve_buffer_pool_mem_free_align(void *ptr);

#endif
