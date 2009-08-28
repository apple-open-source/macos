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
#include "regcache.h"
#include "source.h"
#include "completer.h"
#include "exceptions.h"

#include <dirent.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <mach/machine.h>

#include <CoreFoundation/CoreFoundation.h>

#include "readline/tilde.h" /* For tilde_expand */

#include "macosx-nat-utils.h"

static char *find_info_plist_filename_from_bundle_name (const char *bundle,
                                                     const char *bundle_suffix);

#if USE_DEBUG_SYMBOLS_FRAMEWORK
extern CFArrayRef DBGCopyMatchingUUIDsForURL (CFURLRef path,
                                              int /* cpu_type_t */ cpuType,
                                           int /* cpu_subtype_t */ cpuSubtype);
extern CFURLRef DBGCopyDSYMURLForUUID (CFUUIDRef uuid);
#endif

static const char dsym_extension[] = ".dSYM";
static const char dsym_bundle_subdir[] = "Contents/Resources/DWARF/";
static const int  dsym_extension_len = (sizeof (dsym_extension) - 1);
static const int  dsym_bundle_subdir_len = (sizeof (dsym_bundle_subdir) - 1);
static int dsym_locate_enabled = 1;
#define APPLE_DSYM_EXT_AND_SUBDIRECTORY ".dSYM/Contents/Resources/DWARF/"

int
actually_do_stack_frame_prologue (unsigned int count_limit, 
				unsigned int print_limit,
				unsigned int wordsize,
				unsigned int *count,
				struct frame_info **out_fi,
				void (print_fun) (struct ui_out * uiout, int frame_num,
						  CORE_ADDR pc, CORE_ADDR fp));

/* When we're doing native debugging, and we attach to a process,
   we start out by finding the in-memory dyld -- the osabi of that
   dyld is stashed away here for use when picking the right osabi of
   a fat file.  In the case of cross-debugging, none of this happens
   and this global remains untouched.  */

enum gdb_osabi osabi_seen_in_attached_dyld = GDB_OSABI_UNKNOWN;

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
      /* complain (&unsupported_indirect_symtype_complaint, hex_string (macho_type)); */
      return macho_type;

    default:
      /* complain (&unknown_macho_symtype_complaint, hex_string (macho_type)); */
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
              /* complain (&unknown_macho_section_complaint, hex_string (macho_sect)); */
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
              /* complain (&unknown_macho_section_complaint, hex_string (macho_sect)); */
              ntype |= N_DATA;
            }
        }

      else
        {
          /* complain (&unknown_macho_section_complaint, hex_string (macho_sect)); */
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

  sym = lookup_symbol_global (lname, lname, VAR_DOMAIN, 0);
  if ((sym == NULL) && (lname[0] == '_'))
    sym = lookup_symbol_global (lname + 1, lname + 1, VAR_DOMAIN, 0);
  if (sym != NULL && SYMBOL_BLOCK_VALUE (sym) != NULL)
    /* APPLE LOCAL begin address ranges  */
    return BLOCK_LOWEST_PC (SYMBOL_BLOCK_VALUE (sym));
    /* APPLE LOCAL end address ranges  */

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
   in a stub.  NOTE, this function assumes the symbols passed in are of type
   N_SECT.  */

int
macosx_record_symbols_from_sect_p (bfd *abfd, unsigned char macho_type, 
				   unsigned char macho_sect)
{
  const bfd_mach_o_section *sect;
  /* We sometimes get malformed symbols which are of type N_SECT, but
     with a section number of NO_SECT.  */
  if (macho_sect <= 0 || macho_sect > abfd->tdata.mach_o_data->nsects)
    {
      warning ("Bad symbol - type is N_SECT but section is %d", macho_sect);
      return 0;
    }

  sect = abfd->tdata.mach_o_data->sections[macho_sect - 1];
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
  if (TYPE_CODE (value_type (val)) == TYPE_CODE_REF)
    val = value_ind (val);
  if ((TYPE_CODE (value_type (val)) == TYPE_CODE_FUNC)
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

struct sal_chain
{
  struct sal_chain *next;
  struct symtab_and_line sal;
};


/* On some platforms, you need to turn on the exception callback
   to hit the catchpoints for exceptions.  Not on Mac OS X. */

int
macosx_enable_exception_callback (enum exception_event_kind kind, int enable)
{
  return 1;
}

/* The MacOS X implemenatation of the find_exception_catchpoints
   target vector entry.  Relies on the __cxa_throw and
   __cxa_begin_catch functions from libsupc++.  */

struct symtabs_and_lines *
macosx_find_exception_catchpoints (enum exception_event_kind kind,
                                   struct objfile *restrict_objfile)
{
  struct symtabs_and_lines *return_sals;
  char *symbol_name;
  struct objfile *objfile;
  struct minimal_symbol *msymbol;
  unsigned int hash;
  struct sal_chain *sal_chain = 0;

  switch (kind)
    {
    case EX_EVENT_THROW:
      symbol_name = "__cxa_throw";
      break;
    case EX_EVENT_CATCH:
      symbol_name = "__cxa_begin_catch";
      break;
    default:
      error ("We currently only handle \"throw\" and \"catch\"");
    }

  hash = msymbol_hash (symbol_name) % MINIMAL_SYMBOL_HASH_SIZE;

  ALL_OBJFILES (objfile)
  {
    for (msymbol = objfile->msymbol_hash[hash];
         msymbol != NULL; msymbol = msymbol->hash_next)
      if (MSYMBOL_TYPE (msymbol) == mst_text
          && (strcmp_iw (SYMBOL_LINKAGE_NAME (msymbol), symbol_name) == 0))
        {
          /* We found one, add it here... */
          CORE_ADDR catchpoint_address;
          CORE_ADDR past_prologue;

          struct sal_chain *next
            = (struct sal_chain *) alloca (sizeof (struct sal_chain));

          next->next = sal_chain;
          init_sal (&next->sal);
          next->sal.symtab = NULL;

          catchpoint_address = SYMBOL_VALUE_ADDRESS (msymbol);
          past_prologue = SKIP_PROLOGUE (catchpoint_address);

          next->sal.pc = past_prologue;
          next->sal.line = 0;
          next->sal.end = past_prologue;

          sal_chain = next;

        }
  }

  if (sal_chain)
    {
      int index = 0;
      struct sal_chain *temp;

      for (temp = sal_chain; temp != NULL; temp = temp->next)
        index++;

      return_sals = (struct symtabs_and_lines *)
        xmalloc (sizeof (struct symtabs_and_lines));
      return_sals->nelts = index;
      return_sals->sals =
        (struct symtab_and_line *) xmalloc (index *
                                            sizeof (struct symtab_and_line));

      for (index = 0; sal_chain; sal_chain = sal_chain->next, index++)
        return_sals->sals[index] = sal_chain->sal;
      return return_sals;
    }
  else
    return NULL;

}

/* Returns data about the current exception event */

struct exception_event_record *
macosx_get_current_exception_event ()
{
  static struct exception_event_record *exception_event = NULL;
  struct frame_info *curr_frame;
  struct frame_info *fi;
  CORE_ADDR pc;
  int stop_func_found;
  char *stop_name;
  char *typeinfo_str;

  if (exception_event == NULL)
    {
      exception_event = (struct exception_event_record *)
        xmalloc (sizeof (struct exception_event_record));
      exception_event->exception_type = NULL;
    }

  curr_frame = get_current_frame ();
  if (!curr_frame)
    return (struct exception_event_record *) NULL;

  pc = get_frame_pc (curr_frame);
  stop_func_found = find_pc_partial_function (pc, &stop_name, NULL, NULL);
  if (!stop_func_found)
    return (struct exception_event_record *) NULL;

  if (strcmp (stop_name, "__cxa_throw") == 0)
    {

      fi = get_prev_frame (curr_frame);
      if (!fi)
        return (struct exception_event_record *) NULL;

      exception_event->throw_sal = find_pc_line (get_frame_pc (fi), 1);

      /* FIXME: We don't know the catch location when we
         have just intercepted the throw.  Can we walk the
         stack and redo the runtimes exception matching
         to figure this out? */
      exception_event->catch_sal.pc = 0x0;
      exception_event->catch_sal.line = 0;

      exception_event->kind = EX_EVENT_THROW;

    }
  else if (strcmp (stop_name, "__cxa_begin_catch") == 0)
    {
      fi = get_prev_frame (curr_frame);
      if (!fi)
        return (struct exception_event_record *) NULL;

      exception_event->catch_sal = find_pc_line (get_frame_pc (fi), 1);

      /* By the time we get here, we have totally forgotten
         where we were thrown from... */
      exception_event->throw_sal.pc = 0x0;
      exception_event->throw_sal.line = 0;

      exception_event->kind = EX_EVENT_CATCH;


    }

#ifdef THROW_CATCH_FIND_TYPEINFO
  typeinfo_str =
    THROW_CATCH_FIND_TYPEINFO (curr_frame, exception_event->kind);
#else
  typeinfo_str = NULL;
#endif

  if (exception_event->exception_type != NULL)
    xfree (exception_event->exception_type);

  if (typeinfo_str == NULL)
    {
      exception_event->exception_type = NULL;
    }
  else
    {
      exception_event->exception_type = xstrdup (typeinfo_str);
    }

  return exception_event;
}

void
update_command (char *args, int from_tty)
{
  registers_changed ();
  reinit_frame_cache ();
}

void
stack_flush_command (char *args, int from_tty)
{
  reinit_frame_cache ();
  if (from_tty)
    printf_filtered ("Stack cache flushed.\n");
}

/* Opens the file pointed to in ARGS with the default editor
   given by LaunchServices.  If ARGS is NULL, opens the current
   source file & line.  You can also supply file:line and it will
   open the that file & try to put the selection on that line.  */

static void
open_command (char *args, int from_tty)
{
  const char *filename = NULL;  /* Possibly directory-less filename */
  const char *fullname = NULL;  /* Fully qualified on-disk filename */
  struct stat sb;
  int line_no = 0;

  warning ("open command no longer supported - may be back in a future build.");
  return;

  if (args == NULL || args[0] == '\0')
    {
      filename = NULL;
      line_no = 0;
    }

  else
    {
      char *colon_pos = strrchr (args, ':');
      if (colon_pos == NULL)
	line_no = 0;
      else
	{
	  line_no = atoi (colon_pos + 1);
	  *colon_pos = '\0';
	}
      filename = args;
    }

  if (filename == NULL)
    {
      struct symtab_and_line cursal = get_current_source_symtab_and_line ();
      if (cursal.symtab)
        fullname = symtab_to_fullname (cursal.symtab);
      else
        error ("No currently selected source file available; "
               "please specify one.");
      /* The cursal is actually set to the list-size bracket around
         the current line, so we have to add that back in to get the
	 real source line.  */

      line_no = cursal.line + get_lines_to_list () / 2;
    }

  if (fullname == NULL)
    {
       /* lookup_symtab will give us the first match; should we use
	  the Apple local variant, lookup_symtab_all?  And what
	  would we do with the results; open all of them?  */
       struct symtab *s = lookup_symtab (filename);
       if (s)
         fullname = symtab_to_fullname (s);
       else
         error ("Filename '%s' not found in this program's debug information.",
                filename);
    }

  /* Prefer the fully qualified FULLNAME over whatever FILENAME might have.  */

  if (stat (fullname, &sb) == 0)
    filename = fullname;
  else
    if (stat (filename, &sb) != 0)
      error ("File '%s' not found.", filename);
}


/* Helper function for gdb_DBGCopyMatchingUUIDsForURL.
   Given a bfd of a MachO file, look for an LC_UUID load command
   and return that uuid in an allocated CFUUIDRef.
   If the file being examined is fat, we assume that the bfd we're getting
   passed in has already been iterated over to get one of the thin forks of
   the file.
   It is the caller's responsibility to release the memory.
   NULL is returned if we do not find a LC_UUID for any reason.  */

static CFUUIDRef
get_uuidref_for_bfd (struct bfd *abfd)
{
 uint8_t uuid[16];
 if (abfd == NULL)
   return NULL;

 if (bfd_mach_o_get_uuid (abfd, uuid, sizeof (uuid)))
   return CFUUIDCreateWithBytes (kCFAllocatorDefault,
             uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5],
             uuid[6], uuid[7], uuid[8], uuid[9], uuid[10], uuid[11],
             uuid[12], uuid[13], uuid[14], uuid[15]);

 return NULL;
}

/* This is an implementation of the DebugSymbols framework's
   DBGCopyMatchingUUIDsForURL function.  Given the path to a
   dSYM file (not the bundle directory but the actual dSYM dwarf
   file), it will return a CF array of UUIDs that this file has.  
   Normally depending on DebugSymbols.framework isn't a problem but
   we don't have this framework on all platforms and we want the
   "add-dsym" command to continue to work without it. */

static CFMutableArrayRef
gdb_DBGCopyMatchingUUIDsForURL (const char *path)
{
  if (path == NULL || path[0] == '\0')
    return NULL;

  CFAllocatorRef alloc = kCFAllocatorDefault;
  CFMutableArrayRef uuid_array = NULL;
  struct gdb_exception e;
  bfd *abfd;

  TRY_CATCH (e, RETURN_MASK_ERROR)
  {
    abfd = symfile_bfd_open (path, 0);
  }
  
  if (abfd == NULL || e.reason == RETURN_ERROR)
    return NULL;
  if (bfd_check_format (abfd, bfd_archive)
      && strcmp (bfd_get_target (abfd), "mach-o-fat") == 0)
    {
      bfd *nbfd = NULL;
      for (;;)
        {
          nbfd = bfd_openr_next_archived_file (abfd, nbfd);
          if (nbfd == NULL)
            break;
          if (!bfd_check_format (nbfd, bfd_object) 
              && !bfd_check_format (nbfd, bfd_archive))
            continue;
          CFUUIDRef nbfd_uuid = get_uuidref_for_bfd (nbfd);
          if (nbfd_uuid != NULL)
            {
              if (uuid_array == NULL)
                uuid_array = CFArrayCreateMutable(alloc, 0, &kCFTypeArrayCallBacks);
              if (uuid_array)
                CFArrayAppendValue (uuid_array, nbfd_uuid);
              CFRelease (nbfd_uuid);
            }
        }
      bfd_free_cached_info (abfd);
    }
  else
   {
      CFUUIDRef abfd_uuid = get_uuidref_for_bfd (abfd);
      if (abfd_uuid != NULL)
        {
          if (uuid_array == NULL)
            uuid_array = CFArrayCreateMutable(alloc, 0, &kCFTypeArrayCallBacks);
          if (uuid_array)
            CFArrayAppendValue (uuid_array, abfd_uuid);
          CFRelease (abfd_uuid);
        }
    }

  bfd_close (abfd);
  return uuid_array;
}


CFMutableDictionaryRef
create_dsym_uuids_for_path (char *dsym_bundle_path)
{
  char path[PATH_MAX];
  struct dirent* dp = NULL;
  DIR* dirp = NULL;
  char* dsym_path = NULL;
  CFMutableDictionaryRef paths_and_uuids;
  
  /* Copy the base dSYM bundle directory path into our new path.  */
  strncpy (path, dsym_bundle_path, sizeof (path));
  
  if (path[sizeof (path) - 1])
    return NULL;  /* Path is too large.  */
  
  int path_len = strlen (path);
  
  if (path_len > 0)
    {
      /* Add directory delimiter to end of bundle path if
	 needed to normalize the path.  */
      if (path[path_len-1] != '/')
	{
	  path[path_len] = '/';
	  if (path_len + 1 < sizeof (path))
	    path[++path_len] = '\0';
	  else
	    return NULL; /* Path is too large.  */
	}
    }
  
  /* Append the bundle subdirectory path.  */
  if (dsym_bundle_subdir_len + 1 > sizeof (path) - path_len)
    return NULL; /* Path is too large.  */
  
  strncat (path, dsym_bundle_subdir, sizeof (path) - path_len - 1);
  
  if (path[sizeof (path) - 1])
    return NULL;  /* Path is too large.  */
  
  dirp = opendir (path);
  if (dirp == NULL)
    return NULL;
  
  path_len = strlen (path);
  
  /* Leave room for a NULL at the end in case strncpy 
     doesn't NULL terminate.  */
  paths_and_uuids = CFDictionaryCreateMutable (kCFAllocatorDefault, 0, 
					       &kCFTypeDictionaryKeyCallBacks, 
					       &kCFTypeDictionaryValueCallBacks);
  
  while (dsym_path == NULL && (dp = readdir (dirp)) != NULL)
    {
      /* Don't search directories.  Note, some servers return DT_UNKNOWN
         for everything, so you can't assume this test will keep you from
	 trying to read directories...  */
      if (dp->d_type == DT_DIR)
	continue;
      
      /* Full path length to each file in the dSYM's
	 /Contents/Resources/DWARF/ directory.  */
      int full_path_len = path_len + dp->d_namlen + 1;
      if (sizeof (path) > full_path_len)
	{
	  CFURLRef path_url = NULL;
	  CFArrayRef uuid_array = NULL;
	  CFStringRef path_cfstr = NULL;
	  /* Re-use the path each time and only copy the 
	     directory entry name just past the 
	     ".../Contents/Resources/DWARF/" part of PATH.  */
	  strcpy(&path[path_len], dp->d_name);
	  path_cfstr = CFStringCreateWithCString (NULL, path,  
						  kCFStringEncodingUTF8);
	  path_url = CFURLCreateWithFileSystemPath (NULL, path_cfstr,
                                                    kCFURLPOSIXPathStyle, 0);
	  
	  CFRelease (path_cfstr), path_cfstr = NULL;
	  if (path_url == NULL)
	    continue;
	  
	  uuid_array = gdb_DBGCopyMatchingUUIDsForURL (path);
	  if (uuid_array != NULL)
	    CFDictionarySetValue (paths_and_uuids, path_url, uuid_array);
	  
	  /* We are done with PATH_URL.  */
	  CFRelease (path_url);
	  path_url = NULL;
	  
	  /* Skip to next while loop iteration if we didn't get any matches.  */
	  /* Release the UUID array.  */
	  if (uuid_array != NULL)
	    CFRelease (uuid_array);
	  uuid_array = NULL;
	}
    }
  closedir (dirp);
  if (CFDictionaryGetCount (paths_and_uuids) == 0)
    {
      CFRelease (paths_and_uuids);
      return NULL;
    }
  else
    return paths_and_uuids;
}

struct search_baton
{
  CFUUIDRef test_uuid;
  int found_it;
  CFURLRef path_url;
};

void 
paths_and_uuids_map_func (const void *in_url, 
			  const void *in_array, 
			  void *in_results)
{
  const CFURLRef path_url = (CFURLRef) in_url;
  const CFArrayRef uuid_array = (CFArrayRef) in_array;
  struct search_baton *results = (struct search_baton *) in_results;

  const CFIndex count = CFArrayGetCount (uuid_array);
  CFIndex i;

  if (results->found_it)
    return;
  for (i = 0; i < count; i++)
    {
      CFUUIDRef tmp_uuid = CFArrayGetValueAtIndex (uuid_array, i);
      if (CFEqual (tmp_uuid, results->test_uuid))
	{
	  results->path_url = path_url;
	  CFRetain (path_url);
	  results->found_it = 1;
	  break;
	}
    }
}

/* Search a dSYM bundle for a specific UUID. UUID_REF is the UUID object
   to look for, and DSYM_BUNDLE_PATH is a path to the top level dSYM bundle
   directory. We need to look through the bundle and find the correct dSYM 
   mach file. This allows dSYM bundle directory names and the dsym mach files
   within them to have names that are different from the name of the
   executable which can be handy when debugging build styles (debug and 
   profile). The returned string has been xmalloc()'ed and it is the 
   responsibility of the caller to xfree it. */
static char *
locate_dsym_mach_in_bundle (CFUUIDRef uuid_ref, char *dsym_bundle_path)
{
  CFMutableDictionaryRef paths_and_uuids;
  struct search_baton results;

  paths_and_uuids = create_dsym_uuids_for_path (dsym_bundle_path);

  if (paths_and_uuids == NULL)
    return NULL;

  results.found_it = 0;
  results.test_uuid = uuid_ref;

  CFDictionaryApplyFunction (paths_and_uuids, paths_and_uuids_map_func,
			     &results);

  CFRelease (paths_and_uuids);

  if (results.found_it)
    {
      char path[PATH_MAX];
      path[PATH_MAX-1] = '\0';
      if (CFURLGetFileSystemRepresentation (results.path_url, 1, 
						(UInt8 *)path, sizeof (path)))
	return xstrdup (path);
      else
	return NULL;
    }
  else
    return NULL;

}

#if USE_DEBUG_SYMBOLS_FRAMEWORK
/* Locate a full path to the dSYM mach file within the dSYM bundle using
   OJBFILE's uuid and the DebugSymbols.framework. The DebugSymbols.framework 
   will used using the current set of global DebugSymbols.framework defaults 
   from com.apple.DebugSymbols.plist.  If a UUID is available and a path to
   a dSYM is returned from the framework, the dSYM bundle contents will be
   searched to find a matching UUID only if the URL returned by the framework
   doesn't fully specify the dSYM mach file. The returned string has been 
   xmalloc()'ed and it is the responsibility of the caller to xfree it. */
static char *
locate_dsym_using_framework (struct objfile *objfile)
{
  unsigned char uuid[16];
  CFURLRef dsym_bundle_url = NULL;
  char* dsym_path = NULL;
  /* Extract the UUID from the objfile.  */
  if (bfd_mach_o_get_uuid (objfile->obfd, uuid, sizeof (uuid)))
    {
      /* Create a CFUUID object for use with DebugSymbols framework.  */
      CFUUIDRef uuid_ref = CFUUIDCreateWithBytes (kCFAllocatorDefault, uuid[0], 
						  uuid[1], uuid[2], uuid[3], 
						  uuid[4], uuid[5], uuid[6], 
						  uuid[7], uuid[8], uuid[9],
						  uuid[10], uuid[11], uuid[12], 
						  uuid[13], uuid[14], uuid[15]);
      if (uuid_ref == NULL)
	return NULL;
      
      /* Use DebugSymbols framework to search for the dSYM.  */
      dsym_bundle_url = DBGCopyDSYMURLForUUID (uuid_ref);
      if (dsym_bundle_url)
	{
	  /* Get the path for the URL in 8 bit format.  */
	  char path[PATH_MAX];
	  path[PATH_MAX-1] = '\0';
	  if (CFURLGetFileSystemRepresentation (dsym_bundle_url, 1, 
						(UInt8 *)path, sizeof (path)))
	    {
	      char *dsym_ext = strcasestr (path, dsym_extension);
	      /* Check the dsym path to see if it is a full path to a dSYM
		 mach file in the dSYM bundle. We do this by checking:
		 1 - If there is no dSYM extension in the path
		 2 - If the path ends with ".dSYM"
		 3 - If the path ends with ".dSYM/"
	      */
	      int search_bundle_dir = ((dsym_ext == NULL) || 
				       (dsym_ext[dsym_extension_len] == '\0') ||
					(dsym_ext[dsym_extension_len] == '/' && 
					dsym_ext[dsym_extension_len+1] == '\0'));
				       
	      if (search_bundle_dir)
		{
		  dsym_path = locate_dsym_mach_in_bundle (uuid_ref, path);
		}
	      else
		{
		  /* Don't mess with the path if it was fully specified. PATH 
		     should be a full path to the dSYM mach file within the 
		     dSYM bundle directory.  */
		  dsym_path = xstrdup (path);
		}
	    }
	  CFRelease (dsym_bundle_url);
	  dsym_bundle_url = NULL;
	}
      CFRelease (uuid_ref);
      uuid_ref = NULL;
    }
  return dsym_path;
}
#endif

/* Locate a full path to the dSYM mach file within the dSYM bundle given
   OJBFILE. This function will first search in the same directory as the
   executable for OBJFILE, then it will traverse the directory structure
   upwards looking for any dSYM bundles at the bundle level. If no dSYM
   file is found in the parent directories of the executable, then the
   DebugSymbols.framework will used using the current set of global 
   DebugSymbols.framework defaults from com.apple.DebugSymbols.plist.  */

char *
macosx_locate_dsym (struct objfile *objfile)
{
  char *basename_str;
  char *dot_ptr;
  char *slash_ptr;
  char *dsymfile;
  const char *executable_name;

  /* Don't load a dSYM file unless we our load level is set to ALL.  If a
     load level gets raised, then the old objfile will get destroyed and
     it will get rebuilt, and this function will get called again and get
     its chance to locate the dSYM file.  */
  if (objfile->symflags != OBJF_SYM_ALL)
    return NULL;

  /* When we're debugging a kext with dSYM, OBJFILE is the kext syms
     output by kextload (com.apple.IOKitHello.syms), 
     objfile->not_loaded_kext_filename is the name of the kext bundle
     (IOKitHello.kext) and we're going to be looking for IOKitHello.kext.dSYM
     in this function.  */
  if (objfile->not_loaded_kext_filename != NULL)
    executable_name = objfile->not_loaded_kext_filename;
  else
    executable_name = objfile->name;

  /* Make sure the object file name itself doesn't contain ".dSYM" in it or we
     will end up with an infinite loop where after we add a dSYM symbol file,
     it will then enter this function asking if there is a debug file for the
     dSYM file itself.  */
  if (strcasestr (executable_name, ".dSYM") == NULL)
    {
      /* Check for the existence of a .dSYM file for a given executable.  */
      basename_str = basename (executable_name);
      dsymfile = alloca (strlen (executable_name)
			       + strlen (APPLE_DSYM_EXT_AND_SUBDIRECTORY)
			       + strlen (basename_str)
			       + 1);
      
      /* First try for the dSYM in the same directory as the original file.  */
      strcpy (dsymfile, executable_name);
      strcat (dsymfile, APPLE_DSYM_EXT_AND_SUBDIRECTORY);
      strcat (dsymfile, basename_str);
	  
      if (file_exists_p (dsymfile))
	return xstrdup (dsymfile);
      
      /* Now search for any parent directory that has a '.' in it so we can find
	 Mac OS X applications, bundles, plugins, and any other kinds of files.  
	 Mac OS X application bundles wil have their program in
	 "/some/path/MyApp.app/Contents/MacOS/MyApp" (or replace ".app" with
	 ".bundle" or ".plugin" for other types of bundles).  So we look for any
	 prior '.' character and try appending the apple dSYM extension and 
	 subdirectory and see if we find an existing dSYM file (in the above 
         MyApp example the dSYM would be at either:
	 "/some/path/MyApp.app.dSYM/Contents/Resources/DWARF/MyApp" or
	 "/some/path/MyApp.dSYM/Contents/Resources/DWARF/MyApp".  */
      strcpy (dsymfile, dirname (executable_name));
      /* Append a directory delimiter so we don't miss shallow bundles that
         have the dSYM appended on like "/some/path/MacApp.app.dSYM" when
	 we start with "/some/path/MyApp.app/MyApp".  */
      strcat (dsymfile, "/");
      while ((dot_ptr = strrchr (dsymfile, '.')))
	{
	  /* Find the directory delimiter that follows the '.' character since
	     we now look for a .dSYM that follows any bundle extension.  */
	  slash_ptr = strchr (dot_ptr, '/');
	  if (slash_ptr)
	    {
	      /* NULL terminate the string at the '/' character and append
	         the path down to the dSYM file.  */
	      *slash_ptr = '\0';
	      strcat (slash_ptr, APPLE_DSYM_EXT_AND_SUBDIRECTORY);
	      strcat (slash_ptr, basename_str);
	      if (file_exists_p (dsymfile))
		return xstrdup (dsymfile);
	    }
	    
	  /* NULL terminate the string at the '.' character and append
	     the path down to the dSYM file.  */
	  *dot_ptr = '\0';
	  strcat (dot_ptr, APPLE_DSYM_EXT_AND_SUBDIRECTORY);
	  strcat (dot_ptr, basename_str);
	  if (file_exists_p (dsymfile))
	    return xstrdup (dsymfile);

	  /* NULL terminate the string at the '.' locatated by the strrchr() 
             function again.  */
	  *dot_ptr = '\0';

	  /* We found a previous extension '.' character and did not find a 
             dSYM file so now find previous directory delimiter so we don't 
             try multiple times on a file name that may have a version number 
             in it such as "/some/path/MyApp.6.0.4.app".  */
	  slash_ptr = strrchr (dsymfile, '/');
	  if (!slash_ptr)
	    break;
	  /* NULL terminate the string at the previous directory character 
             and search again.  */
	  *slash_ptr = '\0';
	}
#if USE_DEBUG_SYMBOLS_FRAMEWORK
      /* Check to see if configure detected the DebugSymbols framework, and
	 try to use it to locate the dSYM files if it was detected.  */
      if (dsym_locate_enabled)
	return locate_dsym_using_framework (objfile);
#endif
    }
  return NULL;
}


struct objfile *
macosx_find_objfile_matching_dsym_in_bundle (char *dsym_bundle_path, char **out_full_path)
{
  CFMutableDictionaryRef paths_and_uuids;
  struct search_baton results;
  struct objfile *objfile;
  struct objfile *out_objfile = NULL;

  paths_and_uuids = create_dsym_uuids_for_path (dsym_bundle_path);
  if (paths_and_uuids == NULL)
    return NULL;

  results.found_it = 0;
  *out_full_path = NULL;

  ALL_OBJFILES (objfile)
  {
    unsigned char uuid[16];
  /* Extract the UUID from the objfile.  */
    if (bfd_mach_o_get_uuid (objfile->obfd, uuid, sizeof (uuid)))
      {
	/* Create a CFUUID object for use with DebugSymbols framework.  */
	CFUUIDRef uuid_ref = CFUUIDCreateWithBytes (kCFAllocatorDefault, uuid[0], 
						    uuid[1], uuid[2], uuid[3], 
						    uuid[4], uuid[5], uuid[6], 
						    uuid[7], uuid[8], uuid[9],
						    uuid[10], uuid[11], uuid[12], 
						    uuid[13], uuid[14], uuid[15]);
	if (uuid_ref == NULL)
	  continue;
	results.test_uuid = uuid_ref;
	CFDictionaryApplyFunction (paths_and_uuids, paths_and_uuids_map_func,
				   &results);
	CFRelease (uuid_ref);

	if (results.found_it)
	  {
	    *out_full_path = xmalloc (PATH_MAX);
	    *(*out_full_path) = '\0';
	    if (CFURLGetFileSystemRepresentation (results.path_url, 1,
						  (UInt8 *) (*out_full_path), PATH_MAX - 1))
	      {
		out_objfile = objfile;
	      }
	    else
	      {
		warning ("Could not get file system representation for URL:");
		CFShow (results.path_url);
		*out_full_path = NULL;
		out_objfile = NULL;
	      }
	    CFRelease (results.path_url);
	    goto cleanup_and_return;
	    
	  }
      }
    else
      continue;
    
  }
 cleanup_and_return:
  CFRelease (paths_and_uuids);
  return out_objfile;
}

/* Given a path to a kext bundle look in the Info.plist and retrieve
   the CFBundleExecutable (the name of the kext bundle executable) and
   the CFBundleIdentifier (the thing that kextload -s/-a outputs).  
   Returns the canonicalized path to the kext bundle top-level directory.

   For instance, given a FILENAME of
       /tmp/DummySysctl/build/Debug/DummySysctl.kext/Contents/MacOS/DummySysctl

   BUNDLE_EXECUTABLE_NAME_FROM_PLIST will be set to DummySysctl
   BUNDLE_IDENTIFIER_NAME_FROM_PLIST will be set to com.osxbook.kext.DummySysctl
   and the value /tmp/DummySysctl/build/Debug/DummySysctl.kext will be returned.

   All three strings have been xmalloc()'ed, it is the caller's responsibility
   to xfree them.  */

char *
macosx_kext_info (const char *filename, 
                  const char **bundle_executable_name_from_plist,
                  const char **bundle_identifier_name_from_plist)
{
  char *info_plist_name;
  char *t;
  *bundle_executable_name_from_plist = NULL;
  *bundle_identifier_name_from_plist = NULL;
  const void *plist = NULL;

  info_plist_name = find_info_plist_filename_from_bundle_name 
                                                      (filename, ".kext");
  if (info_plist_name == NULL)
    return NULL;

  plist = macosx_parse_plist (info_plist_name);

  *bundle_executable_name_from_plist = macosx_get_plist_posix_value (plist, 
						      "CFBundleExecutable");
  *bundle_identifier_name_from_plist = macosx_get_plist_string_value (plist, 
						      "CFBundleIdentifier");
  macosx_free_plist (&plist);
  
  /* Was there a /Contents directory in the bundle?  */
  t = strstr (info_plist_name, "/Contents");
  if (t != NULL)
    t[0] = '\0';
  
  /* Or is it a flat bundle with the Info.plist at the top level?  */
  t = strstr (info_plist_name, "/Info.plist");
  if (t != NULL)
    t[0] = '\0';

  if (*bundle_executable_name_from_plist == NULL
      || *bundle_identifier_name_from_plist == NULL)
    return NULL;
  else
    return info_plist_name;
}

/* Given a BUNDLE from the user such as
    /a/b/c/Foo.app
    /a/b/c/Foo.app/
    /a/b/c/Foo.app/Contents/MacOS/Foo
   (for BUNDLE_SUFFIX of ".app") return the string
    /a/b/c/Foo.app/Contents/Info.plist
   The return string has been xmalloc()'ed; it is the caller's
   responsibility to free it.  */

static char *
find_info_plist_filename_from_bundle_name (const char *bundle, 
                                           const char *bundle_suffix)
{
  char *t;
  char *bundle_copy;
  char tmp_path[PATH_MAX];
  char realpath_buf[PATH_MAX];
  char *retval = NULL;
   
  /* Make a local copy of BUNDLE so it may be modified below.  */
  bundle_copy = tilde_expand (bundle);
  tmp_path[0] = '\0';

  /* Is BUNDLE in the form "/a/b/c/Foo.kext/Contents/MacOS/Foo"?  */
  t = strstr (bundle_copy, bundle_suffix);
  if (t != NULL && t > bundle_copy)
    {
      t += strlen (bundle_suffix);
      /* Do we have a / character after the bundle suffix?  */
      if (t[0] == '/')
        {
          strncpy (tmp_path, bundle_copy, t - bundle_copy);
          tmp_path[t - bundle_copy] = '\0';
        }
    }

   /* Is BUNDLE in the form "/a/b/c/Foo.kext"?  */
   t = strstr (bundle_copy, bundle_suffix);
   if (t != NULL && t > bundle_copy && t[strlen (bundle_suffix)] == '\0')
     {
          strcpy (tmp_path, bundle_copy);
     }

   if (tmp_path[0] == '\0')
     return NULL;

   /* Now let's find the Info.plist in the bundle.  */

   strcpy (realpath_buf, tmp_path);
   strcat (realpath_buf, "/Contents/Info.plist");
   if (file_exists_p (realpath_buf))
     {
       retval = realpath_buf;
     }
   else
     {
       strcpy (realpath_buf, tmp_path);
       strcat (realpath_buf, "/Info.plist");
       if (file_exists_p (realpath_buf))
         {
           retval = realpath_buf;
         }
     }

   if (retval == NULL)
     return retval;

   tmp_path[0] = '\0';  /* Not necessary; just to make it clear. */

    if (realpath (realpath_buf, tmp_path) == NULL)
        retval = xstrdup (realpath_buf);
    else
        retval = xstrdup (tmp_path);

   xfree (bundle_copy);
  return retval;
}

/* FIXME: We shouldn't be grabbing internal functions from bfd!  It's
 used in both the osabi sniffers.  */
extern const bfd_arch_info_type *bfd_default_compatible
  (const bfd_arch_info_type *a, const bfd_arch_info_type *b);

/* If we're attaching to a process, we start by finding the dyld that
   is loaded and go from there.  So when we're selecting the OSABI,
   prefer the osabi of the actually-loaded dyld when we can.  

   That's what this function implements, but it does it in a somewhat
   roundabout way.  What this really does is this:

   1) If we haven't seen a dyld loaded into a running program yet,
   it returns GDB_OSABI_UNKNOWN.
   2) If you give it a bfd_object file it returns GDB_OSABI_UNKNOWN.
   3) If you give it a mach-o-fat file, it will return osabi_seen_in_attached_dyld
   if that architecture exists in the fat file, otherwise it will return 
   GDB_OSABI_UNKNOWN.

   The sniffer code gets asked two questions - generically what architecture
   do you think we are - where we usually get handed the fat file, and we see
   if it matches what either what DYLD told us or what we guessed from the
   system & the executable file.  That's the job of this function.
   The sniffer also gets asked whether a bfd_object (usually one fork of the
   fat file) is the one that we want.  This function doesn't do that, and
   instead the code in generic_mach_o_osabi_sniffer_use_dyld does that job.
   So this function really only looks at fat archives.

*/

static enum gdb_osabi
generic_mach_o_osabi_sniffer_use_dyld_hint (bfd *abfd,
					    enum bfd_architecture arch,
					    unsigned long mach_32,
					    unsigned long mach_64)
{
  if (osabi_seen_in_attached_dyld == GDB_OSABI_UNKNOWN)
    return GDB_OSABI_UNKNOWN;

  bfd *nbfd = NULL;

  for (;;)
    {
      nbfd = bfd_openr_next_archived_file (abfd, nbfd);

      if (nbfd == NULL)
        break;

      /* We don't deal with FAT archives here.  So just skip it if we were
         handed a fat archive.  */
      if (bfd_check_format (nbfd, bfd_archive))
        return GDB_OSABI_UNKNOWN;

      if (!bfd_check_format (nbfd, bfd_object))
        continue;
      if (bfd_default_compatible (bfd_get_arch_info (nbfd),
                                  bfd_lookup_arch (arch,
                                                   mach_64))
          && osabi_seen_in_attached_dyld == GDB_OSABI_DARWIN64)
        return GDB_OSABI_DARWIN64;

      else if (bfd_default_compatible (bfd_get_arch_info (nbfd),
                                  bfd_lookup_arch (arch,
                                                   mach_32))
          && osabi_seen_in_attached_dyld == GDB_OSABI_DARWIN)
        return GDB_OSABI_DARWIN;

    }

  return GDB_OSABI_UNKNOWN;
}

enum gdb_osabi
generic_mach_o_osabi_sniffer (bfd *abfd, enum bfd_architecture arch, 
			      unsigned long mach_32,
			      unsigned long mach_64,
			      int (*query_64_bit_fn) ())
{
  enum gdb_osabi ret;
  ret = generic_mach_o_osabi_sniffer_use_dyld_hint (abfd, arch, mach_32, mach_64);

  if (ret == GDB_OSABI_DARWIN64 || ret == GDB_OSABI_DARWIN)
    return ret;

 if (bfd_check_format (abfd, bfd_archive))
    {
      bfd *nbfd = NULL;
      /* For a fat archive, look up each component and see which
	 architecture best matches the current architecture.  */
      if (strcmp (bfd_get_target (abfd), "mach-o-fat") == 0)
	{
	  enum gdb_osabi best = GDB_OSABI_UNKNOWN;
	  enum gdb_osabi cur = GDB_OSABI_UNKNOWN;
	  
	  for (;;)
	    {
	      nbfd = bfd_openr_next_archived_file (abfd, nbfd);

	      if (nbfd == NULL)
		break;
	      /* We can check the architecture of objects, and
		 "ar" archives.  Do that here.  */

	      if (!bfd_check_format (nbfd, bfd_object) 
		  && !bfd_check_format (nbfd, bfd_archive))
		continue;
	      
	      cur = generic_mach_o_osabi_sniffer (nbfd, arch,
						  mach_32, mach_64, 
						  query_64_bit_fn);
	      if (cur == GDB_OSABI_DARWIN64 &&
		  best != GDB_OSABI_DARWIN64 && query_64_bit_fn ())
		best = cur;
	      
	      if (cur == GDB_OSABI_DARWIN
		  && best != GDB_OSABI_DARWIN64 
		  && best != GDB_OSABI_DARWIN)
		best = cur;
	    }
	  return best;
	}
      else
	{
	  /* For an "ar" archive, look at the first object element
	     (there's an initial element in the archive that's not a
	     bfd_object, so we have to skip over that.)  And return
	     the architecture from that.  N.B. We can't close the
	     files we open here since the BFD archive code caches
	     them, and there's no way to get them out of the cache
	     without closing the whole archive.  */
	  for (;;)
	    {
	      nbfd = bfd_openr_next_archived_file (abfd, nbfd);
	      if (nbfd == NULL)
		break;
	      if (!bfd_check_format (nbfd, bfd_object))
		continue;
	      
	      /* .a files have to be homogenous, so return the result
	         for the first file.  */

	      return generic_mach_o_osabi_sniffer (nbfd, arch, 
						   mach_32, mach_64, 
						   query_64_bit_fn);
	    }
	}
    }

  if (!bfd_check_format (abfd, bfd_object))
    return GDB_OSABI_UNKNOWN;

  if (bfd_get_arch (abfd) == arch)
    {
      if (bfd_default_compatible (bfd_get_arch_info (abfd),
                                  bfd_lookup_arch (arch,
                                                   mach_64)))
        return GDB_OSABI_DARWIN64;

	  if (bfd_default_compatible (bfd_get_arch_info (abfd),
                                  bfd_lookup_arch (arch,
                                                   mach_32)))
        return GDB_OSABI_DARWIN;

      return GDB_OSABI_UNKNOWN;
    }

  return GDB_OSABI_UNKNOWN;

}

/* This is the common bit of the fast show stack trace.  Here we look
   up the sigtramp start & end, and use the regular backtracer to skip
   over the first few frames, which is the hard bit anyway.  Fills
   COUNT with the number of frames consumed, sets OUT_FI to the last
   frame we read.  Returns 1 if there's more to backtrace, and 0 if we
   are done, and -1 if there was an error.  Note, this is separate
   from COUNT, since you can reach main before you exceed
   COUNT_LIMIT.*/

int
fast_show_stack_trace_prologue (unsigned int count_limit, 
				unsigned int print_limit,
				unsigned int wordsize,
				CORE_ADDR *sigtramp_start_ptr,
				CORE_ADDR *sigtramp_end_ptr,
				unsigned int *count,
				struct frame_info **out_fi,
				void (print_fun) (struct ui_out * uiout, int frame_num,
						  CORE_ADDR pc, CORE_ADDR fp))
{
  ULONGEST pc = 0;
  struct frame_id selected_frame_id;
  struct frame_info *selected_frame;

  if (*sigtramp_start_ptr == 0)
    {
      char *name;
      struct minimal_symbol *msymbol;

      msymbol = lookup_minimal_symbol ("_sigtramp", NULL, NULL);
      if (msymbol == NULL)
        warning
          ("Couldn't find minimal symbol for \"_sigtramp\" - "
	   "backtraces may be unreliable");
      else
        {
          pc = SYMBOL_VALUE_ADDRESS (msymbol);
          if (find_pc_partial_function_no_inlined (pc, &name,
						   sigtramp_start_ptr, 
						   sigtramp_end_ptr) == 0)
            {
              warning
		("Couldn't find minimal bounds for \"_sigtramp\" - "
		 "backtraces may be unreliable");
	      *sigtramp_start_ptr = (CORE_ADDR) -1;
	      *sigtramp_end_ptr = (CORE_ADDR) -1;
            }
        }
    }

  /* I call flush_cached_frames here before we start doing the
     backtrace.  You usually only call stack-list-frames-lite 
     (the parent of this) right when you've stopped.  But you may
     needed to raise the load level of the bottom-most frame to 
     get the backtrace right, and if you've done something like
     called a function before doing the backtrace, the bottom-most
     frame could have inaccurate data.  For instance, I've seen
     a case where the func for the bottom frame was errantly
     given as _start because there were no external symbols
     between the real function and _start...  This will set
     us back straight, and then we can do the backtrace accurately
     from here.  */
  /* Watch out, though.  flush_cached_frames unsets the currently
     selected frame.  So we need to restore that.  */
  selected_frame_id = get_frame_id (get_selected_frame (NULL));

  flush_cached_frames ();

  selected_frame = frame_find_by_id (selected_frame_id);
  if (selected_frame == NULL)
    select_frame (get_current_frame ());
  else
    select_frame (selected_frame);

  /* I have to do this twice because I want to make sure that if
     any of the early backtraces causes the load level of a library
     to be raised, I flush the current frame set & start over.  
     But I can't figure out how to flush the accumulated ui_out
     content and start afresh if this happens.  If we could
     make this an mi-only command, I could, but there isn't a
     way to do that generically.  You can redirect the output
     in the cli case, but you can't stuff the stream that you've
     gathered the new output to down the current ui_out.  You can
     do that with mi_out_put, but that's not a generic command.  
     This looks stupid, but shouldn't be all that inefficient.  */

  actually_do_stack_frame_prologue (count_limit, 
			      print_limit,
			      wordsize,
			      count,
			      out_fi,
			      NULL);

  return actually_do_stack_frame_prologue (count_limit,
				       print_limit,
				       wordsize,
				       count,
				       out_fi,
				       print_fun);

}


int
actually_do_stack_frame_prologue (unsigned int count_limit, 
				unsigned int print_limit,
				unsigned int wordsize,
				unsigned int *count,
				struct frame_info **out_fi,
				void (print_fun) (struct ui_out * uiout, int frame_num,
						  CORE_ADDR pc, CORE_ADDR fp))
{
  CORE_ADDR fp;
  ULONGEST pc;
  struct frame_info *fi = NULL;
  int i;
  int more_frames;
  int old_load_state;
  
  /* Get the first few frames.  If anything funky is going on, it will
     be here.  The second frame helps us get above frameless functions
     called from signal handlers.  Above these frames we have to deal
     with sigtramps and alloca frames, that is about all. */

 start_again:
  if (print_fun)
    ui_out_begin (uiout, ui_out_type_list, "frames");

  i = 0;
  more_frames = 1;
  pc = 0;

  if (i >= count_limit)
    {
      more_frames = 0;
      goto count_finish;
    }

  fi = get_current_frame ();
  if (fi == NULL)
    {
      more_frames = -1;
      goto count_finish;
    }

  /* Sometimes we can backtrace more accurately when we read
     in debug information.  So let's do that here.  */

  old_load_state = pc_set_load_state (get_frame_pc (fi), OBJF_SYM_ALL, 0);
  if (old_load_state >= 0 && old_load_state != OBJF_SYM_ALL && print_fun == NULL)
    {
      flush_cached_frames ();
      goto start_again;
    }

  if (print_fun && (i < print_limit))
    print_fun (uiout, i, get_frame_pc (fi), get_frame_base (fi));
  i = 1;

  do
    {
      if (i >= count_limit)
	{
	  more_frames = 0;
	  goto count_finish;
	}

      fi = get_prev_frame (fi);
      if (fi == NULL)
	{
	  more_frames = 0;
	  goto count_finish;
	}

      pc = get_frame_pc (fi);
      fp = get_frame_base (fi);

  /* Sometimes we can backtrace more accurately when we read
     in debug information.  So let's do that here.  */

      old_load_state = pc_set_load_state (pc, OBJF_SYM_ALL, 0);
      if (old_load_state >= 0 && old_load_state != OBJF_SYM_ALL && print_fun == NULL)
	{
	  flush_cached_frames ();
	  goto start_again;
	}

      if (print_fun && (i < print_limit))
        print_fun (uiout, i, pc, fp);

      i++;

      /* APPLE LOCAL begin subroutine inlining  */
      if (!backtrace_past_main && inside_main_func (fi)
	  && get_frame_type (fi) != INLINED_FRAME)
	{
	  more_frames = 0;
	  goto count_finish;
	}
      /* APPLE LOCAL end subroutine inlining  */
    }
  while (i < 5);

 count_finish:
  *out_fi = fi;
  *count = i;
  return more_frames;
}

void
_initialize_macosx_tdep ()
{
  struct cmd_list_element *c;
  macosx_symbol_types_init ();

  add_info ("trampoline", info_trampoline_command,
            "Resolve function for DYLD trampoline stub and/or Objective-C call");
  c = add_com ("open", class_support, open_command, _("\
Open the named source file in an application determined by LaunchServices.\n\
With no arguments, open the currently selected source file.\n\
Also takes file:line to hilight the file at the given line."));
  set_cmd_completer (c, filename_completer);
  add_com_alias ("op", "open", class_support, 1);
  add_com_alias ("ope", "open", class_support, 1);

  add_com ("flushstack", class_maintenance, stack_flush_command,
           "Force gdb to flush its stack-frame cache (maintainer command)");

  add_com_alias ("flush", "flushregs", class_maintenance, 1);

  add_com ("update", class_obscure, update_command,
           "Re-read current state information from inferior.");
  
  add_setshow_boolean_cmd ("locate-dsym", class_obscure,
			    &dsym_locate_enabled, _("\
Set locate dSYM files using the DebugSymbols framework."), _("\
Show locate dSYM files using the DebugSymbols framework."), _("\
If set, gdb will try and locate dSYM files using the DebugSymbols framework."),
			    NULL, NULL,
			    &setlist, &showlist);
}
