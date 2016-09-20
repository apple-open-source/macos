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
 * Support routines for CMS implementation, none of which are exported.
 *
 * Do not export this file!  If something in here is really needed outside
 * of smime code, first try to add a CMS interface which will do it for
 * you.  If that has a problem, then just move out what you need, changing
 * its name as appropriate!
 */

#ifndef _CMSLOCAL_H_
#define _CMSLOCAL_H_

#include "cmspriv.h"
#include "cmsreclist.h"
#include <Security/secasn1t.h>

extern const SecAsn1Template SecCmsIssuerAndSNTemplate[];
extern const SecAsn1Template SecCmsContentInfoTemplate[];
extern const SecAsn1Template *nss_cms_get_kea_template(SecCmsKEATemplateSelector whichTemplate);

/************************************************************************/
SEC_BEGIN_PROTOS

/***********************************************************************
 * cmscipher.c - en/decryption routines
 ***********************************************************************/

/*
 * SecCmsCipherContextStartDecrypt - create a cipher context to do decryption
 * based on the given bulk * encryption key and algorithm identifier (which may include an iv).
 */
extern SecCmsCipherContextRef
SecCmsCipherContextStartDecrypt(SecSymmetricKeyRef key, SECAlgorithmID *algid);

/*
 * SecCmsCipherContextStartEncrypt - create a cipher object to do encryption,
 * based on the given bulk encryption key and algorithm tag.  Fill in the algorithm
 * identifier (which may include an iv) appropriately.
 */
extern SecCmsCipherContextRef
SecCmsCipherContextStartEncrypt(PRArenaPool *poolp, SecSymmetricKeyRef key, SECAlgorithmID *algid);

extern void
SecCmsCipherContextDestroy(SecCmsCipherContextRef cc);

/*
 * SecCmsCipherContextDecryptLength - find the output length of the next call to decrypt.
 *
 * cc - the cipher context
 * input_len - number of bytes used as input
 * final - true if this is the final chunk of data
 *
 * Result can be used to perform memory allocations.  Note that the amount
 * is exactly accurate only when not doing a block cipher or when final
 * is false, otherwise it is an upper bound on the amount because until
 * we see the data we do not know how many padding bytes there are
 * (always between 1 and bsize).
 */
extern unsigned int
SecCmsCipherContextDecryptLength(SecCmsCipherContextRef cc, unsigned int input_len, Boolean final);

/*
 * SecCmsCipherContextEncryptLength - find the output length of the next call to encrypt.
 *
 * cc - the cipher context
 * input_len - number of bytes used as input
 * final - true if this is the final chunk of data
 *
 * Result can be used to perform memory allocations.
 */
extern unsigned int
SecCmsCipherContextEncryptLength(SecCmsCipherContextRef cc, unsigned int input_len, Boolean final);

/*
 * SecCmsCipherContextDecrypt - do the decryption
 *
 * cc - the cipher context
 * output - buffer for decrypted result bytes
 * output_len_p - number of bytes in output
 * max_output_len - upper bound on bytes to put into output
 * input - pointer to input bytes
 * input_len - number of input bytes
 * final - true if this is the final chunk of data
 *
 * Decrypts a given length of input buffer (starting at "input" and
 * containing "input_len" bytes), placing the decrypted bytes in
 * "output" and storing the output length in "*output_len_p".
 * "cc" is the return value from SecCmsCipherStartDecrypt.
 * When "final" is true, this is the last of the data to be decrypted.
 */ 
extern OSStatus
SecCmsCipherContextDecrypt(SecCmsCipherContextRef cc, unsigned char *output,
		  unsigned int *output_len_p, unsigned int max_output_len,
		  const unsigned char *input, unsigned int input_len,
		  Boolean final);

/*
 * SecCmsCipherContextEncrypt - do the encryption
 *
 * cc - the cipher context
 * output - buffer for decrypted result bytes
 * output_len_p - number of bytes in output
 * max_output_len - upper bound on bytes to put into output
 * input - pointer to input bytes
 * input_len - number of input bytes
 * final - true if this is the final chunk of data
 *
 * Encrypts a given length of input buffer (starting at "input" and
 * containing "input_len" bytes), placing the encrypted bytes in
 * "output" and storing the output length in "*output_len_p".
 * "cc" is the return value from SecCmsCipherStartEncrypt.
 * When "final" is true, this is the last of the data to be encrypted.
 */ 
extern OSStatus
SecCmsCipherContextEncrypt(SecCmsCipherContextRef cc, unsigned char *output,
		  unsigned int *output_len_p, unsigned int max_output_len,
		  const unsigned char *input, unsigned int input_len,
		  Boolean final);

/************************************************************************
 * cmspubkey.c - public key operations
 ************************************************************************/

/*
 * SecCmsUtilEncryptSymKeyRSA - wrap a symmetric key with RSA
 *
 * this function takes a symmetric key and encrypts it using an RSA public key
 * according to PKCS#1 and RFC2633 (S/MIME)
 */
extern OSStatus
SecCmsUtilEncryptSymKeyRSA(PLArenaPool *poolp, SecCertificateRef cert,
                              SecSymmetricKeyRef key,
                              SecAsn1Item * encKey);

extern OSStatus
SecCmsUtilEncryptSymKeyRSAPubKey(PLArenaPool *poolp,
                                    SecPublicKeyRef publickey,
                                    SecSymmetricKeyRef bulkkey, SecAsn1Item * encKey);

/*
 * SecCmsUtilDecryptSymKeyRSA - unwrap a RSA-wrapped symmetric key
 *
 * this function takes an RSA-wrapped symmetric key and unwraps it, returning a symmetric
 * key handle. Please note that the actual unwrapped key data may not be allowed to leave
 * a hardware token...
 */
extern SecSymmetricKeyRef
SecCmsUtilDecryptSymKeyRSA(SecPrivateKeyRef privkey, SecAsn1Item * encKey, SECOidTag bulkalgtag);

extern OSStatus
SecCmsUtilEncryptSymKeyECDH(PLArenaPool *poolp, SecCertificateRef cert, SecSymmetricKeyRef key,
                            SecAsn1Item * encKey, SecAsn1Item * ukm, SECAlgorithmID *keyEncAlg,
                            SecAsn1Item * originatorPubKey);

extern SecSymmetricKeyRef
SecCmsUtilDecryptSymKeyECDH(SecPrivateKeyRef privkey, SecAsn1Item * encKey, SecAsn1Item * ukm,
                            SECAlgorithmID *keyEncAlg, SECOidTag bulkalgtag, SecAsn1Item * pubKey);

#if 0
extern OSStatus
SecCmsUtilEncryptSymKeyMISSI(PLArenaPool *poolp, SecCertificateRef cert, SecSymmetricKeyRef key,
			SECOidTag symalgtag, SecAsn1Item * encKey, SecAsn1Item * *pparams, void *pwfn_arg);

extern SecSymmetricKeyRef
SecCmsUtilDecryptSymKeyMISSI(SecPrivateKeyRef privkey, SecAsn1Item * encKey,
			SECAlgorithmID *keyEncAlg, SECOidTag bulkalgtag, void *pwfn_arg);

extern OSStatus
SecCmsUtilEncryptSymKeyESDH(PLArenaPool *poolp, SecCertificateRef cert, SecSymmetricKeyRef key,
			SecAsn1Item * encKey, SecAsn1Item * *ukm, SECAlgorithmID *keyEncAlg,
			SecAsn1Item * originatorPubKey);

extern SecSymmetricKeyRef
SecCmsUtilDecryptSymKeyESDH(SecPrivateKeyRef privkey, SecAsn1Item * encKey,
			SECAlgorithmID *keyEncAlg, SECOidTag bulkalgtag, void *pwfn_arg);
#endif

/************************************************************************
 * cmsreclist.c - recipient list stuff
 ************************************************************************/
extern SecCmsRecipient **nss_cms_recipient_list_create(SecCmsRecipientInfoRef *recipientinfos);
extern void nss_cms_recipient_list_destroy(SecCmsRecipient **recipient_list);
extern SecCmsRecipientEncryptedKey *SecCmsRecipientEncryptedKeyCreate(PLArenaPool *poolp);

/************************************************************************
 * cmsarray.c - misc array functions
 ************************************************************************/
/*
 * SecCmsArrayAlloc - allocate an array in an arena
 */
extern void **
SecCmsArrayAlloc(PRArenaPool *poolp, int n);

/*
 * SecCmsArrayAdd - add an element to the end of an array
 */
extern OSStatus
SecCmsArrayAdd(PRArenaPool *poolp, void ***array, void *obj);

/*
 * SecCmsArrayIsEmpty - check if array is empty
 */
extern Boolean
SecCmsArrayIsEmpty(void **array);

/*
 * SecCmsArrayCount - count number of elements in array
 */
extern int
SecCmsArrayCount(void **array);

/*
 * SecCmsArraySort - sort an array ascending, in place
 *
 * If "secondary" is not NULL, the same reordering gets applied to it.
 * If "tertiary" is not NULL, the same reordering gets applied to it.
 * "compare" is a function that returns 
 *  < 0 when the first element is less than the second
 *  = 0 when the first element is equal to the second
 *  > 0 when the first element is greater than the second
 */
extern void
SecCmsArraySort(void **primary, int (*compare)(void *,void *), void **secondary, void **tertiary);

/************************************************************************
 * cmsattr.c - misc attribute functions
 ************************************************************************/
/*
 * SecCmsAttributeCreate - create an attribute
 *
 * if value is NULL, the attribute won't have a value. It can be added later
 * with SecCmsAttributeAddValue.
 */
extern SecCmsAttribute *
SecCmsAttributeCreate(PRArenaPool *poolp, SECOidTag oidtag, SecAsn1Item * value, Boolean encoded);

/*
 * SecCmsAttributeAddValue - add another value to an attribute
 */
extern OSStatus
SecCmsAttributeAddValue(PLArenaPool *poolp, SecCmsAttribute *attr, SecAsn1Item * value);

/*
 * SecCmsAttributeGetType - return the OID tag
 */
extern SECOidTag
SecCmsAttributeGetType(SecCmsAttribute *attr);

/*
 * SecCmsAttributeGetValue - return the first attribute value
 *
 * We do some sanity checking first:
 * - Multiple values are *not* expected.
 * - Empty values are *not* expected.
 */
extern SecAsn1Item *
SecCmsAttributeGetValue(SecCmsAttribute *attr);

/*
 * SecCmsAttributeCompareValue - compare the attribute's first value against data
 */
extern Boolean
SecCmsAttributeCompareValue(SecCmsAttribute *attr, SecAsn1Item * av);

/*
 * SecCmsAttributeArrayEncode - encode an Attribute array as SET OF Attributes
 *
 * If you are wondering why this routine does not reorder the attributes
 * first, and might be tempted to make it do so, see the comment by the
 * call to ReorderAttributes in cmsencode.c.  (Or, see who else calls this
 * and think long and hard about the implications of making it always
 * do the reordering.)
 */
extern SecAsn1Item *
SecCmsAttributeArrayEncode(PRArenaPool *poolp, SecCmsAttribute ***attrs, SecAsn1Item * dest);

/*
 * SecCmsAttributeArrayReorder - sort attribute array by attribute's DER encoding
 *
 * make sure that the order of the attributes guarantees valid DER (which must be
 * in lexigraphically ascending order for a SET OF); if reordering is necessary it
 * will be done in place (in attrs).
 */
extern OSStatus
SecCmsAttributeArrayReorder(SecCmsAttribute **attrs);

/*
 * SecCmsAttributeArrayFindAttrByOidTag - look through a set of attributes and
 * find one that matches the specified object ID.
 *
 * If "only" is true, then make sure that there is not more than one attribute
 * of the same type.  Otherwise, just return the first one found. (XXX Does
 * anybody really want that first-found behavior?  It was like that when I found it...)
 */
extern SecCmsAttribute *
SecCmsAttributeArrayFindAttrByOidTag(SecCmsAttribute **attrs, SECOidTag oidtag, Boolean only);

/*
 * SecCmsAttributeArrayAddAttr - add an attribute to an
 * array of attributes. 
 */
extern OSStatus
SecCmsAttributeArrayAddAttr(PLArenaPool *poolp, SecCmsAttribute ***attrs, SecCmsAttribute *attr);

/*
 * SecCmsAttributeArraySetAttr - set an attribute's value in a set of attributes
 */
extern OSStatus
SecCmsAttributeArraySetAttr(PLArenaPool *poolp, SecCmsAttribute ***attrs, SECOidTag type, SecAsn1Item * value, Boolean encoded);

/************************************************************************/
SEC_END_PROTOS

#endif /* _CMSLOCAL_H_ */
