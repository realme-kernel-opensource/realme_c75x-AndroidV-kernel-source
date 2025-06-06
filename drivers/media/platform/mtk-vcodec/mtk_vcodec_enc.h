/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2016 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*         Tiffany Lin <tiffany.lin@mediatek.com>
*/

#ifndef _MTK_VCODEC_ENC_H_
#define _MTK_VCODEC_ENC_H_

#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include "mtk_vcodec_util.h"

#define isSLB_CPU_USED_PERFORMANCE_USAGE(w, h, fr, throughput) \
		((((w)*(h)*(fr)) <= (throughput)) ? (1) : (0))
/**
 * struct mtk_video_enc_buf - Private data related to each VB2 buffer.
 * @vb: Pointer to related VB2 buffer.
 * @list:       list that buffer link to
 * @param_change: Types of encode parameter change before encoding this
 *                              buffer
 * @enc_params: Encode parameters changed before encode this buffer
 * @flags:  flags derived from v4l2_buffer for cache operations
 */
#define VENC_FLAG_MULTINAL					0x00000001
#define VENC_FLAG_ENCODE_TIMEOUT			0x00000002
#define VENC_FLAG_ENCODE_HWBREAK_TIMEOUT	0x00000004
#define VENC_FLAG_NAL_LENGTH_BS			0x00000008

struct mtk_video_enc_buf {
	struct vb2_v4l2_buffer vb;
	struct list_head list;
	u32 param_change;
	struct mtk_enc_params enc_params;
	enum eos_types lastframe;
	int    flags;
	int    general_user_fd;
	struct mtk_vcodec_mem bs_buf;
	struct venc_frm_buf frm_buf;
};

extern const struct v4l2_ioctl_ops mtk_venc_ioctl_ops;
extern const struct v4l2_m2m_ops mtk_venc_m2m_ops;

void mtk_venc_unlock(struct mtk_vcodec_ctx *ctx, u32 hw_id);
int mtk_venc_lock(struct mtk_vcodec_ctx *ctx, u32 hw_id, bool sec);
void mtk_venc_queue_error_event(struct mtk_vcodec_ctx *ctx);
int mtk_vcodec_enc_queue_init(void *priv, struct vb2_queue *src_vq,
	struct vb2_queue *dst_vq);
void mtk_vcodec_enc_empty_queues(struct file *file, struct mtk_vcodec_ctx *ctx);
void mtk_vcodec_enc_release(struct mtk_vcodec_ctx *ctx);
int mtk_vcodec_enc_ctrls_setup(struct mtk_vcodec_ctx *ctx);
void mtk_vcodec_enc_set_default_params(struct mtk_vcodec_ctx *ctx);
int mtk_vcodec_enc_set_config_data(struct mtk_vcodec_ctx *ctx, char *data);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
void venc_vcp_probe(struct mtk_vcodec_dev *dev);
void venc_vcp_remove(struct mtk_vcodec_dev *dev);
#endif

#endif /* _MTK_VCODEC_ENC_H_ */
