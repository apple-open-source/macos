/*
 * crypto.h - public data structures and prototypes for the crypto library
 *
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

#include "cryptohi.h"

#include "secoid.h"
#include "cmspriv.h"
#include <security_asn1/secerr.h>
#include <Security/cssmapi.h>
#include <Security/cssmapi.h>
#include <Security/SecKeyPriv.h>
#include <Security/cssmapple.h>

static CSSM_CSP_HANDLE gCsp = 0;
static char gCssmInitialized = 0;

/* @@@ Ugly hack casting, but the extra argument at the end will be ignored. */
static CSSM_API_MEMORY_FUNCS memFuncs =
{
    (CSSM_MALLOC)malloc,
    (CSSM_FREE)free,
    (CSSM_REALLOC)realloc,
    (CSSM_CALLOC)calloc,
    NULL
};

/*
 *
 * SecCspHandleForAlgorithm
 * @@@ This function should get more parameters like keysize and operation required and use mds.
 *
 */
CSSM_CSP_HANDLE
SecCspHandleForAlgorithm(CSSM_ALGORITHMS algorithm)
{

    if (!gCsp)
    {
	CSSM_VERSION version = { 2, 0 };
	CSSM_RETURN rv;

	if (!gCssmInitialized)
	{
	    CSSM_GUID myGuid = { 0xFADE, 0, 0, { 1, 2, 3, 4, 5, 6, 7, 0 } };
	    CSSM_PVC_MODE pvcPolicy = CSSM_PVC_NONE;
    
	    rv = CSSM_Init (&version, CSSM_PRIVILEGE_SCOPE_NONE, &myGuid, CSSM_KEY_HIERARCHY_NONE, &pvcPolicy, NULL);
	    if (rv)
		goto loser;
	    gCssmInitialized = 1;
	}

	rv = CSSM_ModuleLoad(&gGuidAppleCSP, CSSM_KEY_HIERARCHY_NONE, NULL, NULL);
	if (rv)
	    goto loser;
	rv = CSSM_ModuleAttach(&gGuidAppleCSP, &version, &memFuncs, 0, CSSM_SERVICE_CSP, 0, CSSM_KEY_HIERARCHY_NONE, NULL, 0, NULL, &gCsp);
    }

loser:
    return gCsp;
}

CSSM_ALGORITHMS
SECOID_FindyCssmAlgorithmByTag(SECOidTag algTag)
{
    const SECOidData *oidData = SECOID_FindOIDByTag(algTag);
    return oidData ? oidData->cssmAlgorithm : CSSM_ALGID_NONE;
}

SECStatus
SEC_SignData(SecAsn1Item *result, unsigned char *buf, int len,
	    SecPrivateKeyRef pk, SECOidTag digAlgTag, SECOidTag sigAlgTag)
{
    const CSSM_ACCESS_CREDENTIALS *accessCred;
    CSSM_ALGORITHMS algorithm;
    CSSM_CC_HANDLE cc = 0;
    CSSM_CSP_HANDLE csp;
    OSStatus rv;
    SecAsn1Item dataBuf = { (uint32)len, (uint8_t *)buf };
    SecAsn1Item sig = {};
    const CSSM_KEY *key;

    algorithm = SECOID_FindyCssmAlgorithmByTag(SecCmsUtilMakeSignatureAlgorithm(digAlgTag, sigAlgTag));
    if (!algorithm)
    {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	rv = SECFailure;
	goto loser;
    }

    rv = SecKeyGetCSPHandle(pk, &csp);
    if (rv) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
	goto loser;
    }
    rv = SecKeyGetCSSMKey(pk, &key);
    if (rv) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
	goto loser;
    }
    rv = SecKeyGetCredentials(pk, CSSM_ACL_AUTHORIZATION_SIGN, kSecCredentialTypeDefault, &accessCred);
    if (rv) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
	goto loser;
    }

    rv = CSSM_CSP_CreateSignatureContext(csp, algorithm, accessCred, key, &cc);
    if (rv) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
    	goto loser;
    }

    rv = CSSM_SignData(cc, &dataBuf, 1, CSSM_ALGID_NONE, &sig);
    if (rv) {
        SECErrorCodes code;
        if (CSSM_ERRCODE(rv) == CSSM_ERRCODE_USER_CANCELED
            || CSSM_ERRCODE(rv) == CSSM_ERRCODE_OPERATION_AUTH_DENIED)
            code = SEC_ERROR_USER_CANCELLED;
        else if (CSSM_ERRCODE(rv) == CSSM_ERRCODE_NO_USER_INTERACTION
                 || rv == CSSMERR_CSP_KEY_USAGE_INCORRECT)
            code = SEC_ERROR_INADEQUATE_KEY_USAGE;
        else
            code = SEC_ERROR_LIBRARY_FAILURE;

        PORT_SetError(code);
	goto loser;
    }

    result->Length = sig.Length;
    result->Data = sig.Data;

loser:
    if (cc)
	CSSM_DeleteContext(cc);

    return rv;
}

SECStatus
SGN_Digest(SecPrivateKeyRef pk, SECOidTag digAlgTag, SECOidTag sigAlgTag, SecAsn1Item *result, SecAsn1Item *digest)
{
    const CSSM_ACCESS_CREDENTIALS *accessCred;
    CSSM_ALGORITHMS digalg, sigalg;
    CSSM_CC_HANDLE cc = 0;
    CSSM_CSP_HANDLE csp;
    const CSSM_KEY *key;
    SecAsn1Item sig = {};
    OSStatus rv;

    digalg = SECOID_FindyCssmAlgorithmByTag(digAlgTag);
    sigalg = SECOID_FindyCssmAlgorithmByTag(sigAlgTag);
    if (!digalg || !sigalg)
    {
        PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	rv = SECFailure;
	goto loser;
    }

    rv = SecKeyGetCSPHandle(pk, &csp);
    if (rv) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
	goto loser;
    }
    rv = SecKeyGetCSSMKey(pk, &key);
    if (rv) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
	goto loser;
    }
    rv = SecKeyGetCredentials(pk, CSSM_ACL_AUTHORIZATION_SIGN, kSecCredentialTypeDefault, &accessCred);
    if (rv) {
        PORT_SetError(SEC_ERROR_BAD_KEY);
	goto loser;
    }

    rv = CSSM_CSP_CreateSignatureContext(csp, sigalg, accessCred, key, &cc);
    if (rv) {
        PORT_SetError(SEC_ERROR_NO_MEMORY);
	goto loser;
    }

    rv = CSSM_SignData(cc, digest, 1, digalg, &sig);
    if (rv) {
        SECErrorCodes code;
        if (CSSM_ERRCODE(rv) == CSSM_ERRCODE_USER_CANCELED
            || CSSM_ERRCODE(rv) == CSSM_ERRCODE_OPERATION_AUTH_DENIED)
            code = SEC_ERROR_USER_CANCELLED;
        else if (CSSM_ERRCODE(rv) == CSSM_ERRCODE_NO_USER_INTERACTION
                 || rv == CSSMERR_CSP_KEY_USAGE_INCORRECT)
            code = SEC_ERROR_INADEQUATE_KEY_USAGE;
        else
            code = SEC_ERROR_LIBRARY_FAILURE;

        PORT_SetError(code);
	goto loser;
    }

    result->Length = sig.Length;
    result->Data = sig.Data;

loser:
    if (cc)
	CSSM_DeleteContext(cc);

    return rv;
}

SECStatus
VFY_VerifyData(unsigned char *buf, int len,
		SecPublicKeyRef pk, SecAsn1Item *sig,
		SECOidTag digAlgTag, SECOidTag sigAlgTag, void *wincx)
{
    SECOidTag algTag;
    CSSM_ALGORITHMS algorithm;
    CSSM_CC_HANDLE cc = 0;
    CSSM_CSP_HANDLE csp;
    OSStatus rv = SECFailure;
    SecAsn1Item dataBuf = { (uint32)len, (uint8_t *)buf };
    const CSSM_KEY *key;

    algTag = SecCmsUtilMakeSignatureAlgorithm(digAlgTag, sigAlgTag);
    algorithm = SECOID_FindyCssmAlgorithmByTag(algTag);
    if (!algorithm)
    {
	rv = algTag == SEC_OID_UNKNOWN ? SecCmsVSSignatureAlgorithmUnknown : SecCmsVSSignatureAlgorithmUnsupported;
	goto loser;
    }

    rv = SecKeyGetCSPHandle(pk, &csp);
    if (rv)
	goto loser;
    rv = SecKeyGetCSSMKey(pk, &key);
    if (rv)
	goto loser;

    rv = CSSM_CSP_CreateSignatureContext(csp, algorithm, NULL, key, &cc);
    if (rv)
	goto loser;

    rv = CSSM_VerifyData(cc, &dataBuf, 1, CSSM_ALGID_NONE, sig);

loser:
    if (cc)
	CSSM_DeleteContext(cc);

    return rv;
}

SECStatus
VFY_VerifyDigest(SecAsn1Item *digest, SecPublicKeyRef pk,
		SecAsn1Item *sig, SECOidTag digAlgTag, SECOidTag sigAlgTag, void *wincx)
{
    CSSM_ALGORITHMS sigalg, digalg;
    CSSM_CC_HANDLE cc = 0;
    CSSM_CSP_HANDLE csp;
    const CSSM_KEY *key;
    OSStatus rv;

    digalg = SECOID_FindyCssmAlgorithmByTag(digAlgTag);
    sigalg = SECOID_FindyCssmAlgorithmByTag(sigAlgTag);
    if (!digalg || !sigalg)
    {
	rv = digAlgTag == SEC_OID_UNKNOWN  || sigAlgTag == SEC_OID_UNKNOWN ? SecCmsVSSignatureAlgorithmUnknown : SecCmsVSSignatureAlgorithmUnsupported;
	goto loser;
    }

    rv = SecKeyGetCSPHandle(pk, &csp);
    if (rv)
	goto loser;
    rv = SecKeyGetCSSMKey(pk, &key);
    if (rv)
	goto loser;

    rv = CSSM_CSP_CreateSignatureContext(csp, sigalg, NULL, key, &cc);
    if (rv)
	goto loser;

    rv = CSSM_VerifyData(cc, digest, 1, digalg, sig);

loser:
    if (cc)
	CSSM_DeleteContext(cc);

    return rv;
}

SECStatus
WRAP_PubWrapSymKey(SecPublicKeyRef publickey,
		   SecSymmetricKeyRef bulkkey,
		   SecAsn1Item * encKey)
{
    CSSM_WRAP_KEY wrappedKey = {};
    //CSSM_WRAP_KEY wrappedPk = {}
    //CSSM_KEY upk = {};
    CSSM_CC_HANDLE cc = 0;
    CSSM_CSP_HANDLE pkCsp, bkCsp;
    const CSSM_KEY *pk, *bk, *pubkey;
    OSStatus rv;
    CSSM_ACCESS_CREDENTIALS accessCred = {};

    rv = SecKeyGetCSPHandle(publickey, &pkCsp);
    if (rv)
	goto loser;
    rv = SecKeyGetCSSMKey(publickey, &pk);
    if (rv)
	goto loser;

    rv = SecKeyGetCSPHandle(bulkkey, &bkCsp);
    if (rv)
	goto loser;
    rv = SecKeyGetCSSMKey(bulkkey, &bk);
    if (rv)
	goto loser;

#if 1
    pubkey = pk;
#else
    /* We need to get the publickey out of it's pkCsp and into the bkCsp so we can operate with it. */

    /* Make a NULL wrap symmetric context to extract the public key from pkCsp. */
    rv = CSSM_CSP_CreateSymmetricContext(pkCsp,
	    CSSM_ALGID_NONE,
	    CSSM_MODE_NONE,
	    NULL, /* accessCred */
	    NULL, /* key */
	    NULL, /* iv */
	    CSSM_PADDING_NONE,
	    NULL, /* reserved */
	    &cc);
    if (rv)
	goto loser;
    rv = CSSM_WrapKey(cc,
	    NULL /* accessCred */,
	    pk,
	    NULL /* descriptiveData */,
	    &wrappedPk);
    CSSM_DeleteContext(cc);
    cc = 0;

    /* Make a NULL unwrap symmetric context to import the public key into bkCsp. */
    rv = CSSM_CSP_CreateSymmetricContext(bkCsp,
	    CSSM_ALGID_NONE,
	    CSSM_MODE_NONE,
	    NULL, /* accessCred */
	    NULL, /* key */
	    NULL, /* iv */
	    CSSM_PADDING_NONE,
	    NULL, /* reserved */
	    &cc);
    if (rv)
	goto loser;
    rv = CSSM_UnwrapKey(cc, NULL, &wrappedPk, usage, attr, NULL /* label */, NULL /* rcc */, &upk, NULL /* descriptiveData */);
    CSSM_DeleteContext(cc);
    cc = 0;

    pubkey  = &upk;
#endif

    rv = CSSM_CSP_CreateAsymmetricContext(bkCsp,
	    pubkey->KeyHeader.AlgorithmId,
	    &accessCred,
	    pubkey,
	    CSSM_PADDING_PKCS1,
	    &cc);
    if (rv)
	goto loser;

    {
	/* Set the wrapped key format to indicate we want just the raw bits encrypted. */
	CSSM_CONTEXT_ATTRIBUTE contextAttribute = { CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT, sizeof(uint32) };
	contextAttribute.Attribute.Uint32 = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7;
	rv = CSSM_UpdateContextAttributes(cc, 1, &contextAttribute);
	if (rv)
	    goto loser;
    }

    {
	/* Set the mode to CSSM_ALGMODE_PKCS1_EME_V15. */
	CSSM_CONTEXT_ATTRIBUTE contextAttribute = { CSSM_ATTRIBUTE_MODE, sizeof(uint32) };
	contextAttribute.Attribute.Uint32 = CSSM_ALGMODE_NONE; /* CSSM_ALGMODE_PKCS1_EME_V15 */
	rv = CSSM_UpdateContextAttributes(cc, 1, &contextAttribute);
	if (rv)
	    goto loser;
    }

    {
	// @@@ Stick in an empty initVector to work around a csp bug.
	SecAsn1Item initVector = {};
	CSSM_CONTEXT_ATTRIBUTE contextAttribute = { CSSM_ATTRIBUTE_INIT_VECTOR, sizeof(SecAsn1Item *) };
	contextAttribute.Attribute.Data = &initVector;
	rv = CSSM_UpdateContextAttributes(cc, 1, &contextAttribute);
	if (rv)
	    goto loser;
    }

    rv = CSSM_WrapKey(cc,
	    &accessCred,
	    bk,
	    NULL, /* descriptiveData */
	    &wrappedKey);
    if (rv)
	goto loser;

    // @@@ Fix leaks!
    if (encKey->Length < wrappedKey.KeyData.Length)
	abort();
    encKey->Length = wrappedKey.KeyData.Length;
    memcpy(encKey->Data, wrappedKey.KeyData.Data, encKey->Length);
    CSSM_FreeKey(bkCsp, NULL /* credentials */, &wrappedKey, FALSE);

loser:
    if (cc)
	CSSM_DeleteContext(cc);

    return rv;
}

SecSymmetricKeyRef
WRAP_PubUnwrapSymKey(SecPrivateKeyRef privkey, const SecAsn1Item *encKey, SECOidTag bulkalgtag)
{
    SecSymmetricKeyRef bulkkey = NULL;
    CSSM_WRAP_KEY wrappedKey = {};
    CSSM_CC_HANDLE cc = 0;
    CSSM_CSP_HANDLE pkCsp;
    const CSSM_KEY *pk;
    CSSM_KEY unwrappedKey = {};
    const CSSM_ACCESS_CREDENTIALS *accessCred;
    SecAsn1Item descriptiveData = {};
    CSSM_ALGORITHMS bulkalg;
    OSStatus rv;

    rv = SecKeyGetCSPHandle(privkey, &pkCsp);
    if (rv)
	goto loser;
    rv = SecKeyGetCSSMKey(privkey, &pk);
    if (rv)
	goto loser;
    rv = SecKeyGetCredentials(privkey,
	    CSSM_ACL_AUTHORIZATION_DECRYPT, /* @@@ Should be UNWRAP */
	    kSecCredentialTypeDefault,
	    &accessCred);
    if (rv)
	goto loser;

    bulkalg = SECOID_FindyCssmAlgorithmByTag(bulkalgtag);
    if (!bulkalg)
    {
	rv = SEC_ERROR_INVALID_ALGORITHM;
	goto loser;
    }

    rv = CSSM_CSP_CreateAsymmetricContext(pkCsp,
	    pk->KeyHeader.AlgorithmId,
	    accessCred,
	    pk,
	    CSSM_PADDING_PKCS1,
	    &cc);
    if (rv)
	goto loser;

    {
	// @@@ Stick in an empty initvector to work around a csp bug.
	SecAsn1Item initVector = {};
	CSSM_CONTEXT_ATTRIBUTE contextAttribute = { CSSM_ATTRIBUTE_INIT_VECTOR, sizeof(SecAsn1Item *) };
	contextAttribute.Attribute.Data = &initVector;
	rv = CSSM_UpdateContextAttributes(cc, 1, &contextAttribute);
	if (rv)
	    goto loser;
    }

    wrappedKey.KeyHeader.HeaderVersion = CSSM_KEYHEADER_VERSION;
    wrappedKey.KeyHeader.BlobType = CSSM_KEYBLOB_WRAPPED;
    wrappedKey.KeyHeader.Format = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7;
    wrappedKey.KeyHeader.AlgorithmId = bulkalg;
    wrappedKey.KeyHeader.KeyClass = CSSM_KEYCLASS_SESSION_KEY;
    wrappedKey.KeyHeader.WrapAlgorithmId = pk->KeyHeader.AlgorithmId;
    wrappedKey.KeyHeader.WrapMode = CSSM_ALGMODE_NONE; /* CSSM_ALGMODE_PKCS1_EME_V15 */
    wrappedKey.KeyData = *encKey;

    rv = CSSM_UnwrapKey(cc,
	    NULL, /* publicKey */
	    &wrappedKey,
	    CSSM_KEYUSE_DECRYPT,
	    CSSM_KEYATTR_EXTRACTABLE /* | CSSM_KEYATTR_RETURN_DATA */,
	    NULL, /* keyLabel */
	    NULL, /* rcc */
	    &unwrappedKey,
	    &descriptiveData);
    if (rv) {
        SECErrorCodes code;
        if (CSSM_ERRCODE(rv) == CSSM_ERRCODE_USER_CANCELED
            || CSSM_ERRCODE(rv) == CSSM_ERRCODE_OPERATION_AUTH_DENIED)
            code = SEC_ERROR_USER_CANCELLED;
        else if (CSSM_ERRCODE(rv) == CSSM_ERRCODE_NO_USER_INTERACTION
                 || rv == CSSMERR_CSP_KEY_USAGE_INCORRECT)
            code = SEC_ERROR_INADEQUATE_KEY_USAGE;
        else
            code = SEC_ERROR_LIBRARY_FAILURE;

        PORT_SetError(code);
	goto loser;
    }

    // @@@ Export this key from the csp/dl and import it to the standard csp
    rv = SecKeyCreate(&unwrappedKey, &bulkkey);
    if (rv)
	goto loser;

loser:
    if (rv)
	PORT_SetError(rv);

    if (cc)
	CSSM_DeleteContext(cc);

    return bulkkey;
}
