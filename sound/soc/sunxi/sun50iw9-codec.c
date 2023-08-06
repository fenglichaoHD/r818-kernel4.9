/*
 * sound\soc\sunxi\sun50iw9_codec.c
 * (C) Copyright 2014-2018
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * yumingfeng <yumingfeng@allwinnertech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/sunxi-gpio.h>
//#include <linux/sys_config.h>

#include "sunxi_rw_func.h"
#include "sun50iw9-codec.h"

#define	DRV_NAME	"sunxi-internal-codec"

/* dacdrc_function */
enum sunxi_hw_drc {
	DRC_HP_EN = 0x1,
	DRC_SPK_EN = 0x2,
	/* DRC_HP_EN | DRC_SPK_EN */
	DRC_HPSPK_EN = 0x3,
};

struct sample_rate {
	unsigned int samplerate;
	unsigned int rate_bit;
};

static const struct sample_rate sample_rate_conv[] = {
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

static struct audiocodec_reg_label reg_labels[] = {
	LABEL(SUNXI_DAC_DPC),
	LABEL(SUNXI_DAC_FIFO_CTL),
	LABEL(SUNXI_DAC_FIFO_STA),
	LABEL(SUNXI_DAC_CNT),
	LABEL(SUNXI_DAC_DG_REG),
	LABEL(SUNXI_ADC_FIFO_CTL),
	LABEL(SUNXI_ADC_FIFO_STA),
	LABEL(SUNXI_ADC_CNT),
	LABEL(SUNXI_ADC_DG_REG),
	LABEL(SUNXI_DAC_DAP_CTL),
	LABEL(SUNXI_ADC_DAP_CTL),

	LABEL(AC_DAC_REG),
	LABEL(AC_MIXER_REG),
	LABEL(AC_RAMP_REG),

	LABEL_END,
};

//static const DECLARE_TLV_DB_SCALE(digital_tlv, -7424, 116, 0);
static const DECLARE_TLV_DB_SCALE(digital_tlv, 0, -116, -7424);
static const DECLARE_TLV_DB_SCALE(linein_to_l_r_mix_vol_tlv, -450, 150, 0);
static const DECLARE_TLV_DB_SCALE(fmin_to_l_r_mix_vol_tlv, -450, 150, 0);

static const unsigned int lineout_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 0, TLV_DB_SCALE_ITEM(0, 0, 1),
	1, 31, TLV_DB_SCALE_ITEM(-4350, 150, 1),
};

/*lineoutL mux select */
const char * const left_lineout_text[] = {
	"LOMixer", "LROMixer",
};

static const struct soc_enum left_lineout_enum =
SOC_ENUM_SINGLE(AC_DAC_REG, LINEOUTL_SEL,
ARRAY_SIZE(left_lineout_text), left_lineout_text);

static const struct snd_kcontrol_new left_lineout_mux =
SOC_DAPM_ENUM("Left LINEOUT Mux", left_lineout_enum);

/*lineoutR mux select */
const char * const right_lineout_text[] = {
	"ROMixer", "LROMixer",
};

static const struct soc_enum right_lineout_enum =
SOC_ENUM_SINGLE(AC_DAC_REG, LINEOUTR_SEL,
ARRAY_SIZE(right_lineout_text), right_lineout_text);

static const struct snd_kcontrol_new right_lineout_mux =
SOC_DAPM_ENUM("Right LINEOUT Mux", right_lineout_enum);

static void adcdrc_config(struct snd_soc_codec *codec)
{
	/* Left peak filter attack time */
	snd_soc_write(codec, AC_ADC_DRC_LPFHAT, (0x000B77BF >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LPFLAT, 0x000B77BF & 0xFFFF);
	/* Right peak filter attack time */
	snd_soc_write(codec, AC_ADC_DRC_RPFHAT, (0x000B77BF >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_RPFLAT, 0x000B77BF & 0xFFFF);
	/* Left peak filter release time */
	snd_soc_write(codec, AC_ADC_DRC_LPFHRT, (0x00FFE1F8 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LPFLRT, 0x00FFE1F8 & 0xFFFF);
	/* Right peak filter release time */
	snd_soc_write(codec, AC_ADC_DRC_RPFHRT, (0x00FFE1F8 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_RPFLRT, 0x00FFE1F8 & 0xFFFF);

	/* Left RMS filter attack time */
	snd_soc_write(codec, AC_ADC_DRC_LPFHAT, (0x00012BAF >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LPFLAT, 0x00012BAF & 0xFFFF);
	/* Right RMS filter attack time */
	snd_soc_write(codec, AC_ADC_DRC_RPFHAT, (0x00012BAF >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_RPFLAT, 0x00012BAF & 0xFFFF);

	/* smooth filter attack time */
	snd_soc_write(codec, AC_ADC_DRC_SFHAT, (0x00025600 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_SFLAT, 0x00025600 & 0xFFFF);
	/* gain smooth filter release time */
	snd_soc_write(codec, AC_ADC_DRC_SFHRT, (0x00000F04 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_SFLRT, 0x00000F04 & 0xFFFF);

	/* OPL */
	snd_soc_write(codec, AC_ADC_DRC_HOPL, (0xFBD8FBA7 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LOPL, 0xFBD8FBA7 & 0xFFFF);
	/* OPC */
	snd_soc_write(codec, AC_ADC_DRC_HOPC, (0xF95B2C3F >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LOPC, 0xF95B2C3F & 0xFFFF);
	/* OPE */
	snd_soc_write(codec, AC_ADC_DRC_HOPE, (0xF45F8D6E >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LOPE, 0xF45F8D6E & 0xFFFF);
	/* LT */
	snd_soc_write(codec, AC_ADC_DRC_HLT, (0x01A934F0 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LLT, 0x01A934F0 & 0xFFFF);
	/* CT */
	snd_soc_write(codec, AC_ADC_DRC_HCT, (0x06A4D3C0 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LCT, 0x06A4D3C0 & 0xFFFF);
	/* ET */
	snd_soc_write(codec, AC_ADC_DRC_HET, (0x0BA07291 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LET, 0x0BA07291 & 0xFFFF);
	/* Ki */
	snd_soc_write(codec, AC_ADC_DRC_HKI, (0x00051EB8 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LKI, 0x00051EB8 & 0xFFFF);
	/* Kc */
	snd_soc_write(codec, AC_ADC_DRC_HKC, (0x00800000 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LKC, 0x00800000 & 0xFFFF);
	/* Kn */
	snd_soc_write(codec, AC_ADC_DRC_HKN, (0x01000000 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LKN, 0x01000000 & 0xFFFF);
	/* Ke */
	snd_soc_write(codec, AC_ADC_DRC_HKE, (0x0000F45F >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LKE, 0x0000F45F & 0xFFFF);
}

static void adcdrc_enable(struct snd_soc_codec *codec, bool on)
{
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);

	if (on) {
		snd_soc_update_bits(codec, SUNXI_ADC_DAP_CTL,
			(0x1 << ADC_DRC0_EN | 0x1 << ADC_DRC1_EN),
			(0x1 << ADC_DRC0_EN | 0x1 << ADC_DRC1_EN));

		if (sunxi_codec->adcdap_en++ == 0) {
			snd_soc_update_bits(codec, SUNXI_ADC_DAP_CTL,
				(0x1 << ADC_DAP0_EN | 0x1 << ADC_DAP1_EN),
				(0x1 << ADC_DAP0_EN | 0x1 << ADC_DAP1_EN));
		}
	} else {
		if (--sunxi_codec->adcdap_en == 0) {
			snd_soc_update_bits(codec, SUNXI_ADC_DAP_CTL,
				(0x1 << ADC_DAP0_EN | 0x1 << ADC_DAP1_EN),
				(0x0 << ADC_DAP0_EN | 0x0 << ADC_DAP1_EN));
		}
		snd_soc_update_bits(codec, SUNXI_ADC_DAP_CTL,
			(0x1 << ADC_DRC0_EN | 0x1 << ADC_DRC1_EN),
			(0x0 << ADC_DRC0_EN | 0x0 << ADC_DRC1_EN));
	}
}

static void adchpf_config(struct snd_soc_codec *codec)
{
	/* HPF */
	snd_soc_write(codec, AC_ADC_DRC_HHPFC, (0xFFFAC1 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_ADC_DRC_LHPFC, 0xFFFAC1 & 0xFFFF);
}

static void adchpf_enable(struct snd_soc_codec *codec, bool on)
{
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);

	if (on) {
		snd_soc_update_bits(codec, SUNXI_ADC_DAP_CTL,
			(0x1 << ADC_HPF0_EN | 0x1 << ADC_HPF1_EN),
			(0x1 << ADC_HPF0_EN | 0x1 << ADC_HPF1_EN));

		if (sunxi_codec->adcdap_en++ == 0) {
			snd_soc_update_bits(codec, SUNXI_ADC_DAP_CTL,
				(0x1 << ADC_DAP0_EN | 0x1 << ADC_DAP1_EN),
				(0x1 << ADC_DAP0_EN | 0x1 << ADC_DAP1_EN));
		}
	} else {
		if (--sunxi_codec->adcdap_en == 0) {
			snd_soc_update_bits(codec, SUNXI_ADC_DAP_CTL,
				(0x1 << ADC_DAP0_EN | 0x1 << ADC_DAP1_EN),
				(0x0 << ADC_DAP0_EN | 0x0 << ADC_DAP1_EN));
		}
		snd_soc_update_bits(codec, SUNXI_ADC_DAP_CTL,
			(0x1 << ADC_HPF0_EN | 0x1 << ADC_HPF1_EN),
			(0x0 << ADC_HPF0_EN | 0x0 << ADC_HPF1_EN));
	}
}

static void dacdrc_config(struct snd_soc_codec *codec)
{
	/* Left peak filter attack time */
	snd_soc_write(codec, AC_DAC_DRC_LPFHAT, (0x000B77BF >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LPFLAT, 0x000B77BF & 0xFFFF);
	/* Right peak filter attack time */
	snd_soc_write(codec, AC_DAC_DRC_RPFHAT, (0x000B77BF >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_RPFLAT, 0x000B77BF & 0xFFFF);
	/* Left peak filter release time */
	snd_soc_write(codec, AC_DAC_DRC_LPFHRT, (0x00FFE1F8 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LPFLRT, 0x00FFE1F8 & 0xFFFF);
	/* Right peak filter release time */
	snd_soc_write(codec, AC_DAC_DRC_RPFHRT, (0x00FFE1F8 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_RPFLRT, 0x00FFE1F8 & 0xFFFF);

	/* Left RMS filter attack time */
	snd_soc_write(codec, AC_DAC_DRC_LPFHAT, (0x00012BAF >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LPFLAT, 0x00012BAF & 0xFFFF);
	/* Right RMS filter attack time */
	snd_soc_write(codec, AC_DAC_DRC_RPFHAT, (0x00012BAF >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_RPFLAT, 0x00012BAF & 0xFFFF);

	/* smooth filter attack time */
	snd_soc_write(codec, AC_DAC_DRC_SFHAT, (0x00025600 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_SFLAT, 0x00025600 & 0xFFFF);
	/* gain smooth filter release time */
	snd_soc_write(codec, AC_DAC_DRC_SFHRT, (0x00000F04 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_SFLRT, 0x00000F04 & 0xFFFF);

	/* OPL */
	snd_soc_write(codec, AC_DAC_DRC_HOPL, (0xFBD8FBA7 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LOPL, 0xFBD8FBA7 & 0xFFFF);
	/* OPC */
	snd_soc_write(codec, AC_DAC_DRC_HOPC, (0xF95B2C3F >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LOPC, 0xF95B2C3F & 0xFFFF);
	/* OPE */
	snd_soc_write(codec, AC_DAC_DRC_HOPE, (0xF45F8D6E >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LOPE, 0xF45F8D6E & 0xFFFF);
	/* LT */
	snd_soc_write(codec, AC_DAC_DRC_HLT, (0x01A934F0 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LLT, 0x01A934F0 & 0xFFFF);
	/* CT */
	snd_soc_write(codec, AC_DAC_DRC_HCT, (0x06A4D3C0 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LCT, 0x06A4D3C0 & 0xFFFF);
	/* ET */
	snd_soc_write(codec, AC_DAC_DRC_HET, (0x0BA07291 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LET, 0x0BA07291 & 0xFFFF);
	/* Ki */
	snd_soc_write(codec, AC_DAC_DRC_HKI, (0x00051EB8 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LKI, 0x00051EB8 & 0xFFFF);
	/* Kc */
	snd_soc_write(codec, AC_DAC_DRC_HKC, (0x00800000 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LKC, 0x00800000 & 0xFFFF);
	/* Kn */
	snd_soc_write(codec, AC_DAC_DRC_HKN, (0x01000000 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LKN, 0x01000000 & 0xFFFF);
	/* Ke */
	snd_soc_write(codec, AC_DAC_DRC_HKE, (0x0000F45F >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LKE, 0x0000F45F & 0xFFFF);
}

static void dacdrc_enable(struct snd_soc_codec *codec, bool on)
{
	struct sunxi_codec_info *sunxi_codec =
			snd_soc_codec_get_drvdata(codec);

	if (on) {
		regmap_update_bits(sunxi_codec->regmap, AC_DAC_DRC_CTL,
				(0x1 << DAC_DRC_CTL_GAIN_MAXLIM_EN),
				(0x1 << DAC_DRC_CTL_GAIN_MAXLIM_EN));
		regmap_update_bits(sunxi_codec->regmap, AC_DAC_DRC_CTL,
				(0x1 << DAC_DRC_CTL_GAIN_MINLIM_EN),
				(0x1 << DAC_DRC_CTL_GAIN_MINLIM_EN));

		regmap_update_bits(sunxi_codec->regmap, AC_DAC_DRC_CTL,
				(0x1 << DAC_DRC_CTL_DRC_LT_EN),
				(0x1 << DAC_DRC_CTL_DRC_LT_EN));

		regmap_update_bits(sunxi_codec->regmap, AC_DAC_DRC_CTL,
				(0x1 << DAC_DRC_CTL_CONTROL_DRC_EN),
				(0x1 << DAC_DRC_CTL_CONTROL_DRC_EN));
		regmap_update_bits(sunxi_codec->regmap, AC_DAC_DRC_CTL,
				(0x1 << DAC_DRC_CTL_DRC_ET_EN),
				(0x1 << DAC_DRC_CTL_DRC_ET_EN));

		regmap_update_bits(sunxi_codec->regmap, SUNXI_DAC_DAP_CTL,
				(0x1 << DDAP_DRC_EN),
				(0x1 << DDAP_DRC_EN));

		if (sunxi_codec->dacdap_en == 0) {
			regmap_update_bits(sunxi_codec->regmap,
				SUNXI_DAC_DAP_CTL,
				(0x1 << DDAP_EN),
				(0x1 << DDAP_EN));
		}

		sunxi_codec->dacdap_en++;
	} else {
		sunxi_codec->dacdap_en--;
		if (sunxi_codec->dacdap_en == 0) {
			regmap_update_bits(sunxi_codec->regmap,
				SUNXI_DAC_DAP_CTL,
				(0x1 << DDAP_EN),
				(0x0 << DDAP_EN));
		}
		regmap_update_bits(sunxi_codec->regmap, SUNXI_DAC_DAP_CTL,
				(0x1 << DDAP_DRC_EN),
				(0x0 << DDAP_DRC_EN));
		regmap_update_bits(sunxi_codec->regmap, AC_DAC_DRC_CTL,
				(0x1 << DAC_DRC_CTL_GAIN_MAXLIM_EN),
				(0x0 << DAC_DRC_CTL_GAIN_MAXLIM_EN));
		regmap_update_bits(sunxi_codec->regmap, AC_DAC_DRC_CTL,
				(0x1 << DAC_DRC_CTL_GAIN_MINLIM_EN),
				(0x0 << DAC_DRC_CTL_GAIN_MINLIM_EN));

		regmap_update_bits(sunxi_codec->regmap, AC_DAC_DRC_CTL,
				(0x1 << DAC_DRC_CTL_DRC_LT_EN),
				(0x0 << DAC_DRC_CTL_DRC_LT_EN));

		regmap_update_bits(sunxi_codec->regmap, AC_DAC_DRC_CTL,
				(0x1 << DAC_DRC_CTL_CONTROL_DRC_EN),
				(0x0 << DAC_DRC_CTL_CONTROL_DRC_EN));
		regmap_update_bits(sunxi_codec->regmap, AC_DAC_DRC_CTL,
				(0x1 << DAC_DRC_CTL_DRC_ET_EN),
				(0x0 << DAC_DRC_CTL_DRC_ET_EN));
	}
}

static void dachpf_config(struct snd_soc_codec *codec)
{
	/* HPF */
	snd_soc_write(codec, AC_DAC_DRC_HHPFC, (0xFFFAC1 >> 16) & 0xFFFF);
	snd_soc_write(codec, AC_DAC_DRC_LHPFC, 0xFFFAC1 & 0xFFFF);
}

static void dachpf_enable(struct snd_soc_codec *codec, bool on)
{
	struct sunxi_codec_info *sunxi_codec =
			snd_soc_codec_get_drvdata(codec);

	if (on) {
		regmap_update_bits(sunxi_codec->regmap, SUNXI_DAC_DAP_CTL,
				(0x1 << DDAP_HPF_EN),
				(0x1 << DDAP_HPF_EN));
		if (sunxi_codec->dacdap_en == 0) {
			regmap_update_bits(sunxi_codec->regmap,
				SUNXI_DAC_DAP_CTL,
				(0x1 << DDAP_EN),
				(0x1 << DDAP_EN));
		}
		sunxi_codec->dacdap_en++;
	} else {
		sunxi_codec->dacdap_en--;
		if (sunxi_codec->dacdap_en == 0) {
			regmap_update_bits(sunxi_codec->regmap,
				SUNXI_DAC_DAP_CTL,
				(0x1 << DDAP_EN),
				(0x0 << DDAP_EN));
		}
		regmap_update_bits(sunxi_codec->regmap, SUNXI_DAC_DAP_CTL,
				(0x1 << DDAP_HPF_EN),
				(0x0 << DDAP_HPF_EN));
	}
}

static int sunxi_codec_get_hub_mode(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val;

	regmap_read(sunxi_codec->regmap, SUNXI_DAC_DPC, &reg_val);

	ucontrol->value.integer.value[0] = ((reg_val & (1 << DAC_HUB_EN)) ? 2 : 1);

	return 0;
}

static int sunxi_codec_set_hub_mode(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);

	switch (ucontrol->value.integer.value[0]) {
	case	0:
	case	1:
		regmap_update_bits(sunxi_codec->regmap, SUNXI_DAC_DPC,
				(0x1 << DAC_HUB_EN), (0x0 << DAC_HUB_EN));
		break;
	case	2:
		regmap_update_bits(sunxi_codec->regmap, SUNXI_DAC_DPC,
				(0x1 << DAC_HUB_EN), (0x1 << DAC_HUB_EN));
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/* sunxi codec hub mdoe select */
static const char * const sunxi_codec_hub_function[] = {"null",
			"hub_disable", "hub_enable"};

static const struct soc_enum sunxi_codec_hub_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sunxi_codec_hub_function),
			sunxi_codec_hub_function),
};

static int sunxi_lineout_event(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);
	struct spk_config *spk_cfg = &sunxi_codec->spk_cfg;
	unsigned int dacdrc_cfg = sunxi_codec->hw_cfg.dacdrc_cfg;

	switch (event) {
	case	SND_SOC_DAPM_POST_PMU:
		if (spk_cfg->used) {
			if ((dacdrc_cfg & DRC_SPK_EN) > 0)
				dacdrc_enable(codec, 1);
		} else if ((dacdrc_cfg & DRC_HP_EN) > 0)
			dacdrc_enable(codec, 1);

		mdelay(30);
		regmap_update_bits(sunxi_codec->regmap, AC_DAC_REG,
				(0x1 << LINEOUTL_EN),
				(0x1 << LINEOUTL_EN));
		regmap_update_bits(sunxi_codec->regmap, AC_DAC_REG,
				(0x1 << LINEOUTR_EN),
				(0x1 << LINEOUTR_EN));

		if (spk_cfg->used) {
			gpio_set_value(spk_cfg->gpio, spk_cfg->pa_ctl_level);
			/*
			 * time delay to wait lineout work fine,
			 * general setting 160ms
			 */
			mdelay(spk_cfg->pa_msleep_time);
		}
		break;
	case	SND_SOC_DAPM_PRE_PMD:
		if (spk_cfg->used) {
			gpio_set_value(spk_cfg->gpio, !spk_cfg->pa_ctl_level);
			mdelay(spk_cfg->pa_msleep_time);
		}
		regmap_update_bits(sunxi_codec->regmap, AC_DAC_REG,
				(0x1 << LINEOUTL_EN),
				(0x0 << LINEOUTL_EN));
		regmap_update_bits(sunxi_codec->regmap, AC_DAC_REG,
				(0x1 << LINEOUTR_EN),
				(0x0 << LINEOUTR_EN));
		if (spk_cfg->used) {
			if ((dacdrc_cfg & DRC_SPK_EN) > 0)
				dacdrc_enable(codec, 0);
		} else if ((dacdrc_cfg & DRC_HP_EN) > 0)
			dacdrc_enable(codec, 0);

		break;
	default:
		break;
	}
	return 0;
}


static int sunxi_codec_playback_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case	SND_SOC_DAPM_POST_PMU:
		if (sunxi_codec->dac_en++ == 0)
			regmap_update_bits(sunxi_codec->regmap, SUNXI_DAC_DPC,
				(0x1 << EN_DA), (0x1 << EN_DA));
		break;
	case	SND_SOC_DAPM_POST_PMD:
		sunxi_codec->dac_en--;
		if (sunxi_codec->dac_en == 0)
			regmap_update_bits(sunxi_codec->regmap, SUNXI_DAC_DPC,
				(0x1 << EN_DA), (0x0 << EN_DA));
		break;
	default:
		break;
	}
	return 0;
}

static int sunxi_codec_capture_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);

	pr_warn("[%s] start; event:%d, adc_en:%d\n", __func__,
		event, sunxi_codec->adc_en);
	switch (event) {
	case	SND_SOC_DAPM_POST_PMU:
		if (sunxi_codec->adc_en++ == 0)
			regmap_update_bits(sunxi_codec->regmap,
				SUNXI_ADC_FIFO_CTL,
				(0x1 << EN_AD), (0x1 << EN_AD));
		break;
	case	SND_SOC_DAPM_POST_PMD:
		sunxi_codec->adc_en--;
		if (sunxi_codec->adc_en == 0)
			regmap_update_bits(sunxi_codec->regmap,
				SUNXI_ADC_FIFO_CTL,
				(0x1 << EN_AD), (0x0 << EN_AD));
		break;
	default:
		break;
	}
	pr_warn("[%s] end; event:%d, adc_en:%d\n", __func__,
		event, sunxi_codec->adc_en);
	return 0;
}

static const struct snd_kcontrol_new sunxi_codec_controls[] = {
	SOC_ENUM_EXT("codec hub mode", sunxi_codec_hub_mode_enum[0],
				sunxi_codec_get_hub_mode,
				sunxi_codec_set_hub_mode),

	SOC_SINGLE_TLV("digital volume", SUNXI_DAC_DPC,
				DVOL, 0x3F, 0, digital_tlv),

	SOC_SINGLE_TLV("LINEIN to output mixer gain control",
				AC_MIXER_REG,
				LINEING, 0x7, 0, linein_to_l_r_mix_vol_tlv),
	SOC_SINGLE_TLV("FMIN to output mixer gain control",
				AC_MIXER_REG,
				FMING, 0x7, 0, fmin_to_l_r_mix_vol_tlv),

	SOC_SINGLE_TLV("LINEOUT volume", AC_DAC_REG,
					LINEOUT_VOL, 0x1F, 0, lineout_tlv),

	SOC_SINGLE("ADDA Loopback Debug", SUNXI_DAC_DG_REG, ADDA_LOOP_MODE, 1, 0),
};

static const struct snd_kcontrol_new adc_input_mixer[] = {
	SOC_DAPM_SINGLE("ADCL Switch", SUNXI_ADC_FIFO_CTL, ADCL_EN, 1, 0),
	SOC_DAPM_SINGLE("ADCR Switch", SUNXI_ADC_FIFO_CTL, ADCR_EN, 1, 0),
	SOC_DAPM_SINGLE("ADCX Switch", SUNXI_ADC_FIFO_CTL, ADCX_EN, 1, 0),
	SOC_DAPM_SINGLE("ADCY Switch", SUNXI_ADC_FIFO_CTL, ADCY_EN, 1, 0),
};

static const struct snd_kcontrol_new left_output_mixer[] = {
	SOC_DAPM_SINGLE("DACL Switch", AC_MIXER_REG, LMIX_LDAC, 1, 0),
	SOC_DAPM_SINGLE("DACR Switch", AC_MIXER_REG, LMIX_RDAC, 1, 0),
	SOC_DAPM_SINGLE("FMINL Switch", AC_MIXER_REG, LMIX_FMINL, 1, 0),
	SOC_DAPM_SINGLE("LINEINL Switch", AC_MIXER_REG, LMIX_LINEINL, 1, 0),
};

static const struct snd_kcontrol_new right_output_mixer[] = {
	SOC_DAPM_SINGLE("DACL Switch", AC_MIXER_REG, RMIX_LDAC, 1, 0),
	SOC_DAPM_SINGLE("DACR Switch", AC_MIXER_REG, RMIX_RDAC, 1, 0),
	SOC_DAPM_SINGLE("FMINR Switch", AC_MIXER_REG, RMIX_FMINR, 1, 0),
	SOC_DAPM_SINGLE("LINEINR Switch", AC_MIXER_REG, RMIX_LINEINR, 1, 0),
};

static const struct snd_soc_dapm_widget sunxi_codec_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN_E("DACL", "Playback", 0, AC_DAC_REG, DACLEN, 0,
				sunxi_codec_playback_event,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("DACR", "Playback", 0, AC_DAC_REG, DACREN, 0,
				sunxi_codec_playback_event,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_OUT_E("ADC", "Capture", 0, SND_SOC_NOPM, 0, 0,
				sunxi_codec_capture_event,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("Left Output Mixer", AC_MIXER_REG, LMIXEN, 0,
			left_output_mixer, ARRAY_SIZE(left_output_mixer)),
	SND_SOC_DAPM_MIXER("Right Output Mixer", AC_MIXER_REG, RMIXEN, 0,
			right_output_mixer, ARRAY_SIZE(right_output_mixer)),
	SND_SOC_DAPM_MIXER("ADC Input Mixer", SND_SOC_NOPM, 0, 0,
			adc_input_mixer, ARRAY_SIZE(adc_input_mixer)),

	SND_SOC_DAPM_MUX("Left LINEOUT Mux", SND_SOC_NOPM,
			0, 0, &left_lineout_mux),
	SND_SOC_DAPM_MUX("Right LINEOUT Mux", SND_SOC_NOPM,
			0, 0, &right_lineout_mux),

	SND_SOC_DAPM_INPUT("ADCL"),
	SND_SOC_DAPM_INPUT("ADCR"),
	SND_SOC_DAPM_INPUT("ADCX"),
	SND_SOC_DAPM_INPUT("ADCY"),
	SND_SOC_DAPM_INPUT("LINEINL"),
	SND_SOC_DAPM_INPUT("LINEINR"),
	SND_SOC_DAPM_INPUT("FMINL"),
	SND_SOC_DAPM_INPUT("FMINR"),

	SND_SOC_DAPM_OUTPUT("LINEOUTL"),
	SND_SOC_DAPM_OUTPUT("LINEOUTR"),

	SND_SOC_DAPM_LINE("LINEOUT", sunxi_lineout_event),
};

static const struct snd_soc_dapm_route sunxi_codec_dapm_routes[] = {
	{"ADC Input Mixer", "ADCL Switch", "ADCL"},
	{"ADC Input Mixer", "ADCR Switch", "ADCR"},
	{"ADC Input Mixer", "ADCX Switch", "ADCX"},
	{"ADC Input Mixer", "ADCY Switch", "ADCY"},

	{"ADC", NULL, "ADC Input Mixer"},

	{"Left Output Mixer", "DACR Switch", "DACR"},
	{"Left Output Mixer", "DACL Switch", "DACL"},
	{"Left Output Mixer", "LINEINL Switch", "LINEINL"},
	{"Left Output Mixer", "FMINL Switch", "FMINL"},

	{"Right Output Mixer", "DACL Switch", "DACL"},
	{"Right Output Mixer", "DACR Switch", "DACR"},
	{"Right Output Mixer", "LINEINR Switch", "LINEINR"},
	{"Right Output Mixer", "FMINR Switch", "FMINR"},

	{"Left LINEOUT Mux", "LOMixer", "Left Output Mixer"},
	{"Left LINEOUT Mux", "LROMixer", "Right Output Mixer"},
	{"Right LINEOUT Mux", "ROMixer", "Right Output Mixer"},
	{"Right LINEOUT Mux", "LROMixer", "Left Output Mixer"},

	{"LINEOUTL", NULL, "Left LINEOUT Mux"},
	{"LINEOUTR", NULL, "Right LINEOUT Mux"},
};

static void sunxi_codec_init(struct snd_soc_codec *codec)
{
	struct sunxi_codec_info *sunxi_codec =
			snd_soc_codec_get_drvdata(codec);
	struct gain_config *gain_cfg = &sunxi_codec->gain_cfg;

	sunxi_codec->adc_en = 0;
	sunxi_codec->dac_en = 0;
	sunxi_codec->dacdap_en = 0;
	sunxi_codec->adcdap_en = 0;

	/* Disable DRC function for playback */
	regmap_write(sunxi_codec->regmap, SUNXI_DAC_DAP_CTL, 0);

	/* Enable HPF(high passed filter) */
	regmap_update_bits(sunxi_codec->regmap, SUNXI_DAC_DPC,
			(0x1 << HPF_EN), (0x1 << HPF_EN));

	/* Enable ADCFDT to overcome niose at the beginning */
	regmap_update_bits(sunxi_codec->regmap, SUNXI_ADC_FIFO_CTL,
			(0x7 << ADCDFEN), (0x7 << ADCDFEN));

	/* setup gain */
	regmap_update_bits(sunxi_codec->regmap, AC_MIXER_REG,
			(0x7 << LINEING), (gain_cfg->linein_gain << LINEING));
	regmap_update_bits(sunxi_codec->regmap, AC_MIXER_REG,
			(0x7 << FMING), (gain_cfg->fmin_gain << FMING));
	regmap_update_bits(sunxi_codec->regmap, AC_DAC_REG,
			(0x1f << LINEOUT_VOL),
			(gain_cfg->lineout_vol << LINEOUT_VOL));

	regmap_update_bits(sunxi_codec->regmap, AC_DAC_REG,
			(0x1f << SUNXI_DAC_DPC),
			(gain_cfg->digital_vol << DVOL));

	/* Enable output ramp */
	regmap_update_bits(sunxi_codec->regmap, AC_DAC_REG,
			(0x1 << RAMPEN), (0x1 << RAMPEN));

	if (sunxi_codec->hw_cfg.adcdrc_cfg)
		adcdrc_config(codec);

	if (sunxi_codec->hw_cfg.adchpf_cfg)
		adchpf_config(codec);

	if (sunxi_codec->hw_cfg.dacdrc_cfg)
		dacdrc_config(codec);

	if (sunxi_codec->hw_cfg.dachpf_cfg)
		dachpf_config(codec);
}

static int sunxi_codec_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);
	int i = 0;

	switch (params_format(params)) {
	case	SNDRV_PCM_FORMAT_S16_LE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_DAC_FIFO_CTL,
					(0x3 << DAC_FIFO_MODE),
					(0x3 << DAC_FIFO_MODE));
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_DAC_FIFO_CTL,
					(0x1 << TX_SAMPLE_BITS),
					(0x0 << TX_SAMPLE_BITS));
		} else {
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_ADC_FIFO_CTL,
					(0x1 << RX_FIFO_MODE),
					(0x1 << RX_FIFO_MODE));
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_ADC_FIFO_CTL,
					(0x1 << RX_SAMPLE_BITS),
					(0x0 << RX_SAMPLE_BITS));
		}
		break;
	case	SNDRV_PCM_FORMAT_S24_LE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_DAC_FIFO_CTL,
					(0x3 << DAC_FIFO_MODE),
					(0x0 << DAC_FIFO_MODE));
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_DAC_FIFO_CTL,
					(0x1 << TX_SAMPLE_BITS),
					(0x1 << TX_SAMPLE_BITS));
		} else {
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_ADC_FIFO_CTL,
					(0x1 << RX_FIFO_MODE),
					(0x0 << RX_FIFO_MODE));
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_ADC_FIFO_CTL,
					(0x1 << RX_SAMPLE_BITS),
					(0x1 << RX_SAMPLE_BITS));
		}
		break;
	default:
		break;
	}

	for (i = 0; i < ARRAY_SIZE(sample_rate_conv); i++) {
		if (sample_rate_conv[i].samplerate == params_rate(params)) {
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				regmap_update_bits(sunxi_codec->regmap,
					SUNXI_DAC_FIFO_CTL,
					(0x7 << DAC_FS),
					(sample_rate_conv[i].rate_bit << DAC_FS));
			} else {
				if (sample_rate_conv[i].samplerate > 48000)
					return -EINVAL;
				regmap_update_bits(sunxi_codec->regmap,
					SUNXI_ADC_FIFO_CTL,
					(0x7 << ADC_FS),
					(sample_rate_conv[i].rate_bit << ADC_FS));
			}
		}
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (params_channels(params)) {
		case 1:
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_DAC_FIFO_CTL,
					(0x1 << DAC_MONO_EN),
					(0x1 << DAC_MONO_EN));
			break;
		case 2:
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_DAC_FIFO_CTL,
					(0x1 << DAC_MONO_EN),
					(0x0 << DAC_MONO_EN));
			break;
		default:
			pr_err("[%s] Playback cannot support %d channels.\n",
				__func__, params_channels(params));
			return -EINVAL;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		switch (params_channels(params)) {
		case 1:
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_ADC_FIFO_CTL,
					(0xf << ADC_CHAN_SEL),
					(0x1 << ADC_CHAN_SEL));
			break;
		case 2:
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_ADC_FIFO_CTL,
					(0xf << ADC_CHAN_SEL),
					(0x3 << ADC_CHAN_SEL));
			break;
		case 3:
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_ADC_FIFO_CTL,
					(0xf << ADC_CHAN_SEL),
					(0x7 << ADC_CHAN_SEL));
			break;
		case 4:
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_ADC_FIFO_CTL,
					(0xf << ADC_CHAN_SEL),
					(0xf << ADC_CHAN_SEL));
			break;
		default:
			pr_err("[%s] Capture cannot support %d channels.\n",
				__func__, params_channels(params));
			return -EINVAL;
		}
	}

	return 0;
}

static int sunxi_codec_set_sysclk(struct snd_soc_dai *dai,
			int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);

	if (clk_set_rate(sunxi_codec->pllclk, freq)) {
		dev_err(sunxi_codec->dev, "set pllclk rate failed\n");
		return -EINVAL;
	}
	return 0;
}

static int sunxi_codec_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (sunxi_codec->hw_cfg.dachpf_cfg)
			dachpf_enable(codec, true);
	} else {
		if (sunxi_codec->hw_cfg.adcdrc_cfg)
			adcdrc_enable(codec, true);

		if (sunxi_codec->hw_cfg.adchpf_cfg)
			adchpf_enable(codec, true);
	}

	return 0;
}

static void sunxi_codec_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (sunxi_codec->hw_cfg.dachpf_cfg)
			dachpf_enable(codec, false);
	} else {
		if (sunxi_codec->hw_cfg.adcdrc_cfg)
			adcdrc_enable(codec, false);

		if (sunxi_codec->hw_cfg.adchpf_cfg)
			adchpf_enable(codec, false);
	}
}

static int sunxi_codec_digital_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

static int sunxi_codec_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		regmap_update_bits(sunxi_codec->regmap, SUNXI_DAC_FIFO_CTL,
				(0x1 << DAC_FIFO_FLUSH),
				(0x1 << DAC_FIFO_FLUSH));
		regmap_write(sunxi_codec->regmap, SUNXI_DAC_FIFO_STA,
			(0x1 << DAC_TXE_INT | 1 << DAC_TXU_INT | 0x1 << DAC_TXO_INT));
		regmap_write(sunxi_codec->regmap, SUNXI_DAC_CNT, 0);
	} else {
		regmap_update_bits(sunxi_codec->regmap, SUNXI_ADC_FIFO_CTL,
				(0x1 << ADC_FIFO_FLUSH),
				(0x1 << ADC_FIFO_FLUSH));
		regmap_write(sunxi_codec->regmap, SUNXI_ADC_FIFO_STA,
				(0x1 << ADC_RXA_INT | 0x1 << ADC_RXO_INT));
		regmap_write(sunxi_codec->regmap, SUNXI_ADC_CNT, 0);
	}
	return 0;
}

static int sunxi_codec_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);

	switch (cmd) {
	case	SNDRV_PCM_TRIGGER_START:
	case	SNDRV_PCM_TRIGGER_RESUME:
	case	SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_DAC_FIFO_CTL,
					(0x1 << DAC_DRQ_EN),
					(0x1 << DAC_DRQ_EN));
		else
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_ADC_FIFO_CTL,
					(0x1 << ADC_DRQ_EN),
					(0x1 << ADC_DRQ_EN));
		break;
	case	SNDRV_PCM_TRIGGER_STOP:
	case	SNDRV_PCM_TRIGGER_SUSPEND:
	case	SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_DAC_FIFO_CTL,
					(0x1 << DAC_DRQ_EN),
					(0x0 << DAC_DRQ_EN));
		else
			regmap_update_bits(sunxi_codec->regmap,
					SUNXI_ADC_FIFO_CTL,
					(0x1 << ADC_DRQ_EN),
					(0x0 << ADC_DRQ_EN));
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct snd_soc_dai_ops sunxi_codec_dai_ops = {
	.startup	= sunxi_codec_startup,
	.hw_params	= sunxi_codec_hw_params,
	.set_sysclk	= sunxi_codec_set_sysclk,
	.digital_mute	= sunxi_codec_digital_mute,
	.prepare	= sunxi_codec_prepare,
	.trigger	= sunxi_codec_trigger,
	.shutdown	= sunxi_codec_shutdown,
};

static struct snd_soc_dai_driver sunxi_codec_dai[] = {
	{
		.name	= "sun50iw9-codec",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates	= SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT,
			.formats = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = SNDRV_PCM_RATE_8000_48000
				| SNDRV_PCM_RATE_KNOT,
			.formats = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &sunxi_codec_dai_ops,
	},
};

static int sunxi_codec_probe(struct snd_soc_codec *codec)
{
	int ret;
	struct snd_soc_dapm_context *dapm = &codec->component.dapm;

	ret = snd_soc_add_codec_controls(codec, sunxi_codec_controls,
					 ARRAY_SIZE(sunxi_codec_controls));
	if (ret) {
		pr_err("failed to register codec controls!\n");
		return -EBUSY;
	}
	snd_soc_dapm_new_controls(dapm, sunxi_codec_dapm_widgets,
			ARRAY_SIZE(sunxi_codec_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, sunxi_codec_dapm_routes,
			ARRAY_SIZE(sunxi_codec_dapm_routes));
	sunxi_codec_init(codec);

	return 0;
}

static int sunxi_codec_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static int sunxi_gpio_iodisable(u32 gpio)
{
	char pin_name[8];
	u32 config, ret;

	sunxi_gpio_to_name(gpio, pin_name);
	config = 7 << 16;
	ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
	return ret;
}

static int save_audio_reg(struct sunxi_codec_info *sunxi_codec)
{
	int i = 0;

	while (reg_labels[i].name != NULL) {
		regmap_read(sunxi_codec->regmap, reg_labels[i].address,
				&(reg_labels[i].value));
		i++;
	}

	return i;
}

static int echo_audio_reg(struct sunxi_codec_info *sunxi_codec)
{
	int i = 0;

	while (reg_labels[i].name != NULL) {
		regmap_write(sunxi_codec->regmap, reg_labels[i].address,
				reg_labels[i].value);
		i++;
	}

	return i;
}

static int sunxi_codec_suspend(struct snd_soc_codec *codec)
{
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);
	struct spk_config *spk_gpio = &sunxi_codec->spk_cfg;

	pr_debug("Enter %s\n", __func__);

	if (spk_gpio->used)
		sunxi_gpio_iodisable(spk_gpio->gpio);

	save_audio_reg(sunxi_codec);

	clk_disable_unprepare(sunxi_codec->moduleclk);

	clk_disable_unprepare(sunxi_codec->pllclk);

	if (sunxi_codec->vol_supply.avcc)
		regulator_disable(sunxi_codec->vol_supply.avcc);

	pr_debug("End %s\n", __func__);

	return 0;
}

static int sunxi_codec_resume(struct snd_soc_codec *codec)
{
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);
	struct spk_config *spk_gpio = &sunxi_codec->spk_cfg;
	int ret = 0;

	pr_debug("Enter %s\n", __func__);

	if (sunxi_codec->vol_supply.avcc) {
		ret = regulator_enable(sunxi_codec->vol_supply.avcc);
		if (ret)
			pr_err("[%s]: avcc:enable() failed!\n", __func__);
	}

	if (clk_prepare_enable(sunxi_codec->pllclk)) {
		dev_err(sunxi_codec->dev, "enable pllclk failed, resume exit\n");
		return -EBUSY;
	}

	if (clk_prepare_enable(sunxi_codec->moduleclk)) {
		dev_err(sunxi_codec->dev, "enable  moduleclk failed, resume exit\n");
		clk_disable_unprepare(sunxi_codec->pllclk);
		return -EBUSY;
	}

	if (spk_gpio->used) {
		gpio_direction_output(spk_gpio->gpio, 1);
		gpio_set_value(spk_gpio->gpio, !spk_gpio->pa_ctl_level);
	}
	/* should waiting the avcc stable */
	msleep(100);
	sunxi_codec_init(codec);
	echo_audio_reg(sunxi_codec);

	pr_debug("End %s\n", __func__);

	return 0;
}

#ifdef SUNXI_CODEC_READ
static unsigned int sunxi_codec_read(struct snd_soc_codec *codec,
					unsigned int reg)
{
	struct sunxi_codec_info *sunxi_codec = snd_soc_codec_get_drvdata(codec);
	unsigned int reg_val;

	regmap_read(sunxi_codec->regmap, reg, &reg_val);

	return reg_val;
}

static int sunxi_codec_write(struct snd_soc_codec *codec,
				unsigned int reg, unsigned int val)
{
	struct sunxi_codec *sunxi_codec = snd_soc_codec_get_drvdata(codec);

	regmap_write(sunxi_codec->regmap, reg, val);

	return 0;
};
#endif

static struct snd_soc_codec_driver soc_codec_dev_sunxi = {
	.probe = sunxi_codec_probe,
	.remove = sunxi_codec_remove,
	.suspend = sunxi_codec_suspend,
	.resume = sunxi_codec_resume,
#ifdef SUNXI_CODEC_READ
	.read = sunxi_codec_read,
	.write = sunxi_codec_write,
#endif
	.ignore_pmdown_time = 1,
#if 0
	.controls = sunxi_codec_controls,
	.num_controls = ARRAY_SIZE(sunxi_codec_controls),
	.dapm_widgets = sunxi_codec_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sunxi_codec_dapm_widgets),
	.dapm_routes = sunxi_codec_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(sunxi_codec_dapm_routes),
#endif
};

static ssize_t show_audio_reg(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sunxi_codec_info *sunxi_codec = dev_get_drvdata(dev);
	int count = 0, i = 0;
	unsigned int reg_val;

	count += sprintf(buf, "dump audio reg:\n");

	while (reg_labels[i].name != NULL) {
		regmap_read(sunxi_codec->regmap, reg_labels[i].address, &reg_val);
		count += sprintf(buf + count, "[%s] \t0x%03x: 0x%08x\n",
			reg_labels[i].name, (reg_labels[i].address), reg_val);
		i++;
	}

	return count;
}

/* ex:
 * param 1: 0 read;1 write
 * param 2: reg value;
 * param 3: write value;
 *	read:
 *		echo 0,0x00 > audio_reg
 *		echo 0,0x00 > audio_reg
 *	write:
 *		echo 1,0x00,0xa > audio_reg
 *		echo 1,0x00,0xff > audio_reg
 */
static ssize_t store_audio_reg(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	int rw_flag;
	unsigned int input_reg_val = 0;
	unsigned int input_reg_offset = 0;
	struct sunxi_codec_info *sunxi_codec = dev_get_drvdata(dev);

	ret = sscanf(buf, "%d,0x%x,0x%x", &rw_flag,
			&input_reg_offset, &input_reg_val);
	dev_info(dev, "ret:%d, reg_offset:%d, reg_val:0x%x\n",
			ret, input_reg_offset, input_reg_val);

	if (!(rw_flag == 1 || rw_flag == 0)) {
		pr_err("not rw_flag\n");
		ret = count;
		goto out;
	}

	if (rw_flag)
		regmap_write(sunxi_codec->regmap, input_reg_offset, input_reg_val);
	else {
		regmap_read(sunxi_codec->regmap, input_reg_offset, &input_reg_val);
		dev_info(dev, "\n\n Reg[0x%03x] : 0x%08x\n\n",
					input_reg_offset, input_reg_val);
	}

	ret = count;

out:
	return ret;
}

static DEVICE_ATTR(audio_reg, 0644, show_audio_reg, store_audio_reg);

static struct attribute *audio_debug_attrs[] = {
	&dev_attr_audio_reg.attr,
	NULL,
};

static struct attribute_group audio_debug_attr_group = {
	.name   = "audio_reg_debug",
	.attrs  = audio_debug_attrs,
};

static const struct regmap_config sunxi_codec_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = AC_RAMP_REG,
	.cache_type = REGCACHE_NONE,
};

static int sunxi_internal_codec_probe(struct platform_device *pdev)
{
	struct sunxi_codec_info *sunxi_codec;
	struct device_node *np = pdev->dev.of_node;
	void __iomem *sunxi_digibase;
	struct spk_config *spk_gpio;
	int ret;
	unsigned int temp_val;

	sunxi_codec = devm_kzalloc(&pdev->dev, sizeof(struct sunxi_codec_info),
				GFP_KERNEL);
	if (!sunxi_codec) {
		dev_err(&pdev->dev, "Can't allocate sunxi codec memory\n");
		ret = -ENOMEM;
		goto err_node_put;
	}
	dev_set_drvdata(&pdev->dev, sunxi_codec);
	sunxi_codec->dev = &pdev->dev;

	sunxi_codec->pllclk = of_clk_get(np, 0);
	sunxi_codec->moduleclk = of_clk_get(np, 1);
	if (IS_ERR_OR_NULL(sunxi_codec->pllclk)) {
		dev_err(&pdev->dev, "pllclk not exist or invaild\n");
		ret = PTR_ERR(sunxi_codec->pllclk);
		goto err_devm_kfree;
	} else {
		if (IS_ERR_OR_NULL(sunxi_codec->moduleclk)) {
			dev_err(&pdev->dev, "moduleclk not exist or invaild\n");
			ret = PTR_ERR(sunxi_codec->moduleclk);
			goto err_devm_kfree;
		} else {
			if (clk_set_parent(sunxi_codec->moduleclk,
					sunxi_codec->pllclk)) {
				dev_err(&pdev->dev, "set parent of moduleclk to pllclk failed\n");
				ret = -EBUSY;
				goto err_devm_kfree;
			}
			if (clk_prepare_enable(sunxi_codec->pllclk)) {
				dev_err(&pdev->dev, "pllclk enable failed\n");
				ret = -EBUSY;
				goto err_devm_kfree;
			}
			if (clk_prepare_enable(sunxi_codec->moduleclk)) {
				dev_err(&pdev->dev, "moduleclk enable failed\n");
				ret = -EBUSY;
				goto err_pllclk_put;
			}
		}
	}

	sunxi_codec->digitbase = of_iomap(np, 0);
	if (sunxi_codec->digitbase == NULL) {
		dev_err(&pdev->dev, "digital register iomap failed\n");
		ret = -EINVAL;
		goto err_moduleclk_put;
	}

	sunxi_codec->regmap = devm_regmap_init_mmio(&pdev->dev,
				sunxi_codec->digitbase, &sunxi_codec_regmap_config);
	if (IS_ERR(sunxi_codec->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(sunxi_codec->regmap);
		goto err_digital_iounmap;
	}

	ret = of_property_read_u32(np, "lineout_vol", &temp_val);
	if (ret < 0) {
		dev_warn(&pdev->dev, "lineout volume get failed\n");
		sunxi_codec->gain_cfg.lineout_vol = 0;
	} else {
		sunxi_codec->gain_cfg.lineout_vol = temp_val;
	}

	ret = of_property_read_u32(np, "linein_gain", &temp_val);
	if (ret < 0) {
		dev_warn(&pdev->dev, "linein gain get failed\n");
		sunxi_codec->gain_cfg.linein_gain = 0;
	} else {
		sunxi_codec->gain_cfg.linein_gain = temp_val;
	}

	ret = of_property_read_u32(np, "fmin_gain", &temp_val);
	if (ret < 0) {
		dev_warn(&pdev->dev, "fmin gain get failed\n");
		sunxi_codec->gain_cfg.fmin_gain = 0;
	} else {
		sunxi_codec->gain_cfg.fmin_gain = temp_val;
	}

	ret = of_property_read_u32(np, "digital_vol", &temp_val);
	if (ret < 0) {
		dev_warn(&pdev->dev, "digital volume get failed\n");
		sunxi_codec->gain_cfg.digital_vol = 0;
	} else {
		sunxi_codec->gain_cfg.digital_vol = temp_val;
	}

	pr_debug("lineout_vol:%d, linein_gain:%d, fmin_gain:%d, "
		"digital_vol:%d, pa_msleep_time:%d\n",
		sunxi_codec->gain_cfg.lineout_vol,
		sunxi_codec->gain_cfg.linein_gain,
		sunxi_codec->gain_cfg.fmin_gain,
		sunxi_codec->gain_cfg.digital_vol,
		sunxi_codec->spk_cfg.pa_msleep_time
	);

	ret = of_property_read_u32(np, "adcdrc_cfg", &temp_val);
	if (ret < 0) {
		pr_err("[audio-codec]adcdrc_cfg configurations missing or invalid.\n");
		ret = -EINVAL;
	} else {
		sunxi_codec->hw_cfg.adcdrc_cfg = temp_val;
	}

	ret = of_property_read_u32(np, "adchpf_cfg", &temp_val);
	if (ret < 0) {
		pr_err("[audio-codec]adchpf_cfg configurations missing or invalid.\n");
		ret = -EINVAL;
	} else {
		sunxi_codec->hw_cfg.adchpf_cfg = temp_val;
	}

	ret = of_property_read_u32(np, "dacdrc_cfg", &temp_val);
	if (ret < 0) {
		pr_err("[audio-codec]dacdrc_cfg configurations missing or invalid.\n");
		ret = -EINVAL;
	} else {
		sunxi_codec->hw_cfg.dacdrc_cfg = temp_val;
	}

	ret = of_property_read_u32(np, "dachpf_cfg", &temp_val);
	if (ret < 0) {
		pr_err("[audio-codec]dachpf_cfg configurations missing or invalid.\n");
		ret = -EINVAL;
	} else {
		sunxi_codec->hw_cfg.dachpf_cfg = temp_val;
	}

	spk_gpio = &sunxi_codec->spk_cfg;
	ret = of_property_read_u32(np, "pa_msleep_time", &temp_val);
	if (ret < 0) {
		dev_warn(&pdev->dev, "pa_msleep_time get failed\n");
		spk_gpio->pa_msleep_time = 160;
	} else {
		spk_gpio->pa_msleep_time = temp_val;
	}

	ret = of_property_read_u32(np, "pa_ctl_level", &temp_val);
	if (ret < 0) {
		pr_err("[audio-codec]dachpf_cfg configurations missing or invalid.\n");
		ret = -EINVAL;
	} else {
		spk_gpio->pa_ctl_level = temp_val;
	}

	ret = of_get_named_gpio(np, "gpio-spk", 0);
	if (ret >= 0) {
		spk_gpio->used = 1;
		spk_gpio->gpio = ret;
		if (!gpio_is_valid(spk_gpio->gpio)) {
			dev_err(&pdev->dev, "gpio-spk is invalid\n");
			ret = -EINVAL;
			goto err_digital_iounmap;
		}

		ret = devm_gpio_request(&pdev->dev, spk_gpio->gpio, "SPK");
		if (ret) {
			dev_err(&pdev->dev, "failed to request gpio-spk gpio\n");
			ret = -EBUSY;
			goto err_digital_iounmap;
		}

		gpio_direction_output(spk_gpio->gpio, 1);
		gpio_set_value(spk_gpio->gpio, !spk_gpio->pa_ctl_level);
	} else
		spk_gpio->used = 0;

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_sunxi,
				sunxi_codec_dai, ARRAY_SIZE(sunxi_codec_dai));
	if (ret < 0) {
		dev_err(&pdev->dev, "register codec failed\n");
		goto err_digital_iounmap;
	}

	ret  = sysfs_create_group(&pdev->dev.kobj, &audio_debug_attr_group);
	if (ret)
		dev_warn(&pdev->dev, "failed to create attr group\n");
	return 0;

err_digital_iounmap:
	iounmap(sunxi_digibase);
err_moduleclk_put:
	clk_disable_unprepare(sunxi_codec->moduleclk);
err_pllclk_put:
	clk_disable_unprepare(sunxi_codec->pllclk);
err_devm_kfree:
	devm_kfree(&pdev->dev, sunxi_codec);
err_node_put:
	of_node_put(np);
	return ret;
}

static int  __exit sunxi_internal_codec_remove(struct platform_device *pdev)
{
	struct sunxi_codec_info *sunxi_codec = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_codec(&pdev->dev);
	clk_put(sunxi_codec->moduleclk);
	clk_put(sunxi_codec->pllclk);
	devm_kfree(&pdev->dev, sunxi_codec);

	return 0;
}

static const struct of_device_id sunxi_internal_codec_of_match[] = {
	{ .compatible = "allwinner,sunxi-internal-codec", },
	{},
};

static struct platform_driver sunxi_internal_codec_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = sunxi_internal_codec_of_match,
	},
	.probe = sunxi_internal_codec_probe,
	.remove = __exit_p(sunxi_internal_codec_remove),
};

module_platform_driver(sunxi_internal_codec_driver);

MODULE_DESCRIPTION("SUNXI Codec ASoC driver");
MODULE_AUTHOR("yumingfeng <yumingfeng@allwinnertech.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sunxi-internal-codec");
