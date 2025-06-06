/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef _DT_BINDINGS_CLK_MT6779_H
#define _DT_BINDINGS_CLK_MT6779_H

/* TOPCKGEN */
#define TOP_MUX_AXI 1
#define TOP_MUX_MM 2
#define TOP_MUX_CAM 3
#define TOP_MUX_MFG 4
#define TOP_MUX_CAMTG 5
#define TOP_MUX_UART 6
#define TOP_MUX_SPI 7
#define TOP_MUX_MSDC50_0_HCLK 8
#define TOP_MUX_MSDC50_0 9
#define TOP_MUX_MSDC30_1 10
#define TOP_MUX_MSDC30_2 11
#define TOP_MUX_AUDIO 12
#define TOP_MUX_AUD_INTBUS 13
#define TOP_MUX_FPWRAP_ULPOSC 14
#define TOP_MUX_SCP 15
#define TOP_MUX_ATB 16
#define TOP_MUX_SSPM 17
#define TOP_MUX_DPI0 18
#define TOP_MUX_SCAM 19
#define TOP_MUX_AUD_1 20
#define TOP_MUX_AUD_2 21
#define TOP_MUX_DISP_PWM 22
#define TOP_MUX_SSUSB_TOP_XHCI 23
#define TOP_MUX_USB_TOP 24
#define TOP_MUX_SPM 25
#define TOP_MUX_I2C 26
#define TOP_MUX_F52M_MFG 27
#define TOP_MUX_SENINF 28
#define TOP_MUX_DXCC 29
#define TOP_MUX_CAMTG2 30
#define TOP_MUX_AUD_ENG1 31
#define TOP_MUX_AUD_ENG2 32
#define TOP_MUX_FAES_UFSFDE 33
#define TOP_MUX_FUFS 34
#define TOP_MUX_IMG	35
#define TOP_MUX_DSP	36
#define TOP_MUX_DSP1	37
#define TOP_MUX_DSP2	38
#define TOP_MUX_IPU_IF	39
#define TOP_MUX_CAMTG3	40
#define TOP_MUX_CAMTG4	41
#define TOP_MUX_PMICSPI	42
#define TOP_MAINPLL_CK 43
#define TOP_MAINPLL_D2 44
#define TOP_MAINPLL_D3 45
#define TOP_MAINPLL_D5 46
#define TOP_MAINPLL_D7 47
#define TOP_MAINPLL_D2_D2 48
#define TOP_MAINPLL_D2_D4 49
#define TOP_MAINPLL_D2_D8 50
#define TOP_MAINPLL_D2_D16 51
#define TOP_MAINPLL_D3_D2 52
#define TOP_MAINPLL_D3_D4 53
#define TOP_MAINPLL_D3_D8 54
#define TOP_MAINPLL_D5_D2 55
#define TOP_MAINPLL_D5_D4 56
#define TOP_MAINPLL_D7_D2 57
#define TOP_MAINPLL_D7_D4 58
#define TOP_UNIVPLL_CK 59
#define TOP_UNIVPLL_D2 60
#define TOP_UNIVPLL_D3 61
#define TOP_UNIVPLL_D5 62
#define TOP_UNIVPLL_D7 63
#define TOP_UNIVPLL_D2_D2 64
#define TOP_UNIVPLL_D2_D4 65
#define TOP_UNIVPLL_D2_D8 66
#define TOP_UNIVPLL_D3_D2 67
#define TOP_UNIVPLL_D3_D4 68
#define TOP_UNIVPLL_D3_D8 69
#define TOP_UNIVPLL_D5_D2 70
#define TOP_UNIVPLL_D5_D4 71
#define TOP_UNIVPLL_D5_D8 72
#define TOP_APLL1_CK 73
#define TOP_APLL1_D2 74
#define TOP_APLL1_D4 75
#define TOP_APLL1_D8 76
#define TOP_APLL2_CK 77
#define TOP_APLL2_D2 78
#define TOP_APLL2_D4 79
#define TOP_APLL2_D8 80
#define TOP_TVDPLL_CK 81
#define TOP_TVDPLL_D2 82
#define TOP_TVDPLL_D4 83
#define TOP_TVDPLL_D8 84
#define TOP_TVDPLL_D16 85
#define TOP_MSDCPLL_CK 86
#define TOP_MSDCPLL_D2 87
#define TOP_MSDCPLL_D4 88
#define TOP_MSDCPLL_D8 89
#define TOP_MSDCPLL_D16 90
#define TOP_AD_OSC_CK 91
#define TOP_OSC_D2 92
#define TOP_OSC_D4 93
#define TOP_OSC_D8 94
#define TOP_OSC_D16 95
#define TOP_F26M_CK_D2 96
#define TOP_MFGPLL_CK	97
#define TOP_UNIVP_192M_CK	98
#define TOP_UNIVP_192M_D2	99
#define TOP_UNIVP_192M_D4	100
#define TOP_UNIVP_192M_D8	101
#define TOP_UNIVP_192M_D16	102
#define TOP_UNIVP_192M_D32	103
#define TOP_MMPLL_CK 104
#define TOP_MMPLL_D4	105
#define TOP_MMPLL_D4_D2	106
#define TOP_MMPLL_D4_D4	107
#define TOP_MMPLL_D5	108
#define TOP_MMPLL_D5_D2	109
#define TOP_MMPLL_D5_D4	110
#define TOP_MMPLL_D6	111
#define TOP_MMPLL_D7	112
#define TOP_CLK26M	113
#define TOP_CLK13M	114
#define TOP_MUX_ADSP	115
#define TOP_MUX_DPMAIF	116
#define TOP_MUX_VENC	117
#define TOP_MUX_VDEC	118
#define TOP_MUX_CAMTM	119
#define TOP_MUX_PWM	120
#define TOP_ADSPPLL_CK	121
#define TOP_I2S0_M_SEL	122
#define TOP_I2S1_M_SEL	123
#define TOP_I2S2_M_SEL	124
#define TOP_I2S3_M_SEL	125
#define TOP_I2S4_M_SEL	126
#define TOP_I2S5_M_SEL	127
#define TOP_APLL12_DIV0	128
#define TOP_APLL12_DIV1	129
#define TOP_APLL12_DIV2	130
#define TOP_APLL12_DIV3	131
#define TOP_APLL12_DIV4	132
#define TOP_APLL12_DIVB	133
#define TOP_APLL12_DIV5	134
#define TOP_MUX_IPE	135
#define TOP_MUX_DPE	136
#define TOP_MUX_CCU	137
#define TOP_MUX_DSP3	138
#define TOP_MUX_SENINF1	139
#define TOP_MUX_SENINF2	140
#define TOP_MUX_AUDIO_H	141
#define TOP_MUX_CAMTG5	142
#define TOP_TVDPLL_MAINPLL_D2_CK 143
#define TOP_AD_OSC2_CK 144
#define TOP_OSC2_D2 145
#define TOP_OSC2_D3 146
#define TOP_FMEM_466M_CK 147
#define TOP_ADSPPLL_D4	148
#define TOP_ADSPPLL_D5	149
#define TOP_ADSPPLL_D6	150
#define TOP_OSC_D10 151
#define TOP_UNIVPLL_D3_D16 152
#define TOP_APUPLL_CK	153
#define TOP_NR_CLK	154

/* APMIXED */
#define APMIXED_ARMPLL_LL 1
#define APMIXED_ARMPLL_BL 2
#define APMIXED_ARMPLL_BB 3
#define APMIXED_CCIPLL 4
#define APMIXED_MAINPLL 5
#define APMIXED_UNIV2PLL 6
#define APMIXED_MSDCPLL 7
#define APMIXED_ADSPPLL 8
#define APMIXED_MMPLL 9
#define APMIXED_MFGPLL 10
#define APMIXED_TVDPLL 11
#define APMIXED_APLL1 12
#define APMIXED_APLL2 13
#define APMIXED_SSUSB26M 14
#define APMIXED_APPLL26M 15
#define APMIXED_MIPIC0_26M 16
#define APMIXED_MDPLLGP26M 17
#define APMIXED_MMSYS_F26M 18
#define APMIXED_UFS26M 19
#define APMIXED_MIPIC1_26M 20
#define APMIXED_MEMPLL26M 21
#define APMIXED_CLKSQ_LVPLL_26M 22
#define APMIXED_MIPID0_26M 23
#define APMIXED_MIPID1_26M 24
#define APMIXED_APUPLL 25
#define APMIXED_NR_CLK 26

/* CAMSYS */
#define CAMSYS_LARB6_CGPDN 1
#define CAMSYS_LARB7_CGPDN 2
#define CAMSYS_GALS_CGPDN 3
#define CAMSYS_CAM_CGPDN 4
#define CAMSYS_CAMTG_CGPDN 5
#define CAMSYS_SENINF_CGPDN 6
#define CAMSYS_CAMSV0_CGPDN 7
#define CAMSYS_CAMSV1_CGPDN 8
#define CAMSYS_CCU_CGPDN 9
#define CAMSYS_FAKE_ENG_CGPDN 10
#define CAMSYS_NR_CLK 11

/* INFRACFG_AO */
#define INFRACFG_AO_PMIC_CG_TMR 1
#define INFRACFG_AO_PMIC_CG_AP 2
#define INFRACFG_AO_PMIC_CG_MD 3
#define INFRACFG_AO_PMIC_CG_CONN 4
#define INFRACFG_AO_SCPSYS_CG 5
#define INFRACFG_AO_SEJ_CG 6
#define INFRACFG_AO_APXGPT_CG 7
#define INFRACFG_AO_ICUSB_CG 8
#define INFRACFG_AO_GCE_CG 9
#define INFRACFG_AO_THERM_CG 10
#define INFRACFG_AO_I2C0_CG 11
#define INFRACFG_AO_I2C1_CG 12
#define INFRACFG_AO_I2C2_CG 13
#define INFRACFG_AO_I2C3_CG 14
#define INFRACFG_AO_PWM_HCLK_CG 15
#define INFRACFG_AO_PWM1_CG 16
#define INFRACFG_AO_PWM2_CG 17
#define INFRACFG_AO_PWM3_CG 18
#define INFRACFG_AO_PWM4_CG 19
#define INFRACFG_AO_PWM_CG 20
#define INFRACFG_AO_UART0_CG 21
#define INFRACFG_AO_UART1_CG 22
#define INFRACFG_AO_UART2_CG 23
#define INFRACFG_AO_UART3_CG 24
#define INFRACFG_AO_GCE_26M_CG 25
#define INFRACFG_AO_CQ_DMA_FPC_CG 26
#define INFRACFG_AO_BTIF_CG 27
#define INFRACFG_AO_SPI0_CG 28
#define INFRACFG_AO_MSDC0_CG 29
#define INFRACFG_AO_MSDC1_CG 30
#define INFRACFG_AO_MSDC2_CG 31
#define INFRACFG_AO_MSDC0_SCK_CG 32
#define INFRACFG_AO_DVFSRC_CG 33
#define INFRACFG_AO_GCPU_CG 34
#define INFRACFG_AO_TRNG_CG 35
#define INFRACFG_AO_AUXADC_CG 36
#define INFRACFG_AO_CPUM_CG 37
#define INFRACFG_AO_CCIF1_AP_CG 38
#define INFRACFG_AO_CCIF1_MD_CG 39
#define INFRACFG_AO_AUXADC_MD_CG 40
#define INFRACFG_AO_MSDC1_SCK_CG 41
#define INFRACFG_AO_MSDC2_SCK_CG 42
#define INFRACFG_AO_AP_DMA_CG 43
#define INFRACFG_AO_XIU_CG 44
#define INFRACFG_AO_DEVICE_APC_CG 45
#define INFRACFG_AO_CCIF_AP_CG 46
#define INFRACFG_AO_DEBUGSYS_CG 47
#define INFRACFG_AO_AUDIO_CG 48
#define INFRACFG_AO_CCIF_MD_CG 49
#define INFRACFG_AO_DXCC_SEC_CORE_CG 50
#define INFRACFG_AO_DXCC_AO_CG 51
#define INFRACFG_AO_DRAMC_F26M_CG 52
#define INFRACFG_AO_IRTX_CG 53
#define INFRACFG_AO_DISP_PWM_CG 54
#define INFRACFG_AO_DPMAIF_CK  55
#define INFRACFG_AO_AUDIO_26M_BCLK_CK 56
#define INFRACFG_AO_SPI1_CG 57
#define INFRACFG_AO_I2C4_CG 58
#define INFRACFG_AO_MODEM_TEMP_SHARE_CG 59
#define INFRACFG_AO_SPI2_CG 60
#define INFRACFG_AO_SPI3_CG 61
#define INFRACFG_AO_UNIPRO_SCK_CG 62
#define INFRACFG_AO_UNIPRO_TICK_CG 63
#define INFRACFG_AO_UFS_MP_SAP_BCLK_CG 64
#define INFRACFG_AO_MD32_BCLK_CG 65
#define INFRACFG_AO_SSPM_CG 66
#define INFRACFG_AO_UNIPRO_MBIST_CG 67
#define INFRACFG_AO_SSPM_BUS_HCLK_CG 68
#define INFRACFG_AO_I2C5_CG 69
#define INFRACFG_AO_I2C5_ARBITER_CG 70
#define INFRACFG_AO_I2C5_IMM_CG 71
#define INFRACFG_AO_I2C1_ARBITER_CG 72
#define INFRACFG_AO_I2C1_IMM_CG 73
#define INFRACFG_AO_I2C2_ARBITER_CG 74
#define INFRACFG_AO_I2C2_IMM_CG 75
#define INFRACFG_AO_SPI4_CG 76
#define INFRACFG_AO_SPI5_CG 77
#define INFRACFG_AO_CQ_DMA_CG 78
#define INFRACFG_AO_UFS_CG 79
#define INFRACFG_AO_AES_UFSFDE_CG 80
#define INFRACFG_AO_UFS_TICK_CG 81
#define INFRACFG_AO_MSDC0_SELF_CG 82
#define INFRACFG_AO_MSDC1_SELF_CG 83
#define INFRACFG_AO_MSDC2_SELF_CG 84
#define INFRACFG_AO_SSPM_26M_SELF_CG 85
#define INFRACFG_AO_SSPM_32K_SELF_CG 86
#define INFRACFG_AO_UFS_AXI_CG 87
#define INFRACFG_AO_I2C6_CG 88
#define INFRACFG_AO_AP_MSDC0_CG 89
#define INFRACFG_AO_MD_MSDC0_CG 90
#define INFRACFG_AO_USB_CG 91
#define INFRACFG_AO_DEVMPU_BCLK_CG 92
#define INFRACFG_AO_CCIF2_AP_CG 93
#define INFRACFG_AO_CCIF2_MD_CG 94
#define INFRACFG_AO_CCIF3_AP_CG 95
#define INFRACFG_AO_CCIF3_MD_CG 96
#define INFRACFG_AO_SEJ_F13M_CG 97
#define INFRACFG_AO_AES_BCLK_CG 98
#define INFRACFG_AO_I2C7_CG 99
#define INFRACFG_AO_I2C8_CG 100
#define INFRACFG_AO_FBIST2FPC_CG 101
#define INFRACFG_AO_CCIF4_AP_CG 102
#define INFRACFG_AO_CCIF4_MD_CG 103
#define INFRACFG_AO_FADSP_CG 104
#define INFRACFG_AO_SSUSB_XHCI_CG 105
#define INFRACFG_AO_SPI6_CG 106
#define INFRACFG_AO_SPI7_CG 107
#define INFRACFG_AO_FADSP_26M_CG 108
#define INFRACFG_AO_FADSP_32K_CG 109
#define INFRACFG_AO_DEVICE_APC_SYNC_CG 110
#define INFRACFG_AO_NR_CLK 111

/* MFGCFG */
#define MFGCFG_BG3D 1
#define MFGCFG_NR_CLK 2

/* IMG */
#define	IMG_LARB5 1
#define	IMG_LARB4 2
#define	IMG_DIP 3
#define	IMG_FDVT 4
#define	IMG_DPE 5
#define	IMG_RSC 6
#define	IMG_MFB	7
#define	IMG_WPE_A	8
#define	IMG_WPE_B 9
#define	IMG_OWE 10
#define IMG_NR_CLK 11


/* MMSYS_CONFIG */
#define	MMSYS_SMI_COMMON 1
#define	MMSYS_SMI_LARB0	2
#define	MMSYS_SMI_LARB1	3
#define	MMSYS_GALS_COMM0	4
#define	MMSYS_GALS_COMM1	5
#define	MMSYS_GALS_CCU2MM	6
#define	MMSYS_GALS_IPU12MM	7
#define	MMSYS_GALS_IMG2MM	8
#define	MMSYS_GALS_CAM2MM	9
#define	MMSYS_GALS_IPU2MM	10
#define	MMSYS_MDP_DL_TXCK	11
#define	MMSYS_IPU_DL_TXCK	12
#define	MMSYS_MDP_RDMA0	13
#define	MMSYS_MDP_RDMA1	14
#define	MMSYS_MDP_RSZ0	15
#define	MMSYS_MDP_RSZ1	16
#define	MMSYS_MDP_TDSHP	17
#define	MMSYS_MDP_WROT0	18
#define	MMSYS_FAKE_ENG	19
#define	MMSYS_DISP_OVL0	20
#define	MMSYS_DISP_OVL0_2L	21
#define	MMSYS_DISP_OVL1_2L	22
#define	MMSYS_DISP_RDMA0	23
#define	MMSYS_DISP_RDMA1	24
#define	MMSYS_DISP_WDMA0	25
#define	MMSYS_DISP_COLOR0	26
#define	MMSYS_DISP_CCORR0	27
#define	MMSYS_DISP_AAL0		28
#define	MMSYS_DISP_GAMMA0	29
#define	MMSYS_DISP_DITHER0	30
#define	MMSYS_DISP_SPLIT	31
#define	MMSYS_DSI0_MM_CK	32
#define	MMSYS_DSI0_IF_CK	33
#define	MMSYS_DPI_MM_CK	34
#define	MMSYS_DPI_IF_CK	35
#define	MMSYS_FAKE_ENG2	36
#define	MMSYS_MDP_DL_RX_CK	37
#define	MMSYS_IPU_DL_RX_CK	38
#define	MMSYS_26M	39
#define	MMSYS_MMSYS_R2Y	40
#define	MMSYS_DISP_RSZ	41
#define MMSYS_MDP_WDMA0 42
#define MMSYS_MDP_AAL 43
#define MMSYS_MDP_HDR 44
#define MMSYS_DBI_MM_CK 45
#define MMSYS_DBI_IF_CK 46
#define	MMSYS_MDP_WROT1	47
#define	MMSYS_DISP_POSTMASK0 48
#define	MMSYS_DISP_HRT_BW 49
#define	MMSYS_DISP_OVL_FBDC 50
#define MMSYS_CONFIG_NR_CLK 51

/* VDEC_GCON */
#define	VDEC_VDEC	1
#define	VDEC_LARB1	2
#define VDEC_GCON_NR_CLK 3

/* VENC_GCON */
#define VENC_GCON_LARB 1
#define VENC_GCON_VENC 2
#define VENC_GCON_JPGENC 3
#define VENC_GCON_GALS 4
#define VENC_GCON_NR_CLK 5

/* AUDIO */
#define	AUDIO_AFE		1
#define	AUDIO_22M		2
#define	AUDIO_24M		3
#define	AUDIO_APLL2_TUNER	4
#define	AUDIO_APLL_TUNER	5
#define	AUDIO_TDM		6
#define	AUDIO_ADC		7
#define	AUDIO_DAC		8
#define	AUDIO_DAC_PREDIS	9
#define	AUDIO_TML		10
#define	AUDIO_NLE		11
#define	AUDIO_I2S1_BCLK_SW	12
#define AUDIO_I2S2_BCLK_SW	13
#define AUDIO_I2S3_BCLK_SW	14
#define AUDIO_I2S4_BCLK_SW	15
#define AUDIO_I2S5_BCLK_SW	16
#define	AUDIO_CONN_I2S_ASRC	17
#define	AUDIO_GENERAL1_ASRC	18
#define	AUDIO_GENERAL2_ASRC	19
#define	AUDIO_DAC_HIRES		20
#define AUDIO_PDN_ADDA6_ADC	21
#define AUDIO_ADC_HIRES		22
#define AUDIO_ADC_HIRES_TML	23
#define AUDIO_ADDA6_ADC_HIRES	24
#define AUDIO_3RD_DAC		25
#define AUDIO_3RD_DAC_PREDIS	26
#define AUDIO_3RD_DAC_TML	27
#define AUDIO_3RD_DAC_HIRES	28
#define AUDIO_NR_CLK		29

/* APU_CONN */
#define	APU_CONN_APU_CG	1
#define	APU_CONN_AHB_CG	2
#define	APU_CONN_AXI_CG	3
#define	APU_CONN_ISP_CG	4
#define	APU_CONN_CAM_ADL_CG	5
#define	APU_CONN_IMG_ADL_CG	6
#define APU_CONN_EMI_26M_CG	7
#define APU_CONN_VPU_UDI_CG	8
#define APU_CONN_NR_CLK		9

/* APU_CORE0 */
#define	APU0_JTAG_CG	1
#define	APU0_AXI_M_CG	2
#define	APU0_APU_CG	3
#define APU0_NR_CLK	4

/* APU_CORE1 */
#define	APU1_JTAG_CG	1
#define	APU1_AXI_M_CG	2
#define	APU1_APU_CG	3
#define APU1_NR_CLK	4

/* APU_VCORE */
#define	APU_VCORE_AHB_CG	1
#define	APU_VCORE_AXI_CG	2
#define	APU_VCORE_ADL_CG	3
#define	APU_VCORE_QOS_CG	4
#define APU_VCORE_NR_CLK	5

/* SCP_SYS */
#define SCP_SYS_MD1  1
#define SCP_SYS_CONN  2
#define SCP_SYS_DIS  3
#define SCP_SYS_MFG1  4
#define SCP_SYS_ISP  5
#define SCP_SYS_VEN  6
#define SCP_SYS_MFG0  7
#define SCP_SYS_AUDIO  8
#define SCP_SYS_CAM  9
#define SCP_SYS_MFG3  10
#define SCP_SYS_MFG2  11
#define SCP_SYS_MFG5  12
#define SCP_SYS_VDE  13
#define SCP_SYS_VPU_VCORE_DORMANT  14
#define SCP_SYS_VPU_VCORE_SHUTDOWN  15
#define SCP_SYS_VPU_CORE0_DORMANT  16
#define SCP_SYS_VPU_CORE0_SHUTDOWN  17
#define SCP_SYS_VPU_CORE1_DORMANT  18
#define SCP_SYS_VPU_CORE1_SHUTDOWN  19
#define SCP_SYS_VPU_CONN_DORMANT  20
#define SCP_SYS_VPU_CONN_SHUTDOWN  21
#define SCP_SYS_VPU_CORE2_DORMANT  22
#define SCP_SYS_VPU_CORE2_SHUTDOWN  23
#define SCP_SYS_MFG4  24
#define SCP_NR_SYSS  25

#endif				/* _DT_BINDINGS_CLK_MT6779_H */
