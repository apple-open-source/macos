/*
 *  CNSLPlugin.cpp
 *
 *	This is a wrapper base class that is to be used by migrating plugins from NSL
 *
 *  Created by imlucid on Tue Aug 14 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

//#include <Carbon/Carbon.h>

/*
#include "DSUtils.h"
#include "ServerModuleLib.h"
#include "CDataBuff.h"
#include "CPlugInRef.h"
#include "UException.h"
*/


#include "CNSLHeaders.h"

#ifndef kServerRunLoop
#define kServerRunLoop (eDSPluginCalls)(kHandleNetworkTransition + 1)
#endif

//#define DONT_WAIT_FOR_NSL_TO_ACTIVATE_US
//#define IDLE_CALL_IS_BUSTED
#define kTaskInterval	2

int LogHexDump(char *pktPtr, long pktLen);

boolean_t NetworkChangeCallBack(SCDynamicStoreRef session, void *callback_argument);

// These are the CFContainer callback protos
CFStringRef NSLNodeValueCopyDesctriptionCallback ( const void *item );
Boolean NSLNodeValueEqualCallback ( const void *value1, const void *value2 );
void NSLNodeHandlerFunction(const void *inKey, const void *inValue, void *inContext);
//pthread_mutex_t	CNSLPlugin::mQueueLock;
//pthread_mutex_t CNSLPlugin::mPluginLock;

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
    Boolean		okToStartNextSearch = false;
    
    pthread_mutex_lock(&gOutstandingSearchesLock);

    gOutstandingSearches--;
    
	DBGLOG( "SearchCompleted, gOutstandingSearches=%d\n", gOutstandingSearches );
#ifdef IDLE_CALL_IS_BUSTED
    if ( CNSLPlugin::TheNSLPlugin()->NumOutstandingSearches() < kMaxNumOutstandingSearches )
    	okToStartNextSearch = true;
#endif    
    pthread_mutex_unlock(&gOutstandingSearchesLock);

    if ( okToStartNextSearch )
        CNSLPlugin::TheNSLPlugin()->StartNextQueuedSearch();		// just give this a chance to fire one off
}

#pragma mark -
CFStringRef NSLQueuedSearchesCopyDesctriptionCallback ( const void *item )
{
    return CFSTR("DS NSL Queued Searche");
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
	mMinTimeBetweenNodeLookups = kMinTimeBetweenNodeLookups;
    mTimeToClearOutStaleNodes = false;
    mOurResources = NULL;
    mOpenRefTable = NULL;
    mActivatedByNSL = false;
    
    mDSLocalNodeLabel = NULL;
    mDSNetworkNodeLabel = NULL;
    mSearchQueue = NULL;
	mRunLoopRef = NULL;
	mTimerRef = NULL;
	mSearchTicklerInstalled = false;
	
	mState		= kUnknownState;
    
    gsTheNSLPlugin = this;
} // CNSLPlugin


CNSLPlugin::~CNSLPlugin( void )
{
	DBGLOG( "CNSLPlugin::~CNSLPlugin\n" );
	
	if ( mTimerRef )
	{
		UnInstallSearchTickler();
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
/*    
    if ( mOpenRefTable )
    {
        ::CFRelease( mOpenRefTable );
        mOpenRefTable = NULL;
    }
    
    if ( mUnOpenedRecordRefList )
    {
        ::CFRelease( mUnOpenedRecordRefList );
        mUnOpenedRecordRefList = NULL;
    }
*/    
    if ( mOurResources )
    {
        delete mOurResources;
        mOurResources = NULL;
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
    valueCallBack.release = NULL;
    valueCallBack.copyDescription = NSLNodeValueCopyDesctriptionCallback;
    valueCallBack.equal = NSLNodeValueEqualCallback;

    mPublishedNodes = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &valueCallBack);
    
	CFArrayCallBacks	callBack;
    
    callBack.version = 0;
    callBack.retain = NULL;
    callBack.release = NULL;
    callBack.copyDescription = NSLQueuedSearchesCopyDesctriptionCallback;
    callBack.equal = NSLQueuedSearchesEqualCallback;
    
    mSearchQueue = ::CFArrayCreateMutable( NULL, 0, &callBack );

    mOurResources = new TCFResources( GetBundleIdentifier() );
    
    pthread_mutex_init( &mQueueLock, NULL );
    pthread_mutex_init( &mPluginLock, NULL );
    pthread_mutex_init( &mSearchQueueLock, NULL );
    
    // use these for the reftable dictionary
    keyCallBack.version = 0;
    keyCallBack.retain = NULL;
    keyCallBack.release = NULL;
    keyCallBack.copyDescription = NULL;
    keyCallBack.equal = NULL;
    keyCallBack.hash = NULL;		// this is fine

    valueCallBack.release = NULL;
    valueCallBack.copyDescription = NULL;
    valueCallBack.equal = NULL;
    
    mOpenRefTable = ::CFDictionaryCreateMutable( NULL, 0, &keyCallBack, &valueCallBack );
	if ( !mOpenRefTable )
		DBGLOG("************* mOpenRefTable is NULL ***************\n");
/*
    mOpenRefTable = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
    
    mUnOpenedRecordRefList = ::CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
*/    
    if ( !siResult )
    {
        // set the initted flags
        mState = kInitalized | kInactive;
    
    // don't start a periodic task until we get activated
//        siResult = NSLSearchTickler();		// get this fired off
    }
    
    if ( !siResult )
        siResult = InitPlugin();		// initialize our subclass

    return siResult;
} // Initialize

void PeriodicTimerCallback( CFRunLoopTimerRef timer, void *info );
void PeriodicTimerCallback( CFRunLoopTimerRef timer, void *info )
{
	CNSLPlugin*		plugin = (CNSLPlugin*)info;
	
	plugin->NSLSearchTickler();
}

void CNSLPlugin::InstallSearchTickler( void )
{
	if ( !mSearchTicklerInstalled && mRunLoopRef )
	{
		DBGLOG( "CNSLPlugin::InstallSearchTickler\n" );
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
		DBGLOG( "CNSLPlugin::UnInstallSearchTickler\n" );
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

	DBGLOG( "CNSLPlugin::NSLSearchTickler, Outstanding Searches: %ld, queued Searches: %ld\n", NumOutstandingSearches(), (mSearchQueue)?CFArrayGetCount(mSearchQueue):-1 );

    if ( mState & kActive && mActivatedByNSL )
    {
		while ( NumOutstandingSearches() < kMaxNumOutstandingSearches && mSearchQueue && CFArrayGetCount(mSearchQueue) > 0 )
			StartNextQueuedSearch();		// just give this a chance to fire one off

		if ( NumOutstandingSearches() == 0 )
			UnInstallSearchTickler();

		if ( mSearchTicklerInstalled )
			CFRunLoopTimerSetNextFireDate( mTimerRef, CFAbsoluteTimeGetCurrent()+kTaskInterval );
    }
    else if ( mSearchTicklerInstalled )
		UnInstallSearchTickler();

    return( siResult );
} // NSLTickler

sInt32 CNSLPlugin::PeriodicTask( void )
{
    sInt32				siResult	= eDSNoErr;

	DBGLOG( "CNSLPlugin::PeriodicTask, Outstanding Searches: %ld, queued Searches: %ld\n", NumOutstandingSearches(), (mSearchQueue)?CFArrayGetCount(mSearchQueue):-1 );

    if ( mState & kActive && mActivatedByNSL )
    {
        if ( IsTimeForNewNeighborhoodLookup() )
        {
            StartNodeLookup();
        }
        
        if ( IsTimeToClearOutStaleNodes() )
        {
            ClearOutStaleNodes();
        }
    }

    return( siResult );
} // PeriodicTask

sInt32 CNSLPlugin::ProcessRequest ( void *inData )
{
	sInt32		siResult	= 0;

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
	else if ( ((mState & kInactive) || !(mState & kActive)) )
	{
		if ( ((sHeader *)inData)->fType == kOpenDirNode )
		{
			// these are ok when we are inactive if this is the top level
			char			   *pathStr				= nil;
			tDataListPtr		pNodeList			= nil;

			pNodeList	=	((sOpenDirNode *)inData)->fInDirNodeName;
			pathStr = dsGetPathFromListPriv( pNodeList, (char *)"/" );
			pathStr++;	// advance past first /
			
			if ( pathStr && GetProtocolPrefixString() && strcmp( pathStr, GetProtocolPrefixString() ) == 0 )
			{
				DBGLOG( "CNSLPlugin::ProcessRequest (kOpenDirNode), plugin not active, open on (%s) ok\n", pathStr );
			}
			else
			{
				DBGLOG( "CNSLPlugin::ProcessRequest (kOpenDirNode), plugin not active, returning ePlugInNotActive on open (%s)\n", pathStr );
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
	DBGLOG( "CNSLPlugin::SetPluginState(%s):", GetProtocolPrefixString() );

	if (kActive & inState) //want to set to active
    {
        DBGLOG( "kActive\n" );
        if ( mState & kInactive )
            mState -= kInactive;
            
        if ( !(mState & kActive) )
            mState += kActive;

#ifdef DONT_WAIT_FOR_NSL_TO_ACTIVATE_US				
		mActivatedByNSL = true;	// for now we are activating ourselves
#endif
		
		if ( mActivatedByNSL )
			StartNodeLookup();		// get this fired off
    }

	if (kInactive & inState) //want to set to in-active
    {
        DBGLOG( "kInactive\n" );
        if ( !(mState & kInactive) )
            mState += kInactive;
            
        if ( mState & kActive )
            mState -= kActive;
        // we need to deregister all our nodes

		if ( mSearchTicklerInstalled )
			UnInstallSearchTickler();
			
        ClearOutAllNodes();
    }

	return( eDSNoErr );

} // SetPluginState

#pragma mark - 
void CNSLPlugin::AddNode( CFStringRef nodeNameRef, Boolean isLocalNode )
{
    if ( !nodeNameRef )
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
    if ( !nodeName )
        return;
        
    NodeData*		node = NULL;
    CFStringRef		nodeRef = CFStringCreateWithCString( NULL, nodeName, kCFStringEncodingUTF8 );
    bool			isADefaultNode = false;
    bool			isADefaultOnlyNode = false;
    
	if ( nodeRef )
	{
		DBGLOG( "CNSLPlugin::AddNode called with %s\n", nodeName );
		LockPublishedNodes();
		if ( ::CFDictionaryContainsKey( mPublishedNodes, nodeRef ) )
			node = (NodeData*)::CFDictionaryGetValue( mPublishedNodes, nodeRef );
	//    UnlockPublishedNodes();
		
		/*
		if ( GetLocalNodeString() && strcmp( nodeName, GetLocalNodeString() ) == 0 )
			isADefaultNode = true;
		*/
		if ( isLocalNode || IsLocalNode( nodeName ) )
			isADefaultNode = true;
		
		if ( isADefaultNode )
			isADefaultOnlyNode = IsADefaultOnlyNode( nodeName );
			
		if ( node && isADefaultNode != node->fIsADefaultNode )
		{
			// this node is being republished and has a different default state.  We will deregister the old one and create a new one.
fprintf( stderr, "CNSLPlugin::AddNode calling DSUnregisterNode (%s) as isADefaultNode:(%d), node->fIsADefaultNode:(%d)\n", nodeName, isADefaultNode, node->fIsADefaultNode );
			DSUnregisterNode( mSignature, node->fDSName );
			
	//        LockPublishedNodes();
			::CFDictionaryRemoveValue( mPublishedNodes, node->fNodeName );		// remove it from the dictionary
	//        UnlockPublishedNodes();
	
			DeallocateNodeData( node );
			
			node = NULL;
		}
		
		if ( node )
		{
			node->fTimeStamp = GetCurrentTime();	// update this node
		}
		else
		{
			// ah, we have a new node
			DBGLOG( "CNSLPlugin::AddNode Adding new node %s\n", nodeName );
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
			
	//        LockPublishedNodes();
			::CFDictionaryAddValue( mPublishedNodes, nodeRef, node );
	//        UnlockPublishedNodes();
		}
		
		CFRelease( nodeRef );
		
		UnlockPublishedNodes();
	}
}

void CNSLPlugin::RemoveNode( CFStringRef nodeNameRef )
{
    NodeData*		node = NULL;
    
    DBGLOG( "CNSLPlugin::RemoveNode called with" );
    if ( getenv("NSLDEBUG") )
        CFShow(nodeNameRef);
        
    LockPublishedNodes();
    Boolean		containsNode = ::CFDictionaryContainsKey( mPublishedNodes, nodeNameRef );
//    UnlockPublishedNodes();

    if ( containsNode )
    {
//        LockPublishedNodes();
        node = (NodeData*)::CFDictionaryGetValue( mPublishedNodes, nodeNameRef );
//        UnlockPublishedNodes();

        DSUnregisterNode( mSignature, node->fDSName );
        
//        LockPublishedNodes();
        ::CFDictionaryRemoveValue( mPublishedNodes, node->fNodeName );		// remove it from the dictionary
//        UnlockPublishedNodes();

        DeallocateNodeData( node );
        node = NULL;
    }

    UnlockPublishedNodes();
}

void CNSLPlugin::NodeLookupComplete( void )
{
	DBGLOG( "CNSLPlugin::NodeLookupComplete\n" );
    SetTimeToClearOutStaleNodes( true );			// get rid of any that are old
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
			siResult = eNotHandledByThisNode;
			break;
			
		case kGetAttributeValue:
            DBGLOG( "CNSLPlugin::HandleRequest, we don't handle kGetAttributeValue yet\n" );
			siResult = eNotHandledByThisNode;
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
			siResult = HandleNetworkTransition( (sHeader*)inData );
			break;

			
		default:
			siResult = eNotHandledByThisNode;
			break;
	}

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
    sInt32				siResult			= eNotHandledByThisNode;	// plugins can override
	
	return siResult;		
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
    mMinTimeBetweenNodeLookups = kMinTimeBetweenNodeLookups;			// reset this if someone is currently looking at us
	
	if ( inData != nil )
    {
        pNodeList	=	inData->fInDirNodeName;
        pathStr = dsGetPathFromListPriv( pNodeList, (char *)"/" );
        protocolStr = pathStr + 1;	// advance past the '/'
        
        DBGLOG( "CNSLPlugin::OpenDirNode, ProtocolPrefixString is %s, pathStr is %s\n", GetProtocolPrefixString(), pathStr );
        
        if ( strstr( pathStr, "NSLActivate" ) )
        {
            // we have a directed open for our plugins, fire off our node searches if needed
            if ( !mActivatedByNSL )
            {
                mActivatedByNSL = true;
                StartNodeLookup();
            }
            
			free( pathStr );
			
            return eDSNoErr;
        }
/*        else if ( (strlen(pathStr) > strlen(GetProtocolPrefixString()+1)) && (::strncmp(protocolStr, GetProtocolPrefixString(), strlen(GetProtocolPrefixString())) == 0) && protocolStr[strlen(GetProtocolPrefixString())] == '/' )
        {
            DBGLOG( "CNSLPlugin::OpenDirNode, weHandleThisNode set to true\n" );
            weHandleThisNode = true;
        }
*/        
		else if ( strcmp( protocolStr, GetProtocolPrefixString() ) == 0 )
		{
			// they are opening this plugin at the top level so they'll probably be wanting to configure it
			nodeName = new char[1+strlen(protocolStr)];
			if ( !nodeName ) throw ( eDSNullNodeName );
				
			::strcpy(nodeName,protocolStr);
			
			if ( nodeName )
			{
				DBGLOG( "CNSLPlugin::OpenDirNode on %s\n", nodeName );
				CNSLDirNodeRep*		newNodeRep = new CNSLDirNodeRep( this, (const void*)inData->fOutNodeRef );
				
				if (!newNodeRep) throw ( eDSNullNodeName );
				
				newNodeRep->Initialize( nodeName, inData->fInUID );
	
				// add the item to the reference table
				::CFDictionaryAddValue( mOpenRefTable, (const void*)inData->fOutNodeRef, (const void*)newNodeRep );
	
				delete( nodeName );
				nodeName = nil;
			}
            
			free( pathStr );
			
			return eDSNoErr;
		}
		else if ( (strlen(pathStr) > strlen(GetProtocolPrefixString()+1)) && (::strncmp(protocolStr, GetProtocolPrefixString(), strlen(GetProtocolPrefixString())) == 0) && protocolStr[strlen(GetProtocolPrefixString())] == '/' )
        {
			// only work on nodes with our prefix
            subStr = pathStr + strlen(GetProtocolPrefixString()) + 2;
			
			CFStringRef		nodeAsRef = ::CFStringCreateWithCString( NULL, subStr, kCFStringEncodingUTF8 );
            
            if ( nodeAsRef )
            {
                LockPublishedNodes();
                weHandleThisNode = ::CFDictionaryContainsKey( mPublishedNodes, nodeAsRef );
                UnlockPublishedNodes();
                
                if ( weHandleThisNode )
                    DBGLOG( "CNSLPlugin::OpenDirNode, we will handle the request since we have this node (%s) already published\n", subStr );
				else
				{
					if ( getenv("NSLDEBUG") )
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
						CFStringRef		nodeRef = CFStringCreateWithCString( NULL, subStr, kCFStringEncodingUTF8 );
						
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
                ThrowThisIfNULL_( nodeName, eDSNullNodeName );
                
                ::strcpy(nodeName,subStr);
            }
            
            if ( nodeName )
            {
                DBGLOG( "CNSLPlugin::OpenDirNode on %s\n", nodeName );
                CNSLDirNodeRep*		newNodeRep = new CNSLDirNodeRep( this, (const void*)inData->fOutNodeRef );
                
                ThrowIfNULL_( newNodeRep );
                
                newNodeRep->Initialize( nodeName, inData->fInUID );

                // add the item to the reference table
                ::CFDictionaryAddValue( mOpenRefTable, (const void*)inData->fOutNodeRef, (const void*)newNodeRep );

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
    else
    	DBGLOG( "CNSLPlugin::OpenDirNode inData is NULL!\n" );
		
	if (pathStr != NULL)
	{
		free(pathStr);
		pathStr = NULL;
	}

    return siResult;
}


sInt32 CNSLPlugin::CloseDirNode ( sCloseDirNode *inData )
{
    sInt32				siResult			= eDSNoErr;
    
	DBGLOG( "CNSLPlugin::CloseDirNode %lx\n", inData->fInNodeRef );
    Try_
    {
        if ( inData != nil && inData->fInNodeRef && mOpenRefTable )
        {
            CNSLDirNodeRep*		nodeRep = NULL;
            
            LockPlugin();
			
			nodeRep = (CNSLDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
			
			DBGLOG( "CNSLPlugin::CloseDirNode, CFDictionaryGetValue returned nodeRep: 0x%x\n", (void*)nodeRep );

            if ( nodeRep )
            {
                ::CFDictionaryRemoveValue( mOpenRefTable, (void*)inData->fInNodeRef );

                UnlockPlugin();

                DBGLOG( "CNSLPlugin::CloseDirNode, delete nodeRep: 0x%x\n", (void*)nodeRep );
                nodeRep->DeleteSelf();
            }
            else
            {
                DBGLOG( "CNSLPlugin::CloseDirNode, nodeRef not found in our list\n" );
                UnlockPlugin();
            }
        }
    }
    
    Catch_( err )
    {
        siResult = err;
        DBGLOG( "CNSLPlugin::CloseDirNode, Caught error:%li\n", siResult );
    }

    return siResult;
}

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
    
	DBGLOG( "CNSLPlugin::GetRecordList called\n" );
//    if( !::CFDictionaryGetValueIfPresent( mOpenRefTable, (const void*)inData->fInNodeRef, &dictionaryResult ) )
	dictionaryResult = ::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
    if( !dictionaryResult )
        DBGLOG( "CNSLPlugin::GetRecordList called but we couldn't find the nodeDirRep!\n" );

    nodeDirRep = (CNSLDirNodeRep*)dictionaryResult;
    
    Try_
    {
        if ( nodeDirRep && !nodeDirRep->IsLookupStarted() )
        {
 DBGLOG( "CNSLPlugin::GetRecordList called, lookup hasn't been started yet.\n" );
           // ok, we need to initialize this nodeDirRep and start the searches...
            // Verify all the parameters
            ThrowThisIfNULL_( inData , eMemoryError );
            ThrowThisIfNULL_( inData->fInDataBuff , eDSEmptyBuffer );
            ThrowThisIf_( (inData->fInDataBuff->fBufferSize == 0), eDSEmptyBuffer );
            ThrowThisIfNULL_( inData->fInRecNameList , eDSEmptyRecordNameList );
            ThrowThisIfNULL_( inData->fInRecTypeList , eDSEmptyRecordTypeList );
            ThrowThisIfNULL_( inData->fInAttribTypeList , eDSEmptyAttributeTypeList );
    
            inData->fOutRecEntryCount	= 0;
			
            //check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pContext
			if (inData->fOutRecEntryCount >= 0)
			{
				nodeDirRep->LimitRecSearch( inData->fOutRecEntryCount );
			}
                   
           // Node context data
    
            // Get the record type list
            cpRecTypeList = new CAttributeList( inData->fInRecTypeList );
    
            ThrowThisIfNULL_( cpRecTypeList , eDSEmptyRecordTypeList );
            //save the number of rec types here to use in separating the buffer data
            countDownRecTypes = cpRecTypeList->GetCount();
    
            ThrowThisIf_( (countDownRecTypes == 0), eDSEmptyRecordTypeList );
    
            sInt32		error = eDSNoErr;
            nodeDirRep->LookupHasStarted();

            for ( uInt16 i=1; i<=countDownRecTypes; i++ )
            {
                if ( (error = cpRecTypeList->GetAttribute( i, &pRecType )) == eDSNoErr )
                {
                    DBGLOG( "CNSLPlugin::GetRecordList, GetAttribute returned pRecType:%s\n", pRecType );
                    
                    pNSLRecType = CreateNSLTypeFromRecType( pRecType );
                    
                    if ( pNSLRecType )
                    {
                        DBGLOG( "CNSLPlugin::GetRecordList, CreateNSLTypeFromRecType returned pNSLRecType:%s\n", pNSLRecType );
                        
                        StartServicesLookup( pNSLRecType, nodeDirRep );
                    
                        free( pNSLRecType );
                        inData->fIOContinueData = (void*)inData->fInNodeRef;	// just to show we have continueing data
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
            inData->fIOContinueData = (void*)inData->fInNodeRef;	// just to show we have continueing data
            DBGLOG( "***Sending Back Results, fBufferSize = %ld, fBufferLength = %ld\n", inData->fInDataBuff->fBufferSize, inData->fInDataBuff->fBufferLength );
        }
        else if ( nodeDirRep && nodeDirRep->LookupComplete() )
        {
			DBGLOG( "CNSLPlugin::GetRecordList called and nodeDirRep->LookupComplete so setting inData->fIOContinueData to NULL\n" );
            inData->fIOContinueData = NULL;
            inData->fOutRecEntryCount = 0;			// no more entries to parse
			nodeDirRep->ResetLookupHasStarted();	// so users can start another search
        }
		else
			usleep( 100000 );		// sleep for a 10th of a second so the client can't spinlock
			
    } // try
    
    Catch_( err )
    {
        siResult = err;
        DBGLOG( "CNSLPlugin::GetRecordList, Caught error:%li\n", siResult );
    }

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

sInt32 CNSLPlugin::OpenRecord ( sOpenRecord *inData )
{
    sInt32					siResult			= eDSRecordNotFound;		// by default we don't Open records
    const void*				dictionaryResult	= NULL;
    CFStringRef		        recordNameRef		= ::CFStringCreateWithCString( kCFAllocatorDefault, (char*)inData->fInRecName->fBufferData, kCFStringEncodingUTF8 );
        
    DBGLOG( "CNSLPlugin::OpenRecord called on refNum:0x%x\n", inData->fOutRecRef );
//    if( !::CFDictionaryGetValueIfPresent( mOpenRefTable, (const void*)inData->fInNodeRef, &dictionaryResult ) )
	dictionaryResult = ::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
    if ( !dictionaryResult )
	{
        DBGLOG( "CNSLPlugin::OpenRecord called but we couldn't find the nodeDirRep!\n" );
        return eDSInvalidNodeRef;
    }
    
    CFMutableStringRef		serviceKeyRef = ::CFStringCreateMutable( NULL, 0 );			// we'll use this as a key
    CFStringRef				nodeNameRef = ((CNSLDirNodeRep*)dictionaryResult)->GetNodeName();

    char*		    		pNSLRecType = CreateNSLTypeFromRecType( (char*)inData->fInRecType->fBufferData );
	CFStringRef				recordTypeRef = ::CFStringCreateWithCString( kCFAllocatorDefault, pNSLRecType, kCFStringEncodingUTF8 );
    
    ::CFStringAppend( serviceKeyRef, recordNameRef );
    ::CFStringAppend( serviceKeyRef, recordTypeRef );
    
    ::CFRelease( recordNameRef );
    ::CFRelease( recordTypeRef );
    free( pNSLRecType );
    
    LockPublishedNodes();
    Boolean		nodeInPublishedNodes = ::CFDictionaryContainsKey( mPublishedNodes, nodeNameRef );
    UnlockPublishedNodes();
    
    if ( nodeInPublishedNodes )
    {
        if ( getenv("NSLDEBUG") )
        {
            DBGLOG( "CNSLPlugin::OpenRecord, found the node in question\n" );
            CFShow( nodeNameRef );
            DBGLOG( "CNSLPlugin::OpenRecord, looking up record\n" );
            CFShow( serviceKeyRef );
        }
        
        LockPublishedNodes();
        NodeData* node = (NodeData*)::CFDictionaryGetValue( mPublishedNodes, nodeNameRef );
        UnlockPublishedNodes();

        CFDictionaryRef	recordRef = (CFDictionaryRef)::CFDictionaryGetValue( node->fServicesRefTable, serviceKeyRef );

        if ( recordRef && CFGetTypeID(recordRef) == CFDictionaryGetTypeID() )
        {
            CFStringRef recordName = (CFStringRef)::CFDictionaryGetValue( recordRef, CFSTR(kDSNAttrRecordName) );
            CFStringRef nodeName = (CFStringRef)::CFDictionaryGetValue( recordRef, CFSTR(kDS1AttrLocation) );
            
            if ( !recordName && !nodeName )
            {
                if ( getenv("NSLDEBUG") )
                {
                    DBGLOG( "CNSLPlugin::OpenRecord, the node->fServicesRefTable doesn't have both recordName and nodeName keys!\n" );
                    if (recordName) CFShow( recordName ); else DBGLOG( "CNSLPlugin::OpenRecord, recordName is null\n" );
                    if (nodeName) CFShow( nodeName ); else DBGLOG( "CNSLPlugin::OpenRecord, nodeName is null\n" );
                }
            }
            else if ( CFGetTypeID(recordName) == CFStringGetTypeID() && CFGetTypeID(nodeName) == CFStringGetTypeID() )
            {
                // found it.
                ::CFDictionaryAddValue( mOpenRefTable, (void*)inData->fOutRecRef, (void*)recordRef );
                siResult = eDSNoErr;
                
                if ( getenv("NSLDEBUG") )
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
    }
    else
    {
        // do we want to allow two openings of a record?  Or is this not possible as we should be getting a unique
        // ref assigned to this open?...
        DBGLOG( "CNSLPlugin::OpenRecord this record already opened...\n" );
        siResult = eDSNoErr;
    }
    
    ::CFRelease( serviceKeyRef );
    
    return( siResult );
}	// OpenRecord

sInt32 CNSLPlugin::CloseRecord ( sCloseRecord *inData )
{
    sInt32					siResult			= eDSNoErr;		// by default we don't Close records

    DBGLOG( "CNSLPlugin::CloseRecord called on ref:%ld\n", inData->fInRecRef );
    
    if ( ::CFDictionaryContainsKey( mOpenRefTable, (const void*)inData->fInRecRef ) )
    {
        ::CFDictionaryRemoveValue( mOpenRefTable, (const void*)inData->fInRecRef );
    }
    else
    {
        DBGLOG( "CNSLPlugin::CloseRecord called but the record wasn't found!\n" );
        return eDSRecordNotFound;
    }
    
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
    
	Try_
	{
//        if( !::CFDictionaryGetValueIfPresent( mOpenRefTable, (const void*)inData->fInNodeRef, &dictionaryResult ) )
		dictionaryResult = ::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
        if ( !dictionaryResult )
		{
            DBGLOG( "CNSLPlugin::CreateRecord called but we couldn't find the nodeDirRep!\n" );
            return eDSInvalidNodeRef;
        }

        serviceKeyRef = ::CFStringCreateMutable( NULL, 0 );			// we'll use this as a key
        
        newService = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

        nodeDirRep = (CNSLDirNodeRep*)dictionaryResult;
        
		pRecType = inData->fInRecType;
		ThrowThisIfNULL_( pRecType, eDSNullRecType );

		pRecName = inData->fInRecName;
		ThrowThisIfNULL_( pRecName, eDSNullRecName );

        pNSLRecType = CreateNSLTypeFromRecType( (char*)pRecType->fBufferData );
		ThrowThisIfNULL_( pNSLRecType, eDSInvalidRecordType );

        if ( pNSLRecType )
        {
            if ( getenv("NSLDEBUG") )
            {
                DBGLOG( "CNSLPlugin::CreateRecord, CreateNSLTypeFromRecType returned pNSLRecType:%s\n", pNSLRecType );
                DBGLOG( "dictionary contents before:\n");
                CFShow( newService );
            }
            
            CFStringRef		keyRef, valueRef;
            
            // add node name
            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, kDS1AttrLocation, kCFStringEncodingUTF8 );
            valueRef = nodeDirRep->GetNodeName();
            
            if ( !CFDictionaryContainsKey( newService, keyRef ) )
                ::CFDictionaryAddValue( newService, keyRef, valueRef );
                
            ::CFRelease( keyRef );
			
            // add record name
            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, kDSNAttrRecordName, kCFStringEncodingUTF8 );
            valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, (char*)pRecName->fBufferData, kCFStringEncodingUTF8 );
            if ( !CFDictionaryContainsKey( newService, keyRef ) )
                ::CFDictionaryAddValue( newService, keyRef, valueRef );

            ::CFStringAppend( serviceKeyRef, valueRef );	// key is made up of name and type
            
            ::CFRelease( keyRef );
            ::CFRelease( valueRef );
            
            // add record type
            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, kDS1AttrServiceType, kCFStringEncodingUTF8 );
            valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, pNSLRecType, kCFStringEncodingUTF8 );

            ::CFDictionarySetValue( newService, keyRef, valueRef );		// set this regardless whether it is already there

            ::CFStringAppend( serviceKeyRef, valueRef );	// key is made up of name and type

            ::CFRelease( keyRef );
            ::CFRelease( valueRef );
            
            if ( getenv("NSLDEBUG") )
            {
                DBGLOG( "dictionary contents after:\n");
                CFShow( newService );

                DBGLOG( "CNSLPlugin::CreateRecord, finished intial creation of opened service dictionary\n" );
                if ( getenv( "NSLDEBUG" ) )
                    ::CFShow( newService );
            }
            
            free( pNSLRecType );
            
            // now we need to add this to our published node's dictionary of services
            LockPublishedNodes();
            if ( ::CFDictionaryContainsKey( mPublishedNodes, nodeDirRep->GetNodeName() ) )
            {
                NodeData* node = (NodeData*)::CFDictionaryGetValue( mPublishedNodes, nodeDirRep->GetNodeName() );
                
                if ( node )
                {
                    ::CFDictionarySetValue( node->fServicesRefTable, serviceKeyRef, newService );
                    
                    if ( getenv( "NSLDEBUG" ) )
                    {
                        DBGLOG( "CNSLPlugin::CreateRecord, adding key to node\n" );
                        CFShow( serviceKeyRef );
                    }
                }
                else
                    DBGLOG( "CNSLPlugin::CreateRecord, couldn't find node in our published nodes!\n" );
            }

            UnlockPublishedNodes();
        }
	}

	Catch_ ( err )
	{
		siResult = err;
	}
		
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
    
    DBGLOG( "CNSLPlugin::DeleteRecord called on refNum:%ld\n", inData->fInRecRef );
    if ( inData->fInRecRef && ::CFDictionaryContainsKey( mOpenRefTable, (const void*)inData->fInRecRef ) )
    {
        CFMutableDictionaryRef serviceToDeregister = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInRecRef );

        if ( serviceToDeregister )
            siResult = DeregisterService( inData->fInRecRef, serviceToDeregister );
        else
            DBGLOG( "CNSLPlugin::DeleteRecord, couldn't find record to delete!\n" );
    }
    else
    {
        siResult = eDSInvalidRecordRef;
        DBGLOG( "CNSLPlugin::DeleteRecord called with invalid fInRecRef.\n" );
    }
    
    return( siResult );
}	// DeleteRecord

sInt32 CNSLPlugin::FlushRecord ( sFlushRecord *inData )
{
    sInt32						siResult = eDSNoErr;

    DBGLOG( "CNSLPlugin::FlushRecord called on refNum:%ld\n", inData->fInRecRef );
	Try_
	{
        if ( inData->fInRecRef && ::CFDictionaryContainsKey( mOpenRefTable, (const void*)inData->fInRecRef ) )
        {
            CFMutableDictionaryRef		serviceToRegister = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInRecRef );

            DBGLOG( "CNSLPlugin::FlushRecord calling RegisterService with the following service:\n" );
            if ( getenv( "NSLDEBUG" ) )
                ::CFShow( serviceToRegister );
                
            if ( serviceToRegister )
                siResult = RegisterService( inData->fInRecRef, serviceToRegister );
            else
                DBGLOG( "CNSLPlugin::FlushRecord, couldn't find service to register!\n" );
//            ::CFRelease( serviceToRegister );
        }
        else
        {
            siResult = eDSInvalidReference;
            DBGLOG( "CNSLPlugin::FlushRecord called with invalid fInRecRef.\n" );
        }
	}

	Catch_ ( err )
	{
		siResult = err;
	}

    return( siResult );
}	// DeleteRecord

sInt32 CNSLPlugin::AddAttributeValue ( sAddAttributeValue *inData )
{
    sInt32						siResult = eDSNoErr;

    DBGLOG( "CNSLPlugin::AddAttributeValue called\n" );
	Try_
	{
        if ( inData->fInRecRef && ::CFDictionaryContainsKey( mOpenRefTable, (const void*)inData->fInRecRef ) )
        {
            CFMutableDictionaryRef		serviceToManipulate = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInRecRef );
			
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
						// ahem, if this is the service type, then we want to ignore as this was already set...
						if ( ::CFStringCompare( (CFStringRef)keyRef, CFSTR(kDS1AttrServiceType), 0 ) != kCFCompareEqualTo )
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
        }
        else
        {
            siResult = eDSInvalidRecordRef;
            DBGLOG( "CNSLPlugin::AddAttributeValue called but with no value in fInRecRef (0x%x) not in our list.\n", (const void*)inData->fInRecRef );
        }
	}

	Catch_ ( err )
	{
		siResult = err;
	}
    
    return( siResult );
}

sInt32 CNSLPlugin::RemoveAttribute ( sRemoveAttribute *inData )
{
    sInt32						siResult = eDSNoErr;

	Try_
	{
        if ( inData->fInRecRef && ::CFDictionaryContainsKey( mOpenRefTable, (const void*)inData->fInRecRef ) )
        {
            CFMutableDictionaryRef		serviceToManipulate = serviceToManipulate;
    
            CFStringRef		keyRef;

            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttribute->fBufferData, kCFStringEncodingUTF8 );

            ::CFDictionaryRemoveValue( serviceToManipulate, keyRef );
            ::CFRelease( keyRef );
        }
        else
        {
            siResult = eDSInvalidRecordRef;
            DBGLOG( "CNSLPlugin::RemoveAttribute called but with no value in fOurRecRef.\n" );
        }
	}

	Catch_ ( err )
	{
		siResult = err;
	}

    return( siResult );
}

sInt32 CNSLPlugin::RemoveAttributeValue ( sRemoveAttributeValue *inData )
{
    sInt32						siResult = eDSNoErr;

	Try_
	{
        if ( inData->fInRecRef && ::CFDictionaryContainsKey( mOpenRefTable, (const void*)inData->fInRecRef ) )
        {
            CFMutableDictionaryRef		serviceToManipulate = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInRecRef );
    
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
        }
        else
        {
            siResult = eDSInvalidRecordRef;
            DBGLOG( "CNSLPlugin::RemoveAttributeValue called but with no value in fOurRecRef.\n" );
        }
	}

	Catch_ ( err )
	{
		siResult = err;
	}

    return( siResult );
}

sInt32 CNSLPlugin::SetAttributeValue ( sSetAttributeValue *inData )
{
    sInt32						siResult = eDSNoErr;

	Try_
	{
        if ( inData->fInRecRef && ::CFDictionaryContainsKey( mOpenRefTable, (const void*)inData->fInRecRef ) )
        {
            CFMutableDictionaryRef		serviceToManipulate = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInRecRef );
    
            CFStringRef			keyRef, valueRef;
            CFMutableArrayRef	attributeArrayRef;

            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
            valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttrValueEntry->fAttributeValueData.fBufferData, kCFStringEncodingUTF8 );

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


            ::CFRelease( keyRef );
            ::CFRelease( valueRef );
        }
        else
        {
            siResult = eDSInvalidRecordRef;
            DBGLOG( "CNSLPlugin::SetAttributeValue called but with no value in fOurRecRef.\n" );
        }
	}

	Catch_ ( err )
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

sInt32 CNSLPlugin::HandleNetworkTransition( sHeader *inData )
{
    sInt32					siResult			= eDSNoErr;
    
    if ( mActivatedByNSL )
    {
        ClearOutAllNodes();			// clear these out
        StartNodeLookup();			// and then start all over
    }
    
    return ( siResult );
}

Boolean CNSLPlugin::ReadOnlyPlugin( void )
{
    return true;		// by default we are read only
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
	CFStringRef		resultRecordNameRef = result->GetAttributeRef( CFSTR(kDSNAttrRecordName) );
	
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
					if ( result->GetAttributeRef( CFSTR(kDSNAttrRecordName) ) && CFStringHasPrefix( result->GetAttributeRef( CFSTR(kDSNAttrRecordType) ), CFSTR(kDSStdRecordTypePrefix) ) )
					{
						resultIsOK = true;
						break;
					}
					else
						continue;
				}
				else if ( pRecName && strcmp( pRecName, kDSRecordsNativeAll ) == 0 )
				{
					if ( result->GetAttributeRef( CFSTR(kDSNAttrRecordName) ) && CFStringHasPrefix( result->GetAttributeRef( CFSTR(kDSNAttrRecordType) ), CFSTR(kDSNativeRecordTypePrefix) ) )
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
					else if ( request->fInPatternMatch == eDSContains )
					{
						CFRange 	result = CFStringFind( resultRecordNameRef, recNameMatchRef, 0 );
						
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
		&&	inData->fInPatternMatch != eDSContains )
	{
		return eNotYetImplemented;
    }
	
	Try_
    {    	
		aRecData = new CDataBuff();
		ThrowThisIfNULL_( aRecData , eMemoryError );
		aAttrData = new CDataBuff();
		ThrowThisIfNULL_( aAttrData , eMemoryError );
		aTempData = new CDataBuff();
		ThrowThisIfNULL_( aTempData , eMemoryError );
        
        // copy the buffer data into a more manageable form
        outBuff = new CBuff();
        ThrowThisIfNULL_( outBuff , eMemoryError );
    
        siResult = outBuff->Initialize( inData->fInDataBuff, true );
        ThrowIfOSErr_( siResult );
    
        siResult = outBuff->GetBuffStatus();
        ThrowIfOSErr_( siResult );
    
        //siResult = outBuff->SetBuffType( kRecordListType );
        siResult = outBuff->SetBuffType( kClientSideParsingBuff );
        ThrowIfOSErr_( siResult );
        
        while ( nodeRep->HaveResults() && !siResult )
        {
            // package the record into the DS format into the buffer
            // steps to add an entry record to the buffer
            const CNSLResult*		newResult = nodeRep->GetNextResult();

			// ok, so first check to see that this result matches the caller's criteria
			if ( !ResultMatchesRequestCriteria( newResult, inData ) )
				continue;		// get next one
				
            aAttrData->Clear();
            aRecData->Clear();
            
            if ( newResult )
            {
                char*		curURL = NULL;
//                CFIndex		curURLLength = 0;
//                CFStringRef	urlRef = NULL;
                char*		curServiceType = NULL;
                CFIndex		curServiceTypeLength = 0;
                CFStringRef	serviceTypeRef = NULL;
                char*		recordName = NULL;
                CFIndex		recordNameLength = 0;
				CFStringRef	recordNameRef = NULL;
                
/*                urlRef = newResult->GetURLRef();
                
                if ( urlRef )
                {
                    curURLLength = ::CFStringGetLength( urlRef )*2;				// CFStringGetLength returns possibly 16 bit values!
                    curURL = (char*)malloc( curURLLength+1 );
                }
*/                
                serviceTypeRef = newResult->GetServiceTypeRef();
                
				if ( !serviceTypeRef )
					continue;			// skip this result
					
				curServiceTypeLength = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(serviceTypeRef), kCFStringEncodingUTF8) +1;
                curServiceType = (char*)malloc( curServiceTypeLength+1 );

                if ( !::CFStringGetCString( serviceTypeRef, curServiceType, curServiceTypeLength+1, kCFStringEncodingUTF8 ) )
                {
                    DBGLOG( "CNSLPlugin::RetrieveResults couldn't convert serviceTypeRef CFString to char* in UTF8!\n" );
                    curServiceType[0] = '\0';
                }
                else
                    DBGLOG( "CNSLPlugin::RetrieveResults curServiceType=%s\n", curServiceType );
                    
//                if ( ::CFStringGetCString( urlRef, curURL, curURLLength+1, kCFStringEncodingUTF8 ) )
                {
                    char* recType = CreateRecTypeFromURL( curServiceType );
                    
                    if ( recType != nil )
                    {
						DBGLOG( "CNSLPlugin::RetrieveResults recType=%s\n", recType );
                        aRecData->AppendShort( ::strlen( recType ) );
                        aRecData->AppendString( recType );
                        free(recType);
                    } // what to do if the recType is nil? - never get here then
                    else
                    {
                        aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
                        aRecData->AppendString( "Record Type Unknown" );
                    }
                    
                    // now the record name
					recordNameRef = newResult->GetAttributeRef( CFSTR(kDSNAttrRecordName) );
                
					if ( !recordNameRef )
					{
						if ( curServiceType )
							free( curServiceType );
						curServiceType = NULL;
						continue;			// skip this result
					}
					
					recordNameLength = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(recordNameRef), kCFStringEncodingUTF8) +1;

					recordName = (char*)malloc( recordNameLength );
	
					if ( !::CFStringGetCString( recordNameRef, recordName, recordNameLength, kCFStringEncodingUTF8 ) )
					{
						DBGLOG( "CNSLPlugin::RetrieveResults couldn't convert recordNameRef CFString to char* in UTF8!\n" );
						recordName[0] = '\0';
					}
					else
						DBGLOG( "CNSLPlugin::RetrieveResults recordName=%s\n", curServiceType );
	
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
                    
                    // now the attributes
                    AttrDataContext		context = {0,aAttrData,inData->fInAttribInfoOnly};
                        

                    if ( newResult->GetAttributeDict() )
                    {
//                        ::CFDictionaryApplyFunction( newResult->GetAttributeDict(), AddToAttrData, &context );
                        // we want to pull out the appropriate attributes that the client is searching for...
                        uInt32					numAttrTypes	= 0;
                        CAttributeList			cpAttributeList( inData->fInAttribTypeList );
                
                        //save the number of rec types here to use in separating the buffer data
                        numAttrTypes = cpAttributeList.GetCount();
                
                        ThrowThisIf_( (numAttrTypes == 0), eDSEmptyRecordTypeList );
                
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
                                    CFStringRef		keyRef = ::CFStringCreateWithCString( NULL, pAttrType, kCFStringEncodingUTF8 );
                                    CFStringRef		valueRef = NULL;
                                    CFTypeRef		valueTypeRef = NULL;
									CFArrayRef		valueArrayRef	= NULL;
									
									aTempData->Clear();
									
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
											valueArrayRef = CFArrayCreateMutable( NULL, 1, &kCFTypeArrayCallBacks );
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
								
											for ( CFIndex i=0; i< CFArrayGetCount(valueArrayRef); i++ )
											{
												valueRef = (CFStringRef)::CFArrayGetValueAtIndex( valueArrayRef, i );
								
												if ( !inData->fInAttribInfoOnly && valueRef && ::CFStringGetLength(valueRef) > 0 )
												{
													valueBuf = (char*)malloc( ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueRef),kCFStringEncodingUTF8) + 1 );	                                            
													if ( ::CFStringGetCString( valueRef, valueBuf, ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueRef),kCFStringEncodingUTF8) + 1, kCFStringEncodingUTF8 ) )	
													{
														// Append attribute value
														aTempData->AppendShort( ::strlen( valueBuf ) );
														aTempData->AppendString( valueBuf );
                                                
														DBGLOG( "CNSLPlugin::RetrieveResults, adding valueBuf: %s\n", valueBuf );
													}
													else
														DBGLOG( "CNSLPlugin::RetrieveResults, CFStringGetCString couldn't create a string for valueRef!\n" );
                                                
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
                }
//                else
//                    DBGLOG( "CNSLPlugin::RetrieveResults couldn't convert CFString to char* in UTF8!\n" );
                
                if ( curURL )
					free( curURL );
                curURL = NULL;
				
				if ( curServiceType )
					free( curServiceType );
				curServiceType = NULL;
				
                if ( recordName )
					free( recordName );
                recordName = NULL;
				
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

    Catch_ ( err )
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

char* CNSLPlugin::CreateNSLTypeFromRecType ( char *inRecType )
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
            }
        }//native maps
        //now deal with the standard mappings
		else if ( ::strncmp( inRecType, kDSStdRecordTypePrefix, uiStdLen ) == 0 )
		{
            DBGLOG( "CNSLPlugin::CreateNSLTypeFromRecType kDSStdRecordTypePrefix, uiStrLen:%ld uiStdLen:%ld\n", uiStrLen, uiStdLen );
            if ( strcmp( inRecType, kDSStdRecordTypeAFPServer ) == 0 )
            {
                outResult = new char[1+::strlen(kAFPServiceType)];
                ::strcpy(outResult, kAFPServiceType);
            }
            else
            if ( strcmp( inRecType, kDSStdRecordTypeSMBServer ) == 0 )
            {
                outResult = new char[1+::strlen(kSMBServiceType)];
                ::strcpy(outResult, kSMBServiceType);
            }
            else
            if ( strcmp( inRecType, kDSStdRecordTypeFTPServer ) == 0 )
            {
                outResult = new char[1+::strlen(kFTPServiceType)];
                ::strcpy(outResult, kFTPServiceType);
            }
            else
            if ( strcmp( inRecType, kDSStdRecordTypeNFS ) == 0 )
            {
                outResult = new char[1+::strlen(kNFSServiceType)];
                ::strcpy(outResult, kNFSServiceType);
            }
            else
            if ( strcmp( inRecType, kDSStdRecordTypeWebServer ) == 0 )
            {
                outResult = new char[1+::strlen(kHTTPServiceType)];
                ::strcpy(outResult, kHTTPServiceType);
            }
            else
            if ( strcmp( inRecType, kDSStdRecordTypePrinters ) == 0 )
            {
                outResult = new char[1+::strlen(kLaserWriterServiceType)];
                ::strcpy(outResult, kLaserWriterServiceType);
            }
            else if ( uiStrLen > uiStdLen )
            {
                uiStrLen = uiStrLen - uiStdLen;
                outResult = new char[ uiStrLen + 2 ];
                ::strcpy( outResult, inRecType + uiStdLen );
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

char* CNSLPlugin::CreateRecTypeFromURL( char *inNSLType )
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
    
    if ( inNSLType != nil )
    {
        uiStrLen = ::strlen( inNSLType );

        if ( strcmp( inNSLType, kAFPServiceType ) == 0 )
        {
            outResult = new char[1+::strlen(kDSStdRecordTypeAFPServer)];
            ::strcpy(outResult,kDSStdRecordTypeAFPServer);
        }
        else if ( strcmp( inNSLType, kSMBServiceType ) == 0 )
        {
            outResult = new char[1+::strlen(kDSStdRecordTypeSMBServer)];
            ::strcpy(outResult,kDSStdRecordTypeSMBServer);
        }
        else if ( strcmp( inNSLType, kNFSServiceType ) == 0 )
        {
            outResult = new char[1+::strlen(kDSStdRecordTypeNFS)];
            ::strcpy(outResult,kDSStdRecordTypeNFS);
        }
        else if ( strcmp( inNSLType, kFTPServiceType ) == 0 )
        {
            outResult = new char[1+::strlen(kDSStdRecordTypeFTPServer)];
            ::strcpy(outResult,kDSStdRecordTypeFTPServer);
        }
        else if ( strcmp( inNSLType, kHTTPServiceType ) == 0 )
        {
            outResult = new char[1+::strlen(kDSStdRecordTypeWebServer)];
            ::strcpy(outResult,kDSStdRecordTypeWebServer);
        }
        else
        {
            // native record type
            outResult = new char[1+::strlen(kDSNativeAttrTypePrefix)+strlen(inNSLType)];
            ::strcpy(outResult,kDSNativeAttrTypePrefix);
            ::strcat(outResult,inNSLType);
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
			CFStringAppend( outResultRef, CFSTR(kDSStdRecordTypeAFPServer) );
		}
		else if ( ::strcmp( inNativeType, kSMBServiceType ) == 0 )
		{
			CFStringAppend( outResultRef, CFSTR(kDSStdRecordTypeSMBServer) );
		}
		else if ( ::strcmp( inNativeType, kNFSServiceType ) == 0 )
		{
			CFStringAppend( outResultRef, CFSTR(kDSStdRecordTypeNFS) );
		}
		else if ( ::strcmp( inNativeType, kDSStdRecordTypeFTPServer ) == 0 )
		{
			CFStringAppend( outResultRef, CFSTR(kDSStdRecordTypeFTPServer) );
		}
		else if ( ::strcmp( inNativeType, kDSStdRecordTypeWebServer ) == 0 )
		{
			CFStringAppend( outResultRef, CFSTR(kDSStdRecordTypeWebServer) );
		}
		else if ( outResultRef )
		{
			CFStringAppend( outResultRef, CFSTR(kDSNativeRecordTypePrefix) );
			
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
Boolean CNSLPlugin::IsTimeForNewNeighborhoodLookup( void )
{
	DBGLOG( "CNSLPlugin::IsTimeForNewNeighborhoodLookup, mLastNodeLookupStartTime=%li, GetCurrentTime()=%li\n",mLastNodeLookupStartTime, GetCurrentTime() );
    return ( mLastNodeLookupStartTime + mMinTimeBetweenNodeLookups < GetCurrentTime() );
}

void CNSLPlugin::StartNodeLookup( void )
{
    mLastNodeLookupStartTime = GetCurrentTime();		// get current time
    DBGLOG( "CNSLPlugin::StartNodeLookup called\n" );
    
	if ( mMinTimeBetweenNodeLookups < kMaxTimeBetweenNodeLookups )	// double the back off time
		mMinTimeBetweenNodeLookups *= 2;
		
    NewNodeLookup();
}

sInt16 CNSLPlugin::NumOutstandingSearches( void )
{
    return gOutstandingSearches;
}

Boolean	 CNSLPlugin::OKToStartNewSearch( void )
{
    pthread_mutex_lock(&gOutstandingSearchesLock);

    Boolean		okToStartNewSearch = NumOutstandingSearches() < kMaxNumOutstandingSearches;

    pthread_mutex_unlock(&gOutstandingSearchesLock);

    if ( okToStartNewSearch )
        DBGLOG( "CNSLPlugin::OKToStartNewSearch is returning true (%d current searches)\n", gOutstandingSearches );
    else
        DBGLOG( "CNSLPlugin::OKToStartNewSearch is returning false (%d current searches)\n", gOutstandingSearches );

    return okToStartNewSearch; 
}

void CNSLPlugin::StartSubNodeLookup( char* parentNodeName )
{
    DBGLOG( "CNSLPlugin::StartSubNodeLookup called on %s\n", parentNodeName );
    
    OKToOpenUnPublishedNode( parentNodeName );
}

void CNSLPlugin::ClearOutStaleNodes( void )
{
	DBGLOG( "CNSLPlugin::ClearOutStaleNodes\n" );
    // we want to look at each node we have registered and unregister any whose timestamp is older than
    // the start of our last node lookup search
    NSLNodeHandlerContext	context = {mPublishedNodes, kClearOutStaleNodes, (void*)GetLastNodeLookupStartTime()};
    
    LockPublishedNodes();
    ::CFDictionaryApplyFunction( mPublishedNodes, NSLNodeHandlerFunction, &context );
    
    SetTimeToClearOutStaleNodes( false );
    UnlockPublishedNodes();
}


void CNSLPlugin::ClearOutAllNodes( void )
{
	DBGLOG( "CNSLPlugin::ClearOutAllNodes\n" );
    // we want to look at each node we have registered and unregister any whose timestamp is older than
    // the start of our last node lookup search
    NSLNodeHandlerContext	context = {mPublishedNodes, kClearOutAllNodes, (void*)GetLastNodeLookupStartTime()};
    
    LockPublishedNodes();
    ::CFDictionaryApplyFunction( mPublishedNodes, NSLNodeHandlerFunction, &context );
    
    // may as well clear this since there will not be any nodes, let alone stale ones.
    SetTimeToClearOutStaleNodes( false );
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
    LockSearchQueue();
    if ( mSearchQueue && ::CFArrayGetCount(mSearchQueue) > 0 )
    {
#if DEQUEUE_LIFO
        CNSLServiceLookupThread*	newLookup = (CNSLServiceLookupThread*)::CFArrayGetValueAtIndex( mSearchQueue, CFArrayGetCount(mSearchQueue)-1 );
        ::CFArrayRemoveValueAtIndex( mSearchQueue, CFArrayGetCount(mSearchQueue)-1 );
#else
        CNSLServiceLookupThread*	newLookup = (CNSLServiceLookupThread*)::CFArrayGetValueAtIndex( mSearchQueue, 0 );
        ::CFArrayRemoveValueAtIndex( mSearchQueue, 0 );
#endif        
        if ( ::CFDictionaryContainsValue( mOpenRefTable, newLookup->GetNodeToSearch() ) && !newLookup->AreWeCanceled() )
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
    UnlockSearchQueue();
}

void CNSLPlugin::StartServicesLookup( char* serviceType, CNSLDirNodeRep* nodeDirRep )
{
    if ( serviceType )
        NewServiceLookup( serviceType, nodeDirRep );
}

#pragma mark -


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
                DBGLOG( "NSLNodeHandlerFunction, Removing Node from published list\n" );
                DSUnregisterNode( curNodeData->fSignature, curNodeData->fDSName );
                ::CFDictionaryRemoveValue( context->fDictionary, curNodeData->fNodeName );		// remove it from the dictionary
				DeallocateNodeData( curNodeData );
            }
        }    
        break;
        
        case kClearOutAllNodes:
            DSUnregisterNode( curNodeData->fSignature, curNodeData->fDSName );
            ::CFDictionaryRemoveValue( context->fDictionary, curNodeData->fNodeName );		// remove it from the dictionary
            DeallocateNodeData( curNodeData );
            break;
			
        default:
            break;
    };
}


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
    char*				valueBuf	= NULL;
	CDataBuff*			aTmpData	= nil;
    AttrDataContext*	dataContext	= (AttrDataContext*)context;
    
    dataContext->count++;		// adding one more
    
    aTmpData = new CDataBuff();
    ThrowThisIfNULL_( aTmpData , eMemoryError );
    
    aTmpData->Clear();
    
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
			valueArrayRef = CFArrayCreateMutable( NULL, 1, &kCFTypeArrayCallBacks );
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

			for ( CFIndex i=0; i< CFArrayGetCount(valueArrayRef); i++ )
			{
				valueRef = (CFStringRef)::CFArrayGetValueAtIndex( valueArrayRef, i );

				if ( !dataContext->attrOnly && ::CFStringGetLength(valueRef) > 0 )
				{
					valueBuf = (char*)malloc( ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueRef), kCFStringEncodingUTF8) + 1 );	// GetLength returns 16 bit value...
					
					if ( ::CFStringGetCString( valueRef, valueBuf, ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueRef), kCFStringEncodingUTF8) + 1, kCFStringEncodingUTF8 ) )	
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
						
					delete( valueBuf );
				}
			}
				
			// Add the attribute length
			dataContext->attrDataBuf->AppendShort( aTmpData->GetLength() );
			dataContext->attrDataBuf->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
	
			aTmpData->Clear();
			
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
		return noErr;						// no data to log, just go back to Jamaica ;)
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
        DBGLOG( "LogHexDump return memFullErr\n" );
        return(memFullErr);
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
	*charPtr = '\0';					// and a terminating NUL
	
	DBGLOG( "%s\n",buf);
	free(buf);
	return noErr;
	
}

CFStringEncoding	gsEncoding = kCFStringEncodingInvalidId;

CFStringEncoding NSLGetSystemEncoding( void )
{
	if ( gsEncoding == kCFStringEncodingInvalidId )
	{
		// need to parse out the encoding from /var/root/.CFUserTextEncoding
		FILE *				fp;
		char 				buf[1024];
		CFStringEncoding	encoding = 0;
		
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
		}
		
		fclose(fp);
		
		gsEncoding = encoding;
	}
	
	DBGLOG( "NSLGetSystemEncoding: returning encoding (%ld)", gsEncoding );
	
	return gsEncoding;
}


