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

/*!
 * @header CSearchPlugin
 * Implements the search policies.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>			//used for mkdir and stat
#include <syslog.h>
#include <mach/mach_time.h>	// for dsTimeStamp
#include <libkern/OSAtomic.h>

#include <Security/Authorization.h>

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"
#include "DirServicesPriv.h"

#include "SharedConsts.h"
#include "CSharedData.h"
#include "PrivateTypes.h"
#include "DSUtils.h"
#include "CAttributeList.h"
#include "CPlugInRef.h"
#include "CDataBuff.h"
#include "CRecTypeList.h"
#include "ServerControl.h"
#include "CLog.h"
#include "CAttributeList.h"

#include "CSearchPlugin.h"
#include "ServerModuleLib.h"
#include "DSUtils.h"
#include "COSUtils.h"
#include "PluginData.h"
#include "DSCThread.h"
#include "DSEventSemaphore.h"
#include "CContinue.h"
#include "GetMACAddress.h"

extern	bool			gServerOS;
extern	CCachePlugin	*gCacheNode;

uint32_t				gSystemGoingToSleep			= 0;
int32_t CSearchPlugin::fAuthCheckNodeThreadActive		= false;
int32_t CSearchPlugin::fContactCheckNodeThreadActive	= false;
DSEventSemaphore	CSearchPlugin::fAuthPolicyChangeEvent;
DSEventSemaphore	CSearchPlugin::fContactPolicyChangeEvent;
bool					gDHCPLDAPEnabled	= false;
int32_t					gInitializeActive	= false;

// Globals ---------------------------------------------------------------------------

#define		kDS1AttrDHCPLDAPDefault		"dsAttrTypeStandard:DHCPLDAPDefault"

static CPlugInRef		 	*gSNNodeRef			= nil;
static CContinue		 	*gSNContinue			= nil;
DSEventSemaphore			gKickSearchRequests;
static DSEventSemaphore		gNetworkTransition;
static CSearchPlugin		*gSearchNode			= nil;

static void DHCPChangeNotification( SCDynamicStoreRef aSCDStore, CFArrayRef changedKeys, void *inInfo )
{                       
	if ( inInfo != NULL && !gSystemGoingToSleep && OSAtomicCompareAndSwap32Barrier(false, true, &gInitializeActive) == true ) //if going to sleep do not do this
	{
		CSearchPluginHandlerThread* aSearchPluginHandlerThread = new CSearchPluginHandlerThread( DSCThread::kTSSearchPlugInHndlrThread, 2, inInfo );
		if (aSearchPluginHandlerThread != NULL)
			aSearchPluginHandlerThread->StartThread();
		//we don't keep a handle to the search plugin handler threads and don't check if they get created
	}
}// DHCPChangeNotification

// returns -1 = deregister, 0 = no change, 1 = register.
int ShouldRegisterWorkstation(void)
{
	int result = 0;
	
	if ( ( gSearchNode != nil ) && !gSystemGoingToSleep ) //if going to sleep do not do this
		result = gSearchNode->fRegisterWorkstation ? 1 : -1;
	
	return result;
}

#pragma mark -
#pragma mark Specific Search Plugin Handler Routines for Run Loop spawned jobs
#pragma mark -

//--------------------------------------------------------------------------------------------------
//	* CSearchPluginHandlerThread()
//
//--------------------------------------------------------------------------------------------------

CSearchPluginHandlerThread::CSearchPluginHandlerThread ( void ) : CInternalDispatchThread(kTSSearchPlugInHndlrThread)
{
	fThreadSignature	= kTSSearchPlugInHndlrThread;
	fWhichFunction		= 0;
	fNeededClass		= nil;
} // CSearchPluginHandlerThread


//--------------------------------------------------------------------------------------------------
//	* CSearchPluginHandlerThread(const FourCharCode inThreadSignature)
//
//--------------------------------------------------------------------------------------------------

CSearchPluginHandlerThread::CSearchPluginHandlerThread ( const FourCharCode inThreadSignature, int inWhichFunction, void *inNeededClass ) : CInternalDispatchThread(inThreadSignature)
{
	fThreadSignature	= inThreadSignature;
	fWhichFunction		= inWhichFunction;
	fNeededClass		= inNeededClass;
} // CSearchPluginHandlerThread ( FourCharCode inThreadSignature, int inWhichFunction, void *inNeededClass )

//--------------------------------------------------------------------------------------------------
//	* ~CSearchPluginHandlerThread()
//
//--------------------------------------------------------------------------------------------------

CSearchPluginHandlerThread::~CSearchPluginHandlerThread()
{
} // ~CSearchPluginHandlerThread

//--------------------------------------------------------------------------------------------------
//	* StartThread()
//
//--------------------------------------------------------------------------------------------------

void CSearchPluginHandlerThread::StartThread ( void )
{
	if ( this == nil ) throw((SInt32)eMemoryError);

	this->Resume();
} // StartThread


//--------------------------------------------------------------------------------------------------
//	* LastChance()
//
//--------------------------------------------------------------------------------------------------

void CSearchPluginHandlerThread:: LastChance ( void )
{
	//nothing to do here
} // LastChance


//--------------------------------------------------------------------------------------------------
//	* StopThread()
//
//--------------------------------------------------------------------------------------------------

void CSearchPluginHandlerThread::StopThread ( void )
{
	SetThreadRunState( kThreadStop );		// Tell our thread to stop

} // StopThread


//--------------------------------------------------------------------------------------------------
//	* ThreadMain()
//
//--------------------------------------------------------------------------------------------------

SInt32 CSearchPluginHandlerThread::ThreadMain ( void )
{
	CSearchPlugin *aSearchPlugin = (CSearchPlugin *) fNeededClass;

	if ( aSearchPlugin != nil )
	{
		switch ( fWhichFunction )
		{
			case 1:
				// check network node reachability for nodes on the authentication search path
				aSearchPlugin->CheckNodes( eDSAuthenticationSearchNodeName, &CSearchPlugin::fAuthCheckNodeThreadActive, &CSearchPlugin::fAuthPolicyChangeEvent );
				break;
			case 2:
				// someone wants to reinitialize the search config
				aSearchPlugin->Initialize();
				break;
			case 3:
				aSearchPlugin->CheckNodes( eDSContactsSearchNodeName, &CSearchPlugin::fContactCheckNodeThreadActive, &CSearchPlugin::fContactPolicyChangeEvent );
				break;
			default:
				break;
		}
	}
	
	//not really needed
	StopThread();
	
	return( 0 );

} // ThreadMain


#pragma mark -
#pragma mark Search Plugin
#pragma mark -

// --------------------------------------------------------------------------------
//	* CSearchPlugin ()
// --------------------------------------------------------------------------------

CSearchPlugin::CSearchPlugin ( FourCharCode inSig, const char *inName ) : CServerPlugin(inSig, inName), 
	fMutex("CSearchPlugin::fMutex")
{
	fDirRef					= 0;
	fState					= kUnknownState;
	pSearchConfigList		= nil;
	fAuthSearchPathCheck	= nil;
	fSomeNodeFailedToOpen	= false;
		
	if ( gSNNodeRef == nil )
	{
		if (gServerOS)
			gSNNodeRef = new CPlugInRef( CSearchPlugin::ContextDeallocProc, 1024 );
		else
			gSNNodeRef = new CPlugInRef( CSearchPlugin::ContextDeallocProc, 256 );
	}

	if ( gSNContinue == nil )
	{
		if (gServerOS)
			gSNContinue = new CContinue( CSearchPlugin::ContinueDeallocProc, 256 );
		else
			gSNContinue = new CContinue( CSearchPlugin::ContinueDeallocProc, 64 );
	}
	
	// let's register our notification of DHCP LDAP changes and Location changes
	CFMutableArrayRef		notifyKeys	= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	SCDynamicStoreContext	scContext	= { 0, this, NULL, NULL, NULL };
	CFStringRef				locationKey = SCDynamicStoreKeyCreateLocation( kCFAllocatorDefault );

	// now look for notification of location changes so we can re-initialize based on location
	if ( locationKey != NULL )
		CFArrayAppendValue( notifyKeys, locationKey );

	// also look for DHCP options available
	CFArrayAppendValue( notifyKeys, CFSTR(kDSStdNotifyDHCPOptionsAvailable) );

	// now create our store and setup notifications
	SCDynamicStoreRef store = SCDynamicStoreCreate( kCFAllocatorDefault, CFSTR("CSearchPlugin::Initialize"), DHCPChangeNotification,
												    &scContext );
	if ( store != NULL )
	{
		SCDynamicStoreSetNotificationKeys( store, notifyKeys, NULL );
		CFRunLoopSourceRef rls = SCDynamicStoreCreateRunLoopSource( kCFAllocatorDefault, store, 0 );
		if (rls != NULL)
		{
			// this source will not block as it will spawn a thread to do the work
			CFRunLoopAddSource( CFRunLoopGetMain(), rls, kCFRunLoopDefaultMode );
			CFRelease( rls );
		}
	}
	
	DSCFRelease( locationKey );
	DSCFRelease( notifyKeys );
	DSCFRelease( store );
	
	gSearchNode = this;
	::dsOpenDirService( &fDirRef ); //don't check the return since we are direct dispatch inside the daemon
	
	fRegisterWorkstation = false;
	
#if AUGMENT_RECORDS
	fAugmentNodeRef = 0;
#endif

} // CSearchPlugin


// --------------------------------------------------------------------------------
//	* ~CSearchPlugin ()
// --------------------------------------------------------------------------------

CSearchPlugin::~CSearchPlugin ( void )
{
	sSearchConfig  	   *pConfig			= nil;
	sSearchConfig  	   *pDeleteConfig	= nil;

	//need to cleanup the struct list ie. the internals
	pConfig = pSearchConfigList;
	while (pConfig != nil)
	{
		pDeleteConfig = pConfig;
		pConfig = pConfig->fNext;		//assign to next BEFORE deleting current
		CleanSearchConfigData( pDeleteConfig );
		free( pDeleteConfig );
		pDeleteConfig = nil;
	}
	pSearchConfigList = nil;

	if (fDirRef != 0)
	{
		::dsCloseDirService( fDirRef );
	}
} // ~CSearchPlugin


// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

SInt32 CSearchPlugin::Validate ( const char *inVersionStr, const UInt32 inSignature )
{
	fPlugInSignature = inSignature;

	return( eDSNoErr );
} // Validate


// --------------------------------------------------------------------------------
//	* PeriodicTask ()
// --------------------------------------------------------------------------------

SInt32 CSearchPlugin::PeriodicTask ( void )
{
	return( eDSNoErr );
} // PeriodicTask

// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

SInt32 CSearchPlugin::Initialize ( void )
{
	SInt32			siResult				= eDSNoErr;
	SInt32			result					= eDSNoErr;
	SInt32			addLDAPResult			= eSearchPathNotDefined;
	tDataList	   *aNodeName				= nil;
	sSearchConfig  *aSearchConfig			= nil;
	sSearchConfig  *lastSearchConfig		= nil;
	UInt32			index					= 0;
	UInt32			aSearchConfigType		= 0;
	char		   *aSearchNodeName			= nil;
	char		   *aSearchConfigFilePrefix	= nil;
	sSearchList	   *aSearchNodeList			= nil;
	sSearchList	   *autoSearchNodeList		= nil;
	CConfigs	   *aConfigFromXML			= nil;
	UInt32			aSearchPolicy			= 0;
	eDirNodeType	aDirNodeType			= kUnknownNodeType;
	char		   *authSearchPathCheck		= nil;
	bool			bShouldNotify			= fSomeNodeFailedToOpen;
	bool			bRegisterWorkstation	= false;
	
	try
	{
		//verify the dirRef here and only open a new one if required
		//can't believe we ever need a new one since we are direct dispatch inside the daemon
		siResult = ::dsVerifyDirRefNum(fDirRef);
		if (siResult != eDSNoErr)
		{
			// Get a directory services reference as a member variable
			siResult = ::dsOpenDirService( &fDirRef );
			if ( siResult != eDSNoErr ) throw( siResult );
		}

		//KW here we create the multiple search configs that CAN be used by this search node ie. three of them now
		//--might in the future determine exactly how these get initialized but for now they are hard coded
		//--to three separate configs:one for Auth, one for Contacts, and one for Default Network
		//note that the first two are setup the same way and the Default Network one is separate outside of the for loop
		//the rest of the code can easily deal with any number of configs
		for (index = 0; index < 2; index++)
		{
			switch( index )
			{
				case 0:
					DbgLog( kLogPlugin, "Setting Authentication Search Node Configuraton" );
					aSearchConfigType	= eDSAuthenticationSearchNodeName;
					aDirNodeType		= kSearchNodeType;
					break;
					
				case 1:
					DbgLog( kLogPlugin, "Setting Contacts Search Node Configuraton" );
					aSearchConfigType 	= eDSContactsSearchNodeName;
					aDirNodeType		= kContactsSearchNodeType;
					break;
			}
			
			fMutex.WaitLock();
			aSearchConfig = FindSearchConfigWithKey(aSearchConfigType);
			if (aSearchConfig != nil)  //checking if we are simply re-entrying intialize
			//so don't want to ignore what is already set-up but do want to possibly switch the search policy
			{
				aConfigFromXML 			= aSearchConfig->pConfigFromXML;
				aSearchNodeName			= aSearchConfig->fSearchNodeName;
				aSearchConfigFilePrefix	= aSearchConfig->fSearchConfigFilePrefix;
			}
			else
			{
				if (index == 0)
				{
					aSearchNodeName = (char *) calloc(sizeof(kstrAuthenticationNodeName) + 1, sizeof(char));
					if ( aSearchNodeName != NULL )
						strcpy(aSearchNodeName, kstrAuthenticationNodeName);
					else
						siResult = eMemoryError;
					
					aSearchConfigFilePrefix = (char *) calloc(sizeof(kstrAuthenticationConfigFilePrefix) + 1, sizeof(char));
					if ( aSearchConfigFilePrefix != NULL )
						strcpy(aSearchConfigFilePrefix, kstrAuthenticationConfigFilePrefix);
					else
						siResult = eMemoryError;
				}
				else
				{
					aSearchNodeName = (char *) calloc(sizeof(kstrContactsNodeName) + 1, sizeof(char));
					if ( aSearchNodeName != NULL )
						strcpy(aSearchNodeName, kstrContactsNodeName);
					else
						siResult = eMemoryError;
					
					aSearchConfigFilePrefix = (char *) calloc(sizeof(kstrContactsConfigFilePrefix) + 1, sizeof(char));
					if ( aSearchConfigFilePrefix != NULL )
						strcpy(aSearchConfigFilePrefix, kstrContactsConfigFilePrefix);
					else
						siResult = eMemoryError;
				}
			}
			fMutex.SignalLock();
			
			//this is where the XML config file comes from
			if ( aConfigFromXML == nil )
			{
				aConfigFromXML = new CConfigs();
				if ( aConfigFromXML != nil )
				{
					result = aConfigFromXML->Init( aSearchConfigFilePrefix, aSearchPolicy );
					if ( result != eDSNoErr ) //use default if error
					{
						aSearchPolicy = kAutomaticSearchPolicy; //automatic is the default
					}
				}
				else
				{
					aSearchPolicy = kAutomaticSearchPolicy; //automatic is the default
				}
			}
			else if (aSearchConfig != nil) //retain the same search policy for re-entry
			{
				aSearchPolicy = aSearchConfig->fSearchNodePolicy;
			}
		
			switch ( aSearchPolicy )
			{
				case kCustomSearchPolicy:
					DbgLog( kLogPlugin, "Setting search policy to Custom search" );
					aSearchNodeList = aConfigFromXML->GetCustom();
					if ( aSearchNodeList != NULL )
						bRegisterWorkstation = true;
					
					//if custom list was nil we go ahead anyways with local only
					//local policy nodes always added in regardless
					siResult = AddLocalNodesAsFirstPaths(&aSearchNodeList);
					break;

				case kLocalSearchPolicy:
					DbgLog( kLogPlugin, "Setting search policy to Local search" );
					//local policy call
					siResult = AddLocalNodesAsFirstPaths(&aSearchNodeList);
					break;

				case kAutomaticSearchPolicy:
				default:
					DbgLog( kLogPlugin, "Setting search policy to Automatic search" );
					siResult = AddLocalNodesAsFirstPaths(&aSearchNodeList);
					autoSearchNodeList = DupSearchListWithNewRefs(aSearchNodeList);
					break;
			} // switch on aSearchPolicy
			
			if (siResult == eDSNoErr)
			{
				if (aSearchPolicy == kAutomaticSearchPolicy)
				{
					bool bStateChanged = false;
					
					//get the default LDAP search paths if they are present
					//don't check status on return as continuing on anyways
					//don't add on to the custom path
					if ( aConfigFromXML->IsDHCPLDAPEnabled() )
					{
						// lets tell the LDAP plugin it is looking for DHCP LDAP
						bStateChanged = (gDHCPLDAPEnabled == false);
						gDHCPLDAPEnabled = true;
						bRegisterWorkstation = true;
					}
					else
					{
						bStateChanged = (gDHCPLDAPEnabled == true);
						gDHCPLDAPEnabled = false;
					}

					// this checks the flag, so it will skip if necessary
					addLDAPResult = AddDefaultLDAPNodesLast( &aSearchNodeList );

					if (bStateChanged)
					{
						DbgLog( kLogPlugin, "CSearchPlugin::Initialize DHCP LDAP setting changed to <%s>", (gDHCPLDAPEnabled ? "on" : "off") );

						SCDynamicStoreRef store = SCDynamicStoreCreate( kCFAllocatorDefault, CFSTR("DirectoryService"), NULL, NULL );
						if (store != NULL)
						{
							SCDynamicStoreNotifyValue( store, CFSTR(kDSStdNotifyDHCPConfigStateChanged) );
							DbgLog( kLogPlugin, "CSearchPlugin::Initialize DHCP LDAP sent notification" );
							CFRelease( store );
						}
					}
				}
				
				if (aSearchConfig != nil) //clean up the old search list due to re-entry and add in the new
				{
					sSearchList *toCleanSearchNodeList = nil;
					fMutex.WaitLock();
					toCleanSearchNodeList = aSearchConfig->fSearchNodeList;
					aSearchConfig->fSearchNodeList		= aSearchNodeList;
					aSearchConfig->fSearchNodePolicy	= aSearchPolicy;
					if (aSearchConfigType == eDSAuthenticationSearchNodeName)
					{
						sSearchList *aNewListPtr = nil;
						sSearchList *anOldListPtr = nil;
						//let us retain the fNodeReachability setting here for now - heuristics can be added later
						aNewListPtr = aSearchNodeList->fNext; //always skip the local node
						aNewListPtr = aNewListPtr->fNext; //always skip the BSD node
						while (aNewListPtr != nil)
						{
							anOldListPtr = toCleanSearchNodeList->fNext; //always skip the local node
							anOldListPtr = anOldListPtr->fNext; //always skip the BSD node
							while (anOldListPtr != nil)
							{
								if ( (anOldListPtr->fNodeName != nil) && (aNewListPtr->fNodeName != nil) && 
									 strcmp(anOldListPtr->fNodeName, aNewListPtr->fNodeName) == 0) //same node
								{
									DBGLOG2( kLogPlugin, "CSearchPlugin::Initialize: reinit - reachability of node <%s> retained as <%s>", 
											 aNewListPtr->fNodeName, anOldListPtr->fNodeReachable ? "true" : "false");
									aNewListPtr->fNodeReachable = anOldListPtr->fNodeReachable;
									aNewListPtr->fHasNeverOpened = anOldListPtr->fHasNeverOpened;
									break;
								}
								anOldListPtr = anOldListPtr->fNext;
							}
							aNewListPtr = aNewListPtr->fNext;
						}
					}
					else
					{
						sSearchList *aListPtr = nil;
						aListPtr = aSearchNodeList->fNext; //always skip the local node
						aListPtr = aListPtr->fNext; //always skip the BSD node
						while (aListPtr != nil)
						{
							aListPtr->fNodeReachable = false;
							aListPtr->fHasNeverOpened = true;
							aListPtr = aListPtr->fNext;
						}
					}
					fMutex.SignalLock();
					//flush the old search path list outside of the mutex
					CleanSearchListData( toCleanSearchNodeList );
				}
				else
				{
					//now get this search config
					aSearchConfig	= MakeSearchConfigData(	aSearchNodeList,
															aSearchPolicy,
															aConfigFromXML,
															aSearchNodeName,
															aSearchConfigFilePrefix,
															aDirNodeType,
															aSearchConfigType);
					//now put aSearchConfig in the list
					AddSearchConfigToList(aSearchConfig);
				}
	
				//set the indicator file
				if (addLDAPResult == eSearchPathNotDefined)
				{
					SetSearchPolicyIndicatorFile(aSearchConfigType, aSearchPolicy);
				}
				else //DHCP LDAP nodes added so make sure indicator file shows a custom policy
				{
					SetSearchPolicyIndicatorFile(aSearchConfigType, kCustomSearchPolicy);
				}
				addLDAPResult = eSearchPathNotDefined;
	
				EnsureCheckNodesThreadIsRunning( (tDirPatternMatch) aSearchConfigType );

				aSearchNodeList			= nil;
				aSearchPolicy			= 0;
				aConfigFromXML			= nil;
				aSearchNodeName			= nil;
				aSearchConfigFilePrefix	= nil;
				lastSearchConfig		= aSearchConfig;
				aSearchConfig			= nil;
				
				// make search node active
				fState = kUnknownState;
				fState += kInitialized;
				fState += kActive;
		
				gSNNodeRef->DoOnAllItems(CSearchPlugin::ContextSetListChangedProc);
			}

		} //for loop over search node indices
		
		//clean up the cached auto search list if it exists
		if ( (autoSearchNodeList != nil) && (lastSearchConfig != nil) )
		{
			CleanSearchListData( autoSearchNodeList );
		}

		
		{  //Default Network Search Policy
			DbgLog( kLogPlugin, "Setting Default Network Search Node Configuraton" );
			aSearchConfigType	= eDSNetworkSearchNodeName;
			aDirNodeType		= kNetworkSearchNodeType;
			aSearchPolicy		= kCustomSearchPolicy;
			
			fMutex.WaitLock();
			aSearchConfig = FindSearchConfigWithKey(aSearchConfigType);
			if (aSearchConfig != nil)  //checking if we are simply re-entrying intialize
			//so don't want to ignore what is already set-up
			{
				aConfigFromXML 			= aSearchConfig->pConfigFromXML;
				aSearchNodeName			= aSearchConfig->fSearchNodeName;
				aSearchConfigFilePrefix	= aSearchConfig->fSearchConfigFilePrefix;  //should be NULL
			}
			else
			{
				aSearchNodeName = (char *) calloc( 1, sizeof(kstrNetworkNodeName) + 1 );
				if ( aSearchNodeName != NULL )
					strcpy(aSearchNodeName, kstrNetworkNodeName);
				aSearchConfigFilePrefix = NULL;
				//this is where the XML config file comes from but is unused by this search node
				//however, we need the class for functions within it
				if ( aConfigFromXML == nil )
				{
					aConfigFromXML = new CConfigs();
					//if aConfigFromXML is nil then it is checked for later and not used
				}
			}
			fMutex.SignalLock();
			
			//register any default network nodes if any can be determined here
			//siResult = DoDefaultNetworkNodes(&aSearchNodeList);??
			//likely that since they are built automatically that there will be none added here
			//so the search policy list for now will be nil
			//the DS daemon knows the list and the search node can get the list when it needs it
			//ie. lazily get it when a call comes in needing it ie. when the Default Network node is actually opened
			
			if (aSearchConfig != nil) //clean up the old search list due to re-entry and add in the new
			{
				sSearchList *toCleanSearchNodeList = nil;
				fMutex.WaitLock();
				toCleanSearchNodeList = aSearchConfig->fSearchNodeList;
				aSearchConfig->fSearchNodeList		= aSearchNodeList;
				aSearchConfig->fSearchNodePolicy	= aSearchPolicy;
				fMutex.SignalLock();
				//flush the old search path list outside of the mutex
				CleanSearchListData( toCleanSearchNodeList );
			}
			else
			{
				//now get this search config
				aSearchConfig	= MakeSearchConfigData(	aSearchNodeList,
														aSearchPolicy,
														aConfigFromXML,
														aSearchNodeName,
														aSearchConfigFilePrefix,
														aDirNodeType,
														aSearchConfigType);
				//now put aSearchConfig in the list
				AddSearchConfigToList(aSearchConfig);
			}
		} //Default Network Search Policy end
		
	}

	catch( SInt32 err )
	{
		siResult = err;
		fState = kUnknownState;
		fState += kFailedToInit;
	}

	fMutex.WaitLock();
	aSearchConfig = pSearchConfigList;
	while (aSearchConfig != nil) //register all the search nodes that were successfully created
	{
		aNodeName = ::dsBuildFromPathPriv( aSearchConfig->fSearchNodeName, "/" );
		if ( aNodeName != nil )
		{
			CServerPlugin::_RegisterNode( fPlugInSignature, aNodeName, aSearchConfig->fDirNodeType );

			::dsDataListDeallocatePriv( aNodeName );
			free( aNodeName );
			aNodeName = nil;
			
			//build a checksum or string to determine the authentication search policy actually changed
			if ( strcmp(kstrAuthenticationNodeName, aSearchConfig->fSearchNodeName) == 0 )
			{
				CDataBuff	   *aTmpData	= nil;
				sSearchList	   *pListPtr	= nil;
				char		   *p			= nil;
				aTmpData = new CDataBuff();
				if ( aTmpData != nil )
				{
					pListPtr = aSearchConfig->fSearchNodeList;
					while ( pListPtr != nil )
					{
						p = pListPtr->fNodeName;
						if (p != nil)
						{
							aTmpData->AppendString( p );
						}
						pListPtr = pListPtr->fNext;
					}
					authSearchPathCheck = (char *) calloc(1, (aTmpData->GetLength()) + 1);
					memcpy(authSearchPathCheck, aTmpData->GetData(), aTmpData->GetLength());
					delete(aTmpData);
					aTmpData = nil;
				}
			}
		}
		aSearchConfig = aSearchConfig->fNext;
	}
	
	fRegisterWorkstation = bRegisterWorkstation;
	
	//check for search policy change
	if (fAuthSearchPathCheck == nil)
	{
		//on startup let's not change anything?
		//KW does this impact automounter or other clients at startup?
		bShouldNotify = false;
	}
	else if (authSearchPathCheck != nil)
	{
		if (strcmp(fAuthSearchPathCheck, authSearchPathCheck) != 0)
		{
			bShouldNotify = true;
		}
	}
	DSFreeString(fAuthSearchPathCheck);
	fAuthSearchPathCheck = authSearchPathCheck;
	if (bShouldNotify)
	{
		gSrvrCntl->NodeSearchPolicyChanged();
	}
	fSomeNodeFailedToOpen = false;
	fMutex.SignalLock();
	
	OSAtomicCompareAndSwap32Barrier( true, false, &gInitializeActive );
	
	return( siResult );

} // Initialize


// --------------------------------------------------------------------------------
//	* SwitchSearchPolicy ()
// --------------------------------------------------------------------------------

bool CSearchPlugin:: SwitchSearchPolicy ( UInt32 inSearchPolicy, sSearchConfig *inSearchConfig )
{
	SInt32			siResult				= eDSNoErr;
	bool			bAuthSwitched			= false; //only relevant with true if auth search policy changed
	SInt32			addLDAPResult			= eSearchPathNotDefined;
	char		   *authSearchPathCheck		= nil;
	bool			bRegisterWorkstation	= false;

	fMutex.WaitLock();
	
	try
	{
			//this is where the XML config file comes from
			if ( inSearchConfig->pConfigFromXML == nil )
			{
				inSearchConfig->pConfigFromXML = new CConfigs();
				if ( inSearchConfig->pConfigFromXML  == nil ) throw( (SInt32)eDSPlugInConfigFileError );
				//don't use the search policy from the XML config file
				//however, need to init with the file
				siResult = inSearchConfig->pConfigFromXML->Init( inSearchConfig->fSearchConfigFilePrefix, inSearchConfig->fSearchNodePolicy );
				if ( siResult != eDSNoErr ) throw( siResult );
				//KW need to update this file
				//siResult = pConfigFromXML->SetSearchPolicy(inSearchPolicy);
				//if ( siResult != eDSNoErr ) throw( siResult );
			}

			//switch the search policy here
			inSearchConfig->fSearchNodePolicy = inSearchPolicy;
		
			//since switching need to remove the old and
			//need to cleanup the struct list ie. the internals
			sSearchList *toCleanSearchNodeList = nil;
			toCleanSearchNodeList = inSearchConfig->fSearchNodeList;
			inSearchConfig->fSearchNodeList = nil;

			switch ( inSearchConfig->fSearchNodePolicy )
			{
				case kCustomSearchPolicy:
					DbgLog( kLogPlugin, "Setting search policy to Custom search" );
					inSearchConfig->fSearchNodeList = inSearchConfig->pConfigFromXML->GetCustom();
					if ( inSearchConfig->fSearchNodeList != NULL )
						bRegisterWorkstation = true;
						
					//if custom list was nil we go ahead anyways with local only
					//local policy nodes always added in regardless
					siResult = AddLocalNodesAsFirstPaths(&(inSearchConfig->fSearchNodeList));
					break;

				case kLocalSearchPolicy:
					DbgLog( kLogPlugin, "Setting search policy to Local search" );
					//local policy call
					siResult = AddLocalNodesAsFirstPaths(&(inSearchConfig->fSearchNodeList));
					break;

				case kAutomaticSearchPolicy:
				default:
					DbgLog( kLogPlugin, "Setting search policy to Automatic default" );
					siResult = AddLocalNodesAsFirstPaths(&(inSearchConfig->fSearchNodeList));
					break;
			} // select the search policy

			if (siResult == eDSNoErr)
			{
				if (inSearchConfig->fSearchNodePolicy == kAutomaticSearchPolicy)
				{
					//get the default LDAP search paths if they are present
					//don't check status on return as continuing on anyways
					//don't add on to the custom path
					if ( inSearchConfig->pConfigFromXML != nil )
					{
						bool bStateChanged = false;
						
						if (inSearchConfig->pConfigFromXML->IsDHCPLDAPEnabled())
						{
							bStateChanged = (gDHCPLDAPEnabled == false);
							gDHCPLDAPEnabled = true;
							bRegisterWorkstation = true;
						}
						else
						{
							bStateChanged = (gDHCPLDAPEnabled == true);
							gDHCPLDAPEnabled = false;
						}

						if (bStateChanged)
						{
							DbgLog( kLogPlugin, "CSearchPlugin::SwitchSearchPolicy DHCP LDAP setting changed to <%s>", 
								    (gDHCPLDAPEnabled ? "on" : "off") );
							
							SCDynamicStoreRef store = SCDynamicStoreCreate( NULL, CFSTR("DirectoryService"), NULL, NULL );
							if (store != NULL)
							{   // we don't have to change it we can just cause a notify....
								SCDynamicStoreNotifyValue( store, CFSTR(kDSStdNotifyDHCPConfigStateChanged) );
								DSCFRelease( store );
							}
						}
						
						addLDAPResult = AddDefaultLDAPNodesLast( &(inSearchConfig->fSearchNodeList) );
					}
				}
				
				if (inSearchConfig->fSearchConfigKey == eDSAuthenticationSearchNodeName)
				{
					sSearchList *aNewListPtr = nil;
					sSearchList *anOldListPtr = nil;
					//let us retain the fNodeReachability setting here for now - heuristics can be added later
					aNewListPtr = inSearchConfig->fSearchNodeList->fNext; //always skip the local node
					aNewListPtr = aNewListPtr->fNext; //always skip the BSD node
					while (aNewListPtr != nil)
					{
						anOldListPtr = toCleanSearchNodeList->fNext; //always skip the local node
						anOldListPtr = anOldListPtr->fNext; //always skip the BSD node
						while (anOldListPtr != nil)
						{
							if ( (anOldListPtr->fNodeName != nil) && (aNewListPtr->fNodeName != nil) && 
								 strcmp(anOldListPtr->fNodeName, aNewListPtr->fNodeName) == 0) //same node
							{
								DBGLOG2( kLogPlugin, "CSearchPlugin::SwitchSearchPolicy: switch - reachability of node <%s> retained as <%s>",
										 aNewListPtr->fNodeName, anOldListPtr->fNodeReachable ? "true" : "false");
								aNewListPtr->fNodeReachable = anOldListPtr->fNodeReachable;
								aNewListPtr->fHasNeverOpened = anOldListPtr->fHasNeverOpened;
								break;
							}
							anOldListPtr = anOldListPtr->fNext;
						}
						aNewListPtr = aNewListPtr->fNext;
					}
				}
				else
				{
					sSearchList *aListPtr = nil;
					aListPtr = inSearchConfig->fSearchNodeList->fNext; //always skip the local node
					aListPtr = aListPtr->fNext;
					while (aListPtr != nil)
					{
						aListPtr->fNodeReachable = false;
						aListPtr->fHasNeverOpened = true;
						aListPtr = aListPtr->fNext;
					}
				}
				
				EnsureCheckNodesThreadIsRunning( (tDirPatternMatch) inSearchConfig->fSearchConfigKey );
				
				// make search node active
				fState = kUnknownState;
				fState += kInitialized;
				fState += kActive;
		
				//set the indicator file
				if (addLDAPResult == eSearchPathNotDefined)
				{
					SetSearchPolicyIndicatorFile( inSearchConfig->fSearchConfigKey, inSearchConfig->fSearchNodePolicy );
				}
				else //DHCP LDAP nodes added so make sure indicator file shows a custom policy
				{
					SetSearchPolicyIndicatorFile( inSearchConfig->fSearchConfigKey, kCustomSearchPolicy );
				}

				//let all the context node references know about the switch
				gSNNodeRef->DoOnAllItems(CSearchPlugin::ContextSetListChangedProc);
			}

			CleanSearchListData( toCleanSearchNodeList );
			
	} // try

	catch( SInt32 err )
	{
		siResult = err;
		fState = kUnknownState;
		fState += kInactive;
	}

	fRegisterWorkstation = bRegisterWorkstation;
	
	//build a checksum or string to determine the authentication search policy actually changed
	if ( strcmp(kstrAuthenticationNodeName, inSearchConfig->fSearchNodeName) == 0 )
	{
		CDataBuff	   *aTmpData	= nil;
		sSearchList	   *pListPtr	= nil;
		char		   *p			= nil;
		aTmpData = new CDataBuff();
		if ( aTmpData != nil )
		{
			pListPtr = inSearchConfig->fSearchNodeList;
			while ( pListPtr != nil )
			{
				p = pListPtr->fNodeName;
				if (p != nil)
				{
					aTmpData->AppendString( p );
				}
				pListPtr = pListPtr->fNext;
			}
			authSearchPathCheck = (char *) calloc(1, (aTmpData->GetLength()) + 1);
			memcpy(authSearchPathCheck, aTmpData->GetData(), aTmpData->GetLength());
			delete(aTmpData);
			aTmpData = nil;
		}
	}
	//update the auth search path check value
	if (authSearchPathCheck != nil)
	{
		if (fAuthSearchPathCheck != nil)
		{
			free(fAuthSearchPathCheck);
		}
		fAuthSearchPathCheck = authSearchPathCheck;

		gSrvrCntl->NodeSearchPolicyChanged();
		
		//we should flush our cache because our policy changed
		if ( gCacheNode != NULL )
		{
			gCacheNode->EmptyCacheEntryType( CACHE_ENTRY_TYPE_ALL );
		}

		//not really checking if changed
		bAuthSwitched = true;
	}
	
	fMutex.SignalLock();

	return( bAuthSwitched );

} // SwitchSearchPolicy


// --------------------------------------------------------------------------------
//	* GetDefaultLocalPath ()
// --------------------------------------------------------------------------------

sSearchList *CSearchPlugin:: GetDefaultLocalPath( void )
{
	UInt32					uiCntr				= 1;
	sSearchList			   *outSrchList			= nil;


	outSrchList = (sSearchList *)calloc( sizeof( sSearchList ), sizeof(char) );
	outSrchList->fNodeName = strdup(kstrDefaultLocalNodeName);

	outSrchList->fDataList = ::dsBuildFromPathPriv( kstrDefaultLocalNodeName, "/" );
	DbgLog( kLogPlugin, "CSearchPlugin::GetDefaultLocalPath<setlocalfirst>:Search policy node %l = %s", uiCntr++, outSrchList->fNodeName );

	return( outSrchList );

} // GetDefaultLocalPath


// --------------------------------------------------------------------------------
//	* GetBSDLocalPath ()
// --------------------------------------------------------------------------------

sSearchList *CSearchPlugin:: GetBSDLocalPath( void )
{
	UInt32					uiCntr				= 2;
	sSearchList			   *outSrchList			= nil;

	outSrchList = (sSearchList *)calloc( sizeof( sSearchList ), sizeof(char) );
	outSrchList->fNodeName = strdup( kstrBSDLocalNodeName );

	outSrchList->fDataList = dsBuildFromPathPriv( kstrBSDLocalNodeName, "/" );
	DbgLog( kLogPlugin, "CSearchPlugin::GetBSDLocalPath<setbsdsecond>:Search policy node %l = %s", uiCntr++, outSrchList->fNodeName );

	return( outSrchList );

} // GetBSDLocalPath

// --------------------------------------------------------------------------------
//	* AddDefaultLDAPNodesLast ()
// --------------------------------------------------------------------------------

SInt32 CSearchPlugin::AddDefaultLDAPNodesLast( sSearchList **inSearchNodeList )
{
	SInt32				siResult		= eSearchPathNotDefined;
	sSearchList		   *ldapSrchList	= nil;
	sSearchList		   *pSrchList		= nil;

	if ( gDHCPLDAPEnabled )
	{
		ldapSrchList = GetDefaultLDAPPaths();
		if ( ldapSrchList != nil )
		{
			if ( *inSearchNodeList == nil )
			{
				*inSearchNodeList = ldapSrchList;
			}
			else
			{
				// Add this search data list to the end
				pSrchList = *inSearchNodeList;
				while(pSrchList->fNext != nil)
				{
					pSrchList = pSrchList->fNext;
				}
				pSrchList->fNext = ldapSrchList;
			}
			
			siResult = eDSNoErr;
		}
	}

	return( siResult );

} // AddDefaultLDAPNodesLast


// --------------------------------------------------------------------------------
//	* AddLocalNodesAsFirstPaths ()
// --------------------------------------------------------------------------------

SInt32 CSearchPlugin::AddLocalNodesAsFirstPaths( sSearchList **inSearchNodeList )
{
	SInt32				siResult		= eDSNoErr;
	sSearchList		   *aSrchList		= nil;
	sSearchList		   *pSrchList		= nil;
	char			   *localNodeName	= nil;

	//first prepend with the BSD flat file node
	localNodeName = strdup( kstrBSDLocalNodeName );
	aSrchList = GetBSDLocalPath();
	free(localNodeName);
	if ( aSrchList  == nil )
	{
		siResult = eSearchPathNotDefined;
	}
	else
	{
		if ( *inSearchNodeList == nil )
		{
			*inSearchNodeList = aSrchList;
		}
		else
		{
			// Add this search data list to the start of the list
			pSrchList = aSrchList;
			while (pSrchList->fNext != nil)
			{
				pSrchList = pSrchList->fNext;
			}
			pSrchList->fNext = *inSearchNodeList;
			*inSearchNodeList = aSrchList;
		}
	}

	//now put the true local node first
	localNodeName = strdup( kstrDefaultLocalNodeName );
	aSrchList = GetDefaultLocalPath();
	free(localNodeName);
	if ( aSrchList  == nil )
	{
		siResult = eSearchPathNotDefined;
	}
	else
	{
		if ( *inSearchNodeList == nil )
		{
			*inSearchNodeList = aSrchList;
		}
		else
		{
			// Add this search data list to the start of the list
			pSrchList = aSrchList;
			while (pSrchList->fNext != nil)
			{
				pSrchList = pSrchList->fNext;
			}
			pSrchList->fNext = *inSearchNodeList;
			*inSearchNodeList = aSrchList;
		}
	}

	return( siResult );

} // AddLocalNodesAsFirstPaths


// --------------------------------------------------------------------------------
//	* GetDefaultLDAPPaths ()
// --------------------------------------------------------------------------------

sSearchList *CSearchPlugin:: GetDefaultLDAPPaths ( void )
{
	SInt32					siResult			= eDSNoErr;
	tDataBuffer			   *pNodeBuff 			= nil;
	tDataList			   *pNodeNameDL			= nil;
	tDataList			   *pNodeList			= nil;
	UInt32					uiCount				= 0;
	sSearchList			   *pCurList			= nil;
	sSearchList	 		   *pSrchList			= nil;
	UInt32					uiCntr				= 1;
	sSearchList			   *outSrchList			= nil;
	tDirNodeReference  	   	aNodeRef			= 0;
	tAttributeListRef		attrListRef			= 0;
	tAttributeValueListRef	attrValueListRef	= 0;
	tAttributeValueEntry   *pAttrValueEntry		= nil;
	tAttributeEntry		   *pAttrEntry			= nil;
	UInt32					aIndex				= 0;

//open the /LDAPv3 node and then call in to get the default LDAP server names
//use a call to dsGetDirNodeInfo

	try
	{
		pNodeBuff = ::dsDataBufferAllocate( fDirRef, 2048 );
		if ( pNodeBuff == nil ) throw( (SInt32)eMemoryError );
		
		pNodeNameDL = ::dsBuildListFromStringsPriv( "LDAPv3", nil );
		if ( pNodeNameDL == nil ) throw( (SInt32)eMemoryAllocError );

		//open the LDAPv3 node
		siResult = ::dsOpenDirNode( fDirRef, pNodeNameDL, &aNodeRef );
		if ( siResult != eDSNoErr ) throw( siResult );

		::dsDataListDeallocate( fDirRef, pNodeNameDL );
		free(pNodeNameDL);
		pNodeNameDL = nil;
		
		pNodeList = ::dsBuildListFromStringsPriv( kDSNAttrDefaultLDAPPaths, nil );
		if ( pNodeList == nil ) throw( (SInt32)eMemoryAllocError );

		//extract the node list
		siResult = ::dsGetDirNodeInfo( aNodeRef, pNodeList, pNodeBuff, false, &uiCount, &attrListRef, nil  );
		if ( siResult != eDSNoErr ) throw( siResult );
		if ( uiCount == 0 ) throw ( (SInt32)eNoSearchNodesFound );
			
		::dsDataListDeallocate( fDirRef, pNodeList );
		free(pNodeList);
		pNodeList = nil;

		//assume first attribute since only 1 expected
		siResult = dsGetAttributeEntry( aNodeRef, pNodeBuff, attrListRef, 1, &attrValueListRef, &pAttrEntry );
		if ( siResult != eDSNoErr ) throw( siResult );

		//retrieve the node path strings
		for (aIndex=1; aIndex < (pAttrEntry->fAttributeValueCount+1); aIndex++)
		{
			siResult = dsGetAttributeValue( aNodeRef, pNodeBuff, aIndex, attrValueListRef, &pAttrValueEntry );
			if ( siResult != eDSNoErr ) throw( siResult );
			if ( pAttrValueEntry->fAttributeValueData.fBufferData == nil ) throw( (SInt32)eMemoryAllocError );
			//pAttrValueEntry->fAttributeValueData.fBufferData
			//pAttrValueEntry->fAttributeValueData.fBufferLength
			
			// Make the search list data node
			pSrchList = (sSearchList *)calloc( sizeof( sSearchList ), sizeof(char) );

			pSrchList->fNodeName = (char *)calloc( pAttrValueEntry->fAttributeValueData.fBufferLength + 1, sizeof(char) );
			if ( pSrchList->fNodeName != NULL )
				strcpy( pSrchList->fNodeName, pAttrValueEntry->fAttributeValueData.fBufferData );
			
			pSrchList->fDataList = ::dsBuildFromPathPriv( pSrchList->fNodeName, "/" );
			DbgLog( kLogPlugin, "CSearchPlugin::GetDefaultLDAPPaths:Search policy node %l = %s", uiCntr++, pSrchList->fNodeName );

			if ( outSrchList == nil )
			{
				outSrchList = pSrchList;
				outSrchList->fNext = nil;
				pCurList = outSrchList;
			}
			else
			{
				// Add this LDAP v3 node to the end of the default LDAP v3 list
				pCurList->fNext = pSrchList;
				pCurList = pSrchList;
				pCurList->fNext = nil;
			}
			dsDeallocAttributeValueEntry(fDirRef, pAttrValueEntry);
			pAttrValueEntry = nil;
		}

		dsCloseAttributeList(attrListRef);
		dsCloseAttributeValueList(attrValueListRef);
		dsDeallocAttributeEntry(fDirRef, pAttrEntry);
		pAttrEntry = nil;

		//close dir node after releasing attr references
		siResult = ::dsCloseDirNode(aNodeRef);
		if ( siResult != eDSNoErr ) throw( siResult );

		if ( pNodeBuff != nil )
		{
			::dsDataBufferDeAllocate( fDirRef, pNodeBuff );
			pNodeBuff = nil;
		}
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	if ( pNodeBuff != nil )
	{
		::dsDataBufferDeAllocate( fDirRef, pNodeBuff );
		pNodeBuff = nil;
	}

	if ( pNodeList != nil )
	{
		::dsDataListDeallocate( fDirRef, pNodeList );
		free(pNodeList);
		pNodeList = nil;
	}

	if ( pNodeNameDL != nil )
	{
		::dsDataListDeallocate( fDirRef, pNodeNameDL );
		free(pNodeNameDL);
		pNodeNameDL = nil;
	}

	return( outSrchList );

} // GetDefaultLDAPPaths


//--------------------------------------------------------------------------------------------------
//	* WakeUpRequests() (static)
//
//--------------------------------------------------------------------------------------------------

void CSearchPlugin::WakeUpRequests ( void )
{
	gKickSearchRequests.PostEvent();

} // WakeUpRequests


// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CSearchPlugin::WaitForInit ( void )
{
    // we wait for 2 minutes before giving up
    gKickSearchRequests.WaitForEvent( (UInt32)(2 * 60 * kMilliSecsPerSec) );
} // WaitForInit


// ---------------------------------------------------------------------------
//	* ProcessRequest
//
// ---------------------------------------------------------------------------

SInt32 CSearchPlugin::ProcessRequest ( void *inData )
{
	SInt32		siResult	= eDSNoErr;
	char	   *pathStr		= nil;

	try
	{
		if ( inData == nil )
		{
			throw( (SInt32)ePlugInDataError );
		}

		if (((sHeader *)inData)->fType == kOpenDirNode)
		{
			if (((sOpenDirNode *)inData)->fInDirNodeName != nil)
			{
				pathStr = ::dsGetPathFromListPriv( ((sOpenDirNode *)inData)->fInDirNodeName, "/" );
				if ( (pathStr != nil) && (strncmp(pathStr,"/Search",7) != 0) )
				{
					throw( (SInt32)eDSOpenNodeFailed);
				}
			}
		}
		
		if ( ((sHeader *)inData)->fType == kServerRunLoop || ((sHeader *)inData)->fType == kKerberosMutex )
		{
			// we don't care about these, just return
			return eDSNoErr;
		}
		
		WaitForInit();

		if (fState == kUnknownState)
		{
			throw( (SInt32)ePlugInCallTimedOut );
		}

        if ( (fState & kFailedToInit) || !(fState & kInitialized) )
        {
            throw( (SInt32)ePlugInFailedToInitialize );
        }

        if ( (fState & kInactive) || !(fState & kActive) )
        {
            throw( (SInt32)ePlugInNotActive );
        }

		if ( ((sHeader *)inData)->fType == kHandleNetworkTransition )
		{
			EnsureCheckNodesThreadIsRunning( eDSAuthenticationSearchNodeName ); // ensure our thread is running
			EnsureCheckNodesThreadIsRunning( eDSContactsSearchNodeName ); // ensure our thread is running
		}
		else
		{
			siResult = HandleRequest( inData );
		}
	}

	catch( SInt32 err )
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


// ---------------------------------------------------------------------------
//	* HandleRequest
//
// ---------------------------------------------------------------------------

SInt32 CSearchPlugin::HandleRequest ( void *inData )
{
	SInt32				siResult	= eDSNoErr;
	sHeader			   *pMsgHdr		= nil;

	try
	{
		pMsgHdr = (sHeader *)inData;

		switch ( pMsgHdr->fType )
		{
			case kReleaseContinueData:
				siResult = ReleaseContinueData( (sReleaseContinueData *)inData );
				break;

			case kOpenDirNode:
				siResult = OpenDirNode( (sOpenDirNode *)inData );
				break;

			case kCloseDirNode:
				siResult = CloseDirNode( (sCloseDirNode *)inData );
				break;

			case kGetDirNodeInfo:
				siResult = GetDirNodeInfo( (sGetDirNodeInfo *)inData );
				break;

			case kGetRecordList:
				siResult = GetRecordList( (sGetRecordList *)inData );
				break;

			case kGetRecordEntry:
				siResult = GetRecordEntry( (sGetRecordEntry *)inData );
				break;

			case kGetAttributeEntry:
				siResult = GetAttributeEntry( (sGetAttributeEntry *)inData );
				break;

			case kGetAttributeValue:
				siResult = GetAttributeValue( (sGetAttributeValue *)inData );
				break;

			case kDoAttributeValueSearch:
			case kDoAttributeValueSearchWithData:
				siResult = AttributeValueSearch( (sDoAttrValueSearchWithData *)inData );
				break;

			case kDoMultipleAttributeValueSearch:
			case kDoMultipleAttributeValueSearchWithData:
				siResult = MultipleAttributeValueSearch( (sDoMultiAttrValueSearchWithData *)inData );
				break;

			case kCloseAttributeList:
				siResult = CloseAttributeList( (sCloseAttributeList *)inData );
				break;

			case kCloseAttributeValueList:
				siResult = CloseAttributeValueList( (sCloseAttributeValueList *)inData );
				break;

			case kDoPlugInCustomCall:
				siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
				break;
                                
			case kServerRunLoop:
				siResult = eDSNoErr;
				break;
				
			case kHandleSystemWillSleep:
				siResult = eDSNoErr;
				SystemGoingToSleep();
				break;
				
			case kHandleSystemWillPowerOn:
				siResult = eDSNoErr;
				SystemWillPowerOn();
				break;

			default:
				siResult = eNotHandledByThisNode;
				break;
		}

		pMsgHdr->fResult = siResult;
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	catch ( ... )
	{
		siResult = ePlugInError;
	}

	return( siResult );

} // HandleRequest


//------------------------------------------------------------------------------------
//	* ReleaseContinueData
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::ReleaseContinueData ( sReleaseContinueData *inData )
{
	SInt32	siResult	= eDSNoErr;

	// RemoveItem calls our ContinueDeallocProc to clean up
	if ( gSNContinue->RemoveItem( inData->fInContinueData ) != eDSNoErr )
	{
		siResult = eDSInvalidContext;
	}

	return( siResult );

} // ReleaseContinueData


#pragma mark -
#pragma mark DS API Service Routines
#pragma mark -

//------------------------------------------------------------------------------------
//	* OpenDirNode
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::OpenDirNode ( sOpenDirNode *inData )
{
	SInt32				siResult			= eDSOpenNodeFailed;
	char			   *pathStr				= nil;
	sSearchContextData *pContext			= nil;
	sSearchConfig	   *aSearchConfigList	= nil;

	if ( inData != nil )
	{
		pathStr = ::dsGetPathFromListPriv( inData->fInDirNodeName, "/" );
		if ( pathStr != nil )
		{
			fMutex.WaitLock();
			aSearchConfigList = pSearchConfigList;
			while (aSearchConfigList != nil)
			{
				if ( ::strcmp( pathStr, aSearchConfigList->fSearchNodeName ) == 0 )
				{
					pContext = MakeContextData();
					if (pContext != nil)
					{
						//create a mutex for future use in a switch of search policy - only here in the node context
						pContext->pSearchListMutex = new DSMutexSemaphore("sSearchContextData::pSearchListMutex");
						pContext->fSearchConfigKey = aSearchConfigList->fSearchConfigKey;
						//check if this is the default network node at which point we need to build the node list
						if (strcmp(pathStr, kstrNetworkNodeName) == 0)
						{
							pContext->fSearchNodeList = BuildNetworkNodeList();
						}
						else //regular type of search node ie. either auth or contacts
						{
							//get the search path list with new unique refs of each
							//search path node for use by this client who opened the search node
							pContext->fSearchNodeList = DupSearchListWithNewRefs(aSearchConfigList->fSearchNodeList);
						}
						if (aSearchConfigList->fSearchNodePolicy == kAutomaticSearchPolicy)
						{
							pContext->bAutoSearchList = true;
						}
						pContext->fUID = inData->fInUID;
						pContext->fEffectiveUID = inData->fInEffectiveUID;
#if AUGMENT_RECORDS
						//here we refer to the relevant augment data via CConfigs class from within the node context for use later
						pContext->pConfigFromXML = aSearchConfigList->pConfigFromXML;
#endif

						gSNNodeRef->AddItem( inData->fOutNodeRef, pContext );
						siResult = eDSNoErr;
					}
					break;
				}
				aSearchConfigList = aSearchConfigList->fNext;
			}
			fMutex.SignalLock();
			
			free( pathStr );
			pathStr = nil;
		}
	}

	return( siResult );

} // OpenDirNode


//------------------------------------------------------------------------------------
//	* CloseDirNode
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::CloseDirNode ( sCloseDirNode *inData )
{
	SInt32				siResult		= eDSNoErr;
	sSearchContextData *pContext		= nil;

	try
	{
		pContext = (sSearchContextData *) gSNNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (SInt32)eDSInvalidNodeRef );

		gSNNodeRef->RemoveItem( inData->fInNodeRef );
		gSNContinue->RemoveItems( inData->fInNodeRef );
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // CloseDirNode


//------------------------------------------------------------------------------------
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::GetDirNodeInfo ( sGetDirNodeInfo *inData )
{
	SInt32				siResult		= eDSNoErr;
	UInt32				uiOffset		= 0;
	UInt32				uiNodeCnt		= 0;
	char			   *p				= nil;
	char			   *localNodeName	= nil;
	UInt32				uiCntr			= 1;
	UInt32				uiAttrCnt		= 0;
	CAttributeList	   *inAttrList		= nil;
	char			   *pAttrName		= nil;
	char			   *pData			= nil;
	sSearchContextData *pAttrContext	= nil;
	sSearchList		   *pListPtr		= nil;
	sSearchList		   *pListPtrToo		= nil;
	sSearchList		   *pListCustom		= nil;
	CBuff				outBuff;
	char			   *policyValue		= nil;
	sSearchContextData *pContext		= nil;
	sSearchConfig	   *aSearchConfig	= nil;
	CDataBuff	 	   *aRecData		= nil;
	CDataBuff	 	   *aAttrData		= nil;
	CDataBuff	 	   *aTmpData		= nil;
	UInt32				searchNodeNameBufLen = 0;
	bool				bHaveLock		= false;

	try
	{

		aRecData	= new CDataBuff();
		aAttrData	= new CDataBuff();
		aTmpData	= new CDataBuff();

		if ( inData  == nil ) throw( (SInt32)eMemoryError );

		pContext = (sSearchContextData *)gSNNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (SInt32)eDSInvalidNodeRef );
		
		siResult = CheckSearchPolicyChange(pContext, inData->fInNodeRef, inData->fOutContinueData);
		if( siResult != eDSNoErr )
		{
			throw( siResult );
		}

		fMutex.WaitLock();
		bHaveLock = true;
		aSearchConfig	= FindSearchConfigWithKey(pContext->fSearchConfigKey);
		if ( aSearchConfig == nil ) throw( (SInt32)eDSInvalidNodeRef );		

		inAttrList = new CAttributeList( inData->fInDirNodeInfoTypeList );
		if ( inAttrList == nil ) throw( (SInt32)eDSNullNodeInfoTypeList );
		if (inAttrList->GetCount() == 0) throw( (SInt32)eDSEmptyNodeInfoTypeList );

		siResult = outBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		// Set the real buffer type
		siResult = outBuff.SetBuffType( 'StdA' );
		if ( siResult != eDSNoErr ) throw( siResult );

		aRecData->Clear();
		aAttrData->Clear();

		// Set the record name and type
		aRecData->AppendShort( ::strlen( kDSStdRecordTypeSearchNodeInfo ) );
		aRecData->AppendString( kDSStdRecordTypeSearchNodeInfo );
		if (aSearchConfig->fSearchNodeName != nil)
		{
			searchNodeNameBufLen = strlen( aSearchConfig->fSearchNodeName );
			aRecData->AppendShort( searchNodeNameBufLen );
			searchNodeNameBufLen += 2;
			aRecData->AppendString( aSearchConfig->fSearchNodeName );
		}
		else
		{
			aRecData->AppendShort( ::strlen( "SearchNodeInfo" ) );
			aRecData->AppendString( "SearchNodeInfo" );
			searchNodeNameBufLen = 16; //2 + 14 = 16
		}

		while ( inAttrList->GetAttribute( uiCntr++, &pAttrName ) == eDSNoErr )
		{
			if (	(::strcmp( pAttrName, kDSAttributesAll ) == 0)	|| 
				(::strcmp( pAttrName, kDS1AttrSearchPath ) == 0) )
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrSearchPath ) );
				aTmpData->AppendString( kDS1AttrSearchPath );

				if ( inData->fInAttrInfoOnly == false )
				{
					uiNodeCnt = 0;
					pListPtr = pContext->fSearchNodeList;
					while ( pListPtr != nil )
					{
						uiNodeCnt++;
						pListPtr = pListPtr->fNext;
					}

					// Attribute value count
					aTmpData->AppendShort( uiNodeCnt );

					pListPtr = pContext->fSearchNodeList;
					while ( pListPtr != nil )
					{
						p = pListPtr->fNodeName;

						// Append attribute value
						aTmpData->AppendLong( ::strlen( p ) );
						aTmpData->AppendString( p );

						pListPtr = pListPtr->fNext;
					}
				}
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
			
			if ( (	(::strcmp( pAttrName, kDSAttributesAll ) == 0)	|| 
					(::strcmp( pAttrName, kDS1AttrNSPSearchPath ) == 0) ) &&
					(pContext->fSearchConfigKey != eDSNetworkSearchNodeName) )
				//NetInfo search policy path
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrNSPSearchPath ) );
				aTmpData->AppendString( kDS1AttrNSPSearchPath );

				if ( inData->fInAttrInfoOnly == false )
				{
					uiNodeCnt = 0;
					AddLocalNodesAsFirstPaths(&pListPtr);
					if ( pListPtr == nil ) throw( (SInt32)eSearchPathNotDefined );
					AddDefaultLDAPNodesLast(&pListPtr);
					
					pListPtrToo = pListPtr;
					while ( pListPtr != nil )
					{
						uiNodeCnt++;
						pListPtr = pListPtr->fNext;
					}

					// Attribute value count
					aTmpData->AppendShort( uiNodeCnt );

					pListPtr = pListPtrToo;
					while ( pListPtr != nil )
					{
						p = pListPtr->fNodeName;

						// Append attribute value
						aTmpData->AppendLong( ::strlen( p ) );
						aTmpData->AppendString( p );

						pListPtr = pListPtr->fNext;
					}
					
					//need to cleanup the struct list ie. the internals
					CleanSearchListData( pListPtrToo );
					pListPtrToo = nil;
				}
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
			
			if ( (	(::strcmp( pAttrName, kDSAttributesAll ) == 0)	|| 
					(::strcmp( pAttrName, kDS1AttrLSPSearchPath ) == 0) ) &&
					(pContext->fSearchConfigKey != eDSNetworkSearchNodeName) )
				//Local search policy path
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrLSPSearchPath ) );
				aTmpData->AppendString( kDS1AttrLSPSearchPath );

				if ( inData->fInAttrInfoOnly == false )
				{
					uiNodeCnt = 0;
					AddLocalNodesAsFirstPaths(&pListPtr);
					if ( pListPtr == nil ) throw( (SInt32)eSearchPathNotDefined );

					pListPtrToo = pListPtr;
					while ( pListPtr != nil )
					{
						uiNodeCnt++;
						pListPtr = pListPtr->fNext;
					}

					// Attribute value count
					aTmpData->AppendShort( uiNodeCnt );

					pListPtr = pListPtrToo;
					while ( pListPtr != nil )
					{
						p = pListPtr->fNodeName;

						// Append attribute value
						aTmpData->AppendLong( ::strlen( p ) );
						aTmpData->AppendString( p );

						pListPtr = pListPtr->fNext;
					}
					
					//need to cleanup the struct list ie. the internals
					CleanSearchListData( pListPtrToo );
					pListPtrToo = nil;
				}
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
			
			if ( (	(::strcmp( pAttrName, kDSAttributesAll ) == 0)	|| 
					(::strcmp( pAttrName, kDS1AttrCSPSearchPath ) == 0) ) &&
					(pContext->fSearchConfigKey != eDSNetworkSearchNodeName) )
				//Custom search policy path
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrCSPSearchPath ) );
				aTmpData->AppendString( kDS1AttrCSPSearchPath );

				if ( inData->fInAttrInfoOnly == false )
				{
					//get the custom portion
					if ( aSearchConfig->pConfigFromXML != nil )
					{
						pListCustom = aSearchConfig->pConfigFromXML->GetCustom();
					}
					AddLocalNodesAsFirstPaths(&pListPtr);
					if ( pListPtr == nil ) throw( (SInt32)eSearchPathNotDefined );
					
					//add the local to the front of the custom
					pListPtrToo = pListPtr;
					while ( pListPtrToo->fNext != nil )
					{
						pListPtrToo = pListPtrToo->fNext;
					}
					pListPtrToo->fNext = pListCustom;

					uiNodeCnt = 0;
					pListPtrToo = pListPtr;
					while ( pListPtr != nil )
					{
						uiNodeCnt++;
						pListPtr = pListPtr->fNext;
					}

					// Attribute value count
					aTmpData->AppendShort( uiNodeCnt );

					pListPtr = pListPtrToo;
					while ( pListPtr != nil )
					{
						p = pListPtr->fNodeName;

						// Append attribute value
						aTmpData->AppendLong( ::strlen( p ) );
						aTmpData->AppendString( p );

						pListPtr = pListPtr->fNext;
					}
					
					//need to cleanup the struct list ie. the internals
					CleanSearchListData( pListPtrToo );
					pListPtrToo = nil;
				}
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
			
			if (	(::strcmp( pAttrName, kDSAttributesAll ) == 0)	|| 
				(::strcmp( pAttrName, kDS1AttrSearchPolicy ) == 0) )
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrSearchPolicy ) );
				aTmpData->AppendString( kDS1AttrSearchPolicy );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count
					aTmpData->AppendShort( 1 );

					if (aSearchConfig->fSearchNodePolicy == kAutomaticSearchPolicy)
					{
						policyValue = new char[1+strlen(kDS1AttrNSPSearchPath)];
						strcpy(policyValue, kDS1AttrNSPSearchPath);
					}
					else if (aSearchConfig->fSearchNodePolicy == kLocalSearchPolicy)
					{
						policyValue = new char[1+strlen(kDS1AttrLSPSearchPath)];
						strcpy(policyValue, kDS1AttrLSPSearchPath);
					}
					else if (aSearchConfig->fSearchNodePolicy == kCustomSearchPolicy)
					{
						policyValue = new char[1+strlen(kDS1AttrCSPSearchPath)];
						strcpy(policyValue, kDS1AttrCSPSearchPath);
					}
					else
					{
						policyValue = new char[1+strlen("Unknown")];
						strcpy(policyValue,"Unknown");
					}
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( policyValue ) );
					aTmpData->AppendString( policyValue );
				}
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}

			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrReadOnlyNode ) == 0) )
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrReadOnlyNode ) );
				aTmpData->AppendString( kDS1AttrReadOnlyNode );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count
					aTmpData->AppendShort( 1 );

					//possible for a node to be ReadOnly, ReadWrite, WriteOnly
					//note that ReadWrite does not imply fully readable or writable
					
					// Add the root node as an attribute value
					aTmpData->AppendLong( ::strlen( "ReadOnly" ) );
					aTmpData->AppendString( "ReadOnly" );

				}
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length and data
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
				 
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrDHCPLDAPDefault ) == 0) )
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrDHCPLDAPDefault ) );
				aTmpData->AppendString( kDS1AttrDHCPLDAPDefault );

				if ( inData->fInAttrInfoOnly == false )
				{
					if ( aSearchConfig->pConfigFromXML != nil )
					{
						// Attribute value count
						aTmpData->AppendShort( 1 );
						
						if ( gDHCPLDAPEnabled )
						{
							aTmpData->AppendLong( sizeof("on")-1 );
							aTmpData->AppendString( "on" );
						}
						else
						{
							aTmpData->AppendLong( sizeof("off")-1 );
							aTmpData->AppendString( "off" );
						}
					}
				}
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length and data
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}

//			else if ( ::strcmp( pAttrName, kDS1AttrCapabilities ) == 0 )
//			else if ( ::strcmp( pAttrName, kDSNAttrRecordType ) == 0 )
		} // while loop over the attributes requested

		fMutex.SignalLock();
		bHaveLock = false;
		aRecData->AppendShort( uiAttrCnt );
		aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );

		outBuff.AddData( aRecData->GetData(), aRecData->GetLength() );
		inData->fOutAttrInfoCount = uiAttrCnt;

		pData = outBuff.GetDataBlock( 1, &uiOffset );
		if ( pData != nil )
		{
			pAttrContext = MakeContextData();
			
		//add to the offset for the attr list the length of the GetDirNodeInfo fixed record labels
//		record length = 4
//		aRecData->AppendShort( ::strlen( kDSStdRecordTypeSearchNodeInfo ) ); = 2
//		aRecData->AppendString( kDSStdRecordTypeSearchNodeInfo ); = 32
//		aRecData->AppendShort( ::strlen( "SearchNodeInfo" ) ); = see above for distinct node
//		aRecData->AppendString( "SearchNodeInfo" ); = see above for distinct node
//		total adjustment = 4 + 2 + 32 + 2 + 14 = 38

			pAttrContext->offset = uiOffset + 38 + searchNodeNameBufLen;

			gSNNodeRef->AddItem( inData->fOutAttrListRef, pAttrContext );
		}
		else
		{
			siResult = eDSBufferTooSmall;
		}
		
		inData->fOutDataBuff->fBufferLength = inData->fOutDataBuff->fBufferSize;
	}

	catch( SInt32 err )
	{
		siResult = err;
		if ( bHaveLock )
		{
			fMutex.SignalLock();
		}
	}

	if ( localNodeName != nil )
	{
		free( localNodeName );
		localNodeName = nil;
	}
	if ( inAttrList != nil )
	{
		delete( inAttrList );
		inAttrList = nil;
	}
	if (policyValue != nil)
	{
		delete( policyValue );
	}

	if ( aRecData != nil )
	{
		delete(aRecData);
		aRecData = nil;
	}
	
	if ( aAttrData != nil )
	{
		delete(aAttrData);
		aAttrData = nil;
	}
	
	if ( aTmpData != nil )
	{
		delete(aTmpData);
		aTmpData = nil;
	}

	return( siResult );

} // GetDirNodeInfo

//------------------------------------------------------------------------------------
//	* GetRecordList
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::GetRecordList ( sGetRecordList *inData )
{
	SInt32				siResult		= eDSNoErr;
	UInt32				recCount		= 0;
	bool				done			= false;
	sSearchContinueData	*pContinue		= nil;
	sSearchContinueData	*pInContinue	= nil;
	eSearchState		runState		= keGetRecordList;
	eSearchState		lastState		= keUnknownState;
	CBuff				inOutBuff;
	sSearchContextData *pContext		= nil;
	bool				bKeepOldBuffer	= false;

	try
	{
		pContext = (sSearchContextData *)gSNNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (SInt32)eDSInvalidNodeRef );

		if (pContext->pSearchListMutex == nil ) throw( (SInt32)eDSBadContextData);

		siResult = CheckSearchPolicyChange(pContext, inData->fInNodeRef, inData->fIOContinueData);
		
		// grab the mutex now
		pContext->pSearchListMutex->WaitLock();

		// we need to throw after we grab a valid mutex otherwise we unlock a mutex we don't own
		if( siResult != eDSNoErr ) throw( siResult );

		// Set it to the first node in the search list - this check doesn't need to use the context search path
		if ( pContext->fSearchNodeList == nil ) throw( (SInt32)eSearchPathNotDefined );

		if ( inData->fIOContinueData != nil )
		{
			if ( gSNContinue->VerifyItem( inData->fIOContinueData ) == true )
			{
				pInContinue = (sSearchContinueData *)inData->fIOContinueData;
				if (pInContinue->bNodeBuffTooSmall)
				{
					pInContinue->bNodeBuffTooSmall = false;
					throw( (SInt32)eDSBufferTooSmall );
				}

				// Create the new
				pContinue = (sSearchContinueData *)calloc( sizeof( sSearchContinueData ), sizeof( char ) );
				if ( pContinue == nil ) throw( (SInt32)eMemoryAllocError );

				pContinue->fDirRef				= pInContinue->fDirRef;
				pContinue->fNodeRef				= pInContinue->fNodeRef;
				pContinue->fAttrOnly			= pInContinue->fAttrOnly;
				pContinue->fRecCount			= pInContinue->fRecCount;
				pContinue->fRecIndex			= pInContinue->fRecIndex;
				pContinue->fState				= pInContinue->fState;
				pContinue->bIsAugmented			= pInContinue->bIsAugmented;
				pContinue->fAugmentReqAttribs	= dsDataListCopyList( 0, pInContinue->fAugmentReqAttribs );
				
				//check to see if the buffer has been resized
				if (inData->fInDataBuff->fBufferSize != pInContinue->fDataBuff->fBufferSize)
				{
					//need to save the contents of the buffer if there is something still there that we need
					//ie. check for pContinue->fState == keAddDataToBuff
					if (pContinue->fState == keAddDataToBuff)
					{
						//can we stall on this new allocation until we extract the remaining blocks?
						bKeepOldBuffer = true;
						pContinue->fDataBuff	= pInContinue->fDataBuff; //save the old buffer
						pInContinue->fDataBuff	= nil; //clean up separately in this case
					}
					else
					{
						pContinue->fDataBuff = ::dsDataBufferAllocatePriv( inData->fInDataBuff->fBufferSize );
						if ( pContinue->fDataBuff == nil ) throw( (SInt32)eMemoryAllocError );
					}
					//pInContinue->fDataBuff will get freed below in gSNContinue->RemoveItem
				}
				else
				{
					pContinue->fDataBuff	= pInContinue->fDataBuff;
					pInContinue->fDataBuff	= nil;
				}
				pContinue->fContextData		= pInContinue->fContextData;
				pContinue->fLimitRecSearch	= pInContinue->fLimitRecSearch;
				pContinue->fTotalRecCount	= pInContinue->fTotalRecCount;
				pContinue->bNodeBuffTooSmall= pInContinue->bNodeBuffTooSmall;

				// RemoveItem calls our ContinueDeallocProc to clean up
				// since we transfered ownership of these pointers we need to make sure they
				// are nil so the ContinueDeallocProc doesn't free them now
				pInContinue->fContextData		= nil;
				gSNContinue->RemoveItem( inData->fIOContinueData );

				pInContinue = nil;
				inData->fIOContinueData = nil;

				runState = pContinue->fState;
			}
			else
			{
				throw( (SInt32)eDSInvalidContinueData );
			}
		}
		else
		{
			pContinue = (sSearchContinueData *)calloc( 1, sizeof( sSearchContinueData ) );
			if ( pContinue == nil ) throw( (SInt32)eMemoryAllocError );

			pContinue->fDataBuff = ::dsDataBufferAllocatePriv( inData->fInDataBuff->fBufferSize );
			if ( pContinue->fDataBuff == nil ) throw( (SInt32)eMemoryAllocError );

			siResult = GetNextNodeRef( 0, &pContinue->fNodeRef, pContext );
			if ( siResult != eDSNoErr ) throw( siResult );

			pContinue->fDirRef = fDirRef;
			
			pContinue->fRecIndex		= 1;
			pContinue->fTotalRecCount	= 0;
			pContinue->fLimitRecSearch	= 0;
			pContinue->bNodeBuffTooSmall= false;
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pContinue
			if (inData->fOutRecEntryCount >= 0)
			{
				pContinue->fLimitRecSearch = inData->fOutRecEntryCount;
			}
		}

		// Empty the out buffer
		siResult = inOutBuff.Initialize( inData->fInDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = inOutBuff.SetBuffType( 'StdA' );
		if ( siResult != eDSNoErr ) throw( siResult );

		inData->fIOContinueData		= nil;
		//need to return zero if no records found
		inData->fOutRecEntryCount	= 0;

		while ( !done )
		{
			// Do the task
			switch ( runState )
			{
				// Get the original record list request
				case keGetRecordList:
				{

					if (pContinue->fLimitRecSearch > pContinue->fTotalRecCount)
					{
						recCount = pContinue->fLimitRecSearch - pContinue->fTotalRecCount;
					}
					else
					{
						recCount = 0;
					}

					siResult = ::dsGetRecordList( pContinue->fNodeRef,
													pContinue->fDataBuff,
													inData->fInRecNameList,
													inData->fInPatternMatch,
													inData->fInRecTypeList,
													(pContinue->bIsAugmented && pContinue->fAugmentReqAttribs ? pContinue->fAugmentReqAttribs : inData->fInAttribTypeList),
													inData->fInAttribInfoOnly,
													&recCount,
													&pContinue->fContextData );

					pContinue->fRecCount	= recCount;
					pContinue->fRecIndex	= 1;

					lastState = keGetRecordList;

				}
				break;

				// Add any data from the original record request to our own
				//	buffer format
				case keAddDataToBuff:
				{
					siResult = AddDataToOutBuff( pContinue, &inOutBuff, pContext, inData->fInAttribTypeList );
					if (bKeepOldBuffer)
					{
						if (siResult == eDSNoErr)
						{
							if ( pContinue->fDataBuff != nil )
							{
								::dsDataBufferDeallocatePriv( pContinue->fDataBuff );
								pContinue->fDataBuff = nil;
							}
							pContinue->fDataBuff = ::dsDataBufferAllocatePriv( inData->fInDataBuff->fBufferSize );
							if ( pContinue->fDataBuff == nil ) throw( (SInt32)eMemoryAllocError );
							bKeepOldBuffer = false;
						}
					}
					lastState = keAddDataToBuff;
				}
				break;
				
				case keGetNextNodeRef:
				{
					siResult	= GetNextNodeRef( pContinue->fNodeRef, &pContinue->fNodeRef, pContext );
					lastState	= keGetNextNodeRef;
					
					if ( siResult == eDSNoErr )
						UpdateContinueForAugmented( pContext, pContinue, inData->fInAttribTypeList );
				}
				break;

				case keSetContinueData:
				{
					switch ( lastState )
					{
						case keAddDataToBuff:
							inOutBuff.GetDataBlockCount( &inData->fOutRecEntryCount );
							//KW add to the total rec count what is going out for this call
							pContinue->fTotalRecCount += inData->fOutRecEntryCount;
							pContinue->fState = lastState;
							inData->fIOContinueData	= pContinue;
							gSNContinue->AddItem( pContinue, inData->fInNodeRef );
							siResult = eDSNoErr;
							break;

						case keGetRecordList:
						case keGetNextNodeRef:
							inOutBuff.GetDataBlockCount( &inData->fOutRecEntryCount );
							//KW add to the total rec count what is going out for this call
							pContinue->fTotalRecCount += inData->fOutRecEntryCount;
							pContinue->fState = keGetRecordList;
							if ( siResult == keSearchNodeListEnd )
							{
								siResult = eDSNoErr;
								inData->fIOContinueData = nil;
							}
							else
							{
								inData->fIOContinueData = pContinue;
								gSNContinue->AddItem( pContinue, inData->fInNodeRef );
							}
							break;

						case keBufferTooSmall:
							if (pContinue->fContextData == nil) //buffer too small in search node itself
							{
								pContinue->fState = keAddDataToBuff;
								siResult = eDSBufferTooSmall;
							}
							else //buffer too small in a search path node
							{
								//could be zero too
								inOutBuff.GetDataBlockCount( &inData->fOutRecEntryCount );
								//KW add to the total rec count what is going out for this call
								pContinue->fTotalRecCount += inData->fOutRecEntryCount;
								if (inData->fOutRecEntryCount == 0)
								{
									siResult = eDSBufferTooSmall;
								}
								else
								{
									pContinue->bNodeBuffTooSmall= true;
									siResult = eDSNoErr;
								}
								
								// if we have something in the buffer still, lets be sure we don't lose it
								if( pContinue->fRecIndex <= pContinue->fRecCount )
									pContinue->fState = keAddDataToBuff;
								else
									pContinue->fState = keGetRecordList;
							}
							inData->fIOContinueData	= pContinue;
							gSNContinue->AddItem( pContinue, inData->fInNodeRef );
							break;
							
						default:
							DbgLog( kLogPlugin, "*** Invalid continue state = %l", lastState );
							break;
					}
				}
				break;

				case keDone:
				{
					if ( pContinue != nil )
					{
						CSearchPlugin::ContinueDeallocProc( pContinue );
						pContinue = nil;
					}
					done = true;
				}
				break;

				default:
				{
					DbgLog( kLogPlugin, "*** Unknown run state = %l", runState );
					done = true;
				}
				break;

			} // switch for run state


			// *** Change State ***

			switch ( runState )
			{
				case keGetRecordList:
				{
					// Did dsGetRecordList succeed 
					if ( siResult == eDSNoErr )
					{
						// Did we find any records
						if ( pContinue->fRecCount != 0 )
						{
							// We found records, add them to our out buff
							runState = keAddDataToBuff;
						}
						else if (pContinue->fContextData == nil)
						{
							// No records were found on this node, do
							//	we need to get next node
							runState = keGetNextNodeRef;
						}
					}
					//condition on eDSRecordNotFound will no longer be needed
					else if ( siResult == eDSRecordNotFound ||
							  siResult == eDSInvalidRecordName ||
							  siResult == eDSInvalidRecordType ||
							  siResult == eDSNoStdMappingAvailable )
					{
						// No records were found on this node,
						// get next node
						runState = keGetNextNodeRef;
						//we need to ensure that continue data from previous node is NOT sent to the next node and that it is released
						if (siResult == eDSInvalidRecordType)
						{
							if (pContinue->fContextData != nil)
							{
								//release the continue data
								dsReleaseContinueData(pContinue->fNodeRef, pContinue->fContextData);
								pContinue->fContextData = 0;
							}
						}
					}
					else if (siResult == eDSBufferTooSmall)
					{
						lastState	= keBufferTooSmall;
						runState	= keSetContinueData;
					}
					else //move on to the next node
					{
						runState = keGetNextNodeRef;
					}
				}
				break;

				case keAddDataToBuff:
				{
					UInt32 aRecCnt = 0;
					inOutBuff.GetDataBlockCount(&aRecCnt);
					// Did we add all records to our buffer 
					if ( ( siResult == eDSNoErr ) || ( ( siResult == CBuff::kBuffFull ) && (aRecCnt > 0) ) )
					{
						inData->fOutRecEntryCount = aRecCnt;
							
						//check if we retrieved all that was requested
						//continue data might even be nil here
						if ((pContinue->fLimitRecSearch <= (pContinue->fTotalRecCount + inData->fOutRecEntryCount)) &&
								(pContinue->fLimitRecSearch != 0))
						{
							//KW would seem that setting continue data when we know we are done is wrong
							//runState = keSetContinueData;
							
							//KW add to the total rec count what is at least going out for this call
							pContinue->fTotalRecCount += inData->fOutRecEntryCount;
							
							//KW don't know why we need this continue data anymore?
							pContinue->fState = runState;
							//gSNContinue->AddItem( pContinue, inData->fInNodeRef );
							runState = keDone;
							inData->fIOContinueData	= nil;
							siResult = eDSNoErr;
						}
						else
						{
							if ( siResult == CBuff::kBuffFull )
							{
								runState = keSetContinueData;
							}
							// Do we need to continue the original read
							else if ( pContinue->fContextData )
							{
								lastState = keGetRecordList;
								//runState = keSetContinueData;
								runState = keGetRecordList;
							}
							else
							{
								// we need to get next node
								runState = keGetNextNodeRef;
							}
						}
					}
					else
					{
						if ( siResult == CBuff::kBuffFull )
						{
							runState = keSetContinueData;
							lastState = keBufferTooSmall;
						}
						else
						{
							// We got an error we don't know how to deal with, we be gone
							runState = keDone;
						}
					}
				}
				break;

				case keGetNextNodeRef:
				{
					inOutBuff.GetDataBlockCount( &recCount );
					if ( siResult == eDSNoErr )
					{
						if ( recCount == 0 )
						{
							runState = keGetRecordList;
						}
						else
						{
							runState = keSetContinueData;
						}
					}
					else
					{
						if ( siResult == keSearchNodeListEnd )
						{
							runState = keSetContinueData;
						}
						else
						{
							runState = keDone;
						}
					}
				}
				break;

				case keSetContinueData:
				case keDone:
				case keError:
				{
					done = true;
				}
				break;

				default:
				{
					DbgLog( kLogPlugin, "*** Unknown transition state = %l", runState );
					done = true;
				}
				break;

			} // switch for transition state
		}

		pContext->pSearchListMutex->SignalLock();
	
	}

	catch( SInt32 err )
	{
		if ( (pContext != nil) && (pContext->pSearchListMutex != nil) )
		{
			pContext->pSearchListMutex->SignalLock();
		}
		siResult = err;
	}

	if ( (inData->fIOContinueData == nil) && (pContinue != nil ) )
	{
		// we have decided not to return contine data, need to free it
		CSearchPlugin::ContinueDeallocProc( pContinue );
		pContinue = nil;
	}

	return( siResult );

} // GetRecordList


//------------------------------------------------------------------------------------
//	* GetRecordEntry
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::GetRecordEntry ( sGetRecordEntry *inData )
{
	SInt32					siResult		= eDSNoErr;
	UInt32					uiIndex			= 0;
	UInt32					uiCount			= 0;
	UInt32					uiOffset		= 0;
	UInt32					uberOffset		= 0;
	char 				   *pData			= nil;
	tRecordEntryPtr			pRecEntry		= nil;
	sSearchContextData 	   *pContext		= nil;
	CBuff					inBuff;
	UInt32					offset			= 0;
	UInt16					usTypeLen		= 0;
	char				   *pRecType		= nil;
	UInt16					usNameLen		= 0;
	char				   *pRecName		= nil;
	UInt16					usAttrCnt		= 0;
	UInt32					buffLen			= 0;

	try
	{
		if ( inData  == nil ) throw( (SInt32)eMemoryError );
		if ( inData->fInOutDataBuff  == nil ) throw( (SInt32)eDSEmptyBuffer );
		if (inData->fInOutDataBuff->fBufferSize == 0) throw( (SInt32)eDSEmptyBuffer );

		siResult = inBuff.Initialize( inData->fInOutDataBuff );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = inBuff.GetDataBlockCount( &uiCount );
		if ( siResult != eDSNoErr ) throw( siResult );

		uiIndex = inData->fInRecEntryIndex;
		if ( uiIndex == 0 ) throw( (SInt32)eDSInvalidIndex );

		if ( uiIndex > uiCount ) throw( (SInt32)eDSIndexOutOfRange );

		pData = inBuff.GetDataBlock( uiIndex, &uberOffset );
		if ( pData  == nil ) throw( (SInt32)eDSCorruptBuffer );

		//assume that the length retrieved is valid
		buffLen = inBuff.GetDataBlockLength( uiIndex );
		
		// Skip past this same record length obtained from GetDataBlockLength
		pData	+= 4;
		offset	= 0; //buffLen does not include first four bytes

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record type
		::memcpy( &usTypeLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecType = pData;
		
		pData	+= usTypeLen;
		offset	+= usTypeLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record name
		::memcpy( &usNameLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecName = pData;
		
		pData	+= usNameLen;
		offset	+= usNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the attribute count
		::memcpy( &usAttrCnt, pData, 2 );

		pRecEntry = (tRecordEntry *)calloc( 1, sizeof( tRecordEntry ) + usNameLen + usTypeLen + 4 + kBuffPad );

		pRecEntry->fRecordNameAndType.fBufferSize	= usNameLen + usTypeLen + 4 + kBuffPad;
		pRecEntry->fRecordNameAndType.fBufferLength	= usNameLen + usTypeLen + 4;

		// Add the record name length
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData, &usNameLen, 2 );
		uiOffset += 2;

		// Add the record name
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecName, usNameLen );
		uiOffset += usNameLen;

		// Add the record type length
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &usTypeLen, 2 );

		// Add the record type
		uiOffset += 2;
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecType, usTypeLen );

		pRecEntry->fRecordAttributeCount = usAttrCnt;

		pContext = MakeContextData();
		if ( pContext  == nil ) throw( (SInt32)eMemoryAllocError );

		pContext->offset = uberOffset + offset + 4;	// context used by next calls of GetAttributeEntry
													// include the four bytes of the buffLen
		
		gSNNodeRef->AddItem( inData->fOutAttrListRef, pContext );

		inData->fOutRecEntryPtr = pRecEntry;
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetRecordEntry


//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::GetAttributeEntry ( sGetAttributeEntry *inData )
{
	SInt32					siResult			= eDSNoErr;
	UInt16					usAttrTypeLen		= 0;
	UInt16					usAttrCnt			= 0;
	UInt32					usAttrLen			= 0;
	UInt16					usValueCnt			= 0;
	UInt32					usValueLen			= 0;
	UInt32					i					= 0;
	UInt32					uiIndex				= 0;
	UInt32					uiAttrEntrySize		= 0;
	UInt32					uiOffset			= 0;
	UInt32					uiTotalValueSize	= 0;
	UInt32					offset				= 0;
	UInt32					buffSize			= 0;
	UInt32					buffLen				= 0;
	char				   *p			   		= nil;
	char				   *pAttrType	   		= nil;
	tDataBuffer			   *pDataBuff			= nil;
	tAttributeValueListRef	attrValueListRef	= 0;
	tAttributeEntryPtr		pAttribInfo			= nil;
	sSearchContextData 	   *pAttrContext		= nil;
	sSearchContextData 	   *pValueContext		= nil;

	try
	{
		if ( inData  == nil ) throw( (SInt32)eMemoryError );

		pAttrContext = (sSearchContextData *)gSNNodeRef->GetItemData( inData->fInAttrListRef );
		if ( pAttrContext  == nil ) throw( (SInt32)eDSBadContextData );

		uiIndex = inData->fInAttrInfoIndex;
		if (uiIndex == 0) throw( (SInt32)eDSInvalidIndex );
				
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (SInt32)eDSNullDataBuff );
		
		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pAttrContext->offset;
		offset	= pAttrContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
				
		// Get the attribute count
		::memcpy( &usAttrCnt, p, 2 );
		if (uiIndex > usAttrCnt) throw( (SInt32)eDSIndexOutOfRange );

		// Move 2 bytes
		p		+= 2;
		offset	+= 2;

		// Skip to the attribute that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 + offset > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
		
			// Get the length for the attribute
			::memcpy( &usAttrLen, p, 4 );

			// Move the offset past the length word and the length of the data
			p		+= 4 + usAttrLen;
			offset	+= 4 + usAttrLen;
		}

		// Get the attribute offset
		uiOffset = offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 + offset > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute block
		::memcpy( &usAttrLen, p, 4 );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		//set the bufLen to stricter range
		buffLen = offset + usAttrLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute type
		::memcpy( &usAttrTypeLen, p, 2 );
		
		pAttrType = p + 2;
		p		+= 2 + usAttrTypeLen;
		offset	+= 2 + usAttrTypeLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get number of values for this attribute
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;
		
		for ( i = 0; i < usValueCnt; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
			
			uiTotalValueSize += usValueLen;
		}

		uiAttrEntrySize = sizeof( tAttributeEntry ) + usAttrTypeLen + kBuffPad;
		pAttribInfo = (tAttributeEntry *)calloc( 1, uiAttrEntrySize );

		pAttribInfo->fAttributeValueCount				= usValueCnt;
		pAttribInfo->fAttributeDataSize					= uiTotalValueSize;
		pAttribInfo->fAttributeValueMaxSize				= 512;				// KW this is not used anywhere
		pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kBuffPad;
		pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
		::memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

		attrValueListRef = inData->fOutAttrValueListRef;

		pValueContext = MakeContextData();
		if ( pValueContext  == nil ) throw( (SInt32)eMemoryAllocError );

		pValueContext->offset	= uiOffset;

		gSNNodeRef->AddItem( inData->fOutAttrValueListRef, pValueContext );

		inData->fOutAttrInfoPtr = pAttribInfo;
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeEntry


//------------------------------------------------------------------------------------
//	* GetAttributeValue
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::GetAttributeValue ( sGetAttributeValue *inData )
{
	SInt32						siResult		= eDSNoErr;
	UInt16						usValueCnt		= 0;
	UInt32						usValueLen		= 0;
	UInt16						usAttrNameLen	= 0;
	UInt32						i				= 0;
	UInt32						uiIndex			= 0;
	UInt32						offset			= 0;
	char					   *p				= nil;
	tDataBuffer				   *pDataBuff		= nil;
	tAttributeValueEntry	   *pAttrValue		= nil;
	sSearchContextData 		   *pValueContext	= nil;
	UInt32						buffSize		= 0;
	UInt32						buffLen			= 0;
	UInt32						attrLen			= 0;

	try
	{
		pValueContext = (sSearchContextData *)gSNNodeRef->GetItemData( inData->fInAttrValueListRef );
		if ( pValueContext  == nil ) throw( (SInt32)eDSBadContextData );

		uiIndex = inData->fInAttrValueIndex;
		if (uiIndex == 0) throw( (SInt32)eDSInvalidIndex );
		
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (SInt32)eDSNullDataBuff );

		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pValueContext->offset;
		offset	= pValueContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 + offset > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );
				
		// Get the buffer length
		::memcpy( &attrLen, p, 4 );

		//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
		//AND add the length of the buffer length var as stored ie. 4 bytes
		buffLen		= attrLen + pValueContext->offset + 4;
		if (buffLen > buffSize)  throw( (SInt32)eDSInvalidBuffFormat );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the attribute name length
		::memcpy( &usAttrNameLen, p, 2 );
		
		p		+= 2 + usAttrNameLen;
		offset	+= 2 + usAttrNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the value count
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		if (uiIndex > usValueCnt)  throw( (SInt32)eDSIndexOutOfRange );

		// Skip to the value that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
		}

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 + offset > buffLen)  throw( (SInt32)eDSInvalidBuffFormat );
		
		::memcpy( &usValueLen, p, 4 );
		
		p		+= 4;
		offset	+= 4;

		//if (usValueLen == 0)  throw( (SInt32)eDSInvalidBuffFormat ); //if zero is it okay?

		pAttrValue = (tAttributeValueEntry *)calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kBuffPad );

		pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kBuffPad;
		pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if ( usValueLen + offset > buffLen ) throw( (SInt32)eDSInvalidBuffFormat );
		
		::memcpy( pAttrValue->fAttributeValueData.fBufferData, p, usValueLen );

		// Set the attribute value ID
		pAttrValue->fAttributeValueID = 0x00;

		inData->fOutAttrValue = pAttrValue;
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeValue
	

//------------------------------------------------------------------------------------
//	* AttributeValueSearch
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::AttributeValueSearch ( sDoAttrValueSearchWithData *inData )
{
	SInt32				siResult		= eDSNoErr;
	UInt32				recCount		= 0;
	bool				done			= false;
	sSearchContinueData	*pContinue		= nil;
	sSearchContinueData	*pInContinue	= nil;
	eSearchState		runState		= keGetRecordList;		//note that there is NO keAttributeValueSearch
																//but keGetRecordList is used here instead
	eSearchState		lastState		= keUnknownState;
	CBuff				inOutBuff;
	sSearchContextData *pContext		= nil;
	bool				bKeepOldBuffer	= false;

	try
	{
		pContext = (sSearchContextData *)gSNNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (SInt32)eDSInvalidNodeRef );
		
		if (pContext->pSearchListMutex == nil ) throw( (SInt32)eDSBadContextData);

		siResult = CheckSearchPolicyChange(pContext, inData->fInNodeRef, inData->fIOContinueData);
				
		// grab the mutex now
		pContext->pSearchListMutex->WaitLock();

		// we need to throw after we grab a valid mutex otherwise we unlock a mutex we don't own
		if( siResult != eDSNoErr ) throw( siResult );
		
		// Set it to the first node in the search list - this check doesn't need to use the context search path
		if ( pContext->fSearchNodeList == nil ) throw( (SInt32)eSearchPathNotDefined );

		if ( inData->fIOContinueData != nil )
		{
			if ( gSNContinue->VerifyItem( inData->fIOContinueData ) == true )
			{
				pInContinue = (sSearchContinueData *)inData->fIOContinueData;
				if (pInContinue->bNodeBuffTooSmall)
				{
					pInContinue->bNodeBuffTooSmall = false;
					throw( (SInt32)eDSBufferTooSmall );
				}

				// Create the new
				pContinue = (sSearchContinueData *)calloc( sizeof( sSearchContinueData ), sizeof(char) );
				if ( pContinue == nil ) throw( (SInt32)eMemoryAllocError );

				pContinue->fDirRef				= pInContinue->fDirRef;
				pContinue->fNodeRef				= pInContinue->fNodeRef;
				pContinue->fAttrOnly			= pInContinue->fAttrOnly;
				pContinue->fRecCount			= pInContinue->fRecCount;
				pContinue->fRecIndex			= pInContinue->fRecIndex;
				pContinue->fState				= pInContinue->fState;
				pContinue->bIsAugmented			= pInContinue->bIsAugmented;
				pContinue->fAugmentReqAttribs	= dsDataListCopyList( 0, pInContinue->fAugmentReqAttribs );

				//check to see if the buffer has been resized
				if (inData->fOutDataBuff->fBufferSize != pInContinue->fDataBuff->fBufferSize)
				{
					//need to save the contents of the buffer if there is something still there that we need
					//ie. check for pContinue->fState == keAddDataToBuff
					if (pContinue->fState == keAddDataToBuff)
					{
						//can we stall on this new allocation until we extract the remaining blocks?
						bKeepOldBuffer = true;
						pContinue->fDataBuff	= pInContinue->fDataBuff; //save the old buffer
						pInContinue->fDataBuff	= nil; //clean up separately in this case
					}
					else
					{
						pContinue->fDataBuff = ::dsDataBufferAllocatePriv( inData->fOutDataBuff->fBufferSize );
						if ( pContinue->fDataBuff == nil ) throw( (SInt32)eMemoryAllocError );
					}
					//pInContinue->fDataBuff will get freed below in gSNContinue->RemoveItem
				}
				else
				{
					pContinue->fDataBuff	= pInContinue->fDataBuff;
					pInContinue->fDataBuff	= nil;
				}
				pContinue->fContextData		= pInContinue->fContextData;
				pContinue->fLimitRecSearch	= pInContinue->fLimitRecSearch;
				pContinue->fTotalRecCount	= pInContinue->fTotalRecCount;
				pContinue->bNodeBuffTooSmall= pInContinue->bNodeBuffTooSmall;
				

				// RemoveItem calls our ContinueDeallocProc to clean up
				// since we transfered ownership of these pointers we need to make sure they
				// are nil so the ContinueDeallocProc doesn't free them now
				pInContinue->fContextData		= nil;
				gSNContinue->RemoveItem( inData->fIOContinueData );

				pInContinue = nil;
				inData->fIOContinueData = nil;

				runState = pContinue->fState;
			}
			else
			{
				throw( (SInt32)eDSInvalidContinueData );
			}
		}
		else
		{
			pContinue = (sSearchContinueData *)calloc( 1, sizeof( sSearchContinueData ) );
			if ( pContinue == nil ) throw( (SInt32)eMemoryAllocError );

			pContinue->fDataBuff = ::dsDataBufferAllocatePriv( inData->fOutDataBuff->fBufferSize );
			if ( pContinue->fDataBuff == nil ) throw( (SInt32)eMemoryAllocError );

			siResult = GetNextNodeRef( 0, &pContinue->fNodeRef, pContext );
			if ( siResult != eDSNoErr ) throw( siResult );
			
			pContinue->fDirRef = fDirRef;
			
			pContinue->fRecIndex		= 1;
			pContinue->fTotalRecCount	= 0;
			pContinue->fLimitRecSearch	= 0;
			pContinue->bNodeBuffTooSmall= false;
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pContinue
			if (inData->fOutMatchRecordCount >= 0)
			{
				pContinue->fLimitRecSearch = inData->fOutMatchRecordCount;
			}
		}

		// Empty the out buffer
		siResult = inOutBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = inOutBuff.SetBuffType( 'StdA' );
		if ( siResult != eDSNoErr ) throw( siResult );

		inData->fIOContinueData			= nil;
		//need to return zero if no records found
		inData->fOutMatchRecordCount	= 0;
		

		while ( !done )
		{
			// Do the task
			switch ( runState )
			{
				// Get the original record list request
				case keGetRecordList:
				{
					if (pContinue->fLimitRecSearch > pContinue->fTotalRecCount)
					{
						recCount = pContinue->fLimitRecSearch - pContinue->fTotalRecCount;
					}
					else
					{
						recCount = 0;
					}
						if ( inData->fType == kDoAttributeValueSearchWithData )
						{
							siResult = ::dsDoAttributeValueSearchWithData(
																	pContinue->fNodeRef,
																	pContinue->fDataBuff,
																	inData->fInRecTypeList,
																	inData->fInAttrType,
																	inData->fInPattMatchType,
																	inData->fInPatt2Match,
																	(pContinue->bIsAugmented && pContinue->fAugmentReqAttribs ? pContinue->fAugmentReqAttribs : inData->fInAttrTypeRequestList),
																	inData->fInAttrInfoOnly,
																	&recCount,
																	&pContinue->fContextData );
						}
						else
						{
							siResult = ::dsDoAttributeValueSearch(	pContinue->fNodeRef,
																	pContinue->fDataBuff,
																	inData->fInRecTypeList,
																	inData->fInAttrType,
																	inData->fInPattMatchType,
																	inData->fInPatt2Match,
																	&recCount,
																	&pContinue->fContextData );
						}

					pContinue->fRecCount	= recCount;
					pContinue->fRecIndex	= 1;
					
					lastState = keGetRecordList;
				}
				break;

				// Add any data from the original record request to our own
				//	buffer format
				case keAddDataToBuff:
				{
					if ( inData->fType == kDoAttributeValueSearchWithData )
					{
						siResult = AddDataToOutBuff( pContinue, &inOutBuff, pContext, inData->fInAttrTypeRequestList );
					}
					else
					{
						siResult = AddDataToOutBuff( pContinue, &inOutBuff, pContext, NULL );
					}
					
					if (bKeepOldBuffer)
					{
						if (siResult == eDSNoErr)
						{
							if ( pContinue->fDataBuff != nil )
							{
								::dsDataBufferDeallocatePriv( pContinue->fDataBuff );
								pContinue->fDataBuff = nil;
							}
							pContinue->fDataBuff = ::dsDataBufferAllocatePriv( inData->fOutDataBuff->fBufferSize );
							if ( pContinue->fDataBuff == nil ) throw( (SInt32)eMemoryAllocError );
							bKeepOldBuffer = false;
						}
					}
					lastState = keAddDataToBuff;
				}
				break;
				
				case keGetNextNodeRef:
				{
					siResult	= GetNextNodeRef( pContinue->fNodeRef, &pContinue->fNodeRef, pContext );
					lastState	= keGetNextNodeRef;

					if ( inData->fType == kDoAttributeValueSearchWithData && siResult == eDSNoErr )
						UpdateContinueForAugmented( pContext, pContinue, inData->fInAttrTypeRequestList );
				}
				break;

				case keSetContinueData:
				{
					switch ( lastState )
					{
						case keAddDataToBuff:
							inOutBuff.GetDataBlockCount( &inData->fOutMatchRecordCount );
							//KW add to the total rec count what is going out for this call
							pContinue->fTotalRecCount += inData->fOutMatchRecordCount;
							pContinue->fState = lastState;
							inData->fIOContinueData	= pContinue;
							gSNContinue->AddItem( pContinue, inData->fInNodeRef );
							siResult = eDSNoErr;
							break;

						case keGetRecordList:
						case keGetNextNodeRef:
							inOutBuff.GetDataBlockCount( &inData->fOutMatchRecordCount );
							//KW add to the total rec count what is going out for this call
							pContinue->fTotalRecCount += inData->fOutMatchRecordCount;
							pContinue->fState = keGetRecordList;
							if ( siResult == keSearchNodeListEnd )
							{
								siResult = eDSNoErr;
								inData->fIOContinueData = nil;
							}
							else
							{
								inData->fIOContinueData = pContinue;
								gSNContinue->AddItem( pContinue, inData->fInNodeRef );
							}
							break;

						case keBufferTooSmall:
							if (pContinue->fContextData == nil) //buffer too small in search node itself
							{
								pContinue->fState = keAddDataToBuff;
								siResult = eDSBufferTooSmall;
							}
							else //buffer too small in a search path node
							{
								//could be zero too
								inOutBuff.GetDataBlockCount( &inData->fOutMatchRecordCount );
								//KW add to the total rec count what is going out for this call
								pContinue->fTotalRecCount += inData->fOutMatchRecordCount;
								if (inData->fOutMatchRecordCount == 0)
								{
									siResult = eDSBufferTooSmall;
								}
								else
								{
									pContinue->bNodeBuffTooSmall= true;
									siResult = eDSNoErr;
								}
								
								// if we have something in the buffer still, lets be sure we don't lose it
								if( pContinue->fRecIndex <= pContinue->fRecCount )
									pContinue->fState = keAddDataToBuff;
								else
									pContinue->fState = keGetRecordList;
							}
							inData->fIOContinueData	= pContinue;
							gSNContinue->AddItem( pContinue, inData->fInNodeRef );
							break;

						default:
							DbgLog( kLogPlugin, "*** Invalid continue state = %l", lastState );
							break;
					}
				}
				break;

				case keDone:
				{
					if ( pContinue != nil )
					{
						CSearchPlugin::ContinueDeallocProc( pContinue );
						pContinue = nil;
					}
					done = true;
				}
				break;

				default:
				{
					DbgLog( kLogPlugin, "*** Unknown run state = %l", runState );
					done = true;
				}
				break;

			} // switch for run state



			// *** Change State ***

			switch ( runState )
			{
				case keGetRecordList:
				{
					// Did dsGetRecordList succeed 
					if ( siResult == eDSNoErr )
					{
						// Did we find any records
						if ( pContinue->fRecCount != 0 )
						{
							// We found records, add them to our out buff
							runState = keAddDataToBuff;
						}
						else if (pContinue->fContextData == nil)
						{
							// No records were found on this node, do
							//	we need to get next node
							runState = keGetNextNodeRef;
						}
					}
					//condition on eDSRecordNotFound will no longer be needed
					else if ( siResult == eDSRecordNotFound ||
							  siResult == eDSInvalidRecordName ||
							  siResult == eDSInvalidRecordType ||
							  siResult == eDSNoStdMappingAvailable )
					{
						// No records were found on this node,
						// get next node
						runState = keGetNextNodeRef;
						//we need to ensure that continue data from previous node is NOT sent to the next node and that it is released
						if (siResult == eDSInvalidRecordType)
						{
							if (pContinue->fContextData != nil)
							{
								//release the continue data
								dsReleaseContinueData(pContinue->fNodeRef, pContinue->fContextData);
								pContinue->fContextData = 0;
							}
						}
					}
					else if (siResult == eDSBufferTooSmall)
					{
						lastState	= keBufferTooSmall;
						runState	= keSetContinueData;
					}
					else //move on to the next node
					{
						runState = keGetNextNodeRef;
					}
				}
				break;

				case keAddDataToBuff:
				{
					UInt32 aRecCnt = 0;
					inOutBuff.GetDataBlockCount(&aRecCnt);
					// Did we add all records to our buffer 
					if ( ( siResult == eDSNoErr ) || ( ( siResult == CBuff::kBuffFull ) && (aRecCnt > 0) ) )
					{
						inData->fOutMatchRecordCount = aRecCnt;
							
						//check if we retrieved all that was requested
						//continue data might even be nil here
						if ((pContinue->fLimitRecSearch <= (pContinue->fTotalRecCount + inData->fOutMatchRecordCount)) &&
								(pContinue->fLimitRecSearch != 0))
						{
							//KW would seem that setting continue data when we know we are done is wrong
							//runState = keSetContinueData;
							
							//KW add to the total rec count what is at least going out for this call
							pContinue->fTotalRecCount += inData->fOutMatchRecordCount;
							
							//KW don't know why we need this continue data anymore?
							pContinue->fState = runState;
							//gSNContinue->AddItem( pContinue, inData->fInNodeRef );
							runState = keDone;
							inData->fIOContinueData	= nil;
							siResult = eDSNoErr;
						}
						else
						{
							if ( siResult == CBuff::kBuffFull )
							{
								runState = keSetContinueData;
							}
							// Do we need to continue the original read
							else if ( pContinue->fContextData )
							{
								lastState = keGetRecordList;
								//runState = keSetContinueData;
								runState = keGetRecordList;
							}
							else
							{
								// we need to get the next node
								runState = keGetNextNodeRef;
							}
						}
					}
					else
					{
						if ( siResult == CBuff::kBuffFull )
						{
							runState = keSetContinueData;
							lastState = keBufferTooSmall;
						}
						else
						{
							// We got an error we don't know how to deal with, we be gone
							runState = keDone;
						}
					}
				}
				break;

				case keGetNextNodeRef:
				{
					inOutBuff.GetDataBlockCount( &recCount );
					if ( siResult == eDSNoErr )
					{
						if ( recCount == 0 )
						{
							runState = keGetRecordList;
						}
						else
						{
							runState = keSetContinueData;
						}
					}
					else
					{
						if ( siResult == keSearchNodeListEnd )
						{
							runState = keSetContinueData;
						}
						else
						{
							runState = keDone;
						}
					}
				}
				break;

				case keSetContinueData:
				case keDone:
				case keError:
				{
					done = true;
				}
				break;

				default:
				{
					DbgLog( kLogPlugin, "*** Unknown transition state = %l", runState );
					done = true;
				}
				break;

			} // switch for transition state
		}
		
		pContext->pSearchListMutex->SignalLock();
		
	}

	catch( SInt32 err )
	{
		if ( (pContext != nil) && (pContext->pSearchListMutex != nil) )
		{
			pContext->pSearchListMutex->SignalLock();
		}
		siResult = err;
	}

	if ( (inData->fIOContinueData == nil) && (pContinue != nil ) )
	{
		// we have decided not to return contine data, need to free it
		CSearchPlugin::ContinueDeallocProc( pContinue );
		pContinue = nil;
	}

	return( siResult );

} // AttributeValueSearch


//------------------------------------------------------------------------------------
//	* MultipleAttributeValueSearch
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::MultipleAttributeValueSearch ( sDoMultiAttrValueSearchWithData *inData )
{

	SInt32				siResult		= eDSNoErr;
	UInt32				recCount		= 0;
	bool				done			= false;
	sSearchContinueData	*pContinue		= nil;
	sSearchContinueData	*pInContinue	= nil;
	eSearchState		runState		= keGetRecordList;		//note that there is NO keMultipleAttributeValueSearch
																//but keGetRecordList is used here instead
	eSearchState		lastState		= keUnknownState;
	CBuff				inOutBuff;
	sSearchContextData *pContext		= nil;
	bool				bKeepOldBuffer	= false;

	try
	{		
		pContext = (sSearchContextData *)gSNNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (SInt32)eDSInvalidNodeRef );
		
		if (pContext->pSearchListMutex == nil ) throw( (SInt32)eDSBadContextData);

		siResult = CheckSearchPolicyChange(pContext, inData->fInNodeRef, inData->fIOContinueData);
		
		// grab the mutex now
		pContext->pSearchListMutex->WaitLock();
		
		// we need to throw after we grab a valid mutex otherwise we unlock a mutex we don't own
		if( siResult != eDSNoErr ) throw( siResult );
		
		// Set it to the first node in the search list - this check doesn't need to use the context search path
		if ( pContext->fSearchNodeList == nil ) throw( (SInt32)eSearchPathNotDefined );

		if ( inData->fIOContinueData != nil )
		{
			if ( gSNContinue->VerifyItem( inData->fIOContinueData ) == true )
			{
				pInContinue = (sSearchContinueData *)inData->fIOContinueData;
				if (pInContinue->bNodeBuffTooSmall)
				{
					pInContinue->bNodeBuffTooSmall = false;
					throw( (SInt32)eDSBufferTooSmall );
				}

				// Create the new
				pContinue = (sSearchContinueData *)calloc( sizeof( sSearchContinueData ), sizeof(char) );
				if ( pContinue == nil ) throw( (SInt32)eMemoryAllocError );

				pContinue->fDirRef				= pInContinue->fDirRef;
				pContinue->fNodeRef				= pInContinue->fNodeRef;
				pContinue->fAttrOnly			= pInContinue->fAttrOnly;
				pContinue->fRecCount			= pInContinue->fRecCount;
				pContinue->fRecIndex			= pInContinue->fRecIndex;
				pContinue->fState				= pInContinue->fState;
				pContinue->bIsAugmented			= pInContinue->bIsAugmented;
				pContinue->fAugmentReqAttribs	= dsDataListCopyList( 0, pInContinue->fAugmentReqAttribs );

				//check to see if the buffer has been resized
				if (inData->fOutDataBuff->fBufferSize != pInContinue->fDataBuff->fBufferSize)
				{
					//need to save the contents of the buffer if there is something still there that we need
					//ie. check for pContinue->fState == keAddDataToBuff
					if (pContinue->fState == keAddDataToBuff)
					{
						//can we stall on this new allocation until we extract the remaining blocks?
						bKeepOldBuffer = true;
						pContinue->fDataBuff	= pInContinue->fDataBuff; //save the old buffer
						pInContinue->fDataBuff	= nil; //clean up separately in this case
					}
					else
					{
						pContinue->fDataBuff = ::dsDataBufferAllocatePriv( inData->fOutDataBuff->fBufferSize );
						if ( pContinue->fDataBuff == nil ) throw( (SInt32)eMemoryAllocError );
					}
					//pInContinue->fDataBuff will get freed below in gSNContinue->RemoveItem
				}
				else
				{
					pContinue->fDataBuff	= pInContinue->fDataBuff;
					pInContinue->fDataBuff	= nil;
				}
				pContinue->fContextData		= pInContinue->fContextData;
				pContinue->fLimitRecSearch	= pInContinue->fLimitRecSearch;
				pContinue->fTotalRecCount	= pInContinue->fTotalRecCount;
				pContinue->bNodeBuffTooSmall= pInContinue->bNodeBuffTooSmall;
				

				// RemoveItem calls our ContinueDeallocProc to clean up
				// since we transfered ownership of these pointers we need to make sure they
				// are nil so the ContinueDeallocProc doesn't free them now
				pInContinue->fContextData		= nil;
				gSNContinue->RemoveItem( inData->fIOContinueData );

				pInContinue = nil;
				inData->fIOContinueData = nil;

				runState = pContinue->fState;
			}
			else
			{
				throw( (SInt32)eDSInvalidContinueData );
			}
		}
		else
		{
			pContinue = (sSearchContinueData *)calloc( 1, sizeof( sSearchContinueData ) );
			if ( pContinue == nil ) throw( (SInt32)eMemoryAllocError );

			pContinue->fDataBuff = ::dsDataBufferAllocatePriv( inData->fOutDataBuff->fBufferSize );
			if ( pContinue->fDataBuff == nil ) throw( (SInt32)eMemoryAllocError );

			siResult = GetNextNodeRef( 0, &pContinue->fNodeRef, pContext );
			if ( siResult != eDSNoErr ) throw( siResult );
			
			pContinue->fDirRef = fDirRef;
			
			pContinue->fRecIndex		= 1;
			pContinue->fTotalRecCount	= 0;
			pContinue->fLimitRecSearch	= 0;
			pContinue->bNodeBuffTooSmall= false;
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pContinue
			if (inData->fOutMatchRecordCount >= 0)
			{
				pContinue->fLimitRecSearch = inData->fOutMatchRecordCount;
			}
		}

		// Empty the out buffer
		siResult = inOutBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = inOutBuff.SetBuffType( 'StdA' );
		if ( siResult != eDSNoErr ) throw( siResult );

		inData->fIOContinueData			= nil;
		//need to return zero if no records found
		inData->fOutMatchRecordCount	= 0;
		

		while ( !done )
		{
			// Do the task
			switch ( runState )
			{
				// Get the original record list request
				case keGetRecordList:
				{
					if (pContinue->fLimitRecSearch > pContinue->fTotalRecCount)
					{
						recCount = pContinue->fLimitRecSearch - pContinue->fTotalRecCount;
					}
					else
					{
						recCount = 0;
					}
					if ( inData->fType == kDoMultipleAttributeValueSearchWithData )
					{
						siResult = ::dsDoMultipleAttributeValueSearchWithData(
																pContinue->fNodeRef,
																pContinue->fDataBuff,
																inData->fInRecTypeList,
																inData->fInAttrType,
																inData->fInPattMatchType,
																inData->fInPatterns2MatchList,
																(pContinue->bIsAugmented && pContinue->fAugmentReqAttribs ? pContinue->fAugmentReqAttribs : inData->fInAttrTypeRequestList),
																inData->fInAttrInfoOnly,
																&recCount,
																&pContinue->fContextData );

					}
					else
					{
						siResult = ::dsDoMultipleAttributeValueSearch(	
																pContinue->fNodeRef,
																pContinue->fDataBuff,
																inData->fInRecTypeList,
																inData->fInAttrType,
																inData->fInPattMatchType,
																inData->fInPatterns2MatchList,
																&recCount,
																&pContinue->fContextData );
					}

					pContinue->fRecCount	= recCount;
					pContinue->fRecIndex	= 1;

					lastState = keGetRecordList;
				}
				break;

				// Add any data from the original record request to our own
				//	buffer format
				case keAddDataToBuff:
				{
					if ( inData->fType == kDoMultipleAttributeValueSearchWithData )
					{
						siResult = AddDataToOutBuff( pContinue, &inOutBuff, pContext, inData->fInAttrTypeRequestList );
					}
					else
					{
						siResult = AddDataToOutBuff( pContinue, &inOutBuff, pContext, NULL );
					}
					if (bKeepOldBuffer)
					{
						if (siResult == eDSNoErr)
						{
							if ( pContinue->fDataBuff != nil )
							{
								::dsDataBufferDeallocatePriv( pContinue->fDataBuff );
								pContinue->fDataBuff = nil;
							}
							pContinue->fDataBuff = ::dsDataBufferAllocatePriv( inData->fOutDataBuff->fBufferSize );
							if ( pContinue->fDataBuff == nil ) throw( (SInt32)eMemoryAllocError );
							bKeepOldBuffer = false;
						}
					}
					lastState = keAddDataToBuff;
				}
				break;
				
				case keGetNextNodeRef:
				{
					siResult	= GetNextNodeRef( pContinue->fNodeRef, &pContinue->fNodeRef, pContext );
					lastState	= keGetNextNodeRef;

					if ( inData->fType == kDoMultipleAttributeValueSearchWithData && siResult == eDSNoErr )
						UpdateContinueForAugmented( pContext, pContinue, inData->fInAttrTypeRequestList );
				}
				break;

				case keSetContinueData:
				{
					switch ( lastState )
					{
						case keAddDataToBuff:
							inOutBuff.GetDataBlockCount( &inData->fOutMatchRecordCount );
							//KW add to the total rec count what is going out for this call
							pContinue->fTotalRecCount += inData->fOutMatchRecordCount;
							pContinue->fState = lastState;
							inData->fIOContinueData	= pContinue;
							gSNContinue->AddItem( pContinue, inData->fInNodeRef );
							siResult = eDSNoErr;
							break;

						case keGetRecordList:
						case keGetNextNodeRef:
							inOutBuff.GetDataBlockCount( &inData->fOutMatchRecordCount );
							//KW add to the total rec count what is going out for this call
							pContinue->fTotalRecCount += inData->fOutMatchRecordCount;
							pContinue->fState = keGetRecordList;
							if ( siResult == keSearchNodeListEnd )
							{
								siResult = eDSNoErr;
								inData->fIOContinueData = nil;
							}
							else
							{
								inData->fIOContinueData = pContinue;
								gSNContinue->AddItem( pContinue, inData->fInNodeRef );
							}
							break;

						case keBufferTooSmall:
							if (pContinue->fContextData == nil) //buffer too small in search node itself
							{
								pContinue->fState = keAddDataToBuff;
								siResult = eDSBufferTooSmall;
							}
							else //buffer too small in a search path node
							{
								//could be zero too
								inOutBuff.GetDataBlockCount( &inData->fOutMatchRecordCount );
								//KW add to the total rec count what is going out for this call
								pContinue->fTotalRecCount += inData->fOutMatchRecordCount;
								if (inData->fOutMatchRecordCount == 0)
								{
									siResult = eDSBufferTooSmall;
								}
								else
								{
									pContinue->bNodeBuffTooSmall= true;
									siResult = eDSNoErr;
								}

								// if we have something in the buffer still, lets be sure we don't lose it
								if( pContinue->fRecIndex <= pContinue->fRecCount )
									pContinue->fState = keAddDataToBuff;
								else
									pContinue->fState = keGetRecordList;
							}
							inData->fIOContinueData	= pContinue;
							gSNContinue->AddItem( pContinue, inData->fInNodeRef );
							break;

						default:
							DbgLog( kLogPlugin, "*** Invalid continue state = %l", lastState );
							break;
					}
				}
				break;

				case keDone:
				{
					if ( pContinue != nil )
					{
						CSearchPlugin::ContinueDeallocProc( pContinue );
						pContinue = nil;
					}
					done = true;
				}
				break;

				default:
				{
					DbgLog( kLogPlugin, "*** Unknown run state = %l", runState );
					done = true;
				}
				break;

			} // switch for run state



			// *** Change State ***

			switch ( runState )
			{
				case keGetRecordList:
				{
					// Did dsGetRecordList succeed 
					if ( siResult == eDSNoErr )
					{
						// Did we find any records
						if ( pContinue->fRecCount != 0 )
						{
							// We found records, add them to our out buff
							runState = keAddDataToBuff;
						}
						else if (pContinue->fContextData == nil)
						{
							runState = keGetNextNodeRef;
						}
					}
					//condition on eDSRecordNotFound will no longer be needed
					else if ( siResult == eDSRecordNotFound ||
							  siResult == eDSInvalidRecordName ||
							  siResult == eDSInvalidRecordType ||
							  siResult == eDSNoStdMappingAvailable )
						  //move on to the next node if these
						  //conditions are met
					{
						// No records were found on this node
						runState = keGetNextNodeRef;
						//we need to ensure that continue data from previous node is NOT sent to the next node and that it is released
						if (siResult == eDSInvalidRecordType)
						{
							if (pContinue->fContextData != nil)
							{
								//release the continue data
								dsReleaseContinueData(pContinue->fNodeRef, pContinue->fContextData);
								pContinue->fContextData = 0;
							}
						}
					}
					else if (siResult == eDSBufferTooSmall)
					{
						lastState	= keBufferTooSmall;
						runState	= keSetContinueData;
					}
					else //move on to the next node
					{
						runState = keGetNextNodeRef;
					}
				}
				break;

				case keAddDataToBuff:
				{
					UInt32 aRecCnt = 0;
					inOutBuff.GetDataBlockCount(&aRecCnt);
					// Did we add all records to our buffer 
					if ( ( siResult == eDSNoErr ) || ( ( siResult == CBuff::kBuffFull ) && (aRecCnt > 0) ) )
					{
						inData->fOutMatchRecordCount = aRecCnt;
							
						//check if we retrieved all that was requested
						//continue data might even be nil here
						if ((pContinue->fLimitRecSearch <= (pContinue->fTotalRecCount + inData->fOutMatchRecordCount)) &&
								(pContinue->fLimitRecSearch != 0))
						{
							//KW would seem that setting continue data when we know we are done is wrong
							//runState = keSetContinueData;
							
							//KW add to the total rec count what is at least going out for this call
							pContinue->fTotalRecCount += inData->fOutMatchRecordCount;
							
							//KW don't know why we need this continue data anymore?
							pContinue->fState = runState;
							//gSNContinue->AddItem( pContinue, inData->fInNodeRef );
							runState = keDone;
							inData->fIOContinueData	= nil;
							siResult = eDSNoErr;
						}
						else
						{
							if ( siResult == CBuff::kBuffFull )
							{
								runState = keSetContinueData;
							}
							// Do we need to continue the original read
							else if ( pContinue->fContextData )
							{
								lastState = keGetRecordList;
								//runState = keSetContinueData;
								runState = keGetRecordList;
							}
							else
							{
								runState = keGetNextNodeRef;
							}
						}
					}
					else
					{
						if ( siResult == CBuff::kBuffFull )
						{
							runState = keSetContinueData;
							lastState = keBufferTooSmall;
						}
						else
						{
							// We got an error we don't know how to deal with, we be gone
							runState = keDone;
						}
					}
				}
				break;

				case keGetNextNodeRef:
				{
					inOutBuff.GetDataBlockCount( &recCount );
					if ( siResult == eDSNoErr )
					{
						if ( recCount == 0 )
						{
							runState = keGetRecordList;
						}
						else
						{
							runState = keSetContinueData;
						}
					}
					else
					{
						if ( siResult == keSearchNodeListEnd )
						{
							runState = keSetContinueData;
						}
						else
						{
							runState = keDone;
						}
					}
				}
				break;

				case keSetContinueData:
				case keDone:
				case keError:
				{
					done = true;
				}
				break;

				default:
				{
					DbgLog( kLogPlugin, "*** Unknown transition state = %l", runState );
					done = true;
				}
				break;

			} // switch for transition state
		}
		
		pContext->pSearchListMutex->SignalLock();
		
	}

	catch( SInt32 err )
	{
		if ( (pContext != nil) && (pContext->pSearchListMutex != nil) )
		{
			pContext->pSearchListMutex->SignalLock();
		}
		siResult = err;
	}

	if ( (inData->fIOContinueData == nil) && (pContinue != nil ) )
	{
		// we have decided not to return contine data, need to free it
		CSearchPlugin::ContinueDeallocProc( pContinue );
		pContinue = nil;
	}
	
	return( siResult );

} // MultipleAttributeValueSearch

//------------------------------------------------------------------------------------
//	SystemGoingToSleep
//------------------------------------------------------------------------------------

void CSearchPlugin::SystemGoingToSleep( void )
{
	//set a network change blocking flag at sleep
	OSAtomicTestAndSet( 0, &gSystemGoingToSleep );
	
	sSearchConfig	*aSearchConfig		= NULL;
	sSearchList		*aSearchNodeList	= NULL;
	sSearchList		*aNodeListPtr		= NULL;
	
	//mutex -- get the node list to flag all of the nodes offline and update the cache the same
	fMutex.WaitLock();
	
	aSearchConfig = FindSearchConfigWithKey(eDSAuthenticationSearchNodeName);
	if ( aSearchConfig != nil )
	{
		aSearchNodeList = aSearchConfig->fSearchNodeList;
		if (aSearchNodeList != nil)
		{
			aNodeListPtr = aSearchNodeList->fNext; //always skip the local node(s)
			aNodeListPtr = aNodeListPtr->fNext; //bsd is always 2nd now
			while (aNodeListPtr != nil)
			{
				if (aNodeListPtr->fNodeName != nil)
				{
					aNodeListPtr->fNodeReachable = false;
					if ( gCacheNode != NULL )
					{
						DBGLOG1( kLogPlugin, "CSearchPlugin::SystemGoingToSleep updating node reachability <%s> to <Unavailable>", 
								 aNodeListPtr->fNodeName );
						gCacheNode->UpdateNodeReachability( aNodeListPtr->fNodeName, false );
					}
				}
				aNodeListPtr = aNodeListPtr->fNext;
			}
		}
	}		
	
	fMutex.SignalLock();
}//SystemGoingToSleep

//------------------------------------------------------------------------------------
//	SystemWillPowerOn
//------------------------------------------------------------------------------------

void CSearchPlugin::SystemWillPowerOn( void )
{
	//reset a network change blocking flag at wake
	DBGLOG( kLogPlugin, "CSearchPlugin::SystemWillPowerOn request" );
	OSAtomicTestAndClear( 0, &gSystemGoingToSleep );
	EnsureCheckNodesThreadIsRunning( eDSAuthenticationSearchNodeName ); // ensure our thread is running
	EnsureCheckNodesThreadIsRunning( eDSContactsSearchNodeName ); // ensure our thread is running
}//SystemWillPowerOn

#pragma mark -
#pragma mark Support Routines
#pragma mark -

void CSearchPlugin::UpdateContinueForAugmented( sSearchContextData *inContext, sSearchContinueData *inContinue, tDataListPtr inAttrTypeRequestList )
{
	// let's see if the required attributes exist, inAttrTypeRequestList can be null for all attributes
	if ( IsAugmented(inContext, inContinue->fNodeRef) == true && inAttrTypeRequestList != NULL )
	{
		bool			bAddRecordName	= true;
		
		inContinue->bIsAugmented = true;
		
		tDataBufferPriv *pPrivData = (tDataBufferPriv *) (inAttrTypeRequestList != NULL ? inAttrTypeRequestList->fDataListHead : NULL);
		while ( pPrivData != NULL )
		{
			if ( strcmp(pPrivData->fBufferData, kDSAttributesStandardAll) == 0 || 
				 strcmp(pPrivData->fBufferData, kDSAttributesAll) == 0 ||
				 strcmp(pPrivData->fBufferData, kDSNAttrRecordName) == 0 )
			{
				bAddRecordName = false;
				break;
			}
			
			pPrivData = (tDataBufferPriv *) pPrivData->fNextPtr;
		}
		
		if ( bAddRecordName == true )
		{
			inContinue->fAugmentReqAttribs = dsDataListCopyList( 0, inAttrTypeRequestList );
			
			dsAppendStringToListAllocPriv( inContinue->fAugmentReqAttribs, kDSNAttrRecordName );
		}
	}
	else
	{
		inContinue->bIsAugmented = false;
	}	
}

bool CSearchPlugin::IsAugmented( sSearchContextData *inContext, tDirNodeReference inNodeRef )
{
	//need to determine if we should attempt to retrieve an augment record here
	//check for augment setting AND for correct searched on node
	if ( inContext != NULL && inNodeRef != 0 && inContext->pConfigFromXML != NULL && inContext->pConfigFromXML->AugmentSearch() )
	{
		//check and ensure that the augmenting node is also on the search policy here
		char	*augmentingNodeName		= inContext->pConfigFromXML->AugmentDirNodeName();
		bool	bAugmentOK				= false;
		char	*thisNodeName			= NULL;
		
		//figure out what node this data is from
		sSearchList *aList = inContext->fSearchNodeList;
		while ( aList != NULL )
		{
			if ( aList->fNodeRef == inNodeRef )
				thisNodeName = aList->fNodeName;
			
			if ( aList->fNodeName != NULL && strcmp(aList->fNodeName, augmentingNodeName) == 0 )
				bAugmentOK = true;
			
			aList = aList->fNext;
		}
		
		if ( bAugmentOK && thisNodeName != NULL && strcmp(thisNodeName, inContext->pConfigFromXML->ToBeAugmentedDirNodeName()) == 0 )
			return true;
	}
	
	return false;
}
//------------------------------------------------------------------------------------
//	* GetNextNodeRef
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::GetNextNodeRef ( tDirNodeReference inNodeRef, tDirNodeReference *outNodeRef, sSearchContextData *inContext )
{
	SInt32				siResult		= keSearchNodeListEnd;
	sSearchList		   *pNodeList		= nil;
	tDirNodeReference	aNodeRef		= inNodeRef;
	UInt32				nodeIndex		= 0;
	bool				bFailedToOpen	= false;

	pNodeList = (sSearchList *)inContext->fSearchNodeList;
	
	// Search the node list looking for the current node ref
	if (aNodeRef != 0) //if it is zero we look at the first one
	{
		while ( pNodeList != nil )
		{
			nodeIndex++;
			if ( aNodeRef == pNodeList->fNodeRef )
			{
				pNodeList = pNodeList->fNext;
				break;
			}
			pNodeList = pNodeList->fNext;
		}
	}

	if ( (nodeIndex == 0) || (nodeIndex == 1) ) //local node and local BSD node in all cases should always be reachable
	{
		pNodeList->fNodeReachable = true;
		pNodeList->fHasNeverOpened = false;
	}

	//no more attempt to enhance connectability to netinfo hierarchy
	
	//look over the remainder of the list to find the next successful open or simply finish
	while ( pNodeList != nil )
	{
		if (pNodeList->fNodeReachable)
		{
			// Has the node been previously opened
			if ( pNodeList->fOpened == false )
			{
				siResult = ::dsOpenDirNode( fDirRef, pNodeList->fDataList, &pNodeList->fNodeRef );
				if ( siResult == eDSNoErr )
				{
					*outNodeRef = pNodeList->fNodeRef;
					pNodeList->fOpened = true;
					break;
				}
				else
				{
					siResult = keSearchNodeListEnd;
					fSomeNodeFailedToOpen = true;
					bFailedToOpen = true;
				}
			}
			else
			{
				*outNodeRef	= pNodeList->fNodeRef;
				siResult	= eDSNoErr;
				break;
			}
		}
		pNodeList = pNodeList->fNext;
	}
	
	// if something failed to open, lets ensure the thread to check is running
	if ( bFailedToOpen == true )
		EnsureCheckNodesThreadIsRunning( (tDirPatternMatch) inContext->fSearchConfigKey );
	
	return( siResult );

} // GetNextNodeRef


//------------------------------------------------------------------------------------
//	* GetNodePath
//------------------------------------------------------------------------------------

tDataList* CSearchPlugin::GetNodePath ( tDirNodeReference inNodeRef, sSearchContextData *inContext )
{
	tDataList	   *pResult		= nil;
	sSearchList	   *pNodeList	= nil;

//do we check ??? whether search policy has switched and if it has then adjust to the new one
//ie. in this method we are in the middle of a getrecordlist or doattributevaluesearch(withdata)
		
	pNodeList = (sSearchList *)inContext->fSearchNodeList;

	// Search the node list looking for the current node ref
	while ( pNodeList != nil )
	{
		// Is it the one we are looking for
		if ( inNodeRef == pNodeList->fNodeRef )
		{
			pResult = pNodeList->fDataList;
			break;
		}
		pNodeList = pNodeList->fNext;
	}

	return( pResult );

} // GetNodePath


// ---------------------------------------------------------------------------
//	* MakeContextData
// ---------------------------------------------------------------------------

sSearchContextData* CSearchPlugin::MakeContextData ( void )
{
	sSearchContextData	*pOut	= nil;

	pOut = (sSearchContextData *) calloc( 1, sizeof(sSearchContextData) );
	if ( pOut != nil )
	{
		pOut->fSearchNodeList	= nil;
		pOut->bListChanged		= false;
		pOut->pSearchListMutex	= nil;
		pOut->fSearchNode		= this;
		pOut->bAutoSearchList	= false;
		pOut->bCheckForNIParentNow = false;
#if AUGMENT_RECORDS
		pOut->pConfigFromXML	= nil;
#endif
	}

	return( pOut );

} // MakeContextData


// ---------------------------------------------------------------------------
//	* CleanContextData
// ---------------------------------------------------------------------------

SInt32 CSearchPlugin::CleanContextData ( sSearchContextData *inContext )
{
    SInt32				siResult 	= eDSNoErr;
	DSMutexSemaphore   *ourMutex	= nil;
    
    if (( inContext == nil ) || ( gSearchNode == nil ))
    {
        siResult = eDSBadContextData;
	}
    else
    {
		ourMutex = inContext->pSearchListMutex;
		if (ourMutex != nil)
		{
			//gSearchNode->fMutex.WaitLock();
			ourMutex->WaitLock();

			//need a handle to the pConfigFromXML class
			//cheat by using the global since all we want is the function
			//pSearchConfigList->pConfigFromXML->CleanListData( xxxxx );
        	if (inContext->fSearchNodeList != nil && inContext->fSearchNode != nil)
        	{
				gSearchNode->CleanSearchListData( inContext->fSearchNodeList );
				inContext->fSearchNodeList = nil;
			}

			OSAtomicCompareAndSwap32Barrier(true, false, &inContext->bListChanged);
			inContext->offset			= 0;
			inContext->fSearchConfigKey	= 0;
			inContext->pSearchListMutex	= nil;
			inContext->bAutoSearchList	= false;
			inContext->bCheckForNIParentNow = false;
#if AUGMENT_RECORDS
			inContext->pConfigFromXML	= nil; //not owned by this struct
#endif
			
			//ourMutex->SignalLock(); //we are going to delete this here - don't make it available
			delete(ourMutex);
			ourMutex = nil;
			//gSearchNode->fMutex.SignalLock();
		}
		//only node refs have a mutex assigned so always free this
		free( inContext );
		inContext = nil;
	}
		
	return( siResult );

} // CleanContextData

// ---------------------------------------------------------------------------
//	* AddDataToOutBuff
// ---------------------------------------------------------------------------

SInt32 CSearchPlugin::AddDataToOutBuff ( sSearchContinueData *inContinue, CBuff *inOutBuff, sSearchContextData *inContext, tDataListPtr inRequestedAttrList )
{
	UInt32					i				= 1;
	UInt32					j				= 1;
	SInt32					attrCnt			= 0;
	SInt32					siResult		= eDSNoErr;
	char				   *cpRecType		= nil;
	char				   *cpRecName		= nil;
	tRecordEntry		   *pRecEntry		= nil;
	tAttributeListRef		attrListRef		= 0;
	tAttributeValueListRef	valueRef		= 0;
	tAttributeEntry		   *pAttrEntry		= nil;
	tAttributeValueEntry   *pValueEntry		= nil;
	CDataBuff			   *aRecData		= nil;
	CDataBuff			   *aAttrData		= nil;
	CDataBuff			   *aTmpData		= nil;
#if AUGMENT_RECORDS
	bool					bAugmentThisNode= false;
	bool					bAskedForGUIDinAugment	= false;
	char*					augGUIDValue	= nil;
	CFMutableArrayRef		realNodeAttrList= NULL;
	CFArrayRef				anArray			= NULL;
	CFMutableArrayRef		augNodeAttrList	= NULL;
	char*					realNodeGUID	= nil;
	char*					recTypeSuffix	= nil;
#endif

	try
	{

		aRecData	= new CDataBuff();
		aAttrData	= new CDataBuff();
		aTmpData	= new CDataBuff();
		
		while ( (inContinue->fRecIndex <= inContinue->fRecCount) && (siResult == eDSNoErr) )
		{
			attrCnt = 0;
			siResult = ::dsGetRecordEntry( inContinue->fNodeRef, inContinue->fDataBuff, inContinue->fRecIndex, &attrListRef, &pRecEntry );
			if ( siResult != eDSNoErr ) throw( siResult );

			siResult = ::dsGetRecordTypeFromEntry( pRecEntry, &cpRecType );
			if ( siResult != eDSNoErr ) throw( siResult );

			siResult = ::dsGetRecordNameFromEntry( pRecEntry, &cpRecName );
			if ( siResult != eDSNoErr ) throw( siResult );

			aRecData->Clear();
			aAttrData->Clear();
			aTmpData->Clear();

			// Set the record type and name
			aRecData->AppendShort( ::strlen( cpRecType ) );
			aRecData->AppendString( cpRecType );
			aRecData->AppendShort( ::strlen( cpRecName ) );
			aRecData->AppendString( cpRecName );

//we know to always get the data from the real node regardless of augmentation
//we also need to know if the request was kDSAttributesAll or specific attributes
//in either case we also need to track which potential augment attrs have values in the real node and do NOT replace them below
//for the all case we need to only that above
//for the specific requested atributes case we also need to know if any of the augment attrs were actually requested

#if AUGMENT_RECORDS
			//need to determine if we should attempt to retrieve an augment record here
			//check for augment setting AND for correct searched on node
			if ( IsAugmented(inContext, inContinue->fNodeRef) == true )
			{
				//next check in the dictionary if this is a record type we want to augment
				CFDictionaryRef aDict = inContext->pConfigFromXML->AugmentAttrListDict();
				if (aDict != NULL)
				{
					if (strncmp(cpRecType, kDSStdRecordTypePrefix, sizeof(kDSStdRecordTypePrefix)-1) == 0)
					{
						// skip past the prefix
						recTypeSuffix = &cpRecType[sizeof(kDSStdRecordTypePrefix)-1];
					}
					
					//if it is not a proper standard type then we will not augment the record
					if (recTypeSuffix != NULL)
					{
						CFStringRef cfRecType = CFStringCreateWithCString( NULL, cpRecType, kCFStringEncodingUTF8 );
						anArray = (CFArrayRef)CFDictionaryGetValue( aDict, cfRecType );
						DSCFRelease(cfRecType);
						
						if (anArray != NULL ) //found that we want to augment this record type
						{
							bAugmentThisNode = true;
							CFIndex arrayCnt = CFArrayGetCount(anArray);
							if (arrayCnt > 0)
							{
								if (fAugmentNodeRef == 0)
								{
									//open the node which contains the augment records in it
									char* nodeName = inContext->pConfigFromXML->AugmentDirNodeName();
									if (nodeName != nil)
									{
										tDataListPtr aNodeName = dsBuildFromPathPriv( nodeName, "/" );
										siResult = dsOpenDirNode( fDirRef, aNodeName, &fAugmentNodeRef );
										dsDataListDeallocatePriv( aNodeName );
										free( aNodeName );
									}					
								}
								
							}//if (arrayCnt > 0)
						}//if (anArray != NULL)
					}//recTypeSuffix != NULL
				}//aDict != NULL
			}
#endif

			if ( pRecEntry->fRecordAttributeCount != 0 )
			{
				attrCnt += pRecEntry->fRecordAttributeCount;
				for ( i = 1; i <= pRecEntry->fRecordAttributeCount; i++ )
				{
					siResult = ::dsGetAttributeEntry( inContinue->fNodeRef, inContinue->fDataBuff, attrListRef, i, &valueRef, &pAttrEntry );
					if ( siResult != eDSNoErr ) throw( siResult );
					
#if AUGMENT_RECORDS
					if (bAugmentThisNode)
					{
						// if inContinue->bIsAugmented is set, see if this is a record name attribute, if so, skip the attribute
						// because we added it even though it wasn't requested in order ensure augments worked
						if ( inContinue->bIsAugmented && inContinue->fAugmentReqAttribs != NULL )
						{
							// if so, just continue to next attribute
							if ( strcmp(pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName) == 0 )
							{
								attrCnt--; // decrement count since we aren't adding it
								goto skipRecord;
							}
						}

						if (realNodeAttrList == NULL)
							realNodeAttrList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
						
						CFStringRef cfTempString = CFStringCreateWithCString( kCFAllocatorDefault, pAttrEntry->fAttributeSignature.fBufferData, 
																			  kCFStringEncodingUTF8 );
						CFArrayAppendValue( realNodeAttrList, cfTempString );
						DSCFRelease( cfTempString );
					}
#endif

					aTmpData->AppendShort( ::strlen( pAttrEntry->fAttributeSignature.fBufferData ) );
					aTmpData->AppendString( pAttrEntry->fAttributeSignature.fBufferData );

					if ( inContinue->fAttrOnly == false )
					{
						aTmpData->AppendShort( pAttrEntry->fAttributeValueCount );

						for ( j = 1; j <= pAttrEntry->fAttributeValueCount; j++ )
						{
							siResult = dsGetAttributeValue( inContinue->fNodeRef, inContinue->fDataBuff, j, valueRef, &pValueEntry );
							if ( siResult != eDSNoErr ) throw( siResult );
							
#if AUGMENT_RECORDS
							if (bAugmentThisNode)
							{
								if (strcmp(pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrGeneratedUID) == 0)
								{
									//not forcing retrieval of this attr so this can be used as a failure check but not an absolute match check
									realNodeGUID = strdup(pValueEntry->fAttributeValueData.fBufferData);
								}
							}
#endif
							aTmpData->AppendLong( pValueEntry->fAttributeValueData.fBufferLength );
							aTmpData->AppendBlock( pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
							dsDeallocAttributeValueEntry(fDirRef, pValueEntry);
							pValueEntry = nil;
						}
					}
					else
					{
						aTmpData->AppendShort( 0 );
					}
					aAttrData->AppendLong( aTmpData->GetLength() );
					aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

#if AUGMENT_RECORDS
				skipRecord:
#endif		
					// Clear the temp block
					aTmpData->Clear();
					
					dsCloseAttributeValueList(valueRef);
					dsDeallocAttributeEntry(fDirRef, pAttrEntry);
					pAttrEntry = nil;
				}
			}


#if AUGMENT_RECORDS			
			if ( (fAugmentNodeRef != 0) && bAugmentThisNode )
			{
				tDataListPtr	augmentRecName		= nil;
				tDataListPtr	augmentRecType		= nil;
				tDataListPtr	augmentAttrTypes	= nil;
				UInt32			augmentRecCount		= 1; // only care about first match
				tDataBufferPtr	dataBuff			= nil;
				tContextData	augmentContext		= nil;
				
				dataBuff = dsDataBufferAllocate( fDirRef, 1024 );
				if ( dataBuff == nil ) throw( (SInt32)eMemoryAllocError );
								
				//build the augment record name by decided convention
				//TODO perhaps later this is only a partial and GUID is also added to the end so then we could search with eDSStartsWith
				char * cpAugmentRecName = (char *)calloc(1, strlen(cpRecName) + 1 + strlen(recTypeSuffix) + 1);
				strcpy(cpAugmentRecName, recTypeSuffix);
				strcat(cpAugmentRecName, ":");
				strcat(cpAugmentRecName, cpRecName);
				
				augmentRecName = dsBuildListFromStrings( fDirRef, cpAugmentRecName, NULL );				
				augmentRecType = dsBuildListFromStrings( fDirRef, kDSStdRecordTypeAugments, NULL );
				//we can build a specific list of what to ask for since we know this above
				//only ask for GUID if we have it above so we can verify this is the correct augment record
				
				//examine the variations on the list of attrs now as anArray holds the dictionary list
				//and realNodeAttrList holds what was retrieved already and inRequestedAttrList is the requested list
				//make another CFMutableArrayRef for the tDataList
				CFMutableArrayRef reqAttrList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
				bool bReqAll = false;
				if (inRequestedAttrList == NULL)
				{
					bReqAll = true;
				}
				else
				{
					CAttributeList* cpAttrTypeList = new CAttributeList( inRequestedAttrList );
					if ( cpAttrTypeList  == nil ) throw( (SInt32)eDSEmptyAttributeTypeList );
					if (cpAttrTypeList->GetCount() == 0) 
					{
						DSCFRelease( reqAttrList );
						DSDelete( cpAttrTypeList );
						throw( (SInt32)eDSEmptyAttributeTypeList );
					}
					if (cpAttrTypeList->GetCount() == 1)
					{
						char* pAttrType = nil;
						cpAttrTypeList->GetAttribute( 1, &pAttrType );
						if (pAttrType != nil)
						{
							if (strcmp(pAttrType, kDSAttributesAll) == 0 )
							{
								bReqAll = true;
							}
						}
					}
					if (!bReqAll)
					{
						//go thru the list and build a CFArray
						for (UInt32 anIndex = 1; anIndex <= cpAttrTypeList->GetCount(); anIndex++)
						{
							char* aString = nil;
							cpAttrTypeList->GetAttribute( anIndex, &aString );
							if (aString != NULL)
							{
								CFStringRef aCFString = CFStringCreateWithCString( kCFAllocatorDefault, aString, kCFStringEncodingUTF8 );
								CFArrayAppendValue(reqAttrList, aCFString);
								DSCFRelease(aCFString);
							}
						}
					}
					
					DSDelete( cpAttrTypeList );
				}	
				
				augNodeAttrList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
				if ( anArray != NULL && realNodeAttrList != NULL )
				{
					for (CFIndex arrayIndex = 0; arrayIndex < CFArrayGetCount(anArray); arrayIndex++)
					{
						CFStringRef anItem = (CFStringRef)CFArrayGetValueAtIndex(anArray, arrayIndex);
						if (anItem != NULL)
						{
							//is it not found already in the real node
							if ( CFArrayContainsValue(realNodeAttrList, CFRangeMake(0, CFArrayGetCount(realNodeAttrList)), anItem) == FALSE )
							{
								if (bReqAll)
								{
									CFArrayAppendValue(augNodeAttrList, anItem);
								}
								else if (augNodeAttrList != NULL)
								{
									if ( CFArrayContainsValue(reqAttrList, CFRangeMake( 0, ::CFArrayGetCount( reqAttrList ) ), anItem) )
									{
										CFArrayAppendValue(augNodeAttrList, anItem);
									}
								}
							}
						}// if (anItem != NULL)
					}
				}// if (anArray != NULL)
				bool firstAddedToList = false;
				if (realNodeGUID != nil)
				{
					augmentAttrTypes = dsBuildListFromStrings( fDirRef, kDS1AttrGeneratedUID, NULL );
					firstAddedToList = true;
				}
				CFIndex arrayCnt = CFArrayGetCount(augNodeAttrList);
				for (CFIndex arrayIndex = 0; arrayIndex < arrayCnt; arrayIndex++)
				{
				// can't depend upon CFStringGetCStringPtr
					const char * aString = CFStringGetCStringPtr( (CFStringRef)CFArrayGetValueAtIndex(augNodeAttrList, arrayIndex), kCFStringEncodingUTF8 );
					char* ioCStr = nil;
					if (aString == NULL)
					{
						CFStringRef aCFStr = (CFStringRef)CFArrayGetValueAtIndex(augNodeAttrList, arrayIndex);
						CFIndex maxCStrLen = CFStringGetMaximumSizeForEncoding( CFStringGetLength( aCFStr ), kCFStringEncodingUTF8 ) + 1;
						size_t ioCStrSize = 0;
						ioCStr = (char*)calloc( 1, maxCStrLen );
						ioCStrSize = maxCStrLen;
						if( CFStringGetCString( aCFStr, ioCStr, ioCStrSize, kCFStringEncodingUTF8 ) )
						{
							aString = ioCStr;
						}
					}
					if (aString != NULL)
					{
						if (strcmp(aString, kDS1AttrGeneratedUID) != 0)
						{
							if (firstAddedToList)
							{
								dsAppendStringToListAlloc(0, augmentAttrTypes, aString );
							}
							else
							{
								augmentAttrTypes = dsBuildListFromStrings( fDirRef, aString, NULL );
								firstAddedToList = true;
							}
						}
						else
						{
							bAskedForGUIDinAugment = true;
						}
					}
					DSFreeString(ioCStr);
				}
				
				// we're done with these
				DSCFRelease( reqAttrList );
				DSCFRelease( augNodeAttrList );
				
				//if we decide not to augment it is a one level check on the augmentAttrTypes being empty for internal dispatch ie. -14212
				do 
				{
					siResult =	dsGetRecordList(	fAugmentNodeRef,
													dataBuff,
													augmentRecName,
													eDSExact, //eDSStartsWith
													augmentRecType,
													augmentAttrTypes,
													inContinue->fAttrOnly,
													&augmentRecCount,
													&augmentContext);
					if (siResult == eDSBufferTooSmall)
					{
						UInt32 bufSize = dataBuff->fBufferSize;
						dsDataBufferDeallocatePriv( dataBuff );
						dataBuff = nil;
						dataBuff = ::dsDataBufferAllocate( fDirRef, bufSize * 2 );
					}
				} while ( (siResult == eDSBufferTooSmall) || ( (siResult == eDSNoErr) && (augmentRecCount == 0) && (augmentContext != nil) ) );
				DSFreeString( cpAugmentRecName );
				
				if ( augmentRecName != NULL ) {
					dsDataListDeallocatePriv( augmentRecName );
					DSFree( augmentRecName );
				}
				
				if ( augmentRecType != NULL ) {
					dsDataListDeallocatePriv( augmentRecType );
					DSFree( augmentRecType );
				}
				
				if ( augmentAttrTypes != NULL ) {
					dsDataListDeallocatePriv( augmentAttrTypes );
					DSFree( augmentAttrTypes );
				}

				tRecordEntry		   *pAugRecEntry		= nil;
				tAttributeListRef		attrAugListRef		= 0;
				tAttributeValueListRef	valueAugRef			= 0;
				tAttributeEntry		   *pAugAttrEntry		= nil;
				tAttributeValueEntry   *pAugValueEntry		= nil;

				if ( (siResult == eDSNoErr) && (augmentRecCount > 0) )
				{
					siResult = ::dsGetRecordEntry( fAugmentNodeRef, dataBuff, 1, &attrAugListRef, &pAugRecEntry );
					if ( (siResult == eDSNoErr) && (pAugRecEntry != nil) )
					{
						//index starts at one
						for ( i = 1; i <= pAugRecEntry->fRecordAttributeCount; i++ )
						{
							attrCnt++;
							siResult = ::dsGetAttributeEntry( fAugmentNodeRef, dataBuff, attrAugListRef, i, &valueAugRef, &pAugAttrEntry );
							if ( siResult != eDSNoErr ) break;
		
							aTmpData->AppendShort( ::strlen( pAugAttrEntry->fAttributeSignature.fBufferData ) );
							aTmpData->AppendString( pAugAttrEntry->fAttributeSignature.fBufferData );

							bool bAddAttr = true;
							bool bGetGUID = false;
							if ( inContinue->fAttrOnly == false )
							{
								// did we get a guid
								if ( ::strcmp( pAugAttrEntry->fAttributeSignature.fBufferData, kDS1AttrGeneratedUID ) == 0 )
								{
									if (!bAskedForGUIDinAugment)
									{
										bAddAttr = false;
										attrCnt--; //decrement total attr count
									}
									bGetGUID = true;
								}
								
								if (bAddAttr)
								{
									aTmpData->AppendShort( pAugAttrEntry->fAttributeValueCount );
								}

								for ( j = 1; j <= pAugAttrEntry->fAttributeValueCount; j++ )
								{
									siResult = dsGetAttributeValue( fAugmentNodeRef, dataBuff, j, valueAugRef, &pAugValueEntry );
									if ( siResult != eDSNoErr ) break;
									if (bAddAttr)
									{
										aTmpData->AppendLong( pAugValueEntry->fAttributeValueData.fBufferLength );
										aTmpData->AppendBlock( pAugValueEntry->fAttributeValueData.fBufferData, pAugValueEntry->fAttributeValueData.fBufferLength );
									}
									if (bGetGUID)
									{
										//get the GUID to possibly compare with to be augmented record
										augGUIDValue = strdup(pAugValueEntry->fAttributeValueData.fBufferData);
									}
									if ( pAugValueEntry != NULL )
									{
										dsDeallocAttributeValueEntry( fDirRef, pAugValueEntry );
										pAugValueEntry = NULL;
									}
								}

								if ( siResult != eDSNoErr )
									break;
							}
							else
							{
								aTmpData->AppendShort( 0 );
							}
							if (bAddAttr)
							{
								aAttrData->AppendLong( aTmpData->GetLength() );
								aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
							}

							// Clear the temp block
							aTmpData->Clear();
							dsCloseAttributeValueList(valueAugRef);
							if (pAugAttrEntry != nil)
							{
								dsDeallocAttributeEntry(fDirRef, pAugAttrEntry);
								pAugAttrEntry = nil;
							}
						}
					}
					dsCloseAttributeList(attrAugListRef);
					if (pAugRecEntry != nil)
					{
						dsDeallocRecordEntry(fDirRef, pAugRecEntry);
						pAugRecEntry = nil;
					}
				}// got records returned
				
				if ( dataBuff != NULL ) {
					dsDataBufferDeallocatePriv( dataBuff );
					dataBuff = NULL;
				}
			} //if (fAugmentNodeRef != 0)
			
			//TODO do we even want to do this as it lend additional complexity and a performance hit
			//if we have a GUID mismatch then we reset the attrCnt and cleanup the aAttrData ie. we could havve two different attrDatq blobs I guess
			//if ()
			//{
				//attrCnt = 0;
				//aAttrData->Clear();
			//}
#endif

			//clean up these strings here
			DSFreeString( cpRecName );
			DSFreeString( cpRecType );


			// Attribute count
			aRecData->AppendShort( attrCnt );

			if ( attrCnt != 0 )
			{
				aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
			}

			siResult = inOutBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
			if ( siResult == eDSNoErr )
			{
				inContinue->fRecIndex++;
			}
			
			dsCloseAttributeList(attrListRef);
			dsDeallocRecordEntry(fDirRef, pRecEntry);
			pRecEntry = nil;
			
#if AUGMENT_RECORDS
			DSCFRelease( realNodeAttrList ); // we don't keep this, it's per record
#endif
		} //while ( (inContinue->fRecIndex <= inContinue->fRecCount) && (siResult == eDSNoErr) )
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	if ( cpRecType != nil )
	{
		free( cpRecType );
		cpRecType = nil;
	}

	if ( cpRecName != nil )
	{
		free( cpRecName );
		cpRecName = nil;
	}

	if ( aRecData != nil )
	{
		delete(aRecData);
		aRecData = nil;
	}
	
	if ( aAttrData != nil )
	{
		delete(aAttrData);
		aAttrData = nil;
	}
	
	if ( aTmpData != nil )
	{
		delete(aTmpData);
		aTmpData = nil;
	}
	
#if AUGMENT_RECORDS
	DSFreeString(realNodeGUID);
	DSFreeString(augGUIDValue);
#endif

	return( siResult );

} // AddDataToOutBuff


//------------------------------------------------------------------------------------
//	  * DoPlugInCustomCall
//------------------------------------------------------------------------------------ 

SInt32 CSearchPlugin::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	SInt32						siResult		= eDSNoErr;
	bool						bAuthSwitched	= false;
	UInt32						aRequest		= 0;
	SInt32						xmlDataLength	= 0;
	CFDataRef					xmlData			= nil;
	CFDictionaryRef				dhcpLDAPdict	= nil;
	CFMutableArrayRef			cspArray		= nil;
	UInt32						bufLen			= 0;
	sSearchContextData			*pContext		= nil;
	sSearchConfig				*aSearchConfig	= nil;
	sSearchList					*nodeListPtr	= nil;
	AuthorizationRef			authRef			= 0;
	AuthorizationItemSet		*resultRightSet = NULL;
	AuthorizationExternalForm	blankExtForm;
	bool						verifyAuthRef	= true;
	CFRange						aRange;

	try
	{
		if ( inData == nil ) throw( (SInt32)eDSNullParameter );
		if ( inData->fInRequestData == nil ) throw( (SInt32)eDSNullDataBuff );
		if ( inData->fInRequestData->fBufferData == nil ) throw( (SInt32)eDSEmptyBuffer );
		
		pContext = (sSearchContextData *)gSNNodeRef->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (SInt32)eDSInvalidNodeRef );
		
		//stop the call if the call comes in for the DefaultNetwork Node
		if (pContext->fSearchConfigKey == eDSNetworkSearchNodeName) throw( (SInt32)eDSInvalidNodeRef );
		
		aRequest = inData->fInRequestCode;
		bufLen = inData->fInRequestData->fBufferLength;
		
		if (aRequest != eDSCustomCallSearchSubNodesUnreachable && aRequest != eDSCustomCallSearchCheckForAugmentRecord)
		{
			if ( pContext->fEffectiveUID == 0 )
			{
				if ( bufLen >= sizeof(AuthorizationExternalForm) )
				{
					bzero(&blankExtForm, sizeof(AuthorizationExternalForm));
					if (memcmp(inData->fInRequestData->fBufferData, &blankExtForm, sizeof(AuthorizationExternalForm)) == 0)
						verifyAuthRef = false;
				}
				else
				{
					verifyAuthRef = false;
				}
			}
			else
			{
				if ( bufLen < sizeof(AuthorizationExternalForm) )
					throw( (SInt32)eDSInvalidBuffFormat );
			}
			if (verifyAuthRef) {
				siResult = AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)inData->fInRequestData->fBufferData,
														&authRef);
				if (siResult != errAuthorizationSuccess)
				{
					DbgLog( kLogPlugin, "CSearchPlugin: AuthorizationCreateFromExternalForm returned error %d", siResult );
					syslog( LOG_ALERT, "Search Custom Call <%d> AuthorizationCreateFromExternalForm returned error %d", aRequest, siResult );
					throw( (SInt32)eDSPermissionError );
				}
		
				AuthorizationItem rights[] = { {"system.services.directory.configure", 0, 0, 0} };
				AuthorizationItemSet rightSet = { sizeof(rights)/ sizeof(*rights), rights };
		
				siResult = AuthorizationCopyRights(authRef, &rightSet, NULL,
											kAuthorizationFlagExtendRights, &resultRightSet);
				if (resultRightSet != NULL)
				{
					AuthorizationFreeItemSet(resultRightSet);
					resultRightSet = NULL;
				}
				if (siResult != errAuthorizationSuccess)
				{
					DbgLog( kLogPlugin, "CSearchPlugin: AuthorizationCopyRights returned error %d", siResult );
					syslog( LOG_ALERT, "AuthorizationCopyRights returned error %d", siResult );
					throw( (SInt32)eDSPermissionError );
				}
			}
		}

		// have to verify the auth ref before grabbing the mutex to avoid deadlock
		fMutex.WaitLock();
		try
		{
			DSEventSemaphore *eventSemaphore = NULL;
			
			aSearchConfig = FindSearchConfigWithKey(pContext->fSearchConfigKey);
			if ( aSearchConfig == nil ) throw( (SInt32)eDSInvalidNodeRef );		

			// Set it to the first node in the search list - this check doesn't need to use the context search path
			if ( aSearchConfig->fSearchNodeList == nil ) throw( (SInt32)eSearchPathNotDefined );

			switch( aRequest )
			{
				case eDSCustomCallSearchSetPolicyAutomatic:
					bAuthSwitched = SwitchSearchPolicy( kAutomaticSearchPolicy, aSearchConfig );
					//need to save the switch to the config file
					if (aSearchConfig->pConfigFromXML)
					{
						siResult = aSearchConfig->pConfigFromXML->SetSearchPolicy(kAutomaticSearchPolicy);
						siResult = aSearchConfig->pConfigFromXML->WriteConfig();
					}
					break;

				case eDSCustomCallSearchSetPolicyLocalOnly:
					bAuthSwitched = SwitchSearchPolicy( kLocalSearchPolicy, aSearchConfig );
					//need to save the switch to the config file
					if (aSearchConfig->pConfigFromXML)
					{
						siResult = aSearchConfig->pConfigFromXML->SetSearchPolicy(kLocalSearchPolicy);
						siResult = aSearchConfig->pConfigFromXML->WriteConfig();
					}
					break;

				case eDSCustomCallSearchSetPolicyCustom:
					eventSemaphore = &fAuthPolicyChangeEvent;
					
					if ( aSearchConfig->fSearchConfigKey == eDSContactsSearchNodeName )
						eventSemaphore = &fContactPolicyChangeEvent;
					
					eventSemaphore->ResetEvent();
					
					bAuthSwitched = SwitchSearchPolicy( kCustomSearchPolicy, aSearchConfig );
					//need to save the switch to the config file
					if (aSearchConfig->pConfigFromXML)
					{
						siResult = aSearchConfig->pConfigFromXML->SetSearchPolicy(kCustomSearchPolicy);
						siResult = aSearchConfig->pConfigFromXML->WriteConfig();
					}
				
					// wait up to 15 seconds for at least 1 pass through the nodes, this is not guaranteed since any node can block
					// the check
					if ( (bAuthSwitched && aSearchConfig->fSearchConfigKey == eDSAuthenticationSearchNodeName) || 
						 aSearchConfig->fSearchConfigKey == eDSContactsSearchNodeName )
					{
						// need to give up our lock so the thread can run
						fMutex.SignalLock();
						
						// post a network transition to free the search for next round
						if ( eventSemaphore->WaitForEvent(15 * kMilliSecsPerSec) == false )
						{
							DbgLog( kLogPlugin, "CSearchPlugin::DoPluginCustomCall - eDSCustomCallSearchSetPolicyCustom - timed out waiting for node check" );
						}
						
						fMutex.WaitLock();
#if AUGMENT_RECORDS
						CheckForAugmentConfig( (tDirPatternMatch) aSearchConfig->fSearchConfigKey );
#endif
					}
					break;
					
#if AUGMENT_RECORDS
				case eDSCustomCallSearchCheckForAugmentRecord:
					CheckForAugmentConfig( (tDirPatternMatch) aSearchConfig->fSearchConfigKey );
					break;
#endif

					//here we accept an XML blob to replace the current custom search path nodes
				case eDSCustomCallSearchSetCustomNodeList:
					//need to make xmlData large enough to receive the data
					//the XML data immediately follows the AuthorizationExternalForm
					xmlDataLength = (SInt32) bufLen - sizeof( AuthorizationExternalForm );
					if ( xmlDataLength <= 0 ) throw( (SInt32)eDSInvalidBuffFormat );
					
					xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData + sizeof( AuthorizationExternalForm )),xmlDataLength);
					//build the csp array
					cspArray = (CFMutableArrayRef)CFPropertyListCreateFromXMLData(NULL,xmlData,0,NULL);
					if (aSearchConfig->pConfigFromXML)
					{
						siResult = aSearchConfig->pConfigFromXML->SetListArray(cspArray);
						siResult = aSearchConfig->pConfigFromXML->WriteConfig();
					}
					CFRelease(cspArray);
					CFRelease(xmlData);
					if ( aSearchConfig->fSearchNodePolicy == kCustomSearchPolicy )
					{
						eventSemaphore = &fAuthPolicyChangeEvent;
						
						if ( aSearchConfig->fSearchConfigKey == eDSContactsSearchNodeName )
							eventSemaphore = &fContactPolicyChangeEvent;

						eventSemaphore->ResetEvent();

						// need to reset the policy since changes made to the data need to be picked up
						bAuthSwitched = SwitchSearchPolicy( kCustomSearchPolicy, aSearchConfig );

						// wait up to 15 seconds for at least 1 pass through the nodes, this is not guaranteed since any node can block
						// the check
						if ( (bAuthSwitched && aSearchConfig->fSearchConfigKey == eDSAuthenticationSearchNodeName) || 
							 aSearchConfig->fSearchConfigKey == eDSContactsSearchNodeName )
						{
							// need to give up our lock so the thread can run
							fMutex.SignalLock();
							
							if ( eventSemaphore->WaitForEvent(15 * kMilliSecsPerSec) == false )
							{
								DbgLog( kLogPlugin, "CSearchPlugin::DoPluginCustomCall - eDSCustomCallSearchSetCustomNodeList timed out waiting for node check" );
							}
							
							fMutex.WaitLock();
	#if AUGMENT_RECORDS
							CheckForAugmentConfig( (tDirPatternMatch) aSearchConfig->fSearchConfigKey );
	#endif
						}
					}
					break;

				case eDSCustomCallSearchReadDHCPLDAPSize:
					// get length of DHCP LDAP dictionary

					if ( inData->fOutRequestResponse == nil ) throw( (SInt32)eDSNullDataBuff );
					if ( inData->fOutRequestResponse->fBufferData == nil ) throw( (SInt32)eDSEmptyBuffer );
					if ( inData->fOutRequestResponse->fBufferSize < sizeof( CFIndex ) ) throw( (SInt32)eDSInvalidBuffFormat );
					if ( aSearchConfig->pConfigFromXML != nil)
					{
						// need four bytes for size
						dhcpLDAPdict = aSearchConfig->pConfigFromXML->GetDHCPLDAPDictionary();
						if (dhcpLDAPdict != 0)
						{
							xmlData = CFPropertyListCreateXMLData(NULL,dhcpLDAPdict);
						}
						if (xmlData != 0)
						{
							*(CFIndex*)(inData->fOutRequestResponse->fBufferData) = CFDataGetLength(xmlData);
							inData->fOutRequestResponse->fBufferLength = sizeof( CFIndex );
							CFRelease(xmlData);
							xmlData = 0;
						}
						else
						{
							*(CFIndex*)(inData->fOutRequestResponse->fBufferData) = 0;
							inData->fOutRequestResponse->fBufferLength = sizeof( CFIndex );
						}
					}
					break;

				case eDSCustomCallSearchReadDHCPLDAPData:
					// read xml config

					if ( inData->fOutRequestResponse == nil ) throw( (SInt32)eDSNullDataBuff );
					if ( inData->fOutRequestResponse->fBufferData == nil ) throw( (SInt32)eDSEmptyBuffer );
					if ( aSearchConfig->pConfigFromXML != nil )
					{
						dhcpLDAPdict = aSearchConfig->pConfigFromXML->GetDHCPLDAPDictionary();
						if (dhcpLDAPdict != 0)
						{
							xmlData = CFPropertyListCreateXMLData(NULL,dhcpLDAPdict);
							if (xmlData != 0)
							{
								aRange.location = 0;
								aRange.length = CFDataGetLength( xmlData );
								if ( inData->fOutRequestResponse->fBufferSize < (unsigned int)aRange.length )
									throw( (SInt32)eDSBufferTooSmall );
								CFDataGetBytes( xmlData, aRange, (UInt8*)(inData->fOutRequestResponse->fBufferData) );
								inData->fOutRequestResponse->fBufferLength = aRange.length;
								CFRelease( xmlData );
								xmlData = 0;
							}
						}
					}
					break;
					
				case eDSCustomCallSearchWriteDHCPLDAPData:
					//need to make xmlData large enough to receive the data
					//the XML data immediately follows the AuthorizationExternalForm
					xmlDataLength = (SInt32) bufLen - sizeof( AuthorizationExternalForm );
					if ( xmlDataLength <= 0 ) throw( (SInt32)eDSInvalidBuffFormat );

					xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData + sizeof( AuthorizationExternalForm )),xmlDataLength);
					//build the csp array
					dhcpLDAPdict = (CFDictionaryRef)CFPropertyListCreateFromXMLData(NULL,xmlData,0,NULL);
					if (aSearchConfig->pConfigFromXML)
					{
						aSearchConfig->pConfigFromXML->SetDHCPLDAPDictionary(dhcpLDAPdict);
						siResult = aSearchConfig->pConfigFromXML->WriteConfig();
					}
					CFRelease(dhcpLDAPdict);
					CFRelease(xmlData);

					// need to reset the policy since changes made to the data need to be picked up
					// need to make sure we pick up any changes if automatic search policy is active
					DbgLog( kLogPlugin, "CSearchPlugin::DoPluginCustomCall updating DHCP LDAP configuration for policy %X", aSearchConfig->fSearchConfigKey );
				
					bAuthSwitched = SwitchSearchPolicy( aSearchConfig->fSearchNodePolicy, aSearchConfig );

	#if AUGMENT_RECORDS
					if ( (bAuthSwitched && aSearchConfig->fSearchConfigKey == eDSAuthenticationSearchNodeName) || 
						 aSearchConfig->fSearchConfigKey == eDSContactsSearchNodeName)
					{
						CheckForAugmentConfig( (tDirPatternMatch) aSearchConfig->fSearchConfigKey );
					}
	#endif
					break;
					
				case eDSCustomCallSearchSubNodesUnreachable:
				{
					// see which sub-nodes have yet to be opened.
					
					if ( inData->fOutRequestResponse == nil ) throw( (SInt32)eDSNullDataBuff );
					if ( inData->fOutRequestResponse->fBufferData == nil ) throw( (SInt32)eDSEmptyBuffer );
					
					CFMutableArrayRef nodeList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

					const int NUM_POLICIES = 2;
					int policies[NUM_POLICIES] = { eDSAuthenticationSearchNodeName, eDSContactsSearchNodeName };
					for (int i = 0;  i < NUM_POLICIES;  i++)
					{
						aSearchConfig = FindSearchConfigWithKey( policies[i] );
						if (aSearchConfig != nil)
						{
							nodeListPtr = aSearchConfig->fSearchNodeList->fNext;	// skip /Local/Default - always reachable
							nodeListPtr = nodeListPtr->fNext;						// skip /BSD/Local - always reachable
							while (nodeListPtr != nil)
							{
								if (!nodeListPtr->fNodeReachable && nodeListPtr->fNodeName != nil)
								{
									CFStringRef nodeStr = CFStringCreateWithCString( kCFAllocatorDefault, nodeListPtr->fNodeName, kCFStringEncodingUTF8 );
									CFArrayAppendValue( nodeList, nodeStr );
									DSCFRelease( nodeStr );
								}
								nodeListPtr = nodeListPtr->fNext;
							}
						}
					}
					
					DbgLog( kLogPlugin, "CSearchPlugin::DoPluginCustomCall eDSCustomCallSearchSubNodesUnavailable - %d nodes unreachable", CFArrayGetCount(nodeList) );

					// return the data.
					xmlData = CFPropertyListCreateXMLData( NULL, nodeList );
					if (xmlData != nil)
					{
						aRange.location = 0;
						aRange.length = CFDataGetLength( xmlData );
						if ( inData->fOutRequestResponse->fBufferSize < (unsigned int)aRange.length )
							throw( (SInt32)eDSBufferTooSmall );

						CFDataGetBytes( xmlData, aRange, (UInt8*)(inData->fOutRequestResponse->fBufferData) );
						inData->fOutRequestResponse->fBufferLength = aRange.length;
						DSCFRelease( nodeList );
						DSCFRelease( xmlData );
					}
					break;
				}
				
				default:
					break;
			}
		}
		catch( SInt32 err )
		{
			siResult = err;
		}
		catch( ... )
		{
			siResult = eUndefinedError;
		}
		
		fMutex.SignalLock();
	}
	catch( SInt32 err )
	{
		siResult = err;
	}

	if (authRef != 0)
	{
		AuthorizationFree(authRef, 0);
		authRef = 0;
	}

	return( siResult );

} // DoPlugInCustomCall


// ---------------------------------------------------------------------------
//	* CleanSearchConfigData
// ---------------------------------------------------------------------------

SInt32 CSearchPlugin:: CleanSearchConfigData ( sSearchConfig *inList )
{
    SInt32				siResult	= eDSNoErr;

    if ( inList != nil )
    {
		inList->fSearchNodePolicy		= 0;
		inList->fSearchConfigKey		= 0;
		inList->fDirNodeType			= kUnknownNodeType;

		inList->fNext					= nil;
		
		if (inList->fSearchNodeName != nil)
		{
			free(inList->fSearchNodeName);
			inList->fSearchNodeName = nil;
		}
		if (inList->fSearchConfigFilePrefix != nil)
		{
			free(inList->fSearchConfigFilePrefix);
			inList->fSearchConfigFilePrefix = nil;
		}
		
		CleanSearchListData( inList->fSearchNodeList );
		
		inList->fSearchNodeList = nil; //take the chance this is a leak if inList->pConfigFromXML was nil
		
		if (inList->pConfigFromXML != nil)
		{
			delete(inList->pConfigFromXML);
			inList->pConfigFromXML = nil;
		}
   }

    return( siResult );

} // CleanSearchConfigData


// ---------------------------------------------------------------------------
//	* CleanSearchListData
// ---------------------------------------------------------------------------

SInt32 CSearchPlugin:: CleanSearchListData ( sSearchList *inList )
{
    SInt32				siResult	= eDSNoErr;
	sSearchList		   *pList		= nil;
	sSearchList		   *pDeleteList	= nil;


	if (inList != nil)
	{
		//need the nested ifs so that can make use of the CleanListData method
		//need to cleanup the struct list ie. the internals
		pList = inList;
		while (pList != nil)
		{
			pDeleteList = pList;
			pList = pList->fNext;		//assign to next BEFORE deleting current
			if (pDeleteList->fNodeName != nil)
			{
				delete ( pDeleteList->fNodeName );
			}
			pDeleteList->fOpened = false;
			pDeleteList->fNodeReachable		= false;
			if (pDeleteList->fNodeRef != 0)
			{
				::dsCloseDirNode(pDeleteList->fNodeRef); // don't check error code
				pDeleteList->fNodeRef = 0;
			}
			pDeleteList->fNext = nil;
			if (pDeleteList->fDataList != nil)
			{
				dsDataListDeallocatePriv ( pDeleteList->fDataList );
				//need to free the header as well
				free( pDeleteList->fDataList );
				pDeleteList->fDataList = nil;
			}
			delete( pDeleteList );
			pDeleteList = nil;
		}
	}

    return( siResult );

} // CleanSearchListData

// ---------------------------------------------------------------------------
//	* MakeSearchConfigData
// ---------------------------------------------------------------------------

sSearchConfig *CSearchPlugin::MakeSearchConfigData (	sSearchList *inSearchNodeList,
													UInt32 inSearchPolicy,
													CConfigs *inConfigFromXML,
													char *inSearchNodeName,
													char *inSearchConfigFilePrefix,
													eDirNodeType inDirNodeType,
													UInt32 inSearchConfigType )
{
    SInt32				siResult		= eDSNoErr;
    sSearchConfig  	   *configOut		= nil;

	configOut = (sSearchConfig *) calloc(sizeof(sSearchConfig), sizeof(char));
	if (configOut != nil)
	{
		//just created so no need to check siResult?
		siResult = CleanSearchConfigData(configOut);
		configOut->fSearchNodeList			= inSearchNodeList;
		configOut->fSearchNodePolicy		= inSearchPolicy;
		configOut->pConfigFromXML			= inConfigFromXML;
		configOut->fSearchNodeName			= inSearchNodeName;
		configOut->fSearchConfigFilePrefix	= inSearchConfigFilePrefix;
		configOut->fDirNodeType				= inDirNodeType;
		configOut->fSearchConfigKey			= inSearchConfigType;
		configOut->fNext					= nil;
	}

    return( configOut );

} // MakeSearchConfigData


// ---------------------------------------------------------------------------
//	* FindSearchConfigWithKey
// ---------------------------------------------------------------------------

sSearchConfig *CSearchPlugin:: FindSearchConfigWithKey (	UInt32 inSearchConfigKey )
{
    sSearchConfig  	   *configOut		= nil;

	fMutex.WaitLock();
	configOut = pSearchConfigList;
	while ( configOut != nil )
	{
		if (configOut->fSearchConfigKey == inSearchConfigKey)
		{
			break;
		}
		configOut = configOut->fNext;
	}
	fMutex.SignalLock();

    return( configOut );

} // FindSearchConfigWithKey


// ---------------------------------------------------------------------------
//	* AddSearchConfigToList
// ---------------------------------------------------------------------------

SInt32 CSearchPlugin:: AddSearchConfigToList ( sSearchConfig *inSearchConfig )
{
    sSearchConfig  	   *aConfigList		= nil;
	SInt32				siResult		= eDSInvalidIndex;
	bool				uiDup			= false;

	fMutex.WaitLock();
	aConfigList = pSearchConfigList;
	while ( aConfigList != nil ) // look for existing entry with same key
	{
		if (aConfigList->fSearchConfigKey == inSearchConfig->fSearchConfigKey)
		{
			uiDup = true;
			break;
		}
		aConfigList = aConfigList->fNext;
	}

	if (!uiDup) //don't add if entry already exists
	{
		aConfigList = pSearchConfigList;
		if (aConfigList == nil)
		{
			pSearchConfigList = inSearchConfig;
		}
		else
		{
			while ( aConfigList->fNext != nil )
			{
				aConfigList = aConfigList->fNext;
			}
			aConfigList->fNext = inSearchConfig;
		}
		siResult = eDSNoErr;
	}
	fMutex.SignalLock();
	
    return( siResult );

} // AddSearchConfigToList


// ---------------------------------------------------------------------------
//	* RemoveSearchConfigWithKey //TODO this could be a problem if it is ever called
// ---------------------------------------------------------------------------
/*
SInt32 CSearchPlugin:: RemoveSearchConfigWithKey ( UInt32 inSearchConfigKey )
{
    sSearchConfig  	   *aConfigList		= nil;
    sSearchConfig  	   *aConfigPtr		= nil;
	SInt32				siResult		= eDSInvalidIndex;

	fMutex.WaitLock();
	aConfigList = pSearchConfigList;
	aConfigPtr	= pSearchConfigList;
	if (aConfigList->fSearchConfigKey == inSearchConfigKey)
	{
		pSearchConfigList = aConfigList->fNext;
		siResult = eDSNoErr;
	}
	else
	{
		aConfigList = aConfigList->fNext;
		while ( aConfigList != nil ) // look for existing entry with same key
		{
			if (aConfigList->fSearchConfigKey == inSearchConfigKey)
			{
				aConfigPtr->fNext = aConfigList->fNext;
				siResult = eDSNoErr;
				break;
			}
			aConfigList = aConfigList->fNext;
			aConfigPtr	= aConfigPtr->fNext;
		}
	}
	fMutex.SignalLock();
	
    return( siResult );

} // RemoveSearchConfigWithKey
*/

//------------------------------------------------------------------------------------
//	* CloseAttributeList
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::CloseAttributeList ( sCloseAttributeList *inData )
{
	SInt32				siResult		= eDSNoErr;
	sSearchContextData *pContext		= nil;

	pContext = (sSearchContextData *)gSNNodeRef->GetItemData( inData->fInAttributeListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gSNNodeRef->RemoveItem( inData->fInAttributeListRef );
	}
	else
	{
		siResult = eDSInvalidAttrListRef;
	}

	return( siResult );

} // CloseAttributeList


//------------------------------------------------------------------------------------
//	* CloseAttributeValueList
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::CloseAttributeValueList ( sCloseAttributeValueList *inData )
{
	SInt32				siResult		= eDSNoErr;
	sSearchContextData *pContext		= nil;

	pContext = (sSearchContextData *)gSNNodeRef->GetItemData( inData->fInAttributeValueListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gSNNodeRef->RemoveItem( inData->fInAttributeValueListRef );
	}
	else
	{
		siResult = eDSInvalidAttrValueRef;
	}

	return( siResult );

} // CloseAttributeValueList


//------------------------------------------------------------------------------------
//	* DupSearchListWithNewRefs
//------------------------------------------------------------------------------------

sSearchList *CSearchPlugin::DupSearchListWithNewRefs ( sSearchList *inSearchList )
{
	sSearchList	   *outSearchList		= nil;
	sSearchList	   *pSearchList			= inSearchList;
	sSearchList	   *aSearchList			= nil;
	sSearchList	   *tailSearchList		= nil;
	bool			isFirst				= true;
	bool			getLocalFirst  		= true;

	//this might be a good place to refresh (re-init?) the search policy to get a fresh perspective on the search paths?
	while (pSearchList != nil)
	{
   		aSearchList = (sSearchList *)calloc( 1, sizeof( sSearchList ) );
		
		//init
		aSearchList->fOpened	= false;
		aSearchList->fHasNeverOpened	= pSearchList->fHasNeverOpened;
		aSearchList->fNodeReachable		= pSearchList->fNodeReachable;
		aSearchList->fNodeRef	= 0;
		aSearchList->fNodeName	= nil;
		aSearchList->fDataList	= nil;
		aSearchList->fNext		= nil;

		//need to retain the order
		if (isFirst)
		{
			outSearchList		= aSearchList;
			tailSearchList		= aSearchList;
			isFirst				= false;
		}
		else
		{
			tailSearchList->fNext	= aSearchList;
			tailSearchList			= aSearchList;
		}

		if (pSearchList->fNodeName != nil)
		{
			aSearchList->fNodeName = (char *)calloc(1, ::strlen(pSearchList->fNodeName) + 1);
			strcpy(aSearchList->fNodeName,pSearchList->fNodeName);

			//aSearchList->fDataList = ::dsBuildFromPathPriv( aSearchList->fNodeName, "/" );
			
			if (getLocalFirst)
			{
				aSearchList->fDataList = ::dsBuildFromPathPriv( kstrDefaultLocalNodeName, "/" );

				//let's open lazily when we actually need the node ref
				getLocalFirst = false;
			}
			else
			{
				aSearchList->fDataList = ::dsBuildFromPathPriv( aSearchList->fNodeName, "/" );
			
				//let's open lazily when we actually need the node ref
			}
		}

		pSearchList = pSearchList->fNext;
	}

	return( outSearchList );

} // DupSearchListWithNewRefs


// ---------------------------------------------------------------------------
//	* ContinueDeallocProc
// ---------------------------------------------------------------------------

void CSearchPlugin::ContinueDeallocProc ( void* inContinueData )
{
	sSearchContinueData *pContinue = (sSearchContinueData *)inContinueData;

	if ( pContinue != nil )
	{
		if ( pContinue->fDataBuff != nil )
		{
			::dsDataBufferDeallocatePriv( pContinue->fDataBuff );
			pContinue->fDataBuff = nil;
		}

		if ( pContinue->fContextData != nil )
		{
			::dsReleaseContinueData( pContinue->fNodeRef, pContinue->fContextData );
			pContinue->fContextData = nil;
		}
		
		if ( pContinue->fAugmentReqAttribs != NULL )
		{
			dsDataListDeallocate( 0, pContinue->fAugmentReqAttribs );
			DSFree( pContinue->fAugmentReqAttribs );
		}

		free( pContinue );
		pContinue = nil;
	}
} // ContinueDeallocProc


// ---------------------------------------------------------------------------
//	* ContextDeallocProc
// ---------------------------------------------------------------------------

void CSearchPlugin::ContextDeallocProc ( void* inContextData )
{
	sSearchContextData *pContext = (sSearchContextData *) inContextData;

	if ( pContext != nil )
	{
		CleanContextData( pContext );
	}
} // ContextDeallocProc

// ---------------------------------------------------------------------------
//	* ContextSetListChangedProc
// ---------------------------------------------------------------------------

void CSearchPlugin:: ContextSetListChangedProc ( void* inContextData )
{
	sSearchContextData *pContext = (sSearchContextData *) inContextData;

	if ( pContext != nil )
	{
		OSAtomicCompareAndSwap32Barrier(false, true, &pContext->bListChanged);
	}
} // ContextSetListChangedProc

// ---------------------------------------------------------------------------
//	* ContextSetCheckForNIParentNowProc
// ---------------------------------------------------------------------------

void CSearchPlugin:: ContextSetCheckForNIParentNowProc ( void* inContextData )
{
	sSearchContextData *pContext = (sSearchContextData *) inContextData;

	if ( pContext != nil )
	{
		pContext->bCheckForNIParentNow	= true;
	}
} // ContextSetCheckForNIParentNowProc

//------------------------------------------------------------------------------------
//	* CheckSearchPolicyChange
//------------------------------------------------------------------------------------

SInt32 CSearchPlugin::CheckSearchPolicyChange( sSearchContextData *pContext, tDirNodeReference inNodeRef, tContextData inContinueData )
{
	SInt32			siResult		= eDSNoErr;
	sSearchConfig	*aSearchConfig	= nil;
	
	// it's important to always aquire the global mutex before the individual
	// reference mutex to avoid deadlock
	fMutex.WaitLock();
	
	aSearchConfig	= FindSearchConfigWithKey(pContext->fSearchConfigKey);
	if ( aSearchConfig == nil )
	{
		siResult = eDSInvalidNodeRef;
	}
	//switch search policy does not work with the DefaultNetwork Node
	//check whether search policy has switched and if it has then adjust to the new one
	else if ( ( pContext->bListChanged ) && ( pContext->fSearchConfigKey != eDSNetworkSearchNodeName ) )
	{
		if ( inContinueData != nil )
		{
			//in the middle of continue data the search policy has changed so exit here
			siResult = eDSInvalidContinueData; //KW would like a more appropriate error code
		}
		else
		{
			if ( pContext->pSearchListMutex )
			{
				// If the list has changed and we can't get the lock, invalidate
				// whatever is going on.  Prevents deadlock.
				if ( !pContext->pSearchListMutex->WaitTry() )
				{
					fMutex.SignalLock();
					return eDSBadContextData;
				}
			}
	
			//switch the search policy to the current one
			//flush the old search path list
			CleanSearchListData( pContext->fSearchNodeList );
			
			//remove all existing continue data off of this reference
			gSNContinue->RemoveItems( inNodeRef );
			
			//get the updated search path list with new unique refs of each
			//search path node for use by this client who opened the search node
			pContext->fSearchNodeList = DupSearchListWithNewRefs(aSearchConfig->fSearchNodeList);
			
			if (aSearchConfig->fSearchNodePolicy == kAutomaticSearchPolicy)
			{
				pContext->bAutoSearchList = true;
			}
			else
			{
				pContext->bAutoSearchList = false;
			}
			
			//reset the flag
			OSAtomicCompareAndSwap32Barrier(true, false, &pContext->bListChanged);
			
			if( pContext->pSearchListMutex )
			{
				pContext->pSearchListMutex->SignalLock();
			}
		}
	}
	
	fMutex.SignalLock();
	
	return siResult;
} // CheckSearchPolicyChange

// ---------------------------------------------------------------------------
//	* SetSearchPolicyIndicatorFile -- ONLY used with AuthenticationSearch Node
// ---------------------------------------------------------------------------

void CSearchPlugin:: SetSearchPolicyIndicatorFile ( UInt32 inSearchNodeKey, UInt32 inSearchPolicyIndex )
{
	if (inSearchNodeKey == eDSAuthenticationSearchNodeName)
	{
		//check if the directory exists that holds the indicator file
		dsCreatePrefsDirectory();
		
		//eliminate the existing indicator file
		RemoveSearchPolicyIndicatorFile();
		
		//add the new indicator file
		if (inSearchPolicyIndex == 3)
		{
			dsTouch( "/var/run/.DSRunningSP3" );
		}
		else if (inSearchPolicyIndex == 2)
		{
			dsTouch( "/var/run/.DSRunningSP2" );
		}
		else //assume inSearchPolicyIndex = 1
		{
			dsTouch( "/var/run/.DSRunningSP1" );
		}
	}

} // SetSearchPolicyIndicatorFile

// ---------------------------------------------------------------------------
//	* RemoveSearchPolicyIndicatorFile
// ---------------------------------------------------------------------------

void CSearchPlugin:: RemoveSearchPolicyIndicatorFile ( void )
{
	dsRemove( "/var/run/.DSRunningSP1" );
	dsRemove( "/var/run/.DSRunningSP2" );
	dsRemove( "/var/run/.DSRunningSP3" );
} // RemoveSearchPolicyIndicatorFile


//--------------------------------------------------------------------------------------------------
// * BuildNetworkNodeList ()
//--------------------------------------------------------------------------------------------------

sSearchList *CSearchPlugin::BuildNetworkNodeList ( void )
{
	sSearchList	   *outSearchList	= nil;
	sSearchList	   *aSearchList		= nil;
	sSearchList	   *tailSearchList	= nil;
	bool			isFirst			= true;
	tDataBuffer	   *pNodeBuff 		= nil;
	bool			done			= false;
	UInt32			uiCount			= 0;
	UInt32			uiIndex			= 0;
	tContextData	context			= NULL;
	tDataList	   *pDataList		= nil;
	SInt32			siResult		= eDSNoErr;

// alloc a buffer
// find dir nodes of default network type
// set only the path str and the tDataList
// since we open nodes lazily
// add to the list

	try
	{
		pNodeBuff	= ::dsDataBufferAllocatePriv( 2048 );
		if ( pNodeBuff == nil ) throw( (SInt32)eMemoryAllocError );
			
		while ( done == false )
		{
			do 
			{
				siResult = dsFindDirNodes( fDirRef, pNodeBuff, NULL, eDSDefaultNetworkNodes, &uiCount, &context );
				if (siResult == eDSBufferTooSmall)
				{
					UInt32 bufSize = pNodeBuff->fBufferSize;
					dsDataBufferDeallocatePriv( pNodeBuff );
					pNodeBuff = nil;
					pNodeBuff = ::dsDataBufferAllocatePriv( bufSize * 2 );
				}
			} while (siResult == eDSBufferTooSmall);
		
			if ( siResult != eDSNoErr ) throw( siResult );
			
			for ( uiIndex = 1; uiIndex <= uiCount; uiIndex++ )
			{
				siResult = dsGetDirNodeName( fDirRef, pNodeBuff, uiIndex, &pDataList );
				if ( siResult != eDSNoErr ) throw( siResult );
				
				//here we have the node name in a tDataList
				//NOW build the search list item
				aSearchList = (sSearchList *)calloc( 1, sizeof( sSearchList ) );
				if ( aSearchList == nil ) throw( (SInt32)eMemoryAllocError );
				
				//init
				aSearchList->fOpened	= false;
				aSearchList->fHasNeverOpened	= true;
				aSearchList->fNodeReachable			= false;
				aSearchList->fNodeRef	= 0;
				aSearchList->fDataList	= pDataList;
				//get path str from tDatalist
				aSearchList->fNodeName	= dsGetPathFromListPriv( pDataList, "/" );
				aSearchList->fNext		= nil;
		
				//retaining the ordering from dsFindDirNodes
				if (isFirst)
				{
					outSearchList		= aSearchList;
					tailSearchList		= aSearchList;
					isFirst				= false;
				}
				else
				{
					tailSearchList->fNext	= aSearchList;
					tailSearchList			= aSearchList;
				}
		
						
				//the pDataList is consumed by the aSearchList so don't dealloc it
				//siResult = dsDataListDeallocatePriv( pDataList );
				//if ( siResult != eDSNoErr ) throw( siResult );
				//free(pDataList);
				pDataList = nil;
				
			} // for loop over uiIndex
			
			done = (context == nil);

		} // while done == false
		
		dsDataBufferDeallocatePriv( pNodeBuff );
		pNodeBuff = nil;
		
	} // try

	catch( SInt32 err )
	{
		//KW might try to clean up outSearchList here but hard to know where the memory alloc above failed
		outSearchList = nil;
		DbgLog( kLogPlugin, "Memory error finding the Default Network Nodes with error: %l", err );
	}

	return( outSearchList );

} // BuildNetworkNodeList


// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

SInt32 CSearchPlugin::SetPluginState ( const UInt32 inState )
{
	if ( inState & kActive )
    {
		//tell everyone we are ready to go
		WakeUpRequests();
	}
	return( eDSNoErr );
} // SetPluginState


#if AUGMENT_RECORDS

//--------------------------------------------------------------------------------------------------
// * CheckForAugmentConfig ()
//--------------------------------------------------------------------------------------------------

SInt32 CSearchPlugin::CheckForAugmentConfig ( tDirPatternMatch policyToCheck )
{
	SInt32				siResult		= eDSNoErr;
    CFDictionaryRef		aDict			= NULL;
	sSearchConfig		*aSearchConfig	= nil;
	
	//we don't remove anything that we are not using
	//we only support one at a time
	//last one in wins
	
	//looking for the first AugmentConfig record in the kDSStdRecordTypeConfig record type
	aDict = FindAugmentConfigRecord( policyToCheck );
	if ( aDict != NULL )
	{
		if (	CFDictionaryContainsKey( aDict, CFSTR( kXMLAugmentSearchKey ) )  &&
				CFDictionaryContainsKey( aDict, CFSTR( kXMLAugmentDirNodeNameKey ) )  &&
				CFDictionaryContainsKey( aDict, CFSTR( kXMLToBeAugmentedDirNodeNameKey ) )  &&
				CFDictionaryContainsKey( aDict, CFSTR( kXMLAugmentAttrListDictKey ) )  )
		{
			fMutex.WaitLock();
			aSearchConfig = FindSearchConfigWithKey( policyToCheck );
			if ( (aSearchConfig != nil) && (aSearchConfig->pConfigFromXML != nil) )
			{
				aSearchConfig->pConfigFromXML->UpdateAugmentDict(aDict);
			}
			fMutex.SignalLock();
		}
		
		DSCFRelease( aDict );
	}

	return( siResult );

} // CheckForAugmentConfig


//--------------------------------------------------------------------------------------------------
// * FindAugmentConfigRecord ()
//--------------------------------------------------------------------------------------------------

CFDictionaryRef CSearchPlugin::FindAugmentConfigRecord( tDirPatternMatch nodeType )
{

	sSearchConfig		   *aSearchConfig			= nil;
	sSearchList			   *aSearchList				= nil;
	bool					bLookForConfig			= false;
	tDataBufferPtr			dataBuff				= nil;
	SInt32					siResult				= eDSNoErr;
	UInt32					nodeCount				= 0;
	tContextData			context					= nil;
	tDataListPtr			nodeName				= nil;
	tDirNodeReference		aSearchNodeRef			= 0;
	tDataListPtr			recName					= nil;
	tDataListPtr			recType					= nil;
	tDataListPtr			attrTypes				= nil;
	UInt32					recCount				= 1; // only care about first match
	tRecordEntry		   *pRecEntry				= nil;
	tAttributeListRef		attrListRef				= 0;
	tAttributeValueListRef	valueRef				= 0;
	tAttributeEntry		   *pAttrEntry				= nil;
	tAttributeValueEntry   *pValueEntry				= nil;
	CFMutableDictionaryRef	outDictionary			= NULL;
	char				   *pMetaNodeLocation		= NULL;
	
	fMutex.WaitLock();
	aSearchConfig = FindSearchConfigWithKey( nodeType );
	if ( (aSearchConfig != nil) && (aSearchConfig->fSearchNodePolicy == kCustomSearchPolicy) )
	{
		//we are on custom search policy for authentication
		if ((aSearchConfig->fSearchNodeList != nil) && (aSearchConfig->fSearchNodeList->fNext != nil))
		{
			//we likely have more than the two local nodes on the policy
			aSearchList = aSearchConfig->fSearchNodeList->fNext;
			aSearchList = aSearchList->fNext;
			if (aSearchList != nil)
			{
				bLookForConfig = true;
			}
		}
	}
	fMutex.SignalLock();

	try
	{
		if ( (bLookForConfig) && (fDirRef != 0) )
		{
			dataBuff = dsDataBufferAllocate( fDirRef, 256 );
			if ( dataBuff == nil ) throw( (SInt32)eMemoryAllocError );
			
			siResult = dsFindDirNodes( fDirRef, dataBuff, nil, nodeType, &nodeCount, &context );
			if ( siResult != eDSNoErr ) throw( siResult );
			if ( nodeCount < 1 ) throw( eDSNodeNotFound );
	
			siResult = dsGetDirNodeName( fDirRef, dataBuff, 1, &nodeName );
			if ( siResult != eDSNoErr ) throw( siResult );
	
			siResult = dsOpenDirNode( fDirRef, nodeName, &aSearchNodeRef );
			if ( siResult != eDSNoErr ) throw( siResult );
			if ( nodeName != NULL )
			{
				dsDataListDeallocate( fDirRef, nodeName );
				free( nodeName );
				nodeName = NULL;
			}
			recName = dsBuildListFromStrings( fDirRef, "augmentconfiguration", NULL );
			recType = dsBuildListFromStrings( fDirRef, kDSStdRecordTypeConfig, NULL );
			attrTypes = dsBuildListFromStrings( fDirRef, kDS1AttrXMLPlist, kDSNAttrMetaNodeLocation, NULL );
            context = nil;
			do 
			{
				siResult = dsGetRecordList( aSearchNodeRef, dataBuff, recName, eDSExact, recType, attrTypes, false, &recCount, &context);
				if (siResult == eDSBufferTooSmall)
				{
					UInt32 bufSize = dataBuff->fBufferSize;
					dsDataBufferDeallocatePriv( dataBuff );
					dataBuff = nil;
					dataBuff = ::dsDataBufferAllocate( fDirRef, bufSize * 2 );
				}
			} while ( (siResult == eDSBufferTooSmall) || ( (siResult == eDSNoErr) && (recCount == 0) && (context != nil) ) );
			
			if ( (siResult == eDSNoErr) && (recCount > 0) )
			{
				siResult = ::dsGetRecordEntry( aSearchNodeRef, dataBuff, 1, &attrListRef, &pRecEntry );
				if ( (siResult == eDSNoErr) && (pRecEntry != nil) )
				{
					//index starts at one - should have a single entry
					for ( UInt32 ii = 1; ii <= pRecEntry->fRecordAttributeCount; ii++ )
					{
						siResult = ::dsGetAttributeEntry( aSearchNodeRef, dataBuff, attrListRef, ii, &valueRef, &pAttrEntry );
						//should have only one value - get first only
						if ( ( siResult == eDSNoErr ) && ( pAttrEntry->fAttributeValueCount > 0 ) )
						{
							// Get the first attribute value
							siResult = ::dsGetAttributeValue( aSearchNodeRef, dataBuff, 1, valueRef, &pValueEntry );
							
							// Is it what we expected
							if ( ( ::strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrXMLPlist ) == 0 ) &&
                                 ( pValueEntry->fAttributeValueData.fBufferData != nil ) )
							{
                                //build the dictionary here
                                CFDataRef			xmlData				= NULL;
                                CFPropertyListRef	configPropertyList	= NULL;
                                xmlData = CFDataCreate(	NULL, (UInt8*)pValueEntry->fAttributeValueData.fBufferData,
                                                        strlen(pValueEntry->fAttributeValueData.fBufferData) );
                                if (xmlData != nil)
                                {
                                    // extract the config dictionary from the XML data.
                                    configPropertyList = CFPropertyListCreateFromXMLData(	kCFAllocatorDefault,
                                                                                            xmlData,
                                                                                            kCFPropertyListMutableContainers, 
                                                                                            NULL );
                                    if (configPropertyList != nil )
                                    {
                                        //make the propertylist a dict
                                        if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
                                        {
                                            outDictionary = (CFMutableDictionaryRef) configPropertyList;
                                        }
                                        else
                                        {
                                            DSCFRelease(configPropertyList);
                                        }
                                    }
                                    CFRelease(xmlData);
                                }
							}
							else if ( strcmp(pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation) == 0 &&
									  pValueEntry->fAttributeValueData.fBufferData != nil )
							{
								pMetaNodeLocation = strdup( pValueEntry->fAttributeValueData.fBufferData );
							}
							
							if ( pValueEntry != NULL )
							{
								dsDeallocAttributeValueEntry( fDirRef, pValueEntry );
								pValueEntry = NULL;
							}
						}
						dsCloseAttributeValueList(valueRef);
						if (pAttrEntry != nil)
						{
							dsDeallocAttributeEntry(fDirRef, pAttrEntry);
							pAttrEntry = nil;
						}
					}
				}
				dsCloseAttributeList(attrListRef);
				if (pRecEntry != nil)
				{
					dsDeallocRecordEntry(fDirRef, pRecEntry);
					pRecEntry = nil;
				}
			}// got records returned
		} // if ( (bLookForConfig) && (fDirRef != 0) )
	}
	
	catch( SInt32 err )
	{
        DbgLog( kLogPlugin, "CSearchPlugin::FindAugmentConfigRecord failed with error %d", err );
	}	
	
	// let's replace the metanode location since the server may not be called exactly what the config record says
	if ( outDictionary != NULL && pMetaNodeLocation != NULL )
	{
		CFStringRef	cfTempString = CFStringCreateWithCString( kCFAllocatorDefault, pMetaNodeLocation, kCFStringEncodingUTF8 );
		
		if ( cfTempString != NULL )
		{
			CFDictionarySetValue( outDictionary, CFSTR(kXMLAugmentDirNodeNameKey), cfTempString );
			DSCFRelease( cfTempString );
		}
	}
	
	DSFree( pMetaNodeLocation );
	
	if ( recName != NULL )
	{
		dsDataListDeallocate( fDirRef, recName );
		free( recName );
		recName = NULL;
	}
	if ( recType != NULL )
	{
		dsDataListDeallocate( fDirRef, recType );
		free( recType );
		recType = NULL;
	}
	if ( attrTypes != NULL )
	{
		dsDataListDeallocate( fDirRef, attrTypes );
		free( attrTypes );
		attrTypes = NULL;
	}
	if ( dataBuff != NULL )
	{
		dsDataBufferDeAllocate( fDirRef, dataBuff );
		dataBuff = NULL;
	}
	if ( nodeName != NULL )
	{
		dsDataListDeallocate( fDirRef, nodeName );
		free( nodeName );
		nodeName = NULL;
	}
	if (aSearchNodeRef != 0)
	{
		dsCloseDirNode(aSearchNodeRef);
		aSearchNodeRef = 0;
	}
	
	return outDictionary;
} // FindAugmentConfigRecord
#endif


// ---------------------------------------------------------------------------
//	* EnsureCheckNodesThreadIsRunning
// ---------------------------------------------------------------------------

void CSearchPlugin::EnsureCheckNodesThreadIsRunning( tDirPatternMatch policyToCheck )
{
	if ( policyToCheck == eDSAuthenticationSearchNodeName && OSAtomicCompareAndSwap32Barrier(false, true, &fAuthCheckNodeThreadActive) == true )
	{
		CSearchPluginHandlerThread* aSearchPluginHandlerThread = new CSearchPluginHandlerThread(DSCThread::kTSSearchPlugInHndlrThread, 1, (void *)this);
		if (aSearchPluginHandlerThread != NULL)
			aSearchPluginHandlerThread->StartThread();
		//we don't keep a handle to the search plugin handler threads and don't check if they get created
	}
	else if ( policyToCheck == eDSContactsSearchNodeName && OSAtomicCompareAndSwap32Barrier(false, true, &fContactCheckNodeThreadActive) == true )
	{
		CSearchPluginHandlerThread* aSearchPluginHandlerThread = new CSearchPluginHandlerThread(DSCThread::kTSSearchPlugInHndlrThread, 3, (void *)this);
		if (aSearchPluginHandlerThread != NULL)
			aSearchPluginHandlerThread->StartThread();
		//we don't keep a handle to the search plugin handler threads and don't check if they get created
	}
	else
	{
		gNetworkTransition.PostEvent();
	}
	
} // EnsureCheckNodesThreadIsRunning

// ---------------------------------------------------------------------------
//     * CheckNodes - How often do we call this is the real question ie. if all nodes are not yet reachable
// ---------------------------------------------------------------------------

void CSearchPlugin::CheckNodes( tDirPatternMatch policyToCheck, int32_t *threadFlag, DSEventSemaphore *eventSemaphore )
{
	// This function is called by the extra thread only...
	
	sSearchConfig  *aSearchConfig		= nil;
	sSearchList	   *aSearchNodeList		= nil;
	sSearchList	   *aNodeListPtr		= nil;
	CFMutableArrayRef	aNodeList		= 0;
	CFStringRef			aNodeStr		= 0;
	bool			bTryAgain			= true;
	int				cntBeforeWaitLonger	= 0; //check four times and then double wait time until it maxes out to once every 15 minutes
	int				waitSeconds			= 5;

	DBGLOG1( kLogPlugin, "CSearchPlugin::CheckNodes: checking network node reachability on search policy %X", policyToCheck );
	
	while (bTryAgain)
	{
		bTryAgain = false;

		aNodeList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		
		//mutex -- get the node list to check reachability to the network nodes
		fMutex.WaitLock();
		
		aSearchConfig = FindSearchConfigWithKey( policyToCheck );
		if ( aSearchConfig != nil )
		{
			aSearchNodeList = aSearchConfig->fSearchNodeList;
			if (aSearchNodeList != nil)
			{
				aNodeListPtr = aSearchNodeList->fNext; //always skip the local node(s)
				aNodeListPtr = aNodeListPtr->fNext; //bsd is always 2nd now
				while (aNodeListPtr != nil)
				{
					if (aNodeListPtr->fNodeName != nil)
					{
						aNodeStr = CFStringCreateWithCString( kCFAllocatorDefault, aNodeListPtr->fNodeName, kCFStringEncodingUTF8 );
						CFArrayAppendValue(aNodeList, aNodeStr);
						CFRelease(aNodeStr);
					}
					aNodeListPtr = aNodeListPtr->fNext;
				}
			}
		}		
		
		fMutex.SignalLock();
		
		//start trying to open the nodes - loop over aNodeList
		CFIndex nodeCount = CFArrayGetCount(aNodeList);
		for (CFIndex indexCount = 0; indexCount < nodeCount; indexCount++)
		{
			aNodeStr = (CFStringRef)CFArrayGetValueAtIndex(aNodeList, indexCount);
			int maxLength = 1 + CFStringGetMaximumSizeForEncoding( CFStringGetLength(aNodeStr), kCFStringEncodingUTF8 );
			char *tmpStr = (char *)calloc(1, maxLength);
			CFStringGetCString(aNodeStr, tmpStr, maxLength, kCFStringEncodingUTF8);
			tDataListPtr aNodeName = dsBuildFromPathPriv( tmpStr, "/" );
			tDirNodeReference aNodeRef = 0;
			SInt32 result = dsOpenDirNode( fDirRef, aNodeName, &aNodeRef );
			if (result == eDSNoErr)
			{
				DBGLOG1( kLogPlugin, "CSearchPlugin::CheckNodes: calling dsOpenDirNode succeeded on node <%s>", tmpStr);
				dsCloseDirNode(aNodeRef);
			}
			else
			{
				DBGLOG1( kLogPlugin, "CSearchPlugin::CheckNodes: calling dsOpenDirNode failed on node <%s>", tmpStr);
				bTryAgain = true;
				gNetworkTransition.ResetEvent();
			}			
			
			//mutex -- get the node list to change the reachability setting
			fMutex.WaitLock();
			
			aSearchConfig = FindSearchConfigWithKey( policyToCheck );
			if ( aSearchConfig != nil )
			{
				aSearchNodeList = aSearchConfig->fSearchNodeList;
				if (aSearchNodeList != nil)
				{
					// always two now
					aNodeListPtr = aSearchNodeList->fNext; //always skip the local node(s)
					aNodeListPtr = aNodeListPtr->fNext; //always skip the local node(s)
					while (aNodeListPtr != nil)
					{
						if (strcmp(aNodeListPtr->fNodeName, tmpStr) == 0) //same node
						{
							bool newState = (result==eDSNoErr);
							
							if (aNodeListPtr->fNodeReachable != newState)
							{
								aNodeListPtr->fNodeReachable = newState;
								
								// if this node has never been opened, flush the cache
								if (newState && aNodeListPtr->fHasNeverOpened)
								{
									aNodeListPtr->fHasNeverOpened = false;
									if ( gCacheNode != NULL && policyToCheck == eDSAuthenticationSearchNodeName )
									{
										gCacheNode->EmptyCacheEntryType( CACHE_ENTRY_TYPE_ALL );
										DBGLOG1( kLogPlugin, "CSearchPlugin::CheckNodes first time reachability of <%s> flushing cache", tmpStr );
									}
								}
								else
								{
									if ( gCacheNode != NULL && policyToCheck == eDSAuthenticationSearchNodeName )
										gCacheNode->EmptyCacheEntryType( CACHE_ENTRY_TYPE_NEGATIVE );
									
									DBGLOG2( kLogPlugin, "CSearchPlugin::CheckNodes updating search policy for reachability of <%s> to <%s>", tmpStr,
											(newState ? "Available" : "Unavailable") );
								}
								
								//let all the context node references know about the update
								gSNNodeRef->DoOnAllItems(CSearchPlugin::ContextSetListChangedProc);
								
								// we only notify of positive changes as they are the ones that make a difference
								if (newState)
								{
									if ( policyToCheck == eDSAuthenticationSearchNodeName ) {
										gSrvrCntl->NodeSearchPolicyChanged();
									}
									else if ( policyToCheck == eDSContactsSearchNodeName ) {
										SCDynamicStoreRef store = SCDynamicStoreCreate( NULL, CFSTR("DirectoryService"), NULL, NULL );
										if ( store != NULL )
										{
											if ( SCDynamicStoreNotifyValue( store, CFSTR(kDSStdNotifyContactSearchPolicyChanged) ) )
												DbgLog( kLogNotice, "CSearchPlugin::CheckNodes - notify sent for contact search policy change" );
											DSCFRelease(store);
										}
									}
								}
								
								// update the reachability of the node
								//   if it is reachable we update all
								//   if it is not reachable, we do not update LDAPv3 since it does it directly
								if ( gCacheNode != NULL && policyToCheck == eDSAuthenticationSearchNodeName )
								{
									gCacheNode->UpdateNodeReachability( aNodeListPtr->fNodeName, aNodeListPtr->fNodeReachable );
								}
							}
							
							break;
						}
						aNodeListPtr = aNodeListPtr->fNext;
					}
				}
			}		
			
			fMutex.SignalLock();
			
			dsDataListDeallocatePriv( aNodeName );
			free( aNodeName );
			aNodeName = nil;
			DSFreeString(tmpStr);
		}
		
		CFRelease(aNodeList);
		aNodeList = 0;
		
		// post policy change event that we scanned the search nodes at least once
		eventSemaphore->PostEvent();
		
		//delay until next try again to check reachability of search policy nodes
		if (bTryAgain)
		{
			// if the event happens, we want to reset our counts, otherwise we'll just timeout and do our normal checking
			if( gNetworkTransition.WaitForEvent( waitSeconds * kMilliSecsPerSec ) == true )
			{
				cntBeforeWaitLonger = 0;
				waitSeconds = 5;
			}
			else
			{
				cntBeforeWaitLonger++;
				if (cntBeforeWaitLonger == 4)
				{
					cntBeforeWaitLonger = 0;
					waitSeconds = waitSeconds * 2;
					if (waitSeconds > 900)
					{
						waitSeconds = 900;
					}
					DBGLOG1( kLogPlugin, "CSearchPlugin::CheckNodes: adjusting check delay time to <%d> seconds for checking network node reachability", waitSeconds);
				}
			}
		}
	}

	CheckForAugmentConfig( policyToCheck );
	
	OSAtomicCompareAndSwap32Barrier( true, false, threadFlag );
	
} //CheckNodes

