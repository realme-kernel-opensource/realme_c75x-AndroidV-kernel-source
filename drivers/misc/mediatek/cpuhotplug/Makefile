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

# common source and includes
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/ppm_v3/inc/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/inc/


obj-$(CONFIG_MTK_CPUHOTPLUG) += mtk_cpuhp.o
mtk_cpuhp-y += mtk_cpuhp_core.o
mtk_cpuhp-y += mtk_cpuhp_platform.o
ifneq ($(strip $(CONFIG_MTK_PPM_V3)),)
mtk_cpuhp-y += mtk_cpuhp_ppm.o
endif
