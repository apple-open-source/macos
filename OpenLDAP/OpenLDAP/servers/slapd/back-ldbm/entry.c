/* entry.c - ldbm backend entry_release routine */
/* $OpenLDAP: pkg/ldap/servers/slapd/back-ldbm/entry.c,v 1.11.2.1 2003/03/03 17:10:10 kurt Exp $ */
/*
 * Copyright 1998-2003 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

#include "portable.h"

#include <stdio.h>

#include <ac/socket.h>
#include <ac/string.h>

#include "slap.h"
#include "back-ldbm.h"
#include "proto-back-ldbm.h"


int
ldbm_back_entry_release_rw(
	Backend *be,
	Connection *conn,
	Operation *op,
	Entry   *e,
	int     rw
)
{
	struct ldbminfo	*li = (struct ldbminfo *) be->be_private;

	if ( slapMode == SLAP_SERVER_MODE ) {
		/* free entry and reader or writer lock */
		cache_return_entry_rw( &li->li_cache, e, rw ); 
		if( rw ) {
			ldap_pvt_thread_rdwr_wunlock( &li->li_giant_rwlock );
		} else {
			ldap_pvt_thread_rdwr_runlock( &li->li_giant_rwlock );
		}

	} else {
		entry_free( e );
	}

	return 0;
}
