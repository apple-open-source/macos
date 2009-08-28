/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
 * CSPAttacher.h - process-wide class which loads and attaches to CSP at most
 *				   once, and detaches and unloads the CSP when this code is
 *	 			   unloaded.
 */
 
#ifndef	_CSP_ATTACHER_H_
#define _CSP_ATTACHER_H_

#include <Security/cssmtype.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* 
 * Just one public function - "give me a CSP handle".
 *   bareCsp true : AppleCSP
 *   bareCsp false: AppleCSPDL
 *
 * Throws a CssmError on failure. 
 */
extern CSSM_CSP_HANDLE	getGlobalCspHand(bool bareCsp);

#ifdef	__cplusplus
}
#endif

#endif	/* _CSP_ATTACHER_H_ */
