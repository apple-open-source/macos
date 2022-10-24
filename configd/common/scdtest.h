/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#ifndef _SCDTEST_H
#define _SCDTEST_H

/*
 * SCDynamicStore Test keys/entitlements
 */
#define SCDTEST_PREFIX			"com.apple.SCDynamicStore.test."
#define SCDTEST_PREFIX_PATTERN_STR	"com\\.apple\\.SCDynamicStore\\.test\\."
#define SCDTEST_ENTITLEMENT(a)		CFSTR(a ".entitlement")
#define SCDTEST_KEY(a)			CFSTR(a ".key")
#define SCDTEST_PREFIX_PATTERN(a)	CFSTR(SCDTEST_PREFIX_PATTERN_STR a)

/*
 * readDeny
 *   SCDTEST_READ_DENY{1,2}_KEY are not readable if process holds
 *   corresponding SCDTEST_READ_DENY{1,2}_ENTITLEMENT
 */
#define SCDTEST_READ_DENY		SCDTEST_PREFIX "read-deny"
#define SCDTEST_READ_DENY1		SCDTEST_READ_DENY "1"
#define SCDTEST_READ_DENY1_ENTITLEMENT 	SCDTEST_ENTITLEMENT(SCDTEST_READ_DENY1)
#define SCDTEST_READ_DENY1_KEY		SCDTEST_KEY(SCDTEST_READ_DENY1)

#define SCDTEST_READ_DENY2		SCDTEST_READ_DENY "2"
#define SCDTEST_READ_DENY2_ENTITLEMENT 	SCDTEST_ENTITLEMENT(SCDTEST_READ_DENY2)
#define SCDTEST_READ_DENY2_KEY		SCDTEST_KEY(SCDTEST_READ_DENY2)

/*
 * readAllow
 *   SCDTEST_READ_ALLOW{1,2}_KEY are only readable if process holds
 *   corresponding SCDTEST_READ_ALLOW{1,2}_ENTITLEMENT
 */
#define SCDTEST_READ_ALLOW		SCDTEST_PREFIX "read-allow"
#define SCDTEST_READ_ALLOW1		SCDTEST_READ_ALLOW "1"
#define SCDTEST_READ_ALLOW1_ENTITLEMENT	SCDTEST_ENTITLEMENT(SCDTEST_READ_ALLOW1)
#define SCDTEST_READ_ALLOW1_KEY		SCDTEST_KEY(SCDTEST_READ_ALLOW1)

#define SCDTEST_READ_ALLOW2		SCDTEST_READ_ALLOW "2"
#define SCDTEST_READ_ALLOW2_ENTITLEMENT	SCDTEST_ENTITLEMENT(SCDTEST_READ_ALLOW2)
#define SCDTEST_READ_ALLOW2_KEY		SCDTEST_KEY(SCDTEST_READ_ALLOW2)

/*
 * Read pattern
 */
#define SCDTEST_READ_PATTERN	SCDTEST_PREFIX_PATTERN("read.*")

/*
 * writeProtect
 *   SCDTEST_WRITE_PROTECT{1,2}_KEY are only writable if process holds the
 *   key-specific entitlement.
 */
#define SCDTEST_WRITE_PROTECT		SCDTEST_PREFIX "write-protect"
#define SCDTEST_WRITE_PROTECT1		SCDTEST_WRITE_PROTECT "1"
#define SCDTEST_WRITE_PROTECT1_KEY	SCDTEST_KEY(SCDTEST_WRITE_PROTECT1)

#define SCDTEST_WRITE_PROTECT2		SCDTEST_WRITE_PROTECT "2"
#define SCDTEST_WRITE_PROTECT2_KEY	SCDTEST_KEY(SCDTEST_WRITE_PROTECT2)
	
#endif /* _SCDTEST_H */
