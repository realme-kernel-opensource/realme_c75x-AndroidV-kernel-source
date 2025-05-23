// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[orienthub] " fmt

#include <hwmsensor.h>
#include "fusion.h"
#include "orienthub.h"
#include <SCP_sensorHub.h>
#include <linux/notifier.h>

static struct fusion_init_info orienthub_init_info;

static int orientation_get_data(int *x, int *y, int *z,
	int *scalar, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp __maybe_unused = 0;

	err = sensor_get_data_from_hub(ID_ORIENTATION, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!!\n");
		return -1;
	}
	time_stamp = data.time_stamp;
	*x = data.orientation_t.azimuth;
	*y = data.orientation_t.pitch;
	*z = data.orientation_t.roll;
	*scalar = data.orientation_t.scalar;
	*status = data.orientation_t.status;
	return 0;
}

static int orientation_open_report_data(int open)
{
	return 0;
}

static int orientation_enable_nodata(int en)
{
	return sensor_enable_to_hub(ID_ORIENTATION, en);
}

static int orientation_set_delay(u64 delay)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	unsigned int delayms = 0;

	delayms = delay / 1000 / 1000;
	return sensor_set_delay_to_hub(ID_ORIENTATION, delayms);
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int orientation_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	orientation_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_ORIENTATION,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int orientation_flush(void)
{
	return sensor_flush_to_hub(ID_ORIENTATION);
}
static int orientation_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == DATA_ACTION)
		err = orientation_data_report(event->orientation_t.azimuth,
			event->orientation_t.pitch,
			event->orientation_t.roll, event->orientation_t.status,
			(int64_t)event->time_stamp);
	else if (event->flush_action == FLUSH_ACTION)
		err = orientation_flush_report();

	return err;
}
static int orienthub_local_init(void)
{
	struct fusion_control_path ctl = { 0 };
	struct fusion_data_path data = { 0 };
	int err = 0;

	ctl.open_report_data = orientation_open_report_data;
	ctl.enable_nodata = orientation_enable_nodata;
	ctl.set_delay = orientation_set_delay;
	ctl.batch = orientation_batch;
	ctl.flush = orientation_flush;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#else
#endif
	err = fusion_register_control_path(&ctl, ID_ORIENTATION);
	if (err) {
		pr_err("register orientation control path err\n");
		goto exit;
	}

	data.get_data = orientation_get_data;
	data.vender_div = 100;
	err = fusion_register_data_path(&data, ID_ORIENTATION);
	if (err) {
		pr_err("register orientation data path err\n");
		goto exit;
	}
	err = scp_sensorHub_data_registration(ID_ORIENTATION,
		orientation_recv_data);
	if (err < 0) {
		pr_err("SCP_sensorHub_data_registration failed\n");
		goto exit;
	}
	return 0;
 exit:
	return -1;
}

static int orienthub_local_uninit(void)
{
	return 0;
}

static struct fusion_init_info orienthub_init_info = {
	.name = "orientation_hub",
	.init = orienthub_local_init,
	.uninit = orienthub_local_uninit,
};

static int __init orienthub_init(void)
{
	pr_debug("%s \r\n", __func__);
	fusion_driver_add(&orienthub_init_info, ID_ORIENTATION);
	return 0;
}

static void __exit orienthub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(orienthub_init);
module_exit(orienthub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITYHUB driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
