/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#include "config.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "str.h"
#include "strparse.h"
#include "sio.h"

static int str_errno ;

#define HANDLE_ERROR( flags, retval, errp, errval, msg )    \
	if ( flags & STR_RETURN_ERROR )                 \
	{                                               \
               *errp = errval ;                         \
               return( retval ) ;                       \
        }                                               \
        else                                            \
               terminate( msg );


char *new_string( const char *s )
{
   if ( s )
      return strdup( s ) ;
   else
      return 0;
}

str_h str_parse( register char *str, const char *separ, int flags, int *errnop )
{
	register struct str_handle *hp ;
	int *errp = ( errnop == NULL ) ? &str_errno : errnop ;

	if ( separ == NULL ) {
		HANDLE_ERROR( flags, NULL, errp, STR_ENULLSEPAR,
			"STR str_parse: NULL separator" ) ;
	}

	hp = (struct str_handle *) malloc( sizeof( struct str_handle ) ) ;
	if ( hp == NULL ) {
		HANDLE_ERROR( flags, NULL, errp, STR_ENOMEM,
			"STR str_parse: malloc failed" ) ;
	}

	hp->string = str ;
	hp->pos = str ;
	hp->separator = new_string( separ ) ;
	if ( hp->separator == NULL ) {
		if ( flags & STR_RETURN_ERROR )
		{
			free( (char *) hp ) ;
			*errp = STR_ENOMEM ;
			return( NULL ) ;
		} else {
			terminate( "STR str_parse: malloc failed" ) ;
		}
	}
	
	hp->flags = flags ;
	hp->errnop = errp ;
	hp->no_more = ( str == NULL ) ;
	return( (str_h) hp ) ;
}


void str_endparse( str_h handle )
{
	register struct str_handle *hp = (struct str_handle *) handle ;

	if( hp->separator != NULL )
		free( hp->separator ) ;
	free( (char *) handle ) ;
}


/*
 * Change the string
 */
int str_setstr( str_h handle, char *newstr )
{
        register struct str_handle *hp = (struct str_handle *) handle ;
		
        if ( newstr == NULL ) {
                HANDLE_ERROR( hp->flags, STR_ERR, hp->errnop, STR_ENULLSTRING,
                        "STR str_setstr: NULL string" ) ;
        }
			        
        hp->string = newstr ;
        hp->pos = newstr ;
        hp->no_more = FALSE ;
        return( STR_OK ) ;
}


char *str_component( str_h handle )
{
	register char			*start ;
	register char			*last ;
	register unsigned int		sep_count ;
	char				*retval ;
	int				last_char ;
	register struct str_handle	*hp	= (struct str_handle *) handle ;
	register int			first_call = ( hp->pos == hp->string ) ;

	if ( hp->no_more )
		return( NULL ) ;

	/*
	 * Get number of separator characters.
	 * Find beginning of component.
	 */
	sep_count = strspn( hp->pos, hp->separator ) ;

	/*
	 * If this is the first call, and there are separator characters
	 * at the beginning of the string and the STR_NULL_START flag is set
	 * we return a 0-length string.
	 */
	if ( first_call && ( sep_count > 0 ) && ( hp->flags & STR_NULL_START ))
	{
		start = hp->pos ;
		last = hp->pos ;
	}
	else
	{
		start = hp->pos + sep_count ;

		if ( *start == '\0' )
		{
			last = start ;
			hp->no_more = TRUE ;
			if ( ! ( hp->flags & STR_NULL_END ) )
				return( NULL ) ;
		}
		else
		{
			last = strpbrk( start, hp->separator ) ;
			if ( last == NULL )
				last = start + strlen( start ) ;
		}
	}

	/*
	 * At this point, the following variables must be set:
	 *		start:	beginning of component
	 *		last:   end of component + 1
	 *
	 * If STR_MALLOC is set, allocate space for the new string.
	 *
	 * NOTE: If STR_MALLOC is not set, the processed string is trashed.
	 */
	last_char = *last ;
	if ( hp->flags & STR_MALLOC )
	{
		int len = last - start ;

		retval = malloc( (unsigned)len + 1 ) ;
		if ( retval == NULL ) {
			HANDLE_ERROR( hp->flags, NULL, hp->errnop, STR_ENOMEM,
				"STR str_component: malloc failed" ) ;
		}
		strncpy( retval, start, len )[ len ] = '\0' ;
	}
	else
	{
		retval = start ;
		*last = '\0' ;
	}

	/*
	 * Check if last_char is NUL to avoid setting hp->pos past the
	 * end of the string
	 */
	hp->pos = ( last_char == '\0' ) ? last : last+1 ;
	return( retval ) ;
}

