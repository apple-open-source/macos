/*
 * p12GetPassKey.h - get a CSSM_ALGID_SECURE_PASSPHRASE key for encode/decode
 */
 
#ifndef	__P12_GET_PASSKEY_H__
#define __P12_GET_PASSKEY_H__

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef enum {
	GPK_Decode = 1,
	GPK_Encode
} GPK_Type;

OSStatus p12GetPassKey(
	CSSM_CSP_HANDLE	cspHand,
	GPK_Type 		gpkType,
	bool			isRawCsp,
	CSSM_KEY		*passKey);		// RETURNED

#ifdef  __cplusplus
}
#endif

#endif  /* __P12_GET_PASSKEY_H__ */
