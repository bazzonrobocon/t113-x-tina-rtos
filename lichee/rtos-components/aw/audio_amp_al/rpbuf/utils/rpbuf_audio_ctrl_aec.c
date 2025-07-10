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

#include "rpbuf_aec_process.h"
#include "rpbuf_aec_downlink_process.h"
#include "rpbuf_common_interface.h"
#include "rpbuf_audio_ctrl_aec.h"

#include "audio_amp_log.h"

#define RPBUF_AUDIO_CRTL_AEC_QUEUE				(8)

static void *g_aec_thread[RPBUF_CRTL_AEC_COUNTS];

static int g_aec_init_done[RPBUF_CRTL_AEC_COUNTS] = { 0 };
static void* g_aec_hld[RPBUF_CRTL_AEC_COUNTS] = { NULL };
static hal_queue_t g_aec_queue[RPBUF_CRTL_AEC_COUNTS] = { NULL };

static int g_aec_downlink_init_done[RPBUF_CRTL_AEC_COUNTS] = { 0 };
static void* g_aec_downlink_hld[RPBUF_CRTL_AEC_COUNTS] = { NULL };
static hal_queue_t g_aec_downlink_queue[RPBUF_CRTL_AEC_COUNTS] = { NULL };

typedef struct RpbufAudioAecItem {
	rpbuf_audio_local_ctrl_aec_t rpbuf_audio_ctrl_aec;
	int len;
}RpbufAudioAecItem;

struct rpbuf_audio_ctrl_aec_entry {
	void *rx_task;
	int index;
	char name[32];
	char ack_name[32];
	char ack_downlink_name[32];
	aec_ctrl_param_t aec_ctrl_param;
	aec_downlink_ctrl_param_t aec_downlink_ctrl_param;
	#ifdef CONFIG_COMPONENT_NF_AEC
	tdspCfgCore dspCfgCore;
	#endif
	hal_queue_t rpbuf_audio_queue;
};

int rpbuf_audio_ctrl_aec_send_cmd(void *handle, rpbuf_audio_remote_ctrl_aec_t *rpbuf_ctl_arg, unsigned long data_offset)
{
	struct rpbuf_audio_ctrl_aec_entry *rpbuf_hdl = (struct rpbuf_audio_ctrl_aec_entry *)handle;
	int ret = -1;
	uint32_t size = sizeof(rpbuf_audio_remote_ctrl_aec_t);

	if (!rpbuf_hdl) {
		printf("rpbuf_hdl is null!\n");
		return ret;
	}

	ret = rpbuf_common_transmit(rpbuf_hdl->ack_name, (void *)rpbuf_ctl_arg, size, data_offset);
	if (ret < 0) {
		aamp_err("rpbuf_common_transmit (with data) failed\n");
		goto err_out;
	}

	return 0;

err_out:
	return ret;
}

int rpbuf_audio_ctrl_aec_downlink_send_cmd(void *handle, rpbuf_audio_remote_ctrl_aec_t *rpbuf_ctl_arg, unsigned long data_offset)
{
	struct rpbuf_audio_ctrl_aec_entry *rpbuf_hdl = (struct rpbuf_audio_ctrl_aec_entry *)handle;
	int ret = -1;
	uint32_t size = sizeof(rpbuf_audio_remote_ctrl_aec_t);

	if (!rpbuf_hdl) {
		printf("rpbuf_hdl is null!\n");
		return ret;
	}

	ret = rpbuf_common_transmit(rpbuf_hdl->ack_downlink_name, (void *)rpbuf_ctl_arg, size, data_offset);
	if (ret < 0) {
		aamp_err("rpbuf_common_transmit (with data) failed\n");
		goto err_out;
	}

	return 0;

err_out:
	return ret;
}

static int rpbuf_audio_ctrl_aec(struct rpbuf_audio_ctrl_aec_entry *audio_ctrl_aec_entry, void *data, int data_len)
{
	rpbuf_audio_local_ctrl_aec_t rpbuf_ctrl_init;
	RPBUF_CTRL_TYPE rpbuf_ctrl_type;
	RPBUF_ALGO_TYPE rpbuf_algo_type;
	aec_queue_item aec_item;
	aec_downlink_queue_item aec_downlink_item;
	int ret;
	int index = audio_ctrl_aec_entry->index;

	if (index > RPBUF_CRTL_AEC_COUNTS - 1) {
		aamp_err("err! rpbuf_audio_ctrl_eq index %d is large than  %d \n", index, RPBUF_CRTL_AEC_COUNTS);
		return -1;
	}

	if (data_len != sizeof(rpbuf_audio_local_ctrl_aec_t)) {
		aamp_err("rpbuf_audio_ctrl get rpbuf len %d err!\n", data_len);
		return -1;
	}

	memcpy(&rpbuf_ctrl_init, data, data_len);

	rpbuf_ctrl_type = rpbuf_ctrl_init.cmd;
	rpbuf_algo_type = rpbuf_ctrl_init.type;

	aamp_info("get cmd %d type %d value %d",rpbuf_ctrl_init.cmd, rpbuf_ctrl_init.type, rpbuf_ctrl_init.value);

	switch(rpbuf_ctrl_type)
	{

		case RPBUF_CTL_INIT:
		{
			//algo init
			if (rpbuf_algo_type == RPBUF_ALGO_AEC) {
				if (!g_aec_init_done[index]) {
					g_aec_hld[index] = aec_ctrl_init(index, (void*)audio_ctrl_aec_entry);
					if (!g_aec_hld[index]) {
						aamp_err("aec_ctrl_init %d err!", index);
						return -1;
					}
					aamp_info("init aec priv 0x%08x\n", (uint32_t)g_aec_hld[index]);
					ret = rpbuf_aec_transfer_create(g_aec_hld[index]);
					if (ret != 0) {
						aamp_err("rpbuf_aec_transfer_create err!");
						return ret;
					}
					rpbuf_aec_init_default_param(g_aec_hld[index]);
					g_aec_queue[index] = aec_get_queue_handle(g_aec_hld[index]);
					if (!g_aec_queue[index]) {
						aamp_err("aec_get_queue_handle %d err!", index);
						return -1;
					}
					g_aec_init_done[index] = 1;
				}
			} else if (rpbuf_algo_type == RPBUF_ALGO_AEC_DOWNLINK) {
				if (!g_aec_downlink_init_done[index]) {
					g_aec_downlink_hld[index] = aec_downlink_ctrl_init(index, (void*)audio_ctrl_aec_entry);
					if (!g_aec_downlink_hld[index]) {
						aamp_err("eq_ctrl_init err!");
						return -1;
					}
					ret = rpbuf_aec_downlink_transfer_create(g_aec_downlink_hld[index]);
					if (ret != 0) {
						aamp_err("rpbuf_aec_downlink_transfer_create err!");
						return ret;
					}
					rpbuf_aec_downlink_init_default_param(g_aec_downlink_hld[index]);
					g_aec_downlink_queue[index] = aec_downlink_get_queue_handle(g_aec_downlink_hld[index]);
					if (!g_aec_downlink_queue[index]) {
						aamp_err("aec_downlink_get_queue_handle %d err!", index);
						return -1;
					}
					g_aec_downlink_init_done[index] = 1;
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

			if (rpbuf_algo_type == RPBUF_ALGO_AEC) {
				aec_config_param_t *aec_config_param = &audio_ctrl_aec_entry->aec_ctrl_param.aec_config_param;
				aec_config_param->rate = rpbuf_ctrl_init.rate;
				aec_config_param->channels = rpbuf_ctrl_init.channels;
				aec_config_param->aec_out_channels = rpbuf_ctrl_init.out_channels;
				aec_config_param->bits = rpbuf_ctrl_init.bits;
				aec_config_param->msec = rpbuf_ctrl_init.msec;
				aec_config_param->lms_msec = rpbuf_ctrl_init.lms_msec;
				aec_config_param->dspCfgCore = rpbuf_ctrl_init.dspCfgCore;

				if (rpbuf_ctrl_init.dspCfgCore == NULL) {
					aamp_err("aec in dspCfgCore is null\n");
					return -1;
				}
				aec_item.cmd = RPBUF_CTL_CONFIG;
				aec_item.arg = aec_config_param;
				aec_item.len = sizeof(aec_config_param_t);

				if (g_aec_hld[index]) {
					ret = hal_queue_send(g_aec_queue[index], &aec_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("aec handle is null, please init first!\n");
				}
			} else if (rpbuf_algo_type == RPBUF_ALGO_AEC_DOWNLINK) {
				aec_downlink_config_param_t *aec_downlink_config_param = &audio_ctrl_aec_entry->aec_downlink_ctrl_param.aec_downlink_config_param;
				aec_downlink_config_param->rate = rpbuf_ctrl_init.rate;
				aec_downlink_config_param->channels = rpbuf_ctrl_init.channels;
				aec_downlink_config_param->aec_downlink_out_channels = rpbuf_ctrl_init.out_channels;
				aec_downlink_config_param->bits = rpbuf_ctrl_init.bits;
				aec_downlink_config_param->msec = rpbuf_ctrl_init.msec;
				aec_downlink_config_param->dspCfgCore = rpbuf_ctrl_init.dspCfgCore;

				if (rpbuf_ctrl_init.dspCfgCore == NULL) {
					aamp_err("aec downlink dspCfgCore is null\n");
					return -1;
				}
				aec_downlink_item.cmd = RPBUF_CTL_CONFIG;
				aec_downlink_item.arg = aec_downlink_config_param;
				aec_downlink_item.len = sizeof(aec_downlink_config_param_t);

				if (g_aec_downlink_hld[index]) {
					ret = hal_queue_send(g_aec_downlink_queue[index], &aec_downlink_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("aec downlink handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}

		case RPBUF_CTL_START:
		{
			// start
			if (rpbuf_algo_type == RPBUF_ALGO_AEC) {
				aec_item.cmd = RPBUF_CTL_START;
				aec_item.arg = NULL;
				aec_item.len = 0;
				if (g_aec_hld[index]) {
					ret = hal_queue_send(g_aec_queue[index], &aec_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("aec handle is null, please init first!\n");
				}
			} else if (rpbuf_algo_type == RPBUF_ALGO_AEC_DOWNLINK) {
				aec_downlink_item.cmd = RPBUF_CTL_START;
				aec_downlink_item.arg = NULL;
				aec_downlink_item.len = 0;
				if (g_aec_downlink_hld[index]) {
					ret = hal_queue_send(g_aec_downlink_queue[index], &aec_downlink_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("aec downlink handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}

		case RPBUF_CTL_PAUSE:
		{
			// pause
			if (rpbuf_algo_type == RPBUF_ALGO_AEC) {
				aec_item.cmd = RPBUF_CTL_PAUSE;
				aec_item.arg = NULL;
				aec_item.len = 0;
				if (g_aec_hld[index]) {
					ret = hal_queue_send(g_aec_queue[index], &aec_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("aec handle is null, please init first!\n");
				}
			} else if (rpbuf_algo_type == RPBUF_ALGO_AEC_DOWNLINK) {
				aec_downlink_item.cmd = RPBUF_CTL_PAUSE;
				aec_downlink_item.arg = NULL;
				aec_downlink_item.len = 0;
				if (g_aec_downlink_hld[index]) {
					ret = hal_queue_send(g_aec_downlink_queue[index], &aec_downlink_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("aec downlink handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}

		case RPBUF_CTL_RESUME:
		{
			// resume
			if (rpbuf_algo_type == RPBUF_ALGO_AEC) {
				aec_item.cmd = RPBUF_CTL_RESUME;
				aec_item.arg = NULL;
				aec_item.len = 0;
				if (g_aec_hld[index]) {
					ret = hal_queue_send(g_aec_queue[index], &aec_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("asr handle is null, please init first!\n");
				}
			} else if (rpbuf_algo_type == RPBUF_ALGO_AEC_DOWNLINK) {
				aec_downlink_item.cmd = RPBUF_CTL_RESUME;
				aec_downlink_item.arg = NULL;
				aec_downlink_item.len = 0;
				if (g_aec_downlink_hld[index]) {
					ret = hal_queue_send(g_aec_downlink_queue[index], &aec_downlink_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("aec downlink handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}

		case RPBUF_CTL_STOP:
		{
			//stop
			if (rpbuf_algo_type == RPBUF_ALGO_AEC) {
				aec_item.cmd = RPBUF_CTL_STOP;
				aec_item.arg = NULL;
				aec_item.len = 0;
				if (g_aec_hld[index]) {
					ret = hal_queue_send(g_aec_queue[index], &aec_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("asr handle is null, please init first!\n");
				}
			} else if (rpbuf_algo_type == RPBUF_ALGO_AEC_DOWNLINK) {
				aec_downlink_item.cmd = RPBUF_CTL_STOP;
				aec_downlink_item.arg = NULL;
				aec_downlink_item.len = 0;
				if (g_aec_downlink_hld[index]) {
					ret = hal_queue_send(g_aec_downlink_queue[index], &aec_downlink_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("aec downlink handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}

		case RPBUF_CTL_RELEASE:
		{
			//release
			if (rpbuf_algo_type == RPBUF_ALGO_AEC) {
				if (g_aec_init_done[index]) {
					aec_item.cmd = RPBUF_CTL_RELEASE;
					aec_item.arg = NULL;
					aec_item.len = 0;
					if (g_aec_hld[index]) {
						ret = hal_queue_send(g_aec_queue[index], &aec_item);
						if (ret != 0) {
							aamp_err("queue send error\n");
							return ret;
						}
						rpbuf_aec_deinit_param(g_aec_hld[index]);
						aec_ctrl_destroy(g_aec_hld[index]);
					} else {
						aamp_err("aec handle is null, please init first!\n");
					}
					g_aec_hld[index] = NULL;
					g_aec_init_done[index] = 0;
				} else {
					aamp_err("please init aec first!\n");
				}
			} else if (rpbuf_algo_type == RPBUF_ALGO_AEC_DOWNLINK) {
				aec_downlink_item.cmd = RPBUF_CTL_RELEASE;
				aec_downlink_item.arg = NULL;
				aec_downlink_item.len = 0;
				if (g_aec_downlink_hld[index]) {
					ret = hal_queue_send(g_aec_downlink_queue[index], &aec_downlink_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
					rpbuf_aec_downlink_deinit_param(g_aec_downlink_hld[index]);
					aec_downlink_ctrl_destroy(g_aec_downlink_hld[index]);
				} else {
					aamp_err("aec downlink handle is null, please init first!\n");
				}
				g_aec_downlink_hld[index] = NULL;
				g_aec_downlink_init_done[index] = 0;
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}
		case RPBUF_CTL_ENABLE_ALG:
		{
			// enable/disable alg.
			if (rpbuf_algo_type == RPBUF_ALGO_AEC) {
				ret = do_rpbuf_enable_aec(g_aec_hld[index], index, rpbuf_ctrl_init.value);
				if (ret != 0) {
					aamp_err("do_rpbuf_enable_aec failed\n");
					return -1;
				}
			} else if (rpbuf_algo_type == RPBUF_ALGO_AEC_DOWNLINK) {
				ret = do_rpbuf_enable_aec_downlink(g_aec_downlink_hld[index], index, rpbuf_ctrl_init.value);
				if (ret != 0) {
					aamp_err("do_rpbuf_enable_aec_downlink failed\n");
					return -1;
				}
			} else {
				aamp_err("rpbuf ctrl get algo type %d err!", rpbuf_algo_type);
			}
			break;
		}
		case RPBUF_CTL_DUMP_MERGE_DATA:
		{
			// enable/disable data dump.
			if (rpbuf_algo_type == RPBUF_ALGO_AEC) {
				ret = do_rpbuf_enable_aec_dump_merge(g_aec_hld[index], index, rpbuf_ctrl_init.value);
				if (ret != 0) {
					aamp_err("do_rpbuf_enable_aec_dump_merge failed\n");
					return -1;
				}
			} else if (rpbuf_algo_type == RPBUF_ALGO_AEC_DOWNLINK) {
				ret = do_rpbuf_enable_aec_downlink_dump_merge(g_aec_downlink_hld[index], index, rpbuf_ctrl_init.value);
				if (ret != 0) {
					aamp_err("do_rpbuf_enable_aec_downlink_dump_merge failed\n");
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

static void rpbuf_audio_aec_rx_thread(void *pram)
{
	struct rpbuf_audio_ctrl_aec_entry *audio_ctrl_aec_entry = pram;
	RpbufAudioAecItem item;
	int ret;

	aamp_debug("%s rx thread start...\r\n", audio_ctrl_aec_entry->name);

	while(1) {

		ret = hal_queue_recv(audio_ctrl_aec_entry->rpbuf_audio_queue, &item, HAL_WAIT_FOREVER);
		if (ret != 0) {
			aamp_err("Failed to recv queue\n");
			goto out;
		}

		ret = rpbuf_audio_ctrl_aec(audio_ctrl_aec_entry, &item.rpbuf_audio_ctrl_aec, item.len);
		if (ret != 0) {
			aamp_err("Failed to call rpbuf_audio_ctrl_aec\n");
			continue;
		}
	}

out:
	aamp_info("%s rx thread exit...\r\n", audio_ctrl_aec_entry->name);
	hal_thread_stop(audio_ctrl_aec_entry->rx_task);

}

static void rpbuf_audio_available_cb(struct rpbuf_buffer *buffer, void *priv)
{
	aamp_debug("buffer \"%s\" is available\n", rpbuf_buffer_name(buffer));
}

static int rpbuf_audio_ctrl_aec_callback(struct rpbuf_buffer *buffer,
		void *data, int data_len, void *priv)
{
	struct rpbuf_audio_ctrl_aec_entry *audio_ctrl_aec_entry = (struct rpbuf_audio_ctrl_aec_entry *)priv;
	RpbufAudioAecItem audio_item;
	rpbuf_audio_remote_ctrl_aec_t * rpbuf_audio_remote_ctrl_aec= (rpbuf_audio_remote_ctrl_aec_t *)data;
	int ret = 0;

	if (audio_ctrl_aec_entry == NULL) {
		aamp_err("rpbuf audio ctrl callback get audio_ctrl_entry is null!");
		return -1;
	}

	if (data_len != sizeof(rpbuf_audio_remote_ctrl_aec_t)) {
		aamp_err("rpbuf audio callback get rpdata len %d!", data_len);
		return -1;
	}

	/* convert remote audio type to local audio type */
	memset(&audio_item, 0, sizeof(RpbufAudioAecItem));
	audio_item.rpbuf_audio_ctrl_aec.cmd = rpbuf_audio_remote_ctrl_aec->cmd;
	audio_item.rpbuf_audio_ctrl_aec.type = rpbuf_audio_remote_ctrl_aec->type;
	audio_item.rpbuf_audio_ctrl_aec.value = rpbuf_audio_remote_ctrl_aec->value;
	if (audio_item.rpbuf_audio_ctrl_aec.cmd == RPBUF_CTL_CONFIG && audio_item.rpbuf_audio_ctrl_aec.type == RPBUF_ALGO_AEC) {
		audio_item.rpbuf_audio_ctrl_aec.rate = rpbuf_audio_remote_ctrl_aec->rate;
		audio_item.rpbuf_audio_ctrl_aec.msec = rpbuf_audio_remote_ctrl_aec->msec;
		audio_item.rpbuf_audio_ctrl_aec.lms_msec = rpbuf_audio_remote_ctrl_aec->lms_msec;
		audio_item.rpbuf_audio_ctrl_aec.channels = rpbuf_audio_remote_ctrl_aec->channels;
		audio_item.rpbuf_audio_ctrl_aec.out_channels = rpbuf_audio_remote_ctrl_aec->out_channels;
		audio_item.rpbuf_audio_ctrl_aec.bits = rpbuf_audio_remote_ctrl_aec->bits;
		#ifdef CONFIG_COMPONENT_NF_AEC
		audio_item.rpbuf_audio_ctrl_aec.dspCfgCore = &audio_ctrl_aec_entry->dspCfgCore;
		memcpy(audio_item.rpbuf_audio_ctrl_aec.dspCfgCore, &rpbuf_audio_remote_ctrl_aec->dspCfgCore, sizeof(tdspCfgCore));
		#endif
	} else if (audio_item.rpbuf_audio_ctrl_aec.cmd == RPBUF_CTL_CONFIG && audio_item.rpbuf_audio_ctrl_aec.type == RPBUF_ALGO_AEC_DOWNLINK) {
		audio_item.rpbuf_audio_ctrl_aec.rate = rpbuf_audio_remote_ctrl_aec->rate;
		audio_item.rpbuf_audio_ctrl_aec.msec = rpbuf_audio_remote_ctrl_aec->msec;
		audio_item.rpbuf_audio_ctrl_aec.channels = rpbuf_audio_remote_ctrl_aec->channels;
		audio_item.rpbuf_audio_ctrl_aec.out_channels = rpbuf_audio_remote_ctrl_aec->out_channels;
		audio_item.rpbuf_audio_ctrl_aec.bits = rpbuf_audio_remote_ctrl_aec->bits;
		#ifdef CONFIG_COMPONENT_NF_AEC
		audio_item.rpbuf_audio_ctrl_aec.dspCfgCore = &audio_ctrl_aec_entry->dspCfgCore;
		memcpy(audio_item.rpbuf_audio_ctrl_aec.dspCfgCore, &rpbuf_audio_remote_ctrl_aec->dspCfgCore, sizeof(tdspCfgCore));
		#endif
	}
	audio_item.len = sizeof(rpbuf_audio_local_ctrl_aec_t);
	ret = hal_queue_send(audio_ctrl_aec_entry->rpbuf_audio_queue, &audio_item);
	if (ret != 0) {
		aamp_err("queue send error\n");
		return ret;
	}

	aamp_debug("%s.%p: rx %d Bytes\r\n", audio_ctrl_aec_entry->name,
					rpbuf_buffer_va(buffer), data_len);


	return 0;
}

static int rpbuf_audio_destroyed_cb(struct rpbuf_buffer *buffer, void *priv)
{
	struct rpbuf_audio_ctrl_aec_entry *private = (struct rpbuf_audio_ctrl_aec_entry *)priv;

	aamp_debug("buffer \"%s\": remote buffer destroyed\n", rpbuf_buffer_name(buffer));
	if (!strncmp(rpbuf_buffer_name(buffer), private->name, strlen(private->name)))
	{
		aamp_debug("buffer \"%s\": recv thread will exit\n", rpbuf_buffer_name(buffer));
	}
	return 0;
}

static const struct rpbuf_buffer_cbs rpbuf_audio_ctrl_aec_cbs = {
	.available_cb 	= rpbuf_audio_available_cb,
	.rx_cb 			= rpbuf_audio_ctrl_aec_callback,
	.destroyed_cb 	= rpbuf_audio_destroyed_cb,
};

static const struct rpbuf_buffer_cbs rpbuf_audio_ctrl_aec_ack_cbs = {
	.available_cb 	= rpbuf_audio_available_cb,
	.rx_cb 			= NULL,
	.destroyed_cb 	= rpbuf_audio_destroyed_cb,
};

void RpbufAudioCtrlAecThread(void *arg)
{
	int index = *(int *)arg;
	char name[32];
	int ret = 0;
	struct rpbuf_audio_ctrl_aec_entry *audio_ctrl_aec_entry = NULL;

	audio_ctrl_aec_entry = hal_malloc(sizeof(*audio_ctrl_aec_entry));
	if (audio_ctrl_aec_entry == NULL) {
		aamp_err("failed to alloc entry\r\n");
		return;
	}
	memset(audio_ctrl_aec_entry, 0, sizeof(*audio_ctrl_aec_entry));

	audio_ctrl_aec_entry->rpbuf_audio_queue = hal_queue_create("rpbuf_audio_ctrl_aec", sizeof(RpbufAudioAecItem), RPBUF_AUDIO_CRTL_AEC_QUEUE);
	if (audio_ctrl_aec_entry->rpbuf_audio_queue == NULL) {
		aamp_err("rpbuf_audio: Failed to allocate memory for queue\n");
		goto init_err_out;
	}

	audio_ctrl_aec_entry->index = index;

	snprintf(audio_ctrl_aec_entry->name, sizeof(audio_ctrl_aec_entry->name), "%s_%d", RPBUF_CTL_AEC_NAME, audio_ctrl_aec_entry->index);

	aamp_info("rpbuf %s start", audio_ctrl_aec_entry->name);

	/* dsp recv, A7 send ctrl cmd.
	 */
	ret = rpbuf_common_create(RPBUF_CTRL_ID, audio_ctrl_aec_entry->name, sizeof(rpbuf_audio_remote_ctrl_aec_t), 1, &rpbuf_audio_ctrl_aec_cbs, audio_ctrl_aec_entry);
	if (ret < 0) {
		aamp_err("rpbuf_create for name %s (len: %d) failed\n", audio_ctrl_aec_entry->name, sizeof(rpbuf_audio_remote_ctrl_aec_t));
		goto init_err_out;
	}

	while (!rpbuf_common_is_available(audio_ctrl_aec_entry->name)) {
		//aamp_err("rpbuf %s wait\n",  audio_ctrl_aec_entry->name);
		hal_msleep(10);
	}

	aamp_info("rpbuf %s ready", audio_ctrl_aec_entry->name);

	snprintf(audio_ctrl_aec_entry->ack_name, sizeof(audio_ctrl_aec_entry->ack_name), "%s_%d", RPBUF_CTL_AEC_ACK_NAME, audio_ctrl_aec_entry->index);

	aamp_info("rpbuf %s start", audio_ctrl_aec_entry->ack_name);

	/* dsp send, A7 recv ctrl cmd.
	 */
	ret = rpbuf_common_create(RPBUF_CTRL_ID, audio_ctrl_aec_entry->ack_name, sizeof(rpbuf_audio_remote_ctrl_aec_t), 0, &rpbuf_audio_ctrl_aec_ack_cbs, audio_ctrl_aec_entry);
	if (ret < 0) {
		aamp_err("rpbuf_create for name %s (len: %d) failed\n", audio_ctrl_aec_entry->ack_name, sizeof(rpbuf_audio_remote_ctrl_aec_t));
		goto init_err_out;
	}

	while (!rpbuf_common_is_available(audio_ctrl_aec_entry->ack_name)) {
		//aamp_err("rpbuf %s wait\n",  audio_ctrl_aec_entry->ack_name);
		hal_msleep(10);
	}

	snprintf(audio_ctrl_aec_entry->ack_downlink_name, sizeof(audio_ctrl_aec_entry->ack_downlink_name), "%s_%d", RPBUF_CTL_AEC_DOWNLINK_ACK_NAME, audio_ctrl_aec_entry->index);

	aamp_info("rpbuf %s start", audio_ctrl_aec_entry->ack_downlink_name);

	/* dsp send, A7 recv ctrl cmd.
	 */
	ret = rpbuf_common_create(RPBUF_CTRL_ID, audio_ctrl_aec_entry->ack_downlink_name, sizeof(rpbuf_audio_remote_ctrl_aec_t), 0, &rpbuf_audio_ctrl_aec_ack_cbs, audio_ctrl_aec_entry);
	if (ret < 0) {
		aamp_err("rpbuf_create for name %s (len: %d) failed\n", audio_ctrl_aec_entry->ack_downlink_name, sizeof(rpbuf_audio_remote_ctrl_aec_t));
		goto init_err_out;
	}

	while (!rpbuf_common_is_available(audio_ctrl_aec_entry->ack_downlink_name)) {
		//aamp_err("rpbuf %s wait\n",  audio_ctrl_aec_entry->ack_downlink_name);
		hal_msleep(10);
	}

	aamp_info("rpbuf %s ready", audio_ctrl_aec_entry->ack_downlink_name);

	snprintf(name, sizeof(name), "RpbufAudioAecRx%d", audio_ctrl_aec_entry->index);

	audio_ctrl_aec_entry->rx_task = hal_thread_create(rpbuf_audio_aec_rx_thread, audio_ctrl_aec_entry, name, \
					2048, configAPPLICATION_AAMP_PRIORITY);
	if (audio_ctrl_aec_entry->rx_task == NULL) {
		aamp_err("Failed to create %s rx_task\r\n", name);
		goto err_out;
	}
	hal_thread_start(audio_ctrl_aec_entry->rx_task);

	hal_msleep(100);

	hal_thread_stop(g_aec_thread[audio_ctrl_aec_entry->index]);

	g_aec_thread[audio_ctrl_aec_entry->index] = NULL;

	return;


err_out:

	rpbuf_common_destroy(audio_ctrl_aec_entry->name);
	rpbuf_common_destroy(audio_ctrl_aec_entry->ack_name);
	rpbuf_common_destroy(audio_ctrl_aec_entry->ack_downlink_name);

init_err_out:

	if (audio_ctrl_aec_entry->rpbuf_audio_queue)
		hal_queue_delete(audio_ctrl_aec_entry->rpbuf_audio_queue);

	if (audio_ctrl_aec_entry)
		hal_free(audio_ctrl_aec_entry);

	hal_thread_stop(g_aec_thread[audio_ctrl_aec_entry->index]);

	g_aec_thread[audio_ctrl_aec_entry->index] = NULL;

	aamp_info("end");

}

void rpbuf_audio_ctrl_aec_init(void)
{
	int ret;
	int i = 0;
	char name[32];

	ret = rpbuf_common_init();
	if (ret < 0) {
		aamp_err("rpbuf_common_init failed\n");
		goto err_out;
	}
#ifdef OMX_COMMON_OUTPORT_SEM
	ret = omx_init_mutex();
	if (ret < 0) {
		aamp_err("omx_init_mutex failed\n");
		goto err_out;
	}
#endif
	for (i = 0; i< RPBUF_CRTL_AEC_COUNTS; ++i) {
		snprintf(name, sizeof(name), "RpbufAudioCtrlAec%d", i);
		g_aec_thread[i] = hal_thread_create(RpbufAudioCtrlAecThread, (void *)&i, name,
						2048, configAPPLICATION_AAMP_PRIORITY);
		if (!g_aec_thread[i]) {
			aamp_err("Failed to create rpbuf-audio-ctrl task\r\n");
		}

		hal_thread_start(g_aec_thread[i]);
		hal_msleep(20);
	}

	return;

err_out:
	rpbuf_common_deinit();
#ifdef OMX_COMMON_OUTPORT_SEM
	omx_deinit_mutex();
#endif
}

