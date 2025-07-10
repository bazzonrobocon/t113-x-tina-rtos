/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 lvda@allwinnertech.com
 */

#ifndef _CCU_SLAB_H_
#define _CCU_SLAB_H_

#define CCU_SLAB_NAME_LEN_MAX	10
struct ccu_slab {
	char name[CCU_SLAB_NAME_LEN_MAX];
	void *addr;
	int size;
	int num;
	int idx;
};

struct ccu_slab *ccu_slab_init(int size, int num, char *name);
void ccu_slab_deinit(struct ccu_slab *slab);
void *ccu_malloc(struct ccu_slab *slab, int num);

#endif
