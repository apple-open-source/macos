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
 *  add.c
 */

#ifdef NOTDEF
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include <stdio.h>
#include <string.h>

#ifdef MACOS
#include "macos.h"
#endif /* MACOS */

#if defined( DOS ) || defined( _WIN32 )
#include <malloc.h>
#include "msdos.h"
#endif /* DOS */

#if !defined( MACOS ) && !defined( DOS )
#include <sys/types.h>
#include <sys/socket.h>
#endif /* !MACOS && !DOS */

#include "lber.h"
#include "ldap_ldap-int.h"
#include "ldap.h"

/*
 * ldap_add - initiate an ldap (and X.500) add operation.  Parameters:
 *
 *	ld		LDAP descriptor
 *	dn		DN of the entry to add
 *	mods		List of attributes for the entry.  This is a null-
 *			terminated array of pointers to LDAPMod structures.
 *			only the type and values in the structures need be
 *			filled in.
 *
 * Example:
 *	LDAPMod	*attrs[] = { 
 *			{ 0, "cn", { "babs jensen", "babs", 0 } },
 *			{ 0, "sn", { "jensen", 0 } },
 *			{ 0, "objectClass", { "person", 0 } },
 *			0
 *		}
 *	msgid = ldap_add( ld, dn, attrs );
 */
int
ldap_add( LDAP *ld, char *dn, LDAPMod **attrs )
{
	BerElement	*ber;
	int		i, rc;

	/*
	 * An add request looks like this:
	 *	AddRequest ::= SEQUENCE {
	 *		entry	DistinguishedName,
	 *		attrs	SEQUENCE OF SEQUENCE {
	 *			type	AttributeType,
	 *			values	SET OF AttributeValue
	 *		}
	 *	}
	 */

	Debug( LDAP_DEBUG_TRACE, "ldap_add\n", 0, 0, 0 );

	/* create a message to send */
	if ( (ber = alloc_ber_with_options( ld )) == NULLBER ) {
		return( -1 );
	}

	if ( ber_printf( ber, "{it{s{", ++ld->ld_msgid, LDAP_REQ_ADD, dn )
	    == -1 ) {
		ld->ld_errno = LDAP_ENCODING_ERROR;
		ber_free( ber, 1 );
		return( -1 );
	}

	/* for each attribute in the entry... */
	for ( i = 0; attrs[i] != NULL; i++ ) {
		if ( ( attrs[i]->mod_op & LDAP_MOD_BVALUES) != 0 ) {
			rc = ber_printf( ber, "{s[V]}", attrs[i]->mod_type,
			    attrs[i]->mod_values );
		} else {
			rc = ber_printf( ber, "{s[v]}", attrs[i]->mod_type,
			    attrs[i]->mod_values );
		}
		if ( rc == -1 ) {
			ld->ld_errno = LDAP_ENCODING_ERROR;
			ber_free( ber, 1 );
			return( -1 );
		}
	}

	if ( ber_printf( ber, "}}}" ) == -1 ) {
		ld->ld_errno = LDAP_ENCODING_ERROR;
		ber_free( ber, 1 );
		return( -1 );
	}

	/* send the message */
	return( send_initial_request( ld, LDAP_REQ_ADD, dn, ber ));
}

int
ldap_add_s( LDAP *ld, char *dn, LDAPMod **attrs )
{
	int		msgid;
	LDAPMessage	*res;

	if ( (msgid = ldap_add( ld, dn, attrs )) == -1 )
		return( ld->ld_errno );

	if ( ldap_result( ld, msgid, 1, (struct timeval *) NULL, &res ) == -1 )
		return( ld->ld_errno );

	return( ldap_result2error( ld, res, 1 ) );
}

