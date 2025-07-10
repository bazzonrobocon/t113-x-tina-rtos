/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <errno.h>
#include <console.h>
#include <hal_osal.h>
#include <hal_queue.h>
#include <hal_interrupt.h>
#include <hal_msgbox.h>
#include <errno.h>
#include <task.h>
#include <osal/hal_atomic.h>
#include <mpu_wrappers.h>

#include <pm_adapt.h>
#include <pm_debug.h>
#include <pm_suspend.h>
#include <pm_plat.h>
#include <pm_e906_client.h>

struct sunxi_pm {
	struct msg_endpoint ept;
	hal_queue_t recv_queue;
	void *thread;
};

static void pm_client_recv_callback(uint32_t data, void *priv)
{
	int ret;
	struct sunxi_pm *inst = priv;

	ret = hal_queue_send(inst->recv_queue, &data);
	if (ret != 0)
		pm_err("standby recv send queue failed\n");
}

static void pm_client_send_msg(struct sunxi_pm *inst, uint32_t data)
{
	int ret;

	pm_log("standby send msg: 0x%08x\n", data);
	ret = hal_msgbox_channel_send(&inst->ept, (uint8_t *)&data, sizeof(uint32_t));
	if (ret)
		pm_err("standby send msg: 0x%08x failed\n", data);
}

static void standby_thread(void *arg)
{
	int ret;
	uint32_t data;
	struct sunxi_pm *inst = arg;

	while (1) {
		data = 0;
		ret = hal_queue_recv(inst->recv_queue, &data, HAL_WAIT_FOREVER);
		if (ret != 0) {
			pm_warn("standby thread read form queue failed\r\n");
			continue;
		}
		pm_client_send_msg(inst, data);

		switch (data) {
		case PM_CLIENT_SUSPEND:
			ret = pm_trigger_suspend(PM_MODE_STANDBY);
			if (ret)
				pm_err("trigger suspend failed, return %d\n", ret);
			break;
		case PM_CLIENT_RESUME:
			break;
		default:
			pm_warn("Undown data:0x%08x\r\n", data);
			break;
		}
	}
}

int pm_client_init(void)
{
	int ret;
	struct sunxi_pm *inst;

	inst = hal_malloc(sizeof(*inst));
	if (!inst) {
		pm_err("standby init alloc failed\r\n");
		ret = -ENOMEM;
		goto out;
	}

	inst->ept.rec = pm_client_recv_callback;
	inst->ept.private = inst;

	ret = hal_msgbox_alloc_channel(&inst->ept, MSGBOX_PM_REMOTE, MSGBOX_PM_RECV_CHANNEL, MSGBOX_PM_SEND_CHANNEL);
	if (ret) {
		pm_err("fail to alloc msgbox channel(remote:%d rx:%d tx:%d)\n",
					MSGBOX_PM_REMOTE, MSGBOX_PM_RECV_CHANNEL, MSGBOX_PM_SEND_CHANNEL);
		ret = -EINVAL;
		goto free_inst;
	}

	inst->recv_queue = hal_queue_create("standby-queue", sizeof(uint32_t), 4);
	if (!inst->recv_queue) {
		pm_err("standby init create queue\r\n");
		ret = -ENOMEM;
		goto free_channel;
	}

	inst->thread = hal_thread_create(standby_thread, inst, "pm_client", 4 * 1024, OS_PRIORITY_NORMAL);
	if (!inst->thread) {
		pm_err("fail to create standby thread\r\n");
		ret = -ENOMEM;
		goto free_queue;
	}
	hal_thread_start(inst->thread);

	return 0;

free_queue:
	hal_queue_delete(inst->recv_queue);
free_channel:
	hal_msgbox_free_channel(&inst->ept);
free_inst:
	hal_free(inst);
out:
	return ret;

}
