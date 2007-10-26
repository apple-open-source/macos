/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include "CDSLocalConfigureNode.h"

CDSLocalConfigureNode::CDSLocalConfigureNode( CFStringRef inNodeName, uid_t inUID, uid_t inEffectiveUID ) 
	: BDPIVirtualNode( inNodeName, inUID, inEffectiveUID )
{
}

CDSLocalConfigureNode::~CDSLocalConfigureNode( void )
{
}

CFMutableDictionaryRef CDSLocalConfigureNode::CopyNodeInfo( CFArrayRef inAttributes )
{
	CFMutableDictionaryRef	cfNodeInfo		= CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
																		&kCFTypeDictionaryValueCallBacks );
	CFMutableDictionaryRef	cfAttributes	= CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
																		&kCFTypeDictionaryValueCallBacks );
	CFRange					cfAttribRange	= CFRangeMake( 0, CFArrayGetCount(inAttributes) );
	bool					bNeedAll		= CFArrayContainsValue( inAttributes, cfAttribRange, CFSTR(kDSAttributesAll) );
	
	CFDictionarySetValue( cfNodeInfo, kBDPINameKey, CFSTR("DirectoryNodeInfo") );
	CFDictionarySetValue( cfNodeInfo, kBDPITypeKey, CFSTR(kDSStdRecordTypeDirectoryNodeInfo) );
	CFDictionarySetValue( cfNodeInfo, kBDPIAttributeKey, cfAttributes );
	
	if (bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDSNAttrNodePath)))
	{
		CFArrayRef cfNodePath = CFStringCreateArrayBySeparatingStrings( kCFAllocatorDefault, fNodeName, CFSTR("/") );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDSNAttrNodePath), cfNodePath );
		
		DSCFRelease( cfNodePath );
	}
	
	if (bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDS1AttrReadOnlyNode)))
	{
		CFStringRef	cfReadOnly	= CFSTR("ReadOnly");
		CFArrayRef	cfValue		= CFArrayCreate( kCFAllocatorDefault, (const void **) &cfReadOnly, 1, &kCFTypeArrayCallBacks );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDS1AttrReadOnlyNode), cfValue );
		
		DSCFRelease( cfValue );
	}
	
	if (bNeedAll || CFArrayContainsValue(inAttributes, cfAttribRange, CFSTR(kDS1AttrDistinguishedName)))
	{
		CFStringRef	cfRealName	= CFSTR("Local Configure");
		CFArrayRef	cfValue		= CFArrayCreate( kCFAllocatorDefault, (const void **) &cfRealName, 1, &kCFTypeArrayCallBacks );
		
		CFDictionarySetValue( cfAttributes, CFSTR(kDS1AttrDistinguishedName), cfValue );
		
		DSCFRelease( cfValue );
	}
	
	DSCFRelease( cfAttributes );
	
	return cfNodeInfo;
}

tDirStatus CDSLocalConfigureNode::SearchRecords( sBDPISearchRecordsContext *inContext, BDPIOpaqueBuffer inBuffer, UInt32 *outCount)
{
	return eNotHandledByThisNode;
}
