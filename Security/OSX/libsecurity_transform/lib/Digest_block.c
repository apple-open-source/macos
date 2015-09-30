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


#include "Digest_block.h"
#include "SecCustomTransform.h"
#include <CommonCrypto/CommonDigest.h>
#include "Utilities.h"

extern const CFStringRef kSecDigestMD2, kSecDigestMD4, kSecDigestMD5, kSecDigestSHA1, kSecDigestSHA2;
extern const CFStringRef kSecDigestTypeAttribute, kSecDigestLengthAttribute;


static CFStringRef kCustomDigestTransformName = CFSTR("com.apple.security.digest");

SecTransformRef SecDigestTransformCreate_block(SecProviderRef device, CFTypeRef digestType, CFIndex digestLength, CFErrorRef* error) {
	static dispatch_once_t inited;
	__block CFStringRef current_algo;
	__block CFIndex current_length;
	
	dispatch_once(&inited, ^{
		SecTransformRegister(kCustomDigestTransformName, ^(CFStringRef name, SecTransformRebindActionBlock rebind) {
			rebind(kSecTransformActionProcessData, NULL, ^(SecTransformRef tr, CFDataRef d, SecTransformSendAttribute set) {
				set(kSecDigestTypeAttribute, kSecDigestSHA2);
			});
			rebind(SecTransformSetAttributeAction, NULL, ^(SecTransformRef tr, CFStringRef name, CFTypeRef value, SecTransformSendAttribute set) {
				Boolean algo_changed = FALSE;
				if (name == kSecDigestTypeAttribute || CFStringCompare(kSecDigestTypeAttribute, name, 0) == kCFCompareEqualTo) {
					algo_changed = TRUE;
					current_algo = CFStringCreateCopy(NULL, (CFStringRef)value);

				} else if (name == kSecDigestLengthAttribute || CFStringCompare(kSecDigestLengthAttribute, name, 0) == kCFCompareEqualTo) {
					algo_changed = TRUE;
					CFNumberGetValue(value, kCFNumberCFIndexType, &current_length);
				} else if (CFStringCompare(kSecTransformInputAttributeName, name, 0) == kCFCompareEqualTo) {
					return (CFTypeRef)CreateSecTransformErrorRef(kSecTransformErrorInvalidType, "The type %@ is invalid.", name);
				} else {
					return (CFTypeRef)CreateSecTransformErrorRef(kSecTransformErrorInvalidType, "The type %@ is invalid.", name);
				}
				if (!algo_changed) {
					return value;
				}
				
				if (current_algo == kSecDigestSHA2 || CFStringCompare(current_algo, kSecDigestSHA2, 0) == kCFCompareEqualTo) {
					switch (current_length) {
						case 0:
						case 512: {
							__block CC_SHA512_CTX cc_context;
							CC_SHA512_Init(&cc_context);
							rebind(kSecTransformActionProcessData, NULL, ^(SecTransformRef tr, CFDataRef d, SecTransformSendAttribute set) {
								if (d) {
									CC_SHA512_Update(&cc_context, CFDataGetBytePtr(d), CFDataGetLength(d));
									return SecTransformNoData();
								} else {
									u_int8_t digest_buffer[CC_SHA512_DIGEST_LENGTH];

									CC_SHA512_Final(digest_buffer, &cc_context);
									set(kSecTransformOutputAttributeName, CFDataCreate(NULL, digest_buffer, CC_SHA512_DIGEST_LENGTH));
									return NULL;
								}
							});
							return value;
						}
					}
					return (CFTypeRef)CreateSecTransformErrorRef(kSecTransformErrorInvalidLength, "Invalid length.");
				} else {
					return (CFTypeRef)CreateSecTransformErrorRef(kSecTransformErrorInvalidAlgorithm, "Invalid algorithm.");
				}
			});
		}, NULL);
	});
	
	SecTransformRef dt = SecTransformCreate(kCustomDigestTransformName, NULL);
	SecTransformSetAttribute(dt, kSecDigestTypeAttribute, digestType, error);
	CFNumberRef dlen = CFNumberCreate(NULL, kCFNumberCFIndexType, &digestLength);
	SecTransformSetAttribute(dt, kSecDigestLengthAttribute, dlen, error);
	CFRelease(dlen);
	
	return dt;
}
