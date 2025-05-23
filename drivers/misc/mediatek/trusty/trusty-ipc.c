// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#include <linux/aio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/idr.h>
#include <linux/completion.h>
#include <linux/dma-buf.h>
#include <linux/sched.h>
#include <linux/compat.h>
#include <linux/uio.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>

#include <linux/virtio.h>
#include <linux/virtio_ids.h>
#include <linux/virtio_config.h>

#include <linux/trusty/smcall.h>
#include <linux/trusty/trusty.h>
#include <linux/trusty/trusty_ipc.h>
#include <linux/trusty/trusty_shm.h>
#include <linux/bug.h>

#include <uapi/linux/trusty/ipc.h>

#define MAX_DEVICES			4

#define REPLY_TIMEOUT			5000
#define REPLY_RETRY_TIMEOUT		1000
#define TXBUF_TIMEOUT			15000
#define TIPC_TIMEOUT			100

#define MAX_SRV_NAME_LEN		256
#define MAX_DEV_NAME_LEN		32

#define DEFAULT_MSG_BUF_SIZE		PAGE_SIZE
#define DEFAULT_MSG_BUF_ALIGN		PAGE_SIZE

#define TIPC_CTRL_ADDR			53
#define TIPC_ANY_ADDR			0xFFFFFFFF

#define TIPC_MIN_LOCAL_ADDR		1024

#define TIPC_IOC_MAGIC			'r'
#define TIPC_IOC_CONNECT		_IOW(TIPC_IOC_MAGIC, 0x80, char *)
#if IS_ENABLED(CONFIG_COMPAT)
#define TIPC_IOC_CONNECT_COMPAT		_IOW(TIPC_IOC_MAGIC, 0x80, \
					     compat_uptr_t)
#endif

#define MAX_ELF_SZ			(100*1024)

#define format_log(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))

struct tipc_virtio_dev;

struct tipc_dev_config {
	u32 msg_buf_max_size;
	u32 msg_buf_alignment;
	char dev_name[MAX_DEV_NAME_LEN];
} __packed;

struct tipc_msg_hdr {
	u32 src;
	u32 dst;
	u32 reserved;
	u16 len;
	u16 flags;
	u8 data[0];
} __packed;

enum tipc_ctrl_msg_types {
	TIPC_CTRL_MSGTYPE_GO_ONLINE = 1,
	TIPC_CTRL_MSGTYPE_GO_OFFLINE,
	TIPC_CTRL_MSGTYPE_CONN_REQ,
	TIPC_CTRL_MSGTYPE_CONN_RSP,
	TIPC_CTRL_MSGTYPE_DISC_REQ,
};

struct tipc_ctrl_msg {
	u32 type;
	u32 body_len;
	u8  body[0];
} __packed;

struct tipc_conn_req_body {
	char name[MAX_SRV_NAME_LEN];
} __packed;

struct tipc_conn_rsp_body {
	u32 target;
	u32 status;
	u32 remote;
	u32 max_msg_size;
	u32 max_msg_cnt;
} __packed;

struct tipc_disc_req_body {
	u32 target;
} __packed;

struct tipc_cdev_node {
	struct cdev cdev;
	struct device *dev;
	unsigned int minor;
};

enum tipc_device_state {
	VDS_OFFLINE = 0,
	VDS_ONLINE,
	VDS_DEAD,
};

struct tipc_virtio_dev {
	struct kref refcount;
	struct mutex lock; /* protects access to this device */
	struct virtio_device *vdev;
	struct virtqueue *rxvq;
	struct virtqueue *txvq;
	uint msg_buf_cnt;
	uint msg_buf_max_cnt;
	size_t msg_buf_max_sz;
	uint free_msg_buf_cnt;
	struct list_head free_buf_list;
	wait_queue_head_t sendq;
	struct idr addr_idr;
	enum tipc_device_state state;
	struct tipc_cdev_node cdev_node;
	char   cdev_name[MAX_DEV_NAME_LEN];
};

enum tipc_chan_state {
	TIPC_DISCONNECTED = 0,
	TIPC_CONNECTING,
	TIPC_CONNECTED,
	TIPC_STALE,
};

struct tipc_chan {
	struct mutex lock; /* protects channel state  */
	struct kref refcount;
	enum tipc_chan_state state;
	struct tipc_virtio_dev *vds;
	const struct tipc_chan_ops *ops;
	void *ops_arg;
	u32 remote;
	u32 local;
	u32 max_msg_size;
	u32 max_msg_cnt;
	char srv_name[MAX_SRV_NAME_LEN];
};

struct apploader_load_app_req {
	uint64_t package_size;
} __packed;

static struct class *tipc_class;
static unsigned int tipc_major;

struct virtio_device *default_vdev;

static DEFINE_IDR(tipc_devices);
static DEFINE_MUTEX(tipc_devices_lock);

static bool ise_disable;

static phys_addr_t elf_paddr;
static size_t elf_size;
static void *shm_vaddr;

static int _match_any(int id, void *p, void *data)
{
	return id;
}

static int _match_data(int id, void *p, void *data)
{
	return (p == data);
}

static void *_alloc_shareable_mem(size_t sz, phys_addr_t *ppa, gfp_t gfp)
{
	return trusty_shm_alloc(sz, gfp);
}

static void _free_shareable_mem(size_t sz, void *va, phys_addr_t pa)
{
	trusty_shm_free(va, sz);
}

static struct tipc_msg_buf *_alloc_msg_buf(size_t sz)
{
	struct tipc_msg_buf *mb;

	/* allocate tracking structure */
	mb = kzalloc(sizeof(struct tipc_msg_buf), GFP_KERNEL);
	if (!mb)
		return NULL;

	/* allocate buffer that can be shared with secure world */
	mb->buf_va = _alloc_shareable_mem(sz, &mb->buf_pa, GFP_KERNEL);
	if (!mb->buf_va)
		goto err_alloc;

	mb->buf_sz = sz;

	return mb;

err_alloc:
	kfree(mb);
	return NULL;
}

static void _free_msg_buf(struct tipc_msg_buf *mb)
{
	_free_shareable_mem(mb->buf_sz, mb->buf_va, mb->buf_pa);
	kfree(mb);
}

static void _free_msg_buf_list(struct list_head *list)
{
	struct tipc_msg_buf *mb = NULL;

	mb = list_first_entry_or_null(list, struct tipc_msg_buf, node);
	while (mb) {
		list_del(&mb->node);
		_free_msg_buf(mb);
		mb = list_first_entry_or_null(list, struct tipc_msg_buf, node);
	}
}

static inline void mb_reset(struct tipc_msg_buf *mb)
{
	mb->wpos = 0;
	mb->rpos = 0;
}

static void _free_vds(struct kref *kref)
{
	struct tipc_virtio_dev *vds =
		container_of(kref, struct tipc_virtio_dev, refcount);
	kfree(vds);
}

static void _free_chan(struct kref *kref)
{
	struct tipc_chan *ch = container_of(kref, struct tipc_chan, refcount);

	if (ch->ops && ch->ops->handle_release)
		ch->ops->handle_release(ch->ops_arg);

	kref_put(&ch->vds->refcount, _free_vds);
	kfree(ch);
}

static struct tipc_msg_buf *vds_alloc_msg_buf(struct tipc_virtio_dev *vds)
{
	return _alloc_msg_buf(vds->msg_buf_max_sz);
}

static void vds_free_msg_buf(struct tipc_virtio_dev *vds,
			     struct tipc_msg_buf *mb)
{
	_free_msg_buf(mb);
}

static bool _put_txbuf_locked(struct tipc_virtio_dev *vds,
			      struct tipc_msg_buf *mb)
{
	list_add_tail(&mb->node, &vds->free_buf_list);
	return vds->free_msg_buf_cnt++ == 0;
}

static struct tipc_msg_buf *_get_txbuf_locked(struct tipc_virtio_dev *vds)
{
	struct tipc_msg_buf *mb;

	if (vds->state != VDS_ONLINE)
		return  ERR_PTR(-ENODEV);

	if (vds->free_msg_buf_cnt) {
		/* take it out of free list */
		mb = list_first_entry(&vds->free_buf_list,
				      struct tipc_msg_buf, node);
		list_del(&mb->node);
		vds->free_msg_buf_cnt--;
	} else {
		if (vds->msg_buf_cnt >= vds->msg_buf_max_cnt)
			return ERR_PTR(-EAGAIN);

		/* try to allocate it */
		mb = _alloc_msg_buf(vds->msg_buf_max_sz);
		if (!mb)
			return ERR_PTR(-ENOMEM);

		vds->msg_buf_cnt++;
	}
	return mb;
}

static struct tipc_msg_buf *_vds_get_txbuf(struct tipc_virtio_dev *vds)
{
	struct tipc_msg_buf *mb;

	mutex_lock(&vds->lock);
	mb = _get_txbuf_locked(vds);
	mutex_unlock(&vds->lock);

	return mb;
}

static void vds_put_txbuf(struct tipc_virtio_dev *vds, struct tipc_msg_buf *mb)
{
	mutex_lock(&vds->lock);
	_put_txbuf_locked(vds, mb);
	wake_up_interruptible(&vds->sendq);
	mutex_unlock(&vds->lock);
}

static struct tipc_msg_buf *vds_get_txbuf(struct tipc_virtio_dev *vds,
					  long timeout)
{
	struct tipc_msg_buf *mb;

	mb = _vds_get_txbuf(vds);

	if ((PTR_ERR(mb) == -EAGAIN) && timeout) {
		DEFINE_WAIT_FUNC(wait, woken_wake_function);

		timeout = msecs_to_jiffies(timeout);
		add_wait_queue(&vds->sendq, &wait);
		for (;;) {
			timeout = wait_woken(&wait, TASK_INTERRUPTIBLE,
					     timeout);
			if (!timeout) {
				mb = ERR_PTR(-ETIMEDOUT);
				break;
			}

			if (signal_pending(current)) {
				mb = ERR_PTR(-ERESTARTSYS);
				break;
			}

			mb = _vds_get_txbuf(vds);
			if (PTR_ERR(mb) != -EAGAIN)
				break;
		}
		remove_wait_queue(&vds->sendq, &wait);
	}

	if (IS_ERR(mb))
		return mb;

	BUG_ON(!mb);

	/* reset and reserve space for message header */
	mb_reset(mb);
	mb_put_data(mb, sizeof(struct tipc_msg_hdr));

	return mb;
}

static int vds_queue_txbuf(struct tipc_virtio_dev *vds,
			   struct tipc_msg_buf *mb)
{
	int err;
	struct scatterlist sg;
	bool need_notify = false;

	mutex_lock(&vds->lock);
	if (vds->state == VDS_ONLINE) {
		sg_init_one(&sg, mb->buf_va, mb->wpos);
		err = virtqueue_add_outbuf(vds->txvq, &sg, 1, mb, GFP_KERNEL);
		need_notify = virtqueue_kick_prepare(vds->txvq);
	} else {
		err = -ENODEV;
	}
	mutex_unlock(&vds->lock);

	if (need_notify)
		virtqueue_notify(vds->txvq);

	return err;
}

static int vds_add_channel(struct tipc_virtio_dev *vds,
			   struct tipc_chan *chan)
{
	int ret;

	mutex_lock(&vds->lock);
	if (vds->state == VDS_ONLINE) {
		ret = idr_alloc(&vds->addr_idr, chan,
				TIPC_MIN_LOCAL_ADDR, TIPC_ANY_ADDR - 1,
				GFP_KERNEL);
		if (ret > 0) {
			chan->local = ret;
			kref_get(&chan->refcount);
			ret = 0;
		}
	} else {
		ret = -EINVAL;
	}
	mutex_unlock(&vds->lock);

	return ret;
}

static void vds_del_channel(struct tipc_virtio_dev *vds,
			    struct tipc_chan *chan)
{
	mutex_lock(&vds->lock);
	if (chan->local) {
		idr_remove(&vds->addr_idr, chan->local);
		chan->local = 0;
		chan->remote = 0;
		kref_put(&chan->refcount, _free_chan);
	}
	mutex_unlock(&vds->lock);
}

static struct tipc_chan *vds_lookup_channel(struct tipc_virtio_dev *vds,
					    u32 addr)
{
	int id;
	struct tipc_chan *chan = NULL;

	mutex_lock(&vds->lock);
	if (addr == TIPC_ANY_ADDR) {
		id = idr_for_each(&vds->addr_idr, _match_any, NULL);
		if (id > 0)
			chan = idr_find(&vds->addr_idr, id);
	} else {
		chan = idr_find(&vds->addr_idr, addr);
	}
	if (chan)
		kref_get(&chan->refcount);
	mutex_unlock(&vds->lock);

	return chan;
}

static struct tipc_chan *vds_create_channel(struct tipc_virtio_dev *vds,
					    const struct tipc_chan_ops *ops,
					    void *ops_arg)
{
	int ret;
	struct tipc_chan *chan = NULL;

	if (!vds)
		return ERR_PTR(-ENOENT);

	if (!ops)
		return ERR_PTR(-EINVAL);

	chan = kzalloc(sizeof(*chan), GFP_KERNEL);
	if (!chan)
		return ERR_PTR(-ENOMEM);

	kref_get(&vds->refcount);
	chan->vds = vds;
	chan->ops = ops;
	chan->ops_arg = ops_arg;
	mutex_init(&chan->lock);
	kref_init(&chan->refcount);
	chan->state = TIPC_DISCONNECTED;

	ret = vds_add_channel(vds, chan);
	if (ret) {
		kfree(chan);
		kref_put(&vds->refcount, _free_vds);
		return ERR_PTR(ret);
	}

	return chan;
}

static void fill_msg_hdr(struct tipc_msg_buf *mb, u32 src, u32 dst)
{
	struct tipc_msg_hdr *hdr = mb_get_data(mb, sizeof(*hdr));

	hdr->src = src;
	hdr->dst = dst;
	hdr->len = mb_avail_data(mb);
	hdr->flags = 0;
	hdr->reserved = 0;
}

/*****************************************************************************/

struct tipc_chan *ise_create_channel(struct device *dev,
				      const struct tipc_chan_ops *ops,
				      void *ops_arg)
{
	struct virtio_device *vd;
	struct tipc_chan *chan;
	struct tipc_virtio_dev *vds;

	mutex_lock(&tipc_devices_lock);
	if (dev) {
		vd = container_of(dev, struct virtio_device, dev);
	} else {
		vd = default_vdev;
		if (!vd) {
			mutex_unlock(&tipc_devices_lock);
			return ERR_PTR(-ENOENT);
		}
	}
	vds = vd->priv;
	kref_get(&vds->refcount);
	mutex_unlock(&tipc_devices_lock);

	chan = vds_create_channel(vds, ops, ops_arg);
	kref_put(&vds->refcount, _free_vds);
	return chan;
}
EXPORT_SYMBOL(ise_create_channel);

struct tipc_msg_buf *ise_chan_get_rxbuf(struct tipc_chan *chan)
{
	return vds_alloc_msg_buf(chan->vds);
}
EXPORT_SYMBOL(ise_chan_get_rxbuf);

void ise_chan_put_rxbuf(struct tipc_chan *chan, struct tipc_msg_buf *mb)
{
	vds_free_msg_buf(chan->vds, mb);
}
EXPORT_SYMBOL(ise_chan_put_rxbuf);

struct tipc_msg_buf *ise_chan_get_txbuf_timeout(struct tipc_chan *chan,
						 long timeout)
{
	return vds_get_txbuf(chan->vds, timeout);
}
EXPORT_SYMBOL(ise_chan_get_txbuf_timeout);

void ise_chan_put_txbuf(struct tipc_chan *chan, struct tipc_msg_buf *mb)
{
	vds_put_txbuf(chan->vds, mb);
}
EXPORT_SYMBOL(ise_chan_put_txbuf);

int ise_chan_queue_msg(struct tipc_chan *chan, struct tipc_msg_buf *mb)
{
	int err;

	mutex_lock(&chan->lock);
	switch (chan->state) {
	case TIPC_CONNECTED:
		fill_msg_hdr(mb, chan->local, chan->remote);
		err = vds_queue_txbuf(chan->vds, mb);
		ISE_FOOTPRINT("srv='%s', ree=%d, ise=%d (%d)\n",
			chan->srv_name, chan->local, chan->remote, err);
		if (err) {
			/* this should never happen */
			pr_err("%s: failed to queue tx buffer (%d)\n",
			       __func__, err);
		}
		break;
	case TIPC_DISCONNECTED:
	case TIPC_CONNECTING:
		err = -ENOTCONN;
		pr_err("%s: unexpected connection state %d\n",
			__func__, chan->state);
		break;
	case TIPC_STALE:
		err = -ESHUTDOWN;
		pr_err("%s: stale state\n", __func__);
		break;
	default:
		err = -EBADFD;
		pr_err("%s: unexpected channel state %d\n",
			__func__, chan->state);
	}
	mutex_unlock(&chan->lock);
	return err;
}
EXPORT_SYMBOL(ise_chan_queue_msg);


int ise_chan_connect(struct tipc_chan *chan, const char *name)
{
	int err;
	struct tipc_ctrl_msg *msg;
	struct tipc_conn_req_body *body;
	struct tipc_msg_buf *txbuf;

	txbuf = vds_get_txbuf(chan->vds, TXBUF_TIMEOUT);
	if (IS_ERR(txbuf))
		return PTR_ERR(txbuf);

	/* reserve space for connection request control message */
	msg = mb_put_data(txbuf, sizeof(*msg) + sizeof(*body));
	body = (struct tipc_conn_req_body *)msg->body;

	/* fill message */
	msg->type = TIPC_CTRL_MSGTYPE_CONN_REQ;
	msg->body_len  = sizeof(*body);

	strncpy(body->name, name, sizeof(body->name));
	body->name[sizeof(body->name)-1] = '\0';

	mutex_lock(&chan->lock);
	switch (chan->state) {
	case TIPC_DISCONNECTED:
		/* save service name we are connecting to */
		strcpy(chan->srv_name, body->name);

		fill_msg_hdr(txbuf, chan->local, TIPC_CTRL_ADDR);
		err = vds_queue_txbuf(chan->vds, txbuf);
		ISE_FOOTPRINT("to='%s', ree=%d, ise=%d (CONN_REQ)\n",
			body->name, chan->local, TIPC_CTRL_ADDR);
		if (err) {
			/* this should never happen */
			pr_err("%s: failed to queue tx buffer (%d)\n",
			       __func__, err);
		} else {
			chan->state = TIPC_CONNECTING;
			txbuf = NULL; /* prevents discarding buffer */
		}
		break;
	case TIPC_CONNECTED:
	case TIPC_CONNECTING:
		/* check if we are trying to connect to the same service */
		if (strcmp(chan->srv_name, body->name) == 0) {
			err = 0;
			ISE_FOOTPRINT("to='%s', srv='%s' (0)\n",
				body->name, chan->srv_name);
		} else {
			if (chan->state == TIPC_CONNECTING)
				err = -EALREADY; /* in progress */
			else
				err = -EISCONN;  /* already connected */

			ISE_FOOTPRINT("to='%s', srv='%s' (1) state %d\n",
				body->name, chan->srv_name, chan->state);
		}
		break;

	case TIPC_STALE:
		err = -ESHUTDOWN;
		pr_err("%s: stale state\n", __func__);
		break;
	default:
		err = -EBADFD;
		pr_err("%s: unexpected channel state %d\n",
			__func__, chan->state);
		break;
	}
	mutex_unlock(&chan->lock);

	if (txbuf)
		ise_chan_put_txbuf(chan, txbuf); /* discard it */

	return err;
}
EXPORT_SYMBOL(ise_chan_connect);

int ise_chan_shutdown(struct tipc_chan *chan)
{
	int err;
	struct tipc_ctrl_msg *msg;
	struct tipc_disc_req_body *body;
	struct tipc_msg_buf *txbuf = NULL;

	/* get tx buffer */
	txbuf = vds_get_txbuf(chan->vds, TXBUF_TIMEOUT);
	if (IS_ERR(txbuf))
		return PTR_ERR(txbuf);

	mutex_lock(&chan->lock);
	if (chan->state == TIPC_CONNECTED || chan->state == TIPC_CONNECTING) {
		/* reserve space for disconnect request control message */
		msg = mb_put_data(txbuf, sizeof(*msg) + sizeof(*body));
		body = (struct tipc_disc_req_body *)msg->body;

		msg->type = TIPC_CTRL_MSGTYPE_DISC_REQ;
		msg->body_len = sizeof(*body);
		body->target = chan->remote;

		ISE_FOOTPRINT("srv='%s', ree=%d, ise=%d (%d)(DISC_REQ)\n",
			chan->srv_name, chan->local, chan->remote, chan->state);

		fill_msg_hdr(txbuf, chan->local, TIPC_CTRL_ADDR);
		err = vds_queue_txbuf(chan->vds, txbuf);
		if (err) {
			/* this should never happen */
			pr_err("%s: failed to queue tx buffer (%d)\n",
				__func__, err);
		}
	} else {
		err = -ENOTCONN;
		pr_err("%s: unexpected channel state %d (%d)\n",
			__func__, chan->state, err);
	}
	chan->state = TIPC_STALE;
	mutex_unlock(&chan->lock);

	if (err) {
		/* release buffer */
		ise_chan_put_txbuf(chan, txbuf);
	}

	return err;
}
EXPORT_SYMBOL(ise_chan_shutdown);

void ise_chan_destroy(struct tipc_chan *chan)
{
	vds_del_channel(chan->vds, chan);
	kref_put(&chan->refcount, _free_chan);
}
EXPORT_SYMBOL(ise_chan_destroy);

/***************************************************************************/

struct tipc_dn_chan {
	int state;
	struct mutex lock; /* protects rx_msg_queue list and channel state */
	struct tipc_chan *chan;
	wait_queue_head_t readq;
	struct completion reply_comp;
	struct list_head rx_msg_queue;
	struct mutex timer_lock;
	struct timer_list tipc_timer;
	int event_status;
};

static int dn_wait_for_reply(struct tipc_dn_chan *dn, int timeout)
{
	int ret;
	int max_retry;

	if (timeout < 0)
		return -EINVAL;

	max_retry = timeout / REPLY_RETRY_TIMEOUT;
	while (max_retry > 0) {
		/* -ERESTARTSYS if interrupted, 0 if timedout, positive if completed */
		ret = wait_for_completion_interruptible_timeout(&dn->reply_comp,
					msecs_to_jiffies(REPLY_RETRY_TIMEOUT));
		if (ret != 0)
			break;
		ise_check_vqs_call();
		max_retry--;
		ISE_FOOTPRINT("max_retry=%d\n", max_retry);
	}

	if (ret < 0)
		return ret;

	mutex_lock(&dn->lock);
	if (!ret) {
		/* no reply from remote */
		dn->state = TIPC_STALE;
		pr_err("%s: Met time-out!! empty %d in iSE\n", __func__,
					list_empty(&dn->rx_msg_queue));
#if IS_ENABLED(CONFIG_TRUSTY_BUG_ON_WARN)
		BUG_ON(1);
#else
		WARN_ON(1);
#endif
		ret = -ETIMEDOUT;
	} else {
		/* got reply */
		if (dn->state == TIPC_CONNECTED)
			ret = 0;
		else if (dn->state == TIPC_DISCONNECTED)
			if (!list_empty(&dn->rx_msg_queue))
				ret = 0;
			else
				ret = -ENOTCONN;
		else
			ret = -EIO;
	}
	mutex_unlock(&dn->lock);

	return ret;
}

struct tipc_msg_buf *dn_handle_msg(void *data, struct tipc_msg_buf *rxbuf)
{
	struct tipc_dn_chan *dn = data;
	struct tipc_msg_buf *newbuf = rxbuf;

	mutex_lock(&dn->lock);
	if (dn->state == TIPC_CONNECTED) {
		/* get new buffer */
		newbuf = ise_chan_get_rxbuf(dn->chan);
		if (newbuf) {
			/* queue an old buffer and return a new one */
			list_add_tail(&rxbuf->node, &dn->rx_msg_queue);
			wake_up_interruptible(&dn->readq);
		} else {
			/*
			 * return an old buffer effectively discarding
			 * incoming message
			 */
			pr_err("%s: discard incoming message\n", __func__);
			newbuf = rxbuf;
		}
	}
	mutex_unlock(&dn->lock);

	return newbuf;
}

static void dn_connected(struct tipc_dn_chan *dn)
{
	mutex_lock(&dn->lock);
	dn->state = TIPC_CONNECTED;

	/* complete all pending  */
	complete(&dn->reply_comp);

	mutex_unlock(&dn->lock);
}

static void dn_disconnected(struct tipc_dn_chan *dn)
{
	mutex_lock(&dn->lock);
	dn->state = TIPC_DISCONNECTED;

	/* complete all pending  */
	complete(&dn->reply_comp);

	/* wakeup all readers */
	wake_up_interruptible_all(&dn->readq);

	mutex_unlock(&dn->lock);
}

static void dn_shutdown(struct tipc_dn_chan *dn)
{
	mutex_lock(&dn->lock);

	/* set state to STALE */
	dn->state = TIPC_STALE;

	/* complete all pending  */
	complete(&dn->reply_comp);

	/* wakeup all readers */
	wake_up_interruptible_all(&dn->readq);

	mutex_unlock(&dn->lock);
}

static void dn_handle_event(void *data, int event)
{
	struct tipc_dn_chan *dn = data;

	switch (event) {
	case TIPC_CHANNEL_SHUTDOWN:
		dn_shutdown(dn);
		break;

	case TIPC_CHANNEL_DISCONNECTED:
		dn_disconnected(dn);
		break;

	case TIPC_CHANNEL_CONNECTED:
		dn_connected(dn);
		break;

	default:
		pr_err("%s: unhandled event %d\n", __func__, event);
		break;
	}
}

static void dn_handle_release(void *data)
{
	kfree(data);
}

static struct tipc_chan_ops _dn_ops = {
	.handle_msg = dn_handle_msg,
	.handle_event = dn_handle_event,
	.handle_release = dn_handle_release,
};

#define cdev_to_cdn(c) container_of((c), struct tipc_cdev_node, cdev)
#define cdn_to_vds(cdn) container_of((cdn), struct tipc_virtio_dev, cdev_node)

static struct tipc_virtio_dev *_dn_lookup_vds(struct tipc_cdev_node *cdn)
{
	int ret;
	struct tipc_virtio_dev *vds = NULL;

	mutex_lock(&tipc_devices_lock);
	ret = idr_for_each(&tipc_devices, _match_data, cdn);
	if (ret) {
		vds = cdn_to_vds(cdn);
		kref_get(&vds->refcount);
	}
	mutex_unlock(&tipc_devices_lock);
	return vds;
}

static void tipc_timeout_callback(struct timer_list *timer)
{
	ise_notifier_call();
}

static int tipc_open(struct inode *inode, struct file *filp)
{
	int ret;
	struct tipc_virtio_dev *vds;
	struct tipc_dn_chan *dn;
	struct tipc_cdev_node *cdn = cdev_to_cdn(inode->i_cdev);

	if (ise_disable) {
		pr_info("%s: ise trusty ipc dummy function\n", __func__);
		return -EPERM;
	}

	/* kick ise rx for multiple client applications running */
	ise_notifier_call();

	vds = _dn_lookup_vds(cdn);
	if (!vds) {
		ret = -ENOENT;
		goto err_vds_lookup;
	}

	dn = kzalloc(sizeof(*dn), GFP_KERNEL);
	if (!dn) {
		ret = -ENOMEM;
		goto err_alloc_chan;
	}

	mutex_init(&dn->lock);
	init_waitqueue_head(&dn->readq);
	init_completion(&dn->reply_comp);
	INIT_LIST_HEAD(&dn->rx_msg_queue);

	dn->state = TIPC_DISCONNECTED;

	dn->chan = vds_create_channel(vds, &_dn_ops, dn);
	if (IS_ERR(dn->chan)) {
		ret = PTR_ERR(dn->chan);
		goto err_create_chan;
	}

	filp->private_data = dn;
	kref_put(&vds->refcount, _free_vds);

	mutex_init(&dn->timer_lock);

	mutex_lock(&dn->timer_lock);
	timer_setup(&dn->tipc_timer, tipc_timeout_callback, 0);
	mutex_unlock(&dn->timer_lock);

	return 0;

err_create_chan:
	kfree(dn);
err_alloc_chan:
	kref_put(&vds->refcount, _free_vds);
err_vds_lookup:
	return ret;
}


static int dn_connect_ioctl(struct tipc_dn_chan *dn, char __user *usr_name)
{
	int err;
	char name[MAX_SRV_NAME_LEN];

	/* copy in service name from user space */
	err = strncpy_from_user(name, usr_name, sizeof(name));
	if (err < 0) {
		pr_err("%s: copy_from_user (%p) failed (%d)\n",
		       __func__, usr_name, err);
		return err;
	}
	name[sizeof(name)-1] = '\0';

	/* send connect request */
	err = ise_chan_connect(dn->chan, name);
	if (err)
		return err;

	/* and wait for reply */
	return dn_wait_for_reply(dn, REPLY_TIMEOUT);
}

static int trusty_elf_init(struct device *dev)
{
	return ise_fast_call32(dev->parent, SMC_FC_ELF_PA_INFO,
			(u32)elf_paddr,
			(u32)(elf_paddr >> 32),
			(u32)elf_size);
}

static ssize_t txbuf_write_iter(struct tipc_msg_buf *txbuf,
				struct iov_iter *iter)
{
	size_t len;
	/* message length */
	len = iov_iter_count(iter);

	/* check available space */
	if (len > mb_avail_space(txbuf))
		return -EMSGSIZE;

	/* copy in message data */
	if (copy_from_iter(mb_put_data(txbuf, len), len, iter) != len)
		return -EFAULT;

	return len;
}

static long filp_send_ioctl(struct file *filp,
			const struct tipc_send_msg_req __user *arg)
{
	struct tipc_send_msg_req req;
	struct iovec fast_iovs[UIO_FASTIOV];
	struct iovec *iov = fast_iovs;
	struct iov_iter iter;
	struct trusty_shdm *shm = NULL;
	struct tipc_dn_chan *dn = filp->private_data;
	struct tipc_virtio_dev *vds = dn->chan->vds;
	struct device *dev = &vds->vdev->dev;
	struct tipc_msg_buf *txbuf = NULL;
	struct dma_buf *dmabuf = NULL;
	struct iosys_map map;
	phys_addr_t shm_paddr;
	// void *shm_vaddr = NULL;
	__u64 *va;
	ssize_t data_len = 0;
	long timeout = TXBUF_TIMEOUT;
	long ret = 0;

	if (copy_from_user(&req, arg, sizeof(req)))
		return -EFAULT;

	if (req.shm_cnt > U16_MAX || req.iov_cnt > U16_MAX)
		return -E2BIG;

	dev_dbg(dev, "%s: req.shm_cnt 0x%llx\n", __func__, req.shm_cnt);

	shm = kmalloc_array(req.shm_cnt, sizeof(*shm), GFP_KERNEL);
	if (!shm)
		return -ENOMEM;

	if (copy_from_user(shm, u64_to_user_ptr(req.shm),
			   req.shm_cnt * sizeof(struct trusty_shdm))) {
		ret = -EFAULT;
		goto load_shm_args_failed;
	}
	dev_dbg(dev, "%s: shm->size 0x%llx\n", __func__, shm->size);

	if (shm->size > MAX_ELF_SZ) {
		pr_err("%s: size (0x%llx) is over than MAX (0x%x)\n", __func__, shm->size, MAX_ELF_SZ);
		ret = -ENOMEM;
		goto load_shm_args_failed;
	}

	dmabuf = dma_buf_get(shm->fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(dev, "%s: dma_buf_get failed: %ld\n", __func__, PTR_ERR(dmabuf));
		ret = -EFAULT;
		goto dma_buf_get_failed;
	}

	memset(&map, 0, sizeof(struct iosys_map));
	if (dma_buf_vmap(dmabuf, &map)) {
		dev_err(dev, "%s: dma_buf_vmap failed\n", __func__);
		ret = -ENOMEM;
		goto dma_buf_vmap_failed;
	}
	va = (__u64 *)map.vaddr;
	dev_dbg(dev, "%s: *va 0x%llx\n", __func__, *va);

	// shm_vaddr = trusty_shm_alloc((size_t)shm->size, GFP_KERNEL | __GFP_ZERO);
	if (shm_vaddr == NULL) {
		shm_vaddr = trusty_shm_alloc(MAX_ELF_SZ, GFP_KERNEL | __GFP_ZERO);
		if (!shm_vaddr) {
			ret = -ENOMEM;
			goto trusty_shm_alloc_failed;
		}
	}
	memset(shm_vaddr, 0, MAX_ELF_SZ);
	memcpy(shm_vaddr, map.vaddr, (size_t)shm->size);
	shm_paddr = trusty_shm_virt_to_phys(shm_vaddr);
	dev_dbg(dev, "%s: shm_paddr 0x%llx\n", __func__, shm_paddr);
	elf_paddr = shm_paddr;
	elf_size = shm->size;

	ret = trusty_elf_init(dev);
	if (ret < 0)
		goto trusty_elf_init_failed;

	ret = import_iovec(WRITE, u64_to_user_ptr(req.iov), req.iov_cnt,
			   ARRAY_SIZE(fast_iovs), &iov, &iter);
	if (ret < 0) {
		dev_err(dev, "%s: Failed to import iovec\n", __func__);
		ret = -EINVAL;
		goto iov_import_failed;
	}

	if (filp->f_flags & O_NONBLOCK)
		timeout = 0;

	txbuf = ise_chan_get_txbuf_timeout(dn->chan, timeout);
	if (IS_ERR(txbuf)) {
		dev_err(dev, "%s: Failed to get txbuffer\n", __func__);
		ret = PTR_ERR(txbuf);
		goto get_txbuf_failed;
	}

	data_len = txbuf_write_iter(txbuf, &iter);
	if (data_len < 0) {
		ret = data_len;
		goto txbuf_write_failed;
	}

	ret = ise_chan_queue_msg(dn->chan, txbuf);
	if (ret)
		goto queue_failed;

	ret = data_len;
	dev_dbg(dev, "%s: ret 0x%lx\n", __func__, ret);

common_cleanup:
	kfree(iov);
iov_import_failed:
trusty_elf_init_failed:
	// trusty_shm_free(shm_vaddr, (size_t)shm->size);
trusty_shm_alloc_failed:
	dma_buf_vunmap(dmabuf, &map);
dma_buf_vmap_failed:
	if (!IS_ERR_OR_NULL(dmabuf))
		dma_buf_put(dmabuf);
dma_buf_get_failed:
load_shm_args_failed:
	kfree(shm);
	return ret;

queue_failed:
txbuf_write_failed:
	ise_chan_put_txbuf(dn->chan, txbuf);
get_txbuf_failed:
	goto common_cleanup;
}

static long tipc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct tipc_dn_chan *dn = filp->private_data;

	if (ise_disable) {
		pr_info("%s: ise trusty ipc dummy function\n", __func__);
		return -EPERM;
	}

	if (_IOC_TYPE(cmd) != TIPC_IOC_MAGIC)
		return -EINVAL;

	switch (cmd) {
	case TIPC_IOC_CONNECT:
		ret = dn_connect_ioctl(dn, (char __user *)arg);
		break;
	case TIPC_IOC_SEND_MSG:
		return filp_send_ioctl(filp, (const struct tipc_send_msg_req __user *)arg);
	default:
		pr_warn("%s: Unhandled ioctl cmd: 0x%x\n",
			__func__, cmd);
		ret = -EINVAL;
	}
	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long tipc_compat_ioctl(struct file *filp,
			      unsigned int cmd, unsigned long arg)
{
	int ret;
	struct tipc_dn_chan *dn = filp->private_data;
	void __user *user_req = compat_ptr(arg);

	if (_IOC_TYPE(cmd) != TIPC_IOC_MAGIC)
		return -EINVAL;

	switch (cmd) {
	case TIPC_IOC_CONNECT_COMPAT:
		ret = dn_connect_ioctl(dn, user_req);
		break;
	default:
		pr_warn("%s: Unhandled ioctl cmd: 0x%x\n",
			__func__, cmd);
		ret = -EINVAL;
	}
	return ret;
}
#endif

static inline bool _got_rx(struct tipc_dn_chan *dn)
{
	if (dn->state != TIPC_CONNECTED)
		return true;

	if (!list_empty(&dn->rx_msg_queue))
		return true;

	return false;
}

static ssize_t tipc_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	ssize_t ret;
	size_t len;
	struct tipc_msg_buf *mb;
	struct file *filp = iocb->ki_filp;
	struct tipc_dn_chan *dn = filp->private_data;

	if (ise_disable) {
		pr_info("%s: ise trusty ipc dummy function\n", __func__);
		return -EPERM;
	}

	mutex_lock(&dn->lock);

	while (list_empty(&dn->rx_msg_queue)) {
		if (dn->state != TIPC_CONNECTED) {
			if (dn->state == TIPC_CONNECTING)
				ret = -ENOTCONN;
			else if (dn->state == TIPC_DISCONNECTED)
				ret = -ENOTCONN;
			else if (dn->state == TIPC_STALE)
				ret = -ESHUTDOWN;
			else
				ret = -EBADFD;
			goto out;
		}

		mutex_unlock(&dn->lock);

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		mutex_lock(&dn->timer_lock);
		if (!dn->event_status)
			mod_timer(&dn->tipc_timer, jiffies + msecs_to_jiffies(TIPC_TIMEOUT));
		mutex_unlock(&dn->timer_lock);

		ISE_FOOTPRINT("wait for read!\n");
		dn->event_status = wait_event_interruptible(dn->readq, _got_rx(dn));
		if (dn->event_status)
			return -ERESTARTSYS;

		mutex_lock(&dn->lock);
	}

	mb = list_first_entry(&dn->rx_msg_queue, struct tipc_msg_buf, node);

	len = mb_avail_data(mb);
	if (len > iov_iter_count(iter)) {
		ret = -EMSGSIZE;
		goto out;
	}

	if (copy_to_iter(mb_get_data(mb, len), len, iter) != len) {
		ret = -EFAULT;
		goto out;
	}

	ret = len;
	list_del(&mb->node);
	ise_chan_put_rxbuf(dn->chan, mb);

out:
	mutex_unlock(&dn->lock);
	return ret;
}

static ssize_t tipc_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	ssize_t ret;
	size_t len;
	long timeout = TXBUF_TIMEOUT;
	struct tipc_msg_buf *txbuf = NULL;
	struct file *filp = iocb->ki_filp;
	struct tipc_dn_chan *dn = filp->private_data;

	if (ise_disable) {
		pr_info("%s: ise trusty ipc dummy function\n", __func__);
		return -EPERM;
	}

	if (filp->f_flags & O_NONBLOCK)
		timeout = 0;

	txbuf = ise_chan_get_txbuf_timeout(dn->chan, timeout);
	if (IS_ERR(txbuf))
		return PTR_ERR(txbuf);

	/* message length */
	len = iov_iter_count(iter);

	/* check available space */
	if (len > mb_avail_space(txbuf)) {
		ret = -EMSGSIZE;
		goto err_out;
	}

	/* copy in message data */
	if (copy_from_iter(mb_put_data(txbuf, len), len, iter) != len) {
		ret = -EFAULT;
		goto err_out;
	}

	/* queue message */
	ret = ise_chan_queue_msg(dn->chan, txbuf);
	if (ret)
		goto err_out;

	return len;

err_out:
	ise_chan_put_txbuf(dn->chan, txbuf);
	return ret;
}

static unsigned int tipc_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct tipc_dn_chan *dn = filp->private_data;

	if (ise_disable) {
		pr_info("%s: ise trusty ipc dummy function\n", __func__);
		return -EPERM;
	}

	mutex_lock(&dn->lock);

	poll_wait(filp, &dn->readq, wait);

	/* Writes always succeed for now */
	mask |= POLLOUT | POLLWRNORM;

	if (!list_empty(&dn->rx_msg_queue))
		mask |= POLLIN | POLLRDNORM;

	if (dn->state != TIPC_CONNECTED)
		mask |= POLLERR;

	mutex_unlock(&dn->lock);
	return mask;
}


static int tipc_release(struct inode *inode, struct file *filp)
{
	struct tipc_dn_chan *dn = filp->private_data;

	if (ise_disable) {
		pr_info("%s: ise trusty ipc dummy function\n", __func__);
		return -EPERM;
	}

	mutex_lock(&dn->timer_lock);
	del_timer(&dn->tipc_timer);
	mutex_unlock(&dn->timer_lock);

	dn_shutdown(dn);

	/* free all pending buffers */
	_free_msg_buf_list(&dn->rx_msg_queue);

	/* shutdown channel  */
	ise_chan_shutdown(dn->chan);

	/* and destroy it */
	ise_chan_destroy(dn->chan);

	return 0;
}

static const struct file_operations tipc_fops = {
	.open		= tipc_open,
	.release	= tipc_release,
	.unlocked_ioctl	= tipc_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl	= tipc_compat_ioctl,
#endif
	.read_iter	= tipc_read_iter,
	.write_iter	= tipc_write_iter,
	.poll		= tipc_poll,
	.owner		= THIS_MODULE,
};

/*****************************************************************************/

static void chan_trigger_event(struct tipc_chan *chan, int event)
{
	if (!event)
		return;

	chan->ops->handle_event(chan->ops_arg, event);
}

static void _cleanup_vq(struct virtqueue *vq)
{
	struct tipc_msg_buf *mb;

	while ((mb = virtqueue_detach_unused_buf(vq)) != NULL)
		_free_msg_buf(mb);
}

static int _create_cdev_node(struct device *parent,
			     struct tipc_cdev_node *cdn,
			     const char *name)
{
	int ret;
	dev_t devt;

	if (!name) {
		dev_dbg(parent, "%s: cdev name has to be provided\n",
			__func__);
		return -EINVAL;
	}

	/* allocate minor */
	ret = idr_alloc(&tipc_devices, cdn, 0, MAX_DEVICES-1, GFP_KERNEL);
	if (ret < 0) {
		dev_dbg(parent, "%s: failed (%d) to get id\n",
			__func__, ret);
		return ret;
	}

	cdn->minor = ret;
	cdev_init(&cdn->cdev, &tipc_fops);
	cdn->cdev.owner = THIS_MODULE;

	/* Add character device */
	devt = MKDEV(tipc_major, cdn->minor);
	ret = cdev_add(&cdn->cdev, devt, 1);
	if (ret) {
		dev_dbg(parent, "%s: cdev_add failed (%d)\n",
			__func__, ret);
		goto err_add_cdev;
	}

	/* Create a device node */
	cdn->dev = device_create(tipc_class, parent,
				 devt, NULL, "trusty-ipc-%s", name);
	if (IS_ERR(cdn->dev)) {
		ret = PTR_ERR(cdn->dev);
		dev_dbg(parent, "%s: device_create failed: %d\n",
			__func__, ret);
		goto err_device_create;
	}

	return 0;

err_device_create:
	cdn->dev = NULL;
	cdev_del(&cdn->cdev);
err_add_cdev:
	idr_remove(&tipc_devices, cdn->minor);
	return ret;
}

static void create_cdev_node(struct tipc_virtio_dev *vds,
			     struct tipc_cdev_node *cdn)
{
	int err;

	mutex_lock(&tipc_devices_lock);

	if (!default_vdev) {
		kref_get(&vds->refcount);
		default_vdev = vds->vdev;
	}

	if (vds->cdev_name[0] && !cdn->dev) {
		kref_get(&vds->refcount);
		err = _create_cdev_node(&vds->vdev->dev, cdn, vds->cdev_name);
		if (err) {
			dev_err(&vds->vdev->dev,
				"failed (%d) to create cdev node\n", err);
			kref_put(&vds->refcount, _free_vds);
		}
	}
	mutex_unlock(&tipc_devices_lock);
}

static void destroy_cdev_node(struct tipc_virtio_dev *vds,
			      struct tipc_cdev_node *cdn)
{
	mutex_lock(&tipc_devices_lock);
	if (cdn->dev) {
		device_destroy(tipc_class, MKDEV(tipc_major, cdn->minor));
		cdev_del(&cdn->cdev);
		idr_remove(&tipc_devices, cdn->minor);
		cdn->dev = NULL;
		kref_put(&vds->refcount, _free_vds);
	}

	if (default_vdev == vds->vdev) {
		default_vdev = NULL;
		kref_put(&vds->refcount, _free_vds);
	}

	mutex_unlock(&tipc_devices_lock);
}

static void _go_online(struct tipc_virtio_dev *vds)
{
	mutex_lock(&vds->lock);
	if (vds->state == VDS_OFFLINE)
		vds->state = VDS_ONLINE;
	mutex_unlock(&vds->lock);

	create_cdev_node(vds, &vds->cdev_node);

	dev_info(&vds->vdev->dev, "is online\n");
}

static void _go_offline(struct tipc_virtio_dev *vds)
{
	struct tipc_chan *chan;

	/* change state to OFFLINE */
	mutex_lock(&vds->lock);
	if (vds->state != VDS_ONLINE) {
		mutex_unlock(&vds->lock);
		return;
	}
	vds->state = VDS_OFFLINE;
	mutex_unlock(&vds->lock);

	/* wakeup all waiters */
	wake_up_interruptible_all(&vds->sendq);

	/* shutdown all channels */
	while ((chan = vds_lookup_channel(vds, TIPC_ANY_ADDR))) {
		mutex_lock(&chan->lock);
		chan->state = TIPC_STALE;
		chan->remote = 0;
		chan_trigger_event(chan, TIPC_CHANNEL_SHUTDOWN);
		mutex_unlock(&chan->lock);
		kref_put(&chan->refcount, _free_chan);
	}

	/* shutdown device node */
	destroy_cdev_node(vds, &vds->cdev_node);

	dev_info(&vds->vdev->dev, "is offline\n");
}

static void _handle_conn_rsp(struct tipc_virtio_dev *vds,
			     struct tipc_conn_rsp_body *rsp, size_t len)
{
	struct tipc_chan *chan;

	if (sizeof(*rsp) != len) {
		dev_err(&vds->vdev->dev, "%s: Invalid response length %zd\n",
			__func__, len);
		return;
	}

	dev_dbg(&vds->vdev->dev,
		"%s: connection response: for addr 0x%x: "
		"status %d remote addr 0x%x\n",
		__func__, rsp->target, rsp->status, rsp->remote);

	/* Lookup channel */
	chan = vds_lookup_channel(vds, rsp->target);
	if (chan) {
		mutex_lock(&chan->lock);
		if (chan->state == TIPC_CONNECTING) {
			if (!rsp->status) {
				ISE_FOOTPRINT("srv='%s', ree=%d, ise=%d (%d)(CONN_RSP)\n",
					chan->srv_name, chan->local, rsp->remote, chan->state);
				chan->state = TIPC_CONNECTED;
				chan->remote = rsp->remote;
				chan->max_msg_cnt = rsp->max_msg_cnt;
				chan->max_msg_size = rsp->max_msg_size;
				chan_trigger_event(chan,
						   TIPC_CHANNEL_CONNECTED);
			} else {
				ISE_FOOTPRINT("srv='%s', ree=%d, ise=%d (%d)(CONN_RSP)\n",
					chan->srv_name, chan->local, 0, chan->state);
				chan->state = TIPC_DISCONNECTED;
				chan->remote = 0;
				chan_trigger_event(chan,
						   TIPC_CHANNEL_DISCONNECTED);
			}
		} else {
			ISE_FOOTPRINT("state %d\n", chan->state);
		}

		mutex_unlock(&chan->lock);
		kref_put(&chan->refcount, _free_chan);
	}
}

static void _handle_disc_req(struct tipc_virtio_dev *vds,
			     struct tipc_disc_req_body *req, size_t len)
{
	struct tipc_chan *chan;

	if (sizeof(*req) != len) {
		dev_err(&vds->vdev->dev, "%s: Invalid request length %zd\n",
			__func__, len);
		return;
	}

	dev_dbg(&vds->vdev->dev, "%s: disconnect request: for addr 0x%x\n",
		__func__, req->target);

	chan = vds_lookup_channel(vds, req->target);
	if (chan) {
		mutex_lock(&chan->lock);
		pr_info("%s:%d srv='%s', ree=%d, ise=%d (%d)\n", __func__, __LINE__,
			chan->srv_name, chan->local, chan->remote, chan->state);
		if (chan->state == TIPC_CONNECTED ||
			chan->state == TIPC_CONNECTING) {
			chan->state = TIPC_DISCONNECTED;
			chan->remote = 0;
			chan_trigger_event(chan, TIPC_CHANNEL_DISCONNECTED);
		}
		mutex_unlock(&chan->lock);
		kref_put(&chan->refcount, _free_chan);
	} else {
		ISE_FOOTPRINT("no chan\n");
	}
}

static void _handle_ctrl_msg(struct tipc_virtio_dev *vds,
			     void *data, int len, u32 src)
{
	struct tipc_ctrl_msg *msg = data;

	if ((len < sizeof(*msg)) || (sizeof(*msg) + msg->body_len != len)) {
		dev_err(&vds->vdev->dev,
			"%s: Invalid message length ( %d vs. %d)\n",
			__func__, (int)(sizeof(*msg) + msg->body_len), len);
		return;
	}

	dev_dbg(&vds->vdev->dev,
		"%s: Incoming ctrl message: src 0x%x type %d len %d\n",
		__func__, src, msg->type, msg->body_len);

	switch (msg->type) {
	case TIPC_CTRL_MSGTYPE_GO_ONLINE:
		ISE_FOOTPRINT("GO_ONLINE\n");
		_go_online(vds);
	break;

	case TIPC_CTRL_MSGTYPE_GO_OFFLINE:
		ISE_FOOTPRINT("GO_OFFLINE\n");
		_go_offline(vds);
	break;

	case TIPC_CTRL_MSGTYPE_CONN_RSP:
		_handle_conn_rsp(vds, (struct tipc_conn_rsp_body *)msg->body,
				 msg->body_len);
	break;

	case TIPC_CTRL_MSGTYPE_DISC_REQ:
		_handle_disc_req(vds, (struct tipc_disc_req_body *)msg->body,
				 msg->body_len);
	break;

	default:
		dev_warn(&vds->vdev->dev,
			 "%s: Unexpected message type: %d\n",
			 __func__, msg->type);
	}
}

static int _handle_rxbuf(struct tipc_virtio_dev *vds,
			 struct tipc_msg_buf *rxbuf, size_t rxlen)
{
	int err;
	struct scatterlist sg;
	struct tipc_msg_hdr *msg;
	struct device *dev = &vds->vdev->dev;

	/* message sanity check */
	if (rxlen > rxbuf->buf_sz) {
		dev_warn(dev, "inbound msg is too big: %zd\n", rxlen);
		goto drop_it;
	}

	if (rxlen < sizeof(*msg)) {
		dev_warn(dev, "inbound msg is too short: %zd\n", rxlen);
		goto drop_it;
	}

	/* reset buffer and put data  */
	mb_reset(rxbuf);
	mb_put_data(rxbuf, rxlen);

	/* get message header */
	msg = mb_get_data(rxbuf, sizeof(*msg));
	if (mb_avail_data(rxbuf) != msg->len) {
		dev_warn(dev, "inbound msg length mismatch: (%d vs. %d)\n",
			 (uint) mb_avail_data(rxbuf), (uint)msg->len);
		goto drop_it;
	}

	dev_dbg(dev, "From: %d, To: %d, Len: %d, Flags: 0x%x, Reserved: %d\n",
		msg->src, msg->dst, msg->len, msg->flags, msg->reserved);

	/* message directed to control endpoint is a special case */
	if (msg->dst == TIPC_CTRL_ADDR) {
		_handle_ctrl_msg(vds, msg->data, msg->len, msg->src);
	} else {
		struct tipc_chan *chan = NULL;
		/* Lookup channel */
		chan = vds_lookup_channel(vds, msg->dst);
		if (chan) {
			ISE_FOOTPRINT("srv='%s', ree=%d, ise=%d (%d)\n",
				chan->srv_name, chan->local, chan->remote, chan->state);
			/* handle it */
			rxbuf = chan->ops->handle_msg(chan->ops_arg, rxbuf);
			BUG_ON(!rxbuf);
			kref_put(&chan->refcount, _free_chan);
		}
	}

drop_it:
	/* add the buffer back to the virtqueue */
	sg_init_one(&sg, rxbuf->buf_va, rxbuf->buf_sz);
	err = virtqueue_add_inbuf(vds->rxvq, &sg, 1, rxbuf, GFP_KERNEL);
	if (err < 0) {
		dev_err(dev, "failed to add a virtqueue buffer: %d\n", err);
		return err;
	}

	return 0;
}

static void _rxvq_cb(struct virtqueue *rxvq)
{
	unsigned int len;
	struct tipc_msg_buf *mb;
	unsigned int msg_cnt = 0;
	struct tipc_virtio_dev *vds = rxvq->vdev->priv;

	while ((mb = virtqueue_get_buf(rxvq, &len)) != NULL) {
		if (_handle_rxbuf(vds, mb, len))
			break;
		msg_cnt++;
	}

	/* tell the other size that we added rx buffers */
	if (msg_cnt)
		virtqueue_kick(rxvq);
}

static void _txvq_cb(struct virtqueue *txvq)
{
	unsigned int len;
	struct tipc_msg_buf *mb;
	bool need_wakeup = false;
	struct tipc_virtio_dev *vds = txvq->vdev->priv;

	dev_dbg(&txvq->vdev->dev, "%s\n", __func__);

	/* detach all buffers */
	mutex_lock(&vds->lock);
	while ((mb = virtqueue_get_buf(txvq, &len)) != NULL)
		need_wakeup |= _put_txbuf_locked(vds, mb);
	mutex_unlock(&vds->lock);

	if (need_wakeup) {
		/* wake up potential senders waiting for a tx buffer */
		wake_up_interruptible_all(&vds->sendq);
	}
}

static int tipc_virtio_probe(struct virtio_device *vdev)
{
	int err, i;
	struct tipc_virtio_dev *vds;
	struct tipc_dev_config config;
	struct virtqueue *vqs[2];
	vq_callback_t *vq_cbs[] = {_rxvq_cb, _txvq_cb};
	const char *vq_names[] = { "rx", "tx" };

	dev_dbg(&vdev->dev, "%s:\n", __func__);

	if (!is_trusty_real_driver()) {
		dev_info(&vdev->dev, "%s: ise trusty ipc dummy driver\n", __func__);
		return 0;
	}

	vds = kzalloc(sizeof(*vds), GFP_KERNEL);
	if (!vds)
		return -ENOMEM;

	vds->vdev = vdev;

	mutex_init(&vds->lock);
	kref_init(&vds->refcount);
	init_waitqueue_head(&vds->sendq);
	INIT_LIST_HEAD(&vds->free_buf_list);
	idr_init(&vds->addr_idr);

	/* set default max message size and alignment */
	memset(&config, 0, sizeof(config));
	config.msg_buf_max_size  = DEFAULT_MSG_BUF_SIZE;
	config.msg_buf_alignment = DEFAULT_MSG_BUF_ALIGN;

	/* get configuration if present */
	vdev->config->get(vdev, 0, &config, sizeof(config));

	/* copy dev name */
	strncpy(vds->cdev_name, config.dev_name, sizeof(vds->cdev_name));
	vds->cdev_name[sizeof(vds->cdev_name)-1] = '\0';

	/* find tx virtqueues (rx and tx and in this order) */
	err = vdev->config->find_vqs(vdev, 2, vqs, vq_cbs, vq_names, NULL,
				NULL);
	if (err)
		goto err_find_vqs;

	vds->rxvq = vqs[0];
	vds->txvq = vqs[1];

	/* save max buffer size and count */
	vds->msg_buf_max_sz = config.msg_buf_max_size;
	vds->msg_buf_max_cnt = virtqueue_get_vring_size(vds->txvq);

	/* set up the receive buffers */
	for (i = 0; i < virtqueue_get_vring_size(vds->rxvq); i++) {
		struct scatterlist sg;
		struct tipc_msg_buf *rxbuf;

		rxbuf = _alloc_msg_buf(vds->msg_buf_max_sz);
		if (!rxbuf) {
			dev_err(&vdev->dev, "failed to allocate rx buffer\n");
			err = -ENOMEM;
			goto err_free_rx_buffers;
		}

		sg_init_one(&sg, rxbuf->buf_va, rxbuf->buf_sz);
		err = virtqueue_add_inbuf(vds->rxvq, &sg, 1, rxbuf, GFP_KERNEL);
		WARN_ON(err); /* sanity check; this can't really happen */
	}

	vdev->priv = vds;
	vds->state = VDS_OFFLINE;

	dev_dbg(&vdev->dev, "%s: done\n", __func__);
	return 0;

err_free_rx_buffers:
	_cleanup_vq(vds->rxvq);
err_find_vqs:
	kref_put(&vds->refcount, _free_vds);
	return err;
}

static void tipc_virtio_remove(struct virtio_device *vdev)
{
	struct tipc_virtio_dev *vds = vdev->priv;

	_go_offline(vds);

	mutex_lock(&vds->lock);
	vds->state = VDS_DEAD;
	vds->vdev = NULL;
	mutex_unlock(&vds->lock);

	vdev->config->reset(vdev);

	idr_destroy(&vds->addr_idr);

	_cleanup_vq(vds->rxvq);
	_cleanup_vq(vds->txvq);
	_free_msg_buf_list(&vds->free_buf_list);

	vdev->config->del_vqs(vds->vdev);

	kref_put(&vds->refcount, _free_vds);
}

static struct virtio_device_id tipc_virtio_id_table[] = {
	{ VIRTIO_ID_TRUSTY_IPC, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static unsigned int features[] = {
	0,
};

static struct virtio_driver virtio_tipc_driver = {
	.feature_table	= features,
	.feature_table_size = ARRAY_SIZE(features),
	.driver.name	= KBUILD_MODNAME,
	.driver.owner	= THIS_MODULE,
	.id_table	= tipc_virtio_id_table,
	.probe		= tipc_virtio_probe,
	.remove		= tipc_virtio_remove,
};

static ssize_t ise_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	char *parm_str, *cmd_str, *pinput;
	char input[32] = {0};
	long param;
	uint32_t len;
	int err;

	len = (count < (sizeof(input) - 1)) ? count : (sizeof(input) - 1);
	if (copy_from_user(input, buffer, len)) {
		pr_err("%s: copy from user failed\n", __func__);
		return -EFAULT;
	}

	input[len] = '\0';
	pinput = input;

	cmd_str = strsep(&pinput, " ");

	if (!cmd_str)
		return -EINVAL;

	parm_str = strsep(&pinput, " ");

	if (!parm_str)
		return -EINVAL;

	err = kstrtol(parm_str, 10, &param);

	if (err)
		return err;

	if (!strncmp(cmd_str, "disable", sizeof("disable"))) {
		if (param != 0)
			ise_disable = true;
		else
			ise_disable = false;

		pr_info("%s: exit, ise_disable %d\n", __func__, ise_disable);
		return count;
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t ise_read(struct file *file, char __user *buffer,
	size_t count, loff_t *ppos)
{
	char msg_buf[1024] = {0};
	char *p = msg_buf;
	int len;

	format_log(p, msg_buf, "%d", !ise_disable);
	len = p - msg_buf;
	pr_info("%s: ise_disable %d\n", __func__, ise_disable);

	return simple_read_from_buffer(buffer, count, ppos, msg_buf, len);
}

static const struct proc_ops ise_fops = {
	.proc_write = ise_write,
	.proc_read = ise_read,
};

static int __init tipc_init(void)
{
	int ret;
	dev_t dev;

	if (!is_trusty_real_driver()) {
		pr_info("%s: ise trusty ipc dummy driver\n", __func__);
		return 0;
	}

	ret = alloc_chrdev_region(&dev, 0, MAX_DEVICES, KBUILD_MODNAME);
	if (ret) {
		pr_err("%s: alloc_chrdev_region failed: %d\n", __func__, ret);
		return ret;
	}

	tipc_major = MAJOR(dev);
	tipc_class = class_create(KBUILD_MODNAME);
	if (IS_ERR(tipc_class)) {
		ret = PTR_ERR(tipc_class);
		pr_err("%s: class_create failed: %d\n", __func__, ret);
		goto err_class_create;
	}

	ret = register_virtio_driver(&virtio_tipc_driver);
	if (ret) {
		pr_err("failed to register virtio driver: %d\n", ret);
		goto err_register_virtio_drv;
	}

	proc_create("ise", 0664, NULL, &ise_fops);

	return 0;

err_register_virtio_drv:
	class_destroy(tipc_class);

err_class_create:
	unregister_chrdev_region(dev, MAX_DEVICES);
	return ret;
}

static void __exit tipc_exit(void)
{
	unregister_virtio_driver(&virtio_tipc_driver);
	class_destroy(tipc_class);
	unregister_chrdev_region(MKDEV(tipc_major, 0), MAX_DEVICES);
}

/* We need to init this early */
subsys_initcall(tipc_init);
module_exit(tipc_exit);

MODULE_DEVICE_TABLE(tipc, tipc_virtio_id_table);
MODULE_DESCRIPTION("Trusty IPC driver");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
