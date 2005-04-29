/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef _privatelib_h_
#define _privatelib_h_

__private_extern__ void _askNicelyThenShutdownSystem(void);

__private_extern__ void _askNicelyThenSleepSystem(void);

//__private_extern__ void _doNiceShutdown(void);

__private_extern__ CFArrayRef _copyBatteryInfo(void);

__private_extern__ CFUserNotificationRef _showUPSWarning(void);

//__private_extern__ CFUserNotificationRef _showLowBatteryWarning(void);

__private_extern__ IOReturn _setRootDomainProperty(
                                    CFStringRef     key,
                                    CFTypeRef       val);

#endif

