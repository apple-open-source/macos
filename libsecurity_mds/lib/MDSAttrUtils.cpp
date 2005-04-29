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


/*
   File:      MDSAttrUtils.cpp

   Contains:  Stateless functions used by MDSAttrParser.  

   Copyright: (c) 2001 Apple Computer, Inc., all rights reserved.
*/

#include "MDSAttrUtils.h"
#include <strings.h>

namespace Security
{

/*
 * Fill in one CSSM_DB_ATTRIBUTE_DATA with specified data, type and attribute name.
 * CSSM_DB_ATTRIBUTE_DATA.Value and its referent are new[]'d and copied.
 * Assumes:
 *   -- AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING
 *   -- NumberOfValues = 1  
 */
void MDSRawValueToDbAttr(
	const void *value,
	size_t len,
	CSSM_DB_ATTRIBUTE_FORMAT attrFormat,	// CSSM_DB_ATTRIBUTE_FORMAT_STRING, etc.
	const char *attrName,
	CSSM_DB_ATTRIBUTE_DATA &attr,
	uint32 numValues)
{
	CSSM_DB_ATTRIBUTE_INFO_PTR attrInfo = &attr.Info;
	attrInfo->AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attrInfo->Label.AttributeName = const_cast<char *>(attrName);
	attrInfo->AttributeFormat = attrFormat;
	attr.NumberOfValues = numValues;
	attr.Value = new CSSM_DATA[1];
	attr.Value->Data = new uint8[len];
	attr.Value->Length = len;
	memcpy(attr.Value->Data, value, len);
}


/*
 * Free data new[]'d in the above function.
 */
void MDSFreeDbRecordAttrs(
	CSSM_DB_ATTRIBUTE_DATA 	*attrs,
	unsigned				numAttrs)
{
	uint32 i;
	for(i=0; i<numAttrs; i++) {
		assert(attrs->Value != NULL);
		delete [] attrs->Value->Data;
		attrs->Value->Data = NULL;
		attrs->Value->Length = 0;
		delete [] attrs->Value;
		attrs->Value = NULL;
		attrs++;
	}
}

/* safely get a new[]'d C string from a CFString */
char *MDSCFStringToCString(
	CFStringRef cfStr)
{
	char *rtn = NULL;
	unsigned len = CFStringGetLength(cfStr) + 1;
	rtn = new char[len];
	if(rtn) {
		CFStringGetCString(cfStr, rtn, len, CFStringGetSystemEncoding());
	}
	return rtn;
}

/* copy a new[]'d C string from a C string */
char *MDSCopyCstring(
	const char *inStr)
{
	char *outStr = new char[::strlen(inStr) + 1];
	strcpy(outStr, inStr);
	return outStr;
}

/* 
 * Given a CFTypeRef which is either a CFString, a CFNumber, or a CFBoolean,
 * do our best to convert it to a uint32. If it's a CFString, we'll use a 
 * MDSNameValuePair to convert it. CFStrings expressed as decimal numbers 
 * are also converted properly. (MAYBE we'll convert hex strings too...TBD...)
 * Returns true if conversion was successful.
 */
bool MDSCfTypeToInt(
	CFTypeRef cfValue,
	const MDSNameValuePair *nameValues,	// optional for converting strings to numbers
	const char *key,					// for debug logging only 
	uint32 &iValue)						// RETURNED
{
	assert(cfValue != NULL);
	CFTypeID valueType = CFGetTypeID(cfValue);
	if(valueType == CFStringGetTypeID()) {
		CSSM_RETURN crtn = MDSStringToUint32((CFStringRef)cfValue, 
			nameValues, iValue);
		if(crtn) {
			MPDebug("cfTypeToInt: key %s uint32 form, string data (%s), "
				"bad conv", key, 
				CFStringGetCStringPtr((CFStringRef)cfValue, 
					CFStringGetSystemEncoding()));
			return false;
		} 
		return true;
	}	/* stored as string */
	else if(valueType == CFNumberGetTypeID()) {
		/* be paranoid - there is no unsigned type for CFNumber */
		CFNumberRef cfNum = (CFNumberRef)cfValue;
		CFNumberType numType = CFNumberGetType(cfNum);
		switch(numType) {
			case kCFNumberSInt8Type:
			case kCFNumberSInt16Type:
			case kCFNumberSInt32Type:
			case kCFNumberCharType:
			case kCFNumberShortType:
			case kCFNumberIntType:
			case kCFNumberLongType:
			case kCFNumberSInt64Type:	// apparently the default
				/* OK */
				break;
			default:
				MPDebug("MDS cfTypeToInt: Bad CFNumber type (%d) key %s", numType, key);
				return false;
		}
		Boolean brtn = CFNumberGetValue(cfNum, kCFNumberLongType, &iValue);
		if(!brtn) {
			MPDebug("MDS cfTypeToInt: Bad CFNumber conversion");
			return false;
		}
		return true;
	}	/* stored as number */
	else if(valueType == CFBooleanGetTypeID()) {
		Boolean b = CFBooleanGetValue((CFBooleanRef)cfValue);
		iValue = b ? 1 : 0;
		return true;
	}
	else {
		MPDebug("MDS cfTypeToInt: key %s, uint32 form, bad CF type (%d)", 
			key, (int)valueType);
		return false;
	}
}

/*
 * Insert a record, defined by a CSSM_DB_ATTRIBUTE_DATA array, into specified
 * DL and DB. Returns true on success.
 */
bool MDSInsertRecord(
	const CSSM_DB_ATTRIBUTE_DATA 	*inAttr,
	unsigned						numAttrs,
	CSSM_DB_RECORDTYPE				recordType,
	MDSSession						&dl,
	CSSM_DB_HANDLE					dbHand)
{
	CSSM_DB_RECORD_ATTRIBUTE_DATA 	recordAttrData;
	CSSM_DB_UNIQUE_RECORD_PTR		uid = NULL;
	bool							ourRtn = true;
	
	recordAttrData.DataRecordType = recordType;
	recordAttrData.SemanticInformation = 0;
	recordAttrData.NumberOfAttributes = numAttrs;
	recordAttrData.AttributeData = 
		const_cast<CSSM_DB_ATTRIBUTE_DATA_PTR>(inAttr);
	
	try {
		dl.DataInsert(dbHand,
			recordType,
			&recordAttrData,
			NULL,
			uid);
	}
	catch (const CssmError &cerr) {
		MPDebug("MDSInsertRecord: DataInsert: %ld", cerr.error);
		ourRtn = false;
	}
	catch(...) {
		MPDebug("MDSInsertRecord: DataInsert: unknown exception");
		ourRtn = false;
	}
	if(uid != NULL) {
		dl.FreeUniqueRecord(dbHand, *uid);
	}
	return ourRtn;
}

/*
 * Convert a number expressed as a CFString to a uint32 using the specified
 * name/value conversion table. The string may have multiple fields from that
 * table, ORd together in normal C syntax. Like
 *
 *      CSSM_SERVICE_CSP | CSSM_SERVICE_DL
 *
 * Individual tokens can also be expressed numerically, either in decimal or 
 * (if prefaced by "0x" hex. Numeric tokens and symbolic string tokens can
 * be intermixed in the same incoming string.
 *
 * Individual tokens can be prefixed with "<<" indicating that the indicated
 * value is to be shifted 16 bits. Cf. CL Primary Relation, {Cert,Crl}TypeFormat.
 * This applies to both numeric and string tokens. 
 */
CSSM_RETURN MDSStringToUint32(
	CFStringRef str, 
	const MDSNameValuePair *table,		// optional, string must be decimal
	uint32 &value)
{	
	char *cstr = MDSCFStringToCString(str);
	if(cstr == NULL) {
		/* should "never" happen...right? */
		MPDebug("MDSStringToUint32: CFString conversion error");
		return CSSMERR_CSSM_MDS_ERROR;
	}
	
	char tokenStr[200];
	char *src = cstr;
	char *dst = tokenStr;
	char c;
	CSSM_RETURN crtn = CSSM_OK;
	
	value = 0;
	while(*src != '\0') {
		/* Get one token from src --> tokenStr[] */ 
		/* First skip whitespace and '|' */
		for( ; *src != '\0'; src++) {
			c = *src;
			if(!isspace(c) && (c != '|')) {
				/* first char of token */
				*dst++ = c;
				src++;
				break;
			}
		}
		if((*src == '\0') && (dst == tokenStr)) {
			/* done */
			break;
		}
		
		/* dst[-1] is the first good character of token; copy until 
		 * space or '|' */
		for( ; *src != '\0'; src++) {
			c = *src;
			if(isspace(c) || (c == '|')) {
				break;
			}
			else {
				*dst++ = c;
			}
		}
		
		/* NULL terminate token string, convert to numeric value */
		*dst = '\0';
		uint32 tokenVal = 0;
		CSSM_RETURN crtn = MDSAttrNameToValue(tokenStr, table, tokenVal);
		if(crtn) {
			/* punt */
			break;
		}
		value |= tokenVal;
		
		/* restart */
		dst = tokenStr;
	}
	delete [] cstr;
	return crtn;
}

} // end namespace Security
