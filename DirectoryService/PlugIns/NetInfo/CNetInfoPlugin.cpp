/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

/*!
 * @header CNetInfoPlugin
 */

#include <stdlib.h>
#include <string.h>
#include <syslog.h>		// for syslog() to log calls
#include <unistd.h>
#include <notify.h>

#include <netinfo/ni.h>
#include <netinfo/ni_util.h>
#include <netinfo/ni_prot.h>

#include "CNetInfoPlugin.h"
#include "DSUtils.h"
#include "CNiNodeList.h"
#include "CNiPlugIn.h"
#include "SharedConsts.h"
#include "CString.h"
#include "DSCThread.h"
#include "DSEventSemaphore.h"
#include "my_ni_pwdomain.h"
#include "CSharedData.h"
#include "CLog.h"
#include "NiLib2.h"


//PFIXextern	dsBool	gLocalNodeNotAvailable;

/*
 * PRIVATE!
 * notifiation center SPI
 */
#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t notify_get_state(int token, int *state);
extern uint32_t notify_register_plain(const char *name, int *out_token);
#ifdef __cplusplus
}
#endif

#define NETINFO_BINDING_KEY "com.apple.system.netinfo.local.binding_change"

#define BINDING_STATE_UNBOUND 0
#define BINDING_STATE_BOUND 1
#define BINDING_STATE_NETROOT 2

// --------------------------------------------------------------------------------
//	Globals

DSEventSemaphore		*gKickNodeRequests	= nil;
DSMutexSemaphore		*gNetInfoMutex		= nil;

CNiNodeList	   *CNetInfoPlugin::fNiNodeList			= nil;
FourCharCode	CNetInfoPlugin::fToken				= 0;

static void DoNIPINetworkChange(CFRunLoopTimerRef timer, void *info);
void DoNIPINetworkChange(CFRunLoopTimerRef timer, void *info)
{
	if ( info != nil )
	{
		((CNetInfoPlugin *)info)->ReDiscoverNetwork();
	}
}// DoNIPINetworkChange


CFStringRef NetworkChangeNIPICopyStringCallback( const void *item );
CFStringRef NetworkChangeNIPICopyStringCallback( const void *item )
{
	return CFSTR("NetworkChangeinNetInfoModule");
}

// --------------------------------------------------------------------------------
//	* CNetInfoPlugin ()
// --------------------------------------------------------------------------------

CNetInfoPlugin::CNetInfoPlugin ( FourCharCode inSig, const char *inName ) : CServerPlugin(inSig, inName)
{
	fNiNodeList				= nil;
	fState					= kUnknownState;
	fRegisterNodePtr		= nil;
	fServerRunLoop			= nil;  //could be obtained directly now since a module not plugin
	fTransitionCheckTime	= 0;

	//need the signature for some static functions
	CNetInfoPlugin::fToken = inSig;
	
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
} // CNetInfoPlugin


// --------------------------------------------------------------------------------
//	* ~CNetInfoPlugin ()
// --------------------------------------------------------------------------------

CNetInfoPlugin::~CNetInfoPlugin ( void )
{
} // ~CNetInfoPlugin


// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

sInt32 CNetInfoPlugin::Validate ( const char *inVersionStr, const uInt32 inSignature )
{
	CNetInfoPlugin::fToken = fPlugInSignature = inSignature;

	return( eDSNoErr );

} // Validate


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

sInt32 CNetInfoPlugin::Initialize ( void )
{
	sInt32		siResult		= eDSOpenNodeFailed;
	tDataList  *pDataList		= nil;
	tDataList  *aDataList		= nil;

	gNetInfoMutex->Wait();

	try
	{
		{
			void			   *domain			= nil;
			int					tokcheck		= 0;
			int					tokplain		= 0;
			int					status			= eDSNoErr;
			int					binding			= 0;
			int					retcheck		= 0;
			int					numRetries		= 0;
			DSSemaphore			aCheckNIMutex;
                        
			DBGLOG( kLogPlugin, "CNetInfoPlugin::Initialize - kick start netinfod initiated." );
			//kick start netinfod here to ensure netinfod starts early upon boot/reboot?
			//status = (int)do_open( nil, "local", &domain, true, 1, NULL, NULL );
			status = (int)do_open( nil, "127.0.0.1/local", &domain, true, 1, NULL, NULL );
			if (status == NI_OK)
			{
				DBGLOG( kLogPlugin, "CNetInfoPlugin::Initialize - do_open on tag succeeded." );
			}
			else
			{
				DBGLOG( kLogPlugin, "CNetInfoPlugin::Initialize - do_open on tag failed." );
			}
			
			DBGLOG( kLogPlugin, "CNetInfoPlugin::Initialize - netinfo notification check attempted." );
			status = notify_register_check(NETINFO_BINDING_KEY, &tokcheck);
			if (status == NOTIFY_STATUS_OK)
			{
				status = notify_check(tokcheck, &retcheck);
				int	checkCount = 0;
				while ( (status == NOTIFY_STATUS_OK) && (retcheck == 0) && (checkCount < 3) )
				{
					checkCount++;
					aCheckNIMutex.Wait( 1 * kMilliSecsPerSec );
					status = notify_check(tokcheck, &retcheck);
				}
				if (retcheck == 1)
				{
					DBGLOG( kLogPlugin, "CNetInfoPlugin::Initialize - netinfo notification check succeeded." );
				}
				if (tokcheck != 0)
				{
					notify_cancel(tokcheck);
					tokcheck = 0;
				}
			}
			
			DBGLOG( kLogPlugin, "CNetInfoPlugin::Initialize - netinfo binding discovery attempted." );
			status = notify_register_plain(NETINFO_BINDING_KEY, &tokplain);
			if (status != NOTIFY_STATUS_OK)
			{
				DBGLOG1( kLogPlugin, "CNetInfoPlugin::Initialize - notify_register_plain failed for netinfo binding (status %d).", status );
				syslog(LOG_INFO,"CNetInfoPlugin::Initialize - notify_register_plain failed for netinfo binding (status %d)\n", status);
			}
			else
			{
				binding = BINDING_STATE_UNBOUND;
	
				DBGLOG1( kLogPlugin, "CNetInfoPlugin::Initialize - notify_register_plain succeeded for netinfo binding (status %d).", status );
				while (binding == BINDING_STATE_UNBOUND)
				{
					status = notify_get_state(tokplain, &binding);
					if (status != NOTIFY_STATUS_OK)
					{
						DBGLOG1( kLogPlugin, "CNetInfoPlugin::Initialize - notify_get_state failed for netinfo binding (status %d).", status );
						syslog(LOG_INFO,"CNetInfoPlugin::Initialize - notify_get_state failed for netinfo binding (status %d)\n", status);
						status = -1;
						break;
					}
			
					if (binding != BINDING_STATE_UNBOUND)
					{
						status = binding;
						DBGLOG1( kLogPlugin, "CNetInfoPlugin::Initialize - netinfo binding bound after %d queries on netinfod status.", numRetries );
						break;
					}
					
					numRetries++;
					if (numRetries > 10)
					{
						status = -1;
						DBGLOG1( kLogPlugin, "CNetInfoPlugin::Initialize - notify_get_state failed over last minute (status %d).", status );
						syslog(LOG_INFO,"CNetInfoPlugin::Initialize - notify_get_state failed over last minute (status %d)\n", status);
						break;
					}
			
					aCheckNIMutex.Wait( 2 * kMilliSecsPerSec );
					if ((numRetries % 5) == 0)
					{
						notify_cancel(tokplain);
						tokplain = 0;
						DBGLOG2( kLogPlugin, "CNetInfoPlugin::Initialize - retry attempt %d on notify_register_plain for netinfo binding (status %d).", numRetries, status );
						status = notify_register_plain(NETINFO_BINDING_KEY, &tokplain);
						if (status != NOTIFY_STATUS_OK)
						{
							DBGLOG2( kLogPlugin, "CNetInfoPlugin::Initialize - retry %d on notify_register_plain failed for netinfo binding (status %d)\n", numRetries, status );
							syslog(LOG_INFO,"CNetInfoPlugin::Initialize - retry %d on notify_register_plain failed for netinfo binding (status %d)\n", numRetries, status);
							status = -1;
							break;
						}
					}
				}
	
				if (tokplain != 0)
				{
					notify_cancel(tokplain);
				}
			
				switch (status)
				{
					case BINDING_STATE_BOUND:
					case BINDING_STATE_NETROOT:
						siResult = eDSNoErr;
						break;
					default:
						siResult = eDSOpenNodeFailed;
						break;
				}
			}
			if (domain != nil)
			{
				ni_free(domain);
				domain = nil;
			}
		}

		if (siResult != eDSNoErr)
		{
			throw(siResult);
		}

		if ( fNiNodeList == nil )
		{
			fNiNodeList = new CNiNodeList();
		}

		if ( fNiNodeList == nil ) throw( (sInt32)eMemoryAllocError );

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
			DBGLOG1( kLogPlugin, "Registering node %s.", kstrDefaultLocalNodeName );

			CServerPlugin::_RegisterNode( fPlugInSignature, aDataList, kLocalNodeType );
			::dsDataListDeallocatePriv( aDataList );
			free(aDataList);
			aDataList = nil;
			
			//need to set global that local node will be available shortly
//PFIX			gLocalNodeNotAvailable = false;
			
		}

		fRegisterMutex.Wait();
		if (fRegisterNodePtr == nil)
		{
			fRegisterNodePtr = new CNodeRegister( fPlugInSignature, fNiNodeList, false, this );
			if ( fRegisterNodePtr != nil )
			{
				//this call could throw
				fRegisterNodePtr->StartThread();
			}
		}
		fRegisterMutex.Signal();

		// Turn off the failed flag and turn on active and inited flag
		fState &= ~kFailedToInit;
		fState += kActive;
		fState += kInitialized;

		// Tell everyone that we are ready to go now
		CNetInfoPlugin::WakeUpRequests();
	}

	catch( sInt32 err )
	{
		fState += kFailedToInit;

		siResult = err;
	}

	gNetInfoMutex->Signal();

	return( siResult );

} // Initialize


// --------------------------------------------------------------------------------
//	* PeriodicTask ()
// --------------------------------------------------------------------------------

sInt32 CNetInfoPlugin::PeriodicTask ( void )
{
	return( eDSNoErr );
} // PeriodicTask


//--------------------------------------------------------------------------------------------------
//	* WakeUpRequests() (static)
//
//--------------------------------------------------------------------------------------------------

void CNetInfoPlugin::WakeUpRequests ( void )
{
	gKickNodeRequests->Signal();
} // WakeUpRequests


// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CNetInfoPlugin::WaitForInit ( void )
{
	volatile	uInt32		uiAttempts	= 0;

	while ( !(fState & kInitialized) &&
			!(fState & kFailedToInit) )
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

sInt32 CNetInfoPlugin::ProcessRequest ( void *inData )
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
		
		if ( ((sHeader *)inData)->fType == kServerRunLoop )
		{
			if ( (((sHeader *)inData)->fContextData) != nil )
			{
				fServerRunLoop = (CFRunLoopRef)(((sHeader *)inData)->fContextData);
				return (siResult);
			}
		}

		WaitForInit();

		if (fState == kUnknownState)
		{
			throw( (sInt32)ePlugInCallTimedOut );
		}

        if ( (fState & kFailedToInit) || !(fState & kInitialized) )
        {
            throw( (sInt32)ePlugInFailedToInitialize );
        }

        if ( (fState & kInactive) || !(fState & kActive) )
        {
            throw( (sInt32)ePlugInNotActive );
        }

		if ( ((sHeader *)inData)->fType == kHandleNetworkTransition )
		{
			HandleMultipleNetworkTransitions();
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

void CNetInfoPlugin::HandleMultipleNetworkTransitions ( void )
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

void CNetInfoPlugin::ReDiscoverNetwork(void)
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
				fRegisterNodePtr = new CNodeRegister( fPlugInSignature, fNiNodeList, true, this );
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

sInt32 CNetInfoPlugin::SafeOpen ( const char   *inName,
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
					if (CServerPlugin::_RegisterNode( CNetInfoPlugin::fToken, pDataList, kDirNodeType ) == eDSNoErr)
					{
						DBGLOG1( kLogPlugin, "CNetInfoPlugin::SafeOpen: Registering node that was not already registered %s.", domName );
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

sInt32 CNetInfoPlugin::SafeClose ( const char   *inName )
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

sInt32 CNetInfoPlugin::UnregisterNode ( const uInt32 inToken, tDataList *inNode )
{
	sInt32	siResult	= eDSNoErr;

	siResult = CServerPlugin::_UnregisterNode( inToken, inNode );
	
	return (siResult);

} // UnregisterNode

// ---------------------------------------------------------------------------
//	* NodeRegisterComplete
//
// ---------------------------------------------------------------------------

void CNetInfoPlugin::NodeRegisterComplete ( CNodeRegister *aRegisterThread )
{
	fRegisterMutex.Wait();
	if (fRegisterNodePtr == aRegisterThread)
	{
		fRegisterNodePtr = nil;
	}
	fRegisterMutex.Signal();
} // NodeRegisterComplete


// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

sInt32 CNetInfoPlugin::SetPluginState ( const uInt32 inState )
{
//does nothing yet
	return( eDSNoErr );
} // SetPluginState


