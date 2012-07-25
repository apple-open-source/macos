/*
 * Copyright (c) 2000 - 2004 Apple Inc. All rights reserved.
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
 * DHCPServer.h
 */
/* 
 * Modification History
 *
 * November 10, 2000 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */
#ifndef _S_DHCPSERVER_H
#define _S_DHCPSERVER_H
#include <CoreFoundation/CoreFoundation.h>

/**
 ** DHCP and NetBoot properties 
 **/
extern const CFStringRef	kDHCPSPropIdentifier;
extern const CFStringRef	kDHCPSPropName;

/**
 ** DHCP lease dictionary properties 
 **/
extern const CFStringRef	kDHCPSPropDHCPLease;
extern const CFStringRef	kDHCPSPropDHCPIPAddress;
extern const CFStringRef	kDHCPSPropDHCPHWAddress;

/** 
 ** NetBoot client dictionary properties 
 **/

extern const CFStringRef	kDHCPSPropNetBootArch;
extern const CFStringRef	kDHCPSPropNetBootSysid;
extern const CFStringRef	kDHCPSPropNetBootLastBootTime;
extern const CFStringRef	kDHCPSPropNetBootIPAddress;

/*
 * Key: kDHCPSPropNetBootImageIndex, kDHCPSPropNetBootImageKind, 
 *      kDHCPSPropNetBootImageIsInstall
 * Purpose:
 *   Gives the image index, kind, and install attributes of the image that
 *   the client is bound to.
 * Notes:
 *   kDHCPSPropNetBootImageIndex        CFNumber (1..65535)
 *   kDHCPSPropNetBootImageKind         CFNumber
 *                                       0 = Classic/Mac OS 9,
 *                                       1 = Mac OS X, 2 = Mac OS X Server,
 *                                       3 = Diagnostics
 *   kDHCPSPropNetBootImageIsInstall    CFBoolean
 */
extern const CFStringRef	kDHCPSPropNetBootImageIndex;
extern const CFStringRef	kDHCPSPropNetBootImageKind;
extern const CFStringRef	kDHCPSPropNetBootImageIsInstall;

/*
 * Key: kDHCPSPropNetBootImageID (deprecated)
 * Purpose:
 *   Gives a string representation of the 32-bit boot image ID, consisting
 *   of the index, kind, and install attributes.  
 * Notes:
 *   This attribute is deprecated in favor of kDHCPSPropNetBootImageIndex, 
 *   kDHCPSPropNetBootImageKind, and kDHCPSPropNetBootImageIsInstall defined 
 *   above.
 */
extern const CFStringRef	kDHCPSPropNetBootImageID;

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

#endif /* _S_DHCPSERVER_H */
