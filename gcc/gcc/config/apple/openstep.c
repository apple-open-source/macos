/* Functions for generic NeXT as target machine for GNU C compiler.
   Copyright (C) 1989, 1990, 1991, 1992, 1993 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "c-tree.h"

/* Make everything that used to go in the text section really go there.  */

int flag_no_mach_text_sections = 0;

#ifdef DWARF2_UNWIND_INFO
int dwarf_set_counter = 0;
#endif
#ifdef HAVE_COALESCED_SYMBOLS
/* Force all succeeding decls to be marked COALESCED.  */
static int initial_flag_force_coalesced;
static int initial_flag_privatize_coalesced;
#endif

/* 1 if handle_pragma has been called yet.  */

static int pragma_initialized;

/* Initial setting of `optimize'.  */

static int initial_optimize_flag, initial_optimize_size_flag;
static int initial_flag_expensive_optimizations;

static int initial_flag_altivec;
static int initial_vrsave_save_type;
static int initial_flag_internal_refs;

#ifdef APPLE_MAC68K_ALIGNMENT
#define FIELD_ALIGN_STK_SIZE 1024
static unsigned char field_align_stk[FIELD_ALIGN_STK_SIZE];
static int field_align_stk_top;

void
push_field_alignment (bit_alignment)
     int bit_alignment;
{
  if (field_align_stk_top < FIELD_ALIGN_STK_SIZE)
    {
      field_align_stk[field_align_stk_top++] = maximum_field_alignment;
      maximum_field_alignment = bit_alignment;
    }
  else
    error ("too many #pragma options align or #pragma pack(n)");
}

void
pop_field_alignment ()
{
  if (field_align_stk_top > 0)
    maximum_field_alignment = field_align_stk[--field_align_stk_top];
  else
    error ("too many #pragma options align=reset");
}
#endif	/* APPLE_MAC68K_ALIGNMENT */

static int OnOffReset(const char *p, int *the_flag, int initial_flag_value)
{
  if (strcmp (p, "on") == 0)
    *the_flag = 1;
  else
  if (strcmp (p, "off") == 0)
    *the_flag = 0;
  else
  if (strcmp (p, "reset") == 0)
    *the_flag = initial_flag_value;
  else
    return 0;	/* Not handled.  */

  return 1;	/* This was handled.  */
}

/* get_optional_number: scan P for an optional "(<digit>)", returning
   the value of said digit if found, otherwise return -1.  */

static int get_optional_number(const char *p)
{
  int val = -1;

  while (isspace (*p)) ++p;		/* skip whitespace.  */
  if (*p++ == '(')			/* skip past LPAREN.  */
    {
      int got_digit = 0;

      while (isspace (*p)) ++p;
      if (isdigit (*p))			/* single digit only... */
	{
	  got_digit = 1;
	  val = *p++ - '0';
	}
      while (isspace (*p)) ++p;
      if (*p != ')')
	val = got_digit = 0;

      if (! got_digit)
	warning("expected \"(0..9)\"");
    }

  return val;    
} 

/* Called from check_newline via the macro HANDLE_PRAGMA.
   PGETC and PUNGETC are pointers to the lexer's internal (un)getc routines.
   PNAME is the word after 'pragma'.

   Implementation Note: this was originally written to read a line's worth
   of text into LINEBUF and all further pragma processing takes place
   there.  From sheer laziness, I see no reason to change even though we
   now have PGETC, etc. :-)  */

int 
handle_pragma (pgetc, pungetc, pname)
  int (*pgetc)(void);
  void (*pungetc)(int);
  const char *pname;
{
  int  handled = 1;
  char lineBuf[2048];
  int  c, count = 0;
  char *p = lineBuf;

  /* Fill our buffer with the remainder of the "#pragma xxx" line.  */

  while ((c = pgetc ()) != EOF)
    {
      if (c == '\n')
	{
	  pungetc (c);
	  break;
	}
      if (++count < sizeof(lineBuf))	/* leave room for terminating 0.  */
        *p++ = c;
      else if (count == sizeof(lineBuf))
        warning ("#pragma line is too long - ignoring remainder of line");
    }                   /* getc(finput) != EOF  */
  *p = 0;
  p = lineBuf;          /* Reset to point to buffer start.  */

  /* Record initial setting of optimize flag, so we can restore it.  */
  if (!pragma_initialized)
    {
      pragma_initialized = 1;
      initial_optimize_flag = optimize;
      initial_optimize_size_flag = optimize_size;
      initial_flag_expensive_optimizations = flag_expensive_optimizations;
      initial_flag_altivec = flag_altivec;
#ifdef HAVE_COALESCED_SYMBOLS
      initial_flag_force_coalesced = flag_force_coalesced;
      initial_flag_privatize_coalesced = flag_privatize_coalesced;
#endif
#ifdef TARGET_TOC /* i.e., PowerPC  */
      initial_vrsave_save_type = standard_vrsave_save_type;
#endif
    }

#define	OPT_STRCMP(opt)	(strcmp(opt, pname) == 0)

#ifdef HAVE_COALESCED_SYMBOLS
  if (OPT_STRCMP ("force_coalesced"))
    {
      while (isspace (*p)) ++p;

      /* Ignore this unless we're generating dynamic code...  */
      if (flag_pic)
	if (!OnOffReset (p,&flag_force_coalesced,initial_flag_force_coalesced))
	  warning ("usage is: \"#pragma force_coalesced on | off | reset\"");
    }
  else
  if (OPT_STRCMP ("privatize_coalesced"))
    {
      while (isspace (*p)) ++p;

      /* Ignore this unless we're generating dynamic code...  */
      if (flag_pic)
	if (!OnOffReset (p, &flag_privatize_coalesced,
			 initial_flag_privatize_coalesced))
	  warning ("usage is: \"#pragma privatize_coalesced on | off | reset\"");
    }
  else
#endif
  if (OPT_STRCMP ("altivec_model"))
    {
      while (isspace (*p)) ++p;

      if (OnOffReset (p, &flag_altivec, initial_flag_altivec) == 0)
	warning ("usage is: \"#pragma altivec_model  on | off | reset\"");
    }
  else
  if (OPT_STRCMP ("altivec_codegen"))
    {
	/* Just ignore this pragma.  */
    }
  else
  if (OPT_STRCMP ("altivec_vrsave"))
    {
#ifdef TARGET_TOC
	while (isspace (*p)) ++p;

	/* From the MrC manual:

	   If the pragma is placed inside a function body (i.e., anywhere
	   between its enclosing braces), then the pragma temporarily
	   overrides the current setting for that function only.  */

	if (OnOffReset (p, &current_vrsave_save_type,
				initial_vrsave_save_type) == 0)
	  {
	    if (strcmp (p, "allon") == 0)
	      current_vrsave_save_type = VRSAVE_ALLON;
	    else
	      warning ("usage is: \"#pragma altivec_vrsave on | off | reset | allon\"");
	  }

	if (global_bindings_p ())
	  {
	    /* At global level, the "standard" setting is changed, not just 
	       the "current" setting.  (The "current" setting is reset to the
	       "standard" setting at the end of every function.)  */

	    if (current_vrsave_save_type == VRSAVE_ALLON)
	      error ("'altivec_vrsave allon' is only allowed inside a function (not at global binding level.)");
	    else
	      standard_vrsave_save_type = current_vrsave_save_type;
	  }
#endif	/* TARGET_TOC  */
    }
  else
  if (OPT_STRCMP ("unused"))
    {
      char name[1024];
      int malformed = 1;

      while (isspace (*p)) ++p;

      if (*p == '(')
      {
        while (*p && *p != ')')
        {
	  char *np = name;
	  tree decl;

	  ++p;					/* skip lparen or comma */
          while (isspace (*p)) ++p;		/* skip whitespace */

          while (*p == '_' || isalnum (*p))
	    *np++ = *p++;

          *np = 0;
	  if (name[0] != 0) malformed = 0;	/* got an ID */
	  decl = get_identifier (name);
          if (decl != 0)
	  {
	    tree local = IDENTIFIER_LOCAL_VALUE (decl);
	    if (local != 0 && (TREE_CODE (local) == PARM_DECL
			       || TREE_CODE (local) == VAR_DECL ))
	      TREE_USED (local) = 1;
	  }
        }					/* while */
	if (*p != ')')
	  malformed = 1;
      }						/* if *p == '(' */
      if (malformed)
	warning ("malformed #pragma unused ( var [,var ...] )");
    }
  else
  if (OPT_STRCMP ("once"))
    {
      /* do nothing -- this has already been handled by cpp, but is 
         passed through anyway.  */
    }
  else
  if (OPT_STRCMP ("mark") || OPT_STRCMP ("segment"))
    {
      /* do nothing -- this is commonly used in MacOS source files.  */
    }
  else
  if (OPT_STRCMP ("CC_OPT_ON"))
    {
      int num = get_optional_number(p);
      if (num <= 0) num = 1;

      optimize = num, obey_regdecls = 0;
      flag_expensive_optimizations = initial_flag_expensive_optimizations;
      warning ("optimization turned on - level %d", optimize);
    }
  else if (OPT_STRCMP ("CC_OPT_SIZE"))
    {
      int num = get_optional_number(p);
      optimize_size = 1;
      if (num <= 0) num = 2;
      optimize = (optimize > num) ? optimize : num;
      obey_regdecls = 0;
      flag_expensive_optimizations = initial_flag_expensive_optimizations;
      warning ("size optimization turned on - level %d", optimize); 
    }
  else if (OPT_STRCMP ("CC_OPT_OFF"))
    {
      optimize = 0, obey_regdecls = 1;
      flag_expensive_optimizations = 0;
      warning ("optimization turned off");
    }
  else if (OPT_STRCMP ("CC_OPT_RESTORE"))
    {
      if (optimize != initial_optimize_flag
          || optimize_size != initial_optimize_size_flag)
	{
	  if (initial_optimize_flag)
	    obey_regdecls = 0;
	  else
	    obey_regdecls = 1;
	  optimize = initial_optimize_flag;
          optimize_size = initial_optimize_size_flag;
	  flag_expensive_optimizations = initial_flag_expensive_optimizations;
	}
      warning ("optimization level restored (level %d%s)", optimize,
		(optimize_size) ? ", size" : "");
    }
  else if (OPT_STRCMP ("CC_WRITABLE_STRINGS"))
    flag_writable_strings = 1;
  else if (OPT_STRCMP ("CC_NON_WRITABLE_STRINGS"))
    flag_writable_strings = 0;
  else if (OPT_STRCMP ("CC_NO_MACH_TEXT_SECTIONS"))
    flag_no_mach_text_sections = 1;
#ifdef MODERN_OBJC_SYNTAX
  else if (OPT_STRCMP ("SELECTOR_ALIAS"))
    {
      char realname[1024], *rp = &(realname[0]);
      char alias[1024], *ap = &(alias[0]);

      /* This code is slightly more complex than it needs to be because
	 cpp-precomp insists on inserting whitespace when spitting out the
	 pragma.  For now, a workaround is better than requiring compiling
	 with -traditional-cpp.  */
      do
	{
	  while (*p && (isspace (*p))) p++;
	  if (*p && *p == '=') break;
	  while (*p && !isspace (*p)) *rp++ = *p++;
	} while (*p);
      *rp = 0;

      p++; /* Skip over the equal sign.  */

      do
	{
	  while (*p && (isspace (*p))) p++;
	  while (*p && !isspace (*p)) *ap++ = *p++;
	} while (*p);
      *ap = 0;

      objc_selector_alias_pragma (get_identifier (realname),
				  get_identifier (alias));
    }
  else if (OPT_STRCMP ("BEGIN_FUNCTION"))
    objc_begin_function_pragma ();
  else if (OPT_STRCMP ("END_FUNCTION"))
    objc_end_function_pragma ();
#endif
#ifdef APPLE_MAC68K_ALIGNMENT
  else if (OPT_STRCMP ("options"))
    {
      /* Look for something like that looks like: options align = some_word
	 where some_word is either mac68k, power, or reset.
	 Spaces on either side of the equal sign are optional.  */
      while (isspace (*p)) p++;
      if (strncmp (p, "align", 5))
	warning ("'#pragma options align={mac68k|power|reset}' is the only supported #pragma options");
      else
	{
	  for (p += 5; isspace (*p); p++);
	  if (*p != '=')
	    warning ("malformed '#pragma options align={mac68k|power|reset}'");
	  else
	    {
	      while (isspace (*++p));
	      if (!strncmp (p, "mac68k", 6))
		push_field_alignment (16);
	      else if (!strncmp (p, "power", 5))
		push_field_alignment (0);
	      else if (!strncmp (p, "reset", 5))
		pop_field_alignment ();
	      else
		warning ("malformed '#pragma options align={mac68k|power|reset}'");
	    }
	}
    }
  else if (OPT_STRCMP ("pack"))
    {
      /* Look for something like that looks like: pack ( 2 )
	 The spaces and whole number are optional.  */
      while (isspace (*p)) p++;
      if (*p != '(')
	warning ("malformed pragma pack(<AlignmentBytes>)");
      else
	{
	  long int i = strtol (++p, &p, 10);
	  while (isspace (*p)) p++;
	  if (i < 0 || i > 16 || *p != ')')
	    warning ("malformed pragma pack(0..16)");
	  else
	    push_field_alignment (i * 8);
	}
    }
#endif /* APPLE_MAC68K_ALIGNMENT */
  else if (OPT_STRCMP ("SECTION"))
    {
      char name[1024];
      char *q = &(name[0]);

      while (*p && (isspace (*p) || (*p == '.'))) p++;
      while (*p && !isspace (*p)) *q++ = *p++;
      *q = 0;

      while (*p && isspace (*p)) p++;
      if (*p == 0)
        alias_section (name, 0);
      else if (*p == '"')
        {
          char *start = ++p;
          while (*p && *p != '"')
            {
              if (*p == '\\') p++;
              p++;
            }
          *p = 0;
          alias_section (name, start);
        }
      else
        {
          alias_section (name, p);
        }
    }
  else if (OPT_STRCMP ("CALL_ON_MODULE_BIND") || OPT_STRCMP ("CALL_ON_LOAD"))
    {
      extern FILE *asm_out_file;

      while (*p && isspace (*p)) p++;

      if (*p)
        {
          mod_init_section ();
          fprintf (asm_out_file, "\t.long _%s\n", p);
        }
    }
  else if (OPT_STRCMP ("CALL_ON_UNLOAD"))
    {
      extern FILE *asm_out_file;

      while (*p && isspace (*p)) p++;

      if (*p)
        {
          mod_term_section ();
          fprintf (asm_out_file, "\t.long _%s\n", p);
        }
    }
  else /* not handled */
    {
      handled = 0;
      /* Issue a warning message if we have been asked to do so.
         Ignoring unknown pragmas in system header file unless
         an explcit -Wunknown-pragmas has been given.  */
      if (warn_unknown_pragmas > 1
          || (warn_unknown_pragmas && ! in_system_header))
        warning ("unknown pragma %s", pname);
    }

  return handled;
}

static int
name_needs_quotes(name)
     const char *name;
{
  int c;
  while ((c = *name++) != '\0')
    if (!isalnum(c) && c != '_')
      return 1;
  return 0;
}

/* This can be called instead of EXIT.  It will emit a '.abort' directive
   into any existing assembly file, causing assembly to immediately abort,
   thus preventing the assembler from spewing out numerous, irrelevant
   error messages.  */

void
abort_assembly_and_exit (status)
    int status;
{
  /* If we're aborting, get the assembler to abort, too.  */
  if (status == FATAL_EXIT_CODE && asm_out_file != 0)
    fprintf (asm_out_file, "\n.abort\n");

  exit (status);
}

#if defined (I386) || defined (MACHOPIC_M68K) || defined (TARGET_TOC) /* i.e., PowerPC  */
/* Go through all the insns looking for a double constant.  Return nonzero
   if one is found.  */

const_double_used ()
{
  rtx insn;
  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
    {
      enum rtx_code code;
      rtx set = single_set (insn);
      if (set)
	if ((code = GET_CODE (SET_SRC (set))) == CONST_DOUBLE)
	  return 1;
#ifdef MACHOPIC_M68K
        else
	  /* Hopefully this catches all the cases we're interested in.  */
	  switch (GET_RTX_CLASS (code))
	    {
	      int i;
	    case '<':
	    case '1':
	    case 'c':
	    case '2':
	      for (i = 0; i < GET_RTX_LENGTH (code); i++)
		if (GET_CODE (XEXP (SET_SRC (set), i)) == CONST_DOUBLE)
		  return 1;
	    }
#endif
    }
  return 0;
}
#endif /* I386 || MACHOPIC_M68K */

#define GEN_BINDER_NAME_FOR_STUB(BUF,STUB,STUB_LENGTH)		\
  do {								\
    const char *stub_ = (STUB);					\
    char *buffer_ = (BUF);					\
    strcpy(buffer_, stub_);					\
    if (stub_[0] == '"')					\
      {								\
	strcpy(buffer_ + (STUB_LENGTH) - 1, "_binder\"");	\
      }								\
    else							\
      {								\
	strcpy(buffer_ + (STUB_LENGTH), "_binder");		\
      }								\
  } while (0)

#define GEN_SYMBOL_NAME_FOR_SYMBOL(BUF,SYMBOL,SYMBOL_LENGTH)	\
  do {								\
    const char *symbol_ = (SYMBOL);				\
    char *buffer_ = (BUF);					\
    if (name_needs_quotes(symbol_) && symbol_[0] != '"')	\
      {								\
	  sprintf(buffer_, "\"%s\"", symbol_);			\
      }								\
    else							\
      {								\
	strcpy(buffer_, symbol_);				\
      }								\
  } while (0)

#define GEN_LAZY_PTR_NAME_FOR_SYMBOL(BUF,SYMBOL,SYMBOL_LENGTH)	\
  do {								\
    const char *symbol_ = (SYMBOL);				\
    char *buffer_ = (BUF);					\
    if (symbol_[0] == '"')					\
      {								\
        strcpy(buffer_, "\"L");					\
        strcpy(buffer_ + 2, symbol_ + 1);			\
	strcpy(buffer_ + (SYMBOL_LENGTH), "$lazy_ptr\"");	\
      }								\
    else if (name_needs_quotes(symbol_))			\
      {								\
        strcpy(buffer_, "\"L");					\
        strcpy(buffer_ + 2, symbol_);				\
	strcpy(buffer_ + (SYMBOL_LENGTH) + 2, "$lazy_ptr\"");	\
      }								\
    else							\
      {								\
        strcpy(buffer_, "L");					\
        strcpy(buffer_ + 1, symbol_);				\
	strcpy(buffer_ + (SYMBOL_LENGTH) + 1, "$lazy_ptr");	\
      }								\
  } while (0)


/* Output assembler code for a constant address expression for PIC
   code generation. This PIC address is computed by subtracting the
   address of the location holding the pointer from the pointer.
   This produces a position independent relative pointer which is
   trivial to turn back into an absolute address at runtime. Only
   minimal preprocessing of the tree occurs so a simple constant
   expression tree should be passed to this routine.

   This is a sawn-off version of OUTPUT_CONSTANT in varasm.c;
   we deal only with REFERENCE_TYPEs of typeinfo functions.

   This is used for the read-only ZOE (Dwarf2) tables.  */

void
output_const_pic_addr (exp, size)
	tree exp;
	int size;
{
  register enum tree_code code = TREE_CODE (TREE_TYPE (exp));
  rtx func_sym;
  char *name;

  if (size == 0 || flag_syntax_only)
    return;

  /* Eliminate the NON_LVALUE_EXPR_EXPR that makes a cast not be an lvalue.
     That way we get the constant (we hope) inside it.  Also, strip off any
     NOP_EXPR that converts between two record, union, array, or set types.  */
  while ((TREE_CODE (exp) == NOP_EXPR
	  && (TREE_TYPE (exp) == TREE_TYPE (TREE_OPERAND (exp, 0))
	      || AGGREGATE_TYPE_P (TREE_TYPE (exp))))
	 || TREE_CODE (exp) == NON_LVALUE_EXPR)
    exp = TREE_OPERAND (exp, 0);

  switch (code)
    {
    case POINTER_TYPE:
    case REFERENCE_TYPE:
      /* ??? What about       (int)((float)(int)&foo + 4)    */
      while (TREE_CODE (exp) == NOP_EXPR || TREE_CODE (exp) == CONVERT_EXPR
             || TREE_CODE (exp) == NON_LVALUE_EXPR)
        exp = TREE_OPERAND (exp, 0);

      fputs("\t.long ", asm_out_file) ;
      func_sym = expand_expr (exp, NULL_RTX, VOIDmode, EXPAND_INITIALIZER);
      if (GET_CODE (func_sym) != SYMBOL_REF)
	abort ();

      /* PIC entries may need to use a stub if the function is not defined
	 in this compilation unit.  */

      name = XSTR (func_sym, 0);
      if (flag_pic && ! machopic_name_defined_p (name))
	name = machopic_stub_name (name);

      assemble_name (asm_out_file, name);

      /* PIC based table entries are PC-relative.  */
      if (flag_pic)
        fputs ("-.\n", asm_out_file);
      break;

    default:
        abort();
    }
}

