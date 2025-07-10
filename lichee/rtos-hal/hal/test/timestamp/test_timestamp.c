/*
 * Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 *
 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the the people's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.
 *
 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY'S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS'SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY'S TECHNOLOGY.
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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <hal_timer.h>
#include <hal_log.h>
#include <hal_cmd.h>
#include <sunxi_hal_timestamp.h>
#include <timestamp/platform_timestamp.h>

int cmd_test_timestamp(int argc, char **argv)
{
	int delay_s = 0;
	if (argc >= 2) {
		if (strcmp("-h", argv[1]) == 0) {
			printf("usage: hal_timestamp <delay_s>\n");
			return 0;
		}
	}
	if (argc > 1 && atoi(argv[1]) > 0)
		delay_s = atoi(argv[1]);

	hal_timestamp_init();
	hal_timestamp_printf_runtime();
	if (delay_s > 0) {
		hal_sleep(delay_s);
		hal_timestamp_printf_runtime();
	}

	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_test_timestamp, hal_timestamp, timestamp hal APIs tests)

int cmd_test_get_timestamp_data(int argc, char **argv)
{
	unsigned long timestamp_data;
	if (argc >= 2) {
		if (strcmp("-h", argv[1]) == 0) {
			printf("usage: hal_timestamp_data\n");
			return 0;
		}
	}

	hal_timestamp_init();
	timestamp_data= hal_timestamp_read();
	printf("the timestamp data is %ld",timestamp_data);
	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_test_get_timestamp_data, hal_timestamp_data, get timestamp counting value APIs tests)
