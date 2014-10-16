/* Copyright (c) 2002-2004,2006,2008 Apple Inc.
 *
 * smimePolicy.cpp
 *
 * Test CSSMOID_APPLE_TP_SMIME and CSSMOID_APPLE_TP_ICHAT TP policies. 
 *
 */

#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/certVerify.h>
#include <clAppUtils/BlobList.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Security/cssm.h>
#include <Security/x509defs.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/oidsalg.h>
#include <Security/certextensions.h>
#include <Security/cssmapple.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <string.h>

/* key labels */
#define SUBJ_KEY_LABEL		"subjectKey"
#define ROOT_KEY_LABEL		"rootKey"

/* key and signature algorithm - shouldn't matter for this test */
#define SIG_ALG_DEFAULT		CSSM_ALGID_SHA1WithRSA
#define SIG_OID_DEFAULT		CSSMOID_SHA1WithRSA
#define KEY_ALG_DEFAULT		CSSM_ALGID_RSA

#define KEY_SIZE_DEFAULT	512

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("    p(rint leaf certs)\n");
	printf("    P(ause for MallocDebug)\n");
	printf("    q(uiet)\n");
	printf("    v(erbose)\n");
	exit(1);
}

/*
 * RDN components for root
 */
CSSM_APPLE_TP_NAME_OID rootRdn[] = 
{
	{ "Apple Computer",					&CSSMOID_OrganizationName },
	{ "The Big Cheese",					&CSSMOID_Title }
};
#define NUM_ROOT_NAMES	(sizeof(rootRdn) / sizeof(CSSM_APPLE_TP_NAME_OID))

/*
 * Test cases
 */
typedef struct {
	/* test description */
	const char		*testDesc;
	
	/* policy: CVP_SMIME or CVP_iChat */
	CertVerifyPolicy policy;
	
	/* email addrs for leaf cert - zero, one, or two of these */
	const char		*subjNameEmail;		// CSSMOID_EmailAddress
	const char		*subjAltNameEmail;	// RFC822Name
	CSSM_BOOL		subjAltNameCritical;
	
	/* common name portion of Subject Name, optional */
	const char		*subjNameCommon;
	
	/* email addrs for CertGroupVerify */
	const char		*vfyEmailAddrs;

	/* Key Usage - if zero, no KU extension */
	CE_KeyUsage		certKeyUse;
	CSSM_BOOL		keyUseCritical;
	
	/* key usage specified in CertGroupVerify */
	CE_KeyUsage		vfyKeyUse;
	
	/* ExtendedKeyUSage OID - NULL means no EKU */
	const CSSM_OID	*ekuOid;
	
	/* expected error - NULL or e.g. "CSSMERR_TP_VERIFY_ACTION_FAILED" */
	const char		*expectErrStr;
	
	/* one optional per-cert error string */
	/* e.g., "0:CSSMERR_APPLETP_SMIME_BAD_KEY_USE" */
	const char		*certErrorStr;
	
	/* addenda - at end so we don't have to update every existing test case.
	 * For iChat, orgUnit and org fields in SubjectName.
	 */
	const char		*orgUnit;
	const char		*orgName;		/* nominally either Apple Computer, Inc. */
	
} SP_TestCase;

/* 
 * upper case char in each component to make sure both ends get 
 * normalized as appropriate 
 */
#define GOOD_EMAIL		"Alice@Apple.com"
#define BAD_EMAIL		"bob@apple.com"		/* always bad */
#define CASE_EMAIL_GOOD	"Alice@APPLE.com"	/* always good */
/*
 * iChat: good
 * SMIME, emailAddrs in subjAltName: bad
 * SMIME, emailAddrs in SUbjectName RDN: good 
 */
#define CASE_EMAIL_BAD	"ALice@Apple.com"	
#define COMMON_NAME		"Alice"

/* for Apple-custom iChat name encoding */
/* first the constant org field, case sensitive */
#define ICHAT_ORG		"Apple Computer, Inc."		/* reference */
#define ICHAT_ORG_CASE	"APPLE Computer, Inc."		/* should fail */
#define ICHAT_ORG_BAD	"Apple Computer, Inc"

/* commonName = name, orgUnit = domain, case insensitive */
#define ICHAT_NAME			"Alice"					/* commonName */
#define ICHAT_DOMAIN		"mac.com"				/* orgUnit */
#define ICHAT_HANDLE		"Alice@mac.com"			/* what we pass in */

#define ICHAT_HANDLE_CASE1	"ALice@mac.com"
#define ICHAT_HANDLE_CASE2	"Alice@mAc.com"
#define ICHAT_NAME_CASE		"ALice"
#define ICHAT_DOMAIN_CASE	"maC.com"
#define ICHAT_NAME_BAD		"Alice_"
#define ICHAT_DOMAIN_BAD	"mac.co"
#define ICHAT_HANDLE_BAD1	"Alice@mac.co"
#define ICHAT_HANDLE_BAD2	"Alicg@mac.com"
#define ICHAT_HANDLE_BAD3	"Alicemac.com"
#define ICHAT_HANDLE_BAD4	"Alice@mac@com"

#define KEYUSE_NONE		0

SP_TestCase testCases[] = 
{
	/* move these to end after we debug 'em */
	/* custom iChat name encoding */
	{
		"iChat custom",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_APPLE_EKU_ICHAT_SIGNING,
		NULL, NULL,
		ICHAT_DOMAIN, ICHAT_ORG
	},
	{
		"iChat custom",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		/* verify this EKU */
		&CSSMOID_APPLE_EKU_ICHAT_ENCRYPTION,
		NULL, NULL,
		ICHAT_DOMAIN, ICHAT_ORG
	},
	{
		"iChat custom",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		/* verify this EKU */
		&CSSMOID_ExtendedKeyUsageAny,
		NULL, NULL,
		ICHAT_DOMAIN, ICHAT_ORG
	},
	{
		"iChat custom, alt case in name",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME_CASE, ICHAT_HANDLE,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_APPLE_EKU_ICHAT_ENCRYPTION,	
		NULL, NULL,
		ICHAT_DOMAIN, ICHAT_ORG
	},
	{
		"iChat custom, alt case in domain",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_APPLE_EKU_ICHAT_SIGNING,
		NULL, NULL,
		ICHAT_DOMAIN_CASE, ICHAT_ORG
	},
	{
		"iChat custom, alt case in org, expect fail",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_APPLE_EKU_ICHAT_ENCRYPTION,	
		"CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS", 
		"0:CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS",
		ICHAT_DOMAIN, ICHAT_ORG_CASE
	},
	{
		"iChat custom, bad name",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME_BAD, ICHAT_HANDLE,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_APPLE_EKU_ICHAT_SIGNING,
		"CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS", 
		"0:CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS",
		ICHAT_DOMAIN, ICHAT_ORG
	},
	{
		"iChat custom, bad name",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME_BAD, ICHAT_HANDLE,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_APPLE_EKU_ICHAT_ENCRYPTION,	
		"CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS", 
		"0:CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS",
		ICHAT_DOMAIN, ICHAT_ORG
	},
	{
		"iChat custom, bad domain",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_ExtendedKeyUsageAny,	
		"CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS", 
		"0:CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS",
		ICHAT_DOMAIN_BAD, ICHAT_ORG
	},
	{
		"iChat custom, bad org",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_APPLE_EKU_ICHAT_ENCRYPTION,	
		"CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS", 
		"0:CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS",
		ICHAT_DOMAIN, ICHAT_ORG_BAD
	},
	{
		"iChat custom, bad handle (short)",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE_BAD1,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_APPLE_EKU_ICHAT_ENCRYPTION,	
		"CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS", 
		"0:CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS",
		ICHAT_DOMAIN, ICHAT_ORG
	},
	{
		"iChat custom, bad handle (bad name component)",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE_BAD2,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_APPLE_EKU_ICHAT_ENCRYPTION,	
		"CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS", 
		"0:CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS",
		ICHAT_DOMAIN, ICHAT_ORG
	},
	{
		"iChat custom, bad handle (no @)",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE_BAD3,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_APPLE_EKU_ICHAT_ENCRYPTION,	
		"CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS", 
		"0:CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS",
		ICHAT_DOMAIN, ICHAT_ORG
	},
	{
		"iChat custom, bad handle (two @s)",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE_BAD4,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_APPLE_EKU_ICHAT_ENCRYPTION,	
		"CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS", 
		"0:CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS",
		ICHAT_DOMAIN, ICHAT_ORG
	},
	{
		"iChat custom, no EKU",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		"CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE",
		"0:CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE",
		ICHAT_DOMAIN, ICHAT_ORG
	},
	{
		"iChat custom, bad EKU",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_TimeStamping,
		"CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE",
		"0:CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE",
		ICHAT_DOMAIN, ICHAT_ORG
	},
	
	/* end of move to end */
	{
		"Email addrs in subjAltName, S/MIME",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,				// ekuOid
		NULL, NULL
	},
	{
		"Email addrs in subjAltName, iChat",
		CVP_iChat,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		NULL, NULL
	},
	{
		"Email addrs in subject name, S/MIME",
		CVP_SMIME,
		GOOD_EMAIL, NULL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		NULL, NULL
	},
	{
		"Email addrs in subject name, iChat",
		CVP_iChat,
		GOOD_EMAIL, NULL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		NULL, NULL
	},
	{
		/*
		 * The spec (RFC 2632) says that if there is no email address
		 * in the cert at all, then the TP doesn't have to 
		 * enforce a match. So this one works, since we're not
		 * putting any email addresses in the cert.
		 */
		"No Email addrs, S/MIME",
		CVP_SMIME,
		NULL, NULL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		NULL, NULL
	},
	{
		/*
		 * For iChat the same scenario fails; it requires an email address in
		 * the cert if we pass one in.
		 */
		"No Email addrs, iChat",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		"CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS", 
		"0:CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS"
	},
	{
		"Wrong email addrs in SubjectName, S/MIME",
		CVP_SMIME,
		BAD_EMAIL, NULL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		"CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND", 
		"0:CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND"
	},
	{
		"Wrong email addrs in SubjectName, iChat",
		CVP_iChat,
		BAD_EMAIL, NULL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		"CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND", 
		"0:CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND"
	},
	{
		"Wrong email addrs in SubjectAltName, S/MIME",
		CVP_SMIME,
		NULL, BAD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		"CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND", 
		"0:CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND"
	},
	{
		"Wrong email addrs in SubjectAltName, iChat",
		CVP_iChat,
		NULL, BAD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		"CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND", 
		"0:CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND"
	},
	
	/*
	 * The presence of an incorrect email address is not supposed
	 * be an error as long as the right one is in there somewhere.
	 */
	{
		"Wrong addrs in subj name, correct in subjAltName, S/MIME",
		CVP_SMIME,
		BAD_EMAIL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		NULL, NULL
	},
	{
		"Wrong addrs in subj name, correct in subjAltName, iChat",
		CVP_iChat,
		BAD_EMAIL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		NULL, NULL
	},
	{
		"Wrong addrs in subjAltname, correct in subjName, S/MIME",
		CVP_SMIME,
		GOOD_EMAIL, BAD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		NULL, NULL
	},
	{
		"Wrong addrs in subjAltname, correct in subjName, iChat",
		CVP_iChat,
		GOOD_EMAIL, BAD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		NULL, NULL
	},
	/* empty subject name processing - S/MIME only*/
	{
		"Empty subj name, correct subjAltName, no search",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_TRUE,
		NULL, NULL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		NULL, NULL
	},
	{
		"Empty subj name, correct subjAltName, good search",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_TRUE,
		NULL, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		NULL, NULL
	},
	{
		"Empty subj name, correct subjAltName, not critical, no search",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		NULL, NULL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		"CSSMERR_TP_VERIFY_ACTION_FAILED", 
		"0:CSSMERR_APPLETP_SMIME_SUBJ_ALT_NAME_NOT_CRIT"
	},
	{
		"Empty subj name, correct subjAltName, not critical, good search",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		NULL, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		"CSSMERR_TP_VERIFY_ACTION_FAILED", 
		"0:CSSMERR_APPLETP_SMIME_SUBJ_ALT_NAME_NOT_CRIT"
	},
	{
		"Empty subj name, empty subjAltName, no search",
		CVP_SMIME,
		NULL, NULL, CSSM_FALSE,
		NULL, NULL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		"CSSMERR_TP_VERIFY_ACTION_FAILED", 
		"0:CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS"
	},
	
	/* case sensitivity handling */
	{
		"Different case domain in subjAltName, S/MIME",
		CVP_SMIME,
		NULL, CASE_EMAIL_GOOD, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,				// ekuOid
		NULL, NULL
	},
	{
		"Different case domain in subjAltName, iChat",
		CVP_iChat,
		NULL, CASE_EMAIL_GOOD, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		NULL, NULL
	},
	{
		"Different case domain in subjectName, S/MIME",
		CVP_SMIME,
		CASE_EMAIL_GOOD, NULL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		NULL, NULL
	},
	{
		"Different case domain in subjectName, iChat",
		CVP_iChat,
		CASE_EMAIL_GOOD, NULL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		NULL, NULL
	},
	/* 
	 * local-part: 
	 *		SMIME, emailAddrs in SubjectName - case insensitive
	 *		SMIME, emailAddrs in SubjectAltName - case sensitive
	 *		iChat, anywhere - case insensitive
	 */
	{
		/* the only time local-part is case sensitive */
		"Different case local-part in subjAltName, SMIME",
		CVP_SMIME,
		NULL, CASE_EMAIL_BAD, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		"CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND", 
		"0:CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND"
	},
	{
		"Different case local-part in subjAltName, iChat",
		CVP_iChat,
		NULL, CASE_EMAIL_BAD, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		NULL, NULL
	},
	{
		"Different case local-part in SubjectName, S/MIME",
		CVP_SMIME,
		CASE_EMAIL_BAD, NULL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		NULL, NULL
	},
	{
		"Different case local-part in SubjectName, iChat",
		CVP_iChat,
		CASE_EMAIL_BAD, NULL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		NULL, NULL
	},
	{
		/* just to test a corner case of tpNormalizeAddrSpec */
		"local-part missing @ in SubjectName",
		CVP_SMIME,
		"alice_apple.com", NULL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		NULL,
		"CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND", 
		"0:CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND"
	},

	/*** 
	 *** Key Usage testing 
	 ***
	 *** All leaf certs have good email addrs in subjectAltName
	 ***/
	{
		"Key Usage good 1",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		CE_KU_DigitalSignature, CSSM_TRUE, CE_KU_DigitalSignature,
		NULL,
		NULL, NULL
	},
	{
		"Key Usage good 2",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		CE_KU_DigitalSignature | CE_KU_NonRepudiation, CSSM_TRUE, 
		CE_KU_DigitalSignature,
		NULL,
		NULL, NULL
	},
	{
		"KeyUsage in cert but not verified",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		CE_KU_DigitalSignature | CE_KU_NonRepudiation, CSSM_TRUE, 
		KEYUSE_NONE,			// no use specified
		NULL,
		NULL, NULL
	},
	{
		"Key Usage bad 1",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		CE_KU_DigitalSignature, CSSM_TRUE, CE_KU_NonRepudiation,
		NULL,
		"CSSMERR_TP_VERIFY_ACTION_FAILED", 
		"0:CSSMERR_APPLETP_SMIME_BAD_KEY_USE"
	},
	{
		"Key Usage bad 2",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		CE_KU_DigitalSignature | CE_KU_NonRepudiation, CSSM_TRUE, 
		CE_KU_CRLSign,
		NULL,
		"CSSMERR_TP_VERIFY_ACTION_FAILED", 
		"0:CSSMERR_APPLETP_SMIME_BAD_KEY_USE"
	},
	#if 0
	/* obsolete per radar 3523221 */
	{
		"Key Usage not critical",
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		CE_KU_DigitalSignature, CSSM_FALSE, CE_KU_DigitalSignature,
		NULL,
		"CSSMERR_TP_VERIFY_ACTION_FAILED", 
		"0:CSSMERR_APPLETP_SMIME_KEYUSAGE_NOT_CRITICAL"
	},
	#endif
	/*
	 * The tricky ones involving KeyAgreement
	 */
	{
		"Key Usage KeyAgreement good 1",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		CE_KU_KeyAgreement | CE_KU_EncipherOnly, CSSM_TRUE, 
		CE_KU_KeyAgreement | CE_KU_EncipherOnly,
		NULL,
		NULL, NULL
	},
	{
		"Key Usage KeyAgreement good 2",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		CE_KU_KeyAgreement | CE_KU_DecipherOnly, CSSM_TRUE, 
		CE_KU_KeyAgreement | CE_KU_DecipherOnly,
		NULL,
		NULL, NULL
	},
	{
		"Key Usage KeyAgreement no {En,De}CipherOnly",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		CE_KU_KeyAgreement | CE_KU_DecipherOnly, CSSM_TRUE, 
		CE_KU_KeyAgreement,
		NULL,
		"CSSMERR_TP_VERIFY_ACTION_FAILED", 
		"0:CSSMERR_APPLETP_SMIME_BAD_KEY_USE"
	},
	{
		"Key Usage KeyAgreement bad EncipherOnly",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		CE_KU_KeyAgreement | CE_KU_DecipherOnly, CSSM_TRUE, 
		CE_KU_KeyAgreement | CE_KU_EncipherOnly,
		NULL,
		"CSSMERR_TP_VERIFY_ACTION_FAILED", 
		"0:CSSMERR_APPLETP_SMIME_BAD_KEY_USE"
	},
	{
		"Key Usage KeyAgreement bad DecipherOnly",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		CE_KU_KeyAgreement | CE_KU_EncipherOnly, CSSM_TRUE, 
		CE_KU_KeyAgreement | CE_KU_DecipherOnly,
		NULL,
		"CSSMERR_TP_VERIFY_ACTION_FAILED", 
		"0:CSSMERR_APPLETP_SMIME_BAD_KEY_USE"
	},

	/* Extended Key Usage tests */
	{
		"Extended Key Usage EmailProtection, S/MIME",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		NULL, NULL
	},
	{
		"Extended Key Usage EmailProtection, iChat",
		CVP_iChat,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_EmailProtection,
		NULL, NULL
	},
	{
		"Extended Key Usage ExtendedKeyUsageAny, S/MIME",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_ExtendedKeyUsageAny,
		NULL, NULL
	},
	{
		"Extended Key Usage ExtendedKeyUsageAny, iChat",
		CVP_iChat,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_ExtendedKeyUsageAny,
		NULL, NULL
	},
	{
		"Extended Key Usage TimeStamping (bad), S/MIME",
		CVP_SMIME,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_TimeStamping,
		"CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE", 
		"0:CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE"
	},
	{
		"Extended Key Usage TimeStamping (bad), iChat",
		CVP_iChat,
		NULL, GOOD_EMAIL, CSSM_FALSE,
		COMMON_NAME, GOOD_EMAIL,
		KEYUSE_NONE, CSSM_FALSE, KEYUSE_NONE,
		&CSSMOID_TimeStamping,
		"CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE", 
		"0:CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE"
	},
	{
		"iChat custom, missing ICHAT_SIGNING EKU",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE,
		CE_KU_DigitalSignature, CSSM_TRUE, CE_KU_DigitalSignature,
		NULL,		// requires &CSSMOID_APPLE_EKU_ICHAT_SIGNING,
		"CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE",
		"0:CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE",
		ICHAT_DOMAIN, ICHAT_ORG
	},
	{
		"iChat custom, missing ICHAT_ENCRYPTION EKU",
		CVP_iChat,
		NULL, NULL, CSSM_FALSE,		// no email address
		ICHAT_NAME, ICHAT_HANDLE,
		CE_KU_DataEncipherment, CSSM_TRUE, CE_KU_DataEncipherment,
		NULL,		// requires &CSSMOID_APPLE_EKU_ICHAT_ENCRYPTION,
		"CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE",
		"0:CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE",
		ICHAT_DOMAIN, ICHAT_ORG
	},
};

#define NUM_TEST_CASES	(sizeof(testCases) / sizeof(SP_TestCase))

/*
 * Generate a pair of certs. Root is standard, we never mess with 
 * it.
 */
static CSSM_RETURN genCerts(
	CSSM_CL_HANDLE	clHand,
	CSSM_CSP_HANDLE	cspHand,	
	CSSM_TP_HANDLE	tpHand,	
	CSSM_KEY_PTR	rootPrivKey,
	CSSM_KEY_PTR	rootPubKey,
	CSSM_KEY_PTR	subjPubKey,
	
	/* all five SubjectName components are optional */
	const char		*subjNameEmail,		// CSSMOID_EmailAddress
	const char		*subjAltNameEmail,	// RFC822Name
	const char		*subjNameCommon,
	CSSM_BOOL		subjAltNameCritical,
	const char		*subjOrgUnit,
	const char		*subjOrgName,
	
	/* Key Usage - if zero, no KU extension */
	CE_KeyUsage		certKeyUse,
	CSSM_BOOL		keyUseCritical,
	
	/* ExtendedKeyUSage OID - if NULL, no EKU extension */
	const CSSM_OID	*ekuOid,

	CSSM_DATA		&rootCert,		// RETURNED
	CSSM_DATA		&subjCert)		// RETURNED
	
{
	CSSM_DATA					refId;	
								// mallocd by CSSM_TP_SubmitCredRequest
	CSSM_RETURN					crtn;
	CSSM_APPLE_TP_CERT_REQUEST	certReq;
	CSSM_TP_REQUEST_SET			reqSet;
	sint32						estTime;
	CSSM_BOOL					confirmRequired;
	CSSM_TP_RESULT_SET_PTR		resultSet;
	CSSM_ENCODED_CERT			*encCert;
	CSSM_TP_CALLERAUTH_CONTEXT 	CallerAuthContext;
	CSSM_FIELD					policyId;
	CE_GeneralNames				genNames;
	CE_GeneralName				genName;
	CE_ExtendedKeyUsage			extendedKeyUse;
	
	/* 
	 * Subject has zero, one, two, or three extensions (KeyUsage, 
	 * subjectAltName, extendedKeyUsage); root has two: KeyUsage and 
	 * BasicConstraints.
	 */
	CE_DataAndType 				rootExts[2];
	CE_DataAndType 				leafExts[3];
	unsigned					numLeafExts = 0;
	
	/* 
	 * root extensions
	 * 1. KeyUSage
	 */
	rootExts[0].type = DT_KeyUsage;
	rootExts[0].critical = CSSM_TRUE;
	rootExts[0].extension.keyUsage = 
		CE_KU_DigitalSignature | CE_KU_KeyCertSign;

	/* 
	 * 2. BasicConstraints
	 */
	rootExts[1].type = DT_BasicConstraints;
	rootExts[1].critical = CSSM_TRUE;
	rootExts[1].extension.basicConstraints.cA = CSSM_TRUE;
	rootExts[1].extension.basicConstraints.pathLenConstraintPresent = 
			CSSM_TRUE;
	rootExts[1].extension.basicConstraints.pathLenConstraint = 2;

	/*
	 * Leaf extensions - all are optional
	 */
	if(certKeyUse) {
		leafExts[0].type = DT_KeyUsage;
		leafExts[0].critical = keyUseCritical;
		leafExts[0].extension.keyUsage =  certKeyUse;
		numLeafExts++;
	}
	if(subjAltNameEmail) {
		leafExts[numLeafExts].type = DT_SubjectAltName;
		leafExts[numLeafExts].critical = subjAltNameCritical;
		
		genName.berEncoded = CSSM_FALSE;
		genName.name.Data = (uint8 *)subjAltNameEmail;
		genName.nameType = GNT_RFC822Name;
		genName.name.Length = strlen(subjAltNameEmail);

		genNames.numNames = 1;
		genNames.generalName = &genName;
		leafExts[numLeafExts].extension.subjectAltName = genNames;
		numLeafExts++;
	}
	if(ekuOid) {
		leafExts[numLeafExts].type = DT_ExtendedKeyUsage;
		leafExts[numLeafExts].critical = CSSM_FALSE;		// don't care - right?
		extendedKeyUse.numPurposes = 1;
		extendedKeyUse.purposes = (CSSM_OID_PTR)ekuOid;
		leafExts[numLeafExts].extension.extendedKeyUsage = extendedKeyUse;
		numLeafExts++;
	}
	
	/* 
	 * Variable length subject name for leaf
	 */
	CSSM_APPLE_TP_NAME_OID subjRdn[4];
	unsigned numSubjNames = 0;
	if(subjNameEmail) {
		subjRdn[0].string = subjNameEmail;
		subjRdn[0].oid = &CSSMOID_EmailAddress;
		numSubjNames++;
	}
	if(subjNameCommon) {
		subjRdn[numSubjNames].string = subjNameCommon;
		subjRdn[numSubjNames].oid = &CSSMOID_CommonName;
		numSubjNames++;
	}
	if(subjOrgUnit) {
		subjRdn[numSubjNames].string = subjOrgUnit;
		subjRdn[numSubjNames].oid = &CSSMOID_OrganizationalUnitName;
		numSubjNames++;
	}
	if(subjOrgName) {
		subjRdn[numSubjNames].string = subjOrgName;
		subjRdn[numSubjNames].oid = &CSSMOID_OrganizationName;
		numSubjNames++;
	}
	
	/* certReq for root */
	memset(&certReq, 0, sizeof(CSSM_APPLE_TP_CERT_REQUEST));
	certReq.cspHand = cspHand;
	certReq.clHand = clHand;
	certReq.serialNumber = 0x12345678;
	certReq.numSubjectNames = NUM_ROOT_NAMES;
	certReq.subjectNames = rootRdn;
	certReq.numIssuerNames = 0;
	certReq.issuerNames = NULL;
	certReq.certPublicKey = rootPubKey;
	certReq.issuerPrivateKey = rootPrivKey;
	certReq.signatureAlg = SIG_ALG_DEFAULT;
	certReq.signatureOid = SIG_OID_DEFAULT;
	certReq.notBefore = 0;			// now
	certReq.notAfter = 10000;		// seconds from now
	certReq.numExtensions = 2;
	certReq.extensions = rootExts;
	
	reqSet.NumberOfRequests = 1;
	reqSet.Requests = &certReq;
	
	/* a big CSSM_TP_CALLERAUTH_CONTEXT just to specify an OID */
	memset(&CallerAuthContext, 0, sizeof(CSSM_TP_CALLERAUTH_CONTEXT));
	memset(&policyId, 0, sizeof(CSSM_FIELD));
	policyId.FieldOid = CSSMOID_APPLE_TP_LOCAL_CERT_GEN;
	CallerAuthContext.Policy.NumberOfPolicyIds = 1;
	CallerAuthContext.Policy.PolicyIds = &policyId;
	
	/* generate root cert */
	crtn = CSSM_TP_SubmitCredRequest(tpHand,
		NULL,				// PreferredAuthority
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
		&reqSet,
		&CallerAuthContext,	
		&estTime,
		&refId);
	if(crtn) {
		printError("CSSM_TP_SubmitCredRequest", crtn);
		return crtn;
	}
	crtn = CSSM_TP_RetrieveCredResult(tpHand,
		&refId,
		NULL,				// CallerAuthCredentials
		&estTime,
		&confirmRequired,
		&resultSet);
	if(crtn) {
		printError("CSSM_TP_RetrieveCredResult", crtn);
		return crtn;
	}
	if(resultSet == NULL) {
		printf("***CSSM_TP_RetrieveCredResult returned NULL result set.\n");
		return crtn;
	}
	encCert = (CSSM_ENCODED_CERT *)resultSet->Results;
	rootCert = encCert->CertBlob;
	CSSM_FREE(encCert);
	CSSM_FREE(resultSet);
	
	/* now a subject cert signed by the root cert */
	certReq.serialNumber = 0x8765;
	certReq.numSubjectNames = numSubjNames;
	certReq.subjectNames = subjRdn;
	certReq.numIssuerNames = NUM_ROOT_NAMES;
	certReq.issuerNames = rootRdn;
	certReq.certPublicKey = subjPubKey;
	certReq.issuerPrivateKey = rootPrivKey;
	certReq.numExtensions = numLeafExts;
	certReq.extensions = leafExts;

	crtn = CSSM_TP_SubmitCredRequest(tpHand,
		NULL,				// PreferredAuthority
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
		&reqSet,
		&CallerAuthContext,
		&estTime,
		&refId);
	if(crtn) {
		printError("CSSM_TP_SubmitCredRequest (2)", crtn);
		return crtn;
	}
	crtn = CSSM_TP_RetrieveCredResult(tpHand,
		&refId,
		NULL,				// CallerAuthCredentials
		&estTime,
		&confirmRequired,
		&resultSet);		// leaks.....
	if(crtn) {
		printError("CSSM_TP_RetrieveCredResult (2)", crtn);
		return crtn;
	}
	if(resultSet == NULL) {
		printf("***CSSM_TP_RetrieveCredResult (2) returned NULL "
				"result set.\n");
		return crtn;
	}
	encCert = (CSSM_ENCODED_CERT *)resultSet->Results;
	subjCert = encCert->CertBlob;
	CSSM_FREE(encCert);
	CSSM_FREE(resultSet);

	return CSSM_OK;
}

static void copyToBlob(
	const CSSM_DATA &src, 
	CSSM_DATA		&blob)
{
	blob.Data = (uint8 *)malloc(src.Length);
	blob.Length = src.Length;
	memmove(blob.Data, src.Data, src.Length);
}

int main(int argc, char **argv)
{
	CSSM_CL_HANDLE	clHand;			// CL handle
	CSSM_CSP_HANDLE	cspHand;		// CSP handle
	CSSM_TP_HANDLE	tpHand;			// TP handle
	CSSM_DATA		rootCert;		
	CSSM_DATA		subjCert;	
	CSSM_KEY		subjPubKey;		// subject's RSA public key blob
	CSSM_KEY		subjPrivKey;	// subject's RSA private key - ref format
	CSSM_KEY		rootPubKey;		// root's RSA public key blob
	CSSM_KEY		rootPrivKey;	// root's RSA private key - ref format
	CSSM_RETURN		crtn = CSSM_OK;
	int				vfyRtn = 0;
	int				arg;
	SP_TestCase		*testCase;
	unsigned		testNum;
	
	CSSM_BOOL		printLeafCerts = CSSM_FALSE;
	CSSM_BOOL		quiet = CSSM_FALSE;
	CSSM_BOOL		verbose = CSSM_FALSE;
	CSSM_BOOL		doPause = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) {
		char *argp = argv[arg];
		switch(argp[0]) {
			case 'p':
				printLeafCerts = CSSM_TRUE;
				break;
			case 'P':
				doPause = CSSM_TRUE;
				break;
			case 'q':
				quiet = CSSM_TRUE;
				break;
			case 'v':
				verbose = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}
	
	testStartBanner("smimePolicy", argc, argv);
	
	/* connect to CL, TP, and CSP */
	clHand = clStartup();
	if(clHand == 0) {
		return 0;
	}
	tpHand = tpStartup();
	if(tpHand == 0) {
		return 0;
	}
	cspHand = cspStartup();
	if(cspHand == 0) {
		return 0;
	}

	/* subsequent errors to abort: to detach */
	
	/* cook up an RSA key pair for the subject */
	crtn = cspGenKeyPair(cspHand,
		KEY_ALG_DEFAULT,
		SUBJ_KEY_LABEL,
		strlen(SUBJ_KEY_LABEL),
		KEY_SIZE_DEFAULT,
		&subjPubKey,
		CSSM_FALSE,			// pubIsRef
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&subjPrivKey,
		CSSM_TRUE,			// privIsRef - doesn't matter
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		return crtn;
	}

	/* and the root */
	crtn = cspGenKeyPair(cspHand,
		KEY_ALG_DEFAULT,
		ROOT_KEY_LABEL,
		strlen(ROOT_KEY_LABEL),
		KEY_SIZE_DEFAULT,
		&rootPubKey,
		CSSM_FALSE,			// pubIsRef
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&rootPrivKey,
		CSSM_TRUE,			// privIsRef - doesn't matter
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		goto abort;
	}

	for(testNum=0; testNum<NUM_TEST_CASES; testNum++) {
		testCase = &testCases[testNum];
		if(!quiet) {
			printf("%s\n", testCase->testDesc);
		}
		crtn = genCerts(clHand, cspHand, tpHand,
			&rootPrivKey, &rootPubKey, &subjPubKey,
			testCase->subjNameEmail, testCase->subjAltNameEmail, 
			testCase->subjNameCommon, testCase->subjAltNameCritical,
			testCase->orgUnit, testCase->orgName,
			testCase->certKeyUse, testCase->keyUseCritical, testCase->ekuOid,
			rootCert, subjCert);
		if(printLeafCerts) {
			printCert(subjCert.Data, subjCert.Length, CSSM_FALSE);
		}
		BlobList leaf;
		BlobList root;
		/* BlobList uses regular free() on the referent of the blobs */
		CSSM_DATA blob;
		copyToBlob(subjCert, blob);
		CSSM_FREE(subjCert.Data);
		leaf.addBlob(blob);
		copyToBlob(rootCert, blob);
		CSSM_FREE(rootCert.Data);
		root.addBlob(blob);
		if(crtn) {
			if(testError(quiet)) {
				break;
			}
		}
		vfyRtn = certVerifySimple(tpHand, clHand, cspHand,
			leaf, root, 
			CSSM_FALSE,		// useSystemAnchors
			CSSM_FALSE,		// leafCertIsCA
			CSSM_FALSE,		// allow expired root
			testCase->policy,
			NULL,			// sslHost
			CSSM_FALSE,		// sslClient
			testCase->vfyEmailAddrs,
			testCase->vfyKeyUse,
			testCase->expectErrStr,
			testCase->certErrorStr ? 1 : 0,
			testCase->certErrorStr ? (const char **)&testCase->certErrorStr :
				NULL,
			0, NULL,		// certStatus
			CSSM_FALSE,		// trustSettings
			quiet,
			verbose);
		if(vfyRtn) {
			if(testError(quiet)) {
				break;
			}
		}
		/* cert data freed by ~BlobList */
		
		if(doPause) {
			fpurge(stdin);
			printf("Pausing for mallocDebug. CR to continue: ");
			getchar();
		}
	}

	/* free keys */
	cspFreeKey(cspHand, &rootPubKey);
	cspFreeKey(cspHand, &rootPrivKey);
	cspFreeKey(cspHand, &subjPubKey);
	cspFreeKey(cspHand, &subjPrivKey);

abort:
	if(cspHand != 0) {
		CSSM_ModuleDetach(cspHand);
	}
	if(clHand != 0) {
		CSSM_ModuleDetach(clHand);
	}
	if(tpHand != 0) {
		CSSM_ModuleDetach(tpHand);
	}
	if(!vfyRtn && !crtn && !quiet) {
		printf("...test passed\n");
	}
	return 0;
}


