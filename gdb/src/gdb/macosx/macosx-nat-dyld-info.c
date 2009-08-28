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
#include "macosx-nat-dyld-process.h"

#include <string.h>
#include <ctype.h>

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"
#include "ui-out.h"

const char *
dyld_reason_string (dyld_objfile_reason r)
{
  if (r == 0)
    {
      return "deallocated";
    }

  switch (r & dyld_reason_flags_mask)
    {

    case 0:
      switch (r)
        {
        case dyld_reason_user:
          return "user";
        case dyld_reason_init:
          return "init";
        case dyld_reason_executable:
          return "exec";
        case dyld_reason_dyld:
          return "dyld";
        case dyld_reason_cfm:
          return "cfm";
        default:
          return "INVALID";
        }
      break;

    case dyld_reason_cached_mask:
      switch (r & dyld_reason_type_mask)
        {
        case dyld_reason_user:
          return "c-user";
        case dyld_reason_init:
          return "c-init";
        case dyld_reason_executable:
          return "c-exec";
        case dyld_reason_dyld:
          return "c-dyld";
        case dyld_reason_cfm:
          return "c-cfm";
        default:
          return "INVALID";
        }
      break;

    case dyld_reason_weak_mask:
      switch (r & dyld_reason_type_mask)
        {
        case dyld_reason_user:
          return "w-user";
        case dyld_reason_init:
          return "w-init";
        case dyld_reason_executable:
          return "w-exec";
        case dyld_reason_dyld:
          return "w-dyld";
        case dyld_reason_cfm:
          return "w-cfm";
        default:
          return "INVALID";
        }
      break;

    case dyld_reason_cached_weak_mask:
      switch (r & dyld_reason_type_mask)
        {
        case dyld_reason_user:
          return "c-w-user";
        case dyld_reason_init:
          return "c-w-init";
        case dyld_reason_executable:
          return "c-w-exec";
        case dyld_reason_dyld:
          return "c-w-dyld";
        case dyld_reason_cfm:
          return "c-w-cfm";
        default:
          return "INVALID";
        }
      break;

    default:
      return "INVALID";
    }

  return "INVALID";
}

void
dyld_check_entry (struct dyld_objfile_entry *e)
{
}

void
dyld_objfile_entry_clear (struct dyld_objfile_entry *e)
{
  e->prefix = NULL;

  e->dyld_name = NULL;
  e->dyld_name_valid = 0;

  e->dyld_addr = 0;
  e->dyld_slide = 0;
  e->dyld_valid = 0;
  e->dyld_length = 0;
  e->dyld_section_offsets = NULL;

#if WITH_CFM
  e->cfm_container = 0;
#endif

  e->user_name = NULL;

  e->image_name = NULL;
  e->image_name_valid = 0;

  e->image_addr = 0;
  e->image_addr_valid = 0;

  e->pre_run_slide_addr = 0;
  e->pre_run_slide_addr_valid = 0;

  e->text_name = NULL;
  e->text_name_valid = 0;

  e->abfd = NULL;
  e->objfile = NULL;

  e->commpage_bfd = NULL;
  e->commpage_objfile = NULL;

  e->loaded_name = NULL;
  e->loaded_memaddr = 0;
  e->loaded_addr = 0;
  e->loaded_offset = 0;
  e->loaded_addrisoffset = 0;
  e->loaded_from_memory = 0;
  e->loaded_error = 0;

  e->load_flag = -1;

  e->reason = 0;

  /* This isn't really correct -- we should use an invalid value like -1 here
     to indicate "uninitialized" instead of making a stealthy default of 0.  */
  e->in_shared_cache = 0;

  e->allocated = 0;
}

void
dyld_objfile_info_init (struct dyld_objfile_info *i)
{
  i->entries = NULL;
  i->nents = 0;
  i->maxents = 0;
  i->sections = NULL;
  i->sections_end = NULL;
}

void
dyld_objfile_info_clear_objfiles (struct dyld_objfile_info *i)
{
  int n;

  /* Don't use DYLD_ALL_OBJFILE_INFO_ENTRIES here because we don't want
     to skip unallocated entries.  */

  for (n = 0; n < i->nents; n++)
    {
      struct dyld_objfile_entry *e = &i->entries[n];
      e->abfd = NULL;
      e->objfile = NULL;
      e->commpage_bfd = NULL;
      e->commpage_objfile = NULL;
    }
}

void
dyld_objfile_info_pack (struct dyld_objfile_info *i)
{
  int j;
  for (j = 0; j < i->nents - 1; j++)
    {
      if (!i->entries[j].allocated)
        {
          memmove (&i->entries[j], &i->entries[j + 1],
                   (i->nents - j - 1) * sizeof (struct dyld_objfile_entry));
          i->nents--;
          j--;
        }
    }
}

void
dyld_objfile_info_free (struct dyld_objfile_info *i)
{
  CHECK_FATAL (i != NULL);
  if (i->entries != NULL)
    {
      xfree (i->entries);
      i->entries = NULL;
    }
  i->nents = 0;
  i->maxents = 0;
}

int
dyld_objfile_entry_compare (struct dyld_objfile_entry *a,
                            struct dyld_objfile_entry *b)
{
#define COMPARE_SCALAR(field) { \
  if (a->field != b->field) { \
    return 1; \
  } \
}

#define COMPARE_STRING(field) { \
 if (a->field != b->field) { \
   if (a->field == NULL || b->field == NULL) { \
     return 1; \
   } else if (strcmp (a->field, b->field) != 0) { \
     return 1; \
   } \
 } \
}

  COMPARE_STRING (prefix);

  COMPARE_STRING (dyld_name);
  COMPARE_SCALAR (dyld_name_valid);

  COMPARE_SCALAR (dyld_addr);
  COMPARE_SCALAR (dyld_slide);
  COMPARE_SCALAR (dyld_valid);

#if WITH_CFM
  COMPARE_SCALAR (cfm_container);
#endif

  COMPARE_STRING (user_name);

  COMPARE_STRING (image_name);

  COMPARE_SCALAR (image_name_valid);

  COMPARE_SCALAR (image_addr);
  COMPARE_SCALAR (image_addr_valid);

  COMPARE_STRING (text_name);
  COMPARE_SCALAR (text_name_valid);

  COMPARE_SCALAR (abfd);
  COMPARE_SCALAR (objfile);

  COMPARE_SCALAR (commpage_bfd);
  COMPARE_SCALAR (commpage_objfile);

  COMPARE_STRING (loaded_name);

  COMPARE_SCALAR (loaded_memaddr);
  COMPARE_SCALAR (loaded_addr);
  COMPARE_SCALAR (loaded_offset);
  COMPARE_SCALAR (loaded_addrisoffset);
  COMPARE_SCALAR (loaded_from_memory);
  COMPARE_SCALAR (loaded_error);

  COMPARE_SCALAR (load_flag);

  COMPARE_SCALAR (reason);

  COMPARE_SCALAR (allocated);

#undef COMPARE_SCALAR
#undef COMPARE_STRING

  return 0;
}

int
dyld_objfile_info_compare (struct dyld_objfile_info *a,
                           struct dyld_objfile_info *b)
{
  int i;

  if (a->nents != b->nents)
    {
      return 1;
    }
  if (a->maxents != b->maxents)
    {
      return 1;
    }

  for (i = 0; i < a->nents; i++)
    {
      if (dyld_objfile_entry_compare (&a->entries[i], &b->entries[i]) != 0)
        {
          return 1;
        }
    }

  return 0;
}

void
dyld_objfile_info_copy_entries (struct dyld_objfile_info *d,
                                struct dyld_objfile_info *s,
                                unsigned int mask)
{
  struct dyld_objfile_entry *e, *n;
  int i;

  DYLD_ALL_OBJFILE_INFO_ENTRIES (s, e, i)
    {
      if (e->reason & mask)
        {
          n = dyld_objfile_entry_alloc (d);
          *n = *e;
	  if (e->dyld_section_offsets != NULL)
	    /* FIXME: COPY SECTION OFFSETS HERE? */
	    n->dyld_section_offsets = 0;
        }
    }
}

void
dyld_objfile_info_copy (struct dyld_objfile_info *d,
                        struct dyld_objfile_info *s)
{
  dyld_objfile_info_init (d);
  if (s->maxents == 0)
    {
      return;
    }
  d->entries = xmalloc (s->maxents * sizeof (struct dyld_objfile_entry));
  d->nents = s->nents;
  d->maxents = s->maxents;
  memcpy (d->entries, s->entries,
          s->nents * sizeof (struct dyld_objfile_entry));
}

/* Return the next free dyld_objfile_entry structure, or grow the
   array if necessary.

   FIXME:  Why not scan through the dyld_objfile_info's extant
   records and return the first allocated == 0 one instead of always
   making a new one?  It's probably not important, but the size of
   this array could probably be 1/2 to 1/3rd smaller if we did that.
   I *think* it would be OK... */

struct dyld_objfile_entry *
dyld_objfile_entry_alloc (struct dyld_objfile_info *i)
{
  struct dyld_objfile_entry *e = NULL;

  if (i->nents < i->maxents)
    {
      e = &i->entries[i->nents++];
    }
  else
    {
      i->maxents = (i->nents > 0) ? (i->nents * 2) : 16;
      if (i->entries == NULL)
        {
          i->entries =
            xmalloc (i->maxents * sizeof (struct dyld_objfile_entry));
        }
      else
        {
          i->entries =
            xrealloc (i->entries,
                      i->maxents * sizeof (struct dyld_objfile_entry));
        }
      e = &i->entries[i->nents++];
    }

  dyld_objfile_entry_clear (e);
  e->allocated = 1;

  return e;
}

/* Return the appropriate filename for the given dyld_objfile_entry.

   If TYPE is DYLD_ENTRY_FILENAME_BASE, give the base filename as
   given by dyld.
   If TYPE is DYLD_ENTRY_FILENAME_USER, return the user-specified filename,
   if there is one; else use the name given by dyld.
   If TYPE is DYLD_ENTRY_FILENAME_LOADED, return the symbol file actually
   loaded for that entry.

   All filenames are processed according to the path information
   contained in D; if the function is unable to resolve a pathname
   according to D, it will return NULL. */

const char *
dyld_entry_filename (const struct dyld_objfile_entry *e,
                     const struct dyld_path_info *d, 
                     enum dyld_entry_filename_type type)
{
  CHECK_FATAL (e != NULL);
  CHECK_FATAL (e->allocated);

  const char *name = NULL;
  char *resolved = NULL;
  int name_is_absolute = 0;

  if (e->text_name != NULL)
    {
      name = e->text_name;
      name_is_absolute = 0;
    }

  if (e->image_name != NULL)
    {
      name = e->image_name;
      name_is_absolute = 0;
    }

  if (e->dyld_name != NULL)
    {
      name = e->dyld_name;
      name_is_absolute = 1;
    }

  if ((name == NULL || type == DYLD_ENTRY_FILENAME_USER
       || type == DYLD_ENTRY_FILENAME_LOADED)
      && e->user_name != NULL)
    {
      name = e->user_name;
      name_is_absolute = 1;
    }

  if ((name == NULL || type == DYLD_ENTRY_FILENAME_LOADED)
      && e->loaded_name != NULL)
    {
      name = e->loaded_name;
      name_is_absolute = 1;
    }

  if (name == NULL)
    return NULL;

  if (d == NULL)
    return name;

  if (name_is_absolute)
    return name;


  resolved = dyld_resolve_image (d, name);
  if (resolved == NULL)
    return name;

  char buf[PATH_MAX];
  resolved = realpath (resolved, buf);
  if (resolved == NULL)
    return name;

  name = xstrdup (resolved);

  return name;
}

char *
dyld_offset_string (CORE_ADDR offset)
{
  CORE_ADDR CORE_ADDR_MAX = (CORE_ADDR) - 1;
  char *ret = NULL;
  if (offset > LONG_MAX)  /* FIXME: 64bit - I doubt this is right for a 64 bit inferior... */
    {
      xasprintf (&ret, "-0x%s", paddr_nz ((CORE_ADDR_MAX - offset) + 1));
    }
  else
    {
      xasprintf (&ret, "0x%s", paddr_nz (offset));
    }
  return ret;
}

/* Build up an English description of the dyld_objfile_entry E and
   return an xmalloc()'ed string with that text in it.  */

char *
dyld_entry_string (struct dyld_objfile_entry *e, int print_basenames)
{
  char *name;
  char *objname;
  char *symname;
  char *addr;
  char *slide;
  char *prefix;

  char *ret;
  int maxlen = 0;

  dyld_entry_info (e, print_basenames, &name, &objname, &symname, 
		   NULL, NULL, NULL,
                   &addr, &slide, &prefix);

  maxlen = 0;
  if (name != NULL)
    maxlen += strlen (name);
  if (objname != NULL)
    maxlen += strlen (objname);
  if (symname != NULL)
    maxlen += strlen (symname);
  if (addr != NULL)
    maxlen += strlen (addr);
  if (slide != NULL)
    maxlen += strlen (slide);
  if (prefix != NULL)
    maxlen += strlen (prefix);
  maxlen += 128;

  ret = (char *) xmalloc (maxlen);
  ret[0] = '\0';

  if (name == NULL)
    {
      if (addr == NULL)
        sprintf (ret + strlen (ret), "[unknown]");
      else
        sprintf (ret + strlen (ret), "[memory at %s]", addr);
    }
  else
    {
      if (addr == NULL)
        sprintf (ret + strlen (ret), "\"%s\"", name);
      else
        sprintf (ret + strlen (ret), "\"%s\" at %s", name, addr);
    }

  if (symname != NULL)
    sprintf (ret + strlen (ret), " (symbols from \"%s\")", symname);
  if (slide != NULL)
    sprintf (ret + strlen (ret), " (offset %s)", addr);
  if (prefix != NULL)
    sprintf (ret + strlen (ret), " (prefix %s)", prefix);

  xfree (name);
  xfree (objname);
  xfree (symname);
  xfree (addr);
  xfree (slide);
  xfree (prefix);

  return xstrdup (ret);
}

void
dyld_entry_info (struct dyld_objfile_entry *e, int print_basenames,
                 char **name,
                 char **objname, char **symname,
                 char **auxobjname, char **auxsymname,
		 char **dsymobjname,
                 char **addr, char **slide, char **prefix)
{
  CHECK_FATAL (e != NULL);

  if (name != NULL)
    *name = NULL;
  if (objname != NULL)
    *objname = NULL;
  if (symname != NULL)
    *symname = NULL;
  if (auxobjname != NULL)
    *auxobjname = NULL;
  if (auxsymname != NULL)
    *auxsymname = NULL;
  if (dsymobjname != NULL)
    *dsymobjname = NULL;
  *addr = NULL;
  *slide = NULL;
  *prefix = NULL;

  if (e->objfile && e->loaded_from_memory)
    {
      CHECK_FATAL (!e->loaded_addrisoffset);
      CHECK_FATAL (e->loaded_addr == e->loaded_memaddr);
      if (e->image_addr_valid)
        {
          *slide = dyld_offset_string (e->loaded_memaddr - e->image_addr);
          xasprintf (addr, "0x%s", paddr_nz (e->loaded_memaddr));
        }
      else
        {
          xasprintf (addr, "0x%s", paddr_nz (e->loaded_memaddr));
        }
    }

  if (e->objfile)
    {
      const char *loaded_name =
        dyld_entry_filename (e, NULL, DYLD_ENTRY_FILENAME_BASE);
      const char *loaded_objname =
        dyld_entry_filename (e, NULL, DYLD_ENTRY_FILENAME_LOADED);
      const char *loaded_symname =
        (e->objfile != NULL) ? e->objfile->name : NULL;
      const char *loaded_auxobjname =
        (e->commpage_objfile != NULL) ? e->commpage_objfile->name : NULL;
      const char *loaded_auxsymname =
        (e->commpage_objfile != NULL) ? e->commpage_objfile->name : NULL;
      const char *loaded_dsymobjname =
        (e->objfile->separate_debug_objfile != NULL) ? e->objfile->separate_debug_objfile->name : NULL;

      /* If we're printing only the basenames of paths, point to the base
         name in every non-NULL string.  */

      if (!print_basenames && loaded_name != NULL)
        if (strrchr (loaded_name, '/') != NULL)
          loaded_name = strrchr (loaded_name, '/') + 1;

      if (!print_basenames && loaded_objname != NULL)
        if (strrchr (loaded_objname, '/') != NULL)
          loaded_objname = strrchr (loaded_objname, '/') + 1;

      if (!print_basenames && loaded_symname != NULL)
        if (strrchr (loaded_symname, '/') != NULL)
          loaded_symname = strrchr (loaded_symname, '/') + 1;

      if (!print_basenames && loaded_auxobjname != NULL)
        if (strrchr (loaded_auxobjname, '/') != NULL)
          loaded_auxobjname = strrchr (loaded_auxobjname, '/') + 1;

      if (!print_basenames && loaded_auxsymname != NULL)
        if (strrchr (loaded_auxsymname, '/') != NULL)
          loaded_auxsymname = strrchr (loaded_auxsymname, '/') + 1;

      if (!print_basenames && loaded_dsymobjname != NULL)
        if (strrchr (loaded_dsymobjname, '/') != NULL)
          loaded_dsymobjname = strrchr (loaded_dsymobjname, '/') + 1;

      /* Copy over all non-NULL strings to our arguments, if they were
         provided.  */

      if (loaded_name != NULL && name != NULL)
        {
          int namelen = strlen (loaded_name) + 1;
          *name = (char *) xmalloc (namelen);
          memcpy (*name, loaded_name, namelen);
        }

      if (loaded_objname != NULL && objname != NULL)
        {
          int namelen = strlen (loaded_objname) + 1;
          *objname = (char *) xmalloc (namelen);
          memcpy (*objname, loaded_objname, namelen);
        }

      if (loaded_symname != NULL && symname != NULL)
        {
          int namelen = strlen (loaded_symname) + 1;
          *symname = (char *) xmalloc (namelen);
          memcpy (*symname, loaded_symname, namelen);
        }

      if (loaded_auxobjname != NULL && auxobjname != NULL)
        {
          int namelen = strlen (loaded_auxobjname) + 1;
          *auxobjname = (char *) xmalloc (namelen);
          memcpy (*auxobjname, loaded_auxobjname, namelen);
        }

      if (loaded_auxsymname != NULL && auxsymname != NULL)
        {
          int namelen = strlen (loaded_auxsymname) + 1;
          *auxsymname = (char *) xmalloc (namelen);
          memcpy (*auxsymname, loaded_auxsymname, namelen);
        }

      if (loaded_dsymobjname != NULL && dsymobjname != NULL)
        {
          int namelen = strlen (loaded_dsymobjname) + 1;
          *dsymobjname = (char *) xmalloc (namelen);
          memcpy (*dsymobjname, loaded_dsymobjname, namelen);
        }

      if (e->loaded_addrisoffset)
        {
          if (e->image_addr_valid)
            {
              *slide = dyld_offset_string (e->loaded_offset);
              xasprintf (addr, "0x%s", paddr_nz (e->image_addr));
            }
          else
            {
              *slide = dyld_offset_string (e->loaded_offset);
            }
        }
      else
        {
          if (e->dyld_valid)
            {
              *slide = dyld_offset_string (e->dyld_slide);
              xasprintf (addr, "0x%s", paddr_nz (e->loaded_addr));
            }
          else
            {
              if (e->pre_run_slide_addr_valid && e->pre_run_slide_addr != 0)
                {
                  *slide = dyld_offset_string (e->pre_run_slide_addr);
                  xasprintf (addr, "0x%s", paddr_nz (e->image_addr + e->pre_run_slide_addr));
                }
              else if (e->image_addr_valid)
                {
                  *slide =
                    dyld_offset_string (e->loaded_addr - e->image_addr);
                  xasprintf (addr, "0x%s", paddr_nz (e->loaded_addr));
                }
              else
                {
                  xasprintf (addr, "0x%s", paddr_nz (e->loaded_addr));
                }
            }
        }

    }

  if (e->objfile == NULL && !e->loaded_from_memory)
    {
      const char *s;
      const char *tmp;
      int namelen;
      s = dyld_entry_filename (e, NULL, 0);
      if (s == NULL)
        {
          s = "[UNKNOWN]";
        }
      if (!print_basenames)
        {
          tmp = strrchr (s, '/');
          if (tmp == NULL)
            {
              tmp = s;
            }
          else
            {
              tmp++;
            }
        }
      else
        {
          tmp = s;
        }

      if (tmp != NULL)
        {
          namelen = strlen (tmp) + 1;
          *name = xmalloc (namelen);
          memcpy (*name, tmp, namelen);
        }
    }

  if (e->prefix != NULL && e->prefix[0] != '\0')
    {
      int prefixlen = strlen (e->prefix) + 1;
      *prefix = xmalloc (prefixlen);
      memcpy (*prefix, e->prefix, prefixlen);
    }

}

int
dyld_resolve_shlib_num (struct dyld_objfile_info *s,
                        int num,
                        struct dyld_objfile_entry **eptr,
                        struct objfile **optr)
{
  struct objfile *objfile;
  struct objfile *temp;
  int i;

  CHECK_FATAL (eptr != NULL);
  CHECK_FATAL (optr != NULL);

  *eptr = NULL;
  *optr = NULL;

  for (i = 0; i < s->nents; i++)
    {
      struct dyld_objfile_entry *j = &s->entries[i];

      if (!j->allocated)
        continue;

      num--;

      if (num == 0)
        {
          *eptr = j;
          *optr = j->objfile;
          return 0;
        }
    }

  ALL_OBJFILES_SAFE (objfile, temp)
  {

    int found = 0;

    for (i = 0; i < s->nents; i++)
      {
        struct dyld_objfile_entry *j = &s->entries[i];
        if (!j->allocated)
          {
            continue;
          }
        if (j->objfile == objfile || j->commpage_objfile == objfile)
          {
            found = 1;
          }
      }

    if (!found)
      {
        num--;
      }

    if (num == 0)
      {
        *eptr = NULL;
        *optr = objfile;
        return 0;
      }
  }

  return -1;
}

/* Returns the shared library number of the entry at EPTR in
   NUMPTR.  Returns 0 on success, anything else on failure. */

int
dyld_entry_shlib_num (struct dyld_objfile_info *s,
                      struct dyld_objfile_entry *eptr, int *numptr)
{
  int i;
  struct dyld_objfile_entry *j;
  CHECK_FATAL (numptr != NULL);

  DYLD_ALL_OBJFILE_INFO_ENTRIES(s, j, i)
    {
      if (j == eptr)
        {
          *numptr = i + 1;
          return 0;
        }
    }

  *numptr = 0;
  return -1;
}

/* Returns the length of the longest field that would be printed when
   displaying 's' according to 'reason_mask'. */

int
dyld_shlib_info_basename_length (struct dyld_objfile_info *s,
                                 unsigned int reason_mask)
{
  int i;
  int baselen = 0;
  struct objfile *objfile, *temp;
  struct dyld_objfile_entry *j;

  DYLD_ALL_OBJFILE_INFO_ENTRIES (s, j, i)
    {
      const char *name = NULL;
      const char *tfname = NULL;
      int tfnamelen = 0;

      if (!(j->reason & reason_mask))
        {
          continue;
        }

      name = dyld_entry_filename (j, NULL, 0);
      if (name == NULL)
        {
          if (baselen < 1)
            {
              baselen = 1;
            }
        }
      else
        {
          dyld_library_basename (name, &tfname, &tfnamelen, NULL, NULL);
          if (baselen < tfnamelen)
            {
              baselen = tfnamelen;
            }
          xfree ((char *) tfname);
        }
    }

  if (!(reason_mask & dyld_reason_user))
    {
      return baselen;
    }

  ALL_OBJFILES_SAFE (objfile, temp)
  {

    const char *name = NULL;
    const char *tfname = NULL;
    int tfnamelen = 0;
    struct dyld_objfile_entry *j;

    int found = 0;

    DYLD_ALL_OBJFILE_INFO_ENTRIES (s, j, i)
      {
        if (j->objfile == objfile || j->commpage_objfile == objfile)
          {
            found = 1;
          }
      }

    if (!found)
      {

        struct dyld_objfile_entry tentry;
        dyld_convert_entry (objfile, &tentry);

        name = dyld_entry_filename (&tentry, NULL, 0);
        if (name == NULL)
          {
            if (baselen < 1)
              {
                baselen = 1;
              }
          }
        else
          {
            dyld_library_basename (name, &tfname, &tfnamelen, NULL, NULL);
            if (baselen < tfnamelen)
              {
                baselen = tfnamelen;
              }
            xfree ((char *) tfname);
          }
      }
  }

  return baselen;
}

/* Return true if and only if the shared library specifier 'shlibnum'
   matches the string in 'args'.  Returns 'false' on error; outputs
   warning messages only if 'verbose' is passed as true. */

int
dyld_entry_shlib_num_matches (int shlibnum, char *args, int verbose)
{
  char *p, *p1;
  int num;

  if (args == NULL)
    error_no_arg ("one or more shlib numbers");

  if (args[0] == '\0')
    error_no_arg ("one or more shlib numbers");

  p = args;
  while (*p)
    {
      p1 = p;

      num = get_number_or_range (&p1);
      if (num == 0)
        {
          if (verbose)
            warning ("bad shlib number at or near '%s'", p);
          return 0;
        }

      if (num == shlibnum)
        return 1;

      p = p1;
    }

  return 0;
}

void
dyld_print_entry_info (struct dyld_objfile_entry *j, int shlibnum, int baselen)
{
  char *name;
  char *objname;
  char *symname;
  char *auxobjname;
  char *auxsymname;
  char *dsymobjname;
  char *addr;
  char *slide;
  char *prefix;

  const char *tfname = NULL;
  int tfnamelen = 0;
  int is_framework, is_bundle;
  char *fname = NULL;
  char addrbuf[24];
  const char *ptr;
  struct cleanup *list_cleanup;

  dyld_entry_info (j, 1, &name, &objname, &symname, 
		   &auxobjname, &auxsymname, &dsymobjname,
                   &addr, &slide, &prefix);

  if (name == NULL)
    {
      fname = savestring ("-", strlen ("-"));
      is_framework = 0;
      is_bundle = 0;
    }
  else
    {
      dyld_library_basename (name, &tfname, &tfnamelen, &is_framework,
                             &is_bundle);
      fname = savestring (tfname, tfnamelen);
      xfree ((char *) tfname);
    }

  if (j->dyld_valid)
    {
      snprintf (addrbuf, 24, "0x%s", paddr_nz (j->dyld_addr));
    }
  else
    {
      strcpy (addrbuf, "-");
    }

  if (baselen < strlen (fname))
    {
      baselen = strlen (fname);
    }

  list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "shlib-info");
  if (shlibnum < 10)
    ui_out_text (uiout, "  ");
  else if (shlibnum < 100)
    ui_out_text (uiout, " ");

  ui_out_field_int (uiout, "num", shlibnum);
  ui_out_spaces (uiout, 1);

  ui_out_field_string (uiout, "name", fname);
  ui_out_spaces (uiout, baselen - strlen (fname) + 1);

  ptr = is_framework ? "F" : (is_bundle ? "B" : "-");
  ui_out_field_string (uiout, "kind", ptr);
  if (strlen (ptr) == 0)
    ui_out_spaces (uiout, 1);

  ui_out_spaces (uiout, 1);

  ui_out_field_string (uiout, "dyld-addr", addrbuf);
  /* For a 64-bit program, the number 10 here is not correct.  
     I don't want to change the formatting for all 32-bit but
     make sure ui_out_spaces gets a non-negative value.  */
  if (strlen (addrbuf) < 10)
    ui_out_spaces (uiout, 10 - strlen (addrbuf));
  ui_out_spaces (uiout, 1);

  ptr = dyld_reason_string (j->reason);
  /* For a 64-bit program, the number 11 here is not correct.  
     I don't want to change the formatting for all 32-bit but
     make sure ui_out_spaces gets a non-negative value.  */
  if (strlen (ptr) < 11)
    ui_out_spaces (uiout, 11 - strlen (ptr));
  ui_out_field_string (uiout, "reason", ptr);
  ui_out_spaces (uiout, 1);

  if ((j->load_flag & OBJF_SYM_LEVELS_MASK) == OBJF_SYM_ALL)
    {
      ptr = "Y";
    }
  else if ((j->load_flag & OBJF_SYM_LEVELS_MASK) == OBJF_SYM_NONE)
    {
      ptr = "N";
    }
  else if ((j->load_flag & OBJF_SYM_LEVELS_MASK) == OBJF_SYM_EXTERN)
    {
      ptr = "E";
    }
  else if ((j->load_flag & OBJF_SYM_LEVELS_MASK) == OBJF_SYM_CONTAINER)
    {
      ptr = "C";
    }
  else
    {
      ptr = "?";
    }

  ui_out_field_string (uiout, "requested-state", ptr);
  ui_out_spaces (uiout, 1);

  if (j->loaded_error)
    {
      ptr = "!";
    }
  else
    {
      if (j->objfile != NULL)
        {
          if (!dyld_objfile_allocated (j->objfile))
            {
              ptr = "!";
            }
          else if ((j->objfile->symflags & OBJF_SYM_LEVELS_MASK) == OBJF_SYM_ALL)
            {
              ptr = "Y";
            }
          else if ((j->objfile->symflags & OBJF_SYM_LEVELS_MASK) == OBJF_SYM_NONE)
            {
              ptr = "N";
            }
          else if ((j->objfile->symflags & OBJF_SYM_LEVELS_MASK) == OBJF_SYM_EXTERN)
            {
              ptr = "E";
            }
	  else if ((j->load_flag & OBJF_SYM_LEVELS_MASK) == OBJF_SYM_CONTAINER)
	    {
	      ptr = "C";
	    }
          else
            {
              ptr = "?";
            }
        }
      else if (j->abfd != NULL)
        {
          ptr = "B";
        }
      else
        {
          ptr = "N";
        }
    }

  ui_out_field_string (uiout, "state", ptr);
  ui_out_spaces (uiout, 1);

  if (ui_out_is_mi_like_p (uiout))
    ui_out_field_string (uiout, "path", (symname != NULL) ? symname : "");

  if (name == NULL)
    {
      if (ui_out_is_mi_like_p (uiout))
        {
          char *s = dyld_entry_string (j, 1);
          ui_out_field_string (uiout, "description", s);
          xfree (s);
        }
      else
        ui_out_field_skip (uiout, "description");

      if (addr != NULL)
        {
          ui_out_text (uiout, "[memory at ");
          ui_out_field_string (uiout, "loaded_addr", addr);

          if (slide != NULL)
            {
              ui_out_text (uiout, "] (offset ");
              ui_out_field_string (uiout, "slide", slide);
              ui_out_text (uiout, ")");
            }
          else
            {
              ui_out_field_skip (uiout, "slide");
              ui_out_text (uiout, "]");
            }
        }
      else
        {
          ui_out_field_skip (uiout, "loaded_addr");
          if (slide == NULL)
            ui_out_field_skip (uiout, "slide");
          else
            {
              ui_out_text (uiout, "(offset ");
              ui_out_field_string (uiout, "slide", slide);
              ui_out_text (uiout, ")");
            }
        }
    }
  else
    {
      ui_out_field_string (uiout, "description", name);

      if (slide == NULL)
        {
          ui_out_field_skip (uiout, "slide");
          if (addr == NULL)
            ui_out_field_skip (uiout, "addr");
          else
            {
              ui_out_text (uiout, " at ");
              ui_out_field_string (uiout, "loaded_addr", addr);
            }
        }
      else
        {
          if (addr == NULL)
            {
              ui_out_field_skip (uiout, "loaded_addr");
              ui_out_text (uiout, " (offset ");
              ui_out_field_string (uiout, "slide", slide);
              ui_out_text (uiout, ")");
            }
          else
            {
              ui_out_text (uiout, " at ");
              ui_out_field_string (uiout, "loaded_addr", addr);
              ui_out_text (uiout, " (offset ");
              ui_out_field_string (uiout, "slide", slide);
              ui_out_text (uiout, ")");
            }
        }

      if (prefix == NULL)
        {
          ui_out_field_skip (uiout, "prefix");
        }
      else
        {
          ui_out_text (uiout, " with prefix \"");
          ui_out_field_string (uiout, "prefix", prefix);
          ui_out_text (uiout, "\"");
        }
    }

  if (objname != NULL)
    if (name == NULL || strcmp (name, objname) != 0)
      {
        const char *tag = "(objfile is)";
        ui_out_text (uiout, "\n");
        ui_out_spaces (uiout, baselen + 34 - strlen (tag) - 1);
        ui_out_text (uiout, tag);
        ui_out_text (uiout, " ");
        ui_out_field_string (uiout, "objpath", objname);
      }

  if (symname != NULL)
    if (objname == NULL || strcmp (objname, symname) != 0)
      {
        const char *tag = "(symbols read from)";
        ui_out_text (uiout, "\n");
        ui_out_spaces (uiout, baselen + 34 - strlen (tag) - 1);
        ui_out_text (uiout, tag);
        ui_out_text (uiout, " ");
        ui_out_field_string (uiout, "sympath", symname);
      }

  if (auxobjname != NULL)
    {
      const char *tag = "(commpage objfile is)";
      ui_out_text (uiout, "\n");
      ui_out_spaces (uiout, baselen + 34 - strlen (tag) - 1);
      ui_out_text (uiout, tag);
      ui_out_text (uiout, " ");
      ui_out_field_string (uiout, "commpage-objpath", auxobjname);
    }

  if (auxsymname != NULL)
    if (auxobjname == NULL || strcmp (auxobjname, auxsymname) != 0)
      {
        const char *tag = "(commpage symbols read from)";
        ui_out_text (uiout, "\n");
        ui_out_spaces (uiout, baselen + 34 - strlen (tag) - 1);
        ui_out_text (uiout, tag);
        ui_out_text (uiout, " ");
        ui_out_field_string (uiout, "commpage-sympath", auxsymname);
      }

  if (dsymobjname != NULL)
    {
      const char *tag = "(dSYM file is)";
      ui_out_text (uiout, "\n");
      ui_out_spaces (uiout, baselen + 34 - strlen (tag) - 1);
      ui_out_text (uiout, tag);
      ui_out_text (uiout, " ");
      ui_out_field_string (uiout, "dsym-objpath", dsymobjname);
    }

  do_cleanups (list_cleanup);

  ui_out_text (uiout, "\n");

  xfree (name);
  xfree (objname);
  xfree (symname);
  xfree (auxobjname);
  xfree (auxsymname);
  xfree (dsymobjname);
  xfree (addr);
  xfree (slide);
  xfree (prefix);
}

void
dyld_convert_entry (struct objfile *o, struct dyld_objfile_entry *e)
{
  dyld_objfile_entry_clear (e);

  e->allocated = 1;
  e->reason = dyld_reason_user;

  e->objfile = o;
  /* No need to set e->abfd, since e->objfile is present. */

  e->user_name = e->objfile->name;
  e->loaded_name = o->name;
}

void
dyld_print_shlib_info (struct dyld_objfile_info *s, unsigned int reason_mask,
                       int header, char *args)
{
  int baselen = 0;
  char *basepad = NULL;
  int shlibnum = 0;
  struct objfile *objfile, *temp;
  int i;

  baselen = dyld_shlib_info_basename_length (s, reason_mask);
  if (baselen < 12)
    {
      baselen = 12;
    }

  basepad = xmalloc (baselen + 1);
  memset (basepad, ' ', baselen);
  basepad[baselen] = '\0';

  if (header)
    ui_out_text_fmt (uiout,
                     "%s                            Requested State Current State\n"
                     "Num Basename%s  Type Address         Reason | | Source     \n"
                     "  | |%s            | |                    | | | |          \n",
                     basepad + 12, basepad + 12, basepad + 12);

  if (args != NULL)
    dyld_entry_shlib_num_matches (-1, args, 1);

  /* First, print all objfiles managed by the dyld_objfile_entry code, in
     order. */

  /* Be sure to keep the order of output here synchronized with
     update_section_tables (). */

  for (i = 0; i < s->nents; i++)
    {
      struct dyld_objfile_entry *j = &s->entries[i];

      if (!j->allocated)
        {
          if (reason_mask & dyld_reason_deallocated)
            printf_filtered ("[DEALLOCATED]\n");
          continue;
        }

      shlibnum++;

      if (!(j->reason & reason_mask))
        {
          continue;
        }

      if ((args == NULL) || dyld_entry_shlib_num_matches (shlibnum, args, 0))
        {
          dyld_print_entry_info (j, shlibnum, baselen);
        }
    }

  /* Then, print all the remaining objfiles. */

  ALL_OBJFILES_SAFE (objfile, temp)
    {
      int found = 0;
      struct dyld_objfile_entry *j;
      
      /* Don't print out the dSYM files here.  They are printed with
	 the objfile in dyld_print_entry_info above.  */
      
      if (objfile->separate_debug_objfile_backlink != NULL)
	{
	  found = 1;
	}
      else
	{
	  DYLD_ALL_OBJFILE_INFO_ENTRIES (s, j, i)
	    {
	      if (j->objfile == objfile || j->commpage_objfile == objfile)
		{
		  found = 1;
		}
	    }
	}

      if (!found)
	{
	  
	  struct dyld_objfile_entry tentry;
	  shlibnum++;
	  
	  if (!(reason_mask & dyld_reason_user))
	    {
	      continue;
	    }
	  
	  if (args == NULL || dyld_entry_shlib_num_matches (shlibnum, args, 0))
	    {
	      dyld_convert_entry (objfile, &tentry);
	      dyld_print_entry_info (&tentry, shlibnum, baselen);
	    }
	}
    }
}

/* There is one struct dyld_objfile_info per program.  Within INFO,
   each dylib/bundle/executable/etc we know about has an entry in
   an array of dyld_objfile_entry structures.

   This function provides the numeric offsets of each member of the array,
   skipping over unallocated entries that might be in the middle (huh?
   whatever.)  If you call it with N having a value of 5, you can get
   back 5 or a higher number as it skips over unallocated entries.  */

int
dyld_next_allocated_shlib (struct dyld_objfile_info *info, int n)
{
  for (;;)
    {
      if (n >= info->nents)
        return n;
      if (info->entries[n].allocated)
        return n;
      n++;
    }
}

