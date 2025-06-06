// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 MediaTek Inc.
 */



#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <asm/ioctl.h>
#include <linux/compiler.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "ccci_util_log.h"
#include "ccci_util_lib_main.h"

/* Broadcast event.time_stamp defination */
struct ccci_timespec {
	long tv_sec;	/* seconds */
	long tv_nsec;	/* nanoseconds */
};

/* Broadcast event defination */
struct md_status_event {
	struct ccci_timespec time_stamp;

	/* compatible with lk2 tag info
	 * "load_modem_info" sturct
	 */
	int md_id;
	int event_type;
	char reason[32];
};

#ifdef CONFIG_COMPAT
struct md_status_compat_event {
	struct {
		int tv_sec;
		int tv_usec;
	} time_stamp;
	int md_id;
	int event_type;
	char reason[32];
};
#endif

#define MD_BC_MAX_NUM	(2)
#define MD_BC_SUPPORT	(1<<0)
#define EVENT_BUFF_SIZE (8)

static struct bc_ctl_block_t *s_bc_ctl_tbl[MD_BC_MAX_NUM];
static spinlock_t s_event_update_lock;
static int s_md1_user_request_lock_cnt;
static int s_md3_user_request_lock_cnt;
static spinlock_t s_md1_user_lock_cnt_lock;
static spinlock_t s_md3_user_lock_cnt_lock;

static struct class *s_ccci_bd_class;
static dev_t s_md_status_dev;
struct cdev s_bd_char_dev;

struct last_md_status_event {
	int has_value;
	int md_id;
	struct timespec64 time_stamp;
	int event_type;
	char reason[32];
};

static struct last_md_status_event last_md_status;

#define CCCI_UTIL_BC_MAGIC 'B'  /*magic */

#define CCCI_IOC_HOLD_RST_LOCK		_IO(CCCI_UTIL_BC_MAGIC, 0)
#define CCCI_IOC_FREE_RST_LOCK		_IO(CCCI_UTIL_BC_MAGIC, 1)
#define CCCI_IOC_GET_HOLD_RST_CNT	_IO(CCCI_UTIL_BC_MAGIC, 2)
#define CCCI_IOC_SHOW_LOCK_USER		_IO(CCCI_UTIL_BC_MAGIC, 3)

/* Internal used */
struct bc_ctl_block_t {
	struct list_head user_list;
	unsigned int md_bit_mask;
	wait_queue_head_t wait;
};

struct ccci_util_bc_user_ctlb {
	struct bc_ctl_block_t *bc_dev_ptr;
	int pending_event_cnt;
	atomic_t  has_request_rst_lock;
	int curr_w;
	int curr_r;
	int buff_cnt;
	struct md_status_event event_buf[EVENT_BUFF_SIZE];
	int exit;
	struct list_head node;
	char user_name[32];
};

static void inject_event_helper(struct ccci_util_bc_user_ctlb *user_ctlb,
	const struct timespec64 *ev_rtime, int event_type, char reason[])
{
	if (user_ctlb->pending_event_cnt == user_ctlb->buff_cnt) {
		/* Free one space */
		user_ctlb->curr_r++;
		user_ctlb->pending_event_cnt--;
		if (user_ctlb->curr_r >= user_ctlb->buff_cnt)
			user_ctlb->curr_r = 0;
	}

	if (user_ctlb->curr_w >= EVENT_BUFF_SIZE || user_ctlb->curr_w < 0) {
		CCCI_UTIL_ERR_MSG("%s (user_ctlb->curr_w = %d)\n",
				  __func__, user_ctlb->curr_w);
		return;
	}

	user_ctlb->event_buf[user_ctlb->curr_w].time_stamp = *(struct ccci_timespec *)ev_rtime;
	user_ctlb->event_buf[user_ctlb->curr_w].md_id = 0;
	user_ctlb->event_buf[user_ctlb->curr_w].event_type = event_type;
	if (reason != NULL)
		scnprintf(user_ctlb->event_buf[user_ctlb->curr_w].reason, 32,
			"%s", reason);
	else
		scnprintf(user_ctlb->event_buf[user_ctlb->curr_w].reason, 32,
			"%s", "----");
	user_ctlb->curr_w++;
	if (user_ctlb->curr_w >= user_ctlb->buff_cnt)
		user_ctlb->curr_w = 0;
	user_ctlb->pending_event_cnt++;
}

static void save_last_md_status(
	const struct timespec64 *time_stamp, int event_type, char reason[])
{
	/* MD_STA_EV_HS1 = 9
	 * ignore events before MD_STA_EV_HS1
	 */
	if (event_type < 9)
		return;

	CCCI_UTIL_DBG_MSG("[%s] event_type = %d\n",
			__func__, event_type);

	last_md_status.has_value = 1;
	last_md_status.md_id = 0;
	last_md_status.time_stamp = *time_stamp;
	last_md_status.event_type = event_type;

	if (reason != NULL)
		scnprintf(last_md_status.reason, 32, "%s", reason);
	else
		scnprintf(last_md_status.reason, 32, "%s", "----");
}

static void send_last_md_status_to_user(
		struct ccci_util_bc_user_ctlb *user_ctlb)
{
	CCCI_UTIL_DBG_MSG("[%s] user_name = %s\n",
			__func__, user_ctlb->user_name);

	if (last_md_status.has_value == 1)
		inject_event_helper(user_ctlb, &last_md_status.time_stamp,
				last_md_status.event_type, last_md_status.reason);
}

void inject_md_status_event(int event_type, char reason[])
{
	struct timespec64 time_spec64;
	struct timespec64 time_stamp;
	struct ccci_util_bc_user_ctlb *user_ctlb = NULL;
	unsigned int md_mark;
	int i;
	unsigned long flag;

	md_mark = MD_BC_SUPPORT;

	ktime_get_ts64(&time_spec64);
	time_stamp.tv_sec = time_spec64.tv_sec;
	time_stamp.tv_nsec = time_spec64.tv_nsec;
	spin_lock_irqsave(&s_event_update_lock, flag);
	save_last_md_status(&time_stamp, event_type, reason);

	for (i = 0; i < MD_BC_MAX_NUM; i++) {
		if (s_bc_ctl_tbl[i]->md_bit_mask & md_mark) {
			list_for_each_entry(user_ctlb,
				&s_bc_ctl_tbl[i]->user_list, node)
				inject_event_helper(user_ctlb, &time_stamp, event_type, reason);
			wake_up_interruptible(&s_bc_ctl_tbl[i]->wait);
		}
	}
	spin_unlock_irqrestore(&s_event_update_lock, flag);
}
EXPORT_SYMBOL(inject_md_status_event);

int get_lock_rst_user_list(char list_buff[], int size)
{
	int cpy_size;
	int total_size = 0;
	struct ccci_util_bc_user_ctlb *user_ctlb = NULL;
	unsigned long flag;

	if (list_buff == NULL) {
		pr_info("ccci: NULL buffer for lock list\r\n");
		return 0;
	}

	spin_lock_irqsave(&s_event_update_lock, flag);
	list_for_each_entry(user_ctlb,
		&s_bc_ctl_tbl[0]->user_list, node) {
		if (atomic_read(&user_ctlb->has_request_rst_lock) == 1) {
			cpy_size = scnprintf(&list_buff[total_size],
			size - total_size,
			"%s,", user_ctlb->user_name);
			if (cpy_size > 0)
				total_size += cpy_size;
		}
	}
	spin_unlock_irqrestore(&s_event_update_lock, flag);


	list_buff[total_size] = 0;

	return total_size;
}

static int ccci_util_bc_open(struct inode *inode, struct file *filp)
{
	struct ccci_util_bc_user_ctlb *user_ctlb;
	struct bc_ctl_block_t *bc_dev;
	int minor;
	unsigned long flag;

	minor = iminor(inode);
	if (minor < 0 || minor >= MD_BC_MAX_NUM) {
		CCCI_UTIL_ERR_MSG("invalid minor = %d\n", minor);
		return -ENOMEM;
	}
	bc_dev = s_bc_ctl_tbl[minor];

	user_ctlb = kzalloc(sizeof(struct ccci_util_bc_user_ctlb),
					GFP_KERNEL);
	if (user_ctlb == NULL)
		return -ENOMEM;

	user_ctlb->bc_dev_ptr = s_bc_ctl_tbl[minor];
	user_ctlb->pending_event_cnt = 0;
	atomic_set(&user_ctlb->has_request_rst_lock, 0);
	user_ctlb->curr_w = 0;
	user_ctlb->curr_r = 0;
	user_ctlb->buff_cnt = EVENT_BUFF_SIZE;
	user_ctlb->exit = 0;
	INIT_LIST_HEAD(&user_ctlb->node);
	scnprintf(user_ctlb->user_name, 32, "%s", current->comm);
	filp->private_data = user_ctlb;
	nonseekable_open(inode, filp);

	spin_lock_irqsave(&s_event_update_lock, flag);
	list_add_tail(&user_ctlb->node, &bc_dev->user_list);
	send_last_md_status_to_user(user_ctlb);
	spin_unlock_irqrestore(&s_event_update_lock, flag);

	return 0;
}


static int ccci_util_bc_release(struct inode *inode, struct file *filp)
{
	struct ccci_util_bc_user_ctlb *user_ctlb;
	struct bc_ctl_block_t *bc_dev;
	unsigned long flag;

	user_ctlb = filp->private_data;
	bc_dev = user_ctlb->bc_dev_ptr;

	user_ctlb->exit = 1;
	wake_up_interruptible(&bc_dev->wait);
	spin_lock_irqsave(&s_event_update_lock, flag);
	user_ctlb->pending_event_cnt = 0;
	spin_unlock_irqrestore(&s_event_update_lock, flag);

	if (bc_dev->md_bit_mask & MD_BC_SUPPORT) {
		spin_lock(&s_md1_user_lock_cnt_lock);
		if (atomic_read(&user_ctlb->has_request_rst_lock) == 1) {
			atomic_set(&user_ctlb->has_request_rst_lock, 0);
			s_md1_user_request_lock_cnt--;
		}
		spin_unlock(&s_md1_user_lock_cnt_lock);
	}

	spin_lock_irqsave(&s_event_update_lock, flag);
	list_del(&user_ctlb->node);
	spin_unlock_irqrestore(&s_event_update_lock, flag);
	kfree(user_ctlb);

	return 0;
}

static ssize_t ccci_util_bc_write(struct file *filp, const char __user *buf,
	size_t size, loff_t *ppos)
{
	return 0;
}

static int read_out_event(struct ccci_util_bc_user_ctlb *user_ctlb,
	struct md_status_event *event)
{
	int ret;
	struct md_status_event *src_event = NULL;
	unsigned long flag;

	spin_lock_irqsave(&s_event_update_lock, flag);
	if (user_ctlb->pending_event_cnt) {
		src_event = &user_ctlb->event_buf[user_ctlb->curr_r];
		memcpy(event, src_event, sizeof(struct md_status_event));
		user_ctlb->pending_event_cnt--;
		ret = 1;
		user_ctlb->curr_r++;
		if (user_ctlb->curr_r >= user_ctlb->buff_cnt)
			user_ctlb->curr_r = 0;
	} else
		ret = 0;
	spin_unlock_irqrestore(&s_event_update_lock, flag);

	return ret;
}

static int cpy_compat_event_to_user(char __user *buf, size_t size,
	const struct md_status_event *event)
{
	unsigned int event_size;
#ifdef CONFIG_COMPAT
	struct md_status_compat_event compat_event;
#endif

	/*check parameter from user is legal */
	if (buf == NULL)
		return -EFAULT;

#ifdef CONFIG_COMPAT
	if (test_thread_flag(TIF_32BIT) && !COMPAT_USE_64BIT_TIME) {
		event_size = sizeof(struct md_status_compat_event);
		if (event_size > size)
			return -EINVAL;
		compat_event.time_stamp.tv_sec = event->time_stamp.tv_sec;
		compat_event.time_stamp.tv_usec = event->time_stamp.tv_nsec/NSEC_PER_USEC;
		compat_event.md_id = 0;
		compat_event.event_type = event->event_type;
		memcpy(compat_event.reason, event->reason, 32);
		if (copy_to_user(buf, &compat_event, event_size))
			return -EFAULT;
		else
			return (ssize_t)event_size;
	} else {
#endif
		event_size = sizeof(struct md_status_event);
		if (event_size > size)
			return -EINVAL;
		if (copy_to_user(buf, event, event_size))
			return -EFAULT;
		else
			return (ssize_t)event_size;
#ifdef CONFIG_COMPAT
	}
#endif
}

static ssize_t ccci_util_bc_read(struct file *filp, char __user *buf,
	size_t size, loff_t *ppos)
{
	struct ccci_util_bc_user_ctlb *user_ctlb;
	struct bc_ctl_block_t *bc_dev;
	struct md_status_event event;
	int ret;
	int has_data;

	user_ctlb = filp->private_data;
	bc_dev = user_ctlb->bc_dev_ptr;
	has_data = read_out_event(user_ctlb, &event);

	/* For Non-block read */
	if (filp->f_flags & O_NONBLOCK) {
		if (has_data == 0)
			return -EAGAIN;
		else
			return cpy_compat_event_to_user(buf, size, &event);
	}

	/* For block read */
	while (1) {
		if (has_data)
			return cpy_compat_event_to_user(buf, size, &event);

		ret = wait_event_interruptible(bc_dev->wait,
				user_ctlb->pending_event_cnt ||
				user_ctlb->exit);
		if (ret)
			return ret;

		has_data = read_out_event(user_ctlb, &event);
	}

	return 0;
}

static long ccci_util_bc_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	int err = 0;
	struct ccci_util_bc_user_ctlb *user_ctlb;
	struct bc_ctl_block_t *bc_dev;
	int lock_cnt;
	char *buf = NULL;

	user_ctlb = filp->private_data;
	bc_dev = user_ctlb->bc_dev_ptr;

	/* check device type and device number */
	if (_IOC_TYPE(cmd) != CCCI_UTIL_BC_MAGIC)
		return -EINVAL;

	switch (cmd) {
	case CCCI_IOC_HOLD_RST_LOCK:
		if (bc_dev->md_bit_mask & MD_BC_SUPPORT) {
			spin_lock(&s_md1_user_lock_cnt_lock);
			if (atomic_read(&user_ctlb->has_request_rst_lock) == 0) {
				atomic_set(&user_ctlb->has_request_rst_lock, 1);
				s_md1_user_request_lock_cnt++;
			}
			spin_unlock(&s_md1_user_lock_cnt_lock);
		} else
			err = -1;
		break;
	case CCCI_IOC_FREE_RST_LOCK:
		if (bc_dev->md_bit_mask & MD_BC_SUPPORT) {
			spin_lock(&s_md1_user_lock_cnt_lock);
			if (atomic_read(&user_ctlb->has_request_rst_lock) == 1) {
				atomic_set(&user_ctlb->has_request_rst_lock, 0);
				s_md1_user_request_lock_cnt--;
			}
			spin_unlock(&s_md1_user_lock_cnt_lock);
		} else
			err = -1;
		break;
	case CCCI_IOC_GET_HOLD_RST_CNT:
		if (bc_dev->md_bit_mask & MD_BC_SUPPORT)
			lock_cnt = s_md1_user_request_lock_cnt;
		else
			lock_cnt = 0;
		err = put_user((unsigned int)lock_cnt,
					(unsigned int __user *)arg);
		break;
	case CCCI_IOC_SHOW_LOCK_USER:
		buf = kmalloc(1024, GFP_KERNEL);
		if (buf == NULL) {
			err = -1;
			break;
		}
		if (bc_dev->md_bit_mask & MD_BC_SUPPORT)
			get_lock_rst_user_list(buf, 1024);
		else
			buf[0] = 0;
		pr_info("ccci: Lock reset user list: %s\r\n", buf);
		kfree(buf);

		break;
	default:
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}

#ifdef CONFIG_COMPAT
long ccci_util_bc_compat_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		CCCI_UTIL_ERR_MSG(
		"%s(!filp->f_op || !filp->f_op->unlocked_ioctl)\n", __func__);
		return -ENOTTY;
	}

	return filp->f_op->unlocked_ioctl(filp, cmd,
			(unsigned long)compat_ptr(arg));
}
#endif

static unsigned int ccci_util_bc_poll(struct file *filp,
	struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct ccci_util_bc_user_ctlb *user_ctlb;
	struct bc_ctl_block_t *bc_dev;
	unsigned long flag;

	user_ctlb = (struct ccci_util_bc_user_ctlb *)filp->private_data;
	bc_dev = user_ctlb->bc_dev_ptr;
	poll_wait(filp, &bc_dev->wait, wait);

	spin_lock_irqsave(&s_event_update_lock, flag);
	if (user_ctlb->pending_event_cnt)
		mask |= POLLIN|POLLRDNORM;
	spin_unlock_irqrestore(&s_event_update_lock, flag);

	return mask;
}

static const struct file_operations broad_cast_fops = {
	.owner = THIS_MODULE,
	.open = ccci_util_bc_open,
	.read = ccci_util_bc_read,
	.write = ccci_util_bc_write,
	.unlocked_ioctl = ccci_util_bc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ccci_util_bc_compat_ioctl,
#endif
	.poll = ccci_util_bc_poll,
	.release = ccci_util_bc_release,
};

int ccci_util_broadcast_init(void)
{
	int ret;
	int i;
	dev_t dev_n;

	memset(&last_md_status, 0, sizeof(last_md_status));

	for (i = 0; i < MD_BC_MAX_NUM; i++) {
		s_bc_ctl_tbl[i] = kmalloc(sizeof(struct bc_ctl_block_t),
		GFP_KERNEL);
		if (s_bc_ctl_tbl[i] == NULL)
			goto _exit;
		INIT_LIST_HEAD(&s_bc_ctl_tbl[i]->user_list);
		init_waitqueue_head(&s_bc_ctl_tbl[i]->wait);
		if (i == 0)
			s_bc_ctl_tbl[i]->md_bit_mask = 0x7;
		else
			s_bc_ctl_tbl[i]->md_bit_mask = (1U << (i-1));
	}

	spin_lock_init(&s_event_update_lock);
	spin_lock_init(&s_md1_user_lock_cnt_lock);
	spin_lock_init(&s_md3_user_lock_cnt_lock);
	s_ccci_bd_class = class_create("ccci_md_sta");
	s_md1_user_request_lock_cnt = 0;
	s_md3_user_request_lock_cnt = 0;

	ret = alloc_chrdev_region(&s_md_status_dev, 0, 3, "ccci_md_sta");
	if (ret != 0) {
		CCCI_UTIL_ERR_MSG("alloc chrdev fail for ccci_md_sta(%d)\n",
		ret);
		goto _exit_1;
	}
	cdev_init(&s_bd_char_dev, &broad_cast_fops);
	s_bd_char_dev.owner = THIS_MODULE;
	ret = cdev_add(&s_bd_char_dev, s_md_status_dev, 3);
	if (ret) {
		CCCI_UTIL_ERR_MSG("cdev_add failed\n");
		goto _exit_2;
	}

	/* create a device node in directory /dev/ */
	for (i = 0; i < MD_BC_MAX_NUM; i++) {
		dev_n = MKDEV(MAJOR(s_md_status_dev), i);
		if (i == 0)
			device_create(s_ccci_bd_class, NULL, dev_n,
			NULL, "ccci_mdx_sta");
		else
			device_create(s_ccci_bd_class, NULL, dev_n,
			NULL, "ccci_md%d_sta", i);
	}

	return 0;

_exit_2:
	cdev_del(&s_bd_char_dev);

_exit_1:
	unregister_chrdev_region(s_md_status_dev, 3);

_exit:
	for (i = 0; i < MD_BC_MAX_NUM; i++)
		kfree(s_bc_ctl_tbl[i]);

	return -1;
}
