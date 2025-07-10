/**
  audio_process/rpbuf_aec_process.c

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <aw_list.h>
#include <hal_mutex.h>
#include <hal_mem.h>
#include <rpbuf.h>
#include <console.h>
#include <ringbuffer.h>

#include <hal_time.h>
#include <hal_status.h>

#include "rpbuf_common_interface.h"

#include "rpbuf_eq_process.h"
#include "audio_amp_log.h"

static int  g_eq_dump_merge[RPBUF_CRTL_EQ_COUNTS] = {0};
static int  g_eq_enable[RPBUF_CRTL_EQ_COUNTS] ={0};
static EqControlType g_eq_control[RPBUF_CRTL_EQ_COUNTS];
static int g_eq_index = 0;

//static unsigned long g_recv_time = 0;
//static unsigned long g_send_time = 0;
//static unsigned long g_empty_time = 0;

#define AEQ_IN_EV_DATA_GET  (1 << 0)
#define AEQ_OUT_EV_DATA_GET (1 << 1)
#define AEQ_IN_EV_DATA_SET  (1 << 2)
#define AEQ_OUT_EV_DATA_SET (1 << 3)

#define EQ_PROCESS_DESTROY_BIT		(0x1<<0)

#define EQ_PROCESS_QUEUE				(8)

void rpbuf_eq_transfer_destroy(void *private);

static void setHeader(OMX_PTR header, OMX_U32 size) {
  OMX_VERSIONTYPE* ver = (OMX_VERSIONTYPE*)(header + sizeof(OMX_U32));
  *((OMX_U32*)header) = size;

  ver->s.nVersionMajor = VERSIONMAJOR;
  ver->s.nVersionMinor = VERSIONMINOR;
  ver->s.nRevision = VERSIONREVISION;
  ver->s.nStep = VERSIONSTEP;
}

static void rpbuf_available_cb(struct rpbuf_buffer *buffer, void *priv)
{
	aamp_debug("buffer \"%s\" is available\n", rpbuf_buffer_name(buffer));
}

static int rpbuf_aeq_in_cb(struct rpbuf_buffer *buffer,
		void *data, int data_len, void *priv)
{
	EqPrivateType *private = (EqPrivateType *)priv;
	int ret = 0;
	int write_size = 0;
	int size = -1;
	int offset = 0;
	int timeout =  1000;
	hal_event_bits_t ret_bits = 0;

	if (private->state == EQ_CTL_STOP || private->state == EQ_CTL_RELEASE) {
		aamp_err("eq priv has been release, so exit this cb\n");
		if ((uint32_t)g_eq_control[g_eq_index].private == (uint32_t)private)
			aamp_err("this cb priv %0x08x is the same with last priv, which has been free\n", (uint32_t)private);
		return 0;
	}

	hal_mutex_lock(private->ref_mutex);
	private->eq_ref++;
	hal_mutex_unlock(private->ref_mutex);

/*
	aamp_err("buffer \"%s\" received data (len: %d) delta: %ld\n", \
			rpbuf_buffer_name(buffer), data_len, OSTICK_TO_MS(hal_tick_get()) - g_recv_time);

	g_recv_time = OSTICK_TO_MS(hal_tick_get());
*/
	if (data_len != private->rpbuf_in_len) {
		aamp_err("buffer \"%s\" received data len err! (offset: %d, rpbuf_in_len %ld, len: %d):\n", \
		rpbuf_buffer_name(buffer), data - rpbuf_buffer_va(buffer),private->rpbuf_in_len, data_len);
		ret = -1;
		hal_mutex_lock(private->ref_mutex);
		private->eq_ref--;
		hal_mutex_unlock(private->ref_mutex);
		return ret;
	}

	write_size = data_len;

	while (write_size > 0) {

		size = hal_ringbuffer_put(private->aeq_rb, data + offset, write_size);
		if (size < 0) {
			aamp_err("ring buf put err %d\n", ret);
			goto exit;
		}

		if (size == write_size)
			break;

		offset += size;
		write_size -= size;

		ret_bits = hal_event_wait(private->event, AEQ_IN_EV_DATA_GET,
			HAL_EVENT_OPTION_CLEAR | HAL_EVENT_OPTION_AND, MS_TO_OSTICK(timeout));
		if ((ret_bits & AEQ_IN_EV_DATA_GET) != AEQ_IN_EV_DATA_GET) {
			ret = -1;
			aamp_err("eq in cb wait data timeout:%d", timeout);
			break;
		}
	}

	hal_event_set_bits(private->event, AEQ_IN_EV_DATA_SET);

	memset(data, 0, data_len);


exit:
	hal_mutex_lock(private->ref_mutex);
	private->eq_ref--;
	hal_mutex_unlock(private->ref_mutex);
	return ret;
}

static int rpbuf_destroyed_cb(struct rpbuf_buffer *buffer, void *priv)
{
	EqPrivateType *private = (EqPrivateType *)priv;
	int index = private->index;

	aamp_debug("buffer \"%s\": remote buffer destroyed\n", rpbuf_buffer_name(buffer));
	if (!strncmp(rpbuf_buffer_name(buffer), private->rpbuf_transfer_in_name[index], sizeof(private->rpbuf_transfer_in_name[index])))
	{
		aamp_debug("buffer \"%s\": recv thread will exit\n", rpbuf_buffer_name(buffer));
	}
	return 0;
}

static const struct rpbuf_buffer_cbs rpbuf_eq_in_cbs = {
	.available_cb 	= rpbuf_available_cb,
	.rx_cb 			= rpbuf_aeq_in_cb,
	.destroyed_cb 	= rpbuf_destroyed_cb,
};

static const struct rpbuf_buffer_cbs rpbuf_eq_out_cbs = {
	.available_cb 	= rpbuf_available_cb,
	.rx_cb 			= NULL,
	.destroyed_cb 	= rpbuf_destroyed_cb,
};

/* Application private date: should go in the component field (segs...) */
static OMX_U32 error_value = 0u;

OMX_CALLBACKTYPE eq_untunnel_callbacks = {
	.EventHandler = RpbufAudioEqualizerEventHandler,
	.EmptyBufferDone = RpbufAudioEqualizerEmptyBufferDone,
	.FillBufferDone = RpbufAudioEqualizerFillBufferDone,
};

OMX_CALLBACKTYPE eq_tunnel_callbacks = {
	.EventHandler = RpbufAudioEqualizerEventHandler,
	.EmptyBufferDone = NULL,
	.FillBufferDone = NULL,
};


/* Callbacks implementation */
OMX_ERRORTYPE RpbufAudioEqualizerEventHandler(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 Data1,
  OMX_U32 Data2,
  OMX_PTR pEventData) {

	char *name;

	EqPrivateType *private = (EqPrivateType *)pAppData;
	if (hComponent == private->aeq_handle)
		name = "AudioEqualizer";

	hal_mutex_lock(private->ref_mutex);
	private->eq_ref++;
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
			omx_sem_up(private->aeq_eventSem);
		} else  if (Data1 == OMX_CommandPortEnable){
			aamp_debug("%s CmdComplete OMX_CommandPortEnable\n", name);
			omx_sem_up(private->aeq_eventSem);
		} else if (Data1 == OMX_CommandPortDisable) {
			aamp_debug("%s CmdComplete OMX_CommandPortDisable\n", name);
			omx_sem_up(private->aeq_eventSem);
		}
	} else if(eEvent == OMX_EventBufferFlag) {
		if ((int)Data2 == OMX_BUFFERFLAG_EOS) {
			aamp_info("%s BufferFlag OMX_BUFFERFLAG_EOS\n", name);
			if (hComponent == private->aeq_handle) {
				aamp_info("end of tunnel");
				omx_sem_up(private->eofSem);
			}
		} else
			aamp_info("%s OMX_EventBufferFlag %lx", name, Data2);
	} else if (eEvent == OMX_EventError) {
		error_value = Data1;
		aamp_err("Receive error event. value:%lx", error_value);
		omx_sem_up(private->aeq_eventSem);
	} else {
		aamp_err("Param1 is %i\n", (int)Data1);
		aamp_err("Param2 is %i\n", (int)Data2);
	}

	hal_mutex_lock(private->ref_mutex);
	private->eq_ref--;
	hal_mutex_unlock(private->ref_mutex);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE RpbufAudioEqualizerEmptyBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer) {

	OMX_ERRORTYPE err;
	EqPrivateType *private = (EqPrivateType *)pAppData;

	if (pBuffer == NULL || pAppData == NULL) {
		aamp_err("err: buffer header is null");
		return OMX_ErrorBadParameter;
	}

	hal_mutex_lock(private->ref_mutex);
	private->eq_ref++;
	hal_mutex_unlock(private->ref_mutex);
/*
	aamp_err("empty buffer done>> %p, %lu, input:%lu output:%lu time %ld delta time %ld\n", pBuffer->pBuffer, \
	pBuffer->nFlags, pBuffer->nInputPortIndex, pBuffer->nOutputPortIndex, OSTICK_TO_MS(hal_tick_get()) ,OSTICK_TO_MS(hal_tick_get()) - g_empty_time);

	g_empty_time= OSTICK_TO_MS(hal_tick_get());
*/
	err = queue(private->BufferQueue, pBuffer);
	if (err != OMX_ErrorNone)
		aamp_err("queue buffer err: %d", err);

	hal_mutex_lock(private->ref_mutex);
	private->eq_ref--;
	hal_mutex_unlock(private->ref_mutex);

	return err;

}

OMX_ERRORTYPE RpbufAudioEqualizerFillBufferDone(
  OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE* pBuffer) {

	OMX_ERRORTYPE err;
	int ret;
	int write_size = 0;
	int size = 0;
	int offset = 0;
	EqPrivateType *private = (EqPrivateType *)pAppData;
	int timeout =  200;
	hal_event_bits_t ret_bits = 0;
	int cnt = 0;

/*
	aamp_err("In the %s callback. Got buflen %i for buffer at 0x%p time %ld delta time %ld\n",
                          __func__, (int)pBuffer->nFilledLen, pBuffer, OSTICK_TO_MS(hal_tick_get()), OSTICK_TO_MS(hal_tick_get()) - g_send_time);
	g_send_time = OSTICK_TO_MS(hal_tick_get());
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
	private->eq_ref++;
	hal_mutex_unlock(private->ref_mutex);

	write_size = pBuffer->nFilledLen;

	while (write_size > 0) {

		size = hal_ringbuffer_put(private->aeq_out_rb, pBuffer->pBuffer + pBuffer->nOffset + offset, write_size);
		if (size < 0) {
			aamp_err("ring buf put err %d\n", ret);
			break;
		}

		if (size == write_size)
			break;

		if (size == 0) {
			cnt++;
			if (cnt > 3) {
				cnt = 0;
				break;
			}
		}

		offset += size;
		write_size -= size;

		ret_bits = hal_event_wait(private->send_event, AEQ_OUT_EV_DATA_GET,
			HAL_EVENT_OPTION_CLEAR | HAL_EVENT_OPTION_AND, MS_TO_OSTICK(timeout));
		if ((ret_bits & AEQ_OUT_EV_DATA_GET) != AEQ_OUT_EV_DATA_GET) {
			ret = -1;
			aamp_err("wait data timeout:%d", timeout);
			continue;
		}
	}

	hal_event_set_bits(private->send_event, AEQ_OUT_EV_DATA_SET);

	if (size == 0) {
		aamp_err("Out RingBuffer is full, process data is loss\n");
	}

/*
	if(pBuffer->nFilledLen > 0) {
		ret = rpbuf_common_transmit(private->aeq_test->aeq_arg.recv_name, (void *)pBuffer->pBuffer, pBuffer->nFilledLen, pBuffer->nOffset);
		if (ret < 0) {
			aamp_err("rpbuf_common_transmit (with data) failed\n");
		}
	}
*/
	/* Output data to standard output */
	pBuffer->nFilledLen = 0;
	err = OMX_FillThisBuffer(hComponent, pBuffer);
	if (err != OMX_ErrorNone)
		aamp_err("OMX_FillThisBuffer err: %x", err);

	hal_mutex_lock(private->ref_mutex);
	private->eq_ref--;
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
	for (i = start; i < OMX_PORT_NUMBER_SUPPORTED; i++) {
		port_def.nPortIndex = i;
		ret = OMX_GetParameter(comp, OMX_IndexParamPortDefinition, &port_def);
		if (ret == OMX_ErrorNone && port_def.eDir == dir
			&& port_def.eDomain == domain) {
			ret = i;
			break;
		}
	}
	aamp_debug("index:%d\n", i);
	if (i == OMX_PORT_NUMBER_SUPPORTED)
		aamp_err("can not get port, dir:%d, domain:%d, start:%d",
			dir, domain, start);
	return ret;
}

static OMX_ERRORTYPE alloc_aeq_buffer(EqPrivateType *priv, int port_index, OMX_S32 num, OMX_S32 size)
{
	OMX_S32 i = 0;
	OMX_BUFFERHEADERTYPE **buffer;
	OMX_ERRORTYPE ret = OMX_ErrorNone;


	buffer = malloc(num * sizeof(OMX_BUFFERHEADERTYPE *));
	if (NULL == buffer)
		return OMX_ErrorBadParameter;

	if (port_index == priv->port_filter_in)
	{
		priv->bufferin = buffer;
	}

	if (port_index == priv->port_filter_out)
	{
		priv->bufferout = buffer;
	}

	for (i = 0; i < num; i++) {

		buffer[i] = NULL;
		ret = OMX_AllocateBuffer(priv->aeq_handle, &buffer[i],
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

static int free_aeq_buffer(EqPrivateType *priv, int port_index, OMX_S32 num)
{
	OMX_S32 i = 0;
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	OMX_BUFFERHEADERTYPE **buffer;


	if (priv->aeq_handle) {


		if (port_index == priv->port_filter_in)
		{
			buffer = priv->bufferin;
		}

		if (port_index == priv->port_filter_out)
		{
			buffer = priv->bufferout;
		}


		if (buffer)
		{
			for (i = 0; i < num; i++) {
				if (buffer[i]) {
					ret = OMX_FreeBuffer(priv->aeq_handle,
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

#ifdef CONFIG_COMPONENTS_PROCESS_AW_EQ
static void print_eq_prms(const eq_prms_transfer_t *prms)
{
    int i;
    for (i = 0; i < prms->biq_num; ++i) {
        const eq_core_prms_transfer_t *core_prms = &prms->core_prms[i];
        aamp_debug(" [Biquad%02i] index:%i, type: %i, freq: %d, gain: %d, Q: %.2f\n",
                i + 1, core_prms->index, core_prms->type, core_prms->fc, core_prms->G, core_prms->Q);
    }
}
#endif

static int untunnel_config_aeq_component(EqPrivateType *priv)
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	OMX_PORT_PARAM_TYPE sParam;
	OMX_OTHER_PARAM_EQTYPE sEqParams;
	OMX_AUDIO_PARAM_PCMMODETYPE sAudioParams;
	OMX_AUDIO_CONFIG_EQUALIZERTYPE sEqConfig;
	eq_config_param_t *eq_config_param = &priv->eq_ctrl_param.eq_config_param;

	if (eq_config_param->bits == 0 || eq_config_param->channels == 0 || eq_config_param->eq_out_channels == 0 ||
		eq_config_param->rate == 0 || eq_config_param->msec == 0 || eq_config_param->eqPrmsTransfer == NULL)
		return OMX_ErrorBadParameter;

	memset(&priv->aeq_port_para[priv->port_filter_in], 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	/* set input port */
	priv->aeq_port_para[priv->port_filter_in].nBufferCountActual = DEFAULT_AEQUALIZER_BUF_CNT;
	priv->aeq_port_para[priv->port_filter_in].bBuffersContiguous = 1;
	priv->aeq_port_para[priv->port_filter_in].eDomain = OMX_PortDomainAudio;
	priv->aeq_port_para[priv->port_filter_in].format.audio.eEncoding = OMX_AUDIO_CodingPCM;
	priv->aeq_port_para[priv->port_filter_in].nPortIndex = priv->port_filter_in;
	priv->aeq_port_para[priv->port_filter_in].nBufferSize = priv->rpbuf_in_len;

	setHeader(&priv->aeq_port_para[priv->port_filter_in], sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	ret = OMX_SetParameter(priv->aeq_handle, OMX_IndexParamPortDefinition,
			&priv->aeq_port_para[priv->port_filter_in]);
	if (ret) {
		aamp_err("set port params error!");
		return ret;
	}

	/* set input port eq param */
	memset(&sEqParams, 0, sizeof(OMX_OTHER_PARAM_EQTYPE));
	sEqParams.nPortIndex = priv->port_filter_in;
	sEqParams.bReset = OMX_FALSE;
	sEqParams.bEqdump = priv->eq_ctrl_param.eq_dump;
	sEqParams.nEqOutCh = eq_config_param->eq_out_channels;
	sEqParams.nEqMsec = eq_config_param->msec;
	sEqParams.pEqprm = eq_config_param->eqPrmsTransfer;

	setHeader(&sEqParams, sizeof(OMX_OTHER_PARAM_EQTYPE));
	ret = OMX_SetParameter(priv->aeq_handle, OMX_IndexVendorParamEQ,
			&sEqParams);
	if (ret) {
		aamp_err("set eq params error!");
		return ret;
	}

	/* set input port pcm param */
	memset(&sAudioParams, 0, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
	sAudioParams.nPortIndex = priv->port_filter_in;
	sAudioParams.nChannels = eq_config_param->channels;
	sAudioParams.nBitPerSample = eq_config_param->bits;
	sAudioParams.nSamplingRate = eq_config_param->rate;

	setHeader(&sAudioParams, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
	ret = OMX_SetParameter(priv->aeq_handle, OMX_IndexParamAudioPcm,
			&sAudioParams);
	if (ret) {
		aamp_err("set audio params error!");
		return ret;
	}

	aamp_info("Audio param chan %d, bits %d, rate %d, msec %d, eq out ch %d, eq enable %d, eq dump %d\n", \
		eq_config_param->channels, eq_config_param->bits, eq_config_param->rate, eq_config_param->msec, \
		eq_config_param->eq_out_channels, priv->eq_ctrl_param.eq_enable, priv->eq_ctrl_param.eq_dump);

#ifdef CONFIG_COMPONENTS_PROCESS_AW_EQ
	print_eq_prms((eq_prms_transfer_t *)eq_config_param->eqPrmsTransfer);
#endif

	/* set input port eq config */
	memset(&sEqConfig, 0, sizeof(OMX_AUDIO_CONFIG_EQUALIZERTYPE));
	sEqConfig.nPortIndex = priv->port_filter_in;
	sEqConfig.bEnable = priv->eq_ctrl_param.eq_enable;
	ret = OMX_SetConfig(priv->aeq_handle, OMX_IndexConfigAudioEqualizer,
			&sEqConfig);
	if (ret) {
		aamp_err("set eq_config error!");
		return ret;
	}

	/* set output port */
	memset(&priv->aeq_port_para[priv->port_filter_out], 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	priv->aeq_port_para[priv->port_filter_out].nBufferCountActual = DEFAULT_AEQUALIZER_BUF_CNT;
	priv->aeq_port_para[priv->port_filter_out].bBuffersContiguous = 1;
	priv->aeq_port_para[priv->port_filter_out].eDomain = OMX_PortDomainAudio;
	priv->aeq_port_para[priv->port_filter_out].format.audio.eEncoding = OMX_AUDIO_CodingPCM;
	priv->aeq_port_para[priv->port_filter_out].nPortIndex = priv->port_filter_out;
	priv->aeq_port_para[priv->port_filter_out].nBufferSize = priv->rpbuf_out_len;

	setHeader(&priv->aeq_port_para[priv->port_filter_out], sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	ret = OMX_SetParameter(priv->aeq_handle, OMX_IndexParamPortDefinition,
			&priv->aeq_port_para[priv->port_filter_out]);
	if (ret) {
		aamp_err("set port params error!");
		return ret;
	}

	/** Get the number of ports */
	setHeader(&sParam, sizeof(OMX_PORT_PARAM_TYPE));
	ret = OMX_GetParameter(priv->aeq_handle, OMX_IndexParamAudioInit, &sParam);
	if(ret != OMX_ErrorNone){
		aamp_err("Error in getting OMX_PORT_PARAM_TYPE parameter\n");
		return ret;
	}
	aamp_debug("Audio equalizer has %d ports\n",(int)sParam.nPorts);

	return ret;
}

static void rpbuf_eq_send_thread(void *params)
{
	EqPrivateType *private = (EqPrivateType *)params;
	unsigned char *data = NULL;
	int ret = OMX_ErrorNone;
	int read_bytes = 0;
	unsigned int data_len = private->rpbuf_out_len;
	int index = private->index;
	int timeout = 300;
	int read_size = 0;
	int read_len = 0;
	int offset = 0;
	int cnt = 0;
	hal_event_bits_t ret_bits = 0;

	data = malloc(data_len);
	if (!data) {
		aamp_err("malloc len :%d failed\n", data_len);
		goto exit;
	}

	hal_mutex_lock(private->ref_mutex);
	private->eq_ref++;
	hal_mutex_unlock(private->ref_mutex);

	while (1) {

		if (private->state == EQ_CTL_RELEASE) {
			aamp_err("eq priv has been release, so exit this thread\n");
			if ((uint32_t)g_eq_control[index].private == (uint32_t)private)
				aamp_err("this priv %0x08x is the same with last priv, which has been free\n", (uint32_t)private);
			goto exit;
		}

		if (private->paused)
		{
			/* We are pause.. */
			omx_sem_wait(private->pauseSem);
		}

		read_size = data_len;
		offset = 0;
		read_len = 0;
		cnt = 0;

		while (read_size > 0) {

			read_bytes = hal_ringbuffer_get(private->aeq_out_rb, data + offset, read_size, MS_TO_OSTICK(timeout));
			if(read_bytes < 0) {
				if (read_bytes == HAL_TIMEOUT) {
					aamp_info("read timeout %d\n", timeout);
					read_bytes = 0;
				}
				else {
					aamp_err("read err:%d\n", read_bytes);
					goto exit;
				}
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

			ret_bits = hal_event_wait(private->send_event, AEQ_OUT_EV_DATA_SET,
				HAL_EVENT_OPTION_CLEAR | HAL_EVENT_OPTION_AND, MS_TO_OSTICK(timeout));
			if ((ret_bits & AEQ_OUT_EV_DATA_SET) != AEQ_OUT_EV_DATA_SET) {
				ret = -1;
				aamp_err("eq send wait data timeout:%d\n", timeout);
				continue;
			}
		}

		hal_event_set_bits(private->send_event, AEQ_OUT_EV_DATA_GET);

		if (private->startup_count == 0) {
			if (private->thread_sync)
				hal_sem_timedwait(private->thread_sync, MS_TO_OSTICK(30));
			else
				aamp_err("the private->thread_sync is null, can not call hal_sem_timedwait");
			aamp_info("exit aeq send loop!\n");
			break;
		}

		if (read_len > 0 && data_len != read_len)
		{
			aamp_err("send data len %d is not equal to read_bytes %d\n", data_len, read_len);
			goto exit;
		}

		if (read_len > 0 && private->startup_count > 0) {
			ret = rpbuf_common_transmit(private->rpbuf_transfer_out_name[private->index], data, data_len, 0);
			if (ret < 0) {
				aamp_err("rpbuf_common_transmit (with data) failed\n");
			}
		}
/*
		aamp_err("to remote. buflen %i delta %ld\n",
	                  (int)read_len, OSTICK_TO_MS(hal_tick_get()) - g_send_time);

		g_send_time = OSTICK_TO_MS(hal_tick_get());
*/
	}

exit:

	if (data) {
		free(data);
		data = NULL;
	}

	private->send_task_exit_flag = 1;

	aamp_info("end of eq send thread\n");

	hal_mutex_lock(private->ref_mutex);
	private->eq_ref--;
	hal_mutex_unlock(private->ref_mutex);

	if (private)
		hal_thread_stop(private->eq_send_task);
	else
		hal_thread_stop(NULL);

}

static void rpbuf_eq_process_thread(void *params)
{
	OMX_BUFFERHEADERTYPE *pBuffer = NULL;
	EqPrivateType *private = (EqPrivateType *)params;
	int ret = OMX_ErrorNone;
	int read_bytes = 0;
	unsigned long data_len = private->rpbuf_in_len;
	int index = private->index;
	OMX_BOOL eof = OMX_FALSE;
	int timeout = 300;
	int cnt = 0;
	int read_size = 0;
	int offset = 0;
	int read_len = 0;
	hal_event_bits_t ret_bits = 0;

	hal_mutex_lock(private->ref_mutex);
	private->eq_ref++;
	hal_mutex_unlock(private->ref_mutex);

	while (1) {

		if (private->state == EQ_CTL_RELEASE) {
			aamp_err("eq priv has been release, so exit this thread\n");
			if ((uint32_t)g_eq_control[index].private == (uint32_t)private)
				aamp_err("this priv %0x08x is the same with last priv, which has been free\n", (uint32_t)private);
			goto exit;
		}

		if (eof) {
			aamp_info("exit eq loop!\n");
			break;
		}

		if (private->paused)
		{
			/* We are pause.. */
			omx_sem_wait(private->pauseSem);
		}

		pBuffer = NULL;

		while (pBuffer == NULL) {
			/* dequeue one buffer */
			pBuffer = dequeue(private->BufferQueue);
			if (pBuffer == NULL) {
				aamp_err("queue num is %d\n", getquenelem(private->BufferQueue));
				hal_msleep(10);
				continue;
			}
		}

		if (pBuffer->nAllocLen != data_len) {
			aamp_err("data len %lu is not equal to nAllocLen %lu\n", data_len, pBuffer->nAllocLen);
			break;
		}

		read_size = data_len;
		offset = 0;
		read_len = 0;
		cnt = 0;

		while (read_size > 0) {

			read_bytes = hal_ringbuffer_get(private->aeq_rb, pBuffer->pBuffer + offset, read_size, MS_TO_OSTICK(timeout));
			if(read_bytes < 0) {
				if (read_bytes == HAL_TIMEOUT) {
					aamp_info("read timeout %d\n", timeout);
					read_bytes = 0;
				}
				else {
					aamp_err("read err:%d\n", read_bytes);
					goto exit;
				}
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

			ret_bits = hal_event_wait(private->event, AEQ_IN_EV_DATA_SET,
				HAL_EVENT_OPTION_CLEAR | HAL_EVENT_OPTION_AND, MS_TO_OSTICK(timeout));
			if ((ret_bits & AEQ_IN_EV_DATA_SET) != AEQ_IN_EV_DATA_SET) {
				ret = -1;
				aamp_info("eq process wait data timeout:%d\n", timeout);
				continue;
			}
		}

		hal_event_set_bits(private->event, AEQ_IN_EV_DATA_GET);

		if (read_len > 0 && data_len != read_len)
		{
			aamp_err("eq data len %lu is not equal to read_bytes %d\n", data_len, read_len);
			goto exit;
		}

		if (private->startup_count == 0) {
			eof = OMX_TRUE;
			pBuffer->nFilledLen = 0;
			pBuffer->nOffset = 0;
			pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
		}
		else {
			pBuffer->nFilledLen = read_len;
			pBuffer->nOffset = 0;
			pBuffer->nFlags = 0;
		}

		if (read_len || (private->startup_count == 0)) {
			pBuffer->nInputPortIndex = private->port_filter_in;
			ret = OMX_EmptyThisBuffer(private->aeq_handle, pBuffer);
			if (ret != OMX_ErrorNone) {
				aamp_err("empty this buffer error!\n");
				goto exit;
			}
		}
		else {
			ret = queue(private->BufferQueue, pBuffer);
			if (ret != OMX_ErrorNone){
				aamp_err("queue buffer err: %d", ret);
				goto exit;
			}
		}
	}
exit:

	aamp_info("end of eq process thread\n");

	if (private->thread_sync)
		hal_sem_post(private->thread_sync);

	private->process_task_exit_flag = 1;

	hal_mutex_lock(private->ref_mutex);
	private->eq_ref--;
	hal_mutex_unlock(private->ref_mutex);

	if (private)
		hal_thread_stop(private->eq_process_task);
	else
		hal_thread_stop(NULL);

}

void rpbuf_eq_init_default_param(void *private)
{
	EqPrivateType* priv = (EqPrivateType*)private;

	priv->eq_ctrl_param.eq_config_param.msec = RPBUF_EQ_DEFAULT_MSEC;
	priv->eq_ctrl_param.eq_config_param.rate = RPBUF_EQ_DEFAULT_RATE;
	priv->eq_ctrl_param.eq_config_param.channels = RPBUF_EQ_DEFAULT_CHANNELS;
	priv->eq_ctrl_param.eq_config_param.eq_out_channels = RPBUF_EQ_DEFAULT_OUT_CHANNELS;
	priv->eq_ctrl_param.eq_config_param.bits = RPBUF_EQ_DEFAULT_BIT_WIDTH;
	priv->eq_ctrl_param.eq_config_param.eqPrmsTransfer = NULL;
}

void rpbuf_eq_deinit_param(void *private)
{
	EqPrivateType* priv = (EqPrivateType*)private;

	g_eq_dump_merge[priv->index] = 0;
	g_eq_enable[priv->index] = 0;
}

int do_rpbuf_eq_config(EqPrivateType *priv)
{
	OMX_OTHER_PARAM_EQTYPE sEqParam;
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	if (priv == NULL || priv->aeq_handle == NULL)
		return ret;

	memset(&sEqParam, 0, sizeof(OMX_OTHER_PARAM_EQTYPE));

	sEqParam.nPortIndex = priv->port_filter_in;
	sEqParam.bReset = OMX_TRUE;
	sEqParam.bEqdump = priv->eq_ctrl_param.eq_dump;
	sEqParam.nEqOutCh = priv->eq_ctrl_param.eq_config_param.eq_out_channels;
	sEqParam.nEqMsec = priv->eq_ctrl_param.eq_config_param.msec;
	sEqParam.pEqprm = priv->eq_ctrl_param.eq_config_param.eqPrmsTransfer;

	/* set input port eq parms */
	setHeader(&sEqParam, sizeof(OMX_OTHER_PARAM_EQTYPE));
	ret = OMX_SetParameter(priv->aeq_handle, OMX_IndexVendorParamEQ, &sEqParam);
	if (ret) {
		aamp_err("set EqParam error!");
		return ret;
	}

	aamp_info("do_rpbuf_eq_config\n");
	return ret;
}

int do_rpbuf_enable_eq_dump_merge(void *private, int index, int val)
{
	EqPrivateType* priv = (EqPrivateType*)private;

	OMX_OTHER_PARAM_EQTYPE sEqParam;
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	g_eq_dump_merge[index] = val;

	if (priv == NULL || priv->aeq_handle == NULL)
		return ret;

	memset(&sEqParam, 0, sizeof(OMX_OTHER_PARAM_EQTYPE));

	sEqParam.nPortIndex = priv->port_filter_in;
	sEqParam.bEqdump = g_eq_dump_merge[index];
	sEqParam.nEqOutCh = priv->eq_ctrl_param.eq_config_param.eq_out_channels;
	sEqParam.nEqMsec = priv->eq_ctrl_param.eq_config_param.msec;
	sEqParam.pEqprm = priv->eq_ctrl_param.eq_config_param.eqPrmsTransfer;

	priv->eq_ctrl_param.eq_dump = g_eq_dump_merge[index];

	/* set input port eq parms */
	setHeader(&sEqParam, sizeof(OMX_OTHER_PARAM_EQTYPE));
	ret = OMX_SetParameter(priv->aeq_handle, OMX_IndexVendorParamEQ, &sEqParam);
	if (ret) {
		aamp_err("set EqParam error!");
		return ret;
	}

	aamp_info("do_rpbuf_enable_eq_dump_merge %d\n", val);
	return ret;

}

int do_rpbuf_enable_eq(void *private, int index, int val)
{
	EqPrivateType* priv = (EqPrivateType*)private;

	OMX_AUDIO_CONFIG_EQUALIZERTYPE sEqConfig;
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	g_eq_enable[index] = val;

	if (priv == NULL || priv->aeq_handle == NULL)
		return ret;

	memset(&sEqConfig, 0, sizeof(OMX_AUDIO_CONFIG_EQUALIZERTYPE));
	sEqConfig.nPortIndex = priv->port_filter_in;
	sEqConfig.bEnable = g_eq_enable[index];
	ret = OMX_SetConfig(priv->aeq_handle, OMX_IndexConfigAudioEqualizer,
			&sEqConfig);
	if (ret) {
		aamp_err("set aec_config error!");
		return ret;
	}

	priv->eq_ctrl_param.eq_enable = g_eq_enable[index];

	aamp_info("do_rpbuf_enable_eq %d\n", val);
	return ret;
}

static int do_rpbuf_eq_start(EqPrivateType *priv)
{
	OMX_ERRORTYPE err;
	int ret = -1;
	char name[32];

	if (priv == NULL || priv->aeq_handle == NULL)
		return OMX_ErrorBadParameter;

	priv->startup_count++;
	if (priv->startup_count > 1) {
		if (priv->process_task_exit_flag == 0 && priv->send_task_exit_flag == 0) {
			aamp_info("eq threads have been create, so return");
			priv->startup_count--;
			return OMX_ErrorNone;
		}
		else {
			aamp_err("remote maybe crash, restart thread");
			priv->startup_count--;
		}
	}

	if (priv->process_task_exit_flag == 0 && priv->send_task_exit_flag == 0) {
		/* set component stat to executing */
		err = OMX_SendCommand(priv->aeq_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
		if(err != OMX_ErrorNone){
			aamp_err("Error in send OMX_CommandStateSet OMX_StateExecuting\n");
			return err;
		}
		/* Wait for commands to complete */
		omx_sem_down(priv->aeq_eventSem);

		if (error_value != OMX_ErrorNone) {
			aamp_err("Error in transfet to OMX_StateExecuting\n");
			OMX_STATETYPE state;
			error_value = 0;
			err = OMX_GetState(priv->aeq_handle, &state);
			if(err != OMX_ErrorNone){
				aamp_err("Error in OMX_GetState\n");
				return err;
			}
			if (state == OMX_StateIdle) {
				err = OMX_SendCommand(priv->aeq_handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
				if(err != OMX_ErrorNone){
					aamp_err("Error in send OMX_CommandStateSet OMX_StateLoaded\n");
					return err;
				}
				/* free buffer */
				free_aeq_buffer(priv, priv->port_filter_in, priv->portin_def.nBufferCountActual);
				free_aeq_buffer(priv, priv->port_filter_out, priv->portout_def.nBufferCountActual);
				omx_sem_down(priv->aeq_eventSem);
				aamp_info("Component sent to Loaded\n");
			} else if (state == OMX_StateLoaded) {
				aamp_info("Component already loaded\n");
			} else {
				aamp_err("Component in the wrong state %d!\n", state);
			}
			ret = -1;
			return ret;
		}
	}

	if (priv->eq_process_task == NULL || priv->process_task_exit_flag == 1) {
		snprintf(name, sizeof(name), "eq_process_thread_%d", priv->index);
		priv->eq_process_task = hal_thread_create(rpbuf_eq_process_thread, priv, name, 4096,
				configAPPLICATION_AAMP_PRIORITY);

		if (priv->eq_process_task == NULL) {
			aamp_err("task %s create failed", name);
			ret = -1;
			return ret;
		}
		hal_thread_start(priv->eq_process_task);
		priv->process_task_exit_flag = 0;
	}

	if (priv->eq_send_task == NULL || priv->send_task_exit_flag == 1) {
		snprintf(name, sizeof(name), "eq_send_thread_%d", priv->index);
		priv->eq_send_task = hal_thread_create(rpbuf_eq_send_thread, priv, name, 4096,
				configAPPLICATION_AAMP_PRIORITY);

		if (priv->eq_send_task == NULL) {
			aamp_err("task %s create failed", name);
			ret = -1;
			return ret;
		}
		hal_thread_start(priv->eq_send_task);
		priv->send_task_exit_flag = 0;
	}

	return OMX_ErrorNone;
}

static int do_rpbuf_eq_resume(EqPrivateType *priv)
{
	OMX_ERRORTYPE err;

	if (priv == NULL || priv->aeq_handle == NULL)
		return OMX_ErrorBadParameter;


	err = OMX_SendCommand(priv->aeq_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
	if(err != OMX_ErrorNone){
		aamp_err("Error in send OMX_CommandStateSet OMX_StateExecuting\n");
		return err;
	}
	/* Wait for commands to complete */
	omx_sem_down(priv->aeq_eventSem);
	return OMX_ErrorNone;
}

static int do_rpbuf_eq_pause(EqPrivateType *priv)
{
	OMX_ERRORTYPE err;

	if (priv == NULL || priv->aeq_handle == NULL)
		return OMX_ErrorBadParameter;


	err = OMX_SendCommand(priv->aeq_handle, OMX_CommandStateSet, OMX_StatePause, NULL);
	if(err != OMX_ErrorNone){
		aamp_err("Error in send OMX_CommandStateSet OMX_StateExecuting\n");
		return err;
	}
	/* Wait for commands to complete */
	omx_sem_down(priv->aeq_eventSem);

	return OMX_ErrorNone;

}

static int do_rpbuf_eq_stop(EqPrivateType *priv)
{
	OMX_ERRORTYPE err;
	OMX_STATETYPE state;

	if (priv == NULL || priv->aeq_handle == NULL)
		return OMX_ErrorBadParameter;

	if (!priv->startup_count) {
		aamp_err("startup count already 0\n");
		return OMX_ErrorNone;
	}

	priv->startup_count--;

	if (priv->startup_count == 0) {

		err = OMX_GetState(priv->aeq_handle, &state);
		if(err != OMX_ErrorNone){
			aamp_err("Error in OMX_GetState\n");
			return err;
		}

		if (state == OMX_StateExecuting) {

			omx_sem_down(priv->eofSem);

			/* set component stat to idle */
			err = OMX_SendCommand(priv->aeq_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
			if(err != OMX_ErrorNone){
				aamp_err("Error in send OMX_CommandStateSet OMX_StateIdle\n");
				return err;
			}
			omx_sem_down(priv->aeq_eventSem);
		} else if (state == OMX_StateIdle) {
			aamp_info("Component already idle\n");
		} else {
			aamp_err("Component in the wrong state %d!\n", state);
		}
	}

	return OMX_ErrorNone;

}

static int do_rpbuf_eq_release(EqPrivateType *priv)
{
	OMX_ERRORTYPE err;
	OMX_STATETYPE state;

	if (priv == NULL)
		return OMX_ErrorBadParameter;

	if (priv->aeq_handle) {

		err = OMX_GetState(priv->aeq_handle, &state);
		if(err != OMX_ErrorNone){
			aamp_err("Error in OMX_GetState\n");
			return err;
		}

		if (state == OMX_StateIdle) {
			/* set component stat to loaded */
			err = OMX_SendCommand(priv->aeq_handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
			if(err != OMX_ErrorNone){
				aamp_err("Error in send OMX_CommandStateSet OMX_StateLoaded\n");
				return err;
			}

			/* free buffer */
			free_aeq_buffer(priv, priv->port_filter_in, priv->portin_def.nBufferCountActual);
			free_aeq_buffer(priv, priv->port_filter_out, priv->portout_def.nBufferCountActual);

			omx_sem_down(priv->aeq_eventSem);
		} else if (state == OMX_StateLoaded) {
			aamp_info("Component already Loaded\n");
		} else {
			aamp_err("Component in the wrong state %d!\n", state);
		}
	}

	return OMX_ErrorNone;

}

static void rpbuf_eq_destroy(EqPrivateType *priv)
{
	int eq_ref;
	int cnt = 0;

	priv->state = EQ_CTL_STOP;

	free_aeq_buffer(priv, priv->port_filter_in, priv->portin_def.nBufferCountActual);
	free_aeq_buffer(priv, priv->port_filter_out, priv->portout_def.nBufferCountActual);

	while (1) {
		hal_mutex_lock(priv->ref_mutex);
		eq_ref = priv->eq_ref;
		hal_mutex_unlock(priv->ref_mutex);
		if (eq_ref) {
			if ((++cnt % 10) == 0)
				aamp_info("priv has been used %d times\n", eq_ref);
			hal_msleep(1);
			continue;
		}
		else
			break;
	}

	if (priv->aeq_rb) {
		hal_ringbuffer_release(priv->aeq_rb);
		priv->aeq_rb = NULL;
	}

	if (priv->aeq_out_rb) {
		hal_ringbuffer_release(priv->aeq_out_rb);
		priv->aeq_out_rb = NULL;
	}

	if (priv->send_event) {
		hal_event_delete(priv->send_event);
		priv->send_event = NULL;
	}

	if (priv->event) {
		hal_event_delete(priv->event);
		priv->event = NULL;
	}

	if (priv->thread_sync) {
		hal_sem_delete(priv->thread_sync);
		priv->thread_sync = NULL;
	}

	if (priv->aeq_eventSem) {
		omx_sem_deinit(priv->aeq_eventSem);
		free(priv->aeq_eventSem);
		priv->aeq_eventSem = NULL;
	}

	if (priv->eofSem) {
		omx_sem_deinit(priv->eofSem);
		free(priv->eofSem);
		priv->eofSem = NULL;
	}

	if (priv->pauseSem) {
		omx_sem_deinit(priv->pauseSem);
		free(priv->pauseSem);
		priv->pauseSem = NULL;
	}

	if (priv->aeq_handle) {
		OMX_FreeHandle(priv->aeq_handle);
		priv->aeq_handle = NULL;
	}
	if (priv->BufferQueue) {
		queue_deinit(priv->BufferQueue);
		free(priv->BufferQueue);
		priv->BufferQueue = NULL;
	}

	if (priv->process_task_exit_flag) {
		priv->process_task_exit_flag = 0;
		priv->eq_process_task = NULL;
	}

	if (priv->send_task_exit_flag) {
		priv->send_task_exit_flag = 0;
		priv->eq_send_task = NULL;
	}

}


static int rpbuf_eq_create(EqPrivateType *priv)
{
	int ret = -1;
	int i = 0;
	OMX_ERRORTYPE err = OMX_ErrorNone;
	int frame_bytes = 0;
	eq_config_param_t *eq_config_param = &priv->eq_ctrl_param.eq_config_param;

	if (eq_config_param->bits == 0 || eq_config_param->channels == 0 || eq_config_param->eq_out_channels == 0 ||
		eq_config_param->rate == 0 || eq_config_param->msec == 0) {
		aamp_err("invaild param, bits:%d, channels:%d, eq out channels:%d, rate:%d, msec:%d\n", \
			eq_config_param->bits, eq_config_param->channels, eq_config_param->eq_out_channels, \
			eq_config_param->rate, eq_config_param->msec);
		goto exit;
	}

	frame_bytes = eq_config_param->bits / 8 * eq_config_param->channels;

	priv->rpbuf_in_len = (frame_bytes * eq_config_param->rate * eq_config_param->msec) / 1000; /* 12 * 16 * 16 = 3072 bytes */

	if (priv->eq_ctrl_param.eq_dump)
		priv->rpbuf_out_len = (priv->rpbuf_in_len / eq_config_param->channels) * \
						(eq_config_param->channels + eq_config_param->eq_out_channels); // (3072/ 6)  * 12 (6ch + 6ch) = 6144 byte

	else
		priv->rpbuf_out_len = (priv->rpbuf_in_len / eq_config_param->channels) * (eq_config_param->eq_out_channels); // (3072/ 6)  * 6 (1ch aec)= 3072 byte


	/* init buffer queue */
	priv->BufferQueue = malloc(sizeof(queue_t));
	if(priv->BufferQueue == NULL) {
		aamp_err("Insufficient memory in %s\n", __func__);
		goto exit;
	}
	memset(priv->BufferQueue, 0, sizeof(queue_t));

	ret = queue_init(priv->BufferQueue);
	if (ret != 0) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}

	/* init aeq_eventSem */
	priv->aeq_eventSem = malloc(sizeof(omx_sem_t));
	if (!priv->aeq_eventSem) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}

	ret = omx_sem_init(priv->aeq_eventSem, 0);
	if (ret < 0) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}

	/* init eofSem */
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

	/* init pauseSem */
	priv->pauseSem = malloc(sizeof(omx_sem_t));
	if (!priv->pauseSem) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}

	ret = omx_sem_init(priv->pauseSem, 0);
	if (ret < 0) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}

	priv->aeq_rb = hal_ringbuffer_init(priv->rpbuf_in_len * 4); // 40 * 4 ms
	if (!priv->aeq_rb) {
		aamp_err("create ringbuffer err");
		goto exit;
	}

	priv->aeq_out_rb = hal_ringbuffer_init(priv->rpbuf_out_len * 2); // 40 * 2 ms
	if (!priv->aeq_out_rb) {
		aamp_err("create ringbuffer err");
		goto exit;
	}

	priv->event = hal_event_create();
	if (!priv->event) {
		aamp_err("create event err");
		goto exit;
	}

	priv->send_event = hal_event_create();
	if (!priv->send_event) {
		aamp_err("create event err");
		goto exit;
	}

	priv->thread_sync = hal_sem_create(0);
	if (!priv->thread_sync) {
		aamp_err("hal_sem_create!\n");
		goto exit;
	}

	/* 1. get component handle */
	err = OMX_GetHandle(&priv->aeq_handle, "OMX.audio.equalizer", priv, &eq_untunnel_callbacks);
	if(err != OMX_ErrorNone) {
		aamp_err("Audio equalizer OMX_GetHandle failed\n");
		goto exit;
	}

	priv->port_filter_in = get_port_index(priv->aeq_handle, OMX_DirInput, OMX_PortDomainAudio, 0);
	priv->port_filter_out = get_port_index(priv->aeq_handle, OMX_DirOutput, OMX_PortDomainAudio, 0);
	if (priv->port_filter_in < 0 || priv->port_filter_out < 0) {
		aamp_err("Error in get port index, port_filter_in %d port_filter_out %d\n", priv->port_filter_in, priv->port_filter_out);
		ret = OMX_ErrorBadPortIndex;
		goto exit;
	}

	/* 2. config component */
	err = untunnel_config_aeq_component(priv);
	if(err != OMX_ErrorNone){
		aamp_err("Error in untunnel config aeq component\n");
		goto exit;
	}

	/* 3. set component stat to idle */
	err = OMX_SendCommand(priv->aeq_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
	if(err != OMX_ErrorNone){
		aamp_err("Error in send OMX_CommandStateSet OMX_StateIdle\n");
		goto exit;
	}

	priv->portin_def.nPortIndex = priv->port_filter_in;
	setHeader(&priv->portin_def, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	err = OMX_GetParameter(priv->aeq_handle, OMX_IndexParamPortDefinition, &priv->portin_def);
	if (err != OMX_ErrorNone) {
		aamp_err("Error when getting OMX_PORT_PARAM_TYPE,%x\n", err);
		goto exit;
	}

	if ((priv->aeq_port_para[priv->port_filter_in].nBufferSize != priv->portin_def.nBufferSize) ||
		(priv->aeq_port_para[priv->port_filter_in].nBufferCountActual != priv->portin_def.nBufferCountActual)) {
		aamp_err("Error port nBufferSize %ld port nBufferCountActual %ld nBufferSize %ld nBufferCountActual %ld\n", \
			priv->aeq_port_para[priv->port_filter_in].nBufferSize, priv->aeq_port_para[priv->port_filter_in].nBufferCountActual , \
			priv->portin_def.nBufferSize, priv->portin_def.nBufferCountActual);
		goto exit;
	}
	err = alloc_aeq_buffer(priv, priv->port_filter_in, priv->portin_def.nBufferCountActual, priv->portin_def.nBufferSize);
	if (err != OMX_ErrorNone) {
		aamp_err("Error when alloc_buffer,%x\n", err);
		goto exit;
	}

	for (i = 0; i < priv->portin_def.nBufferCountActual; i++) {
		ret = queue(priv->BufferQueue, priv->bufferin[i]);
		if (ret != 0) {
			aamp_err("queue buffer %d error!\n", i);
			goto exit;
		}
	}
	aamp_debug("queue num is %d\n", getquenelem(priv->BufferQueue));

	priv->portout_def.nPortIndex = priv->port_filter_out;
	setHeader(&priv->portout_def, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	err = OMX_GetParameter(priv->aeq_handle, OMX_IndexParamPortDefinition, &priv->portout_def);
	if (err != OMX_ErrorNone) {
		aamp_err("Error when getting OMX_PORT_PARAM_TYPE,%x\n", err);
		goto exit;
	}

	if ((priv->aeq_port_para[priv->port_filter_out].nBufferSize != priv->portout_def.nBufferSize) ||
		(priv->aeq_port_para[priv->port_filter_out].nBufferCountActual != priv->portout_def.nBufferCountActual)) {
		aamp_err("Error port nBufferSize %ld port nBufferCountActual %ld nBufferSize %ld nBufferCountActual %ld\n", \
			priv->aeq_port_para[priv->port_filter_out].nBufferSize, priv->aeq_port_para[priv->port_filter_out].nBufferCountActual , \
			priv->portout_def.nBufferSize, priv->portout_def.nBufferCountActual);
		goto exit;
	}

	err = alloc_aeq_buffer(priv, priv->port_filter_out, priv->portout_def.nBufferCountActual, priv->portout_def.nBufferSize);
	if (err != OMX_ErrorNone) {
		aamp_err("Error when alloc_buffer,%x\n", err);
		goto exit;
	}
	omx_sem_down(priv->aeq_eventSem);

	/* 4. send buffer to source component queue */
	for (i = 0; i < priv->portout_def.nBufferCountActual; i++) {
		err = OMX_FillThisBuffer(priv->aeq_handle, priv->bufferout[i]);
		aamp_debug("OMX_FillThisBuffer %p on port %lu\n", priv->bufferout[i],
			priv->bufferout[i]->nOutputPortIndex);
		if(err != OMX_ErrorNone){
			aamp_err("Error in send OMX_FillThisBuffer\n");
			goto exit;
		}
	}

	aamp_info("audio eq %d creat finished\n", priv->index);

	return OMX_ErrorNone;

exit:

	rpbuf_eq_destroy(priv);

	return err;

}

static void rpbuf_eq_process(void *arg)
{
	EqPrivateType *priv = (EqPrivateType *)arg;
	int ret;
	eq_queue_item item;
	rpbuf_audio_remote_ctrl_eq_t rpbuf_eq_ctl_arg;

	while (1) {

		if (priv->keep_alive & EQ_PROCESS_DESTROY_BIT)
			goto exit;

		ret = hal_queue_recv(priv->eq_queue, &item, HAL_WAIT_FOREVER);
		if (ret != 0) {
			aamp_err("Failed to recv queue\n");
			goto exit;
		}

		aamp_info("eq %d receive cmd:%d\n", priv->index, item.cmd);

		switch (item.cmd) {

		case RPBUF_CTL_CONFIG:
			if (item.arg == NULL || item.len != sizeof(eq_config_param_t)) {
				aamp_err("audio rpbuf ctrl config param is invaild %d \n", item.len);
				break;
			}
			memcpy(&priv->eq_ctrl_param.eq_config_param ,(eq_config_param_t *)item.arg, sizeof(eq_config_param_t));
			if (priv->config_flag == 0) {
				ret = rpbuf_eq_create(priv);
				if (ret != 0) {
					aamp_err("rpbuf_eq_create failed \n");
				}
				priv->config_flag = 1;
			} else {
				ret = do_rpbuf_eq_config(priv);
				if (ret != 0) {
					aamp_err("do_rpbuf_eq_config failed \n");
				}
			}
			break;
		case RPBUF_CTL_START:
			ret = do_rpbuf_eq_start(priv);
			if (ret != 0) {
				aamp_err("do_rpdata_eq_start failed \n");
			}
			priv->state = EQ_CTL_START;

			memset(&rpbuf_eq_ctl_arg, 0, sizeof(rpbuf_audio_remote_ctrl_eq_t));
			rpbuf_eq_ctl_arg.cmd = RPBUF_CTL_START_ACK;
			rpbuf_eq_ctl_arg.type = RPBUF_ALGO_EQ;
			if (ret != 0)
				rpbuf_eq_ctl_arg.value = RPBUF_ERROR_START_FAIL;

			ret = rpbuf_audio_ctrl_eq_send_cmd(priv->eq_ctrl_handle, &rpbuf_eq_ctl_arg, 0);
			if (ret != 0) {
				aamp_err("rpbuf_audio_ctrl_eq_send_cmd failed");
			}
			aamp_info("eq start ack");
			break;
		case RPBUF_CTL_PAUSE:
			ret = do_rpbuf_eq_pause(priv);
			if (ret != 0) {
				aamp_err("do_rpbuf_eq_pause failed \n");
			}
			priv->paused = 1;
			priv->state = EQ_CTL_PAUSE;
			break;
		case RPBUF_CTL_RESUME:
			ret = do_rpbuf_eq_resume(priv);
			if (ret != 0) {
				aamp_err("do_rpbuf_eq_pause failed \n");
			}
			priv->paused = 0;
			omx_sem_signal(priv->pauseSem);
			priv->state = EQ_CTL_RESUME;
			break;
		case RPBUF_CTL_STOP:
			ret = do_rpbuf_eq_stop(priv);
			if (ret != 0) {
				aamp_err("do_rpdata_eq_stop failed \n");
			}
			ret = do_rpbuf_eq_release(priv);
			if (ret != 0) {
				aamp_err("do_rpbuf_eq_release failed \n");
			}
			rpbuf_eq_destroy(priv);
			priv->config_flag = 0;
			break;
		case RPBUF_CTL_RELEASE:
			priv->keep_alive |= EQ_PROCESS_DESTROY_BIT;
			rpbuf_eq_transfer_destroy(priv);
			break;
		default:
			aamp_err("unknown cmd:%d\n", item.cmd);
			break;
		}
	}

exit:

	hal_sem_post(priv->thread_release);

	hal_thread_stop(priv->eq_task);

}

hal_queue_t eq_get_queue_handle(void *private)
{
	EqPrivateType* priv = (EqPrivateType*)private;

	if (priv == NULL) {
		printf("task create failed");
		return NULL;
	}

	return priv->eq_queue;

}

void rpbuf_eq_transfer_destroy(void *private)
{
	EqPrivateType* priv = (EqPrivateType*)private;
	int ret;

	ret = rpbuf_common_destroy(priv->rpbuf_transfer_in_name[priv->index]);
	if (ret < 0) {
		aamp_err("rpbuf_destroy for name %s (len: %d) failed\n", priv->rpbuf_transfer_in_name[priv->index], RPBUF_EQ_IN_LENGTH);
	}

	ret = rpbuf_common_destroy(priv->rpbuf_transfer_out_name[priv->index]);
	if (ret < 0) {
		aamp_err("rpbuf_destroy for name %s (len: %d) failed\n", priv->rpbuf_transfer_out_name[priv->index], RPBUF_EQ_OUT_LENGTH);
	}

}

int rpbuf_eq_transfer_create(void *private)
{
	EqPrivateType* priv = (EqPrivateType*)private;
	int ret;
	int index = priv->index;

	snprintf(priv->rpbuf_transfer_in_name[index], sizeof(priv->rpbuf_transfer_in_name[index]), "%s_%d", RPBUF_EQ_IN_NAME, index);

	ret = rpbuf_common_create(RPBUF_CTRL_ID, priv->rpbuf_transfer_in_name[index], RPBUF_EQ_IN_LENGTH, 1, &rpbuf_eq_in_cbs, priv);
	if (ret < 0) {
		 aamp_err("rpbuf_create for name %s (len: %d) failed\n", priv->rpbuf_transfer_in_name[index], RPBUF_EQ_IN_LENGTH);
		 goto exit;
	}

	while(!rpbuf_common_is_available(priv->rpbuf_transfer_in_name[index])) {
		hal_msleep(5);
	}
	aamp_info("rpbuf %s ready\n", priv->rpbuf_transfer_in_name[index]);


	snprintf(priv->rpbuf_transfer_out_name[index], sizeof(priv->rpbuf_transfer_out_name[index]), "%s_%d", RPBUF_EQ_OUT_NAME, index);

	ret = rpbuf_common_create(RPBUF_CTRL_ID, priv->rpbuf_transfer_out_name[index], RPBUF_EQ_OUT_LENGTH, 1, &rpbuf_eq_out_cbs, priv);
	if (ret < 0) {
		 aamp_err("rpbuf_create for name %s (len: %d) failed\n", priv->rpbuf_transfer_out_name[index], RPBUF_EQ_OUT_LENGTH);
		 goto exit;
	}

	while (!rpbuf_common_is_available(priv->rpbuf_transfer_out_name[index])) {
		hal_msleep(5);
	}
	aamp_info("rpbuf %s ready\n", priv->rpbuf_transfer_out_name[index]);

	return 0;

exit:

	rpbuf_eq_transfer_destroy(priv);

	return ret;
}

void eq_ctrl_destroy(void *private)
{
	EqPrivateType* priv = (EqPrivateType*)private;

	priv->state = EQ_CTL_RELEASE;

	hal_sem_wait(priv->thread_release);

	OMX_Deinit();

	if (priv->eq_queue) {
		hal_queue_delete(priv->eq_queue);
		priv->eq_queue = NULL;
	}

	if (priv->thread_release) {
		hal_sem_delete(priv->thread_release);
		priv->thread_release = NULL;
	}

	if (priv->ref_mutex) {
		hal_mutex_delete(priv->ref_mutex);
		priv->ref_mutex = NULL;
	}

	if (priv) {
		free(priv);
		priv = NULL;
	}
}

void* eq_ctrl_init(int index, void *audio_ctrl_eq)
{
	int ret = 0;
	EqPrivateType* priv = NULL;
	char name[32];

	/* Initialize application private data */
	priv = malloc(sizeof(EqPrivateType));
	if (!priv) {
		aamp_err("[%s] line:%d \n", __func__, __LINE__);
		goto exit;
	}
	memset(priv, 0, sizeof(EqPrivateType));

	priv->eq_queue = hal_queue_create("eq_ctrl", sizeof(eq_queue_item), EQ_PROCESS_QUEUE);
	if (!priv->eq_queue) {
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

	priv->eq_ctrl_param.eq_enable = g_eq_enable[index];
	priv->eq_ctrl_param.eq_dump = g_eq_dump_merge[index];
	priv->eq_ctrl_handle = audio_ctrl_eq;

	priv->state = EQ_CTL_INIT;
	g_eq_control[index].private = (void *)priv;
	g_eq_index = index;

	ret = OMX_Init();
	if(ret != OMX_ErrorNone) {
		aamp_err("OMX_Init() failed\n");
		goto exit;
	}

	snprintf(name, sizeof(name), "rpbuf_eq_process_%d", index);

	priv->eq_task = hal_thread_create(rpbuf_eq_process, priv, name, 8192,
			configAPPLICATION_AAMP_PRIORITY);

	if (priv->eq_task == NULL) {
		aamp_err("task create failed");
		goto exit;
	}
	hal_thread_start(priv->eq_task);

	priv->index = index;

	return priv;
exit:

	OMX_Deinit();

	priv->state = 0;
	g_eq_control[index].private = NULL;
	g_eq_index = 0;

	if (priv->eq_queue) {
		hal_queue_delete(priv->eq_queue);
		priv->eq_queue = NULL;
	}

	if (priv->thread_release) {
		hal_sem_delete(priv->thread_release);
		priv->thread_release = NULL;
	}

	if (priv->ref_mutex) {
		hal_mutex_delete(priv->ref_mutex);
		priv->ref_mutex = NULL;
	}

	if (priv) {
		free(priv);
		priv = NULL;
	}
	return NULL;
}


