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
#include <hal_gpio.h>
#include <hal_cmd.h>
#include <hal_thread.h>
#include <hal_timer.h>
#include "buffer_pool.h"
#include "preview_display.h"
#include "preview_enhancer.h"
#include "video_source.h"
#include "car_reverse_amp.h"
#include "car_reverse_test.h"
//#include <rp_decode.h>
#include <string.h>
#ifdef CONFIG_ARCH_RISCV_PMP
#include <pmp.h>
#endif

#include "fastboot_video.h"

void ve_parse_config(struct preview_params *params)
{

}
