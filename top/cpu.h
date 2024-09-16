/*
 * Copyright (c) 2008 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef CPU_H
#define CPU_H

#include "statistic.h"

struct statistic *top_cpu_create(WINDOW *parent, const char *name);
struct statistic *top_cpu_me_create(WINDOW *parent, const char *name);
struct statistic *top_cpu_others_create(WINDOW *parent, const char *name);
struct statistic *top_boosts_create(WINDOW *parent, const char *name);
struct statistic *top_instrs_create(WINDOW *parent, const char *name);
struct statistic *top_cycles_create(WINDOW *parent, const char *name);

#endif /*CPU_H*/
