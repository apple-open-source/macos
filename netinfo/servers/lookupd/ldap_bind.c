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
 *  bind.c
 */

#ifdef NOTDEF
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include <stdio.h>
#include <string.h>
#ifdef MACOS
#include <stdlib.h>
#include "macos.h"
#else /* MACOS */
#ifdef DOS
#include "msdos.h"
#ifdef NCSA
#include "externs.h"
#endif /* NCSA */
#else /* DOS */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#endif /* DOS */
#endif /* MACOS */

#include "lber.h"
#include "ldap_ldap-int.h" 
#include "ldap.h"


/*
 * ldap_bind - bind to the ldap server (and X.500).  The dn and password
 * of the entry to which to bind are supplied, along with the authentication
 * method to use.  The msgid of the bind request is returned on success,
 * -1 if there's trouble.  Note, the kerberos support assumes the user already
 * has a valid tgt for now.  ldap_result() should be called to find out the
 * outcome of the bind request.
 *
 * Example:
 *	ldap_bind( ld, "cn=manager, o=university of michigan, c=us", "secret",
 *	    LDAP_AUTH_SIMPLE )
 */

int
ldap_bind( LDAP *ld, char *dn, char *passwd, int authmethod )
{
	/*
	 * The bind request looks like this:
	 *	BindRequest ::= SEQUENCE {
	 *		version		INTEGER,
	 *		name		DistinguishedName,	 -- who
	 *		authentication	CHOICE {
	 *			simple		[0] OCTET STRING -- passwd
#ifdef KERBEROS
	 *			krbv42ldap	[1] OCTET STRING
	 *			krbv42dsa	[2] OCTET STRING
#endif
	 *		}
	 *	}
	 * all wrapped up in an LDAPMessage sequence.
	 */

	Debug( LDAP_DEBUG_TRACE, "ldap_bind\n", 0, 0, 0 );

	switch ( authmethod ) {
	case LDAP_AUTH_SIMPLE:
		return( ldap_simple_bind( ld, dn, passwd ) );

#ifdef KERBEROS
	case LDAP_AUTH_KRBV41:
		return( ldap_kerberos_bind1( ld, dn ) );

	case LDAP_AUTH_KRBV42:
		return( ldap_kerberos_bind2( ld, dn ) );
#endif

	default:
		ld->ld_errno = LDAP_AUTH_UNKNOWN;
		return( -1 );
	}
}

/*
 * ldap_bind_s - bind to the ldap server (and X.500).  The dn and password
 * of the entry to which to bind are supplied, along with the authentication
 * method to use.  This routine just calls whichever bind routine is
 * appropriate and returns the result of the bind (e.g. LDAP_SUCCESS or
 * some other error indication).  Note, the kerberos support assumes the
 * user already has a valid tgt for now.
 *
 * Examples:
 *	ldap_bind_s( ld, "cn=manager, o=university of michigan, c=us",
 *	    "secret", LDAP_AUTH_SIMPLE )
 *	ldap_bind_s( ld, "cn=manager, o=university of michigan, c=us",
 *	    NULL, LDAP_AUTH_KRBV4 )
 */
int
ldap_bind_s( LDAP *ld, char *dn, char *passwd, int authmethod )
{
	Debug( LDAP_DEBUG_TRACE, "ldap_bind_s\n", 0, 0, 0 );

	switch ( authmethod ) {
	case LDAP_AUTH_SIMPLE:
		return( ldap_simple_bind_s( ld, dn, passwd ) );

#ifdef KERBEROS
	case LDAP_AUTH_KRBV4:
		return( ldap_kerberos_bind_s( ld, dn ) );

	case LDAP_AUTH_KRBV41:
		return( ldap_kerberos_bind1_s( ld, dn ) );

	case LDAP_AUTH_KRBV42:
		return( ldap_kerberos_bind2_s( ld, dn ) );
#endif

	default:
		return( ld->ld_errno = LDAP_AUTH_UNKNOWN );
	}
}

void
ldap_set_rebind_proc( LDAP *ld, int (*rebindproc)( LDAP *ld, char **dnp,
	char **passwdp, int *authmethodp, int freeit, void *arg ), void *arg)
{
	ld->ld_rebindproc = rebindproc;
	ld->ld_rebindarg = arg;
}