/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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

#ifndef _XDR_CSSM_H
#define _XDR_CSSM_H

#include <Security/cssmtype.h>
#include "sec_xdr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define xdr_intptr_t xdr_long

/*
 * There are a number of types that expand to 64 bit on 64 bit platforms, but clipped to 32 bit across ipc.
 */
bool_t sec_xdr_clip_long(XDR *xdrs, long *objp);

/* 
 * Some CSSM types have pointers that the remote end of an RPC doesn't care
 * about, but which must be accounted for during size calculations.  We
 * therefore declare a (void *) XDR handler.  
 */
bool_t xdr_voidptr(XDR *xdrs, void **objp);

#define xdr_uint8 xdr_u_char
#define xdr_uint16 xdr_u_int16_t
#define xdr_sint32 xdr_int32_t
#define xdr_uint32 xdr_u_int32_t
#define xdr_CSSM_SIZE sec_xdr_clip_long
#define xdr_CSSM_INTPTR sec_xdr_clip_long

#define xdr_CSSM_HANDLE xdr_CSSM_INTPTR
#define xdr_CSSM_MODULE_HANDLE xdr_CSSM_HANDLE
#define xdr_CSSM_CSP_HANDLE xdr_CSSM_MODULE_HANDLE
#define xdr_CSSM_TP_HANDLE xdr_CSSM_MODULE_HANDLE
#define xdr_CSSM_AC_HANDLE xdr_CSSM_MODULE_HANDLE
#define xdr_CSSM_CL_HANDLE xdr_CSSM_MODULE_HANDLE
#define xdr_CSSM_DL_HANDLE xdr_CSSM_MODULE_HANDLE
#define xdr_CSSM_DB_HANDLE xdr_CSSM_MODULE_HANDLE
#define xdr_CSSM_ACL_HANDLE xdr_CSSM_HANDLE

#define xdr_CSSM_BOOL xdr_sint32
#define xdr_CSSM_RETURN xdr_sint32

#define xdr_CSSM_STRING(xdrs, objp) xdr_opaque(xdrs, objp, sizeof(CSSM_STRING))

#define xdr_CSSM_SERVICE_MASK xdr_uint32
#define xdr_CSSM_SERVICE_TYPE xdr_CSSM_SERVICE_MASK

#define xdr_CSSM_NET_ADDRESS_TYPE xdr_uint32

#define xdr_CSSM_WORDID_TYPE xdr_sint32

#define xdr_CSSM_LIST_ELEMENT_TYPE xdr_uint32
#define xdr_CSSM_LIST_TYPE xdr_uint32

#define xdr_CSSM_CERT_TYPE xdr_uint32
#define xdr_CSSM_CERT_ENCODING xdr_uint32
#define xdr_CSSM_CERT_PARSE_FORMAT xdr_uint32

#define xdr_CSSM_OID xdr_CSSM_DATA

#define xdr_CSSM_CERTGROUP_TYPE xdr_uint32

#define xdr_CSSM_ACL_AUTHORIZATION_TAG xdr_sint32

#define xdr_CSSM_HEADERVERSION xdr_uint32

#define xdr_CSSM_KEYBLOB_TYPE xdr_uint32
#define xdr_CSSM_KEYBLOB_FORMAT xdr_uint32

#define xdr_CSSM_KEYCLASS xdr_uint32

#define xdr_CSSM_KEYATTR_FLAGS xdr_uint32

#define xdr_CSSM_KEYUSE xdr_uint32

#define xdr_CSSM_ALGORITHMS xdr_uint32

#define xdr_CSSM_ENCRYPT_MODE xdr_uint32

#define xdr_CSSM_CONTEXT_TYPE xdr_uint32

#define xdr_CSSM_ATTRIBUTE_TYPE xdr_uint32

#define xdr_CSSM_PADDING xdr_uint32

#define xdr_CSSM_DB_RECORDTYPE xdr_uint32

#define xdr_CSSM_DB_ATTRIBUTE_NAME_FORMAT xdr_uint32

#define xdr_CSSM_DB_ATTRIBUTE_FORMAT xdr_uint32

#define xdr_CSSM_DB_CONJUNCTIVE xdr_uint32

#define xdr_CSSM_DB_OPERATOR xdr_uint32

#define xdr_CSSM_QUERY_FLAGS xdr_uint32

#define xdr_CSSM_PKCS5_PBKDF2_PRF xdr_uint32

typedef struct {
	CSSM_ALGORITHMS algorithm;
	CSSM_DATA baseData;
} CSSM_DERIVE_DATA;

typedef struct {
	uint32 count;
	CSSM_ACL_OWNER_PROTOTYPE *acls;
} CSSM_ACL_OWNER_PROTOTYPE_ARRAY;

typedef struct {
	uint32 count;
	CSSM_ACL_ENTRY_INFO *acls;
} CSSM_ACL_ENTRY_INFO_ARRAY, *CSSM_ACL_ENTRY_INFO_ARRAY_PTR;

bool_t xdr_CSSM_DATA(XDR *xdrs, CSSM_DATA *objp);
bool_t xdr_CSSM_GUID(XDR *xdrs, CSSM_GUID *objp);
bool_t xdr_CSSM_VERSION(XDR *xdrs, CSSM_VERSION *objp);
bool_t xdr_CSSM_SUBSERVICE_UID(XDR *xdrs, CSSM_SUBSERVICE_UID *objp);
bool_t xdr_CSSM_NET_ADDRESS(XDR *xdrs, CSSM_NET_ADDRESS *objp);
bool_t xdr_CSSM_CRYPTO_DATA(XDR *xdrs, CSSM_CRYPTO_DATA *objp);
bool_t xdr_CSSM_LIST_ELEMENT(XDR *xdrs, CSSM_LIST_ELEMENT *objp);
bool_t xdr_CSSM_LIST(XDR *xdrs, CSSM_LIST *objp);
bool_t xdr_CSSM_SAMPLE(XDR *xdrs, CSSM_SAMPLE *objp);
bool_t xdr_CSSM_SAMPLEGROUP(XDR *xdrs, CSSM_SAMPLEGROUP *objp);
bool_t xdr_CSSM_ENCODED_CERT(XDR *xdrs, CSSM_ENCODED_CERT *objp);
bool_t xdr_CSSM_CERTGROUP(XDR *xdrs, CSSM_CERTGROUP *objp);
bool_t xdr_CSSM_BASE_CERTS(XDR *xdrs, CSSM_BASE_CERTS *objp);
bool_t xdr_CSSM_ACCESS_CREDENTIALS(XDR *xdrs, CSSM_ACCESS_CREDENTIALS *objp);
bool_t xdr_CSSM_ACCESS_CREDENTIALS_PTR(XDR *xdrs, CSSM_ACCESS_CREDENTIALS_PTR *objp);
bool_t xdr_CSSM_AUTHORIZATIONGROUP(XDR *xdrs, CSSM_AUTHORIZATIONGROUP *objp);
bool_t xdr_CSSM_ACL_VALIDITY_PERIOD(XDR *xdrs, CSSM_ACL_VALIDITY_PERIOD *objp);
bool_t xdr_CSSM_ACL_ENTRY_PROTOTYPE(XDR *xdrs, CSSM_ACL_ENTRY_PROTOTYPE *objp);
bool_t xdr_CSSM_ACL_ENTRY_PROTOTYPE_PTR(XDR *xdrs, CSSM_ACL_ENTRY_PROTOTYPE_PTR *objp);
bool_t xdr_CSSM_ACL_OWNER_PROTOTYPE(XDR *xdrs, CSSM_ACL_OWNER_PROTOTYPE *objp);
bool_t xdr_CSSM_ACL_OWNER_PROTOTYPE_PTR(XDR *xdrs, CSSM_ACL_OWNER_PROTOTYPE_PTR *objp);
bool_t xdr_CSSM_ACL_ENTRY_INPUT(XDR *xdrs, CSSM_ACL_ENTRY_INPUT *objp);
bool_t xdr_CSSM_ACL_ENTRY_INPUT_PTR(XDR *xdrs, CSSM_ACL_ENTRY_INPUT_PTR *objp);
bool_t xdr_CSSM_ACL_ENTRY_INFO(XDR *xdrs, CSSM_ACL_ENTRY_INFO *objp);
bool_t xdr_CSSM_ACL_ENTRY_INFO_ARRAY(XDR *xdrs, CSSM_ACL_ENTRY_INFO_ARRAY *objp);
bool_t xdr_CSSM_ACL_ENTRY_INFO_ARRAY_PTR(XDR *xdrs, CSSM_ACL_ENTRY_INFO_ARRAY_PTR *objp);
bool_t xdr_CSSM_DATE(XDR *xdrs, CSSM_DATE *objp);
bool_t xdr_CSSM_RANGE(XDR *xdrs, CSSM_RANGE *objp);
bool_t xdr_CSSM_KEYHEADER(XDR *xdrs, CSSM_KEYHEADER *objp);
bool_t xdr_CSSM_KEYHEADER_PTR(XDR *xdrs, CSSM_KEYHEADER_PTR *objp);
bool_t xdr_CSSM_KEY(XDR *xdrs, CSSM_KEY *objp);
bool_t xdr_CSSM_KEY_PTR(XDR *xdrs, CSSM_KEY_PTR *objp);
bool_t xdr_CSSM_POSSIBLY_KEY_IN_DATA_WITH_BOOL(XDR *xdrs, CSSM_DATA *objp, bool_t in_iskey);
bool_t xdr_CSSM_POSSIBLY_KEY_IN_DATA(XDR *xdrs, CSSM_DATA *objp);
bool_t xdr_CSSM_POSSIBLY_KEY_IN_DATA_PTR(XDR *xdrs, CSSM_DATA_PTR *objp);
bool_t xdr_CSSM_KEY_IN_DATA(XDR *xdrs, CSSM_DATA *objp);
bool_t xdr_CSSM_NO_KEY_IN_DATA(XDR *xdrs, CSSM_DATA *objp);
bool_t xdr_CSSM_DB_ATTRIBUTE_INFO(XDR *xdrs, CSSM_DB_ATTRIBUTE_INFO *objp);
bool_t xdr_CSSM_DB_ATTRIBUTE_DATA(XDR *xdrs, CSSM_DB_ATTRIBUTE_DATA *objp);
bool_t xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA(XDR *xdrs, CSSM_DB_RECORD_ATTRIBUTE_DATA *objp);
bool_t xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR(XDR *xdrs, CSSM_DB_RECORD_ATTRIBUTE_DATA_PTR *objp);
bool_t xdr_CSSM_SELECTION_PREDICATE(XDR *xdrs, CSSM_SELECTION_PREDICATE *objp);
bool_t xdr_CSSM_QUERY_LIMITS(XDR *xdrs, CSSM_QUERY_LIMITS *objp);
bool_t xdr_CSSM_QUERY(XDR *xdrs, CSSM_QUERY *objp);
bool_t xdr_CSSM_QUERY_PTR(XDR *xdrs, CSSM_QUERY_PTR *objp);
bool_t xdr_CSSM_CONTEXT_ATTRIBUTE(XDR *xdrs, CSSM_CONTEXT_ATTRIBUTE *objp);
bool_t xdr_CSSM_CONTEXT(XDR *xdrs, CSSM_CONTEXT *objp);
bool_t xdr_CSSM_CONTEXT_PTR(XDR *xdrs, CSSM_CONTEXT_PTR *objp);
bool_t xdr_CSSM_DL_DB_HANDLE(XDR *xdrs, CSSM_DL_DB_HANDLE *objp);
bool_t xdr_CSSM_SUBSERVICE_UID(XDR *xdrs, CSSM_SUBSERVICE_UID *objp);
bool_t xdr_CSSM_NET_ADDRESS(XDR *xdrs, CSSM_NET_ADDRESS *objp);
bool_t xdr_CSSM_PKCS5_PBKDF2_PARAMS(XDR *xdrs, CSSM_PKCS5_PBKDF2_PARAMS *objp);
bool_t xdr_CSSM_DERIVE_DATA(XDR *xdrs, CSSM_DERIVE_DATA *objp);
bool_t xdr_CSSM_DERIVE_DATA_PTR(XDR *xdrs, CSSM_DERIVE_DATA **objp);
bool_t xdr_CSSM_ACL_OWNER_PROTOTYPE_ARRAY(XDR *xdrs, CSSM_ACL_OWNER_PROTOTYPE_ARRAY *objp);

/*
toplevel used converters:

xdr_CSSM_ACCESS_CREDENTIALS
xdr_CSSM_DB_RECORD_ATTRIBUTE_DATA
xdr_CSSM_ACCESS_CREDENTIALS
xdr_CSSM_ACL_ENTRY_PROTOTYPE
xdr_DLDbFlatIdentifier
xdr_CSSM_CONTEXT
xdr_CSSM_DERIVE_DATA
xdr_CSSM_KEY_PTR
xdr_CSSM_KEYHEADER_PTR

not currently used:

bool_t xdr_CSSM_FIELD(XDR *xdrs, CSSM_FIELD *objp);
bool_t xdr_CSSM_FIELDGROUP(XDR *xdrs, CSSM_FIELDGROUP *objp);
bool_t xdr_CSSM_TUPLE(XDR *xdrs, CSSM_TUPLE *objp);
bool_t xdr_CSSM_PARSED_CERT(XDR *xdrs, CSSM_PARSED_CERT *objp);
bool_t xdr_CSSM_CERT_PAIR(XDR *xdrs, CSSM_CERT_PAIR *objp);
*/

#ifdef __cplusplus
}
#endif

#endif /* !_XDR_CSSM_H */
