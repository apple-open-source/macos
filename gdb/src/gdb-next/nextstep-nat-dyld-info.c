#include "nextstep-nat-dyld-info.h"
#include "nextstep-nat-dyld-path.h"

#include <string.h>

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"

const char *dyld_reason_string (dyld_objfile_reason r)
{
  switch (r) {
  case dyld_reason_init: return "init";
  case dyld_reason_cached: return "cached";
  case dyld_reason_dyld: return "dyld";
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

  e->user_name = NULL;

  e->image_name = NULL;
  e->image_name_valid = 0;

  e->image_addr = 0;
  e->image_addr_valid = 0;
  
  e->text_name = NULL;
  e->text_name_valid = 0;

  e->abfd = NULL;
  e->objfile = NULL;

  e->loaded_name = NULL;
  e->loaded_memaddr = 0;
  e->loaded_addr = 0;
  e->loaded_offset = 0;
  e->loaded_addrisoffset = -1;
  e->loaded_from_memory = -1;

  e->loaded_error = 0;
  e->loaded_flag = 0;

  e->load_flag = 0;

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
  char *ret = NULL;
  char *ret2 = NULL;

  CHECK_FATAL (e != NULL);

  if (e->loaded_flag) {
    if (e->loaded_from_memory) {
      CHECK_FATAL (! e->loaded_addrisoffset);
      CHECK_FATAL (e->loaded_addr == e->loaded_memaddr);
      if (e->image_addr_valid) {
	char *offstr = dyld_offset_string ((unsigned long) (e->loaded_memaddr - e->image_addr));
	asprintf (&ret, "[memory at 0x%lx] (offset %s)", 
		  (unsigned long) e->loaded_memaddr, offstr);
	free (offstr);
      } else {
	asprintf (&ret, "[memory at 0x%lx]", 
		  (unsigned long) e->loaded_memaddr);
      }	      
    } else {

      const char *loaded_name;
      if (! print_basenames) {
	loaded_name = strrchr (e->loaded_name, '/');
	if (loaded_name == NULL) {
	  loaded_name = e->loaded_name;
	} else {
	  loaded_name++;
	}
      } else {
	loaded_name = e->loaded_name;
      }
      
      if (e->loaded_addrisoffset) {
	if (e->image_addr_valid) {
	  char *offstr = dyld_offset_string ((unsigned long) e->loaded_offset);
	  asprintf (&ret, "\"%s\" at 0x%lx (offset %s)", loaded_name,
		    (unsigned long) e->image_addr, offstr);
	  free (offstr);
	} else {
	  char *offstr = dyld_offset_string ((unsigned long) e->loaded_offset);
	  asprintf (&ret, "\"%s\" (offset %s)", loaded_name, offstr);
	  free (offstr);
	}
      } else {
	if (e->dyld_valid) {
	  char *offstr = dyld_offset_string ((unsigned long) e->dyld_slide);
	  asprintf (&ret, "\"%s\" at 0x%lx (offset %s)", loaded_name,
		    (unsigned long) e->loaded_addr, offstr);
	  free (offstr);
	} else {
	  if (e->image_addr_valid) {
	    char *offstr = dyld_offset_string ((unsigned long) (e->loaded_addr - e->image_addr));
	    asprintf (&ret, "\"%s\" at 0x%lx (offset %s)", 
		      loaded_name, (unsigned long) e->loaded_addr, offstr);
	    free (offstr);
	  } else {
	    asprintf (&ret, "\"%s\" at 0x%lx", loaded_name,
		      (unsigned long) e->loaded_addr);
	  }	      
	}
      }	  
    }
  } else {
    const char *name, *s;
    s = dyld_entry_source_filename (e);
    if (s == NULL) {
      s = "[UNKNOWN]";
    }
    if (! print_basenames) {
      name = strrchr (s, '/');
      if (name == NULL) {
	name = s;
      } else {
	name++;
      }
    } else {
      name = s;
    }
    asprintf (&ret, "\"%s\"", name);
  }

  if ((e->prefix != NULL) && (e->prefix[0] != '\0')) {
    asprintf (&ret2, "%s with prefix \"%s\"", ret, e->prefix);
    free (ret);
    return ret2;
  }
  
  return ret;
}

void dyld_print_shlib_info (struct dyld_objfile_info *s) 
{
  unsigned int i;
  unsigned int baselen = 0;
  char *basepad;
  char *estr;
  struct objfile *objfile, *temp;

  for (i = 0; i < s->nents; i++) {

    const char *name = NULL;
    const char *tfname = NULL;
    unsigned int tfnamelen = 0;

    struct dyld_objfile_entry *j = &s->entries[i];

    if (! j->allocated) { 
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

  printf_filtered 
    ("%s     Framework?        Loaded? Error?                \n"
     "Basename%s | Address  Reason   | | Source              \n"
     "|%s        | |          |      | | |                   \n",
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
      printf_filtered ("%s%s %-1s %-10s %-6s %-1s %-1s ",
		       "", basepad + strlen (""),
		       "",
		       "",
		       "user",
		       "Y",
		       "N");
      printf_filtered ("\"%s\"", objfile->name ? objfile->name : "[UNKNOWN]");
      if ((objfile->prefix != NULL) && (objfile->prefix[0] != '\0')) {
	printf_filtered (" with prefix \"%s\"", objfile->prefix);
      }
      printf_filtered ("\n");
    }
  }

  for (i = 0; i < s->nents; i++) {

    const char *name = NULL;
    const char *tfname = NULL;
    unsigned int tfnamelen = 0;
    int is_framework, is_bundle;
    char *fname = NULL;
    char addrbuf[24];

    struct dyld_objfile_entry *j = &s->entries[i];

    if (! j->allocated) { 
      printf_filtered ("[DEALLOCATED]\n");
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

    estr = dyld_entry_string (j, 1);
    printf_filtered ("%s%s %-1s %-10s %-6s %-1s %-1s %s\n",
		     fname, basepad + strlen (fname),
		     is_framework ? "F" : (is_bundle ? "B" : ""),
		     addrbuf,
		     dyld_reason_string (j->reason),
		     j->loaded_flag ? "Y" : "N",
		     j->loaded_error ? "Y" : "N",
		     estr);
    free (estr);
  }
}
