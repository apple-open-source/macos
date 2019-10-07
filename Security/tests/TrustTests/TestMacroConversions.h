/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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
 */

#ifndef _TRUSTTEST_MACRO_CONVERSIONS_H_
#define _TRUSTTEST_MACRO_CONVERSIONS_H_

#import <XCTest/XCTest.h>

#define isnt(THIS, THAT, ...) do { XCTAssertNotEqual(THIS, THAT, __VA_ARGS__); } while(0)

#define is(THIS, THAT, ...) do { XCTAssertEqual(THIS, THAT, __VA_ARGS__); } while(0)

#define is_status(THIS, THAT, ...) is(THIS, THAT)

#define ok(THIS, ...) do { XCTAssert(THIS, __VA_ARGS__); } while(0)

#define ok_status(THIS, ...) do { XCTAssertEqual(THIS, errSecSuccess, __VA_ARGS__); } while(0)

#define fail(...) ok(0, __VA_ARGS__)

#endif /* _TRUSTTEST_MACRO_CONVERSIONS_H_ */
