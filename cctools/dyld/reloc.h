/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
#import <mach-o/nlist.h>
#import <mach-o/reloc.h>

#import "images.h"

extern void relocate_modules_being_linked(
    enum bool launching_with_prebound_libraries);

extern void resolve_external_relocations_in_object_image(
    struct object_image *object_image);

extern void local_relocation(
    struct image *image);

extern void undo_prebound_lazy_pointers(
    struct image *image,
    unsigned long PB_LA_PTR_r_type,
    enum bool all_lazy_pointers,
    unsigned long lazy_pointer_address);

extern enum link_state external_relocation(
    struct image *image,
    struct relocation_info *relocs,
    unsigned long nrelocs,
    struct nlist *symbols,
    char *strings,
    char *library_name,
    char *module_name);

extern void undo_external_relocation(
    enum bool undo_prebinding,
    struct image *image,
    struct relocation_info *relocs,
    unsigned long nrelocs,
    struct nlist *symbols,
    char *strings,
    char *library_name,
    char *module_name);

extern void undo_prebinding_for_library_module(
    module_state *module,
    struct image *image,
    struct library_image *library_image);

extern void make_image_writable(
    struct image *image,
    char *image_type);

extern void restore_image_vm_protections(
    struct image *image,
    char *image_type);
