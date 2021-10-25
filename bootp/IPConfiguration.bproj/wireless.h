/*
 * Copyright (c) 1999-2021 Apple Inc. All rights reserved.
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
 * wireless.h
 * - CF object to retrieve Wi-Fi information
 */
/* 
 * Modification History
 *
 * July 6, 2020 	Dieter Siegmund (dieter@apple.com)
 * - moved out of ipconfigd.c
 */
#ifndef _S_WIRELESS_H
#define _S_WIRELESS_H

#include <stdint.h>
#include <stdbool.h>
#include <CoreFoundation/CFString.h>
#include <net/ethernet.h>

typedef struct WiFiInfo *WiFiInfoRef;

typedef CF_ENUM(uint32_t, WiFiAuthType) {
	kWiFiAuthTypeNone 	= 0x0000,
	kWiFiAuthTypeUnknown	= 0xffff,
};

typedef CF_ENUM(uint8_t, WiFiInfoComparisonResult) {
	kWiFiInfoComparisonResultUnknown	= 0,
	kWiFiInfoComparisonResultSameNetwork	= 1,
	kWiFiInfoComparisonResultNetworkChanged	= 2,
	kWiFiInfoComparisonResultBSSIDChanged 	= 3,
};


const char *
WiFiAuthTypeGetString(WiFiAuthType auth_type);

const char *
WiFiInfoComparisonResultGetString(WiFiInfoComparisonResult result);

WiFiInfoRef
WiFiInfoCopy(CFStringRef ifname);

CFStringRef
WiFiInfoGetSSID(WiFiInfoRef w);

const struct ether_addr *
WiFiInfoGetBSSID(WiFiInfoRef w);

WiFiAuthType
WiFiInfoGetAuthType(WiFiInfoRef w);

CFStringRef
WiFiInfoGetNetworkID(WiFiInfoRef w);

WiFiInfoComparisonResult
WiFiInfoCompare(WiFiInfoRef info1, WiFiInfoRef info2);

#endif /* _S_WIRELESS_H */
