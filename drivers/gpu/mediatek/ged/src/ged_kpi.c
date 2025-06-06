// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/semaphore.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/version.h>
#include <ged_kpi.h>
#include <ged_base.h>
#include <ged_hashtable.h>
#include <ged_dvfs.h>
#include <ged_log.h>
#include <ged_tracepoint.h>
#include <ged.h>
#include "ged_thread.h"
/* #include <ged_vsync.h> */

#include <mt-plat/mtk_gpu_utility.h>

#if defined(CONFIG_MTK_GPUFREQ_V2)
#include <ged_gpufreq_v2.h>
#include <gpufreq_v2.h>
#else
#include <ged_gpufreq_v1.h>
#endif /* CONFIG_MTK_GPUFREQ_V2 */

#ifdef GED_KPI_MET_DEBUG
#include <mt-plat/met_drv.h>
#endif /* GED_KPI_MET_DEBUG */

#include <linux/sync_file.h>
#include <linux/dma-fence.h>

#include <ged_notify_sw_vsync.h>

#ifdef MTK_CPUFREQ
#include "mtk_cpufreq_common_api.h"
#endif /* MTK_CPUFREQ */

#include "ged_global.h"
#include "ged_eb.h"

#if defined(MTK_GPU_BM_2)
#include <ged_gpu_bm.h>
#endif /* MTK_GPU_BM_2 */
#include <ged_dcs.h>

#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
#include "mtk_drm_arr.h"
#endif

#ifdef MTK_GED_KPI

#define GED_KPI_TAG "[GED_KPI]"
#define GED_PR_DEBUG(fmt, args...)\
	pr_debug(GED_KPI_TAG"%s %d : "fmt, __func__, __LINE__, ##args)

#define GED_KPI_SEC_DIVIDER 1000000000
#define GED_KPI_MAX_FPS 60
/* set default margin to be distinct from FPSGO(0 or 3) */
#define GED_KPI_DEFAULT_FPS_MARGIN 4
#define GED_KPI_CPU_MAX_OPP 0
#define GED_KPI_FPS_LIMIT 165

#define GED_TIMESTAMP_TYPE_D       0x1
#define GED_TIMESTAMP_TYPE_1       0x2
#define GED_TIMESTAMP_TYPE_2       0x4
#define GED_TIMESTAMP_TYPE_S       0x8
#define GED_TIMESTAMP_TYPE_P       0x10
#define GED_TIMESTAMP_TYPE_H       0x20
#define GED_SET_TARGET_FPS         0x40
#define GED_SET_PANEL_REFRESH_RATE 0x80
#define GED_SET_VSYNC_TARGET_FPS   0x100
#define GED_TIMESTAMP_TYPE         int
#define GED_API_FPS_HINT           -100

/* No frame control is applied */
#define GED_KPI_FRC_DEFAULT_MODE    0
#define GED_KPI_FRC_FRR_MODE        1
#define GED_KPI_FRC_ARR_MODE        2
#define GED_KPI_FRC_SW_VSYNC_MODE   3
#define GED_KPI_FRC_MODE_TYPE       int

/* overdue parameter*/
#define t_gpu_store_count 4

/* for dcs reference frame count */
#define t_gpu_w_store_count 100

#define GED_FPSGO_UX_BQ_ID 5566
#define SHADER_CORE_NUM 2

static int g_max_core_num;

#define GPU_SOC_TIMER_SUPPORT  1

enum ged_gpu_fps_level {
	GED_FPS_LEVEL_0,
	GED_FPS_LEVEL_1,
	GED_FPS_LEVEL_2,
	GED_FPS_LEVEL_3,
	GED_FPS_LEVEL_4,
	GED_FPS_LEVEL_5,
	GED_FPS_CONFIG_NUM
};

#define REFRESH_DIFF           0 // deprecated for limited diff fps with refresh rate
#define HIGH_REFRESH           0 // deprecated for limited with high refresh rate
#define GPU_FPS_START_MASK     0x000000FF
#define GPU_FPS_END_MASK       0x0000FF00
#define GPU_FPS_MARGIN_MASK    0x00FF0000
#define GPU_FPS_DEFAULT_MARGIN 110
#define GPU_FPS_DEFAULT_START  GED_FPS_LEVEL_1
#define GPU_FPS_DEFAULT_END    (GED_FPS_CONFIG_NUM - 2)

unsigned int g_gpu_fps_enable = 1;
unsigned int g_gpu_fps_margin = GPU_FPS_DEFAULT_MARGIN;
unsigned int g_gpu_fps_start_level = GPU_FPS_DEFAULT_START;
unsigned int g_gpu_fps_end_level = GPU_FPS_DEFAULT_END;
unsigned int ignore_fpsgo_enable;

struct ged_gpu_fps_table {
	unsigned int fps;
	unsigned int fps_margin;
	unsigned int target;
};

static struct ged_gpu_fps_table g_ged_gpu_fps[GED_FPS_CONFIG_NUM] = {
	{ 10, 11, 100000000},
	{ 30, 33, 33200000},
	{ 60, 66, 16500000},
	{ 90, 99, 11000000},
	{120, 132, 8300000},
	{144, 144, 6940000},
};

/* small frame parameter */
#define  GPU_MAX_FPS 28
#define  GPU_MIN_FPS 13
#define  V_TARGET_FPS_TH 60
#define  TARGET_FPS_GAP 30
#define  GPU_TINY_FPS_THRESHOLD 10
#define  T_GPU_REAL_PIPE_DIFF_RATIO 10

struct GED_KPI_HEAD {
	int pid;
	int i32Count;   // number of KPI object still in the KPI pool
	unsigned long long ullWnd;   // BQ ID
	unsigned long long last_TimeStampD;   // recent dequeue timestamp
	unsigned long long last_TimeStamp1;   // recent queue timestamp
	unsigned long long last_TimeStamp2;   // recent GPU done timestamp
	unsigned long long last_TimeStampS;   // recent acquire timestamp
	unsigned long long last_TimeStampH;   // recent HWVSYNC timestamp
	unsigned long long last_TimeStampP;   // recent prefence timestamp
	unsigned long long pre_TimeStamp2;   // previous GPU done timestamp
	unsigned long long last_TimeStampD_soc_timer;   // recent dequeue soc timer
	unsigned long long last_TimeStamp1_soc_timer;   // recent queue soc timer
	unsigned long long last_TimeStampP_soc_timer;   // recent prefence soc timer
	long long t_cpu_remained;
	long long t_gpu_remained;
	long long t_cpu_latest;
	long long t_gpu_latest;   // recent completed GPU pipe time
	long long t_gpu_latest_real; // recent completed GPU real time
	long long t_gpu_latest_store[t_gpu_store_count];   //store 4 completed GPU time
	int t_gpu_latest_index;   //store array index
	long long t_gpu_w_latest_store[t_gpu_w_store_count];   //store 100 weighted completed GPU time
	unsigned int non_dcs_latest_store[t_gpu_w_store_count];   //store no dcs
	unsigned int non_dcs_cnt;   //store no dcs
	int t_gpu_w_latest_index;
	unsigned int for_dcs_valid_cnt;
	long long t_gpu_latest_uncompleted;   // largest uncomplete GPU time
	int uncomplete_source;
	struct list_head sList;
	spinlock_t sListLock;
	int i32QedBuffer_length;
	int i32Gpu_uncompleted;
	int i32Gpu_uncompleted_queue;		//calculate uncomplete count from queue
	unsigned int gpu_completed_count;
	int i32DebugQedBuffer_length;
	int isSF;
	int isFRR_enabled;
	int isARR_enabled;

	int target_fps;   // -1 means target FPS via FPSGO is invalid or unprovided
	int idel_target_fps;
	int target_fps_v;	// -1 means vsync target FPS via free vsynv is invalid or unprovided
	int target_fps_margin;
	int eara_fps_margin;

	// per BQ fps
	unsigned long long ullElapsed_time_per_sec;
	unsigned long long ullPreTimeStampS;
	unsigned int frame_count;
	int t_gpu_fps;
	int t_use_gpu_fps;
	int t_last_gpu_fps;
	int candidate_fps;
	int candidate_fps_cnt;

	int t_cpu_target;
	int t_gpu_target;
	int t_cpu_fpsgo;
	int gpu_done_interval;

	GED_KPI_FRC_MODE_TYPE frc_mode;
	int frc_client;
	unsigned long long last_QedBufferDelay;
};

struct GED_CPU_INFO {
	unsigned long cpu_max_freq_LL;
	unsigned long cpu_max_freq_L;
	unsigned long cpu_max_freq_B;
	unsigned long cpu_cur_freq_LL;
	unsigned long cpu_cur_freq_L;
	unsigned long cpu_cur_freq_B;
	unsigned int cpu_cur_avg_load_LL;
	unsigned int cpu_cur_avg_load_L;
	unsigned int cpu_cur_avg_load_B;
};

struct GED_GPU_INFO {
	unsigned long gpu_dvfs;
	/* bit0~bit9: headroom ratio:10-bias */
	/* bit15: is frame base? */
	/* bit16~bit23: dvfs_margin_mode */
	unsigned long tb_dvfs_mode;
	unsigned long tb_dvfs_margin;
	unsigned long t_gpu_real;
	unsigned long limit_upper;
	unsigned long limit_lower;
	unsigned int dvfs_loading_mode;
	unsigned int gpu_util;
	unsigned int gpu_power;
	int gpu_freq_target;
};

union _cpu_gpu_info {
	struct GED_CPU_INFO cpu;
	struct GED_GPU_INFO gpu;
};

struct GED_KPI {
	int pid;
	unsigned long long ullWnd;
	unsigned long i32DeQueueID;
	unsigned long i32QueueID;
	unsigned long  i32AcquireID;
	unsigned long ulMask;
	unsigned long frame_attr;
	unsigned long long ullTimeStampD;   // dequeue timestamp
	unsigned long long ullTimeStamp1;   // queue timestamp
	unsigned long long ullTimeStamp2;   // acquire fence signaled timestamp
	unsigned long long ullTimeStampP;   // release fence signaled timestamp
	unsigned long long ullTimeStampS;   // (* set to queue) acquire timestamp
	unsigned long long ullTimeStampH;   // (X) HWVSYNC timestamp
	unsigned int gpu_freq; /* in MHz*/
	unsigned int gpu_freq_max; /* in MHz*/
	unsigned int gpu_loading;
	struct list_head sList;
	long long t_cpu_remained;
	long long t_gpu_remained;
	int i32QedBuffer_length;
	int i32Gpu_uncompleted;
	int i32DebugQedBuffer_length;
	int boost_linear_cpu;
	int boost_real_cpu;
	int boost_accum_gpu;
	long long t_cpu_remained_pred;
	unsigned long long t_acquire_period;
	unsigned long long QedBufferDelay;
	union _cpu_gpu_info cpu_gpu_info;
	long long t_cpu;
	long long t_gpu;
	int t_cpu_target;
	int t_gpu_target;
	int t_cpu_fpsgo;
	int gpu_done_interval;
	int target_fps_margin;
	int eara_fps_margin;
	int isSF;

	unsigned long long t_cpu_slptime;
};

struct GED_TIMESTAMP {
	GED_TIMESTAMP_TYPE eTimeStampType;
	int pid;
	unsigned long long ullWnd;
	unsigned long i32FrameID;
	unsigned long long ullTimeStamp;
	unsigned int i32GPUloading;
	int i32QedBuffer_length;
	int isSF;
	struct work_struct sWork;
	void *fence_addr;
};

struct GED_KPI_GPU_TS {
	int pid;
	u64 ullWdnd;
	unsigned long i32FrameID;
	struct dma_fence_cb sSyncWaiter;
	struct dma_fence *psSyncFence;
	struct work_struct sWork;
} GED_KPI_GPU_TS;

struct GED_KPI_MEOW_DVFS_FREQ_PRED {
	int gpu_freq_cur;
	int gpu_freq_max;
	int gpu_freq_pred;
	int gift_ratio;

	int target_pid;
	int target_fps;
	int target_fps_margin;
	int eara_fps_margin;
	int gpu_time;
};

static struct GED_KPI_MEOW_DVFS_FREQ_PRED *g_psGIFT;
static unsigned int fb_timer_set_count;


int g_target_fps = GED_KPI_MAX_FPS;
int g_target_fps_default = GED_KPI_MAX_FPS;
int g_gpu_target_default = 16666666;
bool g_set_panel_refresh_rate;
bool g_invalid_fps;

#define GED_KPI_TOTAL_ITEMS 32
#define GED_KPI_UID(pid, wnd) (pid | ((unsigned long)wnd))
#define SCREEN_IDLE_PERIOD 500000000
#define GED_KPI_SWITCH_FB_THRESHOLD 3
#define GED_KPI_SWITCH_LB_THRESHOLD 0
#define GED_KPI_MAX_SWITCH_COUNT 8
/* static int display_fps = GED_KPI_MAX_FPS; */
static long long vsync_period = GED_KPI_SEC_DIVIDER / GED_KPI_MAX_FPS;
static GED_LOG_BUF_HANDLE ghLogBuf_KPI;
static struct workqueue_struct *g_psWorkQueue;
static struct workqueue_struct *g_FenceWorkQueue;

static GED_HASHTABLE_HANDLE gs_hashtable;

static spinlock_t gs_hashtableLock;

static struct GED_KPI g_asKPI[GED_KPI_TOTAL_ITEMS];
static int g_i32Pos;
static GED_THREAD_HANDLE ghThread;
static unsigned long long frame_base_timestamp;
static unsigned long frame_base_ulMask;

#if !defined(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT)
/* Disable for bring-up stage unexpected exception */
static unsigned int is_GED_KPI_enabled;
#else
static unsigned int is_GED_KPI_enabled = 1;
#endif

static unsigned int g_force_gpu_dvfs_fallback;
#define API_SYNC_TIMEOUT_MS   50

#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
#define FW_IDLE_TIMER_0_MS    0
#define FW_IDLE_TIMER_5_MS    5
#define FW_IDLE_TIMER_10_MS   10

#define FW_IDLE_MODE_DEFAULT  0
#define FW_IDLE_MODE_FIX_10   2
#define FW_IDLE_MODE_FIX_5    1
#define FW_IDLE_MODE_Fix_0    3

#define FW_IDLE_FPS_THRESHOLD 60

/* -1:default flavor
 *  0:disable
 *  1:enable
 */
static int g_is_fw_idle_enable = -1;

static int g_fw_idle_mode;
static int g_fw_idle_timer;
static int g_is_panel_hz_change;
#endif /* MTK_GPU_FW_IDLE */

struct ged_head_record {
	u64 ulID[1024];
	int head_num;
};

static struct ged_head_record g_ged_head_record;

struct ged_eb_dvfs_last_record {
	u64 ulID;
	u64 last_uncomplete_soc_timer;
	u64 last_uncomplete_target;
	u32 last_uncomplete_type;
};
static struct ged_eb_dvfs_last_record g_ged_eb_uncomplete_info;

u64 fb_timeout = 100000000;/*100 ms*/
u64 lb_timeout = 100000000;
u64 t_gpu_target_now;

bool g_force_disable_dcs;

module_param(is_GED_KPI_enabled, uint, 0644);

/* for calculating remained time budgets of CPU and GPU:
 *		time budget: the buffering time that prevents fram drop
 */
struct GED_KPI_HEAD *main_head;
struct GED_KPI_HEAD *prev_main_head;
struct GED_KPI_HEAD *last_ux_head;
static bool g_is_ux_fps_set;
static int is_loading_based;
static bool g_is_multiproducer;
unsigned int force_loading_based_enable;

/* end */

/* for calculating KPI info per second */
static unsigned long long g_pre_TimeStamp1;
static unsigned long long g_pre_TimeStamp2;
static unsigned long long g_pre_TimeStampS;
static unsigned long long g_elapsed_time_per_sec;
static unsigned long long g_cpu_time_accum;
static unsigned long long g_gpu_time_accum;
static unsigned long long g_ResponseTimeAccu;
static unsigned long long g_GRemTimeAccu; /*g_gpu_remained_time_accum*/
static unsigned long long g_CRemTimeAccu; /*g_cpu_remained_time_accum*/
static unsigned long long g_gpu_freq_accum;
static unsigned int g_frame_count;
static unsigned int g_effective_frame_count;

static unsigned int gx_fps;
static unsigned int gx_cpu_time_avg;
static unsigned int gx_gpu_time_avg;
static unsigned int gx_response_time_avg;
static unsigned int gx_gpu_remained_time_avg;
static unsigned int gx_cpu_remained_time_avg;
static unsigned int gx_gpu_freq_avg;

unsigned int g_eb_workload;
unsigned int g_eb_coef;

int pid_sysui;
int pid_sf;
static u64 eb_ullTimeStamp;

/* LB async ratio */
#define POLICY_FRAME_NUM 10
static int g_policy_frame_his[POLICY_FRAME_NUM];
static int g_policy_his_idx;
static bool g_stable_lb;

/* ------------------------------------------------------------------- */
void (*ged_kpi_output_gfx_info2_fp)(long long t_gpu, unsigned int cur_freq
	, unsigned int cur_max_freq, u64 ulID);
EXPORT_SYMBOL(ged_kpi_output_gfx_info2_fp);

static void ged_kpi_output_gfx_info2(long long t_gpu, unsigned int cur_freq
	, unsigned int cur_max_freq, u64 ulID)
{
	if (ged_kpi_output_gfx_info2_fp)
		ged_kpi_output_gfx_info2_fp(t_gpu, cur_freq
						, cur_max_freq, ulID);
}

/* ------------------------------------------------------------------- */
int (*ged_kpi_gpu_dvfs_fp)(int t_gpu, int t_gpu_target, int target_fps_margin,
	unsigned int force_fallback);

static int ged_kpi_gpu_dvfs(int t_gpu, int t_gpu_target, int target_fps_margin,
	unsigned int force_fallback)
{
	if (ged_kpi_gpu_dvfs_fp)
		return ged_kpi_gpu_dvfs_fp(t_gpu, t_gpu_target, target_fps_margin,
			force_fallback);

	return 0;
}
EXPORT_SYMBOL(ged_kpi_gpu_dvfs_fp);

/* ------------------------------------------------------------------- */
void (*ged_kpi_trigger_fb_dvfs_fp)(void);

void ged_kpi_trigger_fb_dvfs(void)
{
	if (ged_kpi_trigger_fb_dvfs_fp)
		ged_kpi_trigger_fb_dvfs_fp();
}
EXPORT_SYMBOL(ged_kpi_trigger_fb_dvfs_fp);
/* ------------------------------------------------------------------- */
int (*ged_kpi_check_if_fallback_mode_fp)(void);

int ged_kpi_check_if_fallback_mode(void)
{
	if (ged_kpi_check_if_fallback_mode_fp)
		return ged_kpi_check_if_fallback_mode_fp();
	else
		return 1;
}
EXPORT_SYMBOL(ged_kpi_check_if_fallback_mode_fp);

/* ------------------------------------------------------------------- */
void (*ged_kpi_fastdvfs_update_dcs_fp)(void);
void ged_kpi_fastdvfs_update_dcs(void)
{
	if (ged_kpi_fastdvfs_update_dcs_fp)
		ged_kpi_fastdvfs_update_dcs_fp();
}
EXPORT_SYMBOL(ged_kpi_fastdvfs_update_dcs_fp);

/* ------------------------------------------------------------------- */
static inline void ged_kpi_clean_kpi_info(void)
{
	g_frame_count = 0;
	g_effective_frame_count = 0;
	g_elapsed_time_per_sec = 0;
	g_cpu_time_accum = 0;
	g_gpu_time_accum = 0;
	g_ResponseTimeAccu = 0;
	g_GRemTimeAccu = 0;
	g_CRemTimeAccu = 0;
	g_gpu_freq_accum = 0;
}

/* ------------------------------------------------------------------- */

static inline void update_target_by_gpu(struct GED_KPI_HEAD *psHead,
	unsigned int lower_level, unsigned int upper_level)
{
	if(upper_level >= GED_FPS_CONFIG_NUM || lower_level >= GED_FPS_CONFIG_NUM)
		return;

	if (psHead->t_gpu_fps < g_ged_gpu_fps[upper_level].fps_margin &&
		psHead->t_gpu_fps >= g_ged_gpu_fps[lower_level].fps_margin) {
		psHead->t_gpu_target = g_ged_gpu_fps[upper_level].target;
		trace_GPU_DVFS__Policy__Common__Check_Target(psHead->pid,
			(int)(psHead->ullWnd % 0xF), psHead->t_gpu_fps, (upper_level << 8) + lower_level);
		psHead->t_use_gpu_fps = 1;
	}
}

static inline void check_refresh_diff(struct GED_KPI_HEAD *psHead)
{
	if (psHead != NULL) {
		int pre_t_use_gpu_fps = psHead->t_use_gpu_fps;

		//replace target fps if api fps is available
		if (psHead->target_fps_v) {	// api target fps
			psHead->t_cpu_target = (int)((int)GED_KPI_SEC_DIVIDER/psHead->target_fps_v);
			psHead->t_gpu_target = psHead->t_cpu_target;
			trace_tracing_mark_write(5566, "target_fps_v", psHead->target_fps_v);
			return;
		}

		if (psHead->target_fps_margin != GED_KPI_DEFAULT_FPS_MARGIN)
			return;

		psHead->t_use_gpu_fps = 0;
		if (g_gpu_fps_start_level == 0 && pre_t_use_gpu_fps) {
			psHead->t_gpu_target = g_gpu_target_default;
			 trace_GPU_DVFS__Policy__Common__Check_Target(psHead->pid,
			(int)(psHead->ullWnd % 0xF), psHead->t_gpu_fps, 0);
			return;
		}

		if (g_target_fps_default < HIGH_REFRESH) {
			trace_GPU_DVFS__Policy__Common__Check_Target(psHead->pid,
				(int)(psHead->ullWnd % 0xF), psHead->t_gpu_fps, 1);
			psHead->t_gpu_target = g_gpu_target_default;
			return;
		}
		if (g_target_fps_default - psHead->t_gpu_fps < REFRESH_DIFF) {
			trace_GPU_DVFS__Policy__Common__Check_Target(psHead->pid,
				(int)(psHead->ullWnd % 0xF), psHead->t_gpu_fps, 2);
			psHead->t_gpu_target = g_gpu_target_default;
			return;
		}
		if (psHead->t_gpu_fps < g_ged_gpu_fps[GED_FPS_LEVEL_0].fps_margin) {
			// keep use previous fps
			if (psHead->t_gpu_target < g_gpu_target_default) {
				psHead->t_gpu_target = g_gpu_target_default;
				psHead->t_use_gpu_fps = 0;
				trace_GPU_DVFS__Policy__Common__Check_Target(psHead->pid,
				(int)(psHead->ullWnd % 0xF), psHead->t_gpu_fps, 3);
			} else {
				psHead->t_use_gpu_fps = pre_t_use_gpu_fps;
				trace_GPU_DVFS__Policy__Common__Check_Target(psHead->pid,
				(int)(psHead->ullWnd % 0xF), psHead->t_gpu_fps, 4);
			}

			return;
		}

		if (g_invalid_fps) {
			trace_GPU_DVFS__Policy__Common__Check_Target(psHead->pid,
				(int)(psHead->ullWnd % 0xF), psHead->t_gpu_fps, 6);
			g_invalid_fps = false;
			return;
		}

		update_target_by_gpu(psHead, GED_FPS_LEVEL_0, g_gpu_fps_start_level);
		for (int i = g_gpu_fps_start_level; i < g_gpu_fps_end_level; i++)
			update_target_by_gpu(psHead, i, i + 1);

		if (psHead->t_use_gpu_fps == 0) {
			psHead->t_gpu_target = g_gpu_target_default;
			trace_GPU_DVFS__Policy__Common__Check_Target(psHead->pid,
				(int)(psHead->ullWnd % 0xF), psHead->t_gpu_fps, 5);
		}

	}
}

void update_fb_timer_set_count(void)
{
	fb_timer_set_count++;
	if (fb_timer_set_count >= 0xFFFFFFFF)
		fb_timer_set_count = 0;

	mtk_gpueb_sysram_write(SYSRAM_GPU_FB_FALLBACK_RESET_COUNT, fb_timer_set_count);
}

void reset_fb_timer_set_count(void)
{
	fb_timer_set_count = 0;
	mtk_gpueb_sysram_write(SYSRAM_GPU_FB_FALLBACK_RESET_COUNT, fb_timer_set_count);
}

void update_gpu_fps_table(int i32MarginValue)
{
	/*
	 * value == 0: default, GED_DVFS_TIMER_BASED_DVFS_MARGIN
	 * bit   7~0: level start
	 * bit   15~8: level end
	 * bit   23~16: margin
	 */
	unsigned int start_level = i32MarginValue & GPU_FPS_START_MASK;
	unsigned int end_level = (i32MarginValue & GPU_FPS_END_MASK) >> 8;
	unsigned int margin = (i32MarginValue & GPU_FPS_MARGIN_MASK) >> 16;

	g_gpu_fps_enable = i32MarginValue;

	if (i32MarginValue != 1) {
		g_gpu_fps_start_level = (start_level < GED_FPS_CONFIG_NUM - 1) ?
								start_level : GPU_FPS_DEFAULT_START;
		g_gpu_fps_end_level = (end_level < GED_FPS_CONFIG_NUM - 1) ?
							end_level : GPU_FPS_DEFAULT_END;
		g_gpu_fps_margin = (margin > 0 && margin < 200) ?
							margin : GPU_FPS_DEFAULT_MARGIN;
	} else {
		g_gpu_fps_start_level = GPU_FPS_DEFAULT_START;
		g_gpu_fps_end_level = GPU_FPS_DEFAULT_END;
		g_gpu_fps_margin = GPU_FPS_DEFAULT_MARGIN;
	}

	for (int i = 0; i < GED_FPS_CONFIG_NUM - 1; i++) {
		g_ged_gpu_fps[i].fps_margin = g_ged_gpu_fps[i].fps * g_gpu_fps_margin / 100;
		GED_LOGI("%d:%u, %u, %u",
			i, g_ged_gpu_fps[i].fps, g_ged_gpu_fps[i].fps_margin, g_ged_gpu_fps[i].target);
	}
	GED_LOGI("%u, %u, %u",
		g_gpu_fps_start_level,g_gpu_fps_end_level, g_gpu_fps_margin);
}

unsigned int dump_gpu_fps_table(void)
{
	for (int i = 0; i < GED_FPS_CONFIG_NUM; i++){
		GED_LOGI("%d:%u, %u, %u",
			i, g_ged_gpu_fps[i].fps, g_ged_gpu_fps[i].fps_margin, g_ged_gpu_fps[i].target);
	}
	GED_LOGI("%u, %u, %u",
			g_gpu_fps_start_level,g_gpu_fps_end_level, g_gpu_fps_margin);
	return g_gpu_fps_enable;
}

static inline void update_by_internal_fps(struct GED_KPI_HEAD *psHead, struct GED_KPI *psKPI)
{
	if (!psHead || !psKPI)
		return;

	static unsigned int fps_margin;
	unsigned long long ullTimeStampS_diff;

	ullTimeStampS_diff = psKPI->ullTimeStampS - psHead->ullPreTimeStampS;

	if (g_set_panel_refresh_rate) {
		psHead->ullElapsed_time_per_sec = 0;
		psHead->frame_count = 0;
		g_set_panel_refresh_rate = false;
	} else {
		psHead->ullElapsed_time_per_sec += ullTimeStampS_diff;
		psHead->frame_count++;
	}

	psHead->ullPreTimeStampS = psKPI->ullTimeStampS;

	if (psHead->ullElapsed_time_per_sec >= GED_KPI_SEC_DIVIDER &&
		psHead->frame_count > 0) {
		unsigned long long fps;

		fps = psHead->frame_count;
		fps *= GED_KPI_SEC_DIVIDER;
		do_div(fps, psHead->ullElapsed_time_per_sec);
		psHead->t_gpu_fps = fps;

		for (int i = 1; i < GED_FPS_CONFIG_NUM; i++) {
			if (g_target_fps_default == g_ged_gpu_fps[i].fps) {
				fps_margin = g_ged_gpu_fps[i-1].fps_margin;
				break;
			}
		}

		if (psHead->t_gpu_fps < fps_margin && psHead->t_gpu_fps < psHead->t_last_gpu_fps) {
			g_invalid_fps = true;
			if (psHead->t_gpu_fps != psHead->candidate_fps ) {
				psHead->candidate_fps = psHead->t_gpu_fps;
				psHead->candidate_fps_cnt = 1;
			} else
				psHead->candidate_fps_cnt++;
			if (psHead->candidate_fps_cnt == 3) {
				g_invalid_fps = false;
				psHead->candidate_fps_cnt = 0;
			}
		}

		if (g_invalid_fps != true) {
			psHead->t_last_gpu_fps = psHead->t_gpu_fps;
			psHead->candidate_fps_cnt = 0;
		}


		psHead->frame_count = 0;
		psHead->ullElapsed_time_per_sec = 0;
		check_refresh_diff(psHead);
	}
}

/* ------------------------------------------------------------------- */
/* for calculating average per-second performance info */
/* ------------------------------------------------------------------- */
static inline void ged_kpi_calc_kpi_info(u64 ulID, struct GED_KPI *psKPI
	, struct GED_KPI_HEAD *psHead)
{
	unsigned int cur_minfreq;
	/* check if there is a main rendering thread */
	/* only SF is excluded from the group of considered candidates */
	if (main_head) {
		GED_LOGD("main_head = %p, i32Count= %d, i32QedBuffer_length=%d",
			main_head, main_head->i32Count, main_head->i32QedBuffer_length);
	}
	else
		GED_LOGD("main_head = NULL");

	update_by_internal_fps(psHead, psKPI);

	if (main_head != prev_main_head && main_head == psHead) {
		ged_kpi_clean_kpi_info();
		g_pre_TimeStamp1 = psKPI->ullTimeStamp1;
		g_pre_TimeStamp2 = psKPI->ullTimeStamp2;
		g_pre_TimeStampS = psKPI->ullTimeStampS;

		prev_main_head = main_head;
		return;
	}

	if (psHead == main_head) {
		g_elapsed_time_per_sec += psKPI->ullTimeStampS - g_pre_TimeStampS;
		g_gpu_time_accum += psKPI->t_gpu;
		g_CRemTimeAccu += psKPI->ullTimeStampS - psKPI->ullTimeStamp1;
		g_cpu_time_accum += psKPI->ullTimeStamp1 - g_pre_TimeStamp1;
		g_frame_count++;
		cur_minfreq = ged_get_freq_by_idx(ged_get_min_oppidx());  /* in KHz*/

		if (psKPI->gpu_freq >= cur_minfreq / 1000) {
			g_gpu_freq_accum += psKPI->gpu_freq;
			g_effective_frame_count++;
		}

		g_pre_TimeStamp1 = psKPI->ullTimeStamp1;
		g_pre_TimeStamp2 = psKPI->ullTimeStamp2;
		g_pre_TimeStampS = psKPI->ullTimeStampS;

		if (g_elapsed_time_per_sec >= GED_KPI_SEC_DIVIDER &&
			g_frame_count > 0 && g_effective_frame_count > 0) {
			unsigned long long g_fps;

			g_fps = g_frame_count;
			g_fps *= GED_KPI_SEC_DIVIDER;
			do_div(g_fps, g_elapsed_time_per_sec);
			gx_fps = g_fps;

			do_div(g_cpu_time_accum, g_frame_count);
			gx_cpu_time_avg = (unsigned int)g_cpu_time_accum;

			do_div(g_gpu_time_accum, g_frame_count);
			gx_gpu_time_avg = (unsigned int)g_gpu_time_accum;

			do_div(g_ResponseTimeAccu, g_frame_count);
			gx_response_time_avg = (unsigned int)g_ResponseTimeAccu;

			do_div(g_GRemTimeAccu, g_frame_count);
			gx_gpu_remained_time_avg = (unsigned int)g_GRemTimeAccu;

			do_div(g_CRemTimeAccu, g_frame_count);
			gx_cpu_remained_time_avg = (unsigned int)g_CRemTimeAccu;

			do_div(g_gpu_freq_accum, g_effective_frame_count);
			gx_gpu_freq_avg = g_gpu_freq_accum;

			ged_kpi_clean_kpi_info();
		}
	}
}
/* ------------------------------------------------------------------- */
#define GED_KPI_IS_SF_SHIFT 0
#define GED_KPI_IS_SF_MASK 0x1
#define GED_KPI_QBUFFER_LEN_SHIFT 1
#define GED_KPI_QBUFFER_LEN_MASK 0x7
#define GED_KPI_DEBUG_QBUFFER_LEN_SHIFT 4
#define GED_KPI_DEBUG_QBUFFER_LEN_MASK 0x7
#define GED_KPI_GPU_UNCOMPLETED_LEN_SHIFT 7
#define GED_KPI_GPU_UNCOMPLETED_LEN_MASK 0x7
#define GED_KPI_FRC_MODE_SHIFT 10
#define GED_KPI_FRC_MODE_MASK 0x7
#define GED_KPI_FRC_CLIENT_SHIFT 13
#define GED_KPI_FRC_CLIENT_MASK 0xF
#define GED_KPI_GPU_FREQ_MAX_INFO_SHIFT 19
#define GED_KPI_GPU_FREQ_MAX_INFO_MASK 0xFFF /* max @ 4096 MHz */
#define GED_KPI_GPU_FREQ_INFO_SHIFT 7
#define GED_KPI_GPU_FREQ_INFO_MASK 0xFFF /* max @ 4096 MHz */
#define GED_KPI_GPU_LOADING_INFO_SHIFT 0
#define GED_KPI_GPU_LOADING_INFO_MASK 0x7F
static void ged_kpi_statistics_and_remove(struct GED_KPI_HEAD *psHead,
	struct GED_KPI *psKPI)
{
	u64 ulID = psKPI->ullWnd;
	unsigned long frame_attr = 0;
	unsigned long gpu_info = 0;

	struct GpuUtilization_Ex util_ex;
	unsigned int dvfs_loading_mode = 0;

	memset(&util_ex, 0, sizeof(util_ex));

	ged_kpi_calc_kpi_info(ulID, psKPI, psHead);
	frame_attr |= ((psHead->isSF & GED_KPI_IS_SF_MASK)
			<< GED_KPI_IS_SF_SHIFT);
	frame_attr |= ((psKPI->i32QedBuffer_length & GED_KPI_QBUFFER_LEN_MASK)
			<< GED_KPI_QBUFFER_LEN_SHIFT);
	frame_attr |= ((psKPI->i32DebugQedBuffer_length
			& GED_KPI_DEBUG_QBUFFER_LEN_MASK)
			<< GED_KPI_DEBUG_QBUFFER_LEN_SHIFT);
	frame_attr |= ((psKPI->i32Gpu_uncompleted
			& GED_KPI_GPU_UNCOMPLETED_LEN_MASK)
			<< GED_KPI_GPU_UNCOMPLETED_LEN_SHIFT);
	frame_attr |= ((psHead->frc_mode & GED_KPI_FRC_MODE_MASK)
			<< GED_KPI_FRC_MODE_SHIFT);
	frame_attr |= ((psHead->frc_client & GED_KPI_FRC_CLIENT_MASK)
			<< GED_KPI_FRC_CLIENT_SHIFT);
	gpu_info |= ((psKPI->gpu_freq & GED_KPI_GPU_FREQ_INFO_MASK)
			<< GED_KPI_GPU_FREQ_INFO_SHIFT);
	gpu_info |= ((psKPI->gpu_loading & GED_KPI_GPU_LOADING_INFO_MASK)
			<< GED_KPI_GPU_LOADING_INFO_SHIFT);
	gpu_info |=
		((psKPI->gpu_freq_max & GED_KPI_GPU_FREQ_MAX_INFO_MASK)
		<< GED_KPI_GPU_FREQ_MAX_INFO_SHIFT);
	psKPI->frame_attr = frame_attr;

	psKPI->cpu_gpu_info.gpu.gpu_power =
		ged_get_power_by_idx(
		ged_get_oppidx_by_freq(psKPI->gpu_freq*1000));

	ged_get_gpu_utli_ex(&util_ex);

	psKPI->cpu_gpu_info.gpu.gpu_util =
		(util_ex.util_active&0xff)|
		((util_ex.util_3d&0xff)<<8)|
		((util_ex.util_ta&0xff)<<16)|
		((util_ex.util_compute&0xff)<<24);

	mtk_get_dvfs_loading_mode(&dvfs_loading_mode);

	psKPI->cpu_gpu_info.gpu.dvfs_loading_mode = dvfs_loading_mode;

	/* statistics */
	ged_log_buf_print(ghLogBuf_KPI,
		"%d,%llu,%lu,%lu,%lu,%llu,%llu,%llu,%llu,%llu,%llu,%lu,%d,%d,%lld,%d,%lld,%d,%lu,%lu,%lu,%d,%lu,%lu,%lu,%u,%u,%u,%d,%d,%d,%d,%d,%d",
		psHead->pid,
		psHead->ullWnd,
		psKPI->i32QueueID,
		psKPI->i32AcquireID,
		psKPI->frame_attr,
		psKPI->ullTimeStampD, // dequeue
		psKPI->ullTimeStamp1, // queue == acquire
		psKPI->ullTimeStamp2, // gpu_done
		psKPI->ullTimeStampP, // pre-fence
		psKPI->ullTimeStampS, // acquire buffer
		psKPI->ullTimeStampH, // legacy: HW Vsync
		gpu_info,
		psKPI->t_cpu_target,
		psKPI->t_cpu_fpsgo,
		psKPI->t_gpu,
		psKPI->gpu_done_interval,
		vsync_period,
		g_psGIFT->gift_ratio,

		psKPI->cpu_gpu_info.gpu.gpu_dvfs,
		psKPI->cpu_gpu_info.gpu.tb_dvfs_mode,
		psKPI->cpu_gpu_info.gpu.tb_dvfs_margin,
		psKPI->cpu_gpu_info.gpu.gpu_freq_target,
		psKPI->cpu_gpu_info.gpu.t_gpu_real,
		psKPI->cpu_gpu_info.gpu.limit_upper,
		psKPI->cpu_gpu_info.gpu.limit_lower,
		psKPI->cpu_gpu_info.gpu.dvfs_loading_mode,
		psKPI->cpu_gpu_info.gpu.gpu_util,
#ifdef GED_DCS_POLICY
		dcs_get_cur_core_num(),
#else
		psKPI->cpu_gpu_info.gpu.gpu_power,
#endif /* GED_DCS_POLICY */
#ifdef MTK_CPUFREQ
		mt_cpufreq_get_cur_freq(0) / 1000,
		mt_cpufreq_get_freq_by_idx(0, GED_KPI_CPU_MAX_OPP) / 1000,
		mt_cpufreq_get_cur_freq_idx(0),
		mt_cpufreq_get_cur_freq(1) / 1000,
		mt_cpufreq_get_freq_by_idx(1, GED_KPI_CPU_MAX_OPP) / 1000,
		mt_cpufreq_get_cur_freq_idx(1)
#else
		0, 0, 0, 0, 0, 0
#endif/* MTK_CPUFREQ */
		);
}

/* ------------------------------------------------------------------- */
static GED_BOOL ged_kpi_tag_type_s(u64 ulID, struct GED_KPI_HEAD *psHead
	, struct GED_TIMESTAMP *psTimeStamp)
{
	struct GED_KPI *psKPI = NULL;
	GED_BOOL ret = GED_FALSE;

	if (psHead) {
		struct list_head *psListEntry, *psListEntryTemp;
		struct list_head *psList = &psHead->sList;

		list_for_each_prev_safe(psListEntry, psListEntryTemp, psList) {
			psKPI = list_entry(psListEntry, struct GED_KPI, sList);
			if (psKPI && !(psKPI->ulMask & GED_TIMESTAMP_TYPE_S)
				&& psTimeStamp->i32FrameID == psKPI->i32QueueID)
				break;
			else
				psKPI = NULL;
		}
		if (psKPI) {
			psKPI->ulMask |= GED_TIMESTAMP_TYPE_S;
			psKPI->ullTimeStampS =
				psTimeStamp->ullTimeStamp;
			psHead->t_cpu_remained =
				psTimeStamp->ullTimeStamp
				- psKPI->ullTimeStamp1;
			psKPI->t_cpu_remained = vsync_period;
			psKPI->t_cpu_remained -=
				(psKPI->ullTimeStamp1
				- psHead->last_TimeStampS);
			psKPI->t_acquire_period =
				psTimeStamp->ullTimeStamp
				- psHead->last_TimeStampS;
			psHead->last_TimeStampS =
				psTimeStamp->ullTimeStamp;
			psKPI->i32AcquireID = psTimeStamp->i32FrameID;
			ret = GED_TRUE;
			if (psKPI && (psKPI->ulMask & GED_TIMESTAMP_TYPE_2))
				ged_kpi_statistics_and_remove(psHead, psKPI);
		} else {
			GED_LOGD("[Exception] TYPE_S: psKPI NULL, frameID: %lu",
				psTimeStamp->i32FrameID);
		}
		return ret;
	} else {
		return GED_FALSE;
	}
}

/* ------------------------------------------------------------------- */
/* For GED_TIMESTAMP_TYPE_H (deprecated)*/
static GED_BOOL ged_kpi_h_iterator_func(unsigned long ulID,
	void *pvoid, void *pvParam)
{
	struct GED_KPI_HEAD *psHead = (struct GED_KPI_HEAD *)pvoid;
	struct GED_TIMESTAMP *psTimeStamp = (struct GED_TIMESTAMP *)pvParam;
	struct GED_KPI *psKPI = NULL;
	struct GED_KPI *psKPI_prev = NULL;

	if (psHead) {
		struct list_head *psListEntry, *psListEntryTemp;
		struct list_head *psList = &psHead->sList;

		list_for_each_prev_safe(psListEntry, psListEntryTemp, psList) {
			psKPI = list_entry(psListEntry, struct GED_KPI, sList);
			if (psKPI) {
				if (psKPI->ulMask & GED_TIMESTAMP_TYPE_H) {
					if (psKPI_prev
					&& (psKPI_prev->ulMask &
					GED_TIMESTAMP_TYPE_S)
					&& (psKPI_prev->ulMask &
					GED_TIMESTAMP_TYPE_2)) {
						psKPI_prev->ulMask |=
						GED_TIMESTAMP_TYPE_H;
						psKPI_prev->ullTimeStampH =
						psTimeStamp->ullTimeStamp;

				/* Not yet precise due uncertain type_H ts*/
						psHead->t_gpu_remained =
						psTimeStamp->ullTimeStamp
						- psKPI_prev->ullTimeStamp2;
				/* fixed value since vsync period unchange */
						psKPI_prev->t_gpu_remained =
						vsync_period;
						psKPI_prev->t_gpu_remained -=
						(psKPI_prev->ullTimeStamp2
						- psHead->last_TimeStampH);

						psHead->last_TimeStampH =
						psTimeStamp->ullTimeStamp;
					}
					break;
				}
			}
			psKPI_prev = psKPI;
		}
		if (psKPI && psKPI == psKPI_prev) {
			/* (0 == (psKPI->ulMask & GED_TIMESTAMP_TYPE_H) */
			if ((psKPI->ulMask & GED_TIMESTAMP_TYPE_S)
				&& (psKPI->ulMask & GED_TIMESTAMP_TYPE_2)) {
				psKPI->ulMask |= GED_TIMESTAMP_TYPE_H;
				psKPI->ullTimeStampH =
					psTimeStamp->ullTimeStamp;

				/* Not yet precise due uncertain type_H ts*/
				psHead->t_gpu_remained =
					psTimeStamp->ullTimeStamp
					- psKPI->ullTimeStamp2;
				/* fixed value since vsync period unchange */
				psKPI->t_gpu_remained = vsync_period;
				psKPI->t_gpu_remained -=
					(psKPI->ullTimeStamp2
					- psHead->last_TimeStampH);

				psHead->last_TimeStampH =
					psTimeStamp->ullTimeStamp;
			}
		}
	}
	return GED_TRUE;
}
/* ------------------------------------------------------------------- */
static GED_BOOL ged_kpi_iterator_delete_func(unsigned long ulID,
	void *pvoid, void *pvParam, GED_BOOL *pbDeleted)
{
	struct GED_KPI_HEAD *psHead = (struct GED_KPI_HEAD *)pvoid;

	if (psHead) {
		ged_free(psHead, sizeof(struct GED_KPI_HEAD));
		*pbDeleted = GED_TRUE;
	}

	return GED_TRUE;
}

static GED_BOOL ged_kpi_update_TargetTimeAndTargetFps(
	struct GED_KPI_HEAD *psHead,
	int target_fps,
	int target_fps_margin,
	int cpu_time,
	int eara_fps_margin,
	GED_KPI_FRC_MODE_TYPE mode,
	int client)
{
	GED_BOOL ret = GED_FALSE;
#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
	int idle_timer_ms = g_fw_idle_timer;
#endif /* MTK_GPU_FW_IDLE */

	if (!psHead)
		return ret;

	switch (mode) {
	case GED_KPI_FRC_DEFAULT_MODE:
		psHead->frc_mode = GED_KPI_FRC_DEFAULT_MODE;
		vsync_period = GED_KPI_SEC_DIVIDER / GED_KPI_MAX_FPS;
		break;
	default:
		psHead->frc_mode = GED_KPI_FRC_DEFAULT_MODE;
		vsync_period = GED_KPI_SEC_DIVIDER / GED_KPI_MAX_FPS;
		GED_LOGD("[Exception]: no invalid",
			"FRC mode is specified, use default mode");
		break;
	}
#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
	if (g_is_fw_idle_enable > 0) {
		/* update fw idle timer value */
		switch (g_fw_idle_mode) {
		case FW_IDLE_MODE_DEFAULT:
			if (psHead == main_head)
				idle_timer_ms =
					(target_fps <= FW_IDLE_FPS_THRESHOLD) ?
					FW_IDLE_TIMER_0_MS : FW_IDLE_TIMER_5_MS;
			break;
		case FW_IDLE_MODE_FIX_5:
			idle_timer_ms = FW_IDLE_TIMER_5_MS;
			break;
		case FW_IDLE_MODE_FIX_10:
			idle_timer_ms = FW_IDLE_TIMER_10_MS;
			break;
		}

		/* set fw idle timer if timer value ever changed*/
		if (g_is_panel_hz_change || g_fw_idle_timer != idle_timer_ms) {
			mtk_set_gpu_idle(idle_timer_ms);
			g_fw_idle_timer = idle_timer_ms;
			g_is_panel_hz_change = 0;
		}
	}
#endif /* MTK_GPU_FW_IDLE */

	psHead->target_fps = target_fps;
	psHead->target_fps_margin = target_fps_margin;
	psHead->eara_fps_margin = eara_fps_margin;
	psHead->t_cpu_fpsgo = cpu_time;
	if (target_fps > 0 && target_fps <= GED_KPI_FPS_LIMIT) {   // valid range
		psHead->t_cpu_target = (int)((int)GED_KPI_SEC_DIVIDER/target_fps);
		psHead->target_fps = target_fps;
	} else {   // invalid range, use default value
		psHead->t_cpu_target = (int)((int)GED_KPI_SEC_DIVIDER/g_target_fps_default);
		psHead->target_fps = -1;
	}

	// use api fps first if available
	if (psHead->target_fps_v > 0 &&
		psHead->target_fps_v <= GED_KPI_FPS_LIMIT) {
		psHead->t_gpu_target = (int)((int)GED_KPI_SEC_DIVIDER/psHead->target_fps_v);
	} else if (ignore_fpsgo_enable) {
		psHead->target_fps_margin = GED_KPI_DEFAULT_FPS_MARGIN;
		if (psHead->t_gpu_target == 0)
			psHead->t_gpu_target = g_gpu_target_default;
	} else {
		psHead->t_gpu_target = psHead->t_cpu_target;
	}

	psHead->frc_client = client;
	ret = GED_TRUE;

	GED_LOGD("FPSGO info PID:%d,tfps:%d,fps_margin:%d,eara_diff:%d,t_cpu:%d",
			psHead->pid,
			psHead->target_fps,
			psHead->target_fps_margin,
			psHead->eara_fps_margin,
			psHead->t_cpu_fpsgo);

	return ret;
}

static GED_BOOL ged_kpi_update_default_target_fps_fcn(unsigned long ulID,
	void *pvoid, void *pvParam)
{
	struct GED_KPI_HEAD *psHead = (struct GED_KPI_HEAD *) pvoid;

	if (psHead) {
		if (psHead->target_fps == -1)
			// only reset target FPS for those who use the default
			ged_kpi_update_TargetTimeAndTargetFps(
					psHead,
					-1,
					GED_KPI_DEFAULT_FPS_MARGIN, 0, 0,
					GED_KPI_FRC_DEFAULT_MODE, -1);
	}

	return GED_TRUE;
}

static GED_BOOL ged_kpi_update_api_fps_fcn(unsigned long ulID,
	void *pvoid, void *pvParam)
{
	struct GED_KPI_HEAD *psHead = (struct GED_KPI_HEAD *) pvoid;
	struct GED_TIMESTAMP *psTimeStamp = (struct GED_TIMESTAMP *)pvParam;

	if (psHead)
		psHead->target_fps_v = psTimeStamp->i32FrameID;

	return GED_TRUE;
}

/* ------------------------------------------------------------------- */
struct GED_KPI_MISS_TAG {
	u64 ulID;
	unsigned long i32FrameID;
	GED_TIMESTAMP_TYPE eTimeStampType;
	struct list_head sList;
};

#define GED_KPI_MISS_TAG_COUNT 16
static struct GED_KPI_MISS_TAG *miss_tag_head;
struct GED_KPI_MISS_TAG gs_miss_tag[GED_KPI_MISS_TAG_COUNT];
static int gs_miss_tag_idx;

static void ged_kpi_record_miss_tag(u64 ulID, int i32FrameID
	, GED_TIMESTAMP_TYPE eTimeStampType)
{
	struct GED_KPI_MISS_TAG *psMiss_tag;

	if (unlikely(miss_tag_head == NULL)) {
		miss_tag_head = (struct GED_KPI_MISS_TAG *)ged_alloc_atomic(
			sizeof(struct GED_KPI_MISS_TAG));
		if (miss_tag_head) {
			int i;

			memset(miss_tag_head, 0, sizeof(struct GED_KPI_MISS_TAG));
			memset(gs_miss_tag, 0, sizeof(gs_miss_tag));
			for (i = 0; i < GED_KPI_MISS_TAG_COUNT; i++)
				INIT_LIST_HEAD(&gs_miss_tag[i].sList);
			INIT_LIST_HEAD(&miss_tag_head->sList);
		} else {
			GED_LOGD("ged_alloc_atomic(sizeof(GED_KPI_MISS_TAG)) failed");
			return;
		}
	}
	psMiss_tag = &gs_miss_tag[gs_miss_tag_idx++];
	if (gs_miss_tag_idx == GED_KPI_MISS_TAG_COUNT)
		gs_miss_tag_idx = 0;
	list_del(&psMiss_tag->sList);

	if (unlikely(!psMiss_tag)) {
		GED_LOGD("ged_alloc_atomic(sizeof(GED_KPI_MISS_TAG)) failed");
		return;
	}

	memset(psMiss_tag, 0, sizeof(struct GED_KPI_MISS_TAG));
	INIT_LIST_HEAD(&psMiss_tag->sList);
	psMiss_tag->i32FrameID = i32FrameID;
	psMiss_tag->eTimeStampType = eTimeStampType;
	psMiss_tag->ulID = ulID;
	list_add_tail(&psMiss_tag->sList, &miss_tag_head->sList);
}

static GED_BOOL ged_kpi_find_and_delete_miss_tag(u64 ulID, int i32FrameID
	, GED_TIMESTAMP_TYPE eTimeStampType)
{
	GED_BOOL ret = GED_FALSE;

	if (miss_tag_head) {
		struct GED_KPI_MISS_TAG *psMiss_tag;
		struct list_head *psListEntry, *psListEntryTemp;
		struct list_head *psList = &miss_tag_head->sList;

		list_for_each_prev_safe(psListEntry, psListEntryTemp, psList) {
			psMiss_tag =
			list_entry(psListEntry, struct GED_KPI_MISS_TAG, sList);
			if (psMiss_tag
				&& psMiss_tag->ulID == ulID
				&& psMiss_tag->i32FrameID == i32FrameID
				&& psMiss_tag->eTimeStampType == eTimeStampType) {
				list_del(&psMiss_tag->sList);
				INIT_LIST_HEAD(&psMiss_tag->sList);
				ret = GED_TRUE;
				break;
			}
		}
	}

	return ret;
}

u64 ged_kpi_get_taget_time(void)
{
	//return t_gpu_target_now *2;
	//100ms
	return 100000000;
}

// input argument unit: nanosecond
static void set_lb_timeout(int t_gpu_target)
{
	t_gpu_target_now = t_gpu_target;

	switch (g_loading_target_mode) {
	case 0:
		lb_timeout = (u64)g_loading_stride_size * 1000000; //ms to ns
		break;
	case 1:
		lb_timeout = div_u64((u64)t_gpu_target * g_loading_stride_size, 10);
		break;
	case 2:
		lb_timeout = (u64)g_loading_stride_size * 1000000;
		break;
	case 3:
		lb_timeout = (u64)t_gpu_target;
		break;
	default:
		lb_timeout = (u64)g_loading_stride_size * 1000000;
		break;
	}
}

void ged_kpi_set_loading_mode(unsigned int mode, unsigned int stride_size)
{
	g_loading_target_mode = mode;
	g_loading_stride_size = stride_size;
	set_lb_timeout(t_gpu_target_now);
}

/* ------------------------------------------------------------------- */
/* while producer */
/* ------------------------------------------------------------------- */
static GED_BOOL ged_kpi_check_other_producer_func(unsigned long ulID,
	void *pvoid, void *pvParam)
{
	struct GED_KPI_HEAD *psHead = (struct GED_KPI_HEAD *) pvoid;
	unsigned long long main_ullWnd = *(unsigned long long *) pvParam;

	if (!psHead)
		return GED_FALSE;

	if (psHead->i32Gpu_uncompleted > 0 && ulID != main_ullWnd)
		g_is_multiproducer = true;
	return GED_TRUE;
}

/* ------------------------------------------------------------------- */
/* Determine whether the past 10 frames are LB */
/* ------------------------------------------------------------------- */
static bool check_stable_LB(int current_policy)
{
	int idx = 0;

	g_policy_frame_his[g_policy_his_idx++] = current_policy;
	if (g_policy_his_idx >= POLICY_FRAME_NUM)
		g_policy_his_idx = 0;

	while (idx < POLICY_FRAME_NUM) {
		// return if frame-based
		if (g_policy_frame_his[idx++] == 0)
			return false;
	}
	return true;
}

/* ------------------------------------------------------------------- */
/* for FB-base/LB-base mode switch */
/* ------------------------------------------------------------------- */
static void ged_kpi_set_fallback_mode(struct GED_KPI_HEAD *psHead)
{
	unsigned long ulIRQFlags;
	static unsigned int same_times;
	static unsigned int candidate_same_times;
	static unsigned int diff_times;
	static struct GED_KPI_HEAD *candidate_head;
	int isSmallFrame = 0;
	bool is_offscreen = false;
	unsigned int t_gpu_real_pipe_ratio = 0;

	spin_lock_irqsave(&gs_hashtableLock, ulIRQFlags);
	g_is_multiproducer = false;
	ged_hashtable_iterator(gs_hashtable,
		ged_kpi_check_other_producer_func, (void *) &psHead->ullWnd);
	spin_unlock_irqrestore(&gs_hashtableLock, ulIRQFlags);

	/*main_head first init*/
	if (main_head == NULL)
		main_head = psHead;
	if (psHead->ullWnd != main_head->ullWnd) {
		same_times = 0;
		diff_times += 1;
		/*consecutive not main head*/
		if (candidate_head != psHead) {
			candidate_head = psHead;
			candidate_same_times = 1;
		} else {
			if (g_is_multiproducer)
				candidate_same_times = 0;
			else
				candidate_same_times += 1;
		}
		/*change main_head when > threshold*/
		if (candidate_same_times >= GED_KPI_SWITCH_FB_THRESHOLD) {
			main_head = candidate_head;
			same_times = candidate_same_times;
			diff_times = 0;
			candidate_same_times = 0;
		}
	} else {
		diff_times = 0;
		candidate_same_times = 0;
		if (g_is_multiproducer)
			same_times = 0;
		else
			same_times += 1;
	}
	/*prevent overflow*/
	if (same_times > GED_KPI_MAX_SWITCH_COUNT)
		same_times = GED_KPI_MAX_SWITCH_COUNT;
	if (diff_times > GED_KPI_MAX_SWITCH_COUNT)
		diff_times = GED_KPI_MAX_SWITCH_COUNT;

	trace_GPU_DVFS__Policy__Common__Commit_Reason__TID(psHead->pid,
		(int)(psHead->ullWnd % 0xF), psHead->i32Count);
	trace_GPU_DVFS__Policy__Common__Commit_Reason(same_times, diff_times);
	/*check if LB or FB*/
	if (main_head->t_gpu_fps < GPU_TINY_FPS_THRESHOLD) {
		t_gpu_real_pipe_ratio = div64_u64(main_head->t_gpu_latest_real,
				main_head->t_gpu_latest);
		is_offscreen = (t_gpu_real_pipe_ratio > T_GPU_REAL_PIPE_DIFF_RATIO)? true: false;
	}

	if (same_times >= GED_KPI_SWITCH_FB_THRESHOLD) {
		if (main_head->isSF == 1) //surfacefliger use LB only
			is_loading_based = 1;
		//skip to LB if no fpsgo, 13 < q2q fps < 28, and q2q fps + 30 > vsync target fps
		else if (main_head->t_gpu_fps < GPU_MAX_FPS &&
			main_head->t_gpu_fps > GPU_MIN_FPS &&
			main_head->target_fps_v <= V_TARGET_FPS_TH &&
			main_head->target_fps_v > main_head->t_gpu_fps + TARGET_FPS_GAP) {
			is_loading_based = 1;
			isSmallFrame = 1;
			trace_tracing_mark_write(5566, "sf_policy", 1);
		} else if (is_offscreen) {
			is_loading_based = 1;
			trace_tracing_mark_write(5566, "is_offscreen", 1);
		} else /*switch FB*/
			is_loading_based = 0;
	} else if (diff_times > GED_KPI_SWITCH_LB_THRESHOLD)
		is_loading_based = 1; /*switch LB*/
	/*else keep pre_state*/
	ged_eb_dvfs_task(EB_UPDATE_SMALL_FRAME, isSmallFrame);
	g_stable_lb = check_stable_LB(is_loading_based);
	ged_eb_dvfs_task(EB_UPDATE_STABLE_LB, g_stable_lb);
}

static int ged_kpi_get_fallback_mode(void)
{
	if (force_loading_based_enable)
		return 1;
	else
		return is_loading_based;
}

static void ged_kpi_update_soc_timer(struct GED_KPI_HEAD *psHead, unsigned int ulMask)
{
#if defined(MTK_GPU_EB_SUPPORT)
	u64 soc_timer = 0;

	if (is_fdvfs_enable()) {
		int timer_change = 0;
		int un_count = mtk_gpueb_sysram_read(SYSRAM_GPU_EB_UNCOMPLETE_COUNT);
		int head_t_gpu_target = psHead->t_gpu_target / 1000;

		soc_timer = mtk_gpueb_read_soc_timer();
		psHead->last_TimeStamp1_soc_timer = soc_timer;
		/* update uncomplete time if no any uncomplete */
		if (g_ged_eb_uncomplete_info.last_uncomplete_type == 0 &&
			g_ged_eb_uncomplete_info.ulID == 0 &&
			psHead->i32Gpu_uncompleted_queue == 1) {
			timer_change = 1;
		} else if (g_ged_eb_uncomplete_info.ulID != psHead->ullWnd &&
			       g_ged_eb_uncomplete_info.ulID != 0) {
			unsigned long long info_soc_overdue =
					g_ged_eb_uncomplete_info.last_uncomplete_soc_timer;
			unsigned long long psHead_soc_overdue = soc_timer;
			/* compare overdue timer to check uncomplete soc timer*/
			info_soc_overdue +=
					(g_ged_eb_uncomplete_info.last_uncomplete_target / 1000) * 13;
			psHead_soc_overdue += head_t_gpu_target * 13;
			if (psHead_soc_overdue < info_soc_overdue)
				timer_change = 1;
		}

		if (ged_get_policy_state() == POLICY_STATE_LB) {
			un_count ++;
			mtk_gpueb_sysram_write(SYSRAM_GPU_EB_UNCOMPLETE_COUNT, un_count);
		}

		if (timer_change == 1) {
			trace_GPU_DVFS__Policy__Common__SOC_Timer_LB(SOC_QUEUE,
				(u32)(soc_timer >> 32),
				(u32)(soc_timer & 0xFFFFFFFF));
			g_ged_eb_uncomplete_info.last_uncomplete_type = ulMask;
			g_ged_eb_uncomplete_info.last_uncomplete_soc_timer = soc_timer;
			g_ged_eb_uncomplete_info.last_uncomplete_target = psHead->t_gpu_target;
			g_ged_eb_uncomplete_info.ulID = psHead->ullWnd ;
			mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_UNCOMPLETE_SOC_TIMER_HI,
							(u32)(soc_timer >> 32));
			mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_UNCOMPLETE_SOC_TIMER_LO,
							(u32)(soc_timer & 0xFFFFFFFF));
			mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_UNCOMPLETE_TARGET_TIME,
							head_t_gpu_target);
			mtk_gpueb_sysram_write(SYSRAM_GPU_RISKY_UNCOMPLETE_TIME, 0);
		}
	}
#endif
}

static void ged_kpi_remove_head(u64 ulID)
{
	int i = 0, j = 0;
	//remove head
	if (g_ged_head_record.head_num > 0) {
		for (i = 0; i < g_ged_head_record.head_num; i++) {
			if (g_ged_head_record.ulID[i] == ulID) {
				for (j = i; j < g_ged_head_record.head_num - 1 ; j++)
					g_ged_head_record.ulID[j] = g_ged_head_record.ulID[j+1];

				g_ged_head_record.ulID[g_ged_head_record.head_num - 1] = 0;
				g_ged_head_record.head_num--;
			}
		}
	} else
		g_ged_head_record.head_num = 0;
}

/* ------------------------------------------------------------------- */
static void ged_kpi_work_cb(struct work_struct *psWork)
{
	struct GED_TIMESTAMP *psTimeStamp =
		GED_CONTAINER_OF(psWork, struct GED_TIMESTAMP, sWork);
	struct GED_KPI_HEAD *psHead;
	struct GED_KPI *psKPI = NULL;
	struct list_head *psListEntry, *psListEntryTemp;
	struct list_head *psList;
	/* ================== */

	u64 ulID;
	unsigned long long ullTimeStampTemp;
	int target_FPS;
	unsigned long ulIRQFlags;
	int eara_fps_margin;

	int gpu_freq_pre, t_gpu, t_gpu_target, target_fps_margin;
	unsigned int force_fallback;

	GED_LOGD("ts type = %d, pid = %d, wnd = %llu, frame = %lu",
		psTimeStamp->eTimeStampType,
		psTimeStamp->pid,
		psTimeStamp->ullWnd,
		psTimeStamp->i32FrameID);

	switch (psTimeStamp->eTimeStampType) {
	case GED_TIMESTAMP_TYPE_D:
		psKPI = &g_asKPI[g_i32Pos++];
		if (g_i32Pos >= GED_KPI_TOTAL_ITEMS)
			g_i32Pos = 0;
#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
		/* enable fw idle policy after first dequeue event */
		if (g_is_fw_idle_enable == -1)
			g_is_fw_idle_enable = 1;
#endif /* MTK_GPU_FW_IDLE */

		/* remove */
		ulID = psKPI->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
						, (unsigned long)ulID);
		if (!psHead)
			GED_LOGD("no hashtable head for ulID: %lu", ulID);
		else {
			psHead->i32Count -= 1;
			spin_lock(&psHead->sListLock);
			list_del(&psKPI->sList);
			spin_unlock(&psHead->sListLock);

			if (psHead->i32Count < 1
				&& (psHead->sList.next == &(psHead->sList))) {
				if (psHead == main_head)
					main_head = NULL;

				spin_lock_irqsave(&gs_hashtableLock, ulIRQFlags);
				ged_kpi_remove_head(ulID);
				ged_hashtable_remove(gs_hashtable, (unsigned long)ulID);
				spin_unlock_irqrestore(&gs_hashtableLock, ulIRQFlags);

				ged_free(psHead, sizeof(struct GED_KPI_HEAD));
			}
		}

		/* reset data */
		memset(psKPI, 0, sizeof(struct GED_KPI));
		INIT_LIST_HEAD(&psKPI->sList);

		/* add */
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);

		if (!psHead) {
			psHead = (struct GED_KPI_HEAD *)
				ged_alloc_atomic(sizeof(struct GED_KPI_HEAD));
			if (!psHead) {
				GED_LOGD("ged_alloc_atomic (sizeof(GED_KPI_HEAD)) failed");
				goto work_cb_end;
			}
			memset(psHead, 0, sizeof(struct GED_KPI_HEAD));
			psHead->pid = psTimeStamp->pid;
			psHead->ullWnd = psTimeStamp->ullWnd;
			psHead->isSF = psTimeStamp->isSF;
			spin_lock_init(&psHead->sListLock);
			ged_kpi_update_TargetTimeAndTargetFps(
				psHead,
				-1,
				GED_KPI_DEFAULT_FPS_MARGIN, 0, 0,
				GED_KPI_FRC_DEFAULT_MODE, -1);
			ged_kpi_set_gift_status(0);
			INIT_LIST_HEAD(&psHead->sList);
			spin_lock_irqsave(&gs_hashtableLock, ulIRQFlags);
			ged_hashtable_set(gs_hashtable,
				(unsigned long)ulID, (void *)psHead);
			//add head
			if (g_ged_head_record.head_num < 20) {
				g_ged_head_record.ulID[g_ged_head_record.head_num] = ulID;
				g_ged_head_record.head_num++;
			} else
				g_ged_head_record.head_num = 20;

			spin_unlock_irqrestore(&gs_hashtableLock, ulIRQFlags);
		}
		memset(psKPI, 0, sizeof(struct GED_KPI));
		psKPI->ulMask |= GED_TIMESTAMP_TYPE_D;
		psKPI->ullTimeStampD = psTimeStamp->ullTimeStamp;
		psKPI->pid = psTimeStamp->pid;
		psKPI->ullWnd = psTimeStamp->ullWnd;
		psKPI->i32DeQueueID = psTimeStamp->i32FrameID;
		psKPI->isSF = psTimeStamp->isSF;
		spin_lock(&psHead->sListLock);
		list_add_tail(&psKPI->sList, &psHead->sList);
		spin_unlock(&psHead->sListLock);
		psHead->i32Count += 1;
		psHead->i32Gpu_uncompleted++;
		psHead->last_TimeStampD = psKPI->ullTimeStampD;
		psKPI->i32Gpu_uncompleted = psHead->i32Gpu_uncompleted;

		/*check if LB or FB*/
		ged_kpi_set_fallback_mode(psHead);
		/*First FB to LB*/
		mutex_lock(&gsPolicyLock);
		if (ged_get_policy_state() == POLICY_STATE_FB &&
			ged_kpi_get_fallback_mode()) {
			ged_set_policy_state(POLICY_STATE_LB);
			ged_eb_dvfs_task(EB_UPDATE_POLICY_STATE, GED_DVFS_LOADING_BASE_COMMIT);
			set_lb_timeout(psHead->t_gpu_target);
			ged_set_backup_timer_timeout(lb_timeout);
			ged_cancel_backup_timer();
		}
		mutex_unlock(&gsPolicyLock);
		break;

	/* queue buffer scope */
	case GED_TIMESTAMP_TYPE_1:
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);

		if (!psHead) {
			GED_LOGD("no hashtable head for ulID: %lu", ulID);
			break;
		}

		psList = &psHead->sList;
		list_for_each_prev_safe(psListEntry,
			psListEntryTemp, psList) {
			psKPI =
			list_entry(psListEntry, struct GED_KPI, sList);

			if (psKPI && (psKPI->i32DeQueueID == psTimeStamp->i32FrameID))
				break;
			psKPI = NULL;
		}
		if (psKPI == NULL) {
			GED_LOGD("TYPE_1: psKPI NULL, frameID: %lu",
				psTimeStamp->i32FrameID);
			goto work_cb_end;
		}

		/* new data */
		psKPI->pid = psTimeStamp->pid;
		psKPI->ullWnd = psTimeStamp->ullWnd;
		psKPI->i32QueueID = psTimeStamp->i32FrameID;
		psKPI->ulMask |= GED_TIMESTAMP_TYPE_1;
		psKPI->ullTimeStamp1 = psTimeStamp->ullTimeStamp;
		psKPI->i32QedBuffer_length = psTimeStamp->i32QedBuffer_length;
		psHead->i32QedBuffer_length = psTimeStamp->i32QedBuffer_length;

		/**********************************/
		psKPI->t_cpu_fpsgo = psHead->t_cpu_fpsgo;
		psKPI->t_cpu_target = psHead->t_cpu_target;
		psKPI->t_gpu_target = psHead->t_gpu_target;
		psKPI->target_fps_margin = psHead->target_fps_margin;
		psHead->i32DebugQedBuffer_length += 1;
		psHead->i32Gpu_uncompleted_queue++;
		psKPI->i32DebugQedBuffer_length = psHead->i32DebugQedBuffer_length;
		/* recording cpu time per frame & boost CPU if needed */
		ullTimeStampTemp = psHead->last_TimeStamp1;
		psHead->t_cpu_latest = psKPI->ullTimeStamp1 - psHead->last_TimeStamp1;
		psKPI->t_cpu = psHead->t_cpu_latest;
		psKPI->QedBufferDelay = psHead->last_QedBufferDelay;
		psHead->last_QedBufferDelay = 0;
		psHead->last_TimeStamp1 = psKPI->ullTimeStamp1;
		frame_base_timestamp = psTimeStamp->ullTimeStamp;
		frame_base_ulMask = GED_TIMESTAMP_TYPE_1;

		if (ged_kpi_find_and_delete_miss_tag(ulID,
			psTimeStamp->i32FrameID, GED_TIMESTAMP_TYPE_S) == GED_TRUE) {

			psTimeStamp->eTimeStampType = GED_TIMESTAMP_TYPE_S;
			ged_kpi_tag_type_s(ulID, psHead, psTimeStamp);
			psHead->i32DebugQedBuffer_length -= 1;

			GED_LOGD("timestamp matched, Type_S: psHead: %p", psHead);

			if (main_head != NULL &&
				(main_head->t_cpu_remained > (-1) * SCREEN_IDLE_PERIOD)) {
				GED_LOGD("t_cpu_remained: %lld, main_head: %p",
				main_head->t_cpu_remained, main_head);
				ged_kpi_set_cpu_remained_time(
					main_head->t_cpu_remained,
					main_head->i32QedBuffer_length);
			}
		}

		if (psHead->isSF != psTimeStamp->isSF)
			GED_LOGD("[Exception] psHead->isSF != psTimeStamp->isSF");
		if (psHead->pid != psTimeStamp->pid) {
			GED_LOGD("[Exception] psHead->pid != psTimeStamp->pid: (%d, %d)",
				psHead->pid, psTimeStamp->pid);
		}
		GED_LOGD("TimeStamp1, i32QedBuffer_length: %d, ts: %llu, psHead: %p",
			psTimeStamp->i32QedBuffer_length,
			psTimeStamp->ullTimeStamp,
			psHead);

		ged_eb_dvfs_task(EB_UPDATE_UNCOMPLETE_COUNT, psHead->i32Gpu_uncompleted_queue);
		ged_kpi_update_soc_timer(psHead, GED_TIMESTAMP_TYPE_1);

		break;

	/* GPU done scope */
	case GED_TIMESTAMP_TYPE_2:
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);

		if (!psHead) {
			GED_LOGD(
				"[Exception] no hashtable head for ulID: %lu", ulID);
			break;
		}

		psList = &psHead->sList;
		list_for_each_prev_safe(psListEntry, psListEntryTemp, psList) {
			psKPI = list_entry(psListEntry, struct GED_KPI, sList);

			if (psKPI && ((psKPI->ulMask & GED_TIMESTAMP_TYPE_2) == 0)
				&& (psKPI->i32QueueID == psTimeStamp->i32FrameID))
				break;
			psKPI = NULL;
		}

		if (!psKPI) {
			GED_LOGD(
				"[Exception] TYPE_2: psKPI NULL, frameID: %lu",
				psTimeStamp->i32FrameID);
			break;
		}
		/*only one policy at a time*/
		mutex_lock(&gsPolicyLock);
		psKPI->ulMask |= GED_TIMESTAMP_TYPE_2;
		psKPI->ullTimeStamp2 = psTimeStamp->ullTimeStamp;
		psKPI->cpu_gpu_info.gpu.tb_dvfs_mode =
			ged_dvfs_get_tb_dvfs_margin_mode();
		psKPI->cpu_gpu_info.gpu.tb_dvfs_margin =
			ged_dvfs_get_tb_dvfs_margin_cur();

		/* calculate gpu_pipe time */
		// default GPU completion start time is queue
		ullTimeStampTemp = psKPI->ullTimeStamp1;
		// release fence is signaled after queue
		if (g_ged_pre_fence_chk == 1 &&
			psKPI->ullTimeStampP > ullTimeStampTemp)
			ullTimeStampTemp = psKPI->ullTimeStampP;
		// previous frame's acquire fence is signaled after queue and release
		if (psHead->last_TimeStamp2 > ullTimeStampTemp)
			ullTimeStampTemp = psHead->last_TimeStamp2;
		psHead->t_gpu_latest = psKPI->ullTimeStamp2 - ullTimeStampTemp;
		psHead->t_gpu_latest_store[psHead->t_gpu_latest_index] = psHead->t_gpu_latest;
		psHead->t_gpu_latest_index++;
		if (psHead->t_gpu_latest_index >= t_gpu_store_count)
			psHead->t_gpu_latest_index = 0;

		if (dcs_get_adjust_support()) {
			unsigned int fr_cnt = (t_gpu_w_store_count > dcs_get_adjust_fr_cnt()) ?
									dcs_get_adjust_fr_cnt() : t_gpu_w_store_count;
			unsigned long long power_on_TimeStamp = ged_get_power_on_timestamp();
			unsigned int min_stack_freq = ged_get_freq_by_idx(ged_get_min_oppidx());
			unsigned int cur_stack_freq = ged_get_cur_stack_freq();
			long long t_gpu_w_latest = 0;
			unsigned int non_dcs = (ged_get_cur_oppidx() < ged_get_min_oppidx_real());

			psHead->non_dcs_latest_store[psHead->t_gpu_w_latest_index] = non_dcs;
			min_stack_freq = (min_stack_freq > 0) ? min_stack_freq : 1;
			if (dcs_get_adjust_support() < 3) {
				if (psKPI->ullTimeStamp2 > power_on_TimeStamp &&
					psKPI->ullTimeStamp2 - power_on_TimeStamp < psHead->t_gpu_latest ) {
					unsigned long long power_duration = psKPI->ullTimeStamp2 - power_on_TimeStamp;

					t_gpu_w_latest = div_u64(power_duration * cur_stack_freq, min_stack_freq);
				} else
					t_gpu_w_latest = div_u64(psHead->t_gpu_latest * cur_stack_freq, min_stack_freq);
			} else
				t_gpu_w_latest = div_u64(psHead->t_gpu_latest * cur_stack_freq, min_stack_freq);

			psHead->t_gpu_w_latest_store[psHead->t_gpu_w_latest_index] = t_gpu_w_latest;
			psHead->for_dcs_valid_cnt++;
			// overflow
			if (psHead->for_dcs_valid_cnt == 0)
				psHead->for_dcs_valid_cnt = fr_cnt + 1;
			// calculate risk dcs factor (t_gpu various is dramatic) for current BQ
			psKPI->t_gpu_target = psHead->t_gpu_target;
			t_gpu_target = psKPI->t_gpu_target;
			if (main_head == psHead && (psHead->for_dcs_valid_cnt > fr_cnt)) {
				if (t_gpu_target <= 8333333) {
					unsigned int non_dcs_cnt = 0;
					long long t_gpu_w_max = t_gpu_w_latest;
					long long t_gpu_w_min = t_gpu_w_latest;
					int cond_1 = 0;
					int cond_2 = 0;
					int cond_3 = 0;

					for (int i = 0 ; i < fr_cnt ; i++) {
						non_dcs_cnt += psHead->non_dcs_latest_store[i];
						t_gpu_w_max =
							MAX(psHead->t_gpu_w_latest_store[i], t_gpu_w_max);
						t_gpu_w_min =
							MIN(psHead->t_gpu_w_latest_store[i], t_gpu_w_min);
					}

					if (t_gpu_w_min == 0)
						t_gpu_w_min = 1;
					cond_1 = t_gpu_w_max > t_gpu_target;
					cond_2 = div64_u64(t_gpu_w_max * 10, t_gpu_w_min) >= dcs_get_adjust_ratio_th();
					cond_3 = div_u64(non_dcs_cnt * 100, fr_cnt) > dcs_get_adjust_non_dcs_th();

					if (cond_1 && cond_2 && cond_3)
						g_force_disable_dcs = true;
					else
						g_force_disable_dcs = false;
				} else
					g_force_disable_dcs = false;
			}

			psHead->t_gpu_w_latest_index++;
			if (psHead->t_gpu_w_latest_index >= fr_cnt)
				psHead->t_gpu_w_latest_index = 0;
		} else {
			psHead->for_dcs_valid_cnt = 0;
			psHead->non_dcs_cnt = 0;
			g_force_disable_dcs = false;
		}

		//also disable dcs if api boost happened before
		//if (ged_get_api_sync_ts()) {
		//	Reset timestamp if it has been a long time since the last API boost
		//		if (ged_get_time() / 1000000 - ged_get_api_sync_ts() > API_SYNC_TIMEOUT_MS)
		//			ged_reset_api_sync_ts();
		//		else
		//			g_force_disable_dcs = true;
		//}

		ged_eb_dvfs_task(EB_UPDATE_PRESERVE, g_force_disable_dcs);

		/* gpu info to KPI TAG */
		psKPI->t_gpu = psHead->t_gpu_latest;
		psKPI->gpu_freq = ged_get_cur_freq() / 1000;
		psKPI->gpu_freq_max =
			ged_get_freq_by_idx(ged_get_cur_limit_idx_ceil()) / 1000;

		psHead->pre_TimeStamp2 = psHead->last_TimeStamp2;
		psHead->last_TimeStamp2 = psTimeStamp->ullTimeStamp;
		psHead->i32Gpu_uncompleted--;
		psHead->i32Gpu_uncompleted_queue--;
		//ged_eb_dvfs_task(EB_UPDATE_UNCOMPLETE_COUNT, psHead->i32Gpu_uncompleted_queue);
		psHead->gpu_completed_count++;
		psKPI->gpu_loading = psTimeStamp->i32GPUloading;

		if (psKPI->gpu_loading == 0)
			mtk_get_gpu_loading(&psKPI->gpu_loading);

		psKPI->gpu_done_interval = psHead->last_TimeStamp2 -
			psHead->pre_TimeStamp2;
		psKPI->cpu_gpu_info.gpu.t_gpu_real =
				div_u64(((unsigned long long)
				(psHead->last_TimeStamp2 - psHead->pre_TimeStamp2))
				* psKPI->gpu_loading, 100U);
		psHead->t_gpu_latest_real = psKPI->cpu_gpu_info.gpu.t_gpu_real;
		psKPI->cpu_gpu_info.gpu.limit_upper = ged_get_cur_limiter_ceil();
		psKPI->cpu_gpu_info.gpu.limit_lower = ged_get_cur_limiter_floor();
		psKPI->cpu_gpu_info.gpu.gpu_dvfs |=
		(((unsigned long) ged_get_dvfs_margin()) & 0x3FF);
		psKPI->cpu_gpu_info.gpu.gpu_dvfs |=
		((((unsigned long) ged_get_dvfs_margin_mode()) & 0xFF) << 16);
		frame_base_ulMask = GED_TIMESTAMP_TYPE_2;

		/* Detect if there are multi renderers by */
		/* checking if there is struct GED_KPI info
		 * resource monopoly
		 */
		g_force_gpu_dvfs_fallback = ged_kpi_get_fallback_mode();

		/* set backup timer and execute FB DVFS */
		t_gpu = psKPI->t_gpu;
		t_gpu_target = psKPI->t_gpu_target;
		target_fps_margin = psKPI->target_fps_margin;
		force_fallback = g_force_gpu_dvfs_fallback;

		/* In GPU Job Boundary, ged will check & commit gpueb desire oppidx */
		ged_eb_dvfs_task(EB_COMMIT_DCS, 0);

		if (g_force_gpu_dvfs_fallback) {   // main producer ratio < thresh (LB)
			// use LB policy
			ged_set_policy_state(POLICY_STATE_LB);
			ged_eb_dvfs_task(EB_UPDATE_POLICY_STATE, GED_DVFS_LOADING_BASE_COMMIT);
			set_lb_timeout(psKPI->t_gpu_target);
		} else {   // main producer ratio >= thresh (FB)
			if (main_head == psHead) {   // is main head
				enum gpu_dvfs_policy_state policy_state;

				policy_state = ged_get_policy_state();
				if (policy_state == POLICY_STATE_LB ||
						policy_state == POLICY_STATE_LB_FALLBACK) {
					// previous commit is LB
					// cannot obtain workload of first frame, so use LB policy
					ged_set_policy_state(POLICY_STATE_FORCE_LB);
					force_fallback = 1;
					set_lb_timeout(psKPI->t_gpu_target);
				} else {   // previous commmit is FB or fallback
					// use FB policy
					ged_set_policy_state(POLICY_STATE_FB);
					ged_eb_dvfs_task(EB_UPDATE_POLICY_STATE, GED_DVFS_FRAME_BASE_COMMIT);
				}
			} else {   // not main head
				// do nothing
				t_gpu = -1;   // t_gpu isn't accurate, so hint -1
			}
		}

		gpu_freq_pre = ged_kpi_gpu_dvfs(t_gpu, t_gpu_target,
			target_fps_margin, force_fallback);
		ged_eb_dvfs_task(EB_UPDATE_GPU_TIME_INFO, 0);

		mutex_unlock(&gsPolicyLock);
		psKPI->cpu_gpu_info.gpu.gpu_freq_target = gpu_freq_pre;

		g_psGIFT->gpu_freq_cur = psKPI->gpu_freq * 1000;
		g_psGIFT->gpu_freq_max = psKPI->gpu_freq_max * 1000;
		g_psGIFT->gpu_freq_pred = gpu_freq_pre;
		if (main_head == psHead && psHead->pid == g_psGIFT->target_pid) {
			g_psGIFT->target_fps = psHead->target_fps;
			g_psGIFT->target_fps_margin = psKPI->target_fps_margin;
			g_psGIFT->eara_fps_margin = psHead->eara_fps_margin;
			g_psGIFT->gpu_time = psKPI->t_gpu;
		} else {
			g_psGIFT->target_fps = -1;
			g_psGIFT->target_fps_margin = 0;
			g_psGIFT->eara_fps_margin = 0;
			g_psGIFT->gpu_time = -1;
		}


		if (psHead->last_TimeStamp1 != psKPI->ullTimeStamp1)
			psHead->last_QedBufferDelay =
				psTimeStamp->ullTimeStamp - psHead->last_TimeStamp1;

		if (g_force_gpu_dvfs_fallback) {
			/* hint FPSGO do not use t_gpu */
			ged_kpi_output_gfx_info2(-1
				, psKPI->gpu_freq * 1000
				, psKPI->gpu_freq_max * 1000
				, ulID);
		} else {
			ged_kpi_output_gfx_info2(
				psHead->t_gpu_latest
				, psKPI->gpu_freq * 1000
				, psKPI->gpu_freq_max * 1000
				, ulID);
		}

		if (psKPI && (psKPI->ulMask & GED_TIMESTAMP_TYPE_S))
			ged_kpi_statistics_and_remove(psHead, psKPI);

		/* hint JS0, JS1 info to EAT */
		trace_tracing_mark_write(psTimeStamp->pid, "is_gift_on",
			g_psGIFT->gift_ratio);
		trace_tracing_mark_write(psTimeStamp->pid, "t_cpu", psKPI->t_cpu);
		trace_tracing_mark_write(psTimeStamp->pid, "t_gpu",	psKPI->t_gpu);
		break;

	/* Prefence scope */
	case GED_TIMESTAMP_TYPE_P:
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);

		if (!psHead) {
			GED_LOGD("[Exception] no hashtable head for ulID: %lu", ulID);
			break;
		}

		psList = &psHead->sList;
		list_for_each_prev_safe(psListEntry, psListEntryTemp, psList) {
			psKPI = list_entry(psListEntry, struct GED_KPI, sList);
			if (psKPI
				&& ((psKPI->ulMask & GED_TIMESTAMP_TYPE_P) == 0)
				&& ((psKPI->ulMask & GED_TIMESTAMP_TYPE_2) == 0)
				&& (psKPI->i32QueueID == psTimeStamp->i32FrameID))
				break;
			psKPI = NULL;
		}

		if (psKPI) {
			long long pre_fence_delay;
			pre_fence_delay = psTimeStamp->ullTimeStamp - psKPI->ullTimeStamp1;
			psKPI->ulMask |= GED_TIMESTAMP_TYPE_P;
			psKPI->ullTimeStampP = psTimeStamp->ullTimeStamp;
			psHead->last_TimeStampP = psKPI->ullTimeStampP;
			/* show prefence time */
			trace_tracing_mark_write(5566, "t_pre_fence_delay", pre_fence_delay);
			ged_kpi_update_soc_timer(psHead, GED_TIMESTAMP_TYPE_P);
		} else {
			GED_LOGD("[Exception] TYPE_P: psKPI NULL, frameID: %lu",
				psTimeStamp->i32FrameID);
		}

		break;

	/* acquire buffer scope (deprecated) */
	case GED_TIMESTAMP_TYPE_S:
		ulID = psTimeStamp->ullWnd;
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
			, (unsigned long)ulID);

		if (!psHead) {
			GED_LOGD("[Exception] TYPE_S: no hashtable head for ulID: %lu",
				ulID);
			break;
		}

		if (ged_kpi_tag_type_s(ulID, psHead, psTimeStamp) != GED_TRUE) {
			GED_LOGD("TYPE_S timestamp miss, ulID: %lu", ulID);
			ged_kpi_record_miss_tag(ulID, psTimeStamp->i32FrameID,
				GED_TIMESTAMP_TYPE_S);
		} else {
			psHead->i32DebugQedBuffer_length -= 1;
			GED_LOGD(
				"timestamp matched, Type_S: psHead: %p", psHead);
		}

		if ((main_head != NULL) &&
			(main_head->t_cpu_remained > (-1)*SCREEN_IDLE_PERIOD)) {
			GED_LOGD("t_cpu_remained: %lld, main_head: %p",
				main_head->t_cpu_remained, main_head);
			ged_kpi_set_cpu_remained_time(main_head->t_cpu_remained,
				main_head->i32QedBuffer_length);
		}
		break;

	/* HW vsync scope (deprecated)*/
	case GED_TIMESTAMP_TYPE_H:
		ged_hashtable_iterator(gs_hashtable, ged_kpi_h_iterator_func,
			(void *)psTimeStamp);
		{
			long long t_cpu_remained, t_gpu_remained;
			long t_cpu_target, t_gpu_target;

			ged_kpi_get_latest_perf_state(&t_cpu_remained,
				&t_gpu_remained, &t_cpu_target, &t_gpu_target);
			GED_LOGD("t_cpu: %ld, %lld, t_gpu: %ld, %lld",
				t_cpu_target,
				t_cpu_remained,
				t_gpu_target,
				t_gpu_remained);
		}
		break;

	case GED_SET_TARGET_FPS:
		target_FPS = psTimeStamp->i32FrameID;
		ulID = psTimeStamp->ullWnd;
		eara_fps_margin = psTimeStamp->i32QedBuffer_length;
		psHead =
			(struct GED_KPI_HEAD *)
			ged_hashtable_find(gs_hashtable, (unsigned long)ulID);

		if (!psHead) {
			/* Set target fps for UX BQ */
			int target_fps = target_FPS & 0x000000ff;

			if (ulID == GED_FPSGO_UX_BQ_ID && main_head &&
				g_max_core_num == SHADER_CORE_NUM) {
				//Remember UX BQ to avoid target fps affected by other BQ.
				if (target_fps > 85 && target_fps <= 120) {
					last_ux_head = main_head;
					g_is_ux_fps_set = 1;
				} else {
					last_ux_head = NULL;
					g_is_ux_fps_set = 0;
				}
			}

			GED_LOGD("@%s: no such renderer for BQ_ID: %llu", __func__, ulID);
			break;
		}

		if (eara_fps_margin == GED_API_FPS_HINT)
			psHead->target_fps_v = target_FPS&0x000000ff;
		else {
			ged_kpi_update_TargetTimeAndTargetFps(psHead,
				target_FPS&0x000000ff,
				(target_FPS&0x00000700) >> 8,
				(target_FPS&0xfffff100) >> 11,
				eara_fps_margin,
				GED_KPI_FRC_DEFAULT_MODE, -1);

			if (g_target_fps != psHead->target_fps)
				g_target_fps = psHead->target_fps;

			trace_tracing_mark_write(5566, "target_fps_fpsgo",
				(target_FPS&0x000000ff));
		}
		break;

	case GED_SET_PANEL_REFRESH_RATE:
		target_FPS = psTimeStamp->i32FrameID;
		eara_fps_margin = psTimeStamp->i32QedBuffer_length;

		if (target_FPS > 0 &&
				target_FPS <= GED_KPI_FPS_LIMIT) {   // valid range
			if (eara_fps_margin == GED_API_FPS_HINT)
				ged_hashtable_iterator(gs_hashtable,
					ged_kpi_update_api_fps_fcn, (void *)psTimeStamp);
			else if (g_target_fps_default != target_FPS) {   // panel refresh rate change
				g_target_fps_default = target_FPS;
				g_gpu_target_default = (int)((int)GED_KPI_SEC_DIVIDER/g_target_fps_default);
				g_set_panel_refresh_rate = true;
				ged_hashtable_iterator(gs_hashtable,
					ged_kpi_update_default_target_fps_fcn, NULL);
			}
			trace_tracing_mark_write(5566, "target_fps_panel", target_FPS);
		}
		break;

	default:
		break;
	}

work_cb_end:
	ged_free(psTimeStamp, sizeof(struct GED_TIMESTAMP));
}
/* ------------------------------------------------------------------- */
static GED_ERROR ged_kpi_push_timestamp(
	GED_TIMESTAMP_TYPE eTimeStampType,
	u64 ullTimeStamp,
	int pid,
	unsigned long long ullWnd,
	int i32FrameID,
	int QedBuffer_length,
	int isSF,
	void *fence_addr)
{
	static atomic_t event_QedBuffer_cnt, event_3d_fence_cnt, event_hw_vsync;
	unsigned long ui32IRQFlags;

	if (g_psWorkQueue && ged_kpi_enabled()) {
		struct GED_TIMESTAMP *psTimeStamp = (struct GED_TIMESTAMP *)
			ged_alloc_atomic(sizeof(struct GED_TIMESTAMP));
		unsigned int pui32Block, pui32Idle;

		if (!psTimeStamp) {
			GED_LOGD("GED_ERROR_OOM in %s", __func__);
			return GED_ERROR_OOM;
		}

		if (eTimeStampType == GED_TIMESTAMP_TYPE_2) {
			spin_lock_irqsave(&gsGpuUtilLock, ui32IRQFlags);

			if (!ged_kpi_get_fallback_mode() && pid != pid_sf) {
				struct GpuUtilization_Ex util_ex;
				ged_kpi_trigger_fb_dvfs();
				ged_dvfs_cal_gpu_utilization_ex(
					&(psTimeStamp->i32GPUloading),
					&pui32Block, &pui32Idle, &util_ex);
			} else if (is_fdvfs_enable() && ullTimeStamp > eb_ullTimeStamp) {
				if (ullTimeStamp - eb_ullTimeStamp > fb_timeout) {
					struct GpuUtilization_Ex util_ex;

					ged_dvfs_cal_gpu_utilization_ex(
						&(psTimeStamp->i32GPUloading),
						&pui32Block, &pui32Idle, &util_ex);
					eb_ullTimeStamp = ullTimeStamp;
				}
			} else {
				psTimeStamp->i32GPUloading = 0;
			}
			spin_unlock_irqrestore(&gsGpuUtilLock, ui32IRQFlags);
		}

		psTimeStamp->eTimeStampType = eTimeStampType;
		psTimeStamp->ullTimeStamp = ullTimeStamp;
		psTimeStamp->pid = pid;
		psTimeStamp->ullWnd = ullWnd;
		psTimeStamp->i32FrameID = i32FrameID;
		psTimeStamp->i32QedBuffer_length = QedBuffer_length;
		psTimeStamp->isSF = isSF;
		psTimeStamp->fence_addr = fence_addr;
		INIT_WORK(&psTimeStamp->sWork, ged_kpi_work_cb);
		queue_work(g_psWorkQueue, &psTimeStamp->sWork);
		switch (eTimeStampType) {
		case GED_TIMESTAMP_TYPE_D:
			break;
		case GED_TIMESTAMP_TYPE_1:
			atomic_inc_return(&event_QedBuffer_cnt);
			atomic_inc_return(&event_3d_fence_cnt);
			ged_eb_dvfs_task(EB_UPDATE_FB_TARGET_TIME, div_u64(fb_timeout, 1000));
			break;
		case GED_TIMESTAMP_TYPE_2:
			atomic_dec_return(&event_3d_fence_cnt);
			ged_eb_dvfs_task(EB_UPDATE_FB_TARGET_TIME_DONE, div_u64(fb_timeout, 1000));
			break;
		case GED_TIMESTAMP_TYPE_P:
			break;
		case GED_TIMESTAMP_TYPE_S:
			atomic_dec_return(&event_QedBuffer_cnt);
			break;
		case GED_TIMESTAMP_TYPE_H:
			atomic_set(&event_hw_vsync,
				(atomic_inc_return(&event_hw_vsync)%2));
			break;
		case GED_SET_TARGET_FPS:
			break;
		}
	}

	else {
		GED_LOGD("[Exception]: g_psWorkQueue: NULL or GED KPI is disabled");
		return GED_ERROR_FAIL;
	}

	return GED_OK;
}
/* ------------------------------------------------------------------- */
static GED_ERROR ged_kpi_timeD(int pid, u64 ullWdnd, int i32FrameID, int isSF)
{
	return ged_kpi_push_timestamp(GED_TIMESTAMP_TYPE_D, ged_get_time(), pid,
		ullWdnd, i32FrameID, -1, isSF, NULL);
}
/* ------------------------------------------------------------------- */
static GED_ERROR ged_kpi_time1(int pid, u64 ullWdnd, int i32FrameID
		, int QedBuffer_length, void *fence_addr)
{
	return ged_kpi_push_timestamp(GED_TIMESTAMP_TYPE_1, ged_get_time(), pid,
		ullWdnd, i32FrameID, QedBuffer_length, 0, fence_addr);
}
/* ------------------------------------------------------------------- */
static GED_ERROR ged_kpi_time2(int pid, u64 ullWdnd, int i32FrameID)
{
	return ged_kpi_push_timestamp(GED_TIMESTAMP_TYPE_2, ged_get_time(), pid,
		ullWdnd, i32FrameID, -1, -1, NULL);
}
/* ------------------------------------------------------------------- */
static GED_ERROR ged_kpi_timeP(int pid, u64 ullWdnd, int i32FrameID)
{
	return ged_kpi_push_timestamp(GED_TIMESTAMP_TYPE_P, ged_get_time(), pid,
		ullWdnd, i32FrameID, -1, -1, NULL);
}
/* ------------------------------------------------------------------- */
static GED_ERROR ged_kpi_timeS(int pid, u64 ullWdnd, int i32FrameID)
{
	return ged_kpi_push_timestamp(GED_TIMESTAMP_TYPE_S, ged_get_time(), pid,
		ullWdnd, i32FrameID, -1, -1, NULL);
}
/* ------------------------------------------------------------------- */
/* To prevent deadlock from fence_signal_lock when fence refcount
 * might be decreased to 0 in our fence callback function,
 * we decrease refcount by creating a new workqueue job.
 */
static void ged_kpi_fence_put_cb(struct work_struct *psWork)
{
	struct GED_KPI_GPU_TS *psMonitor;

	psMonitor =
	GED_CONTAINER_OF(psWork, struct GED_KPI_GPU_TS, sWork);

	if (psMonitor != NULL) {
		dma_fence_put(psMonitor->psSyncFence);
		ged_free(psMonitor, sizeof(struct GED_KPI_GPU_TS));
	}
}
static
/* ------------------------------------------------------------------- */
void ged_kpi_pre_fence_sync_cb(struct dma_fence *sFence,
	struct dma_fence_cb *waiter)
{
	struct GED_KPI_GPU_TS *psMonitor;

	psMonitor =
	GED_CONTAINER_OF(waiter, struct GED_KPI_GPU_TS, sSyncWaiter);

	ged_kpi_timeP(psMonitor->pid, psMonitor->ullWdnd,
		psMonitor->i32FrameID);

	INIT_WORK(&psMonitor->sWork, ged_kpi_fence_put_cb);
	queue_work(g_FenceWorkQueue, &psMonitor->sWork);
}
/* ------------------------------------------------------------------- */
static
void ged_kpi_gpu_3d_fence_sync_cb(struct dma_fence *sFence,
	struct dma_fence_cb *waiter)
{
	struct GED_KPI_GPU_TS *psMonitor;

	psMonitor =
	GED_CONTAINER_OF(waiter, struct GED_KPI_GPU_TS, sSyncWaiter);

#if defined(MTK_GPU_BM_2)
	mtk_bandwidth_update_info(psMonitor->pid,
		qos_inc_frame_nr(),
		qos_get_frame_nr());
#endif /* MTK_GPU_BM_2 */

	// Hint frame boundary
	if (ged_is_fdvfs_support() &&
		(!ged_kpi_check_if_fallback_mode() && !g_force_gpu_dvfs_fallback)
			&& psMonitor->pid != pid_sf && psMonitor->pid != pid_sysui)
		g_eb_workload = mtk_gpueb_dvfs_set_frame_done();

	ged_kpi_time2(psMonitor->pid, psMonitor->ullWdnd,
		psMonitor->i32FrameID);

	INIT_WORK(&psMonitor->sWork, ged_kpi_fence_put_cb);
	queue_work(g_FenceWorkQueue, &psMonitor->sWork);
}
#endif /* MTK_GED_KPI */
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_acquire_buffer_ts(int pid, u64 ullWdnd, int i32FrameID)
{
#ifdef MTK_GED_KPI
	GED_ERROR ret;

	ret = ged_kpi_timeS(pid, ullWdnd, i32FrameID);
	return ret;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_enabled(void)
{
#ifdef MTK_GED_KPI
	return is_GED_KPI_enabled && (g_is_bringup == 0);
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_dequeue_buffer_ts(int pid, u64 ullWdnd, int i32FrameID,
					int fence_fd, int isSF)
{
#ifdef MTK_GED_KPI
	int ret = GED_OK;

#if defined(MTK_GPU_BM_2)
	mtk_bandwidth_check_SF(pid, isSF);
#endif /* MTK_GPU_BM_2 */

	struct GED_KPI_GPU_TS *psMonitor;
	struct dma_fence *psSyncFence;

	psSyncFence = sync_file_get_fence(fence_fd);

	psMonitor =
	(struct GED_KPI_GPU_TS *)ged_alloc(sizeof(struct GED_KPI_GPU_TS));

	if (!psMonitor) {
		pr_info("[GED_KPI]: GED_ERROR_OOM in %s\n",
			__func__);
		return GED_ERROR_OOM;
	}

	ged_kpi_timeD(pid, ullWdnd, i32FrameID, isSF);

	if (isSF == -1 && pid != pid_sysui)
		pid_sysui = pid;

	if (isSF == 1 && pid != pid_sf)
		pid_sf = pid;

	psMonitor->psSyncFence = psSyncFence;
	psMonitor->pid = pid;
	psMonitor->ullWdnd = ullWdnd;
	psMonitor->i32FrameID = i32FrameID;

	if (psMonitor->psSyncFence == NULL) {
		ged_free(psMonitor, sizeof(struct GED_KPI_GPU_TS));
		ret = ged_kpi_timeP(pid, ullWdnd, i32FrameID);
	} else {
		ret = dma_fence_add_callback(psMonitor->psSyncFence,
			&psMonitor->sSyncWaiter, ged_kpi_pre_fence_sync_cb);

		if (ret < 0) {
			dma_fence_put(psMonitor->psSyncFence);
			ged_free(psMonitor, sizeof(struct GED_KPI_GPU_TS));
			ret = ged_kpi_timeP(pid, ullWdnd, i32FrameID);
		}
	}
	return ret;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_queue_buffer_ts(int pid, u64 ullWdnd, int i32FrameID,
					int fence_fd, int QedBuffer_length)
{
#ifdef MTK_GED_KPI
	int ret;
	struct GED_KPI_GPU_TS *psMonitor;
	struct dma_fence *psSyncFence;

	psSyncFence = sync_file_get_fence(fence_fd);

	ret = ged_kpi_time1(pid, ullWdnd, i32FrameID, QedBuffer_length,
		(void *)psSyncFence);

	if (ret != GED_OK)
		return ret;

	psMonitor =
	(struct GED_KPI_GPU_TS *)ged_alloc(sizeof(struct GED_KPI_GPU_TS));

	if (!psMonitor) {
		pr_info("GED_ERROR_OOM in %s",
			__func__);
		return GED_ERROR_OOM;
	}

	psMonitor->psSyncFence = psSyncFence;
	psMonitor->pid = pid;
	psMonitor->ullWdnd = ullWdnd;
	psMonitor->i32FrameID = i32FrameID;

	if (psMonitor->psSyncFence == NULL) {
		ged_free(psMonitor, sizeof(struct GED_KPI_GPU_TS));
		ret = ged_kpi_time2(pid, ullWdnd, i32FrameID);
	} else {

		ret = dma_fence_add_callback(psMonitor->psSyncFence
			, &psMonitor->sSyncWaiter
			, ged_kpi_gpu_3d_fence_sync_cb);

		/* acquire fence is already signaled before cb registration */
		if (ret == -ENOENT) {
			// manually execute cb as cb would not be called in this case
			ged_kpi_gpu_3d_fence_sync_cb(psMonitor->psSyncFence,
				&psMonitor->sSyncWaiter);
			ret = 0;
		}
	}
	return ret;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_sw_vsync(void)
{
#ifdef MTK_GED_KPI
	return GED_OK; /* disabled function*/
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_hw_vsync(void)
{
#ifdef MTK_GED_KPI
	return ged_kpi_push_timestamp(GED_TIMESTAMP_TYPE_H,
		ged_get_time(), 0, 0, 0, 0, 0, NULL);
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
void ged_kpi_get_latest_perf_state(long long *t_cpu_remained,
						long long *t_gpu_remained,
						long *t_cpu_target,
						long *t_gpu_target)
{
#ifdef MTK_GED_KPI
	if (t_cpu_remained != NULL && main_head != NULL &&
		!(main_head->t_cpu_remained < (-1)*SCREEN_IDLE_PERIOD))
		*t_cpu_remained = main_head->t_cpu_remained;

	if (t_gpu_remained != NULL && main_head != NULL &&
		!(main_head->t_gpu_remained < (-1)*SCREEN_IDLE_PERIOD))
		*t_gpu_remained = main_head->t_gpu_remained;

	if (t_cpu_target != NULL && main_head != NULL)
		*t_cpu_target = main_head->t_cpu_target;
	if (t_gpu_target != NULL && main_head != NULL)
		*t_gpu_target = main_head->t_gpu_target;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
int ged_kpi_get_uncompleted_count(void)
{
#ifdef MTK_GED_KPI
	/* return gx_i32UncompletedCount; */
	return 0; /* disabled function */
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_fps(void)
{
#ifdef MTK_GED_KPI
	return gx_fps;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_avg_cpu_time(void)
{
#ifdef MTK_GED_KPI
	return gx_cpu_time_avg;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_avg_gpu_time(void)
{
#ifdef MTK_GED_KPI
	return gx_gpu_time_avg;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_avg_response_time(void)
{
#ifdef MTK_GED_KPI
	return gx_response_time_avg;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_avg_gpu_remained_time(void)
{
#ifdef MTK_GED_KPI
	return gx_gpu_remained_time_avg;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_avg_cpu_remained_time(void)
{
#ifdef MTK_GED_KPI
	return gx_cpu_remained_time_avg;
#else
	return 0;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
unsigned int ged_kpi_get_cur_avg_gpu_freq(void)
{
#ifdef MTK_GED_KPI
	return gx_gpu_freq_avg;
#else
	return 0;
#endif /* MTK_GED_KPI */
}

static GED_BOOL ged_kpi_update_sysram_uncompleted_fcn(void *pvoid, void *pvParam)
{
	struct GED_KPI_HEAD *psHead = (struct GED_KPI_HEAD *) pvoid;
	struct ged_sysram_info *info = (struct ged_sysram_info *) pvParam;
	struct GED_KPI *psKPI = NULL;
	struct list_head *psListEntry, *psListEntryTemp;
	struct list_head *psList = &psHead->sList;
	long long t_gpu_uncomplete = 0;
	int i = 0;

	spin_lock(&psHead->sListLock);
	list_for_each_prev_safe(psListEntry, psListEntryTemp, psList) {
		psKPI = list_entry(psListEntry, struct GED_KPI, sList);
		if (psKPI && (psKPI->ulMask & GED_TIMESTAMP_TYPE_2) == 0) {
			// only uncompleted frame has uncompleted time
			// GPU completion start time is queue
			if (psKPI->ulMask & GED_TIMESTAMP_TYPE_1)
				t_gpu_uncomplete = info->current_timestamp - psKPI->ullTimeStamp1;

			// release fence is signaled after queue
			if (g_ged_pre_fence_chk == 1 &&
				psKPI->ulMask & GED_TIMESTAMP_TYPE_P &&
				psKPI->ulMask & GED_TIMESTAMP_TYPE_1 &&
				psKPI->ullTimeStampP > psKPI->ullTimeStamp1)
				t_gpu_uncomplete = info->current_timestamp - psKPI->ullTimeStampP;
			// 4/3 buffer per BQ for 120FPS/<120FPS producer
			if (++i >= 4)
				break;
		}
	}
	spin_unlock(&psHead->sListLock);

	psHead->t_gpu_latest_uncompleted = t_gpu_uncomplete;

	if (info->last_tgpu_uncompleted <  psHead->t_gpu_latest_uncompleted)
		info->last_tgpu_uncompleted = psHead->t_gpu_latest_uncompleted;
	info->uncompleted_count += psHead->i32Gpu_uncompleted;

	return GED_TRUE;
}

void ged_kpi_update_sysram_uncompleted_tgpu(struct ged_sysram_info *info)
{
#if defined(MTK_GPU_EB_SUPPORT)
	unsigned long long t_gpu_uncomplete = 0;
	unsigned long long soc_timer = 0;
	unsigned long long cur_type = 0;
	unsigned long ulIRQFlags;
	int i;
	struct GED_KPI_HEAD *psHead;
	unsigned long long current_timestamp;

	if (info == NULL) {
		GED_LOGD("ged sysram is null pointer");
		return;
	}

	memset(info, 0, sizeof(struct ged_sysram_info));
	current_timestamp = ged_get_time();

	spin_lock_irqsave(&gs_hashtableLock, ulIRQFlags);
	for	(i = 0; i < g_ged_head_record.head_num; i++) {
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
						, (unsigned long)g_ged_head_record.ulID[i]);
		if (psHead) {
			if (psHead->i32Gpu_uncompleted_queue > 0) {
				unsigned long long ullTimeStampTemp;

				// Default GPU completion start time is queue
				ullTimeStampTemp = psHead->last_TimeStamp1;
				cur_type = GED_TIMESTAMP_TYPE_1;
				soc_timer = psHead->last_TimeStamp1_soc_timer;

				// release fence is signaled after queue
				if (g_ged_pre_fence_chk == 1 &&
					psHead->last_TimeStampP > psHead->last_TimeStamp1) {
					ullTimeStampTemp = psHead->last_TimeStampP;
					cur_type = GED_TIMESTAMP_TYPE_P;
					soc_timer = psHead->last_TimeStampP_soc_timer;
				}

				t_gpu_uncomplete = current_timestamp - ullTimeStampTemp;
			} else {
				t_gpu_uncomplete = 0 ;
			}

			psHead->t_gpu_latest_uncompleted = t_gpu_uncomplete;

			if (GPU_SOC_TIMER_SUPPORT) {
				unsigned long long info_soc_overdue = info->last_uncomplete_soc_timer;
				unsigned long long psHead_soc_overdue = soc_timer;
				/* compare overdue timer to check uncomplete */
				/* info is no data at first time */
				if (info->last_tgpu_uncompleted_target == 0 &&
					psHead->t_gpu_latest_uncompleted > 0) {
					info->last_tgpu_uncompleted = psHead->t_gpu_latest_uncompleted;
					info->last_tgpu_uncompleted_target = psHead->t_gpu_target;
					info->last_uncomplete_soc_timer = soc_timer;
					g_ged_eb_uncomplete_info.last_uncomplete_type = cur_type;
					g_ged_eb_uncomplete_info.last_uncomplete_soc_timer = soc_timer;
					g_ged_eb_uncomplete_info.ulID = psHead->ullWnd;
					g_ged_eb_uncomplete_info.last_uncomplete_target =
						psHead->t_gpu_target;
				} else if (psHead->t_gpu_latest_uncompleted > 0){
					info_soc_overdue += (info->last_tgpu_uncompleted_target / 1000) * 13;
					psHead_soc_overdue += (psHead->t_gpu_target / 1000) * 13;
					if (psHead_soc_overdue < info_soc_overdue) {
						info->last_tgpu_uncompleted = psHead->t_gpu_latest_uncompleted;
						info->last_tgpu_uncompleted_target = psHead->t_gpu_target;
						info->last_uncomplete_soc_timer = soc_timer;
						g_ged_eb_uncomplete_info.last_uncomplete_type = cur_type;
						g_ged_eb_uncomplete_info.last_uncomplete_soc_timer = soc_timer;
						g_ged_eb_uncomplete_info.ulID = psHead->ullWnd;
						g_ged_eb_uncomplete_info.last_uncomplete_target =
							psHead->t_gpu_target;
					}
				}
			} else {
				if (info->last_tgpu_uncompleted <=  psHead->t_gpu_latest_uncompleted) {
					info->last_tgpu_uncompleted = psHead->t_gpu_latest_uncompleted;
					info->last_tgpu_uncompleted_target = psHead->t_gpu_target;
					info->last_uncomplete_soc_timer = soc_timer;
					g_ged_eb_uncomplete_info.last_uncomplete_type = cur_type;
					g_ged_eb_uncomplete_info.last_uncomplete_soc_timer = soc_timer;
					g_ged_eb_uncomplete_info.ulID = psHead->ullWnd;
					g_ged_eb_uncomplete_info.last_uncomplete_target =
							psHead->t_gpu_target;

				}
			}

			info->uncompleted_count += psHead->i32Gpu_uncompleted_queue;
		}
	}


	if (info->uncompleted_count == 0) {
		memset(&g_ged_eb_uncomplete_info, 0, sizeof(struct ged_eb_dvfs_last_record));
	}

	spin_unlock_irqrestore(&gs_hashtableLock, ulIRQFlags);
#endif
}


/* ------------------------------------------------------------------- */
void ged_dfrc_fps_limit_cb(unsigned int target_fps)
{
#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
	/* update fw idle timer value */
	switch (g_fw_idle_mode) {
	case FW_IDLE_MODE_DEFAULT:
		g_fw_idle_timer = (target_fps <= FW_IDLE_FPS_THRESHOLD) ?
			FW_IDLE_TIMER_0_MS : FW_IDLE_TIMER_5_MS;
		break;
	case FW_IDLE_MODE_FIX_5:
		g_fw_idle_timer = FW_IDLE_TIMER_5_MS;
		break;
	case FW_IDLE_MODE_FIX_10:
		g_fw_idle_timer = FW_IDLE_TIMER_10_MS;
		break;
	}

	g_is_panel_hz_change = 1;
#endif /* MTK_GPU_FW_IDLE */

#ifdef MTK_GED_KPI
	ged_kpi_push_timestamp(GED_SET_PANEL_REFRESH_RATE, 0, -1, 0,
		(int) target_fps, -1, -1, NULL);
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_system_init(void)
{
#ifdef MTK_GED_KPI
#ifndef GED_BUFFER_LOG_DISABLE
	ghLogBuf_KPI = ged_log_buf_alloc(GED_KPI_MAX_FPS * 10,
		220 * GED_KPI_MAX_FPS * 10,
		GED_LOG_BUF_TYPE_RINGBUFFER, NULL, "KPI");
#else
	ghLogBuf_KPI = 0;
#endif /* GED_BUFFER_LOG_DISABLE */
	struct device_node *dvfs_prefence_node = NULL;
	is_GED_KPI_enabled = ged_gpufreq_bringup() ? 0 : 1;
	g_eb_workload = 0;
	force_loading_based_enable = 0;
	update_gpu_fps_table(1);

	g_psGIFT = (struct GED_KPI_MEOW_DVFS_FREQ_PRED *)
		ged_alloc_atomic(sizeof(struct GED_KPI_MEOW_DVFS_FREQ_PRED));
	if (unlikely(!g_psGIFT)) {
		GED_LOGD(
		"ged_alloc_atomic(sizeof(struct GED_KPI_MEOW_DVFS_FREQ_PRED)) failed");
		return GED_ERROR_FAIL;
	}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
	drm_register_fps_chg_callback(ged_dfrc_fps_limit_cb);
#endif
	g_max_core_num = gpufreq_get_core_num();

	dvfs_prefence_node = of_find_compatible_node(NULL, NULL, "mediatek,gpu_prefence");
	if (unlikely(!dvfs_prefence_node))
		GED_LOGI("Failed to find dvfs_prefence_node");
	else
		of_property_read_u32(dvfs_prefence_node, "dvfs-prefence-enable",
							&g_ged_pre_fence_chk);

	g_psWorkQueue =
		alloc_ordered_workqueue("ged_kpi",
			WQ_FREEZABLE | WQ_MEM_RECLAIM);
	g_FenceWorkQueue =
		alloc_ordered_workqueue("ged_fence",
			WQ_FREEZABLE | WQ_MEM_RECLAIM);
	if (g_psWorkQueue && g_FenceWorkQueue) {
		int i;

		memset(g_asKPI, 0, sizeof(g_asKPI));
		for (i = 0; i < GED_KPI_TOTAL_ITEMS; i++)
			g_asKPI[i].ullWnd = 0x0 - 1;
		gs_hashtable = ged_hashtable_create(10);
		spin_lock_init(&gs_hashtableLock);

		return gs_hashtable ? GED_OK : GED_ERROR_FAIL;
	}
	return GED_ERROR_FAIL;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
void ged_kpi_system_exit(void)
{
#ifdef MTK_GED_KPI
	unsigned long ulIRQFlags;

	spin_lock_irqsave(&gs_hashtableLock, ulIRQFlags);
	ged_hashtable_iterator_delete(gs_hashtable,
		ged_kpi_iterator_delete_func, NULL);
	spin_unlock_irqrestore(&gs_hashtableLock, ulIRQFlags);
	destroy_workqueue(g_psWorkQueue);
#if IS_ENABLED(CONFIG_DEVICE_MODULES_DRM_MEDIATEK)
	drm_unregister_fps_chg_callback(ged_dfrc_fps_limit_cb);
#endif
	destroy_workqueue(g_FenceWorkQueue);
	ged_thread_destroy(ghThread);
#ifndef GED_BUFFER_LOG_DISABLE
	ged_log_buf_free(ghLogBuf_KPI);
	ghLogBuf_KPI = 0;
#endif /* GED_BUFFER_LOG_DISABLE */
	ged_free(g_psGIFT, sizeof(struct GED_KPI_MEOW_DVFS_FREQ_PRED));
#endif /* MTK_GED_KPI */
}
/* ------------------------------------------------------------------- */
void (*ged_kpi_set_gpu_dvfs_hint_fp)(int t_gpu_target, int boost_accum_gpu);
EXPORT_SYMBOL(ged_kpi_set_gpu_dvfs_hint_fp);

bool ged_kpi_set_gpu_dvfs_hint(int t_gpu_target, int boost_accum_gpu)
{
	if (ged_kpi_set_gpu_dvfs_hint_fp != NULL) {
		ged_kpi_set_gpu_dvfs_hint_fp(t_gpu_target, boost_accum_gpu);
		return true;
	}
	return false;
}
/* ------------------------------------------------------------------- */
void (*ged_kpi_set_cpu_remained_time_fp)(long long t_cpu_remained,
	int QedBuffer_length);
EXPORT_SYMBOL(ged_kpi_set_cpu_remained_time_fp);

bool ged_kpi_set_cpu_remained_time(long long t_cpu_remained,
	int QedBuffer_length)
{
	if (ged_kpi_set_cpu_remained_time_fp != NULL) {
		ged_kpi_set_cpu_remained_time_fp(t_cpu_remained,
			QedBuffer_length);
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------- */
// void ged_kpi_set_target_FPS(u64 ulID, int target_FPS)
// {
// #ifdef MTK_GED_KPI
// 	ged_kpi_push_timestamp(GED_SET_TARGET_FPS, 0, -1,
// 				ulID, target_FPS, -1, -1, NULL);
// #endif /* MTK_GED_KPI */
// }
// EXPORT_SYMBOL(ged_kpi_set_target_FPS);
/* ------------------------------------------------------------------- */
void ged_kpi_set_target_FPS_margin(u64 ulID, int target_FPS,
		int target_FPS_margin, int eara_fps_margin, int cpu_time)
{
#ifdef MTK_GED_KPI
		ged_kpi_push_timestamp(GED_SET_TARGET_FPS, 0, -1, ulID,
			(target_FPS | (target_FPS_margin << 8)
			| ((cpu_time/1000) << 11)), eara_fps_margin, -1, NULL);
#endif /* MTK_GED_KPI */
}
EXPORT_SYMBOL(ged_kpi_set_target_FPS_margin);
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_target_fps_hint(int promotion_enable, int target_fps)
{
	int ret = GED_OK;

	prom_enable = promotion_enable;
	g_target_fps_vsync = target_fps;

	//ret = ged_kpi_push_timestamp(GED_SET_VSYNC_TARGET_FPS, 0, -1, promotion_enable,
		//0, target_fps, -1, NULL);
	trace_tracing_mark_write(5566, "promotion_enable", prom_enable);
	trace_tracing_mark_write(5566, "vsync_target_fps", g_target_fps_vsync);
	return ret;
}
/* ------------------------------------------------------------------- */
void ged_kpi_set_target_FPS_api(u64 ulID, int target_FPS, int target_FPS_margin)
{
#ifdef MTK_GED_KPI
	trace_tracing_mark_write(5566, "target_fps_api", target_FPS);

	if (ulID == 0)
		ged_kpi_push_timestamp(GED_SET_PANEL_REFRESH_RATE, 0, -1, 0,
			target_FPS, GED_API_FPS_HINT, -1, NULL);
	else
		ged_kpi_push_timestamp(GED_SET_TARGET_FPS, 0, -1, ulID,
			(target_FPS | (target_FPS_margin << 8)), GED_API_FPS_HINT, -1, NULL);

#endif /* MTK_GED_KPI */
}
EXPORT_SYMBOL(ged_kpi_set_target_FPS_api);
/* ------------------------------------------------------------------- */
#if IS_ENABLED(CONFIG_MTK_GPU_FW_IDLE)
int ged_kpi_get_fw_idle_mode(void)
{
	return g_fw_idle_mode;
}
/* ------------------------------------------------------------------- */
int ged_kpi_is_fw_idle_policy_enable(void)
{
	return g_is_fw_idle_enable;
}
/* ------------------------------------------------------------------- */
void ged_kpi_set_fw_idle_mode(unsigned int mode)
{
	if (mode == FW_IDLE_MODE_DEFAULT || mode == FW_IDLE_MODE_Fix_0
		|| mode == FW_IDLE_MODE_FIX_5 || mode == FW_IDLE_MODE_FIX_10)
		g_fw_idle_mode = mode;

	if (!mode)
		mtk_set_gpu_idle(g_fw_idle_timer);

}
EXPORT_SYMBOL(ged_kpi_set_fw_idle_mode);
/* ------------------------------------------------------------------- */
void ged_kpi_enable_fw_idle_policy(unsigned int enable)
{
	g_is_fw_idle_enable = enable;

	if (enable)
		g_fw_idle_mode = FW_IDLE_MODE_DEFAULT;
}
EXPORT_SYMBOL(ged_kpi_enable_fw_idle_policy);
#endif /* MTK_GPU_FW_IDLE */
/* ------------------------------------------------------------------- */
int ged_kpi_get_panel_refresh_rate(void)
{
	return g_target_fps_default;
}
EXPORT_SYMBOL(ged_kpi_get_panel_refresh_rate);
/* ------------------------------------------------------------------- */
int ged_kpi_get_target_fps(void)
{
	return g_target_fps;
}
EXPORT_SYMBOL(ged_kpi_get_target_fps);
/* ------------------------------------------------------------------- */

static GED_BOOL ged_kpi_find_riskyBQ_func(unsigned long ulID,
	void *pvoid, void *pvParam)
{
	struct GED_KPI_HEAD *psHead = (struct GED_KPI_HEAD *) pvoid;
	struct ged_risky_bq_info *info = (struct ged_risky_bq_info *) pvParam;

	if (psHead && info &&
			psHead->t_gpu_latest > 0 &&
			psHead->t_gpu_target > 0) {
		long long t_gpu_latest = 0;
		long long t_gpu_latest_uncompleted = 0;
		int t_gpu_target;
		unsigned long long risk_completed, risk_uncompleted;
		int i = 0;

		// aggregate GPU completed count
		info->total_gpu_completed_count += psHead->gpu_completed_count;

		// calculate risk factor for current BQ
		for (i = 0 ; i < t_gpu_store_count ; i++)
			t_gpu_latest = MAX(psHead->t_gpu_latest_store[i], t_gpu_latest);

		t_gpu_latest_uncompleted = psHead->t_gpu_latest_uncompleted;
		t_gpu_target = psHead->t_gpu_target;
		risk_completed = div_u64(t_gpu_latest * 100, t_gpu_target);
		risk_uncompleted = div_u64(t_gpu_latest_uncompleted * 100, t_gpu_target);

		if (psHead->t_gpu_fps < GPU_MAX_FPS &&
			psHead->t_gpu_fps > GPU_MIN_FPS &&
			psHead->target_fps_v <= V_TARGET_FPS_TH &&
			psHead->target_fps_v > psHead->t_gpu_fps + TARGET_FPS_GAP) {
			trace_tracing_mark_write(5566, "sf_policy", 2);
			info->smallframe = true;
		}

		//prevent less than 1%
		if (t_gpu_latest > 0 && risk_completed == 0)
			risk_completed = 1;
		if (t_gpu_latest_uncompleted > 0 && risk_uncompleted == 0)
			risk_uncompleted = 1;
		// update most risky BQ
		if (risk_completed > info->completed_bq.risk) {
			info->completed_bq.t_gpu = t_gpu_latest;
			info->completed_bq.t_gpu_target = t_gpu_target;
			info->completed_bq.risk = risk_completed;
			info->completed_bq.ullWnd = psHead->ullWnd;
			info->completed_bq.pid = psHead->pid;
		}
		if (risk_uncompleted > info->uncompleted_bq.risk) {
			info->uncompleted_bq.t_gpu = t_gpu_latest_uncompleted;
			info->uncompleted_bq.t_gpu_target = t_gpu_target;
			info->uncompleted_bq.risk = risk_uncompleted;
			info->uncompleted_bq.ullWnd = psHead->ullWnd;
			info->uncompleted_bq.pid = psHead->pid;
			if (psHead->uncomplete_source == GED_TIMESTAMP_TYPE_D)
				info->uncompleted_bq.useTimeStampD = true;
			else
				info->uncompleted_bq.useTimeStampD = false;
		}
	}
	return GED_TRUE;
}
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_timer_based_pick_riskyBQ(struct ged_risky_bq_info *info)
{
	GED_ERROR ret = GED_ERROR_FAIL;
	unsigned long ulIRQFlags;

	memset(info, 0, sizeof(struct ged_risky_bq_info));
	spin_lock_irqsave(&gs_hashtableLock, ulIRQFlags);
	ged_hashtable_iterator(gs_hashtable,
		ged_kpi_find_riskyBQ_func, (void *) info);
	spin_unlock_irqrestore(&gs_hashtableLock, ulIRQFlags);

	if (info->completed_bq.ullWnd == 0 ||
			info->completed_bq.t_gpu <= 0 ||
			info->completed_bq.t_gpu_target <= 0)
		// no BQ available yet or all BQ is skipped by FPSGO
		// uncompleted BQ is optional, so no need to check
		return ret;

#if IS_ENABLED(CONFIG_MTK_GPU_APO_SUPPORT)
	if (info->uncompleted_bq.t_gpu_target > 0)
		ged_get_gpu_frame_time(min(info->completed_bq.t_gpu_target, info->uncompleted_bq.t_gpu_target));
	else
		ged_get_gpu_frame_time(info->completed_bq.t_gpu_target);
#endif /* CONFIG_MTK_GPU_APO_SUPPORT */
	// t_gpu unit: ns -> us
	info->completed_bq.t_gpu = div_u64(info->completed_bq.t_gpu, 1000);
	info->completed_bq.t_gpu_target = div_u64(info->completed_bq.t_gpu_target, 1000);
	info->uncompleted_bq.t_gpu = div_u64(info->uncompleted_bq.t_gpu, 1000);
	info->uncompleted_bq.t_gpu_target = div_u64(info->uncompleted_bq.t_gpu_target, 1000);

	// if uncompleted gpu time is calculated by TimeStampD
	// allow gpu freq to decrease in fallback mode
	if (info->uncompleted_bq.useTimeStampD)
		ged_dvfs_set_uncomplete_ts_type(GED_TIMESTAMP_TYPE_D);
	else
		ged_dvfs_set_uncomplete_ts_type(GED_TIMESTAMP_TYPE_1);

	ret = GED_OK;

	/* if return OK, completed BQ's t_gpu & t_gpu_target must be non-zero, but
	 * uncompleted BQ's t_gpu & t_gpu_target may be zero
	 */
	return ret;
}

GED_ERROR ged_kpi_eb_dvfs_get_time(struct ged_risky_bq_info *info)
{
	GED_ERROR ret = GED_ERROR_FAIL;
	unsigned long ulIRQFlags;
	int i;
	struct GED_KPI_HEAD *psHead;

	memset(info, 0, sizeof(struct ged_risky_bq_info));
	spin_lock_irqsave(&gs_hashtableLock, ulIRQFlags);
	for (i = 0; i < g_ged_head_record.head_num; i++) {
		psHead = (struct GED_KPI_HEAD *)ged_hashtable_find(gs_hashtable
						, (unsigned long)g_ged_head_record.ulID[i]);
		if (psHead) {
			ged_kpi_find_riskyBQ_func((unsigned long)g_ged_head_record.ulID[i],
				(void *) psHead, (void *) info);
		}
	}
	spin_unlock_irqrestore(&gs_hashtableLock, ulIRQFlags);

	if (info->completed_bq.ullWnd == 0 ||
			info->completed_bq.t_gpu <= 0 ||
			info->completed_bq.t_gpu_target <= 0)
		// no BQ available yet or all BQ is skipped by FPSGO
		// uncompleted BQ is optional, so no need to check
		return ret;

	// t_gpu unit: ns -> us
	info->completed_bq.t_gpu = div_u64(info->completed_bq.t_gpu, 1000);
	info->completed_bq.t_gpu_target = div_u64(info->completed_bq.t_gpu_target, 1000);
	info->uncompleted_bq.t_gpu = div_u64(info->uncompleted_bq.t_gpu, 1000);
	info->uncompleted_bq.t_gpu_target = div_u64(info->uncompleted_bq.t_gpu_target, 1000);

	// if uncompleted gpu time is calculated by TimeStampD
	// allow gpu freq to decrease in fallback mode
	if (info->uncompleted_bq.useTimeStampD)
		ged_dvfs_set_uncomplete_ts_type(GED_TIMESTAMP_TYPE_D);
	else
		ged_dvfs_set_uncomplete_ts_type(GED_TIMESTAMP_TYPE_1);

	ret = GED_OK;

	/* if return OK, completed BQ's t_gpu & t_gpu_target must be non-zero, but
	 * uncompleted BQ's t_gpu & t_gpu_target may be zero
	 */
	return ret;
}

static GED_BOOL ged_kpi_update_t_gpu_latest_uncompleted_fcn(unsigned long ulID,
	void *pvoid, void *pvParam)
{
	struct GED_KPI_HEAD *psHead = (struct GED_KPI_HEAD *) pvoid;
	unsigned long long current_timestamp = *((unsigned long long *) pvParam);
	struct GED_KPI *psKPI = NULL;
	struct list_head *psListEntry, *psListEntryTemp;
	struct list_head *psList = &psHead->sList;
	long long t_gpu_uncomplete = 0;
	int i = 0;

	spin_lock(&psHead->sListLock);
	list_for_each_prev_safe(psListEntry, psListEntryTemp, psList) {
		psKPI = list_entry(psListEntry, struct GED_KPI, sList);
		if (psKPI && (psKPI->ulMask & GED_TIMESTAMP_TYPE_2) == 0) {
			// only uncompleted frame has uncompleted time
			// GPU completion start time is queue
			if (psKPI->ulMask & GED_TIMESTAMP_TYPE_1) {
				psHead->uncomplete_source = GED_TIMESTAMP_TYPE_1;
				t_gpu_uncomplete = current_timestamp - psKPI->ullTimeStamp1;
			}
			// release fence is signaled after queue
			if (g_ged_pre_fence_chk == 1 &&
				psKPI->ulMask & GED_TIMESTAMP_TYPE_P &&
				psKPI->ulMask & GED_TIMESTAMP_TYPE_1 &&
				psKPI->ullTimeStampP > psKPI->ullTimeStamp1) {
				psHead->uncomplete_source = GED_TIMESTAMP_TYPE_P;
				t_gpu_uncomplete = current_timestamp - psKPI->ullTimeStampP;
			}

			// 4/3 buffer per BQ for 120FPS/<120FPS producer
			if (++i >= 4)
				break;
		}
	}
	spin_unlock(&psHead->sListLock);
	psHead->t_gpu_latest_uncompleted = t_gpu_uncomplete;

	return GED_TRUE;
}

void ged_kpi_update_t_gpu_latest_uncompleted(void)
{
	unsigned long long current_timestamp;
	unsigned long ulIRQFlags;

	current_timestamp = ged_get_time();
	spin_lock_irqsave(&gs_hashtableLock, ulIRQFlags);
	ged_hashtable_iterator(gs_hashtable,
		ged_kpi_update_t_gpu_latest_uncompleted_fcn,
		(void *) &current_timestamp);
	spin_unlock_irqrestore(&gs_hashtableLock, ulIRQFlags);
}

int ged_kpi_get_main_bq_uncomplete_count(void)
{
	if (main_head)
		return main_head->i32Gpu_uncompleted;
	else
		return -1;
}

unsigned long long ged_kpi_get_fb_timestamp(void)
{
	return frame_base_timestamp;
}

unsigned long ged_kpi_get_fb_ulMask(void)
{
	return frame_base_ulMask;
}


/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_query_dvfs_freq_pred(int *gpu_freq_cur
	, int *gpu_freq_max, int *gpu_freq_pred)
{
#ifdef MTK_GED_KPI
	if (gpu_freq_cur == NULL
			|| gpu_freq_max == NULL
			|| gpu_freq_pred == NULL)
		return GED_ERROR_FAIL;

	*gpu_freq_cur = g_psGIFT->gpu_freq_cur;
	*gpu_freq_max = g_psGIFT->gpu_freq_max;
	*gpu_freq_pred = g_psGIFT->gpu_freq_pred;

	return GED_OK;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
EXPORT_SYMBOL(ged_kpi_query_dvfs_freq_pred);

/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_query_gpu_dvfs_info(
	struct GED_BRIDGE_OUT_QUERY_GPU_DVFS_INFO *out)
{
#ifdef MTK_GED_KPI
	if (out == NULL)
		return GED_ERROR_FAIL;

	out->gpu_freq_cur = g_psGIFT->gpu_freq_cur;
	out->gpu_freq_max = g_psGIFT->gpu_freq_max;
	out->gpu_freq_dvfs_pred = g_psGIFT->gpu_freq_pred;
	out->target_fps = g_psGIFT->target_fps;
	out->target_fps_margin = g_psGIFT->target_fps_margin;
	out->eara_fps_margin = g_psGIFT->eara_fps_margin;
	out->gpu_time = (g_psGIFT->gpu_time == -1) ? -1 : g_psGIFT->gpu_time / 1000;

	GED_LOGD("pid:%d,freq_c:%d,freq_max:%d,freq_pre:%d,tfps:%d",
		g_psGIFT->target_pid,
		out->gpu_freq_cur,
		out->gpu_freq_max,
		out->gpu_freq_dvfs_pred,
		out->target_fps);
	GED_LOGD("tfps_mar:%d,eara_diff:%d t_gpu:%d,gift_ratio:%d",
		out->target_fps_margin,
		out->eara_fps_margin,
		out->gpu_time,
		g_psGIFT->gift_ratio);

	return GED_OK;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
EXPORT_SYMBOL(ged_kpi_query_gpu_dvfs_info);
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_hint_frame_info(
	struct GED_BRIDGE_OUT_HINT_FRAME_INFO *out)
{
#ifdef MTK_GED_KPI
	if (out == NULL)
		return GED_ERROR_FAIL;

	out->eError = GED_OK;
	out->mainHead_fps_v = main_head->target_fps_v;
	out->mainHead_fps_gpu = main_head->t_gpu_fps;
	out->mainHead_BQ_ID = main_head->ullWnd;
	return GED_OK;
#else
	return GED_OK;
#endif /* MTK_GED_KPI */
}
EXPORT_SYMBOL(ged_kpi_hint_frame_info);
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_set_gift_status(int ratio)
{
#ifdef MTK_GED_KPI
	if (ratio > 0)
		g_psGIFT->gift_ratio = ratio;
	else
		g_psGIFT->gift_ratio = 0;

	return GED_OK;
#endif /* MTK_GED_KPI */
	return GED_OK;
}
EXPORT_SYMBOL(ged_kpi_set_gift_status);
/* ------------------------------------------------------------------- */
GED_ERROR ged_kpi_set_gift_target_pid(int pid)
{
#ifdef MTK_GED_KPI
	if (pid != g_psGIFT->target_pid)
		g_psGIFT->target_pid = pid;
	return GED_OK;
#endif /* MTK_GED_KPI */
	return GED_OK;
}
EXPORT_SYMBOL(ged_kpi_set_gift_target_pid);
/* ------------------------------------------------------------------- */
bool ged_kpi_get_stable_lb(void)
{
	return g_stable_lb;
}
