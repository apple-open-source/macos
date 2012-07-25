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
 * Stuff specific to S/MIME policy and interoperability.
 */

#include "cmslocal.h"

#include "secoid.h"
#include "secitem.h"
#include "cert.h"
#include "SecSMIMEPriv.h"

#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <Security/SecSMIME.h>
#include <Security/SecKeyPriv.h>

SEC_ASN1_MKSUB(CERT_IssuerAndSNTemplate)
SEC_ASN1_MKSUB(SEC_OctetStringTemplate)
SEC_ASN1_CHOOSER_DECLARE(CERT_IssuerAndSNTemplate)

/* various integer's ASN.1 encoding */
static unsigned char asn1_int40[] = { SEC_ASN1_INTEGER, 0x01, 0x28 };
static unsigned char asn1_int64[] = { SEC_ASN1_INTEGER, 0x01, 0x40 };
static unsigned char asn1_int128[] = { SEC_ASN1_INTEGER, 0x02, 0x00, 0x80 };

/* RC2 algorithm parameters (used in smime_cipher_map) */
static CSSM_DATA param_int40 = { sizeof(asn1_int40), asn1_int40 };
static CSSM_DATA param_int64 = { sizeof(asn1_int64), asn1_int64 };
static CSSM_DATA param_int128 = { sizeof(asn1_int128), asn1_int128 };

/*
 * XXX Would like the "parameters" field to be a CSSM_DATA_PTR , but the
 * encoder is having trouble with optional pointers to an ANY.  Maybe
 * once that is fixed, can change this back...
 */
typedef struct {
    CSSM_DATA capabilityID;
    CSSM_DATA parameters;
    long cipher;		/* optimization */
} NSSSMIMECapability;

static const SecAsn1Template NSSSMIMECapabilityTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSSMIMECapability) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSSSMIMECapability,capabilityID), },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_ANY,
	  offsetof(NSSSMIMECapability,parameters), },
    { 0, }
};

static const SecAsn1Template NSSSMIMECapabilitiesTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, NSSSMIMECapabilityTemplate }
};

/*
 * NSSSMIMEEncryptionKeyPreference - if we find one of these, it needs to prompt us
 *  to store this and only this certificate permanently for the sender email address.
 */
typedef enum {
    NSSSMIMEEncryptionKeyPref_IssuerSN,
    NSSSMIMEEncryptionKeyPref_RKeyID,
    NSSSMIMEEncryptionKeyPref_SubjectKeyID
} NSSSMIMEEncryptionKeyPrefSelector;

typedef struct {
    NSSSMIMEEncryptionKeyPrefSelector selector;
    union {
	SecCmsIssuerAndSN		*issuerAndSN;
	SecCmsRecipientKeyIdentifier	*recipientKeyID;
	CSSM_DATA_PTR subjectKeyID;
    } id;
} NSSSMIMEEncryptionKeyPreference;

extern const SecAsn1Template SecCmsRecipientKeyIdentifierTemplate[];

static const SecAsn1Template smime_encryptionkeypref_template[] = {
    { SEC_ASN1_CHOICE,
	  offsetof(NSSSMIMEEncryptionKeyPreference,selector), NULL,
	  sizeof(NSSSMIMEEncryptionKeyPreference) },
    { SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
	  offsetof(NSSSMIMEEncryptionKeyPreference,id.issuerAndSN),
	  SEC_ASN1_SUB(SecCmsIssuerAndSNTemplate),
	  NSSSMIMEEncryptionKeyPref_IssuerSN },
    { SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(NSSSMIMEEncryptionKeyPreference,id.recipientKeyID),
	  SecCmsRecipientKeyIdentifierTemplate,
	  NSSSMIMEEncryptionKeyPref_IssuerSN },
    { SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 2,
	  offsetof(NSSSMIMEEncryptionKeyPreference,id.subjectKeyID),
	  SEC_ASN1_SUB(kSecAsn1OctetStringTemplate),
	  NSSSMIMEEncryptionKeyPref_SubjectKeyID },
    { 0, }
};

/* smime_cipher_map - map of SMIME symmetric "ciphers" to algtag & parameters */
typedef struct {
    unsigned long cipher;
    SECOidTag algtag;
    CSSM_DATA_PTR parms;
    Boolean enabled;	/* in the user's preferences */
    Boolean allowed;	/* per export policy */
} smime_cipher_map_entry;

/* global: list of supported SMIME symmetric ciphers, ordered roughly by increasing strength */
static smime_cipher_map_entry smime_cipher_map[] = {
/*    cipher			algtag			parms		enabled  allowed */
/*    ---------------------------------------------------------------------------------- */
    { SMIME_RC2_CBC_40,		SEC_OID_RC2_CBC,	&param_int40,	PR_TRUE, PR_TRUE },
    { SMIME_DES_CBC_56,		SEC_OID_DES_CBC,	NULL,		PR_TRUE, PR_TRUE },
    { SMIME_RC2_CBC_64,		SEC_OID_RC2_CBC,	&param_int64,	PR_TRUE, PR_TRUE },
    { SMIME_RC2_CBC_128,	SEC_OID_RC2_CBC,	&param_int128,	PR_TRUE, PR_TRUE },
    { SMIME_DES_EDE3_168,	SEC_OID_DES_EDE3_CBC,	NULL,		PR_TRUE, PR_TRUE },
    { SMIME_AES_CBC_128,        SEC_OID_AES_128_CBC,    NULL,           PR_TRUE, PR_TRUE },
    { SMIME_FORTEZZA,		SEC_OID_FORTEZZA_SKIPJACK, NULL,	PR_TRUE, PR_TRUE }
};
static const int smime_cipher_map_count = sizeof(smime_cipher_map) / sizeof(smime_cipher_map_entry);

/*
 * smime_mapi_by_cipher - find index into smime_cipher_map by cipher
 */
static int
smime_mapi_by_cipher(unsigned long cipher)
{
    int i;

    for (i = 0; i < smime_cipher_map_count; i++) {
	if (smime_cipher_map[i].cipher == cipher)
	    return i;	/* bingo */
    }
    return -1;		/* should not happen if we're consistent, right? */
}

/*
 * NSS_SMIME_EnableCipher - this function locally records the user's preference
 */
OSStatus 
SecSMIMEEnableCipher(uint32 which, Boolean on)
{
    unsigned long mask;
    int mapi;

    mask = which & CIPHER_FAMILYID_MASK;

    PORT_Assert (mask == CIPHER_FAMILYID_SMIME);
    if (mask != CIPHER_FAMILYID_SMIME)
	/* XXX set an error! */
    	return SECFailure;

    mapi = smime_mapi_by_cipher(which);
    if (mapi < 0)
	/* XXX set an error */
	return SECFailure;

    /* do we try to turn on a forbidden cipher? */
    if (!smime_cipher_map[mapi].allowed && on) {
	PORT_SetError (SEC_ERROR_BAD_EXPORT_ALGORITHM);
	return SECFailure;
    }

    if (smime_cipher_map[mapi].enabled != on)
	smime_cipher_map[mapi].enabled = on;

    return SECSuccess;
}


/*
 * this function locally records the export policy
 */
OSStatus 
SecSMIMEAllowCipher(uint32 which, Boolean on)
{
    unsigned long mask;
    int mapi;

    mask = which & CIPHER_FAMILYID_MASK;

    PORT_Assert (mask == CIPHER_FAMILYID_SMIME);
    if (mask != CIPHER_FAMILYID_SMIME)
	/* XXX set an error! */
    	return SECFailure;

    mapi = smime_mapi_by_cipher(which);
    if (mapi < 0)
	/* XXX set an error */
	return SECFailure;

    if (smime_cipher_map[mapi].allowed != on)
	smime_cipher_map[mapi].allowed = on;

    return SECSuccess;
}

/*
 * Based on the given algorithm (including its parameters, in some cases!)
 * and the given key (may or may not be inspected, depending on the
 * algorithm), find the appropriate policy algorithm specification
 * and return it.  If no match can be made, -1 is returned.
 */
static OSStatus
nss_smime_get_cipher_for_alg_and_key(SECAlgorithmID *algid, SecSymmetricKeyRef key, unsigned long *cipher)
{
    SECOidTag algtag;
    unsigned int keylen_bits;
    unsigned long c;

    algtag = SECOID_GetAlgorithmTag(algid);
    switch (algtag) {
    case SEC_OID_RC2_CBC:
	if (SecKeyGetStrengthInBits(key, algid, &keylen_bits))
	    return SECFailure;
	switch (keylen_bits) {
	case 40:
	    c = SMIME_RC2_CBC_40;
	    break;
	case 64:
	    c = SMIME_RC2_CBC_64;
	    break;
	case 128:
	    c = SMIME_RC2_CBC_128;
	    break;
	default:
	    return SECFailure;
	}
	break;
    case SEC_OID_DES_CBC:
	c = SMIME_DES_CBC_56;
	break;
    case SEC_OID_DES_EDE3_CBC:
	c = SMIME_DES_EDE3_168;
	break;
    case SEC_OID_AES_128_CBC:
	c = SMIME_AES_CBC_128;
	break;
    case SEC_OID_FORTEZZA_SKIPJACK:
	c = SMIME_FORTEZZA;
	break;
    default:
	return SECFailure;
    }
    *cipher = c;
    return SECSuccess;
}

static Boolean
nss_smime_cipher_allowed(unsigned long which)
{
    int mapi;

    mapi = smime_mapi_by_cipher(which);
    if (mapi < 0)
	return PR_FALSE;
    return smime_cipher_map[mapi].allowed;
}

Boolean
SecSMIMEDecryptionAllowed(SECAlgorithmID *algid, SecSymmetricKeyRef key)
{
    unsigned long which;

    if (nss_smime_get_cipher_for_alg_and_key(algid, key, &which) != SECSuccess)
	return PR_FALSE;

    return nss_smime_cipher_allowed(which);
}


/*
 * NSS_SMIME_EncryptionPossible - check if any encryption is allowed
 *
 * This tells whether or not *any* S/MIME encryption can be done,
 * according to policy.  Callers may use this to do nicer user interface
 * (say, greying out a checkbox so a user does not even try to encrypt
 * a message when they are not allowed to) or for any reason they want
 * to check whether S/MIME encryption (or decryption, for that matter)
 * may be done.
 *
 * It takes no arguments.  The return value is a simple boolean:
 *   PR_TRUE means encryption (or decryption) is *possible*
 *	(but may still fail due to other reasons, like because we cannot
 *	find all the necessary certs, etc.; PR_TRUE is *not* a guarantee)
 *   PR_FALSE means encryption (or decryption) is not permitted
 *
 * There are no errors from this routine.
 */
Boolean
SecSMIMEEncryptionPossible(void)
{
    int i;

    for (i = 0; i < smime_cipher_map_count; i++) {
	if (smime_cipher_map[i].allowed)
	    return PR_TRUE;
    }
    return PR_FALSE;
}


static int
nss_SMIME_FindCipherForSMIMECap(NSSSMIMECapability *cap)
{
    int i;
    SECOidTag capIDTag;

    /* we need the OIDTag here */
    capIDTag = SECOID_FindOIDTag(&(cap->capabilityID));

    /* go over all the SMIME ciphers we know and see if we find a match */
    for (i = 0; i < smime_cipher_map_count; i++) {
	if (smime_cipher_map[i].algtag != capIDTag)
	    continue;
	/*
	 * XXX If SECITEM_CompareItem allowed NULLs as arguments (comparing
	 * 2 NULLs as equal and NULL and non-NULL as not equal), we could
	 * use that here instead of all of the following comparison code.
	 */
	if (cap->parameters.Data == NULL && smime_cipher_map[i].parms == NULL)
	    break;	/* both empty: bingo */

	if (cap->parameters.Data != NULL && smime_cipher_map[i].parms != NULL &&
	    cap->parameters.Length == smime_cipher_map[i].parms->Length &&
	    PORT_Memcmp (cap->parameters.Data, smime_cipher_map[i].parms->Data,
			     cap->parameters.Length) == 0)
	{
	    break;	/* both not empty, same length & equal content: bingo */
	}
    }

    if (i == smime_cipher_map_count)
	return 0;				/* no match found */
    else
	return smime_cipher_map[i].cipher;	/* match found, point to cipher */
}

/*
 * smime_choose_cipher - choose a cipher that works for all the recipients
 *
 * "scert"  - sender's certificate
 * "rcerts" - recipient's certificates
 */
static long
smime_choose_cipher(SecCertificateRef scert, SecCertificateRef *rcerts)
{
    PRArenaPool *poolp;
    long cipher;
    long chosen_cipher;
    int *cipher_abilities;
    int *cipher_votes;
    int weak_mapi;
    int strong_mapi;
    int rcount, mapi, max, i;
#if 1
    // @@@ We Don't support Fortezza yet.
    Boolean scert_is_fortezza  = PR_FALSE;
#else
    Boolean scert_is_fortezza = (scert == NULL) ? PR_FALSE : PK11_FortezzaHasKEA(scert);
#endif

    chosen_cipher = SMIME_RC2_CBC_40;		/* the default, LCD */
    weak_mapi = smime_mapi_by_cipher(chosen_cipher);

    poolp = PORT_NewArena (1024);		/* XXX what is right value? */
    if (poolp == NULL)
	goto done;

    cipher_abilities = (int *)PORT_ArenaZAlloc(poolp, smime_cipher_map_count * sizeof(int));
    cipher_votes     = (int *)PORT_ArenaZAlloc(poolp, smime_cipher_map_count * sizeof(int));
    if (cipher_votes == NULL || cipher_abilities == NULL)
	goto done;

    /* If the user has the Fortezza preference turned on, make
     *  that the strong cipher. Otherwise, use triple-DES. */
    strong_mapi = smime_mapi_by_cipher (SMIME_DES_EDE3_168);
    if (scert_is_fortezza) {
	mapi = smime_mapi_by_cipher(SMIME_FORTEZZA);
	if (mapi >= 0 && smime_cipher_map[mapi].enabled)
	    strong_mapi = mapi;
    }

    /* walk all the recipient's certs */
    for (rcount = 0; rcerts[rcount] != NULL; rcount++) {
	CSSM_DATA_PTR profile;
	NSSSMIMECapability **caps;
	int pref;

	/* the first cipher that matches in the user's SMIME profile gets
	 * "smime_cipher_map_count" votes; the next one gets "smime_cipher_map_count" - 1
	 * and so on. If every cipher matches, the last one gets 1 (one) vote */
	pref = smime_cipher_map_count;

	/* find recipient's SMIME profile */
	profile = CERT_FindSMimeProfile(rcerts[rcount]);

	if (profile != NULL && profile->Data != NULL && profile->Length > 0) {
	    /* we have a profile (still DER-encoded) */
	    caps = NULL;
	    /* decode it */
	    if (SEC_ASN1DecodeItem(poolp, &caps, NSSSMIMECapabilitiesTemplate, profile) == SECSuccess &&
		    caps != NULL)
	    {
		/* walk the SMIME capabilities for this recipient */
		for (i = 0; caps[i] != NULL; i++) {
		    cipher = nss_SMIME_FindCipherForSMIMECap(caps[i]);
		    mapi = smime_mapi_by_cipher(cipher);
		    if (mapi >= 0) {
			/* found the cipher */
			cipher_abilities[mapi]++;
			cipher_votes[mapi] += pref;
			--pref;
		    }
		}
	    }
	} else {
	    /* no profile found - so we can only assume that the user can do
	     * the mandatory algorithms which is RC2-40 (weak crypto) and 3DES (strong crypto) */
	    SecPublicKeyRef key;
	    unsigned int pklen_bits;

	    /*
	     * if recipient's public key length is > 512, vote for a strong cipher
	     * please not that the side effect of this is that if only one recipient
	     * has an export-level public key, the strong cipher is disabled.
	     *
	     * XXX This is probably only good for RSA keys.  What I would
	     * really like is a function to just say;  Is the public key in
	     * this cert an export-length key?  Then I would not have to
	     * know things like the value 512, or the kind of key, or what
	     * a subjectPublicKeyInfo is, etc.
	     */
	    key = CERT_ExtractPublicKey(rcerts[rcount]);
	    pklen_bits = 0;
	    if (key != NULL) {
		SecKeyGetStrengthInBits(key, NULL, &pklen_bits);
		SECKEY_DestroyPublicKey (key);
	    }

	    if (pklen_bits > 512) {
		/* cast votes for the strong algorithm */
		cipher_abilities[strong_mapi]++;
		cipher_votes[strong_mapi] += pref;
		pref--;
	    } 

	    /* always cast (possibly less) votes for the weak algorithm */
	    cipher_abilities[weak_mapi]++;
	    cipher_votes[weak_mapi] += pref;
	}
	if (profile != NULL)
	    SECITEM_FreeItem(profile, PR_TRUE);
    }

    /* find cipher that is agreeable by all recipients and that has the most votes */
    max = 0;
    for (mapi = 0; mapi < smime_cipher_map_count; mapi++) {
	/* if not all of the recipients can do this, forget it */
	if (cipher_abilities[mapi] != rcount)
	    continue;
	/* if cipher is not enabled or not allowed by policy, forget it */
	if (!smime_cipher_map[mapi].enabled || !smime_cipher_map[mapi].allowed)
	    continue;
	/* if we're not doing fortezza, but the cipher is fortezza, forget it */
	if (!scert_is_fortezza  && (smime_cipher_map[mapi].cipher == SMIME_FORTEZZA))
	    continue;
	/* now see if this one has more votes than the last best one */
	if (cipher_votes[mapi] >= max) {
	    /* if equal number of votes, prefer the ones further down in the list */
	    /* with the expectation that these are higher rated ciphers */
	    chosen_cipher = smime_cipher_map[mapi].cipher;
	    max = cipher_votes[mapi];
	}
    }
    /* if no common cipher was found, chosen_cipher stays at the default */

done:
    if (poolp != NULL)
	PORT_FreeArena (poolp, PR_FALSE);

    return chosen_cipher;
}

/*
 * XXX This is a hack for now to satisfy our current interface.
 * Eventually, with more parameters needing to be specified, just
 * looking up the keysize is not going to be sufficient.
 */
static int
smime_keysize_by_cipher (unsigned long which)
{
    int keysize;

    switch (which) {
      case SMIME_RC2_CBC_40:
	keysize = 40;
	break;
      case SMIME_RC2_CBC_64:
	keysize = 64;
	break;
      case SMIME_RC2_CBC_128:
      case SMIME_AES_CBC_128:
	keysize = 128;
	break;
      case SMIME_DES_CBC_56:
	keysize = 64;
	break;
      case SMIME_DES_EDE3_168:
	keysize = 192;
	break;
      case SMIME_FORTEZZA:
	/*
	 * This is special; since the key size is fixed, we actually
	 * want to *avoid* specifying a key size.
	 */
	keysize = 0;
	break;
      default:
	keysize = -1;
	break;
    }

    return keysize;
}

/*
 * SecSMIMEFindBulkAlgForRecipients - find bulk algorithm suitable for all recipients
 *
 * it would be great for UI purposes if there would be a way to find out which recipients
 * prevented a strong cipher from being used...
 */
OSStatus
SecSMIMEFindBulkAlgForRecipients(SecCertificateRef *rcerts, SECOidTag *bulkalgtag, int *keysize)
{
    unsigned long cipher;
    int mapi;

    cipher = smime_choose_cipher(NULL, rcerts);
    mapi = smime_mapi_by_cipher(cipher);

    *bulkalgtag = smime_cipher_map[mapi].algtag;
    *keysize = smime_keysize_by_cipher(smime_cipher_map[mapi].cipher);

    return SECSuccess;
}

/*
 * SecSMIMECreateSMIMECapabilities - get S/MIME capabilities for this instance of NSS
 *
 * scans the list of allowed and enabled ciphers and construct a PKCS9-compliant
 * S/MIME capabilities attribute value.
 *
 * XXX Please note that, in contradiction to RFC2633 2.5.2, the capabilities only include
 * symmetric ciphers, NO signature algorithms or key encipherment algorithms.
 *
 * "poolp" - arena pool to create the S/MIME capabilities data on
 * "dest" - CSSM_DATA to put the data in
 * "includeFortezzaCiphers" - PR_TRUE if fortezza ciphers should be included
 */
OSStatus
SecSMIMECreateSMIMECapabilities(SecArenaPoolRef pool, CSSM_DATA_PTR dest, Boolean includeFortezzaCiphers)
{
    PLArenaPool *poolp = (PLArenaPool *)pool;
    NSSSMIMECapability *cap;
    NSSSMIMECapability **smime_capabilities;
    smime_cipher_map_entry *map;
    SECOidData *oiddata;
    CSSM_DATA_PTR dummy;
    int i, capIndex;

    /* if we have an old NSSSMIMECapability array, we'll reuse it (has the right size) */
    /* smime_cipher_map_count + 1 is an upper bound - we might end up with less */
    smime_capabilities = (NSSSMIMECapability **)PORT_ZAlloc((smime_cipher_map_count + 1)
				      * sizeof(NSSSMIMECapability *));
    if (smime_capabilities == NULL)
	return SECFailure;

    capIndex = 0;

    /* Add all the symmetric ciphers
     * We walk the cipher list backwards, as it is ordered by increasing strength,
     * we prefer the stronger cipher over a weaker one, and we have to list the
     * preferred algorithm first */
    for (i = smime_cipher_map_count - 1; i >= 0; i--) {
	/* Find the corresponding entry in the cipher map. */
	map = &(smime_cipher_map[i]);
	if (!map->enabled)
	    continue;

	/* If we're using a non-Fortezza cert, only advertise non-Fortezza
	   capabilities. (We advertise all capabilities if we have a 
	   Fortezza cert.) */
	if ((!includeFortezzaCiphers) && (map->cipher == SMIME_FORTEZZA))
	    continue;

	/* get next SMIME capability */
	cap = (NSSSMIMECapability *)PORT_ZAlloc(sizeof(NSSSMIMECapability));
	if (cap == NULL)
	    break;
	smime_capabilities[capIndex++] = cap;

	oiddata = SECOID_FindOIDByTag(map->algtag);
	if (oiddata == NULL)
	    break;

	cap->capabilityID.Data = oiddata->oid.Data;
	cap->capabilityID.Length = oiddata->oid.Length;
	cap->parameters.Data = map->parms ? map->parms->Data : NULL;
	cap->parameters.Length = map->parms ? map->parms->Length : 0;
	cap->cipher = smime_cipher_map[i].cipher;
    }

    /* XXX add signature algorithms */
    /* XXX add key encipherment algorithms */

    smime_capabilities[capIndex] = NULL;	/* last one - now encode */
    dummy = SEC_ASN1EncodeItem(poolp, dest, &smime_capabilities, NSSSMIMECapabilitiesTemplate);

    /* now that we have the proper encoded SMIMECapabilities (or not),
     * free the work data */
    for (i = 0; smime_capabilities[i] != NULL; i++)
	PORT_Free(smime_capabilities[i]);
    PORT_Free(smime_capabilities);

    return (dummy == NULL) ? SECFailure : SECSuccess;
}

/*
 * SecSMIMECreateSMIMEEncKeyPrefs - create S/MIME encryption key preferences attr value
 *
 * "poolp" - arena pool to create the attr value on
 * "dest" - CSSM_DATA to put the data in
 * "cert" - certificate that should be marked as preferred encryption key
 *          cert is expected to have been verified for EmailRecipient usage.
 */
OSStatus
SecSMIMECreateSMIMEEncKeyPrefs(SecArenaPoolRef pool, CSSM_DATA_PTR dest, SecCertificateRef cert)
{
    PLArenaPool *poolp = (PLArenaPool *)pool;
    NSSSMIMEEncryptionKeyPreference ekp;
    CSSM_DATA_PTR dummy = NULL;
    PLArenaPool *tmppoolp = NULL;

    if (cert == NULL)
	goto loser;

    tmppoolp = PORT_NewArena(1024);
    if (tmppoolp == NULL)
	goto loser;

    /* XXX hardcoded IssuerSN choice for now */
    ekp.selector = NSSSMIMEEncryptionKeyPref_IssuerSN;
    ekp.id.issuerAndSN = CERT_GetCertIssuerAndSN(tmppoolp, cert);
    if (ekp.id.issuerAndSN == NULL)
	goto loser;

    dummy = SEC_ASN1EncodeItem(poolp, dest, &ekp, smime_encryptionkeypref_template);

loser:
    if (tmppoolp) PORT_FreeArena(tmppoolp, PR_FALSE);

    return (dummy == NULL) ? SECFailure : SECSuccess;
}

/*
 * SecSMIMECreateSMIMEEncKeyPrefs - create S/MIME encryption key preferences attr value using MS oid
 *
 * "poolp" - arena pool to create the attr value on
 * "dest" - CSSM_DATA to put the data in
 * "cert" - certificate that should be marked as preferred encryption key
 *          cert is expected to have been verified for EmailRecipient usage.
 */
OSStatus
SecSMIMECreateMSSMIMEEncKeyPrefs(SecArenaPoolRef pool, CSSM_DATA_PTR dest, SecCertificateRef cert)
{
    PLArenaPool *poolp = (PLArenaPool *)pool;
    CSSM_DATA_PTR dummy = NULL;
    PLArenaPool *tmppoolp = NULL;
    SecCmsIssuerAndSN *isn;

    if (cert == NULL)
	goto loser;

    tmppoolp = PORT_NewArena(1024);
    if (tmppoolp == NULL)
	goto loser;

    isn = CERT_GetCertIssuerAndSN(tmppoolp, cert);
    if (isn == NULL)
	goto loser;

    dummy = SEC_ASN1EncodeItem(poolp, dest, isn, SEC_ASN1_GET(SecCmsIssuerAndSNTemplate));

loser:
    if (tmppoolp) PORT_FreeArena(tmppoolp, PR_FALSE);

    return (dummy == NULL) ? SECFailure : SECSuccess;
}

/*
 * SecSMIMEGetCertFromEncryptionKeyPreference -
 *				find cert marked by EncryptionKeyPreference attribute
 *
 * "keychainOrArray" - handle for the cert database to look in
 * "DERekp" - DER-encoded value of S/MIME Encryption Key Preference attribute
 *
 * if certificate is supposed to be found among the message's included certificates,
 * they are assumed to have been imported already.
 */
SecCertificateRef
SecSMIMEGetCertFromEncryptionKeyPreference(SecKeychainRef keychainOrArray, CSSM_DATA_PTR DERekp)
{
    PLArenaPool *tmppoolp = NULL;
    SecCertificateRef cert = NULL;
    NSSSMIMEEncryptionKeyPreference ekp;

    tmppoolp = PORT_NewArena(1024);
    if (tmppoolp == NULL)
	return NULL;

    /* decode DERekp */
    if (SEC_ASN1DecodeItem(tmppoolp, &ekp, smime_encryptionkeypref_template, DERekp) != SECSuccess)
	goto loser;

    /* find cert */
    switch (ekp.selector) {
    case NSSSMIMEEncryptionKeyPref_IssuerSN:
	cert = CERT_FindCertByIssuerAndSN(keychainOrArray, NULL, NULL, ekp.id.issuerAndSN);
	break;
    case NSSSMIMEEncryptionKeyPref_RKeyID:
    case NSSSMIMEEncryptionKeyPref_SubjectKeyID:
	/* XXX not supported yet - we need to be able to look up certs by SubjectKeyID */
	break;
    default:
	PORT_Assert(0);
    }
loser:
    if (tmppoolp) PORT_FreeArena(tmppoolp, PR_FALSE);

    return cert;
}

#if 0
extern const char __nss_smime_rcsid[];
extern const char __nss_smime_sccsid[];
#endif

Boolean
NSSSMIME_VersionCheck(const char *importedVersion)
{
#if 1
    return PR_TRUE;
#else
    /*
     * This is the secret handshake algorithm.
     *
     * This release has a simple version compatibility
     * check algorithm.  This release is not backward
     * compatible with previous major releases.  It is
     * not compatible with future major, minor, or
     * patch releases.
     */
    volatile char c; /* force a reference that won't get optimized away */

    c = __nss_smime_rcsid[0] + __nss_smime_sccsid[0]; 

    return NSS_VersionCheck(importedVersion);
#endif
}

