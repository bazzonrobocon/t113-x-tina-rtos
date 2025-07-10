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
#ifndef __AUDIO_AMP_LOG_H
#define __AUDIO_AMP_LOG_H

#include <assert.h>
#include <aw_common.h>
#include <aw_list.h>
#include <errno.h>


#define AUDIO_AMP_VERSION "AAMP-V0.0.1"

#ifndef AAMP_TAG
#define AAMP_TAG	"AAMP"
#endif

#ifndef unlikely
#define unlikely(x)             __builtin_expect ((x), 0)
#endif

#define LOG_COLOR_NONE		"\e[0m"
#define LOG_COLOR_GREEN		"\e[32m"
#define LOG_COLOR_BLUE		"\e[34m"
#define LOG_COLOR_RED		"\e[31m"


#if defined(CONFIG_ARCH_ARM_CORTEX_A7)
#define AAMP_CORE "A7-"
#elif defined(CONFIG_ARCH_ARM_CORTEX_M33)
#define AAMP_CORE "M33-"
#elif defined(CONFIG_ARCH_RISCV)
#define AAMP_CORE "RV-"
#elif defined(CONFIG_ARCH_DSP)
#define AAMP_CORE "DSP-"
#else
#define AAMP_CORE ""
#endif

extern int g_audio_amp_debug_mask;
#define aamp_debug(fmt, args...) \
do { \
	if (unlikely(g_audio_amp_debug_mask)) \
		printf(LOG_COLOR_GREEN "[%s%s-DBG][%s](%d) " fmt "\n" \
			LOG_COLOR_NONE, AAMP_CORE, AAMP_TAG, __func__, __LINE__, ##args); \
} while (0)


#define aamp_info(fmt, args...)	\
    printf(LOG_COLOR_BLUE "[%s%s-INF][%s](%d) " fmt "\n" LOG_COLOR_NONE, \
			AAMP_CORE, AAMP_TAG, __func__, __LINE__, ##args)

#define aamp_err(fmt, args...)	\
    printf(LOG_COLOR_RED "[%s%s-ERR][%s](%d) " fmt "\n" LOG_COLOR_NONE, \
			AAMP_CORE, AAMP_TAG, __func__, __LINE__, ##args)

#if 1
#define aamp_fatal(msg) \
do {\
	printf("[%s%s-FATAL][%s](%d) %s \n", AAMP_CORE, AAMP_TAG, __func__, __LINE__,msg);\
	assert(0);\
} while (0)
#else
#define fatal(msg) \
	assert(0);
#endif

#ifndef configAPPLICATION_AAMP_PRIORITY
#define configAPPLICATION_AAMP_PRIORITY	(configMAX_PRIORITIES > 20 ? configMAX_PRIORITIES - 8 : configMAX_PRIORITIES - 3)
#endif
#define configAPPLICATION_AAMP_HIGH_PRIORITY	(configAPPLICATION_AAMP_PRIORITY + 2)


#define AS_MAYBE_UNUSED(v) 	(void)(v)

#endif /* __AUDIO_AMP_LOG_H */
