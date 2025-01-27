/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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

/*
 * ocspdDbSchema.cpp
 *
 * Definitions of structures which define the schema, including attributes
 * and indexes, for the standard tables that are part of the OCSP server
 * database.
 */

#include "ocspdDbSchema.h"
#include <cstring>

//
// Schema for the lone table in the OCSPD database.
//
static const CSSM_DB_ATTRIBUTE_INFO ocspdDbAttrs[] = {
	OCSPD_DBATTR_CERT_ID,
	OCSPD_DBATTR_URI,
	OCSPD_DBATTR_EXPIRATION
};

static const CSSM_DB_INDEX_INFO ocspdDbIndex[] = {
	UNIQUE_INDEX_ATTRIBUTE((char*) "CertID", BLOB)
};

const OcspdDbRelationInfo kOcspDbRelations[] =
{
	RELATION_INFO(OCSPD_DB_RECORDTYPE, "ocpsd", ocspdDbAttrs, ocspdDbIndex)
};

unsigned kNumOcspDbRelations = sizeof(kOcspDbRelations) / sizeof(kOcspDbRelations[0]);

