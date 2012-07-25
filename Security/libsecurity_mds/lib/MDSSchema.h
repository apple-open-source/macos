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
#include "MDSAttrStrings.h"

namespace Security
{

// Structure used to store information which is needed to create
// a relation with indexes. The info in one of these structs maps to one
// record type in a CSSM_DBINFO - both record attribute info and index info.
// The nameValues field refers to an array of MDSNameValuePair array pointers
// which are used to convert attribute values from strings to uint32s via
// MDS_StringToUint32. The nameValues array is parallel to the AttributeInfo
// array.
struct RelationInfo {
	CSSM_DB_RECORDTYPE DataRecordType;
	const char *relationName;
	uint32 NumberOfAttributes;
	const CSSM_DB_ATTRIBUTE_INFO *AttributeInfo;
	const MDSNameValuePair **nameValues;
	uint32 NumberOfIndexes;
	const CSSM_DB_INDEX_INFO *IndexInfo;
};

// Macros used to simplify declarations of attributes and indexes.

// declare a CSSM_DB_ATTRIBUTE_INFO
#define DB_ATTRIBUTE(name, type) \
	{  CSSM_DB_ATTRIBUTE_NAME_AS_STRING, \
	   {(char*) #name}, \
	   CSSM_DB_ATTRIBUTE_FORMAT_ ## type \
	}

// declare a CSSM_DB_INDEX_INFO
#define UNIQUE_INDEX_ATTRIBUTE(name, type) \
	{  CSSM_DB_INDEX_UNIQUE, \
	   CSSM_DB_INDEX_ON_ATTRIBUTE, \
	   {  CSSM_DB_ATTRIBUTE_NAME_AS_STRING, \
	      {(char*) #name}, \
		  CSSM_DB_ATTRIBUTE_FORMAT_ ## type \
	   } \
	}

// declare a RelationInfo
#define RELATION_INFO(relationId, attributes, nameValues, indexes) \
	{ relationId, \
	  #relationId, \
	  sizeof(attributes) / sizeof(CSSM_DB_ATTRIBUTE_INFO), \
	  attributes, \
	  nameValues, \
	  sizeof(indexes) / sizeof(CSSM_DB_INDEX_INFO), \
	  indexes }

// Object directory DB - one built-in schema.
extern const RelationInfo kObjectRelation;

// list of all built-in schema for the CDSA Directory DB.
extern const RelationInfo kMDSRelationInfo[];
extern const unsigned kNumMdsRelations;			// size of kMDSRelationInfo[]

// special case "subschema" for parsing CSPCapabilities. 
extern const RelationInfo CSPCapabilitiesDict1RelInfo;
extern const RelationInfo CSPCapabilitiesDict2RelInfo;
extern const RelationInfo CSPCapabilitiesDict3RelInfo;

// special case "subschema" for parsing TPPolicyOids. 
extern const RelationInfo TpPolicyOidsDict1RelInfo;
extern const RelationInfo TpPolicyOidsDict2RelInfo;

// Map a CSSM_DB_RECORDTYPE to a RelationInfo *.
extern const RelationInfo *MDSRecordTypeToRelation(
	CSSM_DB_RECORDTYPE recordType);
	
// same as above, based on record type as string. 
extern const RelationInfo *MDSRecordTypeNameToRelation(
	const char *recordTypeName);
	
} // end namespace Security

#endif // _MDSSCHEMA_H
