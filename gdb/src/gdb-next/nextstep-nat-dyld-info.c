#include "nextstep-nat-dyld-info.h"
#include "nextstep-nat-dyld-path.h"

#include <string.h>

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"

void 
dyld_entry_info (struct dyld_objfile_entry *e, int print_basenames, 
		 char **in_name, char **addr, char **slide, char **prefix);

const char *dyld_reason_string (dyld_objfile_reason r)
{
  switch (r) {
  case dyld_reason_init: return "init";
  case dyld_reason_cached: return "cached";
  case dyld_reason_dyld: return "dyld";
  case dyld_reason_cfm: return "cfm";
  case dyld_reason_executable: return "exec";
  default: return "???";
  }
}    

void dyld_check_entry (struct dyld_objfile_entry *e)
{
}

void dyld_objfile_entry_clear (struct dyld_objfile_entry *e)
{
  e->prefix = "";

  e->dyld_name = NULL;
  e->dyld_name_valid = 0;

  e->dyld_addr = 0;
  e->dyld_slide = 0;
  e->dyld_index = 0;
  e->dyld_valid = 0;

  e->cfm_connection = 0;

  e->user_name = NULL;

  e->image_name = NULL;
  e->image_name_valid = 0;

  e->image_addr = 0;
  e->image_addr_valid = 0;
  
  e->text_name = NULL;
  e->text_name_valid = 0;

  e->abfd = NULL;
  e->sym_bfd = NULL;
  e->sym_objfile = NULL;
  e->objfile = NULL;

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
    free (i->entries);
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

  COMPARE_SCALAR(cfm_connection);

  COMPARE_STRING(user_name);

  COMPARE_STRING(image_name);

  COMPARE_SCALAR(image_name_valid);

  COMPARE_SCALAR(image_addr);
  COMPARE_SCALAR(image_addr_valid);

  COMPARE_STRING(text_name);
  COMPARE_SCALAR(text_name_valid);

  COMPARE_SCALAR(abfd);
  COMPARE_SCALAR(sym_bfd);
  COMPARE_SCALAR(objfile);
  COMPARE_SCALAR(sym_objfile);

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

const int dyld_entry_source_filename_is_absolute
(struct dyld_objfile_entry *e)
{
  CHECK_FATAL (e != NULL);
  CHECK_FATAL (e->allocated);
  if (e->loaded_name != NULL) { 
    return 1;
  } else if (e->user_name != NULL) {
    return 1;
  } else if (e->dyld_name != NULL) {
    return 1;
  } else if (e->image_name != NULL) {
    return 0;
  } else if (e->text_name != NULL) {
    return 0;
  } else {
    return 0;
  }
}

const char *dyld_entry_source_filename 
(struct dyld_objfile_entry *e)
{
  CHECK_FATAL (e != NULL);
  CHECK_FATAL (e->allocated);
  if (e->loaded_name != NULL) { 
    return e->loaded_name;
  } else if (e->user_name != NULL) {
    return e->user_name;
  } else if (e->dyld_name != NULL) {
    return e->dyld_name;
  } else if (e->image_name != NULL) {
    return e->image_name;
  } else if (e->text_name != NULL) {
    return e->text_name;
  } else {
    return NULL;
  }
}

char *dyld_offset_string (unsigned long offset)
{
  char *ret = NULL;
  if (offset > LONG_MAX) {
    asprintf (&ret, "-0x%lx", ((ULONG_MAX - offset) + 1));
  } else {
    asprintf (&ret, "0x%lx", offset);
  }
  return ret;
}

char *dyld_entry_string (struct dyld_objfile_entry *e, int print_basenames)
{
  char *name;
  char *addr;
  char *slide;
  char *prefix;

  char *ret;
  char *ret2;

  dyld_entry_info (e, print_basenames, &name, &addr, &slide, &prefix);

  if (name == NULL)
    {
      if (addr != NULL)
	{
	  if (slide != NULL)
	    asprintf (&ret, "[memory at %s] (offset %s)", addr, slide);
	  else
	    asprintf (&ret, "[memory at %s]", addr);
	}
      else
	{
	  if (slide == NULL)
	    ret = NULL;
	  else
	    asprintf (&ret, "(offset %s)", slide);
	}     
    }
  else
    {
      if (slide == NULL)
	{
	  if (addr == NULL)
	    asprintf (&ret, "\"%s\"", name);
	  else
	    asprintf (&ret, "\"%s\" at %s", name, addr);
	}
      else
	{
	  if (addr == NULL)
	    asprintf (&ret, "\"%s\" (offset %s)", name, slide);
	  else
	    asprintf (&ret, "\"%s\" at %s (offset %s)", name, addr, slide);
	}
      if (prefix != NULL)
	{
	  asprintf (&ret2, "%s with prefix \"%s\"", ret, prefix);
	  free (ret);
	  ret = ret2;
	}
    }

  xfree (name);
  xfree (addr);
  xfree (slide);
  xfree (prefix);

  return ret;

}

char *dyld_entry_out (struct ui_out *uiout, struct dyld_objfile_entry *e, int print_basenames)
{
  char *name;
  char *addr;
  char *slide;
  char *prefix;

  char *ret;

  dyld_entry_info (e, print_basenames, &name, &addr, &slide, &prefix);

  if (name == NULL)
    {
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
      ui_out_text (uiout, "\"");
      ui_out_field_string (uiout, "path", name);
      ui_out_text (uiout, "\"");

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
  xfree (addr);
  xfree (slide);
  xfree (prefix);

  return ret;

}

void 
dyld_entry_info (struct dyld_objfile_entry *e, int print_basenames, 
		 char **name, char **addr, char **slide, char **prefix)
{
  CHECK_FATAL (e != NULL);

  *name = NULL;
  *addr = NULL;
  *slide = NULL;
  *prefix = NULL;

  if (e->objfile) {

    if (e->loaded_from_memory) {
      CHECK_FATAL (! e->loaded_addrisoffset);
      CHECK_FATAL (e->loaded_addr == e->loaded_memaddr);
      if (e->image_addr_valid) {
	*slide = dyld_offset_string ((unsigned long) (e->loaded_memaddr - e->image_addr));
	asprintf (addr, "0x%lx", (unsigned long) e->loaded_memaddr);
      } else {
	asprintf (addr, "0x%lx", (unsigned long) e->loaded_memaddr);
      }	      
    } else {

      const char *loaded_name;
      if (! print_basenames) {
        if (e->loaded_name != NULL) {
	  loaded_name = strrchr (e->loaded_name, '/');
	  if (loaded_name == NULL) {
	    loaded_name = e->loaded_name;
	  } else {
	    loaded_name++;
	  }
	} else {
	  loaded_name = NULL;
	}
      } else {
	loaded_name = e->loaded_name;
      }
      
      if (loaded_name != NULL) {
	  int namelen = strlen (loaded_name) + 1;
	  *name = (char *) xmalloc (namelen);
	  memcpy (*name, loaded_name, namelen);
      }

      if (e->loaded_addrisoffset) {
	if (e->image_addr_valid) {
	  *slide = dyld_offset_string ((unsigned long) e->loaded_offset);
	  asprintf (addr, "0x%lx", (unsigned long) e->image_addr);
	} else {
	  *slide = dyld_offset_string ((unsigned long) e->loaded_offset);
	}
      } else {
	if (e->dyld_valid) {
	  *slide = dyld_offset_string ((unsigned long) e->dyld_slide);
	  asprintf (addr, "0x%lx", (unsigned long) e->loaded_addr);
	} else {
	  if (e->image_addr_valid) {
	    *slide = dyld_offset_string ((unsigned long) (e->loaded_addr - e->image_addr));
	    asprintf (addr, "0x%lx", (unsigned long) e->loaded_addr);
	  } else {
	    asprintf (addr, "0x%lx", (unsigned long) e->loaded_addr);
	  }	      
	}
      }	  
    }
  } else {
    const char *s; 
    const char *tmp;
    int namelen;
    s = dyld_entry_source_filename (e);
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
    
    if (tmp != NULL) {
      namelen = strlen (tmp) + 1;
      *name = xmalloc (namelen);
      memcpy (*name, tmp, namelen);
    }
  }

  if ((e->prefix != NULL) && (e->prefix[0] != '\0')) {
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

  ALL_OBJFILES_SAFE (objfile, temp) {

    int found = 0;
    
    for (i = 0; i < s->nents; i++) {
      struct dyld_objfile_entry *j = &s->entries[i];
      if (! j->allocated) {
	continue; 
      }
      if (j->objfile == objfile) {
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

  return -1;
}

void dyld_print_shlib_info (struct dyld_objfile_info *s, unsigned int reason_mask) 
{
  unsigned int i;
  unsigned int baselen = 0;
  unsigned int bpnum = 0; 
  char *basepad;
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

    name = dyld_entry_source_filename (j);
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

  if (baselen < 8) {
    baselen = 8;
  }

  basepad = xmalloc (baselen + 1);
  memset (basepad, ' ', baselen);
  basepad[baselen] = '\0';

  ui_out_text_fmt (uiout, 
		   "%s     Framework?            Loaded? Error?                \n"
		   "Num Basename%s | Address  Reason   | | Source              \n"
		   "  | |%s        | |          |      | | |                   \n",
		   basepad + 8, basepad + 8, basepad + 8);
  
  ALL_OBJFILES_SAFE (objfile, temp) {

    int found = 0;
    
    for (i = 0; i < s->nents; i++) {
      struct dyld_objfile_entry *j = &s->entries[i];
      if (! j->allocated) {
	continue; 
      }
      if (j->objfile == objfile) {
	found = 1;
      }
    }

    if (! found) {

      bpnum++;

      if (reason_mask & dyld_reason_user) {
	char *ptr;

	ui_out_list_begin (uiout, "shlib-info");
	if (bpnum < 10)
	  ui_out_spaces (uiout, 2);
	else if (bpnum < 100)
	  ui_out_spaces (uiout, 1);
	
	ui_out_field_int (uiout, "num", bpnum);
	ui_out_spaces (uiout, 1);
	
	ui_out_field_string (uiout, "name", "");
	ui_out_spaces (uiout, baselen + 1);
	
	ui_out_field_string (uiout, "kind", "");
	ui_out_spaces (uiout, 2);
	
	ui_out_spaces (uiout, 1);
	
	ui_out_spaces (uiout, 9);
	ui_out_field_string (uiout, "dyld-addr", "");
	ui_out_spaces (uiout, 1);
	
	ui_out_spaces (uiout, 6 - strlen ("user"));
	ui_out_field_string (uiout, "reason", "user");
	ui_out_spaces (uiout, 1);
	
	ptr = "-";
	ui_out_field_string (uiout, "requested-state", ptr);
	ui_out_spaces (uiout, 1);
	
	if (objfile->symflags == OBJF_SYM_ALL) {
	  ptr = "Y";
	} else if (objfile->symflags == OBJF_SYM_NONE) {
	  ptr = "N";
	} else if (objfile->symflags == OBJF_SYM_EXTERN) {
	  ptr = "E";
	} else if (objfile->symflags == OBJF_SYM_CONTAINER) {
	  ptr = "C";
	} else {
	  ptr = "?";
	}

	ui_out_field_string (uiout, "state", ptr);
	ui_out_spaces (uiout, 1);
	
	ptr = objfile->name ? objfile->name : "[UNKNOWN]";
	ui_out_text (uiout, "\"");
	ui_out_field_string (uiout, "path", ptr);
	ui_out_text (uiout, "\"");
	ui_out_field_skip (uiout, "slide");
	ui_out_field_skip (uiout, "loaded-addr");
	if ((objfile->prefix != NULL) && (objfile->prefix[0] != '\0'))
	  {
	    ui_out_text (uiout, " with prefix \"");
	    ui_out_field_string (uiout, "prefix", objfile->prefix);
	    ui_out_text (uiout, "\"");
	  }
	else
	  {
	    ui_out_field_skip (uiout, "prefix");
	  }

	ui_out_list_end (uiout);
	ui_out_text (uiout, "\n");
      }
    }
  }

  for (i = 0; i < s->nents; i++) {

    const char *name = NULL;
    const char *tfname = NULL;
    unsigned int tfnamelen = 0;
    int is_framework, is_bundle;
    char *fname = NULL;
    char addrbuf[24];
    const char *ptr;

    struct dyld_objfile_entry *j = &s->entries[i];

    if (! j->allocated) { 
      /* printf_filtered ("[DEALLOCATED]\n"); */
      continue;
    }

    bpnum++;

    if (! (j->reason & reason_mask)) {
      continue;
    }

    name = dyld_entry_source_filename (j);
    if (name == NULL) {
      fname = strsave ("-");
    } else {
      dyld_library_basename (name, &tfname, &tfnamelen, &is_framework, &is_bundle);
      fname = savestring (tfname, tfnamelen);
    }

    if (j->dyld_valid) {
      snprintf (addrbuf, 24, "0x%lx", (unsigned long) j->dyld_addr);
    } else {
      strcpy (addrbuf, "-");
    }

    ui_out_list_begin(uiout, "shlib-info");
    if (bpnum < 10)
      ui_out_text (uiout, "  ");
    else if (bpnum < 100)
      ui_out_text (uiout, " ");

    ui_out_field_int (uiout, "num", bpnum);
    ui_out_spaces (uiout, 1);

    ui_out_field_string (uiout, "name", fname);
    ui_out_spaces (uiout, baselen - strlen (fname) + 1);

    ptr = is_framework ? "F" : (is_bundle ? "B" : "");
    ui_out_field_string (uiout, "kind", ptr);
    if (strlen (ptr) == 0)
	ui_out_spaces (uiout, 1);

    ui_out_spaces (uiout, 1);
    
    ui_out_field_string (uiout, "dyld-addr", addrbuf);
    ui_out_spaces (uiout, 10 - strlen (addrbuf));
    ui_out_spaces (uiout, 1);

    ptr = dyld_reason_string (j->reason);
    ui_out_spaces (uiout, 6 - strlen(ptr));
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
	if (j->objfile->symflags == OBJF_SYM_ALL) {
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

    ui_out_list_end (uiout);

    ui_out_text (uiout, "\n");
  }
}
