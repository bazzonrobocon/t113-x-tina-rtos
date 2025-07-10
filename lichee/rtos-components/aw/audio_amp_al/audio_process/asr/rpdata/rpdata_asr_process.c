/**
  utils/rpdata_asr_process.c

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <FreeRTOS.h>
#include <hal_thread.h>
#include <hal_time.h>
#include <hal_event.h>
#include <hal_status.h>

#include "rpdata_asr_process.h"

#include "audio_amp_log.h"

#define FRONT_ASR_QUEUE				(8)

#define FRONT_ASR_DESTROY_BIT		(0x1<<0)

/* config asr param */
static int g_front_vad_enable[RPDATA_CRTL_ASR_COUNTS] = {0};
static int g_front_asr_enable[RPDATA_CRTL_ASR_COUNTS] = {0};
static int g_front_asr_whole_enable[RPDATA_CRTL_ASR_COUNTS] = {0};
static int g_front_asr_dump_merge[RPDATA_CRTL_ASR_COUNTS] = {0};

static OMX_U32 error_value = 0u;

OMX_CALLBACKTYPE front_asr_tunnel_callbacks = {
	.EventHandler = frontaudioasrEventHandler,
	.EmptyBufferDone = frontaudioasrEmptyBufferDone,
	.FillBufferDone = frontaudioasrFillBufferDone,
};

OMX_CALLBACKTYPE front_asr_arecord_tunnel_callbacks = {
	.EventHandler = frontasrarecordEventHandler,
	.EmptyBufferDone = NULL,
	.FillBufferDone = NULL,
};

OMX_CALLBACKTYPE front_asr_untunnel_callbacks = {
	.EventHandler = frontaudioasrEventHandler,
	.EmptyBufferDone = frontaudioasrEmptyBufferDone,
	.FillBufferDone = frontasrFillBufferDone,
};


void rpdata_asr_init_default_param(void *private)
{
	FrontAsrPrivateType* priv = (FrontAsrPrivateType*)private;

	priv->asr_param->asr_record_param.msec = 10;
	priv->asr_param->asr_record_param.rate = FRONT_ASR_RATE;
	priv->asr_param->asr_record_param.channels = FRONT_ASR_CHANNELS;
	priv->asr_param->asr_record_param.afe_out_channels = FRONT_ASR_OUT_CHANNELS;
	priv->asr_param->asr_record_param.bits = FRONT_ASR_BIT_WIDTH;
}

void rpdata_asr_deinit_param(void *private)
{
	FrontAsrPrivateType* priv = (FrontAsrPrivateType*)private;

	g_front_asr_dump_merge[priv->index] = 0;
	g_front_vad_enable[priv->index] = 0;
	g_front_asr_enable[priv->index] = 0;
	g_front_asr_whole_enable[priv->index] = 0;
}

static void setHeader(OMX_PTR header, OMX_U32 size) {
  OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
  *((OMX_U32*)header) = size;

  ver->s.nVersionMajor = VERSIONMAJOR;
  ver->s.nVersionMinor = VERSIONMINOR;
  ver->s.nRevision = VERSIONREVISION;
  ver->s.nStep = VERSIONSTEP;
}

/* Callbacks implementation */
OMX_ERRORTYPE frontasrarecordEventHandler(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 Data1,
  OMX_U32 Data2,
  OMX_PTR pEventData) {

	char *name;

	FrontAsrPrivateType *private = (FrontAsrPrivateType *)pAppData;
	if (hComponent == private->arecord_handle)
		name = "asraudiorecord";

	hal_mutex_lock(private->ref_mutex);
	private->asr_ref++;
	hal_mutex_unlock(private->ref_mutex);

	aamp_debug("In the %s callback\n", __func__);
	if(eEvent == OMX_EventCmdComplete) {
		if (Data1 == OMX_CommandStateSet) {
			switch ((int)Data2) {
				case OMX_StateInvalid:
					aamp_info("%s StateSet OMX_StateInvalid\n", name);
					break;
				case OMX_StateLoaded:
					aamp_info("%s StateSet OMX_StateLoaded\n", name);
					break;
				case OMX_StateIdle:
					aamp_info("%s StateSet OMX_StateIdle\n", name);
					break;
				case OMX_StateExecuting:
					aamp_info("%s StateSet OMX_StateExecuting\n", name);
					break;
				case OMX_StatePause:
					aamp_info("%s StateSet OMX_StatePause\n", name);
					break;
				case OMX_StateWaitForResources:
					aamp_info("%s StateSet WaitForResources\n", name);
					break;
				default:
					aamp_err("%s StateSet unkown state\n", name);
				break;
			}
			omx_sem_up(private->arecord_eventSem);
		} else  if (Data1 == OMX_CommandPortEnable){
			aamp_info("%s CmdComplete OMX_CommandPortEnable\n", name);
			omx_sem_up(private->arecord_eventSem);
		} else if (Data1 == OMX_CommandPortDisable) {
			aamp_info("%s CmdComplete OMX_CommandPortDisable\n", name);
			omx_sem_up(private->arecord_eventSem);
		}
	} else if(eEvent == OMX_EventBufferFlag) {
		if ((int)Data2 == OMX_BUFFERFLAG_EOS) {
			aamp_info("%s BufferFlag OMX_BUFFERFLAG_EOS\n", name);
			if (hComponent == private->arecord_handle) {
				aamp_info("end of tunnel");
				omx_sem_up(private->eofSem);
			}
		} else
			aamp_info("%s OMX_EventBufferFlag %lx", name, Data2);
	} else if (eEvent == OMX_EventError) {
		error_value = Data1;
		aamp_err("Receive error event. value:%lx", error_value);
		omx_sem_up(private->arecord_eventSem);
	} else {
		aamp_err("Param1 is %i\n", (int)Data1);
		aamp_err("Param2 is %i\n", (int)Data2);
	}

	hal_mutex_lock(private->ref_mutex);
	private->asr_ref--;
	hal_mutex_unlock(private->ref_mutex);

	return OMX_ErrorNone;
}

/* Callbacks implementation */
OMX_ERRORTYPE frontaudioasrEventHandler(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 Data1,
  OMX_U32 Data2,
  OMX_PTR pEventData) {

	char *name;

	FrontAsrPrivateType *private = (FrontAsrPrivateType *)pAppData;
	if (hComponent == private->asr_handle)
		name = "audioasr";

	hal_mutex_lock(private->ref_mutex);
	private->asr_ref++;
	hal_mutex_unlock(private->ref_mutex);

	aamp_debug("In the %s callback\n", __func__);
	if(eEvent == OMX_EventCmdComplete) {
		if (Data1 == OMX_CommandStateSet) {
			switch ((int)Data2) {
				case OMX_StateInvalid:
					aamp_info("%s StateSet OMX_StateInvalid\n", name);
					break;
				case OMX_StateLoaded:
					aamp_info("%s StateSet OMX_StateLoaded\n", name);
					break;
				case OMX_StateIdle:
					aamp_info("%s StateSet OMX_StateIdle\n", name);
					break;
				case OMX_StateExecuting:
					aamp_info("%s StateSet OMX_StateExecuting\n", name);
					break;
				case OMX_StatePause:
					aamp_info("%s StateSet OMX_StatePause\n", name);
					break;
				case OMX_StateWaitForResources:
					aamp_info("%s StateSet WaitForResources\n", name);
					break;
				default:
					aamp_err("%s StateSet unkown state\n", name);
				break;
			}
			omx_sem_up(private->asr_eventSem);
		} else  if (Data1 == OMX_CommandPortEnable){
			aamp_info("%s CmdComplete OMX_CommandPortEnable\n", name);
			omx_sem_up(private->asr_eventSem);
		} else if (Data1 == OMX_CommandPortDisable) {
			aamp_info("%s CmdComplete OMX_CommandPortDisable\n", name);
			omx_sem_up(private->asr_eventSem);
		}
	} else if(eEvent == OMX_EventBufferFlag) {
		if ((int)Data2 == OMX_BUFFERFLAG_EOS) {
			aamp_info("%s BufferFlag OMX_BUFFERFLAG_EOS\n", name);
			if (hComponent == private->asr_handle) {
				aamp_info("end of tunnel");
				omx_sem_up(private->eofSem);
			}
		} else
			aamp_err("%s OMX_EventBufferFlag %lx", name, Data2);
	} else if (eEvent == OMX_EventError) {
		error_value = Data1;
		aamp_err("Receive error event. value:%lx", error_value);
		omx_sem_up(private->asr_eventSem);
	} else {
		aamp_info("Param1 is %i\n", (int)Data1);
		aamp_info("Param2 is %i\n", (int)Data2);
	}

	hal_mutex_lock(private->ref_mutex);
	private->asr_ref--;
	hal_mutex_unlock(private->ref_mutex);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE frontaudioasrEmptyBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer) {

  aamp_debug("In the %s callback from the port %i\n", __func__, (int)pBuffer->nInputPortIndex);

  return OMX_ErrorNone;
}

OMX_ERRORTYPE frontaudioasrFillBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer) {

	OMX_ERRORTYPE err;
	int ret;
	FrontAsrPrivateType *private = (FrontAsrPrivateType *)pAppData;
	int index = private->index;
/*
	aamp_err("In the %s callback. Got buflen %i  offset %i for buffer at 0x%p\n",
                          __func__, (int)pBuffer->nFilledLen, (int)pBuffer->nOffset, pBuffer);
*/
	if (pBuffer == NULL || pAppData == NULL) {
	  aamp_err("err: buffer header is null");
	  return OMX_ErrorBadParameter;
	}

	if (pBuffer->nFilledLen == 0) {
		aamp_err("Ouch! In %s: no data in the output buffer!\n", __func__);
		return OMX_ErrorNone;
	}

	hal_mutex_lock(private->ref_mutex);
	private->asr_ref++;
	hal_mutex_unlock(private->ref_mutex);

	if(pBuffer->nFilledLen > 0) {

		ret = rpdata_audio_send(private->front_asr_data[index], pBuffer->pBuffer, pBuffer->nFilledLen);
		if (ret != 0) {
			aamp_err("[%s] line:%d \n", __func__, __LINE__);
		}

	}

	/* Output data to standard output */
	pBuffer->nFilledLen = 0;
	err = OMX_FillThisBuffer(private->asr_handle, pBuffer);
	if (err != OMX_ErrorNone)
		aamp_err("OMX_FillThisBuffer err: %x", err);

	hal_mutex_lock(private->ref_mutex);
	private->asr_ref--;
	hal_mutex_unlock(private->ref_mutex);

  return err;
}


OMX_ERRORTYPE frontasrFillBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer) {

	OMX_ERRORTYPE err;
	FrontAsrPrivateType *private = (FrontAsrPrivateType *)pAppData;

	aamp_debug("In the %s callback. Got buflen %i for buffer at 0x%p\n",
                          __func__, (int)pBuffer->nFilledLen, pBuffer);

	if (pBuffer == NULL || pAppData == NULL) {
	  aamp_err("err: buffer header is null");
	  return OMX_ErrorBadParameter;
	}

	if (pBuffer->nFilledLen == 0) {
		aamp_err("Ouch! In %s: no data in the output buffer!\n", __func__);
		return OMX_ErrorNone;
	}

	hal_mutex_lock(private->ref_mutex);
	private->asr_ref++;
	hal_mutex_unlock(private->ref_mutex);

	/* Output data to standard output */
	pBuffer->nFilledLen = 0;
	err = OMX_FillThisBuffer(private->asr_handle, pBuffer);
	if (err != OMX_ErrorNone)
		aamp_err("OMX_FillThisBuffer err: %x", err);

	hal_mutex_lock(private->ref_mutex);
	private->asr_ref--;
	hal_mutex_unlock(private->ref_mutex);

	return err;
}

static int get_port_index(OMX_COMPONENTTYPE *comp, OMX_DIRTYPE dir,
	OMX_PORTDOMAINTYPE domain, int start)
{
	int i;
	int ret = -1;
	OMX_PARAM_PORTDEFINITIONTYPE port_def;
	setHeader(&port_def, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	for (i = start; i < OMX_ASR_PORT_NUMBER_SUPPORTED; i++) {
		port_def.nPortIndex = i;
		ret = OMX_GetParameter(comp, OMX_IndexParamPortDefinition, &port_def);
		if (ret == OMX_ErrorNone && port_def.eDir == dir
			&& port_def.eDomain == domain) {
			ret = i;
			break;
		}
	}
	aamp_debug("index:%d\n", i);
	if (i == OMX_ASR_PORT_NUMBER_SUPPORTED)
		aamp_err("can not get port, dir:%d, domain:%d, start:%d",
			dir, domain, start);
	return ret;
}

static OMX_ERRORTYPE alloc_front_asr_buffer(FrontAsrPrivateType *priv, int port_index, OMX_S32 num, OMX_S32 size)
{
	OMX_S32 i = 0;
	OMX_BUFFERHEADERTYPE **buffer;
	OMX_ERRORTYPE ret = OMX_ErrorNone;


	buffer = malloc(num * sizeof(OMX_BUFFERHEADERTYPE *));
	if (NULL == buffer)
		return OMX_ErrorBadParameter;

	if (port_index == priv->port_asr_in)
	{
		priv->bufferin = buffer;
	}

	if (port_index == priv->port_asr_out)
	{
		priv->bufferout = buffer;
	}

	for (i = 0; i < num; i++) {

		buffer[i] = NULL;
		ret = OMX_AllocateBuffer(priv->asr_handle, &buffer[i],
				port_index, priv, size);
		aamp_debug("AllocateBuffer %p on port %d\n", buffer[i], port_index);
		if (ret != OMX_ErrorNone) {
			aamp_err("Error on AllocateBuffer %p on port %d\n",
				&buffer[i], port_index);
			break;
		}

	}

	return ret;
}

static int free_front_asr_buffer(FrontAsrPrivateType *priv, int port_index, OMX_S32 num)
{
	OMX_S32 i = 0;
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	OMX_BUFFERHEADERTYPE **buffer;


	if (priv->asr_handle) {


		if (port_index == priv->port_asr_in)
		{
			buffer = priv->bufferin;
			priv->bufferin = NULL;
		}

		if (port_index == priv->port_asr_out)
		{
			buffer = priv->bufferout;
			priv->bufferout = NULL;
		}


		if (buffer)
		{
			for (i = 0; i < num; i++) {
				if (buffer[i]) {
					ret = OMX_FreeBuffer(priv->asr_handle,
							port_index,
							buffer[i]);
					if (ret != OMX_ErrorNone)
						aamp_err("port %d ,freebuffer:%ld failed",
							port_index, i);
				}
				buffer[i] = NULL;

			}
			free(buffer);
			buffer = NULL;
		}
	}
	return ret;
}

static int front_config_asr_component(FrontAsrPrivateType *priv)
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	OMX_PORT_PARAM_TYPE sParam;
	OMX_AUDIO_PARAM_PCMMODETYPE audio_params;
	OMX_AUDIO_CONFIG_AUTOSPEECHRECOGNITIONTYPE sAsrConfig;
	front_asr_record_param *asr_record_param = &priv->asr_param->asr_record_param;

	if (asr_record_param->bits == 0 || asr_record_param->channels == 0 || asr_record_param->afe_out_channels == 0 ||
		asr_record_param->rate == 0 || asr_record_param->msec == 0)
		return OMX_ErrorBadParameter;

	int frame_bytes = asr_record_param->bits / 8 * asr_record_param->channels;
	uint64_t len = (frame_bytes * asr_record_param->rate * asr_record_param->msec) / 1000;  /* 6 * 16 * 32 = 3072 bytes */

	/* set input port */

	memset(&priv->asr_port_para[priv->port_asr_in], 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	priv->asr_port_para[priv->port_asr_in].nBufferCountActual = DEFAULT_ASR_BUF_CNT;
	priv->asr_port_para[priv->port_asr_in].bBuffersContiguous = 1;
	priv->asr_port_para[priv->port_asr_in].eDomain = OMX_PortDomainAudio;
	priv->asr_port_para[priv->port_asr_in].format.audio.eEncoding = OMX_AUDIO_CodingPCM;
	priv->asr_port_para[priv->port_asr_in].nPortIndex = priv->port_asr_in;
	priv->asr_port_para[priv->port_asr_in].nBufferSize = len;

	setHeader(&priv->asr_port_para[priv->port_asr_in], sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	ret = OMX_SetParameter(priv->asr_handle, OMX_IndexParamPortDefinition,
			&priv->asr_port_para[priv->port_asr_in]);
	if (ret) {
		aamp_err("set port params error!");
		return ret;
	}

	memset(&audio_params, 0, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
	audio_params.nChannels = asr_record_param->channels;
	audio_params.nBitPerSample = asr_record_param->bits;
	audio_params.nSamplingRate = asr_record_param->rate;
	audio_params.nPortIndex = priv->port_asr_in;
	setHeader(&audio_params, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
	ret = OMX_SetParameter(priv->asr_handle, OMX_IndexParamAudioPcm,
			&audio_params);
	if (ret) {
		aamp_err("set audio render params error!");
		return ret;
	}

	/* set input port asr config */
	memset(&sAsrConfig, 0, sizeof(OMX_AUDIO_CONFIG_AUTOSPEECHRECOGNITIONTYPE));
	sAsrConfig.nPortIndex = priv->port_asr_in;
	sAsrConfig.bAutoSpeechRecg = priv->asr_param->asr_enable;
	sAsrConfig.bAsrdump = priv->asr_param->asr_dump;
	sAsrConfig.nAfeOutCh = asr_record_param->afe_out_channels;
	sAsrConfig.nAsrMsec = asr_record_param->msec;
	//sAsrConfig.bAsr = priv->asr_param->asr;
	ret = OMX_SetConfig(priv->asr_handle, OMX_IndexConfigAudioAutoSpeechRecognition,
			&sAsrConfig);
	if (ret) {
		aamp_err("set asr_config error!");
		return ret;
	}

	/* set output port */
	priv->port_asr_out = get_port_index(priv->asr_handle, OMX_DirOutput, OMX_PortDomainAudio, 0);
	if (priv->port_asr_out < 0) {
		aamp_err("Error in get port index, port_asr_out %d\n", priv->port_asr_out);
		ret = OMX_ErrorBadPortIndex;
		return ret;
	}
	memset(&priv->asr_port_para[priv->port_asr_out], 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	priv->asr_port_para[priv->port_asr_out].nBufferCountActual = DEFAULT_ASR_BUF_CNT;
	priv->asr_port_para[priv->port_asr_out].bBuffersContiguous = 1;
	priv->asr_port_para[priv->port_asr_out].eDomain = OMX_PortDomainAudio;
	priv->asr_port_para[priv->port_asr_out].format.audio.eEncoding = OMX_AUDIO_CodingPCM;

	if (priv->asr_param->asr_dump)
		priv->asr_port_para[priv->port_asr_out].nBufferSize = \
		(len / asr_record_param->channels) * (asr_record_param->channels + asr_record_param->afe_out_channels) + sizeof(wakeup_info_t); // (3072/ 3)  * 4 + 124 = 4220 byte
	else
		priv->asr_port_para[priv->port_asr_out].nBufferSize = \
		(len / asr_record_param->channels) * (asr_record_param->afe_out_channels) + sizeof(wakeup_info_t); // (3072 / 3) * 1  + 124 = 1148 byte

	priv->asr_port_para[priv->port_asr_out].nPortIndex = priv->port_asr_out;

	setHeader(&priv->asr_port_para[priv->port_asr_out], sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	ret = OMX_SetParameter(priv->asr_handle, OMX_IndexParamPortDefinition,
			&priv->asr_port_para[priv->port_asr_out]);
	if (ret) {
		aamp_err("set port params error!");
		return ret;
	}

	memset(&audio_params, 0, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
	audio_params.nChannels = asr_record_param->channels;
	audio_params.nBitPerSample = asr_record_param->bits;
	audio_params.nSamplingRate = asr_record_param->rate;
	audio_params.nPortIndex = priv->port_asr_out;
	setHeader(&audio_params, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
	ret = OMX_SetParameter(priv->asr_handle, OMX_IndexParamAudioPcm,
			&audio_params);
	if (ret) {
		aamp_err("set audio render params error!");
		return ret;
	}

	/** Get the number of ports */
	setHeader(&sParam, sizeof(OMX_PORT_PARAM_TYPE));
	ret = OMX_GetParameter(priv->asr_handle, OMX_IndexParamAudioInit, &sParam);
	if(ret != OMX_ErrorNone){
		aamp_err("Error in getting OMX_PORT_PARAM_TYPE parameter\n");
		return ret;
	}
	aamp_debug("asr has %d ports\n",(int)sParam.nPorts);

	return ret;
}

static int front_config_asr_source_component(FrontAsrPrivateType *priv)
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	OMX_AUDIO_PARAM_PCMMODETYPE audio_params;
	OMX_PORT_PARAM_TYPE sParam;
	int port_are_out = -1;
	front_asr_record_param *asr_record_param = &priv->asr_param->asr_record_param;

	if (asr_record_param->bits == 0 || asr_record_param->channels == 0 || asr_record_param->afe_out_channels == 0 ||
		asr_record_param->rate == 0 || asr_record_param->msec == 0)
		return OMX_ErrorBadParameter;

	int frame_bytes = asr_record_param->bits / 8 * asr_record_param->channels;
	uint64_t len = (frame_bytes * asr_record_param->rate * asr_record_param->msec) / 1000; /* 6 * 16 * 32 = 3072 bytes */

	port_are_out = get_port_index(priv->arecord_handle, OMX_DirOutput, OMX_PortDomainAudio, 0);
	if (port_are_out < 0) {
		aamp_err("Error in get port index, port_are_out %d\n", port_are_out);
		ret = OMX_ErrorBadPortIndex;
		return ret;
	}

	memset(&priv->arecord_port_para, 0, sizeof(priv->arecord_port_para));
	priv->buf_num = DEFAULT_ASR_BUF_CNT;
	priv->arecord_port_para.nBufferCountActual = priv->buf_num;
	priv->arecord_port_para.bBuffersContiguous = 1;
	priv->arecord_port_para.eDomain = OMX_PortDomainAudio;
	priv->arecord_port_para.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
	priv->arecord_port_para.nPortIndex = port_are_out;
	priv->arecord_port_para.nBufferSize = len;

	setHeader(&priv->arecord_port_para, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	ret = OMX_SetParameter(priv->arecord_handle, OMX_IndexParamPortDefinition,
			&priv->arecord_port_para);
	if (ret) {
		aamp_err("set port params error!");
		return ret;
	}

	memset(&audio_params, 0, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
	audio_params.nChannels = asr_record_param->channels;
	audio_params.nBitPerSample = asr_record_param->bits;
	audio_params.nSamplingRate = asr_record_param->rate;
	setHeader(&audio_params, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
	ret = OMX_SetParameter(priv->arecord_handle, OMX_IndexParamAudioPcm,
			&audio_params);
	if (ret) {
		aamp_err("set audio record params error!");
		return ret;
	}

	/** Get the number of ports */
	setHeader(&sParam, sizeof(OMX_PORT_PARAM_TYPE));
	ret = OMX_GetParameter(priv->arecord_handle, OMX_IndexParamAudioInit, &sParam);
	if(ret != OMX_ErrorNone){
		aamp_err("Error in getting OMX_PORT_PARAM_TYPE parameter\n");
		return ret;
	}
	aamp_debug("Audio record has %d ports\n",(int)sParam.nPorts);

	return ret;
}

int do_rpdata_enable_front_asr(void *private, int index, int val)
{
	FrontAsrPrivateType* priv = (FrontAsrPrivateType*)private;
	OMX_AUDIO_CONFIG_AUTOSPEECHRECOGNITIONTYPE sAsrConfig;
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	g_front_asr_enable[index] = val;

	if (priv == NULL || priv->asr_handle == NULL)
		return ret;

	memset(&sAsrConfig, 0, sizeof(OMX_AUDIO_CONFIG_AUTOSPEECHRECOGNITIONTYPE));

	sAsrConfig.nPortIndex = priv->port_asr_in;
	sAsrConfig.bAutoSpeechRecg = priv->asr_param->asr_enable;
	sAsrConfig.bAsrdump = priv->asr_param->asr_dump;
	//sAsrConfig.bAsr = g_front_asr_enable[index];
	sAsrConfig.bAsrVad = priv->asr_param->vad;

	priv->asr_param->asr = g_front_asr_enable[index];

	/* set input port asr config */
	ret = OMX_SetConfig(priv->asr_handle, OMX_IndexConfigAudioAutoSpeechRecognition, &sAsrConfig);
	if (ret) {
		aamp_err("set asr_config error!");
		return ret;
	}

	aamp_info("do_rpdata_enable_front_asr %d\n", val);
	return ret;

}

int do_rpdata_enable_front_vad(void *private, int index, int val)
{
	FrontAsrPrivateType* priv = (FrontAsrPrivateType*)private;

	OMX_AUDIO_CONFIG_AUTOSPEECHRECOGNITIONTYPE sAsrConfig;
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	g_front_vad_enable[index] = val;

	if (priv == NULL || priv->asr_handle == NULL)
		return ret;

	memset(&sAsrConfig, 0, sizeof(OMX_AUDIO_CONFIG_AUTOSPEECHRECOGNITIONTYPE));

	sAsrConfig.nPortIndex = priv->port_asr_in;
	sAsrConfig.bAutoSpeechRecg = priv->asr_param->asr_enable;
	sAsrConfig.bAsrdump = priv->asr_param->asr_dump;
	//sAsrConfig.bAsr =  priv->asr_param->asr;
	sAsrConfig.bAsrVad = g_front_vad_enable[index];

	priv->asr_param->vad = g_front_vad_enable[index];

	/* set input port asr config */
	ret = OMX_SetConfig(priv->asr_handle, OMX_IndexConfigAudioAutoSpeechRecognition, &sAsrConfig);
	if (ret) {
		aamp_err("set asr_config error!");
		return ret;
	}

	aamp_info("do_rpdata_enable_front_vad %d\n", val);
	return ret;
}

int do_rpdata_enable_whole_front_asr(void *private, int index, int val)
{
	FrontAsrPrivateType* priv = (FrontAsrPrivateType*)private;
	OMX_AUDIO_CONFIG_AUTOSPEECHRECOGNITIONTYPE sAsrConfig;
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	g_front_asr_whole_enable[index] = val;

	if (priv == NULL || priv->asr_handle == NULL)
		return ret;

	memset(&sAsrConfig, 0, sizeof(OMX_AUDIO_CONFIG_AUTOSPEECHRECOGNITIONTYPE));

	sAsrConfig.nPortIndex = priv->port_asr_in;
	sAsrConfig.bAutoSpeechRecg = g_front_asr_whole_enable[index];
	sAsrConfig.bAsrdump = priv->asr_param->asr_dump;
	//sAsrConfig.bAsr =  priv->asr_param->asr;
	sAsrConfig.bAsrVad = priv->asr_param->vad;

	priv->asr_param->asr_enable = g_front_asr_whole_enable[index];

	/* set input port asr config */
	ret = OMX_SetConfig(priv->asr_handle, OMX_IndexConfigAudioAutoSpeechRecognition, &sAsrConfig);
	if (ret) {
		aamp_err("set asr_config error!");
		return ret;
	}


	aamp_info("do_rpdata_enable_whole_front_asr %d\n", val);
	return ret;
}

int do_rpdata_enable_front_asr_dump_merge(void *private, int index, int val)
{
	FrontAsrPrivateType* priv = (FrontAsrPrivateType*)private;

	OMX_AUDIO_CONFIG_AUTOSPEECHRECOGNITIONTYPE sAsrConfig;
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	g_front_asr_dump_merge[index] = val;

	if (priv == NULL || priv->asr_handle == NULL)
		return ret;

	memset(&sAsrConfig, 0, sizeof(OMX_AUDIO_CONFIG_AUTOSPEECHRECOGNITIONTYPE));

	sAsrConfig.nPortIndex = priv->port_asr_in;
	sAsrConfig.bAutoSpeechRecg = priv->asr_param->asr_enable;
	sAsrConfig.bAsrdump = g_front_asr_dump_merge[index];
	//sAsrConfig.bAsr =  priv->asr_param->asr;
	sAsrConfig.bAsrVad = priv->asr_param->vad;

	priv->asr_param->asr_dump = g_front_asr_dump_merge[index];

	/* set input port asr config */
	ret = OMX_SetConfig(priv->asr_handle, OMX_IndexConfigAudioAutoSpeechRecognition, &sAsrConfig);
	if (ret) {
		aamp_err("set asr_config error!");
		return ret;
	}


	aamp_info("do_rpdata_enable_front_asr_dump_merge %d\n", val);
	return ret;
}

static int do_rpdata_asr_start(FrontAsrPrivateType *priv)
{

	OMX_ERRORTYPE err;

	if (priv == NULL || priv->asr_handle == NULL || priv->arecord_handle == NULL)
		return OMX_ErrorBadParameter;

	priv->startup_count++;
	if (priv->startup_count > 1) {
		return OMX_ErrorNone;
	}

	err = OMX_SendCommand(priv->arecord_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	if(err != OMX_ErrorNone){
		aamp_err("Error in send OMX_CommandStateSet OMX_StateExecuting\n");
		return err;
	}
	/* Wait for commands to complete */
	omx_sem_down(priv->arecord_eventSem);

	err = OMX_SendCommand(priv->asr_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	if(err != OMX_ErrorNone){
		aamp_err("Error in send OMX_CommandStateSet OMX_StateExecuting\n");
		return err;
	}
	/* Wait for commands to complete */
	omx_sem_down(priv->asr_eventSem);

	return OMX_ErrorNone;
}

static int do_rpdata_asr_stop(FrontAsrPrivateType *priv)
{
	OMX_ERRORTYPE err;

	if (priv == NULL || priv->asr_handle == NULL || priv->arecord_handle == NULL)
		return OMX_ErrorBadParameter;

	if (!priv->startup_count) {
		aamp_err("startup count already 0\n");
		return OMX_ErrorNone;
	}

	priv->startup_count--;

	if (priv->startup_count == 0) {

		err = OMX_SendCommand(priv->arecord_handle, OMX_CommandStateSet, OMX_StatePause, NULL);
		if(err != OMX_ErrorNone){
			aamp_err("Error in send OMX_CommandStateSet OMX_StateExecuting\n");
			return err;
		}
		/* Wait for commands to complete */
		omx_sem_down(priv->arecord_eventSem);

		err = OMX_SendCommand(priv->asr_handle, OMX_CommandStateSet, OMX_StatePause, NULL);
		if(err != OMX_ErrorNone){
			aamp_err("Error in send OMX_CommandStateSet OMX_StateExecuting\n");
			return err;
		}
		/* Wait for commands to complete */
		omx_sem_down(priv->asr_eventSem);
	}

	return OMX_ErrorNone;

}

static int do_rpdata_asr_release(FrontAsrPrivateType *priv)
{

	OMX_ERRORTYPE err;

	if (priv == NULL || priv->asr_handle == NULL || priv->arecord_handle == NULL)
		return OMX_ErrorBadParameter;

	err = OMX_SendCommand(priv->arecord_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if(err != OMX_ErrorNone){
		aamp_err("Error in send OMX_CommandStateSet OMX_StateIdle\n");
		return err;
	}
	omx_sem_down(priv->arecord_eventSem);

	err = OMX_SendCommand(priv->asr_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if(err != OMX_ErrorNone){
		aamp_err("Error in send OMX_CommandStateSet OMX_StateIdle\n");
		return err;
	}
	omx_sem_down(priv->asr_eventSem);

	/* 8. free buffer */
	free_front_asr_buffer(priv, priv->port_asr_out, priv->portout_def.nBufferCountActual);

	/* 9. set component stat to loaded */
	err = OMX_SendCommand(priv->asr_handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	if(err != OMX_ErrorNone){
		aamp_err("Error in send OMX_CommandStateSet OMX_StateLoaded\n");
		return err;
	}

	err = OMX_SendCommand(priv->arecord_handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
	if(err != OMX_ErrorNone){
		aamp_err("Error in send OMX_CommandStateSet OMX_StateLoaded\n");
		return err;
	}

	omx_sem_down(priv->arecord_eventSem);

	omx_sem_down(priv->asr_eventSem);

	priv->keep_alive |= FRONT_ASR_DESTROY_BIT;

	return OMX_ErrorNone;

}

static void rpbuf_asr_destroy(FrontAsrPrivateType *priv)
{
	int asr_ref;
	int cnt = 0;

	priv->state = ASR_CTL_STOP;

	while (1) {
		hal_mutex_lock(priv->ref_mutex);
		asr_ref = priv->asr_ref;
		hal_mutex_unlock(priv->ref_mutex);
		if (asr_ref) {
			if ((++cnt % 10) == 0)
				aamp_info("priv has been used %d times\n", asr_ref);
			hal_msleep(1);
			continue;
		}
		else
			break;
	}

	free_front_asr_buffer(priv, priv->port_asr_out, priv->portout_def.nBufferCountActual);

	if (priv->arecord_eventSem) {
		omx_sem_deinit(priv->arecord_eventSem);
		free(priv->arecord_eventSem);
	}

	if (priv->asr_eventSem) {
		omx_sem_deinit(priv->asr_eventSem);
		free(priv->asr_eventSem);
	}

	if (priv->eofSem) {
		omx_sem_deinit(priv->eofSem);
		free(priv->eofSem);
	}

	if (priv->arecord_handle) {
		OMX_FreeHandle(priv->arecord_handle);
		priv->arecord_handle = NULL;
	}

	if (priv->asr_handle) {
		OMX_FreeHandle(priv->asr_handle);
		priv->asr_handle = NULL;
	}

	if (priv->front_asr_data[priv->index]) {
		rpdata_audio_destroy(priv->front_asr_data[priv->index]);
		priv->front_asr_data[priv->index] = NULL;
	}

}

static int rpbuf_asr_create(FrontAsrPrivateType *priv)
{
	OMX_ERRORTYPE err = OMX_ErrorNone;
	int port_src_out = -1;
	int i = 0;
	int ret;
	int frame_bytes = 0;
	uint64_t len = 0;  /* g_front_asr_msec ms */
	uint32_t rpdata_len;
	int index = priv->index;
	front_asr_record_param *asr_record_param = &priv->asr_param->asr_record_param;

	if (asr_record_param->bits == 0 || asr_record_param->channels == 0 || asr_record_param->afe_out_channels == 0 ||
		asr_record_param->rate == 0 || asr_record_param->msec == 0) {
		aamp_err("invaild param, bits:%d, channels:%d, afe out channels:%d, rate:%d, msec:%d\n", \
			asr_record_param->bits, asr_record_param->channels, asr_record_param->afe_out_channels, \
			asr_record_param->rate, asr_record_param->msec);
		goto exit;
	}

	frame_bytes = asr_record_param->bits / 8 * asr_record_param->channels;

	len = (frame_bytes * asr_record_param->rate * asr_record_param->msec) / 1000;

	if (priv->asr_param->asr_dump)
		rpdata_len = (len / asr_record_param->channels) * (asr_record_param->channels + asr_record_param->afe_out_channels) + sizeof(wakeup_info_t);
	else
		rpdata_len = (len / asr_record_param->channels) * (asr_record_param->afe_out_channels) + sizeof(wakeup_info_t);

	snprintf(priv->rpdata_audio_targ[index].name, sizeof(priv->rpdata_audio_targ[index].name), "%s_%d", FRONT_RPDATA_DATA_NAME, index);
	snprintf(priv->rpdata_audio_targ[index].type, sizeof(priv->rpdata_audio_targ[index].type), "%s_%d", FRONT_RPDATA_DATA_TYPE, index);
	priv->rpdata_audio_targ[index].dir = FRONT_RPDATA_DATA_DIR_RV;

	aamp_info("dir:%d, type:%s, name:%s\n", priv->rpdata_audio_targ[index].dir, priv->rpdata_audio_targ[index].type, priv->rpdata_audio_targ[index].name);

	priv->front_asr_data[index] = rpdata_audio_create(&priv->rpdata_audio_targ[index], NULL, rpdata_len, 0, priv);
	if (priv->front_asr_data[index] == NULL) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}

	priv->arecord_eventSem = malloc(sizeof(omx_sem_t));
	if (!priv->arecord_eventSem) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}

	ret = omx_sem_init(priv->arecord_eventSem, 0);
	if (ret < 0) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}

	priv->asr_eventSem = malloc(sizeof(omx_sem_t));
	if (!priv->asr_eventSem) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}

	ret = omx_sem_init(priv->asr_eventSem, 0);
	if (ret < 0) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}

	priv->eofSem = malloc(sizeof(omx_sem_t));
	if (!priv->eofSem) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}

	ret = omx_sem_init(priv->eofSem, 0);
	if (ret < 0) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}

	/* 1. get component arecord_handle */
	err = OMX_GetHandle(&priv->arecord_handle, "OMX.audio.record", priv, &front_asr_arecord_tunnel_callbacks);
	if(err != OMX_ErrorNone) {
		aamp_err("Audio record OMX_GetHandle failed\n");
		goto exit;
	}

	err = OMX_GetHandle(&priv->asr_handle, "OMX.audio.asr", priv, &front_asr_tunnel_callbacks);
	if(err != OMX_ErrorNone) {
		aamp_err("Audio asr OMX_GetHandle failed\n");
		goto exit;
	}

	priv->port_asr_in = get_port_index(priv->asr_handle, OMX_DirInput, OMX_PortDomainAudio, 0);
	priv->port_asr_out = get_port_index(priv->asr_handle, OMX_DirOutput, OMX_PortDomainAudio, 0);
	if (priv->port_asr_out < 0 || priv->port_asr_in < 0) {
		aamp_err("Error in get port index, port_src_out %d port_filter_in %d\n", priv->port_asr_in, priv->port_asr_out);
		goto exit;
	}


	/* 2. config component */
	err = front_config_asr_source_component(priv);
	if(err != OMX_ErrorNone){
		aamp_err("Error in config_asr_source_component\n");
		goto exit;
	}

	err = front_config_asr_component(priv);
	if(err != OMX_ErrorNone){
		aamp_err("Error in config_asr_component\n");
		goto exit;
	}


	/* 3. set tunnel */
	port_src_out = get_port_index(priv->arecord_handle, OMX_DirOutput, OMX_PortDomainAudio, 0);
	if (port_src_out < 0){
		aamp_err("Error in get port index, port_src_out %d\n", port_src_out);
		goto exit;
	}

	err = OMX_SetupTunnel(priv->arecord_handle, port_src_out, priv->asr_handle, priv->port_asr_in);
	if(err != OMX_ErrorNone) {
		aamp_err("Set up Tunnel between dump & audio render Failed\n");
		goto exit;
	}

	/* 4. set component stat to idle */

	err = OMX_SendCommand(priv->asr_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if(err != OMX_ErrorNone){
		aamp_err("Error in send OMX_CommandStateSet OMX_StateIdle\n");
		goto exit;
	}

	err = OMX_SendCommand(priv->arecord_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if(err != OMX_ErrorNone){
		aamp_err("Error in send OMX_CommandStateSet OMX_StateIdle\n");
		goto exit;
	}

	omx_sem_down(priv->arecord_eventSem);

	priv->portout_def.nPortIndex = priv->port_asr_out;
	setHeader(&priv->portout_def, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	err = OMX_GetParameter(priv->asr_handle, OMX_IndexParamPortDefinition, &priv->portout_def);
	if (err != OMX_ErrorNone) {
		aamp_err("Error when getting OMX_PORT_PARAM_TYPE,%x\n", err);
		goto exit;
	}
	if ((priv->asr_port_para[priv->port_asr_out].nBufferSize != priv->portout_def.nBufferSize) ||
		(priv->asr_port_para[priv->port_asr_out].nBufferCountActual != priv->portout_def.nBufferCountActual)) {
		aamp_err("Error port nBufferSize %ld port nBufferCountActual %ld nBufferSize %ld nBufferCountActual %ld\n", \
					priv->asr_port_para[priv->port_asr_out].nBufferSize, priv->asr_port_para[priv->port_asr_out].nBufferCountActual , \
					priv->portout_def.nBufferSize, priv->portout_def.nBufferCountActual);
		goto exit;
	}

	err = alloc_front_asr_buffer(priv, priv->port_asr_out, priv->portout_def.nBufferCountActual, priv->portout_def.nBufferSize);
	if (err != OMX_ErrorNone) {
		aamp_err("Error when alloc_buffer,%x\n", err);
		goto exit;
	}

	omx_sem_down(priv->asr_eventSem);

	/* 5. send buffer to source component queue */
	for (i = 0; i < priv->buf_num; i++) {
		err = OMX_FillThisBuffer(priv->asr_handle, priv->bufferout[i]);
		aamp_err("OMX_FillThisBuffer %p on port %lu\n", priv->bufferout[i],
			priv->bufferout[i]->nOutputPortIndex);
		if(err != OMX_ErrorNone){
			aamp_err("Error in send OMX_FillThisBuffer\n");
			goto exit;
		}
	}

	rpdata_wait_connect(priv->front_asr_data[index]->rpd_hld);

	return OMX_ErrorNone;

exit:

	rpbuf_asr_destroy(priv);

	return err;
}

static void rpbuf_asr_process(void *arg)
{
	FrontAsrPrivateType *priv = (FrontAsrPrivateType *)arg;
	int ret;
	asr_queue_item item;

	while (1) {

		if (priv->keep_alive & FRONT_ASR_DESTROY_BIT)
			goto exit;

		ret = hal_queue_recv(priv->asr_queue, &item, HAL_WAIT_FOREVER);
		if (ret != 0) {
			aamp_err("Failed to recv queue\n");
			goto exit;
		}

		aamp_info("asr %d receive cmd:%d\n", priv->index, item.cmd);

		switch (item.cmd) {

		case RPDATA_CTL_CONFIG:
			if (item.arg == NULL || item.len != sizeof(front_asr_record_param)) {
				aamp_err("audio rpdata ctrl config param is invaild %d \n", item.len);
				break;
			}
			memcpy(&priv->asr_param->asr_record_param, (front_asr_record_param *)item.arg, sizeof(front_asr_record_param));
			if (priv->config_flag == 0) {
				ret = rpbuf_asr_create(priv);
				if (ret != 0) {
					aamp_err("rpbuf_asr_create failed \n");
				}
				priv->config_flag = 1;
			}
			break;

		case RPDATA_CTL_MSPEECH_STATUS:
			if (item.arg == NULL || item.len != sizeof(speech_status_t)) {
				aamp_err("audio rpdata ctrl config speech status is invaild %d \n", item.len);
				break;
			}
			memcpy(&priv->asr_param->speech_status, (speech_status_t *)item.arg, sizeof(speech_status_t));
			aamp_info("mspeech status online:%d, cur_time_hour:%d, loglev:%d, awake_threshold_type:%d, awake_threshold:%d\n", \
				priv->asr_param->speech_status.is_online, priv->asr_param->speech_status.cur_time_hour,priv->asr_param->speech_status.log_level, \
				priv->asr_param->speech_status.awake_threshold_type, priv->asr_param->speech_status.awake_threshold);
			break;

		case RPDATA_CTL_START:
			ret = do_rpdata_asr_start(priv);
			if (ret != 0) {
				aamp_err("do_rpdata_asr_start failed \n");
			}
			priv->state = ASR_CTL_START;
			break;
		case RPDATA_CTL_STOP:
			ret = do_rpdata_asr_stop(priv);
			if (ret != 0) {
				aamp_err("do_rpdata_asr_stop failed \n");
			}
			break;
		case RPDATA_CTL_RELEASE:
			ret = do_rpdata_asr_release(priv);
			if (ret != 0) {
				aamp_err("do_rpdata_asr_release failed \n");
			}
			rpbuf_asr_destroy(priv);
			break;
		default:
			aamp_err("unknown cmd:%d\n", item.cmd);
			break;
		}
	}
exit:

	hal_sem_post(priv->thread_release);

	hal_thread_stop(priv->asr_task);

}

hal_queue_t asr_get_queue_handle(void *private)
{
	FrontAsrPrivateType* priv = (FrontAsrPrivateType*)private;

	if (priv == NULL) {
		aamp_err("task create failed");
		return NULL;
	}

	return priv->asr_queue;

}

void asr_ctrl_destroy(void *private)
{
	FrontAsrPrivateType* priv = (FrontAsrPrivateType*)private;

	priv->state = ASR_CTL_RELEASE;

	hal_sem_wait(priv->thread_release);

	OMX_Deinit();

	if (priv->asr_queue) {
		hal_queue_delete(priv->asr_queue);
		priv->asr_queue = NULL;
	}

	if (priv->thread_release) {
		hal_sem_delete(priv->thread_release);
		priv->thread_release = NULL;
	}

	if (priv->ref_mutex) {
		hal_mutex_delete(priv->ref_mutex);
		priv->ref_mutex = NULL;
	}

	if (priv->asr_param) {
		free(priv->asr_param);
		priv->asr_param = NULL;
	}

	if (priv) {
		free(priv);
		priv = NULL;
	}

}

void* asr_ctrl_init(int index)
{
	int ret = 0;
	FrontAsrPrivateType* priv = NULL;
	char name[32];

	/* Initialize application private data */
	priv = malloc(sizeof(FrontAsrPrivateType));
	if (!priv) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}
	memset(priv, 0, sizeof(FrontAsrPrivateType));

	priv->asr_param = malloc(sizeof(front_asr_param));
	if (!priv->asr_param) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}
	memset(priv->asr_param, 0 , sizeof(front_asr_param));

	priv->asr_queue = hal_queue_create("front_asr", sizeof(asr_queue_item), FRONT_ASR_QUEUE);
	if (!priv->asr_queue) {
		aamp_err("[%s] line:%d", __func__, __LINE__);
		goto exit;
	}

	priv->thread_release = hal_sem_create(0);
	if (!priv->thread_release) {
		aamp_err("hal_sem_create failed!\n");
		goto exit;
	}

	priv->ref_mutex = hal_mutex_create();
	if (!priv->ref_mutex) {
		aamp_err("hal_mutex_create failed\n");
		goto exit;
	}

	priv->asr_param->asr_enable = g_front_asr_whole_enable[index];
	priv->asr_param->asr_dump = g_front_asr_dump_merge[index];
	priv->asr_param->asr = g_front_asr_enable[index];
	priv->asr_param->vad = g_front_vad_enable[index];

	ret = OMX_Init();
	if(ret != OMX_ErrorNone) {
		aamp_err("OMX_Init() failed\n");
		goto exit;
	}

	snprintf(name, sizeof(name), "rpbuf_asr_process_%d", index);

	priv->asr_task = hal_thread_create(rpbuf_asr_process, priv, name, 4096,
			configAPPLICATION_AAMP_PRIORITY);

	if (priv->asr_task == NULL) {
		aamp_err("task create failed");
		goto exit;
	}

	priv->state = ASR_CTL_INIT;

	priv->index = index;

	return priv;
exit:

	OMX_Deinit();

	priv->state = 0;

	if (priv->asr_queue) {
		hal_queue_delete(priv->asr_queue);
		priv->asr_queue = NULL;
	}

	if (priv->thread_release) {
		hal_sem_delete(priv->thread_release);
		priv->thread_release = NULL;
	}

	if (priv->ref_mutex) {
		hal_mutex_delete(priv->ref_mutex);
		priv->ref_mutex = NULL;
	}

	if (priv->asr_param) {
		free(priv->asr_param);
		priv->asr_param = NULL;
	}

	if (priv) {
		free(priv);
		priv = NULL;
	}
	return NULL;
}

