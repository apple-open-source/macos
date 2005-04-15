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
#import <ar.h>
#include "stuff/target_arch.h"
#import <mach-o/fat.h>
#import <mach-o/loader.h>
#import <mach-o/nlist.h>
#import <mach-o/reloc.h>
#import <stuff/bytesex.h>
#import <stuff/bool.h>
#include "stuff/symbol.h"

extern void print_fat_headers(
    struct fat_header *fat_header,
    struct fat_arch *fat_archs,
    unsigned long size,
    enum bool verbose);

extern void print_ar_hdr(
    struct ar_hdr *ar_hdr,
    char *member_name,
    unsigned long member_name_size,
    enum bool verbose);

extern void print_library_toc(
    struct ar_hdr *toc_ar_hdr,
    char *toc_name,
    unsigned long toc_name_size,
    char *toc_addr,
    unsigned long toc_size,
    enum byte_sex toc_byte_sex,
    char *library_name,
    char *library_addr,
    unsigned long library_size,
    char *arch_name,
    enum bool verbose);

extern void print_mach_header(
    mach_header_t *mh,
    enum bool verbose);

extern void print_loadcmds(
    mach_header_t *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    unsigned long object_size,
    enum bool verbose,
    enum bool very_verbose);

extern void print_libraries(
    mach_header_t *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    enum bool just_id,
    enum bool verbose);

extern void print_reloc(
    mach_header_t *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    nlist_t *symbols,
    long nsymbols,
    char *strings,
    long strings_size,
    enum bool verbose);

extern void print_toc(
    mach_header_t *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    struct dylib_table_of_contents *tocs,
    unsigned long ntocs,
    dylib_module_t *mods,
    unsigned long nmods,
    nlist_t *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strings_size,
    enum bool verbose);

extern void print_module_table(
    mach_header_t *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    dylib_module_t *mods,
    unsigned long nmods,
    nlist_t *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strings_size,
    enum bool verbose);

void print_refs(
    mach_header_t *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    struct dylib_reference *refs,
    unsigned long nrefs,
    dylib_module_t *mods,
    unsigned long nmods,
    nlist_t *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strings_size,
    enum bool verbose);

extern void print_indirect_symbols(
    mach_header_t *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    unsigned long *indirect_symbols,
    unsigned long nindirect_symbols,
    nlist_t *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strings_size,
    enum bool verbose);

extern void print_hints(
    mach_header_t *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    struct twolevel_hint *hints,
    unsigned long nhints,
    nlist_t *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strings_size,
    enum bool verbose);

extern void print_segment_command(
    segment_command_t *sg,
    unsigned long object_size,
    enum bool verbose);

extern void print_section(
    section_t *s,
    segment_command_t *sg,
    mach_header_t *mh,
    unsigned long object_size,
    enum bool verbose);

extern void print_symtab_command(
    struct symtab_command *sg,
    unsigned long object_size);

extern void print_dysymtab_command(
    struct dysymtab_command *dyst,
    unsigned long nsyms,
    unsigned long object_size);

extern void print_symseg_command(
    struct symseg_command *ss,
    unsigned long object_size);

extern void print_fvmlib_command(
    struct fvmlib_command *fl,
    struct load_command *lc);

extern void print_dylib_command(
    struct dylib_command *dl,
    struct load_command *lc);

extern void print_sub_framework_command(
    struct sub_framework_command *sub,
    struct load_command *lc);

extern void print_sub_umbrella_command(
    struct sub_umbrella_command *usub,
    struct load_command *lc);

extern void print_sub_library_command(
    struct sub_library_command *lsub,
    struct load_command *lc);

extern void print_sub_client_command(
    struct sub_client_command *csub,
    struct load_command *lc);

extern void print_prebound_dylib_command(
    struct prebound_dylib_command *pbdylib,
    struct load_command *lc,
    enum bool verbose);

extern void print_dylinker_command(
    struct dylinker_command *dyld,
    struct load_command *lc);

extern void print_fvmfile_command(
    struct fvmfile_command *ff,
    struct load_command *lc);

extern void print_routines_command(
    routines_command_t *rc);

extern void print_twolevel_hints_command(
    struct twolevel_hints_command *hints,
    unsigned long object_size);

extern void print_prebind_cksum_command(
    struct prebind_cksum_command *cksum);

extern void print_thread_states(
    char *begin, 
    char *end,
    mach_header_t *mh,
    enum byte_sex thread_states_byte_sex);

extern void print_cstring_section(
    char *sect,
    unsigned long sect_size,
    unsigned long sect_addr,
    enum bool print_addresses);

extern void print_literal4_section(
    char *sect,
    unsigned long sect_size,
    unsigned long sect_addr,
    enum byte_sex literal_byte_sex,
    enum bool print_addresses);

extern void print_literal8_section(
    char *sect,
    unsigned long sect_size,
    unsigned long sect_addr,
    enum byte_sex literal_byte_sex,
    enum bool print_addresses);

extern void print_literal_pointer_section(
    mach_header_t *mh,
    struct load_command *lc,
    enum byte_sex object_byte_sex,
    char *addr,
    unsigned long size,
    char *sect,
    unsigned long sect_size,
    unsigned long sect_addr,
    nlist_t *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strings_size,
    struct relocation_info *relocs,
    unsigned long nrelocs,
    enum bool print_addresses);

extern void print_shlib_init(
    enum byte_sex object_byte_sex,
    char *sect,
    unsigned long sect_size,
    unsigned long sect_addr,
    struct symbol *sorted_symbols,
    unsigned long nsorted_symbols,
    nlist_t *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strings_size,
    struct relocation_info *relocs,
    unsigned long nrelocs,
    enum bool verbose);

extern enum bool print_symbol(
    unsigned long value,
    unsigned long pc,
    unsigned long dot_value,
    struct relocation_info *sorted_relocs,
    unsigned long nsorted_relocs,
    nlist_t *symbols,
    unsigned long nsymbols,
    struct symbol *sorted_symbols,
    unsigned long nsorted_symbols,
    char *strings,
    unsigned long strings_size,
    enum bool verbose);

extern const char *guess_symbol(
    const unsigned long value,
    const struct symbol *sorted_symbols,
    const unsigned long nsorted_symbols,
    const enum bool verbose);

extern const char * guess_indirect_symbol(
    const unsigned long value,
    const mach_header_t *mh,
    const struct load_command *load_commands,
    const enum byte_sex load_commands_byte_sex,
    const unsigned long *indirect_symbols,
    const unsigned long nindirect_symbols,
    const nlist_t *symbols,
    const unsigned long nsymbols,
    const char *strings,
    const unsigned long strings_size);

extern void print_sect(
    cpu_type_t cputype,
    enum byte_sex object_byte_sex,
    char *sect,
    unsigned long size,
    unsigned long addr);

extern void print_objc_segment(
    mach_header_t *mh,
    struct load_command *load_commands,
    enum byte_sex object_byte_sex,
    char *object_addr,
    unsigned long object_size,
    struct symbol *sorted_symbols,
    unsigned long nsorted_symbols,
    enum bool verbose);

extern void print_objc_protocol_section(
    mach_header_t *mh,
    struct load_command *load_commands,
    enum byte_sex object_byte_sex,
    char *object_addr,
    unsigned long object_size,
    enum bool verbose);

extern void print_objc_string_object_section(
    char *sectname,
    mach_header_t *mh,
    struct load_command *load_commands,
    enum byte_sex object_byte_sex,
    char *object_addr,
    unsigned long object_size,
    enum bool verbose);

extern void print_objc_runtime_setup_section(
    mach_header_t *mh,
    struct load_command *load_commands,
    enum byte_sex object_byte_sex,
    char *object_addr,
    unsigned long object_size,
    enum bool verbose);

extern void print_label(
    unsigned long addr,
    enum bool colon_and_newline,
    struct symbol *sorted_symbols,
    unsigned long nsorted_symbols);
