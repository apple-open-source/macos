/*
 * Copyright (c) 2006,2011,2014 Apple Inc. All Rights Reserved.
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

#include "sec_xdr.h"
#include <mach/message.h>
#include <Security/Authorization.h>

#ifdef __cplusplus
extern "C" {
#endif

bool_t xdr_AuthorizationItem(XDR *xdrs, AuthorizationItem *objp);
bool_t xdr_AuthorizationItemSet(XDR *xdrs, AuthorizationItemSet *objp);
bool_t xdr_AuthorizationItemSetPtr(XDR *xdrs, AuthorizationItemSet **objp);

bool_t copyin_AuthorizationItemSet(const AuthorizationItemSet *rights, void **copy, mach_msg_size_t *size);
bool_t copyout_AuthorizationItemSet(const void *copy, mach_msg_size_t size, AuthorizationItemSet **rights);

#ifdef __cplusplus
} // extern "C"
#endif
