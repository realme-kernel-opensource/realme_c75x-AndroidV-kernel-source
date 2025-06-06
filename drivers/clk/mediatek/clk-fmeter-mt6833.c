// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-fmeter.h"
#include "clk-mt6833-fmeter.h"

#define FM_TIMEOUT		30

static DEFINE_SPINLOCK(meter_lock);
#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)

/*
 * clk fmeter
 */

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

#define FMCLK2(_t, _i, _n, _o, _p, _c) { .type = _t, \
		.id = _i, .name = _n, .ofs = _o, .pdn = _p, .ck_div = _c}
#define FMCLK(_t, _i, _n, _c) { .type = _t, .id = _i, .name = _n, .ck_div = _c}

static const struct fmeter_clk fclks[] = {
	/* CKGEN Part */
	FMCLK2(CKGEN, FM_AXI_CK, "fm_axi_ck", 0x0010, 7, 1),
	FMCLK2(CKGEN, FM_SPM_CK, "fm_spm_ck", 0x0010, 15, 1),
	FMCLK2(CKGEN, FM_SCP_CK, "fm_scp_ck", 0x0010, 23, 1),
	FMCLK2(CKGEN, FM_B, "fm_b", 0x0010, 31, 1),
	FMCLK2(CKGEN, FM_DISP_CK, "fm_disp_ck", 0x0020, 7, 1),
	FMCLK2(CKGEN, FM_MDP_CK, "fm_mdp_ck", 0x0020, 15, 1),
	FMCLK2(CKGEN, FM_IMG1_CK, "fm_img1_ck", 0x0020, 23, 1),
	FMCLK2(CKGEN, FM_IMG2_CK, "fm_img2_ck", 0x0020, 31, 1),
	FMCLK2(CKGEN, FM_IPE_CK, "fm_ipe_ck", 0x0030, 7, 1),
	FMCLK2(CKGEN, FM_DPE_CK, "fm_dpe_ck", 0x0030, 15, 1),
	FMCLK2(CKGEN, FM_CAM_CK, "fm_cam_ck", 0x0030, 23, 1),
	FMCLK2(CKGEN, FM_CCU_CK, "fm_ccu_ck", 0x0030, 31, 1),
	FMCLK2(CKGEN, FM_MFG_REF_CK, "fm_mfg_ref_ck", 0x0050, 23, 1),
	FMCLK2(CKGEN, FM_CAMTG_CK, "fm_camtg_ck", 0x0050, 31, 1),
	FMCLK2(CKGEN, FM_CAMTG2_CK, "fm_camtg2_ck", 0x0060, 7, 1),
	FMCLK2(CKGEN, FM_CAMTG3_CK, "fm_camtg3_ck", 0x0060, 15, 1),
	FMCLK2(CKGEN, FM_CAMTG4_CK, "fm_camtg4_ck", 0x0060, 23, 1),
	FMCLK2(CKGEN, FM_CAMTG5_CK, "fm_camtg5_ck", 0x0060, 31, 1),
	FMCLK2(CKGEN, FM_UART_CK, "fm_uart_ck", 0x0070, 15, 1),
	FMCLK2(CKGEN, FM_SPI_CK, "fm_spi_ck", 0x0070, 23, 1),
	FMCLK2(CKGEN, FM_MSDC5HCLK_CK, "fm_msdc5hclk_ck", 0x0070, 31, 1),
	FMCLK2(CKGEN, FM_MSDC50_0_CK, "fm_msdc50_0_ck", 0x0080, 7, 1),
	FMCLK2(CKGEN, FM_MSDC30_1_CK, "fm_msdc30_1_ck", 0x0080, 15, 1),
	FMCLK2(CKGEN, FM_AUDIO_CK, "fm_audio_ck", 0x0080, 31, 1),
	FMCLK2(CKGEN, FM_AUD_INTBUS_CK, "fm_aud_intbus_ck", 0x0090, 7, 1),
	FMCLK2(CKGEN, FM_PWRAP_ULPOSC_CK, "fm_pwrap_ulposc_ck", 0x0090, 15, 1),
	FMCLK2(CKGEN, FM_ATB_CK, "fm_atb_ck", 0x0090, 23, 1),
	FMCLK2(CKGEN, FM_SSPM_CK, "fm_sspm_ck", 0x0090, 31, 1),
	FMCLK2(CKGEN, FM_SCAM_CK, "fm_scam_ck", 0x00A0, 15, 1),
	FMCLK2(CKGEN, FM_DISP_PWM_CK, "fm_disp_pwm_ck", 0x00A0, 23, 1),
	FMCLK2(CKGEN, FM_USB_CK, "fm_usb_ck", 0x00A0, 31, 1),
	FMCLK2(CKGEN, FM_USB_XHCI_CK, "fm_usb_xhci_ck", 0x00B0, 7, 1),
	FMCLK2(CKGEN, FM_I2C_CK, "fm_i2c_ck", 0x00B0, 15, 1),
	FMCLK2(CKGEN, FM_SENINF_CK, "fm_seninf_ck", 0x00B0, 23, 1),
	FMCLK2(CKGEN, FM_SENINF1_CK, "fm_seninf1_ck", 0x00B0, 31, 1),
	FMCLK2(CKGEN, FM_SENINF2_CK, "fm_seninf2_ck", 0x00C0, 7, 1),
	FMCLK2(CKGEN, FM_DXCC_CK, "fm_dxcc_ck", 0x00C0, 31, 1),
	FMCLK2(CKGEN, FM_AUD_ENGEN1_CK, "fm_aud_engen1_ck", 0x00D0, 7, 1),
	FMCLK2(CKGEN, FM_AUD_ENGEN2_CK, "fm_aud_engen2_ck", 0x00D0, 15, 1),
	FMCLK2(CKGEN, FM_AES_UFSFDE_CK, "fm_aes_ufsfde_ck", 0x00D0, 23, 1),
	FMCLK2(CKGEN, FM_U_CK, "fm_u_ck", 0x00D0, 31, 1),
	FMCLK2(CKGEN, FM_AUD_1_CK, "fm_aud_1_ck", 0x00E0, 7, 1),
	FMCLK2(CKGEN, FM_AUD_2_CK, "fm_aud_2_ck", 0x00E0, 15, 1),
	FMCLK2(CKGEN, FM_ADSP_CK, "fm_adsp_ck", 0x00E0, 23, 1),
	FMCLK2(CKGEN, FM_DPMAIF_MAIN_CK, "fm_dpmaif_main_ck", 0x00E0, 31, 1),
	FMCLK2(CKGEN, FM_VENC_CK, "fm_venc_ck", 0x00F0, 7, 1),
	FMCLK2(CKGEN, FM_VDEC_CK, "fm_vdec_ck", 0x00F0, 15, 1),
	FMCLK2(CKGEN, FM_CAMTM_CK, "fm_camtm_ck", 0x00F0, 23, 1),
	FMCLK2(CKGEN, FM_PWM_CK, "fm_pwm_ck", 0x00F0, 31, 1),
	FMCLK2(CKGEN, FM_AUDIO_H_CK, "fm_audio_h_ck", 0x0100, 7, 1),
	FMCLK2(CKGEN, FM_SPMI_MST_CK, "fm_spmi_mst_ck", 0x0100, 15, 1),
	FMCLK2(CKGEN, FM_DVFSRC_CK, "fm_dvfsrc_ck", 0x0100, 23, 1),
	/* ABIST Part */
	FMCLK(ABIST, FM_ADSPPLL_CK, "fm_adsppll_ck", 1),
	FMCLK(ABIST, FM_APLL1_CK, "fm_apll1_ck", 1),
	FMCLK(ABIST, FM_APLL2_CK, "fm_apll2_ck", 1),
	FMCLK(ABIST, FM_APPLLGP_MON_FM_CK, "fm_appllgp_mon_fm_ck", 1),
	FMCLK(ABIST, FM_APUPLL_CK, "fm_apupll_ck", 1),
	FMCLK(ABIST, FM_ARMPLL_BL_CK, "fm_armpll_bl_ck", 1),
	FMCLK(ABIST, FM_NPUPLL_CK, "fm_npupll_ck", 1),
	FMCLK(ABIST, FM_ARMPLL_LL_CK, "fm_armpll_ll_ck", 1),
	FMCLK(ABIST, FM_CCIPLL_CK, "fm_ccipll_ck", 1),
	FMCLK(ABIST, FM_CSI0A_CDPHY_DELAYCAL_CK, "fm_csi0a_cdphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI0B_CDPHY_DELAYCAL_CK, "fm_csi0b_cdphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI1A_DPHY_DELAYCAL_CK, "fm_csi1a_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI1B_DPHY_DELAYCAL_CK, "fm_csi1b_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI2A_DPHY_DELAYCAL_CK, "fm_csi2a_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI2B_DPHY_DELAYCAL_CK, "fm_csi2b_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI3A_DPHY_DELAYCAL_CK, "fm_csi3a_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_CSI3B_DPHY_DELAYCAL_CK, "fm_csi3b_dphy_delaycal_ck", 1),
	FMCLK(ABIST, FM_DSI0_LNTC_DSICLK, "fm_dsi0_lntc_dsiclk", 1),
	FMCLK(ABIST, FM_DSI0_MPPLL_TST_CK, "fm_dsi0_mppll_tst_ck", 1),
	FMCLK(ABIST, FM_MAINPLL_CK, "fm_mainpll_ck", 1),
	FMCLK(ABIST, FM_MDPLL_FS26M_CK, "fm_mdpll_fs26m_ck", 1),
	FMCLK(ABIST, FM_MGPLL_CK, "fm_mgpll_ck", 1),
	FMCLK(ABIST, FM_MPLL_CK, "fm_mpll_ck", 1),
	FMCLK(ABIST, FM_APLL1_CK_2, "fm_apll1_ck_2", 1),
	FMCLK(ABIST, FM_MMPLL_D3_CK, "fm_mmpll_d3_ck", 1),
	FMCLK(ABIST, FM_MSDCPLL_CK, "fm_msdcpll_ck", 1),
	FMCLK(ABIST, FM_RCLRPLL_DIV4_CH2, "fm_rclrpll_div4_ch2", 1),
	FMCLK(ABIST, FM_RCLRPLL_DIV4_CH13, "fm_rclrpll_div4_ch13", 1),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CH2, "fm_rphypll_div4_ch2", 1),
	FMCLK(ABIST, FM_RPHYPLL_DIV4_CH13, "fm_rphypll_div4_ch13", 1),
	FMCLK(ABIST, FM_TVDPLL_CK, "fm_tvdpll_ck", 1),
	FMCLK(ABIST, FM_ULPOSC2_CK, "fm_ulposc2_ck", 1),
	FMCLK(ABIST, FM_ULPOSC_CK, "fm_ulposc_ck", 1),
	FMCLK(ABIST, FM_UNIVPLL_CK, "fm_univpll_ck", 1),
	FMCLK(ABIST, FM_USB20_192M_CK, "fm_usb20_192m_ck", 1),
	FMCLK(ABIST, FM_MPLL_52M_DIV, "fm_mpll_52m_div", 1),
	FMCLK(ABIST, FM_U_CLK2FREQ, "fm_u_clk2freq", 1),
	FMCLK(ABIST, FM_WBG_DIG_BPLL_CK, "fm_wbg_dig_bpll_ck", 1),
	FMCLK(ABIST, FM_WBG_DIG_WPLL_CK960, "fm_wbg_dig_wpll_ck960", 1),
	FMCLK(ABIST, FMEM_AFT_CH0, "fmem_aft_ch0", 1),
	FMCLK(ABIST, FMEM_AFT_CH1, "fmem_aft_ch1", 1),
	FMCLK(ABIST, FMEM_AFT_CH2, "fmem_aft_ch2", 1),
	FMCLK(ABIST, FMEM_AFT_CH3, "fmem_aft_ch3", 1),
	FMCLK(ABIST, FMEM_BFE_CH0, "fmem_bfe_ch0", 1),
	FMCLK(ABIST, FMEM_BFE_CH1, "fmem_bfe_ch1", 1),
	FMCLK(ABIST, FM_466M_FMEM_INFRASYS, "fm_466m_fmem_infrasys", 1),
	FMCLK(ABIST, FM_MCUSYS_ARM_OUT_ALL, "fm_mcusys_arm_out_all", 1),
	FMCLK(ABIST, FM_MSDC01_IN_CK, "fm_msdc01_in_ck", 1),
	FMCLK(ABIST, FM_MSDC02_IN_CK, "fm_msdc02_in_ck", 1),
	FMCLK(ABIST, FM_MSDC11_IN_CK, "fm_msdc11_in_ck", 1),
	FMCLK(ABIST, FM_MSDC12_IN_CK, "fm_msdc12_in_ck", 1),
	FMCLK(ABIST, FM_MSDC21_IN_CK, "fm_msdc21_in_ck", 1),
	FMCLK(ABIST, FM_MSDC22_IN_CK, "fm_msdc22_in_ck", 1),
	FMCLK(ABIST, FM_RTC32K_I, "fm_rtc32k_i", 1),
	FMCLK(ABIST, FM_CKMON1_CK, "fm_ckmon1_ck", 1),
	FMCLK(ABIST, FM_CKMON2_CK, "fm_ckmon2_ck", 1),
	FMCLK(ABIST, FM_CKMON3_CK, "fm_ckmon3_ck", 1),
	FMCLK(ABIST, FM_CKMON4_CK, "fm_ckmon4_ck", 1),
	/* ABIST_2 Part */
	FMCLK(ABIST_2, FM_APLL_I2S0_M_CK, "fm_apll_i2s0_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S1_M_CK, "fm_apll_i2s1_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S2_M_CK, "fm_apll_i2s2_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S3_M_CK, "fm_apll_i2s3_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S4_M_CK, "fm_apll_i2s4_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S4_B_CK, "fm_apll_i2s4_b_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S5_M_CK, "fm_apll_i2s5_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S6_M_CK, "fm_apll_i2s6_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S7_M_CK, "fm_apll_i2s7_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S8_M_CK, "fm_apll_i2s8_m_ck", 1),
	FMCLK(ABIST_2, FM_APLL_I2S9_M_CK, "fm_apll_i2s9_m_ck", 1),
	FMCLK2(ABIST_2, FM_AES_MSDCFDE_CK, "fm_aes_msdcfde_ck", 0x0100, 31, 1),
	FMCLK(ABIST_2, FM_MCUPM_CK, "fm_mcupm_ck", 1),
	FMCLK2(ABIST_2, FM_SFLASH_CK, "fm_sflash_ck", 0x0110, 15, 1),
	FMCLK(ABIST_2, FM_UNIPLL_SES_CK, "fm_unipll_ses_ck", 1),
	FMCLK(ABIST_2, FM_ULPOSC_CK_2, "fm_ulposc_ck_2", 1),
	FMCLK(ABIST_2, FM_ULPOSC_CORE_CK, "fm_ulposc_core_ck", 1),
	FMCLK(ABIST_2, FM_SRCK_CK, "fm_srck_ck", 1),
	FMCLK(ABIST_2, FM_MAIN_H728M_CK, "fm_main_h728m_ck", 1),
	FMCLK(ABIST_2, FM_MAIN_H546M_CK, "fm_main_h546m_ck", 1),
	FMCLK(ABIST_2, FM_MAIN_H436P8M_CK, "fm_main_h436p8m_ck", 1),
	FMCLK(ABIST_2, FM_MAIN_H364M_CK, "fm_main_h364m_ck", 1),
	FMCLK(ABIST_2, FM_MAIN_H312M_CK, "fm_main_h312m_ck", 1),
	FMCLK(ABIST_2, FM_UNIV_1248M_CK, "fm_univ_1248m_ck", 1),
	FMCLK(ABIST_2, FM_UNIV_832M_CK, "fm_univ_832m_ck", 1),
	FMCLK(ABIST_2, FM_UNIV_624M_CK, "fm_univ_624m_ck", 1),
	FMCLK(ABIST_2, FM_UNIV_499M_CK, "fm_univ_499m_ck", 1),
	FMCLK(ABIST_2, FM_UNIV_416M_CK, "fm_univ_416m_ck", 1),
	FMCLK(ABIST_2, FM_UNIV_356P6M_CK, "fm_univ_356p6m_ck", 1),
	FMCLK(ABIST_2, FM_MMPLL_D3_CK_2, "fm_mmpll_d3_ck_2", 1),
	FMCLK(ABIST_2, FM_MMPLL_D4_CK, "fm_mmpll_d4_ck", 1),
	FMCLK(ABIST_2, FM_MMPLL_D5_CK, "fm_mmpll_d5_ck", 1),
	FMCLK(ABIST_2, FM_MMPLL_D6_CK, "fm_mmpll_d6_ck", 1),
	FMCLK(ABIST_2, FM_MMPLL_D7_CK, "fm_mmpll_d7_ck", 1),
	FMCLK(ABIST_2, FM_MMPLL_D9_CK, "fm_mmpll_d9_ck", 1),
	{},
};

#define _CKGEN(x)		(topck_base + (x))
#define CLK_MISC_CFG_0		_CKGEN(0x140)
#define CLK_DBG_CFG		_CKGEN(0x17C)
#define CLK26CALI_0		_CKGEN(0x220)
#define CLK26CALI_1		_CKGEN(0x224)

static void __iomem *topck_base;
static void __iomem *apmixed_base;
static void __iomem *spm_base;

const struct fmeter_clk *mt6833_get_fmeter_clks(void)
{
	return fclks;
}

static unsigned int check_mux_pdn(unsigned int ID)
{
	int i;

	if ((ID > 0) && (ID < 64)) {
		for (i = 0; i < ARRAY_SIZE(fclks) - 1; i++)
			if (fclks[i].id == ID)
				break;
		if (i >= ARRAY_SIZE(fclks) - 1)
			return 1;
		if ((clk_readl(topck_base + fclks[i].ofs)
				& BIT(fclks[i].pdn)))
			return 1;
		else
			return 0;
	} else
		return 1;
}

static int __mt_get_freq(unsigned int ID, int type)
{
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 __maybe_unused = 0;
	unsigned long flags;
	int output = 0, i = 0;

	fmeter_lock(flags);
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	}

	if (type == CKGEN) {
		clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
		clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFFFC0FC) | (ID << 8) | (0x1));
	} else if (type == ABIST) {
		clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
		clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC) | (ID << 16));
	} else if (type == ABIST_2) {
		clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
		clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xC0FFFFFC) | (ID << 24) | (0x2));
	} else
		return 0;

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x0000);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	i = 0;
	do {
		udelay(10);
		i++;
		if (i > FM_TIMEOUT)
			break;
	} while (clk_readl(CLK26CALI_0) & 0x10);

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/

	clk_writel(CLK26CALI_0, 0x0000);
	fmeter_unlock(flags);

	if (i > FM_TIMEOUT)
		return 0;
	if ((output * 4) < 25000) {
		pr_notice("%s: CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			clk_readl(CLK_DBG_CFG),
			clk_readl(CLK_MISC_CFG_0),
			clk_readl(CLK26CALI_0),
			clk_readl(CLK26CALI_1));
	}

	return (output * 4);
}

/* need implement ckgen&abist api here */

/* implement done */

static unsigned int mt6833_get_ckgen_freq(unsigned int ID)
{
	if (check_mux_pdn(ID)) {
		pr_notice("ID-%d: MUX PDN, return 0.\n", ID);
		return 0;
	}

	return __mt_get_freq(ID, CKGEN);
}

static unsigned int mt6833_get_abist_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST);
}

static unsigned int mt6833_get_abist2_freq(unsigned int ID)
{
	return __mt_get_freq(ID, ABIST_2);
}

static unsigned int mt6833_get_fmeter_freq(unsigned int id, enum  FMETER_TYPE type)
{
	if (type == CKGEN)
		return mt6833_get_ckgen_freq(id);
	else if (type == ABIST)
		return mt6833_get_abist_freq(id);
	else if (type == ABIST_2)
		return mt6833_get_abist2_freq(id);

	return FT_NULL;
}

static int mt6833_get_fmeter_id(enum FMETER_ID fid)
{
	if (fid == FID_DISP_PWM)
		return FM_DISP_PWM_CK;
	else if (fid == FID_ULPOSC1)
		return FM_ULPOSC_CK;
	else if (fid == FID_ULPOSC2)
		return FM_ULPOSC2_CK;

	return FID_NULL;
}

static void __iomem *get_base_from_comp(const char *comp)
{
	struct device_node *node;
	static void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, comp);
	if (node) {
		base = of_iomap(node, 0);
		if (!base) {
			pr_err("%s() can't find iomem for %s\n",
					__func__, comp);
			return ERR_PTR(-EINVAL);
		}

		return base;
	}

	pr_err("%s can't find compatible node\n", __func__);

	return ERR_PTR(-EINVAL);
}

/*
 * init functions
 */

static struct fmeter_ops fm_ops = {
	.get_fmeter_clks = mt6833_get_fmeter_clks,
	.get_ckgen_freq = mt6833_get_ckgen_freq,
	.get_abist_freq = mt6833_get_abist_freq,
	.get_abist2_freq = mt6833_get_abist2_freq,
	.get_fmeter_freq = mt6833_get_fmeter_freq,
	.get_fmeter_id = mt6833_get_fmeter_id,
};


static int clk_fmeter_mt6833_probe(struct platform_device *pdev)
{
	topck_base = get_base_from_comp("mediatek,mt6833-topckgen");
	if (IS_ERR(topck_base))
		goto ERR;

	apmixed_base = get_base_from_comp("mediatek,mt6833-apmixedsys");
	if (IS_ERR(apmixed_base))
		goto ERR;

	spm_base = get_base_from_comp("mediatek,mt6833-scpsys");
	if (IS_ERR(spm_base))
		goto ERR;

	fmeter_set_ops(&fm_ops);

	return 0;
ERR:
	pr_err("%s can't find base\n", __func__);

	return -EINVAL;
}

static struct platform_driver clk_fmeter_mt6833_drv = {
	.probe = clk_fmeter_mt6833_probe,
	.driver = {
		.name = "clk-fmeter-mt6833",
		.owner = THIS_MODULE,
	},
};

static int __init clk_fmeter_init(void)
{
	static struct platform_device *clk_fmeter_dev;

	clk_fmeter_dev = platform_device_register_simple("clk-fmeter-mt6833", -1, NULL, 0);
	if (IS_ERR(clk_fmeter_dev))
		pr_warn("unable to register clk-fmeter device");

	return platform_driver_register(&clk_fmeter_mt6833_drv);
}

static void __exit clk_fmeter_exit(void)
{
	platform_driver_unregister(&clk_fmeter_mt6833_drv);
}

subsys_initcall(clk_fmeter_init);
module_exit(clk_fmeter_exit);
MODULE_LICENSE("GPL");
