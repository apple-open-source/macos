/*
 * crlUtils.cpp - CRL CL/TP/DL utilities.
 */

#ifndef	_CRL_UTILS_H_
#define _CRL_UTILS_H_

#include <Security/cssmtype.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Add a CRL to an existing DL/DB.
 */
#define MAX_CRL_ATTRS			8

CSSM_RETURN crlAddCrlToDb(
	CSSM_DL_DB_HANDLE	dlDbHand,
	CSSM_CL_HANDLE		clHand,
	const CSSM_DATA		*crl);

#ifdef	__cplusplus
}
#endif

#endif	/* _CRL_UTILS_H_ */
