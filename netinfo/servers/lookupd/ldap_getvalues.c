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
 *  getvalues.c
 */

#ifdef NOTDEF
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#ifdef MACOS
#include <stdlib.h>
#include "macos.h"
#else /* MACOS */
#if defined( DOS ) || defined( _WIN32 )
#include <malloc.h>
#include "msdos.h"
#else /* DOS */
#include <sys/types.h>
#include <sys/socket.h>
#endif /* DOS */
#endif /* MACOS */

#include "lber.h"
#include "ldap_ldap-int.h"
#include "ldap.h"

char **
ldap_get_values( LDAP *ld, LDAPMessage *entry, char *target )
{
	BerElement	ber;
	char		attr[LDAP_MAX_ATTR_LEN];
	int		found = 0;
	long		len;
	char		**vals;

	Debug( LDAP_DEBUG_TRACE, "ldap_get_values\n", 0, 0, 0 );

	ber = *entry->lm_ber;

	/* skip sequence, dn, sequence of, and snag the first attr */
	len = sizeof(attr);
	if ( ber_scanf( &ber, "{x{{s", attr, &len ) == LBER_ERROR ) {
		ld->ld_errno = LDAP_DECODING_ERROR;
		return( NULL );
	}

	if ( strcasecmp( target, attr ) == 0 )
		found = 1;

	/* break out on success, return out on error */
	while ( ! found ) {
		len = sizeof(attr);
		if ( ber_scanf( &ber, "x}{s", attr, &len ) == LBER_ERROR ) {
			ld->ld_errno = LDAP_DECODING_ERROR;
			return( NULL );
		}

		if ( strcasecmp( target, attr ) == 0 )
			break;
	}

	/* 
	 * if we get this far, we've found the attribute and are sitting
	 * just before the set of values.
	 */

	if ( ber_scanf( &ber, "[v]", &vals ) == LBER_ERROR ) {
		ld->ld_errno = LDAP_DECODING_ERROR;
		return( NULL );
	}

	return( vals );
}

struct berval **
ldap_get_values_len( LDAP *ld, LDAPMessage *entry, char *target )
{
	BerElement	ber;
	char		attr[LDAP_MAX_ATTR_LEN];
	int		found = 0;
	long		len;
	struct berval	**vals;

	Debug( LDAP_DEBUG_TRACE, "ldap_get_values_len\n", 0, 0, 0 );

	ber = *entry->lm_ber;

	/* skip sequence, dn, sequence of, and snag the first attr */
	len = sizeof(attr);
	if ( ber_scanf( &ber, "{x{{s", attr, &len ) == LBER_ERROR ) {
		ld->ld_errno = LDAP_DECODING_ERROR;
		return( NULL );
	}

	if ( strcasecmp( target, attr ) == 0 )
		found = 1;

	/* break out on success, return out on error */
	while ( ! found ) {
		len = sizeof(attr);
		if ( ber_scanf( &ber, "x}{s", attr, &len ) == LBER_ERROR ) {
			ld->ld_errno = LDAP_DECODING_ERROR;
			return( NULL );
		}

		if ( strcasecmp( target, attr ) == 0 )
			break;
	}

	/* 
	 * if we get this far, we've found the attribute and are sitting
	 * just before the set of values.
	 */

	if ( ber_scanf( &ber, "[V]", &vals ) == LBER_ERROR ) {
		ld->ld_errno = LDAP_DECODING_ERROR;
		return( NULL );
	}

	return( vals );
}

int
ldap_count_values( char **vals )
{
	int	i;

	if ( vals == NULL )
		return( 0 );

	for ( i = 0; vals[i] != NULL; i++ )
		;	/* NULL */

	return( i );
}

int
ldap_count_values_len( struct berval **vals )
{
	return( ldap_count_values( (char **) vals ) );
}

void
ldap_value_free( char **vals )
{
	int	i;

	if ( vals == NULL )
		return;
	for ( i = 0; vals[i] != NULL; i++ )
		free( vals[i] );
	free( (char *) vals );
}

void
ldap_value_free_len( struct berval **vals )
{
	int	i;

	if ( vals == NULL )
		return;
	for ( i = 0; vals[i] != NULL; i++ ) {
		free( vals[i]->bv_val );
		free( vals[i] );
	}
	free( (char *) vals );
}
