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
