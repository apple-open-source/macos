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

#include "CLDAPReplicaInfo.h"
#include "CLDAPDefines.h"

#include <DirectoryServiceCore/CLog.h>
#include <libkern/OSAtomic.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <ldap_private.h>

#pragma mark Defines

#define kReplicaFlagSSL			(1<<0)
#define kLDAPServicePrincipalType	"ldap"

#pragma mark -
#pragma mark Callbacks

void CLDAPReplicaInfo::ReachabilityCallback( SCNetworkReachabilityRef inTarget, SCNetworkConnectionFlags inFlags, void *inInfo )
{
	CLDAPReplicaInfo	*replica = (CLDAPReplicaInfo *) inInfo;
	
	replica->ReachabilityNotification( inFlags );
}

#pragma mark -
#pragma mark Class Definition

CLDAPReplicaInfo::CLDAPReplicaInfo( const char *inReplicaIP, int inPort, bool inSSL, bool inSupportsWrites )
{
	fIPAddress = strdup( inReplicaIP );
	fAddrInfo = NULL;
	fbSupportsWrites = inSupportsWrites;
	fFlags = (inSSL == true ? kReplicaFlagSSL : 0);
	fReachabilityRef = NULL;
	fPort = inPort;
	fSASLMethods = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	fNamingContexts = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	fServerFQDN = NULL;
	fServicePrincipal = NULL;
	fSupportedSecurity = kSecNoSecurity;
	fReachable = false;
	fVerified = false;
	fUnchecked = true;
	fLDAPURI = NULL;
	
	struct addrinfo	hints	= { 0 };
	char			portString[16];

	hints.ai_family		= PF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;
	hints.ai_flags		= AI_NUMERICHOST;
	
	snprintf( portString, sizeof(portString), "%d", fPort );
	if ( getaddrinfo(fIPAddress, portString, &hints, &fAddrInfo) == 0 )
	{
		fReachabilityRef = SCNetworkReachabilityCreateWithAddress( kCFAllocatorDefault, fAddrInfo->ai_addr );
		if ( fReachabilityRef != NULL )
		{
			SCNetworkConnectionFlags		flags	= 0;
			SCNetworkReachabilityContext	context	= { 0, this, NULL, NULL, NULL };

			SCNetworkReachabilitySetCallback( fReachabilityRef, ReachabilityCallback, &context );
			SCNetworkReachabilityScheduleWithRunLoop( fReachabilityRef, CFRunLoopGetMain(), kCFRunLoopDefaultMode );
			
			if ( SCNetworkReachabilityGetFlags(fReachabilityRef, &flags) )
			{
				if ( (flags & kSCNetworkFlagsReachable) != 0 && (flags & kSCNetworkFlagsConnectionRequired) == 0 )
					fReachable = true;
			}
		}
	}
}

CLDAPReplicaInfo::CLDAPReplicaInfo( const char *inLDAPI )
{
	fIPAddress = strdup( "ldapi" );
	fAddrInfo = NULL;
	fbSupportsWrites = true;
	fFlags = 0;
	fReachabilityRef = NULL;
	fPort = LDAP_PORT;
	fSASLMethods = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	fNamingContexts = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	fServerFQDN = NULL;
	fServicePrincipal = NULL;
	fSupportedSecurity = kSecNoSecurity;
	fReachable = true;
	fVerified = false;
	fUnchecked = true;
	fLDAPURI = strdup( inLDAPI );
}

CLDAPReplicaInfo::~CLDAPReplicaInfo( void )
{
	if ( fAddrInfo != NULL ) {
		freeaddrinfo( fAddrInfo );
		fAddrInfo = NULL;
	}

	DSFree( fLDAPURI );
	DSFree( fIPAddress );

	if ( fReachabilityRef != NULL ) {
		SCNetworkReachabilityUnscheduleFromRunLoop( fReachabilityRef, CFRunLoopGetMain(), kCFRunLoopDefaultMode );
		DSCFRelease( fReachabilityRef );
	}

	DSCFRelease( fSASLMethods );
	DSCFRelease( fNamingContexts );
	DSFree( fServerFQDN );
	DSFree( fServicePrincipal );
}

bool CLDAPReplicaInfo::SupportsSASLMethod( CFStringRef inSASLMethod )
{
	bool	bReturnValue	= false;

	if ( inSASLMethod == NULL )
		return true;

	if ( fReachable == true && fVerified == true )
	{
		fMutex.WaitLock();
		bReturnValue = CFArrayContainsValue( fSASLMethods, CFRangeMake(0, CFArrayGetCount(fSASLMethods)), inSASLMethod );
		fMutex.SignalLock();
	}
	
	return bReturnValue;
}

CFArrayRef CLDAPReplicaInfo::CopySASLMethods( void )
{
	CFArrayRef	cfReturnValue	= NULL;
	
	if ( fReachable == true && fVerified == true )
	{
		fMutex.WaitLock();
		if ( CFArrayGetCount(fSASLMethods) != 0 )
			cfReturnValue = CFArrayCreateCopy( kCFAllocatorDefault, fSASLMethods );
		fMutex.SignalLock();		
	}
	
	return cfReturnValue;
}

CFArrayRef CLDAPReplicaInfo::CopyNamingContexts( void )
{
	CFArrayRef	cfReturnValue	= NULL;
	
	if ( fReachable == true && fVerified == true )
	{
		fMutex.WaitLock();
		if ( CFArrayGetCount(fNamingContexts) != 0 )
			cfReturnValue = CFArrayCreateCopy( kCFAllocatorDefault, fNamingContexts );
		fMutex.SignalLock();		
	}
	
	return cfReturnValue;
}

char *CLDAPReplicaInfo::CopyServicePrincipal( void )
{
	char	*returnValue		= NULL;
	
	fMutex.WaitLock();
	
	// if we have a principal, let's just use it as is
	if ( fServicePrincipal != NULL )
	{
		returnValue = strdup( fServicePrincipal );
	}
	else
	{
		char *host			= NULL;
		char hostname[256]	= { 0 };
		
		if ( fServerFQDN != NULL )
		{
			host = fServerFQDN;
		}
		else if ( strcmp(fIPAddress, "127.0.0.1") == 0 )
		{
			if ( gethostname(hostname, sizeof(hostname)) == 0 )
				host = hostname;
		}
		else
		{
			struct in_addr ipAddr = {0};
			struct hostent *hostEnt = NULL;
			int err_num = 0;
			
			if ( inet_pton(AF_INET, fIPAddress, &ipAddr) == 1 )
			{
				hostEnt = getipnodebyaddr( &ipAddr, sizeof(ipAddr), AF_INET, &err_num );
				if ( hostEnt != NULL )
				{
					strlcpy( hostname, hostEnt->h_name, sizeof(hostname) );
					host = hostname;
					freehostent( hostEnt );
				}
			}
		}
		
		// if we ended up with a host let's build the principal
		if ( host != NULL )
		{
			int length = (sizeof(kLDAPServicePrincipalType)-1) + 1 + strlen(host) + 1;
			returnValue = (char *) calloc( length, sizeof(char) );
			snprintf( returnValue, length, "%s/%s", kLDAPServicePrincipalType, host );
		}
	}
	
	fMutex.SignalLock();
	
	return returnValue;
}

bool CLDAPReplicaInfo::ShouldAttemptCheck( void )
{
	return ( fReachable == true && (fUnchecked == true || fVerified == true) );
}

void CLDAPReplicaInfo::ReachabilityNotification( SCNetworkConnectionFlags inFlags )
{
	// if reachable is false, or connection required, clear the LDAP server
	if ( (inFlags & kSCNetworkFlagsReachable) == 0 || (inFlags & kSCNetworkFlagsConnectionRequired) != 0 )
	{
		OSAtomicCompareAndSwap32Barrier( true, false, &fReachable );
	}
	else
	{
		OSAtomicCompareAndSwap32Barrier( false, true, &fReachable );
		OSAtomicCompareAndSwap32Barrier( false, true, &fUnchecked );
	}
	
	DbgLog( kLogPlugin, "CLDAPReplicaInfo::ReachabilityNotification - Replica %s is %s", fIPAddress, (fReachable ? "reachable" : "not reachable") );
}

LDAP *CLDAPReplicaInfo::CreateLDAP( void )
{
	if ( fReachable == true && fVerified == true )
		return CreateLDAPInternal();
	
	return NULL;
}

void CLDAPReplicaInfo::ConnectionFailed( void )
{
	OSAtomicCompareAndSwap32Barrier( true, false, &fVerified );
	OSAtomicCompareAndSwap32Barrier( true, false, &fUnchecked );
}

LDAP *CLDAPReplicaInfo::CreateLDAPInternal( void )
{
	LDAP	*pTempLD = NULL;
	
	if ( fLDAPURI != NULL )
	{
		ldap_initialize( &pTempLD, fLDAPURI );
	}
	else
	{
		if ( (fFlags & kReplicaFlagSSL) != 0 )
		{
			// for SSL need to use the server's FQDN, not IP address, as the FQDN
			// is what is in the server's certificate.  Names must match for the
			// connection to succeed.
			struct sockaddr_in sa = { 0 };
			sa.sin_len = sizeof(sa);
			sa.sin_family = PF_INET;
			sa.sin_addr.s_addr = inet_addr( fIPAddress );
			char hostName[NI_MAXHOST];

			if ( getnameinfo( (struct sockaddr*)&sa, sa.sin_len, hostName, sizeof(hostName), NULL, 0, NI_NAMEREQD ) == 0 )
			{
				pTempLD = ldap_init( hostName, fPort );
			}
		}
		else
		{
			pTempLD = ldap_init( fIPAddress, fPort );
		}
		
		if ( pTempLD != NULL )
		{
			int		flag	= 1;

			int ldapOptVal = LDAP_VERSION3;
			ldap_set_option( pTempLD, LDAP_OPT_PROTOCOL_VERSION, &ldapOptVal );
			
			// we only do SSL if we aren't doing LDAPv2
			if ( (fFlags & kReplicaFlagSSL) != 0 )
			{
				ldapOptVal = LDAP_OPT_X_TLS_HARD;
				ldap_set_option(pTempLD, LDAP_OPT_X_TLS, &ldapOptVal);
			}
			
			// set the NOADDR error too
			ldap_set_option( pTempLD, LDAP_OPT_NOADDRERR, &flag );
		}
	}
	
	return pTempLD;
}

LDAP *CLDAPReplicaInfo::VerifiedServerConnection( int inTimeoutSeconds, bool inForceCheck, void *inCallback, void *inParam )
{
	LDAP	*pReturnHost	= NULL;
	
	fMutex.WaitLock();
	
	// if it is in theory reachable, but not to a rootDSE
	if ( fReachable == true && (fUnchecked == true || inForceCheck == true) )
	{
		// remove all of the known SASL methods since we'll check them again
		CFArrayRemoveAllValues( fSASLMethods );
		CFArrayRemoveAllValues( fNamingContexts );
		DSFree( fServerFQDN );
		DSFree( fServicePrincipal );

		// now create a LDAP * to the known server and try to get SASL methods if necessary
		pReturnHost = CreateLDAPInternal();
		if ( pReturnHost != NULL )
		{
			struct timeval	timeout		= { inTimeoutSeconds, 0 }; 
			LDAPMessage		*result		= NULL;
			const char		*attrs[]	= { "supportedSASLMechanisms", "namingContexts", "dnsHostName", "krbName", NULL };

			if ( fLDAPURI == NULL )
			{
				ldap_set_option( pReturnHost, LDAP_OPT_TIMEOUT, &timeout );
				ldap_set_option( pReturnHost, LDAP_OPT_NETWORK_TIMEOUT, &timeout );

				if ( inCallback != NULL )
				{
					ldap_set_option( pReturnHost, LDAP_OPT_NOTIFYDESC_PROC, inCallback );
					ldap_set_option( pReturnHost, LDAP_OPT_NOTIFYDESC_PARAMS, inParam );
				}
			}

			// We are just asking for everything to see if we get a response
			int ldapReturnCode = ldap_search_ext_s( pReturnHost, "", LDAP_SCOPE_BASE, "(objectclass=*)", (char **)attrs, FALSE, NULL, NULL, 
												    &timeout, 0, &result );
			if ( ldapReturnCode == LDAP_SUCCESS )
			{
				struct berval **bValues;
				
				bValues = ldap_get_values_len( pReturnHost, result, "supportedSASLMechanisms" );
				if ( bValues != NULL )
				{
					uint32_t ii = 0;
					
					while ( bValues[ii] != NULL )
					{
						CFStringRef cfValue = CFStringCreateWithCString( kCFAllocatorDefault, bValues[ii]->bv_val, kCFStringEncodingUTF8 ); 
						CFArrayAppendValue( fSASLMethods, cfValue );
						CFRelease( cfValue );
						ii++;
					}
					
					ldap_value_free_len( bValues );
					bValues = NULL;
				}

				bValues = ldap_get_values_len( pReturnHost, result, "namingContexts" );
				if ( bValues != NULL )
				{
					uint32_t ii = 0;
					
					while ( bValues[ii] != NULL )
					{
						CFStringRef cfValue = CFStringCreateWithCString( kCFAllocatorDefault, bValues[ii]->bv_val, kCFStringEncodingUTF8 );
						CFArrayAppendValue( fNamingContexts, cfValue );
						CFRelease( cfValue );
						ii++;
					}
					
					ldap_value_free_len( bValues );
					bValues = NULL;
				}
				
				bValues = ldap_get_values_len( pReturnHost, result, "dnsHostName" );
				if ( bValues != NULL )
				{
					if ( bValues[0] != NULL )
					{
						char *pTemp = bValues[0]->bv_val;
						if ( DSIsStringEmpty(pTemp) == false )
						{
							int	bFlag	= 1;
							
							fServerFQDN = strdup( pTemp );
							
							// if we detect a FQDN provided by the server, we'll use that instead of relying on reverse lookups
							// since this is the name the server is expecting for it's service ticket
							DbgLog( kLogPlugin, "CLDAPReplicaInfo::VerifiedServerConnection - setting FQDN for server as %s from rootDSE", 
								    fServerFQDN );
							ldap_set_option( pReturnHost, LDAP_OPT_SASL_FQDN, fServerFQDN );
							ldap_set_option( pReturnHost, LDAP_OPT_NOREVERSE_LOOKUP, &bFlag );
						}
					}
					
					ldap_value_free_len( bValues );
					bValues = NULL;
				}
				
				bValues = ldap_get_values_len( pReturnHost, result, "krbName" );
				if ( bValues != NULL )
				{
					if ( bValues[0] != NULL )
					{
						char *pTemp = bValues[0]->bv_val;
						if ( DSIsStringEmpty(pTemp) == false )
						{
							fServicePrincipal = strdup( pTemp );
							DbgLog( kLogPlugin, "CLDAPReplicaInfo::VerifiedServerConnection - service principal for server is %s", 
								    fServicePrincipal );
						}
					}
					
					ldap_value_free_len( bValues );
					bValues = NULL;
				}
				
				ldap_msgfree( result );
				result = NULL;
				
				DbgLog( kLogPlugin, "CLDAPReplicaInfo::VerifiedServerConnection - Verified server connectivity - %s", fIPAddress );
				
				OSAtomicCompareAndSwap32Barrier( false, true, &fVerified );
			}
			else
			{
				DbgLog( kLogPlugin, "CLDAPReplicaInfo::VerifiedServerConnection - Unable to verify server connectivity %s", fIPAddress );

				OSAtomicCompareAndSwap32Barrier( true, false, &fVerified );
				ldap_unbind_ext_s( pReturnHost, NULL, NULL );
				pReturnHost = NULL;
			}
		}

		// we attempted to do something with it, so clear the unchecked bit
		OSAtomicCompareAndSwap32Barrier( true, false, &fUnchecked );
	}
	
	int32_t iSecurityLevel = kSecNoSecurity;
	
	// if we have SSL, we have some capabilities already..
	if ( (fFlags & kReplicaFlagSSL) != 0 )
		iSecurityLevel |= (kSecDisallowCleartext | kSecPacketEncryption);

	CFRange		stRange = CFRangeMake( 0, CFArrayGetCount(fSASLMethods) );
	
	// we need to verify with CLDAPNode what types are supported..... TBD
	if ( CFArrayContainsValue(fSASLMethods, stRange, CFSTR("CRAM-MD5")) )
		iSecurityLevel |= kSecDisallowCleartext;
	
	if ( CFArrayContainsValue(fSASLMethods, stRange, CFSTR("GSSAPI")) )
		iSecurityLevel |= (kSecDisallowCleartext | kSecPacketEncryption | kSecManInMiddle | kSecPacketSigning);
	
	fSupportedSecurity = iSecurityLevel;
	OSMemoryBarrier();
	
	fMutex.SignalLock();
	
	return pReturnHost;
}
