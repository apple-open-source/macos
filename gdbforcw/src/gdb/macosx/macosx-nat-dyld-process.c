/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002, 2004
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

#include "macosx-nat-dyld-info.h"
#include "macosx-nat-dyld-path.h"
#include "macosx-nat-dyld-io.h"
#include "macosx-nat-dyld.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-dyld-process.h"

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"
#include "mach-o.h"
#include "gdbcore.h"
#include "gdb_regex.h"
#include "gdb-stabs.h"
#include "gdb_assert.h"
#include "interps.h"
#include "objc-lang.h"
#include "gdb_stat.h"

#include "gdb_stat.h"

#include <mach-o/nlist.h>
#include <mach-o/loader.h>
#include <mach-o/dyld_debug.h>

#ifdef USE_MMALLOC
#include "mmprivate.h"
#endif

#include <sys/mman.h>
#include <string.h>

extern int dyld_preload_libraries_flag;
extern int dyld_filter_events_flag;
extern int dyld_always_read_from_memory_flag;
extern char *dyld_symbols_prefix;
extern int dyld_load_dyld_symbols_flag;
extern int dyld_load_dyld_shlib_symbols_flag;
extern int dyld_load_cfm_shlib_symbols_flag;
extern int dyld_print_basenames_flag;
extern int dyld_reload_on_downgrade_flag;
extern char *dyld_load_rules;
extern char *dyld_minimal_load_rules;

#if WITH_CFM
extern int inferior_auto_start_cfm_flag;
#endif /* WITH_CFM */

extern macosx_inferior_status *macosx_status;

static int
dyld_print_status ()
{
  /* do not print status dots when executing MI */
  return !ui_out_is_mi_like_p (uiout);
}

void
dyld_add_inserted_libraries (struct dyld_objfile_info *info,
                             const struct dyld_path_info *d)
{
  const char *s1, *s2;

  CHECK_FATAL (info != NULL);
  CHECK_FATAL (d != NULL);

  s1 = d->insert_libraries;
  if (s1 == NULL)
    {
      return;
    }

  while (*s1 != '\0')
    {

      struct dyld_objfile_entry *e = NULL;

      s2 = strchr (s1, ':');
      if (s2 == NULL)
        {
          s2 = strchr (s1, '\0');
        }
      CHECK_FATAL (s2 != NULL);

      e = dyld_objfile_entry_alloc (info);

      e->user_name = savestring (s1, (s2 - s1));
      e->reason = dyld_reason_init;

      s1 = s2;
      while (*s1 == ':')
        {
          s1++;
        }
    }
}

void
dyld_add_image_libraries (struct dyld_objfile_info *info, bfd *abfd)
{
  struct mach_o_data_struct *mdata = NULL;
  int i;

  CHECK_FATAL (info != NULL);

  if (abfd == NULL)
    {
      return;
    }

  if (!bfd_mach_o_valid (abfd))
    {
      return;
    }

  mdata = abfd->tdata.mach_o_data;

  if (mdata == NULL)
    {
      dyld_debug ("dyld_add_image_libraries: mdata == NULL\n");
      return;
    }

  for (i = 0; i < mdata->header.ncmds; i++)
    {
      struct bfd_mach_o_load_command *cmd = &mdata->commands[i];
      switch (cmd->type)
        {
        case BFD_MACH_O_LC_LOAD_WEAK_DYLIB:
        case BFD_MACH_O_LC_LOAD_DYLINKER:
        case BFD_MACH_O_LC_LOAD_DYLIB:
          {

            struct dyld_objfile_entry *e = NULL;
            char *name = NULL;

            switch (cmd->type)
              {
              case BFD_MACH_O_LC_LOAD_DYLINKER:
                {
                  bfd_mach_o_dylinker_command *dcmd = &cmd->command.dylinker;

                  name = xmalloc (dcmd->name_len + 1);

                  bfd_seek (abfd, dcmd->name_offset, SEEK_SET);
                  if (bfd_bread (name, dcmd->name_len, abfd) != dcmd->name_len)
                    {
                      warning
                        ("Unable to find library name for LC_LOAD_DYLINKER command; ignoring");
                      xfree (name);
                      continue;
                    }
		  name[dcmd->name_len] = '\0';
                  break;
                }
              case BFD_MACH_O_LC_LOAD_DYLIB:
              case BFD_MACH_O_LC_LOAD_WEAK_DYLIB:
                {
                  bfd_mach_o_dylib_command *dcmd = &cmd->command.dylib;

                  name = xmalloc (dcmd->name_len + 1);

                  bfd_seek (abfd, dcmd->name_offset, SEEK_SET);
                  if (bfd_bread (name, dcmd->name_len, abfd) != dcmd->name_len)
                    {
                      warning
                        ("Unable to find library name for LC_LOAD_DYLIB or LC_LOAD_WEAK_DYLIB command; ignoring");
                      xfree (name);
                      continue;
                    }
		  name[dcmd->name_len] = '\0';
                  break;
                }
              default:
                abort ();
              }

            if (name[0] == '\0')
              {
                warning
                  ("No image name specified by LC_LOAD command; ignoring");
                xfree (name);
                name = NULL;
              }

            e = dyld_objfile_entry_alloc (info);

            /* We have to run realpath on the text name here because
               some makefiles build the library with one name, then
               copy it to another.  For instance, they will build
               libfoo.2.dylib, but install the actual binary as
               libfoo.2.1.dylib, and then link libfoo.2.dylib back
               to this.  That means the load command refers to a
               file that exists (as a link) but isn't the same as
               what gets loaded.  If we canonicalize everything to
               the real file, then we won't get fooled by this.  */

            {
              char buf[PATH_MAX];

              if (realpath (name, buf) != NULL)
                {
                  xfree (name);
                  name = xstrdup (buf);
                }
            }
            e->text_name = name;
            e->text_name_valid = 1;
            e->reason = dyld_reason_init;

            switch (cmd->type)
              {
              case BFD_MACH_O_LC_LOAD_DYLINKER:
                e->prefix = dyld_symbols_prefix;
                break;
              case BFD_MACH_O_LC_LOAD_DYLIB:
                break;
              case BFD_MACH_O_LC_LOAD_WEAK_DYLIB:
                e->reason |= dyld_reason_weak_mask;
                break;
              default:
                abort ();
              };
          }
        default:
          break;
        }
    }
}

void
dyld_resolve_filename_image (const struct macosx_dyld_thread_status *s,
                             struct dyld_objfile_entry *e)
{
  struct mach_header header;
  CHECK_FATAL (e->allocated);
  if (e->image_name_valid)
    {
      return;
    }

  if (!e->dyld_valid)
    {
      return;
    }

  target_read_memory (e->dyld_addr, (char *) &header,
                      sizeof (struct mach_header));

  switch (header.filetype)
    {
    case MH_DYLINKER:
    case MH_DYLIB:
      break;
    case MH_BUNDLE:
      break;
    default:
      return;
    }
  e->image_name = dyld_find_dylib_name (header.ncmds, e->dyld_addr);

  if (e->image_name == NULL)
    {
      dyld_debug ("Unable to determine filename for loaded object "
                  "(no LC_ID load command)\n");
    }
  else
    {
      dyld_debug ("Determined filename for loaded object from image\n");
      e->image_name_valid = 1;
    }
}

/* Assuming a Mach header starts at ADDR, and has NCMDS, look for the
   dylib name, and return a malloc'ed string containing the name */

char *
dyld_find_dylib_name (CORE_ADDR addr, int ncmds)
{
  CORE_ADDR curpos;
  int i;
  char *image_name = NULL;

  /* FIXME 64 bit:  64 bit MachO files have a differently sized
     mach_header - the following line is wrong. */

  curpos = addr + sizeof (struct mach_header);
  for (i = 0; i < ncmds; i++)
    {

      struct load_command cmd;
      struct dylib_command dcmd;
      struct dylinker_command dlcmd;
      char name[256];

      target_read_memory (curpos, (char *) &cmd,
                          sizeof (struct load_command));
      if (cmd.cmd == LC_ID_DYLIB)
        {
          target_read_memory (curpos, (char *) &dcmd,
                              sizeof (struct dylib_command));
          target_read_memory (curpos + dcmd.dylib.name.offset, name, 256);
          image_name = savestring (name, strlen (name));
          break;
        }
      else if (cmd.cmd == LC_ID_DYLINKER)
        {
          target_read_memory (curpos, (char *) &dlcmd,
                              sizeof (struct dylinker_command));
          target_read_memory (curpos + dlcmd.name.offset, name, 256);
          image_name = savestring (name, strlen (name));
          break;
        }

      curpos += cmd.cmdsize;
    }
  return image_name;
}

void
dyld_resolve_filenames (const struct macosx_dyld_thread_status *s,
                        struct dyld_objfile_info *new)
{
  int i;
  struct dyld_objfile_entry *e;

  CHECK_FATAL (s != NULL);
  CHECK_FATAL (new != NULL);

  DYLD_ALL_OBJFILE_INFO_ENTRIES (new, e, i)
    {
      if (e->dyld_name_valid)
        continue;
      dyld_resolve_filename_image (s, e);
    }
}

static CORE_ADDR
library_offset (struct dyld_objfile_entry *e)
{
  CHECK_FATAL (e != NULL);
  if (e->image_addr_valid && e->dyld_valid)
    {
      /* FIXME: 64bit ? */
      CHECK_FATAL (e->dyld_addr ==
                   ((e->image_addr + e->dyld_slide) & 0xffffffff)); 
    }

  if (e->dyld_valid)
    return e->dyld_addr;
  else if (e->loaded_from_memory)
    return e->loaded_addr;
  else if (e->image_addr_valid)
    return e->image_addr;
  else
    return 0;
}

int
dyld_parse_load_level (const char *s)
{
  if (strcmp (s, "all") == 0)
    {
      return OBJF_SYM_ALL;
    }
  else if (strcmp (s, "container") == 0)
    {
      return OBJF_SYM_CONTAINER;
    }
  else if (strcmp (s, "extern") == 0)
    {
      return OBJF_SYM_EXTERN;
    }
  else if (strcmp (s, "none") == 0)
    {
      return OBJF_SYM_NONE;
    }
  else
    {
      warning ("unknown setting \"%s\"; using \"none\"\n", s);
      return OBJF_SYM_NONE;
    }
}

int
dyld_resolve_load_flag (const struct dyld_path_info *d,
                        struct dyld_objfile_entry *e, const char *rules)
{
  const char *name = NULL;
  const char *leaf = NULL;

  char **prules = NULL;
  char **trule = NULL;
  int nrules = 0;
  int crule = 0;

  name = dyld_entry_string (e, 1);

  if (name == NULL)
    return OBJF_SYM_NONE;

  leaf = strrchr (name, '/');
  leaf = ((leaf != NULL) ? leaf : name);

  if (rules != NULL)
    {
      prules = buildargv (rules);
      if (prules == NULL)
        {
          warning ("unable to parse load rules");
          return OBJF_SYM_NONE;
        }
    }

  nrules = 0;

  if (prules != NULL)
    {
      for (trule = prules; *trule != NULL; trule++)
        {
          nrules++;
        }
    }

  if ((nrules % 3) != 0)
    {
      warning
        ("unable to parse load-rules (number of rule clauses must be a "
         "multiple of 3)");
      return OBJF_SYM_NONE;
    }
  nrules /= 3;

  for (crule = 0; crule < nrules; crule++)
    {

      char *matchreason = prules[crule * 3];
      char *matchname = prules[(crule * 3) + 1];
      char *setting = prules[(crule * 3) + 2];

      const char *reason = NULL;
      const char *name = NULL;

      regex_t reasonbuf;
      regex_t namebuf;

      int ret;

      switch (e->reason & dyld_reason_type_mask)
        {
        case dyld_reason_user:
          reason = "user";
          break;
        case dyld_reason_init:
          reason = "dyld";
          break;
        case dyld_reason_executable:
          reason = "exec";
          break;
        case dyld_reason_dyld:
          reason = "dyld";
          break;
        case dyld_reason_cfm:
          reason = "cfm";
          break;
        default:
          reason = "INVALID";
          break;
        }

      if (e->objfile)
        {
          if (e->loaded_from_memory)
            {
              name = "memory";
            }
          else
            {
              name = e->loaded_name;
            }
        }
      else
        {
          name = dyld_entry_filename (e, d, DYLD_ENTRY_FILENAME_LOADED);
          if (name == NULL)
            {
              if (!(e->reason & dyld_reason_weak_mask))
                {
                  warning ("Unable to resolve \"%s\"; not loading.", name);
                }
              return OBJF_SYM_NONE;
            }
        }

      ret = regcomp (&reasonbuf, matchreason, REG_NOSUB);
      if (ret != 0)
        {
          warning ("unable to compile regular expression \"%s\"",
                   matchreason);
          continue;
        }

      ret = regcomp (&namebuf, matchname, REG_NOSUB);
      if (ret != 0)
        {
          warning ("unable to compile regular expression \"%s\"",
                   matchreason);
          continue;
        }

      ret = regexec (&reasonbuf, reason, 0, 0, 0);
      if (ret != 0)
        continue;

      ret = regexec (&namebuf, name, 0, 0, 0);
      if (ret != 0)
        continue;

      return dyld_parse_load_level (setting);
    }

  return -1;
}

int
dyld_minimal_load_flag (const struct dyld_path_info *d,
                        struct dyld_objfile_entry *e)
{
  int ret = dyld_resolve_load_flag (d, e, dyld_minimal_load_rules);
  return (ret >= 0) ? ret : OBJF_SYM_NONE;
}

int
dyld_default_load_flag (const struct dyld_path_info *d,
                        struct dyld_objfile_entry *e)
{
  int ret = dyld_resolve_load_flag (d, e, dyld_load_rules);
  if (ret >= 0)
    return ret;

  if (e->reason != dyld_reason_cfm)
    {
      if (dyld_load_dyld_shlib_symbols_flag)
        return OBJF_SYM_ALL;
    }
  else
    {
      if (dyld_load_cfm_shlib_symbols_flag)
        return OBJF_SYM_ALL;
    }

  return OBJF_SYM_NONE;
}

void
dyld_load_library_from_file (const struct dyld_path_info *d,
			     struct dyld_objfile_entry *e,
			     int print_errors)
{
  const char *name = NULL;
  struct stat dummy;

  name = dyld_entry_filename (e, d, DYLD_ENTRY_FILENAME_LOADED);
  if (name == NULL)
    {
      if (print_errors)
	{
	  char *s = dyld_entry_string (e, 1);
	  warning ("No image filename available for %s.", s);
	  xfree (s);
	}
      return;
    }

  /* We could just go straight to symfile_bfd_open_safe, but since
     GDB's error-handler resets bfd_errno, it's difficult to tell why
     the call has failed.  So instead, check explicitly if the file
     exists, and avoid printing a warning if a weak file is not
     found.  */

  if (stat (name, &dummy) != 0)
    {
      if ((print_errors) && (! (e->reason & dyld_reason_weak_mask)))
	{
	  char *s = dyld_entry_string (e, 1);
	  warning ("Unable to read symbols for %s (file not found).", s);
	  xfree (s);
	}
      return;
    }

  {
    struct ui_file *prev_stderr = gdb_stderr;

    gdb_stderr = gdb_null;
    CHECK_FATAL (e->abfd == NULL);

    e->abfd = symfile_bfd_open_safe (name, 0);

    gdb_stderr = prev_stderr;

    if (e->abfd == NULL)
      {
	if (print_errors)
	  warning (error_last_message ());
	return;
      }
  }
    
  e->loaded_name = bfd_get_filename (e->abfd);
  e->loaded_from_memory = 0;
  e->loaded_error = 0;
}

void
dyld_load_library_from_memory (const struct dyld_path_info *d,
			       struct dyld_objfile_entry *e,
			       int print_errors)
{
  const char *name = NULL;

  if (!e->dyld_valid)
    {
      if (print_errors)
	{
	  char *s = dyld_entry_string (e, dyld_print_basenames_flag);
	  warning ("Unable to read symbols from %s (not yet mapped into memory).", s);
	  xfree (s);
	}
      return;
    }

  name = dyld_entry_filename (e, d, DYLD_ENTRY_FILENAME_LOADED);

  CHECK_FATAL (e->abfd == NULL);
  e->abfd = inferior_bfd (name, e->dyld_addr, e->dyld_slide, e->dyld_length);
  CHECK_FATAL (e->abfd != NULL);

  e->loaded_memaddr = e->dyld_addr;
  e->loaded_from_memory = 1;
  e->loaded_error = 0;
}

void
dyld_load_library (const struct dyld_path_info *d,
                   struct dyld_objfile_entry *e)
{
  int read_from_memory = 0;
  int print_errors = 1;

  CHECK_FATAL (e->allocated);

  if ((e->abfd != NULL) || (e->objfile != NULL))
    return;

  if (e->reason & dyld_reason_executable_mask)
    CHECK_FATAL (e->objfile == symfile_objfile);

  /* For now, we only print any error messages the first time we try
     to load a bfd.  It would be nice to use a more subtle mechanism
     here, that would avoid repeating the error messages when we retry
     a load, but would print them if we progressed further and hit
     some new error. */

  print_errors = !e->loaded_error;

  /* This would be a good candidate for load-rules similar to those
     for shared library load-levels.  For now, though, just hard-code
     some basic logic. */

  if (e->reason == dyld_reason_cfm)
    read_from_memory = 1;

  if (dyld_always_read_from_memory_flag)
    read_from_memory = 1;

  if (read_from_memory)
    dyld_load_library_from_memory (d, e, print_errors);
  else
    dyld_load_library_from_file (d, e, print_errors);

  /* If we weren't able to load the bfd, there must have been an error
     somewhere.  Flag it, so we don't print error messages the next
     time around (see comment above). */

  if (e->abfd == NULL)
    {
      e->loaded_error = 1;
      return;
    }

  if (e->reason & dyld_reason_image_mask)
    {
      asection *text_sect =
        bfd_get_section_by_name (e->abfd, "LC_SEGMENT.__TEXT");
      if (text_sect != NULL)
        {
          e->image_addr = bfd_section_vma (e->abfd, text_sect);
          e->image_addr_valid = 1;
        }
    }
  
  if (e->reason & dyld_reason_executable_mask)
    symfile_objfile = e->objfile;
}

void
dyld_load_libraries (const struct dyld_path_info *d,
                     struct dyld_objfile_info *result)
{
  int i;
  struct dyld_objfile_entry *e;
  CHECK_FATAL (result != NULL);

  DYLD_ALL_OBJFILE_INFO_ENTRIES (result, e, i)
    {
      if (e->load_flag < 0)
        {
          e->load_flag =
            dyld_default_load_flag (d, e) | dyld_minimal_load_flag (d, e);
        }
      if (e->load_flag)
        {
          dyld_load_library (d, e);
        }
    }
}

void
dyld_symfile_loaded_hook (struct objfile *o)
{
}

void
dyld_load_symfile (struct dyld_objfile_entry *e)
{
  struct section_addr_info *addrs;
  int i;

  if (e->loaded_error)
    return;

  CHECK_FATAL (e->allocated);

  if (e->reason & dyld_reason_executable_mask)
    CHECK_FATAL (e->objfile == symfile_objfile);

  if (e->dyld_valid)
    {
      e->loaded_addr = e->dyld_addr;
      e->loaded_addrisoffset = 0;
    }
  else if (e->image_addr_valid)
    {
      e->loaded_addr = e->image_addr;
      e->loaded_addrisoffset = 0;
    }
  else
    {
      e->loaded_addr = e->dyld_slide;
      e->loaded_addrisoffset = 1;
    }

  if (e->objfile != NULL)
    {
      struct section_offsets *new_offsets =
        (struct section_offsets *)
        xmalloc (SIZEOF_N_SECTION_OFFSETS (e->objfile->num_sections));
      tell_breakpoints_objfile_changed (e->objfile);
      tell_objc_msgsend_cacher_objfile_changed (e->objfile);
      for (i = 0; i < e->objfile->num_sections; i++)
        {
          new_offsets->offsets[i] = e->dyld_slide;
        }
      if (info_verbose)
        printf_filtered ("Relocating symbols from %s...", e->objfile->name);
      gdb_flush (gdb_stdout);
      objfile_relocate (e->objfile, new_offsets);
      xfree (new_offsets);
      if (info_verbose)
        printf_filtered ("done\n");

    }
  else
    {

      CHECK_FATAL (e->abfd != NULL);

      addrs = alloc_section_addr_info (bfd_count_sections (e->abfd));

      for (i = 0; i < addrs->num_sections; i++)
        {
          addrs->other[i].name = NULL;
          addrs->other[i].addr = e->dyld_slide;
          addrs->other[i].sectindex = 0;
        }

      addrs->addrs_are_offsets = 1;

      e->objfile =
        symbol_file_add_bfd_safe (e->abfd, 0, addrs, 0, 0, e->load_flag, 0,
                                  e->prefix);
      e->abfd = NULL;

      if (e->objfile == NULL)
        {
          e->loaded_error = 1;
          return;
        }

      e->loaded_name = e->objfile->name;
      /* CHECK_FATAL (e->objfile->obfd == e->abfd); */


      /* If we are loading the library for the first time, check to see
         if it has a __DATA.__commpage section, and if so, process the
         contents of that section as if it were a separate objfile.
         This objfile will not get relocated along with the parent
         library, which is appropriate since the commpage never moves in
         memory. */

      const char *segname = "LC_SEGMENT.__DATA.__commpage";
      asection *commsec;

      commsec = bfd_get_section_by_name (e->objfile->obfd, segname);
      if (commsec != NULL)
        {
          char *buf;
          bfd_size_type len;
          char *bfdname;

          len = bfd_section_size (e->objfile->obfd, commsec);
          buf = xmalloc (len * sizeof (char));
          bfdname = xmalloc (strlen (e->objfile->obfd->filename) + 128);

          sprintf (bfdname, "%s[%s]", e->objfile->obfd->filename, segname);

          if (bfd_get_section_contents
              (e->objfile->obfd, commsec, buf, 0, len) != TRUE)
            warning ("unable to read commpage data");

          e->commpage_bfd = bfd_memopenr (bfdname, NULL, buf, len);

          if (!bfd_check_format (e->commpage_bfd, bfd_object))
            {
              bfd_close (e->commpage_bfd);
              e->commpage_bfd = NULL;
            }

          if (e->commpage_bfd != NULL)
            e->commpage_objfile =
              symbol_file_add_bfd_safe (e->commpage_bfd, 0, 0, 0, 0,
                                        e->load_flag, 0, e->prefix);

          e->commpage_bfd = NULL;
        }
    }

  dyld_symfile_loaded_hook (e->objfile);

  if (e->reason & dyld_reason_executable_mask)
    {
      CHECK_FATAL ((symfile_objfile == NULL)
                   || (symfile_objfile == e->objfile));
      symfile_objfile = e->objfile;
      return;
    }
}

void
dyld_load_symfiles (struct dyld_objfile_info *result)
{
  int i;
  int first = 1;
  struct dyld_objfile_entry *e;
  CHECK_FATAL (result != NULL);

  DYLD_ALL_OBJFILE_INFO_ENTRIES (result, e, i)
    {
      char load_char;

      if (e->loaded_error)
        continue;
      if (e->abfd == NULL && e->objfile == NULL)
        continue;

      if (e->objfile != NULL)
        {
          if (e->dyld_valid && e->loaded_addr == e->dyld_addr
              && !e->loaded_addrisoffset)
            continue;
          if (e->dyld_valid && e->loaded_addr == e->dyld_slide
              && e->loaded_addrisoffset)
            continue;
          if (e->image_addr_valid && e->loaded_addr == e->image_addr
              && !e->loaded_addrisoffset)
            continue;
          if (!e->dyld_valid && !e->image_addr_valid)
            continue;
        }

      load_char = (e->objfile != NULL) ? '+' : '.';
      if (first && !info_verbose && dyld_print_status ())
        {
          first = 0;
          printf_filtered ("Reading symbols for shared libraries ");
          gdb_flush (gdb_stdout);
        }
      dyld_load_symfile (e);
      if (!info_verbose && dyld_print_status ())
        {
          printf_filtered ("%c", load_char);
          gdb_flush (gdb_stdout);
        }
    }

  if (!first && !info_verbose && dyld_print_status ())
    {
      printf_filtered (" done\n");
      gdb_flush (gdb_stdout);
    }
}

/* Look up the objfile for a given shared library entry.  If no
   objfile is currently allocated, or if the objfile has been removed
   by a lower level of GDB, return NULL. */

struct objfile *
dyld_lookup_objfile_safe (struct dyld_objfile_entry *e)
{
  struct objfile *o, *temp;

  ALL_OBJFILES_SAFE (o, temp)
    if (e->objfile == o)
      return o;

  return NULL;
}

int
dyld_objfile_allocated (struct objfile *o)
{
  struct objfile *objfile, *temp;

  ALL_OBJFILES_SAFE (objfile, temp)
  {
    if (o == objfile)
      {
        return 1;
      }
  }
  return 0;
}

void
dyld_purge_objfiles (struct dyld_objfile_info *info)
{
  struct dyld_objfile_entry *e;
  int i;

  DYLD_ALL_OBJFILE_INFO_ENTRIES (info, e, i)
    if (e->objfile != NULL)
      if (dyld_lookup_objfile_safe (e) == NULL)
        dyld_remove_objfile (e);
}

/* Return the dyld_objfile_entry for a given objfile.  If no
   dyld_objfile_entry claims the specified objfile, return NULL. */

struct dyld_objfile_entry *
dyld_lookup_objfile_entry (struct dyld_objfile_info *info, struct objfile *o)
{
  int i;
  struct dyld_objfile_entry *e;

  DYLD_ALL_OBJFILE_INFO_ENTRIES (info, e, i)
    if (e->objfile == o)
      return e;

  return NULL;
}

/* dyld_should_reload_objfile_for_flags: Checks whether the
   load level of the objfile in the dyld_objfile_entry E needs
   to be reloaded.  We always say a upgrade should be done.  But
   if it is a cached symfile, or dyld_reload_on_downgrade_flag
   is true, then reject downgrades.  */

enum dyld_reload_result
dyld_should_reload_objfile_for_flags (struct dyld_objfile_entry *e)
{
  /* For regular symbol files, reload if there is any difference
     in the requested symbols at all if dyld_reload_on_downgrade_flag
     is set.  Otherwise, only reload on upgrade. */
  if (e->load_flag == e->objfile->symflags)
    return DYLD_NO_CHANGE;
  else if (e->load_flag & ~e->objfile->symflags)
    return DYLD_UPGRADE;
  else
    {
      if (dyld_reload_on_downgrade_flag)
	return DYLD_DOWNGRADE;
      else
	return DYLD_NO_CHANGE;
    }
}

/* Called when an objfile is to be freed.
   If a corresponding dyld_objfile_entry exists, we must free that
   as well so the dyld-level structures and the high-level objfile
   structures stay in sync.

   It would be nice if we got a notification from dyld about a dylib/bundle
   being unloaded and handled that correctly.  But as of today, we're not,
   so Fix and Continue is reduced to doing it by hand.  */

void
remove_objfile_from_dyld_records (struct objfile *obj)
{
  struct dyld_objfile_info *info = &macosx_status->dyld_status.current_info;
  struct dyld_objfile_entry *e;
  int i;

  DYLD_ALL_OBJFILE_INFO_ENTRIES (info, e, i)
    if (e->objfile == obj)
      dyld_remove_objfile (e);
}

/* Returns 1 if the objfile OBJ is actually in memory right now.
   A better name might be "dyld_is_objfile_resident".

   If we the OBJ objfile record exists because of a program's load command,
   or it was loaded when the program was running but the inferior has exited
   (has been target_mourn_inferior'ed), we return 0.  */

int
dyld_is_objfile_loaded (struct objfile *obj)
{
  struct dyld_objfile_entry *e;
  int i;
  struct macosx_dyld_thread_status *status = &macosx_status->dyld_status;

  if (obj == NULL)
    return 0;

  /* A SYMS_ONLY_OBJFILE is an objfile added by the user, either with
     add-symbol-file or "sharedlibrary specify-symbol-file"; it shadows
     an actually loaded & resident objfile, but breakpoints will be
     associated with the syms-only-objfile.  So under the theory that
     users don't add-symbol-file things which haven't been loaded yet,
     set breakpoints in this unconditionally.  */
  if (obj->syms_only_objfile == 1)
    return 1;

  /* Another problem is that since we don't consider anything mapped till
     we get the first dyld notification, we won't set any breakpoints
     in the main executable before dyld runs (notably _start), and we won't
     hit any breakpoints in the early parts of the dyld code either.  
     To work around this, we ALWAYS consider the symfile_objfile loaded, and
     always consider dyld loaded. 
     The reason for this whole exercise was to make sure breakpoints in a
     shared library didn't overwrite the dyld area for the main executable.
     So considering the executable always mapped is fine.
     We are adding the possibility that if somebody builds a dyld based at
     0x0 and then sets a breakpoint early on, they will run into the
     bug again.  But there are a small number of people in the world who
     debug dyld itself, and they can learn to always base dyld somewhere 
     above the executable if they want to avoid this problem.  */

  if (obj == symfile_objfile)
    return 1;

  if (bfd_hash_lookup (&obj->obfd->section_htab, "LC_ID_DYLINKER", 0, 0) != NULL)
    return 1;

  DYLD_ALL_OBJFILE_INFO_ENTRIES (&status->current_info, e, i)
    if (e->objfile == obj || e->commpage_objfile == obj)
      if (e->dyld_name != NULL
          || (e->dyld_addr != 0 && e->dyld_addr != INVALID_ADDRESS))
        return 1;

  return 0;
}

void
dyld_remove_objfile (struct dyld_objfile_entry *e)
{
  char *s = NULL;

  CHECK_FATAL (e->allocated);

  if (e->reason & dyld_reason_executable_mask)
    CHECK_FATAL (e->objfile == symfile_objfile);

  if (e->objfile == NULL)
    {
      return;
    }

  CHECK_FATAL (dyld_objfile_allocated (e->objfile));

  s = dyld_entry_string (e, dyld_print_basenames_flag);
  if (info_verbose)
    {
      printf_filtered ("Removing symbols for %s\n", s);
    }
  xfree (s);
  gdb_flush (gdb_stdout);

  free_objfile (e->objfile);
  e->objfile = NULL;

  if (e->abfd != NULL)
    bfd_close (e->abfd);
  e->abfd = NULL;

  if (e->commpage_objfile != NULL)
    {
      /* The commpage objfile is read when symbols for the main library
         are ready for the first time; it needs to be freed along with
         the main objfile. */

      free_objfile (e->commpage_objfile);
      e->commpage_objfile = NULL;

      if (e->commpage_bfd != NULL)
        bfd_close (e->commpage_bfd);
      e->commpage_bfd = NULL;
    }

  e->loaded_name = NULL;
  e->loaded_memaddr = 0;
  gdb_flush (gdb_stdout);

  if (e->reason & dyld_reason_executable_mask)
    {
      symfile_objfile = e->objfile;
    }
}

void
dyld_remove_objfiles (const struct dyld_path_info *d,
                      struct dyld_objfile_info *result)
{
  int i;
  int first = 1;
  struct dyld_objfile_entry *e;
  CHECK_FATAL (result != NULL);

  DYLD_ALL_OBJFILE_INFO_ENTRIES (result, e, i)
    {
      int should_reload = 0;

      if (e->load_flag < 0)
        {
          e->load_flag =
            dyld_default_load_flag (d, e) | dyld_minimal_load_flag (d, e);
        }

      if (e->reason & dyld_reason_executable_mask)
        CHECK_FATAL (e->objfile == symfile_objfile);

      if (e->objfile != NULL)
        {
          if (e->user_name != NULL
              && strcmp (e->user_name, e->objfile->name) != 0)
            should_reload = 1;

          if (dyld_should_reload_objfile_for_flags (e))
            should_reload = 1;
        }

      if (should_reload)
        {
          dyld_remove_objfile (e);
          if (first && !info_verbose && dyld_print_status ())
            {
              first = 0;
              printf_filtered ("Removing symbols for unused shared libraries ");
              gdb_flush (gdb_stdout);
            }
          if (!info_verbose && dyld_print_status ())
            {
              printf_filtered (".");
              gdb_flush (gdb_stdout);
            }
        }
    }
  if (!first && !info_verbose && dyld_print_status ())
    {
      printf_filtered (" done\n");
      gdb_flush (gdb_stdout);
    }
}

static int
dyld_libraries_similar (struct dyld_path_info *d,
			struct dyld_objfile_entry *f,
			struct dyld_objfile_entry *l)
{
  const char *fname = NULL;
  const char *lname = NULL;

  const char *fbase = NULL;
  const char *lbase = NULL;
  int flen = 0;
  int llen = 0;

  CHECK_FATAL (f != NULL);
  CHECK_FATAL (l != NULL);

  if (library_offset (f) != 0 && library_offset (l) != 0)
    {
      return (library_offset (f) == library_offset (l));
    }

  fname = dyld_entry_filename (f, d, DYLD_ENTRY_FILENAME_LOADED);
  lname = dyld_entry_filename (l, d, DYLD_ENTRY_FILENAME_LOADED);

  if (lname != NULL && fname != NULL)
    {

      int f_is_framework, f_is_bundle;
      int l_is_framework, l_is_bundle;

      dyld_library_basename (fname, &fbase, &flen, &f_is_framework,
                             &f_is_bundle);
      dyld_library_basename (lname, &lbase, &llen, &l_is_framework,
                             &l_is_bundle);

      if (flen != llen || strncmp (fbase, lbase, llen) != 0)
        {
          xfree ((char *) fbase);
          xfree ((char *) lbase);
          return 0;
        }

      xfree ((char *) fbase);
      xfree ((char *) lbase);

      if (f_is_framework != l_is_framework)
        return 0;

      if (f_is_bundle != l_is_bundle)
        return 0;

      return 1;
    }

  return 0;
}

/* Do dyld_objfile_entry OLDENT and NEWENT have the same filename?  In
   other words, are they the same dylib/bundle/executable/etc ?  */

int
dyld_libraries_compatible (struct dyld_path_info *d,
                           struct dyld_objfile_entry *newent,
                           struct dyld_objfile_entry *oldent)
{
  const char *newname = NULL;
  const char *oldname = NULL;

  CHECK_FATAL (oldent != NULL);
  CHECK_FATAL (newent != NULL);

  /* If either prefix is non-NULL, then they must both be the same string. */

  if (oldent->prefix != NULL || newent->prefix != NULL)
    {
      if (oldent->prefix == NULL || newent->prefix == NULL)
        return 0;
      if (strcmp (oldent->prefix, newent->prefix) != 0)
        return 0;
    }

  newname = dyld_entry_filename (newent, d, DYLD_ENTRY_FILENAME_LOADED);
  oldname = dyld_entry_filename (oldent, d, DYLD_ENTRY_FILENAME_LOADED);

  /* If we've already loaded the objfile from memory, and from the
     same address, then we can go ahead and re-use it.

     FIXME: What if dyld has moved libraries around, or plug-ins have
     been unloaded and re-loaded for whatever reason, and we now have
     some other library loaded at this address?  We should probably
     store some token in the loaded_* information to provide for more
     reliable matching.
  */

  if ((oldent->loaded_from_memory) && (oldent->loaded_addr == newent->dyld_addr))
    {
      gdb_assert (dyld_libraries_similar (d, newent, oldent));
      return 1;
    }

  /* If either filename is non-NULL, then they must both be the same string. */

  if (oldname != NULL || newname != NULL)
    {
      if (oldname == NULL || newname == NULL)
        return 0;
      if (strcmp (oldname, newname) != 0)
        return 0;
    }

  if (dyld_always_read_from_memory_flag)
    {
      if (oldent->loaded_from_memory != newent->loaded_from_memory)
        {
          return 0;
        }
    }

  /* The same bundle can be loaded more than once under certain
     circumstances.  Both dyld_objfile_entries will be dyld_reason_dyld,
     both will have the same filename, but they'll have different dyld_addr's
     (different load addresses).
     It's not entirely clear to me whether this is technically legal, but
     it happens in real world use.  cf <rdar://problem/4308315> 

     When this comes up, we should say that the two libraries are not
     "compatible".  This is not an error condition.  Yes, that means
     there will be two dyld_objfile_entry's with the same filename and
     two struct objfile's with the same filename, but that's how we roll.  */
 
  return dyld_libraries_similar (d, newent, oldent);
}

/* Move the load data (whatever distinction that is -- not all the
   fields are moved) from the SRC dyld_objfile_entry into DEST.
   Upon completion, SRC won't have any of its load data fields set.  */

void
dyld_objfile_move_load_data (struct dyld_objfile_entry *src,
                             struct dyld_objfile_entry *dest)
{
  gdb_assert (dest->abfd == NULL);
  gdb_assert (dest->commpage_bfd == NULL);

  gdb_assert (src->objfile == dest->objfile || dest->objfile == NULL);
  gdb_assert (dest->commpage_objfile == NULL);

  dest->abfd = src->abfd;
  dest->objfile = src->objfile;

  dest->commpage_bfd = src->commpage_bfd;
  dest->commpage_objfile = src->commpage_objfile;

  /* If we are re-running, and haven't resolved the new load data
     flags, go ahead and pick them up from the previous run. */

  if ((src->load_flag > 0) && (dest->load_flag < 0))
    {
      dest->load_flag = src->load_flag;
    }

  dest->prefix = src->prefix;
  dest->loaded_name = src->loaded_name;
  dest->loaded_memaddr = src->loaded_memaddr;
  dest->loaded_addr = src->loaded_addr;
  dest->loaded_offset = src->loaded_offset;
  dest->loaded_addrisoffset = src->loaded_addrisoffset;
  dest->loaded_from_memory = src->loaded_from_memory;
  dest->loaded_error = src->loaded_error;

  src->objfile = NULL;
  src->abfd = NULL;

  src->commpage_bfd = NULL;
  src->commpage_objfile = NULL;

  src->load_flag = -1;

  src->loaded_name = NULL;
  src->loaded_memaddr = 0;
  src->loaded_addr = 0;
  src->loaded_offset = 0;
  src->loaded_addrisoffset = 0;
  src->loaded_from_memory = 0;
  src->loaded_error = 0;

  dyld_objfile_entry_clear (src);
}

void
dyld_check_discarded (struct dyld_objfile_info *info)
{
  int j;
  for (j = 0; j < info->nents; j++)
    {
      struct dyld_objfile_entry *e = &info->entries[j];
      if (e->abfd == NULL && e->objfile == NULL && !e->loaded_error)
        {
          dyld_objfile_entry_clear (e);
        }
    }
}

/* We're adding NEWENT to the list of dylib/bundle/etcs loaded in the
   inferior in a little while.  Look through the existing entries
   in OLDINFOS and see if we had one for this dylib/bundle/etc
   already.  If so, copy over the load data into NEWENT and clear
   them from the old entry in OLDINFOS.  (I think the old entry is
   marked as obsolete over in the purge_shlib function or something.)  */

void
dyld_merge_shlib (const struct macosx_dyld_thread_status *s,
                  struct dyld_path_info *d,
                  struct dyld_objfile_info *oldinfos,
                  struct dyld_objfile_entry *newent)
{
  int i;
  struct dyld_objfile_entry *oldent;

  DYLD_ALL_OBJFILE_INFO_ENTRIES (oldinfos, oldent, i)
    if (dyld_libraries_compatible (d, newent, oldent))
    {
      dyld_objfile_move_load_data (oldent, newent);
      if (newent->reason & dyld_reason_executable_mask)
        symfile_objfile = newent->objfile;
      return;
    }

  DYLD_ALL_OBJFILE_INFO_ENTRIES (oldinfos, oldent, i)
    if ((newent->reason & dyld_reason_image_mask)
        && dyld_libraries_similar (d, newent, oldent) && oldent->objfile != NULL)
    {
      dyld_objfile_move_load_data (oldent, newent);
      if (newent->reason & dyld_reason_executable_mask)
        symfile_objfile = newent->objfile;
      return;
    }
}

/* Go through all the dyld_objfile_entry's in OBJINFO, looking for
   other entries that are for the same file image as NEW.

   When gdb starts with an app before launching, it creates
   dyld_objfile_entry's for all the dylibs it finds in the app's
   load commands.  Then when the app executes, we get notifications
   from dyld when the dylibs/bundles/etc actually load.  So at that
   point we want to toss the pre-execution speculative dyld_objfile_entry
   and standardize on the actually-seen image file.

   That's one instance where we'll be using this function.  */

void
dyld_prune_shlib (struct dyld_path_info *d,
		  struct dyld_objfile_info *obj_info,
                  struct dyld_objfile_entry *new)
{
  struct dyld_objfile_entry *o;
  int i;

  DYLD_ALL_OBJFILE_INFO_ENTRIES (obj_info, o, i)
    {
      if ((o->reason & dyld_reason_executable_mask)
          && (new->reason & dyld_reason_executable_mask))
        {
          if (o->objfile != NULL && o->objfile != new->objfile)
            {
              tell_breakpoints_objfile_changed (o->objfile);
              tell_objc_msgsend_cacher_objfile_changed (o->objfile);
            }
          dyld_objfile_entry_clear (o);
          continue;
        }

      if (dyld_libraries_similar (d, o, new))
        {
          if (o->objfile != NULL)
            {
              tell_breakpoints_objfile_changed (o->objfile);
              tell_objc_msgsend_cacher_objfile_changed (o->objfile);
            }
          dyld_remove_objfile (o);
          dyld_objfile_entry_clear (o);
        }
    }
}

void
dyld_merge_shlibs (const struct macosx_dyld_thread_status *s,
                   struct dyld_path_info *d,
                   struct dyld_objfile_info *old,
                   struct dyld_objfile_info *new)
{
  struct dyld_objfile_entry *n = NULL;
  struct dyld_objfile_entry *o = NULL;
  int i;

  CHECK_FATAL (old != NULL);
  CHECK_FATAL (new != NULL);
  CHECK_FATAL (old != new);

  dyld_resolve_filenames (s, new);

  DYLD_ALL_OBJFILE_INFO_ENTRIES (new, n, i)
    if (n->objfile == NULL)
      dyld_merge_shlib (s, d, old, n);

  DYLD_ALL_OBJFILE_INFO_ENTRIES (new, n, i)
    dyld_prune_shlib (d, old, n);

  DYLD_ALL_OBJFILE_INFO_ENTRIES (old, o, i)
    {
      struct dyld_objfile_entry *e = NULL;
      e = dyld_objfile_entry_alloc (new);
      *e = *o;

      e->reason |= dyld_reason_cached_mask;

      dyld_objfile_entry_clear (o);
    }
}

static void
dyld_shlibs_updated (struct dyld_objfile_info *info)
{
  dyld_objfile_info_pack (info);
  update_section_tables_dyld (info);
  update_current_target ();
  reread_symbols ();
  breakpoint_update ();
  re_enable_breakpoints_in_shlibs (0);
}

void
dyld_update_shlibs (struct dyld_path_info *d, struct dyld_objfile_info *result)
{
  CHECK_FATAL (result != NULL);

  dyld_debug ("dyld_update_shlibs: updating shared library information\n");

  dyld_remove_objfiles (d, result);
  dyld_load_libraries (d, result);
  dyld_load_symfiles (result);

  dyld_shlibs_updated (result);
}

void
dyld_purge_cached_libraries (struct dyld_objfile_info *info)
{
  int i;
  struct dyld_objfile_entry *e;
  CHECK_FATAL (info != NULL);

  DYLD_ALL_OBJFILE_INFO_ENTRIES (info, e, i)
    if (e->reason & dyld_reason_cached_mask)
      {
        dyld_remove_objfile (e);
        dyld_objfile_entry_clear (e);
      }

  dyld_shlibs_updated (info);
}

void
_initialize_macosx_nat_dyld_process ()
{
}
