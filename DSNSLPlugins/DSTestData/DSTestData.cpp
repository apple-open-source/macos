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
 *  @header DSTestData
 */

#include "CNSLHeaders.h"

#include "DSTestData.h"
#include "CNSLTimingUtils.h"

const CFStringRef	gBundleIdentifier = CFSTR("com.apple.DirectoryService.DSTestData");
const char*		gProtocolPrefixString = "DSTestData";

#define kDefaultDataFile	CFSTR("/Library/Preferences/DirectoryService/DSTestData.plist")

pthread_mutex_t	gDSTestDataLocalZoneLock = PTHREAD_MUTEX_INITIALIZER;

#define kPathDelimiter	CFSTR("\t")
void ReturnMatchingService(const void *key, const void *value, void *context);

#pragma warning "Need to get our default Node String from our resource"

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0xBB, 0x49, 0xB1, 0x1D, 0x9B, 0x3B, 0x11, 0xD5, \
								0x8D, 0xF5, 0x00, 0x30, 0x65, 0x3D, 0x61, 0xE3 );

}

static CDSServerModule* _Creator ( void )
{
	DBGLOG( "Creating new DSTestData Plugin\n" );
    return( new DSTestData );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

DSTestData::DSTestData( void )
    : CNSLPlugin()
{
	DBGLOG( "DSTestData::DSTestData\n" );
    mLocalNodeString = NULL;
	mStaticRegistrations = NULL;
	mActivatedByNSL = true;			// don't wait
	mOnTheFlyEnabled = false;
}

DSTestData::~DSTestData( void )
{
	DBGLOG( "DSTestData::~DSTestData\n" );
    
    if ( mLocalNodeString );
        free( mLocalNodeString );

    mLocalNodeString = NULL;
	
}

sInt32 DSTestData::InitPlugin( void )
{
	return eDSNoErr;
}

void DSTestData::ActivateSelf( void )
{
    DBGLOG( "DSTestData::ActivateSelf called\n" );

	LoadStaticDataFromFile( kDefaultDataFile );		// ignore any errors if the file isn't there
	
	CNSLPlugin::ActivateSelf();
}

void DSTestData::DeActivateSelf( void )
{
    DBGLOG( "DSTestData::DeActivateSelf called\n" );

	if ( mStaticRegistrations )
		CFRelease( mStaticRegistrations );
	mStaticRegistrations = NULL;
	
	CNSLPlugin::DeActivateSelf();
}

#pragma mark -
CFStringRef DSTestData::GetBundleIdentifier( void )
{
    return gBundleIdentifier;
}

// this is used for top of the node's path "NSL"
const char*	DSTestData::GetProtocolPrefixString( void )
{		
    return gProtocolPrefixString;
}


Boolean DSTestData::IsLocalNode( const char *inNode )
{
    Boolean result = false;
    
    pthread_mutex_lock(&gDSTestDataLocalZoneLock);
    
	if ( mLocalNodeString )
    {
        result = ( strcmp( inNode, mLocalNodeString ) == 0 );
    }
    
	pthread_mutex_unlock(&gDSTestDataLocalZoneLock);
    
    return result;
}

void DSTestData::SetLocalZone( const char* zone )
{
    pthread_mutex_lock(&gDSTestDataLocalZoneLock);
    
	if ( zone )
    {
        if ( mLocalNodeString )
            free( mLocalNodeString );
        
        mLocalNodeString = (char*)malloc( strlen(zone) + 1 );
        strcpy( mLocalNodeString, zone );
    }

    pthread_mutex_unlock(&gDSTestDataLocalZoneLock);
}

void DSTestData::AddNode( CFStringRef nodePathRef, tDataList* nodePathList, CFMutableDictionaryRef serviceList )
{
    if ( !nodePathRef /*|| !IsActive()*/ )
        return;
        
    NodeData*		node = NULL;
	char*			nodeName = NULL;
    bool			isADefaultNode = false;
    
	if ( nodePathRef )
	{
		nodeName = (char*)malloc(CFStringGetLength(nodePathRef)+1);
		CFStringGetCString( nodePathRef, nodeName, CFStringGetLength(nodePathRef)+1, kCFStringEncodingUTF8 );
		
		DBGLOG( "DSTestData::AddNode (%s) called with %s\n", GetProtocolPrefixString(), nodeName );
		LockPublishedNodes();
		if ( ::CFDictionaryContainsKey( mPublishedNodes, nodePathRef ) )
			node = (NodeData*)::CFDictionaryGetValue( mPublishedNodes, nodePathRef );
		
		if ( node )
		{
			node->fTimeStamp = GetCurrentTime();	// update this node
		}
		else
		{
			// we have a new node
			DBGLOG( "DSTestData::AddNode(%s) Adding new %snode %s\n", GetProtocolPrefixString(), (isADefaultNode)?"local ":"", nodeName );
			node = AllocateNodeData();
			
			node->fNodeName = nodePathRef;
			CFRetain( node->fNodeName );
			
			node->fDSName = nodePathList;
	
			node->fTimeStamp = GetCurrentTime();
			
			node->fServicesRefTable = serviceList;
			
			if ( node->fServicesRefTable && CFGetTypeID( node->fServicesRefTable ) == CFDictionaryGetTypeID() )
				CFRetain( node->fServicesRefTable );
			else if ( node->fServicesRefTable )
			{
				syslog( LOG_ALERT, "Services is improper type (needs to be a dictionary of services)\n" );
				node->fServicesRefTable = NULL;
			}
				
			node->fIsADefaultNode = isADefaultNode;
			node->fSignature = GetSignature();
			
			if ( node->fDSName )
			{
				DSRegisterNode( node->fSignature, node->fDSName, kDirNodeType );
			}
			
			::CFDictionaryAddValue( mPublishedNodes, nodePathRef, node );
		}
		
		UnlockPublishedNodes();
		
		free( nodeName );
	}
}

#pragma mark -

void DSTestData::NewNodeLookup( void )
{
	if ( mStaticRegistrations )
	{
		sInt32 siResult = RegisterStaticConfigData( mStaticRegistrations, CFSTR("DSTestData") );
	
		if ( siResult != eDSNoErr )
			DBGLOG( "DSTestData::NewNodeLookup, RegisterStaticConfigData returned %d\n", siResult );
	}
}

typedef struct ServiceLookupContext
{
	CFStringRef		serviceTypeRef;
	CNSLDirNodeRep* nodeDirRep;
};

void DSTestData::NewServiceLookup( char* serviceType, CNSLDirNodeRep* nodeDirRep )
{
	DBGLOG( "DSTestData::NewServicesLookup (%s)\n", serviceType );
	
    NodeData*			node = (NodeData*)CFDictionaryGetValue( mPublishedNodes, nodeDirRep->GetNodePath() );
	
	if ( node )
	{
		CFStringRef				serviceTypeRef = CFStringCreateWithCString( NULL, serviceType, kCFStringEncodingUTF8 );
		
		if( mOnTheFlyEnabled && strcmp(serviceType, "afp") == 0 )
		{
			CFIndex			i;
			CFStringRef		serviceNameRef;
			
			for ( i=1; i<=mNumberOfServicesToGenerate; i++ )
			{
				CNSLResult* newResult = new CNSLResult();
				
				serviceNameRef = CFStringCreateWithFormat( NULL, NULL, CFSTR("Server %d"), i );
				
				if ( serviceNameRef )
				{
					newResult->SetURL( CFSTR("afp://example.url.apple.com") );
					newResult->SetServiceType( serviceTypeRef );
					
					newResult->AddAttribute( CFSTR(kDSNAttrRecordName), serviceNameRef );		// this should be what is displayed

					DBGLOG( "DSTestData::NewServicesLookup calling nodeDirRep->AddService on (Server %d)\n", i );
					
					nodeDirRep->AddService( newResult );
					CFRelease( serviceNameRef );
				}
			}
		}
		else if ( !mOnTheFlyEnabled )
		{
			CFDictionaryRef			servicesRef = node->fServicesRefTable;
			
			if ( servicesRef && CFDictionaryGetCount( servicesRef ) > 0 )
			{
				ServiceLookupContext	context = { serviceTypeRef, nodeDirRep };

				::CFDictionaryApplyFunction( servicesRef, ReturnMatchingService, &context );
			}
			
			if ( serviceTypeRef )
				CFRelease( serviceTypeRef );
		}
	}
	else
		DBGLOG( "DSTestData::NewServicesLookup no nodeData!\n" );
}

void ReturnMatchingService(const void *key, const void *value, void *context)
{
	if ( key && value && context )
	{
		ServiceLookupContext*		serviceLookupData = (ServiceLookupContext*)context;
		CFDictionaryRef				serviceRef = (CFDictionaryRef)value;
		CFStringRef					serviceTypeRef = (CFStringRef)CFDictionaryGetValue( serviceRef, CFSTR(kDS1AttrServiceType) );
		
		if ( serviceTypeRef && CFStringCompare( serviceTypeRef, serviceLookupData->serviceTypeRef, 0 ) == kCFCompareEqualTo )
		{
			CNSLResult* newResult = new CNSLResult();
		
			newResult->SetURL( (CFStringRef)CFDictionaryGetValue( serviceRef, CFSTR(kDSNAttrURL) ) );
			newResult->SetServiceType( serviceTypeRef );
			
			newResult->AddAttribute( CFSTR(kDSNAttrRecordName), (CFStringRef)CFDictionaryGetValue( serviceRef, CFSTR(kDSNAttrRecordName) ) );		// this should be what is displayed
		
			serviceLookupData->nodeDirRep->AddService( newResult );
		}
	}
}

Boolean DSTestData::OKToOpenUnPublishedNode( const char* parentNodeName )
{
    return false;
}

#pragma mark -

sInt32 DSTestData::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	sInt32					siResult	= eDSNoErr;
	unsigned long			aRequest	= 0;

	DBGLOG( "DSTestData::DoPlugInCustomCall called\n" );
//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

	try
	{
		if ( inData == nil ) throw( (sInt32)eDSNullParameter );
		if ( mOpenRefTable == nil ) throw ( (sInt32)eDSNodeNotFound );
		
		const void*				dictionaryResult	= NULL;
		CNSLDirNodeRep*			nodeDirRep			= NULL;
		
		dictionaryResult = ::CFDictionaryGetValue( mOpenRefTable, (const void*)inData->fInNodeRef );
		if( !dictionaryResult )
			DBGLOG( "DSTestData::DoPlugInCustomCall called but we couldn't find the nodeDirRep!\n" );
	
		nodeDirRep = (CNSLDirNodeRep*)dictionaryResult;
	
		if ( nodeDirRep )
		{
			aRequest = inData->fInRequestCode;

			switch( aRequest )
			{
				case kOnTheFlySetup:
				{
					siResult = DoOnTheFlySetup( inData );
				}
				break;
				
				case kReadDSTestStaticDataFromFile:
				{
					DBGLOG( "DSTestData::DoPlugInCustomCall kReadDSTestStaticDataFromFile\n" );
					CFStringRef		pathToLoad = NULL;
					
					mOnTheFlyEnabled = false;
					if ( inData->fInRequestData->fBufferData )
					{
						pathToLoad = CFStringCreateWithCString( NULL, inData->fInRequestData->fBufferData, kCFStringEncodingUTF8 );
						
						if ( pathToLoad )
						{
							LoadStaticDataFromFile( pathToLoad );
							CFRelease( pathToLoad );
							
							ClearOutAllNodes();
							StartNodeLookup();
						}
						else
							syslog( LOG_ALERT, "DSTestData::DoPlugInCustomCall, pathToLoad is NULL!\n" );
					}
					else
						syslog( LOG_ALERT, "DSTestData::DoPlugInCustomCall, inData->fInRequestData->fBufferData is NULL!\n" );
				}
				break;
				
				case kReadDSTestConfigXMLData:
				{
					DBGLOG( "DSTestData::DoPlugInCustomCall kReadDSTestConfigXMLData\n" );
				}
				break;
				
				case kReadDSTestStaticDataXMLData:
				{
					DBGLOG( "DSTestData::DoPlugInCustomCall kReadDSTestStaticDataXMLData\n" );
					mOnTheFlyEnabled = false;
					siResult = FillOutCurrentStaticDataWithXML( inData );
				}
				break;
				
				case kWriteDSTestConfigXMLData:
				{
					DBGLOG( "DSTestData::DoPlugInCustomCall kWriteDSTestConfigXMLData\n" );
				}
				break;
				
				case kWriteDSTestStaticDataXMLData:
				{
					DBGLOG( "DSTestData::DoPlugInCustomCall kWriteSMBConfigXMLData\n" );
					mOnTheFlyEnabled = false;
					siResult = SaveNewStaticDataFromXML( inData );
				}
				break;
				
				case kAddNewTopLevelNode:
				{
					DBGLOG( "DSTestData::DoPlugInCustomCall kAddNewTopLevelNode\n" );
					mOnTheFlyEnabled = false;
					siResult = AddNewTopLevelNode( inData );
				}
				break;
				
				case kAddNewNode:
				{
					DBGLOG( "DSTestData::DoPlugInCustomCall kAddNewNode\n" );
					mOnTheFlyEnabled = false;
					siResult = AddNewNode( inData );
				}
				
				default:
					break;
			}
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

}

sInt32 DSTestData::DoOnTheFlySetup( sDoPlugInCustomCall *inData )
{
	sInt32					siResult					= 0;
	sInt32					xmlDataLength				= 0;
	CFDataRef   			xmlData						= NULL;
	CFArrayRef				newNodeRef					= NULL;
	CFStringRef				errorString					= NULL;
	
	DBGLOG( "DSTestData::DoOnTheFlySetup called\n" );
	
	xmlDataLength = (sInt32) inData->fInRequestData->fBufferLength;
	
	if ( xmlDataLength <= 0 )
		return (sInt32)eDSInvalidBuffFormat;
	
	xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData), xmlDataLength);
	
	if ( !xmlData )
	{
		DBGLOG( "DSTestData::DoOnTheFlySetup, couldn't create xmlData from buffer!!\n" );
		siResult = (sInt32)eDSInvalidBuffFormat;
	}
	else
	{
		newNodeRef = (CFArrayRef)CFPropertyListCreateFromXMLData(	NULL,
																		xmlData,
																		kCFPropertyListImmutable,
																		&errorString);
	}
	
	if ( newNodeRef && CFGetTypeID( newNodeRef ) != CFDictionaryGetTypeID() )
	{
		DBGLOG( "DSTestData::DoOnTheFlySetup, XML Data wasn't a CFArray!!\n" );
		siResult = (sInt32)eDSInvalidBuffFormat;
	}
	else if ( newNodeRef )
	{
		mOnTheFlyEnabled = true;
		if ( mStaticRegistrations )
			CFRelease( mStaticRegistrations );
		mStaticRegistrations = NULL;
		
		CFStringRef		numNeighborhoodsRef = (CFStringRef)CFDictionaryGetValue( (CFDictionaryRef)newNodeRef, CFSTR("kNumNeighborhoods") );
		CFStringRef		numServicesRef = (CFStringRef)CFDictionaryGetValue( (CFDictionaryRef)newNodeRef, CFSTR("kNumServices") );
		
		ClearOutAllNodes();
		if ( numNeighborhoodsRef )
		{
			mNumberOfNeighborhoodsToGenerate = CFStringGetIntValue( numNeighborhoodsRef );

			DBGLOG( "DSTestData::DoOnTheFlySetup, mNumberOfNeighborhoodsToGenerate: %d\n", mNumberOfNeighborhoodsToGenerate );

			CFIndex		i=1;
			char		nodeName[256];
			
			for ( i=1; i<=mNumberOfNeighborhoodsToGenerate; i++ )
			{
				snprintf(nodeName, sizeof(nodeName), "Building %d", (int)i );

				CNSLPlugin::AddNode( nodeName, false );
			}
		}
		else
			mNumberOfNeighborhoodsToGenerate = 0;
		
		if ( numServicesRef )
			mNumberOfServicesToGenerate = CFStringGetIntValue( numServicesRef );
		else
			mNumberOfServicesToGenerate = 0;

		CFRelease( newNodeRef );
	}
	
	return siResult;
}

sInt32 DSTestData::FillOutCurrentStaticDataWithXML( sDoPlugInCustomCall *inData )
{
	sInt32					siResult					= eDSNoErr;
	CFDataRef   			xmlData						= NULL;
	CFRange					aRange;
	
	try
	{
		//convert the dict into a XML blob
		if ( mStaticRegistrations )
			xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, mStaticRegistrations );

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
		DBGLOG( "DSTestData::FillOutCurrentState: Caught error: %ld\n", err );
		siResult = err;
	}

	return siResult;
}

sInt32 DSTestData::SaveNewStaticDataFromXML( sDoPlugInCustomCall *inData )
{
	sInt32					siResult					= eDSNoErr;
	sInt32					xmlDataLength				= 0;
	CFDataRef   			xmlData						= NULL;
	CFMutableDictionaryRef	newStateRef					= NULL;
	CFStringRef				errorString					= NULL;
	
	DBGLOG( "DSTestData::SaveNewStateFromXML called\n" );
	
	xmlDataLength = (sInt32) inData->fInRequestData->fBufferLength;
	
	if ( xmlDataLength <= 0 )
		return (sInt32)eDSInvalidBuffFormat;
	
	xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData), xmlDataLength);
	
	if ( !xmlData )
	{
		DBGLOG( "DSTestData::SaveNewStateFromXML, couldn't create xmlData from buffer!!\n" );
		siResult = (sInt32)eDSInvalidBuffFormat;
	}
	else
		newStateRef = (CFMutableDictionaryRef)CFPropertyListCreateFromXMLData(	NULL,
																		xmlData,
																		kCFPropertyListMutableContainersAndLeaves,
																		&errorString);
																		
	if ( newStateRef && CFGetTypeID( newStateRef ) != CFDictionaryGetTypeID() )
	{
		DBGLOG( "DSTestData::SaveNewStateFromXML, XML Data wasn't a CFDictionary!\n" );
		siResult = (sInt32)eDSInvalidBuffFormat;
	}
	else
	{
		if ( mStaticRegistrations )
			CFRelease( mStaticRegistrations );
		mStaticRegistrations = newStateRef;
		
		SaveCurrentStateToDefaultDataFile();
		
		ClearOutAllNodes();
		NewNodeLookup();
	}
	
	if ( xmlData )
		CFRelease( xmlData );
		
	return siResult;
}

sInt32 DSTestData::AddNewTopLevelNode( sDoPlugInCustomCall *inData )
{
	sInt32					siResult					= eDSNoErr;
	CFStringRef				nodeNameRef					= NULL;
	CFMutableDictionaryRef	nodeRef						= NULL;
	CFMutableArrayRef		nodeArray					= NULL;
	
	if ( inData->fInRequestData->fBufferData )
	{
		nodeNameRef = CFStringCreateWithCString( NULL, inData->fInRequestData->fBufferData, kCFStringEncodingUTF8 );
		
		nodeRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );		
	
		CFDictionaryAddValue( nodeRef, CFSTR(kDSNAttrRecordName), nodeNameRef );
	}
	
	if ( !mStaticRegistrations )
	{
		// need to create this
		mStaticRegistrations = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	}
	
	nodeArray = (CFMutableArrayRef)CFDictionaryGetValue( mStaticRegistrations, CFSTR("dsRecTypeNative:Nodes") );
	
	if ( !nodeArray )
	{
		nodeArray = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
		CFDictionaryAddValue( mStaticRegistrations, CFSTR("dsRecTypeNative:Nodes"), nodeArray );
	}
	else
		CFRetain( nodeArray );		// so we can Release below
	
	if ( nodeRef && nodeArray )
		CFArrayAppendValue( nodeArray, nodeRef );
		
	ClearOutAllNodes();
	NewNodeLookup();
	
	if ( nodeNameRef )
		CFRelease( nodeNameRef );
	
	if ( nodeRef )
		CFRelease( nodeRef );
		
	if ( nodeArray )
		CFRelease( nodeArray );
		
	return siResult;
}

sInt32 DSTestData::AddNewNode( sDoPlugInCustomCall *inData )
{
	sInt32					siResult					= 0;
	sInt32					xmlDataLength				= 0;
	CFDataRef   			xmlData						= NULL;
	CFArrayRef				newNodeRef					= NULL;
	CFStringRef				errorString					= NULL;
	
	DBGLOG( "DSTestData::AddNewNode called\n" );
	
	xmlDataLength = (sInt32) inData->fInRequestData->fBufferLength;
	
	if ( xmlDataLength <= 0 )
		return (sInt32)eDSInvalidBuffFormat;
	
	xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData), xmlDataLength);
	
	if ( !xmlData )
	{
		DBGLOG( "DSTestData::AddNewNode, couldn't create xmlData from buffer!!\n" );
		siResult = (sInt32)eDSInvalidBuffFormat;
	}
	else
		newNodeRef = (CFArrayRef)CFPropertyListCreateFromXMLData(	NULL,
																		xmlData,
																		kCFPropertyListImmutable,
																		&errorString);
																		
	if ( newNodeRef && CFGetTypeID( newNodeRef ) != CFArrayGetTypeID() )
	{
		DBGLOG( "DSTestData::AddNewNode, XML Data wasn't a CFArray!!\n" );
		siResult = (sInt32)eDSInvalidBuffFormat;
	}
	else
	{
		// now we have an array that is the node path.  We want to create all pieces along the way if needed
		CFIndex						numPieces = CFArrayGetCount( newNodeRef );
		CFMutableArrayRef			curNodeListRef = (CFMutableArrayRef)CFDictionaryGetValue( mStaticRegistrations, CFSTR("dsRecTypeNative:Nodes") );
		
		if ( !curNodeListRef )
		{
			curNodeListRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
			CFDictionaryAddValue( mStaticRegistrations, CFSTR("dsRecTypeNative:Nodes"), curNodeListRef );
			CFRelease( curNodeListRef );
		}

		for ( CFIndex nodePieceIndex=0; nodePieceIndex<numPieces; nodePieceIndex++ )
		{
			CFStringRef				curNodePieceName = (CFStringRef)CFArrayGetValueAtIndex( newNodeRef, nodePieceIndex );
			CFMutableDictionaryRef	curNode = NULL;
			CFIndex					numNodesAtCurrentLevel = CFArrayGetCount( curNodeListRef );

			for ( CFIndex nodeListIndex=0; nodeListIndex<numNodesAtCurrentLevel; nodeListIndex++ )
			{
				curNode	 = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( curNodeListRef, nodeListIndex );
				
				if ( curNode )
				{
					CFStringRef		currentNodeNameRef = (CFStringRef)CFDictionaryGetValue( curNode, CFSTR("dsAttrTypeStandard:RecordName") );
					
					if ( currentNodeNameRef && CFStringCompare( currentNodeNameRef, curNodePieceName, 0 ) == kCFCompareEqualTo )
					{
						break;
					}
				}
				
				curNode = NULL;
			}
			
			if ( !curNode )
			{
				curNode = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks );
				CFDictionaryAddValue( curNode, CFSTR(kDSNAttrRecordName), curNodePieceName );
				CFArrayAppendValue( curNodeListRef, curNode );
				CFRelease( curNode );
			}
			
			curNodeListRef = (CFMutableArrayRef)CFDictionaryGetValue( curNode, CFSTR("dsRecTypeNative:Nodes") );
				
			if ( !curNodeListRef )
			{
				curNodeListRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
				CFDictionaryAddValue( curNode, CFSTR("dsRecTypeNative:Nodes"), curNodeListRef );
				CFRelease( curNodeListRef );
			}
		}
		
		SaveCurrentStateToDefaultDataFile();
		
		ClearOutAllNodes();
		NewNodeLookup();
	}
	
	if ( xmlData )
		CFRelease( xmlData );
		
	return siResult;
}

#pragma mark -
sInt32 DSTestData::LoadStaticDataFromFile( CFStringRef pathToFile )
{
	sInt32					siResult					= eDSNoErr;
	CFDataRef   			xmlData						= NULL;
	CFMutableDictionaryRef	staticRegistrations			= NULL;
	CFStringRef				errorString					= NULL;
	
//	DBGLOG( "DSTestData::SaveNewStateFromXML called\n" );
	
	CFURLRef configFileURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, pathToFile, kCFURLPOSIXPathStyle, false );
	
	if ( CFURLCreateDataAndPropertiesFromResource(
													kCFAllocatorDefault,
													configFileURL,
													&xmlData,          // place to put file data
													NULL,           
													NULL,
													&siResult) )
	{
		staticRegistrations = (CFMutableDictionaryRef)CFPropertyListCreateFromXMLData(	NULL, xmlData, kCFPropertyListMutableContainersAndLeaves, &errorString);
	}
    	
	if ( staticRegistrations && CFGetTypeID( staticRegistrations ) != CFDictionaryGetTypeID() )
	{
//		DBGLOG( "DSTestData::SaveNewStateFromXML, XML Data wasn't a CFDictionary!\n" );
		syslog( LOG_ALERT, "DSTestData: static registration data is improper type (top level needs to be a dictionary)\n" );
		siResult = (sInt32)eDSPlugInConfigFileError;
	}
	else if ( staticRegistrations )
	{
		if ( mStaticRegistrations )
			CFRelease( mStaticRegistrations );
			
		mStaticRegistrations = staticRegistrations;
		CFRetain( mStaticRegistrations );
		
		if ( CFStringCompare( pathToFile, kDefaultDataFile, 0 ) != kCFCompareEqualTo )
		{
			// we have been told to load a different file, save or current data to our kDefaultDataFile
			SaveCurrentStateToDefaultDataFile();
		}
	}

	if ( xmlData )
		CFRelease( xmlData );
		
	if ( staticRegistrations )
		CFRelease( staticRegistrations );

	return siResult;
}

sInt32 DSTestData::SaveCurrentStateToDefaultDataFile( void )
{
	CFDataRef   			xmlData						= NULL;
	CFURLRef				configFileURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, kDefaultDataFile, kCFURLPOSIXPathStyle, false );
	sInt32					siResult = eDSNoErr;

	if ( mStaticRegistrations )
	{
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, mStaticRegistrations );
	
		if ( xmlData )
		{
			if ( !CFURLWriteDataAndPropertiesToResource(
														configFileURL,
														xmlData,          // place to put file data
														NULL,
														&siResult) )
				syslog( LOG_ALERT, "DSTestData: unable to write out changes to default file: %ld\n", siResult );
			
			CFRelease( xmlData );
		}
													
	}
	
	return siResult;
}

sInt32 DSTestData::RegisterStaticConfigData( CFDictionaryRef parentNodeRef, CFStringRef parentPathRef )
{
	sInt32			status = eDSNoErr;
	
	if ( !parentNodeRef )
		return status;
		
	// the static registration dictionary should contain the following:
	// an array of nodes
		// each node should have an array of nodes and an array of services
	
	CFArrayRef		listOfNodes = (CFArrayRef)CFDictionaryGetValue( parentNodeRef, CFSTR("dsRecTypeNative:Nodes") );

	if ( listOfNodes && CFGetTypeID( listOfNodes ) == CFArrayGetTypeID() )
	{
		CFIndex			numSubNodes = CFArrayGetCount(listOfNodes);
		
		for ( CFIndex index = 0; index<numSubNodes; index++ )
		{
			CFMutableDictionaryRef	subNodeRef = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( listOfNodes, index );
			
			if ( subNodeRef && CFGetTypeID( subNodeRef ) == CFDictionaryGetTypeID() )
			{
				CFStringRef			nodeName = (CFStringRef)CFDictionaryGetValue( subNodeRef, CFSTR(kDSNAttrRecordName) );
				CFMutableStringRef	nodePath = CFStringCreateMutableCopy( NULL, 0, parentPathRef );
				
				if ( nodeName && nodePath )
				{
					if ( CFStringGetLength( nodePath ) > 0 )
						CFStringAppend( nodePath, kPathDelimiter );
					CFStringAppend( nodePath, nodeName );
					
	//				CFShow( nodePath );
					tDataList* nodePathList = dsBuildListFromStringsPriv(nil);
	
					CFArrayRef	pathPieces = CFStringCreateArrayBySeparatingStrings( NULL, nodePath, kPathDelimiter );
					
					if ( pathPieces && CFArrayGetCount(pathPieces)>0 )
					{
						CFIndex					numPieces = (pathPieces)?CFArrayGetCount(pathPieces):0;
						char					nodePathPiece[1024];
						CFMutableDictionaryRef	serviceListRef = (CFMutableDictionaryRef)CFDictionaryGetValue( subNodeRef, CFSTR("dsRecTypeStandard:Services") );
						
						if ( !serviceListRef )
						{
							serviceListRef = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks );
							CFDictionaryAddValue( subNodeRef, CFSTR("dsRecTypeStandard:Services"), serviceListRef );
							CFRelease( serviceListRef );
						}
						
						for ( CFIndex nodePathIndex=0; nodePathIndex<numPieces; nodePathIndex++ )
						{
							if ( CFStringGetCString( (CFStringRef)CFArrayGetValueAtIndex(pathPieces, nodePathIndex), nodePathPiece, sizeof(nodePathPiece), kCFStringEncodingUTF8 )  )
							{
								dsAppendStringToListAllocPriv( nodePathList, nodePathPiece );
							}
						}
				
						AddNode( nodePath, nodePathList, serviceListRef );
					}
				}
				
//				dsDataListDeallocatePriv( nodePathList );
//				free( nodePathList );

				status = RegisterStaticConfigData( subNodeRef, nodePath );
			}
			else
			{
				syslog( LOG_ALERT, "DSTestData: static registration data is improper type (subnode needs to be a array)\n" );
				status = eDSPlugInConfigFileError;
				break;
			}
		}
	}
	else if ( listOfNodes )
	{
		status = eDSPlugInConfigFileError;
		syslog( LOG_ALERT, "DSTestData: static registration data is improper type (dsRecTypeNative:Nodes needs to be a array)\n" );
	}
	
/*	CFArrayRef		listOfServices = (CFArrayRef)CFDictionaryGetValue( parentNodeRef, CFSTR("dsRecTypeNative:Nodes") );

	if ( listOfServices && CFGetTypeID( listOfServices ) == CFArrayGetTypeID() )
	{
		CFIndex			numServices = CFArrayGetCount(listOfServices);
		
		for ( CFIndex index = 0; index<numServices; index++ )
		{
			CFDictionaryRef	serviceRef = (CFDictionaryRef)CFArrayGetValueAtIndex( listOfServices, index );
			
			if ( serviceRef && CFGetTypeID( serviceRef ) == CFDictionaryGetTypeID() )
			{

			}
			else
			{
				status = eDSPlugInConfigFileError;
				break;
			}
		}
	}
	else if ( listOfServices )
	{
		status = eDSPlugInConfigFileError;
	}
*/	
	return status;
}


#pragma mark -
sInt32 DSTestData::RegisterService( tRecordReference recordRef, CFDictionaryRef service )
{
    sInt32		status = eDSNoErr;

	SaveCurrentStateToDefaultDataFile();
        
    return status;
}

sInt32 DSTestData::DeregisterService( tRecordReference recordRef, CFDictionaryRef service )
{
    sInt32			status = eDSNoErr;
    CFStringRef		urlRef = NULL;
    CFStringRef		scopeRef = NULL;
	CFTypeRef		urlResultRef = NULL;
	
    if ( service )
		urlResultRef = (CFTypeRef)::CFDictionaryGetValue( service, kDSNAttrURLSAFE_CFSTR);

	if ( urlResultRef )
	{
		if ( CFGetTypeID(urlResultRef) == CFStringGetTypeID() )
		{
			urlRef = (CFStringRef)urlResultRef;
		}
		else if ( CFGetTypeID(urlResultRef) == CFArrayGetTypeID() )
		{
            DBGLOG( "DSTestData::DeregisterService, we have more than one URL (%ld) in this service! Just deregister the first one\n", CFArrayGetCount((CFArrayRef)urlResultRef) );
			urlRef = (CFStringRef)::CFArrayGetValueAtIndex( (CFArrayRef)urlResultRef, 0 );
		}
	}
	
	if ( urlRef && CFGetTypeID(urlRef) == CFStringGetTypeID() && CFStringGetLength( urlRef ) > 0 )
    {
        UInt32		scopePtrLength;
        char*		scopePtr = NULL;
        
        if ( (scopeRef = (CFStringRef)::CFDictionaryGetValue( service, kDS1AttrLocationSAFE_CFSTR)) )
        {
        	if ( CFGetTypeID( scopeRef ) == CFArrayGetTypeID() )
            {
                scopeRef = (CFStringRef)::CFArrayGetValueAtIndex( (CFArrayRef)scopeRef, 0 );	// just get the first one for now
            }

        	scopePtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( scopeRef ), kCFStringEncodingUTF8 ) + 1;
        	scopePtr = (char*)malloc( scopePtrLength );

            ::CFStringGetCString( scopeRef, scopePtr, scopePtrLength, kCFStringEncodingUTF8 );
        }
        else
        {
            scopePtr = (char*)malloc(1);
            scopePtr[0] = '\0';
        }
        
        UInt32		urlPtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( urlRef ), kCFStringEncodingUTF8 ) + 1;
        char*		urlPtr = (char*)malloc( urlPtrLength );
        
        ::CFStringGetCString( urlRef, urlPtr, urlPtrLength, kCFStringEncodingUTF8 );
        
        if ( urlPtr[0] != '\0' )
        {
#ifdef REGISTER_WITH_ATTRIBUTE_DATA_IN_URL
            char*					attributePtr = NULL;            
            CFMutableStringRef		attributeRef = ::CFStringCreateMutable( NULL, 0 );
            CFMutableStringRef		attributeForURLRef = ::CFStringCreateMutable( NULL, 0 );	// mod this for appending to URL
            CFMutableDictionaryRef	attributesDictRef = ::CFDictionaryCreateMutable( NULL, 2, &kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks );
            
            ::CFDictionaryAddValue( attributesDictRef, kAttributeListSAFE_CFSTR, attributeRef );
            ::CFDictionaryAddValue( attributesDictRef, kAttributeListForURLSAFE_CFSTR, attributeForURLRef );
            
            ::CFDictionaryApplyFunction( service, AddToAttributeList, attributesDictRef );

            CFStringInsert( attributeForURLRef, 0, urlRef );
            
            free( urlPtr );
            
            urlPtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( attributeForURLRef ), kCFStringEncodingUTF8 ) + 1;
            urlPtr = (char*)malloc( urlPtrLength );

            ::CFStringGetCString( attributeForURLRef, urlPtr, urlPtrLength, kCFStringEncodingASCII );
            
            CFIndex		attributePtrSize = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(attributeRef), kCFStringEncodingUTF8 ) + 1;
            attributePtr = (char*)malloc( attributePtrSize );
            attributePtr[0] = '\0';
            
            ::CFStringGetCString( attributeRef, attributePtr, attributePtrSize, kCFStringEncodingUTF8 );
            
            if ( attributePtr && attributePtr[strlen(attributePtr)-1] == ',' )
                attributePtr[strlen(attributePtr)] = '\0';
                
            ::CFRelease( attributeRef );
#endif            
        }
                        
        free( scopePtr );
        free( urlPtr );

    }
    else
        status = eDSNullAttribute;

    return status;
}

