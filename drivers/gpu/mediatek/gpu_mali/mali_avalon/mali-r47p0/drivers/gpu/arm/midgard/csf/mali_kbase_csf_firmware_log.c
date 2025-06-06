// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2021-2023 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

#include <mali_kbase.h>
#include "backend/gpu/mali_kbase_pm_internal.h"
#include <csf/mali_kbase_csf_firmware_log.h>
#include <csf/mali_kbase_csf_trace_buffer.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_MALI_MTK_KE_DUMP_FWLOG)
/* csffw reserved memory */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/proc_fs.h>
#include <linux/sysfs.h>
#include <platform/mtk_platform_common.h>

/* for fwlog get latest 16kb data */
#define FWLOG_DEFAULT_CONTENT_LEN 64
struct device_node *g_rmem_node = NULL;
struct reserved_mem *g_fwlogdump = NULL;
u8 *g_fw_dump_dest = NULL;
static char fw_default_content[FWLOG_DEFAULT_CONTENT_LEN] = "==== CSF fwlog is empy! ====";
#endif /* CONFIG_MALI_MTK_KE_DUMP_FWLOG */

/*
 * ARMv7 instruction: Branch with Link calls a subroutine at a PC-relative address.
 */
#define ARMV7_T1_BL_IMM_INSTR 0xd800f000

/*
 * ARMv7 instruction: Branch with Link calls a subroutine at a PC-relative address, maximum
 * negative jump offset.
 */
#define ARMV7_T1_BL_IMM_RANGE_MIN -16777216

/*
 * ARMv7 instruction: Branch with Link calls a subroutine at a PC-relative address, maximum
 * positive jump offset.
 */
#define ARMV7_T1_BL_IMM_RANGE_MAX 16777214

/*
 * ARMv7 instruction: Double NOP instructions.
 */
#define ARMV7_DOUBLE_NOP_INSTR 0xbf00bf00

#if defined(CONFIG_DEBUG_FS)

static int kbase_csf_firmware_log_enable_mask_read(void *data, u64 *val)
{
	struct kbase_device *kbdev = (struct kbase_device *)data;
	struct firmware_trace_buffer *tb =
		kbase_csf_firmware_get_trace_buffer(kbdev, KBASE_CSFFW_LOG_BUF_NAME);

	if (tb == NULL) {
		dev_err(kbdev->dev, "Couldn't get the firmware trace buffer");
		return -EIO;
	}
	/* The enabled traces limited to u64 here, regarded practical */
	*val = kbase_csf_firmware_trace_buffer_get_active_mask64(tb);
	return 0;
}

static int kbase_csf_firmware_log_enable_mask_write(void *data, u64 val)
{
	struct kbase_device *kbdev = (struct kbase_device *)data;
	struct firmware_trace_buffer *tb =
		kbase_csf_firmware_get_trace_buffer(kbdev, KBASE_CSFFW_LOG_BUF_NAME);
	u64 new_mask;
	unsigned int enable_bits_count;

	if (tb == NULL) {
		dev_err(kbdev->dev, "Couldn't get the firmware trace buffer");
		return -EIO;
	}

	/* Ignore unsupported types */
	enable_bits_count = kbase_csf_firmware_trace_buffer_get_trace_enable_bits_count(tb);
	if (enable_bits_count > 64) {
		dev_dbg(kbdev->dev, "Limit enabled bits count from %u to 64", enable_bits_count);
		enable_bits_count = 64;
	}
	new_mask = val & (UINT64_MAX >> (64 - enable_bits_count));

	if (new_mask != kbase_csf_firmware_trace_buffer_get_active_mask64(tb))
		return kbase_csf_firmware_trace_buffer_set_active_mask64(tb, new_mask);
	else
		return 0;
}

static int kbasep_csf_firmware_log_debugfs_open(struct inode *in, struct file *file)
{
	struct kbase_device *kbdev = in->i_private;

	file->private_data = kbdev;
	dev_dbg(kbdev->dev, "Opened firmware trace buffer dump debugfs file");

	return 0;
}

static ssize_t kbasep_csf_firmware_log_debugfs_read(struct file *file, char __user *buf,
						    size_t size, loff_t *ppos)
{
	struct kbase_device *kbdev = file->private_data;
	struct kbase_csf_firmware_log *fw_log = &kbdev->csf.fw_log;
	unsigned int n_read;
	unsigned long not_copied;
	/* Limit reads to the kernel dump buffer size */
	size_t mem = MIN(size, FIRMWARE_LOG_DUMP_BUF_SIZE);
	int ret;

	struct firmware_trace_buffer *tb =
		kbase_csf_firmware_get_trace_buffer(kbdev, KBASE_CSFFW_LOG_BUF_NAME);

	if (tb == NULL) {
		dev_err(kbdev->dev, "Couldn't get the firmware trace buffer");
		return -EIO;
	}

	if (atomic_cmpxchg(&fw_log->busy, 0, 1) != 0)
		return -EBUSY;

	/* Reading from userspace is only allowed in manual mode or auto-discard mode */
	if (fw_log->mode != KBASE_CSF_FIRMWARE_LOG_MODE_MANUAL &&
	    fw_log->mode != KBASE_CSF_FIRMWARE_LOG_MODE_AUTO_DISCARD) {
		ret = -EINVAL;
		goto out;
	}

	n_read = kbase_csf_firmware_trace_buffer_read_data(tb, fw_log->dump_buf, mem);

	/* Do the copy, if we have obtained some trace data */
	not_copied = (n_read) ? copy_to_user(buf, fw_log->dump_buf, n_read) : 0;

	if (not_copied) {
		dev_err(kbdev->dev, "Couldn't copy trace buffer data to user space buffer");
		ret = -EFAULT;
		goto out;
	}

	*ppos += n_read;
	ret = (int)n_read;

out:
	atomic_set(&fw_log->busy, 0);
	return ret;
}

static int kbase_csf_firmware_log_mode_read(void *data, u64 *val)
{
	struct kbase_device *kbdev = (struct kbase_device *)data;
	struct kbase_csf_firmware_log *fw_log = &kbdev->csf.fw_log;

	*val = fw_log->mode;
	return 0;
}

static int kbase_csf_firmware_log_mode_write(void *data, u64 val)
{
	struct kbase_device *kbdev = (struct kbase_device *)data;
	struct kbase_csf_firmware_log *fw_log = &kbdev->csf.fw_log;
	int ret = 0;

	if (atomic_cmpxchg(&fw_log->busy, 0, 1) != 0)
		return -EBUSY;

	if (val == fw_log->mode)
		goto out;

	switch (val) {
	case KBASE_CSF_FIRMWARE_LOG_MODE_MANUAL:
		cancel_delayed_work_sync(&fw_log->poll_work);
		break;
	case KBASE_CSF_FIRMWARE_LOG_MODE_AUTO_PRINT:
	case KBASE_CSF_FIRMWARE_LOG_MODE_AUTO_DISCARD:
		schedule_delayed_work(
			&fw_log->poll_work,
			msecs_to_jiffies((unsigned int)atomic_read(&fw_log->poll_period_ms)));
		break;
	default:
		ret = -EINVAL;
		goto out;
	}

	fw_log->mode = val;

out:
	atomic_set(&fw_log->busy, 0);
	return ret;
}

static int kbase_csf_firmware_log_poll_period_read(void *data, u64 *val)
{
	struct kbase_device *kbdev = (struct kbase_device *)data;
	struct kbase_csf_firmware_log *fw_log = &kbdev->csf.fw_log;

	*val = (u64)atomic_read(&fw_log->poll_period_ms);
	return 0;
}

static int kbase_csf_firmware_log_poll_period_write(void *data, u64 val)
{
	struct kbase_device *kbdev = (struct kbase_device *)data;
	struct kbase_csf_firmware_log *fw_log = &kbdev->csf.fw_log;

	atomic_set(&fw_log->poll_period_ms, val);
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(kbase_csf_firmware_log_enable_mask_fops,
			 kbase_csf_firmware_log_enable_mask_read,
			 kbase_csf_firmware_log_enable_mask_write, "%llx\n");

static const struct file_operations kbasep_csf_firmware_log_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = kbasep_csf_firmware_log_debugfs_open,
	.read = kbasep_csf_firmware_log_debugfs_read,
	.llseek = no_llseek,
};

DEFINE_DEBUGFS_ATTRIBUTE(kbase_csf_firmware_log_mode_fops, kbase_csf_firmware_log_mode_read,
			 kbase_csf_firmware_log_mode_write, "%llu\n");
DEFINE_DEBUGFS_ATTRIBUTE(kbase_csf_firmware_log_poll_period_fops,
			 kbase_csf_firmware_log_poll_period_read,
			 kbase_csf_firmware_log_poll_period_write, "%llu\n");

#endif /* CONFIG_DEBUG_FS */

static void kbase_csf_firmware_log_discard_buffer(struct kbase_device *kbdev)
{
	struct kbase_csf_firmware_log *fw_log = &kbdev->csf.fw_log;
	struct firmware_trace_buffer *tb =
		kbase_csf_firmware_get_trace_buffer(kbdev, KBASE_CSFFW_LOG_BUF_NAME);

	if (tb == NULL) {
		dev_dbg(kbdev->dev, "Can't get the trace buffer, firmware log discard skipped");
		return;
	}

	if (atomic_cmpxchg(&fw_log->busy, 0, 1) != 0)
		return;

	kbase_csf_firmware_trace_buffer_discard(tb);

	atomic_set(&fw_log->busy, 0);
}

static void kbase_csf_firmware_log_poll(struct work_struct *work)
{
	struct kbase_device *kbdev =
		container_of(work, struct kbase_device, csf.fw_log.poll_work.work);
	struct kbase_csf_firmware_log *fw_log = &kbdev->csf.fw_log;

	if (fw_log->mode == KBASE_CSF_FIRMWARE_LOG_MODE_AUTO_PRINT)
		kbase_csf_firmware_log_dump_buffer(kbdev);
	else if (fw_log->mode == KBASE_CSF_FIRMWARE_LOG_MODE_AUTO_DISCARD)
		kbase_csf_firmware_log_discard_buffer(kbdev);
	else
		return;

	schedule_delayed_work(&fw_log->poll_work,
			      msecs_to_jiffies((unsigned int)atomic_read(&fw_log->poll_period_ms)));
}

#if IS_ENABLED(CONFIG_MALI_MTK_KE_DUMP_FWLOG)
static ssize_t mtk_fwlog_enable_mask_show(struct device * const dev,
                struct device_attribute * const attr, char * const buf)
{
	struct kbase_device *const kbdev = dev_get_drvdata(dev);
	int err;
	u64 val;
	struct firmware_trace_buffer *tb =
		kbase_csf_firmware_get_trace_buffer(kbdev, KBASE_CSFFW_LOG_BUF_NAME);

	if (IS_ERR_OR_NULL(kbdev))
		return -ENODEV;

	if (tb == NULL) {
		dev_info(kbdev->dev, "Couldn't get the firmware trace buffer");
		return -EIO;
	}

	/* The enabled traces limited to u64 here, regarded practical */
	val = kbase_csf_firmware_trace_buffer_get_active_mask64(tb);

	err = scnprintf(buf, PAGE_SIZE, "0x%016llx\n", val);

	return err;
}

static ssize_t mtk_fwlog_enable_mask_store(struct device * const dev,
                struct device_attribute * const attr, const char * const buf,
                size_t const count)
{
	struct kbase_device *const kbdev = dev_get_drvdata(dev);
	struct firmware_trace_buffer *tb =
		kbase_csf_firmware_get_trace_buffer(kbdev, KBASE_CSFFW_LOG_BUF_NAME);
	u64 new_mask, val;
	unsigned int enable_bits_count;

	if (IS_ERR_OR_NULL(kbdev))
		return -ENODEV;

	if (tb == NULL) {
		dev_info(kbdev->dev, "Couldn't get the firmware trace buffer");
		return -EIO;
	}

	if (kstrtoull(buf, 0, &val) == 0) {
		dev_info(kbdev->dev, "[CSFFW] enable bit set to 0x%016llx\n\r\n\r", val);
	} else {
		dev_info(kbdev->dev, "[CSFFW] enable bit set fail.");
		return count;
	}

	/* Ignore unsupported types */
	enable_bits_count = kbase_csf_firmware_trace_buffer_get_trace_enable_bits_count(tb);
	if (enable_bits_count > 64) {
		dev_info(kbdev->dev, "Limit enabled bits count from %u to 64", enable_bits_count);
		enable_bits_count = 64;
	}
	new_mask = val & ((1ULL << enable_bits_count) - 1ULL);

	if (new_mask != kbase_csf_firmware_trace_buffer_get_active_mask64(tb))
		kbase_csf_firmware_trace_buffer_set_active_mask64(tb, new_mask);

	return count;
}
static DEVICE_ATTR_RW(mtk_fwlog_enable_mask);

ssize_t mtk_fwlog_proc_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct kbase_device *kbdev = (struct kbase_device *)mtk_common_get_kbdev();
	struct kbase_csf_firmware_log *fw_log = &kbdev->csf.fw_log;
	unsigned int n_read;
	unsigned long not_copied;
	/* Limit reads to the kernel dump buffer count */
	size_t mem = MIN(count, FIRMWARE_LOG_DUMP_BUF_SIZE);
	int ret;
	struct firmware_trace_buffer *tb =
		kbase_csf_firmware_get_trace_buffer(kbdev, KBASE_CSFFW_LOG_BUF_NAME);

	if (tb == NULL) {
		dev_info(kbdev->dev, "Couldn't get the firmware trace buffer");
		return -EIO;
	}

	if (atomic_cmpxchg(&fw_log->busy, 0, 1) != 0)
		return -EBUSY;

	/* Reading from userspace is only allowed in manual mode */
	if (fw_log->mode != KBASE_CSF_FIRMWARE_LOG_MODE_MANUAL) {
		ret = -EINVAL;
		goto out;
	}

	n_read = kbase_csf_firmware_trace_buffer_read_data(tb, fw_log->dump_buf, mem);

	/* Do the copy, if we have obtained some trace data */
	not_copied = (n_read) ? copy_to_user(buf, fw_log->dump_buf, n_read) : 0;

	if (not_copied) {
		dev_info(kbdev->dev, "Couldn't copy trace buffer data to user space buffer");
		ret = -EFAULT;
		goto out;
	}

	*f_pos += n_read;
	ret = n_read;

out:
	atomic_set(&fw_log->busy, 0);
	return ret;
}

ssize_t mtk_fwlog_proc_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *f_pos)
{
	return count;
}

static const struct  proc_ops mtk_fwlog_proc_ops = {
	.proc_read = mtk_fwlog_proc_read,
	.proc_write = mtk_fwlog_proc_write,
};
#endif /* CONFIG_MALI_MTK_KE_DUMP_FWLOG */

int kbase_csf_firmware_log_init(struct kbase_device *kbdev)
{
	struct kbase_csf_firmware_log *fw_log = &kbdev->csf.fw_log;
	int err = 0;
#if defined(CONFIG_DEBUG_FS)
	struct dentry *dentry;
#endif /* CONFIG_DEBUG_FS */

	/* Add one byte for null-termination */
	fw_log->dump_buf = kmalloc(FIRMWARE_LOG_DUMP_BUF_SIZE + 1, GFP_KERNEL);
	if (fw_log->dump_buf == NULL) {
		err = -ENOMEM;
		goto out;
	}

	/* Ensure null-termination for all strings */
	fw_log->dump_buf[FIRMWARE_LOG_DUMP_BUF_SIZE] = 0;

	/* Set default log polling period */
	atomic_set(&fw_log->poll_period_ms, KBASE_CSF_FIRMWARE_LOG_POLL_PERIOD_MS_DEFAULT);

	INIT_DEFERRABLE_WORK(&fw_log->poll_work, kbase_csf_firmware_log_poll);
#ifdef CONFIG_MALI_FW_TRACE_MODE_AUTO_DISCARD
	fw_log->mode = KBASE_CSF_FIRMWARE_LOG_MODE_AUTO_DISCARD;
	schedule_delayed_work(&fw_log->poll_work,
			      msecs_to_jiffies(KBASE_CSF_FIRMWARE_LOG_POLL_PERIOD_MS_DEFAULT));
#elif defined(CONFIG_MALI_FW_TRACE_MODE_AUTO_PRINT)
	fw_log->mode = KBASE_CSF_FIRMWARE_LOG_MODE_AUTO_PRINT;
	schedule_delayed_work(&fw_log->poll_work,
			      msecs_to_jiffies(KBASE_CSF_FIRMWARE_LOG_POLL_PERIOD_MS_DEFAULT));
#else /* CONFIG_MALI_FW_TRACE_MODE_MANUAL */
	fw_log->mode = KBASE_CSF_FIRMWARE_LOG_MODE_MANUAL;
#endif

	atomic_set(&fw_log->busy, 0);

#if !defined(CONFIG_DEBUG_FS)
	return 0;
#else /* !CONFIG_DEBUG_FS */
	dentry = debugfs_create_file("fw_trace_enable_mask", 0644, kbdev->mali_debugfs_directory,
				     kbdev, &kbase_csf_firmware_log_enable_mask_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		dev_err(kbdev->dev, "Unable to create fw_trace_enable_mask\n");
		err = -ENOENT;
		goto free_out;
	}
	dentry = debugfs_create_file("fw_traces", 0444, kbdev->mali_debugfs_directory, kbdev,
				     &kbasep_csf_firmware_log_debugfs_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		dev_err(kbdev->dev, "Unable to create fw_traces\n");
		err = -ENOENT;
		goto free_out;
	}
	dentry = debugfs_create_file("fw_trace_mode", 0644, kbdev->mali_debugfs_directory, kbdev,
				     &kbase_csf_firmware_log_mode_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		dev_err(kbdev->dev, "Unable to create fw_trace_mode\n");
		err = -ENOENT;
		goto free_out;
	}
	dentry = debugfs_create_file("fw_trace_poll_period_ms", 0644, kbdev->mali_debugfs_directory,
				     kbdev, &kbase_csf_firmware_log_poll_period_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		dev_err(kbdev->dev, "Unable to create fw_trace_poll_period_ms");
		err = -ENOENT;
		goto free_out;
	}

#if IS_ENABLED(CONFIG_MALI_MTK_KE_DUMP_FWLOG)

#if IS_ENABLED(CONFIG_MTK_GPU_DIAGNOSIS_DEBUG)
#else /* only to create node in user load */
	if (sysfs_create_file(&kbdev->dev->kobj,&dev_attr_mtk_fwlog_enable_mask.attr)) {
		dev_info(kbdev->dev, "SysFS file fwlog enable mask creation failed\n");
	}
	if (!proc_create("mtk_mali/fwlog", 0444, NULL, &mtk_fwlog_proc_ops)) {
		dev_info(kbdev->dev, "proc file fwlog creation failed\n");
	}
#endif /*CONFIG_MTK_GPU_DIAGNOSIS_DEBUG*/

	g_rmem_node = of_find_compatible_node(NULL, NULL, "mediatek,me_CSFFWLOG_reserved");
	if(!g_rmem_node) {
		dev_info(kbdev->dev, "[CSFFWLOG] no node for reserved memory\n");
	} else {
		g_fwlogdump = of_reserved_mem_lookup(g_rmem_node);
		if(!g_fwlogdump) {
			dev_info(kbdev->dev, "[CSFFWLOG] cannot lookup reserved memory\n");
		} else {
			g_fw_dump_dest = ioremap_wc(g_fwlogdump->base, g_fwlogdump->size);
			dev_info(kbdev->dev,
				"[me_CSFFWLOG_reserved] phys = 0x%llx, size = %llu\n",
				g_fwlogdump->base, g_fwlogdump->size);

			/* printf "==== CSF fwlog is empy! ====" at START of SYS_MALI_CSFFW_LOG */
			memcpy(g_fw_dump_dest, fw_default_content, FWLOG_DEFAULT_CONTENT_LEN);
		}
	}
#endif /* CONFIG_MALI_MTK_KE_DUMP_FWLOG */

	return 0;

free_out:
	kfree(fw_log->dump_buf);
	fw_log->dump_buf = NULL;
#endif /* CONFIG_DEBUG_FS */
out:
	return err;
}

void kbase_csf_firmware_log_term(struct kbase_device *kbdev)
{
	struct kbase_csf_firmware_log *fw_log = &kbdev->csf.fw_log;

	if (fw_log->dump_buf) {
		cancel_delayed_work_sync(&fw_log->poll_work);
		kfree(fw_log->dump_buf);
		fw_log->dump_buf = NULL;
	}
}

void kbase_csf_firmware_log_dump_buffer(struct kbase_device *kbdev)
{
	struct kbase_csf_firmware_log *fw_log = &kbdev->csf.fw_log;
	u8 *buf = fw_log->dump_buf, *p, *pnewline, *pend, *pendbuf;
	unsigned int read_size, remaining_size;
	struct firmware_trace_buffer *tb =
		kbase_csf_firmware_get_trace_buffer(kbdev, KBASE_CSFFW_LOG_BUF_NAME);

	if (tb == NULL) {
		dev_dbg(kbdev->dev, "Can't get the trace buffer, firmware trace dump skipped");
		return;
	}

	if (atomic_cmpxchg(&fw_log->busy, 0, 1) != 0)
		return;

	/* FW should only print complete messages, so there's no need to handle
	 * partial messages over multiple invocations of this function
	 */

	p = buf;
	pendbuf = &buf[FIRMWARE_LOG_DUMP_BUF_SIZE];

	while ((read_size = kbase_csf_firmware_trace_buffer_read_data(tb, p, pendbuf - p))) {
		pend = p + read_size;
		p = buf;

		while (p < pend && (pnewline = memchr(p, '\n', (size_t)(pend - p)))) {
			/* Null-terminate the string */
			*pnewline = 0;

			dev_err(kbdev->dev, "FW> %s", p);

			p = pnewline + 1;
		}

		remaining_size = pend - p;

		if (!remaining_size) {
			p = buf;
		} else if (remaining_size < FIRMWARE_LOG_DUMP_BUF_SIZE) {
			/* Copy unfinished string to the start of the buffer */
			memmove(buf, p, remaining_size);
			p = &buf[remaining_size];
		} else {
			/* Print abnormally long string without newlines */
			dev_err(kbdev->dev, "FW> %s", buf);
			p = buf;
		}
	}

	if (p != buf) {
		/* Null-terminate and print last unfinished string */
		*p = 0;
		dev_err(kbdev->dev, "FW> %s", buf);
	}

	atomic_set(&fw_log->busy, 0);
}

void kbase_csf_firmware_log_parse_logging_call_list_entry(struct kbase_device *kbdev,
							  const uint32_t *entry)
{
	kbdev->csf.fw_log.func_call_list_va_start = entry[0];
	kbdev->csf.fw_log.func_call_list_va_end = entry[1];
}

/**
 * toggle_logging_calls_in_loaded_image - Toggles FW log func calls in loaded FW image.
 *
 * @kbdev:  Instance of a GPU platform device that implements a CSF interface.
 * @enable: Whether to enable or disable the function calls.
 */
static void toggle_logging_calls_in_loaded_image(struct kbase_device *kbdev, bool enable)
{
	uint32_t bl_instruction, diff;
	uint32_t imm11, imm10, i1, i2, j1, j2, sign;
	uint32_t calling_address = 0, callee_address = 0;
	uint32_t list_entry = kbdev->csf.fw_log.func_call_list_va_start;
	const uint32_t list_va_end = kbdev->csf.fw_log.func_call_list_va_end;

	if (list_entry == 0 || list_va_end == 0)
		return;

	if (enable) {
		for (; list_entry < list_va_end; list_entry += 2 * sizeof(uint32_t)) {
			/* Read calling address */
			kbase_csf_read_firmware_memory(kbdev, list_entry, &calling_address);
			/* Read callee address */
			kbase_csf_read_firmware_memory(kbdev, list_entry + sizeof(uint32_t),
						       &callee_address);

			diff = callee_address - calling_address - 4;
			sign = !!(diff & 0x80000000);
			if (ARMV7_T1_BL_IMM_RANGE_MIN > (int32_t)diff ||
			    ARMV7_T1_BL_IMM_RANGE_MAX < (int32_t)diff) {
				dev_warn(kbdev->dev, "FW log patch 0x%x out of range, skipping",
					 calling_address);
				continue;
			}

			i1 = (diff & 0x00800000) >> 23;
			j1 = !i1 ^ sign;
			i2 = (diff & 0x00400000) >> 22;
			j2 = !i2 ^ sign;
			imm11 = (diff & 0xffe) >> 1;
			imm10 = (diff & 0x3ff000) >> 12;

			/* Compose BL instruction */
			bl_instruction = ARMV7_T1_BL_IMM_INSTR;
			bl_instruction |= j1 << 29;
			bl_instruction |= j2 << 27;
			bl_instruction |= imm11 << 16;
			bl_instruction |= sign << 10;
			bl_instruction |= imm10;

			/* Patch logging func calls in their load location */
			dev_dbg(kbdev->dev, "FW log patch 0x%x: 0x%x\n", calling_address,
				bl_instruction);
			kbase_csf_update_firmware_memory_exe(kbdev, calling_address,
							     bl_instruction);
		}
	} else {
		for (; list_entry < list_va_end; list_entry += 2 * sizeof(uint32_t)) {
			/* Read calling address */
			kbase_csf_read_firmware_memory(kbdev, list_entry, &calling_address);

			/* Overwrite logging func calls with 2 NOP instructions */
			kbase_csf_update_firmware_memory_exe(kbdev, calling_address,
							     ARMV7_DOUBLE_NOP_INSTR);
		}
	}
}

int kbase_csf_firmware_log_toggle_logging_calls(struct kbase_device *kbdev, u32 val)
{
	unsigned long flags;
	struct kbase_csf_firmware_log *fw_log = &kbdev->csf.fw_log;
	bool mcu_inactive;
	bool resume_needed = false;
	int ret = 0;
	struct kbase_csf_scheduler *scheduler = &kbdev->csf.scheduler;

	if (atomic_cmpxchg(&fw_log->busy, 0, 1) != 0)
		return -EBUSY;

	/* Suspend all the active CS groups */
	dev_dbg(kbdev->dev, "Suspend all the active CS groups");

	kbase_csf_scheduler_lock(kbdev);
	while (scheduler->state != SCHED_SUSPENDED) {
		kbase_csf_scheduler_unlock(kbdev);
		kbase_csf_scheduler_pm_suspend(kbdev);
		kbase_csf_scheduler_lock(kbdev);
		resume_needed = true;
	}

	/* Wait for the MCU to get disabled */
	dev_info(kbdev->dev, "Wait for the MCU to get disabled");
	ret = kbase_pm_killable_wait_for_desired_state(kbdev);
	if (ret) {
		dev_err(kbdev->dev, "wait for PM state failed when toggling FW logging calls");
		ret = -EAGAIN;
		goto out;
	}

	spin_lock_irqsave(&kbdev->hwaccess_lock, flags);
	mcu_inactive = kbase_pm_is_mcu_inactive(kbdev, kbdev->pm.backend.mcu_state);
	spin_unlock_irqrestore(&kbdev->hwaccess_lock, flags);
	if (!mcu_inactive) {
		dev_err(kbdev->dev,
			"MCU not inactive after PM state wait when toggling FW logging calls");
		ret = -EAGAIN;
		goto out;
	}

	/* Toggle FW logging call in the loaded FW image */
	toggle_logging_calls_in_loaded_image(kbdev, val);
	dev_dbg(kbdev->dev, "FW logging: %s", val ? "enabled" : "disabled");

out:
	kbase_csf_scheduler_unlock(kbdev);
	if (resume_needed)
		/* Resume queue groups and start mcu */
		kbase_csf_scheduler_pm_resume(kbdev);
	atomic_set(&fw_log->busy, 0);
	return ret;
}
