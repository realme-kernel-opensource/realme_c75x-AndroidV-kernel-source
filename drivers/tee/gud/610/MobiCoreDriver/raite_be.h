/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2023 TRUSTONIC LIMITED
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

#ifndef MC_RAITE_BE_H
#define MC_RAITE_BE_H

#include "protocol_common.h"

#ifdef CONFIG_RAITE_HYP
struct tee_protocol_ops *raite_be_check(void);
#else
static inline
struct tee_protocol_ops *raite_be_check(void)
{
	return NULL;
}
#endif

#endif /* MC_RAITE_BE_H */
