// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mt-plat/mtk_thermal_typedefs.h"
#include "mach/mtk_thermal.h"
// #include <mt-plat/upmu_common.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/mfd/mt6397/core.h>
// #include <mt-plat/mtk_auxadc_intf.h>
// #include <mach/mtk_pmic.h>
#include <tspmic_settings.h>

#if defined(THERMAL_USE_IIO_CHANNEL)
#include <dt-bindings/iio/mt635x-auxadc.h>
#include <linux/iio/consumer.h>
#endif

/*=============================================================
 *Local variable definition
 *=============================================================
 */
#define PMIC_AUXADC_EFUSE_O_VTS_ADDR  0x11d0
#define PMIC_AUXADC_EFUSE_O_VTS_MASK  0x1FFF
#define PMIC_AUXADC_EFUSE_O_VTS_SHIFT 0

#define PMIC_AUXADC_EFUSE_O_VTS_2_ADDR  0x11d6
#define PMIC_AUXADC_EFUSE_O_VTS_2_MASK  0x1FFF
#define PMIC_AUXADC_EFUSE_O_VTS_2_SHIFT 0

#define PMIC_AUXADC_EFUSE_O_VTS_3_ADDR  0x11d8
#define PMIC_AUXADC_EFUSE_O_VTS_3_MASK  0x1FFF
#define PMIC_AUXADC_EFUSE_O_VTS_3_SHIFT 0

#define PMIC_AUXADC_EFUSE_4RSV0_ADDR  0x11d4
#define PMIC_AUXADC_EFUSE_4RSV0_MASK  0x7FF
#define PMIC_AUXADC_EFUSE_4RSV0_SHIFT 5

#define PMIC_AUXADC_EFUSE_DEGC_CALI_ADDR   0x11ce
#define PMIC_AUXADC_EFUSE_DEGC_CALI_MASK   0x3F
#define PMIC_AUXADC_EFUSE_DEGC_CALI_SHIFT   0

#define PMIC_AUXADC_EFUSE_ADC_CALI_EN_ADDR  0x11ce
#define PMIC_AUXADC_EFUSE_ADC_CALI_EN_MASK  0x1
#define PMIC_AUXADC_EFUSE_ADC_CALI_EN_SHIFT 8

#define PMIC_AUXADC_EFUSE_O_SLOPE_SIGN_ADDR 0x11d2
#define PMIC_AUXADC_EFUSE_O_SLOPE_SIGN_MASK 0x1
#define PMIC_AUXADC_EFUSE_O_SLOPE_SIGN_SHIFT  8

#define PMIC_AUXADC_EFUSE_O_SLOPE_SIGN_ADDR 0x11d2
#define PMIC_AUXADC_EFUSE_O_SLOPE_SIGN_MASK 0x1
#define PMIC_AUXADC_EFUSE_O_SLOPE_SIGN_SHIFT  8

#define PMIC_AUXADC_EFUSE_O_SLOPE_ADDR  0x11d2
#define PMIC_AUXADC_EFUSE_O_SLOPE_MASK  0x3F
#define PMIC_AUXADC_EFUSE_O_SLOPE_SHIFT 0

#define PMIC_AUXADC_EFUSE_ID_ADDR 0x11d4
#define PMIC_AUXADC_EFUSE_ID_MASK 0x1
#define PMIC_AUXADC_EFUSE_ID_SHIFT 4

int mtktspmic_debug_log;
/* Cali */
static __s32 g_o_vts;
static __s32 g_o_vts_2;
static __s32 g_o_vts_3;
static __s32 g_o_4rsv0;
static __s32 g_degc_cali;
static __s32 g_adc_cali_en;
static __s32 g_o_slope;
static __s32 g_o_slope_sign;
static __s32 g_id;
static __s32 g_slope1 = 1;
static __s32 g_slope2 = 1;
static __s32 g_intercept;
static __s32 g_tsbuck1_slope1 = 1;
static __s32 g_tsbuck1_slope2 = 1;
static __s32 g_tsbuck1_intercept;
static __s32 g_tsbuck2_slope1 = 1;
static __s32 g_tsbuck2_slope2 = 1;
static __s32 g_tsbuck2_intercept;
static __s32 g_tsbuck3_slope1 = 1;
static __s32 g_tsbuck3_slope2 = 1;
static __s32 g_tsbuck3_intercept;

static DEFINE_MUTEX(TSPMIC_lock);
static int pre_temp1 = 0, PMIC_counter;
static int pre_tsbuck1_temp1 = 0, tsbuck1_cnt;
static int pre_tsbuck2_temp1 = 0, tsbuck2_cnt;
static int pre_tsbuck3_temp1 = 0, tsbuck3_cnt;
static int pre_tstsx_temp1 = 0, tstsx_cnt;
static int pre_tsdcxo_temp1 = 0, tsdcxo_cnt;

#if defined(THERMAL_USE_IIO_CHANNEL)
struct iio_channel *chan_chip_temp;
struct iio_channel *chan_vcore_temp;
struct iio_channel *chan_vproc_temp;
struct iio_channel *chan_vgpu_temp;
struct iio_channel *chan_tsx_temp;
struct iio_channel *chan_dcxo_temp;
#endif
/*=============================================================*/

static __s32 pmic_raw_to_temp(__u32 ret)
{
	__s32 t_current;

	__s32 y_curr = ret;

	t_current = g_intercept + ((g_slope1 * y_curr) / (g_slope2));

	mtktspmic_dprintk("[%s] t_current=%d\n", __func__, t_current);
	return t_current;
}

static __s32 tsbuck1_raw_to_temp(__u32 ret)
{
	__s32 t_current;

	__s32 y_curr = ret;

	t_current = g_tsbuck1_intercept +
		((g_tsbuck1_slope1 * y_curr) / (g_tsbuck1_slope2));

	mtktspmic_dprintk("[%s] t_current=%d\n", __func__, t_current);
	return t_current;
}

static __s32 tsbuck2_raw_to_temp(__u32 ret)
{
	__s32 t_current;

	__s32 y_curr = ret;

	t_current = g_tsbuck2_intercept +
		((g_tsbuck2_slope1 * y_curr) / (g_tsbuck2_slope2));

	mtktspmic_dprintk("[%s] t_current=%d\n", __func__, t_current);
	return t_current;
}

static __s32 tsbuck3_raw_to_temp(__u32 ret)
{
	__s32 t_current;

	__s32 y_curr = ret;

	t_current = g_tsbuck3_intercept +
		((g_tsbuck3_slope1 * y_curr) / (g_tsbuck3_slope2));

	mtktspmic_dprintk("[%s] t_current=%d\n", __func__, t_current);
	return t_current;
}

static unsigned int thermal_pmic_get_register_value(struct regmap *map,
		unsigned int addr, unsigned int shift, unsigned int mask)
{
	unsigned int value = 0;

	regmap_read(map, addr, &value);
	value =
		(value &
		(mask << shift))
		>> shift;
	return value;
}

static void mtktspmic_read_efuse(struct regmap *pmic_map)
{
	mtktspmic_info("[pmic_debug]  start\n");
	/* MT6359 */
	g_o_vts = thermal_pmic_get_register_value(pmic_map, PMIC_AUXADC_EFUSE_O_VTS_ADDR,
			PMIC_AUXADC_EFUSE_O_VTS_SHIFT, PMIC_AUXADC_EFUSE_O_VTS_MASK);
	g_o_vts_2 = thermal_pmic_get_register_value(pmic_map, PMIC_AUXADC_EFUSE_O_VTS_2_ADDR,
			PMIC_AUXADC_EFUSE_O_VTS_2_SHIFT, PMIC_AUXADC_EFUSE_O_VTS_2_MASK);
	g_o_vts_3 = thermal_pmic_get_register_value(pmic_map, PMIC_AUXADC_EFUSE_O_VTS_3_ADDR,
			PMIC_AUXADC_EFUSE_O_VTS_3_SHIFT, PMIC_AUXADC_EFUSE_O_VTS_3_MASK);
	g_o_4rsv0 = thermal_pmic_get_register_value(pmic_map, PMIC_AUXADC_EFUSE_4RSV0_ADDR,
			PMIC_AUXADC_EFUSE_4RSV0_SHIFT, PMIC_AUXADC_EFUSE_4RSV0_MASK);
	g_degc_cali = thermal_pmic_get_register_value(pmic_map, PMIC_AUXADC_EFUSE_DEGC_CALI_ADDR,
			PMIC_AUXADC_EFUSE_DEGC_CALI_SHIFT, PMIC_AUXADC_EFUSE_DEGC_CALI_MASK);
	g_adc_cali_en = thermal_pmic_get_register_value(
			pmic_map, PMIC_AUXADC_EFUSE_ADC_CALI_EN_ADDR,
			PMIC_AUXADC_EFUSE_ADC_CALI_EN_SHIFT, PMIC_AUXADC_EFUSE_ADC_CALI_EN_MASK);
	g_o_slope_sign =
		thermal_pmic_get_register_value(pmic_map, PMIC_AUXADC_EFUSE_O_SLOPE_SIGN_ADDR,
			PMIC_AUXADC_EFUSE_O_SLOPE_SIGN_SHIFT, PMIC_AUXADC_EFUSE_O_SLOPE_SIGN_MASK);
	g_o_slope = thermal_pmic_get_register_value(pmic_map, PMIC_AUXADC_EFUSE_O_SLOPE_ADDR,
			PMIC_AUXADC_EFUSE_O_SLOPE_SHIFT, PMIC_AUXADC_EFUSE_O_SLOPE_MASK);
	g_id = thermal_pmic_get_register_value(pmic_map, PMIC_AUXADC_EFUSE_ID_ADDR,
			PMIC_AUXADC_EFUSE_ID_SHIFT, PMIC_AUXADC_EFUSE_ID_MASK);

	mtktspmic_info("[pmic_debug] 6359_efuse: g_o_vts        = %d\n",
			g_o_vts);
	mtktspmic_info("[pmic_debug] 6359_efuse: g_o_vts_2      = %d\n",
			g_o_vts_2);
	mtktspmic_info("[pmic_debug] 6359_efuse: g_o_vts_3      = %d\n",
			g_o_vts_3);
	mtktspmic_info("[pmic_debug] 6358_efuse: g_o_4rsv0      = %d\n",
			g_o_4rsv0);
	mtktspmic_info("[pmic_debug] 6359_efuse: g_degc_cali    = %d\n",
			g_degc_cali);
	mtktspmic_info("[pmic_debug] 6359_efuse: g_adc_cali_en  = %d\n",
			g_adc_cali_en);
	mtktspmic_info("[pmic_debug] 6359_efuse: g_o_slope_sign = %d\n",
			g_o_slope_sign);
	mtktspmic_info("[pmic_debug] 6359_efuse: g_o_slope      = %d\n",
			g_o_slope);
	mtktspmic_info("[pmic_debug] 6359_efuse: g_id		   = %d\n",
			g_id);

	mtktspmic_info("[pmic_debug]  end\n");
}

void mtktspmic_cali_prepare(struct regmap *pmic_map)
{
	mtktspmic_read_efuse(pmic_map);

	if (g_id == 0)
		g_o_slope = 0;

	/* g_adc_cali_en=0;//FIX ME */

	if (g_adc_cali_en == 0) {	/* no calibration */
		mtktspmic_info("[pmic_debug]  It isn't calibration values\n");
		g_o_vts = 1600;
		g_o_vts_2 = 1600;
		g_o_vts_3 = 1600;
		g_o_4rsv0 = 1600;
		g_degc_cali = 50;
		g_o_slope_sign = 0;
		g_o_slope = 0;
	}

	if (g_degc_cali < 38 || g_degc_cali > 60)
		g_degc_cali = 53;

	mtktspmic_info("[pmic_debug] g_o_vts        = 0x%x\n", g_o_vts);
	mtktspmic_info("[pmic_debug] g_o_vts_2      = 0x%x\n", g_o_vts_2);
	mtktspmic_info("[pmic_debug] g_o_vts_3      = 0x%x\n", g_o_vts_3);
	mtktspmic_info("[pmic_debug] g_o_4rsv0      = 0x%x\n", g_o_4rsv0);
	mtktspmic_info("[pmic_debug] g_degc_cali    = 0x%x\n", g_degc_cali);
	mtktspmic_info("[pmic_debug] g_adc_cali_en  = 0x%x\n", g_adc_cali_en);
	mtktspmic_info("[pmic_debug] g_o_slope      = 0x%x\n", g_o_slope);
	mtktspmic_info("[pmic_debug] g_o_slope_sign = 0x%x\n", g_o_slope_sign);
	mtktspmic_info("[pmic_debug] g_id           = 0x%x\n", g_id);

}

void mtktspmic_cali_prepare2(void)
{
	__s32 vbe_t;
	int factor;

	factor = 1681;

	g_slope1 = (100 * 1000 * 10);	/* 1000 is for 0.001 degree */

	if (g_o_slope_sign == 0)
		g_slope2 = -(factor + g_o_slope);
	else
		g_slope2 = -(factor - g_o_slope);

	vbe_t = (-1) * ((((g_o_vts) * 1800)) / 4096) * 1000;

	if (g_o_slope_sign == 0)
		g_intercept = (vbe_t * 1000) / (-(factor + g_o_slope * 10));
	/*0.001 degree */
	else
		g_intercept = (vbe_t * 1000) / (-(factor - g_o_slope * 10));
	/*0.001 degree */

	g_intercept = g_intercept + (g_degc_cali * (1000 / 2));
	/* 1000 is for 0.1 degree */

	mtktspmic_info(
			"[Thermal calibration] SLOPE1=%d SLOPE2=%d INTERCEPT=%d, Vbe = %d\n",
			g_slope1, g_slope2, g_intercept, vbe_t);

	factor = 1863;

	g_tsbuck1_slope1 = (100 * 1000 * 10);	/* 1000 is for 0.001 degree */

	if (g_o_slope_sign == 0)
		g_tsbuck1_slope2 = -(factor + g_o_slope);
	else
		g_tsbuck1_slope2 = -(factor - g_o_slope);

	vbe_t = (-1) * ((((g_o_vts_2) * 1800)) / 4096) * 1000;

	if (g_o_slope_sign == 0)
		g_tsbuck1_intercept =
			(vbe_t * 1000) / (-(factor + g_o_slope * 10));
	/*0.001 degree */
	else
		g_tsbuck1_intercept =
			(vbe_t * 1000) / (-(factor - g_o_slope * 10));
	/*0.001 degree */

	g_tsbuck1_intercept = g_tsbuck1_intercept + (g_degc_cali * (1000 / 2));
	/* 1000 is for 0.1 degree */

	mtktspmic_info(
		"[Thermal calibration] SLOPE1=%d SLOPE2=%d INTERCEPT=%d, Vbe = %d\n",
		g_tsbuck1_slope1, g_tsbuck1_slope2, g_tsbuck1_intercept, vbe_t);

	factor = 1863;

	g_tsbuck2_slope1 = (100 * 1000 * 10);
	/* 1000 is for 0.001 degree */

	if (g_o_slope_sign == 0)
		g_tsbuck2_slope2 = -(factor + g_o_slope);
	else
		g_tsbuck2_slope2 = -(factor - g_o_slope);

	vbe_t = (-1) * ((((g_o_vts_3) * 1800)) / 4096) * 1000;

	if (g_o_slope_sign == 0)
		g_tsbuck2_intercept =
			(vbe_t * 1000) / (-(factor + g_o_slope * 10));
	/*0.001 degree */
	else
		g_tsbuck2_intercept =
			(vbe_t * 1000) / (-(factor - g_o_slope * 10));
	/*0.001 degree */

	g_tsbuck2_intercept = g_tsbuck2_intercept + (g_degc_cali * (1000 / 2));
	/* 1000 is for 0.1 degree */

	mtktspmic_info(
		"[Thermal calibration] SLOPE1=%d SLOPE2=%d INTERCEPT=%d, Vbe = %d\n",
		g_tsbuck2_slope1, g_tsbuck2_slope2, g_tsbuck2_intercept, vbe_t);

	factor = 1863;

	g_tsbuck3_slope1 = (100 * 1000 * 10);
	/* 1000 is for 0.001 degree */

	if (g_o_slope_sign == 0)
		g_tsbuck3_slope2 = -(factor + g_o_slope);
	else
		g_tsbuck3_slope2 = -(factor - g_o_slope);

	vbe_t = (-1) * ((((g_o_4rsv0) * 1800)) / 4096) * 1000;

	if (g_o_slope_sign == 0)
		g_tsbuck3_intercept =
			(vbe_t * 1000) / (-(factor + g_o_slope * 10));
	/*0.001 degree */
	else
		g_tsbuck3_intercept =
			(vbe_t * 1000) / (-(factor - g_o_slope * 10));
	/*0.001 degree */

	g_tsbuck3_intercept = g_tsbuck3_intercept + (g_degc_cali * (1000 / 2));
	/* 1000 is for 0.1 degree */

	mtktspmic_info(
		"[Thermal calibration] SLOPE1=%d SLOPE2=%d INTERCEPT=%d, Vbe = %d\n",
		g_tsbuck3_slope1, g_tsbuck3_slope2, g_tsbuck3_intercept, vbe_t);

}

#if defined(THERMAL_USE_IIO_CHANNEL)
int mtktspmic_get_from_dts(struct platform_device *pdev)
{
	int ret, error = 0;

	chan_chip_temp = devm_iio_channel_get(&pdev->dev, "pmic_chip_temp");
	if (IS_ERR(chan_chip_temp)) {
		ret = PTR_ERR(chan_chip_temp);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				pr_notice(
					"%s, AUXADC_CHIP_TEMP  get fail, ret=%d\n", __func__, ret);
			error = ret;
		}
	}
	chan_vcore_temp = devm_iio_channel_get(&pdev->dev, "pmic_buck1_temp");
	if (IS_ERR(chan_vcore_temp)) {
		ret = PTR_ERR(chan_vcore_temp);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				pr_notice(
					"%s, AUXADC_VCORE_TEMP  get fail, ret=%d\n", __func__, ret);
			error = ret;
		}
	}

	chan_vproc_temp = devm_iio_channel_get(&pdev->dev, "pmic_buck2_temp");
	if (IS_ERR(chan_vproc_temp)) {
		ret = PTR_ERR(chan_vproc_temp);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				pr_notice(
				"	%s, AUXADC_VPROC_TEMP   get fail, ret=%d\n", __func__, ret);
			error = ret;
		}
	}

	chan_vgpu_temp = devm_iio_channel_get(&pdev->dev, "pmic_buck3_temp");
	if (IS_ERR(chan_vgpu_temp)) {
		ret = PTR_ERR(chan_vgpu_temp);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				pr_notice(
					"%s, AUXADC_VGPU_TEMP  get fail, ret=%d\n", __func__, ret);
			error = ret;
		}
	}

	chan_tsx_temp = devm_iio_channel_get(&pdev->dev, "pmic_tsx_temp");
	if (IS_ERR(chan_tsx_temp)) {
		ret = PTR_ERR(chan_tsx_temp);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				pr_notice(
					"%s, AUXADC_TSX_TEMP   get fail, ret=%d\n", __func__, ret);
			error = ret;
		}
	}

	chan_dcxo_temp = devm_iio_channel_get(&pdev->dev, "pmic_dcxo_temp");
	if (IS_ERR(chan_dcxo_temp)) {
		ret = PTR_ERR(chan_dcxo_temp);
		if (ret) {
			if (ret != -EPROBE_DEFER)
				pr_notice(
					"%s, AUXADC_DCXO_TEMP   get fail, ret=%d\n", __func__, ret);
			error = ret;
		}
	}
	return error;
}
#endif

int mtktspmic_get_hw_temp(void)
{
	int temp = 0, temp1 = 0;
#if defined(THERMAL_USE_IIO_CHANNEL)
	int ret;
#endif

	mutex_lock(&TSPMIC_lock);
#if defined(THERMAL_USE_IIO_CHANNEL)
	if (!IS_ERR(chan_chip_temp)) {
		ret = iio_read_channel_processed(chan_chip_temp, &temp);
		if (ret < 0)
			pr_notice("pmic_chip_temp read fail, ret=%d\n", ret);
	}
#else
	temp = pmic_get_auxadc_value(AUXADC_LIST_CHIP_TEMP);
#endif
	temp1 = pmic_raw_to_temp(temp);
	mutex_unlock(&TSPMIC_lock);

	mtktspmic_dprintk("[pmic_debug] Raw=%d, T=%d\n", temp, temp1);

	if ((temp1 > 100000) || (temp1 < -30000))
		mtktspmic_info("[%s] raw=%d, PMIC T=%d", __func__, temp, temp1);

	if ((temp1 > 150000) || (temp1 < -50000)) {
		mtktspmic_info("[%s] temp(%d) too high, drop this data!\n",
			__func__, temp1);
		temp1 = pre_temp1;
	} else if ((PMIC_counter != 0)
			&& (((pre_temp1 - temp1) > 30000)
				|| ((temp1 - pre_temp1) > 30000))) {
		mtktspmic_info("[%s] temp diff too large, drop this data\n",
			__func__);
		temp1 = pre_temp1;
	} else {
		/* update previous temp */
		pre_temp1 = temp1;
		mtktspmic_dprintk("[%s] pre_temp1=%d\n", __func__, pre_temp1);

		if (PMIC_counter == 0)
			PMIC_counter++;
	}


	return temp1;
}

int mt6366vcore_get_hw_temp(void)
{
	int temp = 0, temp1 = 0;
#if defined(THERMAL_USE_IIO_CHANNEL)
	int ret;
#endif

	mutex_lock(&TSPMIC_lock);
#if defined(THERMAL_USE_IIO_CHANNEL)
	if (!IS_ERR(chan_vcore_temp)) {
		ret = iio_read_channel_processed(chan_vcore_temp, &temp);
		if (ret < 0)
			pr_notice("pmic_vcore_temp read fail, ret=%d\n", ret);
	}
#else
	temp = pmic_get_auxadc_value(AUXADC_LIST_VCORE_TEMP);
#endif
	temp1 = tsbuck1_raw_to_temp(temp);
	mutex_unlock(&TSPMIC_lock);

	mtktspmic_dprintk("%s raw=%d T=%d\n", __func__, temp, temp1);

	if ((temp1 > 100000) || (temp1 < -30000))
		mtktspmic_info("%s raw=%d T=%d\n", __func__, temp, temp1);

	if ((temp1 > 150000) || (temp1 < -50000)) {
		mtktspmic_info("%s T=%d too high, drop it!\n", __func__,
				temp1);
		temp1 = pre_tsbuck1_temp1;
	} else if ((tsbuck1_cnt != 0)
			&& (((pre_tsbuck1_temp1 - temp1) > 30000)
				|| ((temp1 - pre_tsbuck1_temp1) > 30000))) {
		mtktspmic_info("%s delta temp too large, drop it!\n", __func__);
		temp1 = pre_tsbuck1_temp1;
	} else {
		/* update previous temp */
		pre_tsbuck1_temp1 = temp1;
		mtktspmic_dprintk(
				"%s pre_tsbuck1_temp1=%d\n", __func__,
				pre_tsbuck1_temp1);

		if (tsbuck1_cnt == 0)
			tsbuck1_cnt++;
	}

	return temp1;
}

int mt6366vproc_get_hw_temp(void)
{
	int temp = 0, temp1 = 0;
#if defined(THERMAL_USE_IIO_CHANNEL)
	int ret;
#endif

	mutex_lock(&TSPMIC_lock);
#if defined(THERMAL_USE_IIO_CHANNEL)
	if (!IS_ERR(chan_vproc_temp)) {
		ret = iio_read_channel_processed(chan_vproc_temp, &temp);
		if (ret < 0)
			pr_notice("pmic_vproc_temp read fail, ret=%d\n", ret);
	}
#else
	temp = pmic_get_auxadc_value(AUXADC_LIST_VPROC_TEMP);
#endif
	temp1 = tsbuck2_raw_to_temp(temp);
	mutex_unlock(&TSPMIC_lock);

	mtktspmic_dprintk("%s raw=%d T=%d\n", __func__, temp, temp1);

	if ((temp1 > 100000) || (temp1 < -30000))
		mtktspmic_info("%s raw=%d T=%d\n", __func__, temp, temp1);

	if ((temp1 > 150000) || (temp1 < -50000)) {
		mtktspmic_info("%s T=%d too high, drop it!\n", __func__,
				temp1);
		temp1 = pre_tsbuck2_temp1;
	} else if ((tsbuck2_cnt != 0)
		&& (((pre_tsbuck2_temp1 - temp1) > 30000)
		|| ((temp1 - pre_tsbuck2_temp1) > 30000))) {
		mtktspmic_info("%s delta temp too large, drop it!\n", __func__);
		temp1 = pre_tsbuck2_temp1;
	} else {
		/* update previous temp */
		pre_tsbuck2_temp1 = temp1;
		mtktspmic_dprintk("%s pre_tsbuck2_temp1=%d\n", __func__,
				pre_tsbuck2_temp1);

		if (tsbuck2_cnt == 0)
			tsbuck2_cnt++;
	}

	return temp1;
}

int mt6366vgpu_get_hw_temp(void)
{
	int temp = 0, temp1 = 0;
#if defined(THERMAL_USE_IIO_CHANNEL)
	int ret;
#endif

	mutex_lock(&TSPMIC_lock);
#if defined(THERMAL_USE_IIO_CHANNEL)
	if (!IS_ERR(chan_vgpu_temp)) {
		ret = iio_read_channel_processed(chan_vgpu_temp, &temp);
		if (ret < 0)
			pr_notice("pmic_vgpu_temp read fail, ret=%d\n", ret);
	}
#else
	temp = pmic_get_auxadc_value(AUXADC_LIST_VGPU_TEMP);
#endif
	temp1 = tsbuck3_raw_to_temp(temp);
	mutex_unlock(&TSPMIC_lock);

	mtktspmic_dprintk("%s raw=%d T=%d\n", __func__, temp, temp1);

	if ((temp1 > 100000) || (temp1 < -30000))
		mtktspmic_info("%s raw=%d T=%d\n", __func__, temp, temp1);

	if ((temp1 > 150000) || (temp1 < -50000)) {
		mtktspmic_info("%s T=%d too high, drop it!\n", __func__,
				temp1);
		temp1 = pre_tsbuck3_temp1;
	} else if ((tsbuck3_cnt != 0)
			&& (((pre_tsbuck3_temp1 - temp1) > 30000)
				|| ((temp1 - pre_tsbuck3_temp1) > 30000))) {
		mtktspmic_info("%s delta temp too large, drop it!\n", __func__);
		temp1 = pre_tsbuck3_temp1;
	} else {
		/* update previous temp */
		pre_tsbuck3_temp1 = temp1;
		mtktspmic_dprintk(
				"%s pre_tsbuck2_temp1=%d\n", __func__,
				pre_tsbuck3_temp1);

		if (tsbuck3_cnt == 0)
			tsbuck3_cnt++;
	}

	return temp1;
}

static int u_table[126] = {
	64078,
	63972,
	63860,
	63741,
	63614,
	63480,
	63338,
	63188,
	63029,
	62860,
	62683,
	62495,
	62297,
	62089,
	61870,
	61639,
	61396,
	61140,
	60872,
	60591,
	60297,
	59989,
	59667,
	59330,
	58978,
	58612,
	58230,
	57833,
	57420,
	56991,
	56546,
	56083,
	55605,
	55110,
	54598,
	54071,
	53528,
	52968,
	52393,
	51802,
	51195,
	50573,
	49936,
	49285,
	48620,
	47942,
	47251,
	46548,
	45834,
	45110,
	44375,
	43632,
	42880,
	42121,
	41355,
	40584,
	39808,
	39027,
	38245,
	37460,
	36675,
	35891,
	35106,
	34323,
	33543,
	32768,
	31996,
	31229,
	30469,
	29716,
	28968,
	28231,
	27502,
	26780,
	26070,
	25371,
	24683,
	24004,
	23339,
	22682,
	22042,
	21412,
	20795,
	20191,
	19600,
	19023,
	18458,
	17908,
	17369,
	16846,
	16331,
	15834,
	15347,
	14878,
	14415,
	13969,
	13535,
	13111,
	12701,
	12302,
	11914,
	11539,
	11176,
	10822,
	10482,
	10147,
	9827,
	9517,
	9214,
	8922,
	8637,
	8364,
	8098,
	7840,
	7590,
	7354,
	7120,
	6896,
	6674,
	6467,
	6264,
	6065,
	5876,
	5691,
	5510,
	5339
};
#define MIN_TSX_TEMP (-40000)
#define MAX_TSX_TEMP (+85000)
/* Original formula is
 * u = auxadc raw * 2^16 / (2^32 - 1)
 * Because kernel is not able to deal with floating point
 * we changed the formula to
 * u = auxadc raw * 2^16 / 2^32
 * => u = auxadc raw / 2^16
 * => u * 2^16 = auxadc raw
 */

static int tsx_u2t(int auxadc_raw)
{
	int i;
	int ret = 0;
	int u_upper, u_low, t_upper, t_low;

	if (auxadc_raw > u_table[0])
		return (int) MIN_TSX_TEMP;
	if (auxadc_raw < u_table[125])
		return (int) MAX_TSX_TEMP;

	for (i = 0; i < 125; i++) {
		if (auxadc_raw < u_table[i] && auxadc_raw >= u_table[i+1]) {
			u_upper = u_table[i+1];
			u_low = u_table[i];

			t_upper = (int) MIN_TSX_TEMP + (i+1) * 1000;
			t_low = (int) MIN_TSX_TEMP + i * 1000;

			if ((u_upper - u_low) == 0)
				/* This case should not happen */
				ret = t_low;
			else
				ret = t_low + ((t_upper - t_low) *
					(auxadc_raw - u_low)) /
					(u_upper - u_low);

			break;
		}
	}

	return ret;
}

int mt6366tsx_get_hw_temp(void)
{
	int raw = 0, temp1 = 0;
#if defined(THERMAL_USE_IIO_CHANNEL)
	int ret;
#endif

	mutex_lock(&TSPMIC_lock);
#if defined(THERMAL_USE_IIO_CHANNEL)
	if (!IS_ERR(chan_tsx_temp)) {
		ret = iio_read_channel_raw(chan_tsx_temp, &raw);
		if (ret < 0)
			pr_notice("pmic_tsx_temp read fail, ret=%d\n", ret);
	}

	temp1 = tsx_u2t(raw);
#else
	temp1 = 25000;
#endif

	mutex_unlock(&TSPMIC_lock);

	mtktspmic_dprintk("%s raw=%d T=%d\n", __func__, raw, temp1);

	if ((temp1 > 100000) || (temp1 < -30000))
		mtktspmic_info("%s raw=%d T=%d\n", __func__, raw, temp1);

	if ((temp1 > 150000) || (temp1 < -50000)) {
		mtktspmic_info("%s T=%d too high, drop it!\n", __func__,
				temp1);
		temp1 = pre_tstsx_temp1;
	} else if ((tstsx_cnt != 0)
			&& (((pre_tstsx_temp1 - temp1) > 30000)
				|| ((temp1 - pre_tstsx_temp1) > 30000))) {
		mtktspmic_info("%s delta temp too large, drop it!\n", __func__);
		temp1 = pre_tstsx_temp1;
	} else {
		/* update previous temp */
		pre_tstsx_temp1 = temp1;
		mtktspmic_dprintk(
				"%s pre_tsx_temp1=%d\n", __func__,
				pre_tstsx_temp1);

		if (tstsx_cnt == 0)
			tstsx_cnt++;
	}

	return temp1;
}

int mt6366dcxo_get_hw_temp(void)
{
	int raw = 0, temp1 = 0;
#if defined(THERMAL_USE_IIO_CHANNEL)
	int ret;
#endif

	mutex_lock(&TSPMIC_lock);
#if defined(THERMAL_USE_IIO_CHANNEL)
	if (!IS_ERR(chan_dcxo_temp)) {
		ret = iio_read_channel_raw(chan_dcxo_temp, &raw);
		if (ret < 0)
			pr_notice("pmic_dcxo_temp read fail, ret=%d\n", ret);
	}

	/* Temperature (C) = ((auxadc raw/32768*1.8)-0.545)/(-0.0017)+120 */
	/* From dcxo desiger John Chiang */
	temp1 = (((18000000/32768)*raw) - 5450000) / -17 + 120000;

	if (temp1 < -40000)
		temp1 = -40000;
	else if (temp1 > 120000)
		temp1 = 120000;
#else
	temp1 = 25000;
#endif
	mutex_unlock(&TSPMIC_lock);

	mtktspmic_dprintk("%s raw=%d T=%d\n", __func__, raw, temp1);

	if ((temp1 > 100000) || (temp1 < -30000))
		mtktspmic_info("%s raw=%d T=%d\n", __func__, raw, temp1);

	if ((temp1 > 150000) || (temp1 < -50000)) {
		mtktspmic_info("%s T=%d too high, drop it!\n", __func__,
				temp1);
		temp1 = pre_tsdcxo_temp1;
	} else if ((tsdcxo_cnt != 0)
			&& (((pre_tsdcxo_temp1 - temp1) > 30000)
				|| ((temp1 - pre_tsdcxo_temp1) > 30000))) {
		mtktspmic_info("%s delta temp too large, drop it!\n", __func__);
		temp1 = pre_tsdcxo_temp1;
	} else {
		/* update previous temp */
		pre_tsdcxo_temp1 = temp1;
		mtktspmic_dprintk(
				"%s pre_dcxo_temp1=%d\n", __func__,
				pre_tsdcxo_temp1);

		if (tsdcxo_cnt == 0)
			tsdcxo_cnt++;
	}

	return temp1;
}
