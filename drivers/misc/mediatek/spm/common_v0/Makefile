# Include cpuidle_v3 headers mtk_cpuidle.h for atf/dormant driver
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/cpuidle/
# Include mtk idle headers and cpuidle driver header for external modules
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/spm/
#ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/power_gs_v1/
# Include mtk_eem.h
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/eem_v2/$(MTK_PLATFORM)/inc/
# Include mmdvfs_mgr.h
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/
# Include uart header "mtk_uart_api.h"
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/uart/include/
# Include teei header teei_client_main.h
ifneq ($(filter y m,$(CONFIG_MICROTRUST_TEE_SUPPORT)),)
include $(DEVICE_MODULES_PATH)/drivers/tee/teei/Makefile.include
endif
# Include thermal header "mtk_thermal.h"
ifneq ($(CONFIG_MTK_LEGACY_THERMAL),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/gpufreq/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/thermal/v3/inc/mach/
endif
# Include sspm header "sspm_ipi.h"
ifneq ($(CONFIG_MTK_TINYSYS_SSPM_V1),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v1/
endif
# Include mcdi header "mtk_mcdi_api.h"
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mcdi/include/
# Include cpufreq header "mtk_cpufreq_api.h"
ifneq ($(CONFIG_MEDIATEK_CPU_DVFS),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cpufreq_v2/inc/
endif
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/clkbuf/src/

obj-$(CONFIG_MTK_SPM_V0) += MTK_INTERNAL_SPM.o
MTK_INTERNAL_SPM-y += mtk_spm.o
MTK_INTERNAL_SPM-y += mtk_spm_twam.o
MTK_INTERNAL_SPM-y += mtk_spm_irq.o
ifneq ($(CONFIG_MTK_DRAMC),)
MTK_INTERNAL_SPM-y += mtk_spm_dram.o
endif
MTK_INTERNAL_SPM-y += mtk_spm_internal.o
MTK_INTERNAL_SPM-y += mtk_spm_suspend.o
MTK_INTERNAL_SPM-y += mtk_spm_suspend_process.o
ifneq ($(CONFIG_MTK_TINYSYS_SSPM_LEGACY),)
MTK_INTERNAL_SPM-y += mtk_sspm.o
MTK_INTERNAL_SPM-y += mtk_spm_power.o
endif
MTK_INTERNAL_SPM-y += mtk_spm_fs.o
MTK_INTERNAL_SPM-y += mtk_spm_utils.o
MTK_INTERNAL_SPM-y += mtk_idle_cond_check.o
MTK_INTERNAL_SPM-y += mtk_spm_idle.o
MTK_INTERNAL_SPM-y += mtk_spm_vcore_dvfs.o
MTK_INTERNAL_SPM-y += mtk_spm_resource_req_console.o

MTK_INTERNAL_SPM-y += mtk_idle.o
MTK_INTERNAL_SPM-y += mtk_idle_select.o
MTK_INTERNAL_SPM-y += mtk_idle_internal.o
MTK_INTERNAL_SPM-y += mtk_idle_profile.o
MTK_INTERNAL_SPM-y += mtk_idle_twam.o
MTK_INTERNAL_SPM-y += mtk_spm_resource_req.o
MTK_INTERNAL_SPM-y += mtk_sleep.o
MTK_INTERNAL_SPM-y += mtk_sleep_init.o
MTK_INTERNAL_SPM-y += mtk_vcore_dvfs.o
MTK_INTERNAL_SPM-y += mtk_sodi.o
MTK_INTERNAL_SPM-y += mtk_dpidle.o
MTK_INTERNAL_SPM-y += mtk_idle_fs/mtk_lp_sysfs_procfs.o
MTK_INTERNAL_SPM-y += mtk_idle_fs/mtk_idle_sysfs.o
MTK_INTERNAL_SPM-y += mtk_idle_fs/mtk_lp_kernfs.o
MTK_INTERNAL_SPM-y += mtk_idle_fs/mtk_lp_sysfs.o
MTK_INTERNAL_SPM-y += mtk_idle_fs/mtk_lp_sysfs_procfs.o
