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
 *  @header BSDPlugin
 */

#include "BSDHeaders.h"
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
#include <sys/stat.h>		// for file and dir stat calls

#include "crypt-md5.h"
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
	CFStringRef			fAttributeNameToMatchRef;
    Boolean				foundAMatch;
} AttrDataMatchContext;

typedef struct ResultMatchContext {
    tDirPatternMatch	fInPattMatchType;
	CFStringRef			fInPatt2MatchRef;
	CFStringRef			fAttributeNameToMatchRef;
    CFMutableArrayRef	fResultArray;
} ResultMatchContext;

			

Boolean IsResultOK( tDirPatternMatch patternMatch, CFStringRef resultRecordNameRef, CFStringRef recNameMatchRef );
void AddDictionaryDataToAttrData(const void *key, const void *value, void *context);
void FindAttributeMatch(const void *key, const void *value, void *context);
void FindAllAttributeMatches(const void *key, const void *value, void *context);

pthread_mutex_t	gOutstandingSearchesLock = PTHREAD_MUTEX_INITIALIZER;

#define			kSkipYPCatOnRecordNamesFilePath			"/Library/Preferences/DirectoryService/.DisableNISCatSearchOnRecordNames"
Boolean			gSkipYPCatOnRecordNames  = false;		// if the computer has a file at /Library/Preferences/DirectoryService/.DisableNISCatSearchOnRecordNames we won't do ypcat queries

#define			kSkipYPCatOnDisplayNamesFilePath		"/Library/Preferences/DirectoryService/.DisableNISCatSearchOnDisplayNames"
Boolean			gDisableDisplayNameLookups  = false;		// if the computer has a file at /Library/Preferences/DirectoryService/.DisableNISCatSearchOnDisplayNames we won't do ypcat queries

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
#ifdef BUILDING_COMBO_PLUGIN
const char* kFFRecordTypeUsers			= "master.passwd";
const char* kFFRecordTypeGroups			= "group";
const char* kFFRecordTypeAlias			= "aliases";
const char* kFFRecordTypeBootp			= "bootptab";
const char* kFFRecordTypeEthernets		= "ethers";
const char* kFFRecordTypeHosts			= "hosts";
const char* kFFRecordTypeMounts			= "fstab";
const char* kFFRecordTypeNetGroups		= "netgroups";
const char* kFFRecordTypeNetworks		= "networks";
const char* kFFRecordTypePrintService	= "printcap";
const char* kFFRecordTypeProtocols		= "protocols";
const char* kFFRecordTypeRPC			= "rpc";
const char* kFFRecordTypeServices		= "services";
const char* kFFRecordTypeBootParams		= "bootparams";
#endif

const char* kNISRecordTypeUsers			= "passwd.byname";
const char* kNISRecordTypeUsersByUID	= "passwd.byuid";
const char* kNISRecordTypeGroups		= "group.byname";
const char* kNISRecordTypeGroupsByGID	= "group.bygid";
const char* kNISRecordTypeBootParams	= "bootparams.byname";
const char* kNISRecordTypeAlias			= "mail.aliases";
const char* kNISRecordTypeBootp			= "bootptab.byaddr";
const char* kNISRecordTypeEthernets		= "ethers.byname";
const char* kNISRecordTypeHosts			= "hosts.byname";
const char* kNISRecordTypeMounts		= "mounts.byname";
const char* kNISRecordTypeNetGroups		= "netgroups";
const char* kNISRecordTypeNetworks		= "networks.byname";
const char* kNISRecordTypePrintService	= "printcap.byname";
const char* kNISRecordTypeProtocols		= "protocols.byname";
const char* kNISRecordTypeRPC			= "rpc.byname";
const char* kNISRecordTypeServices		= "services.byname";

#define kDefaultDomainFilePath	"/Library/Preferences/DirectoryService/nisdomain"
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


const CFStringRef	gBundleIdentifier = CFSTR("com.apple.DirectoryService.BSD");

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0xCE, 0x63, 0xBE, 0x32, 0xF5, 0xB6, 0x11, 0xD6, \
								0x9D, 0xC0, 0x00, 0x03, 0x93, 0x4F, 0xB0, 0x10 );
}

const char*			gProtocolPrefixString = "BSD";

static CDSServerModule* _Creator ( void )
{
	DBGLOG( "Creating new BSD Plugin\n" );
    return( new BSDPlugin );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;


BSDPlugin::BSDPlugin( void )
{
	DBGLOG( "BSDPlugin::BSDPlugin\n" );
	mLocalNodeString = NULL;
	mNISMetaNodeLocationRef = NULL;

#ifdef BUILDING_COMBO_PLUGIN
	mFFMetaNodeLocationRef = NULL;
#endif

	mPublishedNodes = NULL;
	mOpenRecordsRef = NULL;
    mOpenRefTable = NULL;
	
	mLastTimeCacheReset = 0;
	mWeLaunchedYPBind = false;
    mLookupdIsAlreadyConfigured = false;
	
	mState		= kUnknownState;
	
	mCachedMapsRef = NULL;
#ifdef BUILDING_COMBO_PLUGIN
	mCachedFFRef = NULL;
	bzero( mModTimes, sizeof( mModTimes ) );
#endif
} // BSDPlugin


BSDPlugin::~BSDPlugin( void )
{
	DBGLOG( "BSDPlugin::~BSDPlugin\n" );
	
	if ( mOpenRecordsRef )
		CFRelease( mOpenRecordsRef );
	mOpenRecordsRef = NULL;
	
    if ( mLocalNodeString )
        free( mLocalNodeString );    
    mLocalNodeString = NULL;
	
	if ( mNISMetaNodeLocationRef )
		CFRelease( mNISMetaNodeLocationRef );
	mNISMetaNodeLocationRef = NULL;
	
#ifdef BUILDING_COMBO_PLUGIN
	if ( mFFMetaNodeLocationRef )
		CFRelease( mFFMetaNodeLocationRef );
	mFFMetaNodeLocationRef = NULL;
#endif
	
    if ( mOpenRefTable )
    {
        ::CFDictionaryRemoveAllValues( mOpenRefTable );
        ::CFRelease( mOpenRefTable );
        mOpenRefTable = NULL;
    }

} // ~BSDPlugin

// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

sInt32 BSDPlugin::Validate ( const char *inVersionStr, const uInt32 inSignature )
{
	mSignature = inSignature;

	return( noErr );

} // Validate


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

sInt32 BSDPlugin::Initialize( void )
{
    sInt32				siResult	= eDSNoErr;

	DBGLOG( "BSDPlugin::Initialize\n" );
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
#ifdef BUILDING_COMBO_PLUGIN
    pthread_mutex_init( &mFFCache, NULL );
#endif
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
	
#ifdef BUILDING_COMBO_PLUGIN
	ResetFFCache();
#endif
	ResetMapCache();
		
#ifdef BUILDING_COMBO_PLUGIN
	if ( !mFFMetaNodeLocationRef )
	{
		char		buf[1024];
		snprintf( buf, sizeof(buf), "%s%s", kProtocolPrefixSlashStr, kFFNodeName );
		
		mFFMetaNodeLocationRef = CFStringCreateWithCString( NULL, buf, kCFStringEncodingUTF8 );
	}
#endif

	struct stat				statResult;

	if ( stat( kSkipYPCatOnRecordNamesFilePath, &statResult ) == 0 )
		gSkipYPCatOnRecordNames = true;

	if ( stat( kSkipYPCatOnDisplayNamesFilePath, &statResult ) == 0 )
		gDisableDisplayNameLookups = true;

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

// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void BSDPlugin::WaitForInit( void )
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

		usleep( (uInt32)(50000) );
	}
} // WaitForInit

sInt32 BSDPlugin::GetNISConfiguration( void )
{
	// Are we configured to use BSD?
	sInt32		siResult = eDSNoErr;
		
	// first check to see if our domainname is set
    char*		resultPtr = CopyResultOfNISLookup( kNISdomainname );
	
	DBGLOG( "BSDPlugin::GetNISConfiguration called\n" );
	
	if ( resultPtr )
	{
		if ( strcmp( resultPtr, "\n" ) == 0 )
		{
			DBGLOG( "BSDPlugin::GetNISConfiguration no domain name set, we will check our own configuration\n" );
			// we no longer set the nis domain in /etc/hostconfig since a bad configuration will hose the system
			// at startup time.
			CFStringRef		configDomain = CopyDomainFromFile();
			SetDomain( configDomain );
			
			if ( configDomain )
				CFRelease( configDomain );
		}
		else
		{
			char*	 eoln	= strstr( resultPtr, "\n" );
			if ( eoln )
				*eoln = '\0';
			
			if ( strcmp( resultPtr, kNoDomainName ) == 0 )
			{
				DBGLOG( "BSDPlugin::GetNISConfiguration domain name is: %s, don't use.\n", resultPtr );
			}
			else
			{
				DBGLOG( "BSDPlugin::GetNISConfiguration domain name is: %s\n", resultPtr );
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
		DBGLOG( "BSDPlugin::GetNISConfiguration resultPtr is NULL!\n" );
	
	if ( mLocalNodeString )
	{
		// now we should check to see if portmap is running
		resultPtr = CopyResultOfNISLookup( kNISrpcinfo );
		
		if ( resultPtr )
		{
			if ( strstr( resultPtr, "error" ) )
			{
				DBGLOG( "BSDPlugin::GetNISConfiguration portmapper not running, we'll try starting it\n" );
			
				free( resultPtr );
				resultPtr = NULL;
				
				resultPtr = CopyResultOfNISLookup( kNISportmap );

				if ( resultPtr )
				{
					free( resultPtr );
					resultPtr = NULL;
				}
			}
			else
			{
				DBGLOG( "BSDPlugin::GetNISConfiguration portmap running\n" );
			}
		}
		else
			DBGLOG( "BSDPlugin::GetNISConfiguration resultPtr is NULL!\n" );

		if ( resultPtr )
		{
			free( resultPtr );
			resultPtr = NULL;
		}
		
/*		resultPtr = CopyResultOfNISLookup( kNISypwhich );			
		
		if ( resultPtr && strlen(resultPtr) > 1 )
		{
			char*	 eoln	= strstr( resultPtr, "\n" );
			if ( eoln )
				*eoln = '\0';
				
			DBGLOG( "BSDPlugin::GetNISConfiguration, we are bound to NIS server: %s\n", resultPtr );
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

sInt32 BSDPlugin::PeriodicTask( void )
{
    sInt32				siResult	= eDSNoErr;

	if ( mLastTimeCacheReset + kMaxTimeForMapCacheToLive < CFAbsoluteTimeGetCurrent() )
		ResetMapCache();
		
    return( siResult );
} // PeriodicTask

sInt32 BSDPlugin::ProcessRequest ( void *inData )
{
	sInt32		siResult	= 0;
	
	WaitForInit();

	if ( inData == nil )
	{
        DBGLOG( "BSDPlugin::ProcessRequest, inData is NULL!\n" );
		return( ePlugInDataError );
	}
    
	if ( (mState & kFailedToInit) )
	{
        DBGLOG( "BSDPlugin::ProcessRequest, kFailedToInit!\n" );
        return( ePlugInFailedToInitialize );
	}
	
	if ( ((sHeader *)inData)->fType == kServerRunLoop )
	{
        DBGLOG( "BSDPlugin::ProcessRequest, received a RunLoopRef, 0x%x\n", (int)((sHeader *)inData)->fContextData );
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
				DBGLOG( "BSDPlugin::ProcessRequest (kOpenDirNode), plugin not active, open on (%s) ok\n", pathStr );

				if (pathStr != NULL)
				{
					free(pathStr);
					pathStr = NULL;
				}
			}
			else
			{
				DBGLOG( "BSDPlugin::ProcessRequest (kOpenDirNode), plugin not active, returning ePlugInNotActive on open (%s)\n", pathStr );

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
			DBGLOG( "BSDPlugin::ProcessRequest (kCloseDirNode), plugin not active, returning noErr\n" );
		}
		else if ( ((sHeader *)inData)->fType != kDoPlugInCustomCall )
		{
			DBGLOG( "BSDPlugin::ProcessRequest (%ld), plugin not active!\n", ((sHeader *)inData)->fType );
			return( ePlugInNotActive );
		}
    }
	
	siResult = HandleRequest( inData );

	return( siResult );

} // ProcessRequest

// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

sInt32 BSDPlugin::SetPluginState ( const uInt32 inState )
{
// don't allow any changes other than active / in-active
	sInt32		siResult	= 0;

	WaitForInit();
	
	DBGLOG( "BSDPlugin::SetPluginState(%s):", GetProtocolPrefixString() );

	if ( (kActive & inState) && (mState & kInactive) ) // want to set to active only if currently inactive
    {
        DBGLOG( "kActive\n" );
        if ( mState & kInactive )
            mState -= kInactive;
            
        if ( !(mState & kActive) )
            mState += kActive;

#ifdef BUILDING_COMBO_PLUGIN
		ResetFFCache();
#endif
		ResetMapCache();
		
#ifdef BUILDING_COMBO_PLUGIN
		AddNode( kFFNodeName );
#endif
		siResult = GetNISConfiguration();
		
		if ( siResult == eDSNoErr && mLocalNodeString )
		{
			AddNode( mLocalNodeString );	// all is well, publish our node
		}
    }

	if ( (kInactive & inState) && (mState & kActive) ) // want to set to inactive only if currently active
    {
        DBGLOG( "kInactive\n" );
        if ( !(mState & kInactive) )
            mState += kInactive;
            
        if ( mState & kActive )
            mState -= kActive;
        // we need to deregister all our nodes

#ifdef BUILDING_COMBO_PLUGIN
		ResetFFCache();
#endif
		ResetMapCache();

#ifdef BUILDING_COMBO_PLUGIN
		RemoveNode( CFSTR(kFFNodeName) );
#endif		
		char*		oldDomainStr = mLocalNodeString;
		mLocalNodeString = NULL;
		
		DBGLOG( "Setting pluginstate to inactive, null out domain and servers\n" );
		SetDomain( NULL );
		SetNISServers( NULL, oldDomainStr );

		if ( oldDomainStr )
		{
			CFStringRef		localNodeStringRef = CFStringCreateWithCString( NULL, oldDomainStr, kCFStringEncodingUTF8 );
			
			RemoveNode( localNodeStringRef );
			CFRelease( localNodeStringRef );
			
			free(oldDomainStr);
			oldDomainStr = NULL;
		}
    }

	return( siResult );

} // SetPluginState

#pragma mark -

void BSDPlugin::AddNode( const char* nodeName, Boolean isLocalNode )
{
    if ( !nodeName )
        return;
        
    NodeData*		node = NULL;
    CFStringRef		nodeRef = CFStringCreateWithCString( NULL, nodeName, kCFStringEncodingUTF8 );
    
	if ( nodeRef )
	{
		DBGLOG( "BSDPlugin::AddNode called with %s\n", nodeName );
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
			DBGLOG( "BSDPlugin::AddNode Adding new node %s\n", nodeName );
			node = AllocateNodeData();
			
			node->fNodeName = nodeRef;
			CFRetain( node->fNodeName );
			
			node->fDSName = dsBuildListFromStringsPriv(GetProtocolPrefixString(), nodeName, nil);
	
			node->fTimeStamp = GetCurrentTime();
			
			node->fServicesRefTable = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			
			node->fSignature = mSignature;
			
			if ( node->fDSName )
			{
				DSRegisterNode( mSignature, node->fDSName, (kDirNodeType) );
			}
			
			::CFDictionaryAddValue( mPublishedNodes, nodeRef, node );
		}
		
		CFRelease( nodeRef );
		
		if ( mNISMetaNodeLocationRef )	
			CFRelease( mNISMetaNodeLocationRef );
			
		mNISMetaNodeLocationRef = CFStringCreateMutable( NULL, 0 );
		CFStringAppendCString( mNISMetaNodeLocationRef, kProtocolPrefixSlashStr, kCFStringEncodingUTF8 );
		CFStringAppendCString( mNISMetaNodeLocationRef, nodeName, kCFStringEncodingUTF8 );
		
		UnlockPublishedNodes();
	}
}

void BSDPlugin::RemoveNode( CFStringRef nodeNameRef )
{
    NodeData*		node = NULL;
    
    DBGLOG( "BSDPlugin::RemoveNode called with" );
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

const char*	BSDPlugin::GetProtocolPrefixString( void )
{		
    return gProtocolPrefixString;
}

#pragma mark -
// ---------------------------------------------------------------------------
//	* HandleRequest
//
// ---------------------------------------------------------------------------

sInt32 BSDPlugin::HandleRequest( void *inData )
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
				DBGLOG( "BSDPlugin::HandleRequest, type: kOpenDirNode\n" );
				siResult = OpenDirNode( (sOpenDirNode *)inData );
				break;
				
			case kCloseDirNode:
				DBGLOG( "BSDPlugin::HandleRequest, type: kCloseDirNode\n" );
				siResult = CloseDirNode( (sCloseDirNode *)inData );
				break;
				
			case kGetDirNodeInfo:
				DBGLOG( "BSDPlugin::HandleRequest, type: kGetDirNodeInfo\n" );
				siResult = GetDirNodeInfo( (sGetDirNodeInfo *)inData );
				break;
				
			case kGetRecordList:
				DBGLOG( "BSDPlugin::HandleRequest, type: kGetRecordList\n" );
				siResult = GetRecordList( (sGetRecordList *)inData );
				break;
				
			case kReleaseContinueData:
				DBGLOG( "BSDPlugin::HandleRequest, type: kReleaseContinueData\n" );

				siResult = ReleaseContinueData( (sNISContinueData*)((sReleaseContinueData*)inData)->fInContinueData );
				((sReleaseContinueData*)inData)->fInContinueData = NULL;
				
				break;
				
			case kGetRecordEntry:
				DBGLOG( "BSDPlugin::HandleRequest, we don't handle kGetRecordEntry yet\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kGetAttributeEntry:
				DBGLOG( "BSDPlugin::HandleRequest, type: kGetAttributeEntry\n" );
				siResult = GetAttributeEntry( (sGetAttributeEntry *)inData );
				break;
				
			case kGetAttributeValue:
				DBGLOG( "BSDPlugin::HandleRequest, type: kGetAttributeValue\n" );
			siResult = GetAttributeValue( (sGetAttributeValue *)inData );
				break;
				
			case kOpenRecord:
				DBGLOG( "BSDPlugin::HandleRequest, type: kOpenRecord\n" );
				siResult = OpenRecord( (sOpenRecord *)inData );
				break;
				
			case kGetRecordReferenceInfo:
				DBGLOG( "BSDPlugin::HandleRequest, we don't handle kGetRecordReferenceInfo yet\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kGetRecordAttributeInfo:
				DBGLOG( "BSDPlugin::HandleRequest, we don't handle kGetRecordAttributeInfo yet\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kGetRecordAttributeValueByID:
				DBGLOG( "BSDPlugin::HandleRequest, we don't handle kGetRecordAttributeValueByID yet\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kGetRecordAttributeValueByIndex:
				DBGLOG( "BSDPlugin::HandleRequest, type: kGetRecordAttributeValueByIndex\n" );
				siResult = GetRecordAttributeValueByIndex( (sGetRecordAttributeValueByIndex *)inData );
				break;
				
			case kFlushRecord:
				DBGLOG( "BSDPlugin::HandleRequest, type: kFlushRecord\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kCloseAttributeList:
				DBGLOG( "BSDPlugin::HandleRequest, type: kCloseAttributeList\n" );
				siResult = eDSNoErr;
				break;
	
			case kCloseAttributeValueList:
				DBGLOG( "BSDPlugin::HandleRequest, we don't handle kCloseAttributeValueList yet\n" );
				siResult = eNotHandledByThisNode;
				break;
	
			case kCloseRecord:
				DBGLOG( "BSDPlugin::HandleRequest, type: kCloseRecord\n" );
				siResult = CloseRecord( (sCloseRecord*)inData );
				break;
				
			case kSetRecordName:
				DBGLOG( "BSDPlugin::HandleRequest, we don't handle kSetRecordName yet\n" );
				siResult = eNotHandledByThisNode;
				break;
				
			case kSetRecordType:
				DBGLOG( "BSDPlugin::HandleRequest, we don't handle kSetRecordType yet\n" );
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
				DBGLOG( "BSDPlugin::HandleRequest, we don't handle kAddAttribute yet\n" );
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
				DBGLOG( "BSDPlugin::HandleRequest, type: kDoDirNodeAuth\n" );
				siResult = DoAuthentication( (sDoDirNodeAuth*)inData );
				break;
				
			case kDoAttributeValueSearch:
			case kDoAttributeValueSearchWithData:
				siResult = DoAttributeValueSearch( (sDoAttrValueSearch *)inData );
				break;
	
			case kDoPlugInCustomCall:
				DBGLOG( "BSDPlugin::HandleRequest, type: kDoPlugInCustomCall\n" );
				siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
				break;
				
			case kHandleNetworkTransition:
				DBGLOG( "BSDPlugin::HandleRequest, we have some sort of network transition\n" );
				siResult = HandleNetworkTransition( (sHeader*)inData );
				break;
	
				
			default:
				DBGLOG( "BSDPlugin::HandleRequest, type: %ld\n", pMsgHdr->fType );
				siResult = eNotHandledByThisNode;
				break;
		}
	}
	
    catch ( int err )
    {
        siResult = err;
        DBGLOG( "BSDPlugin::HandleRequest, Caught error:%li\n", siResult );
    }

	pMsgHdr->fResult = siResult;

    if ( siResult )
        DBGLOG( "BSDPlugin::HandleRequest returning %ld on a request of type %ld\n", siResult, pMsgHdr->fType );

	return( siResult );

} // HandleRequest

sInt32 BSDPlugin::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	sInt32					siResult		= eDSNoErr;
	unsigned long			aRequest		= 0;
	unsigned long			bufLen			= 0;
	AuthorizationRef		authRef			= 0;
	AuthorizationItemSet   *resultRightSet	= NULL;
	BSDDirNodeRep*			nodeDirRep		= NULL;
	
	DBGLOG( "BSDPlugin::DoPlugInCustomCall called\n" );
//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

	try
	{
		if ( inData == nil ) throw( (sInt32)eDSNullParameter );
		if ( mOpenRefTable == nil ) throw ( (sInt32)eDSNodeNotFound );
		
		LockPlugin();
		
		nodeDirRep			= (BSDDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );

		if( !nodeDirRep )
		{
			DBGLOG( "BSDPlugin::DoPlugInCustomCall called but we couldn't find the nodeDirRep!\n" );
			
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
					DBGLOG( "BSDPlugin::DoPlugInCustomCall kReadNISConfigData\n" );

					// read config
					siResult = FillOutCurrentState( inData );
				}
				break;
					 
				case kWriteNISConfigData:
				{
					DBGLOG( "BSDPlugin::DoPlugInCustomCall kWriteNISConfigData\n" );
					
					// write config
					siResult = SaveNewState( inData );

					if ( GetNISConfiguration() == eDSNoErr && mLocalNodeString )
					{
						DBGLOG( "BSDPlugin::DoPlugInCustomCall calling AddNode on %s\n", mLocalNodeString );
						AddNode( mLocalNodeString );	// all is well, publish our node
					}
					else
						DBGLOG( "BSDPlugin::DoPlugInCustomCall not calling AddNode because %s\n", (mLocalNodeString)?"we have no damain name":"of an error in GetNISConfiguration()" );

#ifdef BUILDING_COMBO_PLUGIN
					ResetFFCache();
#endif
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

sInt32 BSDPlugin::SaveNewState( sDoPlugInCustomCall *inData )
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
	char*					oldDomainStr				= NULL;
	
	DBGLOG( "BSDPlugin::SaveNewState called\n" );
	
	xmlDataLength = (sInt32) inData->fInRequestData->fBufferLength - sizeof( AuthorizationExternalForm );
	
	if ( xmlDataLength <= 0 )
		return (sInt32)eDSInvalidBuffFormat;
	
	xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData + sizeof( AuthorizationExternalForm )), xmlDataLength);
	
	if ( !xmlData )
	{
		DBGLOG( "BSDPlugin::SaveNewState, couldn't create xmlData from buffer!!\n" );
		siResult = (sInt32)eDSInvalidBuffFormat;
	}
	else
		newStateRef = (CFDictionaryRef)CFPropertyListCreateFromXMLData(	NULL,
																	xmlData,
																	kCFPropertyListImmutable,
																	&errorString);
																		
	if ( newStateRef && CFGetTypeID( newStateRef ) != CFDictionaryGetTypeID() )
	{
		DBGLOG( "BSDPlugin::SaveNewState, XML Data wasn't a CFDictionary!\n" );
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
			DBGLOG( "BSDPlugin::SaveNewState, we received no domain name so we are basically being turned off\n" );
			if ( mLocalNodeString )
				needToChangeDomain = true;
		}
		else
		{
			DBGLOG( "BSDPlugin::SaveNewState, the domain name is of the wrong type! (%ld)\n", CFGetTypeID( domainNameRef ) );
			siResult = (sInt32)eDSInvalidBuffFormat;
		}
		
		nisServersRef = (CFStringRef)CFDictionaryGetValue( newStateRef, CFSTR(kDSStdRecordTypeServer) );

		if ( nisServersRef && CFGetTypeID( nisServersRef ) != CFStringGetTypeID()  )
		{
			DBGLOG( "BSDPlugin::SaveNewState, the list of servers is of the wrong type! (%ld)\n", CFGetTypeID( nisServersRef ) );
			siResult = (sInt32)eDSInvalidBuffFormat;
		}
	}
	
	if ( siResult == eDSNoErr && needToChangeDomain )
	{
		if ( localNodeStringRef )
			RemoveNode( localNodeStringRef );
	
		oldDomainStr = mLocalNodeString;
		mLocalNodeString = NULL;
		
		siResult = SetDomain( domainNameRef );
	}
		
	if ( siResult == eDSNoErr )
		siResult = SetNISServers( nisServersRef, (oldDomainStr)?oldDomainStr:mLocalNodeString );
	
	if ( siResult == eDSNoErr )
		ConfigureLookupdIfNeeded();
		
	if ( localNodeStringRef )
		CFRelease( localNodeStringRef );

	if ( xmlData )
		CFRelease( xmlData );
		
	if ( newStateRef )
		CFRelease( newStateRef );

	if ( oldDomainStr )
		free( oldDomainStr );
	
	return siResult;
}

#pragma mark -
void BSDPlugin::ConfigureLookupdIfNeeded( void )
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


Boolean BSDPlugin::LookupdIsConfigured( void )
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
					DBGLOG( "BSDPlugin::LookupdIsConfigured:dsFindDirNodes on local returned zero" );
					throw( siResult ); //could end up throwing eDSNoErr but no local node will still return nil
				}
				
				// assume there is only one local node
				siResult = dsGetDirNodeName( dirRef, pLocalNodeBuff, 1, &dirNodeName );
				if ( siResult != eDSNoErr )
				{
					DBGLOG( "BSDPlugin::LookupdIsConfigured:dsGetDirNodeName on local returned error %ld", siResult );
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
					DBGLOG( "BSDPlugin::LookupdIsConfigured:dsOpenDirNode on local returned error %ld", siResult );
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

void BSDPlugin::SaveDefaultLookupdConfiguration( void )
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
				DBGLOG( "BSDPlugin::LookupdIsConfigured:dsFindDirNodes on local returned zero" );
				throw( siResult ); //could end up throwing eDSNoErr but no local node will still return nil
			}
			
			// assume there is only one local node
			siResult = dsGetDirNodeName( dirRef, pLocalNodeBuff, 1, &dirNodeName );
			if ( siResult != eDSNoErr )
			{
				DBGLOG( "BSDPlugin::LookupdIsConfigured:dsGetDirNodeName on local returned error %ld", siResult );
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
				DBGLOG( "BSDPlugin::LookupdIsConfigured:dsOpenDirNode on local returned error %ld", siResult );
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
			DBGLOG( "BSDPlugin::LookupdIsConfigured:dsOpenDirNode caught error %ld", err );
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

#pragma mark -
CFStringRef BSDPlugin::CopyDomainFromFile( void )
{
	CFStringRef				domainToReturn = NULL;
    FILE					*sourceFP = NULL;
	char					*eolPtr = NULL;
    char					buf[kMaxSizeOfParam] = {0,};
	
	LockPlugin();
	// do we have a config file?  If so, read its contents
	DBGLOG( "BSDPlugin::CopyDomainFromFile called\n" );
	// now, we want to edit /etc/hostconfig and if the NISDOMAIN entry is different, updated it with nisDomainValue
	sourceFP = fopen(kDefaultDomainFilePath,"r+");
	
	if ( sourceFP == NULL )
	{
		DBGLOG( "BSDPlugin::CopyDomainFromFile: no nisdomain file" );
	}
	else
	{
		if (fgets(buf,kMaxSizeOfParam,sourceFP) != NULL) 
		{
			eolPtr = strstr( buf, "\n" );
			
			if ( eolPtr )
				*eolPtr = '\0';
				
			if ( buf[0] != '\0' )
			{
				domainToReturn = CFStringCreateWithCString( NULL, buf, kCFStringEncodingASCII );
			}
		}
	}
	
	if ( sourceFP )
		fclose( sourceFP );
	
	UnlockPlugin();

	return domainToReturn;
}

void BSDPlugin::SaveDomainToFile( CFStringRef domainNameRef )
{
    FILE					*destFP = NULL;
    char					buf[kMaxSizeOfParam] = {0,};
	
	LockPlugin();
	DBGLOG( "BSDPlugin::SaveDomainToFile called\n" );
	// now, we want to edit /etc/hostconfig and if the NISDOMAIN entry is different, updated it with nisDomainValue
	destFP = fopen(kDefaultDomainFilePath,"w+");
	
	if ( destFP == NULL )
	{
		DBGLOG( "BSDPlugin::SaveDomainToFile: could not open nisdomain file to write! (%s)", strerror(errno) );
	}
	else
	{
		if ( domainNameRef )
		{
			CFStringGetCString( domainNameRef, buf, sizeof(buf), kCFStringEncodingASCII );
		
			fputs( buf, destFP );
		}

		fclose( destFP );
	}

	UnlockPlugin();
}

sInt32 BSDPlugin::SetDomain( CFStringRef domainNameRef )
{
	sInt32					siResult					= eDSNoErr;
	if ( domainNameRef )
	{
		CFIndex	len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(domainNameRef), kCFStringEncodingUTF8) + 1;
		mLocalNodeString = (char*)malloc(len);
		CFStringGetCString( domainNameRef, mLocalNodeString, len, kCFStringEncodingUTF8 ); 
		DBGLOG( "BSDPlugin::SaveNewState, changing our domain to %s\n", mLocalNodeString );
		AddNode( mLocalNodeString );
	}
	
	SaveDomainToFile( domainNameRef );
	
	char*		resultPtr = CopyResultOfNISLookup( kNISdomainname, (domainNameRef)?mLocalNodeString:kNoDomainName );		// set this now

	if ( resultPtr )
		free( resultPtr );
	resultPtr = NULL;
		
	return siResult;
}

sInt32 BSDPlugin::SetNISServers( CFStringRef nisServersRef, char* oldDomainStr )
{
	sInt32					siResult	= eDSNoErr;

	DBGLOG( "BSDPlugin::SetNISServers called\n" );
	
	if ( oldDomainStr )
	{
		const char*	argv[4] = {0};
		char		serverFilePath[1024];
		char*		resultPtr = NULL;
		Boolean		canceled = false;
		int			callTimedOut = 0;
	
		snprintf( serverFilePath, sizeof( serverFilePath ), "/var/yp/binding/%s.ypservers", oldDomainStr );
		
		DBGLOG( "BSDPlugin::SetNISServers going to delete file at %s\n", serverFilePath );
		
		argv[0] = "/bin/rm";
		argv[1] = "-rf";
		argv[2] = serverFilePath;
	
		if ( myexecutecommandas( NULL, "/bin/rm", argv, false, 10, &resultPtr, &canceled, getuid(), getgid(), &callTimedOut ) < 0 )
		{
			DBGLOG( "BSDPlugin::SetNISServers failed to delete ypservers\n" );
		}
	
		if ( resultPtr )
		{
			DBGLOG( "BSDPlugin::SetNISServers rm -rf returned %s\n", resultPtr );
			free( resultPtr );
			resultPtr = NULL;
		}
	}
	
	if ( mLocalNodeString )
	{
		if ( nisServersRef && !CFStringHasPrefix( nisServersRef, CFSTR("\n") ) )	// ignore this if it starts with an newline
		{
			// need to create a file in location /var/yp/binding/<domainname>.ypservers with a list of servers
			FILE*		fp = NULL;
			char		name[1024] = {0};
			char*		resultPtr = NULL;
			const char*	argv[8] = {0};
			Boolean		canceled = false;
			
			snprintf( name, sizeof(name), "/var/yp/binding/%s.ypservers", mLocalNodeString );
			fp = fopen(name,"w+");
	
			if (fp == NULL) 
			{
				int			callTimedOut = 0;
				DBGLOG( "BSDPlugin::SetNISServers: Could not open config file %s: %s\n", name, strerror(errno) );
				DBGLOG( "BSDPlugin::SetNISServers: We will make sure the path exists and try again.\n" );
				
				// try making sure the path exists...
				argv[0] = "/bin/mkdir";
				argv[1] = "-p";
				argv[2] = "/var/yp/binding/";
			
				if ( myexecutecommandas( NULL, "/bin/mkdir", argv, false, 10, &resultPtr, &canceled, getuid(), getgid(), &callTimedOut ) < 0 )
				{
					DBGLOG( "BSDPlugin::SetNISServers failed to delete ypservers\n" );
				}
			
				if ( resultPtr )
				{
					DBGLOG( "BSDPlugin::SetNISServers mkdir -p returned %s\n", resultPtr );
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
					char*		listPtr = NULL;

					CFStringGetCString( nisServersRef, serverList, len, kCFStringEncodingUTF8 );
					
					listPtr = serverList + strlen(serverList)-1;
					
					while ( listPtr > serverList && isspace(*listPtr) )		// we want to trim off any excess whitespace at the end
					{
						*listPtr = '\0';
						listPtr--;
					}	
					fprintf( fp, "%s\n", serverList );
					
					DBGLOG( "BSDPlugin::SetNISServers saving: %s\n", serverList );
					
					free( serverList );
				}
				else
					DBGLOG( "BSDPlugin::SetNISServers could not allocate memory!\n" );
					
				fclose(fp);
			}
			else
				siResult = ePlugInError;

		}
	}

	return siResult;
}

sInt32 BSDPlugin::FillOutCurrentState( sDoPlugInCustomCall *inData )
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
DBGLOG( "BSDPlugin::FillOutCurrentState: mLocalNodeString is %s\n", mLocalNodeString );
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
		DBGLOG( "BSDPlugin::FillOutCurrentState: Caught error: %ld\n", err );
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


#pragma mark -
CFStringRef BSDPlugin::CreateListOfServers( void )
{
    CFMutableStringRef		listRef			= NULL;
	
	if ( mLocalNodeString )
	{
		FILE*		fp = NULL;
		char		name[kMaxSizeOfParam] = {0};
		
		snprintf( name, sizeof(name), "/var/yp/binding/%s.ypservers", mLocalNodeString );
		fp = fopen(name,"r");
	
		if (fp == NULL) 
		{
			DBGLOG( "BSDPlugin::CreateListOfServers: Could not open config file %s: %s", name, strerror(errno) );
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
					DBGLOG( "BSDPlugin::CreateListOfServers: Could not allocate a CFString!\n" );
				}
			}
			
			fclose( fp );
		}
	}
	
	return listRef;		
}

Boolean BSDPlugin::IsOurConfigNode( char* path )
{
	Boolean 		result = false;
	
	if ( strcmp( path+1, kConfigNodeName ) == 0 )
		result = true;
		
	return result;
}

Boolean BSDPlugin::IsOurDomainNode( char* path )
{
	Boolean 		result = false;
	
	DBGLOG( "BSDPlugin::IsOurDomainNode comparing path: %s to localNode: %s\n", (path)?path:"", (mLocalNodeString)?mLocalNodeString:"" );
	if ( mLocalNodeString && strstr( path, mLocalNodeString ) != NULL )
		result = true;
		
	return result;
}

#ifdef BUILDING_COMBO_PLUGIN
Boolean BSDPlugin::IsOurFFNode( char* path )
{
	Boolean 		result = false;
	
	DBGLOG( "BSDPlugin::IsOurDomainNode comparing path: %s to FF Node Name: %s\n", (path)?path:"", kFFNodeName );
	if ( path && strstr( path, kFFNodeName ) != NULL )
		result = true;
		
	return result;
}
#endif

#pragma mark

sInt32 BSDPlugin::OpenDirNode ( sOpenDirNode *inData )
{
    sInt32				siResult			= eDSNoErr;
    char			   *nodeName			= nil;
	char			   *pathStr				= nil;
	char			   *protocolStr			= nil;
    tDataListPtr		pNodeList			= nil;
    
	DBGLOG( "BSDPlugin::OpenDirNode %lx\n", inData->fOutNodeRef );
	
	if ( inData != nil )
    {
        pNodeList	=	inData->fInDirNodeName;
        pathStr 	= dsGetPathFromListPriv( pNodeList, (char *)"/" );
        protocolStr = pathStr + 1;	// advance past the '/'
        
        DBGLOG( "BSDPlugin::OpenDirNode, ProtocolPrefixString is %s, pathStr is %s\n", GetProtocolPrefixString(), pathStr );
        
		if ( IsOurConfigNode(pathStr) || IsOurDomainNode(pathStr)
#ifdef BUILDING_COMBO_PLUGIN
			 || IsOurFFNode(pathStr)
#endif
			 )
		{
			nodeName = new char[1+strlen(protocolStr)];
			if ( !nodeName ) throw ( eDSNullNodeName );
				
			::strcpy(nodeName,protocolStr);
			
			if ( nodeName )
			{
				DBGLOG( "BSDPlugin::OpenDirNode on %s\n", nodeName );
				BSDDirNodeRep*		newNodeRep = new BSDDirNodeRep( this, (const void*)inData->fOutNodeRef );
				
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
    	DBGLOG( "BSDPlugin::OpenDirNode inData is NULL!\n" );
		siResult = eDSNullParameter;
	}
		
	if (pathStr != NULL)
	{
		free(pathStr);
		pathStr = NULL;
	}

    return siResult;
}


sInt32 BSDPlugin::CloseDirNode ( sCloseDirNode *inData )
{
    sInt32				siResult			= eDSNoErr;
    
	DBGLOG( "BSDPlugin::CloseDirNode %lx\n", inData->fInNodeRef );

	if ( inData != nil && inData->fInNodeRef && mOpenRefTable )
	{
		BSDDirNodeRep*		nodeRep = NULL;
		
		LockPlugin();
		
		nodeRep = (BSDDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
		
		DBGLOG( "BSDPlugin::CloseDirNode, CFDictionaryGetValue returned nodeRep: 0x%x\n", (int)nodeRep );

		if ( nodeRep )
		{
			::CFDictionaryRemoveValue( mOpenRefTable, (void*)inData->fInNodeRef );

			DBGLOG( "BSDPlugin::CloseDirNode, Release nodeRep: 0x%x\n", (int)nodeRep );
			nodeRep->Release();
		}
		else
		{
			DBGLOG( "BSDPlugin::CloseDirNode, nodeRef not found in our list\n" );
		}
		
		UnlockPlugin();
	}

    return siResult;
}

#pragma mark
//------------------------------------------------------------------------------------
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

sInt32 BSDPlugin::GetDirNodeInfo ( sGetDirNodeInfo *inData )
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
	BSDDirNodeRep*		nodeRep = NULL;

	LockPlugin();
		
	try
	{
		if ( inData  == nil ) throw( (sInt32)eMemoryError );

		nodeRep = (BSDDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
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
					aTmpData->AppendLong( ::strlen( "BSD" ) );
					aTmpData->AppendString( (char *)"BSD" );
					
					char *tmpStr = NULL;
					
					// now append the node name
					CFStringRef	cfNodeName = nodeRep->GetNodeName();
					
					// if we aren't in the top of the node
					if (cfNodeName != NULL && CFStringCompare(cfNodeName, CFSTR("BSD"), 0) != kCFCompareEqualTo)
					{
						CFIndex		len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(nodeRep->GetNodeName()), kCFStringEncodingUTF8) + 1;
						
						tmpStr = new char[1+len];
							
						CFStringGetCString( nodeRep->GetNodeName(), tmpStr, len, kCFStringEncodingUTF8 );
						
						// skip past the "BSD/"
						char *nodePath = ::strdup( tmpStr + 4 );
						delete tmpStr;
						tmpStr = nodePath;
					}
					else
					{
						tmpStr = ::strdup( "Unknown Node Location" );
					}
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );
					
					delete( tmpStr );

				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}
				
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
				 (::strcmp( pAttrName, kDSNAttrRecordType ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
				
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrRecordType ) );
				aTmpData->AppendString( kDSNAttrRecordType );
				
				CFStringRef	cfNodeName = nodeRep->GetNodeName();
				
				// if we are somewhere below the top node, we will return record types if requested
				if ( inData->fInAttrInfoOnly == false &&
					 cfNodeName != NULL && CFStringCompare(cfNodeName, CFSTR("BSD"), 0) != kCFCompareEqualTo )
				{
					// Attribute value count
					aTmpData->AppendShort( 12 );
					
					// We will hardcode these as we don't know what we can respond with
					aTmpData->AppendLong( ::strlen( kDSStdRecordTypeUsers ) );
					aTmpData->AppendString( kDSStdRecordTypeUsers );
					
					aTmpData->AppendLong( ::strlen( kDSStdRecordTypeGroups ) );
					aTmpData->AppendString( kDSStdRecordTypeGroups );
					
					aTmpData->AppendLong( ::strlen( kDSStdRecordTypeBootp ) );
					aTmpData->AppendString( kDSStdRecordTypeBootp );
					
					aTmpData->AppendLong( ::strlen( kDSStdRecordTypeEthernets ) );
					aTmpData->AppendString( kDSStdRecordTypeEthernets );
					
					aTmpData->AppendLong( ::strlen( kDSStdRecordTypeHosts ) );
					aTmpData->AppendString( kDSStdRecordTypeHosts );
					
					aTmpData->AppendLong( ::strlen( kDSStdRecordTypeMounts ) );
					aTmpData->AppendString( kDSStdRecordTypeMounts );
					
					aTmpData->AppendLong( ::strlen( kDSStdRecordTypeNetGroups ) );
					aTmpData->AppendString( kDSStdRecordTypeNetGroups );
					
					aTmpData->AppendLong( ::strlen( kDSStdRecordTypeNetworks ) );
					aTmpData->AppendString( kDSStdRecordTypeNetworks );
					
					aTmpData->AppendLong( ::strlen( kDSStdRecordTypePrintService ) );
					aTmpData->AppendString( kDSStdRecordTypePrintService );
					
					aTmpData->AppendLong( ::strlen( kDSStdRecordTypeProtocols ) );
					aTmpData->AppendString( kDSStdRecordTypeProtocols );
					
					aTmpData->AppendLong( ::strlen( kDSStdRecordTypeRPC ) );
					aTmpData->AppendString( kDSStdRecordTypeRPC );
					
					aTmpData->AppendLong( ::strlen( kDSStdRecordTypeServices ) );
					aTmpData->AppendString( kDSStdRecordTypeServices );
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
					// Attribute value count
					aTmpData->AppendShort( 4 );
					
					aTmpData->AppendLong( ::strlen( kDSStdAuthCrypt ) );
					aTmpData->AppendString( kDSStdAuthCrypt );

					aTmpData->AppendLong( ::strlen( kDSStdAuthClearText ) );
					aTmpData->AppendString( kDSStdAuthClearText );
					
					aTmpData->AppendLong( ::strlen( kDSStdAuthNodeNativeNoClearText ) );
					aTmpData->AppendString( kDSStdAuthNodeNativeNoClearText );
					
					aTmpData->AppendLong( ::strlen( kDSStdAuthNodeNativeClearTextOK ) );
					aTmpData->AppendString( kDSStdAuthNodeNativeClearTextOK );
					
				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or kDSNAttrAuthMethod
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
sInt32 BSDPlugin::ReleaseContinueData( sNISContinueData* continueData )
{
    sInt32					siResult			= eDSNoErr;

	if ( continueData )
	{
		pthread_mutex_lock( &continueData->fLock );
		if ( continueData->fResultArrayRef )
		{
			CFRelease( continueData->fResultArrayRef );
		}

		continueData->fResultArrayRef = NULL;
		pthread_mutex_unlock( &continueData->fLock );
		free( continueData );
	}
	
	return siResult;
}
#pragma mark

//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

sInt32 BSDPlugin::GetAttributeEntry ( sGetAttributeEntry *inData )
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

sInt32 BSDPlugin::GetAttributeValue ( sGetAttributeValue *inData )
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

sInt32 BSDPlugin::GetRecordList ( sGetRecordList *inData )
{
    sInt32					siResult			= eDSNoErr;
    char				   *pRecType			= nil;
    const char			   *pNSLRecType			= nil;
    CAttributeList		   *cpRecNameList		= nil;
    CAttributeList		   *cpRecTypeList		= nil;
    CBuff				   *outBuff				= nil;
    uInt32					countDownRecTypes	= 0;
    BSDDirNodeRep*			nodeDirRep			= nil;
	sNISContinueData		*pContinue			= (sNISContinueData*)inData->fIOContinueData;
	
	LockPlugin();
	
	DBGLOG( "BSDPlugin::GetRecordList called\n" );
	nodeDirRep = (BSDDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
	
    if( !nodeDirRep )
	{
        DBGLOG( "BSDPlugin::GetRecordList called but we couldn't find the nodeDirRep!\n" );

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
			DBGLOG( "BSDPlugin::GetRecordList, we don't have any records in our top level node.\n" );
            inData->fOutRecEntryCount	= 0;
            inData->fIOContinueData = NULL;
		}
		else
		{
			if ( !mLocalNodeString && !nodeDirRep->IsFFNode() )
			{
				DBGLOG( "BSDPlugin::GetRecordList we have no domain, returning zero results\n" );
				inData->fOutRecEntryCount	= 0;
				inData->fIOContinueData = NULL;
			}
			else if ( nodeDirRep && !pContinue )
			{
				DBGLOG( "BSDPlugin::GetRecordList called, lookup hasn't been started yet.\n" );
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
	
				DBGLOG( "BSDPlugin::GetRecordList created a new pContinue: 0x%x\n", (int)pContinue );

				pContinue->fRecNameIndex	= 1;
				pContinue->fAllRecIndex		= 0;
				pContinue->fTotalRecCount	= 0;
				pContinue->fRecTypeIndex	= 1;
				pContinue->fAttrIndex		= 1;
				pContinue->fLimitRecSearch	= 0;
				pContinue->fResultArrayRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
				pContinue->fSearchComplete = false;
				pthread_mutex_init( &pContinue->fLock, NULL );				
				
				//check if the client has requested a limit on the number of records to return
				//we only do this the first call into this context for pContext
				if (inData->fOutRecEntryCount >= 0)
				{
					DBGLOG( "BSDPlugin::DoAttributeValueSearch, setting pContinue->fLimitRecSearch to %ld\n", inData->fOutRecEntryCount );
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
				
				pthread_mutex_lock( &pContinue->fLock );

				for ( uInt16 i=1; i<=countDownRecTypes; i++ )
				{
					if ( (error = cpRecTypeList->GetAttribute( i, &pRecType )) == eDSNoErr )
					{
						DBGLOG( "BSDPlugin::GetRecordList, GetAttribute(%d of %ld) returned pRecType: %s\n", i, countDownRecTypes, pRecType );
						
#ifdef BUILDING_COMBO_PLUGIN
						if ( nodeDirRep->IsFFNode() )
							pNSLRecType = GetFFTypeFromRecType( pRecType );
						else
#endif
							pNSLRecType = GetNISTypeFromRecType( pRecType );
						
						if ( pNSLRecType )
						{							
							if ( inData->fInPatternMatch == eDSExact || inData->fInPatternMatch == eDSiExact )
							{
								// ah, this is more efficient and we can ask for just the keys we want
								char				*pRecName = nil;
								
								for ( uInt16 i=1; i<=numRecNames; i++ )
								{
									if ( (error = cpRecNameList->GetAttribute( i, &pRecName )) == eDSNoErr && pRecName )
									{
										if ( strcmp( pRecName, "dsRecordsAll" ) == 0 ) 
											DoRecordsLookup( (CFMutableArrayRef)pContinue->fResultArrayRef, nodeDirRep->IsFFNode(), pNSLRecType );	// look them all up
										else
											DoRecordsLookup( (CFMutableArrayRef)pContinue->fResultArrayRef, nodeDirRep->IsFFNode(), pNSLRecType, pRecName );
									}
								}
							}
							else
								DoRecordsLookup( (CFMutableArrayRef)pContinue->fResultArrayRef, nodeDirRep->IsFFNode(), pNSLRecType );
						}
						else
							DBGLOG( "BSDPlugin::GetRecordList, we don't have a mapping for type: %s, skipping\n", pRecType );
					}
					else
					{
						DBGLOG( "BSDPlugin::GetRecordList, GetAttribute returned error: %li\n", error );
					}
				}
				pthread_mutex_unlock( &pContinue->fLock );
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
				DBGLOG( "BSDPlugin::GetRecordList called and pContinue->fSearchComplete, we are done, Releaseing Continue Data (0x%x)\n", (int)pContinue );
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
		
        DBGLOG( "BSDPlugin::GetRecordList, Caught error:%li\n", siResult );
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

	DBGLOG( "BSDPlugin::GetRecordList returning, inData->fOutRecEntryCount: %ld, inData->fIOContinueData: 0x%x\n", inData->fOutRecEntryCount, (int)pContinue );
	inData->fIOContinueData = pContinue;
	
    return( siResult );

}	// GetRecordList

sInt32 BSDPlugin::OpenRecord ( sOpenRecord *inData )
{
    tDirStatus	siResult = eDSNoErr;
	
	DBGLOG( "BSDPlugin::OpenRecord called on refNum:0x%x for record: %s of type: %s\n", (int)inData->fOutRecRef, (char*)inData->fInRecName->fBufferData, (char*)inData->fInRecType->fBufferData );

	const char*		pNSLRecType = NULL;
	BSDDirNodeRep*	nodeDirRep = (BSDDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );

	if ( !nodeDirRep )
		return eDSInvalidDirRef;
		
#ifdef BUILDING_COMBO_PLUGIN
	if ( nodeDirRep->IsFFNode() )
		pNSLRecType = GetFFTypeFromRecType( (char*)inData->fInRecType->fBufferData );
	else
#endif
		pNSLRecType = GetNISTypeFromRecType( (char*)inData->fInRecType->fBufferData );
	
	if ( pNSLRecType )
	{
		CFMutableDictionaryRef	recordResults = CopyRecordLookup( nodeDirRep->IsFFNode(), pNSLRecType, (char*)inData->fInRecName->fBufferData );
	
		AddRecordRecRef( inData->fOutRecRef, recordResults );
    }
	else
	{
		inData->fOutRecRef = 0;
		siResult = eDSInvalidRecordType;
		DBGLOG( "BSDPlugin::OpenRecord we don't support that type!\n" );
	}
		
    return( siResult );
}	// OpenRecord

sInt32 BSDPlugin::GetRecordAttributeValueByIndex( sGetRecordAttributeValueByIndex *inData )
{
    tDirStatus					siResult = eDSNoErr;
	tAttributeValueEntryPtr		pOutAttrValue	= nil;
	uInt32						uiDataLen		= 0;

	DBGLOG( "BSDPlugin::GetRecordAttributeValueByIndex called on refNum:0x%x, for type: %s, index: %ld\n", (int)inData->fInRecRef, (char*)inData->fInAttrType->fBufferData, inData->fInAttrValueIndex );
	CFDictionaryRef recordRef = GetRecordFromRecRef( inData->fInRecRef );
	
	if ( !recordRef )	
	{
		DBGLOG( "BSDPlugin::GetRecordAttributeValueByIndex, unknown record\n" );
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
				DBGLOG( "BSDPlugin::GetRecordAttributeValueByIndex, they are asking for an index of (%ld) when we only have one value\n", inData->fInAttrValueIndex );
		}
		else if ( valueResult && CFGetTypeID( valueResult ) == CFArrayGetTypeID() )		
		{
			if ( (CFIndex)inData->fInAttrValueIndex <= CFArrayGetCount( (CFArrayRef)valueResult ) )
				valueStringRef = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)valueResult, inData->fInAttrValueIndex-1 );
			else
				DBGLOG( "BSDPlugin::GetRecordAttributeValueByIndex, they are asking for an index of (%ld) when we only have %ld value(s)\n", inData->fInAttrValueIndex, CFArrayGetCount( (CFArrayRef)valueResult ) );
		}
		else
			DBGLOG( "BSDPlugin::GetRecordAttributeValueByIndex, the value wasn't one we handle (%ld), ignoring\n", (valueResult)?CFGetTypeID( valueResult ):0 );

		
		if ( valueStringRef )
		{
			uiDataLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueStringRef), kCFStringEncodingUTF8) + 1;
			pOutAttrValue = (tAttributeValueEntry *)::calloc( (sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad), sizeof( char ) );

			pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen;
			pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;

			pOutAttrValue->fAttributeValueID = inData->fInAttrValueIndex;	// what do we do for an ID, index for now
			CFStringGetCString( valueStringRef, pOutAttrValue->fAttributeValueData.fBufferData, uiDataLen, kCFStringEncodingUTF8 ); 

			inData->fOutEntryPtr = pOutAttrValue;

			DBGLOG( "BSDPlugin::GetRecordAttributeValueByIndex, found the value: %s\n", pOutAttrValue->fAttributeValueData.fBufferData );
		}
		else
		{
			DBGLOG( "BSDPlugin::GetRecordAttributeValueByIndex, couldn't find any values with this key!\n" );
			siResult = eDSIndexOutOfRange;
		}
			
		CFRelease( keyRef );
	}
	else
	{
		DBGLOG( "BSDPlugin::GetRecordAttributeValueByIndex, couldn't create a keyRef!\n" );
		siResult = eDSInvalidAttributeType;
	}
		
    return( siResult );
}

sInt32 BSDPlugin::DoAttributeValueSearch( sDoAttrValueSearch* inData )
{
    sInt32					siResult			= eDSNoErr;
    char				   *pRecType			= nil;
    const char*				pNSLRecType			= nil;
    CAttributeList		   *cpRecNameList		= nil;
    CAttributeList		   *cpRecTypeList		= nil;
    CAttributeList		   *cpAttrTypeList 		= nil;
    CBuff				   *outBuff				= nil;
    uInt32					countDownRecTypes	= 0;
    BSDDirNodeRep*			nodeDirRep			= nil;
	sNISContinueData		*pContinue			= (sNISContinueData*)inData->fIOContinueData;
    
	LockPlugin();
	
	DBGLOG( "BSDPlugin::DoAttributeValueSearch called\n" );
	nodeDirRep = (BSDDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
	
    if( !nodeDirRep )
	{
        DBGLOG( "BSDPlugin::DoAttributeValueSearch called but we couldn't find the nodeDirRep!\n" );

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
			DBGLOG( "BSDPlugin::DoAttributeValueSearch, we don't have any records in our top level node.\n" );
            inData->fOutMatchRecordCount	= 0;
            inData->fIOContinueData = NULL;
		}
		else
		{
			if ( nodeDirRep && !pContinue )
			{
				DBGLOG( "BSDPlugin::DoAttributeValueSearch called, lookup hasn't been started yet.\n" );
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
					DBGLOG( "BSDPlugin::DoAttributeValueSearch, setting pContinue->fLimitRecSearch to %ld\n", inData->fOutMatchRecordCount );
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
						DBGLOG( "BSDPlugin::DoAttributeValueSearch, GetAttribute(%d) returned pRecType: %s\n", i, pRecType );
						
#ifdef BUILDING_COMBO_PLUGIN
						if ( nodeDirRep->IsFFNode() )
							pNSLRecType = GetFFTypeFromRecType( pRecType );
						else
#endif
							pNSLRecType = GetNISTypeFromRecType( pRecType );
						
						if ( pNSLRecType )
						{
							DBGLOG( "BSDPlugin::DoAttributeValueSearch, looking up records of pNSLRecType: %s, matchType: 0x%x and pattern: \"%s\"\n", pNSLRecType, inData->fInPattMatchType, (inData->fInPatt2Match)?inData->fInPatt2Match->fBufferData:"NULL" );
							
							DoRecordsLookup( (CFMutableArrayRef)pContinue->fResultArrayRef, nodeDirRep->IsFFNode(), pNSLRecType, NULL, inData->fInPattMatchType, inData->fInPatt2Match, inData->fInAttrType->fBufferData );
						}
						else
							DBGLOG( "BSDPlugin::DoAttributeValueSearch, we don't have a mapping for type: %s, skipping\n", pNSLRecType );
					}
					else
					{
						DBGLOG( "BSDPlugin::DoAttributeValueSearch, GetAttribute returned error: %li\n", error );
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
					DBGLOG( "BSDPlugin::DoAttributeValueSearch inData->fType == kDoAttributeValueSearchWithData, inAttrInfoOnly: %d\n", attrInfoOnly );
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
				DBGLOG( "BSDPlugin::DoAttributeValueSearch called and pContinue->fSearchComplete, we are done\n" );
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
        DBGLOG( "BSDPlugin::DoAttributeValueSearch, Caught error:%li\n", siResult );
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

	DBGLOG( "BSDPlugin::DoAttributeValueSearch returning, inData->fOutMatchRecordCount: %ld, inData->fIOContinueData: 0x%x\n", inData->fOutMatchRecordCount, (int)pContinue );
	inData->fIOContinueData = pContinue;
	
    return( siResult );

}

sInt32 BSDPlugin::CloseRecord ( sCloseRecord *inData )
{
    DBGLOG( "BSDPlugin::CloseRecord called on refNum:0x%x\n", (int)inData->fInRecRef );

	DeleteRecordRecRef( inData->fInRecRef );
    
    return( eDSNoErr );
}	// CloseRecord

void BSDPlugin::AddRecordRecRef( tRecordReference recRef, CFDictionaryRef resultRef )
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

void BSDPlugin::DeleteRecordRecRef( tRecordReference recRef )
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


CFDictionaryRef BSDPlugin::GetRecordFromRecRef( tRecordReference recRef )
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

sInt32 BSDPlugin::HandleNetworkTransition( sHeader *inData )
{
    sInt32					siResult			= eDSNoErr;
	
	DBGLOG( "BSDPlugin::HandleNetworkTransition called\n" );
	
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
		DBGLOG( "BSDPlugin::HandleNetworkTransition calling AddNode on %s\n", mLocalNodeString );
		AddNode( mLocalNodeString );	// all is well, publish our node
	}
	else
		DBGLOG( "BSDPlugin::HandleNetworkTransition not calling AddNode (siResult:%ld) because %s\n", siResult, (mLocalNodeString)?"we have no damain name":"of an error" );

	ResetMapCache();
	
    return ( siResult );
}

#ifdef BUILDING_COMBO_PLUGIN
void BSDPlugin::ResetFFCache( void )
{
	LockFFCache();
	DBGLOG( "BSDPlugin::ResetMapCache called\n" );
	if ( mCachedFFRef )
	{
		CFRelease( mCachedFFRef );
	}
	mCachedFFRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	mLastTimeFFCacheReset = CFAbsoluteTimeGetCurrent();
	UnlockFFCache();
}
#endif

void BSDPlugin::ResetMapCache( void )
{
	LockMapCache();
	DBGLOG( "BSDPlugin::ResetMapCache called\n" );
	if ( mCachedMapsRef )
	{
		CFRelease( mCachedMapsRef );
	}
	mCachedMapsRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

	mLastTimeCacheReset = CFAbsoluteTimeGetCurrent();
	UnlockMapCache();
}

Boolean BSDPlugin::ResultMatchesRequestRecordNameCriteria(	CFDictionaryRef		result,
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
					DBGLOG( "BSDPlugin::ResultMatchesRequestRecordNameCriteria, request name is: %s\n", pRecName );
				else
					DBGLOG( "BSDPlugin::ResultMatchesRequestRecordNameCriteria, request name is NULL\n" );
					
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
						CFIndex		arrayCount = CFArrayGetCount( (CFArrayRef)valueResult );
						
						for ( CFIndex i=0; i<arrayCount && !resultIsOK; i++ )
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
						CFIndex		arrayCount = CFArrayGetCount( (CFArrayRef)valueResult );
						
						for ( CFIndex i=0; i<arrayCount && !resultIsOK; i++ )
						{
							CFStringRef		resultRecordAttributeRef = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)valueResult, i );
							
							resultIsOK = (resultRecordAttributeRef && CFStringHasPrefix( (CFStringRef)resultRecordAttributeRef, CFSTR(kDSNativeRecordTypePrefix) ) );
						}
					}
				}
				else
				{
					CFPropertyListRef	valueRealNameResult = NULL;
					
					valueResult = (CFPropertyListRef)CFDictionaryGetValue( result, CFSTR(kDSNAttrRecordName) );
					valueRealNameResult = (CFPropertyListRef)CFDictionaryGetValue( result, CFSTR(kDS1AttrDistinguishedName) );

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
							CFIndex		arrayCount = CFArrayGetCount( (CFArrayRef)valueResult );
							
							for ( CFIndex i=0; i<arrayCount && !resultIsOK; i++ )
							{
								CFStringRef		resultRecordNameRef = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)valueResult, i );
								
								if ( resultRecordNameRef && CFGetTypeID( resultRecordNameRef ) == CFStringGetTypeID() )
									resultIsOK = IsResultOK( patternMatch, resultRecordNameRef, recNameMatchRef );
							}
						}
						
						if ( !resultIsOK && valueRealNameResult )
						{
							// no match on the record name, try the "RealName"
							if ( CFGetTypeID( valueRealNameResult ) == CFStringGetTypeID() )
							{
								resultIsOK = IsResultOK( patternMatch, (CFStringRef)valueRealNameResult, recNameMatchRef );
							}
							else if ( CFGetTypeID( valueRealNameResult ) == CFArrayGetTypeID() )		
							{
								CFIndex		arrayCount = CFArrayGetCount( (CFArrayRef)valueRealNameResult );
								
								for ( CFIndex i=0; i<arrayCount && !resultIsOK; i++ )
								{
									CFStringRef		resultRecordNameRef = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)valueRealNameResult, i );
									
									if ( resultRecordNameRef && CFGetTypeID( resultRecordNameRef ) == CFStringGetTypeID() )
										resultIsOK = IsResultOK( patternMatch, resultRecordNameRef, recNameMatchRef );
								}
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

						
sInt32 BSDPlugin::RetrieveResults(	tDirNodeReference	inNodeRef,
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
    
	DBGLOG( "BSDPlugin::RetrieveResults called for 0x%x in node %lx\n", (int)continueData, inNodeRef );

	// we only support certain pattern matches which are defined in BSDPlugin::ResultMatchesRequestRecordNameCriteria
	if (	inRecordNamePatternMatch != eDSExact
		&&	inRecordNamePatternMatch != eDSiExact
		&&	inRecordNamePatternMatch != eDSAnyMatch
		&&	inRecordNamePatternMatch != eDSStartsWith 
		&&	inRecordNamePatternMatch != eDSiStartsWith 
		&&	inRecordNamePatternMatch != eDSEndsWith 
		&&	inRecordNamePatternMatch != eDSiEndsWith 
		&&	inRecordNamePatternMatch != eDSContains
		&&	inRecordNamePatternMatch != eDSiContains )
	{
		DBGLOG( "BSDPlugin::RetrieveResults called with a pattern match we haven't implemented yet: 0x%.4x\n", inRecordNamePatternMatch );
		return eNotYetImplemented;
    }
	
	pthread_mutex_lock( &continueData->fLock );
	if ( (CFIndex)*outRecEntryCount >= CFArrayGetCount(continueData->fResultArrayRef) )
	{
		DBGLOG( "BSDPlugin::RetrieveResults we're finished, setting fSearchComplete and fOutRecEntryCount to zero\n" );
		continueData->fSearchComplete = true;		// we are done

		pthread_mutex_unlock( &continueData->fLock );
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
			
			DBGLOG( "BSDPlugin::RetrieveResults clearing aAttrData and aRecData\n" );
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
				DBGLOG( "BSDPlugin::RetrieveResults curResult: 0x%x for index: %ld doesn't satisfy Request Criteria, skipping\n", (int)curResult, CFArrayGetCount(continueData->fResultArrayRef)-1 );
				
				CFRelease( curResult );
				curResult = NULL;
				
				continue;
			}
			
			DBGLOG( "BSDPlugin::RetrieveResults curResult: 0x%x for index: %ld and pattern match: 0x%x\n", (int)curResult, CFArrayGetCount(continueData->fResultArrayRef)+1, (int)inRecordNamePatternMatch );
			
			char		tempBuf[1024];
			char*		recordName = NULL;
			CFIndex		recordNameLength = 0;
			CFStringRef	recordNameRef = NULL;

			char*		recType = NULL;
			CFIndex		recTypeLength = 0;
			
			CFStringRef	recordTypeRef = (CFStringRef)CFDictionaryGetValue( curResult, CFSTR(kDSNAttrRecordType) );

			if ( recordTypeRef && CFGetTypeID( recordTypeRef ) == CFArrayGetTypeID() )
			{
				DBGLOG( "BSDPlugin::RetrieveResults curResult has more than one record type, grabbing the first one\n" );
				recordTypeRef = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)recordTypeRef, 0 );
			}
			
			if ( recordTypeRef && CFGetTypeID( recordTypeRef ) == CFStringGetTypeID() )
			{
				recTypeLength = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(recordTypeRef), kCFStringEncodingUTF8) +1;
		
				if ( recTypeLength > (CFIndex)sizeof(tempBuf) )
					recType = (char*)malloc( recTypeLength );
				else
					recType = tempBuf;

				if ( !::CFStringGetCString( recordTypeRef, recType, recTypeLength, kCFStringEncodingUTF8 ) )
				{
					DBGLOG( "BSDPlugin::RetrieveResults couldn't convert recordTypeRef CFString to char* in UTF8!\n" );
					
					if ( recType != tempBuf )
						free( recType );
					recType = NULL;
				}
			}
			else if ( recordTypeRef )
			{
				DBGLOG( "BSDPlugin::RetrieveResults unknown recordTypeRefID: %ld\n", CFGetTypeID( recordTypeRef ) );
			}
				
			if ( recType != nil )
			{
				DBGLOG( "BSDPlugin::RetrieveResults recType=%s\n", recType );
				aRecData->AppendShort( ::strlen( recType ) );
				aRecData->AppendString( recType );
				if ( recType != tempBuf )
					free(recType);
				recType = NULL;
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
				DBGLOG( "BSDPlugin::RetrieveResults record has no name, skipping\n" );
				
				CFRelease( curResult );
				curResult = NULL;
				continue;			// skip this result
			}
			
			if ( CFGetTypeID( recordNameRef ) == CFArrayGetTypeID() )
			{
				recordNameRef = (CFStringRef)CFArrayGetValueAtIndex( (CFArrayRef)recordNameRef, 0 );
			}
			
			recordNameLength = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(recordNameRef), kCFStringEncodingUTF8) +1;
		
			if ( recordNameLength > (CFIndex)sizeof(tempBuf) )
				recordName = (char*)malloc( recordNameLength );
			else
				recordName = tempBuf;
				
			if ( !::CFStringGetCString( recordNameRef, recordName, recordNameLength, kCFStringEncodingUTF8 ) )
			{
				DBGLOG( "BSDPlugin::RetrieveResults couldn't convert recordNameRef CFString to char* in UTF8!\n" );
				recordName[0] = '\0';
			}
		
			if ( recordNameRef != nil && recordName[0] != '\0' )
			{
				DBGLOG( "BSDPlugin::RetrieveResults looking at record: %s\n", recordName );
				aRecData->AppendShort( ::strlen( recordName ) );
				aRecData->AppendString( recordName );
			}
			else
			{
				aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
				aRecData->AppendString( "Record Name Unknown" );
			}
			
			if ( recordName != tempBuf )
				free( recordName );
			recordName = NULL;
			
			// now the attributes
			AttrDataContext		context = {0,aAttrData,inAttributeInfoOnly, NULL};
				
			// we want to pull out the appropriate attributes that the client is searching for...
			uInt32					numAttrTypes	= 0;
			CAttributeList			cpAttributeList( inAttributeInfoTypeList );
	
			//save the number of rec types here to use in separating the buffer data
			numAttrTypes = cpAttributeList.GetCount();
	
			DBGLOG( "BSDPlugin::RetrieveResults client looking for %ld attribute(s)\n", numAttrTypes );
			if( (numAttrTypes == 0) ) throw( eDSEmptyRecordTypeList );
	
			sInt32					error = eDSNoErr;
			char*				   	pAttrType = nil;
			
			for ( uInt32 i=1; i<=numAttrTypes; i++ )
			{
				if ( (error = cpAttributeList.GetAttribute( i, &pAttrType )) == eDSNoErr )
				{
					DBGLOG( "BSDPlugin::RetrieveResults looking for attribute type: %s\n", pAttrType );
					
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

								if ( CFStringGetLength( valueRef ) > 0 )
									CFArrayAppendValue( (CFMutableArrayRef)valueArrayRef, valueRef );
							}
							else
								DBGLOG( "BSDPlugin::RetrieveResults got unknown value type (%ld), ignore\n", CFGetTypeID(valueTypeRef) );
					
							aTempData->AppendShort( ::strlen( pAttrType ) );		// attrTypeLen
							aTempData->AppendString( pAttrType );					// attrType

							if ( valueArrayRef && !inAttributeInfoOnly )
							{	
								char		valueTmpBuf[1024];
								char*		value = NULL;
								CFIndex		arrayCount = CFArrayGetCount(valueArrayRef);
								
								aTempData->AppendShort( (short)CFArrayGetCount(valueArrayRef) );	// attrValueCount
								
								for ( CFIndex i=0; i<arrayCount; i++ )
								{
									valueRef = (CFStringRef)::CFArrayGetValueAtIndex( valueArrayRef, i );
		
									if ( valueRef && ::CFStringGetLength(valueRef) > 0 )
									{
										CFIndex		maxValueEncodedSize = ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueRef), kCFStringEncodingUTF8) + 1;
										
										if ( maxValueEncodedSize > (CFIndex)sizeof(valueTmpBuf) )
											value = (char*)malloc( maxValueEncodedSize );
										else
											value = valueTmpBuf;
										
										if ( ::CFStringGetCString( valueRef, value, maxValueEncodedSize, kCFStringEncodingUTF8 ) )	
										{
											aTempData->AppendShort( ::strlen( value ) );
											aTempData->AppendString( value );
											DBGLOG( "BSDPlugin::RetrieveResults added value: %s\n", value );
										}
										else
										{
											DBGLOG( "BSDPlugin::RetrieveResults couldn't make cstr from CFString for value!\n" );
											aTempData->AppendShort( 0 );
										}
										
										if ( value != valueTmpBuf )
											free( value );
										value = NULL;
									}
									else
									{
										DBGLOG( "BSDPlugin::RetrieveResults no valueRef or its length is 0!\n" );
										aTempData->AppendShort( 0 );
									}
								}
							}
							else if ( inAttributeInfoOnly )
							{
								aTempData->AppendShort( 0 );	// attrValueCount
								DBGLOG( "BSDPlugin::RetrieveResults skipping values as the caller only wants attributes\n" );
							}
							else
							{
								DBGLOG( "BSDPlugin::RetrieveResults no values for this attribute, skipping!\n" );

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

							DBGLOG( "BSDPlugin::RetrieveResults adding aTempData to aAttrData, context.count: %d\n", (short)context.count );
							aAttrData->AppendShort(aTempData->GetLength());
							aAttrData->AppendBlock(aTempData->GetData(), aTempData->GetLength() );
						}
					}
				}
				else
				{
					DBGLOG( "BSDPlugin::RetrieveResults GetAttribute returned error:%li\n", error );
				}
			}
			
			// Attribute count
			DBGLOG( "BSDPlugin::RetrieveResults adding aAttrData count %d to aRecData\n", (short)context.count );
			aRecData->AppendShort( (short)context.count );

			if ( context.count > 0 )
			{
				// now add the attributes to the record
				aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
			}
			
			DBGLOG( "BSDPlugin::RetrieveResults calling outBuff->AddData()\n" );
			siResult = outBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
			
			if ( siResult == CBuff::kBuffFull )
			{
				DBGLOG( "BSDPlugin::RetrieveResults siResult == CBuff::kBuffFull, don't bump the count for next time\n" );
				CFArrayAppendValue( continueData->fResultArrayRef, curResult );		// put this back
				
				UInt32	numDataBlocks = 0;
				
				if ( outBuff->GetDataBlockCount( &numDataBlocks ) != eDSNoErr || numDataBlocks == 0 )
				{
					siResult = eDSBufferTooSmall;	// we couldn't fit any other data in here either
					DBGLOG( "BSDPlugin::RetrieveResults siResult == eDSBufferTooSmall, we're done\n" );
				}
			}
			else if ( siResult == eDSNoErr )
			{
				continueData->fTotalRecCount++;
				(*outRecEntryCount)++;
			}
			else
				DBGLOG( "BSDPlugin::RetrieveResults siResult == %ld\n", siResult );
			
			CFRelease( curResult );
			curResult = NULL;
		}

        if ( siResult == CBuff::kBuffFull )
            siResult = eDSNoErr;
			
        outBuff->SetLengthToSize();
   }

    catch ( int err )
    {
		DBGLOG( "BSDPlugin::RetrieveResults caught err: %d\n", err );
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

	if ( !continueData->fResultArrayRef || CFArrayGetCount( continueData->fResultArrayRef ) == 0 || (continueData->fTotalRecCount >= continueData->fLimitRecSearch && continueData->fLimitRecSearch > 0) )
	{
		continueData->fSearchComplete = true;
		DBGLOG( "BSDPlugin::RetrieveResults setting fSearchComplete to true\n" );
	}
		
	pthread_mutex_unlock( &continueData->fLock );
	DBGLOG( "BSDPlugin::RetrieveResults returning siResult: %ld\n", siResult );

    return( siResult );
    
}

#ifdef BUILDING_COMBO_PLUGIN
// ---------------------------------------------------------------------------
//	* GetFFTypeFromRecType
// ---------------------------------------------------------------------------

const char* BSDPlugin::GetFFTypeFromRecType ( char *inRecType )
{
	if ( !inRecType )
		return NULL;
		
	if ( strcmp( inRecType, kDSStdRecordTypeUsers ) == 0 )
		return kFFRecordTypeUsers;
	else if ( strcmp( inRecType, kDSStdRecordTypeGroups ) == 0 )
		return kFFRecordTypeGroups;
	else if ( strcmp( inRecType, kDSStdRecordTypeBootp ) == 0 )
		return kFFRecordTypeBootp;
	else if ( strcmp( inRecType, kDSStdRecordTypeEthernets ) == 0 )
		return kFFRecordTypeEthernets;
	else if ( strcmp( inRecType, kDSStdRecordTypeHosts ) == 0 )
		return kFFRecordTypeHosts;
	else if ( strcmp( inRecType, kDSStdRecordTypeMounts ) == 0 )
		return kFFRecordTypeMounts;
	else if ( strcmp( inRecType, kDSStdRecordTypeNetGroups ) == 0 )
		return kFFRecordTypeNetGroups;
	else if ( strcmp( inRecType, kDSStdRecordTypeNetworks ) == 0 )
		return kFFRecordTypeNetworks;
	else if ( strcmp( inRecType, kDSStdRecordTypePrintService ) == 0 )
		return kFFRecordTypePrintService;
	else if ( strcmp( inRecType, kDSStdRecordTypeProtocols ) == 0 )
		return kFFRecordTypeProtocols;
	else if ( strcmp( inRecType, kDSStdRecordTypeRPC ) == 0 )
		return kFFRecordTypeRPC;
	else if ( strcmp( inRecType, kDSStdRecordTypeServices ) == 0 )
		return kFFRecordTypeServices;
	else if ( strcmp( inRecType, kDSNAttrBootParams ) == 0 )
		return kFFRecordTypeBootParams;
	else	
		return NULL;
}
#endif

// ---------------------------------------------------------------------------
//	* GetNISTypeFromRecType
// ---------------------------------------------------------------------------

const char* BSDPlugin::GetNISTypeFromRecType ( char *inRecType )
{
	if ( !inRecType )
		return NULL;
		
	if ( strcmp( inRecType, kDSStdRecordTypeUsers ) == 0 )
		return kNISRecordTypeUsers;
	else if ( strcmp( inRecType, kDSStdRecordTypeGroups ) == 0 )
		return kNISRecordTypeGroups;
	else if ( strcmp( inRecType, kDSStdRecordTypeBootp ) == 0 )
		return kNISRecordTypeBootp;
	else if ( strcmp( inRecType, kDSStdRecordTypeEthernets ) == 0 )
		return kNISRecordTypeEthernets;
	else if ( strcmp( inRecType, kDSStdRecordTypeHosts ) == 0 )
		return kNISRecordTypeHosts;
	else if ( strcmp( inRecType, kDSStdRecordTypeMounts ) == 0 )
		return kNISRecordTypeMounts;
	else if ( strcmp( inRecType, kDSStdRecordTypeNetGroups ) == 0 )
		return kNISRecordTypeNetGroups;
	else if ( strcmp( inRecType, kDSStdRecordTypeNetworks ) == 0 )
		return kNISRecordTypeNetworks;
	else if ( strcmp( inRecType, kDSStdRecordTypePrintService ) == 0 )
		return kNISRecordTypePrintService;
	else if ( strcmp( inRecType, kDSStdRecordTypeProtocols ) == 0 )
		return kNISRecordTypeProtocols;
	else if ( strcmp( inRecType, kDSStdRecordTypeRPC ) == 0 )
		return kNISRecordTypeRPC;
	else if ( strcmp( inRecType, kDSStdRecordTypeServices ) == 0 )
		return kNISRecordTypeServices;
	else if ( strcmp( inRecType, kDSNAttrBootParams ) == 0 )
		return kNISRecordTypeBootParams;
	else	
		return NULL;
}

#pragma mark -
void BSDPlugin::StartNodeLookup( void )
{
    DBGLOG( "BSDPlugin::StartNodeLookup called\n" );
		
	if ( mLocalNodeString )
		AddNode( mLocalNodeString );	// all is well, publish our node
}

CFMutableDictionaryRef BSDPlugin::CopyRecordLookup( Boolean isFFRecord, const char* recordTypeName, char* recordName )
{
	CFMutableDictionaryRef	resultRecordRef = NULL;
	
    DBGLOG( "BSDPlugin::CopyRecordLookup called, recordTypeName: %s, recordName: %s\n", recordTypeName, recordName );

	CFMutableArrayRef	tempArrayRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );

	if ( tempArrayRef )
	{
		DoRecordsLookup( tempArrayRef, isFFRecord, recordTypeName, recordName );
		
		if ( CFArrayGetCount(tempArrayRef) > 0 )
		{
			resultRecordRef = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( tempArrayRef, 0 );
			CFRetain( resultRecordRef );
		}
		
		CFRelease( tempArrayRef );
	}
	
	return resultRecordRef;
}

#ifdef BUILDING_COMBO_PLUGIN
void BSDPlugin::SetLastModTimeOfFileRead( const char* recordTypeName, time_t modTimeOfFile )
{
	if ( !recordTypeName )
	{
		DBGLOG( "BSDPlugin::SetLastModTimeOfFileRead was passed a recordTypeName of NULL!" );
		return;
	}
	
	int		index = -1;
	
	if ( strcmp( recordTypeName, kFFRecordTypeUsers ) == 0 )
		index = 0;
	else if ( strcmp( recordTypeName, kFFRecordTypeGroups ) == 0 )
		index = 1;
	else if ( strcmp( recordTypeName, kFFRecordTypeAlias ) == 0 )
		index = 2;
	else if ( strcmp( recordTypeName, kFFRecordTypeBootp ) == 0 )
		index = 3;
	else if ( strcmp( recordTypeName, kFFRecordTypeEthernets ) == 0 )
		index = 4;
	else if ( strcmp( recordTypeName, kFFRecordTypeHosts ) == 0 )
		index = 5;
	else if ( strcmp( recordTypeName, kFFRecordTypeMounts ) == 0 )
		index = 6;
	else if ( strcmp( recordTypeName, kFFRecordTypeNetGroups ) == 0 )
		index = 7;
	else if ( strcmp( recordTypeName, kFFRecordTypeNetworks ) == 0 )
		index = 8;
	else if ( strcmp( recordTypeName, kFFRecordTypePrintService ) == 0 )
		index = 9;
	else if ( strcmp( recordTypeName, kFFRecordTypeProtocols ) == 0 )
		index = 10;
	else if ( strcmp( recordTypeName, kFFRecordTypeRPC ) == 0 )
		index = 11;
	else if ( strcmp( recordTypeName, kFFRecordTypeServices ) == 0 )
		index = 12;
	else if ( strcmp( recordTypeName, kFFRecordTypeBootParams ) == 0 )
		index = 13;

	if ( index > 0 )
		mModTimes[index] = modTimeOfFile;
	else
		DBGLOG( "BSDPlugin::SetLastModTimeOfFileRead, unknown recordTypeName: %s\n", recordTypeName );
}

time_t BSDPlugin::GetLastModTimeOfFileRead( const char* recordTypeName )
{
	if ( !recordTypeName )
		return 0;
		
	if ( strcmp( recordTypeName, kFFRecordTypeUsers ) == 0 )
		return mModTimes[0];
	else if ( strcmp( recordTypeName, kFFRecordTypeGroups ) == 0 )
		return mModTimes[1];
	else if ( strcmp( recordTypeName, kFFRecordTypeAlias ) == 0 )
		return mModTimes[2];
	else if ( strcmp( recordTypeName, kFFRecordTypeBootp ) == 0 )
		return mModTimes[3];
	else if ( strcmp( recordTypeName, kFFRecordTypeEthernets ) == 0 )
		return mModTimes[4];
	else if ( strcmp( recordTypeName, kFFRecordTypeHosts ) == 0 )
		return mModTimes[5];
	else if ( strcmp( recordTypeName, kFFRecordTypeMounts ) == 0 )
		return mModTimes[6];
	else if ( strcmp( recordTypeName, kFFRecordTypeNetGroups ) == 0 )
		return mModTimes[7];
	else if ( strcmp( recordTypeName, kFFRecordTypeNetworks ) == 0 )
		return mModTimes[8];
	else if ( strcmp( recordTypeName, kFFRecordTypePrintService ) == 0 )
		return mModTimes[9];
	else if ( strcmp( recordTypeName, kFFRecordTypeProtocols ) == 0 )
		return mModTimes[10];
	else if ( strcmp( recordTypeName, kFFRecordTypeRPC ) == 0 )
		return mModTimes[11];
	else if ( strcmp( recordTypeName, kFFRecordTypeServices ) == 0 )
		return mModTimes[12];
	else if ( strcmp( recordTypeName, kFFRecordTypeBootParams ) == 0 )
		return mModTimes[13];
	else	
		return 0;

}

CFMutableDictionaryRef BSDPlugin::CopyResultOfFFLookup( const char* recordTypeName, CFStringRef recordTypeRef )
{
	// need to read contents of config file
	char					filepath[1024] = {0,};
	CFMutableDictionaryRef	resultDictionaryRef = NULL;
	time_t					modTimeOfFile = 0;
	
	snprintf( filepath, sizeof(filepath), "%s%s", kFFConfDir, recordTypeName );
	
	struct stat				statResult;

	// ok, let's stat the file		
	if ( stat( filepath, &statResult ) == 0 )
	{
		DBGLOG( "%s, modtime is: %d, last checked modtime is: %d\n", filepath, statResult.st_mtimespec.tv_sec, GetLastModTimeOfFileRead( recordTypeName ) );
		modTimeOfFile = statResult.st_mtimespec.tv_sec;
		if ( modTimeOfFile > GetLastModTimeOfFileRead( recordTypeName ) )
		{
			DBGLOG( "%s has changed, re-read the file\n", filepath );
		}
		else
		{
			DBGLOG( "%s hasn't changed, go with cached results if applicable\n", filepath );
			if ( mCachedFFRef )
				resultDictionaryRef = (CFMutableDictionaryRef)CFDictionaryGetValue( mCachedFFRef, recordTypeRef );
			
			if ( resultDictionaryRef )
				CFRetain( resultDictionaryRef );
		}
	}
	else
		DBGLOG( "Couldn't stat the file (%s) due to: %s!\n", filepath, strerror(errno) );

	if ( resultDictionaryRef == NULL )
	{
		CFMutableStringRef		alternateRecordTypeRef = NULL;
		CFMutableDictionaryRef	alternateDictionaryRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		size_t					curLen;
		FILE*					fd;
		CFStringRef				recordTypeRef = CFStringCreateWithCString( NULL, recordTypeName, kCFStringEncodingASCII );
		
		resultDictionaryRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

		fd = fopen(filepath, "r");
		
		if ( !fd )
		{
			DBGLOG( "CopyResultOfFFLookup returning NULL as we can't open the file at: (%s)!\n", filepath );
			
			if ( recordTypeRef )
				CFRelease( recordTypeRef );
			
			if ( resultDictionaryRef )
				CFRelease( resultDictionaryRef );
				
			if ( alternateDictionaryRef )
				CFRelease( alternateDictionaryRef );
				
			return NULL;
		}
		
		if ( recordTypeRef )
		{
			alternateRecordTypeRef = CFStringCreateMutableCopy( NULL, 0, recordTypeRef );
			CFStringAppend( alternateRecordTypeRef, CFSTR(kAlternateTag) );
		
			char*	lnResult = NULL;
			char*	continuationBuffer = NULL;
			char	lnBuf[kMaxSizeOfParam] = {0,};
			while ( lnResult = fgets( lnBuf, sizeof(lnBuf), fd ) )
			{
				char*	commentPtr = NULL;
				
				if ( lnResult && (lnResult[0] == '#' || lnResult[0] == '\n') )
					continue;	// just skip past any comments or new lines
					
				curLen = strlen( lnResult );		
				
				if ( curLen )
					lnResult[curLen-1] = '\0';
				
				commentPtr = strchr( lnResult, '#' );
				
				if ( commentPtr )
				{
					char*	trailingWSPtr = commentPtr;
					commentPtr++;					// move beyond '#'
					
					trailingWSPtr--;				// move back and we'll blank out any whitespace
					
					while ( isspace( *trailingWSPtr ) )
						trailingWSPtr--;
		
					*(++trailingWSPtr) = '\0';		// Null terminate good stuff			
				}
				
				if ( continuationBuffer )
				{
					// we are combining lines from this file, we need to add the current line to this line
					char*	tempBuf = NULL;
					size_t	lnLen = strlen(lnResult);
					size_t	conBufLen = strlen(continuationBuffer);
					
					tempBuf = (char*)malloc( lnLen + conBufLen + 1 );
					memcpy( tempBuf, continuationBuffer, conBufLen );
					memcpy( &tempBuf[conBufLen], lnResult, lnLen );
					tempBuf[lnLen + conBufLen] = '\0';
					
					free( continuationBuffer );
					continuationBuffer = tempBuf;
					
					lnResult = continuationBuffer;
				}
				
				curLen = strlen(lnResult);
				
				if ( curLen > 0 && lnResult[curLen-1] == '\\' )	// is the last character a line continuation character?
				{
					char*		oldContBuffer = continuationBuffer;
					
					lnResult[curLen-1] = '\0';
					
					// we need to trim this and read the next line adding it to the end of this line
					continuationBuffer = (char*)malloc(  curLen+1 );
					strcpy( continuationBuffer, lnResult );
					
					if ( oldContBuffer )
						free(oldContBuffer);
						
					continue;							// go to next line
				}
				
				if ( lnResult[0] != '\0' )
				{
					CFMutableDictionaryRef		resultRef = CreateFFParseResult( lnResult, recordTypeName );
			
					if ( resultRef )
					{
						if ( commentPtr && commentPtr[0] != '\0' )
						{
							CFStringRef		commentRef = CFStringCreateWithCString( NULL, commentPtr, kCFStringEncodingUTF8 );
							
							if ( commentRef )
							{
								CFDictionaryAddValue( resultRef, CFSTR(kDS1AttrComment), commentRef );
								CFRelease( commentRef );
							}
						}
						
						AddResultToDictionaries( resultDictionaryRef, alternateDictionaryRef, resultRef );
						
						CFRelease( resultRef );
					}
					else
						DBGLOG( "BSDPlugin::CopyResultOfFFLookup no result\n" );
				}
				
				if ( continuationBuffer )
					free( continuationBuffer );
				continuationBuffer = NULL;
			}
		}
		
		if ( recordTypeRef )
		{
			if ( mCachedFFRef )
			{
				CFDictionarySetValue( mCachedFFRef, recordTypeRef, resultDictionaryRef );
				CFDictionarySetValue( mCachedFFRef, alternateRecordTypeRef, alternateDictionaryRef );
			}
			
			CFRelease( recordTypeRef );
			
			if ( alternateRecordTypeRef )
				CFRelease( alternateRecordTypeRef );
			alternateRecordTypeRef = NULL;
		}
		
		if ( alternateDictionaryRef )
			CFRelease( alternateDictionaryRef );
		alternateDictionaryRef = NULL;
		
		fclose( fd );
		
		DBGLOG( "%s, setting last checked modtime to: %d\n", filepath, modTimeOfFile );
		SetLastModTimeOfFileRead( recordTypeName, modTimeOfFile );

		if ( modTimeOfFile != GetLastModTimeOfFileRead( recordTypeName ) )
			DBGLOG( "setting mod time didn't take!\n" );
	}
	
	return resultDictionaryRef;
}
#endif

void BSDPlugin::AddResultToDictionaries( CFMutableDictionaryRef primaryDictRef, CFMutableDictionaryRef alternateDictRef, CFDictionaryRef resultRef )
{
	CFStringRef		recordName;
	CFStringRef		recordRealName;
	
	recordName = (CFStringRef)CFDictionaryGetValue(resultRef, CFSTR(kDSNAttrRecordName));
	
	if ( recordName && CFGetTypeID(recordName) == CFArrayGetTypeID() )
	{
		CFArrayRef	recordNameList = (CFArrayRef)recordName;
		CFIndex		numRecordNames = CFArrayGetCount(recordNameList);
		
		for ( CFIndex i=0; i<numRecordNames; i++ )
		{
			recordName = (CFStringRef)CFArrayGetValueAtIndex( recordNameList, i );
			
			if ( recordName && CFGetTypeID(recordName) == CFStringGetTypeID() )
			{
				if ( i==0 )
					CFDictionaryAddValue( primaryDictRef, recordName, resultRef );				// add primary name to primary Dict
				else
					CFDictionaryAddValue( alternateDictRef, recordName, resultRef );			// add others to alternate
			}
		}
	}
	else if ( recordName && CFGetTypeID(recordName) == CFStringGetTypeID())
		CFDictionaryAddValue( primaryDictRef, recordName, resultRef );
	
	recordRealName = (CFStringRef)CFDictionaryGetValue(resultRef, CFSTR(kDS1AttrDistinguishedName));	// add entries for the real names as well
	
	if ( recordRealName && CFGetTypeID(recordRealName) == CFArrayGetTypeID() )
	{
		CFArrayRef	recordRealNameList = (CFArrayRef)recordRealName;
		CFIndex		numrecordRealNames = CFArrayGetCount(recordRealNameList);
		
		for ( CFIndex i=0; i<numrecordRealNames; i++ )
		{
			recordRealName = (CFStringRef)CFArrayGetValueAtIndex( recordRealNameList, i );
			
			if ( recordRealName && CFGetTypeID(recordRealName) == CFStringGetTypeID() )
			{
				CFDictionaryAddValue( alternateDictRef, recordRealName, resultRef );			// add all to alternate
			}
		}
	}
	else if ( recordRealName && CFGetTypeID(recordRealName) == CFStringGetTypeID())
		CFDictionaryAddValue( alternateDictRef, recordRealName, resultRef );
}

char* BSDPlugin::CopyResultOfNISLookup( NISLookupType type, const char* recordTypeName, const char* keys )
{
    char*		resultPtr = NULL;
	const char*	argv[8] = {0};
	const char* pathArg = NULL;
    Boolean		canceled = false;
	int			callTimedOut = 0;
	
	DBGLOG( "BSDPlugin::CopyResultOfNISLookup started type: %d, recordTypeName: %s, keys: %s\n", type, (recordTypeName)?recordTypeName:"NULL", (keys)?keys:"NULL" );
	// we can improve here by saving these results and caching them...
	switch (type)
	{
		case kNISypcat:
		{
			if ( gSkipYPCatOnRecordNames && strcmp( recordTypeName, kNISRecordTypeUsers ) == 0 )
			{
				DBGLOG( "BSDPlugin::CopyResultOfNISLookup calling kNISypcat, returning NULL since %s exists\n", kSkipYPCatOnRecordNamesFilePath );
				return NULL;
			}
			
			argv[0] = kNISypcatPath;
			argv[1] = "-k";
			argv[2] = "-d";
			argv[3] = mLocalNodeString;
			argv[4] = recordTypeName;
			
			pathArg = kNISypcatPath;
		}
		break;
	
		case kNISdomainname:
		{
			argv[0] = kNISdomainnamePath;
			argv[1] = recordTypeName;
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
			argv[6] = recordTypeName;
			
			pathArg = kNISypmatchPath;
		}
		break;
	};
			
	if ( myexecutecommandas( NULL, pathArg, argv, false, 10, &resultPtr, &canceled, getuid(), getgid(), &callTimedOut ) < 0 )
	{
		DBGLOG( "BSDPlugin::CopyResultOfNISLookup failed\n" );
	}
			
	if ( type == kNISbind && callTimedOut )
	{
		syslog( LOG_ALERT, "ypbind failed to locate NIS Server, some services may be unavailable...\n" );
	}

	DBGLOG( "BSDPlugin::CopyResultOfNISLookup finished type: %d, recordTypeName: %s, keys: %s\n", type, (recordTypeName)?recordTypeName:"NULL", (keys)?keys:"NULL" );
	return resultPtr;
}

void BSDPlugin::DoRecordsLookup(	CFMutableArrayRef	resultArrayRef,
									Boolean				isFFRecord, 
									const char*			recordTypeName,
									char*				recordName,
									tDirPatternMatch	inAttributePatternMatch,
									tDataNodePtr		inAttributePatt2Match,
									char*				attributeKeyToMatch )
{
	CFDictionaryRef		cachedResult = NULL;

	if ( !resultArrayRef )
	{
		DBGLOG( "BSDPlugin::DoRecordsLookup resultArrayRef passed in is NULL!\n" );
		return;
	}

	if ( isFFRecord )
		DBGLOG( "BSDPlugin::DoRecordsLookup param %s lookup\n", (isFFRecord)?"/BSD/local":"NIS" );
		
	if ( recordTypeName )
		DBGLOG( "BSDPlugin::DoRecordsLookup param recordTypeName: %s\n", recordTypeName );
		
	if ( recordName )
		DBGLOG( "BSDPlugin::DoRecordsLookup param recordName: %s\n", recordName );
		
	DBGLOG( "BSDPlugin::DoRecordsLookup param inAttributePatternMatch: 0x%x\n", inAttributePatternMatch );
		
	if ( inAttributePatt2Match && inAttributePatt2Match->fBufferData )
		DBGLOG( "BSDPlugin::DoRecordsLookup param inAttributePatt2Match: %s\n", inAttributePatt2Match->fBufferData );
		
	if ( attributeKeyToMatch )
		DBGLOG( "BSDPlugin::DoRecordsLookup param attributeKeyToMatch: %s\n", attributeKeyToMatch );
		
	if (	inAttributePatternMatch != eDSExact
		&&	inAttributePatternMatch != eDSiExact
		&&	inAttributePatternMatch != eDSAnyMatch
		&&	inAttributePatternMatch != eDSStartsWith 
		&&	inAttributePatternMatch != eDSiStartsWith 
		&&	inAttributePatternMatch != eDSEndsWith 
		&&	inAttributePatternMatch != eDSiEndsWith 
		&&	inAttributePatternMatch != eDSContains
		&&	inAttributePatternMatch != eDSiContains )
	{
		DBGLOG( "BSDPlugin::DoRecordsLookup called with a pattern match we haven't implemented yet: 0x%.4x\n", inAttributePatternMatch );
		throw( eNotYetImplemented );
    }
	
	if ( recordName )
	{
		cachedResult = CopyRecordResult( isFFRecord, recordTypeName, recordName );
		
		if ( cachedResult )
		{
			if ( RecordIsAMatch( cachedResult, inAttributePatternMatch, inAttributePatt2Match, attributeKeyToMatch ) )
				CFArrayAppendValue( resultArrayRef, cachedResult );
			else
				DBGLOG( "BSDPlugin::DoRecordsLookup RecordIsAMatch returned false\n" );
			
			CFRelease( cachedResult );
		}
		else
			DBGLOG( "BSDPlugin::DoRecordsLookup couldn't find a matching result\n" );
	}
	// if we are not FlatFiles and we are looking for User UniqueID or Group PrimaryGroupID we can match instead of ypcat
	else if( isFFRecord == false && (inAttributePatternMatch == eDSiExact || inAttributePatternMatch == eDSExact) &&
			 ((strcmp(recordTypeName, kNISRecordTypeUsers) == 0 && strcmp(attributeKeyToMatch, kDS1AttrUniqueID) == 0) || 
			  (strcmp(recordTypeName, kNISRecordTypeGroups) == 0 && strcmp(attributeKeyToMatch, kDS1AttrPrimaryGroupID) == 0)) ) 
	{
		char*			resultPtr = NULL;

		DBGLOG( "BSDPlugin::DoRecordsLookup we are not FlatFiles and we are looking for User UniqueID or Group PrimaryGroupID we can match instead of ypcat\n" );
		// re-assign the type name to the byuid / bygid maps instead
		if (strcmp(recordTypeName, kNISRecordTypeUsers) == 0)
			recordTypeName = kNISRecordTypeUsersByUID;
		else
			recordTypeName = kNISRecordTypeGroupsByGID;
		
		resultPtr = CopyResultOfNISLookup( kNISypmatch, recordTypeName, inAttributePatt2Match->fBufferData );
		if ( resultPtr )
		{
			if ( strstr( resultPtr, kBindErrorString ) && !mWeLaunchedYPBind )
			{
				// it looks like ypbind may not be running, lets take a look and run it ourselves if need be.
				DBGLOG( "BSDPlugin::CopyRecordResult got an error, implying that ypbind may not be running\n%s", resultPtr );
				
				free( resultPtr );
				resultPtr = NULL;
				
				DBGLOG( "BSDPlugin::CopyRecordResult attempting to launch ypbind\n" );
#ifdef ONLY_TRY_LAUNCHING_YPBIND_ONCE
				mWeLaunchedYPBind = true;
#endif
				resultPtr = CopyResultOfNISLookup( kNISbind );			
				
				if ( resultPtr )
				{
					free( resultPtr );
					resultPtr = NULL;
				}
				
				// now try again.
				resultPtr = CopyResultOfNISLookup( kNISypmatch, recordTypeName, inAttributePatt2Match->fBufferData );
			}
		}
		
		if ( resultPtr && strncmp( resultPtr, "No such map", strlen("No such map") ) != 0 && strncmp( resultPtr, "Can't match key", strlen("Can't match key") ) != 0 )
		{
			char*				curPtr = resultPtr;
			char*				eoln = NULL;
			char*				key = NULL;
			char*				value = NULL;
			
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
				
				if ( key && value )
				{
					DBGLOG( "BSDPlugin::CopyMapResults key is: %s, value is: %s\n", key, value );
					CFMutableDictionaryRef		resultRef = CreateNISParseResult( value, recordTypeName );
					
					if ( resultRef )
					{
						CFArrayAppendValue( resultArrayRef, resultRef );
						CFRelease( resultRef );
					}
					else
						DBGLOG( "BSDPlugin::CopyMapResults no result\n" );
				}
			}
		}
		else
		{
			DBGLOG("BSDPlugin::CopyMapResults got an error: %s\n", resultPtr );
		}
			
		free( resultPtr );
		resultPtr = NULL;
	}
	else
	if (	gDisableDisplayNameLookups && attributeKeyToMatch && ( (strcmp(attributeKeyToMatch, kDS1AttrDistinguishedName) == 0)
		||	(strcmp(recordTypeName, kNISRecordTypeGroups) == 0 && strcmp(attributeKeyToMatch, kDSNAttrNestedGroups) == 0 ) )	)
	{
		DBGLOG("BSDPlugin::CopyMapResults skipping a lookup we know is not supported\n" );
	}
	else
	{
		cachedResult = CopyMapResults( isFFRecord, recordTypeName );
		
		if ( cachedResult && ( (inAttributePatt2Match && inAttributePatt2Match->fBufferData) || inAttributePatternMatch == eDSAnyMatch ) )
		{
			CFStringRef			patternRef = NULL;
			CFStringRef			attributeNameToMatchRef = NULL;
			
			if ( inAttributePatt2Match && inAttributePatt2Match->fBufferData )
				patternRef = CFStringCreateWithCString( NULL, inAttributePatt2Match->fBufferData, kCFStringEncodingUTF8 );
			
			if ( attributeKeyToMatch )
			{
				DBGLOG( "BSDPlugin::DoRecordsLookup looking for records with keys (%s) matching (%s)\n", attributeKeyToMatch, ( inAttributePatt2Match && inAttributePatt2Match->fBufferData )?inAttributePatt2Match->fBufferData:"null" );
				
				attributeNameToMatchRef = CFStringCreateWithCString( NULL, attributeKeyToMatch, kCFStringEncodingUTF8 );
			}
			
			ResultMatchContext	context = { inAttributePatternMatch, patternRef, attributeNameToMatchRef, resultArrayRef };
		
			CFDictionaryApplyFunction( cachedResult, FindAllAttributeMatches, &context );

			DBGLOG( "BSDPlugin::DoRecordsLookup FindMatchingResults found %ld matches\n", CFArrayGetCount(resultArrayRef) );
		
			if ( patternRef )
				CFRelease( patternRef );
		
			if ( attributeNameToMatchRef )
				CFRelease( attributeNameToMatchRef );
		}
		else
			DBGLOG( "BSDPlugin::DoRecordsLookup couldn't find any matching results\n" );
	
		if ( cachedResult )
			CFRelease( cachedResult );
	}
}

CFDictionaryRef BSDPlugin::CopyMapResults( Boolean isFFRecord, const char* recordTypeName )
{
	if ( !recordTypeName )
		return NULL;

	DBGLOG("BSDPlugin::CopyMapResults on %s for %s\n", (isFFRecord)?"/BSD/local":"NIS", recordTypeName);
		
	CFDictionaryRef			results = NULL;
	CFMutableDictionaryRef	newResult = NULL;
	CFMutableDictionaryRef	alternateDictionaryRef = NULL;
	CFStringRef				recordTypeRef = CFStringCreateWithCString( NULL, recordTypeName, kCFStringEncodingUTF8 );
	CFMutableStringRef		alternateRecordTypeRef = NULL;
	
	if ( recordTypeRef )
	{
		alternateRecordTypeRef = CFStringCreateMutableCopy( NULL, 0, recordTypeRef );	
		CFStringAppend( alternateRecordTypeRef, CFSTR(kAlternateTag) );
	
#ifdef BUILDING_COMBO_PLUGIN
		if ( isFFRecord )
		{
			LockFFCache();

			results = CopyResultOfFFLookup( recordTypeName, recordTypeRef );
			
			UnlockFFCache();
		}
		else
#endif	
		{
			LockMapCache();
	
			results = (CFDictionaryRef)CFDictionaryGetValue( mCachedMapsRef, recordTypeRef );
	
			if ( results )
			{
				CFRetain( results );
	
				DBGLOG( "BSDPlugin::CopyMapResults returning Map cached result\n" );
			}
			
			UnlockMapCache();
		}
	}
	else
		DBGLOG("BSDPlugin::CopyMapResults couldn't make a CFStringRef out of recordTypeName: %s\n", recordTypeName );
	
	if ( !results )
	{
		DBGLOG("BSDPlugin::CopyMapResults no cached entry for map: %s, doing a lookup...\n", recordTypeName );
		char*					resultPtr = NULL;

		newResult = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		alternateDictionaryRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		
		if ( !isFFRecord )
			resultPtr = CopyResultOfNISLookup( kNISypcat, recordTypeName );
		
		if ( resultPtr )
		{
			if ( strstr( resultPtr, kBindErrorString ) && !mWeLaunchedYPBind )
			{
				// it looks like ypbind may not be running, lets take a look and run it ourselves if need be.
				DBGLOG( "BSDPlugin::CopyMapResults got an error, implying that ypbind may not be running\n" );
				
				free( resultPtr );
				resultPtr = NULL;
				
				DBGLOG( "BSDPlugin::CopyMapResults attempting to launch ypbind\n" );
#ifdef ONLY_TRY_LAUNCHING_YPBIND_ONCE
				mWeLaunchedYPBind = true;
#endif
				resultPtr = CopyResultOfNISLookup( kNISbind );			
			
				if ( resultPtr )
				{
					free( resultPtr );
					resultPtr = NULL;
				}
			
				// now try again.
				resultPtr = CopyResultOfNISLookup( kNISypcat, recordTypeName );
			}

			if ( resultPtr != NULL && (strncmp( resultPtr, "No such map", strlen("No such map") ) == 0 || strncmp( resultPtr, "Can't match key", strlen("Can't match key") ) == 0) )
			{
				DBGLOG("BSDPlugin::CopyMapResults got an error: %s\n", resultPtr );
				free( resultPtr );
				resultPtr = NULL;

				if ( recordTypeRef )
					CFRelease( recordTypeRef );
				
				if ( alternateRecordTypeRef )
					CFRelease( alternateRecordTypeRef );
					
				if ( newResult )
					CFRelease( newResult );
				
				if ( alternateDictionaryRef )
					CFRelease( alternateDictionaryRef );
					
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
				
				if ( key && value )
				{
					DBGLOG( "BSDPlugin::CopyMapResults key is: %s, value is: %s\n", key, value );
					CFMutableStringRef		dupValueCheckRef = CFStringCreateMutable( NULL, 0 );
					
					if ( dupValueCheckRef )
					{
						CFStringAppendCString( dupValueCheckRef, key, kCFStringEncodingUTF8 );
						CFStringAppendCString( dupValueCheckRef, value, kCFStringEncodingUTF8 );
						
						if ( CFSetContainsValue( dupFilterSetRef, dupValueCheckRef ) )
						{
							CFRelease( dupValueCheckRef );

							DBGLOG( "BSDPlugin::CopyMapResults filtering duplicate result key is: %s, val is: %s\n", key, value );
							continue;
						}
					
						CFMutableDictionaryRef		resultRef = CreateNISParseResult( value, recordTypeName );
			
						if ( resultRef )
						{
							if ( dupValueCheckRef )
								CFSetAddValue( dupFilterSetRef, dupValueCheckRef );
							
							AddResultToDictionaries( newResult, alternateDictionaryRef, resultRef );
						
							CFRelease( resultRef );
						}
						else
							DBGLOG( "BSDPlugin::CopyMapResults no result\n" );
					
						CFRelease( dupValueCheckRef );
					}
					else
						DBGLOG( "BSDPlugin::CopyMapResults, couldn't make a CFString out of the value (%s)!!!\n", value );
				}
			}
	
			if ( dupFilterSetRef )
			{
				CFRelease( dupFilterSetRef );
			}
			
			free( resultPtr );
			resultPtr = NULL;
		}
		
		results = newResult;

		if ( recordTypeRef )
		{
#ifdef BUILDING_COMBO_PLUGIN
			if ( isFFRecord )
			{
				LockFFCache();
	
				CFDictionarySetValue( mCachedFFRef, recordTypeRef, results ); // only set this if we did a full lookup
				CFDictionarySetValue( mCachedFFRef, alternateRecordTypeRef, alternateDictionaryRef );

				UnlockFFCache();
			}
			else
#endif
			{
				LockMapCache();
	
				CFDictionarySetValue( mCachedMapsRef, recordTypeRef, results ); // only set this if we did a full cat lookup
				CFDictionarySetValue( mCachedMapsRef, alternateRecordTypeRef, alternateDictionaryRef );
	
				UnlockMapCache();
			}
		}
		else
			DBGLOG( "BSDPlugin::CopyMapResults couldn't set the latest cached map in mCachedMapsRef since we couldn't make a recordTypeRef!\n" ); 

		if ( alternateDictionaryRef )
			CFRelease( alternateDictionaryRef );
		alternateDictionaryRef = NULL;
	}
	
	if ( recordTypeRef )
		CFRelease( recordTypeRef );
	recordTypeRef = NULL;
	
	if ( alternateRecordTypeRef )
		CFRelease( alternateRecordTypeRef );
	alternateRecordTypeRef = NULL;
	
	DBGLOG( "BSDPlugin::CopyMapResults results has %ld members\n", CFDictionaryGetCount(results) );
	
	return results;
}

Boolean BSDPlugin::RecordIsAMatch(	CFDictionaryRef		recordRef,
									tDirPatternMatch	inAttributePatternMatch,
									tDataNodePtr		inAttributePatt2Match,
									char*				attributeKeyToMatch )
{
	Boolean		isAMatch = false;
	
	if ( inAttributePatternMatch == eDSAnyMatch )
		isAMatch = true;
	else if ( inAttributePatt2Match )
	{
		CFStringRef attrPatternRef = CFStringCreateWithCString( NULL, inAttributePatt2Match->fBufferData, kCFStringEncodingUTF8 );
		CFStringRef	attrKeyToMatchRef = NULL;
		
		if ( attrPatternRef )
		{
			if ( attributeKeyToMatch )
				attrKeyToMatchRef = CFStringCreateWithCString( NULL, attributeKeyToMatch, kCFStringEncodingUTF8 );
				
			AttrDataMatchContext		context = { inAttributePatternMatch, attrPatternRef, attrKeyToMatchRef, false };
			
			CFDictionaryApplyFunction( recordRef, FindAttributeMatch, &context );
			
			if ( context.foundAMatch )
			{
				isAMatch = true;
			}
			else
				DBGLOG( "BSDPlugin::RecordIsAMatch result didn't have the attribute: %s\n", inAttributePatt2Match->fBufferData );

			CFRelease( attrPatternRef );
			
			if ( attrKeyToMatchRef )
				CFRelease( attrKeyToMatchRef );
		}
		else
			DBGLOG( "BSDPlugin::RecordIsAMatch couldn't make a UTF8 string out of: %s!\n", inAttributePatt2Match->fBufferData );
	}
	
	return isAMatch;
}

CFDictionaryRef BSDPlugin::CopyRecordResult( Boolean isFFRecord, const char* recordTypeName, char* recordName )
{	
	if ( !recordTypeName || !recordName )
	{
		DBGLOG( "BSDPlugin::CopyRecordResult null parameter! recordTypeName: 0x%x, recordName: 0x%x\n", (int)recordTypeName, (int)recordName );
		return NULL;
	}
	
	CFStringRef				recordTypeRef = CFStringCreateWithCString( NULL, recordTypeName, kCFStringEncodingUTF8 );
	CFStringRef				recordNameRef = CFStringCreateWithCString( NULL, recordName, kCFStringEncodingUTF8 );
	CFDictionaryRef			returnRecordRef = NULL;
	
	if ( !recordNameRef || !recordTypeRef )
	{
		DBGLOG( "BSDPlugin::CopyRecordResult couldn't convert to CFStringRef! recordNameRef: 0x%x, recordTypeRef: 0x%x\n", (int)recordNameRef, (int)recordTypeRef );

		if ( recordTypeRef )
			CFRelease( recordTypeRef );
		
		if ( recordNameRef )
			CFRelease( recordNameRef );
			
		return NULL;
	}
	
	CFMutableStringRef		alternateRecordTypeRef = CFStringCreateMutableCopy( NULL, 0, recordTypeRef );

	CFStringAppend( alternateRecordTypeRef, CFSTR(kAlternateTag) );

#ifdef BUILDING_COMBO_PLUGIN
	if ( isFFRecord )
	{
		LockFFCache();
	
		CFDictionaryRef			cachedFFRef = CopyResultOfFFLookup( recordTypeName, recordTypeRef );
			
		if ( cachedFFRef )
		{
			// lookup by primary record name first
			returnRecordRef = (CFDictionaryRef)CFDictionaryGetValue( cachedFFRef, recordNameRef );
			
			if ( !returnRecordRef )
			{
				// try looking in the alternate cache that has duplicate entries by all record names
				CFRelease( cachedFFRef );
				
				cachedFFRef = (CFDictionaryRef)CFDictionaryGetValue( mCachedFFRef, alternateRecordTypeRef );	// try using the alternate mappings
				
				if ( cachedFFRef )
				{
					returnRecordRef = (CFDictionaryRef)CFDictionaryGetValue( cachedFFRef, recordNameRef );
					CFRetain( cachedFFRef );
				}
			}
			
			if ( returnRecordRef )
			{
				CFRetain( returnRecordRef );
				DBGLOG( "BSDPlugin::CopyRecordResult returning cached BSD/local result\n" );
			}
		
			CFRelease( cachedFFRef );
		}
		
		UnlockFFCache();
	}
	else
#endif
	{
		LockMapCache();
	
		CFDictionaryRef		cachedMapRef = (CFDictionaryRef)CFDictionaryGetValue( mCachedMapsRef, recordTypeRef );
		
		if ( cachedMapRef )
		{
			returnRecordRef = (CFDictionaryRef)CFDictionaryGetValue( cachedMapRef, recordNameRef );
			
			if ( !returnRecordRef )
			{
				// try looking in the alternate cache that has duplicate entries by all record names
				cachedMapRef = (CFDictionaryRef)CFDictionaryGetValue( mCachedMapsRef, alternateRecordTypeRef );	// try using the alternate mappings
				
				if ( cachedMapRef )
					returnRecordRef = (CFDictionaryRef)CFDictionaryGetValue( cachedMapRef, recordNameRef );
			}
			
			if ( returnRecordRef )
			{
				CFRetain( returnRecordRef );
				DBGLOG( "BSDPlugin::CopyRecordResult returning cached NIS result\n" );
			}
		}
		
		UnlockMapCache();
	}
	
	if ( !returnRecordRef )
	{
		char*		resultPtr = NULL;
		
		if ( !isFFRecord )
			resultPtr = CopyResultOfNISLookup( (recordName)?kNISypmatch:kNISypcat, recordTypeName, recordName );
		
		if ( resultPtr )
		{
			if ( strstr( resultPtr, kBindErrorString ) && !mWeLaunchedYPBind )
			{
				// it looks like ypbind may not be running, lets take a look and run it ourselves if need be.
				DBGLOG( "BSDPlugin::CopyRecordResult got an error, implying that ypbind may not be running\n" );
				
				free( resultPtr );
				resultPtr = NULL;
				
				DBGLOG( "BSDPlugin::CopyRecordResult attempting to launch ypbind\n" );
#ifdef ONLY_TRY_LAUNCHING_YPBIND_ONCE
				mWeLaunchedYPBind = true;
#endif
				resultPtr = CopyResultOfNISLookup( kNISbind );			
			
				if ( resultPtr )
				{
					free( resultPtr );
					resultPtr = NULL;
				}
			
				// now try again.
				resultPtr = CopyResultOfNISLookup( (recordName)?kNISypmatch:kNISypcat, recordTypeName, recordName );
			}
			
			if ( isFFRecord || ( strncmp( resultPtr, "No such map", strlen("No such map") ) != 0 && strncmp( resultPtr, "Can't match key", strlen("Can't match key") ) != 0 ) )
			{
				char*					curPtr = resultPtr;
				char*					eoln = NULL;
				char*					value = NULL;
				CFMutableSetRef			dupFilterSetRef = CFSetCreateMutable( NULL, 0, &kCFCopyStringSetCallBacks );
				CFMutableDictionaryRef	resultDictionaryRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
				CFMutableDictionaryRef	alternateDictionaryRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
				
				while ( curPtr && curPtr[0] != '\0' )
				{
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
					
					DBGLOG( "BSDPlugin::CopyRecordResult value is: %s\n", value );
					if ( value )
					{
// if we bail out here, then we are going to commit ourselves to looking through the data O(n/2) each time with a total expense of O(n*n/2)
// if we just parse everything now, we take the initial hit of O(n) but all subsequent calls will be O(1).
/*						if ( recordName && strstr( recordName, key ) == NULL )
						{
							DBGLOG( "BSDPlugin::CopyRecordResult key is: %s, looking for recordName: %s so skipping\n", key, recordName );
							continue;
						}
*/							
						CFStringRef		dupValueCheckRef = CFStringCreateWithCString( NULL, value, kCFStringEncodingUTF8 );
						
						if ( dupValueCheckRef )
						{
							if ( CFSetContainsValue( dupFilterSetRef, dupValueCheckRef ) )
							{
								CFRelease( dupValueCheckRef );
								DBGLOG( "BSDPlugin::CopyRecordResult filtering duplicate result, val is: %s\n", value );
								continue;
							}
						
							CFMutableDictionaryRef		resultRef = CreateNISParseResult( value, recordTypeName );
			
							if ( resultRef )
							{
								if ( dupValueCheckRef )
									CFSetAddValue( dupFilterSetRef, dupValueCheckRef );
								
								AddResultToDictionaries( resultDictionaryRef, alternateDictionaryRef, resultRef );
							
								CFRelease( resultRef );
							}
							else
								DBGLOG( "BSDPlugin::CopyRecordResult no result\n" );
						
							CFRelease( dupValueCheckRef );
						}
						else
							DBGLOG( "BSDPlugin::CopyRecordResult, couldn't make a CFString out of the value (%s)!!!\n", value );
					}
				}
		
				if ( dupFilterSetRef )
				{
					CFRelease( dupFilterSetRef );
				}
				
				LockMapCache();
				
				CFDictionarySetValue( mCachedMapsRef, recordTypeRef, resultDictionaryRef );
				CFDictionarySetValue( mCachedMapsRef, alternateRecordTypeRef, alternateDictionaryRef );
	
				UnlockMapCache();
				
				returnRecordRef = (CFDictionaryRef)CFDictionaryGetValue( resultDictionaryRef, recordNameRef );
				
				if ( returnRecordRef )
				{
					CFRetain( returnRecordRef );
				}
				
				if ( resultDictionaryRef )
					CFRelease( resultDictionaryRef );
				resultDictionaryRef = NULL;

				if ( alternateDictionaryRef )
					CFRelease( alternateDictionaryRef );
				alternateDictionaryRef = NULL;
			}
			else
			{
				DBGLOG( "BSDPlugin::CopyRecordResult got an error: %s\n", resultPtr );
			}

			free( resultPtr );
			resultPtr = NULL;
		}
		
		if ( returnRecordRef )
			DBGLOG( "BSDPlugin::CopyRecordResult returnRecordRef has %ld members\n", CFDictionaryGetCount(returnRecordRef) );
		else
			DBGLOG( "BSDPlugin::CopyRecordResult no returnRecordRef\n" );
	}
	
	if ( recordTypeRef )
		CFRelease( recordTypeRef );
	recordTypeRef = NULL;
	
	if ( alternateRecordTypeRef )
		CFRelease( alternateRecordTypeRef );
	alternateRecordTypeRef = NULL;
	
	if ( recordNameRef )
		CFRelease( recordNameRef );
	recordNameRef = NULL;
		
	return returnRecordRef;
}

#ifdef BUILDING_COMBO_PLUGIN
CFMutableDictionaryRef BSDPlugin::CreateFFParseResult(char *data, const char* recordTypeName)
{
	if (data == NULL) return NULL;
	if (data[0] == '#') return NULL;

	CFMutableDictionaryRef	returnResult = NULL;
	
	if ( strcmp( recordTypeName, kFFRecordTypeUsers ) == 0 )
		returnResult = ff_parse_user_A(data);
	else if ( strcmp( recordTypeName, kFFRecordTypeGroups ) == 0 )
		returnResult = ff_parse_group(data);
	else if ( strcmp( recordTypeName, kFFRecordTypeHosts ) == 0 )
		returnResult = ff_parse_host(data);
	else if ( strcmp( recordTypeName, kFFRecordTypeNetworks ) == 0 )
		returnResult = ff_parse_network(data);
	else if ( strcmp( recordTypeName, kFFRecordTypeServices ) == 0 )
		returnResult = ff_parse_service(data);
	else if ( strcmp( recordTypeName, kFFRecordTypeProtocols ) == 0 )
		returnResult = ff_parse_protocol(data);
	else if ( strcmp( recordTypeName, kFFRecordTypeRPC ) == 0 )
		returnResult = ff_parse_rpc(data);
	else if ( strcmp( recordTypeName, kFFRecordTypeMounts ) == 0 )
		returnResult = ff_parse_mount(data);
	else if ( strcmp( recordTypeName, kFFRecordTypePrintService ) == 0 )
		returnResult = ff_parse_printer(data);
	else if ( strcmp( recordTypeName, kFFRecordTypeBootParams ) == 0 )
		returnResult = ff_parse_bootparam(data);
	else if ( strcmp( recordTypeName, kFFRecordTypeBootp ) == 0 )
		returnResult = ff_parse_bootp(data);
	else if ( strcmp( recordTypeName, kFFRecordTypeAlias ) == 0 )
		ff_parse_alias(data);
	else if ( strcmp( recordTypeName, kFFRecordTypeEthernets ) == 0 )
		returnResult = ff_parse_ethernet(data);
	else if ( strcmp( recordTypeName, "netgroup" ) == 0 )
		returnResult = ff_parse_netgroup(data);
	
	if ( returnResult && mFFMetaNodeLocationRef )
	{
		CFDictionarySetValue( returnResult, CFSTR(kDSNAttrMetaNodeLocation), mFFMetaNodeLocationRef );
	}
		
	return returnResult;
}
#endif

CFMutableDictionaryRef BSDPlugin::CreateNISParseResult(char *data, const char* recordTypeName)
{
	if (data == NULL) return NULL;
	if (data[0] == '#') return NULL;

	CFMutableDictionaryRef	returnResult = NULL;
	
	if ( strcmp( recordTypeName, kNISRecordTypeUsers ) == 0 || strcmp( recordTypeName, kNISRecordTypeUsersByUID ) == 0 )
		returnResult = ff_parse_user(data);
	else if ( strcmp( recordTypeName, kNISRecordTypeGroups ) == 0 || strcmp( recordTypeName, kNISRecordTypeGroupsByGID ) == 0 )
		returnResult = ff_parse_group(data);
	else if ( strcmp( recordTypeName, kNISRecordTypeHosts ) == 0 )
		returnResult = ff_parse_host(data);
	else if ( strcmp( recordTypeName, kNISRecordTypeNetworks ) == 0 )
		returnResult = ff_parse_network(data);
	else if ( strcmp( recordTypeName, kNISRecordTypeServices ) == 0 )
		returnResult = ff_parse_service(data);
	else if ( strcmp( recordTypeName, kNISRecordTypeProtocols ) == 0 )
		returnResult = ff_parse_protocol(data);
	else if ( strcmp( recordTypeName, kNISRecordTypeRPC ) == 0 )
		returnResult = ff_parse_rpc(data);
	else if ( strcmp( recordTypeName, kNISRecordTypeMounts ) == 0 )
		returnResult = ff_parse_mount(data);
	else if ( strcmp( recordTypeName, kNISRecordTypePrintService ) == 0 )
		returnResult = ff_parse_printer(data);
	else if ( strcmp( recordTypeName, kNISRecordTypeBootParams ) == 0 )
		returnResult = ff_parse_bootparam(data);
	else if ( strcmp( recordTypeName, kNISRecordTypeBootp ) == 0 )
		returnResult = ff_parse_bootp(data);
	else if ( strcmp( recordTypeName, kNISRecordTypeAlias ) == 0 )
		ff_parse_alias(data);
	else if ( strcmp( recordTypeName, kNISRecordTypeEthernets ) == 0 )
		returnResult = ff_parse_ethernet(data);
	else if ( strcmp( recordTypeName, "netgroup" ) == 0 )
		returnResult = ff_parse_netgroup(data);
	
	if ( returnResult && mNISMetaNodeLocationRef )
	{
		CFDictionarySetValue( returnResult, CFSTR(kDSNAttrMetaNodeLocation), mNISMetaNodeLocationRef );
	}
		
	return returnResult;
}

#pragma mark -


//------------------------------------------------------------------------------------
//	* DoAuthentication
//------------------------------------------------------------------------------------

sInt32 BSDPlugin::DoAuthentication ( sDoDirNodeAuth *inData )
{
	DBGLOG( "BSDPlugin::DoAuthentication fInAuthMethod: %s, fInDirNodeAuthOnlyFlag: %d, fInAuthStepData: %d, fOutAuthStepDataResponse: %d, fIOContinueData: %d\n", (char *)inData->fInAuthMethod->fBufferData, inData->fInDirNodeAuthOnlyFlag, (int)inData->fInAuthStepData, (int)inData->fOutAuthStepDataResponse, (int)inData->fIOContinueData );

    BSDDirNodeRep*			nodeDirRep		= NULL;
	sInt32					siResult		= eDSAuthFailed;
	uInt32					uiAuthMethod	= 0;
	sNISContinueData	   *pContinueData	= NULL;
	char*					userName		= NULL;
	
	try
	{
		LockPlugin();
	
		nodeDirRep = (BSDDirNodeRep*)::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );

	
		if( !nodeDirRep )
		{
			DBGLOG( "BSDPlugin::DoAuthentication called but we couldn't find the nodeDirRep!\n" );
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
					DBGLOG( "BSDPlugin::DoAuthentication we don't support 2 way random!\n" );
					siResult = eDSAuthMethodNotSupported;
				}

				// get the auth authority
				const char*		mapName = NULL;
				
#ifdef BUILDING_COMBO_PLUGIN
				if ( nodeDirRep->IsFFNode() )
					mapName = GetFFTypeFromRecType( kDSStdRecordTypeUsers );
				else
#endif
					mapName = GetNISTypeFromRecType( kDSStdRecordTypeUsers );
				
				CFMutableDictionaryRef	recordResult = CopyRecordLookup( nodeDirRep->IsFFNode(), mapName, userName );
				
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
					DBGLOG( "BSDPlugin::DoAuthentication, unknown user!\n" );
					siResult = eDSAuthUnknownUser;
				}
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

sInt32 BSDPlugin::DoBasicAuth ( tDirNodeReference inNodeRef, tDataNodePtr inAuthMethod, 
								BSDDirNodeRep* nodeDirRep, 
								sNISContinueData** inOutContinueData, 
								tDataBufferPtr inAuthData, tDataBufferPtr outAuthData, 
								bool inAuthOnly, char* inAuthAuthorityData )
{
	sInt32				siResult		= eDSNoErr;
	uInt32				uiAuthMethod	= 0;

	DBGLOG( "BSDPlugin::DoBasicAuth\n" );
	
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
		
		DBGLOG( "BSDPlugin::DoBasicAuth caught exception: %ld\n", siResult );
	}

	return( siResult );

} // DoBasicAuth

//------------------------------------------------------------------------------------
//	* DoUnixCryptAuth
//------------------------------------------------------------------------------------

sInt32 BSDPlugin::DoUnixCryptAuth ( BSDDirNodeRep *nodeDirRep, tDataBuffer *inAuthData, bool inAuthOnly )
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
	char			hashPwd[ 64 ];
	CFMutableDictionaryRef	recordResult = NULL;
	const char*		mapName			= NULL;
	
	DBGLOG( "BSDPlugin::DoUnixCryptAuth\n" );
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

		DBGLOG( "BSDPlugin::DoUnixCryptAuth attempting UNIX Crypt authentication\n" );

#ifdef BUILDING_COMBO_PLUGIN
		if ( nodeDirRep->IsFFNode() )
			mapName = GetFFTypeFromRecType( kDSStdRecordTypeUsers );
		else
#endif
			mapName = GetNISTypeFromRecType( kDSStdRecordTypeUsers );
	
		recordResult = CopyRecordLookup( nodeDirRep->IsFFNode(), mapName, name );
		
		if ( !recordResult )
		{
			DBGLOG( "BSDPlugin::DoUnixCryptAuth, unknown user!\n" );
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
		if ( nisPwd[0] != '\0' )
		{
			siResult = eDSAuthFailed;
			
			::bzero( hashPwd, sizeof(hashPwd) );
			
			// is it MD5?
			if ( strncmp(nisPwd, "$1$", 3) == 0 )
			{
				::strlcpy( hashPwd, ::crypt_md5(pwd, nisPwd), sizeof(hashPwd) );
			}
			else
			{
				// crypt
				salt[ 0 ] = nisPwd[0];
				salt[ 1 ] = nisPwd[1];
				salt[ 2 ] = '\0';
				
				::strlcpy( hashPwd, ::crypt(pwd, salt), sizeof(hashPwd) );
			}
			if ( ::strcmp( hashPwd, nisPwd ) == 0 )
			{
				siResult = eDSNoErr;
			}
		}
		else // nisPwd is == ""
		{
			if ( pwd[0] == '\0' )
			{
				siResult = eDSNoErr;
			}
		}
	}
	
	catch( sInt32 err )
	{
		DBGLOG( "BSDPlugin::DoUnixCryptAuth Crypt authentication error %ld\n", err );
		siResult = err;
	}

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

sInt32 BSDPlugin::GetAuthMethod ( tDataNode *inData, uInt32 *outAuthMethod )
{
	sInt32			siResult		= eDSNoErr;
	uInt32			uiNativeLen		= 0;
	char		   *p				= nil;

	if ( inData == nil )
	{
		*outAuthMethod = kAuthUnknownMethod;
#ifdef DEBUG
		return( eDSAuthParameterError );
#else
		return( eDSAuthFailed );
#endif
	}

	p = (char *)inData->fBufferData;

	DBGLOG( "BSDPlugin::GetAuthMethod using authentication method %s\n", p );

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
			*outAuthMethod = kAuthUnknownMethod;
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

sInt32 BSDPlugin::GetUserNameFromAuthBuffer ( tDataBufferPtr inAuthData, unsigned long inUserNameIndex, 
											  char  **outUserName )
{
	DBGLOG( "BSDPlugin::GetUserNameFromAuthBuffer\n" );
	tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
	if (dataList != NULL)
	{
		*outUserName = dsDataListGetNodeStringPriv(dataList, inUserNameIndex);
		// this allocates a copy of the string
		
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;

		DBGLOG( "BSDPlugin::GetUserNameFromAuthBuffer successfully returning username: %s\n", *outUserName );
		
		return eDSNoErr;
	}
	return eDSInvalidBuffFormat;
}

sInt32 BSDPlugin::VerifyPatternMatch ( const tDirPatternMatch inPatternMatch )
{
	DBGLOG( "BSDPlugin::VerifyPatternMatch\n" );
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
		case eDSiStartsWith:
		case eDSiEndsWith:
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
	else if ( patternMatch == eDSiContains || patternMatch == eDSiStartsWith || patternMatch == eDSiEndsWith )
	{
		CFRange 	result = CFStringFind( resultRecordNameRef, recNameMatchRef, kCFCompareCaseInsensitive );
		
		if ( result.length > 0 )
		{
			if ( patternMatch == eDSiContains )
			{
				resultIsOK = true;
			}
			else if ( patternMatch == eDSiStartsWith && result.location == 0 )
			{
				resultIsOK = true;
			}
			else if ( patternMatch == eDSiEndsWith && result.location == CFStringGetLength(resultRecordNameRef) - result.length )
			{
				resultIsOK = true;
			}
		}
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
			
			if ( CFStringGetLength( valueRef ) > 0 )
				CFArrayAppendValue( (CFMutableArrayRef)valueArrayRef, valueRef );
		}
		else
            DBGLOG( "AddDictionaryDataToAttrData, got unknown value type (%ld), ignore\n", CFGetTypeID(valueTypeRef) );

		if ( valueArrayRef && ::CFStringGetCString( keyRef, keyBuf, sizeof(keyBuf), kCFStringEncodingUTF8 ) )
		{
			char		valueTmpBuf[1024];
			CFIndex		arrayCount = CFArrayGetCount(valueArrayRef);
			
			aTmpData->AppendShort( ::strlen( keyBuf ) );		// attrTypeLen
			aTmpData->AppendString( keyBuf );					// attrType
			
			DBGLOG( "AddDictionaryDataToAttrData, adding keyBuf: %s\n", keyBuf );

			// Append the attribute value count
			aTmpData->AppendShort( arrayCount );	// attrValueCnt

			for ( CFIndex i=0; i< arrayCount; i++ )
			{
				valueRef = (CFStringRef)::CFArrayGetValueAtIndex( valueArrayRef, i );

				if ( !dataContext->attrOnly && ::CFStringGetLength(valueRef) > 0 )
				{
					CFIndex		maxValueEncodedSize = ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueRef), kCFStringEncodingUTF8) + 1;
					
					if ( maxValueEncodedSize > (CFIndex)sizeof(valueTmpBuf) )
						valueBuf = (char*)malloc( maxValueEncodedSize );
					else
						valueBuf = valueTmpBuf;
					
					if ( ::CFStringGetCString( valueRef, valueBuf, maxValueEncodedSize, kCFStringEncodingUTF8 ) )	
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
						
					if ( valueBuf != valueTmpBuf )
						free( valueBuf );
					valueBuf = NULL;
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
    CFStringRef				keyRef			= (CFStringRef)key;
	CFTypeRef				valueRef		= (CFTypeRef)value;
	CFArrayRef				valueArrayRef	= NULL;
	AttrDataMatchContext*	info			= (AttrDataMatchContext*)context;
	
	if ( info->foundAMatch )
		return;
	
	if ( !valueRef )
		return;
		
	if ( info->fAttributeNameToMatchRef && CFStringCompare( keyRef, info->fAttributeNameToMatchRef, kCFCompareCaseInsensitive ) != kCFCompareEqualTo )
	{
		// we aren't matching against all values, but against a specific key and this isn't it
		return;
	}
	
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
	
	CFIndex		arrayCount = CFArrayGetCount(valueArrayRef);
	
	for ( CFIndex i=0; valueArrayRef && i< arrayCount && !info->foundAMatch; i++ )
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
    CFTypeRef				valueRef		= NULL;
	CFDictionaryRef			resultRef		= (CFDictionaryRef)value;
	ResultMatchContext*		info			= (ResultMatchContext*)context;
	Boolean					recordMatches	= false;
	
	if ( info->fInPattMatchType == eDSAnyMatch )
	{
		recordMatches = true;
	}
	else if ( resultRef && CFGetTypeID(resultRef) == CFDictionaryGetTypeID() )
	{
		if ( info->fAttributeNameToMatchRef )
		{
			CFArrayRef		valueArrayRef = NULL;
			
			valueRef = CFDictionaryGetValue( resultRef, info->fAttributeNameToMatchRef );
			
			if (!valueRef)
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
				DBGLOG( "FindAllAttributeMatches, got unknown value type (%ld), ignore\n", CFGetTypeID(valueRef) );
				CFShow( valueRef );
			}
			
			CFIndex		arrayCount = CFArrayGetCount(valueArrayRef);
			
			for ( CFIndex i=0; valueArrayRef && i< arrayCount; i++ )
			{
				CFStringRef valueStringRef = (CFStringRef)::CFArrayGetValueAtIndex( valueArrayRef, i );
		
				if ( IsResultOK( info->fInPattMatchType, valueStringRef, info->fInPatt2MatchRef ) )
				{
					recordMatches = true;
					break;
				}
			}
			
			if ( valueArrayRef )
			{
				CFRelease( valueArrayRef );
			}
		}
		else
		{
			AttrDataMatchContext		context = { info->fInPattMatchType, info->fInPatt2MatchRef, info->fAttributeNameToMatchRef, false };
			
			CFDictionaryApplyFunction( resultRef, FindAttributeMatch, &context );
			
			if ( context.foundAMatch )
				recordMatches = true;
		}
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

#if BUILDING_NSLDEBUG
pthread_mutex_t	bsdSysLogLock = PTHREAD_MUTEX_INITIALIZER;

void ourLog(const char* format, ...)
{
    va_list ap;
    
	pthread_mutex_lock( &bsdSysLogLock );

    va_start( ap, format );
    newlog( format, ap );
    va_end( ap );

	pthread_mutex_unlock( &bsdSysLogLock );
}

void newlog(const char* format, va_list ap )
{
    char	pcMsg[MAXLINE +1];
    
    vsnprintf( pcMsg, MAXLINE, format, ap );

#if LOG_ALWAYS
    syslog( LOG_ALERT, "T:[0x%x] %s", pthread_self(), pcMsg );
#else
    syslog( LOG_ERR, "T:[0x%x] %s", pthread_self(), pcMsg );
#endif
}

#endif

#if LOG_ALWAYS
	int			gDebuggingNSL = 1;
#else
	int			gDebuggingNSL = (getenv("NSLDEBUG") != NULL);
#endif

int IsNSLDebuggingEnabled( void )
{
	return gDebuggingNSL;
}
