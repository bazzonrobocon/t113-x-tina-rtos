// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ac107.c -- AC107 ALSA SoC Audio driver
 *
 * Copyright (c) 2022 Allwinnertech Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <sound/snd_core.h>
#include <sound/snd_pcm.h>
#include <sound/pcm_common.h>
#include <aw_common.h>
#include <unistd.h>
#include <hal_cmd.h>
#include "ac107.h"

struct ac107_real_to_reg {
	unsigned int real;
	unsigned int reg;
};

struct ac107_pll_div {
	unsigned int freq_in;
	unsigned int freq_out;
	unsigned int m1:5;
	unsigned int m2:1;
	unsigned int n:10;
	unsigned int k1:5;
	unsigned int k2:1;
};

/* PLLCLK: FOUT =(FIN * N) / [(M1+1) * (M2+1)*(K1+1)*(K2+1)] */
static const struct ac107_pll_div ac107_pll_div[] = {
	{400000,   12288000, 0,  0, 983,  15, 1},
	{512000,   12288000, 0,  0, 960,  19, 1},
	{768000,   12288000, 0,  0, 640,  19, 1},
	{800000,   12288000, 0,  0, 768,  24, 1},
	{1024000,  12288000, 0,  0, 480,  19, 1},
	{1600000,  12288000, 0,  0, 384,  24, 1},
	{2048000,  12288000, 0,  0, 240,  19, 1},
	{3072000,  12288000, 0,  0, 160,  19, 1},
	{4096000,  12288000, 0,  0, 120,  19, 1},
	{6000000,  12288000, 4,  0, 512,  24, 1},
	{6144000,  12288000, 1,  0, 160,  19, 1},
	{12000000, 12288000, 9,  0, 512,  24, 1},
	{13000000, 12288000, 12, 0, 639,  25, 1},
	{15360000, 12288000, 9,  0, 320,  19, 1},
	{16000000, 12288000, 9,  0, 384,  24, 1},
	{19200000, 12288000, 11, 0, 384,  24, 1},
	{19680000, 12288000, 15, 1, 999,  24, 1},
	{24000000, 12288000, 9,  0, 256,  24, 1},

	{400000,   11289600, 0,  0, 1016, 17, 1},
	{512000,   11289600, 0,  0, 882,  19, 1},
	{768000,   11289600, 0,  0, 588,  19, 1},
	{800000,   11289600, 0,  0, 508,  17, 1},
	{1024000,  11289600, 0,  0, 441,  19, 1},
	{1600000,  11289600, 0,  0, 254,  17, 1},
	{2048000,  11289600, 1,  0, 441,  19, 1},
	{3072000,  11289600, 0,  0, 147,  19, 1},
	{4096000,  11289600, 3,  0, 441,  19, 1},
	{6000000,  11289600, 1,  0, 143,  18, 1},
	{6144000,  11289600, 1,  0, 147,  19, 1},
	{12000000, 11289600, 3,  0, 143,  18, 1},
	{13000000, 11289600, 12, 0, 429,  18, 1},
	{15360000, 11289600, 14, 0, 441,  19, 1},
	{16000000, 11289600, 24, 0, 882,  24, 1},
	{19200000, 11289600, 4,  0, 147,  24, 1},
	{19680000, 11289600, 13, 1, 771,  23, 1},
	{24000000, 11289600, 24, 0, 588,  24, 1},

	{12288000, 12288000, 9,  0, 400,  19, 1},
	{11289600, 11289600, 9,  0, 400,  19, 1},

	{24576000 / 1,   12288000, 9, 0, 200, 19, 1},
	{24576000 / 16,  12288000, 0, 0, 320, 19, 1},
	{24576000 / 64,  12288000, 0, 0, 640, 9,  1},
	{24576000 / 96,  12288000, 0, 0, 960, 9,  1},
	{24576000 / 128, 12288000, 0, 0, 512, 3,  1},
	{24576000 / 176, 12288000, 0, 0, 880, 4,  1},
	{24576000 / 192, 12288000, 0, 0, 960, 4,  1},

	{22579200 / 1,   11289600, 9, 0, 200, 19, 1},
	{22579200 / 4,   11289600, 4, 0, 400, 19, 1},
	{22579200 / 6,   11289600, 2, 0, 360, 19, 1},
	{22579200 / 8,   11289600, 0, 0, 160, 19, 1},
	{22579200 / 12,  11289600, 0, 0, 240, 19, 1},
	{22579200 / 16,  11289600, 0, 0, 320, 19, 1},
	{22579200 / 24,  11289600, 0, 0, 480, 19, 1},
	{22579200 / 32,  11289600, 0, 0, 640, 19, 1},
	{22579200 / 48,  11289600, 0, 0, 960, 19, 1},
	{22579200 / 64,  11289600, 0, 0, 640, 9,  1},
	{22579200 / 96,  11289600, 0, 0, 960, 9,  1},
	{22579200 / 128, 11289600, 0, 0, 512, 3,  1},
	{22579200 / 176, 11289600, 0, 0, 880, 4,  1},
	{22579200 / 192, 11289600, 0, 0, 960, 4,  1},
};

static const struct ac107_real_to_reg ac107_sample_bit[] = {
	{8,  1},
	{12, 2},
	{16, 3},
	{20, 4},
	{24, 5},
	{28, 6},
	{32, 7},
};

static const struct ac107_real_to_reg ac107_sample_rate[] = {
	{8000, 0},
	{11025, 1},
	{12000, 2},
	{16000, 3},
	{22050, 4},
	{24000, 5},
	{32000, 6},
	{44100, 7},
	{48000, 8},
};

static const struct ac107_real_to_reg ac107_slot_width[] = {
	{8,  1},
	{12, 2},
	{16, 3},
	{20, 4},
	{24, 5},
	{28, 6},
	{32, 7},
};

static const struct ac107_real_to_reg ac107_bclk_div[] = {
	{1, 1},
	{2, 2},
	{4, 3},
	{6, 4},
	{8, 5},
	{12, 6},
	{16, 7},
	{24, 8},
	{32, 9},
	{48, 10},
	{64, 11},
	{96, 12},
	{128, 13},
	{176, 14},
	{192, 15},
};

static twi_status_t ac107_init_i2c_device(twi_port_t port)
{
	twi_status_t ret = 0;

	ret = hal_twi_init(port);
	if (ret != TWI_STATUS_OK) {
		snd_err("init i2c err ret=%d.\n", ret);
		return ret;
	}

	return TWI_STATUS_OK;
}

static twi_status_t ac107_deinit_i2c_device(twi_port_t port)
{
	twi_status_t ret = 0;

	ret = hal_twi_uninit(port);
	if (ret != TWI_STATUS_OK) {
		snd_err("init i2c err ret=%d.\n", ret);
		return ret;
	}

	return TWI_STATUS_OK;
}

static twi_status_t ac107_read(struct twi_device *twi_dev,
			       unsigned char reg,
			       unsigned char *rt_value)
{
	twi_status_t ret;

	ret = hal_twi_read(twi_dev->bus, I2C_SLAVE, twi_dev->addr, reg, rt_value, 1);
	if (ret != TWI_STATUS_OK) {
		snd_err("error = %d [REG-0x%02x]\n", ret, reg);
		return ret;
	}

	return TWI_STATUS_OK;
}

static int ac107_write(struct twi_device *twi_dev,
		       unsigned char reg,
		       unsigned char value)
{
	twi_status_t ret;
	twi_msg_t msg;
	unsigned char buf[2] = {reg, value};

	msg.flags = 0;
	msg.addr =  twi_dev->addr;
	msg.len = 2;
	msg.buf = buf;

	ret = hal_twi_write(twi_dev->bus, &msg, 1);
	if (ret != TWI_STATUS_OK) {
		snd_err("error = %d [REG-0x%02x]\n", ret, reg);
		return ret;
	}

	return TWI_STATUS_OK;
}

static int ac107_update_bits(struct twi_device *twi_dev,
			     unsigned char reg,
			     unsigned char mask,
			     unsigned char value)
{
	unsigned char val_old = 0;
	unsigned char val_new = 0;

	ac107_read(twi_dev, reg, &val_old);
	val_new = (val_old & ~mask) | (value & mask);
	if (val_new != val_old) {
		ac107_write(twi_dev, reg, val_new);
	}

	return 0;
}


static int ac107_codec_startup(struct snd_pcm_substream *substream,
			       struct snd_dai *dai)
{
	struct snd_codec *codec = dai->component;
	struct ac107_priv *ac107 = codec->private_data;
	struct ac107_param *param = &ac107->param;

	snd_print("\n");

	/* VREF enable */
	ac107_update_bits(&param->twi_dev, PWR_CTRL1, 0x1 << VREF_ENABLE, 0x1 << VREF_ENABLE);
	/* module enable */
	ac107_update_bits(&param->twi_dev, MOD_CLK_EN, 0x1 << ADC_DIGITAL, 0x1 << ADC_DIGITAL);
	ac107_update_bits(&param->twi_dev, MOD_CLK_EN, 0x1 << ADC_ANALOG, 0x1 << ADC_ANALOG);
	ac107_update_bits(&param->twi_dev, MOD_CLK_EN, 0x1 << I2S, 0x1 << I2S);
	/* ADC & I2S de-asserted */
	ac107_update_bits(&param->twi_dev, MOD_RST_CTRL, 0x1 << ADC_DIGITAL, 0x1 << ADC_DIGITAL);
	ac107_update_bits(&param->twi_dev, MOD_RST_CTRL, 0x1 << I2S, 0x1 << I2S);

	return 0;
}

static int ac107_codec_set_sysclk(struct snd_dai *dai,
				  int clk_id,
				  unsigned int freq,
				  int dir)
{
	struct snd_codec *codec = dai->component;
	struct ac107_priv *ac107 = codec->private_data;
	struct ac107_param *param = &ac107->param;

	snd_print("\n");

	switch (clk_id) {
	case SYSCLK_SRC_MCLK:
		ac107_update_bits(&param->twi_dev, SYSCLK_CTRL,
				  0x3 << SYSCLK_SRC, 0x0 << SYSCLK_SRC);
		break;
	case SYSCLK_SRC_BCLK:
		ac107_update_bits(&param->twi_dev, SYSCLK_CTRL,
				  0x3 << SYSCLK_SRC, 0x1 << SYSCLK_SRC);
		break;
	case SYSCLK_SRC_PLL:
		ac107_update_bits(&param->twi_dev, SYSCLK_CTRL,
				  0x3 << SYSCLK_SRC, 0x2 << SYSCLK_SRC);
		break;
	default:
		snd_err("ac107 sysclk source config error: %d\n", clk_id);
		return -EINVAL;
	}

	param->sysclk_freq = freq;

	return 0;
}

static int ac107_codec_set_pll(struct snd_dai *dai,
			       int pll_id,
			       int source,
			       unsigned int freq_in,
			       unsigned int freq_out)
{
	struct snd_codec *codec = dai->component;
	struct ac107_priv *ac107 = codec->private_data;
	struct ac107_param *param = &ac107->param;
	unsigned int i  = 0;
	unsigned int m1 = 0;
	unsigned int m2 = 0;
	unsigned int n  = 0;
	unsigned int k1 = 0;
	unsigned int k2 = 0;

	snd_print("\n");

	if (freq_in < 128000 || freq_in > 24576000) {
		snd_err("ac107 pllclk source input only support [128K,24M], now %u\n",
			freq_in);
		return -EINVAL;
	}

	if (pll_id != SYSCLK_SRC_PLL) {
		snd_err("ac107 sysclk source don't pll, don't need config pll\n");
		return 0;
	}

	switch (pll_id) {
	case PLLCLK_SRC_MCLK:
		ac107_update_bits(&param->twi_dev, SYSCLK_CTRL,
				  0x3 << PLLCLK_SRC, 0x0 << PLLCLK_SRC);
		break;
	case PLLCLK_SRC_BCLK:
		ac107_update_bits(&param->twi_dev, SYSCLK_CTRL,
				  0x3 << PLLCLK_SRC, 0x1 << PLLCLK_SRC);
		break;
	case PLLCLK_SRC_PDMCLK:
		ac107_update_bits(&param->twi_dev, SYSCLK_CTRL,
				  0x3 << PLLCLK_SRC, 0x2 << PLLCLK_SRC);
		break;
	default:
		snd_err("ac107 pllclk source config error: %d\n", pll_id);
		return -EINVAL;
	}

	/* PLLCLK: FOUT =(FIN * N) / [(M1+1) * (M2+1)*(K1+1)*(K2+1)] */
	for (i = 0; i < ARRAY_SIZE(ac107_pll_div); i++) {
		if (ac107_pll_div[i].freq_in == freq_in && ac107_pll_div[i].freq_out == freq_out) {
			m1 = ac107_pll_div[i].m1;
			m2 = ac107_pll_div[i].m2;
			n  = ac107_pll_div[i].n;
			k1 = ac107_pll_div[i].k1;
			k2 = ac107_pll_div[i].k2;
			snd_err("ac107 PLL match freq_in:%u, freq_out:%u\n",
				freq_in, freq_out);
			break;
		}
	}
	if (i == ARRAY_SIZE(ac107_pll_div)) {
		snd_err("ac107 PLL don't match freq_in and freq_out table\n");
		return -EINVAL;
	}

	/* Config PLL DIV param M1/M2/N/K1/K2 */
	ac107_update_bits(&param->twi_dev, PLL_CTRL2,
			  0x1f << PLL_PREDIV1, m1 << PLL_PREDIV1);
	ac107_update_bits(&param->twi_dev, PLL_CTRL2,
			  0x1 << PLL_PREDIV2, m2 << PLL_PREDIV2);
	ac107_update_bits(&param->twi_dev, PLL_CTRL3,
			  0x3 << PLL_LOOPDIV_MSB, (n >> 8) << PLL_LOOPDIV_MSB);
	ac107_update_bits(&param->twi_dev, PLL_CTRL4,
			  0xff << PLL_LOOPDIV_LSB, n << PLL_LOOPDIV_LSB);
	ac107_update_bits(&param->twi_dev, PLL_CTRL5,
			  0x1f << PLL_POSTDIV1, k1 << PLL_POSTDIV1);
	ac107_update_bits(&param->twi_dev, PLL_CTRL5,
			  0x1 << PLL_POSTDIV2, k2 << PLL_POSTDIV2);

	/* Config PLL module current */
	ac107_update_bits(&param->twi_dev, PLL_CTRL1,
			  0x7 << PLL_IBIAS, 0x4 << PLL_IBIAS);
	ac107_update_bits(&param->twi_dev, PLL_CTRL1,
			  0x1 << PLL_COM_EN, 0x1 << PLL_COM_EN);
	ac107_update_bits(&param->twi_dev, PLL_CTRL1,
			  0x1 << PLL_EN, 0x1 << PLL_EN);
	ac107_update_bits(&param->twi_dev, PLL_CTRL6,
			  0x1f << PLL_CP, 0xf << PLL_CP);

	/* PLLCLK lock */
	ac107_update_bits(&param->twi_dev, PLL_LOCK_CTRL,
			  0x1 << PLL_LOCK_EN, 0x1 << PLL_LOCK_EN);

	return 0;
}

static int ac107_codec_set_clkdiv(struct snd_dai *dai, int div_id, int div)
{
	snd_print("\n");

	return 0;
}

static int ac107_codec_set_fmt(struct snd_dai *dai, unsigned int fmt)
{
	struct snd_codec *codec = dai->component;
	struct ac107_priv *ac107 = codec->private_data;
	struct ac107_param *param = &ac107->param;
	unsigned int i2s_mode, tx_offset, sign_ext;
	unsigned int brck_polarity, lrck_polarity;

	snd_print("\n");

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		ac107_update_bits(&param->twi_dev, I2S_CTRL,
				  0x1 << BCLK_IOEN, 0x1 << BCLK_IOEN);
		ac107_update_bits(&param->twi_dev, I2S_CTRL,
				  0x1 << LRCK_IOEN, 0x1 << LRCK_IOEN);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		ac107_update_bits(&param->twi_dev, I2S_CTRL,
				  0x1 << BCLK_IOEN, 0x0 << BCLK_IOEN);
		ac107_update_bits(&param->twi_dev, I2S_CTRL,
				  0x1 << LRCK_IOEN, 0x0 << LRCK_IOEN);
		break;
	default:
		snd_err("only support CBM_CFM or CBS_CFS\n");
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		i2s_mode = 1;
		tx_offset = 1;
		sign_ext = 3;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		i2s_mode = 2;
		tx_offset = 0;
		sign_ext = 3;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		i2s_mode = 1;
		tx_offset = 0;
		sign_ext = 3;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		i2s_mode = 0;
		tx_offset = 1;
		sign_ext = 3;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		i2s_mode = 0;
		tx_offset = 0;
		sign_ext = 3;
		break;
	default:
		return -EINVAL;
	}
	ac107_update_bits(&param->twi_dev, I2S_FMT_CTRL1,
			  0x3 << MODE_SEL, i2s_mode << MODE_SEL);
	ac107_update_bits(&param->twi_dev, I2S_FMT_CTRL1,
			  0x1 << TX_OFFSET, tx_offset << TX_OFFSET);
	ac107_update_bits(&param->twi_dev, I2S_FMT_CTRL3,
			  0x3 << SEXT, sign_ext << SEXT);
	ac107_update_bits(&param->twi_dev, I2S_FMT_CTRL3, 0x1 << LRCK_WIDTH,
			  (param->frame_sync_width - 1) << LRCK_WIDTH);

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		brck_polarity = 0;
		lrck_polarity = 0;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		brck_polarity = 0;
		lrck_polarity = 1;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		brck_polarity = 1;
		lrck_polarity = 0;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		brck_polarity = 1;
		lrck_polarity = 1;
		break;
	default:
		return -EINVAL;
	}

	ac107_update_bits(&param->twi_dev, I2S_BCLK_CTRL,
			   0x1 << BCLK_POLARITY, brck_polarity << BCLK_POLARITY);
	ac107_update_bits(&param->twi_dev, I2S_LRCK_CTRL1,
			   0x1 << LRCK_POLARITY, lrck_polarity << LRCK_POLARITY);

	param->fmt = fmt;

	snd_soc_dai_set_tdm_slot(dai, 0, 0, param->slots, param->slot_width);

	return 0;
}

static int ac107_codec_set_tdm_slot(struct snd_dai *dai,
				    unsigned int tx_mask, unsigned int rx_mask,
				    int slots, int slot_width)
{
	struct snd_codec *codec = dai->component;
	struct ac107_priv *ac107 = codec->private_data;
	struct ac107_param *param = &ac107->param;
	int i;
	unsigned int slot_width_reg = 0;
	unsigned int lrck_width_reg = 0;

	snd_print("\n");

	for (i = 0; i < ARRAY_SIZE(ac107_slot_width); i++) {
		if (ac107_slot_width[i].real == slot_width) {
			slot_width_reg = ac107_slot_width[i].reg;
			break;
		}
	}
	if (i == ARRAY_SIZE(ac107_slot_width)) {
		snd_err("ac107 unsupport slot_width: %d\n", slot_width);
		return -EINVAL;
	}
	ac107_update_bits(&param->twi_dev, I2S_FMT_CTRL2,
			   0x7 << SLOT_WIDTH_SEL, slot_width_reg << SLOT_WIDTH_SEL);

	switch (param->fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		lrck_width_reg = (slots / 2) * slot_width - 1;
		break;
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		lrck_width_reg = slots * slot_width - 1;
		break;
	default:
		return -EINVAL;
	}
	ac107_update_bits(&param->twi_dev, I2S_LRCK_CTRL1,
			   0x3 << LRCK_PERIODH, (lrck_width_reg >> 8) << LRCK_PERIODH);
	ac107_update_bits(&param->twi_dev, I2S_LRCK_CTRL2,
			   0xff << LRCK_PERIODL, lrck_width_reg << LRCK_PERIODH);

	param->slots = slots;
	param->slot_width = slot_width;

	return 0;

}

static int ac107_codec_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_dai *dai)
{
	struct snd_codec *codec = dai->component;
	struct ac107_priv *ac107 = codec->private_data;
	struct ac107_param *param = &ac107->param;
	int i, ret;
	unsigned int sample_bit;
	unsigned int sample_rate;
	unsigned int channels;
	unsigned int bclk_ratio;
	unsigned int sample_bit_reg = 0;
	unsigned int sample_rate_reg = 0;
	unsigned int channels_en_reg = 0;
	unsigned int bclk_ratio_reg = 0;

	snd_print("\n");

	/* set codec dai fmt */
	ret = snd_soc_dai_set_fmt(dai, param->daudio_format
				  | (param->signal_inversion << SND_SOC_DAIFMT_SIG_SHIFT)
				  | (param->daudio_master << SND_SOC_DAIFMT_MASTER_SHIFT));
	if (ret < 0) {
		snd_err("codec_dai set pll failed\n");
		return -EINVAL;
	}

	/* set sample bit */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		sample_bit = 8;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		sample_bit = 16;
		break;
	/* case SNDRV_PCM_FORMAT_S20_3LE:
		sample_bit = 20;
		break; */
	case SNDRV_PCM_FORMAT_S24_LE:
		sample_bit = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		sample_bit = 32;
		break;
	default:
		snd_err("ac107 unsupport the sample bit\n");
		return -EINVAL;
	}
	for (i = 0; i < ARRAY_SIZE(ac107_sample_bit); i++) {
		if (ac107_sample_bit[i].real == sample_bit) {
			sample_bit_reg = ac107_sample_bit[i].reg;
			break;
		}
	}
	if (i == ARRAY_SIZE(ac107_sample_bit)) {
		snd_err("ac107 unsupport the sample bit config: %u\n", sample_bit);
		return -EINVAL;
	}
	ac107_update_bits(&param->twi_dev, I2S_FMT_CTRL2,
			   0x7 << SAMPLE_RESOLUTION,
			   sample_bit_reg << SAMPLE_RESOLUTION);

	/* set sample rate */
	sample_rate = params_rate(params);
	for (i = 0; i < ARRAY_SIZE(ac107_sample_rate); i++) {
		if (ac107_sample_rate[i].real == sample_rate) {
			sample_rate_reg = ac107_sample_rate[i].reg;
			break;
		}
	}
	if (i == ARRAY_SIZE(ac107_sample_rate)) {
		snd_err("ac107 unsupport the sample rate config: %u\n", sample_rate);
		return -EINVAL;
	}
	ac107_update_bits(&param->twi_dev, ADC_SPRC,
			  0xf << ADC_FS_I2S, sample_rate_reg << ADC_FS_I2S);

	/* set channels */
	channels = params_channels(params);
	if (channels > 16) {
		snd_err("ac107 unsupport the channels config: %u\n", channels);
		return -EINVAL;
	}
	for (i = 0; i < channels; i++)
		channels_en_reg |= (1 << i);
	ac107_update_bits(&param->twi_dev, I2S_TX_CTRL1,
			  0xf << TX_CHSEL, (channels - 1) << TX_CHSEL);
	ac107_write(&param->twi_dev, I2S_TX_CTRL2, (channels_en_reg & 0xff));
	ac107_write(&param->twi_dev, I2S_TX_CTRL3, (channels_en_reg >> 8));

	/* set bclk div: ratio = sysclk / sample_rate / slots / slot_width */
	bclk_ratio = param->sysclk_freq / sample_rate / param->slots / param->slot_width;
	for (i = 0; i < ARRAY_SIZE(ac107_bclk_div); i++) {
		if (ac107_bclk_div[i].real == bclk_ratio) {
			bclk_ratio_reg = ac107_bclk_div[i].reg;
			break;
		}
	}
	if (i == ARRAY_SIZE(ac107_bclk_div)) {
		snd_err("ac107 unsupport bclk_div: %d\n", bclk_ratio);
		return -EINVAL;
	}
	ac107_update_bits(&param->twi_dev, I2S_BCLK_CTRL,
			  0xf << BCLKDIV, bclk_ratio_reg << BCLKDIV);

	/* PLLCLK enable */
	if (param->sysclk_src == SYSCLK_SRC_PLL)
		ac107_update_bits(&param->twi_dev, SYSCLK_CTRL,
				  0x1 << PLLCLK_EN, 0x1 << PLLCLK_EN);
	/* SYSCLK enable */
	ac107_update_bits(&param->twi_dev, SYSCLK_CTRL,
			  0x1 << SYSCLK_EN, 0x1 << SYSCLK_EN);
	/* ac107 ADC digital enable, I2S TX & Globle enable */
	ac107_update_bits(&param->twi_dev, ADC_DIG_EN,
			  0x1 << DG_EN, 0x1 << DG_EN);
	ac107_update_bits(&param->twi_dev, I2S_CTRL,
			  0x1 << SDO_EN, 0x1 << SDO_EN);
	ac107_update_bits(&param->twi_dev, I2S_CTRL,
			  0x1 << TXEN | 0x1 << GEN,
			  0x1 << TXEN | 0x1 << GEN);

	return 0;
}

static int ac107_codec_prepare(struct snd_pcm_substream *substream,
			       struct snd_dai *dai)
{
	snd_print("\n");

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		snd_err("ac107 playback prepare.\n");
	} else {
		snd_err("ac107 capture prepare.\n");
	}

	return 0;
}

static int ac107_codec_trigger(struct snd_pcm_substream *substream,
			       int cmd,
			       struct snd_dai *dai)
{
	snd_print("\n");

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			snd_err("ac107 playback trigger start.\n");
		} else {
			snd_err("ac107 capture trigger start\n");
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			snd_err("ac107 playback trigger stop.\n");
		} else {
			snd_err("ac107 capture trigger stop\n");
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int ac107_codec_hw_free(struct snd_pcm_substream *substream,
			       struct snd_dai *dai)
{
	struct snd_codec *codec = dai->component;
	struct ac107_priv *ac107 = codec->private_data;
	struct ac107_param *param = &ac107->param;

	snd_print("\n");

	ac107_update_bits(&param->twi_dev, I2S_CTRL,
			  0x1 << TXEN | 0x1 << GEN,
			  0x0 << TXEN | 0x0 << GEN);
	ac107_update_bits(&param->twi_dev, I2S_CTRL,
			  0x1 << SDO_EN, 0x0 << SDO_EN);
	ac107_update_bits(&param->twi_dev, ADC_DIG_EN,
			  0x1 << DG_EN, 0x0 << DG_EN);
	ac107_update_bits(&param->twi_dev, SYSCLK_CTRL,
			  0x1 << SYSCLK_EN, 0x0 << SYSCLK_EN);
	ac107_update_bits(&param->twi_dev, SYSCLK_CTRL,
			  0x1 << PLLCLK_EN, 0x0 << PLLCLK_EN);

	return 0;
}

static void ac107_codec_shutdown(struct snd_pcm_substream *substream,
				  struct snd_dai *dai)
{
	struct snd_codec *codec = dai->component;
	struct ac107_priv *ac107 = codec->private_data;
	struct ac107_param *param = &ac107->param;

	snd_print("\n");

	ac107_update_bits(&param->twi_dev, MOD_RST_CTRL,
			  0x1 << I2S, 0x0 << I2S);
	ac107_update_bits(&param->twi_dev, MOD_RST_CTRL,
			  0x1 << ADC_DIGITAL, 0x0 << ADC_DIGITAL);
	ac107_update_bits(&param->twi_dev, MOD_CLK_EN,
			  0x1 << I2S, 0x0 << I2S);
	ac107_update_bits(&param->twi_dev, MOD_CLK_EN,
			  0x1 << ADC_ANALOG, 0x0 << ADC_ANALOG);
	ac107_update_bits(&param->twi_dev, MOD_CLK_EN,
			  0x1 << ADC_DIGITAL, 0x0 << ADC_DIGITAL);
	ac107_update_bits(&param->twi_dev, PWR_CTRL1,
			  0x1 << VREF_ENABLE, 0x0 << VREF_ENABLE);
}

static int ac107_codec_dapm_control(struct snd_pcm_substream *substream,
				    struct snd_dai *dai, int onoff)
{
	struct snd_codec *codec = dai->component;
	struct ac107_priv *ac107 = codec->private_data;
	struct ac107_param *param = &ac107->param;

	if (onoff) {
		/* adc pga*/
		ac107_update_bits(&param->twi_dev, ADC_DIG_EN,
				  0x1 << ENAD1, 0x1 << ENAD1);
		ac107_update_bits(&param->twi_dev, ADC_DIG_EN,
				  0x1 << ENAD2, 0x1 << ENAD2);
		/* mic1 */
		ac107_update_bits(&param->twi_dev, PWR_CTRL2,
				  0x1 << MICBIAS1_EN, 0x1 << MICBIAS1_EN);
		ac107_update_bits(&param->twi_dev, ANA_ADC1_CTRL5,
				  1 << RX1_GLOBAL_EN, 1 << RX1_GLOBAL_EN);
		/* mic2 */
		ac107_update_bits(&param->twi_dev, PWR_CTRL2,
				  0x1 << MICBIAS2_EN, 0x1 << MICBIAS2_EN);
		ac107_update_bits(&param->twi_dev, ANA_ADC2_CTRL5,
				  1 << RX2_GLOBAL_EN, 1 << RX2_GLOBAL_EN);
	} else {

		ac107_update_bits(&param->twi_dev, ANA_ADC1_CTRL5,
				  1 << RX1_GLOBAL_EN, 0 << RX1_GLOBAL_EN);
		ac107_update_bits(&param->twi_dev, PWR_CTRL2,
				  0x1 << MICBIAS1_EN, 0x0 << MICBIAS1_EN);

		ac107_update_bits(&param->twi_dev, ANA_ADC2_CTRL5,
				  1 << RX2_GLOBAL_EN, 0 << RX2_GLOBAL_EN);
		ac107_update_bits(&param->twi_dev, PWR_CTRL2,
				  0x1 << MICBIAS2_EN, 0x0 << MICBIAS2_EN);
	}

	return 0;
}

static const struct snd_dai_ops ac107_codec_dai_ops = {
	.startup	= ac107_codec_startup,
	.set_sysclk	= ac107_codec_set_sysclk,
	.set_pll	= ac107_codec_set_pll,
	.set_clkdiv	= ac107_codec_set_clkdiv,
	.set_fmt	= ac107_codec_set_fmt,
	.set_tdm_slot	= ac107_codec_set_tdm_slot,
	.hw_params	= ac107_codec_hw_params,
	.prepare	= ac107_codec_prepare,
	.trigger	= ac107_codec_trigger,
	.hw_free	= ac107_codec_hw_free,
	.shutdown	= ac107_codec_shutdown,
	.dapm_control	= ac107_codec_dapm_control,
};

static struct snd_dai ac107_codec_dai[] = {
	{
		.name		= "ac107-codecdai",
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FMTBIT_S8
					| SNDRV_PCM_FMTBIT_S16_LE
					/* | SNDRV_PCM_FMTBIT_S20_3LE */
					| SNDRV_PCM_FMTBIT_S24_LE
					| SNDRV_PCM_FMTBIT_S32_LE,
			.rate_min	= 8000,
			.rate_max	= 48000,
			},
		.ops = &ac107_codec_dai_ops,
	},
};

static const char *sunxi_switch_text[] = {"Off", "On"};

static int ac107_ctl_enum_value_get(struct snd_kcontrol *kcontrol, struct snd_ctl_info *info)
{
	unsigned int val = 0;

	if (kcontrol->type != SND_CTL_ELEM_TYPE_ENUMERATED) {
		snd_err("invalid kcontrol type = %d.\n", kcontrol->type);
		return -EINVAL;
	}

	if (kcontrol->private_data_type == SND_MODULE_CODEC) {
		struct snd_codec *codec = kcontrol->private_data;
		struct ac107_priv *ac107 = codec->private_data;
		struct ac107_param *param = &ac107->param;
		ac107_read(&param->twi_dev, kcontrol->reg, (unsigned char *)&val);
	} else {
		snd_err("invalid kcontrol data type = %d.\n", kcontrol->private_data_type);
	}

	snd_kcontrol_to_snd_ctl_info(kcontrol, info, val);

	return 0;
}

static int ac107_ctl_enum_value_set(struct snd_kcontrol *kcontrol, unsigned long val)
{
	if (kcontrol->type != SND_CTL_ELEM_TYPE_ENUMERATED) {
		snd_err("invalid kcontrol type = %d.\n", kcontrol->type);
		return -EINVAL;
	}

	if (val >= kcontrol->items) {
		snd_err("invalid kcontrol items = %ld.\n", val);
		return -EINVAL;
	}

	if (kcontrol->private_data_type == SND_MODULE_CODEC) {
		struct snd_codec *codec = kcontrol->private_data;
		struct ac107_priv *ac107 = codec->private_data;
		struct ac107_param *param = &ac107->param;
		ac107_update_bits(&param->twi_dev, kcontrol->reg,
				(kcontrol->mask << kcontrol->shift),
				((unsigned int)val << kcontrol->shift));
	} else {
		snd_err("invalid kcontrol data type = %d.\n", kcontrol->private_data_type);
		return -EINVAL;
	}
	kcontrol->value = val & kcontrol->mask;

	snd_info("mask:0x%x, shift:%d, value:0x%x\n", kcontrol->mask, kcontrol->shift, kcontrol->value);

	return 0;
}

static int ac107_ctl_value_get(struct snd_kcontrol *kcontrol, struct snd_ctl_info *info)
{
	unsigned int val = 0;

	if (kcontrol->type != SND_CTL_ELEM_TYPE_INTEGER) {
		snd_err("invalid kcontrol type = %d.\n", kcontrol->type);
		return -EINVAL;
	}

	if (kcontrol->private_data_type == SND_MODULE_CODEC) {
		struct snd_codec *codec = kcontrol->private_data;
		struct ac107_priv *ac107 = codec->private_data;
		struct ac107_param *param = &ac107->param;
		ac107_read(&param->twi_dev, kcontrol->reg, (unsigned char *)&val);
	} else {
		snd_err("invalid kcontrol data type = %d.\n", kcontrol->private_data_type);
	}

	snd_kcontrol_to_snd_ctl_info(kcontrol, info, val);

	return 0;
}

static int ac107_ctl_value_set(struct snd_kcontrol *kcontrol, unsigned long val)
{
	if (kcontrol->type != SND_CTL_ELEM_TYPE_INTEGER) {
		snd_err("invalid kcontrol type = %d.\n", kcontrol->type);
		return -EINVAL;
	}

	if (kcontrol->private_data_type == SND_MODULE_CODEC) {
		struct snd_codec *codec = kcontrol->private_data;
		struct ac107_priv *ac107 = codec->private_data;
		struct ac107_param *param = &ac107->param;
		ac107_update_bits(&param->twi_dev, kcontrol->reg,
				  (kcontrol->mask << kcontrol->shift),
				  ((unsigned int)val << kcontrol->shift));
	} else {
		snd_err("invalid kcontrol data type = %d.\n", kcontrol->private_data_type);
	}
	snd_info("mask:0x%x, shitf:%d, value:0x%x\n",
			kcontrol->mask, kcontrol->shift, val);
	return 0;
}

static struct snd_kcontrol ac107_codec_controls[] = {
	SND_CTL_KCONTROL_VALUE_EXT("ADC1 Digital Volume",
				   ADC1_DVOL_CTRL,
				   DIG_ADCL1_VOL, 255, 0,
				   ac107_ctl_value_get, ac107_ctl_value_set),
	SND_CTL_KCONTROL_VALUE_EXT("ADC2 Digital Volume",
				   ADC2_DVOL_CTRL,
				   DIG_ADCL2_VOL, 255, 0,
				   ac107_ctl_value_get, ac107_ctl_value_set),
	SND_CTL_KCONTROL_VALUE_EXT("ADC1 PGA Gain",
				   ANA_ADC1_CTRL3,
				   RX1_PGA_GAIN_CTRL, 31, 0,
				   ac107_ctl_value_get, ac107_ctl_value_set),
	SND_CTL_KCONTROL_VALUE_EXT("ADC2 PGA Gain",
				   ANA_ADC2_CTRL3,
				   RX2_PGA_GAIN_CTRL, 31, 0,
				   ac107_ctl_value_get, ac107_ctl_value_set),

	SND_CTL_ENUM_VALUE_EXT("ADC1 DIG MIXER ADC1 DAT Switch",
			       ARRAY_SIZE(sunxi_switch_text), sunxi_switch_text,
			       ADC1_DMIX_SRC, ADC1_ADC1_DMXL_SRC, SND_CTL_ENUM_AUTO_MASK,
			       ac107_ctl_enum_value_get, ac107_ctl_enum_value_set),
	SND_CTL_ENUM_VALUE_EXT("ADC1 DIG MIXER ADC2 DAT Switch",
			       ARRAY_SIZE(sunxi_switch_text), sunxi_switch_text,
			       ADC1_DMIX_SRC, ADC1_ADC2_DMXL_SRC, SND_CTL_ENUM_AUTO_MASK,
			       ac107_ctl_enum_value_get, ac107_ctl_enum_value_set),
	SND_CTL_ENUM_VALUE_EXT("ADC1 DIG MIXER ADC1 DAT Switch",
			       ARRAY_SIZE(sunxi_switch_text), sunxi_switch_text,
			       ADC2_DMIX_SRC, ADC2_ADC1_DMXL_SRC, SND_CTL_ENUM_AUTO_MASK,
			       ac107_ctl_enum_value_get, ac107_ctl_enum_value_set),
	SND_CTL_ENUM_VALUE_EXT("ADC1 DIG MIXER ADC1 DAT Switch",
			       ARRAY_SIZE(sunxi_switch_text), sunxi_switch_text,
			       ADC2_DMIX_SRC, ADC2_ADC2_DMXL_SRC, SND_CTL_ENUM_AUTO_MASK,
			       ac107_ctl_enum_value_get, ac107_ctl_enum_value_set),
};

static void ac107_codec_init(struct twi_device *twi_dev, struct ac107_param *param)
{
	unsigned int i;

	snd_err("\n");

	/* adc digita volume set */
	ac107_write(&param->twi_dev, ADC1_DVOL_CTRL, 0xa0);
	ac107_write(&param->twi_dev, ADC2_DVOL_CTRL, 0xa0);
	/* adc pga gain set */
	ac107_update_bits(&param->twi_dev, ANA_ADC1_CTRL3,
			  0x1f << RX1_PGA_GAIN_CTRL,
			  0x1a<<RX1_PGA_GAIN_CTRL);
	ac107_update_bits(&param->twi_dev, ANA_ADC2_CTRL3,
			  0x1f << RX2_PGA_GAIN_CTRL,
			  0x1a<<RX2_PGA_GAIN_CTRL);
	/* I2S: SDO drive data and SDI sample data at the different BCLK edge */
	ac107_update_bits(&param->twi_dev, I2S_BCLK_CTRL,
			  0x1 << EDGE_TRANSFER, 0x0 << EDGE_TRANSFER);

	/* I2S:
	 * disable encoding mode
	 * normal mode for the last half cycle of BCLK in the slot
	 * Turn to hi-z state (TDM) when not transferring slot
	 */
	ac107_update_bits(&param->twi_dev, I2S_FMT_CTRL1,
			  0x1 << ENCD_SEL, 0x0 << ENCD_SEL);
	ac107_update_bits(&param->twi_dev, I2S_FMT_CTRL1,
			  0x1 << TX_SLOT_HIZ, 0x0 << TX_SLOT_HIZ);
	ac107_update_bits(&param->twi_dev, I2S_FMT_CTRL1,
			  0x1 << TX_STATE, 0x0 << TX_STATE);

	/* I2S: TX MSB first, SDOUT normal, PCM frame type, Linear PCM Data Mode */
	ac107_update_bits(&param->twi_dev, I2S_FMT_CTRL3,
			   0x1 << SDOUT_MUTE | 0x3 << TX_PDM,
			   0x0 << SDOUT_MUTE | 0x0 << TX_PDM);
	ac107_update_bits(&param->twi_dev, I2S_FMT_CTRL3,
			  0x1 << TX_MLS, param->pcm_bit_first << TX_MLS);

	/* I2S: adc channel map to i2s channel */
	for (i = 0; i < 8; i++)
		ac107_update_bits(&param->twi_dev, I2S_TX_CHMP_CTRL1, 0x1 << i,
				  (param->tx_pin_chmap0 >> (i * 4)) & 0x01);
	for (i = 0; i < 8; i++)
		ac107_update_bits(&param->twi_dev, I2S_TX_CHMP_CTRL2, 0x1 << i,
				 (param->tx_pin_chmap1 >> (i * 4)) & 0x01);

	/* disable PDM (I2S use default) */
	ac107_update_bits(&param->twi_dev, PDM_CTRL, 0x1 << PDM_EN, 0x0 << PDM_EN);

	/* HPF enable */
	ac107_update_bits(&param->twi_dev, HPF_EN, 0x1 << DIG_ADC1_HPF_EN, 0x1 << DIG_ADC1_HPF_EN);
	ac107_update_bits(&param->twi_dev, HPF_EN, 0x1 << DIG_ADC2_HPF_EN, 0x1 << DIG_ADC2_HPF_EN);

	/* ADC PTN sel: 0->normal, 1->0x5A5A5A, 2->0x123456, 3->0x000000 */
	ac107_update_bits(&param->twi_dev, ADC_DIG_DEBUG, 0x7 << ADC_PTN_SEL, 0x0 << ADC_PTN_SEL);
}

static int ac107_codec_probe(struct snd_codec *codec)
{
	twi_status_t ret = 0;
	struct ac107_priv *ac107 = NULL;
	// TODO
	struct ac107_param default_param = {
		.twi_dev		= AC107_CHIP_TWI_CFG,
		.daudio_master		= AC107_DAUDIO_MASTER,
		.daudio_format		= AC107_DAUDIO_FORMAT,
		.signal_inversion 	= AC107_DAUDIO_SIG_INV,
		.slot_width		= AC107_SLOT_WIDTH,
		.slots			= AC107_SLOT,
		.pllclk_src		= AC107_PLLCLK_SRC,
		.sysclk_src		= AC107_SYSCLK_SRC,
		.pcm_bit_first		= AC107_PCM_BIT,
		.tx_pin_chmap0		= AC107_TX_PIN_CHMAP,
		.tx_pin_chmap1		= AC107_TX_PIN_CHMAP,
		.frame_sync_width	= AC107_FRAME_SYNC_WIDTH,
	};

	snd_print("\n");

	if (!codec->codec_dai) {
		snd_err("codec->codec_dai is null.\n");
		return -EFAULT;
	}

	ac107 = snd_malloc(sizeof(struct ac107_priv));
	if (!ac107) {
		snd_err("no memory\n");
		return -ENOMEM;
	}

	snd_print("codec para init.\n");
	codec->private_data = (void *)ac107;
	ac107->param = default_param;
	ac107->codec = codec;
	codec->codec_dai->component = codec;

	snd_print("init ac107 i2c port.\n");
	ret = ac107_init_i2c_device(ac107->param.twi_dev.bus);
	if (ret != TWI_STATUS_OK) {
		snd_err("init i2c err\n");
		ret = -EFAULT;
		goto err_twi_init;
	}

	snd_print("ac107 codec register finished.\n");

	ac107_codec_init(&ac107->param.twi_dev, &ac107->param);

	return 0;

err_twi_init:
	snd_free(ac107);

	return ret;
}


static int ac107_codec_remove(struct snd_codec *codec)
{
	struct ac107_priv *ac107 = codec->private_data;
	struct ac107_param *param = &ac107->param;
	int ret = 0;

	snd_print("deinit ac107 i2c port.\n");
	ret = ac107_deinit_i2c_device(param->twi_dev.bus);
	if (ret != TWI_STATUS_OK) {
		snd_err("i2c deinit port %d failed.\n", param->twi_dev.bus);
	}
	snd_free(ac107);
	codec->private_data = NULL;

	return 0;
}

struct snd_codec ac107_codec = {
	.name		= "ac107-codec",
	.codec_dai	= ac107_codec_dai,
	.codec_dai_num	= ARRAY_SIZE(ac107_codec_dai),
	.private_data	= NULL,
	.probe		= ac107_codec_probe,
	.remove		= ac107_codec_remove,
	.controls	= ac107_codec_controls,
	.num_controls	= ARRAY_SIZE(ac107_codec_controls),
};


/* for ac107 debug */
void ac107_reg_dump_usage(void)
{
	printf("\n\n=========ac107 debug===========\n");
	printf("Usage: ac107_reg [option]\n");
	printf("\t-l,	 ac107 dev list\n");
	printf("\t-h,	 tools help\n");
	printf("\t-d,	 ac107 dev addr(hex)\n");
	printf("\t-r,	 ac107 reg addr(hex)\n");
	printf("\t-n,	 ac107 reg read num(hex)\n");
	printf("\t-s,	 ac107 show all regs\n");
	printf("\t-w,	 ac107 write reg val(hex)\n");
	printf("\n");
}

void ac107_chip_list(void)
{
	unsigned int chip_num = AC107_CHIP_NUM;
	struct twi_device twi_dev = AC107_CHIP_TWI_CFG;
	int i = 0;

	printf("\n\n");
	printf("========= ac107 show =========\n");
	printf("\tac107 dev num:\t%d\n", chip_num);
	for (i = 0; i < chip_num; i++)
		printf("\t%d i2c%d-0x%02x\n", i, twi_dev.bus, twi_dev.addr);
	printf("===============================\n");
}

void ac107_reg_show(void)
{
	struct twi_device twi_dev = AC107_CHIP_TWI_CFG;
	unsigned char read_command;
	unsigned char read_data[1] = {0x0};
	twi_status_t ret = 0;
	unsigned int j = 0;

	printf("\n====== ac107 chip [i2c%d-0x%02x] ======\n", twi_dev.bus, twi_dev.addr);
	for (j = 0; j <= AC107_MAX_REG; j++) {
		if (j % 8 == 0)
			printf("\n");
		read_command = 0x0 + j;
		ret = ac107_read(&twi_dev, read_command, read_data);
		if (ret != TWI_STATUS_OK) {
			snd_err("[i2c%d-0x%02x] read [REG-0x%02x,val-0x%02x] ret = %d.\n",
				twi_dev.bus, twi_dev.addr,
				read_command, read_data[0], ret);
		}
		printf("[0x%02x]: 0x%02x  ", read_command, read_data[0]);
	}
	printf("\n========================================\n");
}

int cmd_ac107_reg(int argc, char ** argv)
{
	unsigned int chip_num = AC107_CHIP_NUM;
	twi_status_t ret = -1;
	unsigned int i;
	const struct option long_option[] = {
		{"help", 0, NULL, 'h'},
		{"list", 0, NULL, 'l'},
		{"addr", 1, NULL, 'd'},
		{"reg", 1, NULL, 'r'},
		{"num", 1, NULL, 'n'},
		{"show", 0, NULL, 's'},
		{"write", 1, NULL, 'w'},
		{NULL, 0, NULL, 0},
	};
	struct twi_device twi_dev = AC107_CHIP_TWI_CFG;
	/*unsigned char reset_cmd[2] = {0x0, 0x12};*/
	unsigned char write_cmd[2] = {0x0, 0x0};
	unsigned char read_cmd[1] = {0x0};
	unsigned char read_data[1] = {0x0};
	unsigned int num = 1;
	unsigned int twi_addr = twi_dev.addr;
	bool wr_flag = 0;

	while (1) {
		int c;

		if ((c = getopt_long(argc, argv, "hlsd:r:n:w:", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h':
			ac107_reg_dump_usage();
			goto ac107_reg_exit;
		case 'l':
			ac107_chip_list();
			goto ac107_reg_exit;
		case 's':
			ac107_reg_show();
			goto ac107_reg_exit;
		case 'd':
			if (isdigit(*optarg)) {
				sscanf(optarg, "0x%x", &twi_addr);
				printf("\ntwi_addr slave address is 0x%02x.\n", twi_addr);
			} else
				fprintf(stderr, "twi addr is not a digital value.\n");
			break;
		case 'r':
			if (isdigit(*optarg)) {
				sscanf(optarg, "0x%hhx", &read_cmd[0]);
				write_cmd[0] = read_cmd[0];
				printf("\nreg is 0x%02x.\n", read_cmd[0]);
			} else
				fprintf(stderr, "reg is not a digital value.\n");
			break;
		case 'n':
			if (isdigit(*optarg)) {
				sscanf(optarg, "0x%x", &num);
				printf("\nnum is %d.\n", num);
			} else
				fprintf(stderr, "num is not a digital value.\n");
			break;
		case 'w':
			if (isdigit(*optarg)) {
				wr_flag = 1;
				sscanf(optarg, "0x%02x 0x%02x", (unsigned int *)&write_cmd[0], (unsigned int *)&write_cmd[1]);
				printf("\nwrite reg is 0x%02x, val is 0x%02x.\n",
					write_cmd[0], write_cmd[1]);
			} else
				fprintf(stderr, "write val is not a digital value.\n");
			break;
		default:
			fprintf(stderr, "Invalid switch or option needs an argument.\n");
			break;
		}
	}

	//checkout i2c port and addr.
	for (i = 0; i < chip_num; i++) {
		if (twi_addr == twi_dev.addr)
			break;
	}
	if (i >= chip_num) {
		fprintf(stderr, "the addr is error.\n");
		goto ac107_reg_exit;
	}
	if ((read_cmd[0] > AC107_MAX_REG) || (write_cmd[0] > AC107_MAX_REG)) {
		fprintf(stderr, "the reg is over 0x%02x error.\n", AC107_MAX_REG);
		goto ac107_reg_exit;
	}

	if (wr_flag) {
		ret = ac107_write(&twi_dev, write_cmd[0], write_cmd[1]);
		if (ret != TWI_STATUS_OK) {
			snd_err("write error [REG-0x%02x,val-0x%02x] ret = %d.\n",
				write_cmd[0], write_cmd[1], ret);
		}
		ret = ac107_read(&twi_dev, write_cmd[0], read_data);
		if (ret != TWI_STATUS_OK) {
			snd_err("write error [I2C%d-0x%0x] REG=0x%02x, val=0x%02x] ret = %d.\n",
				twi_dev.bus, twi_dev.addr, write_cmd[0], read_data[0], ret);
			goto ac107_reg_exit;
		}
		if (read_data[0] == write_cmd[1]) {
			printf("write success, [I2C%d-0x%0x] REG=0x%02x, val=0x%02x] ret = %d.\n",
				twi_dev.bus, twi_dev.addr, write_cmd[0], read_data[0], ret);
		} else {
			printf("write val:0x%02x failed, [I2C%d-0x%0x] REG=0x%02x, val=0x%02x] ret = %d.\n",
				write_cmd[1], twi_dev.bus, twi_dev.addr, write_cmd[0], read_data[0], ret);
		}
	} else {
		for (i = 0; i < num; i++) {
			ret = ac107_read(&twi_dev, read_cmd[0], read_data);
			if (ret != TWI_STATUS_OK) {
				snd_err("read error [I2C%d-0x%0x] REG=0x%02x, val=0x%02x] ret = %d.\n",
					twi_dev.bus, twi_dev.addr, read_cmd[0], read_data[0], ret);
				goto ac107_reg_exit;
			} else {
				printf("read success. [I2C%d-0x%0x] REG=0x%02x, val=0x%02x].\n",
					twi_dev.bus, twi_dev.addr, read_cmd[0], read_data[0]);
			}
		}
	}

ac107_reg_exit:
	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_ac107_reg, ac107_reg, ac107 regs dump);
