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

#include "CLDAPConnectionManager.h"
#include "CLDAPConnection.h"
#include "CLDAPNodeConfig.h"
#include "CLDAPv3Plugin.h"

#include <DirectoryService/DirectoryService.h>
#include <DirectoryServiceCore/CLog.h>
#include <Kerberos/krb5.h>
#include <stack>

using namespace std;

#pragma mark -
#pragma mark Globals, TypeDefs and Static Member Variables

extern uint32_t	gSystemGoingToSleep;

int32_t				CLDAPConnectionManager::fCheckThreadActive	= false;
double				CLDAPConnectionManager::fCheckFailedLastRun	= 0.0;
DSEventSemaphore	CLDAPConnectionManager::fCheckFailedEvent;

#pragma mark -
#pragma mark Struct sLDAPContextData Functions

sLDAPContextData::sLDAPContextData( const sLDAPContextData& inContextData )
{
	fType = 0;
	offset = 0;
	index = 1;
	
	// we don't dupe the fOpenRecordType = fOpenRecordName = fOpenRecordDN
	fOpenRecordType = fOpenRecordName = fOpenRecordDN = NULL;
	
	fUID = inContextData.fUID;
	fEffectiveUID = inContextData.fEffectiveUID;
	
	// we don't copy fPWSRef and fPWSNodeRef
	fPWSRef = 0;
	fPWSNodeRef = 0;
	fPWSUserIDLength = 0;
	fPWSUserID = NULL;
	
	fLDAPConnection = inContextData.fLDAPConnection->Retain();
}

sLDAPContextData::sLDAPContextData( CLDAPConnection *inConnection )
{
	fType = 0;
	offset = 0;
	index = 1;
	
	fOpenRecordType = fOpenRecordName = fOpenRecordDN = NULL;
	
	fPWSRef = 0;
	fPWSNodeRef = 0;
	fPWSUserIDLength = 0;
	fPWSUserID = NULL;
	fUID = fEffectiveUID = 0xffffffff;  // this is -1 (nobody)
	
	if ( inConnection != NULL )
		fLDAPConnection = inConnection->Retain();
	else
		fLDAPConnection = NULL;
}

sLDAPContextData::~sLDAPContextData( void )
{
	DSDelete( fOpenRecordType );
	DSDelete( fOpenRecordName );
	DSDelete( fOpenRecordDN );
	
	if ( fPWSNodeRef != 0 ) {
		dsCloseDirNode( fPWSNodeRef );
		fPWSNodeRef = 0;
	}
	
	if ( fPWSRef != 0 ) {
		dsCloseDirService( fPWSRef );
		fPWSRef = 0;
	}
	
	DSFree( fPWSUserID );
	DSRelease( fLDAPConnection );
}

#pragma mark -
#pragma mark Class Definition

CLDAPConnectionManager::CLDAPConnectionManager( CLDAPv3Configs *inConfigObject ) : 
	fLDAPConnectionMapMutex( "CLDAPConnectionManager::fLDAPConnectionMapMutex" )
{
	CFTypeRef	methods[] = { CFSTR("GSSAPI"), CFSTR("CRAM-MD5") };
	
	fConfigObject = inConfigObject;
	fSupportedSASLMethods = CFArrayCreate( kCFAllocatorDefault, methods, sizeof(methods) / sizeof(CFTypeRef), &kCFTypeArrayCallBacks );
}

CLDAPConnectionManager::~CLDAPConnectionManager( void )
{
	// need to release any of our connections
	fLDAPConnectionMapMutex.WaitLock();
    
	LDAPConnectionMapI		aLDAPConnectionMapI;
	for ( aLDAPConnectionMapI = fLDAPConnectionMap.begin(); aLDAPConnectionMapI != fLDAPConnectionMap.end(); ++aLDAPConnectionMapI )
		aLDAPConnectionMapI->second->Release();
	
	fLDAPConnectionMap.clear();
	
	// here let's just copy into another list before we clear so we can close the connections safely
	LDAPAuthConnectionList	cleanupList( fLDAPAuthConnectionList );

	// now safe to clear the list
	fLDAPAuthConnectionList.clear();

	// release the lock so we can close our authed connections
	fLDAPConnectionMapMutex.SignalLock();	

	for ( LDAPAuthConnectionListI cleanupListI = cleanupList.begin(); cleanupListI != cleanupList.end(); ++cleanupListI )
	{
		(*cleanupListI)->Release();
	}
	
	DSCFRelease( fSupportedSASLMethods );
}

bool CLDAPConnectionManager::IsSASLMethodSupported( CFStringRef inMethod )
{
	return CFArrayContainsValue( fSupportedSASLMethods, CFRangeMake(0, CFArrayGetCount(fSupportedSASLMethods)), inMethod );
}

CLDAPConnection	*CLDAPConnectionManager::GetConnection( const char *inNodeName )
{
	CLDAPConnection	*pConnection = NULL;

	fLDAPConnectionMapMutex.WaitLock();
		
	// first check our existing connections
	LDAPConnectionMapI aLDAPConnectionMapI = fLDAPConnectionMap.find( inNodeName );
	if ( aLDAPConnectionMapI != fLDAPConnectionMap.end() )
	{
		int32_t connectionStatus = aLDAPConnectionMapI->second->ConnectionStatus();

		// if it's safe return it
		if ( connectionStatus == kConnectionSafe )
		{
			pConnection = aLDAPConnectionMapI->second->Retain();
			if ( pConnection->fNodeConfig != NULL )
			{
				if ( pConnection->fNodeConfig->fEnableUse == false )
				{
					DSRelease( pConnection );
				}
				else if ( pConnection->fNodeConfig->fConfigDeleted == true )
				{
					DSRelease( pConnection );
					fLDAPConnectionMap.erase( aLDAPConnectionMapI );
					aLDAPConnectionMapI = fLDAPConnectionMap.end();
				}
			}
		}
		// if it's unknown, try it
		else if ( connectionStatus == kConnectionUnknown )
		{
			// try to establish a connection, if it fails release it
			pConnection = aLDAPConnectionMapI->second->Retain();
			LDAP *pTemp = pConnection->LockLDAPSession();
			if ( pTemp != NULL )
				pConnection->UnlockLDAPSession( pTemp, false );
			else
				DSRelease( pConnection );			
		}
	}
	
	// ok, let's get a new one then if possible
	if ( aLDAPConnectionMapI == fLDAPConnectionMap.end() )
	{
		pConnection = fConfigObject->CreateConnectionForNode( inNodeName );
		if ( pConnection != NULL )
		{
			// we add to our map so we can track bad sessions too, if it's not ldapi
			if ( ldap_is_ldapi_url(inNodeName) == false )
				fLDAPConnectionMap[inNodeName] = pConnection->Retain();

			// now try to establish a connection, if it fails release it
			LDAP *pTemp = pConnection->LockLDAPSession();
			if ( pTemp != NULL )
				pConnection->UnlockLDAPSession( pTemp, false );
			else
				DSRelease( pConnection );			
		}
	}
	
	fLDAPConnectionMapMutex.SignalLock();

	return pConnection;
}

tDirStatus CLDAPConnectionManager::AuthConnection( CLDAPConnection **inConnection, const char *inLDAPUsername, const char *inRecordType, 
												   const char *inKerberosID, const char *inPassword )
{
	tDirStatus dsStatus = eDSAuthMasterUnreachable;
	bool isLDAPI = false;
	
	if ( (*inConnection) != NULL )
	{
		CLDAPConnection	*pConnection = (*inConnection)->CreateCopy();
		if ( pConnection != NULL )
		{
			pConnection->fWriteable = true;
			
			char *ipStr = pConnection->CopyReplicaIPAddress();
			if (ipStr != NULL ) {
				isLDAPI = (strcmp(ipStr, "ldapi") == 0);
				free( ipStr );
			}
			
			if ( isLDAPI )
			{
				dsStatus = eDSNoErr;
			}
			else
			{
				LDAP *pTempLD = pConnection->LockLDAPSession();
				if ( pTempLD != NULL )
				{
					pConnection->UnlockLDAPSession( pTempLD, false );

					dsStatus = pConnection->Authenticate( inLDAPUsername, inRecordType, inKerberosID, inPassword );
					if ( dsStatus == eDSNoErr )
					{
						(*inConnection)->Release();
						(*inConnection) = pConnection;
						
						fLDAPConnectionMapMutex.WaitLock();
						fLDAPAuthConnectionList.push_back( pConnection->Retain() );
						fLDAPConnectionMapMutex.SignalLock();
						
						pConnection = NULL;
					}
				}
			}
			
			DSRelease( pConnection );
		}
	}
	
	return dsStatus;
}

tDirStatus CLDAPConnectionManager::AuthConnectionKerberos( CLDAPConnection **inConnection, const char *inUsername, const char *inRecordType, 
														   krb5_creds *inCredsPtr, const char *inKerberosID )
{
	tDirStatus dsStatus = eDSAuthMasterUnreachable;
	
	if ( (*inConnection) != NULL )
	{
		CLDAPConnection *pConnection = (*inConnection)->CreateCopy();
		if ( pConnection != NULL )
		{
			pConnection->fWriteable = true;

			LDAP *pTempLD = pConnection->LockLDAPSession();
			if ( pTempLD != NULL )
			{
				pConnection->UnlockLDAPSession( pTempLD, false );

				if ( inCredsPtr != NULL )
				{
					dsStatus = pConnection->AuthenticateKerberos( inUsername, inRecordType, inCredsPtr, inKerberosID );
					if ( dsStatus == eDSNoErr )
					{
						(*inConnection)->Release();
						(*inConnection) = pConnection;
						
						fLDAPConnectionMapMutex.WaitLock();
						fLDAPAuthConnectionList.push_back( pConnection->Retain() );
						fLDAPConnectionMapMutex.SignalLock();
						
						pConnection = NULL;
					}
				}
				else
				{
					// we set status to eDSAuthParameterError, because no credentials were supplied, but we picked our replica
					dsStatus = eDSAuthParameterError;
				}
			}
			
			DSRelease( pConnection );
		}
	}
	
	return dsStatus;
}

tDirStatus CLDAPConnectionManager::VerifyCredentials( CLDAPConnection *inConnection, const char *inLDAPUsername, const char *inRecordType,
													  const char *inKerberosID, const char *inPassword )
{
	tDirStatus	siResult	= eDSAuthFailed;
	
	CLDAPConnection	*pConnection	= NULL;
	
	if ( inConnection != NULL )
	{
		pConnection = inConnection->CreateCopy();
		if ( pConnection != NULL )
		{
			siResult = pConnection->Authenticate( inLDAPUsername, inRecordType, inKerberosID, inPassword );
			DSRelease( pConnection );
		}
	}
	
	return siResult;
}

void CLDAPConnectionManager::NodeDeleted( const char *inNodeName )
{
	// if a node is deleted, just remove any existing known connections from our map, they will get
	// deleted when existing sessions fail
	CLDAPConnection *pConnection = NULL;
	
	fLDAPConnectionMapMutex.WaitLock();
	
	LDAPConnectionMapI aLDAPConnectionMapI = fLDAPConnectionMap.find( inNodeName );
	if ( aLDAPConnectionMapI != fLDAPConnectionMap.end() )
	{
		pConnection = aLDAPConnectionMapI->second;
		fLDAPConnectionMap.erase( aLDAPConnectionMapI );
	}

	fLDAPConnectionMapMutex.SignalLock();
	
	// do while not holding mutex due to potential deadlock with Kerberos
	DSRelease( pConnection );
}

void CLDAPConnectionManager::PeriodicTask( void )
{
	if ( gSystemGoingToSleep )
		return;
	
	bool				bShouldCheckThread		= false;
	LDAPConnectionMapI	aLDAPConnectionMapI;
	LDAPAuthConnectionList	cleanupList;
	
	CLDAPv3Plugin::WaitForNetworkTransitionToFinish();
	
	fLDAPConnectionMapMutex.WaitLock();
	
	for ( aLDAPConnectionMapI = fLDAPConnectionMap.begin(); aLDAPConnectionMapI != fLDAPConnectionMap.end();  )
	{
		CLDAPConnection *pConnection = aLDAPConnectionMapI->second;
		
		// see if it should be deleted first
		if ( pConnection->fNodeConfig->fConfigDeleted == true || 
			 (pConnection->RetainCount() == 1 && pConnection->ConnectionStatus() == kConnectionSafe) )
		{
			DbgLog( kLogPlugin, "CLDAPConnectionManager::PeriodicTask - Status Node: %s -- References: 0 -- removing from table", 
				    aLDAPConnectionMapI->first.c_str() );
			
			cleanupList.push_back( pConnection );
			fLDAPConnectionMap.erase( aLDAPConnectionMapI++ );
			continue;
		}
		else if ( pConnection->ConnectionStatus() != kConnectionSafe )
		{
			bShouldCheckThread = true;
		}
		
		pConnection->PeriodicTask();
		aLDAPConnectionMapI++;
	}
	
	// now check the authenticated connections
	LDAPAuthConnectionListI aLDAPAuthConnectionListI;
	for ( aLDAPAuthConnectionListI = fLDAPAuthConnectionList.begin(); aLDAPAuthConnectionListI != fLDAPAuthConnectionList.end(); )
	{
		CLDAPConnection *pConnection = (*aLDAPAuthConnectionListI);
		
		if ( pConnection->fNodeConfig->fConfigDeleted == true || 
			 (pConnection->RetainCount() == 1 && pConnection->ConnectionStatus() == kConnectionSafe) )
		{
			DbgLog( kLogPlugin, "CLDAPConnectionManager::PeriodicTask - Status Node: %s:%s -- References: 0 -- removing from table", 
				    pConnection->fNodeConfig->fNodeName, pConnection->fLDAPUsername );
			
			cleanupList.push_back( pConnection );
			aLDAPAuthConnectionListI++;
			fLDAPAuthConnectionList.remove( pConnection );
			continue;
		}
		else if ( pConnection->ConnectionStatus() != kConnectionSafe )
		{
			bShouldCheckThread = true;
		}
		
		pConnection->PeriodicTask();
		aLDAPAuthConnectionListI++;
	}
	
	fLDAPConnectionMapMutex.SignalLock();
	
	// now Release any we were planning on deleting while not holding the map mutex, due to a Kerberos deadlock potential
	for ( LDAPAuthConnectionListI cleanupListI = cleanupList.begin(); cleanupListI != cleanupList.end(); cleanupListI++ )
	{
		(*cleanupListI)->Release();
	}
	
	// check that there is actually at least one entry in the table that needs to be checked
	if ( bShouldCheckThread )
	{
		// while we are here, let's also kick off the thread for checking failed..
		LaunchCheckFailedThread( true );
	}
}

void CLDAPConnectionManager::NetworkTransition( void )
{
	if ( gSystemGoingToSleep )
		return;
	
	// call the network transition on the connection, let it decide if it wants to do anything
	fLDAPConnectionMapMutex.WaitLock();
	
	LDAPConnectionMapI	aLDAPConnectionMapI;
	for ( aLDAPConnectionMapI = fLDAPConnectionMap.begin(); aLDAPConnectionMapI != fLDAPConnectionMap.end(); ++aLDAPConnectionMapI )
		aLDAPConnectionMapI->second->NetworkTransition();
	
	LDAPAuthConnectionListI aLDAPAuthConnectionListI;
	for ( aLDAPAuthConnectionListI = fLDAPAuthConnectionList.begin(); aLDAPAuthConnectionListI != fLDAPAuthConnectionList.end(); 
		  aLDAPAuthConnectionListI++ )
	{
		(*aLDAPAuthConnectionListI)->NetworkTransition();
	}

	// while we are here, let's also kick off the thread for checking failed..
	LaunchCheckFailedThread( true );

	fLDAPConnectionMapMutex.SignalLock();
}

void CLDAPConnectionManager::SystemGoingToSleep( void )
{
	//set a network change blocking flag at sleep
	OSAtomicTestAndSetBarrier( 0, &gSystemGoingToSleep );
	
	fLDAPConnectionMapMutex.WaitLock();

	// flag all connections unsafe
	LDAPConnectionMapI	aLDAPConnectionMapI;
	for ( aLDAPConnectionMapI = fLDAPConnectionMap.begin(); aLDAPConnectionMapI != fLDAPConnectionMap.end(); ++aLDAPConnectionMapI )
	{
		aLDAPConnectionMapI->second->SetConnectionStatus( kConnectionUnsafe );
		aLDAPConnectionMapI->second->CloseConnectionIfPossible();
	}
	
	// need to flag authenticated ones too
	LDAPAuthConnectionListI aLDAPAuthConnectionListI;
	for ( aLDAPAuthConnectionListI = fLDAPAuthConnectionList.begin(); aLDAPAuthConnectionListI != fLDAPAuthConnectionList.end(); 
		 ++aLDAPAuthConnectionListI )
	{
		(*aLDAPAuthConnectionListI)->SetConnectionStatus( kConnectionUnsafe );
		(*aLDAPAuthConnectionListI)->CloseConnectionIfPossible();
	}
	
	fLDAPConnectionMapMutex.SignalLock();
}

void CLDAPConnectionManager::SystemWillPowerOn( void )
{
	// reset a network change blocking flag at wake
	DbgLog( kLogPlugin, "CLDAPConnectionManager::SystemWillPowerOn - clearing sleep flag" );
	OSAtomicTestAndClearBarrier( 0, &gSystemGoingToSleep );
}

void CLDAPConnectionManager::CheckFailed( void )
{
	if ( gSystemGoingToSleep )
		return;
	
	LDAPConnectionMap	aCheckConnections;
	LDAPAuthConnectionList	cleanupList;

	// we don't want to process failed connections right after a network transition, let the active connections go first
	CLDAPv3Plugin::WaitForNetworkTransitionToFinish();
	
	// let's copy all the failed connections to a new map table....
	fLDAPConnectionMapMutex.WaitLock();
    
	LDAPConnectionMapI	aLDAPConnectionMapI;
	for ( aLDAPConnectionMapI = fLDAPConnectionMap.begin(); aLDAPConnectionMapI != fLDAPConnectionMap.end(); )
	{
		CLDAPConnection *pConnection = aLDAPConnectionMapI->second;

		if ( pConnection->fNodeConfig->fConfigDeleted == true ||
			 (pConnection->RetainCount() == 1 && pConnection->ConnectionStatus() == kConnectionSafe) )
		{
			DbgLog( kLogPlugin, "CLDAPConnectionManager::CheckFailed - Status Node: %s -- References: 0 -- removing from table", 
				    aLDAPConnectionMapI->first.c_str() );
			
			cleanupList.push_back( pConnection );
			fLDAPConnectionMap.erase( aLDAPConnectionMapI++ );
			continue;
		}
		else if ( pConnection->ConnectionStatus() != kConnectionSafe  )
		{
			aCheckConnections[aLDAPConnectionMapI->first] = pConnection->Retain();
		}
		
		aLDAPConnectionMapI++;
	}
	
	// we check connections that have a retain count more than one since they will get removed next go around
	LDAPAuthConnectionListI aLDAPAuthConnectionListI;
	for ( aLDAPAuthConnectionListI = fLDAPAuthConnectionList.begin(); aLDAPAuthConnectionListI != fLDAPAuthConnectionList.end(); )
	{
		CLDAPConnection *pConnection = (*aLDAPAuthConnectionListI);
		
		if ( pConnection->fNodeConfig->fConfigDeleted == true || 
			 (pConnection->RetainCount() == 1 && pConnection->ConnectionStatus() == kConnectionSafe) )
		{
			DbgLog( kLogPlugin, "CLDAPConnectionManager::CheckFailed - Status Node: %s:%s -- References: 0 -- removing from table", 
				    pConnection->fNodeConfig->fNodeName, pConnection->fLDAPUsername );
			
			cleanupList.push_back( pConnection );
			aLDAPAuthConnectionListI++;
			fLDAPAuthConnectionList.remove( pConnection );
		}
		else if ( pConnection->ConnectionStatus() != kConnectionSafe )
		{
			aCheckConnections[string(pConnection->fNodeConfig->fNodeName)+string(":")+string(pConnection->fLDAPUsername)] = pConnection->Retain();
		}
		
		aLDAPAuthConnectionListI++;
	}
    
	fLDAPConnectionMapMutex.SignalLock();	
	
	// now Release any we were planning on deleting while not holding the map mutex, due to a Kerberos deadlock potential
	for ( LDAPAuthConnectionListI cleanupListI = cleanupList.begin(); cleanupListI != cleanupList.end(); cleanupListI++ )
	{
		(*cleanupListI)->Release();
	}
    
	CLDAPv3Plugin::WaitForNetworkTransitionToFinish();
	
	if ( aCheckConnections.empty() == false )
	{
		DbgLog( kLogPlugin, "CLDAPConnectionManager::CheckFailed - checking %d node connections", aCheckConnections.size() );
		
		for ( aLDAPConnectionMapI = aCheckConnections.begin(); aLDAPConnectionMapI != aCheckConnections.end(); ++aLDAPConnectionMapI )
		{
			CLDAPConnection *pConnection = aLDAPConnectionMapI->second;
			
			pConnection->CheckFailed();
			DSRelease( pConnection );
		}
	}
	
	fCheckFailedEvent.PostEvent();
}

void CLDAPConnectionManager::LaunchCheckFailedThread( bool bForceCheck )
{
	if ( (bForceCheck == true || (CFAbsoluteTimeGetCurrent() - fCheckFailedLastRun) > 30.0) && 
		 OSAtomicCompareAndSwap32Barrier(false, true, &fCheckThreadActive) == true )
	{
		pthread_t       checkThread;
		pthread_attr_t	defaultAttrs;
		
        fCheckFailedEvent.ResetEvent();
		fCheckThreadActive = true;
		
		pthread_attr_init( &defaultAttrs );
		pthread_attr_setdetachstate( &defaultAttrs, PTHREAD_CREATE_DETACHED );
		
		pthread_create( &checkThread, &defaultAttrs, CheckFailedServers, (void *) this );
	}
}

void *CLDAPConnectionManager::CheckFailedServers( void *data )
{
	CLDAPConnectionManager   *nodeMgr = (CLDAPConnectionManager *) data;
	
	nodeMgr->CheckFailed();
	
	fCheckFailedLastRun = CFAbsoluteTimeGetCurrent(); // we set the timestamp after we finished our last check
	OSAtomicCompareAndSwap32Barrier( true, false, &fCheckThreadActive );

	return NULL;
}
