/* 
 * dotMacRequest.cpp - simple illustration of using SecCertificateRequestCreate() and
 *				       SecCertificateRequestSubmit to post a request for a .mac cert. 
 */
#include <Security/Security.h>
#include <Security/SecCertificateRequest.h>
#include <security_dotmac_tp/dotMacTp.h>
#include <clAppUtils/keyPicker.h>
#include <unistd.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

/*
 * Defaults for the test setup du jour 
 */
#define USER_DEFAULT		"dmitch10"
#define PWD_DEFAULT			"password"
#define HOST_DEFAULT		"certmgmt.mac.com"

/* 
 * Type of cert to request
 */
typedef enum {
	CRT_Identity,		/* actually, now "iChat encryption", not "identity" */
	CRT_EmailSign,
	CRT_EmailEncrypt
} CertRequestType;

static void usage(char **argv)
{
	printf("usage: %s op [options]\n", argv[0]);
	printf("Op:\n");
	printf("    i              -- generate iChat encryption cert\n");
	printf("    s              -- generate email signing cert\n");
	printf("    e              -- generate email encrypting cert\n");
	printf("    I              -- search/retrieve request for iChat encryption cert\n");
	printf("    S              -- search/retrieve request for signing cert\n");
	printf("    E              -- search/retrieve request for encrypting cert\n");
	printf("    p              -- pending request poll (via -u)\n");
	printf("Options:\n");
	printf("   -u username     -- Default is %s\n", USER_DEFAULT);
	printf("   -Z password     -- default is %s\n", PWD_DEFAULT);
	printf("   -p              -- pick key pair from existing (default is generate)\n");
	printf("   -k keychain     -- Source/destination of keys\n");
	printf("   -r              -- Renew (default is new)\n");
	printf("   -a              -- async (default is synchronous)\n");
	printf("   -H hostname     -- Alternate .mac server host name (default %s)\n",
									HOST_DEFAULT);
	printf("   -M              -- Pause for MallocDebug\n");
	printf("   -l              -- loop\n");
	exit(1);
}

/* print a string int he form of a CSSM_DATA */
static void printString(
	const CSSM_DATA *str)
{
	for(unsigned dex=0; dex<str->Length; dex++) {
		printf("%c", str->Data[dex]);
	}
}

/* basic "generate keypair" routine */
static OSStatus genKeyPair(
	SecKeychainRef  kcRef,		// optional, NULL means the default list
	SecKeyRef		*pubKey,	// RETURNED
	SecKeyRef		*privKey)   // RETURNED
{
	OSStatus ortn;
	
	ortn = SecKeyCreatePair(kcRef,
		DOT_MAC_KEY_ALG,
		DOT_MAC_KEY_SIZE,
		0,						// context handle
		/* public key usage and attrs */
		CSSM_KEYUSE_ANY,	
		CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE,
		/* private key usage and attrs */
		CSSM_KEYUSE_ANY,	
		CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE |
			CSSM_KEYATTR_SENSITIVE,
		NULL,					// initial access
		pubKey,
		privKey);
	if(ortn) {
		cssmPerror("SecKeyCreatePair", ortn);
	}
	return ortn;
}

/* max number of oid/value pairs */
#define MAX_ATTRS		5

/* 
 * search for a pending .mac cert request, get current status. 
 */
static OSStatus dotMacGetPendingRequest(
	/* required fields */
	const char			*userName,		// REQUIRED, C string
	const char			*password,		// REQUIRED, C string
	CertRequestType		requestType,

	/* optional fields */
	const char			*hostName,		// C string
	SecKeychainRef		keychain)		// destination of created cert (if !async) 
{
	SecCertificateRequestAttribute		attrs[MAX_ATTRS];
	SecCertificateRequestAttribute		*attrp = attrs;
	SecCertificateRequestAttributeList	attrList;

	attrList.count = 0;
	attrList.attr = attrs;
	
	/* user name */
	attrp->oid = CSSMOID_DOTMAC_CERT_REQ_VALUE_USERNAME;
	attrp->value.Data = (uint8 *)userName;
	attrp->value.Length = strlen(userName);
	attrp++;
	attrList.count++;
	
	/* password */
	attrp->oid = CSSMOID_DOTMAC_CERT_REQ_VALUE_PASSWORD;
	attrp->value.Data = (uint8 *)password;
	attrp->value.Length = strlen(password);
	attrp++;
	attrList.count++;
	
	/* options */

	if(hostName) {
		attrp->oid = CSSMOID_DOTMAC_CERT_REQ_VALUE_HOSTNAME;
		attrp->value.Data = (uint8 *)hostName;
		attrp->value.Length = strlen(hostName);
		attrp++;
		attrList.count++;
	}

	/* map CertRequestType to a policy OID */
	const CSSM_OID *policy;
	switch(requestType) {
		case CRT_Identity:
			policy = &CSSMOID_DOTMAC_CERT_REQ_IDENTITY;
			break;
		case CRT_EmailSign:
			policy = &CSSMOID_DOTMAC_CERT_REQ_EMAIL_SIGN;
			break;
		case CRT_EmailEncrypt:
			policy = &CSSMOID_DOTMAC_CERT_REQ_EMAIL_ENCRYPT;
			break;
		default:
			printf("GAK! Bad cert type.\n");
			return -1;
	}
	OSStatus ortn;
	SecCertificateRequestRef certReq = NULL;
	SecCertificateRef certRef = NULL;
	sint32 estTime;
	
	printf("...calling SecCertificateFindRequest\n");
	ortn = SecCertificateFindRequest(policy, 
		CSSM_CERT_X_509v3, 
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
		NULL, NULL,			// no keys needed
		&attrList,
		&certReq);
	if(ortn) {
		cssmPerror("SecCertificateFindRequest", ortn);
		return ortn;
	}
	
	printf("...calling SecCertificateRequestGetResult\n");
	ortn = SecCertificateRequestGetResult(certReq, keychain, &estTime, &certRef);
	if(ortn) {
		cssmPerror("SecCertificateRequestGetResult", ortn);
	}
	else {
		printf("...SecCertificateRequestGetResult succeeded; estTime %d; cert %s\n",
			(int)estTime, certRef ? "OBTAINED" : "NOT OBTAINED");
	}
	if(certRef) {
		CFRelease(certRef);
	}
	if(certReq) {
		CFRelease(certReq);
	}
	return ortn;
}

/* 
 * Do an "is there a pending request for this user?" poll.
 * That function - via SecCertificateFindRequest() always returns an error;
 * *we* only return an error if the result is something other than the
 * expected two results:
 * CSSMERR_APPLE_DOTMAC_REQ_IS_PENDING
 * CSSMERR_APPLE_DOTMAC_NO_REQ_PENDING
 */
static OSStatus dotMacPostPendingReqPoll(
	const char *userName, 
	const char *password, 
	const char *hostName)
{
	SecCertificateRequestAttribute		attrs[MAX_ATTRS];
	SecCertificateRequestAttribute		*attrp = attrs;
	SecCertificateRequestAttributeList	attrList;
	uint8								oneBit = 1;

	attrList.count = 0;
	attrList.attr = attrs;
	
	/* user name, required */
	attrp->oid = CSSMOID_DOTMAC_CERT_REQ_VALUE_USERNAME;
	attrp->value.Data = (uint8 *)userName;
	attrp->value.Length = strlen(userName);
	attrp++;
	attrList.count++;
	
	/* password, required */
	attrp->oid = CSSMOID_DOTMAC_CERT_REQ_VALUE_PASSWORD;
	attrp->value.Data = (uint8 *)password;
	attrp->value.Length = strlen(password);
	attrp++;
	attrList.count++;

	/* the "poll the server" indicator */
	attrp->oid = CSSMOID_DOTMAC_CERT_REQ_VALUE_IS_PENDING;
	/* true ::= any nonzero data  */
	attrp->value.Data = &oneBit;
	attrp->value.Length = 1;
	attrp++;
	attrList.count++;

	/* options */

	if(hostName) {
		attrp->oid = CSSMOID_DOTMAC_CERT_REQ_VALUE_HOSTNAME;
		attrp->value.Data = (uint8 *)hostName;
		attrp->value.Length = strlen(hostName);
		attrp++;
		attrList.count++;
	}

	/* policy, not technically needed; use this one by convention */
	const CSSM_OID *policy = &CSSMOID_DOTMAC_CERT_REQ_IDENTITY;

	OSStatus ortn;
	SecCertificateRequestRef certReq = NULL;
	
	printf("...calling SecCertificateFindRequest\n");
	ortn = SecCertificateFindRequest(policy, 
		CSSM_CERT_X_509v3, 
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
		NULL, NULL,			// no keys needed
		&attrList,
		&certReq);
		
	switch(ortn) {
		case CSSMERR_APPLE_DOTMAC_REQ_IS_PENDING:
			printf("...result: REQ_IS_PENDING\n");
			ortn = noErr;
			break;
		case CSSMERR_APPLE_DOTMAC_NO_REQ_PENDING:
			printf("...result: NO_REQ_PENDING\n");
			ortn = noErr;
			break;
		case noErr:
			/* should never happen */
			printf("...UNEXPECTED SUCCESS on SecCertificateFindRequest\n");
			ortn = internalComponentErr;
			if(certReq != NULL) {
				/* Somehow, it got created */
				CFRelease(certReq);
			}
			break;
		default:
			cssmPerror("SecCertificateFindRequest", ortn);
			break;
	}
	return ortn;
}

/* 
 * Post a .mac cert request, with a small number of options. 
 */
static OSStatus dotMacPostCertRequest(
	/* required fields */
	const char			*userName,		// REQUIRED, C string
	const char			*password,		// REQUIRED, C string
	SecKeyRef			privKey,		// REQUIRED
	SecKeyRef			pubKey,
	CertRequestType		requestType,
	bool				renew,			// false: new cert
										// true : renew existing
	bool				async,			// false: wait for result
										// true : just post request and return
	/* optional fields */
	const char			*hostName,		// C string
	SecKeychainRef		keychain)		// destination of created cert (if !async) 
{

	/* the main job here is bundling up the arguments in an array of OID/value pairs */
	SecCertificateRequestAttribute		attrs[MAX_ATTRS];
	SecCertificateRequestAttribute		*attrp = attrs;
	SecCertificateRequestAttributeList	attrList;
	uint8								oneBit = 1;
	
	attrList.count = 0;
	attrList.attr = attrs;
	
	/* user name */
	attrp->oid = CSSMOID_DOTMAC_CERT_REQ_VALUE_USERNAME;
	attrp->value.Data = (uint8 *)userName;
	attrp->value.Length = strlen(userName);
	attrp++;
	attrList.count++;
	
	/* password */
	attrp->oid = CSSMOID_DOTMAC_CERT_REQ_VALUE_PASSWORD;
	attrp->value.Data = (uint8 *)password;
	attrp->value.Length = strlen(password);
	attrp++;
	attrList.count++;
	
	/* options */

	if(hostName) {
		attrp->oid = CSSMOID_DOTMAC_CERT_REQ_VALUE_HOSTNAME;
		attrp->value.Data = (uint8 *)hostName;
		attrp->value.Length = strlen(hostName);
		attrp++;
		attrList.count++;
	}
	
	if(renew) {
		attrp->oid = CSSMOID_DOTMAC_CERT_REQ_VALUE_RENEW;
		/* true ::= any nonzero data  */
		attrp->value.Data = &oneBit;
		attrp->value.Length = 1;
		attrp++;
		attrList.count++;
	}
	
	if(async) {
		attrp->oid = CSSMOID_DOTMAC_CERT_REQ_VALUE_ASYNC;
		/* true ::= any nonzero data  */
		attrp->value.Data = &oneBit;
		attrp->value.Length = 1;
		attrp++;
		attrList.count++;
	}

	/* map CertRequestType to a policy OID */
	const CSSM_OID *policy;
	switch(requestType) {
		case CRT_Identity:
			policy = &CSSMOID_DOTMAC_CERT_REQ_IDENTITY;
			break;
		case CRT_EmailSign:
			policy = &CSSMOID_DOTMAC_CERT_REQ_EMAIL_SIGN;
			break;
		case CRT_EmailEncrypt:
			policy = &CSSMOID_DOTMAC_CERT_REQ_EMAIL_ENCRYPT;
			break;
		default:
			printf("GAK! Bad cert type.\n");
			return -1;
	}
	OSStatus ortn;
	SecCertificateRequestRef certReq = NULL;
	
	ortn = SecCertificateRequestCreate(policy, 
		CSSM_CERT_X_509v3, 
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
		privKey,
		pubKey,
		&attrList,
		&certReq);
	if(ortn) {
		cssmPerror("SecCertificateRequestCreate", ortn);
		return ortn;
	}
	
	printf("...submitting request to .mac server\n");
	sint32 estTime = 0;
	ortn = SecCertificateRequestSubmit(certReq, &estTime);
	switch(ortn) {
		case CSSMERR_APPLE_DOTMAC_REQ_REDIRECT:
		{
			/* 
			 * A special case; the server is redirecting the calling app to 
			 * a URL which we fetch and report like so:
			 */
			CSSM_DATA url = {0, NULL};
			ortn = SecCertificateRequestGetData(certReq, &url);
			if(ortn) {
				cssmPerror("SecCertificateRequestGetData", ortn);
				printf("***APPLE_DOTMAC_REQ_REDIRECT obtained but no URL availalble.\n");
			}
			else {
				printf("***APPLE_DOTMAC_REQ_REDIRECT obtained; redirect URL is: ");
				printString(&url);
				printf("\n");
			}
			break;
		}
		
		case CSSM_OK:
			printf("...cert request submitted; estimatedTime %d.\n", (int)estTime);
			break;
		default:
			cssmPerror("SecCertificateRequestSubmit", ortn);
			break;
	}
	if(ortn || async) {
		/* we're done */
		CFRelease(certReq);
		return ortn;
	}
	
	/* 
	 * Running synchronously, and the submit succeeded. Try to get a result.
	 * In the real world this would be polled, every so often....
	 */
	SecCertificateRef certRef = NULL;
	printf("...attempting to get result of cert request...\n");
	ortn = SecCertificateRequestGetResult(certReq, keychain, &estTime, &certRef);
	if(ortn) {
		cssmPerror("SecCertificateRequestGetResult", ortn);
	}
	else {
		printf("...SecCertificateRequestGetResult succeeded; estTime %d; cert %s\n",
			(int)estTime, certRef ? "OBTAINED" : "NOT OBTAINED");
	}
	if(certRef) {
		CFRelease(certRef);
		CFRelease(certReq);
	}
	return ortn;
}

#define ALWAYS_DO_SUBMIT		0


int main(int argc, char **argv)
{
	SecKeyRef		pubKeyRef = NULL;
	SecKeyRef		privKeyRef = NULL;
	SecKeychainRef	kcRef = NULL;
	OSStatus		ortn;
	
	/* user-spec'd variables */
	bool			genKeys = true;			/* true: generate; false: pick 'em */
	char			*keychainName = NULL;
	char			*userName = USER_DEFAULT;
	char			*password = PWD_DEFAULT;
	char			*hostName = NULL;		/* leave as the default! = HOST_DEFAULT; */	
	/* 
	 * WARNING: doing a renew operation requires that you delete your *current* 
	 * .mac cert from the destination keychain. The DB attrs of the old and new certs
	 * are the same!
	 */
	bool			doRenew = false;
	CertRequestType reqType = CRT_Identity;
	bool			doPause = false;
	bool			async = false;
	bool			doSearch = false;		/* false: post cert request 
											 * true : search for existing request, get 
											 *   status for it */
	bool			loop = false;
	bool			doPendingReqPoll = false;

	if(argc < 2) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'i':
			reqType = CRT_Identity;
			break;
		case 's':
			reqType = CRT_EmailSign;
			break;
		case 'e':
			reqType = CRT_EmailEncrypt;
			break;
		case 'I':
			doSearch = true;
			reqType = CRT_Identity;
			break;
		case 'S':
			doSearch = true;
			reqType = CRT_EmailSign;
			break;
		case 'E':
			doSearch = true;
			reqType = CRT_EmailEncrypt;
			break;
		case 'p':
			doPendingReqPoll = true;
			break;
		default:
			usage(argv);
	}

	extern char *optarg;
	extern int optind;
	optind = 2;
	int arg;
	while ((arg = getopt(argc, argv, "u:Z:pk:rMH:al")) != -1) {
		switch (arg) {
			case 'u':
				userName = optarg;
				break;
			case 'Z':
				password = optarg;
				break;
			case 'p':
				genKeys = false;
				break;
			case 'k':
				keychainName = optarg;
				break;
			case 'r':
				doRenew = true;
				break;
			case 'M':
				doPause = true;
				break;
			case 'H':
				hostName = optarg;
				break;
			case 'a':
				async = true;
				break;
			case 'l':
				loop = true;
				break;
			case 'h':
			default:
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	
	if(doPause) {
		fpurge(stdin);
		printf("Pausing for MallocDebug attach; CR to continue: ");
		getchar();
	}
	
	if(keychainName != NULL) {
		/* pick a keychain (optional) */
		ortn = SecKeychainOpen(keychainName, &kcRef);
		if(ortn) {
			cssmPerror("SecKeychainOpen", ortn);
			exit(1);
		}
		
		/* make sure it's there since a successful SecKeychainOpen proves nothing */
		SecKeychainStatus kcStat;
		ortn = SecKeychainGetStatus(kcRef, &kcStat);
		if(ortn) {
			cssmPerror("SecKeychainGetStatus", ortn);
			exit(1);
		}
	}
	
	if((!doSearch || ALWAYS_DO_SUBMIT) && !doPendingReqPoll) {
		/* get a key pair, somehow */
		if(genKeys) {
			ortn = genKeyPair(kcRef, &pubKeyRef, &privKeyRef);
		}
		else {
			ortn = keyPicker(kcRef, &pubKeyRef, &privKeyRef);
		}
		if(ortn) {
			printf("Can't proceed without a keypair. Aborting.\n");
			exit(1);
		}
	}
	
	/* go */
	do {
		if(doSearch) {
			#if ALWAYS_DO_SUBMIT
			/* debug only */
			dotMacPostCertRequest(userName, password, privKeyRef, pubKeyRef,
				reqType, doRenew, async, hostName, kcRef);
			#endif
			
			/* end */
			ortn = dotMacGetPendingRequest(userName, password, reqType, hostName, kcRef);
		}
		else if(doPendingReqPoll) {
			ortn = dotMacPostPendingReqPoll(userName, password, hostName);
		}
		else {
			ortn = dotMacPostCertRequest(userName, password, privKeyRef, pubKeyRef,
				reqType, doRenew, async, hostName, kcRef);
		}
		if(doPause) {
			fpurge(stdin);
			printf("Pausing for MallocDebug attach; CR to continue: ");
			getchar();
		}
	} while(loop);
	if(privKeyRef) {
		CFRelease(privKeyRef);
	}
	if(pubKeyRef) {
		CFRelease(pubKeyRef);
	}
	if(kcRef) {
		CFRelease(kcRef);
	}

	if(doPause) {
		fpurge(stdin);
		printf("Pausing at end of test for MallocDebug attach; CR to continue: ");
		getchar();
	}

	return ortn;
}

