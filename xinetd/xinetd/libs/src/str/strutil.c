/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <string.h>
#include <ctype.h>

#ifndef NULL
#define NULL				0
#endif

#define TRIVIAL_STR_FIND 1

#ifndef TRIVIAL_STR_FIND

#define LOWER_CASE( c )					( (c) + 'a' - 'A' )

/*
 * look for an instance of sstr in str
 * Returns a pointer to the beginning of sstr in str.
 * It ignores the case of the alphabetic characters
 */
char *str_casefind( register char *str, char *sstr )
{
	register int ssfc = *sstr++ ;		/* sub-string first char */

	if ( ssfc == 0 )
		return( str ) ;

	if ( isalpha( ssfc ) && isupper( ssfc ) )
		ssfc = LOWER_CASE( ssfc ) ;

	while ( *str )
	{
		char *current = str ;
		register int strc = *str++ ;
		char *sp ;							/* string pointer */
		char *ssp ;							/* sub-string pointer */

		if ( isalpha( strc ) && isupper( strc ) )
			strc = LOWER_CASE( strc ) ;
		if ( strc != ssfc )
			continue ;
		
		for ( sp = str, ssp = sstr ;; sp++, ssp++ )
		{
			register int sc = *sp ;				/* string char */
			register int ssc = *ssp ;			/* substring char */

			/*
			 * End-of-substring means we got a match
			 */
			if ( ssc == 0 )
				return( current ) ;

			/*
			 * Convert to lower case if alphanumeric
			 */
			if ( isalpha( sc ) && isupper( sc ) )
				sc = LOWER_CASE( sc ) ;
			if ( isalpha( ssc ) && isupper( ssc ) )
				ssc = LOWER_CASE( ssc ) ;
			if ( sc != ssc )
				break ;
		}
	}

	return( 0 ) ;
}


#else		/* defined( TRIVIAL_STR_FIND ) */

/*
 * look for an instance of s2 in s1
 * Returns a pointer to the beginning of s2 in s1.
 * It ignores the case of the alphabetic characters
 */
char *str_casefind( char *s1, const char *s2 )
{
   unsigned int i ;
   unsigned long l1 = strlen( s1 ) ;
   unsigned long l2 = strlen( s2 ) ;
   
   if ( l2 > l1 )
      return( NULL ) ;

   for ( i = 0 ; i < l1 - l2 + 1 ; i++ )
      if ( strncasecmp( &s1[ i ], s2, l2 ) == 0 )
         return( (char *) &s1[ i ] ) ;
   return( NULL ) ;
}

#endif 	/* TRIVIAL_STR_FIND */


/*
 * Fill string s with character c
 */
void str_fill( char *s, char c )
{
	while ( *s ) *s++ = c ;
}

