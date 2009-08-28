/* Parser for linespec for the GNU debugger, GDB.

   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994,
   1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005
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
#include "frame.h"
#include "command.h"
#include "symfile.h"
#include "objfiles.h"
#include "source.h"
#include "demangle.h"
#include "value.h"
#include "completer.h"
#include "cp-abi.h"
#include "parser-defs.h"
#include "block.h"
#include "objc-lang.h"
#include "linespec.h"
#include "language.h"
#include "ui-out.h" /* for ui_out_is_mi_like_p */
#include "exceptions.h"
/* APPLE LOCAL begin return multiple symbols  */
#include "gdb_assert.h"
#include "breakpoint.h"
/* APPLE LOCAL end return multiple symbols  */

/* APPLE LOCAL: Controls whether decode_line_1* will search for
   ObjC selectors when parsing function name arguments. */
int allow_objc_selectors_flag = 1;

/* We share this one with symtab.c, but it is not exported widely. */

extern char *operator_chars (char *, char **);

/* Prototypes for local functions */

static void initialize_defaults (struct symtab **default_symtab,
				 int *default_line);

static void set_flags (char *arg, int *is_quoted, char **paren_pointer);

static struct symtabs_and_lines decode_indirect (char **argptr);

static char *locate_first_half (char **argptr, int *is_quote_enclosed);

static struct symtabs_and_lines decode_objc (char **argptr,
					     int funfirstline,
					     struct symtab *file_symtab,
					     char ***canonical,
					     char *saved_arg);
/* APPLE LOCAL: decode_compound needs the not_found_ptr or future-break
   won't work.  */
static struct symtabs_and_lines decode_compound (char **argptr,
						 int funfirstline,
						 char ***canonical,
						 char *saved_arg,
						 char *p,
						 int *not_found_ptr);

static struct symbol *lookup_prefix_sym (char **argptr, char *p);

/* APPLE LOCAL: find_method needs the not_found_ptr or future-break
   won't work.  */

static struct symtabs_and_lines find_method (int funfirstline,
					     char ***canonical,
					     char *saved_arg,
					     char *copy,
					     struct type *t,
					     struct symbol *sym_class, 
					     int *not_found_ptr);

/* APPLE LOCAL begin return multiple symbols  */
static int collect_methods (char *copy, struct type *t,
			    struct symbol ***sym_arr, int *sym_arr_size);
/* APPLE LOCAL end return multiple symbols  */

static NORETURN void cplusplus_error (const char *name,
				      const char *fmt, ...)
     ATTR_NORETURN ATTR_FORMAT (printf, 2, 3);

static int total_number_of_methods (struct type *type);

/* APPLE LOCAL begin return multiple symbols  */
static int find_methods (struct type *, char *, struct symbol ***, int *,
			 int *);

static int add_matching_methods (int method_counter, struct type *t,
				 struct symbol ***sym_arr, int *sym_arr_size,
				 int *sym_arr_pos);

static int add_constructors (int method_counter, struct type *t,
			     struct symbol ***sym_arr, int *sym_arr_size,
			     int *sym_arr_pos);
/* APPLE LOCAL end return multiple symbols  */

static void build_canonical_line_spec (struct symtab_and_line *,
				       char *, char ***);

static char *find_toplevel_char (char *s, char c);

static int is_objc_method_format (const char *s);

static struct symtabs_and_lines decode_line_2 (struct symbol *[],
					       int, int, int, int, char ***);

static struct symtab **symtab_from_filename (char **argptr,
					    char *p, int is_quote_enclosed,
					    int *not_found_ptr);

/* APPLE LOCAL: I added the parsed_lineno so we could directly do error
   reporting if there were no actual matches to that file & lineno.  */
 static struct
symtabs_and_lines decode_all_digits_exhaustive (char **argptr, 
                                   int funfirstline,
                                   struct symtab *default_symtab,
                                   int default_line,
                                   char ***canonical,
                                   struct symtab *file_symtab,
                                   char *q,
				   int *parsed_lineno,
				   int *not_found_ptr);

static struct
symtabs_and_lines decode_all_digits (char **argptr,
				     /* APPLE LOCAL linespec */
				     int funfirstline,
				     struct symtab *default_symtab,
				     int default_line,
				     char ***canonical,
				     struct symtab *file_symtab,
				     char *q);

static struct symtabs_and_lines decode_dollar (char *copy,
					       int funfirstline,
					       struct symtab *default_symtab,
					       char ***canonical,
					       struct symtab *file_symtab);

static struct symtabs_and_lines decode_variable (char *copy,
						 int funfirstline,
						 /* APPLE LOCAL equivalences */
						 int equivalencies,
						 char ***canonical,
						 struct symtab *file_symtab,
						 int *not_found_ptr);

/* APPLE LOCAL begin return multiple symbols  */
static struct symtabs_and_lines decode_all_variables (char *copy,
						      int funfirstline,
						      /* APPLE LOCAL equivalences */
						      int equivalencies,
						      char ***canonical,
						      struct symtab *file_symtab,
						      int *not_found_ptr);

/* APPLE LOCAL end return multiple symbols  */

static struct
symtabs_and_lines symbol_found (int funfirstline,
				char ***canonical,
				char *copy,
				struct symbol *sym,
				struct symtab *file_symtab,
				struct symtab *sym_symtab);

/* APPLE LOCAL begin return multiple symbols  */
static struct
symtabs_and_lines symbols_found (int funfirstline,
				 char ***canonical,
				 char *copy,
				 struct symbol_search *sym_list,
				 struct symtab *file_symtab);
/* APPLE LOCAL end return multiple symbols  */

/* APPLE LOCAL: Added the canonical arg, since we might find an
   "equivalent" symbol as well, and thus set more than one breakpoint,
   we need to get the names right.  */
static struct
symtabs_and_lines minsym_found (int funfirstline, int equivalencies,
				struct minimal_symbol *msymbol,
				char ***canonical);

/* APPLE LOCAL begin return multiple symbols  */
static struct
symtabs_and_lines minsyms_found (int funfirstline, int equivalencies,
				 struct symbol_search *sym_list,
				 char ***canonical);
/* APPLE LOCAL end return multiple symbols  */

/* APPLE LOCAL inlined function symbols & blocks  */
static int one_block_contains_other (struct blockvector *, int, int);

/* Helper functions. */

/* Issue a helpful hint on using the command completion feature on
   single quoted demangled C++ symbols as part of the completion
   error.  */

static NORETURN void
cplusplus_error (const char *name, const char *fmt, ...)
{
  struct ui_file *tmp_stream;
  tmp_stream = mem_fileopen ();
  make_cleanup_ui_file_delete (tmp_stream);

  {
    va_list args;
    va_start (args, fmt);
    vfprintf_unfiltered (tmp_stream, fmt, args);
    va_end (args);
  }

  while (*name == '\'')
    name++;
  fprintf_unfiltered (tmp_stream,
		      ("Hint: try '%s<TAB> or '%s<ESC-?>\n"
		       "(Note leading single quote.)"),
		      name, name);
  error_stream (tmp_stream);
}

/* Return the number of methods described for TYPE, including the
   methods from types it derives from. This can't be done in the symbol
   reader because the type of the baseclass might still be stubbed
   when the definition of the derived class is parsed.  */

static int
total_number_of_methods (struct type *type)
{
  int n;
  int count;

  CHECK_TYPEDEF (type);
  if (TYPE_CPLUS_SPECIFIC (type) == NULL)
    return 0;
  count = TYPE_NFN_FIELDS_TOTAL (type);

  for (n = 0; n < TYPE_N_BASECLASSES (type); n++)
    count += total_number_of_methods (TYPE_BASECLASS (type, n));

  return count;
}

/* APPLE LOCAL begin return multiple symbols  */
/* Recursive helper function for decode_line_1.  Look for methods
   named NAME in type T.  Return number of matches.  Put matches in
   SYM_ARR, which should have been allocated with a size of
   total_number_of_methods (T) * sizeof (struct symbol *).  Note that
   this function is g++ specific.  Pass SYM_ARR_SIZE and SYM_ARR_POS
   along as well to functions that fill in SYM_ARR, in case they need
   to grow the array.  */

static int
find_methods (struct type *t, char *name, struct symbol ***sym_arr, 
	      int *sym_arr_size, int *sym_arr_pos)
/* APPLE LOCAL end return multiple symbols  */
{
  int i1 = 0;
  int ibase;
  char *class_name = type_name_no_tag (t);
  struct cleanup *old_chain;

  /* APPLE LOCAL: The whole point of this exercise is to look for
     C++ methods, so let's set the language to cplusplus before trying.  
     FIXME: Dunno whether this will break Java or not, but since we 
     don't use our gdb for Java, this isn't that big a deal.  The comment
     at the start of the function says this is g++ specific anyway...
  */
  old_chain = make_cleanup_restore_language (language_cplus);


  /* Ignore this class if it doesn't have a name.  This is ugly, but
     unless we figure out how to get the physname without the name of
     the class, then the loop can't do any good.  */
  if (class_name
      && (lookup_symbol (class_name, (struct block *) NULL,
			 STRUCT_DOMAIN, (int *) NULL,
			 (struct symtab **) NULL)))
    {
      int method_counter;
      int name_len = strlen (name);

      CHECK_TYPEDEF (t);

      /* Loop over each method name.  At this level, all overloads of a name
         are counted as a single name.  There is an inner loop which loops over
         each overload.  */

      for (method_counter = TYPE_NFN_FIELDS (t) - 1;
	   method_counter >= 0;
	   --method_counter)
	{
	  char *method_name = TYPE_FN_FIELDLIST_NAME (t, method_counter);
	  char dem_opname[64];

	  if (strncmp (method_name, "__", 2) == 0 ||
	      strncmp (method_name, "op", 2) == 0 ||
	      strncmp (method_name, "type", 4) == 0)
	    {
	      if (cplus_demangle_opname (method_name, dem_opname, DMGL_ANSI))
		method_name = dem_opname;
	      else if (cplus_demangle_opname (method_name, dem_opname, 0))
		method_name = dem_opname;
	    }

	  if (strcmp_iw (name, method_name) == 0)
	    /* Find all the overloaded methods with that name.  */
	    /* APPLE LOCAL begin return multiple symbols  */
	    i1 += add_matching_methods (method_counter, t,
					sym_arr, sym_arr_size,
					sym_arr_pos);
	    /* APPLE LOCAL end return multiple symbols  */
	  else if (strncmp (class_name, name, name_len) == 0
		   && (class_name[name_len] == '\0'
		       || class_name[name_len] == '<'))
	    i1 += add_constructors (method_counter, t,
				    sym_arr, sym_arr_size,
				    sym_arr_pos);
	}
    }

  /* Only search baseclasses if there is no match yet, since names in
     derived classes override those in baseclasses.

     FIXME: The above is not true; it is only true of member functions
     if they have the same number of arguments (??? - section 13.1 of the
     ARM says the function members are not in the same scope but doesn't
     really spell out the rules in a way I understand.  In any case, if
     the number of arguments differ this is a case in which we can overload
     rather than hiding without any problem, and gcc 2.4.5 does overload
     rather than hiding in this case).  */

  if (i1 == 0)
    for (ibase = 0; ibase < TYPE_N_BASECLASSES (t); ibase++)
      /* APPLE LOCAL begin return multiple symbols  */
      i1 += find_methods (TYPE_BASECLASS (t, ibase), name, sym_arr,
			  sym_arr_size, sym_arr_pos);
      /* APPLE LOCAL end return multiple symbols  */

  /* APPLE LOCAL - restore the language.  */
  do_cleanups (old_chain);

  return i1;
}

/* APPLE LOCAL begin return multiple symbols  */

/* Given an array of symbols (SYM_ARR) and the number of elements
   currently in the array (SYM_ARR_POS), go through SYM_LIST and
   remove any elements from SYM_LIST that are already in SYM_ARR.  */

static void
remove_duplicate_symbols (struct symbol **sym_arr, int sym_arr_pos,
			  struct symbol_search **sym_list)
{
  struct symbol_search *cur;
  struct symbol_search *prev;
  int i;

  for (i = 0; i < sym_arr_pos; i++)
    {
      prev = NULL;
      cur = *sym_list;
      while (cur)
	{
	  if (cur->symbol == sym_arr[i])
	    {
	      if (prev)
		prev->next = cur->next;
	      else
		*sym_list = cur->next;
	    }
	  else
	    prev = cur;

	  cur = cur->next;
	}
    }
}

/* Add the symbols associated to methods of the class whose type is T
   and whose name matches the method indexed by METHOD_COUNTER in the
   array SYM_ARR.  SYM_ARR_SIZE is the number of elements allocated in
   SYM_ARR (in case we need to grow the array).  SYM_ARR_POS is the
   next open position in the array.  Return the number of methods
   added.  */

static int
add_matching_methods (int method_counter, struct type *t,
		      struct symbol ***sym_arr, int *sym_arr_size,
		      int *sym_arr_pos)
/* APPLE LOCAL end return multiple symbols  */
{
  int field_counter;
  int i1 = 0;
  /* APPLE LOCAL begin return multiple symbols  */
  int syms_found;
  struct symbol_search *sym_list;
  /* APPLE LOCAL end return multiple symbols  */

  for (field_counter = TYPE_FN_FIELDLIST_LENGTH (t, method_counter) - 1;
       field_counter >= 0;
       --field_counter)
    {
      struct fn_field *f;
      char *phys_name;
      /* APPLE LOCAL begin return multiple symbols  */
      syms_found = 0;
      sym_list = NULL;
      /* APPLE LOCAL end return multiple symbols  */

      f = TYPE_FN_FIELDLIST1 (t, method_counter);

      if (TYPE_FN_FIELD_STUB (f, field_counter))
	{
	  char *tmp_name;

	  tmp_name = gdb_mangle_name (t,
				      method_counter,
				      field_counter);
	  phys_name = alloca (strlen (tmp_name) + 1);
	  strcpy (phys_name, tmp_name);
	  xfree (tmp_name);
	}
      else
	phys_name = TYPE_FN_FIELD_PHYSNAME (f, field_counter);
		
      /* Destructor is handled by caller, don't add it to
	 the list.  */
      if (is_destructor_name (phys_name) != 0)
	continue;

      /* APPLE LOCAL: phys_name should really be a mangled name but
	 we may have a method name which is just a source name,
	 e.g.  in the case of a ctor/dtor where the DW_TAG_subprogram
	 DIE doesn't have a linkage name (it will have multiple
	 linkage names in separate DIEs with concrete addresses).
	 So in that case, construct a fully qualified name given
	 the class name and use that for the symbol lookup.  */

      if (phys_name[0] != '_')
        {
          char *demangled_name = alloca (strlen (TYPE_NAME (t)) + 
                                         strlen (phys_name) + 3);
          sprintf (demangled_name, "%s::%s", TYPE_NAME (t), phys_name);
	  /* APPLE LOCAL begin return multiple symbols  */
	  syms_found = lookup_symbol_all (demangled_name, NULL, VAR_DOMAIN,
					  (int *) NULL, 
					  (struct symtab **) NULL,
					  &sym_list);
        }
      else
	syms_found = lookup_symbol_all (phys_name, NULL, VAR_DOMAIN,
					(int *) NULL,
					(struct symtab **) NULL,
					&sym_list);

      if (syms_found)
	{
	  int j;
	  int num_syms = 0;
	  int base_pos = *sym_arr_pos;
	  int new_pos = *sym_arr_pos;
	  struct symbol_search *cur;
	  
	  /* sym_arr may have had some stuff in it when passed into
	     this function. sym_arr_pos indicates the first open
	     position in the array, so we use that as a base_pos
	     (starting position) for filling in our newly found
	     symbols.  new_pos starts at sym_arr_pos and gets
	     incremented every time a new symbol is added to the
	     array, so it indicates the new final value for
	     sym_arr_pos (i.e. it always points at the next open
	     position in the array).  */


	  /* Remove symbols from sym_list that are already in sym_arr  */

	  remove_duplicate_symbols (*sym_arr, *sym_arr_pos,
				    &sym_list);


	  /* Count the number of new symbols we found.  */
	  
	  for (cur = sym_list; cur; cur = cur->next)
	    num_syms++;

	  /* Check to see if there is room left in the array for all 
	     the new symbols.  If not, realloc the array.  */

	  if ((base_pos + num_syms) > *sym_arr_size)
	    {
	      int k;
	      *sym_arr = xrealloc (*sym_arr,
				   (num_syms + base_pos) * sizeof 
				                         (struct symbol *));

	      /* Blank out the new entries.  */

	      for (k = *sym_arr_size; k < num_syms + base_pos; k++)
		(*sym_arr)[k] = NULL;

	      *sym_arr_size = num_syms + base_pos;
	    }

	  /* Walk down the list of new symbols adding each new symbol
	     to the array.  */

	  for (j = 0, cur = sym_list; j < num_syms && cur;
	       j++, cur = cur->next)
	    {
	      (*sym_arr)[j + base_pos] = cur->symbol;
	      new_pos++;
	    }

	  /* Update i1 and sym_arr_pos appropriately.  */

	  i1  += num_syms;
	  *sym_arr_pos = new_pos;
	}
      /* APPLE LOCAL end return multiple symbols  */
      else
	{
	  /* This error message gets printed, but the method
	     still seems to be found
	     fputs_filtered("(Cannot find method ", gdb_stdout);
	     fprintf_symbol_filtered (gdb_stdout, phys_name,
	     language_cplus,
	     DMGL_PARAMS | DMGL_ANSI);
	     fputs_filtered(" - possibly inlined.)\n", gdb_stdout);
	  */
	}
    }

  return i1;
}

/* APPLE LOCAL begin return multiple symbols  */
/* Add the symbols associated to constructors of the class whose type
   is CLASS_TYPE and which are indexed by by METHOD_COUNTER to the
   array SYM_ARR.  Use SYM_ARR_SIZE to decide if we need to resize
   the array; SYM_ARR_POS points to the next open position in the array
   (for adding symbols).  Return the number of methods added.  */

static int
add_constructors (int method_counter, struct type *t,
		  struct symbol ***sym_arr, int *sym_arr_size,
		  int *sym_arr_pos)
{
  int field_counter;
  int i1 = 0;
  int syms_found = 0;
  struct symbol_search *sym_list = NULL;
  /* APPLE LOCAL end return multiple symbols  */

  /* For GCC 3.x and stabs, constructors and destructors
     have names like __base_ctor and __complete_dtor.
     Check the physname for now if we're looking for a
     constructor.  */
  for (field_counter
	 = TYPE_FN_FIELDLIST_LENGTH (t, method_counter) - 1;
       field_counter >= 0;
       --field_counter)
    {
      struct fn_field *f;
      char *phys_name;
		  
      f = TYPE_FN_FIELDLIST1 (t, method_counter);

      /* GCC 3.x will never produce stabs stub methods, so
	 we don't need to handle this case.  */
      if (TYPE_FN_FIELD_STUB (f, field_counter))
	continue;
      phys_name = TYPE_FN_FIELD_PHYSNAME (f, field_counter);
      if (! is_constructor_name (phys_name))
	continue;

      /* If this method is actually defined, include it in the
	 list.  */
      /* APPLE LOCAL begin return multiple symbols  */
      syms_found = lookup_symbol_all (phys_name, NULL,
				      VAR_DOMAIN,
				      (int *) NULL,
				      (struct symtab **) NULL,
				      &sym_list);
      if (syms_found)
	{
	  int j;
	  int num_syms = 0;
	  int base_pos = *sym_arr_pos;
	  int new_pos = *sym_arr_pos;
	  struct symbol_search *cur;

	  /* sym_arr may have had some stuff in it when passed into
	     this function. sym_arr_pos indicates the first open
	     position in the array, so we use that as a base_pos
	     (starting position) for filling in our newly found
	     symbols.  new_pos starts at sym_arr_pos and gets
	     incremented every time a new symbol is added to the
	     array, so it indicates the new final value for
	     sym_arr_pos (i.e. it always points at the next open
	     position in the array).  */


	  /* Remove symbols from sym_list that are already in sym_arr  */

	  remove_duplicate_symbols (*sym_arr, *sym_arr_pos,
				    &sym_list);

	  /* Count the number of new symbols we found.  */
	  
	  for (cur = sym_list; cur; cur = cur->next)
	    num_syms++;

	  /* Check to see if there is room left in the array for all 
	     the new symbols.  If not, realloc the array.  */

	  if ((num_syms + base_pos) > *sym_arr_size)
	    {
	      int k;
	      *sym_arr = xrealloc (*sym_arr,
				   (num_syms + base_pos) * sizeof 
				                          (struct symbol *));
	      /* Blank out the new entries.  */

	      for (k = *sym_arr_size; k < num_syms + base_pos; k++)
		(*sym_arr)[k] = NULL;

	      *sym_arr_size = num_syms + base_pos;
	    }

	  /* Walk down the list of new symbols adding each new symbol
	     to the array.  */

	  i1 = num_syms;
	  for (j = 0, cur = sym_list; j < num_syms && cur;
	       j++, cur = cur->next)
	    {
	      (*sym_arr)[j + base_pos] = cur->symbol;
	      new_pos++;
	    }

	  /* Update sym_arr_pos appropriately.  */

	  *sym_arr_pos = new_pos;
	}
      /* APPLE LOCAL end return multiple symbols  */
    }

  return i1;
}

/* Helper function for decode_line_1.
   Build a canonical line spec in CANONICAL if it is non-NULL and if
   the SAL has a symtab.
   If SYMNAME is non-NULL the canonical line spec is `filename:symname'.
   If SYMNAME is NULL the line number from SAL is used and the canonical
   line spec is `filename:linenum'.  */

static void
build_canonical_line_spec (struct symtab_and_line *sal, char *symname,
			   char ***canonical)
{
  char **canonical_arr;
  char *canonical_name;
  char *filename;
  struct symtab *s = sal->symtab;

  if (s == (struct symtab *) NULL
      || s->filename == (char *) NULL
      || canonical == (char ***) NULL)
    return;

  canonical_arr = (char **) xmalloc (sizeof (char *));
  *canonical = canonical_arr;

  filename = s->filename;
  if (symname != NULL)
    {
      /* APPLE LOCAL: put single quotes around the symbol otherwise we'll
	 fail to parse it correctly if it has parens or "::".  */
      canonical_name = xmalloc (strlen (filename) + strlen (symname) + 2 + 2);
      sprintf (canonical_name, "%s:'%s'", filename, symname);
    }
  else
    {
      canonical_name = xmalloc (strlen (filename) + 30);
      sprintf (canonical_name, "%s:%d", filename, sal->line);
    }
  canonical_arr[0] = canonical_name;
}



/* Find an instance of the character C in the string S that is outside
   of all parenthesis pairs, single-quoted strings, and double-quoted
   strings.  Also, ignore the char within a template name, like a ','
   within foo<int, int>.  */

static char *
find_toplevel_char (char *s, char c)
{
  int quoted = 0;		/* zero if we're not in quotes;
				   '"' if we're in a double-quoted string;
				   '\'' if we're in a single-quoted string.  */
  int depth = 0;		/* Number of unclosed parens we've seen.  */
  char *scan;

  for (scan = s; *scan; scan++)
    {
      if (quoted)
	{
	  if (*scan == quoted)
	    quoted = 0;
	  else if (*scan == '\\' && *(scan + 1))
	    scan++;
	}
      else if (*scan == c && ! quoted && depth == 0)
	return scan;
      else if (*scan == '"' || *scan == '\'')
	quoted = *scan;
      else if (*scan == '(' || *scan == '<')
	depth++;
      else if ((*scan == ')' || *scan == '>') && depth > 0)
	depth--;
    }

  return 0;
}

/* Determines if the gives string corresponds to an Objective-C method
   representation, such as -[Foo bar:] or +[Foo bar]. Objective-C symbols
   are allowed to have spaces and parentheses in them.  */

static int 
is_objc_method_format (const char *s)
{
  if (s == NULL || *s == '\0')
    return 0;
  /* Handle arguments with the format FILENAME:SYMBOL.  */
  if ((s[0] == ':') && (strchr ("+-", s[1]) != NULL) 
      && (s[2] == '[') && strchr(s, ']'))
    return 1;
  /* Handle arguments that are just SYMBOL.  */
  else if ((strchr ("+-", s[0]) != NULL) && (s[1] == '[') && strchr(s, ']'))
    return 1;
  return 0;
}

/* Given a list of NELTS symbols in SYM_ARR, return a list of lines to
   operate on (ask user if necessary).
   If CANONICAL is non-NULL return a corresponding array of mangled names
   as canonical line specs there.  */

/* APPLE LOCAL: Added ACCEPT_ALL meaning don't ask, but just return all the 
   matches.  This is implemented slightly inefficiently - I just mock up a
   return answer of "1".  But this doesn't need to be high performance, and
   it is better that we don't duplicate the code.  This way also reduces merge
   conflicts...  */

/* APPLE LOCAL: Also added the nsyms parameter.  decode_objc returns the total 
   number of elements, AND the number that have symbols.  So we should use this
   info if available.  */

static struct symtabs_and_lines
decode_line_2 (struct symbol *sym_arr[], int nelts, int nsyms, int funfirstline,
	       int accept_all,
	       char ***canonical)
{
  struct symtabs_and_lines values, return_values;
  char *args, *arg1;
  int i;
  char *prompt;
  char *symname;
  struct cleanup *old_chain;
  char **canonical_arr = (char **) NULL;

  values.sals = (struct symtab_and_line *)
    alloca (nelts * sizeof (struct symtab_and_line));
  return_values.sals = (struct symtab_and_line *)
    xmalloc (nelts * sizeof (struct symtab_and_line));
  old_chain = make_cleanup (xfree, return_values.sals);

  if (canonical)
    {
      canonical_arr = (char **) xmalloc (nelts * sizeof (char *));
      make_cleanup (xfree, canonical_arr);
      memset (canonical_arr, 0, nelts * sizeof (char *));
      *canonical = canonical_arr;
    }

  i = 0;
  /* APPLE LOCAL */
  if (!accept_all)
  printf_unfiltered (_("[0] cancel\n[1] all\n"));

  /* APPLE LOCAL */
  while (i < nsyms)
    {
      init_sal (&return_values.sals[i]);	/* Initialize to zeroes.  */
      init_sal (&values.sals[i]);
      if (sym_arr[i] && SYMBOL_CLASS (sym_arr[i]) == LOC_BLOCK)
	{
	  values.sals[i] = find_function_start_sal (sym_arr[i], funfirstline);
	  if (!accept_all)
	    {
	      if (values.sals[i].symtab)
		printf_unfiltered ("[%d] %s at %s:%d\n",
				   (i + 2),
				   SYMBOL_PRINT_NAME (sym_arr[i]),
				   values.sals[i].symtab->filename,
				   values.sals[i].line);
	      else
		printf_unfiltered ("[%d] %s at ?FILE:%d [No symtab? Probably broken debug info...]\n",
				   (i + 2),
				   SYMBOL_PRINT_NAME (sym_arr[i]),
				   values.sals[i].line);
	    }
	}
      else if (!accept_all)
	{
	  /* APPLE LOCAL: We can do a little better than just printing ?HERE?...  */
          printf_filtered ("[%d]    %s\n",
                           (i + 2),
                           (sym_arr[i] && SYMBOL_PRINT_NAME (sym_arr[i])) ?
                           SYMBOL_PRINT_NAME (sym_arr[i]) : "?HERE?");
	}
      else
	printf_unfiltered (_("?HERE\n"));
      i++;
    }

  if (!accept_all && nelts != nsyms)
        printf_filtered ("\nNon-debugging symbols:\n");

  /* handle minimal_symbols */
  for (i = nsyms; i < nelts; i++)
    {
      /* assert (sym_arr[i] != NULL); */
      QUIT;
      values.sals[i].symtab = 0;
      values.sals[i].line = 0;
      values.sals[i].end = 0;
      values.sals[i].entry_type = 0;
      values.sals[i].next = 0;
      values.sals[i].pc = SYMBOL_VALUE_ADDRESS (sym_arr[i]);
      values.sals[i].section = SYMBOL_BFD_SECTION (sym_arr[i]);
      if (funfirstline)
	{
	  /* APPLE LOCAL begin address context.  */
	  /* Check if the current gdbarch supports a safer and more accurate
	     version of prologue skipping that takes an address context.  */
	  if (SKIP_PROLOGUE_ADDR_CTX_P ())
	    {
	      struct address_context sym_addr_ctx;
	      init_address_context (&sym_addr_ctx);
	      sym_addr_ctx.address = values.sals[i].pc;
	      sym_addr_ctx.symbol = sym_arr[i];
	      sym_addr_ctx.bfd_section = SYMBOL_BFD_SECTION (sym_arr[i]);
	      values.sals[i].pc = SKIP_PROLOGUE_ADDR_CTX (&sym_addr_ctx);
	    }
	  else
	    {
	      values.sals[i].pc = SKIP_PROLOGUE (values.sals[i].pc);
	    }
	  /* APPLE LOCAL end address context.  */
	}

      if (!accept_all)
        printf_filtered ("[%d]    %s\n",
                         (i + 2),
                         SYMBOL_PRINT_NAME (sym_arr[i]));
    }

  if (!accept_all)
    {
      prompt = getenv ("PS2");
      if (prompt == NULL)
	{
	  prompt = "> ";
	}
      args = command_line_input (prompt, 0, "overload-choice");
      
      if (args == 0 || *args == 0)
	error_no_arg ("one or more choice numbers");
    }
  else
    args = "1";

  i = 0;
  while (*args)
    {
      int num;

      arg1 = args;
      while (*arg1 >= '0' && *arg1 <= '9')
	arg1++;
      if (*arg1 && *arg1 != ' ' && *arg1 != '\t')
	error (_("Arguments must be choice numbers."));

      num = atoi (args);

      if (num == 0)
	error (_("canceled"));
      else if (num == 1)
	{
	  if (canonical_arr)
	    {
	      for (i = 0; i < nelts; i++)
		{
		  if (canonical_arr[i] == NULL)
		    {
		      symname = DEPRECATED_SYMBOL_NAME (sym_arr[i]);
		      canonical_arr[i] = savestring (symname, strlen (symname));
		    }
		}
	    }
	  memcpy (return_values.sals, values.sals,
		  (nelts * sizeof (struct symtab_and_line)));
	  return_values.nelts = nelts;
	  discard_cleanups (old_chain);
	  return return_values;
	}

      if (num >= nelts + 2)
	{
	  printf_unfiltered (_("No choice number %d.\n"), num);
	}
      else
	{
	  num -= 2;
	  if (values.sals[num].pc)
	    {
	      if (canonical_arr)
		{
		  symname = DEPRECATED_SYMBOL_NAME (sym_arr[num]);
		  make_cleanup (xfree, symname);
		  canonical_arr[i] = savestring (symname, strlen (symname));
		}
	      return_values.sals[i++] = values.sals[num];
	      values.sals[num].pc = 0;
	    }
	  else
	    {
	      printf_unfiltered (_("duplicate request for %d ignored.\n"), num);
	    }
	}

      args = arg1;
      while (*args == ' ' || *args == '\t')
	args++;
    }
  return_values.nelts = i;
  discard_cleanups (old_chain);
  return return_values;
}

/* APPLE LOCAL: Append NUM_SALS members from the array of symtab_and_line 
   structs in SAL to the symtabs_and_lines struct SALS.  */

static void
sals_pushback (struct symtabs_and_lines *sals,
	       struct symtab_and_line *sal,
               unsigned int num_sals)
{
  const unsigned int total_num_sals = sals->nelts + num_sals;
  sals->sals = (struct symtab_and_line *) 
                  xrealloc (sals->sals, 
                            total_num_sals * sizeof (struct symtab_and_line));
  memcpy (sals->sals + sals->nelts, sal, 
          num_sals * sizeof (struct symtab_and_line));
  sals->nelts += num_sals;
}

/* APPLE LOCAL: Only adds the symtab_and_line entries in SRC_SALS to 
   DST_SALS if they are not duplicates of an existing symtab_and_line
   entry in DST_SALS. We can run into this problem when we have a dSYM file 
   whose executable still contains its debug map and the .o files are still
   available.  The other place we run into this is if you have a function that
   is in multiple .o files, but gets coalesced into one function in the
   output.  Then when debug information is still in the .o files, we
   don't want to set multiple breakpoints on the same address.  */

static void
intersect_sals (struct symtabs_and_lines *dst_sals, 
                const struct symtabs_and_lines *src_sals)
{
  if (src_sals->nelts > 0)
    {
      int src_idx;
      int dst_idx;
      /* Iterate through all entries in the SRC_SALS array.  */
      for (src_idx = 0; src_idx < src_sals->nelts; src_idx++)
	{
	  int add_src = 1;
	  struct symtab_and_line *src_sal = &src_sals->sals[src_idx];
	  for (dst_idx = 0; dst_idx < dst_sals->nelts; dst_idx++)
	    {
	      struct symtab_and_line *dst_sal = &dst_sals->sals[dst_idx];
	      
	      /* A duplicate is considered an entry whose address line are the 
                 same and whose objfile is the same as the the others separate
		 debug objfile.  */
	      if (dst_sal->pc == src_sal->pc
		  && dst_sal->line == src_sal->line
		  && dst_sal->symtab 
                  && src_sal->symtab 
		  /* Check that EITHER this sals are coming from an objfile
		     and its dSYM:  */
		  && ((dst_sal->symtab->objfile == 
		       src_sal->symtab->objfile->separate_debug_objfile)
		      /* OR from a coalesced function that shows up in two symtabs:  */
		      || (dst_sal->symtab->objfile == 
			  src_sal->symtab->objfile))
		  /* We should also check that the file & dir are the same... */
 		  && (src_sal->symtab->filename != NULL
		      && dst_sal->symtab->filename != NULL
		      && strcmp (src_sal->symtab->filename, dst_sal->symtab->filename) == 0)
 		  && (src_sal->symtab->dirname != NULL
		      && dst_sal->symtab->dirname != NULL
		      && strcmp (src_sal->symtab->dirname, dst_sal->symtab->dirname) == 0))
		{
		  add_src = 0;
		  break;
		}
	    }
	    
	  if (add_src)
	    {
	      sals_pushback (dst_sals, src_sal, 1);
	    }
	}
    }
}

/* The parser of linespec itself. */

/* Parse a string that specifies a line number.
   Pass the address of a char * variable; that variable will be
   advanced over the characters actually parsed.

   The string can be:

   LINENUM -- that line number in current file.  PC returned is 0.
   FILE:LINENUM -- that line in that file.  PC returned is 0.
   FUNCTION -- line number of openbrace of that function.
   PC returned is the start of the function.
   VARIABLE -- line number of definition of that variable.
   PC returned is 0.
   FILE:FUNCTION -- likewise, but prefer functions in that file.
   *EXPR -- line in which address EXPR appears.

   This may all be followed by an "if EXPR", which we ignore.

   FUNCTION may be an undebuggable function found in minimal symbol table.

   If the argument FUNFIRSTLINE is nonzero, we want the first line
   of real code inside a function when a function is specified, and it is
   not OK to specify a variable or type to get its line number.

   DEFAULT_SYMTAB specifies the file to use if none is specified.
   It defaults to current_source_symtab.
   DEFAULT_LINE specifies the line number to use for relative
   line numbers (that start with signs).  Defaults to current_source_line.
   If CANONICAL is non-NULL, store an array of strings containing the canonical
   line specs there if necessary. Currently overloaded member functions and
   line numbers or static functions without a filename yield a canonical
   line spec. The array and the line spec strings are allocated on the heap,
   it is the callers responsibility to free them.

   Note that it is possible to return zero for the symtab
   if no file is validly specified.  Callers must check that.
   Also, the line number returned may be invalid.  

   If NOT_FOUND_PTR is not null, store a boolean true/false value at the
   location, based on whether or not failure occurs due to an unknown function
   or file.  In the case where failure does occur due to an unknown function
   or file, do not issue an error message.  */

/* We allow single quotes in various places.  This is a hideous
   kludge, which exists because the completer can't yet deal with the
   lack of single quotes.  FIXME: write a linespec_completer which we
   can use as appropriate instead of make_symbol_completion_list.  */

/* APPLE LOCAL begin return multiple symbols:  New parameter
   FIND_ALL_OCCURRENCES.  If set, decode_line_1 calls decode_all_variables,
   rather than decode_variable.  This is mostly for setting breakpoints
   on ALL occurrences of a function with a given name (e.g. multiple
   constructors & destructors).  */
struct symtabs_and_lines
decode_line_1 (char **argptr, int funfirstline, struct symtab *default_symtab,
	       int default_line, char ***canonical, int *not_found_ptr, 
	       int find_all_occurrences)
/* APPLE LOCAL end return multiple symbols  */
{
  char *p;
  char *q;
  /* If a file name is specified, this is its symtab.  */
  struct symtab *file_symtab = NULL;
  struct symtab **file_symtab_arr = NULL;

  char *copy;
  /* This is NULL if there are no parens in *ARGPTR, or a pointer to
     the closing parenthesis if there are parens.  */
  char *paren_pointer;
  /* This says whether or not something in *ARGPTR is quoted with
     completer_quotes (i.e. with single quotes).  */
  int is_quoted;
  /* Is part of *ARGPTR is enclosed in double quotes?  */
  int is_quote_enclosed;
  int is_objc_method = 0;
  char *saved_arg = *argptr;

  if (not_found_ptr)
    *not_found_ptr = 0;

  /* Defaults have defaults.  */

  initialize_defaults (&default_symtab, &default_line);
  
  /* If we get a compound expression of the form: 
        filename.cp:foo::bar(this, that)
     then we'll peal off the filename.cp, and use that to set
     file_symtab_arr, but if we go on from there, we'll have 
     passed the code that deals with the :: in the symbol
     name.  So we come back here again to make sure we pick
     that up.  */

 start_over:
  /* See if arg is *PC.  */

  if (**argptr == '*')
    return decode_indirect (argptr);

  /* Set various flags.  'paren_pointer' is important for overload
     checking, where we allow things like:
        (gdb) break c::f(int)
  */

  set_flags (*argptr, &is_quoted, &paren_pointer);

  /* Check to see if it's a multipart linespec (with colons or
     periods).  */

  /* Locate the end of the first half of the linespec.
     After the call, for instance, if the argptr string is "foo.c:123"
     p will point at "123".  If there is only one part, like "foo", p
     will point to "". If this is a C++ name, like "A::B::foo", p will
     point to "::B::foo". Argptr is not changed by this call.  */

  p = locate_first_half (argptr, &is_quote_enclosed);

  /* Check if this is an Objective-C method (anything that starts with
     a '+' or '-' and a '[').  */
  if (is_objc_method_format (p))
    {
      is_objc_method = 1;
      paren_pointer  = NULL; /* Just a category name.  Ignore it.  */
    }

  /* Check if the symbol could be an Objective-C selector.  */

  if (allow_objc_selectors_flag)
    {
      struct symtabs_and_lines values;
      /* APPLE LOCAL: Don't look for method name matches if
	 it's ``main''.  AppKit has a couple of classes with "main"
	 methods now and this means every time you type "b main"
	 on an ObjC program you get a "Select one of the following"
	 dialogue.  Lame.  And it doesn't look like we can talk the
	 AppKit guys down, so hack it in here.  
         Jeez.  Same thing with "error" now.  Guess what function I put
         a breakpoint on all the time while working on gdb... */
      if (saved_arg == NULL
          || (strcmp (saved_arg, "main") != 0
              && strcmp (saved_arg, "error") != 0))
        {
          values = decode_objc (argptr, funfirstline, NULL,
                                canonical, saved_arg);
          if (values.sals != NULL)
	    return values;
        }
    }

  /* Does it look like there actually were two parts?  */
  
  /* APPLE LOCAL: Ignore parens before ':' or '.', since they might be part
     of the file name and won't be part of a method or symbol name.  */
  /* APPLE LOCAL: The original test here would not go into the symtab_from_filename
     part if you had a paren after the ":".  That breaks parsing something like:
        foo.cp:Class::Method(Type*)
     since we guess the first part is foo.cp:Class, which isn't in fact anything.
     This wouldn't be a big deal except we canonicalize Class::Method(Type*) to 
     just something of this form.
     I didn't want to change the parser too much but it seemed safe to me to test
     for a single quote after the :, which would catch it if we did:
       foo.cp:'Class::Method(Type*)'
     which is actually the right way to do this in gdb.  I also changed the
     canonicalizer so it builds this form.  */

  if ((p[0] == ':' || p[0] == '.') 
      && (paren_pointer == NULL || paren_pointer < p
	  || p[1] == '\''))
    {
      if (is_quoted)
	*argptr = *argptr + 1;
      
      /* Is it a C++ or Java compound data structure?
	 The check on p[1] == ':' is capturing the case of "::",
	 since p[0]==':' was checked above.  
	 Note that the call to decode_compound does everything
	 for us, including the lookup on the symbol table, so we
	 can return now. */
	
      if (p[0] == '.' || p[1] == ':')
	return decode_compound (argptr, funfirstline, canonical,
				saved_arg, p, not_found_ptr);

      /* No, the first part is a filename; set s to be that file's
	 symtab.  Also, move argptr past the filename.  */

      file_symtab_arr = symtab_from_filename (argptr, p, is_quote_enclosed, 
		      			  not_found_ptr);
      if (strchr (p+1, ':') != NULL)
	{
	  goto start_over;
	}
    }
#if 0
  /* No one really seems to know why this was added. It certainly
     breaks the command line, though, whenever the passed
     name is of the form ClassName::Method. This bit of code
     singles out the class name, and if funfirstline is set (for
     example, you are setting a breakpoint at this function),
     you get an error. This did not occur with earlier
     verions, so I am ifdef'ing this out. 3/29/99 */
  else
    {
      /* Check if what we have till now is a symbol name */

      /* We may be looking at a template instantiation such
         as "foo<int>".  Check here whether we know about it,
         instead of falling through to the code below which
         handles ordinary function names, because that code
         doesn't like seeing '<' and '>' in a name -- the
         skip_quoted call doesn't go past them.  So see if we
         can figure it out right now. */

      copy = (char *) alloca (p - *argptr + 1);
      memcpy (copy, *argptr, p - *argptr);
      copy[p - *argptr] = '\000';
      sym = lookup_symbol (copy, 0, VAR_DOMAIN, 0, &sym_symtab);
      if (sym)
	{
	  *argptr = (*p == '\'') ? p + 1 : p;
	  return symbol_found (funfirstline, canonical, copy, sym,
			       NULL, sym_symtab);
	}
      /* Otherwise fall out from here and go to file/line spec
         processing, etc. */
    }
#endif

  /* S is specified file's symtab, or 0 if no file specified.
     arg no longer contains the file name.  */

  /* Check whether arg is all digits (and sign).  */

  q = *argptr;
  if (*q == '-' || *q == '+')
    q++;
  while (*q >= '0' && *q <= '9')
    q++;

  if (q != *argptr && (*q == 0 || *q == ' ' || *q == '\t' || *q == ','))
    {
      /* APPLE LOCAL: Cycle on all the symtabs we have gathered.  */
      /* We found a token consisting of all digits -- at least one digit.  */
      int parsed_lineno = -1;

      if (file_symtab_arr)
      {
        int i;
        struct symtabs_and_lines final_result = {NULL, 0};
        char *start_here;
        for (i = 0; file_symtab_arr[i] != NULL; i++)
          {
            struct symtabs_and_lines this_result;

            start_here = *argptr;

            /* APPLE LOCAL: A symtab from a header include might have types
               but no linetable.  Don't bother looking there.  */

            if (file_symtab_arr[i]->linetable == NULL)
              continue;

            this_result = decode_all_digits_exhaustive (&start_here, 
                              funfirstline, default_symtab, default_line,
			      canonical, file_symtab_arr[i], q, &parsed_lineno,
			      not_found_ptr);
            if (this_result.nelts > 0)
              {
                /* APPLE LOCAL: Only add the sal entries from this_result 
                   if they are not duplicates of ones found in final_result.  */
                intersect_sals (&final_result, &this_result);
              }
	      
	    /* APPLE LOCAL: Be sure to free the sals found in 'this_result'.  */
	    if (this_result.sals)
	      xfree (this_result.sals);
          }
        *argptr = start_here;
	if (final_result.nelts > 1)
	  {
	    /* There's a problem here, if the file that contains
	       templated code contributes SOME code to a given symtab, but not
	       the code at the given breakpoint, we could end up with the line
	       number moved to the next source line that was contributed.  We
	       don't want to do that.  We could go through in each case and
	       try to figure out whether the source file actually contributed
	       code in this case, but that's not easy to do, since we don't
	       know a priori which hit contains the function we want.

	       So I'm going to adopt a cheesier, but simpler heuristic.  I
	       am going to go through the results, and find the line number
	       closest to the given source line, and eliminate all the 
               others.  */

	    int closest_line;
	    int i;
	    int num_matches;

	    closest_line = INT_MAX;
	    for (i = 0; i < final_result.nelts; i++)
	      {
		int this_line = final_result.sals[i].line;
		  /* If we found an exact match, we know there's code
		     at the given line, and we will choose that line.  */
		if (this_line == parsed_lineno)
		  {
		    closest_line = parsed_lineno;
		    break;
		  }
		/* I don't think we ever move the line to lower numbers, 
                   so I will reject those cases as some kind of error.  */
		if (this_line > parsed_lineno 
		    && this_line < closest_line)
		  closest_line = this_line;
	      }

	    num_matches = 0;
	    for (i = 0; i < final_result.nelts; i++)
	      {
		if (final_result.sals[i].line == closest_line)
		  {
		    /* Short-cut the case where we aren't changing
		       anything.  */
		    if (i != num_matches)
		      final_result.sals[num_matches] = final_result.sals[i];
		    num_matches++;
		  }
	      }
	    final_result.nelts = num_matches;
	  }
        if (final_result.nelts == 0)
          {
            final_result.nelts = 0;

            if (file_symtab_arr[0] && file_symtab_arr[0]->filename 
                && file_symtab_arr[0]->linetable == NULL)
              {
                error ("Could not find specified line number in %s, "
                       "no linetable", file_symtab_arr[0]->filename);
              }
            if (parsed_lineno == -1)
              error ("Error parsing file+line number specification or "
                     "could not find source file.");

            /* A file+line specifier like 'objc-prog.m:0' has special meaning 
               in MI; it is used when creating a varobj to indicate a varobj 
               with file-static scope.  So we return a specially constructed 
               sal with just the symtab pointer in that case.  
               If the line number is greater than the number of lines in the
               file, same deal.  */

            final_result.sals = (struct symtab_and_line *) 
              xmalloc (sizeof (struct symtab_and_line));
            memset (final_result.sals, 0, sizeof (struct symtab_and_line));
            final_result.nelts = 1;
            final_result.sals[0].line = parsed_lineno;
            final_result.sals[0].symtab = file_symtab_arr[0];
	  }

        /* We had multiple matching sals but only one (final_result.nelts == 1)
           actually worked out.  Make sure to advance *argptr over the 
           line number specification or we'll get an error about junk at the
           end of the linespec.  */
        *argptr = q;
        return final_result;
      }
      else
	return decode_all_digits (argptr, funfirstline, default_symtab, 
				  default_line, canonical, NULL, q);
    }

  /* APPLE LOCAL: FIXME - should iterate over all the file_symtab's found.  */
  if (file_symtab_arr)
    file_symtab = file_symtab_arr[0];
  else
    file_symtab = NULL;

  /* Arg token is not digits => try it as a variable name
     Find the next token (everything up to end or next whitespace).  */

  if (**argptr == '$')		/* May be a convenience variable.  */
    /* One or two $ chars possible.  */
    p = skip_quoted (*argptr + (((*argptr)[1] == '$') ? 2 : 1));
  else if (is_quoted)
    {
      p = skip_quoted (*argptr);
      if (p[-1] != '\'')
	error (_("Unmatched single quote."));
    }
  else if (is_objc_method)
    {
      /* allow word separators in method names for Obj-C */
      p = skip_quoted_chars (*argptr, NULL, "");
    }
  else if (paren_pointer != NULL)
    {
      p = paren_pointer + 1;
    }
  else
    {
      p = skip_quoted (*argptr);
    }

  copy = (char *) alloca (p - *argptr + 1);
  memcpy (copy, *argptr, p - *argptr);
  copy[p - *argptr] = '\0';
  if (p != *argptr
      && copy[0]
      && copy[0] == copy[p - *argptr - 1]
      && strchr (get_gdb_completer_quote_characters (), copy[0]) != NULL)
    {
      copy[p - *argptr - 1] = '\0';
      copy++;
    }
  while (*p == ' ' || *p == '\t')
    p++;
  *argptr = p;

  /* If it starts with $: may be a legitimate variable or routine name
     (e.g. HP-UX millicode routines such as $$dyncall), or it may
     be history value, or it may be a convenience variable.  */

  if (*copy == '$')
    return decode_dollar (copy, funfirstline, default_symtab,
			  canonical, file_symtab);

  /* Look up that token as a variable.
     If file specified, use that file's per-file block to start with.  */

  /* APPLE LOCAL equivalences */
  /* APPLE LOCAL begin return multiple symbols. decode_all_variables will
     return all the symbols it can find that match "copy".  decode_variable
     only returns the first match it finds.  */
  if (find_all_occurrences)
    return decode_all_variables (copy, funfirstline, !is_quoted, canonical,
				 file_symtab, not_found_ptr);
  else
    return decode_variable (copy, funfirstline, !is_quoted, canonical,
			    file_symtab, not_found_ptr);
  /* APPLE LOCAL end return multiple symbols */
}



/* Now, more helper functions for decode_line_1.  Some conventions
   that these functions follow:

   Decode_line_1 typically passes along some of its arguments or local
   variables to the subfunctions.  It passes the variables by
   reference if they are modified by the subfunction, and by value
   otherwise.

   Some of the functions have side effects that don't arise from
   variables that are passed by reference.  In particular, if a
   function is passed ARGPTR as an argument, it modifies what ARGPTR
   points to; typically, it advances *ARGPTR past whatever substring
   it has just looked at.  (If it doesn't modify *ARGPTR, then the
   function gets passed *ARGPTR instead, which is then called ARG: see
   set_flags, for example.)  Also, functions that return a struct
   symtabs_and_lines may modify CANONICAL, as in the description of
   decode_line_1.

   If a function returns a struct symtabs_and_lines, then that struct
   will immediately make its way up the call chain to be returned by
   decode_line_1.  In particular, all of the functions decode_XXX
   calculate the appropriate struct symtabs_and_lines, under the
   assumption that their argument is of the form XXX.  */

/* First, some functions to initialize stuff at the beggining of the
   function.  */

static void
initialize_defaults (struct symtab **default_symtab, int *default_line)
{
  if (*default_symtab == 0)
    {
      /* Use whatever we have for the default source line.  We don't use
         get_current_or_default_symtab_and_line as it can recurse and call
	 us back! */
      struct symtab_and_line cursal = 
	get_current_source_symtab_and_line ();
      
      *default_symtab = cursal.symtab;
      *default_line = cursal.line;
    }
}

static void
set_flags (char *arg, int *is_quoted, char **paren_pointer)
{
  char *ii;
  int has_if = 0;

  /* 'has_if' is for the syntax:
        (gdb) break foo if (a==b)
  */
  if ((ii = strstr (arg, " if ")) != NULL ||
      (ii = strstr (arg, "\tif ")) != NULL ||
      (ii = strstr (arg, " if\t")) != NULL ||
      (ii = strstr (arg, "\tif\t")) != NULL ||
      (ii = strstr (arg, " if(")) != NULL ||
      (ii = strstr (arg, "\tif( ")) != NULL)
    has_if = 1;
  /* Temporarily zap out "if (condition)" to not confuse the
     parenthesis-checking code below.  This is undone below. Do not
     change ii!!  */
  if (has_if)
    {
      *ii = '\0';
    }

  *is_quoted = (*arg
		&& strchr (get_gdb_completer_quote_characters (),
			   *arg) != NULL);

  *paren_pointer = strchr (arg, '(');
  if (*paren_pointer != NULL)
    *paren_pointer = strrchr (*paren_pointer, ')');

  /* Now that we're safely past the paren_pointer check, put back " if
     (condition)" so outer layers can see it.  */
  if (has_if)
    *ii = ' ';
}



/* Decode arg of the form *PC.  */

static struct symtabs_and_lines
decode_indirect (char **argptr)
{
  struct symtabs_and_lines values;
  CORE_ADDR pc;
  
  (*argptr)++;
  pc = parse_and_eval_address_1 (argptr);

  values.sals = (struct symtab_and_line *)
    xmalloc (sizeof (struct symtab_and_line));

  values.nelts = 1;
  values.sals[0] = find_pc_line (pc, 0);
  values.sals[0].pc = pc;
  values.sals[0].section = find_pc_overlay (pc);
  values.sals[0].entry_type = NORMAL_LT_ENTRY;
  values.sals[0].next = NULL;

  return values;
}



/* Locate the first half of the linespec, ending in a colon, period,
   or whitespace.  (More or less.)  Also, check to see if *ARGPTR is
   enclosed in double quotes; if so, set is_quote_enclosed, advance
   ARGPTR past that and zero out the trailing double quote.
   If ARGPTR is just a simple name like "main", p will point to ""
   at the end.  */

static char *
locate_first_half (char **argptr, int *is_quote_enclosed)
{
  char *ii;
  char *p, *p1;
  int has_comma;

  /* Maybe we were called with a line range FILENAME:LINENUM,FILENAME:LINENUM
     and we must isolate the first half.  Outer layers will call again later
     for the second half.

     Don't count commas that appear in argument lists of overloaded
     functions, or in quoted strings.  It's stupid to go to this much
     trouble when the rest of the function is such an obvious roach hotel.  */
  ii = find_toplevel_char (*argptr, ',');
  has_comma = (ii != 0);

  /* Temporarily zap out second half to not confuse the code below.
     This is undone below. Do not change ii!!  */
  if (has_comma)
    {
      *ii = '\0';
    }

  /* Maybe arg is FILE : LINENUM or FILE : FUNCTION.  May also be
     CLASS::MEMBER, or NAMESPACE::NAME.  Look for ':', but ignore
     inside of <>.  */

  p = *argptr;
  if (p[0] == '"')
    {
      *is_quote_enclosed = 1;
      (*argptr)++;
      p++;
    }
  else
    *is_quote_enclosed = 0;
  for (; *p; p++)
    {
      if (p[0] == '<')
	{
	  char *temp_end = find_template_name_end (p);
	  if (!temp_end)
	    error (_("malformed template specification in command"));
	  p = temp_end;
	}
      /* Check for a colon and a plus or minus and a [ (which
         indicates an Objective-C method) */
      if (is_objc_method_format (p))
	{
	  break;
	}
      /* Check for the end of the first half of the linespec.  End of
         line, a tab, a double colon or the last single colon, or a
         space.  But if enclosed in double quotes we do not break on
         enclosed spaces.  */
      if (!*p
	  || p[0] == '\t'
	  || ((p[0] == ' ') && !*is_quote_enclosed))
	break;

      if (p[0] == ':')
	{
	  if (p[1] == ':') 
	    break;
	  else
	    {
	      char *next_colon = strchr (p + 1, ':');
	      if (next_colon == NULL)
		break;
	      else if (next_colon[1] == ':')
		break;
	    }
	}

      if (p[0] == '.' && strchr (p, ':') == NULL)
	{
	  /* Java qualified method.  Find the *last* '.', since the
	     others are package qualifiers.  */
	  for (p1 = p; *p1; p1++)
	    {
	      if (*p1 == '.')
		p = p1;
	    }
	  break;
	}
    }
  while (p[0] == ' ' || p[0] == '\t')
    p++;

  /* If the closing double quote was left at the end, remove it.  */
  if (*is_quote_enclosed)
    {
      char *closing_quote = strchr (p - 1, '"');
      if (closing_quote && closing_quote[1] == '\0')
	*closing_quote = '\0';
    }

  /* Now that we've safely parsed the first half, put back ',' so
     outer layers can see it.  */
  if (has_comma)
    *ii = ',';

  return p;
}



/* Here's where we recognise an Objective-C Selector.  An Objective C
   selector may be implemented by more than one class, therefore it
   may represent more than one method/function.  This gives us a
   situation somewhat analogous to C++ overloading.  If there's more
   than one method that could represent the selector, then use some of
   the existing C++ code to let the user choose one.  */

struct symtabs_and_lines
decode_objc (char **argptr, int funfirstline, struct symtab *file_symtab,
	     char ***canonical, char *saved_arg)
{
  struct symtabs_and_lines values;
  struct symbol **sym_arr = NULL;
  struct symbol *sym = NULL;
  char *copy = NULL;
  struct block *block = NULL;
  int i1 = 0;
  int i2 = 0;

  values.sals = NULL;
  values.nelts = 0;

  if (file_symtab != NULL)
    block = BLOCKVECTOR_BLOCK (BLOCKVECTOR (file_symtab), STATIC_BLOCK);
  else
    block = get_selected_block (0);
    
  copy = find_imps (file_symtab, block, *argptr, NULL, &i1, &i2); 
    
  if (i1 > 0)
    {
      sym_arr = (struct symbol **) alloca ((i1 + 1) * sizeof (struct symbol *));
      sym_arr[i1] = 0;

      copy = find_imps (file_symtab, block, *argptr, sym_arr, &i1, &i2); 
      *argptr = copy;
    }

  /* i1 now represents the TOTAL number of matches found.
     i2 represents how many HIGH-LEVEL (struct symbol) matches,
     which will come first in the sym_arr array.  Any low-level
     (minimal_symbol) matches will follow those.  */
      
  if (i1 == 1)
    {
      if (i2 > 0)
	{
	  /* Already a struct symbol.  */
	  sym = sym_arr[0];
	}
      else
	{
	  /* APPLE LOCAL: Don't throw away section info if we have it.  */
	  if (SYMBOL_BFD_SECTION (sym_arr[0]) != 0)
	    sym = find_pc_sect_function (SYMBOL_VALUE_ADDRESS (sym_arr[0]),
					 SYMBOL_BFD_SECTION (sym_arr[0]));
	  else
	    sym = find_pc_function (SYMBOL_VALUE_ADDRESS (sym_arr[0]));

	  if ((sym != NULL) && strcmp (SYMBOL_LINKAGE_NAME (sym_arr[0]), SYMBOL_LINKAGE_NAME (sym)) != 0)
	    {
	      warning (_("debugging symbol \"%s\" does not match selector; ignoring"), SYMBOL_LINKAGE_NAME (sym));
	      sym = NULL;
	    }
	}
	      
      values.sals = (struct symtab_and_line *) xmalloc (sizeof (struct symtab_and_line));
      values.nelts = 1;
	      
      if (sym && SYMBOL_CLASS (sym) == LOC_BLOCK)
	{
	  /* Canonicalize this, so it remains resolved for dylib loads.  */
	  values.sals[0] = find_function_start_sal (sym, funfirstline);
	  build_canonical_line_spec (values.sals, SYMBOL_NATURAL_NAME (sym), canonical);
	}
      else
	{
	  /* The only match was a non-debuggable symbol.  */
	  values.sals[0].symtab = 0;
	  values.sals[0].line = 0;
	  values.sals[0].end = 0;
	  values.sals[0].pc = SYMBOL_VALUE_ADDRESS (sym_arr[0]);
          values.sals[0].entry_type = NORMAL_LT_ENTRY;
          values.sals[0].next = NULL;
          if (funfirstline)
	    {
	      /* APPLE LOCAL begin address context.  */
	      /* Check if the current gdbarch supports a safer and more accurate
		 version of prologue skipping that takes an address context.  */
	      if (SKIP_PROLOGUE_ADDR_CTX_P ())
		{
		  struct address_context sym_addr_ctx;
		  init_address_context (&sym_addr_ctx);
		  sym_addr_ctx.address = values.sals[0].pc;
		  sym_addr_ctx.symbol = sym;
		  sym_addr_ctx.bfd_section = SYMBOL_BFD_SECTION (sym_arr[0]);
		  values.sals[0].pc = SKIP_PROLOGUE_ADDR_CTX (&sym_addr_ctx);
		}
	      else
		{
		  values.sals[0].pc = SKIP_PROLOGUE (values.sals[0].pc);
		}
	      /* APPLE LOCAL end address context.  */
	    }
	    
	  values.sals[0].section = SYMBOL_BFD_SECTION (sym_arr[0]);
	}
      return values;
    }

  if (i1 > 1)
    {
      int accept_all;
      if (ui_out_is_mi_like_p (uiout))
	accept_all = 1;
      else
	accept_all = 0;

      /* More than one match. The user must choose one or more.  */
      return decode_line_2 (sym_arr, i1, i2, funfirstline, accept_all, canonical);
    }

  return values;
}

/* This handles C++ and Java compound data structures.  P should point
   at the first component separator, i.e. double-colon or period.  As
   an example, on entrance to this function we could have ARGPTR
   pointing to "AAA::inA::fun" and P pointing to "::inA::fun".  */

static struct symtabs_and_lines
decode_compound (char **argptr, int funfirstline, char ***canonical,
		 char *saved_arg, char *p, int *not_found_ptr)
{
  char *p2;
  char *saved_arg2 = *argptr;
  char *temp_end;
  /* The symtab that SYM was found in.  */
  struct symtab *sym_symtab;
  char *copy;
  struct symbol *sym_class;
  struct type *t;

  /* First check for "global" namespace specification, of the form
     "::foo".  If found, skip over the colons and jump to normal
     symbol processing.  I.e. the whole line specification starts with
     "::" (note the condition that *argptr == p). */
  if (p[0] == ':' 
      && ((*argptr == p) || (p[-1] == ' ') || (p[-1] == '\t')))
    saved_arg2 += 2;

  /* Given our example "AAA::inA::fun", we have two cases to consider:

     1) AAA::inA is the name of a class.  In that case, presumably it
        has a method called "fun"; we then look up that method using
        find_method.

     2) AAA::inA isn't the name of a class.  In that case, either the
        user made a typo or AAA::inA is the name of a namespace.
        Either way, we just look up AAA::inA::fun with lookup_symbol.

     Thus, our first task is to find everything before the last set of
     double-colons and figure out if it's the name of a class.  So we
     first loop through all of the double-colons.  */

  p2 = p;		/* Save for restart.  */

  /* This is very messy. Following the example above we have now the
     following pointers:
     p -> "::inA::fun"
     argptr -> "AAA::inA::fun
     saved_arg -> "AAA::inA::fun
     saved_arg2 -> "AAA::inA::fun
     p2 -> "::inA::fun". */

  /* In the loop below, with these strings, we'll make 2 passes, each
     is marked in comments.*/

  while (1)
    {
      /* Move pointer up to next possible class/namespace token.  */

      p = p2 + 1;	/* Restart with old value +1.  */

      /* PASS1: at this point p2->"::inA::fun", so p->":inA::fun",
	 i.e. if there is a double-colon, p will now point to the
	 second colon. */
      /* PASS2: p2->"::fun", p->":fun" */

      /* Move pointer ahead to next double-colon.  */
      while (*p && (p[0] != ' ') && (p[0] != '\t') && (p[0] != '\''))
	{
	  if (p[0] == '<')
	    {
	      temp_end = find_template_name_end (p);
	      if (!temp_end)
		error (_("malformed template specification in command"));
	      p = temp_end;
	    }
	  /* Note that, since, at the start of this loop, p would be
	     pointing to the second colon in a double-colon, we only
	     satisfy the condition below if there is another
	     double-colon to the right (after). I.e. there is another
	     component that can be a class or a namespace. I.e, if at
	     the beginning of this loop (PASS1), we had
	     p->":inA::fun", we'll trigger this when p has been
	     advanced to point to "::fun".  */
	  /* PASS2: we will not trigger this. */
	  else if ((p[0] == ':') && (p[1] == ':'))
	    break;	/* Found double-colon.  */
	  else
	    /* PASS2: We'll keep getting here, until p->"", at which point
	       we exit this loop.  */
	    p++;
	}

      if (*p != ':')
	break;		/* Out of the while (1).  This would happen
			   for instance if we have looked up
			   unsuccessfully all the components of the
			   string, and p->""(PASS2)  */

      /* We get here if p points to ' ', '\t', '\'', "::" or ""(i.e
	 string ended). */
      /* Save restart for next time around.  */
      p2 = p;
      /* Restore argptr as it was on entry to this function.  */
      *argptr = saved_arg2;
      /* PASS1: at this point p->"::fun" argptr->"AAA::inA::fun",
	 p2->"::fun".  */

      /* All ready for next pass through the loop.  */
    }			/* while (1) */


  /* Start of lookup in the symbol tables. */

  /* Lookup in the symbol table the substring between argptr and
     p. Note, this call changes the value of argptr.  */
  /* Before the call, argptr->"AAA::inA::fun",
     p->"", p2->"::fun".  After the call: argptr->"fun", p, p2
     unchanged.  */
  sym_class = lookup_prefix_sym (argptr, p2);

  /* If sym_class has been found, and if "AAA::inA" is a class, then
     we're in case 1 above.  So we look up "fun" as a method of that
     class.  */
  if (sym_class &&
      (t = check_typedef (SYMBOL_TYPE (sym_class)),
       (TYPE_CODE (t) == TYPE_CODE_STRUCT
	|| TYPE_CODE (t) == TYPE_CODE_UNION)))
    {
      /* Arg token is not digits => try it as a function name.
	 Find the next token (everything up to end or next
	 blank).  */
      if (**argptr
	  && strchr (get_gdb_completer_quote_characters (),
		     **argptr) != NULL)
	{
	  p = skip_quoted (*argptr);
	  *argptr = *argptr + 1;
	}
      else
	{
	  /* At this point argptr->"fun".  */
	  p = *argptr;
	  while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != ':')
	    p++;
	  /* At this point p->"".  String ended.  */
	}

      /* Allocate our own copy of the substring between argptr and
	 p. */
      copy = (char *) alloca (p - *argptr + 1);
      memcpy (copy, *argptr, p - *argptr);
      copy[p - *argptr] = '\0';
      if (p != *argptr
	  && copy[p - *argptr - 1]
	  && strchr (get_gdb_completer_quote_characters (),
		     copy[p - *argptr - 1]) != NULL)
	copy[p - *argptr - 1] = '\0';

      /* At this point copy->"fun", p->"" */

      /* No line number may be specified.  */
      while (*p == ' ' || *p == '\t')
	p++;
      *argptr = p;
      /* At this point arptr->"".  */

      /* Look for copy as a method of sym_class. */
      /* At this point copy->"fun", sym_class is "AAA:inA",
	 saved_arg->"AAA::inA::fun".  This concludes the scanning of
	 the string for possible components matches.  If we find it
	 here, we return. If not, and we are at the and of the string,
	 we'll lookup the whole string in the symbol tables.  */

      return find_method (funfirstline, canonical, saved_arg,
			  copy, t, sym_class, not_found_ptr);

    } /* End if symbol found */


  /* We couldn't find a class, so we're in case 2 above.  We check the
     entire name as a symbol instead.  */

  copy = (char *) alloca (p - saved_arg2 + 1);
  memcpy (copy, saved_arg2, p - saved_arg2);
  /* Note: if is_quoted should be true, we snuff out quote here
     anyway.  */
  copy[p - saved_arg2] = '\000';
  /* Set argptr to skip over the name.  */
  *argptr = (*p == '\'') ? p + 1 : p;

  /* Look up entire name */
  /* APPLE LOCAL begin return multiple symbols  */
  {
    int syms_found = 0;
    struct symbol_search *sym_list = NULL;

    syms_found = lookup_symbol_all (copy, 0, VAR_DOMAIN, 0, &sym_symtab,
				    &sym_list);
    if (syms_found)
      return symbols_found (funfirstline, canonical, copy, sym_list,
			    sym_symtab);
  }
  /* APPLE LOCAL end return multiple symbols  */

  /* Couldn't find any interpretation as classes/namespaces, so give
     up.  The quotes are important if copy is empty.  */
  /* APPLE LOCAL: Need to set the not_found_ptr or future-break
     won't work.  */
  if (not_found_ptr)
    *not_found_ptr = 1;
  cplusplus_error (saved_arg,
		   "Can't find member of namespace, class, struct, or union named \"%s\"\n",
		   copy);
}

/* Next come some helper functions for decode_compound.  */

/* Return the symbol corresponding to the substring of *ARGPTR ending
   at P, allowing whitespace.  Also, advance *ARGPTR past the symbol
   name in question, the compound object separator ("::" or "."), and
   whitespace.  Note that *ARGPTR is changed whether or not the
   lookup_symbol call finds anything (i.e we return NULL).  As an
   example, say ARGPTR is "AAA::inA::fun" and P is "::inA::fun".  */

static struct symbol *
lookup_prefix_sym (char **argptr, char *p)
{
  char *p1;
  char *copy;

  /* Extract the class name.  */
  p1 = p;
  while (p != *argptr && p[-1] == ' ')
    --p;
  copy = (char *) alloca (p - *argptr + 1);
  memcpy (copy, *argptr, p - *argptr);
  copy[p - *argptr] = 0;

  /* Discard the class name from the argptr.  */
  p = p1 + (p1[0] == ':' ? 2 : 1);
  while (*p == ' ' || *p == '\t')
    p++;
  *argptr = p;

  /* At this point p1->"::inA::fun", p->"inA::fun" copy->"AAA",
     argptr->"inA::fun" */

  return lookup_symbol (copy, 0, STRUCT_DOMAIN, 0,
			(struct symtab **) NULL);
}

/* This finds the method COPY in the class whose type is T and whose
   symbol is SYM_CLASS.  */

static struct symtabs_and_lines
find_method (int funfirstline, char ***canonical, char *saved_arg,
	     char *copy, struct type *t, struct symbol *sym_class,
	     int *not_found_ptr)
{
  struct symtabs_and_lines values;
  struct symbol *sym = 0;
  int i1;	/*  Counter for the symbol array.  */
  /* APPLE LOCAL begin return multiple symbols  */
  int sym_arr_size = total_number_of_methods (t);
  struct symbol **sym_arr =  xcalloc (sym_arr_size,
				      sizeof (struct symbol *));
  /* APPLE LOCAL end return multiple symbols  */

  /* Find all methods with a matching name, and put them in
     sym_arr.  */

  /* APPLE LOCAL return multiple symbols  */
  i1 = collect_methods (copy, t, &sym_arr, &sym_arr_size);

  if (i1 == 1)
    {
      /* There is exactly one field with that name.  */
      sym = sym_arr[0];

      if (sym && SYMBOL_CLASS (sym) == LOC_BLOCK)
	{
	  values.sals = (struct symtab_and_line *)
	    xmalloc (sizeof (struct symtab_and_line));
	  values.nelts = 1;
	  values.sals[0] = find_function_start_sal (sym,
						    funfirstline);
	}
      else
	{
	  values.nelts = 0;
	}
      /* APPLE LOCAL return multiple symbols  */
      xfree (sym_arr);
      return values;
    }
  if (i1 > 0)
    {
      int accept_all;
      if (ui_out_is_mi_like_p (uiout))
	accept_all = 1;
      else
	accept_all = 0;

      /* There is more than one field with that name
	 (overloaded).  Ask the user which one to use.  */
      /* APPLE LOCAL begin return multiple values  */
      values = decode_line_2 (sym_arr, i1, i1, funfirstline, accept_all, 
			      canonical);
      xfree (sym_arr);
      return values;
      /* APPLE LOCAL end return multiple values  */
    }
  else
    {
      char *tmp;

      if (is_operator_name (copy))
	{
	  tmp = (char *) alloca (strlen (copy + 3) + 9);
	  strcpy (tmp, "operator ");
	  strcat (tmp, copy + 3);
	}
      else
	tmp = copy;

      /* APPLE LOCAL: Need to set the not_found_ptr so future break will
	 take another crack at this.  */
      if (not_found_ptr)
	*not_found_ptr = 1;

      /* APPLE LOCAL return multiple symbols  */
      xfree (sym_arr);

      if (tmp[0] == '~')
	cplusplus_error (saved_arg,
			 "the class `%s' does not have destructor defined\n",
			 SYMBOL_PRINT_NAME (sym_class));
      else
	cplusplus_error (saved_arg,
			 "the class %s does not have any method named %s\n",
			 SYMBOL_PRINT_NAME (sym_class), tmp);
    }
}

/* APPLE LOCAL begin return multiple symbols  */
/* Find all methods named COPY in the class whose type is T, and put
   them in SYM_ARR.  Use SYM_ARR_SIZE to determine if SYM_ARR needs to
   be resized or not.  Return the number of methods found.  */

static int
collect_methods (char *copy, struct type *t,
		 struct symbol ***sym_arr, int *sym_arr_size)
{
  int i1 = 0;	/*  Counter for the symbol array.  */
  int arr_pos = 0;
  int syms_found = 0;
  struct symbol_search *sym_list = NULL;
  /* APPLE LOCAL end return multiple symbols  */

  if (destructor_name_p (copy, t))
    {
      /* Destructors are a special case.  */
      int m_index, f_index;

      if (get_destructor_fn_field (t, &m_index, &f_index))
	{
	  struct fn_field *f = TYPE_FN_FIELDLIST1 (t, m_index);
	  /* APPLE LOCAL: More fallout from the fact that the DWARF
	     doesn't have the mangled name, so the PHYSNAME is the
	     bare ctor or dtor.  */
	  
	  char *phys_name = TYPE_FN_FIELD_PHYSNAME (f, f_index);
	  
	  if (phys_name[0] != '_')
	    {
	      char *demangled_name = alloca (strlen (TYPE_NAME (t)) + 
					     strlen (phys_name) + 3);
	      sprintf (demangled_name, "%s::%s", TYPE_NAME (t), phys_name);
	      /* APPLE LOCAL begin return multiple symbols  */
	      syms_found = lookup_symbol_all (demangled_name, NULL, VAR_DOMAIN, 
					      (int *) NULL,
					      (struct symtab **) NULL, 
					      &sym_list);

	    }
	  else
	    syms_found = lookup_symbol_all (phys_name, NULL, VAR_DOMAIN,
					    (int *) NULL,
					    (struct symtab **) NULL,
					    &sym_list);

	  if (syms_found)
	    {
	      int j;
	      int num_syms = 0;
	      struct symbol_search *cur;

	      /* Remove symbols from sym_list that are already in sym_arr  */

	      remove_duplicate_symbols (*sym_arr, arr_pos, &sym_list);

	      /* Count the number of new symbols we found.  */

	      for (cur = sym_list; cur; cur = cur->next)
		num_syms++;

	      /* Check to see if the new symbols will fit into the
		 array.  If not, resize the array.  */

	      if (num_syms > *sym_arr_size)
		{
		  int k;

		  *sym_arr = xrealloc (*sym_arr,
				       num_syms * sizeof (struct symbol *));

		  /* Blank out the new entries.  */

		  for (k = *sym_arr_size; k < num_syms; k++)
		    (*sym_arr)[k] = NULL;

		  *sym_arr_size = num_syms;
		}

	      /* Walk down the list of new symbols adding them to
		 the array.  */

	      i1 = num_syms;
	      for (j = 0, cur = sym_list; j < num_syms && cur; 
		   j++, cur = cur->next)
		(*sym_arr)[j] = cur->symbol;
	    }
	  /* APPLE LOCAL end return multiple symbols  */
	}
    }
  else
    i1 = find_methods (t, copy, sym_arr, sym_arr_size, &arr_pos);

  return i1;
}



/* Return the symtab associated to the filename given by the substring
   of *ARGPTR ending at P, and advance ARGPTR past that filename.  If
   NOT_FOUND_PTR is not null and the source file is not found, store
   boolean true at the location pointed to and do not issue an
   error message.  */

/* APPLE LOCAL: Allocate an array of symtab *'s and return that.
   This will look through all the objfiles for symtabs that match.  */

static struct symtab **
symtab_from_filename (char **argptr, char *p, int is_quote_enclosed, 
		      int *not_found_ptr)
{
  char *p1;
  char *copy;
  struct symtab **file_symtab_arr;
  
  p1 = p;
  while (p != *argptr && p[-1] == ' ')
    --p;
  if ((*p == '"') && is_quote_enclosed)
    --p;
  copy = (char *) alloca (p - *argptr + 1);
  memcpy (copy, *argptr, p - *argptr);
  /* It may have the ending quote right after the file name.  */
  if (is_quote_enclosed && copy[p - *argptr - 1] == '"')
    copy[p - *argptr - 1] = 0;
  else
    copy[p - *argptr] = 0;

  /* Find that file's data.  */
  file_symtab_arr = lookup_symtab_all (copy);
  if (file_symtab_arr == 0)
    {
      /* APPLE LOCAL: Set the not_found_ptr before throwing an error.  
         Also return NOT_FOUND_ERROR, not generic error because the
         code that is calling us is expecting that...  */
      if (not_found_ptr)
	*not_found_ptr = 1;
      if (!have_full_symbols () && !have_partial_symbols ())
	throw_error (NOT_FOUND_ERROR, 
		     _("No symbol table is loaded.  Use the \"file\" command."));
      throw_error (NOT_FOUND_ERROR, _("No source file named %s."), copy);
    }

  /* Discard the file name from the arg.  */
  p = p1 + 1;
  while (*p == ' ' || *p == '\t')
    p++;
  *argptr = p;

  return file_symtab_arr;
}


/* APPLE LOCAL begin inlined function symbols & blocks  */
/* Given a blockvector and two indices into the block vector, this
   function determines if one of the blocks contains the other (perhaps
   several superblocks out).  */

static int
one_block_contains_other (struct blockvector *bv, int index1, int index2)
{
  struct block *first_block = BLOCKVECTOR_BLOCK (bv, index1);
  struct block *second_block = BLOCKVECTOR_BLOCK (bv, index2);
  struct block *superblock;
  struct block *inner_block;
  struct block *outer_block;
  int found = 0;
  int done = 0;

  if (!first_block || !second_block)
    return 0;

  /* Determine which block *might* be the inner vs. the outer one.  */

  if (first_block->startaddr < second_block->startaddr)
    {
      outer_block = first_block;
      inner_block = second_block;
    }
  else if (second_block->startaddr < first_block->startaddr)
    {
      outer_block = second_block;
      inner_block = first_block;
    }
  else if (first_block->endaddr > second_block->endaddr)
    {
      outer_block = first_block;
      inner_block = second_block;
    }
  else if (second_block->endaddr > first_block->endaddr)
    {
      outer_block = second_block;
      inner_block = first_block;
    }
  else
    return 0;
  
  superblock = inner_block->superblock;

  if (!superblock)
    return 0;

  /* Keep moving out to the next superblock (from the inner block)
     until either the outer block is matched, the outer block is
     bypassed, or there are no more superblocks.  */

  while (superblock && !done)
    {
      if (superblock == outer_block)
	{
	  found = 1;
	  done = 1;
	}
      else if (superblock->startaddr < outer_block->startaddr
	       || superblock->endaddr > outer_block->endaddr
	       || superblock->superblock == NULL)
	done = 1;
      else
	superblock = superblock->superblock;
    }

  return found;
}
/* APPLE LOCAL end inlined function symbols & blocks  */


/* APPLE LOCAL: This version of decode_all_digits was largely
   rewritten to handle searching for multiple occurrances of the
   same linenumber in a given symtab.

   This decodes a line where the argument is all digits (possibly
   preceded by a sign).  Q should point to the end of those digits;
   the other arguments are as usual.  */

static struct symtabs_and_lines
decode_all_digits_exhaustive (char **argptr, int funfirstline,
			      struct symtab *default_symtab,
			      int default_line, char ***canonical,
			      struct symtab *file_symtab, char *q,
			      int *parsed_lineno,
			      int *not_found_ptr)

{
  struct symtabs_and_lines values;
  int nvalues_allocated;
  int lineno;

  enum sign
    {
      none, plus, minus
    }
  sign = none;

  /* We might need a canonical line spec if no file was specified.  */
  int need_canonical = (file_symtab == 0) ? 1 : 0;

  /* This is where we need to make sure that we have good defaults.
     We must guarantee that this section of code is never executed
     when we are called with just a function name, since
     set_default_source_symtab_and_line uses
     select_source_symtab that calls us with such an argument.  */

  if (file_symtab == 0 && default_symtab == 0)
    {
      /* Make sure we have at least a default source file.  */
      set_default_source_symtab_and_line ();
      initialize_defaults (&default_symtab, &default_line);
    }

  if (**argptr == '+')
    sign = plus, (*argptr)++;
  else if (**argptr == '-')
    sign = minus, (*argptr)++;
  lineno = atoi (*argptr);
  switch (sign)
    {
    case plus:
      if (q == *argptr)
	lineno = 5;
      if (file_symtab == 0)
	lineno = default_line + lineno;
      break;
    case minus:
      if (q == *argptr)
	lineno = 15;
      if (file_symtab == 0)
	lineno = default_line - lineno;
      else
	lineno = 1;
      break;
    case none:
      break;		/* No need to adjust lineno.  */
    }

  *parsed_lineno = lineno;

  values.nelts = 0;
  values.sals = 
    (struct symtab_and_line *) xmalloc (sizeof (struct symtab_and_line));
  nvalues_allocated = 1;

  /* Initialize the first sal with 0x0 addresses so we don't take random
     memory junk as valid start/end addrs.  */

  values.sals[0].pc = 0;
  values.sals[0].end = 0;

  while (*q == ' ' || *q == '\t')
    q++;
  *argptr = q;
  if (file_symtab == 0)
    file_symtab = default_symtab;

  /* Now we have to scan the file_symtab and see if we how many 
     addresses we can find that all share this file & line number.  */

  {
    struct linetable *l;
    struct blockvector *cur_blockvector, *new_blockvector;
    int cur_index, new_index;
    /* APPLE LOCAL inlined function symbols & blocks  */
    int cur_inlined_call_site = 0;    /* For inlined subroutine CALL SITE  */
    int exact = 0;
    int best = 0;
    int i;

    l = LINETABLE (file_symtab);
    
  do_with_best:
    cur_blockvector = NULL;
    cur_index = -2;

    /* In the line table line 0 indicates an artifical entry, e.g.
       the final address of text for this symtab, and we don't want to
       match one of these when we get a LINENO of 0 passed in.  */

    if (lineno != 0)
      {
        for (i = 0; i < l->nitems; i++)
          {
            struct linetable_entry *item = &(l->item[i]);
	    /* APPLE LOCAL radar 6557594  */
	    int inlined_entry = 0;  /* For actual inlined subroutine  */
            
            if (item->line == lineno)
              {
                /* We found a match, but we don't want to keep setting new
                   breakpoints on the line entries for assembly code all 
                   coming from the same source line (due to scheduling).
                   Our heuristic is that if the block hasn't changed, then
                   we won't set a new breakpoint.  That's not 100% sure,
                   but it's the best I can think to do right now.  */
                exact = 1;

		/* APPLE LOCAL begin Check for potential inlining . */
		if (item->entry_type == INLINED_CALL_SITE_LT_ENTRY)
		  cur_inlined_call_site = 1;
		/* APPLE LOCAL begin radar 6557594  */
		else if (item->entry_type == INLINED_SUBROUTINE_LT_ENTRY)
		  inlined_entry = 1;
		/* APPLE LOCAL end radar 6557594  */
		else
		  {
		    int j = i + 1;

		    /* Peek ahead at the next few (if any) line table entries
		       with the same line number to see if any of them are for
		       inlined call sites or subroutines.  */

		    while (j < l->nitems 
			   && (l->item[j].line == lineno)
			   && !cur_inlined_call_site)
		      {
			if (l->item[j].entry_type == INLINED_CALL_SITE_LT_ENTRY)
			  cur_inlined_call_site = 1;
			/* APPLE LOCAL begin radar 6557594  */
			if (l->item[j].entry_type == INLINED_SUBROUTINE_LT_ENTRY)
			  inlined_entry = 1;
			/* APPLE LOCAL end radar 6557594  */
			j++;
		      }
		  }
		/* APPLE LOCAL end Check for potential inlining . */

                new_blockvector = blockvector_for_pc_sect (item->pc, NULL, 
                                                           &new_index, file_symtab);

		/* APPLE LOCAL begin inlined function symbols & blocks  */

                if (new_blockvector == NULL)
                  continue;
                if (new_blockvector == cur_blockvector && new_index == cur_index)
                  continue;
		else if (new_blockvector == cur_blockvector
			 && new_blockvector
			 && cur_inlined_call_site
			 && one_block_contains_other (new_blockvector, new_index,
						      cur_index))
		  continue;
		/* APPLE LOCAL end inlined function symbols & blocks  */
                else
                  {
                    struct symtab_and_line *sal;
                    exact = 1;
                    cur_blockvector = new_blockvector;
                    cur_index = new_index;
                    if (values.nelts == nvalues_allocated)
                      {
                        nvalues_allocated *= 2;
                        values.sals = (struct symtab_and_line *)
                          xrealloc (values.sals, nvalues_allocated * sizeof (struct symtab_and_line));
                      }
                    sal = &(values.sals[values.nelts++]);
                    init_sal (sal);
                    sal->line = lineno;
                    sal->pc = item->pc;
                    sal->symtab = file_symtab;
		    /* APPLE LOCAL begin radar 6557594  */
		    if (inlined_entry)
		      sal->entry_type = INLINED_SUBROUTINE_LT_ENTRY;
		    /* APPLE LOCAL end radar 6557594  */
                  }
              }

            if (!exact && item->line > lineno && (best == 0 || item->line < best))
              {
                best = item->line;
              }
          }
      }

    /* If you didn't find an exact match - for instance somebody gave
       you a line that's in the middle of a statement, then you will
       have only found ONE instance of the real start of the line by
       the algorithm above.  So we need to set LINENO to the best
       match, and start over looking for all instances of that.  */

    if (!exact)
      {
	if (best > 0)
	  {
	    lineno = best;
	    goto do_with_best;
	  }
	else
	  {
	    /* We didn't find ANY match here.  What should we
	       do?  */
	  }
      }

    /* Now go throught he SALs that we found and adjust the pc if
       funfirstline is set.  */
    if (funfirstline)
      {
	int i;
	for (i = 0; i < values.nelts; i++)
	  {
	    struct symtab_and_line *val = &(values.sals[i]);
	    struct symbol *func_sym;
	    
	    /* If we have an objfile for the pc, then be careful to only
	       look in that objfile for the function symbol.  This is
	       important because if you are running gdb on a program
	       BEFORE it has been launched, the shared libraries might
	       overlay each other, in which case we find_pc_function may
	       return a function from another of these libraries.  That
	       might fool us into moving the breakpoint over the
	       prologue of this function, which is now totally in the
	       wrong place...  */
	    
	    if (val->symtab && val->symtab->objfile)
            {
              struct cleanup *restrict_cleanup;
              restrict_cleanup =
                make_cleanup_restrict_to_objfile
                (val->symtab->objfile);
              func_sym = find_pc_function (val->pc);
              do_cleanups (restrict_cleanup);
            }
          else
            {
              func_sym = find_pc_function (val->pc);
            }
          if (func_sym)
            {
	      struct symtab_and_line sal;
	      struct gdb_exception e;
	      /* APPLE LOCAL: If we can't parse the prologue for some reason,
		 make sure the breakpoint gets marked as "future".  */
	      TRY_CATCH (e, RETURN_MASK_ALL)
	      {
		sal = find_function_start_sal (func_sym, 1);
	      }

	      if (e.reason != NO_ERROR)
		{
		  if (not_found_ptr)
		    *not_found_ptr = 1;
		  throw_exception (e);
		}

              /* Don't move the line, just set the pc
                 to the right place. */
	      /* Also, don't move the linenumber if the symtab's
		 are different.  This will happen for inlined functions,
		 and then you don't want to move the pc.  */

              if (val->symtab == sal.symtab 
		  &&val->line <= sal.line)
                val->pc = sal.pc;
            }
	  }
      }
  }

  if (need_canonical && values.nelts > 0)
    build_canonical_line_spec (values.sals, NULL, canonical);
  return values;
}

/* This decodes a line where the argument is all digits (possibly
   preceded by a sign).  Q should point to the end of those digits;
   the other arguments are as usual.  */

static struct symtabs_and_lines
/* APPLE LOCAL begin linespec */
decode_all_digits (char **argptr, int funfirstline,
		   struct symtab *default_symtab,
/* APPLE LOCAL end linespec */
		   int default_line, char ***canonical,
		   struct symtab *file_symtab, char *q)

{
  struct symtabs_and_lines values;
  struct symtab_and_line val;

  enum sign
    {
      none, plus, minus
    }
  sign = none;

  /* We might need a canonical line spec if no file was specified.  */
  int need_canonical = (file_symtab == 0) ? 1 : 0;

  init_sal (&val);

  /* This is where we need to make sure that we have good defaults.
     We must guarantee that this section of code is never executed
     when we are called with just a function name, since
     set_default_source_symtab_and_line uses
     select_source_symtab that calls us with such an argument.  */

  if (file_symtab == 0 && default_symtab == 0)
    {
      /* Make sure we have at least a default source file.  */
      set_default_source_symtab_and_line ();
      initialize_defaults (&default_symtab, &default_line);
    }

  if (**argptr == '+')
    sign = plus, (*argptr)++;
  else if (**argptr == '-')
    sign = minus, (*argptr)++;
  val.line = atoi (*argptr);
  switch (sign)
    {
    case plus:
      if (q == *argptr)
	val.line = 5;
      if (file_symtab == 0)
	val.line = default_line + val.line;
      break;
    case minus:
      if (q == *argptr)
	val.line = 15;
      if (file_symtab == 0)
	val.line = default_line - val.line;
      else
	val.line = 1;
      break;
    case none:
      break;		/* No need to adjust val.line.  */
    }

  while (*q == ' ' || *q == '\t')
    q++;
  *argptr = q;
  if (file_symtab == 0)
    file_symtab = default_symtab;

  /* It is possible that this source file has more than one symtab, 
     and that the new line number specification has moved us from the
     default (in file_symtab) to a new one.  */
  val.symtab = find_line_symtab (file_symtab, val.line, NULL, NULL);
  if (val.symtab == 0)
    val.symtab = file_symtab;

  val.pc = 0;

  /* APPLE LOCAL begin function first line */
  /* If the file:line points to the first line of a function, move it
     past the prologue.  I tried to get the FSF folks to take this
     change but there was no consensus over whether it was the right
     thing to do or not.  But for Xcode, ending up in the middle of
     the prologue is deadly...  */

  if (funfirstline)
    {
      CORE_ADDR pc = 0;

      if (find_line_pc (val.symtab, val.line, &pc))
        {
          struct symbol *func_sym;
          struct symtab_and_line sal;

          /* If we have an objfile for the pc, then be careful to only
             look in that objfile for the function symbol.  This is
             important because if you are running gdb on a program
             BEFORE it has been launched, the shared libraries might
             overlay each other, in which case we find_pc_function may
             return a function from another of these libraries.  That
             might fool us into moving the breakpoint over the
             prologue of this function, which is now totally in the
             wrong place...  */

          if (val.symtab && val.symtab->objfile)
            {
              struct cleanup *restrict_cleanup;
              restrict_cleanup =
                make_cleanup_restrict_to_objfile
                (val.symtab->objfile);
              func_sym = find_pc_function (pc);
              do_cleanups (restrict_cleanup);
            }
          else
            {
              func_sym = find_pc_function (pc);
            }
          if (func_sym)
            {
              sal = find_function_start_sal (func_sym, 1);
              /* Don't move the line, just set the pc
                 to the right place. */
              if (val.line <= sal.line)
                val.pc = sal.pc;
            }
        }
    }
  /* APPLE LOCAL begin function first line */

  values.sals = (struct symtab_and_line *)
    xmalloc (sizeof (struct symtab_and_line));
  values.sals[0] = val;
  values.nelts = 1;
  if (need_canonical)
    build_canonical_line_spec (values.sals, NULL, canonical);
  return values;
}



/* Decode a linespec starting with a dollar sign.  */

static struct symtabs_and_lines
decode_dollar (char *copy, int funfirstline, struct symtab *default_symtab,
	       char ***canonical, struct symtab *file_symtab)
{
  struct value *valx;
  int index = 0;
  int need_canonical = 0;
  struct symtabs_and_lines values;
  struct symtab_and_line val;
  char *p;
  struct symbol *sym;
  /* The symtab that SYM was found in.  */
  struct symtab *sym_symtab;
  struct minimal_symbol *msymbol;

  p = (copy[1] == '$') ? copy + 2 : copy + 1;
  while (*p >= '0' && *p <= '9')
    p++;
  if (!*p)		/* Reached end of token without hitting non-digit.  */
    {
      /* We have a value history reference.  */
      sscanf ((copy[1] == '$') ? copy + 2 : copy + 1, "%d", &index);
      valx = access_value_history ((copy[1] == '$') ? -index : index);
      if (TYPE_CODE (value_type (valx)) != TYPE_CODE_INT)
	error (_("History values used in line specs must have integer values."));
    }
  else
    {
      /* Not all digits -- may be user variable/function or a
	 convenience variable.  */

      /* Look up entire name as a symbol first.  */
      sym = lookup_symbol (copy, 0, VAR_DOMAIN, 0, &sym_symtab);
      file_symtab = (struct symtab *) 0;
      need_canonical = 1;
      /* Symbol was found --> jump to normal symbol processing.  */
      if (sym)
	return symbol_found (funfirstline, canonical, copy, sym,
			     NULL, sym_symtab);

      /* If symbol was not found, look in minimal symbol tables.  */
      msymbol = lookup_minimal_symbol (copy, NULL, NULL);
      /* Min symbol was found --> jump to minsym processing.  */
      if (msymbol)
	/* APPLE LOCAL: We have to pass in canonical as well.  */
	return minsym_found (funfirstline, 0, msymbol, canonical);

      /* Not a user variable or function -- must be convenience variable.  */
      need_canonical = (file_symtab == 0) ? 1 : 0;
      valx = value_of_internalvar (lookup_internalvar (copy + 1));
      if (TYPE_CODE (value_type (valx)) != TYPE_CODE_INT)
	error (_("Convenience variables used in line specs must have integer values."));
    }

  init_sal (&val);

  /* Either history value or convenience value from above, in valx.  */
  val.symtab = file_symtab ? file_symtab : default_symtab;
  val.line = value_as_long (valx);
  val.pc = 0;

  values.sals = (struct symtab_and_line *) xmalloc (sizeof val);
  values.sals[0] = val;
  values.nelts = 1;

  if (need_canonical)
    build_canonical_line_spec (values.sals, NULL, canonical);

  return values;
}



/* Decode a linespec that's a variable.  If FILE_SYMTAB is non-NULL,
   look in that symtab's static variables first.  If NOT_FOUND_PTR
   is not NULL and the function cannot be found, store boolean true
   in the location pointed to and do not issue an error message.  */

static struct symtabs_and_lines
/* APPLE LOCAL equivalences */
decode_variable (char *copy, int funfirstline, int equivalencies, 
                 char ***canonical, struct symtab *file_symtab, 
                 int *not_found_ptr)
{
  struct symbol *sym;
  /* The symtab that SYM was found in.  */
  struct symtab *sym_symtab;

  struct minimal_symbol *msymbol;

  sym = lookup_symbol (copy,
		       (file_symtab
			? BLOCKVECTOR_BLOCK (BLOCKVECTOR (file_symtab),
					     STATIC_BLOCK)
			: get_selected_block (0)),
		       VAR_DOMAIN, 0, &sym_symtab);

  if (sym != NULL)
    return symbol_found (funfirstline, canonical, copy, sym,
			 file_symtab, sym_symtab);

  msymbol = lookup_minimal_symbol (copy, NULL, NULL);

  if (msymbol != NULL)
    /* APPLE LOCAL: We pass in the "canonical" argument.  */
    return minsym_found (funfirstline, equivalencies, msymbol, canonical);

  if (!have_full_symbols () &&
      !have_partial_symbols () && !have_minimal_symbols ())
    /* APPLE LOCAL begin */
    {
      /* This is properly a "file not found" error as well.  */
      if (not_found_ptr)
	*not_found_ptr = 1;
      /* APPLE LOCAL end */
    error (_("No symbol table is loaded.  Use the \"file\" command."));
    /* APPLE LOCAL */
    }

  if (not_found_ptr)
    *not_found_ptr = 1;
  /* APPLE LOCAL more helpful error */
  if (file_symtab == NULL)
  throw_error (NOT_FOUND_ERROR, _("Function \"%s\" not defined."), copy);
  /* APPLE LOCAL begin more helpful error */
  else
    throw_error (NOT_FOUND_ERROR, _("Function \"%s\" not defined in file %s."),
		 copy, file_symtab->filename);
  /* APPLE LOCAL end more helpful error */
}


/* APPLE LOCAL begin return multiple symbols  */
/* Decode a linespec that's a variable.  If FILE_SYMTAB is non-NULL,
   look in that symtab's static variables first.  If NOT_FOUND_PTR
   is not NULL and the function cannot be found, store boolean true
   in the location pointed to and do not issue an error message.
   This differs from decode_variable in that if there are multiple
   occurrences of the variable, it will return the multiple
   occurrences.  This is used to set breakpoints by name.  */

static struct symtabs_and_lines
decode_all_variables (char *copy, int funfirstline, int equivalencies, 
                      char ***canonical, struct symtab *file_symtab, 
                      int *not_found_ptr)
{
  /* The symtab that SYM was found in.  */
  struct symtab *sym_symtab = NULL;
  struct minimal_symbol *msymbol = NULL;
  struct symbol_search *sym_list = NULL;
  /* APPLE LOCAL radar 6366048 search both minsyms & syms for bps.  */
  struct symbol_search *minsym_list = NULL;
  struct symbol_search *outer_current;
  struct symbol_search *cur;
  struct symbol_search *prev;
  int syms_found = 0;
  /* APPLE LOCAL begin radar 6366048 search both minsyms & syms for bps.  */
  struct symtabs_and_lines minsym_sals;
  struct symtabs_and_lines sym_sals;
  struct symtabs_and_lines ret_sals;
  int i;
  int j;
  char **sym_canonical = NULL;
  char **minsym_canonical = NULL;
  char **canonical_arr;
  char *canonical_name;
  /* APPLE LOCAL end radar 6366048 search both minsyms & syms for bps.  */

  syms_found = lookup_symbol_all  
                     (copy, (file_symtab
			    ? BLOCKVECTOR_BLOCK (BLOCKVECTOR (file_symtab),
						 STATIC_BLOCK)
			    : get_selected_block (0)),
		      VAR_DOMAIN, 0, &sym_symtab, &sym_list);
  
  /* APPLE LOCAL begin radar 6366048 search both minsyms & syms for bps.  */
  /* If we are supposed to look in a particular symbol table, then
     we don't want minimal symbols.  */
  if (!file_symtab)
    msymbol = lookup_minimal_symbol_all (copy, NULL, NULL, &minsym_list);
  /* APPLE LOCAL end radar 6366048 search both minsyms & syms for bps.  */

  if (!syms_found && !msymbol)
    {
      if (!have_full_symbols () &&
	  !have_partial_symbols () && !have_minimal_symbols ())
	{
	  /* This is properly a "file not found" error as well.  */
	  if (not_found_ptr)
	    *not_found_ptr = 1;
	  error (_("No symbol table is loaded.  Use the \"file\" command."));
	}
      
      if (not_found_ptr)
	*not_found_ptr = 1;
      if (file_symtab == NULL)
	throw_error (NOT_FOUND_ERROR, _("Function \"%s\" not defined."), copy);
      else
	throw_error (NOT_FOUND_ERROR, _("Function \"%s\" not defined in file %s."),
		     copy, file_symtab->filename);
    }

  /* APPLE LOCAL radar 6366048 search both minsyms & syms for bps.  */
  gdb_assert ((sym_list != NULL) || (minsym_list != NULL));

  /* APPLE LOCAL begin radar 6366048 search both minsyms & syms for bps.  */

  /* If we are supposed to look in a particular symtab, then eliminate 
     all entries that do not match that symtab.  */

  if (file_symtab)
    {
      prev = NULL;
      cur = sym_list;
      while (cur)
	{
	  if (cur->symtab != file_symtab)
	    {
	      if (!prev)
		sym_list = cur->next;
	      else
		prev->next = cur->next;
	    }
	  else
	    prev = cur;
	  cur = cur->next;
	}
    }

  /* Eliminate duplicate entries from sym_list.  Two entries are
     "duplicate" if they point to the same symbol.  */

  for (outer_current = sym_list; 
       outer_current; 
       outer_current = outer_current->next)
    for (prev = outer_current, cur = prev->next; cur; )
      {
	int duplicate = 0;
	if (cur->symbol && cur->symbol == prev->symbol)
	  duplicate = 1;
	else
	  {
	    /* Also check if the symbols have the same block start and
	       end and the same name.  Then they are coming from two .o
	       files, and they've been coalesced into one function.  */
	    struct block *prev_b = SYMBOL_BLOCK_VALUE (prev->symbol);
	    struct block *cur_b = SYMBOL_BLOCK_VALUE (cur->symbol);
	    if (cur_b && prev_b 
		&& cur_b->startaddr == prev_b->startaddr
		&& cur_b->endaddr == prev_b->endaddr
		&& strcmp (SYMBOL_LINKAGE_NAME (cur->symbol), SYMBOL_LINKAGE_NAME (prev->symbol)) == 0)
	      duplicate = 1;
	  }

	if (duplicate)
	  prev->next = cur->next;
	else
	    prev = cur;

	cur = cur->next;
      }

  /* Eliminate duplicate entries from minsym_list.  Two entries are
     "duplicate" if they point to the same msymbol.  */

  for (outer_current = minsym_list; 
       outer_current; 
       outer_current = outer_current->next)
    for (prev = outer_current, cur = prev->next; cur; )
      {
	if (cur->msymbol && cur->msymbol == prev->msymbol)
	  prev->next = cur->next;
	else
	  prev = cur;
	cur = cur->next;
      }

  /* Remove minsyms for which a sym was also found.  This can be determined
     by looking at the block start addres for both.  */

  if (sym_list && minsym_list)
    {
      int found;
      for (outer_current = sym_list; outer_current; 
	   outer_current = outer_current->next)
	{
	  found = 0;
	  prev = NULL;
	  cur = minsym_list;

	  while (cur && !found)
	    {
	      struct block *b = SYMBOL_BLOCK_VALUE (outer_current->symbol);
	      
	      if (b && (b->startaddr == SYMBOL_VALUE_ADDRESS (cur->msymbol)))
		{
		  found = 1;
		  if (!prev)
		    minsym_list = minsym_list->next;
		  else
		    prev->next = cur->next;
		}
	      else
		prev = cur;

	      cur = cur->next;
	    }
	}
    }

  /* Convert symbol lists into sals.  */

  
  if (sym_list)
    sym_sals =  symbols_found (funfirstline, &sym_canonical, copy, sym_list, 
			       file_symtab);
  else
    sym_sals.nelts = 0;

  if (minsym_list)
    minsym_sals =  minsyms_found (funfirstline, equivalencies, minsym_list, 
				  &minsym_canonical);
  else
    minsym_sals.nelts = 0;

  /* Eliminate entries from minsym_list if they are also in sym_list.  */

  if ((minsym_sals.nelts > 0) && (sym_sals.nelts > 0))
    remove_duplicate_sals (&minsym_sals, sym_sals, minsym_canonical);

  /* Combine sals from syms & minsyms, and return.  */

  ret_sals.nelts = minsym_sals.nelts + sym_sals.nelts;
  ret_sals.sals = (struct symtab_and_line *) 
                    xmalloc (ret_sals.nelts * sizeof (struct symtab_and_line));

  for (i = 0; i < minsym_sals.nelts; i++)
    {
      ret_sals.sals[i].symtab     = minsym_sals.sals[i].symtab;
      ret_sals.sals[i].section    = minsym_sals.sals[i].section;
      ret_sals.sals[i].line       = minsym_sals.sals[i].line;
      ret_sals.sals[i].pc         = minsym_sals.sals[i].pc;
      ret_sals.sals[i].end        = minsym_sals.sals[i].end;
      ret_sals.sals[i].entry_type = minsym_sals.sals[i].entry_type;
      ret_sals.sals[i].next       = minsym_sals.sals[i].next;
    }

  for (j = 0; j < sym_sals.nelts; j++)
    {
      ret_sals.sals[i].symtab     = sym_sals.sals[j].symtab;
      ret_sals.sals[i].section    = sym_sals.sals[j].section;
      ret_sals.sals[i].line       = sym_sals.sals[j].line;
      ret_sals.sals[i].pc         = sym_sals.sals[j].pc;
      ret_sals.sals[i].end        = sym_sals.sals[j].end;
      ret_sals.sals[i].entry_type = sym_sals.sals[j].entry_type;
      ret_sals.sals[i].next       = sym_sals.sals[j].next;
      i++;
    }


  /* Fix up "canonical" (to be used for breakpoint addr_string field).  */

  canonical_arr = (char **) xmalloc (ret_sals.nelts * sizeof (char *));

  for (i = 0; i < minsym_sals.nelts; i++)
    {
      if (minsym_canonical != NULL)
	canonical_arr[i] = minsym_canonical[i];
      else
	canonical_arr[i] = NULL;
    }

  for (j = 0; i < ret_sals.nelts; i++, j++)
    {
      if (sym_canonical != NULL)
	canonical_arr[i] = sym_canonical[j];
      else
	canonical_arr[i] = NULL;
    }

  *canonical = canonical_arr;

  return ret_sals;
  /* APPLE LOCAL end radar 6366048 search both minsyms & syms for bps.  */
}
/* APPLE LOCAL end return multiple symbols  */



/* Now come some functions that are called from multiple places within
   decode_line_1.  */


/* APPLE LOCAL begin return multiple symbols  */
/* We've found multiple symbols, in SYM_LIST, to associate with our linespec;
   build a corresponding struct symtabs_and_lines.  */

static struct symtabs_and_lines
symbols_found (int funfirstline, char ***canonical, char *copy,
	       struct symbol_search *sym_list, struct symtab *file_symtab)
{
  struct symbol_search *current;
  struct symtabs_and_lines values;
  char **canonical_arr;
  int num_syms = 0;
  int i;

  for (current = sym_list; current; current = current->next)
    num_syms++;

  values.sals = (struct symtab_and_line *) xmalloc (num_syms * 
						    sizeof (struct symtab_and_line));
  values.nelts = num_syms;

  if (canonical != NULL)
    {
      canonical_arr = (char **) xmalloc (num_syms * sizeof (char *));
      memset (canonical_arr, 0, num_syms * sizeof (char *));
      *canonical = canonical_arr;
    }

  for (current = sym_list, i = 0; current; current = current->next, i++)
    {
      struct symbol *sym = current->symbol;
      
      if (SYMBOL_CLASS (sym) == LOC_BLOCK)
	{
	  values.sals[i] = find_function_start_sal (sym, funfirstline);

          /* APPLE LOCAL: This seems to be happening in a fix & continue
             situation where we get a symbol that should have been obsoleted;
             we get the non-obsolete symtab but the linetable entries are
             all a bad match for the symbol so we return a symtab of null.  
             At the very least, let's not crash in this situation. */
          if (values.sals[i].symtab == 0)
            error (_("Line number not known for symbol \"%s\""), copy);

	  if ((file_symtab == 0) && (canonical != NULL))
	    {
	      struct blockvector *bv = BLOCKVECTOR (values.sals[i].symtab);
	      struct block *b = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
	      if (lookup_block_symbol (b, copy, NULL, VAR_DOMAIN) != NULL)
		{
		  char *canonical_name; 
		  struct symtab *s = values.sals[i].symtab;

		  if (s && s->filename)
		    {
		      if (copy)
			{
			  canonical_name = xmalloc (strlen (s->filename) +
						    strlen (copy) + 4);
			  sprintf (canonical_name, "%s:'%s'", s->filename,
				   copy);
			}
		      else
			{
			  canonical_name = xmalloc (strlen (s->filename) + 30);
			  sprintf (canonical_name, "%s:%d", s->filename,
				   values.sals[i].line);
			}
		      canonical_arr[i] = canonical_name;
		    }
		}
	    }
	}
      else
	{
	  if (funfirstline)
	    error (_("\"%s\" is not a function"), copy);
	  else if (SYMBOL_LINE (sym) != 0)
	    {
	      values.sals[i].symtab = current->symtab;
	      values.sals[i].line = SYMBOL_LINE (sym);
	    }
	  else
	    error (_("Line number not known for symbol \"%s\""), copy);
	}
    } /* for */

  return values;
}
/* APPLE LOCAL end return multiple symbols  */

/* We've found a symbol SYM to associate with our linespec; build a
   corresponding struct symtabs_and_lines.  */

static struct symtabs_and_lines
symbol_found (int funfirstline, char ***canonical, char *copy,
	      struct symbol *sym, struct symtab *file_symtab,
	      struct symtab *sym_symtab)
{
  struct symtabs_and_lines values;
  
  if (SYMBOL_CLASS (sym) == LOC_BLOCK)
    {
      /* Arg is the name of a function */
      values.sals = (struct symtab_and_line *)
	xmalloc (sizeof (struct symtab_and_line));
      values.sals[0] = find_function_start_sal (sym, funfirstline);
      values.nelts = 1;

      /* Don't use the SYMBOL_LINE; if used at all it points to
	 the line containing the parameters or thereabouts, not
	 the first line of code.  */

      /* We might need a canonical line spec if it is a static
	 function.  */
      if (file_symtab == 0)
	{
	  struct blockvector *bv = BLOCKVECTOR (sym_symtab);
	  struct block *b = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
	  if (lookup_block_symbol (b, copy, NULL, VAR_DOMAIN) != NULL)
	    build_canonical_line_spec (values.sals, copy, canonical);
	}
      return values;
    }
  else
    {
      if (funfirstline)
	error (_("\"%s\" is not a function"), copy);
      else if (SYMBOL_LINE (sym) != 0)
	{
	  /* We know its line number.  */
	  values.sals = (struct symtab_and_line *)
	    xmalloc (sizeof (struct symtab_and_line));
	  values.nelts = 1;
	  memset (&values.sals[0], 0, sizeof (values.sals[0]));
	  values.sals[0].symtab = sym_symtab;
	  values.sals[0].line = SYMBOL_LINE (sym);
	  return values;
	}
      else
	/* This can happen if it is compiled with a compiler which doesn't
	   put out line numbers for variables.  */
	/* FIXME: Shouldn't we just set .line and .symtab to zero
	   and return?  For example, "info line foo" could print
	   the address.  */
	error (_("Line number not known for symbol \"%s\""), copy);
    }
}

/* APPLE LOCAL begin return multiple symbols  */
/* We've found a list of minimal symbols SYM_LIST to associate with
   our linespec; build a corresponding struct symtabs_and_lines.  */

static struct symtabs_and_lines
minsyms_found (int funfirstline, int equivalencies,
	       struct symbol_search *sym_list,  char ***canonical)
{
  struct symbol_search *current;
  struct minimal_symbol **equiv_msymbols;
  struct minimal_symbol **pointer;
  struct symtabs_and_lines values;
  struct cleanup *equiv_cleanup;
  int nsymbols = 0;
  int eq_symbols = 0;
  int i;
  int j;

  for (current = sym_list; current; current = current->next)
    nsymbols++;

  equiv_msymbols = NULL;
  if (equivalencies)
    {
      equiv_msymbols = find_equivalent_msymbol (sym_list->msymbol);
      
      if (equiv_msymbols != NULL)
	{
	  for (pointer = equiv_msymbols; *pointer != NULL; eq_symbols++, pointer++)
	    ;
	  equiv_cleanup = make_cleanup (xfree, equiv_msymbols);
	}
      else
	equiv_cleanup = make_cleanup (null_cleanup, NULL);
    }
  else
    equiv_cleanup = make_cleanup (null_cleanup, NULL);

  values.sals = (struct symtab_and_line *)
    xmalloc ((nsymbols + eq_symbols) * sizeof (struct symtab_and_line));

  for (current = sym_list, i = 0; current; current = current->next, i++)
    {
      values.sals[i] = find_pc_sect_line (SYMBOL_VALUE_ADDRESS (current->msymbol),
					  current->msymbol->ginfo.bfd_section, 0);
      values.sals[i].section = SYMBOL_BFD_SECTION (current->msymbol);
      if (funfirstline)
	{
	  values.sals[i].pc += DEPRECATED_FUNCTION_START_OFFSET;

	  /* APPLE LOCAL begin address context.  */
	  /* Check if the current gdbarch supports a safer and more accurate
	     version of prologue skipping that takes an address context.  */
	  if (SKIP_PROLOGUE_ADDR_CTX_P ())
	    {
	      struct address_context sym_addr_ctx;
	      init_address_context (&sym_addr_ctx);
	      sym_addr_ctx.address = values.sals[i].pc;
	      sym_addr_ctx.bfd_section = values.sals[i].section;
	      sym_addr_ctx.sal = values.sals[i];
	      sym_addr_ctx.msymbol = current->msymbol;
	      values.sals[i].pc = SKIP_PROLOGUE_ADDR_CTX (&sym_addr_ctx);
	    }
	  else
	    {
	      values.sals[i].pc = SKIP_PROLOGUE (values.sals[i].pc);
	    }
	  /* APPLE LOCAL end address context.  */
	}
    }

  for (j = 0; j < eq_symbols; j++, i++)
    {
      struct minimal_symbol *msym;
      
      msym = equiv_msymbols[j];
      values.sals[i] = find_pc_sect_line (SYMBOL_VALUE_ADDRESS (msym),
					  msym->ginfo.bfd_section, 0);
      values.sals[i].section = SYMBOL_BFD_SECTION (msym);
      if (funfirstline)
	{
	  values.sals[i].pc += DEPRECATED_FUNCTION_START_OFFSET;
	  /* APPLE LOCAL begin address context.  */
	  /* Check if the current gdbarch supports a safer and more accurate
	     version of prologue skipping that takes an address context.  */
	  if (SKIP_PROLOGUE_ADDR_CTX_P ())
	    {
	      struct address_context sym_addr_ctx;
	      init_address_context (&sym_addr_ctx);
	      sym_addr_ctx.address = values.sals[i].pc;
	      sym_addr_ctx.bfd_section = values.sals[i].section;
	      sym_addr_ctx.sal = values.sals[i];
	      sym_addr_ctx.msymbol = msym;
	      values.sals[i].pc = SKIP_PROLOGUE_ADDR_CTX (&sym_addr_ctx);
	    }
	  else
	    {
	      values.sals[i].pc = SKIP_PROLOGUE (values.sals[i].pc);
	    }
	  /* APPLE LOCAL end address context.  */
	}
    }

  if (eq_symbols && canonical != (char ***) NULL)
    {
      char **canonical_arr;
      canonical_arr = (char **) xmalloc ((eq_symbols + nsymbols) * sizeof (char *));
      *canonical = canonical_arr;
      for (current = sym_list, i = 0; current; current = current->next, i++)
	{
	  struct minimal_symbol *msym;
	  msym = current->msymbol;
	  xasprintf (&canonical_arr[i], "'%s'", SYMBOL_LINKAGE_NAME (msym));
	}
      for (j = 0; j < eq_symbols; j++)
	{
	  struct minimal_symbol *msym;
	  msym = equiv_msymbols[j];
	  xasprintf (&canonical_arr[j+i], "'%s'", SYMBOL_LINKAGE_NAME (msym));
	}
    }

  values.nelts = nsymbols + eq_symbols;
  do_cleanups (equiv_cleanup);
  return values;
}
/* APPLE LOCAL end return multiple symbols  */


/* We've found a minimal symbol MSYMBOL to associate with our
   linespec; build a corresponding struct symtabs_and_lines.  */

static struct symtabs_and_lines
/* APPLE LOCAL begin equivalences */
/* We pass in the "canonical" argument, so we can get the names right
   for equivalent symbols. */
minsym_found (int funfirstline, int equivalencies,
	      struct minimal_symbol *msymbol, char ***canonical)
{
  struct symtabs_and_lines values;
  struct minimal_symbol **equiv_msymbols = NULL;
  struct minimal_symbol **pointer;
  int nsymbols, i;
  struct cleanup *equiv_cleanup;

  /* If there is an "equivalent symbol" we need to add that one as
     well here.  */
  if (equivalencies)
    equiv_msymbols = find_equivalent_msymbol (msymbol);

  nsymbols = 1;

  if (equiv_msymbols != NULL)
    {
      for (pointer = equiv_msymbols; *pointer != NULL; 
	   nsymbols++, pointer++)
	;
      equiv_cleanup = make_cleanup (xfree, equiv_msymbols);
    }
  else
    equiv_cleanup = make_cleanup (null_cleanup, NULL);

  values.sals = (struct symtab_and_line *)
	xmalloc (nsymbols * sizeof (struct symtab_and_line));
  
  for (i = 0; i < nsymbols; i++)
    {
      struct minimal_symbol *msym;
      if (i == 0)
	msym = msymbol;
      else
	msym = equiv_msymbols[i - 1];

      values.sals[i] = find_pc_sect_line (SYMBOL_VALUE_ADDRESS (msym),
					  msym->ginfo.bfd_section, 0);
      values.sals[i].section = SYMBOL_BFD_SECTION (msym);
      if (funfirstline)
	{
	  values.sals[i].pc += DEPRECATED_FUNCTION_START_OFFSET;
	  /* APPLE LOCAL begin address context.  */
	  /* Check if the current gdbarch supports a safer and more accurate
	     version of prologue skipping that takes an address context.  */
	  if (SKIP_PROLOGUE_ADDR_CTX_P ())
	    {
	      struct address_context sym_addr_ctx;
	      init_address_context (&sym_addr_ctx);
	      sym_addr_ctx.address = values.sals[i].pc;
	      sym_addr_ctx.bfd_section = values.sals[i].section;
	      sym_addr_ctx.sal = values.sals[i];
	      sym_addr_ctx.msymbol = msym;
	      values.sals[i].pc = SKIP_PROLOGUE_ADDR_CTX (&sym_addr_ctx);
	    }
	  else
	    {
	      values.sals[i].pc = SKIP_PROLOGUE (values.sals[i].pc);
	    }
	  /* APPLE LOCAL end address context.  */
	}
    }

  /* If we found "equivalent symbols" we need to add them to the
     "canonical" array, so the printer will know the real names of the
     functions.  */

  if (nsymbols > 1 && canonical != (char ***) NULL)
    {
      char **canonical_arr;
      canonical_arr = (char **) xmalloc (nsymbols * sizeof (char *));
      *canonical = canonical_arr;
      for (i = 0; i < nsymbols; i++)
	{
	  struct minimal_symbol *msym;
	  if (i == 0)
	    msym = msymbol;
	  else
	    msym = equiv_msymbols[i - 1];
	  xasprintf (&canonical_arr[i], "'%s'", SYMBOL_LINKAGE_NAME (msym));
	}
    }

  values.nelts = nsymbols;
  do_cleanups (equiv_cleanup);
  /* APPLE LOCAL end equivalences */
  return values;
}

/* APPLE LOCAL begin */
/* This function is put in cleanup chains to reset the
   allow_objc_selectors_flag global var to a correct value in case an
   error occurs. */

void
reset_allow_objc_selectors_flag (PTR dummy)
{
  allow_objc_selectors_flag = 1;
}
/* APPLE LOCAL end */
