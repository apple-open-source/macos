/*
 * clutils.h - common CL app-level routines, X version
 */

#ifndef	_CL_APP_UTILS_CLUTILS_H_
#define _CL_APP_UTILS_CLUTILS_H_

#include <Security/cssm.h>
#include <utilLib/common.h>

#ifdef	__cplusplus
extern "C" {
#endif

CSSM_CL_HANDLE clStartup();
void clShutdown(
	CSSM_CL_HANDLE clHand);

CSSM_TP_HANDLE tpStartup();
void tpShutdown(
	CSSM_TP_HANDLE tpHand);


CSSM_DATA_PTR intToDER(unsigned theInt);
uint32 DER_ToInt(const CSSM_DATA *DER_Data);

#ifdef	__cplusplus
}
#endif

#endif	/* _CL_APP_UTILS_CLUTILS_H_ */
