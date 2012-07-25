/*
 * Copyright (c) 2006, 2008, 2009, 2011 Apple Inc. All rights reserved.
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

#ifndef _SCNETWORKCONNECTIONPRIVATE_H
#define _SCNETWORKCONNECTIONPRIVATE_H

#include <Availability.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCNetworkConfigurationPrivate.h>


typedef const struct __SCUserPreferencesRef * SCUserPreferencesRef;


__BEGIN_DECLS


#pragma mark -
#pragma mark SCNetworkConnection SPIs


CFArrayRef /* of SCNetworkServiceRef's */
SCNetworkConnectionCopyAvailableServices	(SCNetworkSetRef		set)			__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

SCNetworkConnectionRef
SCNetworkConnectionCreateWithService		(CFAllocatorRef			allocator,
						 SCNetworkServiceRef		service,
						 SCNetworkConnectionCallBack	callout,
						 SCNetworkConnectionContext	*context)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

SCNetworkServiceRef
SCNetworkConnectionGetService			(SCNetworkConnectionRef		connection)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

CFArrayRef /* of SCUserPreferencesRef's */
SCNetworkConnectionCopyAllUserPreferences	(SCNetworkConnectionRef		connection)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

SCUserPreferencesRef
SCNetworkConnectionCopyCurrentUserPreferences	(SCNetworkConnectionRef		connection)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

SCUserPreferencesRef
SCNetworkConnectionCreateUserPreferences	(SCNetworkConnectionRef		connection)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

Boolean
SCNetworkConnectionSuspend			(SCNetworkConnectionRef		connection)		__OSX_AVAILABLE_STARTING(__MAC_10_3,__IPHONE_2_0);

Boolean
SCNetworkConnectionResume			(SCNetworkConnectionRef		connection)		__OSX_AVAILABLE_STARTING(__MAC_10_3,__IPHONE_2_0);

Boolean
SCNetworkConnectionSetClientInfo		(SCNetworkConnectionRef		connection,
						 mach_port_t			client_audit_session,
						 uid_t				client_uid,
						 gid_t				client_gid,
						 pid_t				client_pid)		__OSX_AVAILABLE_STARTING(__MAC_10_8,__IPHONE_5_0);


#pragma mark -
#pragma mark SCNetworkConnection "VPN on Demand" SPIs


/* VPN On Demand
 *
 * in the SCDynamicStore we will have :
 *
 *   <key>State:/Network/Global/OnDemand</key>
 *   <dict>
 *     <key>Triggers</key>
 *     <array>
 *       <dict>
 *         <key>ServiceID</key>
 *         <string>A740678C-1983-492B-BF64-B825AAE7101E</string>
 *         <key>Status</key>
 *         <integer>8</integer>
 *         <key>RemoteAddress</key>
 *         <string>vpn.mycompany.com</string>
 *         <key>OnDemandMatchDomainsAlways</key>
 *         <array>
 *           <string>internal.mycompany.com</string>
 *         </array>
 *         <key>OnDemandMatchDomainsOnRetry</key>
 *         <array>
 *           <string>mycompany.com</string>
 *         </array>
 *         <key>kSCNetworkConnectionOnDemandMatchDomainsNever</key>
 *         <array>
 *           <string>external.mycompany.com</string>
 *         </array>
 *       </dict>
 *     </array>
 *   </dict>
 */

// notify(3) key
#define kSCNETWORKCONNECTION_ONDEMAND_NOTIFY_KEY		"com.apple.system.SCNetworkConnectionOnDemand"

// a CFArray[CFDictionary] of VPN on Demand "trigger" configurations
#define kSCNetworkConnectionOnDemandTriggers			CFSTR("Triggers")

// VPN service ID
#define kSCNetworkConnectionOnDemandServiceID			CFSTR("ServiceID")

// VPN service status (idle, connecting, connected, disconnecting)
#define kSCNetworkConnectionOnDemandStatus			CFSTR("Status")

// VPN server address
#define kSCNetworkConnectionOnDemandRemoteAddress		CFSTR("RemoteAddress")

// a CFArray[CFString] representing those domain (or host) names that, if
// matched to a target hostname, should result in our first establishing
// the VPN connection before any DNS queries are issued.
#define kSCNetworkConnectionOnDemandMatchDomainsAlways		CFSTR("OnDemandMatchDomainsAlways")

// a CFArray[CFString] representing those domain (or host) names that, if
// matched to a target hostname, should result in a DNS query regardless of
// whether the VPN connection has been established.  If the DNS query returns
// an [EAI_NONAME] error then we should establish the VPN connection and
// re-issue / retry the query.
#define kSCNetworkConnectionOnDemandMatchDomainsOnRetry		CFSTR("OnDemandMatchDomainsOnRetry")

// a CFArray[CFString] representing those domain (or host) names that should
// be excluded from those that would be used to establish tje VPN connection.
#define kSCNetworkConnectionOnDemandMatchDomainsNever		CFSTR("OnDemandMatchDomainsNever")


Boolean
__SCNetworkConnectionCopyOnDemandInfoWithName	(SCDynamicStoreRef		*storeP,
						 CFStringRef			nodeName,
						 Boolean			onDemandRetry,
						 CFStringRef			*connectionServiceID,
						 SCNetworkConnectionStatus	*connectionStatus,
						 CFStringRef			*vpnRemoteAddress)	__OSX_AVAILABLE_STARTING(__MAC_10_6,__IPHONE_2_0);


#pragma mark -
#pragma mark SCUserPreferences SPIs


Boolean
SCUserPreferencesRemove				(SCUserPreferencesRef		userPreferences)	__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

Boolean
SCUserPreferencesSetCurrent			(SCUserPreferencesRef		userPreferences)	__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

CFStringRef
SCUserPreferencesCopyName			(SCUserPreferencesRef		userPreferences)	__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

CFTypeID
SCUserPreferencesGetTypeID			(void)							__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

CFStringRef
SCUserPreferencesGetUniqueID			(SCUserPreferencesRef		userPreferences)	__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

Boolean
SCUserPreferencesIsForced			(SCUserPreferencesRef		userPreferences)	__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

Boolean
SCUserPreferencesSetName			(SCUserPreferencesRef		userPreferences,
						 CFStringRef			newName)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

Boolean
SCNetworkConnectionStartWithUserPreferences	(SCNetworkConnectionRef		connection,
						 SCUserPreferencesRef		userPreferences,
						 Boolean			linger)			__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

CFDictionaryRef
SCUserPreferencesCopyInterfaceConfiguration	(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

Boolean
SCUserPreferencesSetInterfaceConfiguration	(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 CFDictionaryRef		newOptions)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

CFDictionaryRef
SCUserPreferencesCopyExtendedInterfaceConfiguration
						(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 CFStringRef			extendedType)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

Boolean
SCUserPreferencesSetExtendedInterfaceConfiguration
						(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 CFStringRef			extendedType,
						 CFDictionaryRef		newOptions)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);


#pragma mark -
#pragma mark SCUserPreferences + SCNetworkInterface Password SPIs


Boolean
SCUserPreferencesCheckInterfacePassword		(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 SCNetworkInterfacePasswordType	passwordType)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

CFDataRef
SCUserPreferencesCopyInterfacePassword		(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 SCNetworkInterfacePasswordType	passwordType)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

Boolean
SCUserPreferencesRemoveInterfacePassword	(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 SCNetworkInterfacePasswordType	passwordType)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

Boolean
SCUserPreferencesSetInterfacePassword		(SCUserPreferencesRef		userPreferences,
						 SCNetworkInterfaceRef		interface,
						 SCNetworkInterfacePasswordType	passwordType,
						 CFDataRef			password,
						 CFDictionaryRef		options)		__OSX_AVAILABLE_STARTING(__MAC_10_5,__IPHONE_2_0);

__END_DECLS

#endif /* _SCNETWORKCONNECTIONPRIVATE_H */
