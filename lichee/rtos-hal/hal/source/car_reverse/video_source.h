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

#ifndef __VIDEO_SOURCE_H__
#define __VIDEO_SOURCE_H__

#include "buffer_pool.h"
#include "fastboot_com.h"
#include "vin_common.h"
#include <hal_cmd.h>


#define VIDEO_SRC_TVD		(1)
#define VIDEO_SRC_VIRTUAL	(2)
#define VIDEO_SRC_VIN		(3)


#define ERR_PTR(x)			((void *)(x))
#define ERR_PTR(x)			((void *)(x))
#define IS_ERR(x)			(IS_ERR_VALUE((unsigned long)x))

enum video_packet_type {
	RV_VIN_START,
	RV_VIN_START_ACK,
	RV_VIN_STOP,
	RV_VIN_STOP_ACK,

	ARM_VIN_START,
	ARM_VIN_START_ACK,
	ARM_VIN_STOP,
	ARM_VIN_STOP_ACK,
};

enum msg_type {
	RV_GET_CONTROLL,
	ARM_GET_CONTROLL,
};

enum video_control {
	GET_BY_LINUX,
	GET_BY_RTOS,
	GET_BY_NONE,
};

struct car_reverse_video_source {
	char *type;
	int (*video_open)(int video_id);
	int (*video_close)(int video_id);
	int (*video_info)(void);
	int (*video_streamon)(int video_id);
	int (*video_streamoff)(int video_id);
	int (*queue_buffer)(int video_id, struct video_source_buffer *buf);
	int (*dequeue_buffer)(int video_id, struct video_source_buffer **buf);
	int (*force_reset_buffer)(int video_id);
	int (*video_set_format)(int video_id, struct vin_format *fmt);
	int (*video_get_format)(int video_id, struct vin_format *fmt);
	void (*car_reverse_callback)(int video_id);
	int (*video_rpmsg_init)(void);
	int (*video_rpmsg_status_on_off)(enum video_packet_type type);
	int (*video_rpmsg_get_status)(void);
	int (*video_source_rpmsg_sem_timewait)(int ms);
};

int car_reverse_video_source_register(struct car_reverse_video_source *ops);
int car_reverse_video_source_unregister(struct car_reverse_video_source *ops);
int video_source_connect(struct preview_params *params, int video_id);
int video_source_disconnect(int video_id);
int video_source_streamon(int video_id);
int video_source_streamoff(int video_id);
int video_source_streamon_vin(int video_id);
int video_source_streamoff_vin(int video_id);
int video_source_force_reset_buffer(int video_id);
int video_source_select(int source_type);
int video_source_queue_buffer(struct buffer_node *node, int video_id);
struct buffer_node *video_source_dequeue_buffer(int video_id);
int video_source_rpmsg_status_on_off(enum video_packet_type type);
int video_source_rpmsg_get_status(void);
int video_source_rpmsg_init();
int video_source_rpmsg_sem_timewait(int ms);
void car_reverse_callback_register(void *func);
void video_source_release(void);
void car_reverse_video_source_init(void);

#endif