/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "str.h"
#include "libportable.h"

/*
 * Search the given buffer for an occurrence of "%m"
 */
int __xlog_add_errno( const char *buf, int len )
{
	int i;

	for ( i = 0 ; i < len-1 ; i++ )
		if ( (buf[i] == '%') && (buf[i+1] == 'm') )
			return( i ) ;
	return( -1 ) ;
}

/* __xlog_explain_errno:
 * Description: generates a string to explain the value of errno.
 * On entry: buf must point to space already allocated by the caller,
 * and size points to the amount of memory buf is allocated.
 * On exit: buf contains the NULL terminated string, size now points
 * to the length of the string, and the return value is buf.
 */
char *__xlog_explain_errno( char *buf, unsigned *size )
{
   strx_print((int *)size, buf, *size, "%s (errno = %d)", 
      strerror(errno), errno);
   return( buf ) ;
}


char *__xlog_new_string( const char *s )
{
   if ( s )
      return strdup( s ) ;
   else
      return 0;
}


