/*
	File:		GenericNBPURL.cp

	Contains:	xxx put contents here xxx

	Written by:	Kevin Arnold

	Copyright:	© 1998 - 1999 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

	<14>	05/28/99	KA		convert NewPtr to MMNew and DisposePtr to MMFree
	<13>	05/05/99	KA		our return length was off by two!
	<12>	04/29/99	KA		use new encode/decode calls
	<11>	04/23/99	KA		fixed a bug in ParsingGenericNBPURL
	<10>	04/11/99	KA		added MakeGenericNBPURL
	<09>	04-07-99	sns		MemMgr.h -> NSLMemMgr.h
	<08>	03-30-99	KA		incorporated changes for Steve
	<07>	03/26/99	sns		explicit namespace (for carbon d9)
	<06>	02/25/99	KA		call DecodeHTTPString when parsing out a url
	<05>	01/26/98	KA		added checks for valid pointers in NewGenericNBPURL
	<04>	11/09/98	KA		added IsValidATZone
	<03>	11/06/98	KA		added some missing #includes
	<02>	11/06/98	KA		moved EncodeHTTPString and DecodeHTTPString to URLUtilities.cp
	<01>	11/06/98	KA		added EncodeHTTPString and DecodeHTTPString and converted to .cp file
	<4>		10/15/98	KA		took out conversion of AFPServer to afp
	<3>		7/27/98	 	KA		convert AFPServer to afp in url string
	<2>		6/28/98	 	KA		Added new memmgr calls
	 <1>	 3/25/98	KA		Initial checkin

	To Do:
*/
#include <stdio.h>
#include <string.h>
#include <CoreServices/CoreServices.h>
#include "GenericNBPURL.h"
#include "NSLDebugLog.h"

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
            status = NSLHexEncodeText( entityType, entityTypeLen, tempBuf, &bufLen, &textChanged );
            
            if ( !status )
            {
                tempBuf[bufLen] = '\0';
                strcpy( safeEntityType, tempBuf );
            }
            else
            {
                safeEntityType[0] = '\0';
                DBGLOG( "NSLHexEncodeText failed for entityType, status = %ld\n", status );
            }
            
            // hex-encode zoneName 
            UInt16 zoneNameLen = strlen(zoneName);
            
            bufLen = 256;            
            status = NSLHexEncodeText( zoneName, zoneNameLen, tempBuf, &bufLen, &textChanged );
            
            if ( !status )
            {
                tempBuf[bufLen] = '\0';
                strcpy( safeZoneName, tempBuf );
            }
            else
                DBGLOG( "NSLHexEncodeText failed, status = %ld\n", status );
       
            // hex-encode name 
            UInt16 nameLen = strlen(name);
            
            bufLen = 256;
            status = NSLHexEncodeText( (char*)name, nameLen, tempBuf, &bufLen, &textChanged );
            
            if ( !status )
            {
                tempBuf[bufLen] = '\0';
                strcpy( safeName, tempBuf );
            }
            else
                DBGLOG( "NSLHexEncodeText failed, status = %ld\n", status );
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

#if 0

OSStatus ParseGenericNBPURL(char* url, StringPtr entityType, StringPtr zoneName, StringPtr name)
{
	char*		curPtr1;
	char*		curPtr2;
	OSStatus	status = kNBPURLBadSyntaxErr;
	
	if ( IsGenericNBPURL( url ) )
	{
		string	convertedURL = url;
		DecodeHTTPString( convertedURL );
				
		curPtr1 = (Ptr)convertedURL.c_str();
		curPtr2 = strstr( convertedURL.c_str(), kNBPDivider );
		
		if ( curPtr2 )
		{
            ::BlockMove( convertedURL.c_str(), &entityType[1], curPtr2-curPtr1 );
			entityType[0] = curPtr2-curPtr1;
			
			curPtr1 = curPtr2+strlen(kNBPDivider);	// advance past ":/at/"
			curPtr2 = strstr( curPtr1, kEntityZoneDelimiter );		// find ':' delimiter
			
			if ( curPtr2 )
			{
				::BlockMove( curPtr1, &name[1], curPtr2-curPtr1 );
				name[0] = curPtr2-curPtr1;
				
				curPtr1 = curPtr2 + strlen(kEntityZoneDelimiter);	// advance past ":"
				
				strcpy( (char*)zoneName, curPtr1 );		// copy rest into name
				c2pstr( (char*)zoneName );				// make a pstring
				
				if ( zoneName[zoneName[0]] == '/' )
					zoneName[0]--;							// now discount the trailing '/'
				
				status = noErr;
			}
		}
	}
	
	return status;
}

#endif


