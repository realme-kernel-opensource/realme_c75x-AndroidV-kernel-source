# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2021 MediaTek Inc.
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/spi_slave_drv

obj-$(CONFIG_MTK_MT6382_BDG) += cmdq-bdg-mailbox.o
obj-$(CONFIG_MTK_MT6382_BDG) += cmdq-bdg-test.o