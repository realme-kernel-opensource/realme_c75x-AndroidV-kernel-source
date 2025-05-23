/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_DCM_INTERNAL_H__
#define __MTK_DCM_INTERNAL_H__

#include <mtk_dcm_common.h>
#include "mt6899_dcm_autogen.h"

/* #define DCM_DEFAULT_ALL_OFF */

/* Note: ENABLE_DCM_IN_LK is used in kernel if DCM is enabled in LK */
#define ENABLE_DCM_IN_LK
#ifdef ENABLE_DCM_IN_LK
#define INIT_DCM_TYPE_BY_K	0
#endif

/* Note: If DCM has states other than DCM_OFF/DCM_ON, define here */
enum {
	ARMCORE_DCM_OFF = DCM_OFF,
	ARMCORE_DCM_MODE1 = DCM_ON,
	ARMCORE_DCM_MODE2 = DCM_ON+1,
};

/* Note: DCM_TYPE enums is used in DCM init & SMC calls */
enum {
	BUSDVT_DCM = 0,
	INFRA_DCM = 1,
	MCUSYS_DCM = 2,
	MCUSYS_ACP_DCM = 3,
	MCUSYS_ADB_DCM = 4,
	MCUSYS_APB_DCM = 5,
	MCUSYS_BUS_DCM = 6,
	MCUSYS_CBIP_DCM = 7,
	MCUSYS_CHI_MON_DCM = 8,
	MCUSYS_CORE_DCM = 9,
	MCUSYS_CPC_PBI_DCM = 10,
	MCUSYS_CPC_TURBO_DCM = 11,
	MCUSYS_DSU_ACP_DCM = 12,
	MCUSYS_EBG_DCM = 13,
	MCUSYS_GIC_SPI_DCM = 14,
	MCUSYS_IO_DCM = 15,
	MCUSYS_L3C_DCM = 16,
	MCUSYS_STALL_DCM = 17,
	PERI_DCM = 18,
	VLP_DCM = 19,
};

enum {
	BUSDVT_DCM_TYPE = BIT(BUSDVT_DCM),
	INFRA_DCM_TYPE = BIT(INFRA_DCM),
	MCUSYS_DCM_TYPE = BIT(MCUSYS_DCM),
	MCUSYS_ACP_DCM_TYPE = BIT(MCUSYS_ACP_DCM),
	MCUSYS_ADB_DCM_TYPE = BIT(MCUSYS_ADB_DCM),
	MCUSYS_APB_DCM_TYPE = BIT(MCUSYS_APB_DCM),
	MCUSYS_BUS_DCM_TYPE = BIT(MCUSYS_BUS_DCM),
	MCUSYS_CBIP_DCM_TYPE = BIT(MCUSYS_CBIP_DCM),
	MCUSYS_CHI_MON_DCM_TYPE = BIT(MCUSYS_CHI_MON_DCM),
	MCUSYS_CORE_DCM_TYPE = BIT(MCUSYS_CORE_DCM),
	MCUSYS_CPC_PBI_DCM_TYPE = BIT(MCUSYS_CPC_PBI_DCM),
	MCUSYS_CPC_TURBO_DCM_TYPE = BIT(MCUSYS_CPC_TURBO_DCM),
	MCUSYS_DSU_ACP_DCM_TYPE = BIT(MCUSYS_DSU_ACP_DCM),
	MCUSYS_EBG_DCM_TYPE = BIT(MCUSYS_EBG_DCM),
	MCUSYS_GIC_SPI_DCM_TYPE = BIT(MCUSYS_GIC_SPI_DCM),
	MCUSYS_IO_DCM_TYPE = BIT(MCUSYS_IO_DCM),
	MCUSYS_L3C_DCM_TYPE = BIT(MCUSYS_L3C_DCM),
	MCUSYS_STALL_DCM_TYPE = BIT(MCUSYS_STALL_DCM),
	PERI_DCM_TYPE = BIT(PERI_DCM),
	VLP_DCM_TYPE = BIT(VLP_DCM),
};

int mt_dcm_dts_map(void);

#endif /* #ifndef __MTK_DCM_INTERNAL_H__ */
