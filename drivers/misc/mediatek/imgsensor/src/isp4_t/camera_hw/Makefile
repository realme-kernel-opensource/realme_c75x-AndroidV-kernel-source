# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \

IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/regulator/regulator.o \
	$(subdrv-rpath)/gpio/gpio.o \
	$(subdrv-rpath)/mclk/mclk.o \
	$(subdrv-rpath)/imgsensor_cfg_table.o \

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_T), m)
$(eval $(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ))
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_T), y)
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_T) += $(IMGSENSOR_SUBDRV_OBJ)
endif
