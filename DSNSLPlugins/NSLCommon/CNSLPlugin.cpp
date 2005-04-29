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
 *  @header CNSLPlugin
 */

#define Use_CFStringGetInstallationEncodingAndRegion 1	// set to zero to manually read encoding and region from /var/root/.CFUserTextEncoding

#if Use_CFStringGetInstallationEncodingAndRegion
#include <CoreFoundation/CFStringDefaultEncoding.h>
#endif

#include "CNSLHeaders.h"
#include <mach/mach_time.h>	// for dsTimeStamp
#include "CNSLTimingUtils.h"

#ifndef kServerRunLoop
#define kServerRunLoop (eDSPluginCalls)(kHandleNetworkTransition + 1)
#endif

#define kNSLBuffDataDefaultSize		256
#define	kNSLRecBuffDataDefaultSize	512

//#define LOG_CLEAR_DATA_BUFFER 1
#ifdef LOG_CLEAR_DATA_BUFFER
	#define CLEAR_DATA_BUFFER( dataBuff )		{ \
										if ( dataBuff->GetSize() > kNSLBuffDataDefaultSize ) \
											syslog(LOG_NOTICE, "clearing data buff [0x%x] that had grown to %d bytes: File %s; Line %ld\n", dataBuff, dataBuff->GetSize(), __FILE__, (long)__LINE__ );  \
										dataBuff->Clear(kNSLBuffDataDefaultSize); \
									}

	#define UNLOCK_SERVICE_REFS		{ \
										NSLLog(LOG_NOTICE, "Unlocking Service Refs: File %s; Line %ld\n", __FILE__, (long)__LINE__ );  \
										gRequestMgr->UnlockServiceRefs(); \
									}
#else
	#define CLEAR_DATA_BUFFER( dataBuff ) dataBuff->Clear(kNSLBuffDataDefaultSize)
#endif

// since our plugins are now lazily loaded, we don't need to wait for NSL to activate us.
//#define DONT_WAIT_FOR_NSL_TO_ACTIVATE_US

#define kMinTimeBetweenNodeDefaultStateDiscoveriesConsideredValid		2

static const	uInt32	kBuffPad	= 16;

int LogHexDump(char *pktPtr, long pktLen);

boolean_t NetworkChangeCallBack(SCDynamicStoreRef session, void *callback_argument);

sInt32 CleanContextData ( sNSLContextData *inContext );
sNSLContextData* MakeContextData ( void );

// These are the CFContainer callback protos
void NSLReleaseNodeData( CFAllocatorRef allocator, const void* value );
CFStringRef NSLNodeValueCopyDesctriptionCallback ( const void *item );
Boolean NSLNodeValueEqualCallback ( const void *value1, const void *value2 );
void NSLNodeHandlerFunction(const void *inKey, const void *inValue, void *inContext);

typedef struct AttrDataContext {
    sInt32		count;
    CDataBuff*	attrDataBuf;
    Boolean		attrOnly;
} AttrDataContext;

void AddToAttrData(const void *key, const void *value, void *context);

sInt16	gOutstandingSearches = 0;
pthread_mutex_t	gOutstandingSearchesLock = PTHREAD_MUTEX_INITIALIZER;
CNSLPlugin* CNSLPlugin::gsTheNSLPlugin = NULL;

void SearchStarted( void )
{
    pthread_mutex_lock(&gOutstandingSearchesLock);
    gOutstandingSearches++;
    pthread_mutex_unlock(&gOutstandingSearchesLock);
	DBGLOG( "SearchStarted, gOutstandingSearches=%d\n", gOutstandingSearches );
}

void SearchCompleted( void )
{
    pthread_mutex_lock(&gOutstandingSearchesLock);

    gOutstandingSearches--;
    
	DBGLOG( "SearchCompleted, gOutstandingSearches=%d\n", gOutstandingSearches );
    pthread_mutex_unlock(&gOutstandingSearchesLock);

    if ( CNSLPlugin::TheNSLPlugin()->OKToStartNewSearch() )
	{
		CNSLPlugin::TheNSLPlugin()->LockSearchQueue();
        CNSLPlugin::TheNSLPlugin()->StartNextQueuedSearch();		// just give this a chance to fire one off
		CNSLPlugin::TheNSLPlugin()->UnlockSearchQueue();
	}
}

#pragma mark -
CFStringRef NSLQueuedSearchesCopyDesctriptionCallback ( const void *item )
{
    return kDSNSLQueuedSearchSAFE_CFSTR;
}

Boolean NSLQueuedSearchesEqualCallback ( const void *item1, const void *item2 )
{
    return item1 == item2;
}

NodeData * AllocateNodeData()
{
	NodeData *newNode = (NodeData*)calloc( 1, sizeof(NodeData) );
	return newNode;
}

void DeallocateNodeData( NodeData *nodeData )
{
	if ( nodeData != NULL )
	{
		if ( nodeData->fNodeName )
			::CFRelease( nodeData->fNodeName );
		nodeData->fNodeName = NULL;
		
		if ( nodeData->fServicesRefTable )
			::CFRelease( nodeData->fServicesRefTable );
		nodeData->fServicesRefTable = NULL;
					
		if ( nodeData->fDSName )
		{
			dsDataListDeallocatePriv( nodeData->fDSName );
			free( nodeData->fDSName );
			nodeData->fDSName = NULL;
		}
		
		free( nodeData );
	}
}


/************************************************
 *
 *	Subclass needs to implement the following...
 *
 ************************************************
extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0xD9, 0x70, 0xD5, 0x2E, 0xD5, 0x15, 0x11, 0xD3, \
								0x9F, 0xF9, 0x00, 0x05, 0x02, 0xC1, 0xC7, 0x36 );

}

static CDSServerModule* _Creator ( void )
{
	return( new CNSLPlugin );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;
*/

CNSLPlugin::CNSLPlugin( void )
{
	DBGLOG( "CNSLPlugin::CNSLPlugin\n" );
	mPublishedNodes = NULL;
    mLastNodeLookupStartTime = 0;
    mOpenRefTable = NULL;
    mActivatedByNSL = false;
    
    mDSLocalNodeLabel = NULL;
    mDSNetworkNodeLabel = NULL;
    mSearchQueue = NULL;
	mRunLoopRef = NULL;
	mTimerRef = NULL;
	mNodeLookupTimerRef = NULL;
	mSearchTicklerInstalled = false;
	mNodeLookupTimerInstalled = false;
	mState		= kUnknownState;
    mLastTimeEmptyResultsReturned = 0;
	
    gsTheNSLPlugin = this;
} // CNSLPlugin


CNSLPlugin::~CNSLPlugin( void )
{
	DBGLOG( "CNSLPlugin::~CNSLPlugin\n" );
	
	if ( mTimerRef )
	{
		UnInstallSearchTickler();
	}
	
	if ( mNodeLookupTimerRef )
	{
		UnInstallNodeLookupTimer();
	}
	
    if ( mPublishedNodes )
    {
        ClearOutAllNodes();
        ::CFDictionaryRemoveAllValues( mPublishedNodes );
        ::CFRelease( mPublishedNodes );
        mPublishedNodes = NULL;
    }
    
    if ( mOpenRefTable )
    {
        ::CFDictionaryRemoveAllValues( mOpenRefTable );
        ::CFRelease( mOpenRefTable );
        mOpenRefTable = NULL;
    }

    if ( mSearchQueue )
        ::CFRelease( mSearchQueue );
		
} // ~CNSLPlugin

// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

sInt32 CNSLPlugin::Validate ( const char *inVersionStr, const uInt32 inSignature )
{
	mSignature = inSignature;

	return( noErr );

} // Validate


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

sInt32 CNSLPlugin::Initialize( void )
{
    sInt32				siResult	= eDSNoErr;

	DBGLOG( "CNSLPlugin::Initialize\n" );
	// database initialization
    CFDictionaryKeyCallBacks	keyCallBack;
    CFDictionaryValueCallBacks	valueCallBack;
    
    valueCallBack.version = 0;
    valueCallBack.retain = NULL;
    valueCallBack.release = NSLReleaseNodeData;
    valueCallBack.copyDescription = NSLNodeValueCopyDesctriptionCallback;
    valueCallBack.equal = NSLNodeValueEqualCallback;

	if ( !mPublishedNodes )
		mPublishedNodes = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &valueCallBack);
    
	CFArrayCallBacks	callBack;
    
    callBack.version = 0;
    callBack.retain = NULL;
    callBack.release = NULL;
    callBack.copyDescription = NSLQueuedSearchesCopyDesctriptionCallback;
    callBack.equal = NSLQueuedSearchesEqualCallback;
    
    if ( !mSearchQueue )
		mSearchQueue= ::CFArrayCreateMutable( NULL, 0, &callBack );

    pthread_mutex_init( &mQueueLock, NULL );
	pthread_mutex_init( &mPluginLock, NULL );
	pthread_mutex_init( &mOpenRefTableLock, NULL );
    pthread_mutex_init( &mSearchQueueLock, NULL );
    pthread_mutex_init( &mSearchLookupTimerLock, NULL );
    pthread_mutex_init( &mNodeLookupTimerLock, NULL );
	
    // use these for the reftable dictionary
    keyCallBack.version = 0;
    keyCallBack.retain = NULL;
    keyCallBack.release = NULL;
    keyCallBack.copyDescription = NULL;
    keyCallBack.equal = NULL;
    keyCallBack.hash = NULL;

    valueCallBack.release = NULL;
    valueCallBack.copyDescription = NULL;
    valueCallBack.equal = NULL;
    
    if ( !mOpenRefTable )
		mOpenRefTable = ::CFDictionaryCreateMutable( NULL, 0, &keyCallBack, &valueCallBack );
		
	if ( !mOpenRefTable )
		DBGLOG("************* mOpenRefTable is NULL ***************\n");

    if ( !siResult )
    {
        // set the init flags
        mState = kInitialized | kInactive;
    
    // don't start a periodic task until we get activated
    }
    
    if ( !siResult )
        siResult = InitPlugin();		// initialize our subclass

    return siResult;
} // Initialize

// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CNSLPlugin::WaitForInit( void )
{
	volatile	uInt32		uiAttempts	= 0;

	while ( !(mState & kInitialized) &&
			!(mState & kFailedToInit) )
	{
		// Try for 2 minutes before giving up
		if ( uiAttempts++ >= 240 )
		{
			return;
		}
		// Now wait until we are told that there is work to do or
		//	we wake up on our own and we will look for ourselves

		SmartSleep( (uInt32)(50000) );
	}
} // WaitForInit

Boolean CNSLPlugin::IsActive( void )
{
	Boolean		isActive = ((mState & kActive) || !(mState & kInactive));
	
	return isActive; 
}

#pragma mark -
void PeriodicTimerCallback( CFRunLoopTimerRef timer, void *info );
void PeriodicTimerCallback( CFRunLoopTimerRef timer, void *info )
{
	CNSLPlugin*		plugin = (CNSLPlugin*)info;
	
#ifdef TRACK_FUNCTION_TIMES
	CFAbsoluteTime	startTime = CFAbsoluteTimeGetCurrent();
#endif
	plugin->NSLSearchTickler();

#ifdef TRACK_FUNCTION_TIMES
	syslog( LOG_ERR, "(%s) NSLSearchTickler took %f seconds\n", plugin->GetProtocolPrefixString(), CFAbsoluteTimeGetCurrent() - startTime );
#endif
}

void CNSLPlugin::InstallSearchTickler( void )
{
	if ( !mSearchTicklerInstalled && mRunLoopRef )
	{
		DBGLOG( "CNSLPlugin::InstallSearchTickler (%s)\n", GetProtocolPrefixString() );
		CFRunLoopTimerContext 	c = {0, this, NULL, NULL, NULL};
	
		if ( !mTimerRef )
			mTimerRef = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent()+kTaskInterval, 0, 0, 0, PeriodicTimerCallback, (CFRunLoopTimerContext*)&c);

		CFRunLoopAddTimer(mRunLoopRef, mTimerRef, kCFRunLoopDefaultMode);
	}
	
	mSearchTicklerInstalled = true;
}

void CNSLPlugin::UnInstallSearchTickler( void )
{
	if ( mSearchTicklerInstalled && mRunLoopRef )
	{
		DBGLOG( "CNSLPlugin::UnInstallSearchTickler (%s)\n", GetProtocolPrefixString() );
		if ( mTimerRef )
		{
			CFRunLoopRemoveTimer( mRunLoopRef, mTimerRef, kCFRunLoopDefaultMode );
			CFRelease( mTimerRef );
			mTimerRef = NULL;
		}
	}
	
	mSearchTicklerInstalled = false;
}

sInt32 CNSLPlugin::NSLSearchTickler( void )
{
    sInt32				siResult	= eDSNoErr;

	LockSearchQueue();

    if ( IsActive() && mActivatedByNSL )
    {
		DBGLOG( "CNSLPlugin::NSLSearchTickler, Outstanding Searches: %ld, queued Searches: %ld\n", NumOutstandingSearches(), (mSearchQueue)?CFArrayGetCount(mSearchQueue):-1 );

		while ( NumOutstandingSearches() < kMaxNumOutstandingSearches && mSearchQueue && CFArrayGetCount(mSearchQueue) > 0 )
			StartNextQueuedSearch();		// just give this a chance to fire one off

		if ( NumOutstandingSearches() == 0 )
			UnInstallSearchTickler();

		if ( mSearchTicklerInstalled )
			CFRunLoopTimerSetNextFireDate( mTimerRef, CFAbsoluteTimeGetCurrent()+kTaskInterval );
    }
    else if ( mSearchTicklerInstalled )
		UnInstallSearchTickler();

	UnlockSearchQueue();

    return( siResult );
} // NSLTickler

void CNSLPlugin::CancelCurrentlyQueuedSearches( void )
{
    LockSearchQueue();

	while ( mSearchQueue && CFArrayGetCount(mSearchQueue) > 0 )
    {
        CNSLServiceLookupThread*	newLookup = (CNSLServiceLookupThread*)::CFArrayGetValueAtIndex( mSearchQueue, 0 );
        ::CFArrayRemoveValueAtIndex( mSearchQueue, 0 );
		delete( newLookup );
	}

    UnlockSearchQueue();
}

#pragma mark -
void NodeLookupTimerCallback( CFRunLoopTimerRef timer, void *info );
void NodeLookupTimerCallback( CFRunLoopTimerRef timer, void *info )
{
	CNSLPlugin*		plugin = (CNSLPlugin*)info;
	
#ifdef TRACK_FUNCTION_TIMES
	CFAbsoluteTime	startTime = CFAbsoluteTimeGetCurrent();
#endif
	plugin->PeriodicNodeLookupTask();

#ifdef TRACK_FUNCTION_TIMES
	syslog( LOG_ERR, "(%s) PeriodicNodeLookupTask took %f seconds\n", plugin->GetProtocolPrefixString(), CFAbsoluteTimeGetCurrent() - startTime );
#endif
}

void CNSLPlugin::InstallNodeLookupTimer( void )
{
	LockNodeLookupTimer();
	if ( !mNodeLookupTimerInstalled && mActivatedByNSL && mRunLoopRef )
	{
		DBGLOG( "CNSLPlugin::InstallNodeLookupTimer (%s) called\n", GetProtocolPrefixString() );

		CFRunLoopTimerContext 	c = {0, this, NULL, NULL, NULL};
	
		if ( !mNodeLookupTimerRef )
			mNodeLookupTimerRef = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent()+kNodeTimerIntervalImmediate, kKeepTimerAroundAfterFiring, 0, 0, NodeLookupTimerCallback, (CFRunLoopTimerContext*)&c);

		CFRunLoopAddTimer(mRunLoopRef, mNodeLookupTimerRef, kCFRunLoopDefaultMode);
	}
	
	mNodeLookupTimerInstalled = true;
	UnlockNodeLookupTimer();
}

void CNSLPlugin::UnInstallNodeLookupTimer( void )
{
	LockNodeLookupTimer();
	if ( mNodeLookupTimerInstalled && mRunLoopRef )
	{
		DBGLOG( "CNSLPlugin::UnInstallNodeLookupTimer (%s)\n", GetProtocolPrefixString() );

		if ( mNodeLookupTimerRef )
		{
			CFRunLoopRemoveTimer( mRunLoopRef, mNodeLookupTimerRef, kCFRunLoopDefaultMode );
			CFRelease( mNodeLookupTimerRef );
			mNodeLookupTimerRef = NULL;
		}
	}
	
	mNodeLookupTimerInstalled = false;
	UnlockNodeLookupTimer();
}

void CNSLPlugin::ResetNodeLookupTimer( UInt32 timeTillNewLookup )
{
	LockNodeLookupTimer();
	
	if ( mNodeLookupTimerInstalled && mRunLoopRef )
	{
		DBGLOG( "CNSLPlugin::ResetNodeLookupTimer (%s) setting timer to fire %d seconds from now\n", GetProtocolPrefixString(), timeTillNewLookup );

		CFRunLoopTimerSetNextFireDate( mNodeLookupTimerRef, CFAbsoluteTimeGetCurrent() + timeTillNewLookup );
	}
	
	UnlockNodeLookupTimer();
}

void CNSLPlugin::PeriodicNodeLookupTask( void )
{
	if ( mActivatedByNSL && IsActive() )
    {
		DBGLOG( "CNSLPlugin::PeriodicNodeLookupTask (%s), time to start new node lookup\n", GetProtocolPrefixString() );

		StartNodeLookup();
	}
	else
	{
		DBGLOG( "CNSLPlugin::PeriodicNodeLookupTask (%s), called but mActivatedByNSL== %d && IsActive() == %d!\n", GetProtocolPrefixString(), mActivatedByNSL, IsActive() );
	}
} // PeriodicNodeLookupTask

#pragma mark -

sInt32 CNSLPlugin::ProcessRequest ( void *inData )
{
	sInt32		siResult	= 0;

	WaitForInit();
	
	if ( inData == nil )
	{
        DBGLOG( "CNSLPlugin::ProcessRequest, inData is NULL!\n" );
		return( ePlugInDataError );
	}
    
	if ( (mState & kFailedToInit) )
	{
        DBGLOG( "CNSLPlugin::ProcessRequest, kFailedToInit!\n" );
        return( ePlugInFailedToInitialize );
	}
	
	if ( ((sHeader *)inData)->fType == kServerRunLoop )
	{
        DBGLOG( "CNSLPlugin::ProcessRequest, received a RunLoopRef, 0x%x\n",((sHeader *)inData)->fContextData );
		return SetServerIdleRunLoopRef( (CFRunLoopRef)(((sHeader *)inData)->fContextData) );
	}
	else if ( !IsActive() )
	{
		if ( ((sHeader *)inData)->fType == kOpenDirNode )
		{
			// these are ok when we are inactive if this is the top level
			char			   *pathStr				= nil;
			tDataListPtr		pNodeList			= nil;
			char			   *protocolStr			= nil;

			pNodeList	=	((sOpenDirNode *)inData)->fInDirNodeName;
			pathStr = dsGetPathFromListPriv( pNodeList, (char *)"\t" );
			protocolStr = pathStr + 1;
			        
			if ( strstr( protocolStr, "NSLActivate" ) )
			{
				// we have a directed open for our plugins, just set mActivated to true
				mActivatedByNSL = true;

				free( pathStr );
				
				return( ePlugInNotActive );
			}
			else if ( pathStr && GetProtocolPrefixString() && strcmp( protocolStr, GetProtocolPrefixString() ) == 0 )
			{
				DBGLOG( "CNSLPlugin::ProcessRequest (kOpenDirNode), plugin not active, open on (%s) ok\n", protocolStr );
				free( pathStr );
			}
			else
			{
				DBGLOG( "CNSLPlugin::ProcessRequest (kOpenDirNode), plugin not active, returning ePlugInNotActive on open (%s)\n", protocolStr );
				free( pathStr );
				return( ePlugInNotActive );
			}
		}
		else if ( ((sHeader *)inData)->fType == kCloseDirNode )
		{
			DBGLOG( "CNSLPlugin::ProcessRequest (kCloseDirNode), plugin not active, returning noErr\n" );
		}
		else if ( ((sHeader *)inData)->fType != kDoPlugInCustomCall )
		{
			DBGLOG( "CNSLPlugin::ProcessRequest (%d), plugin not active!\n", ((sHeader *)inData)->fType );
			return( ePlugInNotActive );
		}
    }
	
	siResult = HandleRequest( inData );

	return( siResult );

} // ProcessRequest

#pragma mark -

sInt32 CNSLPlugin::SetServerIdleRunLoopRef( CFRunLoopRef idleRunLoopRef )
{
	mRunLoopRef = idleRunLoopRef;
	
	return eDSNoErr;
}

// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

sInt32 CNSLPlugin::SetPluginState ( const uInt32 inState )
{
// don't allow any changes other than active / in-active
	WaitForInit();
	
	DBGLOG( "CNSLPlugin::SetPluginState(%s):", GetProtocolPrefixString() );

	if ( (kActive & inState) && (mState & kInactive) ) // want to set to active only if currently inactive
    {
        DBGLOG( "kActive\n" );
        if ( mState & kInactive )
            mState -= kInactive;
            
        if ( !(mState & kActive) )
            mState += kActive;

#ifdef DONT_WAIT_FOR_NSL_TO_ACTIVATE_US				
		mActivatedByNSL = true;	// for now we are activating ourselves
#endif

		ActivateSelf();
    }

	if ( (kInactive & inState) && (mState & kActive) ) // want to set to inactive only if currently active
    {
        DBGLOG( "kInactive\n" );
        if ( !(mState & kInactive) )
            mState += kInactive;
            
        if ( mState & kActive )
            mState -= kActive;

		DeActivateSelf();
    }

	return( eDSNoErr );

} // SetPluginState

void CNSLPlugin::ActivateSelf( void )
{	
	if ( mActivatedByNSL )
	{
		ZeroLastNodeLookupStartTime();

		InstallNodeLookupTimer();
	}
}

void CNSLPlugin::DeActivateSelf( void )
{
	// we need to deregister all our nodes

	UnInstallSearchTickler();
	UnInstallNodeLookupTimer();
	
	ClearOutAllNodes();
}

#pragma mark -
void CNSLPlugin::AddNode( CFStringRef nodeNameRef, Boolean isLocalNode )
{
    if ( !nodeNameRef || !IsActive() )
        return;

    char		nodeString[1024] = {0};
    CFIndex		bufferLen = sizeof(nodeString);
    
    if ( CFStringGetCString( nodeNameRef, nodeString, bufferLen, kCFStringEncodingUTF8 ) )
    {
        AddNode( nodeString, isLocalNode );
    }
    else
        fprintf(stderr, "CNSLPlugin::AddNode, CFStringGetCString failed!");
}

void CNSLPlugin::AddNode( const char* nodeName, Boolean isLocalNode )
{
    if ( !nodeName || !IsActive() )
        return;
        
    NodeData*		node = NULL;
//    CFStringRef		nodeRef = CFStringCreateWithCString( NULL, nodeName, kCFStringEncodingUTF8 );
    bool			isADefaultNode = false;
    bool			isADefaultOnlyNode = false;
    CFMutableStringRef		nodeRef = CFStringCreateMutable( NULL, 0 );
    
	if ( nodeRef )
	{
		CFStringAppendCString( nodeRef, GetProtocolPrefixString(), kCFStringEncodingUTF8 );
		CFStringAppend( nodeRef, CFSTR("\t") );
		CFStringAppendCString( nodeRef, nodeName, kCFStringEncodingUTF8 );
		
		DBGLOG( "CNSLPlugin::AddNode (%s) called with %snode %s\n", GetProtocolPrefixString(), (isLocalNode)?"local ":"", nodeName );

		LockPublishedNodes();
		if ( ::CFDictionaryContainsKey( mPublishedNodes, nodeRef ) )
			node = (NodeData*)::CFDictionaryGetValue( mPublishedNodes, nodeRef );
		
		if ( isLocalNode || IsLocalNode( nodeName ) )
			isADefaultNode = true;
		
		if ( isADefaultNode )
			isADefaultOnlyNode = IsADefaultOnlyNode( nodeName );
			
		if ( node && isADefaultNode != node->fIsADefaultNode )
		{
			if ( isADefaultNode || (node->fTimeStamp + kMinTimeBetweenNodeDefaultStateDiscoveriesConsideredValid < GetCurrentTime()) )
			{
				DBGLOG( "this node [%s] is being republished and has a different default state.  We will deregister the old one and create a new one.", nodeName );
				// this node is being republished and has a different default state.  We will deregister the old one and create a new one.
	//fprintf( stderr, "CNSLPlugin::AddNode calling DSUnregisterNode (%s) as isADefaultNode:(%d), node->fIsADefaultNode:(%d)\n", nodeName, isADefaultNode, node->fIsADefaultNode );
				DSUnregisterNode( mSignature, node->fDSName );
				
				::CFDictionaryRemoveValue( mPublishedNodes, node->fNodeName );		// remove it from the dictionary
		
	//			DeallocateNodeData( node );
				
				node = NULL;
			}
		}
		
		if ( node )
		{
			node->fTimeStamp = GetCurrentTime();	// update this node
		}
		else
		{
			// we have a new node
			DBGLOG( "CNSLPlugin::AddNode (%s) Adding new %snode %s\n", GetProtocolPrefixString(), (isADefaultNode)?"local ":"", nodeName );
			node = AllocateNodeData();
			
			node->fNodeName = nodeRef;
			CFRetain( node->fNodeName );
			
			node->fDSName = dsBuildListFromStringsPriv(GetProtocolPrefixString(), nodeName, nil);
	
			node->fTimeStamp = GetCurrentTime();
			
			node->fServicesRefTable = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			
			node->fIsADefaultNode = isADefaultNode;
			node->fSignature = mSignature;
			
			if ( node->fDSName )
			{
				eDirNodeType		nodeTypeToRegister = kUnknownNodeType;
				
				if ( isADefaultNode )
					nodeTypeToRegister = kDefaultNetworkNodeType;
	
				if ( !isADefaultOnlyNode )
					nodeTypeToRegister = (eDirNodeType)( nodeTypeToRegister | kDirNodeType );
					
				if ( nodeTypeToRegister != kUnknownNodeType )
					DSRegisterNode( mSignature, node->fDSName, nodeTypeToRegister );
			}
			
			::CFDictionaryAddValue( mPublishedNodes, nodeRef, node );
		}
		
		CFRelease( nodeRef );
		
		UnlockPublishedNodes();
	}
}

void CNSLPlugin::RemoveNode( CFStringRef nodeNameRef )
{
    NodeData*				node = NULL;
    CFMutableStringRef		modNodeRef = CFStringCreateMutable( NULL, 0 );
    
	if ( modNodeRef && nodeNameRef )
	{
		CFStringAppendCString( modNodeRef, GetProtocolPrefixString(), kCFStringEncodingUTF8 );
		CFStringAppend( modNodeRef, CFSTR("\t") );
		CFStringAppend( modNodeRef, nodeNameRef );
		
		if ( IsNSLDebuggingEnabled() )
		{
			char		nodeName[1024] = {0,};
			
			CFStringGetCString( modNodeRef, nodeName, sizeof(nodeName), kCFStringEncodingUTF8 );
			DBGLOG( "CNSLPlugin::RemoveNode called on [%s/%s]", GetProtocolPrefixString(), nodeName );
		}
	
		LockPublishedNodes();
		node = (NodeData*)::CFDictionaryGetValue( mPublishedNodes, modNodeRef );

		if ( node )
		{
	// now taken care of in the release
	//        DSUnregisterNode( mSignature, node->fDSName );
			
			::CFDictionaryRemoveValue( mPublishedNodes, node->fNodeName );		// remove it from the dictionary

	// now taken care of in the release
	//        DeallocateNodeData( node );
			node = NULL;
		}
		else
			DBGLOG( "CNSLPlugin::RemoveNode called on a node that isn't published!\n" );

		UnlockPublishedNodes();
	
		CFRelease( modNodeRef );
	}
	else if ( nodeNameRef )
		DBGLOG( "CNSLPlugin::RemoveNode unable to allocate a string!" );
	else
		DBGLOG( "CNSLPlugin::RemoveNode called on a NULL name!" );
}

void CNSLPlugin::NodeLookupComplete( void )
{
	DBGLOG( "CNSLPlugin::NodeLookupComplete\n" );
    ClearOutStaleNodes();			// get rid of any that are old
} // NodeLookupComplete

const char* CNSLPlugin::GetLocalNodeString( void )
{
    return NULL;			// plugin should override
}


Boolean CNSLPlugin::IsLocalNode( const char *inNode )
{
    #pragma unused (inNode)
    
    return false;			// plugin should override
}

Boolean CNSLPlugin::IsADefaultOnlyNode( const char *inNode )
{
    #pragma unused (inNode)
    
    return false;			// plugin should override
}

#pragma mark -

CFStringRef NetworkChangeNSLCopyStringCallback( const void *item );
CFStringRef NetworkChangeNSLCopyStringCallback( const void *item )
{
	return kNetworkChangeNSLCopyStringCallbackSAFE_CFSTR;
}

static void DoNetworkTransition(CFRunLoopTimerRef timer, void *info);
void DoNetworkTransition(CFRunLoopTimerRef timer, void *info)
{
	if ( info != nil )
	{
	//do something if the wait period has passed
#ifdef TRACK_FUNCTION_TIMES
		CFAbsoluteTime	startTime = CFAbsoluteTimeGetCurrent();
#endif
		((CNSLPlugin *)info)->HandleNetworkTransitionIfTime();

#ifdef TRACK_FUNCTION_TIMES
		syslog( LOG_ERR, "(%s) HandleNetworkTransitionIfTime took %f seconds\n", ((CNSLPlugin *)info)->GetProtocolPrefixString(), CFAbsoluteTimeGetCurrent() - startTime );
#endif
	}
}// DoNIPINetworkChange

#pragma mark -
// ---------------------------------------------------------------------------
//	* HandleRequest
//
// ---------------------------------------------------------------------------

sInt32 CNSLPlugin::HandleRequest( void *inData )
{
	sInt32	siResult	= 0;
	sHeader	*pMsgHdr	= nil;

	if ( inData == nil )
	{
		return( -8088 );
	}

#ifdef TRACK_FUNCTION_TIMES
	CFAbsoluteTime		startTime = CFAbsoluteTimeGetCurrent();
#endif
	pMsgHdr = (sHeader *)inData;

	switch ( pMsgHdr->fType )
	{
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
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kGetRecordEntry yet\n" );
			siResult = eNotHandledByThisNode;
			break;
			
		case kGetAttributeEntry:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kGetAttributeEntry yet\n" );
			siResult = GetAttributeEntry( (sGetAttributeEntry *)inData );
			break;
			
		case kGetAttributeValue:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kGetAttributeValue yet\n" );
			siResult = GetAttributeValue( (sGetAttributeValue *)inData );
			break;
			
		case kOpenRecord:
			siResult = OpenRecord( (sOpenRecord *)inData );
			break;
			
		case kGetRecordReferenceInfo:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kGetRecordReferenceInfo yet\n" );
			siResult = eNotHandledByThisNode;
			break;
			
		case kGetRecordAttributeInfo:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kGetRecordAttributeInfo yet\n" );
			siResult = eNotHandledByThisNode;
			break;
			
		case kGetRecordAttributeValueByID:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kGetRecordAttributeValueByID yet\n" );
			siResult = eNotHandledByThisNode;
			break;
			
		case kGetRecordAttributeValueByIndex:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kGetRecordAttributeValueByIndex yet\n" );
			siResult = eNotHandledByThisNode;
			break;
			
		case kFlushRecord:
			siResult = FlushRecord( (sFlushRecord *)inData );
			break;
			
		case kCloseAttributeList:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kCloseAttributeList yet\n" );
			siResult = eNotHandledByThisNode;
			break;

		case kCloseAttributeValueList:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kCloseAttributeValueList yet\n" );
			siResult = eNotHandledByThisNode;
			break;

		case kCloseRecord:
			siResult = CloseRecord( (sCloseRecord*)inData );
			break;
			
		case kSetRecordName:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kSetRecordName yet\n" );
			siResult = eNotHandledByThisNode;
			break;
			
		case kSetRecordType:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kSetRecordType yet\n" );
			siResult = eNotHandledByThisNode;
			break;
			
		case kDeleteRecord:
			siResult = DeleteRecord( (sDeleteRecord*)inData );
			break;
			
		case kCreateRecord:
		case kCreateRecordAndOpen:
			siResult = CreateRecord( (sCreateRecord *)inData );
			break;
			
		case kAddAttribute:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kAddAttribute yet\n" );
			siResult = eNotHandledByThisNode;
			break;
			
		case kRemoveAttribute:
			siResult = RemoveAttribute( (sRemoveAttribute*)inData );
			break;
			
		case kAddAttributeValue:
            DBGLOG( "CNSLPlugin::HandleRequest, calling AddAttributeValue\n" );
			siResult = AddAttributeValue( (sAddAttributeValue *)inData );
			break;
			
		case kRemoveAttributeValue:
			siResult = RemoveAttributeValue( (sRemoveAttributeValue*)inData );
			break;
			
		case kSetAttributeValue:
			siResult = SetAttributeValue( (sSetAttributeValue*)inData );
			break;
			
		case kDoDirNodeAuth:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kDoDirNodeAuth yet\n" );
			siResult = eNotHandledByThisNode;
			break;
			
		case kDoAttributeValueSearch:
		case kDoAttributeValueSearchWithData:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kDoAttributeValueSearch or kDoAttributeValueSearchWithData yet\n" );
			siResult = eNotHandledByThisNode;
			break;

		case kDoPlugInCustomCall:
			siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
			break;
            
        case kHandleNetworkTransition:
            DBGLOG( "CNSLPlugin::HandleRequest, we have some sort of network transition\n" );
			//let us be smart about doing the recheck
			//we don't want to re-init multiple times during this wait period
			//however we do go ahead and fire off timers each time
			//each call in here we update the delay time by 5 seconds
			mTransitionCheckTime = time(nil) + 5;
		
			if (mRunLoopRef != nil)
			{
				CFRunLoopTimerContext c = {0, (void*)this, NULL, NULL, NetworkChangeNSLCopyStringCallback};
			
				CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	NULL,
																CFAbsoluteTimeGetCurrent() + 5,
																0,
																0,
																0,
																DoNetworkTransition,
																(CFRunLoopTimerContext*)&c);
			
				CFRunLoopAddTimer(mRunLoopRef, timer, kCFRunLoopDefaultMode);
				if (timer) CFRelease(timer);
			}
//			siResult = HandleNetworkTransition( (sHeader*)inData );
			break;

			
		default:
			siResult = eNotHandledByThisNode;
			break;
	}
    
#ifdef TRACK_FUNCTION_TIMES
	ourLog( "CNSLPlugin::HandleRequest( %d ) took %f seconds\n", pMsgHdr->fType, CFAbsoluteTimeGetCurrent()-startTime );
#endif

	pMsgHdr->fResult = siResult;

    if ( siResult )
        DBGLOG( "CNSLPlugin::HandleRequest returning %ld on a request of type %ld\n", siResult, pMsgHdr->fType );

	return( siResult );

} // HandleRequest

sInt32 CNSLPlugin::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
    sInt32				siResult			= eNotHandledByThisNode;	// plugins can override
	
	return siResult;		
}

sInt32 CNSLPlugin::GetDirNodeInfo( sGetDirNodeInfo *inData )
{
	sInt32				siResult		= eDSNoErr;
	uInt32				uiOffset		= 0;
	uInt32				uiCntr			= 1;
	uInt32				uiAttrCnt		= 0;
	CAttributeList	   *inAttrList		= nil;
	char			   *pAttrName		= nil;
	CBuff				outBuff;
	CDataBuff		   *aRecData		= nil;
	CDataBuff		   *aAttrData		= nil;
	CDataBuff		   *aTmpData		= nil;
	char			   *pData			= nil;
	sNSLContextData	   *pAttrContext	= nil;
	CNSLDirNodeRep*		nodeRep = NULL;

	LockPlugin();
		
	try
	{
		if ( inData  == nil ) throw( (sInt32)eMemoryError );

		nodeRep = (CNSLDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
		if ( nodeRep  == nil ) throw( (sInt32)eDSBadContextData );

		nodeRep->Retain();

		inAttrList = new CAttributeList( inData->fInDirNodeInfoTypeList );
		if ( inAttrList == nil ) throw( (sInt32)eDSNullNodeInfoTypeList );
		if (inAttrList->GetCount() == 0) throw( (sInt32)eDSEmptyNodeInfoTypeList );

		siResult = outBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff.SetBuffType( 'Gdni' );  //can't use 'StdB' since a tRecordEntry is not returned
		if ( siResult != eDSNoErr ) throw( siResult );

		aRecData = new CDataBuff(kNSLBuffDataDefaultSize);
		if ( aRecData  == nil ) throw( (sInt32)eMemoryError );
		aAttrData = new CDataBuff(kNSLBuffDataDefaultSize);
		if ( aAttrData  == nil ) throw( (sInt32)eMemoryError );
		aTmpData = new CDataBuff(kNSLBuffDataDefaultSize);
		if ( aTmpData  == nil ) throw( (sInt32)eMemoryError );

		// Set the record name and type
		aRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"dsAttrTypeStandard:DirectoryNodeInfo" );
		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"DirectoryNodeInfo" );

		while ( inAttrList->GetAttribute( uiCntr++, &pAttrName ) == eDSNoErr )
		{
			//package up all the dir node attributes dependant upon what was asked for
			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, kDSNAttrNodePath ) == 0) )
			{
				CLEAR_DATA_BUFFER(aTmpData);
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrNodePath ) );
				aTmpData->AppendString( kDSNAttrNodePath );

				if ( inData->fInAttrInfoOnly == false )
				{
					char	nameBuf[1024] = {0,};
					
					// Attribute value count always two
					aTmpData->AppendShort( 2 );
					
					// Append attribute value
					{
						aTmpData->AppendLong( ::strlen( GetProtocolPrefixString() ) );
						aTmpData->AppendString( (char *)GetProtocolPrefixString() );
					}
					
					char *tmpStr = nil;
					
					if (nodeRep->GetNodeName() != nil)
					{
						CFIndex		len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(nodeRep->GetNodeName()), kCFStringEncodingUTF8) + 1;
						
						if ( len >= (CFIndex)sizeof(nameBuf) )
							tmpStr = new char[1+len];
						else
							tmpStr = nameBuf;
							
						CFStringGetCString( nodeRep->GetNodeName(), tmpStr, len, kCFStringEncodingUTF8 );
						
						// skip past the "SMB/"
						char *nodePath = ::strdup( tmpStr + strlen(GetProtocolPrefixString()) +1 );
						delete tmpStr;
						tmpStr = nodePath;
					}
					else
					{
						tmpStr = new char[1+::strlen("Unknown Node Location")];
						::strcpy( tmpStr, "Unknown Node Location" );
					}
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					if ( tmpStr != nameBuf )
						delete( tmpStr );

				} // fInAttrInfoOnly is false
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                CLEAR_DATA_BUFFER(aTmpData);
			} // kDSAttributesAll or kDSNAttrNodePath
			
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrReadOnlyNode ) == 0) )
			{
				CLEAR_DATA_BUFFER(aTmpData);

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
					if ( ReadOnlyPlugin() )
					{
						aTmpData->AppendLong( ::strlen( "ReadOnly" ) );
						aTmpData->AppendString( "ReadOnly" );
					}
					else
					{
						aTmpData->AppendLong( ::strlen( "ReadWrite" ) );
						aTmpData->AppendString( "ReadWrite" );
					}
				}
				// Add the attribute length and data
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				CLEAR_DATA_BUFFER(aTmpData);
			}

			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDSNAttrRecordType ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
				
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrRecordType ) );
				aTmpData->AppendString( kDSNAttrRecordType );
				
				CFStringRef		ourProtocolPrefix = CFStringCreateWithCString( NULL, GetProtocolPrefixString(), kCFStringEncodingUTF8 );
				// if we are somewhere below the top node, we will return record types if requested
				if ( inData->fInAttrInfoOnly == false &&
					 nodeRep->GetNodeName() != NULL && ourProtocolPrefix && CFStringCompare(nodeRep->GetNodeName(), ourProtocolPrefix, 0) != kCFCompareEqualTo )
				{
					short	numAttributesSupported = 0;
					
					if ( PluginSupportsServiceType(kDSStdRecordTypeAFPServer) )
						numAttributesSupported++;
					
					if ( PluginSupportsServiceType(kDSStdRecordTypeSMBServer) )
						numAttributesSupported++;
					
					if ( PluginSupportsServiceType(kDSStdRecordTypeNFS) )
						numAttributesSupported++;
					
					if ( PluginSupportsServiceType(kDSStdRecordTypeFTPServer) )
						numAttributesSupported++;
					
					if ( PluginSupportsServiceType(kDSStdRecordTypeWebServer) )
						numAttributesSupported++;

					// Attribute value count
					aTmpData->AppendShort( numAttributesSupported );
					
					// We will hardcode these as these are standard types we respond to
					if ( PluginSupportsServiceType(kDSStdRecordTypeAFPServer) )
					{
						aTmpData->AppendLong( strlen( kDSStdRecordTypeAFPServer ) );
						aTmpData->AppendString( kDSStdRecordTypeAFPServer );
					}
					
					if ( PluginSupportsServiceType(kDSStdRecordTypeSMBServer) )
					{
						aTmpData->AppendLong( strlen( kDSStdRecordTypeSMBServer ) );
						aTmpData->AppendString( kDSStdRecordTypeSMBServer );
					}
					
					if ( PluginSupportsServiceType(kDSStdRecordTypeNFS) )
					{
						aTmpData->AppendLong( strlen( kDSStdRecordTypeNFS ) );
						aTmpData->AppendString( kDSStdRecordTypeNFS );
					}
					
					if ( PluginSupportsServiceType(kDSStdRecordTypeFTPServer) )
					{
						aTmpData->AppendLong( strlen( kDSStdRecordTypeFTPServer ) );
						aTmpData->AppendString( kDSStdRecordTypeFTPServer );
					}
					
					if ( PluginSupportsServiceType(kDSStdRecordTypeWebServer) )
					{
						aTmpData->AppendLong( strlen( kDSStdRecordTypeWebServer ) );
						aTmpData->AppendString( kDSStdRecordTypeWebServer );
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
				
				if ( ourProtocolPrefix )
					CFRelease( ourProtocolPrefix );
			}
 		} // while

		aRecData->AppendShort( uiAttrCnt );
		if (uiAttrCnt > 0)
		{
			aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
		}

		outBuff.AddData( aRecData->GetData(), aRecData->GetLength() );
		inData->fOutAttrInfoCount = uiAttrCnt;
		
		pData = outBuff.GetDataBlock( 1, &uiOffset );
		if ( pData != nil )
		{
			pAttrContext = MakeContextData();
			if ( pAttrContext  == nil ) throw( (sInt32)eMemoryAllocError );

			pAttrContext->offset = uiOffset + 61;
	
			::CFDictionaryAddValue( mOpenRefTable, (const void*)inData->fOutAttrListRef, (const void*)pAttrContext );
		}
	}
	
	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( nodeRep )
		nodeRep->Release();

	UnlockPlugin();

	if ( inAttrList != nil )
	{
		delete( inAttrList );
		inAttrList = nil;
	}
	if ( aRecData != nil )
	{
		delete( aRecData );
		aRecData = nil;
	}
	if ( aAttrData != nil )
	{
		delete( aAttrData );
		aAttrData = nil;
	}
	if ( aTmpData != nil )
	{
		delete( aTmpData );
		aTmpData = nil;
	}

	return( siResult );

}

sInt32 CNSLPlugin::OpenDirNode ( sOpenDirNode *inData )
{
    sInt32				siResult			= eDSNoErr;
    char			   *nodeName			= nil;
	char			   *pathStr				= nil;
	char			   *protocolStr			= nil;
    tDataListPtr		pNodeList			= nil;
    const char*			subStr	 			= nil;
    Boolean				weHandleThisNode	= false;
    
	DBGLOG( "CNSLPlugin::OpenDirNode %lx\n", inData->fOutNodeRef );
	
	if ( inData != nil )
    {
		try {
			pNodeList	=	inData->fInDirNodeName;
			pathStr = dsGetPathFromListPriv( pNodeList, (char *)"\t" );
			protocolStr = pathStr + 1;	// advance past the '/'
			
			DBGLOG( "CNSLPlugin::OpenDirNode, ProtocolPrefixString is %s, pathStr is %s\n", GetProtocolPrefixString(), pathStr );
			
			if ( strstr( pathStr, "NSLActivate" ) )
			{
				// we have a directed open for our plugins, fire off our node searches if needed
				if ( !mActivatedByNSL )
				{
					mActivatedByNSL = true;
					
					if ( IsActive() )		// only start lookups if we are currently active
					{
						InstallNodeLookupTimer();
					}
				}
				
				free( pathStr );
				
				return eDSNoErr;
			}
			else if ( strcmp( protocolStr, GetProtocolPrefixString() ) == 0 )
			{
				// they are opening this plugin at the top level so they'll probably be wanting to configure it
				nodeName = new char[1+strlen(protocolStr)];
				if ( !nodeName ) throw ( eDSNullNodeName );
					
				::strcpy(nodeName,protocolStr);
				
				if ( nodeName )
				{
					DBGLOG( "CNSLPlugin::OpenDirNode on %s\n", nodeName );
					CNSLDirNodeRep*		newNodeRep = new CNSLDirNodeRep( this, (const void*)inData->fInDirRef );
					
					if (!newNodeRep) throw ( eDSNullNodeName );
					
					newNodeRep->Initialize( nodeName, inData->fInUID, true, protocolStr );
					
					newNodeRep->Retain();
					// add the item to the reference table
					LockOpenRefTable();
	
					::CFDictionaryAddValue( mOpenRefTable, (const void*)inData->fOutNodeRef, (const void*)newNodeRep );
		
					UnlockOpenRefTable();
		
					delete( nodeName );
					nodeName = nil;
				}
				
				free( pathStr );
				
				return eDSNoErr;
			}
			else if ( IsActive() && (strlen(pathStr) > strlen(GetProtocolPrefixString()+1)) && (::strncmp(protocolStr, GetProtocolPrefixString(), strlen(GetProtocolPrefixString())) == 0) && protocolStr[strlen(GetProtocolPrefixString())] == '\t' )
			{
				// only work on nodes with our prefix
				subStr = pathStr + strlen(GetProtocolPrefixString()) + 2;
				
				CFStringRef		nodeAsRef = ::CFStringCreateWithCString( NULL, protocolStr, kCFStringEncodingUTF8 );
//				CFStringRef		nodeAsRef = ::CFStringCreateWithCString( NULL, subStr, kCFStringEncodingUTF8 );
				
				if ( nodeAsRef )
				{
					LockPublishedNodes();
					weHandleThisNode = ::CFDictionaryContainsKey( mPublishedNodes, nodeAsRef );
					UnlockPublishedNodes();
					
					if ( weHandleThisNode )
						DBGLOG( "CNSLPlugin::OpenDirNode, we will handle the request since we have this node (%s) already published\n", subStr );
					else
					{
						if ( IsNSLDebuggingEnabled() )
						{
							DBGLOG( "CNSLPlugin::OpenDirNode, we don't handle this node, ref/publishedNodes\n" );
							if ( nodeAsRef ) CFShow(nodeAsRef);
						}
					}
	
					::CFRelease( nodeAsRef );
					
					if ( !weHandleThisNode )
					{
						weHandleThisNode = OKToOpenUnPublishedNode( subStr );		// allow plugin option of searching unpublished nodes
					
						if ( weHandleThisNode )
						{
							subStr = pathStr + strlen(GetProtocolPrefixString()) + 2;	// prefix plus pre and post '/'s
	
							DBGLOG( "CNSLPlugin::OpenDirNode Adding new node %s\n", subStr );
							CFStringRef		nodeRef = CFStringCreateWithCString( NULL, protocolStr, kCFStringEncodingUTF8 );
//							CFStringRef		nodeRef = CFStringCreateWithCString( NULL, subStr, kCFStringEncodingUTF8 );
							
							if ( nodeRef )
							{
								NodeData*  node = AllocateNodeData();
								
								node->fNodeName = nodeRef;
								CFRetain( node->fNodeName );
								
								node->fDSName = dsBuildListFromStringsPriv(GetProtocolPrefixString(), subStr, nil);
						
								node->fTimeStamp = GetCurrentTime();
								
								node->fServicesRefTable = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
								node->fIsADefaultNode = IsLocalNode( subStr );
								node->fSignature = mSignature;
								
								if ( node->fDSName )
								{
									eDirNodeType		nodeTypeToRegister = kUnknownNodeType;
									
									if ( IsLocalNode( subStr ) )
										nodeTypeToRegister = kDefaultNetworkNodeType;
									
									nodeTypeToRegister = (eDirNodeType)( nodeTypeToRegister | kDirNodeType );
									
									if ( nodeTypeToRegister != kUnknownNodeType )
										DSRegisterNode( mSignature, node->fDSName, nodeTypeToRegister );
								}
								
								::CFDictionaryAddValue( mPublishedNodes, nodeRef, node );
							
								::CFRelease( nodeRef );
							}
						}
					}
					
					if ( !weHandleThisNode )
						DBGLOG( "CNSLPlugin::OpenDirNode, we won't handle the request on this node (%s)\n", pathStr );
				}
			}
			
			if ( weHandleThisNode )
			{
				if ( !subStr )
					subStr = pathStr + strlen(GetProtocolPrefixString()) + 2;	// prefix plus pre and post '/'s
	
				if ( subStr )
				{
					DBGLOG( "CNSLPlugin::OpenDirNode subStr is %s\n", subStr );
	
					nodeName = new char[1+strlen(subStr)];
					if( !nodeName ) throw ( eDSNullNodeName );
					
					::strcpy(nodeName,subStr);
				}
				
				if ( nodeName )
				{
					DBGLOG( "CNSLPlugin::OpenDirNode on %s\n", nodeName );
					CNSLDirNodeRep*		newNodeRep = new CNSLDirNodeRep( this, (const void*)inData->fOutNodeRef );
					
					if( !newNodeRep ) throw (eMemoryAllocError);
					
					newNodeRep->Initialize( nodeName, inData->fInUID, false, protocolStr );
	
					newNodeRep->Retain();
					
					// add the item to the reference table
					LockOpenRefTable();
	
					::CFDictionaryAddValue( mOpenRefTable, (const void*)inData->fOutNodeRef, (const void*)newNodeRep );
	
					UnlockOpenRefTable();
	
					delete( nodeName );
					nodeName = nil;
				}
				else
				{
					inData->fOutNodeRef = 0;
					DBGLOG( "CNSLPlugin::OpenDirNode nodeName is NULL!\n" );
				}
			}
			else
			{
				siResult = eNotHandledByThisNode;
				DBGLOG( "CNSLPlugin::OpenDirNode skipping cuz path is %s\n", pathStr );
			}
		}
		
		catch( int err )
		{
			siResult = err;
			DBGLOG( "CNSLPlugin::CloseDirNode, Caught error:%li\n", siResult );
		}
	}
    else
    	DBGLOG( "CNSLPlugin::OpenDirNode inData is NULL!\n" );
		
	if (pathStr != NULL)
	{
		free(pathStr);
		pathStr = NULL;
	}

	if ( nodeName )
	{
		delete( nodeName );
		nodeName = nil;
	}
	
    return siResult;
}


sInt32 CNSLPlugin::CloseDirNode ( sCloseDirNode *inData )
{
    sInt32				siResult			= eDSNoErr;
    
	DBGLOG( "CNSLPlugin::CloseDirNode %lx\n", inData->fInNodeRef );
    try
    {
        if ( inData != nil && inData->fInNodeRef && mOpenRefTable )
        {
            CNSLDirNodeRep*		nodeRep = NULL;
            
			LockOpenRefTable();
			
			nodeRep = (CNSLDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
			
			DBGLOG( "CNSLPlugin::CloseDirNode, CFDictionaryGetValue returned nodeRep: 0x%x\n", (void*)nodeRep );

            if ( nodeRep )
            {
                ::CFDictionaryRemoveValue( mOpenRefTable, (void*)inData->fInNodeRef );

                DBGLOG( "CNSLPlugin::CloseDirNode, delete nodeRep: 0x%x\n", (void*)nodeRep );
                nodeRep->Release();
            }
            else
            {
                DBGLOG( "CNSLPlugin::CloseDirNode, nodeRef not found in our list\n" );
            }

			UnlockOpenRefTable();
        }
    }
    
    catch( int err )
    {
        siResult = err;
        DBGLOG( "CNSLPlugin::CloseDirNode, Caught error:%li\n", siResult );
    }

    return siResult;
}

#pragma mark -
sInt32 CNSLPlugin::GetRecordList ( sGetRecordList *inData )
{
    sInt32					siResult			= eDSNoErr;
    char				   *pRecType			= nil;
    char				   *pNSLRecType			= nil;
    CAttributeList		   *cpRecNameList		= nil;
    CAttributeList		   *cpRecTypeList		= nil;
    CAttributeList		   *cpAttrTypeList 		= nil;
    CBuff				   *outBuff				= nil;
    uInt32					countDownRecTypes	= 0;
    const void*				dictionaryResult	= NULL;
    CNSLDirNodeRep*			nodeDirRep			= NULL;
    unsigned long			incomingRecEntryCount = inData->fOutRecEntryCount;
	
	LockOpenRefTable();
	DBGLOG( "CNSLPlugin::GetRecordList called\n" );
	dictionaryResult = ::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );

    nodeDirRep = (CNSLDirNodeRep*)dictionaryResult;
    
    if( !nodeDirRep )
	{
        DBGLOG( "CNSLPlugin::GetRecordList called but we couldn't find the nodeDirRep!\n" );

		UnlockOpenRefTable();
		return eDSInvalidNodeRef;
    }

	if ( nodeDirRep->IsTopLevelNode() )
	{
        DBGLOG( "CNSLPlugin::GetRecordList called on a top level node, return no results\n" );

		inData->fIOContinueData = NULL;
		inData->fOutRecEntryCount = 0;			// no more entries to parse

		UnlockOpenRefTable();
		return eDSNoErr;
	}
	
	nodeDirRep->Retain();
	UnlockOpenRefTable();

	inData->fOutRecEntryCount = 0;							// make sure to reset this!
			
    try
    {
        if ( nodeDirRep && !nodeDirRep->IsLookupStarted() )
        {
 DBGLOG( "CNSLPlugin::GetRecordList called, lookup hasn't been started yet.\n" );
           // ok, we need to initialize this nodeDirRep and start the searches...
            // Verify all the parameters
            if( !inData )  throw( eMemoryError );
            if( !inData->fInDataBuff)  throw( eDSEmptyBuffer );
            if( (inData->fInDataBuff->fBufferSize == 0) ) throw( eDSEmptyBuffer );
            if( !inData->fInRecNameList )  throw( eDSEmptyRecordNameList );
            if( !inData->fInRecTypeList )  throw( eDSEmptyRecordTypeList );
            if( !inData->fInAttribTypeList )  throw( eDSEmptyAttributeTypeList );
    
            //check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pContext
			if (incomingRecEntryCount >= 0)
			{
				nodeDirRep->LimitRecSearch( incomingRecEntryCount );
			}
                   
           // Node context data
    
            // Get the record type list
            cpRecTypeList = new CAttributeList( inData->fInRecTypeList );
    
            if( !cpRecTypeList )  throw( eDSEmptyRecordTypeList );
            //save the number of rec types here to use in separating the buffer data
            countDownRecTypes = cpRecTypeList->GetCount();
    
            if( (countDownRecTypes == 0) ) throw( eDSEmptyRecordTypeList );
    
            sInt32		error = eDSNoErr;
            nodeDirRep->LookupHasStarted();

			// performance hack...
			Boolean		alreadyLookedupSMBType = false;
			
            for ( uInt16 i=1; i<=countDownRecTypes; i++ )
            {
                if ( (error = cpRecTypeList->GetAttribute( i, &pRecType )) == eDSNoErr )
                {
                    DBGLOG( "CNSLPlugin::GetRecordList, GetAttribute returned pRecType:%s\n", pRecType );
                    
					if ( pRecType )
					{
						if ( strcmp( pRecType, kDSStdRecordTypeSMBServer ) == 0 )
						{
							if ( alreadyLookedupSMBType )
							{
								free( pNSLRecType );
								continue;				// already looked up one of these
							}
							
							alreadyLookedupSMBType = true;
						}

						Boolean		needToFreeRecType = false;
						pNSLRecType = CreateNSLTypeFromRecType( pRecType, &needToFreeRecType );
						
						if ( pNSLRecType )
						{
							DBGLOG( "CNSLPlugin::GetRecordList, CreateNSLTypeFromRecType returned pNSLRecType:%s\n", pNSLRecType );
							
							StartServicesLookup( pNSLRecType, nodeDirRep );
						
							if ( needToFreeRecType )
								free( pNSLRecType );
							inData->fIOContinueData = (void*)inData->fInNodeRef;	// just to show we have continuing data
						}
					}
                }
                else
                {
                    DBGLOG( "CNSLPlugin::GetRecordList, GetAttribute returned error:%li\n", error );
                }
            }
        }
        else if ( nodeDirRep && nodeDirRep->HaveResults() )
        {
			// cool, we have data waiting for us...
            // we already started a search, check for results.
            siResult = RetrieveResults( inData, nodeDirRep );
            inData->fIOContinueData = (void*)inData->fInNodeRef;	// just to show we have continuing data
            DBGLOG( "***Sending Back Results, fBufferSize = %ld, fBufferLength = %ld, inData->fOutRecEntryCount = %d\n", inData->fInDataBuff->fBufferSize, inData->fInDataBuff->fBufferLength, inData->fOutRecEntryCount );
        }
        else if ( nodeDirRep && nodeDirRep->LookupComplete() )
        {
			DBGLOG( "CNSLPlugin::GetRecordList called and nodeDirRep->LookupComplete so setting inData->fIOContinueData to NULL\n" );
            inData->fIOContinueData = NULL;
			nodeDirRep->ResetLookupHasStarted();	// so users can start another search
        }
		else
		{
			if ( dsTimestamp() < mLastTimeEmptyResultsReturned + kMinTimeBetweenEmptyResults )
			{
				DBGLOG( "CNSLPlugin::GetRecordList called too often with Empty Results, sleep for 1/10th a sec\n" );
				SmartSleep( 100000 );		// sleep for a 10th of a second so the client can't spinlock
			}
			
			mLastTimeEmptyResultsReturned = dsTimestamp();
		}
    } // try
    
    catch ( int err )
    {
        siResult = err;
        DBGLOG( "CNSLPlugin::GetRecordList, Caught error:%li\n", siResult );
    }

	LockOpenRefTable();
	if ( nodeDirRep )
		nodeDirRep->Release();
	UnlockOpenRefTable();
		
    if ( cpRecNameList != nil )
    {
        delete( cpRecNameList );
        cpRecNameList = nil;
    }

    if ( cpRecTypeList != nil )
    {
        delete( cpRecTypeList );
        cpRecTypeList = nil;
    }

    if ( cpAttrTypeList != nil )
    {
        delete( cpAttrTypeList );
        cpAttrTypeList = nil;
    }

    if ( outBuff != nil )
    {
        delete( outBuff );
        outBuff = nil;
    }

    return( siResult );

}	// GetRecordList

//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

sInt32 CNSLPlugin::GetAttributeEntry ( sGetAttributeEntry *inData )
{
	sInt32					siResult			= eDSNoErr;
	uInt16					usAttrTypeLen		= 0;
	uInt16					usAttrCnt			= 0;
	uInt32					usAttrLen			= 0;
	uInt16					usValueCnt			= 0;
	uInt32					usValueLen			= 0;
	uInt32					i					= 0;
	uInt32					uiIndex				= 0;
	uInt32					uiAttrEntrySize		= 0;
	uInt32					uiOffset			= 0;
	uInt32					uiTotalValueSize	= 0;
	uInt32					offset				= 4;
	uInt32					buffSize			= 0;
	uInt32					buffLen				= 0;
	char				   *p			   		= nil;
	char				   *pAttrType	   		= nil;
	tDataBufferPtr			pDataBuff			= nil;
	tAttributeEntryPtr		pAttribInfo			= nil;
	sNSLContextData		   *pAttrContext		= nil;
	sNSLContextData		   *pValueContext		= nil;

	LockPlugin();
	try
	{
		if ( inData  == nil ) throw( (sInt32)eMemoryError );

		pAttrContext = (sNSLContextData *)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInAttrListRef );
		if ( pAttrContext  == nil ) throw( (sInt32)eDSBadContextData );

		uiIndex = inData->fInAttrInfoIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );
				
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (sInt32)eDSNullDataBuff );
		
		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pAttrContext->offset;
		offset	= pAttrContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );
				
		// Get the attribute count
		::memcpy( &usAttrCnt, p, 2 );
		if (uiIndex > usAttrCnt) throw( (sInt32)eDSInvalidIndex );

		// Move 2 bytes
		p		+= 2;
		offset	+= 2;

		// Skip to the attribute that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 > (sInt32)(buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the attribute
			::memcpy( &usAttrLen, p, 4 );

			// Move the offset past the length word and the length of the data
			p		+= 4 + usAttrLen;
			offset	+= 4 + usAttrLen;
		}

		// Get the attribute offset
		uiOffset = offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 > (sInt32)(buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute block
		::memcpy( &usAttrLen, p, 4 );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		//set the bufLen to stricter range
		buffLen = offset + usAttrLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute type
		::memcpy( &usAttrTypeLen, p, 2 );
		
		pAttrType = p + 2;
		p		+= 2 + usAttrTypeLen;
		offset	+= 2 + usAttrTypeLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get number of values for this attribute
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;
		
		for ( i = 0; i < usValueCnt; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
			
			uiTotalValueSize += usValueLen;
		}

		uiAttrEntrySize = sizeof( tAttributeEntry ) + usAttrTypeLen + kBuffPad;
		pAttribInfo = (tAttributeEntry *)::calloc( 1, uiAttrEntrySize );

		pAttribInfo->fAttributeValueCount				= usValueCnt;
		pAttribInfo->fAttributeDataSize					= uiTotalValueSize;
		pAttribInfo->fAttributeValueMaxSize				= 512;				// KW this is not used anywhere
		pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kBuffPad;
		pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
		::memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

		pValueContext = MakeContextData();
		if ( pValueContext  == nil ) throw( (sInt32)eMemoryAllocError );

		pValueContext->offset = uiOffset;

		::CFDictionaryAddValue( mOpenRefTable, (const void*)inData->fOutAttrValueListRef, (const void*)pValueContext );

		inData->fOutAttrInfoPtr = pAttribInfo;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}
	
	UnlockPlugin();

	return( siResult );

} // GetAttributeEntry

//------------------------------------------------------------------------------------
//	* GetAttributeValue
//------------------------------------------------------------------------------------

sInt32 CNSLPlugin::GetAttributeValue ( sGetAttributeValue *inData )
{
	sInt32						siResult		= eDSNoErr;
	uInt16						usValueCnt		= 0;
	uInt32						usValueLen		= 0;
	uInt16						usAttrNameLen	= 0;
	uInt32						i				= 0;
	uInt32						uiIndex			= 0;
	uInt32						offset			= 0;
	char					   *p				= nil;
	tDataBuffer				   *pDataBuff		= nil;
	tAttributeValueEntry	   *pAttrValue		= nil;
	sNSLContextData			   *pValueContext	= nil;
	uInt32						buffSize		= 0;
	uInt32						buffLen			= 0;
	uInt32						attrLen			= 0;

	LockPlugin();
	try
	{
		pValueContext = (sNSLContextData *)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInAttrValueListRef );
		if ( pValueContext  == nil ) throw( (sInt32)eDSBadContextData );

		uiIndex = inData->fInAttrValueIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );
		
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (sInt32)eDSNullDataBuff );

		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pValueContext->offset;
		offset	= pValueContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 > (sInt32)(buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );
				
		// Get the buffer length
		::memcpy( &attrLen, p, 4 );

		//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
		//AND add the length of the buffer length var as stored ie. 4 bytes
		buffLen		= attrLen + pValueContext->offset + 4;
		if (buffLen > buffSize) throw( (sInt32)eDSInvalidBuffFormat );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the attribute name length
		::memcpy( &usAttrNameLen, p, 2 );
		
		p		+= 2 + usAttrNameLen;
		offset	+= 2 + usAttrNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the value count
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		if (uiIndex > usValueCnt) throw( (sInt32)eDSInvalidIndex );

		// Skip to the value that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
		}

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		::memcpy( &usValueLen, p, 4 );
		
		p		+= 4;
		offset	+= 4;

		//if (usValueLen == 0) throw( (sInt32)eDSInvalidBuffFormat ); //if zero is it okay?

		pAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kBuffPad );

		pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kBuffPad;
		pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if ( (sInt32)usValueLen > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		::memcpy( pAttrValue->fAttributeValueData.fBufferData, p, usValueLen );

			// Set the attribute value ID
		pAttrValue->fAttributeValueID = inData->fInAttrValueIndex;

		inData->fOutAttrValue = pAttrValue;
			
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	UnlockPlugin();
	
	return( siResult );

} // GetAttributeValue

sInt32 CNSLPlugin::OpenRecord ( sOpenRecord *inData )
{
    sInt32					siResult			= eDSRecordNotFound;		// by default we don't Open records
    const void*				dictionaryResult	= NULL;
        
    DBGLOG( "CNSLPlugin::OpenRecord called on refNum:0x%x\n", inData->fOutRecRef );

	LockOpenRefTable();

	dictionaryResult = ::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
    if ( !dictionaryResult )
	{
		UnlockOpenRefTable();
        DBGLOG( "CNSLPlugin::OpenRecord called but we couldn't find the nodeDirRep!\n" );
        return eDSInvalidNodeRef;
    }
    
	((CNSLDirNodeRep*)dictionaryResult)->Retain();
	UnlockOpenRefTable();

    CFStringRef				nodeNameRef = ((CNSLDirNodeRep*)dictionaryResult)->GetNodePath();
	Boolean					needToFreeRecType = false;
    char*		    		pNSLRecType = CreateNSLTypeFromRecType( (char*)inData->fInRecType->fBufferData, &needToFreeRecType );
	
	if ( pNSLRecType )
	{
		CFMutableStringRef		serviceKeyRef = ::CFStringCreateMutable( NULL, 0 );			// we'll use this as a key
		CFStringRef				recordTypeRef = ::CFStringCreateWithCString( kCFAllocatorDefault, pNSLRecType, kCFStringEncodingUTF8 );
		CFStringRef		        recordNameRef = ::CFStringCreateWithCString( kCFAllocatorDefault, (char*)inData->fInRecName->fBufferData, kCFStringEncodingUTF8 );
		
		if ( serviceKeyRef && recordTypeRef && recordNameRef )
		{
			::CFStringAppend( serviceKeyRef, recordNameRef );
			::CFStringAppend( serviceKeyRef, recordTypeRef );
		}
		
		if ( recordNameRef )
			CFRelease( recordNameRef );
		
		if ( recordTypeRef )
			CFRelease( recordTypeRef );

		if ( needToFreeRecType )
			free( pNSLRecType );
    
		LockPublishedNodes();
		NodeData* node = (NodeData*)::CFDictionaryGetValue( mPublishedNodes, nodeNameRef );
		
		if ( node )
		{
			if ( IsNSLDebuggingEnabled() )
			{
				char		serviceKey[1024] = {0,};
				char		nodeName[1024] = {0,};
				
				CFStringGetCString( serviceKeyRef, serviceKey, sizeof(serviceKey), kCFStringEncodingUTF8 );
				CFStringGetCString( nodeNameRef, nodeName, sizeof(nodeName), kCFStringEncodingUTF8 );
				
				DBGLOG( "CNSLPlugin::OpenRecord, found the node in question [%s]\n", nodeName );
				DBGLOG( "CNSLPlugin::OpenRecord, looking up record [%s]\n", serviceKey );
			}
			
			if ( node->fServicesRefTable )
			{
				CFDictionaryRef	recordRef = (CFDictionaryRef)::CFDictionaryGetValue( node->fServicesRefTable, serviceKeyRef );
		
				if ( recordRef )
					CFRetain( recordRef );		// hold on to this while we manipulate it and release at the end.
					
				UnlockPublishedNodes();
	
				if ( recordRef && CFGetTypeID(recordRef) == CFDictionaryGetTypeID() )
				{
					CFStringRef recordName = (CFStringRef)::CFDictionaryGetValue( recordRef, kDSNAttrRecordNameSAFE_CFSTR );
					CFStringRef nodeName = (CFStringRef)::CFDictionaryGetValue( recordRef, kDS1AttrLocationSAFE_CFSTR );
					
					if ( !recordName && !nodeName )
					{
						if ( IsNSLDebuggingEnabled() )
						{
							DBGLOG( "CNSLPlugin::OpenRecord, the node->fServicesRefTable doesn't have both recordName and nodeName keys!\n" );
							if (recordName) CFShow( recordName ); else DBGLOG( "CNSLPlugin::OpenRecord, recordName is null\n" );
							if (nodeName) CFShow( nodeName ); else DBGLOG( "CNSLPlugin::OpenRecord, nodeName is null\n" );
						}
					}
					else if ( CFGetTypeID(recordName) == CFStringGetTypeID() && CFGetTypeID(nodeName) == CFStringGetTypeID() )
					{
						CFRetain( recordRef );		// manually add retain when putting it into mOpenRefTable.
	
						// found it.
						LockOpenRefTable();
		
						::CFDictionaryAddValue( mOpenRefTable, (void*)inData->fOutRecRef, (void*)recordRef );
						
						UnlockOpenRefTable();
	
						siResult = eDSNoErr;
						
						if ( IsNSLDebuggingEnabled() )
						{
							DBGLOG( "CNSLPlugin::OpenRecord, found the record in question (ref:0x%x)\n", inData->fOutRecRef );
						}
					}
					else
						DBGLOG( "CNSLPlugin::OpenRecord, we just grabbed a name or node name that wasn't a CFString!\n" );
				}
				else if ( recordRef )
					DBGLOG( "CNSLPlugin::OpenRecord, we just grabbed something out of our node->fServicesRefTable that wasn't a CFDictionary!\n" );
				else
					DBGLOG( "CNSLPlugin::OpenRecord, couldn't find previously created record\n" );
	
				if ( recordRef )
					CFRelease( recordRef );		// ok, we're done with this now.
			}
		}
		else
		{				
			UnlockPublishedNodes();
			// do we want to allow two openings of a record?  Or is this not possible as we should be getting a unique
			// ref assigned to this open?...
			DBGLOG( "CNSLPlugin::OpenRecord this record already opened...\n" );
			siResult = eDSNoErr;
		}
    
		::CFRelease( serviceKeyRef );
    }

	((CNSLDirNodeRep*)dictionaryResult)->Release();
	
    return( siResult );
}	// OpenRecord

sInt32 CNSLPlugin::CloseRecord ( sCloseRecord *inData )
{
    sInt32					siResult			= eDSNoErr;		// by default we don't Close records

    DBGLOG( "CNSLPlugin::CloseRecord called on ref:%ld\n", inData->fInRecRef );
    
	LockOpenRefTable();

	CFDictionaryRef	recordRef = (CFDictionaryRef)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInRecRef );

    if ( recordRef )
    {
        ::CFDictionaryRemoveValue( mOpenRefTable, (const void*)inData->fInRecRef );
		CFRelease( recordRef );
    }
    else
    {
        DBGLOG( "CNSLPlugin::CloseRecord called but the record wasn't found!\n" );
        siResult = eDSRecordNotFound;
    }
    
	UnlockOpenRefTable();

    return( siResult );
}	// CloseRecord

sInt32 CNSLPlugin::CreateRecord ( sCreateRecord *inData )
{
    sInt32					siResult			= eDSNoErr;		// by default we don't create records
    const void*				dictionaryResult	= NULL;
    CNSLDirNodeRep*			nodeDirRep			= NULL;
	tDataNodePtr			pRecName			= NULL;
	tDataNodePtr			pRecType			= NULL;
    char				   *pNSLRecType			= NULL;
	CFMutableStringRef		serviceKeyRef		= NULL;
	CFMutableDictionaryRef	newService			= NULL;
	
    DBGLOG( "CNSLPlugin::CreateRecord called\n" );
    DBGLOG( "CNSLPlugin::CreateRecord, fOutRecRef is 0x%x\n", inData->fOutRecRef );
    DBGLOG( "CNSLPlugin::CreateRecord, fInOpen is %d\n", inData->fInOpen );
    
    if ( ReadOnlyPlugin() )
    {
        // ignore if this plugin doesn't support creation anyway
        return eDSReadOnly;
    }
    
    if ( !IsClientAuthorizedToCreateRecords( inData ) )
    {
        return eDSPermissionError;
    }
    
	try
	{
		LockOpenRefTable();
		dictionaryResult = ::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
        if ( !dictionaryResult )
		{
			UnlockOpenRefTable();
            DBGLOG( "CNSLPlugin::CreateRecord called but we couldn't find the nodeDirRep!\n" );
            return eDSInvalidNodeRef;
        }

        nodeDirRep = (CNSLDirNodeRep*)dictionaryResult;
        
		nodeDirRep->Retain();

		UnlockOpenRefTable();

        serviceKeyRef = ::CFStringCreateMutable( NULL, 0 );			// we'll use this as a key
        
        newService = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

		pRecType = inData->fInRecType;
		if( !pRecType ) throw( eDSNullRecType );

		pRecName = inData->fInRecName;
		if( !pRecName ) throw( eDSNullRecName );

		Boolean		needToFreeRecType = false;
		
        pNSLRecType = CreateNSLTypeFromRecType( (char*)pRecType->fBufferData, &needToFreeRecType );
		if( !pNSLRecType ) throw( eDSInvalidRecordType );

        if ( pNSLRecType )
        {
            if ( IsNSLDebuggingEnabled() )
            {
                DBGLOG( "CNSLPlugin::CreateRecord, CreateNSLTypeFromRecType returned pNSLRecType:%s\n", pNSLRecType );
                DBGLOG( "dictionary contents before:\n");
                CFShow( newService );
            }
            
            CFStringRef		valueRef;
            
            // add node name
            valueRef = nodeDirRep->GetNodeName();
            
            if ( !CFDictionaryContainsKey( newService, CFSTR(kDS1AttrLocation) ) )
                ::CFDictionaryAddValue( newService, CFSTR(kDS1AttrLocation), valueRef );
                
            // add record name
            valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, (char*)pRecName->fBufferData, kCFStringEncodingUTF8 );
            if ( !CFDictionaryContainsKey( newService, CFSTR(kDSNAttrRecordName) ) )
                ::CFDictionaryAddValue( newService, CFSTR(kDSNAttrRecordName), valueRef );

            ::CFStringAppend( serviceKeyRef, valueRef );	// key is made up of name and type
            
            ::CFRelease( valueRef );
            
            // add record type
            valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, pNSLRecType, kCFStringEncodingUTF8 );

            ::CFDictionarySetValue( newService, CFSTR(kDS1AttrServiceType), valueRef );		// set this regardless whether it is already there

            ::CFStringAppend( serviceKeyRef, valueRef );	// key is made up of name and type

            ::CFRelease( valueRef );
            
            if ( IsNSLDebuggingEnabled() )
            {
                DBGLOG( "dictionary contents after:\n");
                CFShow( newService );

                DBGLOG( "CNSLPlugin::CreateRecord, finished intial creation of opened service dictionary\n" );
                if ( getenv( "NSLDEBUG" ) )
                    ::CFShow( newService );
            }
            
            if ( needToFreeRecType )
				free( pNSLRecType );
            
            // now we need to add this to our published node's dictionary of services
            LockPublishedNodes();
//            if ( ::CFDictionaryContainsKey( mPublishedNodes, nodeDirRep->GetNodePath() ) )
            {
                NodeData* node = (NodeData*)::CFDictionaryGetValue( mPublishedNodes, nodeDirRep->GetNodePath() );
                
                if ( node )
                {
                    if ( node->fServicesRefTable )
					{
						::CFDictionarySetValue( node->fServicesRefTable, serviceKeyRef, newService );

						if ( IsNSLDebuggingEnabled() )
						{
							char		serviceKey[1024] = {0,};
							char		nodeName[1024] = {0,};
							
							CFStringGetCString( serviceKeyRef, serviceKey, sizeof(serviceKey), kCFStringEncodingUTF8 );
							CFStringGetCString( nodeDirRep->GetNodeName(), nodeName, sizeof(nodeName), kCFStringEncodingUTF8 );
							
							DBGLOG( "CNSLPlugin::CreateRecord, adding record [%s] to node [%s]\n", serviceKey, nodeName );
						}
                    }
					else
						DBGLOG( "CNSLPlugin::CreateRecord, node->fServicesRefTable was NULL!\n" );
                }
                else
                    DBGLOG( "CNSLPlugin::CreateRecord, couldn't find node in our published nodes!\n" );
            }

            UnlockPublishedNodes();
        }
	}

	catch ( int err )
	{
		siResult = err;
	}
		
	LockOpenRefTable();
	nodeDirRep->Release();
	UnlockOpenRefTable();

	if ( serviceKeyRef )
		CFRelease( serviceKeyRef );
		
	if ( newService )
		CFRelease( newService );

    DBGLOG( "CNSLPlugin::CreateRecord, fOutRecRef is 0x%x\n", inData->fOutRecRef );

    if ( !siResult && inData->fInOpen )
    {
        sOpenRecord		inCopyData = { inData->fType, inData->fResult, inData->fInNodeRef, inData->fInRecType, inData->fInRecName, inData->fOutRecRef };
        
        siResult = OpenRecord( &inCopyData );
        DBGLOG( "CNSLPlugin::CreateRecord, fOutRecRef after OpenRecord is 0x%x\n", inData->fOutRecRef );
    }
    else
        DBGLOG( "CNSLPlugin::CreateRecord, siResult:%ld, inData->fInOpen=%d, fOutRecRef is 0x%x\n", siResult, inData->fInOpen, inData->fOutRecRef );

    if ( kCreateRecordAndOpen == inData->fType )
        DBGLOG( "CNSLPlugin::CreateRecord, this was supposed to be a kCreateRecordAndOpen\n");
        
    return( siResult );
}	// CreateRecord


sInt32 CNSLPlugin::DeleteRecord ( sDeleteRecord *inData )
{
    sInt32						siResult			= eDSNoErr;
	CFMutableDictionaryRef		serviceToDeregister	= NULL;
	    
    DBGLOG( "CNSLPlugin::DeleteRecord called on refNum:%ld\n", inData->fInRecRef );
    if ( inData->fInRecRef )
    {
		LockOpenRefTable();
        serviceToDeregister = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInRecRef );

        if ( serviceToDeregister )
            CFRetain( serviceToDeregister );
			
		UnlockOpenRefTable();

        if ( serviceToDeregister )
		{
			siResult = DeregisterService( inData->fInRecRef, serviceToDeregister );

			if ( siResult && siResult != eDSNullAttribute )
			{
				char				name[1024] = {0,};
				char				type[256] = {0,};
				
				CFStringRef	nameOfService = (CFStringRef)::CFDictionaryGetValue( serviceToDeregister, kDSNAttrRecordNameSAFE_CFSTR );
				if ( nameOfService )
					CFStringGetCString( nameOfService, name, sizeof(name), kCFStringEncodingUTF8 );
					
				CFStringRef	typeOfService = (CFStringRef)::CFDictionaryGetValue( serviceToDeregister, kDS1AttrServiceTypeSAFE_CFSTR );
				if ( !typeOfService )
					typeOfService = (CFStringRef)::CFDictionaryGetValue( serviceToDeregister, kDSNAttrRecordTypeSAFE_CFSTR );
			
				if ( typeOfService )
					CFStringGetCString( typeOfService, type, sizeof(type), kCFStringEncodingUTF8 );
			
				DBGLOG( "DS (%s) couldn't deregister %s (%s) due to an error: %d!\n", GetProtocolPrefixString(), name, type, siResult );
			}
			
			CFRelease( serviceToDeregister );
        }
		else
            syslog( LOG_ERR, "CNSLPlugin::DeleteRecord, couldn't find record to delete!\n" );
	}
    else
    {
        siResult = eDSInvalidRecordRef;
        syslog( LOG_ERR, "CNSLPlugin::DeleteRecord called with invalid fInRecRef (0x%x)!\n", inData->fInRecRef );
    }
    
    return( siResult );
}	// DeleteRecord

sInt32 CNSLPlugin::FlushRecord ( sFlushRecord *inData )
{
    sInt32						siResult = eDSNoErr;
	CFMutableDictionaryRef		serviceToRegister = NULL;
	
    DBGLOG( "CNSLPlugin::FlushRecord called on refNum:%ld\n", inData->fInRecRef );
	try
	{
        if ( inData->fInRecRef )
        {
			LockOpenRefTable();
            serviceToRegister = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInRecRef );
			
			if ( serviceToRegister )
				CFRetain( serviceToRegister );
				
			UnlockOpenRefTable();
                
            if ( serviceToRegister )
			{
				DBGLOG( "CNSLPlugin::FlushRecord calling RegisterService with the following service:\n" );
				if ( getenv( "NSLDEBUG" ) )
					::CFShow( serviceToRegister );
	
                siResult = RegisterService( inData->fInRecRef, serviceToRegister );

				if ( siResult && siResult != eDSNullAttribute )
				{
					char				name[1024] = {0,};
					char				type[256] = {0,};
					
					CFStringRef	nameOfService = (CFStringRef)::CFDictionaryGetValue( serviceToRegister, kDSNAttrRecordNameSAFE_CFSTR );
					if ( nameOfService )
						CFStringGetCString( nameOfService, name, sizeof(name), kCFStringEncodingUTF8 );
						
					CFStringRef	typeOfService = (CFStringRef)::CFDictionaryGetValue( serviceToRegister, kDS1AttrServiceTypeSAFE_CFSTR );
					if ( !typeOfService )
						typeOfService = (CFStringRef)::CFDictionaryGetValue( serviceToRegister, kDSNAttrRecordTypeSAFE_CFSTR );
				
					if ( typeOfService )
						CFStringGetCString( typeOfService, type, sizeof(type), kCFStringEncodingUTF8 );
				
					syslog( LOG_ERR, "DS (%s) couldn't register [%s] (%s) due to an error: %d!\n", GetProtocolPrefixString(), name, type, siResult );
				}
				
				CFRelease( serviceToRegister );
            }
			else
                DBGLOG( "CNSLPlugin::FlushRecord, couldn't find service to register!\n" );
        }
        else
        {
            siResult = eDSInvalidReference;
            DBGLOG( "CNSLPlugin::FlushRecord called with invalid fInRecRef.\n" );
        }
	}

	catch ( int err )
	{
		siResult = err;
	}

    return( siResult );
}	// DeleteRecord

#pragma mark -
sInt32 CNSLPlugin::AddAttributeValue ( sAddAttributeValue *inData )
{
    sInt32						siResult = eDSNoErr;
	CFMutableDictionaryRef		serviceToManipulate = NULL;
	
    DBGLOG( "CNSLPlugin::AddAttributeValue called\n" );
	try
	{
        if ( inData->fInRecRef )
        {
			LockOpenRefTable();
            
			serviceToManipulate = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInRecRef );
			
			if ( serviceToManipulate )
				CFRetain( serviceToManipulate );
				
			UnlockOpenRefTable();

			if ( serviceToManipulate && CFGetTypeID(serviceToManipulate) == CFDictionaryGetTypeID() )
			{
				CFStringRef		keyRef, valueRef;
				CFTypeRef		existingValueRef = NULL;
	
				keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
	
				valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttrValue->fBufferData, kCFStringEncodingUTF8 );
	
				if ( keyRef && valueRef )
				{
					existingValueRef = ::CFDictionaryGetValue( serviceToManipulate, keyRef );
					if ( existingValueRef && ::CFGetTypeID( existingValueRef ) == ::CFArrayGetTypeID() )
					{
						// this key is already represented by an array of values, just append this latest one
						::CFArrayAppendValue( (CFMutableArrayRef)existingValueRef, valueRef );
					}
					else if ( existingValueRef && ::CFGetTypeID( existingValueRef ) == ::CFStringGetTypeID() )
					{
						// if this is the service type, then we want to ignore as this was already set...
						if ( ::CFStringCompare( (CFStringRef)keyRef, kDS1AttrServiceTypeSAFE_CFSTR, 0 ) != kCFCompareEqualTo )
						{
							// is this value the same as what we want to add?  If so skip, otherwise make it an array
							if ( ::CFStringCompare( (CFStringRef)existingValueRef, valueRef, 0 ) != kCFCompareEqualTo )
							{
								// this key was represented by a string, we need to swap it with an new array with the two values
								CFStringRef			oldStringRef = (CFStringRef)existingValueRef;
								CFMutableArrayRef	newArrayRef = ::CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
								
								::CFArrayAppendValue( newArrayRef, oldStringRef );
								::CFArrayAppendValue( newArrayRef, valueRef );
								
								::CFDictionaryRemoveValue(  serviceToManipulate, keyRef );
								
								::CFDictionaryAddValue( serviceToManipulate, keyRef, newArrayRef );
								
								::CFRelease( newArrayRef );
							}
						}
					}
					else
					{
						// nothing already there, we'll just add this string to the dictionary
						::CFDictionaryAddValue( serviceToManipulate, keyRef, valueRef );
					}
				
					::CFRelease( keyRef );
					::CFRelease( valueRef );
				}
				else if ( !keyRef )
					siResult = eDSInvalidRefType;
				else if ( !valueRef )
					siResult = eDSInvalidAttrValueRef;
			}
			else
				siResult = eDSInvalidRecordRef;			

			if ( serviceToManipulate )
				CFRelease( serviceToManipulate );
        }
        else
        {
            siResult = eDSInvalidRecordRef;
            DBGLOG( "CNSLPlugin::AddAttributeValue called but with no value in fInRecRef (0x%x) not in our list.\n", (const void*)inData->fInRecRef );
        }
	}

	catch ( int err )
	{
		siResult = err;
	}
    
    return( siResult );
}

sInt32 CNSLPlugin::RemoveAttribute ( sRemoveAttribute *inData )
{
    sInt32						siResult = eDSNoErr;
	CFMutableDictionaryRef		serviceToManipulate = NULL;

	try
	{
        if ( inData->fInRecRef )
        {
			LockOpenRefTable();

            serviceToManipulate = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInRecRef );
    
            CFStringRef		keyRef;

            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttribute->fBufferData, kCFStringEncodingUTF8 );

			if ( keyRef )
			{
				::CFDictionaryRemoveValue( serviceToManipulate, keyRef );
				::CFRelease( keyRef );
			}
				
			UnlockOpenRefTable();
        }
        else
        {
            siResult = eDSInvalidRecordRef;
            DBGLOG( "CNSLPlugin::RemoveAttribute called but with no value in fOurRecRef.\n" );
        }
	}

	catch ( int err )
	{
		siResult = err;
	}

    return( siResult );
}

sInt32 CNSLPlugin::RemoveAttributeValue ( sRemoveAttributeValue *inData )
{
    sInt32						siResult = eDSNoErr;
	CFMutableDictionaryRef		serviceToManipulate = NULL;
	
	try
	{
        if ( inData->fInRecRef )
        {
			LockOpenRefTable();

            serviceToManipulate = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInRecRef );
    
			if ( serviceToManipulate )
				CFRetain( serviceToManipulate );
				
			UnlockOpenRefTable();

			if ( serviceToManipulate )
			{
				CFStringRef			keyRef;
				CFPropertyListRef	valueRef;
	
				keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
	
				valueRef = (CFPropertyListRef)::CFDictionaryGetValue( serviceToManipulate, keyRef );
				
				if ( valueRef && ::CFGetTypeID( valueRef ) == ::CFArrayGetTypeID() )
				{
					if ( (UInt32)::CFArrayGetCount( (CFMutableArrayRef)valueRef ) > inData->fInAttrValueID )
						::CFArrayRemoveValueAtIndex( (CFMutableArrayRef)valueRef, inData->fInAttrValueID );
					else
						siResult = eDSIndexOutOfRange;
				}
				else if ( valueRef && ::CFGetTypeID( valueRef ) == ::CFStringGetTypeID() )
				{
					::CFDictionaryRemoveValue( serviceToManipulate, keyRef );
				}
				else
					siResult = eDSInvalidAttrValueRef;
					
				if ( keyRef )
					::CFRelease( keyRef );
				
				if ( valueRef )
					::CFRelease( valueRef );

				CFRelease( serviceToManipulate );
			}
        }
        else
        {
            siResult = eDSInvalidRecordRef;
            DBGLOG( "CNSLPlugin::RemoveAttributeValue called but with no value in fOurRecRef.\n" );
        }
	}

	catch ( int err )
	{
		siResult = err;
	}

    return( siResult );
}

sInt32 CNSLPlugin::SetAttributeValue ( sSetAttributeValue *inData )
{
    sInt32						siResult = eDSNoErr;
	CFMutableDictionaryRef		serviceToManipulate = NULL;
	
	try
	{
        if ( inData->fInRecRef )
        {
			LockOpenRefTable();

            serviceToManipulate = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInRecRef );
			
			if ( serviceToManipulate )
				CFRetain( serviceToManipulate );
	
			UnlockOpenRefTable();

			if ( serviceToManipulate )
			{
				CFStringRef			keyRef = NULL, valueRef = NULL;
				CFMutableArrayRef	attributeArrayRef = NULL;
	
				keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
				valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttrValueEntry->fAttributeValueData.fBufferData, kCFStringEncodingUTF8 );
	
				if ( keyRef )
					attributeArrayRef = (CFMutableArrayRef)::CFDictionaryGetValue( serviceToManipulate, keyRef );
				
				if ( attributeArrayRef && ::CFGetTypeID( attributeArrayRef ) == ::CFArrayGetTypeID() )
				{
					if ( (UInt32)::CFArrayGetCount( (CFMutableArrayRef)attributeArrayRef ) > inData->fInAttrValueEntry->fAttributeValueID )
						::CFArraySetValueAtIndex( (CFMutableArrayRef)attributeArrayRef, inData->fInAttrValueEntry->fAttributeValueID, valueRef );
					else
						siResult = eDSIndexOutOfRange;
				}
				else if ( attributeArrayRef && ::CFGetTypeID( attributeArrayRef ) == ::CFStringGetTypeID() )
				{
					::CFDictionaryRemoveValue( serviceToManipulate, keyRef );
					::CFDictionarySetValue( serviceToManipulate, keyRef, valueRef );
				}
				else
					siResult = eDSInvalidAttrValueRef;
	
				if ( keyRef )
					CFRelease( keyRef );

				if ( valueRef )
					CFRelease( valueRef );
				
				CFRelease( serviceToManipulate );
			}
        }
        else
        {
            siResult = eDSInvalidRecordRef;
            DBGLOG( "CNSLPlugin::SetAttributeValue called but with no value in fOurRecRef.\n" );
        }
	}

	catch ( int err )
	{
		siResult = err;
	}

    return( siResult );
}

sInt32 CNSLPlugin::RegisterService( tRecordReference recordRef, CFDictionaryRef service )
{
    return eDSReadOnly;
}

sInt32 CNSLPlugin::DeregisterService( tRecordReference recordRef, CFDictionaryRef service )
{
    return eDSReadOnly;
}

#pragma mark -
void CNSLPlugin::HandleNetworkTransitionIfTime( void )
{
	if (time(nil) >= mTransitionCheckTime)
		HandleNetworkTransition( NULL );
}

sInt32 CNSLPlugin::HandleNetworkTransition( sHeader *inData )
{
    sInt32					siResult			= eDSNoErr;
    
	DBGLOG( "CNSLPlugin::HandleNetworkTransition called (%s)\n", GetProtocolPrefixString() );
	if ( mActivatedByNSL && IsActive() )
    {
		ClearOutAllNodes();
		ResetNodeLookupTimer( kNodeTimerIntervalImmediate );
	}
	else
	{
		DBGLOG( "CNSLPlugin::HandleNetworkTransition called (%s) but mActivatedByNSL == %d && IsActive() == %d \n", GetProtocolPrefixString(), mActivatedByNSL, IsActive()  );
	}
	
	return siResult;
}

Boolean CNSLPlugin::IsClientAuthorizedToCreateRecords( sCreateRecord *inData )
{
    return true;
}

Boolean CNSLPlugin::ResultMatchesRequestCriteria( const CNSLResult* result, sGetRecordList* request )
{
	// things that need to match up:
	// record type ( we're assuming this is fine for now )
	// record name ( a particular entry, dsRecordsAll, dsRecordsStandardAll, or dsRecordsNativeAll )
	if ( request->fInPatternMatch == eDSAnyMatch )
		return true;
	
	Boolean			resultIsOK = false;
	CAttributeList* cpRecNameList = new CAttributeList( request->fInRecNameList );
	CFStringRef		resultRecordNameRef = result->GetAttributeRef( kDSNAttrRecordNameSAFE_CFSTR );
	
	if ( cpRecNameList )
	{
		sInt32		error = eDSNoErr;
		uInt32		numRecNames = cpRecNameList->GetCount();
		char		*pRecName = nil;

		for ( uInt16 i=1; !resultIsOK && i<=numRecNames; i++ )
		{
			if ( (error = cpRecNameList->GetAttribute( i, &pRecName )) == eDSNoErr )
			{
				CFStringRef		recNameMatchRef = NULL;
				
				if ( pRecName && strcmp( pRecName, kDSRecordsAll ) == 0 )
				{
					resultIsOK = true;
					break;
				}
				else if ( pRecName && strcmp( pRecName, kDSRecordsStandardAll ) == 0 )
				{
					if ( result->GetAttributeRef( kDSNAttrRecordNameSAFE_CFSTR ) && CFStringHasPrefix( result->GetAttributeRef( kDSNAttrRecordTypeSAFE_CFSTR ), kDSStdRecordTypePrefixSAFE_CFSTR ) )
					{
						resultIsOK = true;
						break;
					}
					else
						continue;
				}
				else if ( pRecName && strcmp( pRecName, kDSRecordsNativeAll ) == 0 )
				{
					if ( result->GetAttributeRef( kDSNAttrRecordNameSAFE_CFSTR ) && CFStringHasPrefix( result->GetAttributeRef( kDSNAttrRecordTypeSAFE_CFSTR ), kDSNativeRecordTypePrefixSAFE_CFSTR ) )
					{
						resultIsOK = true;
						break;
					}
					else
						continue;
				}
				
				recNameMatchRef = CFStringCreateWithCString( NULL, pRecName, kCFStringEncodingUTF8 );
				
				if ( recNameMatchRef )
				{
					if ( request->fInPatternMatch == eDSExact && CFStringCompare( resultRecordNameRef, recNameMatchRef, 0 ) == kCFCompareEqualTo )
					{
						resultIsOK = true;
					}
					else if ( request->fInPatternMatch >= eDSiExact && CFStringCompare( resultRecordNameRef, recNameMatchRef, kCFCompareCaseInsensitive ) == kCFCompareEqualTo )
					{
						resultIsOK = true;
					}
					else if ( request->fInPatternMatch == eDSStartsWith && CFStringHasPrefix( resultRecordNameRef, recNameMatchRef ) )
					{
						resultIsOK = true;
					}
					else if ( request->fInPatternMatch == eDSEndsWith && CFStringHasSuffix( resultRecordNameRef, recNameMatchRef ) )
					{
						resultIsOK = true;
					}
					else if ( request->fInPatternMatch == eDSContains || request->fInPatternMatch == eDSiContains )
					{
						CFOptionFlags	options = (request->fInPatternMatch == eDSContains)?0:kCFCompareCaseInsensitive;
						CFRange 	result = CFStringFind( resultRecordNameRef, recNameMatchRef, options );
						
						if ( result.length > 0 )
							resultIsOK = true;
					}

					CFRelease( recNameMatchRef );
				}
			}
		}
		
		delete cpRecNameList;
	}
	
	return resultIsOK;
}

sInt32 CNSLPlugin::RetrieveResults( sGetRecordList* inData, CNSLDirNodeRep* nodeRep )
{
    sInt32					siResult			= eDSNoErr;
	CDataBuff*				aRecData			= nil;
	CDataBuff*				aAttrData			= nil;
	CDataBuff*				aTempData			= nil;
    CBuff*					outBuff				= nil;
    
    DBGLOG( "CNSLPlugin::RetrieveResults called for %lx\n", inData->fInNodeRef );

	// we only support certain pattern matches which are defined in CNSLPlugin::ResultMatchesRequestCriteria
	if (	inData->fInPatternMatch != eDSExact
		&&	inData->fInPatternMatch != eDSiExact
		&&	inData->fInPatternMatch != eDSAnyMatch
		&&	inData->fInPatternMatch != eDSStartsWith 
		&&	inData->fInPatternMatch != eDSEndsWith 
		&&	inData->fInPatternMatch != eDSContains 
		&&	inData->fInPatternMatch != eDSiContains )
	{
		return eNotYetImplemented;
    }
	
	try
    {    	
		aRecData = new CDataBuff(kNSLRecBuffDataDefaultSize);
		if( !aRecData )  throw( eMemoryError );
		aAttrData = new CDataBuff(kNSLBuffDataDefaultSize);
		if( !aAttrData )  throw( eMemoryError );
		aTempData = new CDataBuff(kNSLBuffDataDefaultSize);
		if( !aTempData )  throw( eMemoryError );
        
        // copy the buffer data into a more manageable form
        outBuff = new CBuff();
        if( !outBuff )  throw( eMemoryError );
    
        siResult = outBuff->Initialize( inData->fInDataBuff, true );
        if( siResult ) throw ( siResult );
    
        siResult = outBuff->GetBuffStatus();
        if( siResult ) throw ( siResult );
    
        siResult = outBuff->SetBuffType( kClientSideParsingBuff );
        if( siResult ) throw ( siResult );
        
        while ( nodeRep->HaveResults() && !siResult )
        {
            // package the record into the DS format into the buffer
            // steps to add an entry record to the buffer
            const CNSLResult*		newResult = nodeRep->GetNextResult();

			// ok, so first check to see that this result matches the caller's criteria
			if ( !ResultMatchesRequestCriteria( newResult, inData ) )
				continue;		// get next one
			
			CLEAR_DATA_BUFFER(aAttrData);

			aRecData->Clear(kNSLRecBuffDataDefaultSize);
            
            if ( newResult )
            {
                char		stackBuf[1024];
				char*		curURL = NULL;
                char*		curServiceType = NULL;
                CFIndex		curServiceTypeLength = 0;
                CFStringRef	serviceTypeRef = NULL;
                char*		recordName = NULL;
                CFIndex		recordNameLength = 0;
				CFStringRef	recordNameRef = NULL;

                serviceTypeRef = newResult->GetServiceTypeRef();
                
				if ( !serviceTypeRef )
					continue;			// skip this result
					
				curServiceTypeLength = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(serviceTypeRef), kCFStringEncodingUTF8) +1;
                if ( curServiceTypeLength > (CFIndex)sizeof(stackBuf) )
					curServiceType = (char*)malloc( curServiceTypeLength+1 );
				else
					curServiceType = stackBuf;
					
                if ( !::CFStringGetCString( serviceTypeRef, curServiceType, curServiceTypeLength+1, kCFStringEncodingUTF8 ) )
                {
                    DBGLOG( "CNSLPlugin::RetrieveResults couldn't convert serviceTypeRef CFString to char* in UTF8!\n" );
                    curServiceType[0] = '\0';
                }
                else
                    DBGLOG( "CNSLPlugin::RetrieveResults curServiceType=%s\n", curServiceType );
                    
				Boolean	needToFreeRecType = false;
				char*	recType = CreateRecTypeFromURL( curServiceType, &needToFreeRecType );

				if ( curServiceType != stackBuf )
					free( curServiceType );
				curServiceType = NULL;

				if ( recType != nil )
				{
					DBGLOG( "CNSLPlugin::RetrieveResults recType=%s\n", recType );
					aRecData->AppendShort( ::strlen( recType ) );
					aRecData->AppendString( recType );
					
					if ( needToFreeRecType )
						free(recType);
				} // what to do if the recType is nil? - never get here then
				else
				{
					aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
					aRecData->AppendString( "Record Type Unknown" );
				}
				
				// now the record name
				recordNameRef = newResult->GetAttributeRef( kDSNAttrRecordNameSAFE_CFSTR );
			
				if ( !recordNameRef )
				{
					continue;			// skip this result
				}
				
				recordNameLength = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(recordNameRef), kCFStringEncodingUTF8) +1;
			
				if ( recordNameLength > (CFIndex)sizeof(stackBuf) )
					recordName = (char*)malloc( recordNameLength );
				else
					recordName = stackBuf;
					
				if ( !::CFStringGetCString( recordNameRef, recordName, recordNameLength, kCFStringEncodingUTF8 ) )
				{
					DBGLOG( "CNSLPlugin::RetrieveResults couldn't convert recordNameRef CFString to char* in UTF8!\n" );
					recordName[0] = '\0';
				}
				else
					DBGLOG( "CNSLPlugin::RetrieveResults recordName=%s\n", recordName );
			
				if ( recordNameRef != nil && recordName[0] != '\0' )
				{
					aRecData->AppendShort( ::strlen( recordName ) );
					aRecData->AppendString( recordName );
				}
				else
				{
					aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
					aRecData->AppendString( "Record Name Unknown" );
				}
				
				if ( recordName != stackBuf )
					free( recordName );
				recordName = NULL;
				
				// now the attributes
				AttrDataContext		context = {0,aAttrData,inData->fInAttribInfoOnly};
					
			
				if ( newResult->GetAttributeDict() )
				{
					// we want to pull out the appropriate attributes that the client is searching for...
					uInt32					numAttrTypes	= 0;
					CAttributeList			cpAttributeList( inData->fInAttribTypeList );
			
					//save the number of rec types here to use in separating the buffer data
					numAttrTypes = cpAttributeList.GetCount();
			
					if( (numAttrTypes == 0) ) throw( eDSEmptyRecordTypeList );
			
					sInt32					error = eDSNoErr;
					char*				   	pAttrType = nil;
					char*					valueBuf = NULL;
					
					DBGLOG( "CNSLPlugin::RetrieveResults, numAttrTypes:%ld\n", numAttrTypes );
					for ( uInt32 i=1; i<=numAttrTypes; i++ )
					{
						if ( (error = cpAttributeList.GetAttribute( i, &pAttrType )) == eDSNoErr )
						{
							DBGLOG( "CNSLPlugin::RetrieveResults, GetAttribute returned pAttrType:%s\n", pAttrType );
							
							if ( strcmp( pAttrType, kDSAttributesAll ) == 0 )
							{
								// we want to return everything
								::CFDictionaryApplyFunction( newResult->GetAttributeDict(), AddToAttrData, &context );
							}
							else
							{
								char			stackBuf[1024];
								CFStringRef		keyRef = ::CFStringCreateWithCString( NULL, pAttrType, kCFStringEncodingUTF8 );
								CFStringRef		valueRef = NULL;
								CFTypeRef		valueTypeRef = NULL;
								CFArrayRef		valueArrayRef	= NULL;
								
								CLEAR_DATA_BUFFER(aTempData);
								
								if ( ::CFDictionaryContainsKey( newResult->GetAttributeDict(), keyRef ) )
									valueTypeRef = (CFTypeRef)::CFDictionaryGetValue( newResult->GetAttributeDict(), keyRef );
								
								CFRelease( keyRef );
								
								if ( valueTypeRef )
								{
									if ( CFGetTypeID(valueTypeRef) == CFArrayGetTypeID() )
									{
										// just point our valueArrayRef at this
										valueArrayRef = (CFArrayRef)valueTypeRef;
										CFRetain( valueArrayRef );					// so we can release this
									}
									else 
									if ( CFGetTypeID(valueTypeRef) == CFStringGetTypeID() )
									{
										valueRef = (CFStringRef)valueTypeRef;
										valueArrayRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
										
										if ( CFStringGetLength( valueRef ) > 0 )
											CFArrayAppendValue( (CFMutableArrayRef)valueArrayRef, valueRef );
									}
									else
										DBGLOG( "CNSLPlugin::RetrieveResults, got unknown value type (%ld), ignore\n", CFGetTypeID(valueTypeRef) );
							
									if ( valueArrayRef )
									{								
										aTempData->AppendShort( ::strlen( pAttrType ) );		// attrTypeLen
										aTempData->AppendString( pAttrType );					// attrType
									
										DBGLOG( "CNSLPlugin::RetrieveResults, adding pAttrType: %s\n", pAttrType );
			
										// Append the attribute value count
										aTempData->AppendShort( CFArrayGetCount(valueArrayRef) );	// attrValueCnt
							
										CFIndex		valueArrayCount = CFArrayGetCount(valueArrayRef);
										for ( CFIndex i=0; i<valueArrayCount ; i++ )
										{
											valueRef = (CFStringRef)::CFArrayGetValueAtIndex( valueArrayRef, i );
							
											if ( !inData->fInAttribInfoOnly && valueRef && ::CFStringGetLength(valueRef) > 0 )
											{
												CFIndex		maxValueEncodedSize = ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueRef),kCFStringEncodingUTF8) + 1;
												
												if ( maxValueEncodedSize > (CFIndex)sizeof(stackBuf) )
													valueBuf = (char*)malloc( maxValueEncodedSize );
												else
													valueBuf = stackBuf;
													
												if ( ::CFStringGetCString( valueRef, valueBuf, maxValueEncodedSize, kCFStringEncodingUTF8 ) )	
												{
													// Append attribute value
													aTempData->AppendShort( ::strlen( valueBuf ) );
													aTempData->AppendString( valueBuf );
											
													DBGLOG( "CNSLPlugin::RetrieveResults, adding valueBuf: %s\n", valueBuf );
												}
												else
													DBGLOG( "CNSLPlugin::RetrieveResults, CFStringGetCString couldn't create a string for valueRef!\n" );
											
												if ( valueBuf != stackBuf )
													delete( valueBuf );
												valueBuf = NULL;
											}
										}
									
										CFRelease( valueArrayRef );		// now release
										valueArrayRef = NULL;
									}
									else if ( valueRef && CFStringGetLength(valueRef) == 0 )
										DBGLOG( "CNSLPlugin::RetrieveResults, CFStringGetLength(valueRef) == 0!\n" );
									else if ( inData->fInAttribInfoOnly )
										DBGLOG( "CNSLPlugin::RetrieveResults, inData->fInAttribInfoOnly\n" );
									
									context.count++;
			
									aAttrData->AppendShort(aTempData->GetLength());
									aAttrData->AppendBlock(aTempData->GetData(), aTempData->GetLength() );
								}
							}
						}
						else
						{
							DBGLOG( "CNSLPlugin::RetrieveResults, GetAttribute returned error:%li\n", error );
						}
					}
				}
			
				// Attribute count
				aRecData->AppendShort( context.count );
				
				// now add the attributes to the record
				aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
			
				DBGLOG( "CNSLPlugin::RetrieveResults calling outBuff->AddData()\n" );
				siResult = outBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
                
                if ( curURL )
					free( curURL );
                curURL = NULL;
				
                if ( siResult == CBuff::kBuffFull )
                {
                    DBGLOG( "CNSLPlugin::RetrieveResults siResult == CBuff::kBuffFull, reset last result for next time\n" );
                    nodeRep->NeedToRedoLastResult();			// couldn't get this one in the buffer
					
					UInt32	numDataBlocks = 0;
					
					if ( outBuff->GetDataBlockCount( &numDataBlocks ) != eDSNoErr || numDataBlocks == 0 )
						siResult = eDSBufferTooSmall;	// we couldn't fit any other data in here either
                }
                else
                    inData->fOutRecEntryCount++;
            }
        }

        if ( siResult == CBuff::kBuffFull )
            siResult = eDSNoErr;
            
        outBuff->SetLengthToSize();
   }

    catch ( int err )
    {
        siResult = err;
    }

    if (aRecData != nil)
    {
        delete (aRecData);
        aRecData = nil;
    }
	
    if (aTempData != nil)
    {
        delete (aTempData);
        aTempData = nil;
    }
	
    if (aAttrData != nil)
    {
        delete (aAttrData);
        aAttrData = nil;
    }

    if ( outBuff != nil )
    {
        delete( outBuff );
        outBuff = nil;
    }

    return( siResult );
    
}

// ---------------------------------------------------------------------------
//	* CreateNSLTypeFromRecType
// ---------------------------------------------------------------------------

char* CNSLPlugin::CreateNSLTypeFromRecType ( char *inRecType, Boolean* needToFree )
{
    char				   *outResult	= nil;
    uInt32					uiStrLen	= 0;
    uInt32					uiNativeLen	= ::strlen( kDSNativeRecordTypePrefix );
    uInt32					uiStdLen	= ::strlen( kDSStdRecordTypePrefix );
	
 DBGLOG( "CNSLPlugin::CreateNSLTypeFromRecType called on %s\n", inRecType );
	//idea here is to use the inIndex to request a specific map
	//if inIndex is 1 then the first map will be returned
	//if inIndex is >= 1 and <= totalCount then that map will be returned
	//if inIndex is <= 0 then nil will be returned
	//if inIndex is > totalCount nil will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get maps until nil is returned
    
	*needToFree = false;
	
    if ( ( inRecType != nil ) )
    {
        uiStrLen = ::strlen( inRecType );

        // First look for native record type
        if ( ::strncmp( inRecType, kDSNativeRecordTypePrefix, uiNativeLen ) == 0 )
        {
            DBGLOG( "CNSLPlugin::CreateNSLTypeFromRecType kDSNativeRecordTypePrefix, uiStrLen:%ld uiNativeLen:%ld\n", uiStrLen, uiNativeLen );
            // Make sure we have data past the prefix
            if ( uiStrLen > uiNativeLen )
            {
                uiStrLen = uiStrLen - uiNativeLen;
                outResult = new char[ uiStrLen + 2 ];
                ::strcpy( outResult, inRecType + uiNativeLen );
				*needToFree = true;
            }
        }//native maps
        //now deal with the standard mappings
		else if ( ::strncmp( inRecType, kDSStdRecordTypePrefix, uiStdLen ) == 0 )
		{
            DBGLOG( "CNSLPlugin::CreateNSLTypeFromRecType kDSStdRecordTypePrefix, uiStrLen:%ld uiStdLen:%ld\n", uiStrLen, uiStdLen );
            if ( strcmp( inRecType, kDSStdRecordTypeAFPServer ) == 0 )
            {
                outResult = kAFPServiceType;
            }
            else
            if ( strcmp( inRecType, kDSStdRecordTypeSMBServer ) == 0 )
            {
                outResult = kSMBServiceType;
            }
            else
            if ( strcmp( inRecType, kDSStdRecordTypeFTPServer ) == 0 )
            {
                outResult = kFTPServiceType;
            }
            else
            if ( strcmp( inRecType, kDSStdRecordTypeNFS ) == 0 )
            {
                outResult = kNFSServiceType;
            }
            else
            if ( strcmp( inRecType, kDSStdRecordTypeWebServer ) == 0 )
            {
                outResult = kHTTPServiceType;
            }
            else
            if ( strcmp( inRecType, kDSStdRecordTypePrinters ) == 0 )
            {
                outResult =kLaserWriterServiceType;
            }
            else if ( uiStrLen > uiStdLen )
            {
				// don't return a result for standard types we don't support
/*                uiStrLen = uiStrLen - uiStdLen;
                outResult = new char[ uiStrLen + 2 ];
                ::strcpy( outResult, inRecType + uiStdLen );
*/
            }
            else
                DBGLOG( "CNSLPlugin::CreateNSLTypeFromRecType, uiStrLen:%ld <= uiStdLen:%ld\n", uiStrLen, uiStdLen );
		}//standard maps
        else
            DBGLOG( "CNSLPlugin::CreateNSLTypeFromRecType, inRecType:%s doesn't map to \"dsRecTypeNative:\" OR \"dsRecTypeStandard:\"!\n", inRecType );
    }// ( inRecType != nil )

    DBGLOG( "CNSLPlugin::CreateNSLTypeFromRecType mapping %s to %s\n", inRecType, outResult );
    return( outResult );

} // CreateNSLTypeFromRecType

// ---------------------------------------------------------------------------
//	* CreateRecTypeFromURL
// ---------------------------------------------------------------------------

char* CNSLPlugin::CreateRecTypeFromURL( char *inNSLType, Boolean* needToFree )
{
    char				   *outResult	= nil;
    uInt32					uiStrLen	= 0;
	
 DBGLOG( "CNSLPlugin::CreateRecTypeFromURL called to map %s to a DS type\n", inNSLType );
	//idea here is to use the inIndex to request a specific map
	//if inIndex is 1 then the first map will be returned
	//if inIndex is >= 1 and <= totalCount then that map will be returned
	//if inIndex is <= 0 then nil will be returned
	//if inIndex is > totalCount nil will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get maps until nil is returned
    *needToFree = false;
	
    if ( inNSLType != nil )
    {
        uiStrLen = ::strlen( inNSLType );

        if ( strcmp( inNSLType, kAFPServiceType ) == 0 )
        {
            outResult = kDSStdRecordTypeAFPServer;
        }
        else if ( strcmp( inNSLType, kSMBServiceType ) == 0 )
        {
            outResult = kDSStdRecordTypeSMBServer;
        }
        else if ( strcmp( inNSLType, kNFSServiceType ) == 0 )
        {
            outResult = kDSStdRecordTypeNFS;
        }
        else if ( strcmp( inNSLType, kFTPServiceType ) == 0 )
        {
            outResult = kDSStdRecordTypeFTPServer;
        }
        else if ( strcmp( inNSLType, kHTTPServiceType ) == 0 )
        {
            outResult = kDSStdRecordTypeWebServer;
        }
        else
        {
            // native record type
            outResult = new char[1+::strlen(kDSNativeAttrTypePrefix)+strlen(inNSLType)];
            ::strcpy(outResult,kDSNativeAttrTypePrefix);
            ::strcat(outResult,inNSLType);
			*needToFree = true;
        }
    }// ( inNSLType != nil )

    return( outResult );

} // CreateRecTypeFromURL

CFStringRef CNSLPlugin::CreateRecTypeFromNativeType ( char *inNativeType )
{
    CFMutableStringRef	   	outResultRef	= NULL;
	
	DBGLOG( "CNSLPlugin::CreateRecTypeFromNativeType called on %s\n", inNativeType );

    if ( ( inNativeType != nil ) )
    {
		outResultRef = CFStringCreateMutable( NULL, 0 );
        
		if ( ::strcmp( inNativeType, kAFPServiceType ) == 0 )
		{
			CFStringAppend( outResultRef, kDSStdRecordTypeAFPServerSAFE_CFSTR );
		}
		else if ( ::strcmp( inNativeType, kSMBServiceType ) == 0 )
		{
			CFStringAppend( outResultRef, kDSStdRecordTypeSMBServerSAFE_CFSTR );
		}
		else if ( ::strcmp( inNativeType, kNFSServiceType ) == 0 )
		{
			CFStringAppend( outResultRef, kDSStdRecordTypeNFSSAFE_CFSTR );
		}
		else if ( ::strcmp( inNativeType, kDSStdRecordTypeFTPServer ) == 0 )
		{
			CFStringAppend( outResultRef, kDSStdRecordTypeFTPServerSAFE_CFSTR );
		}
		else if ( ::strcmp( inNativeType, kDSStdRecordTypeWebServer ) == 0 )
		{
			CFStringAppend( outResultRef, kDSStdRecordTypeWebServerSAFE_CFSTR );
		}
		else if ( outResultRef )
		{
			CFStringAppend( outResultRef, kDSNativeRecordTypePrefixSAFE_CFSTR );
			
			CFStringRef	nativeStringRef = CFStringCreateWithCString( NULL, inNativeType, kCFStringEncodingUTF8 );
			
			if ( nativeStringRef )
			{
				CFStringAppend( outResultRef, nativeStringRef );
				CFRelease( nativeStringRef );
			}
		}
	}
	
	return( outResultRef );

} // CreateNSLTypeFromRecType

CFStringRef CNSLPlugin::CreateRecTypeFromNativeType ( CFStringRef inNativeType )
{
	CFStringRef	returnTypeRef = NULL;
	UInt32		nativeCStrLen = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(inNativeType), kCFStringEncodingUTF8 ) + 1;
	char*		nativeCStr = (char*)malloc( nativeCStrLen );

	::CFStringGetCString( inNativeType, nativeCStr, nativeCStrLen, kCFStringEncodingUTF8 );

	returnTypeRef = CreateRecTypeFromNativeType( nativeCStr );
	free( nativeCStr );
	
    return returnTypeRef;
	
} // CreateNSLTypeFromRecType

#pragma mark -
void CNSLPlugin::StartNodeLookup( void )
{
    mLastNodeLookupStartTime = GetCurrentTime();		// get current time
    DBGLOG( "CNSLPlugin::StartNodeLookup (%s), called\n", GetProtocolPrefixString() );
    
	ResetNodeLookupTimer( GetTimeBetweenNodeLookups() );	// This is how long before we fire another node lookup (barring network events)
	
    NewNodeLookup();
}

sInt16 CNSLPlugin::NumOutstandingSearches( void )
{
    return gOutstandingSearches;
}

Boolean	 CNSLPlugin::OKToStartNewSearch( void )
{
#ifdef RESTRICT_NUMBER_OF_SEARCHES
    pthread_mutex_lock(&gOutstandingSearchesLock);

    Boolean		okToStartNewSearch = NumOutstandingSearches() < kMaxNumOutstandingSearches;

    pthread_mutex_unlock(&gOutstandingSearchesLock);

    if ( okToStartNewSearch )
        DBGLOG( "CNSLPlugin::OKToStartNewSearch is returning true (%d current searches)\n", gOutstandingSearches );
    else
        DBGLOG( "CNSLPlugin::OKToStartNewSearch is returning false (%d current searches)\n", gOutstandingSearches );

    return okToStartNewSearch; 
#else
	return true;
#endif
}

void CNSLPlugin::StartSubNodeLookup( char* parentNodeName )
{
    DBGLOG( "CNSLPlugin::StartSubNodeLookup called on %s\n", parentNodeName );
    
    OKToOpenUnPublishedNode( parentNodeName );
}

void CNSLPlugin::ClearOutStaleNodes( void )
{
	DBGLOG( "CNSLPlugin::ClearOutStaleNodes (%s)\n", GetProtocolPrefixString() );

    // we want to look at each node we have registered and unregister any whose timestamp is older than
    // the start of our last node lookup search
    NSLNodeHandlerContext	context = {mPublishedNodes, kClearOutStaleNodes, (void*)GetLastNodeLookupStartTime(), NULL};
    CFIndex arrayCount;
	CFStringRef skeetString;
	
    LockPublishedNodes();
    ::CFDictionaryApplyFunction( mPublishedNodes, NSLNodeHandlerFunction, &context );

	if ( context.fNodesToRemove )
	{
		arrayCount = CFArrayGetCount( context.fNodesToRemove );
		for ( CFIndex i = 0; i < arrayCount; i++ )
		{
			skeetString = (CFStringRef)CFArrayGetValueAtIndex( context.fNodesToRemove, i );
			if ( skeetString != NULL )
				CFDictionaryRemoveValue( mPublishedNodes, skeetString );
		}
		
		CFRelease( context.fNodesToRemove );
    }
	
    UnlockPublishedNodes();
}


void CNSLPlugin::ClearOutAllNodes( void )
{
	DBGLOG( "CNSLPlugin::ClearOutAllNodes (%s)\n", GetProtocolPrefixString() );

    LockPublishedNodes();

	::CFDictionaryRemoveAllValues( mPublishedNodes );

    UnlockPublishedNodes();
}

void CNSLPlugin::QueueNewSearch( CNSLServiceLookupThread* newLookup )
{
    LockSearchQueue();
    if ( mSearchQueue )
    {
		if ( !mSearchTicklerInstalled )	
			InstallSearchTickler();
	
        ::CFArrayAppendValue( mSearchQueue, newLookup );
        DBGLOG( "CNSLPlugin::QueueNewSearch called, %ld searches waiting to start\n", CFArrayGetCount(mSearchQueue) );
    }
    UnlockSearchQueue();
}

#define DEQUEUE_LIFO 1
void CNSLPlugin::StartNextQueuedSearch( void )
{
    if ( mSearchQueue && ::CFArrayGetCount(mSearchQueue) > 0 )
    {
#if DEQUEUE_LIFO
        CNSLServiceLookupThread*	newLookup = (CNSLServiceLookupThread*)::CFArrayGetValueAtIndex( mSearchQueue, CFArrayGetCount(mSearchQueue)-1 );
        ::CFArrayRemoveValueAtIndex( mSearchQueue, CFArrayGetCount(mSearchQueue)-1 );
#else
        CNSLServiceLookupThread*	newLookup = (CNSLServiceLookupThread*)::CFArrayGetValueAtIndex( mSearchQueue, 0 );
        ::CFArrayRemoveValueAtIndex( mSearchQueue, 0 );
#endif        
		LockOpenRefTable();

		Boolean		newLookupInRefTable = ::CFDictionaryContainsValue( mOpenRefTable, newLookup->GetNodeToSearch() );

		UnlockOpenRefTable();

        if ( newLookupInRefTable && !newLookup->AreWeCanceled() )
        {
            DBGLOG( "CNSLPlugin::StartNextQueuedSearch starting new search, %ld searches waiting to start\n", CFArrayGetCount(mSearchQueue) );
            newLookup->Resume();
        }
        else
        {
            DBGLOG( "CNSLPlugin::StartNextQueuedSearch deleting already canceled search, %ld searches waiting to start\n", CFArrayGetCount(mSearchQueue) );
            delete newLookup;
        }
    }
}

void CNSLPlugin::StartServicesLookup( char* serviceType, CNSLDirNodeRep* nodeDirRep )
{
    if ( serviceType )
        NewServiceLookup( serviceType, nodeDirRep );
}

#pragma mark -

void NSLReleaseNodeData( CFAllocatorRef allocator, const void* value )
{
	NodeData*		nodeData = (NodeData*)value;
	
	if ( nodeData )
	{
		if ( nodeData->fDSName )
			DSUnregisterNode( nodeData->fSignature, nodeData->fDSName );
		DeallocateNodeData( nodeData );
	}
}

CFStringRef NSLNodeValueCopyDesctriptionCallback ( const void *value )
{
    NodeData*		nodeData = (NodeData*)value;
    
    return nodeData->fNodeName;
}

Boolean NSLNodeValueEqualCallback ( const void *value1, const void *value2 )
{
    NodeData*		nodeData1 = (NodeData*)value1;
    NodeData*		nodeData2 = (NodeData*)value2;
    Boolean			areEqual = false;
    
    if ( nodeData1 && nodeData2 && nodeData1->fNodeName && nodeData2->fNodeName )
        areEqual = ( CFStringCompare( nodeData1->fNodeName, nodeData2->fNodeName, kCFCompareCaseInsensitive ) == kCFCompareEqualTo );

    return areEqual;
}

void NSLNodeHandlerFunction(const void *inKey, const void *inValue, void *inContext)
{
    NodeData*					curNodeData = (NodeData*)inValue;
    NSLNodeHandlerContext*		context = (NSLNodeHandlerContext*)inContext;
    
    if ( !inKey || !inValue || !inContext )
        return;
        
    switch ( context->fMessage )
    {
        case kClearOutStaleNodes:
        {
			if ( curNodeData->fTimeStamp < (UInt32)(context->fDataPtr) )
            {
                // we need to delete this...
                DBGLOG( "NSLNodeHandlerFunction, Removing Node (%s) from published list\n", dsGetPathFromListPriv( curNodeData->fDSName, (char *)"\t" )
 );
//                DSUnregisterNode( curNodeData->fSignature, curNodeData->fDSName );
				
				if ( !context->fNodesToRemove )
					context->fNodesToRemove = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
				
				if ( curNodeData->fNodeName != NULL )
					CFArrayAppendValue( context->fNodesToRemove, curNodeData->fNodeName );			// can't manipulate dictionary while iterating
            }
        }    
        break;
        
        default:
            break;
    };
}


#pragma mark -
// ---------------------------------------------------------------------------
//	* MakeContextData
// ---------------------------------------------------------------------------

sNSLContextData* MakeContextData ( void )
{
    sNSLContextData   *pOut		= nil;
    sInt32			siResult	= eDSNoErr;

    pOut = (sNSLContextData *) calloc(1, sizeof(sNSLContextData));
    if ( pOut != nil )
    {
//        ::memset( pOut, 0, sizeof( sNSLContextData ) );
        //do nothing with return here since we know this is new
        //and we did a memset above
        siResult = CleanContextData(pOut);
    }

    return( pOut );

} // MakeContextData

// ---------------------------------------------------------------------------
//	* CleanContextData
// ---------------------------------------------------------------------------

sInt32 CleanContextData ( sNSLContextData *inContext )
{
    sInt32	siResult = eDSNoErr;
    
    if ( inContext == nil )
    {
        siResult = eDSBadContextData;
	}
    else
    {
        inContext->offset			= 0;
    }

    return( siResult );

} // CleanContextData

#pragma mark -
UInt32 GetCurrentTime( void )			// in seconds
{
    struct	timeval curTime;
    
    if ( gettimeofday( &curTime, NULL ) != 0 )
        fprintf( stderr, "call to gettimeofday returned error: %s", strerror(errno) );
    
    return curTime.tv_sec;
}

void AddToAttrData(const void *key, const void *value, void *context)
{
    CFStringRef			keyRef			= (CFStringRef)key;
    CFTypeRef			valueTypeRef	= (CFTypeRef)value;		// need to update this to support an array of values instead
    CFStringRef			valueRef		= NULL;
	CFArrayRef			valueArrayRef	= NULL;
    char				keyBuf[256] = {0};
	char				stackBuf[1024];
    char*				valueBuf	= NULL;
	CDataBuff*			aTmpData	= nil;
    AttrDataContext*	dataContext	= (AttrDataContext*)context;
    
    dataContext->count++;		// adding one more
    
    aTmpData = new CDataBuff(kNSLBuffDataDefaultSize);
    if( !aTmpData )  throw( eMemoryError );
    
	CLEAR_DATA_BUFFER(aTmpData);
    
	if ( valueTypeRef )
	{
		if ( CFGetTypeID(valueTypeRef) == CFArrayGetTypeID() )
		{
			// just point our valueArrayRef at this
			valueArrayRef = (CFArrayRef)valueTypeRef;
			CFRetain( valueArrayRef );					// so we can release this
		}
		else 
		if ( CFGetTypeID(valueTypeRef) == CFStringGetTypeID() )
		{
			valueRef = (CFStringRef)valueTypeRef;
			valueArrayRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			if ( CFStringGetLength( valueRef ) > 0 )
				CFArrayAppendValue( (CFMutableArrayRef)valueArrayRef, valueRef );
		}
		else
            DBGLOG( "AddToAttrData, got unknown value type (%ld), ignore\n", CFGetTypeID(valueTypeRef) );

		if ( valueArrayRef && ::CFStringGetCString( keyRef, keyBuf, sizeof(keyBuf), kCFStringEncodingUTF8 ) )
		{
			aTmpData->AppendShort( ::strlen( keyBuf ) );		// attrTypeLen
			aTmpData->AppendString( keyBuf );					// attrType
			
			DBGLOG( "AddToAttrData, adding keyBuf: %s\n", keyBuf );

			// Append the attribute value count
			aTmpData->AppendShort( CFArrayGetCount(valueArrayRef) );	// attrValueCnt

			CFIndex		valueArrayCount = CFArrayGetCount(valueArrayRef);
			for ( CFIndex i=0; i<valueArrayCount ; i++ )
			{
				valueRef = (CFStringRef)::CFArrayGetValueAtIndex( valueArrayRef, i );

				if ( !dataContext->attrOnly && ::CFStringGetLength(valueRef) > 0 )
				{
					CFIndex		maxValueEncodedSize = ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueRef), kCFStringEncodingUTF8) + 1;

					if ( maxValueEncodedSize > (CFIndex)sizeof(stackBuf) )
						valueBuf = (char*)malloc( maxValueEncodedSize );
					else
						valueBuf = stackBuf;
					
					if ( ::CFStringGetCString( valueRef, valueBuf, maxValueEncodedSize, kCFStringEncodingUTF8 ) )	
					{
						// Append attribute value
						aTmpData->AppendShort( ::strlen( valueBuf ) );	// valueLen
						aTmpData->AppendString( valueBuf );				// value
						
						DBGLOG( "AddToAttrData, adding valueBuf: %s\n", valueBuf );
					}
					else
					{
						DBGLOG( "AddToAttrData, CFStringGetCString couldn't create a string for valueRef!\n" );
						if ( getenv( "NSLDEBUG" ) )
							::CFShow( valueRef );
					}
						
					if ( valueBuf != stackBuf )
						delete( valueBuf );
					valueBuf = NULL;
				}
			}
				
			// Add the attribute length
			dataContext->attrDataBuf->AppendShort( aTmpData->GetLength() );
			dataContext->attrDataBuf->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
	
			CLEAR_DATA_BUFFER(aTmpData);
			
			delete aTmpData;
		}
	
		if ( valueArrayRef )
			CFRelease( valueArrayRef );
	}
}


void AddToAttrData2(CFStringRef keyRef, CFStringRef valueRef, void *context)
{	
}


/**************
 * LogHexDump *
 **************
 
 Log a hex and string formatted version of the given buffer.
 
*/
#define kOffset 				0
#define kHexData 				7
#define kASCIIData 				(kHexData+50)
#define kEOL 					(kASCIIData+16)
#define kLineLength 			(kEOL+1)
#define kLineEnding 			'\n'
#define kBufferTooBig 			-1
#define kMaxBufSize				10240
#define kMaxPacketLen	    	(((kMaxBufSize - 2)/kLineLength)*16)-15

int LogHexDump(char *pktPtr, long pktLen)
{
	static char	hexChars[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
	
	register unsigned char	*curPtr=NULL;	// pointer into the raw hex buffer
	register char	*hexPtr=NULL;			// pointer in line to put next hex output
	register char	*charPtr=NULL;			// place to put next character output
	char	*offsetPtr=NULL;					// pointer to the offset 
	unsigned char	*p=NULL;					// indexes through the "offset"
	short	i,j;
	char	*buf=NULL;
	long	bufLen;
	short	numLines;
	short	numCharsOnLine;
	short	remainder;
	short	offset;						// offset within raw packet
	
	if ( pktLen == 0 )
    {
        DBGLOG( "LogHexDump was passed in a pktLen of zero!\n" );
		return noErr;						// no data to log
	}

    if ( pktLen > kMaxPacketLen )
        pktLen = kMaxPacketLen;
    	
	numLines = (pktLen + 15)/16;			// total number of lines (one may be short)
	remainder = pktLen - (numLines-1)*16;	// number of chars on the last line
	
	bufLen = numLines * kLineLength + 2;	// size of the buffer required
	
	if (bufLen > kMaxBufSize)
    {
	    DBGLOG( "LogHexDump kBufferTooBig (%ld), setting to max log size (%d)\n", bufLen, kMaxBufSize );
        bufLen = kMaxBufSize;
    }
    
	DBGLOG( "Dumping %ld bytes of hex-format data:\n",pktLen );
	buf = (char*)malloc(bufLen);					//  (+2 to hold the NUL and final newline)
	
	if (!buf) 
    {	
        DBGLOG( "LogHexDump return eMemoryAllocError\n" );
        return(eMemoryAllocError);
	}
    
	for (i=0; i<bufLen; i++) 				// initialize to all spaces
		buf[i] = ' ';
		
	// now walk down the packet, turning each byte into a hexadecimal-type string,
	// and putting that and its character equivalent into the correct places in the
	// output string.
	
	curPtr = (unsigned char *)pktPtr;		// source data pointer
	offsetPtr = buf;						// one of the dest pointers
	for (j=0; j<numLines; j++) {			// for each line...
		
		hexPtr = offsetPtr+kHexData;		// pointer to first hex byte
		charPtr = offsetPtr+kASCIIData;		// pointer to first ASCII byte 
		
		offset = j*16;						// first output the current
		p = (unsigned char*)&offset;		//   offset at front of line
		*(offsetPtr++) = hexChars[(*p>>4)];
		*(offsetPtr++) = hexChars[(*p++&0x0F)]; // format will be 0000: xx...
		*(offsetPtr++) = hexChars[(*p>>4)];
		*(offsetPtr++) = hexChars[(*p&0x0F)];
		*offsetPtr = ':';					
		
		if (j == numLines-1)				// last line may be short
			numCharsOnLine = remainder;
		else numCharsOnLine = 16;

		for (i=0; i<numCharsOnLine; i++) {	// for the next line's worth
		
	// print the hex format of the current byte with a space separator
			*(hexPtr++) = hexChars[(*curPtr>>4)];
			*(hexPtr++) = hexChars[(*curPtr&0x0F)];
			hexPtr++;
			
	// now print the actual printable character, if we can
			if (isprint(*curPtr))
				*(charPtr++) = *curPtr;
			else *(charPtr++) = '.';		// for unprintables
			
			curPtr++;					// bump the raw-hex data pointer
		}
		*(charPtr++) = kLineEnding;		// add in the carriage return at end of each line
		offsetPtr = charPtr;			// and move to the next line in the dest buffer
	}
	*(charPtr++) = kLineEnding;
	*charPtr = '\0';					// and a terminating NULL
	
	DBGLOG( "%s\n",buf);
	free(buf);
	return noErr;
	
}

CFStringEncoding	gsEncoding = kCFStringEncodingInvalidId;
UInt32				gsRegion = 0;

CFStringEncoding NSLGetSystemEncoding( UInt32* outRegion )
{
	if ( gsEncoding == kCFStringEncodingInvalidId )
	{
#if Use_CFStringGetInstallationEncodingAndRegion
		uint32_t	encoding;
		uint32_t	region;
		
		__CFStringGetInstallationEncodingAndRegion( &encoding, &region );
#else
		// need to parse out the encoding from /var/root/.CFUserTextEncoding
		FILE *				fp;
		char 				buf[1024];
		CFStringEncoding	encoding = 0;
		UInt32				region = 0;
		
		fp = fopen("/var/root/.CFUserTextEncoding","r");
		
		if (fp == NULL) {
			DBGLOG( "NSLGetSystemEncoding: Could not open config file, return 0 (MacRoman)" );
			return 0;
		}
		
		if ( fgets(buf,sizeof(buf),fp) != NULL) 
		{
			int	i = 0;
			
			while ( buf[i] != '\0' && buf[i] != ':' )
				i++;
	
			buf[i] = '\0';
			
			char*	endPtr = NULL;
			encoding = strtol(buf,&endPtr,10);
			region = strtol(&buf[i+1],&endPtr,10);
		}
		
		fclose(fp);
#endif		
		gsEncoding = encoding;
		gsRegion = region;
	}
	
	DBGLOG( "NSLGetSystemEncoding: returning encoding (%ld), region (%ld)", gsEncoding, gsRegion );
	
	if ( outRegion )
		*outRegion = gsRegion;
		
	return gsEncoding;
}

// RFC 1034 rules:
// Host names must start with a letter, end with a letter or digit,
// and have as interior characters only letters, digits, and hyphen.
// This was subsequently modified in RFC 1123 to allow the first character to be either a letter or a digit
#define mdnsValidHostChar(X, notfirst, notlast) (isalpha(X) || isdigit(X) || ((notfirst) && (notlast) && (X) == '-') )

CFStringRef CreateRFC1034HostLabelFromUTF8Name( CFStringRef	UTF8NameRef, UInt32 maxNameLength )
{
	UInt8				UTF8Name[256];
	UInt8				hostLabel[256];
	CFStringRef			hostLabelRef = NULL;
	
	if ( CFStringGetPascalString( UTF8NameRef, UTF8Name, sizeof(UTF8Name), kCFStringEncodingUTF8 ) )
	{
		const UInt8 *		src  = &UTF8Name[1];
		const UInt8 *const	end  = &UTF8Name[1] + UTF8Name[0];
		UInt8 *				ptr  = &hostLabel[1];
		const UInt8 *const	lim  = &hostLabel[1] + maxNameLength;

		while (src < end)
		{
			// Delete apostrophes from source name
			if (src[0] == '\'')
			{
				src++;
				continue;		// Standard straight single quote
			}
			
			if (src + 2 < end && src[0] == 0xE2 && src[1] == 0x80 && src[2] == 0x99)
			{
				src += 3;
				continue;
			}
			
					// Unicode curly apostrophe
			if (ptr < lim)
			{
				if (mdnsValidHostChar(*src, (ptr > &hostLabel[1]), (src < end-1)))
					*ptr++ = *src;
				else if (ptr > &hostLabel[1] && ptr[-1] != '-')
					*ptr++ = '-';
			}
				
			src++;
		}
	
		while (ptr > &hostLabel[1] && ptr[-1] == '-')
			ptr--;	// Truncate trailing '-' marks
	
		hostLabel[0] = (UInt8)(ptr - &hostLabel[1]);
		
		if ( hostLabel[0] > 0 )
			hostLabelRef = CFStringCreateWithPascalString( NULL, hostLabel, kCFStringEncodingASCII );

	}

	return hostLabelRef;
}



