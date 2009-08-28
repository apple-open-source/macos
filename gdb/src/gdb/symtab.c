/* Symbol table lookup for the GNU debugger, GDB.

   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994,
   1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

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
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcore.h"
#include "frame.h"
#include "target.h"
#include "value.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "call-cmds.h"
#include "gdb_regex.h"
#include "expression.h"
#include "language.h"
#include "demangle.h"
#include "inferior.h"
#include "linespec.h"
#include "source.h"
#include "filenames.h"		/* for FILENAME_CMP */
#include "objc-lang.h"
#include "ada-lang.h"

#include "hashtab.h"

#include "gdb_obstack.h"
/* APPLE LOCAL - subroutine inlining  */
#include "gdb_assert.h"
#include "block.h"
#include "dictionary.h"

#include <sys/types.h>
#include <fcntl.h>
#include "gdb_string.h"
#include "gdb_stat.h"
#include <ctype.h>
#include "cp-abi.h"
#include "observer.h"

/* APPLE LOCAL: So we can complain.  */
#include "complaints.h"

/* APPLE LOCAL begin cache lookup values for improved performance  */

asection * cached_mapped_section = NULL;
asection * cached_overlay_section = NULL;
struct obj_section * cached_sect_section = NULL;
struct symtab * cached_symtab = NULL;
struct partial_symtab * cached_psymtab = NULL;
struct symtab_and_line * cached_pc_line = NULL;
struct symbol * cached_pc_function = NULL;
struct blockvector * cached_blockvector = NULL;
int cached_blockvector_index = -1;
struct block * cached_block = NULL;

CORE_ADDR last_block_lookup_pc = INVALID_ADDRESS;
CORE_ADDR last_blockvector_lookup_pc = INVALID_ADDRESS;
CORE_ADDR last_function_lookup_pc = INVALID_ADDRESS;
CORE_ADDR last_pc_line_lookup_pc = INVALID_ADDRESS;
CORE_ADDR last_psymtab_lookup_pc = INVALID_ADDRESS;
CORE_ADDR last_symtab_lookup_pc = INVALID_ADDRESS;
CORE_ADDR last_sect_section_lookup_pc = INVALID_ADDRESS;
CORE_ADDR last_mapped_section_lookup_pc = INVALID_ADDRESS;
CORE_ADDR last_overlay_section_lookup_pc = INVALID_ADDRESS;

/* APPLE LOCAL end cache lookup values for improved performance  */

/* Prototypes for local functions */

static void completion_list_add_name (char *, char *, int, char *, char *);

static void rbreak_command (char *, int);

static void types_info (char *, int);

static void functions_info (char *, int);

static void variables_info (char *, int);

static void sources_info (char *, int);

static void output_source_filename (const char *, int *);

static int find_line_common (struct linetable *, int, int *);

/* This one is used by linespec.c */

char *operator_chars (char *p, char **end);

static struct symbol *lookup_symbol_aux (const char *name,
					 const char *linkage_name,
					 const struct block *block,
					 const domain_enum domain,
					 int *is_a_field_of_this,
					 struct symtab **symtab);

static
struct symbol *lookup_symbol_aux_local (const char *name,
					const char *linkage_name,
					const struct block *block,
					const domain_enum domain,
					struct symtab **symtab);

/* APPLE LOCAL begin return multiple symbols  */
static
struct symbol *lookup_symbol_aux_symtabs (int block_index,
					  const char *name,
					  const char *linkage_name,
					  const domain_enum domain,
					  struct symtab **symtab,
					  struct symbol_search **sym_list,
					  int find_all_occurrences);

static
struct symbol *lookup_symbol_aux_psymtabs (int block_index,
					   const char *name,
					   const char *linkage_name,
					   const domain_enum domain,
					   struct symtab **symtab,
					   struct symbol_search **sym_list,
					   int find_all_occurrences);
/* APPLE LOCAL end return multiple symbols  */

#if 0
static
struct symbol *lookup_symbol_aux_minsyms (const char *name,
					  const char *linkage_name,
					  const domain_enum domain,
					  int *is_a_field_of_this,
					  struct symtab **symtab);
#endif

/* This flag is used in hppa-tdep.c, and set in hp-symtab-read.c.
   Signals the presence of objects compiled by HP compilers.  */
int deprecated_hp_som_som_object_present = 0;

static void fixup_section (struct general_symbol_info *, struct objfile *);

static int file_matches (char *, char **, int);

static void print_symbol_info (domain_enum,
			       struct symtab *, struct symbol *, int, char *);

static void print_msymbol_info (struct minimal_symbol *);

static void symtab_symbol_info (char *, domain_enum, int);

void _initialize_symtab (void);

/* APPLE LOCAL begin */
/* skip_non_matching_bfd returns 1 if the bfd in SECTION doesn't
   match either the bfd in OBJFILE, or the backlink_objfile for
   OBJFILE, if that exists.  Otherwise it returns 0.  */

static int 
skip_non_matching_bfd (asection *section, struct objfile *objfile)
{

  /* No section info, don't skip.  */
  if (section == NULL)
    return 0;
  
  /* If the objfile & section directly match, don't skip.  */
  if (section->owner == objfile->obfd)
    return 0;
  
  /* If this is not a debug objfile, skip it, since it failed
     the direct test.  */
  if (objfile->separate_debug_objfile_backlink == NULL)
    return 1;
  
  /* If the objfile this is the debug objfile of matches, don't
     skip.  */
  if (section->owner == objfile->separate_debug_objfile_backlink->obfd)
    return 0;
  
  /* Otherwise skip.  */
  return 1;
}
/* APPLE LOCAL end */
/* */

/* The single non-language-specific builtin type */
struct type *builtin_type_error;

/* Block in which the most recently searched-for symbol was found.
   Might be better to make this a parameter to lookup_symbol and 
   value_of_this. */

const struct block *block_found;

/* Check for a symtab of a specific name; first in symtabs, then in
   psymtabs.  *If* there is no '/' in the name, a match after a '/'
   in the symtab filename will also work.  */

struct symtab *
lookup_symtab (const char *name)
{
  struct symtab *s;
  struct partial_symtab *ps;
  struct objfile *objfile;
  char *real_path = NULL;
  char *full_path = NULL;

  /* Here we are interested in canonicalizing an absolute path, not
     absolutizing a relative path.  */
  if (IS_ABSOLUTE_PATH (name))
    {
      full_path = xfullpath (name);
      make_cleanup (xfree, full_path);
      real_path = gdb_realpath (name);
      make_cleanup (xfree, real_path);
    }

got_symtab:

  /* First, search for an exact match */

  ALL_SYMTABS (objfile, s)
  {
    if (FILENAME_CMP (name, s->filename) == 0)
      {
	return s;
      }
      
    /* If the user gave us an absolute path, try to find the file in
       this symtab and use its absolute path.  */
    
    if (full_path != NULL)
      {
        const char *fp = symtab_to_fullname (s);
        if (fp != NULL && FILENAME_CMP (full_path, fp) == 0)
          {
            return s;
          }
      }

    if (real_path != NULL)
      {
        char *fullname = symtab_to_fullname (s);
        if (fullname != NULL)
          {
            char *rp = gdb_realpath (fullname);
            make_cleanup (xfree, rp);
            if (FILENAME_CMP (real_path, rp) == 0)
              {
                return s;
              }
          }
      }
  }

  /* Now, search for a matching tail (only if name doesn't have any dirs) */

  if (lbasename (name) == name)
    ALL_SYMTABS (objfile, s)
    {
      if (FILENAME_CMP (lbasename (s->filename), name) == 0)
	return s;
    }

  /* Same search rules as above apply here, but now we look thru the
     psymtabs.  */

  ps = lookup_partial_symtab (name);
  if (!ps)
    return (NULL);

  if (ps->readin)
    error (_("Internal: readin %s pst for `%s' found when no symtab found."),
	   ps->filename, name);

  s = PSYMTAB_TO_SYMTAB (ps);

  if (s)
    return s;

  /* At this point, we have located the psymtab for this file, but
     the conversion to a symtab has failed.  This usually happens
     when we are looking up an include file.  In this case,
     PSYMTAB_TO_SYMTAB doesn't return a symtab, even though one has
     been created.  So, we need to run through the symtabs again in
     order to find the file.
     XXX - This is a crock, and should be fixed inside of the the
     symbol parsing routines. */
  goto got_symtab;
}

/* APPLE LOCAL begin */

static struct symtab ** 
add_symtab_to_array (struct symtab **arr, void *sym, 
		     int *num_elem, int *max_num)
{
  if (*num_elem == *max_num - 1)
    {
      *max_num = (*max_num) * 2;
      arr = xrealloc (arr, (*max_num) * sizeof (struct symtab *));
    }
  arr[(*num_elem)++] = sym;
  arr[(*num_elem)] = NULL;
  return arr;
}

/* Check for a symtab of a specific name, a la
   lookup_symtab.  But don't stop on the first match.  Instead we
   return a malloc'ed array of all the matches, plus a final NULL
   element.  
   It is the caller's responsibility to free the array.  */

struct symtab **
lookup_symtab_all (const char *name)
{
  struct symtab *s;
  struct partial_symtab **pst_arr;
  struct objfile *objfile;
  char *real_path = NULL;
  char *full_path = NULL;

  struct symtab **sym_arr;
  int num_found;
  int max_num;

  num_found = 0;
  max_num = 5;
  sym_arr = (struct symtab **) xmalloc (max_num * sizeof (struct symtab *));
  sym_arr[0] = NULL;

  /* Here we are interested in canonicalizing an absolute path, not
     absolutizing a relative path.  */
  if (IS_ABSOLUTE_PATH (name))
    {
      full_path = xfullpath (name);
      make_cleanup (xfree, full_path);
      real_path = gdb_realpath (name);
      make_cleanup (xfree, real_path);

      /* If FULL_PATH and REAL_PATH are the same, we're going to be doing
         unnecessary extra comparisons.  */
      if (strcmp (real_path, full_path) == 0)
        real_path = NULL;
    }

  /* APPLE LOCAL: Look through the partial symtabs, and convert any 
     we find to symtab's.  That way all the matches will be guaranteed
     to be in the symtabs.  Then we can just scan the symtabs to
     build up the results we plan to return.

     We have to do this to work around the problem that
     psymtab_to_symtab doesn't always return the symtab for that
     psymtab (i.e. for dbxread when the psymtab is for code in an
     include file.)  */

  pst_arr = lookup_partial_symtab_all (name, 1);
  if (pst_arr)
    {
      int i;
      for (i = 0; pst_arr[i] != NULL; i++)
	{
	  
	  if (!pst_arr[i]->readin)
            {
              if (info_verbose)
                printf_filtered ("Looking for definition of '%s': ", name);
	      PSYMTAB_TO_SYMTAB (pst_arr[i]);
            }
	  
	}
      xfree (pst_arr);
    }

  /* Now search the symtabs.  First, search for an exact match */

  ALL_SYMTABS (objfile, s)
  {
    if (FILENAME_CMP (name, s->filename) == 0)
      {
	sym_arr = add_symtab_to_array (sym_arr, s, &num_found, &max_num);
	continue;
      }
      
    /* If the user gave us an absolute path, try to find the file in
       this symtab and use its absolute path.  */
    
    if (full_path != NULL)
      {
	const char *fp = symtab_to_fullname (s);
	if (fp != NULL && FILENAME_CMP (full_path, fp) == 0)
	  {
	    sym_arr = add_symtab_to_array (sym_arr, s, &num_found, &max_num);
	    continue;
	  }
      }

    if (real_path != NULL)
      {
        const char *fullname = symtab_to_fullname (s);
        if (fullname != NULL)
          {
            char *rp = gdb_realpath (fullname);
            make_cleanup (xfree, rp);
            if (FILENAME_CMP (real_path, rp) == 0)
              {
                sym_arr = add_symtab_to_array (sym_arr, s, &num_found, &max_num);
                continue;
              }
          }
      }
  }

  /* Now, search for a matching tail (only if name doesn't have any dirs) */
  /* But don't do this if the symtab's name is just a base name, since we
     already got that from the direct compare above.  */

  if (lbasename (name) == name)
    ALL_SYMTABS (objfile, s)
    {
      const char *s_basename = lbasename (s->filename);

      if (s_basename != s->filename && FILENAME_CMP (s_basename, name) == 0)
	{
	  sym_arr = add_symtab_to_array (sym_arr, s, &num_found, &max_num);
	  continue;
	}
    }

  if (num_found == 0)
    {
      xfree (sym_arr);
      return NULL;
    }
  else
    return sym_arr;
}
/* APPLE LOCAL end */

/* Lookup the partial symbol table of a source file named NAME.
   *If* there is no '/' in the name, a match after a '/'
   in the psymtab filename will also work.  */

struct partial_symtab *
lookup_partial_symtab (const char *name)
{
  struct partial_symtab *pst;
  struct objfile *objfile;
  char *full_path = NULL;
  char *real_path = NULL;

  /* Here we are interested in canonicalizing an absolute path, not
     absolutizing a relative path.  */
  if (IS_ABSOLUTE_PATH (name))
    {
      full_path = xfullpath (name);
      make_cleanup (xfree, full_path);
      real_path = gdb_realpath (name);
      make_cleanup (xfree, real_path);
    }

  ALL_PSYMTABS (objfile, pst)
  {
    if (FILENAME_CMP (name, pst->filename) == 0)
      {
	return (pst);
      }

    /* If the user gave us an absolute path, try to find the file in
       this symtab and use its absolute path.  */
    if (full_path != NULL)
      {
        if (pst->fullname == NULL)
          source_full_path_of (pst->filename, &pst->fullname);
        if (pst->fullname != NULL
            && FILENAME_CMP (full_path, pst->fullname) == 0)
          {
            return pst;
          }
      }

    if (real_path != NULL)
      {
        char *rp = NULL;
        if (pst->fullname == NULL)
          source_full_path_of (pst->filename, &pst->fullname);
        if (pst->fullname != NULL)
          {
            rp = gdb_realpath (pst->fullname);
            make_cleanup (xfree, rp);
          }
        if (rp != NULL && FILENAME_CMP (real_path, rp) == 0)
          {
            return pst;
          }
      }
  }

  /* Now, search for a matching tail (only if name doesn't have any dirs) */

  if (lbasename (name) == name)
    ALL_PSYMTABS (objfile, pst)
    {
      if (FILENAME_CMP (lbasename (pst->filename), name) == 0)
        return (pst);
    }

  return (NULL);
}

/* APPLE LOCAL: This finds all the partial symtabs that 
   match NAME.  */

static struct partial_symtab ** 
add_partial_symtab_to_array (struct partial_symtab **arr, void *sym, 
		     int *num_elem, int *max_num)
{
  if (*num_elem == *max_num - 1)
    {
      *max_num = (*max_num) * 2;
      arr = xrealloc (arr, (*max_num) * sizeof (struct partial_symtab *));
    }
  arr[(*num_elem)++] = sym;
  arr[(*num_elem)] = NULL;
  return arr;
}

/* APPLE LOCAL: This in basically the FSF lookup_partial_symtab, generalized 
   to optionally search all the partial_symbols and return more than one
   match.   
   N.B. If something changes in lookup_partial_symtab, be sure to
   change it here as well.  */

struct partial_symtab **
lookup_partial_symtab_all (const char *name, int only_unread)
{
  struct partial_symtab *pst;
  struct objfile *objfile;
  char *full_path = NULL;
  char *real_path = NULL;

  struct partial_symtab **psym_arr;
  int num_found;
  int max_num;

  num_found = 0;
  max_num = 5;
  psym_arr = (struct partial_symtab **) 
                          xmalloc (max_num * sizeof (struct partial_symtab *));
  psym_arr[0] = NULL;

  /* Here we are interested in canonicalizing an absolute path, not
     absolutizing a relative path.  */
  if (IS_ABSOLUTE_PATH (name))
    {
      full_path = xfullpath (name);
      make_cleanup (xfree, full_path);
      real_path = gdb_realpath (name);
      make_cleanup (xfree, real_path);

      /* If FULL_PATH and REAL_PATH are the same, we're going to be doing
         unnecessary extra comparisons.  */
      if (strcmp (real_path, full_path) == 0)
        real_path = NULL;
    }

  ALL_PSYMTABS (objfile, pst)
  {
    if (FILENAME_CMP (name, pst->filename) == 0)
      {
	if (only_unread && pst->readin)
	  continue;
	psym_arr = add_partial_symtab_to_array (psym_arr, pst, &num_found, 
                                                &max_num);
	continue;
      }

    /* If the user gave us an absolute path, try to find the file in
       this symtab and use its absolute path.  */
    if (full_path != NULL)
      {
	psymtab_to_fullname (pst);
	if (pst->fullname != NULL
	    && FILENAME_CMP (full_path, pst->fullname) == 0)
	  {
	    if (only_unread && pst->readin)
	      continue;
	    psym_arr = add_partial_symtab_to_array (psym_arr, pst, &num_found, 
                                                    &max_num);
	    continue;
	  }
      }

    if (real_path != NULL)
      {
        char *rp = NULL;
	psymtab_to_fullname (pst);
        if (pst->fullname != NULL)
          {
            rp = gdb_realpath (pst->fullname);
            make_cleanup (xfree, rp);
          }
	if (rp != NULL && FILENAME_CMP (real_path, rp) == 0)
	  {
	    if (only_unread && pst->readin)
	      continue;

	    psym_arr = add_partial_symtab_to_array (psym_arr, pst, &num_found, 
                                                    &max_num);
	    continue;
	  }
      }
  }

  /* Now, search for a matching tail (only if name doesn't have any dirs) */

  if (lbasename (name) == name)
    ALL_PSYMTABS (objfile, pst)
    {
      /* Don't pass here if the pst->filename doesn't have any dirs either,
	 since we've already gotten that above.  */
      const char *pst_name = lbasename (pst->filename);

      if (pst_name != pst->filename && FILENAME_CMP (pst_name, name) == 0)
	{
	  if (only_unread && pst->readin)
	    continue;
	  psym_arr = add_partial_symtab_to_array (psym_arr, pst, &num_found, 
                                                  &max_num);
	}
    }

  if (num_found == 0)
    {
      xfree (psym_arr);
      return NULL;
    }
  else
    return (psym_arr);
}
/* END APPLE LOCAL */

/* Mangle a GDB method stub type.  This actually reassembles the pieces of the
   full method name, which consist of the class name (from T), the unadorned
   method name from METHOD_ID, and the signature for the specific overload,
   specified by SIGNATURE_ID.  Note that this function is g++ specific. */

char *
gdb_mangle_name (struct type *type, int method_id, int signature_id)
{
  int mangled_name_len;
  char *mangled_name;
  struct fn_field *f = TYPE_FN_FIELDLIST1 (type, method_id);
  struct fn_field *method = &f[signature_id];
  char *field_name = TYPE_FN_FIELDLIST_NAME (type, method_id);
  char *physname = TYPE_FN_FIELD_PHYSNAME (f, signature_id);
  char *newname = type_name_no_tag (type);

  /* Does the form of physname indicate that it is the full mangled name
     of a constructor (not just the args)?  */
  int is_full_physname_constructor;

  int is_constructor;
  int is_destructor = is_destructor_name (physname);
  /* Need a new type prefix.  */
  char *const_prefix = method->is_const ? "C" : "";
  char *volatile_prefix = method->is_volatile ? "V" : "";
  char buf[20];
  int len = (newname == NULL ? 0 : strlen (newname));

  /* Nothing to do if physname already contains a fully mangled v3 abi name
     or an operator name.  */
  if ((physname[0] == '_' && physname[1] == 'Z')
      || is_operator_name (field_name))
    return xstrdup (physname);

  is_full_physname_constructor = is_constructor_name (physname);

  is_constructor =
    is_full_physname_constructor || (newname && strcmp (field_name, newname) == 0);

  if (!is_destructor)
    is_destructor = (strncmp (physname, "__dt", 4) == 0);

  if (is_destructor || is_full_physname_constructor)
    {
      mangled_name = (char *) xmalloc (strlen (physname) + 1);
      strcpy (mangled_name, physname);
      return mangled_name;
    }

  if (len == 0)
    {
      sprintf (buf, "__%s%s", const_prefix, volatile_prefix);
    }
  else if (physname[0] == 't' || physname[0] == 'Q')
    {
      /* The physname for template and qualified methods already includes
         the class name.  */
      sprintf (buf, "__%s%s", const_prefix, volatile_prefix);
      newname = NULL;
      len = 0;
    }
  else
    {
      sprintf (buf, "__%s%s%d", const_prefix, volatile_prefix, len);
    }
  mangled_name_len = ((is_constructor ? 0 : strlen (field_name))
		      + strlen (buf) + len + strlen (physname) + 1);

    {
      mangled_name = (char *) xmalloc (mangled_name_len);
      if (is_constructor)
	mangled_name[0] = '\0';
      else
	strcpy (mangled_name, field_name);
    }
  strcat (mangled_name, buf);
  /* If the class doesn't have a name, i.e. newname NULL, then we just
     mangle it using 0 for the length of the class.  Thus it gets mangled
     as something starting with `::' rather than `classname::'. */
  if (newname != NULL)
    strcat (mangled_name, newname);

  strcat (mangled_name, physname);
  return (mangled_name);
}


/* Initialize the language dependent portion of a symbol
   depending upon the language for the symbol. */
void
symbol_init_language_specific (struct general_symbol_info *gsymbol,
			       enum language language)
{
  gsymbol->language = language;
  if (gsymbol->language == language_cplus
      || gsymbol->language == language_java
      /* APPLE LOCAL Objective-C++ */
      || gsymbol->language == language_objc
      || gsymbol->language == language_objcplus)    
    {
      gsymbol->language_specific.cplus_specific.demangled_name = NULL;
    }
  else
    {
      memset (&gsymbol->language_specific, 0,
	      sizeof (gsymbol->language_specific));
    }
}

/* Functions to initialize a symbol's mangled name.  */

/* Create the hash table used for demangled names.  Each hash entry is
   a pair of strings; one for the mangled name and one for the demangled
   name.  The entry is hashed via just the mangled name.  */

static void
create_demangled_names_hash (struct objfile *objfile)
{
  /* Choose 256 as the starting size of the hash table, somewhat arbitrarily.
     The hash table code will round this up to the next prime number. 
     Choosing a much larger table size wastes memory, and saves only about
     1% in symbol reading.  */

  objfile->demangled_names_hash = htab_create_alloc
    (256, htab_hash_string, (int (*) (const void *, const void *)) streq,
     NULL, xcalloc, xfree);
}

/* Try to determine the demangled name for a symbol, based on the
   language of that symbol.  If the language is set to language_auto,
   it will attempt to find any demangling algorithm that works and
   then set the language appropriately.  The returned name is allocated
   by the demangler and should be xfree'd.  */

static char *
symbol_find_demangled_name (struct general_symbol_info *gsymbol,
			    const char *mangled)
{
  char *demangled = NULL;

  if (gsymbol->language == language_unknown)
    gsymbol->language = language_auto;

  if (gsymbol->language == language_objc
      /* APPLE LOCAL Objective-C++ */
      || gsymbol->language == language_objcplus
      || gsymbol->language == language_auto)
    {
      demangled =
	objc_demangle (mangled, 0);
      if (demangled != NULL)
	{
	  gsymbol->language = language_objc;
	  return demangled;
	}
    }
  if (gsymbol->language == language_cplus
      /* APPLE LOCAL Objective-C++ */
      || gsymbol->language == language_objcplus
      || gsymbol->language == language_auto)
    {
      demangled =
        cplus_demangle (mangled, DMGL_PARAMS | DMGL_ANSI);

      /* APPLE LOCAL: N.B. We are forcing the language to
	 C++ even for ObjC++ here.  This is so we will know
	 how to treat the function when we land in it without
	 having to try the demangling again every time.  */
      if (demangled != NULL)
	{
	  gsymbol->language = language_cplus;
	  return demangled;
	}
    }
  if (gsymbol->language == language_java)
    {
      demangled =
        cplus_demangle (mangled,
                        DMGL_PARAMS | DMGL_ANSI | DMGL_JAVA);
      if (demangled != NULL)
	{
	  gsymbol->language = language_java;
	  return demangled;
	}
    }
  return NULL;
}

/* Set both the mangled and demangled (if any) names for GSYMBOL based
   on LINKAGE_NAME and LEN.  The hash table corresponding to OBJFILE
   is used, and the memory comes from that objfile's objfile_obstack.
   LINKAGE_NAME is copied, so the pointer can be discarded after
   calling this function.  */

/* We have to be careful when dealing with Java names: when we run
   into a Java minimal symbol, we don't know it's a Java symbol, so it
   gets demangled as a C++ name.  This is unfortunate, but there's not
   much we can do about it: but when demangling partial symbols and
   regular symbols, we'd better not reuse the wrong demangled name.
   (See PR gdb/1039.)  We solve this by putting a distinctive prefix
   on Java names when storing them in the hash table.  */

/* FIXME: carlton/2003-03-13: This is an unfortunate situation.  I
   don't mind the Java prefix so much: different languages have
   different demangling requirements, so it's only natural that we
   need to keep language data around in our demangling cache.  But
   it's not good that the minimal symbol has the wrong demangled name.
   Unfortunately, I can't think of any easy solution to that
   problem.  */

#define JAVA_PREFIX "##JAVA$$"
#define JAVA_PREFIX_LEN 8

void
symbol_set_names (struct general_symbol_info *gsymbol,
		  const char *linkage_name, int len, struct objfile *objfile)
{
  char **slot;
  /* A 0-terminated copy of the linkage name.  */
  const char *linkage_name_copy;
  /* A copy of the linkage name that might have a special Java prefix
     added to it, for use when looking names up in the hash table.  */
  const char *lookup_name;
  /* The length of lookup_name.  */
  int lookup_len;

  if (objfile->demangled_names_hash == NULL)
    create_demangled_names_hash (objfile);

  /* The stabs reader generally provides names that are not
     NUL-terminated; most of the other readers don't do this, so we
     can just use the given copy, unless we're in the Java case.  */
  if (gsymbol->language == language_java)
    {
      char *alloc_name;
      lookup_len = len + JAVA_PREFIX_LEN;

      alloc_name = alloca (lookup_len + 1);
      memcpy (alloc_name, JAVA_PREFIX, JAVA_PREFIX_LEN);
      memcpy (alloc_name + JAVA_PREFIX_LEN, linkage_name, len);
      alloc_name[lookup_len] = '\0';

      lookup_name = alloc_name;
      linkage_name_copy = alloc_name + JAVA_PREFIX_LEN;
    }
  else if (linkage_name[len] != '\0')
    {
      char *alloc_name;
      lookup_len = len;

      alloc_name = alloca (lookup_len + 1);
      memcpy (alloc_name, linkage_name, len);
      alloc_name[lookup_len] = '\0';

      lookup_name = alloc_name;
      linkage_name_copy = alloc_name;
    }
  else
    {
      lookup_len = len;
      lookup_name = linkage_name;
      linkage_name_copy = linkage_name;
    }

  slot = (char **) htab_find_slot (objfile->demangled_names_hash,
				   lookup_name, INSERT);

  /* If this name is not in the hash table, add it.  */
  if (*slot == NULL)
    {
      char *demangled_name = symbol_find_demangled_name (gsymbol,
							 linkage_name_copy);
      int demangled_len = demangled_name ? strlen (demangled_name) : 0;

      /* If there is a demangled name, place it right after the mangled name.
	 Otherwise, just place a second zero byte after the end of the mangled
	 name.  */
      *slot = obstack_alloc (&objfile->objfile_obstack,
			     lookup_len + demangled_len + 2);
      memcpy (*slot, lookup_name, lookup_len + 1);
      if (demangled_name != NULL)
	{
	  memcpy (*slot + lookup_len + 1, demangled_name, demangled_len + 1);
	  xfree (demangled_name);
	}
      else
	(*slot)[lookup_len + 1] = '\0';
    }
  else
    {
      /* APPLE LOCAL: We already have this name in the demangled name hash
         but we still need to set the language in the minsym.  */
      xfree (symbol_find_demangled_name (gsymbol, linkage_name_copy));
    }

  gsymbol->name = *slot + lookup_len - len;
  if ((*slot)[lookup_len + 1] != '\0')
    gsymbol->language_specific.cplus_specific.demangled_name
      = &(*slot)[lookup_len + 1];
  else
    gsymbol->language_specific.cplus_specific.demangled_name = NULL;
}

/* Initialize the demangled name of GSYMBOL if possible.  Any required space
   to store the name is obtained from the specified obstack.  The function
   symbol_set_names, above, should be used instead where possible for more
   efficient memory usage.  */

void
symbol_init_demangled_name (struct general_symbol_info *gsymbol,
                            struct obstack *obstack)
{
  char *mangled = gsymbol->name;
  char *demangled = NULL;

  demangled = symbol_find_demangled_name (gsymbol, mangled);
  if (gsymbol->language == language_cplus
      || gsymbol->language == language_java
      /* APPLE LOCAL Objective-C++ */
      || gsymbol->language == language_objc
      || gsymbol->language == language_objcplus)
    {
      if (demangled)
	{
	  gsymbol->language_specific.cplus_specific.demangled_name
	    = obsavestring (demangled, strlen (demangled), obstack);
	  xfree (demangled);
	}
      else
	gsymbol->language_specific.cplus_specific.demangled_name = NULL;
    }
  else
    {
      /* Unknown language; just clean up quietly.  */
      if (demangled)
	xfree (demangled);
    }
}

/* Return the source code name of a symbol.  In languages where
   demangling is necessary, this is the demangled name.  */

char *
symbol_natural_name (const struct general_symbol_info *gsymbol)
{
  switch (gsymbol->language) 
    {
    case language_cplus:
    case language_java:
    case language_objc:
    /* APPLE LOCAL Objective-C++ */
    case language_objcplus:
      if (gsymbol->language_specific.cplus_specific.demangled_name != NULL)
	return gsymbol->language_specific.cplus_specific.demangled_name;
      break;
    case language_ada:
      if (gsymbol->language_specific.cplus_specific.demangled_name != NULL)
	return gsymbol->language_specific.cplus_specific.demangled_name;
      else
	return ada_decode_symbol (gsymbol);
      break;
    default:
      break;
    }
  return gsymbol->name;
}

/* Return the demangled name for a symbol based on the language for
   that symbol.  If no demangled name exists, return NULL. */
char *
symbol_demangled_name (struct general_symbol_info *gsymbol)
{
  switch (gsymbol->language) 
    {
    case language_cplus:
    case language_java:
    case language_objc:
    /* APPLE LOCAL Objective-C++ */
    case language_objcplus:
      if (gsymbol->language_specific.cplus_specific.demangled_name != NULL)
	return gsymbol->language_specific.cplus_specific.demangled_name;
      break;
    case language_ada:
      if (gsymbol->language_specific.cplus_specific.demangled_name != NULL)
	return gsymbol->language_specific.cplus_specific.demangled_name;
      else
	return ada_decode_symbol (gsymbol);
      break;
    default:
      break;
    }
  return NULL;
}

/* Return the search name of a symbol---generally the demangled or
   linkage name of the symbol, depending on how it will be searched for.
   If there is no distinct demangled name, then returns the same value 
   (same pointer) as SYMBOL_LINKAGE_NAME. */
char *
symbol_search_name (const struct general_symbol_info *gsymbol)
{
  if (gsymbol->language == language_ada)
    return gsymbol->name;
  else
    return symbol_natural_name (gsymbol);
}

/* Initialize the structure fields to zero values.  */
void
init_sal (struct symtab_and_line *sal)
{
  sal->symtab = 0;
  sal->section = 0;
  sal->line = 0;
  sal->pc = 0;
  sal->end = 0;
  /* APPLE LOCAL begin subroutine inlining  */
  sal->entry_type = 0;
  sal->next = 0;
  /* APPLE LOCAL end subroutine inlinine  */
}

/* APPLE LOCAL begin addr_ctx.  */
void
init_address_context (struct address_context *addr_ctx)
{
  memset(addr_ctx, 0, sizeof(struct address_context));
}

/* APPLE LOCAL end addr_ctx.  */

/* Find which partial symtab contains PC and SECTION.  Return 0 if
   none.  We return the psymtab that contains a symbol whose address
   exactly matches PC, or, if we cannot find an exact match, the
   psymtab that contains a symbol whose address is closest to PC.  */
struct partial_symtab *
find_pc_sect_psymtab (CORE_ADDR pc, asection *section)
{
  struct partial_symtab *pst;
  struct objfile *objfile;
  struct minimal_symbol *msymbol;

  /* APPLE LOCAL begin cache lookup values for improved performance  */
  if (pc == last_psymtab_lookup_pc
      && pc == last_mapped_section_lookup_pc
      && section == cached_mapped_section
      && cached_psymtab)
    return cached_psymtab;

  last_psymtab_lookup_pc = pc;
  /* APPLE LOCAL end cache lookup values for improved performance  */

  /* If we know that this is not a text address, return failure.  This is
     necessary because we loop based on texthigh and textlow, which do
     not include the data ranges.  */
  msymbol = lookup_minimal_symbol_by_pc_section (pc, section);
  if (msymbol
      && (msymbol->type == mst_data
	  || msymbol->type == mst_bss
	  || msymbol->type == mst_abs
	  || msymbol->type == mst_file_data
	  || msymbol->type == mst_file_bss))
    /* APPLE LOCAL begin cache lookup values for improved performance  */
    {
      cached_psymtab = NULL;
      return NULL;
    }
    /* APPLE LOCAL end cache lookup values for improved performance  */

  /* APPLE LOCAL: Change to ALL_OBJFILES from ALL_PSYMTABS so that
     we can hoist the psymtab-invariant sections check out.  */
  ALL_OBJFILES (objfile)
  {
    /* APPLE LOCAL: We were passed in the section, so don't look in 
       objfiles that don't even share the bfd with that section...  */

    if (skip_non_matching_bfd (section, objfile))
      continue;
    
    ALL_OBJFILE_PSYMTABS (objfile, pst)
      {
	if (pc >= pst->textlow && pc < pst->texthigh)
	  {
	    struct partial_symtab *tpst;
	    struct partial_symtab *best_pst = pst;
	    struct partial_symbol *best_psym = NULL;
	    
	    /* An objfile that has its functions reordered might have
	       many partial symbol tables containing the PC, but
	       we want the partial symbol table that contains the
	       function containing the PC.  */
	    if (!(objfile->flags & OBJF_REORDERED) &&
		section == 0)	/* can't validate section this way */
	      /* APPLE LOCAL begin cache lookup values for improved 
		 performance  */
	      {
		cached_psymtab = pst;
		return (pst);
	      }
	      /* APPLE LOCAL end cache lookup values for improved 
		 performance  */
	    
	    if (msymbol == NULL)
	      /* APPLE LOCAL begin cache lookup values for improved 
		 performance  */
	      {
		cached_psymtab = pst;
		return (pst);
	      }
	      /* APPLE LOCAL end cache lookup values for improved 
		 performance  */
	    
	    /* The code range of partial symtabs sometimes overlap, so, in
	       the loop below, we need to check all partial symtabs and
	       find the one that fits better for the given PC address. We
	       select the partial symtab that contains a symbol whose
	       address is closest to the PC address.  By closest we mean
	       that find_pc_sect_symbol returns the symbol with address
	       that is closest and still less than the given PC.  */
	    for (tpst = pst; tpst != NULL; tpst = tpst->next)
	      {
		if (pc >= tpst->textlow && pc < tpst->texthigh)
		  {
		    struct partial_symbol *p;
		    
		    p = find_pc_sect_psymbol (tpst, pc, section);
		    if (p != NULL
			&& SYMBOL_VALUE_ADDRESS (p)
			== SYMBOL_VALUE_ADDRESS (msymbol))
		      /* APPLE LOCAL begin cache lookup values for improved 
			 performance  */
		      {
			cached_psymtab = tpst;
			return (tpst);
		      }
		    /* APPLE LOCAL end cache lookup values for improved 
		       performance  */
		    if (p != NULL)
		      {
			/* We found a symbol in this partial symtab which
			   matches (or is closest to) PC, check whether it
			   is closer than our current BEST_PSYM.  Since
			   this symbol address is necessarily lower or
			   equal to PC, the symbol closer to PC is the
			   symbol which address is the highest.  */
			/* This way we return the psymtab which contains
			   such best match symbol. This can help in cases
			   where the symbol information/debuginfo is not
			   complete, like for instance on IRIX6 with gcc,
			   where no debug info is emitted for
			   statics. (See also the nodebug.exp
			   testcase.)  */
			if (best_psym == NULL
			    || SYMBOL_VALUE_ADDRESS (p)
			    > SYMBOL_VALUE_ADDRESS (best_psym))
			  {
			    best_psym = p;
			    best_pst = tpst;
			  }
		      }
		  }
	      }
	    /* APPLE LOCAL cache lookup values for improved performance  */
	    cached_psymtab = best_pst;
	    return (best_pst);
	  }
      }
  }
  /* APPLE LOCAL cache lookup values for improved performance  */
  cached_psymtab = NULL;
  return (NULL);
}

/* Find which partial symtab contains PC.  Return 0 if none. 
   Backward compatibility, no section */

struct partial_symtab *
find_pc_psymtab (CORE_ADDR pc)
{
  return find_pc_sect_psymtab (pc, find_pc_mapped_section (pc));
}

/* Find which partial symbol within a psymtab matches PC and SECTION.  
   Return 0 if none.  Check all psymtabs if PSYMTAB is 0.  */

struct partial_symbol *
find_pc_sect_psymbol (struct partial_symtab *psymtab, CORE_ADDR pc,
		      asection *section)
{
  struct partial_symbol *best = NULL, *p, **pp;
  CORE_ADDR best_pc;

  if (!psymtab)
    psymtab = find_pc_sect_psymtab (pc, section);
  if (!psymtab)
    return 0;

  /* Cope with programs that start at address 0 */
  best_pc = (psymtab->textlow != 0) ? psymtab->textlow - 1 : 0;

  /* Search the global symbols as well as the static symbols, so that
     find_pc_partial_function doesn't use a minimal symbol and thus
     cache a bad endaddr.  */
  for (pp = psymtab->objfile->global_psymbols.list + psymtab->globals_offset;
    (pp - (psymtab->objfile->global_psymbols.list + psymtab->globals_offset)
     < psymtab->n_global_syms);
       pp++)
    {
      p = *pp;
      if (SYMBOL_DOMAIN (p) == VAR_DOMAIN
	  && SYMBOL_CLASS (p) == LOC_BLOCK
	  && pc >= SYMBOL_VALUE_ADDRESS (p)
	  && (SYMBOL_VALUE_ADDRESS (p) > best_pc
	      || (psymtab->textlow == 0
		  && best_pc == 0 && SYMBOL_VALUE_ADDRESS (p) == 0)))
	{
	  if (section)		/* match on a specific section */
	    {
	      fixup_psymbol_section (p, psymtab->objfile);
	      if (SYMBOL_BFD_SECTION (p) != section)
		continue;
	    }
	  best_pc = SYMBOL_VALUE_ADDRESS (p);
	  best = p;
	}
    }

  for (pp = psymtab->objfile->static_psymbols.list + psymtab->statics_offset;
    (pp - (psymtab->objfile->static_psymbols.list + psymtab->statics_offset)
     < psymtab->n_static_syms);
       pp++)
    {
      p = *pp;
      if (SYMBOL_DOMAIN (p) == VAR_DOMAIN
	  && SYMBOL_CLASS (p) == LOC_BLOCK
	  && pc >= SYMBOL_VALUE_ADDRESS (p)
	  && (SYMBOL_VALUE_ADDRESS (p) > best_pc
	      || (psymtab->textlow == 0
		  && best_pc == 0 && SYMBOL_VALUE_ADDRESS (p) == 0)))
	{
	  if (section)		/* match on a specific section */
	    {
	      fixup_psymbol_section (p, psymtab->objfile);
	      if (SYMBOL_BFD_SECTION (p) != section)
		continue;
	    }
	  best_pc = SYMBOL_VALUE_ADDRESS (p);
	  best = p;
	}
    }

  return best;
}

/* Find which partial symbol within a psymtab matches PC.  Return 0 if none.  
   Check all psymtabs if PSYMTAB is 0.  Backwards compatibility, no section. */

struct partial_symbol *
find_pc_psymbol (struct partial_symtab *psymtab, CORE_ADDR pc)
{
  return find_pc_sect_psymbol (psymtab, pc, find_pc_mapped_section (pc));
}

/* Debug symbols usually don't have section information.  We need to dig that
   out of the minimal symbols and stash that in the debug symbol.  */

static void
fixup_section (struct general_symbol_info *ginfo, struct objfile *objfile)
{
  struct minimal_symbol *msym;
  /* APPLE LOCAL BEGIN: use the section info from the executable objfile
     if we are fixing up section information for a dSYM based object.  */
  if (objfile && objfile->separate_debug_objfile_backlink)
    {
      fixup_section (ginfo, objfile->separate_debug_objfile_backlink);
      return;
    }
  /* APPLE LOCAL END.  */
  msym = lookup_minimal_symbol (ginfo->name, NULL, objfile);

  /* APPLIE LOCAL BEGIN: try with the prefix if we don't find a msym.  */
  if (msym == NULL && objfile && objfile->prefix && objfile->prefix[0] && 
      ginfo->name && ginfo->name[0])
    {
      int prefixed_name_len = strlen (objfile->prefix) + strlen (ginfo->name)
			      + 1;
      char *prefixed_name = (char *)xmalloc (prefixed_name_len);
      if (prefixed_name)
	{
	  sprintf (prefixed_name, "%s%s", objfile->prefix, ginfo->name);
	  msym = lookup_minimal_symbol (prefixed_name, NULL, objfile);
	  xfree (prefixed_name);
	}
    }
  /* APPLE LOCAL END.  */


  if (msym)
    {
      /* APPLE LOCAL huh? */
      if (ginfo->bfd_section != SYMBOL_BFD_SECTION (msym))
	ginfo->bfd_section = SYMBOL_BFD_SECTION (msym);
      /* APPLE LOCAL huh? */
      if (ginfo->section != SYMBOL_SECTION (msym))
	ginfo->section = SYMBOL_SECTION (msym);
    }
  else if (objfile)
    {
      /* Static, function-local variables do appear in the linker
	 (minimal) symbols, but are frequently given names that won't
	 be found via lookup_minimal_symbol().  E.g., it has been
	 observed in frv-uclinux (ELF) executables that a static,
	 function-local variable named "foo" might appear in the
	 linker symbols as "foo.6" or "foo.3".  Thus, there is no
	 point in attempting to extend the lookup-by-name mechanism to
	 handle this case due to the fact that there can be multiple
	 names.
	 
	 So, instead, search the section table when lookup by name has
	 failed.  The ``addr'' and ``endaddr'' fields may have already
	 been relocated.  If so, the relocation offset (i.e. the
	 ANOFFSET value) needs to be subtracted from these values when
	 performing the comparison.  We unconditionally subtract it,
	 because, when no relocation has been performed, the ANOFFSET
	 value will simply be zero.
	 
	 The address of the symbol whose section we're fixing up HAS
	 NOT BEEN adjusted (relocated) yet.  It can't have been since
	 the section isn't yet known and knowing the section is
	 necessary in order to add the correct relocation value.  In
	 other words, we wouldn't even be in this function (attempting
	 to compute the section) if it were already known.

	 Note that it is possible to search the minimal symbols
	 (subtracting the relocation value if necessary) to find the
	 matching minimal symbol, but this is overkill and much less
	 efficient.  It is not necessary to find the matching minimal
	 symbol, only its section.  
	 
	 Note that this technique (of doing a section table search)
	 can fail when unrelocated section addresses overlap.  For
	 this reason, we still attempt a lookup by name prior to doing
	 a search of the section table.  */
	 
      CORE_ADDR addr;
      struct obj_section *s;

      addr = ginfo->value.address;

      ALL_OBJFILE_OSECTIONS (objfile, s)
	{
	  /* APPLE LOCAL - We compute the index into the section_offsets 
	     array using pointer arithmetic here. The FSF version grabs the
	     index from S->THE_BFD_SECTION->INDEX which is correct only if the
	     number of sections in the OBJFILE->OBFD is the same as the 
	     OBJFILE->NUM_SECTIONS. This often is not the case, and the old
	     ANOFFSET macro that was used can easily walk right off the end
	     of the OBJFILE->SECTION_OFFSETS array and grab random data. The
	     OBJFILE->SECTIONS array is contiguous in memory and so we can
	     use pointer math to get the correct index. OBJFILE->SECTION_OFFSETS
	     is created using the number of OBJFILE->SECTIONS that made it into
	     the final list so the index will always be correct.  */
	  int idx = s - objfile->sections;
	  CORE_ADDR offset = objfile_section_offset (objfile, idx);

	  if (s->addr - offset <= addr && addr < s->endaddr - offset)
	    {
	      ginfo->bfd_section = s->the_bfd_section;
	      ginfo->section = idx;
	      return;
	    }
	}
    }
}

struct symbol *
fixup_symbol_section (struct symbol *sym, struct objfile *objfile)
{
  if (!sym)
    return NULL;

  if (SYMBOL_BFD_SECTION (sym))
    return sym;

  fixup_section (&sym->ginfo, objfile);

  return sym;
}

struct partial_symbol *
fixup_psymbol_section (struct partial_symbol *psym, struct objfile *objfile)
{
  if (!psym)
    return NULL;

  if (SYMBOL_BFD_SECTION (psym))
    return psym;

  fixup_section (&psym->ginfo, objfile);

  return psym;
}

/* This function is called from decode_all_variables; it is loosely
   based on lookup_symbol, but is tailored a bit based on the
   assumption that the symbol being looked up is supposed to be a
   function name (since decode_all_variables currently only gets
   call via attempts to set breakpoints by name.  FIXME:  In the
   future if we decide to try to find all occurrences of other types
   of symbols, this function will probably not work correctly.  

   NB: SYM_LIST is a linked list of xmalloc'ed symbol_search structures 
   which need to be xfree()'d individually by the caller.  */

int
lookup_symbol_all (const char *name, const struct block *block, 
		   const domain_enum domain, int *is_a_field_of_this,
		   struct symtab **symtab, struct symbol_search **sym_list)
{
  char *demangled_name = NULL;
  const char *modified_name = NULL;
  const char *mangled_name = NULL;
  struct symbol *returnval = NULL;

  modified_name = name;

  /* If we are using C++ or Java, demangle the name before doing a lookup, so
     we can always binary search. */
  if (current_language->la_language == language_cplus ||
      current_language->la_language == language_objcplus)
    {
      demangled_name = cplus_demangle (name, DMGL_ANSI | DMGL_PARAMS);
      if (demangled_name)
	{
	  mangled_name = name;
	  modified_name = demangled_name;
	}
    }
  else if (current_language->la_language == language_java)
    {
      demangled_name = cplus_demangle (name, 
		      		       DMGL_ANSI | DMGL_PARAMS | DMGL_JAVA);
      if (demangled_name)
	{
	  mangled_name = name;
	  modified_name = demangled_name;
	}
    }

  if (case_sensitivity == case_sensitive_off)
    {
      char *copy;
      int len, i;

      len = strlen (name);
      copy = (char *) alloca (len + 1);
      for (i= 0; i < len; i++)
        copy[i] = tolower (name[i]);
      copy[len] = 0;
      modified_name = copy;
    }

  if (current_language->la_language == language_c
      || current_language->la_language == language_objc)
    {
      const struct block *static_block = block_static_block (block);
      if (static_block != NULL)
	{
	  *sym_list = lookup_block_symbol_all (static_block, modified_name, 
                                               mangled_name, domain);
	  if (*sym_list != NULL
	      && *symtab == NULL)
	    *symtab = (*sym_list)->symtab;
	  if (*sym_list)
	    returnval = (*sym_list)->symbol;
	}
    }

  /* APPLE LOCAL begin radar 6366048 search *ALL* symbols  bp matches.  */
  lookup_symbol_aux_symtabs (GLOBAL_BLOCK, modified_name, 
			     mangled_name, domain, symtab, 
			     sym_list, 1);

  lookup_symbol_aux_symtabs (STATIC_BLOCK, modified_name, 
			     mangled_name, domain, symtab, 
			     sym_list, 1);

  lookup_symbol_aux_psymtabs (GLOBAL_BLOCK, modified_name, 
			      mangled_name, domain, symtab, 
			      sym_list, 1);

  lookup_symbol_aux_psymtabs (STATIC_BLOCK, modified_name, 
			      mangled_name, domain, symtab, 
			      sym_list, 1);
  /* APPLE LOCAL end radar 6366048 search *ALL* symbols  bp matches.  */

  if (demangled_name)
    xfree (demangled_name);

  if (*sym_list)
    {
      struct symbol_search *test_pos;
      struct symbol_search *cur;
      struct symbol_search *prev;

      /* Remove duplicate symbols from SYM_LIST:  TEST_POS moves slowly
	 down the list; at each point, compare the symbol in TEST_POS
	 against the rest of the list and remove any duplicate occurrences
	 of it from the rest of the list.  */

      for (test_pos = *sym_list; test_pos; test_pos = test_pos->next)
	{
	  prev = test_pos;
	  cur = prev->next;
	  while (cur)
	    {
	      if (cur->symbol == test_pos->symbol)
                {
		  prev->next = cur->next;
                  /* FIXME: free the memory from any symbols we've
                     removed from the list.  */
                }
	      else
                {
		  prev = cur;
                }
	      cur = cur->next;
	    }
	}
      return 1;
    }
  else
    return 0;
}

/* Find the definition for a specified symbol name NAME
   in domain DOMAIN, visible from lexical block BLOCK.
   Returns the struct symbol pointer, or zero if no symbol is found.
   If SYMTAB is non-NULL, store the symbol table in which the
   symbol was found there, or NULL if not found.
   C++: if IS_A_FIELD_OF_THIS is nonzero on entry, check to see if
   NAME is a field of the current implied argument `this'.  If so set
   *IS_A_FIELD_OF_THIS to 1, otherwise set it to zero. 
   BLOCK_FOUND is set to the block in which NAME is found (in the case of
   a field of `this', value_of_this sets BLOCK_FOUND to the proper value.) */

/* This function has a bunch of loops in it and it would seem to be
   attractive to put in some QUIT's (though I'm not really sure
   whether it can run long enough to be really important).  But there
   are a few calls for which it would appear to be bad news to quit
   out of here: find_proc_desc in alpha-tdep.c and mips-tdep.c.  (Note
   that there is C++ code below which can error(), but that probably
   doesn't affect these calls since they are looking for a known
   variable and thus can probably assume it will never hit the C++
   code).  */

struct symbol *
lookup_symbol (const char *name, const struct block *block,
	       const domain_enum domain, int *is_a_field_of_this,
	       struct symtab **symtab)
{
  char *demangled_name = NULL;
  const char *modified_name = NULL;
  const char *mangled_name = NULL;
  int needtofreename = 0;
  struct symbol *returnval;

  modified_name = name;

  /* If we are using C++ or Java, demangle the name before doing a lookup, so
     we can always binary search. */
  /* APPLE LOCAL Objective-C++ */
  if (current_language->la_language == language_cplus ||
      current_language->la_language == language_objcplus)
    {
      demangled_name = cplus_demangle (name, DMGL_ANSI | DMGL_PARAMS);
      if (demangled_name)
	{
	  mangled_name = name;
	  modified_name = demangled_name;
	  needtofreename = 1;
	}
    }
  else if (current_language->la_language == language_java)
    {
      demangled_name = cplus_demangle (name, 
		      		       DMGL_ANSI | DMGL_PARAMS | DMGL_JAVA);
      if (demangled_name)
	{
	  mangled_name = name;
	  modified_name = demangled_name;
	  needtofreename = 1;
	}
    }

  if (case_sensitivity == case_sensitive_off)
    {
      char *copy;
      int len, i;

      len = strlen (name);
      copy = (char *) alloca (len + 1);
      for (i= 0; i < len; i++)
        copy[i] = tolower (name[i]);
      copy[len] = 0;
      modified_name = copy;
    }

  returnval = lookup_symbol_aux (modified_name, mangled_name, block,
				 domain, is_a_field_of_this, symtab);
  if (needtofreename)
    xfree (demangled_name);

  return returnval;	 
}

/* Behave like lookup_symbol_aux except that NAME is the natural name
   of the symbol that we're looking for and, if LINKAGE_NAME is
   non-NULL, ensure that the symbol's linkage name matches as
   well.  */

static struct symbol *
lookup_symbol_aux (const char *name, const char *linkage_name,
		   const struct block *block, const domain_enum domain,
		   int *is_a_field_of_this, struct symtab **in_symtab_ptr)
{
  struct symbol *sym = NULL;
  struct symtab *found_symtab = NULL;
  struct symtab **symtab = &found_symtab;

  /* Make sure we do something sensible with is_a_field_of_this, since
     the callers that set this parameter to some non-null value will
     certainly use it later and expect it to be either 0 or 1.
     If we don't set it, the contents of is_a_field_of_this are
     undefined.  */
  if (is_a_field_of_this != NULL)
    *is_a_field_of_this = 0;

  /* Search specified block and its superiors.  Don't search
     STATIC_BLOCK or GLOBAL_BLOCK.  */

  sym = lookup_symbol_aux_local (name, linkage_name, block, domain,
				 symtab);
  if (sym != NULL)
    goto foundit;

  /* If requested to do so by the caller and if appropriate for the
     current language, check to see if NAME is a field of `this'. */

  if (current_language->la_value_of_this != NULL
      && is_a_field_of_this != NULL)
    {
      struct value *v = current_language->la_value_of_this (0);

      if (v && check_field (v, name))
	{
	  /* APPLE LOCAL: If we find that we are in a class method, I
	     want to treat the case where NAME is the name of the
	     class specially.  Both returning with is_a_field_of_this
	     as 1, or falling through to the la_lookup_symbol_nonlocal
	     below tends to pull up the constructor rather than the
	     class type symbol most of the time.  But that is almost
	     never what you want.  In fact, you almost always just
	     want the class type symbol.  
	     For instance, you get here if you are doing:
	         print (Foo *) this
	     for class Foo.  You don't get here when you do:
                 break Foo::Foo
	     since that doesn't set IS_A_FIELD_OF_THIS.
	     So I check here if the NAME passed in is either the class
	     name of "this" or one of its ancestors, and if it is
	     preferentially return that.  */

	  struct type *val_type;
	  
	  val_type = value_type (v);
	  if (val_type) 
	    {
	      CHECK_TYPEDEF (val_type);
	      if (TYPE_CODE (val_type) == TYPE_CODE_PTR)
		val_type = TYPE_TARGET_TYPE (val_type);
	      
	      if (TYPE_CODE (val_type) == TYPE_CODE_STRUCT)
		{
		  char *this_class_name;
		  this_class_name = TYPE_NAME (val_type);
		  if ((this_class_name != NULL && strcmp (this_class_name, name) == 0)
		      || is_ancestor_by_name (name, val_type))
		    {
		      const struct block *global_block = block_global_block (block);
		      
		      if (global_block != NULL)
			sym = lookup_symbol_aux_block (name, linkage_name, global_block,
						       domain, symtab);
		      if (sym != NULL)
			goto foundit;
		    }
		}
	    }
	  *is_a_field_of_this = 1;
	  if (symtab != NULL)
	    *symtab = NULL;
	  return NULL;
	}
    }

  /* Now do whatever is appropriate for the current language to look
     up static and global variables.  */

  sym = current_language->la_lookup_symbol_nonlocal (name, linkage_name,
						     block, domain,
						     symtab);
  if (sym != NULL)
    goto foundit;

  /* Now search all static file-level symbols.  Not strictly correct,
     but more useful than an error.  Do the symtabs first, then check
     the psymtabs.  If a psymtab indicates the existence of the
     desired name as a file-level static, then do psymtab-to-symtab
     conversion on the fly and return the found symbol. */

  /* APPLE LOCAL begin return multiple symbols  */
  sym = lookup_symbol_aux_symtabs (STATIC_BLOCK, name, linkage_name,
				   domain, symtab, NULL, 0);
  /* APPLE LOCAL end return multiple symbols  */

  if (sym != NULL)
    goto foundit;

  /* APPLE LOCAL begin return multiple symbols  */
  sym = lookup_symbol_aux_psymtabs (STATIC_BLOCK, name, linkage_name,
				    domain, symtab, NULL, 0);
  /* APPLE LOCAL end return multiple symbols  */

 foundit:
 
  if (found_symtab != NULL)
    objfile_add_to_hitlist (found_symtab->objfile);
  if (in_symtab_ptr != NULL)
    *in_symtab_ptr = found_symtab;
    
  return sym;
}

/* Check to see if the symbol is defined in BLOCK or its superiors.
   Don't search STATIC_BLOCK or GLOBAL_BLOCK.  */

static struct symbol *
lookup_symbol_aux_local (const char *name, const char *linkage_name,
			 const struct block *block,
			 const domain_enum domain,
			 struct symtab **symtab)
{
  struct symbol *sym;
  const struct block *static_block = block_static_block (block);

  /* Check if either no block is specified or it's a global block.  */

  if (static_block == NULL)
    return NULL;

  while (block != static_block)
    {
      sym = lookup_symbol_aux_block (name, linkage_name, block, domain,
				     symtab);
      if (sym != NULL)
	return sym;
      block = BLOCK_SUPERBLOCK (block);
    }

  /* We've reached the static block without finding a result.  */

  return NULL;
}

/* Look up a symbol in a block; if found, locate its symtab, fixup the
   symbol, and set block_found appropriately.  */

struct symbol *
lookup_symbol_aux_block (const char *name, const char *linkage_name,
			 const struct block *block,
			 const domain_enum domain,
			 struct symtab **symtab)
{
  struct symbol *sym;
  struct objfile *objfile = NULL;
  struct blockvector *bv;
  struct block *b;
  struct symtab *s = NULL;

  sym = lookup_block_symbol (block, name, linkage_name, domain);
  /* APPLE LOCAL fix-and-continue */
  if (sym && !SYMBOL_OBSOLETED (sym))
    {
      block_found = block;
      if (symtab != NULL)
	{
	  /* Search the list of symtabs for one which contains the
	     address of the start of this block.  */
          /* APPLE LOCAL fix-and-continue: Search obsoleted blocks.  If
             gdb found the blockvector to an obsoleted section, it's no
             good to pretend it didn't exist at this point.  */
	  ALL_SYMTABS_INCL_OBSOLETED (objfile, s)
	    {
	      bv = BLOCKVECTOR (s);
	      b = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
	      /* APPLE LOCAL begin address ranges  */
	      /* Assumption:  I'm assuming that the GLOBAL_BLOCK has a single
		 contiguous address range...  */
	      if (BLOCK_START (b) <= BLOCK_LOWEST_PC (block)
		  && BLOCK_END (b) > BLOCK_LOWEST_PC (block))
	      /* APPLE LOCAL end address ranges  */
		goto found;
	    }
	found:
	  *symtab = s;
	}
      
      return fixup_symbol_section (sym, objfile);
    }

  return NULL;
}

/* Check to see if the symbol is defined in one of the symtabs.
   BLOCK_INDEX should be either GLOBAL_BLOCK or STATIC_BLOCK,
   depending on whether or not we want to search global symbols or
   static symbols.  

   APPLE LOCAL: This function can be used in two different ways:
   It can be used to find a single symbol by name -- in which case
   a pointer to the symbol is returned and *SYMTAB is set to the
   symtab, if we found one.

   The second way is to return a comprehensive list of all symbols
   found in all symtabs.  In that case, *SYM_LIST is set to a linked
   list of matching symbols which have been xmalloc()'ed, a pointer
   to one of the symbols is returned and *SYMTAB is set to one of
   the matched symbols' symtabs.  It is the responsibility of the
   caller to free *SYM_LIST.  */

static struct symbol *
lookup_symbol_aux_symtabs (int block_index,
			   const char *name, const char *linkage_name,
			   const domain_enum domain,
			   struct symtab **symtab,
			   struct symbol_search **sym_list,
			   int find_all_occurrences)
{
  struct symbol *sym;
  struct objfile *objfile;
  struct blockvector *bv;
  const struct block *block;
  struct symtab *s;
  struct symbol_search *tmp_list;
  struct symbol_search *prev;
  struct symbol_search *current;

  /* If we're called with a null string for some bizarre reason, just bail.  */
  if (name == NULL || name[0] == '\0'
      || (linkage_name && linkage_name[0] == '\0'))
    {
      if (sym_list)
        *sym_list = NULL;
      if (symtab)
        *symtab = NULL;
      return NULL;
    }

  /* APPLE LOCAL fix-and-continue */
  ALL_SYMTABS_INCL_OBSOLETED (objfile, s)
  {
    tmp_list = NULL;
    if (s->primary == 0)
      continue;
    bv = BLOCKVECTOR (s);
    block = BLOCKVECTOR_BLOCK (bv, block_index);
    if (find_all_occurrences)
      {
	tmp_list = lookup_block_symbol_all (block, name, linkage_name, domain);
	for (prev = NULL, current = tmp_list; current; )
	  {
	    if (SYMBOL_OBSOLETED (current->symbol))
	      {
		if (!prev)
                  {
		    tmp_list = current->next;
                  }
		else
                  {
		    prev->next = current->next;
                  }
	      }
	    else
	      {
	        prev = current;
	      }
	    current = current->next;
            /* FIXME: free the memory from any obsolete symbols we've
               removed from the list.  */
	  }

        /* Add this symtab's matching symbols on to the list of all matched
           symbols we've found so far.  */
	if (tmp_list)
	  {
	    block_found = block;
	    for (prev = NULL, current = tmp_list; current; 
		 prev = current, current = current->next)
	      {
		current->symtab = s;
		fixup_symbol_section (current->symbol, objfile);
	      }
	    gdb_assert (prev != NULL);
	    gdb_assert (current == NULL);
	    prev->next = *sym_list;
	    *sym_list = tmp_list;
	  }
      } /* if find_all_occurrences  */
    else
      {
	sym = lookup_block_symbol (block, name, linkage_name, domain);
	/* APPLE LOCAL fix-and-continue */
	if (sym && !SYMBOL_OBSOLETED (sym))
	  {
	    block_found = block;
	    if (symtab != NULL)
	      *symtab = s;
	    return fixup_symbol_section (sym, objfile);
	  }
      }
  }

  if (!find_all_occurrences
      || *sym_list == NULL)
    {
      if (symtab)
        *symtab = NULL;
      return NULL;
    }
  else
    {
      if (symtab)
        *symtab = (*sym_list)->symtab;
      return (*sym_list)->symbol;
    }
}

/* APPLE LOCAL begin psym equivalences  */
/* Given two names, return 1 if the are identical or if ALTERNATE_NAME
   is a psym equivalence name for NAME.  Return 0 otherwise.  

   A psym equivalence name begins with '*_', contains at least one '$',
   and everything after the '$' must be uppercase, a digit or anther '$'.
   For ALTERNATE_NAME to be an equivalence name for NAME, everything
   between '*_' and the '$' must be identical to NAME, e.g.
   '*_putenv$UNIX2003' and 'putenv'.  

   Returns 1 if ALTERNATE_NAME is an equivalence name for NAME, or
   if they are string-compare equal.  */

int
psym_name_match (const char *alternate_name, const char *name)
{
  int len1, len2;

  if (strcmp (alternate_name, name) == 0)
    return 1;

  /* Make sure the alternate_name is at least 3 chars longer than name  */
  len1 = strlen (alternate_name);
  len2 = strlen (name);
  if (len1 < len2 + 3)
    return 0;

  /* Equivalence symbols start with *_ */
  if (alternate_name[0] !='*' || alternate_name[1] != '_')
    return 0;
  alternate_name += 2;

  /* Following the *_, a copy of the symbol name */
  if (strncmp (alternate_name, name, len2) != 0)
    return 0;

  /* Following the *_, the symbol name, expect a '$' */
  if (alternate_name[len2] != '$')
    return 0;
  alternate_name += len2;

  while (*alternate_name != '\0')
    {
      if (!isupper (*alternate_name) 
          &&!isdigit (*alternate_name)
          && *alternate_name != '$')
        return 0;
      alternate_name++;
    }

  return 1;
}

/* Given partial symbol table, PST, and a function NAME, chec, to see if
   PST contains a psym equivalence name for NAME.  Return 1 it so, 0 
   otherwise.  */

static int
lookup_equiv_partial_symbol (struct partial_symtab *pst, char *name)
{
  int has_equivalent_symbol = 0;
  int i;

  if (psym_equivalences
      && pst->equiv_psyms)
    {
      for (i =0; i < pst->equiv_psyms->num_syms && !has_equivalent_symbol;
	   i++)
	if (psym_name_match (pst->equiv_psyms->sym_list[i], name))
	  has_equivalent_symbol = 1;
    }

  return has_equivalent_symbol;
}
/* APPLE LOCAL end psym equivalences  */

/* Check to see if the symbol is defined in one of the partial
   symtabs.  BLOCK_INDEX should be either GLOBAL_BLOCK or
   STATIC_BLOCK, depending on whether or not we want to search global
   symbols or static symbols. 

   APPLE LOCAL: This function can be used in two different ways:  
   It can be used to find a single symbol by name -- in which case
   a pointer to the symbol is returned and *SYMTAB is set to the
   symtab, if we found one.
              
   The second way is to return a comprehensive list of all symbols 
   found in all symtabs.  In that case, *SYM_LIST is set to a linked
   list of matching symbols which have been xmalloc()'ed, a pointer
   to one of the symbols is returned and *SYMTAB is set to one of
   the matched symbols' symtabs.  It is the responsibility of the
   caller to free *SYM_LIST.  */

static struct symbol *
lookup_symbol_aux_psymtabs (int block_index, const char *name,
			    const char *linkage_name,
			    const domain_enum domain,
			    struct symtab **symtab,
			    /* APPLE LOCAL begin return multiple symbols  */
			    struct symbol_search **sym_list,
			    int find_all_occurrences)
                             /* APPLE LOCAL end return multiple symbols  */
{
  struct symbol *sym;
  struct objfile *objfile;
  struct blockvector *bv;
  const struct block *block;
  struct partial_symtab *ps;
  struct symtab *s;
  const int psymtab_index = (block_index == GLOBAL_BLOCK ? 1 : 0);
  /* APPLE LOCAL begin return multiple symbols  */
  struct symbol_search *tmp_list;
  struct symbol_search *prev;
  struct symbol_search *current;
  /* APPLE LOCAL end return multiple symbols  */

  /* If we're called with a null string for some bizarre reason, just bail.  */
  if (name == NULL || name[0] == '\0'
      || (linkage_name && linkage_name[0] == '\0'))
    {
      if (sym_list)
        *sym_list = NULL;
      if (symtab)
        *symtab = NULL;
      return NULL;
    }

  ALL_PSYMTABS (objfile, ps)
  {
    /* Check to see if there is either a direct match, or a
       psym equivalence match.  */
    if (!ps->readin
	&& (lookup_partial_symbol (ps, name, linkage_name,
				   psymtab_index, domain)
	    || lookup_equiv_partial_symbol (ps, name)))
      {
        if (info_verbose)
          {
            if (name)
              printf_filtered ("Looking for '%s': ", name);
            else if (linkage_name)
              printf_filtered ("Looking for '%s': ", linkage_name);
          }
	s = PSYMTAB_TO_SYMTAB (ps);
        /* APPLE LOCAL: Catch a null symtab and give the user a reportable
           warning.  Don't throw an error here, though, or we won't then
	   look in the minimal symbols.  */
        if (s == NULL)
	  {
	    warning ("Error expanding psymtab %s to symtab in "
		     "lookup_symbol_aux_psymtabs()", ps->filename);
	    return NULL;
	  }

        if (s->primary == 0)
          continue;
	tmp_list = NULL;
	bv = BLOCKVECTOR (s);
	block = BLOCKVECTOR_BLOCK (bv, block_index);
	if (find_all_occurrences)
	  {
	    tmp_list = lookup_block_symbol_all (block, name, linkage_name, 
                                                domain);
	    if (!tmp_list || SYMBOL_OBSOLETED (tmp_list->symbol))
	      {
		block = BLOCKVECTOR_BLOCK (bv,
					   block_index == GLOBAL_BLOCK ?
					   STATIC_BLOCK : GLOBAL_BLOCK);
		tmp_list = lookup_block_symbol_all (block, name, linkage_name, 
                                                    domain);
		if (!tmp_list || SYMBOL_OBSOLETED (tmp_list->symbol))
		  {
		complaint (&symfile_complaints, 
              "Internal: %s symbol `%s' found in %s psymtab but not in symtab."
              "\n%s may be an inlined function, or may be a template function\n"
              "(if a template, try specifying an instantiation: %s<type>).",
			   block_index == GLOBAL_BLOCK ? "global" : "static",
			   name, ps->filename, name, name);
		return NULL;
		  }
	      }
	    for (prev = NULL, current = tmp_list; current; 
		 prev = current, current = current->next)
	      {
		current->symtab = s;
		fixup_symbol_section (current->symbol, objfile);
	      }
	    if (tmp_list)
	      {
		gdb_assert (prev != NULL);
		gdb_assert (current == NULL);
		prev->next = *sym_list;
		*sym_list = tmp_list;
	      }
	  }
	else
	  {
	    sym = lookup_block_symbol (block, name, linkage_name, domain);
	    /* APPLE LOCAL fix-and-continue */
	    if (!sym || SYMBOL_OBSOLETED (sym))
	      {
		/* This shouldn't be necessary, but as a last resort try
		   looking in the statics even though the psymtab claimed
		   the symbol was global, or vice-versa. It's possible
		   that the psymtab gets it wrong in some cases.  */
		
		/* FIXME: carlton/2002-09-30: Should we really do that?
		   If that happens, isn't it likely to be a GDB error, in
		   which case we should fix the GDB error rather than
		   silently dealing with it here?  So I'd vote for
		   removing the check for the symbol in the other
		   block.  */
		block = BLOCKVECTOR_BLOCK (bv,
					   block_index == GLOBAL_BLOCK ?
					   STATIC_BLOCK : GLOBAL_BLOCK);
		sym = lookup_block_symbol (block, name, linkage_name, domain);
		/* APPLE LOCAL fix-and-continue */
		if (!sym || SYMBOL_OBSOLETED (sym))
		  {
		    complaint (&symfile_complaints, 
            "Internal: %s symbol `%s' found in %s psymtab but not in symtab."
            "\n%s may be an inlined function, or may be a template function\n"
            "(if a template, try specifying an instantiation: %s<type>).",
			      block_index == GLOBAL_BLOCK ? "global" : "static",
			      name, ps->filename, name, name);
		    /* APPLE LOCAL: If this symtab got it wrong, continue 
                       looking for one that doesn't.  We used to return NULL 
                       here.  */
		    continue;
		  }
	      }
	    if (symtab != NULL)
	      *symtab = s;
	    return fixup_symbol_section (sym, objfile);
	  }
      }
  }

  if (!find_all_occurrences
      || *sym_list == NULL)
    {
      if (symtab)
        *symtab = NULL;
      return NULL;
    }
  else
    {
      if (symtab)
        *symtab = (*sym_list)->symtab;
      return (*sym_list)->symbol;
    }
}

#if 0
/* Check for the possibility of the symbol being a function or a
   mangled variable that is stored in one of the minimal symbol
   tables.  Eventually, all global symbols might be resolved in this
   way.  */

/* NOTE: carlton/2002-12-05: At one point, this function was part of
   lookup_symbol_aux, and what are now 'return' statements within
   lookup_symbol_aux_minsyms returned from lookup_symbol_aux, even if
   sym was NULL.  As far as I can tell, this was basically accidental;
   it didn't happen every time that msymbol was non-NULL, but only if
   some additional conditions held as well, and it caused problems
   with HP-generated symbol tables.  */

/* NOTE: carlton/2003-05-14: This function was once used as part of
   lookup_symbol.  It is currently unnecessary for correctness
   reasons, however, and using it doesn't seem to be any faster than
   using lookup_symbol_aux_psymtabs, so I'm commenting it out.  */

static struct symbol *
lookup_symbol_aux_minsyms (const char *name,
			   const char *linkage_name,
			   const domain_enum domain,
			   int *is_a_field_of_this,
			   struct symtab **symtab)
{
  struct symbol *sym;
  struct blockvector *bv;
  const struct block *block;
  struct minimal_symbol *msymbol;
  struct symtab *s;

  if (domain == VAR_DOMAIN)
    {
      msymbol = lookup_minimal_symbol (name, NULL, NULL);

      /* APPLE LOCAL fix-and-continue */
      if (msymbol != NULL && !MSYMBOL_OBSOLETED(msymbol))
	{
	  /* OK, we found a minimal symbol in spite of not finding any
	     symbol. There are various possible explanations for
	     this. One possibility is the symbol exists in code not
	     compiled -g. Another possibility is that the 'psymtab'
	     isn't doing its job.  A third possibility, related to #2,
	     is that we were confused by name-mangling. For instance,
	     maybe the psymtab isn't doing its job because it only
	     know about demangled names, but we were given a mangled
	     name...  */

	  /* We first use the address in the msymbol to try to locate
	     the appropriate symtab. Note that find_pc_sect_symtab()
	     has a side-effect of doing psymtab-to-symtab expansion,
	     for the found symtab.  */
	  s = find_pc_sect_symtab (SYMBOL_VALUE_ADDRESS (msymbol),
				   SYMBOL_BFD_SECTION (msymbol));
	  if (s != NULL)
	    {
	      /* This is a function which has a symtab for its address.  */
	      bv = BLOCKVECTOR (s);
	      block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);

	      /* This call used to pass `SYMBOL_LINKAGE_NAME (msymbol)' as the
	         `name' argument to lookup_block_symbol.  But the name
	         of a minimal symbol is always mangled, so that seems
	         to be clearly the wrong thing to pass as the
	         unmangled name.  */
	      sym =
		lookup_block_symbol (block, name, linkage_name, domain);
	      /* We kept static functions in minimal symbol table as well as
	         in static scope. We want to find them in the symbol table. */
              /* APPLE LOCAL fix-and-continue */
	      if (!sym || SYMBOL_OBSOLETED (sym))
		{
		  block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
		  sym = lookup_block_symbol (block, name,
					     linkage_name, domain);
		}

	      /* NOTE: carlton/2002-12-04: The following comment was
		 taken from a time when two versions of this function
		 were part of the body of lookup_symbol_aux: this
		 comment was taken from the version of the function
		 that was #ifdef HPUXHPPA, and the comment was right
		 before the 'return NULL' part of lookup_symbol_aux.
		 (Hence the "Fall through and return 0" comment.)
		 Elena did some digging into the situation for
		 Fortran, and she reports:

		 "I asked around (thanks to Jeff Knaggs), and I think
		 the story for Fortran goes like this:

		 "Apparently, in older Fortrans, '_' was not part of
		 the user namespace.  g77 attached a final '_' to
		 procedure names as the exported symbols for linkage
		 (foo_) , but the symbols went in the debug info just
		 like 'foo'. The rationale behind this is not
		 completely clear, and maybe it was done to other
		 symbols as well, not just procedures."  */

	      /* If we get here with sym == 0, the symbol was 
	         found in the minimal symbol table
	         but not in the symtab.
	         Fall through and return 0 to use the msymbol 
	         definition of "foo_".
	         (Note that outer code generally follows up a call
	         to this routine with a call to lookup_minimal_symbol(),
	         so a 0 return means we'll just flow into that other routine).

	         This happens for Fortran  "foo_" symbols,
	         which are "foo" in the symtab.

	         This can also happen if "asm" is used to make a
	         regular symbol but not a debugging symbol, e.g.
	         asm(".globl _main");
	         asm("_main:");
	       */

	      if (symtab != NULL && sym != NULL)
		*symtab = s;
	      return fixup_symbol_section (sym, s->objfile);
	    }
	}
    }

  return NULL;
}
#endif /* 0 */

/* A default version of lookup_symbol_nonlocal for use by languages
   that can't think of anything better to do.  This implements the C
   lookup rules.  */

struct symbol *
basic_lookup_symbol_nonlocal (const char *name,
			      const char *linkage_name,
			      const struct block *block,
			      const domain_enum domain,
			      struct symtab **symtab)
{
  struct symbol *sym;

  /* NOTE: carlton/2003-05-19: The comments below were written when
     this (or what turned into this) was part of lookup_symbol_aux;
     I'm much less worried about these questions now, since these
     decisions have turned out well, but I leave these comments here
     for posterity.  */

  /* NOTE: carlton/2002-12-05: There is a question as to whether or
     not it would be appropriate to search the current global block
     here as well.  (That's what this code used to do before the
     is_a_field_of_this check was moved up.)  On the one hand, it's
     redundant with the lookup_symbol_aux_symtabs search that happens
     next.  On the other hand, if decode_line_1 is passed an argument
     like filename:var, then the user presumably wants 'var' to be
     searched for in filename.  On the third hand, there shouldn't be
     multiple global variables all of which are named 'var', and it's
     not like decode_line_1 has ever restricted its search to only
     global variables in a single filename.  All in all, only
     searching the static block here seems best: it's correct and it's
     cleanest.  */

  /* NOTE: carlton/2002-12-05: There's also a possible performance
     issue here: if you usually search for global symbols in the
     current file, then it would be slightly better to search the
     current global block before searching all the symtabs.  But there
     are other factors that have a much greater effect on performance
     than that one, so I don't think we should worry about that for
     now.  */

  sym = lookup_symbol_static (name, linkage_name, block, domain, symtab);
  if (sym != NULL)
    return sym;

  return lookup_symbol_global (name, linkage_name, domain, symtab);
}

/* Lookup a symbol in the static block associated to BLOCK, if there
   is one; do nothing if BLOCK is NULL or a global block.  */

struct symbol *
lookup_symbol_static (const char *name,
		      const char *linkage_name,
		      const struct block *block,
		      const domain_enum domain,
		      struct symtab **symtab)
{
  const struct block *static_block = block_static_block (block);

  if (static_block != NULL)
    return lookup_symbol_aux_block (name, linkage_name, static_block,
				    domain, symtab);
  else
    return NULL;
}

/* Lookup a symbol in all files' global blocks (searching psymtabs if
   necessary).  */

struct symbol *
lookup_symbol_global (const char *name,
		      const char *linkage_name,
		      const domain_enum domain,
		      struct symtab **symtab)
{
  struct symbol *sym;

  /* APPLE LOCAL begin return multiple symbols  */
  sym = lookup_symbol_aux_symtabs (GLOBAL_BLOCK, name, linkage_name,
				   domain, symtab, NULL, 0);
  /* APPLE LOCAL end return multiple symbols  */

  if (sym != NULL)
    return sym;

  /* APPLE LOCAL begin return multiple symbols  */
  return lookup_symbol_aux_psymtabs (GLOBAL_BLOCK, name, linkage_name,
				     domain, symtab, NULL, 0);
  /* APPLE LOCAL end return multiple symbols  */
}

/* Look, in partial_symtab PST, for symbol whose natural name is NAME.
   If LINKAGE_NAME is non-NULL, check in addition that the symbol's
   linkage name matches it.  Check the global symbols if GLOBAL, the
   static symbols if not */

struct partial_symbol *
lookup_partial_symbol (struct partial_symtab *pst, const char *name,
		       const char *linkage_name, int global,
		       domain_enum domain)
{
  struct partial_symbol **start, **psym;
  struct partial_symbol **top, **real_top, **bottom, **center;
  int length = (global ? pst->n_global_syms : pst->n_static_syms);
  int do_linear_search = 1;
  
  if (length == 0)
    {
      return (NULL);
    }
  start = (global ?
	   pst->objfile->global_psymbols.list + pst->globals_offset :
	   pst->objfile->static_psymbols.list + pst->statics_offset);
  
  if (global)			/* This means we can use a binary search. */
    {
      do_linear_search = 0;

      /* Binary search.  This search is guaranteed to end with center
         pointing at the earliest partial symbol whose name might be
         correct.  At that point *all* partial symbols with an
         appropriate name will be checked against the correct
         domain.  */

      bottom = start;
      top = start + length - 1;
      real_top = top;
      while (top > bottom)
	{
	  center = bottom + (top - bottom) / 2;
	  if (!(center < top))
	    internal_error (__FILE__, __LINE__, _("failed internal consistency check"));
	  if (!do_linear_search
	      && (SYMBOL_LANGUAGE (*center) == language_java))
	    {
	      do_linear_search = 1;
	    }
	  if (strcmp_iw_ordered (SYMBOL_SEARCH_NAME (*center), name) >= 0)
	    {
	      top = center;
	    }
	  else
	    {
	      bottom = center + 1;
	    }
	}
      if (!(top == bottom))
	internal_error (__FILE__, __LINE__, _("failed internal consistency check"));

      /* APPLE LOCAL: The algorithm in the FSF version is just wrong.  */
      while (top <= real_top)
        {
          if (!SYMBOL_MATCHES_NATURAL_NAME (*top,name))
            break; 

          if (!linkage_name ||
              strcmp (SYMBOL_LINKAGE_NAME (*top), linkage_name) == 0)
            {
              if (SYMBOL_DOMAIN (*top) == domain)
                {
                  return (*top);
                }
            }
          top++;
        }
      /* END APPLE LOCAL */
    }

  /* Can't use a binary search or else we found during the binary search that
     we should also do a linear search. */

  if (do_linear_search)
    {			
      for (psym = start; psym < start + length; psym++)
	{
	  if (domain == SYMBOL_DOMAIN (*psym))
	    {
	      if (linkage_name != NULL
		  ? strcmp (SYMBOL_LINKAGE_NAME (*psym), linkage_name) == 0
		  : SYMBOL_MATCHES_SEARCH_NAME (*psym, name))
		{
		  return (*psym);
		}
	    }
	}
    }

  return (NULL);
}

/* Look up a type named NAME in the struct_domain.  The type returned
   must not be opaque -- i.e., must have at least one field
   defined.  */

struct type *
lookup_transparent_type (const char *name)
{
  return current_language->la_lookup_transparent_type (name);
}

/* The standard implementation of lookup_transparent_type.  This code
   was modeled on lookup_symbol -- the parts not relevant to looking
   up types were just left out.  In particular it's assumed here that
   types are available in struct_domain and only at file-static or
   global blocks.  */

struct type *
basic_lookup_transparent_type (const char *name)
{
  struct symbol *sym;
  struct symtab *s = NULL;
  struct partial_symtab *ps;
  struct blockvector *bv;
  struct objfile *objfile;
  struct block *block;

  /* Now search all the global symbols.  Do the symtab's first, then
     check the psymtab's. If a psymtab indicates the existence
     of the desired name as a global, then do psymtab-to-symtab
     conversion on the fly and return the found symbol.  */

  /* APPLE LOCAL fix-and-continue */
  ALL_SYMTABS_INCL_OBSOLETED (objfile, s)
  {
    bv = BLOCKVECTOR (s);
    block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
    sym = lookup_block_symbol (block, name, NULL, STRUCT_DOMAIN);
    /* APPLE LOCAL fix-and-continue */
    if (sym && !TYPE_IS_OPAQUE (SYMBOL_TYPE (sym)) && !SYMBOL_OBSOLETED (sym))
      {
	return SYMBOL_TYPE (sym);
      }
  }

  ALL_PSYMTABS (objfile, ps)
  {
    if (!ps->readin && lookup_partial_symbol (ps, name, NULL,
					      1, STRUCT_DOMAIN))
      {
        if (info_verbose)
          printf_filtered ("Looking for type '%s': ", name);
	s = PSYMTAB_TO_SYMTAB (ps);
	bv = BLOCKVECTOR (s);
	block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
	sym = lookup_block_symbol (block, name, NULL, STRUCT_DOMAIN);
        /* APPLE LOCAL fix-and-continue */
	if (!sym || SYMBOL_OBSOLETED (sym))
	  {
	    /* This shouldn't be necessary, but as a last resort
	     * try looking in the statics even though the psymtab
	     * claimed the symbol was global. It's possible that
	     * the psymtab gets it wrong in some cases.
	     */
	    block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
	    sym = lookup_block_symbol (block, name, NULL, STRUCT_DOMAIN);
            /* APPLE LOCAL fix-and-continue */
	    if (!sym || SYMBOL_OBSOLETED (sym))
	      {
		complaint (&symfile_complaints, 
			   "Internal: global symbol `%s' found in %s psymtab but not in symtab.\n\
%s may be an inlined function, or may be a template function\n\
(if a template, try specifying an instantiation: %s<type>).",
		           name, ps->filename, name, name);
		continue;
	      }
	  }
	if (!TYPE_IS_OPAQUE (SYMBOL_TYPE (sym)))
	  return SYMBOL_TYPE (sym);
      }
  }

  /* Now search the static file-level symbols.
     Not strictly correct, but more useful than an error.
     Do the symtab's first, then
     check the psymtab's. If a psymtab indicates the existence
     of the desired name as a file-level static, then do psymtab-to-symtab
     conversion on the fly and return the found symbol.
   */

  /* APPLE LOCAL fix-and-continue */
  ALL_SYMTABS_INCL_OBSOLETED (objfile, s)
  {
    bv = BLOCKVECTOR (s);
    block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
    sym = lookup_block_symbol (block, name, NULL, STRUCT_DOMAIN);
    /* APPLE LOCAL fix-and-continue */
    if (sym && !TYPE_IS_OPAQUE (SYMBOL_TYPE (sym)) && !SYMBOL_OBSOLETED (sym))
      {
	return SYMBOL_TYPE (sym);
      }
  }

  ALL_PSYMTABS (objfile, ps)
  {
    if (!ps->readin && lookup_partial_symbol (ps, name, NULL, 0, STRUCT_DOMAIN))
      {
        if (info_verbose)
          printf_filtered ("Looking for type '%s': ", name);
	s = PSYMTAB_TO_SYMTAB (ps);
	bv = BLOCKVECTOR (s);
	block = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
	sym = lookup_block_symbol (block, name, NULL, STRUCT_DOMAIN);
        /* APPLE LOCAL fix-and-continue */
	if (!sym || SYMBOL_OBSOLETED (sym))
	  {
	    /* This shouldn't be necessary, but as a last resort
	     * try looking in the globals even though the psymtab
	     * claimed the symbol was static. It's possible that
	     * the psymtab gets it wrong in some cases.
	     */
	    block = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);
	    sym = lookup_block_symbol (block, name, NULL, STRUCT_DOMAIN);
            /* APPLE LOCAL fix-and-continue */
	    if (!sym || SYMBOL_OBSOLETED (sym))
	      {
		complaint (&symfile_complaints, "Internal: static symbol `%s' found in %s psymtab but not in symtab.\n\
%s may be an inlined function, or may be a template function\n\
(if a template, try specifying an instantiation: %s<type>).",
		     name, ps->filename, name, name);
		continue;
	      }
	  }
	if (!TYPE_IS_OPAQUE (SYMBOL_TYPE (sym)))
	  return SYMBOL_TYPE (sym);
      }
  }
  return (struct type *) 0;
}


/* Find the psymtab containing main(). */
/* FIXME:  What about languages without main() or specially linked
   executables that have no main() ? */

struct partial_symtab *
find_main_psymtab (void)
{
  struct partial_symtab *pst;
  struct objfile *objfile;

  ALL_PSYMTABS (objfile, pst)
  {
    if (lookup_partial_symbol (pst, main_name (), NULL, 1, VAR_DOMAIN))
      {
	return (pst);
      }
  }
  return (NULL);
}

/* APPLE LOCAL begin return multiple symbols  */

/* This function is very similar to lookup_block_symbol, except that instead
   of returning the first matching symbol it finds, it returns a list
   of ALL the matching symbols it could find.  Note, the result is
   returned in a struct symbol_search, but the block field is not set.  */

struct symbol_search *
lookup_block_symbol_all (const struct block *block, const char *name,
			 const char *linkage_name, const domain_enum domain)
{
  struct symbol_search *ret_list = NULL;
  struct symbol_search *new_node;
  struct dict_iterator iter;
  struct symbol *sym;

  if (!BLOCK_FUNCTION (block))
    {
      for (sym = dict_iter_name_first (BLOCK_DICT (block), name, &iter);
	   sym != NULL;
	   sym = dict_iter_name_next (name, &iter))
	{
	  /* APPLE LOCAL begin  psym equivalences  */
	  /* If the strings don't match directly, see if there is a
	     psym equivalence match.  */
	  if (SYMBOL_DOMAIN (sym) == domain)
	    if ((linkage_name != NULL
		 ? ((strcmp (SYMBOL_LINKAGE_NAME (sym), linkage_name) == 0)
		    || psym_name_match (SYMBOL_LINKAGE_NAME (sym), 
					linkage_name))
		 : 1))
	  /* APPLE LOCAL end psym equivalences  */
	    {
	      new_node = (struct symbol_search *) xmalloc (sizeof (struct symbol_search));
	      new_node->block = 0;
	      new_node->symbol = sym;
	      new_node->symtab = NULL;
	      new_node->msymbol = NULL;
	      new_node->next = ret_list;
	      ret_list = new_node;
	    }
	}
      if (!ret_list)
	return NULL;
    }
  else
    return NULL;

  return ret_list;
}
/* APPLE LOCAL end return multiple symbols  */

/* Search BLOCK for symbol NAME in DOMAIN.

   Note that if NAME is the demangled form of a C++ symbol, we will fail
   to find a match during the binary search of the non-encoded names, but
   for now we don't worry about the slight inefficiency of looking for
   a match we'll never find, since it will go pretty quick.  Once the
   binary search terminates, we drop through and do a straight linear
   search on the symbols.  Each symbol which is marked as being a ObjC/C++
   symbol (language_cplus or language_objc set) has both the encoded and 
   non-encoded names tested for a match.

   If LINKAGE_NAME is non-NULL, verify that any symbol we find has this
   particular mangled name.
*/

/* APPLE LOCAL fix-and-continue NOTE: We don't check whether the matched
   SYM is flagged as obsolete here -- if someone is looking for a symbol
   in a specific block, much like if they're looking for a symbol at a specific
   address, we assume they may really need to find the obsolete symbol.  */

struct symbol *
lookup_block_symbol (const struct block *block, const char *name,
		     const char *linkage_name,
		     const domain_enum domain)
{
  struct dict_iterator iter;
  struct symbol *sym;

  if (!BLOCK_FUNCTION (block))
    {
      for (sym = dict_iter_name_first (BLOCK_DICT (block), name, &iter);
	   sym != NULL;
	   sym = dict_iter_name_next (name, &iter))
	{
	  /* APPLE LOCAL begin psym equivalences  */
	  /* If the strings don't match directly, see if there is a
	     psym equivalence match.  */
	  if (SYMBOL_DOMAIN (sym) == domain)
	    if ((linkage_name != NULL
		 ? ((strcmp (SYMBOL_LINKAGE_NAME (sym), linkage_name) == 0)
		    || psym_name_match (SYMBOL_LINKAGE_NAME (sym), 
					linkage_name))
		 : 1))
	 /* APPLE LOCAL end psym equivalences  */
	    return sym;
	}
      return NULL;
    }
  else
    {
      /* Note that parameter symbols do not always show up last in the
	 list; this loop makes sure to take anything else other than
	 parameter symbols first; it only uses parameter symbols as a
	 last resort.  Note that this only takes up extra computation
	 time on a match.  */

      struct symbol *sym_found = NULL;

      for (sym = dict_iter_name_first (BLOCK_DICT (block), name, &iter);
	   sym != NULL;
	   sym = dict_iter_name_next (name, &iter))
	{
	  if (SYMBOL_DOMAIN (sym) == domain
	      && (linkage_name != NULL
		  ? strcmp (SYMBOL_LINKAGE_NAME (sym), linkage_name) == 0 : 1))
	    {
	      sym_found = sym;
	      if (SYMBOL_CLASS (sym) != LOC_ARG &&
		  SYMBOL_CLASS (sym) != LOC_LOCAL_ARG &&
		  SYMBOL_CLASS (sym) != LOC_REF_ARG &&
		  SYMBOL_CLASS (sym) != LOC_REGPARM &&
		  SYMBOL_CLASS (sym) != LOC_REGPARM_ADDR &&
		  SYMBOL_CLASS (sym) != LOC_BASEREG_ARG &&
		  SYMBOL_CLASS (sym) != LOC_COMPUTED_ARG)
		{
		  break;
		}
	    }
	}
      return (sym_found);	/* Will be NULL if not found. */
    }
}

/* APPLE LOCAL begin address ranges  */

static CORE_ADDR
block_size (struct block *bl)
{
  CORE_ADDR sum = 0;
  int i;
 
 if (!BLOCK_RANGES (bl))
   return (BLOCK_END (bl) - BLOCK_START (bl));

 for (i = 0; i < BLOCK_RANGES (bl)->nelts; i++)
   sum += BLOCK_RANGE_END (bl, i) - BLOCK_RANGE_START (bl, i);
 return sum;
}
/* APPLE LOCAL end address ranges  */

/* Find the symtab associated with PC and SECTION.  Look through the
   psymtabs and read in another symtab if necessary. */

struct symtab *
find_pc_sect_symtab (CORE_ADDR pc, asection *section)
{
  struct block *b;
  struct blockvector *bv;
  struct symtab *s = NULL;
  struct symtab *best_s = NULL;
  struct partial_symtab *ps;
  struct objfile *objfile;
  CORE_ADDR distance = 0;
  struct minimal_symbol *msymbol;

  /* APPLE LOCAL begin cache lookup values for improved performance  */
  if (pc == last_symtab_lookup_pc
      && pc == last_mapped_section_lookup_pc
      && cached_mapped_section == section
      && cached_symtab)
    return cached_symtab;

  last_symtab_lookup_pc = pc;
  /* APPLE LOCAL end cache lookup values for improved performance  */

  /* If we know that this is not a text address, return failure.  This is
     necessary because we loop based on the block's high and low code
     addresses, which do not include the data ranges, and because
     we call find_pc_sect_psymtab which has a similar restriction based
     on the partial_symtab's texthigh and textlow.  */
  msymbol = lookup_minimal_symbol_by_pc_section (pc, section);
  if (msymbol
      && (msymbol->type == mst_data
	  || msymbol->type == mst_bss
	  || msymbol->type == mst_abs
	  || msymbol->type == mst_file_data
	  || msymbol->type == mst_file_bss))
    /* APPLE LOCAL begin cache lookup values for improved performance  */
    {
      cached_symtab = NULL;
      return NULL;
    }
    /* APPLE LOCAL end cache lookup values for improved performance  */

  /* Search all symtabs for the one whose file contains our address, and which
     is the smallest of all the ones containing the address.  This is designed
     to deal with a case like symtab a is at 0x1000-0x2000 and 0x3000-0x4000
     and symtab b is at 0x2000-0x3000.  So the GLOBAL_BLOCK for a is from
     0x1000-0x4000, but for address 0x2345 we want to return symtab b.

     This happens for native ecoff format, where code from included files
     gets its own symtab. The symtab for the included file should have
     been read in already via the dependency mechanism.
     It might be swifter to create several symtabs with the same name
     like xcoff does (I'm not sure).

     It also happens for objfiles that have their functions reordered.
     For these, the symtab we are looking for is not necessarily read in.  */

  /* APPLE LOCAL fix-and-continue: Iterate over all object files so we can
     do pc-to-sym matching even in an obsoleted section of code. */

  ALL_SYMTABS_INCL_OBSOLETED (objfile, s)
  {
    /* APPLE LOCAL: We were passed in the section, so don't look in 
       objfiles that don't even share the bfd with that section...  */
    if (skip_non_matching_bfd (section, objfile))
      continue;

    bv = BLOCKVECTOR (s);
    b = BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK);

    /* APPLE LOCAL begin address ranges  */
    if (block_contains_pc (b, pc)
	&& (distance == 0
	    || block_size (b) < distance))
      /* APPLE LOCAL end address ranges */
      {
	/* For an objfile that has its functions reordered,
	   find_pc_psymtab will find the proper partial symbol table
	   and we simply return its corresponding symtab.  */
	/* In order to better support objfiles that contain both
	   stabs and coff debugging info, we continue on if a psymtab
	   can't be found. */
	if ((objfile->flags & OBJF_REORDERED) && objfile->psymtabs)
	  {
	    ps = find_pc_sect_psymtab (pc, section);
	    if (ps)
	      /* APPLE LOCAL begin cache lookup values for improved 
		 performance  */
	      {
		cached_symtab = PSYMTAB_TO_SYMTAB (ps);
		return PSYMTAB_TO_SYMTAB (ps);
	      }
	      /* APPLE LOCAL end cache lookup values for improved 
		 performance  */
	  }
	if (section != 0)
	  {
	    struct dict_iterator iter;
	    struct symbol *sym = NULL;

	    ALL_BLOCK_SYMBOLS (b, iter, sym)
	      {
		fixup_symbol_section (sym, objfile);
		if (section == SYMBOL_BFD_SECTION (sym))
		  break;
	      }
	    if (sym == NULL)
	      continue;		/* no symbol in this symtab matches section */
	  }
	/* APPLE LOCAL begin address ranges  */
	distance = block_size (b);
	/* APPLE LOCAL end address ranges  */
	best_s = s;
      }
  }

  if (best_s != NULL)
    /* APPLE LOCAL begin cache lookup values for improved performance  */
    {
      cached_symtab = best_s;
      return (best_s);
    }
    /* APPLE LOCAL end cache lookup values for improved performance  */

  s = NULL;
  ps = find_pc_sect_psymtab (pc, section);
  if (ps)
    {
      /* APPLE LOCAL: TODO: Decide what we should do about partial_symtabs 
	 that end up having no symtabs. We can run into this more often since 
	 we can have a valid debug map and be missing some .o files. When this
	 happens we will happily make a partial_symtab from the debug map,
	 and when we try and expand it to a symtab, we will emit a warning
	 stating the .o file was missing, but we will continue. This warning
	 can appear many times, so the current solutions is to suppress the
	 warning if we have a OSO name.  */
      if (ps->readin && PSYMTAB_OSO_NAME (ps) == NULL)
	/* Might want to error() here (in case symtab is corrupt and
	   will cause a core dump), but maybe we can successfully
	   continue, so let's not.  */
	   
	warning (_("\
(Internal error: pc 0x%s in read in psymtab, but not in symtab.)\n"),
		 paddr_nz (pc));
      s = PSYMTAB_TO_SYMTAB (ps);
    }
  /* APPLE LOCAL cache lookup values for improved performance  */
  cached_symtab = s;
  return (s);
}

/* Find the symtab associated with PC.  Look through the psymtabs and
   read in another symtab if necessary.  Backward compatibility, no section */

struct symtab *
find_pc_symtab (CORE_ADDR pc)
{
  return find_pc_sect_symtab (pc, find_pc_mapped_section (pc));
}

/* APPLE LOCAL begin cache lookup values for improved performance  */
struct symtab_and_line *
copy_sal (struct symtab_and_line *orig)
{
  struct symtab_and_line *copy;
  struct symtab_and_line *copy_eol;
  struct symtab_and_line *current;
  struct symtab_and_line *tmp;

  if (orig == NULL)
    return NULL;

  /* Copy the main sal entry.  */

  copy = (struct symtab_and_line *) xmalloc (sizeof (struct symtab_and_line));
  
  copy->symtab = orig->symtab;
  copy->section = orig->section;
  copy->line = orig->line;
  copy->pc = orig->pc;
  copy->end = orig->end;
  copy->entry_type = orig->entry_type;
  copy->next = NULL;

  /* copy_eol points to the current end of the copy's linked list.  */

  copy_eol = copy;

  /* If the sal has a linked list of sals, through its 'next' field,
     copy the linked list as well.  */

  for (current = orig->next; current; current = current->next)
    {
      /* Create the next copy to go into the linked list.  */

      tmp = (struct symtab_and_line *) xmalloc (sizeof 
						  (struct symtab_and_line));
      tmp->symtab = current->symtab;
      tmp->section = current->section;
      tmp->line = current->line;
      tmp->pc = current->pc;
      tmp->end = current->end;
      tmp->entry_type = current->entry_type;
      tmp->next = NULL;

      /* Make the 'next' field of the current end of the linked list
	 point to the new node.  */

      copy_eol->next = tmp;

      /* Move the end-of-list pointer to point to the new end of the list.  */
      copy_eol = tmp;
    }

  return copy;
}

/* APPLE LOCAL end cache lookup values for improved performance  */

/* Find the source file and line number for a given PC value and SECTION.
   Return a structure containing a symtab pointer, a line number,
   and a pc range for the entire source line.
   The value's .pc field is NOT the specified pc.
   NOTCURRENT nonzero means, if specified pc is on a line boundary,
   use the line that ends there.  Otherwise, in that case, the line
   that begins there is used.  */

/* The big complication here is that a line may start in one file, and end just
   before the start of another file.  This usually occurs when you #include
   code in the middle of a subroutine.  To properly find the end of a line's PC
   range, we must search all symtabs associated with this compilation unit, and
   find the one whose first PC is closer than that of the next line in this
   symtab.  */

/* If it's worth the effort, we could be using a binary search.  */
struct symtab_and_line
find_pc_sect_line (CORE_ADDR pc, struct bfd_section *section, int notcurrent)
{
  struct symtab *s;
  struct linetable *l;
  int len;
  int i;
  struct linetable_entry *item;
  struct symtab_and_line val;
  struct blockvector *bv;
  struct minimal_symbol *msymbol;
  struct minimal_symbol *mfunsym;
  /* APPLE LOCAL begin subroutine inlining  */
  struct symtab_and_line *temp_list = NULL;
  struct symtab_and_line *temp_val;
  int inlined_entries_found = 0;
  /* APPLE LOCAL end subroutine inlining  */

  /* Info on best line seen so far, and where it starts, and its file.  */

  struct linetable_entry *best = NULL;
  CORE_ADDR best_end = 0;
  struct symtab *best_symtab = 0;

  /* Store here the first line number
     of a file which contains the line at the smallest pc after PC.
     If we don't find a line whose range contains PC,
     we will use a line one less than this,
     with a range from the start of that file to the first line's pc.  */
  struct linetable_entry *alt = NULL;
  struct symtab *alt_symtab = 0;

  /* Info on best line seen in this file.  */

  struct linetable_entry *prev;

  /* APPLE LOCAL begin cache lookup values for improved performance  */
  if (pc == last_pc_line_lookup_pc
      && pc == last_overlay_section_lookup_pc
      && section == cached_overlay_section
      && cached_pc_line)
    return (*cached_pc_line);

  last_pc_line_lookup_pc = pc;
  /* APPLE LOCAL end cache lookup values for improved performance  */

  /* If this pc is not from the current frame,
     it is the address of the end of a call instruction.
     Quite likely that is the start of the following statement.
     But what we want is the statement containing the instruction.
     Fudge the pc to make sure we get that.  */

  init_sal (&val);		/* initialize to zeroes */

  /* It's tempting to assume that, if we can't find debugging info for
     any function enclosing PC, that we shouldn't search for line
     number info, either.  However, GAS can emit line number info for
     assembly files --- very helpful when debugging hand-written
     assembly code.  In such a case, we'd have no debug info for the
     function, but we would have line info.  */

  if (notcurrent)
    pc -= 1;

  /* elz: added this because this function returned the wrong
     information if the pc belongs to a stub (import/export)
     to call a shlib function. This stub would be anywhere between
     two functions in the target, and the line info was erroneously 
     taken to be the one of the line before the pc. 
   */
  /* RT: Further explanation:

   * We have stubs (trampolines) inserted between procedures.
   *
   * Example: "shr1" exists in a shared library, and a "shr1" stub also
   * exists in the main image.
   *
   * In the minimal symbol table, we have a bunch of symbols
   * sorted by start address. The stubs are marked as "trampoline",
   * the others appear as text. E.g.:
   *
   *  Minimal symbol table for main image 
   *     main:  code for main (text symbol)
   *     shr1: stub  (trampoline symbol)
   *     foo:   code for foo (text symbol)
   *     ...
   *  Minimal symbol table for "shr1" image:
   *     ...
   *     shr1: code for shr1 (text symbol)
   *     ...
   *
   * So the code below is trying to detect if we are in the stub
   * ("shr1" stub), and if so, find the real code ("shr1" trampoline),
   * and if found,  do the symbolization from the real-code address
   * rather than the stub address.
   *
   * Assumptions being made about the minimal symbol table:
   *   1. lookup_minimal_symbol_by_pc() will return a trampoline only
   *      if we're really in the trampoline. If we're beyond it (say
   *      we're in "foo" in the above example), it'll have a closer 
   *      symbol (the "foo" text symbol for example) and will not
   *      return the trampoline.
   *   2. lookup_minimal_symbol_text() will find a real text symbol
   *      corresponding to the trampoline, and whose address will
   *      be different than the trampoline address. I put in a sanity
   *      check for the address being the same, to avoid an
   *      infinite recursion.
   */
  msymbol = lookup_minimal_symbol_by_pc (pc);
  if (msymbol != NULL)
    if (MSYMBOL_TYPE (msymbol) == mst_solib_trampoline)
      {
	mfunsym = lookup_minimal_symbol_text (SYMBOL_LINKAGE_NAME (msymbol),
					      NULL);
	if (mfunsym == NULL)
	  /* I eliminated this warning since it is coming out
	   * in the following situation:
	   * gdb shmain // test program with shared libraries
	   * (gdb) break shr1  // function in shared lib
	   * Warning: In stub for ...
	   * In the above situation, the shared lib is not loaded yet, 
	   * so of course we can't find the real func/line info,
	   * but the "break" still works, and the warning is annoying.
	   * So I commented out the warning. RT */
	  /* warning ("In stub for %s; unable to find real function/line info", SYMBOL_LINKAGE_NAME (msymbol)) */ ;
	/* fall through */
	else if (SYMBOL_VALUE_ADDRESS (mfunsym) == SYMBOL_VALUE_ADDRESS (msymbol))
	  /* Avoid infinite recursion */
	  /* See above comment about why warning is commented out */
	  /* warning ("In stub for %s; unable to find real function/line info", SYMBOL_LINKAGE_NAME (msymbol)) */ ;
	/* fall through */
	else
	  /* APPLE LOCAL end cache lookup values for improved performance  */
	  {
	    struct symtab_and_line sal = find_pc_line 
                                            (SYMBOL_VALUE_ADDRESS (mfunsym), 0);
	    cached_pc_line = copy_sal (&sal);
	    return find_pc_line (SYMBOL_VALUE_ADDRESS (mfunsym), 0);
	  }
	  /* APPLE LOCAL end cache lookup values for improved performance  */
      }


  s = find_pc_sect_symtab (pc, section);
  if (!s)
    {
      /* if no symbol information, return previous pc */
      if (notcurrent)
	pc++;
      val.pc = pc;
      /* APPLE LOCAL convert from character position to
	 line number if necessary.  */
      convert_sal (&val);
      /* APPLE LOCAL cache lookup values for improved performance  */
      cached_pc_line = copy_sal (&val);
      return val;
    }

  bv = BLOCKVECTOR (s);

  /* Look at all the symtabs that share this blockvector.
     They all have the same apriori range, that we found was right;
     but they have different line tables.  */

  for (; s && BLOCKVECTOR (s) == bv; s = s->next)
    {
      /* Find the best line in this symtab.  */
      l = LINETABLE (s);
      if (!l)
	continue;
      len = l->nitems;
      if (len <= 0)
	{
	  /* I think len can be zero if the symtab lacks line numbers
	     (e.g. gcc -g1).  (Either that or the LINETABLE is NULL;
	     I'm not sure which, and maybe it depends on the symbol
	     reader).  */
	  continue;
	}

      prev = NULL;
      item = l->item;		/* Get first line info */

      /* Is this file's first line closer than the first lines of other files?
         If so, record this file, and its first line, as best alternate.  */
      /* APPLE LOCAL begin subroutine inlining  */
      if (item->pc > pc && item->entry_type == NORMAL_LT_ENTRY
	  && (!alt || item->pc < alt->pc))
      /* APPLE LOCAL end subroutine inlining  */
	{
	  alt = item;
	  alt_symtab = s;
	}

      for (i = 0; i < len; i++, item++)
	{
	  /* Leave prev pointing to the linetable entry for the last line
	     that started at or before PC.  */
	  if (item->pc > pc)
	    break;
	  /* APPLE LOCAL begin subroutine inlining  */
	  else if (prev && prev->pc == pc && item->pc == prev->pc)
	    break;
	  /* APPLE LOCAL end subroutine inlining  */

	  prev = item;
	}

      /* At this point, prev points at the line whose start addr is <= pc, and
         item points at the next line.  If we ran off the end of the linetable
         (pc >= start of the last line), then prev == item.  If pc < start of
         the first line, prev will not be set.  */

      /* Is this file's best line closer than the best in the other files?
         If so, record this file, and its best line, as best so far.  Don't
         save prev if it represents the end of a function (i.e. line number
         0) instead of a real line.  */

      /* APPLE LOCAL begin subroutine inlining  */
      /* We only want to use non-NORMAL line table entries, if they exactly
	 match the pc we are searching for.  Otherwise we want to use only
	 NORMAL entries.  If 'prev' is not NORMAL, and its pc does not match
	 the one we are searching for then we know that: 1). 'prev' has a pc
	 value less than PC; 2). 'item' has a pc value greater than PC;  3).
	 there is supposed to be a NORMAL entry that has the same pc value
	 as 'prev', and since it is not after prev, it must be before prev.
	 Therefore we will search backwards from prev for the first NORMAL
	 entry we find and assign prev to be that (leaving 'item' where it is,
	 namely the next highest pc in the line table).  */
      if (prev && prev->pc != pc && prev->entry_type != NORMAL_LT_ENTRY)
	{
	  struct linetable_entry *temp_prev;
	  temp_prev = prev;
	  while (temp_prev > l->item
		 && temp_prev->entry_type != NORMAL_LT_ENTRY)
	    temp_prev--;
	  prev = temp_prev;
	}

      if (prev && prev->line && (!best || prev->pc > best->pc))
	{
	  /* If we are changing the value of 'best', then any data
	     in temp_list and inlined_entries_found is stale and needs
	     to be removed/re-set.  Since temp_list is a linked list,
	     we need to traverse the list to free the elements.  */
	  if (temp_list && temp_list->pc != prev->pc)
	    {
	      struct symtab_and_line *p, *c;
	      p = temp_list;
	      while (p)
		{
		  c = p->next;
		  xfree (p);
		  p = c;
		}
	      temp_list = NULL;
	      inlined_entries_found = 0;
	    }
	  /* APPLE LOCAL end subroutine inlining  */
	  best = prev;
	  best_symtab = s;

	  /* Discard BEST_END if it's before the PC of the current BEST.  */
	  if (best_end <= best->pc)
	    best_end = 0;
	}

      /* If another line (denoted by ITEM) is in the linetable and its
         PC is after BEST's PC, but before the current BEST_END, then
	 use ITEM's PC as the new best_end.  */
      if (best && i < len && item->pc > best->pc
          && (best_end == 0 || best_end > item->pc))
	best_end = item->pc;


      /* APPLE LOCAL begin subroutine inlining */

      /* The contents of 'prev' need to go into temp_list... */

      if (prev 
	  && (prev == best || prev->pc == pc))
	/*  || (prev->pc == pc && prev->entry_type != NORMAL_LT_ENTRY))) */
	{
	  if (temp_list && temp_list->pc != prev->pc)
	    {
	      /* If temp_list->pc does not match prev->pc, then the
		 data in temp_list and inlined_entries_found is stale
		 and needs to be removed/re-set.  Since temp_list is a
		 linked list, we need to traverse the list to free the
		 elements.  */
	      struct symtab_and_line *p, *c;
	      p = temp_list;
	      while (p)
		{
		  c = p->next;
		  xfree (p);
		  p = c;
		}
	      temp_list = NULL;
	      inlined_entries_found = 0;
	    }

	  temp_val = (struct symtab_and_line *) xmalloc 
	                                      (sizeof (struct symtab_and_line));
	  temp_val->symtab = s;
	  temp_val->section = section;
	  temp_val->line = prev->line;
	  temp_val->pc = prev->pc;
	  temp_val->end = prev->end_pc;
	  temp_val->entry_type = prev->entry_type;
	  temp_val->next = temp_list;
	  if (temp_val->entry_type == INLINED_SUBROUTINE_LT_ENTRY
	      || temp_val->entry_type == INLINED_CALL_SITE_LT_ENTRY)
	    inlined_entries_found = 1;
	  
	  temp_list = temp_val;
	}

      /* Put every 'item' with a pc matching prev->pc into temp_list.
	 Those should be any/all inlined subroutine and call site
	 entries.  */

      while (prev && prev->pc == pc && item->pc == prev->pc)
	{
	  temp_val = (struct symtab_and_line *) xmalloc 
	                                      (sizeof (struct symtab_and_line));
	  temp_val->symtab = s;
	  temp_val->section = section;
	  temp_val->line = item->line;
	  temp_val->pc = item->pc;
	  temp_val->end = item->end_pc;
	  temp_val->entry_type = item->entry_type;
	  temp_val->next = temp_list;
	  if (temp_val->entry_type == INLINED_SUBROUTINE_LT_ENTRY
	      || temp_val->entry_type == INLINED_CALL_SITE_LT_ENTRY)
	    inlined_entries_found = 1;
	  
	  temp_list = temp_val;

	  prev = item;
	  item++;

	  if (prev && prev->line && prev->entry_type == NORMAL_LT_ENTRY
	      && (!best || prev->pc >= best->pc))
	    {
	      best = prev;
	      best_symtab = s;
	      
	      /* Discard BEST_END if it's before the PC of the current BEST.  */
	      if (best_end <= best->pc)
		best_end = 0;
	    }
	  
	}

      if (best && i < len && item->pc > best->pc
          && (best_end == 0 || best_end > item->pc))
	best_end = item->pc;

      /* APPLE LOCAL end subroutine inlining  */
    }

  val.next = NULL;
  if (!best_symtab)
    {
      if (!alt_symtab)
	{			/* If we didn't find any line # info, just
				   return zeros.  */
	  val.pc = pc;
	  val.entry_type = NORMAL_LT_ENTRY;
	}
      else
	{
	  val.symtab = alt_symtab;
	  val.line = alt->line - 1;

	  /* Don't return line 0, that means that we didn't find the line.  */
	  if (val.line == 0)
	    ++val.line;

	  val.pc = BLOCK_END (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK));
	  val.end = alt->pc;
	  val.entry_type = NORMAL_LT_ENTRY;
	}
    }
  else if (best->line == 0)
    {
      /* If our best fit is in a range of PC's for which no line
	 number info is available (line number is zero) then we didn't
	 find any valid line information. */
      val.pc = pc;
      val.entry_type = NORMAL_LT_ENTRY;
    }
  else
    {
      val.symtab = best_symtab;
      val.line = best->line;
      val.pc = best->pc;
      if (best_end && (!alt || best_end < alt->pc))
	val.end = best_end;
      else if (alt)
	val.end = alt->pc;
      else
	val.end = BLOCK_END (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK));
      val.entry_type = best->entry_type;
    }
  val.section = section;
  /* APPLE LOCAL convert from character position to
     line number if necessary.  */
  convert_sal (&val);

  /* APPLE LOCAL begin subroutine inlining  */

  if (!inlined_entries_found)
    {
      gdb_assert (val.entry_type == NORMAL_LT_ENTRY);
      /* APPLE LOCAL cache lookup values for improved performance  */
      cached_pc_line = copy_sal (&val);
      return val;
    }
  else    
    {
      struct symtab_and_line final_val;
      struct symtab_and_line *cur;
      struct symtab_and_line *p;
      struct symtab_and_line *outer_call_site = NULL;

      init_sal (&final_val);

      /* Find the outermost call_site.  */
      
      for (cur = temp_list; cur; cur = cur->next)
	{
	  if (cur->entry_type == INLINED_CALL_SITE_LT_ENTRY)
	    {
	      if (!outer_call_site)
		outer_call_site = cur;
	      else if (outer_call_site->symtab != best_symtab
		       && cur->symtab == best_symtab)
		outer_call_site = cur;
	      else if (outer_call_site->symtab == cur->symtab
		       && cur->end > outer_call_site->end)
	      outer_call_site = cur;
	    }
	}

      gdb_assert (outer_call_site != NULL);

      if (val.entry_type == NORMAL_LT_ENTRY)
	memcpy (&final_val, &val, sizeof (struct symtab_and_line));
      else
	{
	  int found = 0;
	  
	  /* Find the NORMAL_LT entry in temp_list and fill in
	     final_val with that.  Remove the entry from temp_list.
	     Also verify that there is at most one normal entry in the
	     list.  */

	  for (p = NULL, cur = temp_list; cur; cur = cur->next)
	    {
	      if (cur->entry_type == NORMAL_LT_ENTRY)
		{
		  /* If this is the NORMAL entry for the outermost call site, 
		     put it into final_val.  */
		  
		  if (!found && cur->symtab == outer_call_site->symtab)
		    {
		      final_val.symtab = cur->symtab;
		      final_val.section = cur->section;
		      final_val.line = cur->line;
		      final_val.pc = cur->pc;
		      final_val.end = cur->end;
		      final_val.entry_type = cur->entry_type;
		      final_val.next = NULL;
		    }

		  found = 1;

		  /* Remove cur from list */
		  if (!p)
		    temp_list = cur->next;
		  else
		    p->next = cur->next;
		  xfree (cur);
		}
	      else
		{
		  p = cur;
		}
	    }
	}

      /* Remove all other NORMAL entries from temp_list */

      for (p = NULL, cur = temp_list; cur; cur = cur->next)
	if (cur->entry_type == NORMAL_LT_ENTRY)
	  {
	    if (!p)
	      temp_list = cur->next;
	    else
	      p->next = cur->next;
            xfree (cur);
	  }
	else
	  {
	    p = cur;
	  }

      
      /* Now append the rest of temp_list to final_var.next.  */

      /* FIXME: temp_list must be walked & freed by the callers of this
         function.  Today, that isn't done so we're leaking these sals when
         looking at source lines with inlined code.  */

      final_val.next = temp_list;
     
      if (final_val.pc == outer_call_site->pc)
	{
	  final_val.line = outer_call_site->line;
	  final_val.symtab = outer_call_site->symtab;
	}
 
      if (final_val.symtab == NULL)
	warning ("Returning an unfilled final_val");
      /* APPLE LOCAL cache lookup values for improved performance  */
      cached_pc_line = copy_sal (&final_val);
     return final_val;
    }
  /* APPLE LOCAL end subroutine inlining  */
}

/* Backward compatibility (no section) */

struct symtab_and_line
find_pc_line (CORE_ADDR pc, int notcurrent)
{
  asection *section;

  section = find_pc_overlay (pc);
  if (pc_in_unmapped_range (pc, section))
    pc = overlay_mapped_address (pc, section);
  return find_pc_sect_line (pc, section, notcurrent);
}

/* Find line number LINE in any symtab whose name is the same as
   SYMTAB.

   If found, return the symtab that contains the linetable in which it was
   found, set *INDEX to the index in the linetable of the best entry
   found, and set *EXACT_MATCH nonzero if the value returned is an
   exact match.

   If not found, return NULL.  */

struct symtab *
find_line_symtab (struct symtab *symtab, int line, int *index, int *exact_match)
{
  int exact;

  /* BEST_INDEX and BEST_LINETABLE identify the smallest linenumber > LINE
     so far seen.  */

  int best_index;
  struct linetable *best_linetable;
  struct symtab *best_symtab;

  /* First try looking it up in the given symtab.  */
  best_linetable = LINETABLE (symtab);
  best_symtab = symtab;
  best_index = find_line_common (best_linetable, line, &exact);
  if (best_index < 0 || !exact)
    {
      /* Didn't find an exact match.  So we better keep looking for
         another symtab with the same name.  In the case of xcoff,
         multiple csects for one source file (produced by IBM's FORTRAN
         compiler) produce multiple symtabs (this is unavoidable
         assuming csects can be at arbitrary places in memory and that
         the GLOBAL_BLOCK of a symtab has a begin and end address).  */

      /* BEST is the smallest linenumber > LINE so far seen,
         or 0 if none has been seen so far.
         BEST_INDEX and BEST_LINETABLE identify the item for it.  */
      int best;

      struct objfile *objfile;
      struct symtab *s;

      if (best_index >= 0)
	best = best_linetable->item[best_index].line;
      else
	best = 0;

      ALL_SYMTABS (objfile, s)
      {
	struct linetable *l;
	int ind;

	if (strcmp (symtab->filename, s->filename) != 0)
	  continue;
	l = LINETABLE (s);
	ind = find_line_common (l, line, &exact);
	if (ind >= 0)
	  {
	    if (exact)
	      {
		best_index = ind;
		best_linetable = l;
		best_symtab = s;
		goto done;
	      }
	    if (best == 0 || l->item[ind].line < best)
	      {
		best = l->item[ind].line;
		best_index = ind;
		best_linetable = l;
		best_symtab = s;
	      }
	  }
      }
    }
done:
  if (best_index < 0)
    return NULL;

  if (index)
    *index = best_index;
  if (exact_match)
    *exact_match = exact;

  return best_symtab;
}

/* Set the PC value for a given source file and line number and return true.
   Returns zero for invalid line number (and sets the PC to 0).
   The source file is specified with a struct symtab.  */

int
find_line_pc (struct symtab *symtab, int line, CORE_ADDR *pc)
{
  /* APPLE LOCAL find_line_pc */
  CORE_ADDR end;
  struct symtab_and_line sal;

  *pc = 0;

  sal.symtab = symtab;
  sal.line = line;
  sal.pc = *pc;

  return find_line_pc_range (sal, pc, &end);
  /* APPLE LOCAL end */
}

/* Find the range of pc values in a line.
   Store the starting pc of the line into *STARTPTR
   and the ending pc (start of next line) into *ENDPTR.
   Returns 1 to indicate success.
   Returns 0 if could not find the specified line.  */

int
find_line_pc_range (struct symtab_and_line sal, CORE_ADDR *startptr,
		    CORE_ADDR *endptr)
{
  CORE_ADDR startaddr;
  struct symtab_and_line found_sal;
  int line;

  if (sal.symtab == NULL)
    return 0;
  if (sal.symtab->linetable == NULL)
    return 0;

  if ((sal.symtab->line_charpos == 0) && (sal.symtab->linetable->lines_are_chars))
    {
      int fd = open_source_file (sal.symtab);
      if (fd < 0)
	return 0;
      find_source_lines (sal.symtab, fd);
      close (fd);
    }

  if (sal.symtab->linetable->lines_are_chars)
    line = sal.symtab->line_charpos[sal.line - 1];
  else
    line = sal.line;

  if (sal.pc == 0)
    {
      struct symtab *symtab;
      struct linetable *l;
      int ind;

      symtab = find_line_symtab (sal.symtab, line, &ind, NULL);
      if (symtab == NULL)
	return 0;
      
      l = LINETABLE (symtab);
      /* APPLE LOCAL: We KNOW that this is found by file & line, so the
	 comment below is not relevant.  We already found the start &
	 end addresses, let's just record them and get out of here... */
      *startptr = l->item[ind].pc;
      if (ind == l->nitems - 1)
	/* This shouldn't happen, since there is always a function ending
	   marker entry in the linetable, but I am not sure we should error
	   out here... */
	*endptr = *startptr;
      else if (l->item[ind].line != line)
	/* This gets the case where there is no code at the given line */
	*endptr = *startptr;
      else
	*endptr = l->item[ind + 1].pc;
      return 1;
      /* END APPLE LOCAL */
      
    }
  else
    {
      startaddr = sal.pc;
    }

  /* This whole function is based on address.  For example, if line 10 has
     two parts, one from 0x100 to 0x200 and one from 0x300 to 0x400, then
     "info line *0x123" should say the line goes from 0x100 to 0x200
     and "info line *0x355" should say the line goes from 0x300 to 0x400.
     This also insures that we never give a range like "starts at 0x134
     and ends at 0x12c".  */

  found_sal = find_pc_sect_line (startaddr, sal.section, 0);
  if (found_sal.line != sal.line)
    {
      /* The specified line (sal) has zero bytes.  */
      *startptr = found_sal.pc;
      *endptr = found_sal.pc;
    }
  else
    {
      *startptr = found_sal.pc;
      *endptr = found_sal.end;
    }
  return 1;
}

/* Given a line table and a line number, return the index into the line
   table for the pc of the nearest line whose number is >= the specified one.
   Return -1 if none is found.  The value is >= 0 if it is an index.

   Set *EXACT_MATCH nonzero if the value returned is an exact match.  */

static int
find_line_common (struct linetable *l, int lineno,
		  int *exact_match)
{
  int i;
  int len;

  /* BEST is the smallest linenumber > LINENO so far seen,
     or 0 if none has been seen so far.
     BEST_INDEX identifies the item for it.  */

  int best_index = -1;
  int best = 0;

  if (lineno <= 0)
    return -1;
  if (l == 0)
    return -1;

  len = l->nitems;
  for (i = 0; i < len; i++)
    {
      struct linetable_entry *item = &(l->item[i]);

      if (item->line == lineno)
	{
	  /* Return the first (lowest address) entry which matches.  */
	  *exact_match = 1;
	  return i;
	}

      if (item->line > lineno && (best == 0 || item->line < best))
	{
	  best = item->line;
	  best_index = i;
	}
    }

  /* If we got here, we didn't get an exact match.  */

  *exact_match = 0;
  return best_index;
}

int
find_pc_line_pc_range (CORE_ADDR pc, CORE_ADDR *startptr, CORE_ADDR *endptr)
{
  struct symtab_and_line sal;
  sal = find_pc_line (pc, 0);
  *startptr = sal.pc;
  *endptr = sal.end;
  return sal.symtab != 0;
}

/* Given a function symbol SYM, find the symtab and line for the start
   of the function.
   If the argument FUNFIRSTLINE is nonzero, we want the first line
   of real code inside the function.  */

struct symtab_and_line
find_function_start_sal (struct symbol *sym, int funfirstline)
{
  /* APPLE LOCAL begin address context.  */
  struct address_context pc;
  init_address_context (&pc);
  pc.symbol = sym;
  /* APPLE LOCAL end address context.  */
  
  /* If the block structure is a little bit mangled, we can end up
     with function sym's with a NULL block.  Don't crash.  */
  pc.block = SYMBOL_BLOCK_VALUE (sym);
  if (pc.block == NULL)
    error ("Found function with NULL block: \"%s\"", SYMBOL_PRINT_NAME (sym));
  /* APPLE LOCAL begin address ranges  */
  pc.address = BLOCK_LOWEST_PC (pc.block);
  /* APPLE LOCAL end address ranges  */
  /* END APPLE LOCAL */
  fixup_symbol_section (sym, NULL);
  if (funfirstline)
    {				
      /* skip "first line" of function (which is actually its prologue) */
      pc.bfd_section = SYMBOL_BFD_SECTION (sym);
      /* If function is in an unmapped overlay, use its unmapped LMA
         address, so that SKIP_PROLOGUE has something unique to work on */
      if (section_is_overlay (pc.bfd_section) &&
	  !section_is_mapped (pc.bfd_section))
	pc.address = overlay_unmapped_address (pc.address, 
					       pc.bfd_section);

      pc.address += DEPRECATED_FUNCTION_START_OFFSET;
      /* APPLE LOCAL begin address context.  */
      /* Check if the current architecture supports the address context 
         version of prologue skipping.  */
      if (SKIP_PROLOGUE_ADDR_CTX_P())
	pc.address = SKIP_PROLOGUE_ADDR_CTX (&pc);
      else
	pc.address = SKIP_PROLOGUE (pc.address);
      /* APPLE LOCAL end address context.  */

      /* For overlays, map pc back into its mapped VMA range */
      pc.address = overlay_mapped_address (pc.address, 
					   pc.bfd_section);
    }
  pc.sal = find_pc_sect_line (pc.address, 
			      SYMBOL_BFD_SECTION (sym), 0);

  /* APPLE LOCAL begin inlined function symbols & blocks  */
  if (pc.sal.next
      && (BLOCK_FUNCTION (pc.block) == NULL))
    {
      /* This is an inlined subroutine;  Find the correct sal in
	 the set of sals that was returned, update pc.sal.  */
      struct symtab_and_line *cur;
      for (cur = &(pc.sal); cur; cur = cur->next)
	if (cur->entry_type == INLINED_SUBROUTINE_LT_ENTRY
	    && cur->pc == pc.block->startaddr
	    && cur->end == pc.block->endaddr)
	  {
	    pc.sal.symtab = cur->symtab;
	    pc.sal.section = cur->section;
	    pc.sal.line = cur->line;
	    pc.sal.pc = cur->pc;
	    pc.sal.end = cur->end;
	    pc.sal.entry_type = cur->entry_type;
	    break;
	  }
    }
  /* APPLE LOCAL end inlined function symbols & blocks  */
  /* Check if SKIP_PROLOGUE left us in mid-line, and the next
     line is still part of the same function.  */
  /* APPLE LOCAL begin address ranges  */
  else if (pc.sal.pc != pc.address
	   && block_contains_pc (SYMBOL_BLOCK_VALUE (pc.symbol), 
				 pc.sal.end))
    /* APPLE LOCAL end address ranges  */
    {
      /* First pc of next line */
      pc.address = pc.sal.end;
      /* Recalculate the line number (might not be N+1).  */
      pc.sal = find_pc_sect_line (pc.address, 
				  SYMBOL_BFD_SECTION (pc.symbol), 
				  0);
    }
  pc.sal.pc = pc.address;

  return pc.sal;
}
#if defined (USE_OLD_FIND_FUNCTION_START_SAL)
/* Given a function symbol SYM, find the symtab and line for the start
   of the function.
   If the argument FUNFIRSTLINE is nonzero, we want the first line
   of real code inside the function.  */

struct symtab_and_line
find_function_start_sal (struct symbol *sym, int funfirstline)
{
  CORE_ADDR pc;
  struct symtab_and_line sal;
  /* APPLE LOCAL */
  struct block *block;
  
  /* If the block structure is a little bit mangled, we can end up
     with function sym's with a NULL block.  Don't crash.  */
  block = SYMBOL_BLOCK_VALUE (sym);
  if (block == NULL)
    error ("Found function with NULL block: \"%s\"", SYMBOL_PRINT_NAME (sym));
  /* APPLE LOCAL begin address ranges  */
  pc = BLOCK_LOWEST_PC (block);
  /* APPLE LOCAL end address ranges  */
  /* END APPLE LOCAL */
  fixup_symbol_section (sym, NULL);
  if (funfirstline)
    {				/* skip "first line" of function (which is actually its prologue) */
      asection *section = SYMBOL_BFD_SECTION (sym);
      /* If function is in an unmapped overlay, use its unmapped LMA
         address, so that SKIP_PROLOGUE has something unique to work on */
      if (section_is_overlay (section) &&
	  !section_is_mapped (section))
	pc = overlay_unmapped_address (pc, section);

      pc += DEPRECATED_FUNCTION_START_OFFSET;
      pc = SKIP_PROLOGUE (pc);

      /* For overlays, map pc back into its mapped VMA range */
      pc = overlay_mapped_address (pc, section);
    }
  sal = find_pc_sect_line (pc, SYMBOL_BFD_SECTION (sym), 0);

  /* Check if SKIP_PROLOGUE left us in mid-line, and the next
     line is still part of the same function.  */
  /* APPLE LOCAL begin address ranges  */
  if (sal.pc != pc
      && block_contains_pc (SYMBOL_BLOCK_VALUE (sym), sal.end))
  /* APPLE LOCAL end address ranges  */
    {
      /* First pc of next line */
      pc = sal.end;
      /* Recalculate the line number (might not be N+1).  */
      sal = find_pc_sect_line (pc, SYMBOL_BFD_SECTION (sym), 0);
    }
  sal.pc = pc;

  return sal;
}
#endif
/* If P is of the form "operator[ \t]+..." where `...' is
   some legitimate operator text, return a pointer to the
   beginning of the substring of the operator text.
   Otherwise, return "".  */
char *
operator_chars (char *p, char **end)
{
  *end = "";
  if (strncmp (p, "operator", 8))
    return *end;
  p += 8;

  /* Don't get faked out by `operator' being part of a longer
     identifier.  */
  if (isalpha (*p) || *p == '_' || *p == '$' || *p == '\0')
    return *end;

  /* Allow some whitespace between `operator' and the operator symbol.  */
  while (*p == ' ' || *p == '\t')
    p++;

  /* Recognize 'operator TYPENAME'. */

  if (isalpha (*p) || *p == '_' || *p == '$')
    {
      char *q = p + 1;
      while (isalnum (*q) || *q == '_' || *q == '$')
	q++;
      *end = q;
      return p;
    }

  while (*p)
    switch (*p)
      {
      case '\\':			/* regexp quoting */
	if (p[1] == '*')
	  {
	    if (p[2] == '=')	/* 'operator\*=' */
	      *end = p + 3;
	    else			/* 'operator\*'  */
	      *end = p + 2;
	    return p;
	  }
	else if (p[1] == '[')
	  {
	    if (p[2] == ']')
	      error (_("mismatched quoting on brackets, try 'operator\\[\\]'"));
	    else if (p[2] == '\\' && p[3] == ']')
	      {
		*end = p + 4;	/* 'operator\[\]' */
		return p;
	      }
	    else
	      error (_("nothing is allowed between '[' and ']'"));
	  }
	else 
	  {
	    /* Gratuitous qoute: skip it and move on. */
	    p++;
	    continue;
	  }
	break;
      case '!':
      case '=':
      case '*':
      case '/':
      case '%':
      case '^':
	if (p[1] == '=')
	  *end = p + 2;
	else
	  *end = p + 1;
	return p;
      case '<':
      case '>':
      case '+':
      case '-':
      case '&':
      case '|':
	if (p[0] == '-' && p[1] == '>')
	  {
	    /* Struct pointer member operator 'operator->'. */
	    if (p[2] == '*')
	      {
		*end = p + 3;	/* 'operator->*' */
		return p;
	      }
	    else if (p[2] == '\\')
	      {
		*end = p + 4;	/* Hopefully 'operator->\*' */
		return p;
	      }
	    else
	      {
		*end = p + 2;	/* 'operator->' */
		return p;
	      }
	  }
	if (p[1] == '=' || p[1] == p[0])
	  *end = p + 2;
	else
	  *end = p + 1;
	return p;
      case '~':
      case ',':
	*end = p + 1;
	return p;
      case '(':
	if (p[1] != ')')
	  error (_("`operator ()' must be specified without whitespace in `()'"));
	*end = p + 2;
	return p;
      case '?':
	if (p[1] != ':')
	  error (_("`operator ?:' must be specified without whitespace in `?:'"));
	*end = p + 2;
	return p;
      case '[':
	if (p[1] != ']')
	  error (_("`operator []' must be specified without whitespace in `[]'"));
	*end = p + 2;
	return p;
      default:
	error (_("`operator %s' not supported"), p);
	break;
      }

  *end = "";
  return *end;
}


/* If FILE is not already in the table of files, return zero;
   otherwise return non-zero.  Optionally add FILE to the table if ADD
   is non-zero.  If *FIRST is non-zero, forget the old table
   contents.  */
static int
filename_seen (const char *file, int add, int *first)
{
  /* Table of files seen so far.  */
  static const char **tab = NULL;
  /* Allocated size of tab in elements.
     Start with one 256-byte block (when using GNU malloc.c).
     24 is the malloc overhead when range checking is in effect.  */
  static int tab_alloc_size = (256 - 24) / sizeof (char *);
  /* Current size of tab in elements.  */
  static int tab_cur_size;
  const char **p;

  if (*first)
    {
      if (tab == NULL)
	tab = (const char **) xmalloc (tab_alloc_size * sizeof (*tab));
      tab_cur_size = 0;
    }

  /* Is FILE in tab?  */
  for (p = tab; p < tab + tab_cur_size; p++)
    if (strcmp (*p, file) == 0)
      return 1;

  /* No; maybe add it to tab.  */
  if (add)
    {
      if (tab_cur_size == tab_alloc_size)
	{
	  tab_alloc_size *= 2;
	  tab = (const char **) xrealloc ((char *) tab,
					  tab_alloc_size * sizeof (*tab));
	}
      tab[tab_cur_size++] = file;
    }

  return 0;
}

/* Slave routine for sources_info.  Force line breaks at ,'s.
   NAME is the name to print and *FIRST is nonzero if this is the first
   name printed.  Set *FIRST to zero.  */
static void
output_source_filename (const char *name, int *first)
{
  /* Since a single source file can result in several partial symbol
     tables, we need to avoid printing it more than once.  Note: if
     some of the psymtabs are read in and some are not, it gets
     printed both under "Source files for which symbols have been
     read" and "Source files for which symbols will be read in on
     demand".  I consider this a reasonable way to deal with the
     situation.  I'm not sure whether this can also happen for
     symtabs; it doesn't hurt to check.  */

  /* Was NAME already seen?  */
  if (filename_seen (name, 1, first))
    {
      /* Yes; don't print it again.  */
      return;
    }
  /* No; print it and reset *FIRST.  */
  if (*first)
    {
      *first = 0;
    }
  else
    {
      printf_filtered (", ");
    }

  wrap_here ("");
  fputs_filtered (name, gdb_stdout);
}

static void
sources_info (char *ignore, int from_tty)
{
  struct symtab *s;
  struct partial_symtab *ps;
  struct objfile *objfile;
  int first;

  if (!have_full_symbols () && !have_partial_symbols ())
    {
      error (_("No symbol table is loaded.  Use the \"file\" command."));
    }

  printf_filtered ("Source files for which symbols have been read in:\n\n");

  first = 1;
  ALL_SYMTABS (objfile, s)
  {
    const char *fullname = symtab_to_fullname (s);
    output_source_filename (fullname ? fullname : s->filename, &first);
  }
  printf_filtered ("\n\n");

  printf_filtered ("Source files for which symbols will be read in on demand:\n\n");

  first = 1;
  ALL_PSYMTABS (objfile, ps)
  {
    if (!ps->readin)
      {
	const char *fullname = psymtab_to_fullname (ps);
	output_source_filename (fullname ? fullname : ps->filename, &first);
      }
  }
  printf_filtered ("\n");
}

static int
file_matches (char *file, char *files[], int nfiles)
{
  int i;

  if (file != NULL && nfiles != 0)
    {
      for (i = 0; i < nfiles; i++)
	{
	  if (strcmp (files[i], lbasename (file)) == 0)
	    return 1;
	}
    }
  else if (nfiles == 0)
    return 1;
  return 0;
}

/* Free any memory associated with a search. */
void
free_search_symbols (struct symbol_search *symbols)
{
  struct symbol_search *p;
  struct symbol_search *next;

  for (p = symbols; p != NULL; p = next)
    {
      next = p->next;
      xfree (p);
    }
}

static void
do_free_search_symbols_cleanup (void *symbols)
{
  free_search_symbols (symbols);
}

struct cleanup *
make_cleanup_free_search_symbols (struct symbol_search *symbols)
{
  return make_cleanup (do_free_search_symbols_cleanup, symbols);
}

/* Helper function for sort_search_symbols and qsort.  Can only
   sort symbols, not minimal symbols.  */
static int
compare_search_syms (const void *sa, const void *sb)
{
  struct symbol_search **sym_a = (struct symbol_search **) sa;
  struct symbol_search **sym_b = (struct symbol_search **) sb;

  return strcmp (SYMBOL_PRINT_NAME ((*sym_a)->symbol),
		 SYMBOL_PRINT_NAME ((*sym_b)->symbol));
}

/* Sort the ``nfound'' symbols in the list after prevtail.  Leave
   prevtail where it is, but update its next pointer to point to
   the first of the sorted symbols.  */
static struct symbol_search *
sort_search_symbols (struct symbol_search *prevtail, int nfound)
{
  struct symbol_search **symbols, *symp, *old_next;
  int i;

  symbols = (struct symbol_search **) xmalloc (sizeof (struct symbol_search *)
					       * nfound);
  symp = prevtail->next;
  for (i = 0; i < nfound; i++)
    {
      symbols[i] = symp;
      symp = symp->next;
    }
  /* Generally NULL.  */
  old_next = symp;

  qsort (symbols, nfound, sizeof (struct symbol_search *),
	 compare_search_syms);

  symp = prevtail;
  for (i = 0; i < nfound; i++)
    {
      symp->next = symbols[i];
      symp = symp->next;
    }
  symp->next = old_next;

  xfree (symbols);
  return symp;
}

/* Search the symbol table for matches to the regular expression REGEXP,
   returning the results in *MATCHES.

   Only symbols of KIND are searched:
   FUNCTIONS_DOMAIN - search all functions
   TYPES_DOMAIN     - search all type names
   METHODS_DOMAIN   - search all methods NOT IMPLEMENTED
   VARIABLES_DOMAIN - search all symbols, excluding functions, type names,
   and constants (enums)

   free_search_symbols should be called when *MATCHES is no longer needed.

   The results are sorted locally; each symtab's global and static blocks are
   separately alphabetized.
 */
void
search_symbols (char *regexp, domain_enum kind, int nfiles, char *files[],
		struct symbol_search **matches)
{
  struct symtab *s;
  struct partial_symtab *ps;
  struct blockvector *bv;
  struct blockvector *prev_bv = 0;
  struct block *b;
  int i = 0;
  struct dict_iterator iter;
  struct symbol *sym;
  struct partial_symbol **psym;
  struct objfile *objfile;
  struct minimal_symbol *msymbol;
  char *val;
  int found_misc = 0;
  static enum minimal_symbol_type types[]
  =
  {mst_data, mst_text, mst_abs, mst_unknown};
  static enum minimal_symbol_type types2[]
  =
  {mst_bss, mst_file_text, mst_abs, mst_unknown};
  static enum minimal_symbol_type types3[]
  =
  {mst_file_data, mst_solib_trampoline, mst_abs, mst_unknown};
  static enum minimal_symbol_type types4[]
  =
  {mst_file_bss, mst_text, mst_abs, mst_unknown};
  enum minimal_symbol_type ourtype;
  enum minimal_symbol_type ourtype2;
  enum minimal_symbol_type ourtype3;
  enum minimal_symbol_type ourtype4;
  struct symbol_search *sr;
  struct symbol_search *psr;
  struct symbol_search *tail;
  struct cleanup *old_chain = NULL;

  if (kind < VARIABLES_DOMAIN)
    error (_("must search on specific domain"));

  ourtype = types[(int) (kind - VARIABLES_DOMAIN)];
  ourtype2 = types2[(int) (kind - VARIABLES_DOMAIN)];
  ourtype3 = types3[(int) (kind - VARIABLES_DOMAIN)];
  ourtype4 = types4[(int) (kind - VARIABLES_DOMAIN)];

  sr = *matches = NULL;
  tail = NULL;

  if (regexp != NULL)
    {
      /* Make sure spacing is right for C++ operators.
         This is just a courtesy to make the matching less sensitive
         to how many spaces the user leaves between 'operator'
         and <TYPENAME> or <OPERATOR>. */
      char *opend;
      char *opname = operator_chars (regexp, &opend);
      if (*opname)
	{
	  int fix = -1;		/* -1 means ok; otherwise number of spaces needed. */
	  if (isalpha (*opname) || *opname == '_' || *opname == '$')
	    {
	      /* There should 1 space between 'operator' and 'TYPENAME'. */
	      if (opname[-1] != ' ' || opname[-2] == ' ')
		fix = 1;
	    }
	  else
	    {
	      /* There should 0 spaces between 'operator' and 'OPERATOR'. */
	      if (opname[-1] == ' ')
		fix = 0;
	    }
	  /* If wrong number of spaces, fix it. */
	  if (fix >= 0)
	    {
	      char *tmp = (char *) alloca (8 + fix + strlen (opname) + 1);
	      sprintf (tmp, "operator%.*s%s", fix, " ", opname);
	      regexp = tmp;
	    }
	}

      if (0 != (val = re_comp (regexp)))
	error (_("Invalid regexp (%s): %s"), val, regexp);
    }

  /* Search through the partial symtabs *first* for all symbols
     matching the regexp.  That way we don't have to reproduce all of
     the machinery below. */

  ALL_PSYMTABS (objfile, ps)
  {
    struct partial_symbol **bound, **gbound, **sbound;
    int keep_going = 1;

    if (ps->readin)
      continue;

    gbound = objfile->global_psymbols.list + ps->globals_offset + ps->n_global_syms;
    sbound = objfile->static_psymbols.list + ps->statics_offset + ps->n_static_syms;
    bound = gbound;

    /* Go through all of the symbols stored in a partial
       symtab in one loop. */
    psym = objfile->global_psymbols.list + ps->globals_offset;
    while (keep_going)
      {
	if (psym >= bound)
	  {
	    if (bound == gbound && ps->n_static_syms != 0)
	      {
		psym = objfile->static_psymbols.list + ps->statics_offset;
		bound = sbound;
	      }
	    else
	      keep_going = 0;
	    continue;
	  }
	else
	  {
	    QUIT;

	    /* If it would match (logic taken from loop below)
	       load the file and go on to the next one */
	    if (file_matches (ps->filename, files, nfiles)
		&& ((regexp == NULL
		     || re_exec (SYMBOL_NATURAL_NAME (*psym)) != 0)
		    && ((kind == VARIABLES_DOMAIN && SYMBOL_CLASS (*psym) != LOC_TYPEDEF
			 && SYMBOL_CLASS (*psym) != LOC_BLOCK)
			|| (kind == FUNCTIONS_DOMAIN && SYMBOL_CLASS (*psym) == LOC_BLOCK)
			|| (kind == TYPES_DOMAIN && SYMBOL_CLASS (*psym) == LOC_TYPEDEF)
			|| (kind == METHODS_DOMAIN && SYMBOL_CLASS (*psym) == LOC_BLOCK))))
	      {
		PSYMTAB_TO_SYMTAB (ps);
		keep_going = 0;
	      }
	  }
	psym++;
      }
  }

  /* APPLE LOCAL: Make an additional pass over the msymbols raising
     the load level of any objfiles that contain the symbol of interest.
     Otherwise we'll raise it later when we're holding pointers to
     the msymbols in question and those msymbols will become invalid
     when the old objfile is freed and a new one is created and we crash.  */
  if (nfiles == 0 && (kind == VARIABLES_DOMAIN || kind == FUNCTIONS_DOMAIN))
    {
      struct objfile *tmp;
      ALL_OBJFILES_SAFE (objfile, tmp)
        {
          ALL_OBJFILE_MSYMBOLS (objfile, msymbol)
            {
              /* APPLE LOCAL fix-and-continue */
              if (MSYMBOL_OBSOLETED (msymbol))
                continue;
    
	      if (MSYMBOL_TYPE (msymbol) == ourtype ||
	          MSYMBOL_TYPE (msymbol) == ourtype2 ||
	          MSYMBOL_TYPE (msymbol) == ourtype3 ||
	          MSYMBOL_TYPE (msymbol) == ourtype4)
	        {
	          if (regexp == NULL
                       || re_exec (SYMBOL_NATURAL_NAME (msymbol)) != 0)
                    {
                      objfile_set_load_state (objfile, OBJF_SYM_ALL, 1);
                      /* On to the next objfile.  */
                      break;
	            }
	        }
            }
        }
    }

  /* Here, we search through the minimal symbol tables for functions
     and variables that match, and force their symbols to be read.
     This is in particular necessary for demangled variable names,
     which are no longer put into the partial symbol tables.
     The symbol will then be found during the scan of symtabs below.

     For functions, find_pc_symtab should succeed if we have debug info
     for the function, for variables we have to call lookup_symbol
     to determine if the variable has debug info.
     If the lookup fails, set found_misc so that we will rescan to print
     any matching symbols without debug info.
   */

  if (nfiles == 0 && (kind == VARIABLES_DOMAIN || kind == FUNCTIONS_DOMAIN))
    {
      ALL_OBJFILES (objfile)
      {
      ALL_OBJFILE_MSYMBOLS (objfile, msymbol)
      {
        /* APPLE LOCAL fix-and-continue */
        if (MSYMBOL_OBSOLETED (msymbol))
          continue;

	if (MSYMBOL_TYPE (msymbol) == ourtype ||
	    MSYMBOL_TYPE (msymbol) == ourtype2 ||
	    MSYMBOL_TYPE (msymbol) == ourtype3 ||
	    MSYMBOL_TYPE (msymbol) == ourtype4)
	  {
            /* APPLE LOCAL: Don't match the dyld_stub names; no one is
               interested in seeing them when they're doing an 'rbreak' or
               what have you.  */
            if (strncmp (SYMBOL_LINKAGE_NAME (msymbol), "dyld_stub_", 10) == 0)
              continue;

	    if (regexp == NULL
		|| re_exec (SYMBOL_NATURAL_NAME (msymbol)) != 0)
	      {
		if (0 == find_pc_symtab (SYMBOL_VALUE_ADDRESS (msymbol)))
		  {
		    /* FIXME: carlton/2003-02-04: Given that the
		       semantics of lookup_symbol keeps on changing
		       slightly, it would be a nice idea if we had a
		       function lookup_symbol_minsym that found the
		       symbol associated to a given minimal symbol (if
		       any).  */
		    if (kind == FUNCTIONS_DOMAIN)
		      {
			found_misc = 1;
		      }
		    else
		      {
			struct symbol *sym;
			sym = lookup_symbol (SYMBOL_LINKAGE_NAME (msymbol),
					  (struct block *) NULL,
					  VAR_DOMAIN,
					     0, (struct symtab **) NULL);
			  if (!sym || SYMBOL_VALUE_ADDRESS (sym) != SYMBOL_VALUE_ADDRESS (msymbol))
			    found_misc = 1;
		      }
		  }
	      }
	  }
      }
      }
    }

  ALL_SYMTABS (objfile, s)
  {
    bv = BLOCKVECTOR (s);
    /* Often many files share a blockvector.
       Scan each blockvector only once so that
       we don't get every symbol many times.
       It happens that the first symtab in the list
       for any given blockvector is the main file.  */
    if (bv != prev_bv)
      for (i = GLOBAL_BLOCK; i <= STATIC_BLOCK; i++)
	{
	  struct symbol_search *prevtail = tail;
	  int nfound = 0;
	  b = BLOCKVECTOR_BLOCK (bv, i);
	  ALL_BLOCK_SYMBOLS (b, iter, sym)
	    {
	      QUIT;
              /* APPLE LOCAL fix-and-continue */
              if (SYMBOL_OBSOLETED (sym))
                continue;

	      if (file_matches (s->filename, files, nfiles)
		  && ((regexp == NULL
		       || re_exec (SYMBOL_NATURAL_NAME (sym)) != 0)
		      && ((kind == VARIABLES_DOMAIN && SYMBOL_CLASS (sym) != LOC_TYPEDEF
			   && SYMBOL_CLASS (sym) != LOC_BLOCK
			   && SYMBOL_CLASS (sym) != LOC_CONST)
			  || (kind == FUNCTIONS_DOMAIN && SYMBOL_CLASS (sym) == LOC_BLOCK)
			  || (kind == TYPES_DOMAIN && SYMBOL_CLASS (sym) == LOC_TYPEDEF)
			  || (kind == METHODS_DOMAIN && SYMBOL_CLASS (sym) == LOC_BLOCK))))
		{
		  /* match */
		  psr = (struct symbol_search *) xmalloc (sizeof (struct symbol_search));
		  psr->block = i;
		  psr->symtab = s;
		  psr->symbol = sym;
		  psr->msymbol = NULL;
		  psr->next = NULL;
		  if (tail == NULL)
		    sr = psr;
		  else
		    tail->next = psr;
		  tail = psr;
		  nfound ++;
		}
	    }
	  if (nfound > 0)
	    {
	      if (prevtail == NULL)
		{
		  struct symbol_search dummy;

		  dummy.next = sr;
		  tail = sort_search_symbols (&dummy, nfound);
		  sr = dummy.next;

		  old_chain = make_cleanup_free_search_symbols (sr);
		}
	      else
		tail = sort_search_symbols (prevtail, nfound);
	    }
	}
    prev_bv = bv;
  }

  /* If there are no eyes, avoid all contact.  I mean, if there are
     no debug symbols, then print directly from the msymbol_vector.  */

  if (found_misc || kind != FUNCTIONS_DOMAIN)
    {
      ALL_OBJFILES (objfile)
      {
      ALL_OBJFILE_MSYMBOLS (objfile, msymbol)
      {
        /* APPLE LOCAL fix-and-continue */
        if (MSYMBOL_OBSOLETED (msymbol))
          continue;

	if (MSYMBOL_TYPE (msymbol) == ourtype ||
	    MSYMBOL_TYPE (msymbol) == ourtype2 ||
	    MSYMBOL_TYPE (msymbol) == ourtype3 ||
	    MSYMBOL_TYPE (msymbol) == ourtype4)
	  {
	    if (regexp == NULL
		|| re_exec (SYMBOL_NATURAL_NAME (msymbol)) != 0)
	      {
		/* Functions:  Look up by address. */
		if (kind != FUNCTIONS_DOMAIN ||
		    (0 == find_pc_symtab (SYMBOL_VALUE_ADDRESS (msymbol))))
		  {
		    /* Variables/Absolutes:  Look up by name */
		    struct symbol *sym;
		    sym = lookup_symbol (SYMBOL_LINKAGE_NAME (msymbol),
				       (struct block *) NULL, VAR_DOMAIN,
					 0, (struct symtab **) NULL); 
		    if (sym == NULL || SYMBOL_VALUE_ADDRESS (sym) != SYMBOL_VALUE_ADDRESS (msymbol))
		      {
			/* match */
			psr = (struct symbol_search *) xmalloc (sizeof (struct symbol_search));
			psr->block = i;
			psr->msymbol = msymbol;
			psr->symtab = NULL;
			psr->symbol = NULL;
			psr->next = NULL;
			if (tail == NULL)
			  {
			    sr = psr;
			    old_chain = make_cleanup_free_search_symbols (sr);
			  }
			else
			  tail->next = psr;
			tail = psr;
		      }
		  }
	      }
	  }
      }
      }
    }

  *matches = sr;
  if (sr != NULL)
    discard_cleanups (old_chain);
}

/* Helper function for symtab_symbol_info, this function uses
   the data returned from search_symbols() to print information
   regarding the match to gdb_stdout.
 */
static void
print_symbol_info (domain_enum kind, struct symtab *s, struct symbol *sym,
		   int block, char *last)
{
  if (last == NULL || strcmp (last, s->filename) != 0)
    {
      fputs_filtered ("\nFile ", gdb_stdout);
      fputs_filtered (s->filename, gdb_stdout);
      fputs_filtered (":\n", gdb_stdout);
    }

  if (kind != TYPES_DOMAIN && block == STATIC_BLOCK)
    printf_filtered ("static ");

  /* Typedef that is not a C++ class */
  if (kind == TYPES_DOMAIN
      && SYMBOL_DOMAIN (sym) != STRUCT_DOMAIN)
    typedef_print (SYMBOL_TYPE (sym), sym, gdb_stdout);
  /* variable, func, or typedef-that-is-c++-class */
  else if (kind < TYPES_DOMAIN ||
	   (kind == TYPES_DOMAIN &&
	    SYMBOL_DOMAIN (sym) == STRUCT_DOMAIN))
    {
      type_print (SYMBOL_TYPE (sym),
		  (SYMBOL_CLASS (sym) == LOC_TYPEDEF
		   ? "" : SYMBOL_PRINT_NAME (sym)),
		  gdb_stdout, 0);

      printf_filtered (";\n");
    }
}

/* This help function for symtab_symbol_info() prints information
   for non-debugging symbols to gdb_stdout.
 */
static void
print_msymbol_info (struct minimal_symbol *msymbol)
{
  char *tmp;

  if (TARGET_ADDR_BIT <= 32)
    tmp = hex_string_custom (SYMBOL_VALUE_ADDRESS (msymbol)
			     & (CORE_ADDR) 0xffffffff,
			     8);
  else
    tmp = hex_string_custom (SYMBOL_VALUE_ADDRESS (msymbol),
			     16);
  printf_filtered ("%s  %s\n",
		   tmp, SYMBOL_PRINT_NAME (msymbol));
}

/* This is the guts of the commands "info functions", "info types", and
   "info variables". It calls search_symbols to find all matches and then
   print_[m]symbol_info to print out some useful information about the
   matches.
 */
static void
symtab_symbol_info (char *regexp, domain_enum kind, int from_tty)
{
  static char *classnames[]
  =
  {"variable", "function", "type", "method"};
  struct symbol_search *symbols;
  struct symbol_search *p;
  struct cleanup *old_chain;
  char *last_filename = NULL;
  int first = 1;

  /* must make sure that if we're interrupted, symbols gets freed */
  search_symbols (regexp, kind, 0, (char **) NULL, &symbols);
  old_chain = make_cleanup_free_search_symbols (symbols);

  printf_filtered (regexp
		   ? "All %ss matching regular expression \"%s\":\n"
		   : "All defined %ss:\n",
		   classnames[(int) (kind - VARIABLES_DOMAIN)], regexp);

  for (p = symbols; p != NULL; p = p->next)
    {
      QUIT;

      if (p->msymbol != NULL)
	{
	  if (first)
	    {
	      printf_filtered ("\nNon-debugging symbols:\n");
	      first = 0;
	    }
	  print_msymbol_info (p->msymbol);
	}
      else
	{
	  print_symbol_info (kind,
			     p->symtab,
			     p->symbol,
			     p->block,
			     last_filename);
	  last_filename = p->symtab->filename;
	}
    }

  do_cleanups (old_chain);
}

static void
variables_info (char *regexp, int from_tty)
{
  symtab_symbol_info (regexp, VARIABLES_DOMAIN, from_tty);
}

static void
functions_info (char *regexp, int from_tty)
{
  symtab_symbol_info (regexp, FUNCTIONS_DOMAIN, from_tty);
}


static void
types_info (char *regexp, int from_tty)
{
  symtab_symbol_info (regexp, TYPES_DOMAIN, from_tty);
}

/* Breakpoint all functions matching regular expression. */

void
rbreak_command_wrapper (char *regexp, int from_tty)
{
  rbreak_command (regexp, from_tty);
}

static void
rbreak_command (char *regexp, int from_tty)
{
  struct symbol_search *ss;
  struct symbol_search *p;
  struct cleanup *old_chain;
  struct cleanup *objc_selectors_cleanup;

  search_symbols (regexp, FUNCTIONS_DOMAIN, 0, (char **) NULL, &ss);
  old_chain = make_cleanup_free_search_symbols (ss);

  for (p = ss; p != NULL; p = p->next)
    {
      /* We don't want to make people wade through dyld_stub trampolines;
         just skip those symbols.  */
      if (p->symbol && SYMBOL_LINKAGE_NAME (p->symbol)
          && strncmp (SYMBOL_LINKAGE_NAME (p->symbol), "dyld_stub_", 10) == 0)
        continue;
      if (p->msymbol && SYMBOL_LINKAGE_NAME (p->msymbol)
          && strncmp (SYMBOL_LINKAGE_NAME (p->msymbol), "dyld_stub_", 10) == 0)
        continue;

      if (p->msymbol == NULL)
	{
	  char *shlib_ptr = NULL;
	  int shlib_len = 0;

	  if (p->symtab->objfile != NULL && p->symtab->objfile->name != NULL)
	    {
	      shlib_ptr = p->symtab->objfile->name;
	      shlib_len = strlen (p->symtab->objfile->name)
		+ strlen("-shlib \"\" ");
	    }

	  char *string = alloca (shlib_len
				 + strlen (p->symtab->filename)
				 + strlen (SYMBOL_LINKAGE_NAME (p->symbol))
				 + 4);
          /* APPLE LOCAL: To make it easier for decode_line_1() to decode
             ObjC function names, instead of making the canonical names
               /path/name:'symbol-name'
             Make them
               "/path/name:symbol-name"
             Without this change, the decode parser fails on a canonical name
             like 
                /path/name:'-[SKTGraphic init]'
             But if we format it as "/path/name:-[SKTGraphic init]", we'll
             be OK.  cf
                 http://sources.redhat.com/ml/gdb-patches/2003-09/msg00053.html
          */
	  if (shlib_ptr != NULL) 
	    {
	      strcpy (string, "-shlib \"");
	      strcat (string, p->symtab->objfile->name);
	      strcat (string, "\" \"");
	    }
	  else
	    {
	      strcpy (string, "\"");
	    }

	  strcat (string, p->symtab->filename);
	  strcat (string, ":");
	  strcat (string, SYMBOL_LINKAGE_NAME (p->symbol));
	  strcat (string, "\"");
	  /* APPLE LOCAL radar 6366048 search both minsyms & syms for bps.  */
	  rbr_break_command (string, from_tty, 0);
	  print_symbol_info (FUNCTIONS_DOMAIN,
			     p->symtab,
			     p->symbol,
			     p->block,
			     p->symtab->filename);
	}
      else
	{
          /* APPLE LOCAL:  The symbol names are canonical at this point so
             we need to disable allow_objc_selectors_flag or else we might
             have an ambiguous break command if the function name matches
             some random ObjC selectors. 
	     Also add -shlib so we don't move all the breakpoints to the
	     same symbol...  */
	  const char *shlib_ptr = NULL;
	  int shlib_len = 0;
	  
	  
	  if (p->msymbol->ginfo.bfd_section != NULL 
	      && p->msymbol->ginfo.bfd_section->owner != NULL
              && p->msymbol->ginfo.bfd_section->owner->filename != NULL)
	    
	    {
	      shlib_ptr = p->msymbol->ginfo.bfd_section->owner->filename;
	      shlib_len = strlen (shlib_ptr)
		+ strlen("-shlib \"\" ");
	    }
	  
	  char *string = alloca (shlib_len
				 + strlen (SYMBOL_LINKAGE_NAME (p->msymbol))
				 + 4);
	  if (shlib_ptr != NULL) 
	    {
	      strcpy (string, "-shlib \"");
	      strcat (string, shlib_ptr);
	      strcat (string, "\" '");
	    }
	  else
	    {
	      strcpy (string, "'");
	    }
	  
	  strcat (string, SYMBOL_LINKAGE_NAME (p->msymbol));
	  strcat (string, "'");

          allow_objc_selectors_flag = 0;
          objc_selectors_cleanup = 
              make_cleanup (reset_allow_objc_selectors_flag, 0);

	  
	  /* APPLE LOCAL radar 6366048 search both minsyms & syms for bps.  */
	  rbr_break_command (string, from_tty, 1);
	  printf_filtered ("<function, no debug info> %s;\n",
			   SYMBOL_PRINT_NAME (p->msymbol));
	}
    }

  do_cleanups (old_chain);
}


/* Helper routine for make_symbol_completion_list.  */

static int return_val_size;
static int return_val_index;
static char **return_val;

#define COMPLETION_LIST_ADD_SYMBOL(symbol, sym_text, len, text, word) \
      completion_list_add_name \
	(SYMBOL_NATURAL_NAME (symbol), (sym_text), (len), (text), (word))

/*  Test to see if the symbol specified by SYMNAME (which is already
   demangled for C++ symbols) matches SYM_TEXT in the first SYM_TEXT_LEN
   characters.  If so, add it to the current completion list. */

static void
completion_list_add_name (char *symname, char *sym_text, int sym_text_len,
			  char *text, char *word)
{
  int newsize;

  /* clip symbols that cannot match */

  if (strncmp (symname, sym_text, sym_text_len) != 0)
    {
      return;
    }

  /* We have a match for a completion, so add SYMNAME to the current list
     of matches. Note that the name is moved to freshly malloc'd space. */

  {
    char *new;
    if (word == sym_text)
      {
	new = xmalloc (strlen (symname) + 5);
	strcpy (new, symname);
      }
    else if (word > sym_text)
      {
	/* Return some portion of symname.  */
	new = xmalloc (strlen (symname) + 5);
	strcpy (new, symname + (word - sym_text));
      }
    else
      {
	/* Return some of SYM_TEXT plus symname.  */
	new = xmalloc (strlen (symname) + (sym_text - word) + 5);
	strncpy (new, word, sym_text - word);
	new[sym_text - word] = '\0';
	strcat (new, symname);
      }

    if (return_val_index + 3 > return_val_size)
      {
	newsize = (return_val_size *= 2) * sizeof (char *);
	return_val = (char **) xrealloc ((char *) return_val, newsize);
      }
    return_val[return_val_index++] = new;
    return_val[return_val_index] = NULL;
  }
}

/* ObjC: In case we are completing on a selector, look as the msymbol
   again and feed all the selectors into the mill.  */

static void
completion_list_objc_symbol (struct minimal_symbol *msymbol, char *sym_text,
			     int sym_text_len, char *text, char *word)
{
  static char *tmp = NULL;
  static unsigned int tmplen = 0;
    
  char *method, *category, *selector;
  char *tmp2 = NULL;
    
  method = SYMBOL_NATURAL_NAME (msymbol);

  /* Is it a method?  */
  if ((method[0] != '-') && (method[0] != '+'))
    return;

  if (sym_text[0] == '[')
    /* Complete on shortened method method.  */
    completion_list_add_name (method + 1, sym_text, sym_text_len, text, word);
    
  while ((strlen (method) + 1) >= tmplen)
    {
      if (tmplen == 0)
	tmplen = 1024;
      else
	tmplen *= 2;
      tmp = xrealloc (tmp, tmplen);
    }
  selector = strchr (method, ' ');
  if (selector != NULL)
    selector++;
    
  category = strchr (method, '(');
    
  if ((category != NULL) && (selector != NULL))
    {
      memcpy (tmp, method, (category - method));
      tmp[category - method] = ' ';
      memcpy (tmp + (category - method) + 1, selector, strlen (selector) + 1);
      completion_list_add_name (tmp, sym_text, sym_text_len, text, word);
      if (sym_text[0] == '[')
	completion_list_add_name (tmp + 1, sym_text, sym_text_len, text, word);
    }
    
  if (selector != NULL)
    {
      /* Complete on selector only.  */
      strcpy (tmp, selector);
      tmp2 = strchr (tmp, ']');
      if (tmp2 != NULL)
	*tmp2 = '\0';
	
      completion_list_add_name (tmp, sym_text, sym_text_len, text, word);
    }
}

/* Break the non-quoted text based on the characters which are in
   symbols. FIXME: This should probably be language-specific. */

static char *
language_search_unquoted_string (char *text, char *p)
{
  for (; p > text; --p)
    {
      if (isalnum (p[-1]) || p[-1] == '_' || p[-1] == '\0')
	continue;
      else
	{
	  if ((current_language->la_language == language_objc))
	    {
	      if (p[-1] == ':')     /* might be part of a method name */
		continue;
	      else if (p[-1] == '[' && (p[-2] == '-' || p[-2] == '+'))
		p -= 2;             /* beginning of a method name */
	      else if (p[-1] == ' ' || p[-1] == '(' || p[-1] == ')')
		{                   /* might be part of a method name */
		  char *t = p;

		  /* Seeing a ' ' or a '(' is not conclusive evidence
		     that we are in the middle of a method name.  However,
		     finding "-[" or "+[" should be pretty un-ambiguous.
		     Unfortunately we have to find it now to decide.  */

		  while (t > text)
		    if (isalnum (t[-1]) || t[-1] == '_' ||
			t[-1] == ' '    || t[-1] == ':' ||
			t[-1] == '('    || t[-1] == ')')
		      --t;
		    else
		      break;

		  if (t[-1] == '[' && (t[-2] == '-' || t[-2] == '+'))
		    p = t - 2;      /* method name detected */
		  /* else we leave with p unchanged */
		}
	    }
	  break;
	}
    }
  return p;
}


/* Return a NULL terminated array of all symbols (regardless of class)
   which begin by matching TEXT.  If the answer is no symbols, then
   the return value is an array which contains only a NULL pointer.

   Problem: All of the symbols have to be copied because readline frees them.
   I'm not going to worry about this; hopefully there won't be that many.  */

char **
make_symbol_completion_list (char *text, char *word)
{
  struct symbol *sym;
  struct symtab *s;
  struct partial_symtab *ps;
  struct minimal_symbol *msymbol;
  struct objfile *objfile;
  struct block *b, *surrounding_static_block = 0;
  struct dict_iterator iter;
  int j;
  struct partial_symbol **psym;
  /* The symbol we are completing on.  Points in same buffer as text.  */
  char *sym_text;
  /* Length of sym_text.  */
  int sym_text_len;

  /* Now look for the symbol we are supposed to complete on.
     FIXME: This should be language-specific.  */
  {
    char *p;
    char quote_found;
    char *quote_pos = NULL;

    /* First see if this is a quoted string.  */
    quote_found = '\0';
    for (p = text; *p != '\0'; ++p)
      {
	if (quote_found != '\0')
	  {
	    if (*p == quote_found)
	      /* Found close quote.  */
	      quote_found = '\0';
	    else if (*p == '\\' && p[1] == quote_found)
	      /* A backslash followed by the quote character
	         doesn't end the string.  */
	      ++p;
	  }
	else if (*p == '\'' || *p == '"')
	  {
	    quote_found = *p;
	    quote_pos = p;
	  }
      }
    if (quote_found == '\'')
      /* A string within single quotes can be a symbol, so complete on it.  */
      sym_text = quote_pos + 1;
    else if (quote_found == '"')
      /* A double-quoted string is never a symbol, nor does it make sense
         to complete it any other way.  */
      {
	return_val = (char **) xmalloc (sizeof (char *));
	return_val[0] = NULL;
	return return_val;
      }
    else
      {
	/* It is not a quoted string.  Break it based on the characters
	   which are in symbols.  */
	while (p > text)
	  {
	    if (isalnum (p[-1]) || p[-1] == '_' || p[-1] == '\0')
	      --p;
	    else
	      break;
	  }
	sym_text = p;
      }
  }

  return_val_size = 100;
  return_val_index = 0;
  return_val = (char **) xmalloc ((return_val_size + 1) * sizeof (char *));
  return_val[0] = NULL;

  /* Look through the partial symtabs for all symbols which begin
     by matching SYM_TEXT.  Add each one that you find to the list.  */

  sym_text_len = strlen (sym_text);

  ALL_PSYMTABS (objfile, ps)
  {
    /* If the psymtab's been read in we'll get it when we search
       through the blockvector.  */
    if (ps->readin)
      continue;

    for (psym = objfile->global_psymbols.list + ps->globals_offset;
	 psym < (objfile->global_psymbols.list + ps->globals_offset
		 + ps->n_global_syms);
	 psym++)
      {
	/* If interrupted, then quit. */
	QUIT;
	COMPLETION_LIST_ADD_SYMBOL (*psym, sym_text, sym_text_len, text, word);
      }

    for (psym = objfile->static_psymbols.list + ps->statics_offset;
	 psym < (objfile->static_psymbols.list + ps->statics_offset
		 + ps->n_static_syms);
	 psym++)
      {
	QUIT;
	COMPLETION_LIST_ADD_SYMBOL (*psym, sym_text, sym_text_len, text, word);
      }
  }

  /* At this point scan through the misc symbol vectors and add each
     symbol you find to the list.  Eventually we want to ignore
     anything that isn't a text symbol (everything else will be
     handled by the psymtab code above).  */

  ALL_MSYMBOLS (objfile, msymbol)
  {
    QUIT;
    COMPLETION_LIST_ADD_SYMBOL (msymbol, sym_text, sym_text_len, text, word);

    completion_list_objc_symbol (msymbol, sym_text, sym_text_len, text, word);
  }

  /* Search upwards from currently selected frame (so that we can
     complete on local vars.  */

  for (b = get_selected_block (0); b != NULL; b = BLOCK_SUPERBLOCK (b))
    {
      if (!BLOCK_SUPERBLOCK (b))
	{
	  surrounding_static_block = b;		/* For elmin of dups */
	}

      /* Also catch fields of types defined in this places which match our
         text string.  Only complete on types visible from current context. */

      ALL_BLOCK_SYMBOLS (b, iter, sym)
	{
	  QUIT;
	  COMPLETION_LIST_ADD_SYMBOL (sym, sym_text, sym_text_len, text, word);
	  if (SYMBOL_CLASS (sym) == LOC_TYPEDEF)
	    {
	      struct type *t = SYMBOL_TYPE (sym);
	      enum type_code c = TYPE_CODE (t);

	      if (c == TYPE_CODE_UNION || c == TYPE_CODE_STRUCT)
		{
		  for (j = TYPE_N_BASECLASSES (t); j < TYPE_NFIELDS (t); j++)
		    {
		      if (TYPE_FIELD_NAME (t, j))
			{
			  completion_list_add_name (TYPE_FIELD_NAME (t, j),
					sym_text, sym_text_len, text, word);
			}
		    }
		}
	    }
	}
    }

  /* Go through the symtabs and check the externs and statics for
     symbols which match.  */

  ALL_SYMTABS (objfile, s)
  {
    QUIT;
    b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), GLOBAL_BLOCK);
    ALL_BLOCK_SYMBOLS (b, iter, sym)
      {
	COMPLETION_LIST_ADD_SYMBOL (sym, sym_text, sym_text_len, text, word);
      }
  }

  ALL_SYMTABS (objfile, s)
  {
    QUIT;
    b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK);
    /* Don't do this block twice.  */
    if (b == surrounding_static_block)
      continue;
    ALL_BLOCK_SYMBOLS (b, iter, sym)
      {
	COMPLETION_LIST_ADD_SYMBOL (sym, sym_text, sym_text_len, text, word);
      }
  }

  return (return_val);
}

/* Like make_symbol_completion_list, but returns a list of symbols
   defined in a source file FILE.  */

char **
make_file_symbol_completion_list (char *text, char *word, char *srcfile)
{
  struct symbol *sym;
  struct symtab *s;
  struct block *b;
  struct dict_iterator iter;
  /* The symbol we are completing on.  Points in same buffer as text.  */
  char *sym_text;
  /* Length of sym_text.  */
  int sym_text_len;

  /* Now look for the symbol we are supposed to complete on.
     FIXME: This should be language-specific.  */
  {
    char *p;
    char quote_found;
    char *quote_pos = NULL;

    /* First see if this is a quoted string.  */
    quote_found = '\0';
    for (p = text; *p != '\0'; ++p)
      {
	if (quote_found != '\0')
	  {
	    if (*p == quote_found)
	      /* Found close quote.  */
	      quote_found = '\0';
	    else if (*p == '\\' && p[1] == quote_found)
	      /* A backslash followed by the quote character
	         doesn't end the string.  */
	      ++p;
	  }
	else if (*p == '\'' || *p == '"')
	  {
	    quote_found = *p;
	    quote_pos = p;
	  }
      }
    if (quote_found == '\'')
      /* A string within single quotes can be a symbol, so complete on it.  */
      sym_text = quote_pos + 1;
    else if (quote_found == '"')
      /* A double-quoted string is never a symbol, nor does it make sense
         to complete it any other way.  */
      {
	return_val = (char **) xmalloc (sizeof (char *));
	return_val[0] = NULL;
	return return_val;
      }
    else
      {
	/* Not a quoted string.  */
	sym_text = language_search_unquoted_string (text, p);
      }
  }

  return_val_size = 10;
  return_val_index = 0;
  return_val = (char **) xmalloc ((return_val_size + 1) * sizeof (char *));
  return_val[0] = NULL;

  /* Look through the partial symtabs for all symbols which begin
     by matching SYM_TEXT.  Add each one that you find to the list.  */

  sym_text_len = strlen (sym_text);

  /* Find the symtab for SRCFILE (this loads it if it was not yet read
     in).  */
  s = lookup_symtab (srcfile);
  if (s == NULL)
    {
      /* Maybe they typed the file with leading directories, while the
	 symbol tables record only its basename.  */
      const char *tail = lbasename (srcfile);

      if (tail > srcfile)
	s = lookup_symtab (tail);
    }

  /* If we have no symtab for that file, return an empty list.  */
  if (s == NULL)
    return (return_val);

  /* Go through this symtab and check the externs and statics for
     symbols which match.  */

  b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), GLOBAL_BLOCK);
  ALL_BLOCK_SYMBOLS (b, iter, sym)
    {
      COMPLETION_LIST_ADD_SYMBOL (sym, sym_text, sym_text_len, text, word);
    }

  b = BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK);
  ALL_BLOCK_SYMBOLS (b, iter, sym)
    {
      COMPLETION_LIST_ADD_SYMBOL (sym, sym_text, sym_text_len, text, word);
    }

  return (return_val);
}

/* A helper function for make_source_files_completion_list.  It adds
   another file name to a list of possible completions, growing the
   list as necessary.  */

static void
add_filename_to_list (const char *fname, char *text, char *word,
		      char ***list, int *list_used, int *list_alloced)
{
  char *new;
  size_t fnlen = strlen (fname);

  if (*list_used + 1 >= *list_alloced)
    {
      *list_alloced *= 2;
      *list = (char **) xrealloc ((char *) *list,
				  *list_alloced * sizeof (char *));
    }

  if (word == text)
    {
      /* Return exactly fname.  */
      new = xmalloc (fnlen + 5);
      strcpy (new, fname);
    }
  else if (word > text)
    {
      /* Return some portion of fname.  */
      new = xmalloc (fnlen + 5);
      strcpy (new, fname + (word - text));
    }
  else
    {
      /* Return some of TEXT plus fname.  */
      new = xmalloc (fnlen + (text - word) + 5);
      strncpy (new, word, text - word);
      new[text - word] = '\0';
      strcat (new, fname);
    }
  (*list)[*list_used] = new;
  (*list)[++*list_used] = NULL;
}

static int
not_interesting_fname (const char *fname)
{
  static const char *illegal_aliens[] = {
    "_globals_",	/* inserted by coff_symtab_read */
    NULL
  };
  int i;

  for (i = 0; illegal_aliens[i]; i++)
    {
      if (strcmp (fname, illegal_aliens[i]) == 0)
	return 1;
    }
  return 0;
}

/* Return a NULL terminated array of all source files whose names
   begin with matching TEXT.  The file names are looked up in the
   symbol tables of this program.  If the answer is no matchess, then
   the return value is an array which contains only a NULL pointer.  */

char **
make_source_files_completion_list (char *text, char *word)
{
  struct symtab *s;
  struct partial_symtab *ps;
  struct objfile *objfile;
  int first = 1;
  int list_alloced = 1;
  int list_used = 0;
  size_t text_len = strlen (text);
  char **list = (char **) xmalloc (list_alloced * sizeof (char *));
  const char *base_name;

  list[0] = NULL;

  if (!have_full_symbols () && !have_partial_symbols ())
    return list;

  ALL_SYMTABS (objfile, s)
    {
      if (not_interesting_fname (s->filename))
	continue;
      if (!filename_seen (s->filename, 1, &first)
#if HAVE_DOS_BASED_FILE_SYSTEM
	  && strncasecmp (s->filename, text, text_len) == 0
#else
	  && strncmp (s->filename, text, text_len) == 0
#endif
	  )
	{
	  /* This file matches for a completion; add it to the current
	     list of matches.  */
	  add_filename_to_list (s->filename, text, word,
				&list, &list_used, &list_alloced);
	}
      else
	{
	  /* NOTE: We allow the user to type a base name when the
	     debug info records leading directories, but not the other
	     way around.  This is what subroutines of breakpoint
	     command do when they parse file names.  */
	  base_name = lbasename (s->filename);
	  if (base_name != s->filename
	      && !filename_seen (base_name, 1, &first)
#if HAVE_DOS_BASED_FILE_SYSTEM
	      && strncasecmp (base_name, text, text_len) == 0
#else
	      && strncmp (base_name, text, text_len) == 0
#endif
	      )
	    add_filename_to_list (base_name, text, word,
				  &list, &list_used, &list_alloced);
	}
    }

  ALL_PSYMTABS (objfile, ps)
    {
      if (not_interesting_fname (ps->filename))
	continue;
      if (!ps->readin)
	{
	  if (!filename_seen (ps->filename, 1, &first)
#if HAVE_DOS_BASED_FILE_SYSTEM
	      && strncasecmp (ps->filename, text, text_len) == 0
#else
	      && strncmp (ps->filename, text, text_len) == 0
#endif
	      )
	    {
	      /* This file matches for a completion; add it to the
		 current list of matches.  */
	      add_filename_to_list (ps->filename, text, word,
				    &list, &list_used, &list_alloced);

	    }
	  else
	    {
	      base_name = lbasename (ps->filename);
	      if (base_name != ps->filename
		  && !filename_seen (base_name, 1, &first)
#if HAVE_DOS_BASED_FILE_SYSTEM
		  && strncasecmp (base_name, text, text_len) == 0
#else
		  && strncmp (base_name, text, text_len) == 0
#endif
		  )
		add_filename_to_list (base_name, text, word,
				      &list, &list_used, &list_alloced);
	    }
	}
    }

  return list;
}

/* Determine if PC is in the prologue of a function.  The prologue is the area
   between the first instruction of a function, and the first executable line.
   Returns 1 if PC *might* be in prologue, 0 if definately *not* in prologue.

   If non-zero, func_start is where we think the prologue starts, possibly
   by previous examination of symbol table information.
 */

int
in_prologue (CORE_ADDR pc, CORE_ADDR func_start)
{
  struct symtab_and_line sal;
  CORE_ADDR func_addr, func_end;

  /* We have several sources of information we can consult to figure
     this out.
     - Compilers usually emit line number info that marks the prologue
       as its own "source line".  So the ending address of that "line"
       is the end of the prologue.  If available, this is the most
       reliable method.
     - The minimal symbols and partial symbols, which can usually tell
       us the starting and ending addresses of a function.
     - If we know the function's start address, we can call the
       architecture-defined SKIP_PROLOGUE function to analyze the
       instruction stream and guess where the prologue ends.
     - Our `func_start' argument; if non-zero, this is the caller's
       best guess as to the function's entry point.  At the time of
       this writing, handle_inferior_event doesn't get this right, so
       it should be our last resort.  */

  /* Consult the partial symbol table, to find which function
     the PC is in.  */
  if (! find_pc_partial_function_no_inlined (pc, NULL, &func_addr, &func_end))
    {
      CORE_ADDR prologue_end;
      CORE_ADDR scan_from;

      /* We don't even have minsym information, so fall back to using
         func_start, if given.  */
      if (!func_start)
	return 1;  /* We *might* be in a prologue.  */
      else
	scan_from = func_start;
      
      prologue_end = SKIP_PROLOGUE (scan_from);
      
      return scan_from <= pc && pc < prologue_end;
    }

  /* If we have line number information for the function, that's
     usually pretty reliable.  */
  sal = find_pc_line (func_addr, 0);

  /* Now sal describes the source line at the function's entry point,
     which (by convention) is the prologue.  The end of that "line",
     sal.end, is the end of the prologue.

     Note that, for functions whose source code is all on a single
     line, the line number information doesn't always end up this way.
     So we must verify that our purported end-of-prologue address is
     *within* the function, not at its start or end.  */
  if (sal.line == 0
      || sal.end <= func_addr
      || func_end <= sal.end)
    {
      /* We don't have any good line number info, so use the minsym
	 information, together with the architecture-specific prologue
	 scanning code.  */
      CORE_ADDR prologue_end = SKIP_PROLOGUE (func_addr);
      
      if (func_addr <= pc && pc < prologue_end)
        return 1;
      
      /* Okay, we will try one more thing here.  I have seen cases,
         when we have little symbolic information, where the func_addr
         is from way back up in the executable, and bears no relation
         to the current function.  So let's see if we recognize the pc
         as the beginning of a prologue.  In many cases we call
         in_prologue to see if we have arrived at the start of the
         prologue, so this is a reasonable thing to do. */
      
      prologue_end = SKIP_PROLOGUE (pc);
      if (prologue_end > pc + 4)
        return 1;
      else
        return 0;
    }
  
  /* We have line number info, and it looks good.  */
  return func_addr <= pc && pc < sal.end;
}

/* Given PC at the function's start address, attempt to find the
   prologue end using SAL information.  Return zero if the skip fails.

   A non-optimized prologue traditionally has one SAL for the function
   and a second for the function body.  A single line function has
   them both pointing at the same line.

   An optimized prologue is similar but the prologue may contain
   instructions (SALs) from the instruction body.  Need to skip those
   while not getting into the function body.

   The functions end point and an increasing SAL line are used as
   indicators of the prologue's endpoint.

   This code is based on the function refine_prologue_limit (versions
   found in both ia64 and ppc).  */

CORE_ADDR
skip_prologue_using_sal (CORE_ADDR func_addr)
{
  struct symtab_and_line prologue_sal;
  CORE_ADDR start_pc;
  CORE_ADDR end_pc;

  /* Get an initial range for the function.  */
  find_pc_partial_function_no_inlined (func_addr, NULL, &start_pc, &end_pc);
  start_pc += DEPRECATED_FUNCTION_START_OFFSET;

  prologue_sal = find_pc_line (start_pc, 0);
  if (prologue_sal.line != 0)
    {
      while (prologue_sal.end < end_pc)
	{
	  struct symtab_and_line sal;

	  sal = find_pc_line (prologue_sal.end, 0);
	  if (sal.line == 0)
	    break;
	  /* Assume that a consecutive SAL for the same (or larger)
	     line mark the prologue -> body transition.  */
	  if (sal.line >= prologue_sal.line)
	    break;
	  /* The case in which compiler's optimizer/scheduler has
	     moved instructions into the prologue.  We look ahead in
	     the function looking for address ranges whose
	     corresponding line number is less the first one that we
	     found for the function.  This is more conservative then
	     refine_prologue_limit which scans a large number of SALs
	     looking for any in the prologue */
	  prologue_sal = sal;
	}
    }
  return prologue_sal.end;
}

struct symtabs_and_lines
decode_line_spec (char *string, int funfirstline)
{
  struct symtabs_and_lines sals;
  struct symtab_and_line cursal;
  
  if (string == 0)
    error (_("Empty line specification."));
    
  /* We use whatever is set as the current source line. We do not try
     and get a default  or it will recursively call us! */  
  cursal = get_current_source_symtab_and_line ();
  
  /* APPLE LOCAL begin return multiple symbols  */
  sals = decode_line_1 (&string, funfirstline,
			cursal.symtab, cursal.line,
			(char ***) NULL, NULL, 0);
  /* APPLE LOCAL end return multiple symbols  */

  if (*string)
    error (_("Junk at end of line specification: %s"), string);
  return sals;
}

/* Track MAIN */
static char *name_of_main;

void
set_main_name (const char *name)
{
  if (name_of_main != NULL)
    {
      xfree (name_of_main);
      name_of_main = NULL;
    }
  if (name != NULL)
    {
      name_of_main = xstrdup (name);
    }
}

/* Deduce the name of the main procedure, and set NAME_OF_MAIN
   accordingly.  */

static void
find_main_name (void)
{
  char *new_main_name;

  /* Try to see if the main procedure is in Ada.  */
  /* FIXME: brobecker/2005-03-07: Another way of doing this would
     be to add a new method in the language vector, and call this
     method for each language until one of them returns a non-empty
     name.  This would allow us to remove this hard-coded call to
     an Ada function.  It is not clear that this is a better approach
     at this point, because all methods need to be written in a way
     such that false positives never be returned. For instance, it is
     important that a method does not return a wrong name for the main
     procedure if the main procedure is actually written in a different
     language.  It is easy to guaranty this with Ada, since we use a
     special symbol generated only when the main in Ada to find the name
     of the main procedure. It is difficult however to see how this can
     be guarantied for languages such as C, for instance.  This suggests
     that order of call for these methods becomes important, which means
     a more complicated approach.  */
  new_main_name = ada_main_name ();
  if (new_main_name != NULL)
    { 
      set_main_name (new_main_name);
      return;
    }

  /* The languages above didn't identify the name of the main procedure.
     Fallback to "main".  */
  set_main_name ("main");
}

char *
main_name (void)
{
  if (name_of_main == NULL)
    find_main_name ();

  return name_of_main;
}

/* Handle ``executable_changed'' events for the symtab module.  */

static void
symtab_observer_executable_changed (void *unused)
{
  /* NAME_OF_MAIN may no longer be the same, so reset it for now.  */
  set_main_name (NULL);
}

void
_initialize_symtab (void)
{
  add_info ("variables", variables_info, _("\
All global and static variable names, or those matching REGEXP."));
  if (dbx_commands)
    add_com ("whereis", class_info, variables_info, _("\
All global and static variable names, or those matching REGEXP."));

  add_info ("functions", functions_info,
	    _("All function names, or those matching REGEXP."));

  
  /* FIXME:  This command has at least the following problems:
     1.  It prints builtin types (in a very strange and confusing fashion).
     2.  It doesn't print right, e.g. with
     typedef struct foo *FOO
     type_print prints "FOO" when we want to make it (in this situation)
     print "struct foo *".
     I also think "ptype" or "whatis" is more likely to be useful (but if
     there is much disagreement "info types" can be fixed).  */
  add_info ("types", types_info,
	    _("All type names, or those matching REGEXP."));

  add_info ("sources", sources_info,
	    _("Source files in the program."));

  add_com ("rbreak", class_breakpoint, rbreak_command,
	   _("Set a breakpoint for all functions matching REGEXP."));

  if (xdb_commands)
    {
      add_com ("lf", class_info, sources_info,
	       _("Source files in the program"));
      add_com ("lg", class_info, variables_info, _("\
All global and static variable names, or those matching REGEXP."));
    }

  /* Initialize the one built-in type that isn't language dependent... */
  builtin_type_error = init_type (TYPE_CODE_ERROR, 0, 0,
				  "<unknown type>", (struct objfile *) NULL);

  observer_attach_executable_changed (symtab_observer_executable_changed);
}

/* APPLE LOCAL begin address ranges  */
void
update_inlined_function_line_table_entry (CORE_ADDR start_pc, 
					  CORE_ADDR current_end_pc,
					  CORE_ADDR proper_end_pc)
{
  asection *section;
  struct symtab *s;
  struct linetable *l;
  int len;
  int i;
  int done;
  struct linetable_entry *item;
  struct linetablke_entry *prev;
  struct blockvector *bv;

  section = find_pc_overlay (start_pc);
  if (pc_in_unmapped_range (start_pc, section))
    start_pc = overlay_mapped_address (start_pc, section);

  s = find_pc_sect_symtab (start_pc, section);

  gdb_assert (s != NULL);

  bv = BLOCKVECTOR (s);
  done = 0;

  for (; s && BLOCKVECTOR (s) == bv && !done; s = s->next)
    {
      l = LINETABLE (s);
      if (!l)
	continue;
      len = l->nitems;
      if (len <= 0)
	continue;

      prev = NULL;
      item = l->item;

      for (i = 0; i < len && !done; i++, item++)
	{
	  if (item->pc > start_pc)
	    break;

	  if (item->pc == start_pc
	      && item->end_pc == current_end_pc
	      && item->entry_type == INLINED_SUBROUTINE_LT_ENTRY)
	    {
	      item->end_pc = proper_end_pc;
	      done = 1;
	    }
	}
    }

  gdb_assert (done == 1);
}
/* APPLE LOCAL end address ranges  */


/* APPLE LOCAL begin cache lookup values for improved performance  */

/* Something in the objfiles has changed, so we need to throw away all
   our cached lookup values and start again, to make sure we aren't
   caching and returning stale data.  */

void
symtab_clear_cached_lookup_values (void)
{
  cached_mapped_section = NULL;
  cached_overlay_section = NULL;
  cached_sect_section = NULL;
  cached_symtab = NULL;
  cached_psymtab = NULL;
  cached_pc_line = NULL;
  cached_pc_function = NULL;
  cached_blockvector = NULL;
  cached_blockvector_index = -1;
  cached_block = NULL;
  
  last_block_lookup_pc = INVALID_ADDRESS;
  last_blockvector_lookup_pc = INVALID_ADDRESS;
  last_function_lookup_pc = INVALID_ADDRESS;
  last_pc_line_lookup_pc = INVALID_ADDRESS;
  last_psymtab_lookup_pc = INVALID_ADDRESS;
  last_symtab_lookup_pc = INVALID_ADDRESS;
  last_sect_section_lookup_pc = INVALID_ADDRESS;
  last_mapped_section_lookup_pc = INVALID_ADDRESS;
  last_overlay_section_lookup_pc = INVALID_ADDRESS;
}
/* APPLE LOCAL end cache lookup values for improved performance  */
