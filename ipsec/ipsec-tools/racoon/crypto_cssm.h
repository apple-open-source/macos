
/*
 * Copyright (c) 2001-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __CRYPTO_CSSM_H__
#define __CRYPTO_CSSM_H__

/*
 * Racoon module for verifying and signing certificates through Security
 * Framework and CSSM
 */

#include "vmbuf.h"
#include <CoreFoundation/CoreFoundation.h>


extern int crypto_cssm_check_x509cert(vchar_t *cert, CFStringRef hostname, cert_status_t certStatus);
extern vchar_t* crypto_cssm_getsign(CFDataRef persistentCertRef, vchar_t* hash);
extern vchar_t* crypto_cssm_get_x509cert(CFDataRef persistentCertRef, cert_status_t *certStatus);


#endif /* __CRYPTO_CSSM_H__ */

