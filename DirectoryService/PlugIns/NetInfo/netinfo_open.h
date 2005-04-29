/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header netinfo_open
 * packaged management of opening netinfo hierarchy nodes with timeout values
 * and determination of domain name with caching of the value
 */

#ifndef __netinfo_open_h__
#define __netinfo_open_h__

/* flags for netinfo_clear */
#define NETINFO_CLEAR_PRESERVE_LOCAL 0x00000001

//netinfo private SPI specific
#define NETINFO_BINDING_KEY "com.apple.system.netinfo.local.binding_change"

#define BINDING_STATE_UNBOUND 0
#define BINDING_STATE_BOUND 1
#define BINDING_STATE_NETROOT 2

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PRIVATE!
 * notifiation center SPI
 */
extern uint32_t notify_get_state(int token, int *state);
extern uint32_t notify_register_plain(const char *name, int *out_token);

/*
 * netinfo connection management routines from the netinfo_open package
 */
ni_status	netinfo_open		(void *ni, char *name, void **newni, int timeout);
ni_status	netinfo_connect		(struct in_addr *addr, const char *tag, void **ni, int timeout);
ni_status	netinfo_local		(void **domain, int timeout);
char*		netinfo_domainname	(void *ni);
void		netinfo_clear		(int flags);

#ifdef __cplusplus
}
#endif

#endif
