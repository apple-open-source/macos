/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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

/* 
 * InterestNotification.h
 * - register for IOKit interest notification on a particular BSD name
 */

/* 
 * Modification History
 *
 * March 10, 2009	Dieter Siegmund (dieter@apple)
 * - created
 */

#ifndef _S_INTERESTNOTIFICATION_H
#define _S_INTERESTNOTIFICATION_H

struct InterestNotification;

typedef struct InterestNotification * InterestNotificationRef;

typedef void (*InterestNotificationCallbackRef)(InterestNotificationRef interest_p, const void * arg);


InterestNotificationRef
InterestNotificationCreate(const char * if_name, 
			   InterestNotificationCallbackRef callback,
			   const void * arg);
void
InterestNotificationRelease(InterestNotificationRef interest_p);

#endif /* _S_INTERESTNOTIFICATION_H */
