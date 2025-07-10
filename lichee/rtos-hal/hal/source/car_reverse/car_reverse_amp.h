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

#ifndef __CAR_REVERSE_AMP_H__
#define __CAR_REVERSE_AMP_H__

struct car_reverse_packet {
	int magic;
	int type;
	int car_status;
	int arm_car_reverse_status;
	int rv_car_status;
};

enum car_reverse_amp_packet_type {
	TPYE_NULL = 0,
	SYNC_CAR_REVERSE_STATUS = 1,
	ARM_TRY_GET_CAR_REVERSE_CRTL = 2,
	PREVIEW_START_ACK = 3,
	PREVIEW_STOP = 4,
	PREVIEW_STOP_ACK =5,
};

int car_reverse_amp_rpmsg_init(void);
int car_reverse_amp_rpmsg_send(struct car_reverse_packet *pack);

#endif
