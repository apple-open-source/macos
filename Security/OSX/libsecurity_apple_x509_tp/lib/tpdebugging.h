/*
 * Copyright (c) 2000-2011 Apple Inc. All Rights Reserved.
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

#ifndef _TPDEBUGGING_H_
#define _TPDEBUGGING_H_

#include <security_utilities/debugging.h>

/* If TP_USE_SYSLOG is defined and not 0, use syslog() for debug
 * logging in addition to invoking the secinfo macro (which, as of
 * 10.11, emits a os_log message of a syslog message.)
 */
#ifndef TP_USE_SYSLOG
#define TP_USE_SYSLOG	0
#endif

#if TP_USE_SYSLOG
#include <syslog.h>
#define tp_secinfo(scope, format...) \
{ \
	syslog(LOG_NOTICE, format); \
	secinfo(scope, format); \
}
#else
#define tp_secinfo(scope, format...) \
	secinfo(scope, format)
#endif

#ifdef	NDEBUG
/* this actually compiles to nothing */
#define tpErrorLog(args...)		tp_secinfo("tpError", ## args)
#else
#define tpErrorLog(args...)		printf(args)
#endif

#define tpDebug(args...)		tp_secinfo("tpDebug", ## args)
#define tpDbDebug(args...)		tp_secinfo("tpDbDebug", ## args)
#define tpCrlDebug(args...)		tp_secinfo("tpCrlDebug", ## args)
#define tpPolicyError(args...)	tp_secinfo("tpPolicy", ## args)
#define tpVfyDebug(args...)		tp_secinfo("tpVfyDebug", ## args)
#define tpAnchorDebug(args...)	tp_secinfo("tpAnchorDebug", ## args)
#define tpOcspDebug(args...)	tp_secinfo("tpOcsp", ## args)
#define tpOcspCacheDebug(args...)	tp_secinfo("tpOcspCache", ## args)
#define tpTrustSettingsDbg(args...)	tp_secinfo("tpTrustSettings", ## args)

#endif	/* _TPDEBUGGING_H_ */
