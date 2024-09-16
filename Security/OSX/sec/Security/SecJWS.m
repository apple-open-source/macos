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
   SecJWS.m
*/

#import "SecJWS.h"
#import <Security/oids.h>
#import <Security/SecBasePriv.h>
#import <Security/SecKeyPriv.h>
#import <utilities/SecCFWrappers.h>
#include <libDER/libDER.h>
#include <libDER/asn1Types.h>
#include <libDER/DER_Decode.h>

NSString *const SecJWSErrorDomain = @"com.apple.security.errors.jws";

static NSString *const kSecJWSHeaderKeyAlgorithm = @"alg";
static NSString *const kSecJWSHeaderKeyKeyID = @"kid";
static NSString *const kSecJWSHeaderKeyType = @"typ";
static NSString *const kSecJWSHeaderKeyContentType = @"cty";
static NSString *const kSecJWSHeaderValueES256Algorithm = @"ES256";
static NSString *const kSecJWSHeaderValueEdDSAAlgorithm = @"EdDSA";
static NSString *const kSecJWSHeaderValueType = @"JOSE";
static NSString *const kSecJWSHeaderValueContentType = @"JSON";

/*
 ECDSA-Sig-Value ::= SEQUENCE {
   r  INTEGER,
   s  INTEGER
 }
 */
typedef struct {
    DERItem r;
    DERItem s;
} DER_ECDSASig;

const DERItemSpec DER_ECDSASigItemSpecs[] = {
    { DER_OFFSET(DER_ECDSASig, r),
        ASN1_INTEGER,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DER_ECDSASig, s),
        ASN1_INTEGER,
        DER_DEC_NO_OPTS },
};

const DERSize DERNumECDSASigItemSpecs =
    sizeof(DER_ECDSASigItemSpecs) / sizeof(DERItemSpec);

@implementation SecJWSDecoder

- (instancetype)initWithJWSCompactEncodedString:(NSString *)compactEncodedString keyID:(NSString *)keyID publicKey:(SecKeyRef)publicKeyRef
{
    if (self = [super init]) {
        _keyID = keyID;

        if ([compactEncodedString isKindOfClass:[NSString class]]) {
            NSArray<NSString *> *components = [compactEncodedString componentsSeparatedByString:@"."];
            if (components.count == 3) {
                if ([self _validateJWSProtectedHeader:components[0]]) {
                    NSString *payloadBase64Url = components[1];
                    _payload = [self dataWithBase64URLEncodedString:payloadBase64Url];
                    _signature = [self dataWithBase64URLEncodedString:components[2]];

                    if (!_payload || !_signature) {
                        _verificationError = [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorCompactEncodingPayloadParseError userInfo:nil];
                    } else {
                        // Validate the signature
                        SecKeyRef publicKey = publicKeyRef;
                        CFRetainSafe(publicKey);
                        if (publicKey) {
                            [self _validateJWSSignature:_signature ofHeader:components[0] andPayload:components[1] withPublicKey:publicKey];
                            CFRelease(publicKey);
                        } else {
                            _verificationError = [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorInvalidPublicKey userInfo:nil];
                        }
                    }
                }
            } else {
                _verificationError = [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorCompactEncodingParseError userInfo:nil];
            }
        } else {
            _verificationError = [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorInvalidCompactEncoding userInfo:nil];
        }
    }
    return self;
}

- (BOOL)_validateJWSProtectedHeader:(NSString * _Nonnull)headerBase64URL
{
    // Decode Base64URL encoding
    NSData *header = [self dataWithBase64URLEncodedString:headerBase64URL];
    if (!header) {
        _verificationError = [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorCompactEncodingHeaderParseError userInfo:nil];
        return false;
    }

    // Convert JSON to NSDictionary
    NSError *error = nil;
    NSObject *headerObject = [NSJSONSerialization JSONObjectWithData:header options:(NSJSONReadingOptions)0 error:&error];
    if (error || ![headerObject isKindOfClass:[NSDictionary class]]) {
        _verificationError = error ?: [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorHeaderFormatParseError userInfo:nil];
        return false;
    }
    NSDictionary *headerDict = (NSDictionary *)headerObject;

    // Inspect dictionary for correct keys
    // %%% note: keyID is optional
    if ((headerDict.count < 1) ||
        ![headerDict[kSecJWSHeaderKeyAlgorithm] isKindOfClass:[NSString class]] ||
        (headerDict[kSecJWSHeaderKeyKeyID] &&
         ![headerDict[kSecJWSHeaderKeyKeyID] isKindOfClass:[NSString class]])) {
        _verificationError = [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorHeaderIncorrectKeyError userInfo:nil];
        return false;
    }

    // Validate the algorithm (only ES256 currently)
    NSString *algorithm = headerDict[kSecJWSHeaderKeyAlgorithm];
    if (![algorithm isEqualToString:kSecJWSHeaderValueES256Algorithm]) {
        _verificationError = [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorHeaderIncorrectAlgorithmError userInfo:nil];
        return false;
    }

    // Validate that the keyID matches the provided value
    NSString *keyID = headerDict[kSecJWSHeaderKeyKeyID];
    if (keyID && ![self.keyID isEqualToString:keyID]) {
        _verificationError = [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorHeaderInvalidKeyIDError userInfo:nil];
        return false;
    }

    // Validate optional keys (type and content type)
    NSString *typeValue = headerDict[kSecJWSHeaderKeyType];
    if (typeValue && [typeValue caseInsensitiveCompare:kSecJWSHeaderValueType] != NSOrderedSame) {
        _verificationError = [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorHeaderInvalidValueError userInfo:nil];
    }

    NSString *contentTypeValue = headerDict[kSecJWSHeaderKeyContentType];
    if (contentTypeValue && [contentTypeValue caseInsensitiveCompare:kSecJWSHeaderValueContentType] != NSOrderedSame) {
        _verificationError = [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorHeaderInvalidValueError userInfo:nil];
    }

    return true;
}

- (void)_validateJWSSignature:(NSData * _Nonnull)jwsSignature ofHeader:(NSString * _Nonnull)header andPayload:(NSString * _Nonnull)payload withPublicKey:(SecKeyRef _Nonnull)key
{
    // Compose a message from header and payload
    NSData *message = [[NSString stringWithFormat:@"%@.%@", header, payload] dataUsingEncoding:NSUTF8StringEncoding];

    // Transform the JWS Signature into ASN.1
    NSData *signature = [self _createASN1SignatureFromJWSSignature:jwsSignature];
    if (signature) {
        CFErrorRef error = nil;
        if (!SecKeyVerifySignature(key, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (__bridge CFDataRef)message, (__bridge CFDataRef)signature, &error) || error) {
            _verificationError = [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorSignatureVerificationError userInfo:nil];
        }
    } else {
        _verificationError = [NSError errorWithDomain:SecJWSErrorDomain code:SecJWSErrorSignatureFormatError userInfo:nil];
    }
}

- (NSData *)_createASN1SignatureFromJWSSignature:(NSData *)jwsSignature
{
    return nil; //%%% to be revisited using non-deprecated ASN1 methods
}

- (NSData *) dataWithBase64URLEncodedString:(NSString *)base64URLEncodedString
{
    NSString *base64standard = base64URLEncodedString;
    base64standard = [base64standard stringByReplacingOccurrencesOfString:@"-" withString:@"+"];
    base64standard = [base64standard stringByReplacingOccurrencesOfString:@"_" withString:@"/"];
    while (base64standard.length % 4) {
        base64standard = [base64standard stringByAppendingString:@"="];
    }
    NSData *data = [[NSData alloc] initWithBase64EncodedString:base64standard options:(NSDataBase64DecodingOptions)0];
    return data;
}

@end

@implementation SecJWSEncoder

- (instancetype) init
{
    if (self = [super init]) {
    }
    return self;
}

- (instancetype) initWithPublicKey:(SecKeyRef)pubKey privateKey:(SecKeyRef)privKey
{
    if (self = [super init]) {
        _publicKey = pubKey;
        _privateKey = privKey;
    }
    return self;
}

- (void) dealloc
{
    CFReleaseNull(_publicKey);
    CFReleaseNull(_privateKey);
}

- (NSError *) createKeyPair {
    CFErrorRef error = NULL;
    NSDictionary *dict = @{
        (id)kSecAttrKeyType : (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeySizeInBits : @256,
    };
    _privateKey = SecKeyCreateRandomKey((__bridge CFDictionaryRef)dict, &error);
    if (_privateKey) {
        _publicKey = SecKeyCopyPublicKey(_privateKey);
    }
    if ((!_privateKey || !_publicKey) && !error) {
        error = (CFErrorRef)CFBridgingRetain([NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInvalidKey userInfo:nil]);
    }
    if (error) {
        CFReleaseNull(_privateKey);
        CFReleaseNull(_publicKey);
    }
    return (error) ? (NSError *)CFBridgingRelease(error) : nil;
}

- (NSDictionary * _Nullable) jwkPublicKey
{
    NSDictionary *result = nil;
    CFErrorRef error = NULL;
    CFDataRef pubKeyData = SecKeyCopyExternalRepresentation(_publicKey, &error);
    if (!pubKeyData) { return nil; }
    NSData *data = (__bridge NSData *)pubKeyData;
    const char *p = [data bytes];
    // kSecAttrKeyTypeECSECPrimeRandom  ANSI X9.63 format (04 || X || Y [ || K])
    // note: public key is just (04 || X || Y).
    // we expect a p256 key or greater, with leading 04 byte
    if (data.length > 64 && p && *p++ == 0x04) {
        NSUInteger Xlength = (data.length - 1) / 2;
        NSUInteger Ylength = (data.length - 1) - Xlength;
        NSData *Xdata = [NSData dataWithBytes:p length:Xlength];
        NSData *Ydata = [NSData dataWithBytes:p+Xlength length:Ylength];
        NSString *X = [self base64URLEncodedStringRepresentationWithData:Xdata];
        NSString *Y = [self base64URLEncodedStringRepresentationWithData:Ydata];
        //NSString *K = @"com.apple.acmeclient"; //%%% this needs to be passed in
        NSDictionary *jwkDict = @{
            @"kty" : @"EC",
            @"crv" : @"P-256",
            @"x" : X,
            @"y" : Y,
            //@"kid": K,
        };
        result = jwkDict;
    }
    CFReleaseNull(pubKeyData);

    return result;
}

- (BOOL) appendPaddedToData:(NSMutableData *)data ptr:(const DERByte *)ptr len:(DERSize)len expected:(DERSize)expLen {
    // Attempts to append expLen bytes from ptr to the data instance,
    // zero-padding if needed. Returns false if we cannot zero-pad or
    // remove leading zeroes to get the expected length.
    const DERByte *p = ptr;
    DERSize length = len;
    if (length > expLen) {
        // skip leading zeroes until we get expected length or non-zero
        while (p[0] == 0x00 && length > expLen) {
            p++;
            length--;
        }
        if (length != expLen) {
            return false;
        }
    } else if (length < expLen) {
        // zero-pad to expected length
        uint8_t zeroByte = 0;
        while (length < expLen) {
            [data appendBytes:&zeroByte length:sizeof(zeroByte)];
            length++;
        }
        length = len; // number of bytes remaining to append
    }
    [data appendData:[NSData dataWithBytes:p length:length]];
    return true;
}

- (NSString *) signatureWithProtectedHeader:(NSString *)protected payload:(NSString *)payload
{
    NSData *content = [[NSString stringWithFormat:@"%@.%@", protected, payload] dataUsingEncoding:NSUTF8StringEncoding];
    // according to RFC 7515, we sign this content and not a digest of it
    CFErrorRef error = NULL;
    CFDataRef signature = SecKeyCreateSignature(_privateKey,
                                                kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
                                                (__bridge CFDataRef)content,
                                                &error);
    // Note: consider changing the above to use kSecKeyAlgorithmECDSASignatureRFC4754
    // with a digest of the content string; output signature should be just (R||S).
    if (error) {
        secerror("Failed to create signature: %@", error);
        CFReleaseNull(error);
    }
    if (!signature) {
        return nil; // cannot continue
    }

    // We have an ASN.1 signature, but we need to return a JWS signature.
    // Per RFC 7515, this is just a concatenation of the two integers
    // (R || S) in sequence. Further note that in RFC 7518 3.4, the octet
    // sequence representations of R and S must both be 32 byte values
    // for a total of 64 bytes, maintaining leading zeroes if present.
    NSMutableData *data = [NSMutableData dataWithCapacity:0];
    DERReturn drtn = DR_GenericErr;
    DERSize octlen = 32; // required length of each value in bytes
    DER_ECDSASig parsedSig;
    DERItem signatureItem = {
        .data = (DERByte *)CFDataGetBytePtr(signature),
        .length = (DERSize)CFDataGetLength(signature)
    };
    drtn = DERParseSequence(&signatureItem, DERNumECDSASigItemSpecs, DER_ECDSASigItemSpecs, &parsedSig, sizeof(parsedSig));
    if (drtn != DR_Success ||
        !parsedSig.r.data || parsedSig.r.length <= 0 ||
        !parsedSig.s.data || parsedSig.s.length <= 0) {
        secerror("Failed to parse signature: %d", drtn);
        CFReleaseNull(signature);
        return nil;
    }
    if (![self appendPaddedToData:data ptr:parsedSig.r.data len:parsedSig.r.length expected:octlen]) {
        secerror("Non-compliant signature: r is %lld bytes, expected %lld",
                 (long long)parsedSig.r.length, (long long)octlen);
        CFReleaseNull(signature);
        return nil;
    }
    if (![self appendPaddedToData:data ptr:parsedSig.s.data len:parsedSig.s.length expected:octlen]) {
        secerror("Non-compliant signature: s is %lld bytes, expected %lld",
                 (long long)parsedSig.s.length, (long long)octlen);
        CFReleaseNull(signature);
        return nil;
    }
    NSString *result = [self base64URLEncodedStringRepresentationWithData:data];
    CFReleaseNull(signature);

    return result;
}

- (NSString *) encodedJWSWithPayload:(NSDictionary * _Nullable)payload kid:(NSString * _Nullable)kid nonce:(NSString *)nonce url:(NSString *)url error:(NSError ** _Nullable)error
{
    NSError *localError = nil;
    NSString *compactJsonResult = nil;
    // need a key pair here; create if it doesn't already exist
    if (!_privateKey || !_publicKey) {
        localError = [self createKeyPair];
    }
    NSMutableDictionary *protected = [NSMutableDictionary dictionaryWithCapacity:0];
    protected[@"alg"] = @"ES256";
    // kid and jwk are mutually exclusive options; provide jwk only when kid is not supplied
    if (kid) { protected[@"kid"] = kid; } else { protected[@"jwk"] = [self jwkPublicKey]; }
    protected[@"nonce"] = nonce;
    protected[@"url"] = url;

    if (!localError) {
        // RFC 8555 6.3 says that for a nil payload, the "payload" field of the
        // JWS object MUST be present and set to the empty string ("").
        NSString *payloadValue = (payload) ? [self base64URLEncodedStringRepresentationWithDictionary:payload] : @"";
        NSString *protectedValue = [self base64URLEncodedStringRepresentationWithDictionary:protected];
        NSString *signatureValue = [self signatureWithProtectedHeader:protectedValue payload:payloadValue];
        NSMutableString *jsonStr = [NSMutableString stringWithCapacity:0];
        [jsonStr appendString:@"{"];
        [jsonStr appendFormat:@"\"protected\":\"%@\",", protectedValue];
        [jsonStr appendFormat:@"\"payload\":\"%@\",", payloadValue];
        [jsonStr appendFormat:@"\"signature\":\"%@\"", signatureValue];
        [jsonStr appendString:@"}"];
        compactJsonResult = jsonStr;
        //NSLog(@"JWS: %@", compactJsonResult);
    }
    if (error) {
        *error = localError;
    } else {
        localError = nil;
    }
    return compactJsonResult;
}

- (NSString *) base64URLEncodedStringRepresentationWithData:(NSData *)data
{
    NSString *string = [data base64EncodedStringWithOptions:0];
    string = [string stringByReplacingOccurrencesOfString:@"+" withString:@"-"];
    string = [string stringByReplacingOccurrencesOfString:@"/" withString:@"_"];
    // RFC 8555 6.1 says to remove trailing '=' padding
    while ([string length] > 1 && [string hasSuffix:@"="]) {
        string = [string substringToIndex:[string length]-1];
    }
    return string;
}

- (NSString *) base64URLEncodedStringRepresentationWithDictionary:(NSDictionary *)dictionary
{
    NSError *error = nil;
    NSJSONWritingOptions opts = NSJSONWritingWithoutEscapingSlashes | NSJSONWritingSortedKeys;
    NSData *data = [NSJSONSerialization dataWithJSONObject:dictionary options:opts error:&error];
    NSString *string = [data base64EncodedStringWithOptions:0];
    string = [string stringByReplacingOccurrencesOfString:@"+" withString:@"-"];
    string = [string stringByReplacingOccurrencesOfString:@"/" withString:@"_"];
    // RFC 8555 6.1 says to remove trailing '=' padding
    while ([string length] && [string hasSuffix:@"="]) {
        string = [string substringToIndex:[string length]-1];
    }
    return string;
}

- (NSString *) compactJSONStringRepresentationWithDictionary:(NSDictionary *)dictionary
{
    NSError *error = nil;
    NSJSONWritingOptions opts = NSJSONWritingWithoutEscapingSlashes | NSJSONWritingSortedKeys;
    NSData *data = [NSJSONSerialization dataWithJSONObject:dictionary options:opts error:&error];
    NSString *string = [[NSString alloc] initWithBytes:data.bytes length:data.length encoding:NSUTF8StringEncoding];
    return string;
}

@end
