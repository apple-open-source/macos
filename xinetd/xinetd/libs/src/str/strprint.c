/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


#include "config.h"
#ifndef NO_SIO
#include "sio.h"
#endif

#include "str.h"
#include "strparse.h"

#define INT_NULL			((int *)0)

/*
 * The strx_* functions will never over-run the buffer
 * The str_* functions may over-run the buffer
 */

/*
 * Group 1: the strx_* functions
 */

/*
 * This is the general purpose conversion function. It is invoked
 * by all the other str[x]_* functions
 */
void strx_printv( int *ccp, char *buf, int len, const char *format, va_list ap )
{
#ifndef NO_SIO
	__sio_od_t od ;
	int cc ;

   /*
    * First initialize the descriptor
	 * Notice that if no length is given, we initialize buf_end to the
	 * highest possible address.
    */
   od.buf = buf ;                						/* NOT NEEDED        */
   od.buf_end = len ? &buf[ len ] : (char *) ~0 ;	/* NEEDED				*/
   od.buffer_size = 0 ;          						/* NOT NEEDED        */
   od.start = buf ;              						/* NOT NEEDED        */
   od.nextb = buf ;              						/* NEEDED            */
   od.buftype = 0 ;              						/* NOT NEEDED        */

   /*
    * Do the conversion
    */
   cc = __sio_converter( &od, -1, format, ap ) ;
	if ( len == 0 || od.nextb < od.buf_end )
		*(od.nextb) = '\0' ;
   if ( ccp )
      *ccp = cc ;
#endif	/* ! NO_SIO */
}


void strx_print( int *ccp, char *buf, int len, const char *format, ... )
{
	va_list ap ;

	if (len <= 0) {
		if( ccp )
			*ccp = 0;
		return;
	}

	va_start( ap, format ) ;
	strx_printv( ccp, buf, len, format, ap ) ;
	va_end( ap ) ;
}


char *strx_sprint( char *buf, int len, const char *format, ... )
{
	va_list ap ;

	if (len <= 0) {
		return buf;
	}
	va_start( ap, format ) ;
	strx_printv( INT_NULL, buf, len, format, ap ) ;
	va_end( ap ) ;
	return( buf ) ;
}


int strx_nprint( char *buf, int len, const char *format, ...)
{
	int cc ;
	va_list ap ;

	if (len <= 0) {
		return 0;
	}
	va_start( ap, format ) ;
	strx_printv( &cc, buf, len, format, ap ) ;
	va_end( ap ) ;
	return( cc ) ;
}


int strx_nprintv( char *buf, int len, const char *format, va_list ap )
{
	int cc ;

	if (len <= 0) {
		return 0;
	}
	strx_printv( &cc, buf, len, format, ap ) ;
	return( cc ) ;
}


