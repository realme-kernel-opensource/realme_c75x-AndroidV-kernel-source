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

#obj-$(CONFIG_ISP_DIP_SUPPORT) := cameradip_dummy.o
MTK_DIP_COUNT := 1
MTK_MSF_OFFSET := 0
$(warning "Marvin test")
$(warning $(CONFIG_MTK_CAMERA_ISPVER))
$(warning $(CONFIG_ISP_DIP_SUPPORT))

ifeq ($(CONFIG_MACH_MT6775),y)
  obj-y += $(subst ",,isp_50)/
endif

ifeq ($(CONFIG_MACH_MT6771),y)
  obj-y += $(subst ",,isp_50)/
endif

ifeq ($(CONFIG_MACH_MT6885),y)
  obj-y += $(subst ",,isp_6s)/
  MTK_DIP_COUNT := 2
endif

ifeq ($(CONFIG_MACH_MT6883),y)
  obj-y += $(subst ",,isp_6s)/
  MTK_DIP_COUNT := 2
endif

ifeq ($(CONFIG_MTK_CAMERA_ISP_VER),"isp6s")
obj-$(CONFIG_MTK_CAMERA_ISP_DIP_SUPPORT) += isp_6s/
MTK_MSF_OFFSET := 1
endif

ifeq ($(CONFIG_MTK_CAMERA_DIP_ISP6S),m)
	obj-y += $(subst ",,isp_6s)/
endif

ifeq ($(CONFIG_MACH_MT6853),y)
  obj-y += $(subst ",,isp_6s)/
  MTK_MSF_OFFSET := 1
endif

ifeq ($(CONFIG_MACH_MT3967),y)
  obj-y += $(subst ",,isp_50)/
endif

subdir-ccflags-y += -DMTK_DIP_COUNT=$(MTK_DIP_COUNT)
subdir-ccflags-y += -DMTK_MSF_OFFSET=$(MTK_MSF_OFFSET)
