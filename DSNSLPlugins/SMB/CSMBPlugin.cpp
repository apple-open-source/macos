/*
 *  CSMBPlugin.cpp
 *  DSSMBPlugIn
 *
 *  Created by imlucid on Wed Aug 15 2001.
 *  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <Security/Authorization.h>

#include "CSMBPlugin.h"
#include "CSMBNodeLookupThread.h"
#include "CSMBServiceLookupThread.h"
#include "TGetCFBundleResources.h"
#include "CommandLineUtilities.h"

#define kCommandParamsID				129
#define kServiceTypeStrID				1
#define kNMBLookupToolPath				2
#define kTemplateConfFilePathStrID		3
#define kConfFilePathStrID				4

//#define kListClassPathStrID				2
//#define kLookupWorkgroupsJCIFSCommand	3
#define kDefaultGroupName			"WORKGROUP"

static Boolean			sNMBLookupToolIsAvailable = false;

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
    return( new CSMBPlugin );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

CSMBPlugin::CSMBPlugin( void )
    : CNSLPlugin()
{
	DBGLOG( "CSMBPlugin::CSMBPlugin\n" );
    mNodeListIsCurrent = false;
	mLocalNodeString = NULL;
    mServiceTypeString = NULL;
	mTemplateConfFilePath = NULL;
	mConfFilePath = NULL;
    mNMBLookupToolPath = NULL;
	mWINSServer = NULL;
	mWINSWorkgroups = NULL;
	mBroadcastAddr = NULL;
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
	
	if ( mWINSWorkgroups )
		CFRelease( mWINSWorkgroups );
	mWINSWorkgroups = NULL;
	
	if ( mTemplateConfFilePath )
		free( mTemplateConfFilePath );
	mTemplateConfFilePath = NULL;
	
	if ( mConfFilePath )
		free( mConfFilePath );
	mConfFilePath = NULL;

	if ( mBroadcastAddr )
		free( mBroadcastAddr );
	mBroadcastAddr = NULL;
}

sInt32 CSMBPlugin::InitPlugin( void )
{
    char				resBuff[256] = {0};
    SInt32				len;
    sInt32				siResult	= eDSNoErr;
    // need to see if this is installed!
    struct stat			data;
    int 				result = eDSNoErr;
    
	DBGLOG( "CSMBPlugin::InitPlugin\n" );
	
    if ( siResult == eDSNoErr && !mNMBLookupToolPath )
    {
        DBGLOG( "CSMBPlugin::InitPlugin getting kNMBLookupToolPath\n" );
        len = OurResources()->GetIndString( resBuff, kCommandParamsID, kNMBLookupToolPath );
        
        if ( len > 0 )
        {
            mNMBLookupToolPath = (char *) malloc( len + 1 );
            if ( mNMBLookupToolPath )
                strcpy( mNMBLookupToolPath, resBuff );
            else
            {
                siResult = memFullErr;
                DBGLOG( "CSMBPlugin::InitPlugin returning memFullErr\n" );
            }
        }
        else
        {
            siResult = kNSLBadReferenceErr;
            DBGLOG( "CSMBPlugin::InitPlugin couldn't load a resource (kNMBLookupToolPath) returning kNSLBadReferenceErr, len=%ld\n", len );
        }
    }

    if ( siResult == eDSNoErr && !mServiceTypeString )
    {
        DBGLOG( "CSMBPlugin::InitPlugin getting kServiceTypeStrID\n" );
        len = OurResources()->GetIndString( resBuff, kCommandParamsID, kServiceTypeStrID );
        
        if ( len > 0 )
        {
            mServiceTypeString = (char *) malloc( len + 1 );
            if ( mServiceTypeString )
                strcpy( mServiceTypeString, resBuff );
            else
            {
                siResult = memFullErr;
                DBGLOG( "CSMBPlugin::InitPlugin returning memFullErr\n" );
            }
        }
        else
        {
            siResult = kNSLBadReferenceErr;
            DBGLOG( "CSMBPlugin::InitPlugin couldn't load a resource (service type) returning kNSLBadReferenceErr, len=%ld\n", len );
        }
    }
    
    if ( siResult == eDSNoErr && !mTemplateConfFilePath )
    {
        DBGLOG( "CSMBPlugin::InitPlugin getting kTemplateConfFilePathStrID\n" );
        len = OurResources()->GetIndString( resBuff, kCommandParamsID, kTemplateConfFilePathStrID );
        
        if ( len > 0 )
        {
            mTemplateConfFilePath = (char *) malloc( len + 1 );
            if ( mTemplateConfFilePath )
			{
                strcpy( mTemplateConfFilePath, resBuff );
            }
			else
            {
                siResult = memFullErr;
                DBGLOG( "CSMBPlugin::InitPlugin returning memFullErr\n" );
            }
        }
        else
        {
            siResult = kNSLBadReferenceErr;
            DBGLOG( "CSMBPlugin::InitPlugin couldn't load a resource (mTemplateConfFilePath) returning kNSLBadReferenceErr, len=%ld\n", len );
        }
    }
    
    if ( siResult == eDSNoErr && !mConfFilePath )
    {
        DBGLOG( "CSMBPlugin::InitPlugin getting kConfFilePathStrID\n" );
        len = OurResources()->GetIndString( resBuff, kCommandParamsID, kConfFilePathStrID );
        
        if ( len > 0 )
        {
            mConfFilePath = (char *) malloc( len + 1 );
            if ( mConfFilePath )
                strcpy( mConfFilePath, resBuff );
            else
            {
                siResult = memFullErr;
                DBGLOG( "CSMBPlugin::InitPlugin returning memFullErr\n" );
            }
        }
        else
        {
            siResult = kNSLBadReferenceErr;
            DBGLOG( "CSMBPlugin::InitPlugin couldn't load a resource (mConfFilePath) returning kNSLBadReferenceErr, len=%ld\n", len );
        }
    }
    
    if ( siResult == eDSNoErr )
    {
        result = stat( mNMBLookupToolPath, &data );
        if ( result < 0 )
        {
            DBGLOG( "SMB couldn't find nmblookup tool: %s (should be at:%s?)\n", strerror(errno), mNMBLookupToolPath );
//            siResult = kNSLBadReferenceErr;
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
		
		AddNode( mLocalNodeString, true );
    }

    return siResult;
}

#define kMaxSizeOfParam 1024
void CSMBPlugin::ReadConfigFile( void )
{
	// we can see if there is a config file, if so then see if they have a WINS server specified
	DBGLOG( "CSMBPlugin::ReadConfigFile\n" );
    FILE *fp;
    char buf[kMaxSizeOfParam];
    
	if ( mConfFilePath )
		fp = fopen(mConfFilePath,"r");
	else
		fp = fopen("/etc/smb.conf","r");
	
    if (fp == NULL) 
	{
        DBGLOG( "CSMBPlugin::ReadConfigFile, couldn't open conf file, copy temp to conf\n" );
		if ( mTemplateConfFilePath && mConfFilePath )
		{
			char		command[256];
			
			snprintf( command, sizeof(command), "/bin/cp %s %s\n", mTemplateConfFilePath, mConfFilePath );
			executecommand( command );

			fp = fopen(mConfFilePath,"r");
		}
		
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

void CSMBPlugin::WriteToConfigFile( void )
{
	// we can see if there is a config file, if so then see if they have a WINS server specified
	DBGLOG( "CSMBPlugin::WriteToConfigFile\n" );
    FILE		*sourceFP = NULL, *destFP = NULL;
    char		buf[kMaxSizeOfParam];
    Boolean		writtenWINS = false;
	Boolean		writtenWorkgroup = false;
	
	if ( mConfFilePath )
		sourceFP = fopen(mConfFilePath,"r+");
	else
		sourceFP = fopen("/etc/smb.conf","r+");
	
    if (sourceFP == NULL) 
	{
        DBGLOG( "CSMBPlugin::WriteToConfigFile, couldn't open conf file, copy temp to conf\n" );
		if ( mTemplateConfFilePath && mConfFilePath )
		{
			char		command[256];
			
			snprintf( command, sizeof(command), "/bin/cp %s %s\n", mTemplateConfFilePath, mConfFilePath );
			executecommand( command );

			sourceFP = fopen(mConfFilePath,"r+");
		}
		
		if (sourceFP == NULL) 
			return;
    }
    
	destFP = fopen( "/tmp/smb.conf.temp", "w" );
	
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
        
        if (buf[0] == '\n' || buf[0] == '\0' || buf[0] == '#' || buf[0] == ';' || (writtenWorkgroup && writtenWINS) )
		{
			fputs( buf, destFP );
			continue;
		}
		
		if ( strstr( buf, "[homes]" ) || strstr( buf, "[public]" ) || strstr( buf, "[printers]" )  )
		{
			// ok, we've passed where this data should go, write out whatever we have left
			if ( !writtenWorkgroup )
				WriteWorkgroupToFile( destFP );
			
			if ( !writtenWINS )
				WriteWINSToFile( destFP );
				
			fputs( buf, destFP );			// now add the line we read
			writtenWorkgroup = true;
			writtenWINS = true;
			
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
		else
			fputs( buf, destFP );			// now add the line we read		
    }

    fclose(sourceFP);
    fclose(destFP);

	{
		char		command[256];
		
		snprintf( command, sizeof(command), "/bin/mv /tmp/smb.conf.temp %s\n", (mConfFilePath)?mConfFilePath:"/etc/smb.conf" );

		executecommand( command );
	}
}

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

		const void*				dictionaryResult	= NULL;
		CNSLDirNodeRep*			nodeDirRep			= NULL;
		
	//    if( !::CFDictionaryGetValueIfPresent( mOpenRefTable, (const void*)inData->fInNodeRef, &dictionaryResult ) )
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

					//here we accept an XML blob to replace the current config file
					//need to make xmlData large enough to receive the data
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
						}
					}
					else if ( newWorkgroupString )
					{
						// Huh? we shouldn't be called if we don't have a mLocalNodeString!
						mLocalNodeString = newWorkgroupString;
							
						configChanged = true;
					}
					
					if ( mWINSServer && newWINSServer )
					{
						if ( strcmp( mWINSServer, newWINSServer ) != 0 )
						{
							free( mWINSServer );
							mWINSServer = newWINSServer;
							
							configChanged = true;
						}
					}
					else if ( newWINSServer )
					{
						mWINSServer = newWINSServer;
							
						configChanged = true;
					}
					else if ( mWINSServer )
					{
						free( mWINSServer );
						mWINSServer = NULL;

						configChanged = true;
					}

					if ( configChanged )
					{
						WriteToConfigFile();

						if ( !(mState & kActive) || (mState & kInactive) )
						{
							ClearOutAllNodes();					// clear these out
							mNodeListIsCurrent = false;			// this is no longer current
							DBGLOG( "CSMBPlugin::DoPlugInCustomCall cleared out all our registered nodes after writing config changes as we are inactive\n" );
						}
						
						if ( (mState & kActive) && mActivatedByNSL )
						{
							AddNode( mLocalNodeString, true );	// add our local reg node
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

    if ( mBroadcastAddr )
        free( mBroadcastAddr );
    mBroadcastAddr = NULL;
	
	siResult = CNSLPlugin::HandleNetworkTransition( inData );

    return ( siResult );
}

#define kMaxTimeToWait	60	// 1 minute?
sInt32 CSMBPlugin::FillOutCurrentState( sDoPlugInCustomCall *inData )
{
	sInt32					siResult	= eDSNoErr;
	UInt32					workgroupDataLen = 0;
	void*					workgroupData = NULL;

//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

	try
	{
		if ( !mNodeListIsCurrent )
		{
			NewNodeLookup();
			
			int	i = 0;
			while ( !mNodeListIsCurrent && i++ < kMaxTimeToWait )
				sleep(1);
		}

		if ( !mNodeListIsCurrent )
			throw( ePlugInCallTimedOut );
			
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

// this is used for top of the node's path "NSL"
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

void CSMBPlugin::AddWINSWorkgroup( const char* workgroup )
{
	if ( !mWINSWorkgroups )
		mWINSWorkgroups = CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		
	if ( !mWINSWorkgroups )
		return;
		
	CFStringRef		tempString = CFStringCreateWithCString( NULL, workgroup, kCFStringEncodingUTF8 );
	
	if ( tempString )
	{
		CFDictionaryAddValue( mWINSWorkgroups, tempString, tempString );		// key and value are the same, we don't care
		CFRelease( tempString );
	}
}

void CSMBPlugin::NewNodeLookup( void )
{
	DBGLOG( "CSMBPlugin::NewNodeLookup\n" );
    
	mNodeListIsCurrent = false;

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

void CSMBPlugin::NewServiceLookup( char* serviceType, CNSLDirNodeRep* nodeDirRep )
{
	DBGLOG( "CSMBPlugin::NewServicesLookup\n" );

    if ( sNMBLookupToolIsAvailable )
    {
        if ( mServiceTypeString && serviceType && strcmp( serviceType, mServiceTypeString ) == 0 )
        {
            CSMBServiceLookupThread* newLookup = new CSMBServiceLookupThread( this, serviceType, nodeDirRep );
            
            // if we have too many threads running, just queue this search object and run it later
            if ( OKToStartNewSearch() )
                newLookup->Resume();
            else
                QueueNewSearch( newLookup );
        }
        else if ( serviceType )
            DBGLOG( "CSMBPlugin::NewServicesLookup skipping as we don't support lookups on type:%s\n", serviceType );
    }
}

Boolean CSMBPlugin::OKToOpenUnPublishedNode( const char* parentNodeName )
{
	return false;
}

Boolean CSMBPlugin::IsWINSWorkgroup( const char* workgroup )
{
	Boolean		isWINSWorkgroup = false;
	
	if ( GetWinsServer() && mWINSWorkgroups )
	{
		CFStringRef		tempString = CFStringCreateWithCString( NULL, workgroup, kCFStringEncodingUTF8 );
		
		if ( tempString )
		{
			isWINSWorkgroup = ( CFDictionaryGetValue( mWINSWorkgroups, tempString ) != NULL );
			CFRelease( tempString );
		}
	}
	
	return isWINSWorkgroup;
}

const char* CSMBPlugin::GetBroadcastAdddress( void )
{
	if ( !mBroadcastAddr )
	{
		char*		address = NULL;
		sInt32		status = GetPrimaryInterfaceBroadcastAdrs( &address );
		
		if ( status )
			DBGLOG( "CSMBPlugin::GetPrimaryInterfaceBroadcastAdrs returned error: %ld\n", status );
		else
		{
			SCNetworkConnectionFlags	connectionFlags;
			
			DBGLOG( "CSMBPlugin::GetPrimaryInterfaceBroadcastAdrs returned Broadcast Address: %s\n", address );
			
			SCNetworkCheckReachabilityByName( address, &connectionFlags );
			
			if ( (connectionFlags & kSCNetworkFlagsReachable) && !(connectionFlags & kSCNetworkFlagsConnectionRequired) && !(connectionFlags & kSCNetworkFlagsTransientConnection) )
			{
				DBGLOG( "CSMBPlugin::GetBroadcastAdddress found address reachable w/o dialup required\n" );
				mBroadcastAddr = address;
			}
			else
			{
				DBGLOG( "CSMBPlugin::GetBroadcastAdddress found address not reachable w/o dialup being initiated, ignoreing\n" );
				free( address );
			}
		}
	}
	
	return mBroadcastAddr;
}

/************************************
 * GetPrimaryInterfaceBroadcastAdrs *
*************************************

	Return the IP addr of the primary interface broadcast address.
*/

sInt32 CSMBPlugin::GetPrimaryInterfaceBroadcastAdrs( char** broadcastAddr )
{
	CFArrayRef			subnetMasks = NULL;
	CFDictionaryRef		globalDict = NULL;
	CFStringRef			key = NULL;
	CFStringRef			primaryService = NULL, router = NULL;
	CFDictionaryRef		serviceDict = NULL;
	SCDynamicStoreRef	store = NULL;
	sInt32				status = 0;
	CFStringRef			subnetMask = NULL;
	CFArrayRef			addressPieces = NULL, subnetMaskPieces = NULL;
	
	
	do {
		store = SCDynamicStoreCreate(NULL, CFSTR("getPrimary"), NULL, NULL);
		if (!store) {
			DBGLOG("SCDynamicStoreCreate() failed: %s\n", SCErrorString(SCError()) );
			status = -1;
			break;
		}
	
		key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
								kSCDynamicStoreDomainState,
								kSCEntNetIPv4);
		
		globalDict = (CFDictionaryRef)SCDynamicStoreCopyValue(store, key);
		CFRelease( key );
		
		if (!globalDict) {
			DBGLOG("SCDynamicStoreCopyValue() failed: %s\n", SCErrorString(SCError()) );
			status = -1;
			break;
		}
	
		primaryService = (CFStringRef)CFDictionaryGetValue(globalDict,
							kSCDynamicStorePropNetPrimaryService);
		if (!primaryService) {
			DBGLOG("no primary service: %s\n", SCErrorString(SCError()) );

			status = -1;
			break;
		}
	
		key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								kSCDynamicStoreDomainState,
								primaryService,
								kSCEntNetIPv4);
		serviceDict = (CFDictionaryRef)SCDynamicStoreCopyValue(store, key);
		
		CFRelease(key);
		if (!serviceDict) {
			DBGLOG("SCDynamicStoreCopyValue() failed: %s\n", SCErrorString(SCError()) );
			status = -1;
			break;
		}
	
		CFArrayRef	addressList = (CFArrayRef)CFDictionaryGetValue(serviceDict, kSCPropNetIPv4Addresses);
		
		router = (CFStringRef)CFDictionaryGetValue(serviceDict,
						kSCPropNetIPv4Router);
		if (!router) {
			
			if ( addressList && CFArrayGetCount(addressList) > 0 )
			{
				// no router, just use our address instead.
				router = (CFStringRef)CFArrayGetValueAtIndex( addressList, 0 );
			}	
			else
			{
				DBGLOG("no router\n" );
				status = -1;
				break;

			}
		}
		
		subnetMasks = (CFArrayRef)CFDictionaryGetValue(serviceDict,
						kSCPropNetIPv4SubnetMasks);
		if (!subnetMasks) {
			DBGLOG("no subnetMasks\n" );
			status = -1;
			break;
		}
	
		addressPieces = CFStringCreateArrayBySeparatingStrings( NULL, router, CFSTR(".") );
	
		if ( subnetMasks )
		{
			subnetMask = (CFStringRef)CFArrayGetValueAtIndex(subnetMasks, 0);
	
			subnetMaskPieces = CFStringCreateArrayBySeparatingStrings( NULL, subnetMask, CFSTR(".") );
		}
		
		char	bcastAddr[256] = {0};
		for (int j=0; j<CFArrayGetCount(addressPieces); j++)
		{
			int	addr = CFStringGetIntValue((CFStringRef)CFArrayGetValueAtIndex(addressPieces, j));
			int mask = CFStringGetIntValue((CFStringRef)CFArrayGetValueAtIndex(subnetMaskPieces, j));
			int invMask = (~mask & 255);
			int bcast = invMask | addr;
			
			char	bcastPiece[5];
			snprintf( bcastPiece, sizeof(bcastPiece), "%d.", bcast );
			strcat( bcastAddr, bcastPiece );
		}
		
		bcastAddr[strlen(bcastAddr)-1] = '\0';
		
		*broadcastAddr = (char*)malloc(strlen(bcastAddr)+1);
		strcpy( *broadcastAddr, bcastAddr );
	} while (false);
	
	if ( serviceDict )
		CFRelease( serviceDict );

	if ( globalDict )
		CFRelease( globalDict );

	if ( store )
		CFRelease( store );
		
	if ( addressPieces )
		CFRelease( addressPieces );
	
	if ( subnetMaskPieces )
		CFRelease( subnetMaskPieces );
		
	return status;

}


Boolean ExceptionInResult( const char* resultPtr )
{
    // for now we are just going to strstr for "Exception"
    return (strstr(resultPtr, "Exception") != 0 || strncmp(resultPtr, "dyld: sh:", strlen("dyld: sh:")) == 0 );
}

/***************
 * IsIPAddress *
 ***************
 
 Verifies a CString is a legal dotted-quad format. If it fails, it returns the 
 partial IP address that was collected.
 
*/

int IsIPAddress(const char* adrsStr, long *ipAdrs)
{
	short	i,accum,numOctets,lastDotPos;
	long	tempAdrs;
	register char	c;
	char	localCopy[20];					// local copy of the adrsStr
	
	strncpy(localCopy, adrsStr,sizeof(localCopy)-1);
	*ipAdrs = tempAdrs = 0;
	numOctets = 1;
	accum = 0;
	lastDotPos = -1;
	for (i = 0; localCopy[i] != 0; i++)	{	// loop 'til it hits the NUL
		c = localCopy[i];					// pulled this out of the comparison part of the for so that it is more obvious	// KA - 5/29/97
		if (c == '.')	{
			if (i - lastDotPos <= 1)	return 0;	// no digits
			if (accum > 255) 			return 0;	// only 8 bits, guys
			*ipAdrs = tempAdrs = (tempAdrs<<8) + accum; // copy back result so far
			accum = 0; 
			lastDotPos = i;							
			numOctets++;								// bump octet counter
		}
		else if ((c >= '0') && (c <= '9'))	{
			accum = accum * 10 + (c - '0');				// [0-9] is OK
		}
		else return 0;								// bogus character
	}
	
	if (accum > 255) return 0;						// if not too big...
	tempAdrs = (tempAdrs<<8) + accum;					// add in the last byte
	*ipAdrs = tempAdrs;									// return real IP adrs

	if (numOctets != 4)									// if wrong count
		return 0;									// 	return FALSE;
	else if (i-lastDotPos <= 1)							// if no last byte
		return 0;									//  return FALSE
	else	{											// if four bytes
		return 1;									// say it worked
	}
}

/*************
 * IsDNSName *
 *************
 
 Verify a CString is a valid dot-format DNS name. Check that:
 
 ¥  the name only contains letters, digits, hyphens, and dots; 
 ¥	no label begins or ends with a "-";
 ¥	the name doesn't begin with ".";
 ¥	labels are between 1 and 63 characters long;
 ¥	the entire length isn't > 255 characters;
 ¥	allow "." as a legal name
 ¥	will NOT allow an all-numeric name (ie, 1.2.3.4 will fail)
 .	must have at least ONE dot
*/

Boolean IsDNSName(char* theName)
{
	short	i;
	short	len;
	short	lastDotPos;
	short	lastDashPos;
	register char 	c;
	Boolean seenAlphaChar;
	
	if ( !strstr(theName, ".") )
		return FALSE;
		
	if ((strlen(theName) == 1) && (theName[0] == '.')) return TRUE;	// "." is legal...
	len = 0; 
	lastDotPos = -1;					// just "before" the start of string
	lastDashPos = -1;				
	seenAlphaChar = FALSE;
	for (i = 0; c = theName[i]; i++, len++)	{
	
		if (len > 255)	return FALSE;	// whole name is too long
		
		if (c == '-')	{
			if (lastDotPos == i-1) return FALSE;	// no leading "-" in labels 
			lastDashPos = i;
		}
		else if (c == '.')	{			// check label lengths
			if (lastDashPos == i-1)	 	 return FALSE; // trailing "-" in label
			if (i - lastDotPos - 1 > 63) return FALSE; // label too long
			if (i - lastDotPos <= 1) 	 return FALSE; // zero length label
			lastDotPos = i;
		}		
		else if (isdigit(c))	{ 		// any numeric chars are OK
			// nothing
		}
		else if (isalpha(c))	{ 		// lower or upper case, too.
			seenAlphaChar = TRUE;
		}
		else return FALSE;				// but nothing else
	}
	return seenAlphaChar;
}

