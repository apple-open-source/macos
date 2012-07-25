/*
 * Copyright (c) 2000-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * ModuleAttacher.h
 *
 * Process-wide class which loads and attaches to {CSP, TP, CL} at most
 * once, and detaches and unloads the modules when this code is unloaded.
 */

#ifndef	_MODULE_ATTACHER_H_
#define _MODULE_ATTACHER_H_

#include <Security/cssmtype.h>
#include "sslBuildFlags.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Load and attach to all three modules.
 * Returns CSSMERR_CSSM_ADDIN_LOAD_FAILED or CSSM_OK.
 */
extern CSSM_RETURN attachToModules(
	CSSM_CSP_HANDLE		*cspHand,
	CSSM_CL_HANDLE		*clHand,
	CSSM_TP_HANDLE		*tpHand);

#ifdef	__cplusplus
}
#endif

#endif	/* _CSP_ATTACHER_H_ */
