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
    auto_gdb_interface.h
    Routines called by gdb to implement its info gc-references and gc-roots commands.
    Copyright (c) 2007-2008 Apple Inc. All rights reserved.
 */

#ifndef __AUTO_GDB_INTERFACE__
#define __AUTO_GDB_INTERFACE__

#include <stdint.h>
#include <sys/types.h>
#include <auto_zone.h>

__BEGIN_DECLS

enum {
    auto_memory_block_global = 0,
    auto_memory_block_stack,
    auto_memory_block_object,
    auto_memory_block_bytes,
    auto_memory_block_association
};
typedef uint32_t auto_memory_block_kind_t;

struct auto_memory_reference {
    void                           *address;
    intptr_t                        offset;
    auto_memory_block_kind_t        kind;
    uint32_t                        retainCount;
};
typedef struct auto_memory_reference auto_memory_reference_t;

struct auto_memory_reference_list {
    uint32_t                        count;
    auto_memory_reference_t         references[0];
};
typedef struct auto_memory_reference_list auto_memory_reference_list_t;

struct auto_root_list {
    uint32_t                        count;
    auto_memory_reference_list_t    roots[0];       // variable length structure, size = count * sizeof(auto_memory_reference_t) + sum(i, 0, count) { roots[i].count * sizeof(auto_memory_reference_t) }
};
typedef struct auto_root_list auto_root_list_t;

extern auto_memory_reference_list_t *auto_gdb_enumerate_references(auto_zone_t *zone, void *address, void *stack_base);

extern auto_root_list_t *auto_gdb_enumerate_roots(auto_zone_t *zone, void *address, void *stack_base);

__END_DECLS

#endif /* __AUTO_GDB_INTERFACE__ */
