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
 * Copyright 1998-2000 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

#ifndef __DSX500_H__
#define __DSX500_H__

/* Explode a DN into an array of RDNs. */
char **dsx500_explode_dn(char *dn, u_int32_t notypes);

/* Explode an RDN into an array of AVAs. */
char **dsx500_explode_rdn(char *rdn, u_int32_t notypes);

/* Map a DN to a NetInfo path. */
char *dsx500_dn_to_netinfo_string_path(char *dce);

/* Map a NetInfo path to a DN. */
char * dsx500_netinfo_string_path_to_dn(char *dn);

/* make a new DN by concatenating these DN + RDN */
char *dsx500_make_dn(char *p_dn, char *newrdn);

/* validate an RDN */
u_int32_t dsx500_validate_rdn(char *rdn);

/* get RDN key (left hand side) */
char *dsx500_rdn_attr_type(char *s);

/* get RDN value (right hand side) */
char *dsx500_rdn_attr_value(char * rdn);

#endif __DSX500_H__
