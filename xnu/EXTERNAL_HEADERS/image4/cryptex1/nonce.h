/*
 * Copyright © 2017-2024 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*!
 * @header
 * Definitions and interfaces for handling Cryptex1 nonces on Darwin platforms.
 */
#ifndef __IMAGE4_DARWIN_CRYPTEX1_NONCE_H
#define __IMAGE4_DARWIN_CRYPTEX1_NONCE_H

#include <os/base.h>
#include <stdint.h>

__BEGIN_DECLS
OS_ASSUME_NONNULL_BEGIN

/*!
 * @typedef darwin_cryptex1_nonce_t
 * A type describing nonce handle values for Cryptex1 nonce domains hosted on
 * Darwin.
 *
 * @const DARWIN_CRYPTEX1_NONCE_BOOT
 * The Cryptex1 boot nonce.
 *
 * @const DARWIN_CRYPTEX1_NONCE_ASSET_BRAIN
 * The Cryptex1 MobileAsset brain nonce.
 *
 * @const DARWIN_CRYPTEX1_NONCE_GENERIC
 * The Cryptex1 generic nonce.
 *
 * @const DARWIN_CRYPTEX1_NONCE_SIMULATOR_RUNTIME
 * The Cryptex1 simulator runtime nonce.
 *
 * @const DARWIN_CRYPTEX1_NONCE_MOBILE_ASSET_DFU
 * The Cryptex1 MobileAsset DFU nonce.
 */
OS_CLOSED_ENUM(darwin_cryptex1_nonce, uint32_t,
	DARWIN_CRYPTEX1_NONCE_BOOT = 1,
	DARWIN_CRYPTEX1_NONCE_ASSET_BRAIN = 2,
	DARWIN_CRYPTEX1_NONCE_GENERIC = 3,
	DARWIN_CRYPTEX1_NONCE_SIMULATOR_RUNTIME = 4,
	DARWIN_CRYPTEX1_NONCE_MOBILE_ASSET_DFU = 5,
	DARWIN_CRYPTEX1_NONCE_RESERVED_0 = 6,
	DARWIN_CRYPTEX1_NONCE_RESERVED_1 = 7,
	_DARWIN_CRYPTEX1_NONCE_CNT,
);

OS_ASSUME_NONNULL_END
__END_DECLS

#endif // __IMAGE4_DARWIN_CRYPTEX1_NONCE_H
