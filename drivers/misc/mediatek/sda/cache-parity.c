// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/cputype.h>
#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/sched/clock.h>
#include <linux/workqueue.h>
#include <mt-plat/aee.h>
#include <aed.h>
#include "cputype.h"

#define ECC_UE_BIT			(0x1 << 29)
#define ECC_CE_BIT			(0x3 << 24)
#define ECC_DE_BIT			(0x1 << 23)
#define ECC_SERR_BIT			(0x1F)
#define ECC_CE_AT_LEAST_ONE_ERR		(0x2 << 24)
#define ECC_SERR_FROM_DATA_BUFF		(0x2)
#define ECC_IRQ_TRIGGER_THRESHOLD	(1)
#define MAX_COMPLEX_IRQ_NUM		(4)

struct parity_record_t {
	unsigned int check_offset;
	unsigned int check_mask;
	unsigned int dump_offset;
	unsigned int dump_length;
	unsigned int clear_offset;
	unsigned int clear_mask;
};

struct parity_irq_record_t {
	int irq;
	struct parity_record_t parity_record;
};

struct parity_irq_config_t {
	unsigned int target_cpu;
	struct parity_record_t parity_record;
};

union err_record {
	struct _v1 {
		u32 irq;
		u32 status;
		bool is_err;
	} v1;
	struct _v2 {
		u32 irq;
		int cpu;
		u64 misc0_el1;
		u64 status_el1;
		bool is_err;
	} v2;
	struct _v3 {
		u32 irq;
		int cpu;
		u64 misc0_el1;
		u64 status_el1;
		bool is_err;
	} v3;
};

struct cache_parity {
	struct work_struct work;

	/* setting from device tree */
	unsigned int ver;
	unsigned int nr_irq;
	unsigned int nr_complex_irq;
	int arm_dsu_ecc_hwirq;
	int arm_complex_ecc_hwirq[MAX_COMPLEX_IRQ_NUM];
	void __iomem *cache_parity_base;

	/* recorded parity errors */
	atomic_t nr_err;
	u64 timestampe;
	union err_record *record;
};
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define ECC_LOG(fmt, ...) \
	do { \
		pr_notice(fmt, __VA_ARGS__); \
		aee_sram_printk(fmt, __VA_ARGS__); \
	} while (0)
#else
#define ECC_LOG(fmt, ...)
#endif

static struct cache_parity cache_parity;
static struct parity_irq_record_t *parity_irq_record;
static DEFINE_SPINLOCK(parity_isr_lock);

void __attribute__((weak)) ecc_dump_debug_info(void)
{
	pr_notice("%s is not implemented\n", __func__);
}

static ssize_t cache_status_show(struct device_driver *driver, char *buf)
{
	unsigned int nr_err;

	nr_err = atomic_read(&cache_parity.nr_err);

	if (nr_err)
		return scnprintf(buf, PAGE_SIZE, "True, %u times (%llu ns)\n",
				nr_err, cache_parity.timestampe);
	else
		return scnprintf(buf, PAGE_SIZE, "False\n");
}

static DRIVER_ATTR_RO(cache_status);

static bool cache_parity_trigger_bug(bool force, int irq, u64 misc0, u64 status)
{
	bool trigger_bug = likely(force) || (aee_get_mode() < AEE_MODE_CUSTOMER_ENG);

	if (trigger_bug && (status & (ECC_UE_BIT | ECC_DE_BIT))) {
		const char *err_type = (status & ECC_UE_BIT) ? "UE" : "DE";

		ECC_LOG("%s: %s(%s), %s %d, %s: 0x%016llx, %s: 0x%016llx\n",
			"Cache Parity Issue",
			"Cache ECC errors", err_type,
			"irq", irq,
			"misc0_el1", misc0,
			"status_el1", status);
		BUG();
	}
	return false;
}

static void cache_parity_irq_work(struct work_struct *w)
{
	static char *buf;
	int n, i;
	u64 status;

	if (!buf) {
		buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!buf)
			goto call_aee;
	}

	n = 0;

	if (cache_parity.ver == 1) {
		n += scnprintf(buf + n, PAGE_SIZE - n, "cache parity error,");
		if (n > PAGE_SIZE)
			goto call_aee;

		for (i = 0; i < cache_parity.nr_irq; i++) {
			if (!cache_parity.record[i].v1.is_err)
				continue;
			n += scnprintf(buf + n, PAGE_SIZE - n, "%s:%d, %s:0x%x ",
				"irq", cache_parity.record[i].v1.irq,
				"status", cache_parity.record[i].v1.status);
			if (n > PAGE_SIZE)
				goto call_aee;
		}
	} else if (cache_parity.ver == 2) {
		n += scnprintf(buf + n, PAGE_SIZE - n, "ECC errors(");
		if (n > PAGE_SIZE)
			goto call_aee;

		for (status = 0, i = 0; i < cache_parity.nr_irq; i++) {
			if (cache_parity.record[i].v2.is_err) {
				status = cache_parity.record[i].v2.status_el1;
				break;
			}
		}
		if (status & ECC_UE_BIT)
			n += scnprintf(buf + n, PAGE_SIZE - n, "UE");
		else if (status & ECC_CE_BIT)
			n += scnprintf(buf + n, PAGE_SIZE - n, "CE");
		else if (status & ECC_DE_BIT)
			n += scnprintf(buf + n, PAGE_SIZE - n, "DE");
		else
			n += scnprintf(buf + n, PAGE_SIZE - n, "NA");
		if (n > PAGE_SIZE)
			goto call_aee;

		n += scnprintf(buf + n, PAGE_SIZE - n, "),");
		if (n > PAGE_SIZE)
			goto call_aee;

		for (i = 0; i < cache_parity.nr_irq; i++) {
			if (!cache_parity.record[i].v2.is_err)
				continue;
			n += scnprintf(buf + n, PAGE_SIZE - n,
				"%s:%d,%s:0x%016llx,%s:0x%016llx ",
				"irq", cache_parity.record[i].v2.irq,
				"misc0_el1",
				cache_parity.record[i].v2.misc0_el1,
				"status_el1",
				cache_parity.record[i].v2.status_el1);
			if (n > PAGE_SIZE)
				goto call_aee;
		}
	} else if (cache_parity.ver == 3) {
		n += scnprintf(buf + n, PAGE_SIZE - n, "ECC errors(");
		if (n > PAGE_SIZE)
			goto call_aee;

		for (status = 0, i = 0; i < cache_parity.nr_irq; i++) {
			if (cache_parity.record[i].v3.is_err) {
				status = cache_parity.record[i].v3.status_el1;
				break;
			}
		}
		if (status & ECC_UE_BIT)
			n += scnprintf(buf + n, PAGE_SIZE - n, "UE");
		else if (status & ECC_CE_BIT)
			n += scnprintf(buf + n, PAGE_SIZE - n, "CE");
		else if (status & ECC_DE_BIT)
			n += scnprintf(buf + n, PAGE_SIZE - n, "DE");
		else
			n += scnprintf(buf + n, PAGE_SIZE - n, "NA");
		if (n > PAGE_SIZE)
			goto call_aee;

		n += scnprintf(buf + n, PAGE_SIZE - n, "),");
		if (n > PAGE_SIZE)
			goto call_aee;

		for (i = 0; i < cache_parity.nr_irq; i++) {
			if (!cache_parity.record[i].v3.is_err)
				continue;
			n += scnprintf(buf + n, PAGE_SIZE - n,
				"%s:%d,%s:0x%016llx,%s:0x%016llx ",
				"irq", cache_parity.record[i].v3.irq,
				"misc0_el1",
				cache_parity.record[i].v3.misc0_el1,
				"status_el1",
				cache_parity.record[i].v3.status_el1);
			if (n > PAGE_SIZE)
				goto call_aee;
		}
	} else {
		buf[0] = '\0';
		pr_debug("Unknown Cache Error Irq\n");
	}

call_aee:
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	/* Note: the length of message printed by aee_kernel_exception is limited by
	 * KERNEL_REPORT_LENGTH (should be 344), if oversize, it will be cut off.
	 */
	aee_kernel_exception("cache parity", "%s\n%s", buf, "CRDISPATCH_KEY:Cache Parity Issue");
#endif
	pr_debug("CRDISPATCH_KEY:Cache Parity Issue");
	if (!buf)
		kfree(buf);
}

#ifdef CONFIG_ARM64
static u64 read_ERXMISC0_EL1(void)
{
	u64 v;

	__asm__ volatile ("mrs %0, s3_0_c5_c5_0" : "=r" (v));

	return v;
}

static u64 read_ERXSTATUS_EL1(void)
{
	u64 v;

	__asm__ volatile ("mrs %0, s3_0_c5_c4_2" : "=r" (v));

	return v;
}

static void write_ERXSTATUS_EL1(u64 v)
{
	__asm__ volatile ("msr s3_0_c5_c4_2, %0" : : "r" (v));
}

static void write_ERXSELR_EL1(u64 v)
{
	__asm__ volatile ("msr s3_0_c5_c3_1, %0" : : "r" (v));
}
#else
static u64 read_ERXMISC0_EL1(void)
{
	return 0;
}

static u64 read_ERXSTATUS_EL1(void)
{
	return 0;
}

static void write_ERXSTATUS_EL1(u64 v)
{
}

static void write_ERXSELR_EL1(u32 v)
{
}
#endif

static irqreturn_t cache_parity_isr_v3(int irq, void *dev_id)
{
	u32 hwirq, serr;
	int i, idx, cpu;
	u64 misc0, status;
	u64 sel = 0x1;
#if IS_ENABLED(CONFIG_ARM64)
	static const struct midr_range cpu_list[] = {
		MIDR_ALL_VERSIONS(MIDR_CORTEX_A510), /* KLEIN */
		MIDR_ALL_VERSIONS(MIDR_CORTEX_A715), /* MAKALU */
		MIDR_ALL_VERSIONS(MIDR_CORTEX_A720), /* HUNTER */
		MIDR_ALL_VERSIONS(MIDR_CORTEX_CHABERTON), /* CHABERTON */
		{},
	};
	/*
	 * Error code: 0x2, 0xC, 0x12 of specific DSU type
	 * need to be bypass. But the type of DSU can't be
	 * directly identified. Fortunately, there is an relationship
	 * between CPU type and DSU type. Thus CPU type can be used
	 * to identify which type of DSU we used.
	 */
	static const struct midr_range bypass_list[] = {
		MIDR_ALL_VERSIONS(MIDR_CORTEX_A510), /* KLEIN */
		MIDR_ALL_VERSIONS(MIDR_CORTEX_A715), /* MAKALU */
		MIDR_ALL_VERSIONS(MIDR_CORTEX_X3), /* MAKALU_ELP */
		MIDR_ALL_VERSIONS(MIDR_CORTEX_A720), /* HUNTER */
		MIDR_ALL_VERSIONS(MIDR_CORTEX_X4), /* HUNTER_ELP */
		MIDR_ALL_VERSIONS(MIDR_CORTEX_CHABERTON), /* CHABERTON */
		MIDR_ALL_VERSIONS(MIDR_CORTEX_BLACKHAWK), /* BLACKHAWK */
		{},
	};
#endif

	ecc_dump_debug_info();

	atomic_inc(&cache_parity.nr_err);

	if (!atomic_read(&cache_parity.nr_err))
		cache_parity.timestampe = local_clock();

	hwirq = irqd_to_hwirq(irq_get_irq_data(irq));

	if (hwirq == cache_parity.arm_dsu_ecc_hwirq)
		sel = 0;
	else {
		for (i = 0; i < cache_parity.nr_complex_irq; i++) {
			if (hwirq == cache_parity.arm_complex_ecc_hwirq[i]) {
				sel = 2;
				break;
			}
		}
	}

	write_ERXSELR_EL1(sel);

	misc0 = read_ERXMISC0_EL1();
	status = read_ERXSTATUS_EL1();
	serr = status & 0xFF;

	/* Clear IRQ via clear error status */
	write_ERXSTATUS_EL1(status);

	/*
	 * If the ERxSTATUS register returns zero, clear all errors.
	 */
	if (!misc0 && !status)
		write_ERXSTATUS_EL1(0xFFF80000);

	/* Ensure all transactions are finished */
	dsb(sy);

	for (idx = -1, i = 0; i < cache_parity.nr_irq; i++) {
		if (cache_parity.record[i].v3.irq == irq) {
			idx = i;
			break;
		}
	}
	if (idx >= 0) {
		cache_parity.record[idx].v3.is_err = true;
		cache_parity.record[idx].v3.misc0_el1 = misc0;
		cache_parity.record[idx].v3.status_el1 = status;

		cpu = raw_smp_processor_id();
		if ((cache_parity.record[idx].v3.cpu != nr_cpu_ids) &&
		    (cpu != cache_parity.record[idx].v3.cpu)) {
			ECC_LOG("Cache ECC error, cpu%d serviced irq%d(%s%d)\n",
				cpu, irq, "expected cpu",
				cache_parity.record[idx].v3.cpu);
			if (!misc0 && !status) {
				ECC_LOG("%s", "bypass due to misc0_el1 and status_el1 invalid\n");
				goto check_nr_err;
			}
		}

#if IS_ENABLED(CONFIG_ARM64)
		/* Skip the error, may be caused by externel slave error
		 * 1. When booker receives SLVERR(0x2) of AXI response,
		 *    it will be transferred to DERR(Data Error) to DSU,
		 *    Core which is in the cpu_list will get ECC Error
		 *    with SERR = 0xC/0x2.
		 * 2. When booker receives SLVERR(0x3) of AXI response,
		 *    it will be transferred to DERR(Non-data Error) to DSU,
		 *    Core which is in the cpu_list will get ECC Error with
		 *    SERR = 0x12.
		 */
		if (hwirq == cache_parity.arm_dsu_ecc_hwirq) {
			if (is_midr_in_range_list(read_cpuid_id(), bypass_list)) {
				if ((serr == 0x2) || (serr == 0xC) || (serr == 0x12)) {
					cpu = raw_smp_processor_id();
					ECC_LOG("Cache ECC error, cpu%d serviced irq%d\n", cpu, irq);
					ECC_LOG("SERR[7:0] = 0x%x, bypass this error!\n", serr);
					goto check_nr_err;
				}
			}
		} else {
			if (is_midr_in_range_list(read_cpuid_id(), cpu_list)) {
				if ((serr == 0x2) || (serr == 0xC) || (serr == 0x12)) {
					cpu = raw_smp_processor_id();
					ECC_LOG("Cache ECC error, cpu%d serviced irq%d\n", cpu, irq);
					ECC_LOG("SERR[7:0] = 0x%x, bypass this error!\n", serr);
					goto check_nr_err;
				}
			}
		}
#endif
		if (!cache_parity_trigger_bug(false, irq, misc0, status))
			schedule_work(&cache_parity.work);
	}

	ECC_LOG("Cache ECC error, %s %d, %s: 0x%016llx, %s: 0x%016llx\n",
	       "irq", irq, "misc0_el1", misc0, "status_el1", status);

check_nr_err:

	if (atomic_read(&cache_parity.nr_err) > ECC_IRQ_TRIGGER_THRESHOLD) {
		disable_irq_nosync(irq);
		ECC_LOG("%s disable irq %d due to trigger over than %d times.",
			__func__, irq, ECC_IRQ_TRIGGER_THRESHOLD);
	}

	return IRQ_HANDLED;
}

static irqreturn_t cache_parity_isr_v2(int irq, void *dev_id)
{
	u32 hwirq;
	int i, idx, cpu;
	u64 misc0, status;
#ifdef CONFIG_ARM64_ERRATUM_1800710
	static const struct midr_range erratum_1800710_cpu_list[] = {
		_MIDR_ALL_VERSIONS(MIDR_CORTEX_A76),
		_MIDR_ALL_VERSIONS(MIDR_CORTEX_A77),
	};
#endif

	ecc_dump_debug_info();

	atomic_inc(&cache_parity.nr_err);

	if (!atomic_read(&cache_parity.nr_err))
		cache_parity.timestampe = local_clock();

	hwirq = irqd_to_hwirq(irq_get_irq_data(irq));
	write_ERXSELR_EL1((hwirq == cache_parity.arm_dsu_ecc_hwirq) ? 1 : 0);

	misc0 = read_ERXMISC0_EL1();
	status = read_ERXSTATUS_EL1();

	/* Clear IRQ via clear error status */
	write_ERXSTATUS_EL1(status);

	/*
	 * If the ERxSTATUS register returns zero, clear all errors.
	 */
	if (!misc0 && !status)
		write_ERXSTATUS_EL1(0xFFC00000);

	/* Ensure all transactions are finished */
	dsb(sy);

	for (idx = -1, i = 0; i < cache_parity.nr_irq; i++) {
		if (cache_parity.record[i].v2.irq == irq) {
			idx = i;
			break;
		}
	}
	if (idx >= 0) {
		cache_parity.record[idx].v2.is_err = true;
		cache_parity.record[idx].v2.misc0_el1 = misc0;
		cache_parity.record[idx].v2.status_el1 = status;

		cpu = raw_smp_processor_id();
		if ((cache_parity.record[idx].v2.cpu != nr_cpu_ids) &&
		    (cpu != cache_parity.record[idx].v2.cpu))
			ECC_LOG("Cache ECC error, cpu%d serviced irq%d(%s%d)\n",
				cpu, irq, "expected cpu",
				cache_parity.record[idx].v2.cpu);

		schedule_work(&cache_parity.work);
	}

	ECC_LOG("Cache ECC error, %s %d, %s: 0x%016llx, %s: 0x%016llx\n",
	       "irq", irq, "misc0_el1", misc0, "status_el1", status);

#ifdef CONFIG_ARM64_ERRATUM_1800710
	if (is_midr_in_range_list(read_cpuid_id(), erratum_1800710_cpu_list)) {
		if ((status & ECC_CE_BIT) == ECC_CE_AT_LEAST_ONE_ERR &&
		    (status & ECC_SERR_BIT) == ECC_SERR_FROM_DATA_BUFF) {
			ECC_LOG("%s %s hit, may cause stale translation\n",
					__func__, "Erratum 1800710");
		}
	}
#endif

	if (atomic_read(&cache_parity.nr_err) > ECC_IRQ_TRIGGER_THRESHOLD) {
		disable_irq_nosync(irq);
		ECC_LOG("%s disable irq %d due to trigger over than %d times.",
			__func__, irq, ECC_IRQ_TRIGGER_THRESHOLD);
	}

	return IRQ_HANDLED;
}

static irqreturn_t cache_parity_isr_v1(int irq, void *dev_id)
{
	struct parity_record_t *parity_record;
	void __iomem *base;
	unsigned int status;
	unsigned int offset;
	unsigned int i;

	if (!atomic_read(&cache_parity.nr_err))
		cache_parity.timestampe = local_clock();

	atomic_inc(&cache_parity.nr_err);

	for (i = 0, parity_record = NULL; i < cache_parity.nr_irq; i++) {
		if (parity_irq_record[i].irq == irq) {
			parity_record = &(parity_irq_record[i].parity_record);
			pr_info("parity isr for %d\n", i);
			break;
		}
	}

	if (parity_record == NULL) {
		pr_info("no matched irq %d\n", irq);
		return IRQ_HANDLED;
	}

	base = cache_parity.cache_parity_base;

	status = readl(base + parity_record->check_offset);
	pr_info("status 0x%x\n", status);

	if (status & parity_record->check_mask)
		pr_info("detect cache parity error\n");
	else
		pr_info("no cache parity error\n");

	for (i = 0; i < parity_record->dump_length; i += 4) {
		offset = parity_record->dump_offset + i;
		pr_info("offset 0x%x, val 0x%x\n", offset,
			readl(base + offset));
	}

	for (i = 0; i < cache_parity.nr_irq; i++) {
		if (cache_parity.record[i].v1.irq != irq)
			continue;
		cache_parity.record[i].v1.is_err = true;
		cache_parity.record[i].v1.status = status;
		schedule_work(&cache_parity.work);
	}

	spin_lock(&parity_isr_lock);

	if (parity_record->clear_mask) {
		writel(parity_record->clear_mask,
			base + parity_record->clear_offset);
		dsb(sy);
		writel(0x0, base + parity_record->clear_offset);
		dsb(sy);

		while (readl(base + parity_record->check_offset) &
			parity_record->check_mask) {
			udelay(1);
		}
	}

	spin_unlock(&parity_isr_lock);

	if (atomic_read(&cache_parity.nr_err) > ECC_IRQ_TRIGGER_THRESHOLD) {
		disable_irq_nosync(irq);
		ECC_LOG("%s disable irq %d due to trigger over than %d times.",
			__func__, irq, ECC_IRQ_TRIGGER_THRESHOLD);
	}

	return IRQ_HANDLED;
}

void __attribute__((weak)) cache_parity_init_platform(void)
{
	pr_info("[%s] adopt default flow\n", __func__);
}

static int __count_cache_parity_irq(struct device_node *dev)
{
	struct of_phandle_args oirq;
	int nr = 0;

	while (of_irq_parse_one(dev, nr, &oirq) == 0)
		nr++;

	return nr;
}

static int __probe_v3(struct platform_device *pdev)
{
	unsigned int i, j;
	int ret;
	int irq, hwirq;
	int cpu = 0, complex_cpu = 0, target_cpu;

	for (i = 0; i < cache_parity.nr_irq; i++) {
		irq = irq_of_parse_and_map(pdev->dev.of_node, i);
		if (irq == 0) {
			dev_err(&pdev->dev,
				"failed to irq_of_parse_and_map %d\n", i);
			return -ENXIO;
		}
		cache_parity.record[i].v3.irq = irq;

		/*
		 * Per-cpu system registers will be read and recorded in the
		 * ISR (Interrupt Service Routine). The ISR must be bound to
		 * the corresponding CPU except the ISR for the ARM DSU ECC
		 * interrupt (which can be served on any CPU).
		 */
		hwirq = irqd_to_hwirq(irq_get_irq_data(irq));
		if (hwirq != cache_parity.arm_dsu_ecc_hwirq) {

			/* Check if it is complex IRQ */
			for (j = 0; j < cache_parity.nr_complex_irq; j++) {
				if (hwirq == cache_parity.arm_complex_ecc_hwirq[j]) {
					target_cpu = complex_cpu;
					/*
					 * For dual-core complex, two cores share
					 * the same VPU, L2 TLB and L2 cache
					 */
					complex_cpu += 2;
					break;
				}
			}

			/* Not match any complex IRQ */
			if (j == cache_parity.nr_complex_irq) {
				target_cpu = cpu;
				cpu++;
			}

			cache_parity.record[i].v3.cpu = target_cpu;
#if defined(MODULE)
			/*
			 * FIXME: Here is an issue caused by GKI.
			 *        We should use irq_force_affinity for
			 *        guaranteeing the per-core ECC interrupt
			 *        is routed to the corresponding CPU.
			 *        This is because the ECC status will be read
			 *        from the per-core system register.
			 *        But the kernel function irq_force_affinity
			 *        is NOT exported.
			 *
			 *        Workaround this problem by using the function
			 *        irq_set_affinity_hint. Need to fix this
			 *        after we upstream a patch (to export
			 *        irq_force_affinity).
			 */
			ret = irq_set_affinity_hint(irq, cpumask_of(target_cpu));
#else
			ret = irq_force_affinity(irq, cpumask_of(target_cpu));
#endif
			if (ret) {
				dev_err(&pdev->dev,
					"failed to set affinity for irq %d\n",
					irq);
				return -ENXIO;
			}
			dev_info(&pdev->dev, "bound irq %d for cpu%d\n",
					irq, target_cpu);
		} else
			cache_parity.record[i].v3.cpu = nr_cpu_ids;

		ret = devm_request_irq(&pdev->dev, irq, cache_parity_isr_v3,
				IRQF_TRIGGER_NONE | IRQF_ONESHOT,
				"cache_parity", NULL);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to request irq for irq %d\n", irq);
			return -ENXIO;
		}
	}

	return 0;
}

static int __probe_v2(struct platform_device *pdev)
{
	unsigned int i;
	int ret;
	int irq, hwirq, cpu;

	for (i = 0, cpu = 0; i < cache_parity.nr_irq; i++) {
		irq = irq_of_parse_and_map(pdev->dev.of_node, i);
		if (irq == 0) {
			dev_err(&pdev->dev,
				"failed to irq_of_parse_and_map %d\n", i);
			return -ENXIO;
		}
		cache_parity.record[i].v2.irq = irq;

		/*
		 * Per-cpu system registers will be read and recorded in the
		 * ISR (Interrupt Service Routine). The ISR must be bound to
		 * the corresponding CPU except the ISR for the ARM DSU ECC
		 * interrupt (which can be served on any CPU).
		 */
		hwirq = irqd_to_hwirq(irq_get_irq_data(irq));
		if (hwirq != cache_parity.arm_dsu_ecc_hwirq) {
			cache_parity.record[i].v2.cpu = cpu;
#if defined(MODULE)
			/*
			 * FIXME: Here is an issue caused by GKI.
			 *        We should use irq_force_affinity for
			 *        guaranteeing the per-core ECC interrupt
			 *        is routed to the corresponding CPU.
			 *        This is because the ECC status will be read
			 *        from the per-core system register.
			 *        But the kernel function irq_force_affinity
			 *        is NOT exported.
			 *
			 *        Workaround this problem by using the function
			 *        irq_set_affinity_hint. Need to fix this
			 *        after we upstream a patch (to export
			 *        irq_force_affinity).
			 */
			ret = irq_set_affinity_hint(irq, cpumask_of(cpu));
#else
			ret = irq_force_affinity(irq, cpumask_of(cpu));
#endif
			if (ret) {
				dev_err(&pdev->dev,
					"failed to set affinity for irq %d\n",
					irq);
				return -ENXIO;
			}
			dev_info(&pdev->dev, "bound irq %d for cpu%d\n",
					irq, cpu);
			cpu++;
		} else
			cache_parity.record[i].v2.cpu = nr_cpu_ids;

		ret = devm_request_irq(&pdev->dev, irq, cache_parity_isr_v2,
				IRQF_TRIGGER_NONE | IRQF_ONESHOT,
				"cache_parity", NULL);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to request irq for irq %d\n", irq);
			return -ENXIO;
		}
	}

	return 0;
}

static int __probe_v1(struct platform_device *pdev)
{
	struct parity_irq_config_t *parity_irq_config;
	size_t size;
	unsigned int i, target_cpu;
	int irq, ret;

	size = sizeof(struct parity_irq_record_t) * cache_parity.nr_irq;
	parity_irq_record = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!parity_irq_record)
		return -ENOMEM;

	size = sizeof(struct parity_irq_config_t) * cache_parity.nr_irq;
	parity_irq_config = devm_kmalloc(&pdev->dev, size, GFP_KERNEL);
	if (!parity_irq_config)
		return -ENOMEM;

	size = size >> 2;
	ret = of_property_read_u32_array(pdev->dev.of_node,
			"irq_config", (u32 *)parity_irq_config, size);
	if (ret) {
		dev_err(&pdev->dev, "No irq_config\n");
		return -ENXIO;
	}

	for (i = 0; i < cache_parity.nr_irq; i++) {
		memcpy(&(parity_irq_record[i].parity_record),
			&(parity_irq_config[i].parity_record),
			sizeof(struct parity_record_t));

		irq = irq_of_parse_and_map(pdev->dev.of_node, i);
		if (irq == 0) {
			dev_err(&pdev->dev,
				"failed to irq_of_parse_and_map %d\n", i);
			return -ENXIO;
		}
		parity_irq_record[i].irq = irq;
		cache_parity.record[i].v1.irq = irq;

		target_cpu = parity_irq_config[i].target_cpu;
		if (target_cpu < nr_cpu_ids) {
			ret = irq_set_affinity_hint(irq,
						cpumask_of(target_cpu));
			if (ret)
				dev_notice(&pdev->dev,
				"failed to set IRQ affinity for cpu%d\n",
				target_cpu);
		}

		ret = devm_request_irq(&pdev->dev, irq, cache_parity_isr_v1,
				IRQF_TRIGGER_NONE, "cache_parity", NULL);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to request irq for irq %d\n", irq);
			return -EINVAL;
		}
	}

	return 0;
}

static int cache_parity_probe(struct platform_device *pdev)
{
	int ret;
	size_t size;
	int i, len;

	dev_info(&pdev->dev, "driver probed\n");

	cache_parity.ver = (int)(uintptr_t)of_device_get_match_data(&pdev->dev);

	atomic_set(&cache_parity.nr_err, 0);

	INIT_WORK(&cache_parity.work, cache_parity_irq_work);

	cache_parity_init_platform();

	cache_parity.nr_irq = __count_cache_parity_irq(pdev->dev.of_node);

	size = sizeof(union err_record) * cache_parity.nr_irq;
	cache_parity.record = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!cache_parity.record)
		return -ENOMEM;

	switch (cache_parity.ver) {
	case 1:
		cache_parity.cache_parity_base = of_iomap(pdev->dev.of_node, 0);
		if (!cache_parity.cache_parity_base)
			return -ENOMEM;

		ret = __probe_v1(pdev);

		break;

	case 2:
		ret = of_property_read_u32(pdev->dev.of_node,
					"arm-dsu-ecc-hwirq",
					&cache_parity.arm_dsu_ecc_hwirq);
		if (ret) {
			dev_err(&pdev->dev, "no arm-dsu-ecc-hwir");
			return -ENXIO;
		}

		ret = __probe_v2(pdev);

		break;

	case 3:
		ret = of_property_read_u32(pdev->dev.of_node,
					"arm-dsu-ecc-hwirq",
					&cache_parity.arm_dsu_ecc_hwirq);
		if (ret) {
			dev_err(&pdev->dev, "no arm-dsu-ecc-hwir");
			return -ENXIO;
		}
		len = of_property_count_elems_of_size(pdev->dev.of_node,
							"arm-complex-ecc-hwirq", 4);
		if (len <= 0) {
			dev_err(&pdev->dev, "no arm-complex-ecc-hwirq");
			cache_parity.nr_complex_irq = 0;
		} else {
			if (len > MAX_COMPLEX_IRQ_NUM) {
				dev_err(&pdev->dev,
					"count of arm-complex-ecc-hwirq (%d) exceeds expect (%d)\n",
					len, MAX_COMPLEX_IRQ_NUM);
				return -ENXIO;
			}

			cache_parity.nr_complex_irq = len;
			for (i = 0; i < len; i++) {
				ret = of_property_read_u32_index(pdev->dev.of_node,
					"arm-complex-ecc-hwirq", i,
					&cache_parity.arm_complex_ecc_hwirq[i]);
				if (ret) {
					dev_err(&pdev->dev, "no arm-complex-ecc-hwirq");
					return -ENXIO;
				}
			}
		}

		ret = __probe_v3(pdev);

		break;

	default:
		dev_err(&pdev->dev, "unsupported version\n");
		ret = -ENXIO;

		break;
	}

	if (!ret)
		dev_info(&pdev->dev, "%s %d, %s %d, %s %d %s %d\n",
			"version", cache_parity.ver,
			"nr_irq", cache_parity.nr_irq,
			"arm-dsu-ecc-hwirq", cache_parity.arm_dsu_ecc_hwirq,
			"nr_err", atomic_read(&cache_parity.nr_err));

	return ret;
}

static int cache_parity_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "driver removed\n");

	flush_work(&cache_parity.work);

	return 0;
}

static const struct of_device_id cache_parity_of_ids[] = {
	{ .compatible = "mediatek,mt6785-cache-parity", .data = (void *)1 },
	{ .compatible = "mediatek,mt6873-cache-parity", .data = (void *)2 },
	{ .compatible = "mediatek,mt6985-cache-parity", .data = (void *)3 },
	{}
};

static struct platform_driver cache_parity_drv = {
	.driver = {
		.name = "cache_parity",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = cache_parity_of_ids,
	},
	.probe = cache_parity_probe,
	.remove = cache_parity_remove,
};

static int __init cache_parity_init(void)
{
	int ret;

	ret = platform_driver_register(&cache_parity_drv);
	if (ret)
		return ret;

	ret = driver_create_file(&cache_parity_drv.driver,
				 &driver_attr_cache_status);
	if (ret)
		return ret;

	return 0;
}

static __exit void cache_parity_exit(void)
{
	driver_remove_file(&cache_parity_drv.driver,
			 &driver_attr_cache_status);

	platform_driver_unregister(&cache_parity_drv);
}

module_init(cache_parity_init);
module_exit(cache_parity_exit);

MODULE_DESCRIPTION("MediaTek Cache Parity Driver");
MODULE_LICENSE("GPL v2");
