// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include "cmdq_sec_mtee.h"

static bool cmdq_mtee;

void cmdq_sec_mtee_setup_context(struct cmdq_sec_mtee_context *tee)
{
	static const char ta_uuid[32] = "com.mediatek.geniezone.cmdq";
	static const char wsm_uuid[32] = "com.mediatek.geniezone.srv.mem";
	struct arm_smccc_res res;

	memset(tee, 0, sizeof(*tee));
	strncpy(tee->ta_uuid, ta_uuid, sizeof(ta_uuid));
	strncpy(tee->wsm_uuid, wsm_uuid, sizeof(wsm_uuid));

	arm_smccc_smc(0xBC00000B, 1, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0 == 1)
		cmdq_mtee = true;
	cmdq_msg("%s a0:%#lx cmdq_mtee:%d", __func__, res.a0, cmdq_mtee);
}

s32 cmdq_sec_mtee_allocate_shared_memory(struct cmdq_sec_mtee_context *tee,
	const dma_addr_t MVABase, const u32 size)
{
	s32 status;

	if (!cmdq_mtee) {
		cmdq_msg("%s cmdq_mtee:%d not support", __func__, cmdq_mtee);
		return 0;
	}

	tee->mem_param.size = size;
	tee->mem_param.buffer = (void *)(unsigned long)MVABase;
	status = KREE_RegisterSharedmem(tee->wsm_pHandle,
		&tee->mem_handle, &tee->mem_param);
	if (status != TZ_RESULT_SUCCESS)
		cmdq_err("%s: session:%#x handle:%#x size:%#x buffer:%#lx",
			__func__, tee->wsm_pHandle, tee->mem_handle,
			tee->mem_param.size, (unsigned long)tee->mem_param.buffer);
	else
		cmdq_log("%s: session:%#x handle:%#x size:%#x buffer:%#lx",
			__func__, tee->wsm_pHandle, tee->mem_handle,
			tee->mem_param.size, (unsigned long)tee->mem_param.buffer);
	return status;
}

s32 cmdq_sec_mtee_allocate_wsm(struct cmdq_sec_mtee_context *tee,
	void **wsm_buffer, u32 size, void **wsm_buf_ex, u32 size_ex,
	void **wsm_buf_ex2, u32 size_ex2)
{
	s32 status;

	if (!cmdq_mtee) {
		cmdq_msg("%s cmdq_mtee:%d not support", __func__, cmdq_mtee);
		return 0;
	}

	if (!wsm_buffer || !wsm_buf_ex || !wsm_buf_ex2)
		return -EINVAL;

	/* region_id = 0, mapAry = NULL for continuous */
	*wsm_buffer = kzalloc(size, GFP_KERNEL);
	if (!*wsm_buffer)
		return -ENOMEM;

	tee->wsm_param.size = size;
	tee->wsm_param.buffer = (void *)(u64)virt_to_phys(*wsm_buffer);
	status = KREE_RegisterSharedmem(tee->wsm_pHandle,
		&tee->wsm_handle, &tee->wsm_param);
	if (status != TZ_RESULT_SUCCESS) {
		cmdq_err("%s: session:%#x handle:%#x size:%#x buffer:%p:%#lx",
			__func__, tee->wsm_pHandle, tee->wsm_handle,
			tee->wsm_param.size, *wsm_buffer, (unsigned long)*wsm_buffer);
		return status;
	}
	cmdq_log("%s: session:%#x handle:%#x size:%#x buffer:%p:%#lx",
		__func__, tee->wsm_pHandle, tee->wsm_handle,
		tee->wsm_param.size, *wsm_buffer, (unsigned long)*wsm_buffer);

	*wsm_buf_ex = kzalloc(size_ex, GFP_KERNEL);
	if (!*wsm_buf_ex)
		return -ENOMEM;

	tee->wsm_ex_param.size = size_ex;
	tee->wsm_ex_param.buffer = (void *)(u64)virt_to_phys(*wsm_buf_ex);
	status = KREE_RegisterSharedmem(tee->wsm_pHandle,
		&tee->wsm_ex_handle, &tee->wsm_ex_param);
	if (status != TZ_RESULT_SUCCESS)
		cmdq_err("%s: session:%#x handle:%#x size:%#x buffer:%p:%#lx",
			__func__, tee->wsm_pHandle, tee->wsm_ex_handle,
			tee->wsm_ex_param.size, *wsm_buf_ex, (unsigned long)*wsm_buf_ex);
	else
		cmdq_log("%s: session:%#x handle:%#x size:%#x buffer:%p:%#lx",
			__func__, tee->wsm_pHandle, tee->wsm_ex_handle,
			tee->wsm_ex_param.size, *wsm_buf_ex, (unsigned long)*wsm_buf_ex);

	*wsm_buf_ex2 = kzalloc(size_ex2, GFP_KERNEL);
	if (!*wsm_buf_ex2)
		return -ENOMEM;

	tee->wsm_ex2_param.size = size_ex2;
	tee->wsm_ex2_param.buffer = (void *)(u64)virt_to_phys(*wsm_buf_ex2);
	status = KREE_RegisterSharedmem(tee->wsm_pHandle,
		&tee->wsm_ex2_handle, &tee->wsm_ex2_param);
	if (status != TZ_RESULT_SUCCESS)
		cmdq_err("%s: session:%#x handle:%#x size:%#x buffer:%p:%#lx",
			__func__, tee->wsm_pHandle, tee->wsm_ex2_handle,
			tee->wsm_ex2_param.size, *wsm_buf_ex2, (unsigned long)*wsm_buf_ex2);
	else
		cmdq_log("%s: session:%#x handle:%#x size:%#x buffer:%p:%#lx",
			__func__, tee->wsm_pHandle, tee->wsm_ex2_handle,
			tee->wsm_ex2_param.size, *wsm_buf_ex2, (unsigned long)*wsm_buf_ex2);

	return status;
}

s32 cmdq_sec_mtee_free_wsm(struct cmdq_sec_mtee_context *tee,
	void **wsm_buffer, void **wsm_buf_ex, void **wsm_buf_ex2)
{
	if (!cmdq_mtee) {
		cmdq_msg("%s cmdq_mtee:%d not support", __func__, cmdq_mtee);
		return 0;
	}

	if (!wsm_buffer || !wsm_buf_ex || !wsm_buf_ex2)
		return -EINVAL;

	KREE_UnregisterSharedmem(tee->wsm_pHandle, tee->wsm_handle);
	kfree(*wsm_buffer);
	*wsm_buffer = NULL;

	kfree(*wsm_buf_ex);
	*wsm_buf_ex = NULL;

	kfree(*wsm_buf_ex2);
	*wsm_buf_ex2 = NULL;

	return 0;
}

s32 cmdq_sec_mtee_open_session(struct cmdq_sec_mtee_context *tee,
	void *wsm_buffer)
{
	s32 status;

	if (!cmdq_mtee) {
		cmdq_msg("%s cmdq_mtee:%d not support", __func__, cmdq_mtee);
		return 0;
	}

	status = KREE_CreateSession(tee->ta_uuid, &tee->pHandle);
	if (status != TZ_RESULT_SUCCESS) {
		cmdq_err("%s:%s:%d", __func__, tee->ta_uuid, status);
		return status;
	}
	cmdq_log("%s: tee:%p ta_uuid:%s session:%#x",
		__func__, tee, tee->ta_uuid, tee->pHandle);

	status = KREE_CreateSession(tee->wsm_uuid, &tee->wsm_pHandle);
	if (status != TZ_RESULT_SUCCESS) {
		cmdq_err("%s:%s:%d", __func__, tee->ta_uuid, status);
		return status;
	}
	cmdq_log("%s: tee:%p ta_uuid:%s session:%#x",
		__func__, tee, tee->wsm_uuid, tee->wsm_pHandle);
	return status;
}

s32 cmdq_sec_mtee_close_session(struct cmdq_sec_mtee_context *tee)
{
	if (!cmdq_mtee) {
		cmdq_msg("%s cmdq_mtee:%d not support", __func__, cmdq_mtee);
		return 0;
	}

	KREE_CloseSession(tee->wsm_pHandle);
	return KREE_CloseSession(tee->pHandle);
}

s32 cmdq_sec_mtee_execute_session(struct cmdq_sec_mtee_context *tee,
	u32 cmd, s32 timeout_ms, bool share_mem_ex, bool share_mem_ex2)
{
	s32 status;
	u32 types = TZ_ParamTypes4(TZPT_VALUE_INOUT,
		share_mem_ex ? TZPT_VALUE_INOUT : TZPT_NONE,
		share_mem_ex2 ? TZPT_VALUE_INOUT : TZPT_NONE,
		cmd == 4 ? TZPT_VALUE_INOUT : TZPT_NONE); // TODO
	union MTEEC_PARAM param[4];

	if (!cmdq_mtee) {
		cmdq_msg("%s cmdq_mtee:%d not support", __func__, cmdq_mtee);
		return 0;
	}

	param[0].value.a = tee->wsm_handle;
	param[0].value.b = tee->wsm_param.size;
	param[1].value.a = tee->wsm_ex_handle;
	param[1].value.b = tee->wsm_ex_param.size;
	param[2].value.a = tee->wsm_ex2_handle;
	param[2].value.b = tee->wsm_ex2_param.size;
	param[3].value.a = tee->mem_handle;
	param[3].value.b = (u32)(unsigned long)tee->mem_param.buffer;
	cmdq_log("%s: tee:%p session:%#x cmd:%u timeout:%d",
		__func__, tee, tee->pHandle, cmd, timeout_ms);
	cmdq_log("%s: mem_ex:%d mem_ex2:%d types:%#x",
		__func__, share_mem_ex, share_mem_ex2, types);
	cmdq_log("wsm:%#x:%#x ex:%#x:%#x ex2:%#x:%#x mem:%#x:%#x",
		param[0].value.a, param[0].value.b,
		param[1].value.a, param[1].value.b,
		param[2].value.a, param[2].value.b,
		param[3].value.a, param[3].value.b);

	status = KREE_TeeServiceCallPlus(tee->pHandle, cmd, types, param, 0);
	if (status != TZ_RESULT_SUCCESS)
		cmdq_err("%s:%d cmd:%u", __func__, status, cmd);
	else
		cmdq_msg("%s:%d cmd:%u", __func__, status, cmd);
	return status;
}

MODULE_LICENSE("GPL v2");
