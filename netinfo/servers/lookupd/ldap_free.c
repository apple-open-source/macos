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
 *  Copyright (c) 1994 The Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  free.c - some free routines are included here to avoid having to
 *           link in lots of extra code when not using certain features
 */

#ifdef NOTDEF
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1994 The Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifdef MACOS
#include <stdlib.h>
#include "macos.h"
#else /* MACOS */
#ifdef DOS
#include <malloc.h>
#include "msdos.h"
#else /* DOS */
#include <sys/types.h>
#include <stdlib.h>
#endif /* DOS */
#endif /* MACOS */

#include "lber.h"
#include "ldap_ldap-int.h"
#include "ldap.h"

void
ldap_getfilter_free( LDAPFiltDesc *lfdp )
{
    LDAPFiltList	*flp, *nextflp;
    LDAPFiltInfo	*fip, *nextfip;

    for ( flp = lfdp->lfd_filtlist; flp != NULL; flp = nextflp ) {
	for ( fip = flp->lfl_ilist; fip != NULL; fip = nextfip ) {
	    nextfip = fip->lfi_next;
	    free( fip->lfi_filter );
	    free( fip->lfi_desc );
	    free( fip );
	}
	nextflp = flp->lfl_next;
	free( flp->lfl_pattern );
	free( flp->lfl_delims );
	free( flp->lfl_tag );
	free( flp );
    }

    if ( lfdp->lfd_curvalcopy != NULL ) {
	free( lfdp->lfd_curvalcopy );
    }
    if ( lfdp->lfd_curvalwords != NULL ) {
	free( lfdp->lfd_curvalwords );
    }
    if ( lfdp->lfd_filtprefix != NULL ) {
	free( lfdp->lfd_filtprefix );
    }
    if ( lfdp->lfd_filtsuffix != NULL ) {
	free( lfdp->lfd_filtsuffix );
    }

    free( lfdp );
}

/*
 * free a null-terminated array of pointers to mod structures. the
 * structures are freed, not the array itself, unless the freemods
 * flag is set.
 */

void
ldap_mods_free( LDAPMod **mods, int freemods )
{
	int	i;

	if ( mods == NULL )
		return;

	for ( i = 0; mods[i] != NULL; i++ ) {
		if ( mods[i]->mod_op & LDAP_MOD_BVALUES ) {
			ber_bvecfree( mods[i]->mod_bvalues );
		} else {
			ldap_value_free( mods[i]->mod_values );
		}
		free( (char *) mods[i] );
	}

	if ( freemods )
		free( (char *) mods );
}
