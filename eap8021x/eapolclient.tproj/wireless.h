
/*
 * Copyright (c) 2000-2009 Apple Inc. All rights reserved.
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
 */

/* 
 * Modification History
 *
 * October 26, 2001	Dieter Siegmund (dieter@apple)
 * - created
 */

#ifndef _S_WIRELESS_H
#define _S_WIRELESS_H

#include <net/ethernet.h>
#include <mach/boolean.h>
#include <stdint.h>
#include <CoreFoundation/CFString.h>

enum {
	kKeyTypeDefault = 0,
	kKeyTypeMulticast = 1,		// used for multi/broad-cast rx
	kKeyTypeIndexedTx = 2,		// default (indexed) key for tx
	kKeyTypeIndexedRx = 3,		// default (indexed) key for unicast rx
};
typedef uint32_t 	wirelessKeyType;

typedef const struct _wireless *	wireless_t;
typedef void (wireless_scan_callback)(wireless_t wref, 
				      CFArrayRef bssid_list, void * arg);
typedef wireless_scan_callback * wireless_scan_callback_t;
/*
 * Function: wireless_bind
 * Purpose:
 *   Given the client's BSD interface name if_name, check whether this
 *   refers to a wireless device.  If so, return a wireless_t handle
 *   and return TRUE.  Otherwise return FALSE.
 *   You must call wireless_free() on the returned handle if the
 *   return value is TRUE.
 */
boolean_t
wireless_bind(const char * if_name, wireless_t * wref_p);

/*
 * Function: wireless_ap_mac
 * Purpose:
 *   Return the access point's mac address.  If the client is associated
 *   with an access point, return the access point's mac address in AP_mac
 *   and return TRUE, otherwise return FALSE.
 */
boolean_t
wireless_ap_mac(const wireless_t wref, struct ether_addr * AP_mac);

boolean_t
wireless_set_key(const wireless_t wref, wirelessKeyType type, 
		 int index, const uint8_t * key, int key_length);

boolean_t
wireless_set_wpa_pmk(const wireless_t wref, 
		     const struct ether_addr * bssid,
		     const uint8_t * key, int key_length);

CFStringRef
wireless_copy_ssid_string(wireless_t wref);

void
wireless_free(wireless_t handle);

void
wireless_scan_cancel(wireless_t wref);

boolean_t
wireless_scan(wireless_t wref, CFStringRef ssid, int num_scans,
	      wireless_scan_callback_t func, void * arg);

boolean_t
wireless_is_wpa_enterprise(wireless_t wref);

#endif /* _S_WIRELESS_H */
