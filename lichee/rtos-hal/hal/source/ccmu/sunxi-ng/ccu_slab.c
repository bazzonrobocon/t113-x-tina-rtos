#include <stdlib.h>
#include <string.h>
#include <sunxi_hal_common.h>
#include "ccu_slab.h"

struct ccu_slab *ccu_slab_init(int size, int num, char *name)
{
	struct ccu_slab *slab;

	slab = (struct ccu_slab *)malloc(sizeof(*slab));
	if (!slab) {
		printf("[ccu_slab] init:slab malloc fail\n");
		return NULL;
	}
	memset(slab, 0, sizeof(*slab));

	strncpy(slab->name, name, CCU_SLAB_NAME_LEN_MAX - 1);
	slab->name[CCU_SLAB_NAME_LEN_MAX - 1] = '\0';

	slab->addr = malloc(size * num);
	if (!slab->addr) {
		printf("[ccu_slab] init:slab_addr malloc fail\n");
		return NULL;
	}
	memset(slab->addr, 0, size * num);

	slab->size = size;
	slab->num = num;
	slab->idx = 0;

	return slab;
}

void *ccu_malloc(struct ccu_slab *slab, int num)
{
	void *addr;

	if (!slab) {
		printf("[ccu_slab] malloc:ccu_slab is NULL!\n");
		return NULL;
	}

	if (slab->idx >= slab->num) {
		printf("[ccu_slab] [%s] out of mem!\n", slab->name);
		return NULL;
	}

	addr = slab->addr + slab->idx * slab->size;
	slab->idx += num;

	return addr;
}

void ccu_slab_deinit(struct ccu_slab *slab)
{
	if (!slab) {
		printf("[ccu_slab] free:ccu_slab is NULL!\n");
		return;
	}

	free(slab->addr);
	slab->size = 0;
	slab->num = 0;
	slab->idx = 0;
	free(slab);

	return;
}

