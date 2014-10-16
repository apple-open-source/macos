#include <CoreFoundation/CoreFoundation.h>
#include <Security/cssmtype.h>
#include <Security/SecIdentity.h>

#if 0
/*
 * Convert a generalized time string, assumed to be in UTC with a 4-digit year,
 * to a CFAbsoluteTime. Returns NULL_TIME on error. 
 */
#define NULL_TIME	0.0

extern CFAbsoluteTime genTimeToCFAbsTime(
	const char *str,
	unsigned len);
	
CFAbsoluteTime parseGenTime(
	const char *timeStr,
	unsigned timeStrLen);
#endif

/*
 * Sign some data with an identity. 
 */
OSStatus ocspSign(
	SecIdentityRef idRef,
	CSSM_DATA &plainText,
	CSSM_ALGORITHMS	algId,		// RSA/SHA1, DSA/SHA1
	CSSM_DATA &sig);			// caller must APP_FREE()

