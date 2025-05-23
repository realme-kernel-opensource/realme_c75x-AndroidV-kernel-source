/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2016, Linaro Limited
 * Copyright (c) 2024 MediaTek Inc.
 */
#ifndef TEE_PRIVATE_H
#define TEE_PRIVATE_H

#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/types.h>

#define TEE_DEVICE_FLAG_REGISTERED	0x1
#define TEE_MAX_DEV_NAME_LEN		32

/**
 * struct tee_device - TEE Device representation
 * @name:	name of device
 * @desc:	description of device
 * @id:		unique id of device
 * @flags:	represented by TEE_DEVICE_FLAG_REGISTERED above
 * @dev:	embedded basic device structure
 * @cdev:	embedded cdev
 * @num_users:	number of active users of this device
 * @c_no_user:	completion used when unregistering the device
 * @mutex:	mutex protecting @num_users and @idr
 * @idr:	register of user space shared memory objects allocated or
 *		registered on this device
 * @pool:	shared memory pool
 */
struct tee_device {
	char name[TEE_MAX_DEV_NAME_LEN];
	const struct tee_desc *desc;
	int id;
	unsigned int flags;

	struct device dev;
	struct cdev cdev;

	size_t num_users;
	struct completion c_no_users;
	struct mutex mutex;	/* protects num_users and idr */

	struct idr idr;
	struct tee_shm_pool *pool;
};

int tee_shm_init(void);

int tee_shm_get_fd(struct tee_shm *shm);

bool tee_device_get(struct tee_device *teedev);
void tee_device_put(struct tee_device *teedev);

void teedev_ctx_get(struct tee_context *ctx);
void teedev_ctx_put(struct tee_context *ctx);

struct tee_shm *tee_shm_alloc_user_buf(struct tee_context *ctx, size_t size);
struct tee_shm *tee_shm_register_user_buf(struct tee_context *ctx,
					  unsigned long addr, size_t length);

int tee_client_register_shm(struct tee_context *ctx, unsigned long addr,
				  size_t length, int *pID);
void tee_client_unregister_shm(struct tee_context *ctx, int id);

#endif /*TEE_PRIVATE_H*/
