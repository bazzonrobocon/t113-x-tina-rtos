/*
 * Fast car reverse video source module
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "video_source.h"
#include "buffer_pool.h"
#include "fastboot_com.h"
#include "preview_display.h"

#define VS_MAX 256

struct car_reverse_video_source *g_vsource[VS_MAX];
struct car_reverse_video_source *video_source;
struct buffer_node *t_node;
int video_source_cnt;

void car_reverse_display_update(int id)
{
	return;
}

struct buffer_node *video_source_dequeue_buffer(int video_id)
{
	int retval = 0;
	struct video_source_buffer *vsbuf;
	struct buffer_node *node = NULL;

	if (!video_source || !video_source->dequeue_buffer) {
		CAR_REVERSE_ERR("Invalid video source function: dequeue_buffer!\n");
		goto node_null;
	}

	retval = video_source->dequeue_buffer(video_id, &vsbuf);

	if (retval < 0) {
		VIDEO_SOURCE_DBG("dequeue_buffer failed, buffer maybe not ready!\n");
		goto node_null;
	}

	if (IS_ERR_OR_NULL(vsbuf)) {
		CAR_REVERSE_ERR("video source buffer not found!\n");
		goto node_null;
	}

	node = container_of(vsbuf, struct buffer_node, vsbuf);
	VIDEO_SOURCE_DBG("video_id: %d, node->phy_address = %lx\n", \
					 video_id, (unsigned long)node->phy_address);

node_null:
	return node;

}

int video_source_queue_buffer(struct buffer_node *node, int video_id)
{
	struct video_source_buffer *vsbuf;

	if (!video_source || !video_source->queue_buffer) {
		CAR_REVERSE_ERR("Invalid video source function: queue_buffer!\n");
		return -EINVAL;
	}

	VIDEO_SOURCE_DBG("video_id: %d, node->phy_address = %lx\n", \
					 video_id, (unsigned long)node->phy_address);

	vsbuf = &node->vsbuf;

	vsbuf->phy_address = node->phy_address;
	vsbuf->tvd_buf.paddr = node->phy_address;

	return video_source->queue_buffer(video_id, vsbuf);
}

int video_source_info(int video_id)
{
	if (!video_source) {
		CAR_REVERSE_ERR("Invalid video source function: video_open!\n");
		return -EINVAL;
	}

	if (video_source->video_info)
		return video_source->video_info();
	else
		return 0;
}

int video_source_open(int video_id)
{
	if (!video_source || !video_source->video_open) {
		CAR_REVERSE_ERR("Invalid video source function: video_open!\n");
		return -EINVAL;
	}

	VIDEO_SOURCE_DBG("video_id: %d\n", video_id);
	return video_source->video_open(video_id);
}

int video_source_close(int video_id)
{
	if (!video_source || !video_source->video_close) {
		CAR_REVERSE_ERR("Invalid video source function: video_close!\n");
		return -EINVAL;
	}

	VIDEO_SOURCE_DBG("video_id: %d\n", video_id);
	return video_source->video_close(video_id);
}

int video_source_set_format(int video_id, struct vin_format *fmt)
{
	if (!video_source || !video_source->video_set_format) {
		CAR_REVERSE_ERR("Invalid video source function: video_set_format!\n");
		return -EINVAL;
	}

	VIDEO_SOURCE_DBG("video_id: %d\n", video_id);
	return video_source->video_set_format(video_id, fmt);
}

int video_source_get_format(int video_id, struct vin_format *fmt)
{
	if (!video_source || !video_source->video_get_format) {
		CAR_REVERSE_ERR("Invalid video source function: video_get_format!\n");
		return -EINVAL;
	}

	VIDEO_SOURCE_DBG("video_id: %d\n", video_id);
	return video_source->video_get_format(video_id, fmt);
}

int video_source_streamon(int video_id)
{
	if (!video_source || !video_source->video_streamon) {
		CAR_REVERSE_ERR("Invalid video source function: video_streamon!\n");
		return -EINVAL;
	}

	VIDEO_SOURCE_DBG("video_id: %d\n", video_id);
	return video_source->video_streamon(video_id);
}

int video_source_streamoff(int video_id)
{
	if (!video_source || !video_source->video_streamoff) {
		CAR_REVERSE_ERR("Invalid video source function: video_streamoff!\n");
		return -EINVAL;
	}

	VIDEO_SOURCE_DBG("video_id: %d\n", video_id);
	return video_source->video_streamoff(video_id);
}

int video_source_select(int source_type)
{
	char source_name[10];
	int i;

	if (source_type == 1)
		sprintf(source_name, "tvd");
	else if (source_type == 2)
		sprintf(source_name, "virtual");
	else
		sprintf(source_name, "vin");

	for (i = 0; i < video_source_cnt; i++) {
		if (!strcmp(source_name, g_vsource[i]->type)) {
			CAR_REVERSE_INFO("video source match:%s!\n", g_vsource[i]->type);
			video_source = g_vsource[i];
			return 0;
		}
	}

	CAR_REVERSE_ERR("video source: %s is not registered yet!\n", source_name);
	return -1;
}

void video_source_release(void)
{
	video_source = NULL;
}


int car_reverse_video_source_register(struct car_reverse_video_source *video_source_register)
{
	if (video_source_cnt < VS_MAX) {
		video_source_register->car_reverse_callback = car_reverse_display_update;
		g_vsource[video_source_cnt] = video_source_register;
		video_source = video_source_register;
		video_source_cnt++;
	} else
		CAR_REVERSE_ERR("video source overflow!\n");

	return 0;
}

int car_reverse_video_source_unregister(struct car_reverse_video_source *video_source_register)
{
	int i;

	for (i = 0; i < video_source_cnt; i++) {
		if (!strcmp(video_source_register->type, g_vsource[i]->type)) {
			CAR_REVERSE_INFO("video source unregister:%s!\n", g_vsource[i]->type);
			g_vsource[i] = NULL;
			video_source = NULL;
			return 0;
		}
	}

	return 0;
}

#if defined CONFIG_CAR_REVERSE_SUPPORT_VIN
extern int vin_open_special(int id);
extern int vin_close_special(int id);
extern int vin_dqbuffer_special(int id, struct vin_buffer **vsbuf);
extern int vin_qbuffer_special(int id, struct vin_buffer *vsbuf);
extern int vin_g_fmt_special(int id, struct vin_format *f);
extern int vin_g_fmt_special_ext(int id, struct vin_format *f);
extern int vin_s_fmt_special(int id, struct vin_format *f);
extern int vin_s_input_special(int id, int i);
extern int vin_streamoff_special(int video_id);
extern int vin_streamon_special(int video_id);
extern int vin_force_reset_buffer(int video_id);
extern void vin_register_buffer_done_callback(int id, void *func);
extern int vin_rpmsg_status_on_off(enum video_packet_type type);
extern int rpmsg_vin_get_status(void);
extern int vin_rpmsg_init(void);
extern int vin_rpmsg_sem_timewait(int ms);

static int vin_video_open(int id)
{
	int ret = -1;

	ret = vin_open_special(id);
	if (ret)
		return ret;

	vin_register_buffer_done_callback(id, car_reverse_display_update);

	return 0;
}

static int vin_video_qbuffer(int id, struct video_source_buffer *vsbuf)
{
	struct vin_buffer *vin_buf = &vsbuf->vin_buf;

	vin_buf->phy_addr = vsbuf->phy_address;

	return vin_qbuffer_special(id, vin_buf);
}

static int vin_video_dqbuffer(int id, struct video_source_buffer **vsbuf)
{
	struct vin_buffer *vin_buf = NULL;
	int ret  = 0;

	ret = vin_dqbuffer_special(id, &vin_buf);
	if (ret < 0) {
		VIDEO_SOURCE_DBG("vin dequeue buffer fail!\n");
		return ret;
	}

	*vsbuf = container_of(vin_buf, struct video_source_buffer, vin_buf);
	if (IS_ERR_OR_NULL(*vsbuf)) {
		CAR_REVERSE_ERR("video source buffer not found!\n");
		return -ENOMEM;
	}

	return 0;
}

static int vin_video_get_fmt(int id, struct vin_format *f)
{
	vin_s_input_special(id, 0);

	return vin_g_fmt_special_ext(id, f);
}

static int vin_video_streamon(int id)
{
	return vin_streamon_special(id);
}

static int vin_video_streamoff(int id)
{
	return vin_streamoff_special(id);
}


struct car_reverse_video_source video_source_vin = {
	.type = "vin",
	.video_open = vin_video_open,
	.video_close = vin_close_special,
	.video_streamon = vin_video_streamon,
	.video_streamoff = vin_video_streamoff,
	.queue_buffer = vin_video_qbuffer,
	.dequeue_buffer = vin_video_dqbuffer,
	.video_set_format = vin_s_fmt_special,
	.video_get_format = vin_video_get_fmt,
	.force_reset_buffer = vin_force_reset_buffer,
	.video_rpmsg_init = vin_rpmsg_init,
	.video_rpmsg_status_on_off = vin_rpmsg_status_on_off,
	.video_rpmsg_get_status = rpmsg_vin_get_status,
	.video_source_rpmsg_sem_timewait = vin_rpmsg_sem_timewait,
};
#endif

#if defined CONFIG_CAR_REVERSE_SUPPORT_TVD
extern int tvd_open(int id);
extern int tvd_close(int id);
extern int tvd_vidioc_s_input(unsigned int i);
extern int tvd_dqbuffer(int id, struct tvd_buffer **vsbuf);
extern int tvd_qbuffer(int id, struct tvd_buffer *vsbuf);
extern int vidioc_s_fmt_vid_cap(int id, struct vin_format *fmt);
extern int vidioc_g_fmt_vid_cap(int id, struct vin_format *fmt);
extern int tvd_vidioc_streamoff(int video_id);
extern int tvd_vidioc_streamon(int video_id);
extern int tvd_force_reset_buffer(int video_id);
extern void tvd_register_buffer_done_callback(int id, void *func);
extern int tvd_info(void);
extern int tvd_rpmsg_init();
extern int rpmsg_tvd_remote_call(enum video_packet_type type);
extern int tvd_get_rpmsg_control(void);
extern int tvd_rpmsg_sem_timewait(int ms);

static int tvd_video_open(int id)
{
	int ret = -1;

	ret = tvd_open(id);
	if (ret)
		return ret;

	ret = tvd_vidioc_s_input(0);
	if (ret)
		return ret;

	tvd_register_buffer_done_callback(id, car_reverse_display_update);

	return 0;
}

static int tvd_video_qbuffer(int id, struct video_source_buffer *vsbuf)
{
	struct tvd_buffer *tvd_buf = &vsbuf->tvd_buf;

	return tvd_qbuffer(id, tvd_buf);
}

static int tvd_video_dqbuffer(int id, struct video_source_buffer **vsbuf)
{
	struct tvd_buffer *tvd_buf = NULL;
	int ret  = 0;

	ret = tvd_dqbuffer(id, &tvd_buf);
	if (ret < 0) {
		VIDEO_SOURCE_DBG("vin dequeue buffer fail!\n");
		return ret;
	}

	*vsbuf = container_of(tvd_buf, struct video_source_buffer, tvd_buf);
	if (IS_ERR_OR_NULL(*vsbuf)) {
		CAR_REVERSE_ERR("video source buffer not found!\n");
		return -ENOMEM;
	}

	return 0;
}

struct car_reverse_video_source video_source_tvd = {
	.type = "tvd",
	.video_open = tvd_video_open,
	.video_close = tvd_close,
	.video_info = tvd_info,
	.video_streamon = tvd_vidioc_streamon,
	.video_streamoff = tvd_vidioc_streamoff,
	.queue_buffer = tvd_video_qbuffer,
	.dequeue_buffer = tvd_video_dqbuffer,
	.video_set_format = vidioc_s_fmt_vid_cap,
	.video_get_format = vidioc_g_fmt_vid_cap,
	.force_reset_buffer = tvd_force_reset_buffer,
	.video_rpmsg_init = tvd_rpmsg_init,
	.video_rpmsg_status_on_off = rpmsg_tvd_remote_call,
	.video_rpmsg_get_status = tvd_get_rpmsg_control,
	.video_source_rpmsg_sem_timewait = tvd_rpmsg_sem_timewait,
};
#endif


void car_reverse_video_source_init(void)
{
#if defined CONFIG_CAR_REVERSE_SUPPORT_VIN
	car_reverse_video_source_register(&video_source_vin);
#endif

#if defined CONFIG_CAR_REVERSE_SUPPORT_TVD
	car_reverse_video_source_register(&video_source_tvd);
#endif
}

static int video_source_format_setting(struct preview_params *params, int video_id)
{
	struct vin_format format;
	struct vin_format format_prew;

	memset(&format, 0, sizeof(struct vin_format));
	memset(&format_prew, 0, sizeof(struct vin_format));

	video_source_get_format(video_id, &format);

	/* src size will be adjust by video source when src_size_adaptive enable */
	if (params->src_size_adaptive) {
		params->src_width  = format.win.width;
		params->src_height = format.win.height;
	}

	format_prew.pixelformat = params->format;
	format_prew.win.width  = params->src_width;
	format_prew.win.height = params->src_height;

	if (video_source_set_format(video_id, &format_prew) != 0) {
		CAR_REVERSE_ERR("video source set format error!\n");
		return -1;
	}

	VIDEO_SOURCE_DBG("format_got:%d, %dx%d\n", format_prew.pixelformat, \
					 format_prew.win.width, format_prew.win.height);

	return 0;
}

int video_source_connect(struct preview_params *params, int video_id)
{
	int ret = 0;

	ret = video_source_open(video_id);
	if (ret <  0) {
		CAR_REVERSE_ERR("video_source_open fail!\n");
		return ret;
	}

	ret = video_source_format_setting(params, video_id);
	if (ret < 0) {
		CAR_REVERSE_ERR("video_source_format_setting fail!\n");
		video_source_close(video_id);
		return ret;
	}

	return 0;
}

int video_source_disconnect(int video_id)
{
	return video_source_close(video_id);
}

int video_source_force_reset_buffer(int video_id)
{
	return video_source->force_reset_buffer(video_id);
}

int video_source_rpmsg_status_on_off(enum video_packet_type type)
{
	return video_source->video_rpmsg_status_on_off(type);
}

int video_source_rpmsg_get_status(void)
{
	return video_source->video_rpmsg_get_status();
}

int video_source_rpmsg_init()
{
	return video_source->video_rpmsg_init();
}

int video_source_rpmsg_sem_timewait(int ms)
{
	return video_source->video_source_rpmsg_sem_timewait(ms);
}

#define BUFFER_CNT 3
struct buffer_node *node[BUFFER_CNT];
static int video_source_request_buf(struct preview_params *params, int video_id)
{
	unsigned long size;
	int buf_idx = 0;

	size = params->src_width * params->src_height * 3 / 2;

	for (buf_idx = 0; buf_idx < BUFFER_CNT; buf_idx++) {
		node[buf_idx] = (struct buffer_node *)hal_malloc(sizeof(struct buffer_node));
		if(!node[buf_idx]) {
			VIDEO_SOURCE_DBG("buf_idx:%d fail!\n", buf_idx);
			return -1;
		}

		node[buf_idx]->phy_address = hal_malloc_coherent(size);
		if(!node[buf_idx]->phy_address) {
			VIDEO_SOURCE_DBG("buf_idx:%d fail!\n", buf_idx);
			return -1;
		}

		video_source_queue_buffer(node[buf_idx], video_id);
		VIDEO_SOURCE_DBG("%d %lx %ld\n", __LINE__, (unsigned long)node[buf_idx]->phy_address, size);
	}

	return 0;
}

static int video_source_release_buf(struct preview_params *params)
{
	unsigned long size;
	int buf_idx = 0;

	size = params->src_width * params->src_height * 3 / 2;

	for (buf_idx = 0; buf_idx < BUFFER_CNT; buf_idx++) {
		if(node[buf_idx]->phy_address) {
			hal_free_coherent(node[buf_idx]->phy_address);
		}
		if(node[buf_idx]) {
			hal_free(node[buf_idx]);
		}

		VIDEO_SOURCE_DBG("%d %lx %ld\n", __LINE__, (unsigned long)node[buf_idx]->phy_address, size);
	}

	return 0;
}

int video_source_debug(int argc, const char **argv)
{
	int ret, test_cnt = 10, i = 0;
	struct buffer_node *node = NULL;

	car_reverse_video_source_init();
#if defined CONFIG_CAR_REVERSE_SUPPORT_TVD
	ret = video_source_select(VIDEO_SRC_TVD); //tvd
#elif defined CONFIG_CAR_REVERSE_SUPPORT_VIN
	ret = video_source_select(VIDEO_SRC_VIN); //vin
#else
	ret = video_source_select(VIDEO_SRC_VIRTUAL); //virtual
#endif
	if(ret) {
		VIDEO_SOURCE_DBG("%d error!\n", __LINE__);
		return -1;
	}

	video_source_rpmsg_init();

	struct preview_params params;
	params.src_size_adaptive = 1;
	params.format = V4L2_PIX_FMT_NV12;
	params.src_width = 720;
	params.src_height = 480;

	ret = video_source_connect(&params, 0);
	if(ret) {
		VIDEO_SOURCE_DBG("%d connect error!\n", __LINE__);
		return -1;
	}

	ret = video_source_request_buf(&params, 0);
	if(ret) {
		VIDEO_SOURCE_DBG("%d request_buf error!\n", __LINE__);
		return -1;
	}

	ret = video_source_streamon(0);
	if(ret) {
		VIDEO_SOURCE_DBG("%d streamon error!\n", __LINE__);
		return -1;
	}

	video_source_rpmsg_status_on_off(RV_VIN_START);

	for(i = 0 ; i < test_cnt; i++) {
		node = video_source_dequeue_buffer(0);
		if (!node) {
			VIDEO_SOURCE_DBG("%d dequeue_buffer error!\n", __LINE__);
			goto out;
		}

		printf("get a frame cnt:%d\n", i);
		/* use display api */
		preview_display_tmp_test((unsigned long)node->phy_address);

		ret = video_source_queue_buffer(node, 0);
		if(ret) {
			VIDEO_SOURCE_DBG("%d queue_buffer error!\n", __LINE__);
			goto out;
		}
	}

out:
	ret = video_source_streamoff(0);
	if(ret) {
		VIDEO_SOURCE_DBG("%d streamoff error!\n", __LINE__);
		return -1;
	}

	video_source_rpmsg_status_on_off(RV_VIN_STOP);

	ret = video_source_force_reset_buffer(0);
	if(ret) {
		VIDEO_SOURCE_DBG("%d reset_buffer error!\n", __LINE__);
		return -1;
	}

	ret = video_source_release_buf(&params);
	if(ret) {
		VIDEO_SOURCE_DBG("%d release_buf error!\n", __LINE__);
		return -1;
	}

	ret = video_source_disconnect(0);
	if(ret) {
		VIDEO_SOURCE_DBG("%d disconnect error!\n", __LINE__);
		return -1;
	}

	return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(video_source_debug, video_source_debug, Command);
