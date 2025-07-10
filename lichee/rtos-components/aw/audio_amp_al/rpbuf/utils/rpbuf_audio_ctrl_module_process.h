/**

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


#ifndef __RPBUF_AUDIO_CRTL_MODULE_PROCESS_H__
#define __RPBUF_AUDIO_CRTL_MODULE_PROCESS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "rpbuf_audio_ctrl.h"

// A7 to DSP 命令通道
#define RPBUF_CTL_AMP_NAME			("Rpbuf_Ctrl_AMP_DSP")

// A7 to DSP 数据通道
#define RPBUF_AMP_IN_NAME			("Rpbuf_AMP_DATA")

// DSP to A7 数据通道
#define RPBUF_AMP_OUT_NAME			("Rpbuf_AMP_OUT_DATA")

#define RPBUF_AMP_IN_LENGTH			(16 * 1024)

#define RPBUF_AMP_OUT_LENGTH		(16 * 1024)

#define RPBUF_AMP_CONFIG_LENGTH		(2048)

#ifdef CONFIG_COMPONENTS_RPBUF_CRTL_MODULE_PROCESS_CNT
#define RPBUF_CRTL_AMP_COUNTS		CONFIG_COMPONENTS_RPBUF_CRTL_MODULE_PROCESS_CNT
#else
#define RPBUF_CRTL_AMP_COUNTS		1
#endif

typedef struct rpbuf_audio_local_ctrl_amp {
	uint32_t cmd;
	uint32_t type;              /* algo type */
	uint32_t value;
	uint32_t rate;
	uint32_t msec;
	uint32_t channels;           /* in chans */
	uint32_t out_channels;       /* algo output chans */
	uint32_t  bits;
	void *ampPrmsTransfer;
	uint32_t ampPrmslen;
	uint32_t smooth_time;
} rpbuf_audio_local_ctrl_amp_t;

typedef struct rpbuf_audio_remote_ctrl_amp {
	uint32_t cmd;
	uint32_t type;              /* alog type */
	uint32_t value;
	uint32_t rate;
	uint32_t msec;
	uint32_t channels;           /* record chans */
	uint32_t out_channels;       /* output chans */
	uint32_t bits;
	uint8_t gsound_buf[RPBUF_AMP_CONFIG_LENGTH];
	uint32_t gsound_len;
	uint32_t smooth_time;
} rpbuf_audio_remote_ctrl_amp_t;

#endif
