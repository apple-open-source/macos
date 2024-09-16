//
//  CommonCollabKeyGen.h
//  CommonCrypto
//
//  Copyright (c) 2019 Apple Inc. All rights reserved.
//

#ifndef CommonCollabKeyGen_h
#define CommonCollabKeyGen_h

#include <CommonCrypto/CommonDigestSPI.h>
#include <CommonCrypto/CommonECCryptor.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 API for Collaborative Key Generation (CKG).

 The protocol defines two roles, contributor and owner, and allows these two
 parties to agree on:

   1) An EC key pair {d,P} where P = d*G (only the owner knows d)
   2) A symmetric key SK

 Neither the contributor nor the owner can bias P or SK.

 The contributor contributes to the key generation by committing to a scalar
 and nonce at the beginning. Upon receival of the owner's key share and nonce
 it will combine those with the values committed to previously to compute the
 shared public key P and symmetric secret SK.

 The owner incorporates the scalar and nonce the contributor committed to by
 combining those with its own key share to also compute the shared public key P
 and the symmetric secret SK. The key share is then sent to the contributor.
*/

typedef struct _CCCollabKeyGenContributor *CCCollabKeyGenContributorRef;
typedef struct _CCCollabKeyGenOwner *CCCollabKeyGenOwnerRef;

typedef struct _CCCKG2Contributor *CCCKG2ContributorRef;
typedef struct _CCCKG2Owner *CCCKG2OwnerRef;
typedef struct _CCCKG2Params *CCCKG2Params;

/*!
    @function   CCCKG2ParamsP224Sha256Version2

    @abstract   Returns CKG parameters for P-224 and SHA-256, protocol v2.

    @result     The CKG parameters.
*/
CCCKG2Params
CCCKG2ParamsP224Sha256Version2(void)
API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
    @function   CCCKG2GetCommitmentSize

    @abstract   Returns the size of a contributor commitment in bytes.

    @param      params     CKG parameters.

    @result     The commitment size or kCCParamError.
*/
int
CCCKG2GetCommitmentSize(CCCKG2Params params)
API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
    @function   CCCKG2GetShareSize

    @abstract   Returns the size of an owner share in bytes.

    @param      params     CKG parameters.

    @result     The share size or kCCParamError.
*/
int
CCCKG2GetShareSize(CCCKG2Params params)
API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
    @function   CCCKG2GetOpeningSize

    @abstract   Returns the size of a contributor opening in bytes.

    @param      params     CKG parameters.

    @result     The opening size or kCCParamError.
*/
int
CCCKG2GetOpeningSize(CCCKG2Params params)
API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
    @function   CCCKG2ContributorCreate

    @abstract   Creates a new CKG contributor context.

    @param      params     CKG parameters.

    @param      contrib    Pointer to a CCCKG2ContributorRef.

    @result     Returns kCCSuccess on success, and kCCParamError or kCCMemoryFailure on failure.
*/
CCCryptorStatus
CCCKG2ContributorCreate(CCCKG2Params params, CCCKG2ContributorRef *contrib)
API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
    @function   CCCKG2ContributorDestroy

    @abstract   Destroys a CKG contributor context.

    @param      contrib    The CCCKG2ContributorRef to destroy.
*/
void
CCCKG2ContributorDestroy(CCCKG2ContributorRef contrib)
API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
    @function   CCCKG2OwnerCreate

    @abstract   Creates a new CKG owner context.

    @param      params     CKG parameters.

    @param      owner    Pointer to a CCCKG2OwnerRef.

    @result     Returns kCCSuccess on success, and kCCParamError or kCCMemoryFailure on failure.
*/
CCCryptorStatus
CCCKG2OwnerCreate(CCCKG2Params params, CCCKG2OwnerRef *owner)
API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
    @function   CCCKG2OwnerDestroy

    @abstract   Destroys a CKG owner context.

    @param      owner      The CCCKG2OwnerRef to destroy.
*/
void
CCCKG2OwnerDestroy(CCCKG2OwnerRef owner)
API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
    @function   CCCKG2ContributorCommit

    @abstract   Generates a new contributor commitment.

    @param      contrib          A CCCKG2ContributorRef.

    @param      commitment       Commitment output buffer.

    @param      commitmentLen    Length of the commitment buffer (see CCCKG2GetCommitmentSize).

    @result     kCCSuccess on success, or an error code on failure.
*/
CCCryptorStatus
CCCKG2ContributorCommit(CCCKG2ContributorRef contrib,
                        void *commitment, size_t commitmentLen)
API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
    @function   CCCKG2OwnerGenerateShare

    @abstract   Takes a contributor commitment and generates a new owner share.

    @param      owner            A CCCKG2OwnerRef.

    @param      commitment       Contributor commitment buffer.

    @param      commitmentLen    Length of the commitment buffer (see CCCKG2GetCommitmentSize).

    @param      share            Share output buffer.

    @param      shareLen         Length of the share buffer (see CCCKG2GetShareSize).

    @result     kCCSuccess on success, or an error code on failure.
*/
CCCryptorStatus
CCCKG2OwnerGenerateShare(CCCKG2OwnerRef owner,
                         const void *commitment, size_t commitmentLen,
                         void *share, size_t shareLen)
API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
    @function   CCCKG2ContributorFinish

    @abstract   Takes an owner share and finishes the contributor protocol flow
                by opening the commitment and computing the public key P and
                the symmetric secret SK.

    @param      contrib          A CCCKG2ContributorRef.

    @param      share            Owner share buffer.

    @param      shareLen         Length of the share buffer (see CCCKG2GetShareSize).

    @param      opening          Opening output buffer.

    @param      openingLen       Length of the opening buffer (see CCCKG2GetOpeningSize).

    @param      sharedKey        Shared key output buffer (SK).

    @param      sharedKeyLen     Length of the shared key buffer.

    @param      publicKey        Pointer to a CCECCryptorRef (P).

    @result     kCCSuccess on success, or an error code on failure.
*/
CCCryptorStatus
CCCKG2ContributorFinish(CCCKG2ContributorRef contrib,
                        const void *share, size_t shareLen,
                        void *opening, size_t openingLen,
                        void *sharedKey, size_t sharedKeyLen,
                        CCECCryptorRef *publicKey)
API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
    @function   CCCKG2OwnerFinish

    @abstract   Takes a contributor opening and finishes the owner protocol
                flow by computing the public key P and the symmetric secret SK.

    @param      owner            A CCCKG2OwnerRef.

    @param      opening          Opening buffer.

    @param      openingLen       Length of the opening buffer (see CCCKG2GetOpeningSize).

    @param      sharedKey        Shared key output buffer (SK).

    @param      sharedKeyLen     Length of the shared key buffer.

    @param      privateKey       Pointer to a CCECCryptorRef (d,P).

    @result     kCCSuccess on success, or an error code on failure.
*/
CCCryptorStatus
CCCKG2OwnerFinish(CCCKG2OwnerRef owner,
                  const void *opening, size_t openingLen,
                  void *sharedKey, size_t sharedKeyLen,
                  CCECCryptorRef *privateKey)
API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
    @function   CCCKGGetCommitmentSize

    @abstract   Returns the size of a contributor commitment in bytes.

    @param      nbits      Number of bits of the chosen curve.

    @param      alg        Digest algorithm to use (See CommonDigestSPI.h).

    @result     The commitment size or kCCParamError.
*/
int
CCCKGGetCommitmentSize(size_t nbits, CCDigestAlgorithm alg)
API_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0));

/*!
    @function   CCCKGGetShareSize

    @abstract   Returns the size of an owner share in bytes.

    @param      nbits      Number of bits of the chosen curve.

    @param      alg        Digest algorithm to use (See CommonDigestSPI.h).

    @result     The share size or kCCParamError.
*/
int
CCCKGGetShareSize(size_t nbits, CCDigestAlgorithm alg)
API_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0));

/*!
    @function   CCCKGGetOpeningSize

    @abstract   Returns the size of a contributor opening in bytes.

    @param      nbits      Number of bits of the chosen curve.

    @param      alg        Digest algorithm to use (See CommonDigestSPI.h).

    @result     The opening size or kCCParamError.
*/
int
CCCKGGetOpeningSize(size_t nbits, CCDigestAlgorithm alg)
API_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0));

/*!
    @function   CCCKGContributorCreate

    @abstract   Creates a new CKG contributor context.

    @param      nbits      Number of bits of the chosen curve.

    @param      alg        Digest algorithm to use (See CommonDigestSPI.h).

    @param      contrib    Pointer to a CCCollabKeyGenContributorRef.

    @result     Returns kCCSuccess on success, and kCCParamError or kCCMemoryFailure on failure.
*/
CCCryptorStatus
CCCKGContributorCreate(size_t nbits, CCDigestAlgorithm alg, CCCollabKeyGenContributorRef *contrib)
API_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0));

/*!
    @function   CCCKGContributorDestroy

    @abstract   Destroys a CKG contributor context.

    @param      contrib    The CCCollabKeyGenContributorRef to destroy.
*/
void
CCCKGContributorDestroy(CCCollabKeyGenContributorRef contrib)
API_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0));

/*!
    @function   CCCKGOwnerCreate

    @abstract   Creates a new CKG owner context.

    @param      nbits      Number of bits of the chosen curve.

    @param      alg        Digest algorithm to use (See CommonDigestSPI.h).

    @param      owner    Pointer to a CCCollabKeyGenOwnerRef.

    @result     Returns kCCSuccess on success, and kCCParamError or kCCMemoryFailure on failure.
*/
CCCryptorStatus
CCCKGOwnerCreate(size_t nbits, CCDigestAlgorithm alg, CCCollabKeyGenOwnerRef *owner)
API_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0));

/*!
    @function   CCCKGOwnerDestroy

    @abstract   Destroys a CKG owner context.

    @param      owner      The CCCollabKeyGenOwnerRef to destroy.
*/
void
CCCKGOwnerDestroy(CCCollabKeyGenOwnerRef owner)
API_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0));

/*!
    @function   CCCKGContributorCommit

    @abstract   Generates a new contributor commitment.

    @param      contrib          A CCCollabKeyGenContributorRef.

    @param      commitment       Commitment output buffer.

    @param      commitmentLen    Length of the commitment buffer (see CCCKGGetCommitmentSize).

    @result     kCCSuccess on success, or an error code on failure.
*/
CCCryptorStatus
CCCKGContributorCommit(CCCollabKeyGenContributorRef contrib,
                       void *commitment, size_t commitmentLen)
API_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0));

/*!
    @function   CCCKGOwnerGenerateShare

    @abstract   Takes a contributor commitment and generates a new owner share.

    @param      owner            A CCCollabKeyGenOwnerRef.

    @param      commitment       Contributor commitment buffer.

    @param      commitmentLen    Length of the commitment buffer (see CCCKGGetCommitmentSize).

    @param      share            Share output buffer.

    @param      shareLen         Length of the share buffer (see CCCKGGetShareSize).

    @result     kCCSuccess on success, or an error code on failure.
*/
CCCryptorStatus
CCCKGOwnerGenerateShare(CCCollabKeyGenOwnerRef owner,
                        const void *commitment, size_t commitmentLen,
                        void *share, size_t shareLen)
API_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0));

/*!
    @function   CCCKGContributorFinish

    @abstract   Takes an owner share and finishes the contributor protocol flow
                by opening the commitment and computing the public key P and
                the symmetric secret SK.

    @param      contrib          A CCCollabKeyGenContributorRef.

    @param      share            Owner share buffer.

    @param      shareLen         Length of the share buffer (see CCCKGGetShareSize).

    @param      opening          Opening output buffer.

    @param      openingLen       Length of the opening buffer (see CCCKGGetOpeningSize).

    @param      sharedKey        Shared key output buffer (SK).

    @param      sharedKeyLen     Length of the shared key buffer.

    @param      publicKey        Pointer to a CCECCryptorRef (P).

    @result     kCCSuccess on success, or an error code on failure.
*/
CCCryptorStatus
CCCKGContributorFinish(CCCollabKeyGenContributorRef contrib,
                       const void *share, size_t shareLen,
                       void *opening, size_t openingLen,
                       void *sharedKey, size_t sharedKeyLen,
                       CCECCryptorRef *publicKey)
API_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0));

/*!
    @function   CCCKGOwnerFinish

    @abstract   Takes a contributor opening and finishes the owner protocol
                flow by computing the public key P and the symmetric secret SK.

    @param      owner            A CCCollabKeyGenOwnerRef.

    @param      opening          Opening buffer.

    @param      openingLen       Length of the opening buffer (see CCCKGGetOpeningSize).

    @param      sharedKey        Shared key output buffer (SK).

    @param      sharedKeyLen     Length of the shared key buffer.

    @param      privateKey       Pointer to a CCECCryptorRef (d,P).

    @result     kCCSuccess on success, or an error code on failure.
*/
CCCryptorStatus
CCCKGOwnerFinish(CCCollabKeyGenOwnerRef owner,
                 const void *opening, size_t openingLen,
                 void *sharedKey, size_t sharedKeyLen,
                 CCECCryptorRef *privateKey)
API_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0));

#ifdef __cplusplus
}
#endif

#endif /* CommonCollabKeyGen_h */
