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

#include "rpdata_asr_process.h"
#include "rpdata_audio_interface.h"
#include "rpdata_audio_ctrl_asr.h"

#include "audio_amp_log.h"

#define RPBUF_AUDIO_CRTL_ASR_QUEUE				(8)

static void *g_asr_thread[RPDATA_CRTL_ASR_COUNTS];

static int g_front_asr_init_done[RPDATA_CRTL_ASR_COUNTS] = { 0 };
static void* g_asr_hld[RPDATA_CRTL_ASR_COUNTS] = { NULL };
static hal_queue_t g_asr_queue[RPDATA_CRTL_ASR_COUNTS] = { NULL };

typedef struct RpdataAudioItem {
	rpdata_audio_ctrl_asr_t rpdata_audio_ctrl;
	int len;
}RpdataAudioItem;

struct rpdata_audio_ctrl_asr_entry {
	void *rx_task;
	int index;
	rpdata_audio_param_t rpdata_arg;
	front_asr_param asr_param;
	struct rpdata_audio_entry * audio_front_ctrl;
	hal_queue_t rpdata_audio_queue;
};

static int  rpdata_ctrl_audio_asr(struct rpdata_audio_ctrl_asr_entry *audio_ctrl_asr_entry, void *data, int data_len)
{
	rpdata_audio_ctrl_asr_t rpdata_ctrl_init;
	RPDATA_CTRL_TYPE rpdata_ctrl_type;
	RPDATA_ALGO_TYPE rpdata_algo_type;
	asr_queue_item asr_item;
	int ret;
	int index = audio_ctrl_asr_entry->index;

	if (index > RPDATA_CRTL_ASR_COUNTS - 1) {
		aamp_err("err! rpbuf_audio_ctrl_asr index %d is large than  %d \n", index, RPDATA_CRTL_ASR_COUNTS);
		return -1;
	}

	if (data_len != sizeof(rpdata_audio_ctrl_asr_t)) {
		aamp_err("rpdata_ctrl_audio get rpdata len %d err!\n", data_len);
		return -1;
	}

	memcpy(&rpdata_ctrl_init, data, data_len);

	rpdata_ctrl_type = rpdata_ctrl_init.cmd;
	rpdata_algo_type = rpdata_ctrl_init.type;

	aamp_info("%s get cmd %d type %d val %d", audio_ctrl_asr_entry->rpdata_arg.name, rpdata_ctrl_init.cmd, rpdata_ctrl_init.type, rpdata_ctrl_init.value);

	switch(rpdata_ctrl_type)
	{

		case RPDATA_CTL_INIT:
		{
			//algo init
			if (rpdata_algo_type == RPDATA_ALGO_ASR) {
				if (!g_front_asr_init_done[index]) {
					g_asr_hld[index] = asr_ctrl_init(index);
					if (!g_asr_hld[index]) {
						aamp_err("asr_ctrl_init err!");
						return -1;
					}
					rpdata_asr_init_default_param(g_asr_hld[index]);
					g_asr_queue[index] = asr_get_queue_handle(g_asr_hld[index]);
					if (!g_asr_queue[index]) {
						aamp_err("asr_get_queue_handle err!");
						return -1;
					}
					g_front_asr_init_done[index] = 1;
				}
			} else {
				aamp_err("rpdata ctrl get algo type %d err!", rpdata_algo_type);
			}
			break;
		}

		case RPDATA_CTL_CONFIG:
		{
			// config
			if (rpdata_ctrl_init.rate == 0 || rpdata_ctrl_init.channels == 0 || rpdata_ctrl_init.afe_out_channels == 0 ||
				rpdata_ctrl_init.bits == 0 || rpdata_ctrl_init.msec == 0) {
				aamp_err("invaild param, bits:%d, channels:%d, afe out channels:%d, rate:%d, msec:%d\n", \
					rpdata_ctrl_init.bits, rpdata_ctrl_init.channels, rpdata_ctrl_init.afe_out_channels,rpdata_ctrl_init.rate, rpdata_ctrl_init.msec);
				return -1;
			}

			if (rpdata_algo_type == RPDATA_ALGO_ASR) {

				front_asr_record_param *asr_record_param = &audio_ctrl_asr_entry->asr_param.asr_record_param;
				asr_record_param->rate = rpdata_ctrl_init.rate;
				asr_record_param->channels = rpdata_ctrl_init.channels;
				asr_record_param->afe_out_channels = rpdata_ctrl_init.afe_out_channels;
				asr_record_param->bits = rpdata_ctrl_init.bits;
				asr_record_param->msec = rpdata_ctrl_init.msec;

				asr_item.cmd = RPDATA_CTL_CONFIG;
				asr_item.arg = asr_record_param;
				asr_item.len = sizeof(front_asr_record_param);

				if (g_asr_hld[index]) {
					ret = hal_queue_send(g_asr_queue[index], &asr_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("asr handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpdata ctrl get algo type %d err!", rpdata_algo_type);
			}
			break;
		}

		case RPDATA_CTL_MSPEECH_STATUS:
		{
			if ((rpdata_ctrl_init.speech_status.log_level < 0 || rpdata_ctrl_init.speech_status.log_level > 3) || \
				(rpdata_ctrl_init.speech_status.awake_threshold_type < 0 || rpdata_ctrl_init.speech_status.awake_threshold_type > 4) || \
				(rpdata_ctrl_init.speech_status.cur_time_hour < 0 || rpdata_ctrl_init.speech_status.cur_time_hour > 23)) {
				aamp_err("invaild param, log_level:%d, awake_threshold_type:%d, cur_time_hour:%d \n", \
					rpdata_ctrl_init.speech_status.log_level, rpdata_ctrl_init.speech_status.awake_threshold_type, rpdata_ctrl_init.speech_status.cur_time_hour);
				return -1;
			}

			if (rpdata_algo_type == RPDATA_ALGO_ASR) {
				speech_status_t *speech_status = &audio_ctrl_asr_entry->asr_param.speech_status;
				memcpy(speech_status, &rpdata_ctrl_init.speech_status, sizeof(speech_status_t));

				asr_item.cmd = RPDATA_CTL_MSPEECH_STATUS;
				asr_item.arg = speech_status;
				asr_item.len = sizeof(speech_status_t);
				if (g_asr_hld[index]) {
					ret = hal_queue_send(g_asr_queue[index], &asr_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("asr handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpdata ctrl get algo type %d err!", rpdata_algo_type);
			}
			break;
		}

		case RPDATA_CTL_START:
		{
			//asr start
			if (rpdata_algo_type == RPDATA_ALGO_ASR) {
				asr_item.cmd = RPDATA_CTL_START;
				asr_item.arg = NULL;
				asr_item.len = 0;
				if (g_asr_hld[index]) {
					ret = hal_queue_send(g_asr_queue[index], &asr_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("asr handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpdata ctrl get algo type %d err!", rpdata_algo_type);
			}
			break;
		}

		case RPDATA_CTL_STOP:
		{
			//asr stop
			if (rpdata_algo_type == RPDATA_ALGO_ASR) {
				asr_item.cmd = RPDATA_CTL_STOP;
				asr_item.arg = NULL;
				asr_item.len = 0;
				if (g_asr_hld[index]) {
					ret = hal_queue_send(g_asr_queue[index], &asr_item);
					if (ret != 0) {
						aamp_err("queue send error\n");
						return ret;
					}
				} else {
					aamp_err("g_asr_hld is null, please init first!\n");
				}
			} else {
				aamp_err("rpdata ctrl get algo type %d err!", rpdata_algo_type);
			}
			break;
		}

		case RPDATA_CTL_RELEASE:
		{
			//asr release
			if (rpdata_algo_type == RPDATA_ALGO_ASR) {
				if (g_front_asr_init_done[index]) {
					asr_item.cmd = RPDATA_CTL_RELEASE;
					asr_item.arg = NULL;
					asr_item.len = 0;
					if (g_asr_hld[index]) {
						ret = hal_queue_send(g_asr_queue[index], &asr_item);
						if (ret != 0) {
							aamp_err("queue send error\n");
							return ret;
						}
						rpdata_asr_deinit_param(g_asr_hld[index]);
						asr_ctrl_destroy(g_asr_hld[index]);
					} else {
						aamp_err("asr handle is null, please init first!\n");
					}
					g_asr_hld[index] = NULL;
					g_front_asr_init_done[index] = 0;
				} else {
					aamp_err("please init first!\n");
				}
			} else {
				aamp_err("rpdata ctrl get algo type %d err!", rpdata_algo_type);
			}
			break;
		}
		case RPDATA_CTL_ENABLE_ALG:
		{
			// enable/disable alg.
			if (rpdata_algo_type == RPDATA_ALGO_ASR) {
				ret = do_rpdata_enable_whole_front_asr(g_asr_hld[index], index, rpdata_ctrl_init.value);
				if (ret != 0) {
					aamp_err("do_rpdata_enable_whole_front_asr\n");
					return -1;
				}
			} else {
				aamp_err("rpdata ctrl get algo type %d err!", rpdata_algo_type);
			}
			break;
		}
		case RPDATA_CTL_ENABLE_VAD:
		{
			// enable/disable vad.
			if (rpdata_algo_type == RPDATA_ALGO_ASR) {
				if (g_asr_hld[index]) {
					ret = do_rpdata_enable_front_vad(g_asr_hld[index], index, rpdata_ctrl_init.value);
					if (ret != 0) {
						aamp_err("do_rpdata_enable_front_vad\n");
						return -1;
					}
				} else {
					aamp_err("asr handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpdata ctrl get algo type %d err!", rpdata_algo_type);
			}
			break;
		}
		case RPDATA_CTL_ENABLE_ASR:
		{
			// enable/disable asr.
			if (rpdata_algo_type == RPDATA_ALGO_ASR) {
				if (g_asr_hld[index]) {
					ret = do_rpdata_enable_front_asr(g_asr_hld[index], index, rpdata_ctrl_init.value);
					if (ret != 0) {
						aamp_err("do_rpdata_enable_front_asr\n");
						return -1;
					}
				} else {
					aamp_err("asr handle is null, please init first!\n");
				}
			} else {
				aamp_err("rpdata ctrl get algo type %d err!", rpdata_algo_type);
			}
			break;
		}
		case RPDATA_CTL_DUMP_MERGE_DATA:
		{
			// enable/disable data dump.
			if (rpdata_algo_type == RPDATA_ALGO_ASR) {
				ret = do_rpdata_enable_front_asr_dump_merge(g_asr_hld[index], index, rpdata_ctrl_init.value);
				if (ret != 0) {
					aamp_err("do_rpdata_enable_front_asr_dump_merge\n");
					return -1;
				}
			} else {
				aamp_err("rpdata ctrl get algo type %d err!", rpdata_algo_type);
			}
			break;
		}
		default:
		{
			aamp_err("rpdata ctrl get type %d err!", rpdata_ctrl_type);
			break;
		}

	 }
	return 0;
}


static int rpdata_ctrl_audio_callback(rpdata_t *rpd, void *data, uint32_t data_len)
{
	struct rpdata_audio_ctrl_asr_entry *audio_ctrl_asr_entry = (struct rpdata_audio_ctrl_asr_entry *)rpdata_get_private_data(rpd);
	RpdataAudioItem rpdata_audio_item;
	int ret = 0;

	if (audio_ctrl_asr_entry == NULL) {
		aamp_err("rpmsg ctrl callback get audio_ctrl_entry is null!");
		return -1;
	}

	if (data_len != sizeof(rpdata_audio_ctrl_asr_t)) {
		aamp_err("rpmsg ctrl callback get rpdata len %d!", data_len);
		return -1;
	}

	memcpy(&rpdata_audio_item.rpdata_audio_ctrl, data, data_len);
	rpdata_audio_item.len = data_len;
	ret = hal_queue_send(audio_ctrl_asr_entry->rpdata_audio_queue, &rpdata_audio_item);
	if (ret != 0) {
		aamp_err("queue send error\n");
		return ret;
	}

	aamp_debug("%s.%p: rx %d Bytes\r\n", audio_ctrl_asr_entry->rpdata_arg.name,
					audio_ctrl_asr_entry->audio_front_ctrl->rpd_buffer, data_len);


	return 0;
}

static void rpdata_audio_asr_rx_thread(void *pram)
{
	struct rpdata_audio_ctrl_asr_entry *audio_ctrl_asr_entry = pram;
	RpdataAudioItem item;
	char name[32];
	int ret;

	snprintf(name, sizeof(name), "%s.%p", audio_ctrl_asr_entry->rpdata_arg.name,
					audio_ctrl_asr_entry->audio_front_ctrl->rpd_buffer);

	aamp_debug("%s rx thread start...\r\n", name);

	while(1) {

		ret = hal_queue_recv(audio_ctrl_asr_entry->rpdata_audio_queue, &item, HAL_WAIT_FOREVER);
		if (ret != 0) {
			aamp_err("Failed to recv queue\n");
			goto out;
		}

		ret = rpdata_ctrl_audio_asr(audio_ctrl_asr_entry, &item.rpdata_audio_ctrl, item.len);
		if (ret != 0) {
			aamp_err("Failed to call rpmsg_ctrl_audio\n");
			continue;
		}
	}

out:
	aamp_debug("%s rx thread exit...\r\n", name);
	hal_thread_stop(audio_ctrl_asr_entry->rx_task);

}


struct rpdata_cbs rpd_audio_front_ctrl_cbs = {
	.recv_cb = (recv_cb_t)rpdata_ctrl_audio_callback,
};

void RpdataAudioCtrlAsrThread(void *arg)
{
	int index = *(int *)arg;
	int ret = 0;
	char name[32];
	struct rpdata_audio_ctrl_asr_entry *audio_ctrl_asr_entry = NULL;

	audio_ctrl_asr_entry = hal_malloc(sizeof(*audio_ctrl_asr_entry));
	if (audio_ctrl_asr_entry == NULL) {
		aamp_err("failed to alloc entry\r\n");
		return;
	}
	memset(audio_ctrl_asr_entry, 0, sizeof(*audio_ctrl_asr_entry));

	audio_ctrl_asr_entry->rpdata_audio_queue = hal_queue_create("rpdata_audio_front", sizeof(RpdataAudioItem), RPBUF_AUDIO_CRTL_ASR_QUEUE);
	if (audio_ctrl_asr_entry->rpdata_audio_queue == NULL) {
		aamp_err("rpdata_audio: Failed to allocate memory for queue\n");
		goto init_err_out;
	}

	audio_ctrl_asr_entry->index = index;

	snprintf(audio_ctrl_asr_entry->rpdata_arg.name, sizeof(audio_ctrl_asr_entry->rpdata_arg.name), "%s_%d", FRONT_RPDATA_CTL_NAME, audio_ctrl_asr_entry->index);
	snprintf(audio_ctrl_asr_entry->rpdata_arg.type, sizeof(audio_ctrl_asr_entry->rpdata_arg.type), "%s_%d", FRONT_RPDATA_CTL_TYPE, audio_ctrl_asr_entry->index);
	audio_ctrl_asr_entry->rpdata_arg.dir = FRONT_RPDATA_CTL_DIR_RV;

	aamp_info("rpdata %s type %s start", audio_ctrl_asr_entry->rpdata_arg.name, audio_ctrl_asr_entry->rpdata_arg.type);

	/* dsp recv, Rv send ctrl cmd.
	 * if remote core don't create the same rpdata, it will block.
	 */
	audio_ctrl_asr_entry->audio_front_ctrl = rpdata_audio_create(&audio_ctrl_asr_entry->rpdata_arg, &rpd_audio_front_ctrl_cbs, 0, 0, audio_ctrl_asr_entry);
	if (audio_ctrl_asr_entry->audio_front_ctrl == NULL) {
		aamp_err("Failed to create rpdata audio =%s (ret: %d)\n", FRONT_RPDATA_CTL_NAME, ret);
		goto init_err_out;
	}

	aamp_info("rpdata %s type %s ready\n", audio_ctrl_asr_entry->rpdata_arg.name, audio_ctrl_asr_entry->rpdata_arg.type);

	snprintf(name, sizeof(name), "RpbufAudioAsrRx%d", audio_ctrl_asr_entry->index);

	audio_ctrl_asr_entry->rx_task = hal_thread_create(rpdata_audio_asr_rx_thread, audio_ctrl_asr_entry, name, \
					2048, configAPPLICATION_AAMP_PRIORITY);
	if (audio_ctrl_asr_entry->rx_task == NULL) {
		aamp_err("Failed to create rpd-audio-ctrl rx_task\r\n");
		goto err_out;
	}
	hal_thread_start(audio_ctrl_asr_entry->rx_task);

	hal_msleep(100);

	hal_thread_stop(g_asr_thread[audio_ctrl_asr_entry->index]);

	g_asr_thread[audio_ctrl_asr_entry->index] = NULL;

	return;


err_out:

	rpdata_audio_destroy(audio_ctrl_asr_entry->audio_front_ctrl);
	audio_ctrl_asr_entry->audio_front_ctrl  = NULL;

init_err_out:

	if (audio_ctrl_asr_entry->rpdata_audio_queue)
		hal_queue_delete(audio_ctrl_asr_entry->rpdata_audio_queue);

	if (audio_ctrl_asr_entry)
		hal_free(audio_ctrl_asr_entry);

	hal_thread_stop(g_asr_thread[audio_ctrl_asr_entry->index]);

	g_asr_thread[audio_ctrl_asr_entry->index] = NULL;

	aamp_info("end");

}

void rpdata_audio_asr_ctrl_init(void)
{
	int i = 0;
	char name[32];

	for (i = 0; i< RPDATA_CRTL_ASR_COUNTS; ++i) {
		snprintf(name, sizeof(name), "RpdataAudioCtrlAsr%d", i);

		g_asr_thread[i] = hal_thread_create(RpdataAudioCtrlAsrThread, (void *)&i, name,
						2048, configAPPLICATION_AAMP_PRIORITY);
		if (!g_asr_thread[i]) {
			aamp_err("Failed to create rpdata-ctrl task\r\n");
		}

		hal_thread_start(g_asr_thread[i]);
		hal_msleep(20);
	}
	return;
}

