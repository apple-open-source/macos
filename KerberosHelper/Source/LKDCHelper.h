/* 
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef H_LKDCHELPER_H
#define H_LKDCHELPER_H

#ifdef __cplusplus
extern "C" {
#endif
	
#include <asl.h>

#define kLKDCHelperName "com.apple.KerberosHelper.LKDCHelper"

/* Possible error return codes from the LKDCHelper */
#define ERROR(x, y) x,
enum kLKDCHelperErrors {
	kLKDCHelperSuccess = 0,
	kLKDCHelperErrorBase = 38400,
#include "LKDCHelper-error.h"
	kLKDCHelperErrorEnd
};
#undef ERROR

typedef enum kLKDCHelperErrors LKDCHelperErrorType;

extern volatile int	LKDCLogLevel;

extern	void LKDCLogFunc (const char *func, const char *fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

#define LKDCLog(...)		LKDCLogFunc (__func__, __VA_ARGS__)
#define LKDCLogEnter()		asl_log(NULL, NULL, LKDCLogLevel, "[[[ %s", __func__)
#define LKDCLogExit(error)	asl_log(NULL, NULL, LKDCLogLevel, "]]] %s = %d (%s)", __func__, error, LKDCHelperError (error))

extern	const char				*LKDCHelperError (LKDCHelperErrorType err);
extern	void					LKDCHelperExit ();
extern	void					LKDCDumpStatus (int logLevel);
extern	void					LKDCSetLogLevel (int logLevel);
extern	LKDCHelperErrorType		LKDCGetLocalRealm (char **name);
extern	LKDCHelperErrorType		LKDCDiscoverRealm (const char *hostname, char **realm);
extern	LKDCHelperErrorType		LKDCFindKDCForRealm (const char *realm, char **hostname, uint16_t *port);

#ifdef  __cplusplus
}
#endif

#endif /* H_LKDCHELPER_H */
