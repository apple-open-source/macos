/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "language.h"
#include "block.h"

#include "libaout.h"            /* FIXME Secret internal BFD stuff for a.out */
#include "aout/aout64.h"
#include "complaints.h"

#include "mach-o.h"
#include "objc-lang.h"

#include "macosx-tdep.h"

#if 0
struct deprecated_complaint unknown_macho_symtype_complaint =
  { "unknown Mach-O symbol type %s", 0, 0 };

struct deprecated_complaint unknown_macho_section_complaint =
  { "unknown Mach-O section value %s (assuming DATA)", 0, 0 };

struct deprecated_complaint unsupported_indirect_symtype_complaint =
  { "unsupported Mach-O symbol type %s (indirect)", 0, 0 };
#endif

#define BFD_GETB16(addr) ((addr[0] << 8) | addr[1])
#define BFD_GETB32(addr) ((((((uint32_t) addr[0] << 8) | addr[1]) << 8) | addr[2]) << 8 | addr[3])
#define BFD_GETB64(addr) ((((((((((uint64_t) addr[0] << 8) | addr[1]) << 8) | addr[2]) << 8 | addr[3]) << 8 | addr[4]) << 8 | addr[5]) << 8 | addr[6]) << 8 | addr[7])
#define BFD_GETL16(addr) ((addr[1] << 8) | addr[0])
#define BFD_GETL32(addr) ((((((uint32_t) addr[3] << 8) | addr[2]) << 8) | addr[1]) << 8 | addr[0])
#define BFD_GETL64(addr) ((((((((((uint64_t) addr[7] << 8) | addr[6]) << 8) | addr[5]) << 8 | addr[4]) << 8 | addr[3]) << 8 | addr[2]) << 8 | addr[1]) << 8 | addr[0])

unsigned char macosx_symbol_types[256];

static unsigned char
macosx_symbol_type_base (macho_type)
     unsigned char macho_type;
{
  unsigned char mtype = macho_type;
  unsigned char ntype = 0;

  if (macho_type & BFD_MACH_O_N_STAB)
    {
      return macho_type;
    }

  if (mtype & BFD_MACH_O_N_PEXT)
    {
      mtype &= ~BFD_MACH_O_N_PEXT;
      ntype |= N_EXT;
    }

  if (mtype & BFD_MACH_O_N_EXT)
    {
      mtype &= ~BFD_MACH_O_N_EXT;
      ntype |= N_EXT;
    }

  switch (mtype & BFD_MACH_O_N_TYPE)
    {
    case BFD_MACH_O_N_SECT:
      /* should add section here */
      break;

    case BFD_MACH_O_N_PBUD:
      ntype |= N_UNDF;
      break;

    case BFD_MACH_O_N_ABS:
      ntype |= N_ABS;
      break;

    case BFD_MACH_O_N_UNDF:
      ntype |= N_UNDF;
      break;

    case BFD_MACH_O_N_INDR:
      /* complain (&unsupported_indirect_symtype_complaint, local_hex_string (macho_type)); */
      return macho_type;

    default:
      /* complain (&unknown_macho_symtype_complaint, local_hex_string (macho_type)); */
      return macho_type;
    }
  mtype &= ~BFD_MACH_O_N_TYPE;

  CHECK_FATAL (mtype == 0);

  return ntype;
}

static void
macosx_symbol_types_init ()
{
  unsigned int i;
  for (i = 0; i < 256; i++)
    {
      macosx_symbol_types[i] = macosx_symbol_type_base (i);
    }
}

static unsigned char
macosx_symbol_type (macho_type, macho_sect, abfd)
     unsigned char macho_type;
     unsigned char macho_sect;
     bfd *abfd;
{
  unsigned char ntype = macosx_symbol_types[macho_type];

  /* If the symbol refers to a section, modify ntype based on the value of macho_sect. */

  if ((macho_type & BFD_MACH_O_N_TYPE) == BFD_MACH_O_N_SECT)
    {
      if (macho_sect == 1)
        {
          /* Section 1 is always the text segment. */
          ntype |= N_TEXT;
        }

      else if ((macho_sect > 0)
               && (macho_sect <= abfd->tdata.mach_o_data->nsects))
        {
          const bfd_mach_o_section *sect =
            abfd->tdata.mach_o_data->sections[macho_sect - 1];

          if (sect == NULL)
            {
              /* complain (&unknown_macho_section_complaint, local_hex_string (macho_sect)); */
            }
          else if ((sect->segname != NULL)
                   && (strcmp (sect->segname, "__DATA") == 0))
            {
              if ((sect->sectname != NULL)
                  && (strcmp (sect->sectname, "__bss") == 0))
                ntype |= N_BSS;
              else
                ntype |= N_DATA;
            }
          else if ((sect->segname != NULL)
                   && (strcmp (sect->segname, "__TEXT") == 0))
            {
              ntype |= N_TEXT;
            }
          else
            {
              /* complain (&unknown_macho_section_complaint, local_hex_string (macho_sect)); */
              ntype |= N_DATA;
            }
        }

      else
        {
          /* complain (&unknown_macho_section_complaint, local_hex_string (macho_sect)); */
          ntype |= N_DATA;
        }
    }

  /* All modifications are done; return the computed type code. */

  return ntype;
}

void
macosx_internalize_symbol (in, sect_p, ext, abfd)
     struct internal_nlist *in;
     int *sect_p;
     struct external_nlist *ext;
     bfd *abfd;
{
  int symwide = (bfd_mach_o_version (abfd) > 1);

  if (bfd_header_big_endian (abfd))
    {
      in->n_strx = BFD_GETB32 (ext->e_strx);
      in->n_desc = BFD_GETB16 (ext->e_desc);
      if (symwide)
        in->n_value = BFD_GETB64 (ext->e_value);
      else
        in->n_value = BFD_GETB32 (ext->e_value);
    }
  else if (bfd_header_little_endian (abfd))
    {
      in->n_strx = BFD_GETL32 (ext->e_strx);
      in->n_desc = BFD_GETL16 (ext->e_desc);
      if (symwide)
        in->n_value = BFD_GETL64 (ext->e_value);
      else
        in->n_value = BFD_GETL32 (ext->e_value);
    }
  else
    {
      error ("unable to internalize symbol (unknown endianness)");
    }

  if ((ext->e_type[0] & BFD_MACH_O_N_TYPE) == BFD_MACH_O_N_SECT)
    *sect_p = 1;
  else
    *sect_p = 0;

  in->n_type = macosx_symbol_type (ext->e_type[0], ext->e_other[0], abfd);
  in->n_other = ext->e_other[0];
}

CORE_ADDR
dyld_symbol_stub_function_address (CORE_ADDR pc, const char **name)
{
  struct symbol *sym = NULL;
  struct minimal_symbol *msym = NULL;
  const char *lname = NULL;

  lname = dyld_symbol_stub_function_name (pc);
  if (name)
    *name = lname;

  if (lname == NULL)
    return 0;

  /* found a name, now find a symbol and address */

  sym = lookup_symbol (lname, 0, VAR_DOMAIN, 0, 0);
  if ((sym == NULL) && (lname[0] == '_'))
    sym = lookup_symbol (lname + 1, 0, VAR_DOMAIN, 0, 0);
  if (sym != NULL && SYMBOL_BLOCK_VALUE (sym) != NULL)
    return BLOCK_START (SYMBOL_BLOCK_VALUE (sym));

  msym = lookup_minimal_symbol (lname, NULL, NULL);
  if ((msym == 0) && (lname[0] == '_'))
    msym = lookup_minimal_symbol (lname + 1, NULL, NULL);
  if (msym != NULL)
    return SYMBOL_VALUE_ADDRESS (msym);

  return 0;
}

const char *
dyld_symbol_stub_function_name (CORE_ADDR pc)
{
  struct minimal_symbol *msymbol = NULL;
  const char *DYLD_PREFIX = "dyld_stub_";

  msymbol = lookup_minimal_symbol_by_pc (pc);

  if (msymbol == NULL)
    return NULL;

  if (SYMBOL_VALUE_ADDRESS (msymbol) != pc)
    return NULL;

  if (strncmp
      (SYMBOL_LINKAGE_NAME (msymbol), DYLD_PREFIX, strlen (DYLD_PREFIX)) != 0)
    return NULL;

  return SYMBOL_LINKAGE_NAME (msymbol) + strlen (DYLD_PREFIX);
}

CORE_ADDR
macosx_skip_trampoline_code (CORE_ADDR pc)
{
  CORE_ADDR newpc;

  newpc = dyld_symbol_stub_function_address (pc, NULL);
  if (newpc != 0)
    return newpc;

  newpc = decode_fix_and_continue_trampoline (pc);
  if (newpc != 0)
    return newpc;

  return 0;
}

/* This function determings whether a symbol is in a SYMBOL_STUB section.
   ld64 puts symbols there for all the stubs, but if we read those in, they
   will confuse us when we lookup the symbol for the pc to see if we are
   in a stub.  */

int
macosx_record_symbols_from_sect_p (bfd *abfd, unsigned char macho_type, 
				   unsigned char macho_sect)
{
  const bfd_mach_o_section *sect =
    abfd->tdata.mach_o_data->sections[macho_sect - 1];
  if ((sect->flags & BFD_MACH_O_SECTION_TYPE_MASK) ==
      BFD_MACH_O_S_SYMBOL_STUBS)
    return 0;
  else
    return 1;
}

int
macosx_in_solib_return_trampoline (CORE_ADDR pc, char *name)
{
  return 0;
}

int
macosx_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  if (macosx_skip_trampoline_code (pc) != 0)
    {
      return 1;
    }
  return 0;
}

static void
info_trampoline_command (char *exp, int from_tty)
{
  struct expression *expr;
  struct value *val;
  CORE_ADDR address;
  CORE_ADDR trampoline;
  CORE_ADDR objc;

  expr = parse_expression (exp);
  val = evaluate_expression (expr);
  if (TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_REF)
    val = value_ind (val);
  if ((TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_FUNC)
      && (VALUE_LVAL (val) == lval_memory))
    address = VALUE_ADDRESS (val);
  else
    address = value_as_address (val);

  trampoline = macosx_skip_trampoline_code (address);

  find_objc_msgcall (trampoline, &objc);

  fprintf_filtered
    (gdb_stderr, "Function at 0x%s becomes 0x%s becomes 0x%s\n",
     paddr_nz (address), paddr_nz (trampoline), paddr_nz (objc));
}

void
_initialize_macosx_tdep ()
{
  macosx_symbol_types_init ();

  add_info ("trampoline", info_trampoline_command,
            "Resolve function for DYLD trampoline stub and/or Objective-C call");
}
