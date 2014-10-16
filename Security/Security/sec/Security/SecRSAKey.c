/*
 * Copyright (c) 2006-2010,2012-2014 Apple Inc. All Rights Reserved.
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
 * SecRSAKey.c - CoreFoundation based rsa key object
 */


#include "SecRSAKey.h"
#include "SecRSAKeyPriv.h"
#include <Security/SecKeyInternal.h>
#include <Security/SecItem.h>
#include <Security/SecBasePriv.h>
#include <AssertMacros.h>
#include <Security/SecureTransport.h> /* For error codes. */
#include <CoreFoundation/CFData.h> /* For error codes. */
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <CoreFoundation/CFNumber.h>
#include <Security/SecFramework.h>
#include <Security/SecRandom.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include "SecItemPriv.h"
#include <Security/SecInternal.h>

#include <corecrypto/ccn.h>
#include <corecrypto/ccrsa.h>
#include <corecrypto/ccsha1.h>

#include <libDER/asn1Types.h>
#include <libDER/DER_Keys.h>
#include <libDER/DER_Encode.h>

#include <CommonCrypto/CommonDigest.h>

#include <corecrypto/ccrsa_priv.h>

#include <stdint.h>
#include <string.h>

#define kMaximumRSAKeyBits 4096
#define kMaximumRSAKeyBytes ccn_sizeof(kMaximumRSAKeyBits)

#define RSA_PKCS1_PAD_SIGN		0x01
#define RSA_PKCS1_PAD_ENCRYPT	0x02

static void ccn_c_dump(cc_size count, const cc_unit *s)
{
    printf("{ ");
    cc_size ix;
    for (ix = count; ix--;) {
        printf("0x%.02x, 0x%.02x, 0x%.02x, 0x%.02x, ",
               (int) ((s[ix] >> 24) & 0xFF),
               (int) ((s[ix] >> 16) & 0xFF),
               (int) ((s[ix] >> 8 ) & 0xFF),
               (int) ((s[ix] >> 0 ) & 0xFF));
    }
    printf("};");
}

static void ccn_cprint(cc_size count, char* prefix, const cc_unit *s)
{
    printf("%s", prefix);
    ccn_c_dump(count, s);
    printf("\n");
}

void ccrsa_dump_full_key(ccrsa_full_ctx_t key); // Suppress warnings
void ccrsa_dump_full_key(ccrsa_full_ctx_t key) {
    ccn_cprint(ccrsa_ctx_n(key),      "uint8_t m[]  = ", ccrsa_ctx_m(key));
    ccn_cprint(ccrsa_ctx_n(key) + 1,  "uint8_t rm[] = ", cczp_recip(ccrsa_ctx_zm(key)));
    ccn_cprint(ccrsa_ctx_n(key),      "uint8_t e[]  = ", ccrsa_ctx_e(key));
    ccn_cprint(ccrsa_ctx_n(key),      "uint8_t d[]  = ", ccrsa_ctx_d(key));
    
    printf("cc_size np = %lu;\n", cczp_n(ccrsa_ctx_private_zp(ccrsa_ctx_private(key))));
    ccn_cprint(cczp_n(ccrsa_ctx_private_zp(ccrsa_ctx_private(key))),     "uint8_t p[]  = ",
               cczp_prime(ccrsa_ctx_private_zp(ccrsa_ctx_private(key))));
    ccn_cprint(cczp_n(ccrsa_ctx_private_zp(ccrsa_ctx_private(key))) + 1, "uint8_t rp[] = ",
               cczp_recip(ccrsa_ctx_private_zp(ccrsa_ctx_private(key))));
    printf("cc_size nq = %lu;\n", cczp_n(ccrsa_ctx_private_zq(ccrsa_ctx_private(key))));
    ccn_cprint(cczp_n(ccrsa_ctx_private_zq(ccrsa_ctx_private(key))),     "uint8_t q[]  = ",
               cczp_prime(ccrsa_ctx_private_zq(ccrsa_ctx_private(key))));
    ccn_cprint(cczp_n(ccrsa_ctx_private_zq(ccrsa_ctx_private(key))) + 1, "uint8_t rq[] = ",
               cczp_recip(ccrsa_ctx_private_zq(ccrsa_ctx_private(key))));
    ccn_cprint(cczp_n(ccrsa_ctx_private_zp(ccrsa_ctx_private(key))),     "uint8_t dp[] = ",
               ccrsa_ctx_private_dp(ccrsa_ctx_private(key)));
    ccn_cprint(cczp_n(ccrsa_ctx_private_zq(ccrsa_ctx_private(key))),     "uint8_t dq[] = ",
               ccrsa_ctx_private_dq(ccrsa_ctx_private(key)));
    ccn_cprint(cczp_n(ccrsa_ctx_private_zp(ccrsa_ctx_private(key))),     "uint8_t qinv[] = ",
               ccrsa_ctx_private_qinv(ccrsa_ctx_private(key)));
    printf("--\n");
}

void ccrsa_dump_public_key(ccrsa_pub_ctx_t key); // Suppress warning.
void ccrsa_dump_public_key(ccrsa_pub_ctx_t key) {
    ccn_cprint(ccrsa_ctx_n(key),      "uint8_t m[]  = ", ccrsa_ctx_m(key));
    ccn_cprint(ccrsa_ctx_n(key) + 1,  "uint8_t rm[] = ", cczp_recip(ccrsa_ctx_zm(key)));
    ccn_cprint(ccrsa_ctx_n(key),      "uint8_t e[]  = ", ccrsa_ctx_e(key));
    
    printf("--\n");
}

/*
 *
 * Public Key
 *
 */

/* Public key static functions. */
static void SecRSAPublicKeyDestroy(SecKeyRef key) {
    /* Zero out the public key */
    ccrsa_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    cc_zero(ccrsa_pub_ctx_size(ccn_sizeof_n(ccrsa_ctx_n(pubkey))), pubkey.pub);
}

#define cc_skip_zeros(size, ptr) { while (size > 0 && *ptr == 0) { ++ptr; --size; } }

//
// pubkey is initilaized with an n which is the maximum it can hold
// We set the n to its correct value given m.
//
static int ccrsa_pub_init(ccrsa_pub_ctx_t pubkey,
                          size_t m_size, const uint8_t* m,
                          size_t e_size, const uint8_t* e)
{
    cc_skip_zeros(m_size, m);
    
    cc_size nm = ccn_nof_size(m_size);
    if (nm > ccrsa_ctx_n(pubkey))
        return -1;
    
    ccrsa_ctx_n(pubkey) = nm;
    
    ccn_read_uint(nm, ccrsa_ctx_m(pubkey), m_size, m);
    cczp_init(ccrsa_ctx_zm(pubkey));
    
    return ccn_read_uint(nm, ccrsa_ctx_e(pubkey), e_size, e);
}


static OSStatus ccrsa_pub_decode(ccrsa_pub_ctx_t pubkey, size_t pkcs1_size, const uint8_t* pkcs1)
{
    OSStatus result = errSecParam;
    
	DERItem keyItem = {(DERByte *)pkcs1, pkcs1_size};
    DERRSAPubKeyPKCS1 decodedKey;
    
	require_noerr_action(DERParseSequence(&keyItem,
                                          DERNumRSAPubKeyPKCS1ItemSpecs, DERRSAPubKeyPKCS1ItemSpecs,
                                          &decodedKey, sizeof(decodedKey)),
                         errOut, result = errSecDecode);
    
    require_noerr(ccrsa_pub_init(pubkey,
                                 decodedKey.modulus.length, decodedKey.modulus.data,
                                 decodedKey.pubExponent.length, decodedKey.pubExponent.data),
                  errOut);
    
    result = errSecSuccess;
    
errOut:
    return result;
}

static OSStatus ccrsa_pub_decode_apple(ccrsa_pub_ctx_t pubkey, size_t pkcs1_size, const uint8_t* pkcs1)
{
    OSStatus result = errSecParam;
    
	DERItem keyItem = {(DERByte *)pkcs1, pkcs1_size};
    DERRSAPubKeyApple decodedKey;
    
	require_noerr_action(DERParseSequence(&keyItem,
                                          DERNumRSAPubKeyAppleItemSpecs, DERRSAPubKeyAppleItemSpecs,
                                          &decodedKey, sizeof(decodedKey)),
                         errOut, result = errSecDecode);
    
    // We could honor the recipricol, but we don't think this is used enough to care.
    // Don't bother exploding the below function to try to handle this case, it computes.
    
    require_noerr(ccrsa_pub_init(pubkey,
                                 decodedKey.modulus.length, decodedKey.modulus.data,
                                 decodedKey.pubExponent.length, decodedKey.pubExponent.data),
                  errOut);
    
    result = errSecSuccess;
    
errOut:
    return result;
}


static void ccasn_encode_int(cc_size n, const cc_unit*s, size_t s_size, uint8_t **buffer)
{
    **buffer = ASN1_INTEGER;
    *buffer += 1;
    
    DERSize itemLength = 4;
    DEREncodeLength(s_size, *buffer, &itemLength);
    *buffer += itemLength;
    
    ccn_write_int(n, s, s_size, *buffer);
    
    *buffer += s_size;
}


static OSStatus SecRSAPublicKeyInit(SecKeyRef key,
                                    const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    
    OSStatus result = errSecParam;
    
    ccrsa_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    
    // Set maximum size for parsers
    ccrsa_ctx_n(pubkey) = ccn_nof(kMaximumRSAKeyBits);
    
    switch (encoding) {
        case kSecKeyEncodingBytes: // Octets is PKCS1
        case kSecKeyEncodingPkcs1:
            result = ccrsa_pub_decode(pubkey, keyDataLength, keyData);
            break;
        case kSecKeyEncodingApplePkcs1:
            result = ccrsa_pub_decode_apple(pubkey, keyDataLength, keyData);
            break;
        case kSecKeyEncodingRSAPublicParams:
        {
            SecRSAPublicKeyParams *params = (SecRSAPublicKeyParams *)keyData;
            
            require_noerr(ccrsa_pub_init(pubkey,
                                         params->modulusLength, params->modulus,
                                         params->exponentLength, params->exponent), errOut);
            
            result = errSecSuccess;
            break;
        }
        case kSecExtractPublicFromPrivate:
        {
            ccrsa_full_ctx_t fullKey;
            fullKey.full = (ccrsa_full_ctx*) keyData;
            
            cc_size fullKeyN = ccrsa_ctx_n(fullKey);
            require(fullKeyN <= ccrsa_ctx_n(pubkey), errOut);
            memcpy(pubkey.pub, ccrsa_ctx_public(fullKey).pub, ccrsa_pub_ctx_size(ccn_sizeof_n(fullKeyN)));
            result = errSecSuccess;
            break;
        }
        default:
            break;
    }
    
errOut:
    return result;
}

static OSStatus SecRSAPublicKeyRawVerify(SecKeyRef key, SecPadding padding,
                                         const uint8_t *signedData, size_t signedDataLen,
                                         const uint8_t *sig, size_t sigLen) {
    OSStatus result = errSSLCrypto;
    
    ccrsa_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    
    cc_unit s[ccrsa_ctx_n(pubkey)];
    
    ccn_read_uint(ccrsa_ctx_n(pubkey), s, sigLen, sig);
    ccrsa_pub_crypt(pubkey, s, s);
    ccn_swap(ccrsa_ctx_n(pubkey), s);
    
    const uint8_t* sBytes = (uint8_t*) s;
    const uint8_t* sEnd = (uint8_t*) (s + ccrsa_ctx_n(pubkey));
    
    switch (padding) {
        case kSecPaddingNone:
            // Skip leading zeros as long as s is bigger than signedData.
            while (((ptrdiff_t)signedDataLen < (sEnd - sBytes)) && (*sBytes == 0))
                ++sBytes;
            break;
            
        case kSecPaddingPKCS1:
        {
            // Verify and skip PKCS1 padding:
            //
            // 0x00, 0x01 (RSA_PKCS1_PAD_SIGN), 0xFF .. 0x00, signedData
            //
            size_t m_size = ccn_write_uint_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey));
            size_t prefix_zeros = ccn_sizeof_n(ccrsa_ctx_n(pubkey)) - m_size;
            
            while (prefix_zeros--)
                require_quiet(*sBytes++ == 0x00, errOut);
            
            require_quiet(*sBytes++ == 0x00, errOut);
            require_quiet(*sBytes++ == RSA_PKCS1_PAD_SIGN, errOut);
            
            while (*sBytes == 0xFF) {
                require_quiet(++sBytes < sEnd, errOut);
            }
            // Required to have at least 8 0xFFs
            require_quiet((sBytes - (uint8_t*)s) - 2 >= 8, errOut);
            
            require_quiet(*sBytes == 0x00, errOut);
            require_quiet(++sBytes < sEnd, errOut);
            break;
        }
        case kSecPaddingOAEP:
            result = errSecParam;
            goto errOut;
            
        default:
            result = errSecUnimplemented;
            goto errOut;
    }
    
    // Compare the rest.
    require_quiet((sEnd - sBytes) == (ptrdiff_t)signedDataLen, errOut);
    require_quiet(memcmp(sBytes, signedData, signedDataLen) == 0, errOut);
    
    result = errSecSuccess;
    
errOut:
    cc_zero(ccrsa_ctx_n(pubkey), s);
    
    return result;
}

static OSStatus SecRSAPublicKeyRawEncrypt(SecKeyRef key, SecPadding padding,
                                          const uint8_t *plainText, size_t plainTextLen,
                                          uint8_t *cipherText, size_t *cipherTextLen) {
    OSStatus result = errSecParam;
    ccrsa_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    
    cc_unit s[ccrsa_ctx_n(pubkey)];
    const size_t m_size = ccn_write_uint_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey));
    
    require(cipherTextLen, errOut);
    require(*cipherTextLen >= m_size, errOut);
    
    uint8_t* sBytes = (uint8_t*) s;
    
    switch (padding) {
        case kSecPaddingNone:
            // We'll allow modulus size assuming input is smaller than modulus
            require_quiet(plainTextLen <= m_size, errOut);
            require_noerr_quiet(ccn_read_uint(ccrsa_ctx_n(pubkey), s, plainTextLen, plainText), errOut);
            require_quiet(ccn_cmp(ccrsa_ctx_n(pubkey), s, ccrsa_ctx_m(pubkey)) < 0, errOut);
            break;
            
        case kSecPaddingPKCS1:
        {
            // Create PKCS1 padding:
            //
            // 0x00, 0x01 (RSA_PKCS1_PAD_ENCRYPT), 0xFF .. 0x00, signedData
            //
            const int kMinimumPadding = 1 + 1 + 8 + 1;
            
            require_quiet(plainTextLen <= m_size - kMinimumPadding, errOut);
            
            size_t prefix_zeros = ccn_sizeof_n(ccrsa_ctx_n(pubkey)) - m_size;
            
            while (prefix_zeros--)
                *sBytes++ = 0x00;
            
            size_t pad_size = m_size - plainTextLen;
            
            *sBytes++ = 0x00;
            *sBytes++ = RSA_PKCS1_PAD_ENCRYPT;
            
            ccrng_generate(ccrng_seckey, pad_size - 3, sBytes);
            // Remove zeroes from the random pad
            
            const uint8_t* sEndOfPad = sBytes + (pad_size - 3);
            while (sBytes < sEndOfPad)
            {
                if (*sBytes == 0x00)
                    *sBytes = 0xFF; // Michael said 0xFF was good enough.
                
                ++sBytes;
            }
            
            *sBytes++ = 0x00;
            
            memcpy(sBytes, plainText, plainTextLen);
            
            ccn_swap(ccrsa_ctx_n(pubkey), s);
            break;
        }
        case kSecPaddingOAEP:
        {
            const struct ccdigest_info* di = ccsha1_di();
            
            const size_t encodingOverhead = 2 + 2 * di->output_size;
            
            require_action(m_size > encodingOverhead, errOut, result = errSecParam);
            require_action_quiet(plainTextLen <= m_size - encodingOverhead, errOut, result = errSecParam);
            
            require_noerr_action(ccrsa_oaep_encode(di,
                                                   ccrng_seckey,
                                                   m_size, s,
                                                   plainTextLen, plainText), errOut, result = errSecInternal);
            break;
        }
        default:
            goto errOut;
    }
    
    
    ccrsa_pub_crypt(pubkey, s, s);
    
    ccn_write_uint_padded(ccrsa_ctx_n(pubkey), s, m_size, cipherText);
    *cipherTextLen = m_size;
    
    result = errSecSuccess;
    
errOut:
    ccn_zero(ccrsa_ctx_n(pubkey), s);
    return result;
}

static OSStatus SecRSAPublicKeyRawDecrypt(SecKeyRef key, SecPadding padding,
                                          const uint8_t *cipherText, size_t cipherTextLen, uint8_t *plainText, size_t *plainTextLen) {
    OSStatus result = errSSLCrypto;
    
    ccrsa_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    
    cc_unit s[ccrsa_ctx_n(pubkey)];
    
    require_action_quiet(cipherText != NULL, errOut, result = errSecParam);
    require_action_quiet(plainText != NULL, errOut, result = errSecParam);
    require_action_quiet(plainTextLen != NULL, errOut, result = errSecParam);
    
    ccn_read_uint(ccrsa_ctx_n(pubkey), s, cipherTextLen, cipherText);
    ccrsa_pub_crypt(pubkey, s, s);
    ccn_swap(ccrsa_ctx_n(pubkey), s);
    
    const uint8_t* sBytes = (uint8_t*) s;
    const uint8_t* sEnd = (uint8_t*) (s + ccrsa_ctx_n(pubkey));
    
    switch (padding) {
        case kSecPaddingNone:
            // Skip leading zeros
            // We return the bytes for a number and
            // trim leading zeroes
            while (sBytes < sEnd && *sBytes == 0x00)
                ++sBytes;
            break;
            
        case kSecPaddingPKCS1:
        {
            // Verify and skip PKCS1 padding:
            //
            // 0x00, 0x01 (RSA_PKCS1_PAD_ENCRYPT), 0xFF .. 0x00, signedData
            //
            size_t m_size = ccn_write_uint_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey));
            size_t prefix_zeros = ccn_sizeof_n(ccrsa_ctx_n(pubkey)) - m_size;
            
            while (prefix_zeros--)
                require_quiet(*sBytes++ == 0x00, errOut);
            
            require_quiet(*sBytes++ == 0x00, errOut);
            require_quiet(*sBytes++ == RSA_PKCS1_PAD_ENCRYPT, errOut);
            
            while (*sBytes != 0x00) {
                require_quiet(++sBytes < sEnd, errOut);
            }
            // Required to have at least 8 0xFFs
            require_quiet((sBytes - (uint8_t*)s) - 2 >= 8, errOut);
            
            require_quiet(*sBytes == 0x00, errOut);
            require_quiet(++sBytes < sEnd, errOut);
            
            break;
        }
        case kSecPaddingOAEP:
            result = errSecParam;
        default:
            goto errOut;
    }
    
    // Return the rest.
    require_action((sEnd - sBytes) <= (ptrdiff_t)*plainTextLen, errOut, result = errSecParam);
    
    *plainTextLen = sEnd - sBytes;
    memcpy(plainText, sBytes, *plainTextLen);
    
    result = errSecSuccess;
    
errOut:
    ccn_zero(ccrsa_ctx_n(pubkey), s);
    
    return result;
}

static size_t SecRSAPublicKeyBlockSize(SecKeyRef key) {
    ccrsa_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    
    return ccn_write_uint_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey));
}


static CFDataRef SecRSAPublicKeyCreatePKCS1(CFAllocatorRef allocator, ccrsa_pub_ctx_t pubkey)
{
    size_t m_size = ccn_write_int_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey));
    size_t e_size = ccn_write_int_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_e(pubkey));
    
    const size_t seq_size = DERLengthOfItem(ASN1_INTEGER, m_size) +
    DERLengthOfItem(ASN1_INTEGER, e_size);
    
    const size_t result_size = DERLengthOfItem(ASN1_SEQUENCE, seq_size);
    
	CFMutableDataRef pkcs1 = CFDataCreateMutable(allocator, result_size);
    
    if (pkcs1 == NULL)
        return NULL;
    
	CFDataSetLength(pkcs1, result_size);
    
    uint8_t *bytes = CFDataGetMutableBytePtr(pkcs1);
    
    *bytes++ = ASN1_CONSTR_SEQUENCE;
    
    DERSize itemLength = 4;
    DEREncodeLength(seq_size, bytes, &itemLength);
    bytes += itemLength;
    
    ccasn_encode_int(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey), m_size, &bytes);
    ccasn_encode_int(ccrsa_ctx_n(pubkey), ccrsa_ctx_e(pubkey), e_size, &bytes);
    
    return pkcs1;
}

static OSStatus SecRSAPublicKeyCopyPublicSerialization(SecKeyRef key, CFDataRef* serialized)
{
    ccrsa_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    
	CFAllocatorRef allocator = CFGetAllocator(key);
    *serialized = SecRSAPublicKeyCreatePKCS1(allocator, pubkey);
    
    if (NULL == *serialized)
        return errSecDecode;
    else
        return errSecSuccess;
}

static CFDictionaryRef SecRSAPublicKeyCopyAttributeDictionary(SecKeyRef key) {
    return SecKeyGeneratePublicAttributeDictionary(key, kSecAttrKeyTypeRSA);
}

static CFStringRef SecRSAPublicKeyCopyDescription(SecKeyRef key) {
    
    CFStringRef keyDescription = NULL;
    CFDataRef modRef = SecKeyCopyModulus(key);
    
    ccrsa_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    
    CFStringRef modulusString = CFDataCopyHexString(modRef);
    require( modulusString, fail);
    
    keyDescription = CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR( "<SecKeyRef algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, exponent: {hex: %llx, decimal: %lld}, modulus: %@, addr: %p>"), SecKeyGetAlgorithmID(key), key->key_class->name, key->key_class->version, (8*SecKeyGetBlockSize(key)), (long long)*ccrsa_ctx_e(pubkey), (long long)*ccrsa_ctx_e(pubkey), modulusString, key);
    
fail:
    CFReleaseSafe(modRef);
    CFReleaseSafe(modulusString);
	if(!keyDescription)
		keyDescription = CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecKeyRef algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, addr: %p>"), (long)SecKeyGetAlgorithmID(key), key->key_class->name, key->key_class->version, (8*SecKeyGetBlockSize(key)), key);
	
	return keyDescription;
}

SecKeyDescriptor kSecRSAPublicKeyDescriptor = {
    kSecKeyDescriptorVersion,
    "RSAPublicKey",
    ccrsa_pub_ctx_size(kMaximumRSAKeyBytes), /* extraBytes */
    SecRSAPublicKeyInit,
    SecRSAPublicKeyDestroy,
    NULL, /* SecKeyRawSignMethod */
    SecRSAPublicKeyRawVerify,
    SecRSAPublicKeyRawEncrypt,
    SecRSAPublicKeyRawDecrypt,
    NULL, /* SecKeyComputeMethod */
    SecRSAPublicKeyBlockSize,
	SecRSAPublicKeyCopyAttributeDictionary,
    SecRSAPublicKeyCopyDescription,
    NULL,
    SecRSAPublicKeyCopyPublicSerialization,
    NULL,
    NULL
};

/* Public Key API functions. */
SecKeyRef SecKeyCreateRSAPublicKey(CFAllocatorRef allocator,
                                   const uint8_t *keyData, CFIndex keyDataLength,
                                   SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecRSAPublicKeyDescriptor, keyData,
                        keyDataLength, encoding);
}

CFDataRef SecKeyCopyModulus(SecKeyRef key) {
    ccrsa_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    
    size_t m_size = ccn_write_uint_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey));
    
	CFAllocatorRef allocator = CFGetAllocator(key);
	CFMutableDataRef modulusData = CFDataCreateMutable(allocator, m_size);
    
    if (modulusData == NULL)
        return NULL;
    
	CFDataSetLength(modulusData, m_size);
    
    ccn_write_uint(ccrsa_ctx_n(pubkey), ccrsa_ctx_m(pubkey), m_size, CFDataGetMutableBytePtr(modulusData));
    
    return modulusData;
}

CFDataRef SecKeyCopyExponent(SecKeyRef key) {
    ccrsa_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    
    size_t e_size = ccn_write_uint_size(ccrsa_ctx_n(pubkey), ccrsa_ctx_e(pubkey));
    
	CFAllocatorRef allocator = CFGetAllocator(key);
	CFMutableDataRef exponentData = CFDataCreateMutable(allocator, e_size);
    
    if (exponentData == NULL)
        return NULL;
    
	CFDataSetLength(exponentData, e_size);
    
    ccn_write_uint(ccrsa_ctx_n(pubkey), ccrsa_ctx_e(pubkey), e_size, CFDataGetMutableBytePtr(exponentData));
    
    return exponentData;
}


/*
 *
 * Private Key
 *
 */

/* Private key static functions. */
static void SecRSAPrivateKeyDestroy(SecKeyRef key) {
    /* Zero out the public key */
    ccrsa_full_ctx_t fullkey;
    fullkey.full = key->key;
    cc_zero(ccrsa_full_ctx_size(ccn_sizeof_n(ccrsa_ctx_n(fullkey))), fullkey.full);
}

static int ccrsa_priv_init(ccrsa_priv_ctx_t privkey,
                           size_t p_size, const uint8_t* p,
                           size_t q_size, const uint8_t* q,
                           size_t dp_size, const uint8_t* dp,
                           size_t dq_size, const uint8_t* dq,
                           size_t qinv_size, const uint8_t* qinv)
{
    int result = -1;
    
    const cc_size np = cczp_n(ccrsa_ctx_private_zp(privkey));
    cc_size nq = cczp_n(ccrsa_ctx_private_zq(privkey));
    
    if (ccn_read_uint(np, CCZP_PRIME(ccrsa_ctx_private_zp(privkey)), p_size, p))
        goto errOut;
    cczp_init(ccrsa_ctx_private_zp(privkey));
    if (ccn_read_uint(np, ccrsa_ctx_private_dp(privkey), dp_size, dp))
        goto errOut;
    if (ccn_read_uint(np, ccrsa_ctx_private_qinv(privkey), qinv_size, qinv))
        goto errOut;
    
    if (ccn_read_uint(nq, CCZP_PRIME(ccrsa_ctx_private_zq(privkey)), q_size, q))
        goto errOut;
    
    nq = ccn_n(nq, cczp_prime(ccrsa_ctx_private_zq(privkey)));
    CCZP_N(ccrsa_ctx_private_zq(privkey)) = nq;
    
    cczp_init(ccrsa_ctx_private_zq(privkey));
    if (ccn_read_uint(nq, ccrsa_ctx_private_dq(privkey), dq_size, dq))
        goto errOut;
    
    result = 0;
    
errOut:
    return result;
}


static OSStatus ccrsa_full_decode(ccrsa_full_ctx_t fullkey, size_t pkcs1_size, const uint8_t* pkcs1)
{
    OSStatus result = errSecParam;
    
	DERItem keyItem = {(DERByte *)pkcs1, pkcs1_size};
    DERRSAKeyPair decodedKey;
    
	require_noerr_action(DERParseSequence(&keyItem,
                                          DERNumRSAKeyPairItemSpecs, DERRSAKeyPairItemSpecs,
                                          &decodedKey, sizeof(decodedKey)),
                         errOut, result = errSecDecode);
    
    require_noerr(ccrsa_pub_init(fullkey,
                                 decodedKey.n.length, decodedKey.n.data,
                                 decodedKey.e.length, decodedKey.e.data),
                  errOut);
    ccn_read_uint(ccrsa_ctx_n(fullkey), ccrsa_ctx_d(fullkey),
                  decodedKey.d.length, decodedKey.d.data);
    {
        ccrsa_priv_ctx_t privkey = ccrsa_ctx_private(fullkey);
        CCZP_N(ccrsa_ctx_private_zp(privkey)) = ccn_nof((ccn_bitsof_n(ccrsa_ctx_n(fullkey)) / 2) + 1);
        CCZP_N(ccrsa_ctx_private_zq(privkey)) = cczp_n(ccrsa_ctx_private_zp(privkey));
        
        // TODO: Actually remember decodedKey.d.
        
        require_noerr(ccrsa_priv_init(privkey,
                                      decodedKey.p.length, decodedKey.p.data,
                                      decodedKey.q.length, decodedKey.q.data,
                                      decodedKey.dp.length, decodedKey.dp.data,
                                      decodedKey.dq.length, decodedKey.dq.data,
                                      decodedKey.qInv.length, decodedKey.qInv.data),
                      errOut);
    }
    
    result = errSecSuccess;
    
errOut:
    return result;
}

static OSStatus SecRSAPrivateKeyInit(SecKeyRef key,
                                     const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    OSStatus result = errSecParam;
    
    ccrsa_full_ctx_t fullkey;
    fullkey.full = key->key;
    
    // Set maximum size for parsers
    ccrsa_ctx_n(fullkey) = ccn_nof(kMaximumRSAKeyBits);
    
    switch (encoding) {
        case kSecKeyEncodingBytes: // Octets is PKCS1
        case kSecKeyEncodingPkcs1:
            result = ccrsa_full_decode(fullkey, keyDataLength, keyData);
            break;
        case kSecGenerateKey:
        {
            CFDictionaryRef parameters = (CFDictionaryRef) keyData;
            
            CFTypeRef ksize = CFDictionaryGetValue(parameters, kSecAttrKeySizeInBits);
            CFIndex keyLengthInBits = getIntValue(ksize);
            
            if (keyLengthInBits < 256 || keyLengthInBits > kMaximumRSAKeyBits) {
                secwarning("Invalid or missing key size in: %@", parameters);
                return errSecKeySizeNotAllowed;
            }
            
            /* TODO: Add support for kSecPublicExponent parameter. */
            static uint8_t e[] = { 0x01, 0x00, 0x01 }; // Default is 65537
            if (!ccrsa_generate_key(keyLengthInBits, fullkey.full, sizeof(e), e, ccrng_seckey))
                result = errSecSuccess;
            break;
        }
        default:
            break;
    }
    
    return result;
}

static OSStatus SecRSAPrivateKeyRawSign(SecKeyRef key, SecPadding padding,
                                        const uint8_t *dataToSign, size_t dataToSignLen,
                                        uint8_t *sig, size_t *sigLen) {
    
    OSStatus result = errSecParam;
    
    ccrsa_full_ctx_t fullkey;
    fullkey.full = key->key;
    
    size_t m_size = ccn_write_uint_size(ccrsa_ctx_n(fullkey), ccrsa_ctx_m(fullkey));
    cc_unit s[ccrsa_ctx_n(fullkey)];
    
    uint8_t* sBytes = (uint8_t*) s;
    
    require(sigLen, errOut);
    require(*sigLen >= m_size, errOut);
    
    switch (padding) {
        case kSecPaddingNone:
            // We'll allow modulus size assuming input is smaller than modulus
            require_quiet(dataToSignLen <= m_size, errOut);
            require_noerr_quiet(ccn_read_uint(ccrsa_ctx_n(fullkey), s, dataToSignLen, dataToSign), errOut);
            require_quiet(ccn_cmp(ccrsa_ctx_n(fullkey), s, ccrsa_ctx_m(fullkey)) < 0, errOut);
            break;
            
        case kSecPaddingPKCS1:
        {
            // Create PKCS1 padding:
            //
            // 0x00, 0x01 (RSA_PKCS1_PAD_SIGN), 0xFF .. 0x00, signedData
            //
            const int kMinimumPadding = 1 + 1 + 8 + 1;
            
            require_quiet(dataToSignLen <= m_size - kMinimumPadding, errOut);
            
            size_t prefix_zeros = ccn_sizeof_n(ccrsa_ctx_n(fullkey)) - m_size;
            
            while (prefix_zeros--)
                *sBytes++ = 0x00;
            
            size_t pad_size = m_size - dataToSignLen;
            
            *sBytes++ = 0x00;
            *sBytes++ = RSA_PKCS1_PAD_SIGN;
            
            size_t ff_size;
            for(ff_size = pad_size - 3; ff_size > 0; --ff_size)
                *sBytes++ = 0xFF;
            
            *sBytes++ = 0x00;
            
            // Get the user data into s looking like a ccn.
            memcpy(sBytes, dataToSign, dataToSignLen);
            ccn_swap(ccrsa_ctx_n(fullkey), s);
            
            break;
        }
        case kSecPaddingOAEP:
            result = errSecParam;
        default:
            goto errOut;
    }
    
    ccrsa_priv_crypt(ccrsa_ctx_private(fullkey), s, s);
    
    // Pad with leading zeros to fit in modulus size
    ccn_write_uint_padded(ccrsa_ctx_n(fullkey), s, m_size, sig);
    *sigLen = m_size;
    
    result = errSecSuccess;
    
errOut:
    ccn_zero(ccrsa_ctx_n(fullkey), s);
    return result;
}

static OSStatus SecRSAPrivateKeyRawDecrypt(SecKeyRef key, SecPadding padding,
                                           const uint8_t *cipherText, size_t cipherTextLen,
                                           uint8_t *plainText, size_t *plainTextLen) {
    OSStatus result = errSSLCrypto;
    
    ccrsa_full_ctx_t fullkey;
    fullkey.full = key->key;
    
    size_t m_size = ccn_write_uint_size(ccrsa_ctx_n(fullkey), ccrsa_ctx_m(fullkey));
    
    cc_unit s[ccrsa_ctx_n(fullkey)];
    uint8_t recoveredData[ccn_sizeof_n(ccrsa_ctx_n(fullkey))];
    
    ccn_read_uint(ccrsa_ctx_n(fullkey), s, cipherTextLen, cipherText);
    ccrsa_priv_crypt(ccrsa_ctx_private(fullkey), s, s);
    
    const uint8_t* sBytes = (uint8_t*) s;
    const uint8_t* sEnd = (uint8_t*) (s + ccrsa_ctx_n(fullkey));
    
    require(plainTextLen, errOut);
    
    switch (padding) {
        case kSecPaddingNone:
            ccn_swap(ccrsa_ctx_n(fullkey), s);
            // Skip Zeros since our contract is to do so.
            while (sBytes < sEnd && *sBytes == 0x00)
                ++sBytes;
            break;
            
        case kSecPaddingPKCS1:
        {
            ccn_swap(ccrsa_ctx_n(fullkey), s);
            // Verify and skip PKCS1 padding:
            //
            // 0x00, 0x01 (RSA_PKCS1_PAD_ENCRYPT), 0xFF .. 0x00, signedData
            //
            
            size_t prefix_zeros = ccn_sizeof_n(ccrsa_ctx_n(fullkey)) - m_size;
            
            while (prefix_zeros--)
                require_quiet(*sBytes++ == 0x00, errOut);
            
            require_quiet(*sBytes++ == 0x00, errOut);
            require_quiet(*sBytes++ == RSA_PKCS1_PAD_ENCRYPT, errOut);
            
            while (*sBytes != 0x00) {
                require_quiet(++sBytes < sEnd, errOut);
            }
            // Required to have at least 8 non-zeros
            require_quiet((sBytes - (uint8_t*)s) - 2 >= 8, errOut);
            
            require_quiet(*sBytes == 0x00, errOut);
            require_quiet(++sBytes < sEnd, errOut);
            break;
        }
        case kSecPaddingOAEP:
        {
            size_t length = sizeof(recoveredData);
            
            require_noerr_quiet(ccrsa_oaep_decode(ccsha1_di(),
 												  &length, recoveredData,
                                                  ccn_write_uint_size(ccrsa_ctx_n(fullkey),ccrsa_ctx_m(fullkey)), s
                                                  ), errOut);
            
            sBytes = recoveredData;
            sEnd = recoveredData + length;
            break;
        }
        default:
            goto errOut;
    }
    
    require((sEnd - sBytes) <= (ptrdiff_t)*plainTextLen, errOut);
    *plainTextLen = sEnd - sBytes;
    memcpy(plainText, sBytes, *plainTextLen);
    
    result = errSecSuccess;
    
errOut:
    bzero(recoveredData, sizeof(recoveredData));
    ccn_zero(ccrsa_ctx_n(fullkey), s);
    
    return result;
}

static size_t SecRSAPrivateKeyBlockSize(SecKeyRef key) {
    ccrsa_full_ctx_t fullkey;
    fullkey.full = key->key;
    
    return ccn_write_uint_size(ccrsa_ctx_n(fullkey), ccrsa_ctx_m(fullkey));
}

static CFDataRef SecRSAPrivateKeyCreatePKCS1(CFAllocatorRef allocator, ccrsa_full_ctx_t fullkey)
{
    ccrsa_priv_ctx_t privkey = ccrsa_ctx_private(fullkey);
    
    const cc_size np = cczp_n(ccrsa_ctx_private_zp(privkey));
    const cc_size nq = cczp_n(ccrsa_ctx_private_zq(privkey));
    
    size_t m_size = ccn_write_int_size(ccrsa_ctx_n(fullkey), ccrsa_ctx_m(fullkey));
    size_t e_size = ccn_write_int_size(ccrsa_ctx_n(fullkey), ccrsa_ctx_e(fullkey));
    size_t d_size = ccn_write_int_size(ccrsa_ctx_n(fullkey), ccrsa_ctx_d(fullkey));
    
    size_t p_size = ccn_write_int_size(np, cczp_prime(ccrsa_ctx_private_zp(privkey)));
    size_t q_size = ccn_write_int_size(nq, cczp_prime(ccrsa_ctx_private_zq(privkey)));
    
    size_t dp_size = ccn_write_int_size(np, ccrsa_ctx_private_dp(privkey));
    size_t dq_size = ccn_write_int_size(nq, ccrsa_ctx_private_dq(privkey));
    
    size_t qinv_size = ccn_write_int_size(np, ccrsa_ctx_private_qinv(privkey));
    
    const size_t seq_size = 3 +
    DERLengthOfItem(ASN1_INTEGER, m_size) +
    DERLengthOfItem(ASN1_INTEGER, e_size) +
    DERLengthOfItem(ASN1_INTEGER, d_size) +
    DERLengthOfItem(ASN1_INTEGER, p_size) +
    DERLengthOfItem(ASN1_INTEGER, q_size) +
    DERLengthOfItem(ASN1_INTEGER, dp_size) +
    DERLengthOfItem(ASN1_INTEGER, dq_size) +
    DERLengthOfItem(ASN1_INTEGER, qinv_size);
    
    const size_t result_size = DERLengthOfItem(ASN1_SEQUENCE, seq_size);
    
	CFMutableDataRef pkcs1 = CFDataCreateMutable(allocator, result_size);
    
    if (pkcs1 == NULL)
        return NULL;
    
	CFDataSetLength(pkcs1, result_size);
    
    uint8_t *bytes = CFDataGetMutableBytePtr(pkcs1);
    
    *bytes++ = ASN1_CONSTR_SEQUENCE;
    
    DERSize itemLength = 4;
    DEREncodeLength(seq_size, bytes, &itemLength);
    bytes += itemLength;
    
    *bytes++ = ASN1_INTEGER;
    *bytes++ = 0x01;
    *bytes++ = 0x00;
    
    ccasn_encode_int(ccrsa_ctx_n(fullkey), ccrsa_ctx_m(fullkey), m_size, &bytes);
    ccasn_encode_int(ccrsa_ctx_n(fullkey), ccrsa_ctx_e(fullkey), e_size, &bytes);
    ccasn_encode_int(ccrsa_ctx_n(fullkey), ccrsa_ctx_d(fullkey), d_size, &bytes);
    
    ccasn_encode_int(np, cczp_prime(ccrsa_ctx_private_zp(privkey)), p_size, &bytes);
    ccasn_encode_int(nq, cczp_prime(ccrsa_ctx_private_zq(privkey)), q_size, &bytes);
    ccasn_encode_int(np, ccrsa_ctx_private_dp(privkey), dp_size, &bytes);
    ccasn_encode_int(nq, ccrsa_ctx_private_dq(privkey), dq_size, &bytes);
    ccasn_encode_int(np, ccrsa_ctx_private_qinv(privkey), qinv_size, &bytes);
    
    return pkcs1;
}

static CFDataRef SecRSAPrivateKeyCopyPKCS1(SecKeyRef key)
{
    ccrsa_full_ctx_t fullkey;
    fullkey.full = key->key;
    
	CFAllocatorRef allocator = CFGetAllocator(key);
    return SecRSAPrivateKeyCreatePKCS1(allocator, fullkey);
}

static OSStatus SecRSAPrivateKeyCopyPublicSerialization(SecKeyRef key, CFDataRef* serialized)
{
    ccrsa_full_ctx_t fullkey;
    fullkey.full = key->key;
    
	CFAllocatorRef allocator = CFGetAllocator(key);
    *serialized = SecRSAPublicKeyCreatePKCS1(allocator, fullkey);
    
    if (NULL == *serialized)
        return errSecDecode;
    else
        return errSecSuccess;
}


static CFDictionaryRef SecRSAPrivateKeyCopyAttributeDictionary(SecKeyRef key) {
	CFDictionaryRef dict = NULL;
	CFDataRef fullKeyBlob = NULL;
    
	/* PKCS1 encode the key pair. */
	fullKeyBlob = SecRSAPrivateKeyCopyPKCS1(key);
    require(fullKeyBlob, errOut);
    
	dict = SecKeyGeneratePrivateAttributeDictionary(key, kSecAttrKeyTypeRSA, fullKeyBlob);
    
errOut:
	CFReleaseSafe(fullKeyBlob);
    
	return dict;
}

static CFStringRef SecRSAPrivateKeyCopyDescription(SecKeyRef key){
    
	return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR( "<SecKeyRef algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, addr: %p>"), SecKeyGetAlgorithmID(key), key->key_class->name, key->key_class->version, (8*SecKeyGetBlockSize(key)), key);
    
}
SecKeyDescriptor kSecRSAPrivateKeyDescriptor = {
    kSecKeyDescriptorVersion,
    "RSAPrivateKey",
    ccrsa_full_ctx_size(kMaximumRSAKeyBytes), /* extraBytes */
    SecRSAPrivateKeyInit,
    SecRSAPrivateKeyDestroy,
    SecRSAPrivateKeyRawSign,
    NULL, /* SecKeyRawVerifyMethod */
    NULL, /* SecKeyEncryptMethod */
    SecRSAPrivateKeyRawDecrypt,
    NULL, /* SecKeyComputeMethod */
    SecRSAPrivateKeyBlockSize,
	SecRSAPrivateKeyCopyAttributeDictionary,
    SecRSAPrivateKeyCopyDescription,
    NULL,
    SecRSAPrivateKeyCopyPublicSerialization,
    NULL,
    NULL
};

/* Private Key API functions. */
SecKeyRef SecKeyCreateRSAPrivateKey(CFAllocatorRef allocator,
                                    const uint8_t *keyData, CFIndex keyDataLength,
                                    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecRSAPrivateKeyDescriptor, keyData,
                        keyDataLength, encoding);
}


OSStatus SecRSAKeyGeneratePair(CFDictionaryRef parameters,
                               SecKeyRef *rsaPublicKey, SecKeyRef *rsaPrivateKey) {
    OSStatus status = errSecParam;
    
    CFAllocatorRef allocator = NULL; /* @@@ get from parameters. */
    
    SecKeyRef pubKey = NULL;
    SecKeyRef privKey = SecKeyCreate(allocator, &kSecRSAPrivateKeyDescriptor,
                                     (const void*) parameters, 0, kSecGenerateKey);
    
    require(privKey, errOut);
    
	/* Create SecKeyRef's from the pkcs1 encoded keys. */
    pubKey = SecKeyCreate(allocator, &kSecRSAPublicKeyDescriptor,
                          privKey->key, 0, kSecExtractPublicFromPrivate);
    
    require(pubKey, errOut);
    
    if (rsaPublicKey) {
        *rsaPublicKey = pubKey;
        pubKey = NULL;
    }
    if (rsaPrivateKey) {
        *rsaPrivateKey = privKey;
        privKey = NULL;
    }
    
    status = errSecSuccess;
    
errOut:
    CFReleaseSafe(pubKey);
    CFReleaseSafe(privKey);
    
	return status;
}
