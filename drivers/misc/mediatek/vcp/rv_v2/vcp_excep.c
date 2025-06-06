// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/vmalloc.h>         /* needed by vmalloc */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <mt-plat/aee.h>
//#include <mt-plat/sync_write.h>
#include <linux/sched_clock.h>
#include <linux/ratelimit.h>
#include <linux/delay.h>
#include <linux/of_reserved_mem.h>
#include <linux/slab.h>
#include "mmup.h"
#include "vcp.h"
#include "vcp_ipi_pin.h"
#include "vcp_helper.h"
#include "vcp_excep.h"
#include "vcp_feature_define.h"
#include "vcp_reservedmem_define.h"
#include "vcp_status.h"

#define POLLING_RETRY 100

struct vcp_dump_st {
	uint8_t *detail_buff;
	uint8_t *ramdump;
	uint32_t ramdump_length;
	/* use prefix to get size or offset in O(1) to save memory */
	uint32_t prefix[MDUMP_TOTAL];
};

struct vcp_dump_st vcp_dump;

struct vcp_status_reg *c0_m;
struct vcp_status_reg *c0_t1_m;
struct vcp_status_reg *c1_m;
struct vcp_status_reg *c1_t1_m;
void (*vcp_do_tbufdump)(void) = NULL;

void __iomem *vcp_res_req_status_reg;

static struct mutex vcp_excep_mutex;
int vcp_excep_mode;
unsigned int vcp_reset_counts = 0xFFFFFFFF;

static atomic_t coredumping = ATOMIC_INIT(0);
static DECLARE_COMPLETION(vcp_coredump_comp);
static uint32_t get_MDUMP_size_accumulate(enum MDUMP_t type)
{
	return vcp_dump.prefix[type];
}

static uint8_t *get_MDUMP_addr(enum MDUMP_t type)
{
	return (uint8_t *)(vcp_dump.ramdump + vcp_dump.prefix[type - 1]);
}

uint32_t vcp_dump_size_probe(struct platform_device *pdev)
{
	uint32_t i, ret;

	for (i = MDUMP_L2TCM; i < MDUMP_TOTAL; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
			"memorydump", i - 1, &vcp_dump.prefix[i]);
		if (ret) {
			pr_notice("[VCP] %s:Cannot get memorydump size(%d)\n", __func__, i - 1);
			return -1;
		}
		vcp_dump.prefix[i] += vcp_dump.prefix[i - 1];
	}
	return 0;
}

void vcp_dump_last_regs(int mmup_enable)
{
	uint32_t i;

	if (mmup_enable == 0) {
		pr_notice("[VCP] power off, do not %s\n", __func__);
		return;
	}

	if (!IS_ERR((void const *) vcpreg.vcp_vlp_ao_rsvd7))
		pr_notice("[VCP] ready bit = %08x\n", readl(VLP_AO_RSVD7));
	if (!IS_ERR((void const *) vcpreg.vcp_pwr_ack))
		pr_notice("[VCP] pwr ack = %08x\n", readl(VCP_PWR_ACK));
	if (!IS_ERR((void const *) vcpreg.vcp_clk_sys))
		pr_notice("[VCP] pdn = %08x\n", readl(VCP_PDN));
	if (!IS_ERR((void const *) vcpreg.vcp_clk_sys))
		pr_notice("[VCP] fenc = %08x\n", readl(VCP_FENC));

	c0_m->status = readl(R_CORE0_STATUS);
	c0_m->pc = readl(R_CORE0_MON_PC);
	c0_m->lr = readl(R_CORE0_MON_LR);
	c0_m->sp = readl(R_CORE0_MON_SP);
	c0_m->pc_latch = readl(R_CORE0_MON_PC_LATCH);
	c0_m->lr_latch = readl(R_CORE0_MON_LR_LATCH);
	c0_m->sp_latch = readl(R_CORE0_MON_SP_LATCH);
	if (vcpreg.twohart) {
		c0_t1_m->pc = readl(R_CORE0_T1_MON_PC);
		c0_t1_m->lr = readl(R_CORE0_T1_MON_LR);
		c0_t1_m->sp = readl(R_CORE0_T1_MON_SP);
		c0_t1_m->pc_latch = readl(R_CORE0_T1_MON_PC_LATCH);
		c0_t1_m->lr_latch = readl(R_CORE0_T1_MON_LR_LATCH);
		c0_t1_m->sp_latch = readl(R_CORE0_T1_MON_SP_LATCH);
	}

	pr_notice("[VCP] c0_status = %08x\n", c0_m->status);
	pr_notice("[VCP] c0_pc = %08x\n", c0_m->pc);
	pr_notice("[VCP] c0_pc2 = %08x\n", readl(R_CORE0_MON_PC));
	pr_notice("[VCP] c0_lr = %08x\n", c0_m->lr);
	pr_notice("[VCP] c0_sp = %08x\n", c0_m->sp);
	pr_notice("[VCP] c0_pc_latch = %08x\n", c0_m->pc_latch);
	pr_notice("[VCP] c0_lr_latch = %08x\n", c0_m->lr_latch);
	pr_notice("[VCP] c0_sp_latch = %08x\n", c0_m->sp_latch);
	if (vcpreg.twohart) {
		pr_notice("[VCP] c0_t1_pc = %08x\n", c0_t1_m->pc);
		pr_notice("[VCP] c0_t1_pc2 = %08x\n", readl(R_CORE0_T1_MON_PC));
		pr_notice("[VCP] c0_t1_lr = %08x\n", c0_t1_m->lr);
		pr_notice("[VCP] c0_t1_sp = %08x\n", c0_t1_m->sp);
		pr_notice("[VCP] c0_t1_pc_latch = %08x\n", c0_t1_m->pc_latch);
		pr_notice("[VCP] c0_t1_lr_latch = %08x\n", c0_t1_m->lr_latch);
		pr_notice("[VCP] c0_t1_sp_latch = %08x\n", c0_t1_m->sp_latch);
	}
	pr_notice("[VCP] RSTN_CLR = %08x RSTN_CLR = %08x\n",
		readl(R_CORE0_SW_RSTN_CLR), readl(R_CORE0_SW_RSTN_SET));
	pr_notice("[VCP] irq sta: %08x,%08x,%08x,%08x\n", readl(R_CORE0_IRQ_STA0),
		readl(R_CORE0_IRQ_STA1), readl(R_CORE0_IRQ_STA2), readl(R_CORE0_IRQ_STA3));
	pr_notice("[VCP] irq en: %08x,%08x,%08x,%08x\n", readl(R_CORE0_IRQ_EN0),
		readl(R_CORE0_IRQ_EN1), readl(R_CORE0_IRQ_EN2), readl(R_CORE0_IRQ_EN3));
	pr_notice("[VCP] irq wakeup en: %08x,%08x,%08x,%08x\n", readl(R_CORE0_IRQ_SLP0),
		readl(R_CORE0_IRQ_SLP1), readl(R_CORE0_IRQ_SLP2), readl(R_CORE0_IRQ_SLP3));
	pr_notice("[VCP] SEC GPR: %08x,%08x,%08x,%08x\n", readl(R_GPR0_CFGREG_SEC),
		readl(R_GPR1_CFGREG_SEC), readl(R_GPR2_CFGREG_SEC), readl(R_GPR3_CFGREG_SEC));
	pr_notice("[VCP] AP GPR: %08x,%08x,%08x,%08x\n", readl(AP_R_GPR0), readl(AP_R_GPR1),
		readl(AP_R_GPR2), readl(AP_R_GPR3));
	pr_notice("[VCP] core GPR: %08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
		readl(R_CORE0_GENERAL_REG0), readl(R_CORE0_GENERAL_REG1),
		readl(R_CORE0_GENERAL_REG2), readl(R_CORE0_GENERAL_REG3),
		readl(R_CORE0_GENERAL_REG4), readl(R_CORE0_GENERAL_REG5),
		readl(R_CORE0_GENERAL_REG6), readl(R_CORE0_GENERAL_REG7));
	pr_notice("[VCP] GIPC %x %x\n", readl(R_GIPC_IN_SET), readl(R_GIPC_IN_CLR));

	if (vcpreg.core_nums == 2)
		mmup_dump_last_regs();

	if (!IS_ERR((void const *) vcpreg.cfg_pwr))
		pr_notice("[VCP] SLP_EN = %08x PWR_STATUS = %08x\n",
			readl(VCP_R_SLP_EN), readl(VCP_POWER_STATUS));
	pr_notice("[VCP] SRAM region: %08x,%08x,%08x,%08x\n",
		readl(vcpreg.sram), readl(vcpreg.sram + 0x4),
		readl(vcpreg.sram + 0x8), readl(vcpreg.sram + 0xc));
	pr_notice("[VCP] SRAM loader: %08x,%08x,%08x,%08x\n",
		readl(vcpreg.sram + 0x300), readl(vcpreg.sram + 0x304),
		readl(vcpreg.sram + 0x308), readl(vcpreg.sram + 0x30c));
	pr_notice("[VCP] SRAM image: %08x,%08x,%08x,%08x\n",
		readl(vcpreg.sram + 0x2000), readl(vcpreg.sram + 0x2004),
		readl(vcpreg.sram + 0x2008), readl(vcpreg.sram + 0x200c));

	if (vcp_res_req_status_reg)
		pr_notice("[VCP] resource request status: %08x\n",
			readl(vcp_res_req_status_reg));
	pr_notice("[VCP] CLK_SYS/BUS/APSRC/DDREN REQ: %08x,%08x,%08x,%08x\n",
		readl(R_CORE0_CLK_SYS_REQ), readl(R_CORE0_BUS_REQ),
		readl(R_CORE0_APSRC_REQ), readl(R_CORE0_DDREN_REQ));
	pr_notice("[VCP] SYS_CTRL/SLP_PROT_EN: %08x,%08x\n",
		readl(VCP_SYS_CTRL), readl(VCP_SLP_PROT_EN));
	pr_notice("[VCP] DDREN_NEW_CTRL/DDREN_NEW_CTRL2/APSRC_CTRL2: %08x,%08x,%08x\n",
		readl(VCP_DDREN_NEW_CTRL), readl(VCP_DDREN_NEW_CTRL2),
		readl(VCP_APSRC_CTRL2));

	if (vcpreg.bus_prot) {
		pr_notice("[VCP] bus prot_en/ack\n");
		for (i = 0; i < 2; i++) {
			pr_notice("[VCP] %08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
				readl(VCP_BUS_PROT + i * 0x20),
				readl(VCP_BUS_PROT + i * 0x20 + 0x4),
				readl(VCP_BUS_PROT + i * 0x20 + 0x8),
				readl(VCP_BUS_PROT + i * 0x20 + 0xC),
				readl(VCP_BUS_PROT + i * 0x20 + 0x10),
				readl(VCP_BUS_PROT + i * 0x20 + 0x14),
				readl(VCP_BUS_PROT + i * 0x20 + 0x18),
				readl(VCP_BUS_PROT + i * 0x20 + 0x1C));
		}
	}

	/* bus debug reg dump */
	pr_notice("[VCP] BUS DBG CON %08x, port num = %d\n",
		readl(VCP_BUS_DBG_CON), vcpreg.bus_debug_num_ports);
	for (i = 0; i < vcpreg.bus_debug_num_ports; i++) {
		pr_notice("[VCP] bus debug result%d = %08x\n",
			i, readl(VCP_BUS_DBG_RESULT0 + i * 4));
	}

	/* bus tracker reg dump */
	pr_notice("[VCP] BUS TRACKER CON\n");
	for (i = 0; i < 3; i++) {
		pr_notice("[VCP] %08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
			readl(VCP_BUS_TRACKER_CON + i * 0x20),
			readl(VCP_BUS_TRACKER_CON + i * 0x20 + 0x4),
			readl(VCP_BUS_TRACKER_CON + i * 0x20 + 0x8),
			readl(VCP_BUS_TRACKER_CON + i * 0x20 + 0xC),
			readl(VCP_BUS_TRACKER_CON + i * 0x20 + 0x10),
			readl(VCP_BUS_TRACKER_CON + i * 0x20 + 0x14),
			readl(VCP_BUS_TRACKER_CON + i * 0x20 + 0x18),
			readl(VCP_BUS_TRACKER_CON + i * 0x20 + 0x1C));
	}
	pr_notice("[VCP] BUS TRACKER AR\n");
	for (i = 0; i < 8; i++) {
		pr_notice("[VCP] %08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
			readl(VCP_BUS_TRACKER_AR_TRACK_L + i * 0x10),
			readl(VCP_BUS_TRACKER_AR_TRACK_H + i * 0x10),
			readl(VCP_BUS_TRACKER_AR_TRACK_L + i * 0x10 + 0x4),
			readl(VCP_BUS_TRACKER_AR_TRACK_H + i * 0x10 + 0x4),
			readl(VCP_BUS_TRACKER_AR_TRACK_L + i * 0x10 + 0x8),
			readl(VCP_BUS_TRACKER_AR_TRACK_H + i * 0x10 + 0x8),
			readl(VCP_BUS_TRACKER_AR_TRACK_L + i * 0x10 + 0xC),
			readl(VCP_BUS_TRACKER_AR_TRACK_H + i * 0x10 + 0xC));
	}
	pr_notice("[VCP] BUS TRACKER AW\n");
	for (i = 0; i < 8; i++) {
		pr_notice("[VCP] %08x,%08x,%08x,%08x,%08x,%08x,%08x,%08x\n",
			readl(VCP_BUS_TRACKER_AW_TRACK_L + i * 0x10),
			readl(VCP_BUS_TRACKER_AW_TRACK_H + i * 0x10),
			readl(VCP_BUS_TRACKER_AW_TRACK_L + i * 0x10 + 0x4),
			readl(VCP_BUS_TRACKER_AW_TRACK_H + i * 0x10 + 0x4),
			readl(VCP_BUS_TRACKER_AW_TRACK_L + i * 0x10 + 0x8),
			readl(VCP_BUS_TRACKER_AW_TRACK_H + i * 0x10 + 0x8),
			readl(VCP_BUS_TRACKER_AW_TRACK_L + i * 0x10 + 0xC),
			readl(VCP_BUS_TRACKER_AW_TRACK_H + i * 0x10 + 0xC));
	}

	/* mmup2infra RX and TX reg dump */
	pr_notice("[VCP] mmup2infra tx: %08x\n", readl(VCP_TO_INFRA_TX));

	vcp_do_tbufdump();
	if (vcpreg.core_nums == 2)
		mmup_do_tbufdump_RV33();
}

void vcp_do_tbufdump_RV33(void)
{
	uint32_t tmp, index, offset, wbuf_ptr;
	int i;

	wbuf_ptr = readl(R_CORE0_TBUF_WPTR);
	tmp = readl(R_CORE0_DBG_CTRL) & (~M_CORE_TBUF_DBG_SEL);
	for (i = 0; i < 16; i++) {
		index = (wbuf_ptr + i) / 2;
		offset = ((wbuf_ptr + i) % 2) * 0x8;
		writel(tmp | (index << S_CORE_TBUF_DBG_SEL), R_CORE0_DBG_CTRL);
		pr_notice("[VCP] C0:%02d:0x%08x::0x%08x\n", i,
			readl(R_CORE0_TBUF_DATA31_0 + offset), readl(R_CORE0_TBUF_DATA63_32 + offset));
	}
}

void vcp_do_tbufdump_RV55(void)
{
	uint32_t tmp, tmp1, index, offset, wbuf_ptr, wbuf1_ptr;
	int i;

	wbuf1_ptr = readl(R_CORE0_TBUF_WPTR);
	wbuf_ptr = wbuf1_ptr & 0x1f;
	wbuf1_ptr = wbuf1_ptr >> 8;
	tmp = readl(R_CORE0_DBG_CTRL) & M_CORE_TBUF_DBG_SEL_RV55;
	for (i = 0; i < 32; i++) {
		index = ((wbuf_ptr + i) / 4) & 0x7;
		offset = ((wbuf_ptr + i) % 4) * 0x8;
		tmp1 = (index << S_CORE_TBUF_S) | (index << S_CORE_TBUF1_S);
		writel(tmp | tmp1, R_CORE0_DBG_CTRL);
		pr_notice("[VCP] C0:H0:%02d:0x%08x::0x%08x\n", i,
			readl(R_CORE0_TBUF_DATA31_0 + offset), readl(R_CORE0_TBUF_DATA63_32 + offset));
	}

	for (i = 0; i < 32; i++) {
		index = (((wbuf1_ptr + i) / 4) & 0x7) | 0x8;
		offset = ((wbuf1_ptr + i) % 4) * 0x8;
		tmp1 = (index << S_CORE_TBUF_S) | (index << S_CORE_TBUF1_S);
		writel(tmp | tmp1, R_CORE0_DBG_CTRL);
		pr_notice("[VCP] C0:H1:%02d:0x%08x::0x%08x\n", i,
			readl(R_CORE0_TBUF_DATA31_0 + offset), readl(R_CORE0_TBUF_DATA63_32 + offset));
	}
}

static inline unsigned long vcp_do_dump(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
			MTK_TINYSYS_VCP_KERNEL_OP_DUMP_START,
			0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}

static inline unsigned long vcp_do_polling(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_TINYSYS_VCP_CONTROL,
			MTK_TINYSYS_VCP_KERNEL_OP_DUMP_POLLING,
			0, 0, 0, 0, 0, 0, &res);
	return res.a0;
}


/*
 * this function need VCP to keeping awaken
 * vcp_crash_dump: dump vcp tcm info.
 * @param MemoryDump:   vcp dump struct
 * @param vcp_core_id:  core id
 * @return:             vcp dump size
 */
static unsigned int vcp_crash_dump(enum vcp_core_id id)
{
	unsigned int vcp_dump_size = 0;
	unsigned int vcp_awake_fail_flag;

	/*flag use to indicate vcp awake success or not*/
	vcp_awake_fail_flag = 0;

	/*check SRAM lock ,awake vcp*/
	if (vcp_awake_lock((void *)id) == -1) {
		pr_notice("[VCP] %s: awake vcp fail, vcp id=%u\n", __func__, id);
		vcp_awake_fail_flag = 1;
	}
	if (vcpreg.secure_dump) {
		unsigned long polling = 1;
		int retry = POLLING_RETRY;

		vcp_do_dump();

		while (polling != 0 && retry > 0) {
			polling = vcp_do_polling();
			if (!polling)
				break;
			if (polling == -EIO) {
				pr_notice("[VCP] %s: vcp dump polling not support\n", __func__);
				break;
			}
			mdelay(polling);
			retry--;
		}

		/* tbuf is different between rv33/rv55 */
		if (vcpreg.twohart) {
			/* RV55 */
			uint32_t *out = (uint32_t *)get_MDUMP_addr(MDUMP_TBUF);
			int i;

			for (i = 0; i < 32; i++) {
				pr_notice("[VCP] C0:%02d:0x%08x::0x%08x\n",
							i, *(out + i * 2), *(out + i * 2 + 1));
			}
			for (i = 0; i < 32; i++) {
				pr_notice("[VCP] C1:%02d:0x%08x::0x%08x\n",
							i, *(out + 64 + i * 2),
							*(out + 64 + i * 2 + 1));
			}
		} else {
			/* RV33 */
			uint32_t *out = (uint32_t *)get_MDUMP_addr(MDUMP_TBUF);
			int i;

			for (i = 0; i < 16; i++) {
				pr_notice("[VCP] C0:%02d:0x%08x::0x%08x\n",
					i, *(out + i * 2), *(out + i * 2 + 1));
			}
			for (i = 0; i < 16; i++) {
				pr_notice("[VCP] C1:%02d:0x%08x::0x%08x\n",
					i, *(out + i * 2 + 16), *(out + i * 2 + 17));
			}
		}
		vcp_dump_size = get_MDUMP_size_accumulate(MDUMP_DRAM);
	}

	dsb(SY); /* may take lot of time */

	/*check SRAM unlock*/
	if (vcp_awake_fail_flag != 1) {
		if (vcp_awake_unlock((void *)id) == -1)
			pr_debug("[VCP]%s awake unlock fail, vcp id=%u\n",
				__func__, id);
	}

	return vcp_dump_size;
}

/*
 * generate aee argument with vcp register dump
 * @param aed_str:  exception description
 * @param id:       identify vcp core id
 */
static void vcp_prepare_aed_dump(char *aed_str, enum vcp_core_id id)
{
	char *vcp_A_log = NULL;
	size_t offset = 0;

	pr_notice("[VCP] %s begins:%s\n", __func__, aed_str);
	vcp_dump_last_regs(mmup_enable_count());

	vcp_A_log = vcp_pickup_log_for_aee();

	if (vcp_dump.detail_buff == NULL) {
		pr_notice("[VCP AEE]detail buf is null\n");
	} else {
		/* prepare vcp aee detail information*/
		memset(vcp_dump.detail_buff, 0, VCP_AED_STR_LEN);

		offset += VCP_CHECK_AED_STR_LEN(snprintf(vcp_dump.detail_buff + offset,
		VCP_AED_STR_LEN - offset, "%s\n", aed_str), offset);

		offset += VCP_CHECK_AED_STR_LEN(snprintf(vcp_dump.detail_buff + offset,
		VCP_AED_STR_LEN - offset,
		"core0 pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
		c0_m->pc, c0_m->lr, c0_m->sp), offset);

		if (!vcpreg.twohart)
			goto core1;

		offset += VCP_CHECK_AED_STR_LEN(snprintf(vcp_dump.detail_buff + offset,
		VCP_AED_STR_LEN - offset,
		"hart1 pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
		c0_t1_m->pc, c0_t1_m->lr, c0_t1_m->sp), offset);
core1:
		if (vcpreg.core_nums == 1)
			goto end;

		offset += VCP_CHECK_AED_STR_LEN(snprintf(vcp_dump.detail_buff + offset,
		VCP_AED_STR_LEN - offset,
		"core1 pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
		c1_m->pc, c1_m->lr, c1_m->sp), offset);

		if (!vcpreg.twohart_core1)
			goto end;

		offset += VCP_CHECK_AED_STR_LEN(snprintf(vcp_dump.detail_buff + offset,
		VCP_AED_STR_LEN - offset,
		"hart1 pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
		c1_t1_m->pc, c1_t1_m->lr, c1_t1_m->sp), offset);
end:
		if (vcp_A_log) {
			offset += VCP_CHECK_AED_STR_LEN(snprintf(vcp_dump.detail_buff + offset,
			VCP_AED_STR_LEN - offset, "last log:\n%s", vcp_A_log), offset);
		}

		vcp_dump.detail_buff[VCP_AED_STR_LEN - 1] = '\0';
	}

	/*prepare vcp A db file*/
	vcp_dump.ramdump_length = 0;
	vcp_dump.ramdump_length = vcp_crash_dump(id);

	pr_notice("[VCP] %s ends, @%p, size = %x\n", __func__,
		vcp_dump.ramdump, vcp_dump.ramdump_length);
}

/*
 * generate an exception according to exception type
 * NOTE: this function may be blocked and
 * should not be called in interrupt context
 * @param type: exception type
 */
void vcp_aed(enum VCP_RESET_TYPE type, enum vcp_core_id id)
{
	char *vcp_aed_title = NULL;

	if (vcp_excep_mode == VCP_NO_EXCEP) {
		pr_debug("[VCP]ee disable value=%d\n", vcp_excep_mode);
		return;
	}  else if ((vcp_excep_mode == VCP_KE_ENABLE) || (id != VCP_ID))
		BUG_ON(1);

	mutex_lock(&vcp_excep_mutex);

	/* get vcp title and exception type*/
	switch (type) {
	case RESET_TYPE_WDT:
		if (id == VCP_ID)
			vcp_aed_title = "VCP wdt reset";
		else
			vcp_aed_title = "MMUP wdt reset";
		break;
	case RESET_TYPE_AWAKE:
		if (id == VCP_ID)
			vcp_aed_title = "VCP awake reset";
		else
			vcp_aed_title = "MMUP awake reset";
		break;
	case RESET_TYPE_CMD:
		if (id == VCP_ID)
			vcp_aed_title = "VCP cmd reset";
		else
			vcp_aed_title = "MMUP cmd reset";
		break;
	case RESET_TYPE_TIMEOUT:
		if (id == VCP_ID)
			vcp_aed_title = "VCP timeout reset";
		else
			vcp_aed_title = "MMUP timeout reset";
		break;
	}
	vcp_get_log(id);
	/*print vcp message*/
	pr_debug("vcp_aed_title=%s\n", vcp_aed_title);

	vcp_prepare_aed_dump(vcp_aed_title, id);

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	/* vcp aed api, only detail information available*/
	aed_common_exception_api("vcp", NULL, 0, NULL, 0,
			vcp_dump.detail_buff, DB_OPT_DEFAULT);
#endif

	pr_debug("[VCP] vcp exception dump is done\n");

	mutex_unlock(&vcp_excep_mutex);
}

static ssize_t vcp_A_dump_show(struct file *filep,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;

	pr_debug("[VCP] %s begins\n", __func__);


	mutex_lock(&vcp_excep_mutex);

	if (offset >= 0 && offset < vcp_dump.ramdump_length) {
		if ((offset + size) > vcp_dump.ramdump_length)
			size = vcp_dump.ramdump_length - offset;

		memcpy(buf, vcp_dump.ramdump + offset, size);
		length = size;

		/* clean the buff after readed */
		memset(vcp_dump.ramdump + offset, 0xff, size);
		/* log for the first and latest cleanup */
		if (offset == 0 || size == (vcp_dump.ramdump_length - offset))
			pr_notice("[VCP] %s ramdump cleaned of:0x%llx sz:%zu\n", __func__,
				offset, size);

		//clean the buff after readed
		memset(vcp_dump.ramdump + offset, 0xff, size);
		/* the last time read vcp_dump buffer has done
		 * so the next coredump flow can be continued
		 */
		if (size == vcp_dump.ramdump_length - offset) {
			atomic_set(&coredumping, false);
			pr_notice("[VCP] coredumping:%d, coredump complete\n",
				atomic_read(&coredumping));
			complete(&vcp_coredump_comp);
		}
	}

	mutex_unlock(&vcp_excep_mutex);

	return length;
}


struct bin_attribute bin_attr_vcp_dump = {
	.attr = {
		.name = "vcp_dump",
		.mode = 0444,
	},
	.size = 0,
	.read = vcp_A_dump_show,
};



/*
 * init a work struct
 */
int vcp_excep_init(void)
{
	uint32_t *out;

	mutex_init(&vcp_excep_mutex);

	/* alloc dump memory */
	vcp_dump.detail_buff = vmalloc(VCP_AED_STR_LEN);
	if (!vcp_dump.detail_buff)
		return -1;

	/* vcp_status_reg init */
	c0_m = vmalloc(sizeof(struct vcp_status_reg));
	if (!c0_m)
		return -1;
	if (vcpreg.twohart) {
		c0_t1_m = vmalloc(sizeof(struct vcp_status_reg));
		if (!c0_t1_m)
			return -1;
	}
	if (vcpreg.core_nums == 2) {
		c1_m = vmalloc(sizeof(struct vcp_status_reg));
		if (!c1_m)
			return -1;
	}
	if (vcpreg.core_nums == 2 && vcpreg.twohart_core1) {
		c1_t1_m = vmalloc(sizeof(struct vcp_status_reg));
		if (!c1_t1_m)
			return -1;
	}
	/* vcp_do_tbufdump init, because tbuf is different between rv33/rv55 */
	if (vcpreg.twohart)
		vcp_do_tbufdump = vcp_do_tbufdump_RV55;
	else
		vcp_do_tbufdump = vcp_do_tbufdump_RV33;

	if (vcpreg.secure_dump) {
		vcp_dump.ramdump =
			(uint8_t *)(void *)vcp_get_reserve_mem_virt(VCP_SECURE_DUMP_ID);

		if (!vcp_dump.ramdump)
			return -1;

		out = (uint32_t *)vcp_dump.ramdump;
		*out = 0;
		pr_notice("[VCP] %s %d %d %d %d\n", __func__,
				*out, *(out + 1), *(out + 2), *(out + 3));

	} else {
		pr_notice("[VCP] %s secure dump not supported\n", __func__);
	}


	/* init global values */
	vcp_dump.ramdump_length = 0;

	return 0;
}
