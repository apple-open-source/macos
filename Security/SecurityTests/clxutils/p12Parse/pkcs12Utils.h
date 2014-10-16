#ifndef	_PKCS12_UTILS_H_
#define _PKCS12_UTILS_H_

#include <Security/cssmtype.h>
#include <CoreFoundation/CFString.h>
#include <security_pkcs12/pkcs12Templates.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* CSSM_DATA --> uint32. Returns true if OK. */
bool p12DataToInt(
	const CSSM_DATA &cdata,
	uint32 &u);

typedef enum {
	PW_None,			/* not comprehended */
	PW_PKCS5_v1_5,		/* PKCS5 v1.5 */
	PW_PKCS5_v2,		/* PKCS5 v2.0, not used by this module but parsed here */
	PW_PKCS12			/* PKCS12 */
} PKCS_Which;

/* returns false if OID not found */
bool pkcsOidToParams(
	const CSSM_OID 		*oid,
	CSSM_ALGORITHMS		&keyAlg,		// e.g., CSSM_ALGID_DES
	CSSM_ALGORITHMS		&encrAlg,		// e.g., CSSM_ALGID_3DES_3KEY_EDE
	CSSM_ALGORITHMS		&pbeHashAlg,	// SHA1 or MD5
	uint32				&keySizeInBits,
	uint32				&blockSizeInBytes,	// for IV, optional
	CSSM_PADDING		&padding,		// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	&mode,			// CSSM_ALGMODE_CBCPadIV8, etc.
	PKCS_Which			&pkcs);			// PW_PKCS5_v1_5 or PW_PKCS12

const char *p12BagTypeStr(
	NSS_P12_SB_Type type);
const char *p7ContentInfoTypeStr(
	NSS_P7_CI_Type type);

#ifdef	__cplusplus
}
#endif

#endif	/* _PKCS12_UTILS_H_ */
