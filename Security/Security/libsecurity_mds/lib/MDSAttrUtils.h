/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
   File:      MDSAttrUtils.h

   Contains:  Stateless functions used by MDSAttrParser.  

   Copyright (c) 2001,2011,2014 Apple Inc. All Rights Reserved.
*/

#ifndef _MDS_ATTR_UTILS_H_
#define _MDS_ATTR_UTILS_H_  1

#include <Security/cssmtype.h>
#include <security_utilities/debugging.h>
#include <CoreFoundation/CoreFoundation.h>
#include "MDSAttrStrings.h"
#include "MDSSession.h"

/* log parsing events */
#define MPDebug(args...)	secdebug("MDS_Parse", ## args)

/* log scanning events */
#define MSDebug(args...)	secdebug("MDS_Scan", ## args)

/*
 * I can't believe that CFRelease does not do this...
 */
#define CF_RELEASE(c)	if(c != NULL) { CFRelease(c); c = NULL; }

namespace Security
{

/*
 * Fill in one CSSM_DB_ATTRIBUTE_DATA with specified data, type and attribute name.
 * CSSM_DB_ATTRIBUTE_DATA.Value and its referent are new[]'d and copied.
 * Assumes:
 *   -- AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING
 */
extern void MDSRawValueToDbAttr(
	const void *value,
	size_t len,
	CSSM_DB_ATTRIBUTE_FORMAT attrFormat,	// CSSM_DB_ATTRIBUTE_FORMAT_STRING, etc.
	const char *attrName,
	CSSM_DB_ATTRIBUTE_DATA &attr,
	uint32 numValues = 1);

/*
 * Free data new[]'d in the above function.
 */
extern void MDSFreeDbRecordAttrs(
	CSSM_DB_ATTRIBUTE_DATA 	*attrs,
	unsigned				numAttrs);


/* safely get a new[]'d C string from a CFString */
char *MDSCFStringToCString(
	CFStringRef cfStr);

/* copy a new[]'d C string from a C string */
char *MDSCopyCstring(
	const char *inStr);

/* 
 * Given a CFTypeRef which is either a CFString or a CFNumber, do our best to 
 * convert it to a uint32. If it's a CFString, we'll use a MDSNameValuePair
 * to convert it. CFStrings expressed as decimal numbers are also converted
 * properly. (MAYBE we'll convert hex strings too...TBD...)
 * Returns true if conversion was successful.
 */
bool MDSCfTypeToUInt32(
	CFTypeRef cfValue,
	const MDSNameValuePair *nameValues,	// optional for converting strings to numbers
	const char *key,					// for debug logging only 
	uint32 &value,						// RETURNED
    size_t &valueLen);					// RETURNED

/*
 * Insert a record, defined by a CSSM_DB_ATTRIBUTE_DATA array, into specified
 * DL and DB. Returns true on success.
 */
bool MDSInsertRecord(
	const CSSM_DB_ATTRIBUTE_DATA 	*inAttr,
	unsigned						numAttrs,
	CSSM_DB_RECORDTYPE				recordType,
	MDSSession						&dl,
	CSSM_DB_HANDLE					dbHand);

/*
 * Convert a number expressed as a CFString to a uint32 using the specified
 * name/value conversion table. The string may have multiple fields from that
 * table, ORd together in normal C syntax. Like
 *
 *      CSSM_SERVICE_CSP | CSSM_SERVICE_DL
 */
CSSM_RETURN MDSStringToUint32(
	CFStringRef str, 
	const MDSNameValuePair *table,
	uint32 &value);					// RETURNED


} // end namespace Security

#endif	/* _MDS_ATTR_UTILS_H_ */
