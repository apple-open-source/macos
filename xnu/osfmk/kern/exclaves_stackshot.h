/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */


#pragma once

#include <kern/kern_types.h>

#if CONFIG_EXCLAVES

extern void * exclaves_enter_start_label __asm__("EXCLAVES_ENTRY_START");
extern void * exclaves_enter_end_label   __asm__("EXCLAVES_ENTRY_END");

extern void * exclaves_upcall_start_label __asm__("EXCLAVES_UPCALL_START");
extern void * exclaves_upcall_end_label   __asm__("EXCLAVES_UPCALL_END");

extern void * exclaves_scheduler_request_start_label __asm__("EXCLAVES_SCHEDULER_REQUEST_START");
extern void * exclaves_scheduler_request_end_label   __asm__("EXCLAVES_SCHEDULER_REQUEST_END");

extern uintptr_t exclaves_enter_range_start;
extern uintptr_t exclaves_enter_range_end;
extern uintptr_t exclaves_upcall_range_start;
extern uintptr_t exclaves_upcall_range_end;

#endif /* CONFIG_EXCLAVES */
