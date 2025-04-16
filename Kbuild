# SPDX-License-Identifier: GPL-2.0

ifneq ($(CONFIG_DEVICE_MODULES_ALLOW_BUILTIN),y)

LINUXINCLUDE := $(DEVCIE_MODULES_INCLUDE) $(LINUXINCLUDE)

subdir-ccflags-y += -Werror \
		-I$(srctree)/$(src)/include \
		-I$(srctree)/$(src)/include/uapi \

obj-y += drivers/memory/

obj-y += drivers/iio/adc/

obj-y += drivers/mfd/

obj-y += drivers/nvmem/

obj-y += drivers/dma/mediatek/

obj-y += drivers/ufs/

obj-y += drivers/char/

obj-y += drivers/clk/mediatek/

obj-y += drivers/clocksource/

obj-y += drivers/cpufreq/

obj-y += drivers/soc/mediatek/

obj-y += drivers/watchdog/

obj-y += drivers/dma-buf/heaps/

obj-y += drivers/regulator/

obj-y += drivers/leds/

obj-y += drivers/pinctrl/mediatek/

obj-y += drivers/power/supply/

obj-y += drivers/power/oplus/

obj-y += drivers/rtc/

obj-y += drivers/remoteproc/

obj-y += drivers/rpmsg/

obj-y += drivers/input/keyboard/

obj-y += drivers/phy/mediatek/

obj-y += drivers/thermal/mediatek/

obj-y += drivers/spmi/

obj-y += drivers/tty/serial/8250/

obj-y += drivers/reset/

obj-y += drivers/mailbox/

obj-y += drivers/interconnect/

obj-y += drivers/i2c/busses/

obj-y += drivers/i3c/master/

obj-y += drivers/pwm/

obj-y += drivers/spi/

obj-y += drivers/iommu/

obj-y += drivers/mmc/host/

obj-y += drivers/tee/

obj-y += drivers/gpu/drm/mediatek/

obj-y += drivers/input/touchscreen/

obj-y += drivers/gpu/drm/panel/

obj-y += drivers/gpu/drm/bridge/

obj-y += drivers/gpu/mediatek/

obj-y += drivers/media/platform/

obj-y += drivers/usb/

obj-y += drivers/net/ethernet/stmicro/

obj-y += drivers/net/phy/

obj-y += drivers/devfreq/

obj-y += drivers/misc/mediatek/

obj-y += sound/soc/codecs/

obj-y += sound/soc/mediatek/

obj-y += sound/virtio/

obj-y += drivers/pci/controller/

obj-y += drivers/soc/oplus/oplus_mm_fb/

obj-y += drivers/soc/oplus/boot/

obj-y += drivers/base/kernelFwUpdate/

obj-y += drivers/base/touchpanel_notify/

obj-y += drivers/soc/oplus/device_info/

obj-y += drivers/soc/oplus/storage/common/ufs_oplus_dbg/

obj-y += drivers/misc/oplus/vibrator/aw8697_haptic/

obj-y += drivers/misc/oplus/oplus_gpio/

obj-y += drivers/gpu/oplus_mali_interface/

obj-y += drivers/misc/oplus/sim_detect/

obj-y += drivers/misc/oplus/cs_press/

obj-y += drivers/soc/oplus/dft/

obj-y += drivers/video/backlight/

obj-y += drivers/soc/oplus/storage/common/io_metrics/

obj-$(CONFIG_OPLUS_FEATURE_CPU) += kernel/oplus_cpu/

obj-$(CONFIG_OPLUS_MAGCVR_NOTIFY) += drivers/base/magtransfer/

obj-y += drivers/soc/oplus/storage/

obj-y += drivers/gpu/oplus_mali_interface/

obj-y += drivers/soc/oplus/storage/common/oplus_f2fslog_storage

ifeq ($(CONFIG_MTK_SENSOR_ARCHITECTURE),2.0)
obj-y += drivers/misc/mediatek/sensor/2.0/oplus_sensor_devinfo/
obj-y += drivers/misc/mediatek/sensor/2.0/oplus_virtual_sensor/
obj-y += drivers/misc/mediatek/sensor/2.0/oplus_consumer_ir/
else
obj-y += drivers/misc/mediatek/sensor/1.0/oplus_sensor_devinfo/
obj-y += drivers/misc/mediatek/sensor/1.0/oplus_virtual_sensor/
endif


obj-$(CONFIG_OPLUS_FEATURE_FP) += drivers/input/fingerprint/
endif
