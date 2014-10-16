/*
 * attach/creatContext/deleteContext/detach test
 */
#include "testParams.h"
#include <Security/cssm.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>	

/* for memory leak debug only, with only one thread running */
#define DO_PAUSE			0

#define ATTACHES_PER_LOOP	100
#define ATTACH_EACH_LOOP	1		/* 1 ==> ATTACHES_PER_LOOP moduleAttaches */
									/* 0 ==> all thru testParams->cspHand */
#define CONTEXT_EACH_LOOP	1		/* 1 ==> CSSM_CSP_Create*Context for each attach */
									/* 0 ==> just do attach/detach */
#define CSPDL_ENABLE		1		/* attach to CSP and DL sides of CSPDL */
#define DO_UNLOAD           0       /* enable CSSM_ModuleUnload() */

#if		(!ATTACH_EACH_LOOP && !CONTEXT_EACH_LOOP)
#error	Hey! Must configure for attach and/or createContext!
#endif

static CSSM_API_MEMORY_FUNCS memFuncs = {
	appMalloc,
	appFree,
	appRealloc,
 	appCalloc,
 	NULL
 };

int attachTestInit(TestParams *testParams)
{
	/* nothing for now */
	return 0;
}

static uint8 bogusKeyBits[] = {0, 1, 2, 3};
static CSSM_VERSION vers = {2, 0};

static CSSM_RETURN attachMod(
	const CSSM_GUID *guid,
	CSSM_SERVICE_TYPE svc,
	CSSM_MODULE_HANDLE_PTR hand)
{
	CSSM_RETURN crtn = CSSM_ModuleLoad(guid,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		return crtn;
	}
	return CSSM_ModuleAttach(guid,
		&vers,
		&memFuncs,					// memFuncs
		0,							// SubserviceID
		svc,						// SubserviceFlags 
		0,							// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,						// FunctionTable
		0,							// NumFuncTable
		NULL,						// reserved
		hand);
}

static int detachUnload(
	CSSM_HANDLE hand,
	const CSSM_GUID *guid)
{
	CSSM_RETURN crtn = CSSM_ModuleDetach(hand);
	if(crtn) {
		cssmPerror("CSSM_ModuleDetach", crtn);
		return crtn;
	}
    #if DO_UNLOAD
	crtn = CSSM_ModuleUnload(guid, NULL, NULL);
	if(crtn) {
		cssmPerror("CSSM_ModuleUnload", crtn);
	}
    #endif
	return crtn;
}

int attachTest(TestParams *testParams)
{
	unsigned 			loop;
	CSSM_RETURN 		crtn;
	CSSM_CSP_HANDLE		cspHand[ATTACHES_PER_LOOP];
	CSSM_CL_HANDLE		clHand[ATTACHES_PER_LOOP];
	CSSM_TP_HANDLE		tpHand[ATTACHES_PER_LOOP];
	#if CSPDL_ENABLE
	CSSM_DL_HANDLE		dlHand[ATTACHES_PER_LOOP];
	CSSM_CSP_HANDLE		cspDlHand[ATTACHES_PER_LOOP];
	#endif
	CSSM_CC_HANDLE		ccHand[ATTACHES_PER_LOOP];
	unsigned			dex;
	CSSM_KEY			bogusKey;
	
	memset(cspHand, 0, ATTACHES_PER_LOOP * sizeof(CSSM_CSP_HANDLE));
	memset(ccHand,  0, ATTACHES_PER_LOOP * sizeof(CSSM_CC_HANDLE));

	/* set up a bogus key which the CSP won't even see */
	memset(&bogusKey, 0, sizeof(CSSM_KEY));
	bogusKey.KeyData.Data = bogusKeyBits;
	bogusKey.KeyData.Length = sizeof(bogusKeyBits);

	for(loop=0; loop<testParams->numLoops; loop++) {
		if(testParams->verbose) {
			printf("attachTest thread %d: loop %d\n", 
				testParams->threadNum, loop);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		
		if(ATTACH_EACH_LOOP) {
			/* 'n' attaches, skipping load (which has known leaks) */
			for(dex=0; dex<ATTACHES_PER_LOOP; dex++) {
				crtn = attachMod(&gGuidAppleCSP, 
					CSSM_SERVICE_CSP, &cspHand[dex]);
				if(crtn) {
					printError("CSSM_ModuleAttach(CSP)", crtn);
					return 1;
				}
				crtn = attachMod(&gGuidAppleX509CL, 
					CSSM_SERVICE_CL, &clHand[dex]);
				if(crtn) {
					printError("CSSM_ModuleAttach(CL)", crtn);
					return 1;
				}
				crtn = attachMod(&gGuidAppleX509TP, 
					CSSM_SERVICE_TP, &tpHand[dex]);
				if(crtn) {
					printError("CSSM_ModuleAttach(TP)", crtn);
					return 1;
				}
				#if CSPDL_ENABLE
				crtn = attachMod(&gGuidAppleCSPDL, 
					CSSM_SERVICE_DL, &dlHand[dex]);
				if(crtn) {
					printError("CSSM_ModuleAttach(DL)", crtn);
					return 1;
				}
				crtn = attachMod(&gGuidAppleCSPDL, 
					CSSM_SERVICE_CSP, &cspDlHand[dex]);
				if(crtn) {
					printError("CSSM_ModuleAttach(CSPDL)", crtn);
					return 1;
				}
				#endif
			}
		}
		
		/* now one of various crypt handles */
		if(CONTEXT_EACH_LOOP) {
			for(dex=0; dex<ATTACHES_PER_LOOP; dex++) {
				CSSM_CSP_HANDLE curCspHand;
				if(ATTACH_EACH_LOOP) {
					curCspHand = cspHand[dex];
				}
				else {
					curCspHand = testParams->cspHand;
				}
				
				switch(dex & 3) {
					case 0:
						/* symmetric context */
						ccHand[dex] = genCryptHandle(curCspHand,
							CSSM_ALGID_DES,
							CSSM_ALGMODE_NONE,
							CSSM_PADDING_NONE,
							&bogusKey,
							NULL,			// key2
							NULL,			// IV
							0,				// effectiveKeySizeInBits
							0);				// rounds
						break;
					case 1:
						/* asymmetric context */
						ccHand[dex] = genCryptHandle(curCspHand,
							CSSM_ALGID_RSA,
							CSSM_ALGMODE_NONE,
							CSSM_PADDING_NONE,
							&bogusKey,
							NULL,			// key2
							NULL,			// IV
							0,				// effectiveKeySizeInBits
							0);				// rounds
						break;
					case 2:
						/* Digest */
						crtn = CSSM_CSP_CreateDigestContext(curCspHand,
							CSSM_ALGID_SHA1,
							&ccHand[dex]);
						if(crtn) {
							printError("CSSM_CSP_CreateDigestContext", crtn);
							ccHand[dex] = CSSM_INVALID_HANDLE;
						}
						break;
					case 3:
						/* Digest */
						crtn = CSSM_CSP_CreateSignatureContext(curCspHand,
							CSSM_ALGID_SHA1WithRSA,
							NULL,			// AccessCred
							&bogusKey,
							&ccHand[dex]);
						if(crtn) {
							printError("CSSM_CSP_CreateSignatureContext", crtn);
							ccHand[dex] = CSSM_INVALID_HANDLE;
						}
						break;
				}
				if(curCspHand == CSSM_INVALID_HANDLE) {
					return 1;
				}
			}
			
			/* free handles */
			for(dex=0; dex<ATTACHES_PER_LOOP; dex++) {
				crtn = CSSM_DeleteContext(ccHand[dex]);
				if(crtn) {
					printError("CSSM_DeleteContext", crtn);
					return 1;
				}
			}
		}
		
		if(ATTACH_EACH_LOOP) {
			/* detach */
			for(dex=0; dex<ATTACHES_PER_LOOP; dex++) {
				
				crtn = detachUnload(cspHand[dex], &gGuidAppleCSP);
				if(crtn) {
					return 1;
				}
				crtn = detachUnload(clHand[dex], &gGuidAppleX509CL);
				if(crtn) {
					return 1;
				}
				crtn = detachUnload(tpHand[dex], &gGuidAppleX509TP);
				if(crtn) {
					return 1;
				}
				#if CSPDL_ENABLE
				crtn = detachUnload(dlHand[dex], &gGuidAppleCSPDL);
				if(crtn) {
					return 1;
				}
				crtn = detachUnload(cspDlHand[dex], &gGuidAppleCSPDL);
				if(crtn) {
					return 1;
				}
				#endif
			}
		}
		randomDelay();
		
		#if DO_PAUSE
		fpurge(stdin);
		printf("Hit CR to proceed: ");
		getchar();
		#endif
	}
	return 0;
}

