# SPDX-License-Identifier: GPL-2.0

ifeq (m, $(CONFIG_DRM_MEDIATEK_DUMMY))
obj-$(CONFIG_DRM_MEDIATEK_DUMMY) += dummy_drm/
else ifeq (y, $(filter y, $(CONFIG_DRM_MEDIATEK_V2)))
obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += mediatek_v2/
obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += mml/
else

mediatek-drm-y := mtk_disp_aal.o \
		  mtk_disp_ccorr.o \
		  mtk_disp_color.o \
		  mtk_disp_gamma.o \
		  mtk_disp_ovl.o \
		  mtk_disp_rdma.o \
		  mtk_drm_crtc.o \
		  mtk_drm_ddp.o \
		  mtk_drm_ddp_comp.o \
		  mtk_drm_drv.o \
		  mtk_drm_gem.o \
		  mtk_drm_plane.o \
		  mtk_dsi.o \
		  mtk_mipi_tx.o \
		  mtk_mt8173_mipi_tx.o \
		  mtk_mt8183_mipi_tx.o \
		  mtk_dpi.o

obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += mediatek-drm.o

mediatek-drm-hdmi-objs := mtk_cec.o \
			  mtk_hdmi.o \
			  mtk_hdmi_ddc.o

obj-$(CONFIG_DRM_MEDIATEK_HDMI) += mediatek-drm-hdmi.o
endif

obj-$(CONFIG_MTK_DPC) += dpc/
