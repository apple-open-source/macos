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

#include "macosx-nat-inferior.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-dyld-info.h"

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"
#include "target.h"

#include <string.h>
#include <sys/stat.h>

#include <mach-o/nlist.h>
#include <mach-o/loader.h>
#include <mach-o/dyld_debug.h>

#include "mach-o.h"

struct inferior_info {
  bfd_vma addr;
  bfd_vma offset;
  bfd_vma len;
};

static bfd_size_type
inferior_read
(PTR iodata, PTR data, bfd_size_type size, bfd_size_type nitems, bfd *abfd, bfd_vma where)
{
  struct inferior_info *iptr = (struct inferior_info *) iodata;
  int ret;

  CHECK_FATAL (iptr != NULL);
  
  if (strcmp (current_target.to_shortname, "macos-child") != 0) {
    bfd_set_error (bfd_error_no_contents);
    return 0;
  }

  if (where > iptr->len) {
    bfd_set_error (bfd_error_no_contents);
    return 0;
  }

  ret = current_target.to_xfer_memory (iptr->addr + where, data, (size * nitems),
				       NULL, 0, &current_target);
  if (ret <= 0) {
    bfd_set_error (bfd_error_system_call);
    return 0;
  }
  return ret;
}

static bfd_size_type
mach_o_inferior_read
(PTR iodata, PTR data, bfd_size_type size, bfd_size_type nitems, bfd *abfd, bfd_vma where)
{
  struct inferior_info *iptr = (struct inferior_info *) iodata;
  unsigned int i;
  int ret;

  CHECK_FATAL (iptr != NULL);
  
  if (strcmp (current_target.to_shortname, "macos-child") != 0) {
    bfd_set_error (bfd_error_no_contents);
    return 0;
  }

  if ((strcmp (bfd_get_target (abfd), "mach-o-be") != 0)
      && (strcmp (bfd_get_target (abfd), "mach-o-le") != 0)
      && (strcmp (bfd_get_target (abfd), "mach-o") != 0)) {
    bfd_set_error (bfd_error_invalid_target);
    return 0;
  }
  
  {
    struct mach_o_data_struct *mdata = NULL;
    CHECK_FATAL (bfd_mach_o_valid (abfd));
    mdata = abfd->tdata.mach_o_data;
    for (i = 0; i < mdata->header.ncmds; i++) {
      struct bfd_mach_o_load_command *cmd = &mdata->commands[i];
      if (cmd->type == BFD_MACH_O_LC_SEGMENT) {
	struct bfd_mach_o_segment_command *segment = &cmd->command.segment;
	if ((where >= segment->fileoff)
	    && (where < (segment->fileoff + segment->filesize))) {
	  bfd_vma infaddr = (segment->vmaddr + iptr->offset + (where - segment->fileoff));
	  ret = current_target.to_xfer_memory
	    (infaddr, data, (size * nitems), 0, NULL, &current_target);
	  if (ret <= 0) {
	    bfd_set_error (bfd_error_system_call);
	    return 0;
	  }
	  return ret;
	}
      }
    }
  }

  
  bfd_set_error (bfd_error_no_contents);
  return 0;
}

static bfd_size_type
inferior_write
(PTR iodata, const PTR data, bfd_size_type size, bfd_size_type nitems, bfd *abfd, bfd_vma where)
{
  error ("unable to write to in-memory images");
}

static int
inferior_flush (PTR iodata, bfd *abfd)
{
  return 0;
}

static bfd_boolean
inferior_close (PTR iodata, bfd *abfd)
{
  xfree (iodata);
  return 1;
}

static int
inferior_stat (PTR iodata, bfd *abfd, struct stat *statbuf)
{
  struct inferior_info *iptr = (struct inferior_info *) iodata;
  memset (statbuf, 0, sizeof (struct stat));
  statbuf->st_size = iptr->len;
  return 0;
}

static bfd *
inferior_bfd_generic
(const char *name, CORE_ADDR addr, CORE_ADDR offset, CORE_ADDR len)
{
  struct inferior_info *iptr = NULL;
  struct bfd_io_functions fdata;
  char *filename = NULL;
  bfd *ret = NULL;

  iptr = (struct inferior_info *) xmalloc (sizeof (struct inferior_info));
  iptr->addr = addr;
  iptr->offset = offset;
  iptr->len = len;

  fdata.iodata = iptr;
  fdata.read_func = &inferior_read;
  fdata.write_func = &inferior_write;
  fdata.flush_func = &inferior_flush;
  fdata.close_func = &inferior_close;
  fdata.stat_func = &inferior_stat;

  if (name != NULL)
    {
      xasprintf (&filename, "[memory object \"%s\" at 0x%lx for 0x%lx]",
		 name, (unsigned long) addr, (unsigned long) len);
    }
  else
    {
      xasprintf (&filename, "[memory object at 0x%lx for 0x%lx]",
		 (unsigned long) addr, (unsigned long) len);
    }

  if (filename == NULL) {
    warning ("unable to allocate memory for filename for \"%s\"", name);
    return NULL;
  }

  ret = bfd_funopenr (filename, NULL, &fdata);
  if (ret == NULL) { 
    warning ("Unable to open memory image for \"%s\"; skipping", name);
    return NULL;
  }
  
  if (bfd_check_format (ret, bfd_archive))
    {
      bfd *abfd = NULL;

#if defined (__ppc__)
      const bfd_arch_info_type *thisarch = bfd_lookup_arch (bfd_arch_powerpc, 0);
#elif defined (__i386__)
      const bfd_arch_info_type *thisarch = bfd_lookup_arch (bfd_arch_i386, 0);
#else
      const bfd_arch_info_type *thisarch = bfd_lookup_arch (bfd_arch_powerpc, 0);
#endif
      for (;;) {

	const bfd_arch_info_type *subarch = NULL;

	if (thisarch == NULL) { break; }
	abfd = bfd_openr_next_archived_file (ret, abfd);
	if (abfd == NULL) { break; }
	if (! bfd_check_format (abfd, bfd_object)) { abfd = NULL; break; }
	subarch = bfd_get_arch_info (abfd);
	
	if (subarch->compatible (subarch, thisarch));
      }
      if (abfd != NULL) { 
	ret = abfd;
      }
    }

  /* FIXME: should check for errors from bfd_close (for one thing, on
     error it does not free all the storage associated with the bfd).  */

  if (! bfd_check_format (ret, bfd_object)) {
    warning ("Unable to read symbols from %s: %s.", bfd_get_filename (ret), bfd_errmsg (bfd_get_error ()));
    bfd_close (ret);
    return NULL;
  }

  return ret;
}

bfd *
inferior_bfd
(const char *name, CORE_ADDR addr, CORE_ADDR offset, CORE_ADDR len)
{
  bfd *ret = inferior_bfd_generic (name, addr, offset, len);
  if (ret == NULL)
    return ret;

  if ((strcmp (bfd_get_target (ret), "mach-o-be") == 0)
      || (strcmp (bfd_get_target (ret), "mach-o-le") == 0)
      || (strcmp (bfd_get_target (ret), "mach-o") == 0)) 
    {
      struct bfd_io_functions *fun = (struct bfd_io_functions *) ret->iostream;
      CHECK_FATAL (fun != NULL);

      fun->read_func = mach_o_inferior_read;
    }

  if ((strcmp (bfd_get_target (ret), "pef") == 0)
      || (strcmp (bfd_get_target (ret), "pef-xlib") == 0))
    {
      struct bfd_io_functions *fun = (struct bfd_io_functions *) ret->iostream;
      CHECK_FATAL (fun != NULL);

      /* no changes necessary */
    }
  
  return ret;
}
