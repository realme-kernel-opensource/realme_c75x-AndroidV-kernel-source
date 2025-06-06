/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DISP_VIDLE_H__
#define __MTK_DISP_VIDLE_H__

#include "mtk_dpc.h"
#include "mtk_vdisp_common.h"


struct mtk_disp_vidle_para {
	unsigned int vidle_en;
	unsigned int vidle_init;
	unsigned int vidle_stop;
	unsigned int rc_en;
	unsigned int wdt_en;
};

#define DISP_VIDLE_TOP_EN BIT(0)	/* total V-Idle on/off */
#define DISP_VIDLE_MTCMOS_DT_EN BIT(1)
#define DISP_VIDLE_MMINFRA_DT_EN BIT(2)
#define DISP_VIDLE_DVFS_DT_EN BIT(3)
#define DISP_VIDLE_QOS_DT_EN BIT(4)
#define DISP_VIDLE_GCE_TS_EN BIT(5)
#define DISP_DPC_PRE_TE_EN BIT(6)

/* V-idle stop case */
#define VIDLE_STOP_DEBUGE           BIT(0)
#define VIDLE_STOP_MULTI_CRTC       BIT(1)
#define VIDLE_STOP_LCM_DISCONNECT   BIT(2)
#define VIDLE_STOP_VDO_HIGH_FPS     BIT(3)

struct mtk_disp_dpc_data {
	struct mtk_disp_vidle_para *mtk_disp_vidle_flag;
};

bool mtk_vidle_is_ff_enabled(void);
void mtk_set_dt_configure(u8 dt, unsigned int us);
int mtk_vidle_update_dt_by_type(void *_crtc, enum mtk_panel_type type);
int mtk_vidle_update_dt_by_period(void *_crtc, unsigned int dur_frame, unsigned int dur_vblank);
void mtk_vidle_sync_mmdvfsrc_status_rc(unsigned int rc_en);
void mtk_vidle_sync_mmdvfsrc_status_wdt(unsigned int wdt_en);
void mtk_vidle_flag_init(void *crtc);
void mtk_vidle_enable(bool en, void *drm_priv);
void mtk_vidle_force_enable_mml(bool en);
void mtk_vidle_clear_wfe_event(enum mtk_vidle_voter_user user, struct cmdq_pkt *pkt, int event);
int mtk_vidle_user_power_keep(enum mtk_vidle_voter_user user);
void mtk_vidle_user_power_release(enum mtk_vidle_voter_user user);
void mtk_vidle_user_power_keep_by_gce(enum mtk_vidle_voter_user user,
				      struct cmdq_pkt *pkt, u16 gpr);
void mtk_vidle_user_power_release_by_gce(enum mtk_vidle_voter_user user, struct cmdq_pkt *pkt);
int mtk_vidle_force_power_ctrl_by_cpu(bool power_on);
int mtk_vidle_pq_power_get(const char *caller);
void mtk_vidle_pq_power_put(const char *caller);
void mtk_set_vidle_stop_flag(unsigned int flag, unsigned int stop);
void mtk_vidle_set_all_flag(unsigned int en, unsigned int stop);
void mtk_vidle_get_all_flag(unsigned int *en, unsigned int *stop);
void mtk_vidle_hrt_bw_set(const u32 bw_in_mb);
void mtk_vidle_srt_bw_set(const u32 bw_in_mb);
void mtk_vidle_dvfs_set(const u8 level);
void mtk_vidle_dvfs_bw_set(const u32 bw_in_mb);
void mtk_vidle_dvfs_trigger(const char *caller);
void mtk_vidle_register(const struct dpc_funcs *funcs, enum mtk_dpc_version version);
void mtk_vidle_config_ff(bool en);
void mtk_vidle_dpc_analysis(void);
void mtk_vidle_dpc_pm_analysis(void);
void mtk_vidle_debug_cmd_adapter(const char *opt);
void mtk_vidle_set_panel_type(enum mtk_panel_type type);
void mtk_vidle_dsi_pll_set(const u32 value);
void mtk_vidle_channel_bw_set(const u32 bw_in_mb, const u32 idx);

void mtk_vdisp_register(const struct mtk_vdisp_funcs *fp, enum mtk_vdisp_version version);
void mtk_vidle_wait_init(void);
int mtk_vidle_get_power_if_in_use(void);
void mtk_vidle_put_power(void);

#endif
