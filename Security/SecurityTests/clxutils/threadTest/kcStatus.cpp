/*
 * kcStatus.cpp
 *
 * Open default keychain; get status to actually instatiate the thing; release.
 */
#include "testParams.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Security/Security.h>

/* for malloc debug */
#define DO_PAUSE	0

/* for tracking down KC crasher */
#define ABORT_ON_ERROR		1

int kcStatusInit(TestParams *testParams)
{
	return 0;
}


int kcStatus(TestParams *testParams)
{
	OSStatus			ortn;
	SecKeychainRef		kcRef;
	unsigned			loopNum;
	SecKeychainStatus	kcStatus;
	
	for(loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		if(testParams->verbose) {
			printf("kcStatus loop %d\n", loopNum);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		
		ortn = SecKeychainCopyDefault(&kcRef);
		if(ortn) {
			cssmPerror("SecKeychainCopyDefault", ortn);
			if(ABORT_ON_ERROR) {
				exit(1);
			}
			else {
				return (int)ortn;
			}
		}
		ortn = SecKeychainGetStatus(kcRef, &kcStatus);
		if(ortn) {
			cssmPerror("SecKeychainGetStatus", ortn);
			printf("***HEY! A keychain obtained via SecKeychainCopyDefault() resulted\n");
			printf("   in a failed SecKeychainGetStatus()! You really need to fix this!\n");
			printf("kcRef %p\n", kcRef);
			char path[300];
			UInt32 len = 300;
			  ortn = SecKeychainGetPath(kcRef, &len, path);
			if(ortn) {
			  cssmPerror("SecKeychainGetPath", ortn);
			}
			else {
			  printf("kc path %s\n", path);
			}
			if(ABORT_ON_ERROR) {
				exit(1);
			}
			else {
				return (int)ortn;
			}
		}
		
		CFRelease(kcRef);

		#if	DO_PAUSE
		fpurge(stdin);
		printf("Hit CR to continue: ");
		getchar();
		#endif
	}	/* outer loop */
	#if HOLD_SEARCH_LIST
	CFRelease(sl);
	#endif
	return 0;
}
