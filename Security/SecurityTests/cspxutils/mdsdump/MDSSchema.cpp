/*
 * Copyright (c) 2000-2001,2004 Apple Computer, Inc. All Rights Reserved.
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
// MDSSchema.cpp  - COPIED FROM libsecurity_mds since this is not exported from 
//			     Security.framework

//
// Definitions of structures which define the schema, including attributes
// and indexes, for the standard tables that are part of the MDS database.
//

#include "MDSSchema.h"
#include <cstring>
#include <Security/mds_schema.h>

/*
 * There appears to be a bug in AppleDatabase which prevents our assigning 
 * schema to the meta-tables.
 */
#define DEFINE_META_TABLES		0

//
// Schema for the lone table in the Object Directory Database.
//
static const CSSM_DB_ATTRIBUTE_INFO objectAttrs[] = {
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(Manifest, BLOB),
	DB_ATTRIBUTE(ModuleName, STRING),
	DB_ATTRIBUTE(Path, STRING),
	DB_ATTRIBUTE(ProductVersion, STRING),
	
	/* not in the CDSA spec; denotes a plugin which is statically linked to CSSM */
	DB_ATTRIBUTE(BuiltIn, UINT32),
};

static const CSSM_DB_INDEX_INFO objectIndex[] = {
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING)
};

const RelationInfo kObjectRelation =
	RELATION_INFO(MDS_OBJECT_RECORDTYPE, 
		objectAttrs, 
		objectIndex);

//
// Schema for the various tables in the CDSA Directory Database.
//

// CSSM Relation.
static const CSSM_DB_ATTRIBUTE_INFO cssmAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(CDSAVersion, STRING),
	DB_ATTRIBUTE(Vendor, STRING),
	DB_ATTRIBUTE(Desc, STRING),
	DB_ATTRIBUTE(NativeServices, UINT32),
};

static const CSSM_DB_INDEX_INFO cssmIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING)
};

// KRMM Relation.
static const CSSM_DB_ATTRIBUTE_INFO krmmAttrs[] =
{
	DB_ATTRIBUTE(CSSMGuid, STRING),
	DB_ATTRIBUTE(PolicyType, UINT32),
	DB_ATTRIBUTE(PolicyName, STRING),
	DB_ATTRIBUTE(PolicyPath, STRING),
	DB_ATTRIBUTE(PolicyInfo, BLOB),
	DB_ATTRIBUTE(PolicyManifest, BLOB),
	/*
	 * This attribute is not defined in the CDSA spec. It's only here, in the schema,
	 * to avoid throwing exceptions when searching a DB for any records associated
	 * with a specified GUID - in all other schemas, a guid is specified as a 
	 * ModuleID.
	 */
	DB_ATTRIBUTE(ModuleID, STRING),
};

static const CSSM_DB_INDEX_INFO krmmIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(CSSMGuid, STRING),
	UNIQUE_INDEX_ATTRIBUTE(PolicyType, UINT32)
};

// EMM Relation.
static const CSSM_DB_ATTRIBUTE_INFO emmAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(Manifest, BLOB),
	DB_ATTRIBUTE(ModuleName, STRING),
	DB_ATTRIBUTE(Path, STRING),
	DB_ATTRIBUTE(CDSAVersion, STRING),
	DB_ATTRIBUTE(EMMSpecVersion, STRING),
	DB_ATTRIBUTE(Desc, STRING),
	DB_ATTRIBUTE(PolicyStmt, BLOB),
	DB_ATTRIBUTE(EmmVersion, STRING),
	DB_ATTRIBUTE(EmmVendor, STRING),
	DB_ATTRIBUTE(EmmType, UINT32),		// does this need a name/value table?
};

static const CSSM_DB_INDEX_INFO emmIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING)
};

// Primary EMM Service Provider Relation.
static const CSSM_DB_ATTRIBUTE_INFO emmPrimaryAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(ServiceType, UINT32),
	DB_ATTRIBUTE(Manifest, BLOB),
	DB_ATTRIBUTE(ModuleName, STRING),
	DB_ATTRIBUTE(ProductVersion, STRING),
	DB_ATTRIBUTE(Vendor, STRING),
	DB_ATTRIBUTE(SampleTypes, MULTI_UINT32),
	DB_ATTRIBUTE(AclSubjectTypes, MULTI_UINT32),
	DB_ATTRIBUTE(AuthTags, MULTI_UINT32),
	DB_ATTRIBUTE(EmmSpecVersion, STRING),
};

static const CSSM_DB_INDEX_INFO emmPrimaryIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32),
	UNIQUE_INDEX_ATTRIBUTE(ServiceType, UINT32)
};

// Common Relation.
static const CSSM_DB_ATTRIBUTE_INFO commonAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(Manifest, BLOB),
	DB_ATTRIBUTE(ModuleName, STRING),
	DB_ATTRIBUTE(Path, STRING),
	DB_ATTRIBUTE(CDSAVersion, STRING),
	DB_ATTRIBUTE(Desc, STRING),
	DB_ATTRIBUTE(DynamicFlag, UINT32),
	DB_ATTRIBUTE(MultiThreadFlag, UINT32),
	DB_ATTRIBUTE(ServiceMask, UINT32),
};

static const CSSM_DB_INDEX_INFO commonIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING)
};

// CSP Primary Relation.
static const CSSM_DB_ATTRIBUTE_INFO cspPrimaryAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(Manifest, BLOB),
	DB_ATTRIBUTE(ModuleName, STRING),
	DB_ATTRIBUTE(ProductVersion, STRING),
	DB_ATTRIBUTE(Vendor, STRING),
	DB_ATTRIBUTE(CspType, UINT32),
	DB_ATTRIBUTE(CspFlags, UINT32),
	DB_ATTRIBUTE(CspCustomFlags, UINT32),
	DB_ATTRIBUTE(UseeTags, MULTI_UINT32),
	DB_ATTRIBUTE(SampleTypes, MULTI_UINT32),
	DB_ATTRIBUTE(AclSubjectTypes, MULTI_UINT32),
	DB_ATTRIBUTE(AuthTags, MULTI_UINT32),
};

static const CSSM_DB_INDEX_INFO cspPrimaryIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32)
};

// CSP Capabilities Relation.
static const CSSM_DB_ATTRIBUTE_INFO cspCapabilitiesAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(UseeTag, UINT32),
	DB_ATTRIBUTE(ContextType, UINT32),
	DB_ATTRIBUTE(AlgType, UINT32),
	DB_ATTRIBUTE(GroupId, UINT32),
	DB_ATTRIBUTE(AttributeType, UINT32),
	DB_ATTRIBUTE(AttributeValue, MULTI_UINT32),
	DB_ATTRIBUTE(Description, STRING),
};

static const CSSM_DB_INDEX_INFO cspCapabilitiesIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32),
	UNIQUE_INDEX_ATTRIBUTE(UseeTag, UINT32),
	UNIQUE_INDEX_ATTRIBUTE(ContextType, UINT32),
	UNIQUE_INDEX_ATTRIBUTE(AlgType, UINT32),
	UNIQUE_INDEX_ATTRIBUTE(GroupId, UINT32),
	UNIQUE_INDEX_ATTRIBUTE(AttributeType, STRING)
};

// special case "subschema" for parsing CSPCapabilities. These arrays correspond
// dictionaries within a CSPCapabilities info file; they are not part of 
// our DB's schema. They are declared only to streamline the 
// MDSAttrParser::parseCspCapabilitiesRecord function. No index info is needed.

// top-level info, applied to the dictionary for the whole file.
static const CSSM_DB_ATTRIBUTE_INFO kAttributesCSPCapabilitiesDict1[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
};
const RelationInfo CSPCapabilitiesDict1RelInfo = 
	RELATION_INFO(
		MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE,		// actually a don't care
		kAttributesCSPCapabilitiesDict1,
		NULL);										// no index

// "Capabilities" is an array of dictionaries of these
static const CSSM_DB_ATTRIBUTE_INFO kAttributesCSPCapabilitiesDict2[] =
{
	DB_ATTRIBUTE(AlgType, UINT32),
	DB_ATTRIBUTE(ContextType, UINT32),
	DB_ATTRIBUTE(UseeTag, UINT32),
	DB_ATTRIBUTE(Description, STRING),
};
const RelationInfo CSPCapabilitiesDict2RelInfo = 
	RELATION_INFO(
		MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE,		// actually a don't care
		kAttributesCSPCapabilitiesDict2, 
		NULL);										// no index

// Within a Capabilities array, the Attributes array is an array of
// Dictionaries of these.
static const CSSM_DB_ATTRIBUTE_INFO kAttributesCSPCapabilitiesDict3[] =
{
	DB_ATTRIBUTE(AttributeType, UINT32),
	DB_ATTRIBUTE(AttributeValue, MULTI_UINT32),
};
const RelationInfo CSPCapabilitiesDict3RelInfo = 
	RELATION_INFO(
		MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE,		// actually a don't care
		kAttributesCSPCapabilitiesDict3, 
		NULL);



// CSP Encapsulated Products Relation.
static const CSSM_DB_ATTRIBUTE_INFO cspEncapsulatedAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(ProductDesc, STRING),
	DB_ATTRIBUTE(ProductVendor, STRING),
	DB_ATTRIBUTE(ProductVersion, STRING),
	DB_ATTRIBUTE(ProductFlags, UINT32),
	DB_ATTRIBUTE(CustomFlags, UINT32),
	DB_ATTRIBUTE(StandardDesc, STRING),
	DB_ATTRIBUTE(StandardVersion, STRING),
	DB_ATTRIBUTE(ReaderDesc, STRING),
	DB_ATTRIBUTE(ReaderVendor, STRING),
	DB_ATTRIBUTE(ReaderVersion, STRING),
	DB_ATTRIBUTE(ReaderFirmwareVersion, STRING),
	DB_ATTRIBUTE(ReaderFlags, UINT32),
	DB_ATTRIBUTE(ReaderCustomFlags, UINT32),
	DB_ATTRIBUTE(ReaderSerialNumber, STRING),
};

static const CSSM_DB_INDEX_INFO cspEncapsulatedIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32)
};

// CSP Smartcardinfo Relation.
static const CSSM_DB_ATTRIBUTE_INFO cspSmartCardAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(ScDesc, STRING),
	DB_ATTRIBUTE(ScVendor, STRING),
	DB_ATTRIBUTE(ScVersion, STRING),
	DB_ATTRIBUTE(ScFirmwareVersion, STRING),
	DB_ATTRIBUTE(ScFlags, UINT32),
	DB_ATTRIBUTE(ScCustomFlags, UINT32),
	DB_ATTRIBUTE(ScSerialNumber, STRING),
};
static const CSSM_DB_INDEX_INFO cspSmartCardIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32),
	UNIQUE_INDEX_ATTRIBUTE(ScDesc, STRING),
	UNIQUE_INDEX_ATTRIBUTE(ScVendor, STRING),
	UNIQUE_INDEX_ATTRIBUTE(ScVersion, STRING),
	UNIQUE_INDEX_ATTRIBUTE(ScFirmwareVersion, STRING),
	UNIQUE_INDEX_ATTRIBUTE(ScFlags, UINT32)
};

// DL Primary Relation.
static const CSSM_DB_ATTRIBUTE_INFO dlPrimaryAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(Manifest, BLOB),
	DB_ATTRIBUTE(ModuleName, STRING),
	DB_ATTRIBUTE(ProductVersion, STRING),
	DB_ATTRIBUTE(Vendor, STRING),
	DB_ATTRIBUTE(DLType, UINT32),
	DB_ATTRIBUTE(QueryLimitsFlag, UINT32),			// a completely bogus attr; see spec
	DB_ATTRIBUTE(SampleTypes, MULTI_UINT32),
	DB_ATTRIBUTE(AclSubjectTypes, MULTI_UINT32),
	DB_ATTRIBUTE(AuthTags, MULTI_UINT32),
	DB_ATTRIBUTE(ConjunctiveOps, MULTI_UINT32),
	DB_ATTRIBUTE(RelationalOps, MULTI_UINT32),
};
static const CSSM_DB_INDEX_INFO dlPrimaryIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32)
};

// DL Encapsulated Products Relation.
static const CSSM_DB_ATTRIBUTE_INFO dlEncapsulatedAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(ProductDesc, STRING),
	DB_ATTRIBUTE(ProductVendor, STRING),
	DB_ATTRIBUTE(ProductVersion, STRING),
	DB_ATTRIBUTE(ProductFlags, UINT32),
	DB_ATTRIBUTE(StandardDesc, STRING),
	DB_ATTRIBUTE(StandardVersion, STRING),
	DB_ATTRIBUTE(Protocol, UINT32),
	DB_ATTRIBUTE(RetrievalMode, UINT32),
};

static const CSSM_DB_INDEX_INFO dlEncapsulatedIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32)
};

// CL Primary Relation.
static const CSSM_DB_ATTRIBUTE_INFO clPrimaryAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(Manifest, BLOB),
	DB_ATTRIBUTE(ModuleName, STRING),
	DB_ATTRIBUTE(ProductVersion, STRING),
	DB_ATTRIBUTE(Vendor, STRING),
	DB_ATTRIBUTE(CertTypeFormat, UINT32),
	DB_ATTRIBUTE(CrlTypeFormat, UINT32),
	DB_ATTRIBUTE(CertFieldNames, BLOB),
	DB_ATTRIBUTE(BundleTypeFormat, MULTI_UINT32),
	DB_ATTRIBUTE(XlationTypeFormat, MULTI_UINT32),
	DB_ATTRIBUTE(TemplateFieldNames, BLOB),
};

static const CSSM_DB_INDEX_INFO clPrimaryIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32)
};

// CL Encapsulated Products Relation.
static const CSSM_DB_ATTRIBUTE_INFO clEncapsulatedAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(ProductDesc, STRING),
	DB_ATTRIBUTE(ProductVendor, STRING),
	DB_ATTRIBUTE(ProductVersion, STRING),
	DB_ATTRIBUTE(ProductFlags, UINT32),
	DB_ATTRIBUTE(StandardDesc, STRING),
	DB_ATTRIBUTE(StandardVersion, STRING),
};

static const CSSM_DB_INDEX_INFO clEncapsulatedIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32)
};

// TP Primary Relation.
static const CSSM_DB_ATTRIBUTE_INFO tpPrimaryAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(Manifest, BLOB),
	DB_ATTRIBUTE(ModuleName, STRING),
	DB_ATTRIBUTE(ProductVersion, STRING),
	DB_ATTRIBUTE(Vendor, STRING),
	DB_ATTRIBUTE(CertTypeFormat, UINT32),
	DB_ATTRIBUTE(SampleTypes, MULTI_UINT32),
	DB_ATTRIBUTE(AclSubjectTypes, MULTI_UINT32),
	DB_ATTRIBUTE(AuthTags, MULTI_UINT32),
};

static const CSSM_DB_INDEX_INFO tpPrimaryIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32)
};

// TP Policy-OIDs Relation.
static const CSSM_DB_ATTRIBUTE_INFO tpPolicyOidsAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(OID, BLOB),
	DB_ATTRIBUTE(Value, BLOB),
};

static const CSSM_DB_INDEX_INFO tpPolicyOidsIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32),
	UNIQUE_INDEX_ATTRIBUTE(OID, BLOB)
};

// special case "subschema" for parsing tpPolicyOidsAttrs. These arrays correspond
// dictionaries within a tpPolicyOidsAttrs info file; they are not part of 
// our DB's schema. They are declared only to streamline the 
// MDSAttrParser::parseTpPolicyOidsRecord function. No index info is needed.

// top-level info, applied to the dictionary for the whole file.
static const CSSM_DB_ATTRIBUTE_INFO tpPolicyOidsDict1[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
};
const RelationInfo TpPolicyOidsDict1RelInfo = 
	RELATION_INFO(
		MDS_CDSADIR_TP_OIDS_RECORDTYPE,				// actually a don't care
		tpPolicyOidsDict1,
		NULL);										// no index

// One element of the "Policies" array maps to one of these.
static const CSSM_DB_ATTRIBUTE_INFO tpPolicyOidsDict2[] =
{
	DB_ATTRIBUTE(OID, BLOB),
	DB_ATTRIBUTE(Value, BLOB),
};
const RelationInfo TpPolicyOidsDict2RelInfo = 
	RELATION_INFO(
		MDS_CDSADIR_TP_OIDS_RECORDTYPE,				// actually a don't care
		tpPolicyOidsDict2,
		NULL);										// no index

// TP Encapsulated Products Relation.
static const CSSM_DB_ATTRIBUTE_INFO tpEncapsulatedAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(ProductDesc, STRING),
	DB_ATTRIBUTE(ProductVendor, STRING),
	DB_ATTRIBUTE(ProductVersion, STRING),
	DB_ATTRIBUTE(ProductFlags, UINT32),				// vendor-specific, right?
	DB_ATTRIBUTE(AuthorityRequestType, MULTI_UINT32),
	DB_ATTRIBUTE(StandardDesc, STRING),
	DB_ATTRIBUTE(StandardVersion, STRING),
	DB_ATTRIBUTE(ProtocolDesc, STRING),
	DB_ATTRIBUTE(ProtocolFlags, UINT32),
	DB_ATTRIBUTE(CertClassName, STRING),
	DB_ATTRIBUTE(RootCertificate, BLOB),
	DB_ATTRIBUTE(RootCertTypeFormat, UINT32),
};
static const CSSM_DB_INDEX_INFO tpEncapsulatedIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32)
};

#if 	DEFINE_META_TABLES
// MDS Schema Relations (meta) Relation.
static const CSSM_DB_ATTRIBUTE_INFO mdsSchemaRelationsAttrs[] =
{
	DB_ATTRIBUTE(RelationID, UINT32),
	DB_ATTRIBUTE(RelationName, STRING),
};

static const CSSM_DB_INDEX_INFO mdsSchemaRelationsIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(RelationID, UINT32),
};

// MDS Schema Attributes (meta) Relation.
static const CSSM_DB_ATTRIBUTE_INFO mdsSchemaAttributesAttrs[] =
{
	DB_ATTRIBUTE(RelationID, UINT32),
	DB_ATTRIBUTE(AttributeID, UINT32),
	DB_ATTRIBUTE(AttributeNameFormat, UINT32),
	DB_ATTRIBUTE(AttributeName, STRING),
	DB_ATTRIBUTE(AttributeNameID, BLOB),
	DB_ATTRIBUTE(AttributeFormat, UINT32),
};

static const CSSM_DB_INDEX_INFO mdsSchemaAttributesIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(RelationID, UINT32),
	UNIQUE_INDEX_ATTRIBUTE(AttributeID, UINT32)
};

// MDS Schema Indexes (meta) Relation.
static const CSSM_DB_ATTRIBUTE_INFO mdsSchemaIndexesAttrs[] =
{
	DB_ATTRIBUTE(RelationID, UINT32),
	DB_ATTRIBUTE(IndexID, UINT32),
	DB_ATTRIBUTE(AttributeID, UINT32),
	DB_ATTRIBUTE(IndexType, UINT32),
	DB_ATTRIBUTE(IndexedDataLocation, UINT32),
};

static const CSSM_DB_INDEX_INFO mdsSchemaIndexesIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(RelationID, UINT32),
	UNIQUE_INDEX_ATTRIBUTE(IndexID, UINT32)
};

#endif	/* DEFINE_META_TABLES */

// AC Primary Relation.
static const CSSM_DB_ATTRIBUTE_INFO acPrimaryAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(Manifest, BLOB),
	DB_ATTRIBUTE(ModuleName, STRING),
	DB_ATTRIBUTE(ProductVersion, STRING),
	DB_ATTRIBUTE(Vendor, STRING),
};

static const CSSM_DB_INDEX_INFO acPrimaryIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32)
};

// KR Primary Relation.
static const CSSM_DB_ATTRIBUTE_INFO krPrimaryAttrs[] =
{
	DB_ATTRIBUTE(ModuleID, STRING),
	DB_ATTRIBUTE(SSID, UINT32),
	DB_ATTRIBUTE(Manifest, BLOB),
	DB_ATTRIBUTE(ModuleName, STRING),
	DB_ATTRIBUTE(CompatCSSMVersion, STRING),
	DB_ATTRIBUTE(Version, STRING),
	DB_ATTRIBUTE(Vendor, STRING),
	DB_ATTRIBUTE(Description, STRING),
	DB_ATTRIBUTE(ConfigFileLocation, STRING),
};

static const CSSM_DB_INDEX_INFO krPrimaryIndex[] =
{
	UNIQUE_INDEX_ATTRIBUTE(ModuleID, STRING),
	UNIQUE_INDEX_ATTRIBUTE(SSID, UINT32)
};

// list of all built-in schema for the CDSA Directory DB.
const RelationInfo kMDSRelationInfo[] = 
{
	RELATION_INFO(MDS_CDSADIR_CSSM_RECORDTYPE, 
		cssmAttrs, 
		cssmIndex),
	RELATION_INFO(MDS_CDSADIR_KRMM_RECORDTYPE, 
		krmmAttrs, 
		krmmIndex),
	RELATION_INFO(MDS_CDSADIR_EMM_RECORDTYPE, 
		emmAttrs, 
		emmIndex),
	RELATION_INFO(MDS_CDSADIR_EMM_PRIMARY_RECORDTYPE, 
		emmPrimaryAttrs, 
		emmPrimaryIndex),
	RELATION_INFO(MDS_CDSADIR_COMMON_RECORDTYPE, 
		commonAttrs, 
		commonIndex),
	RELATION_INFO(MDS_CDSADIR_CSP_PRIMARY_RECORDTYPE, 
		cspPrimaryAttrs, 
		cspPrimaryIndex),
	RELATION_INFO(MDS_CDSADIR_CSP_CAPABILITY_RECORDTYPE, 
		cspCapabilitiesAttrs, 
		cspCapabilitiesIndex),
	RELATION_INFO(MDS_CDSADIR_CSP_ENCAPSULATED_PRODUCT_RECORDTYPE, 
		cspEncapsulatedAttrs, 
		cspEncapsulatedIndex),
	RELATION_INFO(MDS_CDSADIR_CSP_SC_INFO_RECORDTYPE, 
		cspSmartCardAttrs, 
		cspSmartCardIndex),
	RELATION_INFO(MDS_CDSADIR_DL_PRIMARY_RECORDTYPE, 
		dlPrimaryAttrs, 
		dlPrimaryIndex),
	RELATION_INFO(MDS_CDSADIR_DL_ENCAPSULATED_PRODUCT_RECORDTYPE, 
		dlEncapsulatedAttrs, 
		dlEncapsulatedIndex),
	RELATION_INFO(MDS_CDSADIR_CL_PRIMARY_RECORDTYPE, 
		clPrimaryAttrs, 
		clPrimaryIndex),
	RELATION_INFO(MDS_CDSADIR_CL_ENCAPSULATED_PRODUCT_RECORDTYPE, 
		clEncapsulatedAttrs, 
		clEncapsulatedIndex),
	RELATION_INFO(MDS_CDSADIR_TP_PRIMARY_RECORDTYPE, 
		tpPrimaryAttrs, 
		tpPrimaryIndex),
	RELATION_INFO(MDS_CDSADIR_TP_OIDS_RECORDTYPE, 
		tpPolicyOidsAttrs, 
		tpPolicyOidsIndex),
	RELATION_INFO(MDS_CDSADIR_TP_ENCAPSULATED_PRODUCT_RECORDTYPE, 
		tpEncapsulatedAttrs, 
		tpEncapsulatedIndex),
	#if	DEFINE_META_TABLES
	RELATION_INFO(MDS_CDSADIR_MDS_SCHEMA_RELATIONS, 
		mdsSchemaRelationsAttrs, 
		mdsSchemaRelationsIndex),
	RELATION_INFO(MDS_CDSADIR_MDS_SCHEMA_ATTRIBUTES, 
		mdsSchemaAttributesAttrs, 
		mdsSchemaAttributesIndex),
	RELATION_INFO(MDS_CDSADIR_MDS_SCHEMA_INDEXES, 
		mdsSchemaIndexesAttrs, 
		mdsSchemaIndexesIndex),
	#endif	/* DEFINE_META_TABLES */
	RELATION_INFO(MDS_CDSADIR_AC_PRIMARY_RECORDTYPE, 
		acPrimaryAttrs, 
		acPrimaryIndex),
	RELATION_INFO(MDS_CDSADIR_KR_PRIMARY_RECORDTYPE, 
		krPrimaryAttrs, 
		krPrimaryIndex)
};

const unsigned kNumMdsRelations = sizeof(kMDSRelationInfo) / sizeof(RelationInfo);

// Map a CSSM_DB_RECORDTYPE to a RelationInfo *.
extern const RelationInfo *MDSRecordTypeToRelation(
	CSSM_DB_RECORDTYPE recordType)
{
	const RelationInfo *relInfo = kMDSRelationInfo;
	unsigned dex;
	
	for(dex=0; dex<kNumMdsRelations; dex++) {
		if(relInfo->DataRecordType == recordType) {
			return relInfo;
		}
		relInfo++;
	}
	if(recordType == MDS_OBJECT_RECORDTYPE) {
		return &kObjectRelation;
	}
	return NULL;
}

// same as above, based on record type as string. 
extern const RelationInfo *MDSRecordTypeNameToRelation(
	const char *recordTypeName)
{
	const RelationInfo *relInfo = kMDSRelationInfo;
	unsigned dex;
	
	for(dex=0; dex<kNumMdsRelations; dex++) {
		if(!strcmp(recordTypeName, relInfo->relationName)) {
			return relInfo;
		}
		relInfo++;
	}
	return NULL;
}

