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
 *  @header CSLPServiceLookupThread
 */

#include "CNSLPlugin.h"
#include "CSLPServiceLookupThread.h"
#include "CNSLDirNodeRep.h"
#include "CNSLResult.h"
#include "NSLDebugLog.h"
#include "CSLPPlugin.h"
#include "CommandLineUtilities.h"

#include <CoreServices/CoreServices.h>

static pthread_mutex_t	gSLPNotifierLock;
static Boolean			gLockInitialized = false;

const CFStringRef		kDSNAttrDNSNameSAFE_CFSTR = CFSTR(kDSNAttrDNSName);

SLPBoolean SLPServiceLookupNotifier( SLPHandle hSLP, const char* pcSrvURL, short unsigned int sLifeTime, SLPInternalError errCode, void* pvCookie );
void AddHostNameIfPossible( CNSLResult* newResult );
CFStringRef CreateHostNameFromURL( CFStringRef	urlStringRef );
void HandleAttributeAddition( CNSLResult* newResult, char* keyStr, char* valueStr );
char* CreateDecodedString( char* rawString );
tDirStatus HexDecodeText( const char* encodedText, UInt16 encodedTextLen, char* decodedTextBuffer, UInt16* decodedTextBufferLen, Boolean* textChanged );

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
			size_t		urlLen = strlen(pcSrvURL);
			char		stackBuf[1024];
            char*		ourURLCopy = stackBuf;
            char*		nameTagPtr = NULL;
			
			if ( urlLen+1 > sizeof(stackBuf) )
				ourURLCopy = (char*)malloc( urlLen+1 );
				
            strcpy( ourURLCopy, pcSrvURL );
    
            
            for ( int i=0; ourURLCopy[i] != '\0' && strncmp( &ourURLCopy[i], ":/", 2 ) != 0; i++ )
                serviceType[i] = ourURLCopy[i];
            
            char*	namePtr = NULL;
			
			nameTagPtr = strstr( ourURLCopy, "/?NAME=" );
            if ( !nameTagPtr )
            {
                nameTagPtr = strstr( ourURLCopy, "?NAME=" );
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
            if ( nameTagPtr)
            {
                // ok, this is a hack registration to include the display name
                DBGLOG( "SLPServiceLookupNotifier, found a ?NAME= tag and will try and pull out the display name (%s)\n", namePtr );
				
				namePtr = strstr( nameTagPtr, "&ZONE=" );
				
				if ( namePtr )
					*namePtr = '\0';		// take off this legacy tag
					
                namePtr = nameTagPtr + numCharsToAdvance;			// now look at whats at the end of the name tag
                
				if ( namePtr[0] != '\0' )
				{
					if ( !newResult->GetAttributeRef(kDSNAttrRecordNameSAFE_CFSTR) )		// only do this if we haven't found a name attribute
					{
						DBGLOG( "SLPServiceLookupNotifier, display name raw is (%s)\n", namePtr );
						char		decodedName[256] = {0};
						UInt16		decodedNameLen = sizeof(decodedName) - 1;		// save room for null terminator
						Boolean		wasChanged;
						CFStringRef	nameRef = NULL;
						
						char*	zonePtr = strstr( namePtr, "&ZONE=" );
						
						if ( zonePtr )
							*zonePtr = '\0';		// NULL out this Shareway IP zone info
	
						if ( HexDecodeText( namePtr, strlen(namePtr), decodedName, &decodedNameLen, &wasChanged ) == eDSNoErr )
						{
							DBGLOG( "SLPServiceLookupNotifier hex decoded name to be: %s, using encoding %ld\n", decodedName, NSLGetSystemEncoding() );
							nameRef = CFStringCreateWithCString( NULL, decodedName, NSLGetSystemEncoding() );	// these are system encoded
						}
						else
						{
							DBGLOG( "SLPServiceLookupNotifier name not hex encoded, adding: %s, using encoding %ld\n", namePtr, NSLGetSystemEncoding() );
	
							nameRef = CFStringCreateWithCString( NULL, namePtr, NSLGetSystemEncoding() );	// these are system encoded
						}
						
						if ( nameRef )
						{
							newResult->AddAttribute( kDSNAttrRecordNameSAFE_CFSTR, nameRef );
							CFRelease( nameRef );
						}
						else
						{
							namePtr = NULL;					// set the pointer to null so we will use the hostname/ipaddress below
							DBGLOG( "SLPServiceLookupNotifier couldn't create a CFString from the name, encoding issue perhaps?\n" );
						}
					}
				}
				else
				{
					*nameTagPtr = '\0';				// so we effectively blank the ?NAME= portion out
					namePtr = NULL;					// and set the pointer to null so we will use the hostname/ipaddress below
				}
            }
            
			if ( !namePtr )
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
            
            if ( !CFDictionaryGetValue( newResult->GetAttributeDict(), kDSNAttrDNSNameSAFE_CFSTR ) )
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
                
            if ( ourURLCopy != stackBuf )
				free( ourURLCopy );
        }
        
        pthread_mutex_unlock( &gSLPNotifierLock );
    }

    return wantMoreData;
}

void AddHostNameIfPossible( CNSLResult* newResult )
{
    CFStringRef		hostNameRef = NULL;
    
    if ( (hostNameRef = CreateHostNameFromURL( (CFStringRef)CFDictionaryGetValue( newResult->GetAttributeDict(), kDSNAttrDNSNameSAFE_CFSTR ) ) ) )
    {
        if ( getenv("NSLDEBUG") )
        {
            DBGLOG("AddHostNameIfPossible, grabbed host name from URL\n");
            CFShow( hostNameRef );
        }
        
        newResult->AddAttribute( kDSNAttrDNSNameSAFE_CFSTR, hostNameRef );
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
        char		stackBuf[256];
		char*		dsNativeKeyType = stackBuf;
		size_t		newKeyLen = strlen(kDSNativeAttrTypePrefix) + strlen(keyStr) + 1;
		
		if ( newKeyLen > (CFIndex)sizeof(stackBuf) )
			dsNativeKeyType = (char*)malloc( newKeyLen );
			
        strcpy( dsNativeKeyType, kDSNativeAttrTypePrefix );
        strcat( dsNativeKeyType, keyStr );
        
        newResult->AddAttribute( dsNativeKeyType, valuePtrToUse );

		if ( dsNativeKeyType != stackBuf )
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

                newUniChar = temp1 + temp2;
                strncat( buffer, &newUniChar, 1 );

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


char DecodeHexToChar( const char* oldHexTriplet, Boolean* wasHexTriplet  )
{
	char c, c1, c2;
	
	*wasHexTriplet = false;
	
	c = *oldHexTriplet;

	if ( c == '%' )
	{
		// Convert %xx to ascii equivalent
		c1 = tolower(oldHexTriplet[1]);
		c2 = tolower(oldHexTriplet[2]);
		if (isxdigit(c1) && isxdigit(c2)) 
		{
			c1 = isdigit(c1) ? c1 - '0' : c1 - 'a' + 10;
			c2 = isdigit(c2) ? c2 - '0' : c2 - 'a' + 10;
			c = (c1 << 4) | c2;
			
			*wasHexTriplet = true;
		}
	}
	
	return c;
}

/*****************
 * HexDecodeText *
 *****************
 
 This function will change all encoded text (%XX) to its character equivalent.
 
*/

tDirStatus HexDecodeText( const char* encodedText, UInt16 encodedTextLen, char* decodedTextBuffer, UInt16* decodedTextBufferLen, Boolean* textChanged )
{
	const char*		curReadPtr = encodedText;
	char*			curWritePtr = decodedTextBuffer;
	tDirStatus		status = eDSNoErr;
	Boolean			wasHex;
	
	if ( !encodedText || !decodedTextBuffer || !decodedTextBufferLen || !textChanged )
		status = eDSNullParameter;
	
	*textChanged = false;	// preset
	
	while ( !status && (curReadPtr <= encodedText+encodedTextLen) )
	{
		if ( curWritePtr > decodedTextBuffer + *decodedTextBufferLen )
		{
			status = eDSBufferTooSmall;
			break;
		}
		
		if ( *curReadPtr == '%' )
		{
			*curWritePtr = DecodeHexToChar( curReadPtr, &wasHex );
			
			if ( !wasHex )
				status = eDSInvalidAttributeType;
			else
			{
				curWritePtr++;
				curReadPtr += 3;
				*textChanged = true;
			}
		}
		else
		{
			*curWritePtr = *curReadPtr;
			curWritePtr++;
			curReadPtr++;
		}
	}
	
	if ( !status )
		*decodedTextBufferLen = (curWritePtr-decodedTextBuffer)-1;	// <14>
	else
		*decodedTextBufferLen = 0;
		
	return status;
}
