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

#include "CLDAPConnection.h"
#include "CLDAPNodeConfig.h"
#include "CLDAPReplicaInfo.h"
#include "CCachePlugin.h"
#include "CSharedData.h"

#include <DirectoryServiceCore/PrivateTypes.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryServiceCore/CLog.h>
#include <sasl/sasl.h>
#include <syslog.h>

extern	CCachePlugin		*gCacheNode;
extern	DSMutexSemaphore	*gKerberosMutex;

CLDAPConnection::CLDAPConnection( CLDAPNodeConfig *inNodeConfig, CLDAPReplicaInfo *inCurrentReplica )
{
	fNodeConfig = (inNodeConfig != NULL ? inNodeConfig->Retain() : NULL);
	fReplicaInUse = (inCurrentReplica != NULL ? inCurrentReplica->Retain() : NULL);

	fbAuthenticated = false;
	fbKerberosAuthenticated = false;
	fHost = NULL;
	
	fKerberosCache = NULL;

	fWriteable = false;
	fbBadCredentials = false;
	fKerberosID = NULL;
	fLDAPUsername = NULL;
	fLDAPPassword = NULL;
	fLDAPRecordType = NULL;
	
	fIdleCount = 0;
	
	fReachabilityList = NULL;
	fConnectionStatus = kConnectionSafe;
	
#if defined(DEBUG_LOCKS) || defined(DEBUG_LOCKS_HISTORY) || defined(DEBUG_LDAPSESSION_LOCKS)
	const char *fileName = __FILE__;
	char *offset = strstr(fileName, "/PlugIns/");
	if ( offset != NULL )
		fPrefixLen = (offset - fileName) + 1;
	else
		fPrefixLen = 0;
#endif	
}

CLDAPConnection::~CLDAPConnection( void )
{
	// let's grab our lock as a safety, even though it should not be necessary
	fMutex.WaitLock();
	
	if ( fNodeConfig != NULL )
	{
		if ( fLDAPUsername != NULL )
			DbgLog( kLogPlugin, "CLDAPConnection::~CLDAPConnection - Connection to %s:%s no longer in use - deleted", fNodeConfig->fNodeName, 
				    fLDAPUsername );
		else
			DbgLog( kLogPlugin, "CLDAPConnection::~CLDAPConnection - Connection to %s no longer in use - deleted", fNodeConfig->fNodeName );
	}
	else
	{
		DbgLog( kLogPlugin, "CLDAPConnection::~CLDAPConnection - Connection to Configure Node no longer in use - deleted" );
	}
	
	DSRelease( fNodeConfig );
	
	DSFree( fKerberosID );
	DSFree( fLDAPUsername );
	DSFree( fLDAPPassword );
	DSFree( fLDAPRecordType );
	
	DSRelease( fReplicaInUse );

	if ( fHost != NULL )
	{
		ldap_unbind_ext_s( fHost, NULL, NULL );
		fHost = NULL;
	}
	
	if ( fKerberosCache != NULL )
	{
		gKerberosMutex->WaitLock();
		
		krb5_context	krbContext	= NULL;
		krb5_ccache		krbCache	= NULL;
		krb5_error_code	retval		= 0;

		retval = krb5_init_context( &krbContext );
		if ( retval == 0 )
			retval = krb5_cc_resolve( krbContext, fKerberosCache, &krbCache);
		
		if ( retval == 0 )
			krb5_cc_destroy( krbContext, krbCache );
		
		DSFree( fKerberosCache );

		gKerberosMutex->SignalLock();
	}
	
	// fReachabilityList should be empty because we closed our LDAP connections
	fReachabilityList = NULL;
	
	fMutex.SignalLock();
}

CLDAPConnection	*CLDAPConnection::CreateCopy( void )
{
	CLDAPConnection	*newConnection = NULL;
	
	if ( fConnectionStatus == kConnectionSafe )
		newConnection = new CLDAPConnection( fNodeConfig, fReplicaInUse );
	
	return newConnection;
}

#if defined(DEBUG_LOCKS) || defined(DEBUG_LOCKS_HISTORY) || defined(DEBUG_LDAPSESSION_LOCKS)
LDAP *CLDAPConnection::LockLDAPSessionDebug( const char *inFile, int inLine )
#else
LDAP *CLDAPConnection::LockLDAPSession( void )
#endif
{
	// if the connection isn't safe, just return NULL
	if ( fConnectionStatus == kConnectionUnsafe ||
		 fNodeConfig == NULL ||
		 fNodeConfig->fConfigDeleted == true ||
		 fNodeConfig->fEnableUse == false )
	{
		return NULL;
	}
	
#if defined(DEBUG_LOCKS) || defined(DEBUG_LOCKS_HISTORY)
	fMutex.WaitDebug( inFile, inLine );
#else
	fMutex.WaitLock();
#endif
	
	if ( fHost != NULL )
	{
		// if this connection wasn't safe, we need to unbind the session so we can try to re-establish if necessary
		if ( fConnectionStatus != kConnectionSafe )
		{
			DbgLog( kLogDebug, "CLDAPConnection::LockLDAPSession - closing existing session - state is not safe" );
			ldap_unbind_ext_s( fHost, NULL, NULL );
			fHost = NULL;
		}
		else
		{
			// otherwise let's refresh our dynamic data without closing our connection, if necessary
			fNodeConfig->UpdateDynamicData( fHost, fReplicaInUse );
		}
	}
	
	// if we don't have a pointer right now, try to get a new one
	if ( fHost == NULL && fbBadCredentials == false )
	{
		tDirStatus	dsStatus;
		
		if ( fbAuthenticated )
			fHost = fNodeConfig->EstablishConnection( &fReplicaInUse, fWriteable, fLDAPUsername, fKerberosID, fLDAPPassword, 
													  (void *) LDAPFrameworkCallback, (void *) this, &dsStatus );
		else if ( fbKerberosAuthenticated )
			fHost = fNodeConfig->EstablishConnection( &fReplicaInUse, fWriteable, fKerberosCache, (void *) LDAPFrameworkCallback, 
													  (void *) this, &dsStatus );
		else
			fHost = fNodeConfig->EstablishConnection( &fReplicaInUse, fWriteable, (void *) LDAPFrameworkCallback, (void *) this, &dsStatus );
		
		if ( fHost == NULL && (fbAuthenticated == true || fbKerberosAuthenticated == true) && dsStatus == eDSAuthFailed )
			fbBadCredentials = true;
	}
	
	// if we have a session, return it now holding the lock
	if ( fHost != NULL )
	{
		fIdleCount = 0;
		SetConnectionStatus( kConnectionSafe );
		
#if defined(DEBUG_LOCKS) || defined(DEBUG_LOCKS_HISTORY) || defined(DEBUG_LDAPSESSION_LOCKS)
		DbgLog( kLogDebug, "CLDAPConnection::LockLDAPSession - %s:%d locked session handle %X", inFile + fPrefixLen, inLine, fHost );
#endif
		
		return fHost;
	}

	SetConnectionStatus( kConnectionUnsafe );
	
	// we don't keep the lock if we had no valid session
#if defined(DEBUG_LOCKS) || defined(DEBUG_LOCKS_HISTORY)
	fMutex.SignalDebug( inFile, inLine );
#else
	fMutex.SignalLock();
#endif
	
	return NULL;
}

#if defined(DEBUG_LOCKS) || defined(DEBUG_LOCKS_HISTORY) || defined(DEBUG_LDAPSESSION_LOCKS)
void CLDAPConnection::UnlockLDAPSessionDebug( LDAP * &inLDAP, bool inFailed, const char *inFile, int inLine )
#else
void CLDAPConnection::UnlockLDAPSession( LDAP * &inLDAP, bool inFailed )
#endif
{
	if ( inLDAP != NULL )
	{
		if ( inLDAP == fHost )
		{
			if ( inFailed == true )
			{
				ldap_unbind_ext_s( fHost, NULL, NULL );
				fHost = NULL;
				
				if ( fReplicaInUse != NULL ) fReplicaInUse->ConnectionFailed();
				SetConnectionStatus( kConnectionUnknown );
			}
			
			inLDAP = NULL;

#if defined(DEBUG_LOCKS) || defined(DEBUG_LOCKS_HISTORY) || defined(DEBUG_LDAPSESSION_LOCKS)
			DbgLog( kLogDebug, "CLDAPConnection::UnlockLDAPSession - %s:%d unlocked session handle %X", inFile + fPrefixLen, inLine, fHost );
#endif
			
#if defined(DEBUG_LOCKS) || defined(DEBUG_LOCKS_HISTORY)
			fMutex.SignalDebug( inFile, inLine);
#else
			fMutex.SignalLock();
#endif
		}
		else
		{
#if defined(DEBUG_LOCKS) || defined(DEBUG_LOCKS_HISTORY) || defined(DEBUG_LDAPSESSION_LOCKS)
			DbgLog( kLogDebug, "CLDAPConnection::UnlockLDAPSession - %s:%d attempted to unlock session handle %X != %X", inFile + fPrefixLen, inLine, 
				    inLDAP, fHost );
			syslog( LOG_CRIT, "LDAPv3 - %s:%d attempted to unlock session handle 0x%08x != 0x%08x", inFile + fPrefixLen, inLine, inLDAP, fHost );
#else
			syslog( LOG_CRIT, "LDAPv3:  Attempt to unlock LDAP session not owned by this object" );
#endif
			abort();
		}
	}
}

UInt32 CLDAPConnection::SessionSecurityLevel( LDAP *inLDAP )
{
	UInt32	iSecurityLevel	= 0;
	
	if ( inLDAP != NULL )
	{
		sasl_ssf_t	negotiatedSSF;
		
		if ( fNodeConfig->fIsSSL )
			iSecurityLevel |= (kSecDisallowCleartext | kSecPacketEncryption);
		
		// get the SSF setting so we can return the minimum integrity
		if ( ldap_get_option(inLDAP, LDAP_OPT_X_SASL_SSF, &negotiatedSSF) == 0 )
		{
			if ( negotiatedSSF >= 56 )
				iSecurityLevel |= (kSecDisallowCleartext | kSecPacketSigning | kSecPacketEncryption | kSecManInMiddle);
			else if ( negotiatedSSF >= 1 )
				iSecurityLevel |= (kSecDisallowCleartext | kSecPacketSigning | kSecManInMiddle);
		}
	}
	
	return iSecurityLevel;
}

void CLDAPConnection::PeriodicTask( void )
{
	if ( fMutex.WaitTryLock() )
	{
		if ( fHost != NULL )
		{
			if ( fConnectionStatus != kConnectionSafe || (++fIdleCount) > fNodeConfig->fIdleMaxCount )
			{
				if ( fbAuthenticated )
					DbgLog( kLogPlugin, "CLDAPConnection::PeriodicTask - Status Node: %s:%s -- Server: %s -- %sDisconnected", 
						    fNodeConfig->fNodeName, fLDAPUsername, fReplicaInUse->fIPAddress, 
						    (fConnectionStatus == kConnectionSafe ? "idle " : "") );
				else
					DbgLog( kLogPlugin, "CLDAPConnection::PeriodicTask - Status Node: %s -- Server: %s -- %sDisconnected", 
						    fNodeConfig->fNodeName, fReplicaInUse->fIPAddress, 
						    (fConnectionStatus == kConnectionSafe ? "idle " : "") );

				ldap_unbind_ext_s( fHost, NULL, NULL );
				fHost = NULL;
			}
			else
			{
				if ( fbAuthenticated )
					DbgLog( kLogDebug, "CLDAPConnection::PeriodicTask - Status Node: %s:%s -- Server: %s -- Time: %d sec -- Idle", 
						    fNodeConfig->fNodeName, fLDAPUsername, fReplicaInUse->fIPAddress, fIdleCount * 30 );
				else
					DbgLog( kLogDebug, "CLDAPConnection::PeriodicTask - Status Node: %s -- Server: %s -- Time: %d sec -- Idle", fNodeConfig->fNodeName, 
						    fReplicaInUse->fIPAddress, fIdleCount * 30 );
			}
		}
		
		fMutex.SignalLock();
	}
}

void CLDAPConnection::NetworkTransition( void )
{
	// set the connection status to unknown if we are currently unsafe
	OSAtomicCompareAndSwap32Barrier( kConnectionUnsafe, kConnectionUnknown, &fConnectionStatus );
}

tDirStatus CLDAPConnection::Authenticate( const char *inLDAPUsername, const char *inRecordType, const char *inKerberosID, const char *inPassword )
{
	tDirStatus	dsStatus	= eDSAuthFailed;
	
	if ( inLDAPUsername != NULL && inPassword != NULL )
	{
		fbAuthenticated = true;
		fLDAPUsername = strdup( inLDAPUsername );
		fLDAPRecordType = (inRecordType != NULL ? strdup(inRecordType) : strdup(kDSStdRecordTypeUsers));
		if ( inKerberosID != NULL ) fKerberosID = strdup( inKerberosID );
		fLDAPPassword = strdup( inPassword );

		// check to see if we have a handle, because we need to disconnect if we are going to authenticate
		fMutex.WaitLock();
		if ( fHost != NULL )
		{
			ldap_unbind_ext_s( fHost, NULL, NULL );
			fHost = NULL;
		}
		fMutex.SignalLock();
		
		LDAP *pTempLD = LockLDAPSession();
		if ( pTempLD != NULL )
		{
			dsStatus = eDSNoErr;
			UnlockLDAPSession( pTempLD, false );
		}
	}
	
	return dsStatus;
}

tDirStatus CLDAPConnection::AuthenticateKerberos( const char *inUsername, const char *inRecordType, krb5_creds *inCredsPtr, const char *inKerberosID )
{
	if ( inUsername == NULL || inCredsPtr == NULL || inKerberosID == NULL )
		return eDSNullParameter;
	
	if ( fReplicaInUse == NULL || fReplicaInUse->SupportsSASLMethod(CFSTR("GSSAPI")) == false )
		return eDSAuthMethodNotSupported;
	
	tDirStatus		dsStatus			= eDSAuthFailed;
	krb5_context	krbContext			= NULL;
	krb5_principal	principal			= NULL;
	krb5_ccache		krbCache			= NULL;
	krb5_error_code	retval				= 0;
	char			*principalString	= NULL;
	
	gKerberosMutex->WaitLock();

	do
	{
		retval = krb5_init_context( &krbContext );
		if ( retval ) break;
		
		retval = krb5_parse_name( krbContext, inUsername, &principal );
		if ( retval ) break;
		
		retval = krb5_unparse_name( krbContext, principal, &principalString );
		if ( retval ) break;
		
		// use the cache code to generate a unique cache name
		retval = krb5_cc_new_unique( krbContext, "MEMORY", inUsername, &krbCache );
		if ( retval ) break;
		
		const char *cacheName = krb5_cc_get_name( krbContext, krbCache );
		
		int length = sizeof("MEMORY:") + strlen(cacheName) + 1;
		fKerberosCache = (char *) calloc( length, sizeof(char) );
		
		strlcpy( fKerberosCache, "MEMORY:", length );
		strlcat( fKerberosCache, cacheName, length );
		
		// let's see if we already have a cache for this user..
		retval = krb5_cc_resolve( krbContext, fKerberosCache, &krbCache);
		if ( retval ) break;
		
		retval = krb5_cc_set_default_name( krbContext, fKerberosCache );
		if ( retval ) break;
		
		retval = krb5_cc_initialize( krbContext, krbCache, principal );
		if ( retval ) break;

		DbgLog( kLogDebug, "CLDAPConnection::AuthenticateKerberos - Initialized cache <%s> for user <%s>", fKerberosCache, principalString );
		
		// GSSAPI's cache name needs to be set to match if we are getting ready to use GSSAPI
		retval = krb5_cc_store_cred( krbContext, krbCache, inCredsPtr );
		if ( retval ) break;
		
		// we got this far set the flag and try it
		fbKerberosAuthenticated = true;
		fLDAPUsername = strdup( inUsername );
		fLDAPRecordType = (inRecordType != NULL ? strdup(inRecordType) : strdup(kDSStdRecordTypeUsers));
		fKerberosID = strdup( inKerberosID );
		
		// check to see if we have a handle, because we need to disconnect if we are going to authenticate
		fMutex.WaitLock();
		if ( fHost != NULL )
		{
			ldap_unbind_ext_s( fHost, NULL, NULL );
			fHost = NULL;
		}
		fMutex.SignalLock();

		LDAP *pTempLD = LockLDAPSession();
		if ( pTempLD != NULL )
		{
			// must have worked, we negotiated a session
			dsStatus = eDSNoErr;
			UnlockLDAPSession( pTempLD, false );
		} else {
			// LockLDAPSession destroys this cache automatically if it fails
			krbCache = NULL;
			DSFree( fKerberosCache );
		}
		
	} while ( 0 );
								   
	if ( principalString != NULL )
	{
		krb5_free_unparsed_name( krbContext, principalString );
		principalString = NULL;
	}
	
	if ( principal != NULL )
	{
		krb5_free_principal( krbContext, principal );
		principal = NULL;
	}
	
	if ( krbCache != NULL )
	{
		// if the auth failed, let's destroy the cache instead..
		if ( eDSNoErr != dsStatus )
		{
			krb5_cc_destroy( krbContext, krbCache );
			krbCache = NULL;
		}
		else 
		{
			krb5_cc_close( krbContext, krbCache );
			krbCache = NULL;
		}
	}
	
	if ( inCredsPtr != NULL )
	{
		// No need to hold onto credentials here..
		krb5_free_cred_contents( krbContext, inCredsPtr );
		inCredsPtr = NULL;
	}
	
	if ( krbContext != NULL )
	{
		krb5_free_context( krbContext );
		krbContext = NULL;
	}
	
	gKerberosMutex->SignalLock();

	return dsStatus;
}

void CLDAPConnection::UpdateCredentials( const char *inPassword )
{
	// should already be locked at this point, if not we have a problem
	if ( fbAuthenticated == true && inPassword != NULL )
	{
		DSFree( fLDAPPassword );
		fLDAPPassword = strdup( inPassword );
	}
}

void CLDAPConnection::CheckFailed( void )
{
	if ( fNodeConfig != NULL && fNodeConfig->CheckIfFailed() == false )
	{
		SetConnectionStatus( kConnectionSafe );
	}
}

char *CLDAPConnection::CopyReplicaIPAddress( void )
{
	char	*returnValue	= NULL;
	
	fMutex.WaitLock();
	if ( fReplicaInUse != NULL )
		returnValue = strdup( fReplicaInUse->fIPAddress );
	fMutex.SignalLock();
	
	return returnValue;
}

char *CLDAPConnection::CopyReplicaServicePrincipal( void )
{
	char	*returnValue	= NULL;
	
	fMutex.WaitLock();
	if ( fReplicaInUse != NULL )
		returnValue = fReplicaInUse->CopyServicePrincipal();
	fMutex.SignalLock();
	
	return returnValue;
}

void CLDAPConnection::CloseConnectionIfPossible( void )
{
	char	nodeName[256];

	if ( fNodeConfig == NULL )
	{
		strlcpy( nodeName, "Unknown", sizeof(nodeName) );
	}
	else
	{
		strlcpy( nodeName, "/LDAPv3/", sizeof(nodeName) );
		strlcat( nodeName, fNodeConfig->fNodeName, sizeof(nodeName) );
	}

	// try lock, if we can't grab it, we don't want to block
	if ( fMutex.WaitTryLock() )
	{
		if ( fHost != NULL )
		{
			DbgLog( kLogPlugin, "CLDAPConnection::CloseConnectionIfPossible - %s - closed LDAP session - not in use",
				   nodeName );
			ldap_unbind_ext_s( fHost, NULL, NULL );
			fHost = NULL;
		}
		fMutex.SignalLock();
	}	
	else if ( fHost != NULL ) // we don't care, just informative
	{
		DbgLog( kLogPlugin, "CLDAPConnection::CloseConnectionIfPossible - %s - unable to close LDAP session - in use",
			   nodeName );
	}
}

void CLDAPConnection::SetConnectionStatus( int32_t inStatus )
{
	int32_t	oldStatus = fConnectionStatus;
	
	fConnectionStatus = inStatus;
	OSMemoryBarrier();
 	
	// if our status changed let's update the node reachability too
	if ( oldStatus != inStatus )
	{
		if ( fNodeConfig != NULL )
		{
			char	nodeName[256];
			
			strlcpy( nodeName, "/LDAPv3/", sizeof(nodeName) );
			strlcat( nodeName, fNodeConfig->fNodeName, sizeof(nodeName) );
			
			dsSetNodeCacheAvailability( nodeName, (inStatus == kConnectionSafe) );
		}
	}
}

void CLDAPConnection::ReachabilityCallback( SCNetworkReachabilityRef inTarget, SCNetworkConnectionFlags inFlags, void *inInfo )
{
	CLDAPConnection	*pLDAPConnection	= (CLDAPConnection *) inInfo;
	
	pLDAPConnection->ReachabilityNotification( inTarget, inFlags );
}

void CLDAPConnection::LDAPFrameworkCallback( LDAP *inLD, int inDesc, int inOpening, void *inParams )
{
	CLDAPConnection	*pLDAPConnection	= (CLDAPConnection *) inParams;
	
	if ( inOpening )
		pLDAPConnection->StartReachability( inDesc );
	else
		pLDAPConnection->StopReachability( inDesc );
}

void CLDAPConnection::ReachabilityNotification( SCNetworkReachabilityRef inTarget, SCNetworkConnectionFlags inFlags )
{
	// if reachable is false, or connection required, clear the LDAP server
	if ( (inFlags & kSCNetworkFlagsReachable) == 0 || (inFlags & kSCNetworkFlagsConnectionRequired) != 0 )
	{
		sLDAPReachabilityList	*prevPtr;
		sLDAPReachabilityList	*currPtr;
		char					nodeName[256];
		
		strlcpy( nodeName, "/LDAPv3/", sizeof(nodeName) );
		strlcat( nodeName, fNodeConfig->fNodeName, sizeof(nodeName) );
		
		fReachabilityLock.WaitLock();
		for (prevPtr = NULL, currPtr = fReachabilityList; currPtr != NULL; prevPtr = currPtr, currPtr = currPtr->next)
		{
			if ( currPtr->reachabilityRef == inTarget )
			{
				SCNetworkReachabilityUnscheduleFromRunLoop( currPtr->reachabilityRef, CFRunLoopGetMain(), kCFRunLoopDefaultMode );
				CFRelease( currPtr->reachabilityRef );
				
				// if our previous ptr is non-NULL, reset it's next ptr
				if (prevPtr != NULL)
					prevPtr->next = currPtr->next;
				else // must be the head
					fReachabilityList = currPtr->next;
				
				DbgLog( kLogDebug, "CLDAPConnection::ReachabilityNotification - %s -> %s for node %s is no longer reachable", currPtr->srcIP, 
					    currPtr->dstIP, nodeName );
				
				delete currPtr; // delete the structure
				break;
			}
		}
		
		// if reachability list is NULL, time to turn the node off until we re-establish
		if ( fReachabilityList == NULL )
		{
			SetConnectionStatus( kConnectionUnknown );
			if ( fReplicaInUse != NULL ) fReplicaInUse->ConnectionFailed();
		}
		
		fReachabilityLock.SignalLock();
		
		CloseConnectionIfPossible();
	}
}

void CLDAPConnection::StartReachability( int inSocket )
{
	if ( inSocket >= 0 )
	{
		sockaddr_storage	localStorage;
		sockaddr_storage	peerStorage;
		sockaddr			*localAddr	= (sockaddr *) &localStorage;
		sockaddr			*peerAddr	= (sockaddr *) &peerStorage;
		socklen_t			localLen	= sizeof(sockaddr_storage);
		socklen_t			peerLen		= sizeof(sockaddr_storage);
		
		if ( 0 != getsockname(inSocket, localAddr, &localLen) )
		{
			localAddr = NULL;
			DbgLog( kLogError, "CLDAPConnection::StartReachability - could not get local sockaddr info for %d", inSocket );
		}
		
		if ( 0 != getpeername(inSocket, peerAddr, &peerLen) )
		{
			peerAddr = NULL;
			DbgLog( kLogError, "CLDAPConnection::StartReachability - could not get peer sockaddr info for %d", inSocket );
		}
		
		if (localAddr != NULL && peerAddr != NULL)
		{
			SCNetworkReachabilityRef scReachRef;
			
			scReachRef = SCNetworkReachabilityCreateWithAddressPair( kCFAllocatorDefault, localAddr, peerAddr );
			if (scReachRef != NULL)
			{
				SCNetworkReachabilityContext	reachabilityContext = { 0, NULL, NULL, NULL, NULL };
				sLDAPReachabilityList			*newReachItem		= new sLDAPReachabilityList;
				
				newReachItem->reachabilityRef	= scReachRef;
				newReachItem->socket			= inSocket;
				newReachItem->srcIP[0]			= '\0';
				newReachItem->dstIP[0]			= '\0';
				
				inet_ntop( localAddr->sa_family, (char *)(localAddr) + (localAddr->sa_family == AF_INET6 ? 8 : 4), newReachItem->srcIP,
						  sizeof(newReachItem->srcIP) );
				inet_ntop( peerAddr->sa_family, (char *)(peerAddr) + (peerAddr->sa_family == AF_INET6 ? 8 : 4), newReachItem->dstIP,
						  sizeof(newReachItem->dstIP) );
				
				// schedule with the run loop now that we are done
				reachabilityContext.info = this;
				
				if (SCNetworkReachabilitySetCallback(scReachRef, ReachabilityCallback, &reachabilityContext) == FALSE)
				{
					DbgLog( kLogError, "CLDAPConnection::StartReachability - unable to set callback for SCNetworkReachabilityRef for socket %d",
						   inSocket );
				}
				else if (SCNetworkReachabilityScheduleWithRunLoop(scReachRef, CFRunLoopGetMain(), kCFRunLoopDefaultMode) == FALSE)
				{
					DbgLog( kLogError, "CLDAPConnection::StartReachability - unable to schedule SCNetworkReachabilityRef with Runloop for socket %d",
						   inSocket );
				}
				else
				{
					DbgLog( kLogDebug, "CLDAPConnection::StartReachability - watching socket = %d, %s -> %s", inSocket, newReachItem->srcIP, 
						   newReachItem->dstIP );
					
					// now do lock and update the internal list
					fReachabilityLock.WaitLock();
					
					newReachItem->next	= fReachabilityList;	// set our next pointer
					fReachabilityList	= newReachItem;			// now set the new head of the list
					
					fReachabilityLock.SignalLock();
					
					newReachItem = NULL;
				}
				
				// we must have failed, clean up
				if ( newReachItem != NULL )
				{
					delete newReachItem;
					newReachItem = NULL;
					DSCFRelease( scReachRef );
				}
			}
			else
			{
				DbgLog( kLogError, "CLDAPConnection::StartReachability - unable to watch socket = %d", inSocket );
			}
		}
	}
	else
	{
		DbgLog( kLogError, "CLDAPConnection::StartReachability - unable to watch socket = %d", inSocket );
	}
}

void CLDAPConnection::StopReachability( int inSocket )
{
	sLDAPReachabilityList	*prevPtr;
	sLDAPReachabilityList	*currPtr;
	
	fReachabilityLock.WaitLock();

	for (prevPtr = NULL, currPtr = fReachabilityList; currPtr != NULL; prevPtr = currPtr, currPtr = currPtr->next)
	{
		if ( currPtr->socket == inSocket )
		{
			// if our previous ptr is non-NULL, reset it's next ptr
			if ( prevPtr != NULL )
				prevPtr->next = currPtr->next;
			else // must be the head
				fReachabilityList = currPtr->next;
			
			if ( currPtr->reachabilityRef != NULL )
			{
				SCNetworkReachabilityUnscheduleFromRunLoop( currPtr->reachabilityRef, CFRunLoopGetMain(), kCFRunLoopDefaultMode );
				CFRelease( currPtr->reachabilityRef );
				DbgLog( kLogDebug, "CLDAPConnection::StopReachability - no longer watching socket = %d", inSocket );
			}
			
			delete currPtr; // delete our pointer
			currPtr = NULL;
			break;
		}
	}
	
	fReachabilityLock.SignalLock();
}
