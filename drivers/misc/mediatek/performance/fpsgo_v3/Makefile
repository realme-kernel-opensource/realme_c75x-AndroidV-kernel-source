ifeq ($(CONFIG_MTK_FPSGO_V3),$(filter $(CONFIG_MTK_FPSGO_V3),m))
ccflags-y += -DCONFIG_MTK_FPSGO_V3
subdir-ccflags-y += -DCONFIG_MTK_FPSGO_V3
endif

FPSGO_TOP = $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance/fpsgo_v3/
BASE_TOP = $(FPSGO_TOP)/base
FBT_TOP = $(FPSGO_TOP)/fbt
FSTB_TOP = $(FPSGO_TOP)/fstb
COM_TOP = $(FPSGO_TOP)/composer

ccflags-y += \
        -I$(srctree)/include/ \
        -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/ \
        -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/ged/include/ \
        -I$(FPSGO_TOP)/mt-plat/ \
        -I$(FPSGO_TOP)/ \
        -I$(BASE_TOP)/include/ \
        -I$(FBT_TOP)/include/ \
        -I$(FSTB_TOP)/ \
        -I$(COM_TOP)/include/ \
        -I$(srctree)/kernel/ \
        -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/ \
        -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/ \
        -I$(DEVICE_MODULES_PATH)/drivers/thermal/mediatek/ \
        -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance/powerhal_cpu_ctrl/ \

subdir-ccflags-y += \
        -I$(srctree)/include/ \
        -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/ \
        -I$(DEVICE_MODULES_PATH)/drivers/gpu/mediatek/ged/include/ \
        -I$(FPSGO_TOP)/mt-plat/ \
        -I$(FPSGO_TOP)/ \
        -I$(BASE_TOP)/include/ \
        -I$(FBT_TOP)/include/ \
        -I$(FSTB_TOP)/ \
        -I$(COM_TOP)/include/ \
        -I$(srctree)/kernel/ \
        -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sched/ \
        -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/ \
        -I$(DEVICE_MODULES_PATH)/drivers/thermal/mediatek/ \
        -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/performance/powerhal_cpu_ctrl/ \

#fpsgo_main
MTK_FPSGO_MAIN += fpsgo_main.o

#base
include $(FPSGO_TOP)/base/build.mk
MTK_FPSGO_BASE = $(mtk_fpsgo_base_objs:%.o=base/%.o)

#composer
include $(FPSGO_TOP)/composer/build.mk
MTK_FPSGO_COMPOSER = $(mtk_fpsgo_composer_objs:%.o=composer/%.o)

#fbt
include $(FPSGO_TOP)/fbt/build.mk
MTK_FPSGO_FBT = $(mtk_fpsgo_fbt_objs:%.o=fbt/%.o)

#fstb
include $(FPSGO_TOP)/fstb/build.mk
MTK_FPSGO_FSTB = $(mtk_fpsgo_fstb_objs:%.o=fstb/%.o)

MTK_FPSGO_SRC = $(MTK_FPSGO_MAIN) $(MTK_FPSGO_BASE) $(MTK_FPSGO_COMPOSER) $(MTK_FPSGO_FBT) $(MTK_FPSGO_FSTB)

obj-$(CONFIG_MTK_FPSGO_V3) += mtk_fpsgo.o
mtk_fpsgo-objs += $(MTK_FPSGO_SRC)

