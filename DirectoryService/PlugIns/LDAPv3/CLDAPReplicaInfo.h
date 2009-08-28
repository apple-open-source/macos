/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#ifndef _CLDAPREPLICAINFO_H
#define _CLDAPREPLICAINFO_H

#include <DirectoryServiceCore/DSSemaphore.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <ldap.h>

#include "CObject.h"

using namespace std;

class CLDAPReplicaInfo : public CObject<CLDAPReplicaInfo>
{
	public:
		struct addrinfo	*fAddrInfo;			// addrinfo struct for easy checking
		bool			fbSupportsWrites;	// host is writable with proper authentication
		char			*fIPAddress;		// ip address of the server
		int				fPort;				// port of the server
		int32_t			fSupportedSecurity;	// the supported security level
		int32_t			fReachable;			// means the IP address is in theory reachable according to reachability

	public:
					CLDAPReplicaInfo			( const char *inReplicaIP, int inPort, bool inSSL, bool inSupportsWrites );
					CLDAPReplicaInfo			( const char *inLDAPI );

		LDAP		*CreateLDAP					( void );
		bool		SupportsSASLMethod			( CFStringRef inSASLMethod );
		CFArrayRef	CopySASLMethods				( void );
		CFArrayRef	CopyNamingContexts			( void );
		char		*CopyServicePrincipal		( void );
	
		bool		ShouldAttemptCheck			( void );
		void		ConnectionFailed			( void );
		LDAP		*VerifiedServerConnection	( int inTimeoutSeconds, bool inForceCheck, void *inCallback, void *inParam );
	
		static void ReachabilityCallback		( SCNetworkReachabilityRef inTarget, SCNetworkConnectionFlags inFlags, void *inInfo );

	private:
		char						*fLDAPURI;
		int32_t						fFlags;
		int32_t						fVerified;
		int32_t						fUnchecked;
		int32_t						fIsSSL;
		SCNetworkReachabilityRef	fReachabilityRef;	// the reachability ref for this
		DSSemaphore					fMutex;
		CFMutableArrayRef			fSASLMethods;		// this is protected by mutex
		CFMutableArrayRef			fNamingContexts;	// this is protected by mutex
		char						*fServerFQDN;		// this is protected by mutex
		char						*fServicePrincipal;	// this is protected by mutex

	private:
		virtual		~CLDAPReplicaInfo			( void );
		LDAP		*CreateLDAPInternal			( void );
		void		ReachabilityNotification	( SCNetworkConnectionFlags inFlags );
};

#endif
