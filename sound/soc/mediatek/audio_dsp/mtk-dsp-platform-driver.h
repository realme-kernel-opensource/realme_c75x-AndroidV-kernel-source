/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-dsp-platform.h --  Mediatek ADSP platform
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Chipeng <Chipeng.chang@mediatek.com>
 */


#ifndef _MTK_DSP_PLATFORM_DRIVER_H_
#define _MTK_DSP_PLATFORM_DRIVER_H_
#include <sound/pcm.h>
#include <sound/soc.h>

struct mtk_base_afe;
struct mtk_base_dsp;
struct ipi_msg_t;
#define AFE_DSP_NAME "AUDIO_DSP_PCM"

extern const struct snd_soc_component_driver mtk_dsp_pcm_platform;
extern unsigned int SmartpaSwdspProcessEnable;
extern snd_pcm_uframes_t mtk_dsphw_pcm_pointer
			 (struct snd_soc_component *component,
			  struct snd_pcm_substream *substream);

#endif
