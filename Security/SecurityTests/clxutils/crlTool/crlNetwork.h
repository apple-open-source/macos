/* 
 * crlNetwork.h - Network support for crlTool
 */
 
#ifndef	_CRL_NETWORK_H_
#define _CRL_NETWORK_H_

#include <Security/cssmtype.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Fetch cert or CRL from net, we figure out the schema */

typedef enum {
	LT_Crl = 1,
	LT_Cert
} LF_Type;

CSSM_RETURN crlNetFetch(
	const CSSM_DATA 	*url,
	LF_Type				lfType,
	CSSM_DATA			*fetched);	// mallocd and RETURNED

#ifdef	__cplusplus
}
#endif

#endif	/* _CRL_NETWORK_H_ */
