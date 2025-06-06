/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 *
 * (C) COPYRIGHT 2018-2023 ARM Limited. All rights reserved.
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

#ifndef _KBASE_CSF_KCPU_H_
#define _KBASE_CSF_KCPU_H_

#include <mali_kbase_fence.h>
#include <mali_kbase_sync.h>

#include <linux/version_compat_defs.h>

/* The maximum number of KCPU commands in flight, enqueueing more commands
 * than this value shall block.
 */
#define KBASEP_KCPU_QUEUE_SIZE ((size_t)256)

/**
 * struct kbase_kcpu_command_import_info - Structure which holds information
 *				about the buffer to be imported
 *
 * @gpu_va:	Address of the buffer to be imported.
 */
struct kbase_kcpu_command_import_info {
	u64 gpu_va;
};

/**
 * struct kbase_kcpu_command_fence_info - Structure which holds information about the
 *                                        fence object enqueued in the kcpu command queue
 *
 * @fence_cb:      Fence callback
 * @fence:         Fence
 * @kcpu_queue:    kcpu command queue
 * @fence_has_force_signaled:	fence has forced signaled after fence timeouted
 */
struct kbase_kcpu_command_fence_info {
	struct dma_fence_cb fence_cb;
	struct dma_fence *fence;
	struct kbase_kcpu_command_queue *kcpu_queue;
	bool fence_has_force_signaled;
};

/**
 * struct kbase_kcpu_command_cqs_set_info - Structure which holds information
 *				about CQS objects for the kcpu CQS set command
 *
 * @objs:	Array of structures which define CQS objects to be used by
 *		the kcpu command.
 * @nr_objs:	Number of CQS objects in the array.
 */
struct kbase_kcpu_command_cqs_set_info {
	struct base_cqs_set *objs;
	unsigned int nr_objs;
};

/**
 * struct kbase_kcpu_command_cqs_wait_info - Structure which holds information
 *				about CQS objects for the kcpu CQS wait command
 *
 * @objs:	Array of structures which define CQS objects to be used by
 *		the kcpu command.
 * @signaled:	Bit array used to report the status of the CQS wait objects.
 *              1 is signaled, 0 otherwise.
 * @nr_objs:	Number of CQS objects in the array.
 * @inherit_err_flags:	Bit-pattern for the CQSs in the array who's error field
 *			to be served as the source for importing into the
 *			queue's error-state.
 */
struct kbase_kcpu_command_cqs_wait_info {
	struct base_cqs_wait_info *objs;
	unsigned long *signaled;
	unsigned int nr_objs;
	u32 inherit_err_flags;
};

/**
 * struct kbase_kcpu_command_cqs_set_operation_info - Structure which holds information
 *				about CQS objects for the kcpu CQS timeline set command
 *
 * @objs:	Array of structures which define CQS timeline objects to be used by
 *		the kcpu command.
 * @nr_objs:	Number of CQS objects in the array.
 */
struct kbase_kcpu_command_cqs_set_operation_info {
	struct base_cqs_set_operation_info *objs;
	unsigned int nr_objs;
};

/**
 * struct kbase_kcpu_command_cqs_wait_operation_info - Structure which holds information
 *				about CQS objects for the kcpu CQS timeline wait command
 *
 * @objs:	Array of structures which define CQS timeline objects to be used by
 *		the kcpu command.
 * @signaled:	Bit array used to report the status of the CQS wait objects.
 *              1 is signaled, 0 otherwise.
 * @nr_objs:	Number of CQS objects in the array.
 * @inherit_err_flags: Bit-pattern for CQSs in the array who's error field is to
 *                     be used as the source to import into the queue's error-state
 */
struct kbase_kcpu_command_cqs_wait_operation_info {
	struct base_cqs_wait_operation_info *objs;
	unsigned long *signaled;
	unsigned int nr_objs;
	u32 inherit_err_flags;
};

/**
 * struct kbase_kcpu_command_jit_alloc_info - Structure which holds information
 *				needed for the kcpu command for jit allocations
 *
 * @node:	Used to keep track of all JIT free/alloc commands in submission
 *		order. This must be located in the front of this struct to
 *		match that of kbase_kcpu_command_jit_free_info.
 * @info:	Array of objects of the struct base_jit_alloc_info type which
 *		specify jit allocations to be made by the kcpu command.
 * @count:	Number of jit alloc objects in the array.
 * @blocked:	Whether this allocation has been put into the pending list to
 *		be retried later.
 */
struct kbase_kcpu_command_jit_alloc_info {
	struct list_head node;
	struct base_jit_alloc_info *info;
	u8 count;
	bool blocked;
};

/**
 * struct kbase_kcpu_command_jit_free_info - Structure which holds information
 *				needed for the kcpu jit free command
 *
 * @node:	Used to keep track of all JIT free/alloc commands in submission
 *		order. This must be located in the front of this struct to
 *		match that of kbase_kcpu_command_jit_alloc_info.
 * @ids:	Array of identifiers of jit allocations which are to be freed
 *		by the kcpu command.
 * @count:	Number of elements in the array.
 */
struct kbase_kcpu_command_jit_free_info {
	struct list_head node;
	u8 *ids;
	u8 count;
};

/**
 * struct kbase_suspend_copy_buffer - information about the suspend buffer
 *		to be copied.
 *
 * @size:	size of the suspend buffer in bytes.
 * @pages:	pointer to an array of pointers to the pages which contain
 *		the user buffer.
 * @nr_pages:	number of pages.
 * @offset:	offset into the pages
 * @cpu_alloc:	Reference to physical pages of suspend buffer allocation.
 */
struct kbase_suspend_copy_buffer {
	size_t size;
	struct page **pages;
	unsigned int nr_pages;
	size_t offset;
	struct kbase_mem_phy_alloc *cpu_alloc;
};

#if IS_ENABLED(CONFIG_MALI_VECTOR_DUMP) || MALI_UNIT_TEST
/**
 * struct kbase_kcpu_command_group_suspend_info - structure which contains
 *		suspend buffer data captured for a suspended queue group.
 *
 * @sus_buf:		Pointer to the structure which contains details of the
 *			user buffer and its kernel pinned pages.
 * @group_handle:	Handle to the mapping of CSG.
 */
struct kbase_kcpu_command_group_suspend_info {
	struct kbase_suspend_copy_buffer *sus_buf;
	u8 group_handle;
};
#endif


/**
 * struct kbase_kcpu_command - Command which is to be part of the kernel
 *                            command queue
 *
 * @type:	Type of the command.
 * @enqueue_ts:	Denotes the relative time of enqueueing, a smaller value
 *		indicates that it has been enqueued earlier.
 * @info:	Structure which holds information about the command
 *		dependent on the command type.
 * @info.fence:              Fence
 * @info.cqs_wait:           CQS wait
 * @info.cqs_set:            CQS set
 * @info.cqs_wait_operation: CQS wait operation
 * @info.cqs_set_operation:  CQS set operation
 * @info.import:             import
 * @info.jit_alloc:          JIT allocation
 * @info.jit_free:           JIT deallocation
 * @info.suspend_buf_copy:   suspend buffer copy
 * @info.sample_time:        sample time
 */
struct kbase_kcpu_command {
	enum base_kcpu_command_type type;
	u64 enqueue_ts;
	union {
		struct kbase_kcpu_command_fence_info fence;
		struct kbase_kcpu_command_cqs_wait_info cqs_wait;
		struct kbase_kcpu_command_cqs_set_info cqs_set;
		struct kbase_kcpu_command_cqs_wait_operation_info cqs_wait_operation;
		struct kbase_kcpu_command_cqs_set_operation_info cqs_set_operation;
		struct kbase_kcpu_command_import_info import;
		struct kbase_kcpu_command_jit_alloc_info jit_alloc;
		struct kbase_kcpu_command_jit_free_info jit_free;
#if IS_ENABLED(CONFIG_MALI_VECTOR_DUMP) || MALI_UNIT_TEST
		struct kbase_kcpu_command_group_suspend_info suspend_buf_copy;
#endif
	} info;
};

/**
 * struct kbase_kcpu_command_queue - a command queue executed by the kernel
 *
 * @lock:			Lock to protect accesses to this queue.
 * @kctx:			The context to which this command queue belongs.
 * @commands:			Array of commands which have been successfully
 *				enqueued to this command queue.
 * @work:			struct work_struct which contains a pointer to
 *				the function which handles processing of kcpu
 *				commands enqueued into a kcpu command queue;
 *				part of kernel API for processing workqueues.
 *				This would be used if the context is not
 *				prioritised, otherwise it would be handled by
 *				kbase_csf_scheduler_kthread().
 * @high_prio_work:		A counterpart to @work, this queue would be
 *				added to a list to be processed by
 *				kbase_csf_scheduler_kthread() if it is
 *				prioritised.
 * @pending_kick:		Indicates that kbase_csf_scheduler_kthread()
 *				should re-evaluate pending commands for this
 *				queue. This would be set to false when the work
 *				is done. This is used mainly for
 *				synchronisation with queue termination.
 * @timeout_work:		struct work_struct which contains a pointer to the
 *				function which handles post-timeout actions
 *				queue when a fence signal timeout occurs.
 * @start_offset:		Index of the command to be executed next
 * @id:				KCPU command queue ID.
 * @num_pending_cmds:		The number of commands enqueued but not yet
 *				executed or pending
 * @cqs_wait_count:		Tracks the number of CQS wait commands enqueued
 * @fence_context:		The dma-buf fence context number for this kcpu
 *				queue. A unique context number is allocated for
 *				each kcpu queue.
 * @fence_seqno:		The dma-buf fence sequence number for the fence
 *				that is returned on the enqueue of fence signal
 *				command. This is increased every time the
 *				fence signal command is queued.
 * @fence_wait_processed:	Used to avoid reprocessing of the fence wait
 *				command which has blocked the processing of
 *				commands that follow it.
 * @enqueue_failed:		Indicates that no space has become available in
 *				the buffer since an enqueue operation failed
 *				because of insufficient free space.
 * @command_started:		Indicates that the command at the front of the
 *				queue has been started in a previous queue
 *				process, but was not completed due to some
 *				unmet dependencies. Ensures that instrumentation
 *				of the execution start of these commands is only
 *				fired exactly once.
 * @has_error:			Indicates that the kcpu queue is in error mode
 *				or without errors since last cleaned.
 * @jit_blocked:		Used to keep track of command queues blocked
 *				by a pending JIT allocation command.
 * @fence_timeout:		Timer used to detect the fence wait timeout.
 * @metadata:                   Metadata structure containing basic information about
 *                              this queue for any fence objects associated with this queue.
 * @fence_signal_timeout:	Timer used for detect a fence signal command has
 *				been blocked for too long.
 * @fence_signal_pending_cnt:	Enqueued fence signal commands in the queue.
 */
struct kbase_kcpu_command_queue {
	struct mutex lock;
	struct kbase_context *kctx;
	struct kbase_kcpu_command commands[KBASEP_KCPU_QUEUE_SIZE];
	struct work_struct work;
#if !IS_ENABLED(CONFIG_MALI_MTK_USE_WORKQUEUE_FOR_CSF_SCHEDULE)
	struct list_head high_prio_work;
	atomic_t pending_kick;
#else
	struct workqueue_struct *wq;
#endif /* CONFIG_MALI_MTK_USE_WORKQUEUE_FOR_CSF_SCHEDULE */
	struct work_struct timeout_work;
	u8 start_offset;
	u8 id;
	u16 num_pending_cmds;
	u32 cqs_wait_count;
	u64 fence_context;
	unsigned int fence_seqno;
	bool fence_wait_processed;
	bool enqueue_failed;
	bool command_started;
	struct list_head jit_blocked;
	bool has_error;
	struct timer_list fence_timeout;
#if IS_ENABLED(CONFIG_SYNC_FILE)
	struct kbase_kcpu_dma_fence_meta *metadata;
#endif /* CONFIG_SYNC_FILE */
	struct timer_list fence_signal_timeout;
	atomic_t fence_signal_pending_cnt;
#if IS_ENABLED(CONFIG_MALI_MTK_FENCE_DEBUG)
	/* For ARM kcpu command - FENCE_SIGNAL timeout */
	unsigned int fence_signal_command_timeout_counter;
	char fence_signal_command_timeout_fence[32];
	/* For ARM kcpu command - FENCE_WAIT timeout */
	unsigned int fence_wait_command_timeout_counter;
	char fence_wait_command_timeout_fence[32];
#endif /* CONFIG_MALI_MTK_FENCE_DEBUG */
};

/**
 * kbase_csf_kcpu_queue_new - Create new KCPU command queue.
 *
 * @kctx:	Pointer to the kbase context within which the KCPU command
 *		queue will be created.
 * @newq:	Pointer to the structure which contains information about
 *		the new KCPU command queue to be created.
 *
 * Return: 0 if successful or a negative error code on failure.
 */
int kbase_csf_kcpu_queue_new(struct kbase_context *kctx, struct kbase_ioctl_kcpu_queue_new *newq);

/**
 * kbase_csf_kcpu_queue_delete - Delete KCPU command queue.
 *
 * @kctx:	Pointer to the kbase context from which the KCPU command
 *		queue is to be deleted.
 * @del:	Pointer to the structure which specifies the KCPU command
 *		queue to be deleted.
 *
 * Return: 0 if successful or a negative error code on failure.
 */
int kbase_csf_kcpu_queue_delete(struct kbase_context *kctx,
				struct kbase_ioctl_kcpu_queue_delete *del);

/**
 * kbase_csf_kcpu_queue_process - Proces pending KCPU queue commands
 *
 * @queue:		The queue to process pending commands for
 * @drain_queue:	Whether to skip all blocking commands in the queue.
 *			This is expected to be set to true on queue
 *			termination.
 *
 * Return: 0 if successful or a negative error code on failure.
 */
void kbase_csf_kcpu_queue_process(struct kbase_kcpu_command_queue *queue, bool drain_queue);

/**
 * kbase_csf_kcpu_queue_enqueue - Enqueue a KCPU command into a KCPU command
 *				  queue.
 *
 * @kctx:	Pointer to the kbase context within which the KCPU command
 *		is to be enqueued into the KCPU command queue.
 * @enq:	Pointer to the structure which specifies the KCPU command
 *		as well as the KCPU command queue into which the command
 *		is to be enqueued.
 *
 * Return: 0 if successful or a negative error code on failure.
 */
int kbase_csf_kcpu_queue_enqueue(struct kbase_context *kctx,
				 struct kbase_ioctl_kcpu_queue_enqueue *enq);

/**
 * kbase_csf_kcpu_queue_context_init - Initialize the kernel CPU queues context
 *                                     for a GPU address space
 *
 * @kctx: Pointer to the kbase context being initialized.
 *
 * This function must be called only when a kbase context is instantiated.
 *
 * Return: 0 if successful or a negative error code on failure.
 */
int kbase_csf_kcpu_queue_context_init(struct kbase_context *kctx);

/**
 * kbase_csf_kcpu_queue_context_term - Terminate the kernel CPU queues context
 *                                     for a GPU address space
 * @kctx: Pointer to the kbase context being terminated.
 *
 * This function deletes any kernel CPU queues that weren't deleted before
 * context termination.
 *
 */
void kbase_csf_kcpu_queue_context_term(struct kbase_context *kctx);

#if IS_ENABLED(CONFIG_SYNC_FILE)
/* Test wrappers for dma fence operations. */
int kbase_kcpu_fence_signal_process(struct kbase_kcpu_command_queue *kcpu_queue,
				    struct kbase_kcpu_command_fence_info *fence_info);

int kbase_kcpu_fence_signal_init(struct kbase_kcpu_command_queue *kcpu_queue,
				 struct kbase_kcpu_command *current_command,
				 struct base_fence *fence, struct sync_file **sync_file, int *fd);
#endif /* CONFIG_SYNC_FILE */

/*
 * kbase_csf_kcpu_queue_halt_timers - Halt the KCPU fence timers associated with
 *                                    the kbase device.
 *
 * @kbdev: Kbase device
 *
 * Note that this function assumes that the caller has ensured that the
 * kbase_device::kctx_list does not get updated during this function's runtime.
 * At the moment, the function is only safe to call during system suspend, when
 * the device PM active count has reached zero.
 *
 * Return: 0 on success, negative value otherwise.
 */
int kbase_csf_kcpu_queue_halt_timers(struct kbase_device *kbdev);

/*
 * kbase_csf_kcpu_queue_resume_timers - Resume the KCPU fence timers associated
 *                                      with the kbase device.
 *
 * @kbdev: Kbase device
 *
 * Note that this function assumes that the caller has ensured that the
 * kbase_device::kctx_list does not get updated during this function's runtime.
 * At the moment, the function is only safe to call during system resume.
 */
void kbase_csf_kcpu_queue_resume_timers(struct kbase_device *kbdev);

bool kbase_kcpu_command_fence_has_force_signaled(struct kbase_kcpu_command_fence_info *fence_info);
#endif /* _KBASE_CSF_KCPU_H_ */
