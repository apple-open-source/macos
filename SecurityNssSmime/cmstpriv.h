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
 * Header for CMS types.
 */

#ifndef _CMSTPRIV_H_
#define _CMSTPRIV_H_

#include "cmst.h"

#include "secasn1t.h"
#include "plarenas.h"
#include "nameTemplates.h"

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDate.h>
#include <Security/SecCertificate.h>
#include <Security/SecKey.h>

/* rjr: PKCS #11 cert handling (pk11cert.c) does use SecCmsRecipientInfo's.
 * This is because when we search the recipient list for the cert and key we
 * want, we need to invert the order of the loops we used to have. The old
 * loops were:
 *
 *  For each recipient {
 *       find_cert = PK11_Find_AllCert(recipient->issuerSN);
 *       [which unrolls to... ]
 *       For each slot {
 *            Log into slot;
 *            search slot for cert;
 *      }
 *  }
 *
 *  the new loop searchs all the recipients at once on a slot. this allows
 *  PKCS #11 to order slots in such a way that logout slots don't get checked
 *  if we can find the cert on a logged in slot. This eliminates lots of
 *  spurious password prompts when smart cards are installed... so why this
 *  comment? If you make SecCmsRecipientInfo completely opaque, you need
 *  to provide a non-opaque list of issuerSN's (the only field PKCS#11 needs
 *  and fix up pk11cert.c first. NOTE: Only S/MIME calls this special PKCS #11
 *  function.
 */

typedef union SecCmsContentUnion SecCmsContent;
typedef struct SecCmsSignerIdentifierStr SecCmsSignerIdentifier;

typedef struct SecCmsSMIMEKEAParametersStr SecCmsSMIMEKEAParameters;

typedef struct SecCmsCipherContextStr SecCmsCipherContext;


/* =============================================================================
 * ENCAPSULATED CONTENTINFO & CONTENTINFO
 */

union SecCmsContentUnion {
    /* either unstructured */
    CSSM_DATA *			data;
    /* or structured data */
    SecCmsDigestedData *	digestedData;
    SecCmsEncryptedData	*	encryptedData;
    SecCmsEnvelopedData	*	envelopedData;
    SecCmsSignedData *		signedData;
    /* or anonymous pointer to something */
    void *			pointer;
};

struct SecCmsContentInfoStr {
    CSSM_DATA			contentType;
    SecCmsContent		content;
    /* --------- local; not part of encoding --------- */
    SECOidData *		contentTypeTag;	

    /* additional info for encryptedData and envelopedData */
    /* we waste this space for signedData and digestedData. sue me. */

    SECAlgorithmID		contentEncAlg;
    CSSM_DATA *			rawContent;		/* encrypted DER, optional */
							/* XXXX bytes not encrypted, but encoded? */
    /* --------- local; not part of encoding --------- */
    SecSymmetricKeyRef		bulkkey;		/* bulk encryption key */
    int				keysize;		/* size of bulk encryption key
							 * (only used by creation code) */
    SECOidTag			contentEncAlgTag;	/* oid tag of encryption algorithm
							 * (only used by creation code) */
    SecCmsCipherContext		*ciphcx;		/* context for en/decryption going on */
    SecCmsDigestContext		*digcx;			/* context for digesting going on */
    SecPrivateKeyRef		privkey;		/* @@@ private key is only here as a workaround for 3401088 */
};

/* =============================================================================
 * MESSAGE
 */

struct SecCmsMessageStr {
    SecCmsContentInfo	contentInfo;		/* "outer" cinfo */
    /* --------- local; not part of encoding --------- */
    PLArenaPool *	poolp;
    PRBool		poolp_is_ours;
    int			refCount;
    /* properties of the "inner" data */
    SECAlgorithmID **	detached_digestalgs;
    CSSM_DATA **	detached_digests;
    void *		pwfn_arg;
    SecCmsGetDecryptKeyCallback decrypt_key_cb;
    void *		decrypt_key_cb_arg;
};

/* =============================================================================
 * SIGNEDDATA
 */

struct SecCmsSignedDataStr {
    CSSM_DATA			version;
    SECAlgorithmID **		digestAlgorithms;
    SecCmsContentInfo		contentInfo;
    CSSM_DATA **		rawCerts;
    CSSM_DATA **		rawCrls;
    SecCmsSignerInfo **		signerInfos;
    /* --------- local; not part of encoding --------- */
    SecCmsMessage *		cmsg;			/* back pointer to message */
    CSSM_DATA **		digests;
    CFMutableArrayRef		certs;
};
#define SEC_CMS_SIGNED_DATA_VERSION_BASIC	1	/* what we *create* */
#define SEC_CMS_SIGNED_DATA_VERSION_EXT		3	/* what we *create* */

typedef enum {
    SecCmsSignerIDIssuerSN = 0,
    SecCmsSignerIDSubjectKeyID = 1
} SecCmsSignerIDSelector;

struct SecCmsSignerIdentifierStr {
    SecCmsSignerIDSelector identifierType;
    union {
	SecCmsIssuerAndSN *issuerAndSN;
	CSSM_DATA *subjectKeyID;
    } id;
};

struct SecCmsIssuerAndSNStr {
	NSS_Name issuer;
	CSSM_DATA serialNumber;
    /* --------- local; not part of encoding --------- */
	CSSM_DATA derIssuer;
};

struct SecCmsSignerInfoStr {
    CSSM_DATA			version;
    SecCmsSignerIdentifier	signerIdentifier;
    SECAlgorithmID		digestAlg;
    SecCmsAttribute **		authAttr;
    SECAlgorithmID		digestEncAlg;
    CSSM_DATA			encDigest;
    SecCmsAttribute **		unAuthAttr;
    /* --------- local; not part of encoding --------- */
    SecCmsMessage *		cmsg;			/* back pointer to message */
    SecCertificateRef		cert;
    CFArrayRef			certList;
    CFAbsoluteTime		signingTime;
    SecCmsVerificationStatus	verificationStatus;
    SecPrivateKeyRef		signingKey; /* Used if we're using subjKeyID*/
    SecPublicKeyRef		pubKey;
};
#define SEC_CMS_SIGNER_INFO_VERSION_ISSUERSN	1	/* what we *create* */
#define SEC_CMS_SIGNER_INFO_VERSION_SUBJKEY	3	/* what we *create* */

/* =============================================================================
 * ENVELOPED DATA
 */
struct SecCmsEnvelopedDataStr {
    CSSM_DATA			version;
    SecCmsOriginatorInfo *	originatorInfo;		/* optional */
    SecCmsRecipientInfo **	recipientInfos;
    SecCmsContentInfo		contentInfo;
    SecCmsAttribute **		unprotectedAttr;
    /* --------- local; not part of encoding --------- */
    SecCmsMessage *		cmsg;			/* back pointer to message */
};
#define SEC_CMS_ENVELOPED_DATA_VERSION_REG	0	/* what we *create* */
#define SEC_CMS_ENVELOPED_DATA_VERSION_ADV	2	/* what we *create* */

struct SecCmsOriginatorInfoStr {
    CSSM_DATA **		rawCerts;
    CSSM_DATA **		rawCrls;
    /* --------- local; not part of encoding --------- */
    SecCertificateRef *		certs;
};

/* -----------------------------------------------------------------------------
 * key transport recipient info
 */
typedef enum {
    SecCmsRecipientIDIssuerSN = 0,
    SecCmsRecipientIDSubjectKeyID = 1
} SecCmsRecipientIDSelector;

struct SecCmsRecipientIdentifierStr {
    SecCmsRecipientIDSelector	identifierType;
    union {
	SecCmsIssuerAndSN	*issuerAndSN;
	CSSM_DATA 		*subjectKeyID;
    } id;
};
typedef struct SecCmsRecipientIdentifierStr SecCmsRecipientIdentifier;

struct SecCmsKeyTransRecipientInfoStr {
    CSSM_DATA			version;
    SecCmsRecipientIdentifier	recipientIdentifier;
    SECAlgorithmID		keyEncAlg;
    CSSM_DATA			encKey;
};
typedef struct SecCmsKeyTransRecipientInfoStr SecCmsKeyTransRecipientInfo;

/*
 * View comments before SecCmsRecipientInfoStr for purpose of this
 * structure.
 */
struct SecCmsKeyTransRecipientInfoExStr {
    SecCmsKeyTransRecipientInfo recipientInfo;
    int version;  /* version of this structure (0) */
    SecPublicKeyRef pubKey;
};

typedef struct SecCmsKeyTransRecipientInfoExStr SecCmsKeyTransRecipientInfoEx;

#define SEC_CMS_KEYTRANS_RECIPIENT_INFO_VERSION_ISSUERSN	0	/* what we *create* */
#define SEC_CMS_KEYTRANS_RECIPIENT_INFO_VERSION_SUBJKEY		2	/* what we *create* */

/* -----------------------------------------------------------------------------
 * key agreement recipient info
 */
struct SecCmsOriginatorPublicKeyStr {
    SECAlgorithmID			algorithmIdentifier;
    CSSM_DATA				publicKey;			/* bit string! */
};
typedef struct SecCmsOriginatorPublicKeyStr SecCmsOriginatorPublicKey;

typedef enum {
    SecCmsOriginatorIDOrKeyIssuerSN = 0,
    SecCmsOriginatorIDOrKeySubjectKeyID = 1,
    SecCmsOriginatorIDOrKeyOriginatorPublicKey = 2
} SecCmsOriginatorIDOrKeySelector;

struct SecCmsOriginatorIdentifierOrKeyStr {
    SecCmsOriginatorIDOrKeySelector identifierType;
    union {
	SecCmsIssuerAndSN		*issuerAndSN;		/* static-static */
	CSSM_DATA			*subjectKeyID;		/* static-static */
	SecCmsOriginatorPublicKey	originatorPublicKey;	/* ephemeral-static */
    } id;
};
typedef struct SecCmsOriginatorIdentifierOrKeyStr SecCmsOriginatorIdentifierOrKey;

struct SecCmsRecipientKeyIdentifierStr {
    CSSM_DATA *				subjectKeyIdentifier;
    CSSM_DATA *				date;			/* optional */
    CSSM_DATA *				other;			/* optional */
};
typedef struct SecCmsRecipientKeyIdentifierStr SecCmsRecipientKeyIdentifier;

typedef enum {
    SecCmsKeyAgreeRecipientIDIssuerSN = 0,
    SecCmsKeyAgreeRecipientIDRKeyID = 1
} SecCmsKeyAgreeRecipientIDSelector;

struct SecCmsKeyAgreeRecipientIdentifierStr {
    SecCmsKeyAgreeRecipientIDSelector	identifierType;
    union {
	SecCmsIssuerAndSN		*issuerAndSN;
	SecCmsRecipientKeyIdentifier	recipientKeyIdentifier;
    } id;
};
typedef struct SecCmsKeyAgreeRecipientIdentifierStr SecCmsKeyAgreeRecipientIdentifier;

struct SecCmsRecipientEncryptedKeyStr {
    SecCmsKeyAgreeRecipientIdentifier	recipientIdentifier;
    CSSM_DATA				encKey;
};
typedef struct SecCmsRecipientEncryptedKeyStr SecCmsRecipientEncryptedKey;

struct SecCmsKeyAgreeRecipientInfoStr {
    CSSM_DATA				version;
    SecCmsOriginatorIdentifierOrKey	originatorIdentifierOrKey;
    CSSM_DATA *				ukm;				/* optional */
    SECAlgorithmID			keyEncAlg;
    SecCmsRecipientEncryptedKey **	recipientEncryptedKeys;
};
typedef struct SecCmsKeyAgreeRecipientInfoStr SecCmsKeyAgreeRecipientInfo;

#define SEC_CMS_KEYAGREE_RECIPIENT_INFO_VERSION	3	/* what we *create* */

/* -----------------------------------------------------------------------------
 * KEK recipient info
 */
struct SecCmsKEKIdentifierStr {
    CSSM_DATA			keyIdentifier;
    CSSM_DATA *			date;			/* optional */
    CSSM_DATA *			other;			/* optional */
};
typedef struct SecCmsKEKIdentifierStr SecCmsKEKIdentifier;

struct SecCmsKEKRecipientInfoStr {
    CSSM_DATA			version;
    SecCmsKEKIdentifier		kekIdentifier;
    SECAlgorithmID		keyEncAlg;
    CSSM_DATA			encKey;
};
typedef struct SecCmsKEKRecipientInfoStr SecCmsKEKRecipientInfo;

#define SEC_CMS_KEK_RECIPIENT_INFO_VERSION	4	/* what we *create* */

/* -----------------------------------------------------------------------------
 * recipient info
 */

typedef enum {
    SecCmsRecipientInfoIDKeyTrans = 0,
    SecCmsRecipientInfoIDKeyAgree = 1,
    SecCmsRecipientInfoIDKEK = 2
} SecCmsRecipientInfoIDSelector;

/*
 * In order to preserve backwards binary compatibility when implementing
 * creation of Recipient Info's that uses subjectKeyID in the 
 * keyTransRecipientInfo we need to stash a public key pointer in this
 * structure somewhere.  We figured out that SecCmsKeyTransRecipientInfo
 * is the smallest member of the ri union.  We're in luck since that's
 * the very structure that would need to use the public key. So we created
 * a new structure SecCmsKeyTransRecipientInfoEx which has a member 
 * SecCmsKeyTransRecipientInfo as the first member followed by a version
 * and a public key pointer.  This way we can keep backwards compatibility
 * without changing the size of this structure.
 *
 * BTW, size of structure:
 * SecCmsKeyTransRecipientInfo:  9 ints, 4 pointers
 * SecCmsKeyAgreeRecipientInfo: 12 ints, 8 pointers
 * SecCmsKEKRecipientInfo:      10 ints, 7 pointers
 *
 * The new structure:
 * SecCmsKeyTransRecipientInfoEx: sizeof(SecCmsKeyTransRecipientInfo) +
 *                                1 int, 1 pointer
 */

struct SecCmsRecipientInfoStr {
    SecCmsRecipientInfoIDSelector recipientInfoType;
    union {
	SecCmsKeyTransRecipientInfo keyTransRecipientInfo;
	SecCmsKeyAgreeRecipientInfo keyAgreeRecipientInfo;
	SecCmsKEKRecipientInfo kekRecipientInfo;
	SecCmsKeyTransRecipientInfoEx keyTransRecipientInfoEx;
    } ri;
    /* --------- local; not part of encoding --------- */
    SecCmsMessage *		cmsg;			/* back pointer to message */
    SecCertificateRef 		cert;			/* recipient's certificate */
};

/* =============================================================================
 * DIGESTED DATA
 */
struct SecCmsDigestedDataStr {
    CSSM_DATA			version;
    SECAlgorithmID		digestAlg;
    SecCmsContentInfo		contentInfo;
    CSSM_DATA			digest;
    /* --------- local; not part of encoding --------- */
    SecCmsMessage *		cmsg;		/* back pointer */
    CSSM_DATA			cdigest;	/* calculated digest */
};
#define SEC_CMS_DIGESTED_DATA_VERSION_DATA	0	/* what we *create* */
#define SEC_CMS_DIGESTED_DATA_VERSION_ENCAP	2	/* what we *create* */

/* =============================================================================
 * ENCRYPTED DATA
 */
struct SecCmsEncryptedDataStr {
    CSSM_DATA			version;
    SecCmsContentInfo		contentInfo;
    SecCmsAttribute **		unprotectedAttr;	/* optional */
    /* --------- local; not part of encoding --------- */
    SecCmsMessage *		cmsg;		/* back pointer */
};
#define SEC_CMS_ENCRYPTED_DATA_VERSION		0	/* what we *create* */
#define SEC_CMS_ENCRYPTED_DATA_VERSION_UPATTR	2	/* what we *create* */

/* =============================================================================
 * FORTEZZA KEA
 */

/* An enumerated type used to select templates based on the encryption
   scenario and data specifics. */
typedef enum {
    SecCmsKEAInvalid = -1,
    SecCmsKEAUsesSkipjack = 0,
    SecCmsKEAUsesNonSkipjack = 1,
    SecCmsKEAUsesNonSkipjackWithPaddedEncKey = 2
} SecCmsKEATemplateSelector;

/* ### mwelch - S/MIME KEA parameters. These don't really fit here,
                but I cannot think of a more appropriate place at this time. */
struct SecCmsSMIMEKEAParametersStr {
    CSSM_DATA originatorKEAKey;	/* sender KEA key (encrypted?) */
    CSSM_DATA originatorRA;	/* random number generated by sender */
    CSSM_DATA nonSkipjackIV;	/* init'n vector for SkipjackCBC64
			           decryption of KEA key if Skipjack
				   is not the bulk algorithm used on
				   the message */
    CSSM_DATA bulkKeySize;	/* if Skipjack is not the bulk
			           algorithm used on the message,
				   and the size of the bulk encryption
				   key is not the same as that of
				   originatorKEAKey (due to padding
				   perhaps), this field will contain
				   the real size of the bulk encryption
				   key. */
};

/*
 * *****************************************************************************
 * *****************************************************************************
 * *****************************************************************************
 */

/*
 * See comment above about this type not really belonging to CMS.
 */
struct SecCmsAttributeStr {
    /* The following fields make up an encoded Attribute: */
    CSSM_DATA			type;
    CSSM_DATA **		values;	/* data may or may not be encoded */
    /* The following fields are not part of an encoded Attribute: */
    SECOidData *		typeTag;
    PRBool			encoded;	/* when true, values are encoded */
};


#endif /* _CMSTPRIV_H_ */
