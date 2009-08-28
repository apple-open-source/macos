/* Mac OS X support for GDB, the GNU debugger.
   Copyright 2005
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


/* When we're doing native debugging, and we attach to a process,
   we start out by finding the in-memory dyld -- the osabi of that
   dyld is stashed away here for use when picking the right osabi of
   a fat file.  In the case of cross-debugging, none of this happens
   and this global remains untouched.  */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "arch-utils.h"
#include "floatformat.h"
#include "gdbtypes.h"
#include "regcache.h"
#include "reggroups.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "dummy-frame.h"

#include "libbfd.h"

#include "arm-tdep.h"
#include "elf-bfd.h"
#include "dis-asm.h"
#include "gdbarch.h"
#include "osabi.h"

#include <mach-o/nlist.h>
#include <mach/mach.h>
#include "mach-o.h" /* for BFD mach definitions.  */


///rm enum gdb_osabi osabi_seen_in_attached_dyld = GDB_OSABI_UNKNOWN;
static enum gdb_osabi arm_mach_o_osabi_sniffer_use_dyld_hint (bfd *abfd);
static void arm_macosx_init_abi (struct gdbarch_info info,
                                 struct gdbarch *gdbarch);

static void arm_macosx_init_abi_v6 (struct gdbarch_info info,
                                    struct gdbarch *gdbarch);


int
arm_mach_o_query_v6 ()
{
  host_basic_info_data_t info;
  mach_msg_type_number_t count;

  count = HOST_BASIC_INFO_COUNT;
  host_info (mach_host_self (), HOST_BASIC_INFO, (host_info_t) & info,
             &count);

  return (info.cpu_type == BFD_MACH_O_CPU_TYPE_ARM &&
          info.cpu_subtype == BFD_MACH_O_CPU_SUBTYPE_ARM_6);
}

/* Two functions in one!  If this is a "bfd_archive" (read: a MachO fat file),
   recurse for each separate fork of the fat file.
   If this is not a fat file, detect whether the file is arm32 or arm64.
   Before either of these, check if we've already sniffed an appropriate
   OSABI from dyld (in the case of attaching to a process) and prefer that.  */

static enum gdb_osabi
arm_mach_o_osabi_sniffer (bfd *abfd)
{
  enum gdb_osabi ret;

  /* The way loading works on Darwin is that for FAT files, the
     fork corresponding to the host system will be the one used
     regardless of what the type of the main executable was.
     So the osabi has to be determined solely by the type of
     the host system.  */


  if (strcmp (bfd_get_target (abfd), "mach-o-le") == 0)
    {
      bfd_arch_info_type *arch_info = bfd_get_arch_info (abfd);
      if (arch_info->arch == bfd_arch_arm)
	{
	  if (arch_info->mach == bfd_mach_arm_4T)
	    return GDB_OSABI_DARWIN;
	  else if (arch_info->mach == bfd_mach_arm_6)
	    return GDB_OSABI_DARWINV6;
	}
    }

  /* However, if we are running a cross gdb, we won't know what
     the target architecture is.  FIXME - going to have to determine
     this somehow.  For now, fall back to looking at the binary.  */

  ret = arm_mach_o_osabi_sniffer_use_dyld_hint (abfd);
  if (ret == GDB_OSABI_DARWINV6 || ret == GDB_OSABI_DARWIN)
    return ret;

  if (bfd_check_format (abfd, bfd_archive))
    {
      enum gdb_osabi best = GDB_OSABI_UNKNOWN;
      enum gdb_osabi cur = GDB_OSABI_UNKNOWN;

      bfd *nbfd = NULL;
      for (;;)
        {
          nbfd = bfd_openr_next_archived_file (abfd, nbfd);

          if (nbfd == NULL)
            break;
          if (!bfd_check_format (nbfd, bfd_object))
            continue;

          cur = arm_mach_o_osabi_sniffer (nbfd);
          if (cur == GDB_OSABI_DARWINV6 &&
              best != GDB_OSABI_DARWINV6 && arm_mach_o_query_v6 ())
            best = cur;

          if (cur == GDB_OSABI_DARWIN &&
              best != GDB_OSABI_DARWINV6 && best != GDB_OSABI_DARWIN)
            best = cur;
        }
      return best;
    }

  if (!bfd_check_format (abfd, bfd_object))
    return GDB_OSABI_UNKNOWN;

  return GDB_OSABI_UNKNOWN;
}

/* If we're attaching to a process, we start by finding the dyld that
   is loaded and go from there.  So when we're selecting the OSABI,
   prefer the osabi of the actually-loaded dyld when we can.  */

static enum gdb_osabi
arm_mach_o_osabi_sniffer_use_dyld_hint (bfd *abfd)
{
  if (osabi_seen_in_attached_dyld == GDB_OSABI_UNKNOWN)
    return GDB_OSABI_UNKNOWN;

  bfd *nbfd = NULL;
  for (;;)
    {
      bfd_arch_info_type *arch_info;
      nbfd = bfd_openr_next_archived_file (abfd, nbfd);

      if (nbfd == NULL)
        break;
      if (!bfd_check_format (nbfd, bfd_object))
        continue;
      arch_info = bfd_get_arch_info (nbfd);

      if (arch_info->arch == bfd_arch_arm)
	{
	  if (arch_info->mach == bfd_mach_arm_4T
	      && osabi_seen_in_attached_dyld == GDB_OSABI_DARWIN)
	    return GDB_OSABI_DARWIN;
	  
	  if (arch_info->mach == bfd_mach_arm_6
	      && osabi_seen_in_attached_dyld == GDB_OSABI_DARWINV6)
	    return GDB_OSABI_DARWINV6;
	}
    }

  return GDB_OSABI_UNKNOWN;
}

/* Get the ith function argument for the current function.  */
CORE_ADDR
arm_fetch_pointer_argument (struct frame_info *frame, int argi,
                            struct type *type)
{
  CORE_ADDR addr;

  addr = get_frame_register_unsigned (frame, argi);

  return addr;
}

static void
arm_pseudo_register_read (struct gdbarch *gdbarch, struct regcache *regcache,
                          int reg, gdb_byte *buf)
{
  int s_reg_lsw = 2 * (reg - ARM_FIRST_VFP_PSEUDO_REGNUM)
    + ARM_FIRST_VFP_REGNUM;
  int s_reg_msw = s_reg_lsw + 1;
  regcache_cooked_read (regcache, s_reg_lsw, buf);
  regcache_cooked_read (regcache, s_reg_msw, buf + 4);
}

static void
arm_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache,
                          int reg, const gdb_byte *buf)
{
  int s_reg_lsw = 2 * (reg - ARM_FIRST_VFP_PSEUDO_REGNUM)
    + ARM_FIRST_VFP_REGNUM;
  int s_reg_msw = s_reg_lsw + 1;
  regcache_cooked_write (regcache, s_reg_lsw, buf);
  regcache_cooked_write (regcache, s_reg_msw, buf + 4);
}

/* This is cribbed from arm-tdep.c.  I don't want to add all the mach-o 
   code to that file, since then I'll have to deal with merge conflicts,
   but I need this bit.  */

/*
 * The GDB_N_ARM_THUMB_DEF bit of the n_desc field indicates that the symbol is
 * a defintion of a Thumb function.
 */
#define GDB_N_ARM_THUMB_DEF	0x0008 /* symbol is a Thumb function (ARM) */


#define MSYMBOL_SET_SPECIAL(msym)					\
	MSYMBOL_INFO (msym) = (char *) (((long) MSYMBOL_INFO (msym))	\
					| 0x80000000)
static void
arm_macosx_dbx_make_msymbol_special (int16_t desc, struct minimal_symbol *msym)
{
  if (desc & GDB_N_ARM_THUMB_DEF)
    MSYMBOL_SET_SPECIAL (msym);
}

/* Convert a dbx stab register number (from `r' declaration) to a gdb
   REGNUM. */
int
arm_macosx_stab_reg_to_regnum (int num)
{
  int regnum = num;

  /* Check for the VFP floating point registers numbers.  */
  if (num >= ARM_MACOSX_FIRST_VFP_STABS_REGNUM 
      && num <= ARM_MACOSX_LAST_VFP_STABS_REGNUM)
    regnum = ARM_FIRST_VFP_REGNUM + num - ARM_MACOSX_FIRST_VFP_STABS_REGNUM;
  else
    regnum = num; /* Most registers do not need any modification.  */
    
  return regnum;
}

/* Grub around in the argument list to find the exception object,
   and return the type info string (without the "typeinfo for " bits).
   CURR_FRAME is the __cxa_throw frame.
   NOTE: We are getting the mangled name of the typeinfo object, and
   demangling that.  We could instead look inside the object, and pull
   out the string description field, but then we have to know where this
   is in the typeinfo object, or call a function.  Getting the mangled
   name seems much safer & easier.
*/

char *
arm_throw_catch_find_typeinfo (struct frame_info *curr_frame,
                               int exception_type)
{
  struct minimal_symbol *typeinfo_sym = NULL;
  ULONGEST typeinfo_ptr;
  char *typeinfo_str;

  if (exception_type == EX_EVENT_THROW)
    {
      frame_unwind_unsigned_register (curr_frame,
                                      ARM_R0_REGNUM + 1,
                                      &typeinfo_ptr);
      typeinfo_sym = lookup_minimal_symbol_by_pc (typeinfo_ptr);

    }
  else
    {
      /* This is hacky, the runtime code gets a pointer to an _Unwind_Exception,
         which is actually contained in the __cxa_exception that we want.  But
         the function that does the cast is a static inline, so we can't see it.
         FIXME: we need to get the runtime to keep this so we aren't relying on
         the particular layout of the __cxa_exception...
         Anyway, then the first field of __cxa_exception is the type object. */
      ULONGEST type_obj_addr = 0;

      frame_unwind_unsigned_register (curr_frame,
                                      ARM_R0_REGNUM,
                                      &typeinfo_ptr);

      /* This is also a bit bogus.  We assume that an unsigned integer is the
         same size as an address on our system.  */
      if (safe_read_memory_unsigned_integer
          (typeinfo_ptr - 44, 4, &type_obj_addr))
        typeinfo_sym = lookup_minimal_symbol_by_pc (type_obj_addr);
    }

  if (!typeinfo_sym)
    return NULL;

  typeinfo_str =
    typeinfo_sym->ginfo.language_specific.cplus_specific.demangled_name;
  if ((typeinfo_str == NULL)
      || (strstr (typeinfo_str, "typeinfo for ") != typeinfo_str))
    return NULL;

  return typeinfo_str + strlen ("typeinfo for ");
}

static void
arm_macosx_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* We actually don't have any software float registers, so lets remove 
     the float info printer so we don't crash on "info float" commands.  */
  (gdbarch_tdep (gdbarch))->fp_model = ARM_FLOAT_NONE;
  set_gdbarch_print_float_info (gdbarch, NULL);
  set_gdbarch_stab_reg_to_regnum (gdbarch, arm_macosx_stab_reg_to_regnum);

  set_gdbarch_skip_trampoline_code (gdbarch, macosx_skip_trampoline_code);

  set_gdbarch_in_solib_return_trampoline (gdbarch,
                                          macosx_in_solib_return_trampoline);
  set_gdbarch_fetch_pointer_argument (gdbarch, arm_fetch_pointer_argument);

  set_gdbarch_num_regs (gdbarch, ARM_MACOSX_NUM_REGS);

  set_gdbarch_dbx_make_msymbol_special (gdbarch, arm_macosx_dbx_make_msymbol_special);
}

static void
arm_macosx_init_abi_v6 (struct gdbarch_info info, struct gdbarch *gdbarch)
{

  /* Set the floating point model to be VFP and also initialize the
     stab register number converter.  */
  (gdbarch_tdep (gdbarch))->fp_model = ARM_FLOAT_VFP;
  set_gdbarch_stab_reg_to_regnum (gdbarch, arm_macosx_stab_reg_to_regnum);

  set_gdbarch_skip_trampoline_code (gdbarch, macosx_skip_trampoline_code);

  set_gdbarch_in_solib_return_trampoline (gdbarch,
                                          macosx_in_solib_return_trampoline);
  set_gdbarch_fetch_pointer_argument (gdbarch, arm_fetch_pointer_argument);

  set_gdbarch_num_regs (gdbarch, ARM_V6_MACOSX_NUM_REGS);
  set_gdbarch_num_pseudo_regs (gdbarch, ARM_MACOSX_NUM_VFP_PSEUDO_REGS);
  set_gdbarch_pseudo_register_read (gdbarch, arm_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, arm_pseudo_register_write);
  
  set_gdbarch_dbx_make_msymbol_special (gdbarch, arm_macosx_dbx_make_msymbol_special);
}

void
_initialize_arm_macosx_tdep ()
{
  struct cmd_list_element *cmd = NULL;

  /* This is already done in arm-tdep.c.  I wonder if we shouldn't move this 
     code into there so we can be sure all the initializations happen in the
     right order, etc.  */

  /* register_gdbarch_init (bfd_arch_arm, arm_gdbarch_init); */

  gdbarch_register_osabi_sniffer (bfd_arch_unknown, bfd_target_mach_o_flavour,
                                  arm_mach_o_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_arm, 0, GDB_OSABI_DARWIN,
                          arm_macosx_init_abi);

  gdbarch_register_osabi ((bfd_lookup_arch (bfd_arch_arm, bfd_mach_arm_6))->arch, bfd_mach_arm_6,
                          GDB_OSABI_DARWINV6, arm_macosx_init_abi_v6);

}
