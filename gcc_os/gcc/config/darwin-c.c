/* Darwin support needed only by C/C++ frontends.
   Copyright (C) 2001
   Free Software Foundation, Inc.
   Contributed by Apple Computer Inc.

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

#include "config.h"
#include "system.h"
#include "cpplib.h"
#include "tree.h"
#include "c-pragma.h"
#include "c-lex.h"
#include "c-tree.h"
#include "toplev.h"
#include "tm_p.h"

/* Pragmas.  */

#define BAD(msgid) do { warning (msgid); return; } while (0)
#define BAD2(msgid, arg) do { warning (msgid, arg); return; } while (0)

/* APPLE LOCAL CALL_ON_LOAD/CALL_ON_UNLOAD pragmas  20020202 turly  */
static void directive_with_named_function (const char *, void (*sec_f)(void));

/* Maintain a small stack of alignments.  This is similar to pragma
   pack's stack, but simpler.  */

/* APPLE LOCAL begin Macintosh alignment 2001-12-17 ff */
static void push_field_alignment PARAMS ((int, int, int));
/* APPLE LOCAL end Macintosh alignment 2001-12-17 ff */
static void pop_field_alignment PARAMS ((void));

/* APPLE LOCAL begin Macintosh alignment 2002-1-22 ff */
/* There are four alignment modes supported on the Apple Macintosh
   platform: power, mac68k, natural, and packed.  These modes are
   identified as follows:
     if maximum_field_alignment != 0
       mode = packed
     else if TARGET_ALIGN_NATURAL
       mode = natural
     else if TARGET_ALIGN_MAC68K
       mode
     else
       mode = power
   These modes are saved on the alignment stack by saving the values
   of maximum_field_alignment, TARGET_ALIGN_MAC68K, and 
   TARGET_ALIGN_NATURAL.  */
typedef struct align_stack
{
  int alignment;
  unsigned long mac68k;
  unsigned long natural;
  struct align_stack * prev;
} align_stack;
/* APPLE LOCAL end Macintosh alignment 2002-1-22 ff */

static struct align_stack * field_align_stack = NULL;

/* APPLE LOCAL begin Macintosh alignment 2001-12-17 ff */
static void
push_field_alignment (bit_alignment, mac68k_alignment, natural_alignment)
     int bit_alignment;
     int mac68k_alignment;
     int natural_alignment;
{
  align_stack *entry = (align_stack *) xmalloc (sizeof (align_stack));

  entry->alignment = maximum_field_alignment;
  entry->mac68k = TARGET_ALIGN_MAC68K;
  entry->natural = TARGET_ALIGN_NATURAL;
  entry->prev = field_align_stack;
  field_align_stack = entry;

  maximum_field_alignment = bit_alignment;
  if (mac68k_alignment)
    target_flags |= MASK_ALIGN_MAC68K;
  else
    target_flags &= ~MASK_ALIGN_MAC68K;
  if (natural_alignment)
    target_flags |= MASK_ALIGN_NATURAL;
  else
    target_flags &= ~MASK_ALIGN_NATURAL;
}
/* APPLE LOCAL end Macintosh alignment 2001-12-17 ff */

static void
pop_field_alignment ()
{
  if (field_align_stack)
    {
      align_stack *entry = field_align_stack;

      maximum_field_alignment = entry->alignment;
/* APPLE LOCAL begin Macintosh alignment 2001-12-17 ff */
      if (entry->mac68k)
	target_flags |= MASK_ALIGN_MAC68K;
      else
	target_flags &= ~MASK_ALIGN_MAC68K;
      if (entry->natural)
	target_flags |= MASK_ALIGN_NATURAL;
      else
	target_flags &= ~MASK_ALIGN_NATURAL;
/* APPLE LOCAL end Macintosh alignment 2001-12-17 ff */
      field_align_stack = entry->prev;
      free (entry);
    }
  else
    error ("too many #pragma options align=reset");
}

/* Handlers for Darwin-specific pragmas.  */

void
darwin_pragma_ignore (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  /* Do nothing.  */
}

/* #pragma options align={mac68k|power|reset} */

void
darwin_pragma_options (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  /* APPLE LOCAL const char *  */
  const char *arg;
  tree t, x;

  if (c_lex (&t) != CPP_NAME)
    BAD ("malformed '#pragma options', ignoring");
  arg = IDENTIFIER_POINTER (t);
  if (strcmp (arg, "align"))
    BAD ("malformed '#pragma options', ignoring");
  if (c_lex (&t) != CPP_EQ)
    BAD ("malformed '#pragma options', ignoring");
  if (c_lex (&t) != CPP_NAME)
    BAD ("malformed '#pragma options', ignoring");

  if (c_lex (&x) != CPP_EOF)
    warning ("junk at end of '#pragma options'");

  arg = IDENTIFIER_POINTER (t);
/* APPLE LOCAL begin Macintosh alignment 2002-1-22 ff */
  if (!strcmp (arg, "mac68k"))
    push_field_alignment (0, 1, 0);
  else if (!strcmp (arg, "native"))	/* equivalent to power on PowerPC */
    push_field_alignment (0, 0, 0);
  else if (!strcmp (arg, "natural"))
    push_field_alignment (0, 0, 1);
  else if (!strcmp (arg, "packed"))
    push_field_alignment (8, 0, 0);
  else if (!strcmp (arg, "power"))
    push_field_alignment (0, 0, 0);
  else if (!strcmp (arg, "reset"))
    pop_field_alignment ();
  else
    warning ("malformed '#pragma options align={mac68k|power|natural|reset}', ignoring");
/* APPLE LOCAL end Macintosh alignment 2002-1-22 ff */
}

/* APPLE LOCAL begin Macintosh alignment 2002-1-22 ff */
/* #pragma pack ()
   #pragma pack (N)  
   
   We have a problem handling the semantics of these directives since,
   to play well with the Macintosh alignment directives, we want the
   usual pack(N) form to do a push of the previous alignment state.
   Do we want pack() to do another push or a pop?  */

void
darwin_pragma_pack (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  tree x;
  int align = -1;
  enum cpp_ttype token;
  enum { set, push, pop } action;

  if (c_lex (&x) != CPP_OPEN_PAREN)
    BAD ("missing '(' after '#pragma pack' - ignored");
  token = c_lex (&x);
  if (token == CPP_CLOSE_PAREN)
    {
      action = pop;  		/* or "set" ???  */    
      align = 0;
    }
  else if (token == CPP_NUMBER)
    {
      align = TREE_INT_CST_LOW (x);
      action = push;
      if (c_lex (&x) != CPP_CLOSE_PAREN)
	BAD ("malformed '#pragma pack' - ignored");
    }
  else
    BAD ("malformed '#pragma pack' - ignored");

  if (c_lex (&x) != CPP_EOF)
    warning ("junk at end of '#pragma pack'");
    
  switch (align)
    {
    case 0:
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
      align *= BITS_PER_UNIT;
      break;
    default:
      BAD2 ("alignment must be a small power of two, not %d", align);
    }
  
  switch (action)
    {
    case pop:   pop_field_alignment ();		      break;
    case push:  push_field_alignment (align, 0, 0);   break;
    case set:   				      break;
    }
}
/* APPLE LOCAL end Macintosh alignment 2002-1-22 ff */

/* #pragma unused ([var {, var}*]) */

void
darwin_pragma_unused (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  tree decl, x;
  int tok;

  if (c_lex (&x) != CPP_OPEN_PAREN)
    BAD ("missing '(' after '#pragma unused', ignoring");

  while (1)
    {
      tok = c_lex (&decl);
      if (tok == CPP_NAME && decl)
	{
	  tree local = IDENTIFIER_LOCAL_VALUE (decl);
	  if (local && (TREE_CODE (local) == PARM_DECL
			|| TREE_CODE (local) == VAR_DECL))
	    TREE_USED (local) = 1;
	  tok = c_lex (&x);
	  if (tok != CPP_COMMA)
	    break;
	}
    }

  if (tok != CPP_CLOSE_PAREN)
    BAD ("missing ')' after '#pragma unused', ignoring");

  if (c_lex (&x) != CPP_EOF)
    warning ("junk at end of '#pragma unused'");
}

/* APPLE LOCAL begin CALL_ON_LOAD/CALL_ON_UNLOAD pragmas  20020202 turly  */
extern void mod_init_section (void), mod_term_section (void);
/* Grab the function name from the pragma line and output it to the
   assembly output file with the parameter DIRECTIVE.  Called by the
   pragma CALL_ON_LOAD and CALL_ON_UNLOAD handlers below.
   So: "#pragma CALL_ON_LOAD foo"  will output ".mod_init_func _foo".  */

static void directive_with_named_function (
     const char *pragma_name,
     void (*section_function) (void))
{
  tree decl;
  int tok;

  tok = c_lex (&decl);
  if (tok == CPP_NAME && decl)
    {
      extern FILE *asm_out_file;

      section_function ();
      fprintf (asm_out_file, "\t.long _%s\n", IDENTIFIER_POINTER (decl));

      if (c_lex (&decl) != CPP_EOF)
	warning ("junk at end of #pragma %s <function_name>\n", pragma_name);
    }
  else
    warning ("function name expected after #pragma %s\n", pragma_name);
}
void
darwin_pragma_call_on_load (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  directive_with_named_function ("CALL_ON_LOAD", mod_init_section);
}
void
darwin_pragma_call_on_unload (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  directive_with_named_function ("CALL_ON_UNLOAD", mod_term_section);
}
/* APPLE LOCAL end CALL_ON_LOAD/CALL_ON_UNLOAD pragmas  20020202 turly  */

/* APPLE LOCAL begin CALL_ON_MODULE_BIND deprecated 2002-4-10 ff */
void
darwin_pragma_call_on_module_bind (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  warning ("#pragma CALL_ON_MODULE_BIND is no longer supported, ignoring.  "
  	   "Use CALL_ON_LOAD instead.");
}
/* APPLE LOCAL end CALL_ON_MODULE_BIND deprecated 2002-4-10 ff */

/* APPLE LOCAL begin temporary pragmas 2001-07-05 sts */
/* These need to live only long enough to get their uses flushed out
   of the system.  */
void
darwin_pragma_cc_no_mach_text_sections (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  warning ("#pragma CC_NO_MACH_TEXT_SECTIONS is no longer supported, ignoring");
}

void
darwin_pragma_cc_opt_off (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  warning ("#pragma CC_OPT_OFF is no longer supported, ignoring");
}

void
darwin_pragma_cc_opt_on (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  warning ("#pragma CC_OPT_ON is no longer supported, ignoring");
}

void
darwin_pragma_cc_opt_restore (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  warning ("#pragma CC_OPT_RESTORE is no longer supported, ignoring");
}

void
darwin_pragma_cc_writable_strings (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  warning ("#pragma CC_WRITABLE_STRINGS is no longer supported, ignoring");
}

void
darwin_pragma_cc_non_writable_strings (pfile)
     cpp_reader *pfile ATTRIBUTE_UNUSED;
{
  warning ("#pragma CC_NON_WRITABLE_STRINGS is no longer supported, ignoring");
}
/* APPLE LOCAL end temporary pragmas 2001-07-05 sts */
