/* Copyright (c) 2002-2003,2006 Apple Computer, Inc.
 *
 * dbAttrs.h - Apple DL/DB/Keychain attributes and name/value pairs
 */

#ifndef	_DB_ATTRS_H_
#define _DB_ATTRS_H_

#include <Security/cssmtype.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* entry in a table to map a uint32 to a string */
typedef struct {
	uint32			value;
	const char 		*name;
} NameValuePair;

/* all the info we need about one Relation (schema) */
typedef struct  {
	CSSM_DB_RECORDTYPE DataRecordType;
	const char *relationName;
	uint32 NumberOfAttributes;
	const CSSM_DB_ATTRIBUTE_INFO *AttributeInfo;
	const NameValuePair **nameValues;
} RelationInfo;

extern const NameValuePair recordTypeNames[];

extern const RelationInfo schemaInfoRelation;
extern const RelationInfo allKeysRelation;
extern const RelationInfo anyRecordRelation;
extern const RelationInfo genericKcRelation;
extern const RelationInfo certRecordRelation;
extern const RelationInfo x509CertRecordRelation;
extern const RelationInfo x509CrlRecordRelation;
extern const RelationInfo userTrustRelation;
extern const RelationInfo referralRecordRelation;
extern const RelationInfo extendedAttrRelation;

/*
 * DBBlob record type, private to CSPDL.
 */
#define DBBlobRelationID	(CSSM_DB_RECORDTYPE_APP_DEFINED_START + 0x8000)

#ifdef	__cplusplus
}
#endif

#endif	/* _DB_ATTRS_H_ */
