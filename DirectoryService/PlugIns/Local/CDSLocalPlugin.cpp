/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
 * @header CDSLocalPlugin
 */


#include "CDSLocalPlugin.h"
#include "CDSLocalConfigureNode.h"

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"
#include "DirServicesPriv.h"
#include "DirServiceMain.h"
#include "CAttributeList.h"
#include "DSEventSemaphore.h"
#include "CBuff.h"
#include "CLog.h"
#include "CDataBuff.h"
#include "CRCCalc.h"
#include "DSUtils.h"
#include "CDSPluginUtils.h"
#include "ServerModuleLib.h"
#include "CRefTable.h"
#include <Security/Authorization.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <notify.h>
#include "Mbrd_MembershipResolver.h"
#include "CCachePlugin.h"
#include "CRefTable.h"
#include "CHandlers.h"

#include "buffer_unpackers.h"
#include "CDSLocalPluginNode.h"
#include "CDSLocalAuthHelper.h"
#include "CDSAuthDefs.h"
#include <DirectoryServiceCore/CContinue.h>
#include <DirectoryService/DirServicesConstPriv.h>

DSEventSemaphore		gKickLocalNodeRequests;
CContinue				*gLocalContinueTable = nil;
extern pid_t			gDaemonPID;
extern in_addr_t		gDaemonIPAddress;
extern CRefTable		gRefTable;
const char*				gProtocolPrefixString = kTopLevelNodeName;

enum
{
	eBuffPad =	16
};

extern dsBool gServerOS;
extern dsBool gDSLocalOnlyMode;
extern char* gDSLocalFilePath;
extern CCachePlugin	*gCacheNode;

CDSLocalPlugin	*gLocalNode		= NULL;

inline bool SupportedMatchType( tDirPatternMatch matchType )
{
	bool	bReturn = false;
	
	switch ( matchType )
	{
		case eDSExact:
		case eDSiExact:
		case eDSStartsWith:
		case eDSiStartsWith:
		case eDSEndsWith:
		case eDSiEndsWith:
		case eDSContains:
		case eDSiContains:
		case eDSAnyMatch:
			bReturn = true;
			break;
		default:
			bReturn = false;
			break;
	}
	
	return bReturn;
}

CFArrayRef dsCopyKerberosServiceList( void )
{
	return gLocalNode->CopyKerberosServiceList();
}

#pragma mark -
#pragma mark Plugin Entry Points

CDSLocalPlugin::CDSLocalPlugin( FourCharCode inSig, const char *inName ) :
	BaseDirectoryPlugin(inSig, inName), 
	mOpenNodeRefsLock("CDSLocalPlugin::mOpenNodeRefsLock"), 
	mOpenRecordRefsLock("CDSLocalPlugin::mOpenRecordRefsLock"), 
	mOpenAttrValueListRefsLock("CDSLocalPlugin::mOpenAttrValueListRefsLock"), 
	mGeneralPurposeLock("CDSLocalPlugin::mGeneralPurposeLock")
{
	mSignature							= inSig;
	mOpenNodeRefs						= CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	mOpenAttrValueListRefs				= CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	mOpenRecordRefs						= CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	mAttrNativeToPrefixedNativeMappings	= CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	mPermissions						= NULL; // created in LoadMappings
	mAttrNativeToStdMappings			= NULL; // created in LoadMappings
	mAttrStdToNativeMappings			= NULL; // created in LoadMappings
	mAttrPrefixedNativeToNativeMappings	= NULL; // created in LoadMappings
	mRecNativeToStdMappings				= NULL; // created in LoadMappings
	mRecStdToNativeMappings				= NULL; // created in LoadMappings
	mRecPrefixedNativeToNativeMappings	= NULL; // created in LoadMappings
	mDSRef								= 0;
	mHashList							= 0;
	mDelayFailedLocalAuthReturnsDeltaInSeconds = 1;
	mPWSFrameworkAvailable				= false;
	
	// not normal references because API references are translated, so we have to keep separate
	mInternalNodeRef					= 0;
	mInternalDirRef						= 0;
	
	CContinue *pContinue = new CContinue( CDSLocalPlugin::ContinueDeallocProc );
	if ( __sync_bool_compare_and_swap(&gLocalContinueTable, NULL, pContinue) == false )
		delete pContinue;
	
	__sync_bool_compare_and_swap( &gLocalNode, NULL, this );
	
	CFURLRef	cfURL = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, CFSTR("/System/Library/PrivateFrameworks/PasswordServer.framework"), 
													   kCFURLPOSIXPathStyle, TRUE );
	if ( cfURL != NULL )
	{
		CFBundleRef cfBundle = CFBundleCreate( kCFAllocatorDefault, cfURL );
		if ( cfBundle != NULL )
		{
			mPWSFrameworkAvailable = true;
			DSCFRelease( cfBundle );
		}
		else
		{
			DbgLog( kLogNotice, "CDSLocalPlugin::CDSLocalPlugin - password server framework is NOT available" );
		}
		
		DSCFRelease( cfURL );
	}
}


CDSLocalPlugin::~CDSLocalPlugin( void )
{
	if ( mOpenNodeRefs != NULL )
		CFRelease( mOpenNodeRefs );
	if ( mOpenAttrValueListRefs != NULL )
		CFRelease( mOpenAttrValueListRefs );
	if ( mOpenRecordRefs != NULL )
		CFRelease( mOpenRecordRefs );
	DSCFRelease( mPermissions );
	if ( mAttrNativeToStdMappings != NULL )
		CFRelease( mAttrNativeToStdMappings );
	if ( mAttrStdToNativeMappings != NULL )
		CFRelease( mAttrStdToNativeMappings );
	if ( mAttrPrefixedNativeToNativeMappings != NULL )
		CFRelease( mAttrPrefixedNativeToNativeMappings );
	if ( mAttrNativeToPrefixedNativeMappings != NULL )
		CFRelease( mAttrNativeToPrefixedNativeMappings );
	if ( mRecNativeToStdMappings != NULL )
		CFRelease( mRecNativeToStdMappings );
	if ( mRecStdToNativeMappings != NULL )
		CFRelease( mRecStdToNativeMappings );
	if ( mRecPrefixedNativeToNativeMappings != NULL )
		CFRelease( mRecPrefixedNativeToNativeMappings );
	if ( mDSRef != 0 )
		dsCloseDirService( mDSRef );
	if ( mInternalNodeRef != 0 )
		gRefTable.RemoveReference( mInternalNodeRef, eRefTypeDirNode, 0, 0 );
	if ( mInternalDirRef != 0 )
		gRefTable.RemoveReference( mInternalDirRef, eRefTypeDir, 0, 0 );
}

void CDSLocalPlugin::CloseDatabases( void )
{
	mOpenNodeRefsLock.WaitLock();
	
	CFIndex				numNodeRefs	= CFDictionaryGetCount( mOpenNodeRefs );
		
	// no need to do anything if we don't have any refs
	if (numNodeRefs > 0)
	{
		// check to see if there's already a node object for this node
		CFTypeRef	*values	= (CFTypeRef *) calloc( numNodeRefs, sizeof( CFTypeRef ) );
		if ( values != NULL )
		{
			CFDictionaryGetKeysAndValues( mOpenNodeRefs, NULL, values );
			
			for ( CFIndex i = 0; i < numNodeRefs; i++ )
			{
				CFMutableDictionaryRef	existingNodeDict = (CFMutableDictionaryRef) values[i];

				// now close the database
				NodeObjectFromNodeDict( existingNodeDict )->CloseDatabase();
			}
		}
	}
	
	mOpenNodeRefsLock.SignalLock();
}

SInt32 CDSLocalPlugin::Validate( const char *inVersionStr, const UInt32 inSignature )
{
	mSignature = inSignature;
	
	DbgLog( kLogDebug, "in CDSLocalPlugin::Validate()" );
	
	return eDSNoErr;
}

SInt32 CDSLocalPlugin::Initialize( void )
{
	tDirStatus siResult = eDSNoErr;
	tDataList* nodeName = NULL;

	DbgLog( kLogDebug, "in CDSLocalPlugin::Initialize()" );

	CreateLocalDefaultNodeDirectory();
	
	try
	{
		nodeName = dsBuildListFromStringsPriv( gProtocolPrefixString , kDefaultNodeName, NULL );
		if ( nodeName == NULL )
			throw( eDSAllocationFailed );
			
		//setup the mappings and settings before registering the nodes - very important for startup of localonly mode
		this->LoadMappings();
		this->LoadSettings();
		
		if (gDSLocalOnlyMode)
		{
			tDataList* tnodeName = dsBuildListFromStringsPriv( gProtocolPrefixString , kTargetNodeName, NULL );
			if( tnodeName == NULL ) throw( eDSAllocationFailed );
			CServerPlugin::_RegisterNode( mSignature, tnodeName, kDirNodeType );
			dsDataListDeallocatePriv( tnodeName);
			free(tnodeName);
			tnodeName = NULL;
		}
		
		// we only register the default node if we aren't in local only mode and normal daemon isn't running
		if ( gDSLocalOnlyMode == false || dsIsDirServiceRunning() != eDSNoErr )
		{
			CServerPlugin::_RegisterNode( mSignature, nodeName, kDirNodeType );
			CServerPlugin::_RegisterNode( mSignature, nodeName, kLocalHostedType );
		}
		
		CServerPlugin::_RegisterNode( mSignature, nodeName, kLocalNodeType );
		dsDataListDeallocatePriv( nodeName);
		free(nodeName);
		nodeName = NULL;
		
		//here we read the directory kDBNodesPath to see if there are more nodes that we need to register
		//we know this kDBNodesPath path exists from above
		CFMutableArrayRef subDirs = FindSubDirsInDirectory( kDBNodesPath );
		if ( subDirs != NULL && gDSLocalOnlyMode == false && dsIsDirServiceRunning() == eDSNoErr )
		{
			for (CFIndex idx = 0; idx < CFArrayGetCount( subDirs); idx++)
			{
				char	   *cStr		= NULL;
				size_t		cStrSize	= 0;
				CFStringRef fileStr = (CFStringRef)CFArrayGetValueAtIndex( subDirs, idx );
				nodeName = dsBuildListFromStringsPriv( gProtocolPrefixString , CStrFromCFString( fileStr, &cStr, &cStrSize, NULL ), NULL );
				DSFreeString(cStr);
				CServerPlugin::_RegisterNode( mSignature, nodeName, kDirNodeType );
				dsDataListDeallocatePriv( nodeName);
				free(nodeName);
				nodeName = NULL;
			}
			DSCFRelease(subDirs);
		}
		
		siResult = (tDirStatus) BaseDirectoryPlugin::Initialize();
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::Initialize(): failed with error %d", err );
		siResult = err;
	}

	if ( nodeName != NULL )
	{
		dsDataListDeallocatePriv( nodeName);
		DSFree( nodeName );
	}
	
	return siResult;
}

SInt32 CDSLocalPlugin::SetPluginState( const UInt32 inState )
{
	BaseDirectoryPlugin::SetPluginState( inState );
	
    if ( inState & kActive )
    {
		//tell everyone we are ready to go
		WakeUpRequests();
    }
	
	return( eDSNoErr );
}

CDSAuthParams*	CDSLocalPlugin::NewAuthParamObject( void )
{
	return new CDSLocalAuthParams();
}


void CDSLocalPlugin::WakeUpRequests ( void )
{
	gKickLocalNodeRequests.PostEvent();
} // WakeUpRequests


void CDSLocalPlugin::WaitForInit ( void )
{
    // we wait for 2 minutes before giving up
    gKickLocalNodeRequests.WaitForEvent( (UInt32)(2 * 60 * kMilliSecsPerSec) );
} // WaitForInit


SInt32 CDSLocalPlugin::PeriodicTask( void )
{
	return eDSNoErr;
}

SInt32 CDSLocalPlugin::ProcessRequest( void *inData )
{
	char	*pathStr	= NULL;
	char	*recTypeStr	= NULL;
	
	if ( inData == NULL )
	{
        DbgLog( kLogPlugin, "CDSLocalPlugin::ProcessRequest, inData is NULL!" );
		return( ePlugInDataError );
	}
	
	if (((sHeader *)inData)->fType == kOpenDirNode)
	{
		if (((sOpenDirNode *)inData)->fInDirNodeName != nil)
		{
			pathStr = ::dsGetPathFromListPriv( ((sOpenDirNode *)inData)->fInDirNodeName, "/" );
			if ( (pathStr != NULL) && (strncmp(pathStr,"/Local",6) != 0) )
			{
				free(pathStr);
				return eDSOpenNodeFailed;
			}
		}
	}
	else if( ((sHeader *)inData)->fType == kKerberosMutex || ((sHeader *)inData)->fType == kServerRunLoop )
	{
		// we don't care yet here
		return eDSNoErr;
	}		

	WaitForInit();

	#if LOG_REQUEST_TIMES
	CFTimeInterval startTime = CFAbsoluteTimeGetCurrent();
	#endif

	tDirStatus siResult = eDSNoErr;
	sHeader* msgHdr	= (sHeader*)inData;
	switch ( msgHdr->fType )
	{
		case kGetRecordEntry:
			siResult = eNotHandledByThisNode;
			break;
		
		case kCloseAttributeList:
			siResult = eDSNoErr;
			break;
						
		case kSetRecordType:
			siResult = eNotHandledByThisNode;
			break;
			
		case kCreateRecord:
		case kCreateRecordAndOpen:
			siResult = this->CreateRecord( (sCreateRecord*)inData );
			break;
		
		case kSetAttributeValues:
			recTypeStr = GetRecordTypeFromRef( ((sSetAttributeValue *)inData)->fInRecRef );
			siResult = this->SetAttributeValues( (sSetAttributeValues*)inData, recTypeStr );
			break;
			
		case kDoAttributeValueSearch:
			siResult = this->DoAttributeValueSearch( (sDoAttrValueSearch*)inData );
			break;

		case kDoAttributeValueSearchWithData:
			siResult = this->DoAttributeValueSearchWithData( (sDoAttrValueSearchWithData*)inData );
			break;
		
		case kDoMultipleAttributeValueSearch:
			siResult = this->DoMultipleAttributeValueSearch( (sDoMultiAttrValueSearch*)inData );
			break;
		
		case kDoMultipleAttributeValueSearchWithData:
			siResult = this->DoMultipleAttributeValueSearchWithData( (sDoMultiAttrValueSearchWithData*)inData );
			break;

		case kHandleNetworkTransition:
			siResult = this->HandleNetworkTransition( (sHeader*)inData );
			break;

		default:
			siResult = (tDirStatus) BaseDirectoryPlugin::ProcessRequest( inData );
			break;
	}
	
#if LOG_REQUEST_TIMES
	CFTimeInterval endTime = CFAbsoluteTimeGetCurrent();
	DbgLog( kLogDebug, "CDSLocalPlugin::HandleRequest, finished operation, duration is %f secs", endTime - startTime );
#endif
	
	msgHdr->fResult = siResult;

	DSFreeString( pathStr );
	DSFreeString( recTypeStr );
	
	return siResult;
}

#pragma mark -
#pragma mark Plugin Support 

void CDSLocalPlugin::AddContinueData( tDirNodeReference inNodeRef, CFMutableDictionaryRef inContinueData, tContextData *inOutAttachReference )
{
	if (inContinueData == NULL || *inOutAttachReference != 0)
		return;
	
	CFRetain( inContinueData );
	(*inOutAttachReference) = gLocalContinueTable->AddPointer( inContinueData, inNodeRef );
}

CFMutableDictionaryRef CDSLocalPlugin::CopyNodeDictForNodeRef( tDirNodeReference inNodeRef )
{
	CFNumberRef nodeRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inNodeRef );

	mOpenNodeRefsLock.WaitLock();
	
	CFMutableDictionaryRef nodeDict = (CFMutableDictionaryRef)CFDictionaryGetValue( mOpenNodeRefs, nodeRefNumber );
	if (nodeDict != NULL)
		CFRetain( nodeDict );
	
	mOpenNodeRefsLock.SignalLock();
	
	DSCFRelease( nodeRefNumber );
	
	return nodeDict;
}

// This method throws
CDSLocalPluginNode* CDSLocalPlugin::NodeObjectFromNodeDict( CFDictionaryRef inNodeDict )
{
	CDSLocalPluginNode* node = NULL;
	{
		CFNumberRef nodeObjectNumber = (CFNumberRef)CFDictionaryGetValue( inNodeDict, CFSTR( kNodeObjectkey ) );
		if ( nodeObjectNumber == NULL )
		{
			DbgLog( kLogPlugin, "CDSLocalPlugin::NodeObjectFromNodeDict(): Couldn't find node number object for with nodeDict=%d.",
				inNodeDict );
			throw( eMemoryError );
		}
		else
		{
			if ( !CFNumberGetValue( nodeObjectNumber, kCFNumberLongType, &node ) )
			if ( node == NULL )
			{
				DbgLog( kLogPlugin, "CDSLocalPlugin::NodeObjectFromNodeDict(): Couldn't find node object." );
				throw( eMemoryError );
			}
		}
	}
	
	return node;
}

// This method throws
CDSLocalPluginNode* CDSLocalPlugin::NodeObjectForNodeRef( tDirNodeReference inNodeRef )
{
	CDSLocalPluginNode*	node = NULL;
	CFDictionaryRef nodeDict = CopyNodeDictForNodeRef( inNodeRef );
	if (nodeDict != NULL )
		node = NodeObjectFromNodeDict( nodeDict );
	
	DSCFRelease( nodeDict );
	
	return node;
}

CFArrayRef CDSLocalPlugin::CreateOpenRecordsOfTypeArray( CFStringRef inNativeRecType )
{
	CFArrayRef recordsArray = NULL;
	CFDictionaryRef openRecordAttrsValues = NULL;
	CFBooleanRef isDeleted = kCFBooleanFalse;
	CFMutableDictionaryRef openRecordAttrsValuesCopy = NULL;

	// don't return the open records if it's a membership call because we can cause a deadlock
	// only rely on existing results on disk
	// TODO: need record level locking, not a big lock on all open records
	if ( Mbrd_IsMembershipThread() == true ) return NULL;
	
	// needs to be a rd/rw lock to protect the CF items
	mOpenRecordRefsLock.WaitLock();
	
	try
	{
		if ( mOpenRecordRefs != 0 )
		{
			CFIndex numOpenRecords = CFDictionaryGetCount( mOpenRecordRefs );
			if ( numOpenRecords > 0 )
			{
				const void** keys = (const void**)calloc( numOpenRecords, sizeof( void* ) );
				const void** values = (const void**)calloc( numOpenRecords, sizeof( void* ) );
				if ( keys == NULL || values == NULL )
					throw( eMemoryError );
				
				CFDictionaryGetKeysAndValues( mOpenRecordRefs, keys, values );
				
				CFMutableArrayRef mutableRecords = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
				for ( CFIndex i = 0; i < numOpenRecords; i++ )
				{
					CFMutableDictionaryRef openRecordDict = (CFMutableDictionaryRef)values[i];
					if ( openRecordDict != NULL )
					{
						CFStringRef nativeRecType = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR(kOpenRecordDictRecordType) );
						if ( nativeRecType != NULL && CFStringCompare(inNativeRecType, nativeRecType, 0) == kCFCompareEqualTo )
						{
							openRecordAttrsValues = (CFDictionaryRef)CFDictionaryGetValue( openRecordDict, CFSTR(kOpenRecordDictAttrsValues) );
							if ( openRecordAttrsValues == NULL ||
								 CFArrayContainsValue(mutableRecords, CFRangeMake(0, CFArrayGetCount(mutableRecords)), openRecordAttrsValues) )
								continue;
							
							isDeleted = (CFBooleanRef) CFDictionaryGetValue( openRecordDict, CFSTR(kOpenRecordDictIsDeleted) );
							if ( isDeleted && CFBooleanGetValue( isDeleted ) == TRUE )
								continue;
							
							CFArrayAppendValue( mutableRecords, openRecordDict );
						}
					}
				}
				recordsArray = mutableRecords;
				DSFree( keys );		//do not release the keys themselves since obtained from Get
				DSFree( values );	//do not release the values themselves since obtained from Get
			}
		}
	}
	catch ( ... )
	{
		
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	return recordsArray;
}

CFStringRef CDSLocalPlugin::AttrNativeTypeForStandardType( CFStringRef inStdType )
{
	CFStringRef nativeType = NULL;
	
	if ( inStdType == NULL )
		return NULL;
	
	if ( CFStringHasPrefix( inStdType, CFSTR( kDSNativeAttrTypePrefix ) ) )
	{
		nativeType = (CFStringRef)CFDictionaryGetValue( mAttrPrefixedNativeToNativeMappings, inStdType );
		if ( nativeType == NULL )
		{
			CFRange prefixRange = CFStringFind( inStdType, CFSTR( kDSNativeAttrTypePrefix ), 0 );
			CFRange suffixRange = CFRangeMake( prefixRange.length,
				CFStringGetLength( inStdType ) - prefixRange.length );
			nativeType = CFStringCreateWithSubstring( NULL, inStdType, suffixRange );
			
			CFDictionaryAddValue( mAttrPrefixedNativeToNativeMappings, inStdType, nativeType );
			CFRelease( nativeType );
		}
	}
	else if ( CFStringHasPrefix( inStdType, CFSTR( kDSStdAttrTypePrefix ) ) )
		nativeType = (CFStringRef)CFDictionaryGetValue( mAttrStdToNativeMappings, inStdType );
	else	// otherwise, we were handed a non-prefixed native type. just return it
		return inStdType;
#if LOG_MAPPINGS
	char* cStr = NULL;
	size_t cStrSize = 0;
	char* cStr2 = NULL;
	size_t cStr2Size = 0;
	DbgLog( kLogPlugin, "CDSLocalPlugin::AttrNativeTypeForStandardType(): %s -> %s",
		CStrFromCFString( inStdType, &cStr, &cStrSize, NULL ),
		nativeType == NULL ? "<NULL>" : CStrFromCFString( nativeType, &cStr2, &cStr2Size, NULL ) );
	DSFree( cStr );
	DSFree( cStr2 );
#endif
	if ( nativeType == NULL )
	{
		CFDebugLog( kLogPlugin, "CDSLocalPlugin::AttrNativeTypeForStandardType(): no native attr type found for standard type %@", inStdType );
		throw( eDSInvalidAttributeType );
	}
	return nativeType;
}

CFStringRef CDSLocalPlugin::AttrStandardTypeForNativeType( CFStringRef inNativeType )
{
	CFStringRef stdType = (CFStringRef)CFDictionaryGetValue( mAttrNativeToStdMappings, inNativeType );
#if LOG_MAPPINGS
	char* cStr = NULL;
	size_t cStrSize = 0;
	char* cStr2 = NULL;
	size_t cStr2Size = 0;
	DbgLog( kLogPlugin, "CDSLocalPlugin::AttrStandardTypeForNativeType(): %s -> %s",
		CStrFromCFString( inNativeType, &cStr, &cStrSize, NULL ),
		stdType == NULL ? "<NULL>" : CStrFromCFString( stdType, &cStr2, &cStr2Size, NULL ) );
	DSFree( cStr );
	DSFree( cStr2 );
#endif
	return stdType;
}

CFStringRef CDSLocalPlugin::AttrPrefixedNativeTypeForNativeType( CFStringRef inNativeType )
{
	if ( inNativeType == NULL )
		return NULL;
	
	mGeneralPurposeLock.WaitLock();
	
	CFStringRef prefixedNativeType = (CFStringRef)CFDictionaryGetValue( mAttrNativeToPrefixedNativeMappings, inNativeType );
	if ( prefixedNativeType == NULL )
	{
		//check here to ensure that we never put the native prefix on a Std META type
		if ( !CFStringHasPrefix( inNativeType, CFSTR( kDSStdAttrTypePrefix ) ) )
		{
			prefixedNativeType = CFStringCreateWithFormat( NULL, NULL, CFSTR( "%s%@"), kDSNativeAttrTypePrefix, inNativeType );
			CFDictionaryAddValue( mAttrNativeToPrefixedNativeMappings, inNativeType, prefixedNativeType );
			CFRelease( prefixedNativeType );
		}
	}
#if LOG_MAPPINGS
	if (prefixedNativeType != NULL)
	{
		char* cStr = NULL;
		size_t cStrSize = 0;
		char* cStr2 = NULL;
		size_t cStr2Size = 0;
		DbgLog( kLogPlugin, "CDSLocalPlugin::AttrPrefixedNativeTypeForNativeType(): %s -> %s",
			CStrFromCFString( inNativeType, &cStr, &cStrSize, NULL ),
			prefixedNativeType == NULL ? "<NULL>" :
			CStrFromCFString( prefixedNativeType, &cStr2, &cStr2Size, NULL ) );
		DSFree( cStr );
		DSFree( cStr2 );
	}
#endif
	
	mGeneralPurposeLock.SignalLock();
	
	return prefixedNativeType;
}

CFStringRef CDSLocalPlugin::RecordNativeTypeForStandardType( CFStringRef inStdType )
{
	CFStringRef nativeType = NULL;
	
	if ( inStdType == NULL )
		return NULL;
	
	bool isNativeType = CFStringHasPrefix( inStdType, CFSTR( kDSNativeRecordTypePrefix ) );
	if ( isNativeType )
	{
		nativeType = (CFStringRef)CFDictionaryGetValue( mRecPrefixedNativeToNativeMappings, inStdType );
		if ( nativeType == NULL )
		{
			CFRange prefixRange = CFStringFind( inStdType, CFSTR( kDSNativeRecordTypePrefix ), 0 );
			CFRange suffixRange = CFRangeMake( prefixRange.length,
				CFStringGetLength( inStdType ) - prefixRange.length );
			nativeType = CFStringCreateWithSubstring( NULL, inStdType, suffixRange );
			
			CFRange slashFound = CFStringFind( nativeType, CFSTR("/"), 0 );
			if (slashFound.location != kCFNotFound )
			{
				CFMutableStringRef mutString = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, nativeType);
				CFRelease(nativeType);
				CFIndex numReplaced = CFStringFindAndReplace( mutString, CFSTR("/"), CFSTR("_"), CFRangeMake(0,CFStringGetLength(mutString)), 0 );
				CFDebugLog(kLogPlugin,"CDSLocalPlugin::RecordNAtiveTypeForStandardType found %d hierarchical slashes to replace with underscores", numReplaced);
				nativeType = CFStringCreateCopy(kCFAllocatorDefault, mutString);
				CFRelease(mutString);
			}
			
			CFDictionaryAddValue( mRecPrefixedNativeToNativeMappings, inStdType, nativeType );
			CFRelease( nativeType );
		}
	}
	else if ( CFStringHasPrefix( inStdType, CFSTR( kDSStdRecordTypePrefix ) ) )
		nativeType = (CFStringRef)CFDictionaryGetValue( mRecStdToNativeMappings, inStdType );
	else	// otherwise, we were handed a non-prefixed native type. just return it
		return inStdType;
#if LOG_MAPPINGS
	char* cStr = NULL;
	size_t cStrSize = 0;
	char* cStr2 = NULL;
	size_t cStr2Size = 0;
	DbgLog( kLogPlugin, "CDSLocalPlugin::RecordNativeTypeForStandardType(): %s -> %s",
		CStrFromCFString( inStdType, &cStr, &cStrSize, NULL ),
		nativeType == NULL ? "<NULL>" : CStrFromCFString( nativeType, &cStr2, &cStr2Size, NULL ) );
	DSFree( cStr );
	DSFree( cStr2 );
#endif
	if ( nativeType == NULL )
	{
		CFDebugLog( kLogPlugin, "CDSLocalPlugin::RecordNativeTypeForStandardType(): no native record type found for standard type %@ isNativeType = %d", inStdType );
		throw( eDSInvalidRecordType );
	}
	return nativeType;
}

CFStringRef CDSLocalPlugin::RecordStandardTypeForNativeType( CFStringRef inNativeType )
{
	CFStringRef stdType = NULL;
	
	if ( inNativeType != NULL )
		stdType = (CFStringRef)CFDictionaryGetValue( mRecNativeToStdMappings, inNativeType );
	
#if LOG_MAPPINGS
	char* cStr = NULL;
	size_t cStrSize = 0;
	char* cStr2 = NULL;
	size_t cStr2Size = 0;
	DbgLog( kLogPlugin, "CDSLocalPlugin::RecordStandardTypeForNativeType(): %s -> %s",
		CStrFromCFString( inNativeType, &cStr, &cStrSize, NULL ),
		stdType == NULL ? "<NULL>" : CStrFromCFString( stdType, &cStr2, &cStr2Size, NULL ) );
	DSFree( cStr );
	DSFree( cStr2 );
#endif
	return stdType;
}


CFStringRef CDSLocalPlugin::GetUserGUID( CFStringRef inUserName, CDSLocalPluginNode* inNode )
{
	CFStringRef guidString = NULL;
	CFTypeRef guidRef = NULL;
	
	CFMutableDictionaryRef mutableAuthedUserRecordDict = NULL;
	tDirStatus dirStatus = this->GetRetainedRecordDict( inUserName,
		this->RecordNativeTypeForStandardType(CFSTR(kDSStdRecordTypeUsers)), inNode, &mutableAuthedUserRecordDict );
	if ( dirStatus != eDSNoErr )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::GetUserGUID(): got error %d from CDSLocalPlugin::GetRetainedRecordDict()", dirStatus );
	}
	else
	{
		CFArrayRef values = (CFArrayRef)CFDictionaryGetValue( mutableAuthedUserRecordDict,
											this->AttrNativeTypeForStandardType(CFSTR(kDS1AttrGeneratedUID)) );
		if ( values != NULL ) {
			guidRef = (CFStringRef)CFArrayGetValueAtIndex( values, 0 );
			if ( CFGetTypeID(guidRef) == CFStringGetTypeID() )
				guidString = (CFStringRef)CFRetain( guidRef );
		}
	}
	
	DSCFRelease( mutableAuthedUserRecordDict );
	
	return guidString;
}


bool
CDSLocalPlugin::SearchAllRecordTypes( CFArrayRef inRecTypesArray )
{
	bool searchAllRecordTypes = false;
	CFStringRef stdRecTypeCFStr = NULL;
	CFIndex numRecTypes = 0;
	
	if ( inRecTypesArray != NULL )
	{
		numRecTypes = CFArrayGetCount( inRecTypesArray );

		for ( CFIndex idx = 0; idx < numRecTypes; idx++ )
		{
			stdRecTypeCFStr = (CFStringRef)CFArrayGetValueAtIndex( inRecTypesArray, idx );
			if ( stdRecTypeCFStr != NULL &&
				 CFStringCompare(stdRecTypeCFStr, CFSTR(kDSStdRecordTypeAll), 0) == kCFCompareEqualTo )
			{
				searchAllRecordTypes = true;
				break;
			}
		}
	}
	
	return searchAllRecordTypes;
}

CFTypeRef CDSLocalPlugin::NodeDictCopyValue( CFDictionaryRef inNodeDict, const void *inKey )
{
	CFTypeRef	returnValue = NULL;
	
	mOpenNodeRefsLock.WaitLock();
	
	returnValue = CFDictionaryGetValue( inNodeDict, inKey );
	if ( returnValue != NULL )
		CFRetain( returnValue );
	
	mOpenNodeRefsLock.SignalLock();
	
	return returnValue;
}

void CDSLocalPlugin::NodeDictSetValue( CFMutableDictionaryRef inNodeDict, const void *inKey, const void *inValue )
{
	mOpenNodeRefsLock.WaitLock();
	CFDictionarySetValue( inNodeDict, inKey, inValue );
	mOpenNodeRefsLock.SignalLock();
}

CFArrayRef CDSLocalPlugin::CopyKerberosServiceList( void )
{
	WaitForInit();
	
	CFStringRef kerbServices = AttrNativeTypeForStandardType( CFSTR(kDSNAttrKerberosServices) );
	if ( kerbServices == NULL ) return NULL;

	CDSLocalPluginNode *node = NULL;
	CFArrayRef result = NULL;
	
	mOpenNodeRefsLock.WaitLock();
	
	// no need to do anything if we don't have any refs
	CFIndex numNodeRefs	= CFDictionaryGetCount( mOpenNodeRefs );
	if ( numNodeRefs > 0 )
	{
		// check to see if there's already a node object for this node
		CFDictionaryRef *values = (CFDictionaryRef *) calloc( numNodeRefs, sizeof(CFDictionaryRef) );
		assert( values != NULL );
		
		CFDictionaryGetKeysAndValues( mOpenNodeRefs, NULL, (const void **) values );
		
		for ( CFIndex ii = 0; ii < numNodeRefs; ii++ )
		{
			CFStringRef nodePath = (CFStringRef) CFDictionaryGetValue( values[ii], CFSTR(kNodePathkey) );
			if ( nodePath != NULL && CFStringCompare(nodePath, CFSTR(kLocalNodeDefault), 0) == kCFCompareEqualTo )
			{
				node = NodeObjectFromNodeDict( values[ii] );
				break;
			}
		}
		
		DSFree( values );
	}
		
	mOpenNodeRefsLock.SignalLock();
	
	if ( node != NULL )
	{
		CFStringRef nativeRecType = RecordNativeTypeForStandardType( CFSTR(kDSStdRecordTypeComputers) );
		CFStringRef recordName = CFSTR("localhost");
		CFArrayRef cfTempArray = CFArrayCreate( kCFAllocatorDefault, (const void **) &recordName, 1, &kCFTypeArrayCallBacks );
		CFMutableArrayRef results = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		
		node->GetRecords( nativeRecType, cfTempArray, CFSTR(kDSNAttrRecordName), eDSiExact, false, 1, results, false );
		if ( CFArrayGetCount(results) > 0 )
		{
			CFDictionaryRef dict = (CFDictionaryRef) CFArrayGetValueAtIndex( results, 0 );
			
			result = (CFArrayRef) CFDictionaryGetValue( dict, kerbServices );
			if ( result != NULL ) CFRetain( result );
		}
		
		DSCFRelease( cfTempArray );
		DSCFRelease( results );
	}
	
	return result;
}

#pragma mark -
#pragma mark Auth Helper Record Attribute handling Functions

tDirNodeReference
CDSLocalPlugin::GetInternalNodeRef( void )
{
	static dispatch_once_t	sPredicate		= 0;
	__block sOpenDirNode	openNodeStruct; // use __block (6453258)
	
	// have to do this when needed, if we do it at initialization, it will cause a problem
	dispatch_once( &sPredicate, 
				   ^(void) {
					   // references are allocated, but the node must have a ref inside of the local plugin
					   // so we fake a node open to so that mInternalNodeRef is a valid ref to the plugin
					   gRefTable.CreateReference( &mInternalDirRef, eRefTypeDir, this, 0, 0, 0 );
					   gRefTable.CreateReference( &mInternalNodeRef, eRefTypeDirNode, this, mInternalDirRef, 0, 0 );
					   
					   openNodeStruct.fType = kOpenDirNode;
					   openNodeStruct.fInDirRef = mInternalDirRef;
					   openNodeStruct.fInDirNodeName = dsBuildFromPathPriv( gDSLocalOnlyMode ? "/Local/Target" : kLocalNodeDefault, "/" );
					   openNodeStruct.fOutNodeRef = mInternalNodeRef;
					   openNodeStruct.fInEffectiveUID = 0;
					   openNodeStruct.fInUID = 0;

					   OpenDirNode( &openNodeStruct );
					   
					   dsDataListDeallocatePriv( openNodeStruct.fInDirNodeName );
					   free( openNodeStruct.fInDirNodeName );
					   openNodeStruct.fInDirNodeName = NULL;
				   } );
	
	return mInternalNodeRef;
}

tDirStatus CDSLocalPlugin::CreateRecord( CFStringRef inStdRecType, CFStringRef inRecName, bool inOpenRecord, tRecordReference* outOpenRecordRef )
{
	char* cStr = NULL;
	size_t cStrSize = 0;
	char* cStr2 = NULL;
	size_t cStrSize2 = 0;
	
	sCreateRecord createRecStruct = {};
	createRecStruct.fType = inOpenRecord ? kCreateRecordAndOpen : kCreateRecord;
	createRecStruct.fInNodeRef = GetInternalNodeRef();
	createRecStruct.fInRecType = ::dsDataNodeAllocateString( 0, CStrFromCFString( inStdRecType, &cStr, &cStrSize, NULL ) );
	createRecStruct.fInRecName = ::dsDataNodeAllocateString( 0, CStrFromCFString( inRecName, &cStr2, &cStrSize2, NULL ) );
	createRecStruct.fInOpen = inOpenRecord;
	createRecStruct.fOutRecRef = 0;

	if ( inOpenRecord ) {
		gRefTable.CreateReference( &createRecStruct.fOutRecRef, eRefTypeRecord, this, mInternalNodeRef, 0, 0 );
		DbgLog( kLogDebug, "CDSLocalPlugin::CreateRecord - internal open using reference %d", createRecStruct.fOutRecRef );
	}
	
	tDirStatus dirStatus = this->CreateRecord( &createRecStruct );
	if ( ( dirStatus == eDSNoErr ) && inOpenRecord && ( outOpenRecordRef != NULL ) )
		*outOpenRecordRef = createRecStruct.fOutRecRef;
	else if ( createRecStruct.fOutRecRef != 0 )
		gRefTable.RemoveReference( createRecStruct.fOutRecRef, eRefTypeRecord, 0, 0 );
	
	::dsDataNodeDeAllocate( 0, createRecStruct.fInRecType );
	::dsDataNodeDeAllocate( 0, createRecStruct.fInRecName );
	
	DSFreeString( cStr );
	DSFreeString( cStr2 );
	
	return dirStatus;
}

tDirStatus CDSLocalPlugin::OpenRecord( CFStringRef inStdRecType, CFStringRef inRecName, tRecordReference* outOpenRecordRef )
{
	char* cStr = NULL;
	size_t cStrSize = 0;
	char* cStr2 = NULL;
	size_t cStrSize2 = 0;

	if ( inStdRecType == NULL )
		return eDSInvalidRecordType;
	
	sOpenRecord openRecStruct = {};
	openRecStruct.fType = kOpenRecord;
	openRecStruct.fInNodeRef = GetInternalNodeRef();
	openRecStruct.fInRecType = ::dsDataNodeAllocateString( 0, CStrFromCFString( inStdRecType, &cStr, &cStrSize, NULL ) );
	openRecStruct.fInRecName = ::dsDataNodeAllocateString( 0, CStrFromCFString( inRecName, &cStr2, &cStrSize2, NULL ) );

	gRefTable.CreateReference( &openRecStruct.fOutRecRef, eRefTypeRecord, this, mInternalNodeRef, 0, 0 );
	DbgLog( kLogDebug, "CDSLocalPlugin::OpenRecord - internal open using reference %d", openRecStruct.fOutRecRef );

	tDirStatus dirStatus = this->OpenRecord( &openRecStruct );
	if ( ( dirStatus == eDSNoErr ) && ( outOpenRecordRef != NULL ) )
		*outOpenRecordRef = openRecStruct.fOutRecRef;
	else
		gRefTable.RemoveReference( openRecStruct.fOutRecRef, eRefTypeRecord, 0, 0 );

	::dsDataNodeDeAllocate( 0, openRecStruct.fInRecType );
	::dsDataNodeDeAllocate( 0, openRecStruct.fInRecName );
	
	DSFreeString( cStr );
	DSFreeString( cStr2 );
	
	return dirStatus;
}

tDirStatus CDSLocalPlugin::CloseRecord( tRecordReference inRecordRef )
{
	sCloseRecord closeRecStruct = {};
	closeRecStruct.fType = kCloseRecord;
	closeRecStruct.fInRecRef = inRecordRef;
	
	tDirStatus dirStatus = this->CloseRecord( &closeRecStruct );
	
	return dirStatus;
}

tDirStatus CDSLocalPlugin::AddAttribute( tRecordReference inRecordRef, CFStringRef inAttributeName,
	CFStringRef inAttrValue )
{
	char* cStr = NULL;
	size_t cStrSize = 0;
	char* cStr2 = NULL;
	size_t cStrSize2 = 0;
	
	char *recTypeStr = GetRecordTypeFromRef( inRecordRef );
	
	sAddAttribute addAttrStruct = {};
	addAttrStruct.fType = kAddAttribute;
	addAttrStruct.fInRecRef = inRecordRef;
	addAttrStruct.fInNewAttr = ::dsDataNodeAllocateString( 0, CStrFromCFString( inAttributeName, &cStr, &cStrSize, NULL ) );
	addAttrStruct.fInFirstAttrValue = ::dsDataNodeAllocateString( 0, CStrFromCFString( inAttrValue, &cStr2, &cStrSize2, NULL ) );

	tDirStatus dirStatus = this->AddAttribute( &addAttrStruct, recTypeStr );

	::dsDataNodeDeAllocate( 0, addAttrStruct.fInNewAttr );
	::dsDataNodeDeAllocate( 0, addAttrStruct.fInFirstAttrValue );
	
	DSFreeString( cStr );
	DSFreeString( cStr2 );
	DSFreeString( recTypeStr );
	
	return dirStatus;
}

tDirStatus CDSLocalPlugin::RemoveAttribute( tRecordReference inRecordRef, CFStringRef inAttributeName )
{
	char* cStr = NULL;
	size_t cStrSize = 0;
	
	char *recTypeStr = GetRecordTypeFromRef( inRecordRef );

	sRemoveAttribute removeAttrStruct = {};
	removeAttrStruct.fType = kRemoveAttribute;
	removeAttrStruct.fInRecRef = inRecordRef;
	removeAttrStruct.fInAttribute = ::dsDataNodeAllocateString( 0, CStrFromCFString( inAttributeName, &cStr, &cStrSize, NULL ) );

	tDirStatus dirStatus = this->RemoveAttribute( &removeAttrStruct, recTypeStr );

	::dsDataNodeDeAllocate( 0, removeAttrStruct.fInAttribute );
	
	DSFreeString( cStr );
	DSFreeString( recTypeStr );
	
	return dirStatus;
}

tDirStatus
CDSLocalPlugin::GetRecAttrValueByIndex(
	tRecordReference inRecordRef,
	const char *inAttributeName,
	UInt32 inIndex,
	tAttributeValueEntryPtr *outEntryPtr )
{
	tDataNodePtr attrNameNode = dsDataNodeAllocateString( 0, inAttributeName );
	sGetRecordAttributeValueByIndex getAttrStruct = { kGetRecordAttributeValueByIndex, eDSNoErr, inRecordRef, attrNameNode, inIndex, NULL };

	tDirStatus result = this->GetRecAttrValueByIndex( &getAttrStruct );
	if ( result == eDSNoErr )
		*outEntryPtr = getAttrStruct.fOutEntryPtr;
	
	dsDataNodeDeAllocate( 0, attrNameNode );
	
	return result;
}


tDirStatus
CDSLocalPlugin::AddAttributeValue(
	tRecordReference inRecRef,
	const char *inAttributeType,
	const char *inAttributeValue )
{
	tDataNodePtr attrTypeNode = dsDataNodeAllocateString( 0, inAttributeType );
	tDataNodePtr attrValueNode = dsDataNodeAllocateString( 0, inAttributeValue );
	sAddAttributeValue addAttrStruct = { kAddAttributeValue, 0, inRecRef, attrTypeNode, attrValueNode };
	char *recTypeStr = GetRecordTypeFromRef( inRecRef );

	tDirStatus result = this->AddAttributeValue( &addAttrStruct, recTypeStr );
	
	dsDataNodeDeAllocate( 0, attrTypeNode );
	dsDataNodeDeAllocate( 0, attrValueNode );
	DSFreeString( recTypeStr );
	
	return result;
}

tDirStatus
CDSLocalPlugin::GetRecAttribInfo(
	tRecordReference inRecordRef,
	const char *inAttributeType,
	tAttributeEntryPtr *outAttributeInfo )
{
	tDataNodePtr attrTypeNode = dsDataNodeAllocateString( 0, inAttributeType );
	sGetRecAttribInfo attributeInfoStruct = { kGetRecordAttributeInfo, 0, inRecordRef, attrTypeNode, NULL };

	tDirStatus result = this->GetRecAttribInfo( &attributeInfoStruct );
	if ( result == eDSNoErr )
		*outAttributeInfo = attributeInfoStruct.fOutAttrInfoPtr;
	
	dsDataNodeDeAllocate( 0, attrTypeNode );
	
	return result;
}

									
#pragma mark -
#pragma mark Auth Stuff

tDirStatus CDSLocalPlugin::AuthOpen( tDirNodeReference inNodeRef, const char* inUserName, const char* inPassword,
	bool inUserIsAdmin, bool inIsEffectiveRoot )
{
	CFMutableDictionaryRef nodeDict = NULL;
	
	if ( inUserName == NULL || inPassword == NULL )
		return eDSAuthFailed;
	if ( strlen(inPassword) >= kHashRecoverableLength )
		return eDSAuthPasswordTooLong;
	
	nodeDict = this->CopyNodeDictForNodeRef( inNodeRef );
	if ( nodeDict == NULL )
		return eDSInvalidNodeRef;
	
	CFStringRef authedAsName = (inIsEffectiveRoot ? CFSTR("root") : CFStringCreateWithCString(NULL, inUserName, kCFStringEncodingUTF8));
	if ( authedAsName != NULL ) {
		// remove any old authenticated user name and set new
		NodeDictSetValue( nodeDict, CFSTR( kNodeAuthenticatedUserName ), authedAsName );
		CFRelease( authedAsName );
	}
	
	DSCFRelease( nodeDict );

	return eDSNoErr;
} // AuthOpen

#pragma mark -
#pragma mark Static Methods

void CDSLocalPlugin::ContextDeallocProc( void* inContextData )
{
	CFDictionaryRef contextDict = (CFDictionaryRef)inContextData;
	DSCFRelease( contextDict );
}

const char* CDSLocalPlugin::CreateCStrFromCFString( CFStringRef inCFStr, char **ioCStr )
{
	if (ioCStr == NULL)
		return NULL;

	const char* cStrPtr = CFStringGetCStringPtr( inCFStr, kCFStringEncodingUTF8 );
	if ( cStrPtr != NULL )
		return cStrPtr;
	
	CFIndex maxCStrLen = CFStringGetMaximumSizeForEncoding( CFStringGetLength( inCFStr ), kCFStringEncodingUTF8 ) + 1;
		*ioCStr = (char*)calloc( maxCStrLen, sizeof(char) );

	if ( !CFStringGetCString( inCFStr, *ioCStr, maxCStrLen, kCFStringEncodingUTF8 ) )
	{
		DbgLog( kLogPlugin, "CStrFromCFString(): CFStringGetCString() failed!" );
		return NULL;
	}
	
	return *ioCStr;
}

CFMutableArrayRef CDSLocalPluginCFArrayCreateFromDataList( tDataListPtr inDataList, bool pruneListIfAllTypePresent = false )
{
	bool bPruneList = false;
	
	CAttributeList* attrList = new CAttributeList( inDataList );
	if ( attrList == NULL )
		return NULL;

	if (pruneListIfAllTypePresent && ( attrList->GetCount() > 1 ) )
	{
		bPruneList = true;
	}
	CFMutableArrayRef mutableArray = CFArrayCreateMutable( NULL, attrList->GetCount(), &kCFTypeArrayCallBacks );
	if ( mutableArray == NULL )
	{
		DSDelete(attrList);
		return NULL;
	}
	UInt32 numItems = attrList->GetCount();
	for ( UInt32 i=1; i<=numItems; i++ )
	{
		char* itemName = NULL;
		SInt32 error = eDSNoErr;
		if ( ( error = attrList->GetAttribute( i, &itemName ) ) == eDSNoErr && itemName != NULL )
		{
			CFStringRef itemNameCFStr = CFStringCreateWithCString( NULL, itemName, kCFStringEncodingUTF8 );
			if ( itemNameCFStr != NULL )
			{
				CFArrayAppendValue( mutableArray, itemNameCFStr );
				if ( bPruneList )
				{
					//no check for native all or standard all cases as might be mixed with singles of the other one
					if (	( strcmp( itemName, kDSRecordsAll ) == 0 ) ||
							( strcmp( itemName, kDSStdRecordTypeAll ) == 0 ) ||
							( strcmp( itemName, kDSAttributesAll ) == 0 ) )
					{
						DSCFRelease( mutableArray );
						mutableArray = CFArrayCreateMutable( NULL, 1, &kCFTypeArrayCallBacks );
						CFArrayAppendValue( mutableArray, itemNameCFStr );
						CFRelease( itemNameCFStr );
						break;
					}
				}
				CFRelease( itemNameCFStr );
			}
		}
	}

	DSDelete(attrList);
	
	return mutableArray;
} // CDSLocalPluginCFArrayCreateFromDataList

void CDSLocalPlugin::LogCFDictionary( CFDictionaryRef inDict, CFStringRef inPrefixMessage )
{
#if LOGGING_ON
	CFIndex numKeys = CFDictionaryGetCount( inDict );
	const void** keys = (const void**)malloc( numKeys * sizeof( void* ) );
	const void** values = (const void**)malloc( numKeys * sizeof( void* ) );

	char* cStr = NULL;
	size_t cStrSize = 0;
	bool allocated = false;
	char* cStr2 = NULL;
	size_t cStrSize2 = 0;
	bool allocated2 = false;
	
	DbgLog( kLogPlugin, "%s:", CStrFromCFString( inPrefixMessage, &cStr, &cStrSize, &allocated ) );

	CFDictionaryGetKeysAndValues( inDict, keys, values );
	for ( CFIndex i=0; i<numKeys; i++ )
	{
		CFStringRef keyDesc = CFCopyDescription( keys[i] );
		CFStringRef valueDesc = CFCopyDescription( values[i] );
		DbgLog( kLogPlugin, "CDSLocalPlugin::LogCFDictionary(): %d key: %s, value: %s", i,
			CStrFromCFString( keyDesc, &cStr, &cStrSize, &allocated ),
			CStrFromCFString( valueDesc, &cStr2, &cStrSize2, &allocated2 ) );
		CFRelease( keyDesc );
		CFRelease( valueDesc );
	}
	
	free( keys );
	free( values );
	if ( allocated )
		free( cStr );
	if ( allocated2 )
		free( cStr2 );
#endif
}

tDirStatus CDSLocalPlugin::OpenDirNodeFromPath( CFStringRef inPath, tDirReference inDSRef, tDirNodeReference* outNodeRef )
{
	char* cStr = NULL;
	size_t cStrSize = 0;
	tDirStatus result = eDSNoErr;
	
	tDataList* dataList = ::dsBuildFromPathPriv( CStrFromCFString( inPath, &cStr, &cStrSize ), "/" );
	if ( dataList != NULL )
	{
		result = ::dsOpenDirNode( inDSRef, dataList, outNodeRef );
        ::dsDataListDeallocatePriv( dataList );
		::free( dataList );
	}
	
	DSFreeString( cStr );
	
	return result;
}

#pragma mark -
#pragma mark Task Handlers

tDirStatus CDSLocalPlugin::OpenDirNode( sOpenDirNode* inData )
{
	tDirStatus		siResult			= eDSNoErr;
	CFStringRef		nodeNameCFStr		= NULL;
	CFStringRef		nodePathCFStr		= NULL;
	CFStringRef		nodeFilePathCFStr	= NULL;
	const void	  **values				= NULL;
	bool			openNodeRefsLocked	= false;
	char		   *cStr				= NULL;
	size_t			cStrSize			= 0;
	CFMutableDictionaryRef	newNodeDict	= NULL;
	
	char* fullNodePathStr = dsGetPathFromListPriv( inData->fInDirNodeName, (char *)"/" );
	if ( fullNodePathStr != NULL )
	{
		if ( strcmp(fullNodePathStr, kLocalNodePrefix) == 0 ) {
			DSFree( fullNodePathStr );
			return BaseDirectoryPlugin::OpenDirNode( inData );
		}
	
		if ( strncmp(fullNodePathStr, kLocalNodePrefixRoot, sizeof(kLocalNodePrefixRoot)-1) != 0 )
			DSFree( fullNodePathStr );
		
		// if this isn't the localonly mode, but someone asks for /Local/Target, it could be a redirect
		// so just redirect the open
		if ( gDSLocalOnlyMode == false && strcmp(fullNodePathStr, "/Local/Target") == 0 ) {
			DSFree( fullNodePathStr );
			fullNodePathStr = strdup( kLocalNodeDefault );
		}
	}

	if ( fullNodePathStr == NULL ) {
		DbgLog( kLogPlugin, "CDSLocalPlugin::OpenDirNode: node name is invalid." );
		return eDSOpenNodeFailed;
	}

	nodePathCFStr = CFStringCreateWithCString( NULL, fullNodePathStr, kCFStringEncodingUTF8 );
	nodeNameCFStr = CFStringCreateWithCString( NULL, fullNodePathStr + sizeof(kLocalNodePrefixRoot)-1, kCFStringEncodingUTF8 );

	DSFree( fullNodePathStr );

	CFDebugLog( kLogDebug, "CDSLocalPlugin::OpenDirNode, nodePathCFStr is %@ nodeNameCFStr is \"%@\"", nodePathCFStr, nodeNameCFStr );
	
	try
	{
		const char* nodeFilePathCStr = NULL;
		if ( gDSLocalOnlyMode )
		{
			siResult = eDSOpenNodeFailed;
			if ( CFStringCompare( nodeNameCFStr, CFSTR( kTargetNodeName ), 0 ) == kCFCompareEqualTo )
			{
				if ( gDSLocalFilePath != NULL )
				{
					nodeFilePathCStr = gDSLocalFilePath;
					nodeFilePathCFStr = CFStringCreateWithCString(NULL, nodeFilePathCStr, kCFStringEncodingUTF8);
					DbgLog( kLogPlugin, "CDSLocalPlugin::OpenDirNode: opening target path %s.", nodeFilePathCStr );
					siResult = eDSNoErr;
				}
			}
			else if ( CFStringCompare( nodeNameCFStr, CFSTR( kDefaultNodeName ), 0 ) == kCFCompareEqualTo )
			{
				// if someone asked for default, we'll fail it here if normal DS is registered
				if ( dsIsDirServiceRunning() != eDSNoErr )
				{
					// build the path for the directory in the file system that contains the data files for the node
					nodeFilePathCFStr = CFStringCreateWithFormat( NULL, NULL, CFSTR( "%s%s/%@/" ), kDBPath, kNodesDir, nodeNameCFStr );
					nodeFilePathCStr = CStrFromCFString( nodeFilePathCFStr, &cStr, &cStrSize ); //using cStr for cleanup below
					siResult = eDSNoErr;
				}
				else
				{
					siResult = eDSNormalDSDaemonInUse;
				}
			}
		}
		else
		{
			// build the path for the directory in the file system that contains the data files for the node
			nodeFilePathCFStr = CFStringCreateWithFormat( NULL, NULL, CFSTR( "%s%s/%@/" ), kDBPath, kNodesDir, nodeNameCFStr );
			nodeFilePathCStr = CStrFromCFString( nodeFilePathCFStr, &cStr, &cStrSize ); //using cStr for cleanup below
		}
		
		if (siResult != eDSNoErr) throw(siResult);
		
		struct stat statBuffer={};
		if ( stat( nodeFilePathCStr, &statBuffer ) != 0 )
		{
			DbgLog( kLogPlugin, "CDSLocalPlugin::OpenDirNode, stat returned nonzero error for \"%s\"", nodeFilePathCStr );
			throw( eDSOpenNodeFailed );
		}
		
		if ( ( statBuffer.st_mode & S_IFDIR ) == 0 )
		{
			DbgLog( kLogPlugin, "CDSLocalPlugin::OpenDirNode, \"%s\" either doesn't exist or isn't a directory", nodeFilePathCStr );
			throw( eDSOpenNodeFailed );
		}

		mOpenNodeRefsLock.WaitLock();
		openNodeRefsLocked = true;
		
		newNodeDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		if ( newNodeDict != NULL )
		{	// create a node dictionary containing the node file path and the node DS path
			CFNumberRef	uidNumber			= CFNumberCreate( NULL, kCFNumberIntType, &inData->fInUID );
			CFNumberRef effectiveUIDNumber	= CFNumberCreate( NULL, kCFNumberIntType, &inData->fInEffectiveUID );
			CDSLocalPluginNode* node		= NULL;
			CFIndex numNodeRefs				= CFDictionaryGetCount( mOpenNodeRefs );
			
			// no need to do anything if we don't have any refs
			if (numNodeRefs > 0)
			{
				// check to see if there's already a node object for this node
				values	= (const void**)calloc( numNodeRefs, sizeof( void* ) );
				if ( values == NULL )
					throw( eMemoryError );
				
				CFDictionaryGetKeysAndValues( mOpenNodeRefs, NULL, values );
				
				CFMutableDictionaryRef existingNodeDict = NULL;
				for ( CFIndex i = 0; i < numNodeRefs; i++ )
				{
					existingNodeDict = (CFMutableDictionaryRef)values[i];
					if ( existingNodeDict != NULL )
					{
						CFStringRef existingNodePath = (CFStringRef)CFDictionaryGetValue( existingNodeDict, CFSTR( kNodePathkey ) );
						if ( existingNodePath != NULL && CFStringCompare( existingNodePath, nodePathCFStr, 0 ) == kCFCompareEqualTo )
						{
							node = this->NodeObjectFromNodeDict( existingNodeDict );
							break;
						}
					}
				}
			}
		
			if ( node == NULL )
				node = new CDSLocalPluginNode( nodeFilePathCFStr, this );

			CFNumberRef nodePtrNumber = CFNumberCreate( NULL, kCFNumberLongType, &node );
			CFDictionaryAddValue( newNodeDict, CFSTR( kNodeFilePathkey ), nodeFilePathCFStr );
			CFDictionaryAddValue( newNodeDict, CFSTR( kNodePathkey ), nodePathCFStr );
			CFDictionaryAddValue( newNodeDict, CFSTR( kNodeNamekey ), nodeNameCFStr );

			CFDictionaryAddValue( newNodeDict, CFSTR( kNodeObjectkey ), nodePtrNumber );
			CFDictionaryAddValue( newNodeDict, CFSTR( kNodeUIDKey ), uidNumber );
			CFDictionaryAddValue( newNodeDict, CFSTR( kNodeEffectiveUIDKey ), effectiveUIDNumber );
			DSCFRelease( nodeFilePathCFStr );
			DSCFRelease( nodePathCFStr );
			DSCFRelease( nodeNameCFStr );

			DSCFRelease( nodePtrNumber );

			DSCFRelease( uidNumber );
			DSCFRelease( effectiveUIDNumber );
		}
		
		// add the node dict to our dictionary of open refs with the nodeRef as the key
		CFNumberRef nodeRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fOutNodeRef );
		CFDictionaryAddValue( mOpenNodeRefs, nodeRefNumber, newNodeDict );
		DSCFRelease( nodeRefNumber );
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::OpenDirNode(): failed with error %d", err );
		siResult = err;
	}
	
	if ( openNodeRefsLocked )
	{
		mOpenNodeRefsLock.SignalLock();
	}

	DSCFRelease( newNodeDict );
	DSFree( values );	//do not release the values themselves since obtained from Get
	DSCFRelease( nodeFilePathCFStr );
	DSCFRelease( nodeNameCFStr );
	DSCFRelease( nodePathCFStr );
	DSFreeString( cStr );

	return siResult;
}

tDirStatus CDSLocalPlugin::CloseDirNode( sCloseDirNode* inData )
{
	tDirStatus				siResult					= eDSNoErr;
	CDSLocalPluginNode		*node						= NULL;
	bool					nodeHasMoreThanOneNodeRef	= false;
	
	CFMutableDictionaryRef nodeDict = this->CopyNodeDictForNodeRef( inData->fInNodeRef );
	if ( nodeDict == NULL ) return BaseDirectoryPlugin::CloseDirNode( inData );
	
	mOpenNodeRefsLock.WaitLock();

	// has a valid context, free the continue datas
	gLocalContinueTable->RemovePointersForRefNum( inData->fInNodeRef );
	
	node = this->NodeObjectFromNodeDict( nodeDict );
		
	try
	{
		CFIndex numNodeRefs = CFDictionaryGetCount( mOpenNodeRefs );
		const void** keys	= (const void**)malloc( numNodeRefs * sizeof( void* ) );
		const void** values	= (const void**)malloc( numNodeRefs * sizeof( void* ) );
		CFDictionaryGetKeysAndValues( mOpenNodeRefs, keys, values );
		for ( CFIndex i = 0; i < numNodeRefs; i++ )
		{
			CFMutableDictionaryRef existingNodeDict = (CFMutableDictionaryRef)values[i];
			if ( existingNodeDict == NULL || existingNodeDict == nodeDict )
				continue;
			
			CDSLocalPluginNode* existingNode = this->NodeObjectFromNodeDict( existingNodeDict );
			
			if ( existingNode == node )
			{
				nodeHasMoreThanOneNodeRef = true;	// some other client has this node open. Don't close it
				break;
			}
		}
		
		DSFree( keys );		//do not release the keys themselves since obtained from Get
		DSFree( values );	//do not release the values themselves since obtained from Get
		
		// already locked we can change the dictionary
		if ( !nodeHasMoreThanOneNodeRef )
		{
			this->FreeExternalNodesInDict( nodeDict );
			DSDelete( node );
		}
		
		// need to retain the write lock the whole time since we delete the dict after deleting the node
		CFNumberRef nodeRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInNodeRef );
		CFDictionaryRemoveValue( mOpenNodeRefs, nodeRefNumber );
		DSCFRelease( nodeRefNumber );
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::CloseDirNode(): got error %d", err );
		siResult = err;
	}
	catch ( ... )
	{
		
	}
	
	mOpenNodeRefsLock.SignalLock();
	
	DSCFRelease( nodeDict );

	return siResult;
}

tDirStatus CDSLocalPlugin::GetDirNodeInfo( sGetDirNodeInfo* inData )
{
	tDirStatus			siResult		= eDSNoErr;
	CFArrayRef			desiredAttrs	= NULL;
	CFDictionaryRef		nodeInfoDict	= NULL;
	CFDictionaryRef		nodeDict		= NULL;
	CFMutableArrayRef	cfValues		= NULL;
	SInt32				recCount;

	nodeDict = this->CopyNodeDictForNodeRef( inData->fInNodeRef );
	if ( nodeDict == NULL ) return BaseDirectoryPlugin::GetDirNodeInfo( inData );
	
	if ( inData->fInDirNodeInfoTypeList == NULL ) {
		siResult = eDSEmptyNodeInfoTypeList;
		goto failure;
	}
	
	desiredAttrs = CDSLocalPluginCFArrayCreateFromDataList( inData->fInDirNodeInfoTypeList, true );
	if ( desiredAttrs == NULL ) {
		siResult = eMemoryAllocError;
		goto failure;
	}
	
	// this copy node info uses BDPI format
	nodeInfoDict = CreateNodeInfoDict( desiredAttrs, nodeDict );
	if ( nodeInfoDict == NULL ) {
		siResult = eMemoryAllocError;
		goto failure;
	} 
	
	cfValues = CFArrayCreateMutable( kCFAllocatorDefault, 1, &kCFTypeArrayCallBacks );
	CFArrayAppendValue( cfValues, nodeInfoDict );
	
	recCount = BaseDirectoryPlugin::FillBuffer( cfValues, inData->fOutDataBuff );
	
	// see if we fit anything in the buffer
	if ( recCount != 0 )
	{
		CFDictionaryRef cfAttributes = (CFDictionaryRef) CFDictionaryGetValue( nodeInfoDict, kBDPIAttributeKey );
		inData->fOutAttrInfoCount = CFDictionaryGetCount( cfAttributes );
	}
	else {
		siResult = eDSBufferTooSmall;
	}
	
	DSCFRelease( cfValues );
	
failure:
	DSCFRelease( desiredAttrs );
	DSCFRelease( nodeInfoDict );
	DSCFRelease( nodeDict );
	
	return siResult;
}

tDirStatus CDSLocalPlugin::GetRecordList( sGetRecordList* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFMutableDictionaryRef continueDataDict = NULL;
	CFArrayRef recNamesArray = NULL;
	CFArrayRef recTypesArray = NULL;
	bool allAttributesRequested = false;
	bool addMetanodeToRecords = false;
	CFDictionaryRef nodeDict = NULL;
	tDirStatus getRecordsStatus = eDSNoErr;
	CFStringRef nodePathCFStr = NULL;

	// basic parameter checking
	if ( inData == NULL )						return( eMemoryError );
	if ( inData->fInDataBuff == NULL )			return( eDSEmptyBuffer );
	if (inData->fInDataBuff->fBufferSize == 0 )	return( eDSEmptyBuffer );
	if ( inData->fInRecNameList == NULL )		return( eDSEmptyRecordNameList );
	if ( inData->fInRecTypeList == NULL )		return( eDSEmptyRecordTypeList );
	if ( inData->fInAttribTypeList == NULL )	return( eDSEmptyAttributeTypeList );
	if ( SupportedMatchType(inData->fInPatternMatch) == false )
		return eDSUnSupportedMatchType;
	
	// get the node dict
	nodeDict = this->CopyNodeDictForNodeRef( inData->fInNodeRef );
	if ( nodeDict == NULL ) return BaseDirectoryPlugin::GetRecordList( inData );
	
	if ( inData->fIOContinueData != 0 ) {
		continueDataDict = (CFMutableDictionaryRef) gLocalContinueTable->GetPointer( inData->fIOContinueData );
		if ( continueDataDict == NULL || CFGetTypeID(continueDataDict) != CFDictionaryGetTypeID() )
			return eDSInvalidContinueData;
	}
	
	try
    {
	
// KW looks like performance hit in the architecture here
// the data search goes and gets EVERYTHING the first call in
// so if the client decides to not "continue" after say 100 records but the total is 10,000 records
// then we have already paid the price of extracting all 10.000 records into memory
		if ( continueDataDict == NULL )	// if the continueDataDict is NULL, then we need to do the actual search here
		{
			// create the continue data dict
			continueDataDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			if ( continueDataDict == NULL ) throw( eMemoryAllocError );

			// add the continue data dict to mContinueDatas so it will be properly retained
			// KW this assumes there is no cleanup of the array by any other thread
			// SNS use the class method
			this->AddContinueData( inData->fInNodeRef, continueDataDict, &inData->fIOContinueData );
			CFRelease( continueDataDict ); // release since the continue data now owns it
			
			CDSLocalPluginNode* node = this->NodeObjectFromNodeDict( nodeDict );

			recTypesArray = CDSLocalPluginCFArrayCreateFromDataList( inData->fInRecTypeList, true );
			if ( recTypesArray == NULL ) throw( eMemoryAllocError );
			
			CFIndex numRecTypes = CFArrayGetCount( recTypesArray );
			if ( numRecTypes == 0 ) throw( eDSEmptyRecordTypeList );
				
			// if we're searching all record types, then fill in with all the record types that have directories
			if ( SearchAllRecordTypes(recTypesArray) )
			{
				DSCFRelease( recTypesArray );
				recTypesArray = node->CreateAllRecordTypesArray();
				numRecTypes = CFArrayGetCount( recTypesArray );
			}
			
			recNamesArray = CDSLocalPluginCFArrayCreateFromDataList( inData->fInRecNameList, true );
			if ( recNamesArray == NULL ) throw( eMemoryAllocError );
			
			CFArrayRef desiredAttributesArray = CDSLocalPluginCFArrayCreateFromDataList( inData->fInAttribTypeList, true );
			if ( desiredAttributesArray == NULL ) throw( eMemoryAllocError );

			CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataDesiredAttributesArrayKey ), desiredAttributesArray );
			CFRelease( desiredAttributesArray ); //Note still using this handle below

			CFRange range = CFRangeMake( 0, CFArrayGetCount( desiredAttributesArray ) );

			// check to see if all attributes were requested
			if ( CFArrayContainsValue( desiredAttributesArray, range, CFSTR(kDSAttributesAll) ) )
			{
				allAttributesRequested = true;
			}

			// check to see if we need to add Metanode
			if ( allAttributesRequested 
				|| CFArrayContainsValue( desiredAttributesArray, range, CFSTR(kDSAttributesStandardAll) ) 
				|| CFArrayContainsValue( desiredAttributesArray, range, CFSTR(kDSNAttrMetaNodeLocation) )  )
			{
				nodePathCFStr = (CFStringRef)NodeDictCopyValue( nodeDict, CFSTR( kNodePathkey ) );
				if ( nodePathCFStr != NULL )
				{
					addMetanodeToRecords = true;
				}
			}

			CFMutableArrayRef mutableRecordsArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			if ( mutableRecordsArray == NULL ) throw( eMemoryAllocError );

			bool searchUserLongNames = false;
			for ( CFIndex i=0; i < numRecTypes; i++ )
			{
				CFStringRef stdRecTypeCFStr	= (CFStringRef)CFArrayGetValueAtIndex( recTypesArray, i );
				CFStringRef nativeRecType = this->RecordNativeTypeForStandardType( stdRecTypeCFStr );
				if ( nativeRecType != NULL && CFStringCompare( nativeRecType, CFSTR( "users" ), 0 ) == kCFCompareEqualTo )
				{
					searchUserLongNames = true;
					break;
				}
			}
			
			UInt32 numRecordsToGet = inData->fOutRecEntryCount;
			if ( numRecordsToGet == 0 )	// KW guess we will never be able to handlke a request greater than 0xffffffff anyways
			{
				numRecordsToGet = 0xffffffff;
			}
			for ( CFIndex i=0; i < numRecTypes && numRecordsToGet > 0 ; i++ )
			{
				CFStringRef stdRecTypeCFStr	= (CFStringRef)CFArrayGetValueAtIndex( recTypesArray, i );
				CFStringRef nativeRecType	= this->RecordNativeTypeForStandardType( stdRecTypeCFStr );
				
				if ( nativeRecType != NULL )
				{
					getRecordsStatus = node->GetRecords(	nativeRecType,
															recNamesArray,
															CFSTR( kDSNAttrRecordName ),
															inData->fInPatternMatch,
															inData->fInAttribInfoOnly,
															numRecordsToGet,
															mutableRecordsArray,
															searchUserLongNames );

					// logging
					CFStringRef firstPatternToMatch = NULL;
					if ( ( recNamesArray != NULL ) && ( CFArrayGetCount( recNamesArray ) > 0 ) )
					{
						firstPatternToMatch = (CFStringRef)CFArrayGetValueAtIndex( recNamesArray, 0 );
					}
					else
					{
						firstPatternToMatch = CFSTR( "<NULL>" ); //recNamesArray should always have something in it
					}
					CFDebugLog( kLogDebug, "CDSLocalPlugin::GetRecordList(): recType: %@, pattern: %@, got %d records", nativeRecType, firstPatternToMatch, CFArrayGetCount( mutableRecordsArray ) );

					if ( numRecordsToGet != 0xffffffff )
					{
						CFIndex numRecordsGot = CFArrayGetCount( mutableRecordsArray );
						if ( (UInt32)numRecordsGot > numRecordsToGet )
						{
							numRecordsToGet = 0;
						}
						else
						{
							numRecordsToGet -= numRecordsGot;
						}
					}

// KW this could have been done inside GetRecords or does it matter?
					this->AddRecordTypeToRecords( stdRecTypeCFStr, mutableRecordsArray );
					if ( addMetanodeToRecords ) {
						siResult = this->AddMetanodeToRecords( mutableRecordsArray, nodePathCFStr );
						if ( siResult != eDSNoErr )
							throw( siResult );
					}
				}
				else
				{
					CFDebugLog( kLogPlugin, "CDSLocalPlugin::GetRecordList, we don't have a mapping for type: %@, skipping", stdRecTypeCFStr );
				}
			}

			CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataRecordsArrayKey ), mutableRecordsArray );
			CFRelease( mutableRecordsArray );
		}
		
		//return the next chunk of records
		// KW since the first call went ahead and already extracted ALL the results
		{
			CFNumberRef numRecordsReturnedNumber = (CFNumberRef)CFDictionaryGetValue( continueDataDict, CFSTR( kContinueDataNumRecordsReturnedKey ) );
			UInt32 numRecordsReturned = 0;
			if ( numRecordsReturnedNumber != NULL )
			{
				CFNumberGetValue( numRecordsReturnedNumber, kCFNumberIntType, &numRecordsReturned );
			}
			
			// get the full records array out of continueDataDict
			CFArrayRef recordsArray = (CFArrayRef)CFDictionaryGetValue( continueDataDict, CFSTR( kContinueDataRecordsArrayKey ) );
			if ( recordsArray == NULL ) throw ( eMemoryError );

			UInt32 numRecordsPacked = 0;
			inData->fOutRecEntryCount = numRecordsPacked;	// set this now in case we didn't find any records at all
			if ( numRecordsReturned < (UInt32)CFArrayGetCount( recordsArray ) )
			{
				CFArrayRef desiredAttributesArray = (CFArrayRef)CFDictionaryGetValue( continueDataDict, CFSTR( kContinueDataDesiredAttributesArrayKey ) );
				if ( desiredAttributesArray == NULL )
				{
					DbgLog( kLogPlugin, "CDSLocalPlugin::GetRecordList():couldn't get desiredAttributesArray from continueData" );
					throw( ePlugInError );
				}
			
				// pack the records into the buffer
				siResult = PackRecordsIntoBuffer( nodeDict, numRecordsReturned, recordsArray, desiredAttributesArray, inData->fInDataBuff, inData->fInAttribInfoOnly, &numRecordsPacked );
				inData->fOutRecEntryCount = numRecordsPacked;
				if ( siResult != eDSNoErr ) throw( siResult );
				
				// update numRecordsReturnedNumber and stick it back into the continueDataDict
				numRecordsReturned += numRecordsPacked;
				CFNumberRef newNumRecordsReturnedNumber = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &numRecordsReturned );
				CFDictionarySetValue( continueDataDict, CFSTR( kContinueDataNumRecordsReturnedKey ), newNumRecordsReturnedNumber );
				CFRelease( newNumRecordsReturnedNumber );
			}
			
			if ( ( getRecordsStatus != eDSBufferTooSmall ) && ( numRecordsReturned >= (UInt32)CFArrayGetCount( recordsArray ) ) )
			{	//remove the continueDataDict.  we're done.
				gLocalContinueTable->RemoveContext( inData->fIOContinueData );
				inData->fIOContinueData	= 0;
			}
		}
	}
	catch( tDirStatus err )
    {
        DbgLog( kLogPlugin, "CDSLocalPlugin::GetRecordList(): failed with error:%d", err );

        siResult = err;

		if ( ( err != eDSBufferTooSmall ) && ( continueDataDict != NULL ) )
		{
			gLocalContinueTable->RemoveContext( inData->fIOContinueData );
			inData->fIOContinueData	= 0;
		}
    }

	DSCFRelease( nodePathCFStr );
	DSCFRelease( recNamesArray );
	DSCFRelease( recTypesArray );
	DSCFRelease( nodeDict );
	
	return siResult;
}


tDirStatus CDSLocalPlugin::DoAttributeValueSearch( sDoAttrValueSearch* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFMutableDictionaryRef continueDataDict = NULL;
	CFArrayRef recTypesArray = NULL;
	CFStringRef attributeType = NULL;
	CFArrayRef patternsToMatchArray = NULL;
	CFDictionaryRef nodeDict = NULL;
	CFStringRef nodePathCFStr = NULL;
	#if LOGGING_ON
	char* cStr = NULL;
	size_t cStrSize = 0;
	#endif
	
	tDirStatus getRecordsStatus = eDSNoErr;

	//basic parameter checking
	if ( inData == NULL )
		return( eMemoryError );
	if ( inData->fOutDataBuff == NULL )
		return( eDSEmptyBuffer );
	if (inData->fOutDataBuff->fBufferSize == 0 )
		return( eDSEmptyBuffer );
	if ( inData->fInPatt2Match == NULL )
		return( eDSEmptyPattern2Match );
	if ( inData->fInAttrType == NULL )
		return( eDSEmptyAttributeType );
	if ( inData->fInRecTypeList == NULL )
		return( eDSEmptyRecordTypeList );
	if ( SupportedMatchType(inData->fInPattMatchType) == false )
		return eDSUnSupportedMatchType;
	
	// get the node dict
	nodeDict = this->CopyNodeDictForNodeRef( inData->fInNodeRef );
	if ( nodeDict == NULL ) return BaseDirectoryPlugin::DoAttributeValueSearch( inData );
	
	if ( inData->fIOContinueData != 0 ) {
		continueDataDict = (CFMutableDictionaryRef) gLocalContinueTable->GetPointer( inData->fIOContinueData );
		if ( continueDataDict == NULL || CFGetTypeID(continueDataDict) != CFDictionaryGetTypeID() )
			return eDSInvalidContinueData;
	}
	
	try
    {
		if ( continueDataDict == NULL )	// if the continueDataDict is NULL, then we need to do the actual search here
		{
			// create the continue data dict
			continueDataDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks );
			if ( continueDataDict == NULL )
				throw( eMemoryAllocError );

			// add the continue data dict to mContinueDatas so it will be properly retained
			AddContinueData( inData->fInNodeRef, continueDataDict, &inData->fIOContinueData );
			CFRelease( continueDataDict );
			
			CDSLocalPluginNode* node = this->NodeObjectFromNodeDict( nodeDict );
			
			CFTypeRef patternToMatch = CFStringCreateWithCString( NULL, inData->fInPatt2Match->fBufferData, kCFStringEncodingUTF8 );
			if ( patternToMatch == NULL )
			{
				// not always a memory error, can fail if the data is not UTF8-able
				patternToMatch = CFDataCreate( NULL, (UInt8 *)inData->fInPatt2Match->fBufferData, inData->fInPatt2Match->fBufferLength );
				if ( patternToMatch == NULL )
					throw( eMemoryAllocError );
			}
			patternsToMatchArray = CFArrayCreate( NULL, (const void**)&patternToMatch, 1, &kCFTypeArrayCallBacks );
			DSCFRelease( patternToMatch );
			if ( patternsToMatchArray == NULL )
				throw( eMemoryAllocError );

			recTypesArray = CDSLocalPluginCFArrayCreateFromDataList( inData->fInRecTypeList, true );
			if ( recTypesArray == NULL )
				throw( eMemoryAllocError );
			
			CFIndex numRecTypes = CFArrayGetCount( recTypesArray );
			if ( numRecTypes == 0 )
				throw( eDSEmptyRecordTypeList );
				
			// if we're searchign all record types, then fill in with all the record types that have directories
			if ( SearchAllRecordTypes(recTypesArray) )
			{
				DSCFRelease( recTypesArray );
				recTypesArray = node->CreateAllRecordTypesArray();
				numRecTypes = CFArrayGetCount( recTypesArray );
			}
				
			attributeType = CFStringCreateWithCString( NULL, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
			if ( attributeType == NULL )
				throw( eMemoryAllocError );
			
			CFStringRef allAttrsString = CFSTR( kDSAttributesAll );
			CFArrayRef desiredAttributesArray = CFArrayCreate( NULL, (const void**)&allAttrsString, 1,
				&kCFTypeArrayCallBacks );
			if ( desiredAttributesArray == NULL )
				throw( eMemoryAllocError );
			CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataDesiredAttributesArrayKey ),
				desiredAttributesArray );
			CFRelease( desiredAttributesArray );

			CFMutableArrayRef mutableRecordsArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			if ( mutableRecordsArray == NULL )
				throw( eMemoryAllocError );

			nodePathCFStr = (CFStringRef)NodeDictCopyValue( nodeDict, CFSTR( kNodePathkey ) );
			
			UInt32 numRecordsToGet = inData->fOutMatchRecordCount;
			if ( numRecordsToGet == 0 )	
				numRecordsToGet = 0xffffffff;
			for ( CFIndex i=0; i<numRecTypes && numRecordsToGet > 0 ; i++ )
			{
				CFStringRef stdRecTypeCFStr = (CFStringRef)CFArrayGetValueAtIndex( recTypesArray, i );
				CFStringRef nativeRecType = this->RecordNativeTypeForStandardType( stdRecTypeCFStr );
				
				if ( nativeRecType != NULL )
				{
					getRecordsStatus = node->GetRecords( nativeRecType, patternsToMatchArray, attributeType,
						inData->fInPattMatchType, false, numRecordsToGet, mutableRecordsArray );

					{	// logging
						CFStringRef firstPatternToMatch = NULL;
						if ( ( patternsToMatchArray != NULL ) && ( CFArrayGetCount( patternsToMatchArray ) > 0 ) )
							firstPatternToMatch = (CFStringRef)CFArrayGetValueAtIndex( patternsToMatchArray, 0 );
						else
							firstPatternToMatch = CFSTR( "<NULL>" );
						CFDebugLog( kLogDebug, "CDSLocalPlugin::DoAttributeValueSearch(): recType: %@, pattern: %@, attr: %@, got %d\
 records",
							nativeRecType, firstPatternToMatch, attributeType, CFArrayGetCount( mutableRecordsArray ) );
					}

					if ( numRecordsToGet != 0xffffffff )
					{
						CFIndex numRecordsGot = CFArrayGetCount( mutableRecordsArray );
						if ( (UInt32)numRecordsGot > numRecordsToGet )
							numRecordsToGet = 0;
						else
							numRecordsToGet -= numRecordsGot;
					}

					this->AddRecordTypeToRecords( stdRecTypeCFStr, mutableRecordsArray );
					siResult = this->AddMetanodeToRecords( mutableRecordsArray, nodePathCFStr );
					if ( siResult != eDSNoErr )
						throw( siResult );
				}
				else
				{
	#if LOGGING_ON
					DbgLog( kLogPlugin, "CDSLocalPlugin::DoAttributeValueSearch, we don't have a mapping for type: %s, skipping",
						CStrFromCFString( stdRecTypeCFStr, &cStr, &cStrSize, NULL ) );
	#endif
				}
			}

			CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataRecordsArrayKey ), mutableRecordsArray );
			CFRelease( mutableRecordsArray );
		}

		//return the next chunk of records
		{
			CFNumberRef numRecordsReturnedNumber = (CFNumberRef)CFDictionaryGetValue( continueDataDict,
				CFSTR( kContinueDataNumRecordsReturnedKey ) );
			UInt32 numRecordsReturned = 0;
			if ( numRecordsReturnedNumber != NULL )
				CFNumberGetValue( numRecordsReturnedNumber, kCFNumberIntType, &numRecordsReturned );
			
			// get the full records array out of continueDataDict
			CFArrayRef recordsArray = (CFArrayRef)CFDictionaryGetValue( continueDataDict,
				CFSTR( kContinueDataRecordsArrayKey ) );
			if ( recordsArray == NULL )
				throw ( eMemoryError );

			UInt32 numRecordsPacked = 0;
			inData->fOutMatchRecordCount = numRecordsPacked;	// set this now in case we didn't find any records at all
			if ( numRecordsReturned < (UInt32)CFArrayGetCount( recordsArray ) )
			{
				CFArrayRef desiredAttributesArray = (CFArrayRef)CFDictionaryGetValue( continueDataDict,
					CFSTR( kContinueDataDesiredAttributesArrayKey ) );
				if ( desiredAttributesArray == NULL )
				{
					DbgLog( kLogPlugin, "CDSLocalPlugin::DoAttributeValueSearch():couldn't get desiredAttributesArray from\
					 continueData" );
					throw( ePlugInError );
				}
			
				// pack the records into the buffer
				siResult = PackRecordsIntoBuffer( nodeDict, numRecordsReturned, recordsArray, desiredAttributesArray,
					inData->fOutDataBuff, false, &numRecordsPacked );
				inData->fOutMatchRecordCount = numRecordsPacked;
				if ( siResult != eDSNoErr )
					throw( siResult );
				
				// update numRecordsReturnedNumber and stick it back into the continueDataDict
				numRecordsReturned += numRecordsPacked;
				CFNumberRef newNumRecordsReturnedNumber = CFNumberCreate( NULL ,kCFNumberIntType, &numRecordsReturned );
				if ( numRecordsReturnedNumber == NULL )	// add it if it's not already there
					CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataNumRecordsReturnedKey ),
						newNumRecordsReturnedNumber );
				else									// replace it if it's there
					CFDictionaryReplaceValue( continueDataDict, CFSTR( kContinueDataNumRecordsReturnedKey ),
						newNumRecordsReturnedNumber );
				CFRelease( newNumRecordsReturnedNumber );
			}

			if ( ( getRecordsStatus != eDSBufferTooSmall ) &&
				( numRecordsReturned >= (UInt32)CFArrayGetCount( recordsArray ) ) )
			{	//remove the continueDataDict.  we're done.
				gLocalContinueTable->RemoveContext( inData->fIOContinueData );
				inData->fIOContinueData	= 0;
			}
		}
	}
	catch( tDirStatus err )
    {
        DbgLog( kLogPlugin, "CDSLocalPlugin::DoAttributeValueSearch(): failed with error:%d", err );

        siResult = err;

		if ( ( err != eDSBufferTooSmall ) && ( continueDataDict != NULL ) )
		{
			gLocalContinueTable->RemoveContext( inData->fIOContinueData );
			inData->fIOContinueData	= 0;
		}
    }

	DSCFRelease( nodePathCFStr );
	DSCFRelease( nodeDict );
	DSCFRelease( patternsToMatchArray );
	DSCFRelease( recTypesArray );
	DSCFRelease( attributeType );

#if LOGGING_ON
	DSFree( cStr );
#endif

	return siResult;
}

tDirStatus CDSLocalPlugin::DoAttributeValueSearchWithData( sDoAttrValueSearchWithData* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFMutableDictionaryRef continueDataDict = NULL;
	CFDictionaryRef nodeDict = NULL;
	CFArrayRef recTypesArray = NULL;
	CFStringRef attributeType = NULL;
	CFArrayRef patternsToMatchArray = NULL;
	CFStringRef nodePathCFStr = NULL;
	#if LOGGING_ON
	char* cStr = NULL;
	size_t cStrSize = 0;
	#endif
	bool allAttributesRequested = false;
	bool addMetanodeToRecords = false;
	tDirStatus getRecordsStatus = eDSNoErr;

	//basic parameter checking
	if ( inData == NULL )
		return( eMemoryError );
	if ( inData->fOutDataBuff == NULL )
		return( eDSEmptyBuffer );
	if (inData->fOutDataBuff->fBufferSize == 0 )
		return( eDSEmptyBuffer );
	if ( inData->fInPatt2Match == NULL )
		return( eDSEmptyPattern2Match );
	if ( inData->fInAttrType == NULL )
		return( eDSEmptyAttributeType );
	if ( inData->fInRecTypeList == NULL )
		return( eDSEmptyRecordTypeList );
	if ( SupportedMatchType(inData->fInPattMatchType) == false )
		return eDSUnSupportedMatchType;
	
	// get the node dict
	nodeDict = this->CopyNodeDictForNodeRef( inData->fInNodeRef );
	if ( nodeDict == NULL ) return BaseDirectoryPlugin::DoAttributeValueSearchWithData( inData );
	
	if ( inData->fIOContinueData != 0 ) {
		continueDataDict = (CFMutableDictionaryRef) gLocalContinueTable->GetPointer( inData->fIOContinueData );
		if ( continueDataDict == NULL || CFGetTypeID(continueDataDict) != CFDictionaryGetTypeID() )
			return eDSInvalidContinueData;
	}
		
	try
    {
		if ( continueDataDict == NULL )	// if the continueDataDict is NULL, then we need to do the actual search here
		{
			// create the continue data dict
			continueDataDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks );
			if ( continueDataDict == NULL )
				throw( eMemoryAllocError );

			this->AddContinueData( inData->fInNodeRef, continueDataDict, &inData->fIOContinueData );
			CFRelease( continueDataDict );
			
			CDSLocalPluginNode* node = this->NodeObjectFromNodeDict( nodeDict );

			CFStringRef patternToMatch = CFStringCreateWithCString( NULL, inData->fInPatt2Match->fBufferData,
				kCFStringEncodingUTF8 );
			if ( patternToMatch == NULL )
				throw( eMemoryAllocError );
			patternsToMatchArray = CFArrayCreate( NULL, (const void**)&patternToMatch, 1, &kCFTypeArrayCallBacks );
			CFRelease( patternToMatch );
			patternToMatch = NULL;
			if ( patternsToMatchArray == NULL )
				throw( eMemoryAllocError );

			recTypesArray = CDSLocalPluginCFArrayCreateFromDataList( inData->fInRecTypeList, true );
			if ( recTypesArray == NULL )
				throw( eMemoryAllocError );
			
			CFIndex numRecTypes = CFArrayGetCount( recTypesArray );
			if ( numRecTypes == 0 )
				throw( eDSEmptyRecordTypeList );

			// if we're searchign all record types, then fill in with all the record types that have directories
			if ( SearchAllRecordTypes(recTypesArray) )
			{
				CFRelease( recTypesArray );
				recTypesArray = NULL;
				recTypesArray = node->CreateAllRecordTypesArray();
				numRecTypes = CFArrayGetCount( recTypesArray );
			}
				
			attributeType = CFStringCreateWithCString( NULL, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
			if ( attributeType == NULL )
				throw( eMemoryAllocError );
			
			CFArrayRef desiredAttributesArray = CDSLocalPluginCFArrayCreateFromDataList( inData->fInAttrTypeRequestList, true );
			if ( desiredAttributesArray == NULL )
				throw( eMemoryAllocError );

			CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataDesiredAttributesArrayKey ),
				desiredAttributesArray );
			
			CFRelease( desiredAttributesArray );

			CFRange range = CFRangeMake( 0, CFArrayGetCount( desiredAttributesArray ) );
			
			// check to see if all attributes were requested
			if ( CFArrayContainsValue( desiredAttributesArray, range, CFSTR(kDSAttributesAll) ) )
			{
				allAttributesRequested = true;
			}
			
			// check to see if we need to add Metanode
			if ( allAttributesRequested 
				|| CFArrayContainsValue( desiredAttributesArray, range, CFSTR(kDSAttributesStandardAll) ) 
			|| CFArrayContainsValue( desiredAttributesArray, range, CFSTR(kDSNAttrMetaNodeLocation) )  )
			{
				nodePathCFStr = (CFStringRef)NodeDictCopyValue( nodeDict, CFSTR( kNodePathkey ) );
				if ( nodePathCFStr != NULL )
				{
					addMetanodeToRecords = true;
				}
			}
			
			CFMutableArrayRef mutableRecordsArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			if ( mutableRecordsArray == NULL )
				throw( eMemoryAllocError );

			UInt32 numRecordsToGet = inData->fOutMatchRecordCount;
			if ( numRecordsToGet == 0 )	
				numRecordsToGet = 0xffffffff;
			for ( CFIndex i=0; i<numRecTypes && numRecordsToGet > 0 ; i++ )
			{
				CFStringRef stdRecTypeCFStr = (CFStringRef)CFArrayGetValueAtIndex( recTypesArray, i );
				CFStringRef nativeRecType = this->RecordNativeTypeForStandardType( stdRecTypeCFStr );
				
				if ( nativeRecType != NULL )
				{
					getRecordsStatus = node->GetRecords( nativeRecType, patternsToMatchArray, attributeType,
						inData->fInPattMatchType, inData->fInAttrInfoOnly, numRecordsToGet, mutableRecordsArray );

					{	// logging
						CFStringRef firstPatternToMatch = NULL;
						if ( ( patternsToMatchArray != NULL ) && ( CFArrayGetCount( patternsToMatchArray ) > 0 ) )
							firstPatternToMatch = (CFStringRef)CFArrayGetValueAtIndex( patternsToMatchArray, 0 );
						else
							firstPatternToMatch = CFSTR( "<NULL>" );
						CFDebugLog( kLogDebug,
							"CDSLocalPlugin::DoAttributeValueSearchWithData(): recType: %@, pattern: %@, attr: %@, got %d records",
							nativeRecType, firstPatternToMatch, attributeType, CFArrayGetCount( mutableRecordsArray ) );
					}

					if ( numRecordsToGet != 0xffffffff )
					{
						CFIndex numRecordsGot = CFArrayGetCount( mutableRecordsArray );
						if ( (UInt32)numRecordsGot > numRecordsToGet )
							numRecordsToGet = 0;
						else
							numRecordsToGet -= numRecordsGot;
					}

					this->AddRecordTypeToRecords( stdRecTypeCFStr, mutableRecordsArray );
					if ( addMetanodeToRecords ) {
						siResult = this->AddMetanodeToRecords( mutableRecordsArray, nodePathCFStr );
						if ( siResult != eDSNoErr )
							throw( siResult );
					}
				}
				else
				{
	#if LOGGING_ON
					DbgLog( kLogPlugin, "CDSLocalPlugin::DoAttributeValueSearchWithData, we don't have a mapping for type: %s",
						CStrFromCFString( stdRecTypeCFStr, &cStr, &cStrSize, NULL ) );
	#endif
				}
			}

			CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataRecordsArrayKey ), mutableRecordsArray );
			CFRelease( mutableRecordsArray );
		}

		//return the next chunk of records
		{
			CFNumberRef numRecordsReturnedNumber = (CFNumberRef)CFDictionaryGetValue( continueDataDict,
				CFSTR( kContinueDataNumRecordsReturnedKey ) );
			UInt32 numRecordsReturned = 0;
			if ( numRecordsReturnedNumber != NULL )
				CFNumberGetValue( numRecordsReturnedNumber, kCFNumberIntType, &numRecordsReturned );
			
			// get the full records array out of continueDataDict
			CFArrayRef recordsArray = (CFArrayRef)CFDictionaryGetValue( continueDataDict,
				CFSTR( kContinueDataRecordsArrayKey ) );
			if ( recordsArray == NULL )
				throw ( eMemoryError );

			UInt32 numRecordsPacked = 0;
			inData->fOutMatchRecordCount = numRecordsPacked;	// set this now in case we didn't find any records at all
			if ( numRecordsReturned < (UInt32)CFArrayGetCount( recordsArray ) )
			{
				CFArrayRef desiredAttributesArray = (CFArrayRef)CFDictionaryGetValue( continueDataDict,
					CFSTR( kContinueDataDesiredAttributesArrayKey ) );
				if ( desiredAttributesArray == NULL )
				{
					DbgLog( kLogPlugin, "CDSLocalPlugin::DoAttributeValueSearchWithData():couldn't get desiredAttributesArray from\
					 continueData" );
					throw( ePlugInError );
				}
			
				// pack the records into the buffer
				siResult = PackRecordsIntoBuffer( nodeDict, numRecordsReturned, recordsArray, desiredAttributesArray,
					inData->fOutDataBuff, inData->fInAttrInfoOnly, &numRecordsPacked );
				inData->fOutMatchRecordCount = numRecordsPacked;
				if ( siResult != eDSNoErr )
					throw( siResult );
				
				// update numRecordsReturnedNumber and stick it back into the continueDataDict
				numRecordsReturned += numRecordsPacked;
				CFNumberRef newNumRecordsReturnedNumber = CFNumberCreate( NULL ,kCFNumberIntType, &numRecordsReturned );
				if ( numRecordsReturnedNumber == NULL )	// add it if it's not already there
					CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataNumRecordsReturnedKey ),
						newNumRecordsReturnedNumber );
				else									// replace it if it's there
					CFDictionaryReplaceValue( continueDataDict, CFSTR( kContinueDataNumRecordsReturnedKey ),
						newNumRecordsReturnedNumber );
				DSCFRelease( newNumRecordsReturnedNumber );
			}

			if ( ( getRecordsStatus != eDSBufferTooSmall ) &&
				( numRecordsReturned >= (UInt32)CFArrayGetCount( recordsArray ) ) )
			{	//remove the continueDataDict.  we're done.
				gLocalContinueTable->RemoveContext( inData->fIOContinueData );
				inData->fIOContinueData	= 0;
			}
		}
	}
	catch( tDirStatus err )
    {
        DbgLog( kLogPlugin, "CDSLocalPlugin::DoAttributeValueSearchWithData(): failed with error:%d", err );

        siResult = err;

		if ( ( err != eDSBufferTooSmall ) && ( continueDataDict != NULL ) )
		{
			gLocalContinueTable->RemoveContext( inData->fIOContinueData );
			inData->fIOContinueData	= 0;
		}
    }
	
	DSCFRelease( nodePathCFStr );
	DSCFRelease( nodeDict );
	DSCFRelease( patternsToMatchArray );
	DSCFRelease( recTypesArray );
	DSCFRelease( attributeType );

#if LOGGING_ON
	DSFree( cStr );
#endif

	return siResult;
}

tDirStatus CDSLocalPlugin::DoMultipleAttributeValueSearch( sDoMultiAttrValueSearch* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFMutableDictionaryRef continueDataDict = NULL;
	CFDictionaryRef nodeDict = NULL;
	CFArrayRef recTypesArray = NULL;
	CFStringRef attributeType = NULL;
	CFArrayRef patternsToMatchArray = NULL;
	CFStringRef nodePathCFStr = NULL;
	#if LOGGING_ON
	char* cStr = NULL;
	size_t cStrSize = 0;
	#endif
	
	tDirStatus getRecordsStatus = eDSNoErr;

	//basic parameter checking
	if ( inData == NULL )
		return( eMemoryError );
	if ( inData->fOutDataBuff == NULL )
		return( eDSEmptyBuffer );
	if (inData->fOutDataBuff->fBufferSize == 0 )
		return( eDSEmptyBuffer );
	if ( inData->fInPatterns2MatchList == NULL )
		return( eDSEmptyPattern2Match );
	if ( inData->fInAttrType == NULL )
		return( eDSEmptyAttributeType );
	if ( inData->fInRecTypeList == NULL )
		return( eDSEmptyRecordTypeList );
	if ( SupportedMatchType(inData->fInPattMatchType) == false )
		return eDSUnSupportedMatchType;

	// get the node dict
	nodeDict = this->CopyNodeDictForNodeRef( inData->fInNodeRef );
	if ( nodeDict == NULL ) return BaseDirectoryPlugin::DoAttributeValueSearch( (sDoAttrValueSearch *)inData );
		
	if ( inData->fIOContinueData != 0 ) {
		continueDataDict = (CFMutableDictionaryRef) gLocalContinueTable->GetPointer( inData->fIOContinueData );
		if ( continueDataDict == NULL || CFGetTypeID(continueDataDict) != CFDictionaryGetTypeID() )
			return eDSInvalidContinueData;
	}
		
	try
    {
		if ( continueDataDict == NULL )	// if the continueDataDict is NULL, then we need to do the actual search here
		{
			// create the continue data dict
			continueDataDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks );
			if ( continueDataDict == NULL )
				throw( eMemoryAllocError );

			// add the continue data dict to mContinueDatas so it will be properly retained
			this->AddContinueData( inData->fInNodeRef, continueDataDict, &inData->fIOContinueData );
			CFRelease( continueDataDict );
			
			CDSLocalPluginNode* node = this->NodeObjectFromNodeDict( nodeDict );

			patternsToMatchArray = CDSLocalPluginCFArrayCreateFromDataList( inData->fInPatterns2MatchList );
			if ( patternsToMatchArray == NULL )
				throw( eMemoryAllocError );

			recTypesArray = CDSLocalPluginCFArrayCreateFromDataList( inData->fInRecTypeList, true );
			if ( recTypesArray == NULL )
				throw( eMemoryAllocError );
			
			CFIndex numRecTypes = CFArrayGetCount( recTypesArray );
			if ( numRecTypes == 0 )
				throw( eDSEmptyRecordTypeList );
				
			// if we're searchign all record types, then fill in with all the record types that have directories
			if ( SearchAllRecordTypes(recTypesArray) )
			{
				CFRelease( recTypesArray );
				recTypesArray = NULL;
				recTypesArray = node->CreateAllRecordTypesArray();
				numRecTypes = CFArrayGetCount( recTypesArray );
			}
				
			attributeType = CFStringCreateWithCString( NULL, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
			if ( attributeType == NULL )
				throw( eMemoryAllocError );
			
			CFStringRef allAttrsString = CFSTR( kDSAttributesAll );
			CFArrayRef desiredAttributesArray = CFArrayCreate( NULL, (const void**)&allAttrsString, 1,
				&kCFTypeArrayCallBacks );
			if ( desiredAttributesArray == NULL )
				throw( eMemoryAllocError );
			CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataDesiredAttributesArrayKey ),
				desiredAttributesArray );
			CFRelease( desiredAttributesArray );

			CFMutableArrayRef mutableRecordsArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			if ( mutableRecordsArray == NULL )
				throw( eMemoryAllocError );

			nodePathCFStr = (CFStringRef)NodeDictCopyValue( nodeDict, CFSTR( kNodePathkey ) );
			
			UInt32 numRecordsToGet = inData->fOutMatchRecordCount;
			if ( numRecordsToGet == 0 )	
				numRecordsToGet = 0xffffffff;
			for ( CFIndex i=0; i<numRecTypes && numRecordsToGet > 0 ; i++ )
			{
				CFStringRef stdRecTypeCFStr = (CFStringRef)CFArrayGetValueAtIndex( recTypesArray, i );
				CFStringRef nativeRecType = this->RecordNativeTypeForStandardType( stdRecTypeCFStr );
				
				if ( nativeRecType != NULL )
				{
					getRecordsStatus = node->GetRecords( nativeRecType, patternsToMatchArray, attributeType,
						inData->fInPattMatchType, false, numRecordsToGet, mutableRecordsArray );

					{	// logging
						CFStringRef firstPatternToMatch = NULL;
						if ( ( patternsToMatchArray != NULL ) && ( CFArrayGetCount( patternsToMatchArray ) > 0 ) )
							firstPatternToMatch = (CFStringRef)CFArrayGetValueAtIndex( patternsToMatchArray, 0 );
						else
							firstPatternToMatch = CFSTR( "<NULL>" );
						CFDebugLog( kLogDebug, "CDSLocalPlugin::DoMultipleAttributeValueSearch(): recType: %@, pattern: %@, attr: %@\
, got %d records",
							nativeRecType, firstPatternToMatch, attributeType, CFArrayGetCount( mutableRecordsArray ) );
					}

					if ( numRecordsToGet != 0xffffffff )
					{
						CFIndex numRecordsGot = CFArrayGetCount( mutableRecordsArray );
						if ( (UInt32)numRecordsGot > numRecordsToGet )
							numRecordsToGet = 0;
						else
							numRecordsToGet -= numRecordsGot;
					}

					this->AddRecordTypeToRecords( stdRecTypeCFStr, mutableRecordsArray );
					siResult = this->AddMetanodeToRecords( mutableRecordsArray, nodePathCFStr );
					if ( siResult != eDSNoErr )
						throw( siResult );
				}
				else
				{
	#if LOGGING_ON
					DbgLog( kLogPlugin, "CDSLocalPlugin::DoMultipleAttributeValueSearch, we don't have a mapping for type: %s",
						CStrFromCFString( stdRecTypeCFStr, &cStr, &cStrSize, NULL ) );
	#endif
				}
			}

			CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataRecordsArrayKey ), mutableRecordsArray );
			CFRelease( mutableRecordsArray );
		}

		//return the next chunk of records
		{
			CFNumberRef numRecordsReturnedNumber = (CFNumberRef)CFDictionaryGetValue( continueDataDict,
				CFSTR( kContinueDataNumRecordsReturnedKey ) );
			UInt32 numRecordsReturned = 0;
			if ( numRecordsReturnedNumber != NULL )
				CFNumberGetValue( numRecordsReturnedNumber, kCFNumberIntType, &numRecordsReturned );
			
			// get the full records array out of continueDataDict
			CFArrayRef recordsArray = (CFArrayRef)CFDictionaryGetValue( continueDataDict,
				CFSTR( kContinueDataRecordsArrayKey ) );
			if ( recordsArray == NULL )
				throw ( eMemoryError );

			UInt32 numRecordsPacked = 0;
			inData->fOutMatchRecordCount = numRecordsPacked;	// set this now in case we didn't find any records at all
			if ( numRecordsReturned < (UInt32)CFArrayGetCount( recordsArray ) )
			{
				CFArrayRef desiredAttributesArray = (CFArrayRef)CFDictionaryGetValue( continueDataDict,
					CFSTR( kContinueDataDesiredAttributesArrayKey ) );
				if ( desiredAttributesArray == NULL )
				{
					DbgLog( kLogPlugin, "CDSLocalPlugin::DoMultipleAttributeValueSearch():couldn't get desiredAttributesArray from\
					 continueData" );
					throw( ePlugInError );
				}
			
				// pack the records into the buffer
				siResult = PackRecordsIntoBuffer( nodeDict, numRecordsReturned, recordsArray, desiredAttributesArray,
					inData->fOutDataBuff, false, &numRecordsPacked );
				inData->fOutMatchRecordCount = numRecordsPacked;
				if ( siResult != eDSNoErr )
					throw( siResult );
				
				// update numRecordsReturnedNumber and stick it back into the continueDataDict
				numRecordsReturned += numRecordsPacked;
				CFNumberRef newNumRecordsReturnedNumber = CFNumberCreate( NULL ,kCFNumberIntType, &numRecordsReturned );
				if ( numRecordsReturnedNumber == NULL )	// add it if it's not already there
					CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataNumRecordsReturnedKey ),
						newNumRecordsReturnedNumber );
				else									// replace it if it's there
					CFDictionaryReplaceValue( continueDataDict, CFSTR( kContinueDataNumRecordsReturnedKey ),
						newNumRecordsReturnedNumber );
				CFRelease( newNumRecordsReturnedNumber );
			}

			if ( ( getRecordsStatus != eDSBufferTooSmall ) &&
				( numRecordsReturned >= (UInt32)CFArrayGetCount( recordsArray ) ) )
			{	//remove the continueDataDict.  we're done.
				gLocalContinueTable->RemoveContext( inData->fIOContinueData );
				inData->fIOContinueData	= 0;
			}
		}
	}
	catch( tDirStatus err )
    {
        DbgLog( kLogPlugin, "CDSLocalPlugin::DoMultipleAttributeValueSearch(): failed with error:%d", err );

        siResult = err;

		if ( ( err != eDSBufferTooSmall ) && ( continueDataDict != NULL ) )
		{
			gLocalContinueTable->RemoveContext( inData->fIOContinueData );
			inData->fIOContinueData	= 0;
		}
    }

	DSCFRelease( nodePathCFStr );
	DSCFRelease( nodeDict );
    DSCFRelease( patternsToMatchArray );
    DSCFRelease( recTypesArray );
    DSCFRelease( attributeType );

#if LOGGING_ON
	DSFree( cStr );
#endif

	return siResult;
}

tDirStatus CDSLocalPlugin::DoMultipleAttributeValueSearchWithData( sDoMultiAttrValueSearchWithData* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFMutableDictionaryRef continueDataDict = NULL;
	CFDictionaryRef nodeDict = NULL;
	CFArrayRef recTypesArray = NULL;
	CFStringRef attributeType = NULL;
	CFArrayRef patternsToMatchArray = NULL;
	CFStringRef nodePathCFStr = NULL;
	#if LOGGING_ON
	char* cStr = NULL;
	size_t cStrSize = 0;
	#endif
	bool allAttributesRequested = false;
	bool addMetanodeToRecords = false;
	tDirStatus getRecordsStatus = eDSNoErr;

	//basic parameter checking
	if ( inData == NULL )
		return( eMemoryError );
	if ( inData->fOutDataBuff == NULL )
		return( eDSEmptyBuffer );
	if (inData->fOutDataBuff->fBufferSize == 0 )
		return( eDSEmptyBuffer );
	if ( inData->fInPatterns2MatchList == NULL )
		return( eDSEmptyPattern2Match );
	if ( inData->fInAttrType == NULL )
		return( eDSEmptyAttributeType );
	if ( inData->fInRecTypeList == NULL )
		return( eDSEmptyRecordTypeList );
	if ( SupportedMatchType(inData->fInPattMatchType) == false )
		return eDSUnSupportedMatchType;
	
	// get the node dict
	nodeDict = this->CopyNodeDictForNodeRef( inData->fInNodeRef );
	if ( nodeDict == NULL ) return BaseDirectoryPlugin::DoAttributeValueSearchWithData( (sDoAttrValueSearchWithData *)inData );
		
	if ( inData->fIOContinueData != 0 ) {
		continueDataDict = (CFMutableDictionaryRef) gLocalContinueTable->GetPointer( inData->fIOContinueData );
		if ( continueDataDict == NULL || CFGetTypeID(continueDataDict) != CFDictionaryGetTypeID() )
			return eDSInvalidContinueData;
	}
		
	try
    {
		if ( continueDataDict == NULL )	// if the continueDataDict is NULL, then we need to do the actual search here
		{
			// create the continue data dict
			continueDataDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks );
			if ( continueDataDict == NULL )
				throw( eMemoryAllocError );

			// add the continue data dict to mContinueDatas so it will be properly retained
			this->AddContinueData( inData->fInNodeRef, continueDataDict, &inData->fIOContinueData );
			CFRelease( continueDataDict );
			
			CDSLocalPluginNode* node = this->NodeObjectFromNodeDict( nodeDict );

			patternsToMatchArray = CDSLocalPluginCFArrayCreateFromDataList( inData->fInPatterns2MatchList );
			if ( patternsToMatchArray == NULL )
				throw( eMemoryAllocError );

			recTypesArray = CDSLocalPluginCFArrayCreateFromDataList( inData->fInRecTypeList, true );
			if ( recTypesArray == NULL )
				throw( eMemoryAllocError );
			
			CFIndex numRecTypes = CFArrayGetCount( recTypesArray );
			if ( numRecTypes == 0 )
				throw( eDSEmptyRecordTypeList );
				
			// if we're searchign all record types, then fill in with all the record types that have directories
			if ( SearchAllRecordTypes(recTypesArray) )
			{
				CFRelease( recTypesArray );
				recTypesArray = NULL;
				recTypesArray = node->CreateAllRecordTypesArray();
				numRecTypes = CFArrayGetCount( recTypesArray );
			}
				
			attributeType = CFStringCreateWithCString( NULL, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
			if ( attributeType == NULL )
				throw( eMemoryAllocError );
			
			CFArrayRef desiredAttributesArray = CDSLocalPluginCFArrayCreateFromDataList( inData->fInAttrTypeRequestList, true );
			if ( desiredAttributesArray == NULL )
				throw( eMemoryAllocError );

			CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataDesiredAttributesArrayKey ),
				desiredAttributesArray );
			
			CFRelease( desiredAttributesArray );

			CFRange range = CFRangeMake( 0, CFArrayGetCount( desiredAttributesArray ) );
			
			// check to see if all attributes were requested
			if ( CFArrayContainsValue( desiredAttributesArray, range, CFSTR(kDSAttributesAll) ) )
			{
				allAttributesRequested = true;
			}
			
			// check to see if we need to add Metanode
			if ( allAttributesRequested 
				|| CFArrayContainsValue( desiredAttributesArray, range, CFSTR(kDSAttributesStandardAll) ) 
			|| CFArrayContainsValue( desiredAttributesArray, range, CFSTR(kDSNAttrMetaNodeLocation) )  )
			{
				nodePathCFStr = (CFStringRef)NodeDictCopyValue( nodeDict, CFSTR( kNodePathkey ) );
				if ( nodePathCFStr != NULL )
				{
					addMetanodeToRecords = true;
				}
			}
			
			CFMutableArrayRef mutableRecordsArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			if ( mutableRecordsArray == NULL )
				throw( eMemoryAllocError );

			UInt32 numRecordsToGet = inData->fOutMatchRecordCount;
			if ( numRecordsToGet == 0 )	
				numRecordsToGet = 0xffffffff;
			for ( CFIndex i=0; i<numRecTypes && numRecordsToGet > 0 ; i++ )
			{
				CFStringRef stdRecTypeCFStr = (CFStringRef)CFArrayGetValueAtIndex( recTypesArray, i );
				CFStringRef nativeRecType = this->RecordNativeTypeForStandardType( stdRecTypeCFStr );
				
				if ( nativeRecType != NULL )
				{
					getRecordsStatus = node->GetRecords( nativeRecType, patternsToMatchArray, attributeType,
						inData->fInPattMatchType, inData->fInAttrInfoOnly, numRecordsToGet, mutableRecordsArray );

					{	// logging
						CFStringRef firstPatternToMatch = NULL;
						if ( ( patternsToMatchArray != NULL ) && ( CFArrayGetCount( patternsToMatchArray ) > 0 ) )
							firstPatternToMatch = (CFStringRef)CFArrayGetValueAtIndex( patternsToMatchArray, 0 );
						else
							firstPatternToMatch = CFSTR( "<NULL>" );
						CFDebugLog( kLogDebug, "CDSLocalPlugin::DoMultipleAttributeValueSearchWithData(): recType: %@, pattern: %@,\
 attr: %@, got %d records",
							nativeRecType, firstPatternToMatch, attributeType, CFArrayGetCount( mutableRecordsArray ) );
					}

					if ( numRecordsToGet != 0xffffffff )
					{
						CFIndex numRecordsGot = CFArrayGetCount( mutableRecordsArray );
						if ( (UInt32)numRecordsGot > numRecordsToGet )
							numRecordsToGet = 0;
						else
							numRecordsToGet -= numRecordsGot;
					}

					this->AddRecordTypeToRecords( stdRecTypeCFStr, mutableRecordsArray );
					if ( addMetanodeToRecords ) {
						siResult = this->AddMetanodeToRecords( mutableRecordsArray, nodePathCFStr );
						if ( siResult != eDSNoErr )
							throw( siResult );
					}
				}
				else
				{
	#if LOGGING_ON
					DbgLog( kLogPlugin, "CDSLocalPlugin::DoMultipleAttributeValueSearchWithData, we don't have a mapping for type: %s",
						CStrFromCFString( stdRecTypeCFStr, &cStr, &cStrSize, NULL ) );
	#endif
				}
			}

			CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataRecordsArrayKey ), mutableRecordsArray );
			CFRelease( mutableRecordsArray );
		}

		//return the next chunk of records
		{
			CFNumberRef numRecordsReturnedNumber = (CFNumberRef)CFDictionaryGetValue( continueDataDict,
				CFSTR( kContinueDataNumRecordsReturnedKey ) );
			UInt32 numRecordsReturned = 0;
			if ( numRecordsReturnedNumber != NULL )
				CFNumberGetValue( numRecordsReturnedNumber, kCFNumberIntType, &numRecordsReturned );
			
			// get the full records array out of continueDataDict
			CFArrayRef recordsArray = (CFArrayRef)CFDictionaryGetValue( continueDataDict,
				CFSTR( kContinueDataRecordsArrayKey ) );
			if ( recordsArray == NULL )
				throw ( eMemoryError );

			UInt32 numRecordsPacked = 0;
			inData->fOutMatchRecordCount = numRecordsPacked;	// set this now in case we didn't find any records at all
			if ( numRecordsReturned < (UInt32)CFArrayGetCount( recordsArray ) )
			{
				CFArrayRef desiredAttributesArray = (CFArrayRef)CFDictionaryGetValue( continueDataDict,
					CFSTR( kContinueDataDesiredAttributesArrayKey ) );
				if ( desiredAttributesArray == NULL )
				{
					DbgLog( kLogPlugin, "CDSLocalPlugin::DoMultipleAttributeValueSearchWithData():couldn't get desiredAttributesArray\
					 from continueData" );
					throw( ePlugInError );
				}
			
				// pack the records into the buffer
				siResult = PackRecordsIntoBuffer( nodeDict, numRecordsReturned, recordsArray, desiredAttributesArray,
					inData->fOutDataBuff, inData->fInAttrInfoOnly, &numRecordsPacked );
				inData->fOutMatchRecordCount = numRecordsPacked;
				if ( siResult != eDSNoErr )
					throw( siResult );
				
				// update numRecordsReturnedNumber and stick it back into the continueDataDict
				numRecordsReturned += numRecordsPacked;
				CFNumberRef newNumRecordsReturnedNumber = CFNumberCreate( NULL ,kCFNumberIntType, &numRecordsReturned );
				if ( numRecordsReturnedNumber == NULL )	// add it if it's not already there
					CFDictionaryAddValue( continueDataDict, CFSTR( kContinueDataNumRecordsReturnedKey ),
						newNumRecordsReturnedNumber );
				else									// replace it if it's there
					CFDictionaryReplaceValue( continueDataDict, CFSTR( kContinueDataNumRecordsReturnedKey ),
						newNumRecordsReturnedNumber );
				DSCFRelease( newNumRecordsReturnedNumber );
			}

			if ( ( getRecordsStatus != eDSBufferTooSmall ) &&
				( numRecordsReturned >= (UInt32)CFArrayGetCount( recordsArray ) ) )
			{	//remove the continueDataDict.  we're done.
				gLocalContinueTable->RemoveContext( inData->fIOContinueData );
				inData->fIOContinueData	= 0;
			}
		}
	}
	catch( tDirStatus err )
    {
        DbgLog( kLogPlugin, "CDSLocalPlugin::DoMultipleAttributeValueSearchWithData(): failed with error:%d", err );

        siResult = err;

		if ( ( err != eDSBufferTooSmall ) && ( continueDataDict != NULL ) )
		{
			gLocalContinueTable->RemoveContext( inData->fIOContinueData );
			inData->fIOContinueData	= 0;
		}
    }
	
	DSCFRelease( nodePathCFStr );
	DSCFRelease( nodeDict );
	DSCFRelease( patternsToMatchArray );
	DSCFRelease( recTypesArray );
	DSCFRelease( attributeType );
	
#if LOGGING_ON
	DSFree( cStr );
#endif

	return siResult;
}


tDirStatus CDSLocalPlugin::CloseAttributeValueList( sCloseAttributeValueList* inData )
{
	tDirStatus siResult = eDSInvalidReference;

	CFNumberRef attrValueListRefNum = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInAttributeValueListRef );
	
	mOpenAttrValueListRefsLock.WaitLock();
	if ( CFDictionaryContainsValue(mOpenAttrValueListRefs, attrValueListRefNum) )
	{
		CFDictionaryRemoveValue( mOpenAttrValueListRefs, attrValueListRefNum );
		siResult = eDSNoErr;
	}
	mOpenAttrValueListRefsLock.SignalLock();
	
	if ( siResult != eDSNoErr )
		siResult = BaseDirectoryPlugin::CloseAttributeValueList( inData );
	
	DSCFRelease( attrValueListRefNum );
	
	return siResult;
}

tDirStatus CDSLocalPlugin::OpenRecord( sOpenRecord* inData )
{
	tDirStatus				siResult					= eDSNoErr;
	CFStringRef				stdRecType					= NULL;
	CFStringRef				recName						= NULL;
	CFMutableDictionaryRef	mutableRecordAttrsValues	= NULL;
	CFMutableDictionaryRef	mutableOpenRecordDict		= NULL;
	CFDictionaryRef			nodeDict					= NULL;
	CFStringRef				recordFilePath				= NULL;
	const void				**keys						= NULL;
	CDSLocalPluginNode		*node						= NULL;
	CFNumberRef				recordRefNumber				= NULL;
	CFStringRef				nodePathCFStr				= NULL;
	CFMutableDictionaryRef	mutableDict					= NULL;
	CFDictionaryRef			attrValueDict				= NULL;
	CFArrayRef				recNameArray				= NULL;
	
	nodeDict = this->CopyNodeDictForNodeRef( inData->fInNodeRef );
	if ( nodeDict == NULL )
		return BaseDirectoryPlugin::OpenRecord( inData );
	
	try
	{		
		node = this->NodeObjectFromNodeDict( nodeDict );
		
		recName = CFStringCreateWithCString( NULL, inData->fInRecName->fBufferData, kCFStringEncodingUTF8 );
		if ( recName == NULL )
			throw( eMemoryAllocError );
		
		stdRecType = CFStringCreateWithCString( NULL, inData->fInRecType->fBufferData, kCFStringEncodingUTF8 );
		if ( stdRecType == NULL )
			throw( eMemoryAllocError );

		CFStringRef nativeRecType = this->RecordNativeTypeForStandardType( stdRecType );
		if ( nativeRecType == NULL ) throw eDSInvalidRecordType;

		// lock down the open records so nobody else writes changes while we're doing this
		mOpenRecordRefsLock.WaitLock();

		try
		{
			bool		bFound			= false;
			CFIndex		numOpenRecords	= CFDictionaryGetCount( mOpenRecordRefs );
			
			keys = (const void**)malloc( numOpenRecords * sizeof( void* ) );
			CFDictionaryGetKeysAndValues( mOpenRecordRefs, keys, NULL );
				
			for ( CFIndex i = 0; i < numOpenRecords && bFound == false; i++ )
			{
				recordRefNumber	= (CFNumberRef)keys[i];
				mutableDict		= (CFMutableDictionaryRef)CFDictionaryGetValue( mOpenRecordRefs, recordRefNumber );
				attrValueDict	= (CFDictionaryRef)CFDictionaryGetValue( mutableDict, CFSTR(kOpenRecordDictAttrsValues) );
				recNameArray	= attrValueDict ? (CFArrayRef)CFDictionaryGetValue( attrValueDict, CFSTR("name") ) : NULL;
				
				if ( recNameArray != NULL &&
					 CFArrayContainsValue(recNameArray, CFRangeMake(0, CFArrayGetCount(recNameArray)), recName) )
				{
					// check the deleted flag first, if it is deleted, then we don't use it
					CFBooleanRef isDeleted = (CFBooleanRef) CFDictionaryGetValue( mutableDict, CFSTR(kOpenRecordDictIsDeleted) );
					if ( CFBooleanGetValue(isDeleted) == FALSE )
					{
						recordRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fOutRecRef );
						CFDictionaryAddValue( mOpenRecordRefs, recordRefNumber, mutableDict );
						DSCFRelease( recordRefNumber );
						bFound = true;
					}
				}
			}
			
			DSFree( keys );
			
			if ( bFound == false )
			{
				siResult = node->CreateDictionaryForRecord( nativeRecType, recName, &mutableRecordAttrsValues, &recordFilePath );
				if ( siResult != eDSNoErr )
					throw( siResult );
				
				mutableOpenRecordDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks,
																   &kCFTypeDictionaryValueCallBacks );
				if ( mutableOpenRecordDict == NULL )
					throw( eMemoryAllocError );
				
				CFDictionaryAddValue( mutableOpenRecordDict, CFSTR( kOpenRecordDictAttrsValues ), mutableRecordAttrsValues );
				CFDictionaryAddValue( mutableOpenRecordDict, CFSTR( kOpenRecordDictRecordFile ), recordFilePath );
				CFDictionaryAddValue( mutableOpenRecordDict, CFSTR( kOpenRecordDictRecordType ), nativeRecType );
				CFDictionaryAddValue( mutableOpenRecordDict, CFSTR( kOpenRecordDictNodeDict ), nodeDict );
				CFDictionaryAddValue( mutableOpenRecordDict, CFSTR( kOpenRecordDictIsDeleted ), kCFBooleanFalse );
				CFDictionaryAddValue( mutableOpenRecordDict, CFSTR( kOpenRecordDictRecordWasChanged ), kCFBooleanFalse );
				
				// now add the metanode location to the record when it is opened
				nodePathCFStr = (CFStringRef)NodeDictCopyValue( nodeDict, CFSTR( kNodePathkey ) );
				if ( nodePathCFStr != NULL )
				{
					CFArrayRef nodePathValues = CFArrayCreate( NULL, (const void **) &nodePathCFStr, 1, &kCFTypeArrayCallBacks );
					CFDictionaryAddValue( mutableRecordAttrsValues, CFSTR(kDSNAttrMetaNodeLocation), nodePathValues );
					CFRelease( nodePathValues );
					
					// everything looks good, so go ahead  and  add the open record dictionary to the open refs
					recordRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fOutRecRef );
					CFDictionaryAddValue( mOpenRecordRefs, recordRefNumber, mutableOpenRecordDict );
					DSCFRelease( recordRefNumber );
				}
				else
				{
					DSCFRelease( mutableOpenRecordDict );
					siResult = eDSRecordNotFound;
				}
			}
		}
		catch( tDirStatus err )
		{
			DbgLog( kLogPlugin, "CDSLocalPlugin::OpenRecord(): Got error %d", err );
			siResult = err;
		}
		catch ( ... )
		{
			
		}

		mOpenRecordRefsLock.SignalLock();
		
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::OpenRecord(): Got error %d", err );
		siResult = err;
	}
	
	DSCFRelease( nodePathCFStr );
	DSFree( keys );
	DSCFRelease( stdRecType );
	DSCFRelease( recName );
	DSCFRelease( mutableRecordAttrsValues );
	DSCFRelease( mutableOpenRecordDict );
	DSCFRelease( recordFilePath );
	DSCFRelease( nodeDict );
	
	return siResult;
}

tDirStatus CDSLocalPlugin::CloseRecord( sCloseRecord* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFNumberRef recordRefNumber = NULL;
	CFDictionaryRef nodeDict = NULL;
	
	recordRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
	if ( recordRefNumber == NULL )
		return( eMemoryAllocError );

	mOpenRecordRefsLock.WaitLock();

	try
	{
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::CloseRecord( inData );

		// get the path to the record file
		CFStringRef openRecordFilePath = (CFStringRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictRecordFile ) );
		if ( openRecordFilePath == NULL )
			throw( ePlugInDataError );

		// get the record type
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL )
			throw( ePlugInDataError );
		
		// get the node dictionary
		CDSLocalPluginNode *node = NULL;
		nodeDict = this->CopyNodeDictandNodeObject( openRecordDict, &node );

		// get the attributes and values for the record
		CFMutableDictionaryRef openRecordAttrsValuesDict = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictAttrsValues ) );
		if ( openRecordAttrsValuesDict == NULL )
			throw( ePlugInDataError );

		// write any changes to the record, we need to grab a write lock on this, to ensure it is atomic
		try
		{
			CFBooleanRef wasChanged = (CFBooleanRef) CFDictionaryGetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged) );
			CFBooleanRef isDeleted = (CFBooleanRef) CFDictionaryGetValue( openRecordDict, CFSTR(kOpenRecordDictIsDeleted) );
			if ( CFBooleanGetValue(isDeleted) == FALSE && CFBooleanGetValue(wasChanged) == TRUE )
			{
				// write any changes to the record
				siResult = node->FlushRecord( openRecordFilePath, recordType, openRecordAttrsValuesDict );
				
				// flush the cache at this point
				FlushCaches( RecordStandardTypeForNativeType(recordType) );
				
				// reset to false because multiple people may have an open record
				CFDictionarySetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged), kCFBooleanFalse );
			}
			else {
				DbgLog( kLogInfo, "CDSLocalPlugin::CloseRecord did not flush record to disk no changes or deleted" );
			}
			
			if ( siResult == eDSNoErr )
				CFDictionaryRemoveValue( mOpenRecordRefs, recordRefNumber );
		}
		catch( tDirStatus err )
		{
			siResult = err;
		}
		catch( ... )
		{
			siResult = eUndefinedError;
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::CloseRecord(): Got error %d", err );
		siResult = err;
	}

	mOpenRecordRefsLock.SignalLock();

	DSCFRelease( nodeDict );
	DSCFRelease( recordRefNumber );
	
	return siResult;
}

tDirStatus CDSLocalPlugin::FlushRecord( sFlushRecord* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFDictionaryRef nodeDict = NULL;
	
	mOpenRecordRefsLock.WaitLock();

	try
	{
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::FlushRecord( inData );

		// get the path to the record file
		CFStringRef openRecordFilePath = (CFStringRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictRecordFile ) );
		if ( openRecordFilePath == NULL )
			throw( ePlugInDataError );

		// get the record type
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL )
			throw( ePlugInDataError );

		// get the node dictionary
		CDSLocalPluginNode *node = NULL;
		nodeDict = this->CopyNodeDictandNodeObject( openRecordDict, &node );

		// get the attributes and values for the record
		CFMutableDictionaryRef openRecordAttrsValuesDict = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictAttrsValues ) );
		if ( openRecordAttrsValuesDict == NULL )
			throw( ePlugInDataError );
		
		// write any changes to the record, we need to grab a write lock on this, to ensure it is atomic
		try
		{
			CFBooleanRef wasChanged = (CFBooleanRef) CFDictionaryGetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged) );
			CFBooleanRef isDeleted = (CFBooleanRef) CFDictionaryGetValue( openRecordDict, CFSTR(kOpenRecordDictIsDeleted) );
			if ( CFBooleanGetValue(isDeleted) == FALSE && CFBooleanGetValue(wasChanged) == TRUE )
			{
				// write any changes to the record
				siResult = node->FlushRecord( openRecordFilePath, recordType, openRecordAttrsValuesDict );
				
				// change occured in the local database, flush Users and/or Groups when that happens
				FlushCaches( RecordStandardTypeForNativeType(recordType) );
				
				// reset to false because multiple people may have an open record
				CFDictionarySetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged), kCFBooleanFalse );
			}
			else {
				DbgLog( kLogInfo, "CDSLocalPlugin::CloseRecord did not flush record to disk no changes or deleted" );
			}
		}
		catch( tDirStatus err )
		{
			siResult = err;
		}
		catch( ... )
		{
			siResult = ePlugInDataError;
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::FlushRecord(): Got error %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	DSCFRelease( nodeDict );
	
	return siResult;
}

tDirStatus CDSLocalPlugin::SetRecordName( sSetRecordName* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFNumberRef recRefNumber = NULL;
	tAttributeValueEntry* attrValueEntry = NULL;
	CFStringRef newValue = NULL;
	CFDictionaryRef nodeDict = NULL;
	
	recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
	if ( recRefNumber == NULL )
		return eMemoryError;
	
	mOpenRecordRefsLock.WaitLock();
	
	try
	{
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::SetRecordName( inData );
		
		nodeDict = (CFDictionaryRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictNodeDict ) );
		if ( nodeDict == NULL )
			throw( ePlugInDataError );
		
		CDSLocalPluginNode* node = this->NodeObjectFromNodeDict( nodeDict );

		// get the record type
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL )
			throw( ePlugInDataError );
		
		if ( node->AccessAllowed(nodeDict, recordType, CFSTR(kDSNAttrRecordName), eDSAccessModeDelete) == false ) 
			throw( eDSPermissionError );
		
		CFMutableDictionaryRef mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL )
			throw( ePlugInDataError );
		
		newValue = CFStringCreateWithCString( NULL, inData->fInNewRecName->fBufferData, kCFStringEncodingUTF8 );
		if ( newValue == NULL )
			throw( eMemoryAllocError );
		
		// set the attribute value
		CFStringRef nativeAttrType = this->AttrNativeTypeForStandardType( CFSTR( kDSNAttrRecordName ) );
		siResult = node->ReplaceAttributeValueInRecordByIndex( nativeAttrType, recordType, mutableRecordAttrsValues,
			0, newValue );
		
		if ( siResult == eDSNoErr ) {
			CFDictionarySetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged), kCFBooleanTrue );
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::SetRecordName(): Got error %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	if ( recRefNumber != NULL )
	{
		CFRelease( recRefNumber );
		recRefNumber = NULL;
	}
	if ( attrValueEntry != NULL )
	{
		::free( attrValueEntry );
		attrValueEntry = NULL;
	}
	if ( newValue != NULL )
	{
		CFRelease( newValue );
		newValue = NULL;
	}
		
	return siResult;
}

tDirStatus CDSLocalPlugin::DeleteRecord( sDeleteRecord* inData )
{
	tDirStatus				siResult		= eDSNoErr;
	tAttributeValueEntry	*attrValueEntry	= NULL;
	CFStringRef				newValue		= NULL;

	mOpenRecordRefsLock.WaitLock();

	try
	{
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::DeleteRecord( inData );

		CFStringRef openRecordFilePath = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictRecordFile ) );
		if ( openRecordFilePath == NULL ) throw( ePlugInDataError );
		
		CFDictionaryRef nodeDict = (CFDictionaryRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictNodeDict ) );
		if ( nodeDict == NULL ) throw( ePlugInDataError );
		
		CFMutableDictionaryRef mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL ) throw( ePlugInDataError );
		CFArrayRef recordNames = (CFArrayRef)CFDictionaryGetValue( mutableRecordAttrsValues, this->AttrNativeTypeForStandardType( CFSTR( kDSNAttrRecordName ) ) );
		if ( recordNames == NULL ) throw( ePlugInDataError );
		if ( CFArrayGetCount(recordNames) == 0 ) throw( ePlugInDataError );
		CFStringRef recordName = (CFStringRef)CFArrayGetValueAtIndex( recordNames, 0 );
		if ( recordName == NULL ) throw( ePlugInDataError );
		
		// get the record type
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL ) throw( ePlugInDataError );

		CDSLocalPluginNode* node = NodeObjectFromNodeDict( nodeDict );
		if ( node->AccessAllowed(nodeDict, recordType, CFSTR(kDSRecordsAll), eDSAccessModeDelete) == false ) 
			throw( eDSPermissionError );

		try
		{
			// we need to give up our lock to prevent inter-thread/process deadlock
			CFRetain( openRecordDict ); // retain it so the dictionary get released while we give up the lock
			mOpenRecordRefsLock.SignalLock();
			
			siResult = node->DeleteRecord( openRecordFilePath, recordType, recordName, mutableRecordAttrsValues );
			
			mOpenRecordRefsLock.WaitLock();
			
			if ( siResult == eDSNoErr )
			{
				// we know it's really mutable, we use default behavior to determine who is modifying it
				CFDictionarySetValue( (CFMutableDictionaryRef) openRecordDict, CFSTR(kOpenRecordDictIsDeleted), kCFBooleanTrue );
				
				// need to flush this record type, it was deleted
				FlushCaches( RecordStandardTypeForNativeType(recordType) );
			}
			
			DSCFRelease( openRecordDict );

			CFNumberRef recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
			if ( recRefNumber != NULL )
			{
				CFDictionaryRemoveValue( mOpenRecordRefs, recRefNumber );
				DSCFRelease( recRefNumber );
			}
		}
		catch ( ... )
		{
			
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::DeleteRecord(): Got error %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	DSFree( attrValueEntry );
	DSCFRelease( newValue );
	
	return siResult;
}

tDirStatus CDSLocalPlugin::CreateRecord( sCreateRecord* inData )
{
	tDirStatus				siResult					= eDSNoErr;
	CFStringRef				stdRecType					= NULL;
	CFStringRef				recName						= NULL;
	CFMutableDictionaryRef	mutableRecordAttrsValues	= NULL;
	CFMutableDictionaryRef	mutableOpenRecordDict		= NULL;
	CFStringRef				recordFilePath				= NULL;
	CFDictionaryRef			nodeDict					= NULL;
	bool					openRecord					= (inData->fType == kCreateRecordAndOpen || inData->fInOpen == true);
	
	nodeDict = this->CopyNodeDictForNodeRef( inData->fInNodeRef );
	if ( nodeDict == NULL ) return BaseDirectoryPlugin::CreateRecord( inData );
	
	try
	{
		CDSLocalPluginNode* node = this->NodeObjectFromNodeDict( nodeDict );
		
		recName = CFStringCreateWithCString( NULL, inData->fInRecName->fBufferData, kCFStringEncodingUTF8 );
		if ( recName == NULL ) throw( eMemoryAllocError );
		
		stdRecType = CFStringCreateWithCString( NULL, inData->fInRecType->fBufferData, kCFStringEncodingUTF8 );
		if ( stdRecType == NULL ) throw( eMemoryAllocError );

		CFStringRef recordType = this->RecordNativeTypeForStandardType( stdRecType );
		if ( recordType == NULL ) throw eDSInvalidRecordType;

		if ( !node->AccessAllowed(nodeDict, recordType, CFSTR(kDSRecordsAll), eDSAccessModeWrite) )
			throw( eDSPermissionError );

		siResult = node->CreateDictionaryForNewRecord( recordType, recName, openRecord ? &mutableRecordAttrsValues : NULL, openRecord ? &recordFilePath : NULL );
		if ( siResult != eDSNoErr ) throw( siResult );
		
		if ( openRecord )
		{
			mutableOpenRecordDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			if ( mutableOpenRecordDict == NULL ) throw( eMemoryAllocError );

			CFDictionaryAddValue( mutableOpenRecordDict, CFSTR( kOpenRecordDictAttrsValues ), mutableRecordAttrsValues );
			CFDictionaryAddValue( mutableOpenRecordDict, CFSTR( kOpenRecordDictRecordFile ), recordFilePath );
			CFDictionaryAddValue( mutableOpenRecordDict, CFSTR( kOpenRecordDictNodeDict ), nodeDict );
			CFDictionaryAddValue( mutableOpenRecordDict, CFSTR( kOpenRecordDictRecordType ), recordType );
			CFDictionaryAddValue( mutableOpenRecordDict, CFSTR( kOpenRecordDictIsDeleted ), kCFBooleanFalse );
			CFDictionaryAddValue( mutableOpenRecordDict, CFSTR( kOpenRecordDictRecordWasChanged ), kCFBooleanTrue );
			
			// everything looks good, so go ahead  and  add the open record dictionary to the open refs
			
			CFNumberRef recordRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fOutRecRef );
			mOpenRecordRefsLock.WaitLock();
			CFDictionaryAddValue( mOpenRecordRefs, recordRefNumber, mutableOpenRecordDict );
			mOpenRecordRefsLock.SignalLock();
			DSCFRelease( recordRefNumber );
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::OpenRecord(): Got error %d", err );
		siResult = err;
	}
	
	DSCFRelease( nodeDict );
	DSCFRelease( stdRecType );
	DSCFRelease( recName );
	DSCFRelease( mutableRecordAttrsValues );
	DSCFRelease( mutableOpenRecordDict );
	DSCFRelease( recordFilePath );
	
	return siResult;
}

bool CDSLocalPlugin::CheckForShellAndValidate( CFDictionaryRef nodeDict, CDSLocalPluginNode *node, 
											   CFStringRef nativeAttrType, const char *buffer, UInt32 bufferLen )
{
	bool	bReturn			= false;
	uid_t	effectiveUID	= 99;
	
	CFNumberRef inEffectiveUIDNumber = (CFNumberRef) NodeDictCopyValue( nodeDict, CFSTR(kNodeEffectiveUIDKey) );
	if ( inEffectiveUIDNumber != NULL ) {
		CFNumberGetValue( inEffectiveUIDNumber, kCFNumberIntType, &effectiveUID );
	}
	
	CFStringRef authedUserName;
	if ( effectiveUID != 0 && (authedUserName = (CFStringRef) NodeDictCopyValue(nodeDict, CFSTR(kNodeAuthenticatedUserName))) != NULL )
	{
		char username[256];
		
		if ( CFStringGetCString(authedUserName, username, sizeof(username), kCFStringEncodingUTF8) == true &&
			 dsIsUserMemberOfGroup(username, "admin") == true )
		{
			effectiveUID = 0;
		}
		CFRelease( authedUserName );
	}
	
	if ( effectiveUID != 0 && 
		CFStringCompare(nativeAttrType, AttrNativeTypeForStandardType(CFSTR(kDS1AttrUserShell)), 0) == kCFCompareEqualTo )
	{
		struct stat sb;
		
		if ( bufferLen > 0 && lstat("/etc/shells", &sb) == 0 )
		{
			FILE *shellFile = fopen( "/etc/shells", "r" );
			if ( shellFile != NULL )
			{
				char *shellList = (char *) calloc( sb.st_size + 1, sizeof(char) );
				
				if ( fread(shellList, sb.st_size + 1, sizeof(char), shellFile) == 0 )
				{
					char *shell;
					
					while ((shell = strsep(&shellList, " \n\r")) != NULL)
					{
						if ( shell[0] == '/' )
						{
							if ( strncmp(buffer, shell, bufferLen) == 0 ) {
								bReturn = true;
								break;
							}
						}
					}
				}
				
				fclose( shellFile );
				DSFree( shellFile );
			}
		}
	}
	else
	{
		bReturn = true;
	}
	
	return bReturn;
}

tDirStatus CDSLocalPlugin::AddAttribute( sAddAttribute* inData, const char *inRecTypeStr )
{
	tDirStatus			siResult			= eDSNoErr;
	CFNumberRef			recRefNumber		= NULL;
	CFStringRef			stdAttrType			= NULL;
	CFTypeRef			attrValue			= NULL;
	CFDictionaryRef		nodeDict			= NULL;

	// don't allow user to set the value of kDSNAttrMetaNodeLocation or kDS1AttrMetaAutomountMap, just return eDSNoErr
	if ( inData->fInNewAttr != NULL && (strcmp(inData->fInNewAttr->fBufferData, kDSNAttrMetaNodeLocation) == 0 || strcmp(inData->fInNewAttr->fBufferData, kDS1AttrMetaAutomountMap) == 0) )
		return eDSNoErr;

	recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
	if ( recRefNumber == NULL )
		return eMemoryError;
	
	mOpenRecordRefsLock.WaitLock();
	
	try
	{
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::AddAttribute( inData, inRecTypeStr );

		CDSLocalPluginNode *node = NULL;
		nodeDict = this->CopyNodeDictandNodeObject( openRecordDict, &node );
		if ( nodeDict == NULL ) throw( ePlugInDataError );
		
		// get the record type
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL ) throw( ePlugInDataError );
		
		stdAttrType = CFStringCreateWithCString( NULL, inData->fInNewAttr->fBufferData, kCFStringEncodingUTF8 );
		if ( stdAttrType == NULL ) throw( eMemoryAllocError );
		
		CFMutableDictionaryRef mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL ) throw( ePlugInDataError );
		
		CFStringRef nativeAttrType = this->AttrNativeTypeForStandardType( stdAttrType );
		
		if ( !node->AccessAllowed(nodeDict, recordType, nativeAttrType, eDSAccessModeWriteAttr, mutableRecordAttrsValues) )
			throw( eDSPermissionError );
		
		if ( inData->fInFirstAttrValue != NULL && CheckForShellAndValidate(nodeDict, node, nativeAttrType, inData->fInFirstAttrValue->fBufferData,
																		   inData->fInFirstAttrValue->fBufferLength) == false )
		{
			throw eDSSchemaError;
		}
		
		// we don't care if it exists, we just add the empty list anyway
		if ( inData->fInFirstAttrValue != NULL && inData->fInFirstAttrValue->fBufferData != NULL )
			attrValue = GetAttrValueFromInput( inData->fInFirstAttrValue->fBufferData, inData->fInFirstAttrValue->fBufferLength );

		siResult = node->AddAttributeToRecord( nativeAttrType, recordType, attrValue, mutableRecordAttrsValues );
		if ( siResult == eDSNoErr ) {
			CFDictionarySetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged), kCFBooleanTrue );
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::AddAttribute(): Got error %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	DSCFRelease( nodeDict );
	DSCFRelease( recRefNumber );
	DSCFRelease( stdAttrType );
	DSCFRelease( attrValue );
		
	return siResult;
}


tDirStatus CDSLocalPlugin::AddAttributeValue( sAddAttributeValue* inData, const char *inRecTypeStr )
{
	tDirStatus			siResult			= eDSNoErr;
	CFNumberRef			recRefNumber		= NULL;
	CFStringRef			stdAttrType			= NULL;
	CFTypeRef			attrValue			= NULL;
	CFDictionaryRef		nodeDict			= NULL;
	
	// don't allow user to set the value of kDSNAttrMetaNodeLocation or kDS1AttrMetaAutomountMap, just return eDSNoErr
	if ( inData->fInAttrType != NULL && (strcmp(inData->fInAttrType->fBufferData, kDSNAttrMetaNodeLocation) == 0 || strcmp(inData->fInAttrType->fBufferData, kDS1AttrMetaAutomountMap) == 0) )
		return eDSNoErr;		

	recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
	if ( recRefNumber == NULL )
		return eMemoryError;
	
	mOpenRecordRefsLock.WaitLock();

	try
	{
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::AddAttributeValue( inData, inRecTypeStr );
		
		CDSLocalPluginNode *node = NULL;
		nodeDict = this->CopyNodeDictandNodeObject( openRecordDict, &node );
		if ( nodeDict == NULL ) throw( ePlugInDataError );
		
		CFMutableDictionaryRef mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL ) throw( ePlugInDataError );
		
		stdAttrType = CFStringCreateWithCString( NULL, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
		if ( stdAttrType == NULL ) throw( eMemoryAllocError );
		
		// get the record type
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL ) throw( ePlugInDataError );
		
		CFStringRef nativeAttrType = this->AttrNativeTypeForStandardType( stdAttrType );

		if ( !node->AccessAllowed(nodeDict, recordType, nativeAttrType, eDSAccessModeWriteAttr, mutableRecordAttrsValues) )
			throw( eDSPermissionError );

		if ( inData->fInAttrValue != NULL && CheckForShellAndValidate(nodeDict, node, nativeAttrType, inData->fInAttrValue->fBufferData,
																	  inData->fInAttrValue->fBufferLength) == false )
		{
			throw eDSSchemaError;
		}
		
		// handle binary write here
		attrValue = GetAttrValueFromInput( inData->fInAttrValue->fBufferData, inData->fInAttrValue->fBufferLength );

		siResult = node->AddAttributeValueToRecord( nativeAttrType, recordType, attrValue, mutableRecordAttrsValues );
		if ( siResult == eDSNoErr ) {
			CFDictionarySetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged), kCFBooleanTrue );
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::AddAttributeValue(): Got error %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	DSCFRelease( nodeDict );
	DSCFRelease( recRefNumber );
	DSCFRelease( stdAttrType );
	DSCFRelease( attrValue );
		
	return siResult;
}


tDirStatus CDSLocalPlugin::RemoveAttribute( sRemoveAttribute* inData, const char *inRecTypeStr )
{
	tDirStatus			siResult			= eDSNoErr;
	CFNumberRef			recRefNumber		= NULL;
	CFStringRef			stdAttrType			= NULL;
	CFDictionaryRef		nodeDict			= NULL;

	// don't allow user to set the value of kDSNAttrMetaNodeLocation or kDS1AttrMetaAutomountMap, just return eDSNoErr
	if ( inData->fInAttribute != NULL &&
			(strcmp(inData->fInAttribute->fBufferData, kDSNAttrMetaNodeLocation) == 0 ||
			 strcmp(inData->fInAttribute->fBufferData, kDS1AttrMetaAutomountMap) == 0) )
	{
		return eDSNoErr;
	}
	
	recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
	if ( recRefNumber == NULL )
		return eMemoryError;

	mOpenRecordRefsLock.WaitLock();
	
	try
	{
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::RemoveAttribute( inData, inRecTypeStr );
		
		CDSLocalPluginNode *node = NULL;
		nodeDict = this->CopyNodeDictandNodeObject( openRecordDict, &node );
		if ( nodeDict == NULL ) throw( ePlugInDataError );

		CFMutableDictionaryRef mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL ) throw( ePlugInDataError );
		
		stdAttrType = CFStringCreateWithCString( NULL, inData->fInAttribute->fBufferData, kCFStringEncodingUTF8 );
		if ( stdAttrType == NULL ) throw( eMemoryAllocError );

		// get the record type
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL ) throw( ePlugInDataError );
		
		CFStringRef nativeAttrType = this->AttrNativeTypeForStandardType( stdAttrType );

		if ( !node->AccessAllowed(nodeDict, recordType, nativeAttrType, eDSAccessModeWriteAttr, mutableRecordAttrsValues) )
			throw( eDSPermissionError );

		if ( CheckForShellAndValidate(nodeDict, node, nativeAttrType, "", 0) == false ) {
			throw eDSSchemaError;
		}
		
		siResult = node->RemoveAttributeFromRecord( nativeAttrType, recordType, mutableRecordAttrsValues );
		if ( siResult == eDSNoErr ) {
			CFDictionarySetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged), kCFBooleanTrue );
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::RemoveAttribute(): Got error %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();

	DSCFRelease( nodeDict );
	DSCFRelease( recRefNumber );
	DSCFRelease( stdAttrType );
		
	return siResult;
}

tDirStatus CDSLocalPlugin::RemoveAttributeValue( sRemoveAttributeValue* inData, const char *inRecTypeStr )
{
	tDirStatus					siResult			= eDSNoErr;
	CFNumberRef					recRefNumber		= NULL;
	CFStringRef					stdAttrType			= NULL;
	tAttributeValueEntry	   *attrValueEntry		= NULL;
	CFDictionaryRef				nodeDict			= NULL;
	
	// don't allow user to set the value of kDSNAttrMetaNodeLocation or kDS1AttrMetaAutomountMap, just return eDSNoErr
	if ( inData->fInAttrType != NULL && (strcmp(inData->fInAttrType->fBufferData, kDSNAttrMetaNodeLocation) == 0 || strcmp(inData->fInAttrType->fBufferData, kDS1AttrMetaAutomountMap) == 0) )
		return eDSNoErr;
	
	recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
	if ( recRefNumber == NULL )
		return eMemoryError;
	
	mOpenRecordRefsLock.WaitLock();

	try
	{
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::RemoveAttributeValue( inData, inRecTypeStr );
		
		CDSLocalPluginNode *node = NULL;
		nodeDict = this->CopyNodeDictandNodeObject( openRecordDict, &node );
		if ( nodeDict == NULL ) throw( ePlugInDataError );

		CFMutableDictionaryRef mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL ) throw( ePlugInDataError );
		
		stdAttrType = CFStringCreateWithCString( NULL, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
		if ( stdAttrType == NULL ) throw( eMemoryAllocError );

		// get the record type
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL ) throw( ePlugInDataError );
		
		// get the attribute value
		CFStringRef nativeAttrType = this->AttrNativeTypeForStandardType( stdAttrType );

		if ( !node->AccessAllowed(nodeDict, recordType, nativeAttrType, eDSAccessModeWriteAttr, mutableRecordAttrsValues) )
			throw( eDSPermissionError );

		if ( CheckForShellAndValidate(nodeDict, node, nativeAttrType, "", 0) == false ) {
			throw eDSSchemaError;
		}
				
		siResult = node->RemoveAttributeValueFromRecordByCRC( nativeAttrType, recordType, mutableRecordAttrsValues, inData->fInAttrValueID );
		if ( siResult == eDSNoErr ) {
			CFDictionarySetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged), kCFBooleanTrue );
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::RemoveAttributeValue(): Got error %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();

	DSCFRelease( nodeDict );
	DSCFRelease( recRefNumber );
	DSCFRelease( stdAttrType );
	DSFree( attrValueEntry );
		
	return siResult;
}

tDirStatus CDSLocalPlugin::SetAttributeValue( sSetAttributeValue* inData, const char *inRecTypeStr )
{
	tDirStatus					siResult					= eDSNoErr;
	CFNumberRef					recRefNumber				= NULL;
	CFStringRef					stdAttrType					= NULL;
	tAttributeValueEntry	   *attrValueEntry				= NULL;
	CFTypeRef					newValue					= NULL;
	CFDictionaryRef				nodeDict					= NULL;
	CFMutableDictionaryRef		mutableRecordAttrsValues	= NULL;
	
	// don't allow user to set the value of kDSNAttrMetaNodeLocation or kDS1AttrMetaAutomountMap, just return eDSNoErr
	if ( inData->fInAttrType != NULL && (strcmp(inData->fInAttrType->fBufferData, kDSNAttrMetaNodeLocation) == 0 || strcmp(inData->fInAttrType->fBufferData, kDS1AttrMetaAutomountMap) == 0) )
		return eDSNoErr;
	
	recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
	if ( recRefNumber == NULL )
		return eMemoryError;
	
	mOpenRecordRefsLock.WaitLock();

	try
	{
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::SetAttributeValue( inData, inRecTypeStr );
		
		CDSLocalPluginNode *node = NULL;
		nodeDict = this->CopyNodeDictandNodeObject( openRecordDict, &node );
		if ( nodeDict == NULL ) throw( ePlugInDataError );
		
		mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL ) throw( ePlugInDataError );
		
		stdAttrType = CFStringCreateWithCString( NULL, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
		if ( stdAttrType == NULL ) throw( eMemoryAllocError );
		
		// get the record type
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL ) throw( ePlugInDataError );
		
		// get the attribute value
		CFStringRef nativeAttrType = this->AttrNativeTypeForStandardType( stdAttrType );

		if ( !node->AccessAllowed(nodeDict, recordType, nativeAttrType, eDSAccessModeWriteAttr, mutableRecordAttrsValues) )
			throw( eDSPermissionError );

		if ( inData->fInAttrValueEntry != NULL && 
			 CheckForShellAndValidate(nodeDict, node, nativeAttrType, inData->fInAttrValueEntry->fAttributeValueData.fBufferData,
									  inData->fInAttrValueEntry->fAttributeValueData.fBufferLength) == false )
		{
			throw eDSSchemaError;
		}
				
		// handle binary write here
		newValue = GetAttrValueFromInput( inData->fInAttrValueEntry->fAttributeValueData.fBufferData, inData->fInAttrValueEntry->fAttributeValueData.fBufferLength );

		siResult = node->ReplaceAttributeValueInRecordByCRC( nativeAttrType, recordType, mutableRecordAttrsValues, inData->fInAttrValueEntry->fAttributeValueID, newValue );
		if ( siResult == eDSNoErr ) {
			CFDictionarySetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged), kCFBooleanTrue );
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::SetAttributeValue(): Got error %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	DSCFRelease( nodeDict );
	DSCFRelease( recRefNumber );
	DSCFRelease( stdAttrType );
	DSFree( attrValueEntry );
	DSCFRelease( newValue );
		
	return siResult;
}

tDirStatus CDSLocalPlugin::SetAttributeValues( sSetAttributeValues* inData, const char *inRecTypeStr )
{
	tDirStatus					siResult					= eDSNoErr;
	CFNumberRef					recRefNumber				= NULL;
	CFStringRef					stdAttrType					= NULL;
	tAttributeValueEntry	   *attrValueEntry				= NULL;
	CFMutableArrayRef			mutableAttrValues			= NULL;
	CFDictionaryRef				nodeDict					= NULL;
	CFMutableDictionaryRef		mutableRecordAttrsValues	= NULL;
	
	// don't allow user to set the value of kDSNAttrMetaNodeLocation or kDS1AttrMetaAutomountMap, just return eDSNoErr
	if ( inData->fInAttrType != NULL &&
		 (strcmp(inData->fInAttrType->fBufferData, kDSNAttrMetaNodeLocation) == 0 || strcmp(inData->fInAttrType->fBufferData, kDS1AttrMetaAutomountMap) == 0) )
		return eDSNoErr;		
	
	recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
	if ( recRefNumber == NULL )
		return eMemoryError;
	
	mOpenRecordRefsLock.WaitLock();
		
	try
	{
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::SetAttributeValue( (sSetAttributeValue *)inData, inRecTypeStr );
		
		CDSLocalPluginNode *node = NULL;
		nodeDict = this->CopyNodeDictandNodeObject( openRecordDict, &node );
		if ( nodeDict == NULL ) throw( ePlugInDataError );
		
		mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL ) throw( ePlugInDataError );
		
		stdAttrType = CFStringCreateWithCString( NULL, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
		if ( stdAttrType == NULL ) throw( eMemoryAllocError );
		
		// get the record type
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL ) throw( ePlugInDataError );
		
		// set the attribute type
		CFStringRef nativeAttrType = this->AttrNativeTypeForStandardType( stdAttrType );

		if ( !node->AccessAllowed(nodeDict, recordType, nativeAttrType, eDSAccessModeWriteAttr, mutableRecordAttrsValues) )
			throw( eDSPermissionError );

		// even though shell is a single-value attribute, we have to check anyway
		tDataBufferPriv *buffer = (tDataBufferPriv *) inData->fInAttrValueList->fDataListHead;
		while ( buffer != NULL )
		{
			if ( CheckForShellAndValidate(nodeDict, node, nativeAttrType, buffer->fBufferData, buffer->fBufferLength) == false ) {
				throw eDSSchemaError;
			}
			
			buffer = (tDataBufferPriv *) buffer->fNextPtr;
		}
		
		// handle binary write here
		mutableAttrValues = CreateCFArrayFromGenericDataList( inData->fInAttrValueList );
		if ( mutableAttrValues == NULL ) throw( eDSEmptyAttributeValue );
		
		// if we are modifying auth authority, we need to release mutex because we can deadlock trying to launch kadmin.local
		bool isAuthAuthorityAttr = (CFStringCompare(stdAttrType, CFSTR(kDSNAttrAuthenticationAuthority), 0) == kCFCompareEqualTo);
		
		if ( isAuthAuthorityAttr ) {
			CFRetain( openRecordDict );
			mOpenRecordRefsLock.SignalLock();
		}
		
		siResult = node->SetAttributeValuesInRecord( nativeAttrType, recordType, mutableRecordAttrsValues, mutableAttrValues );

		if ( isAuthAuthorityAttr ) {
			mOpenRecordRefsLock.WaitLock();
			CFRelease( openRecordDict );
		}
		
		if ( siResult == eDSNoErr ) {
			CFDictionarySetValue( openRecordDict, CFSTR(kOpenRecordDictRecordWasChanged), kCFBooleanTrue );
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::SetAttributeValues(): Got error %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	DSCFRelease( nodeDict );
	DSCFRelease( recRefNumber );
	DSCFRelease( stdAttrType );
	DSFree( attrValueEntry );
	DSCFRelease( mutableAttrValues );
		
	return siResult;
}

tDirStatus CDSLocalPlugin::GetRecAttrValueByID( sGetRecordAttributeValueByID* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFNumberRef recRefNumber = NULL;
	CFStringRef stdAttrType = NULL;
	tAttributeValueEntry* attrValueEntry = NULL;
	CFDictionaryRef nodeDict = NULL;
	char* cStr = NULL;
	size_t cStrSize = 0;
	const void *dataValue = NULL;

	mOpenRecordRefsLock.WaitLock();

	try
	{
		recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
		
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::GetRecAttrValueByID( inData );
		
		CDSLocalPluginNode *node = NULL;
		nodeDict = this->CopyNodeDictandNodeObject( openRecordDict, &node );
		if ( nodeDict == NULL ) throw( ePlugInDataError );
		
		CFMutableDictionaryRef mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL )
			throw( ePlugInDataError );
		
		stdAttrType = CFStringCreateWithCString( NULL, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
		if ( stdAttrType == NULL )
			throw( eMemoryAllocError );
		
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL ) throw( ePlugInDataError );
		
		// get the attribute value
		CFStringRef nativeAttrType = this->AttrNativeTypeForStandardType( stdAttrType );
		if ( node->AccessAllowed(nodeDict, recordType, nativeAttrType, eDSAccessModeReadAttr) == false ) 
			throw( eDSPermissionError );
		
		//handle binary read here
		CFTypeRef attrValue = NULL;
		siResult = node->GetAttributeValueByCRCFromRecord( nativeAttrType, mutableRecordAttrsValues, inData->fInValueID, &attrValue );
		if ( siResult != eDSNoErr ) throw( siResult );
		
		size_t attrValueLen = 0;
		
		CFTypeID attrValueTypeID = CFGetTypeID(attrValue);
		if ( attrValueTypeID == CFStringGetTypeID() )
		{
			dataValue = CStrFromCFString( (CFStringRef)attrValue, &cStr, &cStrSize, NULL );
			if ( dataValue == NULL ) throw( eMemoryAllocError );
			attrValueLen = strlen( (char *)dataValue );
		}
		else //CFDataRef since we only return this or a CFStringRef
		{
			dataValue = CFDataGetBytePtr( (CFDataRef)attrValue );
			attrValueLen = (size_t) CFDataGetLength( (CFDataRef)attrValue );
		}
		
		size_t attrValueEntrySize = sizeof( tAttributeValueEntry ) + attrValueLen  + 1 + eBuffPad;
		attrValueEntry = (tAttributeValueEntry*)::calloc( 1, attrValueEntrySize );
		if ( attrValueEntry == NULL ) throw( eMemoryAllocError );
		
		attrValueEntry->fAttributeValueID = inData->fInValueID; //no need to recalc this as we alreday have it
		attrValueEntry->fAttributeValueData.fBufferSize = attrValueLen;
		attrValueEntry->fAttributeValueData.fBufferLength = attrValueLen;
		
		::memcpy( attrValueEntry->fAttributeValueData.fBufferData, dataValue, attrValueLen );
		
		inData->fOutEntryPtr = attrValueEntry;
		
		attrValueEntry = NULL;	//everything's fine, so set this to NULL so it doesn't get free'd below
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::GetRecAttrValueByID(): Got error %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	DSFreeString( cStr );
	DSCFRelease( nodeDict );
	DSCFRelease( recRefNumber );
	DSCFRelease( stdAttrType );
	DSFree( attrValueEntry );
		
	return siResult;
}


tDirStatus CDSLocalPlugin::GetRecAttrValueByIndex( sGetRecordAttributeValueByIndex* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFNumberRef recRefNumber = NULL;
	CFStringRef stdAttrType = NULL;
	tAttributeValueEntry* attrValueEntry = NULL;
	char* cStr = NULL;
	size_t cStrSize = 0;
	const void *dataValue = NULL;
	CDSLocalPluginNode *node = NULL;
	CFDictionaryRef nodeDict = NULL;
	
	if ( inData == NULL )
		return eParameterError;
	if ( inData->fInAttrValueIndex == 0 )
		return eDSInvalidIndex;
	
	mOpenRecordRefsLock.WaitLock();
	
	try
	{		
		recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
		
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::GetRecAttrValueByIndex( inData );
		
		nodeDict = this->CopyNodeDictandNodeObject( openRecordDict, &node );
		if ( nodeDict == NULL )
			throw( ePlugInDataError );
		if ( node == NULL )
			throw( eDSInvalidNodeRef );
		
		CFMutableDictionaryRef mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL )
			throw( ePlugInDataError );
		
		stdAttrType = CFStringCreateWithCString( NULL, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
		if ( stdAttrType == NULL )
			throw( eMemoryAllocError );
		
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL ) throw( ePlugInDataError );
		
		// get the attribute value
		CFStringRef nativeAttrType = this->AttrNativeTypeForStandardType( stdAttrType );
		if ( node->AccessAllowed(nodeDict, recordType, nativeAttrType, eDSAccessModeReadAttr) == false ) 
			throw( eDSPermissionError );
		
		//handle binary read here
		CFTypeRef attrValue = NULL;
		siResult = node->GetAttributeValueByIndexFromRecord( nativeAttrType, mutableRecordAttrsValues, inData->fInAttrValueIndex - 1, &attrValue );
		if ( siResult != eDSNoErr ) throw( siResult );
		
		size_t attrValueLen = 0;
		UInt32 aCRCValue = 0;
		
		CFTypeID attrValueTypeID = CFGetTypeID( attrValue );
		if ( attrValueTypeID == CFStringGetTypeID() )
		{
			dataValue = CStrFromCFString( (CFStringRef)attrValue, &cStr, &cStrSize, NULL );
			if ( dataValue == NULL ) throw( eMemoryAllocError );
			attrValueLen = strlen( (char *)dataValue );
			aCRCValue = CalcCRCWithLength( dataValue, attrValueLen );
		}
		else //CFDataRef since we only return this or a CFStringRef
		{
			dataValue = CFDataGetBytePtr( (CFDataRef)attrValue );
			attrValueLen = (size_t) CFDataGetLength( (CFDataRef)attrValue );
			aCRCValue = CalcCRCWithLength( dataValue, attrValueLen );
		}		
		
		size_t attrValueEntrySize = sizeof( tAttributeValueEntry ) + attrValueLen  + 1 + eBuffPad;
		attrValueEntry = (tAttributeValueEntry*)::calloc( 1, attrValueEntrySize );
		if ( attrValueEntry == NULL ) throw( eMemoryAllocError );
		
		attrValueEntry->fAttributeValueID = aCRCValue;
		attrValueEntry->fAttributeValueData.fBufferSize = attrValueLen;
		attrValueEntry->fAttributeValueData.fBufferLength = attrValueLen;
		
		::memcpy( attrValueEntry->fAttributeValueData.fBufferData, dataValue, attrValueLen );
		
		inData->fOutEntryPtr = attrValueEntry;
				
		attrValueEntry = NULL;	//everything's fine, so set this to NULL so it doesn't get free'd below
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::GetRecAttrValueByIndex(): Got error %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	DSCFRelease( nodeDict );
	DSCFRelease( recRefNumber );
	DSCFRelease( stdAttrType );
	DSFree( attrValueEntry );
	DSFree( cStr );
		
	return siResult;
}


tDirStatus CDSLocalPlugin::GetRecAttrValueByValue( sGetRecordAttributeValueByValue* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFNumberRef recRefNumber = NULL;
	CFStringRef stdAttrType = NULL;
	tAttributeValueEntry* attrValueEntry = NULL;
	CFDictionaryRef nodeDict = NULL;
	char* cStr = NULL;
	size_t cStrSize = 0;
	const void *dataValue = NULL;
	CFTypeRef attrValue = NULL;
	
	recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
	if ( recRefNumber == NULL )
		return( ePlugInDataError );
	
	mOpenRecordRefsLock.WaitLock();

	try
	{
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::GetRecAttrValueByValue( inData );
		
		CDSLocalPluginNode *node = NULL;
		nodeDict = this->CopyNodeDictandNodeObject( openRecordDict, &node );
		if ( nodeDict == NULL )
			throw( ePlugInDataError );
		if ( node == NULL )
			throw( ePlugInDataError );
		
		CFMutableDictionaryRef mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL )
			throw( ePlugInDataError );
		
		stdAttrType = CFStringCreateWithCString( NULL, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
		if ( stdAttrType == NULL )
			throw( eMemoryAllocError );
		
		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL ) throw( ePlugInDataError );
		
		// get the attribute value
		CFStringRef nativeAttrType = this->AttrNativeTypeForStandardType( stdAttrType );
		if ( node->AccessAllowed(nodeDict, recordType, nativeAttrType, eDSAccessModeReadAttr) == false ) 
			throw( eDSPermissionError );
		
		// create reference
		attrValue = CFStringCreateWithCStringNoCopy( NULL, inData->fInAttrValue->fBufferData, kCFStringEncodingUTF8, kCFAllocatorNull );
		if ( attrValue == NULL )
		{
			attrValue = CFDataCreateWithBytesNoCopy( NULL, (const UInt8*) inData->fInAttrValue->fBufferData, inData->fInAttrValue->fBufferLength, kCFAllocatorNull );
		}
		
		CFArrayRef attrValues = (CFArrayRef) CFDictionaryGetValue( mutableRecordAttrsValues, nativeAttrType );
		if ( attrValues == NULL || CFArrayContainsValue(attrValues, CFRangeMake(0, CFArrayGetCount(attrValues)), attrValue) == FALSE )
			siResult = eDSAttributeNotFound;
				
		if ( siResult != eDSNoErr ) throw( siResult );
		
		size_t attrValueLen = 0;
		UInt32 aCRCValue = 0;
		
		CFTypeID attrValueTypeID = CFGetTypeID(attrValue);
		if ( attrValueTypeID == CFStringGetTypeID() )
		{
			dataValue = CStrFromCFString( (CFStringRef)attrValue, &cStr, &cStrSize, NULL );
			if ( dataValue == NULL ) throw( eMemoryAllocError );
			attrValueLen = strlen( (char *)dataValue );
			aCRCValue = CalcCRCWithLength( dataValue, attrValueLen );
		}
		else //CFDataRef since we only return this or a CFStringRef
		{
			dataValue = CFDataGetBytePtr( (CFDataRef)attrValue );
			attrValueLen = (size_t) CFDataGetLength( (CFDataRef)attrValue );
			aCRCValue = CalcCRCWithLength( dataValue, attrValueLen );
		}		
		
		size_t attrValueEntrySize = sizeof( tAttributeValueEntry ) + attrValueLen  + 1 + eBuffPad;
		attrValueEntry = (tAttributeValueEntry*)::calloc( 1, attrValueEntrySize );
		if ( attrValueEntry == NULL ) throw( eMemoryAllocError );
		
		attrValueEntry->fAttributeValueID = aCRCValue;
		attrValueEntry->fAttributeValueData.fBufferSize = attrValueLen;
		attrValueEntry->fAttributeValueData.fBufferLength = attrValueLen;
		
		::memcpy( attrValueEntry->fAttributeValueData.fBufferData, dataValue, attrValueLen );
		
		inData->fOutEntryPtr = attrValueEntry;
		
		attrValueEntry = NULL;	//everything's fine, so set this to NULL so it doesn't get free'd below
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::GetRecAttrValueByValue(): Got error %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	DSCFRelease( nodeDict );
	DSCFRelease( attrValue );
	DSCFRelease( recRefNumber );
	DSCFRelease( stdAttrType );
	DSFree( attrValueEntry );
	DSFree( cStr );
	
	return siResult;
}


tDirStatus CDSLocalPlugin::GetRecRefInfo( sGetRecRefInfo* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFNumberRef recRefNumber = NULL;
	char* cStr = NULL;
	size_t cStrSize = 0;
	char* cStr2 = NULL;
	size_t cStrSize2 = 0;
	tRecordEntry* recEntry = NULL;
	CDataBuff* dataBuff = NULL;
	CFStringRef stdRecType = NULL;
	
	mOpenRecordRefsLock.WaitLock();
	
	try
	{
		recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
		
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::GetRecRefInfo( inData );
		
		CFDictionaryRef nodeDict = (CFDictionaryRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictNodeDict ) );
		if ( nodeDict == NULL )
			throw( ePlugInDataError );
		
		CFMutableDictionaryRef mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL )
			throw( ePlugInDataError );

		CFStringRef nativeRecType = (CFStringRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictRecordType ) );
		if ( nativeRecType == NULL )
			throw( ePlugInDataError );
		stdRecType = this->RecordStandardTypeForNativeType( nativeRecType );
		if ( stdRecType != NULL )
		{
			CFRetain( stdRecType );
		}
		else
		{
			stdRecType = CFStringCreateWithFormat( NULL, NULL, CFSTR("%s%@"), kDSNativeRecordTypePrefix, nativeRecType );
		}
		
		CFArrayRef recordNames = (CFArrayRef)CFDictionaryGetValue( mutableRecordAttrsValues,
			this->AttrNativeTypeForStandardType( CFSTR( kDSNAttrRecordName ) ) );
		if ( recordNames == NULL || CFArrayGetCount( recordNames ) == 0 ) throw( ePlugInDataError );
		CFStringRef recordName = (CFStringRef)CFArrayGetValueAtIndex( recordNames, 0 );
		if ( recordName == NULL )
			throw( ePlugInDataError );

		const char* recordNameCStr = CStrFromCFString( recordName, &cStr, &cStrSize, NULL );
		const char* recTypeCStr = CStrFromCFString( stdRecType, &cStr2, &cStrSize2, NULL );
		
		size_t recNameLen = ::strlen( recordNameCStr );
		size_t recTypeLen = ::strlen( recTypeCStr );
		size_t recEntrySize = sizeof( tRecordEntry ) + 2 * sizeof( short ) + recNameLen + recTypeLen;
		
		recEntry = (tRecordEntry*)::calloc( 1, recEntrySize );
		
		dataBuff = new CDataBuff();
		dataBuff->AppendShort( recNameLen );
		dataBuff->AppendString( recordNameCStr );
		dataBuff->AppendShort( recTypeLen );
		dataBuff->AppendString( recTypeCStr );
		
		::memcpy( recEntry->fRecordNameAndType.fBufferData, dataBuff->GetData(), dataBuff->GetLength() );
		recEntry->fRecordNameAndType.fBufferSize = 2 * sizeof( short ) + recNameLen + recTypeLen;
		recEntry->fRecordNameAndType.fBufferLength = dataBuff->GetLength();

		inData->fOutRecInfo = recEntry;
		
		recEntry = NULL;	// make this null so it  doesn't get freed below
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::GetRecRefInfo(): got error of %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	DSCFRelease( stdRecType );
	DSCFRelease( recRefNumber );
	DSFreeString( cStr );
	DSFreeString( cStr2 );
	DSFree( recEntry );

	if ( dataBuff != NULL )
	{
		delete dataBuff;
		dataBuff = NULL;
	}
	
	return siResult;
}

tDirStatus CDSLocalPlugin::GetRecAttribInfo( sGetRecAttribInfo* inData )
{
	tDirStatus siResult = eDSNoErr;
	CFNumberRef recRefNumber = NULL;
	tAttributeEntry* attrEntry = NULL;
	CFStringRef attrType = NULL;
	CFDictionaryRef nodeDict = NULL;
	
	mOpenRecordRefsLock.WaitLock();

	try
	{
		recRefNumber = CFNumberCreate( NULL, kCFNumberIntType, &inData->fInRecRef );
		
		CFMutableDictionaryRef openRecordDict = RecordDictForRecordRef( inData->fInRecRef );
		if ( openRecordDict == NULL ) throw BaseDirectoryPlugin::GetRecAttribInfo( inData );
		
		CDSLocalPluginNode *node = NULL;
		nodeDict = this->CopyNodeDictandNodeObject( openRecordDict, &node );
		if ( nodeDict == NULL || node == NULL )
			throw( ePlugInDataError );

		CFMutableDictionaryRef mutableRecordAttrsValues = (CFMutableDictionaryRef)CFDictionaryGetValue( openRecordDict,
			CFSTR( kOpenRecordDictAttrsValues ) );
		if ( mutableRecordAttrsValues == NULL )
			throw( ePlugInDataError );

		attrType = CFStringCreateWithCString( NULL, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
		if ( attrType == NULL )
			throw( eMemoryAllocError );

		CFStringRef recordType = (CFStringRef)CFDictionaryGetValue( openRecordDict, CFSTR( kOpenRecordDictRecordType ) );
		if ( recordType == NULL ) throw( ePlugInDataError );
		
		// get the attribute value
		CFStringRef nativeAttrType = AttrNativeTypeForStandardType( attrType );
		if ( node->AccessAllowed(nodeDict, recordType, nativeAttrType, eDSAccessModeReadAttr) == false ) 
			throw( eDSPermissionError );
		
		// need to handle both attribute with no values and no attribute cases.
		CFArrayRef attrValues = NULL;
		if ( ! CFDictionaryGetValueIfPresent(mutableRecordAttrsValues, nativeAttrType, (const void **)&attrValues) )
			throw( eDSAttributeNotFound );
		
		size_t attrTypeLen = ::strlen( inData->fInAttrType->fBufferData ) + 1;
		size_t attrEntrySize = sizeof( tAttributeEntry ) + attrTypeLen + eBuffPad;
		attrEntry = (tAttributeEntry*)::calloc( 1, attrEntrySize );

		attrEntry->fAttributeSignature.fBufferSize = attrTypeLen;
		attrEntry->fAttributeSignature.fBufferLength = attrTypeLen;
		::memcpy( attrEntry->fAttributeSignature.fBufferData, inData->fInAttrType->fBufferData, attrTypeLen ); 

		if ( attrValues == NULL )
			attrEntry->fAttributeValueCount = 0;
		else
			attrEntry->fAttributeValueCount = (UInt32)CFArrayGetCount( attrValues );
		attrEntry->fAttributeValueMaxSize = 1024;

		inData->fOutAttrInfoPtr = attrEntry;
		
		attrEntry = NULL;	// make this null so it  doesn't get freed below
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::GetRecAttribInfo(): got error of %d", err );
		siResult = err;
	}
	
	mOpenRecordRefsLock.SignalLock();
	
	DSCFRelease( recRefNumber );
	DSCFRelease( attrType );
	DSCFRelease( nodeDict );
	
	if ( attrEntry != NULL )
	{
		::free( attrEntry );
		attrEntry = NULL;
	}
		
	return siResult;
}


tDirStatus CDSLocalPlugin::DoAuthentication( sDoDirNodeAuth *inData, const char *inRecTypeStr,
													CDSAuthParams &inParams )
{
	tDirStatus				siResult					= eDSAuthFailed;
	tDirStatus				siResult2					= eDSAuthFailed;
	CFStringRef				userName					= NULL;
	CFMutableDictionaryRef	mutableRecordDict			= NULL;
	char*					cStr						= NULL;
	size_t					cStrSize					= 0;
	char*					cStr2						= NULL;
	size_t					cStrSize2					= 0;
	UInt32					authMethod					= inParams.uiAuthMethod;
	UInt32					settingPolicy				= 0;
	tDataNodePtr			origAuthMethod				= NULL;
	bool					origAuthMethodAuthOnly		= true;
	char*					origAuthMethodStr			= NULL;
	tDataBufferPtr			origAuthStepData			= NULL;
	const char*				guidCStr					= NULL;
	bool					authedUserIsAdmin			= false;
	bool					methodCaresAboutAdminStatus	= false;
	char*					userNameCStr				= NULL;
	CFStringRef				authedUserName				= NULL;
	char*					anAuthenticatorNameCStr		= NULL;
	char*					anAuthenticatorPasswordCStr	= NULL;
	char*					authAuthorityStr			= NULL;
	char*					aaVersion					= NULL;
	char*					aaTag						= NULL;
	char*					aaData						= NULL;
	CFMutableDictionaryRef	nodeDict					= NULL;
	CAuthAuthority			authAuthorityList;
	
	if ( inData->fInAuthStepData == NULL )
		return( eDSNullAuthStepData );
	if ( inRecTypeStr == NULL )
		return( eDSNullRecType );
	
	CFStringRef stdType = CFStringCreateWithCString( NULL, inRecTypeStr, kCFStringEncodingUTF8 );
	CFStringRef nativeRecType = this->RecordNativeTypeForStandardType( stdType );
	CFRelease( stdType );
	stdType = NULL;

	if ( nativeRecType == NULL ) return eDSInvalidRecordType;

	nodeDict = CopyNodeDictForNodeRef( inData->fInNodeRef );
	if ( nodeDict == NULL ) return BaseDirectoryPlugin::DoAuthentication( inData, inRecTypeStr, inParams );
	
	try
	{
		// get the authenticated user name
		authedUserName = (CFStringRef)CFDictionaryGetValue( nodeDict, CFSTR( kNodeAuthenticatedUserName ) );
		if ( authedUserName != NULL )
			CFRetain( authedUserName );
		
		// get the target user
		if ( inParams.pUserName != NULL )
			userName = CFStringCreateWithCString( kCFAllocatorDefault, inParams.pUserName, kCFStringEncodingUTF8 );
		
		CFNumberRef uidNumber = (CFNumberRef)CFDictionaryGetValue( nodeDict, CFSTR( kNodeUIDKey ) );
		uid_t uid = 99;
		CFNumberGetValue( uidNumber, kCFNumberIntType, &uid );

		CFNumberRef effectiveUIDNumber = (CFNumberRef)CFDictionaryGetValue( nodeDict, CFSTR( kNodeEffectiveUIDKey ) );
		uid_t effectiveUID = 99;
		CFNumberGetValue( effectiveUIDNumber, kCFNumberIntType, &effectiveUID );
		
		CDSLocalPluginNode* node = this->NodeObjectFromNodeDict( nodeDict );
		
		if ( ( mHashList & ePluginHashHasReadConfig ) == 0 )
			this->ReadHashConfig( node );

		CFMutableDictionaryRef continueDataDict = NULL;
		if ( inData->fIOContinueData != 0 )
		{
			// get info from continue
			continueDataDict = (CFMutableDictionaryRef) gLocalContinueTable->GetPointer( inData->fIOContinueData );
			if ( continueDataDict == NULL || CFGetTypeID( continueDataDict ) != CFDictionaryGetTypeID() )
				throw( eDSInvalidContinueData );

			CFStringRef aaTagString = (CFStringRef)CFDictionaryGetValue( continueDataDict, CFSTR(kAuthContinueDataHandlerTag) );
			if ( aaTagString == NULL )
				throw( eDSInvalidContinueData );
			
			const char *aaTagCStr = CStrFromCFString( aaTagString, &cStr2, &cStrSize2, NULL );
			AuthAuthorityHandlerProc handlerProc = CDSLocalAuthHelper::GetAuthAuthorityHandler( aaTagCStr );
			
			CFStringRef authAuthorityCFString = (CFStringRef)CFDictionaryGetValue( continueDataDict,
				CFSTR(kAuthContinueDataAuthAuthority) );
			
			authAuthorityList.AddValue( authAuthorityCFString );
			
			mutableRecordDict = (CFMutableDictionaryRef)CFDictionaryGetValue( continueDataDict,
				CFSTR( kAuthContinueDataMutableRecordDict ) );
			if ( mutableRecordDict != NULL )
				CFRetain( mutableRecordDict );
			
			CFNumberRef authedUserIsAdminNumber = (CFNumberRef)CFDictionaryGetValue( continueDataDict,
				CFSTR( kAuthContinueDataAuthedUserIsAdmin ) );

			if ( authedUserIsAdminNumber != NULL )
				CFNumberGetValue( authedUserIsAdminNumber, kCFNumberCharType, &authedUserIsAdmin );

			//parse this value of auth authority
			const char* authAuthorityCStr = CStrFromCFString( authAuthorityCFString, &cStr2, &cStrSize2, NULL );
			siResult2 = dsParseAuthAuthority( authAuthorityCStr, &aaVersion, &aaTag, &aaData );
			// JT need to check version
			if (siResult2 != eDSNoErr)
			{
				throw(eDSAuthFailed);
				//KW do we want to bail if one of the writes failed?
				//could end up with mismatched passwords/hashes regardless of how we handle this
				//note "continue" here instead of "break" would have same effect because of
				//check for siResult in "while" above
				//break;
			}
			
			// if the tag is ;DisabledUser; then we need all of the data section
			if ( aaTag != NULL && strcasecmp(aaTag, kDSTagAuthAuthorityDisabledUser) == 0 )
			{
				if ( aaData != NULL )
					DSFreeString( aaData );
				aaData = strdup( authAuthorityCStr + strlen(aaVersion) + strlen(aaTag) + 2 );
			}
			
			// get the GUID
			CFStringRef guid = NULL;
			CFArrayRef guids = (CFArrayRef)CFDictionaryGetValue( mutableRecordDict,
				this->AttrNativeTypeForStandardType( CFSTR( kDS1AttrGeneratedUID ) ) );
			if ( ( guids != NULL ) && ( CFArrayGetCount( guids ) > 0 ) )
			{
				guid = (CFStringRef)CFArrayGetValueAtIndex( guids, 0 );
				guidCStr = CStrFromCFString( guid, &cStr, &cStrSize, NULL );
			}
			
			siResult = (handlerProc)( inData->fInNodeRef, (CDSLocalAuthParams &)inParams, &inData->fIOContinueData,
				inData->fInAuthStepData, inData->fOutAuthStepDataResponse, inData->fInDirNodeAuthOnlyFlag, false, authAuthorityList,
				guidCStr, authedUserIsAdmin, mutableRecordDict, mHashList, this, node, authedUserName, uid, effectiveUID,
				nativeRecType );
			DSFreeString( aaVersion );
			DSFreeString( aaTag );
			DSFreeString( aaData );
		}
		else
		{
			// first call
			// we do not want to fail if the method is unknown at this point.
			// For password server users, the PSPlugin may know the method.
			if (inParams.mAuthMethodStr != NULL) 
			{
				DbgLog( kLogDebug, "CDSLocalPlugin::DoAuthentication(): Attempting use of authentication method %s",
					inParams.mAuthMethodStr );
			}
			
			// unsupported auth methods are allowed if the user is on password server
			// otherwise, unsupported auth methods are rejected in their handlers
			
			// set flag for admin status
			switch( inParams.uiAuthMethod )
			{
				case kAuthSetPasswdAsRoot:
				case kAuthWriteSecureHash:
				case kAuthNTSetWorkstationPasswd:
				case kAuthNTSetNTHash:
				case kAuthSetLMHash:
				case kAuthSMB_NTUserSessionKey:
				case kAuthSetPolicyAsRoot:
				case kAuthMSLMCHAP2ChangePasswd:
				case kAuthSetShadowHashWindows:
				case kAuthSetShadowHashSecure:
				case kAuthSetComputerAcctPasswdAsRoot:
					methodCaresAboutAdminStatus = true;
					break;
				
				default:
					methodCaresAboutAdminStatus = false;
			}
			
			// For user policy, we need the GUID
			if ( inParams.uiAuthMethod == kAuthGetEffectivePolicy || inParams.uiAuthMethod == kAuthGetPolicy || inParams.uiAuthMethod == kAuthSetPolicy )
			{
				if ( userName == NULL )
					throw( eDSInvalidBuffFormat );
					
				DSCFRelease( mutableRecordDict );
				siResult = this->GetRetainedRecordDict( userName, nativeRecType, node, &mutableRecordDict );
				if ( siResult != eDSNoErr )
					throw( siResult );
				siResult = eDSAuthFailed;
				
				if ( CDSLocalAuthHelper::AuthAuthoritiesHaveTag( userName, (CFArrayRef)CFDictionaryGetValue(
					mutableRecordDict, this->AttrNativeTypeForStandardType( CFSTR( kDSNAttrAuthenticationAuthority ) ) ),
					CFSTR( kDSTagAuthAuthorityShadowHash ) ) )
				{
					CFArrayRef guids = (CFArrayRef)CFDictionaryGetValue( mutableRecordDict,
						this->AttrNativeTypeForStandardType( CFSTR( kDS1AttrGeneratedUID ) ) );
					CFStringRef guid = NULL;
					if ( CFArrayGetCount( guids ) > 0 )
						guid = (CFStringRef)CFArrayGetValueAtIndex( guids, 0 );
					
					guidCStr = CStrFromCFString( guid, &cStr, &cStrSize, NULL );
					if ( inParams.uiAuthMethod == kAuthGetPolicy || inParams.uiAuthMethod == kAuthGetEffectivePolicy )
					{
						// no permissions required, just do it.
						siResult = CDSLocalAuthHelper::DoShadowHashAuth( inData->fInNodeRef, (CDSLocalAuthParams &)inParams,
								&inData->fIOContinueData, inData->fInAuthStepData, inData->fOutAuthStepDataResponse,
								inData->fInDirNodeAuthOnlyFlag, false, authAuthorityList, guidCStr, authedUserIsAdmin, mutableRecordDict,
								mHashList, this, node, authedUserName, uid, effectiveUID, nativeRecType );
					
						// we're done
						throw( siResult );
					}
				}
			}
				
			// For SetPolicy operations, check the permissions
			if ( inParams.uiAuthMethod == kAuthSetGlobalPolicy || inParams.uiAuthMethod == kAuthSetPolicy )
			{
				bool authenticatorFieldsHaveData = true;
				
				// to use root permissions, the authenticator and authenticator password
				// fields must be blank.
				if ( effectiveUID == 0 && BufferFirstTwoItemsEmpty(inData->fInAuthStepData) )
				{
					AuthenticateRoot( nodeDict, &authedUserIsAdmin, &authedUserName );
					authenticatorFieldsHaveData = false;
				}
				
				if ( effectiveUID != 0 || authenticatorFieldsHaveData )
				{
					settingPolicy = inParams.uiAuthMethod;
					authMethod = inParams.uiAuthMethod = kAuthNativeClearTextOK;
					origAuthMethodAuthOnly = inData->fInDirNodeAuthOnlyFlag;
					inData->fInDirNodeAuthOnlyFlag = false;
					origAuthMethodStr = inParams.mAuthMethodStr;
					inParams.mAuthMethodStr = strdup( kDSStdAuthNodeNativeClearTextOK );
					
					origAuthMethod = inData->fInAuthMethod;
					inData->fInAuthMethod = dsDataNodeAllocateString( 0, kDSStdAuthNodeNativeClearTextOK );
					if ( inData->fInAuthMethod == NULL )
						throw( eMemoryError );
					if ( inParams.pAdminUser == NULL )
						throw( eDSAuthUnknownUser );
					
					// check to see if the authenticator is an admin
					authedUserIsAdmin = dsIsUserMemberOfGroup( inParams.pAdminUser, "admin" );
				}
				else
				{
					// go directly to shadowhash, do not pass go...
					siResult = CDSLocalAuthHelper::DoShadowHashAuth( inData->fInNodeRef, (CDSLocalAuthParams &)inParams,
									&inData->fIOContinueData, inData->fInAuthStepData, inData->fOutAuthStepDataResponse,
									inData->fInDirNodeAuthOnlyFlag, false, authAuthorityList, guidCStr, authedUserIsAdmin, 
									mutableRecordDict, mHashList, this, node, authedUserName, uid, effectiveUID,
									nativeRecType );
						
					// we're done here
					throw( siResult );
				}
			}
			else if ( methodCaresAboutAdminStatus )
			{
				if ( authedUserName != NULL )
				{
					char username[256];
					
					if ( CFStringGetCString(authedUserName, username, sizeof(username), kCFStringEncodingUTF8) == true ) {
						authedUserIsAdmin = dsIsUserMemberOfGroup( username, "admin" );
					}
				}
			}
			else if ( effectiveUID == 0 )
			{
				AuthenticateRoot( nodeDict, &authedUserIsAdmin, &authedUserName );
			}
			
			switch( authMethod )
			{
				case kAuthWithAuthorizationRef:
				{
					AuthorizationRef authRef = 0;
					AuthorizationItemSet* resultRightSet = NULL;
					if ( inData->fInAuthStepData->fBufferLength < sizeof( AuthorizationExternalForm ) )
						throw( eDSInvalidBuffFormat );
					siResult = (tDirStatus)AuthorizationCreateFromExternalForm(
						(AuthorizationExternalForm *)inData->fInAuthStepData->fBufferData, &authRef);
					if (siResult != (tDirStatus)errAuthorizationSuccess)
					{
						DbgLog( kLogNotice, "CDSLocalPlugin::DoAuthentication(): AuthorizationCreateFromExternalForm() \
							returned error %d", siResult );
						throw( eDSPermissionError );
					}

					AuthorizationItem rights[] = { {"system.preferences", 0, 0, 0} };
					AuthorizationItemSet rightSet = { sizeof(rights)/ sizeof(*rights), rights };

					siResult = (tDirStatus)AuthorizationCopyRights(authRef, &rightSet, NULL,
										kAuthorizationFlagExtendRights, &resultRightSet);
					if (resultRightSet != NULL)
					{
						AuthorizationFreeItemSet(resultRightSet);
						resultRightSet = NULL;
					}
					if (siResult != (tDirStatus)errAuthorizationSuccess)
					{
						DbgLog( kLogNotice, "CDSLocalPlugin::DoAuthentication(): AuthorizationCopyRights returned error %d",
							siResult );
						throw( eDSPermissionError );
					}
					if (inData->fInDirNodeAuthOnlyFlag == false)
					{
						//TODO: KW does this make sense?
						AuthenticateRoot( nodeDict, &authedUserIsAdmin, &authedUserName );
					}
					AuthorizationFree( authRef, 0 ); // really should hang onto this instead
					siResult = eDSNoErr;
				}
				break;
				
				case kAuthGetGlobalPolicy:
				{
					// kAuthGetGlobalPolicy is not associated with a user record and does not
					// have access to an authentication_authority attribute. If kAuthGetGlobalPolicy
					// is requested of a Local node, then always return the shadowhash global
					// policies.
					
					siResult = CDSLocalAuthHelper::DoShadowHashAuth( inData->fInNodeRef, (CDSLocalAuthParams &)inParams,
							&inData->fIOContinueData, inData->fInAuthStepData, inData->fOutAuthStepDataResponse,
							inData->fInDirNodeAuthOnlyFlag, false, authAuthorityList, NULL, authedUserIsAdmin, mutableRecordDict,
							mHashList, this, node, authedUserName, uid, effectiveUID, nativeRecType );
				}
				break;
				
				default: //everything but AuthRef method, GetGlobalPolicy and kAuthSetGlobalPolicy
				{
					if ( authMethod != kAuth2WayRandom )
					{
						siResult = (tDirStatus)GetUserNameFromAuthBuffer( inData->fInAuthStepData, 1, &userNameCStr );
						if ( siResult != eDSNoErr )
						{
							DbgLog( kLogPlugin, "CDSLocalPlugin::DoAuthentication(): GetUserNameFromAuthBuffer failed with error of %d", siResult );
							throw( siResult );
						}
						if (userNameCStr == NULL)
							throw( eDSNullRecName );
						DSCFRelease( userName );
						userName = CFStringCreateWithCString( NULL, userNameCStr, kCFStringEncodingUTF8 );
					}
					else
					{
						// for 2way random the first buffer is the username
						if ( inData->fInAuthStepData->fBufferLength > inData->fInAuthStepData->fBufferSize )
							throw( eDSInvalidBuffFormat );
						userNameCStr = dsCStrFromCharacters( inData->fInAuthStepData->fBufferData, inData->fInAuthStepData->fBufferLength );
						DSCFRelease( userName );
						userName = CFStringCreateWithCString( NULL, userNameCStr, kCFStringEncodingUTF8 );
					}
					
					DSCFRelease( mutableRecordDict );
					siResult = this->GetRetainedRecordDict( userName, nativeRecType, node, &mutableRecordDict );

					if ( siResult != eDSNoErr )
					{
						// only root can set root
						if ( authMethod == kAuthSetPasswdAsRoot )
						{
							if ( userNameCStr == NULL || strcmp(userNameCStr, "root") == 0 && effectiveUID != 0 )
								throw( eDSPermissionError );
						}
						
						bool bAdminUserAuthUsed = false;
						//here we check if this is AuthOnly == false AND that siResult is
						//not eDSNoErr AND local node only
						if ( ( node->IsLocalNode() ) && !(inData->fInDirNodeAuthOnlyFlag) && (siResult != eDSNoErr) )
						{
							if ( dsIsUserMemberOfGroup(userNameCStr, "admin") == true )
							{
								tDataList *usersNode = NULL;
								const char* memberNameCStr = CStrFromCFString( userName, &cStr, &cStrSize, NULL );
								usersNode = CDSLocalAuthHelper::FindNodeForSearchPolicyAuthUser( memberNameCStr );
								if ( usersNode != NULL )
								{
									SInt32				aResult		= eDSNoErr;
									tDirNodeReference	aNodeRef	= 0;
									tContextData		tContinue	= 0;

									aResult = this->GetDirServiceRef( NULL );
									if ( aResult == eDSNoErr )
									{
										aResult = dsOpenDirNode( mDSRef, usersNode, &aNodeRef );
										dsDataListDeallocatePriv( usersNode );
										free( usersNode );
										usersNode = NULL;
										if ( aResult == eDSNoErr )
										{
											aResult = dsDoDirNodeAuth( aNodeRef, inData->fInAuthMethod, true,
												inData->fInAuthStepData, inData->fOutAuthStepDataResponse, &tContinue );
											//no checking of continue data
											if ( aResult == eDSNoErr )
											{
												//user auth has succeeded on the authentication search policy
												//need to get password out of inData->fInAuthStepData
												tDataList *dataList = NULL;
												// parse input first
												dataList = dsAuthBufferGetDataListAllocPriv(inData->fInAuthStepData);
												if ( dataList != NULL )
												{
													if ( dsDataListGetNodeCountPriv(dataList) >= 2 )
													{
														char *pwd = NULL;
														// this allocates a copy of the password string
														pwd = dsDataListGetNodeStringPriv(dataList, 2);
														if ( pwd != NULL )
														{
															siResult = this->AuthOpen( inData->fInNodeRef, memberNameCStr,
																pwd, true );
															bAdminUserAuthUsed = true;
															DSFreePassword( pwd );
														}
													}
													dsDataListDeallocatePriv(dataList);
													free(dataList);
													dataList = NULL;
												}
											}
											dsCloseDirNode( aNodeRef );
											aNodeRef = 0;
										}// if ( aResult == eDSNoErr) from aResult = dsOpenDirNode(mDSRef, usersNode,
										//	&aNodeRef);
									}// if (aResult == eDSNoErr) from aResult = dsOpenDirService(&mDSRef);
								}// if (usersNode != NULL)
							}// if (UserIsAdmin)
						}// if ( (pContext->bIsLocal) && !(inData->fInDirNodeAuthOnlyFlag) && (siResult != eDSNoErr) )
						if ( bAdminUserAuthUsed )
						{
						/* TODO: Need to deal with this code as it is nonfunctional
							// get the GUID
							CFArrayRef guids = (CFArrayRef)CFDictionaryGetValue( mutableRecordDict,
								this->AttrNativeTypeForStandardType( CFSTR( kDS1AttrGeneratedUID ) ) );
							CFStringRef guid = NULL;
							if ( CFArrayGetCount( guids ) > 0 )
								guid = (CFStringRef)CFArrayGetValueAtIndex( guids, 0 );

							// If <UserGUIDString> is non-NULL, then the target user is ShadowHash
							if ( settingPolicy && siResult == eDSNoErr && guid != NULL )
							{
								guidCStr = CStrFromCFString( guid, &cStr, &cStrSize, NULL );
								// now that the admin is authorized, set the policy
								authMethod = settingPolicy;
								
								// transfer ownership of the auth method constant
								dsDataBufferDeallocatePriv( inData->fInAuthMethod );
								inData->fInAuthMethod = origAuthMethod;
								origAuthMethod = NULL;
								siResult = CDSLocalAuthHelper::DoShadowHashAuth( inData->fInNodeRef,
								inParams, &continueDataDict, inData->fInAuthStepData,
									inData->fOutAuthStepDataResponse, inData->fInDirNodeAuthOnlyFlag, false, NULL,
									guidCStr, authedUserIsAdmin, mutableRecordDict, mHashList, this, node,
									authedUserName, uid, effectiveUID, nativeRecType );
							}
							*/
							throw(siResult);
						}
						else
						{
							throw( eDSAuthFailed ); // unknown user really
						}
					}

					// get the GUID
					CFArrayRef guids = (CFArrayRef)CFDictionaryGetValue( mutableRecordDict,
						this->AttrNativeTypeForStandardType( CFSTR( kDS1AttrGeneratedUID ) ) );
					CFStringRef guid = NULL;
					if ( ( guids != NULL ) && ( CFArrayGetCount( guids ) > 0 ) )
						guid = (CFStringRef)CFArrayGetValueAtIndex( guids, 0 );

					// get the auth authorities
					CFArrayRef authAuthorities = (CFArrayRef)CFDictionaryGetValue( mutableRecordDict,
						this->AttrNativeTypeForStandardType( CFSTR( kDSNAttrAuthenticationAuthority ) ) );
					CFIndex numAuthAuthorities = 0;

					if ( authAuthorityList.AddValues(authAuthorities) )
					{
						// loop through all possibilities for set
						// do first auth authority that supports the method for check password
						CFIndex i = 0;
						bool bLoopAll = CDSLocalAuthHelper::IsWriteAuthRequest(authMethod);
						bool bIsSecondary = false;
						
						if ( guid != NULL )
							guidCStr = CStrFromCFString( guid, &cStr, &cStrSize, NULL );

						siResult = eDSAuthMethodNotSupported;
						
						numAuthAuthorities = authAuthorityList.GetValueCount();
						for ( i = 0; i < numAuthAuthorities 
								&& (siResult == eDSAuthMethodNotSupported ||
									(bLoopAll && siResult == eDSNoErr));
								i++ )
						{
							authAuthorityStr = authAuthorityList.GetValueAtIndex( i );
							siResult2 = dsParseAuthAuthority( authAuthorityStr, &aaVersion, &aaTag, &aaData );
							
							// JT need to check version
							if (siResult2 != eDSNoErr)
							{
								siResult = eDSAuthFailed;
								//KW do we want to bail if one of the writes failed?
								//could end up with mismatched passwords/hashes regardless of how we handle this
								//note "continue" here instead of "break" would have same effect because of
								//check for siResult in "while" above
								break;
							}
							
							// if the tag is ;DisabledUser; then we need all of the data section
							if ( aaTag != NULL && strcasecmp(aaTag, kDSTagAuthAuthorityDisabledUser) == 0 )
							{
								DSFreeString( aaData );
								aaData = strdup( authAuthorityStr + strlen(aaVersion) + strlen(aaTag) + 2 );
							}
							
							AuthAuthorityHandlerProc handlerProc = CDSLocalAuthHelper::GetAuthAuthorityHandler( aaTag );
							if (handlerProc != NULL)
							{
								siResult = (handlerProc)(inData->fInNodeRef, (CDSLocalAuthParams &)inParams, &inData->fIOContinueData, 
												inData->fInAuthStepData, inData->fOutAuthStepDataResponse,
												inData->fInDirNodeAuthOnlyFlag, bIsSecondary, authAuthorityList, guidCStr,
												authedUserIsAdmin, mutableRecordDict, mHashList, this, node,
												authedUserName,
												uid, effectiveUID, nativeRecType );
								
								if (siResult == eDSNoErr)
								{
									if (inData->fIOContinueData != 0) {
										continueDataDict = (CFMutableDictionaryRef)gLocalContinueTable->GetPointer(inData->fIOContinueData);
									}

									if (continueDataDict != NULL)
									{
										// we are supposed to return continue data
										// remember the proc we used
										CFStringRef aaTagString = CFStringCreateWithCString( NULL, aaTag, kCFStringEncodingUTF8 );
										CFDictionarySetValue( continueDataDict, CFSTR(kAuthContinueDataHandlerTag), aaTagString );
										CFRelease( aaTagString );
										
										CFStringRef aaCFString = CFStringCreateWithCString( NULL, authAuthorityStr, kCFStringEncodingUTF8 );
										CFDictionarySetValue( continueDataDict, CFSTR(kAuthContinueDataAuthAuthority), aaCFString );
										CFRelease( aaCFString );
										
										CFDictionarySetValue( continueDataDict, CFSTR(kAuthContinueDataMutableRecordDict), mutableRecordDict );
										
										this->AddContinueData( inData->fInNodeRef, continueDataDict, &inData->fIOContinueData );
										break;
									}
									else
									{
										bIsSecondary = true;
									}
								}
								
							} // if (handlerProc != NULL)
							else if ( !bIsSecondary )
							{
								siResult = eDSAuthMethodNotSupported;
							}
							
							DSFreeString( aaVersion );
							DSFreeString( aaTag );
							DSFreeString( aaData );
							DSFreeString( authAuthorityStr );
						} //for
						
						if ( bLoopAll && bIsSecondary && siResult == eDSAuthMethodNotSupported )
							siResult = eDSNoErr;
						
						// kAuthSetCertificateHashAsRoot isn't handled by all auth methods, so let it
						// proceed so the cert can get set as auth authority.
						if ( siResult == eDSAuthMethodNotSupported && authMethod == kAuthSetCertificateHashAsRoot )
						{
							siResult = eDSNoErr;
						}

						DSFreeString( aaVersion );
						DSFreeString( aaTag );
						DSFreeString( aaData );
					}
					else
					{
						// revert to basic
						siResult = CDSLocalAuthHelper::DoBasicAuth( inData->fInNodeRef, (CDSLocalAuthParams &)inParams, 
										&inData->fIOContinueData, inData->fInAuthStepData, inData->fOutAuthStepDataResponse,
										inData->fInDirNodeAuthOnlyFlag, false, authAuthorityList, NULL, authedUserIsAdmin,
										mutableRecordDict, mHashList, this, node, authedUserName, uid, effectiveUID, nativeRecType );
						if (continueDataDict != NULL && siResult == eDSNoErr)
						{
							// we are supposed to return continue data
							// remember the proc we used
							CFDictionaryAddValue( continueDataDict, CFSTR(kAuthContinueDataHandlerTag), CFSTR(kDSTagAuthAuthorityBasic) );
						}
						
						// basic auth doesn't handle setting a certificate.  allow anyway so that the cert
						// gets set as auth authority.
						if ( siResult == eDSAuthMethodNotSupported && authMethod == kAuthSetCertificateHashAsRoot )
						{
							siResult = eDSNoErr;
						}
					}
					
					if ( settingPolicy && siResult == eDSNoErr )
					{
						// now that the admin is authorized, set the global policy
						authMethod = inParams.uiAuthMethod = settingPolicy;
						DSFreeString( inParams.mAuthMethodStr );
						inParams.mAuthMethodStr = origAuthMethodStr;
						origAuthMethodStr = NULL;
						
						// transfer ownership of the auth method constant
						dsDataBufferDeallocatePriv( inData->fInAuthMethod );
						inData->fInAuthMethod = origAuthMethod;
						origAuthMethod = NULL;
						
						inData->fInDirNodeAuthOnlyFlag = origAuthMethodAuthOnly;
						if ( origAuthStepData != NULL ) {
							dsDataBufferDeallocatePriv( inData->fInAuthStepData );
							inData->fInAuthStepData = origAuthStepData;
							origAuthStepData = NULL;
						}
						
						// get the user GUID
						DSFreeString( userNameCStr );
						siResult = (tDirStatus)GetUserNameFromAuthBuffer( inData->fInAuthStepData,
										(authMethod == kAuthSetPolicy) ? 3 : 1, &userNameCStr );
						if ( siResult != eDSNoErr )
							throw( siResult );
						
						// only root can set root
						if ( strcmp(userNameCStr, "root") == 0 && effectiveUID != 0 &&
							 (authedUserName == NULL || CFStringCompare(authedUserName, CFSTR("root"), 0) != kCFCompareEqualTo) )
							throw( eDSPermissionError );
						
						DSCFRelease( userName );
						userName = CFStringCreateWithCString( NULL, userNameCStr, kCFStringEncodingUTF8 );
				
						DSCFRelease( mutableRecordDict );
						siResult = this->GetRetainedRecordDict( userName, nativeRecType, node, &mutableRecordDict );
						if ( siResult != eDSNoErr )
							throw( siResult );
				
						guids = (CFArrayRef)CFDictionaryGetValue( mutableRecordDict,
									this->AttrNativeTypeForStandardType( CFSTR( kDS1AttrGeneratedUID ) ) );
						guid = NULL;
						if ( CFArrayGetCount( guids ) > 0 )
							guid = (CFStringRef)CFArrayGetValueAtIndex( guids, 0 );
						
						guidCStr = CStrFromCFString( guid, &cStr, &cStrSize, NULL );
						
						siResult = CDSLocalAuthHelper::DoShadowHashAuth(inData->fInNodeRef, (CDSLocalAuthParams &)inParams,
										&inData->fIOContinueData, inData->fInAuthStepData, inData->fOutAuthStepDataResponse,
										inData->fInDirNodeAuthOnlyFlag, false, authAuthorityList, guidCStr, authedUserIsAdmin,
										mutableRecordDict, mHashList, this, node, authedUserName, uid, effectiveUID, nativeRecType );
					}
				} // //everything but AuthRef method
			}
		} //( inData->fIOContinueData == NULL )
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::DoAuthentication(): got error of %d", err );
		siResult = err;
	}
	
	DSCFRelease( authedUserName );
	DSFreeString( anAuthenticatorNameCStr );
	DSFreePassword( anAuthenticatorPasswordCStr );
	DSFreeString( userNameCStr );
	DSCFRelease( userName );
	DSCFRelease( mutableRecordDict );
	DSCFRelease( nodeDict );
	DSFree( cStr );
	DSFree( cStr2 );
	DSFreeString( origAuthMethodStr );
	DSFreeString( authAuthorityStr );
	
	if ( origAuthMethod != NULL )
		dsDataBufferDeallocatePriv( origAuthMethod );
	
	inData->fResult = siResult;

	return siResult;
} // DoAuthentication

bool CDSLocalPlugin::BufferFirstTwoItemsEmpty( tDataBufferPtr inAuthBuffer )
{
	char *user = NULL;
	char *password = NULL;
	unsigned int itemCount = 0;
	tDirStatus siResult = eDSNoErr;
	bool result = false;
	
	// note: Get2FromBuffer returns eDSInvalidBuffFormat if the user name is < 1 character
	siResult = (tDirStatus) Get2FromBuffer( inAuthBuffer, NULL, &user, &password, &itemCount );
	if ( itemCount >= 2 && user != NULL && user[0] == '\0' && password != NULL && password[0] == '\0' )
		result = true;
	
	DSFreeString( user );
	DSFreePassword( password );

	return result;
}

void CDSLocalPlugin::AuthenticateRoot( CFMutableDictionaryRef nodeDict, bool *inAuthedUserIsAdmin, CFStringRef *inOutAuthedUserName )
{
	*inAuthedUserIsAdmin = true;
	*inOutAuthedUserName = CFSTR("root");
	CFDictionarySetValue( nodeDict, CFSTR(kNodeAuthenticatedUserName), CFSTR("root") );
}


tDirStatus CDSLocalPlugin::DoPlugInCustomCall( sDoPlugInCustomCall* inData )
{
	tDirStatus siResult = eDSNoErr;
	
	switch( inData->fInRequestCode )
	{
		case kCustomCallFlushRecordCache:
		{
			DbgLog( kLogDebug, "CDSLocalPlugin::DoPlugInCustomCall(): flushing record cache for nodeRef %d", inData->fInNodeRef );
			CDSLocalPluginNode* node = this->NodeObjectForNodeRef( inData->fInNodeRef );
			if ( node != NULL )
				node->FlushRecordCache();
			break;
		}
	}

	return siResult;
}


tDirStatus CDSLocalPlugin::HandleNetworkTransition( sHeader* inData )
{
	return eDSNoErr;
}

#pragma mark -
#pragma mark Internal Use Methods

void
CDSLocalPlugin::FreeExternalNodesInDict( CFMutableDictionaryRef inNodeDict )
{
	tDirNodeReference aNodeRef = 0;
	
	// LDAP node
	CFNumberRef aNodeRefNumber = (CFNumberRef)CFDictionaryGetValue( inNodeDict, CFSTR(kNodeLDAPNodeRef) );
	if( aNodeRefNumber != NULL )
		CFNumberGetValue( aNodeRefNumber, kCFNumberIntType, &aNodeRef );
	if ( aNodeRef != 0 )
	{
		dsCloseDirNode( aNodeRef );
		CFDictionaryRemoveValue( inNodeDict, CFSTR(kNodeLDAPNodeRef) );
	}
	
	// Password Server node
	aNodeRefNumber = (CFNumberRef)CFDictionaryGetValue( inNodeDict, CFSTR(kNodePWSNodeRef) );
	if( aNodeRefNumber != NULL )
		CFNumberGetValue( aNodeRefNumber, kCFNumberIntType, &aNodeRef );
	if ( aNodeRef != 0 )
	{
		dsCloseDirNode( aNodeRef );
		CFDictionaryRemoveValue( inNodeDict, CFSTR(kNodePWSNodeRef) );
	}
}


bool CDSLocalPlugin::CreateLocalDefaultNodeDirectory( void )
{
	int			siResult			= eDSNoErr;
    struct stat statResult;
	
	DbgLog( kLogDebug, "CDSLocalPlugin: Checking for Default node Path %s", kDBLocalDefaultPath );
	
	//see if the whole path exists
	//if not then make sure the directories exist or create them
	siResult = ::stat( kDBLocalDefaultPath, &statResult );
	
	//if whole path does not exist
	if (siResult != eDSNoErr)
	{
		//move down the path from the system defined local directory and check if it exists
		//if not create it
		siResult = ::stat( kDBPath, &statResult );
		
		//if first sub directory does not exist
		if (siResult != eDSNoErr)
		{
			::mkdir( kDBPath, 0775 );
			::chmod( kDBPath, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
			DbgLog( kLogDebug, "CDSLocalPlugin: Created Local node path", kDBPath );
		}
		
		//next subdirectory
		siResult = ::stat( kDBNodesPath, &statResult );
		//if second sub directory does not exist
		if (siResult != eDSNoErr)
		{
			::mkdir( kDBNodesPath, 0775 );
			::chmod( kDBNodesPath, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
			DbgLog( kLogDebug, "CDSLocalPlugin: Created Local node path", kDBNodesPath );
		}
		::mkdir( kDBLocalDefaultPath, 0775 );
		::chmod( kDBLocalDefaultPath, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
		DbgLog( kLogDebug, "CDSLocalPlugin: Created Local node path", kDBLocalDefaultPath );
	}
	
	return (siResult == eDSNoErr);
	
} //CreateLocalDefaultNodeDirectory

CFMutableArrayRef CDSLocalPlugin::FindSubDirsInDirectory( const char* inDir )
{
	int					siResult	= eDSNoErr;
    struct stat statResult;
	DIR				   *nodesDir	= NULL;
	dirent			   *theDirEnt	= NULL;
	CFMutableArrayRef	cfSubDirs	= NULL;
	
	if (inDir == NULL)
		return NULL;

	DbgLog( kLogDebug, "CDSLocalPlugin: Checking for sub dirs in the Directory %s", inDir );
	
	//see if the dir path exists
	siResult = ::stat( inDir, &statResult );
	
	//if whole path does exist
	if (siResult == eDSNoErr)
	{
		cfSubDirs = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		nodesDir = opendir( inDir );
		if (nodesDir != NULL)
		{
			while( ( theDirEnt = readdir( nodesDir ) ) != NULL )
			{
				if (theDirEnt->d_name[0] != '\0')
				{
					if (strcmp(".",theDirEnt->d_name) && strcmp("..",theDirEnt->d_name) )
					{
						CFStringRef aDirStr = CFStringCreateWithCString( NULL, theDirEnt->d_name, kCFStringEncodingUTF8 );
						CFArrayAppendValue( cfSubDirs, aDirStr );
						DSCFRelease(aDirStr);
					}
				}
			}
		}
	}
	
	if ( nodesDir != NULL )
	{
		::closedir( nodesDir );
		nodesDir = NULL;
	}

	return (cfSubDirs);
	
} //FindSubDirsInDirectory

const char*	CDSLocalPlugin::GetProtocolPrefixString()
{		
    return gProtocolPrefixString;
}

bool CDSLocalPlugin::LoadMappings()
{
	// TODO: Question is whether we allow for different local nodes to have different mapping files
	CFTypeRef		paths[] = {
		CFSTR("/var/db/dslocal/dsmappings/AttributeMappings.plist"), 
		CFSTR("/var/db/dslocal/dsmappings/RecordMappings.plist"),
#ifdef ACL_SUPPORT
		CFSTR("/var/db/dslocal/dsmappings/permissions.plist"),
#endif
	};
	
	CFArrayRef mappingsFilePaths = CFArrayCreate( kCFAllocatorDefault, paths, sizeof(paths) / sizeof(CFStringRef), &kCFTypeArrayCallBacks );
	CFDictionaryRef mappingsDictsDict = this->CreateDictionariesFromFiles( mappingsFilePaths );
	if (mappingsDictsDict != NULL)
	{
		DSCFRelease( mappingsFilePaths );
		
		mAttrStdToNativeMappings = (CFDictionaryRef) CFDictionaryGetValue( mappingsDictsDict, paths[0] );
		mRecStdToNativeMappings = (CFDictionaryRef) CFDictionaryGetValue( mappingsDictsDict, paths[1] );
#ifdef ACL_SUPPORT
		mPermissions = (CFDictionaryRef) CFDictionaryGetValue( mappingsDictsDict, paths[2] );
#endif
	}

	// if one of them is bad ignore them and read defaults
	if ( mAttrStdToNativeMappings == NULL || mRecStdToNativeMappings == NULL
#ifdef ACL_SUPPORT
		 || mPermissions == NULL
#endif
		)
	{
		// fallback to internal defaults if file is unreadable or was corrupted
		CFTypeRef		defaultFiles[] = {
			CFSTR("/System/Library/DirectoryServices/DefaultLocalDB/dsmappings/AttributeMappings.plist"), 
			CFSTR("/System/Library/DirectoryServices/DefaultLocalDB/dsmappings/RecordMappings.plist"),
#ifdef ACL_SUPPORT
			CFSTR("/System/Library/DirectoryServices/DefaultLocalDB/dsmappings/permissions.plist"),
#endif
		};
		
		mappingsFilePaths = CFArrayCreate( kCFAllocatorDefault, defaultFiles, sizeof(defaultFiles) / sizeof(CFStringRef), &kCFTypeArrayCallBacks );
		mappingsDictsDict = this->CreateDictionariesFromFiles( mappingsFilePaths );
		if ( mappingsDictsDict != NULL )
		{
			DSCFRelease(mappingsFilePaths);
			
			mAttrStdToNativeMappings = (CFDictionaryRef) CFDictionaryGetValue( mappingsDictsDict, paths[0] );
			mRecStdToNativeMappings = (CFDictionaryRef) CFDictionaryGetValue( mappingsDictsDict, paths[1] );
#ifdef ACL_SUPPORT
			mPermissions = (CFDictionaryRef) CFDictionaryGetValue( mappingsDictsDict, paths[2] );
#endif
		}
	}

#ifdef ACL_SUPPORT
	assert( mAttrStdToNativeMappings != NULL && mRecStdToNativeMappings != NULL && mPermissions != NULL );
#else
	assert( mAttrStdToNativeMappings != NULL && mRecStdToNativeMappings != NULL );
#endif
	
	CFRetain( mAttrStdToNativeMappings );
	CFRetain( mRecStdToNativeMappings );
#ifdef ACL_SUPPORT
	CFRetain( mPermissions );
#endif
	
	DSCFRelease( mappingsDictsDict );
	
	// now build the reverse mapping tables
	CFIndex numKeys = 0;
	const void** keys = NULL;
	CFIndex attrsSize = 0;
	const void** values = NULL;
	CFIndex recsSize = 0;

	{	// attribute types
		numKeys = CFDictionaryGetCount( mAttrStdToNativeMappings );
		attrsSize = numKeys * sizeof( void* ); //assume one to one mappings
		keys = (const void**)::malloc( attrsSize );
		values = (const void**)::malloc( attrsSize );
		CFDictionaryGetKeysAndValues( mAttrStdToNativeMappings, keys, values );
		
		mAttrNativeToStdMappings = CFDictionaryCreate( NULL, values, keys, numKeys,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

		CFDictionaryGetKeysAndValues( mAttrNativeToStdMappings, keys, values );

		CFMutableDictionaryRef mutableAttrPrefixedNativeToNativeMappings = CFDictionaryCreateMutable( NULL,
			numKeys, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		for ( CFIndex i=0; i<numKeys; i++ )
		{
			CFStringRef prefixedNativeAttr = CFStringCreateWithFormat( NULL, NULL, CFSTR( "%@%@" ),
				CFSTR( kDSNativeAttrTypePrefix ), (CFStringRef)keys[i] );
			CFDictionaryAddValue( mutableAttrPrefixedNativeToNativeMappings, prefixedNativeAttr, keys[i] );
			CFRelease( prefixedNativeAttr );
		}
		
		mAttrPrefixedNativeToNativeMappings = mutableAttrPrefixedNativeToNativeMappings;
	}
	
	{ // record types
		numKeys = CFDictionaryGetCount( mRecStdToNativeMappings );
		recsSize = numKeys * sizeof( void* ); //assume one to one mappings
		//try to make use of the existing void** arrays if they are large enough?
		if ( recsSize > attrsSize )
		{
			if ( keys != NULL )
				free( keys );
			
			keys = (const void**)::malloc( recsSize );

			if ( values != NULL )
				free( values );
			
			values = (const void**)::malloc( recsSize );
		}
		CFDictionaryGetKeysAndValues( mRecStdToNativeMappings, keys, values );
		
		mRecNativeToStdMappings = CFDictionaryCreate( NULL, values, keys, numKeys,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

		CFDictionaryGetKeysAndValues( mRecNativeToStdMappings, keys, values );

		CFMutableDictionaryRef mutableRecPrefixedNativeToNativeMappings = CFDictionaryCreateMutable( NULL,
			numKeys, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		for ( CFIndex i=0; i<numKeys; i++ )
		{
			CFStringRef prefixedNativeRec = CFStringCreateWithFormat( NULL, NULL, CFSTR( "%@%@" ),
				CFSTR( kDSNativeRecordTypePrefix ), (CFStringRef)keys[i] );
			CFDictionaryAddValue( mutableRecPrefixedNativeToNativeMappings, prefixedNativeRec, keys[i] );
			CFRelease( prefixedNativeRec );
		}
		
		mRecPrefixedNativeToNativeMappings = mutableRecPrefixedNativeToNativeMappings;
	}
	
	if ( keys != NULL )
		free( keys );
	if ( values != NULL )
		free( values );

	return true;
}

void CDSLocalPlugin::LoadSettings()
{
	CFStringRef dsPrefsPath = CFSTR( "/Library/Preferences/DirectoryService/DirectoryService.plist" );
	CFStringRef dslocalPrefsPath = CFSTR( "/Library/Preferences/DirectoryService/DSLocalPlugInConfig.plist" );
	CFStringRef prefsPaths[] = { dsPrefsPath, dslocalPrefsPath };
	CFArrayRef filePaths = CFArrayCreate( NULL, (const void **)prefsPaths, 2, &kCFTypeArrayCallBacks );
	CFDictionaryRef prefsDict = this->CreateDictionariesFromFiles( filePaths );
	CFRelease( filePaths );
	if ( prefsDict != NULL )
	{
		CFDictionaryRef dsPrefsDict = (CFDictionaryRef)CFDictionaryGetValue( prefsDict, dsPrefsPath );
		if ( dsPrefsDict != NULL )
		{
			CFNumberRef prefNum = (CFNumberRef)CFDictionaryGetValue( dsPrefsDict,
				CFSTR( "Delay Failed Local Auth Returns Delta In Seconds" ) );
			if ( prefNum != NULL )
				CFNumberGetValue( prefNum, kCFNumberIntType, &mDelayFailedLocalAuthReturnsDeltaInSeconds );
		}
	}
	
	if ( prefsDict != NULL )
		CFRelease( prefsDict );
}

tDirStatus CDSLocalPlugin::PackRecordsIntoBuffer(	CFDictionaryRef		inNodeDict,
													UInt32				inFirstRecordToReturnIndex,
													CFArrayRef			inRecordsArray,
													CFArrayRef			inDesiredAttributes,
													tDataBufferPtr		inBuff,
													bool				inAttrInfoOnly,
													UInt32				*outNumRecordsPacked )
{
	tDirStatus			siResult					= eDSNoErr;
	CBuff			   *buff						= NULL;
	CDataBuff		   *recData						= NULL;
	CDataBuff		   *attrData					= NULL;
	CDataBuff		   *tmpData						= NULL;
	char			   *allocatedCStr				= NULL;
	size_t				allocatedCStrSize			= 0;
	const char		   *cStrPtr						= NULL;
	bool				cStrAllocated				= false;
	const void		  **keys						= NULL;
	CFIndex				keysSize					= 0;
	const void		  **values						= NULL;
	CFIndex				valuesSize					= 0;

	bool allStdAttrsRequested		= false;
	bool allNativeAttrsRequested	= false;

	if ( inRecordsArray == NULL || inDesiredAttributes == NULL )
	{
		*outNumRecordsPacked = 0;
		return eDSNoErr;
	}
	
	CDSLocalPluginNode* node = NodeObjectFromNodeDict( inNodeDict );
	
	CFRange rangeOfDesiredAttributes = CFRangeMake( 0, CFArrayGetCount( inDesiredAttributes ) );
	if ( CFArrayGetFirstIndexOfValue( inDesiredAttributes, rangeOfDesiredAttributes, CFSTR( kDSAttributesAll ) ) != kCFNotFound )
	{
		allStdAttrsRequested	= true;
		allNativeAttrsRequested	= true;
	}
	if ( CFArrayGetFirstIndexOfValue( inDesiredAttributes, rangeOfDesiredAttributes, CFSTR( kDSAttributesStandardAll ) ) != kCFNotFound )
	{
		allStdAttrsRequested = true;
	}
	if ( CFArrayGetFirstIndexOfValue( inDesiredAttributes, rangeOfDesiredAttributes, CFSTR( kDSAttributesNativeAll ) ) != kCFNotFound )
	{
		allNativeAttrsRequested = true;
	}

	try
	{
		// set up the buffer
		buff = new CBuff();
		if ( buff == NULL ) throw( eMemoryAllocError );
		siResult = (tDirStatus)buff->Initialize( inBuff, true );
		if ( siResult != eDSNoErr ) throw ( siResult );
		siResult = (tDirStatus)buff->GetBuffStatus();
		if ( siResult != eDSNoErr ) throw ( siResult );
		siResult = (tDirStatus)buff->SetBuffType( 'StdA' );
		if ( siResult != eDSNoErr ) throw ( siResult );

		UInt32 numRecordsPacked = 0;
		CFIndex numRecordsInArray = (UInt32)CFArrayGetCount( inRecordsArray );
		for ( CFIndex i=(CFIndex)inFirstRecordToReturnIndex; i < numRecordsInArray; i++ )
		{
			CFDictionaryRef recordDict = (CFDictionaryRef)CFArrayGetValueAtIndex( inRecordsArray, i );
			CFStringRef nativeRecType = NULL;
			if ( recordDict == NULL )
			{
				DbgLog( kLogDebug, "CDSLocalPlugin::PackRecordsIntoBuffer(): recordDict for a found record came back NULL!" );
				continue;
			}
			
			if ( recData == NULL )
			{
				recData = new CDataBuff();
			}
			else
			{
				recData->Clear();
			}

// when we encounter a data problem with the record dict
//extract records that we can and skip bad records ie. we use continue and NOT throw
			
			bool bIsAutomount = false;

			{	// record type for this record
				CFArrayRef recTypeArray = (CFArrayRef)CFDictionaryGetValue( recordDict, CFSTR( kDSNAttrRecordType ) );
				if ( recTypeArray == NULL || CFArrayGetCount( recTypeArray ) == 0 ) continue;
				CFStringRef stdRecTypeCFStr = (CFStringRef)CFArrayGetValueAtIndex( recTypeArray, 0 );
				if (stdRecTypeCFStr == NULL) continue;
				cStrPtr = CStrFromCFString( stdRecTypeCFStr, &allocatedCStr, &allocatedCStrSize, &cStrAllocated );
				
				nativeRecType = RecordNativeTypeForStandardType( stdRecTypeCFStr );

				recData->AppendShort( ::strlen( cStrPtr ) );
				recData->AppendString( cStrPtr );
				
				bIsAutomount = ( strcmp(cStrPtr, kDSStdRecordTypeAutomount) == 0 );
			}
			
			{ // add the record name to the record data block
				CFStringRef recordName = NULL;
				CFArrayRef recordNames = (CFArrayRef)CFDictionaryGetValue( recordDict, this->AttrNativeTypeForStandardType( CFSTR(kDSNAttrRecordName) ) );
				if ( recordNames == NULL || CFArrayGetCount( recordNames ) == 0 ) continue;
				recordName = (CFStringRef)CFArrayGetValueAtIndex( recordNames, 0 );
				if ( recordName == NULL ) continue;
				cStrPtr = CStrFromCFString( recordName, &allocatedCStr, &allocatedCStrSize, &cStrAllocated );
				
				recData->AppendShort( ::strlen( cStrPtr ) );
				recData->AppendString( cStrPtr );
			}
			
// TODO: KW should only need keys and values for ALL retrievals
//This is a dict and we can use direct access to the key from the inDesiredAttributes values

// add the attributes to the record data block

			CFIndex numKeys = CFDictionaryGetCount( recordDict );
			{ 
				if ( allNativeAttrsRequested || allStdAttrsRequested )
				{
					// build the keys and values ONLY if requesting All of native or Std
					// make sure our keys and values buffers are big enough to hold the keys and values
					if ( numKeys * (CFIndex)sizeof( void* ) > keysSize )
					{
						DSFree( keys );		//do not release the keys themselves since obtained from Get
						
						keys = (const void**)::malloc( numKeys * sizeof( void* ) );
						keysSize = numKeys * sizeof( void* );
					}
					if ( numKeys * (CFIndex)sizeof( void* ) > valuesSize )
					{
						DSFree( values );	//do not release the values themselves since obtained from Get
						
						values = (const void**)::malloc( numKeys * sizeof( void* ) );
						valuesSize = numKeys * sizeof( void* );
					}
					
					CFDictionaryGetKeysAndValues( recordDict, keys, values );
				}

				CFIndex numAttrsAdd = 0;

				if ( attrData == NULL )
					attrData = new CDataBuff();
				else
					attrData->Clear();
				
				if ( allNativeAttrsRequested || allStdAttrsRequested )
				{
					CFStringRef nativeAttrName	= NULL;
					CFStringRef stdAttrName		= NULL;
					// go through and add each attribute and its value
					for ( CFIndex j = 0; j < numKeys; j++ )
					{
						nativeAttrName	= ( allNativeAttrsRequested ? this->AttrPrefixedNativeTypeForNativeType( (CFStringRef)keys[j] ) : NULL );
						stdAttrName		= ( allStdAttrsRequested ? this->AttrStandardTypeForNativeType( (CFStringRef)keys[j] ) : NULL );

						bool packThisNativeAttr	= ( nativeAttrName != NULL );
						bool packThisStdAttr	= ( stdAttrName != NULL );
						if ( !packThisNativeAttr && !packThisStdAttr )
							continue;
						
						// if this maps to a standard attribute, don't return the native
						if ( packThisStdAttr == true ) packThisNativeAttr = false;
						
						if ( node->AccessAllowed(inNodeDict, nativeRecType, AttrNativeTypeForStandardType((CFStringRef) keys[j]), eDSAccessModeReadAttr) == false ) 
							continue;

						for ( CFIndex m = ( packThisNativeAttr ? 0 : 1 ); m < ( packThisStdAttr ? 2 : 1 ); m++ )
						{
							if ( tmpData == NULL )
								tmpData = new CDataBuff();
							else
								tmpData->Clear();

							CFStringRef attrName = ( m == 0 ? nativeAttrName : stdAttrName );
							cStrPtr = CStrFromCFString( attrName, &allocatedCStr, &allocatedCStrSize, &cStrAllocated );
							tmpData->AppendShort( ::strlen( cStrPtr ) );
							tmpData->AppendString( cStrPtr );

							numAttrsAdd++;

							// attribute values
							if ( inAttrInfoOnly )
								tmpData->AppendShort( 0 );
							else
							{
								CFArrayRef attrValues = (CFArrayRef)values[j];
								if ( attrValues == NULL )
								{
									throw( ePlugInDataError ); //KW do we really want to throw here?
								}
								CFIndex numAttrValues = CFArrayGetCount( attrValues );
								tmpData->AppendShort( numAttrValues );
								
								for ( CFIndex k = 0; k < numAttrValues; k++ )
								{
									CFTypeID attrTypeID = CFGetTypeID( CFArrayGetValueAtIndex( attrValues, k ) );
									if (  attrTypeID == CFStringGetTypeID() )
									{
										CFStringRef value = (CFStringRef)CFArrayGetValueAtIndex( attrValues, k );
										if ( value == NULL )
										{
											throw( ePlugInDataError ); //KW do we really want to throw here?
										}
										cStrPtr = CStrFromCFString( value, &allocatedCStr, &allocatedCStrSize, &cStrAllocated );
										
										// we handle automount names special because we have to strip the map name from it
										char *separator;
										char tempName[1024];
										if ( bIsAutomount && stdAttrName &&
											 CFStringCompare(stdAttrName, CFSTR(kDSNAttrRecordName), 0) == kCFCompareEqualTo &&
											 (separator = strstr(cStrPtr, ",automountMapName=")) != NULL )
										{
											// add one so it has room for the NULL
											strlcpy( tempName, cStrPtr, (separator - cStrPtr) + 1 );
											cStrPtr = tempName;
										}

										tmpData->AppendLong( ::strlen( cStrPtr ) );
										tmpData->AppendString( cStrPtr );
									}
									else if ( attrTypeID == CFDataGetTypeID() ) // support binary data
									{
										CFDataRef value = (CFDataRef)CFArrayGetValueAtIndex( attrValues, k );
										if ( value == NULL )
										{
											throw( ePlugInDataError ); //KW do we really want to throw here?
										}
										CFIndex valueLength = CFDataGetLength( value );
										const UInt8 *valueData = CFDataGetBytePtr( value );
										tmpData->AppendLong( valueLength );
										tmpData->AppendBlock( valueData, valueLength );
									}
									else
									{
										throw(eDSAttributeValueNotFound);
									}
								}
							}
							attrData->AppendLong( tmpData->GetLength() );
							attrData->AppendBlock( tmpData->GetData(), tmpData->GetLength() );
						}
					}
				}
				else //get the attributes as requested in inDesiredAttributes
				{
					CFStringRef requestedAttrName	= NULL;
					CFStringRef nativeAttrName		= NULL;
					// go through and add each attribute and its value
					for ( CFIndex j=0; j < CFArrayGetCount(inDesiredAttributes); j++ )
					{
						requestedAttrName	= (CFStringRef)CFArrayGetValueAtIndex(inDesiredAttributes, j);
						if ( requestedAttrName == NULL ) continue;
						nativeAttrName	= this->AttrNativeTypeForStandardType( requestedAttrName );
						if ( nativeAttrName == NULL ) continue;
						
						if ( node->AccessAllowed(inNodeDict, nativeRecType, nativeAttrName, eDSAccessModeReadAttr) == false ) 
							continue;
						
						CFArrayRef attrValues = (CFArrayRef)CFDictionaryGetValue(recordDict, nativeAttrName);
						if ( attrValues != NULL )
						{
							if ( tmpData == NULL )
								tmpData = new CDataBuff();
							else
								tmpData->Clear();

							cStrPtr = CStrFromCFString( requestedAttrName, &allocatedCStr, &allocatedCStrSize, &cStrAllocated );
							tmpData->AppendShort( ::strlen( cStrPtr ) );
							tmpData->AppendString( cStrPtr );

							numAttrsAdd++;

							// attribute values
							if ( inAttrInfoOnly )
								tmpData->AppendShort( 0 );
							else
							{
								CFIndex numAttrValues = CFArrayGetCount( attrValues );
								tmpData->AppendShort( numAttrValues );
								
								for ( CFIndex k=0; k < numAttrValues; k++ )
								{
									CFTypeID attrTypeID = CFGetTypeID( CFArrayGetValueAtIndex( attrValues, k ) );
									if (  attrTypeID == CFStringGetTypeID() )
									{
										CFStringRef value = (CFStringRef)CFArrayGetValueAtIndex( attrValues, k );
										if ( value == NULL ) throw( ePlugInDataError ); //KW do we really want to throw here?
 										cStrPtr = CStrFromCFString( value, &allocatedCStr, &allocatedCStrSize, &cStrAllocated );
										tmpData->AppendLong( ::strlen( cStrPtr ) );
										tmpData->AppendString( cStrPtr );
									}
									else if ( attrTypeID == CFDataGetTypeID() ) // support binary data
									{
										CFDataRef value = (CFDataRef)CFArrayGetValueAtIndex( attrValues, k );
										if ( value == NULL ) throw( ePlugInDataError ); //KW do we really want to throw here?
 										CFIndex valueLength = CFDataGetLength( value );
										const UInt8 *valueData = CFDataGetBytePtr( value );
										if ( valueData != NULL )
										{
											tmpData->AppendLong( valueLength );
											tmpData->AppendBlock( valueData, valueLength );
										}
									}
									else
									{
										throw(eDSAttributeValueNotFound);
									}
								}
							}
							attrData->AppendLong( tmpData->GetLength() );
							attrData->AppendBlock( tmpData->GetData(), tmpData->GetLength() );
						}
					}
				}
				
				recData->AppendShort( numAttrsAdd );
				if ( numAttrsAdd > 0 )
					recData->AppendBlock( attrData->GetData(), attrData->GetLength() );
			}
			
			SInt32 result = buff->AddData( recData->GetData(), recData->GetLength() );
			if ( result == CBuff::kBuffFull )
			{
				if ( numRecordsPacked == 0 )
					throw( eDSBufferTooSmall );
				break;
			}
			else if ( result == eDSNoErr )
				numRecordsPacked++;
			else
				throw( (tDirStatus)result );
			buff->SetLengthToSize();
		}
	
		if ( outNumRecordsPacked != NULL )
			*outNumRecordsPacked = numRecordsPacked;
		else
			throw( eDSNullParameter );
	}
	catch( tDirStatus err )
    {
		DbgLog( kLogPlugin, "CDSLocalPlugin::PackRecordsIntoBuffer(): Got error %d", err );
        siResult = err;
	}

	if ( allocatedCStr != NULL )
	{
		free( allocatedCStr );
		allocatedCStr = NULL;
	}

	DSFree( keys );		//do not release the keys themselves since obtained from Get
	DSFree( values );	//do not release the values themselves since obtained from Get
	DSDelete(recData);
	DSDelete(attrData);
	DSDelete(tmpData);
	DSDelete(buff);

	return siResult;
}

void CDSLocalPlugin::AddRecordTypeToRecords( CFStringRef inStdType, CFArrayRef inRecordsArray )
{
	CFIndex numRecords = CFArrayGetCount( inRecordsArray );
	for ( CFIndex i=0; i<numRecords; i++ )
	{
		CFMutableDictionaryRef mutableRecordDict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( inRecordsArray, i );
		if ( mutableRecordDict == NULL )
		{
			DbgLog( kLogPlugin, "CDSLocalPlugin::AddRecordTypeToRecords(): record at index %d came back NULL!", i );
			continue;
		}
		
		if ( CFDictionaryGetCountOfKey( mutableRecordDict,  CFSTR( kDSNAttrRecordType ) ) == 0 )
		{
			CFArrayRef recordTypeValues = CFArrayCreate( NULL, (const void**)&inStdType, 1, &kCFTypeArrayCallBacks );
			CFDictionaryAddValue( mutableRecordDict, CFSTR( kDSNAttrRecordType ), recordTypeValues );
			CFRelease( recordTypeValues );
		}
	}
}

tDirStatus CDSLocalPlugin::AddMetanodeToRecords( CFArrayRef inRecords, CFStringRef inNode )
{
	CFIndex numRecords = CFArrayGetCount( inRecords );
	CFArrayRef nodePathValues = CFArrayCreate( NULL, (const void **)&inNode, 1, &kCFTypeArrayCallBacks );
	if ( nodePathValues == NULL )
		return( eMemoryAllocError );
	
	for ( CFIndex i=0; i<numRecords; i++ )
	{
		CFMutableDictionaryRef mutableRecordDict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( inRecords,i );
		if ( mutableRecordDict == NULL )
			continue;
		// we always set the metanode location regardless of the contents of the on-disk contents
		CFDictionaryAddValue( mutableRecordDict, CFSTR( kDSNAttrMetaNodeLocation ), nodePathValues );
	}
	CFRelease( nodePathValues );
	
	return eDSNoErr;
}

void CDSLocalPlugin::RemoveUndesiredAttributes( CFArrayRef inRecords, CFArrayRef inDesiredAttributes )
{
	CFRange rangeOfDesiredAttrs = CFRangeMake( 0, CFArrayGetCount( inDesiredAttributes ) );
	CFStringRef* keys = NULL;
	size_t keysSize = 0;

	CFIndex numRecords = CFArrayGetCount( inRecords );
	for ( CFIndex i=0; i<numRecords; i++ )
	{
		CFMutableDictionaryRef mutableRecordDict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( inRecords, i );
		if ( mutableRecordDict == NULL )
		{
			DbgLog( kLogPlugin, "CDSLocalPlugin::AddRecordTypeToRecords(): record at index %d came back NULL!", i );
			continue;
		}
		
		CFIndex numKeys = CFDictionaryGetCount( mutableRecordDict );
		if ( ( numKeys * sizeof( CFStringRef ) ) > keysSize )
		{
			if ( keys != NULL )
				free( keys );
			keysSize = numKeys * sizeof( CFStringRef );
			keys = (CFStringRef*)::malloc( keysSize );
			if ( keys == NULL )
				throw( eMemoryAllocError );
		}
		
		CFDictionaryGetKeysAndValues( mutableRecordDict, (const void**)keys, NULL );
		
		// find any keys that aren't in the desired attributes list and remove them
		for ( CFIndex j=0; j<numKeys; j++ )
		{
			if ( CFArrayGetFirstIndexOfValue( inDesiredAttributes, rangeOfDesiredAttrs, keys[j] ) == kCFNotFound )
				CFDictionaryRemoveValue( mutableRecordDict, keys[j] );
		}
	}
	if ( keys != NULL )
	{
		free( keys );
		keys = NULL;
	}
}

tDirStatus CDSLocalPlugin::ReadHashConfig( CDSLocalPluginNode* inNode )
{
	tDirStatus siResult = eDSNoErr;
	CFMutableArrayRef mutableRecordsArray = NULL;

	mHashList = gServerOS ? ePluginHashDefaultServerSet : ePluginHashDefaultSet;
	
	try
	{
		mutableRecordsArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		CFStringRef recName = CFSTR( "shadowhash" );
		CFArrayRef recNames = CFArrayCreate( NULL, (const void**)&recName, 1, &kCFTypeArrayCallBacks );
		siResult = inNode->GetRecords( this->RecordNativeTypeForStandardType( CFSTR( kDSStdRecordTypeConfig ) ),
			recNames, CFSTR( kDSNAttrRecordName ), eDSExact, false, 1, mutableRecordsArray );
		CFRelease( recNames );
		recNames = NULL;
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		CFDictionaryRef shadowHashRecord = NULL;
		if ( CFArrayGetCount( mutableRecordsArray ) > 0 )
			shadowHashRecord = (CFDictionaryRef)CFArrayGetValueAtIndex( mutableRecordsArray, 0 );
		if ( shadowHashRecord == NULL )
			throw( eDSRecordNotFound );
		
		CFArrayRef hashListValues = (CFArrayRef)CFDictionaryGetValue( shadowHashRecord,
			this->AttrNativeTypeForStandardType( CFSTR( "dsAttrTypeNative:optional_hash_list" ) ) );
		if ( hashListValues == NULL )
			throw( eDSAuthFailed );
		
		// The attribute is present, so switch from the server default set to a minimum set.
		// To be strict, we could start with a zero-set. However, doing so is dangerous because
		// if the system ever has an attribute with no values, that would mean storing no hashes at all.
		if ( gServerOS )
			mHashList = ePluginHashSaltedSHA1;
		
		// parse the hash strings into a bitfield
		CFIndex numHashes = CFArrayGetCount( hashListValues );
		for ( CFIndex i=0; i<numHashes; i++ )
		{
			CFStringRef hasListValue = (CFStringRef)CFArrayGetValueAtIndex( hashListValues, i );

			if ( CFStringCompare( hasListValue, CFSTR( kHashNameNT ), 0 ) == kCFCompareEqualTo )
				mHashList |= ePluginHashNT;
			else if ( CFStringCompare( hasListValue, CFSTR( kHashNameLM ), 0 ) == kCFCompareEqualTo )
				mHashList |= ePluginHashLM;
			else if ( CFStringCompare( hasListValue, CFSTR( kHashNameCRAM_MD5 ), 0 ) == kCFCompareEqualTo )
				mHashList |= ePluginHashCRAM_MD5;
			else if ( CFStringCompare( hasListValue, CFSTR( kHashNameSHA1 ), 0 ) == kCFCompareEqualTo )
				mHashList |= ePluginHashSaltedSHA1;
			else if ( CFStringCompare( hasListValue, CFSTR( kHashNameRecoverable ), 0 ) == kCFCompareEqualTo )
				mHashList |= ePluginHashRecoverable;
			else if ( CFStringCompare( hasListValue, CFSTR( kHashNameSecure ), 0 ) == kCFCompareEqualTo )
			{
				// If the secure hash is used, all other hashes are OFF
				mHashList = ePluginHashSecurityTeamFavorite;
				break;
			}
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::ReadHashConfig(): got error %d", err );
		siResult = err;
	}
	
	if ( mutableRecordsArray != NULL )
		CFRelease( mutableRecordsArray );
	
	return siResult;
}

CFDictionaryRef CDSLocalPlugin::CreateDictionariesFromFiles( CFArrayRef inFilePaths )
{
	char		   *cStr					= NULL;
	size_t			cStrSize				= 0;
	const char	   *pathCStr				= NULL;
	FILE		   *theFile					= NULL;
	CFDataRef		fileData				= NULL;
	char		   *fileContents			= NULL;
	long			fileContentsBufferSize	= 0;
	CFStringRef		errorString				= NULL;
	bool			abortDictReturn			= false;

	CFMutableDictionaryRef mutableResults = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks );
	
	CFIndex numFiles = CFArrayGetCount( inFilePaths );
	for ( CFIndex i=0; i<numFiles; i++ )
	{
		CFStringRef filePath = (CFStringRef)CFArrayGetValueAtIndex( inFilePaths, i );
		pathCStr = CStrFromCFString( filePath, &cStr, &cStrSize, NULL );
		
		theFile = ::fopen( pathCStr, "r" );
		if ( theFile == NULL )
		{
			CFDebugLog( kLogPlugin, "CDSLocalPlugin::CreateDictionariesFromFiles(): unable to open file <%@>", filePath, pathCStr ? pathCStr : "string create failed" );
			abortDictReturn = true;
			continue;
		}
		
		::fseek( theFile, 0, SEEK_END );
		long fileSize = ftell( theFile );
		::rewind( theFile );
		
		if ( fileContentsBufferSize < fileSize + 1 )
		{
			DSFree( fileContents );
			fileContentsBufferSize = fileSize * 2;
			fileContents = (char*)::malloc( fileContentsBufferSize );
			if ( fileContents == NULL ) throw( eMemoryAllocError );
		}

		fileSize = ::fread( fileContents, 1, fileSize, theFile );
		fileData = CFDataCreate( NULL, (UInt8*)fileContents, fileSize );

		CFDictionaryRef fileDict = (CFDictionaryRef)CFPropertyListCreateFromXMLData( NULL, fileData,
						kCFPropertyListImmutable, &errorString );
		if (fileDict == NULL)
		{
			CFDebugLog( kLogPlugin, "CDSLocalPlugin::CreateDictionariesFromFiles(): unable to extract plist data from file <%@>", filePath );
			abortDictReturn = true;
		}
		CFDictionaryAddValue( mutableResults, filePath, fileDict );
		
		CFRelease( fileDict );

		if ( fileData != NULL );
			CFRelease( fileData );

		::fclose( theFile );
		theFile = NULL;
	}

	if ( cStr != NULL )
		::free( cStr );
	if ( fileContents != NULL )
		::free( fileContents );
	if ( errorString != NULL )
		CFRelease( errorString );
		
	if (abortDictReturn)
	{
		DSCFRelease(mutableResults);
	}
		
	return mutableResults;
}

CFDictionaryRef CDSLocalPlugin::CreateNodeInfoDict( CFArrayRef inAttributes, CFDictionaryRef inNodeDict )
{
	CFMutableDictionaryRef	cfNodeInfo		= CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
																		&kCFTypeDictionaryValueCallBacks );
	CFMutableDictionaryRef	cfAttributes	= CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
																		&kCFTypeDictionaryValueCallBacks );
	CFRange					cfAttribRange	= CFRangeMake( 0, CFArrayGetCount(inAttributes) );
	bool					bNeedAll		= CFArrayContainsValue( inAttributes, cfAttribRange, CFSTR(kDSAttributesAll) );
	
	CFDictionarySetValue( cfNodeInfo, kBDPINameKey, CFSTR("DirectoryNodeInfo") );
	CFDictionarySetValue( cfNodeInfo, kBDPITypeKey, CFSTR(kDSStdRecordTypeDirectoryNodeInfo) );
	CFDictionarySetValue( cfNodeInfo, kBDPIAttributeKey, cfAttributes );
	
	if (bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDS1AttrDistinguishedName)))
	{
		CFStringRef cfRealName	= (CFStringRef) CFDictionaryGetValue( inNodeDict, CFSTR(kNodeNamekey) );
		CFArrayRef	cfValue		= CFArrayCreate( kCFAllocatorDefault, (const void **) &cfRealName, 1, &kCFTypeArrayCallBacks );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDS1AttrDistinguishedName), cfValue );
		
		DSCFRelease( cfValue );
	}
	
	if ( bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDSNAttrNodePath)) == true )
	{
		CFMutableArrayRef mutableArray = CFArrayCreateMutable( NULL, 2, &kCFTypeArrayCallBacks );
		
		CFStringRef prefixString = CFStringCreateWithCString( kCFAllocatorDefault, GetProtocolPrefixString(),
															  kCFStringEncodingUTF8 );
		if ( prefixString != NULL ) {
			CFArrayAppendValue( mutableArray, prefixString );
			DSCFRelease(prefixString);
		}
		
		CFStringRef nodeName = (CFStringRef)CFDictionaryGetValue( inNodeDict, CFSTR(kNodeNamekey) );
		if ( nodeName != NULL ) {
			CFArrayAppendValue( mutableArray, nodeName );
		}
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDSNAttrNodePath), mutableArray );
		DSCFRelease( mutableArray );
	}
	
	if ( bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDS1AttrReadOnlyNode)) == true )
	{
		CFStringRef	cfReadOnly	= CFSTR("ReadWrite");
		CFArrayRef	cfValue		= CFArrayCreate( kCFAllocatorDefault, (const void **) &cfReadOnly, 1, &kCFTypeArrayCallBacks );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDS1AttrReadOnlyNode), cfValue );
		
		DSCFRelease( cfValue );
	}
	
	if ( bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDSNAttrTrustInformation)) == true )
	{
		CFStringRef cfTrustValue	= CFSTR("FullTrust");
		CFArrayRef	cfValue			= CFArrayCreate( kCFAllocatorDefault, (const void **) &cfTrustValue, 1, &kCFTypeArrayCallBacks );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDSNAttrTrustInformation), cfValue );
		
		DSCFRelease( cfValue );
	}

	if ( bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDS1AttrDataStamp)) == true )
	{
		CDSLocalPluginNode*	node			= NodeObjectFromNodeDict( inNodeDict );
		CFStringRef			cfStampValue	= CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%u"), node->GetModValue() );
		CFArrayRef			cfValue			= CFArrayCreate( kCFAllocatorDefault, (const void **) &cfStampValue, 1, &kCFTypeArrayCallBacks );
		
		DSCFRelease( cfStampValue );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDS1AttrDataStamp), cfValue );
		
		DSCFRelease( cfValue );
	}
	
	if ( bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDSNAttrRecordType)) == true )
	{
		CFIndex		numRecordTypes	= CFDictionaryGetCount( mRecStdToNativeMappings );
		CFTypeRef	*keys			= (CFTypeRef *) malloc( numRecordTypes * sizeof(CFTypeRef) );
		CFArrayRef	cfValue			= NULL;
		
		CFDictionaryGetKeysAndValues( mRecStdToNativeMappings, keys, NULL );
		
		cfValue = CFArrayCreate( kCFAllocatorDefault, keys, numRecordTypes, &kCFTypeArrayCallBacks );
		CFDictionarySetValue( cfAttributes, CFSTR(kDSNAttrRecordType), cfValue );

		DSCFRelease( cfValue );
		DSFree( keys );
	}

	if ( bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDSNAttrAuthMethod)) == true )
	{
		CFMutableArrayRef mutableArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		
		CFArrayAppendValue( mutableArray, CFSTR(kDSStdAuthCrypt) );
		CFArrayAppendValue( mutableArray, CFSTR(kDSStdAuthClearText) );
		CFArrayAppendValue( mutableArray, CFSTR(kDSStdAuthNodeNativeNoClearText) );
		CFArrayAppendValue( mutableArray, CFSTR(kDSStdAuthNodeNativeClearTextOK) );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDSNAttrAuthMethod), mutableArray );
		
		DSCFRelease( mutableArray );
	}
	
	DSCFRelease( cfAttributes );
	
	return cfNodeInfo;
}

tDirStatus CDSLocalPlugin::GetRetainedRecordDict( CFStringRef inRecordName, CFStringRef inNativeRecType,
	CDSLocalPluginNode* inNode, CFMutableDictionaryRef* outRecordDict )
{
	tDirStatus siResult = eDSNoErr;
	CFMutableArrayRef mutableRecordsArray = NULL;
	
	try
	{
		if (inRecordName == NULL) throw( eDSRecordNotFound );
		mutableRecordsArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		CFArrayRef recNames = CFArrayCreate( NULL, (const void**)&inRecordName, 1, &kCFTypeArrayCallBacks );
		siResult = inNode->GetRecords( inNativeRecType, recNames, CFSTR( kDSNAttrRecordName ), eDSiExact, false, 1,
			mutableRecordsArray );
		CFRelease( recNames );
		recNames = NULL;
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		if ( CFArrayGetCount( mutableRecordsArray ) == 0 )
			throw( eDSRecordNotFound );
		
		if ( outRecordDict != NULL )
		{
			*outRecordDict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( mutableRecordsArray, 0 );
			CFRetain( *outRecordDict );
		}
	}
	catch( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CDSLocalPlugin::GetRetainedRecordDict(): caught error %d", err );
		siResult = err;
	}
	
	if ( mutableRecordsArray != NULL )
		CFRelease( mutableRecordsArray );
	
	return siResult;
}

bool CDSLocalPlugin::RecurseUserIsMemberOfGroup( CFStringRef inUserRecordName, CFStringRef inUserGUID,
	CFStringRef inUserGID, CFDictionaryRef inGroupDict, CDSLocalPluginNode* inNode )
{
	bool isMember = false;

	// check membership by GID
	if ( inUserGID != NULL )
	{
		CFArrayRef groupGIDs = (CFArrayRef)CFDictionaryGetValue( inGroupDict,
		this->AttrNativeTypeForStandardType( CFSTR( kDS1AttrPrimaryGroupID ) ) );
		if ( ( groupGIDs != NULL ) && CFArrayGetCount( groupGIDs ) > 0 )
		{
			CFStringRef groupGID = (CFStringRef)CFArrayGetValueAtIndex( groupGIDs, 0 );
			if ( CFStringCompare( groupGID, inUserGID, 0 ) == kCFCompareEqualTo )
				isMember = true;
		}
	}
	
	// check membership by GUID
	if ( !isMember && inUserGUID != NULL )
	{
		CFArrayRef guidMembers = (CFArrayRef)CFDictionaryGetValue( inGroupDict,
			this->AttrNativeTypeForStandardType( CFSTR( kDSNAttrGroupMembers ) ) );
		CFIndex numGuidMembers = 0;
		if ( ( guidMembers != NULL ) && ( ( numGuidMembers = CFArrayGetCount( guidMembers ) ) > 0 ) )
		if ( CFArrayContainsValue( guidMembers, CFRangeMake( 0, numGuidMembers ), inUserGUID ) )
			isMember = true;
	}
	
	// check membership by short name
	if ( !isMember && inUserRecordName != NULL )
	{
		CFArrayRef shortNameMembers = (CFArrayRef)CFDictionaryGetValue( inGroupDict,
			this->AttrNativeTypeForStandardType( CFSTR( kDSNAttrGroupMembership ) ) );
		CFIndex numShortNameMembers = 0;
		if ( ( shortNameMembers != NULL ) && ( ( numShortNameMembers = CFArrayGetCount( shortNameMembers ) ) > 0 ) )
		if ( CFArrayContainsValue( shortNameMembers, CFRangeMake( 0, numShortNameMembers ), inUserRecordName ) )
			isMember = true;
	}
	
	// recursively check any groups that have this group as a nested group
	if ( !isMember && inUserGUID != NULL )
	{
		CFArrayRef groupGUIDs = (CFArrayRef)CFDictionaryGetValue( inGroupDict,
			this->AttrNativeTypeForStandardType( CFSTR( kDS1AttrGeneratedUID ) ) );
		if ( ( groupGUIDs != NULL ) && ( CFArrayGetCount( groupGUIDs ) > 0 ) )
		{
			CFMutableArrayRef groups = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
			tDirStatus dirStatus = inNode->GetRecords( this->RecordNativeTypeForStandardType(
				CFSTR( kDSStdRecordTypeGroups ) ), groupGUIDs, CFSTR( kDSNAttrNestedGroups ), eDSExact, false, 0, groups );
			if ( dirStatus == eDSNoErr )
			{
				CFIndex numGroups = CFArrayGetCount( groups );
				for ( CFIndex i=0; i<numGroups && !isMember; i++ )
				{
					if ( this->RecurseUserIsMemberOfGroup( inUserRecordName, inUserGUID, inUserGID, inGroupDict, inNode ) )
						isMember = true;
				}
			}
			CFRelease( groups );
		}
	}

	return isMember;
}

tDirStatus
CDSLocalPlugin::GetDirServiceRef( tDirReference *outDSRef )
{
	tDirStatus status = (mDSRef == 0) ? dsOpenDirService( &mDSRef ) : eDSNoErr;
	if ( outDSRef != NULL )
		*outDSRef = mDSRef;
	
	return status;
}


CFTypeRef CDSLocalPlugin::GetAttrValueFromInput( const char* inData, UInt32 inDataLength )
{
	CFTypeRef	outAttrValue	= NULL;
	UInt32		dataLength		= inDataLength;
	
	// here we attempt to determine whether the data input is binary or UTF8
	// check if the length is correct
	// if so then check if we can make a CFString
	// otherwise binary

	if ( inData != NULL && inDataLength > 0 )
	{
		// are there any nulls in the data?
		// (cannot use strlen() on something that might be data)
		for ( unsigned idx = 0; idx < inDataLength; idx++ ) {
			if ( inData[idx] == '\0' ) {
				dataLength = idx;
				break;
			}
		}
		
		// temp workaround Admin FW bug in passing length with NULL byte included
		if ( (dataLength == inDataLength) || (dataLength + 1 == inDataLength) )
		{
			// use CFStringCreateWithBytes because we're not guaranteed a terminator 
			CFStringRef aStringRef = CFStringCreateWithBytes( NULL, (UInt8 *)inData, dataLength, kCFStringEncodingUTF8, false );
			if ( aStringRef != NULL )
			{
				// we always assume UTF8
				outAttrValue = (CFTypeRef)aStringRef;
			}
		}
		if ( outAttrValue == NULL )
			outAttrValue = (CFTypeRef)CFDataCreate( NULL, (const UInt8*)inData, inDataLength );
	}
	
	return(outAttrValue);

} // GetAttrValueFromInput

CFMutableArrayRef CDSLocalPlugin::CreateCFArrayFromGenericDataList( tDataListPtr inDataList )
{
	if (inDataList == NULL) return NULL;
 
	UInt32 numItems = inDataList->fDataNodeCount;
	CFMutableArrayRef mutableArray = CFArrayCreateMutable( NULL, inDataList->fDataNodeCount, &kCFTypeArrayCallBacks );
	if ( mutableArray == NULL ) return NULL;

	tDataNodePtr		pCurrNode	= nil;
	tDataBufferPriv	   *pPrivData	= nil; // KW don't really want this data type known in here
	CFTypeRef			itemCFType	= NULL;

	pCurrNode = inDataList->fDataListHead;

	if ( pCurrNode == NULL )
	{
		DSCFRelease(mutableArray);
		return NULL;
	}

	for ( UInt32 i = 0; i < numItems && pCurrNode != NULL; i++ )
	{
		pPrivData = (tDataBufferPriv *)pCurrNode;

		// handle binary data
		itemCFType = GetAttrValueFromInput( pPrivData->fBufferData, pPrivData->fBufferLength );
		if (itemCFType != NULL)
		{
			CFArrayAppendValue( mutableArray, itemCFType );
			DSCFRelease( itemCFType );
		}

		pCurrNode = pPrivData->fNextPtr;
	}
	
	if ( mutableArray != NULL && CFArrayGetCount(mutableArray) == 0 )
		DSCFRelease( mutableArray );

	return mutableArray;
	
} // CreateCFArrayFromGenericDataList


CFDictionaryRef
CDSLocalPlugin::CopyNodeDictandNodeObject( CFDictionaryRef inOpenRecordDict, CDSLocalPluginNode **outPluginNode )
{
	CFDictionaryRef nodeDict = NULL;
	
	// get the node dictionary
	mOpenNodeRefsLock.WaitLock();
	try
	{
		nodeDict = (CFDictionaryRef)CFDictionaryGetValue( inOpenRecordDict, CFSTR(kOpenRecordDictNodeDict) );
		if ( nodeDict != NULL )
		{
			CFRetain( nodeDict );
			*outPluginNode = this->NodeObjectFromNodeDict( nodeDict );
		}
	}
	catch( tDirStatus catchStatus )
	{
	}
	catch( ... )
	{
	}
	
	mOpenNodeRefsLock.SignalLock();
	
	return nodeDict;
}


//------------------------------------------------------------------------------------
//	* ReleaseContinueData
//------------------------------------------------------------------------------------

tDirStatus CDSLocalPlugin::ReleaseContinueData( sReleaseContinueData *inData )
{
	tDirStatus siResult = eDSNoErr;
	
	// RemoveContext calls our ContinueDeallocProc to clean up
	if ( gLocalContinueTable->RemoveContext( inData->fInContinueData ) != eDSNoErr )
		siResult = eDSInvalidContext;
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* ContinueDeallocProc
// ---------------------------------------------------------------------------

void CDSLocalPlugin::ContinueDeallocProc( void *inContinueData )
{
	CFMutableDictionaryRef pContinue = (CFMutableDictionaryRef)inContinueData;
	
	if ( pContinue != NULL )
	{
        // clean up anything in the dict
		
		CFRelease( pContinue );
	}
} // ContinueDeallocProc

void CDSLocalPlugin::FlushCaches( CFStringRef inStdType )
{
	uint32_t	theType = 0xffffffff;
	
	if ( inStdType != NULL )
	{
		if (CFStringCompare(inStdType, CFSTR(kDSStdRecordTypeUsers), 0) == kCFCompareEqualTo)
			theType = CACHE_ENTRY_TYPE_USER;
		else
		if (CFStringCompare(inStdType, CFSTR(kDSStdRecordTypeGroups), 0) == kCFCompareEqualTo)
			theType = CACHE_ENTRY_TYPE_GROUP;

		CFStringRef cfNativeType = RecordNativeTypeForStandardType( inStdType );
		if ( cfNativeType != NULL )
		{
			char *cStr = NULL;
			const char* nativeType = CStrFromCFString( cfNativeType, &cStr, NULL, NULL );
			
			// do BSD notify
			// TODO:  need to be based on the node name, but only one node right now
			dsNotifyUpdatedRecord( "Local", NULL, nativeType );
			
			DSFree( cStr );
		}
	}
	
	if ( theType != 0xffffffff )
	{
		const char *theTypeStr	= (theType == CACHE_ENTRY_TYPE_USER ? "user" : "group");
		
		dispatch_async( dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_HIGH),
					    ^(void) {
							DbgLog( kLogDebug, "CDSLocalPlugin::FlushRecord type %s - flushing membership cache", theTypeStr );
							dsFlushMembershipCache();
			
							if ( gCacheNode != NULL )
							{
								DbgLog( kLogDebug, "CDSLocalPlugin::FlushRecord type %s - flushing libinfo cache", theTypeStr );
								gCacheNode->EmptyCacheEntryType( theType );
							}
						} );
	}
}

CFMutableDictionaryRef CDSLocalPlugin::RecordDictForRecordRef( tRecordReference inRecordRef )
{
	CFNumberRef		recRefNumber	= CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &inRecordRef );
	CFMutableDictionaryRef returnValue		= NULL;
	
	if ( recRefNumber != NULL )
	{
		mOpenRecordRefsLock.WaitLock();
		returnValue = (CFMutableDictionaryRef) CFDictionaryGetValue( mOpenRecordRefs, recRefNumber );
		
		// see if the record was deleted
		if ( returnValue != NULL )
		{
			CFBooleanRef isDeleted = (CFBooleanRef) CFDictionaryGetValue( returnValue, CFSTR(kOpenRecordDictIsDeleted) );
			if ( CFBooleanGetValue(isDeleted) == TRUE )
			{
				// it is deleted, lets clean up and return invalid reference
				CFDictionaryRemoveValue( mOpenRecordRefs, recRefNumber );
				returnValue = NULL;
			}
		}
		
		mOpenRecordRefsLock.SignalLock();
		
		DSCFRelease( recRefNumber );
	}
	
	return returnValue;
}

// BaseDirectoryPlugin pure virtual overrides

CFDataRef CDSLocalPlugin::CopyConfiguration( void )
{
	return NULL;
}

bool CDSLocalPlugin::NewConfiguration( const char *inData, UInt32 inLength )
{
	SInt32	siResult = eNotHandledByThisNode;

	return siResult;
}

bool CDSLocalPlugin::CheckConfiguration( const char *inData, UInt32 inLength )
{
	return false;
}

tDirStatus CDSLocalPlugin::HandleCustomCall( sBDPINodeContext *pContext, sDoPlugInCustomCall *inData )
{
	tDirStatus	siResult = eNotHandledByThisNode;

	return siResult;
}

bool CDSLocalPlugin::IsConfigureNodeName( CFStringRef inNodeName )
{
	if ( CFStringCompare(inNodeName, CFSTR(kLocalNodePrefix), 0) == kCFCompareEqualTo )
		return true;
	
	return false;
}

BDPIVirtualNode *CDSLocalPlugin::CreateNodeForPath( CFStringRef inPath, uid_t inUID, uid_t inEffectiveUID )
{
	if ( IsConfigureNodeName(inPath) )
		return new CDSLocalConfigureNode( inPath, inUID, inEffectiveUID );
	
	return NULL;
}
