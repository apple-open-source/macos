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
// MDSSchema.cpp
//
// Definitions of structures which define the schema, including attributes
// and indexes, for the standard tables that are part of the MDS database.
//

#include "MDSSchema.h"
#include <Security/mds_schema.h>

//
// Schema for the lone table in the Object Directory Database.
//

static const CSSM_DB_SCHEMA_ATTRIBUTE_INFO kAttributesObjectRelation[] = {
	SCHEMA_ATTRIBUTE(0, ModuleId, STRING),
	SCHEMA_ATTRIBUTE(1, Manifest, BLOB),
	SCHEMA_ATTRIBUTE(2, ModuleName, STRING),
	SCHEMA_ATTRIBUTE(3, Path, STRING),
	SCHEMA_ATTRIBUTE(4, ProductVersion, STRING)
};

static const CSSM_DB_SCHEMA_INDEX_INFO kIndexObjectRelation[] = {
	UNIQUE_INDEX_ATTRIBUTE(0)
};

const RelationInfo kObjectRelation =
	RELATION_INFO(MDS_OBJECT_RECORDTYPE, kAttributesObjectRelation, kIndexObjectRelation);

//
// Schema for the various tables in the CDSA Directory Database.
//

// CSSM Relation.

static const CSSM_DB_SCHEMA_ATTRIBUTE_INFO kAttributesCSSMRelation[] =
{
	SCHEMA_ATTRIBUTE(0, ModuleID, STRING),
	SCHEMA_ATTRIBUTE(1, CDSAVersion, STRING),
	SCHEMA_ATTRIBUTE(2, Vendor, STRING),
	SCHEMA_ATTRIBUTE(3, Desc, STRING),
	SCHEMA_ATTRIBUTE(4, NativeServices, UINT32)
};

static const CSSM_DB_SCHEMA_INDEX_INFO kIndexCSSMRelation[] =
{
	UNIQUE_INDEX_ATTRIBUTE(0)
};

const RelationInfo kCSSMRelation =
	RELATION_INFO(MDS_CDSADIR_CSSM_RECORDTYPE, kAttributesCSSMRelation, kIndexCSSMRelation);

// KRMM Relation.
	
static const CSSM_DB_SCHEMA_ATTRIBUTE_INFO kAttributesKRMMRelation[] =
{
	SCHEMA_ATTRIBUTE(0, CSSMGuid, STRING),
	SCHEMA_ATTRIBUTE(1, PolicyType, UINT32),
	SCHEMA_ATTRIBUTE(2, PolicyName, STRING),
	SCHEMA_ATTRIBUTE(3, PolicyPath, STRING),
	SCHEMA_ATTRIBUTE(4, PolicyInfo, BLOB),
	SCHEMA_ATTRIBUTE(5, PolicyManifest, BLOB)
};

static const CSSM_DB_SCHEMA_INDEX_INFO kIndexKRMMRelation[] =
{
	UNIQUE_INDEX_ATTRIBUTE(0),
	UNIQUE_INDEX_ATTRIBUTE(1)
};

const RelationInfo kKRMMRelation =
	RELATION_INFO(MDS_CDSADIR_KRMM_RECORDTYPE, kAttributesKRMMRelation, kIndexKRMMRelation);

// Common Relation.

static const CSSM_DB_SCHEMA_ATTRIBUTE_INFO kAttributesCommonRelation[] =
{
	SCHEMA_ATTRIBUTE(0, ModuleID, STRING),
	SCHEMA_ATTRIBUTE(1, Manifest, BLOB),
	SCHEMA_ATTRIBUTE(2, ModuleName, STRING),
	SCHEMA_ATTRIBUTE(3, Path, STRING),
	SCHEMA_ATTRIBUTE(4, CDSAVersion, STRING),
	SCHEMA_ATTRIBUTE(5, Desc, STRING),
	SCHEMA_ATTRIBUTE(6, DynamicFlag, UINT32),
	SCHEMA_ATTRIBUTE(7, MultiThreadFlag, UINT32),
	SCHEMA_ATTRIBUTE(8, ServiceMask, UINT32)
};

static const CSSM_DB_SCHEMA_INDEX_INFO kIndexCommonRelation[] =
{
	UNIQUE_INDEX_ATTRIBUTE(0)
};

const RelationInfo kCommonRelation =
	RELATION_INFO(MDS_CDSADIR_COMMON_RECORDTYPE, kAttributesCommonRelation, kIndexCommonRelation);

