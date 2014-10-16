/* 
 * dotMacArchive.cpp - test and demonstrate use of dotmacp_tp.bundle to
 * manipulate Identity archives ont he .mac server. 
 */
#include <Security/Security.h>
#include <Security/SecImportExport.h>
#include <Security/SecCertificateRequest.h>
//#include <security_dotmac_tp/dotMacTp.h>
#include <dotMacTp.h>
#include "identSearch.h"
#include "dotMacTpAttach.h"
#include <unistd.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

/*
 * Defaults for the test setup du jour 
 */
#define USER_DEFAULT			"dmitch_new"
#define PWD_DEFAULT				"password"
#define ARCHIVE_NAME_DEFAULT	"dmitch_new"
#define HOST_DEFAULT			"certmgmt.mac.com"

/* 
 * Type of archive op
 */
typedef enum {
	AO_List,
	AO_Store,
	AO_Fetch,
	AO_Remove
} ArchiveOp;

static void usage(char **argv)
{
	printf("usage: %s op [options]\n", argv[0]);
	printf("Op:\n");
	printf("    l              -- list archive contents\n");
	printf("    s              -- store archive\n");
	printf("    f              -- fetch archive\n");
	printf("    r              -- remove archive(s)\n");
	printf("Options:\n");
	printf("   -u username     -- Default is %s\n", USER_DEFAULT);
	printf("   -Z password     -- default is %s\n", PWD_DEFAULT);
	printf("   -n archiveName  -- default is %s\n", ARCHIVE_NAME_DEFAULT);
	printf("   -k keychain     -- Source/destination of archive\n");
	printf("   -H hostname     -- Alternate .mac server host name (default %s)\n",
									HOST_DEFAULT);
	printf("   -o outFile      -- write P12 blob to outFile\n");
	printf("   -z p12Phrase    -- PKCS12 passphrase (default is GUI prompt)\n");
	printf("   -M              -- Pause for MallocDebug\n");
	printf("   -l              -- loop\n");
	exit(1);
}

/* print a string in the form of a CSSM_DATA */
static void printString(
	const CSSM_DATA *str)
{
	for(unsigned dex=0; dex<str->Length; dex++) {
		printf("%c", str->Data[dex]);
	}
}


/* 
 * Post a .mac archive request, with a small number of options. 
 */
static CSSM_RETURN dotMacPostArchiveRequest(
	ArchiveOp			op,
	CSSM_TP_HANDLE		tpHand,
	/* required fields for all ops */
	const CSSM_DATA		*userName,		// REQUIRED, C string
	const CSSM_DATA		*password,		// REQUIRED, C string
	
	/* optional (per op, that is...) fields */
	const CSSM_DATA		*hostName,		// optional alternate host 
	const CSSM_DATA		*archiveName,	// required for store, fetch, remove
	const CSSM_DATA		*timeString,	// required for store
	const CSSM_DATA		*pfxIn,			// required for store
	CSSM_DATA			*pfxOut,		// required and RETURNED for fetch
	unsigned			*numArchives,	// required and RETURNED for list
	DotMacArchive		**archives)		// required and RETURNED for list
{
	CSSM_RETURN					crtn;
	CSSM_TP_AUTHORITY_ID		tpAuthority;
	CSSM_TP_AUTHORITY_ID		*tpAuthPtr = NULL;
	CSSM_NET_ADDRESS			tpNetAddrs;
	CSSM_APPLE_DOTMAC_TP_ARCHIVE_REQUEST	archReq;
	CSSM_TP_REQUEST_SET			reqSet;
	CSSM_TP_CALLERAUTH_CONTEXT	callerAuth;
	sint32						estTime;
	CSSM_DATA					refId = {0, NULL};
	CSSM_FIELD					policyField;
	const CSSM_OID				*opOid = NULL;
	
	if((tpHand == 0) || (userName == NULL) || (password == NULL)) {
		printf("dotMacPostArchiveRequest: illegal common args\n");
		return paramErr;
	}
	switch(op) {
		case AO_List:
			if((numArchives == NULL) || (archives == NULL)) {
				printf("dotMacPostArchiveRequest: illegal AO_List args\n");
				return paramErr;
			}
			opOid = &CSSMOID_DOTMAC_CERT_REQ_ARCHIVE_LIST;
			break;
		case AO_Store:
			if((archiveName == NULL) || (timeString == NULL) || (pfxIn == NULL)) {
				printf("dotMacPostArchiveRequest: illegal AO_Store args\n");
				return paramErr;
			}
			opOid = &CSSMOID_DOTMAC_CERT_REQ_ARCHIVE_STORE;
			break;
		case AO_Fetch:
			if((archiveName == NULL) || (pfxOut == NULL)) {
				printf("dotMacPostArchiveRequest: illegal AO_Fetch args\n");
				return paramErr;
			}
			opOid = &CSSMOID_DOTMAC_CERT_REQ_ARCHIVE_FETCH;
			break;
		case AO_Remove:
			if(archiveName == NULL) {
				printf("dotMacPostArchiveRequest: illegal AO_Remove args\n");
				return paramErr;
			}
			opOid = &CSSMOID_DOTMAC_CERT_REQ_ARCHIVE_REMOVE;
			break;
	}
	
	/* 
	 * The main job here is bundling up the arguments into a
	 * CSSM_APPLE_DOTMAC_TP_ARCHIVE_REQUEST 
	 */
	memset(&archReq, 0, sizeof(archReq));
	archReq.version = CSSM_DOT_MAC_TP_ARCHIVE_REQ_VERSION;
	archReq.userName = *userName;
	archReq.password = *password;
	if(archiveName) {
		archReq.archiveName = *archiveName;
	}
	if(timeString) {
		archReq.timeString = *timeString;
	}
	if(pfxIn) {
		archReq.pfx = *pfxIn;
	}
	
	/* remaining arguments for TP call... */
	if((hostName != NULL) && (hostName->Data != NULL)) {
		tpAuthority.AuthorityCert = NULL;
		tpAuthority.AuthorityLocation = &tpNetAddrs;
		tpNetAddrs.AddressType = CSSM_ADDR_NAME;
		tpNetAddrs.Address = *hostName;
		tpAuthPtr = &tpAuthority;
	}

	reqSet.NumberOfRequests = 1;
	reqSet.Requests = &archReq;
	
	policyField.FieldOid = *opOid;
	policyField.FieldValue.Data = NULL;
	policyField.FieldValue.Length = 0;
	memset(&callerAuth, 0, sizeof(callerAuth));
	callerAuth.Policy.NumberOfPolicyIds = 1;
	callerAuth.Policy.PolicyIds = &policyField;

	crtn = CSSM_TP_SubmitCredRequest (tpHand,
		tpAuthPtr,		
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,	// CSSM_TP_AUTHORITY_REQUEST_TYPE 
		&reqSet,	// const CSSM_TP_REQUEST_SET *RequestInput,
		&callerAuth,
		&estTime,   // sint32 *EstimatedTime,
		&refId);	// CSSM_DATA_PTR ReferenceIdentifier
	if(crtn) {
		cssmPerror("CSSM_TP_SubmitCredRequest", crtn);
	}
	
	/* success: post-process */
	switch(op) {
		case AO_List:
			*numArchives = archReq.numArchives;
			*archives = archReq.archives;
			break;
		case AO_Store:
			break;
		case AO_Fetch:
			*pfxOut = archReq.pfx;
			break;
		case AO_Remove:
			break;
	}

	return CSSM_OK;
}

static void cStringToCssmData(
	const char	*cstr,
	CSSM_DATA	*cdata)
{
	if(cstr) {
		cdata->Data = (uint8 *)cstr;
		cdata->Length = strlen(cstr);
	}
	else {
		cdata->Data = NULL;
		cdata->Length = 0;
	}
}

int main(int argc, char **argv)
{
	SecKeychainRef	kcRef = NULL;
	CSSM_RETURN		crtn;
	CSSM_TP_HANDLE	tpHand = 0;
	OSStatus		ortn;
	
	/* user-spec'd variables */
	ArchiveOp		op = AO_List;
	char			*keychainName = NULL;
	const char		*userName = USER_DEFAULT;
	const char		*password = PWD_DEFAULT;
	const char		*archName = ARCHIVE_NAME_DEFAULT;
	const char		*hostName = HOST_DEFAULT;
	char			*outFile = NULL;
	bool			doPause = false;
	bool			loop = false;
	char			*p12Phrase = NULL;
	
	if(argc < 2) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'l':
			op = AO_List;
			break;
		case 's':
			op = AO_Store;
			break;
		case 'f':
			op = AO_Fetch;
			break;
		case 'r':
			op = AO_Remove;
			break;
		default:
			usage(argv);
	}

	extern char *optarg;
	extern int optind;
	optind = 2;
	int arg;
	while ((arg = getopt(argc, argv, "u:Z:n:k:H:Nlo:z:")) != -1) {
		switch (arg) {
			case 'u':
				userName = optarg;
				break;
			case 'Z':
				password = optarg;
				break;
			case 'n':
				archName = optarg;
				break;
			case 'k':
				keychainName = optarg;
				break;
			case 'M':
				doPause = true;
				break;
			case 'H':
				hostName = optarg;
				break;
			case 'l':
				loop = true;
				break;
			case 'o':
				outFile = optarg;
				break;
			case 'z':
				p12Phrase = optarg;
				break;
			case 'h':
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
	
	/* bundle up our crufty C string args into CSSM_DATAs needed at the TP SPI */
	CSSM_DATA userNameData;
	CSSM_DATA passwordData;
	CSSM_DATA hostNameData;
	CSSM_DATA archNameData;
	CSSM_DATA timeStringData;
	
	cStringToCssmData(userName, &userNameData);
	cStringToCssmData(password, &passwordData);
	cStringToCssmData(hostName, &hostNameData);
	cStringToCssmData(archName, &archNameData);
	
	/* time in seconds since the epoch, sprintf'd in base 10 */
	char timeStr[20];
	time_t nowTime = time(NULL);
	printf("...nowTime = %lu\n", nowTime);
	//nowTime += (60 * 60 * 24 * 26);	// fails
	nowTime += (60 * 60 * 24 * 25);		// works
	printf("...expirationTime = %lu\n", nowTime);
	sprintf(timeStr, "%lu", nowTime);
	timeStringData.Data = (uint8 *)timeStr;
	timeStringData.Length = strlen(timeStr);
	
	/* other data needed by dotMacPostArchiveRequest() */
	CFDataRef		p12 = NULL;
	CSSM_DATA		pfxInData = {0, NULL};
	CSSM_DATA		pfxOutData = {0, NULL};
	unsigned		numArchives = 0;
	DotMacArchive	*archives = NULL;	
	
	/* Store op: get identity in p12 form */
	if(op == AO_Store) {
		CFStringRef cfPhrase = NULL;

		/* Cert attribute - email address - contains the "@mac.com" */
		char emailAddr[500];
		strcpy(emailAddr, userName);
		// nope strcat(emailAddr, "@mac.com");
		
		/* find an identity for that email address */
		SecIdentityRef idRef = NULL;
		OSStatus ortn = findIdentity(emailAddr, strlen(emailAddr), kcRef, &idRef);
		if(ortn) {
			printf("***Could not find an identity to store. Aborting.\n");
			goto errOut;
		}
		
		/* convert that identity to p12 */
		SecKeyImportExportParameters keyParams;
		memset(&keyParams, 0, sizeof(keyParams));
		keyParams.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
		if(p12Phrase) {
			cfPhrase = CFStringCreateWithCString(NULL, p12Phrase, 
				kCFStringEncodingUTF8);
			keyParams.passphrase = cfPhrase;
		}
		else {
			keyParams.flags = kSecKeySecurePassphrase;
		}
		keyParams.alertTitle = CFSTR(".mac Identity Backup");
		keyParams.alertPrompt = 
			CFSTR("Enter passphrase for encrypting your .mac private key");
		ortn = SecKeychainItemExport(idRef, kSecFormatPKCS12, kSecItemPemArmour, 
			&keyParams, &p12);
		if(ortn) {
			cssmPerror("SecKeychainItemExport", crtn);
			printf("***Error obtaining .mac identity in PKCS12 format. Aborting.\n");
			goto errOut;
		}
		
		pfxInData.Data = (uint8 *)CFDataGetBytePtr(p12);
		pfxInData.Length = CFDataGetLength(p12);
		printf("...preparing to store archive of %lu bytes\n", pfxInData.Length);
		
		if(outFile) {
			if(writeFile(outFile, pfxInData.Data, pfxInData.Length)) {
				printf("***Error writing P12 to %s\n", outFile);
			}
			else {
				printf("...wrote %lu bytes to %s\n", pfxInData.Length, outFile);
			}
		}
		if(cfPhrase) {
			CFRelease(cfPhrase);
		}
	}
	
	/* attach to the TP */
	tpHand = dotMacTpAttach();
	if(tpHand == 0) {
		printf("***Error attaching to .mac TP; aborting.\n");
		ortn = -1;
		goto errOut;
	}
	
	/* go */
	crtn = dotMacPostArchiveRequest(op, tpHand, &userNameData, &passwordData,
		hostName ? &hostNameData : NULL,
		&archNameData,
		&timeStringData,
		&pfxInData,
		&pfxOutData,
		&numArchives,
		&archives);
	if(crtn) {
		printf("***Error performing archive request; aborting.\n");
		goto errOut;
	}
	
	/* post-request processing */
	
	switch(op) {
		case AO_List:
		{
			printf("=== List request complete; numArchives = %u ===\n", numArchives);
			for(unsigned dex=0; dex<numArchives; dex++) {
				DotMacArchive *dmarch = &archives[dex];
				printf("Archive %u:\n", dex);
				printf("   name : "); 
				printString(&dmarch->archiveName); 
				printf("\n");
				
				printf("   time : "); 
				printString(&dmarch->timeString); 
				printf("\n");
				
				/* now free what the TP allocated on our behalf */
				APP_FREE(dmarch->archiveName.Data);
				APP_FREE(dmarch->timeString.Data);
			}
			APP_FREE(archives);
			break;
		}

		case AO_Store:
			printf("=== archive \'%s\' backup complete ===\n", archName);
			break;
		case AO_Fetch:
		{
			bool didSomething = false;
			if(pfxOutData.Length == 0) {
				printf("***Archive fetch claimed to succeed, but no data seen\n");
				ortn = -1;
				goto errOut;
			}
			
			/* 
			 * OK, we have a blob of PKCS12 data. Import to keychain and/or write it 
			 * to a file.
			 */
			printf("=== %lu bytes of archive fetched ===\n", pfxOutData.Length);
			if(outFile) {
				if(writeFile(outFile, pfxOutData.Data, pfxOutData.Length)) {
					printf("***Error writing P12 to %s\n", outFile);
				}
				else {
					printf("...wrote %lu bytes to %s\n", pfxOutData.Length, outFile);
					didSomething = true;
				}
			}
			if(kcRef) {
				/* Note we avoid importing to default keychain - user must really want 
				 * to perform this step */
				CFDataRef p12Data = CFDataCreate(NULL, pfxOutData.Data, pfxOutData.Length);
				SecExternalFormat extForm = kSecFormatPKCS12;
				SecKeyImportExportParameters keyParams;
				memset(&keyParams, 0, sizeof(keyParams));
				keyParams.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
				CFStringRef cfPhrase = NULL;
				if(p12Phrase) {
					cfPhrase = CFStringCreateWithCString(NULL, p12Phrase, 
						kCFStringEncodingUTF8);
					keyParams.passphrase = cfPhrase;
				}
				else {
					keyParams.flags = kSecKeySecurePassphrase;
				}
				keyParams.alertTitle = CFSTR(".mac Identity Restore");
				keyParams.alertPrompt = 
					CFSTR("Enter passphrase for decrypting your .mac private key");
					
				/* go... */
				ortn = SecKeychainItemImport(p12Data, 
					NULL,		// filename - passing kSecFormatPKCS12 is definitely enough
					&extForm,
					NULL,		// itemType - import'll figure it out
					0,			// SecItemImportExportFlags
					&keyParams,
					kcRef,
					NULL);		// we don't want any items returned
				if(ortn) {
					cssmPerror("SecKeychainItemImport", ortn);
					printf("***Error importing p12 into keychain %s\n", keychainName);
				}
				else {
					printf("...archive successfully imported into keychain %s\n",
						keychainName);
					didSomething = true;
				}
				if(cfPhrase) {
					CFRelease(cfPhrase);
				}
			}
			if(!didSomething) {
				printf("...note we got an archive from the server but didn't have a "
					"place to put it.\n");
			}
			break;
		}
		case AO_Remove:
			printf("=== Archive %s removed ===\n", archName);
			break;
	}

	if(doPause) {
		fpurge(stdin);
		printf("Pausing at end of test for MallocDebug attach; CR to continue: ");
		getchar();
	}

errOut:
	if(kcRef) {
		CFRelease(kcRef);
	}

	if(tpHand) {
		dotMacTpDetach(tpHand);
	}
	return ortn;
}

