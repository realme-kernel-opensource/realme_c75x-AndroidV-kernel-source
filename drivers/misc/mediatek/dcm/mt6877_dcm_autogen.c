// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

// #include <mt-plat/mtk_io.h>
// #include <mt-plat/sync_write.h>
/* #include <mt-plat/mtk_secure_api.h> */
#include <linux/io.h>

#include <mt6877_dcm_internal.h>
#include <mt6877_dcm_autogen.h>
#include <mtk_dcm.h>
/*====================auto gen code 20210105=====================*/
#define INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK ((0x1f << 12) | \
			(0x1 << 17) | \
			(0x1 << 18))
#define INFRACFG_AO_AXIMEM_BUS_DCM_REG0_ON ((0x10 << 12) | \
			(0x1 << 17) | \
			(0x0 << 18))
#define INFRACFG_AO_AXIMEM_BUS_DCM_REG0_OFF ((0x10 << 12) | \
			(0x0 << 17) | \
			(0x1 << 18))

bool dcm_infracfg_ao_aximem_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(INFRA_AXIMEM_IDLE_BIT_EN_0) &
		INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_AXIMEM_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_aximem_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_aximem_bus_dcm'" */
		reg_write(INFRA_AXIMEM_IDLE_BIT_EN_0,
			(reg_read(INFRA_AXIMEM_IDLE_BIT_EN_0) &
			~INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_AXIMEM_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_aximem_bus_dcm'" */
		reg_write(INFRA_AXIMEM_IDLE_BIT_EN_0,
			(reg_read(INFRA_AXIMEM_IDLE_BIT_EN_0) &
			~INFRACFG_AO_AXIMEM_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_AXIMEM_BUS_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1f << 5) | \
			(0x1 << 20) | \
			(0x1 << 23) | \
			(0x1 << 30))
#define INFRACFG_AO_INFRA_BUS_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x10 << 5) | \
			(0x1 << 20) | \
			(0x1 << 23) | \
			(0x1 << 30))
#define INFRACFG_AO_INFRA_BUS_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 3) | \
			(0x1 << 4) | \
			(0x10 << 5) | \
			(0x0 << 20) | \
			(0x0 << 23) | \
			(0x0 << 30))

static void infracfg_ao_infra_dcm_rg_sfsel_set(unsigned int val)
{
	reg_write(INFRA_BUS_DCM_CTRL,
		(reg_read(INFRA_BUS_DCM_CTRL) &
		~(0x1f << 10)) |
		(val & 0x1f) << 10);
}

bool dcm_infracfg_ao_infra_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(INFRA_BUS_DCM_CTRL) &
		INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_INFRA_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_infra_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_infra_bus_dcm'" */
		infracfg_ao_infra_dcm_rg_sfsel_set(0x1);
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_infra_bus_dcm'" */
		infracfg_ao_infra_dcm_rg_sfsel_set(0x1);
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_INFRA_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_BUS_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_MASK ((0xf << 0))
#define INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_ON ((0x0 << 0))
#define INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_OFF ((0xf << 0))

bool dcm_infracfg_ao_infra_rx_p2p_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(P2P_RX_CLK_ON) &
		INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_infra_rx_p2p_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_infra_rx_p2p_dcm'" */
		reg_write(P2P_RX_CLK_ON,
			(reg_read(P2P_RX_CLK_ON) &
			~INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_infra_rx_p2p_dcm'" */
		reg_write(P2P_RX_CLK_ON,
			(reg_read(P2P_RX_CLK_ON) &
			~INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_MASK) |
			INFRACFG_AO_INFRA_RX_P2P_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_MTS_BUS_DCM_REG0_MASK ((0x1 << 31))
#define INFRACFG_AO_MTS_BUS_DCM_REG0_ON ((0x1 << 31))
#define INFRACFG_AO_MTS_BUS_DCM_REG0_OFF ((0x0 << 31))

bool dcm_infracfg_ao_mts_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(INFRA_BUS_DCM_CTRL) &
		INFRACFG_AO_MTS_BUS_DCM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_MTS_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_mts_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_mts_bus_dcm'" */
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_MTS_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_MTS_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_mts_bus_dcm'" */
		reg_write(INFRA_BUS_DCM_CTRL,
			(reg_read(INFRA_BUS_DCM_CTRL) &
			~INFRACFG_AO_MTS_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_MTS_BUS_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_PERI_BUS_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1f << 5) | \
			(0x1f << 15) | \
			(0x1 << 20) | \
			(0x1 << 21))
#define INFRACFG_AO_PERI_BUS_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x1f << 5) | \
			(0x1f << 15) | \
			(0x1 << 20) | \
			(0x1 << 21))
#define INFRACFG_AO_PERI_BUS_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 3) | \
			(0x1 << 4) | \
			(0x1f << 5) | \
			(0x0 << 15) | \
			(0x0 << 20) | \
			(0x0 << 21))

static void infracfg_ao_peri_dcm_rg_sfsel_set(unsigned int val)
{
	reg_write(PERI_BUS_DCM_CTRL,
		(reg_read(PERI_BUS_DCM_CTRL) &
		~(0x1f << 10)) |
		(val & 0x1f) << 10);
}

bool dcm_infracfg_ao_peri_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_PERI_BUS_DCM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_PERI_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_peri_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_peri_bus_dcm'" */
		infracfg_ao_peri_dcm_rg_sfsel_set(0x0);
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_PERI_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_peri_bus_dcm'" */
		infracfg_ao_peri_dcm_rg_sfsel_set(0x1f);
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_BUS_DCM_REG0_MASK) |
			INFRACFG_AO_PERI_BUS_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_PERI_MODULE_DCM_REG0_MASK (0x1 << 29)
#define INFRACFG_AO_PERI_MODULE_DCM_REG0_ON (0x1 << 29)
#define INFRACFG_AO_PERI_MODULE_DCM_REG0_OFF (0x0 << 29)

bool dcm_infracfg_ao_peri_module_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(PERI_BUS_DCM_CTRL) &
		INFRACFG_AO_PERI_MODULE_DCM_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_PERI_MODULE_DCM_REG0_ON);

	return ret;
}

void dcm_infracfg_ao_peri_module_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_peri_module_dcm'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_MODULE_DCM_REG0_MASK) |
			INFRACFG_AO_PERI_MODULE_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_peri_module_dcm'" */
		reg_write(PERI_BUS_DCM_CTRL,
			(reg_read(PERI_BUS_DCM_CTRL) &
			~INFRACFG_AO_PERI_MODULE_DCM_REG0_MASK) |
			INFRACFG_AO_PERI_MODULE_DCM_REG0_OFF);
	}
}

#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_MASK ((0xffffffff << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_MASK ((0xffffffff << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 4) | \
			(0x1 << 5) | \
			(0x1 << 6))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_ON ((0x40388000 << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_ON ((0xff << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 4) | \
			(0x0 << 5) | \
			(0x0 << 6))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_OFF ((0x40388000 << 0))
#define INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_OFF ((0x0 << 0))

bool dcm_infracfg_ao_mem_dcm_emi_group_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(INFRA_EMI_DCM_CFG0) &
		INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_MASK) ==
		(unsigned int) INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_ON);
	ret &= ((reg_read(INFRA_EMI_DCM_CFG1) &
		INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_MASK) ==
		(unsigned int) INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_ON);
	ret &= ((reg_read(INFRA_EMI_DCM_CFG3) &
		INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_MASK) ==
		(unsigned int) INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_ON);

	return ret;
}

void dcm_infracfg_ao_mem_dcm_emi_group(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infracfg_ao_mem_dcm_emi_group'" */
		reg_write(INFRA_EMI_DCM_CFG0,
			(reg_read(INFRA_EMI_DCM_CFG0) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_ON);
		reg_write(INFRA_EMI_DCM_CFG1,
			(reg_read(INFRA_EMI_DCM_CFG1) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_ON);
		reg_write(INFRA_EMI_DCM_CFG3,
			(reg_read(INFRA_EMI_DCM_CFG3) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infracfg_ao_mem_dcm_emi_group'" */
		reg_write(INFRA_EMI_DCM_CFG0,
			(reg_read(INFRA_EMI_DCM_CFG0) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG0_OFF);
		reg_write(INFRA_EMI_DCM_CFG1,
			(reg_read(INFRA_EMI_DCM_CFG1) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG1_OFF);
		reg_write(INFRA_EMI_DCM_CFG3,
			(reg_read(INFRA_EMI_DCM_CFG3) &
			~INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_MASK) |
			INFRACFG_AO_MEM_DCM_EMI_GROUP_REG2_OFF);
	}
}

#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 7) | \
			(0x1f << 16))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 7) | \
			(0x0 << 16))
#define INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_OFF ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x0 << 7) | \
			(0x0 << 16))

bool dcm_infra_ao_bcrm_infra_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
		INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK) ==
		(unsigned int) INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_infra_ao_bcrm_infra_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infra_ao_bcrm_infra_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
			~INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infra_ao_bcrm_infra_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0) &
			~INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_MASK) |
			INFRA_AO_BCRM_INFRA_BUS_DCM_REG0_OFF);
	}
}

#define INFRA_AO_BCRM_PERI_BUS_DCM_REG0_MASK ((0x1 << 6) | \
			(0x1f << 15))
#define INFRA_AO_BCRM_PERI_BUS_DCM_REG0_ON ((0x1 << 6) | \
			(0x0 << 15))
#define INFRA_AO_BCRM_PERI_BUS_DCM_REG0_OFF ((0x0 << 6) | \
			(0x0 << 15))

bool dcm_infra_ao_bcrm_peri_bus_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1) &
		INFRA_AO_BCRM_PERI_BUS_DCM_REG0_MASK) ==
		(unsigned int) INFRA_AO_BCRM_PERI_BUS_DCM_REG0_ON);

	return ret;
}

void dcm_infra_ao_bcrm_peri_bus_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'infra_ao_bcrm_peri_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1) &
			~INFRA_AO_BCRM_PERI_BUS_DCM_REG0_MASK) |
			INFRA_AO_BCRM_PERI_BUS_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'infra_ao_bcrm_peri_bus_dcm'" */
		reg_write(VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1,
			(reg_read(
			VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1) &
			~INFRA_AO_BCRM_PERI_BUS_DCM_REG0_MASK) |
			INFRA_AO_BCRM_PERI_BUS_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_ADB_DCM_REG0_MASK ((0x1 << 17) | \
			(0x1 << 17))
#define MP_CPUSYS_TOP_ADB_DCM_REG1_MASK ((0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 21) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 21))
#define MP_CPUSYS_TOP_ADB_DCM_REG2_MASK ((0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18))
#define MP_CPUSYS_TOP_ADB_DCM_REG0_ON ((0x1 << 17) | \
			(0x1 << 17))
#define MP_CPUSYS_TOP_ADB_DCM_REG1_ON ((0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 21) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 21))
#define MP_CPUSYS_TOP_ADB_DCM_REG2_ON ((0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18) | \
			(0x1 << 15) | \
			(0x1 << 16) | \
			(0x1 << 17) | \
			(0x1 << 18))
#define MP_CPUSYS_TOP_ADB_DCM_REG0_OFF ((0x0 << 17) | \
			(0x0 << 17))
#define MP_CPUSYS_TOP_ADB_DCM_REG1_OFF ((0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 21) | \
			(0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 21))
#define MP_CPUSYS_TOP_ADB_DCM_REG2_OFF ((0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18) | \
			(0x0 << 15) | \
			(0x0 << 16) | \
			(0x0 << 17) | \
			(0x0 << 18))

bool dcm_mp_cpusys_top_adb_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0) &
		MP_CPUSYS_TOP_ADB_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_ADB_DCM_REG0_ON);
	ret &= ((reg_read(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4) &
		MP_CPUSYS_TOP_ADB_DCM_REG1_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_ADB_DCM_REG1_ON);
	ret &= ((reg_read(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0) &
		MP_CPUSYS_TOP_ADB_DCM_REG2_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_ADB_DCM_REG2_ON);

	return ret;
}

void dcm_mp_cpusys_top_adb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_adb_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG0_ON);
		reg_write(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4,
			(reg_read(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4) &
			~MP_CPUSYS_TOP_ADB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG1_ON);
		reg_write(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG2_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG2_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_adb_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_ADB_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG0_OFF);
		reg_write(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4,
			(reg_read(MP_CPUSYS_TOP_MP_ADB_DCM_CFG4) &
			~MP_CPUSYS_TOP_ADB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG1_OFF);
		reg_write(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_ADB_DCM_REG2_MASK) |
			MP_CPUSYS_TOP_ADB_DCM_REG2_OFF);
	}
}

#define MP_CPUSYS_TOP_APB_DCM_REG0_MASK ((0x1 << 5) | \
			(0x1 << 5))
#define MP_CPUSYS_TOP_APB_DCM_REG1_MASK ((0x1 << 8) | \
			(0x1 << 8))
#define MP_CPUSYS_TOP_APB_DCM_REG2_MASK ((0x1 << 16) | \
			(0x1 << 16))
#define MP_CPUSYS_TOP_APB_DCM_REG0_ON ((0x1 << 5) | \
			(0x1 << 5))
#define MP_CPUSYS_TOP_APB_DCM_REG1_ON ((0x1 << 8) | \
			(0x1 << 8))
#define MP_CPUSYS_TOP_APB_DCM_REG2_ON ((0x1 << 16) | \
			(0x1 << 16))
#define MP_CPUSYS_TOP_APB_DCM_REG0_OFF ((0x0 << 5) | \
			(0x0 << 5))
#define MP_CPUSYS_TOP_APB_DCM_REG1_OFF ((0x0 << 8) | \
			(0x0 << 8))
#define MP_CPUSYS_TOP_APB_DCM_REG2_OFF ((0x0 << 16) | \
			(0x0 << 16))

bool dcm_mp_cpusys_top_apb_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
		MP_CPUSYS_TOP_APB_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_APB_DCM_REG0_ON);
	ret &= ((reg_read(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0) &
		MP_CPUSYS_TOP_APB_DCM_REG1_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_APB_DCM_REG1_ON);
	ret &= ((reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG0) &
		MP_CPUSYS_TOP_APB_DCM_REG2_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_APB_DCM_REG2_ON);

	return ret;
}

void dcm_mp_cpusys_top_apb_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_apb_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG0_ON);
		reg_write(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG1_ON);
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG2_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG2_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_apb_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG0_OFF);
		reg_write(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MCUSYS_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG1_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG1_OFF);
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG0) &
			~MP_CPUSYS_TOP_APB_DCM_REG2_MASK) |
			MP_CPUSYS_TOP_APB_DCM_REG2_OFF);
	}
}

#define MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_MASK ((0x1 << 24) | \
			(0x1 << 25) | \
			(0x1 << 11) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_ON ((0x1 << 24) | \
			(0x1 << 25) | \
			(0x1 << 11) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_OFF ((0x0 << 24) | \
			(0x0 << 25) | \
			(0x0 << 11) | \
			(0x0 << 24) | \
			(0x0 << 25))

bool dcm_mp_cpusys_top_bus_pll_div_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_CPUSYS_TOP_BUS_PLLDIV_CFG) &
		MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_bus_pll_div_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_bus_pll_div_dcm'" */
		reg_write(MP_CPUSYS_TOP_BUS_PLLDIV_CFG,
			(reg_read(MP_CPUSYS_TOP_BUS_PLLDIV_CFG) &
			~MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_bus_pll_div_dcm'" */
		reg_write(MP_CPUSYS_TOP_BUS_PLLDIV_CFG,
			(reg_read(MP_CPUSYS_TOP_BUS_PLLDIV_CFG) &
			~MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_BUS_PLL_DIV_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_MASK ((0x1 << 0) | \
			(0x1 << 0))
#define MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_ON ((0x1 << 0) | \
			(0x1 << 0))
#define MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 0))

bool dcm_mp_cpusys_top_core_stall_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG7) &
		MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_core_stall_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_core_stall_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG7,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG7) &
			~MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_core_stall_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG7,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG7) &
			~MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CORE_STALL_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CPUBIU_DCM_REG0_MASK ((0xffff << 0) | \
			(0xffff << 0))
#define MP_CPUSYS_TOP_CPUBIU_DCM_REG0_ON ((0xffff << 0) | \
			(0xffff << 0))
#define MP_CPUSYS_TOP_CPUBIU_DCM_REG0_OFF ((0x0 << 0) | \
			(0x0 << 0))

bool dcm_mp_cpusys_top_cpubiu_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_CPUSYS_TOP_MCSIC_DCM0) &
		MP_CPUSYS_TOP_CPUBIU_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_CPUBIU_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_cpubiu_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_cpubiu_dcm'" */
		reg_write(MP_CPUSYS_TOP_MCSIC_DCM0,
			(reg_read(MP_CPUSYS_TOP_MCSIC_DCM0) &
			~MP_CPUSYS_TOP_CPUBIU_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPUBIU_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_cpubiu_dcm'" */
		reg_write(MP_CPUSYS_TOP_MCSIC_DCM0,
			(reg_read(MP_CPUSYS_TOP_MCSIC_DCM0) &
			~MP_CPUSYS_TOP_CPUBIU_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPUBIU_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_MASK ((0x1 << 24) | \
			(0x1 << 25) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_ON ((0x1 << 24) | \
			(0x1 << 25) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_OFF ((0x0 << 24) | \
			(0x0 << 25) | \
			(0x0 << 24) | \
			(0x0 << 25))

bool dcm_mp_cpusys_top_cpu_pll_div_0_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_CPUSYS_TOP_CPU_PLLDIV_CFG0) &
		MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_cpu_pll_div_0_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_cpu_pll_div_0_dcm'" */
		reg_write(MP_CPUSYS_TOP_CPU_PLLDIV_CFG0,
			(reg_read(MP_CPUSYS_TOP_CPU_PLLDIV_CFG0) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_cpu_pll_div_0_dcm'" */
		reg_write(MP_CPUSYS_TOP_CPU_PLLDIV_CFG0,
			(reg_read(MP_CPUSYS_TOP_CPU_PLLDIV_CFG0) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_0_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_MASK ((0x1 << 24) | \
			(0x1 << 25) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_ON ((0x1 << 24) | \
			(0x1 << 25) | \
			(0x1 << 24) | \
			(0x1 << 25))
#define MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_OFF ((0x0 << 24) | \
			(0x0 << 25) | \
			(0x0 << 24) | \
			(0x0 << 25))

bool dcm_mp_cpusys_top_cpu_pll_div_1_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_CPUSYS_TOP_CPU_PLLDIV_CFG1) &
		MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_cpu_pll_div_1_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_cpu_pll_div_1_dcm'" */
		reg_write(MP_CPUSYS_TOP_CPU_PLLDIV_CFG1,
			(reg_read(MP_CPUSYS_TOP_CPU_PLLDIV_CFG1) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_cpu_pll_div_1_dcm'" */
		reg_write(MP_CPUSYS_TOP_CPU_PLLDIV_CFG1,
			(reg_read(MP_CPUSYS_TOP_CPU_PLLDIV_CFG1) &
			~MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_CPU_PLL_DIV_1_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_MASK ((0x1 << 4) | \
			(0x1 << 4))
#define MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_ON ((0x1 << 4) | \
			(0x1 << 4))
#define MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_OFF ((0x0 << 4) | \
			(0x0 << 4))

bool dcm_mp_cpusys_top_fcm_stall_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG7) &
		MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_fcm_stall_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_fcm_stall_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG7,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG7) &
			~MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_fcm_stall_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG7,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG7) &
			~MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_FCM_STALL_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_MASK ((0x1 << 31) | \
			(0x1 << 31))
#define MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_ON ((0x1 << 31) | \
			(0x1 << 31))
#define MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_OFF ((0x0 << 31) | \
			(0x0 << 31))

bool dcm_mp_cpusys_top_last_cor_idle_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_CPUSYS_TOP_BUS_PLLDIV_CFG) &
		MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_last_cor_idle_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_last_cor_idle_dcm'" */
		reg_write(MP_CPUSYS_TOP_BUS_PLLDIV_CFG,
			(reg_read(MP_CPUSYS_TOP_BUS_PLLDIV_CFG) &
			~MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_last_cor_idle_dcm'" */
		reg_write(MP_CPUSYS_TOP_BUS_PLLDIV_CFG,
			(reg_read(MP_CPUSYS_TOP_BUS_PLLDIV_CFG) &
			~MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_LAST_COR_IDLE_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_MISC_DCM_REG0_MASK ((0x1 << 1) | \
			(0x1 << 4) | \
			(0x1 << 1) | \
			(0x1 << 4))
#define MP_CPUSYS_TOP_MISC_DCM_REG0_ON ((0x1 << 1) | \
			(0x1 << 4) | \
			(0x1 << 1) | \
			(0x1 << 4))
#define MP_CPUSYS_TOP_MISC_DCM_REG0_OFF ((0x0 << 1) | \
			(0x0 << 4) | \
			(0x0 << 1) | \
			(0x0 << 4))

bool dcm_mp_cpusys_top_misc_dcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
		MP_CPUSYS_TOP_MISC_DCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_MISC_DCM_REG0_ON);

	return ret;
}

void dcm_mp_cpusys_top_misc_dcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_misc_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_MISC_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_MISC_DCM_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_misc_dcm'" */
		reg_write(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_MISC_DCM_REG0_MASK) |
			MP_CPUSYS_TOP_MISC_DCM_REG0_OFF);
	}
}

#define MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK ((0x1 << 3) | \
			(0x1 << 3))
#define MP_CPUSYS_TOP_MP0_QDCM_REG1_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3))
#define MP_CPUSYS_TOP_MP0_QDCM_REG0_ON ((0x1 << 3) | \
			(0x1 << 3))
#define MP_CPUSYS_TOP_MP0_QDCM_REG1_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3))
#define MP_CPUSYS_TOP_MP0_QDCM_REG0_OFF ((0x0 << 3) | \
			(0x0 << 3))
#define MP_CPUSYS_TOP_MP0_QDCM_REG1_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3))

bool dcm_mp_cpusys_top_mp0_qdcm_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
		MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_MP0_QDCM_REG0_ON);
	ret &= ((reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG0) &
		MP_CPUSYS_TOP_MP0_QDCM_REG1_MASK) ==
		(unsigned int) MP_CPUSYS_TOP_MP0_QDCM_REG1_ON);

	return ret;
}

void dcm_mp_cpusys_top_mp0_qdcm(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'mp_cpusys_top_mp0_qdcm'" */
		reg_write(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG0_ON);
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG1_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG1_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'mp_cpusys_top_mp0_qdcm'" */
		reg_write(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP_MISC_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG0_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG0_OFF);
		reg_write(MP_CPUSYS_TOP_MP0_DCM_CFG0,
			(reg_read(MP_CPUSYS_TOP_MP0_DCM_CFG0) &
			~MP_CPUSYS_TOP_MP0_QDCM_REG1_MASK) |
			MP_CPUSYS_TOP_MP0_QDCM_REG1_OFF);
	}
}

#define CPCCFG_REG_EMI_WFIFO_REG0_MASK ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3))
#define CPCCFG_REG_EMI_WFIFO_REG0_ON ((0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3) | \
			(0x1 << 0) | \
			(0x1 << 1) | \
			(0x1 << 2) | \
			(0x1 << 3))
#define CPCCFG_REG_EMI_WFIFO_REG0_OFF ((0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3) | \
			(0x0 << 0) | \
			(0x0 << 1) | \
			(0x0 << 2) | \
			(0x0 << 3))

bool dcm_cpccfg_reg_emi_wfifo_is_on(void)
{
	bool ret = true;

	ret &= ((reg_read(CPCCFG_REG_EMI_WFIFO) &
		CPCCFG_REG_EMI_WFIFO_REG0_MASK) ==
		(unsigned int) CPCCFG_REG_EMI_WFIFO_REG0_ON);

	return ret;
}

void dcm_cpccfg_reg_emi_wfifo(int on)
{
	if (on) {
		/* TINFO = "Turn ON DCM 'cpccfg_reg_emi_wfifo'" */
		reg_write(CPCCFG_REG_EMI_WFIFO,
			(reg_read(CPCCFG_REG_EMI_WFIFO) &
			~CPCCFG_REG_EMI_WFIFO_REG0_MASK) |
			CPCCFG_REG_EMI_WFIFO_REG0_ON);
	} else {
		/* TINFO = "Turn OFF DCM 'cpccfg_reg_emi_wfifo'" */
		reg_write(CPCCFG_REG_EMI_WFIFO,
			(reg_read(CPCCFG_REG_EMI_WFIFO) &
			~CPCCFG_REG_EMI_WFIFO_REG0_MASK) |
			CPCCFG_REG_EMI_WFIFO_REG0_OFF);
	}
}
