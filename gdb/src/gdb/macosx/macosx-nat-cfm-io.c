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

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"

#include <string.h>

#if 0
pef_load_library (const struct dyld_path_info *d, struct dyld_objfile_entry *e)
{
  bfd *pbfd = NULL;
  bfd *sbfd = NULL;
  char *symname = NULL;
  asection *csection = NULL;
  asection *dsection = NULL;
  struct section_addr_info addrs;
  unsigned int i = 0;

  if (strcmp (bfd_get_target (pbfd), "pef-xlib") == 0) {
    return pbfd;
  }

  if (strcmp (bfd_get_target (pbfd), "pef") != 0) {
    warning ("Unable to read symbols from %s: invalid file format \"%s\".",
	     bfd_get_filename (pbfd), bfd_get_target (pbfd));
    return pbfd;
  }
  
  csection = bfd_get_section_by_name (pbfd, "code");
  if (csection == NULL) {
    warning ("Unable to find 'code' section in pef container \"%s\" at address 0x%lx for 0x%lx",
	     name, (unsigned long) address, (unsigned long) length);
    return pbfd;
  }

  dsection = bfd_get_section_by_name (pbfd, "packed-data");
  if (dsection == NULL) {
    warning ("Unable to find 'packed-data' section in pef container \"%s\" at address 0x%lx for 0x%lx",
	     name, (unsigned long) address, (unsigned long) length);
    return pbfd;
  }

  symname = xmalloc (strlen (name) + strlen (".xSYM") + 1);
  sprintf (symname, "%s%s", name, ".xSYM");
  sbfd = bfd_openr (symname, "sym");
#if 0
  if (sbfd == NULL) {
    warning ("unable to open \"%s\": %s", symname, bfd_errmsg (bfd_get_error ()));
  }
#endif

  for (i = 0; i < MAX_SECTIONS; i++) {
    addrs.other[i].name = NULL;
    addrs.other[i].addr = address;
    addrs.other[i].sectindex = 0;
  }
  addrs.addrs_are_offsets = 1;

  if (pbfd != NULL) {
    if (! bfd_check_format (pbfd, bfd_object)) {
      warning ("file \"%s\" is not a valid symbol file", pbfd->filename);
    } else {
      symbol_file_add_bfd_safe (pbfd, 0, &addrs, 0, 0, 0, 0);
    }
  }

  addrs.other[0].name = "code";
  addrs.other[0].addr = address + csection->vma;
  addrs.other[0].sectindex = 0;

  addrs.other[1].name = "packed-data";
  addrs.other[1].addr = address + dsection->vma;
  addrs.other[1].sectindex = 1;
  
  for (i = 2; i < MAX_SECTIONS; i++) {
    addrs.other[i].name = NULL;
    addrs.other[i].addr = address + csection->vma;
    addrs.other[i].sectindex = 0;
  }

  addrs.addrs_are_offsets = 1;

  if (sbfd != NULL) {
    if (! bfd_check_format (sbfd, bfd_object)) {
      warning ("file \"%s\" is not a valid symbol file", sbfd->filename);
    } else {
      symbol_file_add_bfd_safe (sbfd, 0, &addrs, 0, 0, 0, 0);
    }
  }

  return pbfd;
}
#endif
