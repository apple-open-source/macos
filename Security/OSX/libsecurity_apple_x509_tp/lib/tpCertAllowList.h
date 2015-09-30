/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

/*!
	@header tpCertAllowList.h
*/


#include <Security/cssmtype.h>
#include <security_utilities/alloc.h>
#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include "TPCertInfo.h"
#include "tpCrlVerify.h"

#ifndef _SECURITY_TPCERTALLOWLIST_H_
#define _SECURITY_TPCERTALLOWLIST_H_

__BEGIN_DECLS

CSSM_RETURN tpCheckCertificateAllowList(TPCertGroup &certGroup);

__END_DECLS

#endif /* _SECURITY_TPCERTALLOWLIST_H_ */
