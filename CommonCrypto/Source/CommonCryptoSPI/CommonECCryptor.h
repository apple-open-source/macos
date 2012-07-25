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

#ifndef _CC_ECCRYPTOR_H_
#define _CC_ECCRYPTOR_H_

#include <Availability.h>

#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include "CommonRSACryptor.h"


#ifdef __cplusplus
extern "C" {
#endif

/*!
    @typedef    CCECCryptorRef
    @abstract   Opaque reference to a CCECCryptor object.
 */

typedef struct _CCECCryptor *CCECCryptorRef;


/*
 	EC Key Types
 */

enum {
    ccECKeyPublic		= 0,
    ccECKeyPrivate		= 1,
    ccECBlankPublicKey    = 97,
    ccECBlankPrivateKey   = 98,
    ccECBadKey          = 99,
};
typedef uint32_t CCECKeyType;

/*
 EC Key Import/Export Formats
 */

enum {
    kCCImportKeyBinary  = 0,
    kCCImportKeyDER		= 1,
};
typedef uint32_t CCECKeyExternalFormat;

/*!
	@discussion
    
    Key sizes for this set of interfaces must be between 128 and 384 bits. 
    The key size must also be evenly divisible by 8
*/

/*!
    @function   CCECCryptorGeneratePair
    @abstract   Generate an EC public and private key.  A curve will be chosen from
    			ECC-256 or ECC-384.
    
	@param      keysize     Must be between 192 and 521 (inclusive)
    
    @param      publicKey	A (required) pointer for the returned public CCECCryptorRef.
    
    @param      privateKey	A (required) pointer for the returned private CCECCryptorRef.
    
                            
    @result     Possible error returns are kCCParamError and kCCMemoryFailure.
*/

CCCryptorStatus 
CCECCryptorGeneratePair( size_t keysize, 
                         CCECCryptorRef *publicKey, 
                         CCECCryptorRef *privateKey)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);

/*!
     @function   CCECCryptorGetPublicKeyFromPrivateKey
     @abstract   Grab the parts from a private key to make a public key.
     
     @param      privateKey		A pointer to a private CCECCryptorRef.
     
     
     @result     Possible error returns are kCCParamError and kCCMemoryFailure.
 */

CCECCryptorRef 
CCECCryptorGetPublicKeyFromPrivateKey(CCECCryptorRef privateKey)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


/*!
    @function   CCECCryptorImportPublicKey
    @abstract   Import an Elliptic Curve public key from data. This imports public
    			keys in ANSI X.9.63 format.
    
    @param      keyPackage		The data package containing the encoded key. 

	@param      keyPackageLen   The length of the encoded key package. 
	
    @param      key				A CCECCryptorRef of the decoded key. 

    @result     Possible error returns are kCCParamError and kCCMemoryFailure.
*/
    

CCCryptorStatus CCECCryptorImportPublicKey( void *keyPackage, 
											size_t keyPackageLen, 
                                            CCECCryptorRef *key)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);
    
/*!
     @function   CCECCryptorImportKey
     @abstract   Import an Elliptic Curve public key from data.
 
     @param      format		The format in which the key is encoded. 
 
     @param      keyPackage		The data package containing the encoded key. 
     
     @param      keyPackageLen   The length of the encoded key package. 
     
     @param      keyType		The type of key to be imported (public or private). 

     @param      key				A CCECCryptorRef of the decoded key. 
     
     @result     Possible error returns are kCCParamError and kCCMemoryFailure.
 */

CCCryptorStatus CCECCryptorImportKey(CCECKeyExternalFormat format, void *keyPackage, size_t keyPackageLen, CCECKeyType keyType, CCECCryptorRef *key)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


/*!
 	@function   CCECCryptorExportPublicKey
 	@abstract   Export an Elliptic Curve public key from data. This exports public
 				keys in ANSI X.9.63 format.
 
 	@param      key				The CCECCryptorRef of the key to encode. 

    @param      out             The destination for the encoded key. 
 
 	@param      outLen          A pointer to the length of the encoded key.
    							This is an in/out parameter.
 
 
 	@result     Possible error returns are kCCParamError and kCCMemoryFailure.
 */
 
CCCryptorStatus CCECCryptorExportPublicKey( CCECCryptorRef key, 
											void *out, 
                                            size_t *outLen)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);
    
    
// We'll remove the   CCECCryptorExportPublicKey later - we like this better.  
CCCryptorStatus CCECCryptorExportKey(CCECKeyExternalFormat format, void *keyPackage, size_t *keyPackageLen, CCECKeyType keyType, CCECCryptorRef key)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


/*!
 	@function   CCECGetKeyType
 	@abstract   Determine whether a CCECCryptorRef is public or private

 	@param      key				The CCECCryptorRef. 
 	@result     Return values are ccECKeyPublic, ccECKeyPrivate, or ccECBadKey

*/

CCECKeyType CCECGetKeyType(CCECCryptorRef key)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);

/*!
 	@function   CCECGetKeySize
 	@abstract   Return the key size

 	@param      key				The CCECCryptorRef. 
 	@result     Returns the keysize in bits or kCCParamError.

*/

int CCECGetKeySize(CCECCryptorRef key)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


/*!
     @function   CCECCryptorRelease
     @abstract   Clear and release a CCECCryptorRef.
     
     @param      key	The CCECCryptorRef of the key to release. 
*/


void CCECCryptorRelease(CCECCryptorRef key)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);
    
/*!
    @function   CCECCryptorSignHash

    @abstract   Compute a signature for a hash with an EC private key. 
    
    @param      privateKey		A pointer to a private CCECCryptorRef.

    @param      hashToSign		A pointer to the bytes of the value to be signed. 

 	@param      hashSignLen		Length of data to be signed. 
 		    
    @param      signedData      The signature bytes. 

	@param      signedDataLen   A pointer to the length of signature material.
    							This is an in/out parameter value.     
                                
    @result     Possible error returns are kCCParamError and kCCMemoryFailure.
*/


CCCryptorStatus 
CCECCryptorSignHash( CCECCryptorRef privateKey, 
                 const void *hashToSign, 
                 size_t hashSignLen,
				 void *signedData, 
                 size_t *signedDataLen)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


/*!
    @function   CCECCryptorVerifyHash

	@abstract   Verify a signature for data with an EC private key. 
    
    @param      publicKey		A pointer to a public CCECCryptorRef.
    
 	@param      hash			A pointer to the bytes of the hash of the data. 
 
	@param      hashLen			Length of hash. 
   
    @param      signedData		The bytes of the signature to be verified. 

	@param      signedDataLen	Length of data associated with the signature. 

	@param		valid			An indicator whether the signature was valid	
                                
    @result     Possible error returns are kCCParamError, kCCMemoryFailure 
				or kCCNotVerified.
*/

CCCryptorStatus 
CCECCryptorVerifyHash(  CCECCryptorRef publicKey,
      				const void *hash, 
                    size_t hashLen, 
      				const void *signedData, 
                    size_t signedDataLen, 
                    uint32_t *valid)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);



/*!
    @function   CCECCryptorWrapKey

	@abstract   Encrypt data (wrap a symmetric key) with an EC public key. 
    
    @param      publicKey		A pointer to a public CCECCryptorRef.

    @param      plainText		A pointer to the data to be encrypted.

	@param      plainTextLen	Length of data to be encrypted. 
	
    @param      cipherText		The encrypted byte result. 

	@param      cipherTextLen	Length of encrypted bytes.     
   
 	@param      digestType		The digest algorithm to use (See CommonDigestSPI.h). 
    
    @result     Possible error returns are kCCParamError.
*/

CCCryptorStatus 
CCECCryptorWrapKey(CCECCryptorRef publicKey, 
                   const void *plainText, 
                   size_t plainTextLen, 
                   void *cipherText, 
                   size_t *cipherTextLen,
                   CCDigestAlg digestType)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


/*!
    @function   CCECCryptorUnwrapKey

	@abstract   Decrypt data (unwrap a symmetric key) with an EC private key. 
    
    @param      privateKey		A pointer to a private CCECCryptorRef.

	@param      cipherText		The encrypted bytes. 

	@param      cipherTextLen	Length of encrypted bytes.     

    @param      plainText		The decrypted data bytes.

	@param      plainTextLen	A pointer to the length of data decrypted.
    							This is an in/out parameter. 	
 	                                  
    @result     Possible error returns are kCCParamError.
*/

CCCryptorStatus 
CCECCryptorUnwrapKey(CCECCryptorRef privateKey, 
                     const void *cipherText, 
                     size_t cipherTextLen,
                     void *plainText, 
                     size_t *plainTextLen)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);
    

/*!
    @function   CCECCryptorComputeSharedSecret

	@abstract   Construct a Diffie-Hellman shared secret with a private and 
    			public ECC key. 

    @param      privateKey		A pointer to a private CCECCryptorRef.

 	@param      publicKey		A pointer to a public CCECCryptorRef (usually
    							obtained from the other party in the session.)
	
	@param      out          The output data buffer. 

	@param      outLen       The output data buffer size.  This is an in-out
                            parameter.  When the function returns this is set
                            to the length of the result.

    @result     Possible error returns are kCCParamError, kCCDecodeError
                or kCCBufferTooSmall.

*/

CCCryptorStatus 
CCECCryptorComputeSharedSecret( CCECCryptorRef privateKey, 
								CCECCryptorRef publicKey, 
                                void *out, 
                                size_t *outLen)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);
    
/*======================================================================================*/
/* Only for FIPS Testing                                                                */
/*======================================================================================*/
    
/*!
 @function   CCECCryptorGetKeyComponents
 @abstract   Get EC Public Key Parameters for FIPS tests
 
 @param      ecKey              The EC Key to deconstruct
 @param      keySize            The EC Keysize.
 @param      qX, qXLength       The pointer and length(return) for the X Parameter.
 @param      qY, qYLength       The pointer and length(return) for the Y Parameter.
 @param      d, dLength         The pointer and length(return) for the D (Private Key Only)
                                Parameter.
 
 @result    If the function is successful (kCCSuccess) the X and Y parameters contain the
            discreet public key point coordinate values.  If the key passed in is a Private 
            Key the D parameter will contain the private key.
            All other errors result in kCCParamError.
 */

CCCryptorStatus
CCECCryptorGetKeyComponents(CCECCryptorRef ecKey, size_t *keySize,
                            uint8_t *qX, size_t *qXLength, 
                            uint8_t *qY, size_t *qYLength,
                            uint8_t *d, size_t *dLength)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);


/*!
 @function   CCECCryptorCreateFromData
 @abstract   For FIPS CAVS testing we need the ability to create an EC 
             key from an X and Y parameter set.
 
 @param      keySize            The EC Keysize.
 @param      qX, qXLength       The pointer and length for the X Parameter.
 @param      qY, qYLength       The pointer and length for the Y Parameter.
 @param      ref                A pointer to the CCECCryptorRef to contain the result.
 @result    If the function is successful (kCCSuccess) a CCECCryptorRef is 
            returned in the ref parameter.  All other errors result in
            kCCParamError.
 */

CCCryptorStatus
CCECCryptorCreateFromData(size_t keySize,
                          uint8_t *qX, size_t qXLength, 
                          uint8_t *qY, size_t qYLength, 
                          CCECCryptorRef *ref)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);

/*!
 @function   CCECSignatureDecode
 @abstract   For FIPS CAVS testing we need the ability to get the binary S and R values
             from the DER signature blob.
 @param      SignedData, signedDataLen  The pointer and length of the DER formatted signature
 @param      r, rLength         The pointer and length for the R component (return value).
 @param      s, sLength         The pointer and length for the S component (return value).
 @result        If the function is successful (kCCSuccess) the r and s parameters have the
                individual values from the signature.
 */

CCCryptorStatus
CCECSignatureDecode(const void *signedData, size_t signedDataLen,
                    uint8_t *r, size_t *rLength,
                    uint8_t *s, size_t *sLength)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);

/*!
 @function   CCECSignatureEncode
 @abstract   For FIPS CAVS testing we need the ability to produce a DER formatted signature from
             discreet R and S components.
 @param      r, rLength         The pointer and length for the R component.
 @param      s, sLength         The pointer and length for the S component.
 @param      signedData, signedDataLen  The pointer and length of the DER formatted 
             signature ( return value)
 @result     If the function is successful (kCCSuccess) the signature is returned as a 
            DER-formatted blob. */
    
CCCryptorStatus
CCECSignatureEncode(uint8_t *r, size_t rLength,
                    uint8_t *s, size_t sLength,
                    void *signedData, size_t *signedDataLen)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_5_0);

#ifdef __cplusplus
}
#endif

#endif  /* _CC_ECCRYPTOR_H_ */
