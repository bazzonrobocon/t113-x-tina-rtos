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
#include "fastboot_com.h"
#include "car_reverse.h"
#include "buffer_pool.h"
#include "preview_display.h"
#include "preview_enhancer.h"
#include "video_source.h"
#include "car_reverse_amp.h"
#include "car_reverse_test.h"

#include <string.h>
#ifdef CONFIG_ARCH_RISCV_PMP
#include <pmp.h>
void car_reverse_mem_set_pmp(void)
{
	pmp_add_region(CONFIG_BUFFER_POOL_MEM_ADDR, \
			(CONFIG_BUFFER_POOL_MEM_ADDR + CONFIG_BUFFER_POOL_MEM_LEN), \
			PMP_R | PMP_W);
}
#endif

#if defined CONFIG_VIDEO_SUNXI_VIN_SPECIAL
extern void vin_return_control(int type);
#endif

#ifdef CONFIG_CAR_REVERSE_GPIO_CHECK
#define GPIO_KEY_MASK			(0x0000000f)
#define GPIO_ENTER_CAR_REVERSE_MASK	(0x0000000f)
#define GPIO_EXIT_CAR_REVERSE_MASK	(0)
#define CAR_REVERSE_GPIO		GPIO_PI15
#endif

static int car_reverse_buffer_alloc(struct fastboot_private_data * car)
{
	int overview_type = car->config.oview_type;
	unsigned int node_registered = 0;
	int s_buffer_size;
	int screen_buffer_size;
	int i = 0, ret = 0;

	if (car->config.format == V4L2_PIX_FMT_NV12)
		s_buffer_size = car->config.src_height * car->config.src_width * 3 / 2;
	else
		s_buffer_size = car->config.src_height * car->config.src_width * 2;

	screen_buffer_size = car->config.screen_height * car->config.screen_width * 4;

	CAR_DRIVER_DBG("s_buffer_size 0x%x\n", s_buffer_size);
	CAR_DRIVER_DBG("screen_buffer_size 0x%x\n", screen_buffer_size);

	//mutex_lock(&car->alloc_lock);
	if (overview_type) {
		/* alloc buffer pool for muti-preview diaplay */
		for (i = 0; (i < CAR_MAX_CH) && (i < overview_type); i++) {
			car->bufferOv_pool[i] = alloc_buffer_pool(SWAP_BUFFER_CNT, s_buffer_size);
			if (!car->bufferOv_pool[i]) {
				CAR_REVERSE_ERR("alloc buffer memory bufferOv_pool failed\n");
				ret = -1;
				goto free_buffer;
			}
			node_registered++;
		}
	} else {
		/* alloc buffer pool for singal preview diaplay */
		car->buffer_pool = alloc_buffer_pool(SWAP_BUFFER_CNT, s_buffer_size);
		if (!car->buffer_pool) {
			ret = -1;
			//mutex_unlock(&car->alloc_lock);
			goto free_buffer;
		}
	}

	/* alloc buffer for di writeback */
	if ((car->buffer_di[0] == NULL) && car->config.di_used) {
		car->buffer_di[0] = buffer_node_alloc(s_buffer_size);

		if (!car->buffer_di[0]) {
			CAR_REVERSE_ERR("alloc buffer_di[0] memory failed\n");
			ret = -1;
			goto free_buffer;
		}
	}

	if ((car->buffer_di[1] == NULL) && car->config.di_used) {
		car->buffer_di[1] = buffer_node_alloc(s_buffer_size);

		if (!car->buffer_di[1]) {
			CAR_REVERSE_ERR("alloc buffer_di[1] memory failed\n");
			ret = -1;
			goto free_buffer;
		}
	}

	/* alloc buffer node for g2d writeback */
	if ((car->buffer_g2d[G2D_BUF_ROTATE] == NULL) && car->config.g2d_used) {
		car->buffer_g2d[G2D_BUF_ROTATE] = buffer_node_alloc(s_buffer_size);

		if (!car->buffer_g2d[G2D_BUF_ROTATE]) {
			CAR_REVERSE_ERR("alloc buffer_g2d[G2D_BUF_ROTATE] memory failed\n");
			ret = -1;
			goto free_buffer;
		}
	}

	/*
	if ((car->buffer_g2d[G2D_BUF_SCALER] == NULL) && car->config.g2d_used) {
		car->buffer_g2d[G2D_BUF_SCALER] = buffer_node_alloc(s_buffer_size);

		if (!car->buffer_g2d[G2D_BUF_SCALER]) {
			CAR_REVERSE_ERR("alloc buffer_g2d[G2D_BUF_SCALER] memory failed\n");
			ret = -1;
			goto free_buffer;
		}
	}*/

	if ((car->buffer_g2d[G2D_BUF_AUXLINE] == NULL) && car->config.g2d_used) {
		car->buffer_g2d[G2D_BUF_AUXLINE] = buffer_node_alloc(screen_buffer_size);
		if (!car->buffer_g2d[G2D_BUF_AUXLINE]) {
			CAR_REVERSE_ERR("alloc buffer_g2d[G2D_BUF_AUXLINE] memory failed\n");
			ret = -1;
			goto free_buffer;
		}
	}


	//mutex_unlock(&car_reverse->alloc_lock);
	return ret;

free_buffer:
	if (car->buffer_pool) {
		free_buffer_pool(car->buffer_pool);
		car->buffer_pool = NULL;
	}

	if (overview_type) {
		for (i = 0; i < node_registered++; i++) {
			if (car->bufferOv_pool[i]) {
				free_buffer_pool(car->bufferOv_pool[i]);
				car->bufferOv_pool[i] = NULL;
			}
		}
	}

	if (car->buffer_auxiliary_line) {
		free_buffer_node(car->buffer_auxiliary_line);
		car->buffer_auxiliary_line = NULL;
	}

	if (car->buffer_di[0]) {
		free_buffer_node(car->buffer_di[0]);
		car->buffer_di[0] = NULL;
	}

	if (car->buffer_di[1]) {
		free_buffer_node(car->buffer_di[1]);
		car->buffer_di[1] = NULL;
	}

	if (car->buffer_g2d[G2D_BUF_ROTATE]) {
		free_buffer_node(car->buffer_g2d[G2D_BUF_ROTATE]);
		car->buffer_g2d[G2D_BUF_ROTATE] = NULL;
	}

	if (car->buffer_g2d[G2D_BUF_SCALER]) {
		free_buffer_node(car->buffer_g2d[G2D_BUF_SCALER]);
		car->buffer_g2d[G2D_BUF_SCALER] = NULL;
	}

	if (car->buffer_g2d[G2D_BUF_AUXLINE]) {
		free_buffer_node(car->buffer_g2d[G2D_BUF_AUXLINE]);
		car->buffer_g2d[G2D_BUF_AUXLINE] = NULL;
	}
	//mutex_unlock(&car_reverse->alloc_lock);

	return ret;
}

static int car_reverse_buffer_free(struct fastboot_private_data * car)
{
	int i = 0;
	int overview_type = car->config.oview_type;

	//mutex_lock(&car_reverse->free_lock);
	if (car->buffer_pool) {
		free_buffer_pool(car->buffer_pool);
		car->buffer_pool = NULL;
	}

	if (overview_type) {
		for (i = 0; (i < CAR_MAX_CH) && (i < overview_type); i++) {
			free_buffer_pool(car->bufferOv_pool[i]);
			car->bufferOv_pool[i] = NULL;
		}
	}

	/*
	if (car->buffer_auxiliary_line) {
		free_buffer_node(car->buffer_auxiliary_line);
		car->buffer_auxiliary_line = NULL;
	}*/

	//if (car->buffer_di[0]) {
	//	free_buffer_node(car->buffer_di[0]);
	//	car->buffer_di[0] = NULL;
	//}
	//
	//if (car->buffer_di[1]) {
	//	free_buffer_node(car->buffer_di[1]);
	//	car->buffer_di[1] = NULL;
	//}

	if (car->buffer_g2d[G2D_BUF_ROTATE]) {
		free_buffer_node(car->buffer_g2d[G2D_BUF_ROTATE]);
		car->buffer_g2d[G2D_BUF_ROTATE] = NULL;
	}
	
	if (car->buffer_g2d[G2D_BUF_SCALER]) {
		free_buffer_node(car->buffer_g2d[G2D_BUF_SCALER]);
		car->buffer_g2d[G2D_BUF_SCALER] = NULL;
	}
	
	if (car->buffer_g2d[G2D_BUF_AUXLINE]) {
		free_buffer_node(car->buffer_g2d[G2D_BUF_AUXLINE]);
		car->buffer_g2d[G2D_BUF_AUXLINE] = NULL;
	}
	//mutex_unlock(&car_reverse->free_lock);
	return 0;
}

int car_reverse_auxiliary_line_buffer_alloc(struct fastboot_private_data * car)
{
	int ret = 0;
	if ((car->buffer_auxiliary_line == NULL) && car->config.aux_line_type) {
		car->buffer_auxiliary_line = buffer_node_alloc(car->config.screen_width * car->config.screen_height * 4);

		if (!car->buffer_auxiliary_line) {
			CAR_REVERSE_ERR("alloc buffer_auxiliary_line  memory failed\n");
			ret = -1;
		}
	}
	return ret;
}

int car_reverse_preview_start(struct fastboot_private_data * car)
{

	int ret = 0;
	int i, n;
	struct buffer_node *node;
	struct buffer_pool *bp ;
	int overview_type = car->config.oview_type;
	enum video_control state;

	ret = video_source_select(car->config.input_src);
	if (ret < 0) {
		CAR_REVERSE_ERR("failed to open video source\n");
		return -1;
	}

	state = video_source_rpmsg_get_status();
	if (GET_BY_RTOS != state) {
		video_source_rpmsg_status_on_off(RV_VIN_START);
#if defined CONFIG_CAR_REVERSE_SUPPORT_VIN
		vin_return_control(RV_GET_CONTROLL);
#endif
	}

	ret = car_reverse_buffer_alloc(car);

	/*if (car->config.g2d_used)
		preview_rotator_init(car->config);*/

	if (car->config.di_used)
		preview_enhancer_init(&car->config);

	/*if (car->config.aux_line_type)
		auxiliary_line_init();*/

	if (overview_type) {
		for (n = 0; (n < CAR_MAX_CH) && (n < overview_type); n++) {
			bp = car->bufferOv_pool[n];

			ret = video_source_connect(&car->config, \
					 n + car->config.video_channel);
			if (ret != 0) {
				CAR_REVERSE_ERR("can't connect to video source!\n");
				goto free_buffer;
			}

			for (i = 0; i < SWAP_BUFFER_CNT; i++) {
				node = buffer_pool_dequeue_buffer(bp);
				video_source_queue_buffer(node, \
					n + car->config.video_channel);
			}
		}


		for (n = 0; (n < CAR_MAX_CH) && (n < overview_type); n++) {
			video_source_streamon(n + car->config.video_channel);
		}

	} else {
		bp = car->buffer_pool;

		ret = video_source_connect(&car->config,
						car->config.video_channel);
		if (ret != 0) {
			CAR_REVERSE_ERR("can't connect to video source!\n");
			goto free_buffer;
		}

		for (i = 0; i < SWAP_BUFFER_CNT; i++) {
			node = buffer_pool_dequeue_buffer(bp);
			video_source_queue_buffer(node, car->config.video_channel);
		}

		video_source_streamon(car->config.video_channel);
	}

	//car->status = CAR_REVERSE_START;
	car->discard_frame = car->config.discard_frame;

	//spin_lock(&car->thread_lock);
	//car->thread_mask = 0;
	//spin_unlock(&car->thread_lock);

	return 0;

free_buffer:
	car_reverse_buffer_free(car);
//destory_thread:
	//kthread_stop(car_reverse->display_update_task);
	//car_reverse->display_update_task = NULL;
	return 0;
}

int car_reverse_preview_stop(struct fastboot_private_data * car)
{

	struct buffer_node *node = NULL;
	struct buffer_pool *bp;
	int i;
	int overview_type = car->config.oview_type;
	int last_status = car->last_status;

	if ((last_status == CAR_REVERSE_START) || (last_status == CAR_REVERSE_HOLD)) {
		if (overview_type) {
			for (i = 0; (i < CAR_MAX_CH) && (i < overview_type); i++)
				video_source_streamoff(i + car->config.video_channel);
		} else {
				video_source_streamoff(car->config.video_channel);
		}
	}

	preview_output_exit();
	car->disp_preview_flag = 0;

	//if (car->config.g2d_used) {
	//	preview_rotator_exit();
	//}
	//
	if (car->config.di_used) {
		preview_enhancer_exit();
	}

	if ((last_status == CAR_REVERSE_START) || (last_status == CAR_REVERSE_HOLD)) {
		if (overview_type) {
			for (i = 0; (i < CAR_MAX_CH) && (i < overview_type); i++)
				video_source_force_reset_buffer(i + car->config.video_channel);

			for (i = 0; (i < CAR_MAX_CH) && (i < overview_type); i++) {
				bp = car->bufferOv_pool[i];
				while (1) {
					node = video_source_dequeue_buffer(i + \
							car->config.video_channel);
					if (!IS_ERR_OR_NULL(node)) {
						buffer_pool_queue_buffer(bp, node);
						CAR_DRIVER_DBG("video source buffer has return to buffer pool!\n");
					} else
						break;
				}
				video_source_disconnect(i + car->config.video_channel);
			}
		} else {
			video_source_force_reset_buffer(car->config.video_channel);
			while (1) {
				node = video_source_dequeue_buffer(car->config.video_channel);
				if (!IS_ERR_OR_NULL(node)) {
					buffer_pool_queue_buffer(car->buffer_pool, node);
					CAR_DRIVER_DBG("video source buffer has return to buffer pool!\n");
				} else
					break;
			}
			video_source_disconnect(car->config.video_channel);
		}
	}

	car_reverse_buffer_free(car);
#if defined CONFIG_CAR_REVERSE_SUPPORT_VIN
	/* Special Operations: return csi control to ARM */
	vin_return_control(ARM_GET_CONTROLL);
#endif

	video_source_rpmsg_status_on_off(RV_VIN_STOP);

	return 0;
}

static void car_reverse_single_display(struct fastboot_private_data * car, \
				struct buffer_node **update_frame, \
				struct buffer_node **enhanced_frame)
{
	struct buffer_node *aux_node = car->buffer_auxiliary_line;
	struct buffer_node *update_aux_frame = NULL;
	struct preview_params *config = &car->config;
	//int aux_angle = car->config.aux_angle;
	//int aux_lr = car->config.aux_lr;
	int w,h;
	int ret = 0;

	if (car->config.aux_line_type) {
		if (car->config.aux_line_type == 1) {
	//		static_aux_line_update(aux_node, config, aux_angle, aux_lr);
			update_aux_frame = aux_node;
		} else if (car->config.aux_line_type == 2) {
	//		dynamic_aux_line_update(aux_node, aux_angle, aux_lr,
	//					AUXLAYER_WIDTH, AUXLAYER_HEIGHT);
			update_aux_frame = aux_node;
		} else
			CAR_REVERSE_WRN("Unknown aux_line_type!\n");

		if (car->config.g2d_used && config->rotation) {
			if (config->rotation == 1 || config->rotation ==3) {
				w = car->config.screen_height;
				h = car->config.screen_width;
			} else {
				w = car->config.screen_width;
				h = car->config.screen_height;
			}
			ret = preview_image_rotate(aux_node, &car->buffer_g2d[G2D_BUF_AUXLINE],
				w, h, config->rotation, -1);
			if (ret < 0)
				CAR_REVERSE_ERR("preview_image_rotate fail, use origin frame!\n");
			else
				update_aux_frame = car->buffer_g2d[G2D_BUF_AUXLINE];
		}

		aux_line_layer_config_update(update_aux_frame);
	}

	if (car->config.di_used) {
		ret = preview_image_enhance(*update_frame, &car->buffer_di[0],
				config->src_width, config->src_height, config->format);
		if (ret < 0)
			CAR_REVERSE_ERR("preview_image_enhance fail, use origin frame!\n");
		else {
			*enhanced_frame = car->buffer_di[0];
			*update_frame = car->buffer_di[0];
		}
	}

	if (car->config.g2d_used) {
		if (config->rotation) {
			ret = preview_image_rotate(*enhanced_frame,
					 &car->buffer_g2d[G2D_BUF_ROTATE],
				config->src_width, config->src_height, config->rotation, config->format);
			if (ret < 0)
				CAR_REVERSE_ERR("preview_image_rotate fail, use origin frame!\n");
			else
				*update_frame = car->buffer_g2d[G2D_BUF_ROTATE];
		}
	}

	preview_layer_config_update(*update_frame);
	preview_display();
	//preview_display_tmp_test((unsigned long)(*update_frame)->phy_address);
}

int car_reverse_preview_hold(struct fastboot_private_data * car)
{
	int i = 0;
	int overview_type = car->config.oview_type;
	struct buffer_node *node = NULL;
	struct buffer_node *update_frameOv[4];
	struct buffer_node *update_frame = NULL;
	struct buffer_node *enhanced_frame = NULL;
	struct preview_params *config = &car->config;

	for (i = 0; i < CAR_MAX_CH; i++)
		update_frameOv[i] = NULL;

	if (overview_type) {
		car->first_buf_unready = false;
		/* dequeue ready buffer from each channel's video source */
		//mutex_lock(&car->preview_lock);
		for (i = 0; (i < CAR_MAX_CH) && (i < overview_type); i++) {
			node = video_source_dequeue_buffer(i + config->video_channel);
			if (IS_ERR_OR_NULL(node)) {
				CAR_DRIVER_DBG("video source_%d buffer is not ready!\n", \
						   i + config->video_channel);
			} else {
				update_frameOv[i] = node;
				video_source_queue_buffer(node, i + config->video_channel);
			}

			if (update_frameOv[i] == NULL) {
				car->first_buf_unready = true;
			}
		}
		//mutex_unlock(&car->preview_lock);

		if (car->discard_frame != 0) {
			car->discard_frame--;
		} else {
			car->discard_frame = 0;
		}

		/* diplay if discard_frame is skiped */
		if (car->discard_frame == 0) {
			/* if first frame is not ready, skip it */
			if (car->first_buf_unready)
				return 0;
			//car_reverse_muti_display(update_frameOv, overview_type);

		}
	} else {

		/* dequeue ready buffer from video source */
		//mutex_lock(&car_reverse->preview_lock);

		node = video_source_dequeue_buffer(config->video_channel);
		if (IS_ERR_OR_NULL(node)) {
			CAR_DRIVER_DBG("video source_%d buffer is not ready!\n", \
					   config->video_channel);
			/* if first frame is not ready, skip it
			if ((update_frame->dmabuf_fd == 0) || (enhanced_frame->dmabuf_fd == 0))
				return 0; */
			return -2;
		} else {
			update_frame = node;
			enhanced_frame = node;
			if (car->disp_driver_ready) {
				/* need to preview init */
				if (car->disp_preview_flag == 0) {
					car->disp_preview_flag = 1;
					preview_output_init();
				}

				if (car->discard_frame != 0)
					car->discard_frame--;
				else
					car->discard_frame = 0;

				if (car->discard_frame == 0) {
					car_reverse_single_display(car, &update_frame, &enhanced_frame);
				}
				if (car->disp_first_flag == 0) {
					//CAR_REVERSE_INFO("disp first frame ok, time %ld ms\n", (read_timer()/24000));
					car->disp_first_flag = 1;
				}
			}
			video_source_queue_buffer(node, config->video_channel);
		}
		//mutex_unlock(&car->preview_lock);
	}

	return 0;
}