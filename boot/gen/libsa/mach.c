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
/* mach simulation */

#import "libsa.h"

port_t task_self_;

char *mach_error_string(int errnum)
{
    extern char *strerror(int errnum);
    
    return strerror(errnum);
}

kern_return_t vm_allocate(
    vm_task_t target_task,
    vm_address_t *address,
    vm_size_t size,
    boolean_t anywhere
)
{
    *address = (vm_address_t)malloc(size);
    if (*address == 0)
	return KERN_FAILURE;
    else {
	bzero(*address, size);
	return KERN_SUCCESS;
    }
}

kern_return_t vm_deallocate(
    vm_task_t target_task,
    vm_address_t address,
    vm_size_t size
)
{
    free((void *)address);
    return KERN_SUCCESS;
}

vm_size_t vm_page_size = 8192;

kern_return_t host_info(
    host_t host,
    int flavor,
    host_info_t host_info,
    unsigned int *host_info_count
)
{
    host_basic_info_t hi = (host_basic_info_t) host_info;
    
    switch(flavor) {
    case HOST_BASIC_INFO:
	hi->max_cpus = 1;
	hi->avail_cpus = 1;
//	hi->memory_size = 
//	    (kernBootStruct->convmem +  kernBootStruct->extmem) * 1024;
	hi->memory_size = 0;
	hi->cpu_type = CPU_TYPE_I386;
	hi->cpu_subtype = CPU_SUBTYPE_486;
	break;
    case HOST_PROCESSOR_SLOTS:
	break;    
    case HOST_SCHED_INFO:
	break;
    }
    return KERN_SUCCESS;
}

host_t host_self(void)
{
    return 0;
}

int getpagesize(void)
{
    return vm_page_size;
}
