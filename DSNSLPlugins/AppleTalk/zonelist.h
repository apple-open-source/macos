#ifndef __ZONELIST__
#define __ZONELIST__

#include <pthread.h>
#include "nbputilities.h"

/* defines */
#define	kIOSize	64

#define ZIP_FIRST_ZONE 1
#define ZIP_NO_MORE_ZONES 0
#define ZIP_DEF_INTERFACE NULL

#define LOOKUP_ALL	0		/* perform lookup on all zones */
#define LOOKUP_LOCAL	1		/* perform lookup on local zones */
#define LOOKUP_CURRENT	2		/* perform lookup on current zone */


int ZIPGetZoneList(int lookupType, char *buffer, int bufferSize, long *actualCount);

extern "C" {
/* zip_getzonelist() will return the zone count on success, 
   and -1 on failure. */

int zip_getzonelist(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	int *context,
		/* *context should be set to ZIP_FIRST_ZONE for the first call.
		   The returned value may be used in the next call, unless it
		   is equal to ZIP_NO_MORE_ZONES.
		*/
	u_char *zones,
		/* Pointer to the beginning of the "zones" buffer.
		   Zone data returned will be a sequence of at_nvestr_t
		   Pascal-style strings, as it comes back from the 
		   ZIP_GETZONELIST request sent over ATP 
		*/
	int size
		/* Length of the "zones" buffer; must be at least 
		   (ATP_DATA_SIZE+1) bytes in length.
		*/
);

/* zip_getlocalzones() will return the zone count on success, 
   and -1 on failure. */

int zip_getlocalzones(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	int *context,
		/* *context should be set to ZIP_FIRST_ZONE for the first call.
		   The returned value may be used in the next call, unless it
		   is equal to ZIP_NO_MORE_ZONES.
		*/
	u_char *zones,
		/* Pointer to the beginning of the "zones" buffer.
		   Zone data returned will be a sequence of at_nvestr_t
		   Pascal-style strings, as it comes back from the 
		   ZIP_GETLOCALZONES request sent over ATP 
		*/
	int size
		/* Length of the "zones" buffer; must be at least 
		   (ATP_DATA_SIZE+1) bytes in length.
		*/
);

int zip_getmyzone(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	at_nvestr_t *zone
);

int at_getdefaultzone(
     char *ifName,
          /* ifName must not be a null pointer; a pointer to a valid 
	     interface name must be supplied. */
     at_nvestr_t *zone
          /* The return value for the default zone, from persistent 
             storage */
);

};


#endif


