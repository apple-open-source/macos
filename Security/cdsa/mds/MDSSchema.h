/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// MDSSchema.h
//
// Declarations of structures which define the schema, including attributes
// and indexes, for the standard tables that are part of the MDS database.
//

#ifndef _MDSSCHEMA_H
#define _MDSSCHEMA_H

#include <Security/cssmtype.h>

// Structure used to store information which is needed to create
// a relation with indexes.

struct RelationInfo {
	CSSM_DB_RECORDTYPE relationId;
	const char *relationName;
	uint32 numAttributes;
	const CSSM_DB_SCHEMA_ATTRIBUTE_INFO *attributes;
	uint32 numIndexes;
	const CSSM_DB_SCHEMA_INDEX_INFO *indexes;
};

// Macros used to simplify declarations of attributes and indexes.

#define SCHEMA_ATTRIBUTE(id, name, type) \
	{ id, #name, { 0, NULL }, CSSM_DB_ATTRIBUTE_FORMAT_ ## type }
	
#define UNIQUE_INDEX_ATTRIBUTE(attributeId) \
	{ attributeId, 0, CSSM_DB_INDEX_UNIQUE, CSSM_DB_INDEX_ON_ATTRIBUTE }

#define RELATION_INFO(relationId, attributes, indexes) \
	{ relationId, \
	  #relationId, \
	  sizeof(attributes) / sizeof(CSSM_DB_SCHEMA_ATTRIBUTE_INFO), \
	  attributes, \
	  sizeof(indexes) / sizeof(CSSM_DB_SCHEMA_INDEX_INFO), \
	  indexes }

// Declarations of schema for MDS relations.

extern const RelationInfo kObjectRelation;
extern const RelationInfo kCSSMRelation;
extern const RelationInfo kKRMMRelation;
extern const RelationInfo kCommonRelation;

#endif // _MDSSCHEMA_H
