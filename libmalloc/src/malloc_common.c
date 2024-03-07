/*
 * Copyright (c) 2018-2023 Apple Inc. All rights reserved.
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

#include "internal.h"

#pragma mark -
#pragma mark Utility Functions

// libplatform does not have strstr() and we don't want to add any new
// dependencies on libc, so we have to implement a version of strstr()
// here. Fortunately, as it's only used to look for boot arguments, it does not
// have to be efficient. We can also assume that the source string is
// nul-terminated. Eventually, we will move the function to a more central
// location and use it to replace other uses of strstr().
const char * __null_terminated
malloc_common_strstr(const char * __null_terminated src, const char * __counted_by(target_len) target, size_t target_len)
{
#if !MALLOC_TARGET_EXCLAVES
	const char *next = src;
	while (*next) {
		if (!strncmp(next, target, target_len)) {
			return next;
		}
		next++;
	}
	return NULL;
#else
	return strstr(src, __unsafe_null_terminated_from_indexable(target, target + target_len));
#endif // MALLOC_TARGET_EXCLAVES
}

// Converts a string to a long. If a non-numeric value is found, the
// return value is whatever has been accumulated so far. end_ptr always points
// to the character that caused the conversion to stop. We can't use strtol()
// etc because that would add a new dependency on libc. Eventually, this
// function could be made generally available within the library and used to
// replace the existing calls to strtol(). Currenly only handles non-negative
// numbers and does not detect overflow.
long
malloc_common_convert_to_long(const char * __null_terminated ptr, const char * __null_terminated *end_ptr)
{
#if !MALLOC_TARGET_EXCLAVES
	long value = 0;
	while (*ptr) {
		char c = *ptr;
		if (c < '0' || c > '9') {
			break;
		}
		value = value * 10 + (c - '0');
		ptr++;
	}
	*end_ptr = ptr;
	return value;
#else
	return strtol(ptr, (char * __null_terminated *)end_ptr, 10);
#endif // MALLOC_TARGET_EXCLAVES
}

// Looks for a sequence of the form "key=value" in the string 'src' and
// returns the location of the first character of 'value', or NULL if not
// found. No spaces are permitted around the "=".
const char * __null_terminated
malloc_common_value_for_key(const char * __null_terminated src, const char * __null_terminated key)
{
	const char * __null_terminated ptr = src;
	size_t keylen = strlen(key);
	while ((ptr = malloc_common_strstr(ptr, __unsafe_forge_bidi_indexable(const char *, key, keylen), keylen)) != NULL) {
		// Workaround for indexable pointers being incrementable by one only
		for (size_t i = 0; i < keylen; ++i) {
			++ptr;
		}
		if (*ptr == '=') {
			return ptr + 1;
		}
	}
	return NULL;
}

// Looks for a sequence of the form "key=value" in the string 'src' and
// returns the location of the first character of 'value'. No spaces are
// permitted around the "=". The value is copied to 'bufp', up to the first
// whitespace or nul character and bounded by maxlen, and nul-terminated.
// Returns bufp if the key was found, NULL if not.
const char * __null_terminated
malloc_common_value_for_key_copy(const char * __null_terminated src, const char * __null_terminated key,
							   char * __counted_by(maxlen) bufp, size_t maxlen)
{
	const char * __null_terminated ptr = malloc_common_value_for_key(src, key);
	if (ptr) {
		size_t to_len = maxlen;
		char * __counted_by(to_len) to = bufp;
		while (to_len > 1) { // Always leave room for a '\0'
			char c = *ptr++;
			if (c == '\0' || c == ' ' || c == '\t' || c == '\n') {
				break;
			}
			*to = c;
			++to;
			to_len--;
		}
		*to = '\0';	// Always nul-terminate
		return __unsafe_null_terminated_from_indexable(bufp, to);
	}
	return NULL;
}

unsigned
malloc_zone_batch_malloc_fallback(malloc_zone_t *zone, size_t size,
		void * __unsafe_indexable * __counted_by(num_requested)  results, unsigned num_requested)
{
	unsigned allocated;
	for (allocated = 0; allocated < num_requested; allocated++) {
		void *ptr = zone->malloc(zone, size);
		if (!ptr) {
			break;
		}

		results[allocated] = ptr;
	}

	return allocated;
}

void
malloc_zone_batch_free_fallback(malloc_zone_t *zone,
		void * __unsafe_indexable * __counted_by(count) to_be_freed,
		unsigned count)
{
	for (unsigned i = 1; i <= count; i++) {
		// Note: we iterate backward because nano and magazine malloc both do,
		// although that seems likely to just be a vestigial codegen
		// optimization for ancient non-optimizing compilers
		void * __unsafe_indexable ptr = to_be_freed[count - i];
		if (ptr) {
			zone->free(zone, ptr);
		}
	}
}

size_t
malloc_zone_pressure_relief_fallback(malloc_zone_t *zone, size_t goal)
{
	return 0;
}
