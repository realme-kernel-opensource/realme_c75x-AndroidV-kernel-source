/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __EMI_MPU_H__
#define __EMI_MPU_H__

#include <linux/types.h>
#include <soc/mediatek/emi.h>

#define WRITE_SRINFO		0
#define READ_SRINFO		1
#define WRITE_AXI		2
#define READ_AXI		3
#define WRITE_AXI_MSB		4
#define READ_AXI_MSB		5
#define AXI_SET_NUM(num)	(num/3)

#ifndef CHECK_BIT
#define CHECK_BIT(var, pos) ((var) & (1<<(pos)))
#endif

typedef irqreturn_t (*emimpu_isr_hook)(
	unsigned int emi_id, struct reg_info_t *dump, unsigned int leng);

struct emi_mpu {
	unsigned long long dram_start;
	unsigned long long dram_end;
	unsigned int region_cnt;
	unsigned int domain_cnt;
	unsigned int addr_align;
	unsigned int ctrl_intf;
	#if IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
	unsigned int bypass;
	#endif /* CONFIG_MTK_EMI_LEGACY */

	unsigned int dump_cnt;
	unsigned int miukp_dump_cnt;
	unsigned int miumpu_dump_cnt;
	unsigned int miumpu_vio_dump_cnt;
	unsigned int kp_vio_dump_cnt;

	struct emimpu_region_t *ap_rg_info;

	struct reg_info_t *dump_reg;
	struct reg_info_t *miukp_dump_reg;
	struct reg_info_t *miumpu_dump_reg;

	struct vio_dump_info_t *miumpu_vio_dump_info;
	struct vio_dump_info_t *kp_vio_dump_info;

	struct reg_info_t *clear_reg;
	struct reg_info_t *clear_md_reg;
	struct reg_info_t *clear_hp_reg;
	struct reg_info_t *clear_miukp_reg;
	struct reg_info_t *clear_miumpu_reg;

	unsigned int clear_reg_cnt;
	unsigned int clear_md_reg_cnt;
	unsigned int clear_hp_reg_cnt;
	unsigned int clear_miukp_reg_cnt;
	unsigned int clear_miumpu_reg_cnt;

	unsigned int emi_cen_cnt;
	void __iomem **emi_cen_base;
	void __iomem **emi_mpu_base;
	void __iomem **miu_kp_base;
	void __iomem **miu_mpu_base;

	/* interrupt id */
	unsigned int irq;

	/* hook functions in ISR */
	emimpu_pre_handler pre_handler;
	emimpu_post_clear post_clear;
	emimpu_md_handler md_handler;
	#if IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
	emimpu_tmem_handler tmem_handler;
	#endif /* CONFIG_MTK_EMI_LEGACY */
	emimpu_iommu_handler iommu_handler;
	emimpu_isr_hook by_plat_isr_hook;

	/* debugging log for EMI MPU violation */
	char *vio_msg;
	unsigned int in_msg_dump;

	/* hook functions in worker thread */
	struct emimpu_dbg_cb *dbg_cb_list;

	/* EMIMPU VERSION */
	unsigned int version;

	/* bypass list*/
	unsigned int *bypass_miu_reg;
	unsigned int bypass_miu_reg_num;
	struct bypass_aix_info_t *bypass_axi;
	unsigned int bypass_axi_num;

	/* bypass for legacy emimpu */
	unsigned int bypass_r_cnt;
	unsigned int *bypass_r_axi;
};

extern struct emi_mpu *global_emi_mpu;

int mtk_emimpu_isr_hook_register(emimpu_isr_hook hook);

#endif /* __EMI_MPU_H__ */
