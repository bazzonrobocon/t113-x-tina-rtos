/*
 * Fast car reverse image preview module
 *
 * Copyright (C) 2015-2023 AllwinnerTech, Inc.
 *
 * Authors:  Huangyongxing <huangyongxing@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __CAR_REVERSE_H__
#define __CAR_REVERSE_H__
#include <sunxi_hal_common.h>
#include "fastboot_com.h"
int car_reverse_auxiliary_line_buffer_alloc(struct fastboot_private_data * car);
int car_reverse_preview_start(struct fastboot_private_data * car);
int car_reverse_preview_stop(struct fastboot_private_data * car);
int car_reverse_preview_hold(struct fastboot_private_data * car);

#define SWAP_BUFFER_CNT 		(CONFIG_BUFFER_POOL_SWAP_BUFFER_CNT)

#endif
