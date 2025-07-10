/**
  audio_process/rpbuf_audio_module_process.c

  This simple test is a demo for audio module process.

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
#include <unistd.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <hal_time.h>
#include <hal_thread.h>
#include <hal_mem.h>
#include <queue.h>
#include <aw_list.h>

#include "rpbuf_common_interface.h"
#include "rpbuf_audio_module_process.h"
#include "audio_amp_log.h"

/* config audio param */
static int g_amp_dump_merge[RPBUF_CRTL_AMP_COUNTS] = { 0 };
static int g_print_prms = 1;

#define AMP_EV_DATA_GET (1 << 0)
#define AMP_EV_DATA_SET (1 << 1)

static void rpbuf_available_cb(struct rpbuf_buffer *buffer, void *priv)
{
	aamp_info("buffer \"%s\" is available\n", rpbuf_buffer_name(buffer));
}


static int rpbuf_amp_in_rx_cb(struct rpbuf_buffer *buffer,
		void *data, int data_len, void *priv)
{
	AudioModuleProcessPrivateType *private = (AudioModuleProcessPrivateType *)priv;
	int ret = 0;
	int write_size = 0;
	int size = -1;
	int offset = 0;
	int timeout =  1000;
/*
	aamp_err("buffer \"%s\" rece(addr:%p,offset:%d,len:%d) t[%ld] delta[%ld]\n", \
			rpbuf_buffer_name(buffer), rpbuf_buffer_va(buffer),
			data - rpbuf_buffer_va(buffer), data_len, OSTICK_TO_MS(hal_tick_get()), OSTICK_TO_MS(hal_tick_get()) - g_recv_time);

	g_recv_time = OSTICK_TO_MS(hal_tick_get());
*/
	if (data_len != private->rpbuf_in_len) {
		aamp_err("buffer \"%s\" received data len err! (offset: %d, len: %d): %d\n", \
				rpbuf_buffer_name(buffer), data - rpbuf_buffer_va(buffer), data_len, private->rpbuf_in_len);
		ret = -1;
		return ret;
	}

	write_size = data_len;

	while (write_size > 0) {

		size = hal_ringbuffer_put(private->amp_rb, data + offset, write_size);
		if (size < 0) {
			aamp_err("ring buf put err %d\n", ret);
			goto exit;
		}

		if (size == write_size)
			break;

		offset += size;
		write_size -= size;

		ret = hal_event_wait(private->event, AMP_EV_DATA_GET,
			HAL_EVENT_OPTION_CLEAR | HAL_EVENT_OPTION_AND, timeout);
		if (!ret) {
			ret = -1;
			aamp_err("amp in cb wait data timeout:%d", timeout);
			break;
		}
	}

	hal_event_set_bits(private->event, AMP_EV_DATA_SET);

	memset(data, 0, data_len);

exit:
	return ret;
}

static int rpbuf_destroyed_cb(struct rpbuf_buffer *buffer, void *priv)
{
	AudioModuleProcessPrivateType *private = (AudioModuleProcessPrivateType *)priv;
	int index = private->index;

	aamp_debug("buffer \"%s\": remote buffer destroyed\n", rpbuf_buffer_name(buffer));
	if (!strncmp(rpbuf_buffer_name(buffer), private->rpbuf_transfer_in_name[index], sizeof(private->rpbuf_transfer_in_name[index])))
	{
		aamp_debug("buffer \"%s\": recv thread will exit\n", rpbuf_buffer_name(buffer));
	}
	return 0;
}

static const struct rpbuf_buffer_cbs rpbuf_amp_in_cbs = {
	.available_cb 	= rpbuf_available_cb,
	.rx_cb 			= rpbuf_amp_in_rx_cb,
	.destroyed_cb 	= rpbuf_destroyed_cb,
};

static const struct rpbuf_buffer_cbs rpbuf_amp_out_cbs = {
	.available_cb 	= rpbuf_available_cb,
	.rx_cb 			= NULL,
	.destroyed_cb 	= rpbuf_destroyed_cb,
};

void rpbuf_audio_module_process_default_param(void *private)
{
	AudioModuleProcessPrivateType* priv = (AudioModuleProcessPrivateType*)private;

	priv->module_process_ctrl_param.module_process_config_param.msec = RPBUF_AUDIO_MODULE_PORCESS_DEFAULT_MSEC;
	priv->module_process_ctrl_param.module_process_config_param.rate = RPBUF_AUDIO_MODULE_PORCESS_DEFAULT_RATE;
	priv->module_process_ctrl_param.module_process_config_param.in_channels = RPBUF_AUDIO_MODULE_PORCESS_DEFAULT_CHANNELS;
	priv->module_process_ctrl_param.module_process_config_param.out_channels = RPBUF_AUDIO_MODULE_PORCESS_DEFAULT_OUT_CHANNELS;
	priv->module_process_ctrl_param.module_process_config_param.bits = RPBUF_AUDIO_MODULE_PORCESS_DEFAULT_BIT_WIDTH;
	priv->module_process_ctrl_param.module_process_config_param.gsound_buf = NULL;
	priv->module_process_ctrl_param.module_process_config_param.gsound_len = 0;
}

void rpbuf_audio_module_process_deinit_param(void *private)
{
	AudioModuleProcessPrivateType* priv = (AudioModuleProcessPrivateType*)private;

	g_amp_dump_merge[priv->index] = 0;
}

int  audio_algorithm_process(AudioModuleProcessPrivateType *priv,
					void *source, uint32_t in_len,
					void **ppdest, uint32_t *out_len)
{
	audio_module_process_config_param_t *amp_config_param = NULL;
	int frame_bytes = 0;
	int sampleperframe = 0;
	int in_chan = 0;
	int out_chan = 0;

	if (!priv) {
		aamp_err("audio_algorithm error, priv is NULL pointer!\n");
		return -1;
	}

	amp_config_param = &priv->module_process_ctrl_param.module_process_config_param;

	frame_bytes = amp_config_param->bits / 8 * out_chan;
	sampleperframe = (amp_config_param->rate * amp_config_param->msec) / 1000;
	in_chan = amp_config_param->in_channels;
	out_chan = amp_config_param->out_channels;

	/* before algo process */
	if (priv->module_process_ctrl_param.amp_dump)
		memcpy(*ppdest, source, in_len);

	/* after algo process */
	if (priv->module_process_ctrl_param.amp_dump) {
		*out_len = (in_len / in_chan) * (in_chan + out_chan);
		/* this source must be algo process data */
		memcpy((char *)*ppdest + in_len, (char *)source, frame_bytes * sampleperframe);
	}
	else {
		*out_len = (in_len / in_chan) * out_chan;
		memcpy(*ppdest, source, *out_len);
	}
	return 0;
}

static void audio_module_process_thread(void *arg)
{
	AudioModuleProcessPrivateType *private = (AudioModuleProcessPrivateType *)arg;
	void* data = NULL;
	int read_size = 0;
	int ret;
	uint32_t data_len = private->rpbuf_in_len;
	int read_bytes;
	int timeout = 300;
	int cnt = 0;
	int offset = 0;
	uint32_t read_len = 0;
	uint32_t output_len = 0;
	void *output_data = NULL;
	int write_size = 0;
	int size = 0;

	data = hal_malloc(data_len * 2);
	if (data == NULL) {
		aamp_err("no memory\n");
		goto exit;
	}

	output_data = hal_malloc(data_len * 2);
	if (output_data == NULL) {
		aamp_err("no memory\n");
		goto exit;
	}

	while (1)
	{
		hal_mutex_lock(private->process_mutex);

		read_size = private->rpbuf_in_len;
		offset = 0;
		read_len = 0;
		cnt = 0;

		while (read_size > 0) {

			read_bytes = hal_ringbuffer_get(private->amp_rb, data + offset, read_size, timeout);
			if(read_bytes < 0) {
				aamp_err("read err:%d\n", read_bytes);
				if (private->process_mutex)
					hal_mutex_unlock(private->process_mutex);
				goto exit;
			}

			read_len += read_bytes;

			if (read_bytes == read_size)
				break;

			if (read_bytes == 0) {
				cnt++;
				if (cnt > 3 || private->startup_count == 0) {
					cnt = 0;
					break;
				}
			}

			offset += read_bytes;
			read_size -= read_bytes;

			ret = hal_event_wait(private->event, AMP_EV_DATA_SET,
				HAL_EVENT_OPTION_CLEAR | HAL_EVENT_OPTION_AND, timeout);
			if (!ret) {
				aamp_info("amp process wait data timeout:%d\n", timeout);
				continue;
			}
		}

		hal_event_set_bits(private->event, AMP_EV_DATA_GET);

		if (read_len > 0 && private->rpbuf_in_len != read_len)
		{
			aamp_err("amp data len %u is not equal to read_bytes %u\n", private->rpbuf_in_len, read_len);
			hal_mutex_unlock(private->process_mutex);
			goto exit;
		}

		hal_mutex_unlock(private->process_mutex);

		if (read_len == 0) {
			if (private->startup_count == 0) {
				aamp_info("exit process loop!\n");
				break;
			}
			aamp_info("ringbuffer is empty");
			continue;
		}

		/* algorithm to do */
		ret = audio_algorithm_process(private, data, read_len, &output_data, &output_len);
		if (ret != 0)
		{
			aamp_err("audio_algorithm_process err %d read len %d output_len %u", ret, read_len, output_len);
			goto exit;
		}

		write_size = output_len;
		offset = 0;

		while (write_size > 0) {

			size = hal_ringbuffer_put(private->amp_out_rb, output_data + offset, write_size);
			if (size < 0) {
				aamp_err("ring buf put err %d\n", ret);
				break;
			}

			if (size == write_size)
				break;

			offset += size;
			write_size -= size;

			ret = hal_event_wait(private->transfer_event, AMP_EV_DATA_GET,
				HAL_EVENT_OPTION_CLEAR | HAL_EVENT_OPTION_AND, timeout);
			if (!ret) {
				ret = -1;
				aamp_info("amp wait data timeout:%d", timeout);
				break;
			}
		}

		hal_event_set_bits(private->transfer_event, AMP_EV_DATA_SET);

	}

exit:

	if (!private->startup_count)
		hal_sem_post(private->thread_sync);

	if (data)
		hal_free(data);
	
	if (output_data)
		hal_free(output_data);

	private->process_task_exit_flag = 1;
	
	aamp_info("audio_module_process_thread stop \r\n");

	hal_thread_stop(private->amp_process_task);

	return;
}

static void audio_module_transfer_thread(void *arg)
{
	AudioModuleProcessPrivateType *private = (AudioModuleProcessPrivateType *)arg;
	int read_bytes = 0;
	void *data = NULL;
	unsigned int data_len = private->rpbuf_out_len;
	int timeout = 300;
	int read_size = 0;
	int read_len = 0;
	int cnt = 0;
	int ret = 0;

	aamp_info("audio_module_transfer_thread start \n");

	data = malloc(data_len * 2);
	if (!data) {
		aamp_err("malloc len :%d failed\n", data_len);
		goto exit;
	}

	while (1) {

		hal_mutex_lock(private->transfer_mutex);

		read_size = private->rpbuf_out_len;
		read_len = 0;
		cnt = 0;
		read_bytes = 0;

		while (read_size > 0) {

			read_bytes = hal_ringbuffer_get(private->amp_out_rb, data + read_bytes, read_size, timeout);
			if(read_bytes < 0) {
				aamp_err("read err:%d\n", read_bytes);
				hal_mutex_unlock(private->transfer_mutex);
				goto exit;
			}

			read_len += read_bytes;

			if (read_bytes == read_size)
				break;

			if (read_bytes == 0) {
				cnt++;
				if (cnt > 3 || private->startup_count == 0) {
					cnt = 0;
					break;
				}
			}

			read_size -= read_bytes;
			
			ret = hal_event_wait(private->transfer_event, AMP_EV_DATA_SET,
				HAL_EVENT_OPTION_CLEAR | HAL_EVENT_OPTION_AND, timeout);
			if (!ret) {
				ret = -1;
				aamp_info("send wait data timeout:%d\n", timeout);
				continue;
			}
		}

		hal_event_set_bits(private->transfer_event, AMP_EV_DATA_GET);

		if (private->startup_count == 0) {
			aamp_info("exit transfer loop!\n");
			hal_mutex_unlock(private->transfer_mutex);
			break;
		}

		if (read_len > 0 && private->rpbuf_out_len != read_len)
		{
			aamp_err("send data len %d is not equal to read_bytes %d\n", private->rpbuf_out_len, read_len);
			hal_mutex_unlock(private->transfer_mutex);
			goto exit;
		}

		hal_mutex_unlock(private->transfer_mutex);

		if (read_len > 0 && private->startup_count > 0) {
			ret = rpbuf_common_transmit(private->rpbuf_transfer_out_name[private->index], data, read_len, 0);
			if (ret < 0) {
				aamp_err("rpbuf_common_transmit (with data) failed\n");
				break;
			}
		}
	}

exit:

	if (data) {
		free(data);
		data = NULL;
	}

	private->transfer_task_exit_flag = 1;

	aamp_info("end of audio_module_transfer_thread\n");

	hal_thread_stop(private->amp_transfer_task);

	return;
}

int do_rpbuf_enable_amp_dump_merge(void *private, int index, int val)
{
	AudioModuleProcessPrivateType* priv = (AudioModuleProcessPrivateType*)private;
	int ret = 0;

	g_amp_dump_merge[index] = val;

	if (priv == NULL)
		return ret;

	aamp_info("do_rpbuf_enable_amp_dump_merge %d\n", val);
	return ret;

}

int do_audio_module_process_config(AudioModuleProcessPrivateType *priv)
{
	audio_module_process_config_param_t *amp_config_param = NULL;
	int num_bytes_copied = 0;
	int frame_bytes = 0;
	int rpbuf_in_len = 0;
	int rpbuf_out_len = 0;

	if (priv == NULL)
		return -1;

	amp_config_param = &priv->module_process_ctrl_param.module_process_config_param;

	if (amp_config_param->bits == 0 || amp_config_param->in_channels == 0 || amp_config_param->out_channels == 0 ||
		amp_config_param->rate == 0 || amp_config_param->msec == 0 ) {
		aamp_err("amp config param is null");
		return -1;
	}

	frame_bytes = amp_config_param->bits / 8 * amp_config_param->in_channels;

	rpbuf_in_len = (frame_bytes * amp_config_param->rate * amp_config_param->msec) / 1000; /* 4 * 16 * 64 = 4096 bytes */

	if (priv->module_process_ctrl_param.amp_dump)
		rpbuf_out_len = (rpbuf_in_len / amp_config_param->in_channels) * \
					(amp_config_param->in_channels + amp_config_param->out_channels);
	else
		rpbuf_out_len = (rpbuf_in_len / amp_config_param->in_channels) * (amp_config_param->out_channels); // (4096/ 2)  * 1 (1ch )= 2048 bytes

	if (rpbuf_in_len > priv->rpbuf_in_len * 2 || rpbuf_out_len > priv->rpbuf_out_len * 2) 
	{
		aamp_err("process len %d is less than config len %d, transfer len %d is less than config  tranfers len %d",
					priv->rpbuf_in_len * 2, rpbuf_in_len, priv->rpbuf_out_len * 2, rpbuf_out_len);
		return -1;
	}

	/* update config parms */
	if (rpbuf_in_len != priv->rpbuf_in_len) {
		hal_mutex_lock(priv->process_mutex);
		priv->rpbuf_in_len = rpbuf_in_len;
		hal_mutex_unlock(priv->process_mutex);
	}

	if (rpbuf_out_len != priv->rpbuf_out_len) {
		hal_mutex_lock(priv->transfer_mutex);
		priv->rpbuf_out_len = rpbuf_out_len;
		hal_mutex_unlock(priv->transfer_mutex);
	}

	/* print algo params */
	if (g_print_prms && amp_config_param->gsound_len) {
		while (num_bytes_copied < amp_config_param->gsound_len) {
			printf("%02X ", ((uint8_t *)amp_config_param->gsound_buf)[num_bytes_copied]);
			num_bytes_copied++;
			if (num_bytes_copied % 16 == 0) {
					printf("\n"); // 每16个字节之间添加一个空格
			}
		}
	}
	return 0;
}

static int do_audio_module_process_start(AudioModuleProcessPrivateType *priv)
{
	int ret = -1;
	audio_module_process_config_param_t *amp_config_param = NULL;
	char name[32];
	int num_bytes_copied = 0;

	if (priv == NULL)
		return -1;

	amp_config_param = &priv->module_process_ctrl_param.module_process_config_param;

	if (amp_config_param->bits == 0 || amp_config_param->in_channels == 0 || amp_config_param->out_channels == 0 ||
		amp_config_param->rate == 0 || amp_config_param->msec == 0) {
		aamp_err("amp config param is null");
		return -1;
	}

	priv->startup_count++;
	if (priv->startup_count > 1) {
		if (priv->process_task_exit_flag == 0 && priv->transfer_task_exit_flag == 0) {
			aamp_info("threads have been create, so return");
			priv->startup_count--;
			return 0;
		}
		else {
			aamp_err("remote maybe crash, restart thread");
			priv->startup_count--;
		}
	}

	if (priv->amp_process_task == NULL || priv->process_task_exit_flag == 1) {
		snprintf(name, sizeof(name), "amp_process_thread_%d", priv->index);
		aamp_info("create %s", name);
		priv->amp_process_task = hal_thread_create(audio_module_process_thread, priv, name,
						8192, configAPPLICATION_AAMP_PRIORITY);
		if (priv->amp_process_task == NULL) {
			aamp_err("Failed to create amp_process task");
			ret = -1;
			goto exit;
		}
		hal_thread_start(priv->amp_process_task);
		priv->process_task_exit_flag = 0;
	}

	if (priv->amp_transfer_task == NULL || priv->transfer_task_exit_flag == 1) {
		snprintf(name, sizeof(name), "amp_transfer_thread_%d", priv->index);
		aamp_info("create %s", name);
		priv->amp_transfer_task = hal_thread_create(audio_module_transfer_thread, priv, name,
						4096, configAPPLICATION_AAMP_PRIORITY);
		if (priv->amp_transfer_task == NULL) {
			aamp_err("Failed to create amp transfer task");
			ret = -1;
			goto exit;
		}
		hal_thread_start(priv->amp_transfer_task);
		priv->transfer_task_exit_flag = 0;
	}
	/* init algo  */

	/* print algo params */
	if (g_print_prms && amp_config_param->gsound_len) {
		while (num_bytes_copied < amp_config_param->gsound_len) {
			printf("%02X ", ((uint8_t *)amp_config_param->gsound_buf)[num_bytes_copied]);
			num_bytes_copied++;
			if (num_bytes_copied % 16 == 0) {
					printf("\n"); // 每16个字节之间添加一个空格
			}
		}
	}
	aamp_info("audio module process start finished");

	return 0;

exit:
	return ret;

}

static int do_audio_module_process_stop(AudioModuleProcessPrivateType *priv)
{
	if (priv == NULL)
		return -1;

	if (!priv->startup_count) {
		aamp_err("startup count already 0\n");
		return 0;
	}

	priv->startup_count--;

	if (priv->startup_count == 0) {
		if (priv->thread_sync)
			hal_sem_wait(priv->thread_sync);
			/* deinit algo */

	}

	return 0;

}

static int do_audio_module_process_release(AudioModuleProcessPrivateType *priv)
{
	if (priv == NULL)
		return -1;

	priv->keep_alive |= AUDIO_MODULE_PORCESS_DESTROY_BIT;

	return 0;

}

static void rpbuf_audio_module_process_destroy(AudioModuleProcessPrivateType *priv)
{

	if (priv->amp_rb) {
		hal_ringbuffer_release(priv->amp_rb);
		priv->amp_rb = NULL;
	}

	if (priv->amp_out_rb) {
		hal_ringbuffer_release(priv->amp_out_rb);
		priv->amp_out_rb = NULL;
	}

	if (priv->event) {
		hal_event_delete(priv->event);
		priv->event = NULL;
	}

	if (priv->transfer_event) {
		hal_event_delete(priv->transfer_event);
		priv->transfer_event = NULL;
	}
	
	if (priv->thread_sync) {
		hal_sem_delete(priv->thread_sync);
		priv->thread_sync = NULL;
	}

	if (priv->process_mutex) {
		hal_mutex_delete(priv->process_mutex);
		priv->process_mutex = NULL;
	}

	if (priv->transfer_mutex) {
		hal_mutex_delete(priv->transfer_mutex);
		priv->transfer_mutex = NULL;
	}

}

static int rpbuf_audio_module_process_create(AudioModuleProcessPrivateType *priv)
{
	int ret = -1;
	int frame_bytes = 0;
	audio_module_process_config_param_t *amp_config_param = &priv->module_process_ctrl_param.module_process_config_param;

	if (amp_config_param->bits == 0 || amp_config_param->in_channels == 0 ||
		amp_config_param->rate == 0 || amp_config_param->msec == 0 || amp_config_param->out_channels == 0) {
		aamp_err("invaild param, bits:%d, channels:%d, rate:%d, msec:%d, rpbuf_len:%d\n", \
			amp_config_param->bits, amp_config_param->in_channels, \
			amp_config_param->rate, amp_config_param->msec, amp_config_param->out_channels);
		goto exit;
	}

	frame_bytes = amp_config_param->bits / 8 * amp_config_param->in_channels;

	priv->rpbuf_in_len = (frame_bytes * amp_config_param->rate * amp_config_param->msec) / 1000; /* 4 * 16 * 64 = 4096 bytes */

	if (priv->module_process_ctrl_param.amp_dump)
			priv->rpbuf_out_len = (priv->rpbuf_in_len / amp_config_param->in_channels) * \
					(amp_config_param->in_channels + amp_config_param->out_channels);
	else
		priv->rpbuf_out_len = (priv->rpbuf_in_len / amp_config_param->in_channels) * (amp_config_param->out_channels); // (4096/ 2)  * 1 (1ch aec)= 2048 bytes

	if (priv->rpbuf_in_len + priv->rpbuf_out_len + sizeof(rpbuf_audio_remote_ctrl_amp_t) > RPBUF_LEN_MAX){
		aamp_err("rpbuf_size %d is larger than MAX %d\n", priv->rpbuf_in_len +priv->rpbuf_out_len + sizeof(rpbuf_audio_remote_ctrl_amp_t), RPBUF_LEN_MAX);
		goto exit;
	}

	/* this rb is using for audio process */
	priv->amp_rb = hal_ringbuffer_init(priv->rpbuf_in_len * 4);
	if (!priv->amp_rb) {
		aamp_err("create ringbuffer err");
		goto exit;
	}

	priv->amp_out_rb = hal_ringbuffer_init(priv->rpbuf_out_len * 2);
	if (!priv->amp_out_rb) {
		aamp_err("create ringbuffer err");
		goto exit;
	}

	priv->event = hal_event_create();
	if (!priv->event) {
		aamp_err("create event err");
		goto exit;
	}

	priv->transfer_event = hal_event_create();
	if (!priv->transfer_event) {
		aamp_err("create transfer event err");
		goto exit;
	}

	priv->thread_sync = hal_sem_create(0);
	if (!priv->thread_sync) {
		aamp_err("hal_sem_create failed!\n");
		goto exit;
	}

	priv->process_mutex = hal_mutex_create();
	if (!priv->process_mutex) {
		aamp_err("process hal_mutex_create failed!\n");
		goto exit;
	}

	priv->transfer_mutex = hal_mutex_create();
	if (!priv->transfer_mutex) {
		aamp_err("transfer hal_mutex_create failed!\n");
		goto exit;
	}

	return 0;
exit:

	rpbuf_audio_module_process_destroy(priv);

	return ret;

}

static void rpbuf_audio_module_process(void *arg)
{
	AudioModuleProcessPrivateType *priv = (AudioModuleProcessPrivateType *)arg;
	int ret;
	audio_module_process_queue_item item;

	while (1) {

		if (priv->keep_alive & AUDIO_MODULE_PORCESS_DESTROY_BIT)
			goto exit;

		ret = hal_queue_recv(priv->amp_queue, &item, HAL_WAIT_FOREVER);
		if (ret != 0) {
			aamp_err("Failed to recv queue\n");
			goto exit;
		}

		aamp_info("receive cmd:%d\n", item.cmd);

		switch (item.cmd) {
		case RPBUF_CTL_CONFIG:
			if (item.arg == NULL || item.len != sizeof(audio_module_process_config_param_t)) {
				aamp_err("audio rpbuf ctrl config param is invaild %d \n", item.len);
				break;
			}
			memcpy(&priv->module_process_ctrl_param.module_process_config_param, \
				(audio_module_process_config_param_t *)item.arg, sizeof(audio_module_process_config_param_t));
			if (priv->init_flag == 0) {
				ret = rpbuf_audio_module_process_create(priv);
				if (ret != 0) {
					aamp_err("rpbuf_arecord_create failed \n");
				}
				priv->init_flag = 1;
			} else {
				ret = do_audio_module_process_config(priv);
				if (ret != 0) {
					aamp_err("do_audio_module_process_config failed \n");
				}
			}
			break;
		case RPBUF_CTL_START:
			ret = do_audio_module_process_start(priv);
			if (ret != 0) {
				aamp_err("do_audio_module_process_start failed \n");
			}
			break;
		case RPBUF_CTL_STOP:
			ret = do_audio_module_process_stop(priv);
			if (ret != 0) {
				aamp_err("do_audio_module_process_stop failed \n");
			}
			break;
		case RPBUF_CTL_RELEASE:
			if (priv->init_flag) {
				ret = do_audio_module_process_release(priv);
				if (ret != 0) {
					aamp_err("do_audio_module_process_release failed \n");
				}
				priv->init_flag = 0;
			} else {
				aamp_err("create record first\n");
			}
			break;
		default:
			aamp_err("unknown cmd:%d\n", item.cmd);
			break;
		}

	}

exit:

	rpbuf_audio_module_process_destroy(priv);

	hal_sem_post(priv->thread_release);

	hal_thread_stop(priv->amp_task);

}

hal_queue_t audio_module_process_get_queue_handle(void *private)
{
	AudioModuleProcessPrivateType* priv = (AudioModuleProcessPrivateType*)private;

	if (priv == NULL) {
		aamp_err("task create failed");
		return NULL;
	}

	return priv->amp_queue;

}

void rpbuf_audio_module_transfer_destroy(AudioModuleProcessPrivateType *priv)
{
	int ret;

	ret = rpbuf_common_destroy(priv->rpbuf_transfer_in_name[priv->index]);
	if (ret < 0) {
		aamp_err("rpbuf_destroy for name %s (len: %d) failed\n", priv->rpbuf_transfer_in_name[priv->index], RPBUF_AMP_IN_LENGTH);
	}

	ret = rpbuf_common_destroy(priv->rpbuf_transfer_out_name[priv->index]);
	if (ret < 0) {
		aamp_err("rpbuf_destroy for name %s (len: %d) failed\n", priv->rpbuf_transfer_out_name[priv->index], RPBUF_AMP_OUT_LENGTH);
	}

}

int rpbuf_audio_module_transfer_create(void *private)
{
	AudioModuleProcessPrivateType* priv = (AudioModuleProcessPrivateType*)private;
	int ret;
	int index = priv->index;

	snprintf(priv->rpbuf_transfer_in_name[index], sizeof(priv->rpbuf_transfer_in_name[index]), "%s_%d", RPBUF_AMP_IN_NAME, index);

	ret = rpbuf_common_create(RPBUF_CTRL_ID, priv->rpbuf_transfer_in_name[index], RPBUF_AMP_IN_LENGTH, 1, &rpbuf_amp_in_cbs, priv);
	if (ret < 0) {
		 aamp_err("rpbuf_create for name %s (len: %d) failed\n", priv->rpbuf_transfer_in_name[index], RPBUF_AMP_IN_LENGTH);
		 goto exit;
	}
	while(!rpbuf_common_is_available(priv->rpbuf_transfer_in_name[index])) {
		hal_msleep(5);
	}
	aamp_info("rpbuf %s ready\n", priv->rpbuf_transfer_in_name[index]);

	snprintf(priv->rpbuf_transfer_out_name[index], sizeof(priv->rpbuf_transfer_out_name[index]), "%s_%d", RPBUF_AMP_OUT_NAME, index);

	ret = rpbuf_common_create(RPBUF_CTRL_ID, priv->rpbuf_transfer_out_name[index], RPBUF_AMP_OUT_LENGTH, 1, &rpbuf_amp_out_cbs, priv);
	if (ret < 0) {
		 aamp_err("rpbuf_create for name %s (len: %d) failed\n", priv->rpbuf_transfer_out_name[index], RPBUF_AMP_OUT_LENGTH);
		 goto exit;
	}

	while (!rpbuf_common_is_available(priv->rpbuf_transfer_out_name[index])) {
		hal_msleep(5);
	}
	aamp_info("rpbuf %s ready\n", priv->rpbuf_transfer_out_name[index]);

	return 0;

exit:

	rpbuf_audio_module_transfer_destroy(priv);

	return ret;
}

void audio_module_process_ctrl_destroy(void *private)
{
	AudioModuleProcessPrivateType* priv = NULL;

	if (private == NULL)
		return;

	priv = (AudioModuleProcessPrivateType*)private;

	hal_sem_wait(priv->thread_release);

	rpbuf_audio_module_transfer_destroy(priv);

	if (priv->amp_queue) {
		hal_queue_delete(priv->amp_queue);
		priv->amp_queue = NULL;
	}

	if (priv->thread_release) {
		hal_sem_delete(priv->thread_release);
		priv->thread_release = NULL;
	}

	if (priv) {
		hal_free(priv);
		priv = NULL;
	}
}

void* audio_module_process_ctrl_init(int index)
{
	AudioModuleProcessPrivateType* priv = NULL;
	char name[32];

	/* Initialize application private data */
	priv = hal_malloc(sizeof(AudioModuleProcessPrivateType));
	if (!priv) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}
	memset(priv, 0, sizeof(AudioModuleProcessPrivateType));

	priv->amp_queue = hal_queue_create("rpbuf_audio_module_process", sizeof(audio_module_process_queue_item), AUDIO_MODULE_PORCESS_QUEUE);
	if (priv->amp_queue == NULL) {
		aamp_err("Failed to allocate memory for queue\n");
		goto exit;
	}

	priv->thread_release = hal_sem_create(0);
	if (!priv->thread_release) {
		aamp_err("hal_sem_create failed!\n");
		goto exit;
	}

	priv->module_process_ctrl_param.amp_dump = g_amp_dump_merge[index];

	snprintf(name, sizeof(name), "rpbuf_amp_process_%d", index);

	priv->amp_task = hal_thread_create(rpbuf_audio_module_process, priv, name,
					2048, configAPPLICATION_AAMP_PRIORITY);
	if (priv->amp_task == NULL) {
		aamp_err("Failed to create arecord_process rx_task\r\n");
		goto exit;
	}
	hal_thread_start(priv->amp_task);

	priv->index = index;

	return priv;
exit:

	if (priv->amp_queue) {
		hal_queue_delete(priv->amp_queue);
		priv->amp_queue = NULL;
	}

	if (priv->thread_release) {
		hal_sem_delete(priv->thread_release);
		priv->thread_release = NULL;
	}

	if (priv) {
		hal_free(priv);
		priv = NULL;
	}
	return NULL;
}

