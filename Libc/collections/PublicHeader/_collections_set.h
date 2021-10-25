/*
* Copyright (c) 2020 Apple Inc. All rights reserved.
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

#ifndef __OS_COLLECTIONS_SET_H
#define __OS_COLLECTIONS_SET_H

#include <os/collections_set.h>

OS_ASSUME_NONNULL_BEGIN
__BEGIN_DECLS

#ifndef os_set_32_ptr_payload_handler_t
typedef bool (^os_set_32_ptr_payload_handler_t) (uint32_t *);
#endif

#ifndef os_set_64_ptr_payload_handler_t
typedef bool (^os_set_64_ptr_payload_handler_t) (uint64_t *);
#endif

#ifndef os_set_str_ptr_payload_handler_t
typedef bool (^os_set_str_ptr_payload_handler_t) (const char **);
#endif

__END_DECLS
OS_ASSUME_NONNULL_END

#define IN_SET(PREFIX, SUFFIX) PREFIX ## os_set_32_ptr ## SUFFIX
#define os_set_insert_val_t uint32_t *
#define os_set_find_val_t uint32_t
#include "_collections_set.in.h"
#undef IN_SET
#undef os_set_insert_val_t
#undef os_set_find_val_t

#define IN_SET(PREFIX, SUFFIX) PREFIX ## os_set_64_ptr ## SUFFIX
#define os_set_insert_val_t uint64_t *
#define os_set_find_val_t uint64_t
#include "_collections_set.in.h"
#undef IN_SET
#undef os_set_insert_val_t
#undef os_set_find_val_t

#define IN_SET(PREFIX, SUFFIX) PREFIX ## os_set_str_ptr ## SUFFIX
#define os_set_insert_val_t const char **
#define os_set_find_val_t const char *
#include "_collections_set.in.h"
#undef IN_SET
#undef os_set_insert_val_t
#undef os_set_find_val_t

#endif
