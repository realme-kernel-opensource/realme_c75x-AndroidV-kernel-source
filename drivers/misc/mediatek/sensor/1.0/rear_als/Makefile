#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# In case the platform does NOT support this type of sensors
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sensor/1.0/hwmon/include

obj-$(CONFIG_MTK_SENSOR_SUPPORT)   +=  rear_als_common.o
rear_als_common-$(CONFIG_MTK_SENSOR_SUPPORT) := rear_als.o rear_als_factory.o

obj-$(CONFIG_MTK_REAR_ALSHUB) += rear_alshub/
