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

IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/s5kjn103mipiraw_Sensor.o \

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR), m y))
imgsensor_isp6s-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_MON), m y))
imgsensor_isp6s_mon-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_MOU), m y))
imgsensor_isp6s_mou-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif

ifneq (,$(filter $(CONFIG_MTK_IMGSENSOR_ISP6S_LAG), m y))
imgsensor_isp6s_lag-objs += $(IMGSENSOR_SUBDRV_OBJ)
endif