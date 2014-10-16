/*
 * Copyright (c) 2003-2006,2011 Apple Inc. All Rights Reserved.
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
 * testleaks.h
 */

#ifndef _TESTLEAKS_H_
#define _TESTLEAKS_H_ 1

#ifdef __cplusplus
extern "C" {
#endif


#define ok_leaks(TESTNAME) \
({ \
    int _this = test_leaks(); \
    test_ok(!_this, TESTNAME, test_directive, test_reason, \
			__FILE__, __LINE__, \
			"#        found: %d leaks\n", \
			_this); \
})
#define is_leaks(THAT, TESTNAME) \
({ \
    int _this = test_leaks(); \
    int _that = (THAT); \
    test_ok(_this == _that, TESTNAME, test_directive, test_reason, \
			__FILE__, __LINE__, \
			"#        found: %d leaks\n" \
			"#     expected: %d leaks\n", \
           _this, _that); \
})

int test_leaks(void);

#ifdef __cplusplus
}
#endif

#endif /*  _TESTLEAKS_H_ */
