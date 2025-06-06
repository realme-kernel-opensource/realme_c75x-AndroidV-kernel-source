// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/clocksource.h>
#include <linux/sched/clock.h>
#include <linux/suspend.h>
#include <linux/timex.h>
#include <linux/types.h>
#include <asm/arch_timer.h>
#include <asm/timex.h>
#include <linux/soc/mediatek/mtk-mbox.h>
#include "gpueb_timesync.h"
#include "gpueb_ipi.h"
#include "gpueb_helper.h"
#include "ghpm_wrapper.h"
#include "gpueb_common.h"

#define TIMESYNC_TAG	"[GPUEB_TS]"

#define TIMESYNC_MAX_VER           (0x7)
#define TIMESYNC_HEADER_FREEZE_OFS (31)
#define TIMESYNC_HEADER_FREEZE     (1 << TIMESYNC_HEADER_FREEZE_OFS)
#define TIMESYNC_HEADER_VER_OFS    (28)
#define TIMESYNC_HEADER_VER_MASK   (TIMESYNC_MAX_VER << TIMESYNC_HEADER_VER_OFS)

#define TIMESYNC_FLAG_SYNC     (1 << 0)
#define TIMESYNC_FLAG_ASYNC    (1 << 1)
#define TIMESYNC_FLAG_FREEZE   (1 << 2)
#define TIMESYNC_FLAG_UNFREEZE (1 << 3)

static unsigned int g_gpueb_ts_mbox;
static unsigned int g_gpueb_ts_mbox_offset_base;

/*
 * Shared MBOX: AP write, GPUEB read
 * Unit for each offset: 4 bytes
 */
#define GPUEB_TS_MBOX_TICK_H               (g_gpueb_ts_mbox_offset_base + 0)
#define GPUEB_TS_MBOX_TICK_L               (g_gpueb_ts_mbox_offset_base + 1)
#define GPUEB_TS_MBOX_TS_H                 (g_gpueb_ts_mbox_offset_base + 2)
#define GPUEB_TS_MBOX_TS_L                 (g_gpueb_ts_mbox_offset_base + 3)
#define GPUEB_TS_MBOX_DEBUG_TS_H           (g_gpueb_ts_mbox_offset_base + 4)
#define GPUEB_TS_MBOX_DEBUG_TS_L           (g_gpueb_ts_mbox_offset_base + 5)

#define gpueb_ts_write(id, val) \
	mtk_mbox_write(&gpueb_mboxdev, g_gpueb_ts_mbox, id, (void *)&val, 1)

struct timesync_context_t {
	spinlock_t lock;
	struct work_struct work;
	ktime_t wrap_kt;
	u8 enabled;
	u64 base_tick;
	u64 base_ts;
};

static struct workqueue_struct *timesync_workqueue;
static struct timesync_context_t timesync_ctx;
static struct timecounter timesync_counter;
static struct hrtimer timesync_refresh_timer;
static u8 gpueb_base_ver;
static u64 latest_tick, latest_ts;
static int latest_freeze;
static int need_sync;

static void gpueb_ts_update(int suspended, u64 tick, u64 ts)
{
	u32 header, val;

	gpueb_base_ver = (gpueb_base_ver + 1)%(TIMESYNC_MAX_VER+1);

	/* make header: freeze and version */
	header = suspended ? TIMESYNC_HEADER_FREEZE : 0;

	header |= ((gpueb_base_ver << TIMESYNC_HEADER_VER_OFS) &
		TIMESYNC_HEADER_VER_MASK);

	/* update tick, h -> l */
	val = (tick >> 32) & 0xFFFFFFFF;
	val |= header;
	gpueb_ts_write(GPUEB_TS_MBOX_TICK_H, val);

	/* fix update sequence to promise atomicity */
	mb();

	val = tick & 0xFFFFFFFF;
	gpueb_ts_write(GPUEB_TS_MBOX_TICK_L, val);

	/* fix update sequence to promise atomicity */
	mb();

	/* update ts, l -> h */
	val = ts & 0xFFFFFFFF;
	gpueb_ts_write(GPUEB_TS_MBOX_TS_L, val);

	/* fix update sequence to promise atomicity */
	mb();

	val = (ts >> 32) & 0xFFFFFFFF;
	val |= header;
	gpueb_ts_write(GPUEB_TS_MBOX_TS_H, val);

	/* fix update sequence to promise atomicity */
	mb();
}

void gpueb_timesync_update(void)
{
	unsigned long irq_flags = 0;

	if (need_sync) {
		spin_lock_irqsave(&timesync_ctx.lock, irq_flags);
		gpueb_ts_update(latest_freeze, latest_tick, latest_ts);
		need_sync = false;
		spin_unlock_irqrestore(&timesync_ctx.lock, irq_flags);
	}
}
EXPORT_SYMBOL(gpueb_timesync_update);

static u64 timesync_tick_read(const struct cyclecounter *cc)
{
	return arch_timer_read_counter();
}

static struct cyclecounter timesync_cc __ro_after_init = {
	.read	= timesync_tick_read,
	.mask	= CLOCKSOURCE_MASK(56),
};

static void timesync_sync_base_internal(unsigned int flag)
{
	u64 tick, ts;
	unsigned long irq_flags = 0;
	int freeze, unfreeze;

	spin_lock_irqsave(&timesync_ctx.lock, irq_flags);

	ts = timecounter_read(&timesync_counter);
	tick = timesync_counter.cycle_last;

	timesync_ctx.base_tick = tick;
	timesync_ctx.base_ts = ts;

	freeze = (flag & TIMESYNC_FLAG_FREEZE) ? 1 : 0;
	unfreeze = (flag & TIMESYNC_FLAG_UNFREEZE) ? 1 : 0;

	/* store latest values, sync with gpueb when pwr on */
	latest_ts = ts;
	latest_tick = tick;
	latest_freeze = freeze;

	/* sync with gpueb */
	if (!g_ghpm_support)
		gpueb_ts_update(freeze, tick, ts);
	else
		/* no need to sync when gpueb power on every time */
		need_sync = true;

	spin_unlock_irqrestore(&timesync_ctx.lock, irq_flags);

	gpueb_log_i(GPUEB_TAG, "%s update base: ts=%llu, tick=0x%llx, fz=%d, ver=%d",
		TIMESYNC_TAG, ts, tick, freeze, gpueb_base_ver);
}

static void timesync_sync_base(unsigned int flag)
{
	if (!timesync_ctx.enabled)
		return;

	if (flag & TIMESYNC_FLAG_ASYNC)
		queue_work(timesync_workqueue, &(timesync_ctx.work));
	else
		timesync_sync_base_internal(flag);
}

static enum hrtimer_restart timesync_refresh(struct hrtimer *hrt)
{
	hrtimer_forward_now(hrt, timesync_ctx.wrap_kt);
	/* snchronize new sched_clock base to co-processors */
	timesync_sync_base(TIMESYNC_FLAG_ASYNC);

	return HRTIMER_RESTART;
}

static void timesync_ws(struct work_struct *ws)
{
	timesync_sync_base(TIMESYNC_FLAG_SYNC);
}

static u64 get_ts_max_nsecs(u32 mult, u32 shift, u64 mask)
{
	u64 max_nsecs, max_cycles;

	max_cycles = ULLONG_MAX;
	do_div(max_cycles, mult);
	max_cycles = min(max_cycles, mask);
	max_nsecs = clocksource_cyc2ns(max_cycles, mult, shift);
	/* Return 50% of the actual maximum, so we can detect bad values */
	max_nsecs >>= 1;
	return max_nsecs;
}

unsigned int gpueb_timesync_init(void)
{
	u64 wrap;

	g_gpueb_ts_mbox = gpueb_get_ts_mbox();
	gpueb_log_i(GPUEB_TAG, "g_gpueb_ts_mbox = %d", g_gpueb_ts_mbox);
	g_gpueb_ts_mbox_offset_base = gpueb_get_send_PIN_offset_by_name("IPI_ID_TIMER");
	gpueb_log_i(GPUEB_TAG, "g_gpueb_ts_mbox_offset_base = %d", g_gpueb_ts_mbox_offset_base);

	timesync_workqueue = create_workqueue("gpueb_ts_wq");
	if (!timesync_workqueue) {
		gpueb_log_i(GPUEB_TAG, "workqueue create failed");
		timesync_ctx.enabled = 0;
		return -1;
	}

	INIT_WORK(&(timesync_ctx.work), timesync_ws);

	spin_lock_init(&timesync_ctx.lock);

	/* init cyclecounter mult and shift as sched_clock */
	clocks_calc_mult_shift(&timesync_cc.mult, &timesync_cc.shift,
				arch_timer_get_cntfrq(), NSEC_PER_SEC, 3600);

	wrap = get_ts_max_nsecs(timesync_cc.mult, timesync_cc.shift,
				timesync_cc.mask);
	timesync_ctx.wrap_kt = ns_to_ktime(wrap);

	/* Init time counter:
	 * start_time: current sched_clock
	 * read: arch timer counter
	 */
	timecounter_init(&timesync_counter, &timesync_cc, sched_clock());

	hrtimer_init(&timesync_refresh_timer,
				CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timesync_refresh_timer.function = timesync_refresh;
	hrtimer_start(&timesync_refresh_timer, timesync_ctx.wrap_kt, HRTIMER_MODE_REL);

	gpueb_log_i(GPUEB_TAG, "%s ts: cycle_last %lld, time_base:%lld, wrap:%lld",
		TIMESYNC_TAG, timesync_counter.cycle_last,
		timesync_counter.nsec, wrap);

	timesync_ctx.enabled = 1;

	timesync_sync_base(TIMESYNC_FLAG_SYNC);

	return 0;
}

void gpueb_timesync_suspend(void)
{
	if (!timesync_ctx.enabled)
		return;

	hrtimer_cancel(&timesync_refresh_timer);

	/* snchronize new sched_clock base to co-processors */
	timesync_sync_base(TIMESYNC_FLAG_SYNC |
		TIMESYNC_FLAG_FREEZE);
}

void gpueb_timesync_resume(void)
{
	if (!timesync_ctx.enabled)
		return;

	/* re-init timecounter because sched_clock will be stopped during
	 * suspend but arch timer counter is not, so we need to update
	 *  start time after resume
	 */
	timecounter_init(&timesync_counter, &timesync_cc, sched_clock());

	hrtimer_start(&timesync_refresh_timer,
		timesync_ctx.wrap_kt, HRTIMER_MODE_REL);

	/* snchronize new sched_clock base to co-processors */
	timesync_sync_base(TIMESYNC_FLAG_SYNC |
		TIMESYNC_FLAG_UNFREEZE);
}

