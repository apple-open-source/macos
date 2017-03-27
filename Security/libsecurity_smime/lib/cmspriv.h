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
#include <security_asn1/seccomon.h> // SEC_BEGIN_PROTOS
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
 *  sort arrays of SecAsn1Items containing DER
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

extern void *SecCmsUtilGetHashObjByAlgID(SECAlgorithmID *algid);

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
 */
extern void
SecCmsMessageSetEncodingParams(SecCmsMessageRef cmsg,
                               PK11PasswordFunc pwfn, void *pwfn_arg,
                               SecCmsGetDecryptKeyCallback encrypt_key_cb, void *encrypt_key_cb_arg);

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
SecCmsContentInfoSetContent(SecCmsContentInfoRef cinfo, SECOidTag type, void *ptr);


/************************************************************************
 * cmssigdata.c - CMS signedData methods
 ************************************************************************/

extern OSStatus
SecCmsSignedDataSetDigestValue(SecCmsSignedDataRef sigd,
				SECOidTag digestalgtag,
				SecAsn1Item * digestdata);

extern OSStatus
SecCmsSignedDataAddDigest(PRArenaPool *poolp,
				SecCmsSignedDataRef sigd,
				SECOidTag digestalgtag,
				SecAsn1Item * digest);

extern SecAsn1Item *
SecCmsSignedDataGetDigestByAlgTag(SecCmsSignedDataRef sigd, SECOidTag algtag);

extern SecAsn1Item *
SecCmsSignedDataGetDigestValue(SecCmsSignedDataRef sigd, SECOidTag digestalgtag);

/*!
    @function
 */
extern OSStatus
SecCmsSignedDataAddSignerInfo(SecCmsSignedDataRef sigd,
				SecCmsSignerInfoRef signerinfo);

/*!
	@function
 */
extern OSStatus
SecCmsSignedDataSetDigests(SecCmsSignedDataRef sigd,
				SECAlgorithmID **digestalgs,
				SecAsn1Item * *digests);

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


/************************************************************************
 * cmssiginfo.c - CMS signerInfo methods
 ************************************************************************/

/*
 * SecCmsSignerInfoSign - sign something
 *
 */
extern OSStatus
SecCmsSignerInfoSign(SecCmsSignerInfoRef signerinfo, SecAsn1Item * digest, SecAsn1Item * contentType);

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
SecCmsSignerInfoVerify(SecCmsSignerInfoRef signerinfo, SecAsn1Item * digest, SecAsn1Item * contentType);

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

/*!
    @function
    @abstract Destroy a SignerInfo data structure.
 */
extern void
SecCmsSignerInfoDestroy(SecCmsSignerInfoRef si);


/************************************************************************
 * cmsenvdata.c - CMS envelopedData methods
 ************************************************************************/

/*!
    @function
    @abstract Add a recipientinfo to the enveloped data msg.
    @discussion Rip must be created on the same pool as edp - this is not enforced, though.
 */
extern OSStatus
SecCmsEnvelopedDataAddRecipient(SecCmsEnvelopedDataRef edp, SecCmsRecipientInfoRef rip);

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

extern SecAsn1Item *
SecCmsRecipientInfoGetEncryptedKey(SecCmsRecipientInfoRef ri, int subIndex);


extern SECOidTag
SecCmsRecipientInfoGetKeyEncryptionAlgorithmTag(SecCmsRecipientInfoRef ri);

extern OSStatus
SecCmsRecipientInfoWrapBulkKey(SecCmsRecipientInfoRef ri, SecSymmetricKeyRef bulkkey, SECOidTag bulkalgtag);

extern SecSymmetricKeyRef
SecCmsRecipientInfoUnwrapBulkKey(SecCmsRecipientInfoRef ri, int subIndex,
		SecCertificateRef cert, SecPrivateKeyRef privkey, SECOidTag bulkalgtag);

/*!
    @function
 */
extern void
SecCmsRecipientInfoDestroy(SecCmsRecipientInfoRef ri);


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
SecCmsDigestContextFinishSingle(SecCmsDigestContextRef cmsdigcx,
			    SecAsn1Item * digest);

/*!
    @function
    @abstract Finish the digests being calculated and put them into to parralel
		arrays of SecAsn1Items.
    @param cmsdigcx A DigestContext object.
    @param digestalgsp will contain a to an array of digest algorithms on
		success.
    @param digestsp A EncryptedData object to set as the content of the cinfo
		object.
    @result A result code. See "SecCmsBase.h" for possible results.
    @discussion This function requires a DigestContext object which can be made
		by calling SecCmsDigestContextStartSingle or
		SecCmsDigestContextStartMultiple.  The returned arrays remain valid
		until SecCmsDigestContextDestroy is called.
    @availability 10.4 and later
 */
extern OSStatus
SecCmsDigestContextFinishMultiple(SecCmsDigestContextRef cmsdigcx,
			    SECAlgorithmID ***digestalgsp,
			    SecAsn1Item * **digestsp);


/************************************************************************/
SEC_END_PROTOS

#endif /* _CMSPRIV_H_ */
