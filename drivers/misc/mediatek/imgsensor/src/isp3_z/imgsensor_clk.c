// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "imgsensor_common.h"

#include <linux/clk.h>
#include "imgsensor_clk.h"

/*by platform settings and elements should not be reordered */
char *gimgsensor_mclk_name[IMGSENSOR_CCF_MAX_NUM] = {
	"CLK_TOP_CAMTG_SEL",
	"CLK_TOP_CAMTG2_SEL",
	"CLK_TOP_CLK26M",
	"CLK_TOP_UNIVPLL_48M_D2",
	"CLK_TOP_UNIVPLL2_D8",
	"CLK_TOP_UNIVPLL_D26",
	"CLK_TOP_UNIVPLL2_D32",
	"CLK_TOP_UNIVPLL_48M_D4",
	"CLK_TOP_UNIVPLL_48M_D8",
	"CLK_TOP_SENINF_SEL",
	"CLK_TOP_SCAM_SEL",
};


enum {
	MCLK_ENU_START,
	MCLK_26MHZ = MCLK_ENU_START,
	MCLK_24MHZ,
	MCLK_52MHZ,
	MCLK_48MHZ,
	MCLK_13MHZ,
	MCLK_12MHZ,
	MCLK_6MHZ,
	MCLK_MAX,
};

enum {
	FREQ_26MHZ = 26,
	FREQ_24MHZ = 24,
	FREQ_52MHZ = 52,
	FREQ_48MHZ = 48,
	FREQ_13MHZ = 13,
	FREQ_12MHZ = 12,
	FREQ_6MHZ  =  6,
};


static inline void imgsensor_clk_check(struct IMGSENSOR_CLK *pclk)
{
	int i;

	for (i = 0; i < IMGSENSOR_CCF_MAX_NUM; i++)
		WARN_ON(IS_ERR(pclk->imgsensor_ccf[i]));
}

/*******************************************************************************
 * Common Clock Framework (CCF)
 *******************************************************************************
 */
enum IMGSENSOR_RETURN imgsensor_clk_init(struct IMGSENSOR_CLK *pclk)
{
	int i;
	struct platform_device *pplatform_dev = gpimgsensor_hw_platform_device;

	if (pplatform_dev == NULL) {
		pr_info("[%s] pdev is null\n", __func__);
		return IMGSENSOR_RETURN_ERROR;
	}
	/* get all possible using clocks */
	for (i = 0; i < IMGSENSOR_CCF_MAX_NUM; i++)
		pclk->imgsensor_ccf[i] =
		    devm_clk_get(&pplatform_dev->dev, gimgsensor_mclk_name[i]);

#if IS_ENABLED(CONFIG_PM_SLEEP)
	pclk->seninf_wake_lock = wakeup_source_register(
			NULL, "seninf_lock_wakelock");
	if (!pclk->seninf_wake_lock) {
		pr_info("[%s] failed to get seninf_wake_lock\n", __func__);
		return IMGSENSOR_RETURN_ERROR;
	}
	pr_info("[%s] seninf_wake_lock register done\n", __func__);
#endif

	return IMGSENSOR_RETURN_SUCCESS;
}

enum IMGSENSOR_RETURN imgsensor_clk_exit(struct IMGSENSOR_CLK *pclk)
{
#if IS_ENABLED(CONFIG_PM_SLEEP)
	if (!pclk->seninf_wake_lock) {
		pr_info("[%s] seninf_wake_lock is NULL\n", __func__);
	} else {
		wakeup_source_unregister(pclk->seninf_wake_lock);
		pr_info("[%s] seninf_wake_lock unregister done\n", __func__);
	}
#endif
	return IMGSENSOR_RETURN_SUCCESS;
}

enum IMGSENSOR_RETURN seninf_wake_lock_get(struct IMGSENSOR_CLK *pclk)
{
#if IS_ENABLED(CONFIG_PM_SLEEP)
	if (!pclk->seninf_wake_lock) {
		pr_info("[%s] seninf_wake_lock is NULL\n", __func__);
	} else {
		__pm_stay_awake(pclk->seninf_wake_lock);
		pr_info("[%s] seninf_wake_lock stay_awake\n", __func__);
	}
#endif
	return IMGSENSOR_RETURN_SUCCESS;
}

enum IMGSENSOR_RETURN seninf_wake_lock_put(struct IMGSENSOR_CLK *pclk)
{
#if IS_ENABLED(CONFIG_PM_SLEEP)
	if (!pclk->seninf_wake_lock) {
		pr_info("[%s] seninf_wake_lock is NULL\n", __func__);
	} else {
		__pm_relax(pclk->seninf_wake_lock);
		pr_info("[%s] seninf_wake_lock relax\n", __func__);
	}
#endif
	return IMGSENSOR_RETURN_SUCCESS;
}

int imgsensor_clk_set(
	struct IMGSENSOR_CLK *pclk, struct ACDK_SENSOR_MCLK_STRUCT *pmclk)
{
	int ret = 0;
	int mclk_index = MCLK_ENU_START;
	const int supported_mclk_freq[MCLK_MAX] = {
		FREQ_26MHZ, FREQ_24MHZ, FREQ_52MHZ, FREQ_48MHZ, FREQ_13MHZ, FREQ_12MHZ, FREQ_6MHZ
	};

	for (mclk_index = MCLK_ENU_START; mclk_index < MCLK_MAX; mclk_index++) {
		if (pmclk->freq == supported_mclk_freq[mclk_index])
			break;
	}
	if (pmclk->TG >= IMGSENSOR_CCF_MCLK_TG_MAX_NUM || mclk_index == MCLK_MAX) {
		pr_info("[CAMERA SENSOR]kdSetSensorMclk out of range, tg=%d, freq= %d\n",
			pmclk->TG, pmclk->freq);
		return -EFAULT;
	}
	mclk_index += IMGSENSOR_CCF_MCLK_FREQ_MIN_NUM;
	imgsensor_clk_check(pclk);

	if (pmclk->on) {

		/* Workaround for timestamp: TG1 always ON */
		if (clk_prepare_enable(pclk->imgsensor_ccf[IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL]))
			pr_info("[CAMERA SENSOR] failed tg=%d\n",
			       IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL);
		else
			atomic_inc(&pclk->enable_cnt[IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL]);

		if (clk_prepare_enable(pclk->imgsensor_ccf[pmclk->TG]))
			pr_info("[CAMERA SENSOR] failed tg=%d\n", pmclk->TG);
		else
			atomic_inc(&pclk->enable_cnt[pmclk->TG]);

		if (clk_prepare_enable(pclk->imgsensor_ccf[mclk_index]))
			pr_info("[CAMERA SENSOR]imgsensor_ccf failed freq= %d, mclk_index %d\n",
					pmclk->freq, mclk_index);
		else
			atomic_inc(&pclk->enable_cnt[mclk_index]);

		ret = clk_set_parent(pclk->imgsensor_ccf[pmclk->TG], pclk->imgsensor_ccf[mclk_index]);
	} else {

		/* Workaround for timestamp: TG1 always ON */
		clk_disable_unprepare(pclk->imgsensor_ccf[IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL]);
		atomic_dec(&pclk->enable_cnt[IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL]);

		clk_disable_unprepare(pclk->imgsensor_ccf[pmclk->TG]);
		atomic_dec(&pclk->enable_cnt[pmclk->TG]);
		clk_disable_unprepare(pclk->imgsensor_ccf[mclk_index]);
		atomic_dec(&pclk->enable_cnt[mclk_index]);
	}

	return ret;
}

void imgsensor_clk_enable_all(struct IMGSENSOR_CLK *pclk)
{
	unsigned int i = 0;

	pr_debug("imgsensor_clk_enable_all_cg\n");
	for (i = IMGSENSOR_CCF_CG_MIN_NUM; i < IMGSENSOR_CCF_CG_MAX_NUM; i++) {
		if (!IS_ERR(pclk->imgsensor_ccf[i])) {
			if (clk_prepare_enable(pclk->imgsensor_ccf[i]))
				pr_info(
				"[CAMERA SENSOR]imgsensor_ccf enable cg fail cg_index = %d\n",
				i);
			else
				atomic_inc(&pclk->enable_cnt[i]);
			/*
			 * pr_debug("imgsensor_clk_enable_all %s ok\n",
			 * gimgsensor_mclk_name[i]);
			 */
		}
	}
}

void imgsensor_clk_disable_all(struct IMGSENSOR_CLK *pclk)
{
	unsigned int i = 0;

	for (i = 0; i < IMGSENSOR_CCF_MAX_NUM; i++) {
		for (; atomic_read(&pclk->enable_cnt[i]) > 0; ) {
			clk_disable_unprepare(pclk->imgsensor_ccf[i]);
			atomic_dec(&pclk->enable_cnt[i]);
		}
	}
	pr_debug("%s X\n", __func__);
}

int imgsensor_clk_ioctrl_handler(void *pbuff)
{
	unsigned int id = *(unsigned int *)pbuff;

	if (id == 3 || id == 8 || id == 35 || id == 41)
		*(unsigned int *)pbuff = mt_get_ckgen_freq(id);
	else
		*(unsigned int *)pbuff = 0;
	pr_debug("hf_fmm_ck = %u\n", mt_get_ckgen_freq(3));
	pr_debug("f_fcamtg_ck = %u\n", mt_get_ckgen_freq(8));
	pr_debug("f_fseninf_ck = %u\n", mt_get_ckgen_freq(35));
	pr_debug("f_fcamtg2_ck = %u\n", mt_get_ckgen_freq(41));
	return 0;
}
