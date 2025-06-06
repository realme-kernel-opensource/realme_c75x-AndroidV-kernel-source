// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2016, Linaro Limited
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 *
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <tee_client_api.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/uaccess.h>
#include <linux/mman.h>
#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>
#include <tee_drv.h>

#include "tee_private.h"
#include "tee.h"

/* How many device sequence numbers will be tried before giving up */
#define TEEC_MAX_DEV_SEQ	10
#define DEFAULT_CAPABILITY "bta_loader"

static inline long ioctl(struct file *filp, unsigned int cmd, void *arg)
{
	long ret;

	if (!filp)
		return -EBADF;

	ret = tee_k_ioctl(filp, cmd, (unsigned long)arg);

	return ret;
}

static DEFINE_MUTEX(teec_mutex);

static inline void teec_mutex_lock(struct mutex *mu)
{
	mutex_lock(mu);
}

static inline void teec_mutex_unlock(struct mutex *mu)
{
	mutex_unlock(mu);
}

static struct file *teec_open_dev(const char *devname, const char *capabilities)
{
	struct file *file;
	struct tee_ioctl_set_hostname_arg arg;
	int err;

	file = kzalloc(sizeof(struct file), GFP_KERNEL);
	if (file == NULL) {
		IMSG_ERROR("No memory for struct file!\n");
		return NULL;
	}

	memset(file, 0, sizeof(struct file));

	err = tee_k_open(file);
	if (err != 0) {
		IMSG_ERROR("Failed to call the tee_k_open %d.\n", err);
		kfree(file);
		return NULL;
	}


	memset(&arg, 0, sizeof(arg));

	if (capabilities && (strcmp(capabilities, "tta") == 0)) {
		IMSG_DEBUG("client request serviced by GPTEE\n");
		strncpy((char *)arg.hostname, capabilities,
				TEE_MAX_HOSTNAME_SIZE-1);
	} else {
		IMSG_DEBUG("client request serviced by BTA Loader\n");
		strncpy((char *)arg.hostname, DEFAULT_CAPABILITY,
				TEE_MAX_HOSTNAME_SIZE-1);
	}

	err = ioctl(file, TEE_IOC_SET_HOSTNAME, &arg);
	if (err) {
		IMSG_ERROR("TEE_IOC_SET_HOSTNAME failed\n");
		goto exit;
	}

	return file;

exit:
	tee_k_release(file);
	kfree(file);

	return ERR_PTR(err);
}

TEEC_Result TEEC_InitializeContext(const char *name, struct TEEC_Context *ctx)
{
	char devname[128];
	struct file *fd;
	size_t n;
	int ret = 0;

	if (!ctx)
		return TEEC_ERROR_BAD_PARAMETERS;

	for (n = 0; n < TEEC_MAX_DEV_SEQ; n++) {
		ret = snprintf(devname, sizeof(devname), "/dev/isee_tee%zu", n);
		if (ret <= 0) {
			IMSG_ERROR("snprintf failed ret = %d\n", ret);
			continue;
		}

		fd = teec_open_dev(devname, name);
		if (!IS_ERR_OR_NULL(fd)) {
			ctx->fd = fd;
			return TEEC_SUCCESS;
		}
	}

	return TEEC_ERROR_ITEM_NOT_FOUND;
}
#if !IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
EXPORT_SYMBOL(TEEC_InitializeContext);
#endif

void TEEC_FinalizeContext(struct TEEC_Context *ctx)
{
	if (ctx) {
		tee_k_release(ctx->fd);
		kfree(ctx->fd);
		ctx->fd = NULL;
	}
}
#if !IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
EXPORT_SYMBOL(TEEC_FinalizeContext);
#endif

static TEEC_Result teec_pre_process_tmpref(struct TEEC_Context *ctx,
			uint32_t param_type,
			struct TEEC_TempMemoryReference *tmpref,
			struct tee_ioctl_param *param,
			struct TEEC_SharedMemory *shm)
{
	TEEC_Result res;

	switch (param_type) {
	case TEEC_MEMREF_TEMP_INPUT:
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
		shm->flags = TEEC_MEM_INPUT;
		break;
	case TEEC_MEMREF_TEMP_OUTPUT:
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
		shm->flags = TEEC_MEM_OUTPUT;
		break;
	case TEEC_MEMREF_TEMP_INOUT:
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
		shm->flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
		break;
	default:
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	shm->size = tmpref->size;

	res = TEEC_AllocateSharedMemory(ctx, shm);
	if (res != TEEC_SUCCESS)
		return res;

	memcpy(shm->buffer, tmpref->buffer, tmpref->size);
	param->size = tmpref->size;
	param->shm_id = shm->id;
	return TEEC_SUCCESS;
}

static TEEC_Result teec_pre_process_whole(
			struct TEEC_RegisteredMemoryReference *memref,
			struct tee_ioctl_param *param)
{
	const uint32_t inout = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	uint32_t flags = memref->parent->flags & inout;
	struct TEEC_SharedMemory *shm;

	if (flags == inout)
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
	else if (flags & TEEC_MEM_INPUT)
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
	else if (flags & TEEC_MEM_OUTPUT)
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
	else
		return TEEC_ERROR_BAD_PARAMETERS;

	shm = memref->parent;
	/*
	 * We're using a shadow buffer in this reference, copy the real buffer
	 * into the shadow buffer if needed. We'll copy it back once we've
	 * returned from the call to secure world.
	 */
	if (shm->shadow_buffer && (flags & TEEC_MEM_INPUT))
		memcpy(shm->shadow_buffer, shm->buffer, shm->size);

	param->shm_id = shm->id;
	param->size = shm->size;
	return TEEC_SUCCESS;
}

static TEEC_Result teec_pre_process_partial(uint32_t param_type,
			struct TEEC_RegisteredMemoryReference *memref,
			struct tee_ioctl_param *param)
{
	uint32_t req_shm_flags;
	struct TEEC_SharedMemory *shm;

	switch (param_type) {
	case TEEC_MEMREF_PARTIAL_INPUT:
		req_shm_flags = TEEC_MEM_INPUT;
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
		break;
	case TEEC_MEMREF_PARTIAL_OUTPUT:
		req_shm_flags = TEEC_MEM_OUTPUT;
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
		break;
	case TEEC_MEMREF_PARTIAL_INOUT:
		req_shm_flags = TEEC_MEM_OUTPUT | TEEC_MEM_INPUT;
		param->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
		break;
	default:
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	shm = memref->parent;

	if ((shm->flags & req_shm_flags) != req_shm_flags)
		return TEEC_ERROR_BAD_PARAMETERS;

	/*
	 * We're using a shadow buffer in this reference, copy the real buffer
	 * into the shadow buffer if needed. We'll copy it back once we've
	 * returned from the call to secure world.
	 */
	if (shm->shadow_buffer && param_type != TEEC_MEMREF_PARTIAL_OUTPUT)
		memcpy((char *)shm->shadow_buffer + memref->offset,
		       (char *)shm->buffer + memref->offset, memref->size);

	param->shm_id = shm->id;
	param->shm_offs = memref->offset;
	param->size = memref->size;
	return TEEC_SUCCESS;
}

static TEEC_Result teec_pre_process_operation(struct TEEC_Context *ctx,
			struct TEEC_Operation *operation,
			struct tee_ioctl_param *params,
			struct TEEC_SharedMemory *shms)
{
	TEEC_Result res;
	size_t n;

	memset(shms, 0, sizeof(struct TEEC_SharedMemory) *
			TEEC_CONFIG_PAYLOAD_REF_COUNT);
	if (!operation) {
		memset(params, 0, sizeof(struct tee_ioctl_param) *
				  TEEC_CONFIG_PAYLOAD_REF_COUNT);
		return TEEC_SUCCESS;
	}

	for (n = 0; n < TEEC_CONFIG_PAYLOAD_REF_COUNT; n++) {
		uint32_t param_type;

		param_type = TEEC_PARAM_TYPE_GET(operation->paramTypes, n);
		switch (param_type) {
		case TEEC_NONE:
			params[n].attr = param_type;
			break;
		case TEEC_VALUE_INPUT:
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			params[n].attr = param_type;
			params[n].a = operation->params[n].value.a;
			params[n].b = operation->params[n].value.b;
			break;
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			res = teec_pre_process_tmpref(ctx, param_type,
				&operation->params[n].tmpref, params + n,
				shms + n);
			if (res != TEEC_SUCCESS)
				return res;
			break;
		case TEEC_MEMREF_WHOLE:
			res = teec_pre_process_whole(
					&operation->params[n].memref,
					params + n);
			if (res != TEEC_SUCCESS)
				return res;
			break;
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
			res = teec_pre_process_partial(param_type,
				&operation->params[n].memref, params + n);
			if (res != TEEC_SUCCESS)
				return res;
			break;
		default:
			return TEEC_ERROR_BAD_PARAMETERS;
		}
	}

	return TEEC_SUCCESS;
}

static void teec_post_process_tmpref(uint32_t param_type,
			struct TEEC_TempMemoryReference *tmpref,
			struct tee_ioctl_param *param,
			struct TEEC_SharedMemory *shm)
{
	if (param_type != TEEC_MEMREF_TEMP_INPUT) {
		if (param->size <= tmpref->size && tmpref->buffer)
			memcpy(tmpref->buffer, shm->buffer,
			       param->size);

		tmpref->size = param->size;
	}
}

static void teec_post_process_whole(
			struct TEEC_RegisteredMemoryReference *memref,
			struct tee_ioctl_param *param)
{
	struct TEEC_SharedMemory *shm = memref->parent;

	if (shm->flags & TEEC_MEM_OUTPUT) {

		/*
		 * We're using a shadow buffer in this reference, copy back
		 * the shadow buffer into the real buffer now that we've
		 * returned from secure world.
		 */
		if (shm->shadow_buffer && param->size <= shm->size)
			memcpy(shm->buffer, shm->shadow_buffer,
			       param->size);

		memref->size = param->size;
	}
}

static void teec_post_process_partial(uint32_t param_type,
			struct TEEC_RegisteredMemoryReference *memref,
			struct tee_ioctl_param *param)
{
	if (param_type != TEEC_MEMREF_PARTIAL_INPUT) {
		struct TEEC_SharedMemory *shm = memref->parent;

		/*
		 * We're using a shadow buffer in this reference, copy back
		 * the shadow buffer into the real buffer now that we've
		 * returned from secure world.
		 */
		if (shm->shadow_buffer && param->size <= memref->size)
			memcpy((char *)shm->buffer + memref->offset,
			       (char *)shm->shadow_buffer + memref->offset,
			       param->size);

		memref->size = param->size;
	}
}

static void teec_post_process_operation(struct TEEC_Operation *operation,
			struct tee_ioctl_param *params,
			struct TEEC_SharedMemory *shms)
{
	size_t n;

	if (!operation)
		return;

	for (n = 0; n < TEEC_CONFIG_PAYLOAD_REF_COUNT; n++) {
		uint32_t param_type;

		param_type = TEEC_PARAM_TYPE_GET(operation->paramTypes, n);
		switch (param_type) {
		case TEEC_VALUE_INPUT:
			break;
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			operation->params[n].value.a = params[n].a;
			operation->params[n].value.b = params[n].b;
			break;
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			teec_post_process_tmpref(param_type,
				&operation->params[n].tmpref, params + n,
				shms + n);
			break;
		case TEEC_MEMREF_WHOLE:
			teec_post_process_whole(&operation->params[n].memref,
						params + n);
			break;
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
			teec_post_process_partial(param_type,
				&operation->params[n].memref, params + n);
			break;
		default:
			break;
		}
	}
}

static void teec_free_temp_refs(struct TEEC_Operation *operation,
			struct TEEC_SharedMemory *shms)
{
	size_t n;

	if (!operation)
		return;

	for (n = 0; n < TEEC_CONFIG_PAYLOAD_REF_COUNT; n++) {
		switch (TEEC_PARAM_TYPE_GET(operation->paramTypes, n)) {
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			TEEC_ReleaseSharedMemory(shms + n);
			break;
		default:
			break;
		}
	}
}

static TEEC_Result ioctl_errno_to_res(int err)
{
	switch (err) {
	case -ENOMEM:
		return TEEC_ERROR_OUT_OF_MEMORY;
	default:
		return TEEC_ERROR_GENERIC;
	}
}

static void uuid_to_octets(uint8_t d[TEE_IOCTL_UUID_LEN],
				const struct TEEC_UUID *s)
{
	d[0] = s->timeLow >> 24;
	d[1] = s->timeLow >> 16;
	d[2] = s->timeLow >> 8;
	d[3] = s->timeLow;
	d[4] = s->timeMid >> 8;
	d[5] = s->timeMid;
	d[6] = s->timeHiAndVersion >> 8;
	d[7] = s->timeHiAndVersion;
	memcpy(d + 8, s->clockSeqAndNode, sizeof(s->clockSeqAndNode));
}

TEEC_Result TEEC_OpenSession(struct TEEC_Context *ctx,
			struct TEEC_Session *session,
			const struct TEEC_UUID *destination,
			uint32_t connection_method,
			const void *connection_data,
			struct TEEC_Operation *operation,
			uint32_t *ret_origin)
{
	uint64_t buf[(sizeof(struct tee_ioctl_open_session_arg) +
			TEEC_CONFIG_PAYLOAD_REF_COUNT *
				sizeof(struct tee_ioctl_param)) /
			sizeof(uint64_t)] = { 0 };
	struct tee_ioctl_buf_data buf_data;
	struct tee_ioctl_open_session_arg *arg;
	struct tee_ioctl_param *params;
	TEEC_Result res;
	uint32_t eorig;
	struct TEEC_SharedMemory shm[TEEC_CONFIG_PAYLOAD_REF_COUNT];
	int rc;

	(void)&connection_data;

	if (!ctx || !session) {
		eorig = TEEC_ORIGIN_API;
		res = TEEC_ERROR_BAD_PARAMETERS;
		goto out;
	}

	buf_data.buf_ptr = (uintptr_t)buf;
	buf_data.buf_len = sizeof(buf);

	arg = (struct tee_ioctl_open_session_arg *)buf;
	arg->num_params = TEEC_CONFIG_PAYLOAD_REF_COUNT;
	params = (struct tee_ioctl_param *)(arg + 1);

	uuid_to_octets(arg->uuid, destination);
	arg->clnt_login = connection_method;

	res = teec_pre_process_operation(ctx, operation, params, shm);
	if (res != TEEC_SUCCESS) {
		eorig = TEEC_ORIGIN_API;
		goto out_free_temp_refs;
	}

	rc = ioctl(ctx->fd, TEE_IOC_OPEN_SESSION, &buf_data);
	if (rc) {
		IMSG_ERROR("TEE_IOC_OPEN_SESSION failed");
		eorig = TEEC_ORIGIN_COMMS;
		res = ioctl_errno_to_res(rc);
		goto out_free_temp_refs;
	}
	res = arg->ret;
	eorig = arg->ret_origin;
	if (res == TEEC_SUCCESS) {
		session->ctx = ctx;
		session->session_id = arg->session;
	}
	teec_post_process_operation(operation, params, shm);

out_free_temp_refs:
	teec_free_temp_refs(operation, shm);
out:
	if (ret_origin)
		*ret_origin = eorig;
	return res;
}
#if !IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
EXPORT_SYMBOL(TEEC_OpenSession);
#endif

void TEEC_CloseSession(struct TEEC_Session *session)
{
	struct tee_ioctl_close_session_arg arg;

	if (!session)
		return;

	arg.session = session->session_id;
	if (ioctl(session->ctx->fd, TEE_IOC_CLOSE_SESSION, &arg))
		IMSG_ERROR("Failed to close session 0x%x",
				session->session_id);
}
#if !IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
EXPORT_SYMBOL(TEEC_CloseSession);
#endif

TEEC_Result TEEC_InvokeCommand(struct TEEC_Session *session, uint32_t cmd_id,
		struct TEEC_Operation *operation, uint32_t *error_origin)
{
	uint64_t buf[(sizeof(struct tee_ioctl_invoke_arg) +
			TEEC_CONFIG_PAYLOAD_REF_COUNT *
				sizeof(struct tee_ioctl_param)) /
			sizeof(uint64_t)] = { 0 };
	struct tee_ioctl_buf_data buf_data;
	struct tee_ioctl_invoke_arg *arg;
	struct tee_ioctl_param *params;
	TEEC_Result res;
	uint32_t eorig;
	struct TEEC_SharedMemory shm[TEEC_CONFIG_PAYLOAD_REF_COUNT];
	int rc;

	if (!session) {
		eorig = TEEC_ORIGIN_API;
		res = TEEC_ERROR_BAD_PARAMETERS;
		goto out;
	}

	buf_data.buf_ptr = (uintptr_t)buf;
	buf_data.buf_len = sizeof(buf);

	arg = (struct tee_ioctl_invoke_arg *)buf;
	arg->num_params = TEEC_CONFIG_PAYLOAD_REF_COUNT;
	params = (struct tee_ioctl_param *)(arg + 1);

	arg->session = session->session_id;
	arg->func = cmd_id;

	if (operation) {
		teec_mutex_lock(&teec_mutex);
		operation->session = session;
		teec_mutex_unlock(&teec_mutex);
	}

	res = teec_pre_process_operation(session->ctx, operation, params, shm);
	if (res != TEEC_SUCCESS) {
		eorig = TEEC_ORIGIN_API;
		goto out_free_temp_refs;
	}

	rc = ioctl(session->ctx->fd, TEE_IOC_INVOKE, &buf_data);
	if (rc) {
		IMSG_ERROR("TEE_IOC_INVOKE failed");
		eorig = TEEC_ORIGIN_COMMS;
		res = ioctl_errno_to_res(rc);
		goto out_free_temp_refs;
	}

	res = arg->ret;
	eorig = arg->ret_origin;
	teec_post_process_operation(operation, params, shm);

out_free_temp_refs:
	teec_free_temp_refs(operation, shm);
out:
	if (error_origin)
		*error_origin = eorig;
	return res;
}
#if !IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
EXPORT_SYMBOL(TEEC_InvokeCommand);
#endif

void TEEC_RequestCancellation(struct TEEC_Operation *operation)
{
	struct tee_ioctl_cancel_arg arg;
	struct TEEC_Session *session;
	int ret;

	if (!operation)
		return;

	teec_mutex_lock(&teec_mutex);
	session = operation->session;
	teec_mutex_unlock(&teec_mutex);

	if (!session)
		return;

	arg.session = session->session_id;
	arg.cancel_id = 0;

	ret = ioctl(session->ctx->fd, TEE_IOC_CANCEL, &arg);
	if (ret)
		IMSG_ERROR("TEE_IOC_CANCEL: %d", ret);
}
#if !IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
EXPORT_SYMBOL(TEEC_RequestCancellation);
#endif

TEEC_Result TEEC_RegisterSharedMemory(struct TEEC_Context *ctx,
					struct TEEC_SharedMemory *shm)
{
	size_t s;
	struct tee_shm *tee_shm;
	struct tee_context *tee_ctx;

	if (!ctx || !shm)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (!shm->flags || (shm->flags & ~(TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)))
		return TEEC_ERROR_BAD_PARAMETERS;

	s = shm->size;
	if (!s)
		s = 8;

	tee_ctx = ctx->fd->private_data;

	mutex_lock(&tee_ctx->mutex);

	tee_shm = isee_shm_kalloc(tee_ctx, s, TEE_SHM_DMA_KERN_BUF | TEE_SHM_MAPPED);
	if (IS_ERR(shm)) {
		IMSG_ERROR("%s:%d Failed to get tee_shm!\n", __func__, __LINE__);
		mutex_unlock(&tee_ctx->mutex);
		return TEEC_ERROR_GENERIC;
	}

	shm->id = tee_shm->id;
	shm->priv = tee_shm;
	shm->shadow_buffer = tee_shm->kaddr;
	shm->alloced_size = s;
	shm->registered_fd = -1;

	mutex_unlock(&tee_ctx->mutex);

	return TEEC_SUCCESS;
}
#if !IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
EXPORT_SYMBOL(TEEC_RegisterSharedMemory);
#endif

TEEC_Result TEEC_RegisterSharedMemoryFileDescriptor(struct TEEC_Context *ctx,
						struct TEEC_SharedMemory *shm,
						int fd)
{
	struct tee_ioctl_shm_register_fd_data data;
	int rfd;

	if (!ctx || !shm || fd < 0)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (!shm->flags || (shm->flags & ~(TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)))
		return TEEC_ERROR_BAD_PARAMETERS;

	memset(&data, 0, sizeof(data));
	data.fd = fd;
	rfd = ioctl(ctx->fd, TEE_IOC_SHM_REGISTER_FD, &data);
	if (rfd < 0)
		return TEEC_ERROR_BAD_PARAMETERS;

	shm->priv = NULL;
	shm->buffer = NULL;
	shm->shadow_buffer = NULL;
	shm->registered_fd = rfd;
	shm->id = data.id;
	shm->size = data.size;
	return TEEC_SUCCESS;
}
#if !IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
EXPORT_SYMBOL(TEEC_RegisterSharedMemoryFileDescriptor);
#endif

TEEC_Result TEEC_AllocateSharedMemory(struct TEEC_Context *ctx,
					struct TEEC_SharedMemory *shm)
{
	size_t s;
	struct tee_shm *tee_shm;
	struct tee_context *tee_ctx;

	if (!ctx || !shm)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (!shm->flags || (shm->flags & ~(TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)))
		return TEEC_ERROR_BAD_PARAMETERS;

	s = shm->size;
	if (!s)
		s = 8;

	tee_ctx = ctx->fd->private_data;

	mutex_lock(&tee_ctx->mutex);

	tee_shm = isee_shm_kalloc(tee_ctx, s, TEE_SHM_DMA_KERN_BUF | TEE_SHM_MAPPED);
	if (IS_ERR(shm)) {
		IMSG_ERROR("%s:%d Failed to get tee_shm!\n", __func__, __LINE__);
		mutex_unlock(&tee_ctx->mutex);
		return TEEC_ERROR_GENERIC;
	}

	shm->id = tee_shm->id;
	shm->priv = tee_shm;
	shm->buffer = tee_shm->kaddr;
	shm->shadow_buffer = NULL;
	shm->alloced_size = s;
	shm->registered_fd = -1;

	mutex_unlock(&tee_ctx->mutex);

	return TEEC_SUCCESS;
}
#if !IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
EXPORT_SYMBOL(TEEC_AllocateSharedMemory);
#endif

void TEEC_ReleaseSharedMemory(struct TEEC_SharedMemory *shm)
{
	struct tee_context *tee_ctx;
	struct tee_shm *tee_shm;

	if (!shm || shm->id == -1)
		return;

	tee_shm = shm->priv;
	tee_ctx = tee_shm->ctx;

	mutex_lock(&tee_ctx->mutex);

	isee_shm_kfree(tee_shm);

	shm->id = -1;
	shm->priv = NULL;
	if (shm->shadow_buffer == NULL)
		shm->buffer = NULL;
	shm->shadow_buffer = NULL;
	shm->registered_fd = -1;

	mutex_unlock(&tee_ctx->mutex);

}
#if !IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
EXPORT_SYMBOL(TEEC_ReleaseSharedMemory);
#endif

#if IS_ENABLED(CONFIG_MTK_TEE_GP_COORDINATOR)
#include "tee_impl_api.h"
static const struct gp_api_impl_info microtrust_gp_api_export_info = {
	.name = "microtrust 400",
	.size = {
		.sharedmemory_max = TEEC_CONFIG_SHAREDMEM_MAX_SIZE,
		.context          = sizeof(struct TEEC_Context),
		.session          = sizeof(struct TEEC_Session),
		.sharedmemory     = sizeof(struct TEEC_SharedMemory),
		.operation        = sizeof(struct TEEC_Operation),
	},
	.ops = {
		.initializecontext    = &TEEC_InitializeContext,
		.finalizecontext      = &TEEC_FinalizeContext,
		.registersharedmemory = &TEEC_RegisterSharedMemory,
		.allocatesharedmemory = &TEEC_AllocateSharedMemory,
		.releasesharedmemory  = &TEEC_ReleaseSharedMemory,
		.opensession          = &TEEC_OpenSession,
		.closesession         = &TEEC_CloseSession,
		.invokecommand        = &TEEC_InvokeCommand,
		.requestcancellation  = &TEEC_RequestCancellation,
	},
};

bool register_gp_api(void)
{
	return gp_api_impl_add(&microtrust_gp_api_export_info);
}
#endif
