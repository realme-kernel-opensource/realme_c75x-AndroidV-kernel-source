// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/types.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_platform.h>
/* local include */
//#include "mtk_cpufreq_api.h"
#include "mtk_unified_power.h"
#include "mtk_unified_power_data.h"
/* #include "mtk_devinfo.h" //NO PORTING */
#include "mtk_static_power.h"


//#ifndef EARLY_PORTING_SPOWER
#include "mtk_common_static_power.h"
//#endif

#undef  BIT
#define BIT(bit)	(1U << (bit))

#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)
/**
 * Genearte a mask wher MSB to LSB are all 0b1
 * @r:	Range in the form of MSB:LSB
 */
#define BITMASK(r)	\
	(((unsigned int) -1 >> (31 - MSB(r))) & ~((1U << LSB(r)) - 1))

/**
 * Set value at MSB:LSB. For example, BITS(7:3, 0x5A)
 * will return a value where bit 3 to bit 7 is 0x5A
 * @r:	Range in the form of MSB:LSB
 */
/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS(r, val)	((val << LSB(r)) & BITMASK(r))

#define GET_BITS_VAL(_bits_, _val_)   \
	(((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))
/* #if (NR_UPOWER_TBL_LIST <= 1) */
struct upower_tbl final_upower_tbl[NR_UPOWER_BANK] = {};
/* #endif */

int degree_set[NR_UPOWER_DEGREE] = {
		UPOWER_DEGREE_0,
		UPOWER_DEGREE_1,
		UPOWER_DEGREE_2,
		UPOWER_DEGREE_3,
		UPOWER_DEGREE_4,
		UPOWER_DEGREE_5,
};

static unsigned int _mt_cpufreq_get_cpu_level(void)
{
	unsigned int lv = 0;
	unsigned int efuse_seg;
	struct device_node *node;
	struct nvmem_cell *efuse_cell;
	size_t efuse_len;
	unsigned int *efuse_buf;


	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6761-dvfsp");

	if (!node) {
		pr_info("%s fail to get device node\n", __func__);
		return 0;
	}

	efuse_cell = of_nvmem_cell_get(node, "efuse_segment_cell");
	if (IS_ERR(efuse_cell)) {
		pr_info("@%s: cannot get efuse_cell\n", __func__);
		return PTR_ERR(efuse_cell);
	}

	efuse_buf = (unsigned int *)nvmem_cell_read(efuse_cell, &efuse_len);
	nvmem_cell_put(efuse_cell);
	efuse_seg = *efuse_buf;
	kfree(efuse_buf);


#if IS_ENABLED(CONFIG_MTK_LP_OPP)
		lv = 0;
#endif

	if ((efuse_seg == 0x10) || (efuse_seg == 0x11) || (efuse_seg == 0x90)
			|| (efuse_seg == 0x91))
		lv = 3;

	return lv;
}

/* collect all the raw tables */
#define INIT_UPOWER_TBL_INFOS(name, tbl) {__stringify(name), &tbl}
struct upower_tbl_info upower_tbl_list[NR_UPOWER_TBL_LIST][NR_UPOWER_BANK] = {
	/* FY */
	[0] = {
	/*INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_FY), */
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_FY),
	/*INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_FY), */
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_FY),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_FY),
	},
	/* SB */
	[1] = {
	/*INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_SB), */
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_SB),
	/*INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_SB),*/
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_SB),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_SB),
	},

	[2] = {
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_FY2),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_FY2),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_FY2),
	},

	[3] = {
	/*INIT_UPOWER_TBL_INFOS(UPOWER_BANK_L, upower_tbl_l_SB), */
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL, upower_tbl_ll_LITE),
	/*INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_L, upower_tbl_cluster_l_SB),*/
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL, upower_tbl_cluster_ll_LITE),
	INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CCI, upower_tbl_cci_LITE),
	},
	/* C65T */
};
/* Upower will know how to apply voltage that comes from EEM */
unsigned char upower_recognize_by_eem[NR_UPOWER_BANK] = {
	UPOWER_BANK_LL, /* LL EEM apply voltage to LL upower bank */
	UPOWER_BANK_LL, /* LL EEM apply voltage to CLS_LL upower bank */
	UPOWER_BANK_CCI, /* CCI EEM apply voltage to CCI upower bank */
};

/* Used for rcu lock, points to all the raw tables list*/
struct upower_tbl_info *p_upower_tbl_infos = &upower_tbl_list[0][0];

#ifndef EARLY_PORTING_SPOWER
int upower_bank_to_spower_bank(int upower_bank)
{
	int ret;

	switch (upower_bank) {
	case UPOWER_BANK_LL:
		ret = MTK_SPOWER_CPULL;
		break;
	case UPOWER_BANK_CLS_LL:
		ret = MTK_SPOWER_CPULL_CLUSTER;
		break;

	case UPOWER_BANK_CCI:
		ret = MTK_SPOWER_CCI;
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}
#endif

/****************************************************
 * According to chip version get the raw upower tbl *
 * and let upower_tbl_infos points to it.           *
 * Choose a non used upower tbl location and let    *
 * upower_tbl_ref points to it to store target      *
 * power tbl.                                       *
 ***************************************************/

int cpu_cluster_mapping(unsigned int cpu)
{
	enum upower_bank bank = UPOWER_BANK_LL;

	if (cpu < 4) /* cpu 0-3 */
		bank = UPOWER_BANK_LL;
	else if (cpu < 8) /* cpu 4-7 */
		bank = UPOWER_BANK_LL + 1;
	else if (cpu < 10) /* cpu 8-9 */
		bank = UPOWER_BANK_LL + 2;

	return bank;
}

void get_original_table(void)
{
	/* unsigned int bin = 0; */
	unsigned short idx = 0; /* default use FY table */
	unsigned int i, j;

	idx = _mt_cpufreq_get_cpu_level();
	upower_debug("idx = %d", idx);
	/* get location of reference table */
	if (idx >= NR_UPOWER_TBL_LIST)
		idx = 0;
	upower_tbl_infos = &upower_tbl_list[idx][0];

	/* get location of target table */
	upower_tbl_ref = &final_upower_tbl[0];

	upower_debug("idx %d dest:%p, src:%p\n",
		(idx+1)%NR_UPOWER_TBL_LIST, upower_tbl_ref, upower_tbl_infos);

	/* If disable upower, ptr will point to original upower table */
	p_upower_tbl_infos = upower_tbl_infos;

	/*
	 *  Clear volt fields before eem run.
	 *  If eem is enabled, it will apply volt into it. If eem is disabled,
	 *  the values of volt are 0 , and upower will apply orig volt into it
	 */
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		for (j = 0; j < UPOWER_OPP_NUM; j++)
			upower_tbl_ref[i].row[j].volt = 0;
	}
	for (i = 0; i < NR_UPOWER_BANK; i++)
		upower_debug("bank[%d] dest:%p dyn_pwr:%u, volt[0]%u\n",
			i, &upower_tbl_ref[i],
			upower_tbl_ref[i].row[0].dyn_pwr,
			upower_tbl_ref[i].row[0].volt);

}

MODULE_DESCRIPTION("MediaTek Unified Power Driver v0.0");
MODULE_LICENSE("GPL");
