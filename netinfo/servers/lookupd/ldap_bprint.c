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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "lber.h"

/*
 * Print arbitrary stuff, for debugging.
 */

#ifdef LDAP_DEBUG

#ifndef NO_USERINTERFACE
#define BPLEN	48

void
lber_bprint( char *data, int len )
{
    static char	hexdig[] = "0123456789abcdef";
    char	out[ BPLEN ];
    int		i = 0;

    memset( out, 0, BPLEN );
    for ( ;; ) {
	if ( len < 1 ) {
	    fprintf( stderr, "\t%s\n", ( i == 0 ) ? "(end)" : out );
	    break;
	}

#ifndef HEX
	if ( isgraph( (unsigned char)*data )) {
	    out[ i ] = ' ';
	    out[ i+1 ] = *data;
	} else {
#endif
	    out[ i ] = hexdig[ ( *data & 0xf0 ) >> 4 ];
	    out[ i+1 ] = hexdig[ *data & 0x0f ];
#ifndef HEX
	}
#endif
	i += 2;
	len--;
	data++;

	if ( i > BPLEN - 2 ) {
	    fprintf( stderr, "\t%s\n", out );
	    memset( out, 0, BPLEN );
	    i = 0;
	    continue;
	}
	out[ i++ ] = ' ';
    }
}
#else /* NO_USERINTERFACE */
void
lber_bprint( char *data, int len )
{
}
#endif /* NO_USERINTERFACE */

#endif
