/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 *  Copyright (c) 1990 Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  friendly.c
 */

#ifdef NOTDEF
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1993 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#ifdef MACOS
#include <stdlib.h>
#include "macos.h"
#endif /* MACOS */

#if defined( DOS ) || defined( _WIN32 )
#include <malloc.h>
#include "msdos.h"
#endif /* DOS */

#if !defined( MACOS ) && !defined( DOS )
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif
#include <NetInfo/config.h>
#ifdef _UNIX_BSD_43_
extern char *strdup(char *);
#endif

#include "lber.h"
#include "ldap_ldap-int.h"
#include "ldap.h"

char *
ldap_friendly_name( char *filename, char *uname, FriendlyMap **map )
{
	int	i, entries;
	FILE	*fp;
	char	*s;
	char	buf[BUFSIZ];

	if ( map == NULL ) {
#if !defined( MACOS ) && !defined( DOS )
		errno = EINVAL;
#endif
		return( uname );
	}

	if ( *map == NULL ) {
		if ( (fp = fopen( filename, "r" )) == NULL )
			return( uname );

		entries = 0;
		while ( fgets( buf, sizeof(buf), fp ) != NULL ) {
			if ( buf[0] != '#' )
				entries++;
		}
		rewind( fp );

		if ( (*map = (FriendlyMap *) malloc( (entries + 1) *
		    sizeof(FriendlyMap) )) == NULL ) {
			fclose( fp );
			return( uname );
		}

		i = 0;
		while ( fgets( buf, sizeof(buf), fp ) != NULL && i < entries ) {
			if ( buf[0] == '#' )
				continue;

			if ( (s = strchr( buf, '\n' )) != NULL )
				*s = '\0';

			if ( (s = strchr( buf, '\t' )) == NULL )
				continue;
			*s++ = '\0';

			if ( *s == '"' ) {
				int	esc = 0, found = 0;

				for ( ++s; *s && !found; s++ ) {
					switch ( *s ) {
					case '\\':
						esc = 1;
						break;
					case '"':
						if ( !esc )
							found = 1;
						/* FALL */
					default:
						esc = 0;
						break;
					}
				}
			}

			(*map)[i].f_unfriendly = strdup( buf );
			(*map)[i].f_friendly = strdup( s );
			i++;
		}

		fclose( fp );
		(*map)[i].f_unfriendly = NULL;
	}

	for ( i = 0; (*map)[i].f_unfriendly != NULL; i++ ) {
		if ( strcasecmp( uname, (*map)[i].f_unfriendly ) == 0 )
			return( (*map)[i].f_friendly );
	}
	return( uname );
}


void
ldap_free_friendlymap( FriendlyMap **map )
{
	struct friendly* pF = *map;

	if ( pF == NULL )
		return;

	while ( pF->f_unfriendly )
	{
		free( pF->f_unfriendly );
		free( pF->f_friendly );
		pF++;
	}
	free( *map );
	*map = NULL;
}
