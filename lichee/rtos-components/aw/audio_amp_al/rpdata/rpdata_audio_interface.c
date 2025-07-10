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
#include <hal_mutex.h>
#include <hal_mem.h>

#include "rpdata_audio_interface.h"


/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: rpdata_audio_create
 *
 * Description:
 *   Initialize the audio rpdata for RV,m33,dsp
 *
 * Input Parameters:
 *   rpdata_audio_param    - The param of type and name must be same with local and remote.
 *                           if local is dsp and remote is RV, they are type and name must be same. 
 *                         - dir: if this call by dsp, and want communicate with RV, the dir must be RPDATA_DIR_RV.
 *   rpdata_audio_cbs      - Only Support for Recv core. Send Core is null.
 *   len                   - Only Support for Send core. Recv Core is 0.
 *   ringbuffer_len        - Only Support for Recv Core. Send core is 0
 *   priv        		   - Private point
 * Returned Value:
 *   A new  rpdata_audio_entry point is returned on
 *   success; NULL is returned on failure.
 *
 ****************************************************************************/


struct rpdata_audio_entry * rpdata_audio_create(rpdata_audio_param_t *rpdata_audio_param, struct rpdata_cbs *rpdata_audio_cbs,
														size_t len, size_t ringbuffer_len, void *priv)
{
	struct rpdata_audio_entry *rpdata_entry = NULL;
	int ret = 0;

	rpdata_entry = hal_malloc(sizeof(*rpdata_entry));
	if (rpdata_entry == NULL) {
		printf("failed to alloc rpdata audio entry\r\n");
		goto err_out;
	}
	memset(rpdata_entry, 0, sizeof(*rpdata_entry));


	if (len) {
		rpdata_entry->rpd_hld = rpdata_create(rpdata_audio_param->dir, rpdata_audio_param->type, rpdata_audio_param->name, len);
		if (!rpdata_entry->rpd_hld) {
			printf("rpdata create failed!\n");
			goto err_out;
		}

		rpdata_entry->rpd_buffer = rpdata_buffer_addr(rpdata_entry->rpd_hld);
		if (!rpdata_entry->rpd_buffer) {
			printf("rpdata buffer addr failed!\n");
			goto err_out;
		}
	}
	else if (rpdata_audio_cbs)
	{
		rpdata_entry->rpd_hld = rpdata_connect(rpdata_audio_param->dir, rpdata_audio_param->type, rpdata_audio_param->name);
		if (!rpdata_entry->rpd_hld) {
			printf("rpdata connect failed");
			goto err_out;
		}

		rpdata_entry->rpd_buffer = rpdata_buffer_addr(rpdata_entry->rpd_hld);
		if (!rpdata_entry->rpd_buffer) {
			printf("rpdata buffer addr failed");
			goto err_out;
		}

		rpdata_set_recv_cb(rpdata_entry->rpd_hld, rpdata_audio_cbs);
	}
	else if (ringbuffer_len)
	{
		rpdata_entry->rpd_hld = rpdata_connect(rpdata_audio_param->dir, rpdata_audio_param->type, rpdata_audio_param->name);
		if (!rpdata_entry->rpd_hld) {
			printf("rpdata connect failed");
			goto err_out;
		}

		rpdata_entry->rpd_buffer = rpdata_buffer_addr(rpdata_entry->rpd_hld);
		if (!rpdata_entry->rpd_buffer) {
			printf("rpdata buffer addr failed");
			goto err_out;
		}

		ret = rpdata_set_recv_ringbuffer(rpdata_entry->rpd_hld, ringbuffer_len);
		if (ret != 0) {
			printf("rpdata_set_recv_ringbuffer failed");
			goto err_out;
		}
	}
	else
	{
		printf("rpdata buffer param invalid");
		goto err_out;
	}
	rpdata_set_private_data(rpdata_entry->rpd_hld, priv);


	return rpdata_entry;

err_out:

	if (rpdata_entry->rpd_hld) {
		rpdata_destroy(rpdata_entry->rpd_hld);
		rpdata_entry->rpd_hld = NULL;
		rpdata_entry->rpd_buffer = NULL;
	}

	if (rpdata_entry)
		hal_free(rpdata_entry);

	return NULL;
}

void rpdata_audio_destroy(struct rpdata_audio_entry *rpdata_entry)
{

	if (rpdata_entry == NULL)
		return;

	rpdata_destroy(rpdata_entry->rpd_hld);
	rpdata_entry->rpd_hld = NULL;
	rpdata_entry->rpd_buffer = NULL;
	hal_free(rpdata_entry);
	return;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: rpdata_audio_send
 *
 * Description:
 *   Send data to remote core
 *
 * Input Parameters:
 *   rpdata_audio_entry    - The handle of rpdata_audio_entry.
 *   data                  - The buffer of data.
 *   len                   - The len of data. The len must be less than the size of rpdata_audio_create.
 *
 * Note:
 *   This function is only Support for Send Core.
 * Returned Value:
 *   0 - Pass.
 *   -1 or than - failure.
 *
 ****************************************************************************/

int rpdata_audio_send(struct rpdata_audio_entry *rpdata_entry, void *data, size_t len)
{
	int ret = 0;

	if (rpdata_entry == NULL) {
		printf("rpdata audio entry is NUll\n");
		return -1;
	}

	if (rpdata_is_connect(rpdata_entry->rpd_hld) != 0) {
		printf("rpdata send is not connect, so waiting connecting...\n");
		rpdata_wait_connect(rpdata_entry->rpd_hld);
		printf("rpdata send is ready\n");
	}

	memcpy(rpdata_entry->rpd_buffer, data, len);

	ret = rpdata_send(rpdata_entry->rpd_hld, 0, len);
	if (ret != 0) {
		printf("rpdata send failed\n");
		return ret;
	}

	return ret;
}


/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: rpdata_audio_recv
 *
 * Description:
 *   Recv data from remote core. Only Support rpdata_audio_create config ringbuffer_len.
 *
 * Input Parameters:
 *   rpdata_audio_entry    - The handle of rpdata_audio_entry.
 *   data                  - The buffer of data want to recv from remote.
 *   len                   - The len of data want to have.
 *   timeout               - The wait time to have data, unit is ms.
 *
 * Note:
 *   This function is only Support for Send Core.
 * Returned Value:
 *  The bytes get from remote actually.
 *
 ****************************************************************************/

int rpdata_audio_recv(struct rpdata_audio_entry *rpdata_entry, void *data, size_t len, int timeout)
{
	int ret = 0;

	if (rpdata_entry == NULL) {
		printf("rpdata audio entry is NUll");
		return 0;
	}

	if (rpdata_is_connect(rpdata_entry->rpd_hld) != 0) {
		printf("rpdata recv is not connect, so waiting connecting...\n");
		rpdata_wait_connect(rpdata_entry->rpd_hld);
		printf("rpdata recv is ready\n");
	}


	memcpy(rpdata_entry->rpd_buffer, data, len);

	ret = rpdata_recv(rpdata_entry->rpd_hld, data, len, timeout);
	if (ret != len) {
		printf("recv buffer len  %d is not match to want len %d\n", ret, len);
		return ret;
	}

	return ret;
}

