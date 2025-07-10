/**
  utils/rpdata_asr_process.h

  This simple test asr application take one input stream. Stores the output in another
  output file.

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


#ifndef __RPDATA_ASR_PROCESS_H__
#define __RPDATA_ASR_PROCESS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <hal_queue.h>
#include <hal_mem.h>
#include <hal_thread.h>
#include <hal_mutex.h>
#include <hal_sem.h>
#include <hal_event.h>

#include "rpdata_audio_interface.h"
#include "rpdata_audio_ctrl_asr.h"

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

#define AUDIO_OUTBUF_DEFAULT_SIZE 1920 //16kHz/3ch/16bit-20ms
#define DEFAULT_ASR_BUF_CNT		4
#define FRONT_ASR_RATE		16000
#define FRONT_ASR_CHANNELS		3
#define FRONT_ASR_OUT_CHANNELS		1
#define FRONT_ASR_BIT_WIDTH		16
#define OMX_ASR_PORT_NUMBER_SUPPORTED 2
//#define ASR_OUT_OFFSET 12


typedef struct {
	uint32_t cmd;
	void *arg;
	int32_t len;
}asr_queue_item;

typedef struct  front_asr_record_param {
	uint32_t rate;
	uint32_t msec;
	uint8_t channels;
	uint8_t afe_out_channels;
	uint8_t bits;
}front_asr_record_param;

typedef struct  front_asr_param {
	front_asr_record_param asr_record_param;
	speech_status_t speech_status;
	bool asr_enable;
	bool asr_dump;
	bool asr;
	bool vad;
}front_asr_param;

typedef enum {
	ASR_CTL_NONE 	= 0,
	ASR_CTL_INIT,
	ASR_CTL_START,             // start algo
	ASR_CTL_PAUSE,             // pause algo
	ASR_CTL_RESUME,            // resume algo
	ASR_CTL_STOP,              // stop algo
	ASR_CTL_RELEASE,           // release algo
	ASR_CTL_NUM,
} ASR_CTRL_TYPE;

#ifdef CONFIG_COMPONENTS_OMX_SYSTEM
/* Application's private data */
typedef struct FrontAsrPrivateType {
	OMX_S8 port_asr_in;
	OMX_S8 port_asr_out;
	OMX_U8 startup_count;
	OMX_U8 keep_alive;
	OMX_U8 config_flag;
	OMX_U16 index;
	OMX_S16 asr_ref;
	OMX_U16 state;
	rpdata_audio_param_t rpdata_audio_targ[RPDATA_CRTL_ASR_COUNTS];
	struct rpdata_audio_entry *front_asr_data[RPDATA_CRTL_ASR_COUNTS];
	front_asr_param *asr_param;
	void *asr_task;   /* asr message task handle */
	OMX_S32 buf_num;
	OMX_BUFFERHEADERTYPE **bufferin;
	OMX_BUFFERHEADERTYPE **bufferout;
	OMX_PARAM_PORTDEFINITIONTYPE portout_def;
	OMX_PARAM_PORTDEFINITIONTYPE arecord_port_para;
	OMX_PARAM_PORTDEFINITIONTYPE asr_port_para[OMX_ASR_PORT_NUMBER_SUPPORTED];
	omx_sem_t* arecord_eventSem;
	omx_sem_t* asr_eventSem;
	omx_sem_t* eofSem;
	hal_sem_t thread_release;
	hal_mutex_t ref_mutex;
	OMX_HANDLETYPE arecord_handle;
	OMX_HANDLETYPE asr_handle;
	hal_queue_t  asr_queue;
} FrontAsrPrivateType;

/* Callback prototypes */
OMX_ERRORTYPE frontaudioasrEventHandler(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 Data1,
  OMX_U32 Data2,
  OMX_PTR pEventData);

OMX_ERRORTYPE frontasrarecordEventHandler(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 Data1,
  OMX_U32 Data2,
  OMX_PTR pEventData);

OMX_ERRORTYPE frontaudioasrEmptyBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE frontaudioasrFillBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer);

OMX_ERRORTYPE frontasrFillBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer);

#endif


#if (defined CONFIG_COMPONENTS_AW_AUDIO_RPDATA_OMX_ASR)

void rpdata_asr_init_default_param(void *private);

void rpdata_asr_deinit_param(void *private);

int do_rpdata_enable_front_asr_dump_merge(void *private, int index ,int val);

int do_rpdata_enable_whole_front_asr(void *private, int index, int val);

int do_rpdata_enable_front_asr(void *private, int index, int val);

int do_rpdata_enable_front_vad(void *private, int index, int val);

void asr_ctrl_destroy(void *private);

void* asr_ctrl_init(int index);

hal_queue_t asr_get_queue_handle(void *private);

#else

void rpdata_asr_init_default_param(void *priv)
{
}

void rpdata_asr_deinit_param(void *private)
{
}


int do_rpdata_enable_front_asr_dump_merge(void *private, int index ,int val)
{
	return 0;
}


int do_rpdata_enable_whole_front_asr(void *priv, int index, int val)
{
	return 0;
}

int do_rpdata_enable_front_asr(void *private, int index ,int val)
{
	return 0;
}


int do_rpdata_enable_front_vad(void *priv, int index, int val)
{
	return 0;
}

void asr_ctrl_destroy(void *priv)
{
}

void* asr_ctrl_init(int index)
{
	return NULL;
}

hal_queue_t asr_get_queue_handle(void *private)
{
	hal_queue_t asr_queue = NULL;
	return asr_queue;
}

#endif

#endif
