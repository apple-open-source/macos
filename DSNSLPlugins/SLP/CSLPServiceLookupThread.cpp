/*
 *  CSLPServiceLookupThread.cpp
 *  DSSLPPlugIn
 *
 *  Created by imlucid on Wed Aug 15 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

//#include <Carbon/Carbon.h>
#include "CNSLPlugin.h"
#include "CSLPServiceLookupThread.h"
#include "CNSLDirNodeRep.h"
#include "CNSLResult.h"
#include "NSLDebugLog.h"

#include "CSLPPlugin.h"
#include "CommandLineUtilities.h"

#define	kRecentsFolder		"Recent Servers"
#define kFavoritesFolder	"Favorite Servers"

static pthread_mutex_t	gSLPNotifierLock;
static Boolean			gLockInitialized = false;

SLPBoolean SLPServiceLookupNotifier( SLPHandle hSLP, const char* pcSrvURL, short unsigned int sLifeTime, SLPInternalError errCode, void* pvCookie );
void AddHostNameIfPossible( CNSLResult* newResult );
CFStringRef CreateHostNameFromURL( CFStringRef	urlStringRef );
void HandleAttributeAddition( CNSLResult* newResult, char* keyStr, char* valueStr );
char* CreateDecodedString( char* rawString );

CSLPServiceLookupThread::CSLPServiceLookupThread( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep )
    : CNSLServiceLookupThread( parentPlugin, serviceType, nodeDirRep )
{
	DBGLOG( "CSLPServiceLookupThread::CSLPServiceLookupThread\n" );

    if ( getenv( "com.apple.slp.interface" ) != 0 )
    {
        SLPSetProperty("com.apple.slp.interface", getenv( "com.apple.slp.interface" ) );
DBGLOG( "Setting interface to: %s", getenv( "com.apple.slp.interface" ) );
    }
    
    mSLPRef = 0;
}

CSLPServiceLookupThread::~CSLPServiceLookupThread()
{
	DBGLOG( "CSLPServiceLookupThread::~CSLPServiceLookupThread\n" );
    if ( mSLPRef )
        SLPClose( mSLPRef );
}

void* CSLPServiceLookupThread::Run( void )
{
    char		serviceType[512] = {0};	// CFStringGetLength returns possibly 16 bit values!

	DBGLOG( "CSLPServiceLookupThread::Run\n" );
    
    if ( !gLockInitialized )
    {
        gLockInitialized = true;
        pthread_mutex_init( &gSLPNotifierLock, NULL );
    }

    if ( AreWeCanceled() )
    {
        DBGLOG( "CSLPServiceLookupThread::Run, we were canceled before we even started\n" );
    }
    else if ( GetServiceTypeRef() && ::CFStringGetCString(GetServiceTypeRef(), serviceType, sizeof(serviceType), kCFStringEncodingUTF8) )
    {
#ifdef TURN_ON_RECENTS_FAVORITES
        if ( getenv("RECENTSFAVORITES") && CFStringCompare( GetNodeName(), ((CSLPPlugin*)GetParentPlugin())->GetRecentFolderName(), 0 ) == kCFCompareEqualTo )
        {
            GetRecFavServices( true, serviceType, GetNodeToSearch() ); 
        }
        else
        if ( getenv("RECENTSFAVORITES") && CFStringCompare( GetNodeName(), ((CSLPPlugin*)GetParentPlugin())->GetFavoritesFolderName(), 0 ) == kCFCompareEqualTo )
        {
            GetRecFavServices( false, serviceType, GetNodeToSearch() ); 
        }
        else
 #endif
        {
            char		searchScope[512] = {0};	// CFStringGetLength returns possibly 16 bit values!
        
            SLPInternalError status = SLPOpen( "en", SLP_FALSE, &mSLPRef );
            
            if ( status )
            {	
                DBGLOG( "CSLPServiceLookupThread::Run, SLPOpen returned %d\n", status );
            }
            else if ( !AreWeCanceled() )
            {
                if ( GetNodeName() && ::CFStringGetCString(GetNodeName(), searchScope, sizeof(searchScope), kCFStringEncodingUTF8) )
                {
                    if ( GetServiceTypeRef() && ::CFStringGetCString(GetServiceTypeRef(), serviceType, sizeof(serviceType), kCFStringEncodingUTF8) )
                    {
                        int	status = SLPFindSrvs( mSLPRef, serviceType, searchScope, "", SLPServiceLookupNotifier, this );
                        
                        if ( status )
                        {	
                            DBGLOG( "CSLPServiceLookupThread::Run, SLPFindSrvs returned %d\n", status );
                        }
                    }
                    else
                        DBGLOG( "CSLPServiceLookupThread::Run, CFStringGetCString returned false on the serviceType" );
                }
                else
                    DBGLOG( "CSLPServiceLookupThread::Run, CFStringGetCString returned false on the searchScope" );
            }
        }
    }
    
    return NULL;
}

#ifdef TURN_ON_RECENTS_FAVORITES
void CSLPServiceLookupThread::GetRecFavServices( Boolean recents, char* serviceType, CNSLDirNodeRep* nodeToSearch )
{
    char		command[512];
    char*		result = NULL;
    char*		resultPtr = NULL;
    Boolean		canceled = false;
    
    if ( recents )
        sprintf( command, "recents_favorites -r %s", serviceType );
    else
        sprintf( command, "recents_favorites -f %s", serviceType );

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
#endif
SLPBoolean SLPServiceLookupNotifier( SLPHandle hSLP, const char* pcSrvURL, short unsigned int sLifeTime, SLPInternalError errCode, void* pvCookie )
{
    CSLPServiceLookupThread*		lookupObj = (CSLPServiceLookupThread*)pvCookie;
    SLPBoolean						wantMoreData = SLP_FALSE;
    
    if ( pcSrvURL )
    {
        pthread_mutex_lock( &gSLPNotifierLock );
        DBGLOG( "SLPServiceLookupNotifier called with pcSrvURL = %s\n", pcSrvURL );

        if ( lookupObj && errCode == SLP_OK && !lookupObj->AreWeCanceled() )
        {
            CNSLResult* newResult = new CNSLResult();
            char		serviceType[256] = {0};
            long		numCharsToAdvance = 7;
            char*		ourURLCopy = (char*)malloc( strlen(pcSrvURL) +1 );
            
            strcpy( ourURLCopy, pcSrvURL );
    
            
            for ( int i=0; ourURLCopy[i] != '\0' && strncmp( &ourURLCopy[i], ":/", 2 ) != 0; i++ )
                serviceType[i] = ourURLCopy[i];
            
            char*	namePtr = strstr( ourURLCopy, "/?NAME=" );
            if ( !namePtr )
            {
                namePtr = strstr( ourURLCopy, "?NAME=" );
                numCharsToAdvance--;
            }
            
#ifdef REGISTER_WITH_ATTRIBUTE_DATA_IN_URL
            char*	attrPortionPtr = strstr( ourURLCopy, ";" );
            if ( attrPortionPtr )
            {
                DBGLOG( "SLPServiceLookupNotifier attribute list is: %s\n", attrPortionPtr );
                
                attrPortionPtr[0] = '\0';
                attrPortionPtr++;
                // this url has attributes embeded in it, parse them out.
                while ( attrPortionPtr )
                {
                    DBGLOG( "**TEMP** SLPServiceLookupNotifier working on attributelist part: %s\n", attrPortionPtr );
                    char*	keyStr = attrPortionPtr;
                    char*	valueStr;
                    char*	nextKeyStr = strstr( keyStr, ";" );

                    if ( nextKeyStr )
                    {
                        nextKeyStr[0] = '\0';
                        nextKeyStr++;
                        attrPortionPtr = nextKeyStr;
                    }
                    else
                        attrPortionPtr = NULL;

                    DBGLOG( "**TEMP** SLPServiceLookupNotifier working on attributelist key/value: %s\n", keyStr );
                        
                    DBGLOG( "**TEMP** SLPServiceLookupNotifier attrPortionPtr for next round points to: %s\n", attrPortionPtr );
                    valueStr = strstr( keyStr, "=" );
                    
                    if ( valueStr )
                    {
                        valueStr[0] = '\0';
                        valueStr++;
                        
                        DBGLOG( "**TEMP** SLPServiceLookupNotifier working on attributelist value part: %s\n", valueStr );
                        char*	endValueStr = strstr( valueStr, "," );
                        
                        while ( endValueStr )
                        {
                            DBGLOG( "**TEMP** SLPServiceLookupNotifier working on attributelist value portion: %s\n", valueStr );
                            endValueStr[0]='\0';
                            
                            HandleAttributeAddition( newResult, keyStr, valueStr );

                            valueStr = endValueStr++;
                            endValueStr = strstr( valueStr, "," );
                        }
                        
                        HandleAttributeAddition( newResult, keyStr, valueStr );
                    }
                    else
                    {
                        // no value, just the attribute
                        DBGLOG( "**TEMP** SLPServiceLookupNotifier adding key with no attribute: %s\n", keyStr );
                        newResult->AddAttribute( keyStr, "" );
                    }
                }
            }
#endif            
            if ( namePtr)
            {
                // ok, this is a hack registration to include the display name
                DBGLOG( "SLPServiceLookupNotifier, found a ?NAME= tag and will try and pull out the display name (%s)\n", namePtr );
//                *namePtr = '\0';						// don't return this as the url
                namePtr += numCharsToAdvance;			// advance to the name
                
                if ( !newResult->GetAttributeRef(CFSTR(kDSNAttrRecordName)) )		// only do this if we haven't found a name attribute
                {
                    DBGLOG( "SLPServiceLookupNotifier, display name raw is (%s)\n", namePtr );
                    char		decodedName[256] = {0};
                    UInt16		decodedNameLen = sizeof(decodedName) - 1;		// save room for null terminator
                    Boolean		wasChanged;
                    CFStringRef	nameRef = NULL;
					
                    char*	zonePtr = strstr( namePtr, "&ZONE=" );
                    
                    if ( zonePtr )
                        *zonePtr = '\0';		// NULL out this Shareway IP zone info

                    if ( NSLHexDecodeText( namePtr, strlen(namePtr), decodedName, &decodedNameLen, &wasChanged ) == noErr )
                    {
                        DBGLOG( "SLPServiceLookupNotifier hex decoded name to be: %s, using encoding %ld\n", decodedName, NSLGetSystemEncoding() );
                        nameRef = CFStringCreateWithCString( NULL, decodedName, NSLGetSystemEncoding() );	// these are system encoded
						//newResult->AddAttribute( kDSNAttrRecordName, decodedName );
                    }
                    else
                    {
                        DBGLOG( "SLPServiceLookupNotifier name not hex encoded, adding: %s, using encoding %ld\n", namePtr, NSLGetSystemEncoding() );
                       // newResult->AddAttribute( kDSNAttrRecordName, namePtr );
                        nameRef = CFStringCreateWithCString( NULL, namePtr, NSLGetSystemEncoding() );	// these are system encoded
                    }
					
					newResult->AddAttribute( CFSTR(kDSNAttrRecordName), nameRef );
					CFRelease( nameRef );
                }
            }
            else
            {
                // We want to set the name to be the portion after the URL
                namePtr = strstr( ourURLCopy, ":/" );
                if ( namePtr )
                {
                    namePtr+=2;
                    namePtr = strstr( namePtr, "/");
                }
                    
                if ( namePtr )
                    namePtr++;
                    
                if ( namePtr )
                {
                    DBGLOG( "SLPServiceLookupNotifier, making name out of the url: %s\n", namePtr );
                    newResult->AddAttribute( kDSNAttrRecordName, namePtr );
                }    
            }
            
            
            DBGLOG( "SLPServiceLookupNotifier creating new result with type=%s url=%s\n", serviceType, ourURLCopy );
    
            newResult->SetURL( ourURLCopy );
            newResult->SetServiceType( serviceType );
            
            if ( !CFDictionaryGetValue( newResult->GetAttributeDict(), CFSTR(kDSNAttrDNSName) ) )
            {
                // no host name, let's try and figure one out
                AddHostNameIfPossible( newResult );
            }
            
            if ( getenv( "NSLDEBUG" ) )
            {
                ::CFShow( newResult->GetAttributeDict() );
            }
            
            lookupObj->AddResult( newResult );
            wantMoreData = SLP_TRUE;					// still going!
                
            free( ourURLCopy );
        }
        
        pthread_mutex_unlock( &gSLPNotifierLock );
    }

    return wantMoreData;
}

void AddHostNameIfPossible( CNSLResult* newResult )
{
    CFStringRef		hostNameRef = NULL;
    
    if ( (hostNameRef = CreateHostNameFromURL( (CFStringRef)CFDictionaryGetValue( newResult->GetAttributeDict(), CFSTR(kDSNAttrDNSName) ) ) ) )
    {
        if ( getenv("NSLDEBUG") )
        {
            DBGLOG("AddHostNameIfPossible, grabbed host name from URL\n");
            CFShow( hostNameRef );
        }
        
        newResult->AddAttribute( CFSTR(kDSNAttrDNSName), hostNameRef );
        CFRelease( hostNameRef );
    }
}

CFStringRef CreateHostNameFromURL( CFStringRef	urlStringRef )
{
    CFStringRef				hostName = NULL;
    
    CFURLRef				urlRef = CFURLCreateWithString( NULL, urlStringRef, NULL );
    
    if ( urlRef )
        hostName = CFURLCopyHostName( urlRef );

    return hostName;
}

void HandleAttributeAddition( CNSLResult* newResult, char* keyStr, char* valueStr )
{
    // we want to map certain keys to standard DS types.  Everything else we will just map to a native DS type
    char*		decodedValue = CreateDecodedString( valueStr );
    char*		valuePtrToUse = (decodedValue) ? decodedValue : valueStr;
    
    if ( SDstrcasecmp( keyStr, "name" ) == 0 )
        newResult->AddAttribute( kDSNAttrRecordName, valuePtrToUse );
    else if ( SDstrcasecmp( keyStr, "Port" ) == 0 )
        newResult->AddAttribute( kDS1AttrPort, valuePtrToUse );
    else if ( SDstrcasecmp( keyStr, "IPAddress" ) == 0 )
        newResult->AddAttribute( kDSNAttrIPAddress, valuePtrToUse );
    else
    {
        char*		dsNativeKeyType = (char*)malloc( strlen(kDSNativeAttrTypePrefix) + strlen(keyStr) + 1 );
        strcpy( dsNativeKeyType, kDSNativeAttrTypePrefix );
        strcat( dsNativeKeyType, keyStr );
        
        newResult->AddAttribute( dsNativeKeyType, valuePtrToUse );

        free( dsNativeKeyType );
    }

    if ( decodedValue )
        free( decodedValue );
}



char* CreateDecodedString( char* rawString )
{
    char*					curPtr = (rawString)?strstr( rawString, "\\" ) : NULL;			// are there any encoded values?
    
    if ( curPtr )
    {
        char*					buffer = (char*)malloc(strlen(rawString)+1);
        
        buffer[0] = '\0';
        
        DBGLOG( "CreateDecodedString called on %s\n", rawString );
        
        curPtr = rawString;
        
        while ( curPtr )
        {
            if ( *curPtr == '\\' )
            {
                char	newUniChar;
                char	temp1, temp2;
                
                if ( curPtr[1] >= 'A' && curPtr[1] <= 'F' )
                    temp1 = (10 + (curPtr[1] - 'A'))*16;
                else if ( curPtr[1] >= '0' && curPtr[1] <= '9' )
                    temp1 = (10 + (curPtr[1] - '9') - 1)*16;
                else
                {
                    fprintf( stderr, "SLP received a improper encoded attribute value\n" );
                    free( buffer );
                    return NULL;
                }
//                strncat( buffer, &curPtr[1], 1 );
DBGLOG( "CreateDecodedString, first char %c should map to 0x%x\n", curPtr[1], temp1 );   
                if ( curPtr[2] >= 'A' && curPtr[2] <= 'F' )
                    temp2 = (10 + (curPtr[2] - 'A'));
                else if ( curPtr[2] >= '0' && curPtr[2] <= '9' )
                    temp2 = (10 + (curPtr[2] - '9') - 1);
                else
                {
                    fprintf( stderr, "SLP received a improper encoded attribute value\n" );
                    free( buffer );
                    return NULL;
                }

DBGLOG( "CreateDecodedString, second char %c should map to 0x%x\n", curPtr[2], temp2 );   

                newUniChar = temp1 + temp2;
                strncat( buffer, &newUniChar, 1 );

DBGLOG( "CreateDecodedString, chars to convert is %c%c, value converted to is 0x%x\n", curPtr[1], curPtr[2], newUniChar );                

                curPtr+=3;   
            }
            else
            {
                strncat( buffer, curPtr, 1 );
                curPtr++;
            }
            
            if ( *curPtr == '\0' )
                break;
        }
    
        DBGLOG( "CreateDecodedString returning %s\n", buffer );

        return buffer;
    }
    else
        DBGLOG( "CreateDecodedString, didn't find encoding in %s\n", rawString );
        
    return NULL;
}



                            

