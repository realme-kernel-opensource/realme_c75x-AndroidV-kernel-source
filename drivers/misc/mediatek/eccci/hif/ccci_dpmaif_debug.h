/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __MODEM_DPMA_DEBUG_H__
#define __MODEM_DPMA_DEBUG_H__

#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/dmapool.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/skbuff.h>
#include <mt-plat/mtk_ccci_common.h>
#include <linux/ip.h>

#include "ccci_dpmaif_com.h"

#define ENABLE_DPMAIF_ISR_LOG

#define TYPE_RX_DONE_SKB_ID  0
#define TYPE_RX_PUSH_SKB_ID  1
#define TYPE_TX_SEND_SKB_ID  2
#define TYPE_TX_DONE_SKB_ID  3
#define TYPE_RXTX_ISR_ID     4
#define TYPE_BAT_ALC_SKB_ID  5
#define TYPE_BAT_ALC_FRG_ID  6
#define TYPE_BAT_TH_WAKE_ID  7
#define TYPE_SKB_ALC_FLG_ID  8
#define TYPE_FRG_ALC_FLG_ID  9
#define TYPE_RX_START_ID     10
#define TYPE_UL_DL_TPUT_ID   11
#define TYPE_MAX_SKB_CNT_ID  12
#define TYPE_DROP_SKB_ID     13
#define TYPE_Q_STOP_START_ID 14
#define TYPE_RX_FLUSH_ID     15
#define TYPE_TX_ERROR_ID     16
#define TYPE_TX_START_ID     17
#define TYPE_SUSPEND_ID      18


#define DEBUG_RX_DONE_SKB    (1 << TYPE_RX_DONE_SKB_ID)
#define DEBUG_RX_PUSH_SKB    (1 << TYPE_RX_PUSH_SKB_ID)
#define DEBUG_TX_SEND_SKB    (1 << TYPE_TX_SEND_SKB_ID)
#define DEBUG_TX_DONE_SKB    (1 << TYPE_TX_DONE_SKB_ID)
#define DEBUG_RXTX_ISR       (1 << TYPE_RXTX_ISR_ID)
#define DEBUG_BAT_ALC_SKB    (1 << TYPE_BAT_ALC_SKB_ID)
#define DEBUG_BAT_ALC_FRG    (1 << TYPE_BAT_ALC_FRG_ID)
#define DEBUG_BAT_TH_WAKE    (1 << TYPE_BAT_TH_WAKE_ID)
#define DEBUG_SKB_ALC_FLG    (1 << TYPE_SKB_ALC_FLG_ID)
#define DEBUG_FRG_ALC_FLG    (1 << TYPE_FRG_ALC_FLG_ID)
#define DEBUG_RX_START       (1 << TYPE_RX_START_ID)
#define DEBUG_UL_DL_TPUT     (1 << TYPE_UL_DL_TPUT_ID)
#define DEBUG_MAX_SKB_CNT    (1 << TYPE_MAX_SKB_CNT_ID)
#define DEBUG_DROP_SKB       (1 << TYPE_DROP_SKB_ID)
#define DEBUG_Q_STOP_START   (1 << TYPE_Q_STOP_START_ID)
#define DEBUG_RX_FLUSH       (1 << TYPE_RX_FLUSH_ID)
#define DEBUG_TX_ERROR       (1 << TYPE_TX_ERROR_ID)
#define DEBUG_TX_START       (1 << TYPE_TX_START_ID)
#define DEBUG_SUSPEND        (1 << TYPE_SUSPEND_ID)


#define DROP_SKB_FROM_RX_TASKLET_LRO     0
#define DROP_SKB_FROM_RX_TASKLET_NOLRO   1
#define DROP_SKB_FROM_RX_ENQUEUE         2
#define DROP_SKB_FROM_TX_SKB_LEN_IS_0    3

#define RX_PUSH_IP_VER_4      1
#define RX_PUSH_IP_VER_6      2
#define RX_PUSH_IP_VER_OTH    3

#define RX_PUSH_IP_PROC_TCP   1
#define RX_PUSH_IP_PROC_UDP   2
#define RX_PUSH_IP_PROC_OTH   3

#define NOIRQ_FLAG_SUSPEND    0
#define NOIRQ_FLAG_RESUME     1


struct debug_rx_done_skb_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u16 bid;
	u16 len;
	u8  cidx;

} __packed;

struct debug_rx_push_skb_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u16 ipid;
	u16 fcnt;
	u16 bid;
	u8  ver:4;
	u8  proc:4;
	s16 ret;

} __packed;

struct debug_tx_send_skb_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u16 wr;
	u16 ipid;
	u16 len;
	u16 count_l;
	u8  ccmni_idx;
	u16 queue_idx:3;
	u16 budget:13;
	u8  net_type:3;
	u8  reserv:5;
} __packed;

struct debug_tx_done_skb_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u16 rel;

} __packed;

struct debug_rxtx_isr_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u32 rxsr;
	u32 rxmr;
	u32 txsr;
	u32 txmr;
	u32 l1sr;

} __packed;

struct debug_bat_alc_skb_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u16 spc;
	u16 cnt;
	u16 crd;
	u16 cwr;
	u16 pre_hw_wr;
	u16 thrd;
	u16 hw_rd;
	u32 pre_time;

} __packed;

struct debug_bat_th_wake_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u8  need1;
	u8  need2;
	u16 req;
	u16 frg;
	u8  est;

} __packed;

struct debug_skb_alc_flg_hdr {
	u8  type:5;
	u8  flag:3;
	u32 time;

} __packed;

struct debug_rx_start_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u16 pcnt;
	u16 ridx;

} __packed;

struct debug_ul_dl_tput_hdr {
	u8  type:5;
	u8  resv:3;
	u32 time;
	u64 uput;
	u64 dput;

} __packed;

struct debug_max_skb_cnt_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u16 value;

} __packed;

struct debug_drop_skb_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u8  from;
	u16 bid;
	u16 ipid;
	u16 len;

} __packed;

struct debug_tx_q_stop_start_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u32 flag:1;
	u32 nidx:4;
	u32 netif:5;
	u32 state:22;
	u16 budget;
	s32 counter;
	s32 ret;

} __packed;

struct debug_rx_flush_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u8  ccmni;
	u8  rx_flush;

} __packed;

struct debug_tx_error_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u16 ipid;
	u16 len;
	u16 bget;
	s32 err;

} __packed;

struct debug_tx_start_hdr {
	u8  type:5;
	u8  qidx:3;
	u32 time;
	u16 drb_rel;
	u16 rel_cnt;

} __packed;

struct debug_suspend_hdr {
	u8  type:5;
	u8  flag:3;
	u32 time;
	u32 rxsr;
	u32 rxmr;
	u32 txsr;
	u32 txmr;
	u32 ipby;

} __packed;


extern unsigned int g_debug_flags;



void ccci_dpmaif_debug_init(void);

void ccci_dpmaif_debug_add(void *data, int len);

extern void ccci_set_dpmaif_debug_cb(void (*dpmaif_debug_cb)(void));


#ifdef ENABLE_DPMAIF_ISR_LOG
void ccci_dpmaif_show_irq_log(void);

int ccci_dpmaif_record_isr_cnt(unsigned long long ts, struct dpmaif_rx_queue *rxq,
		unsigned int L2TISAR0, unsigned int L2RISAR0);

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
extern int mrdump_mini_add_extra_file(unsigned long vaddr, unsigned long paddr,
	unsigned long size, const char *name);
#endif
#endif

#endif /* __MODEM_DPMA_DEBUG_H__ */
