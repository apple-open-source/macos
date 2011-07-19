/*
 * Copyright (c) 2002 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/re/rep.h,v 1.2 2002/11/15 07:01:33 paulo Exp $ */

#include "re.h"

#ifndef _rep_h
#define _rep_h

/*
 * Local defines
 */

#ifdef MIN
#undef MIN
#endif
#define MIN(a, b)	((a) < (b) ? (a) : (b))

#ifdef MAX
#undef MAX
#endif
#define MAX(a, b)	((a) > (b) ? (a) : (b))

/*  This value can not be larger than 255, a depth value is the nesting of
 * repetition operations and alternatives. The number of nested parenthesis
 * does not matter, but a repetition on the pattern inside the parenthesis
 * does. Note also that you cannot have more than 9 parenthesis pairs in
 * an expression.
 *  Depth is always at least 1. So for MAX_DEPTH 8, it is only allowed
 * 7 complex repetitions. A complex repetition is a dot followed by an
 * repetition operator. It is called a complex repetition because dot
 * matches anything but the empty string, so the engine needs to test
 * all possible combinations until the end of the string is found.
 *  Repetitions like .* use one depth until the end of the string is found,
 * for example a.*b.*c.*d has depth 4, while a*b*c*d has depth 2.
 */
#define MAX_DEPTH	8

/*  Minimum number of strings to generate a "large" string list, that is,
 * sort the strings and allocate 512 extra bytes to map the first string
 * with a given initial byte. */
#define LARGE_STL_COUNT	16

/*
 * Local types
 */
/* Intermediate compilation types declaration */
	/* (r)egular (e)xpression (c)ompile (c)a(se) */
typedef struct _rec_cse rec_cse;

	/* (r)egular (e)xpression (c)ompile (r)a(ng)e */
typedef struct _rec_rng rec_rng;

	/* (r)egular (e)xpression (c)ompile (pat)tern */
typedef struct _rec_pat rec_pat;

	/* (r)egular (e)xpression (c)ompile (rep)etition */
typedef struct _rec_rep rec_rep;

	/* (r)egular (e)xpression (c)ompile (gr)ou(p) */
typedef struct _rec_grp rec_grp;

	/* (r)egular (e)xpression (c)ompile (alt)ernatives */
typedef struct _rec_alt rec_alt;


/* Optimization types */
	/* (r)egular (e)xpression (c)ompile (st)ring (l)ist */
typedef struct _rec_stl rec_stl;

/* Final compilation and execution types */
	/* (re)gular expression (inf)ormation */
typedef struct _re_inf re_inf;

	/* (re)gular expression (eng)ine */
typedef struct _re_eng re_eng;


/* Codes used by the engine */
typedef enum {
    /* Grouping */
    Re_Open,			/* ( */
    Re_Close,			/* ) */
    Re_Update,			/* Like Re_Close, but is inside a loop */

    /* Alternatives */
    Re_Alt,			/* Start alternative list, + next offset */
    Re_AltNext,			/* Next alternative, + next offset */
    Re_AltDone,			/* Finish alternative list */

    /* Repetition */
    Re_AnyTimes,		/* * */
    Re_Maybe,			/* ? */
    Re_AtLeast,			/* +, at least one */

    /* Repetition like */
    Re_AnyAnyTimes,		/* .*<re> */
    Re_AnyMaybe,		/* .?<re> */
    Re_AnyAtLeast,		/* .+<re> */

    Re_AnyEatAnyTimes,		/* Expression ends with .* */
    Re_AnyEatMaybe,		/* Expression ends with .? */
    Re_AnyEatAtLeast,		/* Expression ends with .+ */

    /* Repetition with arguments */
    Re_Exact,			/* {e} */
    Re_Min,			/* {n,} */
    Re_Max,			/* {,m} */
    Re_MinMax,			/* {n,m} */

    /* Repetition helper instruction */
    Re_RepJump,			/* Special code, go back to repetition */
    Re_RepLongJump,		/* Jump needs two bytes */
	/*  After the repetition data, all repetitions have an offset
	 * to the code after the repetition */

    /* Matching */
    Re_Any,			/* . */
    Re_Odigit,			/* \o */
    Re_OdigitNot,		/* \O */
    Re_Digit,			/* \d */
    Re_DigitNot,		/* \D */
    Re_Xdigit,			/* \x */
    Re_XdigitNot,		/* \x */
    Re_Space,			/* \s */
    Re_SpaceNot,		/* \S */
    Re_Tab,			/* \t */
    Re_Newline,			/* \n */
    Re_Lower,			/* \l */
    Re_Upper,			/* \u */
    Re_Alnum,			/* \w */
    Re_AlnumNot,		/* \W */
    Re_Control,			/* \c */
    Re_ControlNot,		/* \C */
    Re_Bol,			/* ^ */
    Re_Eol,			/* $ */
    Re_Bow,			/* \< */
    Re_Eow,			/* \> */

    /* Range matching information */
    Re_Range,			/* + 256 bytes */
    Re_RangeNot,		/* + 256 bytes */

    /* Matching with arguments */
    Re_Literal,			/* + character */
    Re_CaseLiteral,		/* + lower + upper */
    Re_LiteralNot,		/* + character */
    Re_CaseLiteralNot,		/* + lower + upper */
    Re_String,			/* + length + string */
    Re_CaseString,		/* + length + string in format lower-upper */

    /* These are useful to start matching, or when RE_NOSPEC is used. */
    Re_SearchLiteral,
    Re_SearchCaseLiteral,
    Re_SearchString,
    Re_SearchCaseString,

    Re_StringList,		/* + total-length + lengths + strings */
    Re_CaseStringList,		/* + total-length + lengths + strings */

    Re_LargeStringList,		/* + total-length + lengths + map + strings */
    Re_LargeCaseStringList,	/* + total-length + lengths + map + strings */

    /* Backreference */
    Re_Backref,			/* + reference number */

    /* The last codes */
    Re_DoneIf,			/* Done if at end of input */
    Re_MaybeDone,		/* Done */
    Re_Done			/* If this code found, finished execution */
} ReCode;


/* (r)egular (e)xpresssion (pat)rern (t)ype */
typedef enum _rec_pat_t {
    Rep_Literal			= Re_Literal,
    Rep_CaseLiteral		= Re_CaseLiteral,
    Rep_LiteralNot		= Re_LiteralNot,
    Rep_CaseLiteralNot		= Re_CaseLiteralNot,
    Rep_Range			= Re_Range,
    Rep_RangeNot		= Re_RangeNot,
    Rep_String			= Re_String,
    Rep_CaseString		= Re_CaseString,
    Rep_SearchLiteral		= Re_SearchLiteral,
    Rep_SearchCaseLiteral	= Re_SearchCaseLiteral,
    Rep_SearchString		= Re_SearchString,
    Rep_SearchCaseString	= Re_SearchCaseString,
    Rep_Any			= Re_Any,
    Rep_AnyAnyTimes		= Re_AnyAnyTimes,
    Rep_AnyEatAnyTimes		= Re_AnyEatAnyTimes,
    Rep_AnyMaybe		= Re_AnyMaybe,
    Rep_AnyEatMaybe		= Re_AnyEatMaybe,
    Rep_AnyAtLeast		= Re_AnyAtLeast,
    Rep_AnyEatAtLeast		= Re_AnyEatAtLeast,
    Rep_Odigit			= Re_Odigit,
    Rep_OdigitNot		= Re_OdigitNot,
    Rep_Digit			= Re_Digit,
    Rep_DigitNot		= Re_DigitNot,
    Rep_Xdigit			= Re_Xdigit,
    Rep_XdigitNot		= Re_XdigitNot,
    Rep_Space			= Re_Space,
    Rep_SpaceNot		= Re_SpaceNot,
    Rep_Tab			= Re_Tab,
    Rep_Newline			= Re_Newline,
    Rep_Lower			= Re_Lower,
    Rep_Upper			= Re_Upper,
    Rep_Alnum			= Re_Alnum,
    Rep_AlnumNot		= Re_AlnumNot,
    Rep_Control			= Re_Control,
    Rep_ControlNot		= Re_ControlNot,
    Rep_Bol			= Re_Bol,
    Rep_Eol			= Re_Eol,
    Rep_Bow			= Re_Bow,
    Rep_Eow			= Re_Eow,
    Rep_Backref			= Re_Backref,
    Rep_StringList		= Re_StringList,
    Rep_Group			= Re_Open
} rec_pat_t;


/* (r)egular (e)xpression (rep)etition (t)ype */
typedef enum _rec_rep_t {
    Rer_AnyTimes		= Re_AnyTimes,
    Rer_AtLeast			= Re_AtLeast,
    Rer_Maybe			= Re_Maybe,
    Rer_Exact			= Re_Exact,
    Rer_Min			= Re_Min,
    Rer_Max			= Re_Max,
    Rer_MinMax			= Re_MinMax
} rec_rep_t;


/*  Decide at re compilation time what is lowercase and what is uppercase */
struct _rec_cse {
    unsigned char lower;
    unsigned char upper;
};


/*  A rec_rng is used only during compilation, just a character map */
struct _rec_rng {
    unsigned char range[256];
};


/*  A rec_pat is used only during compilation, and can be viewed as
 * a regular expression element like a match to any character, a match
 * to the beginning or end of the line, etc.
 *  It is implemented as a linked list, and does not have nesting.
 *  The data field can contain:
 *	chr:	the value of a single character to match.
 *	cse:	the upper and lower case value of a character to match.
 *	rng:	a character map to match or not match.
 *	str:	a simple string or a string where every two bytes
 *		represents the character to match, in lower/upper
 *		case sequence.
 *  The rep field is not used for strings, strings are broken in the
 * last character in this case. That is, strings are just a concatenation
 * of several character matches.
 */
struct _rec_pat {
    rec_pat_t type;
    rec_pat *next, *prev;	/* Linked list information */
    union {
	unsigned char chr;
	rec_cse cse;
	rec_rng *rng;
	rec_grp *grp;
	unsigned char *str;
	rec_stl *stl;
    } data;
    rec_rep *rep;		/* Pattern repetition information */
};


/*  A rec_rep is used only during compilation, and can be viewed as:
 *
 *	? or * or + or {<e>} or {<m>,} or {,<M>} or {<m>,<M>}
 *
 * where <e> is "exact", <m> is "minimum" and <M> is "maximum".
 *  In the compiled step it can also be just a NULL pointer, that
 * is actually equivalent to {1}.
 */
struct _rec_rep {
    rec_rep_t type;
    short mine;			/* minimum or exact number of matches */
    short maxc;			/* maximum number of matches */
};


/*  A rec_alt is used only during compilation, and can be viewed as:
 *
 *	<re>|<re>
 *
 * where <re> is any regular expression. The expressions are nested
 * using the grp field of the rec_pat structure.
 */
struct _rec_alt {
    rec_alt *next, *prev;	/* Linked list information */
    rec_pat *pat;
};


/*  A rec_grp is a place holder for expressions enclosed in parenthesis
 * and is linked to the compilation data by an rec_pat structure. */
struct _rec_grp {
    rec_pat *parent;		/* Reference to parent pattern */
    rec_alt *alt;		/* The pattern information */
    rec_alt *palt;		/* Parent alternative */
    rec_grp *pgrp;		/* Nested groups */
    int comp;			/* (comp)lex repetition pattern inside group */
};


/* Optimization compilation types definition */
	/* (r)egular (e)xpression (c)ompile (st)ring (l)ist (t)ype */
typedef enum {
    Resl_StringList		= Re_StringList,
    Resl_CaseStringList		= Re_CaseStringList
} rec_stl_t;

struct _rec_stl {
    rec_stl_t type;
    int nstrs;			/* Number of strings in list */
    int tlen;			/* Total length of all strings */
    unsigned char *lens;	/* Vector of string lengths */
    unsigned char **strs;	/* The strings */
};


/*
 * Prototypes
 */
	/* rep.c */
rec_alt *irec_comp(const char*, const char*, int, int*);
void irec_free_alt(rec_alt*);

	/* reo.c */
int orec_comp(rec_alt*, int);
void orec_free_stl(rec_stl*);

#endif /* _rep_h */
