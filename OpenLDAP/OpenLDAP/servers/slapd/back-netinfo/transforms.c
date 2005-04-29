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

/*
 * Find the user account name for a distinguished name.
 */
dsstatus distinguishedNameToPosixNameTransform(BackendDB *be, dsdata **dst, struct berval *src, u_int32_t type, void *private)
{
	dsstatus status;
	struct berval ndn;
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	u_int32_t match;
	dsdata dsid;

	if (!IsStringDataType(type))
		return DSStatusNoData;

	/* XXX bind usage of this results in redundant normalization */
	if (dnNormalize(0, NULL, NULL, src, &ndn, NULL) != LDAP_SUCCESS)
		return DSStatusInvalidPath;

	status = netinfo_back_dn_pathmatch(be, &ndn, &match);
	if (status != DSStatusOK)
	{
		ch_free(ndn.bv_val);
		return status;
	}

	ch_free(ndn.bv_val);

	/*
	 * Can't convert the name from the local DN because
	 * schema mapping is not supported by dsengine;
	 * hence we take the step of converting it to a
	 * wrapped directory ID first.
	 */
	dsid.type = DataTypeDirectoryID;
	dsid.retain = 1;
	dsid.length = 4;
	match = htonl(match);
	dsid.data = (void *)&match;

	*dst = dsengine_convert_name(di->engine, &dsid, NameTypeDirectoryID, NameTypeUserName);

	return (*dst == NULL) ? DSStatusInvalidPath : DSStatusOK;
}

/*
 * Find the distinguished name for a user account.
 */
dsstatus posixNameToDistinguishedNameTransform(BackendDB *be, struct berval *dst, dsdata *src, void *private)
{
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsdata *dsid;
	u_int32_t match;

	/*
	 * We cannot convert to a DN directly as dsengine is
	 * unaware of schema mapping; hence, we need to 
	 * take the intermediate step of converting to
	 * a wrapped DSID.
	 */
	dsid = dsengine_convert_name(di->engine, src, NameTypeUserName, NameTypeDirectoryID);
	if (dsid == NULL)
		return DSStatusInvalidKey;

	match = dsdata_to_dsid(dsid);

	dsdata_release(dsid);

	return netinfo_back_global_dn(be, match, dst);
}

/*
 * Append a constant string to a value.
 */
dsstatus appendPrefixTransform(BackendDB *be, struct berval *dst, dsdata *src, void *private)
{
	struct berval *prefix = (struct berval *)private;

	/*
	 * userPassword is interesting as, if created by NetInfo,
	 * it will be stored as a string and the length will 
	 * include the NUL terminator (which is probably the
	 * wrong way to do things, but it's how it is done);
	 * if created by the LDAP bridge, then it will be stored
	 * as a binary attribute (per the userPassword syntax)
	 * without the NUL termination. We need to handle both
	 * cases here, hence the explicit check.
	 */
	if (IsStringDataType(src->type))
		dst->bv_len = src->length - 1 + prefix->bv_len;
	else
		dst->bv_len = src->length + prefix->bv_len;

	dst->bv_val = ch_malloc(dst->bv_len + 1);

	AC_MEMCPY(dst->bv_val, prefix->bv_val, prefix->bv_len);
	AC_MEMCPY(dst->bv_val + prefix->bv_len, src->data, src->length);
	dst->bv_val[dst->bv_len] = '\0';

	return DSStatusOK;	
}

/*
 * Remove a constant case-insensitive string from a value.
 */
dsstatus removeCaseIgnorePrefixTransform(BackendDB *be, dsdata **dst, struct berval *src, u_int32_t type, void *private)
{
	struct berval *prefix = (struct berval *)private;
	struct berval stripped;

	/* don't check type as userPassword is OctetString */

	if (strncasecmp(src->bv_val, prefix->bv_val, prefix->bv_len) != 0)
		return DSStatusConstraintViolation;

	stripped.bv_val = src->bv_val + prefix->bv_len;
	stripped.bv_len = src->bv_len - prefix->bv_len;

	*dst = berval_to_dsdata(&stripped, type);

	return (*dst == NULL) ? DSStatusFailed : DSStatusOK;
}

/*
 * Remove a constant case-sensitive string from a value.
 */
dsstatus removeCaseExactPrefixTransform(BackendDB *be, dsdata **dst, struct berval *src, u_int32_t type, void *private)
{
	struct berval *prefix = (struct berval *)private;
	struct berval stripped;

	/* don't check type as userPassword is OctetString */

	if (strncmp(src->bv_val, prefix->bv_val, prefix->bv_len) != 0)
		return DSStatusConstraintViolation;

	stripped.bv_val = src->bv_val + prefix->bv_len;
	stripped.bv_len = src->bv_len - prefix->bv_len;

	*dst = berval_to_dsdata(&stripped, type);

	return (*dst == NULL) ? DSStatusFailed : DSStatusOK;
}

/*
 * Activate a stored DN; used for referential integrity.
 */
dsstatus distinguishedNameRetrieveTransform(BackendDB *be, struct berval *dst, dsdata *src, void *private)
{
	dsstatus status;
	char buf[64];
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	dsreference *ref;
	u_int32_t dsid;

	status = DSStatusFailed;

	switch (src->type)
	{
		/* RefInt V2: use out-of-band mechanism to update DNs. */
		case DataTypeDSReference:
			ref = dsdata_to_dsreference(src);
			if (ref != NULL && ref->dn != NULL)
			{
				dsdata_to_berval(dst, ref->dn);
				status = DSStatusOK;
			}

			dsreference_release(ref);
			break;

		/* String representation of DN. */
		case DataTypeCStr:
		case DataTypeCaseCStr:
		case DataTypeUTF8Str:
		case DataTypeCaseUTF8Str:
			dsdata_to_berval(dst, src);
			status = DSStatusOK;
			break;

		/* RefInt V1: Store-local ID pointer. */
		case DataTypeDirectoryID:
			status = DSStatusInvalidPath;
			dsid = dsdata_to_dsid(src);

			if (di->flags & DSENGINE_FLAGS_DEREFERENCE_IDS)
			{
				status = netinfo_back_global_dn(be, dsid, dst);
			}

			if (status != DSStatusOK)
			{
				snprintf(buf, sizeof(buf), "dSID=%u", dsid);
				dst->bv_val = ch_strdup(buf);
				dst->bv_len = strlen(dst->bv_val);
				status = DSStatusOK;
			}
			break;

		default:
			status = DSStatusNoData;
			break;
	}

	return status;
}

/*
 * Choose a way to store a DN that is optimal for maintaining
 * referential integrity.
 */
dsstatus distinguishedNameStoreTransform(BackendDB *be, dsdata **dst, struct berval *bv, u_int32_t type, void *private)
{	
	struct dsinfo *di = (struct dsinfo *)be->be_private;
	struct berval dn;
	dsstatus status;

	if (type != DataTypeDirectoryID)
		return DSStatusNoData;

	if (di->flags & DSENGINE_FLAGS_DEREFERENCE_IDS)
	{
		struct berval ndn;
		u_int32_t match;

		if (dnNormalize(0, NULL, NULL, bv, &ndn, NULL) != LDAP_SUCCESS)
			return DSStatusInvalidPath;

		/* This may fail if the path is not local. */
		status = netinfo_back_dn_pathmatch(be, &ndn, &match);
		if (status == DSStatusOK)
		{
			*dst = dsid_to_dsdata(match);
			ch_free(ndn.bv_val);
			return DSStatusOK;
		}
		ch_free(ndn.bv_val);
	}

	if (dnPretty(NULL, bv, &dn, NULL) != LDAP_SUCCESS)
		return DSStatusFailed;

	*dst = berval_to_dsdata(&dn, DataTypeCaseUTF8Str);

	ch_free(dn.bv_val);

	return DSStatusOK;
}

/*
 * Simple conversion routine.
 */
dsstatus dsdataToBerval(BackendDB *be, struct berval *dst, dsdata *src, void *private)
{
	if (dsdata_to_berval(dst, src) == NULL)
		return DSStatusFailed;

	/* Check for zero-length values, they are not allowed by LDAP. */
	if (dst->bv_len == 0)
	{
		ch_free(dst->bv_val);
		return DSStatusNoData;
	}

	return DSStatusOK;
}

/*
 * Simple conversion routine.
 */
dsstatus bervalToDsdata(BackendDB *be, dsdata **dst, struct berval *bv, u_int32_t type, void *private)
{
	*dst = berval_to_dsdata(bv, type);

	if (*dst == NULL)
		return DSStatusFailed;

	return DSStatusOK;
}
