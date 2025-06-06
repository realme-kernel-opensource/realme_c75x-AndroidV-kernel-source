// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



/**
 * @file	mtk_eem_platform.c
 * @brief   Driver for EEM
 *
 */
#define __MTK_EEM_PLATFORM_C__

#include <linux/kernel.h>
#include "mtk_eem_config.h"
#include "mtk_eem.h"
#include "mtk_eem_internal_ap.h"
#include "mtk_eem_internal.h"
#include "mtk_cpufreq_api.h"


struct eemsn_det_ops big_det_ops = {
	.get_volt		= get_volt_cpu,
	.get_freq_table		= get_freq_table_cpu,
	.get_orig_volt_table = get_orig_volt_table_cpu,
};

unsigned int detid_to_dvfsid(struct eemsn_det *det)
{
	unsigned int cpudvfsindex;
	enum eemsn_det_id detid = det_to_id(det);

	if (detid == EEMSN_DET_L)
		cpudvfsindex = MT_CPU_DVFS_LL;
	else if (detid == EEMSN_DET_B)
		cpudvfsindex = MT_CPU_DVFS_L;
	else
		cpudvfsindex = MT_CPU_DVFS_CCI;


	eem_debug("[%s] id:%d, cpudvfsindex:%d\n", __func__,
		detid, cpudvfsindex);


	return cpudvfsindex;
}

/* Will return 10uV */
int get_volt_cpu(struct eemsn_det *det)
{
	unsigned int value = 0;
	enum eemsn_det_id cpudvfsindex;

	FUNC_ENTER(FUNC_LV_HELP);
	/* unit mv * 100 = 10uv */
	cpudvfsindex = detid_to_dvfsid(det);

#if IS_ENABLED(CONFIG_MEDIATEK_CPU_DVFS)
#if SET_PMIC_VOLT_TO_DVFS
	value = mt_cpufreq_get_cur_volt(cpudvfsindex);
#endif
#endif

	FUNC_EXIT(FUNC_LV_HELP);
	eem_debug("proc voltage = %d~~~\n", value);
	return value;
}

void get_freq_table_cpu(struct eemsn_det *det)
{
#if IS_ENABLED(CONFIG_MEDIATEK_CPU_DVFS)
#if SET_PMIC_VOLT_TO_DVFS
	int i = 0;
	enum mt_cpu_dvfs_id cpudvfsindex;

	cpudvfsindex = detid_to_dvfsid(det);

	for (i = 0; i < NR_FREQ; i++) {
		det->freq_tbl[i] =
			mt_cpufreq_get_freq_by_idx(cpudvfsindex, i) / 1000;
#if DVFS_DEBUG_LOG
		eem_error("id:%d, idx:%d, freq_tbl=%d, orgfreq:%d,\n",
			det->det_id, i, det->freq_tbl[i],
			mt_cpufreq_get_freq_by_idx(cpudvfsindex, i));
#endif
		if (det->freq_tbl[i] == 0)
			break;
	}

	det->num_freq_tbl = i;
#endif
#endif
}


/* get original volt from cpu dvfs, and apply this table to dvfs
 *   when ptp need to restore volt
 */
void get_orig_volt_table_cpu(struct eemsn_det *det)
{
#if IS_ENABLED(CONFIG_MEDIATEK_CPU_DVFS)
#if SET_PMIC_VOLT_TO_DVFS
	int i = 0, volt = 0;
	enum mt_cpu_dvfs_id cpudvfsindex;

	FUNC_ENTER(FUNC_LV_HELP);
	cpudvfsindex = detid_to_dvfsid(det);

	for (i = 0; i < det->num_freq_tbl; i++) {
		volt = mt_cpufreq_get_volt_by_idx(cpudvfsindex, i);

		det->volt_tbl_orig[i] =
			(unsigned char)base_ops_volt_2_pmic(det, volt);
#if DVFS_DEBUG_LOG
		eem_error("[%s]@@volt_tbl_orig[%d] = %d(0x%x)\n",
			det->name+8,
			i,
			volt,
			det->volt_tbl_orig[i]);
#endif
	}

	FUNC_EXIT(FUNC_LV_HELP);
#endif
#endif
}
/************************************************
 * common det operations for legacy and sspm ptp
 ************************************************
 */
int base_ops_volt_2_pmic(struct eemsn_det *det, int volt)
{
	return (((volt) - det->pmic_base +
		det->pmic_step - 1) / det->pmic_step);
}

int base_ops_volt_2_eem(struct eemsn_det *det, int volt)
{
	return (((volt) - det->eemsn_v_base +
		det->eemsn_step - 1) / det->eemsn_step);
}

int base_ops_pmic_2_volt(struct eemsn_det *det, int pmic_val)
{
	return (((pmic_val) * det->pmic_step) + det->pmic_base);
}

int base_ops_eem_2_pmic(struct eemsn_det *det, int eem_val)
{
	return ((((eem_val) * det->eemsn_step) + det->eemsn_v_base -
			det->pmic_base + det->pmic_step - 1) / det->pmic_step);
}
#undef __MTK_EEM_PLATFORM_C__
