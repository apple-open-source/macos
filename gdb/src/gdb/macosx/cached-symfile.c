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
#include "bfd.h"                /* Binary File Description */
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb-stabs.h"
#include "target.h"
#include "gdbcmd.h"
#include "bcache.h"
#include "gdbtypes.h"

#include "gdb_stat.h"
#include "obstack.h"

#include <fcntl.h>
#include <sys/mman.h>

#include "gdb_assert.h"

#include "cached-symfile.h"
#include "macosx-nat-dyld.h"

/* From objfiles.c */
extern void objfile_alloc_data (struct objfile *objfile);

#ifdef USE_MMALLOC
#include "mmprivate.h"
#endif

#define MAPPED_SYMFILES (USE_MMALLOC && HAVE_MMAP)

#define MMALLOC_SHARED (1 << 1)

#if MAPPED_SYMFILES
static void move_objfile (struct relocation_context *r, struct objfile *o);

static inline int
relocate (struct relocation_context *r, void *ptr, void **new);
#endif

static char *cached_symfile_path = NULL;
static char *cached_symfile_dir = NULL;

static int check_timestamp = 1;

#ifndef FSF_OBJFILES

static unsigned long cached_symfile_version = 7;

extern int mapped_symbol_files;
extern int use_mapped_symbol_files;

extern struct cmd_list_element *setshliblist;
extern struct cmd_list_element *showshliblist;
extern struct cmd_list_element *infoshliblist;
extern struct cmd_list_element *shliblist;

#ifndef TARGET_KEEP_SECTION
#define TARGET_KEEP_SECTION(ASECT)      0
#endif

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
  objfile->sections_end = objfile->sections;

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

  objfile_add_to_ordered_sections (objfile);

  return 0;
}

void
init_objfile (struct objfile *objfile)
{
  /* Initialize the section indexes for this objfile, so that we can
     later detect if they are used w/o being properly assigned to. */

  objfile->sect_index_text = -1;
  objfile->sect_index_data = -1;
  objfile->sect_index_bss = -1;
  objfile->sect_index_rodata = -1;
}

struct objfile *
allocate_objfile (bfd *abfd, int flags, int symflags, CORE_ADDR mapaddr,
                  const char *prefix)
{
  struct objfile *objfile = NULL;

  if (mapped_symbol_files)
    flags |= OBJF_MAPPED;

#if MAPPED_SYMFILES

  if (use_mapped_symbol_files)
    objfile =
      open_mapped_objfile (bfd_get_filename (abfd), abfd,
                           bfd_get_mtime (abfd), mapaddr, prefix);

  if ((objfile == NULL) && (flags & OBJF_MAPPED))
    {
      mkdir (cached_symfile_dir, 0777);
      objfile = cache_bfd (abfd, prefix, symflags, 0, 0, cached_symfile_dir);
    }

#endif

  if (objfile == NULL)
    {
      objfile = create_objfile (abfd);

      objfile->symflags = symflags;
      objfile->flags |= flags;

      /* Update the per-objfile information that comes from the bfd, ensuring
         that any data that is reference is saved in the per-objfile data
         region. */

      objfile->obfd = abfd;
      if (objfile->name)
        objfile->name = xstrdup (objfile->name);
      else
        objfile->name = xstrdup (bfd_get_filename (abfd));
      objfile->mtime = bfd_get_mtime (abfd);

      if (build_objfile_section_table (objfile))
        error ("Can't find the file sections in `%s': %s",
               objfile->name, bfd_errmsg (bfd_get_error ()));

      /* FIXME: At some point this should be a host specific callout.
         Even though this is a Mac OS X specific copy of allocate_objfile,
         we should still fix this when we fix that...  */

      if (objfile->name != NULL &&
          strstr (objfile->name, "libSystem") != NULL)
        objfile->check_for_equivalence = 1;
      else
        objfile->check_for_equivalence = 0;
      objfile->equivalence_table = NULL;

      objfile->syms_only_objfile = 0;

      link_objfile (objfile);
    }

  return (objfile);
}

#if MAPPED_SYMFILES

void
mmalloc_protect (PTR md, int flags)
{
  size_t start, end;
  int ret;

  mmalloc_endpoints (md, &start, &end);

  ret = mprotect (start, end - start, flags);
  if (ret < 0)
    perror ("setting memory protection");
}

static void
mmalloc_xfree_hook (PTR md, PTR ptr)
{
  free (ptr);
}

static PTR
mmalloc_xmalloc_hook (PTR md, size_t size)
{
  return xmalloc (size);
}

static PTR
mmalloc_xrealloc_hook (PTR md, PTR ptr, size_t size)
{
  return xrealloc (ptr, size);
}

struct objfile *
open_objfile_from_mmalloc_pool (PTR md, bfd *abfd, int fd, time_t mtime,
                                char *filename, const char *prefix)
{
  struct objfile *orig_objfile;
  struct symtab *orig_s;
  struct objfile *objfile;
  struct symtab *s;
  struct partial_symtab *p;
  struct partial_symbol *ps;
  time_t timestamp = 0;
  unsigned long version;
  void *mapto;
  struct minimal_symbol *ms;
  size_t start = 0;
  size_t end = 0;

  struct mdesc *mdp = MD_TO_MDP (md);

  mapto = (void *) mmalloc_getkey (md, 0);

  orig_objfile =
    (struct objfile *) ((CORE_ADDR) mmalloc_getkey (md, 1) +
                        (CORE_ADDR) mmalloc_getkey (md, 0));
  if (orig_objfile == NULL)
    {
      warning ("Unable to read objfile from \"%s\"; ignoring", filename);
      return NULL;
    }

  timestamp = (time_t) mmalloc_getkey (md, 2);

  if (mtime != timestamp)
    {
      if (info_verbose)
        {
          char *str1 = xstrdup (ctime (&mtime));
          char *str2 = xstrdup (ctime (&timestamp));
          str1[strlen (str1) - 1] = '\0';
          str2[strlen (str2) - 1] = '\0';
          if (check_timestamp)
            warning
              ("Timestamp for cached symbol file \"%s\" (%s) does not match timestamp for library (%s); ignoring",
               filename, str1, str2);
          else
            warning
              ("Timestamp for cached symbol file \"%s\" (%s) does not match timestamp for library (%s); using anyway",
               filename, str1, str2);
          xfree (str1);
          xfree (str2);
        }
      if (check_timestamp)
        return NULL;
    }

  version = (unsigned long) mmalloc_getkey (md, 3);
  if (version != cached_symfile_version)
    {
      warning
        ("Version for symbol file \"%s\" (%ld) does not match version for GDB (%ld); ignoring",
         filename, version, cached_symfile_version);
      return NULL;
    }

  objfile = (struct objfile *) xmalloc (sizeof (struct objfile));

  mmalloc_endpoints (md, &start, &end);

  if (md != mapto)
    {
      struct relocation_context r;

      r.len = end - start;

      r.from_start = (size_t) mapto;
      r.to_start = start;
      r.mapped_start = start;

      r.objfile = objfile;

      orig_objfile = objfile_reposition (orig_objfile, &r);
    }

  *objfile = *orig_objfile;

  if (strcmp ((orig_objfile->prefix == NULL) ? "" : orig_objfile->prefix,
              (prefix == NULL) ? "" : prefix) != 0)
    {
      warning
        ("Mapped symbol file \"%s\" uses a different prefix (\"%s\") than the one requested (\"%s\"); ignoring",
         filename, (orig_objfile->prefix == NULL) ? "" : orig_objfile->prefix,
         (prefix == NULL) ? "" : prefix);
      return NULL;
    }

  for (ms = objfile->msymbols; (ms != NULL) && (ms->ginfo.name != NULL); ms++)
    ms->ginfo.bfd_section = NULL;

  mdp->mmalloc_hook = (void *) mmalloc_xmalloc_hook;
  mdp->mrealloc_hook = (void *) mmalloc_xrealloc_hook;
  mdp->mfree_hook = (void *) mmalloc_xfree_hook;

  mmalloc_protect (md, PROT_READ);

  /* Update stale pointers from the old symfile, to point to the
     values in the current GDB address space. */

  objfile->md = md;
  objfile->mmfd = fd;
  objfile->sf = NULL;
  objfile->obfd = abfd;
  objfile->name = filename;
  objfile->flags |= OBJF_MAPPED;

  if (objfile->symtabs != NULL)
    {
      struct symtab *cur, *prev, *temp;

      temp = (struct symtab *) xmalloc (sizeof (struct symtab));
      *temp = *objfile->symtabs;

      objfile->symtabs = temp;
      prev = objfile->symtabs;
      cur = prev->next;

      while (cur != NULL)
        {
          temp = (struct symtab *) xmalloc (sizeof (struct symtab));

          *temp = *cur;
          prev->next = temp;

          prev = prev->next;
          cur = cur->next;
        }
    }

  if (objfile->psymtabs != NULL)
    {
      struct partial_symtab *cur, *prev, *temp;

      temp =
        (struct partial_symtab *) xmalloc (sizeof (struct partial_symtab));
      *temp = *objfile->psymtabs;

      objfile->psymtabs = temp;
      prev = objfile->psymtabs;
      cur = prev->next;

      while (cur != NULL)
        {
          temp =
            (struct partial_symtab *)
            xmalloc (sizeof (struct partial_symtab));

          *temp = *cur;
          prev->next = temp;

          prev = prev->next;
          cur = cur->next;
        }
    }

  for (p = objfile->psymtabs; p != NULL; p = p->next)
    {
      if (p->symtab == NULL)
        continue;

      orig_s = orig_objfile->symtabs;
      s = objfile->symtabs;

      for (;;)
        {
          if ((orig_s == NULL) || (s == NULL))
            error ("unable to find matching symtab when updating pointers");

          if (p->symtab == orig_s)
            {
              p->symtab = s;
              break;
            }

          orig_s = orig_s->next;
          s = s->next;
        }
    }

  objfile->psymbol_cache = bcache_xmalloc (objfile->md);
  objfile->macro_cache = bcache_xmalloc (objfile->md);

  bcache_specify_allocation_with_arg
    (objfile->psymbol_cache, xmmalloc, xmfree, objfile->md);
  bcache_specify_allocation_with_arg
    (objfile->macro_cache, xmmalloc, xmfree, objfile->md);
  obstack_specify_allocation_with_arg
    (&objfile->objfile_obstack, 0, 0, xmmalloc, xmfree, objfile->md);

  link_objfile (objfile);

  if (build_objfile_section_table (objfile))
    error ("Can't find the file sections in `%s': %s",
           objfile->name, bfd_errmsg (bfd_get_error ()));

  return objfile;
}

struct objfile *
open_mapped_objfile (const char *filename, bfd *abfd, time_t mtime,
                     CORE_ADDR mapaddr, const char *prefix)
{
  const char *symsfilename = NULL;
  const char *resolved = NULL;
  struct objfile *objfile;
  struct stat sbuf;
  int fd = -1;

  PTR md = NULL;

  /* First try to open an existing file in the current directory, and
     then try the directory where the symbol file is located. */

  if ((strlen (filename) >= 5)
      && (strcmp (filename + (strlen (filename) - 5), ".syms") == 0))
    symsfilename = filename;
  else
    symsfilename = concat (basename (filename), ".syms", (char *) NULL);

  fd = openp (cached_symfile_path, 1, symsfilename, 0, O_RDWR, &resolved);

  if (fstat (fd, &sbuf) != 0)
    return NULL;

  if (sbuf.st_mtime < mtime)
    {
      if (info_verbose)
        {
          char *str1 = xstrdup (ctime (&sbuf.st_mtime));
          char *str2 = xstrdup (ctime (&mtime));
          str1[strlen (str1) - 1] = '\0';
          str2[strlen (str2) - 1] = '\0';
          if (check_timestamp)
            warning
              ("Cached symbol file \"%s\" is older (%s) than library (%s); ignoring\n",
               symsfilename, str1, str2);
          else
            warning
              ("Cached symbol file \"%s\" is older (%s) than library (%s); ignoring\n",
               symsfilename, str1, str2);
          xfree (str1);
          xfree (str2);
        }
      if (check_timestamp)
        return NULL;
    }

  md = mmalloc_attach (fd, 0, MMALLOC_SHARED);
  if (md == NULL)
    {
      warning ("Unable to attach to mapped file; ignoring");
      return NULL;
    }

  objfile =
    open_objfile_from_mmalloc_pool (md, abfd, fd, mtime, resolved, prefix);

  if (objfile == NULL)
    mmalloc_detach (md);

  return objfile;
}

struct objfile *
create_objfile_from_mmalloc_pool (bfd *abfd, PTR md, int fd,
                                  CORE_ADDR mapaddr)
{
  struct objfile *objfile;

  objfile = (struct objfile *) xmmalloc (md, sizeof (struct objfile));
  memset (objfile, 0, sizeof (struct objfile));
  objfile->md = md;
  objfile->mmfd = fd;
  objfile->obfd = abfd;
  objfile->flags |= OBJF_MAPPED;

  objfile->md = init_malloc (md);

  mmalloc_setkey (md, 0, (void *) (((unsigned char *) 0) + mapaddr));
  mmalloc_setkey (md, 1, (void *) (((unsigned char *) objfile) - mapaddr));
  mmalloc_setkey (md, 2, (void *) bfd_get_mtime (abfd));
  mmalloc_setkey (md, 3, (void *) cached_symfile_version);

  objfile->psymbol_cache = bcache_xmalloc (objfile->md);
  objfile->macro_cache = bcache_xmalloc (objfile->md);

  bcache_specify_allocation_with_arg
    (objfile->psymbol_cache, xmmalloc, xmfree, objfile->md);
  bcache_specify_allocation_with_arg
    (objfile->macro_cache, xmmalloc, xmfree, objfile->md);
  obstack_specify_allocation_with_arg
    (&objfile->objfile_obstack, 0, 0, xmmalloc, xmfree, objfile->md);

  return objfile;
}

/* Cache the specified bfd into the file, or directory, specified by
   'dest'.  Uses 'addr' as the base address for the symbols of the
   bfd, and 'mapaddr' for the location of the resulting objfile in
   GDB's memory.  If 'mapaddr' is specified as 0, GDB will pick an
   available address.  The resulting objfile is linked onto the GDB
   objfile chain, just as any other objfile would be. */

struct objfile *
cache_bfd (bfd *abfd, const char *prefix, int symflags,
           size_t addr, size_t mapaddr, const char *dest)
{
  unsigned int i;
  struct section_addr_info *addrs;
  struct objfile *objfile = NULL;
  struct symtab *s = NULL;
  struct partial_symtab *p = NULL;
  const char *filename = NULL;
  const char *temp_filename = NULL;
  PTR md = NULL;
  int fd = -1;
  size_t start, end;
  struct stat st;
  struct objfile *repositioned = NULL;
  time_t mtime;
  struct relocation_context r;

  if ((dest[0] != '\0') && (dest[strlen (dest) - 1] == '/'))
    filename =
      concat (dest, basename (bfd_get_filename (abfd)), ".syms",
              (char *) NULL);
  else if ((stat (dest, &st) == 0) && S_ISDIR (st.st_mode))
    filename =
      concat (dest, "/", basename (bfd_get_filename (abfd)), ".syms",
              (char *) NULL);
  else
    filename = dest;

  temp_filename = concat (filename, ".new", (char *) NULL);

  fd = open (temp_filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      warning ("Unable to open \"%s\"", filename);
      return NULL;
    }

  if (mapaddr == 0)
    mapaddr = mmalloc_findbase (256 * 1024 * 1024, 0);

  if (info_verbose)
    {
      printf ("Mapping \"%s\" ...", filename);
      fflush (stdout);
    }

  md = mmalloc_attach (fd, ((unsigned char *) 0) + mapaddr, 0);
  if (md == NULL)
    {
      warning ("Unable to map symbol file at 0x%zx", mapaddr);
      return NULL;
    }

  objfile = create_objfile_from_mmalloc_pool (abfd, md, fd, mapaddr);
  if (objfile == NULL)
    {
      warning ("Unable to create objfile");
      return NULL;
    }

#if 0
  swapout_gdbarch_swap (current_gdbarch);
  clear_gdbarch_swap (current_gdbarch);
#endif

  builtin_type_void = init_type (TYPE_CODE_VOID, 1, 0, "void", objfile);

  builtin_type_int =
    init_type (TYPE_CODE_INT, TARGET_INT_BIT / TARGET_CHAR_BIT,
               0, "int", objfile);

  builtin_type_error =
    init_type (TYPE_CODE_ERROR, 0, 0, "<unknown type>", objfile);

  builtin_type_char =
    init_type (TYPE_CODE_INT, TARGET_CHAR_BIT / TARGET_CHAR_BIT,
               (TYPE_FLAG_NOSIGN
                | (TARGET_CHAR_SIGNED ? 0 : TYPE_FLAG_UNSIGNED)),
               "char", objfile);

  objfile->symflags = symflags;
  objfile->obfd = abfd;
  objfile->name = mstrsave (objfile->md, filename);
  objfile->mtime = bfd_get_mtime (abfd);
  if (prefix == NULL)
    objfile->prefix = NULL;
  else
    objfile->prefix = mstrsave (objfile->md, prefix);

  addrs = NULL;
  abort ();

  for (i = 0; i < addrs->num_sections; i++)
    {
      addrs->other[i].name = NULL;
      addrs->other[i].addr = addr;
      addrs->other[i].sectindex = 0;
      addrs->addrs_are_offsets = 0;
    }

  syms_from_objfile (objfile, &addrs, 0, 0, 0, 0);

  ALL_OBJFILE_PSYMTABS (objfile, p) s = PSYMTAB_TO_SYMTAB (p);

  objfile_demangle_msymbols (objfile);

  filename = xstrdup (objfile->name);
  mtime = bfd_get_mtime (abfd);

  mmalloc_endpoints (objfile->md, &start, &end);
  r.len = end - start;
  r.mapped_start = start;
  r.from_start = start;
  r.to_start = 0;
  r.objfile = NULL;
  repositioned = objfile_reposition (objfile, &r);
  mmalloc_setkey (objfile->md, 0, 0);
  mmalloc_setkey (objfile->md, 1, repositioned);

  link_objfile (objfile);
  objfile->obfd = NULL;
  free_objfile (objfile);

  if (rename (temp_filename, filename) != 0)
    {
      warning ("unable to rename created temporary file");
      unlink (temp_filename);
      return NULL;
    }

  objfile = open_mapped_objfile (filename, abfd, mtime, 0, prefix);
  if (objfile == NULL)
    {
      warning ("unable to read mapped objfile");
      return NULL;
    }

  if (info_verbose)
    printf (" done\n");

#if 0
  swapin_gdbarch_swap (current_gdbarch);
#endif

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
  objfile->psymbol_cache = bcache_xmalloc (NULL);
  objfile->macro_cache = bcache_xmalloc (NULL);
  bcache_specify_allocation (objfile->psymbol_cache, xmalloc, xfree);
  bcache_specify_allocation (objfile->macro_cache, xmalloc, xfree);
  obstack_specify_allocation (&objfile->objfile_obstack, 0, 0, xmalloc,
                              xfree);

  /* FIXME: This needs to be converted to use objfile-specific data. */
  objfile_alloc_data (objfile);

  init_objfile (objfile);

  return objfile;
}

#endif /* FSF_OBJFILES */

static void
validate_context (struct relocation_context *r)
{
  if (((r->to_start >= r->from_start)
       && (r->to_start <= (r->from_start + r->len)))
      || ((r->from_start >= r->to_start)
          && (r->from_start <= (r->to_start + r->len))))
    {
      internal_error (__FILE__, __LINE__,
                      "relocation contained overlapping regions");
    }
}

static int
relocate (struct relocation_context *r, void *ptr, void **new)
{
  size_t addr = (size_t) ptr;

  if (ptr == NULL)
    return 0;

  if (addr == r->objfile)
    return 0;

  if ((addr >= r->to_start) && (addr <= (r->to_start + r->len)))
    {
      return 0;
    }

  if ((addr >= r->from_start) && (addr <= (r->from_start + r->len)))
    {
      *new = (void *) (addr + r->to_start - r->from_start);
      return 1;
    }

  internal_error (__FILE__, __LINE__, "relocation was not in valid range");
}

static size_t
lookup (struct relocation_context *r, void *ptr)
{
  size_t addr = (size_t) ptr;

  if (ptr == NULL)
    return 0;

  if ((addr >= r->from_start) && (addr <= (r->from_start + r->len)))
    return (addr - r->from_start + r->mapped_start);

  if ((addr >= r->to_start) && (addr <= (r->to_start + r->len)))
    return (addr - r->to_start + r->mapped_start);

  internal_error (__FILE__, __LINE__, "relocation was not in valid range");
}

#define RELOCATE(x) (relocate (r, x, (void **) &x))
#define LOOKUP(x) ((typeof (x)) lookup (r, x))

static void move_type (struct relocation_context *r, struct type *t);

static void
move_field (struct relocation_context *r, struct field *f)
{
  if (f->static_kind != 0)
    RELOCATE (f->loc.physname);

  if (RELOCATE (f->type))
    move_type (r, LOOKUP (f->type));

  RELOCATE (f->name);
}

static void
move_cplus_struct (struct relocation_context *r, struct cplus_struct_type *cp)
{
  unsigned int i, j;

  if (cp == NULL)
    return;

  RELOCATE (cp->virtual_field_bits);
  RELOCATE (cp->private_field_bits);
  RELOCATE (cp->protected_field_bits);
  RELOCATE (cp->ignore_field_bits);

  if (RELOCATE (cp->fn_fieldlists))
    {
      struct fn_fieldlist *fn = LOOKUP (cp->fn_fieldlists);
      for (i = 0; i < cp->nfn_fields; i++)
        {
          RELOCATE (fn[i].name);
          if (RELOCATE (fn[i].fn_fields))
            {
              struct fn_field *fld = LOOKUP (fn[i].fn_fields);
              for (j = 0; j < fn[i].length; j++)
                {
                  RELOCATE (fld[j].physname);
                  RELOCATE (fld[j].type);
                  RELOCATE (fld[j].fcontext);
                }
            }
        }
    }

  if (RELOCATE (cp->template_args))
    for (i = 0; i < cp->ntemplate_args; i++)
      {
        RELOCATE (LOOKUP (cp->template_args)[i].name);
        RELOCATE (LOOKUP (cp->template_args)[i].type);
      }

  if (RELOCATE (cp->instantiations))
    for (i = 0; i < cp->ninstantiations; i++)
      RELOCATE (LOOKUP (cp->instantiations)[i]);

  if (RELOCATE (cp->runtime_ptr))
    {
      RELOCATE (LOOKUP (cp->runtime_ptr)->primary_base);
      RELOCATE (LOOKUP (cp->runtime_ptr)->virtual_base_list);
    }

  if (RELOCATE (cp->localtype_ptr))
    RELOCATE (LOOKUP (cp->localtype_ptr)->file);
}

static void
move_main_type (struct relocation_context *r, struct main_type *m)
{
  unsigned int i;

  if (m == NULL)
    return;

  RELOCATE (m->name);
  RELOCATE (m->tag_name);

  if (r->objfile != NULL)
    m->objfile = r->objfile;

  if (RELOCATE (m->target_type))
    move_type (r, LOOKUP (m->target_type));

  if (RELOCATE (m->fields))
    {
      for (i = 0; i < m->nfields; i++)
        move_field (r, LOOKUP (m->fields) + i);
    }

  if (RELOCATE (m->vptr_basetype))
    move_type (r, LOOKUP (m->vptr_basetype));

  if ((m->code == TYPE_CODE_STRUCT) || (m->code == TYPE_CODE_UNION))
    if (RELOCATE (m->type_specific.cplus_stuff))
      move_cplus_struct (r, LOOKUP (m->type_specific.cplus_stuff));
}

static void
move_type (struct relocation_context *r, struct type *t)
{
  if (t == NULL)
    return;

  if (RELOCATE (t->pointer_type))
    move_type (r, LOOKUP (t->pointer_type));

  if (RELOCATE (t->reference_type))
    move_type (r, LOOKUP (t->reference_type));

  if (RELOCATE (t->chain))
    move_type (r, LOOKUP (t->chain));

  if (RELOCATE (t->main_type))
    move_main_type (r, LOOKUP (t->main_type));
}

static void
move_symbol_general (struct relocation_context *r,
                     struct general_symbol_info *g)
{
  if (g == NULL)
    return;

  g->bfd_section = NULL;

  RELOCATE (g->name);
  RELOCATE (g->language_specific.cplus_specific.demangled_name);
}

static void
move_msymbol (struct relocation_context *r, struct minimal_symbol *m)
{
  if (m == NULL)
    return;

  move_symbol_general (r, &m->ginfo);

  if (RELOCATE (m->hash_next))
    move_msymbol (r, LOOKUP (m->hash_next));

  if (RELOCATE (m->demangled_hash_next))
    move_msymbol (r, LOOKUP (m->demangled_hash_next));
}

static void
move_psymbol (struct relocation_context *r, struct partial_symbol *p)
{
  if (p == NULL)
    return;

  move_symbol_general (r, &p->ginfo);
}

static void
move_symbol (struct relocation_context *r, struct symbol *s)
{
  if (s == NULL)
    return;

  move_symbol_general (r, &s->ginfo);

  if (s->aclass == LOC_BLOCK)
    RELOCATE (s->ginfo.value.block);

  if (RELOCATE (s->type))
    move_type (r, LOOKUP (s->type));

  /* RELOCATE (s->aliases); */
  /* RELOCATE (s->ranges); */

  if (RELOCATE (s->hash_next))
    move_symbol (r, LOOKUP (s->hash_next));
}

static void
move_block (struct relocation_context *r, struct block *b)
{
#if 0
  unsigned int i;

  if (b == NULL)
    return;

  RELOCATE (BLOCK_FUNCTION (b));
  RELOCATE (BLOCK_SUPERBLOCK (b));

  for (i = 0; i < BLOCK_NSYMS (b); i++)
    {
      if (RELOCATE (b->sym[i]))
        move_symbol (r, LOOKUP (b->sym[i]));
    }
#endif
}

void
move_blockvector (struct relocation_context *r, struct blockvector *b)
{
#if 0
  unsigned int i;

  if (b == NULL)
    return;

  for (i = 0; i < b->nblocks; i++)
    {
      if (RELOCATE (b->block[i]))
        move_block (r, LOOKUP (b->block[i]));
    }
#endif
}

void
move_psymtab (struct relocation_context *r, struct partial_symtab *p)
{
  unsigned int i;

  if (p == NULL)
    return;

  RELOCATE (p->filename);
  RELOCATE (p->fullname);
  RELOCATE (p->section_offsets);

  RELOCATE (p->objfile);
  if (r->objfile != NULL)
    p->objfile = r->objfile;

  RELOCATE (p->symtab);
  RELOCATE (p->read_symtab_private);

  RELOCATE (p->dependencies);
  for (i = 0; i < p->number_of_dependencies; i++)
    RELOCATE (LOOKUP (p->dependencies)[i]);
}

void
move_obstack_chunk (struct relocation_context *r, struct _obstack_chunk *s)
{
  if (s == NULL)
    return;

  RELOCATE (s->limit);
  RELOCATE (s->prev);

  move_obstack_chunk (r, LOOKUP (s->prev));
}

void
move_obstack (struct relocation_context *r, struct obstack *s)
{
  if (s == NULL)
    return;

  RELOCATE (s->object_base);
  RELOCATE (s->next_free);
  RELOCATE (s->chunk_limit);

  RELOCATE (s->chunk);
  move_obstack_chunk (r, LOOKUP (s->chunk));
}

void
move_symtab (struct relocation_context *r, struct symtab *s)
{
#if 0
  if (s == NULL)
    return;

  if (RELOCATE (s->blockvector))
    move_blockvector (r, LOOKUP (s->blockvector));
  RELOCATE (s->linetable);
  RELOCATE (s->filename);
  RELOCATE (s->dirname);
  RELOCATE (s->free_ptr);
  RELOCATE (s->line_charpos);
  RELOCATE (s->debugformat);
  RELOCATE (s->version);
  RELOCATE (s->fullname);
  RELOCATE (s->objfile);
  if (r->objfile != NULL)
    s->objfile = r->objfile;
#endif
}

static void
move_psymbol_allocation_list (struct relocation_context *r,
                              struct psymbol_allocation_list *l)
{
  unsigned int i;

  if (l == NULL)
    return;

  RELOCATE (l->list);
  RELOCATE (l->next);

  for (i = 0; i < l->size; i++)
    {
      if (LOOKUP (l->list)[i] == NULL)
        break;

      if (!RELOCATE (LOOKUP (l->list)[i]))
        continue;

      move_psymbol (r, LOOKUP (LOOKUP (l->list)[i]));
    }
}

static void
move_objfile (struct relocation_context *r, struct objfile *o)
{
  unsigned int i;
  struct minimal_symbol *ms;
  struct partial_symtab *ps;
  struct symtab *ss;

  validate_context (r);

  if (o == NULL)
    return;

  /* RELOCATE (o->md); */

  RELOCATE (o->msymbols);

  for (ms = LOOKUP (o->msymbols); (ms != NULL) && (ms->ginfo.name != NULL);
       ms++)
    {
      move_msymbol (r, ms);
    }
  for (i = 0; i < MINIMAL_SYMBOL_HASH_SIZE; i++)
    {
      RELOCATE (o->msymbol_hash[i]);
    }
  for (i = 0; i < MINIMAL_SYMBOL_HASH_SIZE; i++)
    {
      RELOCATE (o->msymbol_demangled_hash[i]);
    }

  RELOCATE (o->section_offsets);

  gdb_assert (o->sections == NULL);
  gdb_assert (o->sections_end == NULL);

  move_obstack (r, &o->objfile_obstack);

  RELOCATE (o->psymbol_cache);
  RELOCATE (o->macro_cache);

  RELOCATE (o->sym_stab_info);
  RELOCATE (o->sym_private);

  move_psymbol_allocation_list (r, &o->global_psymbols);
  move_psymbol_allocation_list (r, &o->static_psymbols);

  RELOCATE (o->psymtabs);
  ps = LOOKUP (o->psymtabs);
  while (ps != NULL)
    {
      move_psymtab (r, ps);
      RELOCATE (ps->next);
      ps = LOOKUP (ps->next);
    }

  RELOCATE (o->symtabs);
  ss = LOOKUP (o->symtabs);
  while (ss != NULL)
    {
      move_symtab (r, ss);
      RELOCATE (ss->next);
      ss = LOOKUP (ss->next);
    }
}

#if MAPPED_SYMFILES

static void
verify (struct objfile *o, void (*f) (struct relocation_context *, void *),
        void *p)
{
  size_t start = 0;
  size_t end = 0;
  struct relocation_context r;

  if (o->md == NULL)
    error ("Objfile may not be verified if not in memory.");

  mmalloc_endpoints (o->md, &start, &end);

  r.mapped_start = start;
  r.from_start = start;
  r.to_start = 0xf0000000;
  r.len = end - start;
  r.objfile = NULL;

  relocate (&r, p, (void **) &p);
  (*f) (&r, (void *) lookup (&r, p));

  r.mapped_start = start;
  r.from_start = 0xf0000000;
  r.to_start = start;

  relocate (&r, p, (void **) &p);
  (*f) (&r, (void **) lookup (&r, p));
}

void
objfile_verify (struct objfile *o)
{
  verify (o, move_objfile, (void *) o);
}

void
symbol_verify (struct objfile *o, struct symbol *s)
{
  verify (o, move_symbol, (void *) s);
}

void
type_verify (struct objfile *o, struct type *t)
{
  verify (o, move_type, (void *) t);
}

struct objfile *
objfile_reposition (struct objfile *o, struct relocation_context *r)
{
  if (o->md == NULL)
    error ("Objfile may not be moved in memory.");

  validate_context (r);

  RELOCATE (o);
  move_objfile (r, LOOKUP (o));

  return o;
}

#endif /* MAPPED_SYMFILES */

void
_initialize_cached_symfile ()
{
  struct cmd_list_element *c;

  c =
    add_set_cmd ("cached-symfiles-check-timestamp", class_obscure,
                 var_boolean, (char *) &check_timestamp,
                 "Set if GDB should ignore cached symbol files with incorrect timestamps.",
                 &setlist);
  add_show_from_set (c, &showlist);

  add_show_from_set
    (add_set_cmd ("cached-symfile-path", class_support, var_string,
                  (char *) &cached_symfile_path,
                  "Set list of directories to search for cached symbol files.",
                  &setlist), &showlist);

  cached_symfile_path =
    xstrdup ("./gdb-symfile-cache:./syms:/usr/libexec/gdb/symfiles");

  add_show_from_set
    (add_set_cmd ("cached-symfile-dir", class_support, var_string,
                  (char *) &cached_symfile_dir,
                  "Set directory in which to generate cached symbol files.",
                  &setlist), &showlist);

  cached_symfile_dir = xstrdup ("./gdb-symfile-cache");
}
