// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/notifier.h>

#include "mtk_ppm_platform.h"
#include "mtk_ppm_internal.h"
#include "mtk_static_power.h"
#if IS_ENABLED(CONFIG_MTK_UNIFIED_POWER)
#include "mtk_unified_power.h"
#endif
#if IS_ENABLED(CONFIG_MTK_LEGACY_THERMAL)
#include "mach/mtk_thermal.h"
#endif

static unsigned int (*mt_cpufreq_get_cur_volt_cb)(unsigned int);
void ppm_cpufreq_get_cur_volt_register(unsigned int (*cb)(unsigned int))
{
	mt_cpufreq_get_cur_volt_cb = cb;
}
EXPORT_SYMBOL(ppm_cpufreq_get_cur_volt_register);

static unsigned int ppm_cpufreq_get_cur_volt_get(int i)
{
	if (!mt_cpufreq_get_cur_volt_cb)
		return 0;
	return mt_cpufreq_get_cur_volt_cb(i);
}

static int (*s_ppm_thermal_cpuL_temp_cb)(void);
static int (*s_ppm_thermal_cpuB_temp_cb)(void);
void mt_ppm_thermal_get_cpu_cluster_temp_cb(
	int (*ppm_thermal_cpuL_temp_cb)(void), int (*ppm_thermal_cpuB_temp_cb)(void))
{
	s_ppm_thermal_cpuL_temp_cb = ppm_thermal_cpuL_temp_cb;
	s_ppm_thermal_cpuB_temp_cb = ppm_thermal_cpuB_temp_cb;
	ppm_info("%s registered\n", __func__);
}
EXPORT_SYMBOL(mt_ppm_thermal_get_cpu_cluster_temp_cb);

#ifdef PPM_SSPM_SUPPORT
static void *online_core;
#endif
static void ppm_get_cluster_status(struct ppm_cluster_status *cl_status)
{
	struct cpumask cluster_cpu, online_cpu;
	int i;

	for_each_ppm_clusters(i) {
		ppm_get_cl_cpus(&cluster_cpu, i);
		cpumask_and(&online_cpu, &cluster_cpu, cpu_online_mask);

		cl_status[i].core_num = cpumask_weight(&online_cpu);
		cl_status[i].volt = ppm_cpufreq_get_cur_volt_get(i) / 100;
		cl_status[i].freq_idx =
			ppm_main_freq_to_idx(i,
					ppm_cpufreq_get_cur_phy_freq_no_lock_get(i),
					CPUFREQ_RELATION_L);
	}
}

#if IS_ENABLED(CONFIG_CPU_FREQ)
static int ppm_cpu_freq_callback(struct notifier_block *nb,
			unsigned long val, void *data)
{
	struct ppm_cluster_status cl_status[NR_PPM_CLUSTERS] = { {0} };
	struct cpufreq_freqs *freq = data;
	int cpu = freq->policy->cpu;
	int i, is_root_cpu = 0;

	if (freq->flags & CPUFREQ_CONST_LOOPS)
		return NOTIFY_OK;

	/* we only need to notify DLPT when root cpu's freq is changed */
	for_each_ppm_clusters(i) {
		if (cpu == ppm_main_info.cluster_info[i].cpu_id) {
			is_root_cpu = 1;
			break;
		}
	}
	if (!is_root_cpu)
		return NOTIFY_OK;

	switch (val) {
	case CPUFREQ_POSTCHANGE:
		ppm_dbg(DLPT, "%s: POSTCHANGE!, cpu = %d\n", __func__, cpu);
		ppm_get_cluster_status(cl_status);
		mt_ppm_dlpt_kick_PBM(cl_status, ppm_main_info.cluster_num);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block ppm_cpu_freq_notifier = {
	.notifier_call = ppm_cpu_freq_callback,
};
#endif

static int ppm_cpu_dead(unsigned int cpu)
{
	struct ppm_cluster_status cl_status[NR_PPM_CLUSTERS] = { {0} };
#ifdef PPM_SSPM_SUPPORT
	int i = 0;
#endif

	ppm_dbg(DLPT, "action = %s\n", __func__);
	ppm_get_cluster_status(cl_status);
#ifdef PPM_SSPM_SUPPORT
	for_each_ppm_clusters(i)
		mt_reg_sync_writel(cl_status[i].core_num, online_core + 4 * i);
#endif
	mt_ppm_dlpt_kick_PBM(cl_status, ppm_main_info.cluster_num);

	return 0;
}

static int ppm_cpu_up(unsigned int cpu)
{
	struct ppm_cluster_status cl_status[NR_PPM_CLUSTERS] = { {0} };
#ifdef PPM_SSPM_SUPPORT
	int i = 0;
#endif

	ppm_dbg(DLPT, "action = %s\n", __func__);
	ppm_get_cluster_status(cl_status);
#ifdef PPM_SSPM_SUPPORT
	for_each_ppm_clusters(i)
		mt_reg_sync_writel(cl_status[i].core_num, online_core + 4 * i);
#endif
	mt_ppm_dlpt_kick_PBM(cl_status, ppm_main_info.cluster_num);

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_LEGACY_THERMAL)
static unsigned int ppm_get_cpu_temp(enum ppm_cluster cluster)
{
	unsigned int temp = 85;

	switch (cluster) {
	case PPM_CLUSTER_L:
		if (s_ppm_thermal_cpuL_temp_cb)
			temp = s_ppm_thermal_cpuL_temp_cb() / 1000;
		break;
	case PPM_CLUSTER_B:
		if (s_ppm_thermal_cpuB_temp_cb)
			temp = s_ppm_thermal_cpuB_temp_cb() / 1000;
		break;
	default:
		ppm_err("@%s: invalid cluster id = %d\n", __func__, cluster);
		break;
	}

	return temp;
}
#endif

static int ppm_get_spower_devid(enum ppm_cluster cluster)
{
	return 0;
}


int ppm_platform_init(void)
{
#ifdef PPM_SSPM_SUPPORT
	/* map sram to update online core */
	online_core = ioremap_nocache(
		PPM_ONLINE_CORE_SRAM_ADDR, 4 * NR_PPM_CLUSTERS);
	if (!online_core) {
		ppm_err("remap online_core failed!\n");
		WARN_ON(1);
		return -1;
	}
#endif
#if IS_ENABLED(CONFIG_CPU_FREQ)
	cpufreq_register_notifier(
		&ppm_cpu_freq_notifier, CPUFREQ_TRANSITION_NOTIFIER);
#endif
	cpuhp_setup_state_nocalls(CPUHP_BP_PREPARE_DYN,
			"ppm/cpuhp", ppm_cpu_up,
			ppm_cpu_dead);

#if IS_ENABLED(CONFIG_MTK_CPUHOTPLUG)
	ppm_main_info.client_info[PPM_CLIENT_HOTPLUG].name = "HOTPLUG";
	ppm_main_info.client_info[PPM_CLIENT_HOTPLUG].client = PPM_CLIENT_HOTPLUG;
	ppm_main_info.client_info[PPM_CLIENT_HOTPLUG].limit_cb = get_cpuhop_ppm_callback();
#endif

	return 0;
}

void ppm_update_req_by_pwr(struct ppm_policy_req *req)
{
	ppm_cobra_update_limit(req);
}

int ppm_find_pwr_idx(struct ppm_cluster_status *cluster_status)
{
	unsigned int pwr_idx = 0;
	int i;

	if (!cobra_init_done) {
		ppm_warn("@%s: cobra_init_done is 0!\n", __func__);
		return -1; /* wait cobra init */
	}

	for_each_ppm_clusters(i) {
		int core = cluster_status[i].core_num;
		int opp = cluster_status[i].freq_idx;

#if IS_ENABLED(CONFIG_MTK_UNIFIED_POWER)
		if (core > 0 && opp >= 0 && opp < DVFS_OPP_NUM) {
			pwr_idx += cobra_tbl->basic_pwr_tbl
				[CORE_NUM_L*i+core-1][opp].power_idx;
		}
#else
		pwr_idx += 100;
#endif

		ppm_ver("[%d] core = %d, opp = %d\n", i, core, opp);
	}

	if (!pwr_idx) {
		ppm_warn("@%s: pwr_idx is 0!\n", __func__);
		return -1; /* not found */
	}

	ppm_ver("@%s: pwr_idx = %d\n", __func__, pwr_idx);

	return pwr_idx;
}
EXPORT_SYMBOL(ppm_find_pwr_idx);

int ppm_get_min_pwr_idx(void)
{
	struct ppm_cluster_status status[NR_PPM_CLUSTERS];
	int i;

	for_each_ppm_clusters(i) {
		/* use max core for ACAO */
		status[i].core_num = get_cluster_max_cpu_core(i);
		status[i].freq_idx = get_cluster_min_cpufreq_idx(i);
	}

	return ppm_find_pwr_idx(status);
}

int ppm_get_max_pwr_idx(void)
{
	struct ppm_cluster_status status[NR_PPM_CLUSTERS];
	int i;

	for_each_ppm_clusters(i) {
		status[i].core_num = get_cluster_max_cpu_core(i);
		status[i].freq_idx = get_cluster_max_cpufreq_idx(i);
	}

	return ppm_find_pwr_idx(status);
}

unsigned int ppm_calc_total_power(struct ppm_cluster_status *cluster_status,
	unsigned int cluster_num, unsigned int percentage)
{
	unsigned int dynamic, lkg, total, budget = 0;
	int i;
	ktime_t now, delta;

	for (i = 0; i < cluster_num; i++) {
		int core = cluster_status[i].core_num;
		int opp = cluster_status[i].freq_idx;

		if (core != 0 && opp >= 0 && opp < DVFS_OPP_NUM) {
			now = ktime_get();
#if IS_ENABLED(CONFIG_MTK_UNIFIED_POWER)
			dynamic =
				upower_get_power(i, opp, UPOWER_DYN) / 1000;
			lkg =
				mt_ppm_get_leakage_mw((enum ppm_cluster_lkg)i);
			total =
				((((dynamic * 100 + percentage - 1) /
					percentage) + lkg) * core) +
				((upower_get_power(
					i + NR_PPM_CLUSTERS,
					opp,
					UPOWER_DYN) +
				upower_get_power(
					i + NR_PPM_CLUSTERS,
					opp,
					UPOWER_LKG)) / 1000);
#else
			dynamic = 100;
			lkg = 50;
			total = dynamic + lkg;
#endif
			budget += total;
			delta = ktime_sub(ktime_get(), now);

			ppm_dbg(DLPT,
				"(%d):OPP/V/core/Lkg/total = %d/%d/%d/%d/%d(time = %lldus)\n",
				i, opp, cluster_status[i].volt,
				cluster_status[i].core_num,
				lkg, total, ktime_to_us(delta));
		}
	}

	if (!budget) {
		ppm_warn("@%s: pwr_idx is 0!\n", __func__);
		return -1; /* not found */
	}

	ppm_dbg(DLPT, "@%s: total budget = %d\n", __func__, budget);

	return budget;
}

unsigned int mt_ppm_get_leakage_mw(enum ppm_cluster_lkg cluster)
{
	int temp, dev_id, volt;
	unsigned int mw = 0;

	/* read total leakage */
	if (cluster >= TOTAL_CLUSTER_LKG) {
		struct ppm_cluster_status cl_status[NR_PPM_CLUSTERS] = { {0} };
		int i = 0;

		ppm_get_cluster_status(cl_status);

		for_each_ppm_clusters(i) {
			if (!cl_status[i].core_num)
				continue;
#if IS_ENABLED(CONFIG_MTK_LEGACY_THERMAL)
			temp = ppm_get_cpu_temp((enum ppm_cluster)i);
#else
			temp = 85;
#endif
			volt = ppm_cpufreq_get_cur_volt_get(i) / 100;
			dev_id = ppm_get_spower_devid((enum ppm_cluster)i);
			if (dev_id < 0)
				return 0;

			mw += mt_spower_get_leakage(dev_id, volt, temp);
		}
	} else {
#if IS_ENABLED(CONFIG_MTK_LEGACY_THERMAL)
		temp = ppm_get_cpu_temp((enum ppm_cluster)cluster);
#else
		temp = 85;
#endif
		volt = ppm_cpufreq_get_cur_volt_get(cluster) / 100;
		dev_id = ppm_get_spower_devid((enum ppm_cluster)cluster);
		if (dev_id < 0)
			return 0;

		mw = mt_spower_get_leakage(dev_id, volt, temp);
	}

	return mw;
}
