/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
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
 * Modification History
 *
 * Nov 7, 2002                 Allan Nathanson <ajn@apple.com>
 * - use ServiceID *or* LinkID
 *
 * Feb 10, 2001			Allan Nathanson <ajn@apple.com>
 * - cleanup API
 *
 * Feb 2000			Christophe Allie <callie@apple.com>
 * - initial revision (as ppplib.h)
 */

#ifndef _PPP_H
#define _PPP_H

#include <sys/cdefs.h>
#include <ppp/ppp_msg.h>
#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

int		PPPInit			(int			*ref);

int		PPPDispose		(int			ref);

int		PPPGetLinkByInterface	(int			ref,
					 char			*if_name,
					 uint32_t		*link);

int		PPPConnect		(int			ref,
					 CFStringRef		serviceid,
					 uint32_t		link,
					 void			*data,
					 uint32_t		dataLen,
					 int			linger);

int		PPPDisconnect		(int			ref,
					 CFStringRef		serviceid,
					 uint32_t		link,
					 int			force);

int		PPPSuspend		(int			ref,
					 CFStringRef		serviceID,
					 uint32_t		link);

int		PPPResume		(int			ref,
					 CFStringRef		serviceID,
					 uint32_t		link);

int		PPPGetOption		(int			ref,
					 CFStringRef		serviceid,
					 uint32_t		link,
					 uint32_t		option,
					 void			**data,
					 uint32_t		*dataLen);

int		PPPSetOption		(int			ref,
					 CFStringRef		serviceid,
					 uint32_t		link,
					 uint32_t		option,
					 void			*data,
					 uint32_t		dataLen);

int		PPPGetConnectData	(int			ref,
					 CFStringRef		serviceID,
					 uint32_t		link,
					 void			**data,
					 uint32_t		*dataLen);

int		PPPStatus		(int			ref,
					 CFStringRef		serviceid,
					 uint32_t		link,
					 struct ppp_status	**stat);

int		PPPExtendedStatus	(int			ref,
					 CFStringRef		serviceid,
					 uint32_t		link,
					 void			**data,
					 uint32_t		*dataLen);

int		PPPEnableEvents		(int			ref,
					 CFStringRef		serviceid,
					 uint32_t		link,
					 u_char			enable);

int		PPPReadEvent		(int			ref,
					 uint32_t		*event);

CFDataRef		PPPSerialize	(CFPropertyListRef	obj,
					 void			**data,
					 uint32_t		*dataLen);

CFPropertyListRef	PPPUnserialize	(void			*data,
					 uint32_t		dataLen);

__END_DECLS

#endif	/* _PPP_H */
