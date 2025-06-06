/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DRV_CLKCHK_MT6833_H
#define __DRV_CLKCHK_MT6833_H

enum chk_sys_id {
	topckgen = 0,
	infracfg,
	pericfg,
	scpsys,
	apmixedsys,
	audiosys,
	gce,
	mipi_0a,
	mipi_0b,
	mipi_1a,
	mipi_1b,
	mipi_2a,
	mipi_2b,
	mfgsys,
	mmsys,
	imgsys,
	camsys,
	vencsys,
	vdecsys,
	chk_sys_num,
};

extern void print_subsys_reg_mt6768(enum chk_sys_id id);
extern u32 get_mt6768_reg_value(u32 id, u32 ofs);
#endif	/* __DRV_CLKCHK_MT6833_H */

