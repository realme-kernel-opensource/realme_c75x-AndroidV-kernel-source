// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Ming Hsiu Tsai <minghsiu.tsai@mediatek.com>
 *         Rick Chang <rick.chang@mediatek.com>
 *         Xia Jiang <xia.jiang@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/jiffies.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include <soc/mediatek/smi.h>

#include "mtk-interconnect.h"
#include "mtk_jpeg_enc_hw.h"
#include "mtk_jpeg_dec_hw.h"
#include "mtk_jpeg_core.h"
#include "mtk_jpeg_dec_parse.h"
#include "mtk-smmu-v3.h"

#include <linux/dma-buf.h>

static struct mtk_jpeg_fmt mtk_jpeg_enc_formats[] = {
	{
		.fourcc		= V4L2_PIX_FMT_JPEG,
		.colplanes	= 1,
		.flags		= MTK_JPEG_FMT_FLAG_CAPTURE,
	},
	{
		.fourcc		= V4L2_PIX_FMT_NV12M,
		.hw_format	= JPEG_ENC_YUV_FORMAT_NV12,
		.h_sample	= {4, 4},
		.v_sample	= {4, 2},
		.colplanes	= 2,
		.h_align	= 16,
		.v_align	= 16,
		.flags		= MTK_JPEG_FMT_FLAG_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_NV21M,
		.hw_format	= JEPG_ENC_YUV_FORMAT_NV21,
		.h_sample	= {4, 4},
		.v_sample	= {4, 2},
		.colplanes	= 2,
		.h_align	= 16,
		.v_align	= 16,
		.flags		= MTK_JPEG_FMT_FLAG_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_YUYV,
		.hw_format	= JPEG_ENC_YUV_FORMAT_YUYV,
		.h_sample	= {8},
		.v_sample	= {4},
		.colplanes	= 1,
		.h_align	= 32,
		.v_align	= 8,
		.flags		= MTK_JPEG_FMT_FLAG_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_YVYU,
		.hw_format	= JPEG_ENC_YUV_FORMAT_YVYU,
		.h_sample	= {8},
		.v_sample	= {4},
		.colplanes	= 1,
		.h_align	= 32,
		.v_align	= 8,
		.flags		= MTK_JPEG_FMT_FLAG_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_NV12_AFBC,
		.hw_format	= JPEG_ENC_YUV_FORMAT_NV12,
		.h_sample	= {4},
		.v_sample	= {4},
		.colplanes	= 1,
		.h_align	= 16,
		.v_align	= 16,
		.flags		= MTK_JPEG_FMT_FLAG_OUTPUT,
	},
};

static struct mtk_jpeg_fmt mtk_jpeg_dec_formats[] = {
	{
		.fourcc		= V4L2_PIX_FMT_JPEG,
		.colplanes	= 1,
		.flags		= MTK_JPEG_FMT_FLAG_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_YUV420M,
		.h_sample	= {4, 2, 2},
		.v_sample	= {4, 2, 2},
		.colplanes	= 3,
		.h_align	= 32,
		.v_align	= 16,
		.flags		= MTK_JPEG_FMT_FLAG_CAPTURE,
	},
	{
		.fourcc		= V4L2_PIX_FMT_YUV422M,
		.h_sample	= {4, 2, 2},
		.v_sample	= {4, 4, 4},
		.colplanes	= 3,
		.h_align	= 32,
		.v_align	= 8,
		.flags		= MTK_JPEG_FMT_FLAG_CAPTURE,
	},
};

#define MTK_JPEG_ENC_NUM_FORMATS ARRAY_SIZE(mtk_jpeg_enc_formats)
#define MTK_JPEG_DEC_NUM_FORMATS ARRAY_SIZE(mtk_jpeg_dec_formats)

struct mtk_jpeg_src_buf {
	struct vb2_v4l2_buffer b;
	struct list_head list;
	u32 bs_size;
	struct mtk_jpeg_dec_param dec_param;
};

static int debug;
static int jpgenc_probe_time;
module_param(debug, int, 0644);

#define JPEG_LOG(level, format, args...)                       \
	do {                                                        \
		if ((debug & level) == level)              \
			pr_info("level=%d %s(),%d: " format "\n",\
				level, __func__, __LINE__, ##args);      \
	} while (0)

#if MTK_JPEG_DEC_SUPPORT
static int jpgDec_probe_time;
#endif
static inline struct mtk_jpeg_ctx *ctrl_to_ctx(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct mtk_jpeg_ctx, ctrl_hdl);
}

static inline struct mtk_jpeg_ctx *mtk_jpeg_fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_jpeg_ctx, fh);
}

static inline struct mtk_jpeg_src_buf *mtk_jpeg_vb2_to_srcbuf(
							struct vb2_buffer *vb)
{
	return container_of(to_vb2_v4l2_buffer(vb), struct mtk_jpeg_src_buf, b);
}

static int mtk_jpeg_querycap(struct file *file, void *priv,
			     struct v4l2_capability *cap)
{
	struct mtk_jpeg_dev *jpeg = video_drvdata(file);

	strscpy(cap->driver, jpeg->variant->dev_name, sizeof(cap->driver));
	strscpy(cap->card, jpeg->variant->dev_name, sizeof(cap->card));
	SNPRINTF(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
			dev_name(jpeg->dev));
#if MTK_JPEG_DEC_SUPPORT
	if (jpeg->jpeg_dev_type != MTK_JPEG_ENC) {
		v4l2_info(&jpeg->v4l2_dev, "dec device numbers: %d\n", jpgDec_probe_time);
		cap->reserved[0] = jpgDec_probe_time;
	} else
#endif
		{
			cap->reserved[0] = jpgenc_probe_time;
			v4l2_info(&jpeg->v4l2_dev, "device numbers: %d\n", jpgenc_probe_time);
		}
	return 0;
}

static int vidioc_jpeg_enc_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_jpeg_ctx *ctx = ctrl_to_ctx(ctrl);

	switch (ctrl->id) {
	case V4L2_CID_JPEG_RESTART_INTERVAL:
		ctx->restart_interval = ctrl->val;
		break;
	case V4L2_CID_JPEG_COMPRESSION_QUALITY:
		ctx->enc_quality = ctrl->val;
		break;
	case V4L2_CID_JPEG_ACTIVE_MARKER:
		ctx->enable_exif = ctrl->val & V4L2_JPEG_ACTIVE_MARKER_APP1;
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops mtk_jpeg_enc_ctrl_ops = {
	.s_ctrl = vidioc_jpeg_enc_s_ctrl,
};

static int mtk_jpeg_enc_ctrls_setup(struct mtk_jpeg_ctx *ctx)
{
	const struct v4l2_ctrl_ops *ops = &mtk_jpeg_enc_ctrl_ops;
	struct v4l2_ctrl_handler *handler = &ctx->ctrl_hdl;

	v4l2_ctrl_handler_init(handler, 3);

	v4l2_ctrl_new_std(handler, ops, V4L2_CID_JPEG_RESTART_INTERVAL, 0, 100,
			  1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_JPEG_COMPRESSION_QUALITY, 0,
			  100, 1, 90);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_JPEG_ACTIVE_MARKER, 0,
			  V4L2_JPEG_ACTIVE_MARKER_APP1, 0, 0);

	if (handler->error) {
		v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
		return handler->error;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);

	return 0;
}

static void v4l_fill_mtk_fmtdesc(struct v4l2_fmtdesc *fmt)
{
	const char *descr = NULL;

	if (fmt == NULL) {
		JPEG_LOG(0, "error, fmt is NULL\n");
		return;
	}

	switch (fmt->pixelformat) {
	case V4L2_PIX_FMT_NV12_AFBC:
		descr = "MCN8"; break;
	default:
		JPEG_LOG(2, "error pixelformat 0x%08x\n", fmt->pixelformat);
		break;
	}

	if (descr)
		WARN_ON(strscpy(fmt->description, descr, sizeof(fmt->description)) < 0);
}

static int mtk_jpeg_enum_fmt(struct mtk_jpeg_fmt *mtk_jpeg_formats, int n,
			     struct v4l2_fmtdesc *f, u32 type)
{
	int i, num = 0;

	for (i = 0; i < n; ++i) {
		if (mtk_jpeg_formats[i].flags & type) {
			if (num == f->index)
				break;
			++num;
		}
	}

	if (i >= n)
		return -EINVAL;

	f->pixelformat = mtk_jpeg_formats[i].fourcc;
	memset(f->reserved, 0, sizeof(f->reserved));
	v4l_fill_mtk_fmtdesc(f);

	return 0;
}

static int mtk_jpeg_enum_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_fmtdesc *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	return mtk_jpeg_enum_fmt(jpeg->variant->formats,
				 jpeg->variant->num_formats, f,
				 MTK_JPEG_FMT_FLAG_CAPTURE);
}

static int mtk_jpeg_enum_fmt_vid_out(struct file *file, void *priv,
				     struct v4l2_fmtdesc *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	if (strcmp((const char *)jpeg->variant->dev_name, "mtk-jpeg-dec") == 0)
		f->flags = V4L2_FMT_FLAG_DYN_RESOLUTION;

	return mtk_jpeg_enum_fmt(jpeg->variant->formats,
				 jpeg->variant->num_formats, f,
				 MTK_JPEG_FMT_FLAG_OUTPUT);
}

static struct mtk_jpeg_q_data *mtk_jpeg_get_q_data(struct mtk_jpeg_ctx *ctx,
						   enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->out_q;
	return &ctx->cap_q;
}

static struct mtk_jpeg_fmt *
mtk_jpeg_find_format(struct mtk_jpeg_fmt *mtk_jpeg_formats, int num_formats,
		     u32 pixelformat, unsigned int fmt_type)
{
	unsigned int k;
	struct mtk_jpeg_fmt *fmt;

	for (k = 0; k < num_formats; k++) {
		fmt = &mtk_jpeg_formats[k];

		if (fmt->fourcc == pixelformat && fmt->flags & fmt_type)
			return fmt;
	}

	return NULL;
}

static int mtk_jpeg_try_fmt_mplane(struct v4l2_pix_format_mplane *pix_mp,
				   struct mtk_jpeg_fmt *fmt)
{
	int i;

	pix_mp->field = V4L2_FIELD_NONE;

	pix_mp->num_planes = fmt->colplanes;
	pix_mp->pixelformat = fmt->fourcc;

	if (fmt->fourcc == V4L2_PIX_FMT_JPEG) {
		struct v4l2_plane_pix_format *pfmt = &pix_mp->plane_fmt[0];

		pix_mp->height = clamp(pix_mp->height, MTK_JPEG_MIN_HEIGHT,
				       MTK_JPEG_MAX_HEIGHT);
		pix_mp->width = clamp(pix_mp->width, MTK_JPEG_MIN_WIDTH,
				      MTK_JPEG_MAX_WIDTH);

		pfmt->bytesperline = 0;
		/* Source size must be aligned to 128 */
		pfmt->sizeimage = round_up(pfmt->sizeimage, 128);
		if (pfmt->sizeimage == 0)
			pfmt->sizeimage = MTK_JPEG_DEFAULT_SIZEIMAGE;
		return 0;
	}

	/* other fourcc */
	pix_mp->height = clamp(round_up(pix_mp->height, fmt->v_align),
			       MTK_JPEG_MIN_HEIGHT, MTK_JPEG_MAX_HEIGHT);
	pix_mp->width = clamp(round_up(pix_mp->width, fmt->h_align),
			      MTK_JPEG_MIN_WIDTH, MTK_JPEG_MAX_WIDTH);

	for (i = 0; i < fmt->colplanes; i++) {
		struct v4l2_plane_pix_format *pfmt = &pix_mp->plane_fmt[i];
		u32 stride = pix_mp->width * fmt->h_sample[i] / 4;
		u32 h = pix_mp->height * fmt->v_sample[i] / 4;

		if (pix_mp->pixelformat == V4L2_PIX_FMT_NV12_AFBC) {
			unsigned int block_w = 16;
			unsigned int block_h = 16;
			unsigned int bitsPP = 12;
			unsigned int width = round_up(pix_mp->width, 32);
			unsigned int height = round_up(pix_mp->height, 32);
			unsigned int block_count =
					((width + (block_w - 1))/block_w)
					*((height + (block_h - 1))/block_h);
			pfmt->bytesperline = stride;
			pfmt->sizeimage = round_up((block_count << 4), 1024) +
					(block_count * round_up((block_w * block_h  * bitsPP / 8), 128));
		} else if (pix_mp->pixelformat == V4L2_PIX_FMT_YUYV ||
			pix_mp->pixelformat == V4L2_PIX_FMT_YVYU) {
			stride = round_up(pix_mp->width * 2, 32);
			pfmt->bytesperline = stride;
			pfmt->sizeimage = stride * h;
		} else {
			pfmt->bytesperline = stride;
			pfmt->sizeimage = stride * h;
		}
	}
	return 0;
}

static int mtk_jpeg_g_fmt_vid_mplane(struct file *file, void *priv,
				     struct v4l2_format *f)
{
	struct vb2_queue *vq;
	struct mtk_jpeg_q_data *q_data = NULL;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	int i;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	q_data = mtk_jpeg_get_q_data(ctx, f->type);

	pix_mp->width = q_data->pix_mp.width;
	pix_mp->height = q_data->pix_mp.height;
	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->pixelformat = q_data->fmt->fourcc;
	pix_mp->num_planes = q_data->fmt->colplanes;
	pix_mp->colorspace = q_data->pix_mp.colorspace;
	pix_mp->ycbcr_enc = q_data->pix_mp.ycbcr_enc;
	pix_mp->xfer_func = q_data->pix_mp.xfer_func;
	pix_mp->quantization = q_data->pix_mp.quantization;

	v4l2_dbg(1, debug, &jpeg->v4l2_dev, "(%d) g_fmt:%c%c%c%c wxh:%ux%u\n",
		 f->type,
		 (pix_mp->pixelformat & 0xff),
		 (pix_mp->pixelformat >>  8 & 0xff),
		 (pix_mp->pixelformat >> 16 & 0xff),
		 (pix_mp->pixelformat >> 24 & 0xff),
		 pix_mp->width, pix_mp->height);

	for (i = 0; i < pix_mp->num_planes; i++) {
		struct v4l2_plane_pix_format *pfmt = &pix_mp->plane_fmt[i];

		pfmt->bytesperline = q_data->pix_mp.plane_fmt[i].bytesperline;
		pfmt->sizeimage = q_data->pix_mp.plane_fmt[i].sizeimage;

		v4l2_dbg(1, debug, &jpeg->v4l2_dev,
			 "plane[%d] bpl=%u, size=%u\n",
			 i,
			 pfmt->bytesperline,
			 pfmt->sizeimage);
	}
	return 0;
}

static int mtk_jpeg_try_fmt_vid_cap_mplane(struct file *file, void *priv,
					   struct v4l2_format *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct mtk_jpeg_fmt *fmt;

	fmt = mtk_jpeg_find_format(jpeg->variant->formats,
				   jpeg->variant->num_formats,
				   f->fmt.pix_mp.pixelformat,
				   MTK_JPEG_FMT_FLAG_CAPTURE);
	if (!fmt)
		fmt = ctx->cap_q.fmt;

	v4l2_dbg(2, debug, &ctx->jpeg->v4l2_dev, "(%d) try_fmt:%c%c%c%c\n",
		 f->type,
		 (fmt->fourcc & 0xff),
		 (fmt->fourcc >>  8 & 0xff),
		 (fmt->fourcc >> 16 & 0xff),
		 (fmt->fourcc >> 24 & 0xff));

	if (ctx->state != MTK_JPEG_INIT) {
		mtk_jpeg_g_fmt_vid_mplane(file, priv, f);
		return 0;
	}

	return mtk_jpeg_try_fmt_mplane(&f->fmt.pix_mp, fmt);
}

static int mtk_jpeg_try_fmt_vid_out_mplane(struct file *file, void *priv,
					   struct v4l2_format *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct mtk_jpeg_fmt *fmt;

	fmt = mtk_jpeg_find_format(jpeg->variant->formats,
				   jpeg->variant->num_formats,
				   f->fmt.pix_mp.pixelformat,
				   MTK_JPEG_FMT_FLAG_OUTPUT);
	if (!fmt)
		fmt = ctx->out_q.fmt;

	v4l2_dbg(2, debug, &ctx->jpeg->v4l2_dev, "(%d) try_fmt:%c%c%c%c\n",
		 f->type,
		 (fmt->fourcc & 0xff),
		 (fmt->fourcc >>  8 & 0xff),
		 (fmt->fourcc >> 16 & 0xff),
		 (fmt->fourcc >> 24 & 0xff));

	if (ctx->state != MTK_JPEG_INIT) {
		mtk_jpeg_g_fmt_vid_mplane(file, priv, f);
		return 0;
	}

	return mtk_jpeg_try_fmt_mplane(&f->fmt.pix_mp, fmt);
}

static int mtk_jpeg_s_fmt_mplane(struct mtk_jpeg_ctx *ctx,
				 struct v4l2_format *f, unsigned int fmt_type)
{
	struct vb2_queue *vq;
	struct mtk_jpeg_q_data *q_data = NULL;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	int i;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	q_data = mtk_jpeg_get_q_data(ctx, f->type);

	if (vb2_is_busy(vq)) {
		v4l2_err(&jpeg->v4l2_dev, "queue busy\n");
		return -EBUSY;
	}

	q_data->fmt = mtk_jpeg_find_format(jpeg->variant->formats,
					   jpeg->variant->num_formats,
					   pix_mp->pixelformat, fmt_type);
	if (q_data->fmt == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! q_data->fmt is NULL\n");
		return -EINVAL;
	}
	q_data->pix_mp.width = pix_mp->width;
	q_data->pix_mp.height = pix_mp->height;
	q_data->enc_crop_rect.width = pix_mp->width;
	q_data->enc_crop_rect.height = pix_mp->height;
	q_data->pix_mp.colorspace = V4L2_COLORSPACE_SRGB;
	q_data->pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_601;
	q_data->pix_mp.xfer_func = V4L2_XFER_FUNC_SRGB;
	q_data->pix_mp.quantization = V4L2_QUANTIZATION_FULL_RANGE;

	v4l2_dbg(1, debug, &jpeg->v4l2_dev, "(%d) s_fmt:%c%c%c%c wxh:%ux%u\n",
		 f->type,
		 (q_data->fmt->fourcc & 0xff),
		 (q_data->fmt->fourcc >>  8 & 0xff),
		 (q_data->fmt->fourcc >> 16 & 0xff),
		 (q_data->fmt->fourcc >> 24 & 0xff),
		 q_data->pix_mp.width, q_data->pix_mp.height);

	for (i = 0; i < q_data->fmt->colplanes; i++) {
		q_data->pix_mp.plane_fmt[i].bytesperline =
					pix_mp->plane_fmt[i].bytesperline;
		q_data->pix_mp.plane_fmt[i].sizeimage =
					pix_mp->plane_fmt[i].sizeimage;

		v4l2_dbg(1, debug, &jpeg->v4l2_dev,
			 "plane[%d] bpl=%u, size=%u\n",
			 i, q_data->pix_mp.plane_fmt[i].bytesperline,
			 q_data->pix_mp.plane_fmt[i].sizeimage);
	}

	return 0;
}

static int vidioc_enum_framesizes(struct file *file, void *priv,
				 struct v4l2_frmsizeenum *fsize)
{
	fsize->type = V4L2_FRMSIZE_TYPE_CONTINUOUS;
	fsize->stepwise.min_width = MTK_JPEG_MIN_WIDTH;
	fsize->stepwise.max_width = MTK_JPEG_DEFAULT_WIDTH;
	fsize->stepwise.min_height = MTK_JPEG_MIN_HEIGHT;
	fsize->stepwise.max_height = MTK_JPEG_DEFAULT_HEIGHT;
	fsize->stepwise.step_width = 1;
	fsize->stepwise.step_height = 1;

	return 0;
}

static int mtk_jpeg_s_fmt_vid_out_mplane(struct file *file, void *priv,
					 struct v4l2_format *f)
{
	int ret;

	ret = mtk_jpeg_try_fmt_vid_out_mplane(file, priv, f);
	if (ret)
		return ret;

	return mtk_jpeg_s_fmt_mplane(mtk_jpeg_fh_to_ctx(priv), f,
				     MTK_JPEG_FMT_FLAG_OUTPUT);
}

static int mtk_jpeg_s_fmt_vid_cap_mplane(struct file *file, void *priv,
					 struct v4l2_format *f)
{
	int ret;

	ret = mtk_jpeg_try_fmt_vid_cap_mplane(file, priv, f);
	if (ret)
		return ret;

	return mtk_jpeg_s_fmt_mplane(mtk_jpeg_fh_to_ctx(priv), f,
				     MTK_JPEG_FMT_FLAG_CAPTURE);
}

static void mtk_jpeg_queue_src_chg_event(struct mtk_jpeg_ctx *ctx)
{
	static const struct v4l2_event ev_src_ch = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes =
		V4L2_EVENT_SRC_CH_RESOLUTION,
	};

	v4l2_event_queue_fh(&ctx->fh, &ev_src_ch);
}

static int mtk_jpeg_subscribe_event(struct v4l2_fh *fh,
				    const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	}

	return v4l2_ctrl_subscribe_event(fh, sub);
}

static int mtk_jpeg_enc_g_selection(struct file *file, void *priv,
				    struct v4l2_selection *s)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);

	if (s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_CROP:
		s->r = ctx->out_q.enc_crop_rect;
		break;
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		s->r.width = ctx->out_q.pix_mp.width;
		s->r.height = ctx->out_q.pix_mp.height;
		s->r.left = 0;
		s->r.top = 0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int mtk_jpeg_dec_g_selection(struct file *file, void *priv,
				    struct v4l2_selection *s)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
		s->r.width = ctx->out_q.pix_mp.width;
		s->r.height = ctx->out_q.pix_mp.height;
		s->r.left = 0;
		s->r.top = 0;
		break;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_COMPOSE_PADDED:
		s->r.width = ctx->cap_q.pix_mp.width;
		s->r.height = ctx->cap_q.pix_mp.height;
		s->r.left = 0;
		s->r.top = 0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int mtk_jpeg_enc_s_selection(struct file *file, void *priv,
				    struct v4l2_selection *s)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);

	if (s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_CROP:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = min(s->r.width, ctx->out_q.pix_mp.width);
		s->r.height = min(s->r.height, ctx->out_q.pix_mp.height);
		ctx->out_q.enc_crop_rect = s->r;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#if MTK_JPEG_DVFS_BW_SUPPORT
static void mtk_jpeg_prepare_dvfs(struct mtk_jpeg_dev *jpeg)
{

	int ret = 0;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0;

	pr_info("prepare dvfs +\n");
	ret = dev_pm_opp_of_add_table(jpeg->dev);
	if (ret < 0) {
		pr_info("Failed to get opp table (%d)\n", ret);
		return;
	}

	jpeg->jpegenc_reg = devm_regulator_get_optional(jpeg->dev,
						"mmdvfs-dvfsrc-vcore");
	if (IS_ERR_OR_NULL(jpeg->jpegenc_reg)) {
		pr_info("Failed to get regulator\n");
		jpeg->jpegenc_reg = NULL;
		jpeg->jpegenc_mmdvfs_clk = devm_clk_get(jpeg->dev,
							"mmdvfs_clk");
		if (IS_ERR_OR_NULL(jpeg->jpegenc_mmdvfs_clk)) {
			pr_info("Failed to get mmdvfs clk\n");
			jpeg->jpegenc_mmdvfs_clk = NULL;
			return;
		}
	}

	jpeg->freq_cnt = dev_pm_opp_get_opp_count(jpeg->dev);
	freq = 0;
	pr_info("jpeg->freq_cnt %d\n", jpeg->freq_cnt);
	while (!IS_ERR(opp =
		dev_pm_opp_find_freq_ceil(jpeg->dev, &freq))) {
		jpeg->freqs[i] = freq;
	pr_info("i %d freq %lu\n", i, freq);
	freq++;
		i++;
		dev_pm_opp_put(opp);
	}
	pr_info("prepare dvfs -\n");

}

static void mtk_jpeg_prepare_bw_request(struct mtk_jpeg_dev *jpeg)
{
	pr_info("prepare bw request +");
	jpeg->path_y_rdma = of_mtk_icc_get(jpeg->dev, "path_jpegenc_y_rdma");
	pr_info("jpeg->path_y_rdma 0x%lx", (unsigned long)jpeg->path_y_rdma);
	jpeg->path_c_rdma = of_mtk_icc_get(jpeg->dev, "path_jpegenc_c_rmda");
	pr_info("jpeg->path_c_rdma 0x%lx", (unsigned long)jpeg->path_c_rdma);
	jpeg->path_qtbl = of_mtk_icc_get(jpeg->dev, "path_jpegenc_q_table");
	pr_info("jpeg->path_qtbl 0x%lx", (unsigned long)jpeg->path_qtbl);
	jpeg->path_bsdma = of_mtk_icc_get(jpeg->dev, "path_jpegenc_bsdma");
	pr_info("jpeg->path_bsdma 0x%lx", (unsigned long)jpeg->path_bsdma);
	pr_info("prepare bw request -");

}

static void mtk_jpeg_update_bw_request(struct mtk_jpeg_ctx *ctx)
{
	int ret;
	u32 port_num = 0;
	/* limiting FPS, Upper Bound FPS = 20 */
	unsigned int target_fps = 30;
	unsigned int cshot_spec = 0xffffffff;

	/* Support QoS */
	unsigned int emi_bw = 0;
	unsigned int picSize = 0;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	ret = of_property_read_u32(jpeg->dev->of_node, "interconnect-num", &port_num);
	pr_info("%s  ret: %d\n", __func__, ret);
	if (ret >= 0)
		pr_info("%s  port_num: %u\n", __func__, port_num);
	if (port_num == 1) {
		picSize = (ctx->out_q.pix_mp.width * ctx->out_q.pix_mp.height) / 1000000;
		ret = of_property_read_u32(jpeg->dev->of_node, "cshot-spec", &cshot_spec);
		if (ret >= 0)
			pr_info("%s  cshot_spec ret: %d, cshot_spec : %d\n",
		    __func__, ret, cshot_spec);
		if (ctx->out_q.fmt->fourcc == V4L2_PIX_FMT_YUYV ||
			ctx->out_q.fmt->fourcc == V4L2_PIX_FMT_YVYU)
			picSize = ((picSize * 2) * 8/5) + 1;
		else
			picSize = ((picSize * 3/2) * 8/5) + 1;
		if (cshot_spec == 232 || cshot_spec == 26) {
			if ((picSize * 20) < cshot_spec) {
				emi_bw = picSize * 20;
			} else {
				emi_bw = cshot_spec / picSize;
				emi_bw = (emi_bw + 1) * picSize;
			}
		} else {
			emi_bw = picSize * 20;
		}
		emi_bw = emi_bw * 4/3;
		mtk_icc_set_bw(jpeg->path_bsdma, MBps_to_icc(emi_bw), MBps_to_icc(emi_bw));
		pr_info("port_num == 1 Width %d Height %d emi_bw %d\n",
		ctx->out_q.pix_mp.width, ctx->out_q.pix_mp.height, emi_bw);
	} else {
		picSize = (ctx->out_q.pix_mp.width * ctx->out_q.pix_mp.height) / 1000;
		emi_bw = picSize * target_fps;
		pr_info("Width %d Height %d emi_bw %d\n",
		ctx->out_q.pix_mp.width, ctx->out_q.pix_mp.height, emi_bw);
		if (ctx->out_q.fmt->fourcc == V4L2_PIX_FMT_YUYV ||
			ctx->out_q.fmt->fourcc == V4L2_PIX_FMT_YVYU) {
			mtk_icc_set_bw(jpeg->path_y_rdma, kBps_to_icc(emi_bw*2), 0);
			mtk_icc_set_bw(jpeg->path_c_rdma, kBps_to_icc(emi_bw), 0);
		} else {
			mtk_icc_set_bw(jpeg->path_y_rdma, kBps_to_icc(emi_bw), 0);
			mtk_icc_set_bw(jpeg->path_c_rdma,
				kBps_to_icc(emi_bw * 1/2), 0);
		}
		mtk_icc_set_bw(jpeg->path_qtbl, kBps_to_icc(emi_bw), 0);
		mtk_icc_set_bw(jpeg->path_bsdma, kBps_to_icc(emi_bw), 0);
	}
}

static void mtk_jpeg_end_bw_request(struct mtk_jpeg_ctx *ctx)
{
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	mtk_icc_set_bw(jpeg->path_y_rdma, 0, 0);
	mtk_icc_set_bw(jpeg->path_c_rdma, 0, 0);
	mtk_icc_set_bw(jpeg->path_qtbl, 0, 0);
	mtk_icc_set_bw(jpeg->path_bsdma, 0, 0);
}

static void mtk_jpeg_dvfs_begin(struct mtk_jpeg_ctx *ctx)
{
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;
	unsigned long active_freq = 0;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	pr_info("%s freq_cnt %d!\n", __func__, jpeg->freq_cnt);
	if (jpeg->freq_cnt > 0 && jpeg->freq_cnt < MTK_JPEG_MAX_FREQ)
		active_freq = jpeg->freqs[jpeg->freq_cnt - 1];

	if (jpeg->jpegenc_reg) {
		opp = dev_pm_opp_find_freq_ceil(jpeg->dev,
					&active_freq);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(jpeg->jpegenc_reg, volt, INT_MAX);
		if (ret) {
			pr_info("%s Failed to set regulator voltage %d\n",
				 __func__, volt);
		}
		pr_info("%s  volt: %d\n", __func__, volt);
	} else if (jpeg->jpegenc_mmdvfs_clk) {
		pr_info("%s set mmdvfs clk\n", __func__);
		if (mmdvfs_get_version())
			mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_JPEGENC);
		ret = clk_set_rate(jpeg->jpegenc_mmdvfs_clk, active_freq);
		if (ret) {
			pr_info("%s Failed to set freq %lu\n",
				 __func__, active_freq);
		}
		pr_info("%s  freq: %lu\n", __func__, active_freq);
		if (mmdvfs_get_version())
			mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_JPEGENC);
	}
}

static void mtk_jpeg_dvfs_end(struct mtk_jpeg_ctx *ctx)
{
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;
	unsigned long active_freq = 0;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	pr_info("%s  ++\n", __func__);
	active_freq = jpeg->freqs[0];

	if (jpeg->jpegenc_reg) {
		opp = dev_pm_opp_find_freq_ceil(jpeg->dev,
					&active_freq);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		ret = regulator_set_voltage(jpeg->jpegenc_reg, volt, INT_MAX);
		if (ret) {
			pr_info("%s Failed to set regulator voltage %d\n",
				 __func__, volt);
		}
		pr_info("%s  volt: %d --\n", __func__, volt);
	} else if (jpeg->jpegenc_mmdvfs_clk) {
		ret = clk_set_rate(jpeg->jpegenc_mmdvfs_clk, active_freq);
		if (ret) {
			pr_info("%s Failed to set freq %lu\n",
				 __func__, active_freq);
		}
		pr_info("%s  freq: %lu\n", __func__, active_freq);
	}
}
#endif

static int mtk_jpeg_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct v4l2_fh *fh;
	struct vb2_queue *vq;
	struct vb2_buffer *vb;
	struct mtk_jpeg_src_buf *jpeg_src_buf;
	struct mtk_jpeg_ctx *ctx;
	struct mtk_jpeg_dev *jpeg;

	if (IS_ERR_OR_NULL(file) || IS_ERR_OR_NULL(priv) || IS_ERR_OR_NULL(buf)) {
		pr_info("%s %d qbuf error, invalid input param\n", __func__, __LINE__);
		return -EINVAL;
	}

	fh = file->private_data;
	ctx = mtk_jpeg_fh_to_ctx(priv);
	if (IS_ERR_OR_NULL(ctx) || IS_ERR_OR_NULL(fh)) {
		pr_info("%s %d invalid parameter\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(buf->m.planes) || (buf->length <= 0)) {
		pr_info("%s %d invalid buffer parameter\n", __func__, __LINE__);
		return -EINVAL;
	}

	jpeg = ctx->jpeg;
	if (IS_ERR_OR_NULL(jpeg)) {
		pr_info("%s %d invalid ctx->jpeg\n", __func__, __LINE__);
		return -EINVAL;
	}

	vq = v4l2_m2m_get_vq(fh->m2m_ctx, buf->type);
	if (buf->index >= vq->num_buffers) {
		pr_info("%s %d buf num is over %d\n", __func__, __LINE__, buf->index);
		return -EINVAL;
	}

	if (!V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
		pr_info("%s %d buf type is not multiplanar\n", __func__, __LINE__);
		goto end;
	}

	if (strcmp((const char *)jpeg->variant->dev_name, "mtk-jpeg-dec") == 0) {
		vb = vq->bufs[buf->index];
		jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(vb);
		if (IS_ERR_OR_NULL(jpeg_src_buf))
			pr_info("%s %d jpeg_src_buf null\n", __func__, __LINE__);
		else
			jpeg_src_buf->bs_size = buf->m.planes[0].bytesused;
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ctx->dst_offset = buf->m.planes[0].data_offset;
	}

end:
	return v4l2_m2m_qbuf(file, fh->m2m_ctx, buf);
}

static int mtk_jpeg_decoder_cmd(struct file *file, void *priv,
				struct v4l2_decoder_cmd *cmd)
{
	struct v4l2_fh *fh = file->private_data;
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(fh);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	int ret;
	unsigned long flags;

	ret = v4l2_m2m_ioctl_try_decoder_cmd(file, fh, cmd);
	if (ret < 0)
		return ret;

	v4l2_dbg(1, debug, &jpeg->v4l2_dev, "dec cmd=%d\n", cmd->cmd);

	if (cmd->cmd == V4L2_DEC_CMD_STOP) {
		if (!vb2_is_streaming(v4l2_m2m_get_src_vq(fh->m2m_ctx))) {
			v4l2_dbg(0, debug, &jpeg->v4l2_dev, "Out stream off\n");
			return 0;
		}

		spin_lock_irqsave(&ctx->jpeg->hw_lock, flags);
		ret = v4l2_m2m_ioctl_decoder_cmd(file, priv, cmd);
		spin_unlock_irqrestore(&ctx->jpeg->hw_lock, flags);
		if (ret < 0) {
			v4l2_err(&jpeg->v4l2_dev, "dec cmd fail %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int mtk_jpeg_encoder_cmd(struct file *file, void *priv,
				struct v4l2_encoder_cmd *cmd)
{
	struct v4l2_fh *fh = file->private_data;
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(fh);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	int ret;
	unsigned long flags;

	ret = v4l2_m2m_ioctl_try_encoder_cmd(file, fh, cmd);
	if (ret < 0)
		return ret;

	if (cmd->cmd == V4L2_ENC_CMD_STOP) {
		if (!vb2_is_streaming(v4l2_m2m_get_src_vq(fh->m2m_ctx)) ||
		    !vb2_is_streaming(v4l2_m2m_get_dst_vq(fh->m2m_ctx))) {
			v4l2_dbg(0, debug, &jpeg->v4l2_dev, "Out or Cap stream off\n");
			return 0;
		}

		v4l2_dbg(0, debug, &jpeg->v4l2_dev, "V4L2_ENC_CMD_STOP\n");

		spin_lock_irqsave(&ctx->jpeg->hw_lock, flags);
		ret = v4l2_m2m_ioctl_encoder_cmd(file, priv, cmd);
		spin_unlock_irqrestore(&ctx->jpeg->hw_lock, flags);
		if (ret < 0) {
			v4l2_err(&jpeg->v4l2_dev, "enc cmd fail %d\n", ret);
			return 0;
		}
	}

	return 0;
}
static const struct v4l2_ioctl_ops mtk_jpeg_enc_ioctl_ops = {
	.vidioc_querycap                = mtk_jpeg_querycap,
	.vidioc_enum_fmt_vid_cap	= mtk_jpeg_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_out	= mtk_jpeg_enum_fmt_vid_out,
	.vidioc_enum_framesizes         = vidioc_enum_framesizes,
	.vidioc_try_fmt_vid_cap_mplane	= mtk_jpeg_try_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_out_mplane	= mtk_jpeg_try_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_cap_mplane    = mtk_jpeg_g_fmt_vid_mplane,
	.vidioc_g_fmt_vid_out_mplane    = mtk_jpeg_g_fmt_vid_mplane,
	.vidioc_s_fmt_vid_cap_mplane    = mtk_jpeg_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_out_mplane    = mtk_jpeg_s_fmt_vid_out_mplane,
	.vidioc_qbuf                    = mtk_jpeg_qbuf,
	.vidioc_subscribe_event         = mtk_jpeg_subscribe_event,
	.vidioc_g_selection		= mtk_jpeg_enc_g_selection,
	.vidioc_s_selection		= mtk_jpeg_enc_s_selection,

	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf		= v4l2_m2m_ioctl_prepare_buf,
	.vidioc_reqbufs                 = v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf                = v4l2_m2m_ioctl_querybuf,
	.vidioc_dqbuf                   = v4l2_m2m_ioctl_dqbuf,
	.vidioc_expbuf                  = v4l2_m2m_ioctl_expbuf,
	.vidioc_streamon                = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff               = v4l2_m2m_ioctl_streamoff,

	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,

	.vidioc_try_encoder_cmd		= v4l2_m2m_ioctl_try_encoder_cmd,
	.vidioc_encoder_cmd		= mtk_jpeg_encoder_cmd,
};

static const struct v4l2_ioctl_ops mtk_jpeg_dec_ioctl_ops = {
	.vidioc_querycap                = mtk_jpeg_querycap,
	.vidioc_enum_fmt_vid_cap	= mtk_jpeg_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_out	= mtk_jpeg_enum_fmt_vid_out,
	.vidioc_enum_framesizes         = vidioc_enum_framesizes,
	.vidioc_try_fmt_vid_cap_mplane	= mtk_jpeg_try_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_out_mplane	= mtk_jpeg_try_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_cap_mplane    = mtk_jpeg_g_fmt_vid_mplane,
	.vidioc_g_fmt_vid_out_mplane    = mtk_jpeg_g_fmt_vid_mplane,
	.vidioc_s_fmt_vid_cap_mplane    = mtk_jpeg_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_out_mplane    = mtk_jpeg_s_fmt_vid_out_mplane,
	.vidioc_qbuf                    = mtk_jpeg_qbuf,
	.vidioc_subscribe_event         = mtk_jpeg_subscribe_event,
	.vidioc_g_selection		= mtk_jpeg_dec_g_selection,

	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf		= v4l2_m2m_ioctl_prepare_buf,
	.vidioc_reqbufs                 = v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf                = v4l2_m2m_ioctl_querybuf,
	.vidioc_dqbuf                   = v4l2_m2m_ioctl_dqbuf,
	.vidioc_expbuf                  = v4l2_m2m_ioctl_expbuf,
	.vidioc_streamon                = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff               = v4l2_m2m_ioctl_streamoff,

	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,

	.vidioc_try_decoder_cmd		= v4l2_m2m_ioctl_try_decoder_cmd,
	.vidioc_decoder_cmd		= mtk_jpeg_decoder_cmd,
};

static int mtk_jpeg_queue_setup(struct vb2_queue *q,
				unsigned int *num_buffers,
				unsigned int *num_planes,
				unsigned int sizes[],
				struct device *alloc_ctxs[])
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(q);
	struct mtk_jpeg_q_data *q_data = NULL;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	int i;

	v4l2_dbg(1, debug, &jpeg->v4l2_dev, "(%d) buf_req count=%u\n",
		 q->type, *num_buffers);

	q_data = mtk_jpeg_get_q_data(ctx, q->type);
	if (!q_data)
		return -EINVAL;

	if (*num_planes) {
		for (i = 0; i < *num_planes; i++)
			if (sizes[i] < q_data->pix_mp.plane_fmt[i].sizeimage)
				return -EINVAL;
		return 0;
	}

	*num_planes = q_data->fmt->colplanes;
	for (i = 0; i < q_data->fmt->colplanes; i++) {
		sizes[i] =  q_data->pix_mp.plane_fmt[i].sizeimage;
		v4l2_dbg(1, debug, &jpeg->v4l2_dev, "sizeimage[%d]=%u\n",
			 i, sizes[i]);
	}

	return 0;
}

static int mtk_jpeg_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_jpeg_q_data *q_data = NULL;
	struct v4l2_plane_pix_format plane_fmt = {};
	int i;

	q_data = mtk_jpeg_get_q_data(ctx, vb->vb2_queue->type);
	if (!q_data)
		return -EINVAL;


	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		for (i = 0; i < q_data->fmt->colplanes; i++) {
			plane_fmt = q_data->pix_mp.plane_fmt[i];
			vb2_set_plane_payload(vb, i,  plane_fmt.sizeimage);
		}
	} else if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		for (i = 0; i < q_data->fmt->colplanes; i++)
			vb2_set_plane_payload(vb, i,  0);
	}

	return 0;
}

static void mtk_jpeg_buf_finish(struct vb2_buffer *vb)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct v4l2_m2m_ctx *m2m_ctx = ctx->fh.m2m_ctx;

	if (vb->vb2_queue->memory == VB2_MEMORY_DMABUF &&
		vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
		vb->planes[0].bytesused > 0 && !m2m_ctx->has_stopped) {
		if (vb->planes[0].dbuf == NULL)
			return;

		struct dma_buf_attachment *buf_att;
		struct sg_table *sgt;

		buf_att = dma_buf_attach(vb->planes[0].dbuf,
			ctx->jpeg->smmu_dev);

		sgt = dma_buf_map_attachment(buf_att, DMA_TO_DEVICE);
		dma_buf_begin_cpu_access_partial(vb->planes[0].dbuf,
			DMA_FROM_DEVICE, vb->planes[0].data_offset,
			vb->planes[0].bytesused);
		dma_buf_unmap_attachment(buf_att, sgt, DMA_TO_DEVICE);
		dma_buf_detach(vb->planes[0].dbuf, buf_att);
	}

}

static bool mtk_jpeg_check_resolution_change(struct mtk_jpeg_ctx *ctx,
					     struct mtk_jpeg_dec_param *param)
{
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct mtk_jpeg_q_data *q_data;

	q_data = &ctx->out_q;
	if (q_data->pix_mp.width != param->pic_w ||
	    q_data->pix_mp.height != param->pic_h) {
		v4l2_dbg(1, debug, &jpeg->v4l2_dev, "Picture size change\n");
		return true;
	}

	q_data = &ctx->cap_q;
	if (q_data->fmt !=
	    mtk_jpeg_find_format(jpeg->variant->formats,
				 jpeg->variant->num_formats, param->dst_fourcc,
				 MTK_JPEG_FMT_FLAG_CAPTURE)) {
		v4l2_dbg(1, debug, &jpeg->v4l2_dev, "format change\n");
		return true;
	}
	return false;
}

static void mtk_jpeg_set_queue_data(struct mtk_jpeg_ctx *ctx,
				    struct mtk_jpeg_dec_param *param)
{
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct mtk_jpeg_q_data *q_data;
	int i;

	q_data = &ctx->out_q;
	q_data->pix_mp.width = param->pic_w;
	q_data->pix_mp.height = param->pic_h;

	q_data = &ctx->cap_q;
	q_data->pix_mp.width = param->dec_w;
	q_data->pix_mp.height = param->dec_h;
	q_data->fmt = mtk_jpeg_find_format(jpeg->variant->formats,
					   jpeg->variant->num_formats,
					   param->dst_fourcc,
					   MTK_JPEG_FMT_FLAG_CAPTURE);
	if (q_data->fmt == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! q_data->fmt is NULL\n");
		return;
	}

	for (i = 0; i < q_data->fmt->colplanes; i++) {
		q_data->pix_mp.plane_fmt[i].bytesperline = param->mem_stride[i];
		q_data->pix_mp.plane_fmt[i].sizeimage = param->comp_size[i];
	}

	v4l2_dbg(1, debug, &jpeg->v4l2_dev,
		 "set_parse cap:%c%c%c%c pic(%u, %u), buf(%u, %u)\n",
		 (param->dst_fourcc & 0xff),
		 (param->dst_fourcc >>  8 & 0xff),
		 (param->dst_fourcc >> 16 & 0xff),
		 (param->dst_fourcc >> 24 & 0xff),
		 param->pic_w, param->pic_h,
		 param->dec_w, param->dec_h);
}

static void mtk_jpeg_enc_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	v4l2_dbg(2, debug, &jpeg->v4l2_dev, "(%d) buf_q id=%d, vb=%p\n",
		 vb->vb2_queue->type, vb->index, vb);

	if (V4L2_TYPE_IS_CAPTURE(vb->vb2_queue->type) &&
		vb2_is_streaming(vb->vb2_queue) &&
		v4l2_m2m_dst_buf_is_last(ctx->fh.m2m_ctx)) {

		vbuf->field = V4L2_FIELD_NONE;
		v4l2_m2m_last_buffer_done(ctx->fh.m2m_ctx, vbuf);
		v4l2_dbg(0, debug, &jpeg->v4l2_dev, "last cap buf done\n");
		return;
	}

	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, to_vb2_v4l2_buffer(vb));
}

static void mtk_jpeg_dec_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_jpeg_dec_param *param;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct mtk_jpeg_src_buf *jpeg_src_buf;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	bool header_valid;

	v4l2_dbg(2, debug, &jpeg->v4l2_dev, "(%d) buf_q id=%d, vb=%p\n",
		 vb->vb2_queue->type, vb->index, vb);

	if (V4L2_TYPE_IS_CAPTURE(vb->vb2_queue->type) &&
		vb2_is_streaming(vb->vb2_queue) &&
		(v4l2_m2m_dst_buf_is_last(ctx->fh.m2m_ctx)
		|| (ctx->fh.m2m_ctx->is_draining && ctx->early_eos))) {

		vbuf->field = V4L2_FIELD_NONE;
		v4l2_m2m_last_buffer_done(ctx->fh.m2m_ctx, vbuf);
		v4l2_dbg(0, debug, &jpeg->v4l2_dev, "last cap buf done\n");
		return;
	}

	if (vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		goto end;

	jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(vb);
	param = &jpeg_src_buf->dec_param;
	memset(param, 0, sizeof(*param));

	header_valid = mtk_jpeg_parse(param, (u8 *)vb2_plane_vaddr(vb, 0),
				      vb2_get_plane_payload(vb, 0));
	if (!header_valid) {
		v4l2_err(&jpeg->v4l2_dev, "Header invalid.\n");
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		return;
	}

	if (ctx->state == MTK_JPEG_INIT) {
		struct vb2_queue *dst_vq = v4l2_m2m_get_vq(
			ctx->fh.m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

		mtk_jpeg_queue_src_chg_event(ctx);
		mtk_jpeg_set_queue_data(ctx, param);
		ctx->state = vb2_is_streaming(dst_vq) ?
				MTK_JPEG_SOURCE_CHANGE : MTK_JPEG_RUNNING;
	}
end:
	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, to_vb2_v4l2_buffer(vb));
}

static struct vb2_v4l2_buffer *mtk_jpeg_buf_remove(struct mtk_jpeg_ctx *ctx,
				 enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	else
		return v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
}

static void mtk_jpeg_enc_stop_streaming(struct vb2_queue *q)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vb;

	while ((vb = mtk_jpeg_buf_remove(ctx, q->type)))
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);
}

static void mtk_jpeg_dec_stop_streaming(struct vb2_queue *q)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vb;

	/*
	 * STREAMOFF is an acknowledgment for source change event.
	 * Before STREAMOFF, we still have to return the old resolution and
	 * subsampling. Update capture queue when the stream is off.
	 */
	if (ctx->state == MTK_JPEG_SOURCE_CHANGE &&
	    V4L2_TYPE_IS_CAPTURE(q->type)) {
		struct mtk_jpeg_src_buf *src_buf;

		vb = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
		src_buf = mtk_jpeg_vb2_to_srcbuf(&vb->vb2_buf);
		mtk_jpeg_set_queue_data(ctx, &src_buf->dec_param);
		ctx->state = MTK_JPEG_RUNNING;
	} else if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		ctx->state = MTK_JPEG_INIT;
	}

	while ((vb = mtk_jpeg_buf_remove(ctx, q->type)))
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);
}

static const struct vb2_ops mtk_jpeg_dec_qops = {
	.queue_setup        = mtk_jpeg_queue_setup,
	.buf_prepare        = mtk_jpeg_buf_prepare,
	.buf_queue          = mtk_jpeg_dec_buf_queue,
	.wait_prepare       = vb2_ops_wait_prepare,
	.wait_finish        = vb2_ops_wait_finish,
	.buf_finish         = mtk_jpeg_buf_finish,
	.stop_streaming     = mtk_jpeg_dec_stop_streaming,
};

static const struct vb2_ops mtk_jpeg_enc_qops = {
	.queue_setup        = mtk_jpeg_queue_setup,
	.buf_prepare        = mtk_jpeg_buf_prepare,
	.buf_queue          = mtk_jpeg_enc_buf_queue,
	.wait_prepare       = vb2_ops_wait_prepare,
	.wait_finish        = vb2_ops_wait_finish,
	.buf_finish         = mtk_jpeg_buf_finish,
	.stop_streaming     = mtk_jpeg_enc_stop_streaming,
};

static void mtk_jpeg_set_dec_src(struct mtk_jpeg_ctx *ctx,
				 struct vb2_buffer *src_buf,
				 struct mtk_jpeg_bs *bs)
{
	bs->str_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);
	bs->end_addr = bs->str_addr +
		       round_up(vb2_get_plane_payload(src_buf, 0), 16);
	bs->size = round_up(vb2_plane_size(src_buf, 0), 128);
}

static int mtk_jpeg_set_dec_dst(struct mtk_jpeg_ctx *ctx,
				struct mtk_jpeg_dec_param *param,
				struct vb2_buffer *dst_buf,
				struct mtk_jpeg_fb *fb)
{
	int i;

	if (param->comp_num != dst_buf->num_planes) {
		dev_err(ctx->jpeg->dev, "plane number mismatch (%u != %u)\n",
			param->comp_num, dst_buf->num_planes);
		return -EINVAL;
	}

	for (i = 0; i < dst_buf->num_planes; i++) {
		if (vb2_plane_size(dst_buf, i) < param->comp_size[i]) {
			dev_err(ctx->jpeg->dev,
				"buffer size is underflow (%lu < %u)\n",
				vb2_plane_size(dst_buf, 0),
				param->comp_size[i]);
			return -EINVAL;
		}
		fb->plane_addr[i] = vb2_dma_contig_plane_dma_addr(dst_buf, i);
	}

	return 0;
}

static void mtk_jpeg_enc_device_run(void *priv)
{
	struct mtk_jpeg_ctx *ctx = priv;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	unsigned long flags;
	int ret;

	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
	if (src_buf == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! src_buf is NULL\n");
		return;
	}
	if (dst_buf == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! dst_buf is NULL\n");
		return;
	}
	dst_buf->vb2_buf.timestamp = src_buf->vb2_buf.timestamp;

	ret = pm_runtime_get_sync(jpeg->dev);
	if (ret < 0)
		goto enc_end;

#if MTK_JPEG_DVFS_BW_SUPPORT
	mtk_jpeg_update_bw_request(ctx);
	mtk_jpeg_dvfs_begin(ctx);
#endif

	schedule_delayed_work(&jpeg->job_timeout_work,
			      msecs_to_jiffies(MTK_JPEG_HW_TIMEOUT_MSEC));

	spin_lock_irqsave(&jpeg->hw_lock, flags);

	/*
	 * Resetting the hardware every frame is to ensure that all the
	 * registers are cleared. This is a hardware requirement.
	 */
	if (jpeg->gcon_base != NULL && jpeg->support_34bits == MTK_JPEG_FAKE_34BITS)
		mtk_jpeg_enc_set_34bits(ctx, jpeg->gcon_base, &dst_buf->vb2_buf);
	mtk_jpeg_enc_reset(jpeg->reg_base);
	mtk_jpeg_set_enc_src(ctx, jpeg->reg_base, &src_buf->vb2_buf);
	mtk_jpeg_set_enc_dst(ctx, jpeg->reg_base, &dst_buf->vb2_buf);
	mtk_jpeg_set_enc_params(ctx, jpeg->reg_base);
	ctx->time_start = jiffies_to_nsecs(jiffies);
	mtk_jpeg_enc_start(jpeg->reg_base);
	ctx->state = MTK_JPEG_RUNNING;
	spin_unlock_irqrestore(&jpeg->hw_lock, flags);
	return;

enc_end:
	v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_buf_done(src_buf, buf_state);
	v4l2_m2m_buf_done(dst_buf, buf_state);
	v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
}

static void mtk_jpeg_dec_device_run(void *priv)
{
	struct mtk_jpeg_ctx *ctx = priv;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	unsigned long flags;
	struct mtk_jpeg_src_buf *jpeg_src_buf;
	struct mtk_jpeg_bs bs;
	struct mtk_jpeg_fb fb;
	int ret;

	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
	if (src_buf == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! src_buf is NULL\n");
		return;
	}
	if (dst_buf == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! dst_buf is NULL\n");
		return;
	}
	jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(&src_buf->vb2_buf);

	if (mtk_jpeg_check_resolution_change(ctx, &jpeg_src_buf->dec_param)) {
		mtk_jpeg_queue_src_chg_event(ctx);
		ctx->state = MTK_JPEG_SOURCE_CHANGE;
		v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
		return;
	}

	ret = pm_runtime_get_sync(jpeg->dev);
	if (ret < 0)
		goto dec_end;

	schedule_delayed_work(&jpeg->job_timeout_work,
			      msecs_to_jiffies(MTK_JPEG_HW_TIMEOUT_MSEC));

	mtk_jpeg_set_dec_src(ctx, &src_buf->vb2_buf, &bs);
	if (mtk_jpeg_set_dec_dst(ctx, &jpeg_src_buf->dec_param, &dst_buf->vb2_buf, &fb))
		goto dec_end;

	spin_lock_irqsave(&jpeg->hw_lock, flags);
	mtk_jpeg_dec_reset(jpeg->reg_base);
	mtk_jpeg_dec_set_config(jpeg->reg_base,
				jpeg->support_34bits,
				&jpeg_src_buf->dec_param,
				jpeg_src_buf->bs_size,
				&bs,
				&fb);

	mtk_jpeg_dec_start(jpeg->reg_base);
	spin_unlock_irqrestore(&jpeg->hw_lock, flags);
	return;

dec_end:
	v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_buf_done(src_buf, buf_state);
	v4l2_m2m_buf_done(dst_buf, buf_state);
	v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
}

static int mtk_jpeg_dec_job_ready(void *priv)
{
	struct mtk_jpeg_ctx *ctx = priv;

	return (ctx->state == MTK_JPEG_RUNNING) ? 1 : 0;
}

static const struct v4l2_m2m_ops mtk_jpeg_enc_m2m_ops = {
	.device_run = mtk_jpeg_enc_device_run,
};

static const struct v4l2_m2m_ops mtk_jpeg_dec_m2m_ops = {
	.device_run = mtk_jpeg_dec_device_run,
	.job_ready  = mtk_jpeg_dec_job_ready,
};

static int mtk_jpeg_queue_init(void *priv, struct vb2_queue *src_vq,
			       struct vb2_queue *dst_vq)
{
	struct mtk_jpeg_ctx *ctx = priv;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_DMABUF | VB2_MMAP;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct mtk_jpeg_src_buf);
	src_vq->ops = jpeg->variant->qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->jpeg->lock;
	src_vq->dev = ctx->jpeg->smmu_dev;
	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_DMABUF | VB2_MMAP;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct mtk_jpeg_src_buf);//sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = jpeg->variant->qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->jpeg->lock;
	dst_vq->dev = ctx->jpeg->smmu_dev;
	ret = vb2_queue_init(dst_vq);

	return ret;
}

static void mtk_jpeg_clk_on(struct mtk_jpeg_dev *jpeg)
{
	int ret, i;

	for (i = 0; i < jpeg->variant->num_clks; i++) {
		if (jpeg->larb[i]) {
			ret = pm_runtime_resume_and_get(jpeg->larb[i]);
			if (ret)
				pr_info("clk on jpeg clk %d fail %d\n", i, ret);
		}
	}

	pr_info("%s %d num_clks %d\n", __func__,
		__LINE__, jpeg->variant->num_clks);
#if MTK_JPEG_DEC_SUPPORT
	if (jpeg->jpeg_dev_type != MTK_JPEG_ENC) {
		ret = clk_prepare_enable(jpeg->jpegDecClk.clk_jpgDec);
		if (ret)
			JPEG_LOG(0, "clk MT_CG_VENC_JPGDEC failed %d",
					ret);
		ret = clk_prepare_enable(jpeg->jpegDecClk.clk_jpgDec_c1);
		if (ret)
			JPEG_LOG(0, "clk MT_CG_VENC_JPGDEC_C1 failed %d",
					ret);

		//core1 need open core0 firstly
		if (jpeg->jpeg_dev_type == MTK_JPEG_DEC_C1) {
			ret = clk_prepare_enable(jpeg->jpegDecClk.clk_jpgDec_c2);
			if (ret)
				JPEG_LOG(0, "clk enable MT_CG_VENC_JPGDEC_C2 failed %d",
						ret);
		}
	} else
#endif
		{
			ret = clk_bulk_prepare_enable(jpeg->variant->num_clks,
					jpeg->variant->clks);
			if (ret)
				v4l2_err(&jpeg->v4l2_dev, "Failed to open jpeg clk: %d\n", ret);
		}
}

static void mtk_jpeg_clk_off(struct mtk_jpeg_dev *jpeg)
{
	int i;
#if MTK_JPEG_DEC_SUPPORT
	if (jpeg->jpeg_dev_type != MTK_JPEG_ENC) {
		//core1 need close core0 firstly
		if (jpeg->jpeg_dev_type == MTK_JPEG_DEC_C1)
			clk_disable_unprepare(jpeg->jpegDecClk.clk_jpgDec_c2);

		clk_disable_unprepare(jpeg->jpegDecClk.clk_jpgDec_c1);
		clk_disable_unprepare(jpeg->jpegDecClk.clk_jpgDec);
	} else
#endif
		clk_bulk_disable_unprepare(jpeg->variant->num_clks,
					jpeg->variant->clks);

	for (i = 0; i < jpeg->variant->num_clks; i++) {
		if (jpeg->larb[i])
			pm_runtime_put_sync(jpeg->larb[i]);
	}
	pr_info("clk off jpeg OK larb num[%d]\n", jpeg->variant->num_clks);
}

static irqreturn_t mtk_jpeg_enc_done(struct mtk_jpeg_dev *jpeg)
{
	struct mtk_jpeg_ctx *ctx;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	u32 result_size;

	ctx = v4l2_m2m_get_curr_priv(jpeg->m2m_dev);
	if (!ctx) {
		v4l2_err(&jpeg->v4l2_dev, "Context is NULL\n");
		return IRQ_HANDLED;
	}
	ctx->time_end = jiffies_to_nsecs(jiffies);

	src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	if (src_buf == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! src_buf is NULL\n");
		return IRQ_NONE;
	}
	if (dst_buf == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! dst_buf is NULL\n");
		return IRQ_NONE;
	}

	dst_buf->vb2_buf.timestamp = src_buf->vb2_buf.timestamp;
	result_size = mtk_jpeg_enc_get_file_size(jpeg->reg_base, jpeg->support_34bits);
	ctx->size_output = result_size;
	vb2_set_plane_payload(&dst_buf->vb2_buf, 0, result_size);

	buf_state = VB2_BUF_STATE_DONE;

	if (v4l2_m2m_is_last_draining_src_buf(ctx->fh.m2m_ctx, src_buf)) {
		v4l2_dbg(0, debug, &jpeg->v4l2_dev, "mark stopped\n");
		dst_buf->flags |= V4L2_BUF_FLAG_LAST;
		v4l2_m2m_mark_stopped(ctx->fh.m2m_ctx);
	}
	if (debug)
		pr_info("Enc_done:%d\n", result_size);

	v4l2_m2m_buf_done(src_buf, buf_state);
	v4l2_m2m_buf_done(dst_buf, buf_state);
	v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
	return IRQ_HANDLED;
}

static irqreturn_t mtk_jpeg_enc_irq(int irq, void *priv)
{
	struct mtk_jpeg_dev *jpeg = priv;
	u32 irq_status;
	irqreturn_t ret = IRQ_NONE;

	cancel_delayed_work(&jpeg->job_timeout_work);

	irq_status = readl(jpeg->reg_base + JPEG_ENC_INT_STS) &
		     JPEG_ENC_INT_STATUS_MASK_ALLIRQ;
	if (irq_status)
		writel(0, jpeg->reg_base + JPEG_ENC_INT_STS);

	if (irq_status & JPEG_ENC_INT_STATUS_STALL)
		pr_info("irq stall need to check output buffer size");

	if (!(irq_status & JPEG_ENC_INT_STATUS_DONE))
		return ret;

	ret = mtk_jpeg_enc_done(jpeg);
	return ret;
}

static irqreturn_t mtk_jpeg_dec_irq(int irq, void *priv)
{
	struct mtk_jpeg_dev *jpeg = priv;
	struct mtk_jpeg_ctx *ctx;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	struct mtk_jpeg_src_buf *jpeg_src_buf;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	u32	dec_irq_ret;
	u32 dec_ret;
	int i;

	cancel_delayed_work(&jpeg->job_timeout_work);

	dec_ret = mtk_jpeg_dec_get_int_status(jpeg->reg_base);
	dec_irq_ret = mtk_jpeg_dec_enum_result(dec_ret);
	ctx = v4l2_m2m_get_curr_priv(jpeg->m2m_dev);
	if (!ctx) {
		v4l2_err(&jpeg->v4l2_dev, "Context is NULL\n");
		return IRQ_HANDLED;
	}

	src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	if (src_buf == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! src_buf is NULL\n");
		return IRQ_NONE;
	}
	if (dst_buf == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! dst_buf is NULL\n");
		return IRQ_NONE;
	}
	dst_buf->vb2_buf.timestamp = src_buf->vb2_buf.timestamp;
	jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(&src_buf->vb2_buf);

	if (dec_irq_ret >= MTK_JPEG_DEC_RESULT_UNDERFLOW)
		mtk_jpeg_dec_reset(jpeg->reg_base);

	if (dec_irq_ret != MTK_JPEG_DEC_RESULT_EOF_DONE) {
		dev_err(jpeg->dev, "decode failed\n");
		goto dec_end;
	}

	for (i = 0; i < dst_buf->vb2_buf.num_planes; i++)
		vb2_set_plane_payload(&dst_buf->vb2_buf, i,
				      jpeg_src_buf->dec_param.comp_size[i]);

	buf_state = VB2_BUF_STATE_DONE;

dec_end:

	if (v4l2_m2m_is_last_draining_src_buf(ctx->fh.m2m_ctx, src_buf)) {
		v4l2_dbg(0, debug, &jpeg->v4l2_dev, "mark stopped\n");
		dst_buf->flags |= V4L2_BUF_FLAG_LAST;
		v4l2_m2m_mark_stopped(ctx->fh.m2m_ctx);
		ctx->early_eos = true;
	}

	v4l2_m2m_buf_done(src_buf, buf_state);
	v4l2_m2m_buf_done(dst_buf, buf_state);
	pm_runtime_put(ctx->jpeg->dev);
	v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
	return IRQ_HANDLED;
}

static void mtk_jpeg_set_default_params(struct mtk_jpeg_ctx *ctx)
{
	struct mtk_jpeg_q_data *q = &ctx->out_q;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	ctx->fh.ctrl_handler = &ctx->ctrl_hdl;
	q->pix_mp.colorspace = V4L2_COLORSPACE_SRGB;
	q->pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_601;
	q->pix_mp.quantization = V4L2_QUANTIZATION_FULL_RANGE;
	q->pix_mp.xfer_func = V4L2_XFER_FUNC_SRGB;

	q->fmt = mtk_jpeg_find_format(jpeg->variant->formats,
				      jpeg->variant->num_formats,
				      jpeg->variant->out_q_default_fourcc,
				      MTK_JPEG_FMT_FLAG_OUTPUT);
	if (q->fmt == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! q->fmt is NULL\n");
		return;
	}
	q->pix_mp.width = MTK_JPEG_MIN_WIDTH;
	q->pix_mp.height = MTK_JPEG_MIN_HEIGHT;
	mtk_jpeg_try_fmt_mplane(&q->pix_mp, q->fmt);

	q = &ctx->cap_q;
	q->fmt = mtk_jpeg_find_format(jpeg->variant->formats,
				      jpeg->variant->num_formats,
				      jpeg->variant->cap_q_default_fourcc,
				      MTK_JPEG_FMT_FLAG_CAPTURE);
	if (q->fmt == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! q->fmt is NULL\n");
		return;
	}
	q->pix_mp.colorspace = V4L2_COLORSPACE_SRGB;
	q->pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_601;
	q->pix_mp.quantization = V4L2_QUANTIZATION_FULL_RANGE;
	q->pix_mp.xfer_func = V4L2_XFER_FUNC_SRGB;
	q->pix_mp.width = MTK_JPEG_MIN_WIDTH;
	q->pix_mp.height = MTK_JPEG_MIN_HEIGHT;

	mtk_jpeg_try_fmt_mplane(&q->pix_mp, q->fmt);
}

static int mtk_jpeg_open(struct file *file)
{
	struct mtk_jpeg_dev *jpeg = video_drvdata(file);
	struct video_device *vfd = video_devdata(file);
	struct mtk_jpeg_ctx *ctx;
	int ret = 0;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	if (mutex_lock_interruptible(&jpeg->lock)) {
		ret = -ERESTARTSYS;
		goto free;
	}

	v4l2_fh_init(&ctx->fh, vfd);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	ctx->jpeg = jpeg;
	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(jpeg->m2m_dev, ctx,
					    mtk_jpeg_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto error;
	}

	if (jpeg->variant->cap_q_default_fourcc == V4L2_PIX_FMT_JPEG) {
		ret = mtk_jpeg_enc_ctrls_setup(ctx);
		if (ret) {
			v4l2_err(&jpeg->v4l2_dev, "Failed to setup jpeg enc controls\n");
			goto error;
		}
	} else {
		v4l2_ctrl_handler_init(&ctx->ctrl_hdl, 0);
	}
	mtk_jpeg_set_default_params(ctx);
	mutex_unlock(&jpeg->lock);
	return 0;

error:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	mutex_unlock(&jpeg->lock);
free:
	kfree(ctx);
	return ret;
}

static int mtk_jpeg_release(struct file *file)
{
	struct mtk_jpeg_dev *jpeg = video_drvdata(file);
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(file->private_data);

	if (ctx->state == MTK_JPEG_RUNNING) {
	#if MTK_JPEG_DVFS_BW_SUPPORT
		mtk_jpeg_dvfs_end(ctx);
		mtk_jpeg_end_bw_request(ctx);
	#endif
		pm_runtime_put(ctx->jpeg->dev);
	}
	if ((ctx->size_output != 0) && (ctx->time_end != 0))
		pr_info("%s  time(ms) %lld outsize %d\n", __func__,
			NS_TO_MS(ctx->time_end - ctx->time_start),
			ctx->size_output);
	mutex_lock(&jpeg->lock);
	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
	mutex_unlock(&jpeg->lock);
	return 0;
}

static const struct v4l2_file_operations mtk_jpeg_fops = {
	.owner          = THIS_MODULE,
	.open           = mtk_jpeg_open,
	.release        = mtk_jpeg_release,
	.poll           = v4l2_m2m_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap           = v4l2_m2m_fop_mmap,
};

static struct clk_bulk_data mt8173_jpeg_dec_clocks[] = {
	{ .id = "jpgdec-smi" },
	{ .id = "jpgdec" },
};

static struct clk_bulk_data mtk_jpeg_clocks[] = {
	{ .id = "jpgenc" },
};

static struct clk_bulk_data mtk_jpeg_clocks_c1[] = {
	{ .id = "jpgenc_c1" },
};

static struct clk_bulk_data mtk_jpeg_clocks_c0_c1[] = {
	{ .id = "jpgenc" },
	{ .id = "jpgenc_c1" },
};

#if MTK_JPEG_DEC_SUPPORT
static struct clk_bulk_data mt6991_jpeg_dec_c0_clocks[] = {
	{ .id = "MT_CG_VENC_JPGDEC" },
	{ .id = "MT_CG_VENC_JPGDEC_C1" },
};

static struct clk_bulk_data mt6991_jpeg_dec_c1_clocks[] = {
	{ .id = "MT_CG_VENC_JPGDEC_C2" },
};

static int jpeg_get_node_type(const char *name)
{
	if (strncmp(name, "jpgdec0", strlen("jpgdec0")) == 0) {
		JPEG_LOG(0, "name %s", name);
		return MTK_JPEG_DEC_C0;
	} else if (strncmp(name, "jpgdec1", strlen("jpgdec1")) == 0) {
		JPEG_LOG(0, "name %s", name);
		return MTK_JPEG_DEC_C1;
	}else
		return MTK_JPEG_ENC;

	JPEG_LOG(0, "name not found %s", name);
	return 0;
}

static void mtk_jpeg_dec_clk_init(struct mtk_jpeg_dev *jpeg)
{
	jpeg->jpegDecClk.clk_jpgDec =
		of_clk_get_by_name(jpeg->dev->of_node, "MT_CG_VENC_JPGDEC");
	if (IS_ERR(jpeg->jpegDecClk.clk_jpgDec))
		JPEG_LOG(0, "get MT_CG_VENC_JPGDEC clk error!");

	jpeg->jpegDecClk.clk_jpgDec_c1 =
		of_clk_get_by_name(jpeg->dev->of_node, "MT_CG_VENC_JPGDEC_C1");
	if (IS_ERR(jpeg->jpegDecClk.clk_jpgDec))
		JPEG_LOG(0, "get MT_CG_VENC_JPGDEC_C1 clk error!");

	JPEG_LOG(0, "clk_C0 OK!");

	//core1 need open core0 firstly
	if (jpeg->jpeg_dev_type == MTK_JPEG_DEC_C1) {
		jpeg->jpegDecClk.clk_jpgDec_c2 =
		of_clk_get_by_name(jpeg->dev->of_node, "MT_CG_VENC_JPGDEC_C2");
		if (IS_ERR(jpeg->jpegDecClk.clk_jpgDec_c2))
			JPEG_LOG(0, "get MT_CG_VENC_JPGDEC_C2 clk error!");

		JPEG_LOG(0, "clk_C1 OK!");
	}
}
#endif

static int mtk_jpeg_clk_init(struct mtk_jpeg_dev *jpeg)
{
	struct device_node *node;
	struct platform_device *pdev;
	int ret = 0;
	int larb_index;

	for (larb_index = 0; larb_index < jpeg->variant->num_clks; larb_index++) {
		node = of_parse_phandle(jpeg->dev->of_node, "mediatek,larb", larb_index);
		if (!node)
			return -EINVAL;

		pdev = of_find_device_by_node(node);
		if (WARN_ON(!pdev)) {
			of_node_put(node);
			return -EINVAL;
		}

		jpeg->larb[larb_index] = &pdev->dev;
		if (!device_link_add(jpeg->dev, jpeg->larb[larb_index],
					DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS)) {
			pr_info("%s larb device link fail %d\n", __func__, larb_index);
			return -EINVAL;
		}
	}

#if MTK_JPEG_DEC_SUPPORT
	if (jpeg->jpeg_dev_type != MTK_JPEG_ENC)
		mtk_jpeg_dec_clk_init(jpeg);
	else
#endif
	{
		ret = devm_clk_bulk_get(jpeg->dev, jpeg->variant->num_clks,
				jpeg->variant->clks);
		if (ret) {
			v4l2_err(&jpeg->v4l2_dev, "failed to get jpeg clock:%d\n", ret);
			for (larb_index = 0; larb_index < jpeg->variant->num_clks; larb_index++) {
				if (jpeg->larb[larb_index])
					put_device(jpeg->larb[larb_index]);
			}
			return ret;
		}
	}
	return 0;
}

static void mtk_jpeg_job_timeout_work(struct work_struct *work)
{
	struct mtk_jpeg_dev *jpeg = container_of(work, struct mtk_jpeg_dev,
						 job_timeout_work.work);
	struct mtk_jpeg_ctx *ctx;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;

	ctx = v4l2_m2m_get_curr_priv(jpeg->m2m_dev);
	if (ctx == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! ctx is NULL\n");
		return;
	}
	src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	if (src_buf == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! src_buf is NULL\n");
		return;
	}
	if (dst_buf == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Error!! dst_buf is NULL\n");
		return;
	}
	dst_buf->vb2_buf.timestamp = src_buf->vb2_buf.timestamp;

	jpeg->variant->hw_reset(jpeg->reg_base);

	pm_runtime_put(jpeg->dev);

	v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_ERROR);
	v4l2_m2m_buf_done(dst_buf, VB2_BUF_STATE_ERROR);
	v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
}

static inline void mtk_jpeg_clk_release(struct mtk_jpeg_dev *jpeg)
{
	int i;

	for (i = 0; i < jpeg->variant->num_clks; i++) {
		if (jpeg->larb[i])
			put_device(jpeg->larb[i]);
	}
}

static int mtk_jpeg_probe(struct platform_device *pdev)
{
	struct mtk_jpeg_dev *jpeg;
	struct resource *res;
	int jpeg_irq;
	int ret;

	jpeg = devm_kzalloc(mtk_smmu_get_shared_device(&pdev->dev), sizeof(*jpeg), GFP_KERNEL);
	if (!jpeg)
		return -ENOMEM;

#if MTK_JPEG_DEC_SUPPORT
	jpeg->jpeg_dev_type = jpeg_get_node_type(pdev->dev.of_node->name);
#endif

	jpeg->smmu_dev = mtk_smmu_get_shared_device(&pdev->dev);

	mutex_init(&jpeg->lock);
	spin_lock_init(&jpeg->hw_lock);
	jpeg->dev = &pdev->dev;
	jpeg->variant = of_device_get_match_data(jpeg->dev);
	INIT_DELAYED_WORK(&jpeg->job_timeout_work, mtk_jpeg_job_timeout_work);

	ret = of_property_read_u32(pdev->dev.of_node, "support-34bits", &jpeg->support_34bits);
	pr_info("%s  ret: %d  support_34bit: %u", __func__, ret, jpeg->support_34bits);
	if (ret != 0) {
		dev_info(&pdev->dev, "default for support-34bits");
		jpeg->support_34bits = MTK_JPEG_SUPPORT_34BITS;
	}
	dev_info(&pdev->dev, "use 34bits %d", jpeg->support_34bits);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	jpeg->reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(jpeg->reg_base)) {
		ret = PTR_ERR(jpeg->reg_base);
		return ret;
	}

#if MTK_JPEG_DEC_SUPPORT
	if ((jpeg->support_34bits == MTK_JPEG_FAKE_34BITS)
		&& (jpeg->jpeg_dev_type == MTK_JPEG_ENC))
#else
	if (jpeg->support_34bits == MTK_JPEG_FAKE_34BITS)
#endif
	{
		res = platform_get_resource(pdev, IORESOURCE_MEM, VENC_GCON);
		jpeg->gcon_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(jpeg->gcon_base)) {
			ret = PTR_ERR(jpeg->gcon_base);
			return ret;
		}
	}

	jpeg_irq = platform_get_irq(pdev, 0);
	if (jpeg_irq < 0) {
		dev_err(&pdev->dev, "Failed to get jpeg_irq %d.\n", jpeg_irq);
		return jpeg_irq;
	}

	ret = devm_request_irq(&pdev->dev, jpeg_irq,
			       jpeg->variant->irq_handler, 0, pdev->name, jpeg);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request jpeg_irq %d (%d)\n",
			jpeg_irq, ret);
		goto err_req_irq;
	}

	ret = mtk_jpeg_clk_init(jpeg);
	if (ret) {
		dev_err(&pdev->dev, "Failed to init clk, err %d\n", ret);
		goto err_clk_init;
	}

	ret = v4l2_device_register(&pdev->dev, &jpeg->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register v4l2 device\n");
		ret = -EINVAL;
		goto err_dev_register;
	}

	jpeg->m2m_dev = v4l2_m2m_init(jpeg->variant->m2m_ops);

	if (IS_ERR(jpeg->m2m_dev)) {
		v4l2_err(&jpeg->v4l2_dev, "Failed to init mem2mem device\n");
		ret = PTR_ERR(jpeg->m2m_dev);
		goto err_m2m_init;
	}

	jpeg->vdev = video_device_alloc();
	if (!jpeg->vdev) {
		ret = -ENOMEM;
		goto err_vfd_jpeg_alloc;
	}
	SNPRINTF(jpeg->vdev->name, sizeof(jpeg->vdev->name),
		 "%s", jpeg->variant->dev_name);

#if MTK_JPEG_DVFS_BW_SUPPORT
	mtk_jpeg_prepare_dvfs(jpeg);
	mtk_jpeg_prepare_bw_request(jpeg);
#endif

	jpeg->vdev->fops = &mtk_jpeg_fops;
	jpeg->vdev->ioctl_ops = jpeg->variant->ioctl_ops;
	jpeg->vdev->minor = -1;
	jpeg->vdev->release = video_device_release;
	jpeg->vdev->lock = &jpeg->lock;
	jpeg->vdev->v4l2_dev = &jpeg->v4l2_dev;
	jpeg->vdev->vfl_dir = VFL_DIR_M2M;
	jpeg->vdev->device_caps = V4L2_CAP_STREAMING |
				  V4L2_CAP_VIDEO_M2M_MPLANE;

	ret = video_register_device(jpeg->vdev, VFL_TYPE_VIDEO, -1);
	if (ret) {
		v4l2_err(&jpeg->v4l2_dev, "Failed to register video device\n");
		goto err_vfd_jpeg_register;
	}

	ret = dma_set_mask_and_coherent(jpeg->smmu_dev, DMA_BIT_MASK(34));
	if (ret) {
		dev_info(&pdev->dev, "34-bit DMA enable failed\n");
		return ret;
	}

	video_set_drvdata(jpeg->vdev, jpeg);
	v4l2_info(&jpeg->v4l2_dev,
		  "%s device registered as /dev/video%d (%d,%d)\n",
		  jpeg->variant->dev_name, jpeg->vdev->num,
		  VIDEO_MAJOR, jpeg->vdev->minor);

	platform_set_drvdata(pdev, jpeg);

	pm_runtime_enable(&pdev->dev);

	if (strcmp((const char *)jpeg->variant->dev_name, "mtk-jpeg-enc") == 0)
		jpgenc_probe_time++;
#if MTK_JPEG_DEC_SUPPORT
	if (strcmp((const char *)jpeg->variant->dev_name, "mtk-jpeg-dec") == 0)
		jpgDec_probe_time++;
#endif
	return 0;

err_vfd_jpeg_register:
	video_device_release(jpeg->vdev);

err_vfd_jpeg_alloc:
	v4l2_m2m_release(jpeg->m2m_dev);

err_m2m_init:
	v4l2_device_unregister(&jpeg->v4l2_dev);

err_dev_register:
	mtk_jpeg_clk_release(jpeg);

err_clk_init:

err_req_irq:
	return ret;
}

static int mtk_jpeg_remove(struct platform_device *pdev)
{
	struct mtk_jpeg_dev *jpeg = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	video_unregister_device(jpeg->vdev);
	video_device_release(jpeg->vdev);
	v4l2_m2m_release(jpeg->m2m_dev);
	v4l2_device_unregister(&jpeg->v4l2_dev);
	mtk_jpeg_clk_release(jpeg);

	return 0;
}

static __maybe_unused int mtk_jpeg_pm_suspend(struct device *dev)
{
	struct mtk_jpeg_dev *jpeg = dev_get_drvdata(dev);

	mtk_jpeg_clk_off(jpeg);

	return 0;
}

static __maybe_unused int mtk_jpeg_pm_resume(struct device *dev)
{
	struct mtk_jpeg_dev *jpeg = dev_get_drvdata(dev);

	mtk_jpeg_clk_on(jpeg);

	return 0;
}

static const struct dev_pm_ops mtk_jpeg_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_jpeg_pm_suspend, mtk_jpeg_pm_resume, NULL)
};

static const struct mtk_jpeg_variant mt8173_jpeg_drvdata = {
	.clks = mt8173_jpeg_dec_clocks,
	.num_clks = ARRAY_SIZE(mt8173_jpeg_dec_clocks),
	.formats = mtk_jpeg_dec_formats,
	.num_formats = MTK_JPEG_DEC_NUM_FORMATS,
	.qops = &mtk_jpeg_dec_qops,
	.irq_handler = mtk_jpeg_dec_irq,
	.hw_reset = mtk_jpeg_dec_reset,
	.m2m_ops = &mtk_jpeg_dec_m2m_ops,
	.dev_name = "mtk-jpeg-dec",
	.ioctl_ops = &mtk_jpeg_dec_ioctl_ops,
	.out_q_default_fourcc = V4L2_PIX_FMT_JPEG,
	.cap_q_default_fourcc = V4L2_PIX_FMT_YUV420M,
};

static const struct mtk_jpeg_variant mtk_jpeg_drvdata = {
	.clks = mtk_jpeg_clocks,
	.num_clks = ARRAY_SIZE(mtk_jpeg_clocks),
	.formats = mtk_jpeg_enc_formats,
	.num_formats = MTK_JPEG_ENC_NUM_FORMATS,
	.qops = &mtk_jpeg_enc_qops,
	.irq_handler = mtk_jpeg_enc_irq,
	.hw_reset = mtk_jpeg_enc_reset,
	.m2m_ops = &mtk_jpeg_enc_m2m_ops,
	.dev_name = "mtk-jpeg-enc",
	.ioctl_ops = &mtk_jpeg_enc_ioctl_ops,
	.out_q_default_fourcc = V4L2_PIX_FMT_YUYV,
	.cap_q_default_fourcc = V4L2_PIX_FMT_JPEG,
};

static const struct mtk_jpeg_variant mtk_jpeg_drvdata_c1 = {
	.clks = mtk_jpeg_clocks_c1,
	.num_clks = ARRAY_SIZE(mtk_jpeg_clocks_c1),
	.formats = mtk_jpeg_enc_formats,
	.num_formats = MTK_JPEG_ENC_NUM_FORMATS,
	.qops = &mtk_jpeg_enc_qops,
	.irq_handler = mtk_jpeg_enc_irq,
	.hw_reset = mtk_jpeg_enc_reset,
	.m2m_ops = &mtk_jpeg_enc_m2m_ops,
	.dev_name = "mtk-jpeg-enc",
	.ioctl_ops = &mtk_jpeg_enc_ioctl_ops,
	.out_q_default_fourcc = V4L2_PIX_FMT_YUYV,
	.cap_q_default_fourcc = V4L2_PIX_FMT_JPEG,
};

static const struct mtk_jpeg_variant mtk_jpeg_drvdata_c0_c1 = {
	.clks = mtk_jpeg_clocks_c0_c1,
	.num_clks = ARRAY_SIZE(mtk_jpeg_clocks_c0_c1),
	.formats = mtk_jpeg_enc_formats,
	.num_formats = MTK_JPEG_ENC_NUM_FORMATS,
	.qops = &mtk_jpeg_enc_qops,
	.irq_handler = mtk_jpeg_enc_irq,
	.hw_reset = mtk_jpeg_enc_reset,
	.m2m_ops = &mtk_jpeg_enc_m2m_ops,
	.dev_name = "mtk-jpeg-enc",
	.ioctl_ops = &mtk_jpeg_enc_ioctl_ops,
	.out_q_default_fourcc = V4L2_PIX_FMT_YUYV,
	.cap_q_default_fourcc = V4L2_PIX_FMT_JPEG,
};

#if MTK_JPEG_DEC_SUPPORT
static const struct mtk_jpeg_variant mt6991_jpeg_drvdata_c0 = {
	.clks = mt6991_jpeg_dec_c0_clocks,
	.num_clks = 1,/*ARRAY_SIZE(mt6991_jpeg_dec_c0_clocks),*/
	.formats = mtk_jpeg_dec_formats,
	.num_formats = MTK_JPEG_DEC_NUM_FORMATS,
	.qops = &mtk_jpeg_dec_qops,
	.irq_handler = mtk_jpeg_dec_irq,
	.hw_reset = mtk_jpeg_dec_reset,
	.m2m_ops = &mtk_jpeg_dec_m2m_ops,
	.dev_name = "mtk-jpeg-dec",
	.ioctl_ops = &mtk_jpeg_dec_ioctl_ops,
	.out_q_default_fourcc = V4L2_PIX_FMT_JPEG,
	.cap_q_default_fourcc = V4L2_PIX_FMT_YUV420M,
};

//dec core1 need open core0 firstly, so num_clks = 2
static const struct mtk_jpeg_variant mt6991_jpeg_drvdata_c1 = {
	.clks = mt6991_jpeg_dec_c1_clocks,
	.num_clks = 2,/*ARRAY_SIZE(mt6991_jpeg_dec_c1_clocks),*/
	.formats = mtk_jpeg_dec_formats,
	.num_formats = MTK_JPEG_DEC_NUM_FORMATS,
	.qops = &mtk_jpeg_dec_qops,
	.irq_handler = mtk_jpeg_dec_irq,
	.hw_reset = mtk_jpeg_dec_reset,
	.m2m_ops = &mtk_jpeg_dec_m2m_ops,
	.dev_name = "mtk-jpeg-dec",
	.ioctl_ops = &mtk_jpeg_dec_ioctl_ops,
	.out_q_default_fourcc = V4L2_PIX_FMT_JPEG,
	.cap_q_default_fourcc = V4L2_PIX_FMT_YUV420M,
};
#endif

static const struct of_device_id mtk_jpeg_match[] = {
	{
		.compatible = "mediatek,mt8173-jpgdec",
		.data = &mt8173_jpeg_drvdata,
	},
	{
		.compatible = "mediatek,mt2701-jpgdec",
		.data = &mt8173_jpeg_drvdata,
	},
#if MTK_JPEG_DEC_SUPPORT
	{
		.compatible = "mediatek,jpgdec_c0_v4l2",
		.data = &mt6991_jpeg_drvdata_c0,
	},
	{
		.compatible = "mediatek,jpgdec_c1_v4l2",
		.data = &mt6991_jpeg_drvdata_c1,
	},
#endif
	{
		.compatible = "mediatek,mtk-jpgenc",
		.data = &mtk_jpeg_drvdata,
	},
	{
		.compatible = "mediatek,mtk-jpgenc_c1",
		.data = &mtk_jpeg_drvdata_c1,
	},
	{
		.compatible = "mediatek,mtk-jpgenc_c0_c1",
		.data = &mtk_jpeg_drvdata_c0_c1,
	},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_jpeg_match);

static struct platform_driver mtk_jpeg_driver = {
	.probe = mtk_jpeg_probe,
	.remove = mtk_jpeg_remove,
	.driver = {
		.name           = MTK_JPEG_NAME,
		.of_match_table = mtk_jpeg_match,
		.pm             = &mtk_jpeg_pm_ops,
	},
};

module_platform_driver(mtk_jpeg_driver);

MODULE_DESCRIPTION("MediaTek JPEG codec driver");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(DMA_BUF);
