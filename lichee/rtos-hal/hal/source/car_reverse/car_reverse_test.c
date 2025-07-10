/*
 * Fast car reverse driver module
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
#include <hal_log.h>
#include <hal_cmd.h>
#include <hal_thread.h>
#include "fastboot_com.h"

#define FREESIZE_LIST_MAX	(6)
#define LEAKS_BYTE_MAX		(1)
int freesize_list[FREESIZE_LIST_MAX];
int freesize_list_cnt = 0;

void car_reverse_check_memory_leaks(void)
{
	int i = 0;
	int diff = 0;
	int tmp_diff = 0;

	freesize_list[freesize_list_cnt] = xPortGetFreeHeapSize();
	if (++freesize_list_cnt >= FREESIZE_LIST_MAX) {
		diff = freesize_list[0] - freesize_list[FREESIZE_LIST_MAX - 1];
		if (diff < 0) {
			CAR_REVERSE_ERR("memory leak is negative ...\n");
			while(1);
		}

		if (diff >= LEAKS_BYTE_MAX) {
			CAR_REVERSE_ERR("possible memory leak, leaks byte %d\n", diff);
			CAR_REVERSE_ERR("freesize_list[0] = %d\n", freesize_list[0]);
			for (i = 1; i < FREESIZE_LIST_MAX; i++) {
				tmp_diff = freesize_list[i-1] - freesize_list[i];
				CAR_REVERSE_ERR("freesize_list[%d] = %d, leaks byte %d\n", i, freesize_list[i], tmp_diff);
			}
			CAR_REVERSE_ERR("code blocking ...\n");
			while(1);
		}
		freesize_list_cnt = 0;
	}
}
