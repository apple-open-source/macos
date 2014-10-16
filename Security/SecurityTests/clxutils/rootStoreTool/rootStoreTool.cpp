/*
 * rootStoreTool.cpp - exercise SecTrustSettings API
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <Security/Security.h>
#include <Security/SecTrustSettings.h>
#include <Security/SecTrustPriv.h>
#include <Security/TrustSettingsSchema.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/cssmapplePriv.h>
#include <Security/SecPolicyPriv.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_utilities/cfutilities.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <security_cdsa_utils/cuOidParser.h>
#include "parseTrustedRootList.h"
#include <Security/TrustSettingsSchema.h>		/* private header */
#include "rootUtils.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <pthread.h>
#include <sys/param.h>

static void usage(char **argv)
{
	printf("usage: %s op [options]\n", argv[0]);
	printf("Op values:\n");
	printf("   a   -- add cert\n");
	printf("   p   -- parse TrustSettings record\n");
	printf("   r   -- get certs from TS & display\n");
	printf("   d   -- delete entries from TS interactively\n");
	printf("   D   -- delete ALL certs from TS (requires -R argument)\n");
	printf("   R   -- remove legacy User Trust setting\n");
	
	printf("Options:\n");
	printf("  -c certFile        -- specify cert\n");
	printf("  -s                 -- system TrustSettings; default is user\n");
	printf("  -d                 -- Admin TrustSettings; default is user\n");
	printf("  -t settingsFile    -- settings from file; default is user\n");
	printf("  -T settingsFileOut -- settings to file\n");
	printf("  -a appPath         -- specify app constraints\n");
	printf("  -p policy          -- specify policy constraint\n");
	printf("                        policy = ssl, smime, swuSign, codeSign, IPSec, iChat\n");
	printf("  -P appPath policy  -- specify app AND policy constraint\n");
	printf("  -e emailAddress    -- specify SMIME policy plus email address\n");
	printf("  -L hostname        -- specify SSL policy plus hostname\n");
	printf("  -r resultType      -- resultType = trust, trustAsRoot, deny, unspecified\n");
	printf("  -w allowErr        -- allowed error, an integer; implies result unspecified\n");
	printf("  -W allowErr policy -- allowed error AND policy AND implies result unspecified\n");
	printf("  -u keyUsage        -- key usage, an integer\n");
	printf("  -k keychain        -- Default is default keychain.\n");
	printf("  -R                 -- Really. For Delete All op.\n");
	printf("  -v                 -- verbose cert display\n");
	printf("  -A                 -- add cert to keychain\n");
	printf("  -U                 -- use SecTrustSetUserTrust\n");
	printf("  -2                 -- use SecTrustSetUserTrustLegacy\n");
	printf("  -l                 -- loop and pause for malloc debug\n");
	printf("  -h                 -- help\n");
	exit(1);
}

/* 
 * Start up a CFRunLoop. This is needed to field keychain event callbacks, used
 * to maintain root cert cache coherency. This operation is only needed in command 
 * line tools; regular GUI apps already have a CFRunLoop. 
 */
 
/* first we need something to register so we *have* a run loop */
static OSStatus kcCacheCallback (
   SecKeychainEvent keychainEvent,
   SecKeychainCallbackInfo *info,
   void *context)
{
	return noErr;
}

/* main thread has to wait for this to be set to know a run loop has been set up */
static int runLoopInitialized = 0;

/* this is the thread which actually runs the CFRunLoop */
void *cfRunLoopThread(void *arg)
{
	OSStatus ortn = SecKeychainAddCallback(kcCacheCallback, 
		kSecTrustSettingsChangedEventMask, NULL);
	if(ortn) {
		printf("registerCacheCallbacks: SecKeychainAddCallback returned %ld", ortn);
		/* Not sure how this could ever happen - maybe if there is no run loop active? */
		return NULL;
	}
	runLoopInitialized = 1;
	CFRunLoopRun();
	/* should not be reached */
	printf("\n*** Hey! CFRunLoopRun() exited!***\n");
	return NULL;
}

static int startCFRunLoop()
{
	pthread_t runLoopThread;
	
	int result = pthread_create(&runLoopThread, NULL, cfRunLoopThread, NULL);
	if(result) {
		printf("***pthread_create returned %d, aborting\n", result);
		return -1;
	}
	return 0;
}

static SecCertificateRef certFromFile(
	const char *fileName)
{
	unsigned char *cp = NULL;
	unsigned len = 0;
	if(readFile(fileName, &cp, &len)) {
		printf("***Error reading file %s\n", fileName);
		return NULL;
	}
	SecCertificateRef certRef;
	CSSM_DATA certData = {len, cp};
	OSStatus ortn = SecCertificateCreateFromData(&certData, 
			CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER, &certRef);
	if(ortn) {
		cssmPerror("SecCertificateCreateFromData", ortn);
		return NULL;
	}
	free(cp);
	return certRef;
}

/*
 * Display usage constraints array as obtained from 
 * SecTrustSettingsCopyTrustSettings().
 */
static int displayTrustSettings(
	CFArrayRef	trustSettings,
	OidParser	&parser)
{
	/* must always be there though it may be empty */
	if(trustSettings == NULL) {
		printf("***displayTrustSettings: missing trust settings array");
		return -1;
	}
	if(CFGetTypeID(trustSettings) != CFArrayGetTypeID()) {
		printf("***displayTrustSettings: malformed trust settings array");
		return -1;
	}
	
	int ourRtn = 0;
	CFIndex numUseConstraints = CFArrayGetCount(trustSettings);
	indentIncr();
	indent(); printf("Number of trust settings : %ld\n", numUseConstraints);
	OSStatus ortn;
	SecPolicyRef certPolicy;
	SecTrustedApplicationRef certApp;
	CFDictionaryRef ucDict;
	CFStringRef policyStr;
	CFNumberRef cfNum;
	
	/* grind thru the trust settings dictionaries */
	for(CFIndex ucDex=0; ucDex<numUseConstraints; ucDex++) {
		indent(); printf("Trust Setting %ld:\n", ucDex);
		indentIncr();
		
		ucDict = (CFDictionaryRef)CFArrayGetValueAtIndex(trustSettings, ucDex);
		if(CFGetTypeID(ucDict) != CFDictionaryGetTypeID()) {
			printf("***displayTrustSettings: malformed usage constraints dictionary");
			ourRtn = -1;
			goto nextAp;
		}
		
		/* policy - optional */
		certPolicy = (SecPolicyRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsPolicy);
		if(certPolicy != NULL) {
			if(CFGetTypeID(certPolicy) != SecPolicyGetTypeID()) {
				printf("***displayTrustSettings: malformed certPolicy");
				ourRtn = -1;
				goto nextAp;
			}
			CSSM_OID policyOid;
			ortn = SecPolicyGetOID(certPolicy, &policyOid);
			if(ortn) {
				cssmPerror("SecPolicyGetOID", ortn);
				ourRtn = -1;
				goto nextAp;
			}
			indent(); printf("Policy OID            : "); 
			printOid(policyOid.Data, policyOid.Length, parser);
			printf("\n");
		}
		
		/* app - optional  */
		certApp = (SecTrustedApplicationRef)CFDictionaryGetValue(ucDict, 
			kSecTrustSettingsApplication);
		if(certApp != NULL) {
			if(CFGetTypeID(certApp) != SecTrustedApplicationGetTypeID()) {
				printf("***displayTrustSettings: malformed certApp");
				ourRtn = -1;
				goto nextAp;
			}
			CFRef<CFDataRef> appPath;
			ortn = SecTrustedApplicationCopyData(certApp, appPath.take());
			if(ortn) {
				cssmPerror("SecTrustedApplicationCopyData", ortn);
				ourRtn = -1;
				goto nextAp;
			}
			indent(); printf("Application           : %s", CFDataGetBytePtr(appPath));
			printf("\n");
		}
		
		/* policy string */
		policyStr = (CFStringRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsPolicyString);
		if(policyStr != NULL) {
			if(CFGetTypeID(policyStr) != CFStringGetTypeID()) {
				printf("***displayTrustSettings: malformed policyStr");
				ourRtn = -1;
				goto nextAp;
			}
			indent(); printf("Policy String         : ");
			printCfStr(policyStr); printf("\n");
		}

		/* Allowed error */
		cfNum = (CFNumberRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsAllowedError);
		if(cfNum != NULL) {
			if(CFGetTypeID(cfNum) != CFNumberGetTypeID()) {
				printf("***displayTrustSettings: malformed allowedError");
				ourRtn = -1;
				goto nextAp;
			}
			indent(); printf("Allowed Error         : ");
			printCssmErr(cfNum); printf("\n");
		}

		/* Result */
		cfNum = (CFNumberRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsResult);
		if(cfNum != NULL) {
			if(CFGetTypeID(cfNum) != CFNumberGetTypeID()) {
				printf("***displayTrustSettings: malformed Result");
				ourRtn = -1;
				goto nextAp;
			}
			indent(); printf("Result Type           : ");
			printResult(cfNum); printf("\n");
		}

		/* key usage */
		cfNum = (CFNumberRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsKeyUsage);
		if(cfNum != NULL) {
			if(CFGetTypeID(cfNum) != CFNumberGetTypeID()) {
				printf("***displayTrustSettings: malformed keyUsage");
				ourRtn = -1;
				goto nextAp;
			}
			indent(); printf("Key Usage             : ");
			printKeyUsage(cfNum); printf("\n");
		}

	nextAp:
		indentDecr();
	}
	indentDecr();
	return ourRtn;
}

/* convert an OID to a SecPolicyRef */
static SecPolicyRef oidToPolicy(
	const CSSM_OID &oid)
{
	SecPolicyRef policyRef = NULL;

	OSStatus ortn = SecPolicyCopy(CSSM_CERT_X_509v3, &oid, &policyRef);
	if(ortn) {
		cssmPerror("SecPolicyCopy", ortn);
		return NULL;
	}
	return policyRef;
}

/* Convert cmdline policy string to SecPolicyRef */
static SecPolicyRef policyStringToPolicy(
	const char *policy)
{
	if(policy == NULL) {
		return NULL;
	}
	const CSSM_OID *oid = NULL;
	if(!strcmp(policy, "ssl")) {
		oid = &CSSMOID_APPLE_TP_SSL;
	}
	else if(!strcmp(policy, "smime")) {
		oid = &CSSMOID_APPLE_TP_SMIME;
	}
	else if(!strcmp(policy, "codeSign")) {
		oid = &CSSMOID_APPLE_TP_CODE_SIGNING;
	}
	else if(!strcmp(policy, "swuSign")) {
		oid = &CSSMOID_APPLE_TP_SW_UPDATE_SIGNING;
	}
	else if(!strcmp(policy, "IPSec")) {
		oid = &CSSMOID_APPLE_TP_IP_SEC;
	}
	else if(!strcmp(policy, "iChat")) {
		oid = &CSSMOID_APPLE_TP_ICHAT;
	}
	else {
		printf("***Unknown policy string (%s)\n", policy);
		return NULL;
	}

	/* OID to SecPolicyRef */
	return oidToPolicy(*oid);
}

static int appendConstraintToArray(
	const char *appPath,			/* optional, "-" means ensure apArray is nonempty */
	const char *policy,				/* optional (ssl/smime), "-" as above */
	const char *policyStr,			/* optional policy string */
	const SInt32 *allowErr,			/* optional allowed error */
	const char *resultType,			/* optional allow/confirm/deny */
	SecTrustSettingsKeyUsage keyUse,	/* optional key use */
	CFMutableArrayRef &array)		/* result RETURNED here, created if necessary */
{
	if(array == NULL) {
		array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	
	CFMutableDictionaryRef outDict = CFDictionaryCreateMutable(NULL, 
		0,		// capacity 
		&kCFTypeDictionaryKeyCallBacks, 
		&kCFTypeDictionaryValueCallBacks);

	if((policy != NULL) && (strcmp(policy, "-"))) {
		
		/* policy string to SecPolicyRef */
		SecPolicyRef policyRef = policyStringToPolicy(policy);
		if(policyRef == NULL) {
			return -1;
		}
		CFDictionaryAddValue(outDict, kSecTrustSettingsPolicy, policyRef);
		CFRelease(policyRef);
	}
	
	/* app string to SecTrustedApplicationRef */
	if((appPath != NULL) && (strcmp(appPath, "-"))) {
		SecTrustedApplicationRef appRef;
		OSStatus ortn = SecTrustedApplicationCreateFromPath(appPath, &appRef);
		if(ortn) {
			cssmPerror("SecTrustedApplicationCreateFromPath", ortn);
			return -1;
		}
		CFDictionaryAddValue(outDict, kSecTrustSettingsApplication, appRef);
		CFRelease(appRef);
	}
	
	if(policyStr != NULL) {
		CFStringRef pstr = CFStringCreateWithCString(NULL, policyStr, kCFStringEncodingASCII);
		CFDictionaryAddValue(outDict, kSecTrustSettingsPolicyString, pstr);
		CFRelease(pstr);
	}
	
	if(allowErr != NULL) {
		CFNumberRef cfNum = CFNumberCreate(NULL, kCFNumberSInt32Type, allowErr);
		CFDictionaryAddValue(outDict, kSecTrustSettingsAllowedError, cfNum);
		CFRelease(cfNum);
	}

	if(keyUse != 0) {
		SInt32 ku = (SInt32)ku;
		CFNumberRef cfNum = CFNumberCreate(NULL, kCFNumberSInt32Type, &ku);
		CFDictionaryAddValue(outDict, kSecTrustSettingsKeyUsage, cfNum);
		CFRelease(cfNum);
	}
	
	if(resultType != NULL) {
		SInt32 n;
		
		if(!strcmp(resultType, "trust")) {
			n = kSecTrustSettingsResultTrustRoot;
		}
		else if(!strcmp(resultType, "trustAsRoot")) {
			n = kSecTrustSettingsResultTrustAsRoot;
		}
		else if(!strcmp(resultType, "deny")) {
			n = kSecTrustSettingsResultDeny;
		}
		else if(!strcmp(resultType, "unspecified")) {
			n = kSecTrustSettingsResultUnspecified;
		}
		else {
			printf("***unknown resultType spec (%s)\n", resultType);
			return -1;
		}
		CFNumberRef cfNum = CFNumberCreate(NULL, kCFNumberSInt32Type, &n);
		CFDictionaryAddValue(outDict, kSecTrustSettingsResult, cfNum);
		CFRelease(cfNum);
	}
	
	/* append dictionary to output */
	CFArrayAppendValue(array, outDict);
	/* array owns the dictionary now */
	CFRelease(outDict);
	return 0;
}

/* read a file --> CFDataRef */
CFDataRef readFileCFData(
	const char *fileName)
{
	int rtn;
	unsigned char *fileData = NULL;
	unsigned fileDataLen = 0;

	rtn = readFile(fileName, &fileData, &fileDataLen);
	if(rtn) {
		printf("Error (%d) reading %s.\n", rtn, fileName);
		return NULL;
	}
	CFDataRef cfd = CFDataCreate(NULL, (const UInt8 *)fileData, fileDataLen);
	free(fileData);
	return cfd;
}

static int fetchParseTrustRecord(
	SecTrustSettingsDomain domain,
	char *settingsFile)				/* optional, ignore domain if present */
{
	CFDataRef trustSettings = NULL;
	
	if(settingsFile) {
		trustSettings = readFileCFData(settingsFile);
		if(trustSettings == NULL) {
			return -1;
		}
	}
	else {
		OSStatus ortn = SecTrustSettingsCreateExternalRepresentation(domain, &trustSettings);
		if(ortn) {
			cssmPerror("SecTrustSettingsCreateExternalRepresentation", ortn);
			return -1;
		}
	}
	int rtn = parseTrustedRootList(trustSettings);
	CFRelease(trustSettings);
	return rtn;
}

static int copyCertsAndDisplay(
	bool verbose,
	SecTrustSettingsDomain domain)
{
	OSStatus ortn;
	
	auto_ptr<OidParser> parser(NULL);
	
	if(verbose) {
		parser.reset(new OidParser);
	}
	
	CFArrayRef certArray = NULL;
	ortn = SecTrustSettingsCopyCertificates(domain, &certArray);
	if(ortn) {
		cssmPerror("SecTrustSettingsCopyCertificates", ortn);
		return ortn;
	}
	
	CFIndex numCerts = CFArrayGetCount(certArray);
	indent();
	printf("Num certs = %ld\n", numCerts);
	int ourRtn = 0;
	for(CFIndex dex=0; dex<numCerts; dex++) {
		SecCertificateRef certRef = (SecCertificateRef)CFArrayGetValueAtIndex(certArray, dex);
		if(CFGetTypeID(certRef) != SecCertificateGetTypeID()) {
			printf("***Bad CFGetTypeID for cert\n");
			return -1;
		}
		indent();
		printf("Cert %ld: ", dex); 
		printCertLabel(certRef);
		printf("\n");
		if(verbose) {
			CFRef<CFArrayRef> appPolicies;
			ortn = SecTrustSettingsCopyTrustSettings(certRef, domain, appPolicies.take());
			if(ortn) {
				cssmPerror("SecRootCertificateCopyAppPolicyConstraints", ortn);
				ourRtn = -1;
				continue;
			}
			if(displayTrustSettings(appPolicies, *parser.get())) {
				ourRtn = -1;
			}
		}
	}
	CFRelease(certArray);
	return ourRtn;
}

static int deleteCerts(
	SecTrustSettingsDomain domain,
	bool deleteAll)
{
	OSStatus ortn;
	
	CFArrayRef certArray = NULL;
	ortn = SecTrustSettingsCopyCertificates(domain, &certArray);
	if(ortn) {
		cssmPerror("SecTrustSettingsCopyCertificates", ortn);
		return ortn;
	}
	
	CFIndex numCerts = CFArrayGetCount(certArray);
	unsigned numDeleted = 0;
	
	for(CFIndex dex=0; dex<numCerts; dex++) {
		SecCertificateRef certRef = (SecCertificateRef)CFArrayGetValueAtIndex(certArray, dex);
		if(CFGetTypeID(certRef) != SecCertificateGetTypeID()) {
			printf("***Bad CFGetTypeID for cert\n");
			return -1;
		}
		bool doDelete = false;
		
		if(deleteAll) {
			printf("DELETING: ");
			printCertLabel(certRef);
			printf("\n");
			doDelete = true;
		}
		else {
			indent();
			printf("Cert %ld: ", dex);
			printCertLabel(certRef);
			printf("\n");
			fpurge(stdin);
			printf("Delete (y/anything)? ");
			char resp = getchar();
			if(resp == 'y') {
				doDelete = true;
			}
		}
		if(doDelete) {
			ortn = SecTrustSettingsRemoveTrustSettings(certRef, domain);
			if(ortn) {
				cssmPerror("SecTrustSettingsRemoveTrustSettings", ortn);
				fpurge(stdin);
				printf("Continue deleting (y/anything)? ");
				char resp = getchar();
				fflush(stdout);
				if(resp != 'y') {
					return ortn;
				}
			}
			else {
				numDeleted++;
			}
		}
	}
	CFRelease(certArray);
	printf("...%u certs deleted\n", numDeleted);
	return noErr;
}

/* add a cert to trust list */
static int addCert(
	SecCertificateRef certRef,
	SecTrustSettingsDomain domain,
	bool addToKc,					// import cert to keychain 
	const char *kcName,				// only for addToKC option
	CFArrayRef trustSettings,
	CFDataRef settingsIn,			// optional, requires settingsFileOut
	CFDataRef *settingsOut)
{
	OSStatus ortn;
	char *domainName;

	if(settingsIn && !settingsOut) {
		printf("Modifying trust settings as file requires output file\n");
		return -1;
	}
	switch(domain) {
		case kSecTrustSettingsDomainSystem:
			printf("***Can't modify system trust settings.\n");
			return -1;
		case kSecTrustSettingsDomainAdmin:
			kcName = "/Library/Keychains/System.keychain";
			domainName = "Admin";
			break;
		default:
			domainName = "User";
			break;
	}
	if(addToKc) {
		SecKeychainRef kcRef = NULL;
		if(kcName) {
			ortn = SecKeychainOpen(kcName, &kcRef);
			if(ortn) {
				cssmPerror("SecKeychainOpen", ortn);
				return -1;
			}
		}
		ortn = SecCertificateAddToKeychain(certRef, kcRef);
		if(ortn) {
			cssmPerror("SecCertificateAddToKeychain", ortn);
			return -1;
		}
		printf("...cert added to keychain %s\n", (kcName ? kcName : "<default>"));
	}
	if(settingsIn) {
		ortn = SecTrustSettingsSetTrustSettingsExternal(settingsIn,
			certRef, trustSettings, settingsOut);
		if(ortn) {
			cssmPerror("SecTrustSettingsSetTrustSettingsExternal", ortn);
			return -1;
		}
	}
	else {
		ortn = SecTrustSettingsSetTrustSettings(certRef, domain, trustSettings);
		if(ortn) {
			cssmPerror("SecTrustSettingsSetTrustSettings", ortn);
			return -1;
		}
		printf("...cert added to %s TrustList.\n", domainName);
	}
	return 0;
}

static int addCertLegacy(
	SecCertificateRef certRef, 
	const char *policy, 
	const char *resultStr,
	bool useLegacy)
{
	/* OID string to an OID pointer */
	if(policy == NULL) {
		printf("***You must specify a policy to set legacy User Trust\n");
		return 1;
	}
	SecPolicyRef policyRef = policyStringToPolicy(policy);
	if(policyRef == NULL) {
		return -1;
	}

	/* result string to legacy SecTrustUserSetting */
	SecTrustUserSetting setting = kSecTrustResultInvalid;
	if(resultStr == NULL) {
		setting = kSecTrustResultProceed;
	}
	else if(!strcmp(resultStr, "trust")) {
		setting = kSecTrustResultProceed;
	}
	else if(!strcmp(resultStr, "trustAsRoot")) {
		setting = kSecTrustResultProceed;
	}
	else if(!strcmp(resultStr, "deny")) {
		setting = kSecTrustResultDeny;
	}
	else if (!strcmp(resultStr, "unspecified")) {
		setting = kSecTrustResultUnspecified;
	}
	else {
		printf("***Can't map %s to a SecTrustUserSetting\n", resultStr);
		return -1;
	}
	OSStatus ortn;
	if(useLegacy) {
		ortn = SecTrustSetUserTrustLegacy(certRef, policyRef, setting);
		if(ortn) {
			cssmPerror("SecTrustSetUserTrustLegacy", ortn);
		}
		else {
			if(setting == kSecTrustResultUnspecified) {
				printf("...User Trust removed via SecTrustSetUserTrustLegacy().\n");
			}
			else {
				printf("...User Trust set via SecTrustSetUserTrustLegacy().\n");
			}
		}
	}
	else {
		#if 1
		printf("...Legacy implementation needs Makefile work to avoid deprecation error\n");
		exit(1);
		#else
		ortn = SecTrustSetUserTrust(certRef, policyRef, setting);
		if(ortn) {
			cssmPerror("SecTrustSetUserTrust", ortn);
		}
		else {
			printf("...trust setting set via SecTrustSetUserTrust().\n");
		}
		#endif
	}
	if(policyRef != NULL) {
		CFRelease(policyRef);
	}
	return ortn;
}

int main(int argc, char **argv)
{
	int arg;
	CFMutableArrayRef appPolicies = NULL;
	CFDataRef settingsIn = NULL;
	CFDataRef settingsOut = NULL;
	
	/* user-spec'd variables */
	bool loopPause = false;
	bool really = false;
	bool verbose = false;
	char *kcName = NULL;
	SecTrustSettingsDomain domain = kSecTrustSettingsDomainUser;
	SecCertificateRef certRef = NULL;
	bool addToKeychain = false;
	char *settingsFileIn = NULL;
	char *settingsFileOut = NULL;
	bool userTrustLegacy = false;
	char *policyStr = NULL;
	char *resultStr = NULL;
	bool userTrust = false;

	extern char *optarg;
	extern int optind;
	optind = 2;
	while ((arg = getopt(argc, argv, "c:sdt:T:a:p:P:e:L:r:w:W:k:u:RvAU2lh")) != -1) {
		switch (arg) {
			case 'c':
				if(certRef) {
					printf("***Only one cert at a time, please.\n");
					usage(argv);
				}
				certRef = certFromFile(optarg);
				if(certRef == NULL) {
					exit(1);
				}
				break;
			case 's':
				domain = kSecTrustSettingsDomainSystem;
				break;
			case 'd':
				domain = kSecTrustSettingsDomainAdmin;
				break;
			case 't':
				settingsFileIn = optarg;
				break;
			case 'T':
				settingsFileOut = optarg;
				break;
			case 'a':
				if(appendConstraintToArray(optarg, NULL, NULL, NULL, NULL, 0, appPolicies)) {
					exit(1);
				}
				break;
			case 'p':
				if(appendConstraintToArray(NULL, optarg, NULL, NULL, NULL, 0, appPolicies)) {
					exit(1);
				}
				policyStr = optarg;
				break;
			case 'P':
				/* this takes an additional argument */
				if(optind > (argc - 1)) {
					usage(argv);
				}
				if(appendConstraintToArray(optarg, argv[optind], NULL, NULL, NULL, 
						0, appPolicies)) {
					exit(1);
				}
				optind++;
				break;
			case 'e':
				if(appendConstraintToArray(NULL, "smime", optarg, NULL, NULL, 
						0, appPolicies)) {
					exit(1);
				}
				policyStr = "smime";
				break;
			case 'L':
				if(appendConstraintToArray(NULL, "ssl", optarg, NULL, NULL, 0, appPolicies)) {
					exit(1);
				}
				policyStr = "ssl";
				break;
			case 'r':
				if(appendConstraintToArray(NULL, NULL, NULL, NULL, optarg, 0, appPolicies)) {
					exit(1);
				}
				resultStr = optarg;
				break;
			case 'w':
			{
				SInt32 l = atol(optarg);
				if(appendConstraintToArray(NULL, NULL, NULL, &l, "unspecified", 0, appPolicies)) {
					exit(1);
				}
				break;
			}
			case 'W':
			{
				/* this takes an additional argument */
				if(optind > (argc - 1)) {
					usage(argv);
				}
				SInt32 l = atol(optarg);
				if(appendConstraintToArray(NULL, argv[optind], NULL, &l, "unspecified", 0,
						appPolicies)) {
					exit(1);
				}
				optind++;
				break;
			}
			case 'u':
			{
				SInt32 l = atol(optarg);
				SecTrustSettingsKeyUsage ku = (SecTrustSettingsKeyUsage)l;
				if(appendConstraintToArray(NULL, NULL, NULL, NULL, NULL, ku, appPolicies)) {
					exit(1);
				}
				break;
			}
			case 'k':
				kcName = optarg;
				break;
			case 'R':
				really = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'A':
				addToKeychain = true;
				break;
			case 'l':
				loopPause = true;
				break;
			case '2':
				userTrustLegacy = true;
				break;
			case 'U':
				userTrust = true;
				break;
			default:
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	if(startCFRunLoop()) {
		/* enable reception of KC event messages */
		exit(1);
	}

	/* give that thread a chance right now */
	while(!runLoopInitialized) {
		usleep(1000);
	};

	int ortn = 0;
	do {
		switch(argv[1][0]) {
			case 'a':
				if(certRef == NULL) {
					printf("You must supply a cert.\n");
					usage(argv);
				}
				if(settingsFileIn) {
					if(!settingsFileOut) {
						printf("Modifying trust settings as file requires output file\n");
						return -1;
					}
					settingsIn = readFileCFData(settingsFileIn);
					if(!settingsIn) {
						return -1;
					}
				}
				if(userTrustLegacy || userTrust) {
					ortn = addCertLegacy(certRef, policyStr, resultStr, userTrustLegacy);
				}
				else {
					ortn = addCert(certRef, domain, addToKeychain, kcName, appPolicies,
						settingsIn, &settingsOut);
					if((ortn == noErr) && (settingsOut != NULL)) {
						unsigned len = CFDataGetLength(settingsOut);
						if(writeFile(settingsFileOut, CFDataGetBytePtr(settingsOut), len)) {
							printf("***Error writing settings to %s\n", settingsFileOut);
						}
						else {
							printf("...wrote %u bytes to %s\n", len, settingsFileOut);
						}
					}
				}
				if(settingsIn) {
					CFRelease(settingsIn);
				}
				if(settingsOut) {
					CFRelease(settingsOut);
				}
				break;
			case 'p':
				ortn = fetchParseTrustRecord(domain, settingsFileIn);
				break;
			case 'r':
				ortn = copyCertsAndDisplay(verbose, domain);
				break;
			case 'd':
				ortn = deleteCerts(domain, false);
				break;
			case 'D':
				if(!really) {
					printf("I do not believe you. Specify -D option to delete all roots.\n");
					exit(1);
				}
				ortn = deleteCerts(domain, true);
				break;
			case 'R':
				ortn = addCertLegacy(certRef, policyStr, "unspecified", true);
				break;
			default:
				usage(argv);
		}
		if(loopPause) {
			fpurge(stdin);
			printf("Pausing for MallocDebug. Hit CR to continue: ");
			fflush(stdout);
			getchar();
		}
	} while(loopPause);
	return (int)ortn;
}
