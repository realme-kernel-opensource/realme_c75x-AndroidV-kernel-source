// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#include <net/genetlink.h>
#include <linux/kobject.h>
#include <linux/netlink.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/module.h>

#include "frs.h"
#include "fpsgo_common.h"
#include "fstb.h"
#include "fbt_cpu.h"

#define EARA_MAX_COUNT 10
#define EARA_PROC_NAME_LEN 16
#define TAG "FRS"

static int frs_nl_id = 31;
static int frs_pid = -1;
module_param(frs_nl_id, int, 0644);
module_param(frs_pid, int, 0644);
struct _EARA_THRM_PACKAGE {
	__s32 type;
	__s32 request;
	__s32 is_camera;
	__s32 pair_pid[EARA_MAX_COUNT];
	__u64 pair_bufid[EARA_MAX_COUNT];
	__s32 pair_tfps[EARA_MAX_COUNT];
	__s32 pair_rfps[EARA_MAX_COUNT];
	__s32 pair_diff[EARA_MAX_COUNT];
	__s32 pair_hwui[EARA_MAX_COUNT];
	char proc_name[EARA_MAX_COUNT][EARA_PROC_NAME_LEN];
	__s32 pair_proc_id[EARA_MAX_COUNT];
};

struct _EARA_THRM_ENABLE {
	__s32 type;
	__s32 enable;
	__s32 pid;
};

static int eara_enable;
static DEFINE_MUTEX(pre_lock);
static struct sock *frs_nl_sk;

static void set_tfps_diff(int max_cnt, int *pid, unsigned long long *buf_id, int *tfps, int *diff)
{
	int i;
	unsigned long long t2wnt;

	mutex_lock(&pre_lock);

	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return;
	}
	mutex_unlock(&pre_lock);

	for (i = 0; i < max_cnt; i++) {
		if (pid[i] == 0)
			break;
		pr_debug(TAG "Set %d %llu: %d\n", pid[i], buf_id[i], diff[i]);
		eara2fstb_tfps_mdiff(pid[i], buf_id[i], diff[i], tfps[i]);
		if (diff[i] == 0)
			t2wnt = 0;
		else
			t2wnt = (1000000000 / tfps[i]) * 2;
		eara2fbt_set_2nd_t2wnt(pid[i], buf_id[i], t2wnt);
	}
}

static void switch_eara(int enable, int pid)
{
	pr_debug(TAG "%s enable:%d\n", __func__, enable);
	mutex_lock(&pre_lock);
	eara_enable = enable;
	frs_pid = pid;
	mutex_unlock(&pre_lock);

}

int pre_change_single_event(int pid, unsigned long long bufID,
			int target_fps)
{
	struct _EARA_THRM_PACKAGE change_msg;
	int ret = 0;

	mutex_lock(&pre_lock);
	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return -1;
	}
	mutex_unlock(&pre_lock);

	memset(&change_msg, 0, sizeof(struct _EARA_THRM_PACKAGE));
	change_msg.request = 1;
	change_msg.pair_pid[0] = pid;
	change_msg.pair_bufid[0] = bufID;
	change_msg.pair_tfps[0] = target_fps;
	ret = eara_nl_send_to_user((void *)&change_msg, sizeof(struct _EARA_THRM_PACKAGE));

	return ret;
}

int pre_change_event(void)
{
	struct _EARA_THRM_PACKAGE change_msg;
	int ret = 0;

	pr_debug("eara_enable %d\n", eara_enable);
	mutex_lock(&pre_lock);
	if (!eara_enable) {
		mutex_unlock(&pre_lock);
		return -1;
	}
	mutex_unlock(&pre_lock);
	memset(&change_msg, 0, sizeof(struct _EARA_THRM_PACKAGE));
	eara2fstb_get_tfps(EARA_MAX_COUNT, &(change_msg.is_camera), change_msg.pair_pid,
			change_msg.pair_bufid, change_msg.pair_tfps, change_msg.pair_rfps,
			change_msg.pair_hwui, change_msg.proc_name, change_msg.pair_proc_id);
	ret = eara_nl_send_to_user((void *)&change_msg, sizeof(struct _EARA_THRM_PACKAGE));

	return ret;
}

int eara_nl_send_to_user(void *buf, int size)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;

	int len = NLMSG_SPACE(size);
	void *data;
	int ret;
	static int c;

	if (frs_nl_sk == NULL)
		return -1;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return -1;
	nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, size, 0);
	if (!nlh) {
		kfree_skb(skb);
		return -EMSGSIZE;
	}
	data = NLMSG_DATA(nlh);
	memcpy(data, buf, size);
	NETLINK_CB(skb).portid = 0; /* from kernel */
	NETLINK_CB(skb).dst_group = 0; /* unicast */

	pr_debug(TAG "Netlink_unicast size=%d\n", size);

	ret = netlink_unicast(frs_nl_sk, skb, frs_pid, MSG_DONTWAIT);
	if (ret < 0 && c < 3) {
		pr_debug(TAG "Send to pid %d failed %d, retry %d\n", frs_pid, ret, c);
		c++;
	} else if (ret == 0)
		c = 0;
	if (c >= 3) {
		switch_eara(0, -1);
		c = 0;
	}
	pr_debug(TAG "Netlink_unicast- ret=%d\n", ret);
	return 0;

}

static void eara_nl_data_handler(struct sk_buff *skb)
{
	/*u32 pid;
	 * kuid_t uid;
	 * int seq;
	 */

	struct nlmsghdr *nlh;
	struct _EARA_THRM_PACKAGE *change_msg;
	struct _EARA_THRM_ENABLE *enable_msg;

	nlh = (struct nlmsghdr *)skb->data;
	/*pid = NETLINK_CREDS(skb)->pid;
	 * uid = NETLINK_CREDS(skb)->uid;
	 * seq = nlh->nlmsg_seq;
	 */

	/*tsta_dprintk(
	 *"[ta_nl_data_handler] recv skb from user space uid:%d pid:%d seq:%d\n"
	 * ,uid, pid, seq);
	 */

	change_msg = (struct _EARA_THRM_PACKAGE *) NLMSG_DATA(nlh);
	enable_msg = (struct _EARA_THRM_ENABLE *) NLMSG_DATA(nlh);
	if (change_msg->type == 0)
		set_tfps_diff(EARA_MAX_COUNT, change_msg->pair_pid,
			change_msg->pair_bufid, change_msg->pair_tfps, change_msg->pair_diff);
	else
		switch_eara(enable_msg->enable, enable_msg->pid);
}


int eara_netlink_init(void)
{
	/*add by willcai for the userspace  to kernelspace*/
	struct netlink_kernel_cfg cfg = {
		.input  = eara_nl_data_handler,
	};

	frs_nl_sk = NULL;
	frs_nl_sk = netlink_kernel_create(&init_net, frs_nl_id, &cfg);

	pr_debug(TAG "netlink_kernel_create protol= %d\n", frs_nl_id);

	if (frs_nl_sk == NULL) {
		pr_debug(TAG "netlink_kernel_create fail\n");
		return -1;
	}

	return 0;
}

static ssize_t frs_pid_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", frs_pid);

	return len;
}

static ssize_t frs_nl_id_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", frs_nl_id);

	return len;
}
static struct kobj_attribute frs_nl_id_attr = __ATTR_RO(frs_nl_id);
static struct kobj_attribute frs_pid_attr = __ATTR_RO(frs_pid);
static struct attribute *thermal_attrs[] = {
	&frs_nl_id_attr.attr,
	&frs_pid_attr.attr,
	NULL
};
static struct attribute_group thermal_attr_group = {
	.name	= "thermal",
	.attrs	= thermal_attrs,
};

void __exit eara_thrm_pre_exit(void)
{
	eara_pre_change_fp = NULL;
	eara_pre_change_single_fp = NULL;
}

int __init eara_thrm_pre_init(void)
{
	int ret;

	eara_pre_change_fp = pre_change_event;
	eara_pre_change_single_fp =  pre_change_single_event;
	eara_netlink_init();

	ret = sysfs_merge_group(kernel_kobj, &thermal_attr_group);
	if (ret) {
		pr_info("%s failed to create thermal sysfs, ret=%d!\n", TAG, ret);
		return ret;
	}
	return 0;
}

module_init(eara_thrm_pre_init);
module_exit(eara_thrm_pre_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Frame Rate Smoother");
MODULE_AUTHOR("MediaTek Inc.");

