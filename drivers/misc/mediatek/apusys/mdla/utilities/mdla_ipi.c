// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/rpmsg.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_ipi.h>
#include <platform/mdla_plat_api.h>

#include "../../apusys_rv/apu.h"
#include "../../apusys_ce/apu_ce_excep.h"

/*
 * type0: Distinguish pwr_time, timeout, klog, or CX
 * type1: prepare to C1~C16
 * dir  : Distinguish read or write
 * data : store data
 */

struct mdla_ipi_data {
	u32 type0;
	u16 type1;
	u16 dir;
	u64 data;
};
static struct mdla_ipi_data ipi_tx_recv_buf;
static struct mdla_ipi_data ipi_rx_send_buf;
static struct mutex mdla_ipi_mtx;

struct mdla_rpmsg_device {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;
	struct completion ack;
};

static struct mdla_rpmsg_device mdla_tx_rpm_dev;
static struct mdla_rpmsg_device mdla_rx_rpm_dev;

int mdla_ipi_send(int type_0, int type_1, u64 val)
{
	struct mdla_ipi_data ipi_cmd_send;
	int ret = 0;

	if (!mdla_tx_rpm_dev.ept)
		return 0;


	ipi_cmd_send.type0  = type_0;
	ipi_cmd_send.type1  = type_1;
	ipi_cmd_send.dir    = MDLA_IPI_WRITE;
	ipi_cmd_send.data   = val;
	mdla_verbose("send : %d %d, %d, %llu(0x%llx)\n",
				ipi_cmd_send.type0,
				ipi_cmd_send.type1,
				ipi_cmd_send.dir,
				ipi_cmd_send.data, ipi_cmd_send.data);

	mutex_lock(&mdla_ipi_mtx);

	/* power on */
	ret = rpmsg_sendto(mdla_tx_rpm_dev.ept, NULL, 1, 0);
	if (ret && ret != -EOPNOTSUPP) {
		pr_info("%s: rpmsg_sendto(power on) fail(%d)\n", __func__, ret);
		goto out;
	}
	ret = rpmsg_send(mdla_tx_rpm_dev.ept, &ipi_cmd_send, sizeof(ipi_cmd_send));
	if (ret) {
		mdla_err("%s: rpmsg_send fail(%d)\n", __func__, ret);
		/* power off to restore ref cnt */
		ret = rpmsg_sendto(mdla_tx_rpm_dev.ept, NULL, 0, 1);
		if (ret && ret != -EOPNOTSUPP)
			mdla_err("%s: rpmsg_sendto(power off) fail(%d)\n", __func__, ret);
		goto out;
	}
out:
	mutex_unlock(&mdla_ipi_mtx);
	return 0;
}

int mdla_ipi_recv(int type_0, int type_1, u64 *val)
{
	struct mdla_ipi_data ipi_cmd_send;
	int ret = 0;

	ipi_cmd_send.type0  = type_0;
	ipi_cmd_send.type1  = type_1;
	ipi_cmd_send.dir    = MDLA_IPI_READ;
	ipi_cmd_send.data   = 0;

	mutex_lock(&mdla_ipi_mtx);

	/* power on */
	ret = rpmsg_sendto(mdla_tx_rpm_dev.ept, NULL, 1, 0);
	if (ret && ret != -EOPNOTSUPP) {
		mdla_err("%s: rpmsg_sendto(power on) fail(%d)\n", __func__, ret);
		goto out;
	}

	ret = rpmsg_send(mdla_tx_rpm_dev.ept, &ipi_cmd_send, sizeof(ipi_cmd_send));

	if (ret) {
		mdla_err("%s: rpmsg_send fail(%d)\n", __func__, ret);
		/* power off to restore ref cnt */
		ret = rpmsg_sendto(mdla_tx_rpm_dev.ept, NULL, 0, 1);
		if (ret && ret != -EOPNOTSUPP)
			mdla_err("%s: rpmsg_sendto(power off) fail(%d)\n", __func__, ret);
		goto out;
	}

	if (wait_for_completion_interruptible_timeout(
			&mdla_tx_rpm_dev.ack,
			msecs_to_jiffies(10)) == 0) {
		mutex_unlock(&mdla_ipi_mtx);
		mdla_err("%s: timeout\n", __func__);
		ret = -1;
		goto out;
	}

	*val  = (u64)ipi_tx_recv_buf.data;

out:
	mutex_unlock(&mdla_ipi_mtx);
	return ret;
}

static int mdla_rpmsg_tx_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	struct mdla_ipi_data *d = (struct mdla_ipi_data *)data;
	int ret = 0;

	if (d->dir != MDLA_IPI_READ)
		goto END;

	if (d->type0 == MDLA_IPI_MICROP_MSG) {
		mdla_err("Receive uP message -> use the wrong channel!?\n");
	} else {
		ipi_tx_recv_buf.type0  = d->type0;
		ipi_tx_recv_buf.type1  = d->type1;
		ipi_tx_recv_buf.dir    = d->dir;
		ipi_tx_recv_buf.data   = d->data;
	}
	mdla_verbose("tx rpmsg cb : %d %d, %d, %llu(0x%llx)\n",
				d->type0,
				d->type1,
				d->dir,
				d->data,
				d->data);
END:
	complete(&mdla_tx_rpm_dev.ack);
	/* power off to restore ref cnt */
	ret = rpmsg_sendto(mdla_tx_rpm_dev.ept, NULL, 0, 1);
	if (ret && ret != -EOPNOTSUPP)
		mdla_err("%s: rpmsg_sendto(power off) fail(%d)\n", __func__, ret);
	return 0;
}

static void mdla_ipi_up_msg(u32 type, u64 val)
{
	struct device *mdla_dev;

	mdla_err("tpye = %d, val = 0x%llx\n", type, val);

	/* dump brisket debug information if ce is exist */
	if (is_apu_ce_excep_init()) {
		mdla_dev = mdla_get_device(0)->dev;
		apu_ce_sram_dump(mdla_dev);
	}

	if (type == MDLA_IPI_MICROP_MSG_TIMEOUT)
		mdla_aee_exception("MDLA", "MDLA timeout");
	else if (type == MDLA_IPI_MICROP_MSG_DBG_CHECK_FAILED)
		mdla_aee_exception("MDLA", "MDLA debug check failed");
	else if (type == MDLA_IPI_MICROP_MSG_CMD_FAILED)
		mdla_aee_exception("MDLA", "MDLA cmd failed");
	else if (type == MDLA_IPI_MICROP_MSG_WDEC_RESOURCE_BUSY)
		mdla_aee_exception("MDLA", "WDEC resource busy");
}

static int mdla_rpmsg_rx_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	struct mdla_ipi_data *d = (struct mdla_ipi_data *)data;

	if (d->type0 == MDLA_IPI_MICROP_MSG) {

		ipi_rx_send_buf.type0  = d->type0;
		ipi_rx_send_buf.type1  = d->type1;
		ipi_rx_send_buf.dir    = d->dir;
		ipi_rx_send_buf.data   = d->data;
		/* receive side don't need call power on, it will call by send side */
		rpmsg_send(mdla_rx_rpm_dev.ept, &ipi_rx_send_buf, sizeof(ipi_rx_send_buf));

		mdla_ipi_up_msg(d->type1, d->data);
	} else {
		mdla_err("Receive command ack -> use the wrong channel!?\n");
	}
	mdla_verbose("rpmsg cb : %d %d, %d, %llu(0x%llx)\n",
				d->type0,
				d->type1,
				d->dir,
				d->data,
				d->data);
	return 0;
}

static int mdla_rpmsg_tx_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;

	dev_info(dev, "%s: name=%s, src=%d\n", __func__,
			rpdev->id.name, rpdev->src);

	mdla_tx_rpm_dev.ept = rpdev->ept;
	mdla_tx_rpm_dev.rpdev = rpdev;

	mdla_plat_up_init();

	return 0;
}

static int mdla_rpmsg_rx_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;

	dev_info(dev, "%s: name=%s, src=%d\n", __func__,
			rpdev->id.name, rpdev->src);

	mdla_rx_rpm_dev.ept = rpdev->ept;
	mdla_rx_rpm_dev.rpdev = rpdev;

	return 0;
}

static void mdla_rpmsg_remove(struct rpmsg_device *rpdev)
{
}


static const struct of_device_id mdla_tx_rpmsg_of_match[] = {
	{ .compatible = "mediatek,mdla-tx-rpmsg"},
	{},
};

static const struct of_device_id mdla_rx_rpmsg_of_match[] = {
	{ .compatible = "mediatek,mdla-rx-rpmsg"},
	{},
};

static struct rpmsg_driver mdla_rpmsg_tx_drv = {
	.drv = {
		.name = "mdla-tx-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = mdla_tx_rpmsg_of_match,
	},
	.probe = mdla_rpmsg_tx_probe,
	.callback = mdla_rpmsg_tx_cb,
	.remove = mdla_rpmsg_remove,
};

static struct rpmsg_driver mdla_rpmsg_rx_drv = {
	.drv = {
		.name = "mdla-rx-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = mdla_rx_rpmsg_of_match,
	},
	.probe = mdla_rpmsg_rx_probe,
	.callback = mdla_rpmsg_rx_cb,
	.remove = mdla_rpmsg_remove,
};

int mdla_ipi_init(void)
{
	int ret;

	init_completion(&mdla_rx_rpm_dev.ack);
	init_completion(&mdla_tx_rpm_dev.ack);
	mutex_init(&mdla_ipi_mtx);

	ret = register_rpmsg_driver(&mdla_rpmsg_rx_drv);
	if (ret)
		mdla_err("failed to register mdla rx rpmsg\n");

	ret = register_rpmsg_driver(&mdla_rpmsg_tx_drv);
	if (ret)
		mdla_err("failed to register mdla tx rpmsg\n");

	return 0;
}

void mdla_ipi_deinit(void)
{
	unregister_rpmsg_driver(&mdla_rpmsg_tx_drv);
	unregister_rpmsg_driver(&mdla_rpmsg_rx_drv);
	mutex_destroy(&mdla_ipi_mtx);
}

