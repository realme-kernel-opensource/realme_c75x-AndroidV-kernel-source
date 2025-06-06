// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
#include <mtk_qos_ipi.h>
#endif /* CONFIG_MTK_QOS_FRAMEWORK */
#include <mt-plat/mtk_gpu_utility.h>
#include <mtk_gpufreq.h>
#include "gpu_bm.h"

struct v1_data *gpu_info_buf;
static int gpu_bm_inited;
static int g_mode_sport_flag;
static int g_mode_hmlp_flag;
static DEFINE_MUTEX(g_GPU_BM_lock);
static unsigned int g_mode;
static unsigned int g_value;
static unsigned int idx, min_idx, high_idx, low_idx;
#if defined(CONFIG_MTK_GPUFREQ_V2)
static unsigned int idx_freq, min_freq, high_freq, low_freq, peak_perf_limit_freq;
static int gpu_bm_freq_inited;
#endif
static int gpu_bm_idx_inited;
static int gpu_opp_num;
static int gpu_opp_high;

static void _mgq_proc_show_v1(struct seq_file *m)
{
	seq_printf(m, "ctx: \t%d\n", gpu_info_buf->ctx);
	seq_printf(m, "frame: \t%d\n", gpu_info_buf->frame);
	seq_printf(m, "job: \t%d\n", gpu_info_buf->job);
	seq_printf(m, "freq: \t%d\n", gpu_info_buf->freq);
	seq_printf(m, "mode: \t%d\n", g_mode);
	seq_printf(m, "value: \t%d\n", g_value);
	//seq_printf(m, "bw: \t0x%x\n", readl(gpu_info_buf + 5));
	//seq_printf(m, "pbw: \t0x%x\n", readl(gpu_info_buf + 6));
}

static int _mgq_proc_show(struct seq_file *m, void *v)
{
	if (gpu_info_buf) {
		unsigned int version = readl(gpu_info_buf + 0);

		seq_printf(m, "version: %d\n", version);
		if (version == 1)
			_mgq_proc_show_v1(m);
		else
			seq_printf(m, "unknown version: 0x%x\n", version);
	} else
		seq_puts(m, "gpu_info_buf == null\n");

	return 0;
}

static int _mgq_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, _mgq_proc_show, NULL);
}

static ssize_t
_mgq_proc_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *f_pos)
{
	int mode = -1;
	char buf[32];
	unsigned int len = 0;

	len = (count < (sizeof(buf) - 1)) ? count : (sizeof(buf) - 1);
	if (copy_from_user(buf, buffer, len))
		return 0;
	buf[len] = '\0';

	/* 0         : default bw prediction.
	 * 1         : sport mode specialized
	 * 2         : no bw prediction
	 * 10-300    : apply a ratio for bw predict output
	 * 2010-2300 : apply a ratio for no predict output
	 * 6010-6300 : apply a ratio for mid-low for gpu scernaio bw predict output
	 */

	mutex_lock(&g_GPU_BM_lock);

	if (!kstrtoint(buf, 10, &mode)) {
		pr_info("@%s: mode: %d\n", __func__, mode);

		if (mode == GPU_BW_SPORT_MODE) {
			g_mode_sport_flag = GPU_BW_SPORT_MODE;
			g_value = 0;
		} else if (mode >= GPU_BW_MLP_RATIO_FLOOR && mode <= GPU_BW_MLP_RATIO_CEIL) {
			g_mode_hmlp_flag = GPU_BW_MLP_MODE;
			g_value = mode;
		} else if (mode == GPU_BW_DEFAULT_MODE) {
			g_mode = GPU_BW_DEFAULT_MODE;
			g_value = 0;
		} else if (mode >= GPU_BW_RATIO_FLOOR && mode <= GPU_BW_RATIO_CEIL) {
			g_mode = GPU_BW_DEFAULT_MODE;
			g_value = mode;
		} else if (mode == GPU_BW_NO_PRED_MODE)
			g_mode = GPU_BW_NO_PRED_MODE;
		else if (mode >= GPU_BW_NO_PRED_RATIO_FLOOR && mode <= GPU_BW_NO_PRED_RATIO_CEIL) {
			g_mode = GPU_BW_NO_PRED_MODE;
			g_value = mode;
		} else
			pr_info("@%s: wrong input: %d\n", __func__, mode);
	}

	if (mode != 1)
		g_mode_sport_flag = 0;

	if (mode < GPU_BW_MLP_RATIO_FLOOR || mode > GPU_BW_MLP_RATIO_CEIL)
		g_mode_hmlp_flag = 0;

	if (g_value != 0)
		gpu_info_buf->freq = g_value;
	else
		gpu_info_buf->freq = g_mode;

	mutex_unlock(&g_GPU_BM_lock);

	return count;
}

static const struct proc_ops _mgq_proc_fops = {
	.proc_open = _mgq_proc_open,
	.proc_read = seq_read,
	.proc_write = _mgq_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int _MTKGPUQoS_initDebugFS(void)
{
	struct proc_dir_entry *dir = NULL;

	dir = proc_mkdir("mgq", NULL);
	if (!dir) {
		pr_debug("@%s: create /proc/mgq failed\n", __func__);
		return -ENOMEM;
	}

	if (!proc_create("job_status", 0660, dir, &_mgq_proc_fops))
		pr_debug("@%s: create /proc/mgq/job_status failed\n", __func__);

	return 0;
}

static struct setupfw_t setupfw_data;
static void setupfw_work_handler(struct work_struct *work);
static DECLARE_DELAYED_WORK(g_setupfw_work, setupfw_work_handler);

static void setupfw_work_handler(struct work_struct *work)
{
#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
	struct qos_ipi_data qos_d;
	int ret = -1;

	qos_d.cmd = QOS_IPI_SETUP_GPU_INFO;
	qos_d.u.gpu_info.addr = (unsigned int)setupfw_data.phyaddr;
#if IS_ENABLED(CONFIG_ARM64)
	qos_d.u.gpu_info.addr_hi = (unsigned int)(setupfw_data.phyaddr >> 32);
#else
	qos_d.u.gpu_info.addr_hi = (unsigned int)(setupfw_data.phyaddr);
#endif /* CONFIG_ARM64 */
	qos_d.u.gpu_info.size = (unsigned int)setupfw_data.size;

#ifdef MTK_SCMI
	ret = qos_ipi_to_sspm_scmi_command(qos_d.cmd,
		qos_d.u.gpu_info.addr,
		qos_d.u.gpu_info.addr_hi,
		qos_d.u.gpu_info.size, QOS_IPI_SCMI_SET);

	if (ret)
		pr_info("%s: sspm_ipi_to_scmi fail (%d)\n", __func__, ret);
	else
		pr_info("%s: sspm_ipi_to_scmi success! (%d)\n", __func__, ret);

#else
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	ret = qos_ipi_to_sspm_command(&qos_d, 4);

	if (ret == 1)
		pr_info("%s: sspm_ipi success! (%d)\n", __func__, ret);
	else
		pr_info("%s: sspm_ipi fail (%d)\n", __func__, ret);
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
#endif /* MTK_SCMI */
	gpu_bm_inited = 1;
	pr_debug("%s: addr:0x%x, addr_hi:0x%x, ret:%d\n",
		__func__,
		qos_d.u.gpu_info.addr,
		qos_d.u.gpu_info.addr_hi,
		ret);

#else
	pr_debug("%s: sspm_ipi is not support!\n", __func__);
#endif /* MTK_QOS_FRAMEWORK */
}

static void _MTKGPUQoS_setupFW(phys_addr_t phyaddr, size_t size)
{

	setupfw_data.phyaddr = phyaddr;
	setupfw_data.size = size;

	schedule_delayed_work(&g_setupfw_work, 1);
}

void MTKGPUQoS_mode(int seg_flag)
{
	unsigned int loading = 0;

	mtk_get_gpu_loading(&loading);

#if defined(CONFIG_MTK_GPUFREQ_V2)
	idx_freq = gpufreq_get_cur_out_freq(TARGET_DEFAULT);
	idx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);

	if (!gpu_bm_idx_inited) {
		gpu_opp_num = gpufreq_get_opp_num(TARGET_DEFAULT);
		min_idx = gpu_opp_num - 1;
		if (gpu_opp_high != -1)
			high_idx = gpu_opp_high;
		else
			high_idx = (gpu_opp_num - 1) / 4 + 1;
		low_idx = (gpu_opp_num - 1) / 3 * 2 + 1;
		gpu_bm_idx_inited = 1;
	}

	if (!gpu_bm_freq_inited && idx_freq > 0) {
		min_freq = gpufreq_get_freq_by_idx(TARGET_DEFAULT, min_idx);
		high_freq = gpufreq_get_freq_by_idx(TARGET_DEFAULT, high_idx);
		low_freq = gpufreq_get_freq_by_idx(TARGET_DEFAULT, low_idx);
		peak_perf_limit_freq = gpufreq_get_freq_by_idx(TARGET_DEFAULT, GPU_BM_PEAK_INDEX_TOP_LIMIT);
		gpu_bm_freq_inited = 1;
	}
#else
	idx = mt_gpufreq_get_cur_freq_index();

	if (!gpu_bm_idx_inited) {
		gpu_opp_num = mt_gpufreq_get_dvfs_table_num();
		min_idx = gpu_opp_num - 1;
		if (gpu_opp_high != -1)
			high_idx = gpu_opp_high;
		else
			high_idx = (gpu_opp_num - 1) / 4 + 1;
		low_idx = (gpu_opp_num - 1) / 3 * 2 + 1;
		gpu_bm_idx_inited = 1;
	}
#endif

	/* sport mode */
	if (g_mode_sport_flag) {
		/* if gpu freq at top quartile, boost dram freq. */
#if defined(CONFIG_MTK_GPUFREQ_V2)
		if(gpu_bm_freq_inited) {
			if (idx_freq >= high_freq) {
				if (g_mode == GPU_BW_DEFAULT_MODE)
					gpu_info_buf->freq = GPU_BW_RATIO_CEIL;
				else if (g_mode == GPU_BW_NO_PRED_MODE)
					gpu_info_buf->freq = GPU_BW_RATIO_CEIL + 2000;
			} else
				if (g_mode == GPU_BW_DEFAULT_MODE)
					gpu_info_buf->freq = 0;
				else if (g_mode == GPU_BW_NO_PRED_MODE)
					gpu_info_buf->freq = 2000;
		} else {
			if (idx <= high_idx) {
				if (g_mode == GPU_BW_DEFAULT_MODE)
					gpu_info_buf->freq = GPU_BW_RATIO_CEIL;
				else if (g_mode == GPU_BW_NO_PRED_MODE)
					gpu_info_buf->freq = GPU_BW_RATIO_CEIL + 2000;
			} else
				if (g_mode == GPU_BW_DEFAULT_MODE)
					gpu_info_buf->freq = 0;
				else if (g_mode == GPU_BW_NO_PRED_MODE)
					gpu_info_buf->freq = 2000;
		}
		return;
#else
		if (idx <= high_idx) {
			if (g_mode == GPU_BW_DEFAULT_MODE)
				gpu_info_buf->freq = GPU_BW_RATIO_CEIL;
			else if (g_mode == GPU_BW_NO_PRED_MODE)
				gpu_info_buf->freq = GPU_BW_RATIO_CEIL + 2000;
		} else
			if (g_mode == GPU_BW_DEFAULT_MODE)
				gpu_info_buf->freq = 0;
			else if (g_mode == GPU_BW_NO_PRED_MODE)
				gpu_info_buf->freq = 2000;
		return;
#endif
	}

	/* default avg prediction */
	if (g_mode == GPU_BW_DEFAULT_MODE) {
		/*
		 * if gpu loading < 40% and gpu freq is lowest,
		 * don't do GPU QoS prediction.
		 */
#if defined(CONFIG_MTK_GPUFREQ_V2)
		if(gpu_bm_freq_inited) {
			if ((idx_freq <= min_freq) && (loading < 40))
				gpu_info_buf->freq = GPU_BW_NO_PRED_MODE;
			else {
				if (seg_flag && idx_freq <= low_freq)
					gpu_info_buf->freq = GPU_BW_LP_MODE;
				else if (idx_freq >= peak_perf_limit_freq)
					gpu_info_buf->freq = GPU_BM_PEAK_PERF_MODE_LIMIT;
				else if (idx_freq >= high_freq)
					gpu_info_buf->freq = GPU_BM_PEAK_PERF_MODE;
				else if (g_mode_hmlp_flag && idx_freq < high_freq)
					gpu_info_buf->freq = g_value;
				else
					gpu_info_buf->freq = 0;

				if (g_value >= GPU_BW_RATIO_FLOOR && g_value <= GPU_BW_RATIO_CEIL)
					/* apply a ratio for bw prediction */
					gpu_info_buf->freq = g_value;
			}
		} else {
			if ((idx == min_idx) && (loading < 40))
				gpu_info_buf->freq = GPU_BW_NO_PRED_MODE;
			else {
				if (seg_flag && idx >= low_idx)
					gpu_info_buf->freq = GPU_BW_LP_MODE;
				else if (idx <= GPU_BM_PEAK_INDEX_TOP_LIMIT)
					gpu_info_buf->freq = GPU_BM_PEAK_PERF_MODE_LIMIT;
				else if (idx <= high_idx)
					gpu_info_buf->freq = GPU_BM_PEAK_PERF_MODE;
				else if (g_mode_hmlp_flag && idx > high_idx)
					gpu_info_buf->freq = g_value;
				else
					gpu_info_buf->freq = 0;

				if (g_value >= GPU_BW_RATIO_FLOOR && g_value <= GPU_BW_RATIO_CEIL)
					/* apply a ratio for bw prediction */
					gpu_info_buf->freq = g_value;
			}
		}
#else
		if ((idx == min_idx) && (loading < 40))
			gpu_info_buf->freq = GPU_BW_NO_PRED_MODE;
		else {
			if (seg_flag && idx >= low_idx)
				gpu_info_buf->freq = GPU_BW_LP_MODE;
			else if (idx <= GPU_BM_PEAK_INDEX_TOP_LIMIT)
				gpu_info_buf->freq = GPU_BM_PEAK_PERF_MODE_LIMIT;
			else if (idx <= high_idx)
				gpu_info_buf->freq = GPU_BM_PEAK_PERF_MODE;
			else if (g_mode_hmlp_flag && idx > high_idx)
				gpu_info_buf->freq = g_value;
			else
				gpu_info_buf->freq = 0;

			if (g_value >= GPU_BW_RATIO_FLOOR && g_value <= GPU_BW_RATIO_CEIL)
				/* apply a ratio for bw prediction */
				gpu_info_buf->freq = g_value;
		}
#endif
		return;
	}

	/* no bw prediction */
	if (g_mode == GPU_BW_NO_PRED_MODE) {
		gpu_info_buf->freq = GPU_BW_NO_PRED_MODE;
		if (g_value >= GPU_BW_NO_PRED_RATIO_FLOOR && g_value <= GPU_BW_NO_PRED_RATIO_CEIL)
			/* apply a ratio for bw prediction */
			gpu_info_buf->freq = g_value;
		return;
	}

}
EXPORT_SYMBOL(MTKGPUQoS_mode);

void MTKGPUQoS_mode_ratio(int mode)
{
	/* 0         : default bw prediction.
	 * 1         : sport mode specialized
	 * 2         : no bw prediction
	 * 10-300    : apply a ratio for bw predict output
	 * 2010-2300 : apply a ratio for no predict output
	 * 6010-6300 : apply a ratio for mid-low for gpu scernaio bw predict output
	 */

	mutex_lock(&g_GPU_BM_lock);

	pr_info("@%s: mode: %d\n", __func__, mode);

	if (mode == GPU_BW_SPORT_MODE) {
		g_mode_sport_flag = GPU_BW_SPORT_MODE;
		g_value = 0;
	} else if (mode >= GPU_BW_MLP_RATIO_FLOOR && mode <= GPU_BW_MLP_RATIO_CEIL) {
		g_mode_hmlp_flag = GPU_BW_MLP_MODE;
		g_value = mode;
	} else if (mode == GPU_BW_DEFAULT_MODE) {
		g_mode = GPU_BW_DEFAULT_MODE;
		g_value = 0;
	} else if (mode >= GPU_BW_RATIO_FLOOR && mode <= GPU_BW_RATIO_CEIL) {
		g_mode = GPU_BW_DEFAULT_MODE;
		g_value = mode;
	} else if (mode == GPU_BW_NO_PRED_MODE)
		g_mode = GPU_BW_NO_PRED_MODE;
	else if (mode >= GPU_BW_NO_PRED_RATIO_FLOOR && mode <= GPU_BW_NO_PRED_RATIO_CEIL) {
		g_mode = GPU_BW_NO_PRED_MODE;
		g_value = mode;
	} else
		pr_info("@%s: wrong input: %d\n", __func__, mode);

	if (mode != 1)
		g_mode_sport_flag = 0;

	if (mode < GPU_BW_MLP_RATIO_FLOOR || mode > GPU_BW_MLP_RATIO_CEIL)
		g_mode_hmlp_flag = 0;

	if (g_value != 0)
		gpu_info_buf->freq = g_value;
	else
		gpu_info_buf->freq = g_mode;

	mutex_unlock(&g_GPU_BM_lock);
}
EXPORT_SYMBOL(MTKGPUQoS_mode_ratio);

static void bw_v1_gpu_power_change_notify(int power_on)
{
	static int ctx;

	if (!power_on) {
		ctx = gpu_info_buf->ctx;
		gpu_info_buf->ctx = 0; // ctx
		return;
	}
	gpu_info_buf->ctx = ctx;

	MTKGPUQoS_mode(0);
}

static void _MTKGPUQoS_init(void)
{
	struct device_node *gpu_bm_node = NULL;
	bool enable_mode = false;

	/* get GPU QoS pre-defined device node from dts */
	gpu_bm_node = of_find_compatible_node(NULL, NULL, "mediatek,gpu_qos");
	if (unlikely(!gpu_bm_node))
		pr_info("@%s: Failed to find gpu_qos node\n", __func__);
	else {
		/* check and get qos mode & value by getting qos-mode & qos-value property */
		enable_mode = of_property_read_bool(gpu_bm_node, "qos-mode");
		if (likely(enable_mode)) {
			of_property_read_u32(gpu_bm_node, "qos-mode", &g_mode);
			of_property_read_u32(gpu_bm_node, "qos-value", &g_value);
			pr_info("@%s: g_mode: %d, g_value: %d\n", __func__, g_mode, g_value);
			if (g_value >= GPU_BW_RATIO_FLOOR && g_value <= GPU_BW_RATIO_CEIL) {
				if (g_mode == GPU_BW_DEFAULT_MODE)
					gpu_info_buf->freq = g_value;
				else if (g_mode == GPU_BW_NO_PRED_MODE)
					gpu_info_buf->freq = g_value + 2000;
			} else {
				if (g_mode == GPU_BW_DEFAULT_MODE)
					gpu_info_buf->freq = g_mode;
				else if (g_mode == GPU_BW_NO_PRED_MODE)
					gpu_info_buf->freq = g_mode;
			}
			of_property_read_u32(gpu_bm_node, "qos-opp-high", &gpu_opp_high);
			if (!gpu_opp_high)
				gpu_opp_high = -1;
			pr_info("@%s: qos-opp-high: %d\n", __func__, gpu_opp_high);
		}

	}
}

void MTKGPUQoS_setup(struct v1_data *v1, phys_addr_t phyaddr, size_t size)
{
	gpu_info_buf = v1;
	idx = min_idx = high_idx = low_idx = -1;
	gpu_bm_idx_inited = 0;
	g_mode_sport_flag = 0;
	g_mode_hmlp_flag = 0;
#if defined(CONFIG_MTK_GPUFREQ_V2)
	idx_freq = min_freq = high_freq = low_freq = peak_perf_limit_freq = -1;
	gpu_bm_freq_inited = 0;
#endif

	_MTKGPUQoS_init();
	_MTKGPUQoS_initDebugFS();
	_MTKGPUQoS_setupFW(phyaddr, size);

	mtk_register_gpu_power_change("qpu_qos", bw_v1_gpu_power_change_notify);
}
EXPORT_SYMBOL(MTKGPUQoS_setup);

int MTKGPUQoS_is_inited(void)
{
	return gpu_bm_inited;
}
EXPORT_SYMBOL(MTKGPUQoS_is_inited);

uint32_t MTKGPUQoS_getBW(uint32_t offset)
{
	return 0;
}
EXPORT_SYMBOL(MTKGPUQoS_getBW);

static int __init mtk_gpu_qos_init(void)
{
	/*Do Nothing*/
	return 0;
}

static void __exit mtk_gpu_qos_exit(void)
{
	/*Do Nothing*/
	;
}

arch_initcall(mtk_gpu_qos_init);
module_exit(mtk_gpu_qos_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GPU QOS");
MODULE_AUTHOR("MediaTek Inc.");
