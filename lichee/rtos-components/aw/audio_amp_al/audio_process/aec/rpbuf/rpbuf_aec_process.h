/**
  audio_process/rpbuf_aec_process.h

  This simple test application take one input stream. Put the input to aec.

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


#ifndef __RPBUF_AEC_PROCESS_H__
#define __RPBUF_AEC_PROCESS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ringbuffer.h>
#include <hal_queue.h>

#include "rpbuf_audio_ctrl_aec.h"

#ifdef CONFIG_COMPONENTS_OMX_SYSTEM
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_Types.h>
#include <OMX_Audio.h>
#include "OMX_Base.h"
#endif

/** Specification version*/
#define VERSIONMAJOR    1
#define VERSIONMINOR    1
#define VERSIONREVISION 0
#define VERSIONSTEP     0

#define RPBUF_AEC_DEFAULT_RATE			16000
#define RPBUF_AEC_DEFAULT_CHANNELS		2
#define RPBUF_AEC_DEFAULT_OUT_CHANNELS	1
#define RPBUF_AEC_DEFAULT_BIT_WIDTH		16
#define RPBUF_AEC_DEFAULT_MSEC			64
#define RPBUF_AEC_DEFAULT_LMS_MSEC		0


#define DEFAULT_AECHO_CANCEL_BUF_CNT	6

#define AEC_OMX_PORT_NUMBER_SUPPORTED 2

typedef struct {
	uint32_t cmd;
	void *arg;
	int32_t len;
}aec_queue_item;

typedef struct  aec_config_param {
	uint32_t rate;
	uint32_t msec;
	uint32_t lms_msec;
	uint32_t channels;           /* record chans */
	uint32_t aec_out_channels;   /* aec output chans */
	uint32_t bits;
	void *dspCfgCore;
}aec_config_param_t;

typedef struct  aec_ctrl_param {
	aec_config_param_t aec_config_param;
	bool 	aec_dump;
	uint32_t aec_enable;
}aec_ctrl_param_t;

extern int rpbuf_audio_ctrl_aec_send_cmd(void *handle, rpbuf_audio_remote_ctrl_aec_t *rpbuf_ctl_arg, unsigned long data_offset);

#ifdef CONFIG_COMPONENTS_OMX_SYSTEM
/* Application's private data */
typedef struct AecPrivateType{
	int port_filter_in;
	int port_filter_out;
	OMX_U8 startup_count;
	OMX_U8 process_task_exit_flag;
	OMX_U8 send_task_exit_flag;
	OMX_U8 paused;
	OMX_U8 keep_alive;
	OMX_U8 config_flag;
	OMX_U32 rpbuf_in_len;
	OMX_U32 rpbuf_out_len;
	OMX_U16 index;
	OMX_S16 aec_ref;
	OMX_U16 state;
	char rpbuf_transfer_in_name[RPBUF_CRTL_AEC_COUNTS][32];
	char rpbuf_transfer_out_name[RPBUF_CRTL_AEC_COUNTS][32];
	aec_ctrl_param_t aec_ctrl_param;
	void *aec_task;           /* aec message task handle */
	void *aec_process_task;   /* aec process task handle */
	void *aec_send_task;      /* aec send task handle */
	OMX_BUFFERHEADERTYPE **bufferin;
	OMX_BUFFERHEADERTYPE **bufferout;
	OMX_PARAM_PORTDEFINITIONTYPE portin_def;
	OMX_PARAM_PORTDEFINITIONTYPE portout_def;
	OMX_PARAM_PORTDEFINITIONTYPE aec_port_para[AEC_OMX_PORT_NUMBER_SUPPORTED];
	hal_ringbuffer_t aec_rb;
	hal_ringbuffer_t aec_out_rb;
	hal_event_t event;
	hal_event_t send_event;
	hal_sem_t thread_sync;
	hal_sem_t thread_release;
	hal_mutex_t ref_mutex;
	omx_sem_t* aec_eventSem;
	omx_sem_t* eofSem;
	omx_sem_t* pauseSem;
	queue_t* BufferQueue;
	OMX_HANDLETYPE aec_handle;
	hal_queue_t  aec_queue;
	void *aec_ctrl_handle;
}AecPrivateType;

typedef enum {
	AEC_CTL_NONE 	= 0,
	AEC_CTL_INIT,
	AEC_CTL_START,             // start algo
	AEC_CTL_PAUSE,             // pause algo
	AEC_CTL_RESUME,            // resume algo
	AEC_CTL_STOP,              // stop algo
	AEC_CTL_RELEASE,           // release algo
	AEC_CTL_NUM,
}AEC_CTRL_TYPE;

typedef struct AecControlType{
	int state;
	void *private;
}AecControlType;

/* Callback prototypes */
OMX_ERRORTYPE RpbufAudioEchoCancelEventHandler(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 Data1,
  OMX_U32 Data2,
  OMX_PTR pEventData);

OMX_ERRORTYPE RpbufAudioEchoCancelEmptyBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE RpbufAudioEchoCancelFillBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer);
#endif

#if (defined CONFIG_COMPONENTS_AW_AUDIO_RPBUF_OMX_AEC)

void rpbuf_aec_init_default_param(void *priv);

void rpbuf_aec_deinit_param(void *private);

int do_rpbuf_enable_aec_dump_merge(void *private, int index ,int val);

int do_rpbuf_enable_aec(void *private, int index, int val);

void aec_ctrl_destroy(void *priv);

void* aec_ctrl_init(int index, void *audio_ctrl_aec);

hal_queue_t aec_get_queue_handle(void *private);

int rpbuf_aec_transfer_create(void *priv);

#else

void rpbuf_aec_init_default_param(void *priv)
{
}

void rpbuf_aec_deinit_param(void *private)
{
}


int do_rpbuf_enable_aec_dump_merge(void *private, int index ,int val)
{
	return 0;
}


int do_rpbuf_enable_aec(void *priv, int index, int val)
{
	return 0;
}


void aec_ctrl_destroy(void *priv)
{
}

void* aec_ctrl_init(int index, void *audio_ctrl_aec)
{
	return NULL;
}

int rpbuf_aec_transfer_create(void *private)
{
	return 0;
}

hal_queue_t aec_get_queue_handle(void *private)
{
	hal_queue_t aec_queue = NULL;
	return aec_queue;
}

#endif


#endif
