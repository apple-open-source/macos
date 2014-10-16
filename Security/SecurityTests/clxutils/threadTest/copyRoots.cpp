/*
 * DER decode test
 */
#include "testParams.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>	
#include <Security/Security.h>
#include <Security/SecTrustSettingsPriv.h>
#include <stddef.h>
#include <unistd.h>

int copyRootsInit(
	TestParams *testParams)
{
    /* nothing for now */
    return 0;
}

int copyRootsTest(TestParams *testParams)
{
	for(unsigned loop=0; loop<testParams->numLoops; loop++) {
		if(testParams->verbose) {
			printf("derDecode thread %d: loop %d\n", 
				testParams->threadNum, loop);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
        
        CFArrayRef theArray = NULL;
        OSStatus ortn = SecTrustSettingsCopyQualifiedCerts(&CSSMOID_APPLE_TP_SSL,
            "localhost", 10,        // policyString
            CSSM_KEYUSE_ENCRYPT,    // wrong key use type but that's what ST passes
            &theArray);
        if(ortn) {
            cssmPerror("SecTrustSettingsCopyQualifiedCerts", ortn);
            return 1;
        }
        CFRelease(theArray);
    }
    return 0;
}
