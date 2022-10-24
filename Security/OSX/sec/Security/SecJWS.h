/*
 * Copyright (c) 2020, 2022 Apple Inc. All Rights Reserved.
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
   SecJWS.h
*/

#import <Foundation/Foundation.h>
#import <Security/Security.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString *const SecJWSErrorDomain;

typedef NS_ENUM(NSInteger, SecJWSError) {
    SecJWSErrorInvalidCompactEncoding,            /// Not compact encoding
    SecJWSErrorCompactEncodingParseError,         /// Did not have all 3-tuple compact encoding segments
    SecJWSErrorCompactEncodingClaimError,         /// The Claim body is invalid or unparseable for this specification
    SecJWSErrorCompactEncodingPayloadParseError,  /// Could not parse the BASE64*URL encoding of the payload
    SecJWSErrorCompactEncodingHeaderParseError,   /// Could not parse the BASE64*URL encoding of the header
    SecJWSErrorHeaderFormatParseError,            /// Could not parse the JSON encoding of the header
    SecJWSErrorHeaderIncorrectKeyError,           /// Header contains an incorrect set of keys
    SecJWSErrorHeaderIncorrectAlgorithmError,     /// Header specifies an incorrect signature algorithm
    SecJWSErrorHeaderInvalidKeyIDError,           /// Header specifies a key ID that does not match one from the public DB
    SecJWSErrorHeaderInvalidValueError,           /// Invalid value for an unspecified header
    SecJWSErrorInvalidPublicKey,                  /// The public key is invalid (unable to parse/decode, wrong type, etc.)
    SecJWSErrorSignatureFormatError,              /// The signature has an incorrect format
    SecJWSErrorSignatureVerificationError,        /// Verification of the signature failed
};

@interface SecJWSDecoder : NSObject
@property (nonatomic, readonly) NSString *keyID;
@property (nonatomic, readonly) NSData *payload;
@property (nonatomic, readonly) NSError *verificationError;   /// One of the SecJWSError errors
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithJWSCompactEncodedString:(NSString *)compactEncodedString keyID:(NSString *)keyID publicKey:(SecKeyRef)publicKeyRef NS_DESIGNATED_INITIALIZER;
- (NSData *) dataWithBase64URLEncodedString:(NSString *)base64URLEncodedString;
@end

@interface SecJWSEncoder : NSObject
@property (assign) SecKeyRef publicKey;
@property (assign) SecKeyRef privateKey;
- (instancetype) init;                          /// will generate default EC 256 key pair
- (instancetype) initWithPublicKey:(SecKeyRef)pubKey privateKey:(SecKeyRef)privKey;
- (NSString *) encodedJWSWithPayload:(NSDictionary * _Nullable)payload kid:(NSString * _Nullable)kid nonce:(NSString *)nonce url:(NSString *)url error:(NSError ** _Nullable)error;
- (NSDictionary * _Nullable) jwkPublicKey;
- (NSString *) base64URLEncodedStringRepresentationWithData:(NSData *)data;
- (NSString *) base64URLEncodedStringRepresentationWithDictionary:(NSDictionary *)dictionary;
- (NSString *) compactJSONStringRepresentationWithDictionary:(NSDictionary *)dictionary;
@end

NS_ASSUME_NONNULL_END
