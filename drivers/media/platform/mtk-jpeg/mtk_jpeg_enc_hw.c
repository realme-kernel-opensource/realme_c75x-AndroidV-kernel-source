// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Xia Jiang <xia.jiang@mediatek.com>
 *
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>

#include "mtk_jpeg_enc_hw.h"

static const struct mtk_jpeg_enc_qlt mtk_jpeg_enc_quality[] = {
	{.quality_param = 24, .hardware_value = JPEG_ENC_QUALITY_Q24},
	{.quality_param = 30, .hardware_value = JPEG_ENC_QUALITY_Q30},
	{.quality_param = 34, .hardware_value = JPEG_ENC_QUALITY_Q34},
	{.quality_param = 38, .hardware_value = JPEG_ENC_QUALITY_Q38},
	{.quality_param = 44, .hardware_value = JPEG_ENC_QUALITY_Q44},
	{.quality_param = 52, .hardware_value = JPEG_ENC_QUALITY_Q52},
	{.quality_param = 60, .hardware_value = JPEG_ENC_QUALITY_Q60},
	{.quality_param = 66, .hardware_value = JPEG_ENC_QUALITY_Q66},
	{.quality_param = 72, .hardware_value = JPEG_ENC_QUALITY_Q72},
	{.quality_param = 78, .hardware_value = JPEG_ENC_QUALITY_Q78},
	{.quality_param = 82, .hardware_value = JPEG_ENC_QUALITY_Q82},
	{.quality_param = 85, .hardware_value = JPEG_ENC_QUALITY_Q85},
	{.quality_param = 90, .hardware_value = JPEG_ENC_QUALITY_Q90},
	{.quality_param = 95, .hardware_value = JPEG_ENC_QUALITY_Q95},
	{.quality_param = 97, .hardware_value = JPEG_ENC_QUALITY_Q97},
};

void mtk_jpeg_enc_set_34bits(struct mtk_jpeg_ctx *ctx, void __iomem *base,
				struct vb2_buffer *dst_buf)
{
	#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	dma_addr_t dma_addr;
	u32 value = 0;

	dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, 0);
	dma_addr += ctx->dst_offset;
	value = (dma_addr >> 32);
	writel(value << 1, base + 0x108);
	#else
	pr_info("%s %d: don't support 34bit", __func__, __LINE__);
	#endif
}

void mtk_jpeg_enc_reset(void __iomem *base)
{
	writel(0, base + JPEG_ENC_RSTB);
	writel(JPEG_ENC_RESET_BIT, base + JPEG_ENC_RSTB);
	writel(0, base + JPEG_ENC_CODEC_SEL);
}

u32 mtk_jpeg_enc_get_file_size(void __iomem *base, enum mtk_jpeg_support_34bits support_34bits)
{
	if (support_34bits != MTK_JPEG_NON_SUPPORT_34BITS) {
		return readl(base + JPEG_ENC_DMA_ADDR0)*4 -
			readl(base + JPEG_ENC_DST_ADDR0);
	} else {
		return readl(base + JPEG_ENC_DMA_ADDR0) -
			readl(base + JPEG_ENC_DST_ADDR0);
	}
}

void mtk_jpeg_enc_start(void __iomem *base)
{
	u32 value;

	value = readl(base + JPEG_ENC_CTRL);
	value |= JPEG_ENC_CTRL_INT_EN_BIT | JPEG_ENC_CTRL_ENABLE_BIT;
	value |= JPEG_ENC_CTRL_RDMA_PADDING_EN;
	value |= JPEG_ENC_CTRL_RDMA_RIGHT_PADDING_EN;
	value &= ~JPEG_ENC_CTRL_RDMA_PADDING_0_EN;

	writel(value, base + JPEG_ENC_CTRL);
}

void mtk_jpeg_set_enc_src(struct mtk_jpeg_ctx *ctx,  void __iomem *base,
			  struct vb2_buffer *src_buf)
{
	int i;
	dma_addr_t dma_addr;

	for (i = 0; i < src_buf->num_planes; i++) {
		dma_addr = vb2_dma_contig_plane_dma_addr(src_buf, i) +
			   src_buf->planes[i].data_offset;
		if (!i) {
			pr_info("%s %d dma_addr %pad", __func__, __LINE__, &dma_addr);
			writel(dma_addr, base + JPEG_ENC_SRC_LUMA_ADDR);
	#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
			writel(dma_addr >> 32, base + JPEG_ENC_SRC_LUMA_ADDR_EXT);
	#endif
		} else {
			pr_info("%s %d dma_addr %pad", __func__, __LINE__, &dma_addr);
			writel(dma_addr, base + JPEG_ENC_SRC_CHROMA_ADDR);
	#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
			writel(dma_addr >> 32, base + JPEG_ENC_SRC_CHROMA_ADDR_EXT);
	#endif
		}
	}
}

void mtk_jpeg_set_enc_dst(struct mtk_jpeg_ctx *ctx, void __iomem *base,
			  struct vb2_buffer *dst_buf)
{
	dma_addr_t dma_addr;
	size_t size;
	u32 dma_addr_offset;
	u32 dma_addr_offsetmask;

	dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, 0);
	dma_addr += ctx->dst_offset;
	dma_addr_offset = 0;
	dma_addr_offsetmask = dma_addr & JPEG_ENC_DST_ADDR_OFFSET_MASK;
	size = vb2_plane_size(dst_buf, 0);

	writel(dma_addr_offset & ~0xf, base + JPEG_ENC_OFFSET_ADDR);
	writel(dma_addr_offsetmask & 0xf, base + JPEG_ENC_BYTE_OFFSET_MASK);
	writel(dma_addr & ~0xf, base + JPEG_ENC_DST_ADDR0);
	#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	writel(dma_addr >> 32, base + JPEG_ENC_DEST_ADDR0_EXT);
	#endif
	writel((dma_addr + size) & ~0xf, base + JPEG_ENC_STALL_ADDR0);
	#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	writel(((dma_addr + size)>>32), base + JPEG_ENC_STALL_ADDR0_EXT);
	#endif
}

void mtk_jpeg_set_enc_params(struct mtk_jpeg_ctx *ctx,  void __iomem *base)
{
	u32 value;
	u32 width = ctx->out_q.enc_crop_rect.width;
	u32 height = ctx->out_q.enc_crop_rect.height;
	u32 enc_format = ctx->out_q.fmt->fourcc;
	u32 bytesperline = ctx->out_q.pix_mp.plane_fmt[0].bytesperline;
	u32 blk_num;
	u32 img_stride;
	u32 mem_stride;
	u32 enc_quality;
	s32 i;

	value = width << 16 | height;
	writel(value, base + JPEG_ENC_IMG_SIZE);

	if (enc_format == V4L2_PIX_FMT_NV12M ||
		enc_format == V4L2_PIX_FMT_NV21M ||
		enc_format == V4L2_PIX_FMT_NV12_AFBC)
	    /*
	     * Total 8 x 8 block number of luma and chroma.
	     * The number of blocks is counted from 0.
	     */
		blk_num = DIV_ROUND_UP(width, 16) *
			  DIV_ROUND_UP(height, 16) * 6 - 1;
	else
		blk_num = DIV_ROUND_UP(width, 16) *
			  DIV_ROUND_UP(height, 8) * 4 - 1;
	writel(blk_num, base + JPEG_ENC_BLK_NUM);

	if (enc_format == V4L2_PIX_FMT_NV12M ||
		enc_format == V4L2_PIX_FMT_NV21M ||
		enc_format == V4L2_PIX_FMT_NV12_AFBC) {
		/* 4:2:0 */
		img_stride = round_up(width, 16);
		mem_stride = bytesperline;
	} else {
		/* 4:2:2 */
		img_stride = round_up(width * 2, 32);
		mem_stride = img_stride;
	}
	writel(img_stride, base + JPEG_ENC_IMG_STRIDE);
	writel(mem_stride, base + JPEG_ENC_STRIDE);

	if (enc_format == V4L2_PIX_FMT_NV12_AFBC) {
		value = (1U << 0) +   //enable
		(4U << 4) +   //disable SMI command grouping
		(3U << 8) +   //SMI command grouping number of plane 0 (payload)
		(4U << 12) +  //SMI command grouping number of plane 2 (header)
		(15U << 16) +  //SMI gburst type selection
				//0:Single access
				//1:Burst 2 access
				//7:Burst 8 access
				//15:Burst 16 access
		(1U << 24);   // support YUV_transform
		writel(value, base + JPGENC_AFBC_CONFIG_0);
	}

	enc_quality = JPEG_ENC_QUALITY_Q24;
	for (i = ARRAY_SIZE(mtk_jpeg_enc_quality)-1; i >= 0; i--) {
		if (ctx->enc_quality >= mtk_jpeg_enc_quality[i].quality_param) {
			enc_quality = mtk_jpeg_enc_quality[i].hardware_value;
			break;
		}
	}
	writel(enc_quality, base + JPEG_ENC_QUALITY);

	value = readl(base + JPEG_ENC_CTRL);
	value &= ~JPEG_ENC_CTRL_YUV_FORMAT_MASK;
	value |= (ctx->out_q.fmt->hw_format & 3) << 3;
	if (ctx->enable_exif)
		value |= JPEG_ENC_CTRL_FILE_FORMAT_BIT;
	else
		value &= ~JPEG_ENC_CTRL_FILE_FORMAT_BIT;
	if (ctx->restart_interval)
		value |= JPEG_ENC_CTRL_RESTART_EN_BIT;
	else
		value &= ~JPEG_ENC_CTRL_RESTART_EN_BIT;
	writel(value, base + JPEG_ENC_CTRL);

	writel(ctx->restart_interval, base + JPEG_ENC_RST_MCU_NUM);


	pr_info("fmt %d, w,h %d,%d, enable_exif %d, enc_quality %d, restart_interval %d,img_stride %d, mem_stride %d\n",
		enc_format, width, height,
		ctx->enable_exif, enc_quality, ctx->restart_interval,
		img_stride, mem_stride);

}
