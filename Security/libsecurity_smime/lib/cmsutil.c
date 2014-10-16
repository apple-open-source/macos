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
 * CMS miscellaneous utility functions.
 */

#include <Security/SecCmsEncoder.h> /* @@@ Remove this when we move the Encoder method. */
#include <Security/SecCmsSignerInfo.h>

#include "cmslocal.h"

#include "SecAsn1Item.h"
#include "secoid.h"
#include "cryptohi.h"

#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <security_asn1/secport.h>

#if USE_CDSA_CRYPTO
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>
#include <Security/SecBase.h>

#else
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecBase.h>

#endif


/*
 * SecCmsArraySortByDER - sort array of objects by objects' DER encoding
 *
 * make sure that the order of the objects guarantees valid DER (which must be
 * in lexigraphically ascending order for a SET OF); if reordering is necessary it
 * will be done in place (in objs).
 */
OSStatus
SecCmsArraySortByDER(void **objs, const SecAsn1Template *objtemplate, void **objs2)
{
    PRArenaPool *poolp;
    int num_objs;
    SecAsn1Item **enc_objs;
    OSStatus rv = SECFailure;
    int i;

    if (objs == NULL)					/* already sorted */
	return SECSuccess;

    num_objs = SecCmsArrayCount((void **)objs);
    if (num_objs == 0 || num_objs == 1)		/* already sorted. */
	return SECSuccess;

    poolp = PORT_NewArena (1024);	/* arena for temporaries */
    if (poolp == NULL)
	return SECFailure;		/* no memory; nothing we can do... */

    /*
     * Allocate arrays to hold the individual encodings which we will use
     * for comparisons and the reordered attributes as they are sorted.
     */
    enc_objs = (SecAsn1Item **)PORT_ArenaZAlloc(poolp, (num_objs + 1) * sizeof(SecAsn1Item *));
    if (enc_objs == NULL)
	goto loser;

    /* DER encode each individual object. */
    for (i = 0; i < num_objs; i++) {
	enc_objs[i] = SEC_ASN1EncodeItem(poolp, NULL, objs[i], objtemplate);
	if (enc_objs[i] == NULL)
	    goto loser;
    }
    enc_objs[num_objs] = NULL;

    /* now compare and sort objs by the order of enc_objs */
    SecCmsArraySort((void **)enc_objs, SecCmsUtilDERCompare, objs, objs2);

    rv = SECSuccess;

loser:
    PORT_FreeArena (poolp, PR_FALSE);
    return rv;
}

/*
 * SecCmsUtilDERCompare - for use with SecCmsArraySort to
 *  sort arrays of SecAsn1Items containing DER
 */
int
SecCmsUtilDERCompare(void *a, void *b)
{
    SecAsn1Item * der1 = (SecAsn1Item *)a;
    SecAsn1Item * der2 = (SecAsn1Item *)b;

    /*
     * Find the lowest (lexigraphically) encoding.  One that is
     * shorter than all the rest is known to be "less" because each
     * attribute is of the same type (a SEQUENCE) and so thus the
     * first octet of each is the same, and the second octet is
     * the length (or the length of the length with the high bit
     * set, followed by the length, which also works out to always
     * order the shorter first).  Two (or more) that have the
     * same length need to be compared byte by byte until a mismatch
     * is found.
     */
    if (der1->Length != der2->Length)
	return (der1->Length < der2->Length) ? -1 : 1;

#if 1
    return memcmp(der1->Data, der2->Data, der1->Length);
#else
    size_t j;
    for (j = 0; j < der1->Length; j++) {
	if (der1->Data[j] == der2->Data[j])
	    continue;
	return (der1->Data[j] < der2->Data[j]) ? -1 : 1;
    }
    return 0;
#endif
}

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
int
SecCmsAlgArrayGetIndexByAlgID(SECAlgorithmID **algorithmArray, SECAlgorithmID *algid)
{
    int i;

    if (algorithmArray == NULL || algorithmArray[0] == NULL)
	return -1;

    for (i = 0; algorithmArray[i] != NULL; i++) {
	if (SECOID_CompareAlgorithmID(algorithmArray[i], algid) == SECEqual)
	    break;	/* bingo */
    }

    if (algorithmArray[i] == NULL)
	return -1;	/* not found */

    return i;
}

/*
 * SecCmsAlgArrayGetIndexByAlgTag - find a specific algorithm in an array of 
 * algorithms.
 *
 * algorithmArray - array of algorithm IDs
 * algtag - algorithm tag of algorithm to pick
 *
 * Returns:
 *  An integer containing the index of the algorithm in the array or -1 if 
 *  algorithm was not found.
 */
int
SecCmsAlgArrayGetIndexByAlgTag(SECAlgorithmID **algorithmArray, 
                                 SECOidTag algtag)
{
    SECOidData *algid;
    int i = -1;

    if (algorithmArray == NULL || algorithmArray[0] == NULL)
	return i;

#ifdef ORDER_N_SQUARED
    for (i = 0; algorithmArray[i] != NULL; i++) {
	algid = SECOID_FindOID(&(algorithmArray[i]->algorithm));
	if (algid->offset == algtag)
	    break;	/* bingo */
    }
#else
    algid = SECOID_FindOIDByTag(algtag);
    if (!algid) 
    	return i;
    for (i = 0; algorithmArray[i] != NULL; i++) {
	if (SECITEM_ItemsAreEqual(&algorithmArray[i]->algorithm, &algid->oid))
	    break;	/* bingo */
    }
#endif

    if (algorithmArray[i] == NULL)
	return -1;	/* not found */

    return i;
}

#if USE_CDSA_CRYPTO
CSSM_CC_HANDLE
#else
void *
#endif
SecCmsUtilGetHashObjByAlgID(SECAlgorithmID *algid)
{
    SECOidData *oidData = SECOID_FindOID(&(algid->algorithm));
    if (oidData)
    {
#if USE_CDSA_CRYPTO
	CSSM_ALGORITHMS alg = oidData->cssmAlgorithm;
	if (alg)
	{
	    CSSM_CC_HANDLE digobj;
	    CSSM_CSP_HANDLE cspHandle = SecCspHandleForAlgorithm(alg);

	    if (!CSSM_CSP_CreateDigestContext(cspHandle, alg, &digobj))
		return digobj;
	}
#else
        void *digobj = NULL;
        switch (oidData->offset) {
        case SEC_OID_SHA1:
            digobj = calloc(1, sizeof(CC_SHA1_CTX));
            CC_SHA1_Init(digobj);
            break;
        case SEC_OID_MD5:
            digobj = calloc(1, sizeof(CC_MD5_CTX));
            CC_MD5_Init(digobj);
            break;
        case SEC_OID_SHA224:
            digobj = calloc(1, sizeof(CC_SHA256_CTX));
            CC_SHA224_Init(digobj);
            break;
        case SEC_OID_SHA256:
            digobj = calloc(1, sizeof(CC_SHA256_CTX));
            CC_SHA256_Init(digobj);
            break;
        case SEC_OID_SHA384:
            digobj = calloc(1, sizeof(CC_SHA512_CTX));
            CC_SHA384_Init(digobj);
            break;
        case SEC_OID_SHA512:
            digobj = calloc(1, sizeof(CC_SHA512_CTX));
            CC_SHA512_Init(digobj);
            break;
        default:
            break;
        }
        return digobj;
#endif
    }

    return 0;
}

/*
 * XXX I would *really* like to not have to do this, but the current
 * signing interface gives me little choice.
 */
SECOidTag
SecCmsUtilMakeSignatureAlgorithm(SECOidTag hashalg, SECOidTag encalg)
{
    switch (encalg) {
      case SEC_OID_PKCS1_RSA_ENCRYPTION:
	switch (hashalg) {
	  case SEC_OID_MD2:
	    return SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION;
	  case SEC_OID_MD5:
	    return SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;
	  case SEC_OID_SHA1:
	    return SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
	  case SEC_OID_SHA256:
	    return SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
	  case SEC_OID_SHA384:
	    return SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
	  case SEC_OID_SHA512:
	    return SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
	  default:
	    return SEC_OID_UNKNOWN;
	}
      case SEC_OID_ANSIX9_DSA_SIGNATURE:
      case SEC_OID_MISSI_KEA_DSS:
      case SEC_OID_MISSI_DSS:
	switch (hashalg) {
	  case SEC_OID_SHA1:
	    return SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;
	  default:
	    return SEC_OID_UNKNOWN;
	}
      default:
	break;
    }

    return encalg;		/* maybe it is already the right algid */
}

const SecAsn1Template *
SecCmsUtilGetTemplateByTypeTag(SECOidTag type)
{
    const SecAsn1Template *template;
    extern const SecAsn1Template SecCmsSignedDataTemplate[];
    extern const SecAsn1Template SecCmsEnvelopedDataTemplate[];
    extern const SecAsn1Template SecCmsEncryptedDataTemplate[];
    extern const SecAsn1Template SecCmsDigestedDataTemplate[];

    switch (type) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	template = SecCmsSignedDataTemplate;
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	template = SecCmsEnvelopedDataTemplate;
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	template = SecCmsEncryptedDataTemplate;
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	template = SecCmsDigestedDataTemplate;
	break;
    default:
    case SEC_OID_PKCS7_DATA:
	template = NULL;
	break;
    }
    return template;
}

size_t
SecCmsUtilGetSizeByTypeTag(SECOidTag type)
{
    size_t size;

    switch (type) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	size = sizeof(SecCmsSignedData);
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	size = sizeof(SecCmsEnvelopedData);
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	size = sizeof(SecCmsEncryptedData);
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	size = sizeof(SecCmsDigestedData);
	break;
    default:
    case SEC_OID_PKCS7_DATA:
	size = 0;
	break;
    }
    return size;
}

SecCmsContentInfoRef
SecCmsContentGetContentInfo(void *msg, SECOidTag type)
{
    SecCmsContent c;
    SecCmsContentInfoRef cinfo;

    if (!msg)
	return NULL;
    c.pointer = msg;
    switch (type) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	cinfo = &(c.signedData->contentInfo);
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	cinfo = &(c.envelopedData->contentInfo);
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	cinfo = &(c.encryptedData->contentInfo);
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	cinfo = &(c.digestedData->contentInfo);
	break;
    default:
	cinfo = NULL;
    }
    return cinfo;
}

// @@@ Return CFStringRef and do localization.
const char *
SecCmsUtilVerificationStatusToString(SecCmsVerificationStatus vs)
{
    switch (vs) {
    case SecCmsVSUnverified:			return "Unverified";
    case SecCmsVSGoodSignature:			return "GoodSignature";
    case SecCmsVSBadSignature:			return "BadSignature";
    case SecCmsVSDigestMismatch:		return "DigestMismatch";
    case SecCmsVSSigningCertNotFound:		return "SigningCertNotFound";
    case SecCmsVSSigningCertNotTrusted:		return "SigningCertNotTrusted";
    case SecCmsVSSignatureAlgorithmUnknown:	return "SignatureAlgorithmUnknown";
    case SecCmsVSSignatureAlgorithmUnsupported: return "SignatureAlgorithmUnsupported";
    case SecCmsVSMalformedSignature:		return "MalformedSignature";
    case SecCmsVSProcessingError:		return "ProcessingError";
    default:					return "Unknown";
    }
}
