/* error.c - BDB errcall routine */
/* $OpenLDAP: pkg/ldap/servers/slapd/back-bdb/error.c,v 1.6 2002/01/04 20:17:49 kurt Exp $ */
/*
 * Copyright 1998-2002 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

#include "portable.h"

#include <stdio.h>
#include <ac/string.h>

#include "slap.h"
#include "back-bdb.h"

void bdb_errcall( const char *pfx, char * msg )
{
	Debug( LDAP_DEBUG_ANY, "bdb(%s): %s\n", pfx, msg, 0 );
}
