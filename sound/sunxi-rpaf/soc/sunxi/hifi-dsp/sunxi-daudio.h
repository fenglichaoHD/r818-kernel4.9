/*
 * sound\soc\sunxi\hifi-dsp\sunxi-daudio.h
 *
 * (C) Copyright 2019-2025
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * yumingfeng <yumingfeng@allwinertech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#ifndef	__SUNXI_HIFI_DAUDIO_H_
#define	__SUNXI_HIFI_DAUDIO_H_

#include "sunxi-hifi-pcm.h"

/*
 * Platform    I2S_count      ARM	DSP
 * sun50iw11	  5	      0-2	3-4
 */

/* DAUDIO register definition */
#define	SUNXI_DAUDIO_CTL		0x00
#define	SUNXI_DAUDIO_FMT0		0x04
#define	SUNXI_DAUDIO_FMT1		0x08
#define	SUNXI_DAUDIO_INTSTA		0x0C
#define	SUNXI_DAUDIO_RXFIFO		0x10
#define	SUNXI_DAUDIO_FIFOCTL		0x14
#define	SUNXI_DAUDIO_FIFOSTA		0x18
#define	SUNXI_DAUDIO_INTCTL		0x1C
#define	SUNXI_DAUDIO_TXFIFO		0x20
#define	SUNXI_DAUDIO_CLKDIV		0x24
#define	SUNXI_DAUDIO_TXCNT		0x28
#define	SUNXI_DAUDIO_RXCNT		0x2C
#define	SUNXI_DAUDIO_CHCFG		0x30
#define	SUNXI_DAUDIO_TX0CHSEL		0x34
#define	SUNXI_DAUDIO_TX1CHSEL		0x38
#define	SUNXI_DAUDIO_TX2CHSEL		0x3C
#define	SUNXI_DAUDIO_TX3CHSEL		0x40

#define	SUNXI_DAUDIO_TX0CHMAP0		0x44
#define	SUNXI_DAUDIO_TX0CHMAP1		0x48
#define	SUNXI_DAUDIO_TX1CHMAP0		0x4C
#define	SUNXI_DAUDIO_TX1CHMAP1		0x50
#define	SUNXI_DAUDIO_TX2CHMAP0		0x54
#define	SUNXI_DAUDIO_TX2CHMAP1		0x58
#define	SUNXI_DAUDIO_TX3CHMAP0		0x5C
#define	SUNXI_DAUDIO_TX3CHMAP1		0x60

#define	SUNXI_DAUDIO_RXCHSEL		0x64
#define	SUNXI_DAUDIO_RXCHMAP0		0x68
#define	SUNXI_DAUDIO_RXCHMAP1		0x6C
#define	SUNXI_DAUDIO_RXCHMAP2		0x70
#define	SUNXI_DAUDIO_RXCHMAP3		0x74

#define	SUNXI_DAUDIO_DEBUG		0x78
#define	SUNXI_DAUDIO_REV		0x7C

#define SUNXI_DAUDIO_ASRC_MCLKCFG	0x80
#define SUNXI_DAUDIO_ASRC_FSOUTCFG	0x84
#define SUNXI_DAUDIO_ASRC_FSIN_EXTCFG	0x88
#define SUNXI_DAUDIO_ASRC_ASRCEN	0x8C
#define SUNXI_DAUDIO_ASRC_MANCFG	0x90
#define SUNXI_DAUDIO_ASRC_RATIOSTAT	0x94
#define SUNXI_DAUDIO_ASRC_FIFOSTAT	0x98
#define SUNXI_DAUDIO_ASRC_MBISTCFG	0x9C
#define SUNXI_DAUDIO_ASRC_MBISTSTA	0xA0

/* about platform define */
#ifdef CONFIG_ARCH_SUN50IW11
#define SUNXI_DAUDIO_ASRC_EN
#define SUNXI_DAUDIO_REG_MAX SUNXI_DAUDIO_ASRC_MBISTSTA
#else
#define SUNXI_DAUDIO_REG_MAX SUNXI_DAUDIO_REV
#undef SUNXI_DAUDIO_ASRC_EN
#endif

/* SUNXI_DAUDIO_CTL:0x00 */
#define RX_SYNC_EN			21
#define RX_EN_MUX			20
#define	BCLK_OUT			18
#define	LRCK_OUT			17
#define	LRCKR_CTL			16
#define	SDO3_EN				11
#define	SDO2_EN				10
#define	SDO1_EN				9
#define	SDO0_EN				8
#define DAUDIO_MAD_DATA_EN		7
#define	MUTE_CTL			6
#define	MODE_SEL			4
#define	LOOP_EN				3
#define	CTL_TXEN			2
#define	CTL_RXEN			1
#define	GLOBAL_EN			0

/* SUNXI_DAUDIO_FMT0:0x04 */
#define	SDI_SYNC_SEL			31
#define	LRCK_WIDTH			30
#define	LRCKR_PERIOD			20
#define	LRCK_POLARITY			19
#define	LRCK_PERIOD			8
#define	BRCK_POLARITY			7
#define	DAUDIO_SAMPLE_RESOLUTION	4
#define	EDGE_TRANSFER			3
#define	SLOT_WIDTH			0

/* SUNXI_DAUDIO_FMT1:0x08 */
#define	RX_MLS				7
#define	TX_MLS				6
#define	SEXT				4
#define	RX_PDM				2
#define	TX_PDM				0

/* SUNXI_DAUDIO_INTSTA:0x0C */
#define	TXU_INT				6
#define	TXO_INT				5
#define	TXE_INT				4
#define	RXU_INT				2
#define RXO_INT				1
#define	RXA_INT				0

/* SUNXI_DAUDIO_FIFOCTL:0x14 */
#define	HUB_EN				31
#define	FIFO_CTL_FTX			25
#define	FIFO_CTL_FRX			24
#define	TXTL				12
#define	RXTL				4
#define	TXIM				2
#define	RXOM				0

/* SUNXI_DAUDIO_FIFOSTA:0x18 */
#define	FIFO_TXE			28
#define	FIFO_TX_CNT			16
#define	FIFO_RXA			8
#define DAUDIO_MAD_DATA_ALIGN		7
#define	FIFO_RX_CNT			0

/* SUNXI_DAUDIO_INTCTL:0x1C */
#define	TXDRQEN				7
#define	TXUI_EN				6
#define	TXOI_EN				5
#define	TXEI_EN				4
#define	RXDRQEN				3
#define	RXUIEN				2
#define	RXOIEN				1
#define	RXAIEN				0

/* SUNXI_DAUDIO_CLKDIV:0x24 */
#define	MCLKOUT_EN			8
#define	BCLK_DIV			4
#define	MCLK_DIV			0

/* SUNXI_DAUDIO_CHCFG:0x30 */
#define	TX_SLOT_HIZ			9
#define	TX_STATE			8
#define	RX_SLOT_NUM			4
#define	TX_SLOT_NUM			0

/* SUNXI_DAUDIO_TXnCHSEL:0X34+n*0x04 */
#define	TX_OFFSET			20
#define	TX_CHSEL			16
#define	TX_CHEN				0

/* SUNXI_DAUDIO_RXCHSEL */
#define	RX_OFFSET			20
#define	RX_CHSEL			16

/* CHMAP default setting */
#define SUNXI_DAUDIO_TXCH_NUM		0x4
#define	SUNXI_DEFAULT_TXCHMAP0		0xFEDCBA98
#define	SUNXI_DEFAULT_TXCHMAP1		0x76543210

/* RXCHMAP default setting */
#define SUNXI_DAUDIO_RXCH_NUM		0x4
#define	SUNXI_DEFAULT_RXCHMAP		0x76543210

#define	SUNXI_DEFAULT_RXCHMAP3		0x03020100
#define	SUNXI_DEFAULT_RXCHMAP2		0x07060504
#define	SUNXI_DEFAULT_RXCHMAP1		0x0B0A0908
#define	SUNXI_DEFAULT_RXCHMAP0		0x0F0E0D0C

/* Shift & Mask define */

/* SUNXI_DAUDIO_CTL:0x00 */
#define	SUNXI_DAUDIO_MODE_CTL_MASK		3
#define	SUNXI_DAUDIO_MODE_CTL_PCM		0
#define	SUNXI_DAUDIO_MODE_CTL_I2S		1
#define	SUNXI_DAUDIO_MODE_CTL_LEFT		1
#define	SUNXI_DAUDIO_MODE_CTL_RIGHT		2
#define	SUNXI_DAUDIO_MODE_CTL_REVD		3
/* combine LRCK_CLK & BCLK setting */
#define	SUNXI_DAUDIO_LRCK_OUT_MASK		3
#define	SUNXI_DAUDIO_LRCK_OUT_DISABLE		0
#define	SUNXI_DAUDIO_LRCK_OUT_ENABLE		3

/* SUNXI_DAUDIO_FMT0 */
#define	SUNXI_DAUDIO_LRCK_PERIOD_MASK		0x3FF
#define	SUNXI_DAUDIO_SLOT_WIDTH_MASK		7
/* Left Blank */
#define	SUNXI_DAUDIO_SR_MASK			7
#define	SUNXI_DAUDIO_SR_16BIT			3
#define	SUNXI_DAUDIO_SR_24BIT			5
#define	SUNXI_DAUDIO_SR_32BIT			7

#define	SUNXI_DAUDIO_LRCK_POLARITY_NOR		0
#define	SUNXI_DAUDIO_LRCK_POLARITY_INV		1
#define	SUNXI_DAUDIO_BCLK_POLARITY_NOR		0
#define	SUNXI_DAUDIO_BCLK_POLARITY_INV		1

/* SUNXI_DAUDIO_FMT1 */
#define	SUNXI_DAUDIO_FMT1_DEF			0x30

/* SUNXI_DAUDIO_FIFOCTL */
#define	SUNXI_DAUDIO_TXIM_MASK			1
#define	SUNXI_DAUDIO_TXIM_VALID_MSB		0
#define	SUNXI_DAUDIO_TXIM_VALID_LSB		1
/* Left Blank */
#define	SUNXI_DAUDIO_RXOM_MASK			3
/* Expanding 0 at LSB of RX_FIFO */
#define	SUNXI_DAUDIO_RXOM_EXP0			0
/* Expanding sample bit at MSB of RX_FIFO */
#define	SUNXI_DAUDIO_RXOM_EXPH			1
/* Fill RX_FIFO low word be 0 */
#define	SUNXI_DAUDIO_RXOM_TUNL			2
/* Fill RX_FIFO high word be higher sample bit */
#define	SUNXI_DAUDIO_RXOM_TUNH			3

/* SUNXI_DAUDIO_CLKDIV */
#define	SUNXI_DAUDIO_BCLK_DIV_MASK		0xF
#define	SUNXI_DAUDIO_BCLK_DIV_1			1
#define	SUNXI_DAUDIO_BCLK_DIV_2			2
#define	SUNXI_DAUDIO_BCLK_DIV_3			3
#define	SUNXI_DAUDIO_BCLK_DIV_4			4
#define	SUNXI_DAUDIO_BCLK_DIV_5			5
#define	SUNXI_DAUDIO_BCLK_DIV_6			6
#define	SUNXI_DAUDIO_BCLK_DIV_7			7
#define	SUNXI_DAUDIO_BCLK_DIV_8			8
#define	SUNXI_DAUDIO_BCLK_DIV_9			9
#define	SUNXI_DAUDIO_BCLK_DIV_10		10
#define	SUNXI_DAUDIO_BCLK_DIV_11		11
#define	SUNXI_DAUDIO_BCLK_DIV_12		12
#define	SUNXI_DAUDIO_BCLK_DIV_13		13
#define	SUNXI_DAUDIO_BCLK_DIV_14		14
#define	SUNXI_DAUDIO_BCLK_DIV_15		15
/* Left Blank */
#define	SUNXI_DAUDIO_MCLK_DIV_MASK		0xF
#define	SUNXI_DAUDIO_MCLK_DIV_1			1
#define	SUNXI_DAUDIO_MCLK_DIV_2			2
#define	SUNXI_DAUDIO_MCLK_DIV_3			3
#define	SUNXI_DAUDIO_MCLK_DIV_4			4
#define	SUNXI_DAUDIO_MCLK_DIV_5			5
#define	SUNXI_DAUDIO_MCLK_DIV_6			6
#define	SUNXI_DAUDIO_MCLK_DIV_7			7
#define	SUNXI_DAUDIO_MCLK_DIV_8			8
#define	SUNXI_DAUDIO_MCLK_DIV_9			9
#define	SUNXI_DAUDIO_MCLK_DIV_10		10
#define	SUNXI_DAUDIO_MCLK_DIV_11		11
#define	SUNXI_DAUDIO_MCLK_DIV_12		12
#define	SUNXI_DAUDIO_MCLK_DIV_13		13
#define	SUNXI_DAUDIO_MCLK_DIV_14		14
#define	SUNXI_DAUDIO_MCLK_DIV_15		15

/* SUNXI_DAUDIO_CHCFG */
#define	SUNXI_DAUDIO_TX_SLOT_MASK		0XF
#define	SUNXI_DAUDIO_RX_SLOT_MASK		0XF
/* SUNXI_DAUDIO_TX0CHSEL: */
#define	SUNXI_DAUDIO_TX_OFFSET_MASK		3
#define	SUNXI_DAUDIO_TX_OFFSET_0		0
#define	SUNXI_DAUDIO_TX_OFFSET_1		1
/* Left Blank */
#define	SUNXI_DAUDIO_TX_CHEN_MASK		0xFFFF
#define	SUNXI_DAUDIO_TX_CHSEL_MASK		0xF

/* SUNXI_DAUDIO_RXCHSEL */
#define SUNXI_DAUDIO_RX_OFFSET_MASK		3
#define	SUNXI_DAUDIO_RX_CHSEL_MASK		0XF

#define DAUDIO_RXCH_DEF_MAP(x) (x << ((x%4)<<3))
#define DAUDIO_RXCHMAP(x) (0x1f << ((x%4)<<3))

/* SUNXI_DAUDIO_ASRC_MCLKCFG:0x80 */
#define DAUDIO_ASRC_MCLK_GATE		16
#define DAUDIO_ASRC_MCLK_RATIO		0

/* SUNXI_DAUDIO_ASRC_FSOUTCFG:0x84 */
#define DAUDIO_ASRC_FSOUT_GATE		20
#define DAUDIO_ASRC_FSOUT_CLKSRC	16
#define DAUDIO_ASRC_FSOUT_CLKDIV1	4
#define DAUDIO_ASRC_FSOUT_CLKDIV2	0

/* SUNXI_DAUDIO_ASRC_FSIN_EXTCFG:0x88 */
#define DAUDIO_ASRC_FSIN_EXTEN		16
#define DAUDIO_ASRC_FSIN_EXTCYCLE	0

/* SUNXI_DAUDIO_ASRC_ASRCEN:0x8C */
#define DAUDIO_ASRC_ASRCEN		0

/* SUNXI_DAUDIO_ASRC_MANCFG:0x90 */
#define DAUDIO_ASRC_MANRATIOEN		31
#define DAUDIO_ASRC_MAN_RATIO		0

/* SUNXI_DAUDIO_ASRC_RATIOSTAT:0x94 */
/* SUNXI_DAUDIO_ASRC_FIFOSTAT:0x98 */
/* SUNXI_DAUDIO_ASRC_MBISTCFG:0x9C */
/* SUNXI_DAUDIO_ASRC_MBISTSTA:0xA0 */

#define DAUDIO_NUM_MAX			2

#define	SND_SOC_DAIFMT_SIG_SHIFT		8
#define	SND_SOC_DAIFMT_MASTER_SHIFT		12

/* to clear FIFO */
#define SUNXI_DAUDIO_FTX_TIMES		10

#define DAUDIO_RX_FIFO_SIZE 64
#define DAUDIO_TX_FIFO_SIZE 128

struct daudio_label {
	unsigned long address;
	int value;
};

struct daudio_reg_label {
	const char *name;
	const unsigned int address;
	int value;
};

#define DAUDIO_REG_LABEL(constant) {#constant, constant, 0}
#define DAUDIO_REG_LABEL_END {NULL, -1, 0}

struct sunxi_daudio_mem_info {
	struct resource res;
	void __iomem *membase;
	struct resource *memregion;
	struct regmap *regmap;
};

struct sunxi_daudio_platform_data {
	unsigned int daudio_type;

	unsigned int pcm_lrck_period;
	unsigned int msb_lsb_first:1;
	unsigned int sign_extend:2;
	unsigned int tx_data_mode:2;
	unsigned int rx_data_mode:2;
	unsigned int slot_width_select;
	unsigned int frame_type;
	unsigned int tdm_config;
	unsigned int tdm_num;
	unsigned int mclk_div;
	unsigned int tx_num;
	unsigned int tx_chmap0;
	unsigned int tx_chmap1;
	unsigned int rx_num;
	unsigned int rx_chmap0;
	unsigned int rx_chmap1;
	unsigned int rx_chmap2;
	unsigned int rx_chmap3;

	/* eg:0 snddaudio0, 1 snddaudio1 */
	unsigned int dsp_daudio;
	/* eg:0 sndcodec; 1 snddmic; 2 snddaudio0; */
	unsigned int dsp_card;
	/* default is 0, for reserved */
	unsigned int dsp_device;
};

struct daudio_voltage_supply {
	struct regulator *daudio_regulator;
	const char *regulator_name;
};

struct sunxi_daudio_dts_info {
	struct sunxi_daudio_mem_info mem_info;
	struct sunxi_daudio_platform_data pdata_info;
	struct daudio_voltage_supply vol_supply;

	/* value must be (2^n)Kbyte */
	size_t playback_cma;
	size_t capture_cma;
};

struct sunxi_daudio_info {
	struct device *dev;
	struct sunxi_daudio_dts_info dts_info;
	struct sunxi_dma_params playback_dma_param;
	struct sunxi_dma_params capture_dma_param;
	struct snd_soc_dai *cpu_dai;
	struct daudio_label *reg_label;

	/* for hifi */
	unsigned int capturing;
	unsigned int playing;
	char wq_capture_name[32];
	struct mutex rpmsg_mutex_capture;
	struct workqueue_struct *wq_capture;
	struct work_struct trigger_work_capture;
	char wq_playback_name[32];
	struct mutex rpmsg_mutex_playback;
	struct workqueue_struct *wq_playback;
	struct work_struct trigger_work_playback;

	struct msg_substream_package msg_playback;
	struct msg_substream_package msg_capture;
	struct msg_mixer_package msg_mixer;
	struct msg_debug_package msg_debug;

	struct snd_dsp_component dsp_playcomp;
	struct snd_dsp_component dsp_capcomp;
};

#endif	/* __SUNXI_DAUDIO_H_ */
