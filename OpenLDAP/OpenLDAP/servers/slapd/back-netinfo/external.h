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

#ifndef _NETINFO_EXTERNAL_H
#define _NETINFO_EXTERNAL_H

LDAP_BEGIN_DECL

extern int netinfo_back_initialize LDAP_P(( BackendInfo *bi ));
extern int netinfo_back_open LDAP_P(( BackendInfo *bi ));
extern int netinfo_back_close LDAP_P(( BackendInfo *bi ));
extern int netinfo_back_destroy LDAP_P(( BackendInfo *bi ));

extern int netinfo_back_db_init LDAP_P(( BackendDB *bd ));
extern int netinfo_back_db_open LDAP_P(( BackendDB *bd ));
extern int netinfo_back_db_close LDAP_P(( BackendDB *bd ));
extern int netinfo_back_db_destroy LDAP_P(( BackendDB *bd ));

extern int netinfo_back_db_config LDAP_P(( BackendDB *bd,
	const char *fname, int lineno, int argc, char **argv ));

extern int netinfo_back_bind LDAP_P(( struct slap_op *op, 
	struct slap_rep *rs ));

extern int netinfo_back_conn_destroy LDAP_P(( BackendDB *bd,
	Connection *conn ));

extern int netinfo_back_referrals LDAP_P(( struct slap_op *op, 
	struct slap_rep *rs ));

extern int netinfo_back_search LDAP_P(( struct slap_op *op, 
	struct slap_rep *rs ));

extern int netinfo_back_compare LDAP_P(( struct slap_op *op, 
	struct slap_rep *rs ));

extern int netinfo_back_modify LDAP_P(( struct slap_op *op, 
	struct slap_rep *rs ));

extern int netinfo_back_modrdn LDAP_P(( struct slap_op *op, 
	struct slap_rep *rs ));

extern int netinfo_back_add LDAP_P(( struct slap_op *op, 
	struct slap_rep *rs ));

extern int netinfo_back_delete LDAP_P(( struct slap_op *op, 
	struct slap_rep *rs ));

extern int netinfo_back_abandon LDAP_P(( struct slap_op *op, 
	struct slap_rep *rs ));

extern int netinfo_back_exop_passwd LDAP_P(( struct slap_op *op, 
	struct slap_rep *rs));

extern int netinfo_back_extended LDAP_P(( struct slap_op *op, 
	struct slap_rep *rs ));

extern int netinfo_back_attribute LDAP_P((
	BackendDB *be,
	Connection *conn,
	Operation *op,
	Entry *target,
	struct berval *endn,
	AttributeDescription *entry_at,
	BerVarray *vals));

extern int netinfo_back_operational LDAP_P(( struct slap_op *op, 
	struct slap_rep *rs, int opattrs, Attribute **ap ));

extern int netinfo_back_entry_release LDAP_P(( struct slap_op *op, Entry *e, 
	int rw ));

extern int netinfo_back_group LDAP_P((
	Backend *be,
	Connection *conn,
	Operation *op,
	Entry *target,
	struct berval *gr_ndn,
	struct berval *op_ndn,
	ObjectClass *group_oc,
	AttributeDescription *group_at));

extern int netinfo_tool_entry_open LDAP_P((BackendDB *db, int mode));
extern int netinfo_tool_entry_close LDAP_P((BackendDB *db));
extern ID netinfo_tool_entry_next LDAP_P((BackendDB *db));
extern Entry *netinfo_tool_entry_get LDAP_P((BackendDB *db, ID id));
extern ID netinfo_tool_entry_put LDAP_P((BackendDB *db, Entry *e, struct berval *text));
extern int netinfo_tool_entry_reindex LDAP_P((BackendDB *db, ID id));
extern int netinfo_tool_sync LDAP_P((BackendDB *db));

LDAP_END_DECL

#endif /* _NETINFO_EXTERNAL_H */

