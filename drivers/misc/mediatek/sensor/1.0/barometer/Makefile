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

obj-$(CONFIG_MTK_SENSOR_SUPPORT)  += baro_common.o
baro_common-$(CONFIG_MTK_SENSOR_SUPPORT) += barometer.o barometer_factory.o

obj-$(CONFIG_MTK_BAROHUB) += barohub/
ifeq ($(CONFIG_MTK_BMP280_NEW),y)
obj-$(CONFIG_MTK_BMP280_NEW) += BMP280-new/
endif
