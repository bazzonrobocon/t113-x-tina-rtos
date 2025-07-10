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


#ifndef __RPDATA_AUDIO_CRTL_ASR_H__
#define __RPDATA_AUDIO_CRTL_ASR_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "rpdata_audio_ctrl.h"

// RV to DSP 命令通道
#define FRONT_RPDATA_CTL_DIR_DSP			(RPDATA_DIR_DSP)
#define FRONT_RPDATA_CTL_DIR_RV			    (RPDATA_DIR_RV)
#define FRONT_RPDATA_CTL_TYPE				("RVtoDSPFctl")
#define FRONT_RPDATA_CTL_NAME				("RVtoDSPFctlcb")
// DSP to RV 数据通道
#define FRONT_RPDATA_DATA_DIR_DSP			(RPDATA_DIR_DSP)
#define FRONT_RPDATA_DATA_DIR_RV			(RPDATA_DIR_RV)
#define FRONT_RPDATA_DATA_TYPE				("DSPtoRVAsr")
#define FRONT_RPDATA_DATA_NAME				("RVrecvDSPsend")

#ifdef CONFIG_COMPONENTS_RPDATA_CRTL_ASR_CNT
#define RPDATA_CRTL_ASR_COUNTS		CONFIG_COMPONENTS_RPDATA_CRTL_ASR_CNT
#else
#define RPDATA_CRTL_ASR_COUNTS		1
#endif

#define ASR_MSG_SIZE  (100)

typedef struct speech_status {
    int size;
    bool is_online; //false 代表离线 true 代表在线
    int cur_time_hour; //实时0~23小时
    int log_level; //串口输入打印dsp level，合法值0~3，对于其他值，DSP应视作无效
    int awake_threshold_type; //串口输入标志，传输 0~4(5个数值)，0：阈值无效，1：热阈值，2：冷阈值，3：打断阈值，4：深夜阈值
    int awake_threshold; 
} speech_status_t;

typedef struct wakeup_info {
    int size;
    int wakeup_ret;
    float score;
    char asr_msg[ASR_MSG_SIZE];
    int asr_msg_len;
    float threshold;
    int wake_up_cnt;
} wakeup_info_t;

typedef struct rpdata_audio_ctrl_asr {
	uint32_t cmd;
	uint32_t type;              /* algo type */
	uint32_t value;
	uint32_t rate;
	uint32_t msec;
	uint8_t channels;           /* record chans */
	uint8_t afe_out_channels;   /* afe output chans */
	uint8_t  bits;
	speech_status_t speech_status;
} rpdata_audio_ctrl_asr_t;


#endif
