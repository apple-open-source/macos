/*
 * script.h - run certcrl from script file
 */

#ifndef	_SCRIPT_H_
#define _SCRIPT_H_

#include <Security/cssmtype.h>

/*
 * Test variables which can be specified at the top level (on the 
 * call to runScript), or globally in a script, or within the scope
 * of one test.
 */
typedef struct {
	CSSM_BOOL	allowUnverified;
	CSSM_BOOL	requireCrlIfPresent;
	CSSM_BOOL	requireOcspIfPresent;
	CSSM_BOOL	crlNetFetchEnable;
	CSSM_BOOL	certNetFetchEnable;
	CSSM_BOOL	useSystemAnchors;
	CSSM_BOOL	useTrustSettings;
	CSSM_BOOL	leafCertIsCA;
	CSSM_BOOL	cacheDisable;
	CSSM_BOOL	ocspNetFetchDisable;
	CSSM_BOOL	requireCrlForAll;
	CSSM_BOOL	requireOcspForAll;
} ScriptVars;

extern "C" {
int runScript(
	const char 		*fileName,
	CSSM_TP_HANDLE	tpHand, 
	CSSM_CL_HANDLE 	clHand,
	CSSM_CSP_HANDLE cspHand,
	CSSM_DL_HANDLE 	dlHand,
	ScriptVars		*scriptVars,
	CSSM_BOOL		quiet,
	CSSM_BOOL		verbose,
	CSSM_BOOL		doPause);
	
/* parse policy string; returns nonzero of not found */
int parsePolicyString(
	const char *str,
	CertVerifyPolicy *policy);

void printPolicyStrings();
void printScriptVars();

}
#endif	/* _SCRIPT_H_ */
