/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Netgroup lookup routines
 * Copyright (c) 1989 by NeXT, Inc.
 */

#ifndef _NETGR_H_
#define _NETGR_H_

struct netgrent {
	char	*ng_host;
	char	*ng_user;
	char	*ng_domain;
};

#include <sys/cdefs.h>

__BEGIN_DECLS
int innetgr __P((const char *,const char *,const char *,const char *));
void setnetgrent __P((const char *));
struct netgrent *getnetgrent __P((void));
void endnetgrent __P((void));
__END_DECLS

#endif /* !_NETGR_H_ */
