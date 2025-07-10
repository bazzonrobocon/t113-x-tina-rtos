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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include <hal_time.h>
#include <hal_thread.h>
#include <hal_mem.h>
#include <hal_queue.h>
#include <aw_list.h>

#include "rpbuf_audio_module_process.h"
#include "rpbuf_common_interface.h"
#include "rpbuf_audio_ctrl_module_process.h"

#include "audio_amp_log.h"

#define RPBUF_AUDIO_CRTL_AMP_QUEUE				(8)

static void *g_amp_thread[RPBUF_CRTL_AMP_COUNTS];

static int g_amp_init_done[RPBUF_CRTL_AMP_COUNTS] = { 0 };
static void* g_amp_hld[RPBUF_CRTL_AMP_COUNTS] = { NULL };
static hal_queue_t g_amp_queue[RPBUF_CRTL_AMP_COUNTS] = { NULL };

typedef struct RpbufAudioAmpItem {
	rpbuf_audio_local_ctrl_amp_t rpbuf_audio_ctrl_amp;
	int len;
}RpbufAudioAmpItem;

struct rpbuf_audio_ctrl_amp_entry {
	void *rx_task;
	int index;
	char name[32];
	audio_module_process_ctrl_param_t amp_ctrl_param;
	uint8_t gsound_buf[RPBUF_AMP_CONFIG_LENGTH];
	hal_queue_t rpbuf_audio_queue;
};

static int  rpbuf_audio_ctrl_amp(struct rpbuf_audio_ctrl_amp_entry *audio_ctrl_amp_entry, void *data, int data_len)
{
	rpbuf_audio_local_ctrl_amp_t rpbuf_ctrl_init;
	RPBUF_CTRL_TYPE rpbuf_ctrl_type;
	RPBUF_ALGO_TYPE rpbuf_algo_type;
	audio_module_process_queue_item amp_item;
	int ret;
	int index = audio_ctrl_amp_entry->index;

	if (index > RPBUF_CRTL_AMP_COUNTS - 1) {
		aamp_err("err! rpbuf_audio_ctrl_amp index %d is large than  %d \n", index, RPBUF_CRTL_AMP_COUNTS);
		return -1;
	}

	if (data_len != sizeof(rpbuf_audio_local_ctrl_amp_t)) {
		aamp_err("rpbuf_audio_ctrl_amp get rpbuf len %d err!\n", data_len);
		return -1;
	}

	memcpy(&rpbuf_ctrl_init, data, data_len);

	rpbuf_ctrl_type = rpbuf_ctrl_init.cmd;
	rpbuf_algo_type = rpbuf_ctrl_init.type;

	aamp_info("%s get cmd %d type %d val %d", audio_ctrl_amp_entry->name, rpbuf_ctrl_init.cmd, rpbuf_ctrl_init.type, rpbuf_ctrl_init.value);

	switch(rpbuf_ctrl_type)
	{

		case RPBUF_CTL_INIT:
		{
			//algo init
			if (rpbuf_algo_type == RPBUF_ALGO_AMP) {
				if (!g_amp_init_done[index]) {
					g_amp_hld[index] = audio_module_process_ctrl_init(index);
					if (!g_amp_hld[index]) {
						aamp_err("amp_ctrl_init err!");
						return -1;
					}
					ret = rpbuf_audio_module_transfer_create(g_amp_hld[index]);
					if (ret != 0) {
						aamp_err("rpbuf_audio_module_transfer_create err!");
						return ret;
					}
					rpbuf_audio_module_process_default_param(g_amp_hld[index]);
					g_amp_queue[index] = audio_module_process_get_queue_handle(g_amp_hld[index]);
					if (!g_amp_queue[index]) {
						aamp_err("amp_get_queue_handle err!");
						return -1;
					}
					g_amp_init_done[index] = 1;
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}

		case RPBUF_CTL_CONFIG:
		{
			//algo config
			if (rpbuf_ctrl_init.rate == 0 || rpbuf_ctrl_init.channels == 0 || rpbuf_ctrl_init.out_channels == 0 || \
				rpbuf_ctrl_init.bits == 0 || rpbuf_ctrl_init.msec == 0) {
				aamp_err("invaild param, bits:%d, channels:%d, out channels:%d, rate:%d, msec:%d \n", \
					rpbuf_ctrl_init.bits, rpbuf_ctrl_init.channels, rpbuf_ctrl_init.out_channels, \
					rpbuf_ctrl_init.rate, rpbuf_ctrl_init.msec);
				return -1;
			}

			if (rpbuf_algo_type == RPBUF_ALGO_AMP) {
				audio_module_process_config_param_t *amp_config_param = &audio_ctrl_amp_entry->amp_ctrl_param.module_process_config_param;
				amp_config_param->rate = rpbuf_ctrl_init.rate;
				amp_config_param->in_channels = rpbuf_ctrl_init.channels;
				amp_config_param->out_channels = rpbuf_ctrl_init.out_channels;
				amp_config_param->bits = rpbuf_ctrl_init.bits;
				amp_config_param->msec = rpbuf_ctrl_init.msec;
				amp_config_param->smooth_time = rpbuf_ctrl_init.smooth_time;
				if (rpbuf_ctrl_init.ampPrmslen) {
					amp_config_param->gsound_buf = rpbuf_ctrl_init.ampPrmsTransfer;
					amp_config_param->gsound_len = rpbuf_ctrl_init.ampPrmslen;
				}
				else {
					amp_config_param->gsound_buf = NULL;
					amp_config_param->gsound_len = 0;
				}

				amp_item.cmd = RPBUF_CTL_CONFIG;
				amp_item.arg = amp_config_param;
				amp_item.len = sizeof(audio_module_process_config_param_t);

				if (g_amp_hld[index]) {
					ret = hal_queue_send(g_amp_queue[index], &amp_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("amp handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}

		case RPBUF_CTL_START:
		{
			// start
			if (rpbuf_algo_type == RPBUF_ALGO_AMP) {
				amp_item.cmd = RPBUF_CTL_START;
				amp_item.arg = NULL;
				amp_item.len = 0;
				if (g_amp_hld[index]) {
					ret = hal_queue_send(g_amp_queue[index], &amp_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("amp handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}

		case RPBUF_CTL_PAUSE:
		{
			// pause
			if (rpbuf_algo_type == RPBUF_ALGO_AMP) {
				amp_item.cmd = RPBUF_CTL_PAUSE;
				amp_item.arg = NULL;
				amp_item.len = 0;
				if (g_amp_hld[index]) {
					ret = hal_queue_send(g_amp_queue[index], &amp_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("amp handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}

		case RPBUF_CTL_RESUME:
		{
			// resume
			if (rpbuf_algo_type == RPBUF_ALGO_AMP) {
				amp_item.cmd = RPBUF_CTL_RESUME;
				amp_item.arg = NULL;
				amp_item.len = 0;
				if (g_amp_hld[index]) {
					ret = hal_queue_send(g_amp_queue[index], &amp_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("amp handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}

		case RPBUF_CTL_STOP:
		{
			//stop
			if (rpbuf_algo_type == RPBUF_ALGO_AMP) {
				amp_item.cmd = RPBUF_CTL_STOP;
				amp_item.arg = NULL;
				amp_item.len = 0;
				if (g_amp_hld[index]) {
					ret = hal_queue_send(g_amp_queue[index], &amp_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("amp handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}

		case RPBUF_CTL_RELEASE:
		{
			//release
			if (rpbuf_algo_type == RPBUF_ALGO_AMP) {
				if (g_amp_init_done[index]) {
					amp_item.cmd = RPBUF_CTL_RELEASE;
					amp_item.arg = NULL;
					amp_item.len = 0;
					if (g_amp_hld[index]) {
						ret = hal_queue_send(g_amp_queue[index], &amp_item);
						if (ret != 0) {
							aamp_err("queue send error\n");
							return ret;
						}
						rpbuf_audio_module_process_deinit_param(g_amp_hld[index]);
						audio_module_process_ctrl_destroy(g_amp_hld[index]);
					} else {
						aamp_err("amp handle is null, please init first!\n");
					}
					g_amp_hld[index] = NULL;
					g_amp_init_done[index] = 0;
				} else {
					aamp_err("please init amp first!\n");
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}
		case RPBUF_CTL_DUMP_MERGE_DATA:
		{
			// enable/disable data dump.
			if (rpbuf_algo_type == RPBUF_ALGO_AMP) {
				ret = do_rpbuf_enable_amp_dump_merge(g_amp_hld[index], index, rpbuf_ctrl_init.value);
				if (ret != 0) {
					aamp_err("do_rpbuf_enable_amp_dump_merge failed\n");
					return -1;
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}
		default:
		{
			aamp_err("rpbuf ctrl get type %d err!", rpbuf_ctrl_type);
			break;
		}

	 }
	return 0;
}

static void rpbuf_audio_amp_rx_thread(void *pram)
{
	struct rpbuf_audio_ctrl_amp_entry *audio_ctrl_amp_entry = (struct rpbuf_audio_ctrl_amp_entry *)pram;
	RpbufAudioAmpItem item;
	int ret;

	aamp_debug("%s rx thread start...\r\n", audio_ctrl_amp_entry->name);

	while(1) {

		ret = hal_queue_recv(audio_ctrl_amp_entry->rpbuf_audio_queue, &item, HAL_WAIT_FOREVER);
		if (ret != 0) {
			aamp_err("Failed to recv queue\n");
			goto out;
		}

		ret = rpbuf_audio_ctrl_amp(audio_ctrl_amp_entry, &item.rpbuf_audio_ctrl_amp, item.len);
		if (ret != 0) {
			aamp_err("Failed to call rpbuf_audio_ctrl_amp\n");
			continue;
		}
	}

out:
	aamp_info("%s rx thread exit...\r\n", audio_ctrl_amp_entry->name);
	hal_thread_stop(audio_ctrl_amp_entry->rx_task);

}

static void rpbuf_audio_available_cb(struct rpbuf_buffer *buffer, void *priv)
{
	aamp_debug("buffer \"%s\" is available\n", rpbuf_buffer_name(buffer));
}

static int rpbuf_audio_ctrl_amp_callback(struct rpbuf_buffer *buffer,
		void *data, int data_len, void *priv)
{
	struct rpbuf_audio_ctrl_amp_entry *audio_ctrl_amp_entry = (struct rpbuf_audio_ctrl_amp_entry *)priv;
	RpbufAudioAmpItem audio_item;
	rpbuf_audio_remote_ctrl_amp_t * rpbuf_audio_remote_ctrl_amp= (rpbuf_audio_remote_ctrl_amp_t *)data;
	int ret = 0;

	if (audio_ctrl_amp_entry == NULL) {
		aamp_err("rpbuf audio ctrl callback get audio_ctrl_entry is null!");
		return -1;
	}

	if (data_len != sizeof(rpbuf_audio_remote_ctrl_amp_t)) {
		aamp_err("rpbuf audio callback get rpdata len %d!", data_len);
		return -1;
	}

	/* convert remote audio type to local audio type */
	memset(&audio_item, 0, sizeof(RpbufAudioAmpItem));
	audio_item.rpbuf_audio_ctrl_amp.cmd = rpbuf_audio_remote_ctrl_amp->cmd;
	audio_item.rpbuf_audio_ctrl_amp.type = rpbuf_audio_remote_ctrl_amp->type;
	audio_item.rpbuf_audio_ctrl_amp.value = rpbuf_audio_remote_ctrl_amp->value;
	if (audio_item.rpbuf_audio_ctrl_amp.cmd == RPBUF_CTL_CONFIG && audio_item.rpbuf_audio_ctrl_amp.type == RPBUF_ALGO_AMP) {
		audio_item.rpbuf_audio_ctrl_amp.rate = rpbuf_audio_remote_ctrl_amp->rate;
		audio_item.rpbuf_audio_ctrl_amp.msec = rpbuf_audio_remote_ctrl_amp->msec;
		audio_item.rpbuf_audio_ctrl_amp.channels = rpbuf_audio_remote_ctrl_amp->channels;
		audio_item.rpbuf_audio_ctrl_amp.out_channels = rpbuf_audio_remote_ctrl_amp->out_channels;
		audio_item.rpbuf_audio_ctrl_amp.bits = rpbuf_audio_remote_ctrl_amp->bits;
		audio_item.rpbuf_audio_ctrl_amp.smooth_time = rpbuf_audio_remote_ctrl_amp->smooth_time;
		if (rpbuf_audio_remote_ctrl_amp->gsound_len) {
			audio_item.rpbuf_audio_ctrl_amp.ampPrmsTransfer = audio_ctrl_amp_entry->gsound_buf;
			audio_item.rpbuf_audio_ctrl_amp.ampPrmslen = rpbuf_audio_remote_ctrl_amp->gsound_len;
			memcpy(audio_item.rpbuf_audio_ctrl_amp.ampPrmsTransfer, rpbuf_audio_remote_ctrl_amp->gsound_buf, rpbuf_audio_remote_ctrl_amp->gsound_len);
		}
	}
	audio_item.len = sizeof(rpbuf_audio_local_ctrl_amp_t);
	ret = hal_queue_send(audio_ctrl_amp_entry->rpbuf_audio_queue, &audio_item);
	if (ret != 0) {
		aamp_err("queue send error\n");
		return ret;
	}

	aamp_debug("%s.%p: rx %d Bytes\r\n", audio_ctrl_amp_entry->name,
					rpbuf_buffer_va(buffer), data_len);


	return 0;
}

static int rpbuf_audio_destroyed_cb(struct rpbuf_buffer *buffer, void *priv)
{
	struct rpbuf_audio_ctrl_amp_entry *private = (struct rpbuf_audio_ctrl_amp_entry *)priv;

	aamp_debug("buffer \"%s\": remote buffer destroyed\n", rpbuf_buffer_name(buffer));
	if (!strncmp(rpbuf_buffer_name(buffer), private->name, strlen(private->name)))
	{
		aamp_debug("buffer \"%s\": recv thread will exit\n", rpbuf_buffer_name(buffer));
	}
	return 0;
}

static const struct rpbuf_buffer_cbs rpbuf_audio_ctrl_amp_cbs = {
	.available_cb 	= rpbuf_audio_available_cb,
	.rx_cb 			= rpbuf_audio_ctrl_amp_callback,
	.destroyed_cb 	= rpbuf_audio_destroyed_cb,
};

void RpbufAudioCtrlAmpThread(void *arg)
{
	int index = *(int *)arg;
	char name[32];
	int ret = 0;
	struct rpbuf_audio_ctrl_amp_entry *audio_ctrl_amp_entry = NULL;

	audio_ctrl_amp_entry = hal_malloc(sizeof(*audio_ctrl_amp_entry));
	if (audio_ctrl_amp_entry == NULL) {
		aamp_err("failed to alloc entry\r\n");
		return;
	}
	memset(audio_ctrl_amp_entry, 0, sizeof(*audio_ctrl_amp_entry));

	audio_ctrl_amp_entry->rpbuf_audio_queue = hal_queue_create("rpbuf_audio_ctrl_amp", sizeof(RpbufAudioAmpItem), RPBUF_AUDIO_CRTL_AMP_QUEUE);
	if (audio_ctrl_amp_entry->rpbuf_audio_queue == NULL) {
		aamp_err("rpbuf_audio: Failed to allocate memory for queue\n");
		goto init_err_out;
	}

	audio_ctrl_amp_entry->index = index;

	snprintf(audio_ctrl_amp_entry->name, sizeof(audio_ctrl_amp_entry->name), "%s_%d", RPBUF_CTL_AMP_NAME, audio_ctrl_amp_entry->index);

	aamp_info("rpbuf %s start", audio_ctrl_amp_entry->name);

	/* dsp recv, A7 send ctrl cmd.
	 */
	ret = rpbuf_common_create(RPBUF_CTRL_ID, audio_ctrl_amp_entry->name, sizeof(rpbuf_audio_remote_ctrl_amp_t), 1, &rpbuf_audio_ctrl_amp_cbs, audio_ctrl_amp_entry);
	if (ret < 0) {
		aamp_err("rpbuf_create for name %s (len: %d) failed\n", audio_ctrl_amp_entry->name, sizeof(rpbuf_audio_remote_ctrl_amp_t));
		goto init_err_out;
	}

	while (!rpbuf_common_is_available(audio_ctrl_amp_entry->name)) {
		//aamp_debug("rpbuf %s wait\n", audio_ctrl_amp_entry->name);
		hal_msleep(10);
	}

	aamp_info("rpbuf %s ready", audio_ctrl_amp_entry->name);

	snprintf(name, sizeof(name), "RpbufAudioAmpRx%d", audio_ctrl_amp_entry->index);

	audio_ctrl_amp_entry->rx_task = hal_thread_create(rpbuf_audio_amp_rx_thread, audio_ctrl_amp_entry, name, \
					2048, configAPPLICATION_AAMP_PRIORITY);
	if (audio_ctrl_amp_entry->rx_task == NULL) {
		aamp_err("Failed to create %s rx_task\r\n", name);
		goto err_out;
	}
	hal_thread_start(audio_ctrl_amp_entry->rx_task);

	hal_msleep(100);

	hal_thread_stop(g_amp_thread[audio_ctrl_amp_entry->index]);

	g_amp_thread[audio_ctrl_amp_entry->index] = NULL;

	return;

err_out:

	rpbuf_common_destroy(audio_ctrl_amp_entry->name);

init_err_out:

	if (audio_ctrl_amp_entry->rpbuf_audio_queue)
		hal_queue_delete(audio_ctrl_amp_entry->rpbuf_audio_queue);

	if (audio_ctrl_amp_entry) {
		hal_thread_stop(g_amp_thread[audio_ctrl_amp_entry->index]);
		g_amp_thread[audio_ctrl_amp_entry->index] = NULL;
	}

	if (audio_ctrl_amp_entry)
		hal_free(audio_ctrl_amp_entry);

	aamp_info("end");

}

void rpbuf_audio_ctrl_amp_init(void)
{
	int ret;
	int i = 0;
	char name[32];

	ret = rpbuf_common_init();
	if (ret < 0) {
		aamp_err("rpbuf_common_init failed\n");
		goto err_out;
	}

	for (i = 0; i< RPBUF_CRTL_AMP_COUNTS; ++i) {
		snprintf(name, sizeof(name), "RpbufAudioCtrlAmp%d", i);
		g_amp_thread[i] = hal_thread_create(RpbufAudioCtrlAmpThread, (void *)&i, name,
						2048, configAPPLICATION_AAMP_PRIORITY);
		if (!g_amp_thread[i]) {
			aamp_err("Failed to create %s task\r\n", name);
		}
		hal_thread_start(g_amp_thread[i]);
		hal_msleep(20);
	}

	return;

err_out:
	rpbuf_common_deinit();
}

