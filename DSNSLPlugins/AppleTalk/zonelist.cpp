#include <NSLSemaphore.h>
#include "zonelist.h"

static NSLSemaphore *sNSLSemaphore = NULL;

int ZIPGetZoneList(int lookupType, char *buffer, int bufferSize, long *actualCount)
{
//	static u_char	zonebuf[ATP_DATA_SIZE+1];
    static u_char	zonebuf[sizeof(Str32)];
    u_char			*start, *end, save;
	int             i, j, n;
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
                n = zip_getzonelist( ZIP_DEF_INTERFACE, &i, zonebuf, sizeof(Str32) );
                break;
            
            case LOOKUP_LOCAL:
                n = zip_getlocalzones( ZIP_DEF_INTERFACE, &i, zonebuf, sizeof(Str32) );
                break;
                
            case LOOKUP_CURRENT:
                // these calls require an interface name.
                // do not use ZIP_DEF_INTERFACE 
//                n = zip_getmyzone( "en0", &aZone );
                n = zip_getmyzone( ZIP_DEF_INTERFACE, &aZone );
                //n = at_getdefaultzone( "en0", &aZone );
                DBGLOG( "zip_getmyzone = %d\n",n );
                break;
        }
        
        sNSLSemaphore->Signal();
        
        if (n == -1)
        {
            DBGLOG( "zip_getzonelist failed\n" );
            return (-1);				// got an error, so punt
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
        
        if ((entryCount + n) > maxCount)		// dont overrun their buffer
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

#if 0
    currStr = (Str32 *) buffer;
    for (j = 0; j < entryCount; j++)
    {
        fprintf (stderr, "Entry %d, <%s>\n", j, (char*) currStr);
        currStr++;
    }
#endif

    qsort(buffer, entryCount, sizeof (Str32), my_strcmp);


#if 0
    currStr = buffer;
    for (j = 0; j < entryCount; j++)
    {
        fprintf (stderr, "Entry %d, <%s>\n", j, (char*) currStr);
        currStr++;
    }
#endif
    *actualCount = entryCount;

    return(result);
}


