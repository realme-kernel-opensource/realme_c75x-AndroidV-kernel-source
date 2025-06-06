/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __MTK_SPM_IRQ_EDGE_H__
#define __MTK_SPM_IRQ_EDGE_H__

/*************************************************************
 * Define spm edge trigger interrupt list according to chip.
 * Only included by mtk_spm_irq.c
 ************************************************************/

struct edge_trigger_irq_list {
	const char *name;
	int order;
	unsigned int wakesrc;
};

static struct edge_trigger_irq_list list[] = {
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6761)
	{ "mediatek,mt6761-infracfg",   0,	0 },
#else
	{ "mediatek,mt6765-infracfg",   0,	0 },
#endif
	{ "mediatek,kp",            0,  WAKE_SRC_R12_KP_IRQ_B },
	{ "mediatek,mddriver",       0,  WAKE_SRC_R12_MD1_WDT_B },
	{ "mediatek,dsi0",   0,      WAKE_SRC_R12_SYS_CIRQ_IRQ_B},
};

#endif /* __MTK_SPM_IRQ_EDGE_H__ */
