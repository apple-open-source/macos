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
 *  getentry.c
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

#include "lber.h"
#include "ldap_ldap-int.h"
#include "ldap.h"

/* ARGSUSED */
LDAPMessage *
ldap_first_entry( LDAP *ld, LDAPMessage *chain )
{
	return( chain == NULLMSG || chain->lm_msgtype == LDAP_RES_SEARCH_RESULT
	    ? NULLMSG : chain );
}

/* ARGSUSED */
LDAPMessage *ldap_next_entry( LDAP *ld, LDAPMessage *entry )
{
	if ( entry == NULLMSG || entry->lm_chain == NULLMSG
	    || entry->lm_chain->lm_msgtype == LDAP_RES_SEARCH_RESULT )
		return( NULLMSG );

	return( entry->lm_chain );
}

/* ARGSUSED */
int
ldap_count_entries( LDAP *ld, LDAPMessage *chain )
{
	int	i;

	for ( i = 0; chain != NULL && chain->lm_msgtype
	    != LDAP_RES_SEARCH_RESULT; chain = chain->lm_chain )
		i++;

	return( i );
}
