/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef SWITCH_QUEUE_H
#define SWITCH_QUEUE_H

struct task_entry_struct {
	unsigned long long work_type;
	unsigned long long x0;
	unsigned long long x1;
	unsigned long long x2;
#ifndef TEEI_FFA_SUPPORT
	unsigned long long x3;
#endif
	struct list_head c_link;
};
#ifdef TEEI_FFA_SUPPORT
int add_work_entry(unsigned long long work_type, unsigned long long x0,
		unsigned long long x1, unsigned long long x2);
#else
int add_work_entry(unsigned long long work_type, unsigned long long x0,
		unsigned long long x1, unsigned long long x2, unsigned long long x3);
#endif

int teei_notify_switch_fn(void);
int init_teei_switch_comp(void);
int teei_switch_fn(void *work);
#endif  /* end of SWITCH_QUEUE_H */
