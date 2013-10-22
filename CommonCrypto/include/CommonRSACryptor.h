/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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

#ifndef _CC_RSACRYPTOR_H_
#define _CC_RSACRYPTOR_H_

#include <Availability.h>

#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigestSPI.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
    @typedef    CCRSACryptorRef
    @abstract   Opaque reference to a CCRSACryptor object.
 */

typedef struct _CCRSACryptor *CCRSACryptorRef;


/*
 	RSA Key Types
 */

enum {
    ccRSAKeyPublic          = 0,
    ccRSAKeyPrivate         = 1,
    ccRSABlankPublicKey     = 97,
    ccRSABlankPrivateKey    = 98,
    ccRSABadKey             = 99,
};
typedef uint32_t CCRSAKeyType;

/*
	Padding for Asymmetric ciphers
*/

enum {
	ccPaddingNone		= 1000,
	ccPKCS1Padding		= 1001,
	ccOAEPPadding		= 1002,
	ccX931Padding		= 1003, // Work in Progress - don't use.
    ccPKCS1PaddingRaw   = 1004,
};
typedef uint32_t CCAsymmetricPadding;
    
// The definition below will be removed.
#define CCAsymetricPadding CCAsymmetricPadding

/*
	Additional CCCryptorStatus for signature verification failure.
 */

enum {
    kCCNotVerified    = -4306
};

/*!
	@discussion
    
    Key sizes for this set of interfaces must be between 1024 and 4096 bits. 
    The key size must also be evenly divisible by 32.
*/

/*!
    @function   CCRSACryptorGeneratePair
    @abstract   Generate an RSA public and private key. 
    
	@param      keysize     Example sizes for RSA keys are: 512, 768, 1024, 2048.

	@param      e           The "e" value (public key). Must be odd; 65537 or larger
    
    @param      publicKey	A (required) pointer for the returned public CCRSACryptorRef.
    
    @param      privateKey	A (required) pointer for the returned private CCRSACryptorRef.
    
                            
    @result     Possible error returns are kCCParamError and kCCMemoryFailure.
*/

CCCryptorStatus CCRSACryptorGeneratePair(
	size_t keysize, 
    uint32_t e,
	CCRSACryptorRef *publicKey, 
    CCRSACryptorRef *privateKey)
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);
    

/*!
    @function   CCRSACryptorGetPublicKeyFromPrivateKey
    @abstract   Create an RSA public key from a full private key. 
    
    @param      privateKey		A pointer to a private CCRSACryptorRef.
    @result     returns either a valid public key CCRSACryptorRef or NULL.
 */
    
CCRSACryptorRef CCRSACryptorGetPublicKeyFromPrivateKey(CCRSACryptorRef privkey)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


/*!
    @function   CCRSACryptorImport
    @abstract   Import an RSA key from data. This imports public or private
    			keys in PKCS#1 format.
    
    @param      keyPackage		The data package containing the encoded key. 

	@param      keyPackageLen   The length of the encoded key package. 
	
    @param      key				A CCRSACryptorRef of the decoded key. 

    @result     Possible error returns are kCCParamError and kCCMemoryFailure.
*/

CCCryptorStatus CCRSACryptorImport( const void *keyPackage, 
                                    size_t keyPackageLen, 
                                    CCRSACryptorRef *key)
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);

/*!
 	@function   CCRSACryptorExport
 	@abstract   Import an RSA key from data. This exports public or private
 				keys in PKCS#1 format.
 
 	@param      key				The CCRSACryptorRef of the key to encode. 

    @param      keyPackage		The data package in which to put the encoded key. 
 
 	@param      keyPackageLen   A pointer to the length of the encoded key 
    							package.  This is an in/out parameter.
 
 
 	@result     Possible error returns are kCCParamError and kCCMemoryFailure.
 */
 
CCCryptorStatus CCRSACryptorExport( CCRSACryptorRef key, 
									void *out, 
                                    size_t *outLen)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


/*!
 	@function   CCRSAGetKeyType
 	@abstract   Determine whether a CCRSACryptorRef is public or private

 	@param      key				The CCRSACryptorRef. 
 	@result     Return values are ccRSAKeyPublic, ccRSAKeyPrivate, or ccRSABadKey

*/

CCRSAKeyType CCRSAGetKeyType(CCRSACryptorRef key)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);

/*!
 	@function   CCRSAGetKeySize
 	@abstract   Return the key size

 	@param      key				The CCRSACryptorRef. 
 	@result     Returns the keysize in bits or kCCParamError.

*/

int CCRSAGetKeySize(CCRSACryptorRef key)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


/*!
     @function   CCRSACryptorRelease
     @abstract   Clear and release a CCRSACryptorRef.
     
     @param      key	The CCRSACryptorRef of the key to release. 
*/


void CCRSACryptorRelease(CCRSACryptorRef key)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);
    
/*!
    @function   CCRSACryptorSign

    @abstract   Compute a signature for data with an RSA private key. 
    
    @param      privateKey		A pointer to a private CCRSACryptorRef.

	@param		padding			A selector for the padding to be used.

    @param      hashToSign		A pointer to the bytes of the value to be signed. 

 	@param      hashSignLen		Length of data to be signed. 
 	
    @param      digestType		The digest algorithm to use (See CommonDigestSPI.h). 
	
 	@param      saltLen			Length of salt to use for the signature. 
    
    @param      sig				The signature bytes. 

	@param      sigLen			A pointer to the length of signature material.
    							This is an in/out parameter value.     
                                
    @result     Possible error returns are kCCParamError and kCCMemoryFailure.
*/


CCCryptorStatus 
CCRSACryptorSign(	CCRSACryptorRef privateKey,
					CCAsymmetricPadding padding, 
                 	const void *hashToSign, 
                    size_t hashSignLen,
                 	CCDigestAlgorithm digestType,
                    size_t saltLen,
                 	void *signedData, 
                    size_t *signedDataLen)
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


/*!
    @function   CCRSACryptorVerify

	@abstract   Verify a signature for data with an RSA private key. 
    
    @param      publicKey		A pointer to a public CCRSACryptorRef.

	@param		padding			A selector for the padding to be used.
    
 	@param      hash			A pointer to the bytes of the hash of the data. 
 
	@param      hashLen			Length of hash. 
 
 	@param      digestType		The digest algorithm to use (See CommonDigestSPI.h). 
 
 	@param      saltLen			Length of salt to use for the signature. 
 
    @param      signedData		The bytes of the signature to be verified. 

	@param      signedDataLen	Length of data associated with the signature. 
	
                                
    @result     Possible error returns are kCCParamError, kCCMemoryFailure 
				or kCCNotVerified.
*/

CCCryptorStatus 
CCRSACryptorVerify(	CCRSACryptorRef publicKey, 
					CCAsymmetricPadding padding,
                   	const void *hash, 
                    size_t hashLen, 
                   	CCDigestAlgorithm digestType, 
                    size_t saltLen,
                   	const void *signedData, 
                    size_t signedDataLen)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);



/*!
    @function   CCRSACryptorEncrypt

	@abstract   Encrypt data with an RSA public key. 
    
    @param      publicKey		A pointer to a public CCRSACryptorRef.

	@param		padding			A selector for the padding to be used.

    @param      plainText		A pointer to the data to be encrypted.

	@param      plainTextLen	Length of data to be encrypted. 
	
    @param      cipherText		The encrypted byte result. 

	@param      cipherTextLen	Length of encrypted bytes.     
 
 	@param      tagData			tag to be included in the encryption. 
 
	@param      tagDataLen		Length of tag bytes.     
 
 	@param      digestType		The digest algorithm to use (See CommonDigestSPI.h). 
    
    @result     Possible error returns are kCCParamError.
*/

CCCryptorStatus CCRSACryptorEncrypt( 
    CCRSACryptorRef publicKey, 
	CCAsymmetricPadding padding, 
	const void *plainText, 
    size_t plainTextLen, 
	void *cipherText, 
    size_t *cipherTextLen,
    const void *tagData, 
    size_t tagDataLen, 
    CCDigestAlgorithm digestType)
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


/*!
    @function   CCRSACryptorDecrypt

	@abstract   Decrypt data with an RSA private key. 
    
    @param      privateKey		A pointer to a private CCRSACryptorRef.

	@param		padding			A selector for the padding to be used.

	@param      cipherText		The encrypted bytes. 

	@param      cipherTextLen	Length of encrypted bytes.     

    @param      plainText		The decrypted data bytes.

	@param      plainTextLen	A pointer to the length of data decrypted.
    							This is an in/out parameter. 	
 	 
	@param      tagData			tag to be included in the encryption. 
 
	@param      tagDataLen		Length of tag bytes.     

	@param      digestType		The digest algorithm to use (See CommonDigestSPI.h). 
                                
    @result     Possible error returns are kCCParamError.
*/

CCCryptorStatus 
CCRSACryptorDecrypt(
	CCRSACryptorRef privateKey, 
    CCAsymmetricPadding padding, 
    const void *cipherText, 
    size_t cipherTextLen,
    void *plainText, 
    size_t *plainTextLen, 
    const void *tagData, 
    size_t tagDataLen, 
    CCDigestAlgorithm digestType)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);
    
/*!
 @function   CCRSACryptorDecodePayloadPKCS1
 
 @abstract   Decrypt data with an RSA public key, strip off PKCS1 padding and return the payload. 
 
 @param      publicKey		A pointer to a public CCRSACryptorRef.
  
 @param      cipherText		The encrypted bytes. 
 
 @param      cipherTextLen	Length of encrypted bytes.     
 
 @param      plainText		The decrypted data bytes.
 
 @param      plainTextLen	A pointer to the length of data decrypted.
 This is an in/out parameter. 	
 
 @param      digestType		The digest algorithm to use (See CommonDigestSPI.h). 
 
 @result     Possible error returns are kCCParamError.
 */

CCCryptorStatus 
CCRSACryptorDecodePayloadPKCS1(
                    CCRSACryptorRef publicKey, 
                    const void *cipherText, 
                    size_t cipherTextLen,
                    void *plainText, 
                    size_t *plainTextLen)
__OSX_AVAILABLE_STARTING(__MAC_NA, __IPHONE_5_0);

/*!
    @function   CCRSACryptorCrypt

	@abstract   En/Decrypt data with an RSA key. 

    @param      rsaKey		A pointer to a CCRSACryptorRef.

	@param      in          The input data. 

	@param      inLen       The input data length.     

	@param      out          The output data buffer. 

	@param      outLen       The output data buffer size.  This is an in-out
                            parameter.  When the function returns this is set
                            to the length of the result.

    @result     Possible error returns are kCCParamError, kCCDecodeError
                or kCCBufferTooSmall.

*/

CCCryptorStatus 
CCRSACryptorCrypt(
	CCRSACryptorRef rsaKey, 
    const void *in, 
    size_t inLen, 
    void *out, 
    size_t *outLen)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);

/*!
    @function   CCRSAGetKeyComponents
	@abstract   En/Decrypt data with an RSA key. 
    @param      rsaKey		A pointer to a CCRSACryptorRef.
	@param      modulus     		The modulus in MSB format.
	@param      modulusLength     	The modulus data length. 	(in/out parameter)
    @param      exponent			The raw data bytes of the exponent.
	@param      exponentLength     	The exponent data length.	(in/out parameter)
    @param      p					The raw data bytes of the modulus factor P.
    								(ccRSAKeyPrivate only)
	@param      pLength     		The P data length.	(in/out parameter)
    @param      q					The raw data bytes of the modulus factor Q.
    								(ccRSAKeyPrivate only)
	@param      qLength     		The Q data length.	(in/out parameter)
    @result		If the function is successful (kCCSuccess)
*/

CCCryptorStatus
CCRSAGetKeyComponents(CCRSACryptorRef rsaKey, uint8_t *modulus, size_t *modulusLength, uint8_t *exponent, size_t *exponentLength,
	uint8_t *p, size_t *pLength, uint8_t *q, size_t *qLength)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);

/*!
    @function   CCRSACryptorCreateFromData
	@abstract   For FIPS CAVS testing we need the ability to create an RSA 
    			key from an exponent and Modulus.

	@param      keyType     		The type of key to create - ccRSAKeyPublic
    								 or ccRSAKeyPrivate.
	@param      modulus     		The modulus in MSB format.
	@param      modulusLength     	The modulus data length.
    @param      exponent			The raw data bytes of the exponent.
	@param      exponentLength     	The exponent data length.
    @param      p					The raw data bytes of the modulus factor P.
    								(ccRSAKeyPrivate only)
	@param      pLength     		The P data length.
    @param      q					The raw data bytes of the modulus factor Q.
    								(ccRSAKeyPrivate only)
	@param      qLength     		The Q data length.

    @result		If the function is successful (kCCSuccess) a RSACryptoRef is 
    			returned in the ref parameter.  All other errors result in
                kCCParamError.
*/

CCCryptorStatus
CCRSACryptorCreateFromData( CCRSAKeyType keyType, uint8_t *modulus, size_t modulusLength, uint8_t *exponent, size_t exponentLength,
 							uint8_t *p, size_t pLength, uint8_t *q, size_t qLength, CCRSACryptorRef *ref)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


CCCryptorStatus 
CCRSACryptorCreatePairFromData(uint32_t e, 
    uint8_t *xp1, size_t xp1Length,
    uint8_t *xp2, size_t xp2Length,
    uint8_t *xp, size_t xpLength,
    uint8_t *xq1, size_t xq1Length,
    uint8_t *xq2, size_t xq2Length,
    uint8_t *xq, size_t xqLength,
    CCRSACryptorRef *publicKey, 
    CCRSACryptorRef *privateKey,
    uint8_t *retp, size_t *retpLength,
    uint8_t *retq, size_t *retqLength,
    uint8_t *retm, size_t *retmLength,
    uint8_t *retd, size_t *retdLength)
__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_6_0);


#ifdef __cplusplus
}
#endif

#endif  /* _CC_RSACRYPTOR_H_ */
