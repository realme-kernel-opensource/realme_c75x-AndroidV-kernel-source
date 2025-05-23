// SPDX-License-Identifier: GPL-2.0
/*
 * mtu3_dr.c - dual role switch and host glue layer
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>

#include "mtu3.h"
#include "mtu3_dr.h"

/* mt8678 port2/3 */
#define PERI_WK_CTRL4	0x10
#define WCP2_IS_EN	BIT(0) /* port2/3 en bit */

/* mt6991/mt8678 port1 */
#define WCP1_IS_EN	BIT(7) /* port1 en bit */

/* mt6886 */
#define PERI_WK_CTRL3	0x14
#define UWK_V1_4_CTRL0_MASK	0x1

/* mt6983 etc */
#define PERI_WK_CTRL2	0x8
#define UWK_V1_3_CTRL0_MASK	0x3
#define UWK_V1_3_CTRL2_MASK	0x699

/* mt6878 etc */
/* bit0: IP_SLEEP */
/* bit2: LINESTATE */
#define UWK_V1_5_CTRL2_MASK	0x5

/* mt8173 etc */
#define PERI_WK_CTRL1	0x4
#define WC1_IS_C(x)	(((x) & 0xf) << 26)  /* cycle debounce */
#define WC1_IS_EN	BIT(25)
#define WC1_IS_P	BIT(6)  /* polarity for ip sleep */

/* mt8183 */
#define PERI_WK_CTRL0	0x0
#define WC0_IS_C(x)	((u32)(((x) & 0xf) << 28))  /* cycle debounce */
#define WC0_IS_P	BIT(12)	/* polarity */
#define WC0_IS_EN	BIT(6)

/* mt8192 */
#define WC0_SSUSB0_CDEN		BIT(6)
#define WC0_IS_SPM_EN		BIT(1)

/* mt2712 etc */
#define PERI_SSUSB_SPM_CTRL	0x0
#define SSC_IP_SLEEP_EN	BIT(4)
#define SSC_SPM_INT_EN		BIT(1)

enum ssusb_uwk_vers {
	SSUSB_UWK_V1 = 1,
	SSUSB_UWK_V2,
	SSUSB_UWK_V1_1 = 101,	/* specific revision 1.01 */
	SSUSB_UWK_V1_2,		/* specific revision 1.02 */
	SSUSB_UWK_V1_3,		/* specific revision 1.03 */
	SSUSB_UWK_V1_4,		/* specific revision 1.04 */
	SSUSB_UWK_V1_5,		/* specific revision 1.05 */
	SSUSB_UWK_V1_6,		/* specific revision 1.06 */
	SSUSB_UWK_V1_7,		/* specific revision 1.07 */
};

#define USB2_PORT_SC(x)		(0x420 + 0x10 * (x))
#define DEV_SPEED_MASK		(0xf << 10)
#define DEV_LOWSPEED(p)		(((p) & DEV_SPEED_MASK) == (0x2 << 10))

/*
 * ip-sleep wakeup mode:
 * all clocks can be turn off, but power domain should be kept on
 */
static void ssusb_wakeup_ip_sleep_set(struct ssusb_mtk *ssusb, bool enable)
{
	u32 reg, msk, val;

	switch (ssusb->uwk_vers) {
	case SSUSB_UWK_V1:
		reg = ssusb->uwk_reg_base + PERI_WK_CTRL1;
		msk = WC1_IS_EN | WC1_IS_C(0xf) | WC1_IS_P;
		val = enable ? (WC1_IS_EN | WC1_IS_C(0x8)) : 0;
		break;
	case SSUSB_UWK_V1_1:
		reg = ssusb->uwk_reg_base + PERI_WK_CTRL0;
		msk = WC0_IS_EN | WC0_IS_C(0xf) | WC0_IS_P;
		val = enable ? (WC0_IS_EN | WC0_IS_C(0x1)) : 0;
		break;
	case SSUSB_UWK_V1_2:
		reg = ssusb->uwk_reg_base + PERI_WK_CTRL0;
		msk = WC0_SSUSB0_CDEN | WC0_IS_SPM_EN;
		val = enable ? msk : 0;
		break;
	case SSUSB_UWK_V1_3:
		reg = ssusb->uwk_reg_base + PERI_WK_CTRL0;
		msk = UWK_V1_3_CTRL0_MASK;
		val = enable ? msk : 0;
		regmap_update_bits(ssusb->uwk, reg, msk, val);

		reg = ssusb->uwk_reg_base + PERI_WK_CTRL2;
		msk = UWK_V1_3_CTRL2_MASK;
		val = enable ? msk : 0;
		break;
	case SSUSB_UWK_V1_4:
		reg = ssusb->uwk_reg_base + PERI_WK_CTRL3;
		msk = UWK_V1_4_CTRL0_MASK;
		val = enable ? msk : 0;
		break;
	case SSUSB_UWK_V1_5:
		reg = ssusb->uwk_reg_base + PERI_WK_CTRL2;
		msk = UWK_V1_5_CTRL2_MASK;
		val = enable ? msk : 0;
		break;
	case SSUSB_UWK_V1_6:
		reg = ssusb->uwk_reg_base + PERI_WK_CTRL2;
		msk = WCP1_IS_EN;
		val = enable ? msk : 0;
		break;
	case SSUSB_UWK_V1_7:
		reg = ssusb->uwk_reg_base + PERI_WK_CTRL4;
		msk = WCP2_IS_EN;
		val = enable ? msk : 0;
		break;
	case SSUSB_UWK_V2:
		reg = ssusb->uwk_reg_base + PERI_SSUSB_SPM_CTRL;
		msk = SSC_IP_SLEEP_EN | SSC_SPM_INT_EN;
		val = enable ? msk : 0;
		break;
	default:
		return;
	}
	regmap_update_bits(ssusb->uwk, reg, msk, val);
}

int ssusb_wakeup_of_property_parse(struct ssusb_mtk *ssusb,
				struct device_node *dn)
{
	struct of_phandle_args args;
	int ret;

	/* wakeup function is optional */
	ssusb->uwk_en = of_property_read_bool(dn, "wakeup-source");
	if (!ssusb->uwk_en)
		return 0;

	ret = of_parse_phandle_with_fixed_args(dn,
				"mediatek,syscon-wakeup", 2, 0, &args);
	if (ret)
		return ret;

	ssusb->uwk_reg_base = args.args[0];
	ssusb->uwk_vers = args.args[1];
	ssusb->uwk = syscon_node_to_regmap(args.np);
	of_node_put(args.np);
	dev_info(ssusb->dev, "uwk - reg:0x%x, version:%d\n",
			ssusb->uwk_reg_base, ssusb->uwk_vers);

	return PTR_ERR_OR_ZERO(ssusb->uwk);
}

void ssusb_set_host_low_speed_bypass(struct ssusb_mtk *ssusb)
{
	u32 value;
	int num_u2p = ssusb->u2_ports;
	int num_u3p = ssusb->u3_ports;
	int i;

	if (!ssusb->ls_slp_quirk || IS_ERR_OR_NULL(ssusb->host_base))
		return;

	for (i = 0; i < num_u2p; i++) {
		if ((0x1 << i) & ssusb->u2p_dis_msk)
			continue;

		value = readl(ssusb->host_base + USB2_PORT_SC(num_u3p + i));

		if (DEV_LOWSPEED(value))
			ssusb->ls_slp_bypass |= (0x1 << i);
		else
			ssusb->ls_slp_bypass &= ~(0x1 << i);
	}

}

void ssusb_clear_host_low_speed_bypass(struct ssusb_mtk *ssusb)
{
	ssusb->ls_slp_bypass = 0;
}

void ssusb_wakeup_set(struct ssusb_mtk *ssusb, bool enable)
{
	if (ssusb->uwk_en)
		ssusb_wakeup_ip_sleep_set(ssusb, enable);
}

static void host_ports_num_get(struct ssusb_mtk *ssusb)
{
	u32 xhci_cap;

	xhci_cap = mtu3_readl(ssusb->ippc_base, U3D_SSUSB_IP_XHCI_CAP);
	ssusb->u2_ports = SSUSB_IP_XHCI_U2_PORT_NUM(xhci_cap);
	ssusb->u3_ports = SSUSB_IP_XHCI_U3_PORT_NUM(xhci_cap);

	dev_dbg(ssusb->dev, "host - u2_ports:%d, u3_ports:%d\n",
		 ssusb->u2_ports, ssusb->u3_ports);
}

/* only configure ports will be used later */
int ssusb_host_enable(struct ssusb_mtk *ssusb)
{
	void __iomem *ibase = ssusb->ippc_base;
	int num_u3p = ssusb->u3_ports;
	int num_u2p = ssusb->u2_ports;
	int u3_ports_disabled;
	u32 check_clk;
	u32 value;
	int i;
	int ret;

	/* if dp 4-lane, set u3_ports = 0 */
	if (get_dp_switch_status(ssusb))
		num_u3p = 0;

	/* power on host ip */
	mtu3_clrbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	/* power on and enable u3 ports except skipped ones */
	u3_ports_disabled = 0;
	for (i = 0; i < num_u3p; i++) {
		if ((0x1 << i) & ssusb->u3p_dis_msk) {
			u3_ports_disabled++;
			continue;
		}

		value = mtu3_readl(ibase, SSUSB_U3_CTRL(i));
		value &= ~(SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS);
		value |= SSUSB_U3_PORT_HOST_SEL;
		mtu3_writel(ibase, SSUSB_U3_CTRL(i), value);
	}

	/* power on and enable all u2 ports */
	for (i = 0; i < num_u2p; i++) {
		if ((0x1 << i) & ssusb->u2p_dis_msk)
			continue;

		value = mtu3_readl(ibase, SSUSB_U2_CTRL(i));
		value &= ~(SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS);
		value |= SSUSB_U2_PORT_HOST_SEL;
		mtu3_writel(ibase, SSUSB_U2_CTRL(i), value);
	}

	check_clk = SSUSB_XHCI_RST_B_STS;
	if (num_u3p > u3_ports_disabled)
		check_clk = SSUSB_U3_MAC_RST_B_STS;

	ret =  ssusb_check_clocks(ssusb, check_clk);

	/* update txdeemph */
	ssusb_set_txdeemph(ssusb);

	if (ssusb->eusb2_cm_l1) {
		value = mtu3_readl(ssusb->mac_base, U3D_USB20_LPM_TIMING_PARAM);
		value &= ~(LPM_L1_RESIDENCY_MSK);
		value |= LPM_L1_RESIDENCY(ssusb->eusb2_cm_l1);
		mtu3_writel(ssusb->mac_base, U3D_USB20_LPM_TIMING_PARAM, value);
		dev_info(ssusb->dev, "U3D_USB20_LPM_TIMING_PARAM - value:0x%x\n", value);
	}

	if (ssusb->utmi_width == 16) {
		mtu3_setbits(ibase, U3D_SSUSB_SYS_CK_CTRL, SSUSB_U2_UTMI_DATABUS_16_8);
		if (num_u2p == 2)
			mtu3_setbits(ibase, U3D_SSUSB_SYS_CK_CTRL,
					SSUSB_U2_UTMI_DATABUS_16_8_1P);
		value = mtu3_readl(ibase, U3D_SSUSB_SYS_CK_CTRL);
		dev_info(ssusb->dev, "U3D_SSUSB_SYS_CK_CTRL - value:0x%x\n", value);
	}

	return ret;
}

int ssusb_host_disable(struct ssusb_mtk *ssusb)
{
	void __iomem *ibase = ssusb->ippc_base;
	int num_u3p = ssusb->u3_ports;
	int num_u2p = ssusb->u2_ports;
	u32 value;
	int i;

	/* power down and disable u3 ports except skipped ones */
	for (i = 0; i < num_u3p; i++) {
		if ((0x1 << i) & ssusb->u3p_dis_msk)
			continue;

		value = mtu3_readl(ibase, SSUSB_U3_CTRL(i));
		value |= SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U3_CTRL(i), value);
	}

	/* power down and disable u2 ports except skipped ones */
	for (i = 0; i < num_u2p; i++) {
		if ((0x1 << i) & ssusb->u2p_dis_msk)
			continue;

		value = mtu3_readl(ibase, SSUSB_U2_CTRL(i));
		value |= SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U2_CTRL(i), value);
	}

	/* power down host ip */
	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	return 0;
}

int ssusb_host_resume(struct ssusb_mtk *ssusb, bool p0_skipped)
{
	void __iomem *ibase = ssusb->ippc_base;
	int u3p_skip_msk = ssusb->u3p_dis_msk;
	int u2p_skip_msk = ssusb->u2p_dis_msk;
	int num_u3p = ssusb->u3_ports;
	int num_u2p = ssusb->u2_ports;
	u32 value;
	int i;

	/* if dp 4-lane, set u3_ports = 0 */
	if (get_dp_switch_status(ssusb))
		num_u3p = 0;

	if (p0_skipped) {
		u2p_skip_msk |= 0x1;
		if (ssusb->otg_switch.is_u3_drd)
			u3p_skip_msk |= 0x1;
	}

	/* power on host ip */
	mtu3_clrbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	/* power on u3 ports except skipped ones */
	for (i = 0; i < num_u3p; i++) {
		if ((0x1 << i) & u3p_skip_msk)
			continue;

		value = mtu3_readl(ibase, SSUSB_U3_CTRL(i));
		/* resume u3phy since power issue. */
		value &= ~(SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS);
		mtu3_writel(ibase, SSUSB_U3_CTRL(i), value);
	}

	/* power on all u2 ports except skipped ones */
	for (i = 0; i < num_u2p; i++) {
		if ((0x1 << i) & u2p_skip_msk)
			continue;

		value = mtu3_readl(ibase, SSUSB_U2_CTRL(i));
		value &= ~SSUSB_U2_PORT_PDN;
		mtu3_writel(ibase, SSUSB_U2_CTRL(i), value);
	}

	return 0;
}

/* here not skip port0 due to PDN can be set repeatedly */
int ssusb_host_suspend(struct ssusb_mtk *ssusb)
{
	void __iomem *ibase = ssusb->ippc_base;
	int num_u3p = ssusb->u3_ports;
	int num_u2p = ssusb->u2_ports;
	u32 value;
	int i;

	/* power down u3 ports except skipped ones */
	for (i = 0; i < num_u3p; i++) {
		if ((0x1 << i) & ssusb->u3p_dis_msk)
			continue;

		value = mtu3_readl(ibase, SSUSB_U3_CTRL(i));
		/* disable u3phy since power issue. */
		value |= SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U3_CTRL(i), value);
	}

	/* power down u2 ports except skipped ones */
	for (i = 0; i < num_u2p; i++) {
		if ((0x1 << i) & ssusb->u2p_dis_msk)
			continue;

		value = mtu3_readl(ibase, SSUSB_U2_CTRL(i));
		value |= SSUSB_U2_PORT_PDN;
		mtu3_writel(ibase, SSUSB_U2_CTRL(i), value);
	}

	/* power down host ip */
	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	return 0;
}

int ssusb_host_u3_resume(struct ssusb_mtk *ssusb)
{
	void __iomem *ibase = ssusb->ippc_base;
	int num_u3p = ssusb->u3_ports;
	u32 value;
	int i;

	/* power on u3 ports */
	for (i = 0; i < num_u3p; i++) {
		value = mtu3_readl(ibase, SSUSB_U3_CTRL(i));
		/* resume u3phy since power issue. */
		value &= ~(SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS);
		mtu3_writel(ibase, SSUSB_U3_CTRL(i), value);
	}

	dev_info(ssusb->dev, "power on u3 ports\n");
	return 0;
}

int ssusb_host_u3_suspend(struct ssusb_mtk *ssusb)
{
	void __iomem *ibase = ssusb->ippc_base;
	int num_u3p = ssusb->u3_ports;
	u32 value;
	int i;

	/* power down u3 ports */
	for (i = 0; i < num_u3p; i++) {
		value = mtu3_readl(ibase, SSUSB_U3_CTRL(i));
		/* disable u3phy since power issue. */
		value |= SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U3_CTRL(i), value);
	}

	dev_info(ssusb->dev, "power down u3 ports\n");
	return 0;
}

static void ssusb_host_setup(struct ssusb_mtk *ssusb)
{
	host_ports_num_get(ssusb);

	/*
	 * power on host and power on/enable all ports
	 * if support OTG, gadget driver will switch port0 to device mode
	 */
	ssusb_host_enable(ssusb);
	ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_HOST);

	/* if port0 supports dual-role, works as host mode by default */
	ssusb_set_force_vbus(ssusb, false);
	ssusb_set_vbus(&ssusb->otg_switch, 1);
}

static void ssusb_host_cleanup(struct ssusb_mtk *ssusb)
{
	if (ssusb->is_host)
		ssusb_set_vbus(&ssusb->otg_switch, 0);

	ssusb_host_disable(ssusb);
}

static void ssusb_get_platform_driver(struct ssusb_mtk *ssusb)
{
	struct device_node *parent_dn = ssusb->dev->of_node;
	struct device_node *child;
	struct platform_device *pdev;
	struct resource *res;

	for_each_child_of_node(parent_dn, child) {
		if (of_device_is_compatible(child, "mediatek,mtk-xhci") ||
		    of_device_is_compatible(child, "mediatek,mtk-xhci-p1") ||
		    of_device_is_compatible(child, "mediatek,mtk-xhci-p2")) {
			pdev = of_find_device_by_node(child);
			if (pdev) {
				ssusb->xhci_pdrv =
					to_platform_driver(pdev->dev.driver);
				break;
			}
		}
	}

	if (pdev) {
		if (!ssusb->ls_slp_quirk)
			return;

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mac");
		if (res)
			ssusb->host_base = devm_ioremap(ssusb->dev, res->start,
			    resource_size(res));

		if (IS_ERR_OR_NULL(ssusb->host_base))
			dev_info(ssusb->dev, "failed to get host_base\n");
	}
}

/*
 * If host supports multiple ports, the VBUSes(5V) of ports except port0
 * which supports OTG are better to be enabled by default in DTS.
 * Because the host driver will keep link with devices attached when system
 * enters suspend mode, so no need to control VBUSes after initialization.
 */
int ssusb_host_init(struct ssusb_mtk *ssusb, struct device_node *parent_dn)
{
	struct device *parent_dev = ssusb->dev;
	int ret;

	ssusb_host_setup(ssusb);

	ret = of_platform_populate(parent_dn, NULL, NULL, parent_dev);
	if (ret) {
		dev_dbg(parent_dev, "failed to create child devices at %pOF\n",
				parent_dn);
		return ret;
	}

	dev_info(parent_dev, "xHCI platform device register success...\n");

	ssusb_set_noise_still_tr(ssusb);

	ssusb_get_platform_driver(ssusb);

	return 0;
}

void ssusb_host_exit(struct ssusb_mtk *ssusb)
{
	of_platform_depopulate(ssusb->dev);
	ssusb_host_cleanup(ssusb);
}
