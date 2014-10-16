#include <ctype.h>
#include <strings.h>
#include "ocspUtils.h"
#include <utilLib/cspwrap.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>


/*
 * Sign some data with an identity. 
 */
OSStatus ocspSign(
	SecIdentityRef idRef,
	CSSM_DATA &plainText,
	CSSM_ALGORITHMS	algId,		// RSA/SHA1, DSA/SHA1
	CSSM_DATA &sig)				// caller must APP_FREE()
{
	const CSSM_KEY *privCssmKey;
	OSStatus ortn;
	SecKeyRef privKeyRef;
	CSSM_CSP_HANDLE cspHand;
	CSSM_RETURN crtn;
	
	ortn = SecIdentityCopyPrivateKey(idRef, &privKeyRef);
	if(ortn) {
		cssmPerror("SecIdentityCopyPrivateKey", ortn);
		return ortn;
	}
	ortn = SecKeyGetCSSMKey(privKeyRef, &privCssmKey);
	if(ortn) {
		cssmPerror("SecKeyGetCSSMKey", ortn);
		goto errOut;
	}
	ortn = SecKeyGetCSPHandle(privKeyRef, &cspHand);
	if(ortn) {
		cssmPerror("SecKeyGetCSPHandle", ortn);
		goto errOut;
	}
	sig.Data = NULL;
	sig.Length = 0; 
	crtn = cspSign(cspHand, algId, (CSSM_KEY_PTR)privCssmKey, 
		&plainText, &sig);
	if(crtn) {
		cssmPerror("cspSign", crtn);
		ortn = crtn;
	}
errOut:
	CFRelease(privKeyRef);
	return ortn;
}
