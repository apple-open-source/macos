/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * netinfo.c - Routines for dealing with the NetInfo database.
 *
 **********************************************************************
 * HISTORY
 * 10-Jun-89  Peter King
 *	Created.
 * 23-Feb-98  Dieter Siegmund (dieter@apple.com)
 *      Removed all of the promiscous-related stuff,
 *	left with routines to do host creation/lookup.
 **********************************************************************
 */

/*
 * Include Files
 */
#import <ctype.h>
#import <pwd.h>
#import	<netdb.h>
#import <string.h>
#import <syslog.h>
#import <sys/types.h>
#import <sys/socket.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/if_ether.h>
#import <arpa/inet.h>
#import <string.h>
#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import "host_identifier.h"
#import	"netinfo.h"

/*
 * External Routines
 */
char *  	ether_ntoa(struct ether_addr *e);


/*
 * Exported routines
 */

void
ni_proplist_dump(ni_proplist * pl)
{
    int i, j;

    for (i = 0; i < pl->nipl_len; i++) {
	ni_property * prop = &(pl->nipl_val[i]);
	ni_namelist * nl_p = &prop->nip_val;
	if (nl_p->ninl_len == 0) {
	    printf("\"%s\"\n", prop->nip_name);
	}
	else {
	    printf("\"%s\" = ", prop->nip_name);
	    for (j = 0; j < nl_p->ninl_len; j++)
		printf("%s\"%s\"", (j == 0) ? "" : ", ", nl_p->ninl_val[j]);
	    printf("\n");
	}
    }
}

boolean_t
ni_get_checksum(void * h, unsigned long * checksum)
{
    ni_proplist		pl;
    ni_status		status;
    char * 		val;
    ni_index 		where;

    NI_INIT(&pl);
    status = ni_statistics(h, &pl);
    if (status != NI_OK) {
	fprintf(stderr, 
		"ni_statistics failed, %s\n", ni_error(status));
	return (FALSE);
    }
    where = ni_proplist_match(pl, "checksum", NULL);
    if (where == -1 ||
	pl.nipl_val[where].nip_val.ninl_len == 0) {
	fprintf(stderr, "couldn't get checksum\n");
	ni_proplist_free(&pl);
	return (FALSE);
    }
    val = pl.nipl_val[where].nip_val.ninl_val[0];
    *checksum = strtoul(val, 0, 0);
    ni_proplist_free(&pl);
    return (TRUE);
}

void
ni_set_prop(ni_proplist * pl_p, ni_name prop, ni_name value, 
	    boolean_t * modified)
{
    ni_index		where;
    
    where = ni_proplist_match(*pl_p, prop, NULL);
    if (where != NI_INDEX_NULL) {
	if (value != NULL && where == ni_proplist_match(*pl_p, prop, value)) {
	    return; /* already set */
	}
	ni_proplist_delete(pl_p, where);
    }
    ni_proplist_insertprop(pl_p, prop, value, where);
    if (modified)
	*modified = TRUE;
    return;
}

void
ni_delete_prop(ni_proplist * pl_p, ni_name prop, boolean_t * modified)
{
    int where;

    where = ni_proplist_match(*pl_p, prop, NULL);
    if (where != NI_INDEX_NULL) {
	ni_proplist_delete(pl_p, where);
	if (modified)
	    *modified = TRUE;
    }
    return;
}

