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
 *  @header NISPlugin
 */

#include "NISHeaders.h"
#include "CommandLineUtilities.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Authorization.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <rpc/types.h>

#include "ffparser.h"

#ifndef kServerRunLoop
#define kServerRunLoop (eDSPluginCalls)(kHandleNetworkTransition + 1)
#endif

#define kTaskInterval	2
#define kMaxSizeOfParam 1024

static const	uInt32	kBuffPad	= 16;

int LogHexDump(char *pktPtr, long pktLen);

boolean_t NetworkChangeCallBack(SCDynamicStoreRef session, void *callback_argument);

sInt32 CleanContextData ( sNISContextData *inContext );
sNISContextData* MakeContextData ( void );

// These are the CFContainer callback protos
CFStringRef NSLNodeValueCopyDesctriptionCallback ( const void *item );
Boolean NSLNodeValueEqualCallback ( const void *value1, const void *value2 );
void NSLNodeHandlerFunction(const void *inKey, const void *inValue, void *inContext);

typedef struct AttrDataContext {
    sInt32		count;
    CDataBuff*	attrDataBuf;
    Boolean		attrOnly;
	char*		attrType;
} AttrDataContext;

typedef struct AttrDataMatchContext {
    tDirPatternMatch	fInPattMatchType;
	CFStringRef			fInPatt2MatchRef;
    Boolean				foundAMatch;
} AttrDataMatchContext;

typedef struct ResultMatchContext {
    tDirPatternMatch	fInPattMatchType;
	CFStringRef			fInPatt2MatchRef;
    CFMutableArrayRef	fResultArray;
} ResultMatchContext;

			

Boolean IsResultOK( tDirPatternMatch patternMatch, CFStringRef resultRecordNameRef, CFStringRef recNameMatchRef );
void AddDictionaryDataToAttrData(const void *key, const void *value, void *context);
void FindAttributeMatch(const void *key, const void *value, void *context);
void FindAllAttributeMatches(const void *key, const void *value, void *context);

pthread_mutex_t	gOutstandingSearchesLock = PTHREAD_MUTEX_INITIALIZER;

#define kNumLookupTypes			6
#define kMaxNumAgents			6
static const char* kLookupdRecordNames[kNumLookupTypes] =	{	"lookupd",
																"hosts",
																"services",
																"protocols",
																"rpcs",
																"networks"
															};

static const char* kLookupdRecordLookupOrders[kNumLookupTypes][kMaxNumAgents+1] =	{	{"CacheAgent", "NIAgent", "DSAgent", "NISAgent", NULL},
																		{"CacheAgent", "FFAgent", "DNSAgent", "NIAgent", "DSAgent", "NISAgent", NULL},
																		{"CacheAgent", "FFAgent", "NIAgent", "DSAgent", "NISAgent", NULL},
																		{"CacheAgent", "FFAgent", "NIAgent", "DSAgent", "NISAgent", NULL},
																		{"CacheAgent", "FFAgent", "NIAgent", "DSAgent", "NISAgent", NULL},
																		{"CacheAgent", "FFAgent", "NIAgent", "DSAgent", "NISAgent", NULL}
																	};

#pragma mark -
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


const CFStringRef	gBundleIdentifier = CFSTR("com.apple.DirectoryService.NIS");
const char*			gProtocolPrefixString = "NIS";

#pragma warning "Need to get our default Node String from our resource"

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0xCE, 0x63, 0xBE, 0x32, 0xF5, 0xB6, 0x11, 0xD6, \
								0x9D, 0xC0, 0x00, 0x03, 0x93, 0x4F, 0xB0, 0x10 );
}

static CDSServerModule* _Creator ( void )
{
	DBGLOG( "Creating new NIS Plugin\n" );
    return( new NISPlugin );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;


NISPlugin::NISPlugin( void )
{
	DBGLOG( "NISPlugin::NISPlugin\n" );
	mMappingTable = NULL;
	mLocalNodeString = NULL;
	mMetaNodeLocationRef = NULL;
	mPublishedNodes = NULL;
	mOpenRecordsRef = NULL;
    mOpenRefTable = NULL;
	
	mLastTimeCacheReset = 0;
	mWeLaunchedYPBind = false;
    mLookupdIsAlreadyConfigured = false;
	
	mState		= kUnknownState;
	
	mCachedMapsRef = NULL;
} // NISPlugin


NISPlugin::~NISPlugin( void )
{
	DBGLOG( "NISPlugin::~NISPlugin\n" );
	
	if ( mOpenRecordsRef )
		CFRelease( mOpenRecordsRef );
	mOpenRecordsRef = NULL;
	
    if ( mLocalNodeString )
        free( mLocalNodeString );    
    mLocalNodeString = NULL;
	
	if ( mMetaNodeLocationRef )
		CFRelease( mMetaNodeLocationRef );
	mMetaNodeLocationRef = NULL;
	
	if ( mMappingTable )
		CFRelease( mMappingTable );
	mMappingTable = NULL;
	
    if ( mOpenRefTable )
    {
        ::CFDictionaryRemoveAllValues( mOpenRefTable );
        ::CFRelease( mOpenRefTable );
        mOpenRefTable = NULL;
    }

} // ~NISPlugin

// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

sInt32 NISPlugin::Validate ( const char *inVersionStr, const uInt32 inSignature )
{
	mSignature = inSignature;

	return( noErr );

} // Validate


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

sInt32 NISPlugin::Initialize( void )
{
    sInt32				siResult	= eDSNoErr;

	DBGLOG( "NISPlugin::Initialize\n" );
	// database initialization
    CFDictionaryKeyCallBacks	keyCallBack;
    CFDictionaryValueCallBacks	valueCallBack;
    
    valueCallBack.version = 0;
    valueCallBack.retain = NULL;
    valueCallBack.release = NULL;
    valueCallBack.copyDescription = NSLNodeValueCopyDesctriptionCallback;
    valueCallBack.equal = NSLNodeValueEqualCallback;

    mPublishedNodes = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &valueCallBack);
    

    pthread_mutex_init( &mQueueLock, NULL );
    pthread_mutex_init( &mPluginLock, NULL );
    pthread_mutex_init( &mMapCache, NULL );
	
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
    
    mOpenRefTable = ::CFDictionaryCreateMutable( NULL, 0, &keyCallBack, &valueCallBack );
	if ( !mOpenRefTable )
		DBGLOG("************* mOpenRefTable is NULL ***************\n");

	if ( !mOpenRecordsRef )
		mOpenRecordsRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	ResetMapCache();
		
	siResult = ReadInNISMappings();

    if ( !siResult )
    {
        // set the init flags
        mState = kInitalized | kInactive;
    
    // don't start a periodic task until we get activated
    }
    else
		mState = kFailedToInit;

    return siResult;
} // Initialize

sInt32 NISPlugin::GetNISConfiguration( void )
{
	// Are we configured to use NIS?
	sInt32		siResult = eDSNoErr;
		
	// first check to see if our domainname is set
    char*		resultPtr = CopyResultOfLookup( kNISdomainname );
	
	DBGLOG( "NISPlugin::GetNISConfiguration called\n" );
	
	if ( resultPtr )
	{
		if ( strcmp( resultPtr, "\n" ) == NULL )
		{
			DBGLOG( "NISPlugin::GetNISConfiguration no domain name set, we are not configured to run\n" );
		}
		else
		{
			char*	 eoln	= strstr( resultPtr, "\n" );
			if ( eoln )
				*eoln = '\0';
			
			if ( strcmp( resultPtr, kNoDomainName ) == 0 )
			{
				DBGLOG( "NISPlugin::GetNISConfiguration domain name is: %s, don't use.\n", resultPtr );
			}
			else
			{
				DBGLOG( "NISPlugin::GetNISConfiguration domain name is: %s\n", resultPtr );
				if ( mLocalNodeString )
					free( mLocalNodeString );
					
				mLocalNodeString = (char*)malloc(strlen(resultPtr)+1);
				strcpy( mLocalNodeString, resultPtr );
			}
		}
		
		free( resultPtr );
		resultPtr = NULL;
	}
	else
		DBGLOG( "NISPlugin::GetNISConfiguration resultPtr is NULL!\n" );
	
	if ( mLocalNodeString )
	{
		// now we should check to see if portmap is running
		resultPtr = CopyResultOfLookup( kNISrpcinfo );
		
		if ( resultPtr )
		{
			if ( strstr( resultPtr, "error" ) )
			{
				DBGLOG( "NISPlugin::GetNISConfiguration portmapper not running, we'll try starting it\n" );
			
				free( resultPtr );
				resultPtr = NULL;
				
				resultPtr = CopyResultOfLookup( kNISportmap );

				if ( resultPtr )
				{
					free( resultPtr );
					resultPtr = NULL;
				}
			}
			else
			{
				DBGLOG( "NISPlugin::GetNISConfiguration portmap running\n" );
			}
		}
		else
			DBGLOG( "NISPlugin::GetNISConfiguration resultPtr is NULL!\n" );

		if ( resultPtr )
		{
			free( resultPtr );
			resultPtr = NULL;
		}
		
/*		resultPtr = CopyResultOfLookup( kNISypwhich );			
		
		if ( resultPtr && strlen(resultPtr) > 1 )
		{
			char*	 eoln	= strstr( resultPtr, "\n" );
			if ( eoln )
				*eoln = '\0';
				
			DBGLOG( "NISPlugin::GetNISConfiguration, we are bound to NIS server: %s\n", resultPtr );
		}
*/
	}
			
	if ( resultPtr )
	{
		free( resultPtr );
		resultPtr = NULL;
	}
					
	return siResult;
}

sInt32 NISPlugin::ReadInNISMappings( void )
{
	sInt32		siResult = eDSNoErr;
	
	// Load our table to map NIS maps to DS Types
	if ( !mMappingTable )
	{
		CFStringRef		keys[14]	=	{
											CFSTR(kDSNAttrBootParams),
											CFSTR(kDSNAttrRecordAlias),
											CFSTR(kDSStdRecordTypeBootp),
											CFSTR(kDSStdRecordTypeEthernets),
											CFSTR(kDSStdRecordTypeGroups),
											CFSTR(kDSStdRecordTypeHosts),
											CFSTR(kDSStdRecordTypeMounts),
											CFSTR(kDSStdRecordTypeNetGroups),
											CFSTR(kDSStdRecordTypeNetworks),
											CFSTR(kDSStdRecordTypePrintService),
											CFSTR(kDSStdRecordTypeProtocols),
											CFSTR(kDSStdRecordTypeRPC),
											CFSTR(kDSStdRecordTypeServices),
											CFSTR(kDSStdRecordTypeUsers)
										};

		CFStringRef		values[14]	=	{
											CFSTR("bootparams.byname"),
											CFSTR("mail.aliases"),
											CFSTR("bootptab.byaddr"),
											CFSTR("ethers.byname"),
											CFSTR("group.byname"),
											CFSTR("hosts.byname"),
											CFSTR("mounts.byname"),
											CFSTR("netgroups"),
											CFSTR("networks.byname"),
											CFSTR("printcap.byname"),
											CFSTR("protocols.byname"),
											CFSTR("rpc.byname"),
											CFSTR("services.byname"),
											CFSTR("passwd.byname")
										};

		mMappingTable = CFDictionaryCreate( NULL, (const void**)keys, (const void**)values, 14, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	}
	
	DBGLOG( "NISPlugin::ReadInNISMappings, mMappingTable:\n" );
	
	return siResult;
}

sInt32 NISPlugin::PeriodicTask( void )
{
    sInt32				siResult	= eDSNoErr;

	if ( mLastTimeCacheReset + kMaxTimeForMapCacheToLive < CFAbsoluteTimeGetCurrent() )
		ResetMapCache();
		
    return( siResult );
} // PeriodicTask

sInt32 NISPlugin::ProcessRequest ( void *inData )
{
	sInt32		siResult	= 0;

	if ( inData == nil )
	{
        DBGLOG( "NISPlugin::ProcessRequest, inData is NULL!\n" );
		return( ePlugInDataError );
	}
    
	if ( (mState & kFailedToInit) )
	{
        DBGLOG( "NISPlugin::ProcessRequest, kFailedToInit!\n" );
        return( ePlugInFailedToInitialize );
	}
	
	if ( ((sHeader *)inData)->fType == kServerRunLoop )
	{
        DBGLOG( "NISPlugin::ProcessRequest, received a RunLoopRef, 0x%x\n", (int)((sHeader *)inData)->fContextData );
		return siResult;
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
				DBGLOG( "NISPlugin::ProcessRequest (kOpenDirNode), plugin not active, open on (%s) ok\n", pathStr );

				if (pathStr != NULL)
				{
					free(pathStr);
					pathStr = NULL;
				}
			}
			else
			{
				DBGLOG( "NISPlugin::ProcessRequest (kOpenDirNode), plugin not active, returning ePlugInNotActive on open (%s)\n", pathStr );

				if (pathStr != NULL)
				{
					free(pathStr);
					pathStr = NULL;
				}

				return( ePlugInNotActive );
			}
		}
		else if ( ((sHeader *)inData)->fType == kCloseDirNode )
		{
			DBGLOG( "NISPlugin::ProcessRequest (kCloseDirNode), plugin not active, returning noErr\n" );
		}
		else if ( ((sHeader *)inData)->fType != kDoPlugInCustomCall )
		{
			DBGLOG( "NISPlugin::ProcessRequest (%ld), plugin not active!\n", ((sHeader *)inData)->fType );
			return( ePlugInNotActive );
		}
    }
	
	siResult = HandleRequest( inData );

	return( siResult );

} // ProcessRequest

// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

sInt32 NISPlugin::SetPluginState ( const uInt32 inState )
{
// don't allow any changes other than active / in-active
	sInt32		siResult	= 0;

	DBGLOG( "NISPlugin::SetPluginState(%s):", GetProtocolPrefixString() );

	if (kActive & inState) //want to set to active
    {
        DBGLOG( "kActive\n" );
        if ( mState & kInactive )
            mState -= kInactive;
            
        if ( !(mState & kActive) )
            mState += kActive;

		ResetMapCache();
		siResult = GetNISConfiguration();
		
		if ( siResult == eDSNoErr && mLocalNodeString )
		{
			AddNode( mLocalNodeString );	// all is well, publish our node
		}
    }

	if (kInactive & inState) //want to set to in-active
    {
        DBGLOG( "kInactive\n" );
        if ( !(mState & kInactive) )
            mState += kInactive;
            
        if ( mState & kActive )
            mState -= kActive;
        // we need to deregister all our nodes

		ResetMapCache();

		if ( mLocalNodeString )
		{
			CFStringRef		localNodeStringRef = CFStringCreateWithCString( NULL, mLocalNodeString, kCFStringEncodingUTF8 );
			
			RemoveNode( localNodeStringRef );
			CFRelease( localNodeStringRef );
			
			free(mLocalNodeString);
			mLocalNodeString = NULL;
		}
    }

	return( siResult );

} // SetPluginState

#pragma mark

void NISPlugin::AddNode( const char* nodeName, Boolean isLocalNode )
{
    if ( !nodeName )
        return;
        
    NodeData*		node = NULL;
    CFStringRef		nodeRef = CFStringCreateWithCString( NULL, nodeName, kCFStringEncodingUTF8 );
    
	if ( nodeRef )
	{
		DBGLOG( "NISPlugin::AddNode called with %s\n", nodeName );
		LockPublishedNodes();
		if ( ::CFDictionaryContainsKey( mPublishedNodes, nodeRef ) )
			node = (NodeData*)::CFDictionaryGetValue( mPublishedNodes, nodeRef );

		if ( node )
		{
			// this node is being republished and has a different default state.  We will deregister the old one and create a new one.
			DSUnregisterNode( mSignature, node->fDSName );
			
			::CFDictionaryRemoveValue( mPublishedNodes, node->fNodeName );		// remove it from the dictionary
	
			DeallocateNodeData( node );
			
			node = NULL;
		}
		
		if ( node )
		{
			node->fTimeStamp = GetCurrentTime();	// update this node
		}
		else
		{
			// we have a new node
			DBGLOG( "NISPlugin::AddNode Adding new node %s\n", nodeName );
			node = AllocateNodeData();
			
			node->fNodeName = nodeRef;
			CFRetain( node->fNodeName );
			
			node->fDSName = dsBuildListFromStringsPriv(GetProtocolPrefixString(), nodeName, nil);
	
			node->fTimeStamp = GetCurrentTime();
			
			node->fServicesRefTable = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			
			node->fSignature = mSignature;
			
			if ( node->fDSName )
			{
				DSRegisterNode( mSignature, node->fDSName, kDirNodeType );
			}
			
			::CFDictionaryAddValue( mPublishedNodes, nodeRef, node );
		}
		
		CFRelease( nodeRef );
		
		if ( mMetaNodeLocationRef )	
			CFRelease( mMetaNodeLocationRef );
			
		mMetaNodeLocationRef = CFStringCreateMutable( NULL, 0 );
		CFStringAppendCString( mMetaNodeLocationRef, kProtocolPrefixSlashStr, kCFStringEncodingUTF8 );
		CFStringAppendCString( mMetaNodeLocationRef, nodeName, kCFStringEncodingUTF8 );
		
		UnlockPublishedNodes();
	}
}

void NISPlugin::RemoveNode( CFStringRef nodeNameRef )
{
    NodeData*		node = NULL;
    
    DBGLOG( "NISPlugin::RemoveNode called with" );
    if ( getenv("NSLDEBUG") )
        CFShow(nodeNameRef);
        
    LockPublishedNodes();
    Boolean		containsNode = ::CFDictionaryContainsKey( mPublishedNodes, nodeNameRef );

    if ( containsNode )
    {
        node = (NodeData*)::CFDictionaryGetValue( mPublishedNodes, nodeNameRef );

        DSUnregisterNode( mSignature, node->fDSName );
        
        ::CFDictionaryRemoveValue( mPublishedNodes, node->fNodeName );		// remove it from the dictionary

        DeallocateNodeData( node );
        node = NULL;
    }

    UnlockPublishedNodes();
}

const char*	NISPlugin::GetProtocolPrefixString( void )
{		
    return gProtocolPrefixString;
}

#pragma mark -
// ---------------------------------------------------------------------------
//	* HandleRequest
//
// ---------------------------------------------------------------------------

sInt32 NISPlugin::HandleRequest( void *inData )
{
	sInt32	siResult	= 0;
	sHeader	*pMsgHdr	= nil;

	if ( inData == nil )
	{
		return( -8088 );
	}

	try
	{
		pMsgHdr = (sHeader *)inData;
	
		switch ( pMsgHdr->fType )
		{
			case kOpenDirNode:
				DBGLOG( "NISPlugin::HandleRequest, type: kOpenDirNode\n" );
				siResult = OpenDirNode( (sOpenDirNode *)inData );
				break;
				
			case kCloseDirNode:
				DBGLOG( "NISPlugin::HandleRequest, type: kCloseDirNode\n" );
				siResult = CloseDirNode( (sCloseDirNode *)inData );
				break;
				
			case kGetDirNodeInfo:
				DBGLOG( "NISPlugin::HandleRequest, type: kGetDirNodeInfo\n" );
				siResult = GetDirNodeInfo( (sGetDirNodeInfo *)inData );
				break;
				
			case kGetRecordList:
				DBGLOG( "NISPlugin::HandleRequest, type: kGetRecordList\n" );
				siResult = GetRecordList( (sGetRecordList *)inData );
				break;
				
			case kReleaseContinueData:
				DBGLOG( "NISPlugin::HandleRequest, type: kReleaseContinueData\n" );

				siResult = ReleaseContinueData( (sNISContinueData*)((sReleaseContinueData*)inData)->fInContinueData );
				((sReleaseContinueData*)inData)->fInContinueData = NULL;
				
				break;
				
			case kGetRecordEntry:
				DBGLOG( "NISPlugin::HandleRequest, we don't handle kGetRecordEntry yet\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kGetAttributeEntry:
				DBGLOG( "NISPlugin::HandleRequest, type: kGetAttributeEntry\n" );
				siResult = GetAttributeEntry( (sGetAttributeEntry *)inData );
				break;
				
			case kGetAttributeValue:
				DBGLOG( "NISPlugin::HandleRequest, type: kGetAttributeValue\n" );
			siResult = GetAttributeValue( (sGetAttributeValue *)inData );
				break;
				
			case kOpenRecord:
				DBGLOG( "NISPlugin::HandleRequest, type: kOpenRecord\n" );
				siResult = OpenRecord( (sOpenRecord *)inData );
				break;
				
			case kGetRecordReferenceInfo:
				DBGLOG( "NISPlugin::HandleRequest, we don't handle kGetRecordReferenceInfo yet\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kGetRecordAttributeInfo:
				DBGLOG( "NISPlugin::HandleRequest, we don't handle kGetRecordAttributeInfo yet\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kGetRecordAttributeValueByID:
				DBGLOG( "NISPlugin::HandleRequest, we don't handle kGetRecordAttributeValueByID yet\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kGetRecordAttributeValueByIndex:
				DBGLOG( "NISPlugin::HandleRequest, type: kGetRecordAttributeValueByIndex\n" );
				siResult = GetRecordAttributeValueByIndex( (sGetRecordAttributeValueByIndex *)inData );
				break;
				
			case kFlushRecord:
				DBGLOG( "NISPlugin::HandleRequest, type: kFlushRecord\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kCloseAttributeList:
				DBGLOG( "NISPlugin::HandleRequest, type: kCloseAttributeList\n" );
				siResult = eDSNoErr;
				break;
	
			case kCloseAttributeValueList:
				DBGLOG( "NISPlugin::HandleRequest, we don't handle kCloseAttributeValueList yet\n" );
				siResult = eNotHandledByThisNode;
				break;
	
			case kCloseRecord:
				DBGLOG( "NISPlugin::HandleRequest, type: kCloseRecord\n" );
				siResult = CloseRecord( (sCloseRecord*)inData );
				break;
				
			case kSetRecordName:
				DBGLOG( "NISPlugin::HandleRequest, we don't handle kSetRecordName yet\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kSetRecordType:
				DBGLOG( "NISPlugin::HandleRequest, we don't handle kSetRecordType yet\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kDeleteRecord:
				siResult = eNotHandledByThisNode;
				break;
				
			case kCreateRecord:
			case kCreateRecordAndOpen:
				siResult = eNotHandledByThisNode;
				break;
				
			case kAddAttribute:
				DBGLOG( "NISPlugin::HandleRequest, we don't handle kAddAttribute yet\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kRemoveAttribute:
				siResult = eNotHandledByThisNode;
				break;
				
			case kAddAttributeValue:
				siResult = eNotHandledByThisNode;
				break;
				
			case kRemoveAttributeValue:
				siResult = eNotHandledByThisNode;
				break;
				
			case kSetAttributeValue:
				siResult = eNotHandledByThisNode;
				break;
				
			case kDoDirNodeAuth:
				DBGLOG( "NISPlugin::HandleRequest, type: kDoDirNodeAuth\n" );
				siResult = DoAuthentication( (sDoDirNodeAuth*)inData );
				break;
				
			case kDoAttributeValueSearch:
			case kDoAttributeValueSearchWithData:
				siResult = DoAttributeValueSearch( (sDoAttrValueSearch *)inData );
				break;
	
			case kDoPlugInCustomCall:
				DBGLOG( "NISPlugin::HandleRequest, type: kDoPlugInCustomCall\n" );
				siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
				break;
				
			case kHandleNetworkTransition:
				DBGLOG( "NISPlugin::HandleRequest, we have some sort of network transition\n" );
				siResult = HandleNetworkTransition( (sHeader*)inData );
				break;
	
				
			default:
				DBGLOG( "NISPlugin::HandleRequest, type: %ld\n", pMsgHdr->fType );
				siResult = eNotHandledByThisNode;
				break;
		}
	}
	
    catch ( int err )
    {
        siResult = err;
        DBGLOG( "NISPlugin::HandleRequest, Caught error:%li\n", siResult );
    }

	pMsgHdr->fResult = siResult;

    if ( siResult )
        DBGLOG( "NISPlugin::HandleRequest returning %ld on a request of type %ld\n", siResult, pMsgHdr->fType );

	return( siResult );

} // HandleRequest

sInt32 NISPlugin::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	sInt32					siResult		= eDSNoErr;
	unsigned long			aRequest		= 0;
	unsigned long			bufLen			= 0;
	AuthorizationRef		authRef			= 0;
	AuthorizationItemSet   *resultRightSet	= NULL;
	NISDirNodeRep*			nodeDirRep		= NULL;
	
	DBGLOG( "NISPlugin::DoPlugInCustomCall called\n" );
//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

	try
	{
		if ( inData == nil ) throw( (sInt32)eDSNullParameter );
		if ( mOpenRefTable == nil ) throw ( (sInt32)eDSNodeNotFound );
		
		LockPlugin();
		
		nodeDirRep			= (NISDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );

		if( !nodeDirRep )
		{
			DBGLOG( "NISPlugin::DoPlugInCustomCall called but we couldn't find the nodeDirRep!\n" );
			
			UnlockPlugin();
			return eDSInvalidNodeRef;
		}
	
		if ( nodeDirRep )
		{
			nodeDirRep->Retain();
			UnlockPlugin();
			
			aRequest = inData->fInRequestCode;

			if ( aRequest != kReadNISConfigData )
			{
				if ( inData->fInRequestData == nil ) throw( (sInt32)eDSNullDataBuff );
				if ( inData->fInRequestData->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );
		
				bufLen = inData->fInRequestData->fBufferLength;
				if ( bufLen < sizeof( AuthorizationExternalForm ) ) throw( (sInt32)eDSInvalidBuffFormat );

				siResult = AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)inData->fInRequestData->fBufferData,
					&authRef);
				if (siResult != errAuthorizationSuccess)
				{
					throw( (sInt32)eDSPermissionError );
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
					throw( (sInt32)eDSPermissionError );
				}
			}
			
			switch( aRequest )
			{
				case kReadNISConfigData:
				{
					DBGLOG( "NISPlugin::DoPlugInCustomCall kReadNISConfigData\n" );

					// read config
					siResult = FillOutCurrentState( inData );
				}
				break;
					 
				case kWriteNISConfigData:
				{
					DBGLOG( "NISPlugin::DoPlugInCustomCall kWriteNISConfigData\n" );
					
					// write config
					siResult = SaveNewState( inData );

					if ( GetNISConfiguration() == eDSNoErr && mLocalNodeString )
					{
						DBGLOG( "NISPlugin::DoPlugInCustomCall calling AddNode on %s\n", mLocalNodeString );
						AddNode( mLocalNodeString );	// all is well, publish our node
					}
					else
						DBGLOG( "NISPlugin::DoPlugInCustomCall not calling AddNode because %s\n", (mLocalNodeString)?"we have no damain name":"of an error in GetNISConfiguration()" );

					ResetMapCache();
				}
				break;
					
				default:
					siResult = eDSInvalidReference;
					break;
			}
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	if ( nodeDirRep )
		nodeDirRep->Release();
		
	if (authRef != 0)
	{
		AuthorizationFree(authRef, 0);
		authRef = 0;
	}

	return( siResult );
}

sInt32 NISPlugin::SaveNewState( sDoPlugInCustomCall *inData )
{
	sInt32					siResult					= eDSNoErr;
	sInt32					xmlDataLength				= 0;
	CFDataRef   			xmlData						= NULL;
	CFDictionaryRef			newStateRef					= NULL;
	CFStringRef				domainNameRef				= NULL;
	CFStringRef				nisServersRef				= NULL;
	CFStringRef				errorString					= NULL;
	Boolean					needToChangeDomain			= false;
	CFStringRef				localNodeStringRef			= NULL;
	
	DBGLOG( "NISPlugin::SaveNewState called\n" );
	
	xmlDataLength = (sInt32) inData->fInRequestData->fBufferLength - sizeof( AuthorizationExternalForm );
	
	if ( xmlDataLength <= 0 )
		return (sInt32)eDSInvalidBuffFormat;
	
	xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData + sizeof( AuthorizationExternalForm )), xmlDataLength);
	
	if ( !xmlData )
	{
		DBGLOG( "NISPlugin::SaveNewState, couldn't create xmlData from buffer!!\n" );
		siResult = (sInt32)eDSInvalidBuffFormat;
	}
	else
		newStateRef = (CFDictionaryRef)CFPropertyListCreateFromXMLData(	NULL,
																	xmlData,
																	kCFPropertyListImmutable,
																	&errorString);
																		
	if ( newStateRef && CFGetTypeID( newStateRef ) != CFDictionaryGetTypeID() )
	{
		DBGLOG( "NISPlugin::SaveNewState, XML Data wasn't a CFDictionary!\n" );
		siResult = (sInt32)eDSInvalidBuffFormat;
	}
	else
	{
		if ( mLocalNodeString )
			localNodeStringRef = CFStringCreateWithCString( NULL, mLocalNodeString, kCFStringEncodingUTF8 );
			
		domainNameRef = (CFStringRef)CFDictionaryGetValue( newStateRef, CFSTR(kDS1AttrLocation) );
		
		if ( domainNameRef && CFGetTypeID( domainNameRef ) == CFStringGetTypeID() )
		{
			if ( !mLocalNodeString )
				needToChangeDomain = true;
			else if ( localNodeStringRef && CFStringCompare( domainNameRef, localNodeStringRef, kCFCompareCaseInsensitive ) != kCFCompareEqualTo )
				needToChangeDomain = true;
		}
		else if ( !domainNameRef )
		{
			DBGLOG( "NISPlugin::SaveNewState, we received no domain name so we are basically being turned off\n" );
			if ( mLocalNodeString )
				needToChangeDomain = true;
		}
		else
		{
			DBGLOG( "NISPlugin::SaveNewState, the domain name is of the wrong type! (%ld)\n", CFGetTypeID( domainNameRef ) );
			siResult = (sInt32)eDSInvalidBuffFormat;
		}
		
		nisServersRef = (CFStringRef)CFDictionaryGetValue( newStateRef, CFSTR(kDSStdRecordTypeServer) );

		if ( nisServersRef && CFGetTypeID( nisServersRef ) != CFStringGetTypeID()  )
		{
			DBGLOG( "NISPlugin::SaveNewState, the list of servers is of the wrong type! (%ld)\n", CFGetTypeID( nisServersRef ) );
			siResult = (sInt32)eDSInvalidBuffFormat;
		}
	}
	
	if ( siResult == eDSNoErr && needToChangeDomain )
		siResult = SetDomain( domainNameRef );
		
	if ( siResult == eDSNoErr )
		siResult = SetNISServers( nisServersRef );
	
	if ( siResult == eDSNoErr )
		ConfigureLookupdIfNeeded();
		
	if ( localNodeStringRef )
		CFRelease( localNodeStringRef );

	if ( xmlData )
		CFRelease( xmlData );
		
	if ( newStateRef )
		CFRelease( newStateRef );

	return siResult;
}

void NISPlugin::ConfigureLookupdIfNeeded( void )
{
	// Until we have solved the circular dependancy problem between lookupd -> DS -> lookupd, 
	// we want to have the NISAgent handle any lookups sent through lookupd and we'll just handle
	// anything that comes directly into DS.
	
	// see if there already exists configuration info for lookupd in NetInfo.  If so, we won't
	// mess with it since it means that either we have been here before or the user has already
	// set it up the way they want it.
	if ( !LookupdIsConfigured() )
	{
		SaveDefaultLookupdConfiguration();
	}
}


Boolean NISPlugin::LookupdIsConfigured( void )
{
	if ( !mLookupdIsAlreadyConfigured )
	{
		// we'll have to check.  We are basically looking to see if there is any data in NetInfo at the
		// following path:
		// /locations/lookupd
		tDirReference			dirRef;
		tDataListPtr			dirNodeName = NULL;
		tDirNodeReference		dirNodeRef;
		tDirStatus				status;
		
		status = dsOpenDirService( &dirRef );
		
		if ( !status )
		{
			sInt32					siResult			= eDSNoErr;
			tDataBuffer			   *pLocalNodeBuff 		= nil;
			tDataList			   *pNodePath			= nil;
			uInt32					uiCount				= 0;
			tContextData			context				= NULL;
			tDataBufferPtr			dataBuffPtr			= NULL;
			tDataListPtr			recName				= NULL;
			tDataListPtr			recordType			= NULL;
			tDataListPtr			attrListAll			= NULL;
			
			try
			{
				pLocalNodeBuff = ::dsDataBufferAllocate( dirRef, 512 );
				if ( pLocalNodeBuff == nil ) throw( (sInt32)eMemoryError );
		
				do 
				{
					siResult = dsFindDirNodes( dirRef, pLocalNodeBuff, NULL, eDSLocalNodeNames, &uiCount, &context );
					if (siResult == eDSBufferTooSmall)
					{
						uInt32 bufSize = pLocalNodeBuff->fBufferSize;
						dsDataBufferDeallocatePriv( pLocalNodeBuff );
						pLocalNodeBuff = nil;
						pLocalNodeBuff = ::dsDataBufferAllocatePriv( bufSize * 2 );
					}
				} while (siResult == eDSBufferTooSmall);
			
				if ( siResult != eDSNoErr ) throw( siResult );
				if ( uiCount == 0 )
				{
					DBGLOG( "NISPlugin::LookupdIsConfigured:dsFindDirNodes on local returned zero" );
					throw( siResult ); //could end up throwing eDSNoErr but no local node will still return nil
				}
				
				// assume there is only one local node
				siResult = dsGetDirNodeName( dirRef, pLocalNodeBuff, 1, &dirNodeName );
				if ( siResult != eDSNoErr )
				{
					DBGLOG( "NISPlugin::LookupdIsConfigured:dsGetDirNodeName on local returned error %ld", siResult );
					throw( siResult );
				}
				
				if ( pLocalNodeBuff != nil )
				{
					::dsDataBufferDeAllocate( dirRef, pLocalNodeBuff );
					pLocalNodeBuff = nil;
				}
				
				//open the local node
				siResult = ::dsOpenDirNode( dirRef, dirNodeName, &dirNodeRef );
				if ( siResult != eDSNoErr )
				{
					DBGLOG( "NISPlugin::LookupdIsConfigured:dsOpenDirNode on local returned error %ld", siResult );
					throw( siResult );
				}
		
				::dsDataListDeAllocate( dirRef, dirNodeName, false );
				free(dirNodeName);
				dirNodeName = nil;
				
				pNodePath = ::dsBuildListFromStringsPriv( kDSNAttrNodePath, nil );
				if ( pNodePath == nil ) throw( (sInt32)eMemoryAllocError );
		
// do a get record list to find the locations native recordtype
				recName = dsDataListAllocate( dirRef );
				recordType = dsDataListAllocate( dirRef );
				attrListAll = dsDataListAllocate( dirRef );
				unsigned long	recEntryCount = 1; // just want to know if the record exists!

				status = dsBuildListFromStringsAlloc( dirRef, recName, "lookupd", nil );

				status = dsBuildListFromStringsAlloc( dirRef, recordType, "dsRecTypeNative:locations", nil );
		
				status = dsBuildListFromStringsAlloc( dirRef, attrListAll, kDSAttributesAll, nil );

				dataBuffPtr = dsDataBufferAllocate( dirRef, 4096 );
	
				tContextData*	continueData = NULL;
				
				status = dsGetRecordList(	dirNodeRef,
											dataBuffPtr,
											recName,
											eDSExact,
											recordType,
											attrListAll,		// all attribute types
											TRUE,
											&recEntryCount,
											continueData );

				if ( recEntryCount > 0 )
					mLookupdIsAlreadyConfigured = true;
					
				//close dir node after releasing attr references
				siResult = ::dsCloseDirNode(dirNodeRef);
			}
		
			catch( sInt32 err )
			{
				siResult = err;
			}
		
			if ( dataBuffPtr != nil )
			{
				::dsDataBufferDeAllocate( dirRef, dataBuffPtr );
				dataBuffPtr = nil;
			}
			
			if ( recName != nil )
			{
				::dsDataListDeAllocate( dirRef, recName, false );
				free(recName);
				recName = nil;
			}
		
			if ( recordType != nil )
			{
				::dsDataListDeAllocate( dirRef, recordType, false );
				free(recordType);
				recordType = nil;
			}
		
			if ( attrListAll != nil )
			{
				::dsDataListDeAllocate( dirRef, attrListAll, false );
				free(attrListAll);
				attrListAll = nil;
			}
		
			if ( dirNodeName != nil )
			{
				::dsDataListDeAllocate( dirRef, dirNodeName, false );
				free(dirNodeName);
				dirNodeName = nil;
			}
		
			if ( pNodePath != nil )
			{
				::dsDataListDeAllocate( dirRef, pNodePath, false );
				free(pNodePath);
				pNodePath = nil;
			}
		
			if ( pLocalNodeBuff != nil )
			{
				::dsDataBufferDeAllocate( dirRef, pLocalNodeBuff );
				pLocalNodeBuff = nil;
			}

			if ( !status )
			{
				// ok, there was a dir node at this location, looks like this has been set up!
				status = dsCloseDirNode( dirNodeRef );
			}
			
			status = dsCloseDirService( dirRef );
		}
		
	}
	
	return mLookupdIsAlreadyConfigured;
}

void NISPlugin::SaveDefaultLookupdConfiguration( void )
{
	tDirReference			dirRef;
	tDataListPtr			dirNodeName = NULL;
	tDirNodeReference		dirNodeRef = 0;
	tDirStatus				status;
	
	status = dsOpenDirService( &dirRef );
	
	if ( !status )
	{
		sInt32					siResult			= eDSNoErr;
		tDataBuffer			   *pLocalNodeBuff 		= nil;
		tDataList			   *pNodePath			= nil;
		uInt32					uiCount				= 0;
		tContextData			context				= NULL;
		tDataNodePtr			attributeKey		= NULL;
		tDataNodePtr			attributeValue		= NULL;
		tDataNodePtr			recName				= NULL;
		tDataNodePtr			recordType			= NULL;
		tRecordReference		userRecRef			= 0;
	
		try
		{
			pLocalNodeBuff = ::dsDataBufferAllocate( dirRef, 512 );
			if ( pLocalNodeBuff == nil ) throw( (sInt32)eMemoryError );
	
			do 
			{
				siResult = dsFindDirNodes( dirRef, pLocalNodeBuff, NULL, eDSLocalNodeNames, &uiCount, &context );
				if (siResult == eDSBufferTooSmall)
				{
					uInt32 bufSize = pLocalNodeBuff->fBufferSize;
					dsDataBufferDeallocatePriv( pLocalNodeBuff );
					pLocalNodeBuff = nil;
					pLocalNodeBuff = ::dsDataBufferAllocatePriv( bufSize * 2 );
				}
			} while (siResult == eDSBufferTooSmall);
		
			if ( siResult != eDSNoErr ) throw( siResult );
			if ( uiCount == 0 )
			{
				DBGLOG( "NISPlugin::LookupdIsConfigured:dsFindDirNodes on local returned zero" );
				throw( siResult ); //could end up throwing eDSNoErr but no local node will still return nil
			}
			
			// assume there is only one local node
			siResult = dsGetDirNodeName( dirRef, pLocalNodeBuff, 1, &dirNodeName );
			if ( siResult != eDSNoErr )
			{
				DBGLOG( "NISPlugin::LookupdIsConfigured:dsGetDirNodeName on local returned error %ld", siResult );
				throw( siResult );
			}
			
			if ( pLocalNodeBuff != nil )
			{
				::dsDataBufferDeAllocate( dirRef, pLocalNodeBuff );
				pLocalNodeBuff = nil;
			}
			
			//open the local node
			siResult = ::dsOpenDirNode( dirRef, dirNodeName, &dirNodeRef );
			if ( siResult != eDSNoErr )
			{
				DBGLOG( "NISPlugin::LookupdIsConfigured:dsOpenDirNode on local returned error %ld", siResult );
				throw( siResult );
			}
	
			::dsDataListDeAllocate( dirRef, dirNodeName, false );
			free(dirNodeName);
			dirNodeName = nil;
			
			pNodePath = ::dsBuildListFromStringsPriv( kDSNAttrNodePath, nil );
			if ( pNodePath == nil ) throw( (sInt32)eMemoryAllocError );
	
// do a get record list to find the locations native recordtype
			
			for ( int i=0; i<kNumLookupTypes; i++ )
			{
				recordType = dsDataNodeAllocateString( dirRef, (i==0)?"dsRecTypeNative:locations":"dsRecTypeNative:locations/lookupd" );
				recName = dsDataNodeAllocateString( dirRef, kLookupdRecordNames[i] );
				if ( recName == nil ) throw( (sInt32)eMemoryAllocError );

				status = dsCreateRecordAndOpen( dirNodeRef, recordType, recName, &userRecRef );
			
				if ( status ) throw( status );
				
				attributeKey = dsDataNodeAllocateString( dirNodeRef, "dsAttrTypeNative:LookupOrder" );
				if ( attributeKey == nil ) throw( (sInt32)eMemoryAllocError );
				
				for ( int j=0; kLookupdRecordLookupOrders[i][j]; j++ )
				{
					attributeValue = dsDataNodeAllocateString( dirNodeRef, kLookupdRecordLookupOrders[i][j] );
					if ( attributeValue == nil ) throw( (sInt32)eMemoryAllocError );

					status = dsAddAttributeValue( userRecRef, attributeKey, attributeValue );
					dsDataNodeDeAllocate( dirNodeRef, attributeValue );
					attributeValue = NULL;
				}

				dsDataNodeDeAllocate( dirNodeRef, attributeKey );
				attributeKey = NULL;
			
				// if this is the first one, we want to add a couple of other attributes
				if ( i == 0 )
				{
					attributeKey = dsDataNodeAllocateString( dirNodeRef, "dsAttrTypeNative:MaxThreads" );
					if ( attributeKey == nil ) throw( (sInt32)eMemoryAllocError );

					attributeValue = dsDataNodeAllocateString( dirNodeRef, "64" );
					if ( attributeValue == nil ) throw( (sInt32)eMemoryAllocError );

					status = dsAddAttributeValue( userRecRef, attributeKey, attributeValue );
					
					dsDataNodeDeAllocate( dirNodeRef, attributeValue );
					attributeValue = NULL;
					
					dsDataNodeDeAllocate( dirNodeRef, attributeKey );
					attributeKey = NULL;
				}

				//close record after releasing attr references
				siResult = ::dsFlushRecord( userRecRef );
				siResult = ::dsCloseRecord( userRecRef );
				userRecRef = 0;
			}			
		}
	
		catch( sInt32 err )
		{
			DBGLOG( "NISPlugin::LookupdIsConfigured:dsOpenDirNode caught error %ld", err );
			siResult = err;
		}
	
		if ( userRecRef != 0 )
		{
			siResult = ::dsCloseRecord(userRecRef);
			userRecRef = 0;
		}
		
		if ( dirNodeRef != 0 )
		{
			dsCloseDirNode(dirNodeRef);
			dirNodeRef = 0;
		}
		
		if ( dirNodeName != nil )
		{
			::dsDataListDeAllocate( dirRef, dirNodeName, false );
			free(dirNodeName);
			dirNodeName = nil;
		}
	
		if ( pNodePath != nil )
		{
			::dsDataListDeAllocate( dirRef, pNodePath, false );
			free(pNodePath);
			pNodePath = nil;
		}
	
		if ( pLocalNodeBuff != nil )
		{
			::dsDataBufferDeAllocate( dirRef, pLocalNodeBuff );
			pLocalNodeBuff = nil;
		}

		if ( recName )
		{
			dsDataNodeDeAllocate( dirRef, recName );
			recName = NULL;
		}
		
		if ( recordType )
		{
			dsDataNodeDeAllocate( dirRef, recordType );
			recordType = NULL;
		}
		
		if ( attributeValue )
		{
			dsDataNodeDeAllocate( dirNodeRef, attributeValue );
			attributeValue = NULL;
		}
		
		if ( attributeKey )
		{	
			dsDataNodeDeAllocate( dirNodeRef, attributeKey );
			attributeKey = NULL;
		}
		
		if ( !status )
		{
			// ok, there was a dir node at this location, looks like this has been set up!
			status = dsCloseDirNode( dirNodeRef );
		}
		
		status = dsCloseDirService( dirRef );
	}
	
	mLookupdIsAlreadyConfigured = true;
}


sInt32 NISPlugin::SetDomain( CFStringRef domainNameRef )
{
	sInt32					siResult					= eDSNoErr;
	CFStringRef				localNodeStringRef			= NULL;
	
	if ( mLocalNodeString )
		localNodeStringRef = CFStringCreateWithCString( NULL, mLocalNodeString, kCFStringEncodingUTF8 );

	if ( localNodeStringRef )
		RemoveNode( localNodeStringRef );
		
	if ( mLocalNodeString )
	{
		free(mLocalNodeString);
		mLocalNodeString = NULL;
	}
	
	if ( domainNameRef )
	{
		CFIndex	len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(domainNameRef), kCFStringEncodingUTF8) + 1;
		mLocalNodeString = (char*)malloc(len);
		CFStringGetCString( domainNameRef, mLocalNodeString, len, kCFStringEncodingUTF8 ); 
		DBGLOG( "NISPlugin::SaveNewState, changing our domain to %s\n", mLocalNodeString );
		AddNode( mLocalNodeString );
	}
	
	char*		resultPtr = CopyResultOfLookup( kNISdomainname, (domainNameRef)?mLocalNodeString:kNoDomainName );		// set this now

	if ( resultPtr )
		free( resultPtr );
	resultPtr = NULL;
	
	if ( localNodeStringRef )
		CFRelease( localNodeStringRef );

	if ( siResult == eDSNoErr )
		siResult = UpdateHostConfig();
		
	return siResult;
}

sInt32 NISPlugin::UpdateHostConfig( void )
{
	sInt32					siResult		= eDSNoErr;
	const char*				nisDomainValue	= (mLocalNodeString)?mLocalNodeString:kNoDomainName;
    FILE					*sourceFP = NULL, *destFP = NULL;
    char					buf[kMaxSizeOfParam];
	
	DBGLOG( "NISPlugin::UpdateHostConfig called\n" );
	// now, we want to edit /etc/hostconfig and if the NISDOMAIN entry is different, updated it with nisDomainValue
	sourceFP = fopen("/etc/hostconfig","r+");
	
	if ( sourceFP == NULL )
	{
		DBGLOG( "NISPlugin::UpdateHostConfig: Could not open hostconfig: %s", strerror(errno) );
		siResult = ePlugInError;
	}

	destFP = fopen( "/tmp/hostconfig.temp", "w+" );

	if ( destFP == NULL )
	{
		DBGLOG( "NISPlugin::UpdateHostConfig: Could not create temp hostconfig.temp: %s", strerror(errno) );
		siResult = ePlugInError;
	}
	
	if ( siResult == eDSNoErr )
	{
		while (fgets(buf,kMaxSizeOfParam,sourceFP) != NULL) 
		{
			char *pcKey = strstr( buf, "NISDOMAIN" );
			
			if ( pcKey == NULL )
			{
				fputs( buf, destFP );
				continue;
			}
			else
			{
				char	domainLine[kMaxSizeOfParam];
				// ok, we are at the line to update.  
				sprintf( domainLine, "NISDOMAIN=%s\n", nisDomainValue );
				fputs( domainLine, destFP );
			}
		}
	}
	
	if ( sourceFP )
		fclose( sourceFP );
	
	if ( destFP )
		fclose( destFP );

	if ( siResult == eDSNoErr )
	{
		const char*	argv[4] = {0};
		char*		resultPtr = NULL;
		Boolean		canceled = false;
		int			callTimedOut = 0;
		
		argv[0] = "/bin/mv";
		argv[1] = "/tmp/hostconfig.temp";
		argv[2] = "/etc/hostconfig";

		if ( myexecutecommandas( NULL, "/bin/mv", argv, false, 10, &resultPtr, &canceled, getuid(), getgid(), &callTimedOut ) < 0 )
		{
			DBGLOG( "NISPlugin::UpdateHostConfig failed to update hostconfig\n" );
			if ( callTimedOut )
				siResult = ePlugInCallTimedOut;
			else
				siResult = ePlugInError;
		}

		if ( resultPtr )
		{
			DBGLOG( "NISPlugin::UpdateHostConfig mv /tmp/hostconfig.temp /etc/hostconfig returned %s\n", resultPtr );
			free( resultPtr );
			resultPtr = NULL;
		}
	}
	
	return siResult;
}

sInt32 NISPlugin::SetNISServers( CFStringRef nisServersRef )
{
	sInt32					siResult	= eDSNoErr;

	DBGLOG( "NISPlugin::SetNISServers called\n" );
	
	if ( mLocalNodeString )
	{
		const char*	argv[4] = {0};
		char		serverFilePath[1024];
		char*		resultPtr = NULL;
		Boolean		canceled = false;
		int			callTimedOut = 0;
	
		sprintf( serverFilePath, "/var/yp/binding/%s.ypservers", mLocalNodeString );
		
		DBGLOG( "NISPlugin::SetNISServers going to delete file at %s\n", serverFilePath );
		
		argv[0] = "/bin/rm";
		argv[1] = "-rf";
		argv[2] = serverFilePath;
	
		if ( myexecutecommandas( NULL, "/bin/rm", argv, false, 10, &resultPtr, &canceled, getuid(), getgid(), &callTimedOut ) < 0 )
		{
			DBGLOG( "NISPlugin::SetNISServers failed to delete ypservers\n" );
		}
	
		if ( resultPtr )
		{
			DBGLOG( "NISPlugin::SetNISServers rm -rf returned %s\n", resultPtr );
			free( resultPtr );
			resultPtr = NULL;
		}

		if ( nisServersRef )
		{
			// need to create a file in location /var/yp/binding/<domainname>.ypservers with a list of servers
			FILE*		fp = NULL;
			char		name[1024] = {0};
			
			sprintf( name, "/var/yp/binding/%s.ypservers", mLocalNodeString );
			fp = fopen(name,"w+");
	
			if (fp == NULL) 
			{
				int			callTimedOut = 0;
				DBGLOG( "NISPlugin::SetNISServers: Could not open config file %s: %s\n", name, strerror(errno) );
				DBGLOG( "NISPlugin::SetNISServers: We will make sure the path exists and try again.\n" );
				
				// try making sure the path exists...
				argv[0] = "/bin/mkdir";
				argv[1] = "-p";
				argv[2] = "/var/yp/binding/";
			
				if ( myexecutecommandas( NULL, "/bin/mkdir", argv, false, 10, &resultPtr, &canceled, getuid(), getgid(), &callTimedOut ) < 0 )
				{
					DBGLOG( "NISPlugin::SetNISServers failed to delete ypservers\n" );
				}
			
				if ( resultPtr )
				{
					DBGLOG( "NISPlugin::SetNISServers mkdir -p returned %s\n", resultPtr );
					free( resultPtr );
					resultPtr = NULL;
				}

				fp = fopen(name,"w+");
			}
			
			if ( fp )
			{
				UInt32 len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(nisServersRef), kCFStringEncodingUTF8) + 1;
				char*	serverList = (char *)::malloc( len );
	
				if ( serverList )
				{
					CFStringGetCString( nisServersRef, serverList, len, kCFStringEncodingUTF8 );
					
					fprintf( fp, "%s\n", serverList );
					DBGLOG( "NISPlugin::SetNISServers saving: %s\n", serverList );
					
					free( serverList );
				}
				else
					DBGLOG( "NISPlugin::SetNISServers could not allocate memory!\n" );
					
				fclose(fp);
			}
			else
				siResult = ePlugInError;

		}
	}

	return siResult;
}

sInt32 NISPlugin::FillOutCurrentState( sDoPlugInCustomCall *inData )
{
	sInt32					siResult					= eDSNoErr;
	CFMutableDictionaryRef	currentStateRef				= CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	CFStringRef				nisServersRef				= NULL;
	CFStringRef				domainNameRef				= NULL;
	CFRange					aRange;
	CFDataRef   			xmlData						= NULL;
	
	try
	{
		if ( !currentStateRef )
			throw( eMemoryError );
		
		if ( mLocalNodeString )
		{
DBGLOG( "NISPlugin::FillOutCurrentState: mLocalNodeString is %s\n", mLocalNodeString );
			domainNameRef = CFStringCreateWithCString( NULL, mLocalNodeString, kCFStringEncodingUTF8 );
			
			if ( domainNameRef )
				CFDictionaryAddValue( currentStateRef, CFSTR(kDS1AttrLocation), domainNameRef );
				
			nisServersRef = CreateListOfServers();
			
			if ( nisServersRef )
				CFDictionaryAddValue( currentStateRef, CFSTR(kDSStdRecordTypeServer), nisServersRef );
		}
		
		//convert the dict into a XML blob
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, currentStateRef );

		if (xmlData != 0)
		{
			aRange.location = 0;
			aRange.length = CFDataGetLength(xmlData);
			if ( inData->fOutRequestResponse->fBufferSize < (unsigned int)aRange.length ) throw( (sInt32)eDSBufferTooSmall );
			CFDataGetBytes( xmlData, aRange, (UInt8*)(inData->fOutRequestResponse->fBufferData) );
			inData->fOutRequestResponse->fBufferLength = aRange.length;
		}
	}

	catch ( sInt32 err )
	{
		DBGLOG( "NISPlugin::FillOutCurrentState: Caught error: %ld\n", err );
		siResult = err;
	}
	
	if ( currentStateRef )
		CFRelease( currentStateRef );
	
	if ( nisServersRef )
		CFRelease( nisServersRef );
	
	if ( domainNameRef )
		CFRelease( domainNameRef );
		
	if ( xmlData )
		CFRelease( xmlData );

	return siResult;
}



CFStringRef NISPlugin::CreateListOfServers( void )
{
    CFMutableStringRef		listRef			= NULL;
	
	if ( mLocalNodeString )
	{
		FILE*		fp = NULL;
		char		name[kMaxSizeOfParam] = {0};
		
		sprintf( name, "/var/yp/binding/%s.ypservers", mLocalNodeString );
		fp = fopen(name,"r");
	
		if (fp == NULL) 
		{
			DBGLOG( "NISPlugin::CreateListOfServers: Could not open config file %s: %s", name, strerror(errno) );
		}
		else
		{
			char	buf[kMaxSizeOfParam];
			
			while (fgets(buf,kMaxSizeOfParam,fp) != NULL) 
			{
				if ( !listRef )
					listRef = CFStringCreateMutable( NULL, 0 );
				
				if ( listRef )
				{
					CFStringAppendCString( listRef, buf, kCFStringEncodingUTF8 );
				}
				else
				{
					DBGLOG( "NISPlugin::CreateListOfServers: Could not allocate a CFString!\n" );
				}
			}
			
			fclose( fp );
		}
	}
	
	return listRef;		
}

Boolean NISPlugin::IsOurConfigNode( char* path )
{
	Boolean 		result = false;
	
	if ( strcmp( path+1, GetProtocolPrefixString() ) == 0 )
		result = true;
		
	return result;
}

Boolean NISPlugin::IsOurDomainNode( char* path )
{
	Boolean 		result = false;
	
	DBGLOG( "NISPlugin::IsOurDomainNode comparing path: %s to localNode: %s\n", (path)?path:"", (mLocalNodeString)?mLocalNodeString:"" );
	if ( mLocalNodeString && strstr( path, mLocalNodeString ) != NULL )
		result = true;
		
	return result;
}

#pragma mark

sInt32 NISPlugin::OpenDirNode ( sOpenDirNode *inData )
{
    sInt32				siResult			= eDSNoErr;
    char			   *nodeName			= nil;
	char			   *pathStr				= nil;
	char			   *protocolStr			= nil;
    tDataListPtr		pNodeList			= nil;
    
	DBGLOG( "NISPlugin::OpenDirNode %lx\n", inData->fOutNodeRef );
	
	if ( inData != nil )
    {
        pNodeList	=	inData->fInDirNodeName;
        pathStr = dsGetPathFromListPriv( pNodeList, (char *)"/" );
        protocolStr = pathStr + 1;	// advance past the '/'
        
        DBGLOG( "NISPlugin::OpenDirNode, ProtocolPrefixString is %s, pathStr is %s\n", GetProtocolPrefixString(), pathStr );
        
		if ( IsOurConfigNode(pathStr) || IsOurDomainNode(pathStr) )
		{
			nodeName = new char[1+strlen(protocolStr)];
			if ( !nodeName ) throw ( eDSNullNodeName );
				
			::strcpy(nodeName,protocolStr);
			
			if ( nodeName )
			{
				DBGLOG( "NISPlugin::OpenDirNode on %s\n", nodeName );
				NISDirNodeRep*		newNodeRep = new NISDirNodeRep( this, (const void*)inData->fOutNodeRef );
				
				if (!newNodeRep) throw ( eDSNullNodeName );
				
				siResult = newNodeRep->Initialize( nodeName, inData->fInUID );
	
				if ( !siResult )
				{
					LockPlugin();
					
					newNodeRep->Retain();
					
					// add the item to the reference table
					::CFDictionaryAddValue( mOpenRefTable, (const void*)inData->fOutNodeRef, (const void*)newNodeRep );
					UnlockPlugin();
				}
				
				delete( nodeName );
				nodeName = nil;
			}
		}
		else
        {
            siResult = eNotHandledByThisNode;            
        }
    }
    else
	{
    	DBGLOG( "NISPlugin::OpenDirNode inData is NULL!\n" );
		siResult = eDSNullParameter;
	}
		
	if (pathStr != NULL)
	{
		free(pathStr);
		pathStr = NULL;
	}

    return siResult;
}


sInt32 NISPlugin::CloseDirNode ( sCloseDirNode *inData )
{
    sInt32				siResult			= eDSNoErr;
    
	DBGLOG( "NISPlugin::CloseDirNode %lx\n", inData->fInNodeRef );

	if ( inData != nil && inData->fInNodeRef && mOpenRefTable )
	{
		NISDirNodeRep*		nodeRep = NULL;
		
		LockPlugin();
		
		nodeRep = (NISDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
		
		DBGLOG( "NISPlugin::CloseDirNode, CFDictionaryGetValue returned nodeRep: 0x%x\n", (int)nodeRep );

		if ( nodeRep )
		{
			::CFDictionaryRemoveValue( mOpenRefTable, (void*)inData->fInNodeRef );

			DBGLOG( "NISPlugin::CloseDirNode, Release nodeRep: 0x%x\n", (int)nodeRep );
			nodeRep->Release();
		}
		else
		{
			DBGLOG( "NISPlugin::CloseDirNode, nodeRef not found in our list\n" );
		}
		
		UnlockPlugin();
	}

    return siResult;
}

#pragma mark
//------------------------------------------------------------------------------------
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

sInt32 NISPlugin::GetDirNodeInfo ( sGetDirNodeInfo *inData )
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
	sNISContextData	   *pAttrContext	= nil;
	NISDirNodeRep*		nodeRep = NULL;

	LockPlugin();
		
	try
	{
		if ( inData  == nil ) throw( (sInt32)eMemoryError );

		nodeRep = (NISDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
		if ( nodeRep  == nil ) throw( (sInt32)eDSBadContextData );

		nodeRep->Retain();

		inAttrList = new CAttributeList( inData->fInDirNodeInfoTypeList );
		if ( inAttrList == nil ) throw( (sInt32)eDSNullNodeInfoTypeList );
		if (inAttrList->GetCount() == 0) throw( (sInt32)eDSEmptyNodeInfoTypeList );

		siResult = outBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff.SetBuffType( 'Gdni' );  //can't use 'StdB' since a tRecordEntry is not returned
		if ( siResult != eDSNoErr ) throw( siResult );

		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32)eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32)eMemoryError );
		aTmpData = new CDataBuff();
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
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrNodePath ) );
				aTmpData->AppendString( kDSNAttrNodePath );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count always two
					aTmpData->AppendShort( 2 );
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( "NIS" ) );
					aTmpData->AppendString( (char *)"NIS" );

					char *tmpStr = nil;
					//don't need to retrieve for the case of "generic unknown" so don't check index 0
					// simply always use the pContext->fName since case of registered it is identical to
					// pConfig->fServerName and in the case of generic it will be correct for what was actually opened
					/*
					if (( pContext->fConfigTableIndex < gConfigTableLen) && ( pContext->fConfigTableIndex >= 1 ))
					{

				        pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( pContext->fConfigTableIndex );
				        if (pConfig != nil)
				        {
				        	if (pConfig->fServerName != nil)
				        	{
				        		tmpStr = new char[1+::strlen(pConfig->fServerName)];
				        		::strcpy( tmpStr, pConfig->fServerName );
			        		}
		        		}
	        		}
					*/
					
					
					if (nodeRep->GetNodeName() != nil)
					{
						CFIndex		len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(nodeRep->GetNodeName()), kCFStringEncodingUTF8) + 1;
						tmpStr = new char[1+len];

						CFStringGetCString( nodeRep->GetNodeName(), tmpStr, len, kCFStringEncodingUTF8 );
					}
					else
					{
						tmpStr = new char[1+::strlen("Unknown Node Location")];
						::strcpy( tmpStr, "Unknown Node Location" );
					}
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );

				} // fInAttrInfoOnly is false
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			} // kDSAttributesAll or kDSNAttrNodePath
			
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
				// Add the attribute length and data
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
				 
			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, kDSNAttrAuthMethod ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrAuthMethod ) );
				aTmpData->AppendString( kDSNAttrAuthMethod );

				if ( inData->fInAttrInfoOnly == false )
				{
					char *tmpStr = nil;
					
					// Attribute value count
					aTmpData->AppendShort( 2 );
					
					tmpStr = new char[1+::strlen( kDSStdAuthCrypt )];
					::strcpy( tmpStr, kDSStdAuthCrypt );
					
					// Append first attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;

					tmpStr = new char[1+::strlen( kDSStdAuthClearText )];
					::strcpy( tmpStr, kDSStdAuthClearText );
					
					// Append second attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;
				} // fInAttrInfoOnly is false

				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or kDSNAttrAuthMethod

			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, "dsAttrTypeStandard:AccountName" ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( "dsAttrTypeStandard:AccountName" ) );
				aTmpData->AppendString( (char *)"dsAttrTypeStandard:AccountName" );

				if ( inData->fInAttrInfoOnly == false )
				{
					char *tmpStr = nil;

					if (tmpStr == nil)
					{
						tmpStr = new char[1+::strlen("No Account Name")];
						::strcpy( tmpStr, "No Account Name" );
					}
					
					// Attribute value count
					aTmpData->AppendShort( 1 );
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );

				} // fInAttrInfoOnly is false
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or dsAttrTypeStandard:AccountName

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
	//add to the offset for the attr list the length of the GetDirNodeInfo fixed record labels
//		record length = 4
//		aRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "dsAttrTypeStandard:DirectoryNodeInfo" ); = 36
//		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "DirectoryNodeInfo" ); = 17
//		total adjustment = 4 + 2 + 36 + 2 + 17 = 61

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

} // GetDirNodeInfo

#pragma mark
sInt32 NISPlugin::ReleaseContinueData( sNISContinueData* continueData )
{
    sInt32					siResult			= eDSNoErr;

	if ( continueData )
	{
		if ( continueData->fResultArrayRef )
		{
			CFRelease( continueData->fResultArrayRef );
		}

		continueData->fResultArrayRef = NULL;
		free( continueData );
	}
	
	return siResult;
}
#pragma mark

//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

sInt32 NISPlugin::GetAttributeEntry ( sGetAttributeEntry *inData )
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
	sNISContextData		   *pAttrContext		= nil;
	sNISContextData		   *pValueContext		= nil;

	LockPlugin();
	try
	{
		if ( inData  == nil ) throw( (sInt32)eMemoryError );

		pAttrContext = (sNISContextData *)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInAttrListRef );
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

sInt32 NISPlugin::GetAttributeValue ( sGetAttributeValue *inData )
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
	sNISContextData			   *pValueContext	= nil;
	uInt32						buffSize		= 0;
	uInt32						buffLen			= 0;
	uInt32						attrLen			= 0;

	LockPlugin();
	try
	{
		pValueContext = (sNISContextData *)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInAttrValueListRef );
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

sInt32 NISPlugin::GetRecordList ( sGetRecordList *inData )
{
    sInt32					siResult			= eDSNoErr;
    char				   *pRecType			= nil;
    char				   *pNSLRecType			= nil;
    CAttributeList		   *cpRecNameList		= nil;
    CAttributeList		   *cpRecTypeList		= nil;
    CBuff				   *outBuff				= nil;
    uInt32					countDownRecTypes	= 0;
    NISDirNodeRep*			nodeDirRep			= nil;
	sNISContinueData		*pContinue			= (sNISContinueData*)inData->fIOContinueData;
	
	LockPlugin();
	
	DBGLOG( "NISPlugin::GetRecordList called\n" );
	nodeDirRep = (NISDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
	
    if( !nodeDirRep )
	{
        DBGLOG( "NISPlugin::GetRecordList called but we couldn't find the nodeDirRep!\n" );

		UnlockPlugin();
		return eDSInvalidNodeRef;
    }

	nodeDirRep->Retain();

	UnlockPlugin();
    
	nodeDirRep->SearchListQueueLock();
    try
    {
		if ( nodeDirRep->IsTopLevelNode() )
		{
			DBGLOG( "NISPlugin::GetRecordList, we don't have any records in our top level node.\n" );
            inData->fOutRecEntryCount	= 0;
            inData->fIOContinueData = NULL;
		}
		else
		{
			if ( !mLocalNodeString )
			{
				DBGLOG( "NISPlugin::GetRecordList we have no domain, returning zero results\n" );
				inData->fOutRecEntryCount	= 0;
				inData->fIOContinueData = NULL;
			}
			else if ( nodeDirRep && !pContinue )
			{
				DBGLOG( "NISPlugin::GetRecordList called, lookup hasn't been started yet.\n" );
			// ok, we need to initialize this nodeDirRep and start the searches...
				// Verify all the parameters
				if( !inData )  throw( eMemoryError );
				if( !inData->fInDataBuff)  throw( eDSEmptyBuffer );
				if( (inData->fInDataBuff->fBufferSize == 0) ) throw( eDSEmptyBuffer );
				if( !inData->fInRecNameList )  throw( eDSEmptyRecordNameList );
				if( !inData->fInRecTypeList )  throw( eDSEmptyRecordTypeList );
				if( !inData->fInAttribTypeList )  throw( eDSEmptyAttributeTypeList );
					
			// Node context data
		
				pContinue = (sNISContinueData *)::calloc( sizeof( sNISContinueData ), sizeof( char ) );
	
				DBGLOG( "NISPlugin::GetRecordList created a new pContinue: 0x%x\n", (int)pContinue );

				pContinue->fRecNameIndex	= 1;
				pContinue->fAllRecIndex		= 0;
				pContinue->fTotalRecCount	= 0;
				pContinue->fRecTypeIndex	= 1;
				pContinue->fAttrIndex		= 1;
				pContinue->fLimitRecSearch	= 0;
				pContinue->fResultArrayRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
				pContinue->fSearchComplete = false;

				//check if the client has requested a limit on the number of records to return
				//we only do this the first call into this context for pContext
				if (inData->fOutRecEntryCount >= 0)
				{
					DBGLOG( "NISPlugin::DoAttributeValueSearch, setting pContinue->fLimitRecSearch to %ld\n", inData->fOutRecEntryCount );
					pContinue->fLimitRecSearch = inData->fOutRecEntryCount;
				}
					
				inData->fOutRecEntryCount	= 0;
							
				// Get the record type list
				cpRecTypeList = new CAttributeList( inData->fInRecTypeList );
		
				if( !cpRecTypeList )  throw( eDSEmptyRecordTypeList );
				//save the number of rec types here to use in separating the buffer data
				countDownRecTypes = cpRecTypeList->GetCount();
		
				if( (countDownRecTypes == 0) ) throw( eDSEmptyRecordTypeList );
		
				sInt32		error = eDSNoErr;
				nodeDirRep->LookupHasStarted();
	
				cpRecNameList = new CAttributeList( inData->fInRecNameList );
				
				if ( !cpRecNameList ) throw( eMemoryError );
				
				uInt32 numRecNames = cpRecNameList->GetCount();
								
				for ( uInt16 i=1; i<=countDownRecTypes; i++ )
				{
					if ( (error = cpRecTypeList->GetAttribute( i, &pRecType )) == eDSNoErr )
					{
						DBGLOG( "NISPlugin::GetRecordList, GetAttribute(%d of %ld) returned pRecType: %s\n", i, countDownRecTypes, pRecType );
						
						pNSLRecType = CreateNISTypeFromRecType( pRecType );
						
						if ( pNSLRecType )
						{

							DBGLOG( "NISPlugin::GetRecordList, CreateNISTypeFromRecType returned pNSLRecType: %s, patternMatch: 0x%x\n", pNSLRecType, (int)inData->fInPatternMatch );
							
							if ( inData->fInPatternMatch == eDSExact || inData->fInPatternMatch == eDSiExact )
							{
								// ah, this is more efficient and we can ask for just the keys we want
								char				*pRecName = nil;
								
								for ( uInt16 i=1; i<=numRecNames; i++ )
								{
									if ( (error = cpRecNameList->GetAttribute( i, &pRecName )) == eDSNoErr && pRecName )
									{
										if ( strcmp( pRecName, "dsRecordsAll" ) == 0 ) 
											DoRecordsLookup( (CFMutableArrayRef)pContinue->fResultArrayRef, pNSLRecType );	// look them all up
										else
											DoRecordsLookup( (CFMutableArrayRef)pContinue->fResultArrayRef, pNSLRecType, pRecName );
									}
								}
							}
							else
								DoRecordsLookup( (CFMutableArrayRef)pContinue->fResultArrayRef, pNSLRecType );
							
							free( pNSLRecType );
						}
						else
							DBGLOG( "NISPlugin::GetRecordList, we don't have a mapping for type: %s, skipping\n", pRecType );
					}
					else
					{
						DBGLOG( "NISPlugin::GetRecordList, GetAttribute returned error: %li\n", error );
					}
				}
			}
			
			if ( nodeDirRep && pContinue )
			{
				// now check to see if we have data waiting for us...
				// we already started a search, check for results.
				siResult = RetrieveResults(	inData->fInNodeRef,
											inData->fInDataBuff,
											inData->fInPatternMatch,
											inData->fInRecNameList,
											inData->fInRecTypeList,
											inData->fInAttribInfoOnly,
											inData->fInAttribTypeList,
											&(inData->fOutRecEntryCount),
											pContinue );
	
				DBGLOG( "***Sending Back Results, fBufferSize = %ld, fBufferLength = %ld\n", inData->fInDataBuff->fBufferSize, inData->fInDataBuff->fBufferLength );
			}
			
			if ( nodeDirRep && pContinue && pContinue->fSearchComplete )
			{
				DBGLOG( "NISPlugin::GetRecordList called and pContinue->fSearchComplete, we are done, Releaseing Continue Data (0x%x)\n", (int)pContinue );
				ReleaseContinueData ( pContinue );
				pContinue = NULL;
			}
		}
    } // try
    
    catch ( int err )
    {
        siResult = err;

		if ( pContinue )
		{
			ReleaseContinueData ( pContinue );
			pContinue = NULL;
		}
		
        DBGLOG( "NISPlugin::GetRecordList, Caught error:%li\n", siResult );
    }

	if ( nodeDirRep )
		nodeDirRep->Release();
		
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

    if ( outBuff != nil )
    {
        delete( outBuff );
        outBuff = nil;
    }

	nodeDirRep->SearchListQueueUnlock();

	DBGLOG( "NISPlugin::GetRecordList returning, inData->fOutRecEntryCount: %ld, inData->fIOContinueData: 0x%x\n", inData->fOutRecEntryCount, (int)pContinue );
	inData->fIOContinueData = pContinue;
	
    return( siResult );

}	// GetRecordList

sInt32 NISPlugin::OpenRecord ( sOpenRecord *inData )
{
    tDirStatus	siResult = eDSNoErr;
	
	DBGLOG( "NISPlugin::OpenRecord called on refNum:0x%x for record: %s of type: %s\n", (int)inData->fOutRecRef, (char*)inData->fInRecName->fBufferData, (char*)inData->fInRecType->fBufferData );

	char*	pNSLRecType = CreateNISTypeFromRecType( (char*)inData->fInRecType->fBufferData );
	
	if ( pNSLRecType )
	{
		CFMutableDictionaryRef	recordResults = CopyRecordLookup( pNSLRecType, (char*)inData->fInRecName->fBufferData );
	
		AddRecordRecRef( inData->fOutRecRef, recordResults );
		
		free( pNSLRecType );
    }
	else
	{
		inData->fOutRecRef = 0;
		siResult = eDSInvalidRecordType;
		DBGLOG( "NISPlugin::OpenRecord we don't support that type!\n" );
	}
		
    return( siResult );
}	// OpenRecord

sInt32 NISPlugin::GetRecordAttributeValueByIndex( sGetRecordAttributeValueByIndex *inData )
{
    tDirStatus					siResult = eDSNoErr;
	tAttributeValueEntryPtr		pOutAttrValue	= nil;
	uInt32						uiDataLen		= 0;

	DBGLOG( "NISPlugin::GetRecordAttributeValueByIndex called on refNum:0x%x, for type: %s, index: %ld\n", (int)inData->fInRecRef, (char*)inData->fInAttrType->fBufferData, inData->fInAttrValueIndex );
	CFDictionaryRef recordRef = GetRecordFromRecRef( inData->fInRecRef );
	
	if ( !recordRef )	
	{
		DBGLOG( "NISPlugin::GetRecordAttributeValueByIndex, unknown record\n" );
		return eDSInvalidRecordRef;
	}

	CFStringRef		keyRef = CFStringCreateWithCString( NULL, (char*)inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
	
	if ( keyRef )
	{
		CFStringRef			valueStringRef = NULL;
		CFPropertyListRef	valueResult = (CFPropertyListRef)CFDictionaryGetValue( recordRef, keyRef );
		
		if ( valueResult && CFGetTypeID( valueResult ) == CFStringGetTypeID() )
		{
			if ( inData->fInAttrValueIndex == 1 )
				valueStringRef = (CFStringRef)valueResult;
			else
				DBGLOG( "NISPlugin::GetRecordAttributeValueByIndex, they are asking for an index of (%ld) when we only have one value\n", inData->fInAttrValueIndex );
		}
		else if ( valueResult && CFGetTypeID( valueResult ) == CFArrayGetTypeID() )		
		{
			if ( (CFIndex)inData->fInAttrValueIndex <= CFArrayGetCount( (CFArrayRef)valueResult ) )
				valueStringRef = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)valueResult, inData->fInAttrValueIndex-1 );
			else
				DBGLOG( "NISPlugin::GetRecordAttributeValueByIndex, they are asking for an index of (%ld) when we only have %ld value(s)\n", inData->fInAttrValueIndex, CFArrayGetCount( (CFArrayRef)valueResult ) );
		}
		else
			DBGLOG( "NISPlugin::GetRecordAttributeValueByIndex, the value wasn't one we handle (%ld), ignoring\n", (valueResult)?CFGetTypeID( valueResult ):0 );

		
		if ( valueStringRef )
		{
			uiDataLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueStringRef), kCFStringEncodingUTF8) + 1;
			pOutAttrValue = (tAttributeValueEntry *)::calloc( (sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad), sizeof( char ) );

			pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen;
			pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;

			pOutAttrValue->fAttributeValueID = inData->fInAttrValueIndex;	// what do we do for an ID, index for now
			CFStringGetCString( valueStringRef, pOutAttrValue->fAttributeValueData.fBufferData, uiDataLen, kCFStringEncodingUTF8 ); 

			inData->fOutEntryPtr = pOutAttrValue;

			DBGLOG( "NISPlugin::GetRecordAttributeValueByIndex, found the value: %s\n", pOutAttrValue->fAttributeValueData.fBufferData );
		}
		else
		{
			DBGLOG( "NISPlugin::GetRecordAttributeValueByIndex, couldn't find any values with this key!\n" );
			siResult = eDSIndexOutOfRange;
		}
			
		CFRelease( keyRef );
	}
	else
	{
		DBGLOG( "NISPlugin::GetRecordAttributeValueByIndex, couldn't create a keyRef!\n" );
		siResult = eDSInvalidAttributeType;
	}
		
    return( siResult );
}

sInt32 NISPlugin::DoAttributeValueSearch( sDoAttrValueSearch* inData )
{
    sInt32					siResult			= eDSNoErr;
    char				   *pRecType			= nil;
    char				   *pNSLRecType			= nil;
    CAttributeList		   *cpRecNameList		= nil;
    CAttributeList		   *cpRecTypeList		= nil;
    CAttributeList		   *cpAttrTypeList 		= nil;
    CBuff				   *outBuff				= nil;
    uInt32					countDownRecTypes	= 0;
    NISDirNodeRep*			nodeDirRep			= nil;
	sNISContinueData		*pContinue			= (sNISContinueData*)inData->fIOContinueData;
    
	LockPlugin();
	
	DBGLOG( "NISPlugin::DoAttributeValueSearch called\n" );
	nodeDirRep = (NISDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
	
    if( !nodeDirRep )
	{
        DBGLOG( "NISPlugin::DoAttributeValueSearch called but we couldn't find the nodeDirRep!\n" );

		UnlockPlugin();
		return eDSInvalidNodeRef;
    }

	nodeDirRep->Retain();
	
	UnlockPlugin();
	
	nodeDirRep->SearchListQueueLock();
    try
    {
		if ( nodeDirRep->IsTopLevelNode() )
		{
			DBGLOG( "NISPlugin::DoAttributeValueSearch, we don't have any records in our top level node.\n" );
            inData->fOutMatchRecordCount	= 0;
            inData->fIOContinueData = NULL;
		}
		else
		{
			if ( nodeDirRep && !pContinue )
			{
				DBGLOG( "NISPlugin::DoAttributeValueSearch called, lookup hasn't been started yet.\n" );
			// ok, we need to initialize this nodeDirRep and start the searches...
				// Verify all the parameters
				if( !inData )  throw( eMemoryError );
				if( !inData->fOutDataBuff)  throw( eDSEmptyBuffer );
				if( (inData->fOutDataBuff->fBufferSize == 0) ) throw( eDSEmptyBuffer );
				if( !inData->fInAttrType )  throw( eDSEmptyAttributeType );
				if( !inData->fInPattMatchType )  throw( eDSEmptyPatternMatch );
				if( !inData->fInPatt2Match )  throw( eDSEmptyPattern2Match );
				if( !inData->fInRecTypeList )  throw( eDSEmptyRecordTypeList );
		
				pContinue = (sNISContinueData *)::calloc( sizeof( sNISContinueData ), sizeof( char ) );
	
				pContinue->fRecNameIndex	= 1;
				pContinue->fAllRecIndex		= 0;
				pContinue->fTotalRecCount	= 0;
				pContinue->fRecTypeIndex	= 1;
				pContinue->fAttrIndex		= 1;
				pContinue->fLimitRecSearch	= 0;
				pContinue->fResultArrayRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
				pContinue->fSearchComplete = false;
					
				//check if the client has requested a limit on the number of records to return
				//we only do this the first call into this context for pContext
				if (inData->fOutMatchRecordCount >= 0)
				{
					DBGLOG( "NISPlugin::DoAttributeValueSearch, setting pContinue->fLimitRecSearch to %ld\n", inData->fOutMatchRecordCount );
					pContinue->fLimitRecSearch = inData->fOutMatchRecordCount;
				}
					
				inData->fOutMatchRecordCount	= 0;
			// Node context data
		
				// Get the record type list
				cpRecTypeList = new CAttributeList( inData->fInRecTypeList );
		
				if( !cpRecTypeList )  throw( eDSEmptyRecordTypeList );
				//save the number of rec types here to use in separating the buffer data
				countDownRecTypes = cpRecTypeList->GetCount();
		
				if( (countDownRecTypes == 0) ) throw( eDSEmptyRecordTypeList );
		
				sInt32		error = eDSNoErr;
				nodeDirRep->LookupHasStarted();
	
				for ( uInt16 i=1; i<=countDownRecTypes; i++ )
				{
					if ( (error = cpRecTypeList->GetAttribute( i, &pRecType )) == eDSNoErr )
					{
						DBGLOG( "NISPlugin::DoAttributeValueSearch, GetAttribute(%d) returned pRecType: %s\n", i, pRecType );
						
						pNSLRecType = CreateNISTypeFromRecType( pRecType );
						
						if ( pNSLRecType )
						{
							DBGLOG( "NISPlugin::DoAttributeValueSearch, looking up records of pNSLRecType: %s, matchType: 0x%x and pattern: \"%s\"\n", pNSLRecType, inData->fInPattMatchType, (inData->fInPatt2Match)?inData->fInPatt2Match->fBufferData:"NULL" );
							
							DoRecordsLookup( (CFMutableArrayRef)pContinue->fResultArrayRef, pNSLRecType, NULL, inData->fInPattMatchType, inData->fInPatt2Match );
							
							free( pNSLRecType );
						}
						else
							DBGLOG( "NISPlugin::DoAttributeValueSearch, we don't have a mapping for type: %s, skipping\n", pNSLRecType );
					}
					else
					{
						DBGLOG( "NISPlugin::DoAttributeValueSearch, GetAttribute returned error: %li\n", error );
					}
				}
			}
			
			if ( nodeDirRep && pContinue )
			{
				// we have data waiting for us...
				// we already started a search, check for results.
				tDataList		allAttributesWanted;
				tDataListPtr	attrTypeList = NULL;
				bool			attrInfoOnly = false;
				
				if (inData->fType == kDoAttributeValueSearchWithData)
				{
					attrTypeList = ((sDoAttrValueSearchWithData*)inData)->fInAttrTypeRequestList;
					attrInfoOnly = ((sDoAttrValueSearchWithData*)inData)->fInAttrInfoOnly;
					DBGLOG( "NISPlugin::DoAttributeValueSearch inData->fType == kDoAttributeValueSearchWithData, inAttrInfoOnly: %d\n", attrInfoOnly );
				}
				else
				{
					// need to fill this out manually.
					dsBuildListFromStringsAlloc( inData->fInNodeRef, &allAttributesWanted,  kDSAttributesAll, NULL );
					attrTypeList = &allAttributesWanted;

				}
				
				if ( siResult == eDSNoErr )
				{
					siResult = RetrieveResults(	inData->fInNodeRef,
												inData->fOutDataBuff,
												eDSAnyMatch,
												NULL,
												inData->fInRecTypeList,
												attrInfoOnly,
												attrTypeList,
												&(inData->fOutMatchRecordCount),
												pContinue );
	
					if ( attrTypeList == &allAttributesWanted )
						dsDataListDeallocate( inData->fInNodeRef, attrTypeList );
				}
				
				DBGLOG( "***Sending Back Results, fBufferSize = %ld, fBufferLength = %ld\n", inData->fOutDataBuff->fBufferSize, inData->fOutDataBuff->fBufferLength );
			}
			
			if ( nodeDirRep && pContinue && pContinue->fSearchComplete )
			{
				DBGLOG( "NISPlugin::DoAttributeValueSearch called and pContinue->fSearchComplete, we are done\n" );
				ReleaseContinueData ( pContinue );
				pContinue = NULL;
			}
			else
				usleep( 100000 );		// sleep for a 10th of a second so the client can't spinlock
		}	
    } // try
    
    catch ( int err )
    {
        siResult = err;
        DBGLOG( "NISPlugin::DoAttributeValueSearch, Caught error:%li\n", siResult );
    }

	if ( nodeDirRep )
		nodeDirRep->Release();
		
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

	nodeDirRep->SearchListQueueUnlock();

	DBGLOG( "NISPlugin::DoAttributeValueSearch returning, inData->fOutMatchRecordCount: %ld, inData->fIOContinueData: 0x%x\n", inData->fOutMatchRecordCount, (int)pContinue );
	inData->fIOContinueData = pContinue;
	
    return( siResult );

}

sInt32 NISPlugin::CloseRecord ( sCloseRecord *inData )
{
    DBGLOG( "NISPlugin::CloseRecord called on refNum:0x%x\n", (int)inData->fInRecRef );

	DeleteRecordRecRef( inData->fInRecRef );
    
    return( eDSNoErr );
}	// CloseRecord

void NISPlugin::AddRecordRecRef( tRecordReference recRef, CFDictionaryRef resultRef )
{
	if ( resultRef )
	{
		char				recAsString[32] = {0};
		
		sprintf( recAsString, "%ld", recRef );
		
		CFStringRef	recAsStringRef = CFStringCreateWithCString( NULL, recAsString, kCFStringEncodingUTF8 );
		
		if ( recAsStringRef )
		{
			CFDictionarySetValue( mOpenRecordsRef, recAsStringRef, resultRef );
			CFRelease( recAsStringRef );
		}
	}
}

void NISPlugin::DeleteRecordRecRef( tRecordReference recRef )
{
	char				recAsString[32] = {0};
	
	sprintf( recAsString, "%ld", recRef );
	
	CFStringRef	recAsStringRef = CFStringCreateWithCString( NULL, recAsString, kCFStringEncodingUTF8 );
	
	if ( recAsStringRef )
	{
		CFDictionaryRemoveValue( mOpenRecordsRef, recAsStringRef );
		CFRelease( recAsStringRef );
	}
}


CFDictionaryRef NISPlugin::GetRecordFromRecRef( tRecordReference recRef )
{
	CFDictionaryRef		resultRef = NULL;
	char				recAsString[32] = {0};
	
	sprintf( recAsString, "%ld", recRef );
	
	CFStringRef	recAsStringRef = CFStringCreateWithCString( NULL, recAsString, kCFStringEncodingUTF8 );
	
	if ( recAsStringRef )
	{
		resultRef = (CFDictionaryRef)CFDictionaryGetValue( mOpenRecordsRef, recAsStringRef );
		CFRelease( recAsStringRef );
	}
	
	return resultRef;
}

#pragma mark

sInt32 NISPlugin::HandleNetworkTransition( sHeader *inData )
{
    sInt32					siResult			= eDSNoErr;
	
	DBGLOG( "NISPlugin::HandleNetworkTransition called\n" );
	
	if ( mLocalNodeString )
	{
		CFStringRef		localNodeStringRef = CFStringCreateWithCString( NULL, mLocalNodeString, kCFStringEncodingUTF8 );
		
		RemoveNode( localNodeStringRef );
		CFRelease( localNodeStringRef );
		
		free( mLocalNodeString );
	}
	
	mLocalNodeString = NULL;

	siResult = GetNISConfiguration();
	
	if ( siResult == eDSNoErr && mLocalNodeString )
	{
		DBGLOG( "NISPlugin::HandleNetworkTransition calling AddNode on %s\n", mLocalNodeString );
		AddNode( mLocalNodeString );	// all is well, publish our node
	}
	else
		DBGLOG( "NISPlugin::HandleNetworkTransition not calling AddNode (siResult:%ld) because %s\n", siResult, (mLocalNodeString)?"we have no damain name":"of an error" );

	ResetMapCache();
	
    return ( siResult );
}

void NISPlugin::ResetMapCache( void )
{
	LockMapCache();
	DBGLOG( "NISPlugin::ResetMapCache called\n" );
	if ( mCachedMapsRef )
	{
		CFRelease( mCachedMapsRef );
	}
	mCachedMapsRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	mLastTimeCacheReset = CFAbsoluteTimeGetCurrent();
	UnlockMapCache();
}

Boolean NISPlugin::ResultMatchesRequestRecordNameCriteria(	CFDictionaryRef		result,
															tDirPatternMatch	patternMatch,
															tDataListPtr		inRecordNameList )
{
	// things that need to match up:
	// record type ( we're assuming this is fine for now )
	// record name ( a particular entry, dsRecordsAll, dsRecordsStandardAll, or dsRecordsNativeAll )
	if ( patternMatch == eDSAnyMatch )
		return true;
	
	Boolean				resultIsOK = false;
	CAttributeList* 	cpRecNameList = new CAttributeList( inRecordNameList );
	
	CFPropertyListRef	valueResult = NULL;

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
				
				if ( pRecName )
					DBGLOG( "NISPlugin::ResultMatchesRequestRecordNameCriteria, request name is: %s\n", pRecName );
				else
					DBGLOG( "NISPlugin::ResultMatchesRequestRecordNameCriteria, request name is NULL\n" );
					
				if ( pRecName && strcmp( pRecName, kDSRecordsAll ) == 0 )
				{
					resultIsOK = true;
					break;
				}
				else if ( pRecName && strcmp( pRecName, kDSRecordsStandardAll ) == 0 )
				{
					valueResult = (CFPropertyListRef)CFDictionaryGetValue( result, CFSTR(kDSNAttrRecordType) );
					
					if ( valueResult && CFGetTypeID( valueResult ) == CFStringGetTypeID() )
					{
						resultIsOK = CFStringHasPrefix( (CFStringRef)valueResult, CFSTR(kDSStdRecordTypePrefix) );
					}
					else if ( valueResult && CFGetTypeID( valueResult ) == CFArrayGetTypeID() )		
					{
						for ( CFIndex i=0; i<CFArrayGetCount( (CFArrayRef)valueResult ) && !resultIsOK; i++ )
						{
							CFStringRef		resultRecordAttributeRef = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)valueResult, i );
							
							resultIsOK = (resultRecordAttributeRef && CFStringHasPrefix( (CFStringRef)resultRecordAttributeRef, CFSTR(kDSStdRecordTypePrefix) ) );
						}
					}
				}
				else if ( pRecName && strcmp( pRecName, kDSRecordsNativeAll ) == 0 )
				{
					valueResult = (CFPropertyListRef)CFDictionaryGetValue( result, CFSTR(kDSNAttrRecordType) );
					
					if ( valueResult && CFGetTypeID( valueResult ) == CFStringGetTypeID() )
					{
						resultIsOK = CFStringHasPrefix( (CFStringRef)valueResult, CFSTR(kDSNativeRecordTypePrefix) );
					}
					else if ( valueResult && CFGetTypeID( valueResult ) == CFArrayGetTypeID() )		
					{
						for ( CFIndex i=0; i<CFArrayGetCount( (CFArrayRef)valueResult ) && !resultIsOK; i++ )
						{
							CFStringRef		resultRecordAttributeRef = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)valueResult, i );
							
							resultIsOK = (resultRecordAttributeRef && CFStringHasPrefix( (CFStringRef)resultRecordAttributeRef, CFSTR(kDSNativeRecordTypePrefix) ) );
						}
					}
				}
				else
				{
					valueResult = (CFPropertyListRef)CFDictionaryGetValue( result, CFSTR(kDSNAttrRecordName) );
					
					if ( !valueResult )
						continue;
						
					recNameMatchRef = CFStringCreateWithCString( NULL, pRecName, kCFStringEncodingUTF8 );
					
					if ( recNameMatchRef )
					{
						if ( CFGetTypeID( valueResult ) == CFStringGetTypeID() )
						{
							resultIsOK = IsResultOK( patternMatch, (CFStringRef)valueResult, recNameMatchRef );
						}
						else if ( CFGetTypeID( valueResult ) == CFArrayGetTypeID() )		
						{
							for ( CFIndex i=0; i<CFArrayGetCount( (CFArrayRef)valueResult ) && !resultIsOK; i++ )
							{
								CFStringRef		resultRecordNameRef = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)valueResult, i );
								
								if ( resultRecordNameRef && CFGetTypeID( resultRecordNameRef ) == CFStringGetTypeID() )
									resultIsOK = IsResultOK( patternMatch, resultRecordNameRef, recNameMatchRef );
							}
						}
						
						CFRelease( recNameMatchRef );
					}
				}
			}
		}
		
		delete cpRecNameList;
	}
	
	return resultIsOK;
}

						
sInt32 NISPlugin::RetrieveResults(	tDirNodeReference	inNodeRef,
									tDataBufferPtr		inDataBuff,
									tDirPatternMatch	inRecordNamePatternMatch,
									tDataListPtr		inRecordNameList,
									tDataListPtr		inRecordTypeList,
									bool				inAttributeInfoOnly,
									tDataListPtr		inAttributeInfoTypeList,
									unsigned long*		outRecEntryCount,
									sNISContinueData*	continueData )
{
    sInt32					siResult			= eDSNoErr;
	CDataBuff*				aRecData			= nil;
	CDataBuff*				aAttrData			= nil;
	CDataBuff*				aTempData			= nil;
    CBuff*					outBuff				= nil;
	
	if ( !outRecEntryCount || !continueData || !inDataBuff )
		return eDSNullParameter;
		
	*outRecEntryCount = 0;
    
	DBGLOG( "NISPlugin::RetrieveResults called for 0x%x in node %lx\n", (int)continueData, inNodeRef );

	// we only support certain pattern matches which are defined in NISPlugin::ResultMatchesRequestRecordNameCriteria
	if (	inRecordNamePatternMatch != eDSExact
		&&	inRecordNamePatternMatch != eDSiExact
		&&	inRecordNamePatternMatch != eDSAnyMatch
		&&	inRecordNamePatternMatch != eDSStartsWith 
		&&	inRecordNamePatternMatch != eDSEndsWith 
		&&	inRecordNamePatternMatch != eDSContains
		&&	inRecordNamePatternMatch != eDSiContains )
	{
		DBGLOG( "NISPlugin::RetrieveResults called with a pattern match we haven't implemented yet: 0x%.4x\n", inRecordNamePatternMatch );
		return eNotYetImplemented;
    }
	
	if ( (CFIndex)*outRecEntryCount >= CFArrayGetCount(continueData->fResultArrayRef) )
	{
		DBGLOG( "NISPlugin::RetrieveResults we're finished, setting fSearchComplete and fOutRecEntryCount to zero\n" );
		continueData->fSearchComplete = true;		// we are done

		return eDSNoErr;
	}

	try
    {    	
		aRecData = new CDataBuff();
		if( !aRecData )  throw( eMemoryError );
		aAttrData = new CDataBuff();
		if( !aAttrData )  throw( eMemoryError );
		aTempData = new CDataBuff();
		if( !aTempData )  throw( eMemoryError );
        
        // copy the buffer data into a more manageable form
        outBuff = new CBuff();
        if( !outBuff )  throw( eMemoryError );
    
        siResult = outBuff->Initialize( inDataBuff, true );
        if( siResult ) throw ( siResult );
    
        siResult = outBuff->GetBuffStatus();
        if( siResult ) throw ( siResult );
    
        siResult = outBuff->SetBuffType( kClientSideParsingBuff );
        if( siResult ) throw ( siResult );
        
		while ( continueData->fResultArrayRef && CFArrayGetCount(continueData->fResultArrayRef) > 0 && !siResult && (continueData->fTotalRecCount < continueData->fLimitRecSearch || continueData->fLimitRecSearch == 0) )
        {				
			Boolean		okToAddResult = false;
			
			DBGLOG( "NISPlugin::RetrieveResults clearing aAttrData and aRecData\n" );
			aAttrData->Clear();
			aRecData->Clear();

            // package the record into the DS format into the buffer
            // steps to add an entry record to the buffer
			CFDictionaryRef		curResult = (CFDictionaryRef)CFArrayGetValueAtIndex( continueData->fResultArrayRef, CFArrayGetCount(continueData->fResultArrayRef)-1 );
			
			if ( !curResult )
				throw( ePlugInDataError );
				
			CFRetain( curResult );
			
			CFArrayRemoveValueAtIndex( continueData->fResultArrayRef, CFArrayGetCount(continueData->fResultArrayRef)-1 );
			
			if ( !ResultMatchesRequestRecordNameCriteria( curResult, inRecordNamePatternMatch, inRecordNameList ) )
			{
				DBGLOG( "NISPlugin::RetrieveResults curResult: 0x%x for index: %ld doesn't satisfy Request Criteria, skipping\n", (int)curResult, CFArrayGetCount(continueData->fResultArrayRef)+1 );
				continue;
			}
			
			DBGLOG( "NISPlugin::RetrieveResults curResult: 0x%x for index: %ld and pattern match: 0x%x\n", (int)curResult, CFArrayGetCount(continueData->fResultArrayRef)+1, (int)inRecordNamePatternMatch );
			char*		recordName = NULL;
			CFIndex		recordNameLength = 0;
			CFStringRef	recordNameRef = NULL;

			char*		recType = NULL;
			CFIndex		recTypeLength = 0;
			
			CFStringRef	recordTypeRef = (CFStringRef)CFDictionaryGetValue( curResult, CFSTR(kDSNAttrRecordType) );

			if ( recordTypeRef && CFGetTypeID( recordTypeRef ) == CFArrayGetTypeID() )
			{
				DBGLOG( "NISPlugin::RetrieveResults curResult has more than one record type, grabbing the first one\n" );
				recordTypeRef = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)recordTypeRef, 0 );
			}
			
			if ( recordTypeRef && CFGetTypeID( recordTypeRef ) == CFStringGetTypeID() )
			{
				recTypeLength = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(recordTypeRef), kCFStringEncodingUTF8) +1;
		
				recType = (char*)malloc( recTypeLength );

				if ( !::CFStringGetCString( recordTypeRef, recType, recTypeLength, kCFStringEncodingUTF8 ) )
				{
					DBGLOG( "NISPlugin::RetrieveResults couldn't convert recordTypeRef CFString to char* in UTF8!\n" );
					free( recType );
					recType = NULL;
				}
			}
			else if ( recordTypeRef )
			{
				DBGLOG( "NISPlugin::RetrieveResults unknown recordTypeRefID: %ld\n", CFGetTypeID( recordTypeRef ) );
			}
				
			if ( recType != nil )
			{
				DBGLOG( "NISPlugin::RetrieveResults recType=%s\n", recType );
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
			recordNameRef = (CFStringRef)CFDictionaryGetValue( curResult, CFSTR(kDSNAttrRecordName) );
		
			if ( !recordNameRef )
			{
				DBGLOG( "NISPlugin::RetrieveResults record has no name, skipping\n" );
				continue;			// skip this result
			}
			
			if ( CFGetTypeID( recordNameRef ) == CFArrayGetTypeID() )
			{
				recordNameRef = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)recordNameRef, 0 );
			}
			
			recordNameLength = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(recordNameRef), kCFStringEncodingUTF8) +1;
		
			recordName = (char*)malloc( recordNameLength );
		
			if ( !::CFStringGetCString( recordNameRef, recordName, recordNameLength, kCFStringEncodingUTF8 ) )
			{
				DBGLOG( "NISPlugin::RetrieveResults couldn't convert recordNameRef CFString to char* in UTF8!\n" );
				recordName[0] = '\0';
			}
		
			if ( recordNameRef != nil && recordName[0] != '\0' )
			{
				DBGLOG( "NISPlugin::RetrieveResults looking at record: %s\n", recordName );
				aRecData->AppendShort( ::strlen( recordName ) );
				aRecData->AppendString( recordName );
			}
			else
			{
				aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
				aRecData->AppendString( "Record Name Unknown" );
			}
			
			// now the attributes
			AttrDataContext		context = {0,aAttrData,inAttributeInfoOnly, NULL};
				
			// we want to pull out the appropriate attributes that the client is searching for...
			uInt32					numAttrTypes	= 0;
			CAttributeList			cpAttributeList( inAttributeInfoTypeList );
	
			//save the number of rec types here to use in separating the buffer data
			numAttrTypes = cpAttributeList.GetCount();
	
			DBGLOG( "NISPlugin::RetrieveResults client looking for %ld attribute(s)\n", numAttrTypes );
			if( (numAttrTypes == 0) ) throw( eDSEmptyRecordTypeList );
	
			sInt32					error = eDSNoErr;
			char*				   	pAttrType = nil;
			
			for ( uInt32 i=1; i<=numAttrTypes; i++ )
			{
				if ( (error = cpAttributeList.GetAttribute( i, &pAttrType )) == eDSNoErr )
				{
					DBGLOG( "NISPlugin::RetrieveResults looking for attribute type: %s\n", pAttrType );
					
					if ( strcmp( pAttrType, kDSAttributesAll ) == 0 || strcmp( pAttrType, kDSAttributesStandardAll ) == 0 || strcmp( pAttrType, kDSAttributesNativeAll ) == 0 )
					{
						// we want to return everything
						context.attrType = pAttrType;
						::CFDictionaryApplyFunction( curResult, AddDictionaryDataToAttrData, &context );
						okToAddResult = true;
						break;
					}
					else
					{
						CFStringRef		keyRef = ::CFStringCreateWithCString( NULL, pAttrType, kCFStringEncodingUTF8 );
						CFStringRef		valueRef = NULL;
						CFTypeRef		valueTypeRef = NULL;
						CFArrayRef		valueArrayRef	= NULL;
						
						aTempData->Clear();
						
						if ( ::CFDictionaryContainsKey( curResult, keyRef ) )
							valueTypeRef = (CFTypeRef)::CFDictionaryGetValue( curResult, keyRef );
						
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
								DBGLOG( "NISPlugin::RetrieveResults got unknown value type (%ld), ignore\n", CFGetTypeID(valueTypeRef) );
					
							aTempData->AppendShort( ::strlen( pAttrType ) );		// attrTypeLen
							aTempData->AppendString( pAttrType );					// attrType

							if ( valueArrayRef && !inAttributeInfoOnly )
							{	
								aTempData->AppendShort( (short)CFArrayGetCount(valueArrayRef) );	// attrValueCount
								
								for ( CFIndex i=0; i< CFArrayGetCount(valueArrayRef); i++ )
								{
									valueRef = (CFStringRef)::CFArrayGetValueAtIndex( valueArrayRef, i );
		
									if ( valueRef && ::CFStringGetLength(valueRef) > 0 )
									{
										char*	value = (char*)malloc( ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueRef),kCFStringEncodingUTF8) + 1 );
										
										if ( ::CFStringGetCString( valueRef, value, ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueRef),kCFStringEncodingUTF8) + 1, kCFStringEncodingUTF8 ) )	
										{
											aTempData->AppendShort( ::strlen( value ) );
											aTempData->AppendString( value );
											DBGLOG( "NISPlugin::RetrieveResults added value: %s\n", value );
										}
										else
										{
											DBGLOG( "NISPlugin::RetrieveResults couldn't make cstr from CFString for value!\n" );
											aTempData->AppendShort( 0 );
										}
										
										free( value );
										value = NULL;
									}
									else
									{
										DBGLOG( "NISPlugin::RetrieveResults no valueRef or its length is 0!\n" );
										aTempData->AppendShort( 0 );
									}
								}
							}
							else if ( inAttributeInfoOnly )
							{
								aTempData->AppendShort( 0 );	// attrValueCount
								DBGLOG( "NISPlugin::RetrieveResults skipping values as the caller only wants attributes\n" );
							}
							else
							{
								DBGLOG( "NISPlugin::RetrieveResults no values for this attribute, skipping!\n" );

								if ( valueArrayRef )
								{
									CFRelease( valueArrayRef );		// now release
									valueArrayRef = NULL;
								}
	
								continue;
							}
							
							if ( valueArrayRef )
							{
								CFRelease( valueArrayRef );		// now release
								valueArrayRef = NULL;
							}
							
							context.count++;

							DBGLOG( "NISPlugin::RetrieveResults adding aTempData to aAttrData, context.count: %d\n", (short)context.count );
							aAttrData->AppendShort(aTempData->GetLength());
							aAttrData->AppendBlock(aTempData->GetData(), aTempData->GetLength() );
						}
					}
				}
				else
				{
					DBGLOG( "NISPlugin::RetrieveResults GetAttribute returned error:%li\n", error );
				}
			}
			
			// Attribute count
			DBGLOG( "NISPlugin::RetrieveResults adding aAttrData count %d to aRecData\n", (short)context.count );
			aRecData->AppendShort( (short)context.count );

			if ( context.count > 0 )
			{
				// now add the attributes to the record
				aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
			}
			
			DBGLOG( "NISPlugin::RetrieveResults calling outBuff->AddData()\n" );
			siResult = outBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
			
			if ( recordName )
				free( recordName );
			recordName = NULL;
			
			if ( siResult == CBuff::kBuffFull )
			{
				DBGLOG( "NISPlugin::RetrieveResults siResult == CBuff::kBuffFull, don't bump the count for next time\n" );
				CFArrayAppendValue( continueData->fResultArrayRef, curResult );		// put this back
				
				UInt32	numDataBlocks = 0;
				
				if ( outBuff->GetDataBlockCount( &numDataBlocks ) != eDSNoErr || numDataBlocks == 0 )
				{
					siResult = eDSBufferTooSmall;	// we couldn't fit any other data in here either
					DBGLOG( "NISPlugin::RetrieveResults siResult == eDSBufferTooSmall, we're done\n" );
				}
			}
			else if ( siResult == eDSNoErr )
			{
				continueData->fTotalRecCount++;
				(*outRecEntryCount)++;
			}
			else
				DBGLOG( "NISPlugin::RetrieveResults siResult == %ld\n", siResult );
			
			CFRelease( curResult );
			curResult = NULL;
		}

        if ( siResult == CBuff::kBuffFull )
            siResult = eDSNoErr;
			
        outBuff->SetLengthToSize();
   }

    catch ( int err )
    {
		DBGLOG( "NISPlugin::RetrieveResults caught err: %d\n", err );
        siResult = err;
    }

    if ( aRecData != nil )
    {
        delete (aRecData);
        aRecData = nil;
    }
	
    if ( aTempData != nil )
    {
        delete (aTempData);
        aTempData = nil;
    }
	
    if ( aAttrData != nil )
    {
        delete (aAttrData);
        aAttrData = nil;
    }

    if ( outBuff != nil )
    {
        delete( outBuff );
        outBuff = nil;
    }

	if ( !continueData->fResultArrayRef || CFArrayGetCount( continueData->fResultArrayRef ) == 0 || continueData->fTotalRecCount >= continueData->fLimitRecSearch )
	{
		continueData->fSearchComplete = true;
		DBGLOG( "NISPlugin::RetrieveResults setting fSearchComplete to true\n" );
	}
		
	DBGLOG( "NISPlugin::RetrieveResults returning siResult: %ld\n", siResult );

    return( siResult );
    
}

// ---------------------------------------------------------------------------
//	* CreateNISTypeFromRecType
// ---------------------------------------------------------------------------

char* NISPlugin::CreateNISTypeFromRecType ( char *inRecType )
{
    char				   *outResult	= nil;
	CFStringRef				nisTypeRef = CreateNISTypeRefFromRecType( inRecType );
	
	if ( nisTypeRef )
	{
		CFIndex		len = ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(nisTypeRef), kCFStringEncodingUTF8) + 1;
		
		outResult = (char*)malloc( len );	// GetLength returns 16 bit value...
		CFStringGetCString( nisTypeRef, outResult, len, kCFStringEncodingUTF8 );
	
		DBGLOG( "NISPlugin::CreateNISTypeFromRecType mapping %s to %s\n", inRecType, outResult );
    }
	
	return outResult;
}

CFStringRef NISPlugin::CreateNISTypeRefFromRecType ( char *inRecType )
{
	CFStringRef		nisTypeRef = NULL;
	
	if ( mMappingTable )
	{
		CFStringRef		inTypeRef = CFStringCreateWithCString( NULL, inRecType, kCFStringEncodingUTF8 );
		
		if ( inTypeRef )
		{
			nisTypeRef = (CFStringRef)CFDictionaryGetValue( mMappingTable, inTypeRef );
		
			CFRelease( inTypeRef );
		}
		
	}

    return( nisTypeRef );

} // CreateNISTypeFromRecType

#pragma mark -
void NISPlugin::StartNodeLookup( void )
{
    DBGLOG( "NISPlugin::StartNodeLookup called\n" );
		
	if ( mLocalNodeString )
		AddNode( mLocalNodeString );	// all is well, publish our node
}

CFMutableDictionaryRef NISPlugin::CopyRecordLookup(  char* mname, char* recordName )
{
	CFMutableDictionaryRef	resultRecordRef = NULL;
	
    DBGLOG( "NISPlugin::CopyRecordLookup called, mname: %s, recordName: %s\n", mname, recordName );

	CFMutableArrayRef	tempArrayRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );

	if ( tempArrayRef )
	{
		DoRecordsLookup( tempArrayRef, mname, recordName );
		
		if ( CFArrayGetCount(tempArrayRef) > 0 )
		{
			resultRecordRef = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( tempArrayRef, 0 );
			CFRetain( resultRecordRef );
		}
		
		CFRelease( tempArrayRef );
	}
	
	return resultRecordRef;
}

char* NISPlugin::CopyResultOfLookup( NISLookupType type, const char* mname, const char* keys )
{
    char*		resultPtr = NULL;
	const char*	argv[8] = {0};
	const char* pathArg = NULL;
    Boolean		canceled = false;
	int			callTimedOut = 0;
	
	// we can improve here by saving these results and caching them...
	switch (type)
	{
		case kNISypcat:
		{
			argv[0] = kNISypcatPath;
			argv[1] = "-k";
			argv[2] = "-d";
			argv[3] = mLocalNodeString;
			argv[4] = mname;
			
			pathArg = kNISypcatPath;
		}
		break;
	
		case kNISdomainname:
		{
			argv[0] = kNISdomainnamePath;
			argv[1] = mname;
			pathArg = kNISdomainnamePath;
		}
		break;
		
		case kNISrpcinfo:
		{
			argv[0] = kNISrpcinfoPath;
			argv[1] = "-p";

			pathArg = kNISrpcinfoPath;
		}
		break;
		
		case kNISportmap:
		{
			argv[0] = kNISportmapPath;

			pathArg = kNISportmapPath;
		}
		break;
		
		case kNISbind:
		{
			argv[0] = kNISbindPath;

			pathArg = kNISbindPath;
		}
		break;
		
		case kNISypwhich:
		{
			argv[0] = kNISypwhichPath;

			pathArg = kNISypwhichPath;
		}
		break;

		case kNISypmatch:
		{
			argv[0] = kNISypmatchPath;
			argv[1] = "-d";
			argv[2] = mLocalNodeString;
			argv[3] = "-t";
			argv[4] = "-k";
			argv[5] = keys;
			argv[6] = mname;
			
			pathArg = kNISypmatchPath;
		}
		break;
	};
			
	if ( myexecutecommandas( NULL, pathArg, argv, false, 10, &resultPtr, &canceled, getuid(), getgid(), &callTimedOut ) < 0 )
	{
		DBGLOG( "NISPlugin::CopyResultOfLookup failed\n" );
	}
	
	return resultPtr;
}

void NISPlugin::DoRecordsLookup(	CFMutableArrayRef	resultArrayRef,
									char*				mname,
									char*				recordName,
									tDirPatternMatch	inAttributePatternMatch,
									tDataNodePtr		inAttributePatt2Match )
{
	CFDictionaryRef		cachedResult = NULL;

	if ( !resultArrayRef )
	{
		DBGLOG( "NISPlugin::DoRecordsLookup resultArrayRef passed in is NULL!\n" );
		return;
	}

	if ( recordName )
		DBGLOG( "NISPlugin::DoRecordsLookup looking for recordName: %s in map: %s\n", recordName, mname );
		
	if (	inAttributePatternMatch != eDSExact
		&&	inAttributePatternMatch != eDSiExact
		&&	inAttributePatternMatch != eDSAnyMatch
		&&	inAttributePatternMatch != eDSStartsWith 
		&&	inAttributePatternMatch != eDSEndsWith 
		&&	inAttributePatternMatch != eDSContains
		&&	inAttributePatternMatch != eDSiContains )
	{
		DBGLOG( "NISPlugin::DoRecordsLookup called with a pattern match we haven't implemented yet: 0x%.4x\n", inAttributePatternMatch );
		throw( eNotYetImplemented );
    }
	
	if ( recordName )
	{
		cachedResult = CopyRecordResult( mname, recordName );
		
		if ( cachedResult )
		{
			if ( RecordIsAMatch( cachedResult, inAttributePatternMatch, inAttributePatt2Match ) )
				CFArrayAppendValue( resultArrayRef, cachedResult );
			else
				DBGLOG( "NISPlugin::DoRecordsLookup RecordIsAMatch returned false\n" );
			
			CFRelease( cachedResult );
		}
		else
			DBGLOG( "NISPlugin::DoRecordsLookup couldn't find a matching result\n" );
	}
	else
	{
		cachedResult = CopyMapResults( mname );
		
		if ( cachedResult && ( (inAttributePatt2Match && inAttributePatt2Match->fBufferData) || inAttributePatternMatch == eDSAnyMatch ) )
		{
			CFStringRef			patternRef = NULL;
			
			if ( inAttributePatt2Match && inAttributePatt2Match->fBufferData )
				patternRef = CFStringCreateWithCString( NULL, inAttributePatt2Match->fBufferData, kCFStringEncodingUTF8 );
			
			ResultMatchContext	context = { inAttributePatternMatch, patternRef, resultArrayRef };
		
			CFDictionaryApplyFunction( cachedResult, FindAllAttributeMatches, &context );

			DBGLOG( "NISPlugin::DoRecordsLookup FindMatchingResults found %ld matches\n", CFArrayGetCount(resultArrayRef) );
		}
		else
			DBGLOG( "NISPlugin::DoRecordsLookup couldn't find any matching results\n" );
	
		if ( cachedResult )
			CFRelease( cachedResult );
	}
}

CFDictionaryRef NISPlugin::CopyMapResults( const char* mname )
{
	if ( !mname )
		return NULL;
		
	CFDictionaryRef			mapResults = NULL;
	CFMutableDictionaryRef	newMapResult = NULL;
	CFStringRef				mapNameRef = CFStringCreateWithCString( NULL, mname, kCFStringEncodingUTF8 );
	
	if ( mapNameRef )
	{
		LockMapCache();

		mapResults = (CFDictionaryRef)CFDictionaryGetValue( mCachedMapsRef, mapNameRef );

		if ( mapResults )
		{
			CFRetain( mapResults );

			DBGLOG( "NISPlugin::CopyMapResults returning cached result\n" );
		}
		
		UnlockMapCache();
	}
	else
		DBGLOG("NISPlugin::CopyMapResults couldn't make a CFStringRef out of mname: %s\n", mname );
	
	if ( !mapResults )
	{
		DBGLOG("NISPlugin::CopyMapResults no cached entry for map: %s, doing a lookup...\n", mname );
		newMapResult = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		char*					resultPtr = CopyResultOfLookup( kNISypcat, mname );
		
		if ( resultPtr )
		{
			if ( strstr( resultPtr, kBindErrorString ) && !mWeLaunchedYPBind )
			{
				// it looks like ypbind may not be running, lets take a look and run it ourselves if need be.
				DBGLOG( "NISPlugin::CopyMapResults got an error, implying that ypbind may not be running\n" );
				
				free( resultPtr );
				resultPtr = NULL;
				
				DBGLOG( "NISPlugin::CopyMapResults attempting to launch ypbind\n" );
				mWeLaunchedYPBind = true;
				resultPtr = CopyResultOfLookup( kNISbind );			
			
				if ( resultPtr )
				{
					free( resultPtr );
					resultPtr = NULL;
				}
			
				// now try again.
				resultPtr = CopyResultOfLookup( kNISypcat, mname );
			}

			if ( strncmp( resultPtr, "No such map", strlen("No such map") ) == 0 || strncmp( resultPtr, "Can't match key", strlen("Can't match key") ) == 0 )
			{
				DBGLOG("NISPlugin::CopyMapResults got an error: %s\n", resultPtr );
				free( resultPtr );
				resultPtr = NULL;

				if ( mapNameRef )
					CFRelease( mapNameRef );
				
				if ( newMapResult )
					CFRelease( newMapResult );
					
				return NULL;
			}
			
			char*				curPtr = resultPtr;
			char*				eoln = NULL;
			char*				key = NULL;
			char*				value = NULL;
			CFMutableSetRef		dupFilterSetRef = CFSetCreateMutable( NULL, 0, &kCFCopyStringSetCallBacks );
			
			while ( curPtr && curPtr[0] != '\0' )
			{
				key = curPtr;
				
				eoln = strstr( curPtr, "\n" );
				
				if ( !eoln )
					eoln = curPtr + strlen(curPtr);
				
				curPtr = strstr( curPtr, " " );	// advance to the space
				
				if ( curPtr && curPtr < eoln )
				{
					*curPtr = '\0';
				
					curPtr++;
					value = curPtr;
				
					curPtr = strstr( curPtr, "\n" ); // advance to eoln
				}
				else
					curPtr = eoln;
					
				if ( curPtr )
				{
					curPtr[0] = '\0';			
					curPtr++;
				}
				
				DBGLOG( "NISPlugin::CopyMapResults key is: %s, value is: %s\n", key, value );
				if ( key && value )
				{
					CFMutableStringRef		dupValueCheckRef = CFStringCreateMutable( NULL, 0 );
					
					if ( dupValueCheckRef )
					{
						CFStringAppendCString( dupValueCheckRef, key, kCFStringEncodingUTF8 );
						CFStringAppendCString( dupValueCheckRef, value, kCFStringEncodingUTF8 );
						
						if ( CFSetContainsValue( dupFilterSetRef, dupValueCheckRef ) )
						{
							CFRelease( dupValueCheckRef );

							DBGLOG( "NISPlugin::CopyMapResults filtering duplicate result key is: %s, val is: %s\n", key, value );
							continue;
						}
					
						CFMutableDictionaryRef		resultRef = CreateParseResult( value, mname );
			
						if ( resultRef )
						{
							if ( dupValueCheckRef )
								CFSetAddValue( dupFilterSetRef, dupValueCheckRef );
							
							CFStringRef		recordName = (CFStringRef)CFDictionaryGetValue(resultRef, CFSTR(kDSNAttrRecordName));
							
							if ( recordName && CFGetTypeID(recordName) == CFArrayGetTypeID() )
								recordName = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)recordName, 0 );
							
							if ( recordName && CFGetTypeID(recordName) == CFStringGetTypeID())
								CFDictionaryAddValue( newMapResult, recordName, resultRef );
							else
								DBGLOG( "NISPlugin::CopyMapResults resultRef had no value %s\n", kDSNAttrRecordName );
						
							CFRelease( resultRef );
						}
						else
							DBGLOG( "NISPlugin::CopyMapResults no result\n" );
					
						CFRelease( dupValueCheckRef );
					}
					else
						DBGLOG( "NISPlugin::CopyMapResults, couldn't make a CFString out of the value (%s)!!!\n", value );
				}
			}
	
			if ( dupFilterSetRef )
			{
				CFRelease( dupFilterSetRef );
			}
			
			free( resultPtr );
			resultPtr = NULL;
		}
		
		mapResults = newMapResult;

		if ( mapNameRef )
		{
			LockMapCache();

			CFDictionarySetValue( mCachedMapsRef, mapNameRef, mapResults );					// only set this if we did a full cat lookup

			UnlockMapCache();
		}
		else
			DBGLOG( "NISPlugin::CopyMapResults couldn't set the latest cached map in mCachedMapsRef since we couldn't make a mapNameRef!\n" ); 
	}
	
	if ( mapNameRef )
		CFRelease( mapNameRef );
		
	DBGLOG( "NISPlugin::CopyMapResults mapResults has %ld members\n", CFDictionaryGetCount(mapResults) );
	
	return mapResults;
}

Boolean NISPlugin::RecordIsAMatch( CFDictionaryRef recordRef, tDirPatternMatch inAttributePatternMatch, tDataNodePtr inAttributePatt2Match )
{
	Boolean		isAMatch = false;
	
	if ( inAttributePatternMatch == eDSAnyMatch )
		isAMatch = true;
	else if ( inAttributePatt2Match )
	{
		CFStringRef attrPatternRef = CFStringCreateWithCString( NULL, inAttributePatt2Match->fBufferData, kCFStringEncodingUTF8 );

		if ( attrPatternRef )
		{
			AttrDataMatchContext		context = { inAttributePatternMatch, attrPatternRef, false };
			
			CFDictionaryApplyFunction( recordRef, FindAttributeMatch, &context );
			
			if ( context.foundAMatch )
			{
				isAMatch = true;
			}
			else
				DBGLOG( "NISPlugin::RecordIsAMatch result didn't have the attribute: %s\n", inAttributePatt2Match->fBufferData );

			CFRelease( attrPatternRef );
		}
		else
			DBGLOG( "NISPlugin::RecordIsAMatch couldn't make a UTF8 string out of: %s!\n", inAttributePatt2Match->fBufferData );
	}
	
	return isAMatch;
}

CFDictionaryRef NISPlugin::CopyRecordResult( char* mname, char* recordName )
{	
	CFStringRef		mapNameRef = CFStringCreateWithCString( NULL, mname, kCFStringEncodingUTF8 );
	CFStringRef		recordNameRef = CFStringCreateWithCString( NULL, recordName, kCFStringEncodingUTF8 );
	CFDictionaryRef	returnRecordRef = NULL;
	
	if ( !recordNameRef || !mapNameRef )
	{
		DBGLOG( "NISPlugin::CopyRecordResult couldn't convert to CFStringRef! recordNameRef: 0x%x, mapNameRef: 0x%x\n", (int)recordNameRef, (int)mapNameRef );

		if ( mapNameRef )
			CFRelease( mapNameRef );
		
		if ( recordNameRef )
			CFRelease( recordNameRef );
			
		return NULL;
	}
	
	LockMapCache();

	CFDictionaryRef		cachedMapRef = (CFDictionaryRef)CFDictionaryGetValue( mCachedMapsRef, mapNameRef );
	
	if ( cachedMapRef )
	{
		returnRecordRef = (CFDictionaryRef)CFDictionaryGetValue( cachedMapRef, recordNameRef );
		
		if ( returnRecordRef )
		{
			CFRetain( returnRecordRef );
			DBGLOG( "NISPlugin::CopyRecordResult returning cached result\n" );
		}
	}
	
	UnlockMapCache();

	if ( !returnRecordRef )
	{
		char*		resultPtr = CopyResultOfLookup( (recordName)?kNISypmatch:kNISypcat, mname, recordName );
		
		if ( resultPtr )
		{
			if ( strstr( resultPtr, kBindErrorString ) && !mWeLaunchedYPBind )
			{
				// it looks like ypbind may not be running, lets take a look and run it ourselves if need be.
				DBGLOG( "NISPlugin::CopyRecordResult got an error, implying that ypbind may not be running\n" );
				
				free( resultPtr );
				resultPtr = NULL;
				
				DBGLOG( "NISPlugin::CopyRecordResult attempting to launch ypbind\n" );
				mWeLaunchedYPBind = true;
				resultPtr = CopyResultOfLookup( kNISbind );			
			
				if ( resultPtr )
				{
					free( resultPtr );
					resultPtr = NULL;
				}
			
				// now try again.
				resultPtr = CopyResultOfLookup( (recordName)?kNISypmatch:kNISypcat, mname, recordName );
			}
			
			if ( strncmp( resultPtr, "No such map", strlen("No such map") ) != 0 && strncmp( resultPtr, "Can't match key", strlen("Can't match key") ) != 0 )
			{
				char*					curPtr = resultPtr;
				char*					eoln = NULL;
				char*					key = NULL;
				char*					value = NULL;
				CFMutableSetRef			dupFilterSetRef = CFSetCreateMutable( NULL, 0, &kCFCopyStringSetCallBacks );
				CFMutableDictionaryRef	mapDictionaryRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
				
				while ( curPtr && curPtr[0] != '\0' )
				{
					key = curPtr;
					
					eoln = strstr( curPtr, "\n" );
					
					if ( !eoln )
						eoln = curPtr + strlen(curPtr);
					
					curPtr = strstr( curPtr, " " );	// advance to the space
					
					if ( curPtr && curPtr < eoln )
					{
						*curPtr = '\0';
					
						curPtr++;
						value = curPtr;
					
						curPtr = strstr( curPtr, "\n" ); // advance to eoln
					}
					else
						curPtr = eoln;
						
					if ( curPtr )
					{
						curPtr[0] = '\0';			
						curPtr++;
					}
					
					DBGLOG( "NISPlugin::CopyRecordResult key is: %s, value is: %s\n", key, value );
					if ( key && value )
					{
						if ( recordName && strstr( recordName, key ) == NULL )
						{
							DBGLOG( "NISPlugin::CopyRecordResult key is: %s, looking for recordName: %s so skipping\n", key, recordName );
							continue;
						}
							
						CFStringRef		dupValueCheckRef = CFStringCreateWithCString( NULL, value, kCFStringEncodingUTF8 );
						
						if ( dupValueCheckRef )
						{
							if ( CFSetContainsValue( dupFilterSetRef, dupValueCheckRef ) )
							{
								CFRelease( dupValueCheckRef );
								DBGLOG( "NISPlugin::CopyRecordResult filtering duplicate result key is: %s, val is: %s\n", key, value );
								continue;
							}
						
							CFMutableDictionaryRef		resultRef = CreateParseResult( value, mname );
			
							if ( resultRef )
							{
								if ( dupValueCheckRef )
									CFSetAddValue( dupFilterSetRef, dupValueCheckRef );
								
								CFStringRef	resultRecordName = (CFStringRef)CFDictionaryGetValue(resultRef, CFSTR(kDSNAttrRecordName));
								if ( resultRecordName )
									CFDictionaryAddValue( mapDictionaryRef, resultRecordName, resultRef );
							
								CFRelease( resultRef );
							}
							else
								DBGLOG( "NISPlugin::CopyRecordResult no result\n" );
						
							CFRelease( dupValueCheckRef );
						}
						else
							DBGLOG( "NISPlugin::CopyRecordResult, couldn't make a CFString out of the value (%s)!!!\n", value );
					}
				}
		
				if ( dupFilterSetRef )
				{
					CFRelease( dupFilterSetRef );
				}
					
				LockMapCache();
				if ( !recordName )
					CFDictionarySetValue( mCachedMapsRef, mapNameRef, mapDictionaryRef );					// only set this if we did a full cat lookup
	
				UnlockMapCache();
				
				returnRecordRef = (CFDictionaryRef)CFDictionaryGetValue( mapDictionaryRef, recordNameRef );
				
				if ( returnRecordRef )
				{
					CFRetain( returnRecordRef );
				}
				
				CFRelease( mapDictionaryRef );
			}
			else
			{
				DBGLOG( "NISPlugin::CopyRecordResult got an error: %s\n", resultPtr );
			}

			free( resultPtr );
			resultPtr = NULL;
		}
		
		if ( returnRecordRef )
			DBGLOG( "NISPlugin::CopyRecordResult returnRecordRef has %ld members\n", CFDictionaryGetCount(returnRecordRef) );
		else
			DBGLOG( "NISPlugin::CopyRecordResult no returnRecordRef\n" );
	}
	
	if ( mapNameRef )
		CFRelease( mapNameRef );
	
	if ( recordNameRef )
		CFRelease( recordNameRef );
		
	return returnRecordRef;
}

CFMutableDictionaryRef NISPlugin::CreateParseResult(char *data, const char* mname)
{
	if (data == NULL) return NULL;
	if (data[0] == '#') return NULL;

	CFMutableDictionaryRef	returnResult = NULL;
	
	if ( strcmp( mname, "passwd.byname" ) == 0 )
		returnResult = ff_parse_user(data);
	else if ( strcmp( mname, "group.byname" ) == 0 )
		returnResult = ff_parse_group(data);
	else if ( strcmp( mname, "hosts.byname" ) == 0 )
		returnResult = ff_parse_host(data);
	else if ( strcmp( mname, "networks.byname" ) == 0 )
		returnResult = ff_parse_network(data);
	else if ( strcmp( mname, "services.byname" ) == 0 )
		returnResult = ff_parse_service(data);
	else if ( strcmp( mname, "protocols.byname" ) == 0 )
		returnResult = ff_parse_protocol(data);
	else if ( strcmp( mname, "rpc.byname" ) == 0 )
		returnResult = ff_parse_rpc(data);
	else if ( strcmp( mname, "mounts.byname" ) == 0 )
		returnResult = ff_parse_mount(data);
	else if ( strcmp( mname, "printcap.byname" ) == 0 )
		returnResult = ff_parse_printer(data);
	else if ( strcmp( mname, "bootparams.byname" ) == 0 )
		returnResult = ff_parse_bootparam(data);
	else if ( strcmp( mname, "bootptab.byaddr" ) == 0 )
		returnResult = ff_parse_bootp(data);
	else if ( strcmp( mname, "mail.aliases" ) == 0 )
		ff_parse_alias(data);
	else if ( strcmp( mname, "ethers.byname" ) == 0 )
		returnResult = ff_parse_ethernet(data);
	else if ( strcmp( mname, "netgroup" ) == 0 )
		returnResult = ff_parse_netgroup(data);
	
	if ( returnResult )
	{
		CFDictionarySetValue( returnResult, CFSTR(kDSNAttrMetaNodeLocation), mMetaNodeLocationRef );
	}
		
	return returnResult;
}

#pragma mark -


//------------------------------------------------------------------------------------
//	* DoAuthentication
//------------------------------------------------------------------------------------

sInt32 NISPlugin::DoAuthentication ( sDoDirNodeAuth *inData )
{
	DBGLOG( "NISPlugin::DoAuthentication fInAuthMethod: %s, fInDirNodeAuthOnlyFlag: %d, fInAuthStepData: %d, fOutAuthStepDataResponse: %d, fIOContinueData: %d\n", (char *)inData->fInAuthMethod->fBufferData, inData->fInDirNodeAuthOnlyFlag, (int)inData->fInAuthStepData, (int)inData->fOutAuthStepDataResponse, (int)inData->fIOContinueData );

    NISDirNodeRep*			nodeDirRep		= NULL;
	sInt32					siResult		= eDSAuthFailed;
	uInt32					uiAuthMethod	= 0;
	sNISContinueData	   *pContinueData	= NULL;
	char*					userName		= NULL;
	
	try
	{
		LockPlugin();
	
		nodeDirRep = (NISDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );

	
		if( !nodeDirRep )
		{
			DBGLOG( "NISPlugin::DoAuthentication called but we couldn't find the nodeDirRep!\n" );
			UnlockPlugin();
			return eDSInvalidNodeRef;
		}

		nodeDirRep->Retain();	// retain while we are using this
		
		UnlockPlugin();
	
		if ( nodeDirRep == nil ) throw( (sInt32)eDSInvalidNodeRef );
		if ( inData->fInAuthStepData == nil ) throw( (sInt32)eDSNullAuthStepData );
		
		if ( inData->fIOContinueData != NULL )
		{
			// get info from continue
			pContinueData = (sNISContinueData *)inData->fIOContinueData;

			siResult = DoBasicAuth(inData->fInNodeRef, inData->fInAuthMethod, nodeDirRep,
										&pContinueData, inData->fInAuthStepData, 
										inData->fOutAuthStepDataResponse, 
										inData->fInDirNodeAuthOnlyFlag, 
										pContinueData->fAuthAuthorityData);
		}
		else
		{
			// first call
			siResult = GetAuthMethod( inData->fInAuthMethod, &uiAuthMethod );
			if ( siResult == eDSNoErr )
			{
				if ( uiAuthMethod != kAuth2WayRandom )
				{
					siResult = GetUserNameFromAuthBuffer( inData->fInAuthStepData, 1, &userName );
					if ( siResult != eDSNoErr ) throw( siResult );
				}
				else
				{
					DBGLOG( "NISPlugin::DoAuthentication we don't support 2 way random!\n" );
					siResult = eDSAuthMethodNotSupported;
				}

				// get the auth authority
				char*		mapName = CreateNISTypeFromRecType( kDSStdRecordTypeUsers );
				
				CFMutableDictionaryRef	recordResult = CopyRecordLookup( mapName, userName );
				
				if ( recordResult )
				{
					siResult = DoBasicAuth(inData->fInNodeRef,inData->fInAuthMethod, nodeDirRep, 
											&pContinueData, inData->fInAuthStepData,
											inData->fOutAuthStepDataResponse,
											inData->fInDirNodeAuthOnlyFlag,NULL);
				
					CFRelease( recordResult );
				}
				else
				{
					DBGLOG( "NISPlugin::DoAuthentication, unknown user!\n" );
					siResult = eDSAuthUnknownUser;
				}
				
				if ( mapName )
					free( mapName );
			}
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}
	
	if (userName != NULL)
	{
		free(userName);
		userName = NULL;
	}

	if ( nodeDirRep )
		nodeDirRep->Release();		// done
		
	inData->fResult = siResult;
	inData->fIOContinueData = pContinueData;

	return( siResult );

} // DoAuthentication

//------------------------------------------------------------------------------------
//	* DoBasicAuth
//------------------------------------------------------------------------------------

sInt32 NISPlugin::DoBasicAuth ( tDirNodeReference inNodeRef, tDataNodePtr inAuthMethod, 
								NISDirNodeRep* nodeDirRep, 
								sNISContinueData** inOutContinueData, 
								tDataBufferPtr inAuthData, tDataBufferPtr outAuthData, 
								bool inAuthOnly, char* inAuthAuthorityData )
{
	sInt32				siResult		= eDSNoErr;
	uInt32				uiAuthMethod	= 0;

	DBGLOG( "NISPlugin::DoBasicAuth\n" );
	
	try
	{
		if ( nodeDirRep == nil ) throw( (sInt32)eDSInvalidNodeRef );

		siResult = GetAuthMethod( inAuthMethod, &uiAuthMethod );
		if ( siResult == eDSNoErr )
		{
			switch( uiAuthMethod )
			{
				//native auth is always UNIX crypt possibly followed by 2-way random using tim auth server
				case kAuthNativeMethod:
				case kAuthNativeNoClearText:
				case kAuthNativeClearTextOK:
					if ( outAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );
					siResult = DoUnixCryptAuth( nodeDirRep, inAuthData, inAuthOnly );
					if ( siResult == eDSNoErr )
					{
						if ( outAuthData->fBufferSize > ::strlen( kDSStdAuthCrypt ) )
						{
							::strcpy( outAuthData->fBufferData, kDSStdAuthCrypt );
						}
					}

					break;

				case kAuthClearText:
				case kAuthCrypt:
					siResult = DoUnixCryptAuth( nodeDirRep, inAuthData, inAuthOnly );
					break;
/*
				case kAuthSetPasswd:
					siResult = DoSetPassword( inContext, inAuthData );
					break;

				case kAuthSetPasswdAsRoot:
					siResult = DoSetPasswordAsRoot( inContext, inAuthData );
					break;

				case kAuthChangePasswd:
					siResult = DoChangePassword( inContext, inAuthData );
					break;
*/
				default:
					siResult = eDSAuthFailed;
			}
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
		
		DBGLOG( "NISPlugin::DoBasicAuth caught exception: %ld\n", siResult );
	}

	return( siResult );

} // DoBasicAuth

//------------------------------------------------------------------------------------
//	* DoUnixCryptAuth
//------------------------------------------------------------------------------------

sInt32 NISPlugin::DoUnixCryptAuth ( NISDirNodeRep *nodeDirRep, tDataBuffer *inAuthData, bool inAuthOnly )
{
	sInt32			siResult		= eDSAuthFailed;
	char		   *pData			= nil;
	char		   *nisPwd			= nil;
	char		   *name			= nil;
	uInt32			nameLen			= 0;
	char		   *pwd				= nil;
	uInt32			pwdLen			= 0;
	uInt32			offset			= 0;
	uInt32			buffSize		= 0;
	uInt32			buffLen			= 0;
	char			salt[ 9 ];
	char			hashPwd[ 32 ];
	CFMutableDictionaryRef	recordResult = NULL;
	char*			mapName			= NULL;
	
	DBGLOG( "NISPlugin::DoUnixCryptAuth\n" );
	try
	{
#ifdef DEBUG
		if ( inAuthData == nil ) throw( (sInt32)eDSAuthParameterError );
#else
		if ( inAuthData == nil ) throw( (sInt32)eDSAuthFailed );
#endif

		pData		= inAuthData->fBufferData;
		buffSize	= inAuthData->fBufferSize;
		buffLen		= inAuthData->fBufferLength;

		if (buffLen > buffSize) throw( (sInt32)eDSInvalidBuffFormat );

		if (offset + (2 * sizeof( unsigned long) + 1) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );
		// need username length, password length, and username must be at least 1 character
		::memcpy( &nameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		
#ifdef DEBUG
		if (nameLen == 0) throw( (sInt32)eDSAuthUnknownUser );
		if (nameLen > 256) throw( (sInt32)eDSInvalidBuffFormat );
		if (offset + nameLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );
#else
		if (nameLen == 0) throw( (sInt32)eDSAuthFailed );
		if (nameLen > 256) throw( (sInt32)eDSAuthFailed );
		if (offset + nameLen > buffLen) throw( (sInt32)eDSAuthFailed );
#endif

		name = (char *)::calloc( nameLen + 1, sizeof( char ) );
		::memcpy( name, pData, nameLen );
		pData += nameLen;
		offset += nameLen;

		DBGLOG( "NISPlugin::DoUnixCryptAuth attempting UNIX Crypt authentication\n" );

		mapName = CreateNISTypeFromRecType( kDSStdRecordTypeUsers );
				
		recordResult = CopyRecordLookup( mapName, name );
		
		if ( !recordResult )
		{
			DBGLOG( "NISPlugin::DoUnixCryptAuth, unknown user!\n" );
			throw( (sInt32)eDSAuthUnknownUser );
		}
			
		CFStringRef nisPwdRef = (CFStringRef)CFDictionaryGetValue( recordResult, CFSTR(kDS1AttrPassword) );
		
		if ( nisPwdRef )
		{
			CFIndex		len = ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(nisPwdRef), kCFStringEncodingUTF8) + 1;
			
			nisPwd = (char*)malloc( len );	// GetLength returns 16 bit value...
			CFStringGetCString( nisPwdRef, nisPwd, len, kCFStringEncodingUTF8 );
		}
		else
		{
			nisPwd = (char*)""; // empty string, we are not freeing it so direct assignment is OK
		}

#ifdef DEBUG
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );
#else
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSAuthFailed );
#endif
		::memcpy( &pwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		
#ifdef DEBUG
		if (offset + pwdLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );
		if (pwdLen > 256) throw( (sInt32)eDSAuthFailed );
#else
		if (offset + pwdLen > buffLen) throw( (sInt32)eDSAuthFailed );
		if (pwdLen > 256) throw( (sInt32)eDSAuthFailed );
#endif
		pwd = (char *)::calloc( pwdLen + 1, sizeof( char ) );
		::memcpy( pwd, pData, pwdLen );

		//account for the case where nisPwd == "" such that we will auth if pwdLen is 0
		if (::strcmp(nisPwd,"") != 0)
		{
			salt[ 0 ] = nisPwd[0];
			salt[ 1 ] = nisPwd[1];
			salt[ 2 ] = '\0';

			::memset( hashPwd, 0, 32 );
			::strcpy( hashPwd, ::crypt( pwd, salt ) );

			siResult = eDSAuthFailed;
			if ( ::strcmp( hashPwd, nisPwd ) == 0 )
			{
				siResult = eDSNoErr;
			}
			
		}
		else // nisPwd is == ""
		{
			if ( ::strcmp(pwd,"") == 0 )
			{
				siResult = eDSNoErr;
			}
		}
	}

	catch( sInt32 err )
	{
		DBGLOG( "NISPlugin::DoUnixCryptAuth Crypt authentication error %ld\n", err );
		siResult = err;
	}

	if ( mapName )
		free( mapName );

	if ( name != nil )
	{
		free( name );
		name = nil;
	}

	if ( pwd != nil )
	{
		free( pwd );
		pwd = nil;
	}
	
	if ( nisPwd && *nisPwd != '\0' )
		free( nisPwd );

	if ( recordResult )
	{
		CFRelease( recordResult );
	}	
	return( siResult );

} // DoUnixCryptAuth

// ---------------------------------------------------------------------------
//	* GetAuthMethod
// ---------------------------------------------------------------------------

sInt32 NISPlugin::GetAuthMethod ( tDataNode *inData, uInt32 *outAuthMethod )
{
	sInt32			siResult		= eDSNoErr;
	uInt32			uiNativeLen		= 0;
	char		   *p				= nil;

	if ( inData == nil )
	{
		*outAuthMethod = kAuthUnknowMethod;
#ifdef DEBUG
		return( eDSAuthParameterError );
#else
		return( eDSAuthFailed );
#endif
	}

	p = (char *)inData->fBufferData;

	DBGLOG( "NISPlugin::GetAuthMethod using authentication method %s\n", p );

	if ( ::strcmp( p, kDSStdAuthClearText ) == 0 )
	{
		// Clear text auth method
		*outAuthMethod = kAuthClearText;
	}
	else if ( ::strcmp( p, kDSStdAuthNodeNativeClearTextOK ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeClearTextOK;
	}
	else if ( ::strcmp( p, kDSStdAuthNodeNativeNoClearText ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeNoClearText;
	}
	else if ( ::strcmp( p, kDSStdAuthCrypt ) == 0 )
	{
		// Unix Crypt auth method
		*outAuthMethod = kAuthCrypt;
	}
	else if ( ::strcmp( p, kDSStdAuth2WayRandom ) == 0 )
	{
		// Two way random auth method
		*outAuthMethod = kAuth2WayRandom;
	}
	else if ( ::strcmp( p, kDSStdAuthSMB_NT_Key ) == 0 )
	{
		// Two way random auth method
		*outAuthMethod = kAuthNIS_NT_Key;
	}
	else if ( ::strcmp( p, kDSStdAuthSMB_LM_Key ) == 0 )
	{
		// Two way random auth method
		*outAuthMethod = kAuthNIS_LM_Key;
	}
	else if ( ::strcmp( p, kDSStdAuthSetPasswd ) == 0 )
	{
		// Admin set password
		*outAuthMethod = kAuthSetPasswd;
	}
	else if ( ::strcmp( p, kDSStdAuthSetPasswdAsRoot ) == 0 )
	{
		// Admin set password
		*outAuthMethod = kAuthSetPasswdAsRoot;
	}
	else if ( ::strcmp( p, kDSStdAuthChangePasswd ) == 0 )
	{
		// User change password
		*outAuthMethod = kAuthChangePasswd;
	}
	else if ( ::strcmp( p, kDSStdAuthAPOP ) == 0 )
	{
		// APOP auth method
		*outAuthMethod = kAuthAPOP;
	}
	else if ( ::strcmp( p, kDSStdAuthCRAM_MD5 ) == 0 )
	{
		// CRAM-MD5 auth method
		*outAuthMethod = kAuthCRAM_MD5;
	}
	else
	{
		uiNativeLen	= ::strlen( kDSNativeAuthMethodPrefix );

		if ( ::strncmp( p, kDSNativeAuthMethodPrefix, uiNativeLen ) == 0 )
		{
			// User change password
			*outAuthMethod = kAuthNativeMethod;
		}
		else
		{
			*outAuthMethod = kAuthUnknowMethod;
#ifdef DEBUG
			siResult = eDSAuthMethodNotSupported;
#else
			siResult = eDSAuthFailed;
#endif
		}
	}

	return( siResult );

} // GetAuthMethod

// ---------------------------------------------------------------------------
//	* GetUserNameFromAuthBuffer
//    retrieve the username from a standard auth buffer
//    buffer format should be 4 byte length followed by username, then optional
//    additional data after. Buffer length must be at least 5 (length + 1 character name)
// ---------------------------------------------------------------------------

sInt32 NISPlugin::GetUserNameFromAuthBuffer ( tDataBufferPtr inAuthData, unsigned long inUserNameIndex, 
											  char  **outUserName )
{
	DBGLOG( "NISPlugin::GetUserNameFromAuthBuffer\n" );
	tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
	if (dataList != NULL)
	{
		*outUserName = dsDataListGetNodeStringPriv(dataList, inUserNameIndex);
		// this allocates a copy of the string
		
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;

		DBGLOG( "NISPlugin::GetUserNameFromAuthBuffer successfully returning username: %s\n", *outUserName );
		
		return eDSNoErr;
	}
	return eDSInvalidBuffFormat;
}

sInt32 NISPlugin::VerifyPatternMatch ( const tDirPatternMatch inPatternMatch )
{
	DBGLOG( "NISPlugin::VerifyPatternMatch\n" );
	sInt32		siResult = eDSNoErr;

	switch ( inPatternMatch )
	{
		case eDSExact:
		case eDSStartsWith:
		case eDSEndsWith:
		case eDSContains:
		//case eDSLessThan:
		//case eDSGreaterThan:
		//case eDSLessEqual:
		//case eDSGreaterEqual:
		//case eDSWildCardPattern:
		//case eDSRegularExpression:
		case eDSiExact:
		//case eDSiStartsWith:
		//case eDSiEndsWith:
		case eDSiContains:
		//case eDSiLessThan:
		//case eDSiGreaterThan:
		//case eDSiLessEqual:
		//case eDSiGreaterEqual:
		//case eDSiWildCardPattern:
		//case eDSiRegularExpression:
		case eDSAnyMatch:
			siResult = eDSNoErr;
			break;

		default:
			siResult = eDSInvalidPatternMatchType;
			break;
	}


	return( siResult );

} // VerifyPatternMatch


#pragma mark

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

#pragma mark -
// ---------------------------------------------------------------------------
//	* MakeContextData
// ---------------------------------------------------------------------------

sNISContextData* MakeContextData ( void )
{
    sNISContextData   *pOut		= nil;
    sInt32			siResult	= eDSNoErr;

    pOut = (sNISContextData *) calloc(1, sizeof(sNISContextData));
    if ( pOut != nil )
    {
//        ::memset( pOut, 0, sizeof( sNISContextData ) );
        //do nothing with return here since we know this is new
        //and we did a memset above
        siResult = CleanContextData(pOut);
    }

    return( pOut );

} // MakeContextData

// ---------------------------------------------------------------------------
//	* CleanContextData
// ---------------------------------------------------------------------------

sInt32 CleanContextData ( sNISContextData *inContext )
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

UInt32 GetCurrentTime( void )			// in seconds
{
    struct	timeval curTime;
    
    if ( gettimeofday( &curTime, NULL ) != 0 )
        fprintf( stderr, "call to gettimeofday returned error: %s", strerror(errno) );
    
    return curTime.tv_sec;
}

Boolean IsResultOK( tDirPatternMatch patternMatch, CFStringRef resultRecordNameRef, CFStringRef recNameMatchRef )
{
	Boolean resultIsOK = false;
	
	if ( patternMatch == eDSAnyMatch )
	{
		resultIsOK = true;
	}
	else if ( !resultRecordNameRef || !recNameMatchRef )	// double check validity of values
	{
		resultIsOK = false;
	}
	else if ( patternMatch == eDSExact && CFStringCompare( resultRecordNameRef, recNameMatchRef, 0 ) == kCFCompareEqualTo )
	{
		resultIsOK = true;
	}
	else if ( patternMatch >= eDSiExact && CFStringCompare( resultRecordNameRef, recNameMatchRef, kCFCompareCaseInsensitive ) == kCFCompareEqualTo )
	{
		resultIsOK = true;
	}
	else if ( patternMatch == eDSStartsWith && CFStringHasPrefix( resultRecordNameRef, recNameMatchRef ) )
	{
		resultIsOK = true;
	}
	else if ( patternMatch == eDSEndsWith && CFStringHasSuffix( resultRecordNameRef, recNameMatchRef ) )
	{
		resultIsOK = true;
	}
	else if ( patternMatch == eDSContains )
	{
		CFRange 	result = CFStringFind( resultRecordNameRef, recNameMatchRef, 0 );
		
		if ( result.length > 0 )
			resultIsOK = true;
	}
	else if ( patternMatch == eDSiContains )
	{
		CFRange 	result = CFStringFind( resultRecordNameRef, recNameMatchRef, kCFCompareCaseInsensitive );
		
		if ( result.length > 0 )
			resultIsOK = true;
	}
	
	return resultIsOK;
}


void AddDictionaryDataToAttrData(const void *key, const void *value, void *context)
{
    CFStringRef			keyRef			= (CFStringRef)key;
    CFTypeRef			valueTypeRef	= (CFTypeRef)value;		// need to update this to support an array of values instead
    CFStringRef			valueRef		= NULL;
	CFArrayRef			valueArrayRef	= NULL;
    char				keyBuf[256] = {0};
    char*				valueBuf	= NULL;
	CDataBuff*			aTmpData	= nil;
    AttrDataContext*	dataContext	= (AttrDataContext*)context;
    
	DBGLOG( "AddDictionaryDataToAttrData called with key: 0x%x, value 0x%x\n", (int)key, (int)value );
	
	if ( dataContext->attrType && strcmp( dataContext->attrType, kDSAttributesStandardAll ) == 0 )
	{
		if ( CFStringHasPrefix( keyRef, CFSTR(kDSAttributesStandardAll) ) )
		{
			DBGLOG( "AddDictionaryDataToAttrData ignoring type as it isn't a standard attribute\n" );
			return;
		}
	}
	else if ( dataContext->attrType && strcmp( dataContext->attrType, kDSAttributesNativeAll ) == 0 )
	{
		if ( CFStringHasPrefix( keyRef, CFSTR(kDSAttributesNativeAll) ) )
		{
			DBGLOG( "AddDictionaryDataToAttrData ignoring type as it isn't a native attribute\n" );
			return;
		}
	}

    dataContext->count++;		// adding one more
    
    aTmpData = new CDataBuff();
    if( !aTmpData )  throw( eMemoryError );
    
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
            DBGLOG( "AddDictionaryDataToAttrData, got unknown value type (%ld), ignore\n", CFGetTypeID(valueTypeRef) );

		if ( valueArrayRef && ::CFStringGetCString( keyRef, keyBuf, sizeof(keyBuf), kCFStringEncodingUTF8 ) )
		{
			aTmpData->AppendShort( ::strlen( keyBuf ) );		// attrTypeLen
			aTmpData->AppendString( keyBuf );					// attrType
			
			DBGLOG( "AddDictionaryDataToAttrData, adding keyBuf: %s\n", keyBuf );

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
						
						DBGLOG( "AddDictionaryDataToAttrData, adding valueBuf: %s\n", valueBuf );
					}
					else
					{
						DBGLOG( "AddDictionaryDataToAttrData, CFStringGetCString couldn't create a string for valueRef!\n" );
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
		{
			CFRelease( valueArrayRef );
		}
	}
}

void FindAttributeMatch(const void *key, const void *value, void *context)
{
    CFTypeRef				valueRef		= (CFTypeRef)value;
	CFArrayRef				valueArrayRef	= NULL;
	AttrDataMatchContext*	info			= (AttrDataMatchContext*)context;
	
	if ( info->foundAMatch )
		return;
		
	if ( CFGetTypeID(valueRef) == CFArrayGetTypeID() )
	{
		// just point our valueArrayRef at this
		valueArrayRef = (CFArrayRef)valueRef;
		CFRetain( valueArrayRef );					// so we can release this
	}
	else if ( CFGetTypeID(valueRef) == CFStringGetTypeID() )
	{
		valueArrayRef = CFArrayCreateMutable( NULL, 1, &kCFTypeArrayCallBacks );
		CFArrayAppendValue( (CFMutableArrayRef)valueArrayRef, valueRef );
	}
	else
	{
		DBGLOG( "FindAttributeMatch, got unknown value type (%ld), ignore\n", CFGetTypeID(valueRef) );
		CFShow( valueRef );
	}
	
	for ( CFIndex i=0; valueArrayRef && i< CFArrayGetCount(valueArrayRef) && !info->foundAMatch; i++ )
	{
		CFStringRef valueStringRef = (CFStringRef)::CFArrayGetValueAtIndex( valueArrayRef, i );

		if ( IsResultOK( info->fInPattMatchType, valueStringRef, info->fInPatt2MatchRef ) )
			info->foundAMatch = true;
	}
	
	if ( valueArrayRef )
	{
		CFRelease( valueArrayRef );
	}
}

void FindAllAttributeMatches(const void *key, const void *value, void *context)
{
    CFTypeRef				valueRef		= (CFTypeRef)value;
	CFDictionaryRef			resultRef		= (CFDictionaryRef)value;
	ResultMatchContext*		info			= (ResultMatchContext*)context;
	Boolean					recordMatches	= false;
	
	if ( info->fInPattMatchType == eDSAnyMatch )
	{
		recordMatches = true;
	}
	else if ( CFGetTypeID(resultRef) == CFDictionaryGetTypeID() )
	{
		AttrDataMatchContext		context = { info->fInPattMatchType, info->fInPatt2MatchRef, false };
		
		CFDictionaryApplyFunction( resultRef, FindAttributeMatch, &context );
		
		if ( context.foundAMatch )
			recordMatches = true;
	}
	else
		DBGLOG( "FindAllAttributeMatches, got unknown value type (%ld), ignore\n", CFGetTypeID(valueRef) );

	if ( recordMatches )
		CFArrayAppendValue( info->fResultArray, resultRef );
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
	*charPtr = '\0';					// and a terminating NULL
	
	DBGLOG( "%s\n",buf);
	free(buf);
	return noErr;
	
}
