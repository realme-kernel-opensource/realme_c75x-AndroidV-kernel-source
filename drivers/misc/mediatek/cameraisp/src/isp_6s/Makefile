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

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/staging/android/ion/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cam_timesync/
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/dt-bindings/memory/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cameraisp/src/common/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cameraisp/src/isp_6s/

obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT) += camera_isp.o
obj-$(CONFIG_MTK_CAMERA_ISP_RAW_SUPPORT) += cam_qos.o
