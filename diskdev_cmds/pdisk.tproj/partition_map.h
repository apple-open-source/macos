/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
//
// partition_map.h - partition map routines
//
// Written by Eryk Vershen (eryk@apple.com)
//

/*
 * Copyright 1996 by Apple Computer, Inc.
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */

#include "dpme.h"


//
// Defines
//


//
// Types
//
struct partition_map_header {
    struct media *fd;
    char *name;
    struct partition_map * disk_order;
    struct partition_map * base_order;
    Block0 *misc;
    int writeable;
    int changed;
    int regular_file;
    int physical_block;		// must be == sbBlockSize
    int logical_block;		// must be <= physical_block
    int blocks_in_map;
    int maximum_in_map;
    unsigned long media_size;	// in logical_blocks
};
typedef struct partition_map_header partition_map_header;

struct partition_map {
    struct partition_map * next_on_disk;
    struct partition_map * prev_on_disk;
    struct partition_map * next_by_base;
    struct partition_map * prev_by_base;
    long disk_address;
    struct partition_map_header * the_map;
    int contains_driver;
    DPME *data;
};
typedef struct partition_map partition_map;


//
// Global Constants
//
extern const char * kFreeType;
extern const char * kMapType;
extern const char * kUnixType;
extern const char * kHFSType;
extern const char * kFreeName;


//
// Global Variables
//


//
// Forward declarations
//
int add_partition_to_map(const char *name, const char *dptype, u32 base, u32 length, partition_map_header *map);
void close_partition_map(partition_map_header *map);
partition_map_header* create_partition_map(char *name, partition_map_header *oldmap);
void delete_partition_from_map(partition_map *entry);
partition_map* find_entry_by_disk_address(long index, partition_map_header *map);
partition_map* find_entry_by_base(u32 base, partition_map_header *map);
partition_map* find_entry_of_type(u8 * type, int instance, 
				  partition_map_header *map);
partition_map* find_entry_with_name(u8 * name, int instance, 
				    partition_map_header *map);
partition_map_header* init_partition_map(char *name, partition_map_header* oldmap);
void move_entry_in_map(long old_index, long index, partition_map_header *map);
partition_map_header* open_partition_map(char *name, int *valid_file, int ask_logical_size);
void resize_map(long new_size, partition_map_header *map);
void write_partition_map(partition_map_header *map);
