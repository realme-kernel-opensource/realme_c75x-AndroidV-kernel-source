// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "devapc-mt6899.h"

static const struct mtk_device_num mtk6899_devices_num[] = {
	{SLAVE_TYPE_INFRA, VIO_SLAVE_NUM_INFRA, IRQ_TYPE_INFRA, DEVAPC_GET_INFRA},
	{SLAVE_TYPE_INFRA1, VIO_SLAVE_NUM_INFRA1, IRQ_TYPE_INFRA, DEVAPC_GET_INFRA},
	{SLAVE_TYPE_PERI_PAR, VIO_SLAVE_NUM_PERI_PAR, IRQ_TYPE_PERI, DEVAPC_GET_PERI},
	{SLAVE_TYPE_VLP, VIO_SLAVE_NUM_VLP, IRQ_TYPE_VLP, DEVAPC_GET_VLP},
	{SLAVE_TYPE_ADSP, VIO_SLAVE_NUM_ADSP, IRQ_TYPE_ADSP, DEVAPC_GET_ADSP},
	{SLAVE_TYPE_MMINFRA, VIO_SLAVE_NUM_MMINFRA, IRQ_TYPE_MMINFRA, DEVAPC_GET_MMINFRA},
	{SLAVE_TYPE_MMUP, VIO_SLAVE_NUM_MMUP, IRQ_TYPE_MMUP, DEVAPC_GET_MMUP},
	{SLAVE_TYPE_GPU, VIO_SLAVE_NUM_GPU, IRQ_TYPE_GPU, DEVAPC_GET_GPU},
	{SLAVE_TYPE_GPU1, VIO_SLAVE_NUM_GPU1, IRQ_TYPE_GPU, DEVAPC_GET_GPU},
};

/* Infra bus id table */
static const struct INFRAAXI_ID_INFO infra_mi_id_to_master[] = {
	{"MD_AP_M",             { 0, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"THERM_M",             { 1, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"CCU_M",               { 1, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0 } },
	{"THERM_M2",            { 1, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"HWCCF_M",             { 1, 0, 0, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"ADSPSYS_M1_M",        { 1, 0, 1, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0 } },
	{"CONN_M",              { 1, 0, 1, 0, 0, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0 } },
	{"VLPSYS_M",            { 1, 0, 1, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"SCP_M",               { 1, 0, 1, 0, 0, 0, 1, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"SSPM_M",              { 1, 0, 1, 0, 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"SPM_M@AHB-S",         { 1, 0, 1, 0, 0, 0, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"DPMSR_AHB_M@AHB-S",   { 1, 0, 1, 0, 0, 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"CPUM_M",              { 1, 0, 0, 1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"CQDMA_M",             { 1, 0, 0, 1, 0, 0, 1, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"DEBUG_M",             { 1, 0, 0, 1, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"GPU_EB_M",            { 1, 0, 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0 } },
	{"IPU_M",               { 1, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"INFRA_BUS_HRE_M",     { 1, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"NTH_EMI_GMC_M",       { 1, 0, 1, 0, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0 } },
	{"STH_EMI_GMC_M",       { 1, 0, 1, 0, 1, 1, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0 } },
	{"PERI2INFRA1_M",       { 0, 1, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"SPI0_M@AHB-M",        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0 } },
	{"SPI1_M@AHB-M",        { 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 2, 0, 0, 0, 0, 0 } },
	{"SPI2_M@AHB-M",        { 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 2, 0, 0, 0, 0, 0 } },
	{"SPI3_M@AHB-M",        { 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 2, 0, 0, 0, 0, 0 } },
	{"PWM_M@AHB-M",         { 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 2, 0, 0, 0, 0, 0 } },
	{"MSDC1_M",             { 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 2, 0, 0, 0, 0, 0 } },
	{"MSDC2_M",             { 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0 } },
	{"AUDIO_M@AHB-S",       { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0 } },
	{"APDMA_INT_M",         { 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 2, 2, 2, 0, 0, 0, 0 } },
	{"SPI4_M@AHB-M",        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 0 } },
	{"SPI5_M@AHB-M",        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 0 } },
	{"SPI6_M@AHB-M",        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 0 } },
	{"SPI7_M@AHB-M",        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 0 } },
	{"PCIE_M",              { 0, 1, 0, 0, 0, 0, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"APDMA_EXT_M",         { 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0 } },
	{"SSUSB_M",             { 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 2, 2, 0, 0, 0, 0, 0 } },
	{"SSUSB2_M",            { 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0 } },
	{"SSUSB3_M",            { 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 2, 2, 0, 0, 0, 0, 0 } },
	{"UFS0_M",              { 0, 1, 0, 0, 0, 0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"SSR_M",               { 0, 1, 0, 0, 1, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"DPMAIF_M",            { 0, 1, 0, 0, 0, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"MM2SLB1_M",           { 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
	{"HFRP2INFRA_M",        { 0, 1, 0, 1, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 1, 0 } },
	{"GCE_D_M",             { 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 } },
	{"GCE_M_M",             { 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 } },
	{"MCU_AP_M",            { 0, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
};

static const struct ADSPAXI_ID_INFO ADSP_mi12_id_to_master[] = {
	{"DSP_M1",            { 0, 0, 0, 2, 2, 2, 2, 0  } },
	{"DMA_M1",            { 0, 0, 1, 2, 0, 0, 0, 0  } },
	{"DSP_M2",            { 1, 0, 0, 2, 2, 2, 2, 0  } },
	{"DMA_M2",            { 1, 0, 1, 2, 0, 0, 0, 0  } },
	{"DMA_AXI_M1",        { 0, 1, 0, 0, 2, 2, 2, 2  } },
	{"IDMA_M",            { 0, 1, 1, 0, 2, 2, 2, 2  } },
	{"TINYXNNE",          { 0, 1, 0, 1, 2, 2, 2, 2  } },
	{"MASRC",             { 1, 1, 0, 2, 2, 2, 2, 0  } },
	{"AFE_M",             { 1, 1, 1, 2, 0, 0, 0, 0  } },
};

static const struct ADSPAXI_ID_INFO ADSP_mi17_id_to_master[] = {
	{"DSP_M1",            { 0, 0, 0, 2, 2, 2, 2, 0  } },
	{"DMA_M1",            { 0, 0, 1, 2, 0, 0, 0, 0  } },
	{"DSP_M2",            { 1, 0, 0, 2, 2, 2, 2, 0  } },
	{"DMA_M2",            { 1, 0, 1, 2, 0, 0, 0, 0  } },
	{"DMA_AXI_M1",        { 0, 1, 0, 0, 2, 2, 2, 2  } },
	{"IDMA_M",            { 0, 1, 1, 0, 2, 2, 2, 2  } },
	{"TINYXNNE",          { 0, 1, 0, 1, 2, 2, 2, 2  } },
	{"INFRA_M",           { 1, 1, 2, 2, 2, 2, 0, 2  } },
};

static const char * const adsp_domain[] = {
	"AP",
	"SSPM",
	"CONN",
	"SCP",
	"MCUPM",
	"CCU",
	"others",
	"others",
};

static const char * const mminfra_domain[] = {
	"AP",
	"SSPM",
	"CCU",
	"SCP",
	"GCE",
	"GZ",
	"MMuP",
	"others",
};

static const char *infra_mi_trans(uint32_t bus_id)
{
	int master_count = ARRAY_SIZE(infra_mi_id_to_master);
	const char *master = "UNKNOWN_MASTER_FROM_INFRA";
	int i, j;

	for (i = 0; i < master_count; i++) {
		for (j = 0; j < INFRAAXI_MI_BIT_LENGTH; j++) {
			if (infra_mi_id_to_master[i].bit[j] == 2)
				continue;
			if (((bus_id >> j) & 0x1) ==
					infra_mi_id_to_master[i].bit[j])
				continue;
			break;
		}
		if (j == INFRAAXI_MI_BIT_LENGTH) {
			pr_info(PFX "%s %s %s\n",
				"catch it from INFRAAXI_MI",
				"Master is:",
				infra_mi_id_to_master[i].master);
			master = infra_mi_id_to_master[i].master;
		}
	}

	return master;
}

static const char *adsp_mi_trans(uint32_t bus_id, int mi)
{
	int master_count = 0;
	const char *master = "UNKNOWN_MASTER_FROM_ADSP";
	int i, j;

	const struct ADSPAXI_ID_INFO *ADSP_mi_id_to_master;

	if (mi == ADSP_MI12) {
		ADSP_mi_id_to_master = ADSP_mi12_id_to_master;
		master_count = ARRAY_SIZE(ADSP_mi12_id_to_master);
	} else {
		ADSP_mi_id_to_master = ADSP_mi17_id_to_master;
		master_count = ARRAY_SIZE(ADSP_mi17_id_to_master);
	}

	for (i = 0; i < master_count; i++) {
		for (j = 0; j < ADSPAXI_MI_BIT_LENGTH; j++) {
			if (ADSP_mi_id_to_master[i].bit[j] == 2)
				continue;
			if (((bus_id >> j) & 0x1) ==
				ADSP_mi_id_to_master[i].bit[j])
				continue;
			break;
		}
		if (j == ADSPAXI_MI_BIT_LENGTH) {
			pr_debug(PFX "%s %s %s\n",
				"catch it from ADSPAXI_MI",
				"Master is:",
				ADSP_mi_id_to_master[i].master);
			master = ADSP_mi_id_to_master[i].master;
		}
	}
	return master;
}

static const char *mt6899_bus_id_to_master(uint32_t bus_id, uint32_t vio_addr,
		int slave_type, int shift_sta_bit, uint32_t domain)
{
	pr_debug(PFX "%s:0x%x, %s:0x%x, %s:0x%x, %s:%d\n",
		"bus_id", bus_id, "vio_addr", vio_addr,
		"slave_type", slave_type,
		"shift_sta_bit", shift_sta_bit);

	if (vio_addr <= SRAM_END_ADDR) {
		pr_info(PFX "vio_addr is from on-chip SRAMROM\n");
		return infra_mi_trans(bus_id);
	} else if ((vio_addr >= L3CACHE_0_START && vio_addr <= L3CACHE_0_END) ||
		(vio_addr >= L3CACHE_1_START && vio_addr <= L3CACHE_1_END) ||
		(vio_addr >= L3CACHE_2_START && vio_addr <= L3CACHE_2_END)) {
		pr_info(PFX "vio_addr is from L3Cache share SRAM\n");
		if ((bus_id & 0x1) == 0x1)
			return "IRQ2AXI_M";
		else
			return infra_mi_trans(bus_id >> 1);
	} else if (slave_type == SLAVE_TYPE_VLP) {
		/*
		 * For VLP slave type parse:
		 * 1. Using violation address to determine the
		 * transaction come from which MI node(MI1, MI2 and MI3).
		 * 2. And then, according axi id to find the master.
		 * 3. The way of bus id detection would be changed by different chips,
		 * so must update it.
		 */
		if ((vio_addr >= VLP_SCP_START_ADDR) && (vio_addr <= VLP_SCP_END_ADDR)) {
			/* mi3 */
			if ((bus_id & 0x3) == 0x0)
				return "SSPM_M";
			else if ((bus_id & 0x3) == 0x1)
				return "SPM_M or DPMSR_AHB_M";
			else if ((bus_id & 0x3) == 0x2)
				return infra_mi_trans(bus_id >> 2);
			else
				return "UNKNOWN_MASTER_TO_SCP";
		} else if (vio_addr <= VLP_INFRA_END || (vio_addr >= VLP_INFRA_1_START)) {
			/* mi1 */
			if ((bus_id & 0x3) == 0x0)
				return "SCP_M";
			else if ((bus_id & 0x3) == 0x1)
				return "SSPM_M";
			else if ((bus_id & 0x3) == 0x2)
				return "SPM_M or DPMSR_AHB_M";
			else
				return "UNKNOWN_MASTER_TO_INFRA";
		} else {
			/* mi2 */
			if ((bus_id & 0x3) == 0x0)
				return "SCP_M";
			else if ((bus_id & 0x3) == 0x1)
				return "SSPM_M";
			else if ((bus_id & 0x3) == 0x2)
				return "SPM_M or DPMSR_AHB_M";
			else if ((bus_id & 0x3) == 0x3)
				return infra_mi_trans(bus_id >> 2);
			else
				return "UNKNOWN_MASTER_TO_VLP";
		}
	} else if (slave_type == SLAVE_TYPE_ADSP) {
		/* infra slave */
		if ((vio_addr >= ADSP_INFRA_START && vio_addr <= ADSP_INFRA_END) ||
			(vio_addr >= ADSP_INFRA_1_START && vio_addr <= ADSP_INFRA_1_END) ||
			(vio_addr >= ADSP_INFRA_2_START && vio_addr <= ADSP_INFRA_2_END)) {
			return adsp_mi_trans(bus_id, ADSP_MI12);
		/* adsp misc slave */
		} else if (vio_addr >= ADSP_OTHER_START && vio_addr <= ADSP_OTHER_END) {
			if ((bus_id & 0x1) == 0x1)
				return "HRE";
			else if ((bus_id & 0x7) == 0x6)
				return adsp_domain[domain];
			else
				return adsp_mi_trans(bus_id >> 1, ADSP_MI17);
		/* adsp audio_pwr, dsp_pwr slave */
		} else {
			if ((bus_id & 0x3) == 0x3)
				return adsp_domain[domain];
			else
				return adsp_mi_trans(bus_id, ADSP_MI17);
		}
	} else if (slave_type == SLAVE_TYPE_MMINFRA) {
		/* ISP slave */
		if (((vio_addr >= IMG_START_ADDR) && (vio_addr <= IMG_END_ADDR)) ||
			((vio_addr >= CAM_START_ADDR) && (vio_addr <= CAM_END_ADDR))) {
			if ((bus_id & 0x1) == 0x0)
				return "GCEM_direct";
			else if ((bus_id & 0xf) == 0x1)
				return infra_mi_trans(bus_id >> 4);
			else if ((bus_id & 0xf) == 0x3)
				return "MMINFRA_HRE";
			else if ((bus_id & 0xf) == 0x5)
				return "GCED";
			else if ((bus_id & 0xf) == 0x7)
				return "GCEM";
			else if ((bus_id & 0xf) == 0x9)
				return "HFRP";
			else
				return mminfra_domain[domain];

		/* VENC/VDEC slave*/
		} else if ((vio_addr >= CODEC_START_ADDR) && (vio_addr <= CODEC_END_ADDR)) {
			if ((bus_id & 0x1) == 0x0)
				return "HFRP_direct";
			else if ((bus_id & 0xf) == 0x1)
				return infra_mi_trans(bus_id >> 4);
			else if ((bus_id & 0xf) == 0x3)
				return "MMINFRA_HRE";
			else if ((bus_id & 0xf) == 0x5)
				return "GCED";
			else if ((bus_id & 0xf) == 0x7)
				return "GCEM";
			else if ((bus_id & 0xf) == 0x9)
				return "HFRP";
			else
				return mminfra_domain[domain];

		/* DISP/OVL/MML */
		} else if (((vio_addr >= DISP_START_ADDR) && (vio_addr <= DISP_END_ADDR)) ||
			((vio_addr >= OVL_START_ADDR) && (vio_addr <= OVL_END_ADDR)) ||
			((vio_addr >= MML_START_ADDR) && (vio_addr <= MML_END_ADDR))) {
			if ((bus_id & 0x1) == 0x0)
				return "GCED_direct";
			else if ((bus_id & 0xf) == 0x1)
				return infra_mi_trans(bus_id >> 4);
			else if ((bus_id & 0xf) == 0x3)
				return "MMINFRA_HRE";
			else if ((bus_id & 0xf) == 0x5)
				return "GCED";
			else if ((bus_id & 0xf) == 0x7)
				return "GCEM";
			else if ((bus_id & 0xf) == 0x9)
				return "HFRP";
			else
				return mminfra_domain[domain];

		/* other mminfra slave*/
		} else {
			if ((bus_id & 0x7) == 0x0)
				return infra_mi_trans(bus_id >> 3);
			else if ((bus_id & 0x7) == 0x1)
				return "MMINFRA_HRE";
			else if ((bus_id & 0x7) == 0x2)
				return "GCED";
			else if ((bus_id & 0x7) == 0x3)
				return "GCEM";
			else if ((bus_id & 0xf) == 0x4)
				return "HFRP";
			else
				return mminfra_domain[domain];
		}
	} else if (slave_type == SLAVE_TYPE_MMUP) {
		return mminfra_domain[domain];
	} else if (slave_type == SLAVE_TYPE_GPU) {
		/* PD_BUS */
		if (domain == 0x6) {
			if (((bus_id & 0xf800) == 0x0) && ((bus_id & 0x3f) == 0x2a)) {
				if (((bus_id >> 6) & 0x3) == 0x0)
					return "GPUEB_RV33_P";
				else if (((bus_id >> 6) & 0x3) == 0x1)
					return "GPUEB_RV33_D";
				else if (((bus_id >> 6) & 0x3) == 0x2)
					return "GPUEB_DMA";
				else
					return "GPUEB_AutoDMA";
			} else
				return "GPU_BRCAST";
		} else
			return infra_mi_trans(bus_id);
	} else if (slave_type == SLAVE_TYPE_GPU1) {
		/* PD_BUS */
		if ((vio_addr >= GPU1_PD_START) && (vio_addr <= GPU1_PD_END)) {
			if (domain == 0x6) {
				if ((bus_id & 0x3) == 0x0)
					return "GPUEB_RV33_P";
				else if ((bus_id & 0x3) == 0x1)
					return "GPUEB_RV33_D";
				else if ((bus_id & 0x3) == 0x2)
					return "GPUEB_DMA";
				else
					return "GPUEB_AutoDMA";
			} else
				return infra_mi_trans(bus_id);
		/* AO_BUS */
		} else {
			if (domain == 0x6) {
				if (((bus_id >> 6) & 0x3) == 0x0)
					return "GPUEB_RV33_P";
				else if (((bus_id >> 6) & 0x3) == 0x1)
					return "GPUEB_RV33_D";
				else if (((bus_id >> 6) & 0x3) == 0x2)
					return "GPUEB_DMA";
				else
					return "GPUEB_AutoDMA";
			} else
				return infra_mi_trans(bus_id);
		}
	} else
		return infra_mi_trans(bus_id);
}

/* violation index corresponds to subsys */
const char *index_to_subsys(int slave_type, uint32_t vio_index,
		uint32_t vio_addr)
{
	int i;

	pr_debug(PFX "%s %s %d, %s %d, %s 0x%x\n",
			__func__,
			"slave_type", slave_type,
			"vio_index", vio_index,
			"vio_addr", vio_addr);

	/* Filter by violation index */
	if (slave_type == SLAVE_TYPE_INFRA) {
		for (i = 0; i < VIO_SLAVE_NUM_INFRA; i++) {
			if (vio_index == mt6899_devices_infra[i].vio_index)
				return mt6899_devices_infra[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_INFRA1) {
		for (i = 0; i < VIO_SLAVE_NUM_INFRA1; i++) {
			if (vio_index == mt6899_devices_infra1[i].vio_index)
				return mt6899_devices_infra1[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_PERI_PAR) {
		for (i = 0; i < VIO_SLAVE_NUM_PERI_PAR; i++) {
			if (vio_index == mt6899_devices_peri_par[i].vio_index)
				return mt6899_devices_peri_par[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_VLP) {
		for (i = 0; i < VIO_SLAVE_NUM_VLP; i++) {
			if (vio_index == mt6899_devices_vlp[i].vio_index)
				return mt6899_devices_vlp[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_ADSP) {
		for (i = 0; i < VIO_SLAVE_NUM_ADSP; i++) {
			if (vio_index == mt6899_devices_adsp[i].vio_index)
				return mt6899_devices_adsp[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_MMINFRA) {
		for (i = 0; i < VIO_SLAVE_NUM_MMINFRA; i++) {
			if (vio_index == mt6899_devices_mminfra[i].vio_index)
				return mt6899_devices_mminfra[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_MMUP) {
		for (i = 0; i < VIO_SLAVE_NUM_MMUP; i++) {
			if (vio_index == mt6899_devices_mmup[i].vio_index)
				return mt6899_devices_mmup[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_GPU) {
		for (i = 0; i < VIO_SLAVE_NUM_GPU; i++) {
			if (vio_index == mt6899_devices_gpu[i].vio_index)
				return mt6899_devices_gpu[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_GPU1) {
		for (i = 0; i < VIO_SLAVE_NUM_GPU1; i++) {
			if (vio_index == mt6899_devices_gpu1[i].vio_index)
				return mt6899_devices_gpu1[i].device;
		}
	}


	return "OUT_OF_BOUND";
}

/*
 * This function has been removed. Now keep it for common code.
 * return only.
 */
static void mm2nd_vio_handler(void __iomem *infracfg,
			      struct mtk_devapc_vio_info *vio_info,
			      bool mdp_vio, bool disp2_vio, bool mmsys_vio)
{
}

static uint32_t mt6899_shift_group_get(int slave_type, uint32_t vio_idx)
{
	return 31;
}

/*
 * This function has been removed. Now keep it for common code.
 * return only.
 */
void devapc_catch_illegal_range(phys_addr_t phys_addr, size_t size)
{
}

static struct mtk_devapc_dbg_status mt6899_devapc_dbg_stat = {
	.enable_ut = PLAT_DBG_UT_DEFAULT,
	.enable_KE = PLAT_DBG_KE_DEFAULT,
	.enable_AEE = PLAT_DBG_AEE_DEFAULT,
	.enable_WARN = PLAT_DBG_WARN_DEFAULT,
	.enable_dapc = PLAT_DBG_DAPC_DEFAULT,
};

static const char * const slave_type_to_str[] = {
	"SLAVE_TYPE_INFRA",
	"SLAVE_TYPE_INFRA1",
	"SLAVE_TYPE_PERI_PAR",
	"SLAVE_TYPE_VLP",
	"SLAVE_TYPE_ADSP",
	"SLAVE_TYPE_MMINFRA",
	"SLAVE_TYPE_MMUP",
	"SLAVE_TYPE_GPU",
	"SLAVE_TYPE_GPU1",
	"WRONG_SLAVE_TYPE",
};

static int mtk_vio_mask_sta_num[] = {
	VIO_MASK_STA_NUM_INFRA,
	VIO_MASK_STA_NUM_INFRA1,
	VIO_MASK_STA_NUM_PERI_PAR,
	VIO_MASK_STA_NUM_VLP,
	VIO_MASK_STA_NUM_ADSP,
	VIO_MASK_STA_NUM_MMINFRA,
	VIO_MASK_STA_NUM_MMUP,
	VIO_MASK_STA_NUM_GPU,
	VIO_MASK_STA_NUM_GPU1,
};

static struct mtk_devapc_vio_info mt6899_devapc_vio_info = {
	.vio_mask_sta_num = mtk_vio_mask_sta_num,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
	.mdp_vio_idx = MDP_VIO_INDEX,
	.disp2_vio_idx = DISP2_VIO_INDEX,
	.mmsys_vio_idx = MMSYS_VIO_INDEX,
	.sramrom_slv_type = SRAMROM_SLAVE_TYPE,
	.mm2nd_slv_type = MM2ND_SLAVE_TYPE,
};

static const struct mtk_infra_vio_dbg_desc mt6899_vio_dbgs = {
	.vio_dbg_mstid = INFRA_VIO_DBG_MSTID,
	.vio_dbg_mstid_start_bit = INFRA_VIO_DBG_MSTID_START_BIT,
	.vio_dbg_dmnid = INFRA_VIO_DBG_DMNID,
	.vio_dbg_dmnid_start_bit = INFRA_VIO_DBG_DMNID_START_BIT,
	.vio_dbg_w_vio = INFRA_VIO_DBG_W_VIO,
	.vio_dbg_w_vio_start_bit = INFRA_VIO_DBG_W_VIO_START_BIT,
	.vio_dbg_r_vio = INFRA_VIO_DBG_R_VIO,
	.vio_dbg_r_vio_start_bit = INFRA_VIO_DBG_R_VIO_START_BIT,
	.vio_addr_high = INFRA_VIO_ADDR_HIGH,
	.vio_addr_high_start_bit = INFRA_VIO_ADDR_HIGH_START_BIT,
};

static const struct mtk_sramrom_sec_vio_desc mt6899_sramrom_sec_vios = {
	.vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const uint32_t mt6899_devapc_pds[] = {
	PD_VIO_MASK_OFFSET,
	PD_VIO_STA_OFFSET,
	PD_VIO_DBG0_OFFSET,
	PD_VIO_DBG1_OFFSET,
	PD_VIO_DBG2_OFFSET,
	PD_APC_CON_OFFSET,
	PD_SHIFT_STA_OFFSET,
	PD_SHIFT_SEL_OFFSET,
	PD_SHIFT_CON_OFFSET,
	PD_VIO_DBG3_OFFSET,
};

static struct mtk_devapc_soc mt6899_data = {
	.dbg_stat = &mt6899_devapc_dbg_stat,
	.slave_type_arr = slave_type_to_str,
	.slave_type_num = SLAVE_TYPE_NUM,
	.device_info[SLAVE_TYPE_INFRA] = mt6899_devices_infra,
	.device_info[SLAVE_TYPE_INFRA1] = mt6899_devices_infra1,
	.device_info[SLAVE_TYPE_PERI_PAR] = mt6899_devices_peri_par,
	.device_info[SLAVE_TYPE_VLP] = mt6899_devices_vlp,
	.device_info[SLAVE_TYPE_ADSP] = mt6899_devices_adsp,
	.device_info[SLAVE_TYPE_MMINFRA] = mt6899_devices_mminfra,
	.device_info[SLAVE_TYPE_MMUP] = mt6899_devices_mmup,
	.device_info[SLAVE_TYPE_GPU] = mt6899_devices_gpu,
	.device_info[SLAVE_TYPE_GPU1] = mt6899_devices_gpu1,
	.ndevices = mtk6899_devices_num,
	.vio_info = &mt6899_devapc_vio_info,
	.vio_dbgs = &mt6899_vio_dbgs,
	.sramrom_sec_vios = &mt6899_sramrom_sec_vios,
	.devapc_pds = mt6899_devapc_pds,
	.irq_type_num = IRQ_TYPE_NUM,
	.subsys_get = &index_to_subsys,
	.master_get = &mt6899_bus_id_to_master,
	.mm2nd_vio_handler = &mm2nd_vio_handler,
	.shift_group_get = mt6899_shift_group_get,
};

static const struct of_device_id mt6899_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6899-devapc" },
	{},
};

static const struct dev_pm_ops devapc_dev_pm_ops = {
	.suspend_noirq	= devapc_suspend_noirq,
	.resume_noirq = devapc_resume_noirq,
};

static int mt6899_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6899_data);
}

static int mt6899_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6899_devapc_driver = {
	.probe = mt6899_devapc_probe,
	.remove = mt6899_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = mt6899_devapc_dt_match,
		.pm = &devapc_dev_pm_ops,
	},
};

module_platform_driver(mt6899_devapc_driver);

MODULE_DESCRIPTION("Mediatek MT6899 Device APC Driver");
MODULE_AUTHOR("kyle-jk.liao <kyle-jk.liao@mediatek.com>");
MODULE_LICENSE("GPL");
