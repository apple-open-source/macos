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
 *  getattr.c
 */

#ifdef NOTDEF 
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
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
#include <NetInfo/config.h>
#ifdef _UNIX_BSD_43_
extern char *strdup(char *);
#endif

#include "lber.h"
#include "ldap_ldap-int.h"
#include "ldap.h"

char *
ldap_first_attribute( LDAP *ld, LDAPMessage *entry, BerElement **ber )
{
	long	len;

	Debug( LDAP_DEBUG_TRACE, "ldap_first_attribute\n", 0, 0, 0 );

	if ( (*ber = alloc_ber_with_options( ld )) == NULLBER ) {
		return( NULL );
	}

	**ber = *entry->lm_ber;

	/* 
	 * Skip past the sequence, dn, sequence of sequence, snarf the
	 * attribute type, and skip the set of values, leaving us
	 * positioned right before the next attribute type/value sequence.
	 */

	len = LDAP_MAX_ATTR_LEN;
	if ( ber_scanf( *ber, "{x{{sx}", ld->ld_attrbuffer, &len )
	    == LBER_ERROR ) {
		ld->ld_errno = LDAP_DECODING_ERROR;
		ber_free( *ber, 0 );
		return( NULL );
	}

	return( strdup( ld->ld_attrbuffer ) );
}

/* ARGSUSED */
char *
ldap_next_attribute( LDAP *ld, LDAPMessage *entry, BerElement *ber )
{
	long	len;

	Debug( LDAP_DEBUG_TRACE, "ldap_next_attribute\n", 0, 0, 0 );

	/* skip sequence, snarf attribute type, skip values */
	len = LDAP_MAX_ATTR_LEN;
	if ( ber_scanf( ber, "{sx}", ld->ld_attrbuffer, &len ) 
	    == LBER_ERROR ) {
		ld->ld_errno = LDAP_DECODING_ERROR;
		ber_free( ber, 0 );
		return( NULL );
	}

	return( strdup( ld->ld_attrbuffer ) );
}
