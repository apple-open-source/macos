/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * putpwpasswd()
 * Copyright (C) 1989 by NeXT, Inc.
 *
 * Changes a user's password entry. Works only for NetInfo. 
 *
 * NOTE: This is not done in lookupd because we need to know
 * the identity of the user and there is currently no way to
 * get that information through a Mach message. Privileged users
 * get privileged sockets and do not need to supply a old password
 * if they are changing their password on the master server for their
 * account entry. Unprivileged users get unprivileged sockets and must
 * supply the old password.
 */
#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <ctype.h>
#include <netinfo/ni.h>
#include <libc.h>

static const ni_name NAME_USERS = "users";
static const ni_name NAME_PASSWD = "passwd";

static int
changeit(
	 void *ni, 
	 ni_id *id, 
	 char *login,
	 char *old_passwd, 
	 char *new_passwd
	 )
{
	ni_proplist pl;
	ni_index i;
	ni_index prop_index;
	ni_property prop;
	ni_status stat;

	ni_setabort(ni, TRUE);
	ni_needwrite(ni, TRUE);
	ni_setuser(ni, login);
	ni_setpassword(ni, old_passwd);

	if (ni_read(ni, id, &pl) != NI_OK) {
		return (0);
	}
	prop_index = NI_INDEX_NULL;
	for (i = 0; i < pl.nipl_len; i++) {
		if (ni_name_match(pl.nipl_val[i].nip_name, NAME_PASSWD)) {
			prop_index = i;
			break;
		}
	}
	if (prop_index == NI_INDEX_NULL) {
		prop.nip_name = NAME_PASSWD;
		prop.nip_val.ninl_len = 1;
		prop.nip_val.ninl_val = &new_passwd;
		stat = ni_createprop(ni, id, prop, NI_INDEX_NULL);
	} else {
		if (pl.nipl_val[i].nip_val.ninl_len == 0) {
			stat = ni_createname(ni, id, prop_index, 
					     new_passwd, 0);
		} else {
			stat = ni_writename(ni, id, prop_index, 0, 
					    new_passwd);
		}
	}
	ni_proplist_free(&pl);
	return (stat == NI_OK);
}

int
putpwpasswd(
	    char *login,
	    char *old_passwd, /* cleartext */
	    char *new_passwd /* encrypted */
	    )
{
	char *dir;
	void *ni;
	void *newni;
	ni_id id;
	ni_status stat;
	int changed;
	
	stat = ni_open(NULL, ".", &ni);
	if (stat != NI_OK) {
		return (0);
	}

	dir = malloc(1 + strlen(NAME_USERS) + 1 + strlen(login) + 1);
	sprintf(dir, "/%s/%s", NAME_USERS, login);

	changed = 0;
	for (;;) {
		stat = ni_pathsearch(ni, &id, dir);
		if (stat == NI_OK) {
			changed = changeit(ni, &id, login, old_passwd, 
					   new_passwd);
			break;
		}
		stat = ni_open(ni, "..", &newni);
		if (stat != NI_OK) {
			break;
		}
		ni_free(ni);
		ni = newni;
	}
	free(dir);
	ni_free(ni);
	return (changed);
}	

