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
 *  unbind.c
 */

#ifdef NOTDEF
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1990 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef MACOS
#include <stdlib.h>
#include "macos.h"
#else /* MACOS */
#if defined( DOS ) || defined( _WIN32 )
#include "msdos.h"
#ifdef NCSA
#include "externs.h"
#endif /* NCSA */
#else /* DOS */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#endif /* DOS */
#endif /* MACOS */

#include "lber.h"
#include "ldap_ldap-int.h"
#include "ldap.h"


int
ldap_unbind( LDAP *ld )
{
	Debug( LDAP_DEBUG_TRACE, "ldap_unbind\n", 0, 0, 0 );

	return( ldap_ld_free( ld, 1 ));
}


int
ldap_ld_free( LDAP *ld, int close )
{
	LDAPMessage	*lm, *next;
	int		err = LDAP_SUCCESS;
#ifdef LDAP_REFERRALS
	LDAPRequest	*lr, *nextlr;
#endif /* LDAP_REFERRALS */

	if ( ld->ld_sb.sb_naddr == 0 ) {
#ifdef LDAP_REFERRALS
		/* free LDAP structure and outstanding requests/responses */
		for ( lr = ld->ld_requests; lr != NULL; lr = nextlr ) {
			nextlr = lr->lr_next;
			free_request( ld, lr );
		}

		/* free and unbind from all open connections */
		while ( ld->ld_conns != NULL ) {
			free_connection( ld, ld->ld_conns, 1, close );
		}
#else /* LDAP_REFERRALS */
		if ( close ) {
			err = send_unbind( ld, &ld->ld_sb );
			close_connection( &ld->ld_sb );
		}
#endif /* LDAP_REFERRALS */
	} else {
		int	i;

		for ( i = 0; i < ld->ld_sb.sb_naddr; ++i ) {
			free( ld->ld_sb.sb_addrs[ i ] );
		}
		free( ld->ld_sb.sb_addrs );
		free( ld->ld_sb.sb_fromaddr );
	}

	for ( lm = ld->ld_responses; lm != NULL; lm = next ) {
		next = lm->lm_next;
		ldap_msgfree( lm );
	}

#ifndef NO_CACHE
	if ( ld->ld_cache != NULL )
		ldap_destroy_cache( ld );
#endif /* !NO_CACHE */
	if ( ld->ld_error != NULL )
		free( ld->ld_error );
	if ( ld->ld_matched != NULL )
		free( ld->ld_matched );
	if ( ld->ld_host != NULL )
		free( ld->ld_host );
	if ( ld->ld_defbase != NULL )
		free( ld->ld_defbase );
	if ( ld->ld_ufnprefix != NULL )
		free( ld->ld_ufnprefix );
	if ( ld->ld_filtd != NULL )
		ldap_getfilter_free( ld->ld_filtd );
#ifndef LDAP_REFERRALS
	if ( ld->ld_sb.sb_ber.ber_buf != NULL )
		free( ld->ld_sb.sb_ber.ber_buf );
#endif /* !LDAP_REFERRALS */
	if ( ld->ld_abandoned != NULL )
		free( ld->ld_abandoned );

#ifdef LDAP_REFERRALS
	if ( ld->ld_selectinfo != NULL )
		free_select_info( ld->ld_selectinfo );
#endif /* LDAP_REFERRALS */

	if ( ld->ld_defhost != NULL )
		free( ld->ld_defhost );

	free( (char *) ld );

	return( err );
}

int
ldap_unbind_s( LDAP *ld )
{
	return( ldap_ld_free( ld, 1 ));
}


int
send_unbind( LDAP *ld, Sockbuf *sb )
{
	BerElement	*ber;

	Debug( LDAP_DEBUG_TRACE, "send_unbind\n", 0, 0, 0 );

	/* create a message to send */
	if ( (ber = alloc_ber_with_options( ld )) == NULLBER ) {
		return( ld->ld_errno );
	}

	/* fill it in */
	if ( ber_printf( ber, "{itn}", ++ld->ld_msgid,
	    LDAP_REQ_UNBIND ) == -1 ) {
		ld->ld_errno = LDAP_ENCODING_ERROR;
		ber_free( ber, 1 );
		return( ld->ld_errno );
	}

	/* send the message */
	if ( ber_flush( sb, ber, 1 ) == -1 ) {
		ld->ld_errno = LDAP_SERVER_DOWN;
		ber_free( ber, 1 );
		return( ld->ld_errno );
	}

	return( LDAP_SUCCESS );
}
