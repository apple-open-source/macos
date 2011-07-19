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

/* $XFree86: xc/programs/xedit/lisp/re/reo.c,v 1.8 2002/09/29 02:55:01 paulo Exp $ */

#include "rep.h"

/*
 *  This file is a placeholder to add code to analyse and optimize the
 * intermediate data structure generated in rep.c.
 *  Character ranges are optimized while being generated.
 */

/*
 * Types
 */
typedef struct _orec_inf {
    rec_alt *alt;			/* Main alternatives list */
    rec_grp *grp;			/* Current group pointer */
    int flags;
    int ecode;
} orec_inf;

/*
 * Prototypes
 */
static int orec_alt(orec_inf*, rec_alt*);
static int orec_pat(orec_inf*, rec_pat*);
static int orec_grp(orec_inf*, rec_grp*);
static int orec_pat_bad_rpt(orec_inf*, rec_pat*);
static int orec_pat_bad_forward_rpt(orec_inf*, rec_pat*);
static int orec_pat_rng(orec_inf*, rec_pat*);
static int orec_pat_cse(orec_inf*, rec_pat*);
static int orec_pat_cse_can(orec_inf*, rec_pat*);
static int orec_str_list(orec_inf*, rec_alt*, int, int);

/*
 * Initialization
 */
extern unsigned char re__alnum[256];
extern unsigned char re__odigit[256];
extern unsigned char re__ddigit[256];
extern unsigned char re__xdigit[256];
extern unsigned char re__control[256];

/*
 * Implementation
 */
int
orec_comp(rec_alt *alt, int flags)
{
    orec_inf inf;

    inf.alt = alt;
    inf.grp = NULL;
    inf.flags = flags;
    inf.ecode = 0;

    orec_alt(&inf, alt);

    return (inf.ecode);
}

void
orec_free_stl(rec_stl *stl)
{
    int i;

    for (i = 0; i < stl->nstrs; i++) {
	if (stl->lens[i] > 2)
	    free(stl->strs[i]);
    }

    free(stl->lens);
    free(stl->strs);
    free(stl);
}


static int
orec_alt(orec_inf *inf, rec_alt *alt)
{
    if (alt) {
	rec_alt *ptr = alt;
	int ret, count = 0, str = 1, cstr = 1, lits = 0, clits = 0;

	/* Check if can build a string list */
	if (ptr->next) {
	    /* If more than one alternative */
	    while (ptr && (str || cstr)) {
		if (ptr->pat == NULL || ptr->pat->rep != NULL) {
		    cstr = str = 0;
		    break;
		}
		if ((inf->flags & RE_ICASE)) {
		    if (!(ret = orec_pat_cse_can(inf, ptr->pat))) {
			cstr = str = 0;
			break;
		    }
		    if (ret == 1)
			++lits;
		    else if (ret == 2)
			++clits;
		}
		else if (ptr->pat->next == NULL) {
		    if (ptr->pat->type != Rep_String) {
			if (ptr->pat->type != Rep_Literal) {
			    str = 0;
			    if (ptr->pat->type != Rep_CaseString) {
				if (ptr->pat->type != Rep_CaseLiteral)
				    cstr = 0;
				else
				    ++clits;
			    }
			    else if (strlen((char*)ptr->pat->data.str) >= 255)
				str = cstr = 0;
			}
			else
			    ++lits;
		    }
		    else if (strlen((char*)ptr->pat->data.str) >= 255)
			str = cstr = 0;
		}
		else {
		    str = cstr = 0;
		    break;
		}
		if (++count >= 255)
		    str = cstr = 0;
		ptr = ptr->next;
	    }

	    if (str || cstr) {
		if (inf->flags & RE_ICASE) {
		    for (ptr = alt; ptr; ptr = ptr->next) {
			if (orec_pat_cse(inf, ptr->pat))
			    return (inf->ecode);
		    }
		    str = 0;
		}
		return (orec_str_list(inf, alt, str, count));
	    }
	}
	else if (alt == inf->alt && alt->pat && alt->pat->rep == NULL) {
	    /* If the toplevel single alternative */
	    switch (alt->pat->type) {
		/*  One of these will always be true for RE_NOSPEC,
		 * but can also be optimized for simple patterns */
		case Rep_Literal:
		    alt->pat->type = Rep_SearchLiteral;
		    break;
		case Rep_CaseLiteral:
		    alt->pat->type = Rep_SearchCaseLiteral;
		    break;
		case Rep_String:
		    alt->pat->type = Rep_SearchString;
		    break;
		case Rep_CaseString:
		    alt->pat->type = Rep_SearchCaseString;
		    break;
		default:
		    break;
	    }
	}

	while (alt) {
	    orec_pat(inf, alt->pat);
	    alt = alt->next;
	}
    }

    return (inf->ecode);
}

static int
orec_pat(orec_inf *inf, rec_pat *pat)
{
    rec_pat *next;

    while (pat) {
	switch (pat->type) {
	    case Rep_AnyAnyTimes:
		if (pat->next == NULL) {
		    rec_grp *grp = inf->grp;

		    next = NULL;
		    while (grp) {
			next = grp->parent->next;
			/* Cannot check if is .*$ as the input
			 * may be a substring */
			if (next)
			    break;
			grp = grp->pgrp;
		    }
		    if (next == NULL) {
			/*    <re>.*    */
			pat->type = Rep_AnyEatAnyTimes;
			grp = inf->grp;
			while (grp) {
			    --grp->comp;
			    next = grp->parent->next;
			    if (next)
				break;
			    grp = grp->pgrp;
			}
		    }
		    else if (orec_pat_bad_rpt(inf, next))
			return (inf->ecode);
		}
		else if (orec_pat_bad_rpt(inf, pat->next))
		    return (inf->ecode);
		break;
	    case Rep_AnyMaybe:
		if (pat->next == NULL) {
		    rec_grp *grp = inf->grp;

		    next = NULL;
		    while (grp) {
			next = grp->parent->next;
			if (next)
			    break;
			grp = grp->pgrp;
		    }
		    if (next == NULL) {
			/*    <re>.?    */
			pat->type = Rep_AnyEatMaybe;
			grp = inf->grp;
			while (grp) {
			    --grp->comp;
			    next = grp->parent->next;
			    if (next)
				break;
			    grp = grp->pgrp;
			}
		    }
		    else if (orec_pat_bad_rpt(inf, next))
			return (inf->ecode);
		}
		else if (orec_pat_bad_rpt(inf, pat->next))
		    return (inf->ecode);
		break;
	    case Rep_AnyAtLeast:
		if (pat->next == NULL) {
		    rec_grp *grp = inf->grp;

		    next = NULL;
		    while (grp) {
			next = grp->parent->next;
			if (next)
			    break;
			grp = grp->pgrp;
		    }
		    if (next == NULL) {
			/*    <re>.+    */
			pat->type = Rep_AnyEatAtLeast;
			grp = inf->grp;
			while (grp) {
			    --grp->comp;
			    next = grp->parent->next;
			    if (next)
				break;
			    grp = grp->pgrp;
			}
		    }
		    else if (orec_pat_bad_rpt(inf, next))
			return (inf->ecode);
		}
		else if (orec_pat_bad_rpt(inf, pat->next))
		    return (inf->ecode);
		break;
	    case Rep_Range:
	    case Rep_RangeNot:
		orec_pat_rng(inf, pat);
		break;
	    case Rep_Group:
		orec_grp(inf, pat->data.grp);
		break;
	    default:
		break;
	}
	pat = pat->next;
    }

    return (inf->ecode);
}

static int
orec_pat_bad_rpt(orec_inf *inf, rec_pat *pat)
{
    switch (pat->type) {
	/* Not really an error, but aren't supported by the library.
	 * Includes:  .*.*, .+<re>?  .*<re>*, (.*)(<re>*), etc.
	 */

	/* Not a repetition, but mathes anything... */
	case Rep_Any:

	/* Zero length matches */
	case Rep_Eol:
	    if (!(inf->flags & RE_NEWLINE))
		break;
	case Rep_Bol:
	case Rep_Bow:
	case Rep_Eow:

	/* Repetitions */
	case Rep_AnyAnyTimes:
	case Rep_AnyMaybe:
	case Rep_AnyAtLeast:
	    inf->ecode = RE_BADRPT;
	    break;

	/* Check if the first group element is a complex pattern */
	case Rep_Group:
	    if (pat->rep == NULL) {
		if (pat->data.grp->alt) {
		    for (pat = pat->data.grp->alt->pat; pat; pat = pat->next) {
			if (orec_pat_bad_rpt(inf, pat))
			    break;
		    }
		}
		break;
	    }
	    /*FALLTHROUGH*/
	default:
	    if (pat->rep)
		inf->ecode = RE_BADRPT;
	    break;
    }

    if (!inf->ecode && pat && pat->next)
	orec_pat_bad_forward_rpt(inf, pat->next);

    return (inf->ecode);
}

static int
orec_pat_bad_forward_rpt(orec_inf *inf, rec_pat *pat)
{
    if (pat->rep) {
	switch (pat->rep->type) {
	    case Rer_MinMax:
		if (pat->rep->mine > 0)
		    break;
	    case Rer_AnyTimes:
	    case Rer_Maybe:
	    case Rer_Max:
		inf->ecode = RE_BADRPT;
	    default:
		break;
	}
    }
    else if (pat->type == Rep_Group &&
	     pat->data.grp->alt &&
	     pat->data.grp->alt->pat)
	orec_pat_bad_forward_rpt(inf, pat->data.grp->alt->pat);

    return (inf->ecode);
}

static int
orec_grp(orec_inf *inf, rec_grp *grp)
{
    rec_grp *prev = inf->grp;

    inf->grp = grp;
    orec_alt(inf, grp->alt);
    /* Could also just say: inf->grp = grp->gparent */
    inf->grp = prev;

    return (inf->ecode);
}

static int
orec_pat_rng(orec_inf *inf, rec_pat *pat)
{
    int i, j[2], count;
    rec_pat_t type = pat->type;
    unsigned char *range = pat->data.rng->range;

    for (i = count = j[0] = j[1] = 0; i < 256; i++) {
	if (range[i]) {
	    if (count == 2) {
		++count;
		break;
	    }
	    j[count++] = i;
	}
    }

    if (count == 1 ||
	(count == 2 &&
	 ((islower(j[0]) && toupper(j[0]) == j[1]) ||
	  (isupper(j[0]) && tolower(j[0]) == j[1])))) {
	free(pat->data.rng);
	if (count == 1) {
	    pat->data.chr = j[0];
	    pat->type = type == Rep_Range ? Rep_Literal : Rep_LiteralNot;
	}
	else {
	    pat->data.cse.upper = j[0];
	    pat->data.cse.lower = j[1];
	    pat->type = type == Rep_Range ? Rep_CaseLiteral : Rep_CaseLiteralNot;
	}
    }
    else {
	if (memcmp(re__alnum, range, 256) == 0)
	    type = type == Rep_Range ? Rep_Alnum : Rep_AlnumNot;
	else if (memcmp(re__odigit, range, 256) == 0)
	    type = type == Rep_Range ? Rep_Odigit : Rep_OdigitNot;
	else if (memcmp(re__ddigit, range, 256) == 0)
	    type = type == Rep_Range ? Rep_Digit : Rep_DigitNot;
	else if (memcmp(re__xdigit, range, 256) == 0)
	    type = type == Rep_Range ? Rep_Xdigit : Rep_XdigitNot;
	else if (memcmp(re__control, range, 256) == 0)
	    type = type == Rep_Range ? Rep_Control : Rep_ControlNot;

	if (type != pat->type) {
	    free(pat->data.rng);
	    pat->type = type;
	}
    }

    return (inf->ecode);
}

/*  Join patterns if required, will only fail on memory allocation failure:
 */
static int
orec_pat_cse(orec_inf *inf, rec_pat *pat)
{
    rec_pat_t type;
    int i, len, length;
    rec_pat *ptr, *next;
    unsigned char *str, *tofree;

    if (pat->next == NULL && pat->type == Rep_CaseString)
	return (inf->ecode);

    type = Rep_CaseString;

    /* First calculate how many bytes will be required */
    for (ptr = pat, length = 1; ptr; ptr = ptr->next) {
	switch (ptr->type) {
	    case Rep_Literal:
		length += 2;
		break;
	    case Rep_String:
		length += strlen((char*)ptr->data.str) << 1;
		break;
	    case Rep_CaseLiteral:
		length += 2;
		break;
	    case Rep_CaseString:
		length += strlen((char*)ptr->data.str);
		break;
	    default:
		break;
	}
    }

    if ((str = malloc(length)) == NULL)
	return (inf->ecode = RE_ESPACE);

    for (ptr = pat, length = 0; ptr; ptr = next) {
	tofree = NULL;
	next = ptr->next;
	switch (ptr->type) {
	    case Rep_Literal:
		str[length++] = ptr->data.chr;
		str[length++] = ptr->data.chr;
		break;
	    case Rep_String:
		tofree = ptr->data.str;
		len = strlen((char*)tofree);
		for (i = 0; i < len; i++) {
		    str[length++] = tofree[i];
		    str[length++] = tofree[i];
		}
		break;
	    case Rep_CaseLiteral:
		str[length++] = ptr->data.cse.lower;
		str[length++] = ptr->data.cse.upper;
		break;
	    case Rep_CaseString:
		tofree = ptr->data.str;
		len = strlen((char*)tofree);
		memcpy(str + length, tofree, len);
		length += len;
		break;
	    default:
		break;
	}
	if (tofree)
	    free(tofree);
	if (ptr != pat)
	    free(ptr);
    }
    str[length] = '\0';

    pat->type = type;
    pat->data.str = str;
    pat->next = NULL;

    return (inf->ecode);
}

/*  Return 0 if the patterns in the list cannot be merged, 1 if will
 * be a simple string, 2 if a case string.
 *  This is useful when building an alternative list that is composed
 * only of strings, but the regex is case insensitive, in wich case
 * the first pass may have splited some patterns, but if it is a member
 * of an alternatives list, the cost of using a string list is smaller */
static int
orec_pat_cse_can(orec_inf *inf, rec_pat *pat)
{
    int ret;

    if (pat == NULL)
	return (0);

    for (ret = 1; pat; pat = pat->next) {
	if (pat->rep)
	    return (0);
	switch (pat->type) {
	    case Rep_Literal:
	    case Rep_String:
		break;
	    case Rep_CaseLiteral:
	    case Rep_CaseString:
		ret = 2;
		break;
	    default:
		return (0);
	}
    }

    return (ret);
}


/*  XXX If everything is a (case) byte, the pattern should be
 * [abcde] instead of a|b|c|d|e (or [aAbBcCdDeE] instead of aA|bB|cC|dD|eE)
 * as a string list works fine, but as a character range
 * should be faster, and maybe could be converted here. But not
 * very important, if performance is required, it should have already
 * been done in the pattern.
 */
static int
orec_str_list(orec_inf *inf, rec_alt *alt, int str, int count)
{
    rec_stl *stl;
    rec_pat *pat;
    rec_alt *ptr, *next;
    int i, j, tlen, len, is;

    if ((stl = calloc(1, sizeof(rec_stl))) == NULL)
	return (inf->ecode = RE_ESPACE);

    if ((stl->lens = malloc(sizeof(unsigned char) * count)) == NULL) {
	free(stl);
	return (inf->ecode = RE_ESPACE);
    }

    if ((stl->strs = malloc(sizeof(char*) * count)) == NULL) {
	free(stl->lens);
	free(stl);
	return (inf->ecode = RE_ESPACE);
    }

    if ((pat = calloc(1, sizeof(rec_pat))) == NULL) {
	free(stl->strs);
	free(stl->lens);
	free(stl);
	return (inf->ecode = RE_ESPACE);
    }

    pat->data.stl = stl;
    pat->type = Rep_StringList;
    stl->type = str ? Resl_StringList : Resl_CaseStringList;
    for (i = tlen = 0, ptr = alt; i < count; i++) {
	next = ptr->next;
	switch (ptr->pat->type) {
	    case Rep_Literal:
		is = len = 1;
		break;
	    case Rep_CaseLiteral:
		is = len = 2;
		break;
	    default:
		is = 0;
		len = strlen((char*)ptr->pat->data.str);
		break;
	}
	tlen += len;
	stl->lens[i] = len;
	if (!is) {
	    if (len > 2)
		stl->strs[i] = ptr->pat->data.str;
	    else {
		if (len == 1)
		    stl->strs[i] = (void*)(long)(ptr->pat->data.str[0]);
		else
		    stl->strs[i] = (void*)(long)
				   (ptr->pat->data.str[0] |
				    ((int)ptr->pat->data.str[1] << 8));
		free(ptr->pat->data.str);
	    }
	}
	else {
	    if (is == 1)
		stl->strs[i] = (void*)(long)ptr->pat->data.chr;
	    else
		stl->strs[i] = (void*)(long)
			       (ptr->pat->data.cse.lower |
				(ptr->pat->data.cse.upper << 8));
	}
	free(ptr->pat);
	if (i)
	    free(ptr);
	ptr = next;
    }
    stl->tlen = tlen;
    stl->nstrs = count;

    alt->pat = pat;
    alt->next = NULL;

    {
	int li, lj;
	unsigned char ci, cj, *str;

	/*  Don't need a stable sort, there shouldn't be duplicated strings,
	 * but don't check for it either. Only need to make sure that all
	 * strings that start with the same byte are together */
	for (i = 0; i < count; i++) {
	    li = stl->lens[i];
	    ci = li > 2 ? stl->strs[i][0] : (long)stl->strs[i] & 0xff;
	    for (j = i + 1; j < count; j++) {
		lj = stl->lens[j];
		cj = lj > 2 ? stl->strs[j][0] : (long)stl->strs[j] & 0xff;
		if ((count >= LARGE_STL_COUNT && cj < ci) ||
		    (cj == ci && lj > li)) {
		    /* If both strings start with the same byte,
		     * put the longer first */
		    str = stl->strs[j];
		    stl->strs[j] = stl->strs[i];
		    stl->strs[i] = str;
		    stl->lens[j] = li;
		    stl->lens[i] = lj;
		    li ^= lj; lj ^= li; li ^= lj;
		    ci ^= cj; cj ^= ci; ci ^= cj;
		}
	    }
	}
    }

    return (inf->ecode);
}
