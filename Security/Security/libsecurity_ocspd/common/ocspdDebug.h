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

#ifndef	_OCSPD_DEBUGGING_H_
#define _OCSPD_DEBUGGING_H_

#include <security_utilities/debugging.h>

/* If OCSP_USE_SYSLOG is defined and not 0, use syslog() for debug
 * logging in addition to invoking the secdebug macro (which, as of
 * Snow Leopard, emits a static dtrace probe instead of an actual
 * log message.)
 */
#ifndef OCSP_USE_SYSLOG
#define OCSP_USE_SYSLOG	0
#endif

#if OCSP_USE_SYSLOG
#include <syslog.h>
#define ocsp_secdebug(scope, format...) \
{ \
	syslog(LOG_NOTICE, format); \
	secdebug(scope, format); \
}
#else
#define ocsp_secdebug(scope, format...) \
	secdebug(scope, format)
#endif

#ifdef	NDEBUG
/* this actually compiles to nothing */
#define ocspdErrorLog(args...)		ocsp_secdebug("ocspdError", ## args)
#else
/*#define ocspdErrorLog(args...)		printf(args)*/
#define ocspdErrorLog(args...)		ocsp_secdebug("ocspdError", ## args)
#endif

#define ocspdDebug(args...)			ocsp_secdebug("ocspd", ## args)
#define ocspdDbDebug(args...)		ocsp_secdebug("ocspdDb", ## args)
#define ocspdCrlDebug(args...)		ocsp_secdebug("ocspdCrlDebug", ## args)
#define ocspdTrustDebug(args...)	ocsp_secdebug("ocspdTrustDebug", ## args)
#define ocspdHttpDebug(args...)	ocsp_secdebug("ocspdHttp", ## args)
#define ocspdLdapDebug(args...)	ocsp_secdebug("ocspdLdap", ## args)


#endif	/* _OCSPD_DEBUGGING_H_ */
