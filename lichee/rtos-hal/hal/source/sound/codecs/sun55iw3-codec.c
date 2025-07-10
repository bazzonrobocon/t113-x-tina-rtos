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
#include <stdlib.h>
#include <string.h>
#include <hal_gpio.h>
#include <hal_dma.h>
#include <hal_clk.h>
#include <hal_reset.h>
#include <hal_time.h>
#ifdef CONFIG_DRIVER_SYSCONFIG
#include <script.h>
#include <hal_cfg.h>
#endif
#include <sound/snd_core.h>
#include <sound/snd_pcm.h>
#include <sound/snd_dma.h>
#include <sound/dma_wrap.h>
#include <sound/pcm_common.h>
#include <sound/common/snd_sunxi_common.h>
#include <sound/snd_io.h>

#include "sun55iw3-codec.h"

struct snd_codec sunxi_audiocodec;

struct sample_rate {
	unsigned int samplerate;
	unsigned int rate_bit;
};


static struct sunxi_codec_param default_param = {
	.dac_vol	= 63,
	.dacl_vol	= 160,
	.dacr_vol	= 160,
	.adc1_vol	= 160,
	.adc2_vol	= 160,
	.adc3_vol	= 160,
	.lineout_gain	= 31,
	.hpout_gain	= 7,
	.adc1_gain	= 31,
	.adc2_gain	= 31,
	.adc3_gain	= 31,
	.mic1_en	= true,
	.mic2_en	= false,
	.mic3_en	= false,
	.lineoutl_en	= true,
	.lineoutr_en	= true,
	.spk_en		= true,
	.hpout_en	= false,
	.tx_hub_en	= false,
	.rx_sync_en	= false,
};

static struct sunxi_pa_config default_pa_cfg = {
	.gpio 		= GPIOH(6),
	.drv_level	= GPIO_DRIVING_LEVEL1,
	.mul_sel	= GPIO_MUXSEL_OUT,
	.data		= GPIO_DATA_HIGH,
	.pa_msleep_time	= 0,
};

static const struct sample_rate sunxi_sample_rate_conv[] = {
	{44100, 0},
	{48000, 0},
	{8000, 5},
	{32000, 1},
	{22050, 2},
	{24000, 2},
	{16000, 3},
	{11025, 4},
	{12000, 4},
	{192000, 6},
	{96000, 7},
};

static const char *const codec_switch_onoff[] = {"off", "on"};
/* static const char *sunxi_adda_loop_mode_text[] = {"Off", "DACLR-to-ADC12", "DACL-to-ADC3"}; */

static int snd_sunxi_clk_init(struct sunxi_codec_clk *clk);
static void snd_sunxi_clk_exit(struct sunxi_codec_clk *clk);
static int snd_sunxi_clk_enable(struct sunxi_codec_clk *clk);
static void snd_sunxi_clk_disable(struct sunxi_codec_clk *clk);
static int snd_sunxi_clk_set_rate(struct sunxi_codec_clk *clk, int stream,
				  unsigned int freq_in, unsigned int freq_out);

/* static int sunxi_codec_get_rx_sync_status(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_info *info)
{

	return 0;
}

static int sunxi_codec_set_rx_sync_status(struct snd_kcontrol *kcontrol,
					  unsigned long value)
{

	return 0;
}

static int sunxi_codec_get_adda_loop_mode(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_info *info)
{

	return 0;
}

static int sunxi_codec_set_adda_loop_mode(struct snd_kcontrol *kcontrol,
					  unsigned long value)
{

	return 0;
} */

static int sunxi_codec_lienoutl_event(struct snd_codec *codec, bool enable)
{
	if (enable) {
		snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
				      0x1 << LMUTE, 0x1 << LMUTE);
		snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
				      0x1 << LINEOUTL_EN, 0x1 << LINEOUTL_EN);
	} else {
		snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
				      0x1 << LINEOUTL_EN, 0x0 << LINEOUTL_EN);
		snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
				      0x1 << LMUTE, 0x0 << LMUTE);
	}

	return 0;
}

static int sunxi_codec_lienoutr_event(struct snd_codec *codec, bool enable)
{
	if (enable) {
		snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
				      0x1 << RMUTE, 0x1 << RMUTE);
		snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
				      0x1 << LINEOUTR_EN, 0x1 << LINEOUTR_EN);
	} else {
		snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
				      0x1 << LINEOUTR_EN, 0x0 << LINEOUTR_EN);
		snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
				      0x1 << RMUTE, 0x0 << RMUTE);
	}

	return 0;
}

static int sunxi_codec_spk_event(struct snd_codec *codec, bool enable)
{
	struct sunxi_codec_info *sunxi_codec = codec->private_data;

	if (enable) {
		snd_sunxi_pa_enable(&sunxi_codec->pa_cfg);
	} else {
		snd_sunxi_pa_disable(&sunxi_codec->pa_cfg);
	}

	return 0;
}

static int sunxi_codec_hpout_event(struct snd_codec *codec, bool enable)
{
	if (enable) {
		snd_codec_update_bits(codec, SUNXI_HP_AN_CTL,
				      0x1 << HPPA_EN, 0x1 << HPPA_EN);
	} else {
		snd_codec_update_bits(codec, SUNXI_HP_AN_CTL,
				      0x1 << HPPA_EN, 0x0 << HPPA_EN);
	}

	return 0;
}

static int sunxi_codec_mic1_event(struct snd_codec *codec, bool enable)
{
	struct sunxi_codec_info *sunxi_codec = codec->private_data;

	if (enable) {
		snd_codec_update_bits(codec, SUNXI_ADC1_AN_CTL,
				      0x1 << MIC1_PGA_EN, 0x1 << MIC1_PGA_EN);
		snd_codec_update_bits(codec, SUNXI_ADC1_AN_CTL,
				      0x1 << ADC1_EN, 0x1 << ADC1_EN);
		sunxi_codec->mic_sts.mic1 = true;
		if (sunxi_codec->mic_sts.mic2 == false && sunxi_codec->mic_sts.mic3 == false) {
			snd_codec_update_bits(codec, SUNXI_MICBIAS_AN_CTL,
					      0x1 << MMIC_BIAS_EN, 0x1 << MMIC_BIAS_EN);
			/* delay 240ms to avoid pop recording in begining,
			 * and adc fifo delay time must not be less than 20ms.
			 */
			hal_msleep(240);
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					   0x1 << ADC_DIG_EN, 0x1 << ADC_DIG_EN);
		}
	} else {
		sunxi_codec->mic_sts.mic1 = false;
		if (sunxi_codec->mic_sts.mic2 == false && sunxi_codec->mic_sts.mic3 == false) {
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					      0x1 << ADC_DIG_EN, 0x0 << ADC_DIG_EN);
			snd_codec_update_bits(codec, SUNXI_MICBIAS_AN_CTL,
					      0x1 << MMIC_BIAS_EN, 0x0 << MMIC_BIAS_EN);
		}
		snd_codec_update_bits(codec, SUNXI_ADC1_AN_CTL,
				      0x1 << ADC1_EN, 0x0 << ADC1_EN);
		snd_codec_update_bits(codec, SUNXI_ADC1_AN_CTL,
				      0x1 << MIC1_PGA_EN, 0x0 << MIC1_PGA_EN);
	}

	return 0;
}

static int sunxi_codec_mic2_event(struct snd_codec *codec, bool enable)
{
	struct sunxi_codec_info *sunxi_codec = codec->private_data;

	if (enable) {
		snd_codec_update_bits(codec, SUNXI_ADC2_AN_CTL,
				      0x1 << MIC2_PGA_EN, 0x1 << MIC2_PGA_EN);
		snd_codec_update_bits(codec, SUNXI_ADC2_AN_CTL,
				      0x1 << ADC2_EN, 0x1 << ADC2_EN);
		sunxi_codec->mic_sts.mic2 = true;
		if (sunxi_codec->mic_sts.mic1 == false && sunxi_codec->mic_sts.mic3 == false) {
			snd_codec_update_bits(codec, SUNXI_MICBIAS_AN_CTL,
					      0x1 << MMIC_BIAS_EN, 0x1 << MMIC_BIAS_EN);
			/* delay 24ms to avoid pop recording in begining,
			 * and adc fifo delay time must not be less than 20ms.
			 */
			hal_msleep(240);
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					      0x1 << ADC_DIG_EN, 0x1 << ADC_DIG_EN);
		}
	} else {
		sunxi_codec->mic_sts.mic2 = false;
		if (sunxi_codec->mic_sts.mic1 == false && sunxi_codec->mic_sts.mic3 == false) {
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					      0x1 << ADC_DIG_EN, 0x0 << ADC_DIG_EN);
			snd_codec_update_bits(codec, SUNXI_MICBIAS_AN_CTL,
					      0x1 << MMIC_BIAS_EN, 0x0 << MMIC_BIAS_EN);
		}
		snd_codec_update_bits(codec, SUNXI_ADC2_AN_CTL,
				      0x1 << ADC2_EN, 0x0 << ADC2_EN);
		snd_codec_update_bits(codec, SUNXI_ADC2_AN_CTL,
				      0x1 << MIC2_PGA_EN, 0x0 << MIC2_PGA_EN);
	}

	return 0;
}

static int sunxi_codec_mic3_event(struct snd_codec *codec, bool enable)
{
	struct sunxi_codec_info *sunxi_codec = codec->private_data;

	if (enable) {
		snd_codec_update_bits(codec, SUNXI_ADC3_AN_CTL,
				      0x1 << MIC3_PGA_EN, 0x1 << MIC3_PGA_EN);
		snd_codec_update_bits(codec, SUNXI_ADC3_AN_CTL,
				      0x1 << ADC3_EN, 0x1 << ADC3_EN);
		sunxi_codec->mic_sts.mic3 = true;
		if (sunxi_codec->mic_sts.mic1 == false && sunxi_codec->mic_sts.mic2 == false) {
			snd_codec_update_bits(codec, SUNXI_MICBIAS_AN_CTL,
					   0x1 << MMIC_BIAS_EN, 0x1 << MMIC_BIAS_EN);
			/* delay 240ms to avoid pop recording in begining,
			 * and adc fifo delay time must not be less than 20ms.
			 */
			hal_msleep(240);
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					   0x1 << ADC_DIG_EN, 0x1 << ADC_DIG_EN);
		}
	} else {
		sunxi_codec->mic_sts.mic3 = false;
		if (sunxi_codec->mic_sts.mic1 == false && sunxi_codec->mic_sts.mic2 == false) {
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					   0x1 << ADC_DIG_EN, 0x0 << ADC_DIG_EN);
			snd_codec_update_bits(codec, SUNXI_MICBIAS_AN_CTL,
					   0x1 << MMIC_BIAS_EN, 0x0 << MMIC_BIAS_EN);
		}
		snd_codec_update_bits(codec, SUNXI_ADC3_AN_CTL,
				      0x1 << ADC3_EN, 0x0 << ADC3_EN);
		snd_codec_update_bits(codec, SUNXI_ADC3_AN_CTL,
				      0x1 << MIC3_PGA_EN, 0x0 << MIC3_PGA_EN);
	}

	return 0;
}

static int sunxi_codec_playback_event(struct snd_codec *codec, bool enable)
{
	if (enable) {
		snd_codec_update_bits(codec, SUNXI_DAC_DPC,
				      0x1 << DAC_DIG_EN, 0x1 << DAC_DIG_EN);
	} else {
		snd_codec_update_bits(codec, SUNXI_DAC_DPC,
				      0x1 << DAC_DIG_EN, 0x0 << DAC_DIG_EN);
	}

	return 0;
}


/* TODO */
static int sunxi_codec_dapm_control(struct snd_pcm_substream *substream, struct snd_dai *dai, int onoff)
{
	struct snd_codec *codec = dai->component;
	struct sunxi_codec_info *sunxi_codec = codec->private_data;
	struct sunxi_codec_param *codec_param = &sunxi_codec->param;

	snd_print("\n");

	if (substream->dapm_state == onoff)
		return 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		codec_param->playback_dapm = onoff;

		if (onoff) {

			/* hp*/
			if (codec_param->hpout_en)
				sunxi_codec_hpout_event(codec, true);

			if (codec_param->lineoutl_en) {
				snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
						      0x1 << DACL_EN,
						      0x1 << DACL_EN);
				sunxi_codec_lienoutl_event(codec, true);
			}
			if (codec_param->lineoutr_en) {
				snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
						      0x1 << DACR_EN,
						      0x1 << DACR_EN);
				sunxi_codec_lienoutr_event(codec, true);
			}

			sunxi_codec_playback_event(codec, true);

			if (codec_param->spk_en)
				sunxi_codec_spk_event(codec, true);

		} else {
			if (codec_param->spk_en)
				sunxi_codec_spk_event(codec, false);

			sunxi_codec_playback_event(codec, false);

			if (codec_param->lineoutl_en)
				sunxi_codec_lienoutl_event(codec, false);
			if (codec_param->lineoutr_en)
				sunxi_codec_lienoutr_event(codec, false);

			if (codec_param->hpout_en)
				sunxi_codec_hpout_event(codec, false);
		}
	} else {
		codec_param->capture_dapm = onoff;

		if (onoff) {

			if (substream->runtime->channels > 3 || substream->runtime->channels < 1) {
				snd_err("unsupport channels: %d!\n", substream->runtime->channels);
				return -EINVAL;
			}

			if (codec_param->mic1_en)
				sunxi_codec_mic1_event(codec, true);
			if (codec_param->mic2_en)
				sunxi_codec_mic2_event(codec, true);
			if (codec_param->mic3_en)
				sunxi_codec_mic3_event(codec, true);
		} else {
			if (codec_param->mic1_en)
				sunxi_codec_mic1_event(codec, false);
			if (codec_param->mic2_en)
				sunxi_codec_mic2_event(codec, false);
			if (codec_param->mic3_en)
				sunxi_codec_mic3_event(codec, false);
		}
	}

	substream->dapm_state = onoff;

	return 0;

}

static int sunxi_get_mic_ch(struct snd_kcontrol *kcontrol, struct snd_ctl_info *info)
{
	unsigned int val = 0;

	snd_print("\n");

	if (kcontrol->type != SND_CTL_ELEM_TYPE_ENUMERATED) {
		snd_err("invalid kcontrol type = %d.\n", kcontrol->type);
		return -EINVAL;
	}

	if (kcontrol->private_data_type == SND_MODULE_CODEC) {
		struct snd_codec *codec = kcontrol->private_data;
		struct sunxi_codec_info *sunxi_codec = codec->private_data;
		struct sunxi_codec_param *codec_param = &sunxi_codec->param;

		if (kcontrol->mask == SND_CTL_ENUM_MIC1_MASK)
			val = codec_param->mic1_en << 0;
		else if (kcontrol->mask == SND_CTL_ENUM_MIC2_MASK)
			val = codec_param->mic2_en << 1;
		else if (kcontrol->mask == SND_CTL_ENUM_MIC3_MASK)
			val = codec_param->mic3_en << 2;
	} else {
		snd_err("%s invalid kcontrol data type = %d.\n", __func__,
			kcontrol->private_data_type);
	}

	snd_kcontrol_to_snd_ctl_info(kcontrol, info, val);

	return 0;
}

static int sunxi_set_mic_ch(struct snd_kcontrol *kcontrol, unsigned long value)
{
	snd_print("\n");

	if (kcontrol->type != SND_CTL_ELEM_TYPE_ENUMERATED) {
		snd_err("invalid kcontrol type = %d.\n", kcontrol->type);
		return -EINVAL;
	}

	if (value >= kcontrol->items) {
		snd_err("invalid kcontrol items = %ld.\n", value);
		return -EINVAL;
	}

	if (kcontrol->private_data_type == SND_MODULE_CODEC) {
		struct snd_codec *codec = kcontrol->private_data;
		struct sunxi_codec_info *sunxi_codec = codec->private_data;
		struct sunxi_codec_param *codec_param = &sunxi_codec->param;

		if (kcontrol->mask == SND_CTL_ENUM_MIC1_MASK) {
			codec_param->mic1_en = value;
			if (codec_param->capture_dapm)
				sunxi_codec_mic1_event(codec, value);
		} else if (kcontrol->mask == SND_CTL_ENUM_MIC2_MASK) {
			codec_param->mic2_en = value;
			if (codec_param->capture_dapm)
				sunxi_codec_mic2_event(codec, value);
		} else if (kcontrol->mask == SND_CTL_ENUM_MIC3_MASK) {
			codec_param->mic3_en = value;
			if (codec_param->capture_dapm)
				sunxi_codec_mic3_event(codec, value);
		}
	} else {
		snd_err("invalid kcontrol data type = %d.\n", kcontrol->private_data_type);
	}
	snd_info("mask:0x%x, items:%d, value:0x%x\n", kcontrol->mask, kcontrol->items, value);

	return 0;
}

static int sunxi_get_lineout_ch(struct snd_kcontrol *kcontrol, struct snd_ctl_info *info)
{
	unsigned int val = 0;

	snd_print("\n");

	if (kcontrol->type != SND_CTL_ELEM_TYPE_ENUMERATED) {
		snd_err("invalid kcontrol type = %d.\n", kcontrol->type);
		return -EINVAL;
	}

	if (kcontrol->private_data_type == SND_MODULE_CODEC) {
		struct snd_codec *codec = kcontrol->private_data;
		struct sunxi_codec_info *sunxi_codec = codec->private_data;
		struct sunxi_codec_param *codec_param = &sunxi_codec->param;

		if (kcontrol->mask == SND_CTL_ENUM_LINEOUTL_MASK)
			val = codec_param->lineoutl_en << 0;
		else if (kcontrol->mask == SND_CTL_ENUM_LINEOUTR_MASK)
			val = codec_param->lineoutr_en << 1;
	} else {
		snd_err("%s invalid kcontrol data type = %d.\n", __func__,
			kcontrol->private_data_type);
	}

	snd_kcontrol_to_snd_ctl_info(kcontrol, info, val);

	return 0;
}

static int sunxi_set_lineout_ch(struct snd_kcontrol *kcontrol, unsigned long value)
{
	snd_print("\n");

	if (kcontrol->type != SND_CTL_ELEM_TYPE_ENUMERATED) {
		snd_err("invalid kcontrol type = %d.\n", kcontrol->type);
		return -EINVAL;
	}

	if (value >= kcontrol->items) {
		snd_err("invalid kcontrol items = %ld.\n", value);
		return -EINVAL;
	}

	if (kcontrol->private_data_type == SND_MODULE_CODEC) {
		struct snd_codec *codec = kcontrol->private_data;
		struct sunxi_codec_info *sunxi_codec = codec->private_data;
		struct sunxi_codec_param *codec_param = &sunxi_codec->param;

		if (kcontrol->mask == SND_CTL_ENUM_LINEOUTL_MASK) {
			codec_param->lineoutl_en = value;
			/* TODO */
			if (codec_param->playback_dapm)
				sunxi_codec_lienoutl_event(codec, value);
		} else if (kcontrol->mask == SND_CTL_ENUM_LINEOUTR_MASK) {
			codec_param->lineoutr_en = value;
			if (codec_param->playback_dapm)
				sunxi_codec_lienoutr_event(codec, value);
		}
	} else {
		snd_err("invalid kcontrol data type = %d.\n", kcontrol->private_data_type);
	}
	snd_info("mask:0x%x, items:%d, value:0x%x\n", kcontrol->mask, kcontrol->items, value);

	return 0;
}

static int sunxi_get_spk_ch(struct snd_kcontrol *kcontrol, struct snd_ctl_info *info)
{
	unsigned int val = 0;

	snd_print("\n");

	if (kcontrol->type != SND_CTL_ELEM_TYPE_ENUMERATED) {
		snd_err("invalid kcontrol type = %d.\n", kcontrol->type);
		return -EINVAL;
	}

	if (kcontrol->private_data_type == SND_MODULE_CODEC) {
		struct snd_codec *codec = kcontrol->private_data;
		struct sunxi_codec_info *sunxi_codec = codec->private_data;
		struct sunxi_codec_param *codec_param = &sunxi_codec->param;

		val = codec_param->spk_en << 0;
	} else {
		snd_err("%s invalid kcontrol data type = %d.\n", __func__,
			kcontrol->private_data_type);
	}

	snd_kcontrol_to_snd_ctl_info(kcontrol, info, val);

	return 0;
}

static int sunxi_set_spk_ch(struct snd_kcontrol *kcontrol, unsigned long value)
{
	snd_print("\n");

	if (kcontrol->type != SND_CTL_ELEM_TYPE_ENUMERATED) {
		snd_err("invalid kcontrol type = %d.\n", kcontrol->type);
		return -EINVAL;
	}

	if (value >= kcontrol->items) {
		snd_err("invalid kcontrol items = %ld.\n", value);
		return -EINVAL;
	}

	if (kcontrol->private_data_type == SND_MODULE_CODEC) {
		struct snd_codec *codec = kcontrol->private_data;
		struct sunxi_codec_info *sunxi_codec = codec->private_data;
		struct sunxi_codec_param *codec_param = &sunxi_codec->param;

		codec_param->spk_en = value;
		/* TODO */
		if (codec_param->playback_dapm)
			sunxi_codec_spk_event(codec, value);
	} else {
		snd_err("invalid kcontrol data type = %d.\n", kcontrol->private_data_type);
	}
	snd_info("mask:0x%x, items:%d, value:0x%x\n", kcontrol->mask, kcontrol->items, value);

	return 0;
}

static int sunxi_get_hpout_ch(struct snd_kcontrol *kcontrol, struct snd_ctl_info *info)
{
	unsigned int val = 0;

	snd_print("\n");

	if (kcontrol->type != SND_CTL_ELEM_TYPE_ENUMERATED) {
		snd_err("invalid kcontrol type = %d.\n", kcontrol->type);
		return -EINVAL;
	}

	if (kcontrol->private_data_type == SND_MODULE_CODEC) {
		struct snd_codec *codec = kcontrol->private_data;
		struct sunxi_codec_info *sunxi_codec = codec->private_data;
		struct sunxi_codec_param *codec_param = &sunxi_codec->param;

		val = codec_param->hpout_en << 0;
	} else {
		snd_err("%s invalid kcontrol data type = %d.\n", __func__,
			kcontrol->private_data_type);
	}

	snd_kcontrol_to_snd_ctl_info(kcontrol, info, val);

	return 0;
}

static int sunxi_set_hpout_ch(struct snd_kcontrol *kcontrol, unsigned long value)
{
	snd_print("\n");

	if (kcontrol->type != SND_CTL_ELEM_TYPE_ENUMERATED) {
		snd_err("invalid kcontrol type = %d.\n", kcontrol->type);
		return -EINVAL;
	}

	if (value >= kcontrol->items) {
		snd_err("invalid kcontrol items = %ld.\n", value);
		return -EINVAL;
	}

	if (kcontrol->private_data_type == SND_MODULE_CODEC) {
		struct snd_codec *codec = kcontrol->private_data;
		struct sunxi_codec_info *sunxi_codec = codec->private_data;
		struct sunxi_codec_param *codec_param = &sunxi_codec->param;

		codec_param->hpout_en = value;
		/* TODO */
		if (codec_param->playback_dapm)
			sunxi_codec_hpout_event(codec, value);
	} else {
		snd_err("invalid kcontrol data type = %d.\n", kcontrol->private_data_type);
	}
	snd_info("mask:0x%x, items:%d, value:0x%x\n", kcontrol->mask, kcontrol->items, value);

	return 0;
}

static struct snd_kcontrol sunxi_codec_controls[] = {
	/* volume */
	SND_CTL_KCONTROL("DAC Volume", SUNXI_DAC_DPC, DVOL, 0x3F),
	SND_CTL_KCONTROL("DACL Volume", SUNXI_DAC_VOL_CTL, DAC_VOL_L, 0xFF),
	SND_CTL_KCONTROL("DACR Volume", SUNXI_DAC_VOL_CTL, DAC_VOL_R, 0xFF),
	SND_CTL_KCONTROL("ADC1 Volume", SUNXI_ADC_VOL_CTL1, ADC1_VOL, 0xFF),
	SND_CTL_KCONTROL("ADC2 Volume", SUNXI_ADC_VOL_CTL1, ADC2_VOL, 0xFF),
	SND_CTL_KCONTROL("ADC3 Volume", SUNXI_ADC_VOL_CTL1, ADC3_VOL, 0xFF),

	SND_CTL_KCONTROL("LINEOUT Gain", SUNXI_DAC_AN_REG, LINEOUT_GAIN, 0x1F),
	SND_CTL_KCONTROL("HPOUT Gain", SUNXI_DAC_AN_REG, HEADPHONE_GAIN, 0x7),
	SND_CTL_KCONTROL("ADC1 Gain", SUNXI_ADC1_AN_CTL, ADC1_PGA_GAIN_CTL, 0x1F),
	SND_CTL_KCONTROL("ADC2 Gain", SUNXI_ADC2_AN_CTL, ADC2_PGA_GAIN_CTL, 0x1F),
	SND_CTL_KCONTROL("ADC3 Gain", SUNXI_ADC3_AN_CTL, ADC3_PGA_GAIN_CTL, 0x1F),

	SND_CTL_ENUM("DACL DACR Swap", ARRAY_SIZE(codec_switch_onoff),
		     codec_switch_onoff, SUNXI_DAC_DEBUG, DA_SWP),
	SND_CTL_ENUM("ADC1 ADC2 Swap", ARRAY_SIZE(codec_switch_onoff),
		     codec_switch_onoff, SUNXI_ADC_DEBUG, ADC_SWP1),
	SND_CTL_ENUM("ADC3 ADC4 Swap", ARRAY_SIZE(codec_switch_onoff),
		     codec_switch_onoff, SUNXI_ADC_DEBUG, ADC_SWP1),

	/* SND_CTL_ENUM("TX Hub Mode", ARRAY_SIZE(codec_switch_onoff),
		     codec_switch_onoff, SUNXI_DAC_DPC, HUB_EN),
	SND_CTL_ENUM("RX Sync Mode", ARRAY_SIZE(codec_switch_onoff),
			 codec_switch_onoff, SND_CTL_ENUM_AUTO_MASK,
			 sunxi_codec_get_rx_sync_status,
			 sunxi_codec_set_rx_sync_status),
	SND_CTL_ENUM_EXT("ADDA Loop Mode", ARRAY_SIZE(sunxi_adda_loop_mode_text),
			 sunxi_adda_loop_mode_text, SND_CTL_ENUM_AUTO_MASK,
			 sunxi_codec_get_adda_loop_mode,
			 sunxi_codec_set_adda_loop_mode), */

	SND_CTL_ENUM_EXT("MIC1 Switch", ARRAY_SIZE(codec_switch_onoff),
			 codec_switch_onoff, SND_CTL_ENUM_MIC1_MASK,
			 sunxi_get_mic_ch, sunxi_set_mic_ch),
	SND_CTL_ENUM_EXT("MIC2 Switch", ARRAY_SIZE(codec_switch_onoff),
			 codec_switch_onoff, SND_CTL_ENUM_MIC2_MASK,
			 sunxi_get_mic_ch, sunxi_set_mic_ch),
	SND_CTL_ENUM_EXT("MIC3 Switch", ARRAY_SIZE(codec_switch_onoff),
			 codec_switch_onoff, SND_CTL_ENUM_MIC3_MASK,
			 sunxi_get_mic_ch, sunxi_set_mic_ch),


	SND_CTL_ENUM_EXT("LINEOUTL Switch", ARRAY_SIZE(codec_switch_onoff),
			 codec_switch_onoff, SND_CTL_ENUM_LINEOUTL_MASK,
			 sunxi_get_lineout_ch, sunxi_set_lineout_ch),
	SND_CTL_ENUM_EXT("LINEOUTR Switch", ARRAY_SIZE(codec_switch_onoff),
			 codec_switch_onoff, SND_CTL_ENUM_LINEOUTR_MASK,
			 sunxi_get_lineout_ch, sunxi_set_lineout_ch),

	SND_CTL_ENUM_EXT("SPK Switch", ARRAY_SIZE(codec_switch_onoff),
			 codec_switch_onoff, SND_CTL_ENUM_SPK_MASK,
			 sunxi_get_spk_ch, sunxi_set_spk_ch),

	SND_CTL_ENUM_EXT("HPOUT Switch", ARRAY_SIZE(codec_switch_onoff),
			 codec_switch_onoff, SND_CTL_ENUM_HPOUT_MASK,
			 sunxi_get_hpout_ch, sunxi_set_hpout_ch),


};

static int sunxi_get_adc_ch(struct snd_codec *codec)
{
	int ret = -1;
	struct sunxi_codec_info *sunxi_codec = codec->private_data;
	struct sunxi_codec_param *codec_param = &sunxi_codec->param;

	snd_print("\n");

	if (codec_param->mic1_en) {
		codec_param->adc1_en = true;
		ret = 1;
	}
	else {
		codec_param->adc1_en = false;
	}

	if (codec_param->mic2_en) {
		codec_param->adc2_en = true;
		ret = 1;
	}
	else {
		codec_param->adc2_en = false;
	}

	if (codec_param->mic3_en) {
		codec_param->adc3_en = true;
		ret = 1;
	}
	else {
		codec_param->adc3_en = false;
	}

	return ret;
}

static int sunxi_codec_startup(struct snd_pcm_substream *substream,
			       struct snd_dai *dai)
{
	snd_print("\n");

	return 0;
}

static int sunxi_codec_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_dai *dai)
{
	struct snd_codec *codec = dai->component;
	struct sunxi_codec_info *sunxi_codec = codec->private_data;
	struct sunxi_codec_param *codec_param = &sunxi_codec->param;
	unsigned int sample_channels;
	int i = 0;

	snd_print("\n");

	switch (params_format(params)) {
	case SND_PCM_FORMAT_S16_LE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			snd_codec_update_bits(codec, SUNXI_DAC_FIFO_CTL,
					      0x3 << DAC_FIFO_MODE,
					      0x3 << DAC_FIFO_MODE);
			snd_codec_update_bits(codec, SUNXI_DAC_FIFO_CTL,
					      0x1 << TX_SAMPLE_BITS,
					      0x0 << TX_SAMPLE_BITS);
		} else {
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					      0x1 << RX_FIFO_MODE,
					      0x1 << RX_FIFO_MODE);
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					      0x1 << RX_SAMPLE_BITS,
					      0x0 << RX_SAMPLE_BITS);
		}
		break;
	// case SND_PCM_FORMAT_S24_3LE:
	case SND_PCM_FORMAT_S24_LE:
	case SND_PCM_FORMAT_S32_LE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			snd_codec_update_bits(codec, SUNXI_DAC_FIFO_CTL,
					      0x3 << DAC_FIFO_MODE,
					      0x0 << DAC_FIFO_MODE);
			snd_codec_update_bits(codec, SUNXI_DAC_FIFO_CTL,
					      0x1 << TX_SAMPLE_BITS,
					      0x1 << TX_SAMPLE_BITS);
		} else {
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					      0x1 << RX_FIFO_MODE,
					      0x0 << RX_FIFO_MODE);
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					      0x1 << RX_SAMPLE_BITS,
					      0x1 << RX_SAMPLE_BITS);
		}
		break;
	default:
		break;
	}

	/* Set rate */
	for (i = 0; i < ARRAY_SIZE(sunxi_sample_rate_conv); i++) {
		if (sunxi_sample_rate_conv[i].samplerate == params_rate(params)) {
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				snd_codec_update_bits(codec, SUNXI_DAC_FIFO_CTL,
						      0x7 << DAC_FS,
						      sunxi_sample_rate_conv[i].rate_bit << DAC_FS);
			} else {
				if (sunxi_sample_rate_conv[i].samplerate > 48000)
					return -EINVAL;
				snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
						      0x7 << ADC_FS,
						      sunxi_sample_rate_conv[i].rate_bit << ADC_FS);
			}
		}
	}

	/* Set channels */
	sample_channels = params_channels(params);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (sample_channels == 1)
			snd_codec_update_bits(codec, SUNXI_DAC_FIFO_CTL,
					      0x1 << DAC_MONO_EN,
					      0x1 << DAC_MONO_EN);
		else
			snd_codec_update_bits(codec, SUNXI_DAC_FIFO_CTL,
					      0x1 << DAC_MONO_EN,
					      0x0 << DAC_MONO_EN);
	} else {
		if (sunxi_get_adc_ch(codec) < 0) {
			snd_err("capture only support 1~3 channels\n");
			return -EINVAL;
		}

		if (codec_param->adc1_en) {
			snd_codec_update_bits(codec, SUNXI_ADC1_AN_CTL,
					      0x1 << MIC1_PGA_EN, 0x1 << MIC1_PGA_EN);
			snd_codec_update_bits(codec, SUNXI_ADC1_AN_CTL,
					      0x1 << ADC1_EN, 0x1 << ADC1_EN);
			snd_codec_update_bits(codec, SUNXI_ADC_DIG_CTL,
					      0x1 << ADC1_CHANNEL_EN,
					      0x1 << ADC1_CHANNEL_EN);
		} else {
			snd_codec_update_bits(codec, SUNXI_ADC1_AN_CTL,
					      0x1 << ADC1_EN, 0x0 << ADC1_EN);
			snd_codec_update_bits(codec, SUNXI_ADC1_AN_CTL,
					      0x1 << MIC1_PGA_EN, 0x0 << MIC1_PGA_EN);
			snd_codec_update_bits(codec, SUNXI_ADC_DIG_CTL,
					      0x1 << ADC1_CHANNEL_EN,
					      0x0 << ADC1_CHANNEL_EN);
		}

		if (codec_param->adc2_en) {
			snd_codec_update_bits(codec, SUNXI_ADC2_AN_CTL,
					      0x1 << MIC2_PGA_EN, 0x1 << MIC2_PGA_EN);
			snd_codec_update_bits(codec, SUNXI_ADC2_AN_CTL,
					      0x1 << ADC2_EN, 0x1 << ADC2_EN);
			snd_codec_update_bits(codec, SUNXI_ADC_DIG_CTL,
					      0x1 << ADC2_CHANNEL_EN,
					      0x1 << ADC2_CHANNEL_EN);
		} else {
			snd_codec_update_bits(codec, SUNXI_ADC2_AN_CTL,
					      0x1 << ADC2_EN, 0x0 << ADC2_EN);
			snd_codec_update_bits(codec, SUNXI_ADC2_AN_CTL,
					      0x1 << MIC2_PGA_EN, 0x0 << MIC2_PGA_EN);
			snd_codec_update_bits(codec, SUNXI_ADC_DIG_CTL,
					      0x1 << ADC2_CHANNEL_EN,
					      0x0 << ADC2_CHANNEL_EN);
		}

		if (codec_param->adc3_en) {
			snd_codec_update_bits(codec, SUNXI_ADC3_AN_CTL,
					      0x1 << MIC3_PGA_EN, 0x1 << MIC3_PGA_EN);
			snd_codec_update_bits(codec, SUNXI_ADC3_AN_CTL,
					      0x1 << ADC3_EN, 0x1 << ADC3_EN);
			snd_codec_update_bits(codec, SUNXI_ADC_DIG_CTL,
					      0x1 << ADC3_CHANNEL_EN,
					      0x1 << ADC3_CHANNEL_EN);
		} else {
			snd_codec_update_bits(codec, SUNXI_ADC3_AN_CTL,
					      0x1 << ADC3_EN, 0x0 << ADC3_EN);
			snd_codec_update_bits(codec, SUNXI_ADC3_AN_CTL,
					      0x1 << MIC3_PGA_EN, 0x0 << MIC3_PGA_EN);
			snd_codec_update_bits(codec, SUNXI_ADC_DIG_CTL,
					      0x1 << ADC3_CHANNEL_EN,
					      0x0 << ADC3_CHANNEL_EN);
		}

		if (codec_param->adc1_en || codec_param->adc2_en || codec_param->adc3_en) {
			/* mic bias, only once*/
			snd_codec_update_bits(codec, SUNXI_MICBIAS_AN_CTL,
					0x1 << MMIC_BIAS_EN, 0x1 << MMIC_BIAS_EN);
			/* delay 80ms to avoid pop recording in begining,
				* and adc fifo delay time must not be less than 20ms.
				*/
			hal_msleep(80);
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					0x1 << ADC_DIG_EN, 0x1 << ADC_DIG_EN);
		} else {
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					      0x1 << ADC_DIG_EN, 0x0 << ADC_DIG_EN);
			snd_codec_update_bits(codec, SUNXI_MICBIAS_AN_CTL,
					      0x1 << MMIC_BIAS_EN, 0x0 << MMIC_BIAS_EN);
		}
	}

	return 0;
}

static void sunxi_codec_shutdown(struct snd_pcm_substream *substream,
				 struct snd_dai *dai)
{

	return;
}

static int sunxi_codec_set_sysclk(struct snd_dai *dai,
				  int clk_id, unsigned int freq, int dir)
{
	int ret;
	struct snd_codec *codec = dai->component;
	struct sunxi_codec_info *sunxi_codec = codec->private_data;

	snd_print("\n");

	ret = snd_sunxi_clk_set_rate(&sunxi_codec->clk, dir, freq, freq);
	if (ret < 0) {
		snd_err("snd_sunxi_clk_set_rate failed\n");
		return -1;
	}



	return 0;
}

static int sunxi_codec_prepare(struct snd_pcm_substream *substream,
			       struct snd_dai *dai)
{
	struct snd_codec *codec = dai->component;

	snd_print("\n");

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		snd_codec_update_bits(codec, SUNXI_DAC_FIFO_CTL,
				      0x1 << DAC_FIFO_FLUSH,
				      0x1 << DAC_FIFO_FLUSH);
		snd_codec_write(codec, SUNXI_DAC_FIFO_STA,
				(0x1 << DAC_TXE_INT | 0x1 << DAC_TXU_INT | 0x1 << DAC_TXO_INT));
		snd_codec_write(codec, SUNXI_DAC_CNT, 0x0);
	} else {
		snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
				      0x1 << ADC_FIFO_FLUSH,
				      0x1 << ADC_FIFO_FLUSH);
		snd_codec_write(codec, SUNXI_ADC_FIFO_STA,
				0x1 << ADC_RXA_INT | 0x1 << ADC_RXO_INT);
		snd_codec_write(codec, SUNXI_ADC_CNT, 0x0);
	}

	return 0;
}

static int sunxi_codec_trigger(struct snd_pcm_substream *substream,
			       int cmd, struct snd_dai *dai)
{
	struct snd_codec *codec = dai->component;

	snd_print("cmd=%d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			snd_codec_update_bits(codec, SUNXI_DAC_FIFO_CTL,
					      0x1 << DAC_DRQ_EN,
					      0x1 << DAC_DRQ_EN);
		} else {
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					      0x1 << ADC_DRQ_EN,
					      0x1 << ADC_DRQ_EN);
		}
	break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			snd_codec_update_bits(codec, SUNXI_DAC_FIFO_CTL,
					      0x1 << DAC_DRQ_EN,
					      0x0 << DAC_DRQ_EN);
		} else {
			snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
					      0x1 << ADC_DRQ_EN,
					      0x0 << ADC_DRQ_EN);
		}
	break;
	default:
		snd_err("unsupport cmd\n");
		return -EINVAL;
	}

	return 0;
}

static struct snd_dai_ops sun55iw3_codec_dai_ops = {
	.startup	= sunxi_codec_startup,
	.hw_params	= sunxi_codec_hw_params,
	.shutdown	= sunxi_codec_shutdown,
	.set_sysclk	= sunxi_codec_set_sysclk,
	.trigger	= sunxi_codec_trigger,
	.prepare	= sunxi_codec_prepare,
	.dapm_control   = sunxi_codec_dapm_control,
};

static struct snd_dai sun55iw3_codec_dai[] = {
	{
		.name	= "sun55iw3codec",
		.playback = {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_8000_192000,
			.formats	= SNDRV_PCM_FMTBIT_S16_LE
					// | SNDRV_PCM_FMTBIT_S20_LE
					| SNDRV_PCM_FMTBIT_S24_LE
					| SNDRV_PCM_FMTBIT_S32_LE,
			.rate_min       = 8000,
			.rate_max       = 192000,
		},
		.capture = {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 3,
			.rates		= SNDRV_PCM_RATE_8000_48000,
			.formats	= SNDRV_PCM_FMTBIT_S16_LE
					// | SNDRV_PCM_FMTBIT_S20_LE
					| SNDRV_PCM_FMTBIT_S24_LE
					| SNDRV_PCM_FMTBIT_S32_LE,
			.rate_min       = 8000,
			.rate_max       = 48000,
		},
		.ops = &sun55iw3_codec_dai_ops,
	},
};

static void sunxi_codec_init(struct snd_codec *codec)
{
	struct sunxi_codec_info *sunxi_codec = codec->private_data;
	struct sunxi_codec_param *codec_param = &sunxi_codec->param;

	snd_print("\n");

	snd_codec_update_bits(codec, SUNXI_RAMP, 0x1 << 1, 0x1 << 1);

	/* Enable DAC/ADC DAP */
	snd_codec_update_bits(codec, SUNXI_DAC_DAP_CTL,
			      0x1 << DDAP_EN, 0x1 << DDAP_EN);
	snd_codec_update_bits(codec, SUNXI_ADC_DAP_CTL,
			      0x1 << ADAP0_EN, 0x1 << ADAP0_EN);
	snd_codec_update_bits(codec, SUNXI_ADC_DAP_CTL,
			      0x1 << ADAP1_EN, 0x1 << ADAP1_EN);
	/* cpvcc set 1.2V, analog power for headphone charge pump */
	snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
			      0x1 << CPLDO_EN, 0x1 << CPLDO_EN);
	snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
			      0x3 << CPLDO_VOLTAGE, 0x3 << CPLDO_VOLTAGE);
	/* Open VRP to remove noise */
	snd_codec_update_bits(codec, SUNXI_POWER_AN_CTL,
			      0x1 << VRP_LDO_EN, 0x1 << VRP_LDO_EN);
	/* Enable ADCFDT to overcome niose at the beginning */
	snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
			      0x1 << ADCDFEN, 0x1 << ADCDFEN);
	snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL,
			      0x3 << ADCFDT, 0x2 << ADCFDT);

	/* Set default func switch vaule */
	snd_codec_update_bits(codec, SUNXI_DAC_DPC,
			      0x1 << HUB_EN, codec_param->tx_hub_en << HUB_EN);
	// if (codec_param->rx_sync_en) {
	// 	sunxi_rx_sync_startup(codec_param->rx_sync_domain, codec_param->rx_sync_id,
	// 			      (void *)codec, sunxi_rx_sync_enable);
	// 	snd_codec_update_bits(codec, SUNXI_ADC_FIFO_CTL, 0x1 << RX_SYNC_EN,
	// 			      0x1 << RX_SYNC_EN);
	// 	codec_param->rx_sync_status = 1;
	// }
	/* Open ADC1\2\3 and DACL\R Volume Setting */
	snd_codec_update_bits(codec, SUNXI_DAC_VOL_CTL,
			      0x1 << DAC_VOL_SEL,
			      0x1 << DAC_VOL_SEL);
	snd_codec_update_bits(codec, SUNXI_ADC_DIG_CTL,
			      0x1 << ADC1_2_VOL_EN, 0x1 << ADC1_2_VOL_EN);
	snd_codec_update_bits(codec, SUNXI_ADC_DIG_CTL,
			      0x1 << ADC3_VOL_EN,
			      0x1 << ADC3_VOL_EN);
	/* To solve play quietly */
	snd_codec_update_bits(codec, SUNXI_POWER_AN_CTL,
			      0x1 << BG_BUFFER_DISABLE,
			      0x1 << BG_BUFFER_DISABLE);
	hal_msleep(50);
	snd_codec_update_bits(codec, SUNXI_POWER_AN_CTL,
			      0x1 << BG_BUFFER_DISABLE,
			      0x0 << BG_BUFFER_DISABLE);

	/* Set default volume/gain control value */
	snd_codec_update_bits(codec, SUNXI_DAC_DPC,
			      0x3F << DVOL, codec_param->dac_vol << DVOL);
	snd_codec_update_bits(codec, SUNXI_DAC_VOL_CTL,
			      0xFF << DAC_VOL_L,
			      codec_param->dacl_vol << DAC_VOL_L);
	snd_codec_update_bits(codec, SUNXI_DAC_VOL_CTL,
			      0xFF << DAC_VOL_R,
			      codec_param->dacr_vol << DAC_VOL_R);
	snd_codec_update_bits(codec, SUNXI_ADC_VOL_CTL1,
			      0xFF << ADC1_VOL, codec_param->adc1_vol << ADC1_VOL);
	snd_codec_update_bits(codec, SUNXI_ADC_VOL_CTL1,
			      0xFF << ADC2_VOL, codec_param->adc2_vol << ADC2_VOL);
	snd_codec_update_bits(codec, SUNXI_ADC_VOL_CTL1,
			      0xFF << ADC3_VOL, codec_param->adc3_vol << ADC3_VOL);
	snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
			      0x1F << LINEOUT_GAIN,
			      codec_param->lineout_gain << LINEOUT_GAIN);
	snd_codec_update_bits(codec, SUNXI_DAC_AN_REG,
			      0x7 << HEADPHONE_GAIN,
			      codec_param->hpout_gain << HEADPHONE_GAIN);
	snd_codec_update_bits(codec, SUNXI_ADC1_AN_CTL,
			      0x1F << ADC1_PGA_GAIN_CTL,
			      codec_param->adc1_gain << ADC1_PGA_GAIN_CTL);
	snd_codec_update_bits(codec, SUNXI_ADC2_AN_CTL,
			      0x1F << ADC2_PGA_GAIN_CTL,
			      codec_param->adc2_gain << ADC2_PGA_GAIN_CTL);
	snd_codec_update_bits(codec, SUNXI_ADC3_AN_CTL,
			      0x1F << ADC3_PGA_GAIN_CTL,
			      codec_param->adc3_gain << ADC3_PGA_GAIN_CTL);

	return;
}


static void snd_sunxi_params_init(struct sunxi_codec_param *params)
{
#ifdef CONFIG_DRIVER_SYSCONFIG
	int ret;
	int32_t val;

	ret = hal_cfg_get_keyvalue(CODEC, "tx_hub_en", &val, 1);
	if (ret) {
		snd_err("%s: tx_hub_en miss. ret:%d\n", CODEC);
		params->tx_hub_en = default_param.tx_hub_en;
	} else
		params->tx_hub_en = val;

	ret = hal_cfg_get_keyvalue(CODEC, "rx_sync_en", &val, 1);
	if (ret) {
		snd_err("%s: rx_sync_en miss. ret:%d\n", CODEC);
		params->rx_sync_en = default_param.rx_sync_en;
	} else
		params->rx_sync_en = val;

	ret = hal_cfg_get_keyvalue(CODEC, "dac_vol", &val, 1);
	if (ret) {
		snd_err("%s: dac_vol miss. ret:%d\n", CODEC);
		params->dac_vol = default_param.dac_vol;
	} else
		params->dac_vol = 63 - val;

	ret = hal_cfg_get_keyvalue(CODEC, "dacl_vol", &val, 1);
	if (ret) {
		snd_err("%s: dacl_vol miss. ret:%d\n", CODEC);
		params->dacl_vol = default_param.dacl_vol;
	} else
		params->dacl_vol = val;

	ret = hal_cfg_get_keyvalue(CODEC, "dacr_vol", &val, 1);
	if (ret) {
		snd_err("%s: dacr_vol miss. ret:%d\n", CODEC);
		params->dacr_vol = default_param.dacr_vol;
	} else
		params->dacr_vol = val;

	ret = hal_cfg_get_keyvalue(CODEC, "adc1_vol", &val, 1);
	if (ret) {
		snd_err("%s: adc1_vol miss. ret:%d\n", CODEC);
		params->adc1_vol = default_param.adc1_vol;
	} else
		params->adc1_vol = val;

	ret = hal_cfg_get_keyvalue(CODEC, "adc2_vol", &val, 1);
	if (ret) {
		snd_err("%s: adc2_vol miss. ret:%d\n", CODEC);
		params->adc2_vol = default_param.adc2_vol;
	} else
		params->adc2_vol = val;

	ret = hal_cfg_get_keyvalue(CODEC, "adc3_vol", &val, 1);
	if (ret) {
		snd_err("%s: adc3_vol miss. ret:%d\n", CODEC);
		params->adc3_vol = default_param.adc3_vol;
	} else
		params->adc3_vol = val;

	ret = hal_cfg_get_keyvalue(CODEC, "lineout_gain", &val, 1);
	if (ret) {
		snd_err("%s: lineout_gain miss. ret:%d\n", CODEC);
		params->lineout_gain = default_param.lineout_gain;
	} else
		params->lineout_gain = val;

	ret = hal_cfg_get_keyvalue(CODEC, "hpout_gain", &val, 1);
	if (ret) {
		snd_err("%s: hpout_gain miss. ret:%d\n", CODEC);
		params->hpout_gain = default_param.hpout_gain;
	} else
		params->hpout_gain = 7 - val;

	ret = hal_cfg_get_keyvalue(CODEC, "adc1_gain", &val, 1);
	if (ret) {
		snd_err("%s: adc1_gain miss. ret:%d\n", CODEC);
		params->adc1_gain = default_param.adc1_gain;
	} else
		params->adc1_gain = val;

	ret = hal_cfg_get_keyvalue(CODEC, "adc2_gain", &val, 1);
	if (ret) {
		snd_err("%s: adc2_gain miss. ret:%d\n", CODEC);
		params->adc2_gain = default_param.adc2_gain;
	} else
		params->adc2_gain = val;

	ret = hal_cfg_get_keyvalue(CODEC, "adc3_gain", &val, 1);
	if (ret) {
		snd_err("%s: adc3_gain miss. ret:%d\n", CODEC);
		params->adc3_gain = default_param.adc3_gain;
	} else
		params->adc3_gain = val;

	ret = hal_cfg_get_keyvalue(CODEC, "mic1_en", &val, 1);
	if (ret) {
		snd_print("%s: mic1_en miss.\n", CODEC);
		params->mic1_en = default_param.mic1_en;
	} else
		params->mic1_en = val;

	ret = hal_cfg_get_keyvalue(CODEC, "mic2_en", &val, 1);
	if (ret) {
		snd_print("%s: mic2_en miss.\n", CODEC);
		params->mic2_en = default_param.mic2_en;
	} else
		params->mic2_en = val;

	ret = hal_cfg_get_keyvalue(CODEC, "mic3_en", &val, 1);
	if (ret) {
		snd_print("%s: mic3_en miss.\n", CODEC);
		params->mic3_en = default_param.mic3_en;
	} else
		params->mic3_en = val;

	ret = hal_cfg_get_keyvalue(CODEC, "lineoutl_en", &val, 1);
	if (ret) {
		snd_print("%s: lineoutl_en miss.\n", CODEC);
		params->lineoutl_en = default_param.lineoutl_en;
	} else
		params->lineoutl_en = val;

	ret = hal_cfg_get_keyvalue(CODEC, "lineoutr_en", &val, 1);
	if (ret) {
		snd_print("%s: lineoutr_en miss.\n", CODEC);
		params->lineoutr_en = default_param.lineoutr_en;
	} else
		params->lineoutr_en = val;

	ret = hal_cfg_get_keyvalue(CODEC, "spk_en", &val, 1);
	if (ret) {
		snd_print("%s: spk_en miss.\n", CODEC);
		params->spk_en = default_param.spk_en;
	} else
		params->spk_en = val;

	ret = hal_cfg_get_keyvalue(CODEC, "hpout_en", &val, 1);
	if (ret) {
		snd_print("%s: hpout_en miss.\n", CODEC);
		params->hpout_en = default_param.hpout_en;
	} else
		params->hpout_en = val;
#else
	*params = default_param;
	params->dac_vol = 63 - default_param.dac_vol;
	params->hpout_gain =7 - default_param.hpout_gain;
#endif
}

static int snd_sunxi_clk_init(struct sunxi_codec_clk *clk)
{
	int ret;

	snd_print("\n");

	clk->clk_rst = hal_reset_control_get(HAL_SUNXI_MCU_RESET, RST_BUS_MCU_AUDIO_CODEC);
	if (!clk->clk_rst) {
		snd_err("codec clk_rst hal_reset_control_get failed\n");
		goto err_get_clk_rst;
	}

	clk->clk_peri0_2x = hal_clock_get(HAL_SUNXI_CCU, CLK_PLL_PERI0_2X);
	if (!clk->clk_peri0_2x) {
		snd_err("clk peri0 2x get failed\n");
		goto err_get_clk_peri0_2x;
	}

	clk->clk_dsp_src = hal_clock_get(HAL_SUNXI_CCU, CLK_DSP);
	if (!clk->clk_dsp_src) {
		snd_err("clk dsp src get failed\n");
		goto err_get_clk_dsp_src;
	}

	ret = hal_clk_set_parent(clk->clk_dsp_src, clk->clk_peri0_2x);
	if (ret != HAL_CLK_STATUS_OK) {
		snd_err("set clk_dsp_src parent clk failed\n");
		return -EINVAL;
	}

	ret = hal_clk_set_rate(clk->clk_dsp_src, 600000000);
	if (ret != HAL_CLK_STATUS_OK) {
		snd_err("set clk_dsp_src rate failed\n");
		return -EINVAL;
	}

	/* get bus clk */
	clk->clk_bus = hal_clock_get(HAL_SUNXI_MCU, CLK_BUS_MCU_AUDIO_CODEC);
	if (!clk->clk_bus) {
		snd_err("clk_bus get failed\n");
		goto err_get_clk_bus;
	}

	/* get parent clk */
	clk->clk_pll_audio0_4x = hal_clock_get(HAL_SUNXI_CCU, CLK_PLL_AUDIO0_4X);
	if (!clk->clk_pll_audio0_4x) {
		snd_err("clk_pll_audio0_4x get failed\n");
		goto err_get_clk_pll_audio0_4x;
	}

	clk->clk_pll_audio1_div2 = hal_clock_get(HAL_SUNXI_MCU, CLK_PLL_MCU_AUDIO1_DIV2);
	if (!clk->clk_pll_audio1_div2) {
		snd_err("clk_pll_audio1_div2 get failed\n");
		goto err_get_clk_pll_audio1_div2;
	}

	clk->clk_pll_audio1_div5 = hal_clock_get(HAL_SUNXI_MCU, CLK_PLL_MCU_AUDIO1_DIV5);
	if (!clk->clk_pll_audio1_div5) {
		snd_err("clk_pll_audio1_div5 get failed\n");
		goto err_get_clk_pll_audio1_div5;
	}

	/* get module clk */
	clk->clk_audio_dac = hal_clock_get(HAL_SUNXI_MCU, CLK_MCU_AUDIO_CODEC_DAC);
	if (!clk->clk_audio_dac) {
		snd_err("clk_audio_dac get failed\n");
		goto err_get_clk_audio_dac;
	}

	clk->clk_audio_adc = hal_clock_get(HAL_SUNXI_MCU, CLK_MCU_AUDIO_CODEC_ADC);
	if (!clk->clk_audio_adc) {
		snd_err("clk_audio_adc get failed\n");
		goto err_get_clk_audio_adc;
	}

	ret = snd_sunxi_clk_enable(clk);
	if (ret) {
		snd_err("clk enable failed\n");
		goto err_clk_enable;
	}

	return HAL_CLK_STATUS_OK;

err_clk_enable:
err_get_clk_audio_adc:
	hal_clock_put(clk->clk_audio_dac);
err_get_clk_audio_dac:
	hal_clock_put(clk->clk_pll_audio1_div5);
err_get_clk_pll_audio1_div5:
	hal_clock_put(clk->clk_pll_audio1_div2);
err_get_clk_pll_audio1_div2:
	hal_clock_put(clk->clk_pll_audio0_4x);
err_get_clk_pll_audio0_4x:
	hal_clock_put(clk->clk_bus);
err_get_clk_bus:
	hal_clock_put(clk->clk_dsp_src);
err_get_clk_dsp_src:
	hal_clock_put(clk->clk_peri0_2x);
err_get_clk_peri0_2x:
err_get_clk_rst:
	return HAL_CLK_STATUS_ERROR;

}

static void snd_sunxi_clk_exit(struct sunxi_codec_clk *clk)
{
	snd_print("\n");

	snd_sunxi_clk_disable(clk);
	hal_clock_put(clk->clk_audio_adc);
	hal_clock_put(clk->clk_audio_dac);
	hal_clock_put(clk->clk_pll_audio1_div5);
	hal_clock_put(clk->clk_pll_audio1_div2);
	hal_clock_put(clk->clk_pll_audio0_4x);
	hal_clock_put(clk->clk_bus);
	hal_clock_put(clk->clk_dsp_src);
	hal_clock_put(clk->clk_peri0_2x);
	hal_reset_control_put(clk->clk_rst);

	return;
}

static int snd_sunxi_clk_enable(struct sunxi_codec_clk *clk)
{
	int ret = 0;

	snd_print("\n");

	ret = hal_reset_control_deassert(clk->clk_rst);
	if (ret != HAL_CLK_STATUS_OK) {
		snd_err("clk_rst deassert failed\n");
		goto err_deassert_rst;
	}

	ret = hal_clock_enable(clk->clk_peri0_2x);
	if (ret != HAL_CLK_STATUS_OK) {
		snd_err("clk_peri0_2x enable failed\n");
		goto err_enable_clk_peri0_2x;
	}

	ret = hal_clock_enable(clk->clk_dsp_src);
	if (ret != HAL_CLK_STATUS_OK) {
		snd_err("clk_dsp_src enable failed\n");
		goto err_enable_clk_dsp_src;
	}

	ret = hal_clock_enable(clk->clk_bus);
	if (ret != HAL_CLK_STATUS_OK) {
		snd_err("clk_bus enable failed\n");
		goto err_enable_clk_bus;
	}

	ret = hal_clock_enable(clk->clk_pll_audio0_4x);
	if (ret != HAL_CLK_STATUS_OK) {
		snd_err("clk_pll_audio0_4x enable failed\n");
		goto err_enable_clk_pll_audio0_4x;
	}
	ret = hal_clock_enable(clk->clk_pll_audio1_div2);
	if (ret != HAL_CLK_STATUS_OK) {
		snd_err("clk_pll_audio1_div2 enable failed\n");
		goto err_enable_clk_pll_audio1_div2;
	}

	ret = hal_clock_enable(clk->clk_pll_audio1_div5);
	if (ret != HAL_CLK_STATUS_OK) {
		snd_err("clk_pll_audio1_div5 enable failed\n");
		goto err_enable_clk_pll_audio1_div5;
	}

	ret = hal_clock_enable(clk->clk_audio_dac);
	if (ret != HAL_CLK_STATUS_OK) {
		snd_err("clk_audio_dac enable failed\n");
		goto err_enable_clk_audio_dac;
	}

	ret = hal_clock_enable(clk->clk_audio_adc);
	if (ret != HAL_CLK_STATUS_OK) {
		snd_err("clk_audio_adc enable failed\n");
		goto err_enable_clk_audio_adc;
	}

	return HAL_CLK_STATUS_OK;

err_enable_clk_audio_adc:
	hal_clock_disable(clk->clk_audio_dac);
err_enable_clk_audio_dac:
	hal_clock_disable(clk->clk_pll_audio1_div5);
err_enable_clk_pll_audio1_div5:
	hal_clock_disable(clk->clk_pll_audio1_div2);
err_enable_clk_pll_audio1_div2:
	hal_clock_disable(clk->clk_pll_audio0_4x);
err_enable_clk_pll_audio0_4x:
	hal_clock_disable(clk->clk_bus);
err_enable_clk_bus:
	hal_clock_disable(clk->clk_dsp_src);
err_enable_clk_dsp_src:
	hal_clock_disable(clk->clk_peri0_2x);
err_enable_clk_peri0_2x:
	hal_reset_control_assert(clk->clk_rst);
err_deassert_rst:
	return HAL_CLK_STATUS_ERROR;
}

static void snd_sunxi_clk_disable(struct sunxi_codec_clk *clk)
{
	snd_print("\n");

	hal_clock_disable(clk->clk_audio_adc);
	hal_clock_disable(clk->clk_audio_dac);
	hal_clock_disable(clk->clk_pll_audio1_div5);
	hal_clock_disable(clk->clk_pll_audio1_div2);
	hal_clock_disable(clk->clk_pll_audio0_4x);
	hal_clock_disable(clk->clk_bus);
	hal_clock_disable(clk->clk_dsp_src);
	hal_clock_disable(clk->clk_peri0_2x);
	hal_reset_control_assert(clk->clk_rst);
}

static int snd_sunxi_clk_set_rate(struct sunxi_codec_clk *clk, int stream,
				  unsigned int freq_in, unsigned int freq_out)
{
	int ret = 0;

	snd_print("\n");

	if (stream  == SNDRV_PCM_STREAM_PLAYBACK) {
		if (freq_in % 24576000 == 0) {
			ret = hal_clk_set_parent(clk->clk_audio_dac, clk->clk_pll_audio1_div2);
			if (ret != HAL_CLK_STATUS_OK) {
				snd_err("set dac parent clk failed\n");
				return HAL_CLK_STATUS_ERROR;
			}

			ret = hal_clk_set_rate(clk->clk_pll_audio0_4x, 1083801600);
			if (ret != HAL_CLK_STATUS_OK) {
				snd_err("set clk_pll_audio0_4x rate failed\n");
				return -EINVAL;
			}
		} else {
			ret = hal_clk_set_parent(clk->clk_audio_dac, clk->clk_pll_audio1_div5);
			if (ret != HAL_CLK_STATUS_OK) {
				snd_err("set dac parent clk failed\n");
				return HAL_CLK_STATUS_ERROR;
			}
		}

		ret = hal_clk_set_rate(clk->clk_audio_dac, freq_out);
		if (ret != HAL_CLK_STATUS_OK) {
			snd_err("set clk_audio_dac rate failed, rate: %u\n", freq_out);
			return HAL_CLK_STATUS_ERROR;
		}
	} else {
		if (freq_in % 24576000 == 0) {
			ret = hal_clk_set_parent(clk->clk_audio_adc, clk->clk_pll_audio0_4x);
			if (ret != HAL_CLK_STATUS_OK) {
				snd_err("set adc parent clk failed\n");
				return HAL_CLK_STATUS_ERROR;
			}
			ret = hal_clk_set_rate(clk->clk_pll_audio0_4x, 1083801600);
			if (ret != HAL_CLK_STATUS_OK) {
				snd_err("set clk_pll_audio0_4x rate failed\n");
				return -EINVAL;
			}
		} else {
			ret = hal_clk_set_parent(clk->clk_audio_adc, clk->clk_pll_audio1_div5);
			if (ret != HAL_CLK_STATUS_OK) {
				snd_err("set adc parent clk failed\n");
				return HAL_CLK_STATUS_ERROR;
			}
		}

		ret = hal_clk_set_rate(clk->clk_audio_adc, freq_out);
		if (ret != HAL_CLK_STATUS_OK) {
			snd_err("set clk_audio_adc rate failed, rate: %u\n", freq_out);
			return HAL_CLK_STATUS_ERROR;
		}
	}

	return HAL_CLK_STATUS_OK;
}


static int sun55iw3_codec_probe(struct snd_codec *codec)
{
	struct sunxi_codec_info *sunxi_codec = NULL;
	int ret;

	snd_print("\n");

	if (!codec->codec_dai)
		return -1;

	sunxi_codec = snd_malloc(sizeof(struct sunxi_codec_info));
	if (!sunxi_codec) {
		snd_err("no memory\n");
		return -ENOMEM;
	}

	codec->private_data = (void *)sunxi_codec;

	snd_sunxi_params_init(&sunxi_codec->param);
	snd_sunxi_pa_init(&sunxi_codec->pa_cfg, &default_pa_cfg, CODEC);
	snd_sunxi_pa_disable(&sunxi_codec->pa_cfg);

	codec->codec_base_addr = (void *)SUNXI_CODEC_BASE_ADDR;
	codec->codec_dai->component = codec;

	ret = snd_sunxi_clk_init(&sunxi_codec->clk);
	if (ret != 0) {
		snd_err("snd_sunxi_clk_init failed\n");
		goto err_codec_set_clock;
	}

	sunxi_codec_init(codec);

	return 0;

err_codec_set_clock:
	snd_sunxi_clk_exit(&sunxi_codec->clk);

	return -1;
}

static int sun55iw3_codec_remove(struct snd_codec *codec)
{
	struct sunxi_codec_info *sunxi_codec = codec->private_data;

	snd_print("\n");

	snd_sunxi_clk_exit(&sunxi_codec->clk);

	snd_free(sunxi_codec);
	codec->private_data = NULL;

	return 0;
}

#ifndef CONFIG_DRIVER_SYSCONFIG
static struct snd_pcm_hardware sun55iw3_hardware[2] = {
	{	/* SNDRV_PCM_STREAM_PLAYBACK */
		.info			= SNDRV_PCM_INFO_INTERLEAVED
					| SNDRV_PCM_INFO_BLOCK_TRANSFER
					| SNDRV_PCM_INFO_MMAP
					| SNDRV_PCM_INFO_MMAP_VALID
					| SNDRV_PCM_INFO_PAUSE
					| SNDRV_PCM_INFO_RESUME,
		.buffer_bytes_max	= 1024 * 16,
		.period_bytes_min	= 256,
		.period_bytes_max	= 1024 * 8,
		.periods_min		= 2,
		.periods_max		= 16,
	},
	{	/* SNDRV_PCM_STREAM_CAPTURE */
		.info			= SNDRV_PCM_INFO_INTERLEAVED
					| SNDRV_PCM_INFO_BLOCK_TRANSFER
					| SNDRV_PCM_INFO_MMAP
					| SNDRV_PCM_INFO_MMAP_VALID
					| SNDRV_PCM_INFO_PAUSE
					| SNDRV_PCM_INFO_RESUME,
		.buffer_bytes_max	= 1024 * 16,
		.period_bytes_min	= 256,
		.period_bytes_max	= 1024 * 8,
		.periods_min		= 2,
		.periods_max		= 16,
	},
};
#endif

struct snd_codec sunxi_audiocodec = {
	.name		= "audiocodec",
	.codec_dai	= sun55iw3_codec_dai,
	.codec_dai_num  = ARRAY_SIZE(sun55iw3_codec_dai),
	.private_data	= NULL,
	.probe		= sun55iw3_codec_probe,
	.remove		= sun55iw3_codec_remove,
	.controls       = sunxi_codec_controls,
	.num_controls   = ARRAY_SIZE(sunxi_codec_controls),
#ifndef CONFIG_DRIVER_SYSCONFIG
	.hw 		= sun55iw3_hardware,
#endif
};
