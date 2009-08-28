/* Work with executable files, for GDB. 

   Copyright 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996,
   1997, 1998, 1999, 2000, 2001, 2002, 2003 Free Software Foundation,
   Inc.

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

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "target.h"
#include "gdbcmd.h"
#include "language.h"
#include "symfile.h"
#include "objfiles.h"
#include "completer.h"
#include "value.h"
#include "exec.h"
#include "observer.h"
/* APPLE LOCAL exceptions.h */
#include "exceptions.h"
/* APPLE LOCAL osabi.h */
#include "osabi.h"

#include <fcntl.h>
#include "readline/readline.h"
#include "gdb_string.h"

#include "gdbcore.h"

#include <ctype.h>
#include "gdb_stat.h"
#ifndef O_BINARY
#define O_BINARY 0
#endif

#include "xcoffsolib.h"

/* APPLE LOCAL begin dyld */
#ifdef MACOSX_DYLD
#include "macosx-nat-dyld.h"
#endif
/* APPLE LOCAL end dyld */

/* APPLE LOCAL begin section names */
#ifndef TEXT_SECTION_NAME
#define TEXT_SECTION_NAME ".text"
#endif

#ifndef DATA_SECTION_NAME
#define DATA_SECTION_NAME ".data"
#endif
/* APPLE LOCAL end section names */

/* APPLE LOCAL begin async */
static void
standard_async (void (*callback) (enum inferior_event_type event_type, 
				  void *context), void *context)
{
  return;
}

int standard_is_async_p (void)
{
  return (current_target.to_async_mask_value);
}

int standard_can_async_p (void)
{
  return (current_target.to_async_mask_value);
}
/* APPLE LOCAL end async */

struct vmap *map_vmap (bfd *, bfd *);

void (*deprecated_file_changed_hook) (char *);

/* Prototypes for local functions */

static void exec_close (int);

static void file_command (char *, int);

static void set_section_command (char *, int);

static void exec_files_info (struct target_ops *);

static int ignore (CORE_ADDR, bfd_byte *);

/* APPLE LOCAL make globally visible */
void init_exec_ops (void);

void _initialize_exec (void);

/* The target vector for executable files.  */

struct target_ops exec_ops;

/* The Binary File Descriptor handle for the executable file.  */

bfd *exec_bfd = NULL;

/* Whether to open exec and core files read-only or read-write.  */

int write_files = 0;
static void
show_write_files (struct ui_file *file, int from_tty,
		  struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Writing into executable and core files is %s.\n"),
		    value);
}


struct vmap *vmap;

void
exec_open (char *args, int from_tty)
{
  target_preopen (from_tty);
  exec_file_attach (args, from_tty);
}

static void
exec_close (int quitting)
{
  int need_symtab_cleanup = 0;
  struct vmap *vp, *nxt;

  for (nxt = vmap; nxt != NULL;)
    {
      vp = nxt;
      nxt = vp->nxt;

      /* if there is an objfile associated with this bfd,
         free_objfile() will do proper cleanup of objfile *and* bfd. */

      if (vp->objfile)
	{
	  free_objfile (vp->objfile);
	  need_symtab_cleanup = 1;
	}
      else if (vp->bfd != exec_bfd)
	/* FIXME-leak: We should be freeing vp->name too, I think.  */
	if (!bfd_close (vp->bfd))
	  warning (_("cannot close \"%s\": %s"),
		   vp->name, bfd_errmsg (bfd_get_error ()));

      /* FIXME: This routine is #if 0'd in symfile.c.  What should we
         be doing here?  Should we just free everything in
         vp->objfile->symtabs?  Should free_objfile do that?
         FIXME-as-well: free_objfile already free'd vp->name, so it isn't
         valid here.  */
      free_named_symtabs (vp->name);
      xfree (vp);
    }

  vmap = NULL;

  if (exec_bfd)
    {
      char *name = bfd_get_filename (exec_bfd);

      if (!bfd_close (exec_bfd))
	warning (_("cannot close \"%s\": %s"),
		 name, bfd_errmsg (bfd_get_error ()));
      xfree (name);
      exec_bfd = NULL;
    }

  if (exec_ops.to_sections)
    {
      /* APPLE LOCAL section resize */
      target_resize_to_sections
	(&exec_ops, exec_ops.to_sections_end - exec_ops.to_sections);
    }
}

void
exec_file_clear (int from_tty)
{
  /* Remove exec file.  */
  unpush_target (&exec_ops);

  if (from_tty)
    printf_unfiltered (_("No executable file now.\n"));
}

#ifdef SOLIB_ADD
/* Stub function for catch_errors around shared library hacking.  FROM_TTYP
   is really an int * which points to from_tty.  */

static int
solib_add_stub (PTR from_ttyp)
{
  SOLIB_ADD (NULL, *(int *) from_ttyp, &current_target, auto_solib_add);
  re_enable_breakpoints_in_shlibs (0);
  return 0;
}
#endif /* SOLIB_ADD */

/*  Process the first arg in ARGS as the new exec file.

   This function is intended to be behave essentially the same
   as exec_file_command, except that the latter will detect when
   a target is being debugged, and will ask the user whether it
   should be shut down first.  (If the answer is "no", then the
   new file is ignored.)

   This file is used by exec_file_command, to do the work of opening
   and processing the exec file after any prompting has happened.

   And, it is used by child_attach, when the attach command was
   given a pid but not a exec pathname, and the attach command could
   figure out the pathname from the pid.  (In this case, we shouldn't
   ask the user whether the current target should be shut down --
   we're supplying the exec pathname late for good reason.)
   
   ARGS is assumed to be the filename. */

void
exec_file_attach (char *filename, int from_tty)
{
  /* Remove any previous exec file.  */
  unpush_target (&exec_ops);

  /* Now open and digest the file the user requested, if any.  */

  if (!filename)
    {
      if (from_tty)
        printf_unfiltered (_("No executable file now.\n"));
    }
  else
    {
      char *scratch_pathname;
      int scratch_chan;

      scratch_chan = openp (getenv ("PATH"), OPF_TRY_CWD_FIRST, filename,
		   write_files ? O_RDWR | O_BINARY : O_RDONLY | O_BINARY, 0,
			    &scratch_pathname);
#if defined(__GO32__) || defined(_WIN32) || defined(__CYGWIN__)
      if (scratch_chan < 0)
	{
	  char *exename = alloca (strlen (filename) + 5);
	  strcat (strcpy (exename, filename), ".exe");
	  scratch_chan = openp (getenv ("PATH"), OPF_TRY_CWD_FIRST, exename,
	     write_files ? O_RDWR | O_BINARY : O_RDONLY | O_BINARY, 0,
	     &scratch_pathname);
	}
#endif

      /* APPLE LOCAL begin app bundles */
#ifdef TM_NEXTSTEP
      if (scratch_chan < 0)
	{
	  /* APPLE LOCAL: Look for a wrapped executable of the form
	     Foo.app/Contents/MacOS/Foo, where the user gave us up to
	     Foo.app.  The ".app" is optional. */

	  char *wrapped_filename = macosx_filename_in_bundle (filename, 1);

	  if (wrapped_filename != NULL)
	    {
	      scratch_chan = openp (getenv ("PATH"), 1, wrapped_filename,
				    write_files ? O_RDWR | O_BINARY : O_RDONLY | O_BINARY, 0,
				    &scratch_pathname);
	      xfree (wrapped_filename);
	    }
	}
#endif
      /* APPLE LOCAL end app bundles */

      if (scratch_chan < 0)
	perror_with_name (filename);
      exec_bfd = bfd_fopen (scratch_pathname, gnutarget,
			    write_files ? FOPEN_RUB : FOPEN_RB,
			    scratch_chan);

      if (!exec_bfd)
	error (_("\"%s\": could not open as an executable file: %s"),
	       scratch_pathname, bfd_errmsg (bfd_get_error ()));

      /* At this point, scratch_pathname and exec_bfd->name both point to the
         same malloc'd string.  However exec_close() will attempt to free it
         via the exec_bfd->name pointer, so we need to make another copy and
         leave exec_bfd as the new owner of the original copy. */
      scratch_pathname = xstrdup (scratch_pathname);
      make_cleanup (xfree, scratch_pathname);

      /* APPLE LOCAL begin fat binaries */
      /* If the file is an archive file (i.e. fat binary), look for
	 sub-files that match the current osabi. */

      if (bfd_check_format (exec_bfd, bfd_archive))
	{
	  bfd *tmp_bfd;
	  tmp_bfd = open_bfd_matching_arch (exec_bfd, bfd_object);
	  if (tmp_bfd != NULL)
	    exec_bfd = tmp_bfd;
	}
      /* APPLE LOCAL end fat binaries */
      
      if (!bfd_check_format (exec_bfd, bfd_object))
	{
	  /* Make sure to close exec_bfd, or else "run" might try to use
	     it.  */
	  exec_close (0);
	  error (_("\"%s\": not in executable format: %s"),
		 scratch_pathname, bfd_errmsg (bfd_get_error ()));
	}

      /* FIXME - This should only be run for RS6000, but the ifdef is a poor
         way to accomplish.  */
#ifdef DEPRECATED_IBM6000_TARGET
      /* Setup initial vmap. */

      map_vmap (exec_bfd, 0);
      if (vmap == NULL)
	{
	  /* Make sure to close exec_bfd, or else "run" might try to use
	     it.  */
	  exec_close (0);
	  error (_("\"%s\": can't find the file sections: %s"),
		 scratch_pathname, bfd_errmsg (bfd_get_error ()));
	}
#endif /* DEPRECATED_IBM6000_TARGET */

      if (build_section_table (exec_bfd, &exec_ops.to_sections,
			       &exec_ops.to_sections_end))
	{
	  /* Make sure to close exec_bfd, or else "run" might try to use
	     it.  */
	  exec_close (0);
	  error (_("\"%s\": can't find the file sections: %s"),
		 scratch_pathname, bfd_errmsg (bfd_get_error ()));
	}

#ifdef DEPRECATED_HPUX_TEXT_END
      DEPRECATED_HPUX_TEXT_END (&exec_ops);
#endif

      validate_files ();

      set_gdbarch_from_file (exec_bfd);

      push_target (&exec_ops);

#ifdef MACOSX_DYLD
      macosx_init_dyld_symfile (symfile_objfile, exec_bfd);
#endif

#ifdef SOLIB_ADD
      catch_errors (solib_add_stub, &from_tty, (char *) 0,
		    RETURN_MASK_ALL);
#endif
  
      /* Tell display code (if any) about the changed file name.  */
      if (deprecated_exec_file_display_hook)
	(*deprecated_exec_file_display_hook) (filename);
    }
  bfd_cache_close_all ();
  observer_notify_executable_changed (NULL);
}

/*  Process the first arg in ARGS as the new exec file.

   Note that we have to explicitly ignore additional args, since we can
   be called from file_command(), which also calls symbol_file_command()
   which can take multiple args.
   
   If ARGS is NULL, we just want to close the exec file. */

static void
exec_file_command (char *args, int from_tty)
{
  char **argv;
  char *filename;
  
  target_preopen (from_tty);

  if (args)
    {
      /* Scan through the args and pick up the first non option arg
         as the filename.  */

      argv = buildargv (args);
      if (argv == NULL)
        nomem (0);

      make_cleanup_freeargv (argv);

      for (; (*argv != NULL) && (**argv == '-'); argv++)
        {;
        }
      if (*argv == NULL)
        error (_("No executable file name was specified"));

      filename = tilde_expand (*argv);
      make_cleanup (xfree, filename);
      exec_file_attach (filename, from_tty);
    }
  else
    exec_file_attach (NULL, from_tty);
}

/* Set both the exec file and the symbol file, in one command.  
   What a novelty.  Why did GDB go through four major releases before this
   command was added?  */

static void
file_command (char *arg, int from_tty)
{
  /* FIXME, if we lose on reading the symbol file, we should revert
     the exec file, but that's rough.  */
  exec_file_command (arg, from_tty);
  symbol_file_command (arg, from_tty);
  if (deprecated_file_changed_hook)
    deprecated_file_changed_hook (arg);
}


/* Locate all mappable sections of a BFD file. 
   table_pp_char is a char * to get it through bfd_map_over_sections;
   we cast it back to its proper type.  */

static void
add_to_section_table (bfd *abfd, struct bfd_section *asect,
		      void *table_pp_char)
{
  struct section_table **table_pp = (struct section_table **) table_pp_char;
  flagword aflag;

  aflag = bfd_get_section_flags (abfd, asect);
  if (!(aflag & SEC_ALLOC))
    return;
  if (0 == bfd_section_size (abfd, asect))
    return;
  (*table_pp)->bfd = abfd;
  (*table_pp)->the_bfd_section = asect;
  (*table_pp)->addr = bfd_section_vma (abfd, asect);
  (*table_pp)->endaddr = (*table_pp)->addr + bfd_section_size (abfd, asect);
  (*table_pp)++;
}

/* Builds a section table, given args BFD, SECTABLE_PTR, SECEND_PTR.
   Returns 0 if OK, 1 on error.  */

int
build_section_table (struct bfd *some_bfd, struct section_table **start,
		     struct section_table **end)
{
  unsigned count;

  count = bfd_count_sections (some_bfd);
  if (*start)
    xfree (* start);
  *start = (struct section_table *) xmalloc (count * sizeof (**start));
  *end = *start;
  bfd_map_over_sections (some_bfd, add_to_section_table, (char *) end);
  if (*end > *start + count)
    internal_error (__FILE__, __LINE__, _("failed internal consistency check"));
  /* We could realloc the table, but it probably loses for most files.  */
  return 0;
}

static void
bfdsec_to_vmap (struct bfd *abfd, struct bfd_section *sect, void *arg3)
{
  struct vmap_and_bfd *vmap_bfd = (struct vmap_and_bfd *) arg3;
  struct vmap *vp;

  vp = vmap_bfd->pvmap;

  if ((bfd_get_section_flags (abfd, sect) & SEC_LOAD) == 0)
    return;

  if (strcmp (bfd_section_name (abfd, sect), TEXT_SECTION_NAME) == 0)
    {
      vp->tstart = bfd_section_vma (abfd, sect);
      vp->tend = vp->tstart + bfd_section_size (abfd, sect);
      vp->tvma = bfd_section_vma (abfd, sect);
      vp->toffs = sect->filepos;
    }
  else if (strcmp (bfd_section_name (abfd, sect), DATA_SECTION_NAME) == 0)
    {
      vp->dstart = bfd_section_vma (abfd, sect);
      vp->dend = vp->dstart + bfd_section_size (abfd, sect);
      vp->dvma = bfd_section_vma (abfd, sect);
    }
  /* Silently ignore other types of sections. (FIXME?)  */
}

/* Make a vmap for ABFD which might be a member of the archive ARCH.
   Return the new vmap.  */

struct vmap *
map_vmap (bfd *abfd, bfd *arch)
{
  struct vmap_and_bfd vmap_bfd;
  struct vmap *vp, **vpp;

  vp = (struct vmap *) xmalloc (sizeof (*vp));
  memset ((char *) vp, '\0', sizeof (*vp));
  vp->nxt = 0;
  vp->bfd = abfd;
  vp->name = bfd_get_filename (arch ? arch : abfd);
  vp->member = arch ? bfd_get_filename (abfd) : "";

  vmap_bfd.pbfd = arch;
  vmap_bfd.pvmap = vp;
  bfd_map_over_sections (abfd, bfdsec_to_vmap, &vmap_bfd);

  /* Find the end of the list and append. */
  for (vpp = &vmap; *vpp; vpp = &(*vpp)->nxt)
    ;
  *vpp = vp;

  return vp;
}

/* Read or write the exec file.

   Args are address within a BFD file, address within gdb address-space,
   length, and a flag indicating whether to read or write.

   Result is a length:

   0:    We cannot handle this address and length.
   > 0:  We have handled N bytes starting at this address.
   (If N == length, we did it all.)  We might be able
   to handle more bytes beyond this length, but no
   promises.
   < 0:  We cannot handle this address, but if somebody
   else handles (-N) bytes, we can start from there.

   The same routine is used to handle both core and exec files;
   we just tail-call it with more arguments to select between them.  */

int
xfer_memory (CORE_ADDR memaddr, gdb_byte *myaddr, int len, int write,
	     struct mem_attrib *attrib, struct target_ops *target)
{
  int res;
  struct section_table *p;
  CORE_ADDR nextsectaddr, memend;
  asection *section = NULL;

  if (len <= 0)
    internal_error (__FILE__, __LINE__, _("failed internal consistency check"));

  if (overlay_debugging)
    {
      section = find_pc_overlay (memaddr);
      if (pc_in_unmapped_range (memaddr, section))
	memaddr = overlay_mapped_address (memaddr, section);
    }

  memend = memaddr + len;
  nextsectaddr = memend;

  for (p = target->to_sections; p < target->to_sections_end; p++)
    {
      if (overlay_debugging && section && p->the_bfd_section &&
	  strcmp (section->name, p->the_bfd_section->name) != 0)
	continue;		/* not the section we need */
      if (memaddr >= p->addr)
        {
	  if (memend <= p->endaddr)
	    {
	      /* Entire transfer is within this section.  */
	      if (write)
		res = bfd_set_section_contents (p->bfd, p->the_bfd_section,
						myaddr, memaddr - p->addr,
						len);
	      else
		res = bfd_get_section_contents (p->bfd, p->the_bfd_section,
						myaddr, memaddr - p->addr,
						len);
	      return (res != 0) ? len : 0;
	    }
	  else if (memaddr >= p->endaddr)
	    {
	      /* This section ends before the transfer starts.  */
	      continue;
	    }
	  else
	    {
	      /* This section overlaps the transfer.  Just do half.  */
	      len = p->endaddr - memaddr;
	      if (write)
		res = bfd_set_section_contents (p->bfd, p->the_bfd_section,
						myaddr, memaddr - p->addr,
						len);
	      else
		res = bfd_get_section_contents (p->bfd, p->the_bfd_section,
						myaddr, memaddr - p->addr,
						len);
	      return (res != 0) ? len : 0;
	    }
        }
      else
	nextsectaddr = min (nextsectaddr, p->addr);
    }

  if (nextsectaddr >= memend)
    return 0;			/* We can't help */
  else
    return -(nextsectaddr - memaddr);	/* Next boundary where we can help */
}


void
print_section_info_objfile (struct objfile *o)
{
  struct obj_section *p;
  struct cleanup *info_cleanup, *section_cleanup;

  info_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "section-info");
  ui_out_text (uiout, "\t");
  ui_out_field_string (uiout, "filename", bfd_get_filename (o->obfd));
  ui_out_text (uiout, ", ");
  ui_out_wrap_hint (uiout, "        ");
  ui_out_text (uiout, "file type ");
  ui_out_field_string (uiout, "filetype", bfd_get_target (o->obfd));
  ui_out_text (uiout, ", ");
  ui_out_wrap_hint (uiout, "        ");
  ui_out_text (uiout, "symbol prefix ");
  ui_out_field_string (uiout, "prefix", o->prefix);
  ui_out_text (uiout, ".\n");
  make_cleanup_ui_out_list_begin_end (uiout, "sections");
  for (p = o->sections; p < o->sections_end; p++)
    {
      section_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "section");
      ui_out_text (uiout, "\t");
      ui_out_field_core_addr (uiout, "addr", p->addr);
      ui_out_text (uiout, " - ");
      ui_out_field_core_addr (uiout, "endaddr", p->endaddr);
      if (info_verbose)
	{
	  ui_out_text (uiout, " @ ");
	  ui_out_field_core_addr (uiout, "filepos", p->the_bfd_section->filepos);
	}
      ui_out_text (uiout, " is ");
      ui_out_field_string (uiout, "name", bfd_section_name (p->bfd, p->the_bfd_section));
      ui_out_text (uiout, "\n");
      do_cleanups (section_cleanup); /* "section" */
    }
  do_cleanups(info_cleanup); /* "sections" and "section-info" */
}

void
print_section_info (struct target_ops *t, bfd *abfd)
{
  struct section_table *p;
  struct cleanup *info_cleanup, *section_cleanup;

  info_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "section-info");
  ui_out_text (uiout, "\t");
  ui_out_field_string (uiout, "filename", bfd_get_filename (abfd));
  ui_out_text (uiout, ", ");
  ui_out_wrap_hint (uiout, "        ");
  ui_out_text (uiout, "file type ");
  ui_out_field_string (uiout, "filetype", bfd_get_target (abfd));
  ui_out_text (uiout, ".\n");
  if (abfd == exec_bfd)
    {
      ui_out_text (uiout, "\tEntry point: ");
      ui_out_field_core_addr (uiout, "entry-point", bfd_get_start_address (abfd));
      ui_out_text (uiout, "\n");
    }
  make_cleanup_ui_out_list_begin_end (uiout, "sections");
  for (p = t->to_sections; p < t->to_sections_end; p++)
    {
      section_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "section");
      ui_out_text (uiout, "\t");
      ui_out_field_core_addr (uiout, "addr", p->addr);
      ui_out_text (uiout, " - ");
      ui_out_field_core_addr (uiout, "endaddr", p->endaddr);
      if (info_verbose)
	{
	  ui_out_text (uiout, " @ ");
	  ui_out_field_core_addr (uiout, "filepos", p->the_bfd_section->filepos);
	}
      ui_out_text (uiout, " is ");
      ui_out_field_string (uiout, "name", bfd_section_name (p->bfd, p->the_bfd_section));
      if (p->bfd != abfd)
	{
	  ui_out_text (uiout, " in ");
	  ui_out_field_string (uiout, "filename", bfd_get_filename (p->bfd));
	}
      ui_out_text (uiout, "\n");
      do_cleanups (section_cleanup); /* "section" */
    }
  do_cleanups (info_cleanup); /* "sections" & "section-info" */
}

static void
exec_files_info (struct target_ops *t)
{
  print_section_info (t, exec_bfd);

  if (vmap)
    {
      struct vmap *vp;

      printf_unfiltered (_("\tMapping info for file `%s'.\n"), vmap->name);
      printf_unfiltered ("\t  %*s   %*s   %*s   %*s %8.8s %s\n",
			 strlen_paddr (), "tstart",
			 strlen_paddr (), "tend",
			 strlen_paddr (), "dstart",
			 strlen_paddr (), "dend",
			 "section",
			 "file(member)");

      for (vp = vmap; vp; vp = vp->nxt)
	printf_unfiltered ("\t0x%s 0x%s 0x%s 0x%s %s%s%s%s\n",
			   paddr (vp->tstart),
			   paddr (vp->tend),
			   paddr (vp->dstart),
			   paddr (vp->dend),
			   vp->name,
			   *vp->member ? "(" : "", vp->member,
			   *vp->member ? ")" : "");
    }
}

/* msnyder 5/21/99:
   exec_set_section_offsets sets the offsets of all the sections
   in the exec objfile.  */

void
exec_set_section_offsets (bfd_signed_vma text_off, bfd_signed_vma data_off,
			  bfd_signed_vma bss_off)
{
  struct section_table *sect;

  for (sect = exec_ops.to_sections;
       sect < exec_ops.to_sections_end;
       sect++)
    {
      flagword flags;

      flags = bfd_get_section_flags (exec_bfd, sect->the_bfd_section);

      if (flags & SEC_CODE)
	{
	  sect->addr += text_off;
	  sect->endaddr += text_off;
	}
      else if (flags & (SEC_DATA | SEC_LOAD))
	{
	  sect->addr += data_off;
	  sect->endaddr += data_off;
	}
      else if (flags & SEC_ALLOC)
	{
	  sect->addr += bss_off;
	  sect->endaddr += bss_off;
	}
    }
}

static void
set_section_command (char *args, int from_tty)
{
  struct section_table *p;
  char *secname;
  unsigned seclen;
  unsigned long secaddr;
  char secprint[100];
  long offset;

  if (args == 0)
    error (_("Must specify section name and its virtual address"));

  /* Parse out section name */
  for (secname = args; !isspace (*args); args++);
  seclen = args - secname;

  /* Parse out new virtual address */
  secaddr = parse_and_eval_address (args);

  for (p = exec_ops.to_sections; p < exec_ops.to_sections_end; p++)
    {
      if (!strncmp (secname, bfd_section_name (exec_bfd, p->the_bfd_section), seclen)
	  && bfd_section_name (exec_bfd, p->the_bfd_section)[seclen] == '\0')
	{
	  offset = secaddr - p->addr;
	  p->addr += offset;
	  p->endaddr += offset;
	  if (from_tty)
	    exec_files_info (&exec_ops);
	  return;
	}
    }
  if (seclen >= sizeof (secprint))
    seclen = sizeof (secprint) - 1;
  strncpy (secprint, secname, seclen);
  secprint[seclen] = '\0';
  error (_("Section %s not found"), secprint);
}

/* If we can find a section in FILENAME with BFD index INDEX, and the
   user has not assigned an address to it yet (via "set section"), adjust it
   to ADDRESS.  */

void
exec_set_section_address (const char *filename, int index, CORE_ADDR address)
{
  struct section_table *p;

  for (p = exec_ops.to_sections; p < exec_ops.to_sections_end; p++)
    {
      if (strcmp (filename, p->bfd->filename) == 0
	  && index == p->the_bfd_section->index
	  && p->addr == 0)
	{
	  p->addr = address;
	  p->endaddr += address;
	}
    }
}

/* If mourn is being called in all the right places, this could be say
   `gdb internal error' (since generic_mourn calls
   breakpoint_init_inferior).  */

static int
ignore (CORE_ADDR addr, bfd_byte *contents)
{
  return 0;
}

/* Find mapped memory. */

extern void
exec_set_find_memory_regions (int (*func) (int (*) (CORE_ADDR, 
						    unsigned long, 
						    int, int, int, 
						    void *),
					   void *))
{
  exec_ops.to_find_memory_regions = func;
}

static char *exec_make_note_section (bfd *, int *);

/* Fill in the exec file target vector.  Very few entries need to be
   defined.  */

void
init_exec_ops (void)
{
  exec_ops.to_shortname = "exec";
  exec_ops.to_longname = "Local exec file";
  exec_ops.to_doc = "Use an executable file as a target.\n\
Specify the filename of the executable file.";
  exec_ops.to_open = exec_open;
  exec_ops.to_close = exec_close;
  exec_ops.to_attach = find_default_attach;
  exec_ops.deprecated_xfer_memory = xfer_memory;
  exec_ops.to_files_info = exec_files_info;
  exec_ops.to_insert_breakpoint = ignore;
  exec_ops.to_remove_breakpoint = ignore;
  exec_ops.to_create_inferior = find_default_create_inferior;
  exec_ops.to_stratum = file_stratum;
  exec_ops.to_has_memory = 1;
  exec_ops.to_make_corefile_notes = exec_make_note_section;
  exec_ops.to_magic = OPS_MAGIC;

  exec_ops.to_can_async_p = standard_can_async_p;
  exec_ops.to_is_async_p = standard_is_async_p;
  exec_ops.to_async = standard_async;
  exec_ops.to_async_mask_value = 1;
}

void
_initialize_exec (void)
{
  struct cmd_list_element *c;

  init_exec_ops ();

  if (!dbx_commands)
    {
      c = add_cmd ("file", class_files, file_command, _("\
Use FILE as program to be debugged.\n\
It is read for its symbols, for getting the contents of pure memory,\n\
and it is the program executed when you use the `run' command.\n\
If FILE cannot be found as specified, your execution directory path\n\
($PATH) is searched for a command of that name.\n\
No arg means to have no executable file and no symbols."), &cmdlist);
      set_cmd_completer (c, filename_completer);
      /* c->completer_word_break_characters = gdb_completer_filename_word_break_characters; */ /* FIXME */
    }

  c = add_cmd ("exec-file", class_files, exec_file_command, _("\
Use FILE as program for getting contents of pure memory.\n\
If FILE cannot be found as specified, your execution directory path\n\
is searched for a command of that name.\n\
No arg means have no executable file."), &cmdlist);
  set_cmd_completer (c, filename_completer);
  /* c->completer_word_break_characters = gdb_completer_filename_word_break_characters; */ /* FIXME */

  add_com ("section", class_files, set_section_command, _("\
Change the base address of section SECTION of the exec file to ADDR.\n\
This can be used if the exec file does not contain section addresses,\n\
(such as in the a.out format), or when the addresses specified in the\n\
file itself are wrong.  Each section must be changed separately.  The\n\
``info files'' command lists all the sections and their addresses."));

  add_setshow_boolean_cmd ("write", class_support, &write_files, _("\
Set writing into executable and core files."), _("\
Show writing into executable and core files."), NULL,
			   NULL,
			   show_write_files,
			   &setlist, &showlist);

  add_target (&exec_ops);
}

static char *
exec_make_note_section (bfd *obfd, int *note_size)
{
  error (_("Can't create a corefile"));
}
