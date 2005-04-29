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
 * YarrowConnection.h - single, process-wide, thread-safe Yarrow client
 */

#ifndef	_YARROW_CONNECTION_H_
#define _YARROW_CONNECTION_H_

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Both functions a CssmError::throwMe(CSSMERR_CSP_FUNCTION_FAILED) on failure. 
 * 
 * "Give me some random data". Caller mallocs the data. 
 */
extern void	cspGetRandomBytes(void *buf, unsigned len);

/*
 * Add some entropy to the pool. 
 */
extern void cspAddEntropy(const void *buf, unsigned len);

#ifdef	__cplusplus
}
#endif

#endif	/* _YARROW_CONNECTION_H_ */
