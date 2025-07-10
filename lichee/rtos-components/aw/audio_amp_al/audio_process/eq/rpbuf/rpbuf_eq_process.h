/**
  audio_process/rpbuf_eq_process.h

  This simple test application take one input stream. Put the input to eq.

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


#ifndef __RPBUF_EQ_PROCESS_H__
#define __RPBUF_EQ_PROCESS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ringbuffer.h>
#include <hal_queue.h>
#include "rpbuf_audio_ctrl_eq.h"

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

#define RPBUF_EQ_DEFAULT_RATE			16000
#define RPBUF_EQ_DEFAULT_CHANNELS		6
#define RPBUF_EQ_DEFAULT_OUT_CHANNELS	6
#define RPBUF_EQ_DEFAULT_BIT_WIDTH		16
#define RPBUF_EQ_DEFAULT_MSEC			16

#define DEFAULT_AEQUALIZER_BUF_CNT		4

#define OMX_PORT_NUMBER_SUPPORTED 2

#define AEQ_CAP_EV_DATA_GET (1 << 0)

typedef struct {
	uint32_t cmd;
	void *arg;
	int32_t len;
}eq_queue_item;

typedef struct eq_config_param {
	uint32_t rate;
	uint32_t msec;
	uint32_t channels;           /* record chans */
	uint32_t eq_out_channels;    /* output chans */
	uint32_t bits;
	void *eqPrmsTransfer;
}eq_config_param_t;

typedef struct  eq_ctrl_param {
	eq_config_param_t eq_config_param;
	bool 	eq_dump;
	uint32_t eq_enable;
}eq_ctrl_param_t;

extern int rpbuf_audio_ctrl_eq_send_cmd(void *handle, rpbuf_audio_remote_ctrl_eq_t *rpbuf_ctl_arg, unsigned long data_offset);

#ifdef CONFIG_COMPONENTS_OMX_SYSTEM
/* Application's private data */
typedef struct EqPrivateType{
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
	OMX_S16 eq_ref;
	OMX_U16 state;
	char rpbuf_transfer_in_name[RPBUF_CRTL_EQ_COUNTS][32];
	char rpbuf_transfer_out_name[RPBUF_CRTL_EQ_COUNTS][32];
	eq_ctrl_param_t eq_ctrl_param;
	void *eq_task;           /* eq message task handle */
	void *eq_process_task;   /* eq process task handle */
	void *eq_send_task;      /* eq send task handle */
	OMX_BUFFERHEADERTYPE **bufferin;
	OMX_BUFFERHEADERTYPE **bufferout;
	OMX_PARAM_PORTDEFINITIONTYPE portin_def;
	OMX_PARAM_PORTDEFINITIONTYPE portout_def;
	OMX_PARAM_PORTDEFINITIONTYPE aeq_port_para[OMX_PORT_NUMBER_SUPPORTED];
	hal_ringbuffer_t aeq_rb;
	hal_ringbuffer_t aeq_out_rb;
	hal_event_t event;
	hal_event_t send_event;
	hal_sem_t thread_sync;
	hal_sem_t thread_release;
	hal_mutex_t ref_mutex;
	omx_sem_t* aeq_eventSem;
	omx_sem_t* eofSem;
	omx_sem_t* pauseSem;
	queue_t* BufferQueue;
	OMX_HANDLETYPE aeq_handle;
	hal_queue_t  eq_queue;
	void *eq_ctrl_handle;
}EqPrivateType;

typedef enum {
	EQ_CTL_NONE 	= 0,
	EQ_CTL_INIT,
	EQ_CTL_START,             // start algo
	EQ_CTL_PAUSE,             // pause algo
	EQ_CTL_RESUME,            // resume algo
	EQ_CTL_STOP,              // stop algo
	EQ_CTL_RELEASE,           // release algo
	EQ_CTL_NUM,
}EQ_CTRL_TYPE;

typedef struct EqControlType{
	int state;
	void *private;
}EqControlType;

/* Callback prototypes */
OMX_ERRORTYPE RpbufAudioEqualizerEventHandler(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 Data1,
  OMX_U32 Data2,
  OMX_PTR pEventData);

OMX_ERRORTYPE RpbufAudioEqualizerEmptyBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE RpbufAudioEqualizerFillBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer);

#endif

#if (defined CONFIG_COMPONENTS_AW_AUDIO_RPBUF_OMX_EQ)

void rpbuf_eq_init_default_param(void *priv);

void rpbuf_eq_deinit_param(void *private);

int do_rpbuf_enable_eq_dump_merge(void *priv, int index, int val);

int do_rpbuf_enable_eq(void *private, int index, int val);

void eq_ctrl_destroy(void *priv);

void* eq_ctrl_init(int index, void *audio_ctrl_eq);

int rpbuf_eq_transfer_create(void *private);

hal_queue_t eq_get_queue_handle(void *private);

#else

void rpbuf_eq_init_default_param(void *priv)
{
}

void rpbuf_eq_deinit_param(void *private)
{
}


int do_rpbuf_enable_eq_dump_merge(void *priv, int index, int val)
{
	return 0;
}


int do_rpbuf_enable_eq(void *private, int index, int val)
{
	return 0;
}


void eq_ctrl_destroy(void *priv)
{
}

void* eq_ctrl_init(int index, void *audio_ctrl_eq)
{
	return NULL;
}

int rpbuf_eq_transfer_create(void *private)
{
	return 0;
}

hal_queue_t eq_get_queue_handle(void *private)
{
	hal_queue_t eq_queue = NULL;
	return eq_queue;
}

#endif


#endif
