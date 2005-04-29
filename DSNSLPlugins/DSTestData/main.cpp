/*
 * Copyright (c) 2000 - 2003 Apple Computer, Inc. All rights reserved.
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
 * @header dsTestDataConfig
 * Tool used to manipulate the DSTestData plugin.
 */


#include <stdio.h>
#include <string.h>

#include <CoreFoundation/CoreFoundation.h>
#include <DirectoryService/DirectoryService.h>

#define kMaxArgs		4		
#define kMinArgs		2

#define	kDSDelimiter						"\t"

#define kOnTheFlySetup						'otfs'

#define kWriteDSTestConfigXMLData			'wcfg'
#define kReadDSTestConfigXMLData			'rcfg'

#define kReadDSTestStaticDataXMLData		'rxml'
#define kWriteDSTestStaticDataXMLData		'wxml'

#define kReadDSTestStaticDataFromFile		'rffl'

#define kAddNewTopLevelNode					'adtn'
#define kAddNewNode							'adnn'

#define sInt32	SInt32

typedef struct {
	tDirReference	fDirNodeRef;
    tContextData	fContinueData;
	UInt32			fCount;
	Boolean			fIsBonjourLocalNode;
} NSLXDSNodeDataContext;

void GetXMLData( void );
void SendReadConfigMessage( char* pathToFile );
void SendXMLData( CFDataRef xmlData, unsigned long customCode );

CFDataRef	CopyXMLDataFromFile( char* pathToFile );
void DoAutoConfig1( int numNodes, int numServices );
void DoAutoConfig2( void );
tDirStatus AddNewNode( CFStringRef nodePath, CFStringRef pathDelimiter );
void AddTopLevelNode( char* nodeNameToAdd );
void AddTestServiceToTopLevelNode( char* nodeNameToAdd );
tDirStatus AddServiceToNodePath( tDataListPtr nodeToAddRecordTo, CFMutableDictionaryRef serviceRef );
tDirStatus AddServiceToOpenNode( tDirReference mDirRef, tDirNodeReference dirNodeRef, CFMutableDictionaryRef serviceRef );

tDirStatus BuildNetworkNodeList( CFMutableDictionaryRef nodeHierarchyRef );
tDirStatus BuildNeighborhoodNodeTree(	
										tDirReference mDirRef,
										Boolean isLocalNodeSearch,
										tDataBufferPtr nodeNamesPtr,
										UInt32 nodeCount,
										CFMutableDictionaryRef nodeNeighborhoodTree );
CFMutableDictionaryRef FindExistingNeighborhood( CFMutableArrayRef nodeList, CFStringRef nodeName );

tDirStatus FillOutServiceListWithServices( tDirReference mDirRef, CFMutableDictionaryRef serviceList, tDataListPtr curNodeNameList );
tDirStatus SearchDirNodeForURLData( tDirReference mDirRef, CFMutableDictionaryRef serviceList, NSLXDSNodeDataContext* nodeContext );

CFStringRef CreateServiceTypeFromDSType( const char *inDSType );
void CheckDSNodePath( CFStringRef parentNodePath, CFStringRef dsNodePath, Boolean* doNotShow, Boolean* onlyLocal );
Boolean IsOnlyLocal( CFStringRef dsNodePath );
Boolean IsBadDSNodePath( CFStringRef dsNodePath );
CFMutableDictionaryRef FindNeighborhodInNodeTree( CFMutableDictionaryRef nodeNeighborhoodTree, CFStringRef curNodePieceName, CFStringRef curDSPathRef );

void PrintHelpInfo( void )
{
	// Things we want this tool to do:
	// √ • get the xml of the static data
	// √ • send xml of data to use
	// √ • tell plugin to read config from a different file (use path)
	// √ • Auto create a large fake network
	// √ • Add a node
	// • Delete a node
	// √ • Add a service
	// • Delete a service
	
	printf( "Usage:\ndsTestDataConfig" );
	printf( " -getXMLData");
	printf( " | -sendXMLData pathToFile");
	printf( " | -readConfig pathToFile" );
	printf( " | -autocreate numNodes numServices\n" );
	printf( " | -copyNetwork\n" );
	printf( " | -addNode nodePath\n" );
	printf( " | -addService nodeName\n" );
	printf( "\tgetXMLData                           receives the xml data that the DSTestData plugin is using\n" );
	printf( "\tsendXMLData pathToFile               pathToFile is the local file you want to upload to the DSTestData plugin\n" );
	printf( "\treadConfig                           tells the DSTestData plugin to load a config file\n" );
	printf( "\tautocreate numNodes numServices      builds up a sample network system with numNodes and numServices per node\n" );
	printf( "\tcopyNetwork                          build up a cached view of the current network\n" );
	printf( "\taddNode nodePath                     add a path of nodes (slash delimited i.e. \"Cupertino/IL1/First\")\n" );
	printf( "\taddService nodeName                  add a test service to a top level node\n" );
}

int main(int argc, char *argv[])
{
	if ( argc > kMaxArgs || argc < kMinArgs )
    {
        PrintHelpInfo();
        return -1;
    }
	
	if ( (strcmp(argv[1], "-getXMLData") == 0 || strcmp(argv[1], "getXMLData") == 0) && argc == 2  ) 
	{
		GetXMLData();
	}
	else if ( (strcmp(argv[1], "-sendXMLData") == 0 || strcmp(argv[1], "sendXMLData") == 0) && argc == 3 && argv[2] != NULL )
	{
		CFDataRef xmlData = CopyXMLDataFromFile( argv[2] );
	
		if ( xmlData )
		{
			SendXMLData( xmlData, kWriteDSTestStaticDataXMLData );
			
			CFRelease( xmlData );
		}
	}
	else if ( (strcmp(argv[1], "-readConfig") == 0 || strcmp(argv[1], "readConfig") == 0) && argc == 3 && argv[2] != NULL )
	{
		SendReadConfigMessage( argv[2] );
	}
	else if ( (strcmp(argv[1], "-autocreate") == 0 || strcmp(argv[1], "autocreate") == 0 ) && argc == 4 && argv[2] != NULL && argv[3] != NULL )
	{
		DoAutoConfig1( atoi(argv[2]), atoi(argv[3]) );
	}
	else if ( (strcmp(argv[1], "-copyNetwork") == 0 || strcmp(argv[1], "copyNetwork") == 0) && argc == 2 )
	{
		DoAutoConfig2();
	}
	else if ( (strcmp(argv[1], "-addNode") == 0 || strcmp(argv[1], "addNode") == 0) && argc == 3 && argv[2] != NULL )
	{
		CFStringRef		path = CFStringCreateWithCString( NULL, argv[2], kCFStringEncodingUTF8 );
		AddNewNode( path, CFSTR("/") );
		CFRelease( path );
	}
	else if ( (strcmp(argv[1], "-addService") == 0 || strcmp(argv[1], "addService") == 0) && argc == 3 && argv[2] != NULL )
	{
		AddTestServiceToTopLevelNode( argv[2] );
	}
	else
	{
        PrintHelpInfo();
        return -1;
	}
	
	return 0;
}

#pragma mark -
void GetXMLData( void )
{
	tDirNodeReference			nodeRef				= 0;
	tDataList				   *dataList			= NULL;
	tDataBuffer				   *customBuff1			= NULL;
	tDataBuffer				   *emptyBuff			= NULL;
	tDirReference				dsRef				= 0;
	long						status				= eDSNoErr;

    status = dsOpenDirService(&dsRef);

	do
	{
		dataList = dsBuildListFromStrings( dsRef, "DSTestData", NULL );
		if (dataList == NULL) break;
		
		status = dsOpenDirNode( dsRef, dataList, &nodeRef );
		if (status != eDSNoErr)
		{
			printf( "dsOpenDirNode returned %ld\n", status );
			break;
		}

		// get data
		emptyBuff = dsDataBufferAllocate( dsRef, 1 );
		if (emptyBuff == NULL) break;
		
		customBuff1 = dsDataBufferAllocate( dsRef, 1 );
		if (customBuff1 == NULL) break;

		do
		{
			status = dsDoPlugInCustomCall( nodeRef, kReadDSTestStaticDataXMLData, emptyBuff, customBuff1 );
			if ( status == eDSBufferTooSmall )
			{
				unsigned long buffSize = customBuff1->fBufferSize;
				dsDataBufferDeAllocate( dsRef, customBuff1 );
				customBuff1 = dsDataBufferAllocate( dsRef, buffSize*2 );
			}
		} while (status == eDSBufferTooSmall);

		if (status != eDSNoErr)
		{
			printf( "dsDoPlugInCustomCall returned %ld\n", status );
			break;
		}
	} while ( false );

	printf( customBuff1->fBufferData );
	
	if (emptyBuff != NULL)
	{
		dsDataBufferDeAllocate( dsRef, emptyBuff );
		emptyBuff = NULL;
	}
	if (customBuff1 != NULL)
	{
		dsDataBufferDeAllocate( dsRef, customBuff1 );
		customBuff1 = NULL;
	}
	
	dsCloseDirNode( nodeRef );
	dsCloseDirService( dsRef );
}

void SendReadConfigMessage( char* pathToFile )
{
	tDirNodeReference			nodeRef				= 0;
	tDataList				   *dataList			= NULL;
	tDataBuffer				   *customBuff1			= NULL;
	tDataBuffer				   *emptyBuff			= NULL;
	tDirReference				dsRef				= 0;
	long						status				= eDSNoErr;

    status = dsOpenDirService(&dsRef);

	do
	{
		dataList = dsBuildListFromStrings( dsRef, "DSTestData", NULL );
		if (dataList == NULL) break;
		
		status = dsOpenDirNode( dsRef, dataList, &nodeRef );
		if (status != eDSNoErr)
		{
			printf( "dsOpenDirNode returned %ld\n", status );
			break;
		}

		// send data
		emptyBuff = dsDataBufferAllocate( dsRef, 1 );
		if (emptyBuff == NULL) break;
		
		customBuff1 = dsDataBufferAllocate( dsRef, strlen(pathToFile)+1 );
		if (customBuff1 == NULL) break;

		strcpy( customBuff1->fBufferData, pathToFile );
		
		do
		{
			status = dsDoPlugInCustomCall( nodeRef, kReadDSTestStaticDataFromFile, customBuff1, emptyBuff );
			if ( status == eDSBufferTooSmall )
			{
				unsigned long buffSize = customBuff1->fBufferSize;
				dsDataBufferDeAllocate( dsRef, customBuff1 );
				customBuff1 = dsDataBufferAllocate( dsRef, buffSize*2 );
			}
		} while (status == eDSBufferTooSmall);

		if (status != eDSNoErr)
		{
			printf( "dsDoPlugInCustomCall returned %ld\n", status );
			break;
		}
		else
			printf( "-readConfig successful\n" );
	} while ( false );

	if (emptyBuff != NULL)
	{
		dsDataBufferDeAllocate( dsRef, emptyBuff );
		emptyBuff = NULL;
	}
	if (customBuff1 != NULL)
	{
		dsDataBufferDeAllocate( dsRef, customBuff1 );
		customBuff1 = NULL;
	}	
	
	dsCloseDirNode( nodeRef );
	dsCloseDirService( dsRef );
}

void SendXMLData( CFDataRef xmlData, unsigned long customCode )
{
	if ( xmlData )
	{
		tDirNodeReference			nodeRef				= 0;
		tDataList				   *dataList			= NULL;
		tDataBuffer				   *customBuff1			= NULL;
		tDataBuffer				   *emptyBuff			= NULL;
		tDirReference				dsRef				= 0;
		long						status				= eDSNoErr;
		CFRange						aRange;
	
		status = dsOpenDirService(&dsRef);
	
		do
		{
			dataList = dsBuildListFromStrings( dsRef, "DSTestData", NULL );
			if (dataList == NULL) break;
			
			status = dsOpenDirNode( dsRef, dataList, &nodeRef );
			if (status != eDSNoErr)
			{
				printf( "dsOpenDirNode returned %ld\n", status );
				break;
			}
	
			// send data
			emptyBuff = dsDataBufferAllocate( dsRef, 1 );
			if (emptyBuff == NULL) break;
			
			customBuff1 = dsDataBufferAllocate( dsRef, CFDataGetLength(xmlData) );
			if (customBuff1 == NULL) break;
	
			aRange.location = 0;
			aRange.length = CFDataGetLength(xmlData);
			if ( customBuff1->fBufferSize >= (unsigned int)aRange.length )
			{
				CFDataGetBytes( xmlData, aRange, (UInt8*)(customBuff1->fBufferData) );
				customBuff1->fBufferLength = aRange.length;
				
				do
				{
					status = dsDoPlugInCustomCall( nodeRef, customCode, customBuff1, emptyBuff );
					if ( status == eDSBufferTooSmall )
					{
						unsigned long buffSize = customBuff1->fBufferSize;
						dsDataBufferDeAllocate( dsRef, customBuff1 );
						customBuff1 = dsDataBufferAllocate( dsRef, buffSize*2 );
					}
				} while (status == eDSBufferTooSmall);
		
				if (status != eDSNoErr)
				{
					printf( "dsDoPlugInCustomCall returned %ld\n", status );
					break;
				}
				else
					printf( "-sendXMLData successful\n" );
			}
			else
				printf( "could not allocate enough buffer space to send data to client (have:%ld, need:%ld)\n", customBuff1->fBufferSize, aRange.length );
				
		} while ( false );
	
		if (emptyBuff != NULL)
		{
			dsDataBufferDeAllocate( dsRef, emptyBuff );
			emptyBuff = NULL;
		}
		if (customBuff1 != NULL)
		{
			dsDataBufferDeAllocate( dsRef, customBuff1 );
			customBuff1 = NULL;
		}	
	
		dsCloseDirNode( nodeRef );
		dsCloseDirService( dsRef );
	}
}

CFDataRef	CopyXMLDataFromFile( char* pathToFile )
{
	sInt32					siResult					= eDSNoErr;
	CFDataRef   			xmlData						= NULL;
	CFStringRef				pathToFileRef				= CFStringCreateWithCString( NULL, pathToFile, kCFStringEncodingUTF8 );
	
//	printf( "CNBPPlugin::SaveNewStateFromXML called\n" );
	
	if ( pathToFileRef )
	{
		CFURLRef configFileURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, pathToFileRef, kCFURLPOSIXPathStyle, false );
		
		if ( !CFURLCreateDataAndPropertiesFromResource(
														kCFAllocatorDefault,
														configFileURL,
														&xmlData,          // place to put file data
														NULL,           
														NULL,
														&siResult) )
		{
			printf( "Could not create Data XML from file [%s]: %ld\n", pathToFile, siResult );
		}
	}
	
	return xmlData;
}

void DoAutoConfig1( int numNodes, int numServices )
{
	// basically create a hierarchy with a list of nodes, each node having a bunch of services
	CFMutableDictionaryRef		nodeHierarchyRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	
	if ( nodeHierarchyRef )
	{
//		CFMutableArrayRef		nodeArray = CFArrayCreateMutable( NULL, numNodes, &kCFTypeArrayCallBacks );
//		
//		for ( CFIndex index=0; index<numNodes; index++ )
//		{
//			CFStringRef					nodeNameRef = CFStringCreateWithFormat( NULL, NULL, CFSTR("Building %d"), index+1 );
//			CFMutableDictionaryRef		nodeRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );			
//			CFMutableDictionaryRef		servicesRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
//			
//			// now that we have the node container, lets add some services to it
//			for ( CFIndex serviceIndex=0; serviceIndex<numServices; serviceIndex++ )
//			{
//				CFStringRef				serviceNameRef = CFStringCreateWithFormat( NULL, NULL, CFSTR("Building %d, Server %d"), index+1, serviceIndex+1 );
//				CFMutableDictionaryRef	serviceRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
//				
//				CFDictionaryAddValue( serviceRef, CFSTR(kDSNAttrRecordName), serviceNameRef );
//				CFDictionaryAddValue( serviceRef, CFSTR(kDS1AttrServiceType), CFSTR("afp") );
//				CFDictionaryAddValue( serviceRef, CFSTR(kDSNAttrURL), CFSTR("afp://127.0.0.1") );
//				
//				CFDictionaryAddValue( servicesRef, serviceNameRef, serviceRef );		// add the service to the servicesRef with the name as the key
//				
//				CFRelease( serviceNameRef );
//				CFRelease( serviceRef );
//			}
//
//			CFDictionaryAddValue( nodeRef, CFSTR(kDSStdRecordTypeServices), servicesRef );
//			CFDictionaryAddValue( nodeRef, CFSTR(kDSNAttrRecordName), nodeNameRef );
//			
//			CFArrayAppendValue( nodeArray, nodeRef );
//
//			CFRelease( servicesRef );
//			CFRelease( nodeNameRef );
//			CFRelease( nodeRef );
//		}
//
//		CFDictionaryAddValue( nodeHierarchyRef, CFSTR("dsRecTypeNative:Nodes"), nodeArray );
//		CFRelease( nodeArray );
		CFStringRef		numNodesRef = CFStringCreateWithFormat( NULL, NULL, CFSTR("%d"), numNodes );
		CFStringRef		numServicesRef = CFStringCreateWithFormat( NULL, NULL, CFSTR("%d"), numServices );

		CFDictionaryAddValue( nodeHierarchyRef, CFSTR("kNumNeighborhoods"), numNodesRef );
		CFDictionaryAddValue( nodeHierarchyRef, CFSTR("kNumServices"), numServicesRef );
		CFDataRef xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, nodeHierarchyRef );

		SendXMLData( xmlData, kOnTheFlySetup );
		
		CFRelease( numNodesRef );
		CFRelease( numServicesRef );
		CFRelease( xmlData );
		CFRelease( nodeHierarchyRef );
	}
	else
		printf( "unable to create hierarchy, abort\n" );
}

void DoAutoConfig2( void )
{
	// basically create a hierarchy with a list of nodes, each node having a bunch of services
	CFMutableDictionaryRef		nodeHierarchyRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	
	if ( nodeHierarchyRef )
	{
		BuildNetworkNodeList( nodeHierarchyRef );
/*		
		CFDictionaryAddValue( nodeHierarchyRef, CFSTR("dsRecTypeNative:Nodes"), nodeArray );
		CFRelease( nodeArray );
*/		
		CFDataRef xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, nodeHierarchyRef );

		SendXMLData( xmlData, kWriteDSTestStaticDataXMLData );
		
		CFRelease( xmlData );
		CFRelease( nodeHierarchyRef );
	}
	else
		printf( "unable to create hierarchy, abort\n" );
}


tDirStatus AddNewNode( CFStringRef nodePath, CFStringRef pathDelimiter )
{
	tDirNodeReference			nodeRef				= 0;
	tDataList				   *dataList			= NULL;
	tDataBuffer				   *customBuff1			= NULL;
	tDataBuffer				   *emptyBuff			= NULL;
	tDirReference				dsRef				= 0;
	tDirStatus					status				= eDSNoErr;
	
	CFArrayRef					newNodePathRef		= CFStringCreateArrayBySeparatingStrings( NULL, nodePath, pathDelimiter);	
	CFDataRef					xmlData				= NULL;
	
	if ( newNodePathRef )
		xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, newNodePathRef );
	
	if ( !xmlData )
		return eDSNullParameter;
	
	status = dsOpenDirService(&dsRef);

	do
	{
		dataList = dsBuildListFromStrings( dsRef, "DSTestData", NULL );
		if (dataList == NULL) break;
		
		status = dsOpenDirNode( dsRef, dataList, &nodeRef );
		if (status != eDSNoErr)
		{
			printf( "dsOpenDirNode returned %d\n", status );
			break;
		}

		emptyBuff = dsDataBufferAllocate( dsRef, 1 );
		if (emptyBuff == NULL) break;
		
		customBuff1 = dsDataBufferAllocate( dsRef, CFDataGetLength(xmlData) );
		if (customBuff1 == NULL) break;

		CFRange						aRange;
		aRange.location = 0;
		aRange.length = CFDataGetLength(xmlData);
		if ( customBuff1->fBufferSize >= (unsigned int)aRange.length )
		{
			CFDataGetBytes( xmlData, aRange, (UInt8*)(customBuff1->fBufferData) );
			customBuff1->fBufferLength = aRange.length;
			
			do
			{
				status = dsDoPlugInCustomCall( nodeRef, kAddNewNode, customBuff1, emptyBuff );
				if ( status == eDSBufferTooSmall )
				{
					unsigned long buffSize = customBuff1->fBufferSize;
					dsDataBufferDeAllocate( dsRef, customBuff1 );
					customBuff1 = dsDataBufferAllocate( dsRef, buffSize*2 );
				}
			} while (status == eDSBufferTooSmall);
	
		}
		
		if (status != eDSNoErr)
		{
			printf( "dsDoPlugInCustomCall returned %d\n", status );
			break;
		}
	} while ( false );
	
	if (emptyBuff != NULL)
	{
		dsDataBufferDeAllocate( dsRef, emptyBuff );
		emptyBuff = NULL;
	}
	if (customBuff1 != NULL)
	{
		dsDataBufferDeAllocate( dsRef, customBuff1 );
		customBuff1 = NULL;
	}
	
	dsCloseDirNode( nodeRef );
	dsCloseDirService( dsRef );
	
	return status;
}

void AddTopLevelNode( char* nodeNameToAdd )
{
	tDirNodeReference			nodeRef				= 0;
	tDataList				   *dataList			= NULL;
	tDataBuffer				   *customBuff1			= NULL;
	tDataBuffer				   *emptyBuff			= NULL;
	tDirReference				dsRef				= 0;
	long						status				= eDSNoErr;

    status = dsOpenDirService(&dsRef);

	do
	{
		dataList = dsBuildListFromStrings( dsRef, "DSTestData", NULL );
		if (dataList == NULL) break;
		
		status = dsOpenDirNode( dsRef, dataList, &nodeRef );
		if (status != eDSNoErr)
		{
			printf( "dsOpenDirNode returned %ld\n", status );
			break;
		}

		emptyBuff = dsDataBufferAllocate( dsRef, 1 );
		if (emptyBuff == NULL) break;
		
		customBuff1 = dsDataBufferAllocate( dsRef, strlen(nodeNameToAdd)+1 );
		if (customBuff1 == NULL) break;

		strcpy( customBuff1->fBufferData, nodeNameToAdd );
		
		do
		{
			status = dsDoPlugInCustomCall( nodeRef, kAddNewTopLevelNode, customBuff1, emptyBuff );
			if ( status == eDSBufferTooSmall )
			{
				unsigned long buffSize = customBuff1->fBufferSize;
				dsDataBufferDeAllocate( dsRef, customBuff1 );
				customBuff1 = dsDataBufferAllocate( dsRef, buffSize*2 );
			}
		} while (status == eDSBufferTooSmall);

		if (status != eDSNoErr)
		{
			printf( "dsDoPlugInCustomCall returned %ld\n", status );
			break;
		}
	} while ( false );
	
	if (emptyBuff != NULL)
	{
		dsDataBufferDeAllocate( dsRef, emptyBuff );
		emptyBuff = NULL;
	}
	if (customBuff1 != NULL)
	{
		dsDataBufferDeAllocate( dsRef, customBuff1 );
		customBuff1 = NULL;
	}
	
	dsCloseDirNode( nodeRef );
	dsCloseDirService( dsRef );
}

void AddTestServiceToTopLevelNode( char* nodeNameToAdd )
{
    CFMutableDictionaryRef		serviceRef = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	CFStringRef					locationRef = CFStringCreateWithCString( NULL, nodeNameToAdd, kCFStringEncodingUTF8 );
	
	CFDictionaryAddValue( serviceRef, CFSTR(kDS1AttrServiceType), CFSTR(kDSStdRecordTypeAFPServer) );
	CFDictionaryAddValue( serviceRef, CFSTR(kDSNAttrRecordName), CFSTR("Test Registration Machine") );
	CFDictionaryAddValue( serviceRef, CFSTR(kDSNAttrURL), CFSTR("afp://test.local") );
	CFDictionaryAddValue( serviceRef, CFSTR(kDS1AttrComment), CFSTR("How's the weather?") );

	tDataListPtr nodeToAddRecordTo = dsBuildListFromStrings( 0, "DSTestData", nodeNameToAdd, NULL );

	AddServiceToNodePath( nodeToAddRecordTo, serviceRef );
	
	CFRelease( locationRef );
	CFRelease( serviceRef );
					
	if ( nodeToAddRecordTo != NULL )
		(void)dsDataListDeAllocate( 0, nodeToAddRecordTo, FALSE );
	nodeToAddRecordTo = NULL;
}

#pragma mark -
void ConvertCFStringToDSList( tDirReference inDirRef, CFStringRef inString, tDataListPtr *outDataList )
{
    char nodeStr[512];		// CFStringGetLength returns possibly 16 bit values!
    
    *outDataList = NULL;
    
    if ( inString && ::CFStringGetCString( inString, nodeStr, sizeof(nodeStr), kCFStringEncodingUTF8 ) )
    {
    	*outDataList = dsBuildFromPath( inDirRef, nodeStr, kDSDelimiter );
    }
}

void ConvertCFStringToDSDataNode( tDirReference inDirRef, CFStringRef inString, tDataNodePtr *outDataNode )
{
    char nodeStr[512];		// CFStringGetLength returns possibly 16 bit values!
    
    *outDataNode = NULL;
    
    if ( inString && ::CFStringGetCString( inString, nodeStr, sizeof(nodeStr), kCFStringEncodingUTF8 ) )
    {
    	*outDataNode = dsDataNodeAllocateString( inDirRef, nodeStr );
    }
}

tDirStatus GetDSRecordTypeList( tDirReference inDirRef, char* serviceStr, tDataListPtr *outRecTypes );
tDirStatus GetDSRecordTypeNode( tDirReference inDirRef, char* serviceStr, tDataNodePtr *outRecTypes );
/******************************
 * GetDSRecordTypeNodeFromURL *
 ******************************

Maps the services in the service type Node to DS Record Types.
*/

tDirStatus GetDSRecordTypeNodeFromURL( tDirReference inDirRef, CFStringRef urlRef, tDataNodePtr *outRecTypes )
{
    unsigned char		serviceType[256];
    CFIndex		ignore;
    char*		colonPtr = NULL;
	
	if ( urlRef )
	{
		CFStringGetBytes( urlRef, CFRangeMake(0,255), kCFStringEncodingUTF8, 0, false, serviceType, 255, &ignore );
		serviceType[ignore] = '\0';
		
		colonPtr = strstr( (char*)serviceType, ":" );
		
		if ( colonPtr )
		{
			*colonPtr = '\0';		// replace this with a null
			
			return GetDSRecordTypeNode( inDirRef, (char*)serviceType, outRecTypes );
		}
		else
			return (tDirStatus) -1;
	}
	else
		return eDSNullParameter;
}

/***********************
 * GetDSRecordTypeNode *
 ***********************

Maps the services in the service type Node to DS Record Types.
*/

tDirStatus GetDSRecordTypeNode( tDirReference inDirRef, CFStringRef serviceToSearchRef, tDataNodePtr *outRecTypes )
{
    char serviceStr[256];
    
    if ( !serviceToSearchRef )
        return (tDirStatus) -1;
    
    if ( ! ::CFStringGetCString( serviceToSearchRef, serviceStr, sizeof(serviceStr), CFStringGetSystemEncoding() ) )
        return (tDirStatus) -1;
    
    return GetDSRecordTypeNode( inDirRef, serviceStr, outRecTypes );
}

tDirStatus GetDSRecordTypeNode( tDirReference inDirRef, char* serviceStr, tDataNodePtr *outRecTypes )
{
	tDirStatus status = eDSNoErr;
    char *recordTypeStr = NULL;

    // map the url service type to a DS type
    if ( strcmp( serviceStr, "http" ) == 0 || strcmp( serviceStr, "https" ) == 0 )
        recordTypeStr = kDSStdRecordTypeWebServer;
    else
    if ( strcmp( serviceStr, "ftp" ) == 0 )
        recordTypeStr = kDSStdRecordTypeFTPServer;
    else
    if ( strcmp( serviceStr, "afp" ) == 0 || strcmp( serviceStr, "AFPServer" ) == 0 )
        recordTypeStr = kDSStdRecordTypeAFPServer;
    else
    if ( strcmp( serviceStr, "ldap" ) == 0 )
        recordTypeStr = kDSStdRecordTypeLDAPServer;
/*    else
    if ( strcmp( serviceStr, "lpr" ) == 0 || strcmp( serviceStr, "LaserWriter" ) == 0 )
        recordTypeStr = kDSStdRecordTypePrinters;
*/    else
    if ( strcmp( serviceStr, "nfs" ) == 0 )
        recordTypeStr = kDSStdRecordTypeNFS;
    else
    if ( strcmp( serviceStr, "smb" ) == 0 || strcmp( serviceStr, "cifs" ) == 0 )
        recordTypeStr = kDSStdRecordTypeSMBServer;
    /*
    else
    if ( strcmp( serviceStr, "gopher" ) == 0 )
        recordTypeStr = ???;
    else
    if ( strcmp( serviceStr, "mailto" ) == 0 )
        recordTypeStr = ???;
    else
    if ( strcmp( serviceStr, "news" ) == 0 || strcmp( serviceStr, "nntp" ) == 0 )
        recordTypeStr = ???;
    */
    else
    {
        // no std types, we want to map to a native type
        // i.e. "dsRecTypeNative:unknownServiceType"
        char	cTemp[256]={0};
        
        strcpy( cTemp, kDSNativeRecordTypePrefix );
        strcat( cTemp, serviceStr );
        strcpy( serviceStr, cTemp );
        
        recordTypeStr = serviceStr;
    }
    
    if ( ! recordTypeStr )
        return (tDirStatus) -1;
    
    // the DirectoryService API function names are misleading.  dsBuildNodeFromStringsAlloc doesn't
    // actually alloc so we need to do it manually.
   	*outRecTypes = dsDataNodeAllocateString( inDirRef, recordTypeStr );
    // I don't think care any more about specialized "NSL" url stuff...?  Shouldn't we be standardizing?	// KA 11/15/01
//    status = dsBuildNodeFromStringsAlloc( mDirRef, *outRecTypes, recordTypeStr, kDSNAttrURLForNSL, NULL );
    
    return status;
}

void AddServiceAttributes( const void* key, const void* value, void* context );

struct AddServicesAttributesContext {
    tRecordReference		recRef;
    tDirReference			dirRef;
	tDirStatus				status;
};


void AddServiceAttributes( const void* key, const void* value, void* context )
{
    if ( key && value && context )
    {
        AddServicesAttributesContext*	ourContext = (AddServicesAttributesContext*)context;
		
		if ( ourContext->status == eDSNoErr )
		{
			CFStringRef					keyRef = (CFStringRef) key;
			CFPropertyListRef			valueRef = (CFPropertyListRef) value;
			
			tDataNodePtr				inNewAttributeType;
			tDataNodePtr				inNewAttributeValue;
	
			ConvertCFStringToDSDataNode( 0, keyRef, &inNewAttributeType );
	
			if ( valueRef && ::CFGetTypeID( valueRef ) == ::CFArrayGetTypeID() )
			{
				CFIndex		valueCount = CFArrayGetCount((CFArrayRef)valueRef);
				
				for ( CFIndex i=1; i<valueCount; i++ )
				{
					CFStringRef			valueStringRef = (CFStringRef)::CFArrayGetValueAtIndex( (CFArrayRef)valueRef, i );
					
					ConvertCFStringToDSDataNode( ourContext->dirRef, (CFStringRef)valueStringRef, &inNewAttributeValue );
					
					ourContext->status = dsAddAttributeValue( ourContext->recRef, inNewAttributeType, inNewAttributeValue );
					printf( "AddServiceAttributes, dsAddAttributeValue returned: %d\n", ourContext->status );
					
					dsDataNodeDeAllocate( ourContext->dirRef, inNewAttributeValue );
				}
			}
			else if ( valueRef && ::CFGetTypeID( valueRef ) == ::CFStringGetTypeID() )
			{
				ConvertCFStringToDSDataNode( ourContext->dirRef, (CFStringRef)valueRef, &inNewAttributeValue );
				
				printf( "AddServiceAttributes, adding type: [%s], value: [%s]\n", inNewAttributeType->fBufferData, inNewAttributeValue->fBufferData );	
	
				ourContext->status = dsAddAttributeValue( ourContext->recRef, inNewAttributeType, inNewAttributeValue );
				printf( "AddServiceAttributes, dsAddAttributeValue returned: %d\n", ourContext->status );
				
				dsDataNodeDeAllocate( ourContext->dirRef, inNewAttributeValue );
			}
			else
				printf( "AddServiceAttributes, valueRef isn't what we were expecting, CFGetTypeID=%ld",::CFGetTypeID( valueRef ) );
		}
    }
}

tDirStatus AddServiceToNodePath( tDataListPtr nodeToAddRecordTo, CFMutableDictionaryRef serviceRef )
{
	tDirNodeReference	dirNodeRef;
	tDirReference 		mDirRef;
	tDirStatus			lookupStatus;
	
	lookupStatus = dsOpenDirService( &mDirRef );
	
	if ( lookupStatus == eDSNoErr )
	{
		lookupStatus = dsOpenDirNode( mDirRef, nodeToAddRecordTo, &dirNodeRef );
		
		printf( "dsOpenDirNode on %s, status = %d \n", dsGetPathFromList(mDirRef,nodeToAddRecordTo, kDSDelimiter ), lookupStatus );	
		if ( lookupStatus == eDSNoErr )
		{
			lookupStatus = AddServiceToOpenNode( mDirRef, dirNodeRef, serviceRef );
			
			dsCloseDirNode( dirNodeRef );
		}
				
		dsCloseDirService( mDirRef );
	}
	
	return lookupStatus;
}

tDirStatus AddServiceToOpenNode( tDirReference mDirRef, tDirNodeReference dirNodeRef, CFMutableDictionaryRef serviceRef )
{
	tDirStatus			lookupStatus = eDSNoErr;
    CFStringRef			serviceTypeRef = (CFStringRef)::CFDictionaryGetValue( serviceRef, CFSTR(kDS1AttrServiceType) );
    CFStringRef			serviceNameRef = (CFStringRef)::CFDictionaryGetValue( serviceRef, CFSTR(kDSNAttrRecordName) );

	try
	{
		lookupStatus = eDSNoErr;
        
		if ( lookupStatus == eDSNoErr )
		{
			tDataNodePtr recordName;
			tDataNodePtr recordType;
			tRecordReference userRecRef;
			
			ConvertCFStringToDSDataNode( mDirRef, serviceNameRef, &recordName );
			ConvertCFStringToDSDataNode( mDirRef, serviceTypeRef, &recordType );
			
			// Create a new record
			if ( !lookupStatus )
			{
				lookupStatus = dsCreateRecordAndOpen( dirNodeRef, recordType, recordName, &userRecRef );
				printf( "dsCreateRecordAndOpen on recordType:%s, recordName:%s, userRecRef:%ld, status = %d \n", recordType->fBufferData, recordName->fBufferData, userRecRef, lookupStatus );	
			}
				
			if ( !lookupStatus )
			{
				AddServicesAttributesContext	ourContext = {userRecRef,mDirRef};
				::CFDictionaryApplyFunction( serviceRef, AddServiceAttributes, &ourContext );
				
				lookupStatus = dsFlushRecord( userRecRef );
				printf( "dsFlushRecord returned: %d\n", lookupStatus );
				
				lookupStatus = dsCloseRecord( userRecRef );
				printf( "dsCloseRecord returned: %d\n", lookupStatus );
				
				lookupStatus = eDSNoErr;	// suppress since we may need to try other nodes
			}
			
			// clean up
			dsDataNodeDeAllocate( mDirRef, recordType );
			dsDataNodeDeAllocate( mDirRef, recordName );
		}
		else
		{
			printf( "dsGetDirNodeName, status = %d \n", lookupStatus );
			lookupStatus = eDSNoErr;		// temporarily suppress until things are working better.
		}
	}
	catch ( tDirStatus inErr )
	{
		lookupStatus = inErr;
	}
	
	return lookupStatus;
}

#pragma mark -
tDirStatus BuildNetworkNodeList( CFMutableDictionaryRef nodeHierarchyRef )
{
    tDirStatus				lookupStatus = eDSNoErr;
    unsigned long			nodeCount;
    tDataBufferPtr			nodeNamesPtr = NULL;
    tContextData			continueData = NULL;
	unsigned long			nameBufferSize = 1024;
	tDirReference 			mDirRef;		
	
	lookupStatus = dsOpenDirService( &mDirRef );
    
	nodeNamesPtr = dsDataBufferAllocate( mDirRef, nameBufferSize );
	
	if (!nodeNamesPtr)
	{
		CFRelease( nodeHierarchyRef );
		return eDSAllocationFailed;
	}	
/*	
	// continueData loop, get default network nodes if we aren't showing protocols
	do
	{
		nodeCount = 0;

		// dsFindDirNodes has not implemented continueData so we need to check for eDSBufferTooSmall and reallocate
		do
		{
			lookupStatus = dsFindDirNodes( mDirRef,
											nodeNamesPtr,
											namePattern,
											eDSDefaultNetworkNodes,
											&nodeCount,
											&continueData );

			if ( lookupStatus == eDSBufferTooSmall )
			{
				(void)dsDataBufferDeAllocate( mDirRef, nodeNamesPtr );
				
				nameBufferSize *= 2;
				nodeNamesPtr = dsDataBufferAllocate( mDirRef, nameBufferSize );

				if ( nodeNamesPtr == NULL )
					break;
			}
		}
		while ( lookupStatus == eDSBufferTooSmall );
		
		printf( "BuildNetworkNodeList lookupStatus = %d, count = %ld, continueData = %lx\n",
						lookupStatus, nodeCount, (UInt32)continueData );
		
		if ( lookupStatus == eDSNoErr )
		{
			lookupStatus = BuildNeighborhoodNodeTree( mDirRef, true, nodeNamesPtr, nodeCount, nodeHierarchyRef );

			if ( lookupStatus )
				printf( "BuildNeighborhoodNodeTree, status = %d \n", lookupStatus );
		}
	}
	while ( continueData != NULL );

	// free any leftover search data
	if ( continueData != NULL )
		lookupStatus = dsReleaseContinueData( mDirRef, continueData );
*/	
	// continueData loop, get ds nodes
	do
	{
		nodeCount = 0;

		do
		{
			lookupStatus = dsGetDirNodeList( mDirRef, nodeNamesPtr, &nodeCount, &continueData );

			if ( lookupStatus == eDSBufferTooSmall )
			{
				(void)dsDataBufferDeAllocate( mDirRef, nodeNamesPtr );
				
				nameBufferSize *= 2;
				nodeNamesPtr = dsDataBufferAllocate( mDirRef, nameBufferSize );

				if ( nodeNamesPtr == NULL )
					break;
			}
		}
		while ( lookupStatus == eDSBufferTooSmall );
		
		printf( "BuildNetworkNodeList lookupStatus = %d, count = %ld, continueData = %lx\n",
						lookupStatus, nodeCount, (UInt32)continueData );
		
		if ( lookupStatus == eDSNoErr )
		{
			lookupStatus = BuildNeighborhoodNodeTree( mDirRef, false, nodeNamesPtr, nodeCount, nodeHierarchyRef );

			if ( lookupStatus )
				printf( "BuildNeighborhoodNodeTree, status = %d \n", lookupStatus );
		}
	}
	while ( continueData != NULL );

	// free any leftover search data
	if ( continueData != NULL )
		lookupStatus = dsReleaseContinueData( mDirRef, continueData );

	lookupStatus = dsDataBufferDeAllocate( mDirRef, nodeNamesPtr );
	
	dsCloseDirService( mDirRef );
	
    return lookupStatus;
}

/*****************************
 * BuildNeighborhoodNodeTree *
 *****************************
 
 Given a buffer of Directory Service nodes, build a dictionary with Neighborhoods.

    isLocalNodeSearch			 ->	are we parsing the default network nodes or not
    nodeNamesPtr				 ->	a buffer full of nodes
    nodeCount					 ->	the number of nodes in <nodeNamesPtr>
    nodeNeighborhoodTree		<->	a dictionary with keys being the parent node path and values being an array of neighborhoods
*/

tDirStatus BuildNeighborhoodNodeTree(	
										tDirReference mDirRef,
										Boolean isLocalNodeSearch,
										tDataBufferPtr nodeNamesPtr,
										UInt32 nodeCount,
										CFMutableDictionaryRef nodeNeighborhoodTree )
{
    tDirStatus lookupStatus = eDSNoErr;
    tDataListPtr curNodeNameList = NULL;
    tDataNodePtr nodePtr = NULL;
	
	if ( !nodeNamesPtr || !nodeNeighborhoodTree )
		return eDSAllocationFailed;
	
	CFMutableArrayRef	nodeListRef = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	
	CFDictionaryAddValue( nodeNeighborhoodTree, CFSTR("dsRecTypeNative:Nodes"), nodeListRef );
	
	for ( UInt32 nodeIndex = 1; nodeIndex <= nodeCount; nodeIndex++ )	// one based access to DS
	{
        CFMutableStringRef		parentNodePath = CFStringCreateMutableCopy( NULL, 0, CFSTR("") );					// no parent yet
		
		if ( !parentNodePath )
			lookupStatus = eDSAllocationFailed;
			
		if ( lookupStatus == eDSNoErr )
			lookupStatus = dsGetDirNodeName( mDirRef, nodeNamesPtr, nodeIndex, &curNodeNameList );

		if ( lookupStatus == eDSNoErr )
		{
			UInt32				nodePiecesCount = dsDataListGetNodeCount( curNodeNameList );
			
			if ( nodePiecesCount <= 1 )
				continue;					// we don't want to deal with nodes that are top level only
			
			lookupStatus = dsDataListGetNodeAlloc( mDirRef, curNodeNameList, 1, &nodePtr );
			
			if ( nodePtr->fBufferData && ( strcmp( nodePtr->fBufferData, "DSTestData" ) == 0 || strcmp( nodePtr->fBufferData, "NetInfo" ) == 0 ) )
			{
				if ( nodePtr )
					dsDataNodeDeAllocate( mDirRef, nodePtr );
				nodePtr = NULL;	

				continue;		// skip our own nodes
			}
				
			if ( nodePtr )
				dsDataNodeDeAllocate( mDirRef, nodePtr );
			nodePtr = NULL;	

			lookupStatus = dsDataListGetNodeAlloc( mDirRef, curNodeNameList, 2, &nodePtr );

			if ( lookupStatus == eDSNoErr && nodePtr != NULL )
			{
				CFStringRef		curNodePieceName = NULL;
				
				curNodePieceName = CFStringCreateWithCString( NULL, nodePtr->fBufferData, kCFStringEncodingUTF8 );
				
				if ( curNodePieceName )
				{
					CFMutableDictionaryRef		curNeighborhood = NULL;
					CFMutableDictionaryRef		serviceList = NULL;
					
					curNeighborhood = FindExistingNeighborhood( nodeListRef, curNodePieceName );
					
					if ( curNeighborhood )
					{
						CFRetain(curNeighborhood);

						serviceList = (CFMutableDictionaryRef)CFDictionaryGetValue( curNeighborhood, CFSTR("dsRecTypeStandard:Services") );
					}
					else
					{
						curNeighborhood = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
						CFDictionaryAddValue( curNeighborhood, CFSTR("dsAttrTypeStandard:RecordName"), curNodePieceName );
						CFArrayAppendValue( nodeListRef, curNeighborhood );
					}
						
					if ( serviceList)
					{
						CFRetain(serviceList);
					}	
					else
					{
						serviceList = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
						CFDictionaryAddValue( curNeighborhood, CFSTR("dsRecTypeStandard:Services"), serviceList );
					}

					FillOutServiceListWithServices( mDirRef, serviceList, curNodeNameList );
					
					CFRelease( serviceList );
					CFRelease( curNeighborhood );
				}
				
				if ( nodePtr )
					dsDataNodeDeAllocate( mDirRef, nodePtr );
				nodePtr = NULL;	
			}
		}

		if ( curNodeNameList )
		{
			lookupStatus = dsDataListDeallocate( mDirRef, curNodeNameList );
			free( curNodeNameList );
		}
		curNodeNameList = NULL;
	
		if ( parentNodePath )
			CFRelease( parentNodePath );
		parentNodePath = NULL;
	}

    if ( curNodeNameList )
	{
		lookupStatus = dsDataListDeallocate( mDirRef, curNodeNameList );	
		free( curNodeNameList );		
	}
	curNodeNameList = NULL;
	
    return lookupStatus;
}

CFMutableDictionaryRef FindExistingNeighborhood( CFMutableArrayRef nodeList, CFStringRef nodeName )
{
	CFMutableDictionaryRef		foundRef = NULL;
	
	for ( CFIndex i=CFArrayGetCount(nodeList); i>0; i-- )
	{
		if ( CFStringCompare( nodeName, (CFStringRef)CFDictionaryGetValue( (CFDictionaryRef)CFArrayGetValueAtIndex(nodeList,i-1), CFSTR("dsAttrTypeStandard:RecordName") ), 0 ) == kCFCompareEqualTo )
		{
			foundRef = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(nodeList,i-1);
			break;
		}
	}
	
	return foundRef;
}

#pragma mark -
tDirStatus FillOutServiceListWithServices( tDirReference mDirRef, CFMutableDictionaryRef serviceList, tDataListPtr curNodeNameList )
{
    tDirStatus				lookupStatus = eDSNoErr;
    tDirNodeReference		dirNodeRef;
	tDataBufferPtr			matchingNodesNamesPtr = dsDataBufferAllocate( mDirRef, 1024 );
	tDataBufferPtr			nodeNamesPtr = dsDataBufferAllocate( mDirRef, 1024 );
	
	lookupStatus = dsOpenDirNode( mDirRef, curNodeNameList, &dirNodeRef );

	if ( lookupStatus == eDSNoErr )
	{                
		NSLXDSNodeDataContext*		newDirContext = (NSLXDSNodeDataContext*)calloc(1,sizeof(NSLXDSNodeDataContext));
		
		if ( newDirContext )
		{
			newDirContext->fDirNodeRef = dirNodeRef;

			newDirContext->fIsBonjourLocalNode = false;
			
			lookupStatus = SearchDirNodeForURLData( mDirRef, serviceList, newDirContext );
		}
		
		dsCloseDirNode( dirNodeRef );
	}
	else
	{
		printf( "NSLXDSLookup::DoServicesLookup, dsOpenDirNode, status = %d \n", lookupStatus );
	}
		
	if ( matchingNodesNamesPtr )
		dsDataBufferDeAllocate( mDirRef, matchingNodesNamesPtr );
    
	if ( nodeNamesPtr )
	{
		dsDataBufferDeAllocate( mDirRef, nodeNamesPtr );
		nodeNamesPtr = NULL;
	}

	return lookupStatus;
}

tDirStatus SearchDirNodeForURLData( tDirReference mDirRef, CFMutableDictionaryRef serviceList, NSLXDSNodeDataContext* nodeContext )
{
    tDirStatus status;
    tDataBufferPtr dataBuffPtr = NULL;
    tDataListPtr attrListAll = NULL;
    //tDataListPtr urlAttrList = NULL;
    tDataListPtr recNames = NULL;
    unsigned long recIndex, recEntryCount;
    unsigned long attrIndex, attrCount;
    unsigned long attrValueIndex, attrValueCount;
    tAttributeListRef attributeListRef;
    tRecordEntryPtr recEntryPtr;
    tAttributeValueListRef attrValueListRef;
    tAttributeEntryPtr attrInfoPtr;
    tAttributeValueEntryPtr	attrValue;
	unsigned long recordBufferSize = 1024;

    // Return info
    CFMutableDictionaryRef	resultDictionary = NULL;
    CFStringRef				keyRef = NULL;
    CFStringRef				valueRef = NULL;
    CFMutableArrayRef		valueArrayRef = NULL;
    
    try
    {        
        dataBuffPtr = dsDataBufferAllocate( mDirRef, recordBufferSize );
        if ( dataBuffPtr == NULL ) {
            printf( "SearchDirNodeForURLData, dsDataBufferAllocate is NULL\n" );
            throw( -1 );
        }
        
        attrListAll = dsDataListAllocate( mDirRef );
        status = dsBuildListFromStringsAlloc( mDirRef, attrListAll, kDSAttributesAll, nil );

        recNames = dsDataListAllocate( mDirRef );
        status = dsBuildListFromStringsAlloc( mDirRef, recNames, kDSRecordsAll, nil );
        if ( status != eDSNoErr ) {
            printf( "SearchDirNodeForURLData, dsBuildListFromStringsAlloc (kDSRecordsAll) = %d\n", status );
            throw( status );
        }
        
		tDataListPtr	recordTypeList = dsDataListAllocate( mDirRef );
		status = dsAppendStringToListAlloc( mDirRef, recordTypeList, kDSStdRecordTypeAFPServer );
		status = dsAppendStringToListAlloc( mDirRef, recordTypeList, kDSStdRecordTypeSMBServer );
		status = dsAppendStringToListAlloc( mDirRef, recordTypeList, kDSStdRecordTypeFTPServer );

        do
        {
			do
			{
				recEntryCount = 0;		// set this as we want as many as we can get
				status = dsGetRecordList( nodeContext->fDirNodeRef,
										  dataBuffPtr,
										  recNames,
										  eDSExact,
										  recordTypeList,
										  attrListAll,		// all all attribute types
										  FALSE,
										  &recEntryCount,
										  &(nodeContext->fContinueData) );
            
				if ( status == eDSBufferTooSmall )
				{
					if ( recordBufferSize > 1024 * 1024 )
						break;
					
					(void)dsDataBufferDeAllocate( mDirRef, dataBuffPtr );
					
					recordBufferSize *= 2;
					dataBuffPtr = dsDataBufferAllocate( mDirRef, recordBufferSize );

					if ( dataBuffPtr == NULL )
						break;
				}
			}
			while ( status == eDSBufferTooSmall );

            if ( status == eDSNoErr && recEntryCount > 0 )
            {
                // loop through all records
                // the docs say zero-based, the code is 1-based
                printf( "SearchDirNodeForURLData, recEntryCount = %ld\n", recEntryCount );
                for ( recIndex = 1; recIndex <= recEntryCount; recIndex++ )
                {
                    Boolean recordToBeReturned = false;
                    
                    resultDictionary = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
                    if ( !resultDictionary )
                    {
                        printf( "SearchDirNodeForURLData, resultDictionary is NULL\n" );
                        throw( -1 );
                    }
                    
                    status = dsGetRecordEntry( nodeContext->fDirNodeRef, dataBuffPtr, recIndex, &attributeListRef, &recEntryPtr );
                    if ( status == eDSNoErr )
                    {
                        // loop through all attributes for the current record
                        attrCount = recEntryPtr->fRecordAttributeCount;

                
                        unsigned short	recordNameLen, recordTypeLen;
                        char*	recordNamePtr, * recordTypePtr;
                        
                        memcpy(&recordNameLen, recEntryPtr->fRecordNameAndType.fBufferData, 2 );
                        recordNamePtr = recEntryPtr->fRecordNameAndType.fBufferData + 2;
                        printf( "SearchDirNodeForURLData, recordNameLen = %d, recordNamePtr = %s\n", recordNameLen, recordNamePtr );
                        memcpy(&recordTypeLen, (recordNamePtr + recordNameLen), 2 );
                        recordTypePtr = recordNamePtr + recordNameLen + 2;
                        printf( "SearchDirNodeForURLData, recordTypeLen = %d, recordTypePtr = %s\n", recordTypeLen, recordTypePtr );
                    
                        printf( "SearchDirNodeForURLData, attrCount = %ld\n", attrCount );
                        for ( attrIndex = 1; attrIndex <= attrCount && status == eDSNoErr; attrIndex++ )
                        {
                            status = dsGetAttributeEntry( nodeContext->fDirNodeRef,
                                                          dataBuffPtr,
                                                          attributeListRef,
                                                          attrIndex,
                                                          &attrValueListRef,
                                                          &attrInfoPtr );
                            
                            if ( status == eDSNoErr )
                            {
                                char*	keyPtr = attrInfoPtr->fAttributeSignature.fBufferData;
								
								keyRef = CFStringCreateWithCString( kCFAllocatorDefault, keyPtr, kCFStringEncodingUTF8 );

                                printf( "SearchDirNodeForURLData, attrInfo name  = %s\n", keyPtr );

//                                if ( strcmp(attrInfoPtr->fAttributeSignature.fBufferData, kDSNAttrURL) == 0 )
                                {
//                                    printf( "Found a record with a URL, marking record to be returned\n" );
                                    recordToBeReturned = true;
                                }    
                                // loop through all values for the current attribute
                                attrValueCount = attrInfoPtr->fAttributeValueCount;
                                printf( "SearchDirNodeForURLData, attrValueCount = %ld\n", attrValueCount);

                                if ( attrValueCount > 1 )
                                {
                                    valueArrayRef = ::CFArrayCreateMutable( NULL, attrCount, &kCFTypeArrayCallBacks );
                                    
                                    if ( !valueArrayRef )
                                    {
                                        printf( "SearchDirNodeForURLData, valueArrayRef is NULL!\n" );
                                        throw( -1 );
                                    }
                                }
                                else
                                {
                                    // must be set to NULL each time through the loop
                                    // so it does not get reused.
                                    valueArrayRef = NULL;
                                }
                                
                                for ( attrValueIndex = 1; attrValueIndex <= attrValueCount; attrValueIndex++ )
                                {
                                    status = dsGetAttributeValue( nodeContext->fDirNodeRef,
                                                                  dataBuffPtr,
                                                                  attrValueIndex,
                                                                  attrValueListRef,
                                                                  &attrValue );
                                    
                                    printf( "SearchDirNodeForURLData, attrValue = %s\n", attrValue->fAttributeValueData.fBufferData );
                                    
									valueRef = CreateServiceTypeFromDSType( attrValue->fAttributeValueData.fBufferData );

//                                    valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, attrValue->fAttributeValueData.fBufferData, kCFStringEncodingUTF8 );
                                    if ( valueArrayRef  )
                                    {
                                        printf( "SearchDirNodeForURLData, Adding to value return Array\n" );
                                        ::CFArrayAppendValue( valueArrayRef, valueRef );
                                        ::CFRelease( valueRef );
                                        valueRef = NULL;
                                    }

									dsDeallocAttributeValueEntry( nodeContext->fDirNodeRef, attrValue );
								}
                                
                                dsCloseAttributeValueList( attrValueListRef );
								attrValueListRef = 0;
								dsDeallocAttributeEntry( nodeContext->fDirNodeRef, attrInfoPtr );
								attrInfoPtr = NULL;
                            }
                            else
                            {
                                printf( "SearchDirNodeForURLData, dsGetAttributeEntry = %d\n", status );
                            }
                        
                            if ( valueArrayRef  )
                            {
                                ::CFDictionaryAddValue( resultDictionary, keyRef, valueArrayRef );
                                ::CFRelease( valueArrayRef );		// this has been retained by the dictionary
                                valueArrayRef = NULL;
                            }
                            else if ( valueRef )
                            {
                                ::CFDictionaryAddValue( resultDictionary, keyRef, valueRef );
                                ::CFRelease( valueRef );			// this has been retained by the dictionary
                                valueRef = NULL;
                            }
                            
                            if ( keyRef )
                            {
                                ::CFRelease( keyRef );
                                keyRef = NULL;
                            }
                        }

                        keyRef = NULL;
                        valueRef = NULL;
                        
                        dsCloseAttributeList( attributeListRef );
						attributeListRef = 0;
						dsDeallocRecordEntry( nodeContext->fDirNodeRef, recEntryPtr );
						recEntryPtr = NULL;
                    }
                    else
                    {
                        printf( "SearchDirNodeForURLData, dsGetRecordEntry = %d, ref:%lx, index:%ld\n", status, (UInt32)nodeContext->fDirNodeRef, recIndex );
                    }
                    
                    if ( recordToBeReturned && ::CFDictionaryGetCount(resultDictionary) > 0 )
                    {
                        CFStringRef		recordName = (CFStringRef)CFDictionaryGetValue( resultDictionary, CFSTR("dsAttrTypeStandard:RecordName" ) );
						
						if ( recordName )
						{
							CFDictionaryAddValue( serviceList, recordName, resultDictionary );
							CFRelease( recordName );
						}
						else
							printf( "SearchDirNodeForURLData, record had no name!\n" );
                    }
                                
//                    ::CFRelease( resultDictionary );
                    
                    resultDictionary = NULL;
                }
            }
        }
        while ( nodeContext->fContinueData && status == eDSNoErr );
        
        printf( "SearchDirNodeForURLData, finished in dirNode:%ld, continueData:%lx, status:%d\n", nodeContext->fDirNodeRef, (UInt32)(nodeContext->fContinueData), status );
        
	}
    
    catch( tDirStatus inErr )
    {
        status = inErr;

        if ( resultDictionary )
            ::CFRelease( resultDictionary );
        
        if ( keyRef )
            ::CFRelease( keyRef );
        
        if ( valueRef )
            ::CFRelease( valueRef );
            
        if ( valueArrayRef )
            ::CFRelease( valueArrayRef );
    }
    
    // clean up
    if ( dataBuffPtr )
        dsDataBufferDeAllocate( mDirRef, dataBuffPtr );
    if ( attrListAll )
	{
        dsDataListDeallocate( mDirRef, attrListAll );
		free(attrListAll);
	}
	
    if ( recNames )
	{
        dsDataListDeallocate( mDirRef, recNames );
		free(recNames);
	}
    
    return status;
}

CFStringRef CreateServiceTypeFromDSType( const char *inDSType )
{
    const char *recordTypeStr = inDSType;	// by default just return the same thing
    CFStringRef	returnRef = NULL;
	
    // map the DS type to a service type
	if ( strstr( inDSType, kDSStdRecordTypePrefix ) )
	{
		if ( strcmp( inDSType, kDSStdRecordTypeWebServer ) == 0 )
			recordTypeStr = "http";
		else
		if ( strcmp( inDSType, kDSStdRecordTypeFTPServer ) == 0 )
			recordTypeStr = "ftp";
		else
		if ( strcmp( inDSType, kDSStdRecordTypeAFPServer ) == 0 )
			recordTypeStr = "afp";
		else
		if ( strcmp( inDSType, kDSStdRecordTypeLDAPServer ) == 0 )
			recordTypeStr = "ldap";
		else
		if ( strcmp( inDSType, kDSStdRecordTypeNFS ) == 0 )
			recordTypeStr = "nfs";
		else
		if ( strcmp( inDSType, kDSStdRecordTypeSMBServer ) == 0 )
			recordTypeStr = "smb";
    }	
	else
	if ( strstr( inDSType, kDSNativeRecordTypePrefix ) )
    {
        recordTypeStr = inDSType + strlen(kDSNativeRecordTypePrefix);
    }
	else
	if ( strstr( inDSType, kDSNativeAttrTypePrefix ) )
    {
        recordTypeStr = inDSType + strlen(kDSNativeAttrTypePrefix);
    }

    if ( recordTypeStr )
		returnRef = CFStringCreateWithCString( kCFAllocatorDefault, recordTypeStr, kCFStringEncodingUTF8 );
    
    return returnRef;
}

void CheckDSNodePath( CFStringRef parentNodePath, CFStringRef dsNodePath, Boolean* doNotShow, Boolean* onlyLocal )
{
	*doNotShow = IsBadDSNodePath( parentNodePath ) || IsBadDSNodePath( dsNodePath );
	*onlyLocal = IsOnlyLocal( dsNodePath );
}

Boolean IsOnlyLocal( CFStringRef dsNodePath )
{
	Boolean	isBad = false;
	
	if ( !dsNodePath )	
		isBad = true;
	else if ( CFStringCompare( dsNodePath, CFSTR("\tBonjour"), 0 ) == kCFCompareEqualTo )
		isBad = true;
	else if (  CFStringCompare( dsNodePath, CFSTR("\tBonjour\tlocal"), 0 ) == kCFCompareEqualTo )
		isBad = true;
	else if ( CFStringCompare( dsNodePath, CFSTR("\tSLP"), 0 ) == kCFCompareEqualTo )
		isBad = true;
	else if (  CFStringCompare( dsNodePath, CFSTR("\tSLP\tDEFAULT"), kCFCompareCaseInsensitive ) == kCFCompareEqualTo )
		isBad = true;
	else if ( CFStringCompare( dsNodePath, CFSTR("\tAppleTalk"), 0 ) == kCFCompareEqualTo )
		isBad = true;
	else if (  CFStringCompare( dsNodePath, CFSTR("\tAppleTalk\t*"), kCFCompareCaseInsensitive ) == kCFCompareEqualTo )
		isBad = true;

	return isBad;
}

Boolean IsBadDSNodePath( CFStringRef dsNodePath )
{
#warning "TODO: Need to expand our BadNeighborhood logic"
// here are some ideas on how to move forward:
//	One thing we want is to allow the calling application to specify this list of nodes to ignore
//	Another is to look for this data in the user record (append to above)
//	A third would be to also look at the local machine record (now do we append or use only if user record is empty?)
//	A fourth would be to allow inclusionary lists as well. (i.e. only show things out of that list)

	Boolean	isBad = false;
	
	if ( !dsNodePath )	
		isBad = true;
	else if ( CFStringCompare( dsNodePath, CFSTR("\tNetInfo\troot"), 0 ) == kCFCompareEqualTo )
		isBad = true;
	else if ( CFStringCompare( dsNodePath, CFSTR("\tNetInfo"), 0 ) == kCFCompareEqualTo )
		isBad = true;
	else if ( CFStringCompare( dsNodePath, CFSTR("\tBSD Configuration Files\tLocal"), 0 ) == kCFCompareEqualTo )
		isBad = true;
	else if ( CFStringCompare( dsNodePath, CFSTR("\tNetInfo\tDefaultLocalNode"), 0 ) == kCFCompareEqualTo )
		isBad = true;
	else if ( CFStringCompare( dsNodePath, CFSTR("\tNIS\tlocal"), 0 ) == kCFCompareEqualTo )
		isBad = true;
	else if ( CFStringCompare( dsNodePath, CFSTR("\tBSD\tlocal"), 0 ) == kCFCompareEqualTo )
		isBad = true;
	else if ( CFStringCompare( dsNodePath, CFSTR("\tBSD"), 0 ) == kCFCompareEqualTo )
		isBad = true;

	return isBad;
}

CFMutableDictionaryRef FindNeighborhodInNodeTree( CFMutableDictionaryRef nodeNeighborhoodTree, CFStringRef curNodePieceName, CFStringRef curDSPathRef )
{
	CFMutableDictionaryRef		foundNeighborhood = NULL;
/*	CFArrayRef					neighborhoodList = (CFArrayRef)CFDictionaryGetValue( nodeNeighborhoodTree, curDSPathRef );
	CFIndex						numNeighborhoods = (neighborhoodList)?CFArrayGetCount( neighborhoodList ):0;

#warning "This implementation isn't going to scale for large numbers of neighborhoods - O(n)/2"	
	for ( CFIndex index = 0; index < numNeighborhoods; index++ )
	{
		CFStringRef		curName = (CFStringRef)CFDictionaryGetValue( (CFMutableDictionaryRef)CFArrayGetValueAtIndex( neighborhoodList, index ), CFSTR(kNSLNeighborhoodDisplayName) );
		if ( curName && CFStringCompare( curName, curNodePieceName, 0 ) == kCFCompareEqualTo )
		{
			foundNeighborhood = (CFMutableDictionaryRef)CFArrayGetValueAtIndex( neighborhoodList, index );
			break;
		}
	}
*/	
	return foundNeighborhood;
}

