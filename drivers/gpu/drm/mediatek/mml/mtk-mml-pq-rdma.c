// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
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
#define RDMA_VC1_RANGE			0x0f0
#define RDMA_SRC_OFFSET_0		0x118
#define RDMA_SRC_OFFSET_1		0x120
#define RDMA_SRC_OFFSET_2		0x128
#define RDMA_SRC_OFFSET_WP		0x148
#define RDMA_SRC_OFFSET_HP		0x150
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

	ret = rdma_write(pkt, base_pa, hw_pipe,
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
	u8 px_per_tick;
	bool write_sec_reg;	/* WA: write rdma registers in secured domain */
	bool tile_reset;	/* WA: write dummy register to clean up states */
	bool stash;		/* enable stash prefetch with delay time */

	/* threshold golden setting for racing mode */
	struct rdma_golden golden[GOLDEN_FMT_TOTAL];
};

static const struct rdma_data mt6985_pq_rdma_data = {
	.tile_width = 1760,
	.px_per_tick = 1,
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
	},
};

static const struct rdma_data mt6989_pq_rdma_data = {
	.tile_width = 3520,
	.px_per_tick = 2,
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
	},
};

static const struct rdma_data mt6991_pq_rdma_data = {
	.tile_width = 516,
	.px_per_tick = 2,
	.tile_reset = true,
	.stash = true,
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
};

struct rdma_frame_data {
	u8 hw_fmt;
	u8 swap;
	u8 lb_2b_mode;
	u8 field;
	u8 matrix_sel;
	u32 bits_per_pixel_y;
	u32 bits_per_pixel_uv;
	u32 hor_shift_uv;
	u32 ver_shift_uv;
	u32 pixel_acc;		/* pixel accumulation */
	u32 datasize;		/* qos data size in bytes */
	u16 crop_off_l;		/* crop offset left */
	u16 crop_off_t;		/* crop offset top */
	u32 gmcif_con;

	/* dvfs */
	u32 line_bubble;
	struct mml_frame_size max_size;

	bool ultra_off;

	/* array of indices to one of entry in cache entry list,
	 * use in reuse command
	 */
	u16 labels[RDMA_LABEL_TOTAL];
};

#define rdma_frm_data(i)	((struct rdma_frame_data *)(i->data))

static inline struct mml_comp_rdma *comp_to_rdma(struct mml_comp *comp)
{
	return container_of(comp, struct mml_comp_rdma, comp);
}

static s32 rdma_config_read(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	ccfg->data = kzalloc(sizeof(struct rdma_frame_data), GFP_KERNEL);
	return 0;
}

static s32 rdma_buf_map(struct mml_comp *comp, struct mml_task *task,
			const struct mml_path_node *node)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct mml_frame_config *cfg = task->config;
	s32 ret = 0;

	mml_trace_ex_begin("%s sram or iova", __func__);

	if (!task->buf.seg_map.dma[0].iova) {
		mml_mmp(buf_map, MMPROFILE_FLAG_START,
			((u64)task->job.jobid << 16) | comp->id, 0);

		/* get iova */
		ret = mml_buf_iova_get(cfg->info.src.secure ? rdma->mmu_dev_sec : rdma->mmu_dev,
			&task->buf.seg_map);
		if (ret < 0)
			mml_err("%s iova fail %d", __func__, ret);

		mml_mmp(buf_map, MMPROFILE_FLAG_END,
			((u64)task->job.jobid << 16) | comp->id,
			(unsigned long)task->buf.seg_map.dma[0].iova);

		mml_msg("%s comp %u iova %#11llx (%u) %#11llx (%u) %#11llx (%u)",
			__func__, comp->id,
			task->buf.seg_map.dma[0].iova,
			task->buf.seg_map.size[0],
			task->buf.seg_map.dma[1].iova,
			task->buf.seg_map.size[1],
			task->buf.seg_map.dma[2].iova,
			task->buf.seg_map.size[2]);
	}

	mml_trace_ex_end();

	return ret;
}

static s32 rdma_buf_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	if (!task->buf.seg_map.dma[0].iova) {
		/* sram read case must have allocated iova */
		return -EINVAL;
	}

	return 0;
}

static u32 rdma_get_label_count(struct mml_comp *comp, struct mml_task *task,
				struct mml_comp_config *ccfg)
{
	return RDMA_LABEL_TOTAL;
}

static void rdma_color_fmt(struct mml_frame_config *cfg,
			   struct rdma_frame_data *rdma_frm)
{
	u32 fmt = cfg->info.seg_map.format;
	u16 profile_in = cfg->info.seg_map.profile;

	rdma_frm->matrix_sel = 15;

	rdma_frm->hw_fmt = MML_FMT_HW_FORMAT(fmt);
	rdma_frm->swap = MML_FMT_SWAP(fmt);
	rdma_frm->field = MML_FMT_INTERLACED(fmt);

	switch (fmt) {
	case MML_FMT_GREY:
		rdma_frm->bits_per_pixel_y = 8;
		rdma_frm->bits_per_pixel_uv = 0;
		rdma_frm->hor_shift_uv = 0;
		rdma_frm->ver_shift_uv = 0;
		break;
	default:
		mml_err("[rdma] not support format %x", fmt);
		break;
	}

	if (profile_in == MML_YCBCR_PROFILE_BT2020 ||
	    profile_in == MML_YCBCR_PROFILE_FULL_BT709 ||
	    profile_in == MML_YCBCR_PROFILE_FULL_BT2020)
		profile_in = MML_YCBCR_PROFILE_BT709;
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
	u32 idx, i;

	if (MML_FMT_PLANE(format) == 1) {
		if (MML_FMT_BITS_PER_PIXEL(format) >= 32)
			golden = &rdma->data->golden[GOLDEN_FMT_ARGB];
		else
			golden = &rdma->data->golden[GOLDEN_FMT_RGB];
	} else {
		golden = &rdma->data->golden[GOLDEN_FMT_ARGB];
	}

	for (idx = 0; idx < golden->cnt - 1; idx++)
		if (golden->settings[idx].pixel > pixel)
			break;
	golden_set = &golden->settings[idx];

	/* config threshold for all plane */
	for (i = 0; i < ARRAY_SIZE(rdma_dmabuf); i++) {
		rdma_write(pkt, base_pa, hw_pipe, rdma_dmabuf[i],
			   3 << 24 | 32, write_sec);
		rdma_write(pkt, base_pa, hw_pipe, rdma_urgent_th[i],
			   golden_set->plane[i].urgent, write_sec);
		rdma_write(pkt, base_pa, hw_pipe, rdma_ultra_th[i],
			   golden_set->plane[i].ultra, write_sec);
		rdma_write(pkt, base_pa, hw_pipe, rdma_preultra_th[i],
			   golden_set->plane[i].preultra, write_sec);
	}
}

static s32 rdma_region_pq_bw(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg,
			     struct mml_tile_engine *ref_tile,
			     struct mml_tile_engine *tile)
{
	tile->out.xs = ref_tile->in.xs;
	tile->out.xe = ref_tile->in.xe;
	tile->out.ys = ref_tile->in.ys;
	tile->out.ye = ref_tile->in.ye;

	tile->in.xs = ref_tile->in.xs;
	tile->in.xe = ref_tile->in.xe;
	tile->in.ys = ref_tile->in.ys;
	tile->in.ye = ref_tile->in.ye;
	return 0;
}

static const struct mml_comp_tile_ops rdma_tile_ops = {
	.region_pq_bw = rdma_region_pq_bw,
};

static s32 rdma_config_frame(struct mml_comp *comp, struct mml_task *task,
			     struct mml_comp_config *ccfg)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_file_buf *src_buf = &task->buf.seg_map;
	struct mml_frame_data *src = &cfg->info.seg_map;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];
	struct mml_pipe_cache *cache = &cfg->cache[ccfg->pipe];

	const phys_addr_t base_pa = comp->base_pa;
	const u8 hw_pipe = cfg->path[ccfg->pipe]->hw_pipe;
	const bool write_sec = mml_slt ? false : rdma->data->write_sec_reg;
	u8 filter_mode = 3;
	u64 iova[3];
	u32 gmcif_con;
	u32 prefetch = 0x43000000;

	mml_msg("use config %p rdma %p", cfg, rdma);

	/* clear event */
	cmdq_pkt_clear_event(pkt, rdma->event_eof);

	if (!write_sec) {
		/* Enable engine */
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_EN, 0x1, 0x00000001);
		/* Enable shadow */
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_SHADOW_CTRL,
			(cfg->shadow ? 0 : BIT(1)) | 0x1, U32_MAX);
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
		if (cfg->info.mode == MML_MODE_DIRECT_LINK)
			rdma_select_threshold_hrt(rdma, pkt, base_pa, hw_pipe,
				write_sec, src->format, src->width, src->height);
		else
			rdma_reset_threshold(rdma, pkt, base_pa, hw_pipe, write_sec);
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

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_SRC_CON,
		   (rdma_frm->hw_fmt << 0) +
		   (filter_mode << 9) +
		   (rdma_frm->field << 12) +
		   (rdma_frm->swap << 14) +
		   (1 << 17) +	/* UNIFORM_CONFIG */
		   (0 << 24),/* RING_BUF_READ */
		   write_sec);

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_AFBC_PAYLOAD_OST,
		   0, write_sec);

	/* Write frame base address */
	iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
	iova[1] = src_buf->dma[1].iova + src->plane_offset[1];
	iova[2] = src_buf->dma[2].iova + src->plane_offset[2];

	mml_msg("%s src %#011llx %#011llx %#011llx",
		__func__, iova[0], iova[1], iova[2]);

	if (!mml_slt) {
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

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_TRANSFORM_0,
		   (rdma_frm->matrix_sel << 23),
		   write_sec);

	if (rdma->data->stash) {
		if (mml_stash_en(cfg->info.mode)) {
			/* prefetch_line_cnt = src_w * src_h * FPS * 12.5us / src_w */
			u32 prefetch_line_cnt = src->height * rdma_stash_leading * cfg->fps;

			prefetch = prefetch | (prefetch_line_cnt & 0xffff);
			mml_msg("%s prefetch %#010x", __func__, prefetch);
		}
		cmdq_pkt_write(pkt, NULL, base_pa + RDMA_PREFETCH_CONTROL, prefetch, U32_MAX);
	}


	return 0;
}

static s32 rdma_config_tile(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_frame_data *src = &cfg->info.seg_map;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	u32 plane;

	const phys_addr_t base_pa = comp->base_pa;
	const u8 hw_pipe = cfg->path[ccfg->pipe]->hw_pipe;
	const bool write_sec = mml_slt ? false : rdma->data->write_sec_reg;

	struct mml_tile_engine *tile = config_get_tile(cfg, ccfg, idx);

	u64 src_offset_0;
	u64 src_offset_1;
	u64 src_offset_2;
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
	}

	mml_msg("%s in.xe %d, in.xs %llu, in.ye %d, in.ys %llu",
		__func__, in_xe, in_xs, in_ye, in_ys);
	mml_msg("%s out.xe %d, out.xs %d, out.ye %d, out.ys %llu",
		__func__, out_xe, out_xs, out_ye, out_ys);

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

	rdma_write_ofst(pkt, base_pa, hw_pipe, CPR_RDMA_SRC_OFFSET_0,
			src_offset_0, write_sec);
	rdma_write_ofst(pkt, base_pa, hw_pipe, CPR_RDMA_SRC_OFFSET_1,
			src_offset_1, write_sec);
	rdma_write_ofst(pkt, base_pa, hw_pipe, CPR_RDMA_SRC_OFFSET_2,
			src_offset_2, write_sec);

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_MF_SRC_SIZE,
		   (mf_src_h << 16) + mf_src_w, write_sec);

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_MF_CLIP_SIZE,
		   (mf_clip_h << 16) + mf_clip_w, write_sec);

	rdma_write(pkt, base_pa, hw_pipe, CPR_RDMA_MF_OFFSET_1,
		   (mf_offset_h_1 << 16) + mf_offset_w_1, write_sec);

	/* qos accumulate tile pixel */
	rdma_frm->pixel_acc += mf_src_w;

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
}

static s32 rdma_post(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg)
{
	const struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_frame_data *src = &task->config->info.seg_map;
	struct mml_pipe_cache *cache = &task->config->cache[ccfg->pipe];

	/* Data size add to task and pixel,
	 * it is ok for rdma to directly assign and accumulate in wrot.
	 */
	cache->total_datasize += rdma_frm->datasize;
	dvfs_cache_sz(cache, rdma_frm->pixel_acc / rdma->data->px_per_tick, src->height, 0, 0);
	dvfs_cache_log(cache, comp, "rdma2");

	return 0;
}

static s32 rdma_reconfig_frame(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);
	struct mml_file_buf *src_buf = &task->buf.seg_map;
	struct mml_frame_data *src = &cfg->info.seg_map;
	struct mml_task_reuse *reuse = &task->reuse[ccfg->pipe];

	u64 iova[3];

	/* Write frame base address */
	iova[0] = src_buf->dma[0].iova + src->plane_offset[0];
	iova[1] = src_buf->dma[1].iova + src->plane_offset[1];
	iova[2] = src_buf->dma[2].iova + src->plane_offset[2];


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

	return 0;
}

static const struct mml_comp_config_ops rdma_cfg_ops = {
	.prepare = rdma_config_read,
	.buf_map = rdma_buf_map,
	.buf_prepare = rdma_buf_prepare,
	.get_label_count = rdma_get_label_count,
	.frame = rdma_config_frame,
	.tile = rdma_config_tile,
	.wait = rdma_wait,
	.reset = rdma_reset,
	.post = rdma_post,
	.reframe = rdma_reconfig_frame,
};

static u32 pq_rdma_datasize_get(struct mml_task *task, struct mml_comp_config *ccfg)
{
	struct rdma_frame_data *rdma_frm = rdma_frm_data(ccfg);

	return rdma_frm->datasize;
}

static u32 pq_rdma_qos_stash_bw_get(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 *srt_bw_out, u32 *hrt_bw_out)
{
	/* stash command for every 4KB size */
	*srt_bw_out = *srt_bw_out / 256;
	*hrt_bw_out = *hrt_bw_out / 256;

	*srt_bw_out = max_t(u32, MML_QOS_MIN_STASH_BW, *srt_bw_out);
	if (*hrt_bw_out)
		*hrt_bw_out= max_t(u32, MML_QOS_MIN_STASH_BW, *hrt_bw_out);

	return 0;
}

static u32 pq_rdma_format_get(struct mml_task *task, struct mml_comp_config *ccfg)
{
	return task->config->info.seg_map.format;
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
	.qos_datasize_get = &pq_rdma_datasize_get,
	.qos_stash_bw_get = &pq_rdma_qos_stash_bw_get,
	.qos_format_get = &pq_rdma_format_get,
	.qos_set = &mml_comp_qos_set,
	.qos_clear = &mml_comp_qos_clear,
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

static void rdma_debug_dump(struct mml_comp *comp)
{
	struct mml_comp_rdma *rdma = comp_to_rdma(comp);
	void __iomem *base = comp->base;
	const bool write_sec = rdma->data->write_sec_reg;
	u32 value[33];
	u32 state, greq;
	u32 i;

	mml_err("rdma component %u dump:", comp->id);

	if (write_sec)
		cmdq_util_prebuilt_dump(0, CMDQ_TOKEN_PREBUILT_MML_WAIT);
	else {
		u32 shadow_ctrl;

		/* Enable shadow read working */
		shadow_ctrl = readl(base + RDMA_SHADOW_CTRL);
		shadow_ctrl |= 0x4;
		writel(shadow_ctrl, base + RDMA_SHADOW_CTRL);
	}

	value[0] = readl(base + RDMA_EN);
	value[1] = readl(base + RDMA_RESET);
	value[2] = readl(base + RDMA_SRC_CON);
	value[3] = readl(base + RDMA_COMP_CON);
	/* for afbc case enable more debug info */
	if (value[3] & BIT(22)) {
		u32 debug_con = readl(base + RDMA_DEBUG_CON);

		debug_con |= 0xe000;
		writel(debug_con, base + RDMA_DEBUG_CON);
	}

	value[4] = readl(base + RDMA_MF_BKGD_SIZE_IN_BYTE);
	value[5] = readl(base + RDMA_MF_BKGD_SIZE_IN_PXL);
	value[6] = readl(base + RDMA_MF_SRC_SIZE);
	value[7] = readl(base + RDMA_MF_CLIP_SIZE);
	value[8] = readl(base + RDMA_MF_OFFSET_1);
	value[9] = readl(base + RDMA_SF_BKGD_SIZE_IN_BYTE);
	value[10] = readl(base + RDMA_MF_BKGD_H_SIZE_IN_PXL);
	if (!write_sec) {
		value[11] = readl(base + RDMA_SRC_OFFSET_0_MSB);
		value[12] = readl(base + RDMA_SRC_OFFSET_0);
		value[13] = readl(base + RDMA_SRC_OFFSET_1_MSB);
		value[14] = readl(base + RDMA_SRC_OFFSET_1);
		value[15] = readl(base + RDMA_SRC_OFFSET_2_MSB);
		value[16] = readl(base + RDMA_SRC_OFFSET_2);
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

	mml_err("RDMA_EN %#010x RDMA_RESET %#010x RDMA_SRC_CON %#010x RDMA_COMP_CON %#010x",
		value[0], value[1], value[2], value[3]);
	mml_err("RDMA_MF_BKGD_SIZE_IN_BYTE %#010x RDMA_MF_BKGD_SIZE_IN_PXL %#010x",
		value[4], value[5]);
	mml_err("RDMA_MF_SRC_SIZE %#010x RDMA_MF_CLIP_SIZE %#010x RDMA_MF_OFFSET_1 %#010x",
		value[6], value[7], value[8]);
	mml_err("RDMA_SF_BKGD_SIZE_IN_BYTE %#010x RDMA_MF_BKGD_H_SIZE_IN_PXL %#010x",
		value[9], value[10]);
	if (!write_sec) {
		mml_err("RDMA_SRC OFFSET_0_MSB %#010x OFFSET_0 %#010x",
			value[11], value[12]);
		mml_err("RDMA_SRC OFFSET_1_MSB %#010x OFFSET_1 %#010x",
			value[13], value[14]);
		mml_err("RDMA_SRC OFFSET_2_MSB %#010x OFFSET_2 %#010x",
			value[15], value[16]);
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

const struct of_device_id mml_pq_rdma_driver_dt_match[] = {
	{
		.compatible = "mediatek,mt6985-mml_pq_rdma",
		.data = &mt6985_pq_rdma_data
	},
	{
		.compatible = "mediatek,mt6897-mml_pq_rdma",
		.data = &mt6985_pq_rdma_data
	},
	{
		.compatible = "mediatek,mt6899-mml_pq_rdma",
		.data = &mt6989_pq_rdma_data
	},
	{
		.compatible = "mediatek,mt6989-mml_pq_rdma",
		.data = &mt6989_pq_rdma_data
	},
	{
		.compatible = "mediatek,mt6991-mml_pq_rdma",
		.data = &mt6991_pq_rdma_data
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_pq_rdma_driver_dt_match);

struct platform_driver mml_pq_rdma_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mml-pq-rdma",
		.owner = THIS_MODULE,
		.of_match_table = mml_pq_rdma_driver_dt_match,
	},
};

//module_platform_driver(mml_pq_rdma_driver);

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
module_param_cb(pq_rdma_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(pq_rdma_debug, "mml pq_rdma debug case");

MODULE_AUTHOR("Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MediaTek SoC display MML RDMA driver");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
