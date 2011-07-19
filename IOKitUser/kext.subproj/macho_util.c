/*
 * Copyright (c) 2006-2008 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
#include <unistd.h>
#include <string.h>

#include <stdio.h>
#include <fcntl.h>

#include <mach/vm_types.h>

#include <mach-o/swap.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "macho_util.h"

static boolean_t macho_swap_32(u_char *file);
static boolean_t macho_swap_64(u_char *file);

static boolean_t macho_unswap_32(u_char *file);
static boolean_t macho_unswap_64(u_char *file);

/*******************************************************************************
*
*******************************************************************************/
macho_seek_result macho_find_symbol(
    const void          * file_start,
    const void          * file_end,
    const char          * name,
          uint8_t       * nlist_type,
    const void         ** symbol_address)
{
    macho_seek_result       result = macho_seek_result_not_found;
    macho_seek_result       symtab_result = macho_seek_result_not_found;
    uint8_t                 swap = 0;
    char                    sixtyfourbit = 0;
    struct symtab_command * symtab = NULL;
    struct nlist          * syms_address;
    struct nlist_64       * syms_address_64;
    const void            * string_list;
    char                  * symbol_name;
    unsigned int            symtab_offset;
    unsigned int            str_offset;
    unsigned int            num_syms;
    unsigned int            syms_bytes;
    unsigned int            sym_index;

    if (symbol_address) {
         *symbol_address = 0;
    }

    symtab_result = macho_find_symtab(file_start, file_end, &symtab);
    if (symtab_result != macho_seek_result_found) {
        goto finish;
    }

    if (ISSWAPPEDMACHO(MAGIC32(file_start))) {
        swap = 1;
    }
    if (ISMACHO64(MAGIC32(file_start))) {
        sixtyfourbit = 1;
    }

    symtab_offset = CondSwapInt32(swap, symtab->symoff);
    str_offset = CondSwapInt32(swap, symtab->stroff);
    num_syms   = CondSwapInt32(swap, symtab->nsyms);

    syms_address = (struct nlist *)(file_start + symtab_offset);
    syms_address_64 = (struct nlist_64 *)(file_start + symtab_offset);

    string_list = file_start + str_offset;
    if (sixtyfourbit) {
        syms_bytes = num_syms * sizeof(struct nlist_64);
    } else {
        syms_bytes = num_syms * sizeof(struct nlist);
    }

    if ((char *)syms_address + syms_bytes > (char *)file_end) {
        result = macho_seek_result_error;
        goto finish;
    }

    for (sym_index = 0; sym_index < num_syms; sym_index++) {
        struct nlist    * seekptr;
        struct nlist_64 * seekptr_64;
        uint32_t          string_index;
        uint8_t           n_type;
        uint8_t           n_sect;
        uint64_t          n_value;

        if (sixtyfourbit) {
            seekptr_64   = &syms_address_64[sym_index];
            string_index = CondSwapInt32(swap, seekptr_64->n_un.n_strx);
            n_type       = seekptr_64->n_type;
            n_sect       = seekptr_64->n_sect;
            n_value      = CondSwapInt64(swap, seekptr_64->n_value);
        } else {
            seekptr      = &syms_address[sym_index];
            string_index = CondSwapInt32(swap, seekptr->n_un.n_strx);
            n_type       = seekptr->n_type;
            n_sect       = seekptr->n_sect;
            n_value      = (uint64_t)CondSwapInt32(swap, seekptr->n_value);
        }

        if (string_index == 0 || n_type & N_STAB) {
            continue;
        }
        symbol_name = (char *)(string_list + string_index);

        if (strcmp(name, symbol_name) == 0) {

            if (nlist_type) {
                *nlist_type = n_type;
            }
            switch (n_type & N_TYPE) {
                case N_SECT:
                    {
                        void * v_sect_info = macho_find_section_numbered(
                            file_start, file_end, n_sect);
    
    
                        if (!v_sect_info) {
                            break;  // out of the switch
                        }
                        
                        if (symbol_address) {
                            if (sixtyfourbit) {
                                struct section_64 * sect_info_64 =
                                    (struct section_64 *)v_sect_info;
        
                                // this isn't right for 64bit? compare below
                                size_t reloffset = (n_value -
                                    CondSwapInt64(swap, sect_info_64->addr));
        
                                *symbol_address = file_start;
                                *symbol_address += CondSwapInt32(swap,
                                    sect_info_64->offset);
                                *symbol_address += reloffset;
                            } else {
                                struct section * sect_info =
                                    (struct section *)v_sect_info;
        
                                size_t reloffset = (n_value -
                                    CondSwapInt32(swap, sect_info->addr));
        
                                *symbol_address = file_start;
                                *symbol_address += CondSwapInt32(swap,
                                    sect_info->offset);
                                *symbol_address += reloffset;
                            }
                        }
                        result = macho_seek_result_found;
                        goto finish;
                    }
                    break;
    
                case N_UNDF:
                    result = macho_seek_result_found_no_value;
                    goto finish;
                    break;
  
                case N_ABS:
                    result = macho_seek_result_found_no_value;
                    goto finish;
                    break;
  
              /* We don't chase indirect symbols as they can be external.
               */
                case N_INDR:
                    result = macho_seek_result_found_no_value;
                    goto finish;
                    break;
    
                default:
                    goto finish;
                    break;
            }
        }
    }

finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
typedef struct {
    struct symtab_command * symtab;
} _symtab_scan;

static macho_seek_result __macho_lc_is_symtab(
    struct load_command * lc_cmd,
    const void          * file_end,
    uint8_t               swap,
    void                * user_data);

/******************************************************************************/

macho_seek_result macho_find_symtab(
    const void             * file_start,
    const void             * file_end,
    struct symtab_command ** symtab)
{
    macho_seek_result result = macho_seek_result_not_found;
    _symtab_scan      sym_data;

    bzero(&sym_data, sizeof(sym_data));

    if (symtab) {
        *symtab = NULL;
    }

    result = macho_scan_load_commands(file_start,
        file_end, &__macho_lc_is_symtab, &sym_data);

    if (result == macho_seek_result_found) {
        if (symtab) {
            *symtab = sym_data.symtab;
        }
    }

    return result;
}

/******************************************************************************/

static macho_seek_result __macho_lc_is_symtab(
    struct load_command * lc_cmd,
    const void          * file_end,
    uint8_t               swap,
    void                * user_data)
{
    macho_seek_result   result = macho_seek_result_not_found;
    _symtab_scan      * sym_data = (_symtab_scan *)user_data;
    uint32_t            cmd;

    if ((void *)(lc_cmd + sizeof(struct load_command)) > file_end) {
        result = macho_seek_result_error;
        goto finish;
    }

    cmd = CondSwapInt32(swap, lc_cmd->cmd);

    if (cmd == LC_SYMTAB) {
        uint32_t cmd_size = CondSwapInt32(swap, lc_cmd->cmdsize);

        if ((cmd_size != sizeof(struct symtab_command)) ||
            ((void *)(lc_cmd + sizeof(struct symtab_command)) > file_end)) {
            result = macho_seek_result_error;
            goto finish;
        }
        sym_data->symtab = (struct symtab_command *)lc_cmd;
        result = macho_seek_result_found;
        goto finish;
    }

finish:
    return result;
}

/*******************************************************************************
* macho_find_uuid()
*
* Returns a pointer to the UUID bytes.
*******************************************************************************/
struct _uuid_scan {
    unsigned int   uuid_size;
    char         * uuid;
};

macho_seek_result __uuid_callback(
    struct load_command * load_command,
    const void * file_end,
    uint8_t swap __unused,
    void * user_data)
{
    struct _uuid_scan * uuid_stuff = (struct _uuid_scan *)user_data;
    if (load_command->cmd == LC_UUID) {
        struct uuid_command * uuid_command = (struct uuid_command *)load_command;
        if (((void *)load_command + load_command->cmdsize) > file_end) {
            return macho_seek_result_error;
        }
        uuid_stuff->uuid_size = sizeof(uuid_command->uuid);
        uuid_stuff->uuid = (char *)uuid_command->uuid;
        return macho_seek_result_found;
    }
    return macho_seek_result_not_found;
}

macho_seek_result macho_find_uuid(
    const void * file_start,
    const void * file_end,
    char       * uuid)
{
    macho_seek_result  result;
    struct _uuid_scan seek_uuid;

    result = macho_scan_load_commands(
        file_start, file_end,
        __uuid_callback, (const void **)&seek_uuid);
    if (result == macho_seek_result_found && uuid) {
        *uuid = seek_uuid.uuid;
    }

    return result;
}

/*******************************************************************************
* macho_find_section_numbered()
*
* Returns a pointer to a section in a mach-o file based on its global index
* (which starts at 1, not zero!). The section number is typically garnered from
* some other mach-o struct, such as a symtab entry. Returns NULL if the numbered
* section can't be found.
*******************************************************************************/
typedef struct {
    char       sixtyfourbit;
    uint8_t    sect_num;
    uint8_t    sect_counter;
    void     * sect_info;  // struct section or section_64 depending
} _sect_scan;

static macho_seek_result __macho_sect_in_lc(
    struct load_command * lc_cmd,
    const void          * file_end,
    uint8_t               swap,
    void                * user_data);

/******************************************************************************/

void * macho_find_section_numbered(
    const void * file_start,
    const void * file_end,
    uint8_t      sect_num)
{
    _sect_scan sect_data;

    bzero(&sect_data, sizeof(sect_data));

    sect_data.sect_num = sect_num;

    if (ISMACHO64(MAGIC32(file_start))) {
        sect_data.sixtyfourbit = 1;
    }

    if (macho_seek_result_found == macho_scan_load_commands(
        file_start, file_end, &__macho_sect_in_lc, &sect_data)) {

        return sect_data.sect_info;
    }

    return NULL;
}

/******************************************************************************/

static macho_seek_result __macho_sect_in_lc(
    struct load_command * lc_cmd,
    const void          * file_end,
    uint8_t               swap,
    void                * user_data)
{
    macho_seek_result   result    = macho_seek_result_not_found;
    _sect_scan        * sect_data = (_sect_scan *)user_data;
    uint32_t            cmd;

    if (sect_data->sect_counter > sect_data->sect_num) {
        result = macho_seek_result_stop;
        goto finish;
    }

    if ((void *)(lc_cmd + sizeof(struct load_command)) > file_end) {
        result = macho_seek_result_error;
        goto finish;
    }

    cmd = CondSwapInt32(swap, lc_cmd->cmd);

    if (cmd == LC_SEGMENT_64) {
        struct segment_command_64 * seg_cmd =
            (struct segment_command_64 *)lc_cmd;
        uint32_t                    cmd_size;
        uint32_t                    num_sects;
        uint32_t                    sects_size;
        struct section_64         * seek_sect;
        uint32_t                    sect_index;

        cmd_size = CondSwapInt32(swap, seg_cmd->cmdsize);
        num_sects = CondSwapInt32(swap, seg_cmd->nsects);
        sects_size = num_sects * sizeof(*seek_sect);

        if (cmd_size != (sizeof(*seg_cmd) + sects_size)) {
            result = macho_seek_result_error;
            goto finish;
        }

        if (((void *)lc_cmd + cmd_size) > file_end) {
            result = macho_seek_result_error;
            goto finish;
        }

        for (sect_index = 0; sect_index < num_sects; sect_index++) {

            seek_sect = (struct section_64 *)((void *)lc_cmd +
                sizeof(*seg_cmd) +
                (sect_index * sizeof(*seek_sect)));

            sect_data->sect_counter++;

            if (sect_data->sect_counter == sect_data->sect_num) {
                sect_data->sect_info = seek_sect;
                result = macho_seek_result_found;
                goto finish;
            }
        }
    } else if (cmd == LC_SEGMENT) {
        struct segment_command    * seg_cmd = (struct segment_command *)lc_cmd;
        uint32_t                    cmd_size;
        uint32_t                    num_sects;
        uint32_t                    sects_size;
        struct section            * seek_sect;
        uint32_t                    sect_index;

        cmd_size = CondSwapInt32(swap, seg_cmd->cmdsize);
        num_sects = CondSwapInt32(swap, seg_cmd->nsects);
        sects_size = num_sects * sizeof(*seek_sect);

        if (cmd_size != (sizeof(*seg_cmd) + sects_size)) {
            result = macho_seek_result_error;
            goto finish;
        }

        if (((void *)lc_cmd + cmd_size) > file_end) {
            result = macho_seek_result_error;
            goto finish;
        }

        for (sect_index = 0; sect_index < num_sects; sect_index++) {

            seek_sect = (struct section *)((void *)lc_cmd +
                sizeof(*seg_cmd) +
                (sect_index * sizeof(*seek_sect)));

            sect_data->sect_counter++;

            if (sect_data->sect_counter == sect_data->sect_num) {
                sect_data->sect_info = seek_sect;
                result = macho_seek_result_found;
                goto finish;
            }
        }
    }

finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
#define CMDSIZE_MULT_32  (4)
#define CMDSIZE_MULT_64  (8)

macho_seek_result macho_scan_load_commands(
    const void        * file_start,
    const void        * file_end,
    macho_lc_callback   lc_callback,
    void              * user_data)
{
    macho_seek_result       result = macho_seek_result_not_found;
    struct mach_header    * mach_header = (struct mach_header *)file_start;

    uint8_t                 swap = 0;
    uint32_t                cmdsize_mult = CMDSIZE_MULT_32;

    uint32_t                num_cmds;
    uint32_t                sizeofcmds;
    char                  * cmds_end;

    uint32_t                cmd_index;
    struct load_command  * load_commands;
    struct load_command  * seek_lc;

    switch (MAGIC32(file_start)) {
      case MH_MAGIC_64:
        cmdsize_mult = CMDSIZE_MULT_64;
        break;
      case MH_CIGAM_64:
        cmdsize_mult = CMDSIZE_MULT_64;
        swap = 1;
        break;
      case MH_CIGAM:
        swap = 1;
        break;
      case MH_MAGIC:
        break;
      default:
        result = macho_seek_result_error;
        goto finish;
        break;
    }

    if (cmdsize_mult == CMDSIZE_MULT_64) {
        load_commands = (struct load_command *)
            (file_start + sizeof(struct mach_header_64));
    } else {
        load_commands = (struct load_command *)
            (file_start + sizeof(struct mach_header));
    }

    if (file_start >= file_end || (((void *)load_commands) > file_end)) {
        result = macho_seek_result_error;
        goto finish;
    }

    num_cmds   = CondSwapInt32(swap, mach_header->ncmds);
    sizeofcmds = CondSwapInt32(swap, mach_header->sizeofcmds);
    cmds_end = (char *)load_commands + sizeofcmds;

    if (cmds_end > (char *)file_end) {
        result = macho_seek_result_error;
        goto finish;
    }

    seek_lc = load_commands;

    for (cmd_index = 0; cmd_index < num_cmds; cmd_index++) {
        uint32_t cmd_size;
        char * lc_end;

        cmd_size = CondSwapInt32(swap, seek_lc->cmdsize);
        lc_end = (char *)seek_lc + cmd_size;

        if ((cmd_size % cmdsize_mult != 0) || (lc_end > cmds_end)) {
            result = macho_seek_result_error;
            goto finish;
        }

        result = lc_callback(seek_lc, file_end, swap, user_data);

        switch (result) {
          case macho_seek_result_not_found:
            /* Not found, keep scanning. */
            break;

          case macho_seek_result_stop:
            /* Definitely found that it isn't there. */
            result = macho_seek_result_not_found;
            goto finish;
            break;

          case macho_seek_result_found:
            /* Found it! */
            goto finish;
            break;

          default:
            /* Error, fall through default case. */
            result = macho_seek_result_error;
            goto finish;
            break;
        }

        seek_lc = (struct load_command *)((char *)seek_lc + cmd_size);
    }

finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
boolean_t macho_swap(
    u_char    * file)
{
    boolean_t result = FALSE;
    struct mach_header *hdr = (struct mach_header *) file;

    if (hdr->magic == MH_CIGAM) {
        result = macho_swap_32(file);
    } else if (hdr->magic == MH_CIGAM_64) {
        result = macho_swap_64(file);
    }

    return result;
}

/*******************************************************************************
*******************************************************************************/
static boolean_t macho_swap_32(
    u_char    * file)
{
    boolean_t result = FALSE;
    struct mach_header *hdr = (struct mach_header *) file;
    struct load_command *lc = (struct load_command *) &hdr[1];
    struct segment_command *seg = NULL;
    u_long offset = 0;
    u_int cmd = 0;
    u_int cmdsize = 0;
    u_int i = 0;

    if (!hdr || hdr->magic != MH_CIGAM) goto finish;

    swap_mach_header(hdr, NXHostByteOrder());

    offset = sizeof(*hdr);
    for (i = 0; i < hdr->ncmds; ++i) {
        lc = (struct load_command *) (file + offset);

        cmd = OSSwapInt32(lc->cmd);
        cmdsize = OSSwapInt32(lc->cmdsize);
        offset += cmdsize;

        if (cmd == LC_SEGMENT) {
            seg = (struct segment_command *) lc;
            swap_segment_command(seg, NXHostByteOrder());
        } else {
            swap_load_command(lc, NXHostByteOrder());
        }
    }

    result = TRUE;
finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
boolean_t macho_swap_64(
    u_char    * file)
{
    boolean_t result = FALSE;
    struct mach_header_64 *hdr = (struct mach_header_64 *) file;
    struct load_command *lc = (struct load_command *) &hdr[1];
    struct segment_command_64 *seg = NULL;
    u_long offset = 0;
    u_int cmd = 0;
    u_int cmdsize = 0;
    u_int i = 0;

    if (!hdr || hdr->magic != MH_CIGAM_64) goto finish;

    swap_mach_header_64(hdr, NXHostByteOrder());

    offset = sizeof(*hdr);
    for (i = 0; i < hdr->ncmds; ++i) {
        lc = (struct load_command *) (file + offset);

        cmd = OSSwapInt32(lc->cmd);
        cmdsize = OSSwapInt32(lc->cmdsize);
        offset += cmdsize;

        if (cmd == LC_SEGMENT_64) {
            seg = (struct segment_command_64 *) lc;
            swap_segment_command_64(seg, NXHostByteOrder());
        } else {
            swap_load_command(lc, NXHostByteOrder());
        }
    }

    result = TRUE;
finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
boolean_t macho_unswap(
    u_char    * file)
{
    boolean_t result = FALSE;
    struct mach_header *hdr = (struct mach_header *) file;

    if (hdr->magic == MH_MAGIC) {
        result = macho_unswap_32(file);
    } else if (hdr->magic == MH_MAGIC_64) {
        result = macho_unswap_64(file);
    }

    return result;
}

/*******************************************************************************
*******************************************************************************/
boolean_t macho_unswap_32(
    u_char    * file)
{
    boolean_t result = FALSE;
    enum NXByteOrder order = 0;
    struct mach_header *hdr = (struct mach_header *) file;
    struct load_command *lc = (struct load_command *) &hdr[1];
    struct segment_command *seg = NULL;
    u_long offset = 0;
    u_int i = 0;

    if (NXHostByteOrder() == NX_LittleEndian) {
        order = NX_BigEndian;
    } else {
        order = NX_LittleEndian;
    }

    if (!hdr || hdr->magic != MH_MAGIC) goto finish;

    offset = sizeof(*hdr);
    for (i = 0; i < hdr->ncmds; ++i) {
        lc = (struct load_command *) (file + offset);
        offset += lc->cmdsize;

        if (lc->cmd == LC_SEGMENT) {
            seg = (struct segment_command *) lc;
            swap_segment_command(seg, order);
        } else {
            swap_load_command(lc, order);
        }
            
    }

    swap_mach_header(hdr, order);

    result = TRUE;
finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
boolean_t macho_unswap_64(
    u_char    * file)
{
    boolean_t result = FALSE;
    enum NXByteOrder order = 0;
    struct mach_header_64 *hdr = (struct mach_header_64 *) file;
    struct load_command *lc = (struct load_command *) &hdr[1];
    struct segment_command_64 *seg = NULL;
    u_long offset = 0;
    u_int i = 0;

    if (NXHostByteOrder() == NX_LittleEndian) {
        order = NX_BigEndian;
    } else {
        order = NX_LittleEndian;
    }

    if (!hdr || hdr->magic != MH_MAGIC_64) goto finish;

    offset = sizeof(*hdr);
    for (i = 0; i < hdr->ncmds; ++i) {
        lc = (struct load_command *) (file + offset);
        offset += lc->cmdsize;

        if (lc->cmd == LC_SEGMENT_64) {
            seg = (struct segment_command_64 *) lc;
            swap_segment_command_64(seg, order);
        } else {
            swap_load_command(lc, order);
        }
    }

    swap_mach_header_64(hdr, order);

    result = TRUE;
finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/
struct segment_command * macho_get_segment_by_name(
    struct mach_header    * mach_header,
    const char            * segname)
{
    struct segment_command *segment = NULL;
    struct load_command *lc = NULL;
    u_char *base = (u_char *) mach_header;
    size_t offset = sizeof(*mach_header);
    u_int i = 0;
    
    if (mach_header->magic != MH_MAGIC) goto finish;
    
    for (i = 0; i < mach_header->ncmds; ++i) {
        lc = (struct load_command *) (base + offset);
        
        if (lc->cmd == LC_SEGMENT) {
            segment = (struct segment_command *) lc;
            if (!strncmp(segment->segname, segname, sizeof(segment->segname))) {
                break;
            }
            segment = NULL;
        }
        
        offset += lc->cmdsize;
    }

finish:    
    return segment;
}

/*******************************************************************************
*******************************************************************************/
struct segment_command_64 * macho_get_segment_by_name_64(
    struct mach_header_64      * mach_header,
    const char                 * segname)
{
    struct segment_command_64 *segment = NULL;
    struct load_command *lc = NULL;
    u_char *base = (u_char *) mach_header;
    size_t offset = sizeof(*mach_header);
    u_int i = 0;
    
    if (mach_header->magic != MH_MAGIC_64) goto finish;
    
    for (i = 0; i < mach_header->ncmds; ++i) {
        lc = (struct load_command *) (base + offset);
        
        if (lc->cmd == LC_SEGMENT_64) {
            segment = (struct segment_command_64 *) lc;
            if (!strncmp(segment->segname, segname, sizeof(segment->segname))) {
                break;
            }
            segment = NULL;
        }
        
        offset += lc->cmdsize;
    }
    
finish:    
    return segment;
}

/*******************************************************************************
*******************************************************************************/
struct section * macho_get_section_by_name(
    struct mach_header    * mach_header,
    const char            * segname,
    const char            * sectname)
{
    struct segment_command *segment = NULL;
    struct section *section = NULL;
    u_int i = 0;
    
    if (mach_header->magic != MH_MAGIC) goto finish;

    segment = macho_get_segment_by_name(mach_header, segname);
    if (!segment) goto finish;
    
    section = (struct section *) (&segment[1]);
    for (i = 0; i < segment->nsects; ++i, ++section) {
        if (!strncmp(section->sectname, sectname, sizeof(section->sectname))) {
            break;
        }
    }
    
    if (i == segment->nsects) {
        section = NULL;
    }
    
finish:
    return section;
}

/*******************************************************************************
*******************************************************************************/
struct section_64 * macho_get_section_by_name_64(
    struct mach_header_64     * mach_header,
    const char                * segname,
    const char                * sectname)
{
    struct segment_command_64 *segment = NULL;
    struct section_64 *section = NULL;
    u_int i = 0;
    
    if (mach_header->magic != MH_MAGIC_64) goto finish;
    
    segment = macho_get_segment_by_name_64(mach_header, segname);
    if (!segment) goto finish;
    
    section = (struct section_64 *) (&segment[1]);
    for (i = 0; i < segment->nsects; ++i, ++section) {
        if (!strncmp(section->sectname, sectname, sizeof(section->sectname))) {
            break;
        }
    }
    
    if (i == segment->nsects) {
        section = NULL;
    }
    
finish:
    return section;
}

/*******************************************************************************
*******************************************************************************/
boolean_t macho_remove_linkedit(u_char *macho, u_long *linkedit_size)
{
    boolean_t result = FALSE;
    struct mach_header *mach_hdr;
    struct mach_header_64 *mach_hdr64;
    u_char *src, *dst;
    uint32_t ncmds, cmdsize;
    boolean_t swap = FALSE;
    u_int i;

    swap = macho_swap(macho);

    mach_hdr = (struct mach_header *) macho;
    mach_hdr64 = (struct mach_header_64 *) macho;

    /* Find the start of the load commands */

    if (mach_hdr->magic == MH_MAGIC) {
        src = dst = macho + sizeof(*mach_hdr);
        ncmds = mach_hdr->ncmds;
    } else if (mach_hdr->magic == MH_MAGIC_64) {
        src = dst = macho + sizeof(*mach_hdr64);
        ncmds = mach_hdr64->ncmds;
    } else {
        goto finish;
    }

    /* Remove any LINKEDIT-related load commands */

    for (i = 0; i < ncmds; ++i, src += cmdsize) {
        struct load_command * lc = (struct load_command *) src;
        struct segment_command *seg = (struct segment_command *) src;
        struct segment_command_64 *seg64 = (struct segment_command_64 *) src;
        boolean_t strip = FALSE;

        cmdsize = lc->cmdsize;

        /* We delete the LINKEDIT segment and any symtab load commands */

        switch (lc->cmd) {
        case LC_SEGMENT:
            if (!strncmp(seg->segname, SEG_LINKEDIT, sizeof(SEG_LINKEDIT) - 1)) {
                strip = TRUE;
                *linkedit_size = seg->vmsize;
            }
            break;
        case LC_SEGMENT_64:
            if (!strncmp(seg64->segname, SEG_LINKEDIT, sizeof(SEG_LINKEDIT) - 1)) {
                strip = TRUE;
                *linkedit_size = seg64->vmsize;
            }
            break;
        case LC_SYMTAB:
        case LC_DYSYMTAB:
            strip = TRUE;
            break;
        }

        if (strip) {
            if (mach_hdr->magic == MH_MAGIC) {
                mach_hdr->ncmds--;
                mach_hdr->sizeofcmds -= cmdsize;
            } else {
                mach_hdr64->ncmds--;
                mach_hdr64->sizeofcmds -= cmdsize;
            }
            bzero(src, lc->cmdsize);
        } else {
            memmove(dst, src, cmdsize);
            dst += cmdsize;
        }
    }

    result = TRUE;
finish:
    if (swap) macho_unswap(macho);
    return result;
}

