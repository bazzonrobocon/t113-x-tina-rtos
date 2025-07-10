#ifndef __TVD__RPMSG__H__
#define __TVD__RPMSG__H__

#include <hal_osal.h>
#include <hal_sem.h>
#include <hal_cache.h>
#include <hal_msgbox.h>
#include <openamp/sunxi_helper/openamp.h>
#include <hal_hwspinlock.h>
#include <sys/time.h>

enum tvd_control {
	GET_BY_LINUX,
	GET_BY_RTOS,
	GET_BY_NONE,
};

enum tvd_packet_type {
	RV_TVD_START,
	RV_TVD_START_ACK,
	RV_TVD_STOP,
	RV_TVD_STOP_ACK,

	ARM_TVD_START,
	ARM_TVD_START_ACK,
	ARM_TVD_STOP,
	ARM_TVD_STOP_ACK,
};

struct tvd_packet {
	u32 magic;
	u32 type;
};

struct rpmsg_tvd_private {
	hal_sem_t finish;		/* notify remote call finish */
	hal_spinlock_t ack_lock;	/* protect ack */
	hal_mutex_t call_lock;		/* protect status and make sure remote call is exclusive */
	hal_workqueue *init_workqueue;
	hal_work init_work;
	struct tvd_packet ack;
	int status;
	struct rpmsg_endpoint *ept;
	bool rpmsg_connecting;
	enum tvd_control control;
};
#endif