/* Subroutines for the C front end on the POWER and PowerPC architectures.
   Copyright (C) 2002, 2003, 2004
   Free Software Foundation, Inc.

   Contributed by Zack Weinberg <zack@codesourcery.com>

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to the
   Free Software Foundation, 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "cpplib.h"
#include "tree.h"
#include "c-pragma.h"
#include "errors.h"
#include "tm_p.h"
/* APPLE LOCAL begin AltiVec */
#include "c-common.h"
#include "cpplib.h"
#include "../libcpp/internal.h"
#include "target.h"
#include "options.h"

static cpp_hashnode *altivec_categorize_keyword (const cpp_token *);
static void init_vector_keywords (cpp_reader *pfile);
/* APPLE LOCAL end AltiVec */


/* Handle the machine specific pragma longcall.  Its syntax is

   # pragma longcall ( TOGGLE )

   where TOGGLE is either 0 or 1.

   rs6000_default_long_calls is set to the value of TOGGLE, changing
   whether or not new function declarations receive a longcall
   attribute by default.  */

#define SYNTAX_ERROR(gmsgid) do {			\
  warning (gmsgid);					\
  warning ("ignoring malformed #pragma longcall");	\
  return;						\
} while (0)

void
rs6000_pragma_longcall (cpp_reader *pfile ATTRIBUTE_UNUSED)
{
  tree x, n;

  /* If we get here, generic code has already scanned the directive
     leader and the word "longcall".  */

  if (c_lex (&x) != CPP_OPEN_PAREN)
    SYNTAX_ERROR ("missing open paren");
  if (c_lex (&n) != CPP_NUMBER)
    SYNTAX_ERROR ("missing number");
  if (c_lex (&x) != CPP_CLOSE_PAREN)
    SYNTAX_ERROR ("missing close paren");

  if (n != integer_zero_node && n != integer_one_node)
    SYNTAX_ERROR ("number must be 0 or 1");

  if (c_lex (&x) != CPP_EOF)
    warning ("junk at end of #pragma longcall");

  rs6000_default_long_calls = (n == integer_one_node);
}

/* Handle defining many CPP flags based on TARGET_xxx.  As a general
   policy, rather than trying to guess what flags a user might want a
   #define for, it's better to define a flag for everything.  */

#define builtin_define(TXT) cpp_define (pfile, TXT)
#define builtin_assert(TXT) cpp_assert (pfile, TXT)

/* APPLE LOCAL begin AltiVec */
/* Keep the AltiVec keywords handy for fast comparisons.  */
static GTY(()) cpp_hashnode *__vector_keyword;
static GTY(()) cpp_hashnode *vector_keyword;
static GTY(()) cpp_hashnode *__pixel_keyword;
static GTY(()) cpp_hashnode *pixel_keyword;
static GTY(()) cpp_hashnode *__bool_keyword;
static GTY(()) cpp_hashnode *bool_keyword;
static GTY(()) cpp_hashnode *_Bool_keyword;

static GTY(()) cpp_hashnode *expand_bool_pixel;  /* Preserved across calls.  */

static cpp_hashnode *
altivec_categorize_keyword (const cpp_token *tok)
{
  if (tok->type == CPP_NAME)
    {
      cpp_hashnode *ident = tok->val.node;

      if (ident == vector_keyword || ident == __vector_keyword)
	return __vector_keyword;

      if (ident == pixel_keyword || ident ==  __pixel_keyword)
	return __pixel_keyword;
	
      if (ident == bool_keyword || ident == _Bool_keyword
	  || ident == __bool_keyword)
	return __bool_keyword;

      return ident;
    }

  return 0;
}

/* Called to decide whether a conditional macro should be expanded.
   Since we have exactly one such macro (i.e, 'vector'), we do not
   need to examine the 'tok' parameter.  */

cpp_hashnode *
rs6000_macro_to_expand (cpp_reader *pfile, const cpp_token *tok)
{
  static bool vector_keywords_init = false;
  cpp_hashnode *expand_this = tok->val.node;
  cpp_hashnode *ident;

  if (!vector_keywords_init)
    {
      init_vector_keywords (pfile);
      vector_keywords_init = true;
    }

  ident = altivec_categorize_keyword (tok);

  if (ident == __vector_keyword)
    {
      tok = _cpp_peek_token (pfile, 0);
      ident = altivec_categorize_keyword (tok);

      if (ident ==  __pixel_keyword || ident == __bool_keyword)
	{
	  expand_this = __vector_keyword;
	  expand_bool_pixel = ident;
	}
      else if (ident)
	{
	  enum rid rid_code = (enum rid)(ident->rid_code);
	  if (ident->type == NT_MACRO)
	    {
	      (void)cpp_get_token (pfile);
	      tok = _cpp_peek_token (pfile, 0);
	      ident = altivec_categorize_keyword (tok);
	      rid_code = (enum rid)(ident->rid_code);
	    }

	  if (rid_code == RID_UNSIGNED || rid_code == RID_LONG
	      || rid_code == RID_SHORT || rid_code == RID_SIGNED
	      || rid_code == RID_INT || rid_code == RID_CHAR
	      || rid_code == RID_FLOAT)
	    {
	      expand_this = __vector_keyword;
	      /* If the next keyword is bool or pixel, it
		 will need to be expanded as well.  */
	      tok = _cpp_peek_token (pfile, 1);
	      ident = altivec_categorize_keyword (tok);

	      if (ident ==  __pixel_keyword || ident == __bool_keyword)
		expand_bool_pixel = ident;
	    }
	}
    }
  else if (expand_bool_pixel
	   && (ident ==  __pixel_keyword || ident == __bool_keyword))
    {
      expand_this = expand_bool_pixel;
      expand_bool_pixel = 0;
    }

  return expand_this;
}

static void
init_vector_keywords (cpp_reader *pfile)
{
      /* Keywords without two leading underscores are context-sensitive, and hence
	 implemented as conditional macros, controlled by the rs6000_macro_to_expand()
	 function above.  */
      __vector_keyword = cpp_lookup (pfile, DSC ("__vector"));
      __vector_keyword->flags |= NODE_CONDITIONAL;

      __pixel_keyword = cpp_lookup (pfile, DSC ("__pixel"));
      __pixel_keyword->flags |= NODE_CONDITIONAL;

      __bool_keyword = cpp_lookup (pfile, DSC ("__bool"));
      __bool_keyword->flags |= NODE_CONDITIONAL;

      vector_keyword = cpp_lookup (pfile, DSC ("vector"));
      vector_keyword->flags |= NODE_CONDITIONAL;

      pixel_keyword = cpp_lookup (pfile, DSC ("pixel"));
      pixel_keyword->flags |= NODE_CONDITIONAL;

      _Bool_keyword = cpp_lookup (pfile, DSC ("_Bool"));
      _Bool_keyword->flags |= NODE_CONDITIONAL;

      bool_keyword = cpp_lookup (pfile, DSC ("bool"));
      bool_keyword->flags |= NODE_CONDITIONAL;
      return;
}

/* APPLE LOCAL end AltiVec */

void
rs6000_cpu_cpp_builtins (cpp_reader *pfile)
{
  if (TARGET_POWER2)
    builtin_define ("_ARCH_PWR2");
  else if (TARGET_POWER)
    builtin_define ("_ARCH_PWR");
  if (TARGET_POWERPC)
    builtin_define ("_ARCH_PPC");
  if (TARGET_POWERPC64)
    builtin_define ("_ARCH_PPC64");
  if (! TARGET_POWER && ! TARGET_POWER2 && ! TARGET_POWERPC)
    builtin_define ("_ARCH_COM");
  if (TARGET_ALTIVEC)
    {
      builtin_define ("__ALTIVEC__");
      builtin_define ("__VEC__=10206");

      /* Define the AltiVec syntactic elements.  */
      builtin_define ("__vector=__attribute__((altivec(vector__)))");
      builtin_define ("__pixel=__attribute__((altivec(pixel__))) unsigned short");
      builtin_define ("__bool=__attribute__((altivec(bool__))) unsigned");

      /* APPLE LOCAL begin AltiVec */
      builtin_define ("vector=vector");
      builtin_define ("pixel=pixel");
      builtin_define ("_Bool=_Bool"); 
      builtin_define ("bool=bool");
      init_vector_keywords (pfile);

      /* Indicate that the compiler supports Apple AltiVec syntax,
	 including context-sensitive keywords.  */
      if (rs6000_altivec_pim)
	{
	  builtin_define ("__APPLE_ALTIVEC__");
	  builtin_define ("vec_step(T)=(sizeof (__typeof__(T)) / sizeof (__typeof__(T) __attribute__((altivec(element__)))))");
	}

      /* Enable context-sensitive macros.  */
      cpp_get_callbacks (pfile)->macro_to_expand = rs6000_macro_to_expand;
      /* APPLE LOCAL end AltiVec */
    }
  if (TARGET_SPE)
    builtin_define ("__SPE__");
  if (TARGET_SOFT_FLOAT)
    builtin_define ("_SOFT_FLOAT");
  /* Used by lwarx/stwcx. errata work-around.  */
  if (rs6000_cpu == PROCESSOR_PPC405)
    builtin_define ("__PPC405__");

  /* May be overridden by target configuration.  */
  RS6000_CPU_CPP_ENDIAN_BUILTINS();

  if (TARGET_LONG_DOUBLE_128)
    builtin_define ("__LONG_DOUBLE_128__");

  switch (rs6000_current_abi)
    {
    case ABI_V4:
      builtin_define ("_CALL_SYSV");
      break;
    case ABI_AIX:
      builtin_define ("_CALL_AIXDESC");
      builtin_define ("_CALL_AIX");
      break;
    case ABI_DARWIN:
      builtin_define ("_CALL_DARWIN");
      break;
    default:
      break;
    }
}
