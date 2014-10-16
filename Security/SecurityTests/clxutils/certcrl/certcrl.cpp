/*
 * certcrl - generic cert/CRL verifier
 */
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>
#include <clAppUtils/clutils.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Security/cssm.h>
#include <clAppUtils/BlobList.h>
#include <clAppUtils/certVerify.h>
#include "script.h"

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("   -c certFileName [...]\n");
	printf("   -C rootCertFileName [...]\n");
	printf("   -r crlFileName [...]\n");
	printf("   -d certDbName\n");
	printf("   -D crlDlDbName\n");
	printf("   -s (use system anchor certs)\n");
	printf("   -g (use Trust Settings)\n");
	printf("   -i (implicit anchors)\n");
	printf("   -l=loopCount (default = 1)\n");
	printf("   -f (leaf cert is a CA)\n");
	printf("   -w(rite CRLs to dlDbName)\n");
	printf("Policy options:\n");
	printf("   -y ssl|smime|swuSign|codeSign|pkgSign|resourceSign|iChat|pkinitServer|\n"
		   "      pkinitClient|IPSec\n");
	printf("   -h sslHostName (implies SSL policy; default is basic)\n");
	printf("   -t SSL client side (implies SSL policy, default is server side)\n");
	printf("   -E senderEmail (implies SMIME policy unless iChat is specified)\n");
	printf("Revocation options:\n");
	printf("   -R revocationPolicy (crl|ocsp|both|none); default = none\n");
	printf("   -a (allow certs unverified by CRL or OCSP)\n");
	printf("   -A (require CRL verification if present in cert\n");
	printf("   -4 (require CRL verification for all certs)\n");
	printf("   -Q (require OCSP if present in cert)\n");
	printf("   -5 (require OCSP verification for all certs)\n");
	printf("   -u responderURI\n");
	printf("   -U responderCert\n");
	printf("   -H (OCSP cache disable)\n");
	printf("   -W (network OCSP disable)\n");
	printf("   -o generate OCSP nonce\n");
	printf("   -O require nonce in OCSP response\n");
	printf("Misc. options:\n");
	printf("   -n (no network fetch of CRLs)\n");
	printf("   -N (no network fetch of certs)\n");
	printf("   -k keyUsage (In HEX starting with 0x)\n");
	printf("   -T verifyTime (in CSSM_TIMESTRING format, like 20041217154316)\n");
	printf("   -e=expectedError (default is CSSM_OK)\n");
	printf("   -S scriptFile\n");
	printf("   -p (print script variable names)\n");
	printf("   -P (pause after each script test)\n");
	printf("   -v (verbose)\n");
	printf("   -q (quiet)\n");
	printf("   -L (silent)\n");
	exit(1);
}



/* add files named by successive items in argv to blobList, up until the
 * next '-' arg */
static void gatherFiles(
	BlobList &blobList,
	char **argv,
	int argc,
	int &currArg)
{
	if((currArg == argc) || (argv[currArg][0] == '-')) {
		/* need at least one file name */
		usage(argv);
	}
	while(currArg<argc) {
		char *argp = argv[currArg];
		if(argp[0] == '-') {
			/* done with this file list */
			currArg--;
			return;
		}
		int rtn = blobList.addFile(argv[currArg]);
		if(rtn) {
			exit(1);
		}
		currArg++;
	}
	/* out of args */
	return;
}

int main(int argc, char **argv)
{
	BlobList				certs;
	BlobList				roots;
	BlobList				crls;
	int 					rtn;
	CSSM_DL_HANDLE 			dlHand;
	int						loop;
	int 					arg;
	char 					*argp;
	CSSM_DL_DB_HANDLE_PTR	crlDbHandPtr = NULL;
	CSSM_DL_DB_LIST			dlDbList;
	CSSM_DL_DB_HANDLE		dlDbHandles[2];
	CSSM_RETURN				crtn;
	CSSM_RETURN				silent = CSSM_FALSE;
	CSSM_BOOL				scriptPause = CSSM_FALSE;
	
	CertVerifyArgs			vfyArgs;
	memset(&vfyArgs, 0, sizeof(vfyArgs));
	
	vfyArgs.version = CERT_VFY_ARGS_VERS;
	vfyArgs.certs = &certs;
	vfyArgs.roots = &roots;
	vfyArgs.crls = &crls;

	/* for historical reasons the defaults for these are true */
	vfyArgs.crlNetFetchEnable = CSSM_TRUE;
	vfyArgs.certNetFetchEnable = CSSM_TRUE;
	
	/* user-specd variables */
	int 					loops = 1;
	const char				*crlDbName = NULL;
	const char				*certDbName = NULL;
	char					*scriptFile = NULL;
	
	if(argc < 2) {
		usage(argv);
	}
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		if(argp[0] != '-') {
			usage(argv);
		}
		switch(argp[1]) {
			case 'l':
				loops = atoi(&argp[3]);
				break;
			case 'r':
				arg++;
				gatherFiles(crls, argv, argc, arg);
				break;
			case 'c':
				arg++;
				gatherFiles(certs, argv, argc, arg);
				break;
			case 'C':
				arg++;
				gatherFiles(roots, argv, argc, arg);
				break;
			case 'v':
				vfyArgs.verbose = CSSM_TRUE;
				break;
			case 'q':
				vfyArgs.quiet = CSSM_TRUE;
				break;
			case 's':
				vfyArgs.useSystemAnchors = CSSM_TRUE;
				break;
			case 'g':
				vfyArgs.useTrustSettings = CSSM_TRUE;
				break;
			case 'i':
				vfyArgs.implicitAnchors = CSSM_TRUE;
				break;
			case 'a':
				vfyArgs.allowUnverified = CSSM_TRUE;
				break;
			case 'e':
				vfyArgs.expectedErrStr = &argp[3];
				break;
			case 'n':
				vfyArgs.crlNetFetchEnable = CSSM_FALSE;
				break;
			case 'N':
				vfyArgs.certNetFetchEnable = CSSM_FALSE;
				break;
			case 'f':
				vfyArgs.leafCertIsCA = CSSM_TRUE;
				break;
			case 'd':
				arg++;
				if(arg == argc) {
					usage(argv);
				}
				certDbName = argv[arg];
				break;
			case 'D':
				arg++;
				if(arg == argc) {
					usage(argv);
				}
				crlDbName = argv[arg];
				break;
			case 'S':
				arg++;
				if(arg == argc) {
					usage(argv);
				}
				scriptFile = argv[arg];
				break;
			case 'h':
				arg++;
				if(arg == argc) {
					usage(argv);
				}
				vfyArgs.sslHost= argv[arg];
				vfyArgs.vfyPolicy = CVP_SSL;
				break;
			case 'E':
				arg++;
				if(arg == argc) {
					usage(argv);
				}
				if(vfyArgs.vfyPolicy == CVP_Basic) {
					/* user hasn't specified; now default to SMIME - still 
					 * can override (e.g., for iChat) */
					vfyArgs.vfyPolicy = CVP_SMIME;
				}
				vfyArgs.senderEmail = argv[arg];
				break;
			case 'k':
				arg++;
				if(arg == argc) {
					usage(argv);
				}
				vfyArgs.intendedKeyUse = hexToBin(argv[arg]);
				break;
			case 't':
				vfyArgs.sslClient = CSSM_TRUE;
				vfyArgs.vfyPolicy = CVP_SSL;
				break;
			case 'y':
				arg++;
				if(arg == argc) {
					usage(argv);
				}
				argp = argv[arg];
				if(parsePolicyString(argp, &vfyArgs.vfyPolicy)) {
					printf("Bogus policyValue (%s)\n", argp);
					printPolicyStrings();
					exit(1);
				}
				break;
			case 'R':
				arg++;
				if(arg == argc) {
					usage(argv);
				}
				argp = argv[arg];
				if(!strcmp(argp, "none")) {
					vfyArgs.revokePolicy = CRP_None;
				}
				else if(!strcmp(argp, "crl")) {
					vfyArgs.revokePolicy = CRP_CRL;
				}
				else if(!strcmp(argp, "ocsp")) {
					vfyArgs.revokePolicy = CRP_OCSP;
				}
				else if(!strcmp(argp, "both")) {
					vfyArgs.revokePolicy = CRP_CRL_OCSP;
				}
				else {
					usage(argv);
				}
				break;
			case 'u':
				arg++;
				if(arg == argc) {
					usage(argv);
				}
				vfyArgs.responderURI = argv[arg];
				/* no implied policy yet - could be CRP_OCSP or CRP_CRL_OCSP */
				break;
			case 'U':
				if(readFile(argv[arg], (unsigned char **)vfyArgs.responderCert, 
					&vfyArgs.responderCertLen)) {
					printf("***Error reading responderCert from %s. Aborting.\n", 
						argv[arg]);
					exit(1);
				}
				/* no implied policy yet - could be CRP_OCSP or CRP_CRL_OCSP */
				break;
			case 'H':
				vfyArgs.disableCache = CSSM_TRUE;
				break;
			case 'W':
				vfyArgs.disableOcspNet = CSSM_TRUE;
				break;
			case 'Q':
				vfyArgs.requireOcspIfPresent = CSSM_TRUE;
				break;
			case '5':
				vfyArgs.requireOcspForAll = CSSM_TRUE;
				break;
			case 'o':
				vfyArgs.generateOcspNonce = CSSM_TRUE;
				break;
			case 'O':
				vfyArgs.requireOcspRespNonce = CSSM_TRUE;
				break;
			case 'A':
				vfyArgs.requireCrlIfPresent = CSSM_TRUE;
				break;
			case '4':
				vfyArgs.requireCrlForAll = CSSM_TRUE;
				break;
			case 'T':
				arg++;
				if(arg == argc) {
					usage(argv);
				}
				vfyArgs.vfyTime = argv[arg];
				break;
			case 'p':
				printScriptVars();
				exit(0);
			case 'P':
				scriptPause = CSSM_TRUE;
				break;
			case 'L':
				silent = CSSM_TRUE;				// inhibits start banner
				vfyArgs.quiet = CSSM_TRUE;		// inhibits stdout from certVerify
				break;
			default:
				usage(argv);
		}
	}
	
	if((vfyArgs.responderCert != NULL) || (vfyArgs.responderURI != NULL)) {
		switch(vfyArgs.revokePolicy) {
			case CRP_None:
				vfyArgs.revokePolicy = CRP_OCSP;
				break;
			case CRP_OCSP:
			case CRP_CRL_OCSP:
				break;
			case CRP_CRL:
				printf("*** OCSP options (responderURI, responderCert) only valid "
					"with OCSP policy\n");
				usage(argv);
		}
	}
	
	vfyArgs.clHand = clStartup();
	if(vfyArgs.clHand == CSSM_INVALID_HANDLE) {
		return 1;
	}
	vfyArgs.tpHand = tpStartup();
	if(vfyArgs.tpHand == CSSM_INVALID_HANDLE) {
		return 1;
	}
	vfyArgs.cspHand = cspStartup();
	if(vfyArgs.cspHand == CSSM_INVALID_HANDLE) {
		return 1;
	}
	dlHand = dlStartup();
	if(dlHand == CSSM_INVALID_HANDLE) {
		return 1;
	}
	
	if(!silent) {
		testStartBanner("certcrl", argc, argv);
	}

	if(scriptFile) {
		ScriptVars vars;
		vars.allowUnverified		= vfyArgs.allowUnverified;
		vars.requireCrlIfPresent	= vfyArgs.requireCrlIfPresent;
		vars.requireOcspIfPresent	= vfyArgs.requireOcspIfPresent;
		vars.crlNetFetchEnable		= vfyArgs.crlNetFetchEnable;
		vars.certNetFetchEnable		= vfyArgs.certNetFetchEnable;
		vars.useSystemAnchors		= vfyArgs.useSystemAnchors;
		vars.useTrustSettings		= vfyArgs.useTrustSettings;
		vars.leafCertIsCA			= vfyArgs.leafCertIsCA;
		vars.cacheDisable			= vfyArgs.disableCache;
		vars.ocspNetFetchDisable	= vfyArgs.disableOcspNet;
		vars.requireCrlForAll		= vfyArgs.requireCrlForAll;
		vars.requireOcspForAll		= vfyArgs.requireOcspForAll;
		return runScript(scriptFile, vfyArgs.tpHand, vfyArgs.clHand, 
			vfyArgs.cspHand, dlHand,
			&vars, vfyArgs.quiet, vfyArgs.verbose, scriptPause);
	}
	
	/* open DlDbs if enabled */
	dlDbList.NumHandles = 0;
	dlDbList.DLDBHandle = &dlDbHandles[0];
	dlDbList.DLDBHandle[0].DLHandle = dlHand;
	dlDbList.DLDBHandle[1].DLHandle = dlHand;
	if(certDbName != NULL) {
		crtn = CSSM_DL_DbOpen(dlHand,
			certDbName, 
			NULL,			// DbLocation
			CSSM_DB_ACCESS_READ,
			NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
			NULL,			// void *OpenParameters
			&dlDbList.DLDBHandle[0].DBHandle);
		if(crtn) {
			printError("CSSM_DL_DbOpen", crtn);
			printf("***Error opening DB %s. Aborting.\n", certDbName);
			return 1;
		}
		dlDbList.NumHandles++;
		vfyArgs.dlDbList = &dlDbList;
	}
	if(crlDbName != NULL) {
		vfyArgs.crlDlDb = &dlDbList.DLDBHandle[dlDbList.NumHandles];
		crtn = CSSM_DL_DbOpen(dlHand,
			crlDbName, 
			NULL,			// DbLocation
			CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
			NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
			NULL,			// void *OpenParameters
			&crlDbHandPtr->DBHandle);
		if(crtn) {
			printError("CSSM_DL_DbOpen", crtn);
			printf("***Error opening DB %s. Aborting.\n", crlDbName);
			return 1;
		}
		dlDbList.NumHandles++;
		vfyArgs.dlDbList = &dlDbList;
	}
	for(loop=0; loop<loops; loop++) {
		rtn = certVerify(&vfyArgs);
		if(rtn) {
			break;
		}

		if(loops != 1) {
			fpurge(stdin);
			printf("CR to continue, q to quit: ");
			char c = getchar();
			if(c == 'q') {
				break;
			}
		}
	}
	return rtn;
}

