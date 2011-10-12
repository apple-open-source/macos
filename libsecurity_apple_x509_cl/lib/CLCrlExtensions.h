/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * CLCrlExtensions.h - extern declarations of get/set/free functions 
 *					implemented in CLCrlExtensions.cpp and used 
 *					only in CrlFields.cpp.
 *
 * Created 9/8/2000 by Doug Mitchell. 
 * Copyright (c) 2000 by Apple Computer. 
 */

#ifndef	_CL_CRL_EXTENSIONS_H_
#define _CL_CRL_EXTENSIONS_H_

#include "DecodedCrl.h"
#include "CLCertExtensions.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Functions to map OID --> {get,set,free}field
 */
getItemFieldFcn getFieldCrlNumber, getFieldDeltaCrl;
setItemFieldFcn setFieldCrlNumber;
freeFieldFcn freeFieldIssuingDistPoint, freeFieldOidOrData, freeFieldCrlDistributionPoints;

#ifdef	__cplusplus
}
#endif

#endif	/* _CL_CRL_EXTENSIONS_H_*/
