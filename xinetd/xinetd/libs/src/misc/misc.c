/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Portions Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>

/*
 * MISCELLANEOUS FUNCTIONS
 */

#include "misc.h"


/*
 * We always return a pointer in pathname
 */
#if !defined(linux)
char *basename( char *pathname )
{
   char *s = strrchr( pathname, '/' ) ;

   if ( s == NULL )
      return( pathname ) ;
   else
      return( s+1 ) ;
}
#endif


