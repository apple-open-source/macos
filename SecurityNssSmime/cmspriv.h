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

#include "cms.h"
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
SecCmsArraySortByDER(void **objs, const SEC_ASN1Template *objtemplate, void **objs2);

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

extern const SEC_ASN1Template *
SecCmsUtilGetTemplateByTypeTag(SECOidTag type);

extern size_t
SecCmsUtilGetSizeByTypeTag(SECOidTag type);

extern SecCmsContentInfo *
SecCmsContentGetContentInfo(void *msg, SECOidTag type);


/************************************************************************
 * cmssigdata.c - CMS signedData methods
 ************************************************************************/

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
SecCmsSignedDataEncodeBeforeStart(SecCmsSignedData *sigd);

extern OSStatus
SecCmsSignedDataEncodeBeforeData(SecCmsSignedData *sigd);

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
SecCmsSignedDataEncodeAfterData(SecCmsSignedData *sigd);

extern OSStatus
SecCmsSignedDataDecodeBeforeData(SecCmsSignedData *sigd);

/*
 * SecCmsSignedDataDecodeAfterData - do all the necessary things to a SignedData
 *     after all the encapsulated data was passed through the decoder.
 */
extern OSStatus
SecCmsSignedDataDecodeAfterData(SecCmsSignedData *sigd);

/*
 * SecCmsSignedDataDecodeAfterEnd - do all the necessary things to a SignedData
 *     after all decoding is finished.
 */
extern OSStatus
SecCmsSignedDataDecodeAfterEnd(SecCmsSignedData *sigd);

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
SecCmsEnvelopedDataEncodeBeforeStart(SecCmsEnvelopedData *envd);

/*
 * SecCmsEnvelopedDataEncodeBeforeData - set up encryption
 */
extern OSStatus
SecCmsEnvelopedDataEncodeBeforeData(SecCmsEnvelopedData *envd);

/*
 * SecCmsEnvelopedDataEncodeAfterData - finalize this envelopedData for encoding
 */
extern OSStatus
SecCmsEnvelopedDataEncodeAfterData(SecCmsEnvelopedData *envd);

/*
 * SecCmsEnvelopedDataDecodeBeforeData - find our recipientinfo, 
 * derive bulk key & set up our contentinfo
 */
extern OSStatus
SecCmsEnvelopedDataDecodeBeforeData(SecCmsEnvelopedData *envd);

/*
 * SecCmsEnvelopedDataDecodeAfterData - finish decrypting this envelopedData's content
 */
extern OSStatus
SecCmsEnvelopedDataDecodeAfterData(SecCmsEnvelopedData *envd);

/*
 * SecCmsEnvelopedDataDecodeAfterEnd - finish decoding this envelopedData
 */
extern OSStatus
SecCmsEnvelopedDataDecodeAfterEnd(SecCmsEnvelopedData *envd);


/************************************************************************
 * cmsrecinfo.c - CMS recipientInfo methods
 ************************************************************************/

extern int
SecCmsRecipientInfoGetVersion(SecCmsRecipientInfo *ri);

extern CSSM_DATA *
SecCmsRecipientInfoGetEncryptedKey(SecCmsRecipientInfo *ri, int subIndex);


extern SECOidTag
SecCmsRecipientInfoGetKeyEncryptionAlgorithmTag(SecCmsRecipientInfo *ri);

extern OSStatus
SecCmsRecipientInfoWrapBulkKey(SecCmsRecipientInfo *ri, SecSymmetricKeyRef bulkkey, SECOidTag bulkalgtag);

extern SecSymmetricKeyRef
SecCmsRecipientInfoUnwrapBulkKey(SecCmsRecipientInfo *ri, int subIndex,
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
SecCmsEncryptedDataEncodeBeforeStart(SecCmsEncryptedData *encd);

/*
 * SecCmsEncryptedDataEncodeBeforeData - set up encryption
 */
extern OSStatus
SecCmsEncryptedDataEncodeBeforeData(SecCmsEncryptedData *encd);

/*
 * SecCmsEncryptedDataEncodeAfterData - finalize this encryptedData for encoding
 */
extern OSStatus
SecCmsEncryptedDataEncodeAfterData(SecCmsEncryptedData *encd);

/*
 * SecCmsEncryptedDataDecodeBeforeData - find bulk key & set up decryption
 */
extern OSStatus
SecCmsEncryptedDataDecodeBeforeData(SecCmsEncryptedData *encd);

/*
 * SecCmsEncryptedDataDecodeAfterData - finish decrypting this encryptedData's content
 */
extern OSStatus
SecCmsEncryptedDataDecodeAfterData(SecCmsEncryptedData *encd);

/*
 * SecCmsEncryptedDataDecodeAfterEnd - finish decoding this encryptedData
 */
extern OSStatus
SecCmsEncryptedDataDecodeAfterEnd(SecCmsEncryptedData *encd);


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
SecCmsDigestedDataEncodeBeforeStart(SecCmsDigestedData *digd);

/*
 * SecCmsDigestedDataEncodeBeforeData - do all the necessary things to a DigestedData
 *     before the encapsulated data is passed through the encoder.
 *
 * In detail:
 *  - set up the digests if necessary
 */
extern OSStatus
SecCmsDigestedDataEncodeBeforeData(SecCmsDigestedData *digd);

/*
 * SecCmsDigestedDataEncodeAfterData - do all the necessary things to a DigestedData
 *     after all the encapsulated data was passed through the encoder.
 *
 * In detail:
 *  - finish the digests
 */
extern OSStatus
SecCmsDigestedDataEncodeAfterData(SecCmsDigestedData *digd);

/*
 * SecCmsDigestedDataDecodeBeforeData - do all the necessary things to a DigestedData
 *     before the encapsulated data is passed through the encoder.
 *
 * In detail:
 *  - set up the digests if necessary
 */
extern OSStatus
SecCmsDigestedDataDecodeBeforeData(SecCmsDigestedData *digd);

/*
 * SecCmsDigestedDataDecodeAfterData - do all the necessary things to a DigestedData
 *     after all the encapsulated data was passed through the encoder.
 *
 * In detail:
 *  - finish the digests
 */
extern OSStatus
SecCmsDigestedDataDecodeAfterData(SecCmsDigestedData *digd);

/*
 * SecCmsDigestedDataDecodeAfterEnd - finalize a digestedData.
 *
 * In detail:
 *  - check the digests for equality
 */
extern OSStatus
SecCmsDigestedDataDecodeAfterEnd(SecCmsDigestedData *digd);


/************************************************************************/
SEC_END_PROTOS

#endif /* _CMSPRIV_H_ */
