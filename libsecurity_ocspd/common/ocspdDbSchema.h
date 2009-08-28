/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 * ocspdDbSchema.h
 *
 * Definitions of structures which define the schema, including attributes
 * and indexes, for the standard tables that are part of the OCSP server
 * database.
 */

#ifndef _OCSPD_DB_SCHEMA_H_
#define _OCSPD_DB_SCHEMA_H_

#include <Security/cssmtype.h>

/* 
 * Structure used to store information which is needed to create
 * a relation with indexes. The info in one of these structs maps to one
 * record type in a CSSM_DBINFO - both record attribute info and index info.
 */
typedef struct  {
	CSSM_DB_RECORDTYPE				recordType;
	const char						*relationName;
	uint32							numberOfAttributes;
	const CSSM_DB_ATTRIBUTE_INFO	*attrInfo;
	uint32							numIndexes;
	const CSSM_DB_INDEX_INFO		*indexInfo;
} OcspdDbRelationInfo;

// Macros used to simplify declarations of attributes and indexes.

// declare a CSSM_DB_ATTRIBUTE_INFO
#define DB_ATTRIBUTE(name, type) \
	{  CSSM_DB_ATTRIBUTE_NAME_AS_STRING, \
	   { (char*) name }, \
	   CSSM_DB_ATTRIBUTE_FORMAT_ ## type \
	}

// declare a CSSM_DB_INDEX_INFO
#define UNIQUE_INDEX_ATTRIBUTE(name, type) \
	{  CSSM_DB_INDEX_NONUNIQUE, \
	   CSSM_DB_INDEX_ON_ATTRIBUTE, \
	   {  CSSM_DB_ATTRIBUTE_NAME_AS_STRING, \
	      { name }, \
		  CSSM_DB_ATTRIBUTE_FORMAT_ ## type \
	   } \
	}

// declare a OcspdDbRelationInfo
#define RELATION_INFO(relationId, name, attributes, indexes) \
	{ relationId, \
	  name, \
	  sizeof(attributes) / sizeof(CSSM_DB_ATTRIBUTE_INFO), \
	  attributes, \
	  sizeof(indexes) / sizeof(CSSM_DB_INDEX_INFO), \
	  indexes }

/*
 * Currently there is only one relation in the OCSPD database; this is an array
 * containing it. 
 */
extern const OcspdDbRelationInfo kOcspDbRelations[];
extern unsigned kNumOcspDbRelations;

/* 
 * CSSM_DB_RECORDTYPE for the ocspd DB schema.
 */
#define OCSPD_DB_RECORDTYPE		0x11223344

/*
 * Here are the attribute names and formats in kOcspDbRelation. All attributes
 * have format CSSM_DB_ATTRIBUTE_NAME_AS_STRING. 
 */
 
/* DER encoded CertID - a record can have multiple values of this */
#define OCSPD_DBATTR_CERT_ID			DB_ATTRIBUTE("CertID", BLOB)
/* URI */
#define OCSPD_DBATTR_URI				DB_ATTRIBUTE("URI", STRING)
 /* Expiration time, CSSM_TIMESTRING format */
#define OCSPD_DBATTR_EXPIRATION			DB_ATTRIBUTE("Expiration", STRING)

#define OCSPD_NUM_DB_ATTRS		3

#endif /* _OCSPD_DB_SCHEMA_H_ */

