# Goodix Touchscreen Makefile
# This Makefile is only for tempory compiling use
# Use xxxproject/Makefile-release when release driver
#
#For display ESD recovery

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-m += legacy_gt9896s.o
legacy_gt9896s-y += goodix_ts_spi.o
legacy_gt9896s-y += goodix_ts_core.o
legacy_gt9896s-y += goodix_cfg_bin.o
legacy_gt9896s-y += goodix_gtx8_update.o
legacy_gt9896s-n += goodix_ts_tools.o
legacy_gt9896s-y += goodix_ts_tui.o
