/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * DHCPServer.h
 */
/* 
 * Modification History
 *
 * November 10, 2000 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */
#include <CoreFoundation/CoreFoundation.h>

/* DHCP and NetBoot properties */
extern const CFStringRef	kDHCPSPropIdentifier;
extern const CFStringRef	kDHCPSPropName;

/* DHCP lease dictionary properties */
extern const CFStringRef	kDHCPSPropDHCPLease;
extern const CFStringRef	kDHCPSPropDHCPIPAddress;
extern const CFStringRef	kDHCPSPropDHCPHWAddress;

/* NetBoot client dictionary properties */
extern const CFStringRef	kDHCPSPropNetBootArch;
extern const CFStringRef	kDHCPSPropNetBootSysid;
extern const CFStringRef	kDHCPSPropNetBootImageID;
extern const CFStringRef	kDHCPSPropNetBootLastBootTime;

/*
 * Function: DHCPSDHCPLeaseListCreate
 * Purpose:
 *   Returns an array of DHCP lease dictionaries, NULL if none
 *   available.
 * Note:
 *   Returned array must be released using CFRelease().
 */
extern CFArrayRef		DHCPSDHCPLeaseListCreate();

/*
 * Function: DHCPSNetBootClientListCreate
 * Purpose:
 *   Returns an array of NetBoot client dictionaries, NULL if none
 *   available.
 * Note:
 *   Returned array must be released using CFRelease().
 */
extern CFArrayRef		DHCPSNetBootClientListCreate();

