
MTK_CUSTOM_PATH  := $(DEVICE_MODULES_PATH)/drivers/misc/mediatek

$(info FDVT Tina: Drv use folder: $(DEVICE_MODULES_PATH))

ifneq (, $(filter $(strip $(CONFIG_MTK_CAMERA_FD_VERSION40)),m y))
  $(info FDVT LBJ: Drv use 4.0 folder)
  MTKCAM_FDVT_VERSION := 4.0
  MTKCAM_FDVT_PATH := $(MTK_CUSTOM_PATH)/cameraisp/fdvt/4.0
  subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/cameraisp/fdvt/4.0/inc
  obj-y += 4.0/
endif

ifneq (, $(filter $(strip $(CONFIG_MTK_CAMERA_FD_ISP6S)),m y))
  $(info FDVT: Drv use 5.1 folder)
  MTKCAM_FDVT_VERSION := 5.1
  MTKCAM_FDVT_PATH := $(MTK_CUSTOM_PATH)/cameraisp/fdvt/5.1
  subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/cameraisp/fdvt/5.1/inc
  $(info fdvt tina)
endif

ifneq (, $(filter $(strip $(CONFIG_MTK_CAMERA_FD_ISP6S_V1)),m y))
  $(info FDVT: Drv use 5.1 V1 folder)
  MTKCAM_FDVT_VERSION := 5.1
  MTKCAM_FDVT_PATH := $(MTK_CUSTOM_PATH)/cameraisp/fdvt/6781
  subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/cameraisp/fdvt/6781/inc
  $(info 6781 fdvt tina)
endif

ifneq (, $(filter $(strip $(CONFIG_MTK_CAMERA_FD_VERSION35)),m y))
  $(info FDVT: Drv use 3.5 folder)
  MTKCAM_FDVT_VERSION := 3.5
  MTKCAM_FDVT_PATH := $(MTK_CUSTOM_PATH)/cameraisp/fdvt/3.5
  subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/cameraisp/fdvt/3.5/inc
endif

#ifdef CONFIG_MTK_M4U
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
#else
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/
#endif
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/dma-buf/heaps/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/staging/android/ion \
	     -I$(DEVICE_MODULES_PATH)/drivers/staging/android/ion/mtk
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/

ifneq (, $(filter $(strip $(CONFIG_MTK_CAM_SECURE_FD_SUPPORT)),m y))
ccflags-y += -DCMDQ_MTEE
endif

#obj-y:= fdvt_dummy.o

ifneq (, $(filter $(strip $(CONFIG_MTK_CAMERA_FD_ISP6S)),m y))
obj-$(CONFIG_MTK_CAMERA_FD_ISP6S) += camera_fdvt_isp51.o
camera_fdvt_isp51-objs += 5.1/camera_fdvt.o
endif

ifneq (, $(filter $(strip $(CONFIG_MTK_CAMERA_FD_ISP6S_V1)),m y))
obj-$(CONFIG_MTK_CAMERA_FD_ISP6S_V1) += camera_fdvt_isp51_v1.o
camera_fdvt_isp51_v1-objs += 6781/camera_fdvt.o
endif

ifneq (, $(filter $(strip $(CONFIG_MTK_CAMERA_FD_VERSION40)),m y))
obj-$(CONFIG_MTK_CAMERA_FD_VERSION40) += camera_fdvt_isp40.o
camera_fdvt_isp40-objs += 4.0/camera_fdvt.o
endif

ifneq (, $(filter $(strip $(CONFIG_MTK_CAMERA_FD_VERSION35)),m y))
obj-$(CONFIG_MTK_CAMERA_FD_VERSION35) += camera_fdvt_isp35.o
camera_fdvt_isp35-objs += 3.5/camera_fdvt.o
endif

ifeq ($(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT)),y)
  MTK_SECURE_FD_SUPPORT := 1
else
  MTK_SECURE_FD_SUPPORT := 0
endif

subdir-ccflags-y += -DMTK_SECURE_FD_SUPPORT=$(MTK_SECURE_FD_SUPPORT)

