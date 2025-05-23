/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2023 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef MC_ADMIN_H
#define MC_ADMIN_H

struct cdev;
struct mc_uuid_t;
struct tee_object;

int mc_admin_init(struct cdev *cdev, int (*tee_start_cb)(void),
		  void (*tee_stop_cb)(void));
void mc_admin_exit(void);
int mc_admin_wait_for_daemon(void);

struct tee_object *tee_object_select(const struct mc_uuid_t *uuid);
struct tee_object *tee_object_copy(uintptr_t address, size_t length);
struct tee_object *tee_object_read(uintptr_t address, size_t length);
void tee_object_free(struct tee_object *object);

#endif /* MC_ADMIN_H */
