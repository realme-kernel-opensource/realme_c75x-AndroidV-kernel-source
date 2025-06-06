// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_THERMAL)
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/thermal.h>
#include <linux/types.h>
#endif

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>

#include "apusys_secure.h"
#include "aputop_rpmsg.h"
#include "apu_top.h"
#include "aputop_log.h"
#include "apu_hw_sema.h"
#include "mt6899_apupwr.h"
#include "mt6899_apupwr_prot.h"

#define LOCAL_DBG	(1)
#define RPC_ALIVE_DBG	(0)
/* Below reg_name has to 1-1 mapping DTS's name */
static const char *reg_name[APUPW_MAX_REGS] = {
	"sys_vlp", "sys_spm", "apu_rcx", "apu_rcx_dla", "apu_are",
	"apu_vcore", "apu_md32_mbox", "apu_rpc", "apu_pcu", "apu_ao_ctl",
	"apu_acc", "apu_pll", "apu_rpctop_mdla", "apu_acx0", "apu_acx0_rpc_lite"
};

static struct apu_power apupw = {
	.env = MP,
	.rcx = CE_FW,
};

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_THERMAL)
/*==================================================
 * cooler callback functions
 *==================================================
 */
static int apu_throttle(struct apu_cooling_device *apu_cdev, unsigned long state)
{
	struct aputop_func_param aputop;
	struct device *dev = apu_cdev->dev;

	memset(&aputop, -1, sizeof(struct aputop_func_param));

	if (state > APU_COOLING_UNLIMITED_STATE) {
		aputop.param1 = state; //vpu_max
		aputop.param3 = state; //dla_max
	}

	mt6899_aputop_opp_limit(&aputop, OPP_LIMIT_THERMAL);
	apu_cdev->target_state = state;
	dev_info(dev, "%s: set state %ld done, para= %d\n", apu_cdev->name, state, aputop.param1);
	return 0;
}

static int apu_cooling_get_max_state(struct thermal_cooling_device *cdev,
	unsigned long *state)
{
	struct apu_cooling_device *apu_cdev = cdev->devdata;

	*state = apu_cdev->max_state;

	return 0;
}

static int apu_cooling_get_cur_state(struct thermal_cooling_device *cdev,
	unsigned long *state)
{
	struct apu_cooling_device *apu_cdev = cdev->devdata;

	*state = apu_cdev->target_state;

	return 0;
}

static int apu_cooling_set_cur_state(struct thermal_cooling_device *cdev,
	unsigned long state)
{
	struct apu_cooling_device *apu_cdev = cdev->devdata;
	int ret;

	/* Request state should be less than max_state */
	if (WARN_ON(state > apu_cdev->max_state || !apu_cdev->throttle))
		return -EINVAL;

	if (apu_cdev->target_state == state)
		return 0;

	ret = apu_cdev->throttle(apu_cdev, state);

	return ret;
}

/*==================================================
 * platform data and platform driver callbacks
 *==================================================
 */

static struct thermal_cooling_device_ops apu_cooling_ops = {
	.get_max_state		= apu_cooling_get_max_state,
	.get_cur_state		= apu_cooling_get_cur_state,
	.set_cur_state		= apu_cooling_set_cur_state,
};

static int init_apu_cooling_device(struct device *dev, struct apu_cooling_device *apu_cdev)
{
	struct thermal_cooling_device *cdev;
	char *name = "apu_cooler";
	int ret = 0;

	ret = snprintf(apu_cdev->name, MAX_APU_COOLER_NAME_LEN, "%s", name);
	if (ret <= 0)
		goto init_fail;
	apu_cdev->target_state = APU_COOLING_UNLIMITED_STATE;
	apu_cdev->max_state = APU_COOLING_MAX_STATE;
	apu_cdev->throttle = apu_throttle;
	apu_cdev->dev = dev;

	cdev = thermal_of_cooling_device_register(dev->of_node, apu_cdev->name,
			apu_cdev, &apu_cooling_ops);
	if (IS_ERR(cdev))
		goto init_fail;
	apu_cdev->cdev = cdev;

	dev_info(dev, "register %s done\n", apu_cdev->name);

	return 0;

init_fail:
	return -EINVAL;
}
#endif

static void aputop_dump_reg(enum apupw_reg idx, uint32_t offset, uint32_t size)
{
	char buf[32];
	int ret = 0;

	/* prepare pa address */
	memset(buf, 0, sizeof(buf));
	ret = snprintf(buf, 32, "phys 0x%08lx: ",
			(ulong)(apupw.phy_addr[idx]) + offset);

	/* dump content with pa as prefix */
	if (!ret)
		print_hex_dump(KERN_ERR, buf, DUMP_PREFIX_OFFSET, 16, 4,
			       apupw.regs[idx] + offset, size, true);
}

static int init_reg_base(struct platform_device *pdev)
{
	struct resource *res;
	int idx = 0;

	pr_info("%s %d APUPW_MAX_REGS = %d\n",
			__func__, __LINE__, APUPW_MAX_REGS);

	for (idx = 0; idx < APUPW_MAX_REGS; idx++) {

		res = platform_get_resource_byname(
				pdev, IORESOURCE_MEM, reg_name[idx]);

		if (res == NULL) {
			pr_info("%s: get resource \"%s\" fail\n",
					__func__, reg_name[idx]);
			return -ENODEV;
		}

		pr_info("%s: get resource \"%s\" pass\n",
				__func__, reg_name[idx]);

		apupw.regs[idx] = ioremap(res->start,
				res->end - res->start + 1);

		if (IS_ERR_OR_NULL(apupw.regs[idx])) {
			pr_info("%s: %s remap base fail\n",
					__func__, reg_name[idx]);
			return -ENOMEM;
		}

		pr_info("%s: %s remap base 0x%llx to 0x%llx\n",
				__func__, reg_name[idx],
				res->start, (uint64_t)apupw.regs[idx]);

		apupw.phy_addr[idx] = res->start;
	}

	return 0;
}

#if !APUPW_DUMP_FROM_APMCU
static uint32_t apusys_pwr_smc_call(struct device *dev, uint32_t smc_id,
		uint32_t a2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_APUSYS_CONTROL, smc_id,
			a2, 0, 0, 0, 0, 0, &res);
	if (((int) res.a0) < 0)
		dev_info(dev, "%s: smc call %d return error(%ld)\n",
				__func__,
				smc_id, res.a0);
	return res.a0;
}
#endif

/*
 * APU_PCU_SEMA_CTRL
 * [15:00]      SEMA_KEY_SET    Each bit corresponds to different user.
 * [31:16]      SEMA_KEY_CLR    Each bit corresponds to different user.
 *
 * ctl:
 *      0x1 - acquire hw semaphore
 *      0x0 - release hw semaphore
 */
 #if APU_HW_SEMA_CTRL
static int apu_hw_sema_ctl(struct device *dev, uint32_t sema_offset,
		uint8_t usr_bit, uint8_t ctl, int32_t timeout)
{
	static uint16_t timeout_cnt_max;
	uint16_t timeout_cnt = 0;
	uint8_t ctl_bit = 0;
	int smc_op;

	if (ctl == 0x1) {
		// acquire is set
		ctl_bit = usr_bit;
		smc_op = SMC_HW_SEMA_PWR_CTL_LOCK;

	} else if (ctl == 0x0) {
		// release is clear
		ctl_bit = usr_bit + 16;
		smc_op = SMC_HW_SEMA_PWR_CTL_UNLOCK;

	} else {
		return -1;
	}

	if (log_lvl)
		pr_info("%s ++ usr_bit:%d ctl:%d (0x%08llx = 0x%08x)\n",
				__func__, usr_bit, ctl,
				(uint64_t)(apupw.regs[apu_pcu] + sema_offset),
				apu_readl(apupw.regs[apu_pcu] + sema_offset));

	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			smc_op);
	udelay(1);

	while ((apu_readl(apupw.regs[apu_pcu] + sema_offset) & BIT(ctl_bit))
			>> ctl_bit != ctl) {

		if (timeout >= 0 && timeout_cnt++ >= timeout) {
			pr_info(
			"%s timeout usr_bit:%d ctl:%d rnd:%d (0x%08llx = 0x%08x)\n",
				__func__, usr_bit, ctl, timeout_cnt,
				(uint64_t)(apupw.regs[apu_pcu] + sema_offset),
				apu_readl(apupw.regs[apu_pcu] + sema_offset));

			return -1;
		}

		if (ctl == 0x1) {
			apusys_pwr_smc_call(dev,
					MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
					smc_op);
		}

		udelay(1);
	}

	if (timeout_cnt > timeout_cnt_max)
		timeout_cnt_max = timeout_cnt;

	if (log_lvl)
		pr_info("%s -- usr_bit:%d ctl:%d (0x%08llx = 0x%08x) mx:%d\n",
				__func__, usr_bit, ctl,
				(uint64_t)(apupw.regs[apu_pcu] + sema_offset),
				apu_readl(apupw.regs[apu_pcu] + sema_offset),
				timeout_cnt_max);
	return 0;
}
#endif

/*
 * APU_PCU_SEMA_READ
 * [15:00]      SEMA_KEY_SET    Each bit corresponds to different user.
 */
static uint32_t plat_apu_boot_host(void)
{
	uint32_t host = 0;
	uint32_t sema_offset = APU_HW_SEMA_PWR_CTL;

	host = apu_readl(apupw.regs[apu_pcu] + sema_offset) & 0x0000FFFF;

	if (host == BIT(SYS_APMCU))
		return SYS_APMCU;
	else if  (host == BIT(SYS_APU))
		return SYS_APU;
	else if  (host == BIT(SYS_SCP_LP))
		return SYS_SCP_LP;
	else if  (host == BIT(SYS_SCP_NP))
		return SYS_SCP_NP;
	else
		return SYS_MAX;
}

static void aputop_dump_pwr_reg(struct device *dev)
{
#if APUPW_DUMP_FROM_APMCU
#else
	// dump reg in ATF log
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_DUMP,
			SMC_PWR_DUMP_ALL);
	// dump reg in AEE db
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_REGDUMP, 0);
#endif
}

static void aputop_dump_rpc_data(void)
{
	aputop_dump_reg(apu_rpc, 0x0, 0x20);
}

static void aputop_dump_pcu_data(struct device *dev)
{
	aputop_dump_reg(apu_pcu, 0x0, 0x20);
	aputop_dump_reg(apu_pcu, 0x80, 0x20);

#if APUPW_DUMP_FROM_APMCU
#else
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_DUMP,
			SMC_PWR_DUMP_PCU);
#endif
}

#if APUPW_DUMP_FROM_APMCU
static void aputop_dump_pll_data(void)
{
	// need to 1-1 in order mapping with array in __apu_pll_init func
	uint32_t pll_base_arr[] = {MNOC_PLL_BASE, UP_PLL_BASE};
	uint32_t pll_offset_arr[] = {
				PLL1CPLL_FHCTL_HP_EN, PLL1CPLL_FHCTL_RST_CON,
				PLL1CPLL_FHCTL_CLK_CON, PLL1CPLL_FHCTL0_CFG,
				PLL1C_PLL1_CON1, PLL1CPLL_FHCTL0_DDS};
	int base_arr_size = ARRAY_SIZE(pll_base_arr);
	int offset_arr_size = ARRAY_SIZE(pll_offset_arr);
	int pll_idx;
	int ofs_idx;
	char buf[256];
	int ret = 0;

	for (pll_idx = 0 ; pll_idx < base_arr_size ; pll_idx++) {

		memset(buf, 0, sizeof(buf));

		for (ofs_idx = 0 ; ofs_idx < offset_arr_size ; ofs_idx++) {

			ret = snprintf(buf + strlen(buf),
					sizeof(buf) - strlen(buf),
					" 0x%08x",
					apu_readl(apupw.regs[apu_pll] +
						pll_base_arr[pll_idx] +
						pll_offset_arr[ofs_idx]));
			if (ret <= 0)
				break;
		}

		if (ret <= 0)
			break;

		pr_info("%s pll_base:0x%08x = %s\n", __func__,
				apupw.phy_addr[apu_pll] + pll_base_arr[pll_idx],
				buf);
	}
}
static void aputop_dump_acc_data(void)
{
	// need to 1-1 in order mapping with array in __apu_acc_init func
	uint32_t acc_base_arr[] = {MNOC_ACC_BASE, UP_ACC_BASE};
	uint32_t acc_offset_arr[] = {
				APU_ACC_CONFG_SET0, APU_ACC_CONFG_CLR0};
	int base_arr_size = ARRAY_SIZE(acc_base_arr);
	int offset_arr_size = ARRAY_SIZE(acc_offset_arr);
	int acc_idx;
	int ofs_idx;
	char buf[256];
	int ret = 0;

	for (acc_idx = 0 ; acc_idx < base_arr_size ; acc_idx++) {

		memset(buf, 0, sizeof(buf));

		for (ofs_idx = 0 ; ofs_idx < offset_arr_size ; ofs_idx++) {

			ret = snprintf(buf + strlen(buf),
					sizeof(buf) - strlen(buf),
					" 0x%08x",
					apu_readl(apupw.regs[apu_acc] +
						acc_base_arr[acc_idx] +
						acc_offset_arr[ofs_idx]));
			if (ret <= 0)
				break;
		}

		if (ret <= 0)
			break;

		pr_info("%s acc_base:0x%08x = %s\n", __func__,
				apupw.phy_addr[apu_acc] + acc_base_arr[acc_idx],
				buf);
	}
}

static void mtk_clk_acc_get_rate(void)
{
	int32_t output = 0, i = 0, j = 0;
	uint32_t tempValue = 0;
	bool timeout = false;
	//uint32_t phy_confg_set;
	//uint32_t phy_fm_confg_set, phy_fm_confg_clr, phy_fm_sel, phy_fm_cnt;
	ulong fm_confg_set, fm_confg_clr, fm_sel, fm_cnt;
	uint32_t loop_ref = 0;  // 0 for Max freq  ~ 1074MHz
	int32_t retry = 30;

	uint32_t acc_base_arr[] = {MNOC_ACC_BASE, UP_ACC_BASE};
	uint32_t acc_offset_arr[] = {
				APU_ACC_CONFG_SET0, APU_ACC_FM_SEL, APU_ACC_FM_CONFG_SET,
				APU_ACC_FM_CONFG_CLR, APU_ACC_FM_CNT};

	for (j = 0 ; j < 2 ; j++) {
		fm_sel = (ulong)apupw.regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[1];
		fm_confg_set = (ulong)apupw.regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[2];
		fm_confg_clr = (ulong)apupw.regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[3];
		fm_cnt = (ulong)apupw.regs[apu_acc] + acc_base_arr[j] + acc_offset_arr[4];

		/* reset */
		apu_writel(0x0, (void __iomem *)fm_sel);
		apu_writel(apu_readl((void __iomem *)fm_sel), (void __iomem *)fm_sel);
		apu_writel(apu_readl((void __iomem *)fm_sel) | (loop_ref << 16),
			(void __iomem *)fm_sel);
		apu_writel(BIT(0), (void __iomem *)fm_confg_set);
		apu_writel(BIT(1), (void __iomem *)fm_confg_set);

		/* wait frequency meter finish */
		while (!(apu_readl((void __iomem *)fm_confg_set) & BIT(4))) {
			udelay(10);
			i++;
			if (i > retry) {
				timeout = true;
				pr_info("%s acc error, fm_sel = 0x%08x, fm_confg_set = 0x%08x\n",
					__func__,
					apu_readl((void __iomem *)fm_sel),
					apu_readl((void __iomem *)fm_confg_set));
				break;
			}
		}

		if ((!timeout) &&
			!(apu_readl((void __iomem *)fm_confg_set) & BIT(5))) {
			tempValue = apu_readl((void __iomem *)fm_cnt);
			tempValue = tempValue & 0xFFFFF;
			output = tempValue * 16384 / ((loop_ref + 1) * 1000);  //KHz
		} else {
			output = 0;
		}
		pr_info("%s: MNOC/UP ACC:%d\n", __func__, output);

		apu_writel(BIT(4), (void __iomem *)fm_confg_clr);
		apu_writel(BIT(1), (void __iomem *)fm_confg_clr);
		apu_writel(BIT(0), (void __iomem *)fm_confg_clr);
	}
}
#endif

static int __apu_wake_rpc_rcx(struct device *dev)
{
	int ret = 0, val = 0;

	if (log_lvl)
		dev_info(dev, "%s before wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			 __func__,
			 (u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			 readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

#if APUPW_DUMP_FROM_APMCU
	apu_setl(0x1 << 16, apupw.regs[apu_rpc] + APU_RPC_TOP_SEL_1);
#else
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			SMC_RCX_PWR_AFC_EN);
#endif

#if APUPW_DUMP_FROM_APMCU
	apu_writel(0x00000100, apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
#else
	/* wake up RPC */
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			SMC_RCX_PWR_WAKEUP_RPC);
#endif

	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL), 50, 10000);
	if (ret) {
		pr_info("%s polling RPC RDY timeout, ret %d\n", __func__, ret);
		/* show powerack info */
		dev_info(dev, "%s RCX APU_RPC_PWR_ACK 0x%x = 0x%x\n",
					 __func__,
					 (u32)(apupw.phy_addr[apu_rpc] + APU_RPC_PWR_ACK),
					 readl(apupw.regs[apu_rpc] + APU_RPC_PWR_ACK));
		goto out;
	}

	/*  show this once per 500ms */
	apu_info_ratelimited(dev, "%s after wakeup RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			     __func__,
			     (u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			     readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	/* polling FSM @RPC-lite to ensure RPC is in on/off stage */
	ret |= readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_STATUS_1),
			val, (val & (0x1 << 13)), 50, 10000);

	ret |= readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_PWR_ACK),
			val, (val & 0x3UL), 50, 10000);

	if (ret) {
		pr_info("%s polling ARE FSM timeout, ret %d\n", __func__, ret);
		/* show powerack info */
		dev_info(dev, "%s RCX APU_RPC_PWR_ACK 0x%x = 0x%x\n",
					 __func__,
					 (u32)(apupw.phy_addr[apu_rpc] + APU_RPC_PWR_ACK),
					 readl(apupw.regs[apu_rpc] + APU_RPC_PWR_ACK));
		goto out;
	}

#if APUPW_DUMP_FROM_APMCU
	apu_writel(0xFFFFFFFF, apupw.regs[apu_vcore] + APUSYS_VCORE_CG_CLR);
	apu_writel(0xFFFFFFFF, apupw.regs[apu_rcx] + APU_RCX_CG_CLR);

	mtk_clk_acc_get_rate();

	dev_info(dev, "%s RCX before Spare RG 0x%x = 0x%x\n",
		__func__,
		(u32)(apupw.phy_addr[apu_rcx] + 0x0300),
		readl(apupw.regs[apu_rcx] + 0x0300));

	dev_info(dev, "%s RCX before Spare RG 0x%x = 0x%x\n",
		__func__,
		(u32)(apupw.phy_addr[apu_rcx] + 0x0304),
		readl(apupw.regs[apu_rcx] + 0x0304));

	/* access spare RG */
	apu_writel(0x12345678, apupw.regs[apu_rcx] + 0x0300);
	apu_writel(0x12345678, apupw.regs[apu_rcx] + 0x0304);

	dev_info(dev, "%s RCX after Spare RG 0x%x = 0x%x\n",
		__func__,
		(u32)(apupw.phy_addr[apu_rcx] + 0x0300),
		readl(apupw.regs[apu_rcx] + 0x0300));

	dev_info(dev, "%s RCX after Spare RG 0x%x = 0x%x\n",
		__func__,
		(u32)(apupw.phy_addr[apu_rcx] + 0x0304),
		readl(apupw.regs[apu_rcx] + 0x0304));

	mtk_clk_acc_get_rate();
#else
	/* clear vcore/rcx cgs */
	apusys_pwr_smc_call(dev,
			MTK_APUSYS_KERNEL_OP_APUSYS_PWR_RCX,
			SMC_RCX_PWR_CG_EN);
#endif

out:
	return ret;
}

static int mt6899_apu_top_on(struct device *dev)
{
	int ret = 0;

	if (apupw.env < MP)
		return 0;

	if (log_lvl)
		pr_info("%s +\n", __func__);

	// acquire hw sema
#if APU_HW_SEMA_CTRL
	apu_hw_sema_ctl(dev, APU_HW_SEMA_PWR_CTL, SYS_APMCU, 1, -1);
#endif

	ret = __apu_wake_rpc_rcx(dev);
#if APUPW_DUMP_FROM_APMCU
	aputop_dump_pll_data();
	aputop_dump_acc_data();
#endif
	if (ret) {
		pr_info("%s fail to wakeup RPC, ret %d\n", __func__, ret);
		aputop_dump_pwr_reg(dev);
		aputop_dump_rpc_data();
		aputop_dump_pcu_data(dev);
#if APUPW_DUMP_FROM_APMCU
		aputop_dump_pll_data();
#endif
		if (ret == -EIO)
			apupw_aee_warn("APUSYS_POWER",
					"APUSYS_POWER_RPC_CFG_ERR");
		else
			apupw_aee_warn("APUSYS_POWER",
					"APUSYS_POWER_WAKEUP_FAIL");
		return -1;
	}

	if (log_lvl)
		pr_info("%s -\n", __func__);
	return 0;
}

#if APMCU_REQ_RPC_SLEEP
// backup solution : send request for RPC sleep from APMCU
static int __apu_sleep_rpc_rcx(struct device *dev)
{
	// REG_WAKEUP_CLR
	pr_info("%s step1. set REG_WAKEUP_CLR\n", __func__);
	apu_writel(0x1 << 12, apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	udelay(10);

	// mask RPC IRQ and bypass WFI
	pr_info("%s step2. mask RPC IRQ and bypass WFI\n", __func__);
	apu_setl(1 << 7, apupw.regs[apu_rpc] + APU_RPC_TOP_SEL);
	udelay(10);

	pr_info("%s step3. raise up sleep request.\n", __func__);
	apu_writel(1, apupw.regs[apu_rpc] + APU_RPC_TOP_CON);
	udelay(100);

	dev_info(dev, "%s RCX APU_RPC_INTF_PWR_RDY 0x%x = 0x%x\n",
			__func__,
			(u32)(apupw.phy_addr[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			readl(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY));

	return 0;
}
#endif

static int mt6899_apu_top_off(struct device *dev)
{
	int ret = 0, val = 0;
	int rpc_timeout_val = 500000; // 500 ms

	if (apupw.env < MP)
		return 0;

	if (log_lvl)
		pr_info("%s +\n", __func__);

#if APMCU_REQ_RPC_SLEEP
	__apu_sleep_rpc_rcx(dev);
#endif
	// blocking until sleep success or timeout, delay 50 us per round
	ret = readl_relaxed_poll_timeout_atomic(
			(apupw.regs[apu_rpc] + APU_RPC_INTF_PWR_RDY),
			val, (val & 0x1UL) == 0x0, 50, rpc_timeout_val);
	if (ret) {
		pr_info("%s polling PWR RDY timeout\n", __func__);
	} else {
		ret = readl_relaxed_poll_timeout_atomic(
				(apupw.regs[apu_rpc] + APU_RPC_STATUS),
				val, (val & 0x1UL) == 0x1, 50, 10000);
		if (ret) {
			pr_info("%s polling PWR STATUS timeout\n", __func__);
			return -1;
		}
	}

	if (ret) {
		pr_info(
		"%s timeout to wait RPC sleep (val:%d), ret %d\n", __func__, rpc_timeout_val, ret);
		aputop_dump_pwr_reg(dev);
		aputop_dump_rpc_data();
		aputop_dump_pcu_data(dev);
#if APUPW_DUMP_FROM_APMCU
		aputop_dump_pll_data();
#endif
		apupw_aee_warn("APUSYS_POWER", "APUSYS_POWER_SLEEP_TIMEOUT");
		return -1;
	}

	// release hw sema
#if APU_HW_SEMA_CTRL
	apu_hw_sema_ctl(dev, APU_HW_SEMA_PWR_CTL, SYS_APMCU, 0, -1);
#endif

	if (log_lvl)
		pr_info("%s -\n", __func__);
	return 0;
}

static int mt6899_apu_top_pb(struct platform_device *pdev)
{

	int ret = 0;
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_THERMAL)
	struct device *dev = &pdev->dev;
	struct apu_cooling_device *apu_cdev;

	apu_cdev = devm_kzalloc(dev, sizeof(*apu_cdev), GFP_KERNEL);
	if (!apu_cdev)
		return -ENOMEM;

	ret = init_apu_cooling_device(dev, apu_cdev);
	if (ret) {
		dev_notice(dev, "failed to init apu cooler!\n");
		return ret;
	}

	platform_set_drvdata(pdev, apu_cdev);
#endif

	init_reg_base(pdev);
	if (apupw.env < MP)
		ret = mt6899_all_on(pdev, &apupw);

	/* runtime wake up apu_top, let rv close it */
	pm_runtime_get_sync(&pdev->dev);
	mt6899_init_remote_data_sync(apupw.regs[apu_md32_mbox]);
	return ret;
}

static int mt6899_apu_top_rm(struct platform_device *pdev)
{
	int idx;
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_THERMAL)
	struct apu_cooling_device *apu_cdev;

	apu_cdev = (struct apu_cooling_device *)platform_get_drvdata(pdev);
	thermal_cooling_device_unregister(apu_cdev->cdev);

	platform_set_drvdata(pdev, NULL);
#endif

	pr_info("%s +\n", __func__);

	if (apupw.env < MP)
		mt6899_all_off(pdev);

#if APMCU_REQ_RPC_SLEEP
	pm_runtime_put_sync(&pdev->dev);
#endif
	for (idx = 0; idx < APUPW_MAX_REGS; idx++)
		iounmap(apupw.regs[idx]);
	pr_info("%s -\n", __func__);
	return 0;
}

static int mt6899_apu_top_suspend(struct device *dev)
{
	return 0;
}

static int mt6899_apu_top_resume(struct device *dev)
{
	return 0;
}

static int mt6899_apu_top_func(struct platform_device *pdev,
		enum aputop_func_id func_id, struct aputop_func_param *aputop)
{
	pr_info("%s func_id : %d\n", __func__, aputop->func_id);

	switch (aputop->func_id) {
#if APU_POWER_BRING_UP
	case APUTOP_FUNC_PWR_OFF:
		pm_runtime_put_sync(&pdev->dev);
		break;
	case APUTOP_FUNC_PWR_ON:
		pm_runtime_get_sync(&pdev->dev);
		break;
#endif
	case APUTOP_FUNC_OPP_LIMIT_HAL:
		mt6899_aputop_opp_limit(aputop, OPP_LIMIT_HAL);
		break;
	case APUTOP_FUNC_OPP_LIMIT_DBG:
		mt6899_aputop_opp_limit(aputop, OPP_LIMIT_DEBUG);
		break;
	case APUTOP_FUNC_DUMP_REG:
		aputop_dump_pwr_reg(&pdev->dev);
		break;
	case APUTOP_FUNC_DRV_CFG:
		mt6899_drv_cfg_remote_sync(aputop);
		break;
#if APU_POWER_BRING_UP
	case APUTOP_FUNC_IPI_TEST:
		test_ipi_wakeup_apu();
		break;
#endif
	case APUTOP_FUNC_ARE_DUMP1:
	case APUTOP_FUNC_ARE_DUMP2:
#if APUPW_DUMP_FROM_APMCU
		aputop_dump_reg(apu_are, 0x0, 0x4000);
		aputop_dump_reg(apu_are, 0x10000, 0x4);
#endif
		break;
	case APUTOP_FUNC_BOOT_HOST:
		return plat_apu_boot_host();
	default:
		pr_info("%s invalid func_id : %d\n", __func__, aputop->func_id);
		return -EINVAL;
	}

	return 0;
}

/* call by mt6899_pwr_func.c */
void mt6899_apu_dump_rpc_status(enum t_acx_id id, struct rpc_status_dump *dump)
{
	uint32_t status1 = 0x0;
	uint32_t status2 = 0x0;
	uint32_t status3 = 0x0;

	if (id == D_ACX) { //[Fix me] This is RCX_DLA status
		status1 = apu_readl(apupw.regs[apu_rpctop_mdla]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_rcx_dla]
				+ APU_RCX_MDLA0_CG_CON);
		pr_info("%s D_ACX RPC_PWR_RDY:0x%08x APU_RCX_MDLA0_CG_CON:0x%08x\n",
			__func__, status1, status2);

	} else if (id == ACX0) {
		status1 = apu_readl(apupw.regs[apu_acx0_rpc_lite]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_acx0]
				+ APU_ACX_CONN_CG_CON);
		pr_info("%s ACX0 RPC_PWR_RDY:0x%08x APU_ACX_CONN_CG_CON:0x%08x\n",
			__func__, status1, status2);

	} else {
		status1 = apu_readl(apupw.regs[apu_rpc]
				+ APU_RPC_INTF_PWR_RDY);
		status2 = apu_readl(apupw.regs[apu_vcore]
				+ APUSYS_VCORE_CG_CON);
		status3 = apu_readl(apupw.regs[apu_rcx]
				+ APU_RCX_CG_CON);
		pr_info("%s RCX RPC_PWR_RDY:0x%08x VCORE_CG_CON:0x%08x RCX_CG_CON:0x%08x\n",
			__func__, status1, status2, status3);
		/*
		 * print_hex_dump(KERN_ERR, "rpc: ", DUMP_PREFIX_OFFSET,
		 *              16, 4, apupw.regs[apu_rpc], 0x100, 1);
		 */
	}

	if (!IS_ERR_OR_NULL(dump)) {
		dump->rpc_reg_status = status1;
		dump->conn_reg_status = status2;
		if (id == RCX)
			dump->vcore_reg_status = status3;
	}
}

const struct apupwr_plat_data mt6899_plat_data = {
	.plat_name = "mt6899_apupwr",
	.plat_aputop_on = mt6899_apu_top_on,
	.plat_aputop_off = mt6899_apu_top_off,
	.plat_aputop_pb = mt6899_apu_top_pb,
	.plat_aputop_rm = mt6899_apu_top_rm,
	.plat_aputop_suspend = mt6899_apu_top_suspend,
	.plat_aputop_resume = mt6899_apu_top_resume,
	.plat_aputop_func = mt6899_apu_top_func,
#if IS_ENABLED(CONFIG_DEBUG_FS)
	.plat_aputop_dbg_open = mt6899_apu_top_dbg_open,
	.plat_aputop_dbg_write = mt6899_apu_top_dbg_write,
#endif
	.plat_rpmsg_callback = mt6899_apu_top_rpmsg_cb,
	.bypass_pwr_on = 0,
	.bypass_pwr_off = 0,
};
