/*
 * Copyright (c) 2015 Apple Inc.  All rights reserved.
 */

#include "corefile.h"
#include <mach/task.h>

#ifndef _THREADS_H
#define _THREADS_H

extern size_t sizeof_LC_THREAD(void);
extern void dump_thread_state(native_mach_header_t *, struct thread_command *, mach_port_t);

#endif /* _THREADS_H */
