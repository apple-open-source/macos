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
 *  delete.c
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
#include "msdos.h"
#endif /* DOS */

#if !defined( MACOS ) && !defined( DOS )
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include "lber.h"
#include "ldap_ldap-int.h"
#include "ldap.h"

/*
 * ldap_delete - initiate an ldap (and X.500) delete operation. Parameters:
 *
 *	ld		LDAP descriptor
 *	dn		DN of the object to delete
 *
 * Example:
 *	msgid = ldap_delete( ld, dn );
 */
int
ldap_delete( LDAP *ld, char *dn )
{
	BerElement	*ber;

	/*
	 * A delete request looks like this:
	 *	DelRequet ::= DistinguishedName,
	 */

	Debug( LDAP_DEBUG_TRACE, "ldap_delete\n", 0, 0, 0 );

	/* create a message to send */
	if ( (ber = alloc_ber_with_options( ld )) == NULLBER ) {
		return( -1 );
	}

	if ( ber_printf( ber, "{its}", ++ld->ld_msgid, LDAP_REQ_DELETE, dn )
	    == -1 ) {
		ld->ld_errno = LDAP_ENCODING_ERROR;
		ber_free( ber, 1 );
		return( -1 );
	}

	/* send the message */
	return ( send_initial_request( ld, LDAP_REQ_DELETE, dn, ber ));
}


int
ldap_delete_s( LDAP *ld, char *dn )
{
	int		msgid;
	LDAPMessage	*res;

	if ( (msgid = ldap_delete( ld, dn )) == -1 )
		return( ld->ld_errno );

	if ( ldap_result( ld, msgid, 1, (struct timeval *) NULL, &res ) == -1 )
		return( ld->ld_errno );

	return( ldap_result2error( ld, res, 1 ) );
}
