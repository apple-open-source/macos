/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
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
 * Modification History
 *
 * January 19, 2003		Allan Nathanson <ajn@apple.com>
 * - add advanced reachability APIs
 *
 * June 10, 2001		Allan Nathanson <ajn@apple.com>
 * - updated to use service-based "State:" information
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * January 30, 2001		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>

#ifndef kSCEntNetRefreshConfiguration
#define kSCEntNetRefreshConfiguration	CFSTR("RefreshConfiguration")
#endif kSCEntNetRefreshConfiguration

Boolean
SCNetworkCheckReachabilityByAddress(const struct sockaddr	*address,
				    const int			addrlen,
				    SCNetworkConnectionFlags	*flags)
{
	SCNetworkReachabilityRef		networkAddress;
	Boolean			ok;
	struct sockaddr_storage	ss;

	if (!address ||
	    (addrlen == 0) ||
	    (addrlen > (int)sizeof(struct sockaddr_storage))) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	bzero(&ss, sizeof(ss));
	bcopy(address, &ss, addrlen);
	ss.ss_len = addrlen;

	networkAddress = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&ss);
	ok = SCNetworkReachabilityGetFlags(networkAddress, flags);
	CFRelease(networkAddress);
	return ok;
}


Boolean
SCNetworkCheckReachabilityByName(const char			*nodename,
				 SCNetworkConnectionFlags	*flags)
{
	SCNetworkReachabilityRef	networkAddress;
	Boolean				ok;

	if (!nodename) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	networkAddress = SCNetworkReachabilityCreateWithName(NULL, nodename);
	ok = SCNetworkReachabilityGetFlags(networkAddress, flags);
	CFRelease(networkAddress);
	return ok;
}

Boolean
SCNetworkInterfaceRefreshConfiguration(CFStringRef ifName)
{
	CFStringRef		key;
	Boolean			ret     = FALSE;
	SCDynamicStoreRef	store;

	store = SCDynamicStoreCreate(NULL,
				     CFSTR("SCNetworkInterfaceRefreshConfiguration"),
				     NULL, NULL);
	if (store == NULL) {
		return FALSE;
	}

	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    ifName,
							    kSCEntNetRefreshConfiguration);
	ret = SCDynamicStoreNotifyValue(store, key);
	CFRelease(key);
	CFRelease(store);
	return (ret);
}
