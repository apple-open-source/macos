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
 *  Copyright (c) 1993 Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  sbind.c
 */

#ifdef NOTDEF
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1993 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include <stdio.h>
#include <string.h>

#ifdef MACOS
#include "macos.h"
#endif /* MACOS */

#if !defined( MACOS ) && !defined( DOS )
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include "lber.h"
#include "ldap_ldap-int.h"
#include "ldap.h"


/*
 * ldap_simple_bind - bind to the ldap server (and X.500).  The dn and
 * password of the entry to which to bind are supplied.  The message id
 * of the request initiated is returned.
 *
 * Example:
 *	ldap_simple_bind( ld, "cn=manager, o=university of michigan, c=us",
 *	    "secret" )
 */

int
ldap_simple_bind( LDAP *ld, char *dn, char *passwd )
{
	BerElement	*ber;

	/*
	 * The bind request looks like this:
	 *	BindRequest ::= SEQUENCE {
	 *		version		INTEGER,
	 *		name		DistinguishedName,	 -- who
	 *		authentication	CHOICE {
	 *			simple		[0] OCTET STRING -- passwd
	 *		}
	 *	}
	 * all wrapped up in an LDAPMessage sequence.
	 */

	Debug( LDAP_DEBUG_TRACE, "ldap_simple_bind\n", 0, 0, 0 );

	if ( dn == NULL )
		dn = "";
	if ( passwd == NULL )
		passwd = "";

	/* create a message to send */
	if ( (ber = alloc_ber_with_options( ld )) == NULLBER ) {
		return( -1 );
	}

	/* fill it in */
	if ( ber_printf( ber, "{it{ists}}", ++ld->ld_msgid, LDAP_REQ_BIND,
	    ld->ld_version, dn, LDAP_AUTH_SIMPLE, passwd ) == -1 ) {
		ld->ld_errno = LDAP_ENCODING_ERROR;
		ber_free( ber, 1 );
		return( -1 );
	}

#ifndef NO_CACHE
	if ( ld->ld_cache != NULL ) {
		ldap_flush_cache( ld );
	}
#endif /* !NO_CACHE */

	/* send the message */
	return( send_initial_request( ld, LDAP_REQ_BIND, dn, ber ));
}

/*
 * ldap_simple_bind - bind to the ldap server (and X.500) using simple
 * authentication.  The dn and password of the entry to which to bind are
 * supplied.  LDAP_SUCCESS is returned upon success, the ldap error code
 * otherwise.
 *
 * Example:
 *	ldap_simple_bind_s( ld, "cn=manager, o=university of michigan, c=us",
 *	    "secret" )
 */

int
ldap_simple_bind_s( LDAP *ld, char *dn, char *passwd )
{
	int		msgid;
	LDAPMessage	*result;

	Debug( LDAP_DEBUG_TRACE, "ldap_simple_bind_s\n", 0, 0, 0 );

	if ( (msgid = ldap_simple_bind( ld, dn, passwd )) == -1 )
		return( ld->ld_errno );

	if ( ldap_result( ld, msgid, 1, (struct timeval *) 0, &result ) == -1 )
		return( ld->ld_errno );	/* ldap_result sets ld_errno */

	return( ldap_result2error( ld, result, 1 ) );
}
