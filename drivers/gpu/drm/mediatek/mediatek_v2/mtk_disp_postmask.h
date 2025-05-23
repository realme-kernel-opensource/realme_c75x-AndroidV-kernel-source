/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_POSTMASK_H__
#define __MTK_DISP_POSTMASK_H__

struct mtk_disp_postmask_data {
	bool is_support_34bits;
	bool need_bypass_shadow;
};

void mtk_postmask_relay_debug(struct mtk_ddp_comp *comp, unsigned int relay);

#endif
