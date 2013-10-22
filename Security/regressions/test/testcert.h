/*
 * Copyright (c) 2009 Apple Inc. All Rights Reserved.
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
 * testcert.h
 */

#include <Security/SecIdentity.h>
#include <Security/SecCertificate.h>
#include <Security/SecKey.h>

OSStatus
test_cert_generate_key(uint32_t key_size_in_bits, CFTypeRef sec_attr_key_type,
                       SecKeyRef *private_key, SecKeyRef *public_key);

SecIdentityRef
test_cert_create_root_certificate(CFStringRef subject,
	SecKeyRef public_key, SecKeyRef private_key);

SecCertificateRef
test_cert_issue_certificate(SecIdentityRef ca_identity,
	SecKeyRef public_key, CFStringRef subject,
	unsigned int serial_no, unsigned int key_usage);

CFArrayRef test_cert_string_to_subject(CFStringRef subject);
