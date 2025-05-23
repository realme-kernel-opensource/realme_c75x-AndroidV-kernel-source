// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mt6761-pg.h"

#include <dt-bindings/clock/mt6761-clk.h>

/*#define TOPAXI_PROTECT_LOCK*/
/* need to ignore for bring up */
#ifdef CONFIG_FPGA_EARLY_PORTING
#define IGNORE_MTCMOS_CHECK
#endif
#if !defined(MT_CCF_DEBUG) \
		|| !defined(CLK_DEBUG) || !defined(DUMMY_REG_TEST)
#define MT_CCF_DEBUG	0
#define CONTROL_LIMIT	0
#define DUMMY_REG_TEST	0
#define SUBSYS_IF_ON	0
#define MTCMOS_FORCE_OFF 0
#endif

#define	CHECK_PWR_ST	1

#define CONN_TIMEOUT_RECOVERY	5
#define CONN_TIMEOUT_STEP1	4

#ifndef GENMASK
#define GENMASK(h, l)	(((U32_C(1) << ((h) - (l) + 1)) - 1) << (l))
#endif

#ifdef CONFIG_ARM64
#define IOMEM(a)	((void __force __iomem *)((a)))
#endif

#define mt_reg_sync_writel(v, a) \
	do { \
		__raw_writel((v), IOMEM(a)); \
		/* sync up */ \
		mb(); } \
while (0)
#define spm_read(addr)			__raw_readl(IOMEM(addr))
#define spm_write(addr, val)		mt_reg_sync_writel(val, addr)

#define clk_writel(addr, val)   \
	mt_reg_sync_writel(val, addr)

#define clk_readl(addr)			__raw_readl(IOMEM(addr))

/*
 * MTCMOS
 */

#define STA_POWER_DOWN	0
#define STA_POWER_ON	1
#define SUBSYS_PWR_DOWN		0
#define SUBSYS_PWR_ON		1

struct subsys;

struct subsys_ops {
	int (*prepare)(struct subsys *sys);
	int (*unprepare)(struct subsys *sys);
	int (*enable)(struct subsys *sys);
	int (*disable)(struct subsys *sys);
	int (*get_state)(struct subsys *sys);
};

struct subsys {
	const char *name;
	uint32_t sta_mask;
	void __iomem *ctl_addr;
	uint32_t sram_pdn_bits;
	uint32_t sram_pdn_ack_bits;
	uint32_t bus_prot_mask;
	struct subsys_ops *ops;
};

static DEFINE_SPINLOCK(clk_ops_lock);

#define mtk_clk_lock(flags)	spin_lock_irqsave(&clk_ops_lock, flags)
#define mtk_clk_unlock(flags)	\
	spin_unlock_irqrestore(&clk_ops_lock, flags)

/*static struct subsys_ops general_sys_ops;*/

static struct subsys_ops MD1_sys_ops;
static struct subsys_ops CONN_sys_ops;
static struct subsys_ops DPY_sys_ops;
static struct subsys_ops DIS_sys_ops;
static struct subsys_ops MFG_sys_ops;
static struct subsys_ops IFR_sys_ops;
static struct subsys_ops MFG_CORE0_sys_ops;
static struct subsys_ops MFG_ASYNC_sys_ops;
static struct subsys_ops CAM_sys_ops;
static struct subsys_ops VCODEC_sys_ops;

static void __iomem *infracfg_base;/* infracfg_ao */
static void __iomem *spm_base;/* spm */
static void __iomem *infra_base;/* infra */
static void __iomem *smi_common_base;/* smi_common */
static void __iomem *conn_base;/* connsys */

#define INFRACFG_REG(offset)		(infracfg_base + offset)
#define SPM_REG(offset)			(spm_base + offset)
#define INFRA_REG(offset)		(infra_base + offset)
#define SMI_COMMON_REG(offset)	(smi_common_base + offset)
#define CONN_HIF_REG(offset)	(conn_base + offset)

/* Define MTCMOS power control */
#define PWR_RST_B			(0x1 << 0)
#define PWR_ISO				(0x1 << 1)
#define PWR_ON				(0x1 << 2)
#define PWR_ON_2ND			(0x1 << 3)
#define PWR_CLK_DIS			(0x1 << 4)
#define SRAM_CKISO			(0x1 << 5)
#define SRAM_ISOINT_B			(0x1 << 6)

/**************************************
 * for non-CPU MTCMOS
 **************************************/
#define POWERON_CONFIG_EN		SPM_REG(0x0000)
#define SPM_POWER_ON_VAL0		SPM_REG(0x0004)
#define SPM_POWER_ON_VAL1		SPM_REG(0x0008)
#define PWR_STATUS			SPM_REG(0x0180)
#define PWR_STATUS_2ND			SPM_REG(0x0184)
#define VCODEC_PWR_CON			SPM_REG(0x0300)
#define ISP_PWR_CON			SPM_REG(0x0308)
#define DIS_PWR_CON			SPM_REG(0x030C)
#define MFG_CORE1_PWR_CON		SPM_REG(0x0310)
#define AUDIO_PWR_CON			SPM_REG(0x0314)
#define IFR_PWR_CON			SPM_REG(0x0318)
#define DPY_PWR_CON			SPM_REG(0x031C)
#define MD1_PWR_CON			SPM_REG(0x0320)
#define VPU_TOP_PWR_CON			SPM_REG(0x0324)
#define CONN_PWR_CON			SPM_REG(0x032C)
#define VPU_CORE2_PWR_CON		SPM_REG(0x0330)
#define MFG_ASYNC_PWR_CON		SPM_REG(0x0334)
#define MFG_PWR_CON			SPM_REG(0x0338)
#define VPU_CORE0_PWR_CON		SPM_REG(0x033C)
#define VPU_CORE1_PWR_CON		SPM_REG(0x0340)
#define CAM_PWR_CON			SPM_REG(0x0344)
#define MFG_2D_PWR_CON			SPM_REG(0x0348)
#define MFG_CORE0_PWR_CON		SPM_REG(0x034C)
#define VDE_PWR_CON			SPM_REG(0x0370)
#define MD_SRAM_ISO_CON		SPM_REG(0x0394)
#define MD_EXTRA_PWR_CON		SPM_REG(0x0398)

#define  SPM_PROJECT_CODE		0xB16

#define INFRA_TOPAXI_SI0_CTL		INFRACFG_REG(0x0200)
#define INFRA_TOPAXI_PROTECTEN		INFRACFG_REG(0x0220)
#define INFRA_TOPAXI_PROTECTEN_STA0	INFRACFG_REG(0x0224)

#define INFRA_TOPAXI_PROTECTEN_1	INFRACFG_REG(0x0250)

#define INFRA_TOPAXI_PROTECTEN_SET	INFRACFG_REG(0x02A0)
#define INFRA_TOPAXI_PROTECTEN_STA1	INFRACFG_REG(0x0228)
#define INFRA_TOPAXI_PROTECTEN_CLR	INFRACFG_REG(0x02A4)

#define INFRA_TOPAXI_PROTECTEN_1_SET	INFRACFG_REG(0x02A8)
#define INFRA_TOPAXI_PROTECTEN_STA0_1	INFRACFG_REG(0x0254)
#define INFRA_TOPAXI_PROTECTEN_STA1_1	INFRACFG_REG(0x0258)
#define INFRA_TOPAXI_PROTECTEN_1_CLR	INFRACFG_REG(0x02AC)

/* fix with infra config address */
#define INFRA_TOPAXI_SI0_STA		INFRA_REG(0x0000)
#define INFRA_TOPAXI_SI1_STA		INFRA_REG(0x0004)
#define INFRA_TOPAXI_SI2_STA		INFRA_REG(0x0028)
#define INFRA_TOPAXI_SI3_STA		INFRA_REG(0x002C)
#define INFRA_TOPAXI_SI4_STA		INFRA_REG(0x0030)
#define INFRA_TOPAXI_MI_STA		INFRA_REG(0x0008)
#define INFRA_MCI_SI0_STA		INFRA_REG(0x0010)
#define INFRA_MCI_SI2_STA		INFRA_REG(0x0018)
#define INFRA_BUS_IDLE_STA4		INFRA_REG(0x018C)


/* SMI COMMON */
#define SMI_COMMON_SMI_CLAMP		SMI_COMMON_REG(0x03C0)
#define SMI_COMMON_SMI_CLAMP_SET	SMI_COMMON_REG(0x03C4)
#define SMI_COMMON_SMI_CLAMP_CLR	SMI_COMMON_REG(0x03C8)

/* CONN HIF */
#define CONN_HIF_TOP_MISC		CONN_HIF_REG(0x0104)
#define CONN_HIF_DBG_IDX		CONN_HIF_REG(0x012C)
#define CONN_HIF_DBG_PROBE		CONN_HIF_REG(0x0130)
#define CONN_HIF_BUSY_STATUS		CONN_HIF_REG(0x0138)

/* Define MTCMOS Bus Protect Mask */
#define MD1_PROT_STEP1_0_MASK			((0x1 << 7))
#define MD1_PROT_STEP1_0_ACK_MASK		((0x1 << 7))
#define MD1_PROT_STEP2_0_MASK			((0x1 << 3)	\
							|(0x1 << 4))
#define MD1_PROT_STEP2_0_ACK_MASK		((0x1 << 3)	\
							|(0x1 << 4))
#define MD1_PROT_STEP2_1_MASK			((0x1 << 6))
#define MD1_PROT_STEP2_1_ACK_MASK			((0x1 << 6))
#define CONN_PROT_STEP1_0_MASK			((0x1 << 13))
#define CONN_PROT_STEP1_0_ACK_MASK			((0x1 << 13))
#define CONN_PROT_STEP1_1_MASK			((0x1 << 18))
#define CONN_PROT_STEP1_1_ACK_MASK			((0x1 << 18))
#define CONN_PROT_STEP2_0_MASK		((0x1 << 14))
#define CONN_PROT_STEP2_0_ACK_MASK	((0x1 << 14))
#define CONN_PROT_STEP2_1_MASK			((0x1 << 21))
#define CONN_PROT_STEP2_1_ACK_MASK		((0x1 << 21))
#define DPY_PROT_STEP1_0_MASK			((0x1 << 0)	\
							|(0x1 << 23)	\
							|(0x1 << 26))
#define DPY_PROT_STEP1_0_ACK_MASK		((0x1 << 0)	\
							|(0x1 << 23)	\
							|(0x1 << 26))
#define DPY_PROT_STEP1_1_MASK			((0x1 << 10)	\
							|(0x1 << 11)	\
							|(0x1 << 12)	\
							|(0x1 << 13)	\
							|(0x1 << 14)	\
							|(0x1 << 15)	\
							|(0x1 << 16)	\
							|(0x1 << 17))
#define DPY_PROT_STEP1_1_ACK_MASK		((0x1 << 10)	\
							|(0x1 << 11)	\
							|(0x1 << 12)	\
							|(0x1 << 13)	\
							|(0x1 << 14)	\
							|(0x1 << 15)	\
							|(0x1 << 16)	\
							|(0x1 << 17))
#define DPY_PROT_STEP2_0_MASK			((0x1 << 1)	\
							|(0x1 << 2)	\
							|(0x1 << 3)	\
							|(0x1 << 4)	\
							|(0x1 << 10)	\
							|(0x1 << 11)	\
							|(0x1 << 21)	\
							|(0x1 << 22))
#define DPY_PROT_STEP2_0_ACK_MASK		((0x1 << 1)	\
							|(0x1 << 2)	\
							|(0x1 << 3)	\
							|(0x1 << 4)	\
							|(0x1 << 10)	\
							|(0x1 << 11)	\
							|(0x1 << 21)	\
							|(0x1 << 22))
#define DIS_PROT_STEP1_0_MASK			((0x1 << 16)	\
							|(0x1 << 17))
#define DIS_PROT_STEP1_0_ACK_MASK		((0x1 << 16)	\
							|(0x1 << 17))
#define DIS_PROT_STEP2_0_MASK			((0x1 << 1)	\
							|(0x1 << 2)	\
							|(0x1 << 10)	\
							|(0x1 << 11))
#define DIS_PROT_STEP2_0_ACK_MASK		((0x1 << 1)	\
							|(0x1 << 2)	\
							|(0x1 << 10)	\
							|(0x1 << 11))
#define MFG_PROT_STEP1_0_MASK			((0x1 << 25))
#define MFG_PROT_STEP1_0_ACK_MASK		((0x1 << 25))
#define MFG_PROT_STEP2_0_MASK			((0x1 << 21)	\
							|(0x1 << 22))
#define MFG_PROT_STEP2_0_ACK_MASK		((0x1 << 21)	\
							|(0x1 << 22))
#define CAM_PROT_STEP1_0_MASK			((0x1 << 19))
#define CAM_PROT_STEP1_0_ACK_MASK		((0x1 << 19))
#define CAM_PROT_STEP2_0_MASK			((0x1 << 20))
#define CAM_PROT_STEP2_0_ACK_MASK		((0x1 << 20))
#define CAM_PROT_STEP2_1_MASK			((0x1 << 2))
#define CAM_PROT_STEP2_1_ACK_MASK		((0x1 << 2))

/* Define MTCMOS Power Status Mask */

#define MD1_PWR_STA_MASK			(0x1 << 0)
#define CONN_PWR_STA_MASK			(0x1 << 1)
#define DPY_PWR_STA_MASK			(0x1 << 2)
#define DIS_PWR_STA_MASK			(0x1 << 3)
#define MFG_PWR_STA_MASK			(0x1 << 4)
#define IFR_PWR_STA_MASK			(0x1 << 6)
#define MFG_CORE0_PWR_STA_MASK			(0x1 << 7)
#define MFG_ASYNC_PWR_STA_MASK			(0x1 << 23)
#define CAM_PWR_STA_MASK			(0x1 << 25)
#define VCODEC_PWR_STA_MASK			(0x1 << 26)

/* Define CPU SRAM Mask */

/* Define Non-CPU SRAM Mask */
#define MD1_SRAM_PDN				(0x1 << 8)
#define MD1_SRAM_PDN_ACK			(0x0 << 12)
#define MD1_SRAM_PDN_ACK_BIT0			(0x1 << 12)
#define DPY_SRAM_PDN				(0xF << 8)
#define DPY_SRAM_PDN_ACK			(0xF << 12)
#define DPY_SRAM_PDN_ACK_BIT0			(0x1 << 12)
#define DPY_SRAM_PDN_ACK_BIT1			(0x1 << 13)
#define DPY_SRAM_PDN_ACK_BIT2			(0x1 << 14)
#define DPY_SRAM_PDN_ACK_BIT3			(0x1 << 15)
#define DIS_SRAM_PDN				(0x1 << 8)
#define DIS_SRAM_PDN_ACK			(0x1 << 12)
#define DIS_SRAM_PDN_ACK_BIT0			(0x1 << 12)
#define MFG_SRAM_PDN				(0x1 << 8)
#define MFG_SRAM_PDN_ACK			(0x1 << 12)
#define MFG_SRAM_PDN_ACK_BIT0			(0x1 << 12)
#define IFR_SRAM_PDN				(0x1 << 8)
#define IFR_SRAM_PDN_ACK			(0x1 << 12)
#define IFR_SRAM_PDN_ACK_BIT0			(0x1 << 12)
#define MFG_CORE0_SRAM_PDN			(0x1 << 8)
#define MFG_CORE0_SRAM_PDN_ACK			(0x1 << 12)
#define MFG_CORE0_SRAM_PDN_ACK_BIT0		(0x1 << 12)
#define CAM_SRAM_PDN				(0x3 << 8)
#define CAM_SRAM_PDN_ACK			(0x3 << 12)
#define CAM_SRAM_PDN_ACK_BIT0			(0x1 << 12)
#define CAM_SRAM_PDN_ACK_BIT1			(0x1 << 13)
#define VCODEC_SRAM_PDN				(0x1 << 8)
#define VCODEC_SRAM_PDN_ACK			(0x1 << 12)
#define VCODEC_SRAM_PDN_ACK_BIT0		(0x1 << 12)

static struct subsys syss[] =	/* NR_SYSS *//* FIXME: set correct value */
{
	[SYS_MD1] = {
			.name = __stringify(SYS_MD1),
			.sta_mask = MD1_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &MD1_sys_ops,
			},
	[SYS_CONN] = {
			.name = __stringify(SYS_CONN),
			.sta_mask = CONN_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &CONN_sys_ops,
			},
	[SYS_DPY] = {
			.name = __stringify(SYS_DPY),
			.sta_mask = DPY_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &DPY_sys_ops,
			},
	[SYS_DIS] = {
			.name = __stringify(SYS_DIS),
			.sta_mask = DIS_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &DIS_sys_ops,
			},
	[SYS_MFG] = {
			.name = __stringify(SYS_MFG),
			.sta_mask = MFG_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &MFG_sys_ops,
			},
	[SYS_IFR] = {
			.name = __stringify(SYS_IFR),
			.sta_mask = IFR_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &IFR_sys_ops,
			},
	[SYS_MFG_CORE0] = {
			.name = __stringify(SYS_MFG_CORE0),
			.sta_mask = MFG_CORE0_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &MFG_CORE0_sys_ops,
			},
	[SYS_MFG_ASYNC] = {
			.name = __stringify(SYS_MFG_ASYNC),
			.sta_mask = MFG_ASYNC_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &MFG_ASYNC_sys_ops,
			},
	[SYS_CAM] = {
			.name = __stringify(SYS_CAM),
			.sta_mask = CAM_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &CAM_sys_ops,
			},
	[SYS_VCODEC] = {
			.name = __stringify(SYS_VCODEC),
			.sta_mask = VCODEC_PWR_STA_MASK,
			/* .ctl_addr = NULL,  */
			.sram_pdn_bits = 0,
			.sram_pdn_ack_bits = 0,
			.bus_prot_mask = 0,
			.ops = &VCODEC_sys_ops,
			},
};

LIST_HEAD(pgcb_list);

struct pg_callbacks *register_pg_callback(struct pg_callbacks *pgcb)
{
	INIT_LIST_HEAD(&pgcb->list);

	list_add(&pgcb->list, &pgcb_list);

	return pgcb;
}
EXPORT_SYMBOL(register_pg_callback);

static struct subsys *id_to_sys(unsigned int id)
{
	return id < NR_SYSS ? &syss[id] : NULL;
}

enum dbg_id {
	DBG_ID_MD1_BUS = 0,
	DBG_ID_CONN_BUS,
	DBG_ID_DPY_BUS,
	DBG_ID_DIS_BUS,
	DBG_ID_MFG_BUS,
	DBG_ID_IFR_BUS,
	DBG_ID_MFG_CORE0_BUS,
	DBG_ID_MFG_ASYNC_BUS,
	DBG_ID_CAM_BUS,
	DBG_ID_VCODEC_BUS = 9,

	DBG_ID_MD1_PWR = 10,
	DBG_ID_CONN_PWR,
	DBG_ID_DPY_PWR,
	DBG_ID_DIS_PWR,
	DBG_ID_MFG_PWR,
	DBG_ID_IFR_PWR,
	DBG_ID_MFG_CORE0_PWR,
	DBG_ID_MFG_ASYNC_PWR,
	DBG_ID_CAM_PWR,
	DBG_ID_VCODEC_PWR,
	DBG_ID_NUM = 20,
};

#define ID_MADK   0xFF000000
#define STA_MASK  0x00F00000
#define STEP_MASK 0x000000FF

#define INCREASE_STEPS \
	do { DBG_STEP++; } while (0)

static int DBG_ID;
static int DBG_STA;
static int DBG_STEP;
/*
 * ram console data0 define
 * [31:24] : DBG_ID
 * [23:20] : DBG_STA
 * [7:0] : DBG_STEP
 */
static void ram_console_update(void)
{
	struct pg_callbacks *pgcb;
	u32 data[8] = {0x0};
	u32 i = 0, j = 0;
	static u32 pre_data;
	static int k;
	static bool print_once = true;

	if (DBG_ID < 0 || DBG_ID >= DBG_ID_NUM)
		return;

	data[i] = ((DBG_ID << 24) & ID_MADK)
		| ((DBG_STA << 20) & STA_MASK)
		| (DBG_STEP & STEP_MASK);

	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_1);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA0);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA1);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA0_1);
	data[++i] = clk_readl(INFRA_TOPAXI_PROTECTEN_STA1_1);
	data[++i] = clk_readl(INFRA_TOPAXI_SI3_STA);

	if (pre_data == data[0])
		k++;
	else if (pre_data != data[0]) {
		k = 0;
		pre_data = data[0];
		print_once = true;
	}

	if (k > 5000 && print_once) {
		enum subsys_id id =
			(enum subsys_id)(DBG_ID % (DBG_ID_NUM / 2));

		print_once = false;
		k = 0;
		if (DBG_ID == DBG_ID_CONN_BUS) {
			if (DBG_STEP <= 1 && DBG_STA == STA_POWER_DOWN) {
				/* TINFO="Release bus protect - step1 : 0" */
				spm_write(INFRA_TOPAXI_PROTECTEN_CLR,
						CONN_PROT_STEP1_0_MASK);
			}
		}
		/* print_log with enabled clk/mux/pll */
		/* print_enabled_clks_once(); */
		/* wmt callback function for their debug logs */
		//mtk_wcn_cmb_stub_clock_fail_dump();
		/* debug callback hook searching */
		list_for_each_entry_reverse(pgcb, &pgcb_list, list) {
			if (pgcb->debug_dump)
				pgcb->debug_dump(id);
		}

		if (DBG_ID == DBG_ID_CONN_BUS) {
			if (DBG_STEP <= 1 && DBG_STA == STA_POWER_DOWN) {
				/* TINFO="Set bus protect - step1 : 0" */
				spm_write(INFRA_TOPAXI_PROTECTEN_SET,
						CONN_PROT_STEP1_0_MASK);
				j = 0;
				while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
					& CONN_PROT_STEP1_0_ACK_MASK)
					!= CONN_PROT_STEP1_0_ACK_MASK) {
					udelay(1);
					if (j > 1000)
						break;
					j++;
				}

				if (j > 1000)
					DBG_STEP = CONN_TIMEOUT_STEP1;
				else
					DBG_STEP = CONN_TIMEOUT_RECOVERY;

				data[0] = ((DBG_ID << 24) & ID_MADK)
					| ((DBG_STA << 20) & STA_MASK)
					| (DBG_STEP & STEP_MASK);
				pre_data = data[0];
			}
		}

		if (DBG_ID >= (DBG_ID_NUM / 2))
			pr_notice("%s %s MTCMOS PWR hang at %s flow step %d\n",
				"[clkmgr]",
				syss[(DBG_ID - (DBG_ID_NUM / 2))].name,
				DBG_STA ? "pwron":"pdn",
				DBG_STEP);
		else
			pr_notice("%s %s MTCMOS BUS hang at %s flow step %d\n",
				"[clkmgr]",
				syss[DBG_ID].name,
				DBG_STA ? "pwron":"pdn",
				DBG_STEP);

		for (i = 1; i < ARRAY_SIZE(data); i++)
			pr_notice("%s: clk[%d] = 0x%x\n", __func__, i, data[i]);

		pr_notice("INFRA_TOPAXI_SI0_STA =0x%x\n",
			spm_read(INFRA_TOPAXI_SI0_STA));
		pr_notice("INFRA_TOPAXI_SI1_STA =0x%x\n",
			spm_read(INFRA_TOPAXI_SI1_STA));
		pr_notice("INFRA_TOPAXI_SI2_STA =0x%x\n",
			spm_read(INFRA_TOPAXI_SI2_STA));
		pr_notice("INFRA_TOPAXI_SI3_STA =0x%x\n",
			spm_read(INFRA_TOPAXI_SI3_STA));
		pr_notice("INFRA_TOPAXI_SI4_STA =0x%x\n",
			spm_read(INFRA_TOPAXI_SI4_STA));
		pr_notice("INFRA_TOPAXI_MI_STA =0x%x\n",
			spm_read(INFRA_TOPAXI_MI_STA));
		pr_notice("INFRA_MCI_SI0_STA =0x%x\n",
			spm_read(INFRA_MCI_SI0_STA));
		pr_notice("INFRA_MCI_SI2_STA =0x%x\n",
			spm_read(INFRA_MCI_SI2_STA));
	}

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	for (i = 0; ARRAY_SIZE(data) < 8; i++)
		aee_rr_rec_clk(i, data[i]);
	/*todo: add each domain's debug register to ram console*/
#endif
}

/* auto-gen begin*/
static int spm_mtcmos_ctrl_md1_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MD1_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MD1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& MD1_PROT_STEP1_0_ACK_MASK)
			!= MD1_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& MD1_PROT_STEP2_0_ACK_MASK)
			!= MD1_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, MD1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& MD1_PROT_STEP2_1_ACK_MASK)
			!= MD1_PROT_STEP2_1_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) | MD1_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set MD1 bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) & ~(0x1 << 8));
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MD1_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, MD1_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MD1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release MD1 bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_md1_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MD1_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MD1" */
		/* TINFO="MD_EXTRA_PWR_CON[0]=1"*/
		spm_write(MD_EXTRA_PWR_CON,
			spm_read(MD_EXTRA_PWR_CON) | (0x1 << 0));
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) | PWR_ISO);
		/* TINFO="MD_SRAM_ISO_CON[0]=0"*/
		spm_write(MD_SRAM_ISO_CON,
			spm_read(MD_SRAM_ISO_CON) & ~(0x1 << 0));
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MD1_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MD1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MD1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MD1" */
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS)
			& MD1_PWR_STA_MASK)
			!= MD1_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MD1_PWR_STA_MASK)
			!= MD1_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
		}
#endif
		/* TINFO="MD_SRAM_ISO_CON[0]=1"*/
		spm_write(MD_SRAM_ISO_CON,
			spm_read(MD_SRAM_ISO_CON) | (0x1 << 0));
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MD1_PWR_CON,
			spm_read(MD1_PWR_CON) | PWR_RST_B);
		/* TINFO="MD_EXTRA_PWR_CON[0]=0"*/
		spm_write(MD_EXTRA_PWR_CON,
			spm_read(MD_EXTRA_PWR_CON) & ~(0x1 << 0));
		/* TINFO="Finish to turn on MD1" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_conn_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CONN_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& CONN_PROT_STEP1_0_ACK_MASK)
			!= CONN_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET,
			CONN_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& CONN_PROT_STEP1_1_ACK_MASK)
			!= CONN_PROT_STEP1_1_ACK_MASK) {
			ram_console_update();
			if (DBG_STEP == CONN_TIMEOUT_RECOVERY)
				break;
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& CONN_PROT_STEP2_0_ACK_MASK)
			!= CONN_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET,
			CONN_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& CONN_PROT_STEP2_1_ACK_MASK)
			!= CONN_PROT_STEP2_1_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set CONN bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, CONN_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR,
			CONN_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, CONN_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR,
			CONN_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release CONN bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_conn_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CONN_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CONN" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CONN_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & CONN_PWR_STA_MASK)) {
		/* No logic between pwr_on and pwr_ack.
		 * Print SRAM / MTCMOS control and
		 * PWR_ACK for debug.
		 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off CONN" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CONN" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS)
			& CONN_PWR_STA_MASK)
			!= CONN_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & CONN_PWR_STA_MASK)
			!= CONN_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CONN_PWR_CON,
			spm_read(CONN_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on CONN" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_dpy_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_DPY_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, DPY_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& DPY_PROT_STEP1_0_ACK_MASK)
			!= DPY_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, DPY_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& DPY_PROT_STEP1_1_ACK_MASK)
			!= DPY_PROT_STEP1_1_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, DPY_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& DPY_PROT_STEP2_0_ACK_MASK)
			!= DPY_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) | DPY_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DPY_SRAM_PDN_ACK = 1" */
		while ((spm_read(DPY_PWR_CON) & DPY_SRAM_PDN_ACK)
			!= DPY_SRAM_PDN_ACK) {
			/* n/a */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set DPY bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DPY_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(DPY_PWR_CON) & DPY_SRAM_PDN_ACK_BIT0) {
			/* n/a */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DPY_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(DPY_PWR_CON) & DPY_SRAM_PDN_ACK_BIT1) {
			/* n/a */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) & ~(0x1 << 10));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DPY_SRAM_PDN_ACK_BIT2 = 0" */
		while (spm_read(DPY_PWR_CON) & DPY_SRAM_PDN_ACK_BIT2) {
			/* n/a */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) & ~(0x1 << 11));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DPY_SRAM_PDN_ACK_BIT3 = 0" */
		while (spm_read(DPY_PWR_CON) & DPY_SRAM_PDN_ACK_BIT3) {
			/* n/a */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DPY_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DPY_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 1" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, DPY_PROT_STEP1_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release DPY bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_dpy_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_DPY_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off DPY" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & DPY_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & DPY_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off DPY" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on DPY" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS)
			& DPY_PWR_STA_MASK)
			!= DPY_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & DPY_PWR_STA_MASK)
			!= DPY_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(DPY_PWR_CON,
			spm_read(DPY_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on DPY" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_dis_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_DIS_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, DIS_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& DIS_PROT_STEP1_0_ACK_MASK)
			!= DIS_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, DIS_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& DIS_PROT_STEP2_0_ACK_MASK)
			!= DIS_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) | DIS_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DIS_SRAM_PDN_ACK = 1" */
		while ((spm_read(DIS_PWR_CON) & DIS_SRAM_PDN_ACK)
			!= DIS_SRAM_PDN_ACK) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set DIS bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DIS_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(DIS_PWR_CON) & DIS_SRAM_PDN_ACK_BIT0) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DIS_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, DIS_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release DIS bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_dis_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_DIS_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off DIS" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & DIS_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off DIS" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on DIS" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS)
			& DIS_PWR_STA_MASK)
			!= DIS_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & DIS_PWR_STA_MASK)
			!= DIS_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(DIS_PWR_CON,
			spm_read(DIS_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on DIS" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_mfg_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MFG_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& MFG_PROT_STEP1_0_ACK_MASK)
			!= MFG_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, MFG_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& MFG_PROT_STEP2_0_ACK_MASK)
			!= MFG_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG_PWR_CON,
			spm_read(MFG_PWR_CON) | MFG_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG_PWR_CON) & MFG_SRAM_PDN_ACK)
			!= MFG_SRAM_PDN_ACK) {
			/* Need f_fmfg_core_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set MFG bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG_PWR_CON,
			spm_read(MFG_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG_PWR_CON) & MFG_SRAM_PDN_ACK_BIT0) {
			/* Need f_fmfg_core_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MFG_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, MFG_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release MFG bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_mfg_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG_PWR_CON,
			spm_read(MFG_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG_PWR_CON,
			spm_read(MFG_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG_PWR_CON,
			spm_read(MFG_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG_PWR_CON,
			spm_read(MFG_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG_PWR_CON,
			spm_read(MFG_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG_PWR_STA_MASK)
			|| (spm_read(PWR_STATUS_2ND) & MFG_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG_PWR_CON,
			spm_read(MFG_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG_PWR_CON,
			spm_read(MFG_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS)
			& MFG_PWR_STA_MASK)
			!= MFG_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG_PWR_STA_MASK)
			!= MFG_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG_PWR_CON,
			spm_read(MFG_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG_PWR_CON,
			spm_read(MFG_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG_PWR_CON,
			spm_read(MFG_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MFG" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_ifr_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_IFR_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(IFR_PWR_CON,
			spm_read(IFR_PWR_CON) | IFR_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IFR_SRAM_PDN_ACK = 1" */
		while ((spm_read(IFR_PWR_CON) & IFR_SRAM_PDN_ACK)
			!= IFR_SRAM_PDN_ACK) {
			/* SRAM PDN delay IP clock is 26MHz.
			 * Print SRAM control and ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set IFR bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(IFR_PWR_CON,
			spm_read(IFR_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IFR_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(IFR_PWR_CON) & IFR_SRAM_PDN_ACK_BIT0) {
			/* SRAM PDN delay IP clock is 26MHz.
			 * Print SRAM control and ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to release IFR bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_ifr_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_IFR_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off IFR" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(IFR_PWR_CON,
			spm_read(IFR_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(IFR_PWR_CON,
			spm_read(IFR_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(IFR_PWR_CON,
			spm_read(IFR_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(IFR_PWR_CON,
			spm_read(IFR_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(IFR_PWR_CON,
			spm_read(IFR_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & IFR_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & IFR_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off IFR" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on IFR" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(IFR_PWR_CON,
			spm_read(IFR_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(IFR_PWR_CON,
			spm_read(IFR_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS)
			& IFR_PWR_STA_MASK)
			!= IFR_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & IFR_PWR_STA_MASK)
			!= IFR_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(IFR_PWR_CON,
			spm_read(IFR_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(IFR_PWR_CON,
			spm_read(IFR_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(IFR_PWR_CON,
			spm_read(IFR_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on IFR" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_mfg_core0_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG_CORE0_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | MFG_CORE0_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_CORE0_SRAM_PDN_ACK = 1" */
		while ((spm_read(MFG_CORE0_PWR_CON)
			& MFG_CORE0_SRAM_PDN_ACK) != MFG_CORE0_SRAM_PDN_ACK) {
			/* n/a */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set MFG_CORE0 bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MFG_CORE0_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MFG_CORE0_PWR_CON)
			& MFG_CORE0_SRAM_PDN_ACK_BIT0) {
			/*  */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to release MFG_CORE0 bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_mfg_core0_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG_CORE0_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG_CORE0" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG_CORE0_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG_CORE0_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG_CORE0" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG_CORE0" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS)
			& MFG_CORE0_PWR_STA_MASK)
			!= MFG_CORE0_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG_CORE0_PWR_STA_MASK)
			!= MFG_CORE0_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG_CORE0_PWR_CON,
			spm_read(MFG_CORE0_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MFG_CORE0" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_mfg_async_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG_ASYNC_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
#ifndef IGNORE_MTCMOS_CHECK
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set MFG_ASYNC bus protect" */
	} else {    /* STA_RELEASE_BUS */
#ifndef IGNORE_MTCMOS_CHECK
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to release MFG_ASYNC bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_mfg_async_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_MFG_ASYNC_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MFG_ASYNC" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off MFG_ASYNC" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MFG_ASYNC" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS)
			& MFG_ASYNC_PWR_STA_MASK)
			!= MFG_ASYNC_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)
			!= MFG_ASYNC_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MFG_ASYNC_PWR_CON,
			spm_read(MFG_ASYNC_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on MFG_ASYNC" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_cam_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, CAM_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1)
			& CAM_PROT_STEP1_0_ACK_MASK)
			!= CAM_PROT_STEP1_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CAM_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1)
			& CAM_PROT_STEP2_0_ACK_MASK)
			!= CAM_PROT_STEP2_0_ACK_MASK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Set bus protect - step2 : 1" */
		spm_write(SMI_COMMON_SMI_CLAMP_SET, CAM_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(SMI_COMMON_SMI_CLAMP)
			& CAM_PROT_STEP2_1_ACK_MASK)
			!= CAM_PROT_STEP2_1_ACK_MASK) {
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) | CAM_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK = 1" */
		while ((spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK)
			!= CAM_SRAM_PDN_ACK)
			ram_console_update();

		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set CAM bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK_BIT0)
			ram_console_update();
		INCREASE_STEPS;
#endif
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until CAM_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(CAM_PWR_CON) & CAM_SRAM_PDN_ACK_BIT1)
			ram_console_update();
		INCREASE_STEPS;
#endif
		/* TINFO="Release bus protect - step2 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, CAM_PROT_STEP2_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step2 : 1" */
		spm_write(SMI_COMMON_SMI_CLAMP_CLR, CAM_PROT_STEP2_1_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, CAM_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after
		 * releasing protect has been ignored
		 */
#endif
		/* TINFO="Finish to release CAM bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_cam_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_CAM_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CAM" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CAM_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & CAM_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off CAM" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CAM" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS)
			& CAM_PWR_STA_MASK)
			!= CAM_PWR_STA_MASK)
			|| ((spm_read(PWR_STATUS_2ND) & CAM_PWR_STA_MASK)
			!= CAM_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CAM_PWR_CON,
			spm_read(CAM_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on CAM" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_vcodec_bus_prot(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VCODEC_BUS;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) | VCODEC_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VCODEC_SRAM_PDN_ACK = 1" */
		while ((spm_read(VCODEC_PWR_CON) & VCODEC_SRAM_PDN_ACK)
			!= VCODEC_SRAM_PDN_ACK) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to set VCODEC bus protect" */
	} else {    /* STA_RELEASE_BUS */
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until VCODEC_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(VCODEC_PWR_CON)
			& VCODEC_SRAM_PDN_ACK_BIT0) {
			/* Need hf_fmm_ck for SRAM PDN delay IP. */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to release VCODEC bus protect" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}

static int spm_mtcmos_ctrl_vcodec_pwr(int state)
{
	int err = 0;

	DBG_ID = DBG_ID_VCODEC_PWR;
	DBG_STA = state;
	DBG_STEP = 0;

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off VCODEC" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & VCODEC_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & VCODEC_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
		INCREASE_STEPS;
#endif
		/* TINFO="Finish to turn off VCODEC" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on VCODEC" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS)
			& VCODEC_PWR_STA_MASK)
			!= VCODEC_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & VCODEC_PWR_STA_MASK)
		       != VCODEC_PWR_STA_MASK)) {
			/* No logic between pwr_on and pwr_ack.
			 * Print SRAM / MTCMOS control and
			 * PWR_ACK for debug.
			 */
			ram_console_update();
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(VCODEC_PWR_CON,
			spm_read(VCODEC_PWR_CON) | PWR_RST_B);
		/* TINFO="Finish to turn on VCODEC" */
	}
	INCREASE_STEPS;
	ram_console_update();
	return err;
}


static int MD1_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_ON);
}

static int MD1_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_md1_pwr(STA_POWER_ON);
}

static int MD1_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_DOWN);
}

static int MD1_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_md1_pwr(STA_POWER_DOWN);
}

static int CONN_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_conn_bus_prot(STA_POWER_ON);
}

static int CONN_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_conn_pwr(STA_POWER_ON);
}

static int CONN_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_conn_bus_prot(STA_POWER_DOWN);
}

static int CONN_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_conn_pwr(STA_POWER_DOWN);
}

static int DPY_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dpy_bus_prot(STA_POWER_ON);
}

static int DPY_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dpy_pwr(STA_POWER_ON);
}

static int DPY_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dpy_bus_prot(STA_POWER_DOWN);
}

static int DPY_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dpy_pwr(STA_POWER_DOWN);
}

static int DIS_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dis_bus_prot(STA_POWER_ON);
}

static int DIS_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dis_pwr(STA_POWER_ON);
}

static int DIS_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dis_bus_prot(STA_POWER_DOWN);
}

static int DIS_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_dis_pwr(STA_POWER_DOWN);
}

static int MFG_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_bus_prot(STA_POWER_ON);
}

static int MFG_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_pwr(STA_POWER_ON);
}

static int MFG_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_bus_prot(STA_POWER_DOWN);
}

static int MFG_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_pwr(STA_POWER_DOWN);
}

static int IFR_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ifr_bus_prot(STA_POWER_ON);
}

static int IFR_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ifr_pwr(STA_POWER_ON);
}

static int IFR_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ifr_bus_prot(STA_POWER_DOWN);
}

static int IFR_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_ifr_pwr(STA_POWER_DOWN);
}

static int MFG_CORE0_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_core0_bus_prot(STA_POWER_ON);
}

static int MFG_CORE0_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_core0_pwr(STA_POWER_ON);
}

static int MFG_CORE0_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_core0_bus_prot(STA_POWER_DOWN);
}

static int MFG_CORE0_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_core0_pwr(STA_POWER_DOWN);
}

static int MFG_ASYNC_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_async_bus_prot(STA_POWER_ON);
}

static int MFG_ASYNC_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_async_pwr(STA_POWER_ON);
}

static int MFG_ASYNC_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_async_bus_prot(STA_POWER_DOWN);
}

static int MFG_ASYNC_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_mfg_async_pwr(STA_POWER_DOWN);
}

static int CAM_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_bus_prot(STA_POWER_ON);
}

static int CAM_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_pwr(STA_POWER_ON);
}

static int CAM_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_bus_prot(STA_POWER_DOWN);
}

static int CAM_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_cam_pwr(STA_POWER_DOWN);
}

static int VCODEC_sys_prepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_vcodec_bus_prot(STA_POWER_ON);
}

static int VCODEC_sys_enable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_vcodec_pwr(STA_POWER_ON);
}

static int VCODEC_sys_unprepare_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_vcodec_bus_prot(STA_POWER_DOWN);
}

static int VCODEC_sys_disable_op(struct subsys *sys)
{
	/*pr_debug("[CCF] %s\r\n", __func__); */
	return spm_mtcmos_ctrl_vcodec_pwr(STA_POWER_DOWN);
}

static int sys_get_state_op(struct subsys *sys)
{
	unsigned int sta = clk_readl(PWR_STATUS);
	unsigned int sta_s = clk_readl(PWR_STATUS_2ND);

	return (sta & sys->sta_mask) && (sta_s & sys->sta_mask);
}

static struct subsys_ops MD1_sys_ops = {
	.prepare =  MD1_sys_prepare_op,
	.unprepare =  MD1_sys_unprepare_op,
	.enable =  MD1_sys_enable_op,
	.disable =  MD1_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops CONN_sys_ops = {
	.prepare =  CONN_sys_prepare_op,
	.unprepare =  CONN_sys_unprepare_op,
	.enable =  CONN_sys_enable_op,
	.disable =  CONN_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops DPY_sys_ops = {
	.prepare =  DPY_sys_prepare_op,
	.unprepare =  DPY_sys_unprepare_op,
	.enable =  DPY_sys_enable_op,
	.disable =  DPY_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops DIS_sys_ops = {
	.prepare =  DIS_sys_prepare_op,
	.unprepare =  DIS_sys_unprepare_op,
	.enable =  DIS_sys_enable_op,
	.disable =  DIS_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops MFG_sys_ops = {
	.prepare =  MFG_sys_prepare_op,
	.unprepare =  MFG_sys_unprepare_op,
	.enable =  MFG_sys_enable_op,
	.disable =  MFG_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops IFR_sys_ops = {
	.prepare =  IFR_sys_prepare_op,
	.unprepare =  IFR_sys_unprepare_op,
	.enable =  IFR_sys_enable_op,
	.disable =  IFR_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops MFG_CORE0_sys_ops = {
	.prepare =  MFG_CORE0_sys_prepare_op,
	.unprepare =  MFG_CORE0_sys_unprepare_op,
	.enable =  MFG_CORE0_sys_enable_op,
	.disable =  MFG_CORE0_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops MFG_ASYNC_sys_ops = {
	.prepare =  MFG_ASYNC_sys_prepare_op,
	.unprepare =  MFG_ASYNC_sys_unprepare_op,
	.enable =  MFG_ASYNC_sys_enable_op,
	.disable =  MFG_ASYNC_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops CAM_sys_ops = {
	.prepare =  CAM_sys_prepare_op,
	.unprepare =  CAM_sys_unprepare_op,
	.enable =  CAM_sys_enable_op,
	.disable =  CAM_sys_disable_op,
	.get_state = sys_get_state_op,
};
static struct subsys_ops VCODEC_sys_ops = {
	.prepare =  VCODEC_sys_prepare_op,
	.unprepare =  VCODEC_sys_unprepare_op,
	.enable =  VCODEC_sys_enable_op,
	.disable =  VCODEC_sys_disable_op,
	.get_state = sys_get_state_op,
};

/* auto-gen end*/
static int subsys_is_on(enum subsys_id id)
{
	int r;
	struct subsys *sys = id_to_sys(id);

	if (!sys) {
		WARN_ON(!sys);
		return -EINVAL;
	}

	r = sys->ops->get_state(sys);

#if MT_CCF_DEBUG
	pr_debug("[CCF] %s:%d, sys=%s, id=%d\n", __func__, r, sys->name, id);
#endif				/* MT_CCF_DEBUG */

	return r;
}

#if CONTROL_LIMIT
int allow[NR_SYSS] = {
1, /*SYS_MD1*/
1, /*SYS_CONN*/
1, /*SYS_DPY*/
1, /*SYS_DIS*/
1, /*SYS_MFG*/
1, /*SYS_IFR*/
1, /*SYS_MFG_CORE0*/
1, /*SYS_MFG_ASYNC*/
1, /*SYS_CAM*/
1, /*SYS_VCODEC*/
};
#endif
static int enable_subsys(enum subsys_id id, enum mtcmos_op action)
{
	int r = 0;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);
	struct pg_callbacks *pgcb;

	if (!sys) {
		WARN_ON(!sys);
		return -EINVAL;
	}

	if (!mtk_is_mtcmos_enable()) {
#if MT_CCF_DEBUG
		pr_notice("[CCF] skip %s: sys=%s, id=%d\n",
			__func__, sys->name, id);
#endif
		switch (id) {
		case SYS_MD1:
			spm_mtcmos_ctrl_md1_pwr(STA_POWER_ON);
			spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_ON);
			break;
		case SYS_CONN:
			spm_mtcmos_ctrl_conn_pwr(STA_POWER_ON);
			spm_mtcmos_ctrl_conn_bus_prot(STA_POWER_ON);
			break;
		default:
			break;
		}
		return 0;
	}

#if CONTROL_LIMIT
	#if MT_CCF_DEBUG
	pr_notice("[CCF] %s: sys=%s, id=%d, action = %s\n",
		__func__, sys->name, id, action?"PWN":"BUS_PROT");
	#endif
	if (allow[id] == 0) {
		#if MT_CCF_DEBUG
		pr_debug("[CCF] %s: do nothing return\n", __func__);
		#endif
		return 0;
	}
#endif

	mtk_clk_lock(flags);

	if (action == MTCMOS_BUS_PROT) {
		if (sys->ops->prepare)
			r = sys->ops->prepare(sys);
#if MT_CCF_DEBUG
		else
			pr_notice("%s: %s prepare function is NULL\n",
					__func__, sys->name);
#endif
	} else if (action == MTCMOS_PWR) {
		if (sys->ops->enable)
			r = sys->ops->enable(sys);
#if MT_CCF_DEBUG
		else
			pr_notice("%s: %s enable function is NULL\n",
					__func__, sys->name);
#endif
	}

	WARN_ON(r);

	mtk_clk_unlock(flags);

	if (action == MTCMOS_BUS_PROT) {
		list_for_each_entry(pgcb, &pgcb_list, list) {
			if (pgcb->after_on)
				pgcb->after_on(id);
		}
	}

	return r;
}

static int disable_subsys(enum subsys_id id, enum mtcmos_op action)
{
	int r = 0;
	unsigned long flags;
	struct subsys *sys = id_to_sys(id);
	struct pg_callbacks *pgcb;

	if (!sys) {
		WARN_ON(!sys);
		return -EINVAL;
	}

	if (!mtk_is_mtcmos_enable()) {
#if MT_CCF_DEBUG
		pr_notice("[CCF] skip %s: sys=%s, id=%d\n",
			__func__, sys->name, id);
#endif
		switch (id) {
		case SYS_MD1:
			spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_DOWN);
			spm_mtcmos_ctrl_md1_pwr(STA_POWER_DOWN);
			break;
		case SYS_CONN:
			spm_mtcmos_ctrl_conn_bus_prot(STA_POWER_DOWN);
			spm_mtcmos_ctrl_conn_pwr(STA_POWER_DOWN);
			break;
		default:
			break;
		}
		return 0;
	}

#if CONTROL_LIMIT
	#if MT_CCF_DEBUG
	pr_notice("[CCF] %s: sys=%s, id=%d, action = %s\n",
		__func__, sys->name, id, action?"PWN":"BUS_PROT");
	#endif
	if (allow[id] == 0) {
		#if MT_CCF_DEBUG
		pr_debug("[CCF] %s: do nothing return\n", __func__);
		#endif
		return 0;
	}
#endif

	/* TODO: check all clocks related to this subsys are off */
	/* could be power off or not */
	if (action == MTCMOS_BUS_PROT) {
		list_for_each_entry_reverse(pgcb, &pgcb_list, list) {
			if (pgcb->before_off)
				pgcb->before_off(id);
		}
	}

	mtk_clk_lock(flags);

	if (action == MTCMOS_BUS_PROT) {
		if (sys->ops->unprepare)
			r = sys->ops->unprepare(sys);
#if MT_CCF_DEBUG
		else
			pr_notice("%s: %s unprepare function is NULL\n",
					__func__, sys->name);
#endif
	} else if (action == MTCMOS_PWR) {
		if (sys->ops->disable)
			r = sys->ops->disable(sys);
#if MT_CCF_DEBUG
		else
			pr_notice("%s: %s disable function is NULL\n",
					__func__, sys->name);
#endif
	}

	WARN_ON(r);

	mtk_clk_unlock(flags);

	return r;
}

/*
 * power_gate
 */

#define CLK_NUM	10
struct mt_power_gate {
	struct clk_hw hw;
	struct clk *clks;
	struct clk **subsys_clks;
	int clk_num;
	int sub_clk_num;
	enum subsys_id pd_id;
};

#define to_power_gate(_hw) container_of(_hw, struct mt_power_gate, hw)

struct cg_list {
	const char *cg[CLK_NUM];
};

static int pg_is_enabled(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);

	if (!mtk_is_mtcmos_enable())
		return 1;
	else
		return subsys_is_on(pg->pd_id);
}

static int pg_prepare(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);
	struct subsys *sys =  id_to_sys(pg->pd_id);
	unsigned long flags;
	int skip_pg = 0;
	int ret = 0;
	int i = 0;

	if (!sys) {
		WARN_ON(!sys);
		return -EINVAL;
	}

	mtk_mtcmos_lock(flags);
#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == SUBSYS_PWR_ON)
		skip_pg = 1;
#endif				/* CHECK_PWR_ST */

	for (i = 0; i < pg->clk_num; i++) {
		if (pg->clks == NULL)
			break;

		ret = clk_prepare_enable(pg->clks);
		if (ret)
			goto fail;

#if MT_CCF_DEBUG
		pr_notice("[CCF] %s 1: sys=%s, pre_clk=%s done\n", __func__,
				__clk_get_name(hw->clk),
				pg->clks ?
				__clk_get_name(pg->clks):NULL);
#endif				/* MT_CCF_DEBUG */
	}

	if (!skip_pg) {
		ret = enable_subsys(pg->pd_id, MTCMOS_PWR);
		if (ret)
			goto fail;
	}

	for (i = 0; i < pg->sub_clk_num; i++) {
		if (pg->subsys_clks[i] == NULL)
			break;

		ret = clk_prepare_enable(pg->subsys_clks[i]);
		if (ret)
			goto fail;

#if MT_CCF_DEBUG
		pr_notice("[CCF] %s 2: sys=%s, pre_clk=%s done\n", __func__,
			__clk_get_name(hw->clk),
			pg->subsys_clks[i] ?
			__clk_get_name(pg->subsys_clks[i]):NULL);
#endif				/* MT_CCF_DEBUG */
	}

	if (!skip_pg) {
		ret = enable_subsys(pg->pd_id, MTCMOS_BUS_PROT);
		if (ret)
			goto fail;
	}

fail:
	mtk_mtcmos_unlock(flags);
	return ret;
}

static void pg_unprepare(struct clk_hw *hw)
{
	struct mt_power_gate *pg = to_power_gate(hw);
	struct subsys *sys =  id_to_sys(pg->pd_id);
	unsigned long flags;
	int skip_pg = 0;
	int ret = 0;
	int i = 0;

	if (!sys) {
		WARN_ON(!sys);
		return;
	}

	mtk_mtcmos_lock(flags);
#if CHECK_PWR_ST
	if (sys->ops->get_state(sys) == SUBSYS_PWR_DOWN)
		skip_pg = 1;
#endif				/* CHECK_PWR_ST */
	if (!skip_pg) {
		ret = disable_subsys(pg->pd_id, MTCMOS_BUS_PROT);
		if (ret)
			return;
	}

	for (i = 0; i < pg->sub_clk_num; i++) {
		if (pg->subsys_clks[i] == NULL)
			break;

		clk_disable_unprepare(pg->subsys_clks[i]);
#if MT_CCF_DEBUG
		pr_notice("[CCF] %s: sys=%s, pre_clk=%s\n", __func__,
			__clk_get_name(hw->clk),
			pg->subsys_clks[i] ?
			__clk_get_name(pg->subsys_clks[i]):NULL);
#endif				/* MT_CCF_DEBUG */
	}

	if (!skip_pg)
		disable_subsys(pg->pd_id, MTCMOS_PWR);

	for (i = 0; i < pg->clk_num; i++) {
		if (pg->clks == NULL)
			break;

		clk_disable_unprepare(pg->clks);
#if MT_CCF_DEBUG
		pr_notice("[CCF] %s: sys=%s, pre_clk=%s\n", __func__,
			__clk_get_name(hw->clk),
			pg->clks ?
			__clk_get_name(pg->clks):NULL);
#endif			/* MT_CCF_DEBUG */
	}

	mtk_mtcmos_unlock(flags);
}

static const struct clk_ops mt_power_gate_ops = {
	.prepare = pg_prepare,
	.unprepare = pg_unprepare,
	.is_enabled = pg_is_enabled,
};

#define MAX_CLKS	3
#define MAX_SUBSYS_CLKS 13

enum clk_id {
	CLK_NONE,
	CLK_MM,
	CLK_MFG,
	CLK_CAMSYS,
	CLK_MAX,
};

static const char * const clk_names[] = {
	NULL,
	"mm",
	"mfg",
	"cam",
	NULL,
};

#define pg_md1 "pg_md1"
#define pg_conn "pg_conn"
#define pg_dpy "pg_dpy"
#define pg_dis "pg_dis"
#define pg_mfg "pg_mfg"
#define pg_ifr "pg_ifr"
#define pg_mfg_core0 "pg_mfg_core0"
#define pg_mfg_async "pg_mfg_async"
#define pg_cam "pg_cam"
#define pg_vcodec "pg_vcodec"

struct mtk_power_gate {
	int id;
	const char *name;
	const char *parent_name;
	enum clk_id clk_id;
	enum clk_id subclk_id;
	enum subsys_id pd_id;
	struct clk *subsys_clks[MAX_SUBSYS_CLKS];
};

#define PGATE(_id, _name, _parent, _clk_id, _subclk_id, _pd_id) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.pd_id = _pd_id,				\
		.clk_id = _clk_id,			\
		.subclk_id = _subclk_id,		\
	}

/* FIXME: all values needed to be verified */
static struct mtk_power_gate scp_clks[] = {
	PGATE(SCP_SYS_MD1, pg_md1, NULL, CLK_NONE, CLK_NONE, SYS_MD1),
	// PGATE(SCP_SYS_CONN, pg_conn, NULL, CLK_NONE, CLK_NONE, SYS_CONN),
	// PGATE(SCP_SYS_DPY, pg_dpy, NULL, CLK_NONE, CLK_NONE, SYS_DPY),
	// PGATE(SCP_SYS_DIS, pg_dis, NULL, CLK_MM, CLK_MM, SYS_DIS),
	PGATE(SCP_SYS_MFG, pg_mfg, pg_mfg_async, CLK_NONE, CLK_NONE, SYS_MFG),
	// PGATE(SCP_SYS_IFR, pg_ifr, NULL, CLK_NONE, CLK_NONE, SYS_IFR),
	PGATE(SCP_SYS_MFG_CORE0, pg_mfg_core0, pg_mfg,
			CLK_NONE, CLK_NONE, SYS_MFG_CORE0),
	PGATE(SCP_SYS_MFG_ASYNC, pg_mfg_async, NULL, CLK_MFG, CLK_NONE,
			SYS_MFG_ASYNC),
	// PGATE(SCP_SYS_CAM, pg_cam, pg_dis, CLK_NONE, CLK_CAMSYS, SYS_CAM),
	// PGATE(SCP_SYS_VCODEC, pg_vcodec, pg_dis, CLK_NONE, CLK_NONE,
			// SYS_VCODEC),
};

static struct clk *mt_clk_register_power_gate(const char *name,
					const char *parent_name,
					struct clk *clk,
					struct clk **subsys_clk,
					int clk_num,
					int sub_clk_num,
					enum subsys_id pd_id)
{
	struct mt_power_gate *pg;
	struct clk *pg_clk;
	struct clk_init_data init = {};

	pg = kzalloc(sizeof(*pg), GFP_KERNEL);
	if (!pg)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.flags = CLK_IGNORE_UNUSED;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.ops = &mt_power_gate_ops;

	pg->clks = clk;
	pg->clk_num = clk_num;
	pg->subsys_clks = subsys_clk;
	pg->sub_clk_num = sub_clk_num;
	pg->pd_id = pd_id;
	pg->hw.init = &init;

	pg_clk = clk_register(NULL, &pg->hw);
	if (IS_ERR(pg_clk)) {
		pr_err("%s: clk register fail\n", __func__);
		kfree(pg);
	}

	return pg_clk;
}

static int init_subsys_clks(struct platform_device *pdev,
		enum clk_id clk_id, struct clk **clk)
{
	struct device_node *node = pdev->dev.of_node;
	struct property *prop;
	const char *clk_name;
	const char *prefix;
	u32 prefix_len;
	u32 clk_cnt = 0;

	if (clk_id <= CLK_NONE || clk_id >= CLK_MAX)
		return clk_cnt;

	prefix = clk_names[clk_id];

	if (!node) {
		dev_err(&pdev->dev, "Cannot find scpsys node: %ld\n",
			PTR_ERR(node));
		return PTR_ERR(node);
	}

	prefix_len = strlen(prefix);
	of_property_for_each_string(node, "clock-names", prop, clk_name) {
		if (!strncmp(clk_name, prefix, prefix_len) &&
				(clk_name[prefix_len] == '-')) {
			struct clk *tmp;

			if (clk_cnt >= MAX_SUBSYS_CLKS) {
				dev_err(&pdev->dev,
					"subsys clk out of range %d\n",
					clk_cnt);
				return -ENOMEM;
			}

			tmp = devm_clk_get(&pdev->dev,
						clk_name);

			if (IS_ERR(tmp)) {
				dev_err(&pdev->dev,
					"Subsys clk(%s) read fail %ld\n",
					clk_name, PTR_ERR(tmp));
				return PTR_ERR(tmp);
			}
			clk[clk_cnt] = tmp;
			clk_cnt++;
		}
	}

	return clk_cnt;
}

static int init_clks(struct platform_device *pdev, struct clk **clk)
{
	int ret = 0;
	int i;

	for (i = CLK_NONE + 1; i < CLK_MAX; i++) {
		struct clk *tmp;

		tmp = devm_clk_get(&pdev->dev, clk_names[i]);
		if (IS_ERR(tmp)) {
			pr_err("fail to get scpsys %s clk = (%ld)\n",
				clk_names[i], PTR_ERR(tmp));

			continue;
		}
		clk[i] = tmp;
	}

	return ret;
}

static int init_clk_scpsys(struct platform_device *pdev,
		struct clk_onecell_data *clk_data)
{
	struct clk *clk[CLK_MAX] = {NULL};
	struct clk *ret_clk;
	int clk_num;
	int ret = 0;
	int i;

	ret = init_clks(pdev, clk);
	if (ret)
		goto fail;

	for (i = 0; i < ARRAY_SIZE(scp_clks); i++) {
		struct mtk_power_gate *pg = &scp_clks[i];
		int sub_clk_num;

		sub_clk_num = init_subsys_clks(pdev, pg->subclk_id,
				pg->subsys_clks);
		if (sub_clk_num < 0) {
			ret = sub_clk_num;
			goto fail;
		}

		if (mtk_is_mtcmos_enable()) {
			if (pg->clk_id == CLK_NONE)
				clk_num = 0;
			else
				clk_num = 1;

			ret_clk = mt_clk_register_power_gate(pg->name,
				pg->parent_name, clk[pg->clk_id],
				pg->subsys_clks, clk_num, sub_clk_num,
				pg->pd_id);
		} else
			ret_clk = mt_clk_register_power_gate(pg->name,
				pg->parent_name, NULL,
				NULL, 0, 0,
				pg->pd_id);

		if (IS_ERR(ret_clk)) {
			pr_err("[CCF] %s: Failed to register clk %s: %ld\n",
				__func__, pg->name, PTR_ERR(ret_clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[pg->id] = ret_clk;

#if MT_CCF_DEBUG
		pr_notice("[CCF] %s: pgate %d: %s\n", __func__,
				pg->id, pg->name);
#endif				/* MT_CCF_DEBUG */
	}
fail:
	return ret;
}

/*
 * device tree support
 */

/* TODO: remove this function */
static struct clk_onecell_data *alloc_clk_data(unsigned int clk_num)
{
	int i;
	struct clk_onecell_data *clk_data;

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return NULL;

	clk_data->clks = kcalloc(clk_num, sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks) {
		kfree(clk_data);
		return NULL;
	}

	clk_data->clk_num = clk_num;

	for (i = 0; i < clk_num; ++i)
		clk_data->clks[i] = ERR_PTR(-ENOENT);

	return clk_data;
}

/* TODO: remove this function */
static void __iomem *get_reg(struct device_node *np, int index)
{
#if DUMMY_REG_TEST
	return kzalloc(PAGE_SIZE, GFP_KERNEL);
#else
	return of_iomap(np, index);
#endif
}

static int clk_mt6761_scpsys_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

	pr_notice("clk mt6761 scpsys probe start\n");

	infracfg_base = get_reg(node, 0);
	spm_base = get_reg(node, 1);
	smi_common_base = get_reg(node, 2);
	infra_base = get_reg(node, 3);
	conn_base = get_reg(node, 4);

	if (!infracfg_base || !spm_base || !smi_common_base || !infra_base ||
		!conn_base) {
		pr_err("clk-mt6761-scpsys: missing reg\n");

		return -EINVAL;
	}

	clk_data = alloc_clk_data(SCP_NR_SYSS);
	if (!clk_data)
		return -ENOMEM;

	init_clk_scpsys(pdev, clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (ret) {
		pr_err("[CCF] %s:could not register clock provide\n",
			__func__);

		return ret;
	}

	if (mtk_is_mtcmos_enable()) {
		/* subsys init: per modem owner request,
		 *disable modem power first
		 */
		disable_subsys(SYS_MD1, MTCMOS_PWR);
	} else {	/*power on all subsys for bring up */
#ifndef CONFIG_FPGA_EARLY_PORTING
		if (subsys_is_on(SYS_MD1)) {	/*do after ccif*/
			spm_mtcmos_ctrl_md1_bus_prot(STA_POWER_DOWN);
			spm_mtcmos_ctrl_md1_pwr(STA_POWER_DOWN);
		}
		if (subsys_is_on(SYS_CONN)) {	/*do after ccif*/
			spm_mtcmos_ctrl_conn_bus_prot(STA_POWER_DOWN);
			spm_mtcmos_ctrl_conn_pwr(STA_POWER_DOWN);
		}
		spm_mtcmos_ctrl_dpy_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_dpy_bus_prot(STA_POWER_ON);
		spm_mtcmos_ctrl_dis_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_dis_bus_prot(STA_POWER_ON);
		spm_mtcmos_ctrl_ifr_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_ifr_bus_prot(STA_POWER_ON);
		spm_mtcmos_ctrl_mfg_async_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_mfg_async_bus_prot(STA_POWER_ON);
		spm_mtcmos_ctrl_mfg_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_mfg_bus_prot(STA_POWER_ON);
		spm_mtcmos_ctrl_mfg_core0_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_mfg_core0_bus_prot(STA_POWER_ON);
		spm_mtcmos_ctrl_cam_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_cam_bus_prot(STA_POWER_ON);
		spm_mtcmos_ctrl_vcodec_pwr(STA_POWER_ON);
		spm_mtcmos_ctrl_vcodec_bus_prot(STA_POWER_ON);
#endif
	}

	pr_notice("%s done(%d)\n", __func__, ret);

	return ret;
}

static const struct of_device_id of_match_clk_mt6761_scpsys[] = {
	{ .compatible = "mediatek,mt6761-scpsys-clk", },
	{}
};

static struct platform_driver clk_mt6761_scpsys_drv = {
	.probe = clk_mt6761_scpsys_probe,
	.driver = {
		.name = "clk-mt6761-scpsys",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6761_scpsys,
	},
};

static int __init clk_mt6761_scpsys_init(void)
{
	return platform_driver_register(&clk_mt6761_scpsys_drv);
}

static void __exit clk_mt6761_scpsys_exit(void)
{
}

arch_initcall(clk_mt6761_scpsys_init);
module_exit(clk_mt6761_scpsys_exit);
MODULE_LICENSE("GPL");
