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

#ifndef BACK_NETINFO_H
#define BACK_NETINFO_H

#include <mach/boolean.h>
#include <NetInfo/dsengine.h>
#include <NetInfo/dsindex.h>
#include <NetInfo/dsutil.h>
#include <NetInfo/dsdata.h>
#include <NetInfo/dsrecord.h>
#include <NetInfo/dsx500.h>
#include <NetInfo/dsx500dit.h>
#include <NetInfo/dsreference.h>
#include <NetInfo/network.h> /* for sys_is_my_address() */

LDAP_BEGIN_DECL

struct netinfo_referral {
	struct berval nc;
	struct berval nnc;
	int count;
	BerVarray refs;
};

struct schemamapinfo;

/*
 * The backend specific data.
 */
struct dsinfo {
	int flags;
	char *datasource;
	struct berval suffix;
	struct berval nsuffix;
	struct netinfo_referral *parent;
	struct netinfo_referral **children;
	dsdata *auth_user;
	dsdata *auth_password;
	ldap_pvt_thread_mutex_t *lock;
	dsengine *engine;
	struct schemamapinfo *map;
	int promote_admins;
};

/* A callback for mapping a NetInfo value to an LDAP one. */
typedef dsstatus (*ni_to_x500_transform_t) LDAP_P((BackendDB *be, struct berval *dst, dsdata *src, void *private));

/* A callback for mapping an LDAP value to a NetInfo one. */
typedef dsstatus (*x500_to_ni_transform_t) LDAP_P((BackendDB *be, dsdata **dst, struct berval *src, u_int32_t type, void *private));

/* Attribute mapping info */
struct atmap {
	u_int32_t super;
	u_int32_t type;
	u_int32_t selector; 
	dsdata *ni_key;
	AttributeDescription *x500;

	ni_to_x500_transform_t niToX500Transform;
	void *niToX500Arg;
	x500_to_ni_transform_t x500ToNiTransform;
	void *x500ToNiArg;

	u_int32_t retain;
};

LDAP_END_DECL

#define ENGINE_LOCK(bi)		ldap_pvt_thread_mutex_lock(bi->lock)
#define ENGINE_UNLOCK(bi)	ldap_pvt_thread_mutex_unlock(bi->lock)

#include "external.h"

#include "proto-back-netinfo.h"

extern const dsdata netinfo_back_name_name;
extern const dsdata netinfo_back_name_passwd;
extern const dsdata netinfo_back_name_address;
extern const dsdata netinfo_back_name_trusted_networks;
extern const dsdata netinfo_back_name_readers;
extern const dsdata netinfo_back_name_writers;
extern const dsdata netinfo_back_name_rdn;
extern const dsdata netinfo_back_name_networks;
extern const dsdata netinfo_back_name_groups;
extern const dsdata netinfo_back_name_admin;
extern const dsdata netinfo_back_name_users;
extern const dsdata netinfo_back_name_promote_admins;

extern const dsdata netinfo_back_access_user_anybody;
extern const dsdata netinfo_back_access_user_super;

extern AttributeDescription *netinfo_back_ad_dSID;
extern AttributeDescription *netinfo_back_ad_nIVersionNumber;
extern AttributeDescription *netinfo_back_ad_nISerialNumber;

#endif /* BACK_NETINFO_H */

