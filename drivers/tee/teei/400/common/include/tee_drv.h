/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2016, Linaro Limited
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 *
 */

#ifndef __TEE_DRV_H
#define __TEE_DRV_H

#include <linux/types.h>
#include <linux/idr.h>
#include <linux/list.h>
#include <linux/device.h>
#include "tee.h"

/*
 * The file describes the API provided by the generic TEE driver to the
 * specific TEE driver.
 */

#define TEE_SHM_MAPPED		BIT(0)	/* Memory mapped by the kernel */
#define TEE_SHM_DMA_BUF		BIT(1)	/* Memory with dma-buf handle */
#define TEE_SHM_EXT_DMA_BUF	BIT(2)	/* Memory with dma-buf handle */
#define TEE_SHM_DMA_KERN_BUF	BIT(3)

struct tee_device;
struct tee_shm;
struct tee_shm_pool;

/**
 * struct tee_context - driver specific context on file pointer data
 * @teedev:	pointer to this drivers struct tee_device
 * @list_shm:	List of shared memory object owned by this context
 * @hostname:	to distinguish which TEE subsystem will serving client requests
 * @data:	driver specific context data, managed by the driver
 */
struct tee_context {
	struct tee_device *teedev;
	struct list_head list_shm;
	u8 hostname[TEE_MAX_HOSTNAME_SIZE];
	void *data;
	struct mutex mutex;
};

struct tee_param_memref {
	size_t shm_offs;
	size_t size;
	struct tee_shm *shm;
};

struct tee_param_value {
	u64 a;
	u64 b;
	u64 c;
};

struct tee_param {
	u64 attr;
	union {
		struct tee_param_memref memref;
		struct tee_param_value value;
	} u;
};

/**
 * struct tee_driver_ops - driver operations vtable
 * @get_version:	returns version of driver
 * @open:		called when the device file is opened
 * @release:		release this open file
 * @open_session:	open a new session
 * @close_session:	close a session
 * @invoke_func:	invoke a trusted function
 * @cancel_req:		request cancel of an ongoing invoke or open
 * @supp_revc:		called for supplicant to get a command
 * @supp_send:		called for supplicant to send a response
 */
struct tee_driver_ops {
	void (*get_version)(struct tee_device *teedev,
			    struct tee_ioctl_version_data *vers);
	int (*open)(struct tee_context *ctx);
	void (*release)(struct tee_context *ctx);
	int (*open_session)(struct tee_context *ctx,
			    struct tee_ioctl_open_session_arg *arg,
			    struct tee_param *param);
	int (*close_session)(struct tee_context *ctx, u32 session);
	int (*invoke_func)(struct tee_context *ctx,
			   struct tee_ioctl_invoke_arg *arg,
			   struct tee_param *param);
	int (*cancel_req)(struct tee_context *ctx, u32 cancel_id, u32 session);
	int (*supp_recv)(struct tee_context *ctx, u32 *func, u32 *num_params,
			 struct tee_param *param);
	int (*supp_send)(struct tee_context *ctx, u32 ret, u32 num_params,
			 struct tee_param *param);
};

/**
 * struct tee_desc - Describes the TEE driver to the subsystem
 * @name:	name of driver
 * @ops:	driver operations vtable
 * @owner:	module providing the driver
 * @flags:	Extra properties of driver, defined by TEE_DESC_* below
 */
#define TEE_DESC_PRIVILEGED	0x1
struct tee_desc {
	const char *name;
	const struct tee_driver_ops *ops;
	struct module *owner;
	u32 flags;
};

/**
 * isee_device_alloc() - Allocate a new struct tee_device instance
 * @teedesc:	Descriptor for this driver
 * @dev:	Parent device for this device
 * @pool:	Shared memory pool, NULL if not used
 * @driver_data: Private driver data for this device
 *
 * Allocates a new struct tee_device instance. The device is
 * removed by isee_device_unregister().
 *
 * @returns a pointer to a 'struct tee_device' or an ERR_PTR on failure
 */
struct tee_device *isee_device_alloc(const struct tee_desc *teedesc,
				    struct device *dev,
				    struct tee_shm_pool *pool,
				    void *driver_data);

/**
 * isee_device_register() - Registers a TEE device
 * @teedev:	Device to register
 *
 * isee_device_unregister() need to be called to remove the @teedev if
 * this function fails.
 *
 * @returns < 0 on failure
 */
int isee_device_register(struct tee_device *teedev);

/**
 * isee_device_unregister() - Removes a TEE device
 * @teedev:	Device to unregister
 *
 * This function should be called to remove the @teedev even if
 * isee_device_register() hasn't been called yet. Does nothing if
 * @teedev is NULL.
 */
void isee_device_unregister(struct tee_device *teedev);

/**
 * struct tee_shm_pool_mem_info - holds information needed to create a shared
 * memory pool
 * @vaddr:	Virtual address of start of pool
 * @paddr:	Physical address of start of pool
 * @size:	Size in bytes of the pool
 */
struct tee_shm_pool_mem_info {
	unsigned long vaddr;
	phys_addr_t paddr;
	size_t size;
};

/**
 * isee_shm_pool_alloc_res_mem() - Create a shared memory pool from reserved
 * memory range
 * @priv_info:	 Information for driver private shared memory pool
 * @dmabuf_info: Information for dma-buf shared memory pool
 *
 * Start and end of pools will must be page aligned.
 *
 * Allocation with the flag TEE_SHM_DMA_BUF set will use the range supplied
 * in @dmabuf, others will use the range provided by @priv.
 *
 * @returns pointer to a 'struct tee_shm_pool' or an ERR_PTR on failure.
 */
struct tee_shm_pool *
isee_shm_pool_alloc_res_mem(struct tee_shm_pool_mem_info *priv_info,
			   struct tee_shm_pool_mem_info *dmabuf_info);

/**
 * isee_shm_pool_free() - Free a shared memory pool
 * @pool:	The shared memory pool to free
 *
 * The must be no remaining shared memory allocated from this pool when
 * this function is called.
 */
void isee_shm_pool_free(struct tee_shm_pool *pool);

/**
 * isee_get_drvdata() - Return driver_data pointer
 * @returns the driver_data pointer supplied to isee_register().
 */
void *isee_get_drvdata(struct tee_device *teedev);

struct tee_shm *isee_shm_kalloc(struct tee_context *ctx, size_t size, u32 flags);
void isee_shm_kfree(struct tee_shm *shm);
/**
 * isee_shm_alloc() - Allocate shared memory
 * @ctx:	Context that allocates the shared memory
 * @size:	Requested size of shared memory
 * @flags:	Flags setting properties for the requested shared memory.
 *
 * Memory allocated as global shared memory is automatically freed when the
 * TEE file pointer is closed. The @flags field uses the bits defined by
 * TEE_SHM_* above. TEE_SHM_MAPPED must currently always be set. If
 * TEE_SHM_DMA_BUF global shared memory will be allocated and associated
 * with a dma-buf handle, else driver private memory.
 *
 * @returns a pointer to 'struct tee_shm'
 */
struct tee_shm *isee_shm_alloc_noid(struct tee_context *ctx, size_t size, u32 flags);
struct tee_shm *isee_shm_alloc(struct tee_context *ctx, size_t size, u32 flags);

/**
 * isee_shm_free() - Free shared memory
 * @shm:	Handle to shared memory to free
 */
void isee_shm_free(struct tee_shm *shm);

/**
 * isee_shm_put() - Decrease reference count on a shared memory handle
 * @shm:	Shared memory handle
 */
void isee_shm_put(struct tee_shm *shm);

/**
 * isee_shm_va2pa() - Get physical address of a virtual address
 * @shm:	Shared memory handle
 * @va:		Virtual address to tranlsate
 * @pa:		Returned physical address
 * @returns 0 on success and < 0 on failure
 */
int isee_shm_va2pa(struct tee_shm *shm, void *va, phys_addr_t *pa);

/**
 * isee_shm_pa2va() - Get virtual address of a physical address
 * @shm:	Shared memory handle
 * @pa:		Physical address to tranlsate
 * @va:		Returned virtual address
 * @returns 0 on success and < 0 on failure
 */
int isee_shm_pa2va(struct tee_shm *shm, phys_addr_t pa, void **va);

/**
 * isee_shm_get_va() - Get virtual address of a shared memory plus an offset
 * @shm:	Shared memory handle
 * @offs:	Offset from start of this shared memory
 * @returns virtual address of the shared memory + offs if offs is within
 *	the bounds of this shared memory, else an ERR_PTR
 */
void *isee_shm_get_va(struct tee_shm *shm, size_t offs);

/**
 * isee_shm_get_pa() - Get physical address of a shared memory plus an offset
 * @shm:	Shared memory handle
 * @offs:	Offset from start of this shared memory
 * @pa:		Physical address to return
 * @returns 0 if offs is within the bounds of this shared memory, else an
 *	error code.
 */
int isee_shm_get_pa(struct tee_shm *shm, size_t offs, phys_addr_t *pa);

/**
 * isee_shm_get_id() - Get id of a shared memory object
 * @shm:	Shared memory handle
 * @returns id
 */
int isee_shm_get_id(struct tee_shm *shm);

/**
 * isee_shm_get_from_id() - Find shared memory object and increase reference
 * count
 * @ctx:	Context owning the shared memory
 * @id:		Id of shared memory object
 * @returns a pointer to 'struct tee_shm' on success or an ERR_PTR on failure
 */
struct tee_shm *isee_shm_get_from_id(struct tee_context *ctx, int id);

static inline bool tee_param_is_memref(struct tee_param *param)
{
	switch (param->attr & TEE_IOCTL_PARAM_ATTR_TYPE_MASK) {
	case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT:
	case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT:
	case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT:
		return true;
	default:
		return false;
	}
}

struct tee_context *isee_client_open_context(struct tee_context *start,
			int (*match)(struct tee_ioctl_version_data *,
				const void *),
			const void *data, struct tee_ioctl_version_data *vers);

void isee_client_close_context(struct tee_context *ctx);

void isee_client_get_version(struct tee_context *ctx,
			struct tee_ioctl_version_data *vers);

int isee_client_open_session(struct tee_context *ctx,
			struct tee_ioctl_open_session_arg *arg,
			struct tee_param *param);

int isee_client_close_session(struct tee_context *ctx, u32 session);

int isee_client_invoke_func(struct tee_context *ctx,
			struct tee_ioctl_invoke_arg *arg,
			struct tee_param *param);

#endif /*__TEE_DRV_H*/
