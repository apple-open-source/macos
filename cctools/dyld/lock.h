/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import "stuff/bool.h"

extern volatile enum bool dyld_lock;
extern volatile mach_port_t thread_that_has_dyld_lock;

extern void set_lock(
    void);
extern enum bool set_lock_or_in_multiply_defined_handler(
    void);
extern void release_lock(
    void);

/*
 * These are defined in machdep_lock.s
 */
extern volatile unsigned long * volatile global_lock;
extern volatile unsigned long * volatile debug_thread_lock;
extern volatile unsigned long mem_prot_lock;
extern volatile unsigned long mem_prot_debug_lock;
extern volatile mach_port_t cached_thread;
extern volatile vm_address_t cached_stack;

extern enum bool try_to_get_lock(
    volatile unsigned long *lock);
extern enum bool lock_is_set(
    volatile unsigned long *lock);
extern void clear_lock(
    volatile unsigned long *lock);
