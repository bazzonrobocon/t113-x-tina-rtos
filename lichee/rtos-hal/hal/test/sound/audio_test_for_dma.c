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
#include <string.h>

#include <hal_log.h>
#include <hal_cmd.h>
#include <hal_mem.h>
#include <hal_cache.h>
#include <hal_dma.h>
#include <hal_timer.h>

#include <sunxi_hal_common.h>

#include "audio_test_for_dma.h"

#define I2S_BUF_LEN	64 * 1024
#define	BUF_LEN		8 * 1024
#define PERIOD_LEN	1024

int test_cnt = 10;
#define reg_write(reg, value)			hal_writel(value, reg)

void reg_update_bits(unsigned int reg, unsigned int mask,
		    unsigned int value)
{
	unsigned int reg_val;

	reg_val = hal_readl(reg);
	reg_val = (reg_val & ~mask) | (value & mask);
	hal_writel(reg_val, reg);
}


/*
 * Note:
 * different platform should modify clk
 */
void i2s_clk(void)
{
	/* pllaudio_div2----22.5792*/
	reg_write(0x02001C70, 0x83000001);
	reg_write(0x07102120, 0x80000020);

	reg_write(0x07102020, 0x80000003);
	reg_write(0x07102104, 0x00010001);
	reg_write(0x0710211c, 0x00000003);

	reg_write(0x07102010, 0xA000A234);
	reg_write(0x0710200c, 0xE8415900);

	reg_write(0x0710202c, 0x81000163);
	reg_write(0x07102040, 0x10001);
}

void i2s_init(void)
{
	//daudio_init
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FMT0,
				(1 << LRCK_WIDTH),
				(0x0 << LRCK_WIDTH));

	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FMT0,
				(SUNXI_DAUDIO_LRCK_PERIOD_MASK) << LRCK_PERIOD,
				(63 << LRCK_PERIOD));
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FMT0,
				(SUNXI_DAUDIO_SLOT_WIDTH_MASK) << SLOT_WIDTH,
				(0x7 << SLOT_WIDTH));

	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FMT1,
				(0x1 << TX_MLS),
				(0x0 << TX_MLS));
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FMT1,
				(0x1 << RX_MLS),
				(0x0 << RX_MLS));
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FMT1,
				(0x3 << SEXT),
				(0x0 << SEXT));
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FMT1,
				(0x3 << TX_PDM),
				(0x0 << TX_PDM));
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FMT1,
				(0x3 << RX_PDM),
				(0x0 << RX_PDM));

	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_CTL,
				 1 <<  RX_SYNC_EN, 0 << RX_SYNC_EN);

	/* mclk setting */
	// switch (param->mclk_div) {
	// case 1:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_1;
	// 	break;
	// case 2:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_2;
	// 	break;
	// case 4:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_3;
	// 	break;
	// case 6:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_4;
	// 	break;
	// case 8:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_5;
	// 	break;
	// case 12:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_6;
	// 	break;
	// case 16:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_7;
	// 	break;
	// case 24:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_8;
	// 	break;
	// case 32:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_9;
	// 	break;
	// case 48:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_10;
	// 	break;
	// case 64:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_11;
	// 	break;
	// case 96:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_12;
	// 	break;
	// case 128:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_13;
	// 	break;
	// case 176:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_14;
	// 	break;
	// case 192:
	// 	mclk_div = SUNXI_DAUDIO_MCLK_DIV_15;
	// 	break;
	// default:
	// 	hal_log_err("unsupport  mclk_div\n");
	// }

	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_CLKDIV,
			(SUNXI_DAUDIO_MCLK_DIV_MASK << MCLK_DIV),
			(SUNXI_DAUDIO_MCLK_DIV_1 << MCLK_DIV));
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_CLKDIV,
				(1 << MCLKOUT_EN), (1 << MCLKOUT_EN));

	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_CLKDIV,
			(SUNXI_DAUDIO_BCLK_DIV_MASK<<BCLK_DIV),
			(6 << BCLK_DIV));
	/*set fmt */
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_CTL,
		(SUNXI_DAUDIO_LRCK_OUT_MASK << LRCK_OUT),
		(SUNXI_DAUDIO_LRCK_OUT_ENABLE << LRCK_OUT));

	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FMT0,
			(SUNXI_DAUDIO_SR_MASK << DAUDIO_SAMPLE_RESOLUTION),
			(SUNXI_DAUDIO_SR_16BIT<<DAUDIO_SAMPLE_RESOLUTION));
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FIFOCTL,
			(SUNXI_DAUDIO_TXIM_MASK<<TXIM),
			(SUNXI_DAUDIO_TXIM_VALID_LSB<<TXIM));

	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_CHCFG,
		(SUNXI_DAUDIO_TX_SLOT_MASK<<TX_SLOT_NUM),(0x1<<TX_SLOT_NUM));
	reg_write(I2S0_BASE + SUNXI_DAUDIO_TX0CHMAP0, SUNXI_DEFAULT_CHMAP0);
	reg_write(I2S0_BASE + SUNXI_DAUDIO_TX0CHMAP1, SUNXI_DEFAULT_CHMAP1);
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_TX0CHSEL,
		(SUNXI_DAUDIO_TX_CHSEL_MASK<<TX_CHSEL), (0x1<<TX_CHSEL));
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_TX0CHSEL,
		(SUNXI_DAUDIO_TX_CHEN_MASK<<TX_CHEN), (0x3<<TX_CHEN));

	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_CTL,
			(SUNXI_DAUDIO_MODE_CTL_MASK<<MODE_SEL),
			(0x1 << MODE_SEL));
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_TX0CHSEL,
			(SUNXI_DAUDIO_TX_OFFSET_MASK<<TX_OFFSET),
			(0x1 << TX_OFFSET));
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_RXCHSEL,
			(SUNXI_DAUDIO_RX_OFFSET_MASK<<RX_OFFSET),
			(0x1 << RX_OFFSET));

	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FMT0,
			(1<<LRCK_POLARITY), (0 <<LRCK_POLARITY));
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FMT0,
			(1<<BRCK_POLARITY), (0 <<BRCK_POLARITY));

	/* prepare*/
	 reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_FIFOCTL,
		(1 << FIFO_CTL_FTX), (1 << FIFO_CTL_FTX));

	reg_write(I2S0_BASE + SUNXI_DAUDIO_TXCNT, 0);

	/* trigger */
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_CTL,
		(1 << GLOBAL_EN), (1 << GLOBAL_EN));

	/* trctrl_enable*/
	reg_update_bits(I2S0_BASE + SUNXI_DAUDIO_CTL,
			(1 << SDO0_EN) | (1 << CTL_TXEN),
			(1 << SDO0_EN) | (1 << CTL_TXEN));
	reg_update_bits(I2S0_BASE +  SUNXI_DAUDIO_INTCTL,
			(1 << TXDRQEN), (1 << TXDRQEN));

}


void i2s_gpio_sys(void)
{
	reg_write(0x07010374, 0x1); //sys
	reg_update_bits(0x02000030, 0xf << 16, 0x3 << 16); //I2S0_mclk
	reg_update_bits(0x02000030, 0xf << 20, 0x3 << 20); //I2S0_bclk
	reg_update_bits(0x02000030, 0xf << 24, 0x3 << 24); //I2S0_lrlk
	reg_update_bits(0x02000030, 0xf << 28, 0x3 << 28); //I2S0_DOUT

}

static void i2s_dma_callback(void *arg)
{
	printf("[%s]\n", __func__);
	test_cnt--;
}

void i2s_dma(void)
{
	// dma_addr_t dma_buf;
	char *dma_buf = NULL;
	struct sunxi_dma_chan *chan = NULL;
	struct dma_slave_config slave_config = {0};
	int ret;
	uint32_t size = 0;

	/* 申请内存 */
	dma_buf = hal_malloc_coherent(I2S_BUF_LEN);

	/* 申请通道 */
	ret = hal_dma_chan_request(&chan);
	if (ret == HAL_DMA_CHAN_STATUS_BUSY) {
		printf("dma channel busy!\n");
	}

	/* playback */
	slave_config.direction = DMA_MEM_TO_DEV;
	slave_config.dst_addr = I2S0_BASE + SUNXI_DAUDIO_TXFIFO;
	/* 16bit*/
	slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	slave_config.dst_maxburst = DMA_SLAVE_BURST_4;
	slave_config.src_maxburst = DMA_SLAVE_BURST_4;
	/* playback */
	slave_config.slave_id = sunxi_slave_id(3, DRQSRC_SDRAM);

	ret = hal_dma_slave_config(chan, &slave_config);
	if (ret != HAL_DMA_STATUS_OK) {
		printf("[%s, %d] failed!\n", __func__, __LINE__);
	}

	/* flush ringbuffer */
	hal_dcache_clean_invalidate((unsigned int)dma_buf, I2S_BUF_LEN);

	/* 传输描述符 */
	ret = hal_dma_prep_cyclic(chan, (unsigned long)dma_buf, BUF_LEN, PERIOD_LEN, DMA_MEM_TO_DEV);
	if (ret != HAL_DMA_STATUS_OK) {
		printf("[%s, %d] failed!\n", __func__, __LINE__);
	}

	ret = hal_dma_callback_install(chan, i2s_dma_callback, chan);
	if (ret != HAL_DMA_STATUS_OK) {
		printf("[%s, %d] failed!\n", __func__, __LINE__);
	}

	/* 启动 */
	ret = hal_dma_start(chan);
	if (ret != HAL_DMA_STATUS_OK) {
		printf("[%s, %d] failed!\n", __func__, __LINE__);
	}

	while (hal_dma_tx_status(chan, &size) != 0) {
		printf("test_cnt:%d\n", test_cnt);
		if (!test_cnt)
			break;
	}

	ret = hal_dma_stop(chan);
	if (ret != HAL_DMA_STATUS_OK) {
		printf("[%s, %d] failed!\n", __func__, __LINE__);
	}

	ret = hal_dma_chan_free(chan);
	if (ret != HAL_DMA_STATUS_OK) {
		printf("[%s, %d] failed!\n", __func__, __LINE__);
	}
}

int cmd_audio_test_for_dma(int argc, char **argv)
{
	printf("==========begin test==2============\n");
	i2s_clk();
	printf("==========1============\n");
	i2s_init();
	printf("==========2============\n");
	i2s_gpio_sys();
	printf("==========3============\n");
	i2s_dma();
	printf("============end==============\n");

	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_audio_test_for_dma, audio_test_for_dma, audio test for dma)
