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

#import <malloc.h>

#define stack_logging_type_free	0
#define stack_logging_type_generic	1	/* anything that is not allocation/deallocation */
#define stack_logging_type_alloc	2	/* malloc, realloc, etc... */
#define stack_logging_type_dealloc	4	/* free, realloc, etc... */

// Following flags are absorbed by stack_logging_log_stack()
#define	stack_logging_flag_zone		8	/* NSZoneMalloc, etc... */
#define	stack_logging_flag_calloc	16	/* multiply arguments to get the size */
#define stack_logging_flag_object 	32	/* NSAllocateObject(Class, extraBytes, zone) */
#define stack_logging_flag_cleared	64	/* for NewEmptyHandle */
#define stack_logging_flag_handle	128	/* for Handle (de-)allocation routines */
#define stack_logging_flag_set_handle_size	256	/* (Handle, newSize) treated specially */

/* Macro used to disguise addresses so that leak finding can work */
#define STACK_LOGGING_DISGUISE(address)	(((unsigned)address) ^ 0x00005555) /* nicely idempotent */

typedef struct {
    unsigned	type;
    unsigned	uniqued_stack;
    unsigned	argument;
    unsigned	address; /* disguised, to avoid confusing leaks */
} stack_logging_record_t;

typedef struct {
    unsigned	overall_num_bytes;
    unsigned	num_records;
    unsigned	lock; /* 0 means OK to lock; used for inter-process locking */
    unsigned	*uniquing_table; /* allocated using vm_allocate() */
            /* hashtable organized as (PC, uniqued parent)
            Only the second half of the table is active
            To enable us to grow dynamically */
    unsigned	uniquing_table_num_pages; /* number of pages of the table */
    unsigned	extra_retain_count; /* not used by stack_logging_log_stack */
    unsigned	filler[2]; /* align to cache lines for better performance */
    stack_logging_record_t	records[0]; /* records follow here */
} stack_logging_record_list_t;

extern unsigned stack_logging_get_unique_stack(unsigned **table, unsigned *table_num_pages, unsigned *stack_entries, unsigned count, unsigned num_hot_to_skip);
    /* stack_entries are from hot to cold */

extern stack_logging_record_list_t *stack_logging_the_record_list;
    /* This is the global variable containing all logs */

extern int stack_logging_enable_logging;
    /* when clear, no logging takes place */

extern int stack_logging_dontcompact;
    /* default is to compact; when set does not compact alloc/free logs; useful for tracing history */

extern void stack_logging_log_stack(unsigned type, unsigned arg1, unsigned arg2, unsigned arg3, unsigned result, unsigned num_hot_to_skip);

extern kern_return_t stack_logging_get_frames(task_t task, memory_reader_t reader, vm_address_t address, vm_address_t *stack_frames_buffer, unsigned max_stack_frames, unsigned *num_frames);
    /* Gets the last record in stack_logging_the_record_list about address */

#define STACK_LOGGING_ENUMERATION_PROVIDED	1	// temporary to avoid dependencies between projects

extern kern_return_t stack_logging_enumerate_records(task_t task, memory_reader_t reader, vm_address_t address, void enumerator(stack_logging_record_t, void *), void *context);
    /* Gets all the records about address;
    If !address, gets all records */

extern kern_return_t stack_logging_frames_for_uniqued_stack(task_t task, memory_reader_t reader, unsigned uniqued_stack, vm_address_t *stack_frames_buffer, unsigned max_stack_frames, unsigned *num_frames);
    /* Given a uniqued_stack fills stack_frames_buffer */

extern void thread_stack_pcs(vm_address_t *buffer, unsigned max, unsigned *num);
    /* Convenience to fill buffer with the PCs of the frames, starting with the hot frames;
    num: returned number of frames
    */

