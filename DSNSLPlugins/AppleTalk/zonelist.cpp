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
 *  @header zonelist
 */
 
#include <NSLSemaphore.h>
#include "zonelist.h"

static NSLSemaphore *sNSLSemaphore = NULL;

int ZIPGetZoneList(int lookupType, char *buffer, int bufferSize, long *actualCount)
{
    static u_char	zonebuf[ATP_DATA_SIZE+1];	// for zip_getzonelist call, must be at least ATP_DATA_SIZE+1 bytes in length
    u_char			*start, *end, save;
	int             i, j, n=-1;
    UInt16			len;
	long		entryCount = 0;
	Str32		*currStr = (Str32 *) buffer;
	long		maxCount;
	int		result = 0;
    at_nvestr_t aZone;
    
    if ( actualCount == NULL )
        return -2;
    
    if ( sNSLSemaphore == NULL ) {
        sNSLSemaphore = new NSLSemaphore(1);
        if ( sNSLSemaphore == NULL ) {
            fprintf(stderr, "sNSLSemaphore is NULL\n");
            return -1;
        }
    }
    
    maxCount = bufferSize / sizeof(Str32);		// max number of entries that will fit in the buffer
    *actualCount = 0;
    
    i = ZIP_FIRST_ZONE;
    do
    {
        sNSLSemaphore->Wait();
        
        switch( lookupType )
        {
            case LOOKUP_ALL:
                n = zip_getzonelist( ZIP_DEF_INTERFACE, &i, zonebuf, sizeof(zonebuf) );
                break;
            
            case LOOKUP_LOCAL:
                n = zip_getlocalzones( ZIP_DEF_INTERFACE, &i, zonebuf, sizeof(zonebuf) );
                break;
                
            case LOOKUP_CURRENT:
                // these calls require an interface name.
                n = zip_getmyzone( ZIP_DEF_INTERFACE, &aZone );
                DBGLOG( "zip_getmyzone = %d\n",n );
                break;
        }
        
        sNSLSemaphore->Signal();
        
        if (n == -1)
        {
            DBGLOG( "zip_getzonelist failed\n" );
            return (-1);				// got an error
        }
        
        // handle the zip_getmyzone case
        if ( lookupType == LOOKUP_CURRENT )
        {
            *actualCount = 1;
            strncpy( buffer, (char *)aZone.str, aZone.len );
            buffer[aZone.len] = '\0';
            DBGLOG( "myzone=%s\n", buffer );
            return 0;
        }
        
        if ((entryCount + n) > maxCount)		// don't overrun their buffer
        {
            n = maxCount - entryCount;		// fit in as many entries as we can
            if (n <= 0) {				// if no more will fit, then skip out to do the sort
                result = 1;			// use positive error code to indicate maxCount has been exceeded
                break;
            }
        }
        
        start = zonebuf;
        for (j = 0; j < n; j++)
        {
            len = *start;
            end = start + 1 + len;
            save = *end;	
            *end = 0x00;
            if (sprintf((char*) currStr,"%s", (char*)start+1) <= 0 )
            {
                return (-1);
            }
             
            *end = save;
            currStr ++;
            start = end;
        }
        entryCount += n;

    } while (n != -1 && i != ZIP_NO_MORE_ZONES);

//    qsort(buffer, entryCount, sizeof (Str32), my_strcmp);

    *actualCount = entryCount;

    return(result);
}
