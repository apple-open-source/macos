/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <CoreFoundation/CoreFoundation.h>
#include <libc.h>

#include <IOKit/kext/KXKextManager.h>
#include "queue.h"

typedef struct _request {
    unsigned int   type;
    char *         kmodname;
    char *         kmodvers;
    queue_chain_t  link;
} request_t;


Boolean kextd_launch_kernel_request_thread(void);
void * kextd_kernel_request_loop(void * arg);

void kextd_handle_kernel_request(void * info);
void kextd_check_notification_queue(void * info);

void kextd_load_kext(char * kmod_name,
    KXKextManagerError * result /* out */);

#endif __REQUEST_H__
