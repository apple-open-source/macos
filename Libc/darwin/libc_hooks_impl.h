/*
* Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifndef __LIBC_HOOKS_IMPL_H__
#define __LIBC_HOOKS_IMPL_H__

#include "libc_hooks.h"

// This header file contains libc internal entrypoints to support libsanitizer
// hooks for checking the validity of client memory reads and writes across the
// libc API boundary. When a memory reference is passed into a libc API by the
// client, libc can call one of the following "annotation" style SPIs which will
// indicate that client memory is to be accessed either for reading or writing.
// (NB: Writing can infer reading w/o an additional read check. The calls below
// check their corresponding "hooks" or callbacks if registered, but by default
// the hooks are NULL and no call is made.

// The two primary functions for read and write notification are:
//   void libc_hooks_will_read(const void *p, size_t size);
// and:
//   void libc_hooks_will_write(const void *p, size_t size);

// There are two additional convenience/efficiency read notification functions
// for nul terminated character strings (char *) and wide character strings
// (wchar_t *) as well but should be understood as convenience functions only.

// The string read notification convenience functions are:
//   void libc_hooks_will_read_cstring(const char *s);
// and:
//   void libc_hooks_will_read_wcstring(const wchar_t *wcs);

// In addition to the functions above, there is a specific use case that comes
// up often enough in libc to justify a convenience macro. The use case is when
// a value (say from scanf) is ready to store at an address provided by the
// client and the type is known from context (e.g. a format specifier.

// The macro for storing a value (val) of type (type) at location (ptr) is:
//   LIBC_HOOKS_WRITE_SIMPLE_TYPE(ptr, type, val)

extern libc_hooks_t libc_hooks;

// These functions are used to check client buffers (when hooks are registered)
static inline void libc_hooks_will_read(const void *p, size_t size) {
  if (libc_hooks.will_read)
	return libc_hooks.will_read(p, size);
}

// Inform address sanitizer that a null terminated cstring will be read
static inline void libc_hooks_will_read_cstring(const char *s) {
  if (libc_hooks.will_read_cstring)
	return libc_hooks.will_read_cstring(s);
}

// Inform address sanitizer that a null terminated cstring will be read
static inline void libc_hooks_will_read_wcstring(const wchar_t *wcs) {
  if (libc_hooks.will_read_wcstring)
	return libc_hooks.will_read_wcstring(wcs);
}

// Inform address sanitizer that n bytes of memory starting at p will be written
static inline void libc_hooks_will_write(const void *p, size_t size) {
  if (libc_hooks.will_write)
	return libc_hooks.will_write(p, size);
}

// Convenience macro for storing into client variables from a typeless pointer
// to a location with a known scalar type. Validates writability of the target
// location if appropriate. This situation occurs e.g. while parsing a format
// string in scanf; the address of the corresponding var-arg is a void * pointer,
// the format string defines the type to be stored there, and the converted value
// is held as a universal result of the scan. Here, we have then the ptr, the type
// and the value that should be stored there. The sizeof(type) gives the extent
// of the stored span and so this macros facilitates the store and the (possible)
// validation of the bytes to be written. (The pattern of using the do-while
// construct is only for the ability to use the macro as a statement allowing it
// to be terminated with a semicolon.
#define LIBC_HOOKS_WRITE_SIMPLE_TYPE(ptr, type, val) do { \
	type _val = (val); \
    void *_ptr = (ptr); \
	libc_hooks_will_write((_ptr), sizeof(type)); \
	(*(type *) _ptr) = _val; \
} while(0)

#endif // __LIBC_HOOKS_IMPL_H__
