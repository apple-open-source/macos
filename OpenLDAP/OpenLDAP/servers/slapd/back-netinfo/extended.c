/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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

#include "portable.h"

#include <stdio.h>

#include <ac/socket.h>
#include <ac/string.h>

#include "slap.h"
#include "back-netinfo.h"

static struct exop {
	char *oid;
	BI_op_extended *extended;
} exop_table[] = {
	{ LDAP_EXOP_MODIFY_PASSWD, netinfo_back_exop_passwd },
	{ NULL, NULL }
};

int
netinfo_back_extended(
	Backend *be,
	Connection *conn,
	Operation *op,
	const char *reqoid,
	struct berval *reqdata,
	char **rspoid,
	struct berval **rspdata,
	LDAPControl ***rspctrls,
	const char **text,
	BerVarray *refs 
)
{
	int i;

	for (i = 0; exop_table[i].oid != NULL; i++)
	{
		if (strcmp(exop_table[i].oid, reqoid) == 0)
		{
			return (exop_table[i].extended)(
				be, conn, op,
				reqoid, reqdata,
				rspoid, rspdata, rspctrls,
				text, refs);
		}
	}

	*text = "not supported within naming context";
	return LDAP_OPERATIONS_ERROR;
}

