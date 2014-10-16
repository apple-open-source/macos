/*
 * dotMacTool.cpp - .mac TP exerciser
 */
 
#include <Security/Security.h>
#include <Security/SecKeyPriv.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_cdsa_utils/cuPem.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
//#include <security_dotmac_tp/dotMacTp.h>
#include <dotMacTp.h>
#include <security_cdsa_utils/cuPrintCert.h>

#include "keyPicker.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#define USER_DEFAULT		"dmitchtest@mac.com"
#define PWD_DEF				"123456"

static void usage(char **argv)
{
	printf("usage: %s op [options]\n", argv[0]);
	printf("Op:\n");
	printf("    g                generate identity cert\n");
	printf("    G                generate email signing cert\n");
	printf("    e                generate email encrypting cert\n");
	printf("    l                lookup cert (requires -f)\n");
	printf("    L                lookup identity cert (via -u)\n");
	printf("    M                lookup email signing cert (via -u)\n");
	printf("    N                lookup encrypting cert (via -u)\n");
	printf("Options:\n");
	printf("   -g                Generate keypair\n");
	printf("   -p                pick key pair from existing\n");
	printf("   -u username       Default = %s\n", USER_DEFAULT);
	printf("   -Z password       Specify password immediately\n");
	printf("   -z                Use default password %s\n", PWD_DEF);
	printf("   -k keychain       Source/destination of keys and certs\n");
	printf("   -c filename       Write CSR to filename\n");
	printf("   -C filename       Use existing CSR (no keygen)\n");
	printf("   -f refIdFile      RefId file for cert lookup\n");
	printf("   -n                Do NOT post the CSR to the .mac server\n");
	printf("   -H hostname       Alternate .mac server host name (default %s)\n",
					DOT_MAC_SIGN_HOST_NAME);
	printf("   -o outFile        Write output cert or refId (if any) to outFile\n");
	printf("   -r                Renew (default is new)\n");
	printf("   -M                Pause for MallocDebug\n");
	printf("   -q                Quiet\n");
	printf("   -v                Verbose\n");
	printf("   -h                Usage\n");
	exit(1);
}

static CSSM_VERSION vers = {2, 0};

static CSSM_API_MEMORY_FUNCS memFuncs = {
	cuAppMalloc,
	cuAppFree,
	cuAppRealloc,
 	cuAppCalloc,
 	NULL
 };

static CSSM_TP_HANDLE dotMacStartup()
{
	CSSM_TP_HANDLE tpHand;
	CSSM_RETURN crtn;
	
	if(cuCssmStartup() == CSSM_FALSE) {
		return 0;
	}
	crtn = CSSM_ModuleLoad(&gGuidAppleDotMacTP,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		cuPrintError("CSSM_ModuleLoad(DotMacTP)", crtn);
		return 0;
	}
	crtn = CSSM_ModuleAttach (&gGuidAppleDotMacTP,
		&vers,
		&memFuncs,				// memFuncs
		0,						// SubserviceID
		CSSM_SERVICE_TP,		// SubserviceFlags
		0,						// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,					// FunctionTable
		0,						// NumFuncTable
		NULL,					// reserved
		&tpHand);
	if(crtn) {
		cuPrintError("CSSM_ModuleAttach(DotMacTP)", crtn);
		return 0;
	}
	else {
		return tpHand;
	}
}

/* print text, safely */
static void snDumpText(
	const unsigned char *rcvBuf, 
	unsigned len)
{
	char *cp = (char *)rcvBuf;
	unsigned i;
	char c;
	
	for(i=0; i<len; i++) {
		c = *cp++;
		if(c == '\0') {
			break;
		}
		switch(c) {
			case '\n':
				printf("\\n\n");	// graphic and liternal newline
				break;
			case '\r':
				printf("\\r\n");
				break;
			default:
				if(isprint(c) && (c != '\n')) {
					printf("%c", c);
				}
				else {
					printf("<%02X>", ((unsigned)c) & 0xff);
				}
			break;
		}

	}
}

static OSStatus genKeyPair(
	SecKeychainRef  kcRef,		// NULL means the default list
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

/* Lookup via ReferenceID, obtained from CSSM_TP_SubmitCredRequest() */
OSStatus doLookupViaRefId(
	CSSM_TP_HANDLE tpHand,  
	unsigned char *refId, 
	unsigned refIdLen, 
	char *outFile, 
	bool verbose)
{
	CSSM_DATA refIdData = { refIdLen, refId };
	sint32 EstimatedTime;
	CSSM_BOOL ConfirmationRequired;
	CSSM_TP_RESULT_SET_PTR resultSet = NULL;
	CSSM_RETURN crtn;
	
	crtn = CSSM_TP_RetrieveCredResult(tpHand, &refIdData, NULL, 
		&EstimatedTime, &ConfirmationRequired, &resultSet);
	if(crtn) {
		cssmPerror("CSSM_TP_RetrieveCredResult", crtn);
		return crtn;
	}
	if(resultSet == NULL) {
		printf("***CSSM_TP_RetrieveCredResult OK, but no result set\n");
		return -1;
	}
	if(resultSet->NumberOfResults != 1) {
		printf("***CSSM_TP_RetrieveCredResult OK, NumberOfResults (%u)\n",
			(unsigned)resultSet->NumberOfResults);
		return -1;
	}
	if(resultSet->Results == NULL) {
		printf("***CSSM_TP_RetrieveCredResult OK, but empty result set\n");
		return -1;
	}
	CSSM_DATA_PTR certData = (CSSM_DATA_PTR)resultSet->Results;
	
	printf("...cert retrieval complete\n");
	if(outFile) {
		if(!writeFile(outFile, certData->Data, certData->Length)) {
			printf("...%lu bytes of cert data written to %s\n", 
					certData->Length, outFile);
		}
		else {
			printf("***Error writing cert to %s\n", outFile);
			crtn = ioErr;
		}
	}
	else if(verbose) {
		unsigned char *der;
		unsigned derLen;
		if(pemDecode(certData->Data, certData->Length, &der, &derLen)) {
			printf("***Error PEM decoding returned cert\n");
		}
		else {
			printCert(der, derLen, CSSM_FALSE);
			free(der);
		}
	}
	return noErr;
}

/* 
* Lookup via user name, a greatly simplified form of CSSM_TP_SubmitCredRequest()
*/
OSStatus doLookupViaUserName(
	CSSM_TP_HANDLE tpHand,  
	const CSSM_OID *opOid,
	const char *userName, 
	const char *hostName,		// optional 
	char *outFile, 
	bool verbose)
{
	CSSM_APPLE_DOTMAC_TP_CERT_REQUEST	certReq;
	CSSM_TP_AUTHORITY_ID				tpAuthority;
	CSSM_TP_AUTHORITY_ID				*tpAuthPtr = NULL;
	CSSM_NET_ADDRESS					tpNetAddrs;
	CSSM_TP_REQUEST_SET					reqSet;
	CSSM_FIELD							policyField;
	CSSM_DATA							certData = {0, NULL};
	sint32								estTime;
	CSSM_TP_CALLERAUTH_CONTEXT			callerAuth;

	memset(&certReq, 0, sizeof(certReq));
	certReq.userName.Data = (uint8 *)userName;
	certReq.userName.Length = strlen(userName);
	if(hostName != NULL) {
		tpAuthority.AuthorityCert = NULL;
		tpAuthority.AuthorityLocation = &tpNetAddrs;
		tpNetAddrs.AddressType = CSSM_ADDR_NAME;
		tpNetAddrs.Address.Data = (uint8 *)hostName;
		tpNetAddrs.Address.Length = strlen(hostName);
		tpAuthPtr = &tpAuthority;
	};
	
	certReq.version = CSSM_DOT_MAC_TP_REQ_VERSION;
	reqSet.NumberOfRequests = 1;
	reqSet.Requests = &certReq;
	policyField.FieldOid = *opOid;
	policyField.FieldValue.Data = NULL;
	policyField.FieldValue.Length = 0;
	memset(&callerAuth, 0, sizeof(callerAuth));
	callerAuth.Policy.NumberOfPolicyIds = 1;
	callerAuth.Policy.PolicyIds = &policyField;
	
	CSSM_RETURN crtn = CSSM_TP_SubmitCredRequest (tpHand,
		tpAuthPtr,		
		CSSM_TP_AUTHORITY_REQUEST_CERTLOOKUP, 
		&reqSet,	// const CSSM_TP_REQUEST_SET *RequestInput,
		&callerAuth,
		&estTime,   // sint32 *EstimatedTime,
		&certData);	// CSSM_DATA_PTR ReferenceIdentifier
		
	if(crtn) {
		cssmPerror("CSSM_TP_SubmitCredRequest(lookup)", crtn);
		return crtn;
	}

	printf("...cert lookup complete\n");
	if(outFile) {
		if(!writeFile(outFile, certData.Data, certData.Length)) {
			printf("...%lu bytes of cert data written to %s\n", 
					certData.Length, outFile);
		}
		else {
			printf("***Error writing cert to %s\n", outFile);
			crtn = ioErr;
		}
	}
	if(verbose) {
		/* This one returns the cert in DER format, we might revisit that */
		printCert(certData.Data, certData.Length, CSSM_FALSE);
	}
	return crtn;
}

#define FULL_EMAIL_ADDRESS	1

int main(int argc, char **argv)
{
	CSSM_RETURN							crtn;
	CSSM_TP_AUTHORITY_ID				tpAuthority;
	CSSM_TP_AUTHORITY_ID				*tpAuthPtr = NULL;
	CSSM_NET_ADDRESS					tpNetAddrs;
	CSSM_APPLE_DOTMAC_TP_CERT_REQUEST	certReq;
	CSSM_TP_REQUEST_SET					reqSet;
	CSSM_CSP_HANDLE						cspHand = 0;
	CSSM_X509_TYPE_VALUE_PAIR			tvp;
	char								pwdBuf[1000];
	CSSM_TP_CALLERAUTH_CONTEXT			callerAuth;
	sint32								estTime;
	CSSM_DATA							refId = {0, NULL};
	OSStatus							ortn;
	SecKeyRef							pubKeyRef = NULL;
	SecKeyRef							privKeyRef = NULL;
	const CSSM_KEY						*privKey = NULL;
	const CSSM_KEY						*pubKey = NULL;
	SecKeychainRef						kcRef = NULL;
	CSSM_FIELD							policyField;
			
	/* user-spec'd variables */
	bool genKeys = false;
	bool pickKeys = false;
	char *keychainName = NULL;
	char *csrOutName = NULL;
	char *csrInName = NULL;
	const char *userName = USER_DEFAULT;
	char *password = NULL;
	char *hostName = NULL;
	bool doNotPost = false;
	bool doRenew = false;
	const CSSM_OID *opOid = NULL;
	char *outFile = NULL;
	bool quiet = false;
	bool verbose = false;
	bool lookupViaRefId = false;
	bool lookupViaUserName = false;
	char *refIdFile = NULL;
	bool doPause = false;
	
	if(argc < 2) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'L':
			lookupViaUserName = true;
			/* drop thru */
		case 'g':
			opOid = &CSSMOID_DOTMAC_CERT_REQ_IDENTITY;
			break;
			
		case 'M':
			lookupViaUserName = true;
			/* drop thru */
		case 'G':
			opOid = &CSSMOID_DOTMAC_CERT_REQ_EMAIL_SIGN;
			break;
			
		case 'N':
			lookupViaUserName = true;
			/* drop thru */
		case 'e':
			opOid = &CSSMOID_DOTMAC_CERT_REQ_EMAIL_ENCRYPT;
			break;
			
		case 'l':
			lookupViaRefId = true;
			break;
		default:
			usage(argv);
	}
	
	extern char *optarg;
	extern int optind;
	optind = 2;
	int arg;
	while ((arg = getopt(argc, argv, "gpk:c:u:Z:H:nzrC:o:hf:Mqv")) != -1) {
		switch (arg) {
			case 'g':
				genKeys = true;
				break;
			case 'p':
				pickKeys = true;
				break;
			case 'u':
				userName = optarg;
				break;
			case 'Z':
				password = optarg;
				break;
			case 'z':
				password = (char *)PWD_DEF;
				break;
			case 'k':
				keychainName = optarg;
				break;
			case 'c':
				csrOutName = optarg;
				break;
			case 'C':
				csrInName = optarg;
				break;
			case 'H':
				hostName = optarg;
				break;
			case 'n':
				doNotPost = true;
				break;
			case 'r':
				doRenew = true;
				break;
			case 'o':
				outFile = optarg;
				break;
			case 'f':
				refIdFile = optarg;
				break;
			case 'M':
				doPause = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'q':
				quiet = true;
				break;
			case 'h':
				usage(argv);
		}
	}
	
	if(doPause) {
		fpurge(stdin);
		printf("Pausing for MallocDebug attach; CR to continue: ");
		getchar();
	}
	
	CSSM_TP_HANDLE tpHand = dotMacStartup();
	if(tpHand == 0) {
		printf("Error attaching to the .mac TP. Check your MDS file.\n");
		exit(1);
	}
	
	if(lookupViaRefId) {
		if(refIdFile == NULL) {
			printf("I need a refIdFile to do a lookup.\n");
			usage(argv);
		}
		unsigned char *refId;
		unsigned refIdLen;
		int irtn = readFile(refIdFile, &refId, &refIdLen);
		if(irtn) {
			printf("***Error reading refId from %s. Aborting.\n", refIdFile);
			exit(1);
		}
		ortn = doLookupViaRefId(tpHand, refId, refIdLen, outFile, verbose);
		free(refId);
		goto done;
	}
	if(lookupViaUserName) {
		ortn = doLookupViaUserName(tpHand, opOid, userName, hostName, outFile, verbose);
		goto done;
	}
	if(!pickKeys && !genKeys && (csrInName == NULL)) {
		printf("***You must specify either the -p (pick keys) or -g (generate keys)"
			" arguments, or provide a CSR (-C).\n");
		exit(1);
	}

	memset(&certReq, 0, sizeof(certReq));
	
	/* all of the subsequest argument are superfluous for lookupViaUserName, except for 
	 * the user name itself, which has a default */
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
			goto done;
		}
	}
	
	if(password == NULL) {
		const char *pwdp = getpass("Enter .mac password: ");
		if(pwdp == NULL) {
			printf("Aboerting.\n");
			ortn = paramErr;
			goto done;
		}
		memmove(pwdBuf, pwdp, strlen(pwdp) + 1);
		password = pwdBuf;
	}
	certReq.password.Data = (uint8 *)password;
	certReq.password.Length = strlen(password);
	certReq.userName.Data = (uint8 *)userName;
	certReq.userName.Length = strlen(userName);

	if(csrInName) {
		unsigned len;
		if(readFile(csrInName, &certReq.csr.Data, &len)) {
			printf("***Error reading CSR from %s. Aborting.\n", csrInName);
			exit(1);
		}
		certReq.csr.Length = len;
		certReq.flags |= CSSM_DOTMAC_TP_EXIST_CSR;
	}
	else {
		/*
		 * All the stuff the TP needs to actually generate a CSR.
		 *
		 * Get a key pair, somehow.
		 */
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
		ortn = SecKeyGetCSSMKey(pubKeyRef, &pubKey);
		if(ortn) {
			cssmPerror("SecKeyGetCSSMKey", ortn);
			goto done;
		}
		ortn = SecKeyGetCSSMKey(privKeyRef, &privKey);
		if(ortn) {
			cssmPerror("SecKeyGetCSSMKey", ortn);
			goto done;
		}
		ortn = SecKeyGetCSPHandle(privKeyRef, &cspHand);
		if(ortn) {
			cssmPerror("SecKeyGetCSPHandle", ortn);
			goto done;
		}
		
		/* CSSM_X509_TYPE_VALUE_PAIR - one pair for now */
		// tvp.type = CSSMOID_EmailAddress;
		tvp.type = CSSMOID_CommonName;
		tvp.valueType = BER_TAG_PRINTABLE_STRING;
		#if FULL_EMAIL_ADDRESS
		{
			unsigned nameLen = strlen(userName);
			tvp.value.Data = (uint8 *)malloc(nameLen + strlen("@mac.com") + 1);
			strcpy((char *)tvp.value.Data, userName);
			strcpy((char *)tvp.value.Data + nameLen, "@mac.com");
			tvp.value.Length = strlen((char *)tvp.value.Data);
		}
		#else
		tvp.value.Data = (uint8 *)userName;
		tvp.value.Length = strlen(userName);
		#endif
	}
	/* set up args for CSSM_TP_SubmitCredRequest */
	if(hostName != NULL) {
		tpAuthority.AuthorityCert = NULL;
		tpAuthority.AuthorityLocation = &tpNetAddrs;
		tpNetAddrs.AddressType = CSSM_ADDR_NAME;
		tpNetAddrs.Address.Data = (uint8 *)hostName;
		tpNetAddrs.Address.Length = strlen(hostName);
		tpAuthPtr = &tpAuthority;
	};
	
	certReq.version = CSSM_DOT_MAC_TP_REQ_VERSION;
	if(!csrInName) {
		certReq.cspHand = cspHand;
		certReq.clHand = cuClStartup();
		certReq.numTypeValuePairs = 1;
		certReq.typeValuePairs = &tvp;
		certReq.publicKey = (CSSM_KEY_PTR)pubKey;
		certReq.privateKey = (CSSM_KEY_PTR)privKey;
	}
	if(doNotPost) {
		certReq.flags |= CSSM_DOTMAC_TP_DO_NOT_POST;
	}
	if(csrOutName != NULL) {
		certReq.flags |= CSSM_DOTMAC_TP_RETURN_CSR;
	}
	if(doRenew) {
		certReq.flags |= CSSM_DOTMAC_TP_SIGN_RENEW;
	}
	
	reqSet.NumberOfRequests = 1;
	reqSet.Requests = &certReq;
	
	policyField.FieldOid = *opOid;
	policyField.FieldValue.Data = NULL;
	policyField.FieldValue.Length = 0;
	memset(&callerAuth, 0, sizeof(callerAuth));
	callerAuth.Policy.NumberOfPolicyIds = 1;
	callerAuth.Policy.PolicyIds = &policyField;
	if(!csrInName) {
		ortn = SecKeyGetCredentials(privKeyRef,
			CSSM_ACL_AUTHORIZATION_SIGN,
			kSecCredentialTypeDefault,
			const_cast<const CSSM_ACCESS_CREDENTIALS **>(&callerAuth.CallerCredentials));
		if(ortn) {
			cssmPerror("SecKeyGetCredentials", crtn);
			goto done;
		}
	}
	
	crtn = CSSM_TP_SubmitCredRequest (tpHand,
		tpAuthPtr,		
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,	// CSSM_TP_AUTHORITY_REQUEST_TYPE 
		&reqSet,	// const CSSM_TP_REQUEST_SET *RequestInput,
		&callerAuth,
		&estTime,   // sint32 *EstimatedTime,
		&refId);	// CSSM_DATA_PTR ReferenceIdentifier
	switch(crtn) {
		case CSSM_OK:
		case CSSMERR_APPLE_DOTMAC_REQ_QUEUED:
		{
			/*
			 * refId should be a cert or RefId
			 */
			const char *itemType = "Cert";
			const char *statStr = "OK";
			if(crtn != CSSM_OK) {
				itemType = "RefId";
				statStr = "Cert";
			}
			if((refId.Data == NULL) || (refId.Length == 0)) {
				printf("CSSM_TP_SubmitCredRequest returned %s but no data\n", statStr);
				break;
			}
			if(crtn == CSSM_OK) {
				printf("...cert acquisition complete\n");
			}
			else {
				printf("...Cert request QUEUED\n");
			}
			if(outFile) {
				if(!writeFile(outFile, refId.Data, refId.Length)) {
					if(!quiet) {
						printf("...%lu bytes of %s written to %s\n", 
							refId.Length, itemType, outFile);
					}
				}
				else {
					printf("***Error writing %s to %s\n", itemType, outFile);
					crtn = ioErr;
				}
			}
			else if(verbose) {
				if(crtn == CSSM_OK) {
					unsigned char *der;
					unsigned derLen;
					if(pemDecode(refId.Data, refId.Length, &der, &derLen)) {
						printf("***Error PEM decoding returned cert\n");
					}
					else {
						printCert(der, derLen, CSSM_FALSE);
						free(der);
					}
				}
				else {
					printf("RefId data:\n");
					snDumpText(refId.Data, refId.Length);
				}
			}
			break;
		}
		case CSSMERR_APPLE_DOTMAC_REQ_REDIRECT:
			if((refId.Data == NULL) || (refId.Length == 0)) {
				printf("CSSM_TP_SubmitCredRequest returned REDIRECT but no data\n");
				break;
			}
			printf("...cert acquisition : REDIRECTED to: ");
			snDumpText(refId.Data, refId.Length);
			printf("\n");
			break;
		default:
			cssmPerror("CSSM_TP_SubmitCredRequest", crtn);
			break;
	}
	if(csrOutName) {
		if((certReq.csr.Data == NULL) || (certReq.csr.Length == 0)) {
			printf("***Asked for CSR but didn't get one\n");
			ortn = paramErr;
			goto done;
		}
		if(writeFile(csrOutName, certReq.csr.Data, certReq.csr.Length)) {
			printf("***Error writing CSR to %s.\n", csrOutName);
		}
		else {
			printf("...%lu bytes written as CSR to %s\n", certReq.csr.Length, csrOutName);
		}
	}
done:
	/* cleanup */
	CSSM_ModuleDetach(tpHand);
	if(certReq.clHand) {
		CSSM_ModuleDetach(certReq.clHand);
	}
	if(kcRef) {
		CFRelease(kcRef);
	}
	if(csrInName) {
		free(certReq.csr.Data);
	}
	if(privKeyRef) {
		CFRelease(privKeyRef);
	}
	if(pubKeyRef) {
		CFRelease(pubKeyRef);
	}
	if(refId.Data) {
		cuAppFree(refId.Data, NULL);
	}
	if(doPause) {
		fpurge(stdin);
		printf("Pausing for MallocDebug measurement; CR to continue: ");
		getchar();
	}

	return ortn;
}
