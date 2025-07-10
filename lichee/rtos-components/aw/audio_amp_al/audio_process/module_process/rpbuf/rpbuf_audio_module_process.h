/**
  audio_process/rpbuf_audio_module_process.h

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


#ifndef __RPBUF_AUDIO_MODULE_PORCESS_H__
#define __RPBUF_AUDIO_MODULE_PORCESS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ringbuffer.h>
#include <hal_queue.h>
#include <hal_sem.h>
#include <hal_mutex.h>
#include "rpbuf_audio_ctrl_module_process.h"

#define RPBUF_AUDIO_MODULE_PORCESS_DEFAULT_RATE       16000
#define RPBUF_AUDIO_MODULE_PORCESS_DEFAULT_CHANNELS    2
#define RPBUF_AUDIO_MODULE_PORCESS_DEFAULT_BIT_WIDTH   16
#define RPBUF_AUDIO_MODULE_PORCESS_DEFAULT_MSEC        20
#define RPBUF_AUDIO_MODULE_PORCESS_DEFAULT_OUT_CHANNELS    2

#define AUDIO_MODULE_PORCESS_DESTROY_BIT		(0x1<<0)

#define AUDIO_MODULE_PORCESS_QUEUE				(4)

typedef struct {
	uint32_t 	cmd;
	void 		*arg;
	int32_t 	len;
}audio_module_process_queue_item;

typedef struct  audio_module_process_config_param {
	uint32_t rate;
	uint32_t msec;
	uint32_t in_channels;
	uint32_t out_channels;   /* amp output chans */
	uint32_t bits;
	void *gsound_buf;
	uint32_t gsound_len;
	uint32_t smooth_time;
}audio_module_process_config_param_t;

typedef struct  audio_module_process_ctrl_param {
	audio_module_process_config_param_t module_process_config_param;
	bool 	amp_dump;
}audio_module_process_ctrl_param_t;


/* Application's private data */
typedef struct AudioModuleProcessPrivateType{
	uint8_t startup_count;
	uint8_t keep_alive;
	uint8_t init_flag;
	uint8_t process_task_exit_flag;
	uint8_t transfer_task_exit_flag;
	uint32_t index;
	char rpbuf_transfer_in_name[RPBUF_CRTL_AMP_COUNTS][32];
	char rpbuf_transfer_out_name[RPBUF_CRTL_AMP_COUNTS][32];
	uint32_t rpbuf_in_len;
	uint32_t rpbuf_out_len;
	hal_queue_t  amp_queue;
	hal_sem_t    thread_sync;
	hal_sem_t    thread_release;
	hal_event_t event;
	hal_event_t transfer_event;
	hal_mutex_t process_mutex;
	hal_mutex_t transfer_mutex;
	hal_ringbuffer_t amp_rb;
	hal_ringbuffer_t amp_out_rb;
	void *amp_task;   /* module processmessage task handle */
	void *amp_process_task;  /* amp process data task handle */
	void *amp_transfer_task;  /* amp transfer data task handle */
	audio_module_process_ctrl_param_t module_process_ctrl_param;
} AudioModuleProcessPrivateType;

#if (defined CONFIG_COMPONENTS_AW_AUDIO_RPBUF_MODULE_PROCESS)

void rpbuf_audio_module_process_deinit_param(void *private);

void rpbuf_audio_module_process_default_param(void *priv);

void audio_module_process_ctrl_destroy(void *priv);

void* audio_module_process_ctrl_init(int index);

int rpbuf_audio_module_transfer_create(void *private);

hal_queue_t audio_module_process_get_queue_handle(void *private);

int do_rpbuf_enable_amp_dump_merge(void *private, int index, int val);

#else

void rpbuf_audio_module_process_deinit_param(void *private)
{
}

void rpbuf_audio_module_process_default_param(void *priv)
{
}

void audio_module_process_ctrl_destroy(void *priv)
{
}

void* audio_module_process_ctrl_init(int index)
{
	return NULL;
}

int rpbuf_audio_module_transfer_create(void *private)
{
	return 0;
}

hal_queue_t audio_module_process_get_queue_handle(void *private)
{
	hal_queue_t amp_queue = NULL;
	return amp_queue;
}

int do_rpbuf_enable_amp_dump_merge(void *private, int index, int val)
{
	return 0;
}

#endif


#endif
