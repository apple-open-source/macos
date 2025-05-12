/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#include <_liblibc/_asan_runtime.h>

#include "internal.h"

#define MAX_MALLOC_ZONES 2

#define DEFAULT_MALLOC_ZONE_STRING "DefaultXzoneZone"
#define DEFAULT_SANITIZER_ZONE_STRING "DefaultWrapperSanitizerZone"

MALLOC_NOEXPORT
unsigned int phys_ncpus = 0;

MALLOC_NOEXPORT
unsigned int logical_ncpus = 0;

unsigned malloc_num_zones = 0;
static malloc_zone_t *_malloc_zones[MAX_MALLOC_ZONES] = { NULL };
malloc_zone_t ** __unsafe_indexable malloc_zones = _malloc_zones;

bool malloc_sanitizer_enabled = false;


#if __LIBLIBC_F_ASAN_INSTRUMENTATION
static struct malloc_sanitizer_poison malloc_poison_default = {
	.heap_allocate_poison = __asan_poison_heap_memory_alloc,
	.heap_deallocate_poison = __asan_poison_heap_memory_free,
	.heap_internal_poison = __asan_poison_heap_memory_internal,
};
static struct malloc_sanitizer_poison *malloc_poison = &malloc_poison_default;
#else
static struct malloc_sanitizer_poison *malloc_poison = NULL;
#endif // __LIBLIBC_F_ASAN_INSTRUMENTATION

MALLOC_NOEXPORT
malloc_zero_policy_t malloc_zero_policy = MALLOC_ZERO_POLICY_DEFAULT;

static inline malloc_zone_t *
_find_registered_zone(const void * __unsafe_indexable ptr, size_t *returned_size,
	bool known_non_default)
{
	malloc_zone_t *zone;
	size_t size;

	// We assume that the initial zones will never be unregistered concurrently
	// while this code is running so we can have a fast path without
	// synchronization.  Callers who really do unregister these (to install
	// their own default zone) need to ensure they establish their zone setup
	// during initialization and before entering a multi-threaded environment.
	for (uint32_t i = known_non_default ? 1 : 0; i < malloc_num_zones; i++) {
		zone = _malloc_zones[i];
		size = zone->size(zone, ptr);

		if (size) { // Claimed by this zone?
			if (returned_size) {
				*returned_size = size;
			}

			return zone;
		}
	}

	// Unclaimed by any zone.
	zone = NULL;
	size = 0;

	if (returned_size) {
		*returned_size = size;
	}
	return zone;
}

malloc_zone_t *
find_registered_zone(const void * __unsafe_indexable ptr, size_t *returned_size,
	bool known_non_default)
{
	return _find_registered_zone(ptr, returned_size, known_non_default);
}

/*********  Creation and destruction    ************/

static void
_malloc_zone_register(malloc_zone_t *zone, bool make_default)
{
	/* scan the list of zones, to see if this zone is already registered.  If
	 * so, print an error message and return. */
	for (unsigned i = 0; i < malloc_num_zones; ++i) {
		if (zone == _malloc_zones[i]) {
			malloc_report(MALLOC_REPORT_CRASH,
				"Attempted to register duplicate zone: %p\n", zone);
			return;
		}
	}

	/* maximum number of zones has been reached */
	if (malloc_num_zones == MAX_MALLOC_ZONES) {
		malloc_report(MALLOC_REPORT_CRASH, "No capacity for zone: %p\n", zone);
		return;
	}

	/* unsupported zone version */
	if (zone->version < 13) {
		malloc_report(MALLOC_REPORT_CRASH, "Unsupported zone version: %u\n",
			zone->version);
		return;
	}

	if (make_default) {
		memmove(&_malloc_zones[1], &_malloc_zones[0],
			malloc_num_zones * sizeof(malloc_zone_t *));
		_malloc_zones[0] = zone;
	} else {
		_malloc_zones[malloc_num_zones] = zone;
	}

	malloc_report(ASL_LEVEL_INFO, "Registered zone %p at index %u\n", zone,
		malloc_num_zones);

	++malloc_num_zones;
}

void __malloc_init(const char * __null_terminated * __null_terminated args)
{
	logical_ncpus = _liblibc_plat_num_cpus;
	phys_ncpus = _liblibc_plat_num_cpus;


	const unsigned malloc_debug_flags = MALLOC_ABORT_ON_CORRUPTION |
			MALLOC_ABORT_ON_ERROR;
	mfm_initialize();
	malloc_zone_t *xzone = xzm_main_malloc_zone_create(malloc_debug_flags,
			NULL, args, NULL);
	_malloc_zone_register(xzone, true);
	malloc_set_zone_name(xzone, DEFAULT_MALLOC_ZONE_STRING);

#if __LIBLIBC_F_ASAN_INSTRUMENTATION
	if ((malloc_sanitizer_enabled = sanitizer_should_enable())) {
		malloc_zone_t *sanitizer = sanitizer_create_zone(xzone);
		_malloc_zone_register(sanitizer, true);
		malloc_set_zone_name(sanitizer, DEFAULT_SANITIZER_ZONE_STRING);
	}
#endif // __LIBLIBC_F_ASAN_INSTRUMENTATION
}

malloc_zone_t* malloc_default_zone(void)
{
	return _malloc_zones[0];
};

/*********  Block creation and manipulation ************/

void * __sized_by_or_null(size)
_malloc_zone_malloc(malloc_zone_t *zone, size_t size, malloc_zone_options_t mzo)
{
	// This and similar conditionals are commented out to avoid compiler
	// warnings on unreachable code
	// if (os_unlikely(malloc_too_large(size))) {
	// 	malloc_set_errno_fast(mzo, ENOMEM);
	// 	return NULL;
	// }

	// zone versions >= 13 set errno on failure so we can tail-call
	return zone->malloc(zone, size);
}

MALLOC_NOINLINE
void * __sized_by_or_null(size)
malloc_zone_malloc(malloc_zone_t *zone, size_t size)
{
	return _malloc_zone_malloc(zone, size, MZ_NONE);
}

void * __sized_by_or_null(num_items * size)
_malloc_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size,
		malloc_zone_options_t mzo)
{
	size_t total_bytes;
	if (calloc_get_size(num_items, size, 0, &total_bytes)) {
		malloc_set_errno_fast(mzo, ENOMEM);
		return NULL;
	}

	// zone versions >= 13 set errno on failure so we can tail-call
	return __unsafe_forge_bidi_indexable(void *,
		zone->calloc(zone, num_items, size), total_bytes);
}

MALLOC_NOINLINE
void * __sized_by_or_null(num_items * size)
malloc_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size)
{
	return _malloc_zone_calloc(zone, num_items, size, MZ_NONE);
}

void * __sized_by_or_null(size)
_malloc_zone_valloc(malloc_zone_t *zone, size_t size, malloc_zone_options_t mzo)
{
	// if (os_unlikely(malloc_too_large(size))) {
	// 	malloc_set_errno_fast(MZ_NONE, ENOMEM);
	// 	return NULL;
	// }

	void *ptr = zone->valloc(zone, size);
	if (os_unlikely(ptr == NULL)) {
		malloc_set_errno_fast(mzo, ENOMEM);
	}

	return ptr;
}

MALLOC_NOINLINE
void * __sized_by_or_null(size)
malloc_zone_valloc(malloc_zone_t *zone, size_t size)
{
	return _malloc_zone_valloc(zone, size, MZ_NONE);
}

void * __sized_by_or_null(size)
_malloc_zone_realloc(malloc_zone_t *zone, void * __unsafe_indexable ptr,
		size_t size, malloc_type_descriptor_t type_desc)
{
	// if (os_unlikely(malloc_too_large(size))) {
	// 	return NULL;
	// }

	return zone->realloc(zone, ptr, size);
}

MALLOC_NOINLINE
void * __sized_by_or_null(size)
malloc_zone_realloc(malloc_zone_t *zone, void * __unsafe_indexable ptr,
		size_t size)
{
	return _malloc_zone_realloc(zone, ptr, size, MALLOC_TYPE_DESCRIPTOR_NONE);
}

MALLOC_NOINLINE
void
malloc_zone_free(malloc_zone_t *zone, void * __unsafe_indexable ptr)
{
	zone->free(zone, ptr);
}

static void
malloc_zone_free_definite_size(malloc_zone_t *zone, void * __sized_by(size) ptr,
	size_t size)
{
	zone->free_definite_size(zone, ptr, size);
}

malloc_zone_t *
malloc_zone_from_ptr(const void * __unsafe_indexable ptr)
{
	if (!ptr) {
		return NULL;
	} else {
		return _find_registered_zone(ptr, NULL, false);
	}
}

void * __alloc_align(2) __alloc_size(3) __sized_by_or_null(size)
_malloc_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size,
	malloc_zone_options_t mzo, malloc_type_descriptor_t type_desc)
{
	void * __bidi_indexable ptr = NULL;
	int err = ENOMEM;

	// if (os_unlikely(malloc_too_large(size))) {
	// 	goto out;
	// }

	// excludes 0 == alignment
	// relies on sizeof(void *) being a power of two.
	if (alignment < sizeof(void *) ||
			0 != (alignment & (alignment - 1))) {
		err = EINVAL;
		goto out;
	}
	// C11 aligned_alloc requires size to be a multiple of alignment, but
	// posix_memalign does not
	if ((mzo & MZ_C11) && (size & (alignment - 1)) != 0) {
		err = EINVAL;
		goto out;
	}

	if (!(zone->memalign)) {
		goto out;
	}
	ptr = zone->memalign(zone, alignment, size);

out:
	if (os_unlikely(ptr == NULL)) {
		if (mzo & MZ_POSIX) {
			malloc_set_errno_fast(mzo, err);
		}
	}
	return ptr;
}

MALLOC_NOINLINE
void * __sized_by_or_null(size)
malloc_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size)
{
	return _malloc_zone_memalign(zone, alignment, size, MZ_NONE,
			MALLOC_TYPE_DESCRIPTOR_NONE);
}

MALLOC_NOINLINE
void * __sized_by_or_null(size)
malloc_zone_malloc_with_options_np(malloc_zone_t *zone, size_t align,
		size_t size, malloc_options_np_t options)
{
	if (os_unlikely((align != 0) && (!powerof2(align) ||
			((size & (align-1)) != 0)))) { // equivalent to (size % align != 0)
		return NULL;
	}

	if (zone == NULL) {
		zone = malloc_zones[0];
	}

	if (zone->version >= 15 && zone->malloc_with_options) {
		return zone->malloc_with_options(zone, align, size, options);
	}

	if (align) {
		void *ptr = zone->memalign(zone, align, size);
		if (ptr && (options & MALLOC_NP_OPTION_CLEAR)) {
			memset(ptr, 0, size);
		}
		return ptr;
	} else if (options & MALLOC_NP_OPTION_CLEAR) {
		return zone->calloc(zone, 1, size);
	} else {
		return zone->malloc(zone, size);
	}
}

boolean_t
malloc_zone_claimed_address(malloc_zone_t *zone, void *ptr)
{
	if (!ptr) {
		// NULL is not a member of any zone.
		return false;
	}

	if (!zone->claimed_address) {
		// For zones that have not implemented claimed_address, we always have
		// to return true to avoid a false negative.
		return true;
	}

	return zone->claimed_address(zone, ptr);
}

/*********    Functions for zone implementors    ************/

void
malloc_set_zone_name(malloc_zone_t *z, const char *name)
{
	if (z->zone_name) {
		malloc_zone_t *old_zone = _find_registered_zone(z->zone_name, NULL,
			false);
		if (old_zone) {
			malloc_zone_free(old_zone, (char *)z->zone_name);
		}
		z->zone_name = NULL;
	}

	if (name) {
		const size_t buflen = strlen(name) + 1;
		char * __sized_by(buflen) name_copy =
			__unsafe_forge_bidi_indexable(char *, malloc_zone_malloc(z, buflen),
				buflen);
		if (name_copy) {
			strlcpy(name_copy, name, buflen);
			z->zone_name = __unsafe_null_terminated_from_indexable(name_copy,
				name_copy + buflen - 1);
		}
	}
}

const char *
malloc_get_zone_name(malloc_zone_t *zone)
{
	return zone->zone_name;
}

void
find_zone_and_free(void * __unsafe_indexable ptr, bool known_non_default)
{
	malloc_zone_t *zone;
	size_t size;
	if (!ptr) {
		return;
	}

	zone = _find_registered_zone(ptr, &size, known_non_default);
	if (!zone) {
		malloc_report_pointer_was_not_allocated(MALLOC_REPORT_CRASH, ptr);
	} else if (zone->free_definite_size) {
		malloc_zone_free_definite_size(zone,
			__unsafe_forge_bidi_indexable(void *, ptr, size), size);
	} else {
		malloc_zone_free(zone, ptr);
	}
}

/*********    Generic ANSI callouts    ************/

void * __sized_by_or_null(size)
malloc(size_t size)
{
	return malloc_zone_malloc(_malloc_zones[0], size);
}

void * __sized_by_or_null(size)
aligned_alloc(size_t alignment, size_t size)
{
	return _malloc_zone_memalign(_malloc_zones[0], alignment, size,
		MZ_POSIX | MZ_C11, MALLOC_TYPE_DESCRIPTOR_NONE);
}

void * __sized_by_or_null(num_items * size)
calloc(size_t num_items, size_t size)
{
	return malloc_zone_calloc(_malloc_zones[0], num_items, size);
}

void
_free(void * __unsafe_indexable ptr)
{
	if (!ptr) {
		return;
	}

	malloc_zone_t * __single zone0 = _malloc_zones[0];
	if (zone0->try_free_default) {
		zone0->try_free_default(zone0, ptr);
	} else {
		find_zone_and_free(ptr, false);
	}
}

void
free(void * __unsafe_indexable ptr)
{
	return _free(ptr);
}

void * __sized_by_or_null(new_size)
_realloc(void * __unsafe_indexable in_ptr, size_t new_size)
{
	void * __bidi_indexable retval = NULL;
	void * __unsafe_indexable old_ptr;
	malloc_zone_t *zone;

	// SUSv3: "If size is 0 and ptr is not a null pointer, the object
	// pointed to is freed. If the space cannot be allocated, the object
	// shall remain unchanged."  Also "If size is 0, either a null pointer
	// or a unique pointer that can be successfully passed to free() shall
	// be returned."  We choose to allocate a minimum size object by calling
	// malloc_zone_malloc with zero size, which matches "If ptr is a null
	// pointer, realloc() shall be equivalent to malloc() for the specified
	// size."  So we only free the original memory if the allocation succeeds.
	old_ptr = (new_size == 0) ? NULL : in_ptr;
	if (!old_ptr) {
		retval = malloc_zone_malloc(_malloc_zones[0], new_size);
	} else {
		zone = _find_registered_zone(old_ptr, NULL, false);
		if (!zone) {
			malloc_report_pointer_was_not_allocated(MALLOC_REPORT_CRASH,
					in_ptr);
		} else {
			retval = malloc_zone_realloc(zone, old_ptr, new_size);
		}
	}

	if (retval == NULL) {
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
	} else if (new_size == 0) {
		free(in_ptr);
	}
	return retval;
}

void * __sized_by_or_null(new_size)
realloc(void * __unsafe_indexable in_ptr, size_t new_size)
{
	return _realloc(in_ptr, new_size);
}

void * __sized_by_or_null(new_size)
reallocf(void * __unsafe_indexable in_ptr, size_t new_size)
{
	void *ptr = realloc(in_ptr, new_size);

	if (!ptr && in_ptr && new_size != 0) {
		// Save and restore `errno`, because `realloc` will set it to ENOMEM
		// on allocation failure, but it could be overwritten if `free` calls
		// into a library function that also modifies `errno`
		errno_t error = errno;
		free(in_ptr);
		errno = error;
	}

	return ptr;
}

void * __sized_by_or_null(size)
valloc(size_t size)
{
	return _malloc_zone_valloc(_malloc_zones[0], size, MZ_POSIX);
}

size_t
malloc_size(const void * __unsafe_indexable ptr)
{
	size_t size = 0;

	if (!ptr) {
		return size;
	}

	(void)_find_registered_zone(ptr, &size, false);
	return size;
}

size_t
malloc_good_size(size_t size)
{
	malloc_zone_t * __single zone = _malloc_zones[0];
	return zone->introspect->good_size(zone, size);
}

int
_posix_memalign(void * __unsafe_indexable *memptr, size_t alignment,
		size_t size)
{
	void * __bidi_indexable retval;

	/* POSIX is silent on NULL == memptr !?! */

	retval = malloc_zone_memalign(_malloc_zones[0], alignment, size);
	if (retval == NULL) {
		// To avoid testing the alignment constraints redundantly, we'll rely on
		// the test made in malloc_zone_memalign to vet each request. Only if
		// that test fails and returns NULL, do we arrive here to detect the
		// bogus alignment and give the required EINVAL return.
		if (alignment < sizeof(void *) ||             // excludes 0 == alignment
				0 != (alignment & (alignment - 1))) { // relies on sizeof(void *)
													  // being a power of two.
			return EINVAL;
		}
		return ENOMEM;
	} else {
		*memptr = retval; // Set iff allocation succeeded
		return 0;
	}
}

int
posix_memalign(void * __unsafe_indexable *memptr, size_t alignment, size_t size)
{
	return _posix_memalign(memptr, alignment, size);
}

boolean_t
malloc_claimed_address(void *ptr)
{
	// We need to check with each registered zone whether it claims "ptr".
	// Use logic similar to that in find_registered_zone().
	if (malloc_num_zones == 0) {
		return false;
	}

	// Next, try the initial zones.
	for (uint32_t i = 0; i < malloc_num_zones; i++) {
		if (malloc_zone_claimed_address(_malloc_zones[i], ptr)) {
			return true;
		}
	}

	return false;
}

void * __sized_by_or_null(nmemb * size)
reallocarray(void * in_ptr, size_t nmemb, size_t size)
{
	size_t alloc_size;
	if (os_mul_overflow(nmemb, size, &alloc_size)){
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
		return NULL;
	}
	return realloc(in_ptr, alloc_size);
}

void * __sized_by_or_null(nmemb * size)
reallocarrayf(void * in_ptr, size_t nmemb, size_t size)
{
	size_t alloc_size;
	if (os_mul_overflow(nmemb, size, &alloc_size)){
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
		return NULL;
	}
	return reallocf(in_ptr, alloc_size);
}

/*********  Functions for sanitization ************/

bool malloc_sanitizer_is_enabled(void)
{
	return malloc_sanitizer_enabled;
}

const struct malloc_sanitizer_poison *
malloc_sanitizer_get_functions(void)
{
	return malloc_poison;
}

void
malloc_sanitizer_set_functions(struct malloc_sanitizer_poison *s)
{
	malloc_poison = s;
}

/*********  Debug helpers   ************/

void
malloc_zone_print_ptr_info(void * __unsafe_indexable ptr)
{
	malloc_zone_t *zone;
	if (!ptr) {
		return;
	}
	zone = malloc_zone_from_ptr(ptr);
	if (zone) {
		printf("ptr %p in registered zone %p\n", ptr, zone);
	} else {
		printf("ptr %p not in heap\n", ptr);
	}
}

boolean_t
malloc_zone_check(malloc_zone_t *zone)
{
	boolean_t ok = 1;
	if (!zone) {
		unsigned index = 0;
		while (index < malloc_num_zones) {
			zone = _malloc_zones[index++];
			if (!zone->introspect->check(zone)) {
				ok = 0;
			}
		}
	} else {
		ok = zone->introspect->check(zone);
	}
	return ok;
}

void
malloc_zone_print(malloc_zone_t *zone, boolean_t verbose)
{
	if (!zone) {
		unsigned index = 0;
		while (index < malloc_num_zones) {
			zone = _malloc_zones[index++];
			zone->introspect->print(zone, verbose);
		}
	} else {
		zone->introspect->print(zone, verbose);
	}
}

/*********  Misc other entry points ************/

void
malloc_zero_on_free_disable(void)
{
	malloc_zone_error(MALLOC_ABORT_ON_ERROR, false,
			"xzone cannot disable zero on free");
}
