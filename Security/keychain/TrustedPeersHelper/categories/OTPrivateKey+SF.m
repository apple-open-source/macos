/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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


#import "OTPrivateKey+SF.h"
#import <SecurityFoundation/SFKey_Private.h>
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTConstants.h"
#import "utilities/SecCFWrappers.h"

@implementation OTPrivateKey (SecurityFoundation)

+ (instancetype)fromECKeyPair:(SFECKeyPair *)keyPair
{
    OTPrivateKey *pk = [OTPrivateKey new];
    pk.keyType = OTPrivateKey_KeyType_EC_NIST_CURVES;
    pk.keyData = keyPair.keyData;
    return pk;
}

+ (SecKeyRef) createSecKey:(NSData*)keyData CF_RETURNS_RETAINED
{
    NSDictionary *keyAttributes = @{
                                    (__bridge id)kSecAttrKeyClass : (__bridge id)kSecAttrKeyClassPrivate,
                                    (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeEC,
                                    };

    SecKeyRef key = SecKeyCreateWithData((__bridge CFDataRef)keyData, (__bridge CFDictionaryRef)keyAttributes, NULL);
    return key;
}

- (SFECKeyPair * _Nullable)asECKeyPair:(NSError**)error
{
    if (self.keyType != OTPrivateKey_KeyType_EC_NIST_CURVES) {
        if(error){
            *error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorNotSupported userInfo:nil];
        }
        return nil;
    }
    SecKeyRef secKey = [OTPrivateKey createSecKey:self.keyData];
    SFECKeyPair *result = [[SFECKeyPair alloc] initWithSecKey:secKey];
    CFReleaseNull(secKey);
    return result;
}

@end

