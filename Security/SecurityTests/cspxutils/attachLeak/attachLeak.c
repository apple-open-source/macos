/* Copyright (c) 1999,2003-2004,2008 Apple Inc.
 *
 * attachLeak.c - analyze memory leaks from CSSM_Init/ModuleAttach.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"

static CSSM_API_MEMORY_FUNCS memFuncs = {
	appMalloc,
	appFree,
	appRealloc,
 	appCalloc,
 	NULL
 };

static CSSM_VERSION vers = {2, 0};

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options: \n");
	printf("   l (do ModuleLoad on each loop)\n");
	printf("   u (do Module{Load,Unload} on each loop)\n");
	printf("   d (CSP/DL; default = bare CSP)\n");
	printf("   t (TP)\n");
	printf("   c (CL)\n");
	exit(1);
}

static int doPause(const char *state)
{
	char resp;

	fpurge(stdin);
	printf("%s\n", state);
	printf("q to abort, anything else to continue: ");
	resp = getchar();
	return(resp == 'q');
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	CSSM_HANDLE 		modHand = 0;
	void 				*foo;
	CSSM_SERVICE_TYPE	svcType = CSSM_SERVICE_CSP;
	const CSSM_GUID		*guid = &gGuidAppleCSP;
	const char			*modName = "AppleCSP";
	CSSM_RETURN			crtn;
	CSSM_BOOL			doLoad = CSSM_FALSE;
	CSSM_BOOL			doUnload = CSSM_FALSE;
	
	/* force link against malloc */
	foo = malloc(1);
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'l':
				doLoad = CSSM_TRUE;
				break;
			case 'u':
				doLoad = doUnload = CSSM_TRUE;
				break;
			case 'd':
				guid = &gGuidAppleCSPDL;
				modName = "AppleCSPDL";
				break;
			case 'c':
				guid = &gGuidAppleX509CL;
				svcType = CSSM_SERVICE_CL;
				modName = "AppleX509CL";
				break;
			case 't':
				guid = &gGuidAppleX509TP;
				svcType = CSSM_SERVICE_TP;
				modName = "AppleX509TP";
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	
	if(doPause("Top of test")) {
		goto done;
	}
	
	/* CSSM init, just once */
	if(!cssmStartup()) {
		printf("Oops, error starting up CSSM\n");
		exit(1);
	}
	if(doPause("CSSM initialized")) {
		goto done;
	}
		
	
	if(!doLoad) {
		/* load, just once */
		crtn = CSSM_ModuleLoad(guid,
			CSSM_KEY_HIERARCHY_NONE,
			NULL,			// eventHandler
			NULL);			// AppNotifyCallbackCtx
		if(crtn) {
			printf("Error loading %s\n", modName);
			printError("CSSM_ModuleLoad", crtn);
			return 0;
		}
		if(doPause("CSSM_ModuleLoad() complete")) {
			goto done;
		}
	}
	while(1) {
		if(doLoad) {
			/* load, each time */
			crtn = CSSM_ModuleLoad(guid,
				CSSM_KEY_HIERARCHY_NONE,
				NULL,			// eventHandler
				NULL);			// AppNotifyCallbackCtx
			if(crtn) {
				printf("Error loading %s\n", modName);
				printError("CSSM_ModuleLoad", crtn);
				return 0;
			}
			if(doPause("CSSM_ModuleLoad() complete")) {
				break;
			}
		}
		crtn = CSSM_ModuleAttach (guid,
			&vers,
			&memFuncs,				// memFuncs
			0,						// SubserviceID
			svcType,			
			0,						// AttachFlags
			CSSM_KEY_HIERARCHY_NONE,
			NULL,					// FunctionTable
			0,						// NumFuncTable
			NULL,					// reserved
			&modHand);
		if(crtn) {
			printf("Error attaching to %s\n", modName);
			printError("CSSM_ModuleAttach", crtn);
			return 0;
		}
		if(doPause("ModuleAttach() complete")) {
			break;
		}
		CSSM_ModuleDetach(modHand);
		modHand = 0;
		if(doPause("ModuleDetach() complete")) {
			break;
		}
		if(doUnload) {
			/* unload, each time */
			crtn = CSSM_ModuleUnload(guid, NULL, NULL);
			if(crtn) {
				printf("Error unloading %s\n", modName);
				printError("CSSM_ModuleUnload", crtn);
				return 0;
			}
			if(doPause("ModuleUnload() complete")) {
				break;
			}
		}	/* unloading */
	}		/* main loop */

done:
	fpurge(stdin);
	if(modHand) {
		CSSM_ModuleDetach(modHand);
		printf("Final detach complete; cr to exit: ");
	}
	else {
		printf("Test complete; cr to exit: ");
	}
	getchar();
	return 0;
}
