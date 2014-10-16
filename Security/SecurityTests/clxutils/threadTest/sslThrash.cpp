/*
 * sslThrash.cpp
 *
 * track down multithread SecureTransport memory smasher - this test
 * does no network I/O
 */
#include "testParams.h"
#include <Security/cssm.h>
#include <utilLib/common.h>	
#include <utilLib/cspwrap.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/tpUtils.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <Security/SecureTransport.h>

#define DO_PAUSE	0

#define NUM_INNER_LOOPS			10


/*
 * Derive a symmetric CSSM_KEY from the specified raw key material.
 */
static CSSM_RETURN cdsaDeriveKey(
	CSSM_CSP_HANDLE		cspHandle,
	const void 			*rawKey,
	size_t				rawKeyLen,
	CSSM_ALGORITHMS		keyAlg,			// e.g., CSSM_ALGID_AES
	uint32				keySizeInBits,
	CSSM_KEY_PTR		key)
{
	CSSM_RETURN					crtn;
	CSSM_CC_HANDLE 				ccHand;
	CSSM_DATA					dummyLabel = {8, (uint8 *)"tempKey"};
	CSSM_DATA					saltData = {8, (uint8 *)"someSalt"};
	CSSM_PKCS5_PBKDF2_PARAMS 	pbeParams;
	CSSM_DATA					pbeData;
	CSSM_ACCESS_CREDENTIALS		creds;
	
	memset(key, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateDeriveKeyContext(cspHandle,
		CSSM_ALGID_PKCS5_PBKDF2,
		keyAlg,
		keySizeInBits,
		&creds,
		NULL,			// BaseKey
		1000,			// iterationCount, 1000 is the minimum
		&saltData,
		NULL,			// seed
		&ccHand);
	if(crtn) {
        cssmPerror("CSSM_CSP_CreateDeriveKeyContext", crtn);
		return crtn;
	}
	
	/* this is the caller's raw key bits, typically ASCII (though it
	 * could be anything) */
	pbeParams.Passphrase.Data = (uint8 *)rawKey;
	pbeParams.Passphrase.Length = rawKeyLen;
	/* The only PRF supported by the CSP is HMACSHA1 */
	pbeParams.PseudoRandomFunction = CSSM_PKCS5_PBKDF2_PRF_HMAC_SHA1;
	pbeData.Data = (uint8 *)&pbeParams;
	pbeData.Length = sizeof(pbeParams);
	crtn = CSSM_DeriveKey(ccHand,
		&pbeData,
		CSSM_KEYUSE_ANY,
		CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE,
		&dummyLabel,
		NULL,			// cred and acl
		key);
	CSSM_DeleteContext(ccHand);		// ignore error here
	if(crtn) {
        cssmPerror("CSSM_DeriveKey", crtn);
    }
	return crtn;
}

static CSSM_API_MEMORY_FUNCS memFuncs = {
	appMalloc,
	appFree,
	appRealloc,
 	appCalloc,
 	NULL
 };
static CSSM_VERSION vers = {2, 0};

/*
 * Initialize CDSA and attach to the CSP.
 */
static CSSM_RETURN cdsaCspAttach(
	CSSM_CSP_HANDLE		*cspHandle)
{
	CSSM_CSP_HANDLE cspHand;
	CSSM_RETURN		crtn;
	
	/* Load the CSP bundle into this app's memory space */
	crtn = CSSM_ModuleLoad(&gGuidAppleCSP,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		return crtn;
	}
	
	/* obtain a handle which will be used to refer to the CSP */ 
	crtn = CSSM_ModuleAttach (&gGuidAppleCSP,
		&vers,
		&memFuncs,			// memFuncs
		0,					// SubserviceID
		CSSM_SERVICE_CSP,	
		0,					// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,				// FunctionTable
		0,					// NumFuncTable
		NULL,				// reserved
		&cspHand);
	if(crtn) {
		return crtn;
	}
	*cspHandle = cspHand;
	return CSSM_OK;
}
	
static bool thrashInit = false;
static bool doSsl = true;
static bool doKey = true;
static bool doAttach = true;

int sslThrashInit(TestParams *testParams)
{
	if(thrashInit) {
		return 0;
	}

    char *opts = testParams->testOpts;
	if(opts == NULL) {
		thrashInit = true;
		return 0;
	}
    while(*opts != '\0') {
        switch(*opts) {
            case 'k':
                doKey = false;
                printf("...sslThrash: doKey disabled\n");
                break;
            case 's':
                doSsl = false;
                printf("...sslThrash: doSsl disabled\n");
                break;
            case 'a':
                doAttach = false;
                printf("...sslThrash: doAttach disabled\n");
                break;
            default:
                /* for other tests */
                break;
        }
        opts++;
    }
	thrashInit = true;
	return 0;
}

#define FAKE_SSL		0
#define SSL_CTX_SIZE	600
#define FAKE_DISPOSE	0

int sslThrash(TestParams *testParams)
{
	CSSM_RETURN		crtn;
	unsigned		loopNum;
	SSLContextRef	sslCtx;
	unsigned		dex;
	CSSM_KEY		ckey;
	CSSM_HANDLE		cspHand;
	
	for(loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		if(testParams->verbose) {
			printf("sslThrash loop %d\n", loopNum);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		
		for(dex=0; dex<NUM_INNER_LOOPS; dex++) {
			if(doAttach) {
				crtn = cdsaCspAttach(&cspHand);
				if(crtn) {
					printf("cdsaCspAttach error\n");
					return 1;
				}
			}
			if(doSsl) {
				#if	FAKE_SSL		
				sslCtx = (SSLContext *)malloc(SSL_CTX_SIZE);
				#else
				OSStatus ortn = SSLNewContext(false, &sslCtx);
				if(ortn) {
                    cssmPerror("SSLNewContext", ortn);
					printf("SSLNewContext error %d\n", (int)ortn);
					return 1;
				}
				#endif
			}
			if(doKey) {
				crtn = cdsaDeriveKey(testParams->cspHand,
					"some silly string",
					17,
					CSSM_ALGID_AES,
					128,
					&ckey);
				if(crtn) {
					printf("cdsaDeriveKey error\n");
					return 1;
				}
			}
			if(doSsl) {
				#if FAKE_SSL || FAKE_DISPOSE
				free(sslCtx);
				#else
				OSStatus ortn = SSLDisposeContext(sslCtx);
				if(ortn) {
                    cssmPerror("SSLDisposeContext", ortn);
					printf("SSLDisposeContext error %d\n", (int)ortn);
					return 1;
				}
				#endif
			}
			if(doKey) {
				crtn = CSSM_FreeKey(testParams->cspHand, 
						NULL,			// access cred
						&ckey,	
						CSSM_FALSE);
				if(crtn) {
                    cssmPerror("CSSM_FreeKey", crtn);
					printf("CSSM_FreeKey error\n");
					return 1;
				}
			}
			if(doAttach) {
				crtn = CSSM_ModuleDetach(cspHand);
				if(crtn) {
                    cssmPerror("CSSM_ModuleDetach", crtn);
					printf("CSSM_ModuleDetach error\n");
					return 1;
				}
			}
			randomDelay();

			/* leak debug */
			#if	DO_PAUSE
			fpurge(stdin);
			printf("Hit CR to continue: ");
			getchar();
			#endif
		}	/* inner loop */
		
	}	/* outer loop */
	return 0;
}
