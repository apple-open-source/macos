/*
 * Copyright (c) 2007-2013 Apple Inc. All rights reserved.
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

#include <os/assumes.h>
#include <stdint.h>
#include <TargetConditionals.h>

__attribute__ ((visibility ("hidden")))
uint32_t __chk_assert_no_overlap = 1;

__attribute__ ((visibility ("hidden")))
__attribute__ ((noreturn))
void
__chk_fail_overflow (void) {
  os_crash("detected buffer overflow");
}

__attribute__ ((visibility ("hidden")))
__attribute__ ((noreturn))
void
__chk_fail_overlap (void) {
  os_crash("detected source and destination buffer overlap");
}

__attribute__ ((visibility ("hidden")))
void
__chk_overlap (const void *_a, size_t an, const void *_b, size_t bn) {
  uintptr_t a = (uintptr_t)_a;
  uintptr_t b = (uintptr_t)_b;

  if (__builtin_expect(an == 0 || bn == 0, 0)) {
    return;
  } else if (__builtin_expect(a == b, 0)) {
    __chk_fail_overlap();
  } else if (a < b) {
    if (__builtin_expect(a + an > b, 0))
      __chk_fail_overlap();
  } else { // b < a
    if (__builtin_expect(b + bn > a, 0))
      __chk_fail_overlap();
  }
}
