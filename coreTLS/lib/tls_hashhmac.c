/*
 * Copyright (c) 2002,2005-2008,2010-2011 Apple Inc. All Rights Reserved.
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
 * tls_hashhmac.c - HMAC routines used by TLS
 */

/* THIS FILE CONTAINS KERNEL CODE */

#include "tls_digest.h"
#include "tls_hashhmac.h"

const HashHmacReference HashHmacNull = {
	&SSLHashNull,
	&TlsHmacNull
};

const HashHmacReference HashHmacMD5 = {
	&SSLHashMD5,
	&TlsHmacMD5
};

const HashHmacReference HashHmacSHA1 = {
	&SSLHashSHA1,
	&TlsHmacSHA1
};

const HashHmacReference HashHmacSHA256 = {
	&SSLHashSHA256,
	&TlsHmacSHA256
};

const HashHmacReference HashHmacSHA384 = {
	&SSLHashSHA384,
	&TlsHmacSHA384
};
