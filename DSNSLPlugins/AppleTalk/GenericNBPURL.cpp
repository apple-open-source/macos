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
 *  @header GenericNBPURL
 */
 
#include <stdio.h>
#include <string.h>
#include <CoreServices/CoreServices.h>
#include "GenericNBPURL.h"
#include "NSLDebugLog.h"

OSStatus HexEncodeText( const char* rawText, UInt16 rawTextLen, char* newTextBuffer, UInt16* newTextBufferLen, Boolean* textChanged );

void MakeGenericNBPURL(const char *entityType, const char *zoneName, const char *name, char* returnBuffer, UInt16* returnBufferLen )
{
    char safeEntityType[256*3];
    char safeZoneName[32*3], safeName[32*3];
    Size urlLength = 0;
    
    if ( entityType && zoneName && name )
    {
        // hex-encode our substrings
        {
            char tempBuf[256];
            UInt16 bufLen = 256;
            Boolean textChanged;
            OSStatus status;
            UInt16 entityTypeLen = strlen(entityType);
            
            //entityType
            status = HexEncodeText( entityType, entityTypeLen, tempBuf, &bufLen, &textChanged );
            
            if ( !status )
            {
                tempBuf[bufLen] = '\0';
                strcpy( safeEntityType, tempBuf );
            }
            else
            {
                safeEntityType[0] = '\0';
                DBGLOG( "HexEncodeText failed for entityType, status = %ld\n", status );
            }
            
            // hex-encode zoneName 
            UInt16 zoneNameLen = strlen(zoneName);
            
            bufLen = 256;            
            status = HexEncodeText( zoneName, zoneNameLen, tempBuf, &bufLen, &textChanged );
            
            if ( !status )
            {
                tempBuf[bufLen] = '\0';
                strcpy( safeZoneName, tempBuf );
            }
            else
                DBGLOG( "HexEncodeText failed, status = %ld\n", status );
       
            // hex-encode name 
            UInt16 nameLen = strlen(name);
            
            bufLen = 256;
            status = HexEncodeText( (char*)name, nameLen, tempBuf, &bufLen, &textChanged );
            
            if ( !status )
            {
                tempBuf[bufLen] = '\0';
                strcpy( safeName, tempBuf );
            }
            else
                DBGLOG( "HexEncodeText failed, status = %ld\n", status );
        }
        
        // build URL
        urlLength = strlen( safeEntityType )
                    + strlen( kNBPDivider )
                    + strlen( safeZoneName )
                    + strlen( kEntityZoneDelimiter )
                    + strlen( safeName )
                    + 1;
        
        if ( urlLength <= *returnBufferLen )
        {
            strcpy( returnBuffer, safeEntityType );
            strcat( returnBuffer, kNBPDivider );
            strcat( returnBuffer, safeName );
            strcat( returnBuffer, kEntityZoneDelimiter );
            strcat( returnBuffer, safeZoneName );
            
            *returnBufferLen = urlLength;
        }
        else
            *returnBufferLen = 0;
    }
}

Boolean IsCharURLReservedOrIllegal( const char c )
{
	Boolean 	isIllegal = false;
	
	if ( c <= 0x1F || c == 0x7F || (unsigned char)c >= 0x80 )
		isIllegal = true;
	else
	{
		switch (c)
		{
			case '<':
			case '>':
			case 0x22:	// double quote
			case 0x5c:	// back slash
			case '#':
			case '{':
			case '}':
			case '|':
			case '^':
			case '~':
			case '[':
			case ']':
			case '`':
			case ';':
			case '/':
			case '?':
			case ':':
			case '@':
			case '=':
			case '&':
			case ' ':
			case ',':
			case '%':
				isIllegal = true;
			break;
		}
	}
	
	return isIllegal;
}

void EncodeCharToHex( const char c, char* newHexChar )
{
	// Convert ascii to %xx equivalent
	div_t			result;
	short			hexValue = c;
	char 			c1, c2;
	
	if ( hexValue < 0 )
		hexValue -= 0xFF00;	// clear out the high byte
	
	result = div( hexValue, 16 );
	
	if ( result.quot < 0xA )
		c1 = (char)result.quot + '0';
	else
		c1 = (char)result.quot + 'a' - 10;
	
	if ( result.rem < 0xA )
		c2 = (char)result.rem + '0';
	else
		c2 = (char)result.rem + 'a' - 10;
	
	newHexChar[0] = '%';
	newHexChar[1] = c1;
	newHexChar[2] = c2;
}
/*****************
 * HexEncodeText *
 *****************
 
 This function will change all code in rawText into URL acceptable format by encoding the text using
 the US-ASCII coded character set.  The following are invalid characters: the octets 00-1F, 7F, and 
 80-FF hex.  Also called out are the chars "<", ">", """, "#", "{", "}", "|", "\", "^", "~", "[", "]",
 "`".  The reserved characters for URL syntax are also to be encoded: (so don't pass in a full URL here!)
 ";", "/", "?", ":", "@", "=", "%", and "&".
 
*/

OSStatus HexEncodeText( const char* rawText, UInt16 rawTextLen, char* newTextBuffer, UInt16* newTextBufferLen, Boolean* textChanged )
{
	OSStatus		status = noErr;
	char*			curWritePtr = newTextBuffer;
	UInt16			writeBufferMaxLen;

	if ( !rawText || !newTextBuffer || !newTextBufferLen || !textChanged )
		status = kNSLErrNullPtrError;
	
	writeBufferMaxLen = *newTextBufferLen;
	*textChanged = false;
		
	for ( UInt16 i=0; !status && i<rawTextLen; i++ )
	{
		if ( IsCharURLReservedOrIllegal( rawText[i] ) )
		{
			if ( curWritePtr > newTextBuffer + writeBufferMaxLen + 2 )	// big enough to add two new chars?
			{
				status = kNSLBufferTooSmallForData;
				break;
			}
		
			EncodeCharToHex( rawText[i], curWritePtr );
			curWritePtr += 3;
		
			*textChanged = true;
		}
		else
		{
			if ( curWritePtr > newTextBuffer + writeBufferMaxLen )
			{
				status = kNSLBufferTooSmallForData;
				break;
			}
			
			*curWritePtr = rawText[i];
			curWritePtr++;
		}
	}
	
	if ( !status )
		*newTextBufferLen = (curWritePtr - newTextBuffer);
	else
	{
		*newTextBufferLen = 0;
		*textChanged = false;
	}
	
	return status;
}
