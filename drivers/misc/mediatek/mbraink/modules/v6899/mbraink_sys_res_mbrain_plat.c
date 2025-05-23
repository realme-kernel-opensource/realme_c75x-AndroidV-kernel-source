// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/spinlock.h>
#include <linux/slab.h>

#include "mbraink_sys_res.h"
#include "mbraink_sys_res_plat.h"
#include "mbraink_sys_res_mbrain_dbg.h"
#include "mbraink_sys_res_mbrain_plat.h"

enum {
	ALL_SCENE = 0,
	LAST_SUSPEND_RES = 1,
	LAST_SUSPEND_STATS = 2,
};

static int group_release[NR_SPM_GRP] = {
	MBRAINK_NOT_RELEASE,
	MBRAINK_RELEASE_GROUP,
	MBRAINK_NOT_RELEASE,
	MBRAINK_NOT_RELEASE,
	MBRAINK_NOT_RELEASE,
	MBRAINK_RELEASE_GROUP,
	MBRAINK_NOT_RELEASE,
	MBRAINK_RELEASE_GROUP,
	MBRAINK_NOT_RELEASE,
	MBRAINK_NOT_RELEASE,
	MBRAINK_RELEASE_GROUP,
	MBRAINK_NOT_RELEASE,
	MBRAINK_NOT_RELEASE,
};

struct mbraink_sys_res_mbrain_header header;
static unsigned int sys_res_sig_num, sys_res_grp_num;
static uint32_t spm_res_sig_tbl_num;
static struct mbraink_sys_res_sig_info *spm_res_sig_tbl_ptr;

static void get_sys_res_header(int type)
{
	header.data_offset = sizeof(struct mbraink_sys_res_mbrain_header);
	header.version = MBRAINK_SYS_RES_DATA_VERSION;
	switch (type) {
	case ALL_SCENE:
		header.module = MBRAINK_SYS_RES_ALL_DATA_MODULE_ID;
		header.index_data_length = MBRAINK_SCENE_RELEASE_NUM *
			(sizeof(struct mbraink_sys_res_scene_info) +
			sys_res_sig_num * sizeof(struct mbraink_sys_res_sig_info));
		break;
	case LAST_SUSPEND_RES:
		header.module = MBRAINK_SYS_RES_LVL_DATA_MODULE_ID;
		header.index_data_length = sizeof(struct mbraink_sys_res_scene_info) +
			(sizeof(uint32_t) + sizeof(struct mbraink_sys_res_sig_info)) *
			sys_res_grp_num;
		break;
	case LAST_SUSPEND_STATS:
		header.module = MBRAINK_SYS_RES_LVL_DATA_MODULE_ID;
		header.index_data_length = sizeof(struct mbraink_sys_res_scene_info);
		break;
	}
}

static void *sys_res_data_copy(void *dest, void *src, uint64_t size)
{
	memcpy(dest, src, size);
	dest += size;
	return dest;
}

static unsigned int mbraink_get_sys_res_length(void)
{
	get_sys_res_header(ALL_SCENE);
	return header.index_data_length;
}

static int mbraink_get_sys_res_data(void *address, uint32_t size)
{
	int i = 0;
	int j = 0;
	int ret = 0, sys_res_update = 0;
	unsigned long flag;
	struct mbraink_sys_res_ops *sys_res_ops;
	struct mbraink_sys_res_record *sys_res_record[MBRAINK_SCENE_RELEASE_NUM];
	struct mbraink_sys_res_scene_info scene_info;
	void *sig_info;
	uint64_t addr_idx = 0;
	uint64_t copy_size = 0;

	get_sys_res_header(ALL_SCENE);

	if (!address ||
	    header.index_data_length == 0 ||
	    size < header.index_data_length + header.data_offset) {
		pr_info("[Mbraink][SPM] mbrain address/buffer size error\n");
		ret = -1;
	}

	sys_res_ops = get_mbraink_sys_res_ops();
	if (!sys_res_ops ||
	    !sys_res_ops->update ||
	    !sys_res_ops->get ||
	    !sys_res_ops->get_detail) {
		pr_info("[Mbraink][SPM] Get sys res operations fail\n");
		ret = -1;
	}

	if (ret)
		return ret;

	/* Copy header */
	copy_size = sizeof(struct mbraink_sys_res_mbrain_header);
	if ((addr_idx + copy_size) <= size) {
		address = sys_res_data_copy(address, &header, copy_size);
		addr_idx += copy_size;
	}

	sys_res_update = sys_res_ops->update();
	if (sys_res_update)
		pr_info("[Mbraink][SPM] SWPM data ret(%d)\n", sys_res_update);

	/* Copy scenario data */
	sys_res_record[MBRAINK_SYS_RES_RELEASE_SCENE_COMMON] = sys_res_ops->get(
		MBRAINK_SYS_RES_COMMON);
	sys_res_record[MBRAINK_SYS_RES_RELEASE_SCENE_SUSPEND] = sys_res_ops->get(
		MBRAINK_SYS_RES_SUSPEND);
	sys_res_record[MBRAINK_SYS_RES_RELEASE_SCENE_LAST_SUSPEND] = sys_res_ops->get(
		MBRAINK_SYS_RES_LAST_SUSPEND);

	for (i = 0; i < MBRAINK_SCENE_RELEASE_NUM; i++) {
		if (sys_res_record[i] == NULL || sys_res_record[i]->spm_res_sig_stats_ptr == NULL ||
			sys_res_record[i]->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
			return -1;
	}

	spin_lock_irqsave(sys_res_ops->lock, flag);
	scene_info.res_sig_num = sys_res_sig_num;
	for (i = 0; i < MBRAINK_SCENE_RELEASE_NUM; i++) {
		scene_info.duration_time = sys_res_ops->get_detail(sys_res_record[i],
								   MBRAINK_SYS_RES_DURATION, 0);
		scene_info.suspend_time = sys_res_ops->get_detail(sys_res_record[i],
								   MBRAINK_SYS_RES_SUSPEND_TIME, 0);

		copy_size = sizeof(struct mbraink_sys_res_scene_info);
		if ((addr_idx + copy_size) <= size) {
			address = sys_res_data_copy(address,
					    &scene_info, copy_size);
			addr_idx += copy_size;
		}
	}

	/* Copy signal data */
	for (i = 0; i < MBRAINK_SCENE_RELEASE_NUM; i++) {
		for (j = 0; j < MBRAINK_SYS_RES_SYS_RESOURCE_NUM; j++) {
			if (!group_release[j] || (sys_res_group_info[j].group_num == 0))
				continue;

			sig_info = (void *)sys_res_ops->get_detail(sys_res_record[i],
						MBRAINK_SYS_RES_SIG_ADDR,
						sys_res_group_info[j].sys_index);
			if (sig_info) {
				copy_size = sizeof(struct mbraink_sys_res_sig_info);
				if ((addr_idx + copy_size) <= size) {
					address = sys_res_data_copy(address,
							sig_info, copy_size);
					addr_idx += copy_size;
				}
			}

			sig_info = (void *)sys_res_ops->get_detail(sys_res_record[i],
						MBRAINK_SYS_RES_SIG_ADDR,
						sys_res_group_info[j].sig_table_index);
			if (sig_info) {
				copy_size = sizeof(struct mbraink_sys_res_sig_info) *
					sys_res_group_info[j].group_num;
				if ((addr_idx + copy_size) <= size) {
					address = sys_res_data_copy(address,
							sig_info, copy_size);
					addr_idx += copy_size;
				}
			}
		}
	}
	spin_unlock_irqrestore(sys_res_ops->lock, flag);

	return ret;
}

static int mbraink_get_last_suspend_res_data(void *address, uint32_t size)
{
	int i, ret = 0, sys_res_update = 0;
	unsigned long flag;
	struct mbraink_sys_res_scene_info scene_info;
	struct mbraink_sys_res_record *sys_res_last_suspend_record;
	struct mbraink_sys_res_ops *sys_res_ops;
	void *sig_info = NULL;
	uint64_t addr_idx = 0;
	uint64_t copy_size = 0;

	get_sys_res_header(LAST_SUSPEND_RES);

	if (!address ||
	    header.index_data_length == 0 ||
	    size < header.index_data_length + header.data_offset) {
		pr_info("[Mbraink][SPM] mbrain address/buffer size error\n");
		ret = -1;
	}

	sys_res_ops = get_mbraink_sys_res_ops();
	if (!sys_res_ops ||
		!sys_res_ops->update ||
	    !sys_res_ops->get ||
	    !sys_res_ops->get_detail) {
		pr_info("[Mbraink][SPM] Get sys res operations fail\n");
		ret = -1;
	}

	if (ret)
		return ret;

	copy_size = sizeof(struct mbraink_sys_res_mbrain_header);
	if ((addr_idx + copy_size) <= size) {
		address = sys_res_data_copy(address, &header, copy_size);
		addr_idx += copy_size;
	}

	sys_res_update = sys_res_ops->update();
	if (sys_res_update)
		pr_info("[Mbraink][SPM] SWPM data ret(%d)\n", sys_res_update);

	sys_res_last_suspend_record = sys_res_ops->get(MBRAINK_SYS_RES_LAST_SUSPEND);
	if (sys_res_last_suspend_record == NULL ||
		sys_res_last_suspend_record->spm_res_sig_stats_ptr == NULL ||
		sys_res_last_suspend_record->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
		return -1;

	spin_lock_irqsave(sys_res_ops->lock, flag);
	scene_info.res_sig_num = sys_res_sig_num;
	scene_info.duration_time = sys_res_ops->get_detail(sys_res_last_suspend_record,
							   MBRAINK_SYS_RES_DURATION,
							   0);
	scene_info.suspend_time = sys_res_ops->get_detail(sys_res_last_suspend_record,
							  MBRAINK_SYS_RES_SUSPEND_TIME,
							  0);

	copy_size = sizeof(struct mbraink_sys_res_scene_info);
	if ((addr_idx + copy_size) <= size) {
		address = sys_res_data_copy(address, &scene_info, copy_size);
		addr_idx += copy_size;
	}

	for (i = 0; i < MBRAINK_SYS_RES_SYS_RESOURCE_NUM; i++) {
		if (!group_release[i] || (sys_res_group_info[i].group_num == 0))
			continue;

		copy_size = sizeof(uint32_t);
		if ((addr_idx + copy_size) <= size) {
			address = sys_res_data_copy(address,
					    &(sys_res_group_info[i].threshold), copy_size);
			addr_idx += copy_size;
		}
	}

	for (i = 0; i < MBRAINK_SYS_RES_SYS_RESOURCE_NUM; i++) {
		if (!group_release[i] || (sys_res_group_info[i].group_num == 0))
			continue;

		sig_info = (void *)sys_res_ops->get_detail(sys_res_last_suspend_record,
							   MBRAINK_SYS_RES_SIG_ADDR,
							   sys_res_group_info[i].sys_index);

		if (sig_info) {
			copy_size = sizeof(struct mbraink_sys_res_sig_info);
			if ((addr_idx + copy_size) <= size) {
				address = sys_res_data_copy(address, sig_info, copy_size);
				addr_idx += copy_size;
			}
		}
	}
	spin_unlock_irqrestore(sys_res_ops->lock, flag);
	return 0;
}

static int mbraink_get_over_threshold_num(void *address, uint32_t size,
		uint32_t *threshold, uint32_t threshold_num)
{
	int i, j, k = 0, ret = 0;
	unsigned long flag;
	struct mbraink_sys_res_scene_info scene_info;
	struct mbraink_sys_res_ops *sys_res_ops;
	struct mbraink_sys_res_record *sys_res_last_suspend_record;
	struct mbraink_sys_res_sig_info *ptr = spm_res_sig_tbl_ptr;
	void *sig_info;
	uint64_t ratio;
	uint32_t sig_index;
	uint64_t addr_idx = 0;
	uint64_t copy_size = 0;

	get_sys_res_header(LAST_SUSPEND_STATS);

	if (!address ||
	    header.index_data_length == 0 ||
	    size < header.index_data_length + header.data_offset) {
		pr_info("[Mbraink][SPM] mbrain address/buffer size error\n");
		ret = -1;
	}

	sys_res_ops = get_mbraink_sys_res_ops();
	if (!sys_res_ops ||
	    !sys_res_ops->get ||
	    !sys_res_ops->get_detail) {
		pr_info("[Mbraink][SPM] Get sys res operations fail\n");
		ret = -1;
	}

	if (ret)
		return ret;

	copy_size = sizeof(struct mbraink_sys_res_mbrain_header);
	if ((addr_idx + copy_size) <= size) {
		address = sys_res_data_copy(address, &header, copy_size);
		addr_idx += copy_size;
	}

	sys_res_last_suspend_record = sys_res_ops->get(MBRAINK_SYS_RES_LAST_SUSPEND);

	for (i = 0; i < MBRAINK_SCENE_RELEASE_NUM; i++) {
		if (sys_res_last_suspend_record == NULL ||
			sys_res_last_suspend_record->spm_res_sig_stats_ptr == NULL ||
			sys_res_last_suspend_record->spm_res_sig_stats_ptr->res_sig_tbl == NULL)
			return -1;
	}

	spin_lock_irqsave(sys_res_ops->lock, flag);
	scene_info.duration_time = sys_res_ops->get_detail(sys_res_last_suspend_record,
							   MBRAINK_SYS_RES_DURATION, 0);
	scene_info.suspend_time = sys_res_ops->get_detail(sys_res_last_suspend_record,
							  MBRAINK_SYS_RES_SUSPEND_TIME, 0);
	spm_res_sig_tbl_num = 0;

	for (i = 0; i < MBRAINK_SYS_RES_SYS_RESOURCE_NUM; i++) {
		if (!group_release[i] || (sys_res_group_info[i].group_num == 0))
			continue;

		if (k >= threshold_num) {
			ret = -1;
			pr_info("[Mbraink][SPM] number of threshold is error\n");
			break;
		}

		for (j = 0; j < sys_res_group_info[i].group_num + 1; j++) {
			if (j == 0)
				sig_index = sys_res_group_info[i].sys_index;
			else
				sig_index = j + sys_res_group_info[i].sig_table_index - 1;

			ratio = sys_res_ops->get_detail(sys_res_last_suspend_record,
							MBRAINK_SYS_RES_SIG_SUSPEND_RATIO,
							sig_index);

			if (i == PWR_OFF) {
				if (ratio > threshold[k])
					continue;
			} else {
				if (ratio < threshold[k])
					continue;
			}

			spm_res_sig_tbl_num++;
			sig_info = (void *)sys_res_ops->get_detail(sys_res_last_suspend_record,
								   MBRAINK_SYS_RES_SIG_ADDR,
								   sig_index);
			if (sig_info) {
				ptr = sys_res_data_copy(ptr,
							sig_info,
							sizeof(struct mbraink_sys_res_sig_info));
			}
		}
		k++;
	}
	spin_unlock_irqrestore(sys_res_ops->lock, flag);

	scene_info.res_sig_num = spm_res_sig_tbl_num;
	copy_size = sizeof(struct mbraink_sys_res_scene_info);
	if ((addr_idx + copy_size) <= size) {
		address = sys_res_data_copy(address,
				    &scene_info,
				    copy_size);
		addr_idx += copy_size;
	}
	return ret;
}

static int mbraink_get_over_threshold_data(void *address, uint32_t size)
{
	if (!address ||
	    size < spm_res_sig_tbl_num * sizeof(struct mbraink_sys_res_sig_info)) {
		pr_info("[Mbraink][SPM] mbrain address/buffer size error\n");
		return -1;
	}

	address = sys_res_data_copy(address,
				    spm_res_sig_tbl_ptr,
				    spm_res_sig_tbl_num * sizeof(struct mbraink_sys_res_sig_info));

	return 0;
}

static int mbraink_update_sys_res_record(void)
{
	int ret = 0;
	struct mbraink_sys_res_ops *ops = NULL;

	ops = get_mbraink_sys_res_ops();
	if (ops && ops->update)
		ret = ops->update();

	return ret;
}

static struct mbraink_sys_res_mbrain_dbg_ops sys_res_mbrain_ops = {
	.get_length = mbraink_get_sys_res_length,
	.get_data = mbraink_get_sys_res_data,
	.get_last_suspend_res_data = mbraink_get_last_suspend_res_data,
	.get_over_threshold_num = mbraink_get_over_threshold_num,
	.get_over_threshold_data = mbraink_get_over_threshold_data,
	.update = mbraink_update_sys_res_record,
};

int mbraink_sys_res_mbrain_plat_init(void)
{
	int i = 0;

	for (i = 0; i < NR_SPM_GRP; i++) {
		if (group_release[i]) {
			if (i == DDREN_REQ || i == APSRC_REQ || i == EMI_REQ || i == INFRA_REQ ||
				i == F26M_REQ || i == VCORE_REQ)
				sys_res_grp_num += 1;
			sys_res_sig_num += sys_res_group_info[i].group_num;
		}
	}
	sys_res_sig_num += sys_res_grp_num;
	spm_res_sig_tbl_ptr = kcalloc(sys_res_sig_num,
				      sizeof(struct mbraink_sys_res_sig_info),
				      GFP_KERNEL);

	return register_mbraink_dbg_ops(&sys_res_mbrain_ops);
}

void mbraink_sys_res_mbrain_plat_deinit(void)
{
	unregister_mbraink_dbg_ops();
}
