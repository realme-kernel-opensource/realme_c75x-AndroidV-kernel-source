// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpu_pm.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "mtk_spm.h"
#include "mtk_spm_irq.h"
#include "mtk_spm_internal.h"

static inline unsigned int virq_to_hwirq(unsigned int virq)
{
	struct irq_desc *desc;
	unsigned int hwirq;

	desc = irq_to_desc(virq);
	WARN_ON(!desc);
	hwirq = desc ? desc->irq_data.hwirq : 0;
	return hwirq;
}

/***************************************************
 * spm edge trigger irq backup/restore
 ***************************************************/

/* edge_trigger_irq_list is defined in header file 'mtk_spm_irq_edge.h' */
#include "mtk_spm_irq_edge.h"

#define IRQ_NUMBER (sizeof(list)/sizeof(struct edge_trigger_irq_list))
static u32 edge_trig_irqs[IRQ_NUMBER];

static void mtk_spm_get_edge_trigger_irq(void)
{
	int i;
	struct device_node *node;
	unsigned int irq_type;
	unsigned int hwirq = 0;

	pr_info("[SPM] edge trigger irqs:\n");
	for (i = 0; i < IRQ_NUMBER; i++) {
		node = of_find_compatible_node(NULL, NULL, list[i].name);
		if (!node) {
			pr_info("[SPM] find '%s' node failed\n",
				list[i].name);
			continue;
		}

		edge_trig_irqs[i] =
			irq_of_parse_and_map(node, list[i].order);

		if (!edge_trig_irqs[i]) {
			pr_info("[SPM] get '%s' failed\n",
				list[i].name);
			continue;
		}

		/* Note: Initialize irq type to avoid pending irqs */
		irq_type = irq_get_trigger_type(edge_trig_irqs[i]);
		irq_set_irq_type(edge_trig_irqs[i], irq_type);
		hwirq = virq_to_hwirq(edge_trig_irqs[i]);
		SMC_CALL(ARGS, SPM_ARGS_IRQ_SMC_REG_IRQ, hwirq, i);
		pr_info("[SPM] '%s', irq=%d, type=%d\n", list[i].name,
			edge_trig_irqs[i], irq_type);
	}
}

static bool spm_in_idle;
static int cpu_pm_callback_wakeup_src_restore(
	struct notifier_block *self, unsigned long cmd, void *v)
{
	/* Note: cmd will be CPU_PM_ENTER/CPU_PM_EXIT/CPU_PM_ENTER_FAILED.
	 * Set edge trigger interrupt pending only in case CPU_PM_EXIT
	 */
	if (cmd == CPU_PM_EXIT && spm_in_idle)
		SMC_CALL(ARGS, SPM_ARGS_IRQ_SMC_SET_PENDING, 0, 0);

	return NOTIFY_OK;
}

static struct notifier_block mtk_spm_cpu_pm_notifier_block = {
	.notifier_call = cpu_pm_callback_wakeup_src_restore,
};

static unsigned int spm_irq_0;

void mtk_spm_irq_backup(void)
{
	spm_in_idle = true;
	SMC_CALL(ARGS, SPM_ARGS_IRQ_SMC_BACKUP, 0, 0);
}

void mtk_spm_irq_restore(void)
{
	SMC_CALL(ARGS, SPM_ARGS_IRQ_SMC_RESTORE, 0, 0);
	spm_in_idle = false;
}

unsigned int mtk_spm_get_irq_0(void)
{
	return spm_irq_0;
}

/********************************************************************
 * spm irq handler and initialize function
 *******************************************************************/

static irqreturn_t spm_irq0_handler(int irq, void *dev_id)
{
	u32 isr;
	unsigned long flags;
	struct twam_cfg twamsig;
	twam_handler_t twam_handler;
	u32 twam_idle_con = 0;
	u32 twam_idle_sel = 0;
	struct twam_select twam_sel;

	spin_lock_irqsave(&__spm_lock, flags);
	/* get ISR status */
	isr = spm_read(SPM_IRQ_STA);
	if (isr & ISRS_TWAM) {
		twamsig.byte[0].id = spm_read(SPM_TWAM_LAST_STA0);
		twamsig.byte[1].id = spm_read(SPM_TWAM_LAST_STA1);
		twamsig.byte[2].id = spm_read(SPM_TWAM_LAST_STA2);
		twamsig.byte[3].id = spm_read(SPM_TWAM_LAST_STA3);
		twam_idle_con = spm_read(SPM_TWAM_CON);
		twam_idle_sel = spm_read(SPM_TWAM_IDLE_SEL);
		twam_sel.signal[0] = (twam_idle_sel & 0x00000003);
		twam_sel.signal[1] = ((twam_idle_sel & 0x0000000C) >> 2);
		twam_sel.signal[2] = ((twam_idle_sel & 0x00000030) >> 4);
		twam_sel.signal[3] = ((twam_idle_sel & 0x000000C0) >> 6);
		twam_sel.id[0] = (twam_idle_con & 0x0001F000) >> 12;
		twam_sel.id[1] = ((twam_idle_con & 0x003E0000) >> 17);
		twam_sel.id[2] = ((twam_idle_con & 0x07C00000) >> 22);
		twam_sel.id[3] = ((twam_idle_con & 0xF8000000) >> 27);
		udelay(40); /* delay 1T @ 32K */
	}
	/* clean ISR status */
	SMC_CALL(IRQ0_HANDLER, isr, 0, 0);
	spin_unlock_irqrestore(&__spm_lock, flags);

	if (isr & (ISRS_SW_INT1)) {
		pr_info("[SPM] IRQ0 (ISRS_SW_INT1) HANDLER SHOULD NOT BE EXECUTED (0x%x)\n",
			isr);
		#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
		//spm_vcorefs_dump_dvfs_regs(NULL);
		#endif
		return IRQ_HANDLED;
	}

	twam_handler = spm_twam_handler_get();
	if ((isr & ISRS_TWAM) && twam_handler)
		twam_handler(&twamsig, &twam_sel);

	if (isr & (ISRS_SW_INT0 | ISRS_PCM_RETURN))
		pr_info("[SPM] IRQ0 HANDLER SHOULD NOT BE EXECUTED (0x%x)\n",
			isr);

	return IRQ_HANDLED;
}

struct spm_irq_desc {
	unsigned int irq;
	irq_handler_t handler;
};

int mtk_spm_irq_register(unsigned int spmirq0)
{
	int i, err, r = 0;
	unsigned int hwirq0 = 0;

	struct spm_irq_desc irqdesc[] = {
		{.irq = 0, .handler = spm_irq0_handler,}
	};
	irqdesc[0].irq = spmirq0;
	for (i = 0; i < ARRAY_SIZE(irqdesc); i++) {
		if (cpu_present(i)) {
			pr_info("[SPM] REQUEST IRQ (%d)\n", i);
			err = request_irq(irqdesc[i].irq, irqdesc[i].handler,
				IRQF_NO_SUSPEND |
				IRQF_PERCPU,
				"SPM", NULL);
			if (err) {
				pr_info("[SPM] FAILED TO REQUEST IRQ%d (%d)\n",
					i, err);
				r = -EPERM;
			}
		}
	}

	/* Assing local spm_irq_0 */
	spm_irq_0 = spmirq0;

	mtk_spm_get_edge_trigger_irq();
	hwirq0 = virq_to_hwirq(spm_irq_0);
	SMC_CALL(ARGS, SPM_ARGS_IRQ_SMC_SET_WAKEUP_SOURCES, hwirq0, 0);

	cpu_pm_register_notifier(&mtk_spm_cpu_pm_notifier_block);

	return r;
}

