/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __EMI_H__
#define __EMI_H__

#include <linux/irqreturn.h>

#define MTK_EMIMPU_NO_PROTECTION	0
#define MTK_EMIMPU_SEC_RW		1
#define MTK_EMIMPU_SEC_RW_NSEC_R	2
#define MTK_EMIMPU_SEC_RW_NSEC_W	3
#define MTK_EMIMPU_SEC_R_NSEC_R		4
#define MTK_EMIMPU_FORBIDDEN		5
#define MTK_EMIMPU_SEC_R_NSEC_RW	6

#define MTK_EMIMPU_UNLOCK		false
#define MTK_EMIMPU_LOCK			true

#define MTK_EMIMPU_SET			0
#define MTK_EMIMPU_CLEAR		1
#define MTK_EMIMPU_READ			2
#define MTK_EMIMPU_SLVERR		3
#define MTK_EMIDBG_DUMP			4
#define MTK_EMIDBG_MSG			5
#define MTK_EMIMPU_CLEAR_MD             7
#define MTK_SLBMPU_CLEAR                8
#define MTK_EMIMPU_CLEAR_KP		9
#define MTK_EMI_CLEAR			10
#define MTK_SLC_PARITY_SELECT		11
#define MTK_SLC_PARITY_CLEAR		12

#define MTK_EMIMPU_READ_SA		0
#define MTK_EMIMPU_READ_EA		1
#define MTK_EMIMPU_READ_APC		2
#define MTK_EMIMPU_READ_ENABLE		3
#define MTK_EMIMPU_READ_AID		4

#define MTK_EMI_MAX_TOKEN		4
#define MTK_EMI_MAX_CMD_LEN		256
#define MTK_SLC_PARITY_MAX_LEN		512

#define EMIMPUVER1			1
#define EMIMPUVER2			2

#define SLB_VIOLATION_CB_MAX		10

struct emi_addr_map {
	int emi;
	int channel;
	int dram_chn;
	int rank;
	int bank;
	int row;
	int column;
};

struct reg_info_t {
	unsigned int offset;
	unsigned int value;
	unsigned int leng;
};

struct vio_dump_info_t {
	unsigned int vio_dump_idx;
	unsigned int vio_dump_pos;
};

struct bypass_aix_info_t {
	unsigned int port;
	unsigned int axi_mask;
	unsigned int axi_value;
};

struct emimpu_region_t {
	unsigned long long start;
	unsigned long long end;
	unsigned int rg_num;
	bool lock;
	unsigned int *apc;
};

typedef irqreturn_t (*emimpu_pre_handler)(
	unsigned int emi_id, struct reg_info_t *dump, unsigned int leng);
#if IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
typedef irqreturn_t (*emimpu_tmem_handler) (
	unsigned int emi_id, struct reg_info_t *dump, unsigned int len);
#endif /* CONFIG_MTK_EMI_LEGACY */
typedef void (*emimpu_post_clear)(unsigned int emi_id);
typedef void (*emimpu_md_handler)(const char *vio_msg);
typedef void (*emimpu_iommu_handler)(
	unsigned int emi_id, struct reg_info_t *dump, unsigned int leng);
typedef void (*emimpu_debug_dump)(void);

struct emimpu_dbg_cb {
	emimpu_debug_dump func;
	struct emimpu_dbg_cb *next_dbg_cb;
};

typedef int (*mtk_slb_violation_callback_t)(void *cb_data);

struct mtk_slb_violation_cb {
	mtk_slb_violation_callback_t fault_fn;
	void *fault_data;
};
int mtk_slb_violation_register_callback(mtk_slb_violation_callback_t fn, void *cb_data);

/* mtk emicen api */
unsigned int mtk_emicen_get_ch_cnt(void);
unsigned int mtk_emicen_get_rk_cnt(void);
unsigned long long mtk_emicen_get_rk_size(unsigned int rk_id);
int mtk_emicen_addr2dram(unsigned long addr, struct emi_addr_map *map);

/* mtk emidbg api */
void mtk_emidbg_dump(void);

/* mtk emiisu api */
void mtk_emiisu_record_off(void);
void mtk_emiisu_record_on(void);

/* mtk emimpu api */
#if IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
int emimpu_ap_region_init(void);
int mtk_emimpu_init_region(
	struct emimpu_region_t *rg_info, unsigned int rg_num);
int mtk_emimpu_set_addr(struct emimpu_region_t *rg_info,
	unsigned long long start, unsigned long long end);
int mtk_emimpu_set_apc(struct emimpu_region_t *rg_info,
	unsigned int d_num, unsigned int apc);
int mtk_emimpu_lock_region(struct emimpu_region_t *rg_info, bool lock);
int mtk_emimpu_set_protection(struct emimpu_region_t *rg_info);
int mtk_emimpu_free_region(struct emimpu_region_t *rg_info);
int mtk_emimpu_clear_protection(struct emimpu_region_t *rg_info);
int mtk_emimpu_tmem_handler_register(emimpu_tmem_handler tmem_handler_func);
/*
 * this macro CONFIG_MTK_DEVMPU_EMI and function is for 6885/6893,
 * which devmpu violation will need the both of
 * emimpu and devmpu to handle it.
 */

#if IS_ENABLED(CONFIG_MTK_DEVMPU_EMI)
int mtk_emimpu_prehandle_register(emimpu_pre_handler bypass_func);
#endif /* CONFIG_MTK_DEVMPU_EMI */

#endif /* CONFIG_MTK_EMI_LEGACY */

int mtk_emimpu_md_handling_register(emimpu_md_handler md_handling_func);
void mtk_clear_md_violation(void);
int mtk_clear_smpu_log(unsigned int emi_id);

#endif /* __EMI_H__ */
