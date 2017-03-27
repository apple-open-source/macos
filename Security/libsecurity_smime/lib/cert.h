/*
 *  cert.h
 *  security_smime
 *
 *  Created by john on Wed Mar 12 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _CERT_H_
#define _CERT_H_ 1

#include "SecCmsBase.h"
#include <Security/nameTemplates.h>
#include <Security/SecCertificate.h>
#include <CoreFoundation/CFDate.h>
#include <Security/SecTrust.h>
#include "cmstpriv.h"
#include <security_asn1/seccomon.h>

/************************************************************************/
SEC_BEGIN_PROTOS

bool CERT_CheckIssuerAndSerial(SecCertificateRef cert, SecAsn1Item *issuer, SecAsn1Item *serial);

typedef void CERTVerifyLog;

void CERT_NormalizeX509NameNSS(NSS_Name *nssName);

SecIdentityRef CERT_FindIdentityByUsage(SecKeychainRef keychainOrArray,
			 char *nickname, SECCertUsage usage, Boolean validOnly, void *proto_win);

SecCertificateRef CERT_FindUserCertByUsage(SecKeychainRef dbhandle,
			 char *nickname,SECCertUsage usage,Boolean validOnly,void *proto_win);

// Find a certificate in the database by a email address or nickname
// "name" is the email address or nickname to look up
SecCertificateRef CERT_FindCertByNicknameOrEmailAddr(SecKeychainRef dbhandle, char *name);

SecPublicKeyRef SECKEY_CopyPublicKey(SecPublicKeyRef pubKey);
void SECKEY_DestroyPublicKey(SecPublicKeyRef pubKey);
SecPublicKeyRef SECKEY_CopyPrivateKey(SecPublicKeyRef privKey);
void SECKEY_DestroyPrivateKey(SecPublicKeyRef privKey);
void CERT_DestroyCertificate(SecCertificateRef cert);
SecCertificateRef CERT_DupCertificate(SecCertificateRef cert);

// from security/nss/lib/certdb/cert.h

/*
    Substitutions:
    CERTCertificate * 		->	SecCertificateRef
    SECKEYPublicKey *		-> 	SecPublicKeyRef
    CERTCertDBHandle *		->	SecKeychainRef
    CERT_GetDefaultCertDB	->	OSStatus SecKeychainCopyDefault(SecKeychainRef *keychain);
    CERTCertificateList *	->	CFArrayRef
*/

// Generate a certificate chain from a certificate.

CF_RETURNS_RETAINED CFArrayRef CERT_CertChainFromCert(SecCertificateRef cert, SECCertUsage usage,Boolean includeRoot);

CFArrayRef CERT_CertListFromCert(SecCertificateRef cert);

CFArrayRef CERT_DupCertList(CFArrayRef oldList);

// Extract a public key object from a SubjectPublicKeyInfo
SecPublicKeyRef CERT_ExtractPublicKey(SecCertificateRef cert);

SECStatus CERT_CheckCertUsage (SecCertificateRef cert,unsigned char usage);

// Find a certificate in the database by a email address
// "emailAddr" is the email address to look up
SecCertificateRef CERT_FindCertByEmailAddr(SecKeychainRef keychainOrArray, char *emailAddr);

// Find a certificate in the database by a DER encoded certificate
// "derCert" is the DER encoded certificate
SecCertificateRef CERT_FindCertByDERCert(SecKeychainRef keychainOrArray, const SecAsn1Item *derCert);

// Generate a certificate key from the issuer and serialnumber, then look it up in the database.
// Return the cert if found. "issuerAndSN" is the issuer and serial number to look for
SecCertificateRef CERT_FindCertByIssuerAndSN (CFTypeRef keychainOrArray, const SecCmsIssuerAndSN *issuerAndSN);

SecCertificateRef CERT_FindCertBySubjectKeyID (CFTypeRef keychainOrArray, const SecAsn1Item *subjKeyID);

SecIdentityRef CERT_FindIdentityByIssuerAndSN (CFTypeRef keychainOrArray, const SecCmsIssuerAndSN *issuerAndSN);
SecCertificateRef CERT_FindCertificateByIssuerAndSN (CFTypeRef keychainOrArray, const SecCmsIssuerAndSN *issuerAndSN);

SecIdentityRef CERT_FindIdentityBySubjectKeyID (CFTypeRef keychainOrArray, const SecAsn1Item *subjKeyID);

// find the smime symmetric capabilities profile for a given cert
SecAsn1Item *CERT_FindSMimeProfile(SecCertificateRef cert);

// Return the decoded value of the subjectKeyID extension. The caller should 
// free up the storage allocated in retItem->data.
SECStatus CERT_FindSubjectKeyIDExtension (SecCertificateRef cert, SecAsn1Item *retItem);

// Extract the issuer and serial number from a certificate
SecCmsIssuerAndSN *CERT_GetCertIssuerAndSN(PRArenaPool *pl, SecCertificateRef cert);

// import a collection of certs into the temporary or permanent cert database
SECStatus CERT_ImportCerts(SecKeychainRef keychain, SECCertUsage usage,unsigned int ncerts,
    SecAsn1Item **derCerts,SecCertificateRef **retCerts, Boolean keepCerts,Boolean caOnly, char *nickname);

SECStatus CERT_SaveSMimeProfile(SecCertificateRef cert, SecAsn1Item *emailProfile,SecAsn1Item *profileTime);

// Check the hostname to make sure that it matches the shexp that
// is given in the common name of the certificate.
SECStatus CERT_VerifyCertName(SecCertificateRef cert, const char *hostname);

SECStatus CERT_VerifyCert(SecKeychainRef keychainOrArray, CFArrayRef cert,
			  CFTypeRef policies, CFAbsoluteTime stime, SecTrustRef *trustRef);

CFTypeRef CERT_PolicyForCertUsage(SECCertUsage certUsage);

/************************************************************************/
SEC_END_PROTOS

#endif /* _CERT_H_ */
