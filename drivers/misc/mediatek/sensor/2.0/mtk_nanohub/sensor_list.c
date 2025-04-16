// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "<sensorlist> " fmt

#include <linux/module.h>

#include "sensor_list.h"
#include "hf_sensor_type.h"

enum sensorlist {
	accel,
	gyro,
	mag,
	als,
	ps,
	baro,
	sar,
	ois,
	front_cct,
	pwm_rgb,
	flicker,
	maxhandle,
};

int sensorlist_sensor_to_handle(int sensor)
{
	int handle = -1;

	switch (sensor) {
	case SENSOR_TYPE_ACCELEROMETER:
		handle = accel;
		break;
	case SENSOR_TYPE_GYROSCOPE:
		handle = gyro;
		break;
	case SENSOR_TYPE_MAGNETIC_FIELD:
		handle = mag;
		break;
	case SENSOR_TYPE_LIGHT:
		handle = als;
		break;
	case SENSOR_TYPE_PROXIMITY:
		handle = ps;
		break;
	case SENSOR_TYPE_PRESSURE:
		handle = baro;
		break;
	case SENSOR_TYPE_SAR:
		handle = sar;
		break;
	case SENSOR_TYPE_OIS:
		handle = ois;
		break;
	case SENSOR_TYPE_FRONT_CCT:
                handle = front_cct;
                break;
        case SENSOR_TYPE_PWM_RGB:
                handle = pwm_rgb;
                break;
	case SENSOR_TYPE_FLICKER:
		handle = flicker;
		break;
	}
	return handle;
}

int sensorlist_handle_to_sensor(int handle)
{
	int type = -1;

	switch (handle) {
	case accel:
		type = SENSOR_TYPE_ACCELEROMETER;
		break;
	case gyro:
		type = SENSOR_TYPE_GYROSCOPE;
		break;
	case mag:
		type = SENSOR_TYPE_MAGNETIC_FIELD;
		break;
	case als:
		type = SENSOR_TYPE_LIGHT;
		break;
	case ps:
		type = SENSOR_TYPE_PROXIMITY;
		break;
	case baro:
		type = SENSOR_TYPE_PRESSURE;
		break;
	case sar:
		type = SENSOR_TYPE_SAR;
		break;
	case ois:
		type = SENSOR_TYPE_OIS;
		break;
	case front_cct:
                type = SENSOR_TYPE_FRONT_CCT;
                break;
        case pwm_rgb:
                type = SENSOR_TYPE_PWM_RGB;
                break;
	case flicker:
		type = SENSOR_TYPE_FLICKER;
		break;
	}
	return type;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("dynamic sensorlist driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
