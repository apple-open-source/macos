/*
 * Copyright (c) 2011-12 Apple Inc. All Rights Reserved.
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

#include "ossl-crypto.h"

/**
 * Add all algorithms to the crypto core.
 *
 * @ingroup hcrypto_core
 */
void
OpenSSL_add_all_algorithms(void)
{
}


/**
 * Add all algorithms to the crypto core using configuration file.
 *
 * @ingroup hcrypto_core
 */
void
OpenSSL_add_all_algorithms_conf(void)
{
}


/**
 * Add all algorithms to the crypto core, but don't use the
 * configuration file.
 *
 * @ingroup hcrypto_core
 */
void
OpenSSL_add_all_algorithms_noconf(void)
{
}


long
SSLeay(void)
{
	return (SSLEAY_VERSION_NUMBER);
}


const char *
SSLeay_version(int t)
{
	if (SSLEAY_VERSION == t) {
		return (OPENSSL_VERSION_TEXT);
	}

	return ("information not available");
}
