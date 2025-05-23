// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>

#include <mtk_cpupm_dbg.h>
#include <lpm_dbg_common_v1.h>
#include <lpm_module.h>

#include <lpm_dbg_fs_common.h>
#include <lpm_dbg_logger.h>

#include <lpm_trace_event.h>
#include <dbg_fs.h>
#include <power_gs_compare.h>
#include <lpm_logger.h>

static int __init dbg_early_initcall(void)
{
	return 0;
}
#ifndef MTK_LPM_MODE_MODULE
subsys_initcall(dbg_early_initcall);
#endif

static int __init dbg_device_initcall(void)
{
	mt6781_dbg_ops_register();
	lpm_dbg_common_fs_init();
	mt6781_dbg_fs_init();
	mtk_cpupm_dbg_init();
#if IS_ENABLED(CONFIG_MTK_LPM_GS_DUMP_SUPPORT)
	mt6781_power_gs_init();
#endif
	return 0;
}

static int __init dbg_late_initcall(void)
{
	mt6781_lpm_trace_init();
	lpm_logger_init();
	return 0;
}
#ifndef MTK_LPM_MODE_MODULE
late_initcall_sync(dbg_late_initcall);
#endif

int __init mt6781_dbg_init(void)
{
	int ret = 0;
#ifdef MTK_LPM_MODE_MODULE
	ret = dbg_early_initcall();
#endif
	if (ret)
		goto dbg_init_fail;

	ret = dbg_device_initcall();

	if (ret)
		goto dbg_init_fail;

#ifdef MTK_LPM_MODE_MODULE
	ret = dbg_late_initcall();
#endif

	if (ret)
		goto dbg_init_fail;

	ret = lpm_dbg_pm_init();

	if (ret)
		goto dbg_init_fail;

	return 0;
dbg_init_fail:
	return -EAGAIN;
}

void __exit mt6781_dbg_exit(void)
{
#if IS_ENABLED(CONFIG_MTK_LPM_GS_DUMP_SUPPORT)
	mt6781_power_gs_deinit();
#endif
	lpm_dbg_pm_exit();
	lpm_dbg_common_fs_exit();
	mt6781_dbg_fs_exit();
	mtk_cpupm_dbg_exit();
	mt6781_lpm_trace_deinit();
	lpm_logger_deinit();
}

module_init(mt6781_dbg_init);
module_exit(mt6781_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MT6781 Low Power FileSystem");
MODULE_AUTHOR("MediaTek Inc.");
