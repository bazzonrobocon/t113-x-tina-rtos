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

#ifndef __SND_SUN55IW3_CODEC_H
#define __SND_SUN55IW3_CODEC_H

#include <hal_clk.h>
#include <sound/common/snd_sunxi_common.h>

#define CODEC		"audiocodec"

#define SUNXI_CODEC_BASE_ADDR		0x07110000

/* REG-Digital */
#define SUNXI_DAC_DPC			0x00
#define SUNXI_DAC_VOL_CTL		0x04
#define SUNXI_DAC_FIFO_CTL		0x10
#define SUNXI_DAC_FIFO_STA		0x14
#define SUNXI_DAC_TXDATA		0X20
#define SUNXI_DAC_CNT			0x24
#define SUNXI_DAC_DEBUG			0x28

#define	SUNXI_ADC_FIFO_CTL		0x30
#define SUNXI_ADC_VOL_CTL1		0x34
#define SUNXI_ADC_FIFO_STA		0x38
#define SUNXI_ADC_RXDATA		0x40
#define SUNXI_ADC_CNT			0x44
#define SUNXI_ADC_DEBUG			0x4C
#define SUNXI_ADC_DIG_CTL		0x50

#define SUNXI_VAR1SPEEDUP_DOWN_CTL	0x54

#define SUNXI_DAC_DAP_CTL		0xF0
#define SUNXI_ADC_DAP_CTL		0xF8
#define SUNXI_DAC_DRC_CTL		0x108
#define SUNXI_ADC_DRC_CTL		0x208

#define SUNXI_VERSION			0x2C0

/* REG-Analog */
#define SUNXI_ADC1_AN_CTL		0x300
#define SUNXI_ADC2_AN_CTL		0x304
#define SUNXI_ADC3_AN_CTL		0x308

#define SUNXI_DAC_AN_REG		0x310
#define SUNXI_DAC2_AN_REG		0x314

#define SUNXI_MICBIAS_AN_CTL		0x318
#define SUNXI_RAMP			0x31c
#define SUNXI_BIAS_AN_CTL		0x320
#define SUNXI_HP_AN_CTL			0x324
#define SUNXI_HMIC_CTL			0x328
#define SUNXI_HMIC_STA			0x32C
#define SUNXI_POWER_AN_CTL		0x348
#define SUNXI_AUDIO_MAX_REG		SUNXI_POWER_AN_CTL

/* BITS */
/* SUNXI_DAC_DPC:0x00 */
#define DAC_DIG_EN			31
#define MODQU				25
#define DWA_EN				24
#define HPF_EN				18
#define DVOL				12
#define DITHER_SGM			8
#define DITHER_SFT			4
#define DITHER_EN			1
#define HUB_EN				0
/* SUNXI_DAC_VOL_CTL:0x04 */
#define DAC_VOL_SEL			16
#define DAC_VOL_L			8
#define DAC_VOL_R			0
/* SUNXI_DAC_FIFO_CTL:0x10 */
#define DAC_FS				29
#define FIR_VER				28
#define SEND_LASAT			26
#define DAC_FIFO_MODE			24
#define DAC_DRQ_CLR_CNT			21
#define TX_TRIG_LEVEL			8
#define DAC_MONO_EN			6
#define TX_SAMPLE_BITS			5
#define DAC_DRQ_EN			4
#define DAC_IRQ_EN			3
#define DAC_FIFO_UNDERRUN_IRQ_EN	2
#define DAC_FIFO_OVERRUN_IRQ_EN		1
#define DAC_FIFO_FLUSH			0
/* SUNXI_DAC_FIFO_STA:0x14 */
#define	DAC_TX_EMPTY			23
#define	DAC_TXE_CNT			8
#define	DAC_TXE_INT			3
#define	DAC_TXU_INT			2
#define	DAC_TXO_INT			1
/* SUNXI_DAC_DEBUG:0x28 */
#define	DAC_MODU_SEL			11
#define	DAC_PATTERN_SEL			9
#define	CODEC_CLK_SEL			8
#define	DA_SWP				6
#define	ADDA_LOOP_MODE			0
/* SUNXI_ADC_FIFO_CTL:0x30 */
#define ADC_FS				29
#define ADC_DIG_EN			28
#define ADCFDT				26
#define ADCDFEN				25
#define RX_FIFO_MODE			24
#define RX_SYNC_EN_START		21
#define RX_SYNC_EN			20
#define RX_SAMPLE_BITS			16
#define RX_FIFO_TRG_LEVEL		4
#define ADC_DRQ_EN			3
#define ADC_IRQ_EN			2
#define ADC_OVERRUN_IRQ_EN		1
#define ADC_FIFO_FLUSH			0
/* SUNXI_ADC_VOL_CTL1:0x34 */
#define ADC3_VOL			16
#define ADC2_VOL			8
#define ADC1_VOL			0
/* SUNXI_ADC_FIFO_STA:0x38 */
#define	ADC_RXA				23
#define	ADC_RXA_CNT			8
#define	ADC_RXA_INT			3
#define	ADC_RXO_INT			1
/* SUNXI_ADC_DEBUG:0x4C */
#define	ADC_SWP2			25
#define	ADC_SWP1			24
/* SUNXI_ADC_DIG_CTL:0x50 */
#define ADC3_VOL_EN			17
#define ADC1_2_VOL_EN			16
#define ADC3_CHANNEL_EN			2
#define ADC2_CHANNEL_EN			1
#define ADC1_CHANNEL_EN			0
/* SUNXI_VAR1SPEEDUP_DOWN_CTL:0x54 */
#define VRA1SPEEDUP_DOWN_STATE		4
#define VRA1SPEEDUP_DOWN_CTL		1
#define VRA1SPEEDUP_DOWN_RST_CTL	0
/* SUNXI_DAC_DAP_CTL:0xF0 */
#define DDAP_EN				31
#define DDAP_DRC_EN			29
#define DDAP_HPF_EN			28
/* SUNXI_ADC_DAP_CTL:0xF8 */
#define ADAP0_EN			31
#define ADAP0_DRC_EN			29
#define ADAP0_HPF_EN			28
#define ADAP1_EN			27
#define ADAP1_DRC_EN			25
#define ADAP1_HPF_EN			24
/* SUNXI_ADC1_AN_CTL:0x300 */
#define ADC1_EN				31
#define MIC1_PGA_EN			30
#define ADC1_DITHER_CTL			29
#define DSM_DITHER_LVL			24
#define ADC1_OUTPUT_CURRENT		20
#define ADC1_PGA_CTL_RCM		18
#define ADC1_PGA_IN_VCM_CTL		16
#define IOPADC				14
#define ADC1_PGA_GAIN_CTL		8
#define ADC1_IOPAAF			6
#define ADC1_IOPSDM1			4
#define ADC1_IOPSDM2			2
#define ADC1_IOPMIC			0
/* SUNXI_ADC2_AN_CTL:0x304 */
#define ADC2_EN				31
#define MIC2_PGA_EN			30
#define ADC2_DITHER_CTL			29
#define DSM_DITHER_LVL			24
#define ADC2_OUTPUT_CURRENT		20
#define ADC2_PGA_CTL_RCM		18
#define ADC2_PGA_IN_VCM_CTL		16
#define ADC2_PGA_GAIN_CTL		8
#define ADC2_IOPAAF			6
#define ADC2_IOPSDM1			4
#define ADC2_IOPSDM2			2
#define ADC2_IOPMIC			0
/* SUNXI_ADC3_AN_CTL:0x308 */
#define ADC3_EN				31
#define MIC3_PGA_EN			30
#define ADC3_DITHER_CTL			29
#define DSM_DITHER_LVL			24
#define ADC3_OUTPUT_CURRENT		20
#define ADC3_PGA_CTL_RCM		18
#define ADC3_PGA_IN_VCM_CTL		16
#define ADC3_PGA_GAIN_CTL		8
#define ADC3_IOPAAF			6
#define ADC3_IOPSDM1			4
#define ADC3_IOPSDM2			2
#define ADC3_IOPMIC			0
/* SUNXI_DAC_AN_REG:0x310 */
#define CURRENT_TEST_SEL		31
#define HEADPHONE_GAIN			28
#define IOPHPDRV			26
#define CPLDO_VOLTAGE			24
#define OPDRV_CUR			22
#define IOPVRS				20
#define ILINEOUTAMPS			18
#define IOPDACS				16
#define DACL_EN				15
#define DACR_EN				14
#define LINEOUTL_EN			13
#define LMUTE				12
#define LINEOUTR_EN			11
#define RMUTE				10
#define CPLDO_BIAS			8
#define CPLDO_EN			7
#define LINEOUT_GAIN			0
/* SUNXI_DAC2_AN_REG:0x314 */
#define CKDAC_DELAY_SET			16
#define DACLR_CHOPPER_EN		15
#define DACLR_CHOPPER_NOL_EN		14
#define DACLR_CHOPPER_CKSET		12
#define DACLR_CHOPPER_DELAY_SET		10
#define DACLR_CHOPPER_NOL_DELAY_SET	8
#define LINEOUTLR_CHOPPER_EN		7
#define LINEOUTLR_CHOPPER_NOL_ENABLE	6
#define LINEOUTLR_CHOPPER_CKSET		4
#define LINEOUTLR_CHOPPER_DELAY_SET	2
#define LINEOUTLR_CHOPPER_NOL_DELAY_SET	0
/* SUNXI_MICBIAS_AN_CTL:0x318 */
#define SEL_DET_ADC_FS			28
#define SEL_DET_ADC_DB			26
#define SEL_DET_ADC_BF			24
#define JACK_DET_EN			23
#define SEL_DET_ADC_DELAY		21
#define MIC_DET_ADC_EN			20
#define POPFREE				19
#define DET_MODE			18
#define AUTO_PULL_LOW_EN		17
#define MIC_DET_PULL			16
#define HMIC_BIAS_EN			15
#define HMIC_BIAS_SEL			13
#define HMIC_BIAS_CHOPPER_EN		12
#define HMIC_BIAS_CHOPPER_CLK_SEL	10
#define MMIC_BIAS_EN			7
#define MMIC_BIAS_VOL_SEL		5
#define MMIC_BIAS_CHOPPER_EN		4
#define MMIC_BIAS_CHOPPER_CLK_SEL	2
/* SUNXI_BIAS_AN_CTL:0x320 */
#define BIASDATA			0
/* SUNXI_HP_AN_CTL:0x324 */
#define HPR_CALI_VERIFY			24
#define HPL_CALI_VERIFY			16
#define HPPA_EN				15
#define HP_INPUT_EN			11
#define HP_OUTPUT_EN			10
#define HPPA_DELAY			8
#define CP_CLK_SEL			6
#define HP_CALI_MODE_SEL		5
#define HP_CALI_VERIFY_EN		4
#define HP_CALI_FIRST			3
#define HP_CALI_CLK_SEL			0
/* SUNXI_HMIC_CTL:0x328 */
#define HMIC_FS_SEL			21
#define MDATA_THRESHOLD			16
#define HMIC_SMOOTH_FILTER_SET		14
#define HMIC_M				10
#define HMIC_N				6
#define MDATA_THRESHOLD_DEBOUNCE	3
#define JACK_OUT_IRQ_EN			2
#define JACK_IN_IRQ_EN			1
#define MIC_DET_IRQ_EN			0
/* SUNXI_HMIC_STA:0x32C */
#define MDATA_DISCARD			13
#define HMIC_DATA			8
#define JACK_OUT_IRQ_STA		4
#define JACK_IN_IRQ_STA			3
#define MIC_DET_IRQ_STA			0
/* SUNXI_POWER_AN_CTL:0x348 */
#define ALDO_EN				31
#define VAR1SPEEDUP_DOWN_FURTHER_CTL	29
#define VRP_LDO_CHOPPER_EN		27
#define VRP_LDO_CHOPPER_CLK_SEL		25
#define VRP_LDO_EN			24
#define AVCCPOR_MONITOR			16
#define BG_BUFFER_DISABLE		15
#define ALDO_OUTPUT_VOL			12
#define BG_ROUGH_VOL_TRIM		8
#define BG_FINE_TRIM			0

#define DACDRC_SHIFT			1
#define DACHPF_SHIFT			2
#define ADCDRC0_SHIFT			3
#define ADCHPF0_SHIFT			4
#define ADCDRC1_SHIFT			5
#define ADCHPF1_SHIFT			6


/* priv param */
#define SND_CTL_ENUM_LINEOUTL_MASK	1
#define SND_CTL_ENUM_LINEOUTR_MASK	2
#define SND_CTL_ENUM_MIC1_MASK		1
#define SND_CTL_ENUM_MIC2_MASK		2
#define SND_CTL_ENUM_MIC3_MASK		4
#define SND_CTL_ENUM_SPK_MASK		1
#define SND_CTL_ENUM_HPOUT_MASK		1

struct sunxi_codec_clk {
	/* dsp clk source */
	hal_clk_t clk_peri0_2x;
	hal_clk_t clk_dsp_src;
	/* parent */
	hal_clk_t clk_pll_audio0_4x;
	hal_clk_t clk_pll_audio1_div2;
	hal_clk_t clk_pll_audio1_div5;
	/* module */
	hal_clk_t clk_audio_dac;
	hal_clk_t clk_audio_adc;
	/* bus & reset */
	hal_clk_t clk_bus;
	struct reset_control *clk_rst;
};

struct sunxi_codec_param {
	/* volume & gain */
	uint8_t dac_vol;
	uint8_t dacl_vol;
	uint8_t dacr_vol;
	uint8_t adc1_vol;
	uint8_t adc2_vol;
	uint8_t adc3_vol;
	uint8_t lineout_gain;
	uint8_t hpout_gain;
	uint8_t adc1_gain;
	uint8_t adc2_gain;
	uint8_t adc3_gain;

	bool mic1_en;
	bool mic2_en;
	bool mic3_en;
	bool lineoutl_en;
	bool lineoutr_en;
	bool spk_en;
	bool hpout_en;

	bool adc1_en;
	bool adc2_en;
	bool adc3_en;

	/* tx_hub */
	bool tx_hub_en;

	/* rx_sync */
	bool rx_sync_en;

	bool playback_dapm;
	bool capture_dapm;
};

struct sunxi_mic_status {
	bool mic1;
	bool mic2;
	bool mic3;
};

struct sunxi_codec_info {
	struct snd_codec *codec;

	void *codec_base_addr;
	struct sunxi_codec_clk clk;
	struct sunxi_codec_param param;
	struct sunxi_pa_config pa_cfg;
	struct sunxi_mic_status mic_sts;
};

#endif /* __SND_SUN55IW3_CODEC_H */
