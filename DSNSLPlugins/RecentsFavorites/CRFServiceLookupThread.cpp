/*
 *  CRFServiceLookupThread.cpp
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Thu Feb 28 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */

#include "CRFPlugin.h"
#include "CRFServiceLookupThread.h"
#include "CNSLDirNodeRep.h"
#include "CNSLResult.h"
#include "NSLDebugLog.h"

#include "CommandLineUtilities.h"


CRFServiceLookupThread::CRFServiceLookupThread( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep )
    : CNSLServiceLookupThread( parentPlugin, serviceType, nodeDirRep )
{
	DBGLOG( "CRFServiceLookupThread::CRFServiceLookupThread\n" );
}

CRFServiceLookupThread::~CRFServiceLookupThread()
{
	DBGLOG( "CRFServiceLookupThread::~CRFServiceLookupThread\n" );
}

void* CRFServiceLookupThread::Run( void )
{
    char		serviceType[512] = {0};	// CFStringGetLength returns possibly 16 bit values!

	DBGLOG( "CRFServiceLookupThread::Run\n" );
    
    if ( AreWeCanceled() )
    {
        DBGLOG( "CRFServiceLookupThread::Run, we were canceled before we even started\n" );
    }
    else if ( CFStringCompare( GetNodeName(), CFSTR(kLocalManagedDataName), 0 ) == kCFCompareEqualTo )
    {
        // just need to get a CFDictionary from an xml file.
        GetRecordsFromType();
    }
    else if ( GetServiceTypeRef() && ::CFStringGetCString(GetServiceTypeRef(), serviceType, sizeof(serviceType), kCFStringEncodingUTF8) )
    {
        if ( CFStringCompare( GetNodeName(), ((CRFPlugin*)GetParentPlugin())->GetRecentFolderName(), 0 ) == kCFCompareEqualTo )
        {
            GetRecFavServices( true, serviceType, GetNodeToSearch() ); 
        }
        else
        if ( CFStringCompare( GetNodeName(), ((CRFPlugin*)GetParentPlugin())->GetFavoritesFolderName(), 0 ) == kCFCompareEqualTo )
        {
            GetRecFavServices( false, serviceType, GetNodeToSearch() ); 
        }
    }
    
    return NULL;
}

void CRFServiceLookupThread::ReadRecordFromFile( void )
{
}

#define 	kMaxItemsToIterate	10
void CRFServiceLookupThread::GetRecordsFromType( void )
{
    CFURLRef	urlPath = NULL;
    CFURLRef	urlBasePath = NULL;
    CFStringRef	sPath;
    FSRef		fsRef;
    FSIterator	fsIterator;
    OSErr		err;
    
    DBGLOG( "GetRecordsFromType called\n" );
    urlBasePath = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, CFSTR(kLocalManagedDataBasePath), kCFURLPOSIXPathStyle, true );
    urlPath = ::CFURLCreateWithFileSystemPathRelativeToBase( kCFAllocatorDefault, GetServiceTypeRef(), kCFURLPOSIXPathStyle, true, urlBasePath );
    ::CFRelease( sPath );
    sPath = nil;
    
    if ( getenv("NSLDEBUG") )
        CFShow( urlPath );
    if ( CFURLGetFSRef( urlPath, &fsRef ) && FSOpenIterator( &fsRef, kFSIterateFlat, &fsIterator ) )
    {
        ItemCount		numItemsFound = 0;
        HFSUniStr255	itemNames[kMaxItemsToIterate];
        
        while ( (err = FSGetCatalogInfoBulk( fsIterator, kMaxItemsToIterate, &numItemsFound, NULL, kFSCatInfoNone, NULL, NULL, NULL, itemNames)) == noErr )
        {
            DBGLOG( "FSGetCatalogInfoBulk found %ld items\n", numItemsFound );
            
            for ( ItemCount index = 0; index < numItemsFound; index++ )
            {
                CNSLResult* newResult = new CNSLResult();
                CFStringRef	newName = CFStringCreateWithCharacters( NULL, itemNames[index].unicode, itemNames[index].length );

                if ( getenv("NSLDEBUG") )
                    CFShow( newName );
                
                newResult->AddAttribute( CFSTR(kDSNAttrRecordName), newName );
                newResult->SetServiceType( GetServiceTypeRef() );
                GetNodeToSearch()->AddService( newResult );
                
                CFRelease( newName );
            }
        }
        
        FSCloseIterator( fsIterator );
    }
}

void CRFServiceLookupThread::GetRecFavServices( Boolean recents, char* serviceType, CNSLDirNodeRep* nodeToSearch )
{
    char		command[512];
    char*		result = NULL;
    char*		resultPtr = NULL;
    Boolean		canceled = false;
	
    if ( recents )
        sprintf( command, "/usr/sbin/recents_favorites -r %s", serviceType );
    else
        sprintf( command, "/usr/sbin/recents_favorites -f %s", serviceType );

    myexecutecommandas( command, true, 20, &result, &canceled, nodeToSearch->GetUID(), getgid());		// need to get uid and gid from Dir Services
    
    resultPtr = result;
    
    while ( resultPtr )
    {
        char*	newResult = resultPtr;
        
        resultPtr = strstr( resultPtr, "\n" );
        if ( resultPtr )
        {
            resultPtr[0] = '\0';
            resultPtr++;
        }
        
        char*	newName = newResult;
        char*	newURL	= strstr( newName, "\t" );
        
        if ( newName && newURL )
        {
            newURL[0] = '\0';
            newURL++;
            
            CNSLResult* newResult = new CNSLResult();
            newResult->AddAttribute( kDSNAttrRecordName, newName );
            newResult->SetURL( newURL );
            newResult->SetServiceType( serviceType );

            nodeToSearch->AddService( newResult );
        }
    }
    
    if ( result )
        free ( result );
}





                            

