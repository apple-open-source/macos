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

#ifndef _CMS_H_
#define _CMS_H_

#include <SecurityNssSmime/cmst.h>

#include <CoreFoundation/CFDate.h>
#include <Security/SecTrust.h>

/************************************************************************/
SEC_BEGIN_PROTOS


/************************************************************************
 * cmsdecode.c - CMS decoding
 ************************************************************************/

/*
 * SecCmsDecoderStart - set up decoding of a DER-encoded CMS message
 *
 * "poolp" - pointer to arena for message, or NULL if new pool should be created
 * "cb", "cb_arg" - callback function and argument for delivery of inner content
 *                  inner content will be stored in the message if cb is NULL.
 * "pwfn", pwfn_arg" - callback function for getting token password
 * "decrypt_key_cb", "decrypt_key_cb_arg" - callback function for getting bulk key for encryptedData
 */
extern SecCmsDecoderContext *
SecCmsDecoderStart(PRArenaPool *poolp,
		      SecCmsContentCallback cb, void *cb_arg,
		      PK11PasswordFunc pwfn, void *pwfn_arg,
		      SecCmsGetDecryptKeyCallback decrypt_key_cb, void *decrypt_key_cb_arg);

/*
 * SecCmsDecoderUpdate - feed DER-encoded data to decoder
 */
extern OSStatus
SecCmsDecoderUpdate(SecCmsDecoderContext *p7dcx, const char *buf, unsigned long len);

/*
 * SecCmsDecoderCancel - cancel a decoding process
 */
extern void
SecCmsDecoderCancel(SecCmsDecoderContext *p7dcx);

/*
 * SecCmsDecoderFinish - mark the end of inner content and finish decoding
 */
extern SecCmsMessage *
SecCmsDecoderFinish(SecCmsDecoderContext *p7dcx);

/*
 * SecCmsMessageCreateFromDER - decode a CMS message from DER encoded data
 */
extern SecCmsMessage *
SecCmsMessageCreateFromDER(CSSM_DATA *DERmessage,
		    SecCmsContentCallback cb, void *cb_arg,
		    PK11PasswordFunc pwfn, void *pwfn_arg,
		    SecCmsGetDecryptKeyCallback decrypt_key_cb, void *decrypt_key_cb_arg);

/************************************************************************
 * cmsencode.c - CMS encoding
 ************************************************************************/

/*
 * SecCmsEncoderStart - set up encoding of a CMS message
 *
 * "cmsg" - message to encode
 * "outputfn", "outputarg" - callback function for delivery of DER-encoded output
 *                           will not be called if NULL.
 * "dest" - if non-NULL, pointer to CSSM_DATA that will hold the DER-encoded output
 * "destpoolp" - pool to allocate DER-encoded output in
 * "pwfn", pwfn_arg" - callback function for getting token password
 * "decrypt_key_cb", "decrypt_key_cb_arg" - callback function for getting bulk key for encryptedData
 * "detached_digestalgs", "detached_digests" - digests from detached content
 */
extern SecCmsEncoderContext *
SecCmsEncoderStart(SecCmsMessage *cmsg,
			SecCmsContentCallback outputfn, void *outputarg,
			CSSM_DATA *dest, PLArenaPool *destpoolp,
			PK11PasswordFunc pwfn, void *pwfn_arg,
			SecCmsGetDecryptKeyCallback decrypt_key_cb, void *decrypt_key_cb_arg,
			SECAlgorithmID **detached_digestalgs, CSSM_DATA **detached_digests);

/*
 * SecCmsEncoderUpdate - take content data delivery from the user
 *
 * "p7ecx" - encoder context
 * "data" - content data
 * "len" - length of content data
 */
extern OSStatus
SecCmsEncoderUpdate(SecCmsEncoderContext *p7ecx, const char *data, unsigned long len);

/*
 * SecCmsEncoderCancel - stop all encoding
 */
extern OSStatus
SecCmsEncoderCancel(SecCmsEncoderContext *p7ecx);

/*
 * SecCmsEncoderFinish - signal the end of data
 *
 * we need to walk down the chain of encoders and the finish them from the innermost out
 */
extern OSStatus
SecCmsEncoderFinish(SecCmsEncoderContext *p7ecx);

/************************************************************************
 * cmsmessage.c - CMS message object
 ************************************************************************/

/*
 * SecCmsMessageCreate - create a CMS message object
 *
 * "poolp" - arena to allocate memory from, or NULL if new arena should be created
 */
extern SecCmsMessage *
SecCmsMessageCreate(PLArenaPool *poolp);

/*
 * SecCmsMessageSetEncodingParams - set up a CMS message object for encoding or decoding
 *
 * "cmsg" - message object
 * "pwfn", pwfn_arg" - callback function for getting token password
 * "decrypt_key_cb", "decrypt_key_cb_arg" - callback function for getting bulk key for encryptedData
 * "detached_digestalgs", "detached_digests" - digests from detached content
 *
 * used internally.
 */
extern void
SecCmsMessageSetEncodingParams(SecCmsMessage *cmsg,
			PK11PasswordFunc pwfn, void *pwfn_arg,
			SecCmsGetDecryptKeyCallback decrypt_key_cb, void *decrypt_key_cb_arg,
			SECAlgorithmID **detached_digestalgs, CSSM_DATA **detached_digests);

/*
 * SecCmsMessageDestroy - destroy a CMS message and all of its sub-pieces.
 */
extern void
SecCmsMessageDestroy(SecCmsMessage *cmsg);

/*
 * SecCmsMessageCopy - return a copy of the given message. 
 *
 * The copy may be virtual or may be real -- either way, the result needs
 * to be passed to SecCmsMessageDestroy later (as does the original).
 */
extern SecCmsMessage *
SecCmsMessageCopy(SecCmsMessage *cmsg);

/*
 * SecCmsMessageGetArena - return a pointer to the message's arena pool
 */
extern PLArenaPool *
SecCmsMessageGetArena(SecCmsMessage *cmsg);

/*
 * SecCmsMessageGetContentInfo - return a pointer to the top level contentInfo
 */
extern SecCmsContentInfo *
SecCmsMessageGetContentInfo(SecCmsMessage *cmsg);

/*
 * Return a pointer to the actual content. 
 * In the case of those types which are encrypted, this returns the *plain* content.
 * In case of nested contentInfos, this descends and retrieves the innermost content.
 */
extern CSSM_DATA *
SecCmsMessageGetContent(SecCmsMessage *cmsg);

/*
 * SecCmsMessageContentLevelCount - count number of levels of CMS content objects in this message
 *
 * CMS data content objects do not count.
 */
extern int
SecCmsMessageContentLevelCount(SecCmsMessage *cmsg);

/*
 * SecCmsMessageContentLevel - find content level #n
 *
 * CMS data content objects do not count.
 */
extern SecCmsContentInfo *
SecCmsMessageContentLevel(SecCmsMessage *cmsg, int n);

/*
 * SecCmsMessageContainsCertsOrCrls - see if message contains certs along the way
 */
extern PRBool
SecCmsMessageContainsCertsOrCrls(SecCmsMessage *cmsg);

/*
 * SecCmsMessageIsEncrypted - see if message contains a encrypted submessage
 */
extern PRBool
SecCmsMessageIsEncrypted(SecCmsMessage *cmsg);

/*
 * SecCmsMessageIsSigned - see if message contains a signed submessage
 *
 * If the CMS message has a SignedData with a signature (not just a SignedData)
 * return true; false otherwise.  This can/should be called before calling
 * VerifySignature, which will always indicate failure if no signature is
 * present, but that does not mean there even was a signature!
 * Note that the content itself can be empty (detached content was sent
 * another way); it is the presence of the signature that matters.
 */
extern PRBool
SecCmsMessageIsSigned(SecCmsMessage *cmsg);

/*
 * SecCmsMessageIsContentEmpty - see if content is empty
 *
 * returns PR_TRUE is innermost content length is < minLen
 * XXX need the encrypted content length (why?)
 */
extern PRBool
SecCmsMessageIsContentEmpty(SecCmsMessage *cmsg, unsigned int minLen);

/************************************************************************
 * cmscinfo.c - CMS contentInfo methods
 ************************************************************************/

/*
 * SecCmsContentInfoDestroy - destroy a CMS contentInfo and all of its sub-pieces.
 */
extern void
SecCmsContentInfoDestroy(SecCmsContentInfo *cinfo);

/*
 * SecCmsContentInfoGetChildContentInfo - get content's contentInfo (if it exists)
 */
extern SecCmsContentInfo *
SecCmsContentInfoGetChildContentInfo(SecCmsContentInfo *cinfo);

/*
 * SecCmsContentInfoSetContent - set cinfo's content type & content to CMS object
 */
extern OSStatus
SecCmsContentInfoSetContent(SecCmsMessage *cmsg, SecCmsContentInfo *cinfo, SECOidTag type, void *ptr);

/*
 * SecCmsContentInfoSetContentXXXX - typesafe wrappers for SecCmsContentInfoSetType
 *   set cinfo's content type & content to CMS object
 */
extern OSStatus
SecCmsContentInfoSetContentData(SecCmsMessage *cmsg, SecCmsContentInfo *cinfo, CSSM_DATA *data, PRBool detached);

extern OSStatus
SecCmsContentInfoSetContentSignedData(SecCmsMessage *cmsg, SecCmsContentInfo *cinfo, SecCmsSignedData *sigd);

extern OSStatus
SecCmsContentInfoSetContentEnvelopedData(SecCmsMessage *cmsg, SecCmsContentInfo *cinfo, SecCmsEnvelopedData *envd);

extern OSStatus
SecCmsContentInfoSetContentDigestedData(SecCmsMessage *cmsg, SecCmsContentInfo *cinfo, SecCmsDigestedData *digd);

extern OSStatus
SecCmsContentInfoSetContentEncryptedData(SecCmsMessage *cmsg, SecCmsContentInfo *cinfo, SecCmsEncryptedData *encd);

/*
 * SecCmsContentInfoGetContent - get pointer to inner content
 *
 * needs to be casted...
 */
extern void *
SecCmsContentInfoGetContent(SecCmsContentInfo *cinfo);

/* 
 * SecCmsContentInfoGetInnerContent - get pointer to innermost content
 *
 * this is typically only called by SecCmsMessageGetContent()
 */
extern CSSM_DATA *
SecCmsContentInfoGetInnerContent(SecCmsContentInfo *cinfo);

/*
 * SecCmsContentInfoGetContentType{Tag,OID} - find out (saving pointer to lookup result
 * for future reference) and return the inner content type.
 */
extern SECOidTag
SecCmsContentInfoGetContentTypeTag(SecCmsContentInfo *cinfo);

extern CSSM_OID *
SecCmsContentInfoGetContentTypeOID(SecCmsContentInfo *cinfo);

/*
 * SecCmsContentInfoGetContentEncAlgTag - find out (saving pointer to lookup result
 * for future reference) and return the content encryption algorithm tag.
 */
extern SECOidTag
SecCmsContentInfoGetContentEncAlgTag(SecCmsContentInfo *cinfo);

/*
 * SecCmsContentInfoGetContentEncAlg - find out and return the content encryption algorithm tag.
 */
extern SECAlgorithmID *
SecCmsContentInfoGetContentEncAlg(SecCmsContentInfo *cinfo);

extern OSStatus
SecCmsContentInfoSetContentEncAlg(PLArenaPool *poolp, SecCmsContentInfo *cinfo,
				    SECOidTag bulkalgtag, CSSM_DATA *parameters, int keysize);

extern OSStatus
SecCmsContentInfoSetContentEncAlgID(PLArenaPool *poolp, SecCmsContentInfo *cinfo,
				    SECAlgorithmID *algid, int keysize);

extern void
SecCmsContentInfoSetBulkKey(SecCmsContentInfo *cinfo, SecSymmetricKeyRef bulkkey);

extern SecSymmetricKeyRef
SecCmsContentInfoGetBulkKey(SecCmsContentInfo *cinfo);

extern int
SecCmsContentInfoGetBulkKeySize(SecCmsContentInfo *cinfo);


/************************************************************************
 * cmsutil.c - CMS misc utility functions
 ************************************************************************/

extern const char *
SecCmsUtilVerificationStatusToString(SecCmsVerificationStatus vs);


/************************************************************************
 * cmssigdata.c - CMS signedData methods
 ************************************************************************/

extern SecCmsSignedData *
SecCmsSignedDataCreate(SecCmsMessage *cmsg);

extern void
SecCmsSignedDataDestroy(SecCmsSignedData *sigd);

/* 
 * SecCmsSignedDataGetSignerInfos - retrieve the SignedData's signer list
 */
extern SecCmsSignerInfo **
SecCmsSignedDataGetSignerInfos(SecCmsSignedData *sigd);

extern int
SecCmsSignedDataSignerInfoCount(SecCmsSignedData *sigd);

extern SecCmsSignerInfo *
SecCmsSignedDataGetSignerInfo(SecCmsSignedData *sigd, int i);

/* 
 * SecCmsSignedDataGetDigestAlgs - retrieve the SignedData's digest algorithm list
 */
extern SECAlgorithmID **
SecCmsSignedDataGetDigestAlgs(SecCmsSignedData *sigd);

/*
 * SecCmsSignedDataGetContentInfo - return pointer to this signedData's contentinfo
 */
extern SecCmsContentInfo *
SecCmsSignedDataGetContentInfo(SecCmsSignedData *sigd);

/* 
 * SecCmsSignedDataGetCertificateList - retrieve the SignedData's certificate list
 */
extern CSSM_DATA **
SecCmsSignedDataGetCertificateList(SecCmsSignedData *sigd);

extern OSStatus
SecCmsSignedDataImportCerts(SecCmsSignedData *sigd, SecKeychainRef keychain,
				SECCertUsage certusage, PRBool keepcerts);

/*
 * SecCmsSignedDataHasDigests - see if we have digests in place
 */
extern PRBool
SecCmsSignedDataHasDigests(SecCmsSignedData *sigd);

/*
 * SecCmsSignedDataVerifySignerInfo - check the signatures.
 *
 * The digests were either calculated during decoding (and are stored in the
 * signedData itself) or set after decoding using SecCmsSignedDataSetDigests.
 *
 * The verification checks if the signing cert is valid and has a trusted chain
 * for the purpose specified by "policies".
 *
 * If trustRef is NULL the cert chain is verified and the VerificationStatus is set accordingly.
 * Otherwise a SecTrust object is returned for the caller to evaluate using SecTrustEvaluate().
 */
extern OSStatus
SecCmsSignedDataVerifySignerInfo(SecCmsSignedData *sigd, int i, SecKeychainRef keychainOrArray,
				 CFTypeRef policies, SecTrustRef *trustRef);

/*
 * SecCmsSignedDataVerifyCertsOnly - verify the certs in a certs-only message
*/
extern OSStatus
SecCmsSignedDataVerifyCertsOnly(SecCmsSignedData *sigd, 
                                  SecKeychainRef keychainOrArray, 
                                  CFTypeRef policies);

extern OSStatus
SecCmsSignedDataAddCertList(SecCmsSignedData *sigd, CFArrayRef certlist);

/*
 * SecCmsSignedDataAddCertChain - add cert and its entire chain to the set of certs 
 */
extern OSStatus
SecCmsSignedDataAddCertChain(SecCmsSignedData *sigd, SecCertificateRef cert);

extern OSStatus
SecCmsSignedDataAddCertificate(SecCmsSignedData *sigd, SecCertificateRef cert);

extern PRBool
SecCmsSignedDataContainsCertsOrCrls(SecCmsSignedData *sigd);

extern OSStatus
SecCmsSignedDataAddSignerInfo(SecCmsSignedData *sigd,
				SecCmsSignerInfo *signerinfo);

extern CSSM_DATA *
SecCmsSignedDataGetDigestByAlgTag(SecCmsSignedData *sigd, SECOidTag algtag);

extern OSStatus
SecCmsSignedDataSetDigests(SecCmsSignedData *sigd,
				SECAlgorithmID **digestalgs,
				CSSM_DATA **digests);

extern OSStatus
SecCmsSignedDataSetDigestValue(SecCmsSignedData *sigd,
				SECOidTag digestalgtag,
				CSSM_DATA *digestdata);

extern OSStatus
SecCmsSignedDataAddDigest(PRArenaPool *poolp,
				SecCmsSignedData *sigd,
				SECOidTag digestalgtag,
				CSSM_DATA *digest);

extern CSSM_DATA *
SecCmsSignedDataGetDigestValue(SecCmsSignedData *sigd, SECOidTag digestalgtag);

/*
 * SecCmsSignedDataCreateCertsOnly - create a certs-only SignedData.
 *
 * cert          - base certificates that will be included
 * include_chain - if true, include the complete cert chain for cert
 *
 * More certs and chains can be added via AddCertificate and AddCertChain.
 *
 * An error results in a return value of NULL and an error set.
 */
extern SecCmsSignedData *
SecCmsSignedDataCreateCertsOnly(SecCmsMessage *cmsg, SecCertificateRef cert, PRBool include_chain);

/************************************************************************
 * cmssiginfo.c - signerinfo methods
 ************************************************************************/

extern SecCmsSignerInfo *
SecCmsSignerInfoCreate(SecCmsMessage *cmsg, SecIdentityRef identity, SECOidTag digestalgtag);
extern SecCmsSignerInfo *
SecCmsSignerInfoCreateWithSubjKeyID(SecCmsMessage *cmsg, CSSM_DATA *subjKeyID, SecPublicKeyRef pubKey, SecPrivateKeyRef signingKey, SECOidTag digestalgtag);

/*
 * SecCmsSignerInfoDestroy - destroy a SignerInfo data structure
 */
extern void
SecCmsSignerInfoDestroy(SecCmsSignerInfo *si);

/*
 * SecCmsSignerInfoSign - sign something
 *
 */
extern OSStatus
SecCmsSignerInfoSign(SecCmsSignerInfo *signerinfo, CSSM_DATA *digest, CSSM_DATA *contentType);

/*
 * If trustRef is NULL the cert chain is verified and the VerificationStatus is set accordingly.
 * Otherwise a SecTrust object is returned for the caller to evaluate using SecTrustEvaluate().
 */
extern OSStatus
SecCmsSignerInfoVerifyCertificate(SecCmsSignerInfo *signerinfo, SecKeychainRef keychainOrArray,
				  CFTypeRef policies, SecTrustRef *trustRef);

/*
 * SecCmsSignerInfoVerify - verify the signature of a single SignerInfo
 *
 * Just verifies the signature. The assumption is that verification of the certificate
 * is done already.
 */
extern OSStatus
SecCmsSignerInfoVerify(SecCmsSignerInfo *signerinfo, CSSM_DATA *digest, CSSM_DATA *contentType);

extern SecCmsVerificationStatus
SecCmsSignerInfoGetVerificationStatus(SecCmsSignerInfo *signerinfo);

extern SECOidData *
SecCmsSignerInfoGetDigestAlg(SecCmsSignerInfo *signerinfo);

extern SECOidTag
SecCmsSignerInfoGetDigestAlgTag(SecCmsSignerInfo *signerinfo);

extern int
SecCmsSignerInfoGetVersion(SecCmsSignerInfo *signerinfo);

extern CFArrayRef 
SecCmsSignerInfoGetCertList(SecCmsSignerInfo *signerinfo);

/*
 * SecCmsSignerInfoGetSigningTime - return the signing time,
 *				      in UTCTime format, of a CMS signerInfo.
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a pointer to XXXX (what?)
 * A return value of NULL is an error.
 */
extern OSStatus
SecCmsSignerInfoGetSigningTime(SecCmsSignerInfo *sinfo, CFAbsoluteTime *stime);

/*
 * Return the signing cert of a CMS signerInfo.
 *
 * the certs in the enclosing SignedData must have been imported already
 */
extern SecCertificateRef 
SecCmsSignerInfoGetSigningCertificate(SecCmsSignerInfo *signerinfo, SecKeychainRef keychainOrArray);

/*
 * SecCmsSignerInfoGetSignerCommonName - return the common name of the signer
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a CFStringRef containing the common name of the signer.
 * A return value of NULL is an error.
 */
extern CFStringRef
SecCmsSignerInfoGetSignerCommonName(SecCmsSignerInfo *sinfo);

/*
 * SecCmsSignerInfoGetSignerEmailAddress - return the email address of the signer
 *
 * sinfo - signerInfo data for this signer
 *
 * Returns a CFStringRef containing the name of the signer.
 * A return value of NULL is an error.
 */
extern CFStringRef
SecCmsSignerInfoGetSignerEmailAddress(SecCmsSignerInfo *sinfo);

/*
 * SecCmsSignerInfoAddAuthAttr - add an attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo". 
 */
extern OSStatus
SecCmsSignerInfoAddAuthAttr(SecCmsSignerInfo *signerinfo, SecCmsAttribute *attr);

/*
 * SecCmsSignerInfoAddUnauthAttr - add an attribute to the
 * unauthenticated attributes of "signerinfo". 
 */
extern OSStatus
SecCmsSignerInfoAddUnauthAttr(SecCmsSignerInfo *signerinfo, SecCmsAttribute *attr);

/* 
 * SecCmsSignerInfoAddSigningTime - add the signing time to the
 * authenticated (i.e. signed) attributes of "signerinfo". 
 *
 * This is expected to be included in outgoing signed
 * messages for email (S/MIME) but is likely useful in other situations.
 *
 * This should only be added once; a second call will do nothing.
 *
 * XXX This will probably just shove the current time into "signerinfo"
 * but it will not actually get signed until the entire item is
 * processed for encoding.  Is this (expected to be small) delay okay?
 */
extern OSStatus
SecCmsSignerInfoAddSigningTime(SecCmsSignerInfo *signerinfo, CFAbsoluteTime t);

/*
 * SecCmsSignerInfoAddSMIMECaps - add a SMIMECapabilities attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo".
 *
 * This is expected to be included in outgoing signed
 * messages for email (S/MIME).
 */
extern OSStatus
SecCmsSignerInfoAddSMIMECaps(SecCmsSignerInfo *signerinfo);

/*
 * SecCmsSignerInfoAddSMIMEEncKeyPrefs - add a SMIMEEncryptionKeyPreferences attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo".
 *
 * This is expected to be included in outgoing signed messages for email (S/MIME).
 */
OSStatus
SecCmsSignerInfoAddSMIMEEncKeyPrefs(SecCmsSignerInfo *signerinfo, SecCertificateRef cert, SecKeychainRef keychainOrArray);

/*
 * SecCmsSignerInfoAddMSSMIMEEncKeyPrefs - add a SMIMEEncryptionKeyPreferences attribute to the
 * authenticated (i.e. signed) attributes of "signerinfo", using the OID prefered by Microsoft.
 *
 * This is expected to be included in outgoing signed messages for email (S/MIME),
 * if compatibility with Microsoft mail clients is wanted.
 */
OSStatus
SecCmsSignerInfoAddMSSMIMEEncKeyPrefs(SecCmsSignerInfo *signerinfo, SecCertificateRef cert, SecKeychainRef keychainOrArray);

/* 
 * SecCmsSignerInfoAddCounterSignature - countersign a signerinfo
 */
extern OSStatus
SecCmsSignerInfoAddCounterSignature(SecCmsSignerInfo *signerinfo,
				    SECOidTag digestalg, SecIdentityRef identity);

/*
 * XXXX the following needs to be done in the S/MIME layer code
 * after signature of a signerinfo is verified
 */
extern OSStatus
SecCmsSignerInfoSaveSMIMEProfile(SecCmsSignerInfo *signerinfo);

/*
 * SecCmsSignerInfoIncludeCerts - set cert chain inclusion mode for this signer
 */
extern OSStatus
SecCmsSignerInfoIncludeCerts(SecCmsSignerInfo *signerinfo, SecCmsCertChainMode cm, SECCertUsage usage);

/************************************************************************
 * cmsenvdata.c - CMS envelopedData methods
 ************************************************************************/

/*
 * SecCmsEnvelopedDataCreate - create an enveloped data message
 */
extern SecCmsEnvelopedData *
SecCmsEnvelopedDataCreate(SecCmsMessage *cmsg, SECOidTag algorithm, int keysize);

/*
 * SecCmsEnvelopedDataDestroy - destroy an enveloped data message
 */
extern void
SecCmsEnvelopedDataDestroy(SecCmsEnvelopedData *edp);

/*
 * SecCmsEnvelopedDataGetContentInfo - return pointer to this envelopedData's contentinfo
 */
extern SecCmsContentInfo *
SecCmsEnvelopedDataGetContentInfo(SecCmsEnvelopedData *envd);

/*
 * SecCmsEnvelopedDataAddRecipient - add a recipientinfo to the enveloped data msg
 *
 * rip must be created on the same pool as edp - this is not enforced, though.
 */
extern OSStatus
SecCmsEnvelopedDataAddRecipient(SecCmsEnvelopedData *edp, SecCmsRecipientInfo *rip);


/************************************************************************
 * cmsrecinfo.c - CMS recipientInfo methods
 ************************************************************************/

/*
 * SecCmsRecipientInfoCreate - create a recipientinfo
 *
 * we currently do not create KeyAgreement recipientinfos with multiple recipientEncryptedKeys
 * the certificate is supposed to have been verified by the caller
 */
extern SecCmsRecipientInfo *
SecCmsRecipientInfoCreate(SecCmsMessage *cmsg, SecCertificateRef cert);

extern SecCmsRecipientInfo *
SecCmsRecipientInfoCreateWithSubjKeyID(SecCmsMessage   *cmsg, 
                                         CSSM_DATA         *subjKeyID,
                                         SecPublicKeyRef pubKey);

extern SecCmsRecipientInfo *
SecCmsRecipientInfoCreateWithSubjKeyIDFromCert(SecCmsMessage *cmsg, 
                                                 SecCertificateRef cert);

extern void
SecCmsRecipientInfoDestroy(SecCmsRecipientInfo *ri);


/************************************************************************
 * cmsencdata.c - CMS encryptedData methods
 ************************************************************************/
/*
 * SecCmsEncryptedDataCreate - create an empty encryptedData object.
 *
 * "algorithm" specifies the bulk encryption algorithm to use.
 * "keysize" is the key size.
 * 
 * An error results in a return value of NULL and an error set.
 * (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */
extern SecCmsEncryptedData *
SecCmsEncryptedDataCreate(SecCmsMessage *cmsg, SECOidTag algorithm, int keysize);

/*
 * SecCmsEncryptedDataDestroy - destroy an encryptedData object
 */
extern void
SecCmsEncryptedDataDestroy(SecCmsEncryptedData *encd);

/*
 * SecCmsEncryptedDataGetContentInfo - return pointer to encryptedData object's contentInfo
 */
extern SecCmsContentInfo *
SecCmsEncryptedDataGetContentInfo(SecCmsEncryptedData *encd);


/************************************************************************
 * cmsdigdata.c - CMS encryptedData methods
 ************************************************************************/
/*
 * SecCmsDigestedDataCreate - create a digestedData object (presumably for encoding)
 *
 * version will be set by SecCmsDigestedDataEncodeBeforeStart
 * digestAlg is passed as parameter
 * contentInfo must be filled by the user
 * digest will be calculated while encoding
 */
extern SecCmsDigestedData *
SecCmsDigestedDataCreate(SecCmsMessage *cmsg, SECAlgorithmID *digestalg);

/*
 * SecCmsDigestedDataDestroy - destroy a digestedData object
 */
extern void
SecCmsDigestedDataDestroy(SecCmsDigestedData *digd);

/*
 * SecCmsDigestedDataGetContentInfo - return pointer to digestedData object's contentInfo
 */
extern SecCmsContentInfo *
SecCmsDigestedDataGetContentInfo(SecCmsDigestedData *digd);


/************************************************************************
 * cmsdigest.c - digestion routines
 ************************************************************************/

/*
 * SecCmsDigestContextStartMultiple - start digest calculation using all the
 *  digest algorithms in "digestalgs" in parallel.
 */
extern SecCmsDigestContext *
SecCmsDigestContextStartMultiple(SECAlgorithmID **digestalgs);

/*
 * SecCmsDigestContextStartSingle - same as SecCmsDigestContextStartMultiple, but
 *  only one algorithm.
 */
extern SecCmsDigestContext *
SecCmsDigestContextStartSingle(SECAlgorithmID *digestalg);

/*
 * SecCmsDigestContextUpdate - feed more data into the digest machine
 */
extern void
SecCmsDigestContextUpdate(SecCmsDigestContext *cmsdigcx, const unsigned char *data, int len);

/*
 * SecCmsDigestContextCancel - cancel digesting operation
 */
extern void
SecCmsDigestContextCancel(SecCmsDigestContext *cmsdigcx);

/*
 * SecCmsDigestContextFinishMultiple - finish the digests and put them
 *  into an array of CSSM_DATAs (allocated on poolp)
 */
extern OSStatus
SecCmsDigestContextFinishMultiple(SecCmsDigestContext *cmsdigcx, PLArenaPool *poolp,
			    CSSM_DATA ***digestsp);

/*
 * SecCmsDigestContextFinishSingle - same as SecCmsDigestContextFinishMultiple,
 *  but for one digest.
 */
extern OSStatus
SecCmsDigestContextFinishSingle(SecCmsDigestContext *cmsdigcx, PLArenaPool *poolp,
			    CSSM_DATA *digest);

/************************************************************************
 * 
 ************************************************************************/

/* shortcuts for basic use */

/*
 * SecCmsDEREncode -  DER Encode a CMS message, with input being
 *                    the plaintext message and derOut being the output,
 *                    stored in arena's pool.
 */
extern OSStatus
SecCmsDEREncode(SecCmsMessage *cmsg, CSSM_DATA *input, CSSM_DATA *derOut, 
                 PLArenaPool *arena);


/************************************************************************/
SEC_END_PROTOS

#endif /* _CMS_H_ */
