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
 * cmsutil -- A command to work with CMS data
 */

#include <SecurityNssSmime/cms.h>
#include <SecurityNssSmime/smime.h>
// @@@ Eleminate usage of this header.
#include "cert.h"

#include <SecurityNssAsn1/secerr.h>

#include <Security/SecKeychain.h>
#include <Security/SecIdentity.h>
#include <CoreFoundation/CFString.h>
#include "secitem.h"

#include <stdio.h>

#include "plgetopt.h"


/*
    Mapping:

NSS_CMS					->	SecCms
CERTCertificate			->	SecCertificateRef
CERTCertDBHandle		->	CSSM_DL_DB_HANDLE
CERT_GetDefaultCertDB	->	OSStatus SecKeychainCopyDefault(SecKeychainRef *keychain);
CERT_DestroyCertificate	->	CFRelease?
SecKeychainSearchCreateFromAttributes
_CERT_FindCertByNicknameOrEmailAddr
_CERT_FindUserCertByUsage	---> OSStatus SecIdentitySearchCreate(CFTypeRef keychainOrArray, CSSM_KEYUSE keyUsage, SecIdentitySearchRef *searchRef);
CERT_FindCertByNicknameOrEmailAddr
PRFileDesc *			->	FILE * (normal file descriptor)
*/

//typedef long PRFileDesc; 

#include <stdarg.h>

void SECU_PrintError(char *progName, char *msg, ...);
SECStatus SECU_FileToItem(SECItem *dst, FILE *src);

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#if defined(_WIN32)
#include "fcntl.h"
#include "io.h"
#endif

#include <stdio.h>
#include <string.h>

extern void SEC_Init(void);		/* XXX */
char *progName = NULL;
static int cms_verbose = 0;

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

static SECStatus DigestFile(PLArenaPool *poolp, SECItem ***digests, SECItem *input, SECAlgorithmID **algids)
{
    SecCmsDigestContext *digcx = SecCmsDigestContextStartMultiple(algids);
    if (digcx == NULL)
        return SECFailure;

    SecCmsDigestContextUpdate(digcx, input->Data, input->Length);

    return SecCmsDigestContextFinishMultiple(digcx, poolp, digests);
}

static void Usage(char *progName)
{
    fprintf(stderr, 
"Usage:  %s [-D|-S|-E] [<options>] [-d dbdir] [-u certusage]\n", 
	    progName);
    fprintf(stderr, 
"  -D           decode a CMS message\n"
"  -c content   use this detached content\n"
"  -n           suppress output of content\n"
"  -h num       generate email headers with info about CMS message\n"
"  -S           create a CMS signed message\n"
"  -G           include a signing time attribute\n"
"  -H hash      use hash (default:SHA1)\n"
"  -N nick      use certificate named \"nick\" for signing\n"
"  -P           include a SMIMECapabilities attribute\n"
"  -T           do not include content in CMS message\n"
"  -Y nick      include a EncryptionKeyPreference attribute with cert\n"
"                 (use \"NONE\" to omit)\n"
"  -E           create a CMS enveloped message (NYI)\n"
"  -r id,...    create envelope for these recipients,\n"
"               where id can be a certificate nickname or email address\n"
"  -k keychain  keychain to use\n"
"  -i infile    use infile as source of data (default: stdin)\n"
"  -o outfile   use outfile as destination of data (default: stdout)\n"
"  -p password  use password as key db password (default: prompt)\n"
"  -u certusage set type of certificate usage (default: certUsageEmailSigner)\n"
"  -v           print debugging information\n"
"\nCert usage codes:\n");
    fprintf(stderr, "%-25s  0 - certUsageSSLClient\n", " ");
    fprintf(stderr, "%-25s  1 - certUsageSSLServer\n", " ");
    fprintf(stderr, "%-25s  2 - certUsageSSLServerWithStepUp\n", " ");
    fprintf(stderr, "%-25s  3 - certUsageSSLCA\n", " ");
    fprintf(stderr, "%-25s  4 - certUsageEmailSigner\n", " ");
    fprintf(stderr, "%-25s  5 - certUsageEmailRecipient\n", " ");
    fprintf(stderr, "%-25s  6 - certUsageObjectSigner\n", " ");
    fprintf(stderr, "%-25s  7 - certUsageUserCertImport\n", " ");
    fprintf(stderr, "%-25s  8 - certUsageVerifyCA\n", " ");
    fprintf(stderr, "%-25s  9 - certUsageProtectedObjectSigner\n", " ");
    fprintf(stderr, "%-25s 10 - certUsageStatusResponder\n", " ");
    fprintf(stderr, "%-25s 11 - certUsageAnyCA\n", " ");

    exit(-1);
}

char *
ownpw(void *info, PRBool retry, void *arg)
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
    PRBool suppressContent;
    SecCmsGetDecryptKeyCallback dkcb;
    SecSymmetricKeyRef bulkkey;
};

struct signOptionsStr {
    struct optionsStr *options;
    char *nickname;
    char *encryptionKeyPreferenceNick;
    PRBool signingTime;
    PRBool smimeProfile;
    PRBool detached;
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
    SecCmsMessage *envmsg;
    SECItem *input;
    FILE *outfile;
    FILE *envFile;
    SecSymmetricKeyRef bulkkey;
    SECOidTag bulkalgtag;
    int keysize;
};

static SecCmsMessage *decode(FILE *out, SECItem *output, SECItem *input,const struct decodeOptionsStr *decodeOptions)
{
    SecCmsDecoderContext *dcx;
    SecCmsMessage *cmsg;
    SecCmsContentInfo *cinfo;
    SecCmsSignedData *sigd = NULL;
    SecCmsEnvelopedData *envd;
    SecCmsEncryptedData *encd;
    SECAlgorithmID **digestalgs;
    int nlevels, i, nsigners, j;
    CFStringRef signercn;
    SecCmsSignerInfo *si;
    SECOidTag typetag;
    SECItem **digests;
    PLArenaPool *poolp;
    PK11PasswordFunc pwcb;
    void *pwcb_arg;
    SECItem *item, sitem = { 0, };
    CFTypeRef policy = NULL;

    pwcb     = (PK11PasswordFunc)((decodeOptions->options->password != NULL) ? ownpw : NULL);
    pwcb_arg = (decodeOptions->options->password != NULL) ? 
                  (void *)decodeOptions->options->password : NULL;

    if (decodeOptions->contentFile) // detached content: grab content file
        SECU_FileToItem(&sitem, decodeOptions->contentFile);

    dcx = SecCmsDecoderStart(NULL, 
                               NULL, NULL,         /* content callback     */
                               pwcb, pwcb_arg,     /* password callback    */
			       decodeOptions->dkcb, /* decrypt key callback */
                               decodeOptions->bulkkey);
    (void)SecCmsDecoderUpdate(dcx, (char *)input->Data, input->Length);
    cmsg = SecCmsDecoderFinish(dcx);
    if (cmsg == NULL)
    {
        fprintf(stderr, "%s: failed to decode message.\n", progName);
        return NULL;
    }

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
	    sigd = (SecCmsSignedData *)SecCmsContentInfoGetContent(cinfo);
	    if (sigd == NULL)
	    {
		SECU_PrintError(progName,"problem finding signedData component");
		goto loser;
	    }

	    /* if we have a content file, but no digests for this signedData */
	    if (decodeOptions->contentFile != NULL && !SecCmsSignedDataHasDigests(sigd))
	    {
		if ((poolp = PORT_NewArena(1024)) == NULL)
		{
		    fprintf(stderr, "cmsutil: Out of memory.\n");
		    goto loser;
		}
		digestalgs = SecCmsSignedDataGetDigestAlgs(sigd);
		if (DigestFile (poolp, &digests, &sitem, digestalgs) != SECSuccess)
		{
		    SECU_PrintError(progName,"problem computing message digest");
		    goto loser;
		}
		if (SecCmsSignedDataSetDigests(sigd, digestalgs, digests) != SECSuccess)
		{
		    
		    SECU_PrintError(progName,"problem setting message digests");
		    goto loser;
		}
		PORT_FreeArena(poolp, false);
	    }

	    policy = CERT_PolicyForCertUsage(decodeOptions->options->certUsage);
	    // import the certificates
	    if (SecCmsSignedDataImportCerts(sigd,decodeOptions->options->certDBHandle,decodeOptions->options->certUsage, 
	                                   false) != SECSuccess)
	    {
		SECU_PrintError(progName, "cert import failed");
		goto loser;
	    }

	    /* find out about signers */
	    nsigners = SecCmsSignedDataSignerInfoCount(sigd);
	    if (decodeOptions->headerLevel >= 0)
		fprintf(out, "nsigners=%d; ", nsigners);
	    if (nsigners == 0)
	    {
		/* must be a cert transport message */
		SECStatus rv;
		/* XXX workaround for bug #54014 */
		SecCmsSignedDataImportCerts(sigd,decodeOptions->options->certDBHandle,
		    decodeOptions->options->certUsage,true);
		rv = SecCmsSignedDataVerifyCertsOnly(sigd,decodeOptions->options->certDBHandle, 
					    policy);
		if (rv != SECSuccess)
		{
		    fprintf(stderr, "cmsutil: Verify certs-only failed!\n");
		    goto loser;
		}
		return cmsg;
	    }

	    if (!SecCmsSignedDataHasDigests(sigd))	// still no digests?
	    {
		SECU_PrintError(progName, "no message digests");
		goto loser;
	    }

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
	    envd = (SecCmsEnvelopedData *)SecCmsContentInfoGetContent(cinfo);
	    break;
	case SEC_OID_PKCS7_ENCRYPTED_DATA:
	    if (decodeOptions->headerLevel >= 0)
            fprintf(out, "type=encryptedData; ");
	    encd = (SecCmsEncryptedData *)SecCmsContentInfoGetContent(cinfo);
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
	SECITEM_CopyItem(NULL, output, item);
    }

    if (policy)
	CFRelease(policy);

    return cmsg;
loser:
    if (policy)
	CFRelease(policy);
    if (cmsg)
	SecCmsMessageDestroy(cmsg);
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

static SecCmsMessage *signed_data(struct signOptionsStr *signOptions)
{
    SecCmsMessage *cmsg = NULL;
    SecCmsContentInfo *cinfo;
    SecCmsSignedData *sigd;
    SecCmsSignerInfo *signerinfo;
    SecIdentityRef identity = NULL;
    SecCertificateRef cert = NULL, ekpcert = NULL;

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
    if (signOptions->nickname == NULL)
    {
        fprintf(stderr, "ERROR: please indicate the nickname of a certificate to sign with.\n");
        return NULL;
    }
    if ((identity = CERT_FindIdentityByUsage(signOptions->options->certDBHandle, 
                                         signOptions->nickname,
                                         signOptions->options->certUsage,
                                         false,
                                         NULL)) == NULL)
    {
        SECU_PrintError(progName,"the corresponding cert for key \"%s\" does not exist",signOptions->nickname);
        return NULL;
    }
    if (cms_verbose)
        fprintf(stderr, "Found identity for %s\n", signOptions->nickname);
    // Get the cert from the identity
    if (SecIdentityCopyCertificate(identity, &cert))
	goto loser;
    // create the message object
    cmsg = SecCmsMessageCreate(NULL); // create a message on its own pool
    if (cmsg == NULL)
    {
        fprintf(stderr, "ERROR: cannot create CMS message.\n");
        return NULL;
    }
    // build chain of objects: message->signedData->data
    if ((sigd = SecCmsSignedDataCreate(cmsg)) == NULL)
    {
        fprintf(stderr, "ERROR: cannot create CMS signedData object.\n");
        goto loser;
    }
    cinfo = SecCmsMessageGetContentInfo(cmsg);
    if (SecCmsContentInfoSetContentSignedData(cmsg, cinfo, sigd) != SECSuccess)
    {
        fprintf(stderr, "ERROR: cannot attach CMS signedData object.\n");
        goto loser;
    }
    cinfo = SecCmsSignedDataGetContentInfo(sigd);
    /* we're always passing data in and detaching optionally */
    if (SecCmsContentInfoSetContentData(cmsg, cinfo, NULL, 
                                           signOptions->detached) 
          != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
	goto loser;
    }
    /* 
     * create & attach signer information
     */
    signerinfo = SecCmsSignerInfoCreate(cmsg, identity, signOptions->hashAlgTag);
    if (signerinfo == NULL)
    {
        fprintf(stderr, "ERROR: cannot create CMS signerInfo object.\n");
        goto loser;
    }
    if (cms_verbose)
        fprintf(stderr,"Created CMS message, added signed data w/ signerinfo\n");

    // we want the cert chain included for this one
    if (SecCmsSignerInfoIncludeCerts(signerinfo, SecCmsCMCertChain, signOptions->options->certUsage) != SECSuccess)
    {
        fprintf(stderr, "ERROR: cannot find cert chain.\n");
        goto loser;
    }
    if (cms_verbose)
        fprintf(stderr, "imported certificate\n");
    if (signOptions->signingTime)
    {
        if (SecCmsSignerInfoAddSigningTime(signerinfo, CFAbsoluteTimeGetCurrent()) != SECSuccess)
        {
            fprintf(stderr, "ERROR: cannot add signingTime attribute.\n");
            goto loser;
        }
    }
    if (signOptions->smimeProfile)
    {
        if (SecCmsSignerInfoAddSMIMECaps(signerinfo) != SECSuccess)
        {
            fprintf(stderr, "ERROR: cannot add SMIMECaps attribute.\n");
            goto loser;
        }
    }

    if (!signOptions->encryptionKeyPreferenceNick)
    {
	/* check signing cert for fitness as encryption cert */
        SECStatus FitForEncrypt = CERT_CheckCertUsage(cert,
                                                      certUsageEmailRecipient);

        if (SECSuccess == FitForEncrypt)
        {
            /* if yes, add signing cert as EncryptionKeyPreference */
            if (SecCmsSignerInfoAddSMIMEEncKeyPrefs(signerinfo, cert, signOptions->options->certDBHandle)
                  != SECSuccess) {
                fprintf(stderr, 
                    "ERROR: cannot add default SMIMEEncKeyPrefs attribute.\n");
                goto loser;
            }
            if (SecCmsSignerInfoAddMSSMIMEEncKeyPrefs(signerinfo, cert, signOptions->options->certDBHandle)
                  != SECSuccess) {
                fprintf(stderr, 
                    "ERROR: cannot add default MS SMIMEEncKeyPrefs attribute.\n");
                goto loser;
            }
        } else {
            /* this is a dual-key cert case, we need to look for the encryption
               certificate under the same nickname as the signing cert */
            /* get the cert, add it to the message */
            if ((ekpcert = CERT_FindUserCertByUsage(
                                              signOptions->options->certDBHandle,
                                              signOptions->nickname,
                                              certUsageEmailRecipient,
                                              false,
                                              NULL)) == NULL) {
                SECU_PrintError(progName, 
                         "the corresponding cert for key \"%s\" does not exist",
                         signOptions->encryptionKeyPreferenceNick);
                goto loser;
            }
            if (SecCmsSignerInfoAddSMIMEEncKeyPrefs(signerinfo, ekpcert,signOptions->options->certDBHandle)
                  != SECSuccess) {
                fprintf(stderr, 
                        "ERROR: cannot add SMIMEEncKeyPrefs attribute.\n");
                goto loser;
            }
            if (SecCmsSignerInfoAddMSSMIMEEncKeyPrefs(signerinfo, ekpcert,signOptions->options->certDBHandle)
                  != SECSuccess) {
                fprintf(stderr, 
                        "ERROR: cannot add MS SMIMEEncKeyPrefs attribute.\n");
                goto loser;
            }
            if (SecCmsSignedDataAddCertificate(sigd, ekpcert) != SECSuccess) {
                fprintf(stderr, "ERROR: cannot add encryption certificate.\n");
                goto loser;
            }
        }
    } else if (strcmp(signOptions->encryptionKeyPreferenceNick, "NONE") == 0) {
        /* No action */
    } else {
	/* get the cert, add it to the message */
	if ((ekpcert = CERT_FindUserCertByUsage(
                                     signOptions->options->certDBHandle, 
	                             signOptions->encryptionKeyPreferenceNick,
                                     certUsageEmailRecipient, false, NULL))
	      == NULL) {
	    SECU_PrintError(progName, 
	               "the corresponding cert for key \"%s\" does not exist",
	                signOptions->encryptionKeyPreferenceNick);
	    goto loser;
	}
	if (SecCmsSignerInfoAddSMIMEEncKeyPrefs(signerinfo, ekpcert,signOptions->options->certDBHandle)
	      != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add SMIMEEncKeyPrefs attribute.\n");
	    goto loser;
	}
	if (SecCmsSignerInfoAddMSSMIMEEncKeyPrefs(signerinfo, ekpcert,signOptions->options->certDBHandle)
	      != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add MS SMIMEEncKeyPrefs attribute.\n");
	    goto loser;
	}
	if (SecCmsSignedDataAddCertificate(sigd, ekpcert) != SECSuccess) {
	    fprintf(stderr, "ERROR: cannot add encryption certificate.\n");
	    goto loser;
	}
    }

    if (SecCmsSignedDataAddSignerInfo(sigd, signerinfo) != SECSuccess)
    {
        fprintf(stderr, "ERROR: cannot add CMS signerInfo object.\n");
        goto loser;
    }
    if (cms_verbose)
        fprintf(stderr, "created signed-data message\n");
    if (ekpcert)
        CFRelease(ekpcert);
    if (cert)
        CFRelease(cert);
    if (identity)
	CFRelease(identity);
    return cmsg;
loser:
    if (ekpcert)
        CFRelease(ekpcert);
    if (cert)
        CFRelease(cert);
    if (identity)
	CFRelease(identity);
    SecCmsMessageDestroy(cmsg);
    return NULL;
}

static SecCmsMessage *enveloped_data(struct envelopeOptionsStr *envelopeOptions)
{
    SecCmsMessage *cmsg = NULL;
    SecCmsContentInfo *cinfo;
    SecCmsEnvelopedData *envd;
    SecCmsRecipientInfo *recipientinfo;
    SecCertificateRef *recipientcerts = NULL;
    SecKeychainRef dbhandle;
    PLArenaPool *tmppoolp = NULL;
    SECOidTag bulkalgtag;
    int keysize, i = 0;
    int cnt;
    dbhandle = envelopeOptions->options->certDBHandle;
    /* count the recipients */
    if ((cnt = nss_CMSArray_Count((void **)envelopeOptions->recipients)) == 0)
    {
        fprintf(stderr, "ERROR: please name at least one recipient.\n");
        goto loser;
    }
    if ((tmppoolp = PORT_NewArena (1024)) == NULL)
    {
        fprintf(stderr, "ERROR: out of memory.\n");
        goto loser;
    }
    // XXX find the recipient's certs by email address or nickname
    if ((recipientcerts = (SecCertificateRef *)PORT_ArenaZAlloc(tmppoolp,(cnt+1)*sizeof(SecCertificateRef))) == NULL)
    {
        fprintf(stderr, "ERROR: out of memory.\n");
        goto loser;
    }
    for (i=0; envelopeOptions->recipients[i] != NULL; i++)
    {
        if ((recipientcerts[i] = CERT_FindCertByNicknameOrEmailAddr(dbhandle,envelopeOptions->recipients[i])) == NULL)
        {
            SECU_PrintError(progName, "cannot find certificate for \"%s\"", 
                            envelopeOptions->recipients[i]);
            i=0;
            goto loser;
        }
    }
    recipientcerts[i] = NULL;
    i=0;
    // find a nice bulk algorithm
    if (NSS_SMIMEUtil_FindBulkAlgForRecipients(recipientcerts, &bulkalgtag, &keysize) != SECSuccess)
    {
        fprintf(stderr, "ERROR: cannot find common bulk algorithm.\n");
        goto loser;
    }
    // create the message object
    cmsg = SecCmsMessageCreate(NULL); // create a message on its own pool
    if (cmsg == NULL)
    {
        fprintf(stderr, "ERROR: cannot create CMS message.\n");
        goto loser;
    }
    // build chain of objects: message->envelopedData->data
    if ((envd = SecCmsEnvelopedDataCreate(cmsg, bulkalgtag, keysize)) == NULL)
    {
        fprintf(stderr, "ERROR: cannot create CMS envelopedData object.\n");
        goto loser;
    }
    cinfo = SecCmsMessageGetContentInfo(cmsg);
    if (SecCmsContentInfoSetContentEnvelopedData(cmsg, cinfo, envd) != SECSuccess)
    {
        fprintf(stderr, "ERROR: cannot attach CMS envelopedData object.\n");
        goto loser;
    }
    cinfo = SecCmsEnvelopedDataGetContentInfo(envd);
    // we're always passing data in, so the content is NULL
    if (SecCmsContentInfoSetContentData(cmsg, cinfo, NULL, false) != SECSuccess)
    {
        fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
        goto loser;
    }
    // create & attach recipient information
    for (i = 0; recipientcerts[i] != NULL; i++)
    {
        if ((recipientinfo = SecCmsRecipientInfoCreate(cmsg, recipientcerts[i])) == NULL)
        {
            fprintf(stderr, "ERROR: cannot create CMS recipientInfo object.\n");
            goto loser;
        }
        if (SecCmsEnvelopedDataAddRecipient(envd, recipientinfo) != SECSuccess)
        {
            fprintf(stderr, "ERROR: cannot add CMS recipientInfo object.\n");
            goto loser;
        }
        CFRelease(recipientcerts[i]);
    }
    if (tmppoolp)
        PORT_FreeArena(tmppoolp, false);
    return cmsg;
loser:
    if (recipientcerts)
    {
        for (; recipientcerts[i] != NULL; i++)
            CFRelease(recipientcerts[i]);
    }
    if (cmsg)
	SecCmsMessageDestroy(cmsg);
    if (tmppoolp)
	PORT_FreeArena(tmppoolp, false);
    return NULL;
}

SecSymmetricKeyRef dkcb(void *arg, SECAlgorithmID *algid)
{
    return (SecSymmetricKeyRef)arg;
}

static SECStatus get_enc_params(struct encryptOptionsStr *encryptOptions)
{
    struct envelopeOptionsStr envelopeOptions;
    SECStatus rv = SECFailure;
    SecCmsMessage *env_cmsg;
    SecCmsContentInfo *cinfo;
    int i, nlevels;
    // construct an enveloped data message to obtain bulk keys
    if (encryptOptions->envmsg)
	env_cmsg = encryptOptions->envmsg; // get it from an old message
	else
    {
        SECItem dummyOut = { 0, };
        SECItem dummyIn  = { 0, };
        char str[] = "Hello!";
        PLArenaPool *tmparena = PORT_NewArena(1024);
        dummyIn.Data = (unsigned char *)str;
        dummyIn.Length = strlen(str);
        envelopeOptions.options = encryptOptions->options;
        envelopeOptions.recipients = encryptOptions->recipients;
        env_cmsg = enveloped_data(&envelopeOptions);
        SecCmsDEREncode(env_cmsg, &dummyIn, &dummyOut, tmparena);
        fwrite(dummyOut.Data, 1, dummyOut.Length,encryptOptions->envFile);
        PORT_FreeArena(tmparena, false);
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
            rv = SECSuccess;
            break;
        }
    }
    if (i == nlevels)
        fprintf(stderr, "%s: could not retrieve enveloped data.", progName);
    if (env_cmsg)
        SecCmsMessageDestroy(env_cmsg);
    return rv;
}

static SecCmsMessage *encrypted_data(struct encryptOptionsStr *encryptOptions)
{
    SECStatus rv = SECFailure;
    SecCmsMessage *cmsg = NULL;
    SecCmsContentInfo *cinfo;
    SecCmsEncryptedData *encd;
    SecCmsEncoderContext *ecx = NULL;
    PLArenaPool *tmppoolp = NULL;
    SECItem derOut = { 0, };
    /* arena for output */
    tmppoolp = PORT_NewArena(1024);
    if (!tmppoolp)
    {
        fprintf(stderr, "%s: out of memory.\n", progName);
        return NULL;
    }
    // create the message object
    cmsg = SecCmsMessageCreate(NULL);
    if (cmsg == NULL) {
	fprintf(stderr, "ERROR: cannot create CMS message.\n");
	goto loser;
    }
    /*
     * build chain of objects: message->encryptedData->data
     */
    if ((encd = SecCmsEncryptedDataCreate(cmsg, encryptOptions->bulkalgtag, 
                                                  encryptOptions->keysize)) 
           == NULL) {
	fprintf(stderr, "ERROR: cannot create CMS encryptedData object.\n");
	goto loser;
    }
    cinfo = SecCmsMessageGetContentInfo(cmsg);
    if (SecCmsContentInfoSetContentEncryptedData(cmsg, cinfo, encd)
          != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS encryptedData object.\n");
	goto loser;
    }
    cinfo = SecCmsEncryptedDataGetContentInfo(encd);
    /* we're always passing data in, so the content is NULL */
    if (SecCmsContentInfoSetContentData(cmsg, cinfo, NULL, false) 
          != SECSuccess) {
	fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
	goto loser;
    }
    ecx = SecCmsEncoderStart(cmsg, NULL, NULL, &derOut, tmppoolp, NULL, NULL,
                               dkcb, encryptOptions->bulkkey, NULL, NULL);
    if (!ecx) {
	fprintf(stderr, "%s: cannot create encoder context.\n", progName);
	goto loser;
    }
    rv = SecCmsEncoderUpdate(ecx, (char *)encryptOptions->input->Data, 
                                    encryptOptions->input->Length);
    if (rv) {
	fprintf(stderr, "%s: failed to add data to encoder.\n", progName);
	goto loser;
    }
    rv = SecCmsEncoderFinish(ecx);
    if (rv) {
	fprintf(stderr, "%s: failed to encrypt data.\n", progName);
	goto loser;
    }
    fwrite(derOut.Data, derOut.Length, 1, encryptOptions->outfile);
    /*
    if (bulkkey)
	CFRelease(bulkkey);
	*/
    if (tmppoolp)
	PORT_FreeArena(tmppoolp, false);
    return cmsg;
loser:
    /*
    if (bulkkey)
	CFRelease(bulkkey);
	*/
    if (tmppoolp)
	PORT_FreeArena(tmppoolp, false);
    if (cmsg)
	SecCmsMessageDestroy(cmsg);
    return NULL;
}

static SecCmsMessage *signed_data_certsonly(struct certsonlyOptionsStr *certsonlyOptions)
{
    SecCmsMessage *cmsg = NULL;
    SecCmsContentInfo *cinfo;
    SecCmsSignedData *sigd;
    SecCertificateRef *certs = NULL;
    SecKeychainRef dbhandle;
    PLArenaPool *tmppoolp = NULL;
    int i = 0, cnt;
    dbhandle = certsonlyOptions->options->certDBHandle;
    cnt = nss_CMSArray_Count((void**)certsonlyOptions->recipients);
#if 1
	if (cnt==0)
    {
        fprintf(stderr,"ERROR: please indicate the nickname of a certificate to sign with.\n");
        goto loser;
    }
#endif
    if ((tmppoolp = PORT_NewArena (1024)) == NULL)
    {
        fprintf(stderr, "ERROR: out of memory.\n");
        goto loser;
    }
    if ((certs = (SecCertificateRef *)PORT_ArenaZAlloc(tmppoolp,(cnt+1)*sizeof(SecCertificateRef))) == NULL)
    {
        fprintf(stderr, "ERROR: out of memory.\n");
        goto loser;
    }
    for (i=0; certsonlyOptions->recipients && certsonlyOptions->recipients[i] != NULL; i++)
    {
        if ((certs[i] = CERT_FindCertByNicknameOrEmailAddr(dbhandle,certsonlyOptions->recipients[i])) == NULL)
        {
            SECU_PrintError(progName, "cannot find certificate for \"%s\"",certsonlyOptions->recipients[i]);
            i=0;
            goto loser;
        }
    }
    certs[i] = NULL;
    i=0;
    // create the message object
    cmsg = SecCmsMessageCreate(NULL);
    if (cmsg == NULL)
    {
        fprintf(stderr, "ERROR: cannot create CMS message.\n");
        goto loser;
    }
    // build chain of objects: message->signedData->data
    if ((sigd = SecCmsSignedDataCreateCertsOnly(cmsg, certs[0], true)) == NULL)
    {
        fprintf(stderr, "ERROR: cannot create CMS signedData object.\n");
        goto loser;
    }
    CFRelease(certs[0]);
    for (i=1; i<cnt; i++)
    {
        if (SecCmsSignedDataAddCertChain(sigd, certs[i]))
        {
            fprintf(stderr, "ERROR: cannot add cert chain for \"%s\".\n",
                    certsonlyOptions->recipients[i]);
            goto loser;
        }
        CFRelease(certs[i]);
    }
    cinfo = SecCmsMessageGetContentInfo(cmsg);
    if (SecCmsContentInfoSetContentSignedData(cmsg, cinfo, sigd) != SECSuccess)
    {
        fprintf(stderr, "ERROR: cannot attach CMS signedData object.\n");
        goto loser;
    }
    cinfo = SecCmsSignedDataGetContentInfo(sigd);
    if (SecCmsContentInfoSetContentData(cmsg, cinfo, NULL, false) != SECSuccess)
    {
        fprintf(stderr, "ERROR: cannot attach CMS data object.\n");
        goto loser;
    }
    if (tmppoolp)
        PORT_FreeArena(tmppoolp, false);
    return cmsg;
loser:
    if (certs)
    {
        for (; i<cnt; i++)
            CFRelease(certs[i]);
    }
    if (cmsg)
        SecCmsMessageDestroy(cmsg);
    if (tmppoolp)
        PORT_FreeArena(tmppoolp, false);
    return NULL;
}

typedef enum { UNKNOWN, DECODE, SIGN, ENCRYPT, ENVELOPE, CERTSONLY } Mode;

int main(int argc, char **argv)
{
    FILE *outFile;
    SecCmsMessage *cmsg = NULL;
    FILE *inFile;
    PLOptState *optstate;
    PLOptStatus status;
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
    int exitstatus;
    static char *ptrarray[128] = { 0 };
    int nrecipients = 0;
    char *str, *tok;
    char *envFileName;
    const char *keychainName = NULL;
    SECItem input = { 0,};
    SECItem output = { 0,};
    SECItem dummy = { 0, };
    SECItem envmsg = { 0, };
    SECStatus rv;

    progName = strrchr(argv[0], '/');
    if (!progName)
       progName = strrchr(argv[0], '\\');
    progName = progName ? progName+1 : argv[0];

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
    optstate = PL_CreateOptState(argc, argv,"CDEGH:N:OPSTY:c:e:h:i:k:no:p:r:s:u:v");
    while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK)
    {
	switch (optstate->option)
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
		fprintf(stderr, 
		        "%s: option -G only supported with option -S.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.signingTime = true;
	    break;
       case 'H':
           if (mode != SIGN) {
               fprintf(stderr,
                       "%s: option -n only supported with option -D.\n",
                       progName);
               Usage(progName);
               exit(1);
           }
           decodeOptions.suppressContent = true;
           if (!strcmp(optstate->value, "MD2"))
               signOptions.hashAlgTag = SEC_OID_MD2;
           else if (!strcmp(optstate->value, "MD4"))
               signOptions.hashAlgTag = SEC_OID_MD4;
           else if (!strcmp(optstate->value, "MD5"))
               signOptions.hashAlgTag = SEC_OID_MD5;
           else if (!strcmp(optstate->value, "SHA1"))
               signOptions.hashAlgTag = SEC_OID_SHA1;
           else if (!strcmp(optstate->value, "SHA256"))
               signOptions.hashAlgTag = SEC_OID_SHA256;
           else if (!strcmp(optstate->value, "SHA384"))
               signOptions.hashAlgTag = SEC_OID_SHA384;
           else if (!strcmp(optstate->value, "SHA512"))
               signOptions.hashAlgTag = SEC_OID_SHA512;
           else {
               fprintf(stderr,
           "%s: -H requires one of MD2,MD4,MD5,SHA1,SHA256,SHA384,SHA512\n",
                       progName);
               exit(1);
           }
           break;
	case 'N':
	    if (mode != SIGN) {
		fprintf(stderr, 
		        "%s: option -N only supported with option -S.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.nickname = strdup(optstate->value);
	    break;
	case 'O':
	    mode = CERTSONLY;
	    break;
	case 'P':
	    if (mode != SIGN) {
		fprintf(stderr, 
		        "%s: option -P only supported with option -S.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.smimeProfile = true;
	    break;
	case 'S':
	    mode = SIGN;
	    break;
	case 'T':
	    if (mode != SIGN) {
		fprintf(stderr, 
		        "%s: option -T only supported with option -S.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.detached = true;
	    break;
	case 'Y':
	    if (mode != SIGN) {
		fprintf(stderr, 
		        "%s: option -Y only supported with option -S.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    signOptions.encryptionKeyPreferenceNick = strdup(optstate->value);
	    break;

	case 'c':
	    if (mode != DECODE)
        {
            fprintf(stderr, 
                    "%s: option -c only supported with option -D.\n", 
                    progName);
            Usage(progName);
            exit(1);
	    }
	    if ((decodeOptions.contentFile = fopen(optstate->value, "rb")) == NULL)
        {
            fprintf(stderr, "%s: unable to open \"%s\" for reading.\n",progName, optstate->value);
            exit(1);
	    }
	    break;
	case 'e':
	    envFileName = strdup(optstate->value);
	    encryptOptions.envFile = fopen(envFileName, "rb");	// PR_RDONLY, 00660);
	    break;

	case 'h':
	    if (mode != DECODE) {
		fprintf(stderr, 
		        "%s: option -h only supported with option -D.\n", 
		        progName);
		Usage(progName);
		exit(1);
	    }
	    decodeOptions.headerLevel = atoi(optstate->value);
	    if (decodeOptions.headerLevel < 0) {
		fprintf(stderr, "option -h cannot have a negative value.\n");
		exit(1);
	    }
	    break;
	case 'i':
	    inFile = fopen(optstate->value,"rb");	// PR_RDONLY, 00660);
	    if (inFile == NULL)
        {
            fprintf(stderr, "%s: unable to open \"%s\" for reading\n",progName, optstate->value);
            exit(1);
	    }
	    break;

	case 'k':
	    keychainName = optstate->value;
	    break;

	case 'n':
	    if (mode != DECODE)
        {
            fprintf(stderr, "%s: option -n only supported with option -D.\n",progName);
            Usage(progName);
            exit(1);
	    }
	    decodeOptions.suppressContent = true;
	    break;
	case 'o':
	    outFile = fopen(optstate->value, "wb");
	    if (outFile == NULL)
        {
            fprintf(stderr, "%s: unable to open \"%s\" for writing\n",progName, optstate->value);
            exit(1);
	    }
	    break;
	case 'p':
	    if (!optstate->value)
        {
            fprintf(stderr, "%s: option -p must have a value.\n", progName);
            Usage(progName);
            exit(1);
	    }
		
	    options.password = (PK11PasswordFunc)ownpw;//strdup(optstate->value);
	    break;

	case 'r':
	    if (!optstate->value)
        {
            fprintf(stderr, "%s: option -r must have a value.\n", progName);
            Usage(progName);
            exit(1);
	    }
	    envelopeOptions.recipients = ptrarray;
	    str = (char *)optstate->value;
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

	case 'u':
        {
            int usageType = atoi (strdup(optstate->value));
            if (usageType < certUsageSSLClient || usageType > certUsageAnyCA)
                return -1;
            options.certUsage = (SECCertUsage)usageType;
            break;
        }
	case 'v':
	    cms_verbose = 1;
	    break;

	}
    }
    if (status == PL_OPT_BAD)
	Usage(progName);
    PL_DestroyOptState(optstate);

    if (mode == UNKNOWN)
        Usage(progName);

    if (mode != CERTSONLY)
        SECU_FileToItem(&input, inFile);
    if (inFile != stdin)
		fclose(inFile);
    if (cms_verbose)
        fprintf(stderr, "received commands\n");

    /* Call the libsec initialization routines */
    if (keychainName)
    {
	statusX = SecKeychainOpen(keychainName, &options.certDBHandle);
	if (!options.certDBHandle)
	{
	    fprintf(stderr, "SecKeychainOpen: %s: %ld\n", keychainName, statusX);
	    SECU_PrintError(progName, "No default cert DB");
	    exit(1);
	}
    }

    if (cms_verbose)
        fprintf(stderr, "Got default certdb\n");

#if defined(_WIN32)
    if (outFile == stdout) {
	/* If we're going to write binary data to stdout, we must put stdout
	** into O_BINARY mode or else outgoing \n's will become \r\n's.
	*/
	int smrv = _setmode(_fileno(stdout), _O_BINARY);
	if (smrv == -1) {
	    fprintf(stderr,
	    "%s: Cannot change stdout to binary mode. Use -o option instead.\n",
	            progName);
	    return smrv;
	}
    }
#endif

    exitstatus = 0;
    switch (mode)
    {
    case DECODE:
	decodeOptions.options = &options;
	if (encryptOptions.envFile) {
	    /* Decoding encrypted-data, so get the bulkkey from an
	     * enveloped-data message.
	     */
	    SECU_FileToItem(&envmsg, encryptOptions.envFile);
	    decodeOptions.options = &options;
	    encryptOptions.envmsg = decode(NULL, &dummy, &envmsg, 
	                                   &decodeOptions);
	    if (!encryptOptions.envmsg) {
		SECU_PrintError(progName, "problem decoding env msg");
		exitstatus = 1;
		break;
	    }
	    rv = get_enc_params(&encryptOptions);
	    decodeOptions.dkcb = dkcb;
	    decodeOptions.bulkkey = encryptOptions.bulkkey;
	}
	cmsg = decode(outFile, &output, &input, &decodeOptions);
	if (!cmsg) {
	    SECU_PrintError(progName, "problem decoding");
	    exitstatus = 1;
	}
	fwrite(output.Data, output.Length, 1, outFile);
	break;
    case SIGN:
	signOptions.options = &options;
	cmsg = signed_data(&signOptions);
	if (!cmsg) {
	    SECU_PrintError(progName, "problem signing");
	    exitstatus = 1;
	}
	break;
    case ENCRYPT:
	if (!envFileName) {
	    fprintf(stderr, "%s: you must specify an envelope file with -e.\n",
	            progName);
	    exit(1);
	}
	encryptOptions.options = &options;
	encryptOptions.input = &input;
	encryptOptions.outfile = outFile;
	if (!encryptOptions.envFile) {
	    encryptOptions.envFile = fopen(envFileName,"wb");	//PR_WRONLY|PR_CREATE_FILE, 00660);
	    if (!encryptOptions.envFile) {
		fprintf(stderr, "%s: failed to create file %s.\n", progName,
		        envFileName);
		exit(1);
	    }
	} else {
	    SECU_FileToItem(&envmsg, encryptOptions.envFile);
	    decodeOptions.options = &options;
	    encryptOptions.envmsg = decode(NULL, &dummy, &envmsg, 
	                                   &decodeOptions);
	    if (encryptOptions.envmsg == NULL) {
	    	SECU_PrintError(progName, "problem decrypting env msg");
		exitstatus = 1;
	    	break;
	    }
	}
	/* decode an enveloped-data message to get the bulkkey (create
	 * a new one if neccessary)
	 */
	rv = get_enc_params(&encryptOptions);
	/* create the encrypted-data message */
	cmsg = encrypted_data(&encryptOptions);
	if (!cmsg) {
	    SECU_PrintError(progName, "problem encrypting");
	    exitstatus = 1;
	}
	if (encryptOptions.bulkkey) {
	    CFRelease(encryptOptions.bulkkey);
	    encryptOptions.bulkkey = NULL;
	}
	break;
    case ENVELOPE:
	envelopeOptions.options = &options;
	cmsg = enveloped_data(&envelopeOptions);
	if (!cmsg) {
	    SECU_PrintError(progName, "problem enveloping");
	    exitstatus = 1;
	}
	break;
    case CERTSONLY:
	certsonlyOptions.options = &options;
	cmsg = signed_data_certsonly(&certsonlyOptions);
	if (!cmsg) {
	    SECU_PrintError(progName, "problem with certs-only");
	    exitstatus = 1;
	}
	break;
    default:
	fprintf(stderr, "One of options -D, -S or -E must be set.\n");
	Usage(progName);
	exitstatus = 1;
    }
    if ( (mode == SIGN || mode == ENVELOPE || mode == CERTSONLY)
         && (!exitstatus) ) {
	PLArenaPool *arena = PORT_NewArena(1024);
	SecCmsEncoderContext *ecx;
	SECItem output = { 0, };
	if (!arena) {
	    fprintf(stderr, "%s: out of memory.\n", progName);
	    exit(1);
	}
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
	ecx = SecCmsEncoderStart(cmsg, 
                                   NULL, NULL,     /* DER output callback  */
                                   &output, arena, /* destination storage  */
                                   pwcb, pwcb_arg, /* password callback    */
                                   NULL, NULL,     /* decrypt key callback */
                                   NULL, NULL );   /* detached digests    */
	if (!ecx) {
	    fprintf(stderr, "%s: cannot create encoder context.\n", progName);
	    exit(1);
	}
	if (cms_verbose) {
	    fprintf(stderr, "input len [%ld]\n", input.Length);
	    { unsigned int j; 
		for(j=0;j<input.Length;j++)
	     fprintf(stderr, "%2x%c", input.Data[j], (j>0&&j%35==0)?'\n':' ');
	    }
	}
	if (input.Length > 0) { /* skip if certs-only (or other zero content) */
	    rv = SecCmsEncoderUpdate(ecx, (char *)input.Data, input.Length);
	    if (rv) {
		fprintf(stderr, 
		        "%s: failed to add data to encoder.\n", progName);
		exit(1);
	    }
	}
	rv = SecCmsEncoderFinish(ecx);
	if (rv) {
            SECU_PrintError(progName, "failed to encode data");
	    exit(1);
	}

	if (cms_verbose) {
	    fprintf(stderr, "encoding passed\n");
	}
	/*PR_Write(output.data, output.len);*/
	fwrite(output.Data, output.Length, 1, outFile);
	if (cms_verbose) {
	    fprintf(stderr, "wrote to file\n");
	}
	PORT_FreeArena(arena, false);
    }
    if (cmsg)
	SecCmsMessageDestroy(cmsg);
    if (outFile != stdout)
	fclose(outFile);

    if (decodeOptions.contentFile)
        fclose(decodeOptions.contentFile);
    exit(exitstatus);
}


#pragma mark ================ Misc from NSS ===================
// from /security/nss/cmd/lib/secutil.c

void SECU_PrintError(char *progName, char *msg, ...)
{
    va_list args;
//    PRErrorCode err = PORT_GetError();
  int err = -1;
    const char * errString = "fill in with real error";//SECU_Strerror(err);

    va_start(args, msg);

    fprintf(stderr, "%s: ", progName);
    vfprintf(stderr, msg, args);
    if (errString != NULL) // && PORT_Strlen(errString) > 0)
        fprintf(stderr, ": %s\n", errString);
    else
        fprintf(stderr, ": error %d\n", (int)err);

    va_end(args);
}

SECStatus
SECU_FileToItem(SECItem *dst, FILE *src)
{
    const int kReadSize = 4096;
    size_t bytesRead, totalRead = 0;

    dst->Data = 0;
    dst->Length = 0;

    do
    {
	/* Make room in dst for the new data. */
	if (SECITEM_ReallocItem (NULL, dst, dst->Length, dst->Length + kReadSize) != SECSuccess)
	{
	    SECITEM_FreeItem (dst, false);
	    return SECFailure;
	}

	bytesRead = fread (&dst->Data[totalRead], 1, kReadSize, src);
	totalRead += bytesRead;
    } while (bytesRead == kReadSize);

    /* Trim down the buffer. */
    if (SECITEM_ReallocItem (NULL, dst, dst->Length, totalRead) != SECSuccess)
    {
	SECITEM_FreeItem (dst, false);
	return SECFailure;
    }

    dst->Length = totalRead;

    if (!feof (src)) /* we are here, but there's no EOF.  This is bad */
    {
	PORT_SetError (SEC_ERROR_IO);
	return SECFailure;
    }

    return SECSuccess;
}
