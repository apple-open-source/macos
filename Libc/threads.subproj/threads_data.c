/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/*
 * This file contains global data and the size of the global data can NOT
 * change or otherwise it would make the shared library incompatable.  It
 * is padded so that new data can take the place of storage occupied by part
 * of it.
 */
int msg_send_timeout = 100;	/* milliseconds */
int msg_receive_timeout = 10;	/* milliseconds */
int mutex_spin_limit = 0;
int cthread_stack_mask = 0;
extern void cthread_init();
unsigned int cproc_default_stack_size = 1000000;
int condition_spin_limit = 0;
int condition_yield_limit = 7;
unsigned int initial_stack_boundary = 0;
unsigned int cthread_stack_base = 0;	/* Base for stack allocation */
int	malloc_lock = 0;			/* 
					 * Needs to be shared between malloc.o
					 * and malloc_utils.o
					 */

/* global data padding, must NOT be static */
char _threads_data_padding[208] = { 0 };
