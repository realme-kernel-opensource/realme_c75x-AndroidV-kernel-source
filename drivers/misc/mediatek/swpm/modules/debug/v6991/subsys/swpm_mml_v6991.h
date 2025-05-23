/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SWPM_MML_V6991_H__
#define __SWPM_MML_V6991_H__

/* power index structure */
struct mml_swpm_data {
	u32 cg_con0;
	u32 cg_con1;
	u32 MML_freq;
	u32 MML_pq;
};

void update_mml_wrot_status (u32 wrot0_sts, u32 wrot1_sts, u32 wrot2_sts, u32 wrot3_sts);
void update_mml_cg_status (u32 cg_con0, u32 cg_con1);
void update_mml_freq_status (u32 freq);
void update_mml_pq_status(u32 pq);

extern int swpm_mml_v6991_init(void);
extern void swpm_mml_v6991_exit(void);
extern struct mml_swpm_func *mml_get_swpm_func(void);

#endif
