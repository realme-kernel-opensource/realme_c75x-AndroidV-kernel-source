/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __aee_helper_h
#define __aee_helper_h

#include <asm/stacktrace.h>

extern void *aee_log_buf_addr_get(void);
extern unsigned long aee_get_stext(void);
extern unsigned long aee_get_etext(void);
extern unsigned long aee_get_init_begin(void);

extern struct list_head *aee_get_modules(void);
extern unsigned long aee_get_kti_addresses(void);
extern unsigned long aee_get_kns_addr(void);
extern unsigned long aee_get_kallsyms_mem_size(void);
extern unsigned long aee_get_kn_off(void);
extern unsigned long aee_get_km_off(void);
extern unsigned long aee_get_ktt_off(void);
extern unsigned long aee_get_kti_off(void);
extern unsigned long aee_get_ka_off(void);
extern void aee_reinit_die_lock(void);
#if IS_ENABLED(CONFIG_MODULES)
extern void init_ko_addr_list_late(void);
#endif
#ifdef MODULE
int mrdump_ka_init(void *vaddr);
extern void mrdump_mini_add_klog(void);
extern void mrdump_mini_add_kallsyms(void);
#endif

#endif
