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
 *  @header CSMBPlugin
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <Security/Authorization.h>

#include "CNSLDirNodeRep.h"
#include "CSMBPlugin.h"
#include "CSMBNodeLookupThread.h"
#include "CSMBServiceLookupThread.h"
#include "CommandLineUtilities.h"
#include "CNSLTimingUtils.h"

#define kDefaultGroupName			"WORKGROUP"

#define	kMaxNumLMBsForAggressiveSearch	0		// more than 10 LMBs in your subnet, we take it easy.
#define	kInitialTimeBetweenNodeLookups	60		// look again in a minute if we haven't found any LMBs

const char* GetCodePageStringForCurrentSystem( void );

static Boolean			sNMBLookupToolIsAvailable = false;
static CSMBPlugin*		gPluginInstance = NULL;
const CFStringRef	gBundleIdentifier = CFSTR("com.apple.DirectoryService.SMB");
const char*			gProtocolPrefixString = "SMB";

#pragma warning "Need to get our default Node String from our resource"

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0xFB, 0x9E, 0xDB, 0xD3, 0x9B, 0x3A, 0x11, 0xD5, \
								0xA0, 0x50, 0x00, 0x30, 0x65, 0x3D, 0x61, 0xE4 );

}

static CDSServerModule* _Creator ( void )
{
	DBGLOG( "Creating new SMB Plugin\n" );
    gPluginInstance = new CSMBPlugin;
	return( gPluginInstance );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

void AddNode( CFStringRef nodeNameRef )
{
	gPluginInstance->AddNode( nodeNameRef );
}

CSMBPlugin::CSMBPlugin( void )
    : CNSLPlugin()
{
	DBGLOG( "CSMBPlugin::CSMBPlugin\n" );
    mNodeListIsCurrent = false;
	mNodeSearchInProgress = false;
	mLocalNodeString = NULL;
	mWINSServer = NULL;
	mBroadcastAddr = NULL;
	mOurLMBs = NULL;
	mAllKnownLMBs = NULL;
	mListOfLMBsInProgress = NULL;
	mInitialSearch = true;
	mNeedFreshLookup = true;
	mCurrentSearchCanceled = false;
	mTimeBetweenLookups = kInitialTimeBetweenNodeLookups;
}

CSMBPlugin::~CSMBPlugin( void )
{
	DBGLOG( "CSMBPlugin::~CSMBPlugin\n" );
    
    if ( mLocalNodeString )
        free( mLocalNodeString );    
    mLocalNodeString = NULL;
	
	if ( mWINSServer )
		free( mWINSServer );
	mWINSServer = NULL;
	
	if ( mBroadcastAddr )
		free( mBroadcastAddr );
	mBroadcastAddr = NULL;
	
	if ( mOurLMBs )
		CFRelease( mOurLMBs );
	mOurLMBs = NULL;
	
	if ( mAllKnownLMBs )
		CFRelease( mAllKnownLMBs );
	mAllKnownLMBs = NULL;
	
	if ( mListOfLMBsInProgress )
		CFRelease( mListOfLMBsInProgress );
	mListOfLMBsInProgress = NULL;
}

sInt32 CSMBPlugin::InitPlugin( void )
{
    sInt32				siResult	= eDSNoErr;
    // need to see if this is installed!
    struct stat			data;
    int 				result = eDSNoErr;
	

	mLMBDiscoverer	= new LMBDiscoverer();
	mLMBDiscoverer->Initialize();
	
	pthread_mutex_init( &mNodeStateLock, NULL );
	DBGLOG( "CSMBPlugin::InitPlugin\n" );
	
    if ( siResult == eDSNoErr )
    {
        result = stat( kNMBLookupToolPath, &data );
        if ( result < 0 )
        {
            DBGLOG( "SMB couldn't find nmblookup tool: %s (should be at:%s?)\n", strerror(errno), kNMBLookupToolPath );
        }
        else
            sNMBLookupToolIsAvailable = true;
    }
    
    if ( siResult == eDSNoErr )
		ReadConfigFile();
	
    if ( siResult == eDSNoErr )
	{
		if ( !mLocalNodeString )
		{
			mLocalNodeString = (char *) malloc( strlen(kDefaultGroupName) + 1 );
			if ( mLocalNodeString )
			{
				strcpy( mLocalNodeString, kDefaultGroupName );
			}
		}
    }
	
    return siResult;
}

void CSMBPlugin::ActivateSelf( void )
{
    DBGLOG( "CSMBPlugin::ActivateSelf called\n" );

	OurLMBDiscoverer()->EnableSearches();

	CNSLPlugin::ActivateSelf();
}

void CSMBPlugin::DeActivateSelf( void )
{
	// we are getting deactivated.  We want to tell the slpd daemon to shutdown as
	// well as deactivate our DA Listener
    DBGLOG( "CSMBPlugin::DeActivateSelf called\n" );
	
	OurLMBDiscoverer()->DisableSearches();
	OurLMBDiscoverer()->ClearLMBCache();				// no longer valid
	OurLMBDiscoverer()->ResetOurBroadcastAddress();	
	OurLMBDiscoverer()->ClearBadLMBList();
	
	mInitialSearch = true;	// starting from scratch
	mNeedFreshLookup = true;
	mCurrentSearchCanceled = true;
	ZeroLastNodeLookupStartTime();		// force this to start again
	mNodeSearchInProgress = false;

	CNSLPlugin::DeActivateSelf();
}


#pragma mark -
#define kMaxSizeOfParam 1024
void CSMBPlugin::ReadConfigFile( void )
{
	// we can see if there is a config file, if so then see if they have a WINS server specified
	DBGLOG( "CSMBPlugin::ReadConfigFile\n" );
    FILE *	fp;
    char 	buf[kMaxSizeOfParam];
	
	fp = fopen(kConfFilePath,"r");
	
    if (fp == NULL) 
	{
        DBGLOG( "CSMBPlugin::ReadConfigFile, couldn't open conf file, copy temp to conf\n" );

		char		command[256];
		
		snprintf( command, sizeof(command), "/bin/cp %s %s\n", kTemplateConfFilePath, kConfFilePath );
		executecommand( command );

		fp = fopen(kConfFilePath,"r");
		
		if (fp == NULL) 
			return;
    }
    
    while (fgets(buf,kMaxSizeOfParam,fp) != NULL) 
	{
        char *pcKey;
        
        if (buf[0] == '\n' || buf[0] == '\0' || buf[0] == '#' || buf[0] == ';')
			continue;
    
		if ( buf[strlen(buf)-1] == '\n' )
			buf[strlen(buf)-1] = '\0';
					
        pcKey = strstr( buf, "wins server" );

        if ( pcKey )
		{
			pcKey = strstr( pcKey, "=" );
			
			if ( pcKey )
			{
				pcKey++;
				
				while ( isspace( *pcKey ) )
					pcKey++;
			
				long	ignore;
				if ( IsIPAddress( pcKey, &ignore ) || IsDNSName( pcKey ) )
				{
					mWINSServer = (char*)malloc(strlen(pcKey)+1);
					strcpy( mWINSServer, pcKey );
					DBGLOG( "CSMBPlugin::ReadConfigFile, WINS Server is %s\n", mWINSServer );
					mLMBDiscoverer->SetWinsServer(mWINSServer);
				}
			}
			continue;
		}

        pcKey = strstr( buf, "workgroup" );

        if ( pcKey )
		{
			pcKey = strstr( pcKey, "=" );

			if ( pcKey )
			{
				pcKey++;
				
				while ( isspace( *pcKey ) )
					pcKey++;
			
				mLocalNodeString = (char*)malloc(strlen(pcKey)+1);
				strcpy( mLocalNodeString, pcKey );
				DBGLOG( "CSMBPlugin::ReadConfigFile, Workgroup is %s\n", mLocalNodeString );
			}
			
			continue;
		}
    }

    fclose(fp);
	
	WriteToConfigFile(kBrowsingConfFilePath);			// need to update the browsing config file with the appropriate codepage
}

void CSMBPlugin::WriteWorkgroupToFile( FILE* fp )
{
	if ( mLocalNodeString )
	{
		char	workgroupLine[kMaxSizeOfParam];
		
		sprintf( workgroupLine, "  workgroup = %s\n", mLocalNodeString );
		
		fputs( workgroupLine, fp );
	}
}

void CSMBPlugin::WriteWINSToFile( FILE* fp )
{
	if ( mWINSServer )
	{
		char	winsLine[kMaxSizeOfParam];
		
		sprintf( winsLine, "  wins server = %s\n", mWINSServer );
		
		fputs( winsLine, fp );
	}
}

void CSMBPlugin::WriteCodePageToFile( FILE* fp )
{
	char	winsLine[kMaxSizeOfParam];
	
	sprintf( winsLine, "  dos charset = %s\n", GetCodePageStringForCurrentSystem() );
	
	fputs( winsLine, fp );
}

void CSMBPlugin::WriteUnixCharsetToFile( FILE* fp )
{
	char	winsLine[kMaxSizeOfParam];
		
	sprintf( winsLine, "  unix charset = %s\n", GetCodePageStringForCurrentSystem() );
	
	fputs( winsLine, fp );
}

void CSMBPlugin::WriteDisplayCharsetToFile( FILE* fp )
{
	char	winsLine[kMaxSizeOfParam];
	
	sprintf( winsLine, "  display charset = %s\n", GetCodePageStringForCurrentSystem() );
	
	fputs( winsLine, fp );
}

void CSMBPlugin::WriteToConfigFile( const char* pathToConfigFile )
{
	// we can see if there is a config file, if so then see if they have a WINS server specified
	DBGLOG( "CSMBPlugin::WriteToConfigFile [%s]\n", pathToConfigFile );
    FILE		*sourceFP = NULL, *destFP = NULL;
    char		buf[kMaxSizeOfParam];
    Boolean		writtenWINS = false;
	Boolean		writtenWorkgroup = false;
	Boolean		writeCodePage = (strcmp( kBrowsingConfFilePath, pathToConfigFile ) == 0);	// only update the browsing config's code page
	Boolean		writeUnixCharset = writeCodePage;
	Boolean		writeDisplayCharset = writeCodePage;
	
	sourceFP = fopen(kConfFilePath,"r+");
	
    if (sourceFP == NULL) 
	{
        DBGLOG( "CSMBPlugin::WriteToConfigFile, couldn't open conf file, copy temp to conf\n" );

		char		command[256];
		
		snprintf( command, sizeof(command), "/bin/cp %s %s\n", kTemplateConfFilePath, kConfFilePath );
		executecommand( command );

		sourceFP = fopen(kConfFilePath,"r+");
		
		if (sourceFP == NULL) 
			return;
    }
    
	if ( strcmp( kConfFilePath, pathToConfigFile ) == 0 )
		destFP = fopen( kTempConfFilePath, "w" );		// if we are writing to the config file, need to modify the temp and copy over
	else
		destFP = fopen( pathToConfigFile, "w" );			// otherwise we'll just copy and modify straight into the new file
	
	if ( !destFP )
	{
		fclose( sourceFP );
		return;
	}
	
	if ( !mLocalNodeString )
		writtenWorkgroup = true;
		
	while (fgets(buf,kMaxSizeOfParam,sourceFP) != NULL) 
	{
        char *pcKey = NULL;
        
        if (buf[0] == '\n' || buf[0] == '\0' || buf[0] == '#' || buf[0] == ';' || (writtenWorkgroup && writtenWINS && !writeCodePage && !writeUnixCharset && !writeDisplayCharset) )
		{
			fputs( buf, destFP );
			continue;
		}
		
		if ( strstr( buf, "[homes]" ) || strstr( buf, "[public]" ) || strstr( buf, "[printers]" )  )
		{
			// ok, we've passed where this data should go, write out whatever we have left
			if ( writeUnixCharset )
			{
				WriteUnixCharsetToFile( destFP );
				writeUnixCharset = false;
			}
				
			if ( writeDisplayCharset )
			{
				WriteDisplayCharsetToFile( destFP );
				writeDisplayCharset = false;
			}
				
			if ( writeCodePage )
			{
				WriteCodePageToFile( destFP );
				writeCodePage = false;
			}
				
			if ( !writtenWorkgroup )
			{
				WriteWorkgroupToFile( destFP );
				writtenWorkgroup = true;
			}
			
			if ( !writtenWINS )
			{
				WriteWINSToFile( destFP );
				writtenWINS = true;
			}
				
			fputs( buf, destFP );			// now add the line we read
			
			continue;
		}
		
		if ( !writtenWINS )
			pcKey = strstr( buf, "wins server" );

        if ( pcKey )
		{
			WriteWINSToFile( destFP );
			writtenWINS = true;

			continue;
		}
		else if ( !writtenWorkgroup && (pcKey = strstr( buf, "workgroup" )) != NULL )
        {
			WriteWorkgroupToFile( destFP );
			writtenWorkgroup = true;

			continue;
		}
		else if ( writeCodePage && (pcKey = strstr( buf, "dos charset" )) != NULL )
		{
			WriteCodePageToFile( destFP );
			writeCodePage = false;
		}
		else if ( writeUnixCharset && (pcKey = strstr( buf, "unix charset" )) != NULL )
		{
			WriteUnixCharsetToFile( destFP );
			writeUnixCharset = false;
		}
		else if ( writeDisplayCharset && (pcKey = strstr( buf, "display charset" )) != NULL )
		{
			WriteDisplayCharsetToFile( destFP );
			writeDisplayCharset = false;
		}
		else
			fputs( buf, destFP );			// now add the line we read		
    }

    fclose(sourceFP);
    fclose(destFP);

	if ( strcmp( kConfFilePath, pathToConfigFile ) == 0 )	// if we are writing to the config file, need to modify the temp and copy over
	{
		char		command[256];
		
		snprintf( command, sizeof(command), "/bin/mv %s %s\n", kTempConfFilePath, kConfFilePath );

		executecommand( command );
	}
}

#pragma mark -
sInt32 CSMBPlugin::GetDirNodeInfo( sGetDirNodeInfo *inData )
{
    sInt32				siResult			= eNotHandledByThisNode;	// plugins can override
	
	return siResult;		
}

sInt32 CSMBPlugin::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	sInt32					siResult	= eDSNoErr;
	unsigned long			aRequest	= 0;
	unsigned long			bufLen			= 0;
	AuthorizationRef		authRef			= 0;
	AuthorizationItemSet   *resultRightSet = NULL;

	DBGLOG( "CSMBPlugin::DoPlugInCustomCall called\n" );
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
			DBGLOG( "CSMBPlugin::DoPlugInCustomCall called but we couldn't find the nodeDirRep!\n" );
	
		nodeDirRep = (CNSLDirNodeRep*)dictionaryResult;
	
		if ( nodeDirRep )
		{
			aRequest = inData->fInRequestCode;

			if ( aRequest != kReadSMBConfigData )
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
				case kReadSMBConfigData:
				{
					DBGLOG( "CSMBPlugin::DoPlugInCustomCall kReadSMBConfigData\n" );

					// read config
					siResult = FillOutCurrentState( inData );

					if ( !(mState & kActive) || (mState & kInactive) )
					{
						ClearOutAllNodes();					// clear these out
						mNodeListIsCurrent = false;			// this is no longer current
						DBGLOG( "CSMBPlugin::DoPlugInCustomCall cleared out all our registered nodes as we are inactive\n" );
					}
				}
				break;
					 
				case kWriteSMBConfigData:
				{
					DBGLOG( "CSMBPlugin::DoPlugInCustomCall kWriteSMBConfigData\n" );

					sInt32		dataLength = (sInt32) bufLen - sizeof( AuthorizationExternalForm );
					Boolean		configChanged = false;
					
					if ( dataLength <= 0 ) throw( (sInt32)eDSInvalidBuffFormat );
					
					UInt8*		curPtr =(UInt8 *)(inData->fInRequestData->fBufferData + sizeof( AuthorizationExternalForm ));
					UInt32		curDataLen;
					char*		newWorkgroupString = NULL;
					char*		newWINSServer = NULL;
					
					curDataLen = *((UInt32*)curPtr);
					curPtr += 4;
					
					if ( curDataLen > 0 )
					{
						newWorkgroupString = (char*)malloc(curDataLen+1);
						memcpy( newWorkgroupString, curPtr, curDataLen );
						newWorkgroupString[curDataLen] = '\0';
						curPtr += curDataLen;
					}
					
					curDataLen = *((UInt32*)curPtr);
					curPtr += 4;

					if ( curDataLen > 0 )
					{
						newWINSServer = (char*)malloc(curDataLen+1);
						memcpy( newWINSServer, curPtr, curDataLen );
						newWINSServer[curDataLen] = '\0';
					}

					if ( mLocalNodeString && newWorkgroupString )
					{
						if ( strcmp( mLocalNodeString, newWorkgroupString ) != 0 )
						{
							free( mLocalNodeString );
							mLocalNodeString = newWorkgroupString;
							
							configChanged = true;
						} else {
							// if we are the same string, we need to free it
							free( newWorkgroupString );
						}
					}
					else if ( newWorkgroupString )
					{
						// we shouldn't be called if we don't have a mLocalNodeString!
						mLocalNodeString = newWorkgroupString;
							
						configChanged = true;
					}
					
					if ( mWINSServer && newWINSServer )
					{
						if ( strcmp( mWINSServer, newWINSServer ) != 0 )
						{
							free( mWINSServer );
							mWINSServer = newWINSServer;
							mLMBDiscoverer->SetWinsServer(mWINSServer);
							
							configChanged = true;
						}
					}
					else if ( newWINSServer )
					{
						mWINSServer = newWINSServer;
						mLMBDiscoverer->SetWinsServer(mWINSServer);
							
						configChanged = true;
					}
					else if ( mWINSServer )
					{
						free( mWINSServer );
						mWINSServer = NULL;
						mLMBDiscoverer->SetWinsServer(mWINSServer);

						configChanged = true;
					}

					if ( configChanged )
					{
						WriteToConfigFile(kConfFilePath);
						WriteToConfigFile(kBrowsingConfFilePath);

						if ( !(mState & kActive) || (mState & kInactive) )
						{
							ClearOutAllNodes();					// clear these out
							mNodeListIsCurrent = false;			// this is no longer current
							DBGLOG( "CSMBPlugin::DoPlugInCustomCall cleared out all our registered nodes after writing config changes as we are inactive\n" );
						}
						
						if ( (mState & kActive) && mActivatedByNSL )
						{
							DBGLOG( "CSMBPlugin::DoPlugInCustomCall (mState & kActive) && mActivatedByNSL so starting a new fresh node lookup.\n" );
							mNeedFreshLookup = true;
							OurLMBDiscoverer()->ClearLMBCache();
							StartNodeLookup();					// and then start all over
						}
					}
				}
				break;
					
				default:
					break;
			}
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	if (authRef != 0)
	{
		AuthorizationFree(authRef, 0);
		authRef = 0;
	}

	return( siResult );

}

sInt32 CSMBPlugin::HandleNetworkTransition( sHeader *inData )
{
    sInt32					siResult			= eDSNoErr;

	DBGLOG( "CSMBPlugin::HandleNetworkTransition called\n" );

	if ( mActivatedByNSL && IsActive() )
	{
		DBGLOG( "CSMBPlugin::HandleNetworkTransition cleaning up data and calling CNSLPlugin::HandleNetworkTransition\n" );

		if ( mBroadcastAddr )
			free( mBroadcastAddr );
		mBroadcastAddr = NULL;
		
		OurLMBDiscoverer()->ClearLMBCache();				// no longer valid
		OurLMBDiscoverer()->ResetOurBroadcastAddress();	
		OurLMBDiscoverer()->ClearBadLMBList();
		
		mInitialSearch = true;	// starting from scratch
		mNeedFreshLookup = true;
		mCurrentSearchCanceled = true;
		ZeroLastNodeLookupStartTime();		// force this to start again
		mTimeBetweenLookups = kInitialTimeBetweenNodeLookups;
		
		siResult = CNSLPlugin::HandleNetworkTransition( inData );
	}
	else
		DBGLOG( "CSMBPlugin::HandleNetworkTransition skipped, mActivatedByNSL: %d, IsActive: %d\n", mActivatedByNSL, IsActive() );

    return ( siResult );
}

#pragma mark -
#define kMaxTimeToWait	10	// in seconds
sInt32 CSMBPlugin::FillOutCurrentState( sDoPlugInCustomCall *inData )
{
	sInt32					siResult	= eDSNoErr;
	UInt32					workgroupDataLen = 0;
	void*					workgroupData = NULL;

//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

	try
	{
		if ( !mNodeListIsCurrent && !mNodeSearchInProgress )
		{
			StartNodeLookup();
			
			int	i = 0;
			while ( !mNodeListIsCurrent && i++ < kMaxTimeToWait )
				SmartSleep(1*USEC_PER_SEC);
		}
			
		workgroupDataLen = 0;
		workgroupData = MakeDataBufferOfWorkgroups( &workgroupDataLen );
		
		char*		curPtr = inData->fOutRequestResponse->fBufferData;
		UInt32		dataLen = 4;
		
		 if (mLocalNodeString)
			dataLen += strlen(mLocalNodeString);
			
		dataLen += 4;
		
		 if (mWINSServer)
			dataLen += strlen(mWINSServer);
		
		dataLen += workgroupDataLen;
		
		if ( inData->fOutRequestResponse == nil ) throw( (sInt32)eDSNullDataBuff );
		if ( inData->fOutRequestResponse->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );

		if ( inData->fOutRequestResponse->fBufferSize < (unsigned int)dataLen ) 
			throw( (sInt32)eDSBufferTooSmall );

		if ( mLocalNodeString )
		{
			*((UInt32*)curPtr) = strlen(mLocalNodeString);
			curPtr += 4;
			memcpy( curPtr, mLocalNodeString, strlen(mLocalNodeString) );
			curPtr += strlen(mLocalNodeString);
		}
		else
		{
			*((UInt32*)curPtr) = 0;
			curPtr += 4;
		}
		
		if ( mWINSServer )
		{
			*((UInt32*)curPtr) = strlen(mWINSServer);
			curPtr += 4;
			memcpy( curPtr, mWINSServer, strlen(mWINSServer) );
			curPtr += strlen(mWINSServer);
		}
		else
		{
			*((UInt32*)curPtr) = 0;
			curPtr += 4;
		}
		
		memcpy( curPtr, workgroupData, workgroupDataLen );
		inData->fOutRequestResponse->fBufferLength = dataLen;
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if ( workgroupData )
	{
		free( workgroupData );
		workgroupData = NULL;
	}
	
	return siResult;
}

typedef char	SMBNodeName[16];
typedef struct NSLPackedNodeList {
	UInt32						fBufLen;
	UInt32						fNumNodes;
	SMBNodeName					fNodeData[];
} NSLPackedNodeList;

void SMBNodeHandlerFunction(const void *inKey, const void *inValue, void *inContext);
void SMBNodeHandlerFunction(const void *inKey, const void *inValue, void *inContext)
{
	DBGLOG( "SMBNodeHandlerFunction SMBNodeHandlerFunction\n" );
	CFShow( inKey );

    NodeData*					curNodeData = (NodeData*)inValue;
    NSLNodeHandlerContext*		context = (NSLNodeHandlerContext*)inContext;

	NSLPackedNodeList*	nodeListData = (NSLPackedNodeList*)context->fDataPtr;
	SMBNodeName	curNodeName = {0};
	
	CFStringGetCString( curNodeData->fNodeName, curNodeName, sizeof(curNodeName), kCFStringEncodingUTF8 );
	
	if ( !nodeListData )
	{
		DBGLOG( "SMBNodeHandlerFunction first time called, creating nodeListData from scratch\n" );
		nodeListData = (NSLPackedNodeList*)malloc( sizeof(NSLPackedNodeList) + 4 + sizeof(curNodeName) );
		nodeListData->fBufLen = sizeof(NSLPackedNodeList) + 4 + sizeof(curNodeName);
		nodeListData->fNumNodes = 1;
		memcpy( nodeListData->fNodeData, curNodeName, sizeof(curNodeName) );
	}
	else
	{
		DBGLOG( "SMBNodeHandlerFunction first time called, need to append nodeListData\n" );
		NSLPackedNodeList*	oldNodeListData = nodeListData;
		
		nodeListData = (NSLPackedNodeList*)malloc( oldNodeListData->fBufLen + 4 + sizeof(curNodeName) );
		nodeListData->fBufLen = oldNodeListData->fBufLen + 4 + sizeof(curNodeName);
		nodeListData->fNumNodes = oldNodeListData->fNumNodes + 1;
		memcpy( nodeListData->fNodeData, oldNodeListData->fNodeData, oldNodeListData->fNumNodes * sizeof(curNodeName) );
		memcpy( &nodeListData->fNodeData[nodeListData->fNumNodes - 1], curNodeName, sizeof(curNodeName) );
		
		free( oldNodeListData );
	}
	
	context->fDataPtr = nodeListData;
}

void* CSMBPlugin::MakeDataBufferOfWorkgroups( UInt32* dataLen )
{
	DBGLOG( "CSMBPlugin::MakeDataBufferOfWorkgroups called\n" );
	// need to fill out all the currently found workgroup data
	NSLNodeHandlerContext	context;
	
	context.fDictionary = mPublishedNodes;
	context.fDataPtr = NULL;
	
    LockPublishedNodes();
	CFDictionaryApplyFunction( mPublishedNodes, SMBNodeHandlerFunction, &context );
    UnlockPublishedNodes();
	
	if ( context.fDataPtr )
		*dataLen = ((NSLPackedNodeList*)context.fDataPtr)->fBufLen;
	
	return context.fDataPtr;
}

CFStringRef CSMBPlugin::GetBundleIdentifier( void )
{
    return gBundleIdentifier;
}

// this is used for top of the node's path "SMB"
const char*	CSMBPlugin::GetProtocolPrefixString( void )
{		
    return gProtocolPrefixString;
}

// this maps to the group we belong to  (i.e. WORKGROUP)
const char*	CSMBPlugin::GetLocalNodeString( void )
{		
    return mLocalNodeString;
}


Boolean CSMBPlugin::IsLocalNode( const char *inNode )
{
    Boolean result = false;
    
    if ( mLocalNodeString )
    {
        result = ( strcmp( inNode, mLocalNodeString ) == 0 );
    }
    
    return result;
}

void CSMBPlugin::NodeLookupIsCurrent( void )
{
	LockNodeState();
	mNodeListIsCurrent = true;
	mNodeSearchInProgress = false;

	if ( mCurrentSearchCanceled )
		mInitialSearch = true;		// if this search was canceled, we want to start again
	else
		mInitialSearch = false;

	UnLockNodeState();
}

UInt32 CSMBPlugin::GetTimeBetweenNodeLookups( void )
{
	if ( mTimeBetweenLookups < kOncePerDay )	// are we still in a situation where we haven't found any LMBs?
	{
		CFDictionaryRef		knownLMBDictionary = OurLMBDiscoverer()->GetAllKnownLMBs();
		
		if ( knownLMBDictionary && CFDictionaryGetCount( knownLMBDictionary ) > 0 )
			mTimeBetweenLookups = kOncePerDay;
		else
			mTimeBetweenLookups *= 2;			// we still haven't found any LMBs, back off node discovery
	}
	else if ( mTimeBetweenLookups > kOncePerDay )
		mTimeBetweenLookups = kOncePerDay;
	
	return mTimeBetweenLookups;
}

void CSMBPlugin::NewNodeLookup( void )
{
	DBGLOG( "CSMBPlugin::NewNodeLookup\n" );
	
	LockNodeState();
	if ( !mNodeSearchInProgress )
	{
		mNodeListIsCurrent = false;
		mNodeSearchInProgress = true;
		mNeedFreshLookup = false;
		mCurrentSearchCanceled = false;
		// First add our local scope
		AddNode( GetLocalNodeString() );		// always register the default registration node
		
		if ( sNMBLookupToolIsAvailable )
		{
			CSMBNodeLookupThread* newLookup = new CSMBNodeLookupThread( this );
			
			newLookup->Resume();
		}
		
		if ( !sNMBLookupToolIsAvailable )
			DBGLOG( "CSMBPlugin::NewNodeLookup, ignoring as the SMB library isn't available\n" );
	}
	else if ( mNeedFreshLookup )
	{
		DBGLOG( "CSMBPlugin::NewNodeLookup, we need a fresh lookup, but one is still in progress\n" );
		ZeroLastNodeLookupStartTime();		// force this to start again
	}
	
	UnLockNodeState();
}

void CSMBPlugin::NewServiceLookup( char* serviceType, CNSLDirNodeRep* nodeDirRep )
{
	DBGLOG( "CSMBPlugin::NewServiceLookup\n" );

    if ( sNMBLookupToolIsAvailable )
    {
        if ( serviceType && strcmp( serviceType, kServiceTypeString ) == 0 )
        {
			CFStringRef	workgroupRef = nodeDirRep->GetNodeName();
			
			CFArrayRef listOfLMBs = OurLMBDiscoverer()->CopyBroadcastResultsForLMB( workgroupRef );
			
			if ( listOfLMBs || !GetWinsServer() )
			{
				char	workgroup[256];
				CFStringGetCString( nodeDirRep->GetNodeName(), workgroup, sizeof(workgroup), kCFStringEncodingUTF8 );
				DBGLOG( "CSMBPlugin::NewServiceLookup doing lookup on %ld LMBs responsible for %s\n", (listOfLMBs)?CFArrayGetCount(listOfLMBs):0, workgroup );

				CSMBServiceLookupThread* newLookup = new CSMBServiceLookupThread( this, serviceType, nodeDirRep, listOfLMBs );
				
						// if we have too many threads running, just queue this search object and run it later
						if ( OKToStartNewSearch() )
							newLookup->Resume();
						else
							QueueNewSearch( newLookup );
				
				if ( listOfLMBs )
					CFRelease( listOfLMBs ); // release the list now that we're done with it
			}
			else if ( GetWinsServer() )	// if we have a WINS server, go ahead and try with a cached name, this may get resolved properly and not be on our subnet
			{
				CFStringRef	cachedLMB = OurLMBDiscoverer()->CopyOfCachedLMBForWorkgroup( nodeDirRep->GetNodeName() );

				if ( cachedLMB )
				{
					CFArrayRef listOfLMBs = CFArrayCreate( NULL, &cachedLMB, 1, &kCFTypeArrayCallBacks );
					
					if ( listOfLMBs )
					{
						CSMBServiceLookupThread* newLookup = new CSMBServiceLookupThread( this, serviceType, nodeDirRep, listOfLMBs );
			
					// if we have too many threads running, just queue this search object and run it later
					if ( OKToStartNewSearch() )
						newLookup->Resume();
					else
						QueueNewSearch( newLookup );
						
						CFRelease( listOfLMBs );
					}
					
					CFRelease( cachedLMB );
				}
				else
					DBGLOG( "CSMBPlugin::NewServiceLookup, no cached LMB\n" );
			}
        }
        else if ( serviceType )
            DBGLOG( "CSMBPlugin::NewServiceLookup skipping as we don't support lookups on type:%s\n", serviceType );
    }
}

Boolean CSMBPlugin::OKToOpenUnPublishedNode( const char* parentNodeName )
{
	return false;
}

#pragma mark -
void CSMBPlugin::ClearLMBForWorkgroup( CFStringRef workgroupRef, CFStringRef lmbNameRef )
{
	OurLMBDiscoverer()->ClearLMBForWorkgroup( workgroupRef, lmbNameRef );
}

