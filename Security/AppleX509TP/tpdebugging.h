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
	File:		tpdebugging.h

	Contains:	Debugging macros.

	Written by:	Doug Mitchell

	Copyright:	(c) 1998 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		06/02/98	dpm		Added DEBUG_THREAD_YIELD.
		03/10/98	dpm		Created.

*/

#ifndef	_TPDEBUGGING_H_
#define _TPDEBUGGING_H_

#include <Security/debugging.h>

#ifdef	NDEBUG
/* this actually compiles to nothing */
#define tpErrorLog(args...)		secdebug("tpError", ## args)
#else
#define tpErrorLog(args...)		printf(args)
#endif

#define tpDebug(args...)		secdebug("tpDebug", ## args)
#define tpCrlDebug(args...)		secdebug("tpCrlDebug", ## args)
#define tpPolicyError(args...)	secdebug("tpPolicy", ## args)
#define tpVfyDebug(args...)		secdebug("tpVfyDebug", ## args)
#define tpAnchorDebug(args...)	secdebug("tpAnchorDebug", ## args)

#endif	/* _TPDEBUGGING_H_ */
