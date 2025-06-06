// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include "adsp_mbox.h"
#include "adsp_platform.h"
#include "adsp_platform_driver.h"
#include "adsp_core.h"
#include "adsp_clk_audio.h"
#include "adsp_driver_v2.h"
#include "adsp_qos.h"
#include "adsp_ipic.h"
#include "mtk-afe-external.h"
#include "adsp_dbg_dump.h"

const struct adspsys_description mt6983_adspsys_desc = {
	.platform_name = "mt69xx",
	.semaphore_ways = 3,
	.semaphore_ctrl = 2,
	.semaphore_retry = 5000,
	.axibus_idle_val = 0x0,
};

const struct adspsys_description mt6879_adspsys_desc = {
	.platform_name = "mt6879",
	.semaphore_ways = 3,
	.semaphore_ctrl = 2,
	.semaphore_retry = 5000,
	.axibus_idle_val = 0x0,
};

const struct adspsys_description mt6895_adspsys_desc = {
	.platform_name = "mt6895",
	.semaphore_ways = 3,
	.semaphore_ctrl = 2,
	.semaphore_retry = 5000,
	.axibus_idle_val = 0x0,
};

const struct adspsys_description mt6991_adspsys_desc = {
	.platform_name = "mt6991",
	.semaphore_ways = 3,
	.semaphore_ctrl = 2,
	.semaphore_retry = 5000,
	.axibus_idle_val = 0x0,
	.mtcmos_ao_ctrl = 0,
};

const struct adsp_core_description mt6983_adsp_c0_desc = {
	.id = 0,
	.name = "adsp_0",
	.sharedmems = {
		[ADSP_SHAREDMEM_BOOTUP_MARK] = {0x0004, 0x0004},
		[ADSP_SHAREDMEM_SYS_STATUS] = {0x0008, 0x0004},
		[ADSP_SHAREDMEM_MPUINFO] = {0x0028, 0x0020},
		[ADSP_SHAREDMEM_WAKELOCK] = {0x002C, 0x0004},
		[ADSP_SHAREDMEM_IPCBUF] = {0x0300, 0x0200},
		[ADSP_SHAREDMEM_C2C_0_BUF] = {0x2508, 0x2208}, //common begin
		[ADSP_SHAREDMEM_C2C_BUFINFO] = {0x2510, 0x0008},
		[ADSP_SHAREDMEM_TIMESYNC] = {0x2530, 0x0020},
		[ADSP_SHAREDMEM_DVFSSYNC] = {0x253C, 0x000C},
		[ADSP_SHAREDMEM_SLEEPSYNC] = {0x2540, 0x0004},
		[ADSP_SHAREDMEM_BUS_MON_DUMP] = {0x25FC, 0x00BC},
		[ADSP_SHAREDMEM_INFRA_BUS_DUMP] = {0x269C, 0x00A0},
		[ADSP_SHAREDMEM_LATMON_DUMP] = {0x26B8, 0x001C},
	},
	.ops = {
		.initialize = adsp_core0_init,
		.after_bootup = adsp_after_bootup,
	}
};

const struct adsp_core_description mt6983_adsp_c1_desc = {
	.id = 1,
	.name = "adsp_1",
	.sharedmems = {
		[ADSP_SHAREDMEM_BOOTUP_MARK] = {0x0004, 0x0004},
		[ADSP_SHAREDMEM_SYS_STATUS] = {0x0008, 0x0004},
		[ADSP_SHAREDMEM_MPUINFO] = {0x0028, 0x0020},
		[ADSP_SHAREDMEM_WAKELOCK] = {0x002C, 0x0004},
		[ADSP_SHAREDMEM_IPCBUF] = {0x0300, 0x0200},
		[ADSP_SHAREDMEM_C2C_1_BUF] = {0x2508, 0x2208}, //common begin
	},
	.ops = {
		.initialize = adsp_core1_init,
		.after_bootup = adsp_after_bootup,
	}
};

const struct adsp_core_description mt6879_adsp_c0_desc = {
	.id = 0,
	.name = "adsp_0",
	.sharedmems = {
		[ADSP_SHAREDMEM_BOOTUP_MARK] = {0x0004, 0x0004},
		[ADSP_SHAREDMEM_SYS_STATUS] = {0x0008, 0x0004},
		[ADSP_SHAREDMEM_MPUINFO] = {0x0028, 0x0020},
		[ADSP_SHAREDMEM_WAKELOCK] = {0x002C, 0x0004},
		[ADSP_SHAREDMEM_IPCBUF] = {0x0300, 0x0200},
		[ADSP_SHAREDMEM_TIMESYNC] = {0x2530, 0x0020},
		[ADSP_SHAREDMEM_DVFSSYNC] = {0x253C, 0x000C},
		[ADSP_SHAREDMEM_SLEEPSYNC] = {0x2540, 0x0004},
		[ADSP_SHAREDMEM_BUS_MON_DUMP] = {0x25FC, 0x00BC},
		[ADSP_SHAREDMEM_INFRA_BUS_DUMP] = {0x269C, 0x00A0},
		[ADSP_SHAREDMEM_LATMON_DUMP] = {0x26B8, 0x001C},
	},
	.ops = {
		.initialize = adsp_core0_init,
		.after_bootup = adsp_after_bootup,
	}
};

static const struct of_device_id adspsys_of_ids[] = {
	{ .compatible = "mediatek,mt6983-adspsys", .data = &mt6983_adspsys_desc},
	{ .compatible = "mediatek,mt6879-adspsys", .data = &mt6879_adspsys_desc},
	{ .compatible = "mediatek,mt6895-adspsys", .data = &mt6895_adspsys_desc},
	{ .compatible = "mediatek,mt6985-adspsys", .data = &mt6983_adspsys_desc},
	{ .compatible = "mediatek,mt6886-adspsys", .data = &mt6879_adspsys_desc},
	{ .compatible = "mediatek,mt6897-adspsys", .data = &mt6983_adspsys_desc},
	{ .compatible = "mediatek,mt6989-adspsys", .data = &mt6983_adspsys_desc},
	{ .compatible = "mediatek,mt6991-adspsys", .data = &mt6991_adspsys_desc},
	{ .compatible = "mediatek,mt6899-adspsys", .data = &mt6983_adspsys_desc},
	{}
};

static const struct of_device_id adsp_core_of_ids[] = {
	{ .compatible = "mediatek,mt6983-adsp_core_0", .data = &mt6983_adsp_c0_desc},
	{ .compatible = "mediatek,mt6983-adsp_core_1", .data = &mt6983_adsp_c1_desc},
	{ .compatible = "mediatek,mt6879-adsp_core_0", .data = &mt6879_adsp_c0_desc},
	{ .compatible = "mediatek,mt6895-adsp_core_0", .data = &mt6983_adsp_c0_desc},
	{ .compatible = "mediatek,mt6895-adsp_core_1", .data = &mt6983_adsp_c1_desc},
	{ .compatible = "mediatek,mt6985-adsp_core_0", .data = &mt6983_adsp_c0_desc},
	{ .compatible = "mediatek,mt6985-adsp_core_1", .data = &mt6983_adsp_c1_desc},
	{ .compatible = "mediatek,mt6886-adsp_core_0", .data = &mt6879_adsp_c0_desc},
	{ .compatible = "mediatek,mt6897-adsp_core_0", .data = &mt6983_adsp_c0_desc},
	{ .compatible = "mediatek,mt6897-adsp_core_1", .data = &mt6983_adsp_c1_desc},
	{ .compatible = "mediatek,mt6989-adsp_core_0", .data = &mt6983_adsp_c0_desc},
	{ .compatible = "mediatek,mt6989-adsp_core_1", .data = &mt6983_adsp_c1_desc},
	{ .compatible = "mediatek,mt6991-adsp_core_0", .data = &mt6983_adsp_c0_desc},
	{ .compatible = "mediatek,mt6991-adsp_core_1", .data = &mt6983_adsp_c1_desc},
	{ .compatible = "mediatek,mt6899-adsp_core_0", .data = &mt6983_adsp_c0_desc},
	{ .compatible = "mediatek,mt6899-adsp_core_1", .data = &mt6983_adsp_c1_desc},
	{}
};

static const struct of_device_id adsp_qos_scene_of_ids[] = {
	{ .compatible = "mediatek,mt6897-audio-dsp-hrt-bw"},
	{ .compatible = "mediatek,mt6989-audio-dsp-hrt-bw"},
	{ .compatible = "mediatek,mt6991-audio-dsp-hrt-bw"},
	{ .compatible = "mediatek,mt6899-audio-dsp-hrt-bw"},
	{},
};

static const struct of_device_id adsp_slp_prot_of_ids[] = {
	{ .compatible = "mediatek,mt6991-adsp-slp-prot"},
	{},
};

static int adspsys_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	struct adspsys_priv *adspsys;

	/* create private data */
	adspsys = devm_kzalloc(dev, sizeof(*adspsys), GFP_KERNEL);
	if (!adspsys)
		return -ENOMEM;

	match = of_match_node(adspsys_of_ids, dev->of_node);
	if (!match)
		return -ENODEV;

	adspsys->desc = (struct adspsys_description *)match->data;
	adspsys->dev = dev;

	/* get resource from platform_device */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	adspsys->cfg = devm_ioremap_resource(dev, res);
	adspsys->cfg_size = resource_size(res);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	adspsys->cfg2 = devm_ioremap_resource(dev, res);
	adspsys->cfg2_size = resource_size(res);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cfg3");
	if (!res)
		pr_info("%s, \"cfg3\" not support\n", __func__);
	else {
		adspsys->cfg3 = devm_ioremap_resource(dev, res);
		adspsys->cfg3_size = resource_size(res);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "infracfg_rsv");
	if (!res)
		pr_info("%s(), \"infracfg_rsv\" not support\n", __func__);
	else {
		adspsys->infracfg_rsv = devm_ioremap(dev, res->start, resource_size(res));
		if (unlikely(!adspsys->infracfg_rsv))
			pr_warn("%s(), fail to ioremp 0x%llx\n", __func__, res->start);
	}

	/* adsp bus probe */
	ret = adsp_qos_probe(pdev);
	if (ret) {
		pr_warn("%s(), qos probe fail, %d\n", __func__, ret);
		goto ERROR;
	}

	/* adsp ipic probe */
	ret = adsp_ipic_probe(pdev);
	if (ret)
		pr_warn("%s(), ipic probe fail, %d\n", __func__, ret);

	ret = of_property_read_u32(dev->of_node, "slp-prot-ctrl",
				   &adspsys->slp_prot_ctrl);
	if (ret) {
		pr_info("%s(), get slp-prot-ctrl fail\n", __func__);
		adspsys->slp_prot_ctrl = 0;
	}

	ret = of_property_read_u32(dev->of_node, "sram-sleep-mode-ctrl",
				   &adspsys->sram_sleep_mode_ctrl);
	if (ret) {
		pr_info("%s(), get sram-sleep-mode-ctrl fail\n", __func__);
		adspsys->sram_sleep_mode_ctrl = 0;
	}

	of_property_read_u32(dev->of_node, "core-num", &adspsys->num_cores);

	ret = of_property_read_u32(dev->of_node, "system-l2sram",
				   &adspsys->system_l2sram);
	if (ret)
		pr_info("%s, system l2sram not support %d\n", __func__, adspsys->system_l2sram);

	ret = adsp_clk_probe(pdev, &adspsys->clk_ops);
	if (ret) {
		pr_warn("%s(), clk probe fail, %d\n", __func__, ret);
		goto ERROR;
	}

	ret = adsp_mem_device_probe(pdev);
	if (ret) {
		pr_info("%s(), memory probe fail, %d\n", __func__, ret);
		goto ERROR;
	}

	ret = adsp_mbox_probe(pdev);
	if (ret) {
		pr_warn("%s(), mbox probe fail, %d\n", __func__, ret);
		goto ERROR;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev))
		pr_warn("%s(), pm_runtime_enable fail, %d\n", __func__, ret);

	if (adsp_dbg_dump_init(pdev))
		pr_info("%s(), adsp_dbg_dump_init, %d\n", __func__, ret);

	/* register as syscore_device, not to be turned off when suspend */
	dev_pm_syscore_device(&pdev->dev, true);

	/* register syscore if adsp on infra */
	//register_syscore_ops(&adsp_syscore_ops);

	register_adspsys(adspsys);

	switch_adsp_power(true);

	pr_info("%s, success\n", __func__);
ERROR:
	return ret;
}

static int adspsys_drv_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static int adsp_core_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct adsp_priv *pdata;
	const struct adsp_core_description *desc;
	const struct of_device_id *match;
	struct of_phandle_args spec;
	u64 system_info[2];

	/* create private data */
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	match = of_match_node(adsp_core_of_ids, dev->of_node);
	if (!match)
		return -ENODEV;

	desc = (struct adsp_core_description *)match->data;

	pdata->id = desc->id;
	pdata->name = desc->name;
	pdata->ops = &desc->ops;
	pdata->mapping_table = desc->sharedmems;

	pdata->dev = dev;
	init_completion(&pdata->done);

	/* get resource from platform_device */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pdata->itcm = devm_ioremap_resource(dev, res);
	pdata->itcm_size = resource_size(res);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	pdata->dtcm = devm_ioremap_resource(dev, res);
	pdata->dtcm_size = resource_size(res);

	pdata->irq[ADSP_IRQ_IPC_ID].seq = platform_get_irq(pdev, 0);
	pdata->irq[ADSP_IRQ_IPC_ID].clear_irq = adsp_mt_clr_sysirq;
	pdata->irq[ADSP_IRQ_WDT_ID].seq = platform_get_irq(pdev, 1);
	pdata->irq[ADSP_IRQ_WDT_ID].clear_irq = adsp_mt_disable_wdt;
	pdata->irq[ADSP_IRQ_AUDIO_ID].seq = platform_get_irq(pdev, 2);
	pdata->irq[ADSP_IRQ_AUDIO_ID].clear_irq = adsp_mt_clr_auidoirq;

	/* get l2sram resource from platform_device */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (res) {
		pdata->l2sram = devm_ioremap_resource(dev, res);
		pdata->l2sram_size = resource_size(res);
	} else
		pr_info("%s, l2sram not support\n", __func__);

	of_property_read_u64_array(dev->of_node, "system", system_info, 2);
	pdata->sysram_phys = (phys_addr_t)system_info[0];
	pdata->sysram_size = (size_t)system_info[1];

	if (pdata->sysram_phys == 0 || pdata->sysram_size == 0)
		return -ENODEV;
	pdata->sysram = ioremap_wc(pdata->sysram_phys, pdata->sysram_size);

	of_property_read_u64(dev->of_node, "feature-control-bits",
			     &pdata->feature_set);

	/* mailbox channel parsing */
	if (of_parse_phandle_with_args(dev->of_node, "mboxes",
				       "#mbox-cells", 0, &spec)) {
		dev_dbg(dev, "%s: can't parse \"mboxes\" property\n", __func__);
		return -ENODEV;
	}
	pdata->send_mbox = get_adsp_mbox_pin_send(spec.args[0]);

	if (of_parse_phandle_with_args(dev->of_node, "mboxes",
				       "#mbox-cells", 1, &spec)) {
		dev_dbg(dev, "%s: can't parse \"mboxes\" property\n", __func__);
		return -ENODEV;
	}
	pdata->recv_mbox = get_adsp_mbox_pin_recv(spec.args[0]);

	/* add to adsp_core list */
	register_adsp_core(pdata);

	pr_info("%s, id:%d success\n", __func__, pdata->id);
	return ret;
}

static int adsp_core_drv_remove(struct platform_device *pdev)
{
	return 0;
}

static int adsp_qos_scene_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;

	match = of_match_node(adsp_qos_scene_of_ids, dev->of_node);
	if (match)
		adsp_set_scene_bw(pdev);
	else
		pr_info("%s() no qos scene supported\n", __func__);

	return 0;
}

static int adsp_qos_scene_remove(struct platform_device *pdev)
{
	return 0;
}

static int adsp_pd_event(struct notifier_block *nb,
				  unsigned long flags , void *data)
{
	switch (flags) {
	case GENPD_NOTIFY_ON:
		adsp_set_slp_prot(false, ADSP_INFRA);
		break;
	case GENPD_NOTIFY_PRE_OFF:
		adsp_set_slp_prot(true, ADSP_INFRA);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block adsp_pd_notifier_block = {
	.notifier_call = adsp_pd_event,
	.priority = 0,
};

static int adsp_slp_prot_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev))
		pr_info("%s(), pm_runtime_enable fail, %d\n", __func__, ret);

	ret = dev_pm_genpd_add_notifier(dev, &adsp_pd_notifier_block);
	if (ret)
		pr_info("%s() register pd notifier fail %d", __func__, ret);

	return ret;
}

static int adsp_slp_prot_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver adspsys_driver = {
	.probe = adspsys_drv_probe,
	.remove = adspsys_drv_remove,
	.driver = {
		.name = "adspsys",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = adspsys_of_ids,
#endif
	},
};

static struct platform_driver adsp_core0_driver = {
	.probe = adsp_core_drv_probe,
	.remove = adsp_core_drv_remove,
	.driver = {
		.name = "adsp-core0",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = adsp_core_of_ids,
#endif
	},
};

static struct platform_driver adsp_core1_driver = {
	.probe = adsp_core_drv_probe,
	.remove = adsp_core_drv_remove,
	.driver = {
		.name = "adsp-core1",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = adsp_core_of_ids,
#endif
	},
};

static struct platform_driver adsp_qos_scene_driver = {
	.probe = adsp_qos_scene_probe,
	.remove = adsp_qos_scene_remove,
	.driver = {
		.name = "audio-dsp-hrt-bw",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = adsp_qos_scene_of_ids,
#endif
	},
};

static struct platform_driver adsp_slp_prot_driver = {
	.probe = adsp_slp_prot_probe,
	.remove = adsp_slp_prot_remove,
	.driver = {
		.name = "adsp-slp-prot",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = adsp_slp_prot_of_ids,
#endif
	},
};

static struct platform_driver * const drivers[] = {
	&adspsys_driver,
	&adsp_core0_driver,
	&adsp_core1_driver,
	&adsp_qos_scene_driver,
	&adsp_slp_prot_driver,
};

int notify_adsp_semaphore_event(struct notifier_block *nb,
				unsigned long event, void *v)
{
	int status = NOTIFY_DONE;

	if (event == NOTIFIER_ADSP_3WAY_SEMAPHORE_GET) {
		status = (get_adsp_semaphore(SEMA_AUDIOREG) == ADSP_OK) ?
			 NOTIFY_STOP : NOTIFY_BAD;
	} else if (event == NOTIFIER_ADSP_3WAY_SEMAPHORE_RELEASE) {
		release_adsp_semaphore(SEMA_AUDIOREG);
		status = NOTIFY_STOP;
	}

	return status;
}

static struct notifier_block adsp_semaphore_init_notifier = {
	.notifier_call = notify_adsp_semaphore_event,
};

/*
 * driver initialization entry point
 */
static int __init platform_adsp_init(void)
{
	int ret = 0;

	ret = platform_register_drivers(drivers, ARRAY_SIZE(drivers));
	if (ret)
		return ret;

	register_3way_semaphore_notifier(&adsp_semaphore_init_notifier);

	adsp_system_bootup();
	return 0;
}

static void __exit platform_adsp_exit(void)
{
	unregister_3way_semaphore_notifier(&adsp_semaphore_init_notifier);
	platform_unregister_drivers(drivers, ARRAY_SIZE(drivers));
	pr_info("[ADSP] platform-adsp Exit.\n");
}

module_init(platform_adsp_init);
module_exit(platform_adsp_exit);

MODULE_AUTHOR("Chien-Wei Hsu <Chien-Wei.Hsu@mediatek.com>");
MODULE_DESCRIPTION("MTK AUDIO DSP PLATFORM Device Driver");
MODULE_LICENSE("GPL v2");

