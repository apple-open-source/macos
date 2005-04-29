/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
 *  HighLevelDirServices.h
 *  DirTesting
 *
 *  Created by fhill on Mon Nov 05 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _HIGHLEVELDIRSERVICES_H_
#define _HIGHLEVELDIRSERVICES_H_

#include <DirectoryService/DirServicesTypes.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDictionary.h>
#include <stdarg.h>

#define kHLDSEmptyAttributeValueForRequiredAttributes	"99"
#define kFabricatedUserGUIDPrefix						"FFFFEEEE-DDDD-CCCC-BBBB-AAAA"
#define kFabricatedGroupGUIDPrefix						"AAAABBBB-CCCC-DDDD-EEEE-FFFF"

#ifdef __cplusplus
extern "C"
{
#endif

//getting attributes
tDirStatus				HLDSGetAttributeValuesFromRecord( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, const char* inDSRecordType,
							const char* inRecordName, CFArrayRef inAttributesToGet, CFDictionaryRef* outAttributeValues );
tDirStatus				HLDSGetAttributeValuesFromRecordsByName( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, const char* inDSRecordType,
							CFArrayRef inRecordNames, CFArrayRef inAttributesToGet, CFArrayRef* outAttributeValues );
tDirStatus				HLDSGetAttributeValuesFromRecordsByAttributeValue( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, const char* inDSRecordType,
							const char* inAttributeToSearchOn, CFStringRef inAttributeValueToSearchFor, tDirPatternMatch inPatternMatch, CFArrayRef inAttributesToGet,
							CFArrayRef* outAttributeValues );
tDirStatus				HLDSGetAttributeValuesFromRecordsByAttributeValues( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, const char* inDSRecordType,
							const char* inAttributeToSearchOn, CFArrayRef inAttributeValuesToSearchFor, tDirPatternMatch inPatternMatch, CFArrayRef inAttributesToGet,
							CFArrayRef* outRecsAttributesValues );
//setting attributes
tDirStatus				HLDSSetAttributeValue( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
							char createIfNecessary, const char* inAttributeValue );
tDirStatus				HLDSSetAttributeValues( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
							char createIfNecessary, const char** inAttributeValues, unsigned long inNumValues );
tDirStatus				HLDSSetAttributeCFValues( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
							char createIfNecessary, CFArrayRef inAttributeValues );
tDirStatus				HLDSSetBinaryAttributeValue( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
							dsBool createIfNecessary, CFDataRef inAttributeValue );
tDirStatus				HLDSSetBinaryAttributeValues( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
							dsBool createIfNecessary, CFArrayRef inAttributeValues );
tDirStatus				HLDSAddAttribute( const tDirReference inDirRef, tRecordReference inRecordRef,
							const char* inAttributeName, const char* inAttributeValue );
tDirStatus				HLDSAddAttributeValue( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
							char createIfNecessary, const char* inAttributeValue );
tDirStatus				HLDSRemoveAttribute( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName );
tDirStatus				HLDSRemoveAttributeValue( const tDirReference inDirRef, tRecordReference inRecordRef, const char* inAttributeName,
							const char* inAttributeValue );

//fabricating GUIDs
CFStringRef				HLDSCreateFabricatedGUID( int inID, CFStringRef inRecordType );
int						HLDSGetIDFromFabricatedGUID( CFStringRef theGUID );

//legacy group detection
tDirStatus				HLDSIsLegacyGroup( const tDirReference inDirRef, const tDirNodeReference inDirNodeRef, char* inGroupName, dsBool* outIsLegacy, CFArrayRef* outShortNameMembers );

#ifdef __cplusplus
}
#endif

#endif
