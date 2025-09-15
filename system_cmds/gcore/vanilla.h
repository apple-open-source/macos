/*
 * Copyright (c) 2021 Apple Inc.  All rights reserved.
 */

#include "vm.h"
#include "notes.h"

#ifndef _VANILLA_H
#define _VANILLA_H

struct proc_bsdinfo;

extern int coredump(task_t, int, const struct proc_bsdinfo *);

#endif /* _VANILLA_H */
