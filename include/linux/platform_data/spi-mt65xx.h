/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  MTK SPI bus driver definitions
 *
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Leilk Liu <leilk.liu@mediatek.com>
 */

#ifndef ____LINUX_PLATFORM_DATA_SPI_MTK_H
#define ____LINUX_PLATFORM_DATA_SPI_MTK_H

extern void mt_irq_dump_status(unsigned int irq);
/* Board specific platform_data */
struct mtk_chip_config {
	u32 sample_sel;

	u32 cs_setuptime;
	u32 cs_holdtime;
	u32 cs_idletime;
	u32 tick_delay;
};
#endif
