// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <cmdq-util.h>
#include <mtk-smmu-v3.h>

#include "mtk-mml-rdma-golden.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "tile_driver.h"
#include "mtk-mml-tile.h"
#include "tile_mdp_func.h"
#include "mtk-mml-mmp.h"

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

#define RDMA_EN				0x000
#define RDMA_UFBDC_DCM_EN		0x004
#define RDMA_RESET			0x008
#define RDMA_INTERRUPT_ENABLE		0x010
#define RDMA_INTERRUPT_STATUS		0x018
#define RDMA_CON			0x020
#define RDMA_SHADOW_CTRL		0x024
#define RDMA_GMCIF_CON			0x028
#define RDMA_SRC_CON			0x030
#define RDMA_COMP_CON			0x038
#define RDMA_UFBDC_CONFIG_STASH_CTRL	0x03c
#define RDMA_PVRIC_CRYUVAL_0		0x040
#define RDMA_PVRIC_CRYUVAL_1		0x048
#define RDMA_PVRIC_CRCH0123VAL_0	0x050
#define RDMA_PVRIC_CRCH0123VAL_1	0x058
#define RDMA_MF_BKGD_SIZE_IN_BYTE	0x060
#define RDMA_MF_BKGD_SIZE_IN_PXL	0x068
#define RDMA_MF_SRC_SIZE		0x070
#define RDMA_MF_CLIP_SIZE		0x078
#define RDMA_MF_OFFSET_1		0x080
#define RDMA_SF_BKGD_SIZE_IN_BYTE	0x090
#define RDMA_MF_BKGD_H_SIZE_IN_PXL	0x098
#define RDMA_PREFETCH_CONTROL		0x0a8
#define RDMA_UYVY10B_DUMMY		0x0b0
#define RDMA_VC1_RANGE			0x0f0
#define RDMA_SRC_OFFSET_0		0x118
#define RDMA_SRC_OFFSET_1		0x120
#define RDMA_SRC_OFFSET_2		0x128
#define RDMA_SRC_OFFSET_WP		0x148
#define RDMA_SRC_OFFSET_HP		0x150
#define APU_DIRECT_COUPLE_CONTROL_EN	0x154	/* APU_DIRECT_COUPLE_CONTROL_ENABLE */
#define APU_DIRECT_COUPLE_CONTROL	0x158
#define RDMA_TRANSFORM_0		0x200
#define RDMA_DMABUF_CON_0		0x240
#define RDMA_URGENT_TH_CON_0		0x244
#define RDMA_ULTRA_TH_CON_0		0x248
#define RDMA_PREULTRA_TH_CON_0		0x250
#define RDMA_DMABUF_CON_1		0x254
#define RDMA_URGENT_TH_CON_1		0x258
#define RDMA_ULTRA_TH_CON_1		0x260
#define RDMA_PREULTRA_TH_CON_1		0x264
#define RDMA_DMABUF_CON_2		0x268
#define RDMA_URGENT_TH_CON_2		0x270
#define RDMA_ULTRA_TH_CON_2		0x274
#define RDMA_PREULTRA_TH_CON_2		0x278
#define RDMA_DMABUF_CON_3		0x280
#define RDMA_URGENT_TH_CON_3		0x284
#define RDMA_ULTRA_TH_CON_3		0x288
#define RDMA_PREULTRA_TH_CON_3		0x290
#define RDMA_DITHER_CON			0x2a0
#define RDMA_RESV_DUMMY_0		0x2a8
#define RDMA_UNCOMP_MON			0x2c0
#define RDMA_COMP_MON			0x2c8
#define RDMA_CHKS_EXTR			0x300
#define RDMA_CHKS_INTW			0x308
#define RDMA_CHKS_INTR			0x310
#define RDMA_CHKS_ROTO			0x318
#define RDMA_CHKS_SRIY			0x320
#define RDMA_CHKS_SRIU			0x328
#define RDMA_CHKS_SRIV			0x330
#define RDMA_CHKS_SROY			0x338
#define RDMA_CHKS_SROU			0x340
#define RDMA_CHKS_SROV			0x348
#define RDMA_CHKS_VUPI			0x350
#define RDMA_CHKS_VUPO			0x358
#define RDMA_DUMMY_REG			0x35c
#define RDMA_DEBUG_CON			0x380
#define RDMA_MON_STA_0			0x400
#define RDMA_MON_STA_1			0x408
#define RDMA_MON_STA_2			0x410
#define RDMA_MON_STA_3			0x418
#define RDMA_MON_STA_4			0x420
#define RDMA_MON_STA_5			0x428
#define RDMA_MON_STA_6			0x430
#define RDMA_MON_STA_7			0x438
#define RDMA_MON_STA_8			0x440
#define RDMA_MON_STA_9			0x448
#define RDMA_MON_STA_10			0x450
#define RDMA_MON_STA_11			0x458
#define RDMA_MON_STA_12			0x460
#define RDMA_MON_STA_13			0x468
#define RDMA_MON_STA_14			0x470
#define RDMA_MON_STA_15			0x478
#define RDMA_MON_STA_16			0x480
#define RDMA_MON_STA_17			0x488
#define RDMA_MON_STA_18			0x490
#define RDMA_MON_STA_19			0x498
#define RDMA_MON_STA_20			0x4a0
#define RDMA_MON_STA_21			0x4a8
#define RDMA_MON_STA_22			0x4b0
#define RDMA_MON_STA_23			0x4b8
#define RDMA_MON_STA_24			0x4c0
#define RDMA_MON_STA_25			0x4c8
#define RDMA_MON_STA_26			0x4d0
#define RDMA_MON_STA_27			0x4d8
#define RDMA_MON_STA_28			0x4e0
#define RDMA_SRC_BASE_0			0xf00
#define RDMA_SRC_BASE_1			0xf08
#define RDMA_SRC_BASE_2			0xf10
#define RDMA_UFO_DEC_LENGTH_BASE_Y	0xf20
#define RDMA_UFO_DEC_LENGTH_BASE_C	0xf28
#define RDMA_SRC_BASE_0_MSB		0xf30
#define RDMA_SRC_BASE_1_MSB		0xf34
#define RDMA_SRC_BASE_2_MSB		0xf38
#define RDMA_UFO_DEC_LENGTH_BASE_Y_MSB	0xf3c
#define RDMA_UFO_DEC_LENGTH_BASE_C_MSB	0xf40
#define RDMA_SRC_OFFSET_0_MSB		0xf44
#define RDMA_SRC_OFFSET_1_MSB		0xf48
#define RDMA_SRC_OFFSET_2_MSB		0xf4c
#define RDMA_AFBC_PAYLOAD_OST		0xf50
#define APU_SRC_BASE_0_A		0xf60
#define APU_SRC_BASE_0_B		0xf64
#define APU_SRC_BASE_0_C		0xf68
#define APU_SRC_BASE_0_D		0xf6c
#define APU_SRC_OFFSET_0_A		0xf70
#define APU_SRC_OFFSET_0_B		0xf74
#define APU_SRC_OFFSET_0_C		0xf78
#define APU_SRC_OFFSET_0_D		0xf7c
#define APU_SRC_BASE_0_A_MSB		0xf80
#define APU_SRC_BASE_0_B_MSB		0xf84
#define APU_SRC_BASE_0_C_MSB		0xf88
#define APU_SRC_BASE_0_D_MSB		0xf8c
#define APU_SRC_OFFSET_0_A_MSB		0xf90
#define APU_SRC_OFFSET_0_B_MSB		0xf94
#define APU_SRC_OFFSET_0_C_MSB		0xf98
#define APU_SRC_OFFSET_0_D_MSB		0xf9c

/* RDMA debug monitor register count */
#define RDMA_MON_COUNT 29

enum cpr_reg_idx {
	/* CMDQ_CPR_PREBUILT_REG_CNT = 20 */
	CPR_RDMA_SRC_OFFSET_0 = 0,
	CPR_RDMA_SRC_OFFSET_1,
	CPR_RDMA_SRC_OFFSET_2,
	CPR_RDMA_SRC_OFFSET_WP,
	CPR_RDMA_SRC_OFFSET_HP,
	CPR_RDMA_TRANSFORM_0,
	CPR_RDMA_SRC_BASE_0,
	CPR_RDMA_SRC_BASE_1,
	CPR_RDMA_SRC_BASE_2,
	CPR_RDMA_UFO_DEC_LENGTH_BASE_Y,
	CPR_RDMA_UFO_DEC_LENGTH_BASE_C,
	CPR_RDMA_SRC_BASE_0_MSB,
	CPR_RDMA_SRC_BASE_1_MSB,
	CPR_RDMA_SRC_BASE_2_MSB,
	CPR_RDMA_UFO_DEC_LENGTH_BASE_Y_MSB,
	CPR_RDMA_UFO_DEC_LENGTH_BASE_C_MSB,
	CPR_RDMA_SRC_OFFSET_0_MSB,
	CPR_RDMA_SRC_OFFSET_1_MSB,
	CPR_RDMA_SRC_OFFSET_2_MSB,
	CPR_RDMA_AFBC_PAYLOAD_OST,
	/* CMDQ_CPR_PREBUILT_EXT_REG_CNT = 30 */
	CPR_RDMA_CON,
	CPR_RDMA_GMCIF_CON,
	CPR_RDMA_SRC_CON,
	CPR_RDMA_COMP_CON,
	CPR_RDMA_MF_BKGD_SIZE_IN_BYTE,
	CPR_RDMA_MF_BKGD_SIZE_IN_PXL,
	CPR_RDMA_MF_SRC_SIZE,
	CPR_RDMA_MF_CLIP_SIZE,
	CPR_RDMA_MF_OFFSET_1,
	CPR_RDMA_SF_BKGD_SIZE_IN_BYTE,
	CPR_RDMA_MF_BKGD_H_SIZE_IN_PXL,
	CPR_RDMA_DMABUF_CON_0,
	CPR_RDMA_URGENT_TH_CON_0,
	CPR_RDMA_ULTRA_TH_CON_0,
	CPR_RDMA_PREULTRA_TH_CON_0,
	CPR_RDMA_DMABUF_CON_1,
	CPR_RDMA_URGENT_TH_CON_1,
	CPR_RDMA_ULTRA_TH_CON_1,
	CPR_RDMA_PREULTRA_TH_CON_1,
	CPR_RDMA_DMABUF_CON_2,
	CPR_RDMA_URGENT_TH_CON_2,
	CPR_RDMA_ULTRA_TH_CON_2,
	CPR_RDMA_PREULTRA_TH_CON_2,
	CPR_RDMA_DMABUF_CON_3,
	CPR_RDMA_URGENT_TH_CON_3,
	CPR_RDMA_ULTRA_TH_CON_3,
	CPR_RDMA_PREULTRA_TH_CON_3,
	CPR_RDMA_DITHER_CON,

	CPR_RDMA_COUNT,	/* 50 */
};

#define RDMA_CPR_PREBUILT(mod, pipe, index) \
	((index) < CMDQ_CPR_PREBUILT_REG_CNT ? \
	CMDQ_CPR_PREBUILT(mod, pipe, index) : \
	CMDQ_CPR_PREBUILT_EXT(mod, pipe, (index) - CMDQ_CPR_PREBUILT_REG_CNT))

static const dma_addr_t reg_idx_to_ofst[CPR_RDMA_COUNT] = {
	[CPR_RDMA_SRC_OFFSET_0] = RDMA_SRC_OFFSET_0,
	[CPR_RDMA_SRC_OFFSET_1] = RDMA_SRC_OFFSET_1,
	[CPR_RDMA_SRC_OFFSET_2] = RDMA_SRC_OFFSET_2,
	[CPR_RDMA_SRC_OFFSET_WP] = RDMA_SRC_OFFSET_WP,
	[CPR_RDMA_SRC_OFFSET_HP] = RDMA_SRC_OFFSET_HP,
	[CPR_RDMA_TRANSFORM_0] = RDMA_TRANSFORM_0,
	[CPR_RDMA_SRC_BASE_0] = RDMA_SRC_BASE_0,
	[CPR_RDMA_SRC_BASE_1] = RDMA_SRC_BASE_1,
	[CPR_RDMA_SRC_BASE_2] = RDMA_SRC_BASE_2,
	[CPR_RDMA_UFO_DEC_LENGTH_BASE_Y] = RDMA_UFO_DEC_LENGTH_BASE_Y,
	[CPR_RDMA_UFO_DEC_LENGTH_BASE_C] = RDMA_UFO_DEC_LENGTH_BASE_C,
	[CPR_RDMA_SRC_BASE_0_MSB] = RDMA_SRC_BASE_0_MSB,
	[CPR_RDMA_SRC_BASE_1_MSB] = RDMA_SRC_BASE_1_MSB,
	[CPR_RDMA_SRC_BASE_2_MSB] = RDMA_SRC_BASE_2_MSB,
	[CPR_RDMA_UFO_DEC_LENGTH_BASE_Y_MSB] = RDMA_UFO_DEC_LENGTH_BASE_Y_MSB,
	[CPR_RDMA_UFO_DEC_LENGTH_BASE_C_MSB] = RDMA_UFO_DEC_LENGTH_BASE_C_MSB,
	[CPR_RDMA_SRC_OFFSET_0_MSB] = RDMA_SRC_OFFSET_0_MSB,
	[CPR_RDMA_SRC_OFFSET_1_MSB] = RDMA_SRC_OFFSET_1_MSB,
	[CPR_RDMA_SRC_OFFSET_2_MSB] = RDMA_SRC_OFFSET_2_MSB,
	[CPR_RDMA_AFBC_PAYLOAD_OST] = RDMA_AFBC_PAYLOAD_OST,

	[CPR_RDMA_CON] = RDMA_CON,
	[CPR_RDMA_GMCIF_CON] = RDMA_GMCIF_CON,
	[CPR_RDMA_SRC_CON] = RDMA_SRC_CON,
	[CPR_RDMA_COMP_CON] = RDMA_COMP_CON,
	[CPR_RDMA_MF_BKGD_SIZE_IN_BYTE] = RDMA_MF_BKGD_SIZE_IN_BYTE,
	[CPR_RDMA_MF_BKGD_SIZE_IN_PXL] = RDMA_MF_BKGD_SIZE_IN_PXL,
	[CPR_RDMA_MF_SRC_SIZE] = RDMA_MF_SRC_SIZE,
	[CPR_RDMA_MF_CLIP_SIZE] = RDMA_MF_CLIP_SIZE,
	[CPR_RDMA_MF_OFFSET_1] = RDMA_MF_OFFSET_1,
	[CPR_RDMA_SF_BKGD_SIZE_IN_BYTE] = RDMA_SF_BKGD_SIZE_IN_BYTE,
	[CPR_RDMA_MF_BKGD_H_SIZE_IN_PXL] = RDMA_MF_BKGD_H_SIZE_IN_PXL,
	[CPR_RDMA_DMABUF_CON_0] = RDMA_DMABUF_CON_0,
	[CPR_RDMA_URGENT_TH_CON_0] = RDMA_URGENT_TH_CON_0,
	[CPR_RDMA_ULTRA_TH_CON_0] = RDMA_ULTRA_TH_CON_0,
	[CPR_RDMA_PREULTRA_TH_CON_0] = RDMA_PREULTRA_TH_CON_0,
	[CPR_RDMA_DMABUF_CON_1] = RDMA_DMABUF_CON_1,
	[CPR_RDMA_URGENT_TH_CON_1] = RDMA_URGENT_TH_CON_1,
	[CPR_RDMA_ULTRA_TH_CON_1] = RDMA_ULTRA_TH_CON_1,
	[CPR_RDMA_PREULTRA_TH_CON_1] = RDMA_PREULTRA_TH_CON_1,
	[CPR_RDMA_DMABUF_CON_2] = RDMA_DMABUF_CON_2,
	[CPR_RDMA_URGENT_TH_CON_2] = RDMA_URGENT_TH_CON_2,
	[CPR_RDMA_ULTRA_TH_CON_2] = RDMA_ULTRA_TH_CON_2,
	[CPR_RDMA_PREULTRA_TH_CON_2] = RDMA_PREULTRA_TH_CON_2,
	[CPR_RDMA_DMABUF_CON_3] = RDMA_DMABUF_CON_3,
	[CPR_RDMA_URGENT_TH_CON_3] = RDMA_URGENT_TH_CON_3,
	[CPR_RDMA_ULTRA_TH_CON_3] = RDMA_ULTRA_TH_CON_3,
	[CPR_RDMA_PREULTRA_TH_CON_3] = RDMA_PREULTRA_TH_CON_3,
	[CPR_RDMA_DITHER_CON] = RDMA_DITHER_CON,
};

static const enum cpr_reg_idx lsb_to_msb[CPR_RDMA_COUNT] = {
	[CPR_RDMA_SRC_OFFSET_0] = CPR_RDMA_SRC_OFFSET_0_MSB,
	[CPR_RDMA_SRC_OFFSET_1] = CPR_RDMA_SRC_OFFSET_1_MSB,
	[CPR_RDMA_SRC_OFFSET_2] = CPR_RDMA_SRC_OFFSET_2_MSB,
	[CPR_RDMA_SRC_BASE_0] = CPR_RDMA_SRC_BASE_0_MSB,
	[CPR_RDMA_SRC_BASE_1] = CPR_RDMA_SRC_BASE_1_MSB,
	[CPR_RDMA_SRC_BASE_2] = CPR_RDMA_SRC_BASE_2_MSB,
	[CPR_RDMA_UFO_DEC_LENGTH_BASE_Y] = CPR_RDMA_UFO_DEC_LENGTH_BASE_Y_MSB,
	[CPR_RDMA_UFO_DEC_LENGTH_BASE_C] = CPR_RDMA_UFO_DEC_LENGTH_BASE_C_MSB,
};

static const enum cpr_reg_idx rdma_dmabuf[] = {
	CPR_RDMA_DMABUF_CON_0,
	CPR_RDMA_DMABUF_CON_1,
	CPR_RDMA_DMABUF_CON_2,
	CPR_RDMA_DMABUF_CON_3,
};

static const enum cpr_reg_idx rdma_urgent_th[] = {
	CPR_RDMA_URGENT_TH_CON_0,
	CPR_RDMA_URGENT_TH_CON_1,
	CPR_RDMA_URGENT_TH_CON_2,
	CPR_RDMA_URGENT_TH_CON_3,
};

static const enum cpr_reg_idx rdma_ultra_th[] = {
	CPR_RDMA_ULTRA_TH_CON_0,
	CPR_RDMA_ULTRA_TH_CON_1,
	CPR_RDMA_ULTRA_TH_CON_2,
	CPR_RDMA_ULTRA_TH_CON_3,
};

static const enum cpr_reg_idx rdma_preultra_th[] = {
	CPR_RDMA_PREULTRA_TH_CON_0,
	CPR_RDMA_PREULTRA_TH_CON_1,
	CPR_RDMA_PREULTRA_TH_CON_2,
	CPR_RDMA_PREULTRA_TH_CON_3,
};

/* SMI offset */
#define SMI_LARB_NON_SEC_CON		0x380

/* APU-MML DirectCouple handshake height */
#define MML_RDMA_RACING_MAX		64

/* debug option to change sram read height */
int mml_racing_rh = MML_RDMA_RACING_MAX;
module_param(mml_racing_rh, int, 0644);

enum rdma_label {
	RDMA_LABEL_BASE_0 = 0,
	RDMA_LABEL_BASE_0_MSB,
	RDMA_LABEL_BASE_1,
	RDMA_LABEL_BASE_1_MSB,
	RDMA_LABEL_BASE_2,
	RDMA_LABEL_BASE_2_MSB,
	RDMA_LABEL_UFO_DEC_BASE_Y,
	RDMA_LABEL_UFO_DEC_BASE_Y_MSB,
	RDMA_LABEL_UFO_DEC_BASE_C,
	RDMA_LABEL_UFO_DEC_BASE_C_MSB,
	RDMA_LABEL_TOTAL
};

int mml_rdma_crc;
module_param(mml_rdma_crc, int, 0644);
int mml_rdma_dbg = 0x13;
module_param(mml_rdma_dbg, int, 0644);
#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
u32 *rdma_crc_va[MML_PIPE_CNT];
dma_addr_t rdma_crc_pa[MML_PIPE_CNT];
#endif

/* leading time in ns */
int rdma_stash_leading = 12500;
module_param(rdma_stash_leading, int, 0644);

static s32 rdma_write(struct cmdq_pkt *pkt, phys_addr_t base_pa, u8 hw_pipe,
		      enum cpr_reg_idx idx, u32 value, bool write_sec)
{
	if (write_sec) {
		return cmdq_pkt_assign_command(pkt,
			RDMA_CPR_PREBUILT(CMDQ_PREBUILT_MML, hw_pipe, idx), value);
	}
	/* else */
	return cmdq_pkt_write(pkt, NULL,
		base_pa + reg_idx_to_ofst[idx], value, U32_MAX);
}

static s32 rdma_write_ofst(struct cmdq_pkt *pkt, phys_addr_t base_pa, u8 hw_pipe,
			   enum cpr_reg_idx lsb_idx, u64 value, bool write_sec)
{
	enum cpr_reg_idx msb_idx;
	s32 ret;

	if (unlikely(lsb_idx >= CPR_RDMA_COUNT))
		return -EFAULT;

	msb_idx = lsb_to_msb[lsb_idx];

	ret = rdma_write(pkt,  base_pa, hw_pipe,
			 lsb_idx, value & GENMASK_ULL(31, 0), write_sec);
	if (ret)
		return ret;
	ret = rdma_write(pkt, base_pa, hw_pipe,
			 msb_idx, value >> 32, write_sec);
	return ret;
}

static s32 rdma_write_reuse(struct cmdq_pkt *pkt, u32 comp_id, phys_addr_t base_pa, u8 hw_pipe,
			    enum cpr_reg_idx idx, u32 value,
			    struct mml_task_reuse *reuse,
			    struct mml_pipe_cache *cache,
			    u16 *label_idx, bool write_sec)
{
	if (write_sec) {
		return mml_assign(comp_id, pkt,
			RDMA_CPR_PREBUILT(CMDQ_PREBUILT_MML, hw_pipe, idx), value,
			reuse, cache, label_idx);
	}
	/* else */
	return mml_write(comp_id, pkt,
		base_pa + reg_idx_to_ofst[idx], value, U32_MAX,
		reuse, cache, label_idx);
}

static s32 rdma_write_addr(struct cmdq_pkt *pkt, u32 comp_id, phys_addr_t base_pa, u8 hw_pipe,
			   enum cpr_reg_idx lsb_idx, u64 value,
			   struct mml_task_reuse *reuse,
			   struct mml_pipe_cache *cache,
			   u16 *label_idx, bool write_sec)
{
	enum cpr_reg_idx msb_idx;
	s32 ret;

	if (unlikely(lsb_idx >= CPR_RDMA_COUNT))
		return -EFAULT;

	msb_idx = lsb_to_msb[lsb_idx];

	ret = rdma_write_reuse(pkt, comp_id, base_pa, hw_pipe,
			       lsb_idx, value & GENMASK_ULL(31, 0),
			       reuse, cache, label_idx, write_sec);
	if (ret)
		return ret;
	ret = rdma_write_reuse(pkt, comp_id, base_pa, hw_pipe,
			       msb_idx, value >> 32,
			       reuse, cache, label_idx + 1, write_sec);
	return ret;
}

static void rdma_update_addr(u32 comp_id, struct mml_task_reuse *reuse,
			     u16 label_lsb, u16 label_msb, u64 value)
{
	mml_update(comp_id, reuse, label_lsb, value & GENMASK_ULL(31, 0));
	mml_update(comp_id, reuse, label_msb, value >> 32);
}

enum rdma_golden_fmt {
	GOLDEN_FMT_ARGB,
	GOLDEN_FMT_RGB,
	GOLDEN_FMT_YUV420,
	GOLDEN_FMT_YV12,
	GOLDEN_FMT_HYFBC,
	GOLDEN_FMT_AFBC,
	GOLDEN_FMT_TOTAL
};

struct rdma_data {
	u32 tile_width;
	u32 sram_size;
	u8 px_per_tick;
	u8 rb_swap;		/* WA: version for rb channel swap behavior */
	bool alpha_rsz_crop;	/* WA: align rdma crop size when alpha resize */
	bool write_sec_reg;	/* WA: write rdma registers in secured domain */
	bool tile_reset;	/* WA: write dummy register to clean up states */
	bool stash;		/* enable stash prefetch with delay time */

	/* threshold golden setting for racing mode */
	struct rdma_golden golden[GOLDEN_FMT_TOTAL];
};

static const struct rdma_data mt6893_rdma_data = {
	.tile_width = 640,
	.px_per_tick = 1,
	.rb_swap = 1,
};

static const struct rdma_data mt6983_rdma_data = {
	.tile_width = 1696,
	.px_per_tick = 1,
	.rb_swap = 1,
	.write_sec_reg = true,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = ARRAY_SIZE(th_argb_mt6983),
			.settings = th_argb_mt6983,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = ARRAY_SIZE(th_rgb_mt6983),
			.settings = th_rgb_mt6983,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = ARRAY_SIZE(th_yuv420_mt6983),
			.settings = th_yuv420_mt6983,
		},
		[GOLDEN_FMT_YV12] = {
			.cnt = ARRAY_SIZE(th_yv12_mt6985),
			.settings = th_yv12_mt6985,
		},
		[GOLDEN_FMT_AFBC] = {
			.cnt = ARRAY_SIZE(th_afbc_mt6985),
			.settings = th_afbc_mt6985,
		},
	},
};

static const struct rdma_data mt6879_rdma_data = {
	.tile_width = 1440,
	.px_per_tick = 1,
	.rb_swap = 1,
	.write_sec_reg = true,
};

static const struct rdma_data mt6895_rdma0_data = {
	.tile_width = 1344,
	.px_per_tick = 1,
	.rb_swap = 1,
	.write_sec_reg = true,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = ARRAY_SIZE(th_argb_mt6983),
			.settings = th_argb_mt6983,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = ARRAY_SIZE(th_rgb_mt6983),
			.settings = th_rgb_mt6983,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = ARRAY_SIZE(th_yuv420_mt6983),
			.settings = th_yuv420_mt6983,
		},
		[GOLDEN_FMT_YV12] = {
			.cnt = ARRAY_SIZE(th_yv12_mt6985),
			.settings = th_yv12_mt6985,
		},
		[GOLDEN_FMT_AFBC] = {
			.cnt = ARRAY_SIZE(th_afbc_mt6985),
			.settings = th_afbc_mt6985,
		},
	},
};

static const struct rdma_data mt6895_rdma1_data = {
	.tile_width = 896,
	.px_per_tick = 1,
	.rb_swap = 1,
	.write_sec_reg = true,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = ARRAY_SIZE(th_argb_mt6983),
			.settings = th_argb_mt6983,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = ARRAY_SIZE(th_rgb_mt6983),
			.settings = th_rgb_mt6983,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = ARRAY_SIZE(th_yuv420_mt6983),
			.settings = th_yuv420_mt6983,
		},
		[GOLDEN_FMT_YV12] = {
			.cnt = ARRAY_SIZE(th_yv12_mt6985),
			.settings = th_yv12_mt6985,
		},
		[GOLDEN_FMT_AFBC] = {
			.cnt = ARRAY_SIZE(th_afbc_mt6985),
			.settings = th_afbc_mt6985,
		},
	},
};

static const struct rdma_data mt6985_rdma_data = {
	.tile_width = 1760,
	.sram_size = 512 * 1024,	/* 1MB sram divid to 512K + 512K */
	.px_per_tick = 1,
	.rb_swap = 2,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = ARRAY_SIZE(th_argb_mt6985),
			.settings = th_argb_mt6985,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = ARRAY_SIZE(th_rgb_mt6985),
			.settings = th_rgb_mt6985,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = ARRAY_SIZE(th_yuv420_mt6985),
			.settings = th_yuv420_mt6985,
		},
		[GOLDEN_FMT_YV12] = {
			.cnt = ARRAY_SIZE(th_yv12_mt6985),
			.settings = th_yv12_mt6985,
		},
		[GOLDEN_FMT_HYFBC] = {
			.cnt = ARRAY_SIZE(th_hyfbc_mt6985),
			.settings = th_hyfbc_mt6985,
		},
		[GOLDEN_FMT_AFBC] = {
			.cnt = ARRAY_SIZE(th_afbc_mt6985),
			.settings = th_afbc_mt6985,
		},
	},
};

static const struct rdma_data mt6886_rdma_data = {
	.tile_width = 1312,
	.px_per_tick = 1,
	.rb_swap = 1,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = ARRAY_SIZE(th_argb_mt6886),
			.settings = th_argb_mt6886,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = ARRAY_SIZE(th_rgb_mt6886),
			.settings = th_rgb_mt6886,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = ARRAY_SIZE(th_yuv420_mt6886),
			.settings = th_yuv420_mt6886,
		},
		[GOLDEN_FMT_YV12] = {
			.cnt = ARRAY_SIZE(th_yv12_mt6886),
			.settings = th_yv12_mt6886,
		},
		[GOLDEN_FMT_AFBC] = {
			.cnt = ARRAY_SIZE(th_afbc_mt6886),
			.settings = th_afbc_mt6886,
		},
	},
};

static const struct rdma_data mt6899_mmlt_rdma_data = {
	.tile_width = 640,
	.px_per_tick = 2,
	.alpha_rsz_crop = true,
	.tile_reset = true,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = ARRAY_SIZE(th_argb_mt6985),
			.settings = th_argb_mt6985,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = ARRAY_SIZE(th_rgb_mt6985),
			.settings = th_rgb_mt6985,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = ARRAY_SIZE(th_yuv420_mt6985),
			.settings = th_yuv420_mt6985,
		},
		[GOLDEN_FMT_YV12] = {
			.cnt = ARRAY_SIZE(th_yv12_mt6985),
			.settings = th_yv12_mt6985,
		},
		[GOLDEN_FMT_HYFBC] = {
			.cnt = ARRAY_SIZE(th_hyfbc_mt6985),
			.settings = th_hyfbc_mt6985,
		},
		[GOLDEN_FMT_AFBC] = {
			.cnt = ARRAY_SIZE(th_afbc_mt6985),
			.settings = th_afbc_mt6985,
		},
	},
};

static const struct rdma_data mt6989_rdma_data = {
	.tile_width = 3520,
	.sram_size = 512 * 1024,	/* 1MB sram divid to 512K + 512K */
	.px_per_tick = 2,
	.rb_swap = 1,
	.alpha_rsz_crop = true,
	.tile_reset = true,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = ARRAY_SIZE(th_argb_mt6985),
			.settings = th_argb_mt6985,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = ARRAY_SIZE(th_rgb_mt6985),
			.settings = th_rgb_mt6985,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = ARRAY_SIZE(th_yuv420_mt6985),
			.settings = th_yuv420_mt6985,
		},
		[GOLDEN_FMT_YV12] = {
			.cnt = ARRAY_SIZE(th_yv12_mt6985),
			.settings = th_yv12_mt6985,
		},
		[GOLDEN_FMT_HYFBC] = {
			.cnt = ARRAY_SIZE(th_hyfbc_mt6985),
			.settings = th_hyfbc_mt6985,
		},
		[GOLDEN_FMT_AFBC] = {
			.cnt = ARRAY_SIZE(th_afbc_mt6985),
			.settings = th_afbc_mt6985,
		},
	},
};

static const struct rdma_data mt6878_rdma_data = {
	.tile_width = 640,
	.px_per_tick = 1,
	.rb_swap = 2,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = ARRAY_SIZE(th_argb_mt6985),
			.settings = th_argb_mt6985,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = ARRAY_SIZE(th_rgb_mt6985),
			.settings = th_rgb_mt6985,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = ARRAY_SIZE(th_yuv420_mt6985),
			.settings = th_yuv420_mt6985,
		},
		[GOLDEN_FMT_YV12] = {
			.cnt = ARRAY_SIZE(th_yv12_mt6985),
			.settings = th_yv12_mt6985,
		},
		[GOLDEN_FMT_HYFBC] = {
			.cnt = ARRAY_SIZE(th_hyfbc_mt6985),
			.settings = th_hyfbc_mt6985,
		},
		[GOLDEN_FMT_AFBC] = {
			.cnt = ARRAY_SIZE(th_afbc_mt6985),
			.settings = th_afbc_mt6985,
		},
	},
};

static const struct rdma_data mt6991_mmlt_rdma_data = {
	.tile_width = 640,
	.px_per_tick = 2,
	.alpha_rsz_crop = true,
	.tile_reset = true,
	.stash = true,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = ARRAY_SIZE(th_argb_mt6985),
			.settings = th_argb_mt6985,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = ARRAY_SIZE(th_rgb_mt6985),
			.settings = th_rgb_mt6985,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = ARRAY_SIZE(th_yuv420_mt6985),
			.settings = th_yuv420_mt6985,
		},
		[GOLDEN_FMT_YV12] = {
			.cnt = ARRAY_SIZE(th_yv12_mt6985),
			.settings = th_yv12_mt6985,
		},
		[GOLDEN_FMT_HYFBC] = {
			.cnt = ARRAY_SIZE(th_hyfbc_mt6985),
			.settings = th_hyfbc_mt6985,
		},
		[GOLDEN_FMT_AFBC] = {
			.cnt = ARRAY_SIZE(th_afbc_mt6985),
			.settings = th_afbc_mt6985,
		},
	},
};

static const struct rdma_data mt6991_mmlf_rdma_data = {
	.tile_width = 3872,
	.px_per_tick = 2,
	.sram_size = 512 * 1024,	/* 1MB sram divid to 512K + 512K */
	.alpha_rsz_crop = true,
	.tile_reset = true,
	.stash = true,
	.golden = {
		[GOLDEN_FMT_ARGB] = {
			.cnt = ARRAY_SIZE(th_argb_mt6985),
			.settings = th_argb_mt6985,
		},
		[GOLDEN_FMT_RGB] = {
			.cnt = ARRAY_SIZE(th_rgb_mt6985),
			.settings = th_rgb_mt6985,
		},
		[GOLDEN_FMT_YUV420] = {
			.cnt = ARRAY_SIZE(th_yuv420_mt6985),
			.settings = th_yuv420_mt6985,
		},
		[GOLDEN_FMT_YV12] = {
			.cnt = ARRAY_SIZE(th_yv12_mt6985),
			.settings = th_yv12_mt6985,
		},
		[GOLDEN_FMT_HYFBC] = {
			.cnt = ARRAY_SIZE(th_hyfbc_mt6985),
			.settings = th_hyfbc_mt6985,
		},
		[GOLDEN_FMT_AFBC] = {
			.cnt = ARRAY_SIZE(th_afbc_mt6985),
			.settings = th_afbc_mt6985,
		},
	},
};

struct mml_comp_rdma {
	struct mml_comp comp;
	const struct rdma_data *data;
	struct device *mmu_dev;	/* for dmabuf to iova */
	struct device *mmu_dev_sec; /* for secure dmabuf to secure iova */

	u16 event_eof;

	/* smi register to config sram/dram mode */
	phys_addr_t smi_larb_con;

	u32 sram_cnt;
	u64 sram_pa;
	struct mutex sram_mutex;

	u32 apudc_sel;
};

struct rdma_frame_data {
	u8 enable_ufo;
	u8 hw_fmt;
	u8 swap;
	u8 blk;
	u8 lb_2b_mode;
	u8 field;
	u8 blk_10bit;
	u8 blk_tile;
	u8 color_tran;
	u8 matrix_sel;
	u32 bits_per_pixel_y;
	u32 bits_per_pixel_uv;
	u32 hor_shift_uv;
	u32 ver_shift_uv;
	u32 vdo_blk_shift_w;
	u32 vdo_blk_height;
	u32 vdo_blk_shift_h;
	u32 datasize;		/* qos data size in bytes */
	u16 crop_off_l;		/* crop offset left */
	u16 crop_off_t;		/* crop offset top */
	u32 gmcif_con;

	/* dvfs */
	struct mml_frame_size max_size;

	bool ultra_off;

	/* array of indices to one of entry in cache entry list,
	 * use in reuse command
	 */
	u16 labels[RDMA_LABEL_TOTAL];
};

static inline struct rdma_frame_data *rdma_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static inline struct mml_comp_rdma *comp_to_rdma(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_rdma, comp);
}

static s32 rdma_prepare(struct mml_comp *comp, struct mml_task *task,
			struct mml_comp_config *ccfg)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct mml_frame_config *cfg = task->config;

	ccfg->data = kzalloc(sizeof(struct rdma_frame_data), GFP_KERNEL);
	if (!ccfg->data)
		return -ENOMEM;

	/* align crop size and set to frame config */
	if (rdma->data->alpha_rsz_crop && cfg->info.alpha) {
		if (cfg->info.dest_cnt == 1 ||
		    !memcmp(&cfg->info.dest[0].crop, &cfg->info.dest[1].crop,
			    sizeof(struct mml_crop))) {
			u32 in_crop_w = cfg->frame_tile_sz.width;

			in_crop_w += cfg->info.dest[0].crop.r.left & 1;
			cfg->frame_tile_sz.width = round_up(in_crop_w, 2);
		}
	}
	return 0;
}

static void rdma_config_smi(struct mml_comp_rdma *rdma,
	struct mml_frame_config *cfg, struct cmdq_pkt *pkt)
{
	const enum mml_mode mode = cfg->info.mode;
	u32 mask = GENMASK(19, 16) | (0x1 << 3);
	u32 value;

	/* config smi addr to emi (iova) or sram, and bw throttling */
	if (mode == MML_MODE_SRAM_READ || mode == MML_MODE_APUDC)
		value = 0xf << 16;
	else if (mode == MML_MODE_MML_DECOUPLE)
		value = 0x1 << 3;
	else
		value = 0;

	cmdq_pkt_write(pkt, NULL, rdma->smi_larb_con, value, mask);
}

static s32 rdma_buf_map(struct mml_comp *comp, struct mml_task *task,
			const struct mml_path_node *node)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct mml_frame_config *cfg = task->config;
	s32 ret = 0;

	mml_trace_ex_begin("%s", __func__);

	if (cfg->info.mode != MML_MODE_APUDC &&
		unlikely(task->config->info.mode != MML_MODE_SRAM_READ) &&
		!task->buf.src.dma[0].iova) {
		mml_mmp(buf_map, MMPROFILE_FLAG_START,
			((u64)task->job.jobid << 16) | comp->id, 0);

		/* get iova */
		ret = mml_buf_iova_get(cfg->info.src.secure ? rdma->mmu_dev_sec : rdma->mmu_dev,
			&task->buf.src);
		if (ret < 0)
			mml_err("%s iova fail %d", __func__, ret);

		mml_mmp(buf_map, MMPROFILE_FLAG_END,
			((u64)task->job.jobid << 16) | comp->id,
			(unsigned long)task->buf.src.dma[0].iova);

		mml_msg("%s comp %u dma %p iova %#11llx (%u) %#11llx (%u) %#11llx (%u)",
			__func__, comp->id, task->buf.src.dma[0].dmabuf,
			task->buf.src.dma[0].iova,
			task->buf.src.size[0],
			task->buf.src.dma[1].iova,
			task->buf.src.size[1],
			task->buf.src.dma[2].iova,
			task->buf.src.size[2]);
	}

	mml_trace_ex_end();

	return ret;
}

static s32 rdma_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	if (task->config->info.mode == MML_MODE_APUDC ||
		unlikely(task->config->info.mode == MML_MODE_SRAM_READ)) {
		struct mml_comp_rdma *rdma = comp_to_rdma(comp);

		mml_mmp(buf_prepare, MMPROFILE_FLAG_START,
			((u64)task->job.jobid << 16) | comp->id, 0);

		mutex_lock(&rdma->sram_mutex);
		if (!rdma->sram_cnt)
			rdma->sram_pa = (u64)mml_sram_get(task->config->mml, mml_sram_apudc);
		rdma->sram_cnt++;
		mutex_unlock(&rdma->sram_mutex);

		mml_mmp(buf_prepare, MMPROFILE_FLAG_END,
			((u64)task->job.jobid << 16) | comp->id, rdma->sram_pa);

		mml_msg("%s comp %u sram pa %#llx",
			__func__, comp->id, rdma->sram_pa);

		/* sram read case must have sram pa */
		if (!rdma->sram_pa)
			return -EINVAL;

	} else if (!task->buf.src.dma[0].iova) {
		/* sram read case must have allocated iova */
		return -EINVAL;
	}

	return 0;
}

static void rdma_buf_unprepare(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_comp_rdma *rdma;

	if (task->config->info.mode == MML_MODE_APUDC ||
		unlikely(task->config->info.mode == MML_MODE_SRAM_READ)) {
		rdma = comp_to_rdma(comp);

		mutex_lock(&rdma->sram_mutex);
		rdma->sram_cnt--;
		if (rdma->sram_cnt == 0)
			mml_sram_put(task->config->mml, mml_sram_apudc);
		mutex_unlock(&rdma->sram_mutex);
	}
}

static s32 rdma_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg,
			     struct tile_func_block *func,
			     union mml_tile_data *data)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_data *src = &cfg->info.src;
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);

	data->rdma.src_fmt = src->format;
	data->rdma.blk_shift_w = MML_FMT_BLOCK(src->format) ? 4 : 0;
	data->rdma.blk_shift_h = MML_FMT_BLOCK(src->format) ? 5 : 0;
	data->rdma.max_width = rdma->data->tile_width;

	/* RDMA support crop capability */
	func->type = TILE_TYPE_RDMA | TILE_TYPE_CROP_EN;
	func->init_func = tile_rdma_init;
	func->for_func = tile_rdma_for;
	func->back_func = tile_rdma_back;
	func->data = data;
	func->enable_flag = true;

	func->full_size_x_in = src->width;
	func->full_size_y_in = src->height;
	func->full_size_x_out = cfg->frame_tile_sz.width;
	func->full_size_y_out = cfg->frame_tile_sz.height;

	if (cfg->info.dest_cnt == 1 ||
	    !memcmp(&cfg->info.dest[0].crop, &cfg->info.dest[1].crop,
		    sizeof(struct mml_crop))) {
		struct mml_frame_dest *dest = &cfg->info.dest[0];
		struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);

		data->rdma.crop = dest->crop.r;
		if (rdma->data->alpha_rsz_crop && cfg->info.alpha) {
			data->rdma.crop.left &= ~1;
			data->rdma.crop.width = cfg->frame_tile_sz.width;
			data->rdma.align_x = 2;
		}
		rdma_frm->crop_off_l = data->rdma.crop.left;
		rdma_frm->crop_off_t = data->rdma.crop.top;
	} else {
		data->rdma.crop.left = 0;
		data->rdma.crop.top = 0;
		data->rdma.crop.width = src->width;
		data->rdma.crop.height = src->height;
	}

	return 0;
}

static const struct mml_comp_tile_ops rdma_tile_ops = {
	.prepare = rdma_tile_prepare,
};

static u32 rdma_get_label_count(struct mml_comp *comp, struct mml_task *task,
				struct mml_comp_config *ccfg)
{
	return RDMA_LABEL_TOTAL;
}

static void rdma_color_fmt(struct mml_frame_config *cfg,
			   struct rdma_frame_data *rdma_frm)
{
	u32 fmt = cfg->info.src.format;
	u16 profile_in = cfg->info.src.profile;

	rdma_frm->color_tran = 0;
	rdma_frm->matrix_sel = 15;

	rdma_frm->enable_ufo = MML_FMT_UFO(fmt);
	rdma_frm->hw_fmt = MML_FMT_HW_FORMAT(fmt);
	rdma_frm->swap = MML_FMT_SWAP(fmt);
	rdma_frm->blk = MML_FMT_BLOCK(fmt);
	rdma_frm->lb_2b_mode = rdma_frm->blk ? 0 : 1;
	rdma_frm->field = MML_FMT_INTERLACED(fmt);
	rdma_frm->blk_10bit = MML_FMT_10BIT_PACKED(fmt);
	rdma_frm->blk_tile = MML_FMT_10BIT_TILE(fmt);

	switch (fmt) {
	case MML_FMT_GREY:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_RGB565:
	case MML_FMT_BGR565:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		rdma_frm->color_tran = 1;
		break;
	case MML_FMT_RGB888:
	case MML_FMT_BGR888:
		rdma_frm->bits_per_pixel_y = 24;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		rdma_frm->color_tran = 1;
		break;
	case MML_FMT_RGBA8888:
	case MML_FMT_BGRA8888:
	case MML_FMT_ARGB8888:
	case MML_FMT_ABGR8888:
	case MML_FMT_RGBA1010102:
	case MML_FMT_BGRA1010102:
	case MML_FMT_RGBA8888_AFBC:
	case MML_FMT_RGBA1010102_AFBC:
		rdma_frm->bits_per_pixel_y = 32;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		rdma_frm->color_tran = 1;
		break;
	case MML_FMT_UYVY:
	case MML_FMT_VYUY:
	case MML_FMT_YUYV:
	case MML_FMT_YVYU:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_I420:
	case MML_FMT_YV12:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 8;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_I422:
	case MML_FMT_YV16:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 8;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_I444:
	case MML_FMT_YV24:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 8;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV12:
	case MML_FMT_NV21:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_YUV420_AFBC:
	case MML_FMT_NV12_HYFBC:
		rdma_frm->bits_per_pixel_y = 12;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_BLK_UFO:
	case MML_FMT_BLK_UFO_AUO:
	case MML_FMT_BLK:
		rdma_frm->vdo_blk_shift_w = 4;
		rdma_frm->vdo_blk_height = 32;
		rdma_frm->vdo_blk_shift_h = 5;
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_NV16:
	case MML_FMT_NV61:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV24:
	case MML_FMT_NV42:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 16;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV12_10L:
	case MML_FMT_NV21_10L:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 32;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_YUVA8888:
	case MML_FMT_AYUV8888:
	case MML_FMT_YUVA1010102:
	case MML_FMT_UYV1010102:
		rdma_frm->bits_per_pixel_y = 32;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	case MML_FMT_NV15:
	case MML_FMT_NV51:
		rdma_frm->bits_per_pixel_y = 10;
		rdma_frm->bits_per_pixel_uv = 20;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_YUV420_10P_AFBC:
	case MML_FMT_P010_HYFBC:
		rdma_frm->bits_per_pixel_y = 16;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	case MML_FMT_BLK_10H:
	case MML_FMT_BLK_10V:
	case MML_FMT_BLK_10HJ:
	case MML_FMT_BLK_10VJ:
	case MML_FMT_BLK_UFO_10H:
	case MML_FMT_BLK_UFO_10V:
	case MML_FMT_BLK_UFO_10HJ:
	case MML_FMT_BLK_UFO_10VJ:
		rdma_frm->vdo_blk_shift_w = 4;
		rdma_frm->vdo_blk_height = 32;
		rdma_frm->vdo_blk_shift_h = 5;
		rdma_frm->bits_per_pixel_y = 10;
		rdma_frm->bits_per_pixel_uv = 20;
		rdma_frm->hor_shift_uv = 1;
		rdma_frm->ver_shift_uv = 1;
		break;
	default:
		mml_err("[rdma] not support format %x", fmt);
		break;
	}

	/*
	 * 4'b0000:  0 RGB to JPEG
	 * 4'b0001:  1 RGB to FULL709
	 * 4'b0010:  2 RGB to BT601
	 * 4'b0011:  3 RGB to BT709
	 * 4'b0100:  4 JPEG to RGB
	 * 4'b0101:  5 FULL709 to RGB
	 * 4'b0110:  6 BT601 to RGB
	 * 4'b0111:  7 BT709 to RGB
	 * 4'b1000:  8 JPEG to BT601 / FULL709 to BT709
	 * 4'b1001:  9 JPEG to BT709
	 * 4'b1010: 10 BT601 to JPEG / BT709 to FULL709
	 * 4'b1011: 11 BT709 to JPEG
	 * 4'b1100: 12 BT709 to BT601
	 * 4'b1101: 13 BT601 to BT709
	 * 4'b1110: 14 JPEG to FULL709
	 * 4'b1111: 15 IDENTITY
	 *             FULL709 to JPEG
	 *             FULL709 to BT601
	 *             BT601 to FULL709
	 */
	if (profile_in == MML_YCBCR_PROFILE_BT2020 ||
	    profile_in == MML_YCBCR_PROFILE_FULL_BT2020)
		profile_in = MML_YCBCR_PROFILE_BT709;

	if (rdma_frm->color_tran) {
		if (MML_FMT_IS_RGB(cfg->info.dest[0].data.format) &&
		    !cfg->info.dest[0].pq_config.en)
			rdma_frm->matrix_sel = 1;
		else if (profile_in == MML_YCBCR_PROFILE_BT601)
			rdma_frm->matrix_sel = 2;
		else if (profile_in == MML_YCBCR_PROFILE_BT709)
			rdma_frm->matrix_sel = 3;
		else if (profile_in == MML_YCBCR_PROFILE_FULL_BT601)
			rdma_frm->matrix_sel = 0;
		else if (profile_in == MML_YCBCR_PROFILE_FULL_BT709)
			rdma_frm->matrix_sel = 1;
		else
			mml_err("[rdma] unknown color conversion %x",
				profile_in);
	}
}

s32 check_setting(struct mml_file_buf *src_buf, struct mml_frame_data *src)
{
	s32 plane = MML_FMT_PLANE(src->format);
	/* block format error check */
	if (MML_FMT_BLOCK(src->format)) {
		if ((src->width & 0x0f) || (src->height & 0x1f)) {
			mml_err("invalid blk, width %u, height %u",
				src->width, src->height);
			mml_err("alignment width 16x, height 32x");
			return -1;
		}

		/* 16-byte error */
		if ((src_buf->dma[0].iova & 0x0f) ||
		    (plane > 1 && (src_buf->dma[1].iova & 0x0f)) ||
		    (plane > 2 && (src_buf->dma[2].iova & 0x0f))) {
			mml_err("invalid block format setting, buffer %#11llx %#11llx %#11llx",
				src_buf->dma[0].iova, src_buf->dma[1].iova,
				src_buf->dma[2].iova);
			mml_err("buffer should be 16 align");
			return -1;
		}
		/* 128-byte warning for performance */
		if ((src_buf->dma[0].iova & 0x7f) ||
		    (plane > 1 && (src_buf->dma[1].iova & 0x7f)) ||
		    (plane > 2 && (src_buf->dma[2].iova & 0x7f))) {
			mml_log("warning: block format setting, buffer %#11llx %#11llx %#11llx",
				src_buf->dma[0].iova, src_buf->dma[1].iova,
				src_buf->dma[2].iova);
		}
	}

	if ((MML_FMT_PLANE(src->format) > 1) && src->uv_stride <= 0) {
		src->uv_stride = mml_color_get_min_uv_stride(src->format,
			src->width);
	}
	if ((plane == 1 && !src_buf->dma[0].iova) ||
	    (plane == 2 && (!src_buf->dma[1].iova || !src_buf->dma[0].iova)) ||
	    (plane == 3 && (!src_buf->dma[2].iova || !src_buf->dma[1].iova ||
	    !src_buf->dma[0].iova))) {
		mml_err("buffer plane number error, format = %#x, plane = %d",
			src->format, plane);
		return -1;
	}

	if (MML_FMT_H_SUBSAMPLE(src->format))
		src->width &= ~1;

	if (MML_FMT_V_SUBSAMPLE(src->format))
		src->height &= ~1;

	return 0;
}

static void calc_hyfbc(const struct mml_file_buf *src_buf,
		       const struct mml_frame_data *src,
		       u64 *y_header_addr, u64 *y_data_addr,
		       u64 *c_header_addr, u64 *c_data_addr)
{
	u64 buf_addr = src_buf->dma[0].iova;
	u32 width = round_up(src->width, 64);
	u32 height = round_up(src->height, 64);
	u32 y_data_sz = width * height;
	u32 c_data_sz;
	u32 y_header_sz;
	u32 c_header_sz;
	u32 total_sz;

	if (MML_FMT_10BIT(src->format))
		y_data_sz = y_data_sz * 6 >> 2;

	c_data_sz = y_data_sz >> 1;
	y_header_sz = (width * height + 63) >> 6;
	c_header_sz = ((width * height >> 1) + 63) >> 6;

	*y_data_addr = round_up(buf_addr + y_header_sz, 4096);
	*y_header_addr = *y_data_addr - y_header_sz;	/* should be 64 aligned */
	*c_data_addr = round_up(*y_data_addr + y_data_sz + c_header_sz, 4096);
	*c_header_addr = round_down(*c_data_addr - c_header_sz, 64);

	total_sz = (u32)(*c_data_addr + c_data_sz - buf_addr);
	if (src_buf->size[0] != total_sz)
		mml_log("[rdma]warn %s hyfbc buf size %u calc size %u",
			__func__, src_buf->size[0], total_sz);

	mml_msg("[rdma]hyfbc Y head %#llx data %#llx C head %#llx data %#llx",
		*y_header_addr, *y_data_addr, *c_header_addr, *c_data_addr);
}

static void calc_ufo(const struct mml_file_buf *src_buf,
		     const struct mml_frame_data *src,
		     u64 *ufo_dec_length_y, u64 *ufo_dec_length_c,
		     u32 *u4pic_size_bs, u32 *u4pic_size_y_bs)
{
	u32 u4pic_size_y = src->width * src->height;
	u32 u4ufo_len_size_y =
		((((u4pic_size_y + 255) >> 8) + 63 + (16*8)) >> 6) << 6;
	u32 u4pic_size_c_bs;

	if (MML_FMT_10BIT_PACKED(src->format)) {
		if (MML_FMT_10BIT_JUMP(src->format)) {
			*u4pic_size_y_bs =
				(((u4pic_size_y * 5 >> 2) + 511) >> 9) << 9;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4ufo_len_size_y + 4095) >>
				12) << 12;
			u4pic_size_c_bs =
				(((u4pic_size_y * 5 >> 3) + 63) >> 6) << 6;
		} else {
			*u4pic_size_y_bs =
				(((u4pic_size_y * 5 >> 2) + 4095) >> 12) << 12;
			u4pic_size_c_bs = u4pic_size_y * 5 >> 3;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4pic_size_c_bs + 511) >>
				9) << 9;
		}
	} else {
		if (MML_FMT_AUO(src->format)) {
			u4ufo_len_size_y = u4ufo_len_size_y << 1;
			*u4pic_size_y_bs = ((u4pic_size_y + 511) >> 9) << 9;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4ufo_len_size_y + 4095) >>
				12) << 12;
			u4pic_size_c_bs =
				(((u4pic_size_y >> 1) + 63) >> 6) << 6;
		} else {
			*u4pic_size_y_bs = ((u4pic_size_y + 4095) >> 12) << 12;
			u4pic_size_c_bs = u4pic_size_y >> 1;
			*u4pic_size_bs =
				((*u4pic_size_y_bs + u4pic_size_c_bs + 511) >>
				9) << 9;
		}
	}

	if (MML_FMT_10BIT_JUMP(src->format) || MML_FMT_AUO(src->format)) {
		/* Y YL C CL*/
		*ufo_dec_length_y = src_buf->dma[0].iova + src->plane_offset[0] +
				   *u4pic_size_y_bs;
		*ufo_dec_length_c = src_buf->dma[1].iova + src->plane_offset[1] +
				   u4pic_size_c_bs;
	} else {
		/* Y C YL CL */
		*ufo_dec_length_y = src_buf->dma[0].iova + src->plane_offset[0] +
				   *u4pic_size_bs;
		*ufo_dec_length_c = src_buf->dma[0].iova + src->plane_offset[0] +
				   *u4pic_size_bs + u4ufo_len_size_y;
	}
}

static void rdma_cpr_trigger(struct cmdq_pkt *pkt, const u8 hw_pipe,
			     const bool secure)
{
	cmdq_pkt_wfe(pkt, CMDQ_TOKEN_PREBUILT_MML_LOCK);
	cmdq_pkt_assign_command(pkt, CMDQ_CPR_PREBUILT_PIPE(CMDQ_PREBUILT_MML),
				hw_pipe + (secure << 1));
	cmdq_pkt_set_event(pkt, CMDQ_TOKEN_PREBUILT_MML_WAIT);
	cmdq_pkt_wfe(pkt, CMDQ_TOKEN_PREBUILT_MML_SET);
	cmdq_pkt_set_event(pkt, CMDQ_TOKEN_PREBUILT_MML_LOCK);
}

static void rdma_reset_threshold(struct mml_comp_rdma *rdma,
	struct cmdq_pkt *pkt, const phys_addr_t base_pa, const u8 hw_pipe,
	const bool write_sec)
{
	u32 i;

	/* clear threshold for all plane */
	for (i = 0; i < ARRAY_SIZE(rdma_dmabuf); i++) {
		rdma_write(pkt, base_pa, hw_pipe, rdma_dmabuf[i],
			   0x3000000, write_sec);
		rdma_write(pkt, base_pa, hw_pipe, rdma_urgent_th[i],
			   0, write_sec);
		rdma_write(pkt, base_pa, hw_pipe, rdma_ultra_th[i],
			   0, write_sec);
		rdma_write(pkt, base_pa, hw_pipe, rdma_preultra_th[i],
			   0, write_sec);
	}
}

static void rdma_select_threshold_hrt(struct mml_comp_rdma *rdma,
	struct cmdq_pkt *pkt, const phys_addr_t base_pa, const u8 hw_pipe,
	const bool write_sec, u32 format, u32 width, u32 height)
{
	const struct rdma_golden *golden;
	const struct golden_setting *golden_set;
	u32 pixel = width * height;
	u32 idx, i, dmabuf_con;
	u32 plane = MML_FMT_PLANE(format);

	if (MML_FMT_HYFBC(format)) {
		golden = &rdma->data->golden[GOLDEN_FMT_HYFBC];
	} else if (MML_FMT_AFBC(format)) {
		golden = &rdma->data->golden[GOLDEN_FMT_AFBC];
	} else if (plane == 1) {
		if (MML_FMT_BITS_PER_PIXEL(format) >= 32)
			golden = &rdma->data->golden[GOLDEN_FMT_ARGB];
		else
			golden = &rdma->data->golden[GOLDEN_FMT_RGB];
	} else if (plane == 2) {
		golden = &rdma->data->golden[GOLDEN_FMT_YUV420];
	} else if (plane == 3) {
		golden = &rdma->data->golden[GOLDEN_FMT_YV12];
	} else {
		golden = &rdma->data->golden[GOLDEN_FMT_ARGB];
	}

	for (idx = 0; idx < golden->cnt - 1; idx++)
		if (golden->settings[idx].pixel > pixel)
			break;
	golden_set = &golden->settings[idx];

	/* config threshold for all plane */
	for (i = 0; i < ARRAY_SIZE(rdma_dmabuf); i++) {
		dmabuf_con = 3 << 24;
		if (i < plane && !MML_FMT_COMPRESS(format)) {
			if (i == 0)
				dmabuf_con |= 32;
			else
				dmabuf_con |= 16;
		}

		rdma_write(pkt, base_pa, hw_pipe, rdma_dmabuf[i],
			   dmabuf_con, write_sec);
		rdma_write(pkt, base_pa, hw_pipe, rdma_urgent_th[i],
			   golden_set->plane[i].urgent, write_sec);
		rdma_write(pkt, base_pa, hw_pipe, rdma_ultra_th[i],
			   golden_set->plane[i].ultra, write_sec);
		rdma_write(pkt, base_pa, hw_pipe, rdma_preultra_th[i],
			   golden_set->plane[i].preultra, write_sec);
	}
}

static s32 rdma_config_frame(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	const struct mml_file_buf *src_buf = &task->buf.src;
	const struct mml_frame_data *src = &cfg->info.src;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const phys_addr_t base_pa = comp->base_pa;
	const u8 hw_pipe = cfg->path[ccfg->pipe]->hw_pipe;
	const bool write_sec = mml_slt ? false : rdma->data->write_sec_reg;
	const u32 dst_fmt = cfg->info.dest[ccfg->node->out_idx].data.format;
	u8 simple_mode = 1;
	u8 filter_mode;
	u8 loose = 0;
	u8 bit_number = 0;
	u8 ufo_auo = 0;
	u8 ufo_jump = 0;
	u8 afbc = 0;
	u8 afbc_y2r = 0;
	u8 hyfbc = 0;
	u8 ufbdc = 0;
	u8 output_10bit = 0;
	u32 width_in_pxl = 0;
	u32 height_in_pxl = 0;
	u64 iova[3];
	u64 ufo_dec_length_y = 0;
	u64 ufo_dec_length_c = 0;
	u32 u4pic_size_bs = 0;
	u32 u4pic_size_y_bs = 0;
	u32 gmcif_con;
	u8 in_swap;

	mml_msg("use config %p rdma %p", cfg, rdma);

	/* clear event */
	cmdq_pkt_clear_event(pkt, rdma->event_eof);

	/* before everything start, make sure ddr enable */
	if (ccfg->pipe == 0 && cfg->task_ops->ddren)
		cfg->task_ops->ddren(task, pkt, true);

	if (!write_sec) {
		/* Enable engine */
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_EN, 0x1, 0x00000001);
		/* Enable shadow */
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SHADOW_CTRL,
			(cfg->shadow ? 0 : BIT(1)) | 0x1, U32_MAX);
	}

	if (mml_rdma_crc) {
		if (MML_FMT_COMPRESS(src->format))
			cmdq_pkt_write(pkt, NULL, base_pa + RDMA_DEBUG_CON,
				(mml_rdma_dbg << 13) + 0x1, U32_MAX);
		else
			cmdq_pkt_write(pkt, NULL, base_pa + RDMA_DEBUG_CON,
				0x1, U32_MAX);
	} else if (MML_FMT_COMPRESS(src->format)) {
		/* debug for compress to dump crc */
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_DEBUG_CON,
			(mml_rdma_dbg << 13) + 0x1, U32_MAX);
	}

	rdma_color_fmt(cfg, rdma_frm);

	/* Enable dither on output, not input */
	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_DITHER_CON, 0x0, write_sec);

	gmcif_con = BIT(0) |		/* COMMAND_DIV */
		    GENMASK(7, 4) |	/* READ_REQUEST_TYPE */
		    GENMASK(9, 8) |	/* WRITE_REQUEST_TYPE */
		    BIT(16);		/* PRE_ULTRA_EN */
	/* racing case also enable urgent/ultra to not blocking disp */
	if (unlikely(mml_rdma_urgent)) {
		if (mml_rdma_urgent == 1)
			gmcif_con ^= BIT(16) | BIT(15);	/* URGENT_EN: always */
		else if (mml_rdma_urgent == 2)
			gmcif_con ^= BIT(13) | BIT(16);	/* ULTRA_EN: always */
		else
			gmcif_con |= BIT(14) | BIT(12);	/* URGENT_EN */
		if (cfg->info.mode == MML_MODE_RACING || cfg->info.mode == MML_MODE_DIRECT_LINK)
			rdma_select_threshold_hrt(rdma, pkt, base_pa, hw_pipe,
				write_sec, src->format, src->width, src->height);
		else
			rdma_reset_threshold(rdma, pkt, base_pa, hw_pipe, write_sec);
	} else if (cfg->info.mode == MML_MODE_RACING) {
		gmcif_con |= BIT(14);	/* URGENT_EN */

		/* for IR mode tile 0/1 ULTRA_EN=2(always enable)
		 * other tiles restore to ULTRA_EN=1(enable)
		 */
		gmcif_con |= BIT(13);	/* ULTRA_EN always enable */

		rdma_select_threshold_hrt(rdma, pkt, base_pa, hw_pipe,
			write_sec, src->format, src->width, src->height);
	} else if (cfg->info.mode == MML_MODE_DIRECT_LINK) {
		gmcif_con |= BIT(14) | BIT(12);	/* URGENT_EN and ULTRA_EN */
		rdma_select_threshold_hrt(rdma, pkt, base_pa, hw_pipe,
			write_sec, src->format, src->width, src->height);
	} else {
		rdma_reset_threshold(rdma, pkt, base_pa, hw_pipe, write_sec);
	}

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_GMCIF_CON,
		   gmcif_con, write_sec);
	rdma_frm->gmcif_con = gmcif_con;

	if (cfg->alpharot || cfg->rgbrot)
		rdma_frm->color_tran = 0;
	else if (MML_FMT_10BIT(src->format))
		rdma_frm->color_tran = 1;

	if (MML_FMT_IS_RGB(src->format) && cfg->info.dest[0].pq_config.en_hdr &&
	    cfg->info.dest_cnt == 1)
		rdma_frm->color_tran = 0;

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_TRANSFORM_0,
		   (rdma_frm->matrix_sel << 23) +
		   (rdma_frm->color_tran << 16),
		   write_sec);

	if (MML_FMT_V_SUBSAMPLE(src->format) &&
	    !MML_FMT_V_SUBSAMPLE(dst_fmt) &&
	    !MML_FMT_BLOCK(src->format))
		/* 420 to 422 interpolation solution */
		filter_mode = 2;
	else
		/* config.enRDMACrop ? 3 : 2 */
		/* RSZ uses YUV422, RDMA could use V filter unless cropping */
		filter_mode = 3;

	if (MML_FMT_10BIT_LOOSE(src->format))
		loose = 1;
	if (MML_FMT_10BIT(src->format))
		bit_number = 1;

	in_swap = rdma_frm->swap;
	if (MML_FMT_AFBC(dst_fmt) && MML_FMT_10BIT(dst_fmt)) {
		if (rdma->data->rb_swap == 1) {
			if (cfg->alpharot) {
				if ((MML_FMT_AFBC(src->format) &&
				    MML_FMT_SWAP(dst_fmt)) ||
				    (!MML_FMT_AFBC(src->format) &&
				    !MML_FMT_SWAP(dst_fmt)))
					in_swap = in_swap ? 0 : 1;
			} else {
				if (MML_FMT_IS_RGB(src->format) &&
				    !MML_FMT_SWAP(dst_fmt))
					in_swap = in_swap ? 0 : 1;
			}
		} else if (rdma->data->rb_swap == 2) {
			if (MML_FMT_IS_RGB(src->format) &&
			    !MML_FMT_SWAP(dst_fmt))
				in_swap = in_swap ? 0 : 1;
		}
	}

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_SRC_CON,
		   (rdma_frm->hw_fmt << 0) +
		   (filter_mode << 9) +
		   (loose << 11) +
		   (rdma_frm->field << 12) +
		   (in_swap << 14) +
		   (rdma_frm->blk << 15) +
		   (1 << 17) +	/* UNIFORM_CONFIG */
		   (bit_number << 18) +
		   (rdma_frm->blk_tile << 23) +
		   (0 << 24) +	/* RING_BUF_READ */
		   ((cfg->alpharot || cfg->alpharsz) << 25),
		   write_sec);

	if (rdma_frm->blk_10bit)
		ufo_jump = MML_FMT_10BIT_JUMP(src->format);
	else
		ufo_auo = MML_FMT_AUO(src->format);

	if (MML_FMT_HYFBC(src->format)) {
		hyfbc = 1;
		ufbdc = 1;
		width_in_pxl = round_up(src->width, 32);
		height_in_pxl = round_up(src->height, 16);
	} else if (MML_FMT_AFBC(src->format)) {
		afbc = 1;
		if (MML_FMT_IS_RGB(src->format))
			afbc_y2r = 1;
		ufbdc = 1;
		if (MML_FMT_IS_YUV(src->format)) {
			width_in_pxl = round_up(src->width, 16);
			height_in_pxl = round_up(src->height, 16);
		} else {
			width_in_pxl = round_up(src->width, 32);
			height_in_pxl = round_up(src->height, 8);
		}
	} else if (rdma_frm->enable_ufo && rdma_frm->blk_10bit) {
		width_in_pxl = (src->y_stride << 2) / 5;
	}
	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_MF_BKGD_SIZE_IN_PXL,
		   width_in_pxl, write_sec);
	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_MF_BKGD_H_SIZE_IN_PXL,
		   height_in_pxl, write_sec);
	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_AFBC_PAYLOAD_OST,
		   0, write_sec);

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_COMP_CON,
		   (rdma_frm->enable_ufo << 31) +
		   (ufo_auo << 29) +
		   (ufo_jump << 28) +
		   (1 << 26) +	/* UFO_DATA_IN_NOT_REV */
		   (1 << 25) +	/* UFO_DATA_OUT_NOT_REV */
		   (0 << 24) +	/* ufo_dcp */
		   (0 << 23) +	/* ufo_dcp_10bit */
		   (afbc << 22) +
		   (afbc_y2r << 21) +
		   (0 << 20) +	/* pvric_en */
		   (1 << 19) +	/* SHORT_BURST */
		   (12 << 14) +	/* UFBDC_HG_DISABLE */
		   (hyfbc << 13) +
		   (ufbdc << 12) +
		   (1 << 11),	/* payload_ost */
		   write_sec);

	if (MML_FMT_HYFBC(src->format)) {
		/* ufo_dec_length_y: Y header addr
		 * ufo_dec_length_c: C header addr
		 * src->plane_offset[0]: offset from buf addr to Y data addr
		 * src->plane_offset[1]: offset from buf addr to C data addr
		 */
		calc_hyfbc(src_buf, src, &ufo_dec_length_y, &iova[0],
			&ufo_dec_length_c, &iova[1]);

		rdma_write_addr(pkt, comp->id, base_pa, hw_pipe,
				CPR_RDMA_UFO_DEC_LENGTH_BASE_Y,
				ufo_dec_length_y,
				reuse, cache,
				&rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_Y],
				write_sec);

		rdma_write_addr(pkt, comp->id, base_pa, hw_pipe,
				CPR_RDMA_UFO_DEC_LENGTH_BASE_C,
				ufo_dec_length_c,
				reuse, cache,
				&rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_C],
				write_sec);
	} else if (rdma_frm->enable_ufo) {
		calc_ufo(src_buf, src, &ufo_dec_length_y, &ufo_dec_length_c,
			 &u4pic_size_bs, &u4pic_size_y_bs);

		rdma_write_addr(pkt, comp->id, base_pa, hw_pipe,
				CPR_RDMA_UFO_DEC_LENGTH_BASE_Y,
				ufo_dec_length_y,
				reuse, cache,
				&rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_Y],
				write_sec);

		rdma_write_addr(pkt, comp->id, base_pa, hw_pipe,
				CPR_RDMA_UFO_DEC_LENGTH_BASE_C,
				ufo_dec_length_c,
				reuse, cache,
				&rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_C],
				write_sec);
	} else {
		rdma_write_ofst(pkt, base_pa, hw_pipe,
				CPR_RDMA_UFO_DEC_LENGTH_BASE_Y, 0, write_sec);
		rdma_write_ofst(pkt, base_pa, hw_pipe,
				CPR_RDMA_UFO_DEC_LENGTH_BASE_C, 0, write_sec);
	}

	/* Enable 10-bit output */
	output_10bit = 1;
	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_CON,
		   (rdma_frm->lb_2b_mode << 12) +
		   (output_10bit << 5) +
		   (simple_mode << 4),
		   write_sec);

	if (!mml_slt)
		rdma_config_smi(rdma, cfg, pkt);

	/* Write frame base address */
	if (cfg->info.mode == MML_MODE_APUDC) {
		u32 stride = mml_color_get_min_y_stride(src->format, src->width);
		u32 tile_size = max(stride, src->y_stride) * mml_racing_rh;

		iova[0] = rdma->sram_pa;
		iova[1] = rdma->sram_pa + tile_size;
		iova[2] = 0;

		/* select rdma read toggle signal send to apu, instead of disp */
		if (rdma->apudc_sel)
			cmdq_pkt_write(pkt, NULL, rdma->apudc_sel, 0x2, U32_MAX);

		mml_msg("%s sram %#011llx tile size %u", __func__, iova[0], tile_size);

	} else if (unlikely(cfg->info.mode == MML_MODE_SRAM_READ)) {
		iova[0] = rdma->sram_pa + src->plane_offset[0];
		iova[1] = rdma->sram_pa + src->plane_offset[1];
		iova[2] = rdma->sram_pa + src->plane_offset[2];

		mml_msg("%s sram %#011llx", __func__, iova[0]);

	} else if (MML_FMT_HYFBC(src->format)) {
		/* clear since not use */
		iova[2] = 0;

		mml_msg("%s y %#011llx %#011llx c %#011llx %#011llx",
			__func__, ufo_dec_length_y, iova[0],
			ufo_dec_length_c, iova[1]);

	} else if (rdma_frm->enable_ufo) {
		if (MML_FMT_10BIT_JUMP(src->format) ||
			MML_FMT_AUO(src->format)) {
			iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
			iova[1] = src_buf->dma[0].iova + src->plane_offset[0] +
				  u4pic_size_bs;
			iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		} else {
			iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
			iova[1] = src_buf->dma[0].iova + src->plane_offset[0] +
				  u4pic_size_y_bs;
			iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		}

		mml_msg("%s src %#011llx %#011llx ufo %#011llx %#011llx",
			__func__, iova[0], iova[1],
			ufo_dec_length_y, ufo_dec_length_c);
	} else {
		iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
		iova[1] = src_buf->dma[1].iova + src->plane_offset[1];
		iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		mml_msg("%s src %#011llx %#011llx %#011llx",
			__func__, iova[0], iova[1], iova[2]);
	}

	if (cfg->info.mode == MML_MODE_APUDC) {
		cmdq_pkt_write(pkt, NULL, base_pa + APU_SRC_BASE_0_A, iova[0], U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + APU_SRC_BASE_0_B, iova[1], U32_MAX);

		mml_msg("APU_SRC_BASE A %#010llx B %#010llx", iova[0], iova[1]);

		/* dummy read to make sure direct couple handshaking work */
		cmdq_pkt_read_addr(pkt, base_pa + APU_DIRECT_COUPLE_CONTROL_EN, CMDQ_THR_SPR_IDX0);

		/* also enable sram handshaking with APU */
		cmdq_pkt_write(pkt, NULL, base_pa + APU_DIRECT_COUPLE_CONTROL_EN,
			0x3, U32_MAX);

		if (ccfg->pipe == 0)
			cmdq_pkt_write(pkt, NULL, base_pa + APU_DIRECT_COUPLE_CONTROL,
				(cfg->dual << 8) | (mml_racing_rh << 13), U32_MAX);
		else
			cmdq_pkt_write(pkt, NULL, base_pa + APU_DIRECT_COUPLE_CONTROL,
				mml_racing_rh << 13, U32_MAX);
	} else if (!mml_slt) {
		rdma_write_addr(pkt, comp->id, base_pa, hw_pipe,
				CPR_RDMA_SRC_BASE_0,
				iova[0],
				reuse, cache,
				&rdma_frm->labels[RDMA_LABEL_BASE_0],
				write_sec);
		rdma_write_addr(pkt, comp->id, base_pa, hw_pipe,
				CPR_RDMA_SRC_BASE_1,
				iova[1],
				reuse, cache,
				&rdma_frm->labels[RDMA_LABEL_BASE_1],
				write_sec);
		rdma_write_addr(pkt, comp->id, base_pa, hw_pipe,
				CPR_RDMA_SRC_BASE_2,
				iova[2],
				reuse, cache,
				&rdma_frm->labels[RDMA_LABEL_BASE_2],
				write_sec);
	}

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_MF_BKGD_SIZE_IN_BYTE,
		   src->y_stride, write_sec);
	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_SF_BKGD_SIZE_IN_BYTE,
		   src->uv_stride, write_sec);

	if (rdma->data->stash) {
		if (mml_stash_en(cfg->info.mode)) {
			u32 prefetch;
			/* prefetch_line_cnt = src_w * src_h * FPS * 12.5us / src_w */
			u32 prefetch_line_cnt = src->height * rdma_stash_leading / 1000 * cfg->fps;

			/* TODO: refine the prefetch_line_cnt, update each frame since fps change
			 * every frame in dc mode.
			 */

			if (MML_FMT_COMPRESS(src->format)) {
				prefetch = (prefetch_line_cnt << 12) | 0x1;
				cmdq_pkt_write(pkt, NULL, base_pa + RDMA_UFBDC_CONFIG_STASH_CTRL,
					prefetch, U32_MAX);
			} else {
				prefetch = 0x43000000 | (prefetch_line_cnt & 0xffff);
				cmdq_pkt_write(pkt, NULL, base_pa + RDMA_PREFETCH_CONTROL,
					prefetch, U32_MAX);
			}

			mml_msg("%s prefetch %#010x", __func__, prefetch);
		} else {
			if (MML_FMT_COMPRESS(src->format))
				cmdq_pkt_write(pkt, NULL, base_pa + RDMA_UFBDC_CONFIG_STASH_CTRL,
					0, U32_MAX);
			else
				cmdq_pkt_write(pkt, NULL, base_pa + RDMA_PREFETCH_CONTROL,
					0x43000000, U32_MAX);
		}
	}

	return 0;
}

static bool rdma_check_ultra_tile(struct mml_comp_config *ccfg,
	struct mml_frame_config *cfg, u32 tile_idx)
{
	const struct mml_frame_tile *tout = cfg->frame_tile[ccfg->pipe];
	/* sram always out0 */
	const struct mml_frame_dest *dest = &cfg->info.dest[0];
	u32 cur_tile_idx;

	if (dest->rotate == MML_ROT_90) {
		if (tout->tiles[tile_idx].h_tile_no == tout->h_tile_cnt - 1)
			return true;
		cur_tile_idx = tout->tiles[tile_idx].h_tile_no;
	} else if (dest->rotate == MML_ROT_270) {
		if (tout->tiles[tile_idx].h_tile_no == 0)
			return true;
		cur_tile_idx = tout->h_tile_cnt - tout->tiles[tile_idx].h_tile_no - 1;
	} else if (dest->rotate == MML_ROT_0) {
		if (tout->tiles[tile_idx].v_tile_no == tout->v_tile_cnt - 1)
			return true;
		cur_tile_idx = tout->tiles[tile_idx].v_tile_no;
	} else {
		if (tout->tiles[tile_idx].v_tile_no == 0)
			return true;
		cur_tile_idx = tout->v_tile_cnt - tout->tiles[tile_idx].v_tile_no - 1;
	}

	return cur_tile_idx < 2;
}

static s32 rdma_config_tile(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.src;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	u32 plane;

	const phys_addr_t base_pa = comp->base_pa;
	const u8 hw_pipe = cfg->path[ccfg->pipe]->hw_pipe;
	const bool write_sec = mml_slt ? false : rdma->data->write_sec_reg;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u64 src_offset_0;
	u64 src_offset_1;
	u64 src_offset_2;
	u32 src_offset_wp = 0;
	u32 src_offset_hp = 0;
	u32 mf_src_w;
	u32 mf_src_h;
	u32 mf_clip_w;
	u32 mf_clip_h;
	u32 mf_offset_w_1;
	u32 mf_offset_h_1;
	u32 gmcif_con = rdma_frm->gmcif_con;

	/* Following data retrieve from tile calc result */
	u64 in_xs = tile->in.xs;
	const u32 in_xe = tile->in.xe;
	u64 in_ys = tile->in.ys;
	const u32 in_ye = tile->in.ye;
	const u32 out_xs = tile->out.xs;
	const u32 out_xe = tile->out.xe;
	const u64 out_ys = tile->out.ys;
	const u32 out_ye = tile->out.ye;
	const u32 crop_ofst_x = tile->luma.x;
	const u32 crop_ofst_y = tile->luma.y;

	if (unlikely(mml_rdma_urgent)) {
		if (mml_rdma_urgent == 1)
			gmcif_con |= BIT(15);	/* URGENT_EN: always */
		else if (mml_rdma_urgent == 2)
			gmcif_con |= BIT(13);	/* ULTRA_EN: always */
		rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_GMCIF_CON,
			gmcif_con, write_sec);
		rdma_frm->ultra_off = false;
	} else if (cfg->info.mode == MML_MODE_RACING) {
		if (!rdma_frm->ultra_off && !rdma_check_ultra_tile(ccfg, cfg, idx)) {
			gmcif_con = rdma_frm->gmcif_con;

			gmcif_con ^= BIT(13) | BIT(12);
			rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_GMCIF_CON,
				gmcif_con, write_sec);
			rdma_frm->ultra_off = true;
		} else if (rdma_frm->ultra_off && rdma_check_ultra_tile(ccfg, cfg, idx)) {
			rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_GMCIF_CON,
				rdma_frm->gmcif_con, write_sec);
			rdma_frm->ultra_off = false;
		}
	}

	if (rdma_frm->blk) {
		/* Alignment X left in block boundary */
		in_xs = ((in_xs >> rdma_frm->vdo_blk_shift_w) <<
			rdma_frm->vdo_blk_shift_w);
		/* Alignment Y top in block boundary */
		in_ys = ((in_ys >> rdma_frm->vdo_blk_shift_h) <<
			rdma_frm->vdo_blk_shift_h);
	}

	if (MML_FMT_AFBC(src->format) || MML_FMT_HYFBC(src->format)) {
		src_offset_wp = in_xs;
		src_offset_hp = in_ys;
	}

	if (cfg->info.mode == MML_MODE_APUDC) {
		/* Set Y pixel offset */
		src_offset_0 = in_xs * rdma_frm->bits_per_pixel_y >> 3;

		/* sram case always 1 plane */
		src_offset_1 = 0;
		src_offset_2 = 0;

		/* Set source size */
		mf_src_w = in_xe - in_xs + 1;
		mf_src_h = in_ye - in_ys + 1;

		/* Set target size */
		mf_clip_w = out_xe - out_xs + 1;
		mf_clip_h = out_ye - out_ys + 1;

		/* Set crop offset */
		mf_offset_w_1 = crop_ofst_x;
		mf_offset_h_1 = crop_ofst_y;
	} else if (!rdma_frm->blk) {
		/* Set Y pixel offset */
		src_offset_0 = (in_xs * rdma_frm->bits_per_pixel_y >> 3) +
				in_ys * src->y_stride;

		/* Set U pixel offset */
		src_offset_1 = ((in_xs >> rdma_frm->hor_shift_uv) *
				rdma_frm->bits_per_pixel_uv >> 3) +
				(in_ys >> rdma_frm->ver_shift_uv) *
				src->uv_stride;

		/* Set V pixel offset */
		src_offset_2 = ((in_xs >> rdma_frm->hor_shift_uv) *
				rdma_frm->bits_per_pixel_uv >> 3) +
				(in_ys >> rdma_frm->ver_shift_uv) *
				src->uv_stride;

		/* Set source size */
		mf_src_w = in_xe - in_xs + 1;
		mf_src_h = in_ye - in_ys + 1;

		/* Set target size */
		mf_clip_w = out_xe - out_xs + 1;
		mf_clip_h = out_ye - out_ys + 1;

		/* Set crop offset */
		mf_offset_w_1 = crop_ofst_x;
		mf_offset_h_1 = crop_ofst_y;
	} else {
		/* Set Y pixel offset */
		src_offset_0 = (in_xs *
			       (rdma_frm->vdo_blk_height << rdma_frm->field) *
			       rdma_frm->bits_per_pixel_y >> 3) +
			       (in_ys >> rdma_frm->vdo_blk_shift_h) *
			       src->y_stride;

		/* Set 10bit UFO mode */
		if (MML_FMT_10BIT_PACKED(src->format) && rdma_frm->enable_ufo)
			src_offset_wp = (src_offset_0 << 2) / 5;

		/* Set U pixel offset */
		src_offset_1 = ((in_xs >> rdma_frm->hor_shift_uv) *
			       ((rdma_frm->vdo_blk_height >>
			       rdma_frm->ver_shift_uv) << rdma_frm->field) *
			       rdma_frm->bits_per_pixel_uv >> 3) +
			       (in_ys >> rdma_frm->vdo_blk_shift_h) *
			       src->uv_stride;

		/* Set V pixel offset */
		src_offset_2 = ((in_xs >> rdma_frm->hor_shift_uv) *
			       ((rdma_frm->vdo_blk_height >>
			       rdma_frm->ver_shift_uv) << rdma_frm->field) *
			       rdma_frm->bits_per_pixel_uv >> 3) +
			       (in_ys >> rdma_frm->vdo_blk_shift_h) *
			       src->uv_stride;

		/* Set source size */
		mf_src_w = in_xe - in_xs + 1;
		mf_src_h = (in_ye - in_ys + 1) << rdma_frm->field;

		/* Set target size */
		mf_clip_w = out_xe - out_xs + 1;
		mf_clip_h = (out_ye - out_ys + 1) << rdma_frm->field;

		/* Set crop offset */
		mf_offset_w_1 = out_xs + rdma_frm->crop_off_l - in_xs;
		mf_offset_h_1 = (out_ys + rdma_frm->crop_off_t - in_ys) << rdma_frm->field;
	}

	if (cfg->info.mode != MML_MODE_APUDC) {
		rdma_write_ofst(pkt, base_pa, hw_pipe, CPR_RDMA_SRC_OFFSET_0,
				src_offset_0, write_sec);
		rdma_write_ofst(pkt, base_pa, hw_pipe, CPR_RDMA_SRC_OFFSET_1,
				src_offset_1, write_sec);
		rdma_write_ofst(pkt, base_pa, hw_pipe, CPR_RDMA_SRC_OFFSET_2,
				src_offset_2, write_sec);
	} else {
		cmdq_pkt_write(pkt, NULL, base_pa + APU_SRC_OFFSET_0_A, src_offset_0, U32_MAX);
		cmdq_pkt_write(pkt, NULL, base_pa + APU_SRC_OFFSET_0_B, src_offset_0, U32_MAX);

		mml_msg("APU_SRC_OFFSET A %#010llx B %#010llx", src_offset_0, src_offset_0);
	}

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_SRC_OFFSET_WP,
		   src_offset_wp, write_sec);
	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_SRC_OFFSET_HP,
		   src_offset_hp, write_sec);

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_MF_SRC_SIZE,
		   (mf_src_h << 16) + mf_src_w, write_sec);

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_MF_CLIP_SIZE,
		   (mf_clip_h << 16) + mf_clip_w, write_sec);

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_MF_OFFSET_1,
		   (mf_offset_h_1 << 16) + mf_offset_w_1, write_sec);

	if (MML_FMT_COMPRESS(src->format)) {
		u32 xs = round_down(in_xs, 32);
		u32 xe = round_up(in_xe, 32);

		mf_src_w = xe - xs;
	}

	/* qos accumulate tile pixel */
	if (MML_FMT_AFBC(src->format) || MML_FMT_HYFBC(src->format)) {
		/* align block 32x16 */
		const u32 w = round_up(in_xe + 1, 32) - round_down(in_xs, 32);
		const u32 h = round_up(in_ye + 1, 16) - round_down(in_ys, 16);

		rdma_frm->max_size.width += w;
		rdma_frm->max_size.height = h;
	} else {
		/* no block align */
		rdma_frm->max_size.width += mf_src_w;
		rdma_frm->max_size.height = mf_src_h;
	}

	/* calculate qos for later use */
	plane = MML_FMT_PLANE(src->format);
	rdma_frm->datasize += mml_color_get_min_y_size(src->format,
		mf_src_w, mf_src_h);
	if (plane > 1)
		rdma_frm->datasize += mml_color_get_min_uv_size(src->format,
			mf_src_w, mf_src_h);
	if (plane > 2)
		rdma_frm->datasize += mml_color_get_min_uv_size(src->format,
			mf_src_w, mf_src_h);

	if (write_sec)
		rdma_cpr_trigger(pkt, hw_pipe, src->secure);
	return 0;
}

static s32 rdma_wait(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg, u32 idx)
{
	/* wait rdma frame done */
	cmdq_pkt_wfe(task->pkts[ccfg->pipe], comp_to_rdma(comp)->event_eof);
	return 0;
}

static void rdma_reset(struct mml_comp *comp, struct mml_task *task, struct mml_comp_config *ccfg)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	if (!rdma->data->tile_reset)
		return;

	/* write dummy register to force rdma tick and clean up */
	cmdq_pkt_write(pkt, NULL, comp->base_pa + RDMA_DUMMY_REG, 0, U32_MAX);
	cmdq_pkt_write(pkt, NULL, comp->base_pa + RDMA_DUMMY_REG, 0, U32_MAX);
	cmdq_pkt_write(pkt, NULL, comp->base_pa + RDMA_SHADOW_CTRL, 0x3, U32_MAX);
	cmdq_pkt_write(pkt, NULL, comp->base_pa + RDMA_CON, 0x8000, 0x8000);
	cmdq_pkt_write(pkt, NULL, comp->base_pa + RDMA_CON, 0x0, 0x8000);
	cmdq_pkt_write(pkt, NULL, comp->base_pa + RDMA_SHADOW_CTRL, 0x1, U32_MAX);
}

static void rdma_backup_crc(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	const phys_addr_t crc_reg = MML_FMT_COMPRESS(task->config->info.src.format) ?
		(comp->base_pa + RDMA_MON_STA_0 + 27 * 8) : (comp->base_pa + RDMA_CHKS_EXTR);
	int ret;

	if (likely(!mml_rdma_crc))
		return;

	ret = cmdq_pkt_backup(task->pkts[ccfg->pipe], crc_reg, &task->backup_crc_rdma[ccfg->pipe]);
	if (ret) {
		mml_err("%s fail to backup CRC", __func__);
		mml_rdma_crc = 0;
	}
#endif
}

static void rdma_backup_crc_update(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	if (!mml_rdma_crc || !task->backup_crc_rdma[ccfg->pipe].inst_offset)
		return;

	cmdq_pkt_backup_update(task->pkts[ccfg->pipe], &task->backup_crc_rdma[ccfg->pipe]);
#endif
}

static s32 rdma_post(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg)
{
	const struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	/* ufo case */
	if (MML_FMT_UFO(cfg->info.src.format))
		rdma_frm->datasize = (u32)div_u64((u64)rdma_frm->datasize * 7, 10);

	cache->total_datasize += rdma_frm->datasize;
	dvfs_cache_sz(cache, rdma_frm->max_size.width / rdma->data->px_per_tick,
		rdma_frm->max_size.height, 0, 0);
	dvfs_cache_log(cache, comp, "rdma");

	mml_msg("%s task %p pipe %hhu data %u",
		__func__, task, ccfg->pipe, rdma_frm->datasize);

	/* for sram test rollback smi config */
	if (cfg->info.mode == MML_MODE_APUDC ||
		unlikely(cfg->info.mode == MML_MODE_SRAM_READ)) {

		/* disable handshaking for safe */
		cmdq_pkt_write(task->pkts[ccfg->pipe], NULL,
			comp->base_pa + APU_DIRECT_COUPLE_CONTROL_EN, 0, U32_MAX);
	}

	rdma_backup_crc(comp, task, ccfg);

	/* after rdma stops read, call ddren to sleep */
	if (ccfg->pipe == 0 && cfg->task_ops->ddren)
		cfg->task_ops->ddren(task, task->pkts[0], false);

	return 0;
}

static s32 rdma_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	const struct mml_file_buf *src_buf = &task->buf.src;
	const struct mml_frame_data *src = &cfg->info.src;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];

	u64 iova[3];
	u64 ufo_dec_length_y = 0;
	u64 ufo_dec_length_c = 0;
	u32 u4pic_size_bs = 0;
	u32 u4pic_size_y_bs = 0;

	if (cfg->info.mode == MML_MODE_APUDC ||
		unlikely(cfg->info.mode == MML_MODE_SRAM_READ)) {
		/* no need update addr for sram case */
		return 0;
	}

	if (MML_FMT_HYFBC(src->format)) {
		calc_hyfbc(src_buf, src, &ufo_dec_length_y, &iova[0],
			&ufo_dec_length_c, &iova[1]);

		rdma_update_addr(comp->id, reuse,
				 rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_Y],
				 rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_Y_MSB],
				 ufo_dec_length_y);
		rdma_update_addr(comp->id, reuse,
				 rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_C],
				 rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_C_MSB],
				 ufo_dec_length_c);
	} else if (rdma_frm->enable_ufo) {
		calc_ufo(src_buf, src, &ufo_dec_length_y, &ufo_dec_length_c,
			 &u4pic_size_bs, &u4pic_size_y_bs);

		rdma_update_addr(comp->id, reuse,
				 rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_Y],
				 rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_Y_MSB],
				 ufo_dec_length_y);
		rdma_update_addr(comp->id, reuse,
				 rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_C],
				 rdma_frm->labels[RDMA_LABEL_UFO_DEC_BASE_C_MSB],
				 ufo_dec_length_c);
	}

	/* Write frame base address */
	if (MML_FMT_HYFBC(src->format)) {
		/* clear since not use */
		iova[2] = 0;
	} else if (rdma_frm->enable_ufo) {
		if (MML_FMT_10BIT_JUMP(src->format) ||
			MML_FMT_AUO(src->format)) {
			iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
			iova[1] = src_buf->dma[0].iova + src->plane_offset[0] +
				  u4pic_size_bs;
			iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		} else {
			iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
			iova[1] = src_buf->dma[0].iova + src->plane_offset[0] +
				  u4pic_size_y_bs;
			iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
		}
	} else {
		iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
		iova[1] = src_buf->dma[1].iova + src->plane_offset[1];
		iova[2] = src_buf->dma[2].iova + src->plane_offset[2];
	}

	rdma_update_addr(comp->id, reuse,
			 rdma_frm->labels[RDMA_LABEL_BASE_0],
			 rdma_frm->labels[RDMA_LABEL_BASE_0_MSB],
			 iova[0]);
	rdma_update_addr(comp->id, reuse,
			 rdma_frm->labels[RDMA_LABEL_BASE_1],
			 rdma_frm->labels[RDMA_LABEL_BASE_1_MSB],
			 iova[1]);
	rdma_update_addr(comp->id, reuse,
			 rdma_frm->labels[RDMA_LABEL_BASE_2],
			 rdma_frm->labels[RDMA_LABEL_BASE_2_MSB],
			 iova[2]);

	rdma_backup_crc_update(comp, task, ccfg);

	return 0;
}

static const struct mml_comp_config_ops rdma_cfg_ops = {
	.prepare = rdma_prepare,
	.buf_map = rdma_buf_map,
	.buf_prepare = rdma_buf_prepare,
	.buf_unprepare = rdma_buf_unprepare,
	.get_label_count = rdma_get_label_count,
	.frame = rdma_config_frame,
	.tile = rdma_config_tile,
	.wait = rdma_wait,
	.reset = rdma_reset,
	.post = rdma_post,
	.reframe = rdma_reconfig_frame,
};

u32 rdma_datasize_get(struct mml_task *task, struct mml_comp_config *ccfg)
{
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);

	return rdma_frm->datasize;
}

static u32 rdma_qos_stash_bw_get(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 *srt_bw_out, u32 *hrt_bw_out)
{
	/* stash command for every 4KB size, 4K to 1 stash (1 burst), 1 burst = 16bytes, thus
	 * stash_bw = normal_bw / 4K * 16
	 *	    = normal_bw / 256
	 */
	*srt_bw_out = *srt_bw_out / 256;
	*hrt_bw_out = *hrt_bw_out / 256;

	*srt_bw_out = max_t(u32, MML_QOS_MIN_STASH_BW, *srt_bw_out);
	if (*hrt_bw_out)
		*hrt_bw_out= max_t(u32, MML_QOS_MIN_STASH_BW, *hrt_bw_out);

	return 0;
}

u32 rdma_format_get(struct mml_task *task, struct mml_comp_config *ccfg)
{
	return task->config->info.src.format;
}

static void rdma_store_crc(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	const u32 pipe = ccfg->pipe;

	if (!mml_rdma_crc || !task->backup_crc_rdma[pipe].inst_offset)
		return;

	task->src_crc[ccfg->pipe] =
		cmdq_pkt_backup_get(task->pkts[pipe], &task->backup_crc_rdma[pipe]);
	mml_msg("%s rdma  component %2u job %u pipe %u crc %#010x idx %u",
		__func__, comp->id, task->job.jobid, ccfg->pipe, task->src_crc[pipe],
		task->backup_crc_rdma[pipe].val_idx);
#endif
}

static void rdma_task_done(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg)
{
	rdma_store_crc(comp, task, ccfg);
}

static void rdma_init_frame_done_event(struct mml_comp *comp, u32 event)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);

	if (!rdma->event_eof)
		rdma->event_eof = event;
}

static const struct mml_comp_hw_ops rdma_hw_ops = {
	.init_frame_done_event = &rdma_init_frame_done_event,
	.clk_enable = &mml_comp_clk_enable,
	.clk_disable = &mml_comp_clk_disable,
	.qos_datasize_get = &rdma_datasize_get,
	.qos_stash_bw_get = &rdma_qos_stash_bw_get,
	.qos_format_get = &rdma_format_get,
	.qos_set = &mml_comp_qos_set,
	.qos_clear = &mml_comp_qos_clear,
	.task_done = rdma_task_done,
};

static const char *rdma_state(u32 state)
{
	switch (state) {
	case 0x1:
		return "idle";
	case 0x2:
		return "wait sof";
	case 0x4:
		return "reg update";
	case 0x8:
		return "clear0";
	case 0x10:
		return "clear1";
	case 0x20:
		return "int0";
	case 0x40:
		return "int1";
	case 0x80:
		return "data running";
	case 0x100:
		return "wait done";
	case 0x200:
		return "warm reset";
	case 0x400:
		return "wait reset";
	default:
		return "";
	}
}

static const u32 rdma_ufbdc_debug_sel[] = {
	0x18, 0x27, 0x2d, 0x2e, 0x2f,
};

static void rdma_debug_dump(struct mml_comp *comp)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	void __iomem *base = comp->base;
	const bool write_sec = rdma->data->write_sec_reg;
	u32 value[35], comp_con;
	u32 apu_en;
	u32 state, greq;
	u32 i;

	mml_err("rdma component %u dump:", comp->id);

	if (write_sec)
		cmdq_util_prebuilt_dump(0, CMDQ_TOKEN_PREBUILT_MML_WAIT);
	else {
		u32 shadow_ctrl;

		value[0] = readl(base + RDMA_SRC_CON);
		value[1] = readl(base + RDMA_SRC_BASE_0_MSB);
		value[2] = readl(base + RDMA_SRC_BASE_0);
		value[3] = readl(base + RDMA_SRC_BASE_1_MSB);
		value[4] = readl(base + RDMA_SRC_BASE_1);
		value[5] = readl(base + RDMA_SRC_BASE_2_MSB);
		value[6] = readl(base + RDMA_SRC_BASE_2);

		mml_err("shadow RDMA_SRC_CON %#010x",
			value[0]);
		mml_err("shadow RDMA_SRC BASE_0_MSB %#010x BASE_0 %#010x",
			value[1], value[2]);
		mml_err("shadow RDMA_SRC BASE_1_MSB %#010x BASE_1 %#010x",
			value[3], value[4]);
		mml_err("shadow RDMA_SRC BASE_2_MSB %#010x BASE_2 %#010x",
			value[5], value[6]);

		/* Enable shadow read working */
		shadow_ctrl = readl(base + RDMA_SHADOW_CTRL);
		shadow_ctrl |= 0x4;
		writel(shadow_ctrl, base + RDMA_SHADOW_CTRL);
	}

	value[0] = readl(base + RDMA_EN);
	value[1] = readl(base + RDMA_RESET);
	value[2] = readl(base + RDMA_SRC_CON);
	comp_con = readl(base + RDMA_COMP_CON);

	apu_en = readl(base + APU_DIRECT_COUPLE_CONTROL_EN);

	value[4] = readl(base + RDMA_MF_BKGD_SIZE_IN_BYTE);
	value[5] = readl(base + RDMA_MF_BKGD_SIZE_IN_PXL);
	value[6] = readl(base + RDMA_MF_SRC_SIZE);
	value[7] = readl(base + RDMA_MF_CLIP_SIZE);
	value[8] = readl(base + RDMA_MF_OFFSET_1);
	value[9] = readl(base + RDMA_SF_BKGD_SIZE_IN_BYTE);
	value[10] = readl(base + RDMA_MF_BKGD_H_SIZE_IN_PXL);
	if (!write_sec) {
		if (!apu_en) {
			value[11] = readl(base + RDMA_SRC_OFFSET_0_MSB);
			value[12] = readl(base + RDMA_SRC_OFFSET_0);
			value[13] = readl(base + RDMA_SRC_OFFSET_1_MSB);
			value[14] = readl(base + RDMA_SRC_OFFSET_1);
			value[15] = readl(base + RDMA_SRC_OFFSET_2_MSB);
			value[16] = readl(base + RDMA_SRC_OFFSET_2);
		}
		value[17] = readl(base + RDMA_SRC_OFFSET_WP);
		value[18] = readl(base + RDMA_SRC_OFFSET_HP);
		value[19] = readl(base + RDMA_SRC_BASE_0_MSB);
		value[20] = readl(base + RDMA_SRC_BASE_0);
		value[21] = readl(base + RDMA_SRC_BASE_1_MSB);
		value[22] = readl(base + RDMA_SRC_BASE_1);
		value[23] = readl(base + RDMA_SRC_BASE_2_MSB);
		value[24] = readl(base + RDMA_SRC_BASE_2);
		value[25] = readl(base + RDMA_UFO_DEC_LENGTH_BASE_Y_MSB);
		value[26] = readl(base + RDMA_UFO_DEC_LENGTH_BASE_Y);
		value[27] = readl(base + RDMA_UFO_DEC_LENGTH_BASE_C_MSB);
		value[28] = readl(base + RDMA_UFO_DEC_LENGTH_BASE_C);
		value[29] = readl(base + RDMA_AFBC_PAYLOAD_OST);
	}
	value[30] = readl(base + RDMA_GMCIF_CON);
	value[33] = readl(base + RDMA_CON);
	value[34] = readl(base + RDMA_TRANSFORM_0);

	mml_err("RDMA_EN %#010x RDMA_RESET %#010x RDMA_SRC_CON %#010x RDMA_COMP_CON %#010x",
		value[0], value[1], value[2], comp_con);
	mml_err("RDMA_CON %#010x RDMA_TRANSFORM_0 %#010x",
		value[33], value[34]);
	mml_err("RDMA_MF_BKGD_SIZE_IN_BYTE %#010x RDMA_MF_BKGD_SIZE_IN_PXL %#010x",
		value[4], value[5]);
	mml_err("RDMA_MF_SRC_SIZE %#010x RDMA_MF_CLIP_SIZE %#010x RDMA_MF_OFFSET_1 %#010x",
		value[6], value[7], value[8]);
	mml_err("RDMA_SF_BKGD_SIZE_IN_BYTE %#010x RDMA_MF_BKGD_H_SIZE_IN_PXL %#010x",
		value[9], value[10]);
	if (!write_sec) {
		if (!apu_en) {
			mml_err("RDMA_SRC OFFSET_0_MSB %#010x OFFSET_0 %#010x",
				value[11], value[12]);
			mml_err("RDMA_SRC OFFSET_1_MSB %#010x OFFSET_1 %#010x",
				value[13], value[14]);
			mml_err("RDMA_SRC OFFSET_2_MSB %#010x OFFSET_2 %#010x",
				value[15], value[16]);
		}
		mml_err("RDMA_SRC_OFFSET_WP %#010x RDMA_SRC_OFFSET_HP %#010x",
			value[17], value[18]);
		mml_err("RDMA_SRC BASE_0_MSB %#010x BASE_0 %#010x",
			value[19], value[20]);
		mml_err("RDMA_SRC BASE_1_MSB %#010x BASE_1 %#010x",
			value[21], value[22]);
		mml_err("RDMA_SRC BASE_2_MSB %#010x BASE_2 %#010x",
			value[23], value[24]);
		mml_err("RDMA_UFO_DEC_LENGTH BASE_Y_MSB %#010x BASE_Y %#010x",
			value[25], value[26]);
		mml_err("RDMA_UFO_DEC_LENGTH BASE_C_MSB %#010x BASE_C %#010x",
			value[27], value[28]);
		mml_err("RDMA_AFBC_PAYLOAD_OST %#010x RDMA_GMCIF_CON %#010x",
			value[29], value[30]);
	}

	if (mml_rdma_crc) {
		value[31] = readl(base + RDMA_CHKS_EXTR);
		value[32] = readl(base + RDMA_DEBUG_CON);
		mml_err("RDMA_CHKS_EXTR %#010x RDMA_DEBUG_CON %#010x",
			value[31], value[32]);
	} else if (comp_con & BIT(12)) {
		/* debug for compress to dump crc */
		value[31] = readl(base + RDMA_CHKS_EXTR);
		value[32] = readl(base + RDMA_DEBUG_CON);
		mml_err("RDMA_CHKS_EXTR %#010x RDMA_DEBUG_CON %#010x",
			value[31], value[32]);
	}

	if (apu_en) {
		value[0] = readl(base + APU_DIRECT_COUPLE_CONTROL);
		value[1] = readl(base + APU_SRC_BASE_0_A);
		value[2] = readl(base + APU_SRC_OFFSET_0_A);
		value[3] = readl(base + APU_SRC_BASE_0_B);
		value[4] = readl(base + APU_SRC_OFFSET_0_B);

		mml_err("APU_DIRECT_COUPLE_CONTROL_EN %#010x APU_DIRECT_COUPLE_CONTROL %#010x",
			apu_en, value[0]);
		mml_err("APU_SRC_BASE_0_A %#010x APU_SRC_OFFSET_0_A %#010x",
			value[1], value[2]);
		mml_err("APU_SRC_BASE_0_B %#010x APU_SRC_OFFSET_0_B %#010x",
			value[3], value[4]);
	}

	/* mon sta from 0 ~ 28 */
	for (i = 0; i < RDMA_MON_COUNT; i++)
		value[i] = readl(base + RDMA_MON_STA_0 + i * 8);

	for (i = 0; i < RDMA_MON_COUNT / 3; i++) {
		mml_err("RDMA_MON_STA_%-2u %#010x RDMA_MON_STA_%-2u %#010x RDMA_MON_STA_%-2u %#010x",
			i * 3, value[i * 3],
			i * 3 + 1, value[i * 3 + 1],
			i * 3 + 2, value[i * 3 + 2]);
	}
	mml_err("RDMA_MON_STA_27 %#010x RDMA_MON_STA_28 %#010x",
		value[27], value[28]);

	/* ufbdc enable, dump more */
	if (comp_con & BIT(12)) {
		writel(rdma_ufbdc_debug_sel[0] << 13, base + RDMA_DEBUG_CON);
		value[10] = readl(base + RDMA_MON_STA_27);
		writel(rdma_ufbdc_debug_sel[1] << 13, base + RDMA_DEBUG_CON);
		value[11] = readl(base + RDMA_MON_STA_27);
		writel(rdma_ufbdc_debug_sel[2] << 13, base + RDMA_DEBUG_CON);
		value[12] = readl(base + RDMA_MON_STA_27);
		mml_err("ufbdc dbg sel %#04x %#010x  %#04x %#010x  %#04x %#010x",
			rdma_ufbdc_debug_sel[0], value[10],
			rdma_ufbdc_debug_sel[1], value[11],
			rdma_ufbdc_debug_sel[2], value[12]);

		writel(rdma_ufbdc_debug_sel[3] << 13, base + RDMA_DEBUG_CON);
		value[13] = readl(base + RDMA_MON_STA_27);
		writel(rdma_ufbdc_debug_sel[4] << 13, base + RDMA_DEBUG_CON);
		value[14] = readl(base + RDMA_MON_STA_27);
		mml_err("ufbdc dbg sel %#04x %#010x  %#04x %#010x",
			rdma_ufbdc_debug_sel[3], value[13],
			rdma_ufbdc_debug_sel[4], value[14]);
	}

	/* parse state */
	mml_err("RDMA ack:%u req:%d ufo:%u",
		(value[0] >> 11) & 0x1, (value[0] >> 10) & 0x1,
		(value[0] >> 25) & 0x1);
	state = (value[1] >> 8) & 0x7ff;
	greq = (value[0] >> 21) & 0x1;
	mml_err("RDMA state: %#x (%s)", state, rdma_state(state));
	mml_err("RDMA horz_cnt %u vert_cnt %u",
		value[26] & 0xffff, (value[26] >> 16) & 0xffff);
	mml_err("RDMA greq:%u => suggest to ask SMI help:%u", greq, greq);
}

static const struct mml_comp_debug_ops rdma_debug_ops = {
	.dump = &rdma_debug_dump,
};

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_rdma *rdma = dev_get_drvdata(dev);
	s32 ret;

	ret = mml_register_comp(master, &rdma->comp);
	if (ret)
		dev_err(dev, "Failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
	return ret;
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	struct mml_comp_rdma *rdma = dev_get_drvdata(dev);

	mml_unregister_comp(master, &rdma->comp);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static struct mml_comp_rdma *dbg_probed_components[4];
static int dbg_probed_count;

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mml_comp_rdma *priv;
	s32 ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	platform_set_drvdata(pdev, priv);
	priv->data = of_device_get_match_data(dev);

	if (smmu_v3_enabled()) {
		/* shared smmu device, setup 34bit in dts */
		priv->mmu_dev = mml_smmu_get_shared_device(dev, "mtk,smmu-shared");
		priv->mmu_dev_sec = mml_smmu_get_shared_device(dev, "mtk,smmu-shared-sec");
	} else {
		priv->mmu_dev = dev;
		priv->mmu_dev_sec = dev;
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
		if (ret)
			mml_err("fail to config rdma dma mask %d", ret);
	}

	ret = mml_comp_init(pdev, &priv->comp);
	if (ret) {
		dev_err(dev, "Failed to init mml component: %d\n", ret);
		return ret;
	}

	/* init larb for smi and mtcmos */
	ret = mml_comp_init_larb(&priv->comp, dev);
	if (ret) {
		if (ret == -EPROBE_DEFER)
			return ret;
		dev_err(dev, "fail to init component %u larb ret %d",
			priv->comp.id, ret);
	}

	/* store smi larb con register for later use */
	priv->smi_larb_con = priv->comp.larb_base +
		SMI_LARB_NON_SEC_CON + priv->comp.larb_port * 4;
	mutex_init(&priv->sram_mutex);
	mml_log("comp(rdma) %u smi larb con %pa", priv->comp.id, &priv->smi_larb_con);

	if (of_property_read_u16(dev->of_node, "event-frame-done",
				 &priv->event_eof))
		dev_err(dev, "read event frame_done fail\n");

	/* apu direct couple mml sel register */
	if (of_property_read_u32(dev->of_node, "apudc-sel", &priv->apudc_sel))
		priv->apudc_sel = 0;

	/* assign ops */
	priv->comp.tile_ops = &rdma_tile_ops;
	priv->comp.config_ops = &rdma_cfg_ops;
	priv->comp.hw_ops = &rdma_hw_ops;
	priv->comp.debug_ops = &rdma_debug_ops;

	dbg_probed_components[dbg_probed_count++] = priv;

	ret = mml_comp_add(priv->comp.id, dev, &mml_comp_ops);

	return ret;
}

static int remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mml_comp_ops);
	return 0;
}

const struct of_device_id mml_rdma_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6983-mml_rdma",
		.data = &mt6983_rdma_data,
	},
	{
		.compatible = "mediatek,mt6893-mml_rdma",
		.data = &mt6893_rdma_data
	},
	{
		.compatible = "mediatek,mt6879-mml_rdma",
		.data = &mt6879_rdma_data
	},
	{
		.compatible = "mediatek,mt6895-mml_rdma0",
		.data = &mt6895_rdma0_data
	},
	{
		.compatible = "mediatek,mt6895-mml_rdma1",
		.data = &mt6895_rdma1_data
	},
	{
		.compatible = "mediatek,mt6985-mml_rdma",
		.data = &mt6985_rdma_data,
	},
	{
		.compatible = "mediatek,mt6886-mml_rdma",
		.data = &mt6886_rdma_data,
	},
	{
		.compatible = "mediatek,mt6897-mml_rdma",
		.data = &mt6985_rdma_data,
	},
	{
		.compatible = "mediatek,mt6899-mml0_rdma",
		.data = &mt6899_mmlt_rdma_data,
	},
	{
		.compatible = "mediatek,mt6899-mml1_rdma",
		.data = &mt6989_rdma_data,
	},
	{
		.compatible = "mediatek,mt6989-mml_rdma",
		.data = &mt6989_rdma_data,
	},
	{
		.compatible = "mediatek,mt6878-mml_rdma",
		.data = &mt6878_rdma_data,
	},
	{
		.compatible = "mediatek,mt6991-mml0_rdma",
		.data = &mt6991_mmlt_rdma_data,
	},
	{
		.compatible = "mediatek,mt6991-mml1_rdma",
		.data = &mt6991_mmlf_rdma_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_rdma_driver_dt_match);

struct platform_driver mml_rdma_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-rdma",
		.owner = THIS_MODULE,
		.of_match_table = mml_rdma_driver_dt_match,
	},
};

//module_platform_driver(mml_rdma_driver);

static s32 dbg_case;
static s32 dbg_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump component status");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static s32 dbg_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", dbg_case, dbg_probed_count);
		for (i = 0; i < dbg_probed_count; i++) {
			struct mml_comp *comp = &dbg_probed_components[i]->comp;

			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] mml comp_id: %d.%d @%pa name: %s bound: %d\n", i,
				comp->id, comp->sub_idx, &comp->base_pa,
				comp->name ? comp->name : "(null)", comp->bound);
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  -         larb_port: %d @%pa pw: %d clk: %d\n",
				comp->larb_port, &comp->larb_base,
				comp->pw_cnt, comp->clk_cnt);
		}
		break;
	default:
		mml_err("not support read for debug_case: %d", dbg_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static const struct kernel_param_ops dbg_param_ops = {
	.set = dbg_set,
	.get = dbg_get,
};
module_param_cb(rdma_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(rdma_debug, "mml rdma debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML RDMA driver");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
