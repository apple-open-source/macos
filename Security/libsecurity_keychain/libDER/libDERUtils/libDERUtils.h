/* Copyright (c) 2005-2007 Apple Inc. All Rights Reserved. */

/*
 * libDERUtils.h - support routines for libDER tests & examples 
 *
 * Created Nov. 7 2005 by dmitch
 */

#ifndef	_LIB_DER_UTILS_H_
#define _LIB_DER_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <libDER/libDER.h>

const char *DERReturnString(
	DERReturn		drtn);
	
void DERPerror(
	const char *op,
	DERReturn rtn);
	
#ifdef __cplusplus
}
#endif

#endif	/* _LIB_DER_UTILS_H_ */
