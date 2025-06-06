// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2016 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*       Tiffany Lin <tiffany.lin@mediatek.com>
*/

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <linux/iommu.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/semaphore.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_enc.h"
#include "mtk_vcodec_enc_pm.h"
#include "mtk_vcodec_enc_pm_plat.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"
#include "venc_drv_if.h"
#include "mtk-smmu-v3.h"
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#include "vcp_status.h"
#endif

module_param(mtk_v4l2_dbg_level, int, 0644); //S_IRUGO | S_IWUSR
module_param(mtk_vcodec_dbg, bool, 0644); //S_IRUGO | S_IWUSR
module_param(mtk_vcodec_vcp, int, 0644); //S_IRUGO | S_IWUSR
module_param(mtk_venc_acp_enable, bool, 0644);
module_param(mtk_venc_input_acp_enable, bool, 0644);
char mtk_venc_property_prev[LOG_PROPERTY_SIZE];
char mtk_venc_vcp_log_prev[LOG_PROPERTY_SIZE];

static struct mtk_vcodec_dev *dev_ptr;
static int mtk_vcodec_vcp_log_write(const char *val, const struct kernel_param *kp)
{
	if (!(val == NULL || strlen(val) == 0)) {
		mtk_v4l2_debug(0, "val: %s, len: %zu", val, strlen(val));
		mtk_vcodec_set_log(NULL, dev_ptr, val, MTK_VCODEC_LOG_INDEX_LOG, NULL);
	}
	return 0;
}
static struct kernel_param_ops vcodec_vcp_log_param_ops = {
	.set = mtk_vcodec_vcp_log_write,
};
module_param_cb(mtk_venc_vcp_log, &vcodec_vcp_log_param_ops, &mtk_venc_vcp_log, 0644);

static int mtk_vcodec_vcp_property_write(const char *val, const struct kernel_param *kp)
{
	if (!(val == NULL || strlen(val) == 0)) {
		mtk_v4l2_debug(0, "val: %s, len: %zu", val, strlen(val));
		mtk_vcodec_set_log(NULL, dev_ptr, val, MTK_VCODEC_LOG_INDEX_PROP, NULL);
	}
	return 0;
}
static struct kernel_param_ops vcodec_vcp_prop_param_ops = {
	.set = mtk_vcodec_vcp_property_write,
};
module_param_cb(mtk_venc_property, &vcodec_vcp_prop_param_ops, &mtk_venc_property, 0644);


static int fops_vcodec_open(struct file *file)
{
	struct mtk_vcodec_dev *dev = video_drvdata(file);
	struct mtk_vcodec_ctx *ctx = NULL;
	struct mtk_video_enc_buf *mtk_buf = NULL;
	struct vb2_queue *src_vq;
	int ret = 0;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mtk_buf = kzalloc(sizeof(*mtk_buf), GFP_KERNEL);
	if (!mtk_buf) {
		kfree(ctx);
		return -ENOMEM;
	}
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	if (mtk_vcodec_is_vcp(MTK_INST_ENCODER)) {
		ret = vcp_register_feature_ex(VENC_FEATURE_ID);
		if (ret) {
			mtk_v4l2_err("Failed to vcp_register_feature");
			kfree(ctx);
			kfree(mtk_buf);
			ctx = NULL;
			mtk_buf = NULL;
			return -EPERM;
		}
	}
#endif

	mutex_lock(&dev->dev_mutex);
	/*
	 * Use simple counter to uniquely identify this context. Only
	 * used for logging.
	 */
	ctx->enc_flush_buf = mtk_buf;
	dev->id_counter++;
	if (dev->id_counter == 0)
		dev->id_counter++;
	ctx->id = dev->id_counter;
	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);
	INIT_LIST_HEAD(&ctx->list);
	ctx->dev = dev;
	ctx->dev_ctx = &dev->dev_ctx;
	init_waitqueue_head(&ctx->queue[0]);
	init_waitqueue_head(&ctx->queue[1]);
	spin_lock_init(&ctx->state_lock);
	mutex_init(&ctx->buf_lock);
	mutex_init(&ctx->worker_lock);
	mutex_init(&ctx->hw_status);
	mutex_init(&ctx->q_mutex);
	mutex_init(&ctx->init_lock);
	mutex_init(&ctx->ipi_use_lock);
	mutex_init(&ctx->gen_buf_list_lock);

	ctx->type = MTK_INST_ENCODER;
	ret = mtk_vcodec_enc_ctrls_setup(ctx);
	if (ret) {
		mtk_v4l2_err("Failed to setup controls() (%d)",
					 ret);
		goto err_ctrls_setup;
	}
	ctx->m2m_ctx = v4l2_m2m_ctx_init(dev->m2m_dev_enc, ctx,
		&mtk_vcodec_enc_queue_init);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	if (dev->support_acp && mtk_venc_acp_enable && vcp_get_io_device_ex(VCP_IOMMU_ACP_VENC) != NULL) {
		ctx->general_dev = vcp_get_io_device_ex(VCP_IOMMU_ACP_VENC);
		mtk_v4l2_debug(4, "general buffer use VCP_IOMMU_ACP_VENC domain");
	} else {
		ctx->general_dev = vcp_get_io_device_ex(VCP_IOMMU_VENC);
		mtk_v4l2_debug(4, "general buffer use VCP_IOMMU_VENC domain");
	}
#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
	if (!ctx->general_dev) {
		ctx->general_dev = ctx->dev->smmu_dev;
		mtk_v4l2_debug(4, "general buffer use smmu_dev domain");
	}
#endif
#else
	ctx->general_dev = ctx->dev->smmu_dev;
#endif
	if (IS_ERR((__force void *)ctx->m2m_ctx)) {
		ret = PTR_ERR((__force void *)ctx->m2m_ctx);
		mtk_v4l2_err("Failed to v4l2_m2m_ctx_init() (%d)",
					 ret);
		goto err_m2m_ctx_init;
	}
	src_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
		V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

	ctx->enc_flush_buf->vb.vb2_buf.vb2_queue = src_vq;
	ctx->enc_flush_buf->lastframe = NON_EOS;
	ctx->enc_flush_buf->vb.vb2_buf.planes[0].bytesused = 1;
	mtk_vcodec_enc_set_default_params(ctx);

#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
	if (v4l2_fh_is_singular(&ctx->fh) && VCU_FPTR(vcu_load_firmware)) {
		/*
		 * vcu_load_firmware checks if it was loaded already and
		 * does nothing in that case
		 */
		ret = VCU_FPTR(vcu_load_firmware)(dev->vcu_plat_dev);
		if (ret < 0) {
			/*
			 * Return 0 if downloading firmware successfully,
			 * otherwise it is failed
			 */
			mtk_v4l2_err("vcu_load_firmware failed!");
			goto err_load_fw;
		}
	}
#endif
	mtk_v4l2_debug(2, "Create instance [%d]@%p m2m_ctx=%p ",
				   ctx->id, ctx, ctx->m2m_ctx);

	dev->enc_cnt++;

	mutex_unlock(&dev->dev_mutex);
	mtk_v4l2_debug(0, "%s encoder [%d][%d]", dev_name(&dev->plat_dev->dev),
				   ctx->id, dev->enc_cnt);
	return ret;

	/* Deinit when failure occurred */
#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
err_load_fw:
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	mtk_vcodec_del_ctx_list(ctx);
#endif
err_m2m_ctx_init:
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
err_ctrls_setup:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx->enc_flush_buf);
	kfree(ctx);
	mutex_unlock(&dev->dev_mutex);

	return ret;
}

static int fops_vcodec_release(struct file *file)
{
	struct mtk_vcodec_dev *dev = video_drvdata(file);
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(file->private_data);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	int ret = 0;
#endif

	mtk_v4l2_debug(0, "[%d][%d] encoder", ctx->id, dev->enc_cnt);
	mutex_lock(&dev->dev_mutex);

	/*
	 * Check no more ipi in progress, to avoid inst abort since vcp
	 * wdt (maybe cause by vdec) but still has ipi waiting timeout
	 */
	mutex_lock(&dev->ipi_mutex);
	mutex_unlock(&dev->ipi_mutex);

	mtk_vcodec_enc_empty_queues(file, ctx);
	mutex_lock(&ctx->worker_lock);
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	mutex_unlock(&ctx->worker_lock);
	mtk_vcodec_enc_release(ctx);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);

	kfree(ctx->enc_flush_buf);
	kfree(ctx);
	if (dev->enc_cnt > 0)
		dev->enc_cnt--;
	mutex_unlock(&dev->dev_mutex);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	if (mtk_vcodec_is_vcp(MTK_INST_ENCODER)) {
		ret = vcp_deregister_feature_ex(VENC_FEATURE_ID);
		if (ret) {
			mtk_v4l2_err("Failed to vcp_deregister_feature");
			return -EPERM;
		}
	}
#endif

	return 0;
}

static const struct v4l2_file_operations mtk_vcodec_fops = {
	.owner          = THIS_MODULE,
	.open           = fops_vcodec_open,
	.release        = fops_vcodec_release,
	.poll           = v4l2_m2m_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap           = v4l2_m2m_fop_mmap,
};

/**
 * Suspend callbacks after user space processes are frozen
 * Since user space processes are frozen, there is no need and cannot hold same
 * mutex that protects lock owner while checking status.
 * If video codec hardware is still active now, must not to enter suspend.
 **/
static int mtk_vcodec_enc_suspend(struct device *pDev)
{
	int val, i;
	struct mtk_vcodec_dev *dev = dev_get_drvdata(pDev);

	for (i = 0; i < MTK_VENC_HW_NUM; i++) {
		val = down_trylock(&dev->enc_sem[i]);
	if (val == 1) {
		mtk_v4l2_debug(0, "fail due to videocodec activity");
		return -EBUSY;
	}
		up(&dev->enc_sem[i]);
	}

	mtk_v4l2_debug(1, "done");
	return 0;
}

static int mtk_vcodec_enc_resume(struct device *pDev)
{
	mtk_v4l2_debug(1, "done");
	return 0;
}

static int mtk_vcodec_enc_suspend_notifier(struct notifier_block *nb,
					unsigned long action, void *data)
{
	int wait_cnt = 0;
	int val = 0;
	int i;
	struct mtk_vcodec_dev *dev =
		container_of(nb, struct mtk_vcodec_dev, pm_notifier);

	mtk_v4l2_debug(1, "action = %ld", action);
	switch (action) {
	case PM_SUSPEND_PREPARE:
#if !IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
		dev->is_codec_suspending = 1;
#endif
		for (i = 0; i < MTK_VENC_HW_NUM; i++) {
			val = down_trylock(&dev->enc_sem[i]);
			while (val == 1) {
				usleep_range(10000, 20000);
				wait_cnt++;
				/* Current task is still not finished, don't
				 * care, will check again in real suspend
				 */
				if (wait_cnt > 5) {
					mtk_v4l2_err("waiting fail");
					return NOTIFY_DONE;
				}
				val = down_trylock(&dev->enc_sem[i]);
			}
			up(&dev->enc_sem[i]);
		}
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
#if !IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
		dev->is_codec_suspending = 0;
#endif
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
}

static int mtk_vcodec_enc_probe(struct platform_device *pdev)
{
	struct mtk_vcodec_dev *dev;
	struct video_device *vfd_enc;
	struct resource *res;
	int i = 0, reg_index = 0, ret, slb_cpu_used_pref, slb_extra;
	int port_num[MTK_VENC_HW_NUM] = {0};
	const char *name = NULL;
	int port_args_num = 0, port_data_len = 0, total_port_num = 0;
	unsigned int offset = 0;
	unsigned int core_id = 0, ram_type = 0, port_id = 0;

	dev = devm_kzalloc(mtk_smmu_get_shared_device(&pdev->dev), sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	// init list head, mutex, spin_lock, atomic, etc.
	INIT_LIST_HEAD(&dev->ctx_list);
	for (i = 0; i < MTK_VENC_HW_NUM; i++) {
		sema_init(&dev->enc_sem[i], 1);
		spin_lock_init(&dev->enc_power_lock[i]);
		dev->enc_is_power_on[i] = false;
		atomic_set(&dev->smi_ctrl_get_ref_cnt[i], 0);
	}
	atomic_set(&dev->larb_ref_cnt, 0);
	atomic_set(&dev->smi_dump_ref_cnt, 0);
	mutex_init(&dev->ctx_mutex);
	mutex_init(&dev->dev_mutex);
	mutex_init(&dev->ipi_mutex);
	mutex_init(&dev->enc_dvfs_mutex);
	mutex_init(&dev->enc_qos_mutex);
	mutex_init(&dev->cpu_hint_mutex);
	spin_lock_init(&dev->irqlock);

	INIT_LIST_HEAD(&dev->log_param_list);
	INIT_LIST_HEAD(&dev->prop_param_list);
	mutex_init(&dev->log_param_mutex);
	mutex_init(&dev->prop_param_mutex);

	dev->plat_dev = pdev;
	dev->smmu_dev = mtk_smmu_get_shared_device(&pdev->dev);
	ret = of_property_read_u32(pdev->dev.of_node, "iommu-domain-switch", &dev->iommu_domain_swtich);
	if (ret != 0)
		dev->iommu_domain_swtich = 0;
	mtk_v4l2_debug(0, "iommu_domain_swtich: %d", dev->iommu_domain_swtich);
#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
	if (VCU_FPTR(vcu_get_plat_device)) {
		dev->vcu_plat_dev = VCU_FPTR(vcu_get_plat_device)(dev->plat_dev);
		if (dev->vcu_plat_dev == NULL) {
			mtk_v4l2_err("[VCU] vcu device in not ready");
			return -EPROBE_DEFER;
		}
	}
#endif
	ret = of_property_read_string(pdev->dev.of_node, "mediatek,platform", &dev->platform);
	if (ret != 0) {
		mtk_v4l2_err("failed to find mediatek,platform\n");
		return ret;
	}
	mtk_v4l2_debug(0, "%s", dev->platform);
	mtk_vcodec_get_chipid(&dev->chip_id);

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,ipm", &dev->venc_hw_ipm);
	if (ret != 0 || dev->venc_hw_ipm > VCODEC_IPM_MAX) {
		mtk_v4l2_debug(0, "default use ipm v1");
		dev->venc_hw_ipm = VCODEC_IPM_V1;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "venc-core-count", &dev->hw_max_count);
	if (ret != 0 || dev->hw_max_count > MTK_VENC_HW_NUM) {
		mtk_v4l2_debug(0, "default use 2 core");
		dev->hw_max_count = 2;
	}
	mtk_v4l2_debug(0, "hw ipm: %d, core count %d", dev->venc_hw_ipm, dev->hw_max_count);

	ret = mtk_vcodec_init_enc_pm(dev);
	if (ret < 0) {
		dev_info(&pdev->dev, "Failed to get mt vcodec clock source!");
		return ret;
	}

	for (i = 0; i < NUM_MAX_VENC_REG_BASE; i++)
		dev->enc_reg_base[i] = NULL;
	for (i = 0; !of_property_read_string_index(pdev->dev.of_node, "reg-names", i, &name); i++) {
		if (!strcmp(MTK_VDEC_REG_NAME_VENC_SYS, name)) {
			reg_index = VENC_SYS;
		} else if (!strcmp(MTK_VDEC_REG_NAME_VENC_C1_SYS, name)) {
			reg_index = VENC_C1_SYS;
		} else if (!strcmp(MTK_VDEC_REG_NAME_VENC_C2_SYS, name)) {
			reg_index = VENC_C2_SYS;
		} else if (!strcmp(MTK_VDEC_REG_NAME_VENC_GCON, name)) {
			reg_index = VENC_GCON;
		} else {
			dev_info(&pdev->dev, "invalid reg name: %s, index: %d", name, i);
			return -EINVAL;
		}

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (i == VENC_SYS && res == NULL) {
			dev_info(&pdev->dev,
				"get memory resource failed. idx:%d", i);
			ret = -ENXIO;
			goto err_res;
		} else if (res == NULL) {
			mtk_v4l2_debug(0, "try next resource. idx:%d", i);
			continue;
		}

		dev->enc_reg_base[reg_index] =
			devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR((__force void *)dev->enc_reg_base[reg_index])) {
			ret = PTR_ERR(
				(__force void *)dev->enc_reg_base[reg_index]);
			goto err_res;
		}
		mtk_v4l2_debug(2, "reg[%d] base=0x%lx",
			reg_index, (unsigned long)dev->enc_reg_base[reg_index]);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "support-wfd-region", &support_wfd_region);
	if (ret) {
		mtk_v4l2_debug(0, "[VENC] Cannot get support-wfd-region, skip");
		support_wfd_region = 0;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "venc-disable-hw-break", &venc_disable_hw_break);
	if (ret) {
		mtk_v4l2_debug(0, "[VENC] default enable venc hw break");
		venc_disable_hw_break = 0;
	} else
		mtk_v4l2_debug(0, "[VENC] %s venc hw break", venc_disable_hw_break ? "disable" : "enable");

	ret = mtk_vcodec_enc_irq_setup(pdev, dev);
	if (ret)
		goto err_res;

	ret = of_property_read_u32(pdev->dev.of_node, "port-arg-num", &port_args_num);
	if (ret != 0)
		dev_info(&pdev->dev, "Failed to get port_arg_num!");

	pr_info("after get port_arg_num %d\n", port_args_num);
	if (!of_get_property(pdev->dev.of_node, "port-def", &port_data_len))
		dev_info(&pdev->dev, "Failed to get port-def!");

	pr_info("after get port-def port_data_len %d\n", port_data_len);
	if (port_args_num)
		total_port_num = port_data_len / (sizeof(u32) * port_args_num);

	for (i = 0; i < total_port_num; i++) {
		offset = i * port_args_num;
		if (of_property_read_u32_index(pdev->dev.of_node, "port-def",
					offset, &core_id)) {
			dev_info(&pdev->dev, "fail core id offset %d i %d!", offset, i);
			goto err_res;
		}
		if (of_property_read_u32_index(pdev->dev.of_node, "port-def",
					offset + 1, &port_id)) {
			dev_info(&pdev->dev, "fail port id offset %d i %d!", offset, i);
			goto err_res;
		}
		if (of_property_read_u32_index(pdev->dev.of_node, "port-def",
					offset + 2, &ram_type)) {
			dev_info(&pdev->dev, "fail ram type offset %d i %d!", offset, i);
			goto err_res;
		}

		if (core_id < MTK_VENC_HW_NUM) {
			dev->venc_ports[core_id].port_id[port_num[core_id]] = port_id;
			dev->venc_ports[core_id].ram_type[port_num[core_id]] = ram_type;
			port_num[core_id]++;
		}

	}
	for (i = 0; i < MTK_VENC_HW_NUM; i++) {
		dev->venc_ports[i].total_port_num = port_num[i];
		pr_info("after get port-def  port num [%d] %d\n", i, port_num[i]);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "venc-slb-cpu-used-perf", &slb_cpu_used_pref);
	if (ret != 0)
		dev_info(&pdev->dev, "Failed to get venc-slb-cpu-used-perf!");

	dev->enc_slb_cpu_used_perf = slb_cpu_used_pref;
	pr_info("after get venc-slb-cpu-used-perf %d\n", slb_cpu_used_pref);

	ret = of_property_read_u32(pdev->dev.of_node, "venc-slb-extra", &slb_extra);
	if (ret != 0)
		dev_info(&pdev->dev, "Failed to get venc-slb-extra!");

	dev->enc_slb_extra = slb_extra;
	pr_info("after get venc-slb-extra %d\n", slb_extra);

	ret = of_property_read_u32(pdev->dev.of_node, "support-acp", &dev->support_acp);
	if (ret != 0)
		dev->support_acp = 0;

	if (dev->support_acp && !mtk_venc_acp_enable) {
		mtk_venc_acp_enable = of_property_read_bool(pdev->dev.of_node, "enable-acp");
		mtk_venc_input_acp_enable = mtk_venc_acp_enable;
	}
	if (dev->support_acp && !mtk_venc_acp_enable &&
	    of_property_read_bool(pdev->dev.of_node, "enable-acp-by-sw-ver")) {
		unsigned int sw_ver;

		ret = of_property_read_u32(pdev->dev.of_node, "enable-acp-by-sw-ver", &sw_ver);
		if (!ret && dev->chip_id.sw_ver == sw_ver)
			mtk_venc_input_acp_enable = mtk_venc_acp_enable = true;
	}

	mtk_v4l2_debug(0, "venc slb_cpu_used_pref %d, slb_extra %d, support acp %d, mtk_venc_acp_enable %d, mtk_venc_input_acp_enable %d",
		dev->enc_slb_cpu_used_perf, dev->enc_slb_extra,
		dev->support_acp, mtk_venc_acp_enable, mtk_venc_input_acp_enable);

	SNPRINTF(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "%s",
			 "[MTK_V4L2_VENC]");

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret) {
		mtk_v4l2_err("v4l2_device_register err=%d", ret);
		goto err_res;
	}

	/* allocate video device for encoder and register it */
	vfd_enc = video_device_alloc();
	if (!vfd_enc) {
		mtk_v4l2_err("Failed to allocate video device");
		ret = -ENOMEM;
		goto err_enc_alloc;
	}
	vfd_enc->fops           = &mtk_vcodec_fops;
	vfd_enc->ioctl_ops      = &mtk_venc_ioctl_ops;
	vfd_enc->release        = video_device_release;
	vfd_enc->lock           = &dev->dev_mutex;
	vfd_enc->v4l2_dev       = &dev->v4l2_dev;
	vfd_enc->vfl_dir        = VFL_DIR_M2M;
	vfd_enc->device_caps    = V4L2_CAP_VIDEO_M2M_MPLANE |
							  V4L2_CAP_STREAMING;

	SNPRINTF(vfd_enc->name, sizeof(vfd_enc->name), "%s",
			 MTK_VCODEC_ENC_NAME);
	video_set_drvdata(vfd_enc, dev);
	dev->vfd_enc = vfd_enc;
	platform_set_drvdata(pdev, dev);

	dev->m2m_dev_enc = v4l2_m2m_init(&mtk_venc_m2m_ops);
	if (IS_ERR((__force void *)dev->m2m_dev_enc)) {
		mtk_v4l2_err("Failed to init mem2mem enc device");
		ret = PTR_ERR((__force void *)dev->m2m_dev_enc);
		goto err_enc_mem_init;
	}

	dev->encode_workqueue =
		alloc_ordered_workqueue(MTK_VCODEC_ENC_NAME,
								WQ_MEM_RECLAIM |
								WQ_FREEZABLE);
	if (!dev->encode_workqueue) {
		mtk_v4l2_err("Failed to create encode workqueue");
		ret = -EINVAL;
		goto err_event_workq;
	}

	ret = video_register_device(vfd_enc, VFL_TYPE_VIDEO, -1);
	if (ret) {
		mtk_v4l2_err("Failed to register video device");
		goto err_enc_reg;
	}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_IOMMU)
	dev->io_domain = iommu_get_domain_for_dev(dev->smmu_dev);
	if (dev->io_domain == NULL) {
		mtk_v4l2_err("Failed to get io_domain\n");
		return -EPROBE_DEFER;
	}

	ret = dma_set_mask_and_coherent(dev->smmu_dev, DMA_BIT_MASK(34));
	if (ret) {
		ret = dma_set_mask_and_coherent(dev->smmu_dev, DMA_BIT_MASK(34));
		if (ret) {
			dev_info(&pdev->dev, "64-bit DMA enable failed\n");
			return ret;
		}
	}
	if (!pdev->dev.dma_parms) {
		pdev->dev.dma_parms =
			devm_kzalloc(dev->smmu_dev, sizeof(*pdev->dev.dma_parms), GFP_KERNEL);
	}
	if (pdev->dev.dma_parms) {
		ret = dma_set_max_seg_size(dev->smmu_dev, (unsigned int)DMA_BIT_MASK(34));
		if (ret)
			dev_info(&pdev->dev, "Failed to set DMA segment size\n");
	}
#endif
	mtk_v4l2_debug(0, "encoder registered as /dev/video%d",
				   vfd_enc->num);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_IOMMU)
	mtk_venc_translation_fault_callback_setting(dev);
#endif

#if IS_ENABLED(CONFIG_MTK_EMI) && !IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
	mtk_venc_violation_fault_callback_setting(dev);
#endif
	mtk_prepare_venc_dvfs(dev);
	mtk_prepare_venc_emi_bw(dev);
	dev->pm_notifier.notifier_call = mtk_vcodec_enc_suspend_notifier;
	register_pm_notifier(&dev->pm_notifier);
	dev->is_codec_suspending = 0;
	dev->enc_cnt = 0;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
	venc_vcp_probe(dev);

	ret = of_property_read_u32(pdev->dev.of_node, "venc-power-in-vcp", &dev->power_in_vcp);
	if (ret != 0)
		dev->power_in_vcp = 0;
#endif

	mtk_vcodec_enc_smi_pwr_ctrl_register(dev);

	ret = venc_if_dev_ctx_init(dev);
	if (ret) {
		mtk_v4l2_err("Failed to init dev ctx (ret %d)", ret);
		goto err_enc_reg;
	}

	dev_ptr = dev;
	mtk_vcodec_set_dev(dev, MTK_INST_ENCODER);

	return 0;

err_enc_reg:
	destroy_workqueue(dev->encode_workqueue);
err_event_workq:
	v4l2_m2m_release(dev->m2m_dev_enc);
err_enc_mem_init:
	video_unregister_device(vfd_enc);
err_enc_alloc:
	v4l2_device_unregister(&dev->v4l2_dev);
err_res:
	mtk_vcodec_release_enc_pm(dev);
	return ret;
}

static const struct of_device_id mtk_vcodec_enc_match[] = {
	{.compatible = "mediatek,mt8173-vcodec-enc",},
	{.compatible = "mediatek,mt2712-vcodec-enc",},
	{.compatible = "mediatek,mt8167-vcodec-enc",},
	{.compatible = "mediatek,mt6771-vcodec-enc",},
	{.compatible = "mediatek,mt6885-vcodec-enc",},
	{.compatible = "mediatek,mt6873-vcodec-enc",},
	{.compatible = "mediatek,mt6853-vcodec-enc",},
	{.compatible = "mediatek,mt6983-vcodec-enc",},
	{.compatible = "mediatek,mt6879-vcodec-enc",},
	{.compatible = "mediatek,mt6895-vcodec-enc",},
	{.compatible = "mediatek,mt6855-vcodec-enc",},
	{.compatible = "mediatek,mt6985-vcodec-enc",},
	{.compatible = "mediatek,mt6886-vcodec-enc",},
	{.compatible = "mediatek,mt8195-vcodec-enc",},
	{.compatible = "mediatek,mt6835-vcodec-enc",},
	{.compatible = "mediatek,mt6897-vcodec-enc",},
	{.compatible = "mediatek,mt6989-vcodec-enc",},
	{.compatible = "mediatek,mt6899-vcodec-enc",},
	{.compatible = "mediatek,mt6991-vcodec-enc",},
	{.compatible = "mediatek,mt6768-vcodec-enc",},
	{.compatible = "mediatek,mt6877-vcodec-enc",},
	{.compatible = "mediatek,mt6833-vcodec-enc",},
	{.compatible = "mediatek,mt6781-vcodec-enc",},
	{.compatible = "mediatek,venc_gcon",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_vcodec_enc_match);

static int mtk_vcodec_enc_remove(struct platform_device *pdev)
{
	struct mtk_vcodec_dev *dev = platform_get_drvdata(pdev);

	venc_if_dev_ctx_deinit(dev);

	mtk_unprepare_venc_emi_bw(dev);
	mtk_unprepare_venc_dvfs(dev);

	mtk_v4l2_debug_enter();
	flush_workqueue(dev->encode_workqueue);
	destroy_workqueue(dev->encode_workqueue);
	if (dev->m2m_dev_enc)
		v4l2_m2m_release(dev->m2m_dev_enc);

	if (dev->vfd_enc)
		video_unregister_device(dev->vfd_enc);

	v4l2_device_unregister(&dev->v4l2_dev);
	mtk_vcodec_enc_smi_pwr_ctrl_unregister(dev);
	mtk_vcodec_release_enc_pm(dev);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
		venc_vcp_remove(dev);
#endif
	return 0;
}

static const struct dev_pm_ops mtk_vcodec_enc_pm_ops = {
	.suspend = mtk_vcodec_enc_suspend,
	.resume = mtk_vcodec_enc_resume,
};

static struct platform_driver mtk_vcodec_enc_driver = {
	.probe  = mtk_vcodec_enc_probe,
	.remove = mtk_vcodec_enc_remove,
	.driver = {
		.name   = MTK_VCODEC_ENC_NAME,
		.pm = &mtk_vcodec_enc_pm_ops,
		.of_match_table = mtk_vcodec_enc_match,
	},
};

module_platform_driver(mtk_vcodec_enc_driver);


MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek video codec V4L2 encoder driver");
