/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_STATIC_POWER_H__
#define __MTK_STATIC_POWER_H__

#include <linux/types.h>

/* #define MTK_SPOWER_UT */
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6765)
#include "mtk_static_power_6765.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_6833)
#include "mtk_static_power_6833.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6739)
#include "mtk_static_power_6739.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6761)
#include "mtk_static_power_6761.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6877)
#include "mtk_static_power_6877.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_6893)
#include "mtk_static_power_6893.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_6781)
#include "mtk_static_power_6781.h"
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_6853)
#include "mtk_static_power_6853.h"
#else
#include "mtk_static_power_plat.h"
#endif


#undef  BIT
#define BIT(bit)	(1U << (bit))

#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)
#define GET_BITS_VAL(_bits_, _val_) \
	(((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))

extern u32 get_devinfo_with_index(u32 index);

/*
 * @argument
 * dev: the enum of MT_SPOWER_xxx
 * voltage: the operating voltage, mV.
 * degree: the Tj. (degree C)
 * @return
 *  -1, means sptab is not yet ready.
 *  other value: the mW of leakage value.
 */
extern int mt_spower_get_leakage(int dev, unsigned int voltage, int degree);
extern int mt_spower_get_efuse_lkg(int dev);

extern int mt_spower_init(void);

#endif
