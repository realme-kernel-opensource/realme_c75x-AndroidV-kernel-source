/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MML_PQ_CORE_H__
#define __MTK_MML_PQ_CORE_H__

#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/types.h>
#include <linux/wait.h>

#include "mtk-mml-core.h"

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

#define HDR_CURVE_NUM (1024)
#define AAL_CURVE_NUM (544)
#define AAL_HIST_NUM (768)
#define AAL_DUAL_INFO_NUM (16)
#define AAL_CLARITY_STATUS_NUM (7)
#define C3D_LUT_NUM (729) // 9*9*9

#define CMDQ_GPR_UPDATE	(2)

#define HDR_HIST_NUM (58)

#define TDSHP_CONTOUR_HIST_NUM (17)
#define TDSHP_CLARITY_STATUS_NUM (12)
#define GCE_THREAD_START (2)

#define MML_PQ_RB_ENGINE (2)
#define MAX_ENG_RB_BUF (8)
#define TOTAL_RB_BUF_NUM (MML_PQ_RB_ENGINE*MML_PIPE_CNT*MAX_ENG_RB_BUF)
#define INVALID_OFFSET_ADDR (4096*TOTAL_RB_BUF_NUM)
/*
 * FG_BUF_SIZE = luma_grain_size(11984) +
 *               cb_grain_size(11984) +
 *               cr_grain_size(11984) +
 *               SCALING_LUT_SIZE(1024) * 3
 */
#define FG_BUF_GRAIN_SIZE (11984)
#define FG_BUF_SCALING_SIZE (3072)
#define FG_BUF_SIZE (39024)
#define FG_BUF_NUM (4)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define DB_OPT_MML_PQ	(DB_OPT_DEFAULT | DB_OPT_PROC_CMDQ_INFO | \
		DB_OPT_MMPROFILE_BUFFER | DB_OPT_FTRACE | DB_OPT_DUMP_DISPLAY)

#define MML_PQ_LINK_MAX (127)
#define mml_pq_aee(fmt, args...) _mml_log("[mml_pq][aee]" fmt, ##args)

#define mml_pq_util_aee(module, fmt, args...) \
	do { \
		char tag[MML_PQ_LINK_MAX]; \
		int len = snprintf(tag, MML_PQ_LINK_MAX, "CRDISPATCH_KEY:%s", module); \
		if (len >= LINK_MAX) \
			pr_debug("%s %d len:%d over max:%d\n", \
				__func__, __LINE__, len, MML_PQ_LINK_MAX); \
		mml_pq_aee(fmt, ##args); \
		_aee_api(DB_OPT_MML_PQ, tag, fmt, ##args); \
	} while (0)
#endif

extern int mml_pq_ir_log;

#define mml_pq_ir_log(fmt, args...) \
do { \
	if (mml_pq_ir_log) \
		_mml_log("[pq]" fmt, ##args); \
} while (0)

extern int mml_pq_msg;

#define mml_pq_msg(fmt, args...) \
do { \
	if (mml_pq_msg) \
		_mml_log("[pq]" fmt, ##args); \
} while (0)

#define mml_pq_log(fmt, args...) _mml_log("[pq]" fmt, ##args)

#define mml_pq_err(fmt, args...) \
do { \
	_mml_log("[pq][err]" fmt, ##args); \
} while (0)

extern int mml_pq_dump;

#define mml_pq_dump(fmt, args...) \
do { \
	if (mml_pq_dump) \
		_mml_log("[pq][param_dump]" fmt, ##args); \
} while (0)

extern int mml_pq_rb_msg;

#define mml_pq_rb_msg(fmt, args...) \
do { \
	if (mml_pq_rb_msg) \
		_mml_log("[pq][rb_flow]" fmt, ##args); \
} while (0)

extern int mml_pq_set_msg;

#define mml_pq_set_msg(fmt, args...) \
do { \
	if (mml_pq_set_msg) \
		_mml_log("[pq][set_dump]" fmt, ##args); \
} while (0)

/* mml pq ftrace */
extern int mml_pq_trace;

#define mml_pq_trace_ex_begin(fmt, args...) do { \
	if (mml_pq_trace) \
		mml_trace_begin(fmt, ##args); \
} while (0)

#define mml_pq_trace_ex_end() do { \
	if (mml_pq_trace) \
		mml_trace_end(); \
} while (0)

extern int mml_pq_buf_num;

extern int mml_pq_debug_mode;

struct mml_task;

enum mml_pq_debug_mode {
	MML_PQ_DEBUG_OFF = 0,
	MML_PQ_SET_TEST = 1 << 1,
	MML_PQ_STABILITY_TEST = 1 << 2,
	MML_PQ_HIST_CHECK = 1 << 3,
	MML_PQ_CURVE_CHECK = 1 << 4,
	MML_PQ_TIMEOUT_TEST = 1 << 5,
	MML_PQ_BUFFER_CHECK = 1 << 6,
	MML_PQ_FG_HW_AR_DBG = 1 << 7,
};

enum mml_pq_vcp_engine {
	MML_PQ_HDR0 = 0,
	MML_PQ_HDR1,
	MML_PQ_AAL0,
	MML_PQ_AAL1,
};

enum mml_pq_readback_engine {
	MML_PQ_HDR = 0,
	MML_PQ_AAL,
	MML_PQ_DC,
};

enum mml_pq_clarity_hist_start {
	AAL_CLARITY_HIST_START = 0,
	TDSHP_CLARITY_HIST_START = AAL_CLARITY_STATUS_NUM,
};

enum mml_pq_hist_read_status {
	MML_PQ_HIST_INIT,
	MML_PQ_HIST_READING,
	MML_PQ_HIST_IDLE,
};

enum mml_pq_rb_mode {
	RB_EOF_MODE = 0,
	RB_SOF_MODE,
};

struct mml_pq_readback_buffer {
	dma_addr_t pa;
	u32 *va;
	u32 va_offset;
	struct list_head buffer_list;
};

struct mml_pq_dma_buffer {
	u32 *va;
	dma_addr_t pa;
	struct list_head buffer_list;
};

struct mml_pq_readback_data {
	bool is_dual;
	atomic_t pipe_cnt;
	u32 cut_pos_x;
	u32 *pipe0_hist;
	u32 *pipe1_hist;
};

struct mml_pq_frame_data {
	struct mml_frame_info info;
	struct mml_pq_param pq_param[MML_MAX_OUTPUTS];
	struct mml_frame_size frame_out[MML_MAX_OUTPUTS];
	struct mml_pq_frame_info size_info;
};

struct mml_pq_sub_task {
	struct mutex lock;
	atomic_t queued;
	void *result;
	atomic_t result_ref;
	struct mml_pq_readback_data readback_data;
	struct mml_pq_frame_data *frame_data;
	struct wait_queue_head wq;
	struct list_head mbox_list;
	bool job_cancelled;
	bool first_job;
	bool aee_dump_done;
	u32 mml_task_jobid;
	u64 job_id;
};

struct mml_pq_read_status {
	u32 aal_comp;
	u32 hdr_comp;
	u32 tdshp_comp;
};

struct mml_pq_task {
	struct mml_task *task;
	struct mutex buffer_mutex;
	struct mutex aal_comp_lock;
	struct mutex hdr_comp_lock;
	struct mutex fg_buffer_mutex;
	struct mutex tdshp_comp_lock;
	struct completion aal_hist_done[MML_PIPE_CNT];
	struct mml_pq_readback_buffer *aal_hist[MML_PIPE_CNT];
	struct mml_pq_readback_buffer *hdr_hist[MML_PIPE_CNT];
	struct mml_pq_readback_buffer *tdshp_hist[MML_PIPE_CNT];
	struct mml_pq_dma_buffer *fg_table[FG_BUF_NUM]; /* y, cb, cr, scaling*/
	struct mml_pq_read_status read_status;
	struct completion hdr_curve_ready[MML_PIPE_CNT];
	struct mutex ref_lock;
	struct kref ref;
	struct mml_pq_sub_task tile_init;
	struct mml_pq_sub_task comp_config;
	struct mml_pq_sub_task aal_readback;
	struct mml_pq_sub_task hdr_readback;
	struct mml_pq_sub_task wrot_callback;
	struct mml_pq_sub_task clarity_readback;
	struct mml_pq_sub_task dc_readback;
};

/*
 * mml_pq_core_init - Initialize PQ core
 *
 * Return:	Error of initialization
 */
int mml_pq_core_init(void);

/*
 * mml_pq_core_uninit - Uninitialize PQ core
 */
void mml_pq_core_uninit(void);

/*
 * mml_pq_task_create - create and initial pq task
 *
 * @task:	task data, include pq parameters and frame info
 *
 * Return:	if value < 0, create pq task failed should debug
 */
s32 mml_pq_task_create(struct mml_task *task);

/*
 * mml_pq_task_release - for adaptor to call before destroy mml_task
 *
 * @task:	task data, include pq_task inside
 */
void mml_pq_task_release(struct mml_task *task);

/*
 * mml_pq_get_vcp_buf_offset - get vcp readback buffer offset
 *
 * @task:	task data, include pq_task inside
 * @engine:	module id, engine
 * @hist	hist id, readback hist
 */
void mml_pq_get_vcp_buf_offset(struct mml_task *task, u32 engine,
			       struct mml_pq_readback_buffer *hist);

/*
 * mml_pq_put_vcp_buf_offset - put vcp readback buffer offset
 *
 * @task:	task data, include pq_task inside
 * @engine:	module id, know engine id
 * @hist	hist id, readback hist
 */
void mml_pq_put_vcp_buf_offset(struct mml_task *task, u32 engine,
			       struct mml_pq_readback_buffer *hist);

/*
 * mml_pq_get_readback_buffer - get readback buffer
 *
 * @task:	task data, include pq_task inside
 * @pipe:	pipe id, use in dual pipe
 * @engine	engine id, readback engine
 */
void mml_pq_get_readback_buffer(struct mml_task *task, u8 pipe,
				struct mml_pq_readback_buffer **hist);

/*
 * mml_pq_get_readback_buffer - put readback buffer
 *
 * @task:	task data, include pq_task inside
 * @engine	engine id, readback engine
 */
void mml_pq_put_readback_buffer(struct mml_task *task, u8 pipe,
				struct mml_pq_readback_buffer **hist);

/*
 * mml_pq_get_fg_buffer - get fg buffer
 *
 * @task:	task data, include pq_task inside
 * @pipe:	pipe id, use in dual pipe
 * @dev:	device for getting buffer
 */
void mml_pq_get_fg_buffer(struct mml_task *task, u8 pipe,
				struct device *dev);

/*
 * mml_pq_put_fg_buffer - put fg buffer
 *
 * @task:	task data, include pq_task inside
 * @pipe:	pipe id, use in dual pipe
 * @dev:	device for putting buffer
 */
void mml_pq_put_fg_buffer(struct mml_task *task, u8 pipe,
				struct device *dev);

/*
 * mml_pq_set_tile_init - noify from MML core through MML PQ driver
 *	to update frame information for tile start
 *
 * @task:	task data, include pq parameters and frame info
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
int mml_pq_set_tile_init(struct mml_task *task);

/*
 * mml_pq_get_tile_init_result - wait for result
 *
 * @task:	task data, include pq parameters and frame info
 * @timeout_ms:	timeout setting to get result, unit: ms
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
int mml_pq_get_tile_init_result(struct mml_task *task, u32 timeout_ms);

/*
 * mml_pq_put_tile_init_result - put away result
 *
 * @task:	task data, include pq parameters and frame info
 */
void mml_pq_put_tile_init_result(struct mml_task *task);

/*
 * mml_pq_set_comp_config - noify from MML core through MML PQ driver
 *	to update frame information for frame config
 *
 * @task:	task data, include pq parameters and frame info
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
int mml_pq_set_comp_config(struct mml_task *task);

/*
 * mml_pq_get_comp_config_result - wait for result
 *
 * @task:	task data, include pq parameters and frame info
 * @timeout_ms:	timeout setting to get result, unit: ms
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
int mml_pq_get_comp_config_result(struct mml_task *task, u32 timeout_ms);

/*
 * mml_pq_put_comp_config_result - put away result
 *
 * @task:	task data, include pq parameters and frame info
 */
void mml_pq_put_comp_config_result(struct mml_task *task);

/*
 * mml_pq_sub_task_clear - remove invalid sub_task from list
 *
 * @task:	task data, include pq parameters and frame info
 */
void mml_pq_comp_config_clear(struct mml_task *task);

/*
 * mml_pq_set_aal_status - decide aal hist status by flag status
 *
 * @pq_task:	pq task data, include sub_task info
 * @out_idx: MML output info
 *
 */
void mml_pq_set_aal_status(struct mml_pq_task *pq_task, u8 out_idx);

/*
 * mml_pq_aal_hist_reading - return aal histogram reading status
 *
 * @pq_task:	pq task data, include sub_task info
 * @out_idx: MML output info
 * @pipe: pipe info
 *
 * Return:	if true, means aal histogram is reading, need to skip
 */
bool mml_pq_aal_hist_reading(struct mml_pq_task *pq_task, u8 out_idx, u8 pipe);

/*
 * mml_pq_aal_flag_check - check aal flag reset or not
 *
 * @dual:	dual pipe or single pipe info
 * @out_idx: MML output info
 *
 */
void mml_pq_aal_flag_check(bool dual, u8 out_idx);


/*
 * mml_pq_ir_aal_readback - noify from MML core through MML PQ driver
 *	to update histogram in IR/DL
 *
 * @pq_task:	pq task data, include sub_task info
 * @frame_data: frame related data
 * @pipe:	pipe id
 * @phist:	Histogram result
 * @mml_jobid: mml jobid
 * @dual: dual pipe flag
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
#if !IS_ENABLED(CONFIG_MTK_MML_LEGACY)
int mml_pq_ir_aal_readback(struct mml_pq_task *pq_task,
			 struct mml_pq_frame_data frame_data,
			 u8 pipe, u32 *phist, u32 mml_jobid,
			 bool dual);
#endif

/*
 * mml_pq_dc_aal_readback - noify from MML core through MML PQ driver
 *	to update histogram in DC
 *
 * @task:	task data, include pq parameters and frame info
 * @pipe:	pipe id
 * @phist:	Histogram result
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
int mml_pq_dc_aal_readback(struct mml_task *task, u8 pipe, u32 *phist);

/*
 * mml_pq_set_aal_status - decide hdr hist status by flag status
 *
 * @pq_task:	pq task data, include sub_task info
 * @out_idx: MML output info
 *
 */
void mml_pq_set_hdr_status(struct mml_pq_task *pq_task, u8 out_idx);

/*
 * mml_pq_hdr_flag_check - check hdr flag reset or not
 *
 * @dual:	dual pipe or single pipe info
 * @out_idx: MML output info
 *
 */
void mml_pq_hdr_flag_check(bool dual, u8 out_idx);

/*
 * mml_pq_aal_hist_reading - return hdr histogram reading status
 *
 * @pq_task:	pq task data, include sub_task info
 * @out_idx: MML output info
 * @pipe: pipe info
 *
 * Return:	if true, means aal histogram is reading, need to skip
 */
bool mml_pq_hdr_hist_reading(struct mml_pq_task *pq_task, u8 out_idx, u8 pipe);

/*
 * mml_pq_ir_aal_readback - noify from MML core through MML PQ driver
 *	to update histogram in IR/DL
 *
 * @pq_task:	pq task data, include sub_task info
 * @frame_data: frame related data
 * @pipe:	pipe id
 * @phist:	Histogram result
 * @mml_jobid: mml jobid
 * @dual: dual pipe flag
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
#if !IS_ENABLED(CONFIG_MTK_MML_LEGACY)
int mml_pq_ir_hdr_readback(struct mml_pq_task *pq_task,
			 struct mml_pq_frame_data frame_data,
			 u8 pipe, u32 *phist, u32 mml_jobid,
			 bool dual);
#endif

/*
 * mml_pq_hdr_readback - noify from MML core through MML PQ driver
 *   to update histogram
 *
 * @task:	task data, include pq parameters and frame info
 * @pipe:   pipe id
 * @phist:  Histogram result
 *
 * Return:	if value < 0, means PQ update failed should debug
 */

int mml_pq_dc_hdr_readback(struct mml_task *task, u8 pipe, u32 *phist);

/*
 * mml_pq_ir_wrot_callback - noify from MML core through MML PQ driver
 *   that second output finished
 *
 * @pq_task:	pq task data, include sub_task info
 * @frame_data: frame related data
 * @mml_jobid: mml jobid
 * @dual: dual pipe flag
 *
 * Return:	if value < 0, means PQ update failed should debug
 */

#if !IS_ENABLED(CONFIG_MTK_MML_LEGACY)
int mml_pq_ir_wrot_callback(struct mml_pq_task *pq_task, struct mml_pq_frame_data frame_data,
			u32 mml_jobid, bool dual);
#endif

/*
 * mml_pq_wrot_callback - noify from MML core through MML PQ driver
 *   that second output finished
 *
 * @task:	task data, include pq parameters and frame info
 *
 * Return:	if value < 0, means PQ update failed should debug
 */

int mml_pq_wrot_callback(struct mml_task *task);

/*
 * mml_pq_set_tdshp_status - decide tdshp hist status by flag status
 *
 * @pq_task:	pq task data, include sub_task info
 * @out_idx: MML output info
 *
 */
void mml_pq_set_tdshp_status(struct mml_pq_task *pq_task, u8 out_idx);

/*
 * mml_pq_tdshp_hist_reading - return aal histogram reading status
 *
 * @pq_task:	pq task data, include sub_task info
 * @out_idx: MML output info
 * @pipe: pipe info
 *
 * Return:	if true, means aal histogram is reading, need to skip
 */
bool mml_pq_tdshp_hist_reading(struct mml_pq_task *pq_task, u8 out_idx, u8 pipe);

/*
 * mml_pq_tdshp_flag_check - check aal flag reset or not
 *
 * @dual:	dual pipe or single pipe info
 * @out_idx: MML output info
 *
 */
void mml_pq_tdshp_flag_check(bool dual, u8 out_idx);

/*
 * mml_pq_dc_readback - noify from MML core through MML PQ driver
 *   to update histogram
 *
 * @task:	task data, include pq parameters and frame info
 * @pipe:   pipe id
 * @phist:  Histogram result
 *
 * Return:	if value < 0, means PQ update failed should debug
 */

int mml_pq_dc_readback(struct mml_task *task, u8 pipe, u32 *phist);

/*
 * mml_pq_ir_dc_readback - noify from MML core through MML PQ driver
 *	to update histogram in IR/DL
 *
 * @pq_task:	pq task data, include sub_task info
 * @frame_data: frame related data
 * @pipe:	pipe id
 * @phist:	Histogram result
 * @mml_jobid: mml jobid
 * @arr_idx: Start idx of a histogram array for storing Histogram result
 * @dual: dual pipe flag
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
#if !IS_ENABLED(CONFIG_MTK_MML_LEGACY)
int mml_pq_ir_dc_readback(struct mml_pq_task *pq_task,
			 struct mml_pq_frame_data frame_data,
			 u8 pipe, u32 *phist, u32 mml_jobid,
			 u32 arr_idx, bool dual);
#endif

/*
 * mml_pq_clarity_readback - noify from MML core through MML PQ driver
 *   to update histogram
 *
 * @task:	 task data, include pq parameters and frame info
 * @pipe:    pipe id
 * @phist:   Histogram result
 * @arr_idx: Start idx of a histogram array for storing Histogram result
 * @size:    Number of Histogram result
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
int mml_pq_clarity_readback(struct mml_task *task, u8 pipe, u32 *phist, u32 arr_idx, u32 size);

/*
 * mml_pq_ir_clarity_readback - noify from MML core through MML PQ driver
 *   to update histogram
 *
 * @pq_task:	pq task data, include sub_task info
 * @frame_data: frame related data
 * @pipe:	pipe id
 * @phist:	Histogram result
 * @mml_jobid: mml jobid
 * @arr_idx: Start idx of a histogram array for storing Histogram result
 * @size:    Number of Histogram result
 * @@dual: dual pipe flag
 *
 * Return:	if value < 0, means PQ update failed should debug
 */
#if !IS_ENABLED(CONFIG_MTK_MML_LEGACY)
int mml_pq_ir_clarity_readback(struct mml_pq_task *pq_task, struct mml_pq_frame_data frame_data,
			u8 pipe, u32 *phist, u32 mml_jobid, u32 size, u32 arr_idx,
			bool dual);
#endif
/*
 * mml_pq_reset_hist_status - reset pq histogram use status
 *
 * @task:	task data, include pq parameters and frame info
 *
 */
void mml_pq_reset_hist_status(struct mml_task *task);

/*
 * mml_pq_get_pq_task - get pq_task & increase kret
 *
 * @pq_task:	pq task data, include sub_task info
 *
 */
void mml_pq_get_pq_task(struct mml_pq_task *pq_task);

/*
 * mml_pq_put_pq_task - reset pq histogram use status
 *
 * @pq_task:	pq task data, include sub_task info
 *
 * Return:	if value = 1, pq will be released
 */
int mml_pq_put_pq_task(struct mml_pq_task *pq_task);

#endif	/* __MTK_MML_PQ_CORE_H__ */
