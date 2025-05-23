// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/atomic.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include <mt-plat/sync_write.h>
#include "sspm_define.h"
#include "sspm_helper.h"
#include "sspm_mbox.h"
#include "sspm_ipi.h"
#include "sspm_ipi_mbox.h"
#define IPI_MONITOR
#include "sspm_ipi_define.h"
#include <linux/of.h>

#define TIMEOUT_COMPLETE msecs_to_jiffies(2000)

#ifdef SSPM_STF_ENABLED
#include <linux/cpu.h>
#include "sspm_stf.h"
#endif

/* #define GET_IPI_TIMESTAMP */
#ifdef GET_IPI_TIMESTAMP
#include <linux/cpu.h>
#define IPI_TS_TEST_MAX		20
#define IPI_TS_TEST_PIN		IPI_ID_PMIC_WRAP
u64 ipi_t0[IPI_TS_TEST_MAX];
u64 ipi_t4[IPI_TS_TEST_MAX];
u64 ipi_t5[IPI_TS_TEST_MAX];
static int test_cnt;
#endif

#ifdef IPI_MONITOR
#define IPI_MONITOR_TIMESTAMP
struct ipi_monitor {
	/* 0: has no timestamp of t1/t2/t3 otherwise 1*/
	unsigned int has_time: 1,
	/* 0: no IPI, 1: t1 finished, 2: t2 finished, 3: t3 finished */
		 state   : 2,
	/* count of the IPI pin used */
		 seqno   : 29;
#ifdef IPI_MONITOR_TIMESTAMP
	unsigned long long t0;
	unsigned long long t4;
	unsigned long long t5;
#endif /* IPI_MONITOR_TIMESTAMP */
};
static struct ipi_monitor ipimon[IPI_ID_TOTAL];
static int ipi_last;
static spinlock_t lock_monitor;
#ifdef IPI_MONITOR_TIMESTAMP
static int err_pin;
static unsigned long long err_ts;
#endif /* IPI_MONITOR_TIMESTAMP */
static void ipi_monitor_dump_timeout(int mid, int opts)
{
	int i;
	unsigned long flags = 0;

	spin_lock_irqsave(&lock_monitor, flags);
#ifdef IPI_MONITOR_TIMESTAMP
	err_pin = -1;
	err_ts = 0xffffffffffffffffLL;
	for (i = 0; i < IPI_ID_TOTAL; i++) {
		if ((ipimon[i].state == 1) || (ipimon[i].state == 2)) {
			if (ipimon[i].t0 < err_ts) {
				err_ts = ipimon[i].t0;
				err_pin = i;
			}
		}
	}
	if (err_pin >= 0) {
		pr_err("Error: possible error IPI %d pin=%s: t0=%lld\n",
			   err_pin, pin_name[err_pin], err_ts);
	}
#endif /* IPI_MONITOR_TIMESTAMP */
	pr_err("Error: IPI %d pin=%s mode=%d timeout at %lld (lastOK IPI=%d)\n",
		   mid, pin_name[mid], opts, cpu_clock(0), ipi_last);
#ifdef IPI_MONITOR_TIMESTAMP
	for (i = 0; i < IPI_ID_TOTAL; i++) {
		if ((ipimon[i].state == 0) || (ipimon[i].state == 3))
			pr_err("IPI %d: seqno=%d, state=%d, t0=%lld, t4=%lld, t5=%lld\n",
				i, ipimon[i].seqno, ipimon[i].state,
				ipimon[i].t0, ipimon[i].t4, ipimon[i].t5);
		else
			pr_err("IPI %d: seqno=%d, state_err=%d, t0=%lld, t4=%lld, t5=%lld\n",
				i, ipimon[i].seqno, ipimon[i].state,
				ipimon[i].t0, ipimon[i].t4, ipimon[i].t5);
	}
#else
	for (i = 0; i < IPI_ID_TOTAL; i++) {
		if ((ipimon[i].state == 0) || (ipimon[i].state == 3))
			pr_err("IPI %d: seqno=%d, state=%d\n",
				   i, ipimon[i].seqno, ipimon[i].state);
		else
			pr_err("IPI %d: seqno=%d, state_err=%d\n",
				   i, ipimon[i].seqno, ipimon[i].state);
	}
#endif /* IPI_MONITOR_TIMESTAMP */
	spin_unlock_irqrestore(&lock_monitor, flags);
	pr_err("Error: SSPM IPI=%d timeout\n", mid);
	sspm_ipi_timeout_cb(mid);
	BUG_ON(1);
}
#endif

static void ipi_check_send(int mid)
{
#ifdef SSPM_STF_ENABLED
	if (test_table[mid].data)
		test_table[mid].start_us = (unsigned int)(cpu_clock(0)/1000);
#endif /* SSPM_STF_ENABLED */
#ifdef GET_IPI_TIMESTAMP
	if ((mid == IPI_TS_TEST_PIN) && (test_cnt < IPI_TS_TEST_MAX))
		ipi_t0[test_cnt] = cpu_clock(0);
#endif /* GET_IPI_TIMESTAMP */
#ifdef IPI_MONITOR
	ipimon[mid].seqno++;
#ifdef IPI_MONITOR_TIMESTAMP
	ipimon[mid].t0 = cpu_clock(0);
	ipimon[mid].t4 = 0;
	ipimon[mid].t5 = 0;
#endif /* IPI_MONITOR_TIMESTAMP */
	ipimon[mid].state = 1;
#endif /* IPI_MONITOR */
}

static void ipi_check_ack(int mid, int opts, int ret)
{
	if (ret == 0) {
#ifdef SSPM_STF_ENABLED
		if (test_table[mid].data) {
			struct chk_data *pdata = test_table[mid].data;
			int cnt = test_table[mid].test_cnt;

			pdata[cnt].time_spent = ((unsigned int)
				(cpu_clock(0)/1000) - test_table[mid].start_us);
			if (retbuf)
				pdata[cnt].ack_data_feedback =
					*((unsigned int *)retbuf);
			else
				pdata[cnt].ack_data_feedback = 0;
			test_table[mid].test_cnt++;
		}
#endif /* SSPM_STF_ENABLED */
#ifdef GET_IPI_TIMESTAMP
		if ((mid == IPI_TS_TEST_PIN) && (test_cnt < IPI_TS_TEST_MAX)) {
			ipi_t5[test_cnt] = cpu_clock(0);
			test_cnt++;
		}
		if (test_cnt >= IPI_TS_TEST_MAX) {
			int i;

			for (i = 0; i < IPI_TS_TEST_MAX; i++)
				pr_err("IPI %d: t0=%llu, t4=%llu, t5=%llu\n",
					   i, ipi_t0[i], ipi_t4[i], ipi_t5[i]);
			test_cnt = 0;
		}
#endif /* GET_IPI_TIMESTAMP */
#ifdef IPI_MONITOR
#ifdef IPI_MONITOR_TIMESTAMP
		ipimon[mid].t5 = cpu_clock(0);
#endif /* IPI_MONITOR_TIMESTAMP */
		ipimon[mid].state = 3;
		ipi_last = mid;
	} else { /* timeout case */
		ipi_monitor_dump_timeout(mid, opts);
	}
#else
	}
#endif /* IPI_MONITOR */

}

atomic_t lock_send[TOTAL_SEND_PIN];
atomic_t lock_ack[TOTAL_SEND_PIN];
spinlock_t lock_polling[TOTAL_SEND_PIN];
/* used for IPI module isr to sync with its task */
struct completion sema_ipi_task[TOTAL_RECV_PIN];
struct mutex mutex_ipi_reg;
static int sspm_ipi_inited;
static unsigned int ipi_isr_cb(unsigned int mbox, void __iomem *base,
	unsigned int irq);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_LEGACY)
#define SEND_TBL_ELEM_NUM (5)
#define MBOX_TBL_ELEM_NUM (2)
bool sspm_prepare_ipi_mbox_context(struct platform_device *pdev)
{
	int of_ret;
	u32 i, ret;
	u32 send_count, mbox_count, pin_name_count;
	u32 pins_size, mbox_id, m_retdata, m_lock, m_polling;
	u32 mbox_table_end_index;
	u32 pin_mbox0_used, pin_mbox1_used;
	const char *pin_name_pt;

	send_count = of_property_count_u32_elems(
				pdev->dev.of_node, "sspm_ipi_send_pin_tbl")
				/ SEND_TBL_ELEM_NUM;
	if (send_count <= 0) {
		pr_debug("[SSPM] sspm_ipi_send_pin_tbl is not found, send_count: = (%d)\n",
			send_count);
		return false;
	}

	mbox_count = of_property_count_u32_elems(
				pdev->dev.of_node, "mbox_tbl")
				/ MBOX_TBL_ELEM_NUM;
	if (mbox_count <= 0) {
		pr_debug("[SSPM] mbox_tbl is not found, mbox_count: = (%d)\n", mbox_count);
		return false;
	}

	pin_name_count = of_property_count_strings(pdev->dev.of_node, "pin_name_tbl");
	if (pin_name_count <= 0) {
		pr_debug("[SSPM] pin_name_tbl is not found, pin_name_count: = (%d)\n",
			pin_name_count);
		return false;
	}

	/* Pin Send table */
	for (i = 0; i < send_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"sspm_ipi_send_pin_tbl",
				i * SEND_TBL_ELEM_NUM,
				&pins_size);
		if (ret) {
			pr_debug("[SSPM]%s:Cannot get pins_size(%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		pr_debug("[SSPM] sspm_ipi_send_pin_tbl, pins_size: = (%d)\n", pins_size);

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"sspm_ipi_send_pin_tbl",
				i * SEND_TBL_ELEM_NUM + 1,
				&mbox_id);
		if (ret) {
			pr_debug("[SSPM]%s:Cannot get mbox_id (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		pr_debug("[SSPM] sspm_ipi_send_pin_tbl, mbox_id: = (%d)\n", mbox_id);

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"sspm_ipi_send_pin_tbl",
				i * SEND_TBL_ELEM_NUM + 2,
				&m_retdata);
		if (ret) {
			pr_debug("[SSPM]%s:Cannot get m_retdata (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		pr_debug("[SSPM] sspm_ipi_send_pin_tbl, m_retdata: = (%d)\n", m_retdata);

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"sspm_ipi_send_pin_tbl",
				i * SEND_TBL_ELEM_NUM + 3,
				&m_lock);
		if (ret) {
			pr_debug("[SSPM]%s:Cannot get m_lock (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		pr_debug("[SSPM] sspm_ipi_send_pin_tbl, m_lock: = (%d)\n", m_retdata);

		ret = of_property_read_u32_index(pdev->dev.of_node,
				"sspm_ipi_send_pin_tbl",
				i * SEND_TBL_ELEM_NUM + 4,
				&m_polling);
		if (ret) {
			pr_debug("[SSPM]%s:Cannot get m_polling (%d):%d\n", __func__, i, __LINE__);
			return false;
		}
		pr_debug("[SSPM] sspm_ipi_send_pin_tbl, m_polling: = (%d)\n", m_polling);

		send_pintable[i].mbox = mbox_id;
		send_pintable[i].size = pins_size;
		if (i == 0 || i == IPI_MBOX0_USERS) {
			send_pintable[i].slot = 0;
			if (i == IPI_MBOX0_USERS)
				pin_mbox0_used = send_pintable[i-1].size + send_pintable[i-1].slot;
		} else
			send_pintable[i].slot = send_pintable[i-1].size + send_pintable[i-1].slot;

		pr_debug("[SSPM] sspm_ipi_send_pin_tbl, pins_offset: = (%d)\n",
			send_pintable[i].slot);
		send_pintable[i].retdata = m_retdata;
		send_pintable[i].lock = m_lock;
		send_pintable[i].polling = m_polling;
	}
	pin_mbox1_used = send_pintable[i-1].size + send_pintable[i-1].slot;
	/* Pin Send table */

	/* Mbox table */
	mbox_table[0].used_slot = pin_mbox0_used;
	mbox_table[1].used_slot = pin_mbox1_used;
	for (i = 0; i < mbox_count; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
				"mbox_tbl",
				i * MBOX_TBL_ELEM_NUM,
				&mbox_table_end_index);
		if (ret) {
			pr_debug("[SSPM]%s:Cannot get mbox_table_end_index(%d):%d\n", __func__, i,
				 __LINE__);
			return false;
		}
		pr_debug("[SSPM] mbox_tbl, mbox_table_end_index: = (%d)\n", pins_size);
		mbox_table[i].end = mbox_table_end_index;
	}
	/* Mbox table */

	/* Pin name table */
	for (i = 0; i < pin_name_count; ++i) {
		of_ret = of_property_read_string_index(pdev->dev.of_node, "pin_name_tbl", i,
						    &pin_name_pt);
		if (of_ret < 0) {
			pr_debug("[SSPM]%s:Cannot get pin_name_pt (%d):%d\n", __func__, i,
				 __LINE__);
			return false;
		}
		pr_debug("[SSPM] pin_name_tbl, pin_name_pt : = (%s)\n", pin_name_pt);
		pin_name[i] = pin_name_pt;
		pr_debug("[SSPM] pin_name[%d] : = (%p)\n", i, &pin_name[i]);
	}
	/* Pin name table */

	return true;
}
#endif /*CONFIG_MTK_TINYSYS_SSPM_LEGACY*/

int sspm_ipi_init(struct platform_device *pdev)
{
	int i, ret;
	struct _pin_send *pin;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_LEGACY)
	if (!sspm_prepare_ipi_mbox_context(pdev)) {
		pr_notice("Error while preparing ipi mbox table\n");
		return -1;
	}
#endif

#ifdef IPI_MONITOR
	spin_lock_init(&lock_monitor);
#ifdef IPI_MONITOR_TIMESTAMP
	for (i = 0; i < IPI_ID_TOTAL; i++)
		ipimon[i].has_time = 1;
#endif /* IPI_MONITOR_TIMESTAMP */
#endif /* IPI_MONITOR */

	mutex_init(&mutex_ipi_reg);
	for (i = 0; i < TOTAL_SEND_PIN; i++) {

		mutex_init(&send_pintable[i].mutex_send);
		init_completion(&send_pintable[i].comp_ack);

		atomic_set(&lock_send[i], 1);
		atomic_set(&lock_ack[i], 0);
		spin_lock_init(&lock_polling[i]);
	}

	/* IPI HW initialize and ISR registration */
	if (sspm_mbox_init(IPI_MBOX_MODE, IPI_MBOX_TOTAL, ipi_isr_cb) != 0) {
		pr_err("Error: sspm_mbox_init failed\n");
		return -1;
	}

	for (i = 0; i < TOTAL_SEND_PIN; i++) {
		pin = &(send_pintable[i]);
		pin->prdata = sspm_mbox_addr(pin->mbox, pin->slot);
	}

	ret = check_table_tag(IPI_MBOX_TOTAL);
	if (ret == 0)
		sspm_ipi_inited = 1;

	return ret;
}

extern int sspm_ipi_is_inited(void)
{
	return sspm_ipi_inited;
}

int sspm_ipi_recv_registration(int mid, struct ipi_action *act)
{
	struct _pin_recv *pin;

	if (sspm_ipi_inited == 0)
		return IPI_SERVICE_NOT_INITED;

	if ((mid < 0) || (mid >= TOTAL_RECV_PIN))
		return IPI_SERVICE_NOT_AVAILABLE;
	if (act == NULL)
		return IPI_REG_ACTION_ERROR;

	pin = &(recv_pintable[mid]);
	act->id = mid;
	act->lock = NULL;

	mutex_lock(&mutex_ipi_reg);
	if (pin->act != NULL) {
		mutex_unlock(&mutex_ipi_reg);
		return IPI_REG_ALREADY;
	}

	mutex_unlock(&mutex_ipi_reg);
	init_completion(&sema_ipi_task[mid]);
	pin->act = act;

	return IPI_REG_OK;
}
EXPORT_SYMBOL(sspm_ipi_recv_registration);

int sspm_ipi_recv_registration_ex(int mid, spinlock_t *lock,
	struct ipi_action *act)
{
	int ret = IPI_REG_OK;

	ret = sspm_ipi_recv_registration(mid, act);
	if (ret != IPI_REG_OK)
		return ret;

	act->lock = lock;
	return IPI_REG_OK;
}
EXPORT_SYMBOL(sspm_ipi_recv_registration_ex);

int sspm_ipi_recv_wait(int mid)
{
	struct _pin_recv *pin;

	if (sspm_ipi_inited == 0)
		return IPI_SERVICE_NOT_INITED;

	if ((mid < 0) || (mid >= TOTAL_RECV_PIN))
		return IPI_SERVICE_NOT_AVAILABLE;

	pin = &(recv_pintable[mid]);
	wait_for_completion(&sema_ipi_task[mid]);

	/* if the pin is waiting async data, eliminate multiple completions */
	if (pin->act->lock)
		while (try_wait_for_completion(&sema_ipi_task[mid]))
			;

	return 0;
}
EXPORT_SYMBOL(sspm_ipi_recv_wait);

void sspm_ipi_recv_complete(int mid)
{
	complete(&sema_ipi_task[mid]);
}
EXPORT_SYMBOL(sspm_ipi_recv_complete);

int sspm_ipi_recv_unregistration(int mid)
{
	struct _pin_recv *pin;

	pin = &(recv_pintable[mid]);
	pin->act = NULL;
	return IPI_REG_OK;
}
EXPORT_SYMBOL(sspm_ipi_recv_unregistration);

static void ipi_do_ack(struct _mbox_info *mbox, unsigned int in_irq,
	void __iomem *base)
{
	/* executed from ISR */
	int idx_end = mbox->end;
	int idx_start = mbox->start;
	int i;
	struct _pin_send *pin = &(send_pintable[idx_start]);

	for (i = idx_start; i <= idx_end; i++, pin++) {
		if ((in_irq & 0x01) == 0x01) { /* irq bit enable */

			atomic_inc(&lock_ack[i]);
			/* check if pin user send in WAIT mode,
			 * wait lock & continue if not
			 */
			if (mutex_is_locked(&pin->mutex_send)) { /* WAIT mode */

#ifdef GET_IPI_TIMESTAMP
				if ((i == IPI_TS_TEST_PIN) &&
					  (test_cnt < IPI_TS_TEST_MAX))
					ipi_t4[test_cnt] = cpu_clock(0);
#endif /* GET_IPI_TIMESTAMP */
#ifdef IPI_MONITOR
#ifdef IPI_MONITOR_TIMESTAMP
				ipimon[i].t4 = cpu_clock(0);
#endif /* IPI_MONITOR_TIMESTAMP */
				ipimon[i].state = 2;
#endif /* IPI_MONITOR */

				complete(&pin->comp_ack);
			}
		}
		in_irq >>= 1;
	}
}

static int handle_action(struct ipi_action *action, void *mbox_addr,
	int bytelen)
{
	/* if user has no data, just wakeup user without data */
	if (action->data == NULL)
		return 1;

	/* if user async send without waiting ACK from SSPM      */
	/* use spin lock to mempcy without overwriting user data */
	if (action->lock) {
		if (spin_trylock(action->lock)) {
			memcpy_from_sspm(action->data, mbox_addr, bytelen);
		} else {
			/* Users has lock. Just drop data */
			return 0;
		}
		spin_unlock(action->lock);
	} else {
		memcpy_from_sspm(action->data, mbox_addr, bytelen);
	}
	return 1;
}

static void ipi_do_recv(struct _mbox_info *mbox, unsigned int in_irq,
	void __iomem *base)
{
	/* executed from ISR */
	/* get the value from INT_IRQ_x (MD32 side) or OUT_IRQ_0 (Linux side) */
	int idx_end = mbox->end;
	int idx_start = mbox->start;
	int i, ret;
	struct _pin_recv *pin;
	struct ipi_action *action;

	if (in_irq == 0)
		return;

	/* check each bit for interrupt triggered */
	/* the bit is used to determine the index of callback array */
	pin = &(recv_pintable[idx_start]);
	for (i = idx_start; i <= idx_end; i++, pin++) {
		if ((in_irq & 0x01) == 0x01) { /* irq bit enable */
			action = pin->act;
			if (action != NULL) {
				/* do the action */
				ret = handle_action(action, (void *)
				(base + (pin->slot * SSPM_MBOX_SLOT_SIZE)),
				pin->size * SSPM_MBOX_SLOT_SIZE);
				if (ret)
					complete(&sema_ipi_task[i]);
			}
		} /* check bit is enabled */
		in_irq >>= 1;
	} /* check INT_IRQ bits */
}

int sspm_ipi_send_async(int mid, int opts, void *buffer, int slot)
{
	int mbno, ret, lock = 0;
	struct _pin_send *pin;
	struct _mbox_info *mbox;
	static int timeout;

	if (sspm_ipi_inited == 0)
		return IPI_SERVICE_NOT_INITED;

	if ((mid < 0) || (mid >= TOTAL_SEND_PIN))
		return IPI_SERVICE_NOT_AVAILABLE;

	pin = &(send_pintable[mid]);
	if (!pin->async)
		return IPI_PIN_MISUES;
	if (slot > pin->size)
		return IPI_NO_MEMORY;

	ipi_check_send(mid);

	mbno = pin->mbox;
	mbox = &(mbox_table[mbno]);

	if (!(opts & IPI_OPT_REDEF_MASK)) {
		lock = pin->lock & IPI_LOCK_ORIGINAL;
	} else {
		if (opts & IPI_OPT_LOCK_MASK) {
			lock = 1;
			pin->lock |= 0x6;
		} else {
			pin->lock |= 0x4;
		}
	}

	if (lock == 0) { /* use mutex */
		mutex_lock(&pin->mutex_send);
	} else { /* use spin method */
		timeout = 0xffff;
		while (atomic_read(&lock_send[mid]) == 0) {
			timeout--;
			udelay(10); /* fix me later, should we add this one? */
			if (timeout == 0) {
				if (pin->lock & IPI_LOCK_CHANGE)
					pin->lock &= IPI_LOCK_ORIGINAL;

				return IPI_TIMEOUT_AVL;
			}
		}
		atomic_dec(&lock_send[mid]);
	}

	atomic_set(&lock_ack[mid], 0);
	mbno = pin->mbox;
	mbox = &(mbox_table[mbno]);
	/* note: the bit of INT(OUT)_IRQ is depending on mid */
	if (slot == 0)
		slot = pin->size;

	ret = sspm_mbox_send(mbno, pin->slot, mid - mbox->start, buffer, slot);
	if (ret != 0) {
#ifdef IPI_MONITOR
		ipimon[mid].seqno--;
#endif
		/* release lock */
		if (lock == 0) /* use mutex */
			mutex_unlock(&pin->mutex_send);
		else
			atomic_inc(&lock_send[mid]);

		if (pin->lock & IPI_LOCK_CHANGE)
			pin->lock &= IPI_LOCK_ORIGINAL;

		return IPI_HW_ERROR;
	}
	return IPI_DONE;
}
EXPORT_SYMBOL(sspm_ipi_send_async);

int sspm_ipi_send_async_wait(int mid, int opts, void *retbuf)
{
	int slot = 1;

	if (retbuf == NULL)
		slot = 0;

	return sspm_ipi_send_async_wait_ex(mid, opts, retbuf, slot);
}
EXPORT_SYMBOL(sspm_ipi_send_async_wait);

int sspm_ipi_send_async_wait_ex(int mid, int opts, void *retbuf, int retslot)
{
	int ret = 0, lock = 0, polling __maybe_unused = 0;
	struct _pin_send *pin;
	unsigned long wait_comp;

	if ((mid < 0) || (mid >= TOTAL_SEND_PIN))
		return IPI_SERVICE_NOT_AVAILABLE;

	pin = &(send_pintable[mid]);
	if (!pin->async)
		return IPI_PIN_MISUES;
	if (retslot > pin->size)
		return IPI_NO_MEMORY;

	if (!(opts & IPI_OPT_REDEF_MASK)) {
		lock = pin->lock & IPI_LOCK_ORIGINAL;
		polling = pin->polling;
	} else {
		if (opts & IPI_OPT_LOCK_MASK) {
			lock = 1;
			pin->lock |= 0x6;
			if (opts & IPI_OPT_POLLING_MASK)
				polling = 1;
		} else {
			pin->lock |= 0x4;
		}
	}

	if (lock == 0) { /* use completion */
		wait_comp = wait_for_completion_timeout(&pin->comp_ack,
							TIMEOUT_COMPLETE);
		if ((wait_comp == 0) && (atomic_read(&lock_ack[mid]) == 0)) {
			/* wait mode timeout */
			ret = IPI_TIMEOUT_ACK;
		} else {
			if (retbuf)
				memcpy_from_sspm(retbuf, pin->prdata,
					(SSPM_MBOX_SLOT_SIZE * retslot));
		}
		atomic_set(&lock_ack[mid], 0);
	} else { /* use spin method */
		int retries = 2000000;

		while (retries-- > 0) {
			int mbno = pin->mbox;
			struct _mbox_info *mbox = &(mbox_table[mbno]);

			ret = sspm_mbox_polling(mbno, mid - mbox->start,
					pin->slot, retbuf, retslot, 2000);

			if (ret == 0)
				break;

			if (atomic_read(&lock_ack[mid])) {
				if (retbuf)
					memcpy_from_sspm(retbuf, pin->prdata,
					(SSPM_MBOX_SLOT_SIZE * retslot));

				ret = 0;
				break;
			}
			udelay(1);
		}
		atomic_set(&lock_ack[mid], 0);

		if (retries == 0)
			ret = IPI_TIMEOUT_ACK;
	}

	/* Release mutex */
	if (lock == 0)	/* use mutex */
		mutex_unlock(&pin->mutex_send);
	else
		atomic_inc(&lock_send[mid]);

	if (pin->lock & IPI_LOCK_CHANGE)
		pin->lock &= IPI_LOCK_ORIGINAL;

	ipi_check_ack(mid, opts, ret);
	return ret;
}

int sspm_ipi_send_ack(int mid, unsigned int *data)
{
	int len = 1;

	if (data == NULL)
		len = 0;

	return sspm_ipi_send_ack_ex(mid, data, len);
}
EXPORT_SYMBOL(sspm_ipi_send_ack);

int sspm_ipi_send_ack_ex(int mid, void *data, int retslot)
{
	struct _pin_recv *pin;
	struct _mbox_info *mbox;
	int len, mbno, irq, slot, ret;

	if ((mid < 0) || (mid >= TOTAL_RECV_PIN))
		return IPI_SERVICE_NOT_AVAILABLE;

	pin = &(recv_pintable[mid]);
	if (retslot > pin->size)
		return IPI_NO_MEMORY;

	mbno = pin->mbox;
	mbox = &(mbox_table[mbno]);
	irq = mid - (mbox->start);
	/* return data length */
	if ((pin->retdata) && (data != NULL))
		len = retslot;
	else
		len = 0;
	/* where to put the return data */
	slot = pin->slot;

	ret = sspm_mbox_send(mbno, slot, irq, (void *)data, len);
	if (ret)
		return -1;

	return 0;
}

int sspm_ipi_send_sync(int mid, int opts, void *buffer, int slot,
						   void *retbuf, int retslot)
{
	unsigned long flags = 0;
	unsigned long wait_comp;
	int mbno, ret;
	struct _pin_send *pin;
	struct _mbox_info *mbox;

	if (sspm_ipi_inited == 0)
		return IPI_SERVICE_NOT_INITED;

	/* check if mid is in the predefined range */
	if ((mid < 0) || (mid >= TOTAL_SEND_PIN))
		return IPI_SERVICE_NOT_AVAILABLE;

	/* get the predefined pin info from mid */
	pin = &(send_pintable[mid]);
	if ((slot > pin->size) || (retslot > pin->size))
		return IPI_NO_MEMORY;

	/* check if IPI can be send in different mode */
	if (opts&IPI_OPT_POLLING) {  /* POLLING mode */

		spin_lock_irqsave(&lock_polling[mid], flags);

		if (mutex_is_locked(&pin->mutex_send)) {
			spin_unlock_irqrestore(&lock_polling[mid], flags);
			pr_err("Error: IPI pin=%d has been used in WAIT mode\n",
				mid);
			WARN_ON(1);
			return IPI_USED_IN_WAIT;
		}

	} else {                       /* WAIT mode */
		/* Check if users call in atomic/interrupt/IRQ disabled */
		if (preempt_count() || in_interrupt() || irqs_disabled()) {
			pr_err("IPI panic: pin id=%d, atomic=%d, interrupt=%ld, irq disabled=%d\n",
				mid, preempt_count(), in_interrupt(),
				irqs_disabled());
			WARN_ON(1);
		}

		mutex_lock(&pin->mutex_send);
	}

	ipi_check_send(mid);
	mbno = pin->mbox;
	mbox = &(mbox_table[mbno]);
	/* note: the bit of INT(OUT)_IRQ is depending on mid */
	if (slot == 0)
		slot = pin->size;

	atomic_set(&lock_ack[mid], 0);
	/* send IPI data to SSPM */
	ret = sspm_mbox_send(mbno, pin->slot, mid - mbox->start, buffer, slot);
	if (ret != 0) {
#ifdef IPI_MONITOR
		ipimon[mid].seqno--;
#endif
		/* release lock */
		if (opts&IPI_OPT_POLLING) /* POLLING mode */
			spin_unlock_irqrestore(&lock_polling[mid], flags);
		else
			mutex_unlock(&pin->mutex_send);

		return IPI_HW_ERROR;
	}

	/* if there is no retdata in predefined table */
	if ((pin->retdata == 0) || (retslot == 0))
		retbuf = NULL;

	/* wait ACK from SSPM */
	if (opts&IPI_OPT_POLLING) { /* POLLING mode */
		int retries = 2000000;

		while (retries-- > 0) {
			ret = sspm_mbox_polling(mbno, mid - mbox->start,
					pin->slot, retbuf, retslot, 2000);

			if (ret == 0)
				break;

			if (atomic_read(&lock_ack[mid])) {
				if (retbuf)
					memcpy_from_sspm(retbuf, pin->prdata,
					(SSPM_MBOX_SLOT_SIZE * retslot));

				ret = 0;
				break;
			}
			udelay(1);
		}
		atomic_set(&lock_ack[mid], 0);

		if (retries == 0) /* polling mode timeout */
			ret = IPI_TIMEOUT_ACK;

		ipi_check_ack(mid, opts, ret);

		spin_unlock_irqrestore(&lock_polling[mid], flags);

	} else {                    /* WAIT mode */
		wait_comp = wait_for_completion_timeout(&pin->comp_ack,
							TIMEOUT_COMPLETE);
		if ((wait_comp == 0) && (atomic_read(&lock_ack[mid]) == 0)) {
			/* wait mode timeout */
			ret = IPI_TIMEOUT_ACK;
		} else {
			if (retbuf)
				memcpy_from_sspm(retbuf, pin->prdata,
					(SSPM_MBOX_SLOT_SIZE * retslot));

		}
		atomic_set(&lock_ack[mid], 0);
		ipi_check_ack(mid, opts, ret);

		mutex_unlock(&pin->mutex_send);
	}

	return ret;
}
EXPORT_SYMBOL(sspm_ipi_send_sync);

static unsigned int ipi_isr_cb(unsigned int mbno, void __iomem *base,
	unsigned int irq)
{
	struct _mbox_info *mbox;

	if (mbno >= IPI_MBOX_TOTAL)
		return irq;

	mbox = &(mbox_table[mbno]);

	if (mbox->mode == 2) /* ipi_do_ack */
		ipi_do_ack(mbox, irq, base);
	else if (mbox->mode == 1) /* ipi_do_recv */
		ipi_do_recv(mbox, irq, base);

	return irq;
}
