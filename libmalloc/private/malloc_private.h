/*
 * Copyright (c) 1999-2016 Apple Inc. All rights reserved.
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

#ifndef _MALLOC_PRIVATE_H_
#define _MALLOC_PRIVATE_H_

/* Here be dragons (SPIs) */

#include <TargetConditionals.h>
#include <malloc/_platform.h>
#include <Availability.h>
#include <os/availability.h>
#include <os/base.h>
#include <malloc/malloc.h>

#include <malloc/_ptrcheck.h>
__ptrcheck_abi_assume_single()

__BEGIN_DECLS

#if !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT
/* Memorypressure notification mask to use by default */
extern const unsigned long malloc_memorypressure_mask_default_4libdispatch;
/* Memorypressure notification mask to use if MSL has been enabled */
extern const unsigned long malloc_memorypressure_mask_msl_4libdispatch;
#endif // !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT

/*********	Callbacks	************/

#if !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT
API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0))
void malloc_enter_process_memory_limit_warn_mode(void);
	/* A callback invoked once the process receives a warning for approaching
	 * memory limit. */

__OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0)
__TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0)
void malloc_memory_event_handler(unsigned long);
	/* A function invoked when malloc needs to handle any flavor of
	 * memory pressure notification or process memory limit notification. */
#endif // !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT

API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0))
void * __sized_by_or_null(nmemb * size) reallocarray(void * in_ptr,
		size_t nmemb, size_t size) __DARWIN_EXTSN(reallocarray)
		__result_use_check;

API_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0))
void * __sized_by_or_null(nmemb * size) reallocarrayf(void * in_ptr,
		size_t nmemb, size_t size) __DARWIN_EXTSN(reallocarrayf)
		__result_use_check;

/*
 * Checks whether an address might belong to any registered zone. False positives
 * are allowed (e.g. the memory was freed, or it's in a part of the address
 * space used by malloc that has not yet been allocated.) False negatives are
 * not allowed.
 */
API_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0))
boolean_t malloc_claimed_address(void *ptr) __result_use_check;

/*
 * Checks whether an address might belong to a given zone. False positives are
 * allowed (e.g. the memory was freed, or it's in a part of the address space
 * used by malloc that has not yet been allocated.) False negatives are not
 * allowed.
 */
API_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0))
boolean_t malloc_zone_claimed_address(malloc_zone_t *zone, void *ptr)
		__result_use_check;

/**
 * Returns whether the nano allocator (or a roughly equivalent configuration of
 * another system allocator implementation) is engaged. The return value is 0 if
 * Nano is not engaged and the allocator version otherwise.
 */
#if !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT
API_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0))
int malloc_engaged_nano(void) __result_use_check;
#endif // !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT

/**
 * Returns whether the secure allocator is engaged. The return value is 0 if it
 * is not engaged and the allocator version otherwise.
 */
#if !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT
SPI_AVAILABLE(macos(14.3), ios(17.4), tvos(17.4), watchos(10.4),
		driverkit(23.4), visionos(1.1))
int malloc_engaged_secure_allocator(void) __result_use_check;
#endif

/*
 * Disables zero-on-free in a process.  This has security implications and is
 * intended to be used only as part of binary compatibility workarounds for
 * external code.  It should be called as early as possible in the process
 * lifetime, ideally before the process has gone multithreaded.  It is not
 * guaranteed to have any effect.
 */
SPI_AVAILABLE(macos(13.0), ios(16.1), tvos(16.1), watchos(9.1))
void malloc_zero_on_free_disable(void);

/*
 * Certain zone types (e.g., PGM, sanitizer) wrap other zones to add extra
 * behavior and delegate most of the actual work to the wrapped zone.  Retrieves
 * the address of the wrapped zone or NULL for zone types that don't wrap
 * another zone.
 */
#if !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT
SPI_AVAILABLE(macos(14.3), ios(17.4), tvos(17.4), watchos(10.4))
kern_return_t malloc_get_wrapped_zone(task_t task,
		memory_reader_t reader,
		vm_address_t zone_address,
		vm_address_t *wrapped_zone_address) __result_use_check;
#endif // !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT

/****** Thread-specific libmalloc options ******/

/**
 * Options struct: zero means "default options".
 */
typedef struct {
	uintptr_t DisableExpensiveDebuggingOptions : 1;
	uintptr_t DisableProbabilisticGuardMalloc : 1;
	uintptr_t DisableMallocStackLogging : 1;
	uintptr_t ReservedFlag : 1;
} malloc_thread_options_t;

#if !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT
API_AVAILABLE(macos(13.0), ios(16.0), tvos(16.0), watchos(9.0))
malloc_thread_options_t malloc_get_thread_options(void) __result_use_check;

API_AVAILABLE(macos(13.0), ios(16.0), tvos(16.0), watchos(9.0))
void malloc_set_thread_options(malloc_thread_options_t opts);
#endif // !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT

/****** Crash Reporter integration ******/

typedef struct {
	uint64_t thread_id;
	uint64_t time;
	uint32_t num_frames;
	vm_address_t frames[64];
} stack_trace_t;

/**
 * Like memory_reader_t, but caller must free returned memory if not NULL.
 */
typedef void * __sized_by_or_null(size) (*crash_reporter_memory_reader_t)(
		task_t task, vm_address_t address, size_t size);

/****** Probabilistic Guard Malloc ******/

typedef struct {
	// diagnose_page_fault
	const char *error_type;
	const char *confidence;
	vm_address_t fault_address;
	// fill_in_report
	vm_address_t nearest_allocation;
	size_t allocation_size;
	const char *allocation_state;
	uint32_t num_traces;
	// fill_in_trace
	stack_trace_t alloc_trace;
	stack_trace_t dealloc_trace;
} pgm_report_t;

#if !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT
kern_return_t pgm_extract_report_from_corpse(vm_address_t fault_address,
		pgm_report_t *report, task_t task, vm_address_t *zone_addresses,
		uint32_t zone_count, crash_reporter_memory_reader_t crm_reader)
		__result_use_check;
#endif // !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT

/****** Sanitizer Zone ******/

struct malloc_sanitizer_poison {
	// ASAN_HEAP_LEFTRZ: [ptr, ptr + leftrz_sz)
	// ASAN_VALID: [ptr + leftrz_sz, ptr + alloc_sz)
	// ASAN_HEAP_RIGHTRZ: [ptr + leftrz_sz + alloc_sz, ptr + leftrz_sz + alloc_sz + rightrz_sz)
	void (*heap_allocate_poison)(uintptr_t ptr, size_t leftrz_sz,
			size_t alloc_sz, size_t rightrz_sz);
	// ASAN_HEAP_FREED: [ptr, ptr + sz)
	void (*heap_deallocate_poison)(uintptr_t ptr, size_t sz);
	// ASAN_HEAP_INTERNAL: [ptr, ptr + sz)
	void (*heap_internal_poison)(uintptr_t ptr, size_t sz);
};

/* Returns whether sanitizers are enabled */
bool malloc_sanitizer_is_enabled(void);

/* Returns function pointers for interacting with sanitizer */
extern const struct malloc_sanitizer_poison *malloc_sanitizer_get_functions(void);

/* Sets function pointers for interacting with sanitizer */
void malloc_sanitizer_set_functions(struct malloc_sanitizer_poison *);

typedef struct {
	vm_address_t fault_address;
	vm_address_t nearest_allocation;
	size_t allocation_size;
	stack_trace_t alloc_trace;
	stack_trace_t dealloc_trace;
} sanitizer_report_t;

#if !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT
kern_return_t sanitizer_diagnose_fault_from_crash_reporter(
		vm_address_t fault_address, sanitizer_report_t *report, task_t task,
		vm_address_t zone_address, crash_reporter_memory_reader_t crm_reader)
		__result_use_check;
#endif // !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT

/****** Malloc with flags ******/

OS_OPTIONS(malloc_options_np, uint64_t,
	MALLOC_NP_OPTION_CLEAR = 0x1,
);

#if defined(__LP64__) // MALLOC_TARGET_64BIT
/*!
 * @function malloc_type_zone_malloc_with_options_np
 *
 * @discussion
 * When malloc_zone_malloc_with_options_np was introduced, the TMO inference
 * argument index was incorrectly set to the 4th argument (options). As such,
 * TMO rewritten callsites will put the type descriptor in the 5th argument,
 * instead of the 4th. To avoid revlocking and potential bincompat issues, we're
 * leaving this symbol (_with_options_np) in the dylib with the type_id in the
 * fifth argument, and adding the corrected symbol under a different name.
 *
 * This symbol will be deleted at some point in the future when no software is
 * calling through it.
 */
SPI_DEPRECATED("Do not call through typed entrypoint directly",
		macos(14.3, 15.0), ios(17.4, 18.0), tvos(17.4, 18.0),
		watchos(10.4, 11.0), driverkit(23.4, 24.0), visionos(1.1, 2.0))
void * __sized_by_or_null(size) malloc_type_zone_malloc_with_options_np(
		malloc_zone_t *zone, size_t align, size_t size,
		malloc_options_np_t options, malloc_type_id_t desc) __result_use_check
#if defined(__has_attribute) && __has_attribute(diagnose_if)
__attribute__((__diagnose_if__(align & (align-1),
		"alignment should be 0 or a power of 2", "error")))
__attribute__((__diagnose_if__(align && (size % align),
		"size should be an integral multiple of align", "error")))
#endif
		__alloc_size(3) __alloc_align(2);

/*!
 * @function malloc_type_zone_malloc_with_options_internal
 *
 * @discussion
 * This function shouldn't be called directly, the declaration is an
 * implementation detail for malloc_zone_malloc_with_options_np
 */
SPI_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0),
		driverkit(24.0), visionos(2.0))
void * __sized_by_or_null(size) malloc_type_zone_malloc_with_options_internal(
		malloc_zone_t *zone, size_t align, size_t size,
		malloc_type_id_t desc, malloc_options_np_t options) __result_use_check
#if defined(__has_attribute) && __has_attribute(diagnose_if)
__attribute__((__diagnose_if__(align & (align-1),
		"alignment should be 0 or a power of 2", "error")))
__attribute__((__diagnose_if__(align && (size % align),
		"size should be an integral multiple of align", "error")))
#endif
		__alloc_size(3) __alloc_align(2);
#endif // MALLOC_TARGET_64BIT

/*!
 * @function malloc_zone_malloc_with_options_np
 *
 * @param zone
 * The malloc zone that should be used to used to serve the allocation. This
 * parameter may be NULL, in which case the default zone will be used.
 *
 * @param align
 * The minimum alignment of the requested allocation. This parameter must be
 * zero to request no particular alignment, or a power of 2 >= sizeof(void *).
 *
 * @param size
 * The size, in bytes, of the requested allocation. Must be an integral multiple
 * of align if align is non-zero.
 *
 * @param options
 * A bitmask of options defining how the memory should be allocated. See the
 * available bit values in the malloc_options_np_t enum definition.
 *
 * @result
 * A pointer to the newly allocated block of memory, or NULL if the allocation
 * failed.
 *
 * @discussion
 * This SPI does not set errno on all codepaths when the allocation fails.
 */
SPI_AVAILABLE(macos(14.3), ios(17.4), tvos(17.4), watchos(10.4),
		driverkit(23.4), visionos(1.1))
void * __sized_by_or_null(size) malloc_zone_malloc_with_options_np(
		malloc_zone_t *zone, size_t align, size_t size,
		malloc_options_np_t options) __result_use_check
#if defined(__has_attribute) && __has_attribute(diagnose_if)
__attribute__((__diagnose_if__(align & (align-1),
		"alignment should be 0 or a power of 2", "error")))
__attribute__((__diagnose_if__(align && (size % align),
		"size should be an integral multiple of align", "error")))
#endif
		__alloc_size(3) __alloc_align(2)
		_MALLOC_TYPED(malloc_type_zone_malloc_with_options_internal, 3);

#if !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT
// Indicates whether the libmalloc debug dylib is in use in the current process
SPI_AVAILABLE(macos(14.3), ios(17.4), tvos(17.4), watchos(10.4),
		driverkit(23.4), visionos(1.1))
bool
malloc_variant_is_debug_4test(void);

// Indicates whether libmalloc internal security policy is enabled in the
// current process
bool
malloc_allows_internal_security_4test(void);
#endif /* !TARGET_OS_EXCLAVECORE && !TARGET_OS_EXCLAVEKIT */

__END_DECLS

#endif /* _MALLOC_PRIVATE_H_ */
