/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __PORT_PROXY_H__
#define __PORT_PROXY_H__
#include <linux/skbuff.h>
#include "mt-plat/mtk_ccci_common.h"
#include "port_t.h"
#include "ccmni.h"

#ifndef MAX_HIF_NUM
#define MAX_HIF_NUM 5
#endif

#ifndef MAX_QUEUE_NUM
#define MAX_QUEUE_NUM 16
#endif

struct port_proxy {
	int port_number;
	unsigned int major;
	unsigned int minor_base;
	unsigned int critical_user_active;
	unsigned int traffic_dump_flag;
	/*do NOT use this manner, otherwise spinlock inside
	 * private_data will trigger alignment exception
	 */
	struct port_t *ports;
	struct port_t *sys_port;
	struct port_t *ctl_port;
	/* port list of each Rx channel, for Rx dispatching */
	struct list_head rx_ch_ports[CCCI_MAX_CH_NUM];
	/* port list of each queue for receiving queue status dispatching */
	struct list_head queue_ports[MAX_HIF_NUM][MAX_QUEUE_NUM];
	/*all ee port linked in a list*/
	struct list_head exp_ports;
	unsigned long long latest_rx_thread_time;
};
/****************************************************************************/
/* External API Region called by port proxy object */
/****************************************************************************/
extern int port_get_cfg(struct port_t **ports);
extern int port_ipc_write_check_id(struct port_t *port, struct sk_buff *skb);
extern void inject_md_status_event(int event_type, char reason[]);
#if IS_ENABLED(CONFIG_DEVICE_MODULES_SPMI_MTK_PMIF)
extern int (*register_spmi_md_force_assert)(unsigned int id, char *buf, unsigned int len);
#endif

#endif /* __PORT_PROXY_H__ */
