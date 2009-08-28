/* Objective-C language support routines for GDB, the GNU debugger.

   Copyright 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.
   Written by Michael Snyder.

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
#include "expression.h"
#include "parser-defs.h"
#include "language.h"
#include "c-lang.h"
#include "objc-lang.h"
#include "exceptions.h"
#include "complaints.h"
#include "value.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb_string.h"		/* for strchr */
#include "target.h"		/* for target_has_execution */
#include "gdbcore.h"
#include "gdbcmd.h"
#include "frame.h"
#include "gdb_regex.h"
#include "regcache.h"
#include "block.h"
#include "infcall.h"
#include "valprint.h"
#include "gdb_assert.h"
#include "inferior.h"
#include "demangle.h"          /* For cplus_demangle */
#include "osabi.h"
#include "inlining.h"          /* For the rb tree stuff.  */
#include "ui-out.h"
#include "cli-out.h"
#include "breakpoint.h"

#include <ctype.h>

struct objc_object {
  CORE_ADDR isa;
};

struct objc_class {
  CORE_ADDR isa; 
  CORE_ADDR super_class; 
  CORE_ADDR name;               
  long version;
  long info;
  long instance_size;
  CORE_ADDR ivars;
  CORE_ADDR methods;
  CORE_ADDR cache;
  CORE_ADDR protocols;
};

struct objc_super {
  CORE_ADDR receiver;
  CORE_ADDR class;
};

struct objc_method {
  CORE_ADDR name;
  CORE_ADDR types;
  CORE_ADDR imp;
};

static void find_methods (struct symtab *symtab, char type,
                          const char *class, const char *category,
                          const char *selector, struct symbol **syms,
                          unsigned int *nsym, unsigned int *ndebug);

/* Should we lookup ObjC Classes as part of ordinary symbol resolution? */
int lookup_objc_class_p = 1;

/* Should we override the check for things like malloc on the stack above us before
   calling PO?  */
int call_po_at_unsafe_times = 0;

/* If we can't safely run po with the other threads frozen, should we run it
   with the other threads free to execute.  */
int let_po_run_all_threads = 1;

/* debug_mode_set_p is used to tell us that the debugger mode
   set function has already been run.  If it is 0, then we will
   try to call the debugger mode function.  If it is 1, we will
   assume the call succeeded, and if it is -1 we assume we were
   told it is unsafe to call functions.  */
enum debug_modes
  {
    debug_mode_not_checked,
    debug_mode_okay,
    debug_mode_failed,
    debug_mode_overridden
  };

static enum debug_modes debug_mode_set_p;

/* If debug_mode_set_p has a value of debug_mode_failed, we need to return the
   correct reason for that result.  We'll store that in
   debug_mode_set_reason.  This will be initialized to 
   objc_debugger_mode_unknown.  */
static enum objc_debugger_mode_result debug_mode_set_reason;

/* When we go to read the method table in find_implementation_from_class,
   sometimes we are looking at a bogus object, and just spin forever.  So 
   after we've seen this many "methods" we error out.  */
static unsigned int objc_class_method_limit = 10000;

/* We don't yet have an dynamic way of figuring out what the ObjC
   runtime version is.  So this will override our guess.  */
static int objc_runtime_version = 0;

/* APPLE LOCAL: This tree keeps the map of {class, selector} -> implementation.  */
static struct rb_tree_node *implementation_tree = NULL;
static CORE_ADDR  lookup_implementation_in_cache (CORE_ADDR class, CORE_ADDR sel);
static void add_implementation_to_cache (CORE_ADDR class, CORE_ADDR sel, CORE_ADDR implementation);

/* Are we using the ObjC 2.0 runtime?  */
static int new_objc_runtime_internals ();

/* APPLE LOCAL: This tree keeps the map of class -> class type.  The tree is probably overkill
 in this case, but it's easy to use and we won't have all that many of them.  */
static struct rb_tree_node *classname_tree = NULL;

static char *lookup_classname_in_cache (CORE_ADDR class);
static void add_classname_to_cache (CORE_ADDR class, char *classname);

/* This is the current objc objfile.  I mostly use this to tell if it's worthwhile
   trying to call gdb_objc_startDebuggerMode.  */
static struct objfile *cached_objc_objfile;
static char *objc_library_name = "libobjc.A.dylib";
void objc_init_trampoline_observer ();
static void objc_clear_trampoline_data ();
/* APPLE LOCAL use '[object class]' rather than isa  */
static CORE_ADDR get_class_address_from_object (CORE_ADDR);

#define OBJC_FETCH_POINTER_ARGUMENT(argi) \
  FETCH_POINTER_ARGUMENT (get_current_frame (), argi, builtin_type_void_func_ptr)

#ifndef TARGET_ADDRESS_BYTES
#define TARGET_ADDRESS_BYTES (TARGET_LONG_BIT / TARGET_CHAR_BIT)
#endif

/* Lookup a structure type named "struct NAME", visible in lexical
   block BLOCK.  If NOERR is nonzero, return zero if NAME is not
   suitably defined.  */

struct symbol *
lookup_struct_typedef (char *name, struct block *block, int noerr)
{
  struct symbol *sym;

  sym = lookup_symbol (name, block, STRUCT_DOMAIN, 0, 
		       (struct symtab **) NULL);

  if (sym == NULL)
    {
      if (noerr)
	return 0;
      else 
	error (_("No struct type named %s."), name);
    }
  if (TYPE_CODE (SYMBOL_TYPE (sym)) != TYPE_CODE_STRUCT)
    {
      if (noerr)
	return 0;
      else
	error (_("This context has class, union or enum %s, not a struct."), 
	       name);
    }
  return sym;
}

CORE_ADDR 
lookup_objc_class (char *classname)
{
  static struct cached_value *function = NULL;
  struct value *classval;
  struct value *ret_value;
  struct cleanup *scheduler_cleanup;
  CORE_ADDR retval = 0;
  enum objc_debugger_mode_result objc_retval;

  if (! target_has_execution)
    {
      /* Can't call into inferior to lookup class.  */
      return 0;
    }

  if (function == NULL)
    {
      /* APPLE LOCAL begin Objective-C */
      if (lookup_minimal_symbol("objc_lookUpClass", 0, 0))
	function = create_cached_function ("objc_lookUpClass",
					   builtin_type_voidptrfuncptr);
      else if (lookup_minimal_symbol ("objc_lookup_class", 0, 0))
	function = create_cached_function ("objc_lookup_class",
					   builtin_type_voidptrfuncptr);
      else
        return 0;
      /* APPLE LOCAL end Objective-C */
    }

  /* APPLE LOCAL: Lock the scheduler before calling this so the other threads 
     don't make progress while you are running this.  */

  scheduler_cleanup = make_cleanup_set_restore_scheduler_locking_mode 
                      (scheduler_locking_on);

  /* Pass in "ALL_THREADS" since we've locked the scheduler.  */

  objc_retval = make_cleanup_set_restore_debugger_mode (NULL, 0);
  if (objc_retval == objc_debugger_mode_fail_objc_api_unavailable)
    if (target_check_safe_call (OBJC_SUBSYSTEM, CHECK_SCHEDULER_VALUE))
      objc_retval = objc_debugger_mode_success;

  if (objc_retval == objc_debugger_mode_success)
    {
      classval = value_string (classname, strlen (classname) + 1);
      classval = value_coerce_array (classval);
      
      ret_value = call_function_by_hand (lookup_cached_function (function),
				   1, &classval);
      retval = (CORE_ADDR) value_as_address (ret_value);
    }

  do_cleanups (scheduler_cleanup);

  return retval;
}

CORE_ADDR
lookup_child_selector_nocache (char *selname)
{
  static struct cached_value *function = NULL;
  struct value *selstring;
  struct value *retval;

  if (! target_has_execution)
    {
      /* Can't call into inferior to lookup selector.  */
      return 0;
    }

  if (function == NULL)
    {
      /* APPLE LOCAL begin Objective-C */
      if (lookup_minimal_symbol("sel_getUid", 0, 0))
	function = create_cached_function ("sel_getUid",
					   builtin_type_voidptrfuncptr);

      else if (lookup_minimal_symbol ("sel_get_any_uid", 0, 0))
    	function = create_cached_function ("sel_get_any_uid",
					   builtin_type_voidptrfuncptr);
      else
	{
	  complaint (&symfile_complaints, "no way to lookup Objective-C selectors");
	  return 0;
	}
      /* APPLE LOCAL end Objective-C */
    }

  selstring = value_coerce_array (value_string (selname, 
						strlen (selname) + 1));
  retval = call_function_by_hand (lookup_cached_function (function),
				  1, &selstring);
  return value_as_long (retval);
}

/* Maps selector names to cached id values. */

struct selector_entry {
  char *name;
  CORE_ADDR val;
  struct selector_entry *next;
};

#define SELECTOR_HASH_SIZE 127
static struct selector_entry *selector_hash[SELECTOR_HASH_SIZE] = { 0 };

/* Stores the process-id for which the cached selector-ids are valid. */

static int selector_hash_generation = -1;

/* Reset the selector cache, deallocating all entries. */

static void
reset_child_selector_cache (void)
{
  int i;
  
  for (i = 0; i < SELECTOR_HASH_SIZE; i++)
    {
      struct selector_entry *entry, *temp;

      entry = selector_hash[i];
      while (entry != NULL)
	{
	  temp = entry;
	  entry = entry->next;
	  xfree (temp->name);
	  xfree (temp);
	}

      selector_hash[i] = NULL;
    }
}

/* Look up the id for the specified selector.  If the process-id of
   the inferior has changed, reset the selector cache; otherwise, use
   any cached value that might be present. */

CORE_ADDR
lookup_child_selector (char *selname)
{
  struct selector_entry *entry;
  int hash;
  int current_generation;

  current_generation = ptid_get_pid (inferior_ptid);
  if (current_generation != selector_hash_generation)
    {
      reset_child_selector_cache ();
      selector_hash_generation = current_generation;
    }
  
  hash = msymbol_hash (selname) % SELECTOR_HASH_SIZE;

  for (entry = selector_hash[hash]; entry != NULL; entry = entry->next)
    if ((entry != NULL) && (strcmp (entry->name, selname) == 0))
      break;

  if (entry != NULL)
    return entry->val;

  entry = (struct selector_entry *) xmalloc (sizeof (struct selector_entry));
  entry->name = xstrdup (selname);
  entry->val = lookup_child_selector_nocache (selname);
  entry->next = selector_hash[hash];

  selector_hash[hash] = entry;

  return entry->val;
}

struct value * 
value_nsstring (char *ptr, int len)
{
  struct value *stringValue[3];
  struct value *function, *nsstringValue;
  struct symbol *sym;
  struct type *type;
  static struct type *func_type = NULL;
  struct type *ret_type;

  if (!target_has_execution)
    return 0;		/* Can't call into inferior to create NSString.  */

  sym = lookup_struct_typedef ("NSString", 0, 1);
  if (sym == NULL)
    sym = lookup_struct_typedef ("NXString", 0, 1);
  if (sym == NULL)
    type = lookup_pointer_type (builtin_type_void);
  else
    type = lookup_pointer_type (SYMBOL_TYPE (sym));

  stringValue[2] = value_string (ptr, len);
  stringValue[2] = value_coerce_array (stringValue[2]);

  /* APPLE LOCAL: Make a function type that actually returns an
     (NSString *) since if we just pass voidptrfuncptr then the
     value's enclosing type is wrong, and that can cause problems
     later on.  */

  if (func_type == NULL)
    func_type = alloc_type (NULL);
  func_type = make_function_type (type, &func_type);
  ret_type = lookup_pointer_type (func_type);

  if (lookup_minimal_symbol ("_NSNewStringFromCString", 0, 0))
    {
      function = find_function_in_inferior ("_NSNewStringFromCString",
					    ret_type);
      nsstringValue = call_function_by_hand (function, 1, &stringValue[2]);
    }
  else if (lookup_minimal_symbol ("istr", 0, 0))
    {
      function = find_function_in_inferior ("istr",
					    ret_type);
      nsstringValue = call_function_by_hand (function, 1, &stringValue[2]);
    }
  else if (lookup_minimal_symbol ("+[NSString stringWithCString:]", 0, 0))
    {
      function = find_function_in_inferior("+[NSString stringWithCString:]",
					   ret_type);
      stringValue[0] = value_from_longest 
                           (builtin_type_long, lookup_objc_class ("NSString"));
      stringValue[1] = value_from_longest 
              (builtin_type_long, lookup_child_selector ("stringWithCString:"));
      nsstringValue = call_function_by_hand (function, 3, &stringValue[0]);
    }
  else
    error (_("NSString: internal error -- no way to create new NSString"));

  return nsstringValue;
}

/* Objective-C name demangling.  */

char *
objcplus_demangle (const char *mangled, int options)
{
  char *demangled;

  demangled = objc_demangle (mangled, options);
  if (demangled != NULL)
    return demangled;
  demangled = cplus_demangle (mangled, options);
  return demangled;
}

/* Objective-C name demangling.  */

char *
objc_demangle (const char *mangled, int options)
{
  char *demangled, *cp;

  if (mangled[0] == '_' &&
     (mangled[1] == 'i' || mangled[1] == 'c') &&
      mangled[2] == '_')
    {
      cp = demangled = xmalloc(strlen(mangled) + 2);

      if (mangled[1] == 'i')
	*cp++ = '-';		/* for instance method */
      else
	*cp++ = '+';		/* for class    method */

      *cp++ = '[';		/* opening left brace  */
      strcpy(cp, mangled+3);	/* tack on the rest of the mangled name */

      while (*cp && *cp == '_')
	cp++;			/* skip any initial underbars in class name */

      cp = strchr(cp, '_');
      if (!cp)	                /* find first non-initial underbar */
	{
	  xfree(demangled);	/* not mangled name */
	  return NULL;
	}
      if (cp[1] == '_') {	/* easy case: no category name     */
	*cp++ = ' ';		/* replace two '_' with one ' '    */
	strcpy(cp, mangled + (cp - demangled) + 2);
      }
      else {
	*cp++ = '(';		/* less easy case: category name */
	cp = strchr(cp, '_');
	if (!cp)
	  {
	    xfree(demangled);	/* not mangled name */
	    return NULL;
	  }
	*cp++ = ')';
	*cp++ = ' ';		/* overwriting 1st char of method name...  */
	strcpy(cp, mangled + (cp - demangled));	/* get it back */
      }

      while (*cp && *cp == '_')
	cp++;			/* skip any initial underbars in method name */

      for (; *cp; cp++)
	if (*cp == '_')
	  *cp = ':';		/* replace remaining '_' with ':' */

      *cp++ = ']';		/* closing right brace */
      *cp++ = 0;		/* string terminator */
      return demangled;
    }
  else
    return NULL;	/* Not an objc mangled name.  */
}

/* Print the character C on STREAM as part of the contents of a
   literal string whose delimiter is QUOTER.  Note that that format
   for printing characters and strings is language specific.  */

static void
objc_emit_char (int c, struct ui_file *stream, int quoter)
{

  c &= 0xFF;			/* Avoid sign bit follies.  */

  if (PRINT_LITERAL_FORM (c))
    {
      if (c == '\\' || c == quoter)
	{
	  fputs_filtered ("\\", stream);
	}
      fprintf_filtered (stream, "%c", c);
    }
  else
    {
      switch (c)
	{
	case '\n':
	  fputs_filtered ("\\n", stream);
	  break;
	case '\b':
	  fputs_filtered ("\\b", stream);
	  break;
	case '\t':
	  fputs_filtered ("\\t", stream);
	  break;
	case '\f':
	  fputs_filtered ("\\f", stream);
	  break;
	case '\r':
	  fputs_filtered ("\\r", stream);
	  break;
	case '\033':
	  fputs_filtered ("\\e", stream);
	  break;
	case '\007':
	  fputs_filtered ("\\a", stream);
	  break;
	default:
	  fprintf_filtered (stream, "\\%.3o", (unsigned int) c);
	  break;
	}
    }
}

static void
objc_printchar (int c, struct ui_file *stream)
{
  fputs_filtered ("'", stream);
  objc_emit_char (c, stream, '\'');
  fputs_filtered ("'", stream);
}

/* Print the character string STRING, printing at most LENGTH
   characters.  Printing stops early if the number hits print_max;
   repeat counts are printed as appropriate.  Print ellipses at the
   end if we had to stop before printing LENGTH characters, or if
   FORCE_ELLIPSES.  */

static void
objc_printstr (struct ui_file *stream, const gdb_byte *string, 
	       unsigned int length, int width, int force_ellipses)
{
  unsigned int i;
  unsigned int things_printed = 0;
  int in_quotes = 0;
  int need_comma = 0;

  /* If the string was not truncated due to `set print elements', and
     the last byte of it is a null, we don't print that, in
     traditional C style.  */
  if ((!force_ellipses) && length > 0 && string[length-1] == '\0')
    length--;

  if (length == 0)
    {
      fputs_filtered ("\"\"", stream);
      return;
    }

  for (i = 0; i < length && things_printed < print_max; ++i)
    {
      /* Position of the character we are examining to see whether it
	 is repeated.  */
      unsigned int rep1;
      /* Number of repetitions we have detected so far.  */
      unsigned int reps;

      QUIT;

      if (need_comma)
	{
	  fputs_filtered (", ", stream);
	  need_comma = 0;
	}

      rep1 = i + 1;
      reps = 1;
      while (rep1 < length && string[rep1] == string[i])
	{
	  ++rep1;
	  ++reps;
	}

      if (reps > repeat_count_threshold)
	{
	  if (in_quotes)
	    {
	      if (inspect_it)
		fputs_filtered ("\\\", ", stream);
	      else
		fputs_filtered ("\", ", stream);
	      in_quotes = 0;
	    }
	  objc_printchar (string[i], stream);
	  fprintf_filtered (stream, " <repeats %u times>", reps);
	  i = rep1 - 1;
	  things_printed += repeat_count_threshold;
	  need_comma = 1;
	}
      else
	{
	  if (!in_quotes)
	    {
	      if (inspect_it)
		fputs_filtered ("\\\"", stream);
	      else
		fputs_filtered ("\"", stream);
	      in_quotes = 1;
	    }
	  objc_emit_char (string[i], stream, '"');
	  ++things_printed;
	}
    }

  /* Terminate the quotes if necessary.  */
  if (in_quotes)
    {
      if (inspect_it)
	fputs_filtered ("\\\"", stream);
      else
	fputs_filtered ("\"", stream);
    }

  if (force_ellipses || i < length)
    fputs_filtered ("...", stream);
}

/* Create a fundamental C type using default reasonable for the
   current target.

   Some object/debugging file formats (DWARF version 1, COFF, etc) do
   not define fundamental types such as "int" or "double".  Others
   (stabs or DWARF version 2, etc) do define fundamental types.  For
   the formats which don't provide fundamental types, gdb can create
   such types using this function.

   FIXME: Some compilers distinguish explicitly signed integral types
   (signed short, signed int, signed long) from "regular" integral
   types (short, int, long) in the debugging information.  There is
   some disagreement as to how useful this feature is.  In particular,
   gcc does not support this.  Also, only some debugging formats allow
   the distinction to be passed on to a debugger.  For now, we always
   just use "short", "int", or "long" as the type name, for both the
   implicit and explicitly signed types.  This also makes life easier
   for the gdb test suite since we don't have to account for the
   differences in output depending upon what the compiler and
   debugging format support.  We will probably have to re-examine the
   issue when gdb starts taking it's fundamental type information
   directly from the debugging information supplied by the compiler.
   fnf@cygnus.com */

static struct type *
objc_create_fundamental_type (struct objfile *objfile, int typeid)
{
  struct type *type = NULL;

  switch (typeid)
    {
      default:
	/* FIXME: For now, if we are asked to produce a type not in
	   this language, create the equivalent of a C integer type
	   with the name "<?type?>".  When all the dust settles from
	   the type reconstruction work, this should probably become
	   an error.  */
	type = init_type (TYPE_CODE_INT,
			  TARGET_INT_BIT / TARGET_CHAR_BIT,
			  0, "<?type?>", objfile);
        warning (_("internal error: no C/C++ fundamental type %d"), typeid);
	break;
      case FT_VOID:
	type = init_type (TYPE_CODE_VOID,
			  TARGET_CHAR_BIT / TARGET_CHAR_BIT,
			  0, "void", objfile);
	break;
      case FT_CHAR:
	type = init_type (TYPE_CODE_INT,
			  TARGET_CHAR_BIT / TARGET_CHAR_BIT,
			  0, "char", objfile);
	break;
      case FT_SIGNED_CHAR:
	type = init_type (TYPE_CODE_INT,
			  TARGET_CHAR_BIT / TARGET_CHAR_BIT,
			  0, "signed char", objfile);
	break;
      case FT_UNSIGNED_CHAR:
	type = init_type (TYPE_CODE_INT,
			  TARGET_CHAR_BIT / TARGET_CHAR_BIT,
			  TYPE_FLAG_UNSIGNED, "unsigned char", objfile);
	break;
      case FT_SHORT:
	type = init_type (TYPE_CODE_INT,
			  TARGET_SHORT_BIT / TARGET_CHAR_BIT,
			  0, "short", objfile);
	break;
      case FT_SIGNED_SHORT:
	type = init_type (TYPE_CODE_INT,
			  TARGET_SHORT_BIT / TARGET_CHAR_BIT,
			  0, "short", objfile);	/* FIXME-fnf */
	break;
      case FT_UNSIGNED_SHORT:
	type = init_type (TYPE_CODE_INT,
			  TARGET_SHORT_BIT / TARGET_CHAR_BIT,
			  TYPE_FLAG_UNSIGNED, "unsigned short", objfile);
	break;
      case FT_INTEGER:
	type = init_type (TYPE_CODE_INT,
			  TARGET_INT_BIT / TARGET_CHAR_BIT,
			  0, "int", objfile);
	break;
      case FT_SIGNED_INTEGER:
	type = init_type (TYPE_CODE_INT,
			  TARGET_INT_BIT / TARGET_CHAR_BIT,
			  0, "int", objfile); /* FIXME -fnf */
	break;
      case FT_UNSIGNED_INTEGER:
	type = init_type (TYPE_CODE_INT,
			  TARGET_INT_BIT / TARGET_CHAR_BIT,
			  TYPE_FLAG_UNSIGNED, "unsigned int", objfile);
	break;
      case FT_LONG:
	type = init_type (TYPE_CODE_INT,
			  TARGET_LONG_BIT / TARGET_CHAR_BIT,
			  0, "long", objfile);
	break;
      case FT_SIGNED_LONG:
	type = init_type (TYPE_CODE_INT,
			  TARGET_LONG_BIT / TARGET_CHAR_BIT,
			  0, "long", objfile); /* FIXME -fnf */
	break;
      case FT_UNSIGNED_LONG:
	type = init_type (TYPE_CODE_INT,
			  TARGET_LONG_BIT / TARGET_CHAR_BIT,
			  TYPE_FLAG_UNSIGNED, "unsigned long", objfile);
	break;
      case FT_LONG_LONG:
	type = init_type (TYPE_CODE_INT,
			  TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT,
			  0, "long long", objfile);
	break;
      case FT_SIGNED_LONG_LONG:
	type = init_type (TYPE_CODE_INT,
			  TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT,
			  0, "signed long long", objfile);
	break;
      case FT_UNSIGNED_LONG_LONG:
	type = init_type (TYPE_CODE_INT,
			  TARGET_LONG_LONG_BIT / TARGET_CHAR_BIT,
			  TYPE_FLAG_UNSIGNED, "unsigned long long", objfile);
	break;
      case FT_FLOAT:
	type = init_type (TYPE_CODE_FLT,
			  TARGET_FLOAT_BIT / TARGET_CHAR_BIT,
			  0, "float", objfile);
	break;
      case FT_DBL_PREC_FLOAT:
	type = init_type (TYPE_CODE_FLT,
			  TARGET_DOUBLE_BIT / TARGET_CHAR_BIT,
			  0, "double", objfile);
	break;
      case FT_EXT_PREC_FLOAT:
	type = init_type (TYPE_CODE_FLT,
			  TARGET_LONG_DOUBLE_BIT / TARGET_CHAR_BIT,
			  0, "long double", objfile);
	break;
      }
  return (type);
}

/* This section handles the "vtable" regions used as of the 64-bit
   ObjC runtime in SnowLeopard.  For our benefit, ObjC keeps a linked
   list of trampoline regions in the variable gdb_objc_trampolines.
   Each region begins with a header describing the region, then an
   array of trampoline records.  The ObjC library also provides a
   notify function it calls when adding a new region, passing in a
   pointer to the new region structure.  */

#define OBJC_TRAMPOLINE_MESSAGE (1<<0)  // trampoline acts like objc_msgSend_fixedup
#define OBJC_TRAMPOLINE_STRET (1<<1)    // trampoline is struct-returning
#define OBJC_TRAMPOLINE_VTABLE (1<<2)   // trampoline is vtable dispatcher

struct objc_trampoline_record
{
  CORE_ADDR start_addr; /* The start addr of the trampoline.  */
  uint64_t flags;       /* Flags as given above.  The size of the flags field in 
			   the actual trampoline in 32 bits if desc_size == 8, and
			   64 otherwise.  Might as well just use a 64 bit value 
			   to store it here.  */
};

struct objc_trampoline_region
{
  CORE_ADDR next_region_start;  /* This is the address of the next region header.   */
  CORE_ADDR trampoline_start;   /* We compute the total start & end addresses so we */
  CORE_ADDR trampoline_end;     /* can quickly tell if we are in this region.       */
  struct objc_trampoline_region *next;  /* The link to OUR next region.  */
  int num_records;
  struct objc_trampoline_record records[];
};

struct objc_trampolines
{
  int initialized;   /* Have we read in the data from libobjc.A.dyilb yet?  */
  CORE_ADDR gdb_objc_trampoline_addr;  /* This is where the trampoline data starts in the target. */
  struct breakpoint *update_bpt;  /* This is the notify function breakpoint.  */
  struct objc_trampoline_region *head;  /* The linked list of region structures.  */
};

static struct objc_trampolines objc_trampolines;

/* Given the address ADDR of a trampoline header, read in the data for
   that region and add it to the linked list in objc_trampolines.  */
static struct objc_trampoline_region *
objc_read_trampoline_region (CORE_ADDR addr)
{
  ULONGEST header_size;
  ULONGEST desc_size;
  ULONGEST num_records;
  struct objc_trampoline_region *region;
  int wordsize = TARGET_ADDRESS_BYTES;
  CORE_ADDR orig_addr = addr;
  int i;
  struct cleanup *region_cleanup;
  int entry_size;

  /* First read in the header.  */
  header_size = read_memory_unsigned_integer (addr, 2);
  addr += 2;
  desc_size = read_memory_unsigned_integer (addr, 2);
  addr += 2;
  num_records = read_memory_unsigned_integer (addr, 4);
  addr += 4;
  
  /* There are two versions of the trampoline at present.  One has two
     uint32_t's.  The other has two intptr_t's.  This is a bit ambiguous
     since if fields get added to the 32 bit version later, I can't tell 
     just from the desc_size whether it's a 64 bit with no fields added
     or a 32 bit with fields added.  So I do the "what does desc_size
     mean logic separately for each wordsize value.  */
  if (wordsize == 4)
    entry_size = 4;
  else if (wordsize == 8)
    {
      if (desc_size == 8)
	entry_size = 4;
      else
	entry_size = wordsize;
    }
  else 
    internal_error (__FILE__, __LINE__, "Unrecognized word size: %d for trampoline, skipping.", wordsize);

  /* Now that we know how big it's going to be, we can make the region.  */
  region = xmalloc (sizeof (struct objc_trampoline_region)
		    + num_records * sizeof (struct objc_trampoline_record));

  region_cleanup = make_cleanup (xfree, region);

  region->next_region_start = read_memory_unsigned_integer (addr, wordsize);
  region->num_records = num_records;

  /* Now skip to the start of the records using the header size.  */
  addr = orig_addr + header_size;

  for (i = 0; i < region->num_records; i++)
    {
      CORE_ADDR record_start = addr;
      CORE_ADDR offset;

      /* The address in the ObjC trampoline itself is the
	 given as an offset from the start of this record.  The offset
	 is the first field of the record.  We just store the trampoline
         code address, since that is more convenient.  */

      offset = read_memory_unsigned_integer (addr, entry_size);
      region->records[i].start_addr = addr + offset;
      addr += entry_size;

      region->records[i].flags = read_memory_unsigned_integer (addr, entry_size);
      addr = record_start + desc_size;
    }

  /* Get the bound of the trampoline code.  I'm assuming it is all contiguous,
     which Greg says it will be.  */
  if (region->num_records > 0)
    {
      region->trampoline_start = region->records[0].start_addr;
      region->trampoline_end = region->records[region->num_records - 1].start_addr;
    }

  discard_cleanups (region_cleanup);
  return region;
}

/* This is the function that initializes all our data structures for 
   observing the trampoline structure & its changes.  */

void
objc_init_trampoline_observer ()
{
  struct minimal_symbol *trampoline;
  struct minimal_symbol *observer;
  struct objfile *objc_objfile;
  int wordsize = TARGET_ADDRESS_BYTES;
  CORE_ADDR region_addr = 0;
  CORE_ADDR update_fn_addr;
  struct breakpoint *b;
  struct gdb_exception e;

  if (objc_trampolines.initialized)
    return;

  /* I don't want to keep looking for this symbol forever & ever, so
     I'll assume it is in libobjc and if I find that library but not
     the right symbol, I say it isn't going to be found.  Note, to do this
     I have to have a fixed name for the objc library.  This hasn't changed
     ever (it's still libobjc.A.dylib.  But if this ever changes that's
     going to cause some trouble.  */

  objc_objfile = find_libobjc_objfile ();
  if (objc_objfile == NULL)
    return;

  /* Raise the load state.  Somebody might have set it to none or extern, and we
     wouldn't be able to see these private symbols.  */

  objfile_set_load_state (objc_objfile, OBJF_SYM_ALL, 1);

  trampoline = lookup_minimal_symbol ("gdb_objc_trampolines", NULL, objc_objfile);
  observer = lookup_minimal_symbol ("gdb_objc_trampolines_changed", NULL, objc_objfile);

  if (trampoline == NULL || observer == NULL)
    {
      objc_trampolines.initialized = 1;
      return;
    }

  /* Find the address of the head of the trampolines, and read in all
     that have been set up so far.  */
  objc_trampolines.gdb_objc_trampoline_addr = SYMBOL_VALUE_ADDRESS (trampoline);

  /* Be careful to catch this, since if the objc library isn't loaded
     yet, this may be unreadable (BSS for instance) and so we'll get
     an error here.  */
  TRY_CATCH (e, RETURN_MASK_ALL)
    {
      region_addr = read_memory_unsigned_integer (objc_trampolines.gdb_objc_trampoline_addr, wordsize);
    }

  if (e.reason != NO_ERROR)
    {
      objc_trampolines.initialized = 0;
      return;
    }

  while (region_addr != 0)
    {
      struct objc_trampoline_region *region = NULL;
      /* We have to catch this too.  We can get called here when gdb
	 is running through its cached libraries on rerun or if we've
	 changed the value of inferior-auto-start-dyld from 0 to 1
	 and the objc library isn't slid yet.  */
      TRY_CATCH (e, RETURN_MASK_ALL)
	{
	  region = objc_read_trampoline_region (region_addr);
	}
      if (e.reason != NO_ERROR)
	{
	  objc_clear_trampoline_data ();
	  return;
	}
      if (region != NULL)
	{
	  region_addr = region->next_region_start;
	  region->next = objc_trampolines.head;
	  objc_trampolines.head = region;
	}
      else
	break;      
    }
  
  /* Now find & set the "updated" breakpoint:  */
  update_fn_addr = SYMBOL_VALUE_ADDRESS (observer);
  b = create_solib_event_breakpoint (update_fn_addr);
  /* Give it an addr_string so it will get slid if the library
     it's in slides.  */
  b->addr_string = xstrdup ("gdb_objc_trampolines_changed");
  b->bp_objfile = objc_objfile;
  b->requested_shlib = xstrdup (objc_objfile->name);
  objc_trampolines.update_bpt = b;
  objc_trampolines.initialized = 1;
  if (info_verbose)
    printf_filtered ("objc trampoline structures initialized.");
}

/* This is our update listener function.  Just reads & adds
   another region record.  */

int
objc_handle_update (CORE_ADDR pc)
{
  CORE_ADDR region_addr;
  struct objc_trampoline_region *region;

  if (objc_trampolines.update_bpt == NULL 
      || pc != objc_trampolines.update_bpt->loc->address)
    return 0;

  /* Okay, this is our update event.  The first argument is the pointer to
     the new trampoline region.  Let's add it.  */
  region_addr = OBJC_FETCH_POINTER_ARGUMENT (0);
  region = objc_read_trampoline_region (region_addr);
  if (region != NULL)
    {
      region->next = objc_trampolines.head;
      objc_trampolines.head = region;
    }  
  return 1;
}

static void
objc_clear_trampoline_data ()
{
  /* Free all the regions we've accumulated.  */
  struct objc_trampoline_region *region;
  
  region = objc_trampolines.head;

  while (region != NULL)
    {
      struct objc_trampoline_region *tmp;
      tmp = region;
      region = region->next;
      xfree (tmp);
    }

  /* We could keep the objc breakpoint around and it would
     reset itself, but the trampoline struct addr wouldn't.
     So it's easier just to reset the whole structure.  */
  if (objc_trampolines.update_bpt != NULL)
    {
      delete_breakpoint (objc_trampolines.update_bpt);
      objc_trampolines.update_bpt = NULL;
    }

  objc_trampolines.gdb_objc_trampoline_addr = 0;
  objc_trampolines.head = NULL;
  objc_trampolines.initialized = 0;
}

/* Is PC in an objc trampoline region?  If FLAGS is not NULL, then we
   will fill it with the flags value for the trampoline at PC.  */

int
pc_in_objc_trampoline_p (CORE_ADDR pc, uint32_t *flags)
{
  struct objc_trampoline_region *region;
  int in_tramp = 0;

  /* Next look this up in the vtable trampolines.  */
  for (region = objc_trampolines.head; region != NULL; region = region->next)
    {
      if (region->trampoline_start <= pc && pc <= region->trampoline_end)
	{
	  int i;
	  in_tramp = 1;
	  /* Okay, it's in this trampoline, now we have to figure out whether it
	     is a struct return or regular method call.  The records are in
	     ascending order by start_addr.  But only do this if they want FLAGS
	     to be filled in.  */
	  if (flags != NULL)
	    {
	      for (i = region->num_records - 1; i >= 0; i--)
		{
		  if (region->records[i].start_addr <= pc)
		    {
		      *flags = region->records[i].flags;
		      return in_tramp;
		    }
		}
	    }
	}
    }
  return in_tramp;
}

static CORE_ADDR 
objc_skip_trampoline (CORE_ADDR stop_pc)
{
  CORE_ADDR real_stop_pc;
  CORE_ADDR method_stop_pc;
  
  /* See if we might be stopped in a dyld or F&C trampoline. */
  real_stop_pc = SKIP_TRAMPOLINE_CODE (stop_pc);

  if (real_stop_pc != 0)
    find_objc_msgcall (real_stop_pc, &method_stop_pc);
  else
    find_objc_msgcall (stop_pc, &method_stop_pc);

  if (method_stop_pc)
    {
      real_stop_pc = SKIP_TRAMPOLINE_CODE (method_stop_pc);
      if (real_stop_pc == 0)
	real_stop_pc = method_stop_pc;
    }

  return real_stop_pc;
}


/* Table mapping opcodes into strings for printing operators
   and precedences of the operators.  */

static const struct op_print objc_op_print_tab[] =
  {
    {",",  BINOP_COMMA, PREC_COMMA, 0},
    {"=",  BINOP_ASSIGN, PREC_ASSIGN, 1},
    {"||", BINOP_LOGICAL_OR, PREC_LOGICAL_OR, 0},
    {"&&", BINOP_LOGICAL_AND, PREC_LOGICAL_AND, 0},
    {"|",  BINOP_BITWISE_IOR, PREC_BITWISE_IOR, 0},
    {"^",  BINOP_BITWISE_XOR, PREC_BITWISE_XOR, 0},
    {"&",  BINOP_BITWISE_AND, PREC_BITWISE_AND, 0},
    {"==", BINOP_EQUAL, PREC_EQUAL, 0},
    {"!=", BINOP_NOTEQUAL, PREC_EQUAL, 0},
    {"<=", BINOP_LEQ, PREC_ORDER, 0},
    {">=", BINOP_GEQ, PREC_ORDER, 0},
    {">",  BINOP_GTR, PREC_ORDER, 0},
    {"<",  BINOP_LESS, PREC_ORDER, 0},
    {">>", BINOP_RSH, PREC_SHIFT, 0},
    {"<<", BINOP_LSH, PREC_SHIFT, 0},
    {"+",  BINOP_ADD, PREC_ADD, 0},
    {"-",  BINOP_SUB, PREC_ADD, 0},
    {"*",  BINOP_MUL, PREC_MUL, 0},
    {"/",  BINOP_DIV, PREC_MUL, 0},
    {"%",  BINOP_REM, PREC_MUL, 0},
    {"@",  BINOP_REPEAT, PREC_REPEAT, 0},
    {"-",  UNOP_NEG, PREC_PREFIX, 0},
    {"!",  UNOP_LOGICAL_NOT, PREC_PREFIX, 0},
    {"~",  UNOP_COMPLEMENT, PREC_PREFIX, 0},
    {"*",  UNOP_IND, PREC_PREFIX, 0},
    {"&",  UNOP_ADDR, PREC_PREFIX, 0},
    {"sizeof ", UNOP_SIZEOF, PREC_PREFIX, 0},
    {"++", UNOP_PREINCREMENT, PREC_PREFIX, 0},
    {"--", UNOP_PREDECREMENT, PREC_PREFIX, 0},
    {NULL, OP_NULL, PREC_NULL, 0}
};

struct type ** const (objc_builtin_types[]) = 
{
  &builtin_type_int,
  &builtin_type_long,
  &builtin_type_short,
  &builtin_type_char,
  &builtin_type_float,
  &builtin_type_double,
  &builtin_type_void,
  &builtin_type_long_long,
  &builtin_type_signed_char,
  &builtin_type_unsigned_char,
  &builtin_type_unsigned_short,
  &builtin_type_unsigned_int,
  &builtin_type_unsigned_long,
  &builtin_type_unsigned_long_long,
  &builtin_type_long_double,
  &builtin_type_complex,
  &builtin_type_double_complex,
  0
};

const struct language_defn objc_language_defn = {
  "objective-c",		/* Language name */
  language_objc,
  objc_builtin_types,
  range_check_off,
  type_check_off,
  case_sensitive_on,
  array_row_major,
  &exp_descriptor_standard,
  objc_parse,
  objc_error,
  null_post_parser,
  objc_printchar,		/* Print a character constant */
  objc_printstr,		/* Function to print string constant */
  objc_emit_char,
  objc_create_fundamental_type,	/* Create fundamental type in this language */
  c_print_type,			/* Print a type using appropriate syntax */
  c_val_print,			/* Print a value using appropriate syntax */
  c_value_print,		/* Print a top-level value */
  objc_skip_trampoline,         /* Language specific skip_trampoline */
  value_of_this,                /* value_of_this */
  basic_lookup_symbol_nonlocal, /* lookup_symbol_nonlocal */
  basic_lookup_transparent_type,/* lookup_transparent_type */
  objc_demangle,                /* Language specific symbol demangler */
  NULL,				/* Language specific class_name_from_physname */
  objc_op_print_tab,		/* expression operators for printing */
  1,				/* c-style arrays */
  0,				/* String lower bound */
  &builtin_type_char,		/* Type of string elements */
  default_word_break_characters,
  NULL, /* FIXME: la_language_arch_info.  */
  LANG_MAGIC
};

/* APPLE LOCAL begin Objective-C++ */
const struct language_defn objcplus_language_defn = {
  "objective-c++",				/* Language name */
  language_objcplus,
  objc_builtin_types,
  range_check_off,
  type_check_off,
  case_sensitive_on,
  array_row_major,
  &exp_descriptor_standard,
  objc_parse,
  objc_error,
  null_post_parser,
  objc_printchar,		/* Print a character constant */
  objc_printstr,		/* Function to print string constant */
  objc_emit_char,
  objc_create_fundamental_type,	/* Create fundamental type in this language */
  c_print_type,			/* Print a type using appropriate syntax */
  c_val_print,			/* Print a value using appropriate syntax */
  c_value_print,		/* Print a top-level value */
  objc_skip_trampoline, 	/* Language specific skip_trampoline */
  value_of_this,		/* value_of_this */
  basic_lookup_symbol_nonlocal,	/* lookup_symbol_nonlocal */
  basic_lookup_transparent_type,/* lookup_transparent_type */
  objcplus_demangle,		/* Language specific symbol demangler */
  NULL,
  objc_op_print_tab,		/* Expression operators for printing */
  1,				/* C-style arrays */
  0,				/* String lower bound */
  &builtin_type_char,		/* Type of string elements */
  default_word_break_characters,
  NULL, /* FIXME: la_language_arch_info.  */
  LANG_MAGIC
};

/*
 * ObjC:
 * Following functions help construct Objective-C message calls 
 */

struct selname		/* For parsing Objective-C.  */
  {
    struct selname *next;
    char *msglist_sel;
    int msglist_len;
  };

static int msglist_len;
static struct selname *selname_chain;
static char *msglist_sel;

void
start_msglist(void)
{
  struct selname *new = 
    (struct selname *) xmalloc (sizeof (struct selname));

  new->next = selname_chain;
  new->msglist_len = msglist_len;
  new->msglist_sel = msglist_sel;
  msglist_len = 0;
  msglist_sel = (char *)xmalloc(1);
  *msglist_sel = 0;
  selname_chain = new;
}

void
add_msglist(struct stoken *str, int addcolon)
{
  char *s, *p;
  int len, plen;

  if (str == 0) {		/* Unnamed arg, or...  */
    if (addcolon == 0) {	/* variable number of args.  */
      msglist_len++;
      return;
    }
    p = "";
    plen = 0;
  } else {
    p = str->ptr;
    plen = str->length;
  }
  len = plen + strlen(msglist_sel) + 2;
  s = (char *)xmalloc(len);
  strcpy(s, msglist_sel);
  strncat(s, p, plen);
  xfree(msglist_sel);
  msglist_sel = s;
  if (addcolon) {
    s[len-2] = ':';
    s[len-1] = 0;
    msglist_len++;
  } else
    s[len-2] = '\0';
}

int
end_msglist(void)
{
  int val = msglist_len;
  struct selname *sel = selname_chain;
  char *p = msglist_sel;
  CORE_ADDR selid;

  selname_chain = sel->next;
  msglist_len = sel->msglist_len;
  msglist_sel = sel->msglist_sel;
  selid = lookup_child_selector(p);
  if (!selid)
    error (_("Can't find selector \"%s\""), p);
  write_exp_elt_longcst (selid);
  xfree(p);
  write_exp_elt_longcst (val);	/* Number of args */
  xfree(sel);

  return val;
}

/*
 * Function: specialcmp (char *a, char *b)
 *
 * Special strcmp: treats ']' and ' ' as end-of-string.
 * Used for qsorting lists of objc methods (either by class or selector).
 */

static int
specialcmp (char *a, char *b)
{
  while (*a && *a != ' ' && *a != ']' && *b && *b != ' ' && *b != ']')
    {
      if (*a != *b)
	return *a - *b;
      a++, b++;
    }
  if (*a && *a != ' ' && *a != ']')
    return  1;		/* a is longer therefore greater */
  if (*b && *b != ' ' && *b != ']')
    return -1;		/* a is shorter therefore lesser */
  return    0;		/* a and b are identical */
}

/*
 * Function: compare_selectors (const void *, const void *)
 *
 * Comparison function for use with qsort.  Arguments are symbols or
 * msymbols Compares selector part of objc method name alphabetically.
 */

static int
compare_selectors (const void *a, const void *b)
{
  char *aname, *bname;

  aname = SYMBOL_PRINT_NAME (*(struct symbol **) a);
  bname = SYMBOL_PRINT_NAME (*(struct symbol **) b);
  if (aname == NULL || bname == NULL)
    error (_("internal: compare_selectors(1)"));

  aname = strchr(aname, ' ');
  bname = strchr(bname, ' ');
  if (aname == NULL || bname == NULL)
    error (_("internal: compare_selectors(2)"));

  return specialcmp (aname+1, bname+1);
}

/*
 * Function: selectors_info (regexp, from_tty)
 *
 * Implements the "Info selectors" command.  Takes an optional regexp
 * arg.  Lists all objective c selectors that match the regexp.  Works
 * by grepping thru all symbols for objective c methods.  Output list
 * is sorted and uniqued. 
 */

static void
selectors_info (char *regexp, int from_tty)
{
  struct objfile	*objfile;
  struct minimal_symbol *msymbol;
  char                  *name;
  char                  *val;
  int                    matches = 0;
  int                    maxlen  = 0;
  int                    ix;
  char                   myregexp[2048];
  char                   asel[256];
  struct symbol        **sym_arr;
  int                    plusminus = 0;

  if (regexp == NULL)
    strcpy(myregexp, ".*]");	/* Null input, match all objc methods.  */
  else
    {
      if (*regexp == '+' || *regexp == '-')
	{ /* User wants only class methods or only instance methods.  */
	  plusminus = *regexp++;
	  while (*regexp == ' ' || *regexp == '\t')
	    regexp++;
	}
      if (*regexp == '\0')
	strcpy(myregexp, ".*]");
      else
	{
	  strcpy(myregexp, regexp);
	  if (myregexp[strlen(myregexp) - 1] == '$') /* end of selector */
	    myregexp[strlen(myregexp) - 1] = ']';    /* end of method name */
	  else
	    strcat(myregexp, ".*]");
	}
    }

  if (regexp != NULL)
    {
      val = re_comp (myregexp);
      if (val != 0)
	error (_("Invalid regexp (%s): %s"), val, regexp);
    }

  /* First time thru is JUST to get max length and count.  */
  ALL_MSYMBOLS (objfile, msymbol)
    {
      QUIT;
      name = SYMBOL_NATURAL_NAME (msymbol);
      if (name &&
	 (name[0] == '-' || name[0] == '+') &&
	  name[1] == '[')		/* Got a method name.  */
	{
	  /* Filter for class/instance methods.  */
	  if (plusminus && name[0] != plusminus)
	    continue;
	  /* Find selector part.  */
	  name = (char *) strchr(name+2, ' ');
	  if (regexp == NULL || re_exec(++name) != 0)
	    { 
	      char *mystart = name;
	      char *myend   = (char *) strchr(mystart, ']');
	      
	      if (myend && (myend - mystart > maxlen))
		maxlen = myend - mystart;	/* Get longest selector.  */
	      matches++;
	    }
	}
    }
  if (matches)
    {
      printf_filtered (_("Selectors matching \"%s\":\n\n"), 
		       regexp ? regexp : "*");

      sym_arr = alloca (matches * sizeof (struct symbol *));
      matches = 0;
      ALL_MSYMBOLS (objfile, msymbol)
	{
	  QUIT;
	  name = SYMBOL_NATURAL_NAME (msymbol);
	  if (name &&
	     (name[0] == '-' || name[0] == '+') &&
	      name[1] == '[')		/* Got a method name.  */
	    {
	      /* Filter for class/instance methods.  */
	      if (plusminus && name[0] != plusminus)
		continue;
	      /* Find selector part.  */
	      name = (char *) strchr(name+2, ' ');
	      if (regexp == NULL || re_exec(++name) != 0)
		sym_arr[matches++] = (struct symbol *) msymbol;
	    }
	}

      qsort (sym_arr, matches, sizeof (struct minimal_symbol *), 
	     compare_selectors);
      /* Prevent compare on first iteration.  */
      asel[0] = 0;
      for (ix = 0; ix < matches; ix++)	/* Now do the output.  */
	{
	  char *p = asel;

	  QUIT;
	  name = SYMBOL_NATURAL_NAME (sym_arr[ix]);
	  name = strchr (name, ' ') + 1;
	  if (p[0] && specialcmp(name, p) == 0)
	    continue;		/* Seen this one already (not unique).  */

	  /* Copy selector part.  */
	  while (*name && *name != ']')
	    *p++ = *name++;
	  *p++ = '\0';
	  /* Print in columns.  */
	  puts_filtered_tabular(asel, maxlen + 1, 0);
	}
      begin_line();
    }
  else
    printf_filtered (_("No selectors matching \"%s\"\n"), regexp ? regexp : "*");
}

/*
 * Function: compare_classes (const void *, const void *)
 *
 * Comparison function for use with qsort.  Arguments are symbols or
 * msymbols Compares class part of objc method name alphabetically. 
 */

static int
compare_classes (const void *a, const void *b)
{
  char *aname, *bname;

  aname = SYMBOL_PRINT_NAME (*(struct symbol **) a);
  bname = SYMBOL_PRINT_NAME (*(struct symbol **) b);
  if (aname == NULL || bname == NULL)
    error (_("internal: compare_classes(1)"));

  return specialcmp (aname+1, bname+1);
}

/*
 * Function: classes_info(regexp, from_tty)
 *
 * Implements the "info classes" command for objective c classes.
 * Lists all objective c classes that match the optional regexp.
 * Works by grepping thru the list of objective c methods.  List will
 * be sorted and uniqued (since one class may have many methods).
 * BUGS: will not list a class that has no methods. 
 */

static void
classes_info (char *regexp, int from_tty)
{
  struct objfile	*objfile;
  struct minimal_symbol *msymbol;
  char                  *name;
  char                  *val;
  int                    matches = 0;
  int                    maxlen  = 0;
  int                    ix;
  char                   myregexp[2048];
  char                   aclass[256];
  struct symbol        **sym_arr;

  if (regexp == NULL)
    strcpy(myregexp, ".* ");	/* Null input: match all objc classes.  */
  else
    {
      strcpy(myregexp, regexp);
      if (myregexp[strlen(myregexp) - 1] == '$')
	/* In the method name, the end of the class name is marked by ' '.  */
	myregexp[strlen(myregexp) - 1] = ' ';
      else
	strcat(myregexp, ".* ");
    }

  if (regexp != NULL)
    {
      val = re_comp (myregexp);
      if (val != 0)
	error (_("Invalid regexp (%s): %s"), val, regexp);
    }

  /* First time thru is JUST to get max length and count.  */
  ALL_MSYMBOLS (objfile, msymbol)
    {
      QUIT;
      name = SYMBOL_NATURAL_NAME (msymbol);
      if (name &&
	 (name[0] == '-' || name[0] == '+') &&
	  name[1] == '[')			/* Got a method name.  */
	if (regexp == NULL || re_exec(name+2) != 0)
	  { 
	    /* Compute length of classname part.  */
	    char *mystart = name + 2;
	    char *myend   = (char *) strchr(mystart, ' ');
	    
	    if (myend && (myend - mystart > maxlen))
	      maxlen = myend - mystart;
	    matches++;
	  }
    }
  if (matches)
    {
      printf_filtered (_("Classes matching \"%s\":\n\n"), 
		       regexp ? regexp : "*");
      sym_arr = alloca (matches * sizeof (struct symbol *));
      matches = 0;
      ALL_MSYMBOLS (objfile, msymbol)
	{
	  QUIT;
	  name = SYMBOL_NATURAL_NAME (msymbol);
	  if (name &&
	     (name[0] == '-' || name[0] == '+') &&
	      name[1] == '[')			/* Got a method name.  */
	    if (regexp == NULL || re_exec(name+2) != 0)
		sym_arr[matches++] = (struct symbol *) msymbol;
	}

      qsort (sym_arr, matches, sizeof (struct minimal_symbol *), 
	     compare_classes);
      /* Prevent compare on first iteration.  */
      aclass[0] = 0;
      for (ix = 0; ix < matches; ix++)	/* Now do the output.  */
	{
	  char *p = aclass;

	  QUIT;
	  name = SYMBOL_NATURAL_NAME (sym_arr[ix]);
	  name += 2;
	  if (p[0] && specialcmp(name, p) == 0)
	    continue;	/* Seen this one already (not unique).  */

	  /* Copy class part of method name.  */
	  while (*name && *name != ' ')
	    *p++ = *name++;
	  *p++ = '\0';
	  /* Print in columns.  */
	  puts_filtered_tabular(aclass, maxlen + 1, 0);
	}
      begin_line();
    }
  else
    printf_filtered (_("No classes matching \"%s\"\n"), regexp ? regexp : "*");
}

/* 
 * Function: find_imps (char *selector, struct symbol **sym_arr)
 *
 * Input:  a string representing a selector
 *         a pointer to an array of symbol pointers
 *         possibly a pointer to a symbol found by the caller.
 *
 * Output: number of methods that implement that selector.  Side
 * effects: The array of symbol pointers is filled with matching syms.
 *
 * By analogy with function "find_methods" (symtab.c), builds a list
 * of symbols matching the ambiguous input, so that "decode_line_2"
 * (symtab.c) can list them and ask the user to choose one or more.
 * In this case the matches are objective c methods
 * ("implementations") matching an objective c selector.
 *
 * Note that it is possible for a normal (c-style) function to have
 * the same name as an objective c selector.  To prevent the selector
 * from eclipsing the function, we allow the caller (decode_line_1) to
 * search for such a function first, and if it finds one, pass it in
 * to us.  We will then integrate it into the list.  We also search
 * for one here, among the minsyms.
 *
 * NOTE: if NUM_DEBUGGABLE is non-zero, the sym_arr will be divided
 *       into two parts: debuggable (struct symbol) syms, and
 *       non_debuggable (struct minimal_symbol) syms.  The debuggable
 *       ones will come first, before NUM_DEBUGGABLE (which will thus
 *       be the index of the first non-debuggable one). 
 */

/*
 * Function: total_number_of_imps (char *selector);
 *
 * Input:  a string representing a selector 
 * Output: number of methods that implement that selector.
 *
 * By analogy with function "total_number_of_methods", this allows
 * decode_line_1 (symtab.c) to detect if there are objective c methods
 * matching the input, and to allocate an array of pointers to them
 * which can be manipulated by "decode_line_2" (also in symtab.c).
 */

char * 
parse_selector (char *method, char **selector)
{
  char *s1 = NULL;
  char *s2 = NULL;
  int found_quote = 0;

  char *nselector = NULL;

  gdb_assert (selector != NULL);

  s1 = method;

  while (isspace (*s1))
    s1++;
  if (*s1 == '\'') 
    {
      found_quote = 1;
      s1++;
    }
  while (isspace (*s1))
    s1++;
   
  nselector = s1;
  s2 = s1;

  for (;;) {
    if (isalnum (*s2) || (*s2 == '_') || (*s2 == ':'))
      *s1++ = *s2;
    else if (isspace (*s2))
      ;
    else if ((*s2 == '\0') || (*s2 == '\''))
      break;
    else
      return NULL;
    s2++;
  }
  *s1++ = '\0';

  while (isspace (*s2))
    s2++;
  if (found_quote)
    {
      if (*s2 == '\'') 
	s2++;
      while (isspace (*s2))
	s2++;
    }

  if (selector != NULL)
    *selector = nselector;

  return s2;
}

char * 
parse_method (char *method, char *type, char **class, 
	      char **category, char **selector)
{
  char *s1 = NULL;
  char *s2 = NULL;
  int found_quote = 0;

  char ntype = '\0';
  char *nclass = NULL;
  char *ncategory = NULL;
  char *nselector = NULL;

  gdb_assert (type != NULL);
  gdb_assert (class != NULL);
  gdb_assert (category != NULL);
  gdb_assert (selector != NULL);
  
  s1 = method;

  while (isspace (*s1))
    s1++;
  if (*s1 == '\'') 
    {
      found_quote = 1;
      s1++;
    }
  while (isspace (*s1))
    s1++;
  
  if ((s1[0] == '+') || (s1[0] == '-'))
    ntype = *s1++;

  while (isspace (*s1))
    s1++;

  if (*s1 != '[')
    return NULL;
  s1++;

  nclass = s1;
  while (isalnum (*s1) || (*s1 == '_'))
    s1++;
  
  s2 = s1;
  while (isspace (*s2))
    s2++;
  
  if (*s2 == '(')
    {
      s2++;
      while (isspace (*s2))
	s2++;
      ncategory = s2;
      while (isalnum (*s2) || (*s2 == '_'))
	s2++;
      *s2++ = '\0';
    }

  /* Truncate the class name now that we're not using the open paren.  */
  *s1++ = '\0';

  nselector = s2;
  s1 = s2;

  for (;;) {
    if (isalnum (*s2) || (*s2 == '_') || (*s2 == ':'))
      *s1++ = *s2;
    else if (isspace (*s2))
      ;
    else if (*s2 == ']')
      break;
    else
      return NULL;
    s2++;
  }
  *s1++ = '\0';
  s2++;

  while (isspace (*s2))
    s2++;
  if (found_quote)
    {
      if (*s2 != '\'') 
	return NULL;
      s2++;
      while (isspace (*s2))
	s2++;
    }

  if (type != NULL)
    *type = ntype;
  if (class != NULL)
    *class = nclass;
  if (category != NULL)
    *category = ncategory;
  if (selector != NULL)
    *selector = nselector;

  return s2;
}

static void
find_methods (struct symtab *symtab, char type, 
	      const char *class, const char *category, 
	      const char *selector, struct symbol **syms, 
	      unsigned int *nsym, unsigned int *ndebug)
{
  struct objfile *objfile = NULL;
  struct minimal_symbol *msymbol = NULL;
  struct block *block = NULL;
  struct symbol *sym = NULL;
  struct cleanup * old_list = NULL;
  
  char *symname = NULL;

  char ntype = '\0';
  char *nclass = NULL;
  char *ncategory = NULL;
  char *nselector = NULL;
  char *name_end = NULL;

  unsigned int csym = 0;
  unsigned int cdebug = 0;

  static char *tmp = NULL;
  static unsigned int tmplen = 0;

  gdb_assert (nsym != NULL);
  gdb_assert (ndebug != NULL);

  if (symtab)
    block = BLOCKVECTOR_BLOCK (BLOCKVECTOR (symtab), STATIC_BLOCK);

  ALL_MSYMBOLS (objfile, msymbol)
    {
      QUIT;

      /* APPLE LOCAL fix-and-continue */
      if (MSYMBOL_OBSOLETED (msymbol))
        continue;

      if ((msymbol->type != mst_text) && (msymbol->type != mst_file_text))
	/* Not a function or method.  */
	continue;

      if (symtab)
	/* APPLE LOCAL begin address ranges  */
	if (!block_contains_pc (block, SYMBOL_VALUE_ADDRESS (msymbol)))
       /* APPLE LOCAL end address ranges  */
	  /* Not in the specified symtab.  */
	  continue;

      symname = SYMBOL_NATURAL_NAME (msymbol);
      if (symname == NULL)
	continue;

      if ((symname[0] != '-' && symname[0] != '+') || (symname[1] != '['))
	/* Not a method name.  */
	continue;
      
      while ((strlen (symname) + 1) >= tmplen)
	{
	  tmplen = (tmplen == 0) ? 1024 : tmplen * 2;
	  tmp = xrealloc (tmp, tmplen);
	}
      strcpy (tmp, symname);

      name_end = parse_method (tmp, &ntype, &nclass, &ncategory, &nselector);

      /* Only accept the symbol if the WHOLE name is an ObjC method name.
	 If you compile an objc file with -fexceptions, then you will end up
	 with [Class message].eh symbols for all the real ObjC symbols, and
	 we don't want to match those.  */

      if (name_end == NULL || *name_end != '\0')
	continue;
      
      if ((type != '\0') && (ntype != type))
	continue;

      if ((class != NULL) 
	  && ((nclass == NULL) || (strcmp (class, nclass) != 0)))
	continue;

      if ((category != NULL) && 
	  ((ncategory == NULL) || (strcmp (category, ncategory) != 0)))
	continue;

      if ((selector != NULL) && 
	  ((nselector == NULL) || (strcmp (selector, nselector) != 0)))
	continue;

      /* APPLE LOCAL: Restrict the scope of the search when calling
	 find_pc_sect_function() to the current objfile that we
	 already have else we will get a recursive call that can
	 modify the restrict list and can cause an infinite loop.  */
      /* Set this to null to start, don't want it to carry over from
	 the last time through the loop.  */
      sym = NULL;

      if (objfile->separate_debug_objfile)
        {
	  old_list = 
             make_cleanup_restrict_to_objfile (objfile->separate_debug_objfile);
	  sym = find_pc_sect_function (SYMBOL_VALUE_ADDRESS (msymbol), 
                                       SYMBOL_BFD_SECTION (msymbol));
	  do_cleanups (old_list);
        }
      if (sym == NULL)
        {
	  old_list = make_cleanup_restrict_to_objfile (objfile);
	  sym = find_pc_sect_function (SYMBOL_VALUE_ADDRESS (msymbol), 
                                       SYMBOL_BFD_SECTION (msymbol));
	  do_cleanups (old_list);
        }
      
      if (sym != NULL)
        {
          const char *newsymname = SYMBOL_NATURAL_NAME (sym);
	  
          if (strcmp (symname, newsymname) == 0)
            {
              /* Found a high-level method sym: swap it into the
                 lower part of sym_arr (below num_debuggable).  */
              if (syms != NULL)
                {
                  syms[csym] = syms[cdebug];
                  syms[cdebug] = sym;
                }
              csym++;
              cdebug++;
            }
          else
            {
              warning (
"debugging symbol \"%s\" does not match minimal symbol (\"%s\"); ignoring",
                       newsymname, symname);
              if (syms != NULL)
                syms[csym] = (struct symbol *) msymbol;
              csym++;
            }
        }
      else 
	{
	  /* Found a non-debuggable method symbol.  */
	  if (syms != NULL)
	    syms[csym] = (struct symbol *) msymbol;
	  csym++;
	}
    }

  if (nsym != NULL)
    *nsym = csym;
  if (ndebug != NULL)
    *ndebug = cdebug;
}

char *
find_imps (struct symtab *symtab, struct block *block,
           char *method, struct symbol **syms, 
           unsigned int *nsym, unsigned int *ndebug)
{
  char type = '\0';
  char *class = NULL;
  char *category = NULL;
  char *selector = NULL;

  unsigned int csym = 0;
  unsigned int cdebug = 0;

  unsigned int ncsym = 0;
  unsigned int ncdebug = 0;

  char *buf = NULL;
  char *tmp = NULL;

  gdb_assert (nsym != NULL);
  gdb_assert (ndebug != NULL);

  if (nsym != NULL)
    *nsym = 0;
  if (ndebug != NULL)
    *ndebug = 0;

  buf = (char *) alloca (strlen (method) + 1);
  strcpy (buf, method);
  tmp = parse_method (buf, &type, &class, &category, &selector);

  if (tmp == NULL) {
    
    struct symbol *sym = NULL;
    struct minimal_symbol *msym = NULL;
    
    strcpy (buf, method);
    tmp = parse_selector (buf, &selector);
    
    if (tmp == NULL)
      return NULL;
    
    sym = lookup_symbol (selector, block, VAR_DOMAIN, 0, NULL);
    if (sym != NULL) 
      {
	if (syms)
	  syms[csym] = sym;
	csym++;
	cdebug++;
      }

    if (sym == NULL)
      msym = lookup_minimal_symbol (selector, 0, 0);

    if (msym != NULL) 
      {
	if (syms)
	  syms[csym] = (struct symbol *)msym;
	csym++;
      }
  }

  if (syms != NULL)
    find_methods (symtab, type, class, category, selector, 
		  syms + csym, &ncsym, &ncdebug);
  else
    find_methods (symtab, type, class, category, selector, 
		  NULL, &ncsym, &ncdebug);

  /* If we didn't find any methods, just return.  */
  if (ncsym == 0 && ncdebug == 0)
    return method;

  /* Take debug symbols from the second batch of symbols and swap them
   * with debug symbols from the first batch.  Repeat until either the
   * second section is out of debug symbols or the first section is
   * full of debug symbols.  Either way we have all debug symbols
   * packed to the beginning of the buffer.  
   */

  if (syms != NULL) 
    {
      while ((cdebug < csym) && (ncdebug > 0))
	{
	  struct symbol *s = NULL;
	  /* First non-debugging symbol.  */
	  unsigned int i = cdebug;
	  /* Last of second batch of debug symbols.  */
	  unsigned int j = csym + ncdebug - 1;

	  s = syms[j];
	  syms[j] = syms[i];
	  syms[i] = s;

	  /* We've moved a symbol from the second debug section to the
             first one.  */
	  cdebug++;
	  ncdebug--;
	}
    }

  csym += ncsym;
  cdebug += ncdebug;

  if (nsym != NULL)
    *nsym = csym;
  if (ndebug != NULL)
    *ndebug = cdebug;

  if (syms == NULL)
    return method + (tmp - buf);

  if (csym > 1)
    {
      /* Sort debuggable symbols.  */
      if (cdebug > 1)
	qsort (syms, cdebug, sizeof (struct minimal_symbol *), 
	       compare_classes);
      
      /* Sort minimal_symbols.  */
      if ((csym - cdebug) > 1)
	qsort (&syms[cdebug], csym - cdebug, 
	       sizeof (struct minimal_symbol *), compare_classes);
    }
  /* Terminate the sym_arr list.  */
  syms[csym] = 0;

  return method + (tmp - buf);
}

static void 
print_object_command (char *args, int from_tty)
{
  struct value *object, *function, *description;
  struct cleanup *cleanup_chain = NULL;
  struct cleanup *debugger_mode;
  CORE_ADDR string_addr, object_addr;
  int i = 0;
  gdb_byte c = 0;
  const char *fn_name;
  struct expression *expr;
  struct cleanup *old_chain;
  int pc = 0;

  if (!args || !*args)
    error (
"The 'print-object' command requires an argument (an Objective-C object)");

  if (call_po_at_unsafe_times)
    {
      /* Set the debugger mode so that we don't do any checking.  */
        make_cleanup_set_restore_debugger_mode (&debugger_mode, -1);
    }
  else
    {
      /* If the ObjC debugger mode exists, use that.  But not that that requires
	 that the scheduler is locked.  */
      
      enum objc_debugger_mode_result retval;

      retval = make_cleanup_set_restore_debugger_mode (&debugger_mode, 0);

      if (retval == objc_debugger_mode_fail_objc_api_unavailable)
        if (target_check_safe_call (OBJC_SUBSYSTEM, CHECK_SCHEDULER_VALUE))
          retval = objc_debugger_mode_success;

      if (retval != objc_debugger_mode_success)
	{
          do_cleanups (debugger_mode);
	  
	  /* Okay, let's see if we can let all the other threads run
	     and that will allow the PO to succeed.  */
	  
	  if (!target_check_safe_call (MALLOC_SUBSYSTEM, CHECK_CURRENT_THREAD))
	    {
	      warning ("Cancelling print_object - malloc lock could be held on current thread.");
	      return;
	    }
	  else
	    {
	  
	      /* If we have the new objc runtime, I am going to be a little more
		 paranoid, and if any frames in the first 5 stack frames are in 
		 libobjc, then I'll bail.  */
	      
	      if (new_objc_runtime_internals ())
		{
		  struct objfile *libobjc_objfile;
		  
		  libobjc_objfile = find_libobjc_objfile ();
		  if (libobjc_objfile != NULL)
		    {
		      struct frame_info *fi;
		      fi = get_current_frame ();
		      if (!fi)
			{
			  warning ("Cancelling print_object - can't find base frame of "
				   "the current thread to determine whether it is safe.");
			  return;
			}
		      
		      while (frame_relative_level (fi) < 5)
			{
			  struct obj_section *obj_sect = find_pc_section (get_frame_pc (fi));
			  if (obj_sect == NULL || obj_sect->objfile == libobjc_objfile)
			    {
			      warning ("Cancelling call - objc code on the current "
				       "thread's stack makes this unsafe.");
			      return;
			    }
			  fi = get_prev_frame (fi);
			  if (fi == NULL)
			    break;
			}
		    }
		}
	      else if (!target_check_safe_call (OBJC_SUBSYSTEM, CHECK_CURRENT_THREAD))
		{
		  warning ("Cancelling call as the ObjC runtime would deadlock.");
		  return;
		}
	    }
	  if (let_po_run_all_threads)
	    {
	      printf_unfiltered ("Allowing all threads to run to avoid the possible deadlock.\nprint-object result:\n");
	      make_cleanup_set_restore_debugger_mode (&debugger_mode, -1);
	      make_cleanup_set_restore_scheduler_locking_mode (scheduler_locking_off);
	    }
	  else
	    {
	      warning ("Cancelling call as running only this thread would cause a deadlock.\n"
		       "Set let-po-run-all-threads to \"on\" to try with all threads running.");
	      return;
	    }
	}
    }

  /* APPLE LOCAL begin initialize innermost_block  */
  
  innermost_block = NULL;
  expr = parse_expression (args);
  old_chain =  make_cleanup (free_current_contents, &expr);
  /* APPLE LOCAL end initialize innermost_block  */
  object = expr->language_defn->la_exp_desc->evaluate_exp 
    (builtin_type_void_data_ptr, expr, &pc, EVAL_NORMAL);
  do_cleanups (old_chain);

  /* APPLE LOCAL begin */
  if (object != NULL && TYPE_CODE (value_type (object)) == TYPE_CODE_ERROR)
    {
      struct type *id_type;
      id_type = lookup_typename ("id", NULL, 1);
      if (id_type)
        object = value_cast (id_type, object);
    }
  /* APPLE LOCAL end */

  /* Validate the address for sanity.  */
  object_addr = value_as_address (object);
  read_memory (object_addr, &c, 1);

  /* APPLE LOCAL begin */
  fn_name = "_NSPrintForDebugger";
  if (lookup_minimal_symbol (fn_name, NULL, NULL) == NULL)
    {
      fn_name = "_CFPrintForDebugger";
      if (lookup_minimal_symbol (fn_name, NULL, NULL) == NULL)
        error (_("Unable to locate _NSPrintForDebugger or _CFPrintForDebugger "
               "in child process"));
    }
  function = find_function_in_inferior (fn_name,
                                        builtin_type_voidptrfuncptr);
  /* APPLE LOCAL end */

  if (function == NULL)
    error (_("Unable to locate _NSPrintForDebugger or _CFPrintForDebugger in "
             "child process"));

  cleanup_chain = make_cleanup_set_restore_unwind_on_signal (1);
  
  description = call_function_by_hand (function, 1, &object);

  do_cleanups (cleanup_chain);
  do_cleanups (debugger_mode);
  
  string_addr = value_as_address (description);
  if (string_addr == 0)
    error (_("object returns null description"));

  read_memory (string_addr + i++, &c, 1);
  if (c != 0)
    do
      { /* Read and print characters up to EOS.  */
	QUIT;
	printf_filtered ("%c", c);
	read_memory (string_addr + i++, &c, 1);
      } while (c != 0);
  else
    printf_filtered(_("<object returns empty description>"));
  printf_filtered ("\n");
}

/* The data structure 'methcalls' is used to detect method calls (thru
 * ObjC runtime lib functions objc_msgSend, objc_msgSendSuper, etc.),
 * and ultimately find the method being called. 
 */

struct objc_methcall {
  char *name;
  /* Return instance method to be called.  */
  int (*stop_at) (CORE_ADDR, CORE_ADDR *);
  /* Start of pc range corresponding to method invocation.  */
  CORE_ADDR begin;
  /* End of pc range corresponding to method invocation.  */
  CORE_ADDR end;
};

static int resolve_msgsend (CORE_ADDR pc, CORE_ADDR *new_pc);
static int resolve_msgsend_stret (CORE_ADDR pc, CORE_ADDR *new_pc);
static int resolve_msgsend_super (CORE_ADDR pc, CORE_ADDR *new_pc);
static int resolve_msgsend_super_stret (CORE_ADDR pc, CORE_ADDR *new_pc);

static int resolve_msgsend_fixup (CORE_ADDR pc, CORE_ADDR *new_pc);
static int resolve_msgsend_fixedup (CORE_ADDR pc, CORE_ADDR *new_pc);
static int resolve_msgsend_stret_fixup (CORE_ADDR pc, CORE_ADDR *new_pc);
static int resolve_msgsend_stret_fixedup (CORE_ADDR pc, CORE_ADDR *new_pc);
static int resolve_msgsendsuper2_fixup (CORE_ADDR pc, CORE_ADDR *new_pc);
static int resolve_msgsendsuper2_fixedup (CORE_ADDR pc, CORE_ADDR *new_pc);
static int resolve_msgsendsuper2_stret_fixup (CORE_ADDR pc, CORE_ADDR *new_pc);
static int resolve_msgsendsuper2_stret_fixedup (CORE_ADDR pc, CORE_ADDR *new_pc);
static int resolve_msgsendsuper2 (CORE_ADDR pc, CORE_ADDR *new_pc);
static int resolve_msgsendsuper2_stret (CORE_ADDR pc, CORE_ADDR *new_pc);

static struct objc_methcall methcalls[] = {
  { "_objc_msgSend", resolve_msgsend, 0, 0},
  { "_objc_msgSend_rtp", resolve_msgsend, 0, 0},
  { "_objc_msgSend_stret", resolve_msgsend_stret, 0, 0},
  { "_objc_msgSendSuper", resolve_msgsend_super, 0, 0},
  { "_objc_msgSendSuper_stret", resolve_msgsend_super_stret, 0, 0},
  { "_objc_getClass", NULL, 0, 0},
  { "_objc_getMetaClass", NULL, 0, 0},

  { "_objc_msgSend_fixup", resolve_msgsend_fixup, 0, 0},
  { "_objc_msgSend_fixedup", resolve_msgsend_fixedup, 0, 0},
  { "_objc_msgSend_stret_fixup", resolve_msgsend_stret_fixup, 0, 0},
  { "_objc_msgSend_stret_fixedup", resolve_msgsend_stret_fixedup, 0, 0},

  { "_objc_msgSendSuper2", resolve_msgsendsuper2, 0, 0},
  { "_objc_msgSendSuper2_fixup", resolve_msgsendsuper2_fixup, 0, 0},
  { "_objc_msgSendSuper2_fixedup", resolve_msgsendsuper2_fixedup, 0, 0},
  { "_objc_msgSendSuper2_stret", resolve_msgsendsuper2_stret, 0, 0},
  { "_objc_msgSendSuper2_stret_fixup", resolve_msgsendsuper2_stret_fixup, 0, 0},
  { "_objc_msgSendSuper2_stret_fixedup", resolve_msgsendsuper2_stret_fixedup, 0, 0}
};

#define nmethcalls (sizeof (methcalls) /  sizeof (methcalls[0]))

/* APPLE LOCAL: Have we already cached the locations of the objc_msgsend
   functions?  Set to zero if new objfiles have loaded so our cache might
   be dirty.  */
static int cached_objc_msgsend_table_is_valid = 0;

/* The following function, "find_objc_msgsend", fills in the data
 * structure "objc_msgs" by finding the addresses of each of the
 * (currently four) functions that it holds (of which objc_msgSend is
 * the first).  This must be called each time symbols are loaded, in
 * case the functions have moved for some reason.  
 */

static void 
find_objc_msgsend (void)
{
  unsigned int i;

  /* APPLE LOCAL cached objc msgsend table */
  if (cached_objc_msgsend_table_is_valid)
    return;

  for (i = 0; i < nmethcalls; i++) 
    {
      struct minimal_symbol *func, *orig_func;

      /* Try both with and without underscore.  */
      func = lookup_minimal_symbol (methcalls[i].name, NULL, NULL);
      if (func == NULL && methcalls[i].name[0] == '_') 
        func = lookup_minimal_symbol (methcalls[i].name + 1, NULL, NULL);

      if (func == NULL) 
        {
          methcalls[i].begin = 0;
          methcalls[i].end = 0;
          continue; 
        }
    
      methcalls[i].begin = SYMBOL_VALUE_ADDRESS (func);
      orig_func = func;

      if (find_pc_partial_function_no_inlined (SYMBOL_VALUE_ADDRESS (func), NULL,
					       NULL,
					       &methcalls[i].end) == 0)
        {
          methcalls[i].end = methcalls[i].begin + 1;
        }
    }

  /* APPLE LOCAL cached objc msgsend table */
  cached_objc_msgsend_table_is_valid = 1;
}

/* APPLE LOCAL: When a new objfile is added to the system, let's 
   re-search for the msgsend calls in case, um, somehow things have moved 
   around.  (or maybe they were not present earlier, but are now.)  */

void
tell_objc_msgsend_cacher_objfile_changed (struct objfile *obj /* __attribute__ ((__unused__)) */)
{
  cached_objc_msgsend_table_is_valid = 0;

  /* I'm also hijacking this to clear out the state of the objc trampoline
     stuff if the changed objfile is libobjc.A.dylib.  This should only happen if
     you're working on libobjc, but still...  */

  if (strstr (obj->name, objc_library_name) != NULL)
    {
      cached_objc_objfile = NULL;
      objc_clear_trampoline_data ();
    }
}
/* APPLE LOCAL end */

/* find_objc_msgcall (replaces pc_off_limits)
 *
 * ALL that this function now does is to determine whether the input
 * address ("pc") is the address of one of the Objective-C message
 * dispatch functions (mainly objc_msgSend or objc_msgSendSuper), and
 * if so, it returns the address of the method that will be called.
 *
 * The old function "pc_off_limits" used to do a lot of other things
 * in addition, such as detecting shared library jump stubs and
 * returning the address of the shlib function that would be called.
 * That functionality has been moved into the SKIP_TRAMPOLINE_CODE and
 * IN_SOLIB_TRAMPOLINE macros, which are resolved in the target-
 * dependent modules.  
 */

struct objc_submethod_helper_data {
  int (*f) (CORE_ADDR, CORE_ADDR *);
  CORE_ADDR pc;
  CORE_ADDR *new_pc;
};

static int 
find_objc_msgcall_submethod_helper (void * arg)
{
  struct objc_submethod_helper_data *s = 
    (struct objc_submethod_helper_data *) arg;

  if (s->f (s->pc, s->new_pc) == 0) 
    return 1;
  else 
    return 0;
}

static int 
find_objc_msgcall_submethod (int (*f) (CORE_ADDR, CORE_ADDR *),
			     CORE_ADDR pc, 
			     CORE_ADDR *new_pc)
{
  struct objc_submethod_helper_data s;

  s.f = f;
  s.pc = pc;
  s.new_pc = new_pc;

  if (catch_errors (find_objc_msgcall_submethod_helper,
		    (void *) &s,
		    "Unable to determine target of Objective-C method call (ignoring):\n",
		    RETURN_MASK_ALL) == 0) 
    return 1;
  else 
    return 0;
}

int 
find_objc_msgcall (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  unsigned int i;
  unsigned int flags;

  find_objc_msgsend ();
  if (new_pc != NULL)
    *new_pc = 0;

  for (i = 0; i < nmethcalls; i++) 
    if (pc >= methcalls[i].begin && pc < methcalls[i].end)
      {
        if (methcalls[i].stop_at != NULL) 
          return find_objc_msgcall_submethod (methcalls[i].stop_at, pc, new_pc);
        else 
          return 0;
      }

  /* Next look this up in the vtable trampolines.  */
  if (pc_in_objc_trampoline_p (pc, &flags))
    {
      /* If this doesn't behave like an objc_msgSend_fixedup and is
	 the "vtable" type, I don't know what to do with it.  */
      if (flags & (OBJC_TRAMPOLINE_MESSAGE | OBJC_TRAMPOLINE_VTABLE))
	{
	  if (flags & OBJC_TRAMPOLINE_STRET)
	    return resolve_msgsend_stret_fixedup (pc, new_pc);
	  else
	    return resolve_msgsend_fixedup (pc, new_pc);
	}
    }
  return 0;
}

extern initialize_file_ftype _initialize_objc_language; /* -Wmissing-prototypes */

void
_initialize_objc_language (void)
{
  add_language (&objc_language_defn);
  add_language (&objcplus_language_defn);
  add_info ("selectors", selectors_info,    /* INFO SELECTORS command.  */
	    _("All Objective-C selectors, or those matching REGEXP."));
  add_info ("classes", classes_info, 	    /* INFO CLASSES   command.  */
	    _("All Objective-C classes, or those matching REGEXP."));
  add_com ("print-object", class_vars, print_object_command, 
	   _("Ask an Objective-C object to print itself."));
  add_com_alias ("po", "print-object", class_vars, 1);
  add_setshow_uinteger_cmd ("objc-class-method-limit", class_obscure,
			    &objc_class_method_limit,
			    "Set the maximum number of class methods we scan before deciding we are looking at an uninitialized object.",
			    "Show the maximum number of class methods we scan before deciding we are looking at an uninitialized object.",
			    NULL,
			    NULL, NULL, 
			    &setlist, &showlist);
}

/* In 64-bit programs the ObjC runtime uses a different layout for
   its internal data structures.  In the future, 32-bit ObjC runtimes
   may also switch over to this layout.  */

static int 
new_objc_runtime_internals ()
{
  if (objc_runtime_version == 1)
    return 0;
  else if (objc_runtime_version == 2)
    return 1;

#if defined (TARGET_ARM)
  return 1;
#else
  if (TARGET_ADDRESS_BYTES == 8)
    return 1;
  else
    return 0;
#endif
}

/* The first stage in calling any ObjC 2.0 functions passing in a
   class name is to validate that this class really IS a class.  We
   can't just call into the normal runtime methods to see if the class
   exists, since if the class is bogus that might take the runtime
   lock, then crash, leaving the program wedged.  The runtime provides
   class_getClass that will do anything that might crash before taking
   the lock.  We call that first.  

   INFARGS is a pointer to a gdb value containg the class pointer.

   Note: if we can't find the "class_getClass" function, we'll return a
   zero from this call.  It's too dangerous to try to call
   class_getName on an uninitialized object, since it can crash the
   runtime AFTER it's taken the ObjC lock, which will bring the target
   to a screeching halt.  So we treat not finding the validation function
   the same as finding an invalid class pointer.
   */

static CORE_ADDR
new_objc_runtime_class_getClass (struct value *infargs)
{
  static struct cached_value *validate_function = NULL;
  static int already_warned = 0;
  struct value *ret_value = NULL;

  if (validate_function == NULL)
    {
      if (lookup_minimal_symbol ("gdb_class_getClass", 0, 0))
	  validate_function = create_cached_function ("gdb_class_getClass",
						      builtin_type_voidptrfuncptr);
      else
	{
	  if (!already_warned)
	    {
	      already_warned = 1;
	      warning ("Couldn't find class validation function, calling methods on"
		       " uninitialized objects may deadlock your program.");
	    }
	}
    }

  if (validate_function != NULL)
    {
      struct gdb_exception e;
      int old_timeout = set_hand_function_call_timeout (500000);
      
      TRY_CATCH (e, RETURN_MASK_ALL)
	{		    
	  ret_value = call_function_by_hand
	    (lookup_cached_function (validate_function),
	     1, &infargs);
	}

      set_hand_function_call_timeout (old_timeout);
      
      if (e.reason != NO_ERROR || ret_value == NULL)
	{
	  if (hand_function_call_timeout_p ())
	    warning ("Call to get object type timed out.  Most likely somebody has the objc runtime lock.");
	  return (CORE_ADDR) 0;	  
	}
      else
	return (CORE_ADDR) value_as_address (ret_value);
    }
  else
    return (CORE_ADDR) 0;
}

/* Do an inferior function call into the Objective-C runtime to find
   the function address of a given class + selector.  

   This function bears a remarkable resemblance to lookup_objc_class().
   It uses a function call to do what find_implementation_from_class()
   does by groping around in the old ObjC runtime's internal data structures
   did.

   This function will return 0 if the runtime is uninitialized or 
   the runtime is not present.  */

static CORE_ADDR
new_objc_runtime_find_impl (CORE_ADDR class, CORE_ADDR sel, int stret)
{
  static struct cached_value *function = NULL;
  static struct cached_value *function_stret = NULL;
  static struct cached_value *function_cache_getImp = NULL;
  struct value *ret_value;
  struct cleanup *scheduler_cleanup;
  CORE_ADDR retval = 0;
  enum objc_debugger_mode_result objc_retval;

  if (!target_has_execution)
    {
      /* Can't call into inferior to lookup class.  */
      return 0;
    }

  if (stret == 0 && function == NULL)
    {
      if (lookup_minimal_symbol ("class_getMethodImplementation", 0, 0))
        function = create_cached_function ("class_getMethodImplementation",
                                           builtin_type_voidptrfuncptr);
      else
        return 0;
    }

  if (stret == 1 && function_stret == NULL)
    {
      if (lookup_minimal_symbol ("class_getMethodImplementation_stret", 0, 0))
        function_stret = create_cached_function 
                                       ("class_getMethodImplementation_stret", 
                                        builtin_type_voidptrfuncptr);
      else
        return 0;
    }

  /* Lock the scheduler before calling this so the other threads
     don't make progress while you are running this.  */

  scheduler_cleanup = make_cleanup_set_restore_scheduler_locking_mode 
                      (scheduler_locking_on);

  make_cleanup_set_restore_unwind_on_signal (1);

  /* find_impl is never called directly by the user, so we should not
     tell her if something goes wrong, we should just clean up and 
     handle the error appropriately.  */
  make_cleanup_ui_out_suppress_output (uiout);

  objc_retval = make_cleanup_set_restore_debugger_mode (NULL, 0);
  if (retval == objc_debugger_mode_fail_objc_api_unavailable)
    if (target_check_safe_call (OBJC_SUBSYSTEM, CHECK_SCHEDULER_VALUE))
      retval = objc_debugger_mode_success;

  if (objc_retval == objc_debugger_mode_success)
    {
      struct value *classval, *selval;
      struct value *infargs[2];
      char *imp_name;
      struct cleanup *value_cleanup;

      classval = value_from_pointer (lookup_pointer_type 
                                          (builtin_type_void_data_ptr), class);
      selval = value_from_pointer (lookup_pointer_type 
                                          (builtin_type_void_data_ptr), sel);

      /* Have to release these values since we intend to use them
	 across more than one call into the target. */

      release_value (classval);
      release_value (selval);

      value_cleanup = make_cleanup ((make_cleanup_ftype *) value_free, classval);
      make_cleanup ((make_cleanup_ftype *) value_free, selval);

      infargs[0] = classval;
      infargs[1] = selval;
      
      retval = new_objc_runtime_class_getClass (classval);
      if (retval == 0)
	  goto cleanups;

      ret_value = call_function_by_hand
                  (lookup_cached_function (stret ? function_stret : function),
                   2, infargs);

      do_cleanups (value_cleanup);

      retval = (CORE_ADDR) value_as_address (ret_value);
      retval = gdbarch_addr_bits_remove (current_gdbarch, retval);
      /* If the current object doesn't respond to this selector, then
	 the implementation function will be "_objc_msgForward" or
	 "_objc_msgForward_stret".  We don't want to call those since
	 it is most likely just going to crash, and Greg says that not
	 all implementations of objc_msgForward follow the standard
	 ABI anyway...  */
      if (find_pc_partial_function_no_inlined (retval, &imp_name, NULL, NULL))
	{
	  if (strcmp (imp_name, "_objc_msgForward") == 0
	      || strcmp (imp_name, "_objc_msgForward_stret") == 0)
	    {
	      retval = 0;
	      goto cleanups;
	    }
	}

      add_implementation_to_cache (class, sel, retval);
    }
  else
    {
      if (objc_retval == objc_debugger_mode_fail_malloc_lock_held)
        {
          /* Something is holding a malloc/objc runtime lock so we can't call
             into the usual method lookup functions.  The objc runtime might
             already know this class/selector - see if it can tell us the
             method address by just looking in its existing cache tables.  */

          /* Temporarily force the debugger_mode function to return an "OK"
             result code for the duration of our hand function call below.  */

          struct cleanup *make_call_cleanup;
          make_cleanup_set_restore_debugger_mode (&make_call_cleanup, -1);

          struct value *classval, *selval;
          struct value *infargs[2];
          char *imp_name;

          if (function_cache_getImp == NULL)
            {
              if (lookup_minimal_symbol ("_cache_getImp", 0, 0))
                function_cache_getImp = create_cached_function ("_cache_getImp",
                                                   builtin_type_voidptrfuncptr);
              else
                {
                  retval = 0;
                  goto cleanups;
                }
            }

          classval = value_from_pointer (lookup_pointer_type
                                          (builtin_type_void_data_ptr), class);
          selval = value_from_pointer (lookup_pointer_type
                                          (builtin_type_void_data_ptr), sel);
          infargs[0] = classval;
          infargs[1] = selval;

          ret_value = call_function_by_hand
                  (lookup_cached_function (function_cache_getImp), 2, infargs);

          retval = (CORE_ADDR) value_as_address (ret_value);
          retval = gdbarch_addr_bits_remove (current_gdbarch, retval);
    
          if (retval != 0)
            {
              if (find_pc_partial_function_no_inlined (retval, 
                                                       &imp_name, NULL, NULL))
                {
                  if (strcmp (imp_name, "_objc_msgForward") == 0
                      || strcmp (imp_name, "_objc_msgForward_stret") == 0)
                    {
                      retval = 0;
                      goto cleanups;
                    }
                }         
              add_implementation_to_cache (class, sel, retval);
            }
          do_cleanups (make_call_cleanup);
        }
      else
        {
          warning ("Not safe to look up objc runtime data.");
        }
    }

 cleanups:
  do_cleanups (scheduler_cleanup);
  return retval;
}


static void 
read_objc_method (CORE_ADDR addr, struct objc_method *method)
{
  int addrsize = TARGET_ADDRESS_BYTES;
  method->name  = read_memory_unsigned_integer (addr + 0, addrsize);
  method->types = read_memory_unsigned_integer (addr + addrsize, addrsize);
  method->imp   = read_memory_unsigned_integer (addr + addrsize * 2, addrsize);
}

static unsigned long 
read_objc_method_list_nmethods (CORE_ADDR addr)
{
  int addrsize = TARGET_ADDRESS_BYTES;
  return read_memory_unsigned_integer (addr + addrsize, 4);
}

static void 
read_objc_method_list_method (CORE_ADDR addr, unsigned long num, 
                              struct objc_method *method)
{
  int addrsize = TARGET_ADDRESS_BYTES;
  int offset;

  /* 64-bit objc runtime has an extra field in here.  */
  if (addrsize == 8)
    offset = addrsize + 4 + 4;
  else
    offset = addrsize + 4;

  gdb_assert (num < read_objc_method_list_nmethods (addr));
  read_objc_method (addr + offset + (3 * addrsize * num), method);
}
  
static void 
read_objc_object (CORE_ADDR addr, struct objc_object *object)
{
  int addrsize = TARGET_ADDRESS_BYTES;
  object->isa = read_memory_unsigned_integer (addr, addrsize);
}

static void 
read_objc_super (CORE_ADDR addr, struct objc_super *super)
{
  int addrsize = TARGET_ADDRESS_BYTES;
  super->receiver = read_memory_unsigned_integer (addr, addrsize);
  super->class = read_memory_unsigned_integer (addr + addrsize, addrsize);
};

/* Read a 'struct objc_class' in the inferior's objc runtime.  */

static void 
read_objc_class (CORE_ADDR addr, struct objc_class *class)
{
  int addrsize = TARGET_ADDRESS_BYTES;
  class->isa = read_memory_unsigned_integer (addr, addrsize);
  class->super_class = read_memory_unsigned_integer (addr + addrsize, addrsize);
  class->name = read_memory_unsigned_integer (addr + addrsize * 2, addrsize);
  class->version = read_memory_unsigned_integer (addr + addrsize * 3, addrsize);
  class->info = read_memory_unsigned_integer (addr + addrsize * 4, addrsize); 
  class->instance_size = read_memory_unsigned_integer 
                                               (addr + addrsize * 5, addrsize);
  class->ivars = read_memory_unsigned_integer (addr + addrsize * 6, addrsize);
  class->methods = read_memory_unsigned_integer (addr + addrsize * 7, addrsize);
  class->cache = read_memory_unsigned_integer (addr + addrsize * 8, addrsize);
  class->protocols = read_memory_unsigned_integer 
                                                (addr + addrsize * 9, addrsize);
}

/* When the ObjC garbage collection has a selector we should ignore
   it will have the address of 0xfffeb010 for its name on a little-endian
   machine.  */
#define GC_IGNORED_SELECTOR_LE 0xfffeb010

/* APPLE LOCAL: Build a cache of the (class,selector)->implementation lookups
   that we do.  These are actually fairly expensive, and for inspecting ObjC
   objects, we tend to do the same ones over & over.  */

static void
free_rb_tree_data (struct rb_tree_node *root, void (*free_fn) (void *))
{
  if (root->left)
    free_rb_tree_data (root->left, free_fn);

  if (root->right)
    free_rb_tree_data (root->right, free_fn);

  if (free_fn != NULL)
    free_fn (root->data);
  xfree (root);
}

struct objfile *
find_libobjc_objfile ()
{
  if (cached_objc_objfile == NULL)
    cached_objc_objfile = executable_objfile (find_objfile_by_name (objc_library_name, 0));
  return cached_objc_objfile;
}

void 
reinitialize_objc ()
{
  objc_clear_caches ();
  objc_clear_trampoline_data ();
  debug_mode_set_p = debug_mode_not_checked;
  debug_mode_set_reason = objc_debugger_mode_unknown;
  cached_objc_objfile = NULL;
}

void
objc_clear_caches ()
{
  if (implementation_tree != NULL)
    {
      free_rb_tree_data (implementation_tree, xfree);
      implementation_tree = NULL;
    }
  if (classname_tree != NULL)
    {
      free_rb_tree_data (classname_tree, xfree);
      classname_tree = NULL;
    }
}

static CORE_ADDR 
lookup_implementation_in_cache (CORE_ADDR class, CORE_ADDR sel)
{
  struct rb_tree_node *found;

  found = rb_tree_find_node_all_keys (implementation_tree, class, -1, sel);
  if (found == NULL)
    return 0;
  else
      return *((CORE_ADDR *) found->data);
}

static void
add_implementation_to_cache (CORE_ADDR class, CORE_ADDR sel, CORE_ADDR implementation)
{
  struct rb_tree_node *new_node = (struct rb_tree_node *) xmalloc (sizeof (struct rb_tree_node));
  new_node->key = class;
  new_node->secondary_key = -1;
  new_node->third_key = sel;
  new_node->data = xmalloc (sizeof (CORE_ADDR));
  *((CORE_ADDR *) new_node->data) = implementation;
  new_node->left = NULL;
  new_node->right = NULL;
  new_node->parent = NULL;
  new_node->color = UNINIT;

  rb_tree_insert (&implementation_tree, implementation_tree, new_node);
}

static CORE_ADDR
find_implementation_from_class (CORE_ADDR class, CORE_ADDR sel)
{
  CORE_ADDR subclass = class;
  char sel_str[2048];
  int npasses;
  int addrsize = TARGET_ADDRESS_BYTES;
  int total_methods = 0;
  CORE_ADDR implementation;

  sel_str[0] = '\0';

   implementation = lookup_implementation_in_cache (class, sel);
   if (implementation != 0)
     return implementation;

  if (new_objc_runtime_internals ())
    return (new_objc_runtime_find_impl (class, sel, 0));

  if (sel == GC_IGNORED_SELECTOR_LE)
    return 0;

  /* Before we start trying to look at methods, let's quickly
     check to see that the class hierarchy for the pointer we've
     been given looks sane.  We tried to think of ways to check the
     memory we are grubbing through to see if it looks good, but
     there was no sure way to do that.  So the best solution is to
     look up the class by name, and make sure the address we got
     back is the same as the one we already had.  */
  {
    /* Here's a quick way to make sure that this pointer really is
       class.  Call objc_lookUpClass on it, and if we find the class,
       then it is real and safe to grub around in.  */
    CORE_ADDR class_addr;
    struct objc_class class_str;
    char class_name[2048];
    char *ptr;
    struct gdb_exception e;

    TRY_CATCH (e, RETURN_MASK_ALL)
      {
	read_objc_class (class, &class_str);
	read_memory_string (class_str.name, class_name, 2048);
      }
    if (e.reason != NO_ERROR)
      {
	if (info_verbose)
	  {
	    printf_unfiltered ("Got error reading class or its name.\n");
	  }
	return 0;
      }

    for (ptr = class_name; *ptr != '\0'; ptr++)
      {
	if (!isprint (*ptr))
	  {
	    if (info_verbose)
	      {
		printf_unfiltered ("Class name \"%s\" contains a non-printing character.\n", class_name);
	      }
	    return 0;
	  }
      }

    class_addr = lookup_objc_class (class_name);
    if (class_addr == 0)
      {
	if (info_verbose)
	  printf_unfiltered ("Could not look up class for name: \"%s\".\n", class_name);
	return 0;
      }
    else 
      {
        /* We used to compare the CLASS and VERIFY_STR class object addresses
           but this would fail if we're looking at a metaclass object - when
           we call lookup_objc_class on the metaclass name, we get the class.
           The names of the classes will be the same, so we'll use that.  We
           could have also tested to see if the 'super_class' field of one
           matched the other.  */
        struct objc_class verify_str;
        TRY_CATCH (e, RETURN_MASK_ALL)
          {
            read_objc_class (class, &verify_str);
          }
        if (e.reason == NO_ERROR && verify_str.name != class_str.name)
          {  
	    if (info_verbose)
	      printf_unfiltered ("Class address for name: \"%s\": 0x%s didn't match input address: 0x%s.\n", 
		       class_name, paddr_nz (class_addr), paddr_nz (class));
	    return 0;
          }
      }
  }  

  /* Okay, now let's look for real.  */

  while (subclass != 0) 
    {

      struct objc_class class_str;
      unsigned mlistnum = 0;
      int class_initialized;

      read_objc_class (subclass, &class_str);

      class_initialized = ((class_str.info & 0x4L) == 0x4L);
      if (!class_initialized)
	{
	  if (sel_str[0] == '\0')
	    {
	      read_memory_string (sel, sel_str, 2047);
	    }
	}

     if (info_verbose)
       {
	 char buffer[2048];
	 read_memory_string (class_str.name, buffer, 2047);
         buffer[2047] = '\0';
	 printf_filtered ("Reading methods for %s, info is 0x%s\n", buffer, paddr_nz (class_str.info));
       }

#define CLS_NO_METHOD_ARRAY 0x4000

      npasses = 0;

      for (;;) 
	{
	  CORE_ADDR mlist;
	  unsigned long nmethods;
	  unsigned long i;
	  npasses++;

	  /* As an optimization, if the ObjC runtime can tell that 
	     a class won't need extra fields methods, it will make
	     the method list a static array of method's.  Otherwise
	     it will be a pointer to a list of arrays, so that the
	     runtime can augment the method list in chunks.  There's
	     a bit in the info field that tells which way this works. 
	     Also, if a class has NO methods, then the methods field
	     will be null.  */

	  if (class_str.methods == 0x0)
	    break;
	  else if (class_str.info & CLS_NO_METHOD_ARRAY)
	    {
	      if (npasses == 1)
		mlist = class_str.methods;
	      else
		break;
	    }
	  else
	    {
	      mlist = read_memory_unsigned_integer 
                          (class_str.methods + (addrsize * mlistnum), addrsize);

	      /* The ObjC runtime uses NULL to indicate then end of the
		 method chunk pointers within an allocation block,
		 and -1 for the end of an allocation block.  */

	      if (mlist == 0 || mlist == 0xffffffff 
                  || mlist == 0xffffffffffffffffULL)
		break;
	    }

	  nmethods = read_objc_method_list_nmethods (mlist);

	  for (i = 0; i < nmethods; i++) 
	    {
	      struct objc_method meth_str;
	      char name_str[2048];

	      if (++total_methods >= objc_class_method_limit)
		{
		  static int only_warn_once = 0;
		  if (only_warn_once == 0)
		    warning ("Read %d potential method entries, probably looking "
			     "at an unitialized object.\n"
			     "Set objc-class-method-limit to higher value if your class"
			     " really has this many methods.",
			     total_methods);
		  only_warn_once++;
		  return 0;
		}

	      read_objc_method_list_method (mlist, i, &meth_str);

              /* The GC_IGNORED_SELECTOR_LE  bit pattern indicates
                 that the selector is the ObjC GC's way of telling
                 itself that the selector is ignored.  It may not
                 point to valid memory in the inferior so
                 read_memory_string'ing it will likely fail.  */

              if (meth_str.name == GC_IGNORED_SELECTOR_LE)
                continue;

	      if (!class_initialized)
		read_memory_string (meth_str.name, name_str, 2047);
#if 0
	      if (class_initialized)
		read_memory_string (meth_str.name, name_str, 2047);
	      fprintf (stderr, 
		       "checking method 0x%lx (%s) against selector 0x%lx\n", 
		       (long unsigned int) meth_str.name, name_str, (long unsigned int) sel);
#endif

	      /* The first test relies on the selectors being uniqued across shared
		 library boundaries.  But this uniquing is done lazily when the
		 class is initialized, so if the class is not yet initialized,
		 we should also directly compare the selector strings.  */

	      if (meth_str.name == sel || (!class_initialized 
					   && (name_str[0] == sel_str[0])
					   && (strcmp (name_str, sel_str) == 0))) 
		/* FIXME: hppa arch was doing a pointer dereference
		   here. There needs to be a better way to do that.  */
		{
		  add_implementation_to_cache (class, sel, meth_str.imp);
		  return meth_str.imp;
		}
	    }
	  mlistnum++;
	}
      subclass = class_str.super_class;
    }
  
  return 0;
}

CORE_ADDR
find_implementation (CORE_ADDR object, CORE_ADDR sel, int stret)
{
  struct objc_object ostr;
  /* APPLE LOCAL use '[object class]' rather than isa  */
  CORE_ADDR real_class_addr;

  if (object == 0)
    return 0;
  read_objc_object (object, &ostr);
  if (ostr.isa == 0)
    return 0;

  real_class_addr = ostr.isa;  /* a default value which we may override below */

  if (new_objc_runtime_internals ())
    {
      CORE_ADDR resolves_to;
      struct cleanup *cleanup;
      resolves_to = lookup_implementation_in_cache (ostr.isa, sel);
      if (resolves_to != 0)
	return resolves_to;

      /* If there is a malloc lock being held we can't do the 
         get_class_address_from_object () call below - that requires a
         generic inferior function call to be able to run - but we can
         use a special fallback method in new_objc_runtime_find_impl.  So
         if the malloc lock is held, we'll just assume that there isn't some
         KVO interposer that this method should be dispatched to or anything.
         It isn't really correct but it's probably better than failing 
         outright. */
      if (make_cleanup_set_restore_debugger_mode (&cleanup, 1) == 
                                      objc_debugger_mode_fail_malloc_lock_held)
        {
          real_class_addr = ostr.isa;
        }
      else
        {
          /* APPLE LOCAL begin use '[object class]' rather than isa  */
          real_class_addr = get_class_address_from_object (object);
          if (real_class_addr != ostr.isa)
	    {
	      resolves_to = lookup_implementation_in_cache (real_class_addr, 
                                                            sel);
	      if (resolves_to != 0)
	        {
	          add_implementation_to_cache (ostr.isa, sel, resolves_to);
                  do_cleanups (cleanup);
	          return resolves_to;
	        }
	    }
        }
      do_cleanups (cleanup);

      /* APPLE LOCAL use '[object class]' rather than isa  */
      resolves_to = new_objc_runtime_find_impl (real_class_addr, sel, stret);
      if (resolves_to != 0)
        {
	  if (real_class_addr != ostr.isa)
	    add_implementation_to_cache (ostr.isa, sel, resolves_to);
          return (resolves_to);
        }
    }

  /* APPLE LOCAL begin use '[object class]' rather than isa  */
  if (new_objc_runtime_internals ())
    return find_implementation_from_class (real_class_addr, sel);
  else
    return find_implementation_from_class (ostr.isa, sel);
  /* APPLE LOCAL end use '[object class]' rather than isa  */
}

/* Resolve an objc_msgSend dispatch trampoline in a 32-bit Objective-C
   runtime.  */

static int
resolve_msgsend (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  CORE_ADDR object;
  CORE_ADDR sel;
  CORE_ADDR res;

  object = OBJC_FETCH_POINTER_ARGUMENT (0);
  sel = OBJC_FETCH_POINTER_ARGUMENT (1);

  res = find_implementation (object, sel, 0);
  if (new_pc != 0)
    *new_pc = res;
  if (res == 0)
    return 1;
  return 0;
}

/* Find the target function of an Objective-C dispatch in the new
   64-bit ObjC runtime.
   Returns 0 if it succeeded in finding the target function and sets
   *NEW_PC to the address of that function.
   Returns 1 if it failed and sets *NEW_PC to 0.  */

static int 
resolve_newruntime_objc_msgsend (CORE_ADDR pc, CORE_ADDR *new_pc,
                                 int fixedup, int stret)
{
  CORE_ADDR object;
  CORE_ADDR sel;
  CORE_ADDR res;
  int addrsize = TARGET_ADDRESS_BYTES;

  if (stret == 0)
    {
      object = OBJC_FETCH_POINTER_ARGUMENT (0);
      sel = OBJC_FETCH_POINTER_ARGUMENT (1);
    }
  else
    {
      object = OBJC_FETCH_POINTER_ARGUMENT (1);
      sel = OBJC_FETCH_POINTER_ARGUMENT (2);
    }
  
  /* SEL points to a two-word structure - we want the second word of that
     structure.  */

  sel = read_memory_unsigned_integer (sel + addrsize, addrsize);

  /* First call to one of these classes' methods?  In that case instead of
     SEL being the selector number, it's the address of the string of the
     selector name.  */

  if (fixedup == 0)
    {
      char selname[2048];
      selname[0] = '\0';
      read_memory_string (sel, selname, 2047);
      sel = lookup_child_selector (selname);
    }

  res = find_implementation (object, sel, stret);
  if (new_pc != 0)
    *new_pc = res;
  if (res == 0)
    return 1;
  return 0;
}

static int 
resolve_msgsend_fixup (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  return resolve_newruntime_objc_msgsend (pc, new_pc, 0, 0);
}

static int 
resolve_msgsend_fixedup (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  return resolve_newruntime_objc_msgsend (pc, new_pc, 1, 0);
}

static int 
resolve_msgsend_stret_fixup (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  return resolve_newruntime_objc_msgsend (pc, new_pc, 0, 1);
}

static int 
resolve_msgsend_stret_fixedup (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  return resolve_newruntime_objc_msgsend (pc, new_pc, 1, 1);
}


/* Find the target function of an Objective-C super dispatch in the new
   64-bit ObjC runtime.
   FIXEDUP = 0 if we are in the fixup version
             1 if we are in the fixedup version
             -1 if we are in the straight version.

   Returns 0 if it succeeded in finding the target function and sets
   *NEW_PC to the address of that function.
   Returns 1 if it failed and sets *NEW_PC to 0.  */

static int 
resolve_newruntime_objc_msgsendsuper (CORE_ADDR pc, CORE_ADDR *new_pc,
                                      int fixedup, int stret)
{
  CORE_ADDR object;
  CORE_ADDR sel;
  CORE_ADDR res;
  int addrsize = TARGET_ADDRESS_BYTES;
  struct objc_super sstr;

  if (stret == 0)
    {
      object = OBJC_FETCH_POINTER_ARGUMENT (0);
      sel = OBJC_FETCH_POINTER_ARGUMENT (1);
    }
  else
    {
      object = OBJC_FETCH_POINTER_ARGUMENT (1);
      sel = OBJC_FETCH_POINTER_ARGUMENT (2);
    }
  
  /* For the fixedup & fixup versions, SEL points to a two-word structure - we
     want the second word of that structure.  For the straight version SEL
     is the straight selector.  */

  if (fixedup != -1)
    sel = read_memory_unsigned_integer (sel + addrsize, addrsize);

  /* First call to one of these classes' methods?  In that case instead of
     SEL being the selector number, it's the address of the string of the
     selector name.  */

  if (fixedup == 0)
    {
      char selname[2048];
      selname[0] = '\0';
      read_memory_string (sel, selname, 2047);
      sel = lookup_child_selector (selname);
    }

  /* OBJECT is the address of a two-word struct.
     The second word is the address of the super class.  */
  object = read_memory_unsigned_integer (object + addrsize, addrsize);

  read_objc_super (object, &sstr);

  res = lookup_implementation_in_cache (sstr.class, sel);
  if (res != 0)
    return res;
  res = new_objc_runtime_find_impl (sstr.class, sel, 0);
  if (new_pc != 0)
    *new_pc = res;
  if (res == 0)
    return 1;
  return 0;
}

static int 
resolve_msgsendsuper2_fixup (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  return resolve_newruntime_objc_msgsendsuper (pc, new_pc, 0, 0);
}

static int 
resolve_msgsendsuper2_fixedup (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  return resolve_newruntime_objc_msgsendsuper (pc, new_pc, 1, 0);
}

static int 
resolve_msgsendsuper2_stret_fixup (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  return resolve_newruntime_objc_msgsendsuper (pc, new_pc, 0, 1);
}

static int 
resolve_msgsendsuper2_stret_fixedup (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  return resolve_newruntime_objc_msgsendsuper (pc, new_pc, 1, 1);
}

static int 
resolve_msgsendsuper2 (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  return resolve_newruntime_objc_msgsendsuper (pc, new_pc, -1, 0);
}

static int 
resolve_msgsendsuper2_stret (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  return resolve_newruntime_objc_msgsendsuper (pc, new_pc, -1, 1);
}

/* Resolve an objc_msgSend_stret dispatch trampoline in a 32-bit Objective-C
   runtime.  */

static int
resolve_msgsend_stret (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  CORE_ADDR object;
  CORE_ADDR sel;
  CORE_ADDR res;

  object = OBJC_FETCH_POINTER_ARGUMENT (1);
  sel = OBJC_FETCH_POINTER_ARGUMENT (2);

  res = find_implementation (object, sel, 1);
  if (new_pc != 0)
    *new_pc = res;
  if (res == 0)
    return 1;
  return 0;
}

/* Resolve an objc_msgSendSuper dispatch trampoline in a 32-bit Objective-C
   runtime.  */

static int
resolve_msgsend_super (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  struct objc_super sstr;

  CORE_ADDR super;
  CORE_ADDR sel;
  CORE_ADDR res;

  super = OBJC_FETCH_POINTER_ARGUMENT (0);
  sel = OBJC_FETCH_POINTER_ARGUMENT (1);

  read_objc_super (super, &sstr);
  if (sstr.class == 0)
    return 0;
  
  res = find_implementation_from_class (sstr.class, sel);
  if (new_pc != 0)
    *new_pc = res;
  if (res == 0)
    return 1;
  return 0;
}

/* Resolve an objc_msgSendSuper_stret dispatch trampoline in a 
   32-bit Objective-C runtime.  */

static int
resolve_msgsend_super_stret (CORE_ADDR pc, CORE_ADDR *new_pc)
{
  struct objc_super sstr;

  CORE_ADDR super;
  CORE_ADDR sel;
  CORE_ADDR res;

  super = OBJC_FETCH_POINTER_ARGUMENT (1);
  sel = OBJC_FETCH_POINTER_ARGUMENT (2);

  read_objc_super (super, &sstr);
  if (sstr.class == 0)
    return 0;
  
  res = find_implementation_from_class (sstr.class, sel);
  if (new_pc != 0)
    *new_pc = res;
  if (res == 0)
    return 1;
  return 0;
}

int
should_lookup_objc_class ()
{
  return lookup_objc_class_p;
}

int
new_objc_runtime_get_classname (CORE_ADDR class,
				char *class_name, int size)
{
  static struct cached_value *cached_class_getName = NULL;
  struct value *classval;
  CORE_ADDR addr;
  struct cleanup *scheduler_cleanup;
  int retval = 0;
  enum objc_debugger_mode_result objc_retval;

  if (cached_class_getName == NULL)
    {
      if (lookup_minimal_symbol ("class_getName", 0, 0))
        cached_class_getName = create_cached_function ("class_getName",
						       builtin_type_voidptrfuncptr);
      else
        return 0;
    }

  /* APPLE LOCAL: Lock the scheduler before calling this so the other threads 
     don't make progress while you are running this.  */
  
  scheduler_cleanup = make_cleanup_set_restore_scheduler_locking_mode 
                      (scheduler_locking_on);

  make_cleanup_set_restore_unwind_on_signal (1);

  /* This function gets called when we are in the middle of outputting
     the MI result for the creation of a varobj.  So if any of these
     function calls crash we want to make sure their output doesn't
     get into the result.  So we make a null uiout from the gdb_null
     file stream, and swap that into the uiout while running the
     function calls.  */
  make_cleanup_ui_out_suppress_output (uiout);

  objc_retval = make_cleanup_set_restore_debugger_mode (NULL, 0);
  if (objc_retval == objc_debugger_mode_fail_objc_api_unavailable)
    if (target_check_safe_call (OBJC_SUBSYSTEM, CHECK_SCHEDULER_VALUE))
      objc_retval = objc_debugger_mode_success;

  if (objc_retval == objc_debugger_mode_success)
    {
      struct value *ret_value;
      struct gdb_exception e;
      
      TRY_CATCH (e, RETURN_MASK_ALL)
	{
	  struct cleanup *value_cleanup;

	  classval = value_from_pointer (lookup_pointer_type 
					 (builtin_type_void_data_ptr), class);
	  
	  release_value (classval);
	  value_cleanup = make_cleanup ((make_cleanup_ftype *) value_free, classval);

	  addr = new_objc_runtime_class_getClass (classval);
	  if (addr != 0)
	    {
	  
	      ret_value = call_function_by_hand (lookup_cached_function (cached_class_getName),
						 1, &classval);
	      addr = value_as_address (ret_value);
	      read_memory_string (addr, class_name, size);
	      retval = 1;
	    }
	  do_cleanups (value_cleanup);

	}
      if (e.reason != NO_ERROR)
	retval = 0;
    }

  do_cleanups (scheduler_cleanup);
  return retval;
}

static char *  
lookup_classname_in_cache (CORE_ADDR class)
{
  struct rb_tree_node *found;

  found = rb_tree_find_node_all_keys (classname_tree, class, -1, -1);
  if (found == NULL)
    return NULL;
  else
    return (char *) found->data;

}

static void 
add_classname_to_cache (CORE_ADDR class, char *classname)
{
  struct rb_tree_node *new_node = (struct rb_tree_node *) xmalloc (sizeof (struct rb_tree_node));

  new_node->key = class;
  new_node->secondary_key = -1;
  new_node->third_key = -1;
  new_node->data = xstrdup (classname);
  new_node->left = NULL;
  new_node->right = NULL;
  new_node->parent = NULL;
  new_node->color = UNINIT;

  rb_tree_insert (&classname_tree, classname_tree, new_node);
}

/* If the value VAL points to an objc object, look up its
   isa pointer, and see if you can find the type for its
   dynamic class type.  Will resolve typedefs etc...  */

/* APPLE LOCAL: This is from objc-class.h:  */
#define CLS_META 0x2L

/* APPLE LOCAL: Extract the code that finds the target type given the address OBJECT_ADDR of
   an objc object.  If BLOCK is provided, the symbol will be looked up in the context of
   that block.  ADDRSIZE is the size of an address on this architecture.  
   If CLASS_NAME is not NULL, then a copy of the class name will be returned in CLASS_NAME.  */

struct type *
objc_target_type_from_object (CORE_ADDR object_addr, 
			      struct block *block, 
			      int addrsize, 
			      char **class_name_ptr)
{
  struct symbol *class_symbol;
  struct type *dynamic_type = NULL;
  char class_name[256];
  char *name_ptr;
  CORE_ADDR name_addr;
  CORE_ADDR isa_addr;
  long info_field;
  int retval = 1;

  if (class_name_ptr != NULL)
    *class_name_ptr = NULL;

  /* APPLE LOCAL begin use '[object class]' rather than isa  */
  if (new_objc_runtime_internals ())
    isa_addr = get_class_address_from_object (object_addr);
  else
    isa_addr = 
      read_memory_unsigned_integer (object_addr, addrsize);
  /* APPLE LOCAL end use '[object class]' rather than isa  */

  /* isa_addr now points to a struct objc_class in the inferior.  */
  
  name_ptr = lookup_classname_in_cache (isa_addr);
  if (name_ptr == NULL)
    {
      name_ptr = class_name;
      if (new_objc_runtime_internals ())
	retval = new_objc_runtime_get_classname (isa_addr, class_name, sizeof (class_name));
      else
	{
	  int i;
	  /* APPLE LOCAL: Don't look up the dynamic type if the isa is the
	     MetaClass class, since then we are looking at the Class object
	     which doesn't have the fields of an object of the class.  */
	  info_field = read_memory_unsigned_integer 
	    (isa_addr + addrsize * 4, addrsize);
	  if (info_field & CLS_META)
	    return NULL;
	  
	  name_addr =  read_memory_unsigned_integer 
	    (isa_addr + addrsize * 2, addrsize);
	  
	  read_memory_string (name_addr, class_name, sizeof (class_name));

	  /* Since there's no guarantee we aren't just sampling
	     random memory here, let's make sure the class name is
	     at least a valid C identifier.  */

	  if (!(isalpha (class_name[0]) || class_name[0] == '_'))
	    retval = 0;
	  else
	    {
	      for (i = 1; i < sizeof (class_name) - 1; i++)
		{
		  if (class_name[i] == '\0')
		    break;

		  if (!(isalnum (class_name[i])
			|| class_name[i] == '_'))
		    {
		      retval = 0;
		      break;
		    }
		}

	      /* We got to the end w/o finding a '\0'...  */
	      if (i == sizeof (class_name) - 1)
		retval = 0;
	    }
	}
      if (retval == 0)
	return NULL;
      add_classname_to_cache (isa_addr, class_name);
    }

  if (class_name_ptr != NULL)
    *class_name_ptr = xstrdup (name_ptr);

  class_symbol = lookup_symbol (name_ptr, block, STRUCT_DOMAIN, 0, 0);

  if (! class_symbol)
      return NULL;
  
  /* Make sure the type symbol is sane.  (An earlier version of this
     code would find constructor functions, who have the same name as
     the class.)  */
  if (SYMBOL_CLASS (class_symbol) != LOC_TYPEDEF
      || TYPE_CODE (SYMBOL_TYPE (class_symbol)) != TYPE_CODE_CLASS)
    {
      warning ("The \"isa\" pointer gives a class name of `%s', but that isn't a type name",
	       name_ptr);
      return NULL;
    }
  
  /* This is the object's run-time type!  */
  dynamic_type = SYMBOL_TYPE (class_symbol);

  return dynamic_type;

}

/* Given a value VAL, look up the dynamic type for the object 
   pointed to by VAL in BLOCK, and return it.  If we can't find
   the full type info for the dynamic type of VAL, but we can find
   the class name, then we will return a malloc'ed copy of the name
   in DYNAMIC_TYPE_HANDLE (if it is not NULL).  Note, if we can find
   the full type info, we will set DYNAMIC_TYPE_HANDLE to NULL.  That
   way, if the return value is non-null, you won't have to free the 
   name string.  
   NOTE: To make this behave like value_rtti_target_type, we return
   NULL if the value isn't a pointer or reference to a class.  */

struct type *
value_objc_target_type (struct value *val, struct block *block,
			char **dynamic_type_handle)
{
  struct type *base_type, *dynamic_type = NULL;
  int addrsize = TARGET_ADDRESS_BYTES;

  if (dynamic_type_handle != NULL)
    *dynamic_type_handle = NULL;

  base_type = check_typedef (value_type (val));
  if (TYPE_CODE (base_type) != TYPE_CODE_PTR
      && TYPE_CODE (base_type) != TYPE_CODE_REF)
    return NULL;

  base_type = check_typedef (TYPE_TARGET_TYPE (base_type));

  /* Don't try to get the dynamic type of an objc_class object.  This is the
     class object, not an instance object, so it won't have the fields the
     instance object has.  Also be careful to check for NULL, since val may be
     a typedef or pointer to an incomplete type.  */

  if ((base_type == NULL) || (TYPE_TAG_NAME (base_type) != NULL 
      && (strcmp (TYPE_TAG_NAME (base_type), "objc_class") == 0)))
    return NULL;

  if (TYPE_CODE (base_type) == TYPE_CODE_CLASS)
    {
      char *t_field_name;
      short nfields;
      
      t_field_name = NULL;
      nfields = TYPE_NFIELDS (base_type); 
      
      /* The situation is a little complicated here.
	 1) With stabs, the ObjC type hierarchy is not represented in the
	 debug info, so the first field is the "isa" field.
	 2) With the early DWARF implementations the base class was
	 the first field of the child class, but it had a NULL name.
	 2) This was corrected so we actually had an inheritance tag,
	 and so TYPE_N_BASECLASSES is now correct.
	 In all cases, we need to check here that there is an "isa"
	 field (so we only try to look up the dynamic type in that
	 case.
         isa points to the dynamic type class object.  The "name" field of
         that object gives us the dynamic class name.  

	 If we run across a class that has more than one base type, that's
	 a C++ class.  We don't need to look any further then, we know
	 that we won't get an ObjC dynamic type for this.

	 Finally, again we might get an incomplete type that we have
         baseclass info for, so make sure we aren't indexing past the
         end of the fields array.  */

      while (base_type && nfields != 0)
        {
	  int n_base_class;

	  n_base_class = TYPE_N_BASECLASSES (base_type);
	  if (n_base_class == 1)
	    {
	      /* If we actually have inheritance, we come in here. */
	      base_type = TYPE_FIELD_TYPE (base_type, 0);
	      if (base_type)
		nfields = TYPE_NFIELDS (base_type);
	      else
		nfields = 0;
	    }
	  else if (n_base_class == 0)
	    {	      
	      t_field_name = TYPE_FIELD_NAME (base_type, n_base_class);
	      
	      if (t_field_name && t_field_name[0] == '\0')
		{
		  /* If we have the weird DWARF first field with no 
		     name we come in here.  */
		  base_type = TYPE_FIELD_TYPE (base_type, n_base_class);
		  if (base_type)
		    nfields = TYPE_NFIELDS (base_type); 
		  else
		    nfields = 0;
		}
	      else
		/* We get here for stabs, or if we've reached the end
		   of the inheritance hierarchy.  */
		break;
	    }
	  else
	    /* We get here if we've fed a C++ object to
	       value_objc_target_type.  
	       FIXME: We should use the TYPE_RUNTIME of the original
	       base_type to reject C++ types.  But I'm not sure that
               this wouldn't get set wrong by some older compiler.  */
	    return NULL;
       }

      if (t_field_name && (strcmp_iw (t_field_name, "isa") == 0))
	{
	  /* APPLE LOCAL: Extract the code that finds this into a separate
	     routine so I can reuse it.  */
	  dynamic_type = objc_target_type_from_object (value_as_address (val), 
						       block, addrsize, 
						       dynamic_type_handle);
	  /* Only pass out dynamic name if the dynamic type is NULL.  */
	  if (dynamic_type != NULL && dynamic_type_handle != NULL)
	    {
	      xfree (*dynamic_type_handle);
	      *dynamic_type_handle = NULL;
	    }
	}
    }
  return dynamic_type;
}

/* Check to see if the objc runtime lock is held.
   Returns 1 for is taken, 0 for not taken and -1
   if there was an error figuring this out.  
   NB - This method is obsolete in SnowLeopard, and you
   should use the debugger "safe mode" instead.  */

int
objc_runtime_lock_taken_p ()
{
  static struct cached_value *function = NULL;
  if (function == NULL)
    {
      /* APPLE LOCAL begin Objective-C */
      if (lookup_minimal_symbol("gdb_objc_isRuntimeLocked", 0, 0))
	function = create_cached_function ("gdb_objc_isRuntimeLocked",
					   builtin_type_voidptrfuncptr);
    }
  /* If the lock lookup function is present use it, otherwise fall back
     on the thread check.  */

  if (function != NULL)
    {
      struct value *retval = NULL /* NULL to quiet the compiler */;
      struct gdb_exception e;
      TRY_CATCH (e, RETURN_MASK_ALL)
	{
	  retval = call_function_by_hand (lookup_cached_function (function),
					  0, NULL);
	}
      if (e.reason != NO_ERROR)
	return -1;
      else
	return value_as_long (retval);
    }
  else
    return -1;

}

/* We mark all ObjC offsets "invalid" by setting them to the
   negative of the value that was in the debug information. 
   Then in TYPE_FIELD_BITPOS we test for < 0 values, and fix
   them up one by one in objc_fixup_ivar_offset.  If the BITPOS
   of the field is 0, we set it to INT_MIN.
   We also set
   the length of the type to negative of its value.  */

void
objc_invalidate_objc_class (struct type *type)
{
  int i;
  if (!new_objc_runtime_internals ())
    return;

  for (i = 0; i < TYPE_NFIELDS (type); i++)
    {
      if (i >= TYPE_N_BASECLASSES (type))
	{
	  int old_offset = TYPE_FIELD_BITPOS_ASSIGN (type, i); 
	  if ( old_offset < 0)
	    internal_error (__FILE__, __LINE__, 
			    "TYPE_FIELD_BITPOS < 0 for field %s of class %s.",
			    TYPE_FIELD_NAME (type, i), TYPE_NAME (type));
	  if (old_offset == 0)
	    TYPE_FIELD_BITPOS_ASSIGN (type, i) = INT_MIN;
	  else
	    TYPE_FIELD_BITPOS_ASSIGN (type, i) = -old_offset;
	}
    }
  if (TYPE_LENGTH_ASSIGN (type) > 0)
    TYPE_LENGTH_ASSIGN (type) = - TYPE_LENGTH_ASSIGN (type);

}

#define IVAR_OFFSET_PREFIX "OBJC_IVAR_$_"
/* dwarf2read.c hard-codes bits_per_byte to 8 as well, and I'm following that
   usage.  */
static int bits_per_byte = 8;

/* APPLE LOCAL begin 6478246 fix offsets for bitfields in objc classes.  */
/* These two static variables are used to keep track of important data
   between calls to objc_fixup_ivar_offset on the fields of the same
   type/struct.  These two variables need to have their values re-set
   to these initial values before calling objc_fixup_ivar_offset on
   fields of a new type.  */
static int first_bitfield_index = -1;
static int first_bitfield_offset = 0;
/* APPLE LOCAL end 6478246 fix offsets for bitfields in objc classes.  */

int
objc_fixup_ivar_offset (struct type *type, int ivar)
{
  char *class_name;
  char *field_name;
  char *symbol_name;
  struct minimal_symbol *ivar_sym;
  int len;
  static int prefix_len = 0;
  struct cleanup *name_cleanup;
  CORE_ADDR ivar_addr;
  ULONGEST ivar_offset;
  int ivar_len;
  int class_len;
  /* APPLE LOCAL begin 6478246 fix offsets for bitfields in objc classes.  */
  /* Important note:  THIS FIX ASSUMES THAT ALL THE FIELDS OF AN OBJECTIVE-C
     CLASS WILL BE PASSED TO/THROUGH THIS FUNCTION, IN ORDER, i.e. it
     assumes this function is called on all fields via a loop in the calling
     function.  */
  int is_bitfield_p = 0;
  int cur_bitfield_offset = 0;

  /* Check to see if current field is a bitfield, ie. it has a non-zero
     bitsize value.  */
  
  if (TYPE_FIELD_BITSIZE (type, ivar) != 0)
    is_bitfield_p = 1;

  if (! is_bitfield_p)
    {
      /* We're NOT handling a bitfield, so re-initialize these STATIC 
	 variables.  */
      first_bitfield_index = -1;
      first_bitfield_offset = 0;
    }
  else if (first_bitfield_index < 0)
    {
      /* If we are in a bitfield, and the first_bitfield_index is
	 negative, then this is the first bitfield encountered 
	 (possibly after one or more non-bitfield fields).  Subsequent
	 contiguous bitfields will figure their offsets relative to
	 this field's offset, so save it!  */
      first_bitfield_index = ivar;
      first_bitfield_offset = TYPE_FIELD_BITPOS_ASSIGN (type, ivar);
      if (first_bitfield_offset < 0)
	first_bitfield_offset = -first_bitfield_offset;
    }

  if (is_bitfield_p)
    {
      /* For the current bitfield, figure out it's offset from
	 the first bitfield in this sequence of bitfields in this
	 struct/class.  */
      
      /* Get the current offset & make it positive.  */

      cur_bitfield_offset = TYPE_FIELD_BITPOS_ASSIGN (type, ivar);
      if (cur_bitfield_offset < 0)
	cur_bitfield_offset = -cur_bitfield_offset;

      /* Find it's offset relative to the first bitfield.  */

      cur_bitfield_offset = cur_bitfield_offset - first_bitfield_offset;

      /* Since the Objective C runtime returns the beginning of each
	 byte, we only need an offset from the beginning of the nearest
	 preceding byte, so modulo by bits_per_byte.  */

      if (cur_bitfield_offset != 0)
	cur_bitfield_offset = cur_bitfield_offset % bits_per_byte;
    }
  /* APPLE LOCAL end 6478246 fix offsets for bitfields in objc classes.  */

  /* For enums the bit position is the value of the enum.  We're
     always going to end up calling the function, which is a little
     annoying, but we just return that...  */
  if (TYPE_CODE (type) == TYPE_CODE_ENUM)
    return TYPE_FIELD_BITPOS_ASSIGN (type, ivar);

  /* Otherwise, I don't think the bitpos should ever be negative.  */
  if (TYPE_CODE (type) != TYPE_CODE_STRUCT
      && TYPE_CODE (type) != TYPE_CODE_CLASS)
    {
      if (info_verbose)
	{
	  fprintf_unfiltered (gdb_stdlog,
			      "Asked to fix up class: \"%s\" "
			      "which is not a struct type.\n",
			      TYPE_NAME (type) ? TYPE_NAME (type) : "<unknown>");
	}
      return TYPE_FIELD_BITPOS_ASSIGN (type, ivar);
    }
	     
  if (ivar < TYPE_N_BASECLASSES (type))
    return TYPE_FIELD_BITPOS_ASSIGN (type, ivar);

  class_name = TYPE_NAME (type);
  if (class_name == NULL)
    class_name = TYPE_TAG_NAME (type);
  if (class_name == NULL)
    {
      int old_offset = TYPE_FIELD_BITPOS_ASSIGN (type, ivar);
      if (info_verbose)
	{
	  fprintf_unfiltered (gdb_stdlog,
			      "Asked to fix up class with no name.\n");
	}
      if (old_offset == INT_MIN)
	TYPE_FIELD_BITPOS_ASSIGN (type, ivar) = 0;
      else
	TYPE_FIELD_BITPOS_ASSIGN (type, ivar) = - old_offset;
      return TYPE_FIELD_BITPOS_ASSIGN (type, ivar);

    }

  field_name = TYPE_FIELD_NAME (type, ivar);

  if (prefix_len == 0)
    prefix_len = strlen (IVAR_OFFSET_PREFIX);

  len = prefix_len + strlen (class_name) + strlen (field_name) + 1 + 1;
  symbol_name = xmalloc (len);
  name_cleanup = make_cleanup (xfree, symbol_name);

  snprintf (symbol_name, len, "%s%s.%s", IVAR_OFFSET_PREFIX, class_name, field_name);
  
  ivar_sym = lookup_minimal_symbol (symbol_name, NULL, NULL);

  /* Dunno why we wouldn't be able to find this symbol, but the best
     thing to do is go back to trusting the static debug info.  */
  if (ivar_sym == NULL)
    {
      int old_offset = TYPE_FIELD_BITPOS_ASSIGN (type, ivar);
      if (info_verbose)
	fprintf_unfiltered (gdb_stdlog, 
			    "Couldn't find ivar symbol: \"%s\".\n", symbol_name);
      if (old_offset == INT_MIN)
	TYPE_FIELD_BITPOS_ASSIGN (type, ivar) = 0;
      else
	TYPE_FIELD_BITPOS_ASSIGN (type, ivar) = - old_offset;
      goto done;
    }
  
  ivar_addr = SYMBOL_VALUE_ADDRESS (ivar_sym);

  if (!safe_read_memory_unsigned_integer (ivar_addr, 4, &ivar_offset))
    {
      warning ("Couldn't read ivar offset at %s for ivar symbol \"%s\".\n",
	       paddr_nz (ivar_addr), symbol_name);
      goto done;

    }
  
  /* The added fields will also make the class type bigger than it was
     in the debug information.  So if the position + length of this
     ivar is bigger than the original type's length, extend the
     type.  */
  ivar_len = TYPE_LENGTH (check_typedef (TYPE_FIELD_TYPE (type, ivar)));

  /* The type length of the class will be negative, except if another
     ivar caused it to get fixed up, in which case we need to reverse the
     sign to test the lengths.  */
  class_len = TYPE_LENGTH_ASSIGN (type);
  if (class_len < 0)
    class_len = -class_len;

  if (ivar_offset + ivar_len > class_len)
    TYPE_LENGTH_ASSIGN (type) = ivar_offset + ivar_len;

  /* The ObjC runtime stores the offset in bytes, but we want it in
     bits for the bitpos.  */
  ivar_offset *= bits_per_byte;

  /* APPLE LOCAL begin 6478246 fix offsets for bitfields in objc classes.  */
  if (is_bitfield_p)
    ivar_offset += cur_bitfield_offset;
  /* APPLE LOCAL end 6478246 fix offsets for bitfields in objc classes.  */

  if (info_verbose)
    {
      int orig_value;
      if (TYPE_FIELD_BITPOS_ASSIGN (type, ivar) == INT_MIN)
	orig_value = 0;
      else
	orig_value = -TYPE_FIELD_BITPOS_ASSIGN (type, ivar);

      if (ivar_offset != orig_value)
	{
	  fprintf_unfiltered (gdb_stdlog, "Changing ivar offset for ivar: %s"
			      " of class: %s from: %d to %lu.\n",
			      field_name,
			      class_name,
			      orig_value,
			      ivar_offset);
	}
    }
  TYPE_FIELD_BITPOS_ASSIGN (type, ivar) = ivar_offset;

 done:
  do_cleanups (name_cleanup);
  return TYPE_FIELD_BITPOS_ASSIGN (type, ivar);

}

/* This uses objc_fixup_ivar_offset to reset the class
   length.  If fixing all the ivars doesn't reset the 
   class length, then we just reverse the sign.  */

int
objc_fixup_class_length (struct type *type)
{
  /* objc_fixup_ivar_offset takes care of readjusting
     the type length, so we just need to run through
     the ivars of this class.  */

  int i;

  /* APPLE LOCAL begin 6478246 fix offsets for bitfields in objc classes.  */
  /* N.B.  The following two static variables need to be re-set to these
     initial values before objc_fixup_ivar_offset is called on the
     fields of a new type.  */
  first_bitfield_index = -1;
  first_bitfield_offset = 0;
  /* APPLE LOCAL end 6478246 fix offsets for bitfields in objc classes.  */

  for (i = 0; i < TYPE_NFIELDS (type); i++)
    objc_fixup_ivar_offset (type, i);

  if (TYPE_LENGTH_ASSIGN (type) < 0)
    TYPE_LENGTH_ASSIGN (type) = - TYPE_LENGTH_ASSIGN (type);
  return TYPE_LENGTH_ASSIGN (type);

}


/* For internal testing of various locks-held scenarios it is easiest to
   simulate these being held.  These variables are set by attaching with
   another gdb; there is no exposed way to change them.  */

#ifndef LOCKS_DEBUGGING
#define LOCKS_DEBUGGING 0
#endif
static int spinlock_lock_is_present = 0;
static int malloc_lock_is_present = 0;

/* This is the "maint interval" token for timing calls into
   the inferior to turn on and off the debug mode.  */
static int debug_mode_timer = -1;

static void
do_end_debugger_mode (void *arg)
{
  struct cached_value *end_function = (struct cached_value *) arg;
  struct cleanup *scheduler_cleanup;
  struct gdb_exception e;

  if (!target_has_execution)
    {
      debug_mode_set_p = debug_mode_not_checked;
      debug_mode_set_reason = objc_debugger_mode_unknown;
      return;
    }

  if (debug_handcall_setup)
    fprintf_unfiltered (gdb_stdout, "Ending debugger mode, "
                  "debug_mode_set_p is %d\n", debug_mode_set_p);

  scheduler_cleanup = make_cleanup_set_restore_scheduler_locking_mode 
                      (scheduler_locking_on);
  make_cleanup_set_restore_unwind_on_signal (1);

  if (maint_use_timers)
    start_timer (&debug_mode_timer, "objc-debug-mode", 
                 "Turning off debugger mode");

  debug_mode_set_p = debug_mode_okay;
  debug_mode_set_reason = objc_debugger_mode_success;

  TRY_CATCH (e, RETURN_MASK_ALL)
    {
      call_function_by_hand (lookup_cached_function (end_function),
			     0, NULL);
    }

  if (info_verbose)
    fprintf_unfiltered (gdb_stdout, "Ended debugger mode.\n");

  debug_mode_set_p = debug_mode_not_checked;
  debug_mode_set_reason = objc_debugger_mode_unknown;
  do_cleanups (scheduler_cleanup);

  if (e.reason != NO_ERROR)
    {
      fprintf_unfiltered (gdb_stdout, "Error resetting ObjC debugger mode: %s",
			  e.message);
      throw_exception (e);
    }
}

static void
do_reset_debug_mode_flag (void *unused)
{
  debug_mode_set_p = debug_mode_not_checked;
  debug_mode_set_reason = objc_debugger_mode_unknown;
}

static struct breakpoint *debugger_mode_fail_breakpoint;
static struct breakpoint *objc_exception_throw_breakpoint;

static void 
do_cleanup_objc_exception_breakpoint (void *unused)
{
  if (objc_exception_throw_breakpoint != NULL)
      disable_breakpoint (objc_exception_throw_breakpoint);
}

struct cleanup *
make_cleanup_init_objc_exception_catcher (void)
{
  if (objc_exception_throw_breakpoint != NULL)
    {
      struct cleanup *suppress_cleanup 
	= make_cleanup_ui_out_suppress_output (uiout);
      enable_breakpoint (objc_exception_throw_breakpoint);
      do_cleanups (suppress_cleanup);
    }
  else
    objc_exception_throw_breakpoint 
      = create_objc_hook_breakpoint ("objc_exception_throw");

  return make_cleanup (do_cleanup_objc_exception_breakpoint, NULL);
}

static int
init_debugger_mode_fail_notification ()
{
  if (debugger_mode_fail_breakpoint != NULL)
    return 1;

  debugger_mode_fail_breakpoint 
   = create_objc_hook_breakpoint ("gdb_objc_debuggerModeFailure");

  return (debugger_mode_fail_breakpoint != NULL);
}

enum objc_handcall_fail_reasons
objc_pc_at_fail_point (CORE_ADDR pc)
{
  if (debugger_mode_fail_breakpoint != NULL 
      && pc == debugger_mode_fail_breakpoint->loc->address)
    return objc_debugger_mode_fail;

  if (objc_exception_throw_breakpoint != NULL 
      && pc == objc_exception_throw_breakpoint->loc->address)
    return objc_exception_thrown;

  return objc_no_fail;
}

/* This sets the ObjC runtime into "debugger" mode if available
   and returns a cleanup that will put it back in regular
   mode.  If LEVEL is 0, then we request "limited access"
   which means that calls may fail if the write lock is
   required.  If LEVEL is 1, we request full access, for
   instance if we need to run dlopen.

   If LEVEL is -1 we will debug_mode_set_p to debug_mode_overridden, 
   and not turn on the debugger mode.  Use this to suppress hand_call_function's 
   automatic turning on of this mode.

   For efficiency's sake, we will actually put the "end debugger mode"
   cleanup on the hand_call_cleanup chain, and the cleanup we return
   here is to setting the scheduler locking mode (since the debugger
   mode function requires that once set we only run that thread.)
   However, if we return success we will lock the scheduler every time
   you call the function, UNLESS you have overridden the debugger mode
   by setting LEVEL to -1.  That is required, since the debugger mode
   must only be run on one thread.

   If LEVEL was 0 or 1, *CLEANUP is set to either a no-op cleanup or a 
   cleanup to undo the scheduler mode.
   If LEVEL was -1, the cleanup will restore the debugger mode to
   debug_mode_not_checked.

   This function also interoperated with the debug_mode_set_p
   flag in a way that will allow us to cache the result of
   the debug mode lookup till we run again.  

   * If we succeed in setting the debug mode, we set debug_mode_flag_p
   to debug_mode_okay, and on subsequent calls we return objc_debugger_mode_success.
   * If we successfully call the debugger mode function, but
   we get back "no can do" we set *CLEANUP to a no-op and return
   objc_debugger_mode_fail_unable_to_enter_debug_mode.

   FIXME: This is getting very specific to details of the
   Apple Runtime.  Should we try to abstract this some?  */

enum objc_debugger_mode_result
make_cleanup_set_restore_debugger_mode (struct cleanup **cleanup, int level)
{
  static struct cached_value *start_function = NULL;
  static struct cached_value *end_function = NULL;
  struct value *tmp_value;
  struct cleanup *scheduler_cleanup;
  struct cleanup *timer_cleanup = NULL;
  struct cleanup *unwind_cleanup;

  int success = 0;
  struct gdb_exception e;

  /* Most of the exits from this function return a null cleanup;
     set that by default.  The couple of exits that set it to a real
     cleanup can overwrite this.  */
  if (cleanup)
    *cleanup = make_cleanup (null_cleanup, 0);

  /* If we've already decided to override the checks, then just return the null
     cleanup.  Otherwise the cleanup from a nested call to "override" would set
     us back to "not checked" when the innermost nested call was finished.  */

  if (debug_mode_set_p == debug_mode_overridden)
    {
      if (debug_handcall_setup)
	fprintf_unfiltered (gdb_stdout, "make_cleanup_set_restore_debugger_mode:"
			    " Debug mode set to %d, returning null cleanup.\n", debug_mode_set_p);
      return debug_mode_set_reason;
    }

  /* Otherwise if LEVEL is -1 we're deciding that it's OK to make
     inf. func calls regardless of what the reality might be.  So set our state
     to override, set ourselves to return success, and return a cleanup that
     will turn off the override state.  */

  if (level == -1)
    {
      if (debug_handcall_setup)
	fprintf_unfiltered (gdb_stdout, "make_cleanup_set_restore_debugger_mode:"
                           " LEVEL is -1, returning reset cleanup.\n");

      /* In case the debugger mode is already set on, let's turn it off.  */
      if (debug_mode_set_p == debug_mode_okay)
	do_hand_call_cleanups (ALL_CLEANUPS);

      if (cleanup)
        *cleanup = make_cleanup (do_reset_debug_mode_flag, 0);
      else
        make_cleanup (do_reset_debug_mode_flag, 0);

      /* Set the reason to success so we'll return that on subsequent calls.  */

      debug_mode_set_p = debug_mode_overridden;
      debug_mode_set_reason = objc_debugger_mode_success;
      return objc_debugger_mode_success;
    }

  if (debug_mode_set_p == debug_mode_okay)
    {
      if (debug_handcall_setup)
	    fprintf_unfiltered (gdb_stdout, "make_cleanup_set_restore_debugger_mode:"
                               " debug mode set, returning null cleanup.\n");

      /* Since you can only run the debug mode on one thread, 
	 we should always lock the scheduler if our client is
	 turning on the debugger mode - regardless of whether we
	 actually have to do anything to turn it on or not...  */
      if (cleanup)
	*cleanup = make_cleanup_set_restore_scheduler_locking_mode (scheduler_locking_on);
      else
	make_cleanup_set_restore_scheduler_locking_mode (scheduler_locking_on);

      return objc_debugger_mode_success;
    }
  else if (debug_mode_set_p == debug_mode_failed)
    {
      if (debug_handcall_setup)
	fprintf_unfiltered (gdb_stdout, "make_cleanup_set_restore_debugger_mode:"
			    " Debug mode set to %d, returning null cleanup.\n", debug_mode_set_p);
      return debug_mode_set_reason;
    }

  /* If we can't find the ObjC runtime in the process, don't do
     any of this work.  Return success because if ObjC isn't around,
     it can't be taking locks...  */

  if (find_libobjc_objfile () == NULL)
    return objc_debugger_mode_success;

  /* FIXME - make sure the breakpoint is still good if we
     rerun.  */

  init_debugger_mode_fail_notification ();

  /* Initialize our static cached functions here.  */

  if (start_function == NULL)
    {
      /* APPLE LOCAL begin Objective-C */
      if (lookup_minimal_symbol ("gdb_objc_startDebuggerMode", 0, 0))
	{
	  struct type *func_type;
	  func_type = builtin_type_int;
	  func_type = lookup_function_type (func_type);
	  func_type = lookup_pointer_type (func_type);
	  start_function = create_cached_function ("gdb_objc_startDebuggerMode",
						    func_type);
	}
      else
	{
          return objc_debugger_mode_fail_objc_api_unavailable;
	}
    }
  if (end_function == NULL)
    {
      /* APPLE LOCAL begin Objective-C */
      if (lookup_minimal_symbol ("gdb_objc_endDebuggerMode", 0, 0))
	{
	  end_function = create_cached_function ("gdb_objc_endDebuggerMode",
						   builtin_type_voidptrfuncptr);
	}
      else
	{
          return objc_debugger_mode_fail_objc_api_unavailable;
	}
    }
  
  scheduler_cleanup = make_cleanup_set_restore_scheduler_locking_mode 
    (scheduler_locking_on);

  unwind_cleanup = make_cleanup_set_restore_unwind_on_signal (1);
  tmp_value = value_from_longest (builtin_type_int, level);

  release_value (tmp_value);
  make_cleanup ((make_cleanup_ftype *) value_free, tmp_value);

  if (maint_use_timers)
    timer_cleanup = start_timer (&debug_mode_timer, "objc-debug-mode", 
				 "Turning on debugger mode");

  /* The Mac OS X pthreads implementation of pthread_mutex_trylock currently
     blocks if another function is sitting on the spin_lock.  So we check for
     that first, and if find a spinlock call, we can't start the debugger mode.  
     Everybody that wants to start the ObjC debugger mode is going to run code
     that will malloc at some point, so...  */

  /* Set debug_mode_set_p so we don't re-run this function while
     doing something down in target_check_safe_call.  */

  debug_mode_set_p = debug_mode_okay;
  debug_mode_set_reason = objc_debugger_mode_success;

  TRY_CATCH (e, RETURN_MASK_ALL)
    {
      success = target_check_safe_call (SPINLOCK_SUBSYSTEM, 
					CHECK_SCHEDULER_VALUE);
    }

  /* For internal debugging, pretend that there was a spinlock present.  */
  if (LOCKS_DEBUGGING && spinlock_lock_is_present)
    success = 0;

  if (e.reason != NO_ERROR || !success)
    {
      do_cleanups (scheduler_cleanup);
      make_hand_call_cleanup (do_reset_debug_mode_flag, 0);
      debug_mode_set_p = debug_mode_failed;
      debug_mode_set_reason = objc_debugger_mode_fail_spinlock_held;
      return objc_debugger_mode_fail_spinlock_held;
    }

  /* Set debug_mode_set_p so we don't re-run this function while
     doing something down in target_check_safe_call.  */

  debug_mode_set_p = debug_mode_okay;
  debug_mode_set_reason = objc_debugger_mode_success;

  TRY_CATCH (e, RETURN_MASK_ALL)
    {
      success = target_check_safe_call (MALLOC_SUBSYSTEM, 
					CHECK_SCHEDULER_VALUE);
    }

  /* For internal debugging, pretend that there was a malloc lock present.  */
  if (LOCKS_DEBUGGING && malloc_lock_is_present)
    success = 0;

  if (e.reason != NO_ERROR || !success)
    {
      do_cleanups (scheduler_cleanup);
      make_hand_call_cleanup (do_reset_debug_mode_flag, 0);
      debug_mode_set_p = debug_mode_failed;
      debug_mode_set_reason = objc_debugger_mode_fail_malloc_lock_held;
      return objc_debugger_mode_fail_malloc_lock_held;
    }

  /* Set debug_mode_set_p so we don't re-run this function while
     doing something down in call_function_by_hand.  */

  debug_mode_set_p = debug_mode_okay;
  debug_mode_set_reason = objc_debugger_mode_success;

  TRY_CATCH (e, RETURN_MASK_ALL)
    {
      tmp_value = call_function_by_hand 
                      (lookup_cached_function (start_function), 1, &tmp_value);
    }

  if (timer_cleanup != NULL)
    do_cleanups (timer_cleanup);
  do_cleanups (unwind_cleanup);

  debug_mode_set_p = debug_mode_not_checked;
  debug_mode_set_reason = objc_debugger_mode_unknown;

  if (e.reason != NO_ERROR)
      throw_exception (e);

  success = value_as_long (tmp_value);

  if (info_verbose)
    fprintf_filtered (gdb_stdout, "Tried to start debugger mode, return value: %d.\n", success);

  if (success == 0)
    {
      do_cleanups (scheduler_cleanup);
      make_hand_call_cleanup (do_reset_debug_mode_flag, 0);
      debug_mode_set_p = debug_mode_failed;
      debug_mode_set_reason = objc_debugger_mode_fail_unable_to_enter_debug_mode;
      return objc_debugger_mode_fail_unable_to_enter_debug_mode;
    }
  
  debug_mode_set_p = debug_mode_okay;
  debug_mode_set_reason = objc_debugger_mode_success;

  /* If someone has turned off the runtime checking, we will only get into this
     function when we KNOW that we will be calling into the runtime for just 
     this call (for instance for the print_object_command).  In that case, we 
     return an immediate cleanup of the runtime mode.  Otherwise we put the 
     cleanup on the hand_call cleanup chain, and it will get turned off when 
     we run again.  */

  if (get_objc_runtime_check_level () >= 0)
    {
      if (debug_handcall_setup)
	fprintf_unfiltered (gdb_stdout, 
             "Adding do_end_debugger_mode cleanup to hand_call_cleanup.\n");
      make_hand_call_cleanup (do_end_debugger_mode, end_function);
    }
  else
    {
      if (debug_handcall_setup)
	fprintf_unfiltered (gdb_stdout, 
             "Adding do_end_debugger_mode cleanup to generic cleanup chain.\n");
      make_cleanup (do_end_debugger_mode, end_function);
    }

  if (cleanup)
    *cleanup = scheduler_cleanup;
  return objc_debugger_mode_success;
}

/* APPLE LOCAL begin use '[object class]' rather than isa  */
     
/* This function tries to find the address of the original user-defined
   class to which an object belongs.  This is necessary because, since
   Objective-C classes are mutable, sometimes an object's class may
   get changed or overwritten, so trying to follow the isa pointer
   directly to find the method a user is attempting to step into may
   not work as expected.  However calling the object's 'class' method
   is supposed to return the original class.  */

static CORE_ADDR
get_class_address_from_object (CORE_ADDR object_addr)
{
  struct value *function;
  struct objc_object orig_object;
  static char class_selname[6] = "class";
  CORE_ADDR class_sel = (CORE_ADDR) 0;
  CORE_ADDR class_method_addr = (CORE_ADDR) 0;
  CORE_ADDR ret_addr = (CORE_ADDR) 0;
  struct value *infargs[2];
  struct value *objval;
  struct value *selval;
  struct value *retval;
  
  /* Read the object, to find the isa pointer.  */

  read_objc_object (object_addr, &orig_object);


  if (!target_has_execution)
    {
      /* Can't call into inferior to lookup class.  */
      return orig_object.isa;
    }

  /* Try to find the selector for the object's 'class' method.  */

  class_sel = lookup_child_selector (class_selname);

  /* Lookup the address for the 'class' method.  */

  if (class_sel != 0)
    class_method_addr = new_objc_runtime_find_impl (orig_object.isa, 
						    class_sel, 
						    0);

  if (class_method_addr != 0)
    {
      struct cleanup *scheduler_cleanup;
      enum objc_debugger_mode_result objc_retval;

      /* We have found the method "class" that will return the address
	 of the original class for the object; now we need to call
	 that method by hand, passing it both the object address, and
	 the selector address.  */

      /* Lock the scheduler before calling this so the other threads
	 don't make progress while you are running this.  */

      scheduler_cleanup = make_cleanup_set_restore_scheduler_locking_mode 
	                                                (scheduler_locking_on);

      make_cleanup_set_restore_unwind_on_signal (1);


      /* This isn't called directly by the user, so we should not
	 tell her if something goes wrong, we should just clean up and
	 handle the error appropriately.  */

      make_cleanup_ui_out_suppress_output (uiout);

      objc_retval = make_cleanup_set_restore_debugger_mode (NULL, 0);
      if (objc_retval == objc_debugger_mode_fail_objc_api_unavailable)
        if (target_check_safe_call (OBJC_SUBSYSTEM, CHECK_SCHEDULER_VALUE))
          objc_retval = objc_debugger_mode_success;

      if (objc_retval == objc_debugger_mode_success)
	{
	  /* Package up the arguments for the function call.  */

	  objval = value_from_pointer (lookup_pointer_type 
				                 (builtin_type_void_data_ptr),
				       object_addr);
	  selval = value_from_pointer (lookup_pointer_type 
				                 (builtin_type_void_data_ptr),
				       class_sel);

	  infargs[0] = objval;
	  infargs[1] = selval;
	  
	  /* We need to create the function value directly, by-passing the
	     cache lookup stuff, because the cache lookup stuff requires
	     an msymbol (and there isn't necessarily an msymbol named
	     'class'); and then it uses the address in the msymbol,
	     whereas we've just gone through a bit of trouble to figure
	     out the address we want to use ourselves.  */
	  
	  /* Package up the address of the function to be called.  */
	  
	  function = value_from_pointer (builtin_type_voidptrfuncptr, 
				     class_method_addr);
      
	  /* Perform the '[object class]' call.  */

	  retval = call_function_by_hand (function, 2, infargs);

	  /* Get the address for the object's class out of the return
	     value.  */

	  ret_addr = (CORE_ADDR) value_as_address (retval);
	  ret_addr = gdbarch_addr_bits_remove (current_gdbarch, ret_addr);
	}
      do_cleanups (scheduler_cleanup);
    }

  /* If for some reason we couldn't find the class address previously,
     (either we couldn't find a 'class' method, or the call
     didnt'work) we'd better fall back on using the object.isa
     field.  */

  if (ret_addr == 0
      && orig_object.isa != 0)
    ret_addr = orig_object.isa;

  /* One other thing to check for: Sometimes the "object_addr" that
     gets passed in already IS the real class addr.  In which case we
     want to return the isa pointer.  */

  if (ret_addr == object_addr)
    ret_addr = orig_object.isa;

  return ret_addr;
}
/* APPLE LOCAL end use '[object class]' rather than isa  */

/* APPLE LOCAL begin Disable breakpoints while updating data formatters.  */
/* Return an integer-boolean indicating whether or not the
   breakpoint passed in is the special breakpoint gdb sets in
   objc_exception_throw or not.  */

int
is_objc_exception_throw_breakpoint (struct breakpoint *b)
{
  return (b == objc_exception_throw_breakpoint);
}
/* APPLE LOCAL end Disable breakpoints while updating data formatters.  */

static const char *non_blocking_modes[] = {"off", "limited", "on", NULL};
static const char *non_blocking_mode;
int
get_objc_runtime_check_level ()
{
  if (strcmp (non_blocking_mode, "off") == 0)
    return -1;
  else if (strcmp (non_blocking_mode, "limited") == 0)
    return 0;
  else
    return 1;
}

static void
set_non_blocking_mode_func (char *args, int from_tty, 
                            struct cmd_list_element *c)
{
  /* If we're turning off the non-blocking mode, then we need to 
     end the debugging mode if it is currently on.  */
  if (get_objc_runtime_check_level () == -1)
    {
      do_hand_call_cleanups (ALL_CLEANUPS);
    }
}

void
_initialize_objc_lang ()
{
  add_setshow_boolean_cmd ("call-po-at-unsafe-times", no_class, &call_po_at_unsafe_times, 
			   "Set whether to override the check for potentially unsafe"
			   " situations before calling print-object.",
			   "Show whether to override the check for potentially unsafe"
			   " situations before calling print-object.",
			   "??",
			   NULL, NULL,
			   &setlist, &showlist);

  add_setshow_boolean_cmd ("let-po-run-all-threads", no_class, &let_po_run_all_threads,
			   "Set whether po should run all threads if it can't "
			   " safely run only the current thread.",
			   "Show whether po should run all threads if it can't "
			   " safely run only the current thread.",
			   "By default, \"po\" will try to run only the current thread\n"
			   "However, that may cause deadlocks between the po function and\n"
                           "a lock held on some other thread.  In that case, po will allow\n"
                           "the other threads to run as well.  If you don't want it to do that\n"
                           "then set this variable to off.",
			   NULL, NULL,
			   &setlist, &showlist);
		      		      
  add_setshow_boolean_cmd ("lookup-objc-class", no_class, &lookup_objc_class_p,
			   "Set whether we should attempt to lookup Obj-C classes when we resolve symbols.",
			   "Show whether we should attempt to lookup Obj-C classes when we resolve symbols.",
			   "??",
			   NULL, NULL,
			   &setlist, &showlist);

  add_setshow_zinteger_cmd ("objc-version", no_class, &objc_runtime_version,
			   "Set the current Objc runtime version.  "
			    "If non-zero, this will override the default selection.",
			   "Show the current Objc runtime version.",
			   "??",
			   NULL, NULL,
			   &setlist, &showlist);

  add_setshow_enum_cmd ("objc-non-blocking-mode", no_class, non_blocking_modes, &non_blocking_mode,
			   "Set whether all inferior function calls should use the objc non-blocking mode.\n\
Note that this will mean the attempt to call the function will fail if we can't turn on\n\
the non-blocking mode.\n\
    off - don't use non-blocking mode\n\
    limited - require that no thread have any runtime write locks\n\
    full - require that no thread have any of the objc runtime locks",
			   "Show whether all inferior function calls should use the objc non-blocking mode.",
			   "??",
			   set_non_blocking_mode_func, NULL,
			   &setlist, &showlist);

  non_blocking_mode = non_blocking_modes[1];

}
