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
#include <hal_timer.h>
#include <openamp/sunxi_helper/openamp.h>
#include "fastboot_com.h"
#include "car_reverse_amp.h"

#define RPMSG_SERVICE_NAME			"sunxi,rpmsg_car_reverse"
#define CAR_REVERSE_PACKET_MAGIC		0xA5A51234

struct rpmsg_car_reverse_private {
	struct rpmsg_endpoint *ept;
	bool unbound;
};


static struct rpmsg_car_reverse_private rpmsg_car;
static void rpmsg_unbind_callback(struct rpmsg_endpoint *ept)
{
	//struct rpmsg_demo_private *demo_priv = ept->priv;

	printf("Remote endpoint is destroyed\n");
	rpmsg_car.unbound = 1;
}
extern hal_queue_t car_reverse_queue;

int car_reverse_amp_rpmsg_send(struct car_reverse_packet *pack)
{
	int ret = 0;
	pack->magic = CAR_REVERSE_PACKET_MAGIC;

	ret = openamp_rpmsg_send(rpmsg_car.ept, (void *)pack, sizeof(struct car_reverse_packet));
	if (ret <  sizeof(struct car_reverse_packet)) {
		CAR_REVERSE_ERR("%s rpmsg sent fail\r\n", __FUNCTION__);
		ret = -1;
	}
	return ret;
}

/* receive packet supposed to be a remote call ack */
static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
#if 0
	struct car_reverse_packet *pack = data;
	struct car_reverse_queue_msg msg;

	if (pack->magic != CAR_REVERSE_PACKET_MAGIC || len != sizeof(*pack)) {
		CAR_REVERSE_ERR("packet invalid magic or size %d %d %x\n", (int)len, (int)(sizeof(*pack)), pack->magic);
		return 0;
	}

	msg.type = ARM_REVERSE_SIGNAL_TYPE;
	msg.arm_car_reverse_status = pack->arm_car_reverse_status;
	CAR_REVERSE_INFO("get arm car reverse rpmsg, arm_car_reverse_status %d\n", msg.arm_car_reverse_status);
	if (hal_queue_send_wait(car_reverse_queue, &msg, HAL_WAIT_FOREVER) == HAL_OK) {
		CAR_DRIVER_DBG("send queue msg ok\n");
	}
#endif
	return 0;
}

int car_reverse_amp_rpmsg_init(void)
{
	struct rpmsg_endpoint *ept;

	ept = openamp_ept_open(RPMSG_SERVICE_NAME, 0, RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
					NULL, rpmsg_ept_callback, rpmsg_unbind_callback);
	if (ept == NULL) {
		printf("Failed to Create Endpoint\r\n");
		return -1;
	}
	rpmsg_car.ept = ept;
	return 0;
}

