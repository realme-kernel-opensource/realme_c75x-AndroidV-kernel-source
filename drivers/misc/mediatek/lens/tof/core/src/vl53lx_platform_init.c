// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include "vl53lx_platform.h"
#include <vl53lx_platform_init.h>
#include "vl53lx_ll_def.h"


VL53LX_Error VL53LX_platform_init(
	struct VL53LX_Dev_t *pdev,
	uint8_t       i2c_slave_address,
	uint8_t       comms_type,
	uint16_t      comms_speed_khz)
{


	VL53LX_Error status = VL53LX_ERROR_NONE;



	pdev->i2c_slave_address = i2c_slave_address;
	pdev->comms_type        = comms_type;
	pdev->comms_speed_khz   = comms_speed_khz;

	if (status == VL53LX_ERROR_NONE)
		status =
			VL53LX_CommsInitialise(
				pdev,
				pdev->comms_type,
				pdev->comms_speed_khz);


	if (status == VL53LX_ERROR_NONE)
		status = VL53LX_GpioXshutdown(0);


	if (status == VL53LX_ERROR_NONE)
		status = VL53LX_GpioPowerEnable(0);


	if (status == VL53LX_ERROR_NONE)
		status = VL53LX_GpioCommsSelect(0);


	if (status == VL53LX_ERROR_NONE)
		status = VL53LX_WaitUs(pdev, 1000);


	if (status == VL53LX_ERROR_NONE)
		status = VL53LX_GpioPowerEnable(1);


	if (status == VL53LX_ERROR_NONE)
		status = VL53LX_WaitUs(pdev, 1000);


	if (status == VL53LX_ERROR_NONE)
		status = VL53LX_GpioXshutdown(1);


	if (status == VL53LX_ERROR_NONE)
		status = VL53LX_WaitUs(pdev, 100);

	return status;
}


VL53LX_Error VL53LX_platform_terminate(
		struct VL53LX_Dev_t *pdev)
{


	VL53LX_Error status = VL53LX_ERROR_NONE;


	if (status == VL53LX_ERROR_NONE)
		status = VL53LX_GpioXshutdown(0);


	if (status == VL53LX_ERROR_NONE)
		status = VL53LX_GpioPowerEnable(0);


	if (status == VL53LX_ERROR_NONE)
		status = VL53LX_CommsClose(pdev);

	return status;
}



