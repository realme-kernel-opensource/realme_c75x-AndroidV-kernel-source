// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Ning Li <ning.li@mediatek.com>
 */

/*
 * This driver adds support for perf events to use the Performance
 * Monitor Counter Groups (PMCG) associated with an SMMUv3 node
 * to monitor that node.
 *
 * SMMUv3 PMCG devices are named as smmuv3_pmcg_<phys_addr_page> where
 * <phys_addr_page> is the physical page address of the SMMU PMCG wrapped
 * to 4K boundary. For example, the PMCG at 0xff88840000 is named
 * smmuv3_pmcg_ff88840
 *
 * Filtering by stream id is done by specifying filtering parameters
 * with the event. options are:
 *   filter_enable    - 0 = no filtering, 1 = filtering enabled
 *   filter_span      - 0 = exact match, 1 = pattern match
 *   filter_stream_id - pattern to filter against
 *
 * To match a partial StreamID where the X most-significant bits must match
 * but the Y least-significant bits might differ, STREAMID is programmed
 * with a value that contains:
 *  STREAMID[Y - 1] == 0.
 *  STREAMID[Y - 2:0] == 1 (where Y > 1).
 * The remainder of implemented bits of STREAMID (X bits, from bit Y upwards)
 * contain a value to match from the corresponding bits of event StreamID.
 *
 * Example: perf stat -e smmuv3_pmcg_ff88840/transaction,filter_enable=1,
 *                    filter_span=1,filter_stream_id=0x42/ -a netperf
 * Applies filter pattern 0x42 to transaction events, which means events
 * matching stream ids 0x42 and 0x43 are counted. Further filtering
 * information is available in the SMMU documentation.
 *
 * SMMU events are not attributable to a CPU, so task mode and sampling
 * are not supported.
 */

#include <linux/acpi.h>
#include <linux/acpi_iort.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/cpuhotplug.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/msi.h>
#include <linux/of.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include "mtk-smmu-v3.h"
#include "mtk-iommu-util.h"

#define SMMU_PMCG_EVCNTR0               0x0
#define SMMU_PMCG_EVCNTR(n, stride)     (SMMU_PMCG_EVCNTR0 + (n) * (stride))
#define SMMU_PMCG_EVTYPER0              0x400
#define SMMU_PMCG_EVTYPER(n)            (SMMU_PMCG_EVTYPER0 + (n) * 4)
#define SMMU_PMCG_SID_SPAN_SHIFT        29
#define SMMU_PMCG_SMR0                  0xA00
#define SMMU_PMCG_SMR(n)                (SMMU_PMCG_SMR0 + (n) * 4)
#define SMMU_PMCG_CNTENSET0             0xC00
#define SMMU_PMCG_CNTENCLR0             0xC20
#define SMMU_PMCG_INTENSET0             0xC40
#define SMMU_PMCG_INTENCLR0             0xC60
#define SMMU_PMCG_OVSCLR0               0xC80
#define SMMU_PMCG_OVSSET0               0xCC0
#define SMMU_PMCG_CFGR                  0xE00
#define SMMU_PMCG_CFGR_SID_FILTER_TYPE  BIT(23)
#define SMMU_PMCG_CFGR_MSI              BIT(21)
#define SMMU_PMCG_CFGR_RELOC_CTRS       BIT(20)
#define SMMU_PMCG_CFGR_SIZE             GENMASK(13, 8)
#define SMMU_PMCG_CFGR_NCTR             GENMASK(5, 0)
#define SMMU_PMCG_CR                    0xE04
#define SMMU_PMCG_CR_ENABLE             BIT(0)
#define SMMU_PMCG_IIDR                  0xE08
#define SMMU_PMCG_IIDR_PRODUCTID        GENMASK(31, 20)
#define SMMU_PMCG_IIDR_VARIANT          GENMASK(19, 16)
#define SMMU_PMCG_IIDR_REVISION         GENMASK(15, 12)
#define SMMU_PMCG_IIDR_IMPLEMENTER      GENMASK(11, 0)
#define SMMU_PMCG_CEID0                 0xE20
#define SMMU_PMCG_CEID1                 0xE28
#define SMMU_PMCG_IRQ_CTRL              0xE50
#define SMMU_PMCG_IRQ_CTRL_IRQEN        BIT(0)
#define SMMU_PMCG_IRQ_CFG0              0xE58
#define SMMU_PMCG_IRQ_CFG1              0xE60
#define SMMU_PMCG_IRQ_CFG2              0xE64

/* IMP-DEF ID registers */
#define SMMU_PMCG_PIDR0                 0xFE0
#define SMMU_PMCG_PIDR0_PART_0          GENMASK(7, 0)
#define SMMU_PMCG_PIDR1                 0xFE4
#define SMMU_PMCG_PIDR1_DES_0           GENMASK(7, 4)
#define SMMU_PMCG_PIDR1_PART_1          GENMASK(3, 0)
#define SMMU_PMCG_PIDR2                 0xFE8
#define SMMU_PMCG_PIDR2_REVISION        GENMASK(7, 4)
#define SMMU_PMCG_PIDR2_DES_1           GENMASK(2, 0)
#define SMMU_PMCG_PIDR3                 0xFEC
#define SMMU_PMCG_PIDR3_REVAND          GENMASK(7, 4)
#define SMMU_PMCG_PIDR4                 0xFD0
#define SMMU_PMCG_PIDR4_DES_2           GENMASK(3, 0)

/* MSI config fields */
#define MSI_CFG0_ADDR_MASK              GENMASK_ULL(51, 2)
#define MSI_CFG2_MEMATTR_DEVICE_nGnRE   0x1

#define SMMU_PMCG_DEFAULT_FILTER_SPAN   1
#define SMMU_PMCG_DEFAULT_FILTER_SID    GENMASK(31, 0)

#define SMMU_PMCG_MAX_COUNTERS          64
#define SMMU_PMCG_ARCH_MAX_EVENTS       128

#define SMMU_PMCG_PA_SHIFT              12

#define SMMU_PMCG_EVCNTR_RDONLY         BIT(0)

#define SMMU_TXU_EVENT_START		0x80
#define SMMU_TXU_EVENT_END		0xe7

static const char *PMU_SMMU_PROP_NAME = "mtk,smmu";
static int cpuhp_state_num;

struct smmu_pmu {
	struct hlist_node node;
	struct perf_event *events[SMMU_PMCG_MAX_COUNTERS];
	DECLARE_BITMAP(used_counters, SMMU_PMCG_MAX_COUNTERS);
	DECLARE_BITMAP(supported_events, SMMU_PMCG_ARCH_MAX_EVENTS);
	unsigned int irq;
	unsigned int on_cpu;
	struct pmu pmu;
	unsigned int num_counters;
	struct device *dev;
	void __iomem *reg_base;
	void __iomem *reloc_base;
	u64 counter_mask;
	u32 options;
	u32 iidr;
	bool global_filter;
	spinlock_t pmu_lock;
	bool delay_init_done;
	struct smmuv3_pmu_device *pmu_device;
	struct arm_smmu_device *smmu;
	enum mtk_smmu_type smmu_type;
	bool take_power;
	u32 txu_id;
};

#define to_smmu_pmu(p) (container_of(p, struct smmu_pmu, pmu))

#define SMMU_PMU_EVENT_ATTR_EXTRACTOR(_name, _config, _start, _end)        \
	static inline u32 get_##_name(struct perf_event *event)            \
	{                                                                  \
		return FIELD_GET(GENMASK_ULL(_end, _start),                \
				 event->attr._config);                     \
	}                                                                  \

SMMU_PMU_EVENT_ATTR_EXTRACTOR(event, config, 0, 15);
SMMU_PMU_EVENT_ATTR_EXTRACTOR(filter_stream_id, config1, 0, 31);
SMMU_PMU_EVENT_ATTR_EXTRACTOR(filter_span, config1, 32, 32);
SMMU_PMU_EVENT_ATTR_EXTRACTOR(filter_enable, config1, 33, 33);

static irqreturn_t smmu_pmu_handle_irq_impl(int irq_num, struct device *dev);
static int smmu_pmu_irq_set_up(int irq_num, struct device *dev);
static void smmu_pmu_delay_init(struct smmu_pmu *smmu_pmu);

static const struct smmuv3_pmu_impl smmu_pmu_impl = {
	.pmu_irq_handler = smmu_pmu_handle_irq_impl,
	.irq_set_up = smmu_pmu_irq_set_up,
};

static inline void smmu_pmu_enable(struct pmu *pmu)
{
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(pmu);

	writel(SMMU_PMCG_IRQ_CTRL_IRQEN,
	       smmu_pmu->reg_base + SMMU_PMCG_IRQ_CTRL);
	writel(SMMU_PMCG_CR_ENABLE, smmu_pmu->reg_base + SMMU_PMCG_CR);
}

static inline void smmu_pmu_disable(struct pmu *pmu)
{
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(pmu);

	writel(0, smmu_pmu->reg_base + SMMU_PMCG_CR);
	writel(0, smmu_pmu->reg_base + SMMU_PMCG_IRQ_CTRL);
}

static inline void smmu_pmu_counter_set_value(struct smmu_pmu *smmu_pmu,
					      u32 idx, u64 value)
{
	if (smmu_pmu->counter_mask & BIT(32))
		writeq(value, smmu_pmu->reloc_base + SMMU_PMCG_EVCNTR(idx, 8));
	else
		writel(value, smmu_pmu->reloc_base + SMMU_PMCG_EVCNTR(idx, 4));
}

static inline u64 smmu_pmu_counter_get_value(struct smmu_pmu *smmu_pmu, u32 idx)
{
	u64 value;

	if (smmu_pmu->counter_mask & BIT(32))
		value = readq(smmu_pmu->reloc_base + SMMU_PMCG_EVCNTR(idx, 8));
	else
		value = readl(smmu_pmu->reloc_base + SMMU_PMCG_EVCNTR(idx, 4));

	return value;
}

static inline void smmu_pmu_counter_enable(struct smmu_pmu *smmu_pmu, u32 idx)
{
	writeq(BIT(idx), smmu_pmu->reg_base + SMMU_PMCG_CNTENSET0);
}

static inline void smmu_pmu_counter_disable(struct smmu_pmu *smmu_pmu, u32 idx)
{
	writeq(BIT(idx), smmu_pmu->reg_base + SMMU_PMCG_CNTENCLR0);
}

static inline void smmu_pmu_interrupt_enable(struct smmu_pmu *smmu_pmu, u32 idx)
{
	writeq(BIT(idx), smmu_pmu->reg_base + SMMU_PMCG_INTENSET0);
}

static inline void smmu_pmu_interrupt_disable(struct smmu_pmu *smmu_pmu,
					      u32 idx)
{
	writeq(BIT(idx), smmu_pmu->reg_base + SMMU_PMCG_INTENCLR0);
}

static inline void smmu_pmu_set_evtyper(struct smmu_pmu *smmu_pmu, u32 idx,
					u32 val)
{
	writel(val, smmu_pmu->reg_base + SMMU_PMCG_EVTYPER(idx));
}

static inline void smmu_pmu_set_smr(struct smmu_pmu *smmu_pmu, u32 idx, u32 val)
{
	writel(val, smmu_pmu->reg_base + SMMU_PMCG_SMR(idx));
}

static void smmu_pmu_event_update(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(event->pmu);
	u64 delta, prev, now;
	u32 idx = hwc->idx;

	do {
		prev = local64_read(&hwc->prev_count);
		now = smmu_pmu_counter_get_value(smmu_pmu, idx);
	} while (local64_cmpxchg(&hwc->prev_count, prev, now) != prev);

	/* handle overflow. */
	delta = now - prev;
	delta &= smmu_pmu->counter_mask;

	local64_add(delta, &event->count);
}

static void smmu_pmu_set_period(struct smmu_pmu *smmu_pmu,
				struct hw_perf_event *hwc)
{
	u32 idx = hwc->idx;
	u64 new;

	if (smmu_pmu->options & SMMU_PMCG_EVCNTR_RDONLY) {
		/*
		 * On platforms that require this quirk, if the counter starts
		 * at < half_counter value and wraps, the current logic of
		 * handling the overflow may not work. It is expected that,
		 * those platforms will have full 64 counter bits implemented
		 * so that such a possibility is remote(eg: HiSilicon HIP08).
		 */
		new = smmu_pmu_counter_get_value(smmu_pmu, idx);
	} else {
		/*
		 * We limit the max period to half the max counter value
		 * of the counter size, so that even in the case of extreme
		 * interrupt latency the counter will (hopefully) not wrap
		 * past its initial value.
		 */
		new = smmu_pmu->counter_mask >> 1;
		smmu_pmu_counter_set_value(smmu_pmu, idx, new);
	}

	local64_set(&hwc->prev_count, new);
}

static void smmu_pmu_set_event_filter(struct perf_event *event,
				      int idx, u32 span, u32 sid)
{
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(event->pmu);
	u32 evtyper;

	evtyper = get_event(event) | span << SMMU_PMCG_SID_SPAN_SHIFT;
	smmu_pmu_set_evtyper(smmu_pmu, idx, evtyper);
	smmu_pmu_set_smr(smmu_pmu, idx, sid);
}

static bool smmu_pmu_check_global_filter(struct perf_event *curr,
					 struct perf_event *new)
{
	if (get_filter_enable(new) != get_filter_enable(curr))
		return false;

	if (!get_filter_enable(new))
		return true;

	return get_filter_span(new) == get_filter_span(curr) &&
	       get_filter_stream_id(new) == get_filter_stream_id(curr);
}

static int smmu_pmu_apply_event_filter(struct smmu_pmu *smmu_pmu,
				       struct perf_event *event, int idx)
{
	u32 span, sid;
	unsigned int cur_idx, num_ctrs = smmu_pmu->num_counters;
	bool filter_en = !!get_filter_enable(event);

	span = filter_en ? get_filter_span(event) :
			   SMMU_PMCG_DEFAULT_FILTER_SPAN;
	sid = filter_en ? get_filter_stream_id(event) :
			   SMMU_PMCG_DEFAULT_FILTER_SID;

	cur_idx = find_first_bit(smmu_pmu->used_counters, num_ctrs);
	/*
	 * Per-counter filtering, or scheduling the first globally-filtered
	 * event into an empty PMU so idx == 0 and it works out equivalent.
	 */
	if (!smmu_pmu->global_filter || cur_idx == num_ctrs) {
		smmu_pmu_set_event_filter(event, idx, span, sid);
		return 0;
	}

	/* Otherwise, must match whatever's currently scheduled */
	if (smmu_pmu_check_global_filter(smmu_pmu->events[cur_idx], event)) {
		smmu_pmu_set_evtyper(smmu_pmu, idx, get_event(event));
		return 0;
	}

	return -EAGAIN;
}

static int smmu_pmu_get_event_idx(struct smmu_pmu *smmu_pmu,
				  struct perf_event *event)
{
	int idx, err;
	unsigned int num_ctrs = smmu_pmu->num_counters;
	unsigned long flags;

	spin_lock_irqsave(&smmu_pmu->pmu_lock, flags);
	idx = find_first_zero_bit(smmu_pmu->used_counters, num_ctrs);
	if (idx == num_ctrs) {
		spin_unlock_irqrestore(&smmu_pmu->pmu_lock, flags);
		/* The counters are all in use. */
		return -EAGAIN;
	}
	if (idx == 0) {
		err = mtk_smmu_rpm_get(smmu_pmu->smmu_type);
		if (err) {
			spin_unlock_irqrestore(&smmu_pmu->pmu_lock, flags);
			pr_info_ratelimited("%s, pm get fail, smmu:%d.\n",
					    __func__, smmu_pmu->smmu_type);
			return -EPERM;
		}
		smmu_pmu->take_power = true;
		pr_info("%s, pm get ok, smmu:%d.\n",
			__func__, smmu_pmu->smmu_type);
	}
	spin_unlock_irqrestore(&smmu_pmu->pmu_lock, flags);

	err = smmu_pmu_apply_event_filter(smmu_pmu, event, idx);
	if (err)
		return err;

	set_bit(idx, smmu_pmu->used_counters);

	return idx;
}

static bool smmu_pmu_events_compatible(struct perf_event *curr,
				       struct perf_event *new)
{
	if (new->pmu != curr->pmu)
		return false;

	if (to_smmu_pmu(new->pmu)->global_filter &&
	    !smmu_pmu_check_global_filter(curr, new))
		return false;

	return true;
}

/*
 * Implementation of abstract pmu functionality required by
 * the core perf events code.
 */

static int smmu_pmu_event_init(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(event->pmu);
	struct device *dev = smmu_pmu->dev;
	struct perf_event *sibling;
	int group_num_events = 1;
	unsigned long flags;
	u16 event_id;

	if (event->attr.type != event->pmu->type)
		return -ENOENT;

	if (hwc->sample_period) {
		dev_dbg(dev, "Sampling not supported\n");
		return -EOPNOTSUPP;
	}

	if (event->cpu < 0) {
		dev_dbg(dev, "Per-task mode not supported\n");
		return -EOPNOTSUPP;
	}

	spin_lock_irqsave(&smmu_pmu->pmu_lock, flags);
	if (!smmu_pmu->delay_init_done)
		smmu_pmu_delay_init(smmu_pmu);
	spin_unlock_irqrestore(&smmu_pmu->pmu_lock, flags);

	/* Verify specified event is supported on this PMU */
	event_id = get_event(event);
	if (event_id < SMMU_PMCG_ARCH_MAX_EVENTS &&
	    (!test_bit(event_id, smmu_pmu->supported_events))) {
		dev_dbg(dev, "Invalid event %d for this PMU\n", event_id);
		return -EINVAL;
	}

	/* Don't allow groups with mixed PMUs, except for s/w events */
	if (!is_software_event(event->group_leader)) {
		if (!smmu_pmu_events_compatible(event->group_leader, event))
			return -EINVAL;

		if (++group_num_events > smmu_pmu->num_counters)
			return -EINVAL;
	}

	for_each_sibling_event(sibling, event->group_leader) {
		if (is_software_event(sibling))
			continue;

		if (!smmu_pmu_events_compatible(sibling, event))
			return -EINVAL;

		if (++group_num_events > smmu_pmu->num_counters)
			return -EINVAL;
	}

	hwc->idx = -1;

	/*
	 * Ensure all events are on the same cpu so all events are in the
	 * same cpu context, to avoid races on pmu_enable etc.
	 */
	event->cpu = smmu_pmu->on_cpu;

	return 0;
}

static void smmu_pmu_event_start(struct perf_event *event, int flags)
{
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	hwc->state = 0;

	smmu_pmu_set_period(smmu_pmu, hwc);

	smmu_pmu_counter_enable(smmu_pmu, idx);
}

static void smmu_pmu_event_stop(struct perf_event *event, int flags)
{
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	if (hwc->state & PERF_HES_STOPPED)
		return;

	smmu_pmu_counter_disable(smmu_pmu, idx);
	/* As the counter gets updated on _start, ignore PERF_EF_UPDATE */
	smmu_pmu_event_update(event);
	hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
}

static int smmu_pmu_event_add(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	int idx;
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(event->pmu);

	idx = smmu_pmu_get_event_idx(smmu_pmu, event);
	if (idx < 0)
		return idx;

	hwc->idx = idx;
	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;
	smmu_pmu->events[idx] = event;
	local64_set(&hwc->prev_count, 0);

	smmu_pmu_interrupt_enable(smmu_pmu, idx);

	if (flags & PERF_EF_START)
		smmu_pmu_event_start(event, flags);

	/* Propagate changes to the userspace mapping. */
	perf_event_update_userpage(event);

	return 0;
}

static void smmu_pmu_event_del(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(event->pmu);
	int idx = hwc->idx;
	unsigned long irq_flags;
	unsigned int cur_idx, num_ctrs = smmu_pmu->num_counters;
	int err;

	smmu_pmu_event_stop(event, flags | PERF_EF_UPDATE);
	smmu_pmu_interrupt_disable(smmu_pmu, idx);
	smmu_pmu->events[idx] = NULL;

	spin_lock_irqsave(&smmu_pmu->pmu_lock, irq_flags);
	clear_bit(idx, smmu_pmu->used_counters);
	cur_idx = find_first_bit(smmu_pmu->used_counters, num_ctrs);
	if (cur_idx == num_ctrs && smmu_pmu->take_power) {
		err = mtk_smmu_rpm_put(smmu_pmu->smmu_type);
		smmu_pmu->take_power = false;
		if (err)
			pr_info("%s, pm put fail, smmu:%d.\n",
				__func__, smmu_pmu->smmu_type);
		else
			pr_info("%s, pm put ok, smmu:%d.\n",
				__func__, smmu_pmu->smmu_type);
	}
	spin_unlock_irqrestore(&smmu_pmu->pmu_lock, irq_flags);

	perf_event_update_userpage(event);
}

static void smmu_pmu_event_read(struct perf_event *event)
{
	smmu_pmu_event_update(event);
}

/* cpumask */

static ssize_t smmu_pmu_cpumask_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(dev_get_drvdata(dev));

	return cpumap_print_to_pagebuf(true, buf, cpumask_of(smmu_pmu->on_cpu));
}

static struct device_attribute smmu_pmu_cpumask_attr =
		__ATTR(cpumask, 0444, smmu_pmu_cpumask_show, NULL);

static struct attribute *smmu_pmu_cpumask_attrs[] = {
	&smmu_pmu_cpumask_attr.attr,
	NULL
};

static const struct attribute_group smmu_pmu_cpumask_group = {
	.attrs = smmu_pmu_cpumask_attrs,
};

/* Events */

static ssize_t smmu_pmu_event_show(struct device *dev,
				   struct device_attribute *attr, char *page)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);

	return sysfs_emit(page, "event=0x%02llx\n", pmu_attr->id);
}

#define SMMU_EVENT_ATTR(name, config)			\
	PMU_EVENT_ATTR_ID(name, smmu_pmu_event_show, config)

static struct attribute *smmu_tcu_pmu_events[] = {
	SMMU_EVENT_ATTR(cycles, 0),
	SMMU_EVENT_ATTR(transaction, 1),
	SMMU_EVENT_ATTR(tlb_miss, 2),
	SMMU_EVENT_ATTR(config_cache_miss, 3),
	SMMU_EVENT_ATTR(trans_table_walk_access, 4),
	SMMU_EVENT_ATTR(config_struct_access, 5),
	SMMU_EVENT_ATTR(pcie_ats_trans_rq, 6),
	SMMU_EVENT_ATTR(pcie_ats_trans_passed, 7),

	/* enhanced tcu event */
	SMMU_EVENT_ATTR(s1_l0_walk_cache, 0x80),
	SMMU_EVENT_ATTR(s1_l0_walk_cache_miss, 0x81),
	SMMU_EVENT_ATTR(s1_l1_walk_cache, 0x82),
	SMMU_EVENT_ATTR(s1_l1_walk_cache_miss, 0x83),
	SMMU_EVENT_ATTR(s1_l2_walk_cache, 0x84),
	SMMU_EVENT_ATTR(s1_l2_walk_cache_miss, 0x85),
	SMMU_EVENT_ATTR(s1_l3_walk_cache, 0x86),
	SMMU_EVENT_ATTR(s1_l3_walk_cache_miss, 0x87),
	SMMU_EVENT_ATTR(s2_l0_walk_cache, 0x88),
	SMMU_EVENT_ATTR(s2_l0_walk_cache_miss, 0x89),
	SMMU_EVENT_ATTR(s2_l1_walk_cache, 0x8a),
	SMMU_EVENT_ATTR(s2_l1_walk_cache_miss, 0x8b),
	SMMU_EVENT_ATTR(s2_l2_walk_cache, 0x8c),
	SMMU_EVENT_ATTR(s2_l2_walk_cache_miss, 0x8d),
	SMMU_EVENT_ATTR(s2_l3_walk_cache, 0x8e),
	SMMU_EVENT_ATTR(s2_l3_walk_cache_miss, 0x8f),
	SMMU_EVENT_ATTR(walk_cache_read, 0x90),
	SMMU_EVENT_ATTR(bufferd_translation, 0x91),
	SMMU_EVENT_ATTR(config_cache_lookup, 0x92),
	SMMU_EVENT_ATTR(config_cache_read, 0x93),
	SMMU_EVENT_ATTR(config_cache_looup_miss, 0x94),
	SMMU_EVENT_ATTR(speculative_trans, 0xa0),
	NULL
};

static struct attribute *smmu_tbu_pmu_events[] = {
	SMMU_EVENT_ATTR(cycles, 0),
	SMMU_EVENT_ATTR(transaction, 1),
	SMMU_EVENT_ATTR(tlb_miss, 2),
	SMMU_EVENT_ATTR(pcie_ats_trans_passed, 7),

	/* enhanced tbu event */
	SMMU_EVENT_ATTR(main_tlb_lookup, 0x80),
	SMMU_EVENT_ATTR(main_tlb_miss, 0x81),
	SMMU_EVENT_ATTR(main_tlb_read, 0x82),
	SMMU_EVENT_ATTR(micro_tlb_lookup, 0x83),
	SMMU_EVENT_ATTR(micro_tlb_miss, 0x84),
	SMMU_EVENT_ATTR(slots_full, 0x85),
	SMMU_EVENT_ATTR(out_of_trans_tokens, 0x86),
	SMMU_EVENT_ATTR(write_data_buffer_full, 0x87),
	SMMU_EVENT_ATTR(dcmo_downgrade, 0x8b),
	SMMU_EVENT_ATTR(stash_fail, 0x8c),
	SMMU_EVENT_ATTR(lti_port_slots_full_0, 0xd0),
	SMMU_EVENT_ATTR(lti_port_slots_full_1, 0xd1),
	SMMU_EVENT_ATTR(lti_port_slots_full_2, 0xd2),
	SMMU_EVENT_ATTR(lti_port_slots_full_3, 0xd3),
	SMMU_EVENT_ATTR(lti_port_slots_full_4, 0xd4),
	SMMU_EVENT_ATTR(lti_port_slots_full_5, 0xd5),
	SMMU_EVENT_ATTR(lti_port_slots_full_6, 0xd6),
	SMMU_EVENT_ATTR(lti_port_slots_full_7, 0xd7),
	SMMU_EVENT_ATTR(lti_port_out_of_trans_token_0, 0xe0),
	SMMU_EVENT_ATTR(lti_port_out_of_trans_token_1, 0xe1),
	SMMU_EVENT_ATTR(lti_port_out_of_trans_token_2, 0xe2),
	SMMU_EVENT_ATTR(lti_port_out_of_trans_token_3, 0xe3),
	SMMU_EVENT_ATTR(lti_port_out_of_trans_token_4, 0xe4),
	SMMU_EVENT_ATTR(lti_port_out_of_trans_token_5, 0xe5),
	SMMU_EVENT_ATTR(lti_port_out_of_trans_token_6, 0xe6),
	SMMU_EVENT_ATTR(lti_port_out_of_trans_token_7, 0xe7),
	NULL
};

static umode_t smmu_pmu_event_is_visible(struct kobject *kobj,
					 struct attribute *attr, int unused)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr.attr);

	/* Set all evnets visible, cannot query because smmu is powered off */
	if (pmu_attr->id >= SMMU_TXU_EVENT_END)
		return 0;

	return attr->mode;
}

static const struct attribute_group smmu_tcu_pmu_events_group = {
	.name = "events",
	.attrs = smmu_tcu_pmu_events,
	.is_visible = smmu_pmu_event_is_visible,
};

static const struct attribute_group smmu_tbu_pmu_events_group = {
	.name = "events",
	.attrs = smmu_tbu_pmu_events,
	.is_visible = smmu_pmu_event_is_visible,
};

static ssize_t smmu_pmu_identifier_attr_show(struct device *dev,
					struct device_attribute *attr,
					char *page)
{
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(dev_get_drvdata(dev));

	return sysfs_emit(page, "0x%08x\n", smmu_pmu->iidr);
}

static umode_t smmu_pmu_identifier_attr_visible(struct kobject *kobj,
						struct attribute *attr,
						int n)
{
	struct device *dev = kobj_to_dev(kobj);
	struct smmu_pmu *smmu_pmu = to_smmu_pmu(dev_get_drvdata(dev));

	if (!smmu_pmu->iidr)
		return 0;
	return attr->mode;
}

static struct device_attribute smmu_pmu_identifier_attr =
	__ATTR(identifier, 0444, smmu_pmu_identifier_attr_show, NULL);

static struct attribute *smmu_pmu_identifier_attrs[] = {
	&smmu_pmu_identifier_attr.attr,
	NULL
};

static const struct attribute_group smmu_pmu_identifier_group = {
	.attrs = smmu_pmu_identifier_attrs,
	.is_visible = smmu_pmu_identifier_attr_visible,
};

/* Formats */
PMU_FORMAT_ATTR(event,		   "config:0-15");
PMU_FORMAT_ATTR(filter_stream_id,  "config1:0-31");
PMU_FORMAT_ATTR(filter_span,	   "config1:32");
PMU_FORMAT_ATTR(filter_enable,	   "config1:33");

static struct attribute *smmu_pmu_formats[] = {
	&format_attr_event.attr,
	&format_attr_filter_stream_id.attr,
	&format_attr_filter_span.attr,
	&format_attr_filter_enable.attr,
	NULL
};

static const struct attribute_group smmu_pmu_format_group = {
	.name = "format",
	.attrs = smmu_pmu_formats,
};

static const struct attribute_group *smmu_tcu_pmu_attr_grps[] = {
	&smmu_pmu_cpumask_group,
	&smmu_tcu_pmu_events_group,
	&smmu_pmu_format_group,
	&smmu_pmu_identifier_group,
	NULL
};

static const struct attribute_group *smmu_tbu_pmu_attr_grps[] = {
	&smmu_pmu_cpumask_group,
	&smmu_tbu_pmu_events_group,
	&smmu_pmu_format_group,
	&smmu_pmu_identifier_group,
	NULL
};

/*
 * Generic device handlers
 */
static int smmu_pmu_online_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct smmu_pmu *smmu_pmu;

	smmu_pmu = hlist_entry_safe(node, struct smmu_pmu, node);
	if (!smmu_pmu)
		return -ENODEV;

	if (smmu_pmu->on_cpu == -1) {
		smmu_pmu->on_cpu = cpu;
		irq_set_affinity(smmu_pmu->irq, cpumask_of(cpu));
	}

	return 0;
}

static int smmu_pmu_offline_cpu(unsigned int cpu, struct hlist_node *node)
{
	struct smmu_pmu *smmu_pmu;
	unsigned int target;

	smmu_pmu = hlist_entry_safe(node, struct smmu_pmu, node);
	if (!smmu_pmu)
		return -ENODEV;

	if (cpu != smmu_pmu->on_cpu)
		return 0;

	target = cpumask_any_but(cpu_online_mask, cpu);
	if (target >= nr_cpu_ids)
		return 0;

	perf_pmu_migrate_context(&smmu_pmu->pmu, cpu, target);
	smmu_pmu->on_cpu = target;
	irq_set_affinity(smmu_pmu->irq, cpumask_of(target));

	return 0;
}

static irqreturn_t smmu_pmu_handle_irq(int irq_num, void *data)
{
	struct smmu_pmu *smmu_pmu = data;
	DECLARE_BITMAP(ovs, BITS_PER_TYPE(u64));
	u64 ovsr;
	unsigned int idx;
	unsigned long flags;

	spin_lock_irqsave(&smmu_pmu->pmu_lock, flags);
	if (!smmu_pmu->delay_init_done) {
		spin_unlock_irqrestore(&smmu_pmu->pmu_lock, flags);
		return IRQ_NONE;
	}
	spin_unlock_irqrestore(&smmu_pmu->pmu_lock, flags);
	ovsr = readq(smmu_pmu->reloc_base + SMMU_PMCG_OVSSET0);
	if (!ovsr)
		return IRQ_NONE;

	writeq(ovsr, smmu_pmu->reloc_base + SMMU_PMCG_OVSCLR0);

	bitmap_from_u64(ovs, ovsr);
	for_each_set_bit(idx, ovs, smmu_pmu->num_counters) {
		struct perf_event *event = smmu_pmu->events[idx];
		struct hw_perf_event *hwc;

		if (WARN_ON_ONCE(!event))
			continue;

		smmu_pmu_event_update(event);
		hwc = &event->hw;

		smmu_pmu_set_period(smmu_pmu, hwc);
	}

	return IRQ_HANDLED;
}

static void smmu_pmu_free_msis(void *data)
{
	struct device *dev = data;

	platform_msi_domain_free_irqs(dev);
}

static void smmu_pmu_write_msi_msg(struct msi_desc *desc, struct msi_msg *msg)
{
	phys_addr_t doorbell;
	struct device *dev = msi_desc_to_dev(desc);
	struct smmu_pmu *pmu = dev_get_drvdata(dev);

	doorbell = (((u64)msg->address_hi) << 32) | msg->address_lo;
	doorbell &= MSI_CFG0_ADDR_MASK;

	writeq_relaxed(doorbell, pmu->reg_base + SMMU_PMCG_IRQ_CFG0);
	writel_relaxed(msg->data, pmu->reg_base + SMMU_PMCG_IRQ_CFG1);
	writel_relaxed(MSI_CFG2_MEMATTR_DEVICE_nGnRE,
		       pmu->reg_base + SMMU_PMCG_IRQ_CFG2);
}

static void smmu_pmu_setup_msi(struct smmu_pmu *pmu)
{
	struct device *dev = pmu->dev;
	int ret;

	/* Clear MSI address reg */
	writeq_relaxed(0, pmu->reg_base + SMMU_PMCG_IRQ_CFG0);

	/* MSI supported or not */
	if (!(readl(pmu->reg_base + SMMU_PMCG_CFGR) & SMMU_PMCG_CFGR_MSI))
		return;

	ret = platform_msi_domain_alloc_irqs(dev, 1, smmu_pmu_write_msi_msg);
	if (ret) {
		dev_warn(dev, "failed to allocate MSIs\n");
		return;
	}

	pmu->irq = msi_get_virq(dev, 0);

	/* Add callback to free MSIs on teardown */
	devm_add_action(dev, smmu_pmu_free_msis, dev);
}

static int smmu_pmu_setup_irq(struct smmu_pmu *pmu)
{
	unsigned long flags = IRQF_NOBALANCING | IRQF_SHARED | IRQF_NO_THREAD;
	int irq, ret = -ENXIO;

	smmu_pmu_setup_msi(pmu);

	irq = pmu->irq;
	if (irq)
		ret = devm_request_irq(pmu->dev, irq, smmu_pmu_handle_irq,
				       flags, "smmuv3-pmu", pmu);
	return ret;
}

static void smmu_pmu_reset(struct smmu_pmu *smmu_pmu)
{
	u64 counter_present_mask = GENMASK_ULL(smmu_pmu->num_counters - 1, 0);

	smmu_pmu_disable(&smmu_pmu->pmu);

	/* Disable counter and interrupt */
	writeq_relaxed(counter_present_mask,
		       smmu_pmu->reg_base + SMMU_PMCG_CNTENCLR0);
	writeq_relaxed(counter_present_mask,
		       smmu_pmu->reg_base + SMMU_PMCG_INTENCLR0);
	writeq_relaxed(counter_present_mask,
		       smmu_pmu->reloc_base + SMMU_PMCG_OVSCLR0);
}

static void smmu_pmu_get_acpi_options(struct smmu_pmu *smmu_pmu)
{
	u32 model;

	model = *(u32 *)dev_get_platdata(smmu_pmu->dev);

	switch (model) {
	case IORT_SMMU_V3_PMCG_HISI_HIP08:
		/* HiSilicon Erratum 162001800 */
		smmu_pmu->options |= SMMU_PMCG_EVCNTR_RDONLY;
		break;
	}

	dev_notice(smmu_pmu->dev, "option mask 0x%x\n", smmu_pmu->options);
}

static bool smmu_pmu_coresight_id_regs(struct smmu_pmu *smmu_pmu)
{
	return of_device_is_compatible(smmu_pmu->dev->of_node,
				       "arm,mmu-600-pmcg");
}

static void smmu_pmu_get_iidr(struct smmu_pmu *smmu_pmu)
{
	u32 iidr = readl_relaxed(smmu_pmu->reg_base + SMMU_PMCG_IIDR);

	if (!iidr && smmu_pmu_coresight_id_regs(smmu_pmu)) {
		u32 pidr0 = readl(smmu_pmu->reg_base + SMMU_PMCG_PIDR0);
		u32 pidr1 = readl(smmu_pmu->reg_base + SMMU_PMCG_PIDR1);
		u32 pidr2 = readl(smmu_pmu->reg_base + SMMU_PMCG_PIDR2);
		u32 pidr3 = readl(smmu_pmu->reg_base + SMMU_PMCG_PIDR3);
		u32 pidr4 = readl(smmu_pmu->reg_base + SMMU_PMCG_PIDR4);

		u32 productid = FIELD_GET(SMMU_PMCG_PIDR0_PART_0, pidr0) |
				(FIELD_GET(SMMU_PMCG_PIDR1_PART_1, pidr1) << 8);
		u32 variant = FIELD_GET(SMMU_PMCG_PIDR2_REVISION, pidr2);
		u32 revision = FIELD_GET(SMMU_PMCG_PIDR3_REVAND, pidr3);
		u32 implementer =
			FIELD_GET(SMMU_PMCG_PIDR1_DES_0, pidr1) |
			(FIELD_GET(SMMU_PMCG_PIDR2_DES_1, pidr2) << 4) |
			(FIELD_GET(SMMU_PMCG_PIDR4_DES_2, pidr4) << 8);

		iidr = FIELD_PREP(SMMU_PMCG_IIDR_PRODUCTID, productid) |
		       FIELD_PREP(SMMU_PMCG_IIDR_VARIANT, variant) |
		       FIELD_PREP(SMMU_PMCG_IIDR_REVISION, revision) |
		       FIELD_PREP(SMMU_PMCG_IIDR_IMPLEMENTER, implementer);
	}

	smmu_pmu->iidr = iidr;
}

static void smmu_pmu_delay_init(struct smmu_pmu *smmu_pmu)
{
	u32 cfgr, reg_size;
	u64 ceid_64[2];

	smmu_pmu_get_iidr(smmu_pmu);

	cfgr = readl_relaxed(smmu_pmu->reg_base + SMMU_PMCG_CFGR);

	/* Determine if page 1 is present */
	if (!(cfgr & SMMU_PMCG_CFGR_RELOC_CTRS))
		smmu_pmu->reloc_base = smmu_pmu->reg_base;

	ceid_64[0] = readq_relaxed(smmu_pmu->reg_base + SMMU_PMCG_CEID0);
	ceid_64[1] = readq_relaxed(smmu_pmu->reg_base + SMMU_PMCG_CEID1);
	bitmap_from_arr32(smmu_pmu->supported_events, (u32 *)ceid_64,
			  SMMU_PMCG_ARCH_MAX_EVENTS);

	smmu_pmu->num_counters = FIELD_GET(SMMU_PMCG_CFGR_NCTR, cfgr) + 1;

	smmu_pmu->global_filter = !!(cfgr & SMMU_PMCG_CFGR_SID_FILTER_TYPE);

	reg_size = FIELD_GET(SMMU_PMCG_CFGR_SIZE, cfgr);
	smmu_pmu->counter_mask = GENMASK_ULL(reg_size, 0);

	smmu_pmu_reset(smmu_pmu);
	smmu_pmu->delay_init_done = true;
}

static int smmu_pmu_probe(struct platform_device *pdev)
{
	struct smmuv3_pmu_device *pmu_device;
	struct smmu_pmu *smmu_pmu;
	struct resource *res_0;
	int irq, err;
	char *name;
	struct device *dev = &pdev->dev;
	const struct mtk_smmu_plat_data *plat_data;
	struct platform_device *smmudev;
	struct arm_smmu_device *smmu;
	struct device_node *smmu_node;
	struct mtk_smmu_data *data;
	struct resource *smmu_res_0;

	smmu_pmu = devm_kzalloc(dev, sizeof(*smmu_pmu), GFP_KERNEL);
	if (!smmu_pmu)
		return -ENOMEM;

	smmu_pmu->dev = dev;
	platform_set_drvdata(pdev, smmu_pmu);

	smmu_pmu->pmu = (struct pmu) {
		.module		= THIS_MODULE,
		.task_ctx_nr    = perf_invalid_context,
		.pmu_enable	= smmu_pmu_enable,
		.pmu_disable	= smmu_pmu_disable,
		.event_init	= smmu_pmu_event_init,
		.add		= smmu_pmu_event_add,
		.del		= smmu_pmu_event_del,
		.start		= smmu_pmu_event_start,
		.stop		= smmu_pmu_event_stop,
		.read		= smmu_pmu_event_read,
		.attr_groups	= smmu_tcu_pmu_attr_grps,
		.capabilities	= PERF_PMU_CAP_NO_EXCLUDE,
	};

	smmu_node = of_parse_phandle(dev->of_node, PMU_SMMU_PROP_NAME, 0);
	if (!smmu_node) {
		dev_err(dev, "Can't find smmu node\n");
		return -EINVAL;
	}

	smmudev = of_find_device_by_node(smmu_node);
	if (!smmudev) {
		dev_err(dev, "Can't find smmu dev\n");
		of_node_put(smmu_node);
		return -EINVAL;
	}

	smmu = platform_get_drvdata(smmudev);
	if (!smmu) {
		dev_err(dev, "Can't find smmu data\n");
		of_node_put(smmu_node);
		return -EINVAL;
	}

	data = to_mtk_smmu_data(smmu);
	plat_data = data->plat_data;
	smmu_pmu->smmu = smmu;
	smmu_pmu->smmu_type = plat_data->smmu_type;
	smmu_pmu->take_power = false;

	smmu_pmu->reg_base = devm_platform_get_and_ioremap_resource(pdev, 0, &res_0);
	if (IS_ERR(smmu_pmu->reg_base))
		return PTR_ERR(smmu_pmu->reg_base);
	smmu_pmu->reloc_base = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(smmu_pmu->reloc_base)) {
		dev_info(smmu_pmu->dev,
			 "reloc_base error for %s",
			 smmu_pmu->pmu.name);
		smmu_pmu->reloc_base = smmu_pmu->reg_base;
	}

	/* Update pmu event attr group */
	smmu_res_0 = platform_get_resource(smmudev, IORESOURCE_MEM, 0);
	if (res_0 && smmu_res_0 && (res_0->start - smmu_res_0->start > 0x40000)) {
		smmu_pmu->pmu.attr_groups = smmu_tbu_pmu_attr_grps;
		/* get tbu id */
		smmu_pmu->txu_id =
			(res_0->start - smmu_res_0->start - 0x40000) / 0x20000 + 1;
	} else {
		smmu_pmu->txu_id = 0;
	}

	spin_lock_init(&smmu_pmu->pmu_lock);
	irq = platform_get_irq_optional(pdev, 0);
	if (irq > 0)
		smmu_pmu->irq = irq;
	else {
		// for smmu with combined irq, register pmu to smmu and get
		// irq callback
		pmu_device = devm_kzalloc(dev, sizeof(*pmu_device), GFP_KERNEL);
		if (!pmu_device)
			return -ENOMEM;

		pmu_device->dev = dev;
		pmu_device->impl = &smmu_pmu_impl;
		smmu_pmu->pmu_device = pmu_device;

		mtk_smmu_register_pmu_device(pmu_device);
	}

	if (irq > 0) {
		err = smmu_pmu_setup_irq(smmu_pmu);
		if (err) {
			dev_err(dev, "Setup irq failed, PMU @%pa\n", &res_0->start);
			return err;
		}
	}

	name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "smmuv3_pmcg_%llx",
			      (res_0->start) >> SMMU_PMCG_PA_SHIFT);
	if (!name) {
		dev_err(dev, "Create name failed, PMU @%pa\n", &res_0->start);
		return -EINVAL;
	}

	if (!dev->of_node)
		smmu_pmu_get_acpi_options(smmu_pmu);

	/* Pick one CPU to be the preferred one to use */
	if (smmu_pmu->irq > 0) {
		smmu_pmu->on_cpu = raw_smp_processor_id();
		WARN_ON(irq_set_affinity(smmu_pmu->irq, cpumask_of(smmu_pmu->on_cpu)));
	}

	err = cpuhp_state_add_instance_nocalls(cpuhp_state_num,
					       &smmu_pmu->node);
	if (err) {
		dev_err(dev, "Error %d registering hotplug, PMU @%pa\n",
			err, &res_0->start);
		return err;
	}

	err = perf_pmu_register(&smmu_pmu->pmu, name, -1);
	if (err) {
		dev_err(dev, "Error %d registering PMU @%pa\n",
			err, &res_0->start);
		goto out_unregister;
	}

	dev_info(dev, "Registered PMU @ %pa using %d counters with %s filter settings\n",
		 &res_0->start, smmu_pmu->num_counters,
		 smmu_pmu->global_filter ? "Global(Counter0)" :
		 "Individual");

	return 0;

out_unregister:
	cpuhp_state_remove_instance_nocalls(cpuhp_state_num, &smmu_pmu->node);
	return err;
}

static int smmu_pmu_remove(struct platform_device *pdev)
{
	struct smmu_pmu *smmu_pmu = platform_get_drvdata(pdev);

	if (smmu_pmu->pmu_device)
		mtk_smmu_unregister_pmu_device(smmu_pmu->pmu_device);

	perf_pmu_unregister(&smmu_pmu->pmu);
	cpuhp_state_remove_instance_nocalls(cpuhp_state_num, &smmu_pmu->node);

	return 0;
}

static irqreturn_t smmu_pmu_handle_irq_impl(int irq_num, struct device *dev)
{
	return smmu_pmu_handle_irq(irq_num, dev_get_drvdata(dev));
}

static int smmu_pmu_irq_set_up(int irq_num, struct device *dev)
{
	struct smmu_pmu *smmu_pmu = dev_get_drvdata(dev);

	smmu_pmu->irq = irq_num;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id smmu_pmu_of_match[] = {
	{ .compatible = "arm,smmu-v3-pmcg" },
	{}
};
MODULE_DEVICE_TABLE(of, smmu_pmu_of_match);
#endif

static struct platform_driver smmu_pmu_driver = {
	.driver = {
		.name = "arm-smmu-v3-pmcg",
		.of_match_table = of_match_ptr(smmu_pmu_of_match),
		.suppress_bind_attrs = true,
	},
	.probe = smmu_pmu_probe,
	.remove = smmu_pmu_remove,
};

static int __init arm_smmu_pmu_init(void)
{
	cpuhp_state_num = cpuhp_setup_state_multi(CPUHP_AP_ONLINE_DYN,
						  "perf/arm/pmcg:online",
						  smmu_pmu_online_cpu,
						  smmu_pmu_offline_cpu);
	if (cpuhp_state_num < 0)
		return cpuhp_state_num;

	return platform_driver_register(&smmu_pmu_driver);
}
module_init(arm_smmu_pmu_init);

static void __exit arm_smmu_pmu_exit(void)
{
	platform_driver_unregister(&smmu_pmu_driver);
	cpuhp_remove_multi_state(cpuhp_state_num);
}

module_exit(arm_smmu_pmu_exit);

MODULE_DESCRIPTION("PMU driver for MTK SMMUv3 Performance Monitors Extension");
MODULE_LICENSE("GPL");
