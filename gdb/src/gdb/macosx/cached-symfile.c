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

#include "defs.h"
#include "bfd.h"		/* Binary File Description */
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb-stabs.h"
#include "target.h"
#include "gdbcmd.h"

#include "gdb_stat.h"
#include "obstack.h"
#include "mmprivate.h"

#include <fcntl.h>

#include "macosx-nat-dyld.h"
#include "macosx-nat-dyld-path.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-inferior-debug.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-dyld.h"
#include "macosx-nat-dyld-info.h"
#include "macosx-nat-dyld-path.h"
#include "macosx-nat-dyld-process.h"

#define MAPPED_SYMFILES (USE_MMALLOC && HAVE_MMAP)

#define MMALLOC_SHARED (1 << 1)

static char *cached_symfile_path = NULL;

static unsigned long cached_symfile_version = 1;

extern int mapped_symbol_files;
extern int use_mapped_symbol_files;

extern macosx_inferior_status *macosx_status;

extern struct cmd_list_element *setshliblist;
extern struct cmd_list_element *showshliblist;
extern struct cmd_list_element *infoshliblist;
extern struct cmd_list_element *shliblist;

#ifndef TARGET_KEEP_SECTION
#define TARGET_KEEP_SECTION(ASECT)	0
#endif

/* Declarations for functions defined in objfiles.c */

extern struct objfile *allocate_objfile (bfd *, int, int, CORE_ADDR);

extern int build_objfile_section_table (struct objfile *);

extern struct objfile *open_objfile_from_mmalloc_pool (char *name, bfd *abfd, PTR md, int fd);

extern struct objfile *create_objfile_from_mmalloc_pool (bfd *abfd, PTR md, int fd, CORE_ADDR mapaddr);

extern struct objfile *open_mapped_objfile (bfd *abfd, CORE_ADDR mapaddr);

struct objfile *create_mapped_objfile (bfd *abfd, CORE_ADDR mapaddr);

struct objfile *create_objfile (bfd *abfd);

int
build_objfile_section_table (struct objfile *objfile)
{
  asection *asect;
  unsigned int i = 0;
  bfd *abfd = objfile->obfd;

  i = 0;
  for (asect = abfd->sections; asect != NULL; asect = asect->next)
    i++;

  objfile->sections = xmalloc (sizeof (struct obj_section) * i);

  i = 0;
  for (asect = abfd->sections; asect != NULL; asect = asect->next)
    {
      struct obj_section section;
      flagword aflag;

      aflag = bfd_get_section_flags (abfd, asect);

      if (!(aflag & SEC_ALLOC) && !(TARGET_KEEP_SECTION (asect)))
	continue;

      if (0 == bfd_section_size (abfd, asect))
	continue;

      section.offset = 0;
      section.objfile = objfile;
      section.the_bfd_section = asect;
      section.ovly_mapped = 0;
      section.addr = bfd_section_vma (abfd, asect);
      section.endaddr = section.addr + bfd_section_size (abfd, asect);

      objfile->sections[i++] = section;
      objfile->sections_end = objfile->sections + i;
    }
  
  return 0;
}

struct objfile *
allocate_objfile (bfd *abfd, int flags, int symflags, CORE_ADDR mapaddr)
{
  struct objfile *objfile = NULL;
  struct objfile *last_one = NULL;

  if (mapped_symbol_files)
    flags |= OBJF_MAPPED;

#if MAPPED_SYMFILES

  if (use_mapped_symbol_files)
    objfile = open_mapped_objfile (abfd, mapaddr);

  if ((objfile == NULL) && (flags & OBJF_MAPPED))
    objfile = create_mapped_objfile (abfd, mapaddr);

#endif

  if (objfile == NULL)
    objfile = create_objfile (abfd);

  objfile->symflags = symflags;
  objfile->flags |= flags;

  /* Update the per-objfile information that comes from the bfd, ensuring
     that any data that is reference is saved in the per-objfile data
     region. */

  objfile->obfd = abfd;
  objfile->name = strsave (bfd_get_filename (abfd));
  objfile->mtime = bfd_get_mtime (abfd);

  if (build_objfile_section_table (objfile))
    error ("Can't find the file sections in `%s': %s",
	   objfile->name, bfd_errmsg (bfd_get_error ()));

  /* Initialize the section indexes for this objfile, so that we can
     later detect if they are used w/o being properly assigned to. */

  objfile->sect_index_text = -1;
  objfile->sect_index_data = -1;
  objfile->sect_index_bss = -1;
  objfile->sect_index_rodata = -1;

  /* Add this file onto the tail of the linked list of other such files. */

  objfile->next = NULL;
  if (object_files == NULL)
    object_files = objfile;
  else
    {
      for (last_one = object_files;
	   last_one->next;
	   last_one = last_one->next);
      last_one->next = objfile;
    }

  return (objfile);
}

#if MAPPED_SYMFILES

struct objfile *
open_objfile_from_mmalloc_pool (char *filename, bfd *abfd, PTR md, int fd)
{
  struct objfile *objfile;
  struct stat sbuf;
  time_t mtime = 0;
  time_t timestamp = 0;
  unsigned long version;
  void *mapto;

  struct mdesc *mdp = MD_TO_MDP (md);

  objfile = (struct objfile *) mmalloc_getkey (md, 0);
  if (objfile == NULL)
    {
      warning ("Unable to read objfile from \"%s\"; ignoring", filename);
      return NULL;
    }

  mapto = (void *) mmalloc_getkey (md, 1);
  if (mapto != (void *) md)
    {
      warning ("File \"%s\" is mapped at invalid address 0x%lx (should be 0x%lx)",
	       filename, (unsigned long) md, (unsigned long) mapto);
      return NULL;
    }

  timestamp = (time_t) mmalloc_getkey (md, 2);

  mtime = bfd_get_mtime (abfd);

  if (mtime != timestamp)
    {
      char *str1 = xstrdup (ctime (&mtime));
      char *str2 = xstrdup (ctime (&timestamp));
      str1[strlen(str1) - 1] = '\0';
      str2[strlen(str2) - 1] = '\0';
      warning ("Mapped symbol file \"%s\" is out of date, ignoring\n"
	       "Symbol file was created on %s; mapped file timestamp is %s",
	       filename, str1, str2);
      xfree (str1);
      xfree (str2);
      return NULL;
    }

  version = (unsigned long) mmalloc_getkey (md, 3);
  if (version != cached_symfile_version)
    {
      warning ("Mapped symbol file \"%s\" is for a different version of GDB; ignoring", filename);
      return NULL;
    }

  /* Update memory corruption handler function addresses. */
  md = init_malloc (md);

  /* Forget things specific to a particular gdb, may have changed. */
  objfile->md = md;
  objfile->mmfd = fd;
  objfile->sf = NULL;

  mdp->mmalloc_hook = abort;
  mdp->mrealloc_hook = abort;
  mdp->mfree_hook = abort;

  /* Update pointers to functions to *our* copies */
  objfile->psymbol_cache.cache.chunkfun = xmmalloc;
  objfile->psymbol_cache.cache.freefun = xmfree;
  objfile->psymbol_cache.cache.extra_arg = objfile->md;
  objfile->psymbol_obstack.chunkfun = xmmalloc;
  objfile->psymbol_obstack.freefun = xmfree;
  objfile->psymbol_obstack.extra_arg = objfile->md;
  objfile->symbol_obstack.chunkfun = xmmalloc;
  objfile->symbol_obstack.freefun = xmfree;
  objfile->symbol_obstack.extra_arg = objfile->md;
  objfile->type_obstack.chunkfun = xmmalloc;
  objfile->type_obstack.freefun = xmfree;
  objfile->type_obstack.extra_arg = objfile->md;
  
  return objfile;
}

struct objfile *
open_mapped_objfile (bfd *abfd, CORE_ADDR mapaddr)
{
  char *filename = NULL;
  char *resolved = NULL;
  time_t mtime = 0;

  char *symsfilename = NULL;
  struct stat sbuf;
  int fd = -1;

  PTR md = NULL;
  struct objfile *objfile;

  filename = bfd_get_filename (abfd);
  mtime = bfd_get_mtime (abfd);

  symsfilename = concat (basename (filename), ".syms", (char *) NULL);

  /* First try to open an existing file in the current directory, and
     then try the directory where the symbol file is located. */

  fd = openp (cached_symfile_path, 1, symsfilename, 0, O_RDWR, &resolved);

  if (fstat (fd, &sbuf) != 0)
    return NULL;

  if (sbuf.st_mtime < mtime)
    {
      char *str1 = xstrdup (ctime (&sbuf.st_mtime));
      char *str2 = xstrdup (ctime (&mtime));
      str1[strlen(str1) - 1] = '\0';
      str2[strlen(str2) - 1] = '\0';
      warning ("Mapped symbol file \"%s\" is out of date, ignoring\n"
	       "Symbol file was created on %s; mapped file on %s",
	       symsfilename, str1, str2);
      xfree (str1);
      xfree (str2);
      return NULL;
    }

  md = mmalloc_attach (fd, (PTR) 0, MMALLOC_SHARED);
  if (md == NULL)
    {
      warning ("Unable to attach to mapped file; ignoring");
      return NULL;
    }

  objfile = open_objfile_from_mmalloc_pool (resolved, abfd, md, fd);
  if (objfile == NULL)
    return objfile;

  objfile->flags |= OBJF_MAPPED;

  return objfile;
}

struct objfile *
create_objfile_from_mmalloc_pool (bfd *abfd, PTR md, int fd, CORE_ADDR mapaddr)
{
  struct objfile *objfile;
  struct stat sbuf;

  objfile = (struct objfile *) xmmalloc (md, sizeof (struct objfile));
  memset (objfile, 0, sizeof (struct objfile));
  objfile->md = md;
  objfile->mmfd = fd;
  objfile->flags |= OBJF_MAPPED;

  objfile->md = init_malloc (md);

  mmalloc_setkey (md, 0, objfile);
  mmalloc_setkey (md, 1, ((unsigned char *) 0) + mapaddr);
  mmalloc_setkey (md, 2, bfd_get_mtime (abfd));
  mmalloc_setkey (md, 3, cached_symfile_version);

  obstack_specify_allocation_with_arg
    (&objfile->psymbol_cache.cache, 0, 0, xmmalloc, xmfree, objfile->md);
  obstack_specify_allocation_with_arg
    (&objfile->psymbol_obstack, 0, 0, xmmalloc, xmfree, objfile->md);
  obstack_specify_allocation_with_arg
    (&objfile->symbol_obstack, 0, 0, xmmalloc, xmfree, objfile->md);
  obstack_specify_allocation_with_arg
    (&objfile->type_obstack, 0, 0, xmmalloc, xmfree, objfile->md);

  return objfile;
}

struct objfile *
create_mapped_objfile (bfd *abfd, CORE_ADDR mapaddr)
{
  char *filename = NULL;
  char *symsfilename = NULL;

  int fd = -1;

  PTR md = NULL;
  struct objfile *objfile;

  filename = bfd_get_filename (abfd);

  symsfilename = concat ("./", basename (filename), ".syms", (char *) NULL);
  fd = open (symsfilename, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      warning ("Unable to open symfile");
      return NULL;
    }

  md = mmalloc_attach (fd, ((unsigned char *) 0) + mapaddr, NULL);
  if (md == NULL)
    {
      warning ("Unable to map symbol file at 0x%lx", (unsigned long) mapaddr);
      return NULL;
    }

  objfile = create_objfile_from_mmalloc_pool (abfd, md, fd, mapaddr);
  return objfile;
}

#endif /* MAPPED_SYMFILES */

struct objfile *
create_objfile (bfd *abfd)
{
  struct objfile *objfile;

  objfile = (struct objfile *) xmalloc (sizeof (struct objfile));
  memset (objfile, 0, sizeof (struct objfile));
  objfile->md = NULL;
  obstack_specify_allocation (&objfile->psymbol_cache.cache, 0, 0, xmalloc, xfree);
  obstack_specify_allocation (&objfile->psymbol_obstack, 0, 0, xmalloc, xfree);
  obstack_specify_allocation (&objfile->symbol_obstack, 0, 0, xmalloc, xfree);
  obstack_specify_allocation (&objfile->type_obstack, 0, 0, xmalloc, xfree);

  return objfile;
}

void
_initialize_cached_symfile ()
{
  add_show_from_set
    (add_set_cmd ("cached-symfile-path", class_support, var_string,
		  (char *) &cached_symfile_path,
		  "Set list of directories to search for cached symbol files.",
		  &setlist),
     &showlist);

  cached_symfile_path = xstrdup ("/usr/libexec/gdb/symfiles");
}
