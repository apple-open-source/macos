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

#include "macosx-nat-dyld-info.h"
#include "macosx-nat-dyld-path.h"

#include <string.h>
#include <ctype.h>

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"

const char *dyld_reason_string (dyld_objfile_reason r)
{
  if (r == 0) {
  return "deallocated";
  }

  switch (r & dyld_reason_flags_mask) {

  case 0:
    switch (r) {
    case dyld_reason_user: return "user";
    case dyld_reason_init: return "init";
    case dyld_reason_executable: return "exec";
    case dyld_reason_dyld: return "dyld";
    case dyld_reason_cfm: return "cfm";
    default: return "INVALID";
    }
    break;

  case dyld_reason_cached_mask:
    switch (r & dyld_reason_type_mask) {
    case dyld_reason_user: return "c-user";
    case dyld_reason_init: return "c-init";
    case dyld_reason_executable: return "c-exec";
    case dyld_reason_dyld: return "c-dyld";
    case dyld_reason_cfm: return "c-cfm";
    default: return "INVALID";
    }
    break;

  case dyld_reason_weak_mask:
    switch (r & dyld_reason_type_mask) {
    case dyld_reason_user: return "w-user";
    case dyld_reason_init: return "w-init";
    case dyld_reason_executable: return "w-exec";
    case dyld_reason_dyld: return "w-dyld";
    case dyld_reason_cfm: return "w-cfm";
    default: return "INVALID";
    }
    break;

  case dyld_reason_cached_weak_mask:
    switch (r & dyld_reason_type_mask) {
    case dyld_reason_user: return "c-w-user";
    case dyld_reason_init: return "c-w-init";
    case dyld_reason_executable: return "c-w-exec";
    case dyld_reason_dyld: return "c-w-dyld";
    case dyld_reason_cfm: return "c-w-cfm";
    default: return "INVALID";
    }
    break;

  default: return "INVALID";
  }   

  return "INVALID";
} 

void dyld_check_entry (struct dyld_objfile_entry *e)
{
}

void dyld_objfile_entry_clear (struct dyld_objfile_entry *e)
{
  e->prefix = NULL;

  e->dyld_name = NULL;
  e->dyld_name_valid = 0;

  e->dyld_addr = 0;
  e->dyld_slide = 0;
  e->dyld_index = 0;
  e->dyld_valid = 0;
  e->dyld_length = 0;

  e->cfm_container = 0;

  e->user_name = NULL;

  e->image_name = NULL;
  e->image_name_valid = 0;

  e->image_addr = 0;
  e->image_addr_valid = 0;
  
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

  e->allocated = 0;
}

void dyld_objfile_info_init (struct dyld_objfile_info *i)
{
  i->entries = NULL;
  i->nents = 0;
  i->maxents = 0;
  i->sections = NULL;
  i->sections_end = NULL;
}

void dyld_objfile_info_pack (struct dyld_objfile_info *i)
{
  unsigned int j;
  for (j = 0; j < i->nents; j++) {
    if (! i->entries[j].allocated) {
      memmove (&i->entries[j], &i->entries[j + 1], (i->nents - j) * sizeof (struct dyld_objfile_entry));
      i->nents--;
      j--;
    }
  }
}

void dyld_objfile_info_free (struct dyld_objfile_info *i)
{
  CHECK_FATAL (i != NULL);
  if (i->entries != NULL) { 
    xfree (i->entries);
    i->entries = NULL;
  }
  i->nents = 0;
  i->maxents = 0;
}

int dyld_objfile_entry_compare (struct dyld_objfile_entry *a, struct dyld_objfile_entry *b)
{
#define COMPARE_SCALAR(field) { \
  if (a->field != b->field) { \
    return 1; \
  } \
}

#define COMPARE_STRING(field) { \
 if (a->field != b->field) { \
   if ((a->field == NULL) || (b->field == NULL)) { \
     return 1; \
   } else if (strcmp (a->field, b->field) != 0) { \
     return 1; \
   } \
 } \
}

  COMPARE_STRING(prefix);

  COMPARE_STRING(dyld_name);
  COMPARE_SCALAR(dyld_name_valid);

  COMPARE_SCALAR(dyld_addr);
  COMPARE_SCALAR(dyld_slide);
  COMPARE_SCALAR(dyld_index);
  COMPARE_SCALAR(dyld_valid);

  COMPARE_SCALAR(cfm_container);

  COMPARE_STRING(user_name);

  COMPARE_STRING(image_name);

  COMPARE_SCALAR(image_name_valid);

  COMPARE_SCALAR(image_addr);
  COMPARE_SCALAR(image_addr_valid);

  COMPARE_STRING(text_name);
  COMPARE_SCALAR(text_name_valid);

  COMPARE_SCALAR(abfd);
  COMPARE_SCALAR(objfile);

  COMPARE_SCALAR(commpage_bfd);
  COMPARE_SCALAR(commpage_objfile);

  COMPARE_STRING(loaded_name);

  COMPARE_SCALAR(loaded_memaddr);
  COMPARE_SCALAR(loaded_addr);
  COMPARE_SCALAR(loaded_offset);
  COMPARE_SCALAR(loaded_addrisoffset);
  COMPARE_SCALAR(loaded_from_memory);
  COMPARE_SCALAR(loaded_error);

  COMPARE_SCALAR(load_flag);
  
  COMPARE_SCALAR(reason);

  COMPARE_SCALAR(allocated);

#undef COMPARE_SCALAR
#undef COMPARE_STRING

  return 0;
}

int dyld_objfile_info_compare (struct dyld_objfile_info *a, struct dyld_objfile_info *b)
{
  unsigned int i;
  
  if (a->nents != b->nents) { return 1; }
  if (a->maxents != b->maxents) { return 1; }
  
  for (i = 0; i < a->nents; i++) {
    if (dyld_objfile_entry_compare (&a->entries[i], &b->entries[i]) != 0) {
      return 1;
    }
  }

  return 0;
}

void
dyld_objfile_info_copy_entries (struct dyld_objfile_info *d, struct dyld_objfile_info *s, unsigned int mask)
{
  struct dyld_objfile_entry *e, *n;
  unsigned int i;

  for (i = 0; i < s->nents; i++) {
    e = &s->entries[i];
    if (! e->allocated) {
      continue;
    }
    if (e->reason & mask) {
      n = dyld_objfile_entry_alloc (d);
      *n = *e;
    }
  }
}

void dyld_objfile_info_copy (struct dyld_objfile_info *d, struct dyld_objfile_info *s)
{
  dyld_objfile_info_init (d);
  if (s->maxents == 0) {
    return;
  }
  d->entries = xmalloc (s->maxents * sizeof (struct dyld_objfile_entry));
  d->nents = s->nents;
  d->maxents = s->maxents;
  memcpy (d->entries, s->entries, s->nents * sizeof (struct dyld_objfile_entry));
}

struct dyld_objfile_entry *dyld_objfile_entry_alloc (struct dyld_objfile_info *i)
{
  struct dyld_objfile_entry *e = NULL;

  if (i->nents < i->maxents) { 
    e = &i->entries[i->nents++];
  } else {
    i->maxents = (i->nents > 0) ? (i->nents * 2) : 16;
    if (i->entries == NULL) {
      i->entries = xmalloc (i->maxents * sizeof (struct dyld_objfile_entry));
    } else {
      i->entries = xrealloc (i->entries, i->maxents * sizeof (struct dyld_objfile_entry));
    }
    e = &i->entries[i->nents++];
  }

  dyld_objfile_entry_clear (e);
  e->allocated = 1;

  return e;
}

const char *dyld_entry_filename
(const struct dyld_objfile_entry *e, const struct dyld_path_info *d, int type)
{
  CHECK_FATAL (e != NULL);
  CHECK_FATAL (e->allocated);

  char *name = NULL;
  int name_is_absolute = 0;

  if ((type & DYLD_ENTRY_FILENAME_LOADED) && (e->loaded_name != NULL)) {
    name = e->loaded_name;
    name_is_absolute = 1;
  } else if (e->user_name != NULL) {
    name = e->user_name;
    name_is_absolute = 1;
  } else if (e->dyld_name != NULL) {
    name = e->dyld_name;
    name_is_absolute = 1;
  } else if (e->image_name != NULL) {
    name = e->image_name;
    name_is_absolute = 0;
  } else if (e->text_name != NULL) {
    name = e->text_name;
    name_is_absolute = 0;
  } else {
    name = NULL;
    name_is_absolute = 0;
  }

  if (name == NULL)
    return NULL;

  if (d != NULL)
    {
      char *resolved = NULL;

      if (name_is_absolute) 
	resolved = xstrdup (name);
      else
	resolved = dyld_resolve_image (d, name);
      
      return resolved;
    }
  else
      return name;
}

char *dyld_offset_string (unsigned long offset)
{
  char *ret = NULL;
  if (offset > LONG_MAX) {
    xasprintf (&ret, "-0x%lx", ((ULONG_MAX - offset) + 1));
  } else {
    xasprintf (&ret, "0x%lx", offset);
  }
  return ret;
}

char *dyld_entry_string (struct dyld_objfile_entry *e, int print_basenames)
{
  char *name;
  char *objname;
  char *symname;
  char *addr;
  char *slide;
  char *prefix;

  char *ret;
  unsigned int maxlen = 0;

  dyld_entry_info (e, print_basenames, &name, &objname, &symname, NULL, NULL, &addr, &slide, &prefix);

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

  if (name == NULL) {
    if (addr == NULL)
      sprintf (ret + strlen (ret), "[unknown]");
    else
      sprintf (ret + strlen (ret), "[memory at %s]", addr);
  } else {
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

void dyld_entry_out (struct ui_out *uiout, struct dyld_objfile_entry *e, int print_basenames)
{
  char *name;
  char *objname;
  char *symname;
  char *addr;
  char *slide;
  char *prefix;

  dyld_entry_info (e, print_basenames, &name, &objname, NULL, NULL, &symname, &addr, &slide, &prefix);

  if (name == NULL)
    {
      if (ui_out_is_mi_like_p (uiout))
	{
	  const char *name = dyld_entry_filename (e, NULL, 0);
	  if (name != NULL)
	    {
	      ui_out_field_string (uiout, "path", name);
	    }
	  else
	    {
	      char *s = dyld_entry_string (e, print_basenames);
	      ui_out_field_string (uiout, "path", s);
	      xfree (s);
	    }
	}
      else
	ui_out_field_skip (uiout, "path");

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
      ui_out_field_string (uiout, "path", name);

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

  xfree (name);
  xfree (objname);
  xfree (symname);
  xfree (addr);
  xfree (slide);
  xfree (prefix);
}

/* Fetch a number of strings useful for describing the
   dyld_objfile_entry 'e'.  The returned strings will be allocated
   with xmalloc; if a parameter is passed in as NULL, no string will
   be returned.  If a given string is inapplicable, a null pointer
   will be returned.  The following strings are provided,:

     name        - the name of the image, as specified by DYLD

     objname     - the name of the file on disk that GDB is using for
                   symbol-reading purposes
     
     symname     - the name of the cached symbol file being used, if any

     commobjname - the name of the file on disk that GDB is using for
                   commpage data for that image

     commsymname - the name of the cached symbol file being used for
                   commpage data, if any
*/

void 
dyld_entry_info (struct dyld_objfile_entry *e, int print_basenames, 
		 char **name, char **objname, char **symname,
		 char **commobjname, char **commsymname,
		 char **addr, char **slide, char **prefix)
{
  CHECK_FATAL (e != NULL);

  if (name != NULL)
    *name = NULL;
  if (objname != NULL)
    *objname = NULL;
  if (symname != NULL)
    *symname = NULL;
  if (commobjname != NULL)
    *commobjname = NULL;
  if (commsymname != NULL)
    *commsymname = NULL;
  if (addr != NULL)
    *addr = NULL;
  if (slide != NULL)
    *slide = NULL;
  if (prefix != NULL)
    *prefix = NULL;

  if (e->objfile && e->loaded_from_memory) {

    CHECK_FATAL (! e->loaded_addrisoffset);
    CHECK_FATAL (e->loaded_addr == e->loaded_memaddr);
    if (e->image_addr_valid) {
      if (slide != NULL)
	*slide = dyld_offset_string ((unsigned long) (e->loaded_memaddr - e->image_addr));
      if (addr != NULL)
	xasprintf (addr, "0x%lx", (unsigned long) e->loaded_memaddr);
    } else {
      if (addr != NULL)
	xasprintf (addr, "0x%lx", (unsigned long) e->loaded_memaddr);
    }	      

  } else if (e->objfile && !e->loaded_from_memory) {

    const char *loaded_name = e->loaded_name;
    const char *loaded_objname = dyld_entry_filename (e, NULL, 0);
    const char *loaded_symname = (e->objfile != NULL) ? e->objfile->name : NULL;
    const char *loaded_commobjname = (e->commpage_bfd != NULL) ? e->commpage_bfd->filename : NULL;
    const char *loaded_commsymname = (e->commpage_objfile != NULL) ? e->commpage_objfile->name : NULL;

    if ((! print_basenames) && (loaded_name != NULL))
      if (strrchr (loaded_name, '/') != NULL)
	loaded_name = strrchr (loaded_name, '/') + 1;

    if ((! print_basenames) && (loaded_objname != NULL))
      if (strrchr (loaded_objname, '/') != NULL)
	loaded_objname = strrchr (loaded_objname, '/') + 1;

    if ((! print_basenames) && (loaded_symname != NULL))
      if (strrchr (loaded_symname, '/') != NULL)
	loaded_symname = strrchr (loaded_symname, '/') + 1;
      
    if ((! print_basenames) && (loaded_commobjname != NULL))
      if (strrchr (loaded_commobjname, '/') != NULL)
	loaded_commobjname = strrchr (loaded_commobjname, '/') + 1;

    if ((! print_basenames) && (loaded_commsymname != NULL))
      if (strrchr (loaded_commsymname, '/') != NULL)
	loaded_commsymname = strrchr (loaded_commsymname, '/') + 1;
      
    if ((loaded_name != NULL) && (name != NULL)) {
      int namelen = strlen (loaded_name) + 1;
      *name = (char *) xmalloc (namelen);
      memcpy (*name, loaded_name, namelen);
    }
      
    if ((loaded_objname != NULL) && (objname != NULL)) {
      int namelen = strlen (loaded_objname) + 1;
      *objname = (char *) xmalloc (namelen);
      memcpy (*objname, loaded_objname, namelen);
    }

    if ((loaded_symname != NULL) && (symname != NULL)) {
      int namelen = strlen (loaded_symname) + 1;
      *symname = (char *) xmalloc (namelen);
      memcpy (*symname, loaded_symname, namelen);
    }

    if ((loaded_commobjname != NULL) && (commobjname != NULL)) {
      int namelen = strlen (loaded_commobjname) + 1;
      *commobjname = (char *) xmalloc (namelen);
      memcpy (*commobjname, loaded_commobjname, namelen);
    }

    if ((loaded_commsymname != NULL) && (commsymname != NULL)) {
      int namelen = strlen (loaded_commsymname) + 1;
      *commsymname = (char *) xmalloc (namelen);
      memcpy (*commsymname, loaded_commsymname, namelen);
    }

    if (e->loaded_addrisoffset) {
      if (e->image_addr_valid) {
	if (slide != NULL)
	  *slide = dyld_offset_string ((unsigned long) e->loaded_offset);
	if (addr != NULL)
	  xasprintf (addr, "0x%lx", (unsigned long) e->image_addr);
      } else {
	if (slide != NULL)
	  *slide = dyld_offset_string ((unsigned long) e->loaded_offset);
      }
    } else {
      if (e->dyld_valid) {
	if (slide != NULL)
	  *slide = dyld_offset_string ((unsigned long) e->dyld_slide);
	if (addr != NULL)
	  xasprintf (addr, "0x%lx", (unsigned long) e->loaded_addr);
      } else {
	if (e->image_addr_valid) {
	  if (slide != NULL)
	    *slide = dyld_offset_string ((unsigned long) (e->loaded_addr - e->image_addr));
	  if (addr != NULL)
	    xasprintf (addr, "0x%lx", (unsigned long) e->loaded_addr);
	} else {
	  if (addr != NULL)
	    xasprintf (addr, "0x%lx", (unsigned long) e->loaded_addr);
	}	      
      }
    }	  

  } else {

    const char *s; 
    const char *tmp;
    int namelen;
    s = dyld_entry_filename (e, NULL, 0);
    if (s == NULL) {
      s = "[UNKNOWN]";
    }
    if (! print_basenames) {
      tmp = strrchr (s, '/');
      if (tmp == NULL) {
	tmp = s;
      } else {
	tmp++;
      }
    } else {
      tmp = s;
    }
    
    if ((tmp != NULL) && (name != NULL)) {
      namelen = strlen (tmp) + 1;
      *name = xmalloc (namelen);
      memcpy (*name, tmp, namelen);
    }
  }

  if ((e->prefix != NULL) && (e->prefix[0] != '\0') && (prefix != NULL)) {
    int prefixlen = strlen (e->prefix) + 1;
    *prefix = xmalloc (prefixlen);
    memcpy (*prefix, e->prefix, prefixlen);
  }
  
}

int dyld_resolve_shlib_num
(struct dyld_objfile_info *s, unsigned int num, struct dyld_objfile_entry **eptr, struct objfile **optr)
{
  struct objfile *objfile;
  struct objfile *temp;
  unsigned int i;

  CHECK_FATAL (eptr != NULL);
  CHECK_FATAL (optr != NULL);

  *eptr = NULL;
  *optr = NULL;

  for (i = 0; i < s->nents; i++) {

    struct dyld_objfile_entry *j = &s->entries[i];

    if (! j->allocated) { 
      continue;
    }

    num--;

    if (num == 0) {
      *eptr = j;
      *optr = j->objfile;
      return 0;
    }
  }

  ALL_OBJFILES_SAFE (objfile, temp) {

    int found = 0;
    
    for (i = 0; i < s->nents; i++) {
      struct dyld_objfile_entry *j = &s->entries[i];
      if (! j->allocated) {
	continue; 
      }
      if ((j->objfile == objfile) || (j->commpage_objfile == objfile)) {
	found = 1;
      }
    }

    if (! found) {
      num--;
    }
    
    if (num == 0) {
      *eptr = NULL;
      *optr = objfile;
      return 0;
    }
  }	  

  return -1;
}

/* Returns the shared library number of the entry at 'eptr' in
   'numptr'.  Returns 0 on success, anything else on failure. */

int dyld_entry_shlib_num
(struct dyld_objfile_info *s, struct dyld_objfile_entry *eptr, unsigned int *numptr)
{
  unsigned int num = 0;
  unsigned int i;

  CHECK_FATAL (numptr != NULL);

  *numptr = num;

  for (i = 0; i < s->nents; i++) {

    struct dyld_objfile_entry *j = &s->entries[i];

    if (! j->allocated) { 
      continue;
    }

    if (j == eptr) {
      *numptr = num;
      return 0;
    }

    num++;
  }

  *numptr = 0;
  return -1;
}

/* Returns the length of the longest field that would be printed when
   displaying 's' according to 'reason_mask'. */

unsigned int
dyld_shlib_info_basename_length
(struct dyld_objfile_info *s, unsigned int reason_mask)
{
  unsigned int i;
  unsigned int baselen = 0;
  struct objfile *objfile, *temp;

  for (i = 0; i < s->nents; i++) {

    const char *name = NULL;
    const char *tfname = NULL;
    unsigned int tfnamelen = 0;

    struct dyld_objfile_entry *j = &s->entries[i];

    if (! j->allocated) { 
      continue;
    }

    if (! (j->reason & reason_mask)) {
      continue;
    }

    name = dyld_entry_filename (j, NULL, 0);
    if (name == NULL) {
      if (baselen < 1) {
	baselen = 1;
      }
    } else {
      dyld_library_basename (name, &tfname, &tfnamelen, NULL, NULL);
      if (baselen < tfnamelen) {
	baselen = tfnamelen;
      }
    }
  }

  if (! (reason_mask & dyld_reason_user)) {
    return baselen;
  }

  ALL_OBJFILES_SAFE (objfile, temp) {

    const char *name = NULL;
    const char *tfname = NULL;
    unsigned int tfnamelen = 0;

    int found = 0;
    
    for (i = 0; i < s->nents; i++) {
      struct dyld_objfile_entry *j = &s->entries[i];
      if (! j->allocated) {
	continue; 
      }
      if ((j->objfile == objfile) || (j->commpage_objfile == objfile)) {
	found = 1;
      }
    }
    
    if (! found) {
      
      struct dyld_objfile_entry tentry;
      dyld_convert_entry (objfile, &tentry);

      name = dyld_entry_filename (&tentry, NULL, 0);
      if (name == NULL) {
	if (baselen < 1) {
	  baselen = 1;
	}
      } else {
	dyld_library_basename (name, &tfname, &tfnamelen, NULL, NULL);
	if (baselen < tfnamelen) {
	  baselen = tfnamelen;
	}
      }
    }
  }

  return baselen;
}

/* Return true if and only if the shared library specifier 'shlibnum'
   matches the string in 'args'.  Returns 'false' on error; outputs
   warning messages only if 'verbose' is passed as true. */

int dyld_entry_shlib_num_matches (int shlibnum, const char *args, int verbose)
{
  const char *p, *p1;
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

void dyld_print_entry_info (struct dyld_objfile_entry *j, unsigned int shlibnum,
			    unsigned int baselen)
{
  const char *name = NULL;
  const char *tfname = NULL;
  unsigned int tfnamelen = 0;
  int is_framework, is_bundle;
  char *fname = NULL;
  char addrbuf[24];
  const char *ptr;

  name = dyld_entry_filename (j, NULL, 0);
  if (name == NULL) {
    fname = savestring ("-", strlen ("-"));
    is_framework = 0;
    is_bundle = 0;
  } else {
    dyld_library_basename (name, &tfname, &tfnamelen, &is_framework, &is_bundle);
    fname = savestring (tfname, tfnamelen);
  }

  if (j->dyld_valid) {
    snprintf (addrbuf, 24, "0x%lx", (unsigned long) j->dyld_addr);
  } else {
    strcpy (addrbuf, "-");
  }

  if (baselen < strlen (fname)) {
    baselen = strlen (fname);
  }

  ui_out_list_begin (uiout, "shlib-info");
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
  ui_out_spaces (uiout, 10 - strlen (addrbuf));
  ui_out_spaces (uiout, 1);

  ptr = dyld_reason_string (j->reason);
  ui_out_spaces (uiout, 11 - strlen (ptr));
  ui_out_field_string (uiout, "reason", ptr);
  ui_out_spaces (uiout, 1);
    
  if (j->load_flag == OBJF_SYM_ALL) {
    ptr = "Y";
  } else if (j->load_flag == OBJF_SYM_NONE) {
    ptr = "N";
  } else if (j->load_flag == OBJF_SYM_EXTERN) {
    ptr = "E";
  } else {
    ptr = "?";
  }

  ui_out_field_string (uiout, "requested-state", ptr);
  ui_out_spaces (uiout, 1);
    
  if (j->loaded_error) {
    ptr = "!";
  } else {
    if (j->objfile != NULL) {
      if (! dyld_objfile_allocated (j->objfile)) {
	ptr = "!";
      } else if (j->objfile->symflags == OBJF_SYM_ALL) {
	ptr = "Y";
      } else if (j->objfile->symflags == OBJF_SYM_NONE) {
	ptr = "N";
      } else if (j->objfile->symflags == OBJF_SYM_EXTERN) {
	ptr = "E";
      } else {
	ptr = "?";
      }
    } else if (j->abfd != NULL) {
      ptr = "B";
    } else {
      ptr = "N";
    }
  }

  ui_out_field_string (uiout, "state", ptr);
  ui_out_spaces (uiout, 1);

  dyld_entry_out (uiout, j, 1);

  { 
    char *name;
    char *objname;
    char *symname;
    char *commobjname;
    char *commsymname;
    char *addr;
    char *slide;
    char *prefix;
    
    dyld_entry_info (j, 1, &name, &objname, &symname, &commobjname, &commsymname, &addr, &slide, &prefix);
      
    if (objname != NULL)
      if ((name == NULL) || (strcmp (name, objname) != 0))
	{
	  ui_out_text (uiout, "\n");
	  ui_out_spaces (uiout, baselen + 34);
	  ui_out_field_string (uiout, "objpath", objname);
	}

    if (symname != NULL)
      if ((name == NULL) || (strcmp (name, symname) != 0))
	{
	  ui_out_text (uiout, "\n");
	  ui_out_spaces (uiout, baselen + 34);
	  ui_out_field_string (uiout, "sympath", symname);
	}
    
    if (commobjname != NULL)
      {
	ui_out_text (uiout, "\n");
	ui_out_spaces (uiout, baselen + 34);
	ui_out_field_string (uiout, "objpath", commobjname);
      }

    if (commsymname != NULL)
      {
	ui_out_text (uiout, "\n");
	ui_out_spaces (uiout, baselen + 34);
	ui_out_field_string (uiout, "sympath", commsymname);
      }
    
    xfree (name);
    xfree (objname);
    xfree (symname);
    xfree (commobjname);
    xfree (commsymname);
    xfree (addr);
    xfree (slide);
    xfree (prefix);
  }
  
  ui_out_list_end (uiout);

  ui_out_text (uiout, "\n");
}

void dyld_convert_entry (struct objfile *o, struct dyld_objfile_entry *e)
{
  dyld_objfile_entry_clear (e);

  e->allocated = 1;
  e->reason = dyld_reason_user;

  e->objfile = o;
  e->abfd = o->obfd;

  if (e->abfd != NULL)
    e->loaded_name = bfd_get_filename (e->abfd);
}

void dyld_print_shlib_info (struct dyld_objfile_info *s, unsigned int reason_mask, int header, const char *args) 
{
  unsigned int baselen = 0;
  char *basepad = NULL;
  unsigned int shlibnum = 0; 
  struct objfile *objfile, *temp;
  unsigned int i;

  baselen = dyld_shlib_info_basename_length (s, reason_mask);
  if (baselen < 12) {
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

  for (i = 0; i < s->nents; i++) {

    struct dyld_objfile_entry *j = &s->entries[i];

    if (! j->allocated) { 
      if (reason_mask & dyld_reason_deallocated)
	printf_filtered ("[DEALLOCATED]\n");
      continue;
    }

    shlibnum++;

    if (! (j->reason & reason_mask)) {
      continue;
    }

    if ((args == NULL) || dyld_entry_shlib_num_matches (shlibnum, args, 0)) {
      dyld_print_entry_info (j, shlibnum, baselen);
    }
  }

  ALL_OBJFILES_SAFE (objfile, temp) {

    int found = 0;
    
    for (i = 0; i < s->nents; i++) {
      struct dyld_objfile_entry *j = &s->entries[i];
      if (! j->allocated) {
	continue; 
      }
      if ((j->objfile == objfile) || (j->commpage_objfile == objfile)) {
	found = 1;
      }
    }

    if (! found) {

      struct dyld_objfile_entry tentry;
      shlibnum++;

      if (! (reason_mask & dyld_reason_user)) {
	continue;
      }

      if ((args == NULL) || dyld_entry_shlib_num_matches (shlibnum, args, 0)) {
	dyld_convert_entry (objfile, &tentry);
	dyld_print_entry_info (&tentry, shlibnum, baselen);
      }
    }
  }
}

unsigned int dyld_next_allocated_shlib (struct dyld_objfile_info *info, unsigned int n)
{
  struct dyld_objfile_entry *o;
  for (;;) {
    if (n >= info->nents)
      return n;
    o = &info->entries[n];
    if (o->allocated)
      return n;
    n++;    
  }
}
