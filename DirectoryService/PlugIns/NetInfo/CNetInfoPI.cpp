/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CNetInfoPI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>		// for syslog() to log calls

#include <netinfo/ni.h>
#include <netinfo/ni_util.h>
#include <netinfo/ni_prot.h>

#include "CNetInfoPI.h"
#include "ServerModuleLib.h"
#include "DSUtils.h"
#include "PluginData.h"
#include "CNiNodeList.h"
#include "CNiPlugIn.h"
#include "SharedConsts.h"
#include "CString.h"
#include "PrivateTypes.h"
#include "DSCThread.h"
#include "DSEventSemaphore.h"
#include "my_ni_pwdomain.h"


// --------------------------------------------------------------------------------
//	Globals

DSEventSemaphore		*gKickNodeRequests	= nil;
DSMutexSemaphore		*gNetInfoMutex		= nil;

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0x6D, 0xA7, 0x65, 0xAC, 0x87, 0xA1, 0x12, 0x26, \
								0x80, 0x37, 0x00, 0x05, 0x02, 0xC1, 0xC7, 0x36 );

}


static CDSServerModule* _Creator ( void )
{
	return( new CNetInfoPI );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

char		   *CNetInfoPI::fLocalDomainName	= nil;
CNiNodeList	   *CNetInfoPI::fNiNodeList			= nil;
uInt32			CNetInfoPI::fSignature			= 0;

static void DoNIPINetworkChange(CFRunLoopTimerRef timer, void *info);
void DoNIPINetworkChange(CFRunLoopTimerRef timer, void *info)
{
	if ( info != nil )
	{
		((CNetInfoPI *)info)->ReDiscoverNetwork();
	}
}// DoNIPINetworkChange


CFStringRef NetworkChangeNIPICopyStringCallback( const void *item );
CFStringRef NetworkChangeNIPICopyStringCallback( const void *item )
{
	return CFSTR("NetworkChangeinNIPI");
}

// --------------------------------------------------------------------------------
//	* CNetInfoPI ()
// --------------------------------------------------------------------------------

CNetInfoPI::CNetInfoPI ( void )
{
	fNiNodeList				= nil;
	fState					= kUnknown;
	fRegisterNodePtr		= nil;
	fServerRunLoop			= nil;
	fTransitionCheckTime	= 0;
	fSignature				= 0;

	if ( gKickNodeRequests == nil )
	{
		gKickNodeRequests = new DSEventSemaphore();
		if ( gKickNodeRequests == nil ) throw((sInt32)eMemoryAllocError);
	}

	if ( gNetInfoMutex == nil )
	{
		gNetInfoMutex = new DSMutexSemaphore();
		if ( gNetInfoMutex == nil ) throw((sInt32)eMemoryAllocError);
	}
} // CNetInfoPI


// --------------------------------------------------------------------------------
//	* ~CNetInfoPI ()
// --------------------------------------------------------------------------------

CNetInfoPI::~CNetInfoPI ( void )
{
} // ~CNetInfoPI


// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

sInt32 CNetInfoPI::Validate ( const char *inVersionStr, const uInt32 inSignature )
{
	fSignature = inSignature;

	return( eDSNoErr );

} // Validate


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

sInt32 CNetInfoPI::Initialize ( void )
{
	sInt32		siResult		= eDSNoErr;
	tDataList  *pDataList		= nil;
	tDataList  *aDataList		= nil;

	gNetInfoMutex->Wait();

	try
	{
		if ( fNiNodeList == nil )
		{
			fNiNodeList = new CNiNodeList();
		}

		if ( fNiNodeList == nil ) throw( (sInt32)eMemoryAllocError );

		//Define the local domain name to kstrDefaultLocalNodeName
		fLocalDomainName = (char *)::malloc( ::strlen( "." ) + 1 );
		::strcpy( fLocalDomainName, "." );

		//Register the default local node immediately
		pDataList = ::dsBuildFromPathPriv( kstrDefaultLocalNodeName, "/" );
		aDataList = ::dsBuildFromPathPriv( kstrDefaultLocalNodeName, "/" );
		if ( pDataList != nil )
		{
			//AddNode consumes the pDataList
			fNiNodeList->AddNode( ".", pDataList, false ); //will be registered but not treated the same as others

			pDataList = nil;
		}
		if ( aDataList != nil )
		{
			CShared::LogIt( 0x0F, "Registering node %s.", kstrDefaultLocalNodeName );

			DSRegisterNode( fSignature, aDataList, kLocalNodeType );
			::dsDataListDeallocatePriv( aDataList );
			free(aDataList);
			aDataList = nil;
		}

		fRegisterMutex.Wait();
		if (fRegisterNodePtr == nil)
		{
			fRegisterNodePtr = new CNodeRegister( fSignature, fNiNodeList, false, this );
			if ( fRegisterNodePtr != nil )
			{
				//this call could throw
				fRegisterNodePtr->StartThread();
			}
		}
		fRegisterMutex.Signal();

		// Turn off the failed flag and turn on active and inited flag
		fState &= ~kFailedToInti;
		fState += kActive;
		fState += ktInited;

		// Tell everyone that we are ready to go now
		CNetInfoPI::WakeUpRequests();
	}

	catch( sInt32 err )
	{
		fState += kFailedToInti;

		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // Initialize


// --------------------------------------------------------------------------------
//	* PeriodicTask ()
// --------------------------------------------------------------------------------

sInt32 CNetInfoPI::PeriodicTask ( void )
{

	return( eDSNoErr );

} // PeriodicTask


//--------------------------------------------------------------------------------------------------
//	* WakeUpRequests() (static)
//
//--------------------------------------------------------------------------------------------------

void CNetInfoPI::WakeUpRequests ( void )
{
	gKickNodeRequests->Signal();
} // WakeUpRequests


// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CNetInfoPI::WaitForInit ( void )
{
	volatile	uInt32		uiAttempts	= 0;

	while ( !(fState & ktInited) &&
			!(fState & kFailedToInti) )
	{
		try
		{
			// Try for 2 minutes before giving up
			if ( uiAttempts++ >= 240 )
			{
				return;
			}
			// Now wait until we are told that there is work to do or
			//	we wake up on our own and we will look for ourselves

			gKickNodeRequests->Wait( (uInt32)(.5 * kMilliSecsPerSec) );

//			try
//			{
//				gKickNodeRequests->Reset();
//			}
//
//			catch( sInt32 err )
//			{
//			}
		}

		catch( sInt32 err1 )
		{
		}

//		uiAttempts++;
	}
} // WaitForInit


// ---------------------------------------------------------------------------
//	* ProcessRequest
//
// ---------------------------------------------------------------------------

sInt32 CNetInfoPI::ProcessRequest ( void *inData )
{
	sInt32		siResult	= eDSNoErr;
	CNiPlugIn	cNiPlugin;
	char	   *pathStr		= nil;

	try
	{
		if ( inData == nil )
		{
			return( ePlugInDataError );
		}

		if (((sHeader *)inData)->fType == kOpenDirNode)
		{
			if (((sOpenDirNode *)inData)->fInDirNodeName != nil)
			{
				pathStr = ::dsGetPathFromListPriv( ((sOpenDirNode *)inData)->fInDirNodeName, "/" );
				if ( (pathStr != nil) && (strncmp(pathStr,"/NetInfo",8) != 0) )
				{
					throw( (sInt32)eDSOpenNodeFailed );
				}
			}
		}
		
		WaitForInit();

		if ( fState & kFailedToInti )
		{
			return( ePlugInFailedToInitialize );
		}

		if ( !(fState & kActive) )
		{
			return( ePlugInNotActive );
		}

		if ( ((sHeader *)inData)->fType == kHandleNetworkTransition )
		{
			HandleMultipleNetworkTransitions();
		}
		else if ( ((sHeader *)inData)->fType == kServerRunLoop )
		{
			if ( (((sHeader *)inData)->fContextData) != nil )
			{
				fServerRunLoop = (CFRunLoopRef)(((sHeader *)inData)->fContextData);
			}
		}
		else
		{
			siResult = cNiPlugin.HandleRequest( inData );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if (pathStr != nil)
	{
		free(pathStr);
		pathStr = nil;
	}
	return( siResult );

} // ProcessRequest


//------------------------------------------------------------------------------------
//	* HandleMultipleNetworkTransitions
//------------------------------------------------------------------------------------

void CNetInfoPI::HandleMultipleNetworkTransitions ( void )
{
	void	   *ptInfo		= nil;
	
	//let us be smart about doing the recheck
	//we would like to wait a short period for NetInfo to come back fully
	//we also don't want to re-init multiple times during this wait period
	//however we do go ahead and fire off timers each time
	//each call in here we update the delay time by 5 seconds
	fTransitionCheckTime = time(nil) + 5;

	if (fServerRunLoop != nil)
	{
		ptInfo = (void *)this;
		CFRunLoopTimerContext c = {0, (void*)ptInfo, NULL, NULL, NetworkChangeNIPICopyStringCallback};
	
		CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	NULL,
														CFAbsoluteTimeGetCurrent() + 5,
														0,
														0,
														0,
														DoNIPINetworkChange,
														(CFRunLoopTimerContext*)&c);
	
		CFRunLoopAddTimer(fServerRunLoop, timer, kCFRunLoopDefaultMode);
		if (timer) CFRelease(timer);
	}
} // HandleMultipleNetworkTransitions

//------------------------------------------------------------------------------------
//	* ReDiscoverNetwork
//------------------------------------------------------------------------------------

void CNetInfoPI::ReDiscoverNetwork(void)
{
	sInt32	siResult	= eDSNoErr;
	
	//do something if the wait period has passed
	if (time(nil) >= fTransitionCheckTime)
	{
		if ( fNiNodeList != nil )
		{
			fRegisterMutex.Wait();
			
			if (fRegisterNodePtr == nil)
			{
				fRegisterNodePtr = new CNodeRegister( fSignature, fNiNodeList, true, this );
				if ( fRegisterNodePtr != nil )
				{
					try
					{
						//set all the node flags to dirty
						fNiNodeList->SetAllDirty();
			
						//this call could throw
						fRegisterNodePtr->StartThread();
					}
				
					catch( sInt32 err )
					{
						siResult = err;
					}
				}
			}
			else
			{
				//set all the node flags to dirty
				//fNiNodeList->SetAllDirty(); // this is taken care of inside CNodeRegister
				fRegisterNodePtr->Restart();
			}
			fRegisterMutex.Signal();
		}
	}
} // ReDiscoverNetwork


// ---------------------------------------------------------------------------
//	* SafeOpen
//
// ---------------------------------------------------------------------------

sInt32 CNetInfoPI::SafeOpen ( const char   *inName,
							 sInt32			inTimeoutSecs,
							 ni_id		   *outNiDirID,
							 void		  **outDomain,
							 char		  **outDomainName )
{
	sInt32		siResult		= eDSNoErr;
	ni_status	niStatus		= NI_OK;
	void	   *domain			= nil;
	char	   *domName			= nil;
	ni_id		domainID;
	uInt32		localOrParent	= 0;
	time_t		delayedNI		= 0;
	tDataList  *pDataList		= nil;

	try
	{
		if ( fNiNodeList->IsOpen( inName, outDomain, outDomainName, outNiDirID ) == false )
		{
			localOrParent = fNiNodeList->CheckForLocalOrParent( inName );
			gNetInfoMutex->Wait();

			delayedNI = time(nil) + 2; //normally ni_open will complete in under 2 secs
			if (localOrParent == 1)
			{
				niStatus = ::ni_open( nil, ".", &domain );
				if ( delayedNI < time(nil) )
				{
					syslog(LOG_INFO,"SafeOpen::Call to ni_open was with fixed domain name: . and lasted %d seconds.", (uInt32)(2 + time(nil) - delayedNI));
				}
			}
			else if (localOrParent == 2)
			{
				niStatus = ::ni_open( nil, "..", &domain );
				if ( delayedNI < time(nil) )
				{
					syslog(LOG_INFO,"SafeOpen::Call to ni_open was with fixed domain name: .. and lasted %d seconds.", (uInt32)(2 + time(nil) - delayedNI));
				}
			}
			else
			{
				niStatus = ::ni_open( nil, inName, &domain );
				if ( delayedNI < time(nil) )
				{
					syslog(LOG_INFO,"SafeOpen::Call to ni_open was with argument domain name: %s and lasted %d seconds.", inName, (uInt32)(2 + time(nil) - delayedNI));
				}
			}
			if ( niStatus != NI_OK )
			{
				throw( (sInt32)eDSOpenNodeFailed );
			}

			// Ping the domain to see if it's really alive
			::ni_setreadtimeout( domain, inTimeoutSecs );
			::ni_setabort( domain, 1 );

			NI_INIT( &domainID );
			domainID.nii_object = 0;
			niStatus = ::ni_self( domain, &domainID );
			if ( niStatus != NI_OK )
			{
				//check for nil and free if not
				if (domain != nil)
				{
					::ni_free( domain );
				}
				throw( (sInt32)eDSOpenNodeFailed );
			}

			gNetInfoMutex->Signal();

			if ( ::strcmp( inName, "/" ) == 0 )
			{
				domName = (char *)::calloc( ::strlen( "/NetInfo/root" ) + 1, sizeof( char ) );
				::strcpy( domName, "/NetInfo/root" );
			}
			else if ( ::strcmp( inName, "." ) == 0 )
			{
				domName = strdup( kstrDefaultLocalNodeName );
			}
			else if ( ::strcmp( inName, ".." ) == 0 )
			{
				char	* niDomName	= nil;
				
				gNetInfoMutex->Wait();
				delayedNI = time(nil) + 2; //normally my_ni_pwdomain will complete in under 2 secs
				niStatus = ::my_ni_pwdomain( domain, &niDomName );
				if ( delayedNI < time(nil) )
				{
					syslog(LOG_INFO,"SafeOpen::Call to my_ni_pwdomain was with argument domain name: %s and lasted %d seconds.", inName, (uInt32)(2 + time(nil) - delayedNI));
				}
				gNetInfoMutex->Signal();
				
				if ( niStatus == NI_OK )
				{
					if ( ::strcmp( niDomName, "/" ) == 0 )
					{
						domName = (char *)::calloc( ::strlen( "/NetInfo/root" ) + 1, sizeof( char ) );
						::strcpy( domName, "/NetInfo/root" );
					}
					else
					{
						domName = (char *)::calloc( ::strlen( niDomName ) + ::strlen( "/NetInfo/root" ) + 1, sizeof( char ) );
						::strcpy( domName, "/NetInfo/root" );
						::strcat( domName, niDomName );
					}
					if (niDomName != nil)
					{
						free(niDomName);
						niDomName = nil;
					}
				}
				else
				{
					domName = (char *)::calloc( ::strlen( "Unknown Locations" ) + 1, sizeof( char ) );
					::strcpy( domName, "Unknown Locations" );
				}
			}
			else
			{
				domName = (char *)::calloc( ::strlen( "/NetInfo/root" ) + ::strlen( inName ) + 1, sizeof( char ) );

				::strcpy( domName, "/NetInfo/root" );
				::strcat( domName, inName );
			}

			fNiNodeList->Lock();
			if ( fNiNodeList->IsOpen( inName, outDomain, outDomainName, outNiDirID ) == false )
			{
				//we are forcing the SetDomainInfo code to always create a node via AddNode
				//this gets around the refCount problem
				fNiNodeList->SetDomainInfo( inName, domain, domName, &domainID );
				fNiNodeList->UnLock();
				
				//we also register the node here since it was not already found in our node list
				pDataList = ::dsBuildFromPathPriv( domName, "/" );
				if ( pDataList != nil )
				{
					if (DSRegisterNode( fSignature, pDataList, kDirNodeType ) == eDSNoErr)
					{
						CShared::LogIt( 0x0F, "CNetInfoPI::SafeOpen: Registering node that was not already registered %s.", domName );
					}
					::dsDataListDeallocatePriv( pDataList );
					free( pDataList );
					pDataList = nil;
				}

				*outDomain		= domain;
				*outDomainName	= strdup(domName);
				::memcpy( outNiDirID, &domainID, sizeof( ni_id ) );
			}
			else
			{
				fNiNodeList->UnLock();
				if (domain != nil)
				{
					gNetInfoMutex->Wait();
					ni_free(domain);
					gNetInfoMutex->Signal();
					domain = nil;
				}
				if (domName != nil)
				{
					free(domName);
					domName = nil;
				}
				
			}
		}
	}

	catch( sInt32 err )
	{
		gNetInfoMutex->Signal();
		siResult = err;
	}

	catch ( ... )
	{
		gNetInfoMutex->Signal();
		siResult = -12020;
	}

	return( siResult );

} // SafeOpen


// ---------------------------------------------------------------------------
//	* SafeClose
//
// ---------------------------------------------------------------------------

sInt32 CNetInfoPI::SafeClose ( const char   *inName )
{
	sInt32		siResult	= eDSNoErr;
	void	   *domain		= nil;

	gNetInfoMutex->Wait();
	
	domain = fNiNodeList->DeleteNode( inName );
	if (domain != nil)
	{
		ni_free(domain);
		domain = nil;
	}
	
	gNetInfoMutex->Signal();
	
	return( siResult );

} // SafeClose


// ---------------------------------------------------------------------------
//	* UnregisterNode
//
// ---------------------------------------------------------------------------

sInt32 CNetInfoPI::UnregisterNode ( const uInt32 inToken, tDataList *inNode )
{
	sInt32	siResult	= eDSNoErr;

	siResult = DSUnregisterNode( inToken, inNode );
	
	return (siResult);

} // UnregisterNode

// ---------------------------------------------------------------------------
//	* NodeRegisterComplete
//
// ---------------------------------------------------------------------------

void CNetInfoPI::NodeRegisterComplete ( CNodeRegister *aRegisterThread )
{
	fRegisterMutex.Wait();
	if (fRegisterNodePtr == aRegisterThread)
	{
		fRegisterNodePtr = nil;
	}
	fRegisterMutex.Signal();
} // NodeRegisterComplete

