/*
 *  cert.c
 *  security_smime
 *
 *  Created by john on Wed Mar 12 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#include "cert.h"
#include "cmstpriv.h"
#include "cmslocal.h"
#include "secitem.h"
#include <security_asn1/secerr.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecIdentitySearch.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicySearch.h>
#include <Security/oidsalg.h>
#include <Security/cssmapi.h>
#include <Security/oidscert.h>
#include <Security/oidscert.h>

/* for errKCDuplicateItem */
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#define CERT_DEBUG	0
#if	CERT_DEBUG
#define dprintf(args...)      printf(args)
#else
#define dprintf(args...)
#endif

/* @@@ Remove this once it's back in the appropriate header. */
static const uint8 X509V1IssuerNameStd[] = {INTEL_X509V3_CERT_R08, 23};
static const CSSM_OID OID_X509V1IssuerNameStd = {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1IssuerNameStd};

/* 
 * Normalize a Printable String. Per RFC2459 (4.1.2.4), printable strings are case 
 * insensitive and we're supposed to ignore leading and trailing 
 * whitespace, and collapse multiple whitespace characters into one. 
 */
static void
CERT_NormalizeString(CSSM_DATA_PTR string)
{
    char *pD, *pCh, *pEos;

    if (!string->Length)
	return;

    pD = pCh = (char *)string->Data;
    pEos = pCh + string->Length - 1;

    /* Strip trailing NULL terminators */
    while(*pEos == 0)
	pEos--;
    
    /* Remove trailing spaces */
    while(isspace(*pEos))
	pEos--;

    /* Point to one past last non-space character */
    pEos++;

    /* skip all leading whitespace */
    while(isspace(*pCh) && (pCh < pEos))
	pCh++;

    /* Eliminate multiple whitespace and convent to upper case.
     * pCh points to first non-white char.
     * pD still points to start of string. */
    while(pCh < pEos)
    {
	char ch = *pCh++;
	*pD++ = toupper(ch);
	if(isspace(ch))
	{
	    /* skip 'til next nonwhite */
	    while(isspace(*pCh) && (pCh < pEos))
		pCh++;
	}
    }

    string->Length = pD - (char *)string->Data;
}

/* 
 * Normalize an RDN. Per RFC2459 (4.1.2.4), printable strings are case 
 * insensitive and we're supposed to ignore leading and trailing 
 * whitespace, and collapse multiple whitespace characters into one. 
 *
 * Incoming NSS_Name is assumed to be entirely within specifed coder's
 * address space; we'll be munging some of that and possibly replacing
 * some pointers with others allocated from the same space. 
 */
void
CERT_NormalizeX509NameNSS(NSS_Name *nssName)
{
    NSS_RDN *rdn;

    for (rdn = *nssName->rdns; rdn; ++rdn)
    {
	NSS_ATV *attr;
	for (attr = *rdn->atvs; attr; ++attr)
	{
	    /* 
		* attr->value is an ASN_ANY containing an encoded
		* string. We only normalize Prinatable String types. 
		* If we find one, decode it, normalize it, encode the
		* result, and put the encoding back in attr->value.
		* We temporarily "leak" the original string, which only
		* has a lifetime of the incoming SecNssCoder. 
		*/
	    NSS_TaggedItem *attrVal = &attr->value;
	    if(attrVal->tag != SEC_ASN1_PRINTABLE_STRING)
		continue;

	    CERT_NormalizeString(&attrVal->item);
	}
    }
}

SecCertificateRef CERT_FindCertByNicknameOrEmailAddr(SecKeychainRef keychainOrArray, char *name)
{
   SecCertificateRef certificate;
    OSStatus status=SecCertificateFindByEmail(keychainOrArray,name,&certificate);
    return status==noErr?certificate:NULL;
}

SecPublicKeyRef SECKEY_CopyPublicKey(SecPublicKeyRef pubKey)
{
    CFRetain(pubKey);
    return pubKey;
}

void SECKEY_DestroyPublicKey(SecPublicKeyRef pubKey)
{
    CFRelease(pubKey);
}

SecPublicKeyRef SECKEY_CopyPrivateKey(SecPublicKeyRef privKey)
{
    CFRetain(privKey);
    return privKey;
}

void SECKEY_DestroyPrivateKey(SecPublicKeyRef privKey)
{
    CFRelease(privKey);
}

void CERT_DestroyCertificate(SecCertificateRef cert)
{
    CFRelease(cert);
}

SecCertificateRef CERT_DupCertificate(SecCertificateRef cert)
{
    CFRetain(cert);
    return cert;
}

SecIdentityRef CERT_FindIdentityByUsage(SecKeychainRef keychainOrArray,
			 char *nickname, SECCertUsage usage, Boolean validOnly, void *proto_win)
{
    SecIdentityRef identityRef = NULL;
    SecCertificateRef cert = CERT_FindCertByNicknameOrEmailAddr(keychainOrArray, nickname);
    if (!cert)
	return NULL;

    SecIdentityCreateWithCertificate(keychainOrArray, cert, &identityRef);
    CFRelease(cert);

    return identityRef;
}

SecCertificateRef CERT_FindUserCertByUsage(SecKeychainRef keychainOrArray,
			 char *nickname,SECCertUsage usage,Boolean validOnly,void *proto_win)
{
    SecItemClass itemClass = kSecCertificateItemClass;
    SecKeychainSearchRef searchRef;
    SecKeychainItemRef itemRef = NULL;
    OSStatus status;
    SecKeychainAttribute attrs[1];
    const char *serialNumber = "12345678";
 //   const SecKeychainAttributeList attrList;
#if 0
    attrs[1].tag = kSecLabelItemAttr;
    attrs[1].length = strlen(nickname)+1;
    attrs[1].data = nickname;
#else
    attrs[1].tag = kSecSerialNumberItemAttr;
    attrs[1].length = (UInt32)strlen(serialNumber)+1;
    attrs[1].data = (uint8 *)serialNumber;
#endif
    SecKeychainAttributeList attrList = { 0, attrs };
 //   12 34 56 78
	status = SecKeychainSearchCreateFromAttributes(keychainOrArray,itemClass,&attrList,&searchRef);
    if (status)
    {
        printf("CERT_FindUserCertByUsage: SecKeychainSearchCreateFromAttributes:%d",(int)status);
        return NULL;
    }
	status = SecKeychainSearchCopyNext(searchRef,&itemRef);
    if (status)
    	printf("CERT_FindUserCertByUsage: SecKeychainSearchCopyNext:%d",(int)status);
    CFRelease(searchRef);
    return (SecCertificateRef)itemRef;
}

/*
startNewClass(X509Certificate)
CertType, kSecCertTypeItemAttr, "CertType", 0, NULL, UINT32)
CertEncoding, kSecCertEncodingItemAttr, "CertEncoding", 0, NULL, UINT32)
PrintName, kSecLabelItemAttr, "PrintName", 0, NULL, BLOB)
Alias, kSecAlias, "Alias", 0, NULL, BLOB)
Subject, kSecSubjectItemAttr, "Subject", 0, NULL, BLOB)
Issuer, kSecIssuerItemAttr, "Issuer", 0, NULL, BLOB)
SerialNumber, kSecSerialNumberItemAttr, "SerialNumber", 0, NULL, BLOB)
SubjectKeyIdentifier, kSecSubjectKeyIdentifierItemAttr, "SubjectKeyIdentifier", 0, NULL, BLOB)
PublicKeyHash, kSecPublicKeyHashItemAttr, "PublicKeyHash", 0, NULL, BLOB)
endNewClass()
*/

CFArrayRef CERT_CertChainFromCert(SecCertificateRef cert, SECCertUsage usage, Boolean includeRoot)
{
    SecPolicySearchRef searchRef = NULL;
    SecPolicyRef policy = NULL;
    CFArrayRef wrappedCert = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certChain = NULL;
    CSSM_TP_APPLE_EVIDENCE_INFO *statusChain;
    CFDataRef actionData = NULL;
    OSStatus status = 0;

    if (!cert)
	goto loser;

    status = SecPolicySearchCreate(CSSM_CERT_X_509v3, &CSSMOID_APPLE_X509_BASIC, NULL, &searchRef);
    if (status)
	goto loser;
    status = SecPolicySearchCopyNext(searchRef, &policy);
    if (status)
	goto loser;

    wrappedCert = CERT_CertListFromCert(cert);
    status = SecTrustCreateWithCertificates(wrappedCert, policy, &trust);
    if (status)
	goto loser;

    /* Tell SecTrust that we don't care if any certs in the chain have expired,
       nor do we want to stop when encountering a cert with a trust setting;
       we always want to build the full chain.
    */
    CSSM_APPLE_TP_ACTION_DATA localActionData = {
        CSSM_APPLE_TP_ACTION_VERSION,
        CSSM_TP_ACTION_ALLOW_EXPIRED | CSSM_TP_ACTION_ALLOW_EXPIRED_ROOT
    };
    actionData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)&localActionData, sizeof(localActionData), kCFAllocatorNull);
    if (!actionData)
        goto loser;

    status = SecTrustSetParameters(trust, CSSM_TP_ACTION_DEFAULT, actionData);
    if (status)
        goto loser;

    status = SecTrustEvaluate(trust, NULL);
    if (status)
	goto loser;

    status = SecTrustGetResult(trust, NULL, &certChain, &statusChain);
    if (status)
	goto loser;

    /* We don't drop the root if there is only 1 (self signed) certificate in the chain. */
    if (!includeRoot && CFArrayGetCount(certChain) > 1)
    {
	CFMutableArrayRef subChain =  CFArrayCreateMutableCopy(NULL, 0, certChain);
	CFRelease(certChain);
	certChain = subChain;
	if (subChain)
	    CFArrayRemoveValueAtIndex(subChain, CFArrayGetCount(subChain) - 1);
    }

loser:
    if (searchRef)
	CFRelease(searchRef);
    if (policy)
	CFRelease(policy);
    if (wrappedCert)
	CFRelease(wrappedCert);
    if (trust)
	CFRelease(trust);
    if (actionData)
	CFRelease(actionData);
    if (certChain && status)
    {
	CFRelease(certChain);
	certChain = NULL;
    }

    return certChain;
}

CFArrayRef CERT_CertListFromCert(SecCertificateRef cert)
{
    const void *value = cert;
    return cert ? CFArrayCreate(NULL, &value, 1, &kCFTypeArrayCallBacks) : NULL;
}

CFArrayRef CERT_DupCertList(CFArrayRef oldList)
{
    CFRetain(oldList);
    return oldList;
}

// Extract a public key object from a SubjectPublicKeyInfo
SecPublicKeyRef CERT_ExtractPublicKey(SecCertificateRef cert)
{
    SecPublicKeyRef keyRef = NULL;
    SecCertificateCopyPublicKey(cert,&keyRef);
    return keyRef;
}

SECStatus CERT_CheckCertUsage (SecCertificateRef cert,unsigned char usage)
{
    // abort();
    // @@@ It's all good, it's ok.
    return SECSuccess;
}

// Find a certificate in the database by a email address
// "emailAddr" is the email address to look up
SecCertificateRef CERT_FindCertByEmailAddr(SecKeychainRef keychainOrArray, char *emailAddr)
{
    abort();
    return NULL;
}

// Find a certificate in the database by a DER encoded certificate
// "derCert" is the DER encoded certificate
SecCertificateRef CERT_FindCertByDERCert(SecKeychainRef keychainOrArray, const SECItem *derCert)
{
    // @@@ Technically this should look though keychainOrArray for a cert matching this one I guess.
    SecCertificateRef cert = NULL;
    OSStatus rv;

    rv = SecCertificateCreateFromData(derCert, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER, &cert);
    if (rv && cert)
    {
	PORT_SetError(SEC_ERROR_NO_EMAIL_CERT);
	CFRelease(cert);
	cert = NULL;
    }

    return cert;
}

static int compareCssmData(
    const CSSM_DATA *d1,
    const CSSM_DATA *d2)
{
    if((d1 == NULL) || (d2 == NULL)) {
	return 0;
    }
    if(d1->Length != d2->Length) {
	return 0;
    }
    if(memcmp(d1->Data, d2->Data, d1->Length)) {
	return 0;
    }
    return 1;
}

// Generate a certificate key from the issuer and serialnumber, then look it up in the database.
// Return the cert if found. "issuerAndSN" is the issuer and serial number to look for
SecCertificateRef CERT_FindCertByIssuerAndSN (CFTypeRef keychainOrArray, 
    CSSM_DATA_PTR *rawCerts, PRArenaPool *pl, const SecCmsIssuerAndSN *issuerAndSN)
{
    SecCertificateRef certificate;
    int numRawCerts = SecCmsArrayCount((void **)rawCerts);
    int dex;
    OSStatus ortn;
    
    /* 
     * First search the rawCerts array.
     */
    for(dex=0; dex<numRawCerts; dex++) {
	ortn = SecCertificateCreateFromData(rawCerts[dex], 
	    CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER,
	    &certificate);
	if(ortn) {
	    continue;
	}
	SecCmsIssuerAndSN *isn = CERT_GetCertIssuerAndSN(pl, certificate);
	if(isn == NULL) {
	    CFRelease(certificate);
	    continue;
	}
	if(!compareCssmData(&isn->derIssuer, &issuerAndSN->derIssuer)) {
	    CFRelease(certificate);
	    continue;
	}
	if(!compareCssmData(&isn->serialNumber, &issuerAndSN->serialNumber)) {
	    CFRelease(certificate);
	    continue;
	}
	/* got it */
	dprintf("CERT_FindCertByIssuerAndSN: found cert %p\n", certificate);
	return certificate;
    }
    
    /* now search keychain(s) */
    OSStatus status = SecCertificateFindByIssuerAndSN(keychainOrArray, &issuerAndSN->derIssuer,
	&issuerAndSN->serialNumber, &certificate);
    if (status)
    {
	PORT_SetError(SEC_ERROR_NO_EMAIL_CERT);
	certificate = NULL;
    }

    return certificate;
}

SecCertificateRef CERT_FindCertBySubjectKeyID (CFTypeRef keychainOrArray, 
    CSSM_DATA_PTR *rawCerts, const SECItem *subjKeyID)
{
    SecCertificateRef certificate;
    int numRawCerts = SecCmsArrayCount((void **)rawCerts);
    int dex;
    OSStatus ortn;
    SECItem skid;
    
    /* 
     * First search the rawCerts array.
     */
    for(dex=0; dex<numRawCerts; dex++) {
	int match;
	ortn = SecCertificateCreateFromData(rawCerts[dex], 
	    CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER,
	    &certificate);
	if(ortn) {
	    continue;
	}
	if(CERT_FindSubjectKeyIDExtension(certificate, &skid)) {
	    CFRelease(certificate);
	    /* not present */
	    continue;
	}
	match = compareCssmData(subjKeyID, &skid);
	SECITEM_FreeItem(&skid, PR_FALSE);
	if(match) {
	    /* got it */
	    return certificate;
	}
	CFRelease(certificate);
    }

    /* now search keychain(s) */
    OSStatus status = SecCertificateFindBySubjectKeyID(keychainOrArray,subjKeyID,&certificate);
    if (status)
    {
	PORT_SetError(SEC_ERROR_NO_EMAIL_CERT);
	certificate = NULL;
    }

    return certificate;
}

static SecIdentityRef
CERT_FindIdentityByCertificate (CFTypeRef keychainOrArray, SecCertificateRef certificate)
{
    SecIdentityRef  identity = NULL;
    SecIdentityCreateWithCertificate(keychainOrArray, certificate, &identity);
    if (!identity)
	PORT_SetError(SEC_ERROR_NOT_A_RECIPIENT);

    return identity;
}

SecIdentityRef
CERT_FindIdentityByIssuerAndSN (CFTypeRef keychainOrArray, const SecCmsIssuerAndSN *issuerAndSN)
{
    SecCertificateRef certificate = CERT_FindCertByIssuerAndSN(keychainOrArray, NULL, NULL, issuerAndSN);
    if (!certificate)
	return NULL;

    return CERT_FindIdentityByCertificate(keychainOrArray, certificate);
}

SecIdentityRef
CERT_FindIdentityBySubjectKeyID (CFTypeRef keychainOrArray, const SECItem *subjKeyID)
{
    SecCertificateRef certificate = CERT_FindCertBySubjectKeyID(keychainOrArray, NULL, subjKeyID);
    if (!certificate)
	return NULL;

    return CERT_FindIdentityByCertificate(keychainOrArray, certificate);
}

// find the smime symmetric capabilities profile for a given cert
SECItem *CERT_FindSMimeProfile(SecCertificateRef cert)
{
    return NULL;
}

// Return the decoded value of the subjectKeyID extension. The caller should 
// free up the storage allocated in retItem->data.
SECStatus CERT_FindSubjectKeyIDExtension (SecCertificateRef cert, SECItem *retItem)
{
    CSSM_DATA_PTR fieldValue = NULL;
    OSStatus ortn;
    CSSM_X509_EXTENSION *extp;
    CE_SubjectKeyID *skid;
    
    ortn = SecCertificateCopyFirstFieldValue(cert, &CSSMOID_SubjectKeyIdentifier,
	&fieldValue);
    if(ortn || (fieldValue == NULL)) {
	/* this cert doesn't have that extension */
	return SECFailure;
    }
    extp = (CSSM_X509_EXTENSION *)fieldValue->Data;
    skid = (CE_SubjectKeyID *)extp->value.parsedValue;
    retItem->Data = (uint8 *)PORT_Alloc(skid->Length);
    retItem->Length = skid->Length;
    memmove(retItem->Data, skid->Data, retItem->Length);
    SecCertificateReleaseFirstFieldValue(cert, &CSSMOID_SubjectKeyIdentifier,
	fieldValue);
    return SECSuccess;
}

// Extract the issuer and serial number from a certificate
SecCmsIssuerAndSN *CERT_GetCertIssuerAndSN(PRArenaPool *pl, SecCertificateRef cert)
{
    OSStatus status;
    SecCmsIssuerAndSN *certIssuerAndSN;
    CSSM_CL_HANDLE clHandle;
    CSSM_DATA_PTR serialNumber = 0;
    CSSM_DATA_PTR issuer = 0;
    CSSM_DATA certData = {};
    CSSM_HANDLE resultsHandle = 0;
    uint32 numberOfFields = 0;
    CSSM_RETURN result;
    void *mark;

    mark = PORT_ArenaMark(pl);

    status = SecCertificateGetCLHandle(cert, &clHandle);
    if (status)
	goto loser;
    status = SecCertificateGetData(cert, &certData);
    if (status)
	goto loser;

    /* Get the issuer from the cert. */
    result = CSSM_CL_CertGetFirstFieldValue(clHandle, &certData, 
	&OID_X509V1IssuerNameStd, &resultsHandle, &numberOfFields, &issuer);

    if (result || numberOfFields < 1)
	goto loser;
    result = CSSM_CL_CertAbortQuery(clHandle, resultsHandle);
    if (result)
	goto loser;


    /* Get the serialNumber from the cert. */
    result = CSSM_CL_CertGetFirstFieldValue(clHandle, &certData, 
	&CSSMOID_X509V1SerialNumber, &resultsHandle, &numberOfFields, &serialNumber);
    if (result || numberOfFields < 1)
	goto loser;
    result = CSSM_CL_CertAbortQuery(clHandle, resultsHandle);
    if (result)
	goto loser;

    /* Allocate the SecCmsIssuerAndSN struct. */
    certIssuerAndSN = (SecCmsIssuerAndSN *)PORT_ArenaZAlloc (pl, sizeof(SecCmsIssuerAndSN));
    if (certIssuerAndSN == NULL)
	goto loser;

    /* Copy the issuer. */
    certIssuerAndSN->derIssuer.Data = (uint8 *) PORT_ArenaAlloc(pl, issuer->Length);
    if (!certIssuerAndSN->derIssuer.Data)
	goto loser;
    PORT_Memcpy(certIssuerAndSN->derIssuer.Data, issuer->Data, issuer->Length);
    certIssuerAndSN->derIssuer.Length = issuer->Length;

    /* Copy the serialNumber. */
    certIssuerAndSN->serialNumber.Data = (uint8 *) PORT_ArenaAlloc(pl, serialNumber->Length);
    if (!certIssuerAndSN->serialNumber.Data)
	goto loser;
    PORT_Memcpy(certIssuerAndSN->serialNumber.Data, serialNumber->Data, serialNumber->Length);
    certIssuerAndSN->serialNumber.Length = serialNumber->Length;

    PORT_ArenaUnmark(pl, mark);

    CSSM_CL_FreeFieldValue(clHandle, &CSSMOID_X509V1SerialNumber, serialNumber);
    CSSM_CL_FreeFieldValue(clHandle, &OID_X509V1IssuerNameStd, issuer);

    return certIssuerAndSN;

loser:
    PORT_ArenaRelease(pl, mark);

    if (serialNumber)
	CSSM_CL_FreeFieldValue(clHandle, &CSSMOID_X509V1SerialNumber, serialNumber);
    if (issuer)
	CSSM_CL_FreeFieldValue(clHandle, &OID_X509V1IssuerNameStd, issuer);

    PORT_SetError(SEC_INTERNAL_ONLY);
    return NULL;
}

// import a collection of certs into the temporary or permanent cert database
SECStatus CERT_ImportCerts(SecKeychainRef keychain, SECCertUsage usage, unsigned int ncerts,
    SECItem **derCerts, SecCertificateRef **retCerts, Boolean keepCerts, Boolean caOnly, char *nickname)
{
    OSStatus rv = SECFailure;
    SecCertificateRef cert;
    unsigned int ci;

    // @@@ Do something with caOnly and nickname
    if (caOnly || nickname)
	abort();

    for (ci = 0; ci < ncerts; ++ci)
    {
	rv = SecCertificateCreateFromData(derCerts[ci], CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER, &cert);
	if (rv)
	    break;
	if (keepCerts)
	{
	    rv = SecCertificateAddToKeychain(cert, keychain);
	    if (rv)
	    {
		if (rv == errKCDuplicateItem)
		    rv = noErr;
		else
		{
		    CFRelease(cert);
		    break;
		}
	    }
	}

	if (retCerts)
	{
	    // @@@ not yet
	    abort();
	}
	else
	    CFRelease(cert);
    }

    return rv;
}

SECStatus CERT_SaveSMimeProfile(SecCertificateRef cert, SECItem *emailProfile,SECItem *profileTime)
{
    fprintf(stderr, "WARNING: CERT_SaveSMimeProfile unimplemented\n");
    return SECSuccess;
}

// Check the hostname to make sure that it matches the shexp that
// is given in the common name of the certificate.
SECStatus CERT_VerifyCertName(SecCertificateRef cert, const char *hostname)
{
    fprintf(stderr, "WARNING: CERT_VerifyCertName unimplemented\n");
    return SECSuccess;
}

/*
** OLD OBSOLETE FUNCTIONS with enum SECCertUsage - DO NOT USE FOR NEW CODE
** verify a certificate by checking validity times against a certain time,
** that we trust the issuer, and that the signature on the certificate is
** valid.
**	"cert" the certificate to verify
**	"checkSig" only check signatures if true
*/
SECStatus
CERT_VerifyCert(SecKeychainRef keychainOrArray, SecCertificateRef cert,
		const CSSM_DATA_PTR *otherCerts,    /* intermediates */
		CFTypeRef policies, CFAbsoluteTime stime, SecTrustRef *trustRef)
{
    CFMutableArrayRef certificates = NULL;
    SecTrustRef trust = NULL;
    OSStatus rv;
    int numOtherCerts = SecCmsArrayCount((void **)otherCerts);
    int dex;
    
    /* 
     * Certs to evaluate: first the leaf - our cert - then all the rest we know
     * about. It's OK for otherCerts to contain a copy of the leaf. 
     */
    certificates = CFArrayCreateMutable(NULL, numOtherCerts + 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(certificates, cert);
    for(dex=0; dex<numOtherCerts; dex++) {
	SecCertificateRef intCert;
	
	rv = SecCertificateCreateFromData(otherCerts[dex], 
	    CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER,
	    &intCert);
	if(rv) {
	    goto loser;
	}
	CFArrayAppendValue(certificates, intCert);
	CFRelease(intCert);
    }
    rv = SecTrustCreateWithCertificates(certificates, policies, &trust);
    CFRelease(certificates);
    certificates = NULL;
    if (rv)
	goto loser;

    rv = SecTrustSetKeychains(trust, keychainOrArray);
    if (rv)
	goto loser;

    CFDateRef verifyDate = CFDateCreate(NULL, stime);
    rv = SecTrustSetVerifyDate(trust, verifyDate);
    CFRelease(verifyDate);
    if (rv)
	goto loser;

    if (trustRef)
    {
	*trustRef = trust;
    }
    else
    {
	SecTrustResultType result;
	/* The caller doesn't want a SecTrust object, so let's evaluate it for them. */
	rv = SecTrustEvaluate(trust, &result);
	if (rv)
	    goto loser;

	switch (result)
	{
	case kSecTrustResultProceed:
	case kSecTrustResultUnspecified:
	    /* TP Verification succeeded and there was either a UserTurst entry
	       telling us to procceed, or no user trust setting was specified. */
	    CFRelease(trust);
	    break;
	default:
	    PORT_SetError(SEC_ERROR_UNTRUSTED_CERT);
	    rv = SECFailure;
	    goto loser;
	    break;
	}
    }

    return SECSuccess;
loser:
    if (trust)
	CFRelease(trust);
    if(certificates) 
	CFRelease(certificates);
    return rv;
}

CFTypeRef
CERT_PolicyForCertUsage(SECCertUsage certUsage)
{
    SecPolicySearchRef search = NULL;
    SecPolicyRef policy = NULL;
    const CSSM_OID *policyOID;
    OSStatus rv;

    switch (certUsage)
    {
    case certUsageSSLServerWithStepUp:
    case certUsageSSLCA:
    case certUsageVerifyCA:
    case certUsageAnyCA:
	goto loser;
	break;
    case certUsageSSLClient:
    case certUsageSSLServer:
	policyOID = &CSSMOID_APPLE_TP_SSL;
	break;
    case certUsageUserCertImport:
	policyOID = &CSSMOID_APPLE_TP_CSR_GEN;
	break;
    case certUsageStatusResponder:
	policyOID = &CSSMOID_APPLE_TP_REVOCATION_OCSP;
	break;
    case certUsageObjectSigner:
    case certUsageProtectedObjectSigner:
	policyOID = &CSSMOID_APPLE_ISIGN;
	break;
    case certUsageEmailSigner:
    case certUsageEmailRecipient:
	policyOID = &CSSMOID_APPLE_X509_BASIC;
	break;
    default:
	goto loser;
    }
    rv = SecPolicySearchCreate(CSSM_CERT_X_509v3, policyOID, NULL, &search);
    if (rv)
	goto loser;

    rv = SecPolicySearchCopyNext(search, &policy);
    if (rv)
	goto loser;

loser:
    if(search) CFRelease(search);
    return policy;
}
