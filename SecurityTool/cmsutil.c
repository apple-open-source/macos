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
 * Modifications Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
 *
 * cmsutil -- A command to work with CMS data
 */

#include "security.h"
#include "keychain_utilities.h"

#include <Security/SecCmsBase.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsDigestContext.h>
#include <Security/SecCmsDigestedData.h>
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsEncryptedData.h>
#include <Security/SecCmsEnvelopedData.h>
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsRecipientInfo.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCmsSignerInfo.h>
#include <Security/SecSMIME.h>

#include <Security/oidsalg.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentitySearchPriv.h>
#include <CoreFoundation/CFString.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SEC_CHECK0(CALL, ERROR) do { if (!(CALL)) { sec_error(ERROR); goto loser; } } while(0)
#define SEC_CHECK(CALL, ERROR) do { rv = (CALL); if (rv) { sec_perror(ERROR, rv); goto loser; } } while(0)
#define SEC_CHECK2(CALL, ERROR, ARG) do { rv = CALL; if (rv) \
    { sec_error(ERROR ": %s", ARG, sec_errstr(rv)); goto loser; } } while(0)

// @@@ Remove this
#if 1

static CSSM_KEYUSE CERT_KeyUsageForCertUsage(SECCertUsage certUsage)
{
    switch (certUsage)
    {
    case certUsageSSLClient:             return CSSM_KEYUSE_SIGN;
    case certUsageSSLServer:             return CSSM_KEYUSE_SIGN;
    case certUsageSSLServerWithStepUp:   return CSSM_KEYUSE_SIGN;
    case certUsageSSLCA:                 return CSSM_KEYUSE_SIGN;
    case certUsageEmailSigner:           return CSSM_KEYUSE_SIGN;
    case certUsageEmailRecipient:        return CSSM_KEYUSE_UNWRAP;
    case certUsageObjectSigner:          return CSSM_KEYUSE_SIGN;
    case certUsageUserCertImport:        return CSSM_KEYUSE_SIGN;
    case certUsageVerifyCA:              return CSSM_KEYUSE_SIGN;
    case certUsageProtectedObjectSigner: return CSSM_KEYUSE_SIGN;
    case certUsageStatusResponder:       return CSSM_KEYUSE_SIGN;
    case certUsageAnyCA:                 return CSSM_KEYUSE_SIGN;
    default:
        sec_error("CERT_PolicyForCertUsage %ld: unknown certUsage", certUsage);
        return CSSM_KEYUSE_SIGN;
    }
}

static SecPolicyRef CERT_PolicyForCertUsage(SECCertUsage certUsage, const char *emailAddress)
{
    SecPolicyRef policy = NULL;
    const CSSM_OID *policyOID;
    OSStatus rv;

    switch (certUsage)
    {
    case certUsageSSLClient:             policyOID = &CSSMOID_APPLE_TP_SSL; break;
    case certUsageSSLServer:             policyOID = &CSSMOID_APPLE_TP_SSL; break;
    case certUsageSSLServerWithStepUp:   policyOID = &CSSMOID_APPLE_TP_SSL; break;
    case certUsageSSLCA:                 policyOID = &CSSMOID_APPLE_TP_SSL; break;
    case certUsageEmailSigner:           policyOID = &CSSMOID_APPLE_TP_SMIME; break;
    case certUsageEmailRecipient:        policyOID = &CSSMOID_APPLE_TP_SMIME; break;
    case certUsageObjectSigner:          policyOID = &CSSMOID_APPLE_TP_CODE_SIGN; break;
    case certUsageUserCertImport:        policyOID = &CSSMOID_APPLE_X509_BASIC; break;
    case certUsageVerifyCA:              policyOID = &CSSMOID_APPLE_X509_BASIC; break;
    case certUsageProtectedObjectSigner: policyOID = &CSSMOID_APPLE_ISIGN; break;
    case certUsageStatusResponder:       policyOID = &CSSMOID_APPLE_TP_REVOCATION_OCSP; break;
    case certUsageAnyCA:                 policyOID = &CSSMOID_APPLE_X509_BASIC; break;
    default:
        sec_error("CERT_PolicyForCertUsage %ld: unknown certUsage", certUsage);
        goto loser;
    }

    SEC_CHECK(SecPolicyCopy(CSSM_CERT_X_509v3, policyOID, &policy), "SecPolicyCopy");
    if (certUsage == certUsageEmailSigner || certUsage == certUsageEmailRecipient)
    {
        CSSM_APPLE_TP_SMIME_OPTIONS options =
        {
            CSSM_APPLE_TP_SMIME_OPTS_VERSION,
            certUsage == certUsageEmailSigner
                ? CE_KU_DigitalSignature | CE_KU_NonRepudiation
                : CE_KU_KeyEncipherment,
            emailAddress ? sizeof(emailAddress) : 0,
            emailAddress
        };
        CSSM_DATA value = { sizeof(options), (uint8 *)&options };
        SEC_CHECK(SecPolicySetValue(policy, &value), "SecPolicySetValue");
    }

    // @@@ Need to set values for SSL and other policies.
    return policy;

loser:
    if (policy) CFRelease(policy);
    return NULL;
}

static SecCertificateRef CERT_FindUserCertByUsage(CFTypeRef keychainOrArray, const char *emailAddress,
                                                  SECCertUsage certUsage, Boolean validOnly)
{
    SecKeychainSearchRef search = NULL;
    SecCertificateRef cert = NULL;
    SecPolicyRef policy;
    OSStatus rv;
    
    policy = CERT_PolicyForCertUsage(certUsage, emailAddress);
    if (!policy)
        goto loser;
    
    SEC_CHECK2(SecKeychainSearchCreateForCertificateByEmail(keychainOrArray, emailAddress, &search),
              "create search for certificate with email: \"%s\"", emailAddress);
    for (;;)
    {
        SecKeychainItemRef item;
        rv = SecKeychainSearchCopyNext(search, &item);
        if (rv)
        {
            if (rv == errSecItemNotFound)
                break;

            sec_perror("error finding next matching certificate", rv);
            goto loser;
        }

        cert = (SecCertificateRef)item;
        // @@@ Check cert against policy.
    }

loser:
    if (policy) CFRelease(policy);
    if (search) CFRelease(search);

    return cert;
}

static SecIdentityRef CERT_FindIdentityByUsage(CFTypeRef keychainOrArray, 
                                               const char *emailAddress,
                                               SECCertUsage certUsage,
                                               Boolean validOnly)
{
    SecIdentitySearchRef search = NULL;
    SecIdentityRef identity = NULL;
	CFStringRef idString = CFStringCreateWithCString(NULL, emailAddress, kCFStringEncodingUTF8);
    SecPolicyRef policy;
    OSStatus rv;

    policy = CERT_PolicyForCertUsage(certUsage, emailAddress);
    if (!policy)
        goto loser;


    SEC_CHECK2(SecIdentitySearchCreateWithPolicy(policy, idString,
		CERT_KeyUsageForCertUsage(certUsage), keychainOrArray, validOnly, &search),
		"create search for identity with email: \"%s\"", emailAddress);
    for (;;)
    {
        rv = SecIdentitySearchCopyNext(search, &identity);
        if (rv)
        {
            if (rv == errSecItemNotFound)
                break;

            sec_perror("error finding next matching identity", rv);
            goto loser;
        }
    }

loser:
    if (policy) CFRelease(policy);
    if (search) CFRelease(search);
    if (idString) CFRelease(idString);

    return identity;


#if 0
    SecIdentityRef identity = NULL;
    SecCertificateRef cert;
    OSStatus rv;

    cert = CERT_FindUserCertByUsage(keychainOrArray, emailAddress, certUsage, validOnly);
    if (!cert)
        goto loser;
    
    SEC_CHECK2(SecIdentityCreateWithCertificate(keychainOrArray, cert, &identity),
               "failed to find private key for certificate with email: \"%s\"", emailAddress);
loser:
    if (cert) CFRelease(cert);

    return identity;
#endif
}

static SecCertificateRef CERT_FindCertByNicknameOrEmailAddr(CFTypeRef keychainOrArray, const char *emailAddress)
{
    SecCertificateRef certificate = NULL;
    OSStatus rv;

    SEC_CHECK2(SecCertificateFindByEmail(keychainOrArray, emailAddress, &certificate),
              "failed to find certificate with email: \"%s\"", emailAddress);

loser:
    return certificate;
}

static OSStatus CERT_CheckCertUsage (SecCertificateRef cert,unsigned char usage)
{
    return 0;
}

#endif

// @@@ Eleminate usage of this header.
//#include "cert.h"
//#include <security_asn1/secerr.h>
//#include "plgetopt.h"
//#include "secitem.h"

#ifdef HAVE_DODUMPSTATES
extern int doDumpStates;
#endif /* HAVE_DODUMPSTATES */

OSStatus SECU_FileToItem(CSSM_DATA *dst, FILE *src);


extern void SEC_Init(void);		/* XXX */
static int cms_verbose = 0;
static int cms_update_single_byte = 0;

/* XXX stolen from cmsarray.c
 * nss_CMSArray_Count - count number of elements in array
 */
int nss_CMSArray_Count(void **array)
{
    int n = 0;
    if (array == NULL)
        return 0;
    while (*array++ != NULL)
        n++;
    return n;
}

typedef OSStatus(update_func)(void *cx, const char *data, unsigned int len);

static OSStatus do_update(update_func *update,
			   void *cx, const unsigned char *data, int len)
{
    OSStatus rv = noErr;
    if (cms_update_single_byte)
    {
        for (;len; --len, ++data)
        {
            rv = update(cx, (const char *)data, 1);
            if (rv)
                break;
        }
    }
    else
	rv = update(cx, (const char *)data, len);

    return rv;
}


static OSStatus DigestFile(SecArenaPoolRef poolp, CSSM_DATA ***digests, CSSM_DATA *input, SECAlgorithmID **algids)
{
    SecCmsDigestContextRef digcx = SecCmsDigestContextStartMultiple(algids);
    if (digcx == NULL)
        return paramErr;

    do_update((update_func *)SecCmsDigestContextUpdate, digcx, input->Data, input->Length);

    return SecCmsDigestContextFinishMultiple(digcx, poolp, digests);
}

char *
ownpw(void *info, Boolean retry, void *arg)
{
	char * passwd = NULL;

	if ( (!retry) && arg ) {
		passwd = strdup((char *)arg);
	}

	return passwd;
}

struct optionsStr {
    PK11PasswordFunc password;
    SECCertUsage certUsage;
    SecKeychainRef certDBHandle;
};

struct decodeOptionsStr {
    struct optionsStr *options;
    FILE *contentFile;
    int headerLevel;
    Boolean suppressContent;
    SecCmsGetDecryptKeyCallback dkcb;
    SecSymmetricKeyRef bulkkey;
};

struct signOptionsStr {
    struct optionsStr *options;
    char *nickname;
    char *encryptionKeyPreferenceNick;
    Boolean signingTime;
    Boolean smimeProfile;
    Boolean detached;
    SECOidTag hashAlgTag;
};

struct envelopeOptionsStr {
    struct optionsStr *options;
    char **recipients;
};

struct certsonlyOptionsStr {
    struct optionsStr *options;
    char **recipients;
};

struct encryptOptionsStr {
    struct optionsStr *options;
    char **recipients;
    SecCmsMessageRef envmsg;
    CSSM_DATA *input;
    FILE *outfile;
    FILE *envFile;
    SecSymmetricKeyRef bulkkey;
    SECOidTag bulkalgtag;
    int keysize;
};

static SecCmsMessageRef decode(FILE *out, CSSM_DATA *output, CSSM_DATA *input,
                             const struct decodeOptionsStr *decodeOptions)
{
    SecCmsDecoderRef dcx;
    SecCmsMessageRef cmsg;
    SecCmsContentInfoRef cinfo;
    SecCmsSignedDataRef sigd = NULL;
    SecCmsEnvelopedDataRef envd;
    SecCmsEncryptedDataRef encd;
    SECAlgorithmID **digestalgs;
    int nlevels, i, nsigners, j;
    CFStringRef signercn;
    SecCmsSignerInfoRef si;
    SECOidTag typetag;
    CSSM_DATA **digests;
    SecArenaPoolRef poolp = NULL;
    PK11PasswordFunc pwcb;
    void *pwcb_arg;
    CSSM_DATA *item, sitem = { 0, };
    CFTypeRef policy = NULL;
    OSStatus rv;
    
    pwcb     = (PK11PasswordFunc)((decodeOptions->options->password != NULL) ? ownpw : NULL);
    pwcb_arg = (decodeOptions->options->password != NULL) ? 
        (void *)decodeOptions->options->password : NULL;
    
    if (decodeOptions->contentFile) // detached content: grab content file
        SECU_FileToItem(&sitem, decodeOptions->contentFile);
    
    SEC_CHECK(SecCmsDecoderCreate(NULL, 
                                  NULL, NULL,         /* content callback     */
                                  pwcb, pwcb_arg,     /* password callback    */
                                  decodeOptions->dkcb, /* decrypt key callback */
                                  decodeOptions->bulkkey,
                                  &dcx),
              "failed to create to decoder");
    SEC_CHECK(do_update((update_func *)SecCmsDecoderUpdate, dcx, input->Data, input->Length),
              "failed to add data to decoder");
    SEC_CHECK(SecCmsDecoderFinish(dcx, &cmsg),
              "failed to decode message");

    if (decodeOptions->headerLevel >= 0)
    {
        /*fprintf(out, "SMIME: ", decodeOptions->headerLevel, i);*/
        fprintf(out, "SMIME: ");
    }

    nlevels = SecCmsMessageContentLevelCount(cmsg);
    for (i = 0; i < nlevels; i++)
    {
        cinfo = SecCmsMessageContentLevel(cmsg, i);
        typetag = SecCmsContentInfoGetContentTypeTag(cinfo);

        if (decodeOptions->headerLevel >= 0)
            fprintf(out, "\tlevel=%d.%d; ", decodeOptions->headerLevel, nlevels - i);

        switch (typetag)
        {
        case SEC_OID_PKCS7_SIGNED_DATA:
            if (decodeOptions->headerLevel >= 0)
                fprintf(out, "type=signedData; ");

            SEC_CHECK0(sigd = (SecCmsSignedDataRef )SecCmsContentInfoGetContent(cinfo),
                       "problem finding signedData component");
            /* if we have a content file, but no digests for this signedData */
            if (decodeOptions->contentFile != NULL && !SecCmsSignedDataHasDigests(sigd))
            {
                SEC_CHECK(SecArenaPoolCreate(1024, &poolp), "failed to create arenapool");
                digestalgs = SecCmsSignedDataGetDigestAlgs(sigd);
                SEC_CHECK(DigestFile(poolp, &digests, &sitem, digestalgs),
                          "problem computing message digest");
                SEC_CHECK(SecCmsSignedDataSetDigests(sigd, digestalgs, digests),
                          "problem setting message digests");
                SecArenaPoolFree(poolp, false);
            }

            policy = CERT_PolicyForCertUsage(decodeOptions->options->certUsage, NULL);
            // import the certificates
            SEC_CHECK(SecCmsSignedDataImportCerts(sigd,decodeOptions->options->certDBHandle,
                                                  decodeOptions->options->certUsage, true /* false */),
                      "cert import failed");

            /* find out about signers */
            nsigners = SecCmsSignedDataSignerInfoCount(sigd);
            if (decodeOptions->headerLevel >= 0)
                fprintf(out, "nsigners=%d; ", nsigners);
            if (nsigners == 0)
            {
                /* must be a cert transport message */
                OSStatus rv;
                /* XXX workaround for bug #54014 */
                SecCmsSignedDataImportCerts(sigd,decodeOptions->options->certDBHandle,
                                            decodeOptions->options->certUsage,true);
                SEC_CHECK(SecCmsSignedDataVerifyCertsOnly(sigd,decodeOptions->options->certDBHandle, policy),
                          "verify certs-only failed");
                return cmsg;
            }

            SEC_CHECK0(SecCmsSignedDataHasDigests(sigd), "message has no digests");
            for (j = 0; j < nsigners; j++)
            {
                si = SecCmsSignedDataGetSignerInfo(sigd, j);
                signercn = SecCmsSignerInfoGetSignerCommonName(si);
                if (decodeOptions->headerLevel >= 0)
                {
                    const char *px = signercn ? CFStringGetCStringPtr(signercn,kCFStringEncodingMacRoman) : "<NULL>";
                    fprintf(out, "\n\t\tsigner%d.id=\"%s\"; ", j, px);
                }
                SecCmsSignedDataVerifySignerInfo(sigd, j, decodeOptions->options->certDBHandle, 
                                                 policy, NULL);
                if (decodeOptions->headerLevel >= 0)
                    fprintf(out, "signer%d.status=%s; ", j, 
                            SecCmsUtilVerificationStatusToString(SecCmsSignerInfoGetVerificationStatus(si)));
                /* XXX what do we do if we don't print headers? */
            }
            break;
        case SEC_OID_PKCS7_ENVELOPED_DATA:
            if (decodeOptions->headerLevel >= 0)
                fprintf(out, "type=envelopedData; ");
            envd = (SecCmsEnvelopedDataRef )SecCmsContentInfoGetContent(cinfo);
            break;
        case SEC_OID_PKCS7_ENCRYPTED_DATA:
            if (decodeOptions->headerLevel >= 0)
                fprintf(out, "type=encryptedData; ");
            encd = (SecCmsEncryptedDataRef )SecCmsContentInfoGetContent(cinfo);
            break;
        case SEC_OID_PKCS7_DATA:
            if (decodeOptions->headerLevel >= 0)
                fprintf(out, "type=data; ");
            break;
        default:
            break;
        }
        if (decodeOptions->headerLevel >= 0)
            fprintf(out, "\n");
    }

    if (!decodeOptions->suppressContent)
    {
        item = decodeOptions->contentFile ? &sitem :
	    SecCmsMessageGetContent(cmsg);
        /* Copy the data. */
        output->Length = item->Length;
        output->Data = malloc(output->Length);
        memcpy(output->Data, item->Data, output->Length);
    }

    if (policy) CFRelease(policy);

    return cmsg;
loser:
    if (policy) CFRelease(policy);
    if (cmsg) SecCmsMessageDestroy(cmsg);
    return NULL;
}

/* example of a callback function to use with encoder */
/*
static void
writeout(void *arg, const char *buf, unsigned long len)
{
    FILE *f = (FILE *)arg;

    if (f != NULL && buf != NULL)
	(void)fwrite(buf, len, 1, f);
}
*/

static SecCmsMessageRef signed_data(struct signOptionsStr *signOptions)
{
    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    SecCmsSignedDataRef sigd;
    SecCmsSignerInfoRef signerinfo;
    SecIdentityRef identity = NULL;
    SecCertificateRef cert = NULL, ekpcert = NULL;
    OSStatus rv;

    if (cms_verbose)
    {
        fprintf(stderr, "Input to signed_data:\n");
        if (signOptions->options->password)
            fprintf(stderr, "password [%s]\n", "foo" /*signOptions->options->password*/);
        else
            fprintf(stderr, "password [NULL]\n");
        fprintf(stderr, "certUsage [%d]\n", signOptions->options->certUsage);
        if (signOptions->options->certDBHandle)
            fprintf(stderr, "certdb [%p]\n", signOptions->options->certDBHandle);
        else
            fprintf(stderr, "certdb [NULL]\n");
        if (signOptions->nickname)
            fprintf(stderr, "nickname [%s]\n", signOptions->nickname);
        else
            fprintf(stderr, "nickname [NULL]\n");
    }

    SEC_CHECK0(signOptions->nickname,
               "please indicate the email address of a certificate to sign with");
    if ((identity = CERT_FindIdentityByUsage(signOptions->options->certDBHandle, 
                                         signOptions->nickname,
                                         signOptions->options->certUsage,
                                         false)) == NULL)
    {
        sec_error("could not find signing identity for email: \"%s\"", signOptions->nickname);
        return NULL;
    }

    if (cms_verbose)
        fprintf(stderr, "Found identity for %s\n", signOptions->nickname);

    // Get the cert from the identity
    SEC_CHECK(SecIdentityCopyCertificate(identity, &cert),
              "SecIdentityCopyCertificate");
    // create the message object on its own pool
    SEC_CHECK0(cmsg = SecCmsMessageCreate(NULL), "cannot create CMS message");
    // build chain of objects: message->signedData->data
    SEC_CHECK0(sigd = SecCmsSignedDataCreate(cmsg),
               "cannot create CMS signedData object");
    SEC_CHECK0(cinfo = SecCmsMessageGetContentInfo(cmsg),
               "message has no content info");
    SEC_CHECK(SecCmsContentInfoSetContentSignedData(cmsg, cinfo, sigd),
              "cannot attach CMS signedData object");
    SEC_CHECK0(cinfo = SecCmsSignedDataGetContentInfo(sigd),
               "signed data has no content info");
    /* we're always passing data in and detaching optionally */
    SEC_CHECK(SecCmsContentInfoSetContentData(cmsg, cinfo, NULL, signOptions->detached),
              "cannot attach CMS data object");
    // create & attach signer information
    SEC_CHECK0(signerinfo = SecCmsSignerInfoCreate(cmsg, identity, signOptions->hashAlgTag),
               "cannot create CMS signerInfo object");
    if (cms_verbose)
        fprintf(stderr,"Created CMS message, added signed data w/ signerinfo\n");

    // we want the cert chain included for this one
    SEC_CHECK(SecCmsSignerInfoIncludeCerts(signerinfo, SecCmsCMCertChain, signOptions->options->certUsage),
             "cannot add cert chain");

    if (cms_verbose)
        fprintf(stderr, "imported certificate\n");

    if (signOptions->signingTime)
        SEC_CHECK(SecCmsSignerInfoAddSigningTime(signerinfo, CFAbsoluteTimeGetCurrent()),
                  "cannot add signingTime attribute");

    if (signOptions->smimeProfile)
        SEC_CHECK(SecCmsSignerInfoAddSMIMECaps(signerinfo),
                  "cannot add SMIMECaps attribute");

    if (!signOptions->encryptionKeyPreferenceNick)
    {
        /* check signing cert for fitness as encryption cert */
        OSStatus FitForEncrypt = CERT_CheckCertUsage(cert, certUsageEmailRecipient);

        if (noErr == FitForEncrypt)
        {
            /* if yes, add signing cert as EncryptionKeyPreference */
            SEC_CHECK(SecCmsSignerInfoAddSMIMEEncKeyPrefs(signerinfo, cert, signOptions->options->certDBHandle),
                      "cannot add default SMIMEEncKeyPrefs attribute");
            SEC_CHECK(SecCmsSignerInfoAddMSSMIMEEncKeyPrefs(signerinfo, cert, signOptions->options->certDBHandle),
                      "cannot add default MS SMIMEEncKeyPrefs attribute");
        }
        else
        {
            /* this is a dual-key cert case, we need to look for the encryption
               certificate under the same nickname as the signing cert */
            /* get the cert, add it to the message */
            if ((ekpcert = CERT_FindUserCertByUsage(
                signOptions->options->certDBHandle,
                signOptions->nickname,
                certUsageEmailRecipient,
                false)) == NULL)
            {
                sec_error("can find encryption cert for \"%s\"", signOptions->nickname);
                goto loser;
            }

            SEC_CHECK(SecCmsSignerInfoAddSMIMEEncKeyPrefs(signerinfo, ekpcert,signOptions->options->certDBHandle),
                      "cannot add SMIMEEncKeyPrefs attribute");
            SEC_CHECK(SecCmsSignerInfoAddMSSMIMEEncKeyPrefs(signerinfo, ekpcert,signOptions->options->certDBHandle),
                      "cannot add MS SMIMEEncKeyPrefs attribute");
            SEC_CHECK(SecCmsSignedDataAddCertificate(sigd, ekpcert),
                      "cannot add encryption certificate");
        }
    }
    else if (strcmp(signOptions->encryptionKeyPreferenceNick, "NONE") == 0)
    {
        /* No action */
    }
    else
    {
        /* specific email address for encryption preferred encryption cert specified.
           get the cert, add it to the message */
        if ((ekpcert = CERT_FindUserCertByUsage(
            signOptions->options->certDBHandle, 
            signOptions->encryptionKeyPreferenceNick,
            certUsageEmailRecipient, false)) == NULL)
        {
            sec_error("can find encryption cert for \"%s\"", signOptions->encryptionKeyPreferenceNick);
            goto loser;
        }

        SEC_CHECK(SecCmsSignerInfoAddSMIMEEncKeyPrefs(signerinfo, ekpcert,signOptions->options->certDBHandle),
                  "cannot add SMIMEEncKeyPrefs attribute");
        SEC_CHECK(SecCmsSignerInfoAddMSSMIMEEncKeyPrefs(signerinfo, ekpcert,signOptions->options->certDBHandle),
                  "cannot add MS SMIMEEncKeyPrefs attribute");
        SEC_CHECK(SecCmsSignedDataAddCertificate(sigd, ekpcert),
                  "cannot add encryption certificate");
    }

    SEC_CHECK(SecCmsSignedDataAddSignerInfo(sigd, signerinfo),
              "cannot add CMS signerInfo object");

    if (cms_verbose)
        fprintf(stderr, "created signed-data message\n");

    if (ekpcert) CFRelease(ekpcert);
    if (cert) CFRelease(cert);
    if (identity) CFRelease(identity);
    return cmsg;

loser:
    if (ekpcert) CFRelease(ekpcert);
    if (cert) CFRelease(cert);
    if (identity) CFRelease(identity);
    SecCmsMessageDestroy(cmsg);
    return NULL;
}

static SecCmsMessageRef enveloped_data(struct envelopeOptionsStr *envelopeOptions)
{
    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    SecCmsEnvelopedDataRef envd;
    SecCmsRecipientInfoRef recipientinfo;
    SecCertificateRef *recipientcerts = NULL;
    SecKeychainRef dbhandle;
    SECOidTag bulkalgtag;
    OSStatus rv;
    int keysize, i = 0;
    int cnt;

    dbhandle = envelopeOptions->options->certDBHandle;
    /* count the recipients */
    SEC_CHECK0(cnt = nss_CMSArray_Count((void **)envelopeOptions->recipients),
               "please name at least one recipient");

    // @@@ find the recipient's certs by email address or nickname
    if ((recipientcerts = (SecCertificateRef *)calloc((cnt+1), sizeof(SecCertificateRef))) == NULL)
    {
        sec_error("failed to alloc certs array: %s", strerror(errno));
        goto loser;
    }

    for (i = 0; envelopeOptions->recipients[i] != NULL; ++i)
    {
        if ((recipientcerts[i] =
             CERT_FindCertByNicknameOrEmailAddr(dbhandle, envelopeOptions->recipients[i])) == NULL)
        {
            i = 0;
            goto loser;
        }
    }

    recipientcerts[i] = NULL;
    i = 0;

    // find a nice bulk algorithm
    SEC_CHECK(SecSMIMEFindBulkAlgForRecipients(recipientcerts, &bulkalgtag, &keysize),
              "cannot find common bulk algorithm");
    // create the message object on its own pool
    SEC_CHECK0(cmsg = SecCmsMessageCreate(NULL), "cannot create CMS message");
    // build chain of objects: message->envelopedData->data
    SEC_CHECK0(envd = SecCmsEnvelopedDataCreate(cmsg, bulkalgtag, keysize),
               "cannot create CMS envelopedData object");
    SEC_CHECK0(cinfo = SecCmsMessageGetContentInfo(cmsg),
               "message has no content info");
    SEC_CHECK(SecCmsContentInfoSetContentEnvelopedData(cmsg, cinfo, envd),
              "cannot attach CMS envelopedData object");
    SEC_CHECK0(cinfo = SecCmsEnvelopedDataGetContentInfo(envd),
               "enveloped data has no content info");
    // We're always passing data in, so the content is NULL
    SEC_CHECK(SecCmsContentInfoSetContentData(cmsg, cinfo, NULL, false),
              "cannot attach CMS data object");

    // create & attach recipient information
    for (i = 0; recipientcerts[i] != NULL; i++)
    {
        SEC_CHECK0(recipientinfo = SecCmsRecipientInfoCreate(cmsg, recipientcerts[i]),
                   "cannot create CMS recipientInfo object");
        SEC_CHECK(SecCmsEnvelopedDataAddRecipient(envd, recipientinfo),
                  "cannot add CMS recipientInfo object");
        CFRelease(recipientcerts[i]);
    }

    if (recipientcerts)
        free(recipientcerts);

    return cmsg;
loser:
    if (recipientcerts)
    {
        for (; recipientcerts[i] != NULL; i++)
            CFRelease(recipientcerts[i]);
    }

    if (cmsg)
        SecCmsMessageDestroy(cmsg);

    if (recipientcerts)
        free(recipientcerts);

    return NULL;
}

SecSymmetricKeyRef dkcb(void *arg, SECAlgorithmID *algid)
{
    return (SecSymmetricKeyRef)arg;
}

static OSStatus get_enc_params(struct encryptOptionsStr *encryptOptions)
{
    struct envelopeOptionsStr envelopeOptions;
    OSStatus rv = paramErr;
    SecCmsMessageRef env_cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    int i, nlevels;

    // construct an enveloped data message to obtain bulk keys
    if (encryptOptions->envmsg)
        env_cmsg = encryptOptions->envmsg; // get it from an old message
	else
    {
        CSSM_DATA dummyOut = { 0, };
        CSSM_DATA dummyIn  = { 0, };
        char str[] = "Hello!";
        SecArenaPoolRef tmparena = NULL;

        SEC_CHECK(SecArenaPoolCreate(1024, &tmparena), "failed to create arenapool");

        dummyIn.Data = (unsigned char *)str;
        dummyIn.Length = strlen(str);
        envelopeOptions.options = encryptOptions->options;
        envelopeOptions.recipients = encryptOptions->recipients;
        env_cmsg = enveloped_data(&envelopeOptions);
        SecCmsMessageEncode(env_cmsg, &dummyIn, tmparena, &dummyOut);
        fwrite(dummyOut.Data, 1, dummyOut.Length,encryptOptions->envFile);

        SecArenaPoolFree(tmparena, false);
    }

    // get the content info for the enveloped data 
    nlevels = SecCmsMessageContentLevelCount(env_cmsg);
    for (i = 0; i < nlevels; i++)
    {
    	SECOidTag typetag;
        cinfo = SecCmsMessageContentLevel(env_cmsg, i);
        typetag = SecCmsContentInfoGetContentTypeTag(cinfo);
        if (typetag == SEC_OID_PKCS7_DATA)
        {
            // get the symmetric key
            encryptOptions->bulkalgtag = SecCmsContentInfoGetContentEncAlgTag(cinfo);
            encryptOptions->keysize = SecCmsContentInfoGetBulkKeySize(cinfo);
            encryptOptions->bulkkey = SecCmsContentInfoGetBulkKey(cinfo);
            rv = noErr;
            break;
        }
    }
    if (i == nlevels)
        sec_error("could not retrieve enveloped data: messsage has: %ld levels", nlevels);

loser:
    if (env_cmsg)
        SecCmsMessageDestroy(env_cmsg);

    return rv;
}

static SecCmsMessageRef encrypted_data(struct encryptOptionsStr *encryptOptions)
{
    OSStatus rv = paramErr;
    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    SecCmsEncryptedDataRef encd;
    SecCmsEncoderRef ecx = NULL;
    SecArenaPoolRef tmppoolp = NULL;
    CSSM_DATA derOut = { 0, };

    /* arena for output */
    SEC_CHECK(SecArenaPoolCreate(1024, &tmppoolp), "failed to create arenapool");
    // create the message object on its own pool
    SEC_CHECK0(cmsg = SecCmsMessageCreate(NULL), "cannot create CMS message");
    // build chain of objects: message->encryptedData->data
    SEC_CHECK0(encd = SecCmsEncryptedDataCreate(cmsg, encryptOptions->bulkalgtag, 
                                                encryptOptions->keysize),
               "cannot create CMS encryptedData object");
    SEC_CHECK0(cinfo = SecCmsMessageGetContentInfo(cmsg),
               "message has no content info");
    SEC_CHECK(SecCmsContentInfoSetContentEncryptedData(cmsg, cinfo, encd),
              "cannot attach CMS encryptedData object");
    SEC_CHECK0(cinfo = SecCmsEncryptedDataGetContentInfo(encd),
               "encrypted data has no content info");
    /* we're always passing data in, so the content is NULL */
    SEC_CHECK(SecCmsContentInfoSetContentData(cmsg, cinfo, NULL, false),
              "cannot attach CMS data object");
    SEC_CHECK(SecCmsEncoderCreate(cmsg, NULL, NULL, &derOut, tmppoolp, NULL, NULL,
                                  dkcb, encryptOptions->bulkkey, NULL, NULL, &ecx),
              "cannot create encoder context");
    SEC_CHECK(do_update((update_func *)SecCmsEncoderUpdate, ecx, encryptOptions->input->Data,
                        encryptOptions->input->Length),
              "failed to add data to encoder");
    SEC_CHECK(SecCmsEncoderFinish(ecx), "failed to encrypt data");
    fwrite(derOut.Data, derOut.Length, 1, encryptOptions->outfile);
    /* @@@ Check and report write errors. */
    /*
     if (bulkkey)
     CFRelease(bulkkey);
     */

    if (tmppoolp)
        SecArenaPoolFree(tmppoolp, false);
    return cmsg;
loser:
    /*
    if (bulkkey)
	CFRelease(bulkkey);
	*/
    if (tmppoolp)
        SecArenaPoolFree(tmppoolp, false);
    if (cmsg)
        SecCmsMessageDestroy(cmsg);

    return NULL;
}

static SecCmsMessageRef signed_data_certsonly(struct certsonlyOptionsStr *certsonlyOptions)
{
    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    SecCmsSignedDataRef sigd;
    SecCertificateRef *certs = NULL;
    SecKeychainRef dbhandle;
    OSStatus rv;

    int i = 0, cnt;
    dbhandle = certsonlyOptions->options->certDBHandle;
    SEC_CHECK0(cnt = nss_CMSArray_Count((void**)certsonlyOptions->recipients),
               "please indicate the nickname of a certificate to sign with");
    if ((certs = (SecCertificateRef *)calloc((cnt+1), sizeof(SecCertificateRef))) == NULL)
    {
        sec_error("failed to alloc certs array: %s", strerror(errno));
        goto loser;
    }
    for (i=0; certsonlyOptions->recipients && certsonlyOptions->recipients[i] != NULL; i++)
    {
        if ((certs[i] = CERT_FindCertByNicknameOrEmailAddr(dbhandle,certsonlyOptions->recipients[i])) == NULL)
        {
            i=0;
            goto loser;
        }
    }
    certs[i] = NULL;
    i = 0;
    // create the message object on its own pool
    SEC_CHECK0(cmsg = SecCmsMessageCreate(NULL), "cannot create CMS message");
    // build chain of objects: message->signedData->data
    SEC_CHECK0(sigd = SecCmsSignedDataCreateCertsOnly(cmsg, certs[0], true),
               "cannot create certs only CMS signedData object");
    CFRelease(certs[0]);
    for (i = 1; i < cnt; ++i)
    {
        SEC_CHECK2(SecCmsSignedDataAddCertChain(sigd, certs[i]),
                   "cannot add cert chain for \"%s\"", certsonlyOptions->recipients[i]);
        CFRelease(certs[i]);
    }

    SEC_CHECK0(cinfo = SecCmsMessageGetContentInfo(cmsg),
               "message has no content info");
    SEC_CHECK(SecCmsContentInfoSetContentSignedData(cmsg, cinfo, sigd),
              "cannot attach CMS signedData object");
    SEC_CHECK0(cinfo = SecCmsSignedDataGetContentInfo(sigd),
               "signed data has no content info");
    SEC_CHECK(SecCmsContentInfoSetContentData(cmsg, cinfo, NULL, false),
              "cannot attach CMS data object");

    if (certs)
        free(certs);

    return cmsg;
loser:
    if (certs)
    {
        for (; i < cnt; ++i)
            CFRelease(certs[i]);

        free(certs);
    }
    if (cmsg) SecCmsMessageDestroy(cmsg);

    return NULL;
}

typedef enum { UNKNOWN, DECODE, SIGN, ENCRYPT, ENVELOPE, CERTSONLY } Mode;

int cms_util(int argc, char **argv)
{
    FILE *outFile;
    SecCmsMessageRef cmsg = NULL;
    FILE *inFile;
    int ch;
    OSStatus statusX;
    Mode mode = UNKNOWN;
    PK11PasswordFunc pwcb;
    void *pwcb_arg;
    struct decodeOptionsStr decodeOptions = { 0 };
    struct signOptionsStr signOptions = { 0 };
    struct envelopeOptionsStr envelopeOptions = { 0 };
    struct certsonlyOptionsStr certsonlyOptions = { 0 };
    struct encryptOptionsStr encryptOptions = { 0 };
    struct optionsStr options = { 0 };
    int result = 1;
    static char *ptrarray[128] = { 0 };
    int nrecipients = 0;
    char *str, *tok;
    char *envFileName;
    const char *keychainName = NULL;
    CSSM_DATA input = { 0,};
    CSSM_DATA output = { 0,};
    CSSM_DATA dummy = { 0, };
    CSSM_DATA envmsg = { 0, };
    OSStatus rv;
    
    inFile = stdin;
    outFile = stdout;
    envFileName = NULL;
    mode = UNKNOWN;
    decodeOptions.contentFile = NULL;
    decodeOptions.suppressContent = false;
    decodeOptions.headerLevel = -1;
    options.certUsage = certUsageEmailSigner;
    options.password = NULL;
    signOptions.nickname = NULL;
    signOptions.detached = false;
    signOptions.signingTime = false;
    signOptions.smimeProfile = false;
    signOptions.encryptionKeyPreferenceNick = NULL;
    signOptions.hashAlgTag = SEC_OID_SHA1;
    envelopeOptions.recipients = NULL;
    encryptOptions.recipients = NULL;
    encryptOptions.envmsg = NULL;
    encryptOptions.envFile = NULL;
    encryptOptions.bulkalgtag = SEC_OID_UNKNOWN;
    encryptOptions.bulkkey = NULL;
    encryptOptions.keysize = -1;
    
    // Parse command line arguments
    while ((ch = getopt(argc, argv, "CDEGH:N:OPSTY:c:de:h:i:k:no:p:r:su:v")) != -1)
    {
        switch (ch)
        {
        case 'C':
            mode = ENCRYPT;
            break;
        case 'D':
            mode = DECODE;
            break;
        case 'E':
            mode = ENVELOPE;
            break;
        case 'G':
            if (mode != SIGN) {
                sec_error("option -G only supported with option -S");
                result = 2; /* Trigger usage message. */
                goto loser;
            }
            signOptions.signingTime = true;
            break;
        case 'H':
            if (mode != SIGN) {
                sec_error("option -n only supported with option -D");
                result = 2; /* Trigger usage message. */
                goto loser;
            }
            decodeOptions.suppressContent = true;
            if (!strcmp(optarg, "MD2"))
                signOptions.hashAlgTag = SEC_OID_MD2;
            else if (!strcmp(optarg, "MD4"))
                signOptions.hashAlgTag = SEC_OID_MD4;
            else if (!strcmp(optarg, "MD5"))
                signOptions.hashAlgTag = SEC_OID_MD5;
            else if (!strcmp(optarg, "SHA1"))
                signOptions.hashAlgTag = SEC_OID_SHA1;
            else if (!strcmp(optarg, "SHA256"))
                signOptions.hashAlgTag = SEC_OID_SHA256;
            else if (!strcmp(optarg, "SHA384"))
                signOptions.hashAlgTag = SEC_OID_SHA384;
            else if (!strcmp(optarg, "SHA512"))
                signOptions.hashAlgTag = SEC_OID_SHA512;
            else {
                sec_error("option -H requires one of MD2,MD4,MD5,SHA1,SHA256,SHA384,SHA512");
                goto loser;
            }
                break;
        case 'N':
            if (mode != SIGN) {
                sec_error("option -N only supported with option -S");
                result = 2; /* Trigger usage message. */
                goto loser;
            }
            signOptions.nickname = strdup(optarg);
            break;
        case 'O':
            mode = CERTSONLY;
            break;
        case 'P':
            if (mode != SIGN) {
                sec_error("option -P only supported with option -S");
                result = 2; /* Trigger usage message. */
                goto loser;
            }
            signOptions.smimeProfile = true;
            break;
        case 'S':
            mode = SIGN;
            break;
        case 'T':
            if (mode != SIGN) {
                sec_error("option -T only supported with option -S");
                result = 2; /* Trigger usage message. */
                goto loser;
            }
            signOptions.detached = true;
            break;
        case 'Y':
            if (mode != SIGN) {
                sec_error("option -Y only supported with option -S");
                result = 2; /* Trigger usage message. */
                goto loser;
            }
            signOptions.encryptionKeyPreferenceNick = strdup(optarg);
            break;
            
        case 'c':
            if (mode != DECODE)
            {
                sec_error("option -c only supported with option -D");
                result = 2; /* Trigger usage message. */
                goto loser;
            }
            if ((decodeOptions.contentFile = fopen(optarg, "rb")) == NULL)
            {
                sec_error("unable to open \"%s\" for reading: %s", optarg, strerror(errno));
                result = 1;
                goto loser;
            }
            break;
            
#ifdef HAVE_DODUMPSTATES
        case 'd':
            doDumpStates++;
            break;
#endif /* HAVE_DODUMPSTATES */
            
        case 'e':
            envFileName = strdup(optarg);
            encryptOptions.envFile = fopen(envFileName, "rb");	// PR_RDONLY, 00660);
            break;
            
        case 'h':
            if (mode != DECODE) {
                sec_error("option -h only supported with option -D");
                result = 2; /* Trigger usage message. */
                goto loser;
            }
            decodeOptions.headerLevel = atoi(optarg);
            if (decodeOptions.headerLevel < 0) {
                sec_error("option -h cannot have a negative value");
                goto loser;
            }
                break;
        case 'i':
            inFile = fopen(optarg,"rb");	// PR_RDONLY, 00660);
            if (inFile == NULL)
            {
                sec_error("unable to open \"%s\" for reading: %s", optarg, strerror(errno));
                goto loser;
            }
            break;
            
        case 'k':
            keychainName = optarg;
            break;
            
        case 'n':
            if (mode != DECODE)
            {
                sec_error("option -n only supported with option -D");
                result = 2; /* Trigger usage message. */
                goto loser;
            }
            decodeOptions.suppressContent = true;
            break;
        case 'o':
            outFile = fopen(optarg, "wb");
            if (outFile == NULL)
            {
                sec_error("unable to open \"%s\" for writing: %s", optarg, strerror(errno));
                goto loser;
            }
            break;
        case 'p':
            if (!optarg)
            {
                sec_error("option -p must have a value");
                result = 2; /* Trigger usage message. */
                goto loser;
            }
            
            options.password = (PK11PasswordFunc)ownpw;//strdup(optarg);
            break;
            
        case 'r':
            if (!optarg)
            {
                sec_error("option -r must have a value");
                result = 2; /* Trigger usage message. */
                goto loser;
            }

            envelopeOptions.recipients = ptrarray;
            str = (char *)optarg;
            do {
                tok = strchr(str, ',');
                if (tok) *tok = '\0';
                envelopeOptions.recipients[nrecipients++] = strdup(str);
                if (tok) str = tok + 1;
            } while (tok);
                envelopeOptions.recipients[nrecipients] = NULL;
            encryptOptions.recipients = envelopeOptions.recipients;
            certsonlyOptions.recipients = envelopeOptions.recipients;
            break;
            
        case 's':
            cms_update_single_byte = 1;
            break;
            
        case 'u':
        {
            int usageType = atoi (strdup(optarg));
            if (usageType < certUsageSSLClient || usageType > certUsageAnyCA)
            {
                result = 1;
                goto loser;
            }
            options.certUsage = (SECCertUsage)usageType;
            break;
        }
        case 'v':
            cms_verbose = 1;
            break;
        default:
            result = 2; /* Trigger usage message. */
            goto loser;
        }
    }

	argc -= optind;
	argv += optind;

    if (argc != 0 || mode == UNKNOWN)
    {
        result = 2; /* Trigger usage message. */
        goto loser;
    }

    result = 0;
    
    if (mode != CERTSONLY)
        SECU_FileToItem(&input, inFile);
    if (inFile != stdin)
		fclose(inFile);
    if (cms_verbose)
        fprintf(stderr, "received commands\n");
    
    /* Call the libsec initialization routines */
    if (keychainName)
    {
		check_obsolete_keychain(keychainName);
        statusX = SecKeychainOpen(keychainName, &options.certDBHandle);
        if (!options.certDBHandle)
        {
            sec_perror("SecKeychainOpen", statusX);
            result = 1;
            goto loser;
        }
    }

    if (cms_verbose)
        fprintf(stderr, "Got default certdb\n");
    
    switch (mode)
    {
    case DECODE:
        decodeOptions.options = &options;
        if (encryptOptions.envFile)
        {
            /* Decoding encrypted-data, so get the bulkkey from an
            * enveloped-data message.
            */
            SECU_FileToItem(&envmsg, encryptOptions.envFile);
            decodeOptions.options = &options;
            encryptOptions.envmsg = decode(NULL, &dummy, &envmsg, &decodeOptions);
            if (!encryptOptions.envmsg)
            {
                sec_error("problem decoding env msg");
                result = 1;
                break;
            }
            rv = get_enc_params(&encryptOptions);
            decodeOptions.dkcb = dkcb;
            decodeOptions.bulkkey = encryptOptions.bulkkey;
        }
        cmsg = decode(outFile, &output, &input, &decodeOptions);
        if (!cmsg)
        {
            sec_error("problem decoding");
            result = 1;
        }
        fwrite(output.Data, output.Length, 1, outFile);
        break;
    case SIGN:
        signOptions.options = &options;
        cmsg = signed_data(&signOptions);
        if (!cmsg)
        {
            sec_error("problem signing");
            result = 1;
        }
        break;
    case ENCRYPT:
        if (!envFileName)
        {
            sec_error("you must specify an envelope file with -e");
            result = 1;
            goto loser;
        }
        encryptOptions.options = &options;
        encryptOptions.input = &input;
        encryptOptions.outfile = outFile;
        if (!encryptOptions.envFile) {
            encryptOptions.envFile = fopen(envFileName,"wb");	//PR_WRONLY|PR_CREATE_FILE, 00660);
            if (!encryptOptions.envFile)
            {
                sec_error("failed to create file %s: %s", envFileName, strerror(errno));
                result = 1;
                goto loser;
            }
        }
        else
        {
            SECU_FileToItem(&envmsg, encryptOptions.envFile);
            decodeOptions.options = &options;
            encryptOptions.envmsg = decode(NULL, &dummy, &envmsg, 
                                           &decodeOptions);
            if (encryptOptions.envmsg == NULL)
            {
                sec_error("problem decrypting env msg");
                result = 1;
                break;
            }
        }

        /* decode an enveloped-data message to get the bulkkey (create
         * a new one if neccessary)
         */
        rv = get_enc_params(&encryptOptions);
        /* create the encrypted-data message */
        cmsg = encrypted_data(&encryptOptions);
        if (!cmsg)
        {
            sec_error("problem encrypting");
            result = 1;
        }

        if (encryptOptions.bulkkey)
        {
            CFRelease(encryptOptions.bulkkey);
            encryptOptions.bulkkey = NULL;
        }
        break;
    case ENVELOPE:
        envelopeOptions.options = &options;
        cmsg = enveloped_data(&envelopeOptions);
        if (!cmsg)
        {
            sec_error("problem enveloping");
            result = 1;
        }
        break;
    case CERTSONLY:
        certsonlyOptions.options = &options;
        cmsg = signed_data_certsonly(&certsonlyOptions);
        if (!cmsg)
        {
            sec_error("problem with certs-only");
            result = 1;
        }
        break;
    case UNKNOWN:
        /* Already handled above. */
        break;
    }

    if ( (mode == SIGN || mode == ENVELOPE || mode == CERTSONLY)
         && (!result) )
     {
        SecArenaPoolRef arena = NULL;
        SecCmsEncoderRef ecx;
        CSSM_DATA output = {};

        SEC_CHECK(SecArenaPoolCreate(1024, &arena), "failed to create arenapool");
        pwcb     = (PK11PasswordFunc)((options.password != NULL) ? ownpw : NULL);
        pwcb_arg = (options.password != NULL) ? (void *)options.password : NULL;
        if (cms_verbose) {
            fprintf(stderr, "cmsg [%p]\n", cmsg);
            fprintf(stderr, "arena [%p]\n", arena);
            if (pwcb_arg)
                fprintf(stderr, "password [%s]\n", (char *)pwcb_arg);
            else
                fprintf(stderr, "password [NULL]\n");
        }

        SEC_CHECK(SecCmsEncoderCreate(cmsg, 
                                      NULL, NULL,     /* DER output callback  */
                                      &output, arena, /* destination storage  */
                                      pwcb, pwcb_arg, /* password callback    */
                                      NULL, NULL,     /* decrypt key callback */
                                      NULL, NULL,      /* detached digests    */
                                      &ecx),
                  "cannot create encoder context");
        if (cms_verbose)
        {
            fprintf(stderr, "input len [%ld]\n", input.Length);
            {
                unsigned int j; 
                for (j = 0; j < input.Length; ++j)
                    fprintf(stderr, "%2x%c", input.Data[j], (j>0&&j%35==0)?'\n':' ');
            }
        }

        if (input.Length > 0) { /* skip if certs-only (or other zero content) */
            SEC_CHECK(SecCmsEncoderUpdate(ecx, (char *)input.Data, input.Length),
                      "failed to add data to encoder");
        }

        SEC_CHECK(SecCmsEncoderFinish(ecx), "failed to encode data");

        if (cms_verbose) {
            fprintf(stderr, "encoding passed\n");
        }

        /*PR_Write(output.data, output.len);*/
        fwrite(output.Data, output.Length, 1, outFile);
        if (cms_verbose) {
            fprintf(stderr, "wrote to file\n");
        }
        SecArenaPoolFree(arena, false);
    }

loser:
    if (cmsg)
        SecCmsMessageDestroy(cmsg);
    if (outFile != stdout)
        fclose(outFile);

    if (decodeOptions.contentFile)
        fclose(decodeOptions.contentFile);

    return result;
}


#pragma mark ================ Misc from NSS ===================
// from /security/nss/cmd/lib/secutil.c

OSStatus
SECU_FileToItem(CSSM_DATA *dst, FILE *src)
{
    const int kReadSize = 4096;
    size_t bytesRead, totalRead = 0;

    do
    {
        /* Make room in dst for the new data. */
        dst->Length += kReadSize;
        dst->Data = realloc(dst->Data, dst->Length);
        if (!dst->Data)
            return 1 /* @@@ memFullErr */;

        bytesRead = fread (&dst->Data[totalRead], 1, kReadSize, src);
        totalRead += bytesRead;
    } while (bytesRead == kReadSize);

    if (!feof (src))
    {
        /* We are here, but there's no EOF.  This is bad */
         if (dst->Data) {
            free(dst->Data);
            dst->Data = NULL;
            dst->Length = 0;
        }
        return 1 /* @@@ ioErr */;
    }

    /* Trim down the buffer. */
    dst->Length = totalRead;
    dst->Data = realloc(dst->Data, totalRead);
    if (!dst->Data)
        return 1 /* @@@ memFullErr */;

    return noErr;
}
