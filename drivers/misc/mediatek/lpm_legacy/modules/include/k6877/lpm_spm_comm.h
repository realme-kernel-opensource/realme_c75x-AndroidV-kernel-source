/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __MT6877_SPM_COMM_H__
#define __MT6877_SPM_COMM_H__

#include <lpm_dbg_common_v1.h>
#include <lpm_pcm_def.h>

/* Determine for user name handle */
#define MT_LP_RQ_USER_NAME_LEN	(4)
#define MT_LP_RQ_USER_CHAR_U	(8)
#define MT_LP_RQ_USER_CHAR_MASK	(0xFF)

/* Determine for resource usage id */
#define MT_LP_RQ_ID_ALL_USAGE	(-1)

extern void __iomem *lpm_spm_base;

struct lpm_spm_wake_status {
	u32 r12;			/* SPM_BK_WAKE_EVENT */
	u32 r12_ext;		/* SPM_WAKEUP_EXT_STA */
	u32 raw_sta;		/* SPM_WAKEUP_STA */
	u32 raw_ext_sta;	/* SPM_WAKEUP_EXT_STA */
	u32 md32pcm_wakeup_sta;/* MD32CPM_WAKEUP_STA */
	u32 md32pcm_event_sta;/* MD32PCM_EVENT_STA */
	u32 wake_misc;		/* SPM_BK_WAKE_MISC */
	u32 timer_out;		/* SPM_BK_PCM_TIMER */
	u32 r13;			/* PCM_REG13_DATA */
	u32 idle_sta;		/* SUBSYS_IDLE_STA */
	u32 req_sta0;		/* SRC_REQ_STA_0 */
	u32 req_sta1;		/* SRC_REQ_STA_1 */
	u32 req_sta2;		/* SRC_REQ_STA_2 */
	u32 req_sta3;		/* SRC_REQ_STA_3 */
	u32 req_sta4;		/* SRC_REQ_STA_4 */
	u32 req_sta5;		/* SRC_REQ_STA_5 */
	u32 cg_check_sta;	/* SPM_CG_CHECK_STA */
	u32 debug_flag;		/* PCM_WDT_LATCH_SPARE_0 */
	u32 debug_flag1;	/* PCM_WDT_LATCH_SPARE_1 */
	u32 b_sw_flag0;		/* SPM_SW_RSV_7 */
	u32 b_sw_flag1;		/* SPM_SW_RSV_8 */
	u32 isr;			/* SPM_IRQ_STA */
	u32 sw_flag0;		/* SPM_SW_FLAG_0 */
	u32 sw_flag1;		/* SPM_SW_FLAG_1 */
	u32 clk_settle;		/* SPM_CLK_SETTLE */
	u32 src_req;	/* SPM_SRC_REQ */
	u32 log_index;
	u32 is_abort;
	u32 rt_req_sta0; /* SPM_SW_RSV_2 */
	u32 rt_req_sta1; /* SPM_SW_RSV_3 */
	u32 rt_req_sta2; /* SPM_SW_RSV_4 */
	u32 rt_req_sta3; /* SPM_SW_RSV_5 */
	u32 rt_req_sta4; /* SPM_SW_RSV_6 */
	u32 mcupm_req_sta;
};

struct spm_wakesrc_irq_list {
	unsigned int wakesrc;
	const char *name;
	int order;
	unsigned int irq_no;
};


enum MTK_LPM_SUSPEND_TYPE {
	MTK_LPM_SUSPEND_SYSTEM,
	MTK_LPM_SUSPEND_S2IDLE,
};



#define MTK_LP_PREPARE_FAIL		(1<<4L)

/* Determine for operand bit */

#define MTK_DUMP_LP_GOLDEN	(1 << 0L)
#define MTK_DUMP_GPIO		(1 << 1L)
#define PCM_32K_TICKS_PER_SEC	(32768)
#define PCM_TICK_TO_SEC(TICK)	(TICK / PCM_32K_TICKS_PER_SEC)
extern void mtk_suspend_gpio_dbg(void);
extern void mtk_suspend_clk_dbg(void);
extern u32 mt_irq_get_pending(unsigned int irq);
extern u64 spm_26M_off_count;
extern u64 spm_26M_off_duration;
extern u64 ap_pd_count;
extern u64 ap_slp_duration;
int mtk_lpm_suspend_type_get(void);
int lpm_spm_fs_init(void);
int lpm_spm_fs_deinit(void);


/*
 * Auto generated by DE, please DO NOT modify this file directly.
 * From Legacy sleep_def.h
 */
/* --- SPM Flag Define --- */
#define SPM_FLAG_DISABLE_CPU_PDN		(1 << 0u)
#define SPM_FLAG_DISABLE_INFRA_PDN		(1 << 1u)
#define SPM_FLAG_DISABLE_DDRPHY_PDN		(1 << 2u)
#define SPM_FLAG_DISABLE_VCORE_DVS		(1 << 3u)
#define SPM_FLAG_DISABLE_VCORE_DFS		(1 << 4u)
#define SPM_FLAG_DISABLE_COMMON_SCENARIO	(1 << 5u)
#define SPM_FLAG_DISABLE_BUS_CLK_OFF		(1 << 6u)
#define SPM_FLAG_DISABLE_ARMPLL_OFF		(1 << 7u)
#define SPM_FLAG_KEEP_CSYSPWRACK_HIGH		(1 << 8u)
#define SPM_FLAG_ENABLE_LVTS_WORKAROUND		(1 << 9u)
#define SPM_FLAG_RUN_COMMON_SCENARIO		(1 << 10u)
#define SPM_FLAG_SSPM_INFRA_SLEEP_MODE		(1 << 11u)
#define SPM_FLAG_ENABLE_SPM_DBG_WDT_DUMP	(1 << 12u)
#define SPM_FLAG_USE_SRCCLKENO2			(1 << 13u)
#define SPM_FLAG_ENABLE_6315_CTRL		(1 << 14u)
#define SPM_FLAG_ENABLE_TIA_WORKAROUND		(1 << 15u)
#define SPM_FLAG_DISABLE_SYSRAM_SLEEP		(1 << 16u)
#define SPM_FLAG_DISABLE_SSPM_SRAM_SLEEP	(1 << 17u)
#define SPM_FLAG_DISABLE_MCUPM_SRAM_SLEEP	(1 << 18u)
#define SPM_FLAG_ENABLE_MD_MUMTAS			(1 << 19u)
#define SPM_FLAG_ENABLE_VOLTAGE_BIN			(1 << 20u)
#define SPM_FLAG_ENABLE_6362_CTRL			(1 << 21u)
#define SPM_FLAG_DISABLE_DRAMC_MCU_SRAM_SLEEP	(1 << 22u)
#define SPM_FLAG_DISABLE_SRAM_EVENT			(1 << 23u)
#define SPM_FLAG_DISABLE_6360_SRCLKENO2_CTRL			(1 << 24u)
#define SPM_FLAG_DISABLE_6360_VDDQ_CTRL			(1 << 25u)
#define SPM_FLAG_DISABLE_DRAMC_MCU_OFF			(1 << 26u)
#define SPM_FLAG_VTCXO_STATE			(1 << 27u)
#define SPM_FLAG_INFRA_STATE			(1 << 28u)
#define SPM_FLAG_APSRC_STATE			(1 << 29u)
#define SPM_FLAG_VRF18_STATE			(1 << 30u)
#define SPM_FLAG_DDREN_STATE			(1 << 31u)
#define SPM_FLAG_SYSTEM_POWER_STATE		(1 << 28u)
/* --- SPM Flag1 Define --- */
#define SPM_FLAG1_DISABLE_AXI_BUS_TO_26M      (1 << 0u)
#define SPM_FLAG1_DISABLE_SYSPLL_OFF          (1 << 1u)
#define SPM_FLAG1_DISABLE_PWRAP_CLK_SWITCH    (1 << 2u)
#define SPM_FLAG1_DISABLE_ULPOSC_OFF          (1 << 3u)
#define SPM_FLAG1_FW_SET_ULPOSC_ON            (1 << 4u)
#define SPM_FLAG1_RESERVED_BIT5               (1 << 5u)
#define SPM_FLAG1_ENABLE_REKICK               (1 << 6u)
#define SPM_FLAG1_RESERVED_BIT7               (1 << 7u)
#define SPM_FLAG1_RESERVED_BIT8               (1 << 8u)
#define SPM_FLAG1_RESERVED_BIT9               (1 << 9u)
#define SPM_FLAG1_DISABLE_SRCLKEN_LOW         (1 << 10u)
#define SPM_FLAG1_DISABLE_SCP_CLK_SWITCH      (1 << 11u)
#define SPM_FLAG1_RESERVED_BIT12              (1 << 12u)
#define SPM_FLAG1_RESERVED_BIT13              (1 << 13u)
#define SPM_FLAG1_RESERVED_BIT14              (1 << 14u)
#define SPM_FLAG1_RESERVED_BIT15              (1 << 15u)
#define SPM_FLAG1_RESERVED_BIT16              (1 << 16u)
#define SPM_FLAG1_RESERVED_BIT17              (1 << 17u)
#define SPM_FLAG1_RESERVED_BIT18              (1 << 18u)
#define SPM_FLAG1_RESERVED_BIT19              (1 << 19u)
#define SPM_FLAG1_DISABLE_DEVAPC_SRAM_SLEEP   (1 << 20u)
#define SPM_FLAG1_RESERVED_BIT21              (1 << 21u)
#define SPM_FLAG1_RESERVED_BIT22			(1 << 22u)
#define SPM_FLAG1_RESERVED_BIT23			(1 << 23u)
#define SPM_FLAG1_DISABLE_SCP_VREQ_MASK_CONTROL   (1 << 24u)
#define SPM_FLAG1_RESERVED_BIT25              (1 << 25u)
#define SPM_FLAG1_RESERVED_BIT26              (1 << 26u)
#define SPM_FLAG1_RESERVED_BIT27              (1 << 27u)
#define SPM_FLAG1_RESERVED_BIT28              (1 << 28u)
#define SPM_FLAG1_RESERVED_BIT29              (1 << 29u)
#define SPM_FLAG1_RESERVED_BIT30              (1 << 30u)
#define SPM_FLAG1_ENABLE_MCUPM_OFF              (1 << 31u)
/* --- SPM DEBUG Define --- */
#define SPM_DBG_DEBUG_IDX_26M_WAKE			(1 << 0u)
#define SPM_DBG_DEBUG_IDX_26M_SLEEP			(1 << 1u)
#define SPM_DBG_DEBUG_IDX_INFRA_WAKE			(1 << 2u)
#define SPM_DBG_DEBUG_IDX_INFRA_SLEEP			(1 << 3u)
#define SPM_DBG_DEBUG_IDX_APSRC_WAKE			(1 << 4u)
#define SPM_DBG_DEBUG_IDX_APSRC_SLEEP			(1 << 5u)
#define SPM_DBG_DEBUG_IDX_VRF18_WAKE			(1 << 6u)
#define SPM_DBG_DEBUG_IDX_VRF18_SLEEP			(1 << 7u)
#define SPM_DBG_DEBUG_IDX_DDREN_WAKE			(1 << 8u)
#define SPM_DBG_DEBUG_IDX_DDREN_SLEEP			(1 << 9u)
#define SPM_DBG_DEBUG_IDX_DRAM_SREF_ABORT_IN_APSRC	(1 << 10u)
#define SPM_DBG_DEBUG_IDX_MCUPM_SRAM_STATE		(1 << 11u)
#define SPM_DBG_DEBUG_IDX_SSPM_SRAM_STATE		(1 << 12u)
#define SPM_DBG_DEBUG_IDX_DRAM_SREF_ABORT_IN_DDREN	(1 << 13u)
#define SPM_DBG_DEBUG_IDX_DRAMC_MCU_SRAM_STATE		(1 << 14u)
#define SPM_DBG_DEBUG_IDX_SYSRAM_SLP			(1 << 15u)
#define SPM_DBG_DEBUG_IDX_SYSRAM_ON			(1 << 16u)
#define SPM_DBG_DEBUG_IDX_MCUPM_SRAM_SLP		(1 << 17u)
#define SPM_DBG_DEBUG_IDX_MCUPM_SRAM_ON			(1 << 18u)
#define SPM_DBG_DEBUG_IDX_SSPM_SRAM_SLP			(1 << 19u)
#define SPM_DBG_DEBUG_IDX_SSPM_SRAM_ON			(1 << 20u)
#define SPM_DBG_DEBUG_IDX_DRAMC_MCU_SRAM_SLP		(1 << 21u)
#define SPM_DBG_DEBUG_IDX_DRAMC_MCU_SRAM_ON		(1 << 22u)
#define SPM_DBG_DEBUG_IDX_APSRC_SLEEP_ABORT		(1 << 23u)
#define SPM_DBG_DEBUG_IDX_6360_VMDDR_LP_MODE		(1 << 24u)
#define SPM_DBG_DEBUG_IDX_6360_VDDQ_LP_MODE	(1 << 25u)
#define SPM_DBG_DEBUG_IDX_I2C5_NO_CLK		(1 << 26u)
#define SPM_DBG_DEBUG_IDX_SPM_GO_WAKEUP_NOW		(1 << 27u)
#define SPM_DBG_DEBUG_IDX_VTCXO_STATE			(1 << 28u)
#define SPM_DBG_DEBUG_IDX_INFRA_STATE			(1 << 29u)
#define SPM_DBG_DEBUG_IDX_VRF18_STATE			(1 << 30u)
#define SPM_DBG_DEBUG_IDX_APSRC_STATE			(1 << 31u)

/* --- SPM DEBUG1 Define --- */
#define SPM_DBG1_DEBUG_IDX_CURRENT_IS_LP		(1 << 0u)
#define SPM_DBG1_DEBUG_IDX_VCORE_DVFS_START		(1 << 1u)
#define SPM_DBG1_DEBUG_IDX_SYSPLL_OFF			(1 << 2u)
#define SPM_DBG1_DEBUG_IDX_SYSPLL_ON			(1 << 3u)
#define SPM_DBG1_DEBUG_IDX_CURRENT_IS_VCORE_DFS	(1 << 4u)
#define SPM_DBG1_DEBUG_IDX_INFRA_MTCMOS_OFF		(1 << 5u)
#define SPM_DBG1_DEBUG_IDX_INFRA_MTCMOS_ON		(1 << 6u)
#define SPM_DBG1_DEBUG_IDX_VRCXO_SLEEP_ABORT		(1 << 7u)
#define SPM_DBG1_NOISE_IRQ				(1 << 8u)
#define SPM_DBG1_IDX_6360_SRCLKEN2_LOW		(1 << 9u)
#define SPM_DBG1_IDX_6360_SRCLKEN2_HIGH		(1 << 10u)
#define SPM_DBG1_DEBUG_IDX_PWRAP_CLK_TO_ULPOSC		(1 << 11u)
#define SPM_DBG1_DEBUG_IDX_PWRAP_CLK_TO_26M		(1 << 12u)
#define SPM_DBG1_DEBUG_IDX_SCP_CLK_TO_32K		(1 << 13u)
#define SPM_DBG1_DEBUG_IDX_SCP_CLK_TO_26M		(1 << 14u)
#define SPM_DBG1_DEBUG_IDX_BUS_CLK_OFF			(1 << 15u)
#define SPM_DBG1_DEBUG_IDX_BUS_CLK_ON			(1 << 16u)
#define SPM_DBG1_DEBUG_IDX_SRCLKEN2_LOW			(1 << 17u)
#define SPM_DBG1_DEBUG_IDX_SRCLKEN2_HIGH		(1 << 18u)
#define SPM_DBG1_DEBUG_IDX_MCUPM_WAKE_IRQ		(1 << 19u)
#define SPM_DBG1_DEBUG_IDX_ULPOSC_IS_OFF_BUT_SHOULD_ON	(1 << 20u)
#define SPM_DBG1_DEBUG_IDX_6315_LOW			(1 << 21u)
#define SPM_DBG1_DEBUG_IDX_6315_HIGH                    (1 << 22u)
#define SPM_DBG1_DEBUG_IDX_PWRAP_SLEEP_ACK_LOW_ABORT    (1 << 23u)
#define SPM_DBG1_DEBUG_IDX_PWRAP_SLEEP_ACK_HIGH_ABORT   (1 << 24u)
#define SPM_DBG1_DEBUG_IDX_EMI_SLP_IDLE_ABORT           (1 << 25u)
#define SPM_DBG1_DEBUG_IDX_SCP_SLP_ACK_LOW_ABORT        (1 << 26u)
#define SPM_DBG1_DEBUG_IDX_SCP_SLP_ACK_HIGH_ABORT       (1 << 27u)
#define SPM_DBG1_DEBUG_IDX_SPM_DVFS_CMD_RDY_ABORT       (1 << 28u)
#define SPM_DBG1_DEBUG_IDX_SPM_TIMER_RST_DVFS			(1 << 29u)
#define SPM_DBG1_DEBUG_IDX_SPM_DISABLE_DDREN_EVENT		(1 << 30u)
#define MCUPM_RESTORE							(1 << 31u)


/* ABORT MASK for DEBUG FOORTPRINT */
#define DEBUG_ABORT_MASK (SPM_DBG_DEBUG_IDX_DRAM_SREF_ABORT_IN_APSRC \
	| SPM_DBG_DEBUG_IDX_DRAM_SREF_ABORT_IN_DDREN)

#define DEBUG_ABORT_MASK_1 (SPM_DBG1_DEBUG_IDX_VRCXO_SLEEP_ABORT \
	| SPM_DBG1_DEBUG_IDX_PWRAP_SLEEP_ACK_LOW_ABORT \
	| SPM_DBG1_DEBUG_IDX_PWRAP_SLEEP_ACK_HIGH_ABORT \
	| SPM_DBG1_DEBUG_IDX_EMI_SLP_IDLE_ABORT \
	| SPM_DBG1_DEBUG_IDX_SCP_SLP_ACK_LOW_ABORT \
	| SPM_DBG1_DEBUG_IDX_SCP_SLP_ACK_HIGH_ABORT \
	| SPM_DBG1_DEBUG_IDX_SPM_DVFS_CMD_RDY_ABORT)


#endif
