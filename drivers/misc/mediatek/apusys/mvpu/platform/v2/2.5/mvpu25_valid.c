// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>

#include "apusys_device.h"
#include "mdw_trace.h"

#include "mvpu_plat.h"
#include "mvpu_sysfs.h"
#include "mvpu25_request.h"
#include "mvpu25_sec.h"
#include "mvpu25_handler.h"
#include "mvpu25_ipi.h"

int mvpu25_validation(void *hnd)
{
	int ret = 0;
	struct mvpu_request_v25 *mvpu_req;
	struct apusys_cmd_valid_handle *cmd_hnd;
	void *session;
	struct apusys_cmdbuf *cmdbuf;

	void *kreg_kva;
#ifdef MVPU_SEC_KREG_IN_POOL
	uint32_t kreg_iova_pool = 0;
#endif
	uint32_t knl_num = 0;

	uint32_t batch_name_hash;
	uint32_t buf_num = 0;
	uint32_t rp_num = 0;
	uint32_t kerarg_num = 0;
	uint32_t primem_num = 0;
	uint32_t sec_level = 0;
	uint32_t buf_int_check_pass = 1;

	uint32_t i = 0;

	uint32_t *sec_chk_addr = NULL;
	uint32_t *sec_buf_size = NULL;
	uint32_t *sec_buf_attr = NULL;

	uint32_t *target_buf_old_base = NULL;
	uint32_t *target_buf_old_offset = NULL;
	uint32_t *target_buf_new_base = NULL;
	uint32_t *target_buf_new_offset = NULL;

	uint32_t *kerarg_buf_id = NULL;
	uint32_t *kerarg_offset = NULL;
	uint32_t *kerarg_size = NULL;

	uint32_t *primem_src_buf_id = NULL;
	uint32_t *primem_dst_buf_id = NULL;
	uint32_t *primem_src_offset = NULL;
	uint32_t *primem_dst_offset = NULL;
	uint32_t *primem_size = NULL;

	void *sec_chk_addr_kva = NULL;
	void *sec_buf_size_kva = NULL;
	void *sec_buf_attr_kva = NULL;

	void *tgtbuf_old_base_kva = NULL;
	void *tgtbuf_old_ofst_kva = NULL;
	void *tgtbuf_new_base_kva = NULL;
	void *tgtbuf_new_ofst_kva = NULL;

	void *kerarg_buf_id_kva = NULL;
	void *kerarg_offset_kva = NULL;
	void *kerarg_size_kva = NULL;

	void *primem_src_buf_id_kva = NULL;
	void *primem_dst_buf_id_kva = NULL;
	void *primem_src_offset_kva = NULL;
	void *primem_dst_offset_kva = NULL;
	void *primem_size_kva = NULL;

	uint32_t *target_buf_old_map = NULL;
	uint32_t *target_buf_new_map = NULL;
	uint32_t *rp_skip_buf = NULL;

	bool algo_in_img = false;
	uint32_t ker_bin_offset = 0;
	//uint32_t ker_size = 0;
	uint32_t ker_bin_num = 0;
	uint32_t *ker_bin_each_iova = NULL;

	uint32_t check_buf_size = 0;

	uint32_t session_id = 0xFFFFFFFF;
	uint32_t hash_id = 0xFFFFFFFF;
	bool algo_in_pool = false;
	bool algo_all_same_buf = true;

	uint32_t buf_cmd_cnt = 0;
	uint32_t buf_cmd_kreg = 0;
	uint32_t buf_cmd_next = 0;

	bool support_mod_kerarg = false;

	mdw_trace_begin("[MVPU] %s", __func__);

	cmd_hnd = hnd;

	if (cmd_hnd->session == NULL) {
		pr_info("[MVPU][Sec] [ERROR] APUSYS_CMD_VALIDATE: session is NULL\n");
		ret = -1;
		goto END;
	} else {
		session = cmd_hnd->session;
		if (mvpu_drv_loglv >= APUSYS_MVPU_LOG_DBG)
			pr_info("[MVPU][Sec] get session: 0x%llx\n", (uint64_t)session);
	}

	if (cmd_hnd->num_cmdbufs < MVPU_MIN_CMDBUF_NUM) {
		pr_info("[MVPU][Sec] [ERROR] %s get wrong num_cmdbufs: %d\n",
				__func__, cmd_hnd->num_cmdbufs);
		ret = -1;
		goto END;
	}

	cmdbuf = cmd_hnd->cmdbufs;

	if (cmdbuf[MVPU_CMD_INFO_IDX].size != sizeof(struct mvpu_request_v25)) {
		pr_info("[MVPU][Sec] [ERROR] get wrong cmdbuf size: 0x%x, should be 0x%lx\n",
				cmdbuf[MVPU_CMD_INFO_IDX].size,
				sizeof(struct mvpu_request_v25));
		ret = -1;
		goto END;
	}

	mvpu_req = (struct mvpu_request_v25 *)cmdbuf[MVPU_CMD_INFO_IDX].kva;
	kreg_kva = cmdbuf[MVPU_CMD_KREG_BASE_IDX].kva;

	batch_name_hash = mvpu_req->batch_name_hash;

	//get buf infos
	buf_num = mvpu_req->buf_num & BUF_NUM_MASK;

	//get replace infos
	rp_num = mvpu_req->rp_num;

	if (mvpu_drv_loglv >= APUSYS_MVPU_LOG_DBG) {
		pr_info("[MVPU][Sec] DRV set batch: 0x%08x\n", batch_name_hash);
		pr_info("[MVPU][Sec] buf_num %d\n", buf_num);
		pr_info("[MVPU][Sec] rp_num %d\n", rp_num);
	}

	if ((batch_name_hash & MVPU_BATCH_MASK) == 0x0) {
		pr_info("[MVPU][IMG] [ERROR] get wrong HASH 0x%08x\n", batch_name_hash);
		ret = -1;
		goto END;
	}

	if (mvpu_algo_available == true)
		algo_in_img = mvpu25_get_ptn_hash(batch_name_hash);

	if (algo_in_img == true && mvpu25_get_mvpu_algo_available() == false) {
		pr_info("[MVPU][IMG] [ERROR] get HASH 0x%08x but mvpu_algo.img is wrong, please check\n",
						batch_name_hash);
		ret = -1;
		goto END;
	}

#ifdef MVPU_SEC_BLOCK_EDMA_KERNEL
	if ((batch_name_hash & RT_BATCH_KERNEL_USING_EDMA) != 0x0) {
		if (algo_in_img == false) {
			if (mvpu_drv_loglv >= APUSYS_MVPU_LOG_DBG)
				pr_info("[MVPU][Sec] [ERROR] batch 0x%08x using EDMA function is forbiddened!!!\n",
						batch_name_hash);
#ifdef MVPU_SEC_BLOCK_EDMA_KERNEL_RETURN
			ret = -1;
			goto END;
#endif // MVPU_SEC_BLOCK_EDMA_KERNEL_RETURN
		}
	}
#endif

	if (g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU25a || g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU25b) {
		if ((mvpu_req->feature_control_mask & MVPU_REQ_FEATURE_OSGB_LIMITED) != MVPU_REQ_FEATURE_OSGB_LIMITED) {
			pr_info("[MVPU] [ERROR] binary is too old, please regenerate binary with version after SW 4.1 (include OSG patch)\n");
			strscpy(&mvpu_req->name[31], "\0", sizeof(char));
			pr_info("[MVPU] algo_name: %s\n", mvpu_req->name);
			mvpu_aee_exception("MVPU", "MVPU aee");
			ret = -1;
			goto END;
		}
	}

	if (g_mvpu_platdata->sw_ver == MVPU_SW_VER_MVPU25b) {
		if ((mvpu_req->feature_control_mask & MVPU_REQ_FEATURE_MVPU250_WA) != MVPU_REQ_FEATURE_MVPU250_WA) {
			pr_info("[MVPU] [ERROR] binary is too old, please regenerate binary with version after SW 4.5 (include MVPU250 patch)\n");
			strscpy(&mvpu_req->name[31], "\0", sizeof(char));
			pr_info("[MVPU] algo_name: %s\n", mvpu_req->name);
			mvpu_aee_exception("MVPU", "MVPU aee");
			ret = -1;
			goto END;
		}
	}

	mutex_lock(&mvpu25_pool_lock);

	algo_in_pool = mvpu25_get_hash_info(session,
						batch_name_hash,
						&session_id,
						&hash_id,
						buf_num);

	//get security level
	sec_level = (mvpu_req->buf_num & SEC_LEVEL_MASK) >> SEC_LEVEL_SHIFT;

	if (sec_level >= SEC_LVL_END) {
		pr_info("[MVPU][Sec] [WARNING] wrong sec_level %d, set to check\n",
				sec_level);
		sec_level = SEC_LVL_CHECK;
	}

	// get kerarg setting and check support modify
	kerarg_num = (mvpu_req->buf_num & KERARG_NUM_MASK) >> KERARG_NUM_SHIFT;
	if (cmdbuf[MVPU_CMD_INFO_IDX].size == sizeof(struct mvpu_request_v25))
		support_mod_kerarg = true;

	if (support_mod_kerarg == false && kerarg_num != 0) {
		if (mvpu_drv_loglv >= APUSYS_MVPU_LOG_DBG)
			pr_info("[MVPU][Sec] [WARNING] not support ker arg modify, set to check\n");
		sec_level = SEC_LVL_CHECK;
	}

	// set security level to uP
	mvpu_req->header.reg_bundle_setting_0.s.kreg_0x0000_rsv1 = sec_level;

	if (mvpu_drv_loglv >= APUSYS_MVPU_LOG_DBG) {
		pr_info("[MVPU][Sec] sec_level %d\n", sec_level);
		pr_info("[MVPU][Sec] kerarg_num %d\n", kerarg_num);
	}

	if (mvpu_drv_loglv >= APUSYS_MVPU_LOG_ALL) {
		pr_info("[MVPU][SEC] [MPU] Engine settings\n");
		pr_info("[MVPU][SEC] [MPU] mpu_num = %3d\n", mvpu_req->mpu_num);
		for (i = 0; i < MVPU_MPU_SEGMENT_NUMS; i++)
			pr_info("[MVPU][SEC] [MPU] eng mpu_reg[%3d] = 0x%08x\n",
						i, mvpu_req->mpu_seg[i]);
	}

	if (mvpu25_mem_use_iova(mvpu_req->pmu_buff)) {
		if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
				(uint64_t)mvpu_req->pmu_buff, mvpu_req->buff_size) != 0) {
			pr_info("[MVPU][Sec] pmu_buff 0x%lx integrity checked FAIL\n",
						(unsigned long)mvpu_req->pmu_buff);
			ret = -EINVAL;
			goto END_WITH_MUTEX;
		}
	}

	if ((batch_name_hash & MVPU_BATCH_MASK) !=
			(MVPU_ONLINE_BATCH_NAME_HASH & MVPU_BATCH_MASK)) {
		mdw_trace_begin("[MVPU] mvpu_req integrity check, buf_num %d, rp_num %d",
			buf_num, rp_num);
		if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
				(uint64_t)mvpu_req->sec_chk_addr, buf_num*sizeof(uint32_t)) != 0) {
			pr_info("[MVPU][Sec] sec_chk_addr integrity checked FAIL\n");
			ret = -EINVAL;
			goto END_WITH_MUTEX;
		}

		if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
				(uint64_t)mvpu_req->sec_buf_size, buf_num*sizeof(uint32_t)) != 0) {
			pr_info("[MVPU][Sec] sec_buf_size integrity checked FAIL\n");
			ret = -EINVAL;
			goto END_WITH_MUTEX;
		}

		if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
				(uint64_t)mvpu_req->sec_buf_attr, buf_num*sizeof(uint32_t)) != 0) {
			pr_info("[MVPU][Sec] sec_buf_attr integrity checked FAIL\n");
			ret = -EINVAL;
			goto END_WITH_MUTEX;
		}

		// copy buf infos
		sec_chk_addr = kcalloc(buf_num, sizeof(uint32_t), GFP_KERNEL);
		if (sec_chk_addr == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		sec_chk_addr_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->sec_chk_addr);

		if (sec_chk_addr_kva != NULL) {
			memcpy(sec_chk_addr, sec_chk_addr_kva, buf_num*sizeof(uint32_t));
		} else {
			pr_info("[MVPU][Sec] [ERROR] sec_chk_addr_kva is NULL\n");
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		sec_buf_size = kcalloc(buf_num, sizeof(uint32_t), GFP_KERNEL);
		if (sec_buf_size == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		sec_buf_size_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->sec_buf_size);

		if (sec_buf_size_kva != NULL) {
			memcpy(sec_buf_size, sec_buf_size_kva, buf_num*sizeof(uint32_t));
		} else {
			pr_info("[MVPU][Sec] [ERROR] sec_buf_size_kva is NULL\n");
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		sec_buf_attr = kcalloc(buf_num, sizeof(uint32_t), GFP_KERNEL);
		if (sec_buf_attr == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}


		sec_buf_attr_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->sec_buf_attr);

		if (sec_buf_attr_kva != NULL) {
			memcpy(sec_buf_attr, sec_buf_attr_kva, buf_num*sizeof(uint32_t));
		} else {
			pr_info("[MVPU][Sec] [ERROR] sec_buf_attr_kva is NULL\n");
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}
		mdw_trace_end();

		// buf integrity check
		mdw_trace_begin("[MVPU] user buf integrity check");
		for (i = 0; i < buf_num; i++) {
			if (mvpu_drv_loglv >= APUSYS_MVPU_LOG_DBG) {
				pr_info("[MVPU][Sec] buf[%3d]: addr 0x%08x, attr: %d, size: 0x%08x\n",
					i, sec_chk_addr[i],
					sec_buf_attr[i],
					sec_buf_size[i]);
			}

			if (mvpu25_mem_use_iova(sec_chk_addr[i]) == false) {
				if (sec_chk_addr[i] == 0) {
					buf_cmd_cnt++;
					if (buf_cmd_cnt == MVPU_MIN_CMDBUF_NUM) {
						buf_cmd_kreg = i;
						buf_cmd_next = i + 1;
					}
				}
				continue;
			}

			// check buffer integrity
			if (sec_buf_attr[i] == BUF_IO)
				check_buf_size = 0;
			else
				check_buf_size = sec_buf_size[i];

			if (apusys_mem_validate_by_cmd(cmd_hnd->session, cmd_hnd->cmd,
					(uint64_t)sec_chk_addr[i], check_buf_size) != 0) {
				pr_info("[MVPU][Sec] buf[%3d]: 0x%08x integrity checked FAIL\n",
							i, sec_chk_addr[i]);
				buf_int_check_pass = 0;
			} else {
				if (mvpu_drv_loglv >= APUSYS_MVPU_LOG_DBG)
					pr_info("[MVPU][Sec] buf[%3d]: 0x%08x integrity checked PASS\n",
							i, sec_chk_addr[i]);
			}
		}
		mdw_trace_end();

		if (buf_int_check_pass == 0) {
			pr_info("[MVPU][Sec] [ERROR] integrity checked FAIL\n");
			ret = -1;
			goto END_WITH_MUTEX;
		}

		ret = mvpu25_update_mpu(mvpu_req, session_id, hash_id,
						sec_chk_addr, sec_buf_size, sec_buf_attr, false);

		if (mvpu_req->mpu_num != 0 && mvpu_drv_loglv >= APUSYS_MVPU_LOG_DBG) {
			pr_info("[MVPU][SEC] [MPU] Offline batch\n");
			pr_info("[MVPU][SEC] [MPU] mpu_num = %3d\n", mvpu_req->mpu_num);
			for (i = 0; i < MVPU_MPU_SEGMENT_NUMS; i++)
				pr_info("[MVPU][SEC] [MPU] drv mpu_reg[%3d] = 0x%08x\n",
							i, mvpu_req->mpu_seg[i]);
		}

		if (ret != 0)
			goto END_WITH_MUTEX;
	}

	if (algo_in_pool == false) {
		if ((buf_num == 0 || rp_num == 0) ||
			sec_level != SEC_LVL_PROTECT) {
			if (mvpu_drv_loglv >= APUSYS_MVPU_LOG_DBG)
				pr_info("[MVPU][Sec] check flow\n");

			knl_num = mvpu_req->header.reg_bundle_setting_0.s.kreg_kernel_num;
			ret = mvpu25_check_batch_flow(session, cmd_hnd->cmd, sec_level, kreg_kva, knl_num);

			if (ret != 0) {
				pr_info("[MVPU][Sec] integrity checked FAIL\n");
				goto END_WITH_MUTEX;
			}

			if (algo_in_img == false)
				goto END_WITH_MUTEX;
		}
	}

	if (algo_in_pool == false) {
		if (kerarg_num != 0) {
			if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
					(uint64_t)mvpu_req->kerarg_buf_id,
					kerarg_num*sizeof(uint32_t)) != 0) {
				pr_info("[MVPU][Sec] kerarg_buf_id integrity checked FAIL\n");
				ret = -EINVAL;
				goto END_WITH_MUTEX;
			}

			if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
					(uint64_t)mvpu_req->kerarg_offset,
					kerarg_num*sizeof(uint32_t)) != 0) {
				pr_info("[MVPU][Sec] kerarg_offset integrity checked FAIL\n");
				ret = -EINVAL;
				goto END_WITH_MUTEX;
			}

			if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
					(uint64_t)mvpu_req->kerarg_size,
					kerarg_num*sizeof(uint32_t)) != 0) {
				pr_info("[MVPU][Sec] kerarg_size integrity checked FAIL\n");
				ret = -EINVAL;
				goto END_WITH_MUTEX;
			}

			// get primem only when ker arg num != 0 & not get into check flow
			primem_num = mvpu_req->primem_num;
			if (primem_num != 0) {
				if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
						(uint64_t)mvpu_req->primem_src_buf_id,
						primem_num*sizeof(uint32_t)) != 0) {
					pr_info("[MVPU][Sec] primem_src_buf_id integrity checked FAIL\n");
					ret = -EINVAL;
					goto END_WITH_MUTEX;
				}

				if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
						(uint64_t)mvpu_req->primem_dst_buf_id,
						primem_num*sizeof(uint32_t)) != 0) {
					pr_info("[MVPU][Sec] primem_dst_buf_id integrity checked FAIL\n");
					ret = -EINVAL;
					goto END_WITH_MUTEX;
				}

				if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
						(uint64_t)mvpu_req->primem_dst_offset,
						primem_num*sizeof(uint32_t)) != 0) {
					pr_info("[MVPU][Sec] primem_dst_offset integrity checked FAIL\n");
					ret = -EINVAL;
					goto END_WITH_MUTEX;
				}

				if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
						(uint64_t)mvpu_req->primem_src_offset,
						primem_num*sizeof(uint32_t)) != 0) {
					pr_info("[MVPU][Sec] primem_src_offset integrity checked FAIL\n");
					ret = -EINVAL;
					goto END_WITH_MUTEX;
				}

				if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
						(uint64_t)mvpu_req->primem_size,
						primem_num*sizeof(uint32_t)) != 0) {
					pr_info("[MVPU][Sec] primem_size integrity checked FAIL\n");
					ret = -EINVAL;
					goto END_WITH_MUTEX;
				}
			} //(primem_num != 0)
		} //(kerarg_num != 0)

		if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
				(uint64_t)mvpu_req->target_buf_old_base,
				rp_num*sizeof(uint32_t)) != 0) {
			pr_info("[MVPU][Sec] target_buf_old_base integrity checked FAIL\n");
			ret = -EINVAL;
			goto END_WITH_MUTEX;
		}

		if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
				(uint64_t)mvpu_req->target_buf_old_offset,
				rp_num*sizeof(uint32_t)) != 0) {
			pr_info("[MVPU][Sec] target_buf_old_offset integrity checked FAIL\n");
			ret = -EINVAL;
			goto END_WITH_MUTEX;
		}

		if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
				(uint64_t)mvpu_req->target_buf_new_base,
				rp_num*sizeof(uint32_t)) != 0) {
			pr_info("[MVPU][Sec] target_buf_new_base integrity checked FAIL\n");
			ret = -EINVAL;
			goto END_WITH_MUTEX;
		}

		if (apusys_mem_validate_by_cmd(session, cmd_hnd->cmd,
				(uint64_t)mvpu_req->target_buf_new_offset,
				rp_num*sizeof(uint32_t)) != 0) {
			pr_info("[MVPU][Sec] target_buf_new_offset integrity checked FAIL\n");
			ret = -EINVAL;
			goto END_WITH_MUTEX;
		}
	}

	//get image infos: kernel.bin
	if (algo_in_img) {
		mvpu25_get_ker_info(batch_name_hash, &ker_bin_offset, &ker_bin_num);

		if (ker_bin_num == 0) {
			pr_info("[MVPU][IMG] [ERROR] not found Kernel_*.bin in mvpu_algo.img, please check\n");
			ret = -1;
			goto END_WITH_MUTEX;
		}

		ker_bin_each_iova = kcalloc(ker_bin_num, sizeof(uint32_t), GFP_KERNEL);
		if (ker_bin_each_iova == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		mvpu25_set_ker_iova(ker_bin_offset, ker_bin_num, ker_bin_each_iova);
	}

	// copy ker arg infos
	if (kerarg_num != 0) {
		kerarg_buf_id = kcalloc(kerarg_num, sizeof(uint32_t), GFP_KERNEL);
		if (kerarg_buf_id == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		kerarg_buf_id_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->kerarg_buf_id);

		if (kerarg_buf_id_kva != NULL)
			memcpy(kerarg_buf_id, kerarg_buf_id_kva, kerarg_num*sizeof(uint32_t));

		kerarg_offset = kcalloc(kerarg_num, sizeof(uint32_t), GFP_KERNEL);
		if (kerarg_offset == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		kerarg_offset_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->kerarg_offset);

		if (kerarg_offset_kva != NULL)
			memcpy(kerarg_offset, kerarg_offset_kva, kerarg_num*sizeof(uint32_t));

		kerarg_size = kcalloc(kerarg_num, sizeof(uint32_t), GFP_KERNEL);
		if (kerarg_size == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		kerarg_size_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->kerarg_size);

		if (kerarg_size_kva != NULL)
			memcpy(kerarg_size, kerarg_size_kva, kerarg_num*sizeof(uint32_t));

		// get primem only when ker arg num != 0 & not get into check flow
		primem_num = mvpu_req->primem_num;
		if (primem_num != 0) {
			primem_src_buf_id = kcalloc(primem_num, sizeof(uint32_t), GFP_KERNEL);
			if (primem_src_buf_id == NULL) {
				ret = -ENOMEM;
				goto END_WITH_MUTEX;
			}

			primem_src_buf_id_kva =
				apusys_mem_query_kva_by_sess(session, mvpu_req->primem_src_buf_id);

			if (primem_src_buf_id_kva != NULL)
				memcpy(primem_src_buf_id, primem_src_buf_id_kva,
					primem_num*sizeof(uint32_t));

			primem_dst_buf_id = kcalloc(primem_num, sizeof(uint32_t), GFP_KERNEL);
			if (primem_dst_buf_id == NULL) {
				ret = -ENOMEM;
				goto END_WITH_MUTEX;
			}

			primem_dst_buf_id_kva =
				apusys_mem_query_kva_by_sess(session, mvpu_req->primem_dst_buf_id);

			if (primem_dst_buf_id_kva != NULL)
				memcpy(primem_dst_buf_id, primem_dst_buf_id_kva,
					primem_num*sizeof(uint32_t));

			primem_src_offset = kcalloc(primem_num, sizeof(uint32_t), GFP_KERNEL);
			if (primem_src_offset == NULL) {
				ret = -ENOMEM;
				goto END_WITH_MUTEX;
			}

			primem_src_offset_kva =
				apusys_mem_query_kva_by_sess(session, mvpu_req->primem_src_offset);

			if (primem_src_offset_kva != NULL)
				memcpy(primem_src_offset, primem_src_offset_kva,
					primem_num*sizeof(uint32_t));

			primem_dst_offset = kcalloc(primem_num, sizeof(uint32_t), GFP_KERNEL);
			if (primem_dst_offset == NULL) {
				ret = -ENOMEM;
				goto END_WITH_MUTEX;
			}

			primem_dst_offset_kva =
				apusys_mem_query_kva_by_sess(session, mvpu_req->primem_dst_offset);

			if (primem_dst_offset_kva != NULL)
				memcpy(primem_dst_offset, primem_dst_offset_kva,
					primem_num*sizeof(uint32_t));

			primem_size = kcalloc(primem_num, sizeof(uint32_t), GFP_KERNEL);
			if (primem_size == NULL) {
				ret = -ENOMEM;
				goto END_WITH_MUTEX;
			}

			primem_size_kva =
				apusys_mem_query_kva_by_sess(session, mvpu_req->primem_size);

			if (primem_size_kva != NULL)
				memcpy(primem_size, primem_size_kva, primem_num*sizeof(uint32_t));
		}
	}

	// copy rp infos
	if (rp_num != 0) {
		target_buf_old_base = kcalloc(rp_num, sizeof(uint32_t), GFP_KERNEL);
		if (target_buf_old_base == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		tgtbuf_old_base_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->target_buf_old_base);

		if (tgtbuf_old_base_kva != NULL)
			memcpy(target_buf_old_base, tgtbuf_old_base_kva, rp_num*sizeof(uint32_t));

		target_buf_old_offset = kcalloc(rp_num, sizeof(uint32_t), GFP_KERNEL);
		if (target_buf_old_offset == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		tgtbuf_old_ofst_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->target_buf_old_offset);

		if (tgtbuf_old_ofst_kva != NULL)
			memcpy(target_buf_old_offset, tgtbuf_old_ofst_kva, rp_num*sizeof(uint32_t));

		target_buf_new_base = kcalloc(rp_num, sizeof(uint32_t), GFP_KERNEL);
		if (target_buf_new_base == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		tgtbuf_new_base_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->target_buf_new_base);

		if (tgtbuf_new_base_kva != NULL)
			memcpy(target_buf_new_base, tgtbuf_new_base_kva, rp_num*sizeof(uint32_t));

		target_buf_new_offset = kcalloc(rp_num, sizeof(uint32_t), GFP_KERNEL);
		if (target_buf_new_offset == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		tgtbuf_new_ofst_kva =
			apusys_mem_query_kva_by_sess(session, mvpu_req->target_buf_new_offset);

		if (tgtbuf_new_ofst_kva != NULL)
			memcpy(target_buf_new_offset, tgtbuf_new_ofst_kva, rp_num*sizeof(uint32_t));

		target_buf_old_map = kcalloc(rp_num, sizeof(uint32_t), GFP_KERNEL);
		if (target_buf_old_map == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}

		target_buf_new_map = kcalloc(rp_num, sizeof(uint32_t), GFP_KERNEL);
		if (target_buf_new_map == NULL) {
			ret = -ENOMEM;
			goto END_WITH_MUTEX;
		}
	}

	if ((sec_chk_addr != NULL) && (sec_buf_attr != NULL)) {
		//map rp_info to buf_id
		mvpu25_map_base_buf_id(buf_num,
						sec_chk_addr,
						sec_buf_attr,
						rp_num,
						target_buf_old_map,
						target_buf_old_base,
						target_buf_new_map,
						target_buf_new_base,
						buf_cmd_kreg,
						buf_cmd_next);
	} else {
		pr_info("[MVPU][Sec] [ERROR] sec_chk_addr or sec_buf_attr is NULL\n");
		ret = -ENOMEM;
		goto END_WITH_MUTEX;
	}

	if (algo_in_pool == true)
		goto REPLACE_MEM;

	// special case: check flow with image
	if ((sec_level != SEC_LVL_PROTECT) && (algo_in_img == true)) {
		ret = mvpu25_replace_img_knl(session,
					buf_num,
					sec_chk_addr,
					sec_buf_attr,
					rp_num,
					target_buf_old_map,
					target_buf_old_base,
					target_buf_old_offset,
					target_buf_new_map,
					target_buf_new_base,
					target_buf_new_offset,
					ker_bin_num,
					ker_bin_each_iova);

		if (mvpu_req->mpu_num != 0)
			ret = mvpu25_add_img_mpu(mvpu_req);

		goto END_WITH_MUTEX;
	}

	//mem pool: session/hash
	if (session_id != 0xFFFFFFFF) {
		if (hash_id == 0xFFFFFFFF) {
			hash_id = mvpu25_get_avail_hash_id(session_id);
#ifndef MVPU_SEC_USE_OLDEST_HASH_ID
			if (hash_id == 0xFFFFFFFF) {
				pr_info("[MVPU][SEC] [ERROR] out of hash pool\n");
				ret = -1;
				goto END_WITH_MUTEX;
			}
#endif

			mvpu25_clear_hash(session_id, hash_id);

			ret = mvpu25_update_hash_pool(session,
						algo_in_img,
						session_id,
						hash_id,
						batch_name_hash,
						buf_num,
						kreg_kva,
						sec_chk_addr,
						sec_buf_size,
						sec_buf_attr);

			if (ret != 0) {
				pr_info("[MVPU][SEC] hash error: ret = 0x%08x\n",
							ret);
				mvpu25_clear_hash(session_id, hash_id);
				goto END_WITH_MUTEX;
			}
		}
	} else {
		session_id = mvpu25_get_avail_session_id();
#ifndef MVPU_SEC_USE_OLDEST_SESSION_ID
		if (session_id == 0xFFFFFFFF) {
			pr_info("[MVPU][SEC] [ERROR] out of session pool\n");
			ret = -1;
			goto END_WITH_MUTEX;
		}
#endif

		//clear_session_id(session_id);
		mvpu25_update_session_id(session_id, session);

		hash_id = mvpu25_get_avail_hash_id(session_id);
#ifndef MVPU_SEC_USE_OLDEST_HASH_ID
		if (hash_id == 0xFFFFFFFF) {
			pr_info("[MVPU][SEC] [ERROR] out of hash pool\n");
			ret = -1;
			goto END_WITH_MUTEX;
		}
#endif

		mvpu25_clear_hash(session_id, hash_id);

		ret = mvpu25_update_hash_pool(session,
						algo_in_img,
						session_id,
						hash_id,
						batch_name_hash,
						buf_num,
						kreg_kva,
						sec_chk_addr,
						sec_buf_size,
						sec_buf_attr);

		if (ret != 0) {
			pr_info("[MVPU][SEC] hash error: ret = 0x%08x\n",
						ret);
			mvpu25_clear_hash(session_id, hash_id);
			goto END_WITH_MUTEX;
		}
	}

REPLACE_MEM:

	rp_skip_buf = kcalloc(buf_num, sizeof(uint32_t), GFP_KERNEL);
	if (rp_skip_buf == NULL) {
		ret = -ENOMEM;
		goto END_WITH_MUTEX;
	}

	if (algo_in_pool == true) {
		algo_all_same_buf = mvpu25_set_rp_skip_buf(session_id,
						hash_id,
						buf_num,
						sec_chk_addr,
						sec_buf_attr,
						rp_skip_buf);

		if (algo_all_same_buf == true || rp_num == 0) {
			if (mvpu_drv_loglv >= APUSYS_MVPU_LOG_DBG)
				pr_info("[MVPU][SEC] skip replacement\n");
			goto TRIGGER_SETTING;
		}
	}

	ret = mvpu25_update_new_base_addr(algo_in_img,
							algo_in_pool,
							session_id,
							hash_id,
							sec_chk_addr,
							sec_buf_attr,
							rp_skip_buf,
							rp_num,
							target_buf_new_map,
							target_buf_new_base,
							ker_bin_num,
							ker_bin_each_iova,
							kreg_kva);

	if (ret != 0)
		goto END_WITH_MUTEX;

#ifdef FULL_RP_INFO
	ret = mvpu25_save_hash_info(session_id,
						hash_id,
						buf_num,
						rp_num,
						sec_chk_addr,
						target_buf_old_base,
						target_buf_old_offset,
						target_buf_new_base,
						target_buf_new_offset,
						target_buf_old_map,
						target_buf_new_map);
#else
	ret = mvpu25_save_hash_info(session_id,
						hash_id,
						buf_num,
						sec_chk_addr);

	if (ret != 0)
		goto END_WITH_MUTEX;

#endif

	ret = mvpu25_replace_mem(session_id, hash_id,
					sec_buf_attr,
					algo_in_pool,
					rp_skip_buf,
					rp_num,
					target_buf_old_map,
					target_buf_old_base,
					target_buf_old_offset,
					target_buf_new_map,
					target_buf_new_base,
					target_buf_new_offset,
					kreg_kva);

	if (ret != 0)
		goto END_WITH_MUTEX;

	ret = mvpu25_update_mpu(mvpu_req, session_id, hash_id,
					sec_chk_addr, sec_buf_size, sec_buf_attr, true);

	if (mvpu_req->mpu_num != 0 && mvpu_drv_loglv >= APUSYS_MVPU_LOG_DBG) {
		pr_info("[MVPU][SEC] [MPU] mpu_num = %3d\n", mvpu_req->mpu_num);
		for (i = 0; i < MVPU_MPU_SEGMENT_NUMS; i++)
			pr_info("[MVPU][SEC] [MPU] drv mpu_reg[%3d] = 0x%08x\n",
						i, mvpu_req->mpu_seg[i]);
	}

	if (ret != 0)
		goto END_WITH_MUTEX;

TRIGGER_SETTING:
#ifdef MVPU_SEC_KREG_IN_POOL
	mvpu25_get_pool_kreg_iova(&kreg_iova_pool,
						session_id,
						hash_id,
						buf_cmd_kreg);

	// set KREG iova to uP
	mvpu_req->header.reg_bundle_setting_1.dwValue = kreg_iova_pool;
#endif

	if (kerarg_num != 0 && algo_in_pool == true) {
		ret = mvpu25_replace_kerarg(session,
					session_id, hash_id,
					kerarg_num,
					sec_chk_addr,
					kerarg_buf_id,
					kerarg_offset,
					kerarg_size,
					primem_num,
					primem_src_buf_id,
					primem_dst_buf_id,
					primem_src_offset,
					primem_dst_offset,
					primem_size);
	}

END_WITH_MUTEX:
	mutex_unlock(&mvpu25_pool_lock);

END:
	if (sec_chk_addr != NULL) {
		kfree(sec_chk_addr);
		sec_chk_addr = NULL;
	}

	if (sec_buf_size != NULL) {
		kfree(sec_buf_size);
		sec_buf_size = NULL;
	}

	if (sec_buf_attr != NULL) {
		kfree(sec_buf_attr);
		sec_buf_attr = NULL;
	}

	if (target_buf_old_base != NULL) {
		kfree(target_buf_old_base);
		target_buf_old_base = NULL;
	}

	if (target_buf_old_offset != NULL) {
		kfree(target_buf_old_offset);
		target_buf_old_offset = NULL;
	}

	if (target_buf_new_base != NULL) {
		kfree(target_buf_new_base);
		target_buf_new_base = NULL;
	}

	if (target_buf_new_offset != NULL) {
		kfree(target_buf_new_offset);
		target_buf_new_offset = NULL;
	}

	if (kerarg_buf_id != NULL) {
		kfree(kerarg_buf_id);
		kerarg_buf_id = NULL;
	}

	if (kerarg_offset != NULL) {
		kfree(kerarg_offset);
		kerarg_offset = NULL;
	}

	if (kerarg_size != NULL) {
		kfree(kerarg_size);
		kerarg_size = NULL;
	}

	if (primem_src_buf_id != NULL) {
		kfree(primem_src_buf_id);
		primem_src_buf_id = NULL;
	}

	if (primem_dst_buf_id != NULL) {
		kfree(primem_dst_buf_id);
		primem_dst_buf_id = NULL;
	}

	if (primem_dst_offset != NULL) {
		kfree(primem_dst_offset);
		primem_dst_offset = NULL;
	}

	if (primem_src_offset != NULL) {
		kfree(primem_src_offset);
		primem_src_offset = NULL;
	}

	if (primem_size != NULL) {
		kfree(primem_size);
		primem_size = NULL;
	}

	if (target_buf_old_map != NULL) {
		kfree(target_buf_old_map);
		target_buf_old_map = NULL;
	}

	if (target_buf_new_map != NULL) {
		kfree(target_buf_new_map);
		target_buf_new_map = NULL;
	}

	if (rp_skip_buf != NULL) {
		kfree(rp_skip_buf);
		rp_skip_buf = NULL;
	}

	if (ker_bin_each_iova != NULL) {
		kfree(ker_bin_each_iova);
		ker_bin_each_iova = NULL;
	}

	mdw_trace_end();
	return ret;
}

