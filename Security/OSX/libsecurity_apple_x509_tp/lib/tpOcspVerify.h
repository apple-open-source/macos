/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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

/*
 * tpOcspVerify.h - top-level OCSP verification
 */
 
#ifndef	_TP_OCSP_VERIFY_H_
#define _TP_OCSP_VERIFY_H_

#include "tpCrlVerify.h"

extern "C" {

/* 
 * The sole and deceptively simple looking public interface to this module. 
 * It's pretty heavyweight; expect to spend millions or billions of cycles
 * here before returning. 
 */
CSSM_RETURN tpVerifyCertGroupWithOCSP(
	TPVerifyContext	&tpVerifyContext,
	TPCertGroup 	&certGroup);		// to be verified 
	
}

#endif	/* _TP_OCSP_VERIFY_H_ */

