/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * define path names
 *
 * $Id: pathnames.h,v 1.2 2000/11/08 01:49:04 callie Exp $
 */

#ifdef HAVE_PATHS_H
#include <paths.h>

#else
#ifndef _PATH_VARRUN
#define _PATH_VARRUN 	"/etc/ppp/"
#endif
#define _PATH_DEVNULL	"/dev/null"
#endif

#ifndef _ROOT_PATH
#define _ROOT_PATH
#endif

#define _PATH_UPAPFILE 	 _ROOT_PATH "/etc/ppp/pap-secrets"
#define _PATH_CHAPFILE 	 _ROOT_PATH "/etc/ppp/chap-secrets"
#define _PATH_SYSOPTIONS _ROOT_PATH "/etc/ppp/options"
#define _PATH_IPUP	 _ROOT_PATH "/etc/ppp/ip-up"
#define _PATH_IPDOWN	 _ROOT_PATH "/etc/ppp/ip-down"
#define _PATH_AUTHUP	 _ROOT_PATH "/etc/ppp/auth-up"
#define _PATH_AUTHDOWN	 _ROOT_PATH "/etc/ppp/auth-down"
#define _PATH_TTYOPT	 _ROOT_PATH "/etc/ppp/options."
#define _PATH_CONNERRS	 _ROOT_PATH "/etc/ppp/connect-errors"
#define _PATH_PEERFILES	 _ROOT_PATH "/etc/ppp/peers/"
#define _PATH_RESOLV	 _ROOT_PATH "/etc/ppp/resolv.conf"

#define _PATH_USEROPT	 ".ppprc"

#ifdef INET6
#define _PATH_IPV6UP     _ROOT_PATH "/etc/ppp/ipv6-up"
#define _PATH_IPV6DOWN   _ROOT_PATH "/etc/ppp/ipv6-down"
#endif

#ifdef IPX_CHANGE
#define _PATH_IPXUP	 _ROOT_PATH "/etc/ppp/ipx-up"
#define _PATH_IPXDOWN	 _ROOT_PATH "/etc/ppp/ipx-down"
#endif /* IPX_CHANGE */
