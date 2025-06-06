// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Yc Li <yc.li@mediatek.com>
 */

#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mtk-hw-semaphore.h>
#include <mtk-smi-dbg.h>

#define MASTER_MAX_NR		(8)
#define MTCMOS_MAX_NR		(32)
#define MASTER_CELL_NUM		(3)
#define POWER_CELL_NUM		(2)
#define TIMEOUT			(500)

static u32 log_level;
enum hw_sema_log_level {
	log_config = 0,
};

struct semaphore_master {
	u32 sema_type;
	u32 offset;
	u32 set_bit;
};

struct hw_semaphore {
	struct device *dev;
	u32 base_pa[SEMA_TYPE_NR];			//0: spm, 1: vcp
	void __iomem *base[SEMA_TYPE_NR];		//0: spm, 1: vcp
	struct semaphore_master mast[MASTER_MAX_NR];
	u32 dbg_offset[SEMA_TYPE_NR][MASTER_MAX_NR];
};

static struct hw_semaphore *g_hw_sema;

static int __mtk_hw_semaphore_get(void __iomem *addr, u32 set_val)
{
	struct hw_semaphore *hw_sema = g_hw_sema;
	struct device *dev = hw_sema->dev;
	s32 ret, timeout = TIMEOUT;

	if (!addr) {
		dev_notice(dev, "%s: Null base addr found\n", __func__);
		return -EINVAL;
	}
	if ((readl_relaxed(addr) & set_val) == set_val) {
		dev_notice(dev, "%s: hw_sem was already got\n", __func__);
		return -1;
	}

	while ((readl_relaxed(addr) & set_val) != set_val) {
		writel_relaxed(set_val, addr);
		udelay(10);
		ret = 1;
		if (timeout-- < 0) {
			if ((readl_relaxed(addr) & set_val) == set_val)
				return 1;
			dev_notice(dev, "%s: fail!\n", __func__);
			ret = -EAGAIN;
			break;
		}
	}

	return ret;
}

static int __mtk_hw_semaphore_release(void __iomem *addr, u32 set_val)
{
	struct device *dev = g_hw_sema->dev;
	s32 ret = 1, timeout = TIMEOUT;

	if (!addr) {
		dev_notice(dev, "%s: Null base addr found\n", __func__);
		return -EINVAL;
	}
	if ((readl_relaxed(addr) & set_val) != set_val) {
		dev_notice(dev, "%s: hw_sem was already released\n", __func__);
		return -1;
	}

	do {
		writel_relaxed(set_val, addr);
		udelay(10);
		if (timeout-- < 0) {
			dev_notice(dev, "%s: timeout!\n", __func__);
			ret = -EAGAIN;
			break;
		}
	} while ((readl_relaxed(addr) & set_val) == set_val);

	return ret;
}

 /*
  * mtk_hw_semaphore_ctrl() - hw semaphore ctrl
  * master_id: refer to dts
  * is_get: get or release hw semaphore
  * Return 1 on success, or an appropriate error code otherwise.
  */
int mtk_hw_semaphore_ctrl(u32 master_id, bool is_get)
{
	struct hw_semaphore *hw_sema = g_hw_sema;
	struct device *dev = hw_sema->dev;
	u32 sema_type, offset, set_val, i;
	s32 ret;

	sema_type = hw_sema->mast[master_id].sema_type;
	offset = hw_sema->mast[master_id].offset;
	set_val = BIT(hw_sema->mast[master_id].set_bit);

	if (log_level & 1 << log_config)
		dev_notice(dev, "%s: is_get:%d, type:%d, id=%d, offset=%#x, set_val=%#x\n",
				__func__, is_get, sema_type, master_id, offset, set_val);

	if (is_get)
		ret = __mtk_hw_semaphore_get(hw_sema->base[sema_type] + offset, set_val);
	else
		ret = __mtk_hw_semaphore_release(hw_sema->base[sema_type] + offset, set_val);
	if (ret < 0) {
		dev_notice(dev, "%s timeout!: is_get:%d, type:%d, id=%d, offset=%#x, set_val=%#x\n",
				__func__, is_get, sema_type, master_id, offset, set_val);
		for (i = 0; i < MASTER_MAX_NR; i++) {
			offset = hw_sema->dbg_offset[sema_type][i];
			if (!offset)
				continue;
			dev_notice(dev, "%s: master %#x=%#x\n", __func__, offset,
					readl_relaxed(hw_sema->base[sema_type] + offset));
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_hw_semaphore_ctrl);

static const struct smi_hw_sema_ops mtk_smi_hw_sema_ops = {
	.hw_sema_ctrl = mtk_hw_semaphore_ctrl,
};

static int hw_semaphore_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hw_semaphore *hw_sema;
	u32 base_tmp, tmp = 0, i, master_num, offset;

	dev_notice(dev, "%s: start to probe\n", __func__);

	hw_sema = devm_kzalloc(&pdev->dev, sizeof(*hw_sema), GFP_KERNEL);
	if (!hw_sema)
		return -ENOMEM;
	g_hw_sema = hw_sema;
	hw_sema->dev = dev;
	for (i = 0; i < SEMA_TYPE_NR; i++) {
		if (!of_property_read_u32_index(dev->of_node,
						"base", i, &base_tmp)) {
			hw_sema->base_pa[i] = base_tmp;
			hw_sema->base[i] = ioremap(base_tmp, 0x1000);
			dev_notice(dev, "%s:type:%d pa=%#x\n", __func__, i, hw_sema->base_pa[i]);
		}
	}

	if (of_get_property(dev->of_node, "master", &tmp)) {
		master_num = tmp / (sizeof(u32) * MASTER_CELL_NUM);
		for (i = 0; i < master_num; i++) {
			offset = i * MASTER_CELL_NUM;
			if (of_property_read_u32_index(dev->of_node, "master", offset,
					&hw_sema->mast[i].sema_type))
				break;
			if (of_property_read_u32_index(dev->of_node, "master", offset + 1,
					&hw_sema->mast[i].offset))
				break;
			if (of_property_read_u32_index(dev->of_node, "master", offset + 2,
					&hw_sema->mast[i].set_bit))
				break;
			dev_notice(dev, "%s:master:%d,(type,offset,bit)=(%d,%#x,%d)\n", __func__, i,
						hw_sema->mast[i].sema_type,
						hw_sema->mast[i].offset,
						hw_sema->mast[i].set_bit);
		}
	}

	// spm semaphore dbg reg
	for (i = 0; i < MASTER_MAX_NR; i++) {
		if (!of_property_read_u32_index(dev->of_node,
						"spm-sem-dbg-offset", i, &tmp)) {
			hw_sema->dbg_offset[SEMA_TYPE_SPM][i] = tmp;
			dev_notice(dev, "spm-sem-dbg-offset offset=%#x\n",
						hw_sema->dbg_offset[SEMA_TYPE_SPM][i]);
		}
	}

	// vcp semaphore dbg reg
	for (i = 0; i < MASTER_MAX_NR; i++) {
		if (!of_property_read_u32_index(dev->of_node,
						"vcp-sem-dbg-offset", i, &tmp)) {
			hw_sema->dbg_offset[SEMA_TYPE_VCP][i] = tmp;
			dev_notice(dev, "vcp-sem-dbg-offset offset=%#x\n",
						hw_sema->dbg_offset[SEMA_TYPE_VCP][i]);
		}
	}

	mtk_smi_set_hw_sema_ops(&mtk_smi_hw_sema_ops);

	return 0;
}

static const struct of_device_id mtk_hw_semaphore_of_ids[] = {
	{
		.compatible = "mediatek,hw-semaphore",
	},
	{}
};

static struct platform_driver mtk_hw_semaphore_driver = {
	.probe	= hw_semaphore_probe,
	.driver	= {
		.name = "mtk-hw-semaphore",
		.of_match_table = mtk_hw_semaphore_of_ids,
	}
};

static int __init mtk_hw_semaphore_init(void)
{
	s32 status;

	status = platform_driver_register(&mtk_hw_semaphore_driver);
	if (status) {
		pr_notice("Failed to register hw semaphore driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_hw_semaphore_exit(void)
{
	platform_driver_unregister(&mtk_hw_semaphore_driver);
}

module_init(mtk_hw_semaphore_init);
module_exit(mtk_hw_semaphore_exit);

module_param(log_level, uint, 0644);
MODULE_PARM_DESC(log_level, "hw_semaphore log level");

MODULE_DESCRIPTION("MTK HW semaphore driver");
MODULE_LICENSE("GPL");

int hw_semaphore_ut(const char *val, const struct kernel_param *kp)
{
	s32 ret, is_get, master_id;

	ret = sscanf(val, "%d %d", &is_get, &master_id);
	if (ret != 2) {
		pr_notice("%s: fail!, %d\n", __func__, ret);
		return ret;
	}

	ret = mtk_hw_semaphore_ctrl(master_id, is_get);
	if (ret > 0)
		pr_notice("%s:pass,is_get:%d,ret:%d\n", __func__, is_get, ret);
	else
		pr_notice("%s:fail,is_get:%d,ret:%d\n", __func__, is_get, ret);

	return 0;
}

static const struct kernel_param_ops hw_sema_ut_ops = {
	.set = hw_semaphore_ut,
};
module_param_cb(hw_sema_ut, &hw_sema_ut_ops, NULL, 0644);
MODULE_PARM_DESC(hw_sema_ut, "hw semaphore ut by master");
