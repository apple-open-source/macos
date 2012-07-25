/*
 *  SecExternalSourceTransform.cpp
 *  libsecurity_transform
 *
 *  Created by J Osborne on 8/17/10.
 *  Copyright 2010 Apple. All rights reserved.
 *
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