/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

/*!
 @header SecItemSchema.h - The thing that does the stuff with the gibli.
 */

#ifndef _SECURITYD_SECITEMSCHEMA_H_
#define _SECURITYD_SECITEMSCHEMA_H_

#include <securityd/SecDbItem.h>

__BEGIN_DECLS

// Current schema is always kc_schemas[0]
extern const SecDbSchema *kc_schemas[];

// Direct class accesors for current schema
extern const SecDbClass genp_class;
extern const SecDbClass inet_class;
extern const SecDbClass cert_class;
extern const SecDbClass keys_class;

// Not really a class per-se
extern const SecDbClass identity_class;

// Class with 1 element in it which is the database version.
extern const SecDbClass tversion_class;

// Direct attribute accesors for current schema
extern const SecDbAttr v6v_Data;

extern const SecDbAttr v6agrp;
extern const SecDbAttr v6svce;
extern const SecDbAttr v7vwht;
extern const SecDbAttr v7tkid;
extern const SecDbAttr v7utomb;
extern const SecDbAttr v8musr;

// TODO: Directly expose other important attributes like
// kSecItemSyncAttr, kSecItemTombAttr, kSecItemCdatAttr, kSecItemMdatAttr, kSecItemDataAttr
// This will prevent having to do lookups in SecDbItem for things by kind.

__END_DECLS

#endif /* _SECURITYD_SECITEMSCHEMA_H_ */
