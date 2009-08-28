/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
/* 
    auto_weak.h
    Weak reference accounting
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#ifndef __AUTO_WEAK__
#define __AUTO_WEAK__

#include "auto_impl_utilities.h"

__BEGIN_DECLS

namespace Auto {
    class Zone;
}

// clear references to garbage
extern auto_weak_callback_block_t *weak_clear_references(Auto::Zone *azone, size_t garbage_count, vm_address_t *garbage, uintptr_t *weak_referents_count, uintptr_t *weak_refs_count);

// register a new weak reference
extern void weak_register(Auto::Zone *azone, const void *referent, void **referrer, auto_weak_callback_block_t *block);

// unregister an existing weak reference
extern void weak_unregister(Auto::Zone *azone, const void *referent, void **referrer);

// unregister all weak references from a block.
extern void weak_unregister_with_layout(Auto::Zone *azone, void *block[], const unsigned char *map);

// call all registered weak reference callbacks.
extern void weak_call_callbacks(auto_weak_callback_block_t *block);

// unregister all weak references within a known address range.
extern void weak_unregister_data_segment(Auto::Zone *azone, void *base, size_t size);

// dump all weak registrations
extern void weak_dump_table(Auto::Zone *azone, void (^weak_dump)(const void **address, const void *item));

__END_DECLS

#endif /* __AUTO_WEAK__ */
