// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <asm/cacheflush.h>
#include <asm/kexec.h>
#include <asm/memory.h>
#include <asm/stacktrace.h>
#include <linux/arm-smccc.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
#include <linux/hrtimer.h>
#endif
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#include <linux/sched/debug.h>
#include <linux/sched/signal.h>
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/sysrq.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <uapi/linux/sched/types.h>
#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
#include "../../../../../kernel/time/tick-internal.h"
#include <linux/of_irq.h>
#endif
#include <mt-plat/mboot_params.h>
#include <mt-plat/mrdump.h>
#include "mrdump_helper.h"
#include "mrdump_private.h"
#include <linux/smp.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/math64.h>
#if !IS_ENABLED(CONFIG_ARM64)
#include <asm/arch_timer.h>
#else
#include <linux/clockchips.h>
#endif
#include "hangdet.h"

void sysrq_sched_debug_show_at_AEE(void);

#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
extern void mt_irq_dump_status(unsigned int irq);
#endif

/*************************************************************************
 * Feature configure region
 *************************************************************************/
#if !IS_ENABLED(CONFIG_ARM64)
#define WK_MAX_MSG_SIZE (450)
#else
#define WK_MAX_MSG_SIZE (1024)
#endif
#define MAX_CPUNR 16
#define SOFT_KICK_RANGE     (100741) // 100741us, use magic num to check wdtk in which hrtimer

#define WDT_MODE		0x0
#define WDT_MODE_EN		0x1
#define WDT_STATUS		0xc
#define WDT_STATUS_IRQ          (1 << 29)
#define WDT_LENGTH_TIMEOUT(n)   ((n) << 5)
#define WDT_LENGTH      0x04
#define WDT_LENGTH_KEY      0x8
#define WDT_RST         0x08
#define WDT_RST_RELOAD      0x1971
#define WDT_NONRST_REG2     0x24
#define WDT_STAGE_OFS       29
#define WDT_STAGE_MASK      0x07
#define WDT_STAGE_KERNEL    0x03
#define CPU_NR (nr_cpu_ids)
#define DEFAULT_INTERVAL    15
#define WDT_COUNTER     0x514

#define SYST_CNTCR		0x0
#define SYST0_CON		0x40
#define SYST0_VAL		0x44
#define SYST_DISTL		0x130

#define SYSTIMER_CNTCV_L	(0x8)
#define SYSTIMER_CNTCV_H	(0xC)

/* bit 11 use set the reboot flag */
#define REBOOT_FLAG_MASK 0x1
#define REBOOT_FLAG_OFS 11

/* Delay to change RGU timeout in ms */
#define CHG_TMO_DLY_SEC		8L
#define CHG_TMO_EN		0

#define RGU_GET_S2IDLE	0x12

#if IS_ENABLED(CONFIG_SMP)
static char tmr_buf[MAX_CPUNR][WK_MAX_MSG_SIZE];
#endif

static int start_kicker(void);
static int kwdt_thread(void *arg);
static int g_kicker_init;
static DEFINE_SPINLOCK(lock);
struct task_struct *wk_tsk[MAX_CPUNR] = { 0 };	/* max cpu 16 */
static unsigned int wk_tsk_bind[MAX_CPUNR] = { 0 };	/* max cpu 16 */
static unsigned long long wk_tsk_bind_time[MAX_CPUNR] = { 0 };	/* max cpu 16 */
static unsigned long long wk_tsk_kick_time[MAX_CPUNR] = { 0 };	/* max cpu 16 */
static char wk_tsk_buf[128] = { 0 };
static unsigned long kick_bit;
static int g_kinterval = -1;
static struct work_struct wdk_work;
static struct workqueue_struct *wdk_workqueue;
static unsigned int lasthpg_act;
static unsigned int lasthpg_cpu;
static unsigned long long lasthpg_t;
static unsigned long long wk_lasthpg_t[MAX_CPUNR] = { 0 };	/* max cpu 16 */
static unsigned int cpuid_t[MAX_CPUNR] = { 0 };	/* max cpu 16 */
static unsigned long long lastsuspend_t;
static unsigned long long lastresume_t;
static unsigned long long lastsuspend_syst;
static unsigned long long lastresume_syst;
static struct notifier_block wdt_pm_nb;
static unsigned long g_nxtKickTime;
static int g_hang_detected;
static int g_change_tmo;
static void __iomem *toprgu_base;
static void __iomem *systimer_base;
#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
static unsigned int systimer_irq;
#endif
static unsigned int cpus_kick_bit;
static atomic_t plug_mask = ATOMIC_INIT(0x0);

static struct pt_regs saved_regs;
struct timer_list aee_dump_timer;
static unsigned long long aee_dump_timer_t;
static unsigned long long all_k_timer_t;
static unsigned int aee_dump_timer_c;
static unsigned int cpus_skip_bit;
static uint32_t apwdt_en;
static bool is_s2idle_status;

#if IS_ENABLED(CONFIG_ARM64)
static DEFINE_SPINLOCK(kp_lock);
struct slp_history wk_sl[MAX_CPUNR];
struct clock_event_device *bc_mtk_clkevt;

static int hrtimer_start_range_ns_pre(struct kprobe *p, struct pt_regs *regs);
static int clockevents_exchange_device_pre(struct kprobe *p, struct pt_regs *regs);

static struct kprobe kp_hrtimer_start_range_ns = {
	.symbol_name =  "hrtimer_start_range_ns",
	.pre_handler = hrtimer_start_range_ns_pre,
};

static struct kprobe kp_clockevents_exchange_device = {
	.symbol_name =  "clockevents_exchange_device",
	.pre_handler = clockevents_exchange_device_pre,
};

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define USLEEP_RANGE_HIS_ARRAY_SIZE (50)
#define BURST_HRTIMER_HIS_ARRAY_SIZE (50)
#define HRTIMER_COUNT_ARRAY_SIZE (100)

static struct arch_timer_caller_history_struct
	usleep_range_history[USLEEP_RANGE_HIS_ARRAY_SIZE];
static struct arch_timer_caller_history_struct
	burst_hrtimer_history[BURST_HRTIMER_HIS_ARRAY_SIZE];
static DEFINE_PER_CPU(struct hrtimer_count_struct[HRTIMER_COUNT_ARRAY_SIZE], hrtimer_func_counts);
static DEFINE_PER_CPU(uint64_t, hrtimer_func_counts_last_time);
static uint64_t usleep_range_count;
static uint64_t hrtimer_count;

static DEFINE_SPINLOCK(usleep_range_lock);
static DEFINE_SPINLOCK(hrtimer_lock);

static int aee_kernel_warning_api_func_pre(struct kprobe *p, struct pt_regs *regs);
static int usleep_range_state_pre(struct kprobe *p, struct pt_regs *regs);
static int hrtimer_expire_entry_pre(struct kprobe *p, struct pt_regs *regs);

static struct kprobe kp_aee_kernel_warning_api_func = {
	.symbol_name = "aee_kernel_warning_api_func",
	.pre_handler = aee_kernel_warning_api_func_pre,
};

static struct kprobe kp_usleep_range_state = {
	.symbol_name = "usleep_range_state",
	.pre_handler = usleep_range_state_pre,
};

static struct kprobe kp_hrtimer_expire_entry = {
	.symbol_name = "__run_hrtimer",
	.pre_handler = hrtimer_expire_entry_pre,
};

static void dump_usleep_range_history(void)
{
	int i;
	unsigned long flags = 0;

	spin_lock_irqsave(&usleep_range_lock, flags);
	for (i = 0; i < USLEEP_RANGE_HIS_ARRAY_SIZE; i++)
		pr_info("usleep_range_history[%d].caller %lx %ps, call time: %lld, comm: %s, caller cpu: %d",
			i, usleep_range_history[i].timer_caller_ip, (void *)usleep_range_history[i].timer_caller_ip,
			usleep_range_history[i].last_time,
			usleep_range_history[i].comm, usleep_range_history[i].caller_cpu);
	spin_unlock_irqrestore(&usleep_range_lock, flags);
}

static void dump_hrtimer_burst_history(void)
{
	int i, cpu;
	unsigned long flags = 0;
	struct hrtimer_count_struct *cpu_counts;

	spin_lock_irqsave(&hrtimer_lock, flags);
	for (i = 0; i < BURST_HRTIMER_HIS_ARRAY_SIZE; i++) {
		if (burst_hrtimer_history[i].count > 0)
			pr_info("burst_hrtimer_history[%d].caller %lx %ps, start_time: %lld, last_time: %lld, now: %lld, comm: %s, count: %lld caller cpu: %d",
				i, burst_hrtimer_history[i].timer_caller_ip,
				(void *)burst_hrtimer_history[i].timer_caller_ip,
				burst_hrtimer_history[i].start_time,
				burst_hrtimer_history[i].last_time,
				burst_hrtimer_history[i].now,
				burst_hrtimer_history[i].comm,
				burst_hrtimer_history[i].count,
				burst_hrtimer_history[i].caller_cpu);
	}

	for_each_online_cpu(cpu) {
		cpu_counts = per_cpu_ptr(hrtimer_func_counts, cpu);
		for (i = 0; i < HRTIMER_COUNT_ARRAY_SIZE; i++)
			if (cpu_counts[i].timer_caller_ip && cpu_counts[i].count >= 200)
				pr_info("cpu_counts[%d].caller %lx %ps, count: %llu, start_time: %lld, last_time: %lld cpu=%d",
					i, cpu_counts[i].timer_caller_ip, (void *)cpu_counts[i].timer_caller_ip,
					cpu_counts[i].count,
					cpu_counts[i].start_time,
					cpu_counts[i].last_time,
					cpu);
	}
	spin_unlock_irqrestore(&hrtimer_lock, flags);
}

#endif
#endif

__weak void mt_irq_dump_status(unsigned int irq)
{
	pr_info("empty gic dump\n");
};

static uint32_t get_s2idle_state(void)
{
	if (is_s2idle_status) {
		struct arm_smccc_res res;
#if IS_ENABLED(CONFIG_ARM64)
		arm_smccc_smc(MTK_SIP_KERNEL_RGU_CONTROL,
				RGU_GET_S2IDLE,
				0, 0, 0,
				0, 0, 0,
				&res);
#endif
#if IS_ENABLED(CONFIG_ARM_PSCI)
		arm_smccc_smc(MTK_SIP_KERNEL_RGU_CONTROL,
				RGU_GET_S2IDLE,
				0, 0, 0,
				0, 0, 0,
				&res);
#endif
		return (uint32_t) res.a1;
	} else
		return 0xff;
};

static unsigned int get_check_bit(void)
{
	return cpus_kick_bit;
}

static unsigned int get_kick_bit(void)
{
	return kick_bit;
}

static int start_kicker_thread_with_default_setting(void)
{
	g_kinterval = DEFAULT_INTERVAL;
	start_kicker();

	pr_debug("[wdk] %s done\n", __func__);
	return 0;
}

void wk_start_kick_cpu(int cpu)
{
	if (IS_ERR(wk_tsk[cpu])) {
		pr_debug("[wdk] wk_task[%d] is NULL\n", cpu);
	} else {
		kthread_bind(wk_tsk[cpu], cpu);
		pr_info("[wdk] bind thread %d to cpu %d\n",
			wk_tsk[cpu]->pid, cpu);
		wake_up_process(wk_tsk[cpu]);
	}
}

#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
static char tick_broadcast_mtk_aee_dump_buf[128];

void tick_broadcast_mtk_aee_dump(void)
{
	int i, ret = -1;

	pr_info("[name:bc&]%s\n", bc_dump_buf.buf);

	/* tick_broadcast_oneshot_mask */

	memset(tick_broadcast_mtk_aee_dump_buf, 0,
		sizeof(tick_broadcast_mtk_aee_dump_buf));
	ret = snprintf(tick_broadcast_mtk_aee_dump_buf,
		sizeof(tick_broadcast_mtk_aee_dump_buf),
		"[TICK] oneshot_mask: %*pbl\n",
		cpumask_pr_args(bc_tick_get_broadcast_oneshot_mask()));
	if (ret >= 0)
		aee_sram_fiq_log(tick_broadcast_mtk_aee_dump_buf);

	/* tick_broadcast_pending_mask */

	memset(tick_broadcast_mtk_aee_dump_buf, 0,
		sizeof(tick_broadcast_mtk_aee_dump_buf));
	ret = snprintf(tick_broadcast_mtk_aee_dump_buf,
		sizeof(tick_broadcast_mtk_aee_dump_buf),
		"[TICK] pending_mask: %*pbl\n",
		cpumask_pr_args(bc_tick_get_broadcast_pending_mask()));
	if (ret >= 0)
		aee_sram_fiq_log(tick_broadcast_mtk_aee_dump_buf);

	/* tick_broadcast_force_mask */

	memset(tick_broadcast_mtk_aee_dump_buf, 0,
		sizeof(tick_broadcast_mtk_aee_dump_buf));
	ret = snprintf(tick_broadcast_mtk_aee_dump_buf,
		sizeof(tick_broadcast_mtk_aee_dump_buf),
		"[TICK] force_mask: %*pbl\n",
		cpumask_pr_args(bc_tick_get_broadcast_force_mask()));
	if (ret >= 0)
		aee_sram_fiq_log(tick_broadcast_mtk_aee_dump_buf);

	memset(tick_broadcast_mtk_aee_dump_buf, 0,
		sizeof(tick_broadcast_mtk_aee_dump_buf));
	ret = snprintf(tick_broadcast_mtk_aee_dump_buf,
		sizeof(tick_broadcast_mtk_aee_dump_buf),
		"[TICK] affin_e cpu: %d affin_h cpu: %d last_handle %lld\n",
		tick_broadcast_history[0].affin_enter_cpu,
		tick_broadcast_history[0].affin_handle_cpu,
		tick_broadcast_history[0].handle_time);
	if (ret >= 0)
		aee_sram_fiq_log(tick_broadcast_mtk_aee_dump_buf);

	for_each_possible_cpu(i) {
		/* to avoid unexpected overrun */
		if (i >= num_possible_cpus())
			break;
		memset(tick_broadcast_mtk_aee_dump_buf, 0,
			sizeof(tick_broadcast_mtk_aee_dump_buf));
		ret = snprintf(tick_broadcast_mtk_aee_dump_buf,
			sizeof(tick_broadcast_mtk_aee_dump_buf),
			"[TICK] cpu %d, %llu, %d, %llu\n",
			i, tick_broadcast_history[i].time_enter,
			tick_broadcast_history[i].ret_enter,
			tick_broadcast_history[i].time_exit);
		if (ret >= 0)
			aee_sram_fiq_log(tick_broadcast_mtk_aee_dump_buf);
	}
}
#endif

void dump_wdk_bind_info(bool to_aee_sram)
{
	int i = 0, ret = -1;

	ret = snprintf(wk_tsk_buf, sizeof(wk_tsk_buf),
		"kick=0x%x,check=0x%x\n",
		get_kick_bit(), get_check_bit());

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_rr_rec_kick(('D' << 24) | get_kick_bit());
	aee_rr_rec_check(('B' << 24) | get_check_bit());
#endif

	if (ret >= 0)
		pr_info("%s", wk_tsk_buf);
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	if (to_aee_sram) {
		aee_sram_fiq_log("\n");
		if (ret >= 0)
			aee_sram_fiq_log(wk_tsk_buf);
	}
#endif
	for (i = 0; i < CPU_NR; i++) {
		if (wk_tsk[i] != NULL) {
			memset(wk_tsk_buf, 0, sizeof(wk_tsk_buf));
			ret = snprintf(wk_tsk_buf, sizeof(wk_tsk_buf),
				"[wdk]CPU %d, %d, %lld, %d, %u, %lld\n",
				i, wk_tsk_bind[i], wk_tsk_bind_time[i],
				wk_tsk[i]->on_rq, wk_tsk[i]->__state,
				wk_tsk_kick_time[i]);
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
			if (to_aee_sram && (ret >= 0))
				aee_sram_fiq_log(wk_tsk_buf);
#endif
			if (!to_aee_sram && (ret >= 0))
				pr_info("%s", wk_tsk_buf);
		}
	}
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	if (to_aee_sram)
		aee_sram_fiq_log("\n");
#endif
}

void kicker_cpu_bind(int cpu)
{
	if (IS_ERR(wk_tsk[cpu]))
		pr_debug("[wdk]wk_task[%d] is NULL\n", cpu);
	else {
		/* kthread_bind(wk_tsk[cpu], cpu); */
		if (!wk_tsk[cpu]) {
			cpuid_t[cpu] = cpu;
			wk_tsk[cpu] = kthread_create(kwdt_thread,
				(void *) &cpuid_t[cpu], "wdtk-%d", cpu);
			if (IS_ERR(wk_tsk[cpu])) {
				// int ret = PTR_ERR(wk_tsk[cpu]);
				wk_tsk[cpu] = NULL;
				pr_info("[wdk]kthread_create failed, wdtk-%d\n", cpu);
				return;
			}

			wk_start_kick_cpu(cpu);
		}
		WARN_ON_ONCE(set_cpus_allowed_ptr(wk_tsk[cpu],
			cpumask_of(cpu)) < 0);
		wake_up_process(wk_tsk[cpu]);
		wk_tsk_bind[cpu] = 1;
		wk_tsk_bind_time[cpu] = sched_clock();
	}
}

void wk_cpu_update_bit_flag(int cpu, int plug_status, int set_check)
{
	if (plug_status == 1) {	/* plug on */
		spin_lock_bh(&lock);
		if (set_check)
			cpus_kick_bit |= (1 << cpu);
		lasthpg_cpu = cpu;
		lasthpg_act = plug_status;
		lasthpg_t = sched_clock();
		spin_unlock_bh(&lock);
	}
	if (plug_status == 0) {	/* plug off */
		spin_lock_bh(&lock);
		cpus_kick_bit &= (~(1 << cpu));
		lasthpg_cpu = cpu;
		lasthpg_act = plug_status;
		lasthpg_t = sched_clock();
		wk_tsk_bind[cpu] = 0;
		spin_unlock_bh(&lock);
	}
}

static void (*p_mt_aee_dump_irq_info)(void);
void kwdt_regist_irq_info(void (*fn)(void))
{
	p_mt_aee_dump_irq_info = fn;
}
EXPORT_SYMBOL_GPL(kwdt_regist_irq_info);

static void kwdt_time_sync(void)
{
	struct rtc_time tm;
	struct timespec64 tv = { 0 };
	/* android time */
	struct rtc_time tm_android;
	struct timespec64 tv_android = { 0 };

	ktime_get_real_ts64(&tv);
	tv_android = tv;
	rtc_time64_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= (uint64_t)sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
#if !IS_ENABLED(CONFIG_MTK_AEE_HANGDET_IMPROVE_PERFORMANCE)
	pr_info("[thread:%d] %d-%02d-%02d %02d:%02d:%02d.%u UTC;"
#else
	pr_debug("[thread:%d] %d-%02d-%02d %02d:%02d:%02d.%u UTC;"
#endif
		"android time %d-%02d-%02d %02d:%02d:%02d.%03d\n",
		current->pid, tm.tm_year + 1900, tm.tm_mon + 1,
		tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
		(unsigned int)(tv.tv_nsec / 1000), tm_android.tm_year + 1900,
		tm_android.tm_mon + 1, tm_android.tm_mday, tm_android.tm_hour,
		tm_android.tm_min, tm_android.tm_sec,
		(unsigned int)(tv_android.tv_nsec / 1000));
}

#if IS_ENABLED(CONFIG_ARM64)
static const int irq_to_ipi_type(int irq)
{
	struct irq_desc **ipi_desc = ipi_desc_get();
	struct irq_desc *desc = irq_to_desc(irq);
	int nr_ipi = nr_ipi_get();
	int i = 0;

	if (!ipi_desc || !desc)
		return -1;

	for (i = 0; i < nr_ipi; i++)
		if (ipi_desc[i] == desc)
			return i;
	return -1;
}
#else
static const int irq_to_ipi_type(int irq)
{
	int temp_irq = 0;
	int num = 0;
	struct irq_desc *desc = irq_to_desc(irq);
	struct irq_desc *temp_desc;

	for_each_irq_desc(temp_irq, temp_desc) {
		if (temp_desc->action && temp_desc->action->name) {
			if (!strcmp(temp_desc->action->name, "IPI")) {
				if (temp_desc == desc)
					return num;
				else
					num++;
			}
		}
	}
	return -1;
}
#endif

#define MAX_HWT_IRQ_FILE_SIZE SZ_128K
#define MAX_HWT_IRQ_BUF_SIZE (MAX_HWT_IRQ_FILE_SIZE / 2)
#define CHK_HWT_IRQ 1
static char *hwt_irq_info;
static char *irq_buf_a;
static char *irq_buf_b;
#if CHK_HWT_IRQ
static char *cur_buf;
static int write_irq_buf_index;

static void log_hwt_irq_info(char *addr, const char *fmt, ...)
{
	unsigned long len;
	va_list ap;

	if ((write_irq_buf_index + SZ_256) >=
			(unsigned long)MAX_HWT_IRQ_BUF_SIZE)
		return;

	va_start(ap, fmt);
	len = vscnprintf(&addr[write_irq_buf_index], SZ_256, fmt, ap);
	va_end(ap);
	write_irq_buf_index += len;
}

static void show_irq_info(char *addr)
{
#define MAX_IRQ_NUM 1024
#define MAX_CPU_NUM 8
	int index;
	unsigned long len;
	struct irq_desc *desc;
	char msg[SZ_256];
	int irq;
	int j;
	unsigned long long record_irq_time;

	memset(msg, 0, SZ_256);

	/*record first row： timestamp*/
	record_irq_time = sched_clock();
	scnprintf(msg, SZ_256, "\ntime: %llu\n", record_irq_time);
	log_hwt_irq_info(addr, "%s", msg);

	/*record second row： CPU-num*/
	index = 0;
	memset(msg, 0, SZ_256);
	len = scnprintf(&msg[index], SZ_256, "%5s", "");
	index += len;
	for (j = 0; j < min_t(int, nr_cpu_ids, MAX_CPU_NUM); j++) {
		len = scnprintf(&msg[index], SZ_256-index, "CPU%-7d ", j);
		index += len;
		/*reserved '\0' memory*/
		if (index >= SZ_256 - 2) {
			index = SZ_256 - 2;
			msg[index] = '\0';
			break;
		}
	}
	log_hwt_irq_info(addr, "%s\n", msg);

	/*record irq info*/
	for (irq = 1; irq < min_t(int, nr_irqs, MAX_IRQ_NUM); irq++) {
		index = 0;
		memset(msg, 0, SZ_256);
		desc = irq_to_desc(irq);

		if (desc && desc->kstat_irqs) {
			/*Skip no registered irq*/
			if (!(desc->action && desc->action->name))
				continue;

			len = scnprintf(&msg[index], SZ_256-index, "%3d: ", irq);
			index += len;

			for (j = 0; j < min_t(int, nr_cpu_ids, MAX_CPU_NUM); j++) {
				len = scnprintf(&msg[index], SZ_256-index, "%-10d ",
				/*
				 * read desc->kstat_irqs maybe encounter data race.
				 * use data_race bypass iterator_category
				 */
					data_race(*per_cpu_ptr(desc->kstat_irqs, j)));
				index += len;
				if (index >= SZ_256 - 2) {
					index = SZ_256 - 2;
					msg[index] = '\0';
					goto flush;
				}
			}

			if (desc->irq_data.chip) {
				if (desc->irq_data.chip->name) {
					len = scnprintf(&msg[index], SZ_256-index, "%-8s ",
						desc->irq_data.chip->name);
				} else {
					len = scnprintf(&msg[index], SZ_256-index, "%-8s ", "-");
				}
			} else {
				len = scnprintf(&msg[index], SZ_256-index, "%-8s ", "None");
			}
			index += len;
			if (index >= SZ_256 - 2) {
				index = SZ_256 - 2;
				msg[index] = '\0';
				goto flush;
			}

			if(desc->irq_data.hwirq) {
				len = scnprintf(&msg[index], SZ_256-index, "%3lu ",
					desc->irq_data.hwirq);
			} else {
				len = scnprintf(&msg[index], SZ_256-index, "%3s ", "-");
			}
			index += len;
			if (index >= SZ_256 - 2) {
				index = SZ_256 - 2;
				msg[index] = '\0';
				goto flush;
			}

			if (desc->action && desc->action->name) {
				const char *irq_name = desc->action->name;

				if (!strcmp(irq_name, "IPI")) {
					scnprintf(&msg[index], SZ_256-index, "  %s%d\n",
						irq_name, irq_to_ipi_type(irq));
				} else {
					scnprintf(&msg[index], SZ_256-index, "  %s\n", irq_name);
				}
			} else {
				scnprintf(&msg[index], SZ_256-index, "  %s\n", "NULL");
			}
flush:
			log_hwt_irq_info(addr, "%s", msg);
		}
	}

}

static void save_irq_info(void)
{
	if (hwt_irq_info == NULL)
		return;

	if (cur_buf == irq_buf_a)
		cur_buf = irq_buf_b;
	else
		cur_buf = irq_buf_a;
	memset(cur_buf, 0, MAX_HWT_IRQ_BUF_SIZE);
	write_irq_buf_index = 0;
	show_irq_info(cur_buf);
}
#endif

static void show_irq_count(void)
{
#define MAX_IRQ_NUM 1024
	static unsigned int irq_counts[MAX_IRQ_NUM];
	unsigned int count, unkick_cpu = cpumask_first(cpu_online_mask);
	unsigned int unkick_cpumask = (get_kick_bit()^get_check_bit())&get_check_bit();
	struct task_struct *tsk = cpu_curr(unkick_cpu);
	u64 preempt_cnt = task_thread_info(tsk)->preempt_count;
	struct irq_desc *desc;
	char msg[192];
	char handler[128];
	int irq;

	if (unkick_cpumask != (1U << unkick_cpu) ||
		!(preempt_cnt&(NMI_MASK|SOFTIRQ_MASK|HARDIRQ_MASK)))
		return;
	if (toprgu_base)
		iowrite32(WDT_RST_RELOAD, toprgu_base + WDT_RST);
	for (irq = 0; irq < min_t(int, nr_irqs, MAX_IRQ_NUM); irq++) {
		desc = irq_to_desc(irq);
		if (desc && desc->kstat_irqs) {
			 /*
			  * read desc->kstat_irqs maybe encounter data race.
			  * use data_race bypass iterator_category
			  */
			irq_counts[irq] = data_race(*per_cpu_ptr(desc->kstat_irqs, unkick_cpu));
		}
	}
	mdelay(2000);
	aee_sram_fiq_log("show irq count in 2s:\n");
	for (irq = 0; irq < min_t(int, nr_irqs, MAX_IRQ_NUM); irq++) {
		desc = irq_to_desc(irq);
		if (!desc || !desc->kstat_irqs)
			continue;
		/*
		 * read desc->kstat_irqs maybe encounter data race.
		 * use data_race bypass iterator_category
		 */
		count = data_race(*per_cpu_ptr(desc->kstat_irqs, unkick_cpu));
		if (count == irq_counts[irq])
			continue;

		memset(handler, 0, sizeof(handler));
		if (desc->action && desc->action->handler) {
			scnprintf(handler, sizeof(handler), "irq_handler: %ps",
					desc->action->handler);
		}
		if (desc->action && desc->action->name) {
			const char *irq_name = desc->action->name;

			if (!strcmp(irq_name, "IPI"))
				scnprintf(msg, sizeof(msg), "%3d:%s%d +%d(%d) %s\n",
						irq, irq_name, irq_to_ipi_type(irq),
						count - irq_counts[irq], count, handler);
			else
				scnprintf(msg, sizeof(msg), "%3d:%s +%d(%d) %s\n",
						irq, irq_name, count - irq_counts[irq], count, handler);
		} else {
			scnprintf(msg, sizeof(msg), "%3d:%s +%d(%d) %s\n", irq,
					"NULL", count - irq_counts[irq], count, handler);
		}

		aee_sram_fiq_log(msg);
	}
	aee_sram_fiq_log("\n");
}

#if IS_ENABLED(CONFIG_SMP)
static struct __call_single_data wdt_csd[MAX_CPUNR];
#endif
static void kwdt_dump_func(void)
{
	struct task_struct *g, *t;
	int i = 0;

#if IS_ENABLED(CONFIG_SMP)
#if !IS_ENABLED(CONFIG_ARM64)
	for (i = 0; i < CPU_NR; i++)
		pr_info("%s", tmr_buf[i]);
#else
	unsigned int unkick_mask =
		(get_kick_bit() ^ get_check_bit()) & get_check_bit();

	pr_info("unkick_mask %x\n", unkick_mask);

	for (i = 0; i < CPU_NR; i++) {
		pr_info("%s\n", tmr_buf[i]);
		if (unkick_mask & (1 << i)) {
			wdt_csd[i].info = tmr_buf;
			smp_call_function_single_async(i, &wdt_csd[i]);
			wdt_csd[i].info = NULL;
		}
	}
#endif
#endif

	for_each_process_thread(g, t) {
		if (!strcmp(t->comm, "watchdogd")) {
			pr_info("watchdogd on CPU %d\n", task_thread_info(t)->cpu);
			sched_show_task(t);
			break;
		}
	}
#if !IS_ENABLED(CONFIG_ARM64)
	for_each_process_thread(g, t) {
		if (!strncmp(t->comm, "wdtk-", 5)) {
			pr_info("%s on CPU %d\n", t->comm, task_thread_info(t)->cpu);
			sched_show_task(t);
			break;
		}
	}
#else
	for_each_process_thread(g, t) {
		if (!strncmp(t->comm, "wdtk-", 5)) {
			int cpu = task_thread_info(t)->cpu;

			pr_info("%s on CPU %d\n", t->comm, cpu);
			sched_show_task(t);

			if (unkick_mask & (1 << cpu)) {
				struct slp_history *slp = &wk_sl[cpu];

				/*
				 * enable/disable kprobe cannot be added in
				 * here because preemption is disabled and
				 * the two functions might be sleep
				 */

				if (IS_ERR_OR_NULL(slp) ||
					IS_ERR_OR_NULL(slp->timer)) {
					pr_info("slp is null %d %lld %llx %llx\n",
						slp->cpu, slp->sc,
						(unsigned long long)slp,
						(unsigned long long)slp->timer);
				} else {
					struct hrtimer *wdt_timer = slp->timer;

					if (IS_ERR_OR_NULL(wdt_timer) ||
						IS_ERR_OR_NULL(wdt_timer->base) ||
						IS_ERR_OR_NULL(wdt_timer->base->cpu_base))
						pr_info("wdt sleeper is invalid\n");
					else
						pr_info("%d %lld timer on cpu %d softexpires %lld\n",
						slp->cpu, slp->sc,
						IS_ERR_OR_NULL(wdt_timer) ? 100 :
						 wdt_timer->base->cpu_base->cpu,
						IS_ERR_OR_NULL(wdt_timer) ? 100 :
						 wdt_timer->_softexpires);
				}

			}
			break;
		}
	}

	if(IS_ERR_OR_NULL(bc_mtk_clkevt))
		pr_info("mtk-clkevt is invalid\n");
	else
		pr_info("%s irq %d cpumask %*pbl next_event %lld\n",
			bc_mtk_clkevt->name, bc_mtk_clkevt->irq,
			cpumask_pr_args(bc_mtk_clkevt->cpumask),
			bc_mtk_clkevt->next_event);
#endif
	for (i = 0; i < CPU_NR; i++) {
		struct rq *rq;

		pr_info("task on CPU%d\n", i);
		rq = cpu_rq(i);
		if (cpu_rq(i))
			sched_show_task(rq->curr);
	}

	dump_wdk_bind_info(true);

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR)
	if (p_mt_aee_dump_irq_info)
		p_mt_aee_dump_irq_info();
#endif
	show_irq_count();
#if CHK_HWT_IRQ
	save_irq_info();
#endif
	sysrq_sched_debug_show_at_AEE();
	save_timer_list_info();

	if (toprgu_base)
		iowrite32(WDT_RST_RELOAD, toprgu_base + WDT_RST);
	/* trigger HWT */
	crash_setup_regs(&saved_regs, NULL);
	if (apwdt_en)
		mrdump_common_die(AEE_REBOOT_MODE_WDT, "HWT", &saved_regs);
}

static void aee_dump_timer_func(struct timer_list *t)
{
	spin_lock(&lock);

	if (sched_clock() - aee_dump_timer_t < CHG_TMO_DLY_SEC * 1000000000ULL) {
		g_change_tmo = 0;
		aee_dump_timer_t = 0;
		g_hang_detected = 0;
		spin_unlock(&lock);
		return;
	}

	if ((sched_clock() > all_k_timer_t) &&
	    (sched_clock() - all_k_timer_t) < (CHG_TMO_DLY_SEC + 1) * 1000000000ULL) {
		g_change_tmo = 0;
		aee_dump_timer_t = 0;
		g_hang_detected = 0;
		spin_unlock(&lock);
		return;
	} else if ((all_k_timer_t > sched_clock()) &&
	    (ULLONG_MAX - all_k_timer_t + sched_clock()) < (CHG_TMO_DLY_SEC + 1) * 1000000000ULL) {
		g_change_tmo = 0;
		aee_dump_timer_t = 0;
		g_hang_detected = 0;
		spin_unlock(&lock);
		return;
	}

	if (!g_hang_detected ||
	    (get_kick_bit() & get_check_bit()) == get_check_bit()) {
		g_change_tmo = 0;
		aee_dump_timer_t = 0;
		spin_unlock(&lock);
		if (toprgu_base) {
			unsigned int tmo_len = 0;

			tmo_len = ioread32(toprgu_base + WDT_LENGTH);
			iowrite32(tmo_len | WDT_LENGTH_KEY, toprgu_base + WDT_LENGTH);
			iowrite32(WDT_RST_RELOAD, toprgu_base + WDT_RST);
		}
	} else {
		spin_unlock(&lock);
		kwdt_dump_func();
	}
}

#if IS_ENABLED(CONFIG_SMP)
#if !IS_ENABLED(CONFIG_ARM64)
static u32 arch_timer_reg_read_tval(void)
{
	u32 val = 0;
	asm volatile("mrc p15, 0, %0, c14, c3, 0" : "=r" (val));
	return val;
}
#endif

static void wdt_dump_cntcv(void *arg)
{
	int ret = -1;
	uint64_t cnt = 0;
	uint32_t low = 0;

	if (systimer_base) {
		low = readl(systimer_base + SYSTIMER_CNTCV_L);
		cnt = readl(systimer_base + SYSTIMER_CNTCV_H);
		cnt = cnt << 32 | low;
	}

	if (arg)
		pr_info("sc:%lld\n", sched_clock());
	else {
#if IS_ENABLED(CONFIG_ARM64)
		ret = snprintf(tmr_buf[smp_processor_id()], WK_MAX_MSG_SIZE,
			"CPU:%d CNTCV_CTL:%llx CNTCV_TVAL:%llx CNTVCT:%llx SYST_CNTCR:%x SYST_DISTL:%x SYST_CNTCV:%llx sc:%lld\n",
			smp_processor_id(),
			read_sysreg(cntv_ctl_el0),
			read_sysreg(cntv_tval_el0),
			read_sysreg(cntvct_el0),
			systimer_base ? ioread32(systimer_base + SYST_CNTCR) : 0,
			systimer_base ? ioread32(systimer_base + SYST_DISTL) : 0,
			systimer_base ? cnt : 0,
			sched_clock());
#else
	ret = snprintf(tmr_buf[smp_processor_id()], WK_MAX_MSG_SIZE,
		"%s CPU:%d CNTCV_CTL:%x CNTCV_TVAL:%x CNTVCT:%llx SYST_CNTCR:%x SYST_DISTL:%x SYST_CNTCV:%llx sc:%lld\n",
		__func__,
		smp_processor_id(),
		arch_timer_reg_read_cp15(ARCH_TIMER_VIRT_ACCESS, ARCH_TIMER_REG_CTRL),
		arch_timer_reg_read_tval(),
		__arch_counter_get_cntvct(),
		systimer_base ? ioread32(systimer_base + SYST_CNTCR) : 0,
		systimer_base ? ioread32(systimer_base + SYST_DISTL) : 0,
		systimer_base ? cnt : 0,
		sched_clock());
#endif
	}

	if (ret < 0) {
		tmr_buf[smp_processor_id()][0] = 'E';
		tmr_buf[smp_processor_id()][1] = 'R';
		tmr_buf[smp_processor_id()][2] = 'R';
		tmr_buf[smp_processor_id()][3] = '\0';
	}
}
#endif

static void kwdt_process_kick(int local_bit, int cpu,
				unsigned long curInterval, char msg_buf[],
				unsigned int original_kicker)
{
	unsigned int dump_timeout = 0, r_counter = DEFAULT_INTERVAL;
	int i = 0, ret = -1;
	bool rgu_fiq = false;
	unsigned long s_s2idle = get_s2idle_state();
#if !IS_ENABLED(CONFIG_MTK_AEE_HANGDET_IMPROVE_PERFORMANCE)
	char smp_histroy[60] = {'\0'};
#if IS_ENABLED(CONFIG_SMP)
	static int j;
#endif
#endif

	if (toprgu_base && (ioread32(toprgu_base + WDT_MODE) & WDT_MODE_EN))
		r_counter = ioread32(toprgu_base + WDT_COUNTER) / (32 * 1024);

	if (toprgu_base && (ioread32(toprgu_base + WDT_STATUS) & WDT_STATUS_IRQ))
		rgu_fiq = true;

	if (aee_dump_timer_t && ((sched_clock() - aee_dump_timer_t) >
	    (CHG_TMO_DLY_SEC + 5) * 1000000000ULL)) {
		if (!aee_dump_timer_c) {
			aee_dump_timer_c = 1;
			ret = snprintf(msg_buf, WK_MAX_MSG_SIZE, "wdtk-et %s %d cpu=%d o_k=%d\n",
				  __func__, __LINE__, cpu, original_kicker);
			spin_unlock_bh(&lock);

			if (ret >= 0)
				pr_info("%s", msg_buf);
			kwdt_dump_func();
			return;
		}

		ret = snprintf(msg_buf, WK_MAX_MSG_SIZE,
			"all wdtk was already stopped cpu=%d o_k=%d\n",
			cpu, original_kicker);

		spin_unlock_bh(&lock);

		if (ret >= 0)
			pr_info("%s", msg_buf);
		return;
	}

	local_bit = kick_bit;
	if (cpu != original_kicker) {
		/* wdtk-(original_kicker) is migrated to (cpu) */
		local_bit |= (1 << original_kicker);
	} else if ((local_bit & (1 << cpu)) == 0) {
		/* pr_debug("[wdk] set kick_bit\n"); */
		local_bit |= (1 << cpu);
		/* aee_rr_rec_wdk_kick_jiffies(jiffies); */
	} else if ((g_hang_detected == 0) &&
		    ((local_bit & (get_check_bit() & s_s2idle)) !=
		     (get_check_bit() & s_s2idle)) &&
		    (sched_clock() - wk_lasthpg_t[cpu] >
		     curInterval * 1000)) {
		g_hang_detected = 1;
		dump_timeout = 1;
	}

	if ((g_hang_detected == 0) &&
		    (r_counter < DEFAULT_INTERVAL - 10) && !g_change_tmo) {
		g_hang_detected = 1;
		dump_timeout = 2;
	}
#if IS_ENABLED(CONFIG_SMP) && (!IS_ENABLED(CONFIG_MTK_AEE_HANGDET_IMPROVE_PERFORMANCE))
	if ((((~(local_bit - 1)) & local_bit) == local_bit) && j++ > 3) {
		int cpu = 0;
		int smp_ret[MAX_CPUNR] = {255};

		for (cpu = 0; cpu < CPU_NR; cpu++) {
			smp_ret[cpu] = 255;
			if (get_check_bit() & (1 << cpu))
				smp_ret[cpu] = smp_call_function_single_async(cpu, &wdt_csd[cpu]);
		}

		ret = snprintf(smp_histroy, 60, "s_cpu %d - %d %d %d %d %d %d %d %d\n",
			smp_processor_id(), smp_ret[0], smp_ret[1], smp_ret[2], smp_ret[3],
			 smp_ret[4], smp_ret[5], smp_ret[6], smp_ret[7]);
	}
#endif

	wk_tsk_kick_time[cpu] = sched_clock();
#if !IS_ENABLED(CONFIG_MTK_AEE_HANGDET_IMPROVE_PERFORMANCE)
#if !IS_ENABLED(CONFIG_ARM64)
	ret = snprintf(msg_buf, WK_MAX_MSG_SIZE,
	 "[wdk-c] cpu=%d o_k=%d lbit=0x%x cbit=0x%x,%x,%d,%d,%lld,%x,%llu,%llu,%llu,%llu,[%lld,%ld] %d %lx\n",
	 cpu, original_kicker, local_bit, get_check_bit(),
	 (local_bit ^ get_check_bit()) & get_check_bit(), lasthpg_cpu,
	 lasthpg_act, lasthpg_t, atomic_read(&plug_mask), div_u64(lastsuspend_t, 1000000),
	 div_u64(lastsuspend_syst, 1000000), div_u64(lastresume_t, 1000000),
	 div_u64(lastresume_syst, 1000000), wk_tsk_kick_time[cpu], curInterval, r_counter, s_s2idle);
#else
	ret = snprintf(msg_buf, WK_MAX_MSG_SIZE,
	 "[wdk-c] cpu=%d o_k=%d lbit=0x%x cbit=0x%x,%x,%d,%d,%lld,%x,%llu,%llu,%llu,%llu,[%lld,%ld] %d %lx %s\n",
	 cpu, original_kicker, local_bit, get_check_bit(),
	 (local_bit ^ get_check_bit()) & get_check_bit(), lasthpg_cpu,
	 lasthpg_act, lasthpg_t, atomic_read(&plug_mask), div_u64(lastsuspend_t, 1000000),
	 div_u64(lastsuspend_syst, 1000000), div_u64(lastresume_t, 1000000),
	 div_u64(lastresume_syst, 1000000), wk_tsk_kick_time[cpu], curInterval, r_counter, s_s2idle,
	 (ret >= 0) ? smp_histroy : " ");
#endif
#endif
	if ((local_bit & (get_check_bit() & s_s2idle)) == (get_check_bit() & s_s2idle)) {
		all_k_timer_t = sched_clock();
		if (timer_pending(&aee_dump_timer))
			del_timer(&aee_dump_timer);
		aee_dump_timer_t = 0;
		cpus_skip_bit = 0;
		msg_buf[5] = 'k';
		if (g_hang_detected == 2)
			pm_system_wakeup();
		g_hang_detected = 0;
		dump_timeout = 0;
		local_bit = 0;
		kwdt_time_sync();
#if !IS_ENABLED(CONFIG_MTK_AEE_HANGDET_IMPROVE_PERFORMANCE)
#if CHK_HWT_IRQ
		save_irq_info();
#endif
#endif
		if (toprgu_base)
			iowrite32(WDT_RST_RELOAD, toprgu_base + WDT_RST);
	}

	kick_bit = local_bit;

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	if (!dump_timeout) {
		aee_rr_rec_kick(('D' << 24) | local_bit);
		aee_rr_rec_check(('B' << 24) | get_check_bit());
	}
#endif

	for (i = 0; i < CPU_NR; i++) {
		if ((atomic_read(&plug_mask) & (1 << i)) || (i == cpu)) {
			cpus_kick_bit |= (1 << i);
			if (cpus_skip_bit & (1 << i))
				cpus_kick_bit &= ~(1 << i);
		}
	}

	if (cpu != original_kicker) {
		cpus_kick_bit &= ~(1 << cpu);
		cpus_skip_bit |= (1 << cpu);
	}

	spin_unlock_bh(&lock);

#if !IS_ENABLED(CONFIG_MTK_AEE_HANGDET_IMPROVE_PERFORMANCE)
	if (ret >= 0)
		pr_info("%s", msg_buf);

#if IS_ENABLED(CONFIG_SMP)
	pr_info("%s", tmr_buf[cpu]);
#endif
#endif

	if (dump_timeout) {
#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
		tick_broadcast_mtk_aee_dump();
		if (systimer_irq)
			mt_irq_dump_status(systimer_irq);
#endif
		dump_wdk_bind_info(false);
		//sysrq_sched_debug_show_at_AEE();

		if (systimer_base)
			pr_info("SYST0 CON%x VAL%x\n",
				ioread32(systimer_base + SYST0_CON),
				ioread32(systimer_base + SYST0_VAL));
#if CHG_TMO_EN
		if (toprgu_base) {
			spin_lock_bh(&lock);
			g_change_tmo = 1;
			spin_unlock_bh(&lock);
			iowrite32((WDT_LENGTH_TIMEOUT(6) << 6) | WDT_LENGTH_KEY,
				toprgu_base + WDT_LENGTH);
			iowrite32(WDT_RST_RELOAD, toprgu_base + WDT_RST);
		}
#endif
		/* abort suspend when wdt kick */
		if (dump_timeout == 2)
			pm_system_wakeup();
		else {
			spin_lock_bh(&lock);
			if (g_hang_detected && !aee_dump_timer_t &&
				!timer_pending(&aee_dump_timer)) {
				pm_system_wakeup();
				aee_dump_timer_t = sched_clock();
				g_change_tmo = 1;
				aee_dump_timer.expires = jiffies + CHG_TMO_DLY_SEC * HZ;
				add_timer(&aee_dump_timer);
				spin_unlock_bh(&lock);
				return;
			}
			spin_unlock_bh(&lock);
		}
	}

	if (rgu_fiq)
		pr_info("RGU IRQ triggered, but not raise FIQ\n");
}

static int kwdt_thread(void *arg)
{
#if !IS_ENABLED(CONFIG_GRT_HYPERVISOR)
	struct sched_param param = {.sched_priority = 99 };
#else
	struct sched_param param = {.sched_priority = 50 };
#endif
	int cpu = 0;
	int local_bit = 0;
	unsigned long curInterval = 0;
	char msg_buf[WK_MAX_MSG_SIZE];

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	percpu_debug_timer_init();
	for (;;) {
		if (kthread_should_stop()) {
			pr_info("[wdk] kthread_should_stop do !!\n");
			break;
		}
		msg_buf[0] = '\0';
		/*
		 * pr_debug("[wdk] loc_wk_wdt(%x),loc_wk_wdt->ready(%d)\n",
		 * loc_wk_wdt ,loc_wk_wdt->ready);
		 */
		curInterval = g_kinterval*1000*1000;
		spin_lock_bh(&lock);
		/* smp_processor_id does not
		 * allowed preemptible context
		 */
		cpu = smp_processor_id();

		/* to avoid wk_tsk[cpu] had not created out */
		if (wk_tsk[cpu] != 0) {
			if ((kick_bit & get_check_bit()) == 0) {
				g_nxtKickTime = ktime_to_us(ktime_get())
					+ g_kinterval*1000*1000;
				curInterval = g_kinterval*1000*1000;
			} else {
				curInterval = g_nxtKickTime
				- ktime_to_us(ktime_get());
			}
			/* to avoid interval too long */
			if (curInterval > g_kinterval*1000*1000)
				curInterval = g_kinterval*1000*1000;

			kwdt_process_kick(local_bit, cpu, curInterval,
				msg_buf, *((unsigned int *)arg));
		} else {
			spin_unlock_bh(&lock);
		}

		usleep_range(curInterval, curInterval + SOFT_KICK_RANGE);
	}
	pr_debug("[wdk] wdk thread stop, cpu:%d, pid:%d\n", cpu, current->pid);
	return 0;
}

static int start_kicker(void)
{
	int i;

	for (i = 0; i < CPU_NR; i++) {
		if (cpu_online(i)) {
			cpuid_t[i] = i;
			wk_tsk[i] = kthread_create(kwdt_thread,
				(void *) &cpuid_t[i], "wdtk-%d", i);
			if (IS_ERR(wk_tsk[i])) {
				int ret = PTR_ERR(wk_tsk[i]);

				wk_tsk[i] = NULL;
				pr_info("[wdk]kthread_create failed, wdtk-%d\n", i);
				return ret;
			}
			/* wk_cpu_update_bit_flag(i,1); */
			wk_start_kick_cpu(i);
			atomic_or(1 << i, &plug_mask);
		} else
			atomic_andnot(1 << i, &plug_mask);
	}
	g_kicker_init = 1;
	pr_info("[wdk] WDT start kicker done CPU_NR=%d online cpu NR%d\n",
		CPU_NR, num_online_cpus());
	return 0;
}

static int wk_cpu_callback_online(unsigned int cpu)
{
	wk_cpu_update_bit_flag(cpu, 1, 0);
	wk_lasthpg_t[cpu] = sched_clock();
	atomic_or(1 << cpu, &plug_mask);
	/*
	 * Bind WDK thread to this CPU.
	 * NOTE: Thread binding must be executed after CPU is ready
	 * (online).
	 */
	if (g_kicker_init == 1)
		kicker_cpu_bind(cpu);
	else
		pr_info("kicker was not bound to CPU%d\n", cpu);

	return 0;
}

static int wk_cpu_callback_offline(unsigned int cpu)
{
	wk_cpu_update_bit_flag(cpu, 0, 1);

	atomic_andnot(1 << cpu, &plug_mask);
	return 0;
}

static void wdk_work_callback(struct work_struct *work)
{
	int res = 0;
	int i = 0;

	cpu_hotplug_disable();

	res = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
		"watchdog:wdkctrl:online", wk_cpu_callback_online, NULL);
	if (res < 0)
		pr_info("[wdk]setup CPUHP_AP_ONLINE_DYN fail %d\n", res);

	res = cpuhp_setup_state_nocalls(CPUHP_BP_PREPARE_DYN,
		"watchdog:wdkctrl:offline", NULL, wk_cpu_callback_offline);
	if (res < 0)
		pr_info("[wdk]setup CPUHP_BP_PREPARE_DYN fail %d\n", res);

	for (i = 0; i < CPU_NR; i++) {
		if (cpu_online(i)) {
			wk_cpu_update_bit_flag(i, 1, 1);
			pr_debug("[wdk]init cpu online %d\n", i);
		} else {
			wk_cpu_update_bit_flag(i, 0, 1);
			pr_debug("[wdk]init cpu offline %d\n", i);
		}
	}

	start_kicker_thread_with_default_setting();

	cpu_hotplug_enable();

	pr_info("[wdk]init_wk done late_initcall cpus_kick_bit=0x%x -----\n",
		cpus_kick_bit);

}

static int wdt_pm_notify(struct notifier_block *notify_block,
			unsigned long mode, void *unused)
{
	uint64_t cnt = 0;

	if (systimer_base) {
		uint32_t low = 0;

		cnt = sched_clock();
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		aee_rr_rec_wdk_ktime(cnt);
#endif

		low = readl(systimer_base + SYSTIMER_CNTCV_L);
		cnt = readl(systimer_base + SYSTIMER_CNTCV_H);
		cnt = cnt << 32 | low;
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		aee_rr_rec_wdk_systimer_cnt(cnt);
#endif
	}

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
	case PM_RESTORE_PREPARE:
		lastsuspend_t = sched_clock();
		lastsuspend_syst = cnt;

		spin_lock_bh(&lock);
		if (timer_pending(&aee_dump_timer))
			del_timer(&aee_dump_timer);
		aee_dump_timer_t = 0;
		g_hang_detected = 0;
		spin_unlock_bh(&lock);
		break;

	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		lastresume_t = sched_clock();
		lastresume_syst = cnt;
		break;
	}

	if (toprgu_base)
		iowrite32(WDT_RST_RELOAD, toprgu_base + WDT_RST);

	return 0;
}

static int __init init_wk_check_bit(void)
{
	int i = 0;

	pr_debug("[wdk]arch init check_bit=0x%x+++++\n", cpus_kick_bit);
	for (i = 0; i < CPU_NR; i++) {
		if (cpu_online(i))
			wk_cpu_update_bit_flag(i, 1, 1);
	}

	pr_debug("[wdk]arch init check_bit=0x%x-----\n", cpus_kick_bit);
	return 0;
}

static void wdt_mark_stage(unsigned int stage)
{
	unsigned int reg = ioread32(toprgu_base + WDT_NONRST_REG2);

	reg = (reg & ~(WDT_STAGE_MASK << WDT_STAGE_OFS))
		| (stage << WDT_STAGE_OFS);
	iowrite32(reg, toprgu_base + WDT_NONRST_REG2);
}

static void reboot_set_flag(bool op)
{
	unsigned int reg = ioread32(toprgu_base + WDT_NONRST_REG2);

	pr_info("reboot set flag, old value 0x%x, %d.\n", reg, op);
	reg = ((reg & ~(REBOOT_FLAG_MASK << REBOOT_FLAG_OFS))
		| (op ? 1 : 0) << REBOOT_FLAG_OFS);
	iowrite32(reg, toprgu_base + WDT_NONRST_REG2);
	pr_info("reboot set flag new value 0x%x.\n", reg);
}


static int aee_reset(struct notifier_block *nb, unsigned long action, void *data)
{
	reboot_set_flag(false);
	return 0;
}

static struct notifier_block aee_reboot_notify = {
	.notifier_call = aee_reset,
};


static void aee_reboot_hook_init(void)
{
	int ret = 0;

	ret = register_reboot_notifier(&aee_reboot_notify);
	if (ret)
		pr_err("register restart handler failed: 0x%x.\n", ret);

	/* set reboot flag */
	reboot_set_flag(true);
}

static void aee_reboot_hook_exit(void)
{
	int ret = 0;

	ret = unregister_reboot_notifier(&aee_reboot_notify);
	if (ret != 0)
		pr_err("unregister restart handler failed: 0x%x.\n", ret);

	/* clear reboot flag */
	reboot_set_flag(false);
}


static const struct of_device_id toprgu_of_match[] = {
	{ .compatible = "mediatek,mt6589-wdt" },
	{},
};

static const struct of_device_id systimer_of_match[] = {
	{ .compatible = "mediatek,mt6765-timer" },
	{ .compatible = "mediatek,legacy-timer" },
	{},
};

#if IS_ENABLED(CONFIG_ARM64)
static int hrtimer_start_range_ns_pre(struct kprobe *p, struct pt_regs *regs)
{
	if (!strncmp(current->comm, "wdtk-", 5)) {
		struct slp_history *slp;
		unsigned long flags;

		spin_lock_irqsave(&kp_lock, flags);

		if (!strncmp(current->comm, "wdtk-0", 6))
			slp = &wk_sl[0];
		else if (!strncmp(current->comm, "wdtk-1", 6))
			slp = &wk_sl[1];
		else if (!strncmp(current->comm, "wdtk-2", 6))
			slp = &wk_sl[2];
		else if (!strncmp(current->comm, "wdtk-3", 6))
			slp = &wk_sl[3];
		else if (!strncmp(current->comm, "wdtk-4", 6))
			slp = &wk_sl[4];
		else if (!strncmp(current->comm, "wdtk-5", 6))
			slp = &wk_sl[5];
		else if (!strncmp(current->comm, "wdtk-6", 6))
			slp = &wk_sl[6];
		else if (!strncmp(current->comm, "wdtk-7", 6))
			slp = &wk_sl[7];
		else {
			spin_unlock_irqrestore(&kp_lock, flags);
			return 0;
		}

		if (IS_ERR_OR_NULL(slp)) {
			spin_unlock_irqrestore(&kp_lock, flags);
			return 0;
		}

		slp->cpu = smp_processor_id();
		slp->sc = sched_clock();
		slp->timer = (struct hrtimer *)regs->regs[0];

		spin_unlock_irqrestore(&kp_lock, flags);
	}

	return 0;
}

static int clockevents_exchange_device_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct clock_event_device *clkevt = (struct clock_event_device *)regs->regs[1];

	if (!IS_ERR_OR_NULL(clkevt) && (clkevt->features & CLOCK_EVT_FEAT_ONESHOT) &&
	    !strncmp(clkevt->name, "mtk-clkevt", 10))
		bc_mtk_clkevt = clkevt;

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
static int aee_kernel_warning_api_func_pre(struct kprobe *p, struct pt_regs *regs)
{
	char *module = (char *)regs->regs[3];

	if (strstr(module, "arch_timer") != NULL) {
		dump_usleep_range_history();
		dump_hrtimer_burst_history();
	}

	return 0;
}

static int usleep_range_state_pre(struct kprobe *p, struct pt_regs *regs)
{
	u64 temp_count = 0;
	unsigned long flags = 0;
	unsigned int state = (unsigned int)regs->regs[2];

	if (state == TASK_UNINTERRUPTIBLE) {
		spin_lock_irqsave(&usleep_range_lock, flags);
		usleep_range_count++;
		temp_count = usleep_range_count % USLEEP_RANGE_HIS_ARRAY_SIZE;
		usleep_range_history[temp_count].timer_caller_ip = ((unsigned long)__builtin_return_address(8));
		usleep_range_history[temp_count].last_time = sched_clock();
		usleep_range_history[temp_count].caller_cpu = smp_processor_id();
		strscpy(usleep_range_history[temp_count].comm, current->comm, TASK_COMM_LEN);
		spin_unlock_irqrestore(&usleep_range_lock, flags);
	}

	return 0;
}

static int hrtimer_expire_entry_pre(struct kprobe *p, struct pt_regs *regs)
{
	u64 temp_count = 0;
	unsigned long flags = 0;
	int i;

	struct hrtimer *hrtimer_t = (struct hrtimer *)regs->regs[2];
	ktime_t *now_t = (ktime_t *)regs->regs[3];

	spin_lock_irqsave(&hrtimer_lock, flags);

	struct hrtimer_count_struct *cpu_counts = this_cpu_ptr(hrtimer_func_counts);
	uint64_t *last_time_ptr = this_cpu_ptr(&hrtimer_func_counts_last_time);

	if (sched_clock() - *last_time_ptr > 2000000000) {
		for (i = 0; i < HRTIMER_COUNT_ARRAY_SIZE; i++) {
			cpu_counts[i].timer_caller_ip = 0;
			cpu_counts[i].count = 0;
			cpu_counts[i].start_time = 0;
			cpu_counts[i].last_time = 0;
		}
		*last_time_ptr = sched_clock();
	}

	for (i = 0; i < HRTIMER_COUNT_ARRAY_SIZE; i++) {
		if (cpu_counts[i].timer_caller_ip == (unsigned long)hrtimer_t->function) {
			cpu_counts[i].count++;
			cpu_counts[i].last_time = sched_clock();
			if (cpu_counts[i].count > 0 && cpu_counts[i].count % 5000 == 0) {
				hrtimer_count++;
				temp_count = hrtimer_count % BURST_HRTIMER_HIS_ARRAY_SIZE;
				burst_hrtimer_history[temp_count].timer_caller_ip = (unsigned long)hrtimer_t->function;
				burst_hrtimer_history[temp_count].last_time = cpu_counts[i].last_time;
				burst_hrtimer_history[temp_count].start_time = cpu_counts[i].start_time;
				burst_hrtimer_history[temp_count].now = *now_t;
				burst_hrtimer_history[temp_count].caller_cpu = smp_processor_id();
				burst_hrtimer_history[temp_count].count = cpu_counts[i].count;
				strscpy(burst_hrtimer_history[temp_count].comm, current->comm, TASK_COMM_LEN);
			}
			break;
		} else if (cpu_counts[i].timer_caller_ip == 0) {
			cpu_counts[i].timer_caller_ip = (unsigned long)hrtimer_t->function;
			cpu_counts[i].count = 1;
			cpu_counts[i].start_time = sched_clock();
			cpu_counts[i].last_time = cpu_counts[i].start_time;
			break;
		}
	}

	spin_unlock_irqrestore(&hrtimer_lock, flags);

	return 0;
}
#endif
#endif

static int __init hangdet_init(void)
{
	int res = 0;
#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
	unsigned int systirq = 0;
#endif
	struct device_node *np_toprgu, *np_systimer, *s2idle_node;
	s2idle_node = of_find_node_by_name(NULL, "s2idle");
	if(s2idle_node) {
		is_s2idle_status = 1;
	}

#if IS_ENABLED(CONFIG_MTK_HANG_DETECT_DB)
	hwt_irq_info = kmalloc(MAX_HWT_IRQ_FILE_SIZE, GFP_KERNEL);
	if (hwt_irq_info != NULL) {
		irq_buf_a = hwt_irq_info;
		irq_buf_b = hwt_irq_info + MAX_HWT_IRQ_BUF_SIZE;
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		res = mrdump_mini_add_extra_file((unsigned long)hwt_irq_info,
			__pa_nodebug(hwt_irq_info), MAX_HWT_IRQ_FILE_SIZE, "HWT_IRQ");
		if (res) {
			kfree(hwt_irq_info);
			hwt_irq_info = NULL;
			pr_info("SYS_HWT_IRQ_RAW file add fail...\n");
		}
#endif
	}
#endif

	for_each_matching_node(np_toprgu, toprgu_of_match) {
		pr_info("%s: compatible node found: %s\n",
			 __func__, np_toprgu->name);
		break;
	}

	toprgu_base = of_iomap(np_toprgu, 0);
	if (!toprgu_base)
		pr_debug("toprgu iomap failed\n");
	else {
		int err1, err2;

		err1 = of_property_read_u32(np_toprgu, "apwdt_en", &apwdt_en);
		/* apwdt_en causes warning and build error in the end */
		err2 = of_property_read_u32(np_toprgu, "apwdten", &apwdt_en);
		if (err1 < 0 && err2 < 0)
			apwdt_en = 1;

		pr_info("apwdt %s\n", apwdt_en ? "enabled" : "disabled");
		wdt_mark_stage(WDT_STAGE_KERNEL);
	}

	for_each_matching_node(np_systimer, systimer_of_match) {
		pr_info("%s: compatible node found: %s\n",
			 __func__, np_systimer->name);
		break;
	}

	systimer_base = of_iomap(np_systimer, 0);
	if (!systimer_base)
		pr_debug("systimer iomap failed\n");

#if IS_ENABLED(CONFIG_MTK_TICK_BROADCAST_DEBUG)
	systirq = irq_of_parse_and_map(np_systimer, 0);
	if (systirq <= 0)
		systimer_irq = 0;
	else
		systimer_irq = systirq;
#endif
	init_wk_check_bit();

	wdk_workqueue = create_singlethread_workqueue("mt-wdk");
	INIT_WORK(&wdk_work, wdk_work_callback);

	res = queue_work(wdk_workqueue, &wdk_work);

	if (!res)
		pr_info("[wdk]wdk_work start return:%d!\n", res);

	wdt_pm_nb.notifier_call = wdt_pm_notify;
	register_pm_notifier(&wdt_pm_nb);

	if (systimer_base) {
		uint64_t cnt;
		uint32_t low;

		low = readl(systimer_base + SYSTIMER_CNTCV_L);
		cnt = readl(systimer_base + SYSTIMER_CNTCV_H);
		cnt = cnt << 32 | low;
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		aee_rr_rec_wdk_systimer_cnt(cnt);
#endif
		pr_info("%s systimer_cnt %lld\n", __func__, cnt);

		cnt = sched_clock();
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		aee_rr_rec_wdk_ktime(cnt);
#endif
		pr_info("%s set wdk_ktime %lld\n", __func__, cnt);
	}

	timer_setup(&aee_dump_timer, aee_dump_timer_func, 0);

#if IS_ENABLED(CONFIG_SMP)
	for (res = 0; res < 8; res++) {
		wdt_csd[res].func = wdt_dump_cntcv;
		wdt_csd[res].info = NULL;
	}
#endif

#if IS_ENABLED(CONFIG_ARM64)
	res = register_kprobe(&kp_hrtimer_start_range_ns);
	if (res < 0)
		pr_info("kp_hrtimer_start_range_ns kprobe failed %d\n", res);
	else
		pr_info("Planted kprobe at %llx for hrtimer_start_range_ns\n",
			(unsigned long long) kp_hrtimer_start_range_ns.addr);

	res = register_kprobe(&kp_clockevents_exchange_device);
	if (res < 0)
		pr_info("kp_clockevents_exchange_device kprobe failed %d\n", res);
	else
		pr_info("Planted kprobe at %llx for hrtimer_start_range_ns\n",
			(unsigned long long) kp_clockevents_exchange_device.addr);

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	res = register_kprobe(&kp_aee_kernel_warning_api_func);
	if (res < 0)
		pr_info("aee_kernel_warning_api_func kprobe failed %d\n", res);
	else
		pr_info("Planted kprobe at %p for aee_kernel_warning_api_func\n",
			kp_aee_kernel_warning_api_func.addr);

	res = register_kprobe(&kp_usleep_range_state);
	if (res < 0)
		pr_info("usleep_range_state kprobe failed %d\n", res);
	else
		pr_info("Planted kprobe at %p for usleep_range_state\n",
			kp_usleep_range_state.addr);

	res = register_kprobe(&kp_hrtimer_expire_entry);
	if (res < 0)
		pr_info("hrtimer_expire_entry kprobe failed %d\n", res);
	else
		pr_info("Planted kprobe at %p for hrtimer_expire_entry\n",
			kp_hrtimer_expire_entry.addr);

#endif
#endif

	aee_reboot_hook_init();
	timer_list_debug_init();

	return 0;
}

static void __exit hangdet_exit(void)
{
	int i = 0;

#if IS_ENABLED(CONFIG_ARM64)
	unregister_kprobe(&kp_hrtimer_start_range_ns);
	unregister_kprobe(&kp_clockevents_exchange_device);
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG) && IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	unregister_kprobe(&kp_aee_kernel_warning_api_func);
	unregister_kprobe(&kp_usleep_range_state);
	unregister_kprobe(&kp_hrtimer_expire_entry);
#endif
#endif
	unregister_pm_notifier(&wdt_pm_nb);

	for (i = 0; i < CPU_NR; i++) {
		if (IS_ERR(wk_tsk[i]))
			pr_debug("[wdk] wk_task[%d] is NULL\n", i);
		else
			kthread_stop(wk_tsk[i]);
	}

	aee_reboot_hook_exit();
	kfree(hwt_irq_info);
	timer_list_debug_exit();
}

module_init(hangdet_init);
module_exit(hangdet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mediatek inc.");
MODULE_DESCRIPTION("The cpu hang detector");
