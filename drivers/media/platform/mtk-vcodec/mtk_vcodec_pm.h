/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_PM_H_
#define _MTK_VCODEC_PM_H_

//#include "ion_drv.h"
//#include "mach/mt_iommu.h"

#define MTK_PLATFORM_STR        "platform:mt6873" //TODO: remove when venc ready
#define MTK_VDEC_RACING_INFO_OFFSET  0x100
#define DEC_MAX_UBE_INDEX         32
#define MTK_VDEC_RACING_INFO_SIZE (DEC_MAX_UBE_INDEX*4)
#define MTK_VDEC_MAX_LARB_COUNT 4

#define MTK_MAX_VDEC_CLK_COUNT 10
#define MTK_MAX_VDEC_CLK_SOC_COUNT 3
#define MTK_MAX_VDEC_CLK_CORE_COUNT 5
#define MTK_MAX_VDEC_CLK_LAT_COUNT 3
#define MTK_MAX_VDEC_CLK_MAIN_COUNT 4
#define MTK_MAX_VDEC_CLK_PARENT_SET_COUNT 4

// to distingush clock type, the clock name in dts should start with these prefix
#define MTK_VDEC_CLK_LAT_PREFIX		"LAT"
#define MTK_VDEC_CLK_CORE_PREFIX	"CORE"
#define MTK_VDEC_CLK_SOC_PREFIX		"SOC"
#define MTK_VDEC_CLK_MAIN_PREFIX	"MAIN"

#define MTK_MAX_VENC_CLK_COUNT 4
#define MTK_MAX_VENC_CLK_CORE_COUNT 4
#define MTK_VENC_MAX_LARB_COUNT 3
#define VDEC_HIGHEST_FREQ 880000000

#define IS_SPECIFIC_CLK_TYPE(clk_name, prefix) \
		(!strncmp(clk_name, prefix, strlen(prefix)) ? true : false)

struct mtk_vcodec_clk {
	unsigned int clk_id; // the array index of vdec_clks
	const char *clk_name;
};

struct mtk_vcodec_parent_clk {
	struct mtk_vcodec_clk parent;
	struct mtk_vcodec_clk child;
};

struct mtk_vdec_clks_data {
	struct mtk_vcodec_clk			main_clks[MTK_MAX_VDEC_CLK_MAIN_COUNT];
	unsigned int					main_clks_len;
	struct mtk_vcodec_clk			soc_clks[MTK_MAX_VDEC_CLK_SOC_COUNT];
	unsigned int					soc_clks_len;
	struct mtk_vcodec_clk			lat_clks[MTK_MAX_VDEC_CLK_LAT_COUNT];
	unsigned int					lat_clks_len;
	struct mtk_vcodec_clk			core_clks[MTK_MAX_VDEC_CLK_CORE_COUNT];
	unsigned int					core_clks_len;
	struct mtk_vcodec_parent_clk	parent_clks[MTK_MAX_VDEC_CLK_PARENT_SET_COUNT];
	unsigned int					parent_clks_len;
};

struct mtk_venc_clks_data {
	struct mtk_vcodec_clk			core_clks[MTK_MAX_VENC_CLK_CORE_COUNT];
	unsigned int					core_clks_len;
};


/**
 * struct mtk_vcodec_pm - Power management data structure
 */
struct mtk_vcodec_pm {
	struct clk      *vdec_bus_clk_src;
	struct clk      *vencpll;

	struct clk      *vcodecpll;
	struct clk      *univpll_d2;
	struct clk      *clk_cci400_sel;
	struct clk      *vdecpll;
	struct clk      *vdecpll_ck;
	struct clk      *vdec_sel;
	struct clk      *vencpll_d2;
	struct clk      *venc_sel;
	struct clk      *univpll1_d2;
	struct clk      *venc_lt_sel;
	struct clk      *img_resz;
	struct device   *larbvdecs[MTK_VDEC_MAX_LARB_COUNT];
	struct device   *larbvencs[MTK_VENC_MAX_LARB_COUNT];
	struct device   *larbvenclt;
	struct device   *dev;
	struct device_node      *chip_node;
	struct mtk_vcodec_dev   *mtkdev;

	struct mtk_vdec_clks_data vdec_clks_data;
	struct clk *vdec_clks[MTK_MAX_VDEC_CLK_COUNT];
	struct mtk_venc_clks_data venc_clks_data;
	struct clk *venc_clks[MTK_MAX_VENC_CLK_COUNT];

	atomic_t dec_active_cnt;
	__u32 vdec_racing_info[MTK_VDEC_RACING_INFO_SIZE];
	struct mutex dec_racing_info_mutex;
};

enum mtk_dec_dump_addr_type {
	DUMP_VDEC_IN_BUF,
	DUMP_VDEC_OUT_BUF,
	DUMP_VDEC_REF_BUF,
	DUMP_VDEC_MV_BUF,
	DUMP_VDEC_UBE_BUF,
};

// The port name should same with dts
#define MTK_VDEC_M4U_PORT_NAME_MC		"M4U_PORT_VDEC_MC"
#define MTK_VDEC_M4U_PORT_NAME_UFO		"M4U_PORT_VDEC_UFO"
#define MTK_VDEC_M4U_PORT_NAME_PP		"M4U_PORT_VDEC_PP"
#define MTK_VDEC_M4U_PORT_NAME_PRED_RD		"M4U_PORT_VDEC_PRED_RD"
#define MTK_VDEC_M4U_PORT_NAME_PRED_WR		"M4U_PORT_VDEC_PRED_WR"
#define MTK_VDEC_M4U_PORT_NAME_PPWRAP		"M4U_PORT_VDEC_PPWRAP"
#define MTK_VDEC_M4U_PORT_NAME_TILE		"M4U_PORT_VDEC_TILE"
#define MTK_VDEC_M4U_PORT_NAME_VLD		"M4U_PORT_VDEC_VLD"
#define MTK_VDEC_M4U_PORT_NAME_VLD2		"M4U_PORT_VDEC_VLD2"
#define MTK_VDEC_M4U_PORT_NAME_AVC_MV		"M4U_PORT_VDEC_AVC_MV"
#define MTK_VDEC_M4U_PORT_NAME_RG_CTRL_DMA	"M4U_PORT_VDEC_RG_CTRL_DMA"
#define MTK_VDEC_M4U_PORT_NAME_UFO_ENC		"M4U_PORT_VDEC_UFO_ENC"
#define MTK_VDEC_M4U_PORT_NAME_LAT0_VLD		"M4U_PORT_VDEC_LAT0_VLD"
#define MTK_VDEC_M4U_PORT_NAME_LAT0_VLD2	"M4U_PORT_VDEC_LAT0_VLD2"
#define MTK_VDEC_M4U_PORT_NAME_LAT0_AVC_MV	"M4U_PORT_VDEC_LAT0_AVC_MV"
#define MTK_VDEC_M4U_PORT_NAME_LAT0_PRED_RD	"M4U_PORT_VDEC_LAT0_PRED_RD"
#define MTK_VDEC_M4U_PORT_NAME_LAT0_TILE	"M4U_PORT_VDEC_LAT0_TILE"
#define MTK_VDEC_M4U_PORT_NAME_LAT0_WDMA	"M4U_PORT_VDEC_LAT0_WDMA"
#define MTK_VDEC_M4U_PORT_NAME_LAT0_RG_CTRL_DMA	"M4U_PORT_VDEC_LAT0_RG_CTRL_DMA"
#define MTK_VDEC_M4U_PORT_NAME_LAT0_MC		"M4U_PORT_VDEC_LAT0_MC"
#define MTK_VDEC_M4U_PORT_NAME_LAT0_UFO_ENC	"M4U_PORT_VDEC_LAT0_UFO_ENC"
#define MTK_VDEC_M4U_PORT_NAME_LAT0_UFO_ENC_C	"M4U_PORT_VDEC_LAT0_UFO_ENC_C"
#define MTK_VDEC_M4U_PORT_NAME_LAT0_UNIWRAP	"M4U_PORT_VDEC_LAT0_UNIWRAP"
#define MTK_VDEC_M4U_PORT_NAME_UNIWRAP		"M4U_PORT_VDEC_UNIWRAP"
#define MTK_VDEC_M4U_PORT_NAME_VIDEO_UP_SEC	"M4U_PORT_VDEC_VIDEO_UP_SEC"
#define MTK_VDEC_M4U_PORT_NAME_VIDEO_UP_NOR	"M4U_PORT_VDEC_VIDEO_UP_NOR"
#define MTK_VDEC_M4U_PORT_NAME_UP_1		"M4U_PORT_VIDEO_UP_1"
#define MTK_VDEC_M4U_PORT_NAME_UP_2		"M4U_PORT_VIDEO_UP_2"
#define MTK_VDEC_M4U_PORT_NAME_UP_3		"M4U_PORT_VIDEO_UP_3"
#define MTK_VDEC_M4U_PORT_NAME_UP_4		"M4U_PORT_VIDEO_UP_4"

// The reg name should same with dts
#define MTK_VDEC_REG_NAME_VDEC_BASE				"VDEC_BASE"
#define MTK_VDEC_REG_NAME_VDEC_SYS				"VDEC_SYS"
#define MTK_VDEC_REG_NAME_VDEC_UFO				"VDEC_UFO"
#define MTK_VDEC_REG_NAME_VDEC_VLD				"VDEC_VLD"
#define MTK_VDEC_REG_NAME_VDEC_MC				"VDEC_MC"
#define MTK_VDEC_REG_NAME_VDEC_MV				"VDEC_MV"
#define MTK_VDEC_REG_NAME_VDEC_MISC				"VDEC_MISC"
#define MTK_VDEC_REG_NAME_VDEC_LAT_MISC				"VDEC_LAT_MISC"
#define MTK_VDEC_REG_NAME_VDEC_LAT_VLD				"VDEC_LAT_VLD"
#define MTK_VDEC_REG_NAME_VDEC_SOC_GCON				"VDEC_SOC_GCON"
#define MTK_VDEC_REG_NAME_VDEC_RACING_CTRL			"VDEC_RACING_CTRL"
#define MTK_VDEC_REG_NAME_VDEC_LAT_AVC_VLD			"VDEC_LAT_AVC_VLD"
#define MTK_VDEC_REG_NAME_VDEC_AVC_VLD				"VDEC_AVC_VLD"
#define MTK_VDEC_REG_NAME_VDEC_AV1_VLD				"VDEC_AV1_VLD"

#define MTK_VDEC_REG_NAME_VDEC_CORE1_MISC			"VDEC_CORE1_MISC"
#define MTK_VDEC_REG_NAME_VDEC_LAT1_MISC			"VDEC_LAT1_MISC"
#define MTK_VDEC_REG_NAME_VDEC_LAT_WDMA			"VDEC_LAT_WDMA"
#define MTK_VDEC_REG_NAME_VDEC_LAT1_WDMA			"VDEC_LAT1_WDMA"
#define MTK_VDEC_REG_NAME_VDEC_LAT_TOP			"VDEC_LAT_TOP"
#define MTK_VDEC_REG_NAME_VDEC_UFO_ENC			"VDEC_UFO_ENC"

#define MTK_VDEC_REG_NAME_VENC_SYS				"VENC_SYS"
#define MTK_VDEC_REG_NAME_VENC_C1_SYS				"VENC_C1_SYS"
#define MTK_VDEC_REG_NAME_VENC_C2_SYS				"VENC_C2_SYS"
#define MTK_VDEC_REG_NAME_VENC_GCON				"VENC_GCON"


enum mtk_dec_dtsi_reg_idx {
	VDEC_BASE,
	VDEC_SYS,
	VDEC_UFO,
	VDEC_VLD,
	VDEC_MC,
	VDEC_MV,
	VDEC_MISC,
	VDEC_LAT_MISC,
	VDEC_LAT_VLD,
	VDEC_SOC_GCON,
	VDEC_RACING_CTRL,
	VDEC_CORE1_MISC,
	VDEC_LAT1_MISC,
	VDEC_LAT_WDMA,
	VDEC_LAT1_WDMA,
	VDEC_LAT_TOP,
	VDEC_UFO_ENC,
	VDEC_LAT_AVC_VLD,
	VDEC_AVC_VLD,
	VDEC_AV1_VLD,
	NUM_MAX_VDEC_REG_BASE,
};

enum mtk_enc_dtsi_reg_idx {
	VENC_SYS,
	VENC_C1_SYS,
	VENC_C2_SYS,
	VENC_GCON,
	NUM_MAX_VENC_REG_BASE
};
#endif /* _MTK_VCODEC_PM_H_ */
