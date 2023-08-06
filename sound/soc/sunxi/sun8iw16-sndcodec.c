/*
 * sound\soc\sunxi\sun8iw16-sndcodec.c
 * (C) Copyright 2014-2018
 * Reuuimlla Technology Co., Ltd. <www.reuuimllatech.com>
 * huangxin <huangxin@Reuuimllatech.com>
 * liushaohua <liushaohua@allwinnertech.com>
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
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/input.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/of.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <linux/delay.h>
#include <linux/debugfs.h>

#include "sunxi_rw_func.h"
#include "sun8iw16-codec.h"

struct mc_private {
	struct snd_soc_codec *codec;
	u32 aif2master;
	u32 aif2fmt;
	u32 aif3fmt;
};

static struct snd_soc_dai *card0_device0_interface;
struct snd_soc_pcm_runtime *snd_rtd;


static const struct snd_kcontrol_new ac_pin_controls[] = {
	SOC_DAPM_PIN_SWITCH("Lineout"),
};

static const struct snd_soc_dapm_widget sunxi_ac_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("External MainMic", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	{"MainMic Bias", NULL, "External MainMic"},
	{"MIC1P", NULL, "MainMic Bias"},
	{"MIC1N", NULL, "MainMic Bias"},

	{"MIC2P", NULL, "MainMic Bias"},
	{"MIC2N", NULL, "MainMic Bias"},
};


/*
 * Card initialization
 */
static int sunxi_audio_init(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_codec *codec = runtime->codec;
	struct snd_soc_dapm_context *dapm = &codec->component.dapm;
	struct mc_private *ctx = snd_soc_card_get_drvdata(runtime->card);

	card0_device0_interface = runtime->cpu_dai;
	ctx->codec = runtime->codec;
	snd_soc_dapm_disable_pin(dapm, "LINEOUTL");
	snd_soc_dapm_disable_pin(dapm, "LINEOUTR");
	snd_soc_dapm_disable_pin(&runtime->card->dapm, "Lineout");
	snd_soc_dapm_sync(dapm);
	return 0;
}
static int sunxi_sndpcm_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params)
{
	int ret = 0;
	u32 freq_in = 22579200;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned long sample_rate = params_rate(params);

	switch (sample_rate) {
	case 8000:
	case 16000:
	case 32000:
	case 64000:
	case 128000:
	case 12000:
	case 24000:
	case 48000:
	case 96000:
	case 192000:
		freq_in = 24576000;
		break;
	}
	/* set the codec FLL */
	ret = snd_soc_dai_set_pll(codec_dai, PLLCLK, 0, freq_in, freq_in);
	if (ret < 0) {
		pr_err("err:%s,set codec dai pll failed.\n", __func__);
		return ret;
	}
	/*set system clock source freq */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, freq_in, 0);
	if (ret < 0) {
		pr_err("err:%s,set cpu dai sysclk failed.\n", __func__);
		return ret;
	}
	/*set system clock source freq_in and set the mode as tdm or pcm*/
	ret = snd_soc_dai_set_sysclk(codec_dai, AIF1_CLK, freq_in, 0);
	if (ret < 0) {
		pr_err("err:%s,set codec dai sysclk faided.\n", __func__);
		return ret;
	}
	/*set system fmt:api2s:master aif1:slave*/
	ret = snd_soc_dai_set_fmt(cpu_dai, 0);
	if (ret < 0) {
		pr_err("%s,set cpu dai fmt failed.\n", __func__);
		return ret;
	}

	/*
	* codec: slave. AP: master
	*/
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
						 SND_SOC_DAIFMT_NB_NF |
						 SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		pr_err("%s,set codec dai fmt failed.\n", __func__);
		return ret;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, 0, sample_rate);
	if (ret < 0) {
		pr_err("%s, set cpu dai clkdiv faided.\n", __func__);
		return ret;
	}

	return 0;
}

static struct snd_soc_ops sunxi_sndpcm_ops = {
	.hw_params = sunxi_sndpcm_hw_params,
};

#ifdef CODEC_AIF2_AIF3_ENABLE
static int bb_voice_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	struct mc_private *ctx = NULL;

	int ret = 0;
	int freq_in = 24576000;
	if (params_rate(params) != 8000)
		pr_warn("%s,line:%d,params_rate(params):%d\n", __func__,
			   __LINE__, params_rate(params));

	ctx = snd_soc_card_get_drvdata(card);
	if (ctx == NULL) {
		pr_err("err:%s,get ctx failed.\n", __func__);
		return -EINVAL;
	}

	/*set system clock source freq */
	ret = snd_soc_dai_set_sysclk(card0_device0_interface, 0, freq_in, 0);
	if (ret < 0) {
		pr_err("err:%s,set cpu dai sysclk failed.\n", __func__);
		return ret;
	}

	/* set the codec aif1clk/aif2clk from pllclk */
	ret = snd_soc_dai_set_pll(codec_dai, PLLCLK, 0, freq_in, freq_in);
	if (ret < 0) {
		pr_err("err:%s,set codec dai pll failed.\n", __func__);
		return ret;
	}
	/*set system clock source aif2*/
	ret = snd_soc_dai_set_sysclk(codec_dai, AIF2_CLK, 0, 0);
	if (ret < 0) {
		pr_err("err:%s,set codec dai sysclk faied\n", __func__);
		return ret;
	}
	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
						 (ctx->aif2fmt << 8) |
						 (ctx->aif2master << 12));
	if (ret < 0)
		pr_err("err:%s,set codec dai fmt failed.\n", __func__);

	return ret;
}

static struct snd_soc_ops bb_voice_ops = {
	.hw_params = bb_voice_hw_params,
};

static int bb_clk_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	struct mc_private *ctx = NULL;

	int ret = 0;
	int freq_in = 24576000;
	if (params_rate(params) != 8000)
		pr_warn("%s,line:%d,params_rate(params):%d\n", __func__,
			   __LINE__, params_rate(params));

	ctx = snd_soc_card_get_drvdata(card);
	if (ctx == NULL) {
		pr_err("err:%s,get ctx failed.\n", __func__);
		return -EINVAL;
	}
	/*set system clock source freq */
	ret = snd_soc_dai_set_sysclk(card0_device0_interface, 0, freq_in, 0);
	if (ret < 0) {
		pr_err("err:%s,set cpu dai sysclk failed.\n", __func__);
		return ret;
	}
	/* set the codec aif1clk/aif2clk from pllclk */
	ret = snd_soc_dai_set_pll(codec_dai, PLLCLK, 0, freq_in, freq_in);
	if (ret < 0) {
		pr_err("err:%s,set codec dai pll failed.\n", __func__);
		return ret;
	}
	/*set system clock source aif2*/
	ret = snd_soc_dai_set_sysclk(codec_dai, AIF2_CLK, 0, 0);
	if (ret < 0) {
		pr_err("err:%s,set codec dai sysclk faied\n", __func__);
		return ret;
	}
	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_A |
						 SND_SOC_DAIFMT_IB_NF |
						 SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		pr_err("err:%s,set codec dai fmt failed.\n", __func__);

	return ret;
}

static struct snd_soc_ops bb_clk_ops = {
	.hw_params = bb_clk_hw_params,
};

static int bt_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct mc_private *ctx = NULL;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	int ret = 0;
	if (params_rate(params) != 8000)
		pr_warn("%s,line:%d,params_rate(params):%d\n", __func__,
			   __LINE__, params_rate(params));
	ctx = snd_soc_card_get_drvdata(card);
	if (ctx == NULL) {
		pr_err("err:%s,get ctx failed.\n", __func__);
		return -EINVAL;
	}
	/* set codec aif3 configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, (ctx->aif3fmt << 8));
	if (ret < 0)
		return ret;
	return 0;
}

static struct snd_soc_ops bt_voice_ops = {
	.hw_params = bt_hw_params,
};
#endif

static struct snd_soc_dai_link sunxi_sndpcm_dai_link[] = {
	{
	.name = "audiocodec",
	.stream_name	= "SUNXI-CODEC",
	.cpu_dai_name	= "sunxi-internal-i2s",
	.codec_dai_name = "codec-aif1",
	.platform_name	= "sunxi-internal-i2s",
	.codec_name	= "sunxi-internal-codec",
	.init		= sunxi_audio_init,
	.ops = &sunxi_sndpcm_ops,

	},

#ifdef CODEC_AIF2_AIF3_ENABLE
/**/
	{
	.name = "Voice",
	.stream_name = "bb Voice",
	.cpu_dai_name = "bb-dai",
	.codec_dai_name = "codec-aif2",
	.codec_name = "sunxi-pcm-codec",
	.ops = &bb_voice_ops,
	},
/**/
	{
	.name = "bbclk",
	.stream_name = "bb-bt-clk",
	.cpu_dai_name = "bb-dai",
	.codec_dai_name = "codec-aif2",
	.codec_name = "sunxi-pcm-codec",
	.ops = &bb_clk_ops,
	},
	{
	.name = "bt",
	.stream_name = "bt Voice",
	.cpu_dai_name = "bb-dai",
	.codec_dai_name = "codec-aif3",
	.codec_name = "sunxi-pcm-codec",
	.ops = &bt_voice_ops,
	},
#ifdef CONFIG_SND_SOC_HUB
	{
	/*hub-spdif*/
	},
	{
	/*hub-hdmi*/
	},
#endif

#endif
/**/
};

static int sunxi_suspend(struct snd_soc_card *card)
{
	pr_debug("[codec-machine]  suspend.\n");
	return 0;
}

static int sunxi_resume(struct snd_soc_card *card)
{
	return 0;
}

static struct snd_soc_card snd_soc_sunxi_sndpcm = {
	.name		= "audiocodec",
	.owner		= THIS_MODULE,
	.dai_link	= sunxi_sndpcm_dai_link,
	.num_links	= ARRAY_SIZE(sunxi_sndpcm_dai_link),
	.dapm_widgets	= sunxi_ac_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(sunxi_ac_dapm_widgets),
	.dapm_routes	= audio_map,
	.num_dapm_routes	= ARRAY_SIZE(audio_map),
	.controls	= ac_pin_controls,
	.num_controls	= ARRAY_SIZE(ac_pin_controls),
	.suspend_post	= sunxi_suspend,
	.resume_post	= sunxi_resume,
};

#ifdef CODEC_AIF2_AIF3_ENABLE
static struct snd_soc_dai_driver voice_dai[] = {
	{
		.name = "bb-dai",
		.id = 1,
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	},
	{
		.name = "no-dai",
		.id = 2,
	}
};

static const struct snd_soc_component_driver voice_component = {
	.name		= "bb-voice",
};
#endif

#ifdef CONFIG_DEBUG_FS
static ssize_t acodec_read(struct file *filep, char __user *user_buf, size_t count, loff_t *ppos)
{
	unsigned int ret;
	int acodecvol;
	int adclvol;
	int adcrvol;
	int daclvol;
	int dacrvol;
	int outvol;
	int mic1vol;
	bool mic1boost = 0;
	int mic2vol;
	bool mic2boost = 0;
	char *buf;
	struct snd_soc_pcm_runtime *rtd =
		snd_soc_get_pcm_runtime(&snd_soc_sunxi_sndpcm, "audiocodec");
	struct snd_soc_codec *codec = rtd->codec;
	static const char *str[3] = {"Off", "On", "dB"};

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = snd_soc_read(codec, SUNXI_DAC_DBG_CTRL);
	acodecvol = -(ret & 0x3f) * 116;
	ret = snd_soc_read(codec, SUNXI_DAC_VOL_CTRL);
	dacrvol = (ret & 0xff) * 75 - 12000;
	daclvol = ((ret >> 8) & 0xff) * 75 - 12000;
	ret = snd_soc_read(codec, SUNXI_ADC_VOL_CTRL);
	adcrvol = (ret & 0xff) * 75 - 12000;
	adclvol = ((ret >> 8)&0xff) * 75 - 12000;
	ret = snd_soc_read(codec, LINEOUT_CTRL1);
	outvol = (ret & 0x1f) * 15 - 435;
	if (outvol > 0)
		outvol = 0;
	ret = snd_soc_read(codec, MIC1_CTRL);
	mic1vol = (ret & 7) * 3 + 21;
	if (mic1vol < 24)
		mic1vol = 0;
	if (ret & 8)
		mic1boost = 1;
	else
		mic1boost = 0;
	ret = snd_soc_read(codec, MIC2_CTRL);
	mic2vol = (ret & 7) * 3 + 21;
	if (mic2vol < 24)
		mic2vol = 0;
	if (ret & 8)
		mic2boost = 1;
	else
		mic2boost = 0;
	ret = 0;
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "ACodecVol:%d %s\n", acodecvol, str[2]);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "AdcLVol:%d %s\n", adclvol, str[2]);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "AdcRVol:%d %s\n", adcrvol, str[2]);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "DacLVol:%d %s\n", daclvol, str[2]);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "DacRVol:%d %s\n", dacrvol, str[2]);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "OutVol:%d %s\n", outvol, str[2]);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "Mic1Vol:%d %s\n", mic1vol, str[2]);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "Mic1Boost:%s\n", str[mic1boost]);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "Mic2Vol:%d %s\n", mic2vol, str[2]);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "Mic2Boost:%s\n", str[mic2boost]);
	ret =  simple_read_from_buffer(user_buf, count, ppos, buf, ret);
	kfree(buf);
	return ret;
}

struct file_operations codec_fops = {
	.open = simple_open,
	.read = acodec_read,
	.llseek = default_llseek,
};

static ssize_t acodecin_read(struct file *filep, char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	unsigned int i;
	struct snd_pcm_runtime *rt;
	char *buf;
	unsigned int l_src;
	unsigned int r_src;
	unsigned int l_id;
	unsigned int r_id;
	static const char *str[4] = {"none", "mic", "linein", "mic and linein"};
	struct snd_soc_pcm_runtime *rtd =
		snd_soc_get_pcm_runtime(&snd_soc_sunxi_sndpcm, "audiocodec");
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_pcm_substream *substream = NULL;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	l_src = snd_soc_read(codec, L_ADCMIX_SRC);
	if (l_src & 0x64) {
		if ((l_src & 0x60) && (l_src & 0x04))
			l_id = 3;
		else if (l_src & 0x60)
			l_id = 1;
		else
			l_id = 2;
	} else
		l_id = 0;

	r_src = snd_soc_read(codec, R_ADCMIX_SRC);
	if (r_src & 0x64) {
		if ((r_src & 0x60) && (r_src & 0x04))
			r_id = 3;
		else if (r_src & 0x60)
			r_id = 1;
		else
			r_id = 2;
	} else
		r_id = 0;

	ret = 0;
	for (i = 0; i < ARRAY_SIZE(sunxi_sndpcm_dai_link); i++) {
		substream = snd_soc_get_substream(&snd_soc_sunxi_sndpcm,
					sunxi_sndpcm_dai_link[i].name, 1);
		rt = substream->runtime;
		if (!rt)
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "stream %d inactive\n", i);
		else {
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "stream %d AudioInput Param\n", i);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "BitWidth:%d\n", rt->sample_bits);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "ChnCnt:%d\n", rt->channels);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "SndMod:%d\n", rt->channels);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "PeriodSize:%ld\n", rt->period_size);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "PeriodCnt:%d\n", rt->periods);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "OverRunCnt:%d\n", rt->xrun_cnt);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "LeftIn:%s\n", str[l_id]);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "RightIn:%s\n", str[r_id]);
		}
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "================\n");
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);
	kfree(buf);
	return ret;
}

struct file_operations codecin_fops = {
	.open = simple_open,
	.read = acodecin_read,
	.llseek = default_llseek,
};

static ssize_t acodecout_read(struct file *filep, char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;
	unsigned int i;
	struct snd_pcm_runtime *rt;
	char *buf;
	struct snd_pcm_substream *substream = NULL;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	ret = 0;
	for (i = 0; i < ARRAY_SIZE(sunxi_sndpcm_dai_link); i++) {
		substream = snd_soc_get_substream(&snd_soc_sunxi_sndpcm,
					sunxi_sndpcm_dai_link[i].name, 0);
		rt = substream->runtime;
		if (!rt)
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "stream %d inactive\n", i);
		else {
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "stream %i playback param\n", i);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "BitWidth:%d\n", rt->sample_bits);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "ChnCnt:%d\n", rt->channels);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "SndMod:%d\n", rt->channels);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "PeriodSize:%ld\n", rt->period_size);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "PeriodCnt:%d\n", rt->periods);
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "OverRunCnt:%d\n", rt->xrun_cnt);
		}
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "====================\n");
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, ret);
	kfree(buf);
	return ret;

}

struct file_operations codecout_fops = {
	.open = simple_open,
	.read = acodecout_read,
	.llseek = default_llseek,
};

static void acodec_debug_init(void)
{
	struct dentry *codec_root;
	struct dentry *codec_dir;
	struct dentry *codecin_dir;
	struct dentry *codecout_dir;

	codec_root = debugfs_create_dir("sunxi", snd_soc_sunxi_sndpcm.debugfs_card_root);
	if (codec_root) {
		codec_dir = debugfs_create_file("acodec", 0444, codec_root, NULL, &codec_fops);
		if (!codec_dir)
			pr_err("create codec file failed!\n");
		codecin_dir = debugfs_create_file("acodec_in", 0444, codec_root, NULL, &codecin_fops);
		if (!codecin_dir)
			pr_err("create codec in file failed!\n");
		codecout_dir = debugfs_create_file("acodec_out", 0444, codec_root, NULL, &codecout_fops);
		if (!codecout_dir)
			pr_err("create codec out file failed!\n");

	} else {
		pr_err("create codec root failed ! \n");
	}
}
#endif

static int sunxi_machine_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 temp_val;
	struct mc_private *ctx = NULL;
	struct device_node *np = pdev->dev.of_node;

	if (!np) {
		dev_err(&pdev->dev, "can not get dt node for this device.\n");
		return -EINVAL;
	}

	ctx = devm_kzalloc(&pdev->dev, sizeof(struct mc_private), GFP_KERNEL);
	if (!ctx) {
		pr_err("[audio] allocation mem failed\n");
		return -ENOMEM;
	}

#ifdef CODEC_AIF2_AIF3_ENABLE
	/* register voice DAI here */
	ret = snd_soc_register_component(&pdev->dev, &voice_component,
					 voice_dai, ARRAY_SIZE(voice_dai));
	if (ret) {
		dev_err(&pdev->dev, "register DAI failed\n");
		goto err0;
	}
#endif

#ifdef CONFIG_SND_SOC_HUB
	/*
	*sunxi_sndpcm_dai_link[0]:audiocodec
	*sunxi_sndpcm_dai_link[1]:Voice
	*sunxi_sndpcm_dai_link[2]:bbclk
	*sunxi_sndpcm_dai_link[3]:bt
	*sunxi_sndpcm_dai_link[4]:hub-hdmi
	*sunxi_sndpcm_dai_link[5]:hub-spdif
	*/
	memcpy(sunxi_sndpcm_dai_link + 4, sunxi_hub_dai_link,
	       sizeof(sunxi_hub_dai_link));
#endif
	/* register the soc card */
	snd_soc_sunxi_sndpcm.dev = &pdev->dev;
	snd_soc_card_set_drvdata(&snd_soc_sunxi_sndpcm, ctx);
	platform_set_drvdata(pdev, &snd_soc_sunxi_sndpcm);
	ret = of_property_read_u32(np, "aif2fmt", &temp_val);
	if (ret < 0)
		pr_err("[audio]aif2fmt configurations missing or invalid.\n");
	else
		ctx->aif2fmt = temp_val;


	ret = of_property_read_u32(np, "aif3fmt", &temp_val);
	if (ret < 0)
		pr_err("[audio]aif3fmt configurations missing or invalid.\n");
	else
		ctx->aif3fmt = temp_val;

	ret = of_property_read_u32(np, "aif2master", &temp_val);
	if (ret < 0)
		pr_err(
		    "[audio]aif2master configurations missing or invalid.\n");
	else
		ctx->aif2master = temp_val;

	sunxi_sndpcm_dai_link[0].cpu_dai_name = NULL;
	sunxi_sndpcm_dai_link[0].cpu_of_node =
	    of_parse_phandle(np, "sunxi,i2s-controller", 0);
	if (!sunxi_sndpcm_dai_link[0].cpu_of_node) {
		dev_err(&pdev->dev,
			"Property 'sunxi,i2s-controller' missing or invalid\n");
		ret = -EINVAL;
		goto err1;
	}
	sunxi_sndpcm_dai_link[0].platform_name = NULL;
	sunxi_sndpcm_dai_link[0].platform_of_node =
	    sunxi_sndpcm_dai_link[0].cpu_of_node;

	sunxi_sndpcm_dai_link[0].codec_name = NULL;
	sunxi_sndpcm_dai_link[0].codec_of_node =
	    of_parse_phandle(np, "sunxi,audio-codec", 0);
	if (!sunxi_sndpcm_dai_link[0].codec_of_node) {
		dev_err(&pdev->dev,
			"Property 'sunxi,audio-codec' missing or invalid\n");
		ret = -EINVAL;
		goto err1;
	}
#ifdef CODEC_AIF2_AIF3_ENABLE
	sunxi_sndpcm_dai_link[1].codec_name = NULL;
	sunxi_sndpcm_dai_link[1].codec_of_node =
	    sunxi_sndpcm_dai_link[0].codec_of_node;

	sunxi_sndpcm_dai_link[2].codec_name = NULL;
	sunxi_sndpcm_dai_link[2].codec_of_node =
	    sunxi_sndpcm_dai_link[0].codec_of_node;

	sunxi_sndpcm_dai_link[3].codec_name = NULL;
	sunxi_sndpcm_dai_link[3].codec_of_node =
	    sunxi_sndpcm_dai_link[0].codec_of_node;
#endif

	ret = snd_soc_register_card(&snd_soc_sunxi_sndpcm);
	if (ret) {
		pr_err("snd_soc_register_card failed %d\n", ret);
		goto err1;
	}

#ifdef CONFIG_DEBUG_FS
	acodec_debug_init();
#endif
	return 0;
err1:
#ifdef CODEC_AIF2_AIF3_ENABLE
	snd_soc_unregister_component(&pdev->dev);
err0:
#endif
	return ret;
}


static void sunxi_machine_shutdown(struct platform_device *pdev)
{

}

static const struct of_device_id sunxi_machine_of_match[] = {
	{ .compatible = "allwinner,sunxi-codec-machine", },
	{},
};

/*method relating*/
static struct platform_driver sunxi_machine_driver = {
	.driver = {
		.name = "sunxi-codec-machine",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = sunxi_machine_of_match,
	},
	.probe = sunxi_machine_probe,
	.shutdown = sunxi_machine_shutdown,
};

static int __init sunxi_machine_init(void)
{
	int err = 0;
	err = platform_driver_register(&sunxi_machine_driver);
	if (err < 0)
		return err;

	return 0;
}
module_init(sunxi_machine_init);
static void __exit sunxi_machine_exit(void)
{
	platform_driver_unregister(&sunxi_machine_driver);
}

module_exit(sunxi_machine_exit);

MODULE_AUTHOR("huangxin,liushaohua");
MODULE_DESCRIPTION("SUNXI_sndpcm ALSA SoC audio driver");
MODULE_LICENSE("GPL");
