// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek ALSA SoC Audio DAI ADDA Control
 *
 *  Copyright (c) 2023 MediaTek Inc.
 *  Author: Yen-cheng Wang <Yen-cheng.Wang@mediatek.com>
 */

#include <linux/regmap.h>
#include <linux/delay.h>
#include "mt6899-afe-clk.h"
#include "mt6899-afe-common.h"
#include "mt6899-afe-gpio.h"
#include "mt6899-interconnection.h"

//#define MTKAIF4 //for  mt6338
/* mt6363 vs1 voter */
#define RG_BUCK_VS1_VOTER_EN_LO                 0x189a
#define RG_BUCK_VS1_VOTER_EN_LO_SET             0x189b
#define RG_BUCK_VS1_VOTER_EN_LO_CLR             0x189c

#define VS1_MT6338_MSK                    (0x1 << 0)

enum {
	UL_IIR_SW = 0,
	UL_IIR_5HZ,
	UL_IIR_10HZ,
	UL_IIR_25HZ,
	UL_IIR_50HZ,
	UL_IIR_75HZ,
};

enum {
	AUDIO_SDM_LEVEL_MUTE = 0,
	AUDIO_SDM_LEVEL_NORMAL = 0x1d,
	/* if you change level normal */
	/* you need to change formula of hp impedance and dc trim too */
};

enum {
	AUDIO_SDM_2ND = 0,
	AUDIO_SDM_3RD,
};

enum {
	DELAY_DATA_MISO1 = 0,
	DELAY_DATA_MISO2,
};

enum {
	MTK_AFE_ADDA_DL_RATE_8K = 0,
	MTK_AFE_ADDA_DL_RATE_11K = 1,
	MTK_AFE_ADDA_DL_RATE_12K = 2,
	MTK_AFE_ADDA_DL_RATE_16K = 4,
	MTK_AFE_ADDA_DL_RATE_22K = 5,
	MTK_AFE_ADDA_DL_RATE_24K = 6,
	MTK_AFE_ADDA_DL_RATE_32K = 8,
	MTK_AFE_ADDA_DL_RATE_44K = 9,
	MTK_AFE_ADDA_DL_RATE_48K = 10,
	MTK_AFE_ADDA_DL_RATE_88K = 13,
	MTK_AFE_ADDA_DL_RATE_96K = 14,
	MTK_AFE_ADDA_DL_RATE_176K = 17,
	MTK_AFE_ADDA_DL_RATE_192K = 18,
	MTK_AFE_ADDA_DL_RATE_352K = 21,
	MTK_AFE_ADDA_DL_RATE_384K = 22,
};

enum {
	MTK_AFE_ADDA_UL_RATE_8K = 0,
	MTK_AFE_ADDA_UL_RATE_16K = 1,
	MTK_AFE_ADDA_UL_RATE_32K = 2,
	MTK_AFE_ADDA_UL_RATE_48K = 3,
	MTK_AFE_ADDA_UL_RATE_96K = 4,
	MTK_AFE_ADDA_UL_RATE_192K = 5,
	MTK_AFE_ADDA_UL_RATE_48K_HD = 6,
};

#ifdef MTKAIF4
enum {
	MTK_AFE_MTKAIF_RATE_8K = 0x0,
	MTK_AFE_MTKAIF_RATE_12K = 0x1,
	MTK_AFE_MTKAIF_RATE_16K = 0x2,
	MTK_AFE_MTKAIF_RATE_24K = 0x3,
	MTK_AFE_MTKAIF_RATE_32K = 0x4,
	MTK_AFE_MTKAIF_RATE_48K = 0x5,
	MTK_AFE_MTKAIF_RATE_64K = 0x6,
	MTK_AFE_MTKAIF_RATE_96K = 0x7,
	MTK_AFE_MTKAIF_RATE_128K = 0x8,
	MTK_AFE_MTKAIF_RATE_192K = 0x9,
	MTK_AFE_MTKAIF_RATE_256K = 0xa,
	MTK_AFE_MTKAIF_RATE_384K = 0xb,
	MTK_AFE_MTKAIF_RATE_11K = 0x10,
	MTK_AFE_MTKAIF_RATE_22K = 0x11,
	MTK_AFE_MTKAIF_RATE_44K = 0x12,
	MTK_AFE_MTKAIF_RATE_88K = 0x13,
	MTK_AFE_MTKAIF_RATE_176K = 0x14,
	MTK_AFE_MTKAIF_RATE_352K = 0x15,
};
#endif

#define SDM_AUTO_RESET_THRESHOLD 0x190000

#if !defined(IS_FPGA_EARLY_PORTING)
static struct mtk_afe_adda_priv *get_adda_priv_by_name(struct mtk_base_afe *afe,
		const char *name)
{
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	int dai_id;

	if (strncmp(name, "aud_dl0_dac_hires_clk", 21) == 0 ||
	    strncmp(name, "aud_ul0_adc_hires_clk", 21) == 0)
		dai_id = MT6899_DAI_ADDA;
	else if (strncmp(name, "aud_dl1_dac_hires_clk", 21) == 0 ||
		 strncmp(name, "aud_ul1_adc_hires_clk", 21) == 0)
		dai_id = MT6899_DAI_ADDA_CH34;
	else
		return NULL;

	return afe_priv->dai_priv[dai_id];
}
#endif

static unsigned int adda_dl_rate_transform(struct mtk_base_afe *afe,
		unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_ADDA_DL_RATE_8K;
	case 11025:
		return MTK_AFE_ADDA_DL_RATE_11K;
	case 12000:
		return MTK_AFE_ADDA_DL_RATE_12K;
	case 16000:
		return MTK_AFE_ADDA_DL_RATE_16K;
	case 22050:
		return MTK_AFE_ADDA_DL_RATE_22K;
	case 24000:
		return MTK_AFE_ADDA_DL_RATE_24K;
	case 32000:
		return MTK_AFE_ADDA_DL_RATE_32K;
	case 44100:
		return MTK_AFE_ADDA_DL_RATE_44K;
	case 48000:
		return MTK_AFE_ADDA_DL_RATE_48K;
	case 96000:
		return MTK_AFE_ADDA_DL_RATE_96K;
	case 192000:
		return MTK_AFE_ADDA_DL_RATE_192K;
	default:
		dev_info(afe->dev, "%s(), rate %d invalid, use 48kHz!!!\n",
			 __func__, rate);
		return MTK_AFE_ADDA_DL_RATE_48K;
	}
}

static unsigned int adda_ul_rate_transform(struct mtk_base_afe *afe,
		unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_ADDA_UL_RATE_8K;
	case 16000:
		return MTK_AFE_ADDA_UL_RATE_16K;
	case 32000:
		return MTK_AFE_ADDA_UL_RATE_32K;
	case 48000:
		return MTK_AFE_ADDA_UL_RATE_48K;
	case 96000:
		return MTK_AFE_ADDA_UL_RATE_96K;
	case 192000:
		return MTK_AFE_ADDA_UL_RATE_192K;
	default:
		dev_info(afe->dev, "%s(), rate %d invalid, use 48kHz!!!\n",
			 __func__, rate);
		return MTK_AFE_ADDA_UL_RATE_48K;
	}
}

#ifdef MTKAIF4
static unsigned int mtkaif_rate_transform(struct mtk_base_afe *afe,
					   unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_MTKAIF_RATE_8K;
	case 11025:
		return MTK_AFE_MTKAIF_RATE_11K;
	case 12000:
		return MTK_AFE_MTKAIF_RATE_12K;
	case 16000:
		return MTK_AFE_MTKAIF_RATE_16K;
	case 22050:
		return MTK_AFE_MTKAIF_RATE_22K;
	case 24000:
		return MTK_AFE_MTKAIF_RATE_24K;
	case 32000:
		return MTK_AFE_MTKAIF_RATE_32K;
	case 44100:
		return MTK_AFE_MTKAIF_RATE_44K;
	case 48000:
		return MTK_AFE_MTKAIF_RATE_48K;
	case 96000:
		return MTK_AFE_MTKAIF_RATE_96K;
	case 192000:
		return MTK_AFE_MTKAIF_RATE_192K;
	default:
		dev_info(afe->dev, "%s(), rate %d invalid, use 48kHz!!!\n",
			 __func__, rate);
		return MTK_AFE_MTKAIF_RATE_48K;
	}
}
#endif

/* dai component */
static const struct snd_kcontrol_new mtk_adda_dl_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN014_1, I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN014_1, I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN014_1, I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN014_1, I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN014_1, I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN014_1, I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN014_1, I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN014_1, I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH1", AFE_CONN014_1, I_DL8_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN014_1, I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN014_2, I_DL24_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN014_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN014_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN014_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH1", AFE_CONN014_0,
				    I_GAIN0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN014_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN014_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN014_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH1", AFE_CONN014_6,
				    I_SRC_1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN014_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_adda_dl_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN015_1, I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN015_1, I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN015_1, I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN015_1, I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN015_1, I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN015_1, I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN015_1, I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN015_1, I_DL6_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN015_1, I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH2", AFE_CONN015_1, I_DL8_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN015_1, I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN015_2, I_DL24_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN015_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN015_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN015_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH2", AFE_CONN015_0,
				    I_GAIN0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN015_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN015_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN015_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN015_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN015_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH2", AFE_CONN015_6,
				    I_SRC_1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN015_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_adda_dl_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH1", AFE_CONN016_1, I_DL0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN016_1, I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN016_1, I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN016_1, I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH1", AFE_CONN016_1, I_DL4_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH1", AFE_CONN016_1, I_DL5_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH1", AFE_CONN016_1, I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH1", AFE_CONN016_1, I_DL7_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH1", AFE_CONN016_1, I_DL8_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH1", AFE_CONN016_1, I_DL_24CH_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH3", AFE_CONN016_1, I_DL_24CH_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH1", AFE_CONN016_2, I_DL24_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN016_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN016_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN016_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH1", AFE_CONN016_0,
				    I_GAIN0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN016_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN016_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH1", AFE_CONN016_6,
				    I_SRC_0_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH1", AFE_CONN016_6,
				    I_SRC_1_OUT_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH1", AFE_CONN016_6,
				    I_SRC_2_OUT_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_adda_dl_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DL0_CH2", AFE_CONN017_1, I_DL0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN017_1, I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN017_1, I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN017_1, I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL4_CH2", AFE_CONN017_1, I_DL4_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL5_CH2", AFE_CONN017_1, I_DL5_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL6_CH2", AFE_CONN017_1, I_DL6_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL7_CH2", AFE_CONN017_1, I_DL7_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL8_CH2", AFE_CONN017_1, I_DL8_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH2", AFE_CONN017_1, I_DL_24CH_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL_24CH_CH4", AFE_CONN017_1, I_DL_24CH_CH4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL24_CH2", AFE_CONN017_2, I_DL24_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH3", AFE_CONN017_0,
				    I_ADDA_UL_CH3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN017_0,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN017_0,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_GAIN0_OUT_CH2", AFE_CONN017_0,
				    I_GAIN0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH1", AFE_CONN017_4,
				    I_PCM_0_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH1", AFE_CONN017_4,
				    I_PCM_1_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_0_CAP_CH2", AFE_CONN017_4,
				    I_PCM_0_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_1_CAP_CH2", AFE_CONN017_4,
				    I_PCM_1_CAP_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_0_OUT_CH2", AFE_CONN017_6,
				    I_SRC_0_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_1_OUT_CH2", AFE_CONN017_6,
				    I_SRC_1_OUT_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("HW_SRC_2_OUT_CH2", AFE_CONN017_6,
				    I_SRC_2_OUT_CH2, 1, 0),
};

static const struct snd_kcontrol_new mtk_stf_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN012_0,
				    I_ADDA_UL_CH1, 1, 0),
};

// Luke: dummy BE for codec fail
static const char * const adda_mux_map[] = {
	"Normal", "Dummy_Widget",
};
static int adda_mux_map_value[] = {
	0, 1,
};
static SOC_VALUE_ENUM_SINGLE_AUTODISABLE_DECL(adda_mux_map_enum,
					      SND_SOC_NOPM,
					      0,
					      1,
					      adda_mux_map,
					      adda_mux_map_value);
static const struct snd_kcontrol_new adda_out_mux_control =
	SOC_DAPM_ENUM("ADDA Out Select", adda_mux_map_enum);
static const struct snd_kcontrol_new adda_in_mux_control =
	SOC_DAPM_ENUM("ADDA In Select", adda_mux_map_enum);
// Luke: dummy BE for codec fail


enum {
	SUPPLY_SEQ_ADDA_AFE_ON,
	SUPPLY_SEQ_ADDA_DL_ON,
	SUPPLY_SEQ_ADDA_AUD_PAD_TOP,
	SUPPLY_SEQ_ADDA_MTKAIF_CFG,
	SUPPLY_SEQ_ADDA6_MTKAIF_CFG,
	SUPPLY_SEQ_ADDA_FIFO,
	SUPPLY_SEQ_ADDA_AP_DMIC,
	SUPPLY_SEQ_ADDA_UL_ON,
};

static int mtk_adda_ul_src_dmic(struct mtk_base_afe *afe, int id)
{
	unsigned int reg_con0 = 0, reg_con1 = 0;

	switch (id) {
	case MT6899_DAI_ADDA:
	case MT6899_DAI_AP_DMIC:
		reg_con0 = AFE_ADDA_UL0_SRC_CON0;
		reg_con1 = AFE_ADDA_UL0_SRC_CON1;
		break;
	case MT6899_DAI_ADDA_CH34:
	case MT6899_DAI_AP_DMIC_CH34:
		reg_con0 = AFE_ADDA_UL1_SRC_CON0;
		reg_con1 = AFE_ADDA_UL1_SRC_CON1;
		break;
	default:
		return -EINVAL;
	}

	/* choose Phase */
	regmap_update_bits(afe->regmap, reg_con0,
			   UL_DMIC_PHASE_SEL_CH1_MASK_SFT,
			   0x0 << UL_DMIC_PHASE_SEL_CH1_SFT);
	regmap_update_bits(afe->regmap, reg_con0,
			   UL_DMIC_PHASE_SEL_CH2_MASK_SFT,
			   0x4 << UL_DMIC_PHASE_SEL_CH2_SFT);

	/* dmic mode, 3.25M*/
	regmap_update_bits(afe->regmap, reg_con0,
			   DIGMIC_3P25M_1P625M_SEL_CTL_MASK_SFT,
			   0x0);
	regmap_update_bits(afe->regmap, reg_con0,
			   DMIC_LOW_POWER_MODE_CTL_MASK_SFT,
			   0x0);

	/* turn on dmic, ch1, ch2 */
	regmap_update_bits(afe->regmap, reg_con0,
			   UL_SDM_3_LEVEL_CTL_MASK_SFT,
			   0x1 << UL_SDM_3_LEVEL_CTL_SFT);
	regmap_update_bits(afe->regmap, reg_con0,
			   UL_MODE_3P25M_CH1_CTL_MASK_SFT,
			   0x1 << UL_MODE_3P25M_CH1_CTL_SFT);
	regmap_update_bits(afe->regmap, reg_con0,
			   UL_MODE_3P25M_CH2_CTL_MASK_SFT,
			   0x1 << UL_MODE_3P25M_CH2_CTL_SFT);

	/* ul gain:  gain = 0x7fff/positive_gain = 0x0/gain_mode = 0x10 */
	regmap_update_bits(afe->regmap, reg_con1,
			   ADDA_UL_GAIN_VALUE_MASK_SFT,
			   0x7fff << ADDA_UL_GAIN_VALUE_SFT);
	regmap_update_bits(afe->regmap, reg_con1,
			   ADDA_UL_POSTIVEGAIN_MASK_SFT,
			   0x0 << ADDA_UL_POSTIVEGAIN_SFT);
	/* gain_mode = 0x10: Add 0.5 gain at CIC output */
	regmap_update_bits(afe->regmap, reg_con1,
			   GAIN_MODE_MASK_SFT,
			   0x10 << GAIN_MODE_SFT);
	return 0;
}

static int mtk_adda_ul_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol,
			     int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	int mtkaif_dmic = afe_priv->mtkaif_dmic;

	dev_info(afe->dev, "%s(), name %s, event 0x%x, mtkaif_dmic %d\n",
		 __func__, w->name, event, mtkaif_dmic);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6899_afe_gpio_request(afe, true, MT6899_DAI_ADDA, 1);

		/* update setting to dmic */
		if (mtkaif_dmic) {
			/* mtkaif_rxif_data_mode = 1, dmic */
			regmap_update_bits(afe->regmap, AFE_MTKAIF0_RX_CFG0,
					   0x1, 0x1);

			/* dmic mode, 3.25M*/
			regmap_update_bits(afe->regmap, AFE_MTKAIF0_RX_CFG0,
					   RG_MTKAIF0_RXIF_VOICE_MODE_MASK_SFT,
					   0x0);
			mtk_adda_ul_src_dmic(afe, MT6899_DAI_ADDA);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* should delayed 1/fs(smallest is 8k) = 125us before afe off */
		udelay(125);
		mt6899_afe_gpio_request(afe, false, MT6899_DAI_ADDA, 1);

		/* reset dmic */
		afe_priv->mtkaif_dmic = 0;
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_adda_ch34_ul_event(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol,
				  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	int mtkaif_dmic = afe_priv->mtkaif_dmic_ch34;
	int mtkaif_adda6_only = afe_priv->mtkaif_adda6_only;

	dev_info(afe->dev,
		 "%s(), name %s, event 0x%x, mtkaif_dmic %d, mtkaif_adda6_only %d\n",
		 __func__, w->name, event, mtkaif_dmic, mtkaif_adda6_only);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6899_afe_gpio_request(afe, true, MT6899_DAI_ADDA_CH34, 1);

		/* update setting to dmic */
		if (mtkaif_dmic) {
			/* mtkaif_rxif_data_mode = 1, dmic */
			regmap_update_bits(afe->regmap,
					   AFE_MTKAIF1_RX_CFG0,
					   0x1, 0x1);

			/* dmic mode, 3.25M*/
			regmap_update_bits(afe->regmap,
					   AFE_MTKAIF1_RX_CFG0,
					   RG_MTKAIF1_RXIF_VOICE_MODE_MASK_SFT,
					   0x0);
			mtk_adda_ul_src_dmic(afe, MT6899_DAI_ADDA_CH34);
		}

		/* when using adda6 without adda enabled,
		 * RG_ADDA6_MTKAIF_RX_SYNC_WORD2_DISABLE_SFT need to be set or
		 * data cannot be received.
		 */
		if (mtkaif_adda6_only) {
			regmap_update_bits(afe->regmap,
					   AFE_MTKAIF1_RX_CFG2,
					   RG_MTKAIF1_RXIF_SYNC_WORD1_DISABLE_MASK_SFT,
					   0x1);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* should delayed 1/fs(smallest is 8k) = 125us before afe off */
		udelay(125);
		mt6899_afe_gpio_request(afe, false, MT6899_DAI_ADDA_CH34, 1);

		/* reset dmic */
		afe_priv->mtkaif_dmic_ch34 = 0;

		if (mtkaif_adda6_only) {
			regmap_update_bits(afe->regmap,
					   AFE_MTKAIF1_RX_CFG2,
					   RG_MTKAIF1_RXIF_SYNC_WORD1_DISABLE_MASK_SFT,
					   0x0);
		}
		break;
	default:
		break;
	}

	return 0;
}
static int mtk_adda_ul_ap_dmic_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol,
			     int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	dev_info(afe->dev, "%s(), name %s, event 0x%x\n",
		 __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6899_afe_gpio_request(afe, true, MT6899_DAI_ADDA, 1);
		//mtk_adda_ul_src_dmic(afe, MT6899_DAI_AP_DMIC);
		mt6899_afe_gpio_request(afe, true, MT6899_DAI_AP_DMIC, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* should delayed 1/fs(smallest is 8k) = 125us before afe off */
		udelay(125);
		mt6899_afe_gpio_request(afe, false, MT6899_DAI_ADDA, 1);
		mt6899_afe_gpio_request(afe, false, MT6899_DAI_AP_DMIC, 1);

		break;
	default:
		break;
	}

	return 0;
}


static int mtk_adda_ch34_ul_ap_dmic_event(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol,
				  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	int mtkaif_adda6_only = afe_priv->mtkaif_adda6_only;

	dev_info(afe->dev,
		 "%s(), name %s, event 0x%x, mtkaif_adda6_only %d\n",
		 __func__, w->name, event, mtkaif_adda6_only);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6899_afe_gpio_request(afe, true, MT6899_DAI_ADDA_CH34, 1);
		mt6899_afe_gpio_request(afe, true, MT6899_DAI_AP_DMIC_CH34, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* should delayed 1/fs(smallest is 8k) = 125us before afe off */
		udelay(125);
		mt6899_afe_gpio_request(afe, false, MT6899_DAI_ADDA_CH34, 1);
		mt6899_afe_gpio_request(afe, false, MT6899_DAI_AP_DMIC_CH34, 1);
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_adda_pad_top_event(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol,
				  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (afe_priv->mtkaif_protocol == MTKAIF_PROTOCOL_2_CLK_P2)
			regmap_write(afe->regmap, AFE_AUD_PAD_TOP_CFG0, 0xB8);
		else if (afe_priv->mtkaif_protocol == MTKAIF_PROTOCOL_2)
			regmap_write(afe->regmap, AFE_AUD_PAD_TOP_CFG0, 0xB0);
		else
			regmap_write(afe->regmap, AFE_AUD_PAD_TOP_CFG0, 0xB0);
		break;
	default:
		break;
	}

	return 0;
}


static bool is_adda_mtkaif_need_phase_delay(struct mt6899_afe_private *afe_priv)
{
	if (mt6899_afe_gpio_is_prepared(MT6899_AFE_GPIO_DAT_MISO0_ON) &&
	    afe_priv->mtkaif_chosen_phase[0] < 0) {
#ifndef SKIP_SB_CLK
		AUDIO_AEE("adda mtkaif miso0 calib fail");
#endif
		return false;
	}

	if (mt6899_afe_gpio_is_prepared(MT6899_AFE_GPIO_DAT_MISO1_ON) &&
	    afe_priv->mtkaif_chosen_phase[1] < 0) {
#ifndef SKIP_SB_CLK
		AUDIO_AEE("adda mtkaif miso1 calib fail");
#endif
		return false;
	}

	return true;
}

static int mtk_adda_mtkaif_cfg_event(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	int delay_data;
	int delay_cycle;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
#ifdef MTKAIF4
		/* mtkaif_rxif_clkinv_adc inverse for calibration */
		regmap_update_bits(afe->regmap, AFE_MTKAIF0_CFG0,
				   RG_MTKAIF0_RXIF_CLKINV_MASK_SFT,
				   0x1 << RG_MTKAIF0_RXIF_CLKINV_SFT);
		regmap_update_bits(afe->regmap, AFE_MTKAIF1_CFG0,
				   RG_MTKAIF1_RXIF_CLKINV_ADC_MASK_SFT,
				   0x1 << RG_MTKAIF1_RXIF_CLKINV_ADC_SFT);
#endif

		if (afe_priv->mtkaif_protocol == MTKAIF_PROTOCOL_2_CLK_P2) {
			/* set protocol 2 */
			regmap_write(afe->regmap, AFE_MTKAIF0_CFG0,
				     0x00010000);
			regmap_write(afe->regmap, AFE_MTKAIF1_CFG0,
				     0x00010000);

		/* mtkaif_rxif_clkinv_adc inverse for calibration */
		regmap_update_bits(afe->regmap, AFE_MTKAIF0_CFG0,
				   RG_MTKAIF0_RXIF_CLKINV_MASK_SFT,
				   0x1 << RG_MTKAIF0_RXIF_CLKINV_SFT);
		regmap_update_bits(afe->regmap, AFE_MTKAIF1_CFG0,
				   RG_MTKAIF1_RXIF_CLKINV_ADC_MASK_SFT,
				   0x1 << RG_MTKAIF1_RXIF_CLKINV_ADC_SFT);

		/* This event align the phase of every miso pin */
		/* If only 1 miso is used, there is no need to do phase delay. */
		if (strcmp(w->name, "ADDA_MTKAIF_CFG") == 0 &&
		    !is_adda_mtkaif_need_phase_delay(afe_priv)) {
			dev_info(afe->dev,
				 "%s(), check adda mtkaif_chosen_phase[0/1]:%d/%d\n",
				 __func__,
				 afe_priv->mtkaif_chosen_phase[0],
				 afe_priv->mtkaif_chosen_phase[1]);
			break;
		}

		/* set delay for ch12 to align phase of miso0 and miso1 */
		if (afe_priv->mtkaif_phase_cycle[0] >=
		    afe_priv->mtkaif_phase_cycle[1]) {
			delay_data = DELAY_DATA_MISO1;
			delay_cycle = afe_priv->mtkaif_phase_cycle[0] -
				      afe_priv->mtkaif_phase_cycle[1];
		} else {
			delay_data = DELAY_DATA_MISO2;
			delay_cycle = afe_priv->mtkaif_phase_cycle[1] -
				      afe_priv->mtkaif_phase_cycle[0];
		}

		regmap_update_bits(afe->regmap,
				   AFE_MTKAIF0_RX_CFG2,
				   RG_MTKAIF0_RXIF_DELAY_DATA_MASK_SFT,
				   delay_data <<
				   RG_MTKAIF0_RXIF_DELAY_DATA_SFT);

		regmap_update_bits(afe->regmap,
				   AFE_MTKAIF0_RX_CFG2,
				   RG_MTKAIF0_RXIF_DELAY_CYCLE_MASK_SFT,
				   delay_cycle <<
				   RG_MTKAIF0_RXIF_DELAY_CYCLE_SFT);

		/* set delay between ch3 and ch2 */
		if (afe_priv->mtkaif_phase_cycle[2] >=
		    afe_priv->mtkaif_phase_cycle[1]) {
			delay_data = DELAY_DATA_MISO1;  /* ch3 */
			delay_cycle = afe_priv->mtkaif_phase_cycle[2] -
				      afe_priv->mtkaif_phase_cycle[1];
		} else {
			delay_data = DELAY_DATA_MISO2;  /* ch2 */
			delay_cycle = afe_priv->mtkaif_phase_cycle[1] -
				      afe_priv->mtkaif_phase_cycle[2];
		}

		regmap_update_bits(afe->regmap,
				   AFE_MTKAIF1_RX_CFG2,
				   RG_MTKAIF1_RXIF_DELAY_DATA_MASK_SFT,
				   delay_data <<
				   RG_MTKAIF1_RXIF_DELAY_DATA_SFT);
		regmap_update_bits(afe->regmap,
				   AFE_MTKAIF1_RX_CFG2,
				   RG_MTKAIF1_RXIF_DELAY_CYCLE_MASK_SFT,
				   delay_cycle <<
				   RG_MTKAIF1_RXIF_DELAY_CYCLE_SFT);
		} else if (afe_priv->mtkaif_protocol == MTKAIF_PROTOCOL_2) {
			regmap_write(afe->regmap, AFE_MTKAIF0_CFG0,
				     0x00010000);
			regmap_write(afe->regmap, AFE_MTKAIF1_CFG0,
				     0x00010000);
		} else {
			regmap_write(afe->regmap, AFE_MTKAIF0_CFG0, 0x0);
			regmap_write(afe->regmap, AFE_MTKAIF1_CFG0, 0x0);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_adda_dl_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol,
			     int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	dev_info(afe->dev, "%s(), name %s, event 0x%x\n",
		 __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6899_afe_gpio_request(afe, true, MT6899_DAI_ADDA, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* should delayed 1/fs(smallest is 8k) = 125us before afe off */
		udelay(125);
		mt6899_afe_gpio_request(afe, false, MT6899_DAI_ADDA, 0);
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_adda_ch34_dl_event(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol,
				  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	dev_info(afe->dev, "%s(), name %s, event 0x%x\n",
		 __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mt6899_afe_gpio_request(afe, true, MT6899_DAI_ADDA_CH34, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* should delayed 1/fs(smallest is 8k) = 125us before afe off */
		udelay(125);
		mt6899_afe_gpio_request(afe, false, MT6899_DAI_ADDA_CH34, 0);
		break;
	default:
		break;
	}

	return 0;
}

static void mt6363_vs1_vote(struct mtk_base_afe *afe)
{
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	bool pre_enable = afe_priv->is_mt6363_vote;
	bool enable = false;

	if (afe_priv->pmic_regmap == NULL)
		return;
	enable = (afe_priv->is_adda_dl_on && afe_priv->is_adda_dl_max_vol) ||
		afe_priv->is_adda_ul_on ||
		afe_priv->is_vow_enable;
	if (enable == pre_enable) {
		dev_dbg(afe->dev, "%s() enable == pre_enable = %d\n",
			__func__, enable);
		return;
	}
	afe_priv->is_mt6363_vote = enable;
	dev_info(afe->dev, "%s() enable = %d\n",
		__func__, enable);
	if (enable) {
		regmap_update_bits(afe_priv->pmic_regmap, RG_BUCK_VS1_VOTER_EN_LO_SET,
				  VS1_MT6338_MSK, VS1_MT6338_MSK);
	} else {
		regmap_update_bits(afe_priv->pmic_regmap, RG_BUCK_VS1_VOTER_EN_LO_CLR,
				  VS1_MT6338_MSK, VS1_MT6338_MSK);
	}
}

static int mt_vs1_voter_dl_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(afe->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		afe_priv->is_adda_dl_on = true;
		mt6363_vs1_vote(afe);
		break;
	case SND_SOC_DAPM_POST_PMD:
		afe_priv->is_adda_dl_on = false;
		mt6363_vs1_vote(afe);
		break;
	default:
		break;
	}

	return 0;
}

static int mt_vs1_voter_ul_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(afe->dev, "%s(), event = 0x%x\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		afe_priv->is_adda_ul_on = true;
		mt6363_vs1_vote(afe);
		break;
	case SND_SOC_DAPM_POST_PMD:
		afe_priv->is_adda_ul_on = false;
		mt6363_vs1_vote(afe);
		break;
	default:
		break;
	}

	return 0;
}

/* stf */
static int stf_positive_gain_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->stf_positive_gain_db;
	return 0;
}

static int stf_positive_gain_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	int gain_db = ucontrol->value.integer.value[0];

	afe_priv->stf_positive_gain_db = gain_db;

	if (gain_db >= 0 && gain_db <= 24) {
		regmap_update_bits(afe->regmap,
				   AFE_STF_GAIN,
				   SIDE_TONE_POSITIVE_GAIN_MASK_SFT,
				   (gain_db / 6) << SIDE_TONE_POSITIVE_GAIN_SFT);
	} else {
		dev_info(afe->dev, "%s(), gain_db %d invalid\n",
			 __func__, gain_db);
	}
	return 0;
}

/* mtkaif dmic */
static const char *const mt6899_adda_off_on_str[] = {
	"Off", "On"
};

static const struct soc_enum mt6899_adda_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt6899_adda_off_on_str),
			    mt6899_adda_off_on_str),
};

static int mt6899_adda_ap_dmic_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->ap_dmic;
	return 0;
}

static int mt6899_adda_ap_dmic_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int ap_dmic_on;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	ap_dmic_on = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), kcontrol name %s, ap_dmic_on %d\n",
		 __func__, kcontrol->id.name, ap_dmic_on);

	afe_priv->ap_dmic = ap_dmic_on;
	return 0;
}

static int mt6899_adda_dmic_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->mtkaif_dmic;
	return 0;
}

static int mt6899_adda_dmic_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int dmic_on;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	dmic_on = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), kcontrol name %s, dmic_on %d\n",
		 __func__, kcontrol->id.name, dmic_on);

	afe_priv->mtkaif_dmic = dmic_on;
	afe_priv->mtkaif_dmic_ch34 = dmic_on;
	return 0;
}

static int mt6899_adda6_only_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->mtkaif_adda6_only;
	return 0;
}

static int mt6899_adda6_only_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int mtkaif_adda6_only;

	if (ucontrol->value.enumerated.item[0] >= e->items)
		return -EINVAL;

	mtkaif_adda6_only = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), kcontrol name %s, mtkaif_adda6_only %d\n",
		 __func__, kcontrol->id.name, mtkaif_adda6_only);

	afe_priv->mtkaif_adda6_only = mtkaif_adda6_only;
	return 0;
}

static int mt6899_adda_dl_max_vol_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->is_adda_dl_max_vol;

	return 0;
}

static int mt6899_adda_dl_max_vol_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	bool is_adda_dl_max_vol = ucontrol->value.integer.value[0];

	afe_priv->is_adda_dl_max_vol = is_adda_dl_max_vol;
	mt6363_vs1_vote(afe);

	return 0;
}

static int mt6899_vow_enable_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->is_vow_enable;

	return 0;
}

static int mt6899_vow_enable_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	bool is_vow_enable = ucontrol->value.integer.value[0];

	afe_priv->is_vow_enable = is_vow_enable;
	mt6363_vs1_vote(afe);

	return 0;
}
static const struct snd_kcontrol_new mtk_adda_controls[] = {
	SOC_SINGLE("Sidetone_Gain", AFE_STF_GAIN,
		   SIDE_TONE_GAIN_SFT, SIDE_TONE_GAIN_MASK, 0),
	SOC_SINGLE_EXT("Sidetone_Positive_Gain_dB", SND_SOC_NOPM, 0, 100, 0,
		       stf_positive_gain_get, stf_positive_gain_set),
	SOC_SINGLE("ADDA_DL_GAIN", AFE_ADDA_DL_SRC_CON1,
		   AFE_DL_GAIN1_CTL_PRE_SFT, AFE_DL_GAIN1_CTL_PRE_MASK, 0),
	SOC_ENUM_EXT("MTKAIF_DMIC", mt6899_adda_enum[0],
		     mt6899_adda_dmic_get, mt6899_adda_dmic_set),
	SOC_ENUM_EXT("MTKAIF_ADDA6_ONLY", mt6899_adda_enum[0],
		     mt6899_adda6_only_get, mt6899_adda6_only_set),
	SOC_SINGLE_EXT("ADDA_DL_MAX_VOL",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6899_adda_dl_max_vol_get,
		       mt6899_adda_dl_max_vol_set),
	SOC_SINGLE_EXT("VOW_ENABLE",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6899_vow_enable_get,
		       mt6899_vow_enable_set),
	SOC_ENUM_EXT("AP DMIC Used", mt6899_adda_enum[0],
		     mt6899_adda_ap_dmic_get, mt6899_adda_ap_dmic_set),
};

static const struct snd_kcontrol_new stf_ctl =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const uint16_t stf_coeff_table_16k[] = {
	0x049C, 0x09E8, 0x09E0, 0x089C,
	0xFF54, 0xF488, 0xEAFC, 0xEBAC,
	0xfA40, 0x17AC, 0x3D1C, 0x6028,
	0x7538
};

static const uint16_t stf_coeff_table_32k[] = {
	0xFE52, 0x0042, 0x00C5, 0x0194,
	0x029A, 0x03B7, 0x04BF, 0x057D,
	0x05BE, 0x0555, 0x0426, 0x0230,
	0xFF92, 0xFC89, 0xF973, 0xF6C6,
	0xF500, 0xF49D, 0xF603, 0xF970,
	0xFEF3, 0x065F, 0x0F4F, 0x1928,
	0x2329, 0x2C80, 0x345E, 0x3A0D,
	0x3D08
};

static const uint16_t stf_coeff_table_48k[] = {
	0x0401, 0xFFB0, 0xFF5A, 0xFECE,
	0xFE10, 0xFD28, 0xFC21, 0xFB08,
	0xF9EF, 0xF8E8, 0xF80A, 0xF76C,
	0xF724, 0xF746, 0xF7E6, 0xF90F,
	0xFACC, 0xFD1E, 0xFFFF, 0x0364,
	0x0737, 0x0B62, 0x0FC1, 0x1431,
	0x188A, 0x1CA4, 0x2056, 0x237D,
	0x25F9, 0x27B0, 0x2890
};

static int mtk_stf_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol,
			 int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);

	size_t half_tap_num;
	const uint16_t *stf_coeff_table;
	unsigned int ul_rate;

	uint32_t reg_value;
	size_t coef_addr;

	regmap_read(afe->regmap, AFE_ADDA_UL0_SRC_CON0, &ul_rate);
	ul_rate = ul_rate >> UL_VOICE_MODE_CH1_CH2_CTL_SFT;
	ul_rate = ul_rate & UL_VOICE_MODE_CH1_CH2_CTL_MASK;

	if (ul_rate == MTK_AFE_ADDA_UL_RATE_48K) {
		half_tap_num = ARRAY_SIZE(stf_coeff_table_48k);
		stf_coeff_table = stf_coeff_table_48k;
	} else if (ul_rate == MTK_AFE_ADDA_UL_RATE_32K) {
		half_tap_num = ARRAY_SIZE(stf_coeff_table_32k);
		stf_coeff_table = stf_coeff_table_32k;
	} else {
		half_tap_num = ARRAY_SIZE(stf_coeff_table_16k);
		stf_coeff_table = stf_coeff_table_16k;
	}

	regmap_read(afe->regmap, AFE_STF_CON0, &reg_value);

	dev_info(afe->dev, "%s(), name %s, event 0x%x, ul_rate 0x%x, AFE_STF_CON0 0x%x\n",
		 __func__, w->name, event, ul_rate, reg_value);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* set side tone gain = 0 */
		regmap_update_bits(afe->regmap,
				   AFE_STF_GAIN,
				   SIDE_TONE_GAIN_MASK_SFT,
				   0);
		regmap_update_bits(afe->regmap,
				   AFE_STF_GAIN,
				   SIDE_TONE_POSITIVE_GAIN_MASK_SFT,
				   0);
		/* set stf half tap num */
		regmap_update_bits(afe->regmap,
				   AFE_STF_CON0,
				   SIDE_TONE_HALF_TAP_NUM_MASK_SFT,
				   half_tap_num << SIDE_TONE_HALF_TAP_NUM_SFT);

		/* set side tone coefficient */
		regmap_read(afe->regmap, AFE_STF_MON, &reg_value);
		for (coef_addr = 0; coef_addr < half_tap_num; coef_addr++) {
			bool old_w_ready = (reg_value >> SIDE_TONE_W_RDY_SFT) & 0x1;
			bool new_w_ready = 0;
			int try_cnt = 0;

			regmap_update_bits(afe->regmap,
					   AFE_STF_COEFF,
					   0x11FFFFF,
					   (1 << SIDE_TONE_COEFFICIENT_R_W_SEL_SFT) |
					   (coef_addr <<
					    SIDE_TONE_COEFFICIENT_ADDR_SFT) |
					   stf_coeff_table[coef_addr]);

			/* wait until flag write_ready changed */
			for (try_cnt = 0; try_cnt < 10; try_cnt++) {
				regmap_read(afe->regmap,
					    AFE_STF_MON, &reg_value);
				new_w_ready = (reg_value >> SIDE_TONE_W_RDY_SFT) & 0x1;

				/* flip => ok */
				if (new_w_ready == old_w_ready) {
					udelay(3);
					if (try_cnt == 9) {
						dev_info(afe->dev,
							 "%s(), write coeff not ready",
							 __func__);
					}
				} else
					break;
			}

			/* need write -> read -> write to write next coeff */
			regmap_update_bits(afe->regmap,
					AFE_STF_COEFF,
					SIDE_TONE_COEFFICIENT_R_W_SEL_SFT,
					0x0);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* set side tone gain = 0 */
		regmap_update_bits(afe->regmap,
				   AFE_STF_GAIN,
				   SIDE_TONE_GAIN_MASK_SFT,
				   0);
		regmap_update_bits(afe->regmap,
				   AFE_STF_GAIN,
				   SIDE_TONE_POSITIVE_GAIN_MASK_SFT,
				   0);
		break;
	default:
		break;
	}

	return 0;
}

/* ADDA UL MUX */
enum {
	ADDA_UL_MUX_MTKAIF = 0,
	ADDA_UL_MUX_AP_DMIC,
	ADDA_UL_MUX_MASK = 0x1,
};

static const char *const adda_ul_mux_map[] = {
	"MTKAIF", "AP_DMIC"
};

static int adda_ul_map_value[] = {
	ADDA_UL_MUX_MTKAIF,
	ADDA_UL_MUX_AP_DMIC,
};

static SOC_VALUE_ENUM_SINGLE_DECL(adda_ul_mux_map_enum,
				  SND_SOC_NOPM,
				  0,
				  ADDA_UL_MUX_MASK,
				  adda_ul_mux_map,
				  adda_ul_map_value);

static const struct snd_kcontrol_new adda_ul_mux_control =
	SOC_DAPM_ENUM("ADDA_UL_MUX Select", adda_ul_mux_map_enum);

static const struct snd_kcontrol_new adda_ch34_ul_mux_control =
	SOC_DAPM_ENUM("ADDA_CH34_UL_MUX Select", adda_ul_mux_map_enum);

static const struct snd_soc_dapm_widget mtk_dai_adda_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("ADDA_DL_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_adda_dl_ch1_mix,
			   ARRAY_SIZE(mtk_adda_dl_ch1_mix)),
	SND_SOC_DAPM_MIXER("ADDA_DL_CH2", SND_SOC_NOPM, 0, 0,
			   mtk_adda_dl_ch2_mix,
			   ARRAY_SIZE(mtk_adda_dl_ch2_mix)),

	SND_SOC_DAPM_MIXER("ADDA_DL_CH3", SND_SOC_NOPM, 0, 0,
			   mtk_adda_dl_ch3_mix,
			   ARRAY_SIZE(mtk_adda_dl_ch3_mix)),
	SND_SOC_DAPM_MIXER("ADDA_DL_CH4", SND_SOC_NOPM, 0, 0,
			   mtk_adda_dl_ch4_mix,
			   ARRAY_SIZE(mtk_adda_dl_ch4_mix)),

	SND_SOC_DAPM_SUPPLY_S("ADDA Enable", SUPPLY_SEQ_ADDA_AFE_ON,
			      AUDIO_ENGEN_CON0, AUDIO_F3P25M_EN_ON_SFT, 0,
			      NULL, 0),

	/*AFE_ADDA_MTKAIFV4_TX_CFG0 control by PAD_CLK*/
	SND_SOC_DAPM_SUPPLY_S("ADDA Playback Enable", SUPPLY_SEQ_ADDA_DL_ON,
				  AFE_ADDA_DL_SRC_CON0,
				  AFE_DL_SRC_ON_TMP_CTL_PRE_SFT, 0,
			      mtk_adda_dl_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ADDA CH34 Playback Enable",
			      SUPPLY_SEQ_ADDA_DL_ON,
			      AFE_ADDA_2ND_DL_SRC_CON0,
				  AFE_2ND_DL_SRC_ON_TMP_CTL_PRE_SFT, 0,
			      mtk_adda_ch34_dl_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("ADDA Capture Enable", SUPPLY_SEQ_ADDA_UL_ON,
			      AFE_ADDA_UL0_SRC_CON0,
			      UL_SRC_ON_TMP_CTL_SFT, 0,
			      mtk_adda_ul_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("ADDA CH34 Capture Enable", SUPPLY_SEQ_ADDA_UL_ON,
			      AFE_ADDA_UL1_SRC_CON0,
			      UL_SRC_ON_TMP_CTL_SFT, 0,
			      mtk_adda_ch34_ul_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("AUD_PAD_TOP", SUPPLY_SEQ_ADDA_AUD_PAD_TOP,
			      AFE_AUD_PAD_TOP_CFG0,
			      RG_RX_FIFO_ON_SFT, 0,
			      mtk_adda_pad_top_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("ADDA_MTKAIF_CFG", SUPPLY_SEQ_ADDA_MTKAIF_CFG,
			      SND_SOC_NOPM, 0, 0,
			      mtk_adda_mtkaif_cfg_event,
			      SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_SUPPLY_S("ADDA6_MTKAIF_CFG", SUPPLY_SEQ_ADDA6_MTKAIF_CFG,
			      SND_SOC_NOPM, 0, 0,
			      mtk_adda_mtkaif_cfg_event,
			      SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_SUPPLY_S("AP_DMIC_EN", SUPPLY_SEQ_ADDA_AP_DMIC,
			      AFE_ADDA_UL0_SRC_CON0,
			      UL_AP_DMIC_ON_SFT, 0,
			      mtk_adda_ul_ap_dmic_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("AP_DMIC_CH34_EN", SUPPLY_SEQ_ADDA_AP_DMIC,
			      AFE_ADDA_UL1_SRC_CON0,
			      UL_AP_DMIC_ON_SFT, 0,
			      mtk_adda_ch34_ul_ap_dmic_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("ADDA_FIFO", SUPPLY_SEQ_ADDA_FIFO,
			      AFE_ADDA_UL0_SRC_CON1,
			      FIFO_SOFT_RST_SFT, 1,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADDA_CH34_FIFO", SUPPLY_SEQ_ADDA_FIFO,
			      AFE_ADDA_UL1_SRC_CON1,
			      FIFO_SOFT_RST_SFT, 1,
			      NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("VS1_VOTER_DL", SUPPLY_SEQ_ADDA_AFE_ON,
			      SND_SOC_NOPM, 0, 0,
			      mt_vs1_voter_dl_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("VS1_VOTER_UL", SUPPLY_SEQ_ADDA_AFE_ON,
			      SND_SOC_NOPM, 0, 0,
			      mt_vs1_voter_ul_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("ADDA_UL_Mux", SND_SOC_NOPM, 0, 0,
			 &adda_ul_mux_control),
	SND_SOC_DAPM_MUX("ADDA_CH34_UL_Mux", SND_SOC_NOPM, 0, 0,
			 &adda_ch34_ul_mux_control),

	SND_SOC_DAPM_INPUT("AP_DMIC_INPUT"),
	SND_SOC_DAPM_INPUT("AP_DMIC_CH34_INPUT"),

	/* stf */
	SND_SOC_DAPM_SWITCH_E("Sidetone Filter",
			      AFE_STF_CON0, SIDE_TONE_ON_SFT, 0,
			      &stf_ctl,
			      mtk_stf_event,
			      SND_SOC_DAPM_PRE_PMU |
			      SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("STF_CH1", SND_SOC_NOPM, 0, 0,
			   mtk_stf_ch1_mix,
			   ARRAY_SIZE(mtk_stf_ch1_mix)),
	SND_SOC_DAPM_OUTPUT("STF_OUTPUT"),

	/* allow i2s on without codec on */
	SND_SOC_DAPM_OUTPUT("ADDA_DUMMY_OUT"),
	SND_SOC_DAPM_MUX("ADDA_Out_Mux",
			 SND_SOC_NOPM, 0, 0, &adda_out_mux_control),
	SND_SOC_DAPM_INPUT("ADDA_DUMMY_IN"),
	SND_SOC_DAPM_MUX("ADDA_In_Mux",
			 SND_SOC_NOPM, 0, 0, &adda_in_mux_control),

#if !defined(IS_FPGA_EARLY_PORTING)
	/* clock */
	SND_SOC_DAPM_CLOCK_SUPPLY("top_mux_audio_h"),

	SND_SOC_DAPM_CLOCK_SUPPLY("aud_dl0_dac_clk"),
	SND_SOC_DAPM_CLOCK_SUPPLY("aud_dl0_dac_hires_clk"),
	SND_SOC_DAPM_CLOCK_SUPPLY("aud_dl0_dac_predis_clk"),
	SND_SOC_DAPM_CLOCK_SUPPLY("aud_dl1_dac_clk"),
	SND_SOC_DAPM_CLOCK_SUPPLY("aud_dl1_dac_hires_clk"),
	SND_SOC_DAPM_CLOCK_SUPPLY("aud_dl1_dac_predis_clk"),

	SND_SOC_DAPM_CLOCK_SUPPLY("aud_ul0_adc_clk"),
	/* SND_SOC_DAPM_CLOCK_SUPPLY("aud_ul0_adc_hires_clk"), */
	SND_SOC_DAPM_CLOCK_SUPPLY("aud_ul1_adc_clk"),
	/* SND_SOC_DAPM_CLOCK_SUPPLY("aud_ul1_adc_hires_clk"), */
#endif
};

#define HIRES_THRESHOLD 48000
#if !defined(IS_FPGA_EARLY_PORTING)
static int mtk_afe_dac_hires_connect(struct snd_soc_dapm_widget *source,
				     struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_widget *w = source;
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_afe_adda_priv *adda_priv;

	adda_priv = get_adda_priv_by_name(afe, w->name);

	if (!adda_priv) {
		AUDIO_AEE("adda_priv == NULL");
		return 0;
	}

	return (adda_priv->dl_rate > HIRES_THRESHOLD) ? 1 : 0;
}

#endif

static const struct snd_soc_dapm_route mtk_dai_adda_routes[] = {
	/* playback */
	{"ADDA_DL_CH1", "DL0_CH1", "DL0"},
	{"ADDA_DL_CH2", "DL0_CH1", "DL0"},
	{"ADDA_DL_CH2", "DL0_CH2", "DL0"},

	{"ADDA_DL_CH1", "DL1_CH1", "DL1"},
	{"ADDA_DL_CH2", "DL1_CH2", "DL1"},

	{"ADDA_DL_CH1", "DL2_CH1", "DL2"},
	{"ADDA_DL_CH2", "DL2_CH2", "DL2"},

	{"ADDA_DL_CH1", "DL3_CH1", "DL3"},
	{"ADDA_DL_CH2", "DL3_CH2", "DL3"},

	{"ADDA_DL_CH1", "DL4_CH1", "DL4"},
	{"ADDA_DL_CH2", "DL4_CH2", "DL4"},

	{"ADDA_DL_CH1", "DL5_CH1", "DL5"},
	{"ADDA_DL_CH2", "DL5_CH2", "DL5"},

	{"ADDA_DL_CH1", "DL6_CH1", "DL6"},
	{"ADDA_DL_CH2", "DL6_CH2", "DL6"},

	{"ADDA_DL_CH1", "DL7_CH1", "DL7"},
	{"ADDA_DL_CH2", "DL7_CH2", "DL7"},

	{"ADDA_DL_CH1", "DL8_CH1", "DL8"},
	{"ADDA_DL_CH2", "DL8_CH2", "DL8"},

	{"ADDA_DL_CH1", "DL_24CH_CH1", "DL_24CH"},
	{"ADDA_DL_CH2", "DL_24CH_CH2", "DL_24CH"},

	{"ADDA_DL_CH1", "DL24_CH1", "DL24"},
	{"ADDA_DL_CH2", "DL24_CH2", "DL24"},
	{"ADDA_DL_CH3", "DL24_CH1", "DL24"},
	{"ADDA_DL_CH4", "DL24_CH2", "DL24"},

	{"ADDA_DL_CH1", "HW_SRC_2_OUT_CH1", "Hostless_HW_SRC_2_IN_DL"},
	{"ADDA_DL_CH2", "HW_SRC_2_OUT_CH2", "Hostless_HW_SRC_2_IN_DL"},
	{"ADDA_DL_CH3", "HW_SRC_2_OUT_CH1", "Hostless_HW_SRC_2_IN_DL"},
	{"ADDA_DL_CH4", "HW_SRC_2_OUT_CH2", "Hostless_HW_SRC_2_IN_DL"},

	{"ADDA Playback", NULL, "ADDA_DL_CH1"},
	{"ADDA Playback", NULL, "ADDA_DL_CH2"},

	{"ADDA Playback", NULL, "ADDA Enable"},
	{"ADDA Playback", NULL, "ADDA Playback Enable"},
	{"ADDA Playback", NULL, "AUD_PAD_TOP"},
	{"ADDA Playback", NULL, "VS1_VOTER_DL"},

	{"ADDA_DL_CH3", "DL0_CH1", "DL0"},
	{"ADDA_DL_CH4", "DL0_CH2", "DL0"},

	{"ADDA_DL_CH3", "DL1_CH1", "DL1"},
	{"ADDA_DL_CH4", "DL1_CH2", "DL1"},

	{"ADDA_DL_CH3", "DL2_CH1", "DL2"},
	{"ADDA_DL_CH4", "DL2_CH2", "DL2"},

	{"ADDA_DL_CH3", "DL3_CH1", "DL3"},
	{"ADDA_DL_CH4", "DL3_CH2", "DL3"},

	{"ADDA_DL_CH3", "DL4_CH1", "DL4"},
	{"ADDA_DL_CH4", "DL4_CH2", "DL4"},

	{"ADDA_DL_CH3", "DL5_CH1", "DL5"},
	{"ADDA_DL_CH4", "DL5_CH2", "DL5"},

	{"ADDA_DL_CH3", "DL6_CH1", "DL6"},
	{"ADDA_DL_CH4", "DL6_CH2", "DL6"},

	{"ADDA_DL_CH3", "DL7_CH1", "DL7"},
	{"ADDA_DL_CH4", "DL7_CH2", "DL7"},

	{"ADDA_DL_CH3", "DL8_CH1", "DL8"},
	{"ADDA_DL_CH4", "DL8_CH2", "DL8"},

	{"ADDA_DL_CH3", "DL_24CH_CH1", "DL_24CH"},
	{"ADDA_DL_CH4", "DL_24CH_CH2", "DL_24CH"},
	{"ADDA_DL_CH3", "DL_24CH_CH3", "DL_24CH"},
	{"ADDA_DL_CH4", "DL_24CH_CH4", "DL_24CH"},

	{"ADDA CH34 Playback", NULL, "ADDA_DL_CH3"},
	{"ADDA CH34 Playback", NULL, "ADDA_DL_CH4"},

	{"ADDA CH34 Playback", NULL, "ADDA Enable"},
	{"ADDA CH34 Playback", NULL, "ADDA CH34 Playback Enable"},
	{"ADDA CH34 Playback", NULL, "AUD_PAD_TOP"},
	{"ADDA CH34 Playback", NULL, "VS1_VOTER_DL"},

	/* capture */
	{"ADDA_UL_Mux", "MTKAIF", "ADDA Capture"},
	{"ADDA_UL_Mux", "AP_DMIC", "AP DMIC Capture"},

	{"ADDA_CH34_UL_Mux", "MTKAIF", "ADDA CH34 Capture"},
	{"ADDA_CH34_UL_Mux", "AP_DMIC", "AP DMIC CH34 Capture"},

	{"ADDA Capture", NULL, "ADDA Enable"},
	{"ADDA Capture", NULL, "mt6368_vaud18"},
	{"ADDA Capture", NULL, "ADDA Capture Enable"},
	{"ADDA Capture", NULL, "AUD_PAD_TOP"},
	{"ADDA Capture", NULL, "ADDA_MTKAIF_CFG"},
	{"ADDA Capture", NULL, "VS1_VOTER_UL"},

	{"AP DMIC Capture", NULL, "ADDA Enable"},
	{"AP DMIC Capture", NULL, "ADDA Capture Enable"},
	{"AP DMIC Capture", NULL, "ADDA_FIFO"},
	{"AP DMIC Capture", NULL, "AP_DMIC_EN"},

	{"ADDA CH34 Capture", NULL, "ADDA Enable"},
	{"ADDA CH34 Capture", NULL, "ADDA CH34 Capture Enable"},
	{"ADDA CH34 Capture", NULL, "AUD_PAD_TOP"},
	{"ADDA CH34 Capture", NULL, "ADDA6_MTKAIF_CFG"},
	{"ADDA CH34 Capture", NULL, "VS1_VOTER_UL"},

	{"AP DMIC CH34 Capture", NULL, "ADDA Enable"},
	{"AP DMIC CH34 Capture", NULL, "ADDA CH34 Capture Enable"},
	{"AP DMIC CH34 Capture", NULL, "ADDA_CH34_FIFO"},
	{"AP DMIC CH34 Capture", NULL, "AP_DMIC_CH34_EN"},

	{"AP DMIC Capture", NULL, "AP_DMIC_INPUT"},
	{"AP DMIC CH34 Capture", NULL, "AP_DMIC_CH34_INPUT"},

	/* sidetone filter */
	{"Sidetone Filter", "Switch", "STF_CH1"},

	{"STF_OUTPUT", NULL, "Sidetone Filter"},
	{"ADDA Playback", NULL, "Sidetone Filter"},
	{"ADDA CH34 Playback", NULL, "Sidetone Filter"},

	/* allow i2s on without codec on */
	{"ADDA Capture", NULL, "ADDA_In_Mux"},
	{"ADDA_In_Mux", "Dummy_Widget", "ADDA_DUMMY_IN"},
	{"ADDA_Out_Mux", "Dummy_Widget", "ADDA Playback"},
	{"ADDA_DUMMY_OUT", NULL, "ADDA_Out_Mux"},
#if !defined(IS_FPGA_EARLY_PORTING)

	/* clk */
	{"ADDA Playback", NULL, "aud_dl0_dac_clk"},
	{"ADDA Playback", NULL, "aud_dl0_dac_predis_clk"},
	{"ADDA Playback", NULL, "aud_dl0_dac_hires_clk",
	 mtk_afe_dac_hires_connect},
	{"ADDA CH34 Playback", NULL, "aud_dl1_dac_clk"},
	{"ADDA CH34 Playback", NULL, "aud_dl1_dac_predis_clk"},
	{"ADDA CH34 Playback", NULL, "aud_dl1_dac_hires_clk",
	 mtk_afe_dac_hires_connect},

	{"ADDA Capture Enable", NULL, "aud_ul0_adc_clk"},
	/* {"ADDA Capture Enable", NULL, "aud_ul0_adc_hires_clk", */
	/* mtk_afe_adc_hires_connect}, */
	{"ADDA CH34 Capture Enable", NULL, "aud_ul1_adc_clk"},
	/* {"ADDA CH34 Capture Enable", NULL, "aud_ul1_adc_hires_clk", */
	/* mtk_afe_adc_hires_connect}, */

	/* hires source from apll1 */
	{"top_mux_audio_h", NULL, APLL2_W_NAME},
	{"aud_dl0_dac_hires_clk", NULL, "top_mux_audio_h"},
	{"aud_dl1_dac_hires_clk", NULL, "top_mux_audio_h"},
	/* {"aud_ul0_adc_hires_clk", NULL, "top_mux_audio_h"}, */
	/* {"aud_ul1_adc_hires_clk", NULL, "top_mux_audio_h"}, */
#endif
};

/* dai ops */
static int mtk_dai_adda_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	unsigned int rate = params_rate(params);
#ifdef MTKAIF4
	unsigned int mtkaif_rate = 0;
#endif
	int id = dai->id;
	struct mtk_afe_adda_priv *adda_priv = afe_priv->dai_priv[id];

	dev_info(afe->dev, "%s(), id %d, stream %d, rate %d\n",
		 __func__,
		 id,
		 substream->stream,
		 rate);

	if (!adda_priv) {
		AUDIO_AEE("adda_priv == NULL");
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		unsigned int dl_src_con0 = 0;
		unsigned int dl_src_con1 = 0;

		adda_priv->dl_rate = rate;

#ifdef MTKAIF4
		/* get mtkaif dl rate */
		mtkaif_rate =
			mtkaif_rate_transform(afe, adda_priv->dl_rate);
#endif

		/* set sampling rate */
		dl_src_con0 = adda_dl_rate_transform(afe, rate) <<
			       AFE_DL_INPUT_MODE_CTL_SFT;

		/* set output mode, UP_SAMPLING_RATE_X8 */
		dl_src_con0 |= (0x3 << AFE_DL_OUTPUT_SEL_CTL_SFT);

		/* turn off mute function */
		dl_src_con0 |= (0x01 << AFE_DL_MUTE_CH2_OFF_CTL_PRE_SFT);
		dl_src_con0 |= (0x01 << AFE_DL_MUTE_CH1_OFF_CTL_PRE_SFT);

		/* set voice input data if input sample rate is 8k or 16k */
		if (rate == 8000 || rate == 16000)
			dl_src_con0 |= 0x01 << AFE_DL_VOICE_MODE_CTL_PRE_SFT;

		/* SA suggest apply -0.3db to audio/speech path */
		dl_src_con1 = MTK_AFE_ADDA_DL_GAIN_NORMAL <<
			       AFE_DL_GAIN1_CTL_PRE_SFT;
		dl_src_con1 |= MTK_AFE_ADDA_DL_GAIN_NORMAL <<
			       AFE_DL_GAIN2_CTL_PRE_SFT;

		/* turn on down-link gain */
		dl_src_con0 |= (0x01 << AFE_DL_GAIN_ON_CTL_PRE_SFT);

		if (id == MT6899_DAI_ADDA) {
#ifdef MTKAIF4
			/* MTKAIF sample rate config */
			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_TX_CFG0,
					MTKAIFV4_TXIF_INPUT_MODE_MASK_SFT,
					mtkaif_rate << MTKAIFV4_TXIF_INPUT_MODE_SFT);
			/* AFE_ADDA_MTKAIFV4_TX_CFG0 */
			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_TX_CFG0,
					MTKAIFV4_TXIF_FOUR_CHANNEL_MASK_SFT,
					0x0 << MTKAIFV4_TXIF_FOUR_CHANNEL_SFT);
			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_TX_CFG0,
					MTKAIFV4_ADDA_OUT_EN_SEL_MASK_SFT,
					0x1 << MTKAIFV4_ADDA_OUT_EN_SEL_SFT);
			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_TX_CFG0,
					MTKAIFV4_ADDA6_OUT_EN_SEL_MASK_SFT,
					0x1 << MTKAIFV4_ADDA6_OUT_EN_SEL_SFT);
			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_TX_CFG0,
					MTKAIFV4_TXIF_V4_MASK_SFT,
					0x1 << MTKAIFV4_TXIF_V4_SFT);
			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_TX_CFG0,
					MTKAIFV4_TXIF_EN_SEL_MASK_SFT,
					0x0 << MTKAIFV4_TXIF_EN_SEL_SFT);
#endif
			/* clean predistortion */
			regmap_write(afe->regmap, AFE_ADDA_DL_PREDIS_CON0, 0);
			regmap_write(afe->regmap, AFE_ADDA_DL_PREDIS_CON1, 0);

			regmap_write(afe->regmap,
					 AFE_ADDA_DL_SRC_CON0, dl_src_con0);
			regmap_write(afe->regmap,
					 AFE_ADDA_DL_SRC_CON1, dl_src_con1);

			/* set sdm gain */
			regmap_update_bits(afe->regmap,
					   AFE_ADDA_DL_SDM_DCCOMP_CON,
					   AFE_DL_ATTGAIN_CTL_MASK_SFT,
					   AUDIO_SDM_LEVEL_NORMAL <<
					   AFE_DL_ATTGAIN_CTL_SFT);

			/* 2nd sdm */
			regmap_update_bits(afe->regmap,
					   AFE_ADDA_DL_SDM_DCCOMP_CON,
					   AFE_DL_USE_3RD_SDM_MASK_SFT,
					   AUDIO_SDM_2ND << AFE_DL_USE_3RD_SDM_SFT);

			/* sdm auto reset */
			regmap_write(afe->regmap,
					 AFE_ADDA_DL_SDM_AUTO_RESET_CON,
					 SDM_AUTO_RESET_THRESHOLD);
			regmap_update_bits(
				afe->regmap,
				AFE_ADDA_DL_SDM_AUTO_RESET_CON,
				AFE_DL_SDM_AUTO_RESET_TEST_ON_SFT,
				0x1 << AFE_DL_SDM_AUTO_RESET_TEST_ON_SFT);
		} else {
#ifdef MTKAIF4
			/* MTKAIF sample rate config */
			regmap_update_bits(afe->regmap, AFE_ADDA6_MTKAIFV4_TX_CFG0,
					ADDA6_MTKAIFV4_TXIF_INPUT_MODE_MASK_SFT,
					mtkaif_rate << ADDA6_MTKAIFV4_TXIF_INPUT_MODE_SFT);
			/* AFE_ADDA6_MTKAIFV4_TX_CFG0 */
			regmap_update_bits(afe->regmap, AFE_ADDA6_MTKAIFV4_TX_CFG0,
					ADDA6_MTKAIFV4_TXIF_FOUR_CHANNEL_MASK_SFT,
					0x0 << ADDA6_MTKAIFV4_TXIF_FOUR_CHANNEL_SFT);
			regmap_update_bits(afe->regmap, AFE_ADDA6_MTKAIFV4_TX_CFG0,
					ADDA6_MTKAIFV4_TXIF_EN_SEL_MASK_SFT,
					0x1 << ADDA6_MTKAIFV4_TXIF_EN_SEL_SFT);
#endif
		}
	} else {
		unsigned int voice_mode = 0;
		unsigned int ul_src_con0 = 0;   /* default value */

		adda_priv->ul_rate = rate;

#ifdef MTKAIF4
		/* get mtkaif dl rate */
		mtkaif_rate =
			mtkaif_rate_transform(afe, adda_priv->ul_rate);
#endif

		voice_mode = adda_ul_rate_transform(afe, rate);

		ul_src_con0 |= (voice_mode << 17) & (0x7 << 17);

		/* enable iir */
		ul_src_con0 |= (1 << UL_IIR_ON_TMP_CTL_SFT) &
			       UL_IIR_ON_TMP_CTL_MASK_SFT;
		ul_src_con0 |= (UL_IIR_SW << UL_IIRMODE_CTL_SFT) &
			       UL_IIRMODE_CTL_MASK_SFT;

		switch (id) {
		case MT6899_DAI_ADDA:
#ifdef MTKAIF4
			if (afe_priv->audio_r_miso1_enable == 1) {
				/* AFE_ADDA_MTKAIFV4_RX_CFG0 */
				regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
						MTKAIFV4_RXIF_FOUR_CHANNEL_MASK_SFT,
						0x0 << MTKAIFV4_RXIF_FOUR_CHANNEL_SFT);
				regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
						MTKAIFV4_RXIF_EN_SEL_MASK_SFT,
						0x1 << MTKAIFV4_RXIF_EN_SEL_SFT);

				/* AFE_ADDA6_MTKAIFV4_RX_CFG0 */
				regmap_update_bits(afe->regmap, AFE_ADDA6_MTKAIFV4_RX_CFG0,
						ADDA6_MTKAIFV4_RXIF_FOUR_CHANNEL_MASK_SFT,
						0x1 << ADDA6_MTKAIFV4_RXIF_FOUR_CHANNEL_SFT);
				regmap_update_bits(afe->regmap, AFE_ADDA6_MTKAIFV4_RX_CFG0,
						ADDA6_MTKAIFV4_RXIF_EN_SEL_MASK_SFT,
						0x1 << ADDA6_MTKAIFV4_RXIF_EN_SEL_SFT);
			} else {
				regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
						MTKAIFV4_RXIF_INPUT_MODE_MASK_SFT,
						mtkaif_rate << MTKAIFV4_RXIF_INPUT_MODE_SFT);
				/* AFE_ADDA_MTKAIFV4_RX_CFG0 */
				regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
						MTKAIFV4_RXIF_FOUR_CHANNEL_MASK_SFT,
						0x1 << MTKAIFV4_RXIF_FOUR_CHANNEL_SFT);
				regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
						MTKAIFV4_RXIF_EN_SEL_MASK_SFT,
						0x0 << MTKAIFV4_RXIF_EN_SEL_SFT);
				/* [28] loopback mode
				 * 0: loopback adda tx to adda rx
				 * 1: loopback adda6 tx to adda rx
				 */
				regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
						MTKAIFV4_TXIF_EN_SEL_MASK_SFT,
						0x0 << MTKAIFV4_TXIF_EN_SEL_SFT);
			}

			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
					MTKAIFV4_UL_CH1CH2_IN_EN_SEL_MASK_SFT,
					0x1 << MTKAIFV4_UL_CH1CH2_IN_EN_SEL_SFT);
			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
					MTKAIFV4_UL_CH3CH4_IN_EN_SEL_MASK_SFT,
					0x1 << MTKAIFV4_UL_CH3CH4_IN_EN_SEL_SFT);
#endif
			/* 35Hz @ 48k */
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_02_01, 0x00000000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_04_03, 0x00003FB8);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_06_05, 0x3FB80000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_08_07, 0x3FB80000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_10_09, 0x0000C048);

			regmap_update_bits(afe->regmap,
					 AFE_ADDA_UL1_SRC_CON0,
					 UL_IIR_ON_TMP_CTL_MASK_SFT |
					 UL_IIRMODE_CTL_MASK_SFT |
					 UL_VOICE_MODE_CH1_CH2_CTL_MASK_SFT,
					 ul_src_con0);

			/* mtkaif_rxif_data_mode = 0, amic */
			if (afe_priv->mtkaif_dmic) {
				regmap_update_bits(afe->regmap,
					AFE_MTKAIF1_RX_CFG0,
					0x1 << 0,
					0x1 << 0);
			} else {
				regmap_update_bits(afe->regmap,
					AFE_MTKAIF1_RX_CFG0,
					0x1 << 0,
					0x0 << 0);
			}

			/* 35Hz @ 48k */
			regmap_write(afe->regmap,
					 AFE_ADDA_UL0_IIR_COEF_02_01, 0x00000000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL0_IIR_COEF_04_03, 0x00003FB8);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL0_IIR_COEF_06_05, 0x3FB80000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL0_IIR_COEF_08_07, 0x3FB80000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL0_IIR_COEF_10_09, 0x0000C048);

			regmap_update_bits(afe->regmap,
					 AFE_ADDA_UL0_SRC_CON0,
					 UL_IIR_ON_TMP_CTL_MASK_SFT |
					 UL_IIRMODE_CTL_MASK_SFT |
					 UL_VOICE_MODE_CH1_CH2_CTL_MASK_SFT,
					 ul_src_con0);

			/* mtkaif_rxif_data_mode = 0, amic */
			if (afe_priv->mtkaif_dmic) {
				regmap_update_bits(afe->regmap,
					AFE_MTKAIF0_RX_CFG0,
					0x1 << 0,
					0x1 << 0);
			} else {
				regmap_update_bits(afe->regmap,
					AFE_MTKAIF0_RX_CFG0,
					0x1 << 0,
					0x0 << 0);
			}
			break;

		case MT6899_DAI_AP_DMIC:
#ifdef MTKAIF4

			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
					MTKAIFV4_UL_CH1CH2_IN_EN_SEL_MASK_SFT,
					0x0 << MTKAIFV4_UL_CH1CH2_IN_EN_SEL_SFT);
			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
					MTKAIFV4_UL_CH3CH4_IN_EN_SEL_MASK_SFT,
					0x0 << MTKAIFV4_UL_CH3CH4_IN_EN_SEL_SFT);
#else
			/* mtkaif3.0 no bypass ul0 */
			regmap_update_bits(afe->regmap,
					AFE_MTKAIF0_CFG0,
					RG_MTKAIF0_RXIF_BYPASS_SRC_MASK_SFT,
					0x0 << RG_MTKAIF0_RXIF_BYPASS_SRC_SFT);
#endif
			/* 35Hz @ 48k */
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_02_01, 0x00000000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_04_03, 0x00003FB8);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_06_05, 0x3FB80000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_08_07, 0x3FB80000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_10_09, 0x0000C048);

			regmap_update_bits(afe->regmap,
					 AFE_ADDA_UL1_SRC_CON0,
					 UL_IIR_ON_TMP_CTL_MASK_SFT |
					 UL_IIRMODE_CTL_MASK_SFT |
					 UL_VOICE_MODE_CH1_CH2_CTL_MASK_SFT,
					 ul_src_con0);

			/* mtkaif_rxif_data_mode = 0, amic */
			regmap_update_bits(afe->regmap,
					   AFE_MTKAIF1_RX_CFG0,
					   0x1 << 0,
					   0x0 << 0);

			/* 35Hz @ 48k */
			regmap_write(afe->regmap,
					 AFE_ADDA_UL0_IIR_COEF_02_01, 0x00000000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL0_IIR_COEF_04_03, 0x00003FB8);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL0_IIR_COEF_06_05, 0x3FB80000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL0_IIR_COEF_08_07, 0x3FB80000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL0_IIR_COEF_10_09, 0x0000C048);

			regmap_update_bits(afe->regmap,
					 AFE_ADDA_UL0_SRC_CON0,
					 UL_IIR_ON_TMP_CTL_MASK_SFT |
					 UL_IIRMODE_CTL_MASK_SFT |
					 UL_VOICE_MODE_CH1_CH2_CTL_MASK_SFT,
					 ul_src_con0);

			/* mtkaif_rxif_data_mode = 0, amic */
			regmap_update_bits(afe->regmap,
					AFE_MTKAIF0_RX_CFG0,
					0x1 << 0,
					0x0 << 0);
			break;
		case MT6899_DAI_ADDA_CH34:
#ifdef MTKAIF4
			if (afe_priv->audio_r_miso1_enable == 0) {
				/* AFE_ADDA_MTKAIFV4_RX_CFG0 */
				regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
						MTKAIFV4_RXIF_FOUR_CHANNEL_MASK_SFT,
						0x1 << MTKAIFV4_RXIF_FOUR_CHANNEL_SFT);
				regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
						MTKAIFV4_RXIF_EN_SEL_MASK_SFT,
						0x0 << MTKAIFV4_RXIF_EN_SEL_SFT);
			}
			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
					MTKAIFV4_UL_CH1CH2_IN_EN_SEL_MASK_SFT,
					0x1 << MTKAIFV4_UL_CH1CH2_IN_EN_SEL_SFT);
			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
					MTKAIFV4_UL_CH3CH4_IN_EN_SEL_MASK_SFT,
					0x1 << MTKAIFV4_UL_CH3CH4_IN_EN_SEL_SFT);

#endif
			/* 35Hz @ 48k */
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_02_01, 0x00000000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_04_03, 0x00003FB8);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_06_05, 0x3FB80000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_08_07, 0x3FB80000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_10_09, 0x0000C048);

			regmap_update_bits(afe->regmap,
					 AFE_ADDA_UL1_SRC_CON0,
					 UL_IIR_ON_TMP_CTL_MASK_SFT |
					 UL_IIRMODE_CTL_MASK_SFT |
					 UL_VOICE_MODE_CH1_CH2_CTL_MASK_SFT,
					 ul_src_con0);

			/* mtkaif_rxif_data_mode = 0, amic */
			if (afe_priv->mtkaif_dmic) {
				regmap_update_bits(afe->regmap,
					AFE_MTKAIF1_RX_CFG0,
					0x1 << 0,
					0x1 << 0);
			} else {
				regmap_update_bits(afe->regmap,
					AFE_MTKAIF1_RX_CFG0,
					0x1 << 0,
					0x0 << 0);
			}
			break;
		case MT6899_DAI_AP_DMIC_CH34:
#ifdef MTKAIF4
			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
					MTKAIFV4_UL_CH1CH2_IN_EN_SEL_MASK_SFT,
					0x0 << MTKAIFV4_UL_CH1CH2_IN_EN_SEL_SFT);
			regmap_update_bits(afe->regmap, AFE_ADDA_MTKAIFV4_RX_CFG0,
					MTKAIFV4_UL_CH3CH4_IN_EN_SEL_MASK_SFT,
					0x0 << MTKAIFV4_UL_CH3CH4_IN_EN_SEL_SFT);
#else
			regmap_update_bits(afe->regmap,
					AFE_MTKAIF1_CFG0,
					RG_MTKAIF1_RXIF_BYPASS_SRC_MASK_SFT,
					0x0 << RG_MTKAIF1_RXIF_BYPASS_SRC_SFT);

#endif
			/* 35Hz @ 48k */
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_02_01, 0x00000000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_04_03, 0x00003FB8);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_06_05, 0x3FB80000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_08_07, 0x3FB80000);
			regmap_write(afe->regmap,
					 AFE_ADDA_UL1_IIR_COEF_10_09, 0x0000C048);

			regmap_update_bits(afe->regmap,
					 AFE_ADDA_UL1_SRC_CON0,
					 UL_IIR_ON_TMP_CTL_MASK_SFT |
					 UL_IIRMODE_CTL_MASK_SFT |
					 UL_VOICE_MODE_CH1_CH2_CTL_MASK_SFT,
					 ul_src_con0);

			/* mtkaif_rxif_data_mode = 0, amic */
			regmap_update_bits(afe->regmap,
					AFE_MTKAIF1_RX_CFG0,
					0x1 << 0,
					0x0 << 0);

			break;
		default:
			break;

		}

		/* ap dmic */
		switch (id) {
		case MT6899_DAI_AP_DMIC:
		case MT6899_DAI_AP_DMIC_CH34:
			mtk_adda_ul_src_dmic(afe, id);
			break;
		default:
			break;
		}
	}

	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_adda_ops = {
	.hw_params = mtk_dai_adda_hw_params,
};

/* dai driver */
#define MTK_ADDA_PLAYBACK_RATES (SNDRV_PCM_RATE_8000_48000 |\
				 SNDRV_PCM_RATE_96000 |\
				 SNDRV_PCM_RATE_192000)

#define MTK_ADDA_CAPTURE_RATES (SNDRV_PCM_RATE_8000 |\
				SNDRV_PCM_RATE_16000 |\
				SNDRV_PCM_RATE_32000 |\
				SNDRV_PCM_RATE_48000 |\
				SNDRV_PCM_RATE_96000 |\
				SNDRV_PCM_RATE_192000)

#define MTK_ADDA_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			  SNDRV_PCM_FMTBIT_S24_LE |\
			  SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_adda_driver[] = {
	{
		.name = "ADDA",
		.id = MT6899_DAI_ADDA,
		.playback = {
			.stream_name = "ADDA Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ADDA_PLAYBACK_RATES,
			.formats = MTK_ADDA_FORMATS,
		},
		.capture = {
			.stream_name = "ADDA Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ADDA_CAPTURE_RATES,
			.formats = MTK_ADDA_FORMATS,
		},
		.ops = &mtk_dai_adda_ops,
	},
	{
		.name = "ADDA_CH34",
		.id = MT6899_DAI_ADDA_CH34,
		.playback = {
			.stream_name = "ADDA CH34 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ADDA_PLAYBACK_RATES,
			.formats = MTK_ADDA_FORMATS,
		},
		.capture = {
			.stream_name = "ADDA CH34 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ADDA_CAPTURE_RATES,
			.formats = MTK_ADDA_FORMATS,
		},
		.ops = &mtk_dai_adda_ops,
	},
	{
		.name = "AP_DMIC",
		.id = MT6899_DAI_AP_DMIC,
		.capture = {
			.stream_name = "AP DMIC Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ADDA_CAPTURE_RATES,
			.formats = MTK_ADDA_FORMATS,
		},
		.ops = &mtk_dai_adda_ops,
	},
	{
		.name = "AP_DMIC_CH34",
		.id = MT6899_DAI_AP_DMIC_CH34,
		.capture = {
			.stream_name = "AP DMIC CH34 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_ADDA_CAPTURE_RATES,
			.formats = MTK_ADDA_FORMATS,
		},
		.ops = &mtk_dai_adda_ops,
	},
};

int mt6899_dai_adda_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;
	struct mt6899_afe_private *afe_priv = afe->platform_priv;
	int ret;

	dev_info(afe->dev, "%s() successfully start\n", __func__);

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_adda_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_adda_driver);

	dai->controls = mtk_adda_controls;
	dai->num_controls = ARRAY_SIZE(mtk_adda_controls);
	dai->dapm_widgets = mtk_dai_adda_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_adda_widgets);
	dai->dapm_routes = mtk_dai_adda_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_adda_routes);

	/* set dai priv */
	ret = mt6899_dai_set_priv(afe, MT6899_DAI_ADDA,
				  sizeof(struct mtk_afe_adda_priv), NULL);
	if (ret)
		return ret;

	ret = mt6899_dai_set_priv(afe, MT6899_DAI_ADDA_CH34,
				  sizeof(struct mtk_afe_adda_priv), NULL);
	if (ret)
		return ret;

	/* get ap mic type */
	ret = of_property_read_u32(afe->dev->of_node, "mediatek,ap-dmic",
				   &afe_priv->ap_dmic);
	if (ret) {
		dev_info(afe->dev, "%s() failed to read ap_dmic, default not support\n",
			 __func__);
		afe_priv->ap_dmic = 0;
	}

	/* ap dmic priv share with adda */
	afe_priv->dai_priv[MT6899_DAI_AP_DMIC] =
		afe_priv->dai_priv[MT6899_DAI_ADDA];
	afe_priv->dai_priv[MT6899_DAI_AP_DMIC_CH34] =
		afe_priv->dai_priv[MT6899_DAI_ADDA_CH34];

	return 0;
}

