/*
 * kcTime.cpp - measure performance of keychain ops
 */
 
#include <Security/Security.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <utilLib/common.h>
#include <utilLib/cputime.h>
#include <Security/SecImportExport.h>

/* Hard coded test params */

/*
 * Number of keychains to create/search 
 */
#define KT_NUM_KEYCHAINS		10

/*
 * Certs and P12 blobs to add to keychain
 */
#define KT_CERT0_NAME			"amazon_v3.100.cer"
#define KT_CERT1_NAME			"SecureServer.509.cer"
#define KT_P12_PFX				"test1.p12"
#define KT_P12_PASSWORD			"password"

/*
 * Base name of keychains we create. We delete them before we start.
 * They're in the user's home directory to faciliate testing NFS vs. local. 
 */
#define KT_KC_NAME				"kcTime_test_"

static void usage(char **argv)
{
    printf("Usage: %s [option ...]\n", argv[0]);
    printf("Options:\n");
	printf("   v   verbose\n");
	printf("   h   help\n");
	exit(1);
}

static void printAllTimes(
	bool verbose,
	double *delta,
	unsigned numSamples)
{
	if(!verbose) {
		return;
	}
	for(unsigned dex=0; dex<numSamples; dex++) {
		printf("   sample[%u] %8.4f\n", dex, delta[dex]);
	}
}

static void printSecErr(
	const char *op,
	OSStatus ortn)
{
	cssmPerror(op, ortn);
}

int main(int argc, char **argv)
{
	SecKeychainRef		kcRefs[KT_NUM_KEYCHAINS];
	OSStatus 			ortn;
	int 				arg;
	char				*argp;
	char				kcName[KT_NUM_KEYCHAINS][80];
	unsigned			dex;
	unsigned char		*cert0Data;
	unsigned			cert0Len;
	unsigned char		*cert1Data;
	unsigned			cert1Len;
	SecCertificateRef	certRef0[KT_NUM_KEYCHAINS];
	SecCertificateRef	certRef1[KT_NUM_KEYCHAINS];
	CFArrayRef			savedSearchList;
	CFMutableArrayRef	ourSearchList;
	int					irtn;
	CPUTime				startTime, endTime;
	double				deltaMs[KT_NUM_KEYCHAINS * 2];
	double				deltaMs2[KT_NUM_KEYCHAINS];
	double				deltaMs3[KT_NUM_KEYCHAINS];
	SecKeychainSearchRef srchRef;
	CFStringRef			p12Pwd;
	CFDataRef			p12Data;
	bool				verbose = false;
	
	memset(kcRefs, 0, sizeof(SecKeychainRef) * KT_NUM_KEYCHAINS);
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'v':
				verbose = true;
				break;
			default:
				usage(argv);
		}
	}

	printf("Starting kcTime test using %d keychains\n", KT_NUM_KEYCHAINS);
	
	/* generate keychain names */
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		sprintf(kcName[dex], "%s%d", KT_KC_NAME, dex);
	}
	
	/* delete existing keychains we created in the past */
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		/* 
		 * It may or may not exist, and may or may not be in the search 
		 * list. Brute force and ignore errors.
		 */
		SecKeychainRef kc;
		ortn = SecKeychainOpen(kcName[dex], &kc);
		if(ortn == noErr) {
			SecKeychainDelete(kc);
		}
		
		/* brute force UNIX file delete too */
		char *userHome = getenv("HOME");
		char fullPath[1024];
		if(userHome == NULL) {
			userHome = (char *)"";
		}
		sprintf(fullPath, "%s/Library/Keychains/%s", 
			userHome, kcName[dex]);
		unlink(fullPath);
	}
	
	/* read in certificate data */
	irtn = readFile(KT_CERT0_NAME, &cert0Data, &cert0Len);
	if(irtn) {
		printf("I cannot find file %s in cwd.\n", KT_CERT0_NAME);
		exit(1);
	}
	irtn = readFile(KT_CERT1_NAME, &cert1Data, &cert1Len);
	if(irtn) {
		printf("I cannot find file %s in cwd.\n", KT_CERT1_NAME);
		exit(1);
	}

	/* save KC search list; we'll restore it at end */
	ortn = SecKeychainCopySearchList(&savedSearchList);
	if(ortn) {
		printSecErr("SecKeychainCopySearchList", ortn);
		return 1;
	}
	/* subsequent errors to errOut: to restore searchList! */
	
	/* TIME: create n keychains */
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		unsigned pwdLen = strlen(kcName[dex]);
		startTime = CPUTimeRead();
		ortn = SecKeychainCreate(kcName[dex],
			pwdLen,
			kcName[dex],
			false,		// promptUser,	
			nil,		// initialAccess
			&kcRefs[dex]);
		endTime = CPUTimeRead();
		if(ortn) {
			printSecErr("SecKeychainCreate", ortn);
			goto errOut;
		}
		deltaMs[dex] = CPUTimeDeltaMs(startTime, endTime);
	}
	printf("Create     : %7.3f ms per op\n",
		CPUTimeAvg(deltaMs, KT_NUM_KEYCHAINS));
	printAllTimes(verbose, deltaMs, KT_NUM_KEYCHAINS);
	
	/* set KC search list to only the ones we just created */
	ourSearchList = CFArrayCreateMutable(NULL, KT_NUM_KEYCHAINS, NULL);
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		CFArrayInsertValueAtIndex(ourSearchList, dex, kcRefs[dex]);
	}
	ortn = SecKeychainSetSearchList(ourSearchList);
	if(ortn) {
		printSecErr("SecKeychainSetSearchList", ortn);
		goto errOut;
	}
	CFRelease(ourSearchList);
	
	/* TIME: close all of them */
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		startTime = CPUTimeRead();
		CFRelease(kcRefs[dex]);
		endTime = CPUTimeRead();
		deltaMs[dex] = CPUTimeDeltaMs(startTime, endTime);
	}
	printf("Close      : %7.3f ms per op\n",
		CPUTimeAvg(deltaMs, KT_NUM_KEYCHAINS));
	printAllTimes(verbose, deltaMs, KT_NUM_KEYCHAINS);
	
	/* TIME: open all of them */
	/* This is so fast that we just measure the total time */
	startTime = CPUTimeRead();
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		ortn = SecKeychainOpen(kcName[dex], &kcRefs[dex]);
		if(ortn) {
			printSecErr("SecKeychainOpen", ortn);
			goto errOut;
		}
	}
	endTime = CPUTimeRead();
	deltaMs[0] = CPUTimeDeltaMs(startTime, endTime);
	printf("Open       : %7.3f ms per op\n",
		deltaMs[0] / KT_NUM_KEYCHAINS);
	if(verbose) {
		printf("   total time %7.3f ms\n", deltaMs[0]);
	}
	
	/* TIME: get status on all of them */
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		SecKeychainStatus status;
		startTime = CPUTimeRead();
		ortn = SecKeychainGetStatus(kcRefs[dex], &status);
		endTime = CPUTimeRead();
		if(ortn) {
			printSecErr("SecKeychainGetStatus", ortn);
			goto errOut;
		}
		deltaMs[dex] = CPUTimeDeltaMs(startTime, endTime);
	}
	printf("Get Status : %7.3f ms per op\n",
		CPUTimeAvg(deltaMs, KT_NUM_KEYCHAINS));
	printAllTimes(verbose, deltaMs, KT_NUM_KEYCHAINS);
	
	/* TIME: unlock each keychain */
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		unsigned pwdLen = strlen(kcName[dex]);
		startTime = CPUTimeRead();
		ortn = SecKeychainUnlock(kcRefs[dex],
			pwdLen,	kcName[dex], true);
		endTime = CPUTimeRead();
		if(ortn) {
			printSecErr("SecKeychainUnlock", ortn);
			goto errOut;
		}
		deltaMs[dex] = CPUTimeDeltaMs(startTime, endTime);
	}
	printf("Unlock     : %7.3f ms per op\n",
		CPUTimeAvg(deltaMs, KT_NUM_KEYCHAINS));
	printAllTimes(verbose, deltaMs, KT_NUM_KEYCHAINS);
	
	/* TIME: create two SecCertificateRefs for each KC */
	/* this is fast, just measure total time */
	startTime = CPUTimeRead();
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		CSSM_DATA cdata = {cert0Len, cert0Data};
		ortn = SecCertificateCreateFromData(&cdata,
			CSSM_CERT_X_509v3,
			CSSM_CERT_ENCODING_DER,
			&certRef0[dex]);
		if(ortn) {
			printSecErr("SecCertificateCreateFromData", ortn);
			goto errOut;
		}
		cdata.Length = cert1Len;
		cdata.Data = cert1Data;
		ortn = SecCertificateCreateFromData(&cdata,
			CSSM_CERT_X_509v3,
			CSSM_CERT_ENCODING_DER,
			&certRef1[dex]);
		if(ortn) {
			printSecErr("SecCertificateCreateFromData", ortn);
			goto errOut;
		}
	}
	endTime = CPUTimeRead();
	deltaMs[0] = CPUTimeDeltaMs(startTime, endTime);
	/* we created KT_NUM_KEYCHAINS*2 certs in KT_NUM_KEYCHAINS samples */
	printf("SecCertificateCreateFromData  : %7.3f ms per op\n",
		deltaMs[0] / (KT_NUM_KEYCHAINS * 2.0));
	if(verbose) {
		printf("   total time %7.3f ms\n", deltaMs[0]);
	}
	free(cert0Data);
	free(cert1Data);
	
	/* TIME: add two certs to each keychain */
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		startTime = CPUTimeRead();
		ortn = SecCertificateAddToKeychain(certRef0[dex], kcRefs[dex]);
		if(ortn) {
			printSecErr("SecCertificateAddToKeychain", ortn);
			goto errOut;
		}
		ortn = SecCertificateAddToKeychain(certRef1[dex], kcRefs[dex]);
		endTime = CPUTimeRead();
		if(ortn) {
			printSecErr("SecCertificateAddToKeychain", ortn);
			goto errOut;
		}
		deltaMs[dex] = CPUTimeDeltaMs(startTime, endTime);
	}
	/* we added KT_NUM_KEYCHAINS*2 certs in KT_NUM_KEYCHAINS samples */
	printf("SecCertificateAddToKeychain   : %7.3f ms per op\n",
		CPUTimeAvg(deltaMs, KT_NUM_KEYCHAINS) / 2.0);
	printAllTimes(verbose, deltaMs, KT_NUM_KEYCHAINS);
	
	/* free the certRefs */
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		CFRelease(certRef0[dex]);
		CFRelease(certRef1[dex]);
	}
	
	/* 
	Ê* Three TIMES: 
	 *  -- search for the first cert
	 *  -- search for the next one 
	 *  -- search once more to end (not found)
	 */
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		SecKeychainItemRef certRef = NULL;
		
		/* step 1. set up search, get first item */
		startTime = CPUTimeRead();
		ortn = SecKeychainSearchCreateFromAttributes(kcRefs[dex],
			kSecCertificateItemClass,
			NULL,		// no attrs
			&srchRef);
		if(ortn) {
			printSecErr("SecKeychainSearchCreateFromAttributes", ortn);
			goto errOut;
		}
		ortn = SecKeychainSearchCopyNext(srchRef, &certRef);
		/* might be necessary to actually bring in the cert */
		if(SecCertificateGetTypeID() != CFGetTypeID(certRef)) {
			printf("***Unexpected CFType on cert search\n");
			goto errOut;
		}
		endTime = CPUTimeRead();
		if(ortn) {
			printSecErr("SecKeychainSearchCopyNext", ortn);
			goto errOut;
		}
		deltaMs[dex] = CPUTimeDeltaMs(startTime, endTime);
		CFRelease(certRef);
		
		/* step 2. get next item */
		startTime = CPUTimeRead();
		ortn = SecKeychainSearchCopyNext(srchRef, &certRef);
		if(SecCertificateGetTypeID() != CFGetTypeID(certRef)) {
			printf("***Unexpected CFType on cert search\n");
			goto errOut;
		}
		endTime = CPUTimeRead();
		if(ortn) {
			printSecErr("SecKeychainSearchCreateFromAttributes", ortn);
			goto errOut;
		}
		deltaMs2[dex] = CPUTimeDeltaMs(startTime, endTime);
		CFRelease(certRef);
		
		/* step 3. one more search for nonexistent item */
		startTime = CPUTimeRead();
		ortn = SecKeychainSearchCopyNext(srchRef, &certRef);
		endTime = CPUTimeRead();
		if(ortn != errSecItemNotFound) {
			if(ortn == noErr) {
				printf("***SecKeychainSearchCopyNext got noErr, "
					"expected notFound\n");
			}
			else {
				printSecErr("SecKeychainSearchCopyNext", ortn);
			}
			goto errOut;
		}
		deltaMs3[dex] = CPUTimeDeltaMs(startTime, endTime);
		CFRelease(srchRef);
	}
	printf("SecKeychainSearch first item  : %7.3f ms per op\n",
		CPUTimeAvg(deltaMs, KT_NUM_KEYCHAINS));
	printAllTimes(verbose, deltaMs, KT_NUM_KEYCHAINS);
	printf("SecKeychainSearch next item   : %7.3f ms per op\n",
		CPUTimeAvg(deltaMs2, KT_NUM_KEYCHAINS));
	printAllTimes(verbose, deltaMs2, KT_NUM_KEYCHAINS);
	printf("SecKeychainSearch end of list : %7.3f ms per op\n",
		CPUTimeAvg(deltaMs3, KT_NUM_KEYCHAINS));
	printAllTimes(verbose, deltaMs3, KT_NUM_KEYCHAINS);
	
	/* 
	 * TIME: search all certs in all keychains 
	 * This search create is outside the loop....
	 */
	ortn = SecKeychainSearchCreateFromAttributes(NULL,
		kSecCertificateItemClass,
		NULL,		// no attrs
		&srchRef);
	if(ortn) {
		printSecErr("SecKeychainSearchCreateFromAttributes", ortn);
		goto errOut;
	}
	/* we have 2 certs per keychain, search all of them */
	for(dex=0; dex<KT_NUM_KEYCHAINS*2; dex++) {
		SecKeychainItemRef certRef = NULL;
		
		startTime = CPUTimeRead();
		ortn = SecKeychainSearchCopyNext(srchRef, &certRef);
		/* might be necessary to actually bring in the cert */
		if(SecCertificateGetTypeID() != CFGetTypeID(certRef)) {
			printf("***Unexpected CFType on cert search\n");
			goto errOut;
		}
		endTime = CPUTimeRead();
		deltaMs[dex] = CPUTimeDeltaMs(startTime, endTime);
		CFRelease(certRef);
	}
	printf("SecKeychainSearch all KCs     : %7.3f ms per op\n",
		CPUTimeAvg(deltaMs, KT_NUM_KEYCHAINS * 2));
	printAllTimes(verbose, deltaMs, KT_NUM_KEYCHAINS * 2);
	
	/* 
	 * TIME: import a p12 identity into each keychain 
	 *
	 * First, read the file
	 */
	irtn = readFile(KT_P12_PFX, &cert0Data, &cert0Len);
	if(irtn) {
		printf("I cannot find file %s in cwd.\n", KT_P12_PFX);
		exit(1);
	}
	p12Data = CFDataCreate(NULL, cert0Data, cert0Len);	
	p12Pwd = CFStringCreateWithCString(NULL, KT_P12_PASSWORD,
					kCFStringEncodingASCII);

	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		SecExternalFormat format = kSecFormatPKCS12;
		SecExternalItemType itemType = kSecItemTypeAggregate;
		SecKeyImportExportParameters keyParams;
		memset(&keyParams, 0, sizeof(keyParams));
		keyParams.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
		keyParams.flags = kSecKeyNoAccessControl;
		keyParams.passphrase = p12Pwd;
		
		keyParams.keyAttributes = CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_SENSITIVE | 
			CSSM_KEYATTR_EXTRACTABLE;
			
		startTime = CPUTimeRead();
		ortn = SecKeychainItemImport(p12Data,	
			NULL,		// fileNameOrExtension
			&format, &itemType,
			0,			// flags
			&keyParams,
			kcRefs[dex],
			NULL);		// outItems
		if(ortn) {
			printSecErr("SecKeychainItemImport(p12)", ortn);
			goto errOut;
		}
		endTime = CPUTimeRead();
		deltaMs[dex] = CPUTimeDeltaMs(startTime, endTime);
	}
	CFRelease(p12Data);
	CFRelease(p12Pwd);
	printf("P12 Import                    : %7.3f ms per op\n",
		CPUTimeAvg(deltaMs, KT_NUM_KEYCHAINS));
	printAllTimes(verbose, deltaMs, KT_NUM_KEYCHAINS);
	
	/* TIME: identity search in each keychain */
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		SecIdentitySearchRef idSrch;
		SecIdentityRef idRef;
		startTime = CPUTimeRead();
		ortn = SecIdentitySearchCreate(kcRefs[dex], 0, &idSrch);
		if(ortn) {
			printSecErr("SecIdentitySearchCreate", ortn);
			goto errOut;
		}
		ortn = SecIdentitySearchCopyNext(idSrch, &idRef);
		endTime = CPUTimeRead();
		if(ortn) {
			printSecErr("SecIdentitySearchCopyNext", ortn);
			goto errOut;
		}
		deltaMs[dex] = CPUTimeDeltaMs(startTime, endTime);
		CFRelease(idSrch);
		CFRelease(idRef);
	}
	printf("SecIdentity search            : %7.3f ms per op\n",
		CPUTimeAvg(deltaMs, KT_NUM_KEYCHAINS));
	printAllTimes(verbose, deltaMs, KT_NUM_KEYCHAINS);
	
errOut:
	ortn = SecKeychainSetSearchList(savedSearchList);
	if(ortn) {
		printSecErr("SecKeychainSetSearchList", ortn);
	}
	
	/* delete all the keychains we created */
	for(dex=0; dex<KT_NUM_KEYCHAINS; dex++) {
		if(kcRefs[dex] != NULL) {
			SecKeychainDelete(kcRefs[dex]);
		}
	}

	/* other cleanup if anyone cares */
	return 0;
}
