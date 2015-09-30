/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

#include "resolver_internal.h"

#define _OS_VARIANT_RESOLVER(s, v, ...) \
	__attribute__((visibility(OS_STRINGIFY(v)))) extern void* s(void); \
	void* s(void) { \
	__asm__(".symbol_resolver _" OS_STRINGIFY(s)); \
		__VA_ARGS__ \
	}

#define _OS_VARIANT_UPMP_RESOLVER(s, v) \
	_OS_VARIANT_RESOLVER(s, v, \
		uint32_t *_c = (void*)(uintptr_t)_COMM_PAGE_CPU_CAPABILITIES; \
		if (*_c & kUP) { \
			extern void OS_VARIANT(s, up)(void); \
			return &OS_VARIANT(s, up); \
		} else { \
			extern void OS_VARIANT(s, mp)(void); \
			return &OS_VARIANT(s, mp); \
		})

#define OS_VARIANT_UPMP_RESOLVER(s) \
	_OS_VARIANT_UPMP_RESOLVER(s, default)

#define OS_VARIANT_UPMP_RESOLVER_INTERNAL(s) \
	_OS_VARIANT_UPMP_RESOLVER(s, hidden)


#ifdef OS_VARIANT_SELECTOR

OS_VARIANT_UPMP_RESOLVER(pthread_mutex_lock)
OS_VARIANT_UPMP_RESOLVER(pthread_mutex_trylock)
OS_VARIANT_UPMP_RESOLVER(pthread_mutex_unlock)

#endif // OS_VARIANT_SELECTOR
