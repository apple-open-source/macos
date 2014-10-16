/*
 * cmsTime.cpp - measure performance of CMS decode & verify
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/CMSDecoder.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>

#define LOOPS_DEF			100
#define SIGNED_FILE			"noRoot.p7"

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  -l loops        -- loops; default %d; 0=forever\n", LOOPS_DEF);
	printf("  -i inFile       -- input file; default is %s\n", SIGNED_FILE);
	printf("  -K              -- set empty KC list\n");
	/* etc. */
	exit(1);
}

/* perform one CMS decode */
static OSStatus doDecode(
	const void *cmsData,
	size_t cmsDataLen,
	SecPolicyRef policyRef,
	CFArrayRef kcArray)			/* optional */

{
	OSStatus ortn;
	CMSDecoderRef cmsDecoder = NULL;

	CMSDecoderCreate(&cmsDecoder);
	if(kcArray) {
		ortn = CMSDecoderSetSearchKeychain(cmsDecoder, kcArray);
		if(ortn) {
			cssmPerror("CMSDecoderSetSearchKeychain", ortn);
			return ortn;
		}
	}
	ortn = CMSDecoderUpdateMessage(cmsDecoder, cmsData, cmsDataLen);
	if(ortn) {
		cssmPerror("CMSDecoderUpdateMessage", ortn);
		return ortn;
	}
	ortn = CMSDecoderFinalizeMessage(cmsDecoder);
	if(ortn) {
		cssmPerror("CMSDecoderFinalizeMessage", ortn);
		return ortn;
	}

	CMSSignerStatus signerStatus;
	ortn = CMSDecoderCopySignerStatus(cmsDecoder, 0, policyRef, true, &signerStatus, NULL, NULL);
	if(ortn) {
		cssmPerror("CMSDecoderCopySignerStatus", ortn);
		return ortn;
	}
	if(signerStatus != kCMSSignerValid) {
		printf("***Bad signerStatus (%d)\n", (int)signerStatus);
		ortn = -1;
	}
	CFRelease(cmsDecoder);
	return ortn;
}

int main(int argc, char **argv)
{
	unsigned dex;

	CFArrayRef 			emptyKCList = NULL;
	unsigned char 		*blob = NULL;
	unsigned	 		blobLen;
	SecPolicyRef      	policyRef = NULL;

	/* user-spec'd variables */
	unsigned loops = LOOPS_DEF;
	char *blobFile = SIGNED_FILE;
	bool emptyList = false;			/* specify empty KC list */

	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "l:i:Kh")) != -1) {
		switch (arg) {
			case 'l':
				loops = atoi(optarg);
				break;
			case 'i':
				blobFile = optarg;
				break;
			case 'K':
				emptyList = true;
				emptyKCList = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	
	if(readFile(blobFile, &blob, &blobLen)) {
		printf("***Error reading %s\n", blobFile);
		exit(1);
	}
	/* cook up reusable policy object */
	SecPolicySearchRef	policySearch = NULL;
	OSStatus ortn = SecPolicySearchCreate(CSSM_CERT_X_509v3,
		&CSSMOID_APPLE_X509_BASIC,
		NULL,				// policy opts
		&policySearch);
	if(ortn) {
		cssmPerror("SecPolicySearchCreate", ortn);
		exit(1);
	}
	ortn = SecPolicySearchCopyNext(policySearch, &policyRef);
	if(ortn) {
		cssmPerror("SecPolicySearchCopyNext", ortn);
		exit(1);
	}
	CFRelease(policySearch);

	CFAbsoluteTime startTimeFirst;
	CFAbsoluteTime endTimeFirst;
	CFAbsoluteTime startTimeMulti;
	CFAbsoluteTime endTimeMulti;

	/* GO */
	startTimeFirst = CFAbsoluteTimeGetCurrent();
	if(doDecode(blob, blobLen, policyRef, emptyKCList)) {
		exit(1);
	}
	endTimeFirst = CFAbsoluteTimeGetCurrent();

	startTimeMulti = CFAbsoluteTimeGetCurrent();
	for(dex=0; dex<loops; dex++) {
		if(doDecode(blob, blobLen, policyRef, emptyKCList)) {
			exit(1);
		}
	}
	endTimeMulti = CFAbsoluteTimeGetCurrent();
	CFTimeInterval elapsed = endTimeMulti - startTimeMulti;

	printf("First decode = %4.1f ms\n", (endTimeFirst - startTimeFirst) * 1000.0);
	printf("Next decodes = %4.2f ms/op (%f s total for %u loops)\n",
		elapsed * 1000.0 / loops, elapsed, loops);

	return 0;
}
