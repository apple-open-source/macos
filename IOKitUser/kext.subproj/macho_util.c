#include <unistd.h>
#include <string.h>

#include <stdio.h>
#include <fcntl.h>

#include <mach/vm_types.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "macho_util.h"


/*******************************************************************************
*
*******************************************************************************/
macho_seek_result macho_find_symbol(
    const void * file_start,
    const void * file_end,
    const char * name,
    const struct nlist ** symbol_entry,
    const void ** symbol_address)
{
    macho_seek_result result = macho_seek_result_not_found;
    macho_seek_result symtab_result = macho_seek_result_not_found;
    uint8_t swap = 0;
    struct symtab_command * symtab = NULL;
    struct nlist * syms_address;
    const void * string_list;
    char * symbol_name;
    unsigned int sym_offset;
    unsigned int str_offset;
    unsigned int num_syms;
    unsigned int syms_bytes;
    unsigned int sym_index;

    if (symbol_address) {
         *symbol_address = 0;
    }

    symtab_result = macho_find_symtab(file_start, file_end, &symtab);
    if (symtab_result != macho_seek_result_found) {
        goto finish;
    }

    if (MAGIC32(file_start) == MH_CIGAM) {
        swap = 1;
    }

    sym_offset = CondSwapInt32(swap, symtab->symoff);
    str_offset = CondSwapInt32(swap, symtab->stroff);
    num_syms   = CondSwapInt32(swap, symtab->nsyms);

    syms_address = (struct nlist *)(file_start + sym_offset);
    string_list = file_start + str_offset;
    syms_bytes = num_syms * sizeof(struct nlist);

    if ((char *)syms_address + syms_bytes > (char *)file_end) {
        result = macho_seek_result_error;
        goto finish;
    }

    for (sym_index = 0; sym_index < num_syms; sym_index++) {
        struct nlist * seekptr;
        uint32_t string_index;

        seekptr = &syms_address[sym_index];

        string_index = CondSwapInt32(swap, seekptr->n_un.n_strx);

        // no need to swap n_type (one-byte value)
        if (string_index == 0 || seekptr->n_type & N_STAB) {
            continue;
        }
        symbol_name = (char *)(string_list + string_index);

        if (strcmp(name, symbol_name) == 0) {

            uint32_t symbol_offset = CondSwapInt32(swap, seekptr->n_value);
            uint8_t sect_num = seekptr->n_sect;

            if (symbol_entry) {
                *symbol_entry = seekptr;
            }
            switch (seekptr->n_type & N_TYPE) {
              case N_SECT:
                {
                    struct section * sect_info = macho_find_section_numbered(
                        file_start, file_end, sect_num);

                    if (sect_info) {
                        if (symbol_address) {
                            size_t reloffset = (symbol_offset -
                                CondSwapInt32(swap, sect_info->addr));

                            *symbol_address = file_start;
                            *symbol_address += CondSwapInt32(swap,
                                sect_info->offset);
                            *symbol_address += reloffset;
                        }
                        result = macho_seek_result_found;
                        goto finish;
                    }
                }
                break;

              case N_UNDF:
                result = macho_seek_result_found_no_value;
                goto finish;
                break;

              case N_ABS:
                result = macho_seek_result_found_no_value;
#if 0
// I don't know how to calculate the offset for this and there
// may not be a value anyhow.
                if (symbol_address) {
                    *symbol_address = file_start;
                    *symbol_address += symbol_offset;
                }
#endif
                goto finish;
                break;

#if 0
// don't know how to do this one yet
              case N_INDR:
                symbol_name = (char *)(string_list + string_index);
                result = macho_find_symbol(file_start, file_end,
                    symbol_name, symbol_address);
                goto finish;
                break;
#endif

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
    const void * file_end,
    uint8_t swap,
    void * user_data);

/******************************************************************************/

macho_seek_result macho_find_symtab(
    const void * file_start,
    const void * file_end,
    struct symtab_command ** symtab)
{
    macho_seek_result result = macho_seek_result_not_found;
    _symtab_scan sym_data;

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
    const void * file_end,
    uint8_t swap,
    void * user_data)
{
    macho_seek_result result = macho_seek_result_not_found;
    _symtab_scan * sym_data = (_symtab_scan *)user_data;
    uint32_t cmd;

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
* macho_find_section_numbered()
*
* Returns a pointer to a section in a mach-o file based on its global index
* (which starts at 1, not zero!). The section number is typically garnered from
* some other mach-o struct, such as a symtab entry. Returns NULL if the numbered
* section can't be found.
*******************************************************************************/
typedef struct {
    uint8_t sect_num;
    uint8_t sect_counter;
    struct section * sect_info;
} _sect_scan;

static macho_seek_result __macho_sect_in_lc(
    struct load_command * lc_cmd,
    const void * file_end,
    uint8_t swap,
    void * user_data);

/******************************************************************************/

struct section * macho_find_section_numbered(
    const void * file_start,
    const void * file_end,
    uint8_t sect_num)
{
    _sect_scan sect_data;

    bzero(&sect_data, sizeof(sect_data));
    sect_data.sect_num = sect_num;

    if (macho_seek_result_found == macho_scan_load_commands(
        file_start, file_end, &__macho_sect_in_lc, &sect_data)) {

        return sect_data.sect_info;
    }

    return NULL;
}

/******************************************************************************/

static macho_seek_result __macho_sect_in_lc(
    struct load_command * lc_cmd,
    const void * file_end,
    uint8_t swap,
    void * user_data)
{
    macho_seek_result result = macho_seek_result_not_found;
    _sect_scan * sect_data = (_sect_scan *)user_data;
    uint32_t cmd;

    if (sect_data->sect_counter > sect_data->sect_num) {
        result = macho_seek_result_stop;
        goto finish;
    }

    if ((void *)(lc_cmd + sizeof(struct load_command)) > file_end) {
        result = macho_seek_result_error;
        goto finish;
    }

    cmd = CondSwapInt32(swap, lc_cmd->cmd);

    if (cmd == LC_SEGMENT) {
        struct segment_command * seg_cmd = (struct segment_command *)lc_cmd;
        uint32_t cmd_size;
        uint32_t num_sects;
        uint32_t sects_size;
        struct section * seek_sect;
        uint32_t sect_index;

        cmd_size = CondSwapInt32(swap, seg_cmd->cmdsize);
        num_sects = CondSwapInt32(swap, seg_cmd->nsects);
        sects_size = num_sects * sizeof(struct section);

        if (cmd_size != (sizeof(struct segment_command) + sects_size)) {
            result = macho_seek_result_error;
            goto finish;
        }

        if (((void *)lc_cmd + cmd_size) > file_end) {
            result = macho_seek_result_error;
            goto finish;
        }

        for (sect_index = 0; sect_index < num_sects; sect_index++) {

            seek_sect = (struct section *)((void *)lc_cmd +
                sizeof(struct segment_command) +
                (sect_index * sizeof(struct section)));

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
    const void * file_start,
    const void * file_end,
    macho_lc_callback lc_callback,
    void * user_data)
{
    macho_seek_result result = macho_seek_result_not_found;

    struct mach_header * mach_header = (struct mach_header *)file_start;

    uint8_t swap = 0;
    uint32_t cmdsize_mult = CMDSIZE_MULT_32;

    uint32_t num_cmds;
    uint32_t sizeofcmds;
    char * cmds_end;

    uint32_t cmd_index;
    struct load_command * load_commands;
    struct load_command * seek_lc;

    if ((void *)file_start > file_end ||
        ((void *)(file_start + sizeof(struct mach_header)) > file_end)) {
        result = macho_seek_result_error;
        goto finish;
    }

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

    load_commands = (struct load_command *)
        (file_start + sizeof(struct mach_header));

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
