/*
 * script.cpp - run certcrl from script file
 */
 
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <Security/cssm.h>
#include <clAppUtils/BlobList.h>
#include <clAppUtils/certVerify.h>
#include "script.h"

/* Line type returned from parseLine */
typedef enum {
	LT_Empty,			// comments, whitespace
	LT_TestName,
	LT_DirName,	
	LT_Cert,
	LT_Root,
	LT_CRL,
	LT_CertDb,
	LT_CrlDb,
	LT_ExpectError,		// expected function return
	LT_CertError,		// per-cert error string
	LT_CertStatus,		// per-cert StatusBits
	LT_SslHost,
	LT_SslClient,
	LT_SenderEmail,
	LT_Policy,
	LT_KeyUsage,
	LT_RevokePolicy,
	LT_RespURI,
	LT_RespCert,
	LT_EndOfSection,
	LT_EndOfFile,
	LT_BadLine,
	LT_Globals,
	LT_Echo,
	LT_GenerateOcspNonce,
	LT_RequireOcspNonce,
	LT_AllowExpiredRoot,
	LT_VerifyTime,
	LT_ImplicitAnchors,
	
	/* variables which can be in globals or per-test */
	LT_AllowUnverified,
	LT_CrlNetFetchEnable,
	LT_CertNetFetchEnable,
	LT_UseSystemAnchors,
	LT_UseTrustSettings,
	LT_LeafCertIsCA,
	LT_CacheDisable,
	LT_OcspNetFetchDisable,
	LT_RequireOcspIfPresent,
	LT_RequireCrlIfPresent,
	LT_RequireCrlForAll,
	LT_RequireOcspForAll
} LineType;

/* table to map key names to LineType */
typedef struct {
	const char 	*keyName;
	LineType	lineType;
} KeyLineType;

KeyLineType keyLineTypes[] = 
{
	{ "test", 				LT_TestName 			},
	{ "dir",				LT_DirName 				},
	{ "cert",				LT_Cert 				},
	{ "root",				LT_Root 				},
	{ "crl",				LT_CRL 					},
	{ "certDb",				LT_CertDb				},
	{ "crlDb",				LT_CrlDb				},	// no longer used
	{ "error",				LT_ExpectError 			},
	{ "certerror",			LT_CertError			},
	{ "certstatus",			LT_CertStatus			},
	{ "sslHost",			LT_SslHost				},
	{ "sslClient",			LT_SslClient			},
	{ "senderEmail",		LT_SenderEmail			},
	{ "policy",				LT_Policy				},
	{ "keyUsage",			LT_KeyUsage				},
	{ "revokePolicy",		LT_RevokePolicy			},
	{ "responderURI",		LT_RespURI				},
	{ "responderCert",		LT_RespCert				},
	{ "cacheDisable",       LT_CacheDisable			},
	{ "echo",				LT_Echo					},
	{ "globals",			LT_Globals				},
	{ "end",				LT_EndOfSection			},
	{ "allowUnverified",	LT_AllowUnverified 		},
	{ "requireCrlIfPresent",LT_RequireCrlIfPresent  },
	{ "crlNetFetchEnable",	LT_CrlNetFetchEnable	},
	{ "certNetFetchEnable",	LT_CertNetFetchEnable 	},
	{ "ocspNetFetchDisable",LT_OcspNetFetchDisable 	},
	{ "requireCrlForAll",	LT_RequireCrlForAll		},
	{ "requireOcspForAll",	LT_RequireOcspForAll	},
	{ "useSystemAnchors",	LT_UseSystemAnchors	 	},
	{ "useTrustSettings",	LT_UseTrustSettings		},
	{ "leafCertIsCA",		LT_LeafCertIsCA			},
	{ "requireOcspIfPresent",LT_RequireOcspIfPresent },
	{ "generateOcspNonce",	LT_GenerateOcspNonce	},
	{ "requireOcspNonce",	LT_RequireOcspNonce		},
	{ "allowExpiredRoot",	LT_AllowExpiredRoot		},
	{ "verifyTime",			LT_VerifyTime			},
	{ "implicitAnchors",	LT_ImplicitAnchors		},
};

#define NUM_KEYS (sizeof(keyLineTypes) / sizeof(KeyLineType))

/* map policy string to CertVerifyPolicy */
typedef struct {
	const char *str;
	CertVerifyPolicy policy;
} PolicyString;

static const PolicyString policyStrings[] = 
{
	{ "basic",			CVP_Basic				},
	{ "ssl",			CVP_SSL					},
	{ "smime",			CVP_SMIME				},
	{ "swuSign",		CVP_SWUpdateSign		},
	{ "codeSign",		CVP_AppleCodeSigning	},
	{ "pkgSign",		CVP_PackageSigning		},
	{ "resourceSign",	CVP_ResourceSigning		},
	{ "iChat",			CVP_iChat				},
	{ "pkinitServer",	CVP_PKINIT_Server		},
	{ "pkinitClient",	CVP_PKINIT_Client		},
	{ "IPSec",			CVP_IPSec				},
	{ NULL,				(CertVerifyPolicy)0		}
};

/* skip whitespace (but not line terminators) */
static void skipWhite(
	const unsigned char *&cp,
	unsigned &bytesLeft)
{
	while(bytesLeft != 0) {
		switch(*cp) {
			case ' ':
			case '\t':
				cp++;
				bytesLeft--;
				break;
			default:
				return;
		}
	}
}

/* skip to next char after EOL */
static void skipLine(
	const unsigned char *&cp,
	unsigned &bytesLeft)
{
	bool foundEol = false;
	while(bytesLeft != 0) {
		switch(*cp) {
			case '\n':
			case '\r':
				foundEol = true;
				cp++;
				bytesLeft--;
				break;
			default:
				if(foundEol) {
					return;
				}
				cp++;
				bytesLeft--;
				break;
		}
	}
}

/* skip to end of current token (i.e., find next whitespace or '=') */
static void skipToken(
	const unsigned char *&cp,
	unsigned &bytesLeft,
	bool isQuoted)
{
	while(bytesLeft != 0) {
		char c = *cp;
		if(isQuoted) {
			if(c == '"') {
				/* end of quoted string, return still pointing to it */
				return;
			}
		}
		else {
			if(isspace(c)) {
				return;
			}
			if(c == '=') {
				/* hopefully, end of key */
				return;
			}
		}
		cp++;
		bytesLeft--;
	}
}

/*
 * Parse one line, return value (following "=" and whitespace) as
 * mallocd C string. On return, scriptData points to next char after line
 * terminator(s).
 *
 * The basic form of a line is
 * [whitespace] key [whitespace] = [whitespace] value [whitespace] \n|\r...
 *
 * ...except for comments and blank lines. Comments contain '#' as the 
 * first non-whitespace char.
 */
#define CHECK_EOF(bytesLeft)	\
	if(bytesLeft == 0) {		\
		return LT_BadLine;	\
	}

#define MAX_KEY_LEN		80

static LineType parseLine(
	const unsigned char	*&cp,			// IN/OUT
	unsigned			&bytesLeft,		// IN/OUT bytes left in script
	char				*&value,		// mallocd and RETURNED
	CSSM_BOOL			verbose)
{
	if(bytesLeft == 0) {
		if(verbose) {
			printf("...EOF reached\n");
		}
		return LT_EndOfFile;
	}
	skipWhite(cp, bytesLeft);
	if(bytesLeft == 0) {
		return LT_Empty;
	}
	switch(*cp) {
		case '#':
		case '\n':
		case '\r':
		skipLine(cp, bytesLeft);
		return LT_Empty;
	}
	
	/* 
	 * cp points to start of key
	 * get key value as NULL terminated C string
	 */
	const unsigned char *tokenStart = cp;
	skipToken(cp, bytesLeft, false);
	CHECK_EOF(bytesLeft);
	unsigned tokenLen = cp - tokenStart;
	char key[MAX_KEY_LEN];
	memmove(key, tokenStart, tokenLen);
	key[tokenLen] = '\0';
	
	/* parse key */
	LineType rtnType = LT_BadLine;
	for(unsigned i=0; i<NUM_KEYS; i++) {
		KeyLineType *klt = &keyLineTypes[i];
		if(!strcmp(klt->keyName, key)) {
			rtnType = klt->lineType;
			break;
		}	
	}

	/* these keys have no value */
	bool noValue = false;
	switch(rtnType) {
		case LT_EndOfSection:
			if(verbose) {
				printf("...end of section\n");
			}
			noValue = true;
			break;
		case LT_Globals:
			noValue = true;
			break;
		case LT_BadLine:
			printf("***unknown key '%s'\n", key);
			noValue = true;
			break;
		default:
			break;
	}
	if(noValue) {
		/* done with line */
		skipLine(cp, bytesLeft);
		return rtnType;
	}
	
	/* get to start of value */
	skipWhite(cp, bytesLeft);
	CHECK_EOF(bytesLeft);
	if(rtnType == LT_Echo) {
		/* echo: value is everything from this char to end of line */
		tokenStart = cp;
		for( ; bytesLeft != 0; cp++, bytesLeft--) {
			if((*cp == '\n') || (*cp == '\r')) {
				break;
			}
		}
		if(cp != tokenStart) {
			tokenLen = cp - tokenStart;
			value = (char *)malloc(tokenLen + 1);
			memmove(value, tokenStart, tokenLen);
			value[tokenLen] = '\0';
		}
		else {
			value = NULL;
		}
		skipLine(cp, bytesLeft);
		return LT_Echo;
	}
	
	/* all other line types: value is first token after '=' */
	if(*cp != '=') {
		printf("===missing = after key\n");
		return LT_BadLine;
	}
	cp++;
	bytesLeft--;
	skipWhite(cp, bytesLeft);
	CHECK_EOF(bytesLeft);

	/* cp points to start of value */
	bool isQuoted = false;
	if(*cp == '"') {
		cp++;
		bytesLeft--;
		CHECK_EOF(bytesLeft)
		isQuoted = true;
	}
	tokenStart = cp;
	skipToken(cp, bytesLeft, isQuoted);
	/* cp points to next char after end of value */
	/* get value as mallocd C string */
	tokenLen = cp - tokenStart;
	if(tokenLen == 0) {
		value = NULL;
	}
	else {
		value = (char *)malloc(tokenLen + 1);
		memmove(value, tokenStart, tokenLen);
		value[tokenLen] = '\0';
	}
	skipLine(cp, bytesLeft);
	if(verbose) {
		printf("'%s' = '%s'\n", key, value);
	}
	return rtnType;
}

/* describe fate of one run of runOneTest() */
typedef enum {
	OTR_Success,
	OTR_Fail,
	OTR_EndOfScript
} OneTestResult;

/* parse boolean variable, in globals or per-test */
OneTestResult parseVar(
	LineType lineType, 
	const char *value, 
	ScriptVars &scriptVars)
{
	/* parse value  */
	CSSM_BOOL cval;
	if(!strcmp(value, "true")) {
		cval = CSSM_TRUE;
	}
	else if(!strcmp(value, "false")) {
		cval = CSSM_FALSE;
	}
	else {
		printf("***boolean variables must be true or false, not '%s'\n", value);
		return OTR_Fail;
	}

	switch(lineType) {
		case LT_AllowUnverified:
			scriptVars.allowUnverified = cval; 
			break;
		case LT_CrlNetFetchEnable:
			scriptVars.crlNetFetchEnable = cval; 
			break;
		case LT_CertNetFetchEnable:
			scriptVars.certNetFetchEnable = cval; 
			break;
		case LT_UseSystemAnchors:
			scriptVars.useSystemAnchors = cval; 
			break;
		case LT_UseTrustSettings:
			scriptVars.useTrustSettings = cval; 
			break;
		case LT_LeafCertIsCA:
			scriptVars.leafCertIsCA = cval; 
			break;
		case LT_CacheDisable:
			scriptVars.cacheDisable = cval;
			break;
		case LT_OcspNetFetchDisable:
			scriptVars.ocspNetFetchDisable = cval;
			break;
		case LT_RequireOcspIfPresent:
			scriptVars.requireOcspIfPresent = cval;
			break;
		case LT_RequireCrlIfPresent:
			scriptVars.requireCrlIfPresent = cval;
			break;
		case LT_RequireCrlForAll:
			scriptVars.requireCrlForAll = cval;
			break;
		case LT_RequireOcspForAll:
			scriptVars.requireOcspForAll = cval;
			break;
		default:
			return OTR_Fail;
	}
	return OTR_Success;
}

#if 0
/* sure wish X had strnstr */
static char *strnstr(
	const char *big, 
	const char *little, 
	size_t len)
{
	const char *cp;
	unsigned littleLen = strlen(little);
	const char *end = big + len - littleLen;
	char first = little[0];
	
	for(cp=big; cp<end; cp++) {
		/* find first char of little in what's left of big */
		if(*cp != first) {
			continue;
		}
		if(memcmp(cp, little, littleLen) == 0) {
			return (char *)cp;
		}
	} while(cp < end);
	return NULL;
}
#endif

OneTestResult fetchGlobals(
	const unsigned char *&scriptData,	// IN/OUT
	unsigned 			&bytesLeft,		// IN/OUT
	ScriptVars			&scriptVars,	// may be modified
	CSSM_BOOL			verbose)
{
	char *value;		// mallocd by parseLine
	LineType lineType;
	OneTestResult result;
	
	if(verbose) {
		printf("...processing global section\n");
	}
	/* parse globals section until end encountered */
	do {
		value = NULL;
		lineType = parseLine(scriptData, bytesLeft, value, verbose);
		switch(lineType) {
			case LT_Empty:
			case LT_Globals:
				break;					// nop
			case LT_EndOfSection:
				return OTR_Success;
			case LT_EndOfFile:
				printf("***Premature end of file in globals section.\n");
				return OTR_EndOfScript;
			case LT_BadLine:
				return OTR_Fail;
			default:
				/* hopefully a variable */
				result = parseVar(lineType, value, scriptVars);
				if(result != OTR_Success) {
					return OTR_Fail;
				}
				break;
		}
		if(value != NULL) {
			free(value);
		}
	} while(1);
	/* NOT REACHED */
	return OTR_Success;
}

/* parse script fragment for one test, run it */
OneTestResult runOneTest(
	const unsigned char *&scriptData,			// IN/OUT
	unsigned 			&bytesLeft,				// IN/OUT bytes left in script
	CSSM_TP_HANDLE		tpHand, 
	CSSM_CL_HANDLE 		clHand,
	CSSM_CSP_HANDLE 	cspHand,
	CSSM_DL_HANDLE 		dlHand,
	ScriptVars 			&scriptVars,
	CSSM_BOOL			quiet,
	CSSM_BOOL			verbose)
{
	CertVerifyArgs vfyArgs;
	memset(&vfyArgs, 0, sizeof(vfyArgs));

	/* to be gathered from script */
	char 			*testName = NULL;
	char 			*dirName = NULL;
	BlobList 		certs;
	BlobList 		roots;
	BlobList 		crls;
	
	LineType 		lineType;
	char 			*value;			// mallocd by parseLine 
	int 			blobErr;
	ScriptVars 		localVars = scriptVars;
	OneTestResult 	result;
	char			pathName[300];
	CSSM_RETURN		crtn;
	CSSM_DL_DB_HANDLE_PTR currDlDb = NULL;
	CSSM_DL_DB_LIST	dlDbList = {0, NULL};
	
	vfyArgs.version = CERT_VFY_ARGS_VERS;
	vfyArgs.certs = &certs;
	vfyArgs.roots = &roots;
	vfyArgs.crls = &crls;
	vfyArgs.quiet = quiet;
	vfyArgs.tpHand = tpHand;
	vfyArgs.clHand = clHand;
	vfyArgs.cspHand = cspHand;
	vfyArgs.quiet = quiet;
	vfyArgs.revokePolicy = CRP_None;
	vfyArgs.vfyPolicy = CVP_Basic;
	vfyArgs.dlDbList = &dlDbList;
	
	/* parse script up to end of test */
	do {
		value = NULL;
		blobErr = 0;
		lineType = parseLine(scriptData, bytesLeft, value, verbose);
		switch(lineType) {
			case LT_Empty:
				break;					// nop
			case LT_TestName:
				if(testName != NULL) {
					printf("***Duplicate test name ignored\n");
					free(value);
				}
				else {
					testName = value;	// free after test
				}
				value = NULL;
				break;
			case LT_DirName:
				if(dirName != NULL) {
					printf("***Duplicate directory name ignored\n");
					free(value);
				}
				else {
					dirName = value;	// free after test
				}
				value = NULL;
				break;
			case LT_Cert:
				blobErr = certs.addFile(value, dirName);
				break;
			case LT_Root:
				blobErr = roots.addFile(value, dirName);
				break;
			case LT_CRL:
				blobErr = crls.addFile(value, dirName);
				break;
			case LT_CertDb:
			case LT_CrlDb:
				/* these can be called multiple times */
				if(dirName) {
					sprintf(pathName, "%s/%s", dirName, value);
				}
				else {
					strcpy(pathName, value);
				}
				dlDbList.NumHandles++;
				dlDbList.DLDBHandle = (CSSM_DL_DB_HANDLE_PTR)realloc(
					dlDbList.DLDBHandle, 
					dlDbList.NumHandles * sizeof(CSSM_DL_DB_HANDLE));
				currDlDb = &dlDbList.DLDBHandle[dlDbList.NumHandles-1];
				currDlDb->DLHandle = dlHand;
				crtn = CSSM_DL_DbOpen(dlHand,
					pathName, 
					NULL,			// DbLocation
					CSSM_DB_ACCESS_READ | CSSM_DB_ACCESS_WRITE,
					NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
					NULL,			// void *OpenParameters
					&currDlDb->DBHandle);
				if(crtn) {
					printError("CSSM_DL_DbOpen", crtn);
					printf("***Error opening DB %s. Aborting.\n", value);
					return OTR_Fail;
				}
				break;
			case LT_ExpectError:
				if(vfyArgs.expectedErrStr != NULL) {
					printf("***Duplicate expected error ignored\n");
					free(value);
				}
				else {
					vfyArgs.expectedErrStr = value;	// free after test
				}
				value = NULL;
				break;
			case LT_CertError:
				vfyArgs.numCertErrors++;
				vfyArgs.certErrors = (const char **)realloc(vfyArgs.certErrors,
					vfyArgs.numCertErrors * sizeof(char *));
				vfyArgs.certErrors[vfyArgs.numCertErrors - 1] = value;
				value = NULL;						// free after test 
				break;
			case LT_CertStatus:
				vfyArgs.numCertStatus++;
				vfyArgs.certStatus = (const char **)realloc(vfyArgs.certStatus,
					vfyArgs.numCertStatus * sizeof(char *));
				vfyArgs.certStatus[vfyArgs.numCertStatus - 1] = value;
				value = NULL;						// // free after test 
				break;
			case LT_SslHost:
				vfyArgs.sslHost = value;
				value = NULL;			// free after test
				vfyArgs.vfyPolicy = CVP_SSL;
				break;
			case LT_SenderEmail:
				vfyArgs.senderEmail = value;
				value = NULL;			// free after test
				if(vfyArgs.vfyPolicy == CVP_Basic) {
					/* don't overwrite if it's already been set to e.g. iChat */
					vfyArgs.vfyPolicy = CVP_SMIME;
				}
				break;
			case LT_Policy:
				if(parsePolicyString(value, &vfyArgs.vfyPolicy)) {
					printf("Bogus policyValue (%s)\n", value);
					printPolicyStrings();
					return OTR_Fail;
				}
				break;
			case LT_KeyUsage:
				vfyArgs.intendedKeyUse = hexToBin(value);
				break;
			case LT_RevokePolicy:
				if(!strcmp(value, "none")) {
					vfyArgs.revokePolicy = CRP_None;
				}
				else if(!strcmp(value, "crl")) {
					vfyArgs.revokePolicy = CRP_CRL;
				}
				else if(!strcmp(value, "ocsp")) {
					vfyArgs.revokePolicy = CRP_OCSP;
				}
				else if(!strcmp(value, "both")) {
					vfyArgs.revokePolicy = CRP_CRL_OCSP;
				}
				else {
					printf("***Illegal revokePolicy (%s)\n.", value);
					return OTR_Fail;
				}
				break;
			case LT_RespURI:
				vfyArgs.responderURI = value;
				value = NULL;			// free after test
				break;
			case LT_VerifyTime:
				vfyArgs.vfyTime = value;
				value = NULL;			// free after test
				break;
			case LT_RespCert:
				if(readFile(value, (unsigned char **)&vfyArgs.responderCert, 
						&vfyArgs.responderCertLen)) {
					printf("***Error reading responderCert from %s\n", value);
					return OTR_Fail;
				}
				break;
			case LT_EndOfSection:
				break;
			case LT_EndOfFile:
				/* only legal if we haven't gotten a test name */
				if(testName == NULL) {
					return OTR_EndOfScript;
				}
				printf("***Premature end of file.\n");
				return OTR_Fail;
			case LT_BadLine:
				return OTR_Fail;
			case LT_Globals:
				result = fetchGlobals(scriptData, bytesLeft, scriptVars, verbose);
				if(result != OTR_Success) {
					printf("***Bad globals section\n");
					return OTR_Fail;
				}
				/* and start over with these variables */
				localVars = scriptVars;
				break;
			case LT_SslClient:
				if(!strcmp(value, "true")) {
					vfyArgs.sslClient = CSSM_TRUE;
				}
				else {
					vfyArgs.sslClient = CSSM_FALSE;
				}
				vfyArgs.vfyPolicy = CVP_SSL;
				break;
			case LT_Echo:
				if(!quiet) {
					printf("%s\n", value);
				}
				break;
			case LT_GenerateOcspNonce:
				vfyArgs.generateOcspNonce = CSSM_TRUE;
				break;
			case LT_RequireOcspNonce:
				vfyArgs.requireOcspRespNonce = CSSM_TRUE;
				break;
			case LT_AllowExpiredRoot:
				if(!strcmp(value, "true")) {
					vfyArgs.allowExpiredRoot = CSSM_TRUE;
				}
				else {
					vfyArgs.allowExpiredRoot = CSSM_FALSE;
				}
				break;
			case LT_ImplicitAnchors:
				if(!strcmp(value, "true")) {
					vfyArgs.implicitAnchors = CSSM_TRUE;
				}
				else {
					vfyArgs.implicitAnchors = CSSM_FALSE;
				}
				break;
			default:
				/* hopefully a variable */
				result = parseVar(lineType, value, localVars);
				if(result != OTR_Success) {
					printf("**Bogus line in script %u bytes from EOF\n",
						bytesLeft);
					return OTR_Fail;
				}
				break;

		}
		if(blobErr) {
			return OTR_Fail;
		}
		if(value != NULL) {
			free(value);
		}
	} while(lineType != LT_EndOfSection);
	
	/* some args: copy from ScriptVars -> CertVerifyArgs */
	vfyArgs.allowUnverified = localVars.allowUnverified;
	vfyArgs.requireOcspIfPresent = localVars.requireOcspIfPresent;
	vfyArgs.requireCrlIfPresent = localVars.requireCrlIfPresent;
	vfyArgs.crlNetFetchEnable = localVars.crlNetFetchEnable;
	vfyArgs.certNetFetchEnable = localVars.certNetFetchEnable;
	vfyArgs.useSystemAnchors = localVars.useSystemAnchors;
	vfyArgs.useTrustSettings = localVars.useTrustSettings;
	vfyArgs.leafCertIsCA = localVars.leafCertIsCA;
	vfyArgs.disableCache = localVars.cacheDisable;
	vfyArgs.disableOcspNet = localVars.ocspNetFetchDisable;
	vfyArgs.requireCrlForAll = localVars.requireCrlForAll;
	vfyArgs.requireOcspForAll = localVars.requireOcspForAll;
	vfyArgs.verbose = verbose;
	
	/* here we go */
	if(!quiet && (testName != NULL)) {
		printf("%s\n", testName);
	}
	int rtn = certVerify(&vfyArgs);

	OneTestResult ourRtn = OTR_Success;
	if(rtn) {
		printf("***Failure on %s\n", testName);
		if(testError(quiet)) {
			ourRtn = OTR_Fail;
		}
	}
	/* free the stuff that didn't get freed and the end of the
	 * main per-line loop */
	if(dirName != NULL) {
		free(dirName);
	}
	if(vfyArgs.expectedErrStr != NULL) {
		free((void *)vfyArgs.expectedErrStr);
	}
	if(vfyArgs.certErrors != NULL) {
		for(unsigned i=0; i<vfyArgs.numCertErrors; i++) {
			free((void *)vfyArgs.certErrors[i]);		// mallocd by parseLine
		}
		free((void *)vfyArgs.certErrors);				// reallocd by us
	}
	if(vfyArgs.certStatus != NULL) {
		for(unsigned i=0; i<vfyArgs.numCertStatus; i++) {
			free((void *)vfyArgs.certStatus[i]);		// mallocd by parseLine
		}
		free((void *)vfyArgs.certStatus);				// reallocd by us
	}
	if(testName != NULL) {
		free(testName);
	}
	if(vfyArgs.sslHost) {
		free((void *)vfyArgs.sslHost);
	}
	if(vfyArgs.senderEmail) {
		free((void *)vfyArgs.senderEmail);
	}
	if(vfyArgs.responderURI) {
		free((void *)vfyArgs.responderURI);
	}
	if(vfyArgs.responderCert) {
		free((void *)vfyArgs.responderCert);
	}
	if(vfyArgs.vfyTime) {
		free((void *)vfyArgs.vfyTime);
	}
	if(dlDbList.DLDBHandle) {
		for(unsigned dex=0; dex<dlDbList.NumHandles; dex++) {
			CSSM_DL_DbClose(dlDbList.DLDBHandle[dex]);
		}
		free(dlDbList.DLDBHandle);
	}
	return ourRtn;
}

int runScript(
	const char 		*fileName,
	CSSM_TP_HANDLE	tpHand, 
	CSSM_CL_HANDLE 	clHand,
	CSSM_CSP_HANDLE cspHand,
	CSSM_DL_HANDLE 	dlHand,
	ScriptVars		*scriptVars,
	CSSM_BOOL		quiet,
	CSSM_BOOL		verbose,
	CSSM_BOOL		doPause)
{
	const unsigned char *scriptData;
	unsigned char *cp;
	unsigned scriptDataLen;
	int rtn;
	ScriptVars localVars = *scriptVars;
	
	rtn = readFile(fileName, &cp, &scriptDataLen);
	if(rtn) {
		printf("***Error reading script file; aborting.\n");
		printf("***Are you sure you're running this from the proper directory?\n");
		return rtn;
	}
	scriptData = (const unsigned char *)cp;
	OneTestResult result;
	
	do {
		result = runOneTest(scriptData, scriptDataLen,
			tpHand, clHand, cspHand, dlHand,
			localVars, quiet, verbose);
		if(result == OTR_Fail) {
			rtn = 1;
			break;
		}
		if(doPause) {
			fpurge(stdin);
			printf("CR to continue: ");
			getchar();
		}
	} while(result == OTR_Success);
	free(cp);
	return rtn;
}

/* parse policy string; returns nonzero if not found */
int parsePolicyString(
	const char *str,
	CertVerifyPolicy *policy)
{
	const PolicyString *ps;
	for(ps=policyStrings; ps->str; ps++) {
		if(!strcmp(ps->str, str)) {
			*policy = ps->policy;
			return 0;
		}
	}
	return 1;
}

void printPolicyStrings()
{
	printf("Valid policy strings are:\n   ");
	const PolicyString *ps;
	unsigned i=0;
	for(ps=policyStrings; ps->str; ps++, i++) {
		printf("%s", ps->str);
		if(ps[1].str == NULL) {
			break;
		}
		if((i % 6)  == 5) {
			printf(",\n   ");
		}
		else {
			printf(", ");
		}
	}
	printf("\n");
}

void printScriptVars()
{
	printf("The list of script variables is as follows:\n");
	for(unsigned dex=0; dex<NUM_KEYS; dex++) {
		printf("   %s\n", keyLineTypes[dex].keyName);
	}
	printPolicyStrings();
	printf("Valid revokePolicy strings are:\n   none, crl, ocsp, both\n");
}

