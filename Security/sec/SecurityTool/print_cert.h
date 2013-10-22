/*
 * Copyright (c) 2003-2007 Apple Inc. All Rights Reserved.
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
 *
 *  print_cert.h
 *
 *  Created by Michael Brouwer on 10/10/06.
 */

#ifndef _PRINT_CERT_H_
#define _PRINT_CERT_H_

#include <sys/cdefs.h>
#include <stdio.h>
#include <Security/SecCertificate.h>
#include <CoreFoundation/CFArray.h>

__BEGIN_DECLS

void print_plist(CFArrayRef plist);
void print_cert(SecCertificateRef cert, bool verbose);

__END_DECLS

#endif /* _PRINT_CERT_H_ */
