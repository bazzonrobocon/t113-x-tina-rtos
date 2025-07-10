#include <hal_osal.h>
#include <hal_sem.h>
#include <hal_cache.h>
#include <hal_msgbox.h>
#include <openamp/sunxi_helper/openamp.h>
#include <openamp/sunxi_helper/msgbox_ipi.h>
#include "tvd_rpmsg.h"
#include "tvd.h"

#define RPMSG_SERVICE_NAME		"sunxi,rpmsg_tvd"
#define TVD_PACKET_MAGIC		0x10244023

static struct rpmsg_tvd_private rpmsg_tvd;

int tvd_rpmsg_init();

int rpmsg_tvd_remote_call(enum tvd_packet_type type)
{
	tvd_here;
	int ret;
	struct tvd_packet packet;

	if((GET_BY_RTOS == rpmsg_tvd.control && RV_TVD_START == type) ||
		(GET_BY_LINUX == rpmsg_tvd.control && RV_TVD_STOP == type))
		return 0;

	if (!rpmsg_tvd.ept) {
		tvd_wrn("linux no ok trying to resigter \n");
		tvd_rpmsg_init();
		if (!rpmsg_tvd.ept)
			return 0;
	}

	packet.magic = TVD_PACKET_MAGIC;
	packet.type = type;
	ret = openamp_rpmsg_send(rpmsg_tvd.ept, (void *)&packet, sizeof(packet));
	if (ret <  sizeof(packet)) {
		printf("%s rpmsg sent fail\r\n", __FUNCTION__);
		ret = -1;
	}
	return ret;
}

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	struct tvd_packet *pack = data;

	if (pack->magic != TVD_PACKET_MAGIC || len != sizeof(*pack)) {
		tvd_wrn("packet invalid magic or size %ld %ld %x\n", len, sizeof(*pack), pack->magic);
		return 0;
	}

	if (pack->type == RV_TVD_START_ACK) {
		rpmsg_tvd.control = GET_BY_RTOS;
	} else if (pack->type == RV_TVD_STOP_ACK) {
		rpmsg_tvd.control = GET_BY_NONE;
	} else if (pack->type == ARM_TVD_START) {
		rpmsg_tvd_remote_call(ARM_TVD_START_ACK);
		rpmsg_tvd.control = GET_BY_LINUX;
	} else if (pack->type == ARM_TVD_STOP) {
		rpmsg_tvd_remote_call(ARM_TVD_STOP_ACK);
		rpmsg_tvd.control = GET_BY_NONE;
	}

	tvd_wrn("rtos ept callback type:%d control by:%d\n",
					pack->type, rpmsg_tvd.control);

	return 0;
}

static void rpmsg_unbind_callback(struct rpmsg_endpoint *ept)
{
	//struct rpmsg_demo_private *demo_priv = ept->priv;

	tvd_dbg("Remote endpoint is destroyed\n");
}

int tvd_rpmsg_sem_timewait(int ms)
{
	int ret;
	ret = hal_sem_timedwait(rpmsg_tvd.finish, MS_TO_OSTICK(ms));
	return ret;
}

int tvd_rpmsg_init()
{
	struct rpmsg_endpoint *ept;

	rpmsg_tvd.finish = hal_sem_create(0);
	if (!rpmsg_tvd.finish) {
		printf("rpmsg tvd failed to create sem\r\n");
		return -1;
	}

	if (rpmsg_tvd.ept != NULL) {
		tvd_wrn("tvd rpmsg already init \n");
		return -1;
	}

	rpmsg_tvd.rpmsg_connecting = true;

	ept = openamp_ept_open(RPMSG_SERVICE_NAME, 0, RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
					&rpmsg_tvd, rpmsg_ept_callback, rpmsg_unbind_callback);
	if (ept == NULL) {
		tvd_wrn("Failed to Create Endpoint\r\n");
		return -1;
	}

	rpmsg_tvd.ept = ept;
	rpmsg_tvd.control = GET_BY_RTOS;

	rpmsg_tvd.rpmsg_connecting = false;

	return 0;
}

int tvd_get_rpmsg_control()
{
	return rpmsg_tvd.control;
}