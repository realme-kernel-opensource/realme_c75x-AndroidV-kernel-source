// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <soc/mediatek/smi.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/math64.h>
//#include "smi_public.h"
#include "mtk_vcodec_dec_pm.h"
#include "mtk_vcodec_dec_pm_plat.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"

#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "vcodec_dvfs.h"
#include "vdec_drv_if.h"
#define STD_VDEC_FREQ 218000000
#define VDEC_ADAPTIVE_OPRATE_INTERVAL 500
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect.h"
#include "vcodec_bw.h"

//#define VDEC_PRINT_DTS_INFO
#if DEC_EMI_BW
static bool mtk_dec_tput_init(struct mtk_vcodec_dev *dev)
{
	const int op_item_num = 7;
	const int tp_item_num = 4;
	const int bw_item_num = 3;
	struct platform_device *pdev;
	int i, j, ret;
	u32 nmin = 0, nmax = 0, cnt = 0;

	pdev = dev->plat_dev;

	ret = of_property_read_s32(pdev->dev.of_node, "throughput-op-rate-thresh", &nmax);
	if (ret)
		mtk_v4l2_debug(0, "[VDEC] Cannot get op rate thresh, default 0");

	dev->vdec_dvfs_params.per_frame_adjust_op_rate = nmax;
	dev->vdec_dvfs_params.per_frame_adjust = 1;

	ret = of_property_read_u32(pdev->dev.of_node, "throughput-min", &nmin);
	if (ret) {
		nmin = STD_VDEC_FREQ;
		mtk_v4l2_debug(0, "[VDEC] Cannot get min, default %u", nmin);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "throughput-normal-max", &nmax);
	if (ret) {
		nmax = STD_VDEC_FREQ;
		mtk_v4l2_debug(0, "[VDEC] Cannot get normal max, default %u", nmax);
	}

	dev->vdec_dvfs_params.codec_type = MTK_INST_DECODER;
	dev->vdec_dvfs_params.min_freq = nmin;
	dev->vdec_dvfs_params.normal_max_freq = nmax;
	dev->vdec_dvfs_params.allow_oc = 0;

	mtk_v4l2_debug(8, "[VDEC] tput op_th %d, tmin %u, tmax %u",
		dev->vdec_dvfs_params.per_frame_adjust_op_rate,
		dev->vdec_dvfs_params.min_freq,
		dev->vdec_dvfs_params.normal_max_freq);

	/* max operating rate by codec / resolution */
	cnt = of_property_count_u32_elems(pdev->dev.of_node, "max-op-rate-table");
	dev->vdec_op_rate_cnt = cnt / op_item_num;

	mtk_v4l2_debug(8, "[VDEC] max-op-rate table elements %u, %d per line",
			cnt, op_item_num);
	if (!dev->vdec_op_rate_cnt)
		mtk_v4l2_debug(0, "[VDEC] max-op-rate-table not exist");


	dev->vdec_dflt_op_rate = vzalloc(sizeof(struct vcodec_op_rate) * dev->vdec_op_rate_cnt);

	mtk_v4l2_debug(8, "[VDEC] vzalloc %zu x %d res %p",
			sizeof(struct vcodec_op_rate), dev->vdec_op_rate_cnt,
			dev->vdec_dflt_op_rate);

	if (dev->vdec_dflt_op_rate) {
		for (i = 0; i < dev->vdec_op_rate_cnt; i++) {
			ret = of_property_read_u32_index(pdev->dev.of_node, "max-op-rate-table",
					i * op_item_num, &dev->vdec_dflt_op_rate[i].codec_fmt);
			if (ret) {
				mtk_v4l2_debug(0, "[VDEC] Cannot get default op rate codec_fmt");
				return false;
			}

			for (j = 0; j < (op_item_num - 1) / 2; j++) {
				ret = of_property_read_u32_index(pdev->dev.of_node, "max-op-rate-table",
						i * op_item_num + 1 + j * 2,
						(u32 *)&dev->vdec_dflt_op_rate[i].pixel_per_frame[j]);
				if (ret) {
					mtk_v4l2_debug(0, "[VDEC] Cannot get pixel per frame %d %d",
							i, j);
					return false;
				}

				ret = of_property_read_u32_index(pdev->dev.of_node, "max-op-rate-table",
						i * op_item_num + 2 + j * 2,
						(u32 *)&dev->vdec_dflt_op_rate[i].max_op_rate[j]);
				if (ret) {
					mtk_v4l2_debug(0, "[VDEC] Cannot get max_op_rate %d %d",
							i, j);
					return false;
				}
			}
			dev->vdec_dflt_op_rate[i].codec_type = 0;
		}
	} else
		mtk_v4l2_debug(0, "[VDEC] vzalloc vdec_dflt_op_rate table failed");


	/* throughput */
	cnt = of_property_count_u32_elems(pdev->dev.of_node, "throughput-table");
	dev->vdec_tput_cnt = cnt / tp_item_num;

	mtk_v4l2_debug(8, "[VDEC] tput table elements %u, %d per line",
			cnt, tp_item_num);
	if (dev->vdec_tput_cnt > 0) {
		dev->vdec_tput = vzalloc(sizeof(struct vcodec_perf) * dev->vdec_tput_cnt);
		mtk_v4l2_debug(8, "[VDEC] vzalloc %zu x %d res %p",
			sizeof(struct vcodec_perf), dev->vdec_tput_cnt, dev->vdec_tput);
	} else
		mtk_v4l2_debug(0, "[VDEC] throughtput table not exist");

	if (dev->vdec_tput) {
		for (i = 0; i < dev->vdec_tput_cnt; i++) {
			ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
					i * tp_item_num, &dev->vdec_tput[i].codec_fmt);
			if (ret) {
				mtk_v4l2_debug(0, "[VDEC] Cannot get codec_fmt");
				return false;
			}

			ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
					i * tp_item_num + 1, (u32 *)&dev->vdec_tput[i].config);
			if (ret) {
				mtk_v4l2_debug(0, "[VDEC] Cannot get config");
				return false;
			}


			ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
					i * tp_item_num + 2, &dev->vdec_tput[i].cy_per_mb_1);
			if (ret) {
				mtk_v4l2_debug(0, "[VDEC] Cannot get cycle per mb 1");
				return false;
			}

			ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
					i * tp_item_num + 3, &dev->vdec_tput[i].cy_per_mb_2);
			if (ret) {
				mtk_v4l2_debug(0, "[VDEC] Cannot get cycle per mb 2");
				return false;
			}
			dev->vdec_tput[i].codec_type = 0;
		}
	} else
		mtk_v4l2_debug(0, "[VDEC] vzalloc vdec_tput table failed");

	/* bw */
	dev->vdec_larb_cnt = of_property_count_u32_elems(pdev->dev.of_node,
				"bandwidth-table") / bw_item_num;

	if (dev->vdec_larb_cnt > MTK_VDEC_LARB_NUM) {
		mtk_v4l2_debug(0, "[VDEC] vdec port over limit %d > %d",
			dev->vdec_larb_cnt, MTK_VDEC_LARB_NUM);
		dev->vdec_larb_cnt = MTK_VDEC_LARB_NUM;
	}

	if (!dev->vdec_larb_cnt)
		mtk_v4l2_debug(0, "[VDEC] bandwidth table not exist");

	dev->vdec_larb_bw = vzalloc(sizeof(struct vcodec_larb_bw) * dev->vdec_larb_cnt);
	if (dev->vdec_larb_bw) {
		for (i = 0; i < dev->vdec_larb_cnt; i++) {
			ret = of_property_read_u32_index(pdev->dev.of_node, "bandwidth-table",
					i * bw_item_num, &dev->vdec_larb_bw[i].larb_id);
			if (ret) {
				mtk_v4l2_debug(0, "[VDEC] Cannot get base bw");
				return false;
			}

			ret = of_property_read_u32_index(pdev->dev.of_node, "bandwidth-table",
					i * bw_item_num + 1, (u32 *)&dev->vdec_larb_bw[i].larb_type);
			if (ret) {
				mtk_v4l2_debug(0, "[VDEC] Cannot get bw larb type");
				return false;
			}

			ret = of_property_read_u32_index(pdev->dev.of_node, "bandwidth-table",
					i * bw_item_num + 2, &dev->vdec_larb_bw[i].larb_base_bw);
			if (ret) {
				mtk_v4l2_debug(0, "[VDEC] Cannot get base bw");
				return false;
			}
		}
	} else
		mtk_v4l2_debug(0, "[VDEC] vzalloc vdec_larb_bw table failed");

	ret = of_property_read_u32(pdev->dev.of_node, "os-allow-bw",
			&dev->vdec_dvfs_params.os_bw[1]);
	if (ret)
		mtk_v4l2_debug(0, "[VDEC] Not allow overspec");
	else {
		ret = of_property_read_u32(pdev->dev.of_node, "vdec-max-w",
			&dev->vdec_dvfs_params.os_bw[0]);

		mtk_v4l2_debug(0, "[VDEC] vdec-max-w: %d, os-allow-bw: %d",
			ret ? 0 : dev->vdec_dvfs_params.os_bw[0], dev->vdec_dvfs_params.os_bw[1]);
	}

#ifdef VDEC_PRINT_DTS_INFO
	mtk_v4l2_debug(0, "[VDEC] tput_cnt %d, cfg_cnt %d, port_cnt %d",
		dev->vdec_tput_cnt, dev->vdec_cfg_cnt, dev->vdec_port_cnt);

	for (i = 0; i < dev->vdec_op_rate_cnt; i++) {
		mtk_v4l2_debug(0, "[VDEC] oprate fmt %u, %u/%u,%u/%u,%u/%u,%u/%u",
			dev->vdec_dflt_op_rate[i].codec_fmt,
			dev->vdec_dflt_op_rate[i].pixel_per_frame[0],
			dev->vdec_dflt_op_rate[i].max_op_rate[0],
			dev->vdec_dflt_op_rate[i].pixel_per_frame[1],
			dev->vdec_dflt_op_rate[i].max_op_rate[1],
			dev->vdec_dflt_op_rate[i].pixel_per_frame[2],
			dev->vdec_dflt_op_rate[i].max_op_rate[2],
			dev->vdec_dflt_op_rate[i].pixel_per_frame[3],
			dev->vdec_dflt_op_rate[i].max_op_rate[3]);
	}

	for (i = 0; i < dev->vdec_tput_cnt; i++) {
		mtk_v4l2_debug(0, "[VDEC] tput fmt %u, cfg %d, cy1 %u, cy2 %u",
			dev->vdec_tput[i].codec_fmt,
			dev->vdec_tput[i].config,
			dev->vdec_tput[i].cy_per_mb_1,
			dev->vdec_tput[i].cy_per_mb_2);
	}

	for (i = 0; i < dev->vdec_larb_cnt; i++) {
		mtk_v4l2_debug(0, "[VDEC] larb %u type %d, bw %u",
			dev->vdec_larb_bw[i].larb_id
			dev->vdec_larb_bw[i].larb_type,
			dev->vdec_larb_bw[i].larb_base_bw);
	}
#endif
	return true;
}

static void mtk_dec_tput_deinit(struct mtk_vcodec_dev *dev)
{

	if (dev->vdec_dflt_op_rate) {
		vfree(dev->vdec_dflt_op_rate);
		dev->vdec_dflt_op_rate = 0;
	}

	if (dev->vdec_tput) {
		vfree(dev->vdec_tput);
		dev->vdec_tput = 0;
	}

	if (dev->vdec_larb_bw) {
		vfree(dev->vdec_larb_bw);
		dev->vdec_larb_bw = 0;
	}
}
#endif


void mtk_prepare_vdec_dvfs(struct mtk_vcodec_dev *dev)
{
#if DEC_DVFS
	int ret;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0, vdec_req = 0, flag = 0, util_val = 0;
	struct platform_device *pdev = 0;

	pdev = dev->plat_dev;
	INIT_LIST_HEAD(&dev->vdec_dvfs_inst);

	ret = of_property_read_s32(pdev->dev.of_node, "vdec-mmdvfs-in-vcp", &vdec_req);
	if (ret)
		mtk_v4l2_debug(0, "[VDEC] Faile get vdec-mmdvfs-in-vcp, default %d", vdec_req);
	dev->vdec_dvfs_params.mmdvfs_in_vcp = vdec_req;

	vdec_req = 0;
	ret = of_property_read_s32(pdev->dev.of_node, "vdec-mmdvfs-in-adaptive", &vdec_req);
	if (ret)
		mtk_v4l2_debug(0, "[VDEC] no need vdec-mmdvfs-in-adaptive");
	dev->vdec_dvfs_params.mmdvfs_in_adaptive = vdec_req;

	vdec_req = 0;
	ret = of_property_read_s32(pdev->dev.of_node, "vdec-set-bw-in-min-freq", &vdec_req);
	if (ret)
		mtk_v4l2_debug(0, "[VDEC] no need vdec-set-bw-in-min-freq, default %d", vdec_req);
	dev->vdec_dvfs_params.set_bw_in_min_freq = vdec_req;

	ret = of_property_read_s32(pdev->dev.of_node, "vdec-cpu-hint-mode", &flag);
	if (ret) {
		mtk_v4l2_debug(0, "[VDEC] no need vdec-cpu-hint-mode");
		dev->cpu_hint_mode = (1 << MTK_CPU_UNSUPPORT);
	} else
		dev->cpu_hint_mode = flag;

	ret = of_property_read_u32(pdev->dev.of_node, "vdec-clamp-util-val", &util_val);
	if (ret) {
		mtk_v4l2_debug(0, "[VDEC] use default clamp util value 370");
		dev->uclamp_util_val = 370;
	} else
		dev->uclamp_util_val = util_val;

	ret = dev_pm_opp_of_add_table(&dev->plat_dev->dev);
	if (ret < 0) {
		dev->vdec_reg = 0;
		mtk_v4l2_debug(0, "[VDEC] Failed to get opp table (%d)", ret);
		return;
	}

	dev->vdec_reg = devm_regulator_get_optional(&dev->plat_dev->dev,
						"mmdvfs-dvfsrc-vcore");
	if (IS_ERR_OR_NULL(dev->vdec_reg)) {
		mtk_v4l2_debug(0, "[VDEC] Failed to get regulator");
		dev->vdec_reg = 0;
		dev->vdec_mmdvfs_clk = devm_clk_get(&dev->plat_dev->dev, "mmdvfs_clk");
		if (IS_ERR_OR_NULL(dev->vdec_mmdvfs_clk)) {
			mtk_v4l2_debug(0, "[VDEC] Failed to get mmdvfs_clk");
			dev->vdec_mmdvfs_clk = 0;
		} else
			mtk_v4l2_debug(0, "[VDEC] get vdec_mmdvfs_clk successfully");
	} else {
		mtk_v4l2_debug(0, "[VDEC] get regulator successfully");
	}

	dev->vdec_freq_cnt = dev_pm_opp_get_opp_count(&dev->plat_dev->dev);
	freq = 0;
	while (!IS_ERR(opp =
		dev_pm_opp_find_freq_ceil(&dev->plat_dev->dev, &freq))) {
		dev->vdec_freqs[i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}
	mtk_dec_tput_init(dev);
#endif
}

void mtk_unprepare_vdec_dvfs(struct mtk_vcodec_dev *dev)
{
#if DEC_EMI_BW
	/* Set to lowest clock before leaving */
	mtk_dec_tput_deinit(dev);
#endif
}

void mtk_prepare_vdec_emi_bw(struct mtk_vcodec_dev *dev)
{
#if DEC_EMI_BW
	int i, ret;
	struct platform_device *pdev = 0;
	u32 larb_num = 0;
	const char *path_strs[32];

	pdev = dev->plat_dev;
	for (i = 0; i < MTK_VDEC_LARB_NUM; i++)
		dev->vdec_qos_req[i] = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "interconnect-num", &larb_num);
	if (ret) {
		mtk_v4l2_debug(0, "[VDEC] Cannot get interconnect num, skip");
		return;
	}

	ret = of_property_read_string_array(pdev->dev.of_node, "interconnect-names",
						path_strs, larb_num);

	if (ret < 0) {
		mtk_v4l2_debug(0, "[VDEC] Cannot get interconnect names, skip");
		return;
	} else if (ret != (int)larb_num) {
		mtk_v4l2_debug(0, "[VDEC] Interconnect name count not match %u %d",
			larb_num, ret);
	}

	for (i = 0; i < larb_num; i++) {
		dev->vdec_qos_req[i] = of_mtk_icc_get(&pdev->dev, path_strs[i]);
		mtk_v4l2_debug(16, "[VDEC] qos larb[%d] name %s", i, path_strs[i]);
	}
#endif
}

void mtk_unprepare_vdec_emi_bw(struct mtk_vcodec_dev *dev)
{
#if DEC_EMI_BW
#endif
}

void set_vdec_opp(struct mtk_vcodec_dev *dev, u32 freq)
{
#if DEC_DVFS
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;
	unsigned long freq_64 = (unsigned long)freq;

	if (dev->vdec_reg || dev->vdec_mmdvfs_clk) {
		opp = dev_pm_opp_find_freq_ceil(&dev->plat_dev->dev, &freq_64);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		if (dev->vdec_mmdvfs_clk) {
			ret = clk_set_rate(dev->vdec_mmdvfs_clk, freq_64);
			if (ret) {
				mtk_v4l2_err("[VDEC] Failed to set mmdvfs rate %lu\n",
						freq_64);
			}
			mtk_v4l2_debug(8, "[VDEC] freq %u, find_freq %lu", freq, freq_64);
		} else if (dev->vdec_reg) {
			ret = regulator_set_voltage(dev->vdec_reg, volt, INT_MAX);
			if (ret) {
				mtk_v4l2_err("[VDEC] Failed to set regulator voltage %d\n",
						volt);
			}
			mtk_v4l2_debug(8, "[VDEC] freq %u, voltage %d", freq, volt);
		}
	}
#endif
}

void mtk_vdec_dvfs_sync_vsi_data(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_vcodec_dev *dev = ctx->dev;
	struct vdec_inst *inst = (struct vdec_inst *) ctx->drv_handle;

	if (mtk_vcodec_is_state(ctx, MTK_STATE_ABORT) || inst == (void *) 0)
		return;

	if (IS_ERR_OR_NULL(inst) || IS_ERR_OR_NULL(inst->vsi)) {
		mtk_v4l2_err("[VDVFS][%d] inst/vsi is err or null", ctx->id);
		return;
	}

	dev->vdec_dvfs_params.target_freq = inst->vsi->target_freq;
	ctx->dec_params.operating_rate = inst->vsi->op_rate;
	mtk_vcodec_cpu_adaptive_ctrl(ctx, inst->vsi->cpu_hint);
}

void mtk_vdec_dvfs_begin_inst(struct mtk_vcodec_ctx *ctx)
{
	mtk_v4l2_debug(8, "[VDEC] ctx = %p",  ctx);

	if (need_update(ctx)) {
		update_freq(ctx->dev, MTK_INST_DECODER);
		mtk_v4l2_debug(4, "[VDEC] freq %u", ctx->dev->vdec_dvfs_params.target_freq);
		set_vdec_opp(ctx->dev, ctx->dev->vdec_dvfs_params.target_freq);
	}
}

void mtk_vdec_dvfs_end_inst(struct mtk_vcodec_ctx *ctx)
{
	mtk_v4l2_debug(8, "[VDEC] ctx = %p",  ctx);

	if (remove_update(ctx)) {
		update_freq(ctx->dev, MTK_INST_DECODER);
		mtk_v4l2_debug(4, "[VDEC] freq %u", ctx->dev->vdec_dvfs_params.target_freq);
		set_vdec_opp(ctx->dev, ctx->dev->vdec_dvfs_params.target_freq);
	}
}

void mtk_vdec_pmqos_begin_inst(struct mtk_vcodec_ctx *ctx)
{
	int i;
	struct mtk_vcodec_dev *dev = 0;
	u32 target_bw = 0;

	dev = ctx->dev;
	mtk_v4l2_debug(8, "[VDVFS][VDEC] ctx = %p",  ctx);

	for (i = 0; i < dev->vdec_larb_cnt; i++) {
		target_bw = (u32)div64_ul((u64)dev->vdec_larb_bw[i].larb_base_bw *
			dev->vdec_dvfs_params.target_freq,
			dev->vdec_dvfs_params.min_freq);

		if (dev->vdec_larb_bw[i].larb_type < VCODEC_LARB_SUM) {
			if (!dev->vdec_dvfs_params.set_bw_in_min_freq &&
				(dev->vdec_dvfs_params.target_freq == dev->vdec_dvfs_params.min_freq)) {
				mtk_icc_set_bw(dev->vdec_qos_req[i],
					MBps_to_icc(0), 0);
				mtk_v4l2_debug(8, "[VDEC] larb %d bw %u (min opp, no request) MB/s",
				dev->vdec_larb_bw[i].larb_id, target_bw);
			} else {
				mtk_icc_set_bw(dev->vdec_qos_req[i],
					MBps_to_icc((u32)target_bw), 0);
				mtk_v4l2_debug(8, "[VDEC] larb %d bw %u MB/s",
				dev->vdec_larb_bw[i].larb_id, target_bw);
			}
		} else {
			mtk_v4l2_debug(8, "[VDEC] unknown larb type %d\n",
				dev->vdec_larb_bw[i].larb_type);
		}
	}
}

void mtk_vdec_pmqos_end_inst(struct mtk_vcodec_ctx *ctx)
{
	int i;
	struct mtk_vcodec_dev *dev = 0;
	u32 target_bw = 0;

	dev = ctx->dev;
	mtk_v4l2_debug(8, "[VDVFS][VDEC] ctx = %p",  ctx);


	for (i = 0; i < dev->vdec_larb_cnt; i++) {
		target_bw = (u32)div64_ul((u64)dev->vdec_larb_bw[i].larb_base_bw *
			dev->vdec_dvfs_params.target_freq,
			dev->vdec_dvfs_params.min_freq);

		if (list_empty(&dev->vdec_dvfs_inst)) /* no more instances */
			target_bw = 0;

		if (dev->vdec_larb_bw[i].larb_type < VCODEC_LARB_SUM) {
			if (!dev->vdec_dvfs_params.set_bw_in_min_freq &&
				dev->vdec_dvfs_params.target_freq == dev->vdec_dvfs_params.min_freq) {
				mtk_icc_set_bw(dev->vdec_qos_req[i],
					MBps_to_icc(0), 0);
				mtk_v4l2_debug(8, "[VDEC] larb %d bw %u (min opp, no request) MB/s",
				dev->vdec_larb_bw[i].larb_id, target_bw);
			} else {
				mtk_icc_set_bw(dev->vdec_qos_req[i],
					MBps_to_icc((u32)target_bw), 0);
				mtk_v4l2_debug(8, "[VDEC] larb %d w %u MB/s",
				dev->vdec_larb_bw[i].larb_id, target_bw);
			}
		} else {
			mtk_v4l2_debug(8, "[VDEC] unknown larb type %d",
				dev->vdec_larb_bw[i].larb_type);
		}
	}
}

void mtk_vdec_dvfs_begin_frame(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	struct mtk_vcodec_dev *dev = 0;

	dev = ctx->dev;
	if (dev->vdec_reg == 0 && dev->vdec_mmdvfs_clk == 0)
		return;

	dev->vdec_dvfs_params.frame_need_update = 1;

	if (!dev->vdec_dvfs_params.per_frame_adjust)
		dev->vdec_dvfs_params.frame_need_update = 0;

	if (dev->vdec_dvfs_params.frame_need_update &&
		(dev->vdec_dvfs_params.target_freq != dev->vdec_dvfs_params.min_freq)) {
		mtk_v4l2_debug(4, "[VDEC] f_begin freq %u",
			ctx->dev->vdec_dvfs_params.target_freq);
		set_vdec_opp(ctx->dev, ctx->dev->vdec_dvfs_params.target_freq);
	}
}


void mtk_vdec_dvfs_end_frame(struct mtk_vcodec_ctx *ctx, int hw_id)
{
	struct mtk_vcodec_dev *dev = 0;

	dev = ctx->dev;
	if (dev->vdec_reg == 0 && dev->vdec_mmdvfs_clk == 0)
		return;

	dev->vdec_dvfs_params.frame_need_update = 1;

	if (!dev->vdec_dvfs_params.per_frame_adjust)
		dev->vdec_dvfs_params.frame_need_update = 0;

	if (dev->vdec_dvfs_params.frame_need_update &&
		(dev->vdec_dvfs_params.target_freq != dev->vdec_dvfs_params.min_freq)) {
		mtk_v4l2_debug(4, "[VDEC] f_end freq %u", ctx->dev->vdec_dvfs_params.min_freq);
		set_vdec_opp(ctx->dev, ctx->dev->vdec_dvfs_params.min_freq);
	}
}


void mtk_vdec_pmqos_begin_frame(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_vcodec_dev *dev = 0;

	dev = ctx->dev;

	if (dev->vdec_dvfs_params.frame_need_update &&
		(dev->vdec_dvfs_params.set_bw_in_min_freq ||
		(dev->vdec_dvfs_params.target_freq != dev->vdec_dvfs_params.min_freq))) {
		mtk_vdec_pmqos_begin_inst(ctx);
	}
	dev->vdec_dvfs_params.frame_need_update = 0;
}


void mtk_vdec_pmqos_end_frame(struct mtk_vcodec_ctx *ctx)
{
	int i;
	struct mtk_vcodec_dev *dev = 0;

	dev = ctx->dev;

	if (!dev->vdec_dvfs_params.frame_need_update ||
		(!dev->vdec_dvfs_params.set_bw_in_min_freq &&
		dev->vdec_dvfs_params.target_freq == dev->vdec_dvfs_params.min_freq))
		return;

	for (i = 0; i < dev->vdec_larb_cnt; i++) {
		if (dev->vdec_larb_bw[i].larb_type < VCODEC_LARB_SUM) {
			mtk_icc_set_bw(dev->vdec_qos_req[i], 0, 0);
			mtk_v4l2_debug(8, "[VDEC] set larb %u bw", dev->vdec_larb_bw[i].larb_id);
		} else {
			mtk_v4l2_debug(8, "[VDEC] unknown larb type %d",
				dev->vdec_larb_bw[i].larb_type);
		}
	}
	dev->vdec_dvfs_params.frame_need_update = 0;
}

void mtk_vdec_prepare_vcp_dvfs_data(struct mtk_vcodec_ctx *ctx, unsigned long *in)
{
	struct vcodec_inst *inst = 0;
	struct vdec_inst *inst_handle;
	struct vdec_vsi *vsi_data;

	if (ctx == NULL)
		return;

	inst_handle = (struct vdec_inst *) ctx->drv_handle;
	if (IS_ERR_OR_NULL(inst_handle)) {
		mtk_v4l2_err("[VDVFS][%d] find null drv handler", ctx->id);
		return;
	}

	vsi_data = inst_handle->vsi;

	if (IS_ERR_OR_NULL(vsi_data)) {
		mtk_v4l2_err("[VDVFS][%d] vsi is err or null", ctx->id);
		return;
	}

	inst = get_inst(ctx);
	if (!inst)
		add_inst(ctx);

	in[0] = MTK_INST_START;
	vsi_data->ctx_id = ctx->id;
	vsi_data->op_rate = ctx->dec_params.operating_rate;
	vsi_data->priority = ctx->dec_params.priority;
	vsi_data->codec_fmt = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
	vsi_data->is_active = ctx->is_active;

	ctx->last_monitor_op = -1; // for monitor op rate
	ctx->op_rate_adaptive = ctx->dec_params.operating_rate; // for monitor op rate
	return;
}

void mtk_vdec_unprepare_vcp_dvfs_data(struct mtk_vcodec_ctx *ctx, unsigned long *in)
{
	struct vcodec_inst *inst = 0;

	if (ctx == NULL)
		return;

	inst = get_inst(ctx);
	if (!inst)
		return;

	list_del(&inst->list);
	vfree(inst);

	in[0] = MTK_INST_END;
}

/* update dvfs_params of ctx to vcp in runtime */
void mtk_vdec_dvfs_set_vsi_dvfs_params(struct mtk_vcodec_ctx *ctx)
{
	struct vdec_inst *inst;
	struct vdec_vsi *vsi_data;

	if (ctx == NULL)
		return;
	mtk_v4l2_debug(4, "[VDVFS] ctx: %d vsi updated", ctx->id);

	inst = (struct vdec_inst *) ctx->drv_handle;
	vsi_data = inst->vsi;
	vsi_data->is_active = ctx->is_active;
	vsi_data->op_rate = ctx->dec_params.operating_rate;
	vsi_data->op_rate_adaptive = ctx->op_rate_adaptive;
}

void mtk_vdec_force_update_freq(struct mtk_vcodec_dev *dev)
{
	update_freq(dev, MTK_INST_DECODER);
	mtk_v4l2_debug(4, "[VDEC] freq %u", dev->vdec_dvfs_params.target_freq);
	set_vdec_opp(dev, dev->vdec_dvfs_params.target_freq);
}


void mtk_vdec_dvfs_update_dvfs_params(struct mtk_vcodec_ctx *ctx)
{
	struct vcodec_inst *inst = 0;
	bool mmdvfs_in_vcp = (ctx->dev->vdec_reg == 0 && ctx->dev->vdec_mmdvfs_clk == 0);
	unsigned long vcp_dvfs_data[1] = {MTK_INST_UPDATE};
	int ret;

	if (mmdvfs_in_vcp) {
		mtk_vdec_dvfs_set_vsi_dvfs_params(ctx);
		ret = vdec_if_set_param(ctx, SET_PARAM_MMDVFS, vcp_dvfs_data);
		if (ret != 0)
			mtk_v4l2_err("[VDVFS] %s [%d] alive ipi timeout", __func__, ctx->id);
	} else {
		inst = get_inst(ctx);
		if (!inst)
			return;
		inst->is_active = ctx->is_active;
		inst->op_rate = ctx->dec_params.operating_rate;
	}
}

/*
 *	Function name: mtk_vdec_dvfs_monitor_op_rate
 *	Description: This function updates the op rate of ctx by monitoring input buffer queued.
 *			1. Montior period: 500 ms
 *			2. Bypass the first interval, compare op rate of 2nd & 3rd interval
 *			3. The monitored rate needs to be stable (<20% compares to prev interval)
 *			4. Diff > 20% than current used op rate
 *	Returns: Boolean, the op rate needs to be updated
 */
bool mtk_vdec_dvfs_monitor_op_rate(struct mtk_vcodec_ctx *ctx, int buf_type)
{
	unsigned int cur_in_timestamp, time_diff, threshold = 20;
	unsigned int cur_op, tmp_op;/* monitored op in the prev interval */
	int prev_op;
	bool update_op = false;

	if (buf_type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ||
		!ctx->dev->vdec_dvfs_params.mmdvfs_in_adaptive)
		return false;

	prev_op = ctx->last_monitor_op;
	cur_in_timestamp = jiffies_to_msecs(jiffies);
	if (ctx->prev_inbuf_time == 0) {
		ctx->prev_inbuf_time = cur_in_timestamp;
		return false;
	}

	time_diff = cur_in_timestamp - ctx->prev_inbuf_time;
	ctx->input_buf_cnt++;

	if (time_diff > VDEC_ADAPTIVE_OPRATE_INTERVAL) {
		ctx->last_monitor_op =
			ctx->input_buf_cnt * 1000 / time_diff;
		ctx->prev_inbuf_time = cur_in_timestamp;
		ctx->input_buf_cnt = 0;
		cur_op = ctx->op_rate_adaptive;

		mtk_v4l2_debug(4, "[VDVFS][VDEC][ADAPTIVE][%d] prev_op: %d, moni_op: %d, cur_adp_op: %d",
			ctx->id, prev_op, ctx->last_monitor_op, cur_op);

		if (prev_op < 0) {
			// first interval, bypass
			ctx->last_monitor_op = 0;
			return false;
		} else if (prev_op == 0) {
			// second interval, need compare to 3rd interval value
			return false;
		}

		tmp_op = MAX(ctx->last_monitor_op, prev_op);

		update_op = mtk_dvfs_check_op_diff(prev_op, ctx->last_monitor_op, threshold, 1) &&
			mtk_dvfs_check_op_diff(cur_op, tmp_op, threshold, -1);

		if (update_op) {
			ctx->op_rate_adaptive = tmp_op;
			mtk_v4l2_debug(0, "[VDVFS][VDEC][ADAPTIVE][%d] update adaptive op %d -> %d",
				ctx->id, cur_op, ctx->op_rate_adaptive);
			return true;
		}

	}
	return false;
}
