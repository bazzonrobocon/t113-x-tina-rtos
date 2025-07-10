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
#include "car_reverse.h"


//#include <rp_decode.h>
#include <string.h>
#ifdef CONFIG_ARCH_RISCV_PMP
#include <pmp.h>
#endif

#include "fastboot_com.h"
#include "video_buffer_pool.h"
#include "fastboot_video.h"

hal_queue_t fastboot_com_queue;

extern int cmd_rv_decode(int argc, char *argv[]);
extern int rv_dec_init(void);
extern void start_decode(void);
extern void start_display(void);
u32 loglevel_debug = 0;

struct fastboot_private_data fastboot_data;
static int car_reverse_status = CAR_REVERSE_NULL;

static inline unsigned long read_timer(void)
{
	unsigned long  cnt = 0;
	asm volatile("rdtime %0\n"
			: "=r" (cnt)
			:
			: "memory");
	return cnt;
}

int check_thread_timeout(unsigned long *p, char *pstr, int timeout, int flag)
{
	unsigned long tmp = 0;
	if (flag) {
		p[1] = read_timer();
		tmp = (p[1] - p[0])/24000;
		if (tmp >= timeout) {
			CAR_REVERSE_ERR("%s time out!!! use %ld ms, max %d ms\n", pstr, tmp, timeout);
		}else {
			//CAR_DRIVER_DBG("%s use %ld ms, max %d ms\n", pstr, tmp, timeout);
		}
	}
	else {
		p[0] = read_timer();
	}
	return 0;
}

#ifdef CONFIG_DRIVERS_CAR_REVERSE
static int check_rpmsg_driver_ready(struct fastboot_private_data * car)
{
	int ret = 0;

	CAR_DRIVER_DBG("car->car_reverse_rpmsg_ready %d ...\n", car->car_reverse_rpmsg_ready);
	CAR_DRIVER_DBG("car->vin_driver_rpmsg_ready %d ...\n", car->vin_driver_rpmsg_ready);
	CAR_DRIVER_DBG("car->di_driver_rpmsg_ready %d ...\n", car->di_driver_rpmsg_ready);

#ifdef CONFIG_CAR_REVERSE_SUPPORT_AMP_CRTL
	if (car->car_reverse_rpmsg_ready)
		ret = 1;
	else
		ret = 0;
#endif

	if (car->vin_driver_rpmsg_ready)
		ret = 1;
	else
		ret = 0;

#ifdef CONFIG_CAR_REVERSE_SUPPORT_PREVIEW_ENHANCER
	if (car->di_driver_rpmsg_ready)
		ret = 1;
	else
		ret = 0;
#endif

	return ret;
}
#endif

static void fastboot_parse_config(struct fastboot_private_data * car)
{
	int input_src = VIDEO_SRC_VIRTUAL;

	car->config.oview_type = 0;

#ifdef CONFIG_DRIVERS_CAR_REVERSE
	car->car_reverse_enable = 1;
#else
	car->car_reverse_enable = 0;
#endif

#ifdef CONFIG_DRIVERS_VIDEO_EARLY
	car->video_enable = 1;
#else
	car->video_enable = 0;
#endif

#if defined CONFIG_CAR_REVERSE_SUPPORT_TVD
	input_src = VIDEO_SRC_TVD;
#elif defined CONFIG_CAR_REVERSE_SUPPORT_VIN
	input_src = VIDEO_SRC_VIN;
#else
	input_src = VIDEO_SRC_VIRTUAL;
#endif
	car->config.input_src = input_src;

	car->config.video_channel = 0;
	car->config.src_size_adaptive = 1;
	/* csi/tvd format */
	car->config.format = V4L2_PIX_FMT_NV12;//six
	car->config.src_width = CONFIG_PREVIEW_SOURCE_WIDTH;
	car->config.src_height = CONFIG_PREVIEW_SOURCE_HEIGHT;
	/* screen format */
	car->config.screen_width = CONFIG_PREVIEW_SCREEN_WIDTH;
	car->config.screen_height = CONFIG_PREVIEW_SCREEN_HEIGHT;
	car->config.discard_frame = 0;

#ifdef CONFIG_CAR_REVERSE_SUPPORT_PREVIEW_ROTATOR
	car->config.g2d_used = 1;
	car->config.rotation = 2;
	extern int g2d_probe(void);
	g2d_probe();
#else
	car->config.g2d_used = 0;
	car->config.rotation = 0;
	car->config.rotation = 0;
#endif

#ifdef CONFIG_CAR_REVERSE_SUPPORT_PREVIEW_ENHANCER
	if (720 == car->config.src_width ||
			1440 == car->config.src_height)
		car->config.di_used = 1;
	else
		car->config.di_used = 0;
#else
	car->config.di_used = 0;
#endif

#ifdef CONFIG_CAR_REVERSE_SUPPORT_AUXILIARY_LINE
	car->config.aux_line_type = 1;
#else
	car->config.aux_line_type = 0;
#endif

	car->buffer_display = NULL;
	car->buffer_auxiliary_line = NULL;
	car->buffer_di[0] = NULL;
	car->buffer_di[1] = NULL;
	car->buffer_g2d[G2D_BUF_ROTATE] = NULL;
	car->buffer_g2d[G2D_BUF_SCALER] = NULL;
	car->buffer_g2d[G2D_BUF_AUXLINE] = NULL;
	car->disp_driver_ready = 0;
	car->disp_preview_flag = 0;
	car->disp_rpmsg_ready = 0;
	car->disp_first_flag = 0;
	car->car_reverse_rpmsg_ready = 0;
	car->arm_car_reverse_status = 0;
	car->vin_driver_rpmsg_ready = 0;
	car->di_driver_rpmsg_ready = 0;

#ifdef CONFIG_CAR_REVERSE_GPIO_CHECK
	hal_gpio_pinmux_set_function(CAR_REVERSE_GPIO, GPIO_MUXSEL_IN);
#endif
}

void fastboot_thread(void *param)
{
	int ret;
	struct fastboot_com_queue_t fvq_msg;
#ifdef CONFIG_DRIVERS_CAR_REVERSE
	struct car_reverse_packet pack;
#endif
	int ignore_flag = 0;
	struct fastboot_private_data *car = (struct fastboot_private_data *)param;
	int *pstatus = &car->status;
	int *plast_status = &car->last_status;

	*plast_status = *pstatus = CAR_REVERSE_NULL;
	for(;;)
	{
		ret = hal_queue_recv(fastboot_com_queue, &fvq_msg, 0);
		if (ret == HAL_OK){
			switch(fvq_msg.type) {
			case SIGNAL_TYPE_REVERSE_STATUS:
				if (ignore_flag == 0) {
					*plast_status = *pstatus;
					*pstatus = fvq_msg.reverse_status;
				}
				CAR_DRIVER_DBG("last_status %d, status %d\n", *plast_status, *pstatus);
				break;
			case SIGNAL_TYPE_DISP_DRIVER_REALY:
				car->disp_driver_ready = fvq_msg.disp_driver_ready;
				break;
			case SIGNAL_TYPE_DISP_RPMSG_REALY:
				car->disp_rpmsg_ready = fvq_msg.disp_rpmsg_ready;
				break;
			case SIGNAL_TYPE_CAR_REVERSE_RPMSG_REALY:
				car->car_reverse_rpmsg_ready = fvq_msg.car_reverse_rpmsg_ready;
				break;
			case ARM_REVERSE_SIGNAL_TYPE:
				car->arm_car_reverse_status = fvq_msg.arm_car_reverse_status;
				break;
			case SIGNAL_TYPE_VIN_RPMSG_REALY:
				car->vin_driver_rpmsg_ready = fvq_msg.vin_driver_rpmsg_ready;
				break;
			case SIGNAL_TYPE_DI_RPMSG_REALY:
				car->di_driver_rpmsg_ready = fvq_msg.di_driver_rpmsg_ready;
				break;
			default:
				break;
			}
			CAR_DRIVER_DBG("get queue ok, type %d\n", fvq_msg.type);
		}

		switch (*pstatus) {
		case CAR_REVERSE_NULL:
#ifdef CONFIG_DRIVERS_CAR_REVERSE
			if (car->arm_car_reverse_status == ARM_TRY_GET_CRTL) {
				ignore_flag = 1;
#if defined CONFIG_CAR_REVERSE_SUPPORT_VIN
				/* Special Operations: return csi control to ARM */
				vin_return_control(ARM_GET_CONTROLL);
#endif
				car->arm_car_reverse_status = pack.arm_car_reverse_status = ARM_GET_CRTL_OK;
				pack.type = ARM_TRY_GET_CAR_REVERSE_CRTL;
				car_reverse_amp_rpmsg_send(&pack);
				CAR_DRIVER_DBG("return csi control to arm\n");
			}
#endif
			break;
		case CAR_REVERSE_START:
#ifdef CONFIG_DRIVERS_CAR_REVERSE
			ret = car_reverse_preview_start(car);
			*plast_status = *pstatus;
			*pstatus = CAR_REVERSE_HOLD;
			if (car->arm_car_reverse_status == ARM_TRY_GET_CRTL) {
				car->arm_car_reverse_status = pack.arm_car_reverse_status = ARM_GET_CRTL_FAIL;
				pack.type = ARM_TRY_GET_CAR_REVERSE_CRTL;
				car_reverse_amp_rpmsg_send(&pack);
			}
#endif
			break;
		case CAR_REVERSE_STOP:
#ifdef CONFIG_DRIVERS_CAR_REVERSE
			car_reverse_preview_stop(car);
			*plast_status = *pstatus;
			*pstatus = CAR_REVERSE_NULL;
			if (check_rpmsg_driver_ready(car)) {
				CAR_DRIVER_DBG("check rpmsg driver ok ...\n");
#ifdef CONFIG_CAR_REVERSE_MEMORY_LEAK_TEST
				car_reverse_check_memory_leaks();
#endif
			}

			if (car->arm_car_reverse_status == ARM_TRY_GET_CRTL) {
				ignore_flag = 1;
				car->arm_car_reverse_status = pack.arm_car_reverse_status = ARM_GET_CRTL_OK;
				pack.type = ARM_TRY_GET_CAR_REVERSE_CRTL;
				car_reverse_amp_rpmsg_send(&pack);
				CAR_DRIVER_DBG("return csi control to arm\n");
			}
			CAR_DRIVER_DBG("stop car reverse\n");
#endif

			break;
		case CAR_REVERSE_HOLD:
#ifdef CONFIG_DRIVERS_CAR_REVERSE
			*plast_status = *pstatus;
			ret = car_reverse_preview_hold(car);
			if (car->arm_car_reverse_status == ARM_TRY_GET_CRTL) {
				car->arm_car_reverse_status = pack.arm_car_reverse_status = ARM_GET_CRTL_FAIL;
				pack.type = ARM_TRY_GET_CAR_REVERSE_CRTL;
				car_reverse_amp_rpmsg_send(&pack);
			}
#endif
			break;
		case CAR_VIDEO_START:
			if (car->disp_driver_ready) {
				preview_output_init();
#ifdef CONFIG_DRIVERS_VIDEO_EARLY
				start_display();
#endif
				printf("display driver ready %ld ms \n", (read_timer()/24000));
				*plast_status = *pstatus;
				*pstatus = CAR_VIDEO_HOLD;
			}
			break;
		case CAR_VIDEO_HOLD:
			break;
		default:
			break;
		}
		hal_msleep(1);
	}
}


void fastboot_disp_init_thread(void *param)
{
	int disp_init_flag = 0;
	int ret;
	struct fastboot_com_queue_t fvq_msg;
	struct fastboot_private_data *car = (struct fastboot_private_data *)param;
	int video_enable = car->video_enable;

	CAR_DRIVER_DBG("video_enable %d\n", video_enable);
	for(;;)
	{
		if (disp_init_flag == 0) {
			disp_init_flag = 1;
			if (video_enable) {
				ret = boot_animation_preview_display_init();
			}else {
				ret = car_reverse_preview_display_init(&fastboot_data.config,
						fastboot_data.buffer_auxiliary_line ? fastboot_data.buffer_auxiliary_line->phy_address : NULL );
			}
			if (ret < 0)
				CAR_DRIVER_DBG("preview_display_init err\n");
			else {
				fvq_msg.type = SIGNAL_TYPE_DISP_DRIVER_REALY;
				fvq_msg.disp_driver_ready = 1;

				if (hal_queue_send_wait(fastboot_com_queue, &fvq_msg, HAL_WAIT_FOREVER) == HAL_OK) {
					CAR_DRIVER_DBG("send queue msg ok\n");
				}
				//break;
			}
		}
		hal_msleep(1);
	}
}

static int car_reverse_set_signal(int argc, char **argv)
{
	int status;

	if (argc != 2) {
		printf ("Useage:\r\n");
		printf ("\t: set_signal num\r\n");
		printf ("\t: num -> 1 car reverse start \r\n");
		printf ("\t: num -> 2 car reverse stop \r\n");
		printf ("e.g.\r\n");
		printf ("\t: set_signal 1\r\n");
		return 0;
	}

	status = atoi(argv[1]);
	if (status >= CAR_REVERSE_STATUS_MAX) {
		printf("set value err , must less than %d\n", CAR_REVERSE_STATUS_MAX);
		return 0;
	}
	printf("old car_reverse_status value %d\n", car_reverse_status);
	car_reverse_status = status;
	printf("set car_reverse_status value %d\n", car_reverse_status);
	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(car_reverse_set_signal, set_signal, set signal of car reverse)

static void fastboot_status_thread(void *param)
{
	struct fastboot_com_queue_t fvq_msg;
	int last_status = CAR_REVERSE_NULL;

#ifdef CONFIG_CAR_REVERSE_GPIO_CHECK
	uint32_t key_mask = 0;
	uint32_t gpio_value = 0;
#endif

	car_reverse_status = last_status;
#ifdef CONFIG_CAR_REVERSE_DEFAULT_START
	car_reverse_status = CAR_REVERSE_START;
#endif

#ifdef CONFIG_CAR_REVERSE_STABILITY_TEST
	int delay_cnt = 0;
	int delay_time = 0;
	delay_time = ((rand() % 5) + 1) * 1000;
	car_reverse_status = CAR_REVERSE_START;
#endif

#ifdef CONFIG_FASTBOOT_VIDEO_DEFAULT_START
	car_reverse_status = CAR_VIDEO_START;//use config CAR_VIDEO_START
#endif

	for(;;)
	{
		if (last_status != car_reverse_status) {
			CAR_DRIVER_DBG("car_reverse_status = %d, last_status = %d\n", car_reverse_status, last_status);
			fvq_msg.type = SIGNAL_TYPE_REVERSE_STATUS;
			fvq_msg.reverse_status = last_status = car_reverse_status;
			if (hal_queue_send_wait(fastboot_com_queue, &fvq_msg, HAL_WAIT_FOREVER) == HAL_OK) {
				CAR_DRIVER_DBG("send queue msg ok\n");
			}
		}

#ifdef CONFIG_CAR_REVERSE_STABILITY_TEST

		if (delay_cnt++ >= delay_time) {
			delay_cnt = 0;
			delay_time = ((rand() % 5) + 1) * 1000;
			CAR_REVERSE_INFO("change status time %d\n", delay_time);
			if (car_reverse_status == CAR_REVERSE_NULL)
				car_reverse_status = CAR_REVERSE_START;
			else if (car_reverse_status == CAR_REVERSE_START)
				car_reverse_status = CAR_REVERSE_STOP;
			else if (car_reverse_status == CAR_REVERSE_STOP)
				car_reverse_status = CAR_REVERSE_START;
		}
#else
#ifdef CONFIG_CAR_REVERSE_GPIO_CHECK
		gpio_value = 0;
		hal_gpio_get_data(CAR_REVERSE_GPIO, &gpio_value);
		key_mask <<= 1;
		key_mask |= (gpio_value & 0x1);
		if ((key_mask & GPIO_KEY_MASK) == GPIO_ENTER_CAR_REVERSE_MASK) {
			if (car_reverse_status == CAR_REVERSE_NULL) {
				car_reverse_status = CAR_REVERSE_START;
				CAR_DRIVER_DBG("enter car reverse of gpio statue\n");
			}
			else if (car_reverse_status == CAR_REVERSE_STOP) {
				car_reverse_status = CAR_REVERSE_START;
				CAR_DRIVER_DBG("enter car reverse of gpio statue\n");
			}
		}
		else if ((key_mask & GPIO_KEY_MASK) == GPIO_EXIT_CAR_REVERSE_MASK) {
			if (car_reverse_status == CAR_REVERSE_START) {
				car_reverse_status = CAR_REVERSE_STOP;
				CAR_DRIVER_DBG("exit car reverse of gpio statue\n");
			}
		}
#endif
#endif
		hal_msleep(1);
	}
}

static void fastboot_loglevel(void)
{
	int i = 0;
	loglevel_debug = 0;

	for (i = 0; i < CONFIG_CAR_REVERSE_LOG_LEVEL; i++)
	{
		loglevel_debug |= (1 << i);
	}
}

int fastboo_rpmsg_init(void)
{
	struct fastboot_com_queue_t fvq_msg;
	int ret;

	ret = preview_display_rpmsg_init();
	if (ret < 0)
		CAR_REVERSE_ERR("display rpmsg init fail ...\n");
	else {
		fvq_msg.type = SIGNAL_TYPE_DISP_RPMSG_REALY;
		fvq_msg.disp_rpmsg_ready = 1;
		if (hal_queue_send_wait(fastboot_com_queue, &fvq_msg, HAL_WAIT_FOREVER) == HAL_OK) {
			CAR_DRIVER_DBG("send queue fvq_msg(display rpmsg init) ok\n");
		}
	}
#ifdef CONFIG_DRIVERS_CAR_REVERSE
#ifdef CONFIG_CAR_REVERSE_SUPPORT_AMP_CRTL
	ret = car_reverse_amp_rpmsg_init();
	if (ret < 0)
		CAR_REVERSE_ERR("car reverse rpmsg init fail ...\n");
	else {
		fvq_msg.type = SIGNAL_TYPE_CAR_REVERSE_RPMSG_REALY;
		fvq_msg.car_reverse_rpmsg_ready = 1;
		if (hal_queue_send_wait(fastboot_com_queue, &fvq_msg, HAL_WAIT_FOREVER) == HAL_OK) {
			CAR_DRIVER_DBG("send queue msg(car reverse rpmsg init) ok\n");
		}
	}
#endif

	ret = video_source_rpmsg_init();
	if (ret < 0)
		CAR_REVERSE_ERR("vin rpmsg init fail ...\n");
	else {
		fvq_msg.type = SIGNAL_TYPE_VIN_RPMSG_REALY;
		fvq_msg.vin_driver_rpmsg_ready = 1;
		if (hal_queue_send_wait(fastboot_com_queue, &fvq_msg, HAL_WAIT_FOREVER) == HAL_OK) {
			CAR_DRIVER_DBG("send queue msg(vin rpmsg init) ok\n");
		}
	}

#ifdef CONFIG_CAR_REVERSE_SUPPORT_PREVIEW_ENHANCER
	ret = preview_enhancer_rpmsg_init();
	if (ret < 0)
		CAR_REVERSE_ERR("di rpmsg init fail ...\n");
	else {
		fvq_msg.type = SIGNAL_TYPE_DI_RPMSG_REALY;
		fvq_msg.di_driver_rpmsg_ready = 1;
		if (hal_queue_send_wait(fastboot_com_queue, &fvq_msg, HAL_WAIT_FOREVER) == HAL_OK) {
			CAR_DRIVER_DBG("send queue msg(di rpmsg init) ok\n");
		}
	}
#endif

#ifdef CONFIG_CAR_REVERSE_DEFAULT_STOP
	/*wait for vin(arm) insmod*/
	video_source_rpmsg_sem_timewait(3000);
	car_reverse_status = CAR_REVERSE_STOP;
#endif

#endif
	return 0;
}


int fastboot_init(void)
{
	fastboot_loglevel();
	preview_display_init();
	fastboot_parse_config(&fastboot_data);

#ifdef CONFIG_DRIVERS_CAR_REVERSE
	/* init buffer pool */
	buffer_pool_mem_init();
	/* alloc auxiliary line buffer */
	car_reverse_auxiliary_line_buffer_alloc(&fastboot_data);
	/* init video source */
	car_reverse_video_source_init();
#endif

#ifdef CONFIG_DRIVERS_VIDEO_EARLY
	ve_buffer_pool_mem_init();
	rv_dec_init();
	start_decode();
#endif
	return 0;
}

int fastboot_thread_init(void)
{
	void *thread;

	fastboot_com_queue = hal_queue_create("fastboot_com_queue", \
							sizeof(struct fastboot_com_queue_t), \
							CONFIG_FASTBOOT_COM_QUEUE_SIZE);

	if (!fastboot_com_queue) {
		CAR_REVERSE_ERR("create queue fail\n");
	}

	thread = hal_thread_create(fastboot_thread, &fastboot_data, \
					"fastboot", STACK_FASTBOOT_THRED, PRIORITY_FASTBOOT_THRED);
	if (thread != NULL)
		hal_thread_start(thread);
	else
		CAR_REVERSE_ERR("create thread fail\n");

	thread = hal_thread_create(fastboot_disp_init_thread, &fastboot_data, \
							"fastboot_disp_init", \
							STACK_FASTBOOT_DISPLAY_INT_THRED, \
							PRIORITY_FASTBOOT_DISPLAY_INT_THRED);
	if (thread != NULL)
		hal_thread_start(thread);
	else
		CAR_REVERSE_ERR("create thread fail\n");


	thread = hal_thread_create(fastboot_status_thread, NULL, \
					"fastboot_status_thread", \
					STACK_FASTBOOT_STATUS_THRED, \
					PRIORITY_FASTBOOT_STATUS_THRED);
	if (thread != NULL)
		hal_thread_start(thread);
	else
		CAR_REVERSE_ERR("create thread fail\n");

	return 0;
}
