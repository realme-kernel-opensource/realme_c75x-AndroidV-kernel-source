// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Yu-Chang Wang <Yu-Chang.Wang@mediatek.com>
 */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "clk-fhctl.h"
#include "clk-fhctl-pll.h"
#include "clk-fhctl-util.h"

#define FHCTL_TARGET FHCTL_AP

#define PERCENT_TO_DDSLMT(dds, percent_m10) \
	((((dds) * (percent_m10)) >> 5) / 100)

struct match {
	char *name;
	struct fh_hdlr *hdlr;
	int (*init)(struct pll_dts *array
			, struct match *match);
};
struct hdlr_data_v1 {
	struct pll_dts *array;
	spinlock_t *lock;
	struct fh_pll_domain *domain;
};

static void dump_hw(struct fh_pll_regs *regs,
		struct fh_pll_data *data)
{
	FHDBG("hp_en<%x>,clk_con<%x>,slope0<%x>,slope1<%x>\n",
			readl(regs->reg_hp_en), readl(regs->reg_clk_con),
			readl(regs->reg_slope0), readl(regs->reg_slope1));
	FHDBG("cfg<%x>,lmt<%x>,dds<%x>,dvfs<%x>,mon<%x>\n",
			readl(regs->reg_cfg), readl(regs->reg_updnlmt),
			readl(regs->reg_dds), readl(regs->reg_dvfs),
			readl(regs->reg_mon));
	FHDBG("pcw<%x>\n",
			readl(regs->reg_con_pcw));
}

static int __ssc_v1(struct fh_pll_regs *regs,
		struct fh_pll_data *data,
		unsigned int fh_id, int rate)
{
	unsigned int updnlmt_val;

	if (rate > 0) {
		fh_set_field(regs->reg_cfg, data->frddsx_en, 0);
		fh_set_field(regs->reg_cfg, data->sfstrx_en, 0);
		fh_set_field(regs->reg_cfg, data->fhctlx_en, 0);

		/* Set the relative parameter registers (dt/df/upbnd/downbnd) */
		fh_set_field(regs->reg_cfg, data->msk_frddsx_dys,
				data->df_val);
		fh_set_field(regs->reg_cfg, data->msk_frddsx_dts,
				data->dt_val);

		writel((readl(regs->reg_con_pcw) & data->dds_mask) |
				data->tgl_org, regs->reg_dds);
		/* Calculate UPDNLMT */
		updnlmt_val = PERCENT_TO_DDSLMT((readl(regs->reg_dds) &
					data->dds_mask), rate) << data->updnlmt_shft;

		writel(updnlmt_val, regs->reg_updnlmt);

		/* Switch to FHCTL_CORE controller */
		fh_set_clr_field(regs->reg_hp_en, BIT(fh_id), 1);

		/* Enable SSC */
		fh_set_field(regs->reg_cfg, data->frddsx_en, 1);
		/* Enable Hopping control */
		fh_set_field(regs->reg_cfg, data->fhctlx_en, 1);

	} else {
		fh_set_field(regs->reg_cfg, data->frddsx_en, 0);
		fh_set_field(regs->reg_cfg, data->sfstrx_en, 0);
		fh_set_field(regs->reg_cfg, data->fhctlx_en, 0);

		/* Switch to APMIXEDSYS control */
		fh_set_clr_field(regs->reg_hp_en, BIT(fh_id), 0);

		/* Wait for DDS to be stable */
		udelay(30);
	}

	return 0;
}

static int __hopping_hw_flow_v1(void *priv_data, char *domain_name, unsigned int fh_id,
		unsigned int new_dds, int postdiv)
{
	struct fh_pll_domain *domain;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	unsigned int dds_mask;
	unsigned int mon_dds = 0;
	int ret = 0;
	unsigned int con_pcw_tmp;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	struct pll_dts *array = d->array;

	domain = d->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];
	dds_mask = data->dds_mask;

	if (array->ssc_rate)
		__ssc_v1(regs, data, fh_id, 0);

	/* 1. sync ncpo to DDS of FHCTL */
	writel((readl(regs->reg_con_pcw) & dds_mask) |
			data->tgl_org, regs->reg_dds);

	/* 2. enable DVFS and Hopping control */

	/* enable dvfs mode */
	fh_set_field(regs->reg_cfg, data->sfstrx_en, 1);
	/* enable hopping */
	fh_set_field(regs->reg_cfg, data->fhctlx_en, 1);

	/* for slope setting. */
	writel(data->slope0_value, regs->reg_slope0);
	/* SLOPE1 is for MEMPLL */
	writel(data->slope1_value, regs->reg_slope1);

	/* 3. switch to hopping control */
	fh_set_clr_field(regs->reg_hp_en, BIT(fh_id), 1);

	/* 4. set DFS DDS */
	writel((new_dds) | (data->dvfs_tri), regs->reg_dvfs);

	/* 4.1 ensure jump to target DDS */
	/* Wait 1000 us until DDS stable */
	ret = readl_poll_timeout_atomic(regs->reg_mon, mon_dds,
			(mon_dds & dds_mask) == new_dds, 10, 1000);
	if (ret) {
		/* dump HW */
		dump_hw(regs, data);

		/* notify user that err */
		mb();
		notify_err();
	}

	/* Don't change DIV for fhctl UT */
	con_pcw_tmp = readl(regs->reg_con_pcw) & (~dds_mask);
	con_pcw_tmp = (con_pcw_tmp |
			(readl(regs->reg_mon) & dds_mask) |
			data->pcwchg);

	/* 5. write back to ncpo */
	writel(con_pcw_tmp, regs->reg_con_pcw);

	/* 6. switch to APMIXEDSYS control */
	fh_set_clr_field(regs->reg_hp_en, BIT(fh_id), 0);

	if (array->ssc_rate)
		__ssc_v1(regs, data, fh_id, array->ssc_rate);

	return ret;
}

static unsigned int __get_postdiv(struct fh_pll_regs *regs,
	struct fh_pll_data *data)
{
	// If design change extend this table, please extend it. (But this should not happens)
	// For i <= 4, postdiv_pow[i] = 2^i. For i > 4, postdiv_pow[i] = 1.
	// Using array for idx->postdiv_pow mapping has better performance than pow(2, i)+if_cond.
	static unsigned int postdiv_pow[8] = {1,2,4,8,16,1,1,1};
	unsigned int regval;

	regval = (readl(regs->reg_con_postdiv) & data->postdiv_mask)
		>> data->postdiv_offset;

	FHDBG("Get current postdiv : %x\n", regval);

	return postdiv_pow[regval];
}

static void __set_postdiv(struct fh_pll_regs *regs,
	struct fh_pll_data *data, int postdiv)
{
	unsigned int temp;

	temp = (readl(regs->reg_con_postdiv)) & ~(data->postdiv_mask);
	temp |= (ffs(postdiv) - 1) << data->postdiv_offset;

	FHDBG("Set target postdiv : %x\n", temp);

	writel(temp, regs->reg_con_postdiv);
}

static int ap_hopping_v1(void *priv_data, char *domain_name, unsigned int fh_id,
		unsigned int new_dds, int postdiv)
{
	struct fh_pll_domain *domain;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	int ret = 0;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	spinlock_t *lock = d->lock;
	unsigned long flags = 0;
	unsigned int cur_postdiv;

	FHDBG("fh_id:%d, new_dds:0x%x, postdiv:%d\n", fh_id, new_dds, postdiv);

	//data preparation
	domain = d->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];

	//pre-hopping check
	//	If cur postdiv < target postdiv, change it before DDS
	if (postdiv != -1) {
		cur_postdiv = __get_postdiv(regs, data);
		if (postdiv > cur_postdiv) {
			FHDBG("set pos b4 fhctl: current_postdiv %x, postdiv(target) %x\n",
				cur_postdiv, postdiv);
			__set_postdiv(regs, data, postdiv);
		}
	}

	//hopping
	spin_lock_irqsave(lock, flags);

	ret = __hopping_hw_flow_v1(priv_data, domain_name, fh_id,
		new_dds, postdiv);

	spin_unlock_irqrestore(lock, flags);


	//post-hopping check
	//	If cur postdiv < target postdiv, change it after DDS
	if (postdiv != -1) {
		if (postdiv < cur_postdiv) {
			FHDBG("set pos af fhctl: current_postdiv %x, postdiv(target) %x\n",
				cur_postdiv, postdiv);
			__set_postdiv(regs, data, postdiv);
		}
	}

	return ret;
}

static int ap_ssc_enable_v1(void *priv_data,
		char *domain_name, unsigned int fh_id, int rate)
{
	struct fh_pll_domain *domain;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	spinlock_t *lock = d->lock;
	struct pll_dts *array = d->array;
	unsigned long flags = 0;

	spin_lock_irqsave(lock, flags);

	FHDBG("id<%d>, rate<%d>\n", fh_id, rate);

	domain = d->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];

	__ssc_v1(regs, data, fh_id, rate);

	array->ssc_rate = rate;

	spin_unlock_irqrestore(lock, flags);

	return 0;
}

static int ap_ssc_disable_v1(void *priv_data,
		char *domain_name, unsigned int fh_id)
{
	struct fh_pll_domain *domain;
	struct fh_pll_regs *regs;
	struct fh_pll_data *data;
	struct hdlr_data_v1 *d = (struct hdlr_data_v1 *)priv_data;
	spinlock_t *lock = d->lock;
	struct pll_dts *array = d->array;
	unsigned long flags = 0;

	spin_lock_irqsave(lock, flags);

	FHDBG("id<%d>\n", fh_id);

	domain = d->domain;
	regs = &domain->regs[fh_id];
	data = &domain->data[fh_id];

	__ssc_v1(regs, data, fh_id, 0);

	array->ssc_rate = 0;

	spin_unlock_irqrestore(lock, flags);

	return 0;
}

static int ap_init_v1(struct pll_dts *array, struct match *match)
{
	static DEFINE_SPINLOCK(lock);
	struct hdlr_data_v1 *priv_data;
	struct fh_hdlr *hdlr;
	struct fh_pll_domain *domain;
	unsigned int fh_id = array->fh_id;
	struct fh_pll_regs *regs;

	FHDBG("array<%lx>,%s %s, id<%d>\n",
			(unsigned long)array,
			array->pll_name,
			array->domain,
			fh_id);

	priv_data = kzalloc(sizeof(*priv_data), GFP_KERNEL);
	hdlr = kzalloc(sizeof(*hdlr), GFP_KERNEL);
	init_fh_domain(array->domain,
			array->comp,
			array->fhctl_base,
			array->apmixed_base);

	priv_data->array = array;
	priv_data->lock = &lock;
	priv_data->domain = get_fh_domain(array->domain);

	/* do HW init */
	domain = priv_data->domain;
	regs = &domain->regs[fh_id];

	fh_set_clr_field(regs->reg_clk_con, BIT(fh_id), 1);
	fh_set_clr_field(regs->reg_rst_con, BIT(fh_id), 0);
	fh_set_clr_field(regs->reg_rst_con, BIT(fh_id), 1);

	writel(0x0, regs->reg_cfg);
	writel(0x0, regs->reg_updnlmt);
	writel(0x0, regs->reg_dds);

	/* hook to array */
	hdlr->data = priv_data;
	hdlr->ops = match->hdlr->ops;
	/* hook hdlr to array is the last step */
	mb();
	array->hdlr = hdlr;

	/* do SSC */
	if (array->ssc_rate) {
		struct fh_hdlr *hdlr;

		hdlr = array->hdlr;
		hdlr->ops->ssc_enable(hdlr->data,
				array->domain,
				array->fh_id,
				array->ssc_rate);
	}

	return 0;
}

static struct fh_operation ap_ops_v1 = {
	.hopping = ap_hopping_v1,
	.ssc_enable = ap_ssc_enable_v1,
	.ssc_disable = ap_ssc_disable_v1,
};
static struct fh_hdlr ap_hdlr_v1 = {
	.ops = &ap_ops_v1,
};
static struct match mt6765_match = {
	.name = "mediatek,mt6765-fhctl",
	.hdlr = &ap_hdlr_v1,
	.init = &ap_init_v1,
};
static struct match mt6761_match = {
	.name = "mediatek,mt6761-fhctl",
	.hdlr = &ap_hdlr_v1,
	.init = &ap_init_v1,
};
static struct match mt6768_match = {
	.name = "mediatek,mt6768-fhctl",
	.hdlr = &ap_hdlr_v1,
	.init = &ap_init_v1,
};
static struct match mt6781_match = {
	.name = "mediatek,mt6781-fhctl",
	.hdlr = &ap_hdlr_v1,
	.init = &ap_init_v1,
};
static struct match mt6853_match = {
	.name = "mediatek,mt6853-fhctl",
	.hdlr = &ap_hdlr_v1,
	.init = &ap_init_v1,
};
static struct match mt6877_match = {
	.name = "mediatek,mt6877-fhctl",
	.hdlr = &ap_hdlr_v1,
	.init = &ap_init_v1,
};
static struct match mt6885_match = {
	.name = "mediatek,mt6885-fhctl",
	.hdlr = &ap_hdlr_v1,
	.init = &ap_init_v1,
};
static struct match mt6897_match = {
	.name = "mediatek,mt6897-fhctl",
	.hdlr = &ap_hdlr_v1,
	.init = &ap_init_v1,
};
static struct match mt6985_match = {
	.name = "mediatek,mt6985-fhctl",
	.hdlr = &ap_hdlr_v1,
	.init = &ap_init_v1,
};
static struct match mt6989_match = {
	.name = "mediatek,mt6989-fhctl",
	.hdlr = &ap_hdlr_v1,
	.init = &ap_init_v1,
};
static struct match mt6991_match = {
	.name = "mediatek,mt6991-fhctl",
	.hdlr = &ap_hdlr_v1,
	.init = &ap_init_v1,
};
static struct match *matches[] = {
	&mt6765_match,
	&mt6761_match,
	&mt6768_match,
	&mt6781_match,
	&mt6877_match,
	&mt6853_match,
	&mt6885_match,
	&mt6897_match,
	&mt6985_match,
	&mt6989_match,
	&mt6991_match,
	NULL,
};

int fhctl_ap_init(struct pll_dts *array)
{
	int i;
	int num_pll;
	struct match **match;

	FHDBG("\n");
	match = matches;
	num_pll = array->num_pll;

	/* find match by compatible */
	while (*match != NULL) {
		char *comp = (*match)->name;
		char *target = array->comp;

		if (strcmp(comp, target) == 0)
			break;

		match++;
	}

	if (*match == NULL) {
		FHDBG("no match!\n");
		return -1;
	}

	/* init flow for every pll */
	for (i = 0; i < num_pll ; i++, array++) {
		char *method = array->method;

		if (strcmp(method, FHCTL_TARGET) == 0)
			(*match)->init(array, *match);
	}

	FHDBG("\n");
	return 0;
}
