/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Ming Hsiu Tsai <minghsiu.tsai@mediatek.com>
 *         Rick Chang <rick.chang@mediatek.com>
 *         Xia Jiang <xia.jiang@mediatek.com>
 */

#ifndef _MTK_JPEG_CORE_H
#define _MTK_JPEG_CORE_H

#include <linux/interrupt.h>
#include "mtk-interconnect-provider.h"
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>

#define MTK_JPEG_NAME		"mtk-jpeg"

#define MTK_JPEG_COMP_MAX		3
#define MTK_JPEG_MAX_FREQ		8
#define MTK_JPEG_MAX_LARB_COUNT		2

#define MTK_JPEG_FMT_FLAG_OUTPUT	BIT(0)
#define MTK_JPEG_FMT_FLAG_CAPTURE	BIT(1)

#define MTK_JPEG_MIN_WIDTH	32U
#define MTK_JPEG_MIN_HEIGHT	32U
#define MTK_JPEG_MAX_WIDTH	65535U
#define MTK_JPEG_MAX_HEIGHT	65535U

#define MTK_JPEG_DEFAULT_SIZEIMAGE	(1 * 1024 * 1024)

#define MTK_JPEG_DEFAULT_WIDTH	(8 * 1024)

#define MTK_JPEG_DEFAULT_HEIGHT	(8 * 1024)

#define MTK_JPEG_HW_TIMEOUT_MSEC 1000

#define MTK_JPEG_DVFS_BW_SUPPORT 1

#if IS_ENABLED(CONFIG_JPEG_V4L2_DECODE)
#define MTK_JPEG_DEC_SUPPORT 1
#else
#define MTK_JPEG_DEC_SUPPORT 0
#endif

#define MTK_JPEG_MAX_EXIF_SIZE	(64 * 1024)
#define NS_TO_MS(X) (div_u64(X, 1000000))


#define SNPRINTF(args...)							\
	do {											\
		if (snprintf(args) < 0)						\
			pr_notice("snprintf error\n");			\
	} while (0)

enum mtk_enc_dtsi_reg_idx {
	VENC_SYS,
	VENC_GCON,
	NUM_MAX_VENC_REG_BASE
};
/**
 * enum mtk_jpeg_ctx_state - states of the context state machine
 * @MTK_JPEG_INIT:		current state is initialized
 * @MTK_JPEG_RUNNING:		current state is running
 * @MTK_JPEG_SOURCE_CHANGE:	current state is source resolution change
 */
enum mtk_jpeg_ctx_state {
	MTK_JPEG_INIT = 0,
	MTK_JPEG_RUNNING,
	MTK_JPEG_SOURCE_CHANGE,
};
/**
 * enum mtk_jpeg_support_34bits - value of support 34bits
 * @MTK_JPEG_NON_SUPPORT_34bits:		legacy low-end chip is 0
 * @MTK_JPEG_SUPPORT_34bits:		not define in dts default is 1
 * @MTK_JPEG_FAKE_34bits:	mt6835(fake34bits feature) is 2
 */
enum mtk_jpeg_support_34bits {
	MTK_JPEG_NON_SUPPORT_34BITS = 0,
	MTK_JPEG_SUPPORT_34BITS,
	MTK_JPEG_FAKE_34BITS,
};

struct JpegDecClk {
	struct clk *clk_jpgDec;
	struct clk *clk_jpgDec_c1;
	struct clk *clk_jpgDec_c2;
};

enum mtk_jpeg_drv_type {
	MTK_JPEG_DEC_C0 = 0,
	MTK_JPEG_DEC_C1,
	MTK_JPEG_ENC,
};

/**
 * mtk_jpeg_variant - mtk jpeg driver variant
 * @clks:			clock names
 * @num_clks:			numbers of clock
 * @format:			jpeg driver's internal color format
 * @num_format:			number of format
 * @qops:			the callback of jpeg vb2_ops
 * @irq_handler:		jpeg irq handler callback
 * @hw_reset:			jpeg hardware reset callback
 * @m2m_ops:			the callback of jpeg v4l2_m2m_ops
 * @dev_name:			jpeg device name
 * @ioctl_ops:			the callback of jpeg v4l2_ioctl_ops
 * @out_q_default_fourcc:	output queue default fourcc
 * @cap_q_default_fourcc:	capture queue default fourcc
 */
struct mtk_jpeg_variant {
	struct clk_bulk_data *clks;
	int num_clks;
	struct mtk_jpeg_fmt *formats;
	int num_formats;
	const struct vb2_ops *qops;
	irqreturn_t (*irq_handler)(int irq, void *priv);
	void (*hw_reset)(void __iomem *base);
	const struct v4l2_m2m_ops *m2m_ops;
	const char *dev_name;
	const struct v4l2_ioctl_ops *ioctl_ops;
	u32 out_q_default_fourcc;
	u32 cap_q_default_fourcc;
};

/**
 * struct mt_jpeg - JPEG IP abstraction
 * @lock:		the mutex protecting this structure
 * @hw_lock:		spinlock protecting the hw device resource
 * @workqueue:		decode work queue
 * @dev:		JPEG device
 * @v4l2_dev:		v4l2 device for mem2mem mode
 * @m2m_dev:		v4l2 mem2mem device data
 * @alloc_ctx:		videobuf2 memory allocator's context
 * @vdev:		video device node for jpeg mem2mem mode
 * @reg_base:		JPEG registers mapping
 * @larb:		SMI device
 * @job_timeout_work:	IRQ timeout structure
 * @variant:		driver variant to be used
 */
struct mtk_jpeg_dev {
	struct mutex		lock;
	spinlock_t		hw_lock;
	struct workqueue_struct	*workqueue;
	struct device		*dev;
	struct v4l2_device	v4l2_dev;
	struct v4l2_m2m_dev	*m2m_dev;
	void			*alloc_ctx;
	struct video_device	*vdev;
	void __iomem		*reg_base;
	void __iomem		*gcon_base;
	struct device		*larb[MTK_JPEG_MAX_LARB_COUNT];
	struct delayed_work job_timeout_work;
	const struct mtk_jpeg_variant *variant;
	struct icc_path *path_y_rdma;
	struct icc_path *path_c_rdma;
	struct icc_path *path_qtbl;
	struct icc_path *path_bsdma;
	struct regulator *jpegenc_reg;
	struct clk *jpegenc_mmdvfs_clk;
	int freq_cnt;
	int jpeg_dev_type;
	struct JpegDecClk jpegDecClk;
	unsigned long freqs[MTK_JPEG_MAX_FREQ];
	enum mtk_jpeg_support_34bits support_34bits;
	struct device *smmu_dev;
};

/**
 * struct jpeg_fmt - driver's internal color format data
 * @fourcc:	the fourcc code, 0 if not applicable
 * @hw_format:	hardware format value
 * @h_sample:	horizontal sample count of plane in 4 * 4 pixel image
 * @v_sample:	vertical sample count of plane in 4 * 4 pixel image
 * @colplanes:	number of color planes (1 for packed formats)
 * @h_align:	horizontal alignment order (align to 2^h_align)
 * @v_align:	vertical alignment order (align to 2^v_align)
 * @flags:	flags describing format applicability
 */
struct mtk_jpeg_fmt {
	u32	fourcc;
	u32	hw_format;
	int	h_sample[VIDEO_MAX_PLANES];
	int	v_sample[VIDEO_MAX_PLANES];
	int	colplanes;
	int	h_align;
	int	v_align;
	u32	flags;
};

/**
 * mtk_jpeg_q_data - parameters of one queue
 * @fmt:	  driver-specific format of this queue
 * @pix_mp:	  multiplanar format
 * @enc_crop_rect:	jpeg encoder crop information
 */
struct mtk_jpeg_q_data {
	struct mtk_jpeg_fmt	*fmt;
	struct v4l2_pix_format_mplane pix_mp;
	struct v4l2_rect enc_crop_rect;
};

/**
 * mtk_jpeg_ctx - the device context data
 * @jpeg:		JPEG IP device for this context
 * @out_q:		source (output) queue information
 * @cap_q:		destination (capture) queue queue information
 * @fh:			V4L2 file handle
 * @state:		state of the context
 * @enable_exif:	enable exif mode of jpeg encoder
 * @enc_quality:	jpeg encoder quality
 * @restart_interval:	jpeg encoder restart interval
 * @ctrl_hdl:		controls handler
 */
struct mtk_jpeg_ctx {
	struct mtk_jpeg_dev		*jpeg;
	struct mtk_jpeg_q_data		out_q;
	struct mtk_jpeg_q_data		cap_q;
	struct v4l2_fh			fh;
	enum mtk_jpeg_ctx_state		state;
	bool enable_exif;
	u8 enc_quality;
	u8 restart_interval;
	struct v4l2_ctrl_handler ctrl_hdl;
	u32 dst_offset;
	bool early_eos;
	u64 time_start; // jiffies_to_nsecs
	u64 time_end;
	u32 size_output;
};

#endif /* _MTK_JPEG_CORE_H */
