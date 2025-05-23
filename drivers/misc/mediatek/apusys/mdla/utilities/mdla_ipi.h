/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_IPI_H__
#define __MDLA_IPI_H__

#include <linux/types.h>

enum MDLA_IPI_TYPE_0 {
	/* kernel to uP */
	MDLA_IPI_PWR_TIME,
	MDLA_IPI_TIMEOUT,
	MDLA_IPI_ULOG,
	MDLA_IPI_TRACE_ENABLE,
	MDLA_IPI_CMD_CHECK,
	MDLA_IPI_PREEMPT_CNT,
	MDLA_IPI_PMU_COUNT,
	MDLA_IPI_ADDR,
	MDLA_IPI_PROFILE_EN,
	MDLA_IPI_FORCE_PWR_ON,
	MDLA_IPI_DUMP_CMDBUF_EN,
	MDLA_IPI_INFO,

	/* uP to kernel */
	MDLA_IPI_MICROP_MSG,

	/* platform message */
	MDLA_IPI_PLAT,

	/* sw halt */
	MDLA_IPI_HALT_STA,

	MDLA_IPI_DBG_OPTIONS,

	NF_MDLA_IPI_TYPE_0
};


enum MDLA_IPI_ADDR_TYPE_1 {
	MDLA_IPI_ADDR_BOOT,
	MDLA_IPI_ADDR_MAIN,
	MDLA_IPI_ADDR_DBG_DATA,
	MDLA_IPI_ADDR_BACKUP_DATA,
	MDLA_IPI_ADDR_RV_DATA,

	MDLA_IPI_ADDR_BOOT_SZ,
	MDLA_IPI_ADDR_MAIN_SZ,
	MDLA_IPI_ADDR_DBG_DATA_SZ,
	MDLA_IPI_ADDR_BACKUP_DATA_SZ,
	MDLA_IPI_ADDR_RV_DATA_SZ,

	NF_MDLA_IPI_ADDR_TYPE_1
};


enum MDLA_IPI_INFO_TYPE_1 {
	MDLA_IPI_INFO_PWR,
	MDLA_IPI_INFO_REG,
	MDLA_IPI_INFO_CMDBUF,
	MDLA_IPI_INFO_PROF,

	NF_MDLA_IPI_INFO_TYPE_1
};

enum MDLA_IPI_MICROP_MSG_TYPE_1 {
	MDLA_IPI_MICROP_MSG_TIMEOUT,
	MDLA_IPI_MICROP_MSG_DBG_INFO,
	MDLA_IPI_MICROP_MSG_DBG_CHECK_FAILED,
	MDLA_IPI_MICROP_MSG_CMD_FAILED,

	/* for MDLA5.5, WDEC can only be acquired by one physical core (HW constraint).
	 * If multiple WDEC cmd be executed concurrently, MDLA driver will return -EBUSY to MDW and trigger
	 * AEE_DB with this error type to identify the event occurs
	 */
	MDLA_IPI_MICROP_MSG_WDEC_RESOURCE_BUSY,

	NF_MDLA_IPI_MICROP_MSG_TYPE_1
};

enum MDLA_IPI_DIR_TYPE {
	MDLA_IPI_READ,
	MDLA_IPI_WRITE,
};


int mdla_ipi_send(int type_0, int type_1, u64 val);
int mdla_ipi_recv(int type_0, int type_1, u64 *val);
int mdla_ipi_init(void);
void mdla_ipi_deinit(void);

#endif /* __MDLA_IPI_H__ */

