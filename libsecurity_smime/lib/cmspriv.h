/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * Interfaces of the CMS implementation.
 */

#ifndef _CMSPRIV_H_
#define _CMSPRIV_H_

#include <Security/SecTrust.h>
#include "cmstpriv.h"

/************************************************************************/
SEC_BEGIN_PROTOS


/************************************************************************
 * cmsutil.c - CMS misc utility functions
 ************************************************************************/


/*
 * SecCmsArraySortByDER - sort array of objects by objects' DER encoding
 *
 * make sure that the order of the objects guarantees valid DER (which must be
 * in lexigraphically ascending order for a SET OF); if reordering is necessary it
 * will be done in place (in objs).
 */
extern OSStatus
SecCmsArraySortByDER(void **objs, const SecAsn1Template *objtemplate, void **objs2);

/*
 * SecCmsUtilDERCompare - for use with SecCmsArraySort to
 *  sort arrays of CSSM_DATAs containing DER
 */
extern int
SecCmsUtilDERCompare(void *a, void *b);

/*
 * SecCmsAlgArrayGetIndexByAlgID - find a specific algorithm in an array of 
 * algorithms.
 *
 * algorithmArray - array of algorithm IDs
 * algid - algorithmid of algorithm to pick
 *
 * Returns:
 *  An integer containing the index of the algorithm in the array or -1 if 
 *  algorithm was not found.
 */
extern int
SecCmsAlgArrayGetIndexByAlgID(SECAlgorithmID **algorithmArray, SECAlgorithmID *algid);

/*
 * SecCmsAlgArrayGetIndexByAlgID - find a specific algorithm in an array of 
 * algorithms.
 *
 * algorithmArray - array of algorithm IDs
 * algiddata - id of algorithm to pick
 *
 * Returns:
 *  An integer containing the index of the algorithm in the array or -1 if 
 *  algorithm was not found.
 */
extern int
SecCmsAlgArrayGetIndexByAlgTag(SECAlgorithmID **algorithmArray, SECOidTag algtag);

extern CSSM_CC_HANDLE
SecCmsUtilGetHashObjByAlgID(SECAlgorithmID *algid);

/*
 * XXX I would *really* like to not have to do this, but the current
 * signing interface gives me little choice.
 */
extern SECOidTag
SecCmsUtilMakeSignatureAlgorithm(SECOidTag hashalg, SECOidTag encalg);

extern const SecAsn1Template *
SecCmsUtilGetTemplateByTypeTag(SECOidTag type);

extern size_t
SecCmsUtilGetSizeByTypeTag(SECOidTag type);

extern SecCmsContentInfoRef
SecCmsContentGetContentInfo(void *msg, SECOidTag type);

/************************************************************************
* cmsmessage.c - CMS message methods
************************************************************************/

/*!
@function
 @abstract Set up a CMS message object for encoding or decoding.
 @discussion used internally.
 @param cmsg Pointer to a SecCmsMessage object
 @param pwfn callback function for getting token password for enveloped
 data content with a password recipient.
 @param pwfn_arg first argument passed to pwfn when it is called.
 @param encrypt_key_cb callback function for getting bulk key for encryptedData content.
 @param encrypt_key_cb_arg first argument passed to encrypt_key_cb when it is
 called.
 @param detached_digestalgs digest algorithms in detached_digests
 @param detached_digests digests from detached content (one for every element
                                                        in detached_digestalgs).
 */
extern void
SecCmsMessageSetEncodingParams(SecCmsMessageRef cmsg,
                               PK11PasswordFunc pwfn, void *pwfn_arg,
                               SecCmsGetDecryptKeyCallback encrypt_key_cb, void *encrypt_key_cb_arg,
                               SECAlgorithmID **detached_digestalgs, CSSM_DATA_PTR *detached_digests);

extern void
SecCmsMessageSetTSACallback(SecCmsMessageRef cmsg, SecCmsTSACallback tsaCallback);

extern void
SecCmsMessageSetTSAContext(SecCmsMessageRef cmsg, const void *tsaContext);   //CFTypeRef

/************************************************************************
 * cmscinfo.c - CMS contentInfo methods
 ************************************************************************/

/*!
    Destroy a CMS contentInfo and all of its sub-pieces.
    @param cinfo The contentInfo object to destroy.
 */
extern void
SecCmsContentInfoDestroy(SecCmsContentInfoRef cinfo);

/*
 * SecCmsContentInfoSetContent - set cinfo's content type & content to CMS object
 */
extern OSStatus
SecCmsContentInfoSetContent(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, SECOidTag type, void *ptr);


/************************************************************************
 * cmssigdata.c - CMS signedData methods
 ************************************************************************/

extern OSStatus
SecCmsSignedDataSetDigestValue(SecCmsSignedDataRef sigd,
				SECOidTag digestalgtag,
				CSSM_DATA_PTR digestdata);

extern OSStatus
SecCmsSignedDataAddDigest(SecArenaPoolRef pool,
				SecCmsSignedDataRef sigd,
				SECOidTag digestalgtag,
				CSSM_DATA_PTR digest);

extern CSSM_DATA_PTR
SecCmsSignedDataGetDigestByAlgTag(SecCmsSignedDataRef sigd, SECOidTag algtag);

extern CSSM_DATA_PTR
SecCmsSignedDataGetDigestValue(SecCmsSignedDataRef sigd, SECOidTag digestalgtag);

/*
 * SecCmsSignedDataEncodeBeforeStart - do all the necessary things to a SignedData
 *     before start of encoding.
 *
 * In detail:
 *  - find out about the right value to put into sigd->version
 *  - come up with a list of digestAlgorithms (which should be the union of the algorithms
 *         in the signerinfos).
 *         If we happen to have a pre-set list of algorithms (and digest values!), we
 *         check if we have all the signerinfos' algorithms. If not, this is an error.
 */
extern OSStatus
SecCmsSignedDataEncodeBeforeStart(SecCmsSignedDataRef sigd);

extern OSStatus
SecCmsSignedDataEncodeBeforeData(SecCmsSignedDataRef sigd);

/*
 * SecCmsSignedDataEncodeAfterData - do all the necessary things to a SignedData
 *     after all the encapsulated data was passed through the encoder.
 *
 * In detail:
 *  - create the signatures in all the SignerInfos
 *
 * Please note that nothing is done to the Certificates and CRLs in the message - this
 * is entirely the responsibility of our callers.
 */
extern OSStatus
SecCmsSignedDataEncodeAfterData(SecCmsSignedDataRef sigd);

extern OSStatus
SecCmsSignedDataDecodeBeforeData(SecCmsSignedDataRef sigd);

/*
 * SecCmsSignedDataDecodeAfterData - do all the necessary things to a SignedData
 *     after all the encapsulated data was passed through the decoder.
 */
extern OSStatus
SecCmsSignedDataDecodeAfterData(SecCmsSignedDataRef sigd);

/*
 * SecCmsSignedDataDecodeAfterEnd - do all the necessary things to a SignedData
 *     after all decoding is finished.
 */
extern OSStatus
SecCmsSignedDataDecodeAfterEnd(SecCmsSignedDataRef sigd);

/*
 * Get SecCmsSignedDataRawCerts - obtain raw certs as a NULL_terminated array 
 * of pointers.
 */
extern OSStatus SecCmsSignedDataRawCerts(SecCmsSignedDataRef sigd,
    CSSM_DATA_PTR **rawCerts);

/************************************************************************
 * cmssiginfo.c - CMS signerInfo methods
 ************************************************************************/

/*
 * SecCmsSignerInfoSign - sign something
 *
 */
extern OSStatus
SecCmsSignerInfoSign(SecCmsSignerInfoRef signerinfo, CSSM_DATA_PTR digest, CSSM_DATA_PTR contentType);

/*
 * If trustRef is NULL the cert chain is verified and the VerificationStatus is set accordingly.
 * Otherwise a SecTrust object is returned for the caller to evaluate using SecTrustEvaluate().
 */
extern OSStatus
SecCmsSignerInfoVerifyCertificate(SecCmsSignerInfoRef signerinfo, SecKeychainRef keychainOrArray,
				  CFTypeRef policies, SecTrustRef *trustRef);

/*
 * SecCmsSignerInfoVerify - verify the signature of a single SignerInfo
 *
 * Just verifies the signature. The assumption is that verification of the certificate
 * is done already.
 */
extern OSStatus
SecCmsSignerInfoVerify(SecCmsSignerInfoRef signerinfo, CSSM_DATA_PTR digest, CSSM_DATA_PTR contentType);

/*
 * SecCmsSignerInfoAddAuthAttr - add an attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo". 
 */
extern OSStatus
SecCmsSignerInfoAddAuthAttr(SecCmsSignerInfoRef signerinfo, SecCmsAttribute *attr);

/*
 * SecCmsSignerInfoAddUnauthAttr - add an attribute to the
 * unauthenticated attributes of "signerinfo". 
 */
extern OSStatus
SecCmsSignerInfoAddUnauthAttr(SecCmsSignerInfoRef signerinfo, SecCmsAttribute *attr);

extern int
SecCmsSignerInfoGetVersion(SecCmsSignerInfoRef signerinfo);

/* 
 * Determine whether Microsoft ECDSA compatibility mode is enabled. 
 * See comments in SecCmsSignerInfo.h for details. 
 * Implemented in siginfoUtils.cpp for access to C++ Dictionary class. 
 */
extern bool
SecCmsMsEcdsaCompatMode();


/************************************************************************
 * cmsenvdata.c - CMS envelopedData methods
 ************************************************************************/

/*
 * SecCmsEnvelopedDataEncodeBeforeStart - prepare this envelopedData for encoding
 *
 * at this point, we need
 * - recipientinfos set up with recipient's certificates
 * - a content encryption algorithm (if none, 3DES will be used)
 *
 * this function will generate a random content encryption key (aka bulk key),
 * initialize the recipientinfos with certificate identification and wrap the bulk key
 * using the proper algorithm for every certificiate.
 * it will finally set the bulk algorithm and key so that the encode step can find it.
 */
extern OSStatus
SecCmsEnvelopedDataEncodeBeforeStart(SecCmsEnvelopedDataRef envd);

/*
 * SecCmsEnvelopedDataEncodeBeforeData - set up encryption
 */
extern OSStatus
SecCmsEnvelopedDataEncodeBeforeData(SecCmsEnvelopedDataRef envd);

/*
 * SecCmsEnvelopedDataEncodeAfterData - finalize this envelopedData for encoding
 */
extern OSStatus
SecCmsEnvelopedDataEncodeAfterData(SecCmsEnvelopedDataRef envd);

/*
 * SecCmsEnvelopedDataDecodeBeforeData - find our recipientinfo, 
 * derive bulk key & set up our contentinfo
 */
extern OSStatus
SecCmsEnvelopedDataDecodeBeforeData(SecCmsEnvelopedDataRef envd);

/*
 * SecCmsEnvelopedDataDecodeAfterData - finish decrypting this envelopedData's content
 */
extern OSStatus
SecCmsEnvelopedDataDecodeAfterData(SecCmsEnvelopedDataRef envd);

/*
 * SecCmsEnvelopedDataDecodeAfterEnd - finish decoding this envelopedData
 */
extern OSStatus
SecCmsEnvelopedDataDecodeAfterEnd(SecCmsEnvelopedDataRef envd);


/************************************************************************
 * cmsrecinfo.c - CMS recipientInfo methods
 ************************************************************************/

extern int
SecCmsRecipientInfoGetVersion(SecCmsRecipientInfoRef ri);

extern CSSM_DATA_PTR
SecCmsRecipientInfoGetEncryptedKey(SecCmsRecipientInfoRef ri, int subIndex);


extern SECOidTag
SecCmsRecipientInfoGetKeyEncryptionAlgorithmTag(SecCmsRecipientInfoRef ri);

extern OSStatus
SecCmsRecipientInfoWrapBulkKey(SecCmsRecipientInfoRef ri, SecSymmetricKeyRef bulkkey, SECOidTag bulkalgtag);

extern SecSymmetricKeyRef
SecCmsRecipientInfoUnwrapBulkKey(SecCmsRecipientInfoRef ri, int subIndex,
		SecCertificateRef cert, SecPrivateKeyRef privkey, SECOidTag bulkalgtag);


/************************************************************************
 * cmsencdata.c - CMS encryptedData methods
 ************************************************************************/

/*
 * SecCmsEncryptedDataEncodeBeforeStart - do all the necessary things to a EncryptedData
 *     before encoding begins.
 *
 * In particular:
 *  - set the correct version value.
 *  - get the encryption key
 */
extern OSStatus
SecCmsEncryptedDataEncodeBeforeStart(SecCmsEncryptedDataRef encd);

/*
 * SecCmsEncryptedDataEncodeBeforeData - set up encryption
 */
extern OSStatus
SecCmsEncryptedDataEncodeBeforeData(SecCmsEncryptedDataRef encd);

/*
 * SecCmsEncryptedDataEncodeAfterData - finalize this encryptedData for encoding
 */
extern OSStatus
SecCmsEncryptedDataEncodeAfterData(SecCmsEncryptedDataRef encd);

/*
 * SecCmsEncryptedDataDecodeBeforeData - find bulk key & set up decryption
 */
extern OSStatus
SecCmsEncryptedDataDecodeBeforeData(SecCmsEncryptedDataRef encd);

/*
 * SecCmsEncryptedDataDecodeAfterData - finish decrypting this encryptedData's content
 */
extern OSStatus
SecCmsEncryptedDataDecodeAfterData(SecCmsEncryptedDataRef encd);

/*
 * SecCmsEncryptedDataDecodeAfterEnd - finish decoding this encryptedData
 */
extern OSStatus
SecCmsEncryptedDataDecodeAfterEnd(SecCmsEncryptedDataRef encd);


/************************************************************************
 * cmsdigdata.c - CMS encryptedData methods
 ************************************************************************/

/*
 * SecCmsDigestedDataEncodeBeforeStart - do all the necessary things to a DigestedData
 *     before encoding begins.
 *
 * In particular:
 *  - set the right version number. The contentInfo's content type must be set up already.
 */
extern OSStatus
SecCmsDigestedDataEncodeBeforeStart(SecCmsDigestedDataRef digd);

/*
 * SecCmsDigestedDataEncodeBeforeData - do all the necessary things to a DigestedData
 *     before the encapsulated data is passed through the encoder.
 *
 * In detail:
 *  - set up the digests if necessary
 */
extern OSStatus
SecCmsDigestedDataEncodeBeforeData(SecCmsDigestedDataRef digd);

/*
 * SecCmsDigestedDataEncodeAfterData - do all the necessary things to a DigestedData
 *     after all the encapsulated data was passed through the encoder.
 *
 * In detail:
 *  - finish the digests
 */
extern OSStatus
SecCmsDigestedDataEncodeAfterData(SecCmsDigestedDataRef digd);

/*
 * SecCmsDigestedDataDecodeBeforeData - do all the necessary things to a DigestedData
 *     before the encapsulated data is passed through the encoder.
 *
 * In detail:
 *  - set up the digests if necessary
 */
extern OSStatus
SecCmsDigestedDataDecodeBeforeData(SecCmsDigestedDataRef digd);

/*
 * SecCmsDigestedDataDecodeAfterData - do all the necessary things to a DigestedData
 *     after all the encapsulated data was passed through the encoder.
 *
 * In detail:
 *  - finish the digests
 */
extern OSStatus
SecCmsDigestedDataDecodeAfterData(SecCmsDigestedDataRef digd);

/*
 * SecCmsDigestedDataDecodeAfterEnd - finalize a digestedData.
 *
 * In detail:
 *  - check the digests for equality
 */
extern OSStatus
SecCmsDigestedDataDecodeAfterEnd(SecCmsDigestedDataRef digd);


/************************************************************************
 * cmsdigest.c - CMS encryptedData methods
 ************************************************************************/

/*
 * SecCmsDigestContextStartSingle - same as SecCmsDigestContextStartMultiple, but
 *  only one algorithm.
 */
extern SecCmsDigestContextRef
SecCmsDigestContextStartSingle(SECAlgorithmID *digestalg);

/*
 * SecCmsDigestContextFinishSingle - same as SecCmsDigestContextFinishMultiple,
 *  but for one digest.
 */
extern OSStatus
SecCmsDigestContextFinishSingle(SecCmsDigestContextRef cmsdigcx, SecArenaPoolRef arena,
			    CSSM_DATA_PTR digest);


/************************************************************************/
SEC_END_PROTOS

#endif /* _CMSPRIV_H_ */
