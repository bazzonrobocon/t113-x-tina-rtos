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

#include <hal_mem.h>
#include <hal_sem.h>
#include <hal_log.h>
#include <hal_cache.h>
#include <hal_cmd.h>
#include <hal_dma.h>
#include <hal_timer.h>
#include <sunxi_hal_gpadc_ng.h>

#define BUF_LEN 128
static hal_sem_t g_sem = NULL;
int channel = -1;

static void dma_cb(void *param)
{
	hal_sem_post(g_sem);
}

int sunxi_gpadc_irq_callback(uint32_t dada_type, uint32_t data)
{
	int vol_data;
	data = ((VOL_RANGE / 4096) * data);
	vol_data = data / 1000;
	printf("channel %d vol data: %d\n", channel, vol_data);
	//hal_gpadc_channel_exit(channel);
	//hal_gpadc_deinit();
	return 0;
}

int sunxi_gpadc_test(int argc, char **argv)
{
	int ret = -1;
	uint32_t vol_data;
	uint32_t port;

	printf("Run gpadc test\n");

	if (argc < 3)
	{
		hal_log_err("usage: hal_gpadc_ng [port] [channel]\n");
		return -1;
	}

	port = strtol(argv[1], NULL, 0);
	ret = hal_gpadc_init(port);
	if (ret) {
		hal_log_err("gpadc init failed!\n");
		return -1;
	}

	channel = strtol(argv[2], NULL, 0);

	if (channel < 0 || channel > CHANNEL_NUM)
	{
		hal_log_err("channel %d is wrong, must between 0 and %d\n", channel, CHANNEL_NUM);
		return -1;
	}
	hal_gpadc_channel_init(port, channel, 0);
	//hal_gpadc_register_callback(channel, sunxi_gpadc_irq_callback);

	vol_data = gpadc_read_channel_data(port, channel);
	printf("GPADC[%d]:channel %d vol data is %u\n", port, channel, vol_data);

	hal_gpadc_channel_exit(port, channel, 0);
	hal_gpadc_deinit(port);

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(sunxi_gpadc_test, hal_gpadc_ng, gpadc hal APIs tests)

int cmd_gpadc_dma_transmission(int argc, char **argv)
{
	int ret = -1;
	int cnt = 0;
	uint32_t vol_data;
	uint32_t port;
	struct dma_slave_config config;
	struct sunxi_dma_chan *hdma;
	uint8_t *buf;
	uint16_t *data;

	if (argc < 3)
	{
		hal_log_err("usage: hal_gpadc_ng [port] [channel]\n");
		goto err0;
	}

	g_sem = hal_sem_create(0);
	if (!g_sem) {
		printf("Create semaphore failed!\n");
		goto err0;
	}

	port = strtol(argv[1], NULL, 0);
	ret = hal_gpadc_init(port);
	if (ret) {
		hal_log_err("gpadc init failed!\n");
		goto err1;
	}

	channel = strtol(argv[2], NULL, 0);
	if (channel < 0 || channel > CHANNEL_NUM)
	{
		hal_log_err("channel %d is wrong, must between 0 and %d\n", channel, CHANNEL_NUM);
		goto err1;
	}
	ret = hal_gpadc_channel_init(port, channel, 0);
	if (ret) {
		hal_log_err("gpadc channel init failed!\n");
		goto err2;
	}

	hal_gpadc_fifo_config(port);

	buf = hal_malloc_align(BUF_LEN, 64);
	if (!buf) {
		printf("%s malloc buf fail\n", __func__);
		goto err3;
	}
	memset(buf, 0, BUF_LEN);

	/* config DMA */
	/* request dma chan */
	ret = hal_dma_chan_request(&hdma);
	if (ret == HAL_DMA_CHAN_STATUS_BUSY) {
		printf("gpadc dma request dma failed\n");
		goto err4;
	}

	/* register dma callback */
	ret = hal_dma_callback_install(hdma, dma_cb, hdma);
	if (ret != HAL_DMA_STATUS_OK) {
		printf("register dma callback failed!\n");
		goto err4;
	}

	memset(&config, 0, sizeof(config));
	config.direction = DMA_DEV_TO_MEM;
	config.dst_addr = (uint32_t)buf;
	config.src_addr = (uint32_t)(GPADC_BASE0 + GP_FIFO_DATA_REG);
	config.dst_addr_width = DMA_SLAVE_BUSWIDTH_8_BYTES;
	config.src_addr_width = DMA_SLAVE_BUSWIDTH_8_BYTES;
	config.dst_maxburst = DMA_SLAVE_BURST_4;
	config.src_maxburst = DMA_SLAVE_BURST_4;
	config.slave_id = sunxi_slave_id(DRQDST_SDRAM, DRQSRC_GPADC);
	ret = hal_dma_slave_config(hdma, &config);
	if (ret != HAL_DMA_STATUS_OK) {
		printf("dma config error,ret:%d\n", ret);
		goto err4;
	}

	ret = hal_dma_prep_device(hdma, config.dst_addr, config.src_addr, BUF_LEN, DMA_DEV_TO_MEM);
	if (ret != HAL_DMA_STATUS_OK) {
		printf("%s hal_dma_prep_cyclic failed\n", __func__);
		goto err4;
	}

	hal_dcache_clean_invalidate((unsigned long)buf, (unsigned long)BUF_LEN);
	ret = hal_dma_start(hdma);
	if (ret != HAL_DMA_STATUS_OK) {
		printf("%s dma start error! return %d\n", __func__, ret);
		goto err5;
	}

	data = (uint16_t *)buf;

	hal_sem_wait(g_sem);
	for(cnt = 0; cnt < BUF_LEN / 2; cnt++){
		vol_data = 1800 * data[cnt] / 4095;
		printf("vol is %d mv\n", vol_data);
	}

	printf("gpadc xfer by dma success!\n");

err5:
	hal_dma_stop(hdma);
err4:
	hal_dma_chan_free(hdma);
err3:
	hal_free_align(buf);
err2:
	hal_gpadc_channel_exit(port, channel, 0);
err1:
	hal_gpadc_deinit(port);
err0:

	return 0;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_gpadc_dma_transmission, hal_gpadc_ng_dma_transmission, gpadc hal APIs tests)
