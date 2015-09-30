/*
 * Copyright (c) 2010-2011,2014 Apple Inc. All Rights Reserved.
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


#include "SecTransform.h"
#include "SecCustomTransform.h"
#include "SecExternalSourceTransform.h"
#include <dispatch/dispatch.h>

CFStringRef external_source_name = CFSTR("com.apple.security.external_source");

static SecTransformInstanceBlock SecExternalSourceTransformCreateBlock(CFStringRef name, SecTransformRef newTransform, SecTransformImplementationRef ref)
{
	return Block_copy(^{ 
		SecTransformCustomSetAttribute(ref, kSecTransformInputAttributeName, kSecTransformMetaAttributeRequired, kCFBooleanFalse);
		
		SecTransformAttributeRef out = SecTranformCustomGetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeRef);
		
		SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecTransformInputAttributeName, ^(SecTransformAttributeRef attribute, CFTypeRef value) {
			SecTransformCustomSetAttribute(ref, out, kSecTransformMetaAttributeValue, value);
			return (CFTypeRef)NULL;
		});
		
		return (CFErrorRef)NULL;
	});
}

SecTransformRef SecExternalSourceTransformCreate(CFErrorRef* error)
{
	static dispatch_once_t once;
	dispatch_once(&once, ^{
		SecTransformRegister(external_source_name, SecExternalSourceTransformCreateBlock, error);
	});
	
	return SecTransformCreate(external_source_name, error);
}
