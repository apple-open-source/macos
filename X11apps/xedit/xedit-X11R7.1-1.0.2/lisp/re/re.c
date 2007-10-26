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

/* $XFree86: xc/programs/xedit/lisp/re/re.c,v 1.8 2002/11/17 07:51:30 paulo Exp $ */

#include <stdio.h>
#include "rep.h"
#define DEBUG
/*
 * Types
 */

/*  Information used when generating the final form of the compiled re.
 */
struct _re_inf {
    rec_alt *alt;
    unsigned char *cod;
    long len;
    long spc;

    /* Start offset of special repetition instruction */
    long sr[MAX_DEPTH];

    /* Jump offset of special repetition instruction */
    long sj[MAX_DEPTH];

    /* Just a flag, to know if this nesting is for a special repetition */
    char sp[MAX_DEPTH];

    int bas;			/* Alternatives/repetitions depth */
    int par;			/* Open parenthesis counter */
    int ref;			/* Backreference counter */

    rec_pat *apat;		/*  Alternatives duplicate patterns
				 * if a special repetition is found,
				 * this is done to somewhat simplify
				 * the bytecode engine and still allow
				 * some complex (and time consuming)
				 * patterns. */

    int flags;
    int ecode;
};

/*  This structure is not associated with re_cod as it's data only matters
 * to the current match search.
 */
struct _re_eng {
    unsigned char *bas;		/* Base string pointer */
    unsigned char *str;		/* String to search for pattern */
    unsigned char *end;		/* Where to stop searching */
    unsigned char *cod;		/* Pointer in the re_cod structure */
    long off;			/* Number of used entries in so/eo etc */

    /* Match offset/nesting information */
    long so[MAX_DEPTH];		/* (s)tart of (m)atch */
    long eo[MAX_DEPTH];		/* (e)nd of (m)atch */
    long sv[MAX_DEPTH];		/* (s)a(v)e match end offset */
    long re[MAX_DEPTH];		/* (re)petition count */
    long ss[MAX_DEPTH];		/* (s)ave (s)tart of match */
    unsigned char *rcod[MAX_DEPTH];	/* restart position in regex code */
    unsigned char *rstr[MAX_DEPTH];	/* restart position in string */

    /* Group/backreference information */
    long goff;
    long gso[9];
    long geo[9];
};

/*
 * Prototypes
 */
static void reinit(void);
static int rec_check(re_inf*, int);
static int rec_code(re_inf*, ReCode);
static int rec_byte(re_inf*, int);
static int rec_byte_byte(re_inf*, int, int);
static int rec_code_byte(re_inf*, ReCode, int);
static int rec_length(re_inf*, int);
static int rec_code_byte_byte(re_inf*, ReCode, int, int);
static int rec_build_alt(re_inf*, rec_alt*);
static int rec_build_pat(re_inf*, rec_pat*);
static int rec_build_rng(re_inf*, rec_rng*);
static int rec_build_grp(re_inf*, rec_grp*);
static int rec_build_stl(re_inf*, rec_stl*);
static int rec_build_rep(re_inf*, rec_rep*);
static int rec_inc_spc(re_inf*);
static int rec_dec_spc(re_inf*);
static int rec_add_spc(re_inf*, int);
static int rec_off_spc(re_inf*);
static int rec_alt_spc(re_inf*, int);
static int rec_rep_spc(re_inf*, int);
#ifdef DEBUG
static void redump(re_cod*);
#endif

/*
 * Initialization
 */
unsigned char re__alnum[256];
unsigned char re__odigit[256];
unsigned char re__ddigit[256];
unsigned char re__xdigit[256];
unsigned char re__control[256];

/*
 * Implementation
 */
int
recomp(re_cod *preg, const char *pattern, int flags)
{
    int i, ecode;
    re_inf inf;

    reinit();

    preg->cod = NULL;
    inf.alt = irec_comp(pattern,
			flags & RE_PEND ? preg->re_endp :
				pattern + strlen(pattern),
			flags, &ecode);
    if (ecode != 0)
	return (ecode);

    inf.cod = NULL;
    inf.len = inf.spc = 0;
    inf.bas = 0;
    inf.par = 0;
    inf.ref = 0;
    inf.apat = NULL;
    inf.flags = flags;
    inf.ecode = 0;
    for (i = 0; i < MAX_DEPTH; i++)
	inf.sp[i] = 0;

    /* First byte is runtime modifier flags */
    if (rec_byte(&inf, flags & (RE_NEWLINE | RE_NOSUB)) == 0 &&
	rec_byte(&inf, 0xff) == 0 &&
	rec_build_alt(&inf, inf.alt) == 0 &&
	rec_rep_spc(&inf, 0) == 0 &&
	rec_code(&inf, Re_Done) == 0) {
	/*  Number of possible references, loops will not leave this
	 * value correct, but it is cheap to read it from the second
	 * byte, instead of adding several extra checks in the bytecode. */
	if (inf.ref)
	    inf.cod[1] = inf.ref - 1;
	preg->cod = inf.cod;
	/*  Public structure member */
	preg->re_nsub = inf.ref;
    }

    irec_free_alt(inf.alt);
    if (inf.ecode)
	free(inf.cod);
#ifdef DEBUG
    else if (flags & RE_DUMP)
	redump(preg);
#endif

    return (inf.ecode);
}

int
reexec(const re_cod *preg, const char *string,
       int nmatch, re_mat pmat[], int flags)
{
    unsigned char *ptr, *str, newline, nosub;
    int len, si, ci, bas, i, j, k, l, m;
    re_eng eng;

    if (preg == NULL || preg->cod == NULL || nmatch < 0 ||
	((flags & RE_STARTEND) &&
	 (pmat == NULL || pmat[0].rm_eo < pmat[0].rm_so)))
	return (RE_INVARG);

    eng.str = (unsigned char*)string;
    if (flags & RE_STARTEND) {
	eng.end = eng.str + pmat[0].rm_eo;
	eng.str += pmat[0].rm_so;
    }
    else
	eng.end = eng.str + strlen(string);
    eng.bas = eng.str;
    nosub = preg->cod[0] & RE_NOSUB;
    newline = preg->cod[0] & RE_NEWLINE;
    eng.cod = preg->cod + 2;

    if (!nosub && preg->cod[1] != 0xff) {
	for (i = 0; i <= preg->cod[1]; i++) {
	    eng.gso[i] = 0;
	    eng.geo[i] = -1;
	}
    }

    /* Setup to search for start of match from the first character */
    eng.so[0] = 0;
    eng.eo[0] = eng.sv[0] = -1;
    eng.rcod[0] = eng.cod;
    eng.rstr[0] = eng.str + 1;
    eng.off = 0;
    eng.goff = -1;
    for (ci = si = 1;;) {
reset:
	switch (*eng.cod) {
	    /****************************************************
	     * One byte codes					*
	     ****************************************************/
	    case Re_Any:
		if (eng.str == eng.end || (newline && eng.str[0] == '\n'))
		    goto fail;
		goto match;
	    case Re_AnyEatAnyTimes:
		if (newline) {
		    for (ptr = eng.str; ptr < eng.end; ptr++) {
			if (*ptr == '\n')
			    break;
		    }
		    si = ptr - eng.str;
		}
		else
		    si = eng.end - eng.str;
		goto match;
	    case Re_AnyEatMaybe:
		si = eng.end > eng.str;
		if (newline && si && eng.str[0] == '\n')
		    si = 0;
		goto match;
	    case Re_AnyEatAtLeast:
		if (newline) {
		    for (ptr = eng.str; ptr < eng.end; ptr++) {
			if (*ptr == '\n')
			    break;
		    }
		    si = ptr - eng.str;
		}
		else
		    si = eng.end - eng.str;
		if (si == 0) {
		    si = 1;
		    goto fail;
		}
		goto match;
	    case Re_Odigit:
		if (eng.str >= eng.end)
		    goto fail;
		if (re__odigit[eng.str[0]])
		    goto match;
		goto fail;
	    case Re_OdigitNot:
		if (eng.str >= eng.end || re__odigit[eng.str[0]])
		    goto fail;
		goto match;
	    case Re_Digit:
		if (eng.str >= eng.end)
		    goto fail;
		if (re__ddigit[eng.str[0]])
		    goto match;
		goto fail;
	    case Re_DigitNot:
		if (eng.str >= eng.end || re__ddigit[eng.str[0]])
		    goto fail;
		goto match;
	    case Re_Xdigit:
		if (eng.str >= eng.end)
		    goto fail;
		if (re__xdigit[eng.str[0]])
		    goto match;
		goto fail;
	    case Re_XdigitNot:
		if (eng.str >= eng.end || re__xdigit[eng.str[0]])
		    goto fail;
		goto match;
	    case Re_Space:
		if (eng.str >= eng.end)
		    goto fail;
		if (eng.str[0] == ' ' || eng.str[0] == '\t')
		    goto match;
		goto fail;
	    case Re_SpaceNot:
		if (eng.str >= eng.end)
		    goto fail;
		if (eng.str[0] != ' ' && eng.str[0] != '\t')
		    goto match;
		goto fail;
	    case Re_Tab:
		if (eng.str >= eng.end)
		    goto fail;
		if (eng.str[0] == '\t')
		    goto match;
		goto fail;
	    case Re_Newline:
		if (eng.str >= eng.end)
		    goto fail;
		if (eng.str[0] == '\n')
		    goto match;
		goto fail;
	    case Re_Lower:
		if (eng.str >= eng.end)
		    goto fail;
		if (eng.str[0] >= 'a' && eng.str[0] <= 'z')
		    goto match;
		goto fail;
	    case Re_Upper:
		if (eng.str >= eng.end)
		    goto fail;
		if (eng.str[0] >= 'A' && eng.str[0] <= 'Z')
		    goto match;
		goto fail;
	    case Re_Alnum:
		if (eng.str >= eng.end)
		    goto fail;
		if (re__alnum[eng.str[0]])
		    goto match;
		goto fail;
	    case Re_AlnumNot:
		if (eng.str >= eng.end)
		    goto fail;
		if (re__alnum[eng.str[0]])
		    goto fail;
		goto match;
	    case Re_Control:
		if (eng.str >= eng.end)
		    goto fail;
		if (re__control[eng.str[0]])
		    goto match;
		goto fail;
	    case Re_ControlNot:
		if (eng.str >= eng.end || re__control[eng.str[0]])
		    goto fail;
		goto match;

	    /****************************************************
	     * One byte codes, match special emtpy strings	*
	     ****************************************************/
	    case Re_Bol:
		if (eng.str == eng.bas) {
		    if ((flags & RE_NOTBOL)) {
			/* String does not start at the beginning of a line */
			if (newline)
			    goto fail;
			goto wont;
		    }
		    si = 0;
		    goto match;
		}
		if (newline && eng.str[-1] == '\n') {
		    si = 0;
		    goto match;
		}
		goto fail;
	    case Re_Eol:
		if (eng.str == eng.end) {
		    if (flags & RE_NOTEOL)
			/* String does not finish at the end of a line */
			goto wont;
		    si = 0;
		    goto match;
		}
		if (newline && eng.str[0] == '\n') {
		    si = 0;
		    goto match;
		}
		goto fail;
	    case Re_Bow:
		if (eng.str >= eng.end ||
		    (eng.str > eng.bas &&
		     (re__alnum[eng.str[-1]])))
		    goto fail;
		if (re__alnum[eng.str[0]]) {
		    si = 0;
		    goto match;
		}
		goto fail;
	    case Re_Eow:
		if (eng.str == eng.bas ||
		    (eng.str < eng.end &&
		     re__alnum[eng.str[0]]))
		    goto fail;
		if (re__alnum[eng.str[-1]]) {
		    si = 0;
		    goto match;
		}
		goto fail;

	    /****************************************************
	     * One byte code, one byte argument			*
	     ****************************************************/
	    case Re_Literal:
		if (eng.str >= eng.end)
		    goto fail;
		if (eng.str[0] == eng.cod[1]) {
		    ci = 2;
		    goto match;
		}
		goto fail;
	    case Re_LiteralNot:
		if (eng.str >= eng.end)
		    goto fail;
		if (eng.str[0] != eng.cod[1]) {
		    ci = 2;
		    goto match;
		}
		goto fail;
	    case Re_SearchLiteral:
		for (str = eng.str; str < eng.end; str++) {
		    if (*str == eng.cod[1]) {
			ci = 2;
			eng.str = str;
			goto match;
		    }
		}
		/* This bytecode only happens in the toplevel */
		eng.so[0] = str - eng.bas;
		eng.str = str;
		goto fail;

	    /****************************************************
	     * One byte code, two bytes argument		*
	     ****************************************************/
	    case Re_CaseLiteral:
		if (eng.str >= eng.end)
		    goto fail;
		if (eng.str[0] == eng.cod[1] || eng.str[0] == eng.cod[2]) {
		    ci = 3;
		    goto match;
		}
		goto fail;
	    case Re_CaseLiteralNot:
		if (eng.str >= eng.end)
		    goto fail;
		if (eng.str[0] != eng.cod[1] && eng.str[0] != eng.cod[2]) {
		    ci = 3;
		    goto match;
		}
		goto fail;
	    case Re_SearchCaseLiteral:
		for (str = eng.str; str < eng.end; str++) {
		    if (*str == eng.cod[1] || *str == eng.cod[2]) {
			ci = 3;
			eng.str = str;
			goto match;
		    }
		}
		eng.so[0] = str - eng.bas;
		eng.str = str;
		goto fail;

	    /****************************************************
	     * One byte codes, two arguments, n bytes		*
	     ****************************************************/
	    case Re_String:
		len = eng.cod[1];
		if (len & 0x80) {
		    i = 3;
		    len = (len & 0x7f) + (eng.cod[2] << 7);
		}
		else
		    i = 2;
		if (eng.end - eng.str < len)
		    goto fail;
		ptr = eng.cod + i;
		str = eng.str;
		for (k = len; k > 0; k--) {
		    if (*ptr++ != *str++)
			goto fail;
		}
		ci = i + len;
		si = len;
		goto match;
	    case Re_SearchString:
		len = eng.cod[1];
		if (len & 0x80) {
		    i = 3;
		    len = (len & 0x7f) + (eng.cod[2] << 7);
		}
		else
		    i = 2;
		for (str = eng.str; eng.end - str >= len; str = eng.str++) {
		    for (ptr = eng.cod + i, str = eng.str, k = len; k > 0; k--)
			if (*ptr++ != *str++)
			    break;
		    if (k == 0) {
			/* Substring found */
			ci = i + len;
			si = str - eng.str;
			goto match;
		    }
		}
		eng.so[0] = eng.end - eng.bas;
		eng.str = eng.end;
		goto fail;

	    case Re_CaseString:
		len = eng.cod[1];
		if (len & 0x80) {
		    i = 3;
		    len = (len & 0x7f) + (eng.cod[2] << 7);
		}
		else
		    i = 2;

		len >>= 1;
		/*  Check if there are at least len/2 bytes left, string
		 * is represented as two bytes, lower and upper case */
		if (eng.end - eng.str < len)
		    goto fail;
		ptr = eng.cod + i;
		str = eng.str;
		for (k = len; k > 0; str++, ptr += 2, k--) {
		    if (*str != ptr[0] && *str != ptr[1])
			goto fail;
		}
		ci = i + (len << 1);
		si = len;
		goto match;
	    case Re_SearchCaseString:
		len = eng.cod[1];
		if (len & 0x80) {
		    i = 3;
		    len = (len & 0x7f) + (eng.cod[2] << 7);
		}
		else
		    i = 2;
		len >>= 1;
		for (str = eng.str; eng.end - str >= len; str = eng.str++) {
		    for (ptr = eng.cod + i, str = eng.str, k = len;
			 k > 0; k--, ptr += 2, str++)
			if (ptr[0] != str[0] && ptr[1] != str[0])
			    break;
		    if (k == 0) {
			/* Substring found */
			ci = i + (len << 1);
			si = str - eng.str;
			goto match;
		    }
		}
		eng.so[0] = eng.end - eng.bas;
		eng.str = eng.end;
		goto fail;

	    case Re_StringList:
		/* Number of strings */
		k = eng.cod[1];

		/* Where to jump after match */
		bas = eng.cod[2] | (eng.cod[3] << 8);

		str = eng.str;
		ptr = eng.cod + k + 4;
		l = eng.end - eng.str;
		for (j = 0; j < k; j++) {
		    len = eng.cod[j + 4];
		    if (len <= l) {
			for (i = 0; i < len; i++)
			    if (ptr[i] != str[i])
				goto next_stl;
			goto stl_match;
		    }
next_stl:
		    ptr += len;
		}
		goto fail;
stl_match:
		ci = bas;
		si = len;
		goto match;

	    case Re_CaseStringList:
		/* Number of strings */
		k = eng.cod[1];

		/* Where to jump after match */
		bas = eng.cod[2] | (eng.cod[3] << 8);

		str = eng.str;
		ptr = eng.cod + k + 4;
		l = eng.end - eng.str;
		for (j = 0; j < k; j++) {
		    len = eng.cod[j + 4];
		    if ((len >> 1) <= l) {
			for (i = m = 0; i < len; m++, i += 2)
			    if (ptr[i] != str[m] && ptr[i + 1] != str[m])
				goto next_cstl;
			goto cstl_match;
		    }
next_cstl:
		    ptr += len;
		}
		goto fail;
cstl_match:
		ci = bas;
		si = len >> 1;
		goto match;


	    case Re_LargeStringList:
		/* Where to jump after match */
		bas = eng.cod[1] | (eng.cod[2] << 8);

		str = eng.str;

		/* First entry in index map */
		ptr = eng.cod + 3;
		i = (int)str[0] << 1;
		j = ptr[i] | (ptr[i + 1] << 8);
		if (j == 0xffff)
		    /* No entry with this byte */
		    goto fail;

		/* Bytes left in input */
		l = eng.end - eng.str;

		/* First entry matching initial byte */
		ptr += 512 + j;

		for (len = ptr[0];
		     str[0] == ptr[1];
		     ptr += len + 1, len = ptr[0]) {
		    if (len <= l) {
			for (i = 1; i < len; i++) {
			    if (ptr[i + 1] != str[i])
				goto next_lstl;
			}
			ci = bas;
			si = len;
			goto match;
		    }
next_lstl:;
		}
		goto fail;

	    case Re_LargeCaseStringList:
		/* Where to jump after match */
		bas = eng.cod[1] | (eng.cod[2] << 8);

		str = eng.str;

		/* First entry in index map */
		ptr = eng.cod + 3;
		i = (int)str[0] << 1;
		j = ptr[i] | (ptr[i + 1] << 8);
		if (j == 0xffff)
		    /* No entry with this byte */
		    goto fail;

		/* Bytes left in input */
		l = eng.end - eng.str;

		/* First entry matching initial byte */
		ptr += 512 + j;

		for (len = ptr[0];
		     str[0] == ptr[1] || str[0] == ptr[2];
		     ptr += len + 1, len = ptr[0]) {
		    if ((k = (len >> 1)) <= l) {
			for (i = 2, j = 1; i < len; i += 2, j++) {
			    if (ptr[i + 1] != str[j] && ptr[i + 2] != str[j])
				goto next_lcstl;
			}
			ci = bas;
			si = k;
			goto match;
		    }
next_lcstl:;
		}
		goto fail;


	    /****************************************************
	     * Character range matching				*
	     ****************************************************/
	    case Re_Range:
		if (eng.str < eng.end && eng.cod[eng.str[0] + 1]) {
		    ci = 257;
		    goto match;
		}
		goto fail;
	    case Re_RangeNot:
		if (eng.str >= eng.end || eng.cod[eng.str[0] + 1])
		    goto fail;
		ci = 257;
		goto match;

	    /****************************************************
	     * Group handling					*
	     ****************************************************/
	    case Re_Open:
		if (++eng.goff >= 9)
		    return (RE_ASSERT);
		eng.gso[eng.goff] = eng.str - eng.bas;
		++eng.cod;
		continue;
	    case Re_Close:
		eng.geo[eng.goff] = eng.str - eng.bas;
		++eng.cod;
		continue;
	    case Re_Update:
		bas = eng.cod[1];
		eng.geo[eng.goff] = eng.str - eng.bas;
		eng.cod += 2;		/* + Update + bas */
		continue;

	    /****************************************************
	     * Backreference					*
	     ****************************************************/
	    case Re_Backref:
		i = eng.cod[1];
		j = eng.gso[i];
		k = eng.geo[i];
		len = k - j;
		if (k < j || eng.end - eng.str < len)
		    goto fail;
		ptr = eng.bas + j;
		str = eng.str;
		for (l = len; l > 0; l--) {
		    if (*ptr++ != *str++)
			goto fail;
		}
		ci = 2;
		si = len;
		goto match;

	    /****************************************************
	     * Alternatives handling				*
	     ****************************************************/
	    case Re_Alt:
		bas = eng.off;
		if (++eng.off >= MAX_DEPTH)
		    return (RE_ASSERT);

		/* Get offset of next alternative */
		i = eng.cod[1] | (eng.cod[2] << 8);

		/* Setup for next alternative if the current fails */
		eng.rcod[eng.off] = eng.cod + i + 1;	/* + Alt */

		/* If fail, test the next alternative in the same string */
		eng.rstr[eng.off] = eng.str;

		/* Setup match offsets */
		if (eng.so[bas] <= eng.eo[bas])
		    eng.so[eng.off] = eng.eo[bas];
		else
		    eng.so[eng.off] = eng.so[bas];
		eng.sv[eng.off] = eng.eo[eng.off] = eng.so[eng.off] - 1;

		/* Save start of possible previous matches */
		eng.ss[eng.off] = eng.so[bas];

		/* Skip code */
		eng.cod += 3;

		/* Go try the first alternative */
		continue;

	    case Re_AltNext:
		bas = eng.off - 1;
		/* Check if matched and if it is a better match */
		if (eng.sv[eng.off] - eng.so[eng.off] <
		    eng.eo[eng.off] - eng.so[eng.off])
		    eng.sv[eng.off] = eng.eo[eng.off];

		/* Get offset of next alternative */
		i = eng.cod[1] | (eng.cod[2] << 8);

		/* Setup for next alternative if the current fails */
		eng.rcod[eng.off] = eng.cod + i + 1;	/* + AltNext */

		/* Setup match offset */
		eng.eo[eng.off] = eng.so[eng.off] - 1;

		/* Reset string for next alternative */
		eng.str = eng.rstr[eng.off];

		/* Skip code */
		eng.cod += 3;

		/* Go try the next alternative */
		continue;

	    case Re_AltDone:
		bas = eng.off - 1;
		/* Check if matched and if it is a better match */
		if (eng.sv[eng.off] - eng.so[eng.off] <
		    eng.eo[eng.off] - eng.so[eng.off])
		    eng.sv[eng.off] = eng.eo[eng.off];

		/* If there is a match */
		if (eng.sv[eng.off] >= eng.so[eng.off]) {
		    eng.so[bas] = eng.ss[eng.off];
		    eng.eo[bas] = eng.sv[eng.off];
		    eng.str = eng.bas + eng.eo[bas];

		    /* Pop stack and skip code */
		    --eng.off;
		    ++eng.cod;

		    /* Go try next regular expression pattern */
		    continue;
		}

		/* Failed, reset string and pop stack */
		eng.str = eng.rstr[eng.off];
		--eng.off;
		goto fail;


	    /****************************************************
	     * Repetition					*
	     ****************************************************/

		/*  Note that the repetition counter is not
		 * updated for <re>*, <re>+ and <re>?
		 * it is easy to updated, but since it is not
		 * really useful, code to do it was removed
		 * to save a few cpu cicles. */

#define REPETITION_SETUP()					\
	if (++eng.off >= MAX_DEPTH)				\
	    return (RE_ASSERT);					\
								\
	/* Return here for recovery if match fail */		\
	eng.rcod[eng.off] = eng.cod;				\
								\
	/* Setup match offsets */				\
	if (eng.so[bas] <= eng.eo[bas])				\
	    eng.so[eng.off] = eng.eo[bas];			\
	else							\
	    eng.so[eng.off] = eng.so[bas];			\
	eng.ss[eng.off] = eng.so[bas];				\
	eng.sv[eng.off] = eng.eo[eng.off] = eng.so[eng.off] - 1;\
								\
	/* Skip repetition instruction */			\
	eng.cod += 4;


	    case Re_AnyTimes:
		bas = eng.cod[1];
		if (eng.off == bas) {
		    /* First iteration */
		    REPETITION_SETUP();
		}
		else {
		    if (eng.eo[eng.off] >= eng.so[eng.off] &&
			eng.eo[eng.off] > eng.sv[eng.off]) {
			/* Update offset of match */
			eng.sv[eng.off] = eng.eo[eng.off];

			/* Skip repetition instruction */
			eng.cod += 4;
		    }
		    else {
			/* Match failed but it is ok */
			len = eng.cod[2] | (eng.cod[3] << 8);
			eng.so[bas] = eng.ss[eng.off];
			if (eng.sv[eng.off] >= eng.so[eng.off])
			    /* Something matched earlier, update */
			    eng.eo[bas] = eng.sv[eng.off];
			else if (eng.eo[bas] < eng.so[bas])
			    /* Empty match */
			    eng.eo[bas] = eng.so[bas];

			/* Try next pattern at correct offset */
			eng.str = eng.bas + eng.eo[bas];

			/* Pop stack and skip code */
			--eng.off;
			eng.cod += len;
		    }
		}
		continue;

	    case Re_Maybe:
		bas = eng.cod[1];
		if (eng.off == bas) {
		    /* First iteration */
		    REPETITION_SETUP();
		}
		else {
		    /* Matched or first iteration is done */
		    len = eng.cod[2] | (eng.cod[3] << 8);
		    eng.so[bas] = eng.ss[eng.off];
		    if (eng.eo[eng.off] > eng.so[eng.off]) {
			/* Something matched earlier, update */
			eng.eo[bas] = eng.eo[eng.off];
			eng.str = eng.bas + eng.eo[bas];
			/* Don't need to update counter */
		    }
		    else {
			/* Empty match */
			if (eng.eo[bas] < eng.so[bas])
			    eng.eo[bas] = eng.so[bas];

			/* Try next pattern at correct offset */
			eng.str = eng.bas + eng.eo[bas];
		    }

		    /* Pop stack */
		    --eng.off;

		    /* Skip code */
		    eng.cod += len;
		}
		continue;

	    case Re_AtLeast:
		bas = eng.cod[1];
		if (eng.off == bas) {
		    /* First iteration */
		    REPETITION_SETUP();
		}
		else {
		    if (eng.eo[eng.off] >= eng.so[eng.off] &&
			eng.eo[eng.off] > eng.sv[eng.off]) {
			/* Update offset of match */
			eng.sv[eng.off] = eng.eo[eng.off];

			/* Skip repetition instruction */
			eng.cod += 4;
		    }
		    else {
			/* Last try failed */
			len = eng.cod[2] | (eng.cod[3] << 8);
			if (eng.sv[eng.off] >= eng.so[eng.off]) {
			    /* Something matched earlier, update */
			    eng.so[bas] = eng.ss[eng.off];
			    eng.eo[bas] = eng.sv[eng.off];
			    eng.str = eng.bas + eng.eo[bas];
			}
			else {
			    /*	Do it here, so that the fail label does
			     * not need to do too expensive work for
			     * simple patterns. */
			    eng.so[bas] = eng.str - eng.bas;

			    /* Zero matches, pop stack and restart */
			    --eng.off;
			    goto fail;
			}

			/* Pop stack and skip code */
			--eng.off;
			eng.cod += len;
		    }
		}
		continue;


	    /****************************************************
	     * Repetition with arguments			*
	     ****************************************************/
	    case Re_Exact:
#define COMPLEX_REPETITION_SETUP_0()				\
	i = eng.cod[1];						\
	bas = eng.cod[2];

#define COMPLEX_REPETITION_SETUP()				\
	/* First iteration */					\
	if (++eng.off >= MAX_DEPTH)				\
	    return (RE_ASSERT);					\
								\
	/*  Remeber number or repetitions */			\
	eng.re[eng.off] = 0;					\
								\
	/* Return here for recovery if match fail */		\
	eng.rcod[eng.off] = eng.cod;				\
								\
	/* Setup match offsets */				\
	if (eng.so[bas] <= eng.eo[bas])				\
	    eng.so[eng.off] = eng.eo[bas];			\
	else							\
	    eng.so[eng.off] = eng.so[bas];			\
	eng.sv[eng.off] = eng.eo[eng.off] = eng.so[eng.off] - 1;\
	eng.ss[eng.off] = eng.so[bas];				\
								\
	/* Skip repetition instruction */			\
	eng.cod += 5;

		COMPLEX_REPETITION_SETUP_0();
		if (eng.off == bas) {
		    /* First iteration */
		    COMPLEX_REPETITION_SETUP();
		}
		else {
		    if (eng.eo[eng.off] >= eng.so[eng.off] &&
			eng.eo[eng.off] > eng.sv[eng.off]) {
			/* Update offset of match */
			eng.sv[eng.off] = eng.eo[eng.off];

			/* Update repetition counter */
			if (++eng.re[eng.off] == i) {
			    /* Matched the required times */
			    eng.so[bas] = eng.ss[eng.off];
			    eng.eo[bas] = eng.sv[eng.off];
			    eng.str = eng.bas + eng.eo[bas];

			    /* Update code */
			    k = eng.cod[3] | (eng.cod[4] << 8);
			    eng.cod += k;

			    /* Pop stack and go for next pattern */
			    --eng.off;
			    continue;
			}

			/* Skip repetition instruction */
			eng.cod += 5;
		    }
		    else {
			/*  Do it here, so that the fail label does
			 * not need to do too expensive work for
			 * simple patterns. */
			eng.so[bas] = eng.str - eng.bas;

			/* Pop stack and restart */
			--eng.off;
			goto fail;
		    }
		}
		continue;

	    case Re_Min:
		COMPLEX_REPETITION_SETUP_0();
		if (eng.off == bas) {
		    /* First iteration */
		    COMPLEX_REPETITION_SETUP();
		}
		else {
		    if (eng.eo[eng.off] >= eng.so[eng.off] &&
			eng.eo[eng.off] > eng.sv[eng.off]) {
			/* Update offset of match */
			eng.sv[eng.off] = eng.eo[eng.off];

			/* Update repetition counter */
			++eng.re[eng.off];

			/* Skip repetition instruction and try again */
			eng.cod += 5;
		    }
		    else {
			/* Match failed! */
			if (eng.re[eng.off] < i) {
			    /*	Do it here, so that the fail label does
			     * not need to do too expensive work for
			     * simple patterns. */
			    eng.so[bas] = eng.str - eng.bas;

			    /* Didn't match required number of times */
			    --eng.off;
			    goto fail;
			}
			else {
			    /* Matched minimum number of times */
			    eng.eo[bas] = eng.sv[eng.off];
			    eng.str = eng.bas + eng.eo[bas];
			    k = eng.cod[3] | (eng.cod[4] << 8);

			    /* Update code and pop stack */
			    eng.cod += k;
			    --eng.off;
			}
		    }
		}
		continue;

	    case Re_Max:
		COMPLEX_REPETITION_SETUP_0();
		if (eng.off == bas) {
		    /* First iteration */
		    COMPLEX_REPETITION_SETUP();
		}
		else {
		    if (eng.eo[eng.off] >= eng.so[eng.off] &&
			eng.eo[eng.off] > eng.sv[eng.off]) {
			/* Update offset of match */
			eng.sv[eng.off] = eng.eo[eng.off];

			/* Update repetition counter */
			if (++eng.re[eng.off] == i) {
			    /* Matched the maximum times */
			    eng.so[bas] = eng.ss[eng.off];
			    eng.eo[bas] = eng.sv[eng.off];
			    eng.str = eng.bas + eng.eo[bas];

			    k = eng.cod[3] | (eng.cod[4] << 8);

			    /* Update code and pop stack */
			    eng.cod += k;
			    --eng.off;
			    continue;
			}

			/* Skip repetition instruction and try again */
			eng.cod += 5;
		    }
		    else {
			/* No matches, but zero matches are ok */
			k = eng.cod[3] | (eng.cod[4] << 8);
			if (eng.sv[eng.off] >= eng.so[eng.off]) {
			    /* Something matched earlier, update */
			    eng.so[bas] = eng.ss[eng.off];
			    eng.eo[bas] = eng.sv[eng.off];
			    eng.str = eng.bas + eng.eo[bas];
			}
			else {
			    /* Empty match */
			    if (eng.eo[bas] < eng.so[bas])
				eng.eo[bas] = eng.so[bas];

			    /* Try next pattern at correct offset */
			    eng.str = eng.bas + eng.eo[bas];
			}

			/* Pop stack and update code */
			--eng.off;
			eng.cod += k;
		    }
		}
		continue;

	    case Re_MinMax:
		bas = eng.cod[3];
		if (eng.off == bas) {
		    /* First iteration */
		    COMPLEX_REPETITION_SETUP();
		}
		else {
		    if (eng.eo[eng.off] >= eng.so[eng.off] &&
			eng.eo[eng.off] > eng.sv[eng.off]) {
			/* Update offset of match */
			eng.sv[eng.off] = eng.eo[eng.off];

			/* Update repetition counter */
			if (++eng.re[eng.off] == eng.cod[2]) {
			    /* Matched the maximum times */
			    eng.so[bas] = eng.ss[eng.off];
			    eng.eo[bas] = eng.sv[eng.off];
			    eng.str = eng.bas + eng.eo[bas];
			    k = eng.cod[4] | (eng.cod[5] << 8);

			    /* Update code and pop stack */
			    eng.cod += k;
			    --eng.off;
			    continue;
			}

			/* Skip repetition instruction and try again */
			eng.cod += 6;
		    }
		    else {
			/* Match failed! */
			if (eng.re[eng.off] < eng.cod[1]) {
			    /*	Do it here, so that the fail label does
			     * not need to do too expensive work for
			     * simple patterns. */
			    eng.so[bas] = eng.str - eng.bas;

			    /* Didn't match required number of times */
			    --eng.off;
			    goto fail;
			}
			else {
			    /* Matched minimum number of times */
			    eng.so[bas] = eng.ss[eng.off];
			    eng.eo[bas] = eng.sv[eng.off];
			    eng.str = eng.bas + eng.eo[bas];
			    k = eng.cod[4] | (eng.cod[5] << 8);

			    /* Update code and pop stack */
			    eng.cod += k;
			    --eng.off;
			}
		    }
		}
		continue;


	    /****************************************************
	     * Special repetition handling			*
	     ****************************************************/
	    case Re_AnyAnyTimes:
		/* code(1) + bas(1) + gbas(1) + jump(2) */
		bas = eng.cod[1];
		if (eng.off == bas) {
		    /* First iteration */
		    if (++eng.off >= MAX_DEPTH)
			return (RE_ASSERT);

		    /* Return here for recovery if match fail */
		    eng.rcod[eng.off] = eng.cod;

		    /* If fail, test the next pattern at the same point */
		    eng.rstr[eng.off] = eng.str;

		    /* Setup match offsets */
		    eng.so[eng.off] = eng.str - eng.bas;
		    eng.eo[eng.off] = eng.so[eng.off] - 1;

		    if (newline)
			/*  Use the repetition counter to store start of
			 * skipped string, to later check if skipping a
			 * newline. */
			eng.re[eng.off] = eng.so[eng.off];

		    /* Save start of possible previous matches */
		    eng.ss[eng.off] = eng.so[bas];

		    /* Skip repetition instruction */
		    eng.cod += 5;
		}
		else {
		    /* -1 as an unsigned char */
		    if (eng.cod[2] != 0xff)
			eng.goff = eng.cod[2];
		    else
			eng.goff = -1;

		    if (newline) {
			ptr = eng.bas + eng.re[eng.off];
			str = eng.bas + eng.so[eng.off];
			for (; ptr < str; ptr++)
			    if (*ptr == '\n') {
				eng.cod = eng.rcod[0];
				eng.so[0] = ptr - eng.bas + 1;
				eng.eo[0] = eng.so[0] - 1;
				eng.rstr[0] = eng.str = ptr + 1;
				eng.off = 0;
				goto reset;
			    }
			/* If looping, don't do too many noops */
			eng.re[eng.off] = ptr - eng.bas;
		    }

		    if (eng.eo[eng.off] >= eng.so[eng.off]) {
			/* Note that this is only true if all possibly
			 * nested special repetitions also matched. */

			if (eng.goff >= 0) {
			    if (eng.cod[5] == Re_Update)
				eng.gso[eng.goff] = eng.eo[bas] +
						    (eng.so[bas] > eng.eo[bas]);
			    else if (eng.geo[eng.goff] < eng.so[eng.off])
				eng.geo[eng.goff] = eng.so[eng.off];
			}

			/* Jump relative offset */
			len = eng.cod[3] | (eng.cod[4] << 8);

			/* Restore offset from where started trying */
			eng.so[bas] = eng.ss[eng.off];
			eng.eo[bas] = eng.eo[eng.off];

			/* Pop stack and skip code */
			--eng.off;
			eng.cod += len;
		    }
		    else {
			/* Only give up if the entire string was scanned */
			if (eng.str < eng.end) {
			    /* Update restart point for next pattern */
			    eng.str = ++eng.rstr[eng.off];

			    /* Reset start of nested match */
			    eng.so[eng.off] = eng.str - eng.bas;

			    /* Skip repetition instruction */
			    eng.cod += 5;
			}
			else {
			    /* Entire string scanned and failed */

			    /* Jump relative offset */
			    len = eng.cod[3] | (eng.cod[4] << 8);

			    /* Restore offset from where started trying */
			    eng.so[bas] = eng.ss[eng.off];
			    eng.eo[bas] = eng.ss[eng.off] - 1;

			    /* Pop stack and skip code */
			    --eng.off;
			    eng.cod += len;
			}
		    }
		}
		continue;

	    /*  This is significantly different than matching <re>.*<re>
	     * because it may need to restart several times since it is
	     * possible to find too many false positives, for example:
	     *	a.*b	=> once one "a" is found, scan all
	     *		   the remaining string searching for a "b"
	     *  a.?b	=> the string may have too many "a"s, but the
	     *		   first occurrences of "a" may not be followed
	     *		   by any-character and a "b" or a single "b".
	     */
	    case Re_AnyMaybe:
		bas = eng.cod[1];
		if (eng.off == bas) {
		    /* First iteration */
		    if (++eng.off >= MAX_DEPTH)
			return (RE_ASSERT);

		    /* Return here for recovery if match fail */
		    eng.rcod[eng.off] = eng.cod;

		    /* First try without eating a byte */
		    eng.rstr[eng.off] = eng.str;

		    /* Remember this is the first try if match fail */
		    eng.re[eng.off] = 0;

		    /* Setup match offsets */
		    eng.so[eng.off] = eng.str - eng.bas;
		    eng.eo[eng.off] = eng.so[eng.off] - 1;

		    /* Save start of possible previous matches */
		    eng.ss[eng.off] = eng.so[bas];

		    /* Skip repetition instruction */
		    eng.cod += 5;
		}
		else {
		    /* -1 as an unsigned char */
		    if (eng.cod[2] != 0xff)
			eng.goff = eng.cod[2];
		    else
			eng.goff = -1;

		    if (eng.eo[eng.off] >= eng.so[eng.off]) {
			/* Something matched */

			if (eng.goff >= 0) {
			    if (eng.cod[5] == Re_Update)
				eng.gso[eng.goff] = eng.eo[bas] +
						    (eng.so[bas] > eng.eo[bas]);
			    else if (eng.geo[eng.goff] < eng.so[eng.off])
				eng.geo[eng.goff] = eng.so[eng.off];
			}

			/* Jump relative offset */
			len = eng.cod[3] | (eng.cod[4] << 8);

			/* Update offset of match */
			eng.eo[bas] = eng.eo[eng.off];

			/* Pop stack and skip code */
			--eng.off;
			eng.cod += len;
		    }
		    else if (eng.re[eng.off] == 0 &&
			     (!newline || eng.rstr[eng.off][1] != '\n')) {
			/* Try this time skiping a byte */
			++eng.re[eng.off];

			/* Reset string, skip code and go try one time more */
			eng.str = ++eng.rstr[eng.off];
			eng.cod += 5;
		    }
		    else {
			/* Failed to match */

			/* Update offsets */
			eng.eo[bas] = eng.ss[eng.off];
			eng.so[bas] = eng.eo[bas] + 1;

			eng.str = eng.rstr[eng.off] + (eng.re[eng.off] == 0);

			/* Pop stack and return to toplevel code */
			--eng.off;
			if (eng.str >= eng.end)
			    goto wont;
			eng.cod = eng.rcod[bas];
		    }
		}
		continue;

	    /* .+ almost identical to .* but requires eating at least one byte */
	    case Re_AnyAtLeast:
		bas = eng.cod[1];
		if (eng.off == bas) {
		    /* First iteration */
		    if (++eng.off >= MAX_DEPTH)
			return (RE_ASSERT);

		    /* Return here for recovery if match fail */
		    eng.rcod[eng.off] = eng.cod;

		    /* Skip one byte for the restart string */
		    if (newline && eng.str[0] == '\n') {
			/* Cannot skip newline */
			eng.cod = eng.rcod[0];
			eng.rstr[0] = ++eng.str;
			eng.so[0] = eng.str - eng.bas;
			eng.eo[0] = eng.so[0] - 1;
			eng.off = 0;
			goto reset;
		    }
		    eng.rstr[eng.off] = ++eng.str;

		    /* Setup match offsets */
		    eng.so[eng.off] = eng.str - eng.bas;
		    eng.eo[eng.off] = eng.so[eng.off] - 1;

		    if (newline)
			/*  Use the repetition counter to store start of
			 * skipped string, to later check if skipping a
			 * newline. */
			eng.re[eng.off] = eng.so[eng.off];

		    /* Save start of possible previous matches */
		    eng.ss[eng.off] = eng.so[bas];

		    /* Skip repetition instruction */
		    eng.cod += 5;
		}
		else {
		    /* -1 as an unsigned char */
		    if (eng.cod[2] != 0xff)
			eng.goff = eng.cod[2];
		    else
			eng.goff = -1;

		    if (newline) {
			ptr = eng.bas + eng.re[eng.off];
			str = eng.bas + eng.so[eng.off];
			for (; ptr < str; ptr++)
			    if (*ptr == '\n') {
				eng.cod = eng.rcod[0];
				eng.so[0] = ptr - eng.bas + 1;
				eng.eo[0] = eng.so[0] - 1;
				eng.rstr[0] = eng.str = ptr + 1;
				eng.off = 0;
				goto reset;
			    }
			/* If looping, don't do too many noops */
			eng.re[eng.off] = ptr - eng.bas;
		    }

		    if (eng.eo[eng.off] >= eng.so[eng.off]) {
			/* Note that this is only true if all possibly
			 * nested special repetitions also matched. */

			if (eng.goff >= 0) {
			    if (eng.cod[5] == Re_Update)
				eng.gso[eng.goff] = eng.eo[bas] +
						    (eng.so[bas] > eng.eo[bas]);
			    else if (eng.geo[eng.goff] < eng.so[eng.off])
				eng.geo[eng.goff] = eng.so[eng.off];
			}

			/* Jump relative offset */
			len = eng.cod[3] | (eng.cod[4] << 8);

			/* Restore offset from where started trying */
			eng.so[bas] = eng.ss[eng.off];
			eng.eo[bas] = eng.eo[eng.off];

			/* Pop stack and skip code */
			--eng.off;
			eng.cod += len;
		    }
		    else {
			/* Only give up if the entire string was scanned */
			if (eng.str < eng.end) {
			    /* Update restart point for next pattern */
			    eng.str = ++eng.rstr[eng.off];

			    /* Reset start of nested match */
			    eng.so[eng.off] = eng.str - eng.bas;

			    /* Skip repetition instruction */
			    eng.cod += 5;
			}
			else {
			    /* Entire string scanned and failed */

			    /* Jump relative offset */
			    len = eng.cod[3] | (eng.cod[4] << 8);

			    /* Restore offset from where started trying */
			    eng.so[bas] = eng.ss[eng.off];
			    eng.eo[bas] = eng.ss[eng.off] - 1;

			    /* Pop stack and skip code */
			    --eng.off;
			    eng.cod += len;
			}
		    }
		}
		continue;


	    /****************************************************
	     * Repetition matched!				*
	     ****************************************************/
	    case Re_RepJump:
		/* eng.cod[1] is toplevel offset of repetition */
		if (eng.off > eng.cod[1])
		    /* If still needs to try matches */
		    eng.cod -= eng.cod[2];
		else
		    eng.cod += 3;	/* + RepJump + bas + len-size */
		continue;

	    case Re_RepLongJump:
		/* eng.cod[1] is toplevel offset of repetition */
		if (eng.off > eng.cod[1])
		    /* If still needs to try matches */
		    eng.cod -= eng.cod[2] | (eng.cod[3] << 8);
		else
		    eng.cod += 4;	/* + RepLongJump + bas + len-size */
		continue;

	    /****************************************************
	     * Finished						*
	     ****************************************************/
	    case Re_DoneIf:
		if (eng.eo[eng.off] >= eng.so[eng.off]) {
		    eng.so[0] = eng.ss[eng.off];
		    eng.eo[0] = eng.eo[eng.off];
		    goto done;
		}
		++eng.cod;
		continue;
	    case Re_MaybeDone:
		if (eng.eo[eng.off] >= eng.so[eng.off]) {
		    eng.so[0] = eng.ss[eng.off];
		    eng.eo[0] = eng.eo[eng.off];
		    goto done;
		}
		++eng.cod;
		continue;
	    case Re_Done:
		goto done;

	    default:
		/* Fatal internal error */
		return (RE_ASSERT);
	}


wont:
	/* Surely won't match */
	if (eng.off == 0) {
	    eng.eo[0] = eng.so[0] - 1;
	    break;
	}


fail:
	if (eng.off == 0) {
	    /* If the entire string scanned */
	    if (++eng.str > eng.end) {
		eng.eo[0] = eng.so[0] - 1;
		break;
	    }
	    eng.goff = -1;
	    /* Update start of possible match after restart */
	    if (eng.eo[0] >= eng.so[0]) {
		/* If first fail */
		eng.str = eng.rstr[0];
		++eng.rstr[0];
		eng.so[0] = eng.str - eng.bas;
		eng.eo[0] = eng.so[eng.off] - 1;
	    }
	    else
		/* Just trying at next byte */
		++eng.so[0];
	}
	else
	    /* Remember this match failed */
	    eng.eo[eng.off] = eng.so[eng.off] - 1;

	/* Restart code */
	eng.cod = eng.rcod[eng.off];
	continue;


match:
	/* If first match */
	if (eng.eo[eng.off] < eng.so[eng.off]) {
	    if (eng.off == 0)
		eng.rstr[0] = eng.str + 1;
	    eng.so[eng.off] = eng.eo[eng.off] = eng.str - eng.bas;
	}
	eng.eo[eng.off] += si;
	eng.cod += ci;
	eng.str += si;
	ci = si = 1;
	continue;

done:
	break;
    }

    if (nmatch) {
	if (flags & RE_STARTEND)
	    len = pmat[0].rm_so;
	else
	    len = 0;
	if (!nosub) {
	    if (preg->cod[1] != 0xff)
		eng.goff = preg->cod[1];
	    pmat[0].rm_so = eng.so[0];
	    pmat[0].rm_eo = eng.eo[0];
	    for (i = 1; i < nmatch; i++) {
		if (i - 1 <= eng.goff) {
		    pmat[i].rm_so = eng.gso[i - 1];
		    pmat[i].rm_eo = eng.geo[i - 1];
		}
		else {
		    pmat[i].rm_so = 0;
		    pmat[i].rm_eo = -1;
		}
	    }
	    if (len) {
		/* Update offsets, since the match was done in a substring */
		j = eng.goff + 2 > nmatch ? nmatch : eng.goff + 2;
		for (i = 0; i < j; i++) {
		    pmat[i].rm_so += len;
		    pmat[i].rm_eo += len;
		}
	    }
	}
	else {
	    /*	Already know these values, allow compiling the regex with
	     * RE_NOSUB to use parenthesis only for grouping, but avoiding
	     * the runtime overhead of keeping track of the subexpression
	     * offsets. */
	    pmat[0].rm_so = eng.so[0] + len;
	    pmat[0].rm_eo = eng.eo[0] + len;
	}
    }

    return (eng.so[0] <= eng.eo[0] ? 0 : RE_NOMATCH);
}

int
reerror(int ecode, const re_cod *preg, char *ebuffer, int ebuffer_size)
{
    static char *errors[] = {
	"No error",
	"Failed to match",			/* NOMATCH */

	/* Errors not generated */
	"Invalid regular expression",		/* BADPAT */
	"Invalid collating element",		/* ECOLLATE */
	"Invalid character class",		/* ECTYPE */

	"`\' applied to unescapable character",	/* EESCAPE */
	"Invalid backreference number",		/* ESUBREG */
	"Brackets `[ ]' not balanced",		/* EBRACK */
	"Parentheses `( )' not balanced",	/* EPAREN */
	"Braces `{ }' not balanced",		/* EBRACE */
	"Invalid repetition count(s) in `{ }'",	/* BADBR */
	"Invalid character range in `[ ]'",	/* ERANGE */
	"Out of memory",			/* ESPACE */
	"`?', `*', or `+' operand invalid",	/* BADRPT */
	"Empty (sub)expression",		/* EMPTY */
	"Assertion error - you found a bug",	/* ASSERT */
	"Invalid argument"			/* INVARG */
    };
    char *str;

    if (ecode >= 0 && ecode < sizeof(errors) / sizeof(errors[0]))
	str = errors[ecode];
    else
	str = "Unknown error";

    return (snprintf(ebuffer, ebuffer_size, "%s", str));
}

void
refree(re_cod *cod)
{
    free(cod->cod);
    cod->cod = NULL;
}

static void
reinit(void)
{
    int i;
    static int first = 1;

    if (!first)
	return;
    first = 0;

    re__alnum['_'] = 1;

    for (i = '0'; i <= '7'; i++)
	re__alnum[i] = re__odigit[i] = re__ddigit[i] = re__xdigit[i] = 1;

    for (; i <= '9'; i++)
	re__alnum[i] = re__ddigit[i] = re__xdigit[i] = 1;

    for (i = 'a'; i <= 'f'; i++)
	re__alnum[i] = re__xdigit[i] = 1;
    for (; i <= 'z'; i++)
	re__alnum[i] = 1;

    for (i = 'A'; i <= 'F'; i++)
	re__alnum[i] = re__xdigit[i] = 1;
    for (; i <= 'Z'; i++)
	re__alnum[i] = 1;

    for (i = 1; i < 32; i++)
	re__control[i] = 1;
    re__control[127] = 1;
    /* Don't show tabs as control characters */
    re__control['\t'] = 0;
}

static int
rec_check(re_inf *inf, int count)
{
    if (inf->len + count >= inf->spc) {
	int spc;
	unsigned char *cod;

	if ((spc = (count % 64)) != 0)
	    spc = 64 - spc;
	spc += count + inf->spc;
	if ((cod = realloc(inf->cod, spc)) == NULL)
	    return (inf->ecode = RE_ESPACE);
	inf->cod = cod;
	inf->spc = spc;
    }

    return (inf->ecode);
}

static int
rec_code(re_inf *inf, ReCode code)
{
    if (rec_check(inf, 1) == 0)
	inf->cod[inf->len++] = code;

    return (inf->ecode);
}

static int
rec_byte(re_inf *inf, int value)
{
    if (rec_check(inf, 1) == 0)
	inf->cod[inf->len++] = value;

    return (inf->ecode);
}

static int
rec_code_byte(re_inf *inf, ReCode code, int value)
{
    if (rec_check(inf, 2) == 0) {
	inf->cod[inf->len++] = code;
	inf->cod[inf->len++] = value;
    }

    return (inf->ecode);
}

static int
rec_length(re_inf *inf, int length)
{
    int lo, hi, two;

    if (length >= 16384)
	return (inf->ecode = RE_ESPACE);

    lo = length & 0xff;
    hi = length & 0xff00;
    two = ((length > 0x7f) != 0) + 1;
    if (two == 2) {
	hi <<= 1;
	hi |= (lo & 0x80) != 0;
	lo |= 0x80;
    }

    if (rec_check(inf, two) == 0) {
	inf->cod[inf->len++] = lo;
	if (two == 2)
	    inf->cod[inf->len++] = hi >> 8;
    }

    return (inf->ecode);
}

static int
rec_byte_byte(re_inf *inf, int value0, int value1)
{
    if (rec_check(inf, 2) == 0) {
	inf->cod[inf->len++] = value0;
	inf->cod[inf->len++] = value1;
    }

    return (inf->ecode);
}

static int
rec_code_byte_byte(re_inf *inf, ReCode code, int value0, int value1)
{
    if (rec_check(inf, 3) == 0) {
	inf->cod[inf->len++] = code;
	inf->cod[inf->len++] = value0;
	inf->cod[inf->len++] = value1;
    }

    return (inf->ecode);
}

static int
rec_build_alt(re_inf *inf, rec_alt *alt)
{
    int offset, value, bas = inf->bas + 1;

    if (alt) {
	if (alt->next) {
	    if (rec_inc_spc(inf))
		return (inf->ecode);

	    /* A real a list of alternatives */
	    rec_code(inf, Re_Alt);

	    offset = inf->len;		/* Remember current offset */
	    rec_byte_byte(inf, 0, 0);	/* Reserve two bytes for retry address */
	    while (alt && inf->ecode == 0) {
		if (rec_build_pat(inf, alt->pat))
		    break;
		alt = alt->next;
		if (alt && inf->ecode == 0) {
		    /* Handle (hyper)complex repetitions */
		    if (inf->bas != bas) {
			/* Duplicate patterns up to end of expression */
			rec_build_pat(inf, inf->apat);
			/* Restore engine state for next alternative(s) */
			rec_alt_spc(inf, bas - 1);
		    }

		    /* If the jump would be so long */
		    if ((value = inf->len - offset) >= 16384) {
			inf->ecode = RE_ESPACE;
			break;
		    }
		    inf->cod[offset] = value & 0xff;
		    inf->cod[offset + 1] = (value & 0xff00) >> 8;

		    rec_code(inf, Re_AltNext);
		    offset = inf->len;
		    rec_byte_byte(inf, 0, 0);
		}
	    }
	    if (inf->ecode == 0) {
		/* Handle (hyper)complex repetitions */
		if (inf->bas != bas) {
		    /* Duplicate patterns up to end of expression */
		    rec_build_pat(inf, inf->apat);
		    /* Restore engine state for next alternative(s) */
		    rec_alt_spc(inf, bas - 1);
		}

		/* If the jump would be so long */
		if ((value = inf->len - offset) >= 16384)
		    return (inf->ecode = RE_ESPACE);
		inf->cod[offset] = value & 0xff;
		inf->cod[offset + 1] = (value & 0xff00) >> 8;
		/* Last jump is here */
		rec_code(inf, Re_AltDone);
	    }
	    rec_dec_spc(inf);
	}
	else
	    /* Single alternative */
	    rec_build_pat(inf, alt->pat);
    }

    return (inf->ecode);
}

static int
rec_build_pat(re_inf *inf, rec_pat *pat)
{
    rec_pat *apat;
    int length, offset = 0, distance, jump = 0, bas = 0;

    while (pat && inf->ecode == 0) {
	if (pat->rep) {
	    bas = inf->bas;
	    if (pat->type == Rep_Group && !inf->par && rec_code(inf, Re_Open))
		return (inf->ecode);
	    if (rec_inc_spc(inf))
		return (inf->ecode);
	    offset = inf->len;
	    if (rec_build_rep(inf, pat->rep))
		break;
	    /* Reserve space to jump after repetition done */
	    jump = inf->len;
	    rec_byte_byte(inf, 0, 0);
	}
	switch (pat->type) {
	    case Rep_AnyAnyTimes:
	    case Rep_AnyMaybe:
	    case Rep_AnyAtLeast:
		if (rec_add_spc(inf, pat->type == Rep_AnyMaybe))
		    return (inf->ecode);
		if (rec_code(inf, (ReCode)pat->type) == 0 &&
		    rec_byte(inf, inf->bas - 1) == 0 &&
		    rec_byte(inf, inf->ref - 1) == 0)
		    rec_off_spc(inf);
		break;
	    case Rep_Literal:
	    case Rep_LiteralNot:
	    case Rep_SearchLiteral:
		rec_code_byte(inf, (ReCode)pat->type, pat->data.chr);
		break;
	    case Rep_CaseLiteral:
	    case Rep_CaseLiteralNot:
	    case Rep_SearchCaseLiteral:
		rec_code_byte_byte(inf, (ReCode)pat->type,
				   pat->data.cse.lower, pat->data.cse.upper);
		break;
	    case Rep_Range:
	    case Rep_RangeNot:
		if (rec_code(inf, (ReCode)pat->type) == 0)
		    rec_build_rng(inf, pat->data.rng);
		break;
	    case Rep_String:
	    case Rep_SearchString:
	    case Rep_CaseString:
	    case Rep_SearchCaseString:
		rec_code(inf, (ReCode)pat->type);
		length = strlen((char*)pat->data.str);
		if (rec_length(inf, length) == 0 && rec_check(inf, length) == 0) {
		    memcpy(inf->cod + inf->len, pat->data.str, length);
		    inf->len += length;
		}
		break;
	    case Rep_Any:
	    case Rep_AnyEatAnyTimes:
	    case Rep_AnyEatMaybe:
	    case Rep_AnyEatAtLeast:
	    case Rep_Odigit:
	    case Rep_OdigitNot:
	    case Rep_Digit:
	    case Rep_DigitNot:
	    case Rep_Xdigit:
	    case Rep_XdigitNot:
	    case Rep_Space:
	    case Rep_SpaceNot:
	    case Rep_Tab:
	    case Rep_Newline:
	    case Rep_Lower:
	    case Rep_Upper:
	    case Rep_Alnum:
	    case Rep_AlnumNot:
	    case Rep_Control:
	    case Rep_ControlNot:
	    case Rep_Bol:
	    case Rep_Eol:
	    case Rep_Bow:
	    case Rep_Eow:
		rec_code(inf, (ReCode)pat->type);
		break;
	    case Rep_Backref:
		rec_code_byte(inf, Re_Backref, pat->data.chr);
		break;
	    case Rep_Group:
		if (pat->rep == NULL && !inf->par && rec_code(inf, Re_Open))
		    break;
		apat = inf->apat;
		inf->apat = pat->next;
		rec_build_grp(inf, pat->data.grp);
		inf->apat = apat;
		break;
	    case Rep_StringList:
		rec_build_stl(inf, pat->data.stl);
		break;
	}
	if (pat->rep) {
#if 0
	    if (rec_dec_spc(inf))
		return (inf->ecode);
#else
	    if (rec_rep_spc(inf, bas))
		return (inf->ecode);
#endif
	    distance = inf->len - offset;
	    if (distance > 255) {
		if (rec_code(inf, Re_RepLongJump) ||
		    rec_byte(inf, inf->bas) ||
		    rec_byte(inf, distance & 0xff) ||
		    rec_byte(inf, (distance & 0xff00) >> 8))
		break;
	    }
	    else if (rec_code(inf, Re_RepJump) ||
		     rec_byte(inf, inf->bas) ||
		     rec_byte(inf, distance))
		break;
	    distance = inf->len - offset;
	    inf->cod[jump] = distance & 0xff;
	    inf->cod[jump + 1] = (distance & 0xff00) >> 8;
	}
	pat = pat->next;
    }

    return (inf->ecode);
}

static int
rec_build_rng(re_inf *inf, rec_rng *rng)
{
    if (rec_check(inf, sizeof(rng->range)) == 0) {
	memcpy(inf->cod + inf->len, rng->range, sizeof(rng->range));
	inf->len += sizeof(rng->range);
    }

    return (inf->ecode);
}

static int
rec_build_grp(re_inf *inf, rec_grp *grp)
{
    int par = inf->par;

    if (!(inf->flags & RE_NOSUB)) {
	++inf->par;
	if (par == 0)
	    ++inf->ref;
	if (rec_build_alt(inf, grp->alt) == 0) {
	    if (par == 0) {
		if (grp->comp)
		    rec_code_byte(inf, Re_Update, inf->ref - 1);
		else
		    rec_code(inf, Re_Close);
	    }
	}
	--inf->par;
    }
    else
	rec_build_alt(inf, grp->alt);

    return (inf->ecode);
}

static int
rec_build_stl(re_inf *inf, rec_stl *stl)
{
    int i, len, rlen;
    ReCode code;

    /* Calculate jump distance information */
    rlen = stl->tlen + stl->nstrs + 4;
    /* + code + nstrs + place-offset + data-length */

    if (stl->nstrs >= LARGE_STL_COUNT) {
	rlen += 511;		/* Don't write number of strings */
	code = stl->type == Rep_StringList ?
		Re_LargeStringList : Re_LargeCaseStringList;
    }
    else
	code = (ReCode)stl->type;

    if (rlen >= 16386)
	return (inf->ecode = RE_ESPACE);
    if (rec_check(inf, rlen) ||
	rec_code(inf, code))
	return (inf->ecode);

    /* Space is allocated, just write the data */
    if (stl->nstrs < LARGE_STL_COUNT)
	inf->cod[inf->len++] = stl->nstrs;

    inf->cod[inf->len++] = rlen & 0xff;
    inf->cod[inf->len++] = (rlen & 0xff00) >> 8;

    if (stl->nstrs < LARGE_STL_COUNT) {
	for (i = 0; i < stl->nstrs; i++)
	    inf->cod[inf->len++] = stl->lens[i];
	for (i = 0; i < stl->nstrs; i++) {
	    len = stl->lens[i];
	    if (len > 2) {
		memcpy(inf->cod + inf->len, stl->strs[i], len);
		inf->len += len;
	    }
	    else {
		if (len == 1)
		    inf->cod[inf->len++] = (long)stl->strs[i];
		else {
		    inf->cod[inf->len++] = (long)stl->strs[i] & 0xff;
		    inf->cod[inf->len++] = ((long)stl->strs[i] & 0xff00) >> 8;
		}
	    }
	}
    }
    else {
	/* The string length goes before the string itself */
	int j, chl, chu;

	/* Fill everything with an invalid jump address */
	memset(inf->cod + inf->len, 0xff, 512);
	for (i = len = 0, j = -1; i < stl->nstrs; i++) {
	    chl = stl->lens[i] > 2 ? stl->strs[i][0] : (long)stl->strs[i] & 0xff;
	    if (chl != j) {
		inf->cod[inf->len + (chl << 1)] = len & 0xff;
		inf->cod[inf->len + (chl << 1) + 1] = (len & 0xff00) >> 8;
		if (code == Re_LargeCaseStringList) {
		    chu = stl->lens[i] > 2 ?
			stl->strs[i][1] : ((long)(stl->strs[i]) & 0xff00) >> 8;
		    inf->cod[inf->len + (chu << 1)] = len & 0xff;
		    inf->cod[inf->len + (chu << 1) + 1] = (len & 0xff00) >> 8;
		}
		j = chl;
	    }
	    len += stl->lens[i] + 1;
	}
	inf->len += 512;

	for (i = 0; i < stl->nstrs; i++) {
	    len = stl->lens[i];
	    inf->cod[inf->len++] = len;
	    if (len > 2) {
		memcpy(inf->cod + inf->len, stl->strs[i], len);
		inf->len += len;
	    }
	    else {
		if (len == 1)
		    inf->cod[inf->len++] = (long)stl->strs[i];
		else {
		    inf->cod[inf->len++] = (long)stl->strs[i] & 0xff;
		    inf->cod[inf->len++] = ((long)stl->strs[i] & 0xff00) >> 8;
		}
	    }
	}
    }

    return (inf->ecode);
}

static int
rec_build_rep(re_inf *inf, rec_rep *rep)
{
    if (rep) {
	switch (rep->type) {
	    case Rer_AnyTimes:
	    case Rer_AtLeast:
	    case Rer_Maybe:
		rec_code(inf, (ReCode)rep->type);
		break;
	    case Rer_Exact:
		if (rec_code(inf, Re_Exact) == 0)
		    rec_byte(inf, rep->mine);
		break;
	    case Rer_Min:
		if (rec_code(inf, Re_Min) == 0)
		    rec_byte(inf, rep->mine);
		break;
	    case Rer_Max:
		if (rec_code(inf, Re_Max) == 0)
		    rec_byte(inf, rep->maxc);
		break;
	    case Rer_MinMax:
		if (rec_code(inf, Re_MinMax) == 0 &&
		    rec_byte(inf, rep->mine) == 0)
		    rec_byte(inf, rep->maxc);
		break;
	}
	/* It is incremented in rec_build_pat */
	rec_byte(inf, inf->bas - 1);
    }

    return (inf->ecode);
}

static int
rec_inc_spc(re_inf *inf)
{
    if (++inf->bas >= MAX_DEPTH)
	return (inf->ecode = RE_ESPACE);

    return (inf->ecode);
}

static int
rec_dec_spc(re_inf *inf)
{
    if (--inf->bas < 0)
	return (inf->ecode = RE_ASSERT);

    return (inf->ecode);
}

static int
rec_add_spc(re_inf *inf, int maybe)
{
    if (++inf->bas >= MAX_DEPTH)
	return (inf->ecode = RE_ESPACE);
    inf->sp[inf->bas] = maybe + 1;

    return (inf->ecode);
}

/* Could be joined with rec_rep_spc, code almost identical */
static int
rec_alt_spc(re_inf *inf, int top)
{
    int distance, i, bas = inf->bas;

    while ((inf->bas > top) && inf->sp[inf->bas]) {
	/* Jump to this repetition for cleanup */
	distance = inf->len - inf->sr[inf->bas];

	/* This will generate a jump to a jump decision opcode */
	inf->sj[inf->bas] = inf->len;

	if (distance > 255) {
	    if (rec_code(inf, Re_RepLongJump) ||
		rec_byte(inf, inf->bas - 1) ||
		rec_byte(inf, distance & 0xff) ||
		rec_byte(inf, (distance & 0xff00) >> 8))
	    break;
	}
	else if (rec_code(inf, Re_RepJump) ||
		 rec_byte(inf, inf->bas - 1) ||
		 rec_byte(inf, distance))
 	    break;

	/* Top of stack value before repetition, or end condition value */
	--inf->bas;
    }

    i = inf->bas + 1;

    if (inf->ecode == 0 && i <= bas && inf->sp[i]) {
	/*  Only the repetition at the bottom jump to code after testing
	 * all possibilities */
	distance = inf->len - inf->sr[i];
	inf->cod[inf->sr[i] + 3] = distance & 0xff;
	inf->cod[inf->sr[i] + 4] = (distance & 0xff00) >> 8;

	/* The bottom jump is here */
	if (rec_code(inf, inf->sp[i] == 1 ? Re_DoneIf : Re_MaybeDone))
	   return (inf->ecode);

	/*  Generate jumps to the previous special repetition */
	for (++i; i <= bas; i++) {
	    if (inf->sp[i]) {
		distance = inf->sj[i] - inf->sr[i];
		inf->cod[inf->sr[i] + 3] = distance & 0xff;
		inf->cod[inf->sr[i] + 4] = (distance & 0xff00) >> 8;
	    }
	}
    }

    return (inf->ecode);
}

static int
rec_rep_spc(re_inf *inf, int top)
{
    int distance, i, bas = inf->bas;

    while (inf->bas > top) {
	if (inf->sp[inf->bas]) {
	    /* Jump to this repetition for cleanup */
	    distance = inf->len - inf->sr[inf->bas];

	    /* This will generate a jump to a jump decision opcode */
	    inf->sj[inf->bas] = inf->len;

	    if (distance > 255) {
		if (rec_code(inf, Re_RepLongJump) ||
		    rec_byte(inf, inf->bas - 1) ||
		    rec_byte(inf, distance & 0xff) ||
		    rec_byte(inf, (distance & 0xff00) >> 8))
		break;
	    }
	    else if (rec_code(inf, Re_RepJump) ||
		     rec_byte(inf, inf->bas - 1) ||
		     rec_byte(inf, distance))
		break;
	}

	/* Top of stack value before repetition, or end condition value */
	--inf->bas;
    }

    /* Find first special repetition offset. XXX This should be a noop */
    for (i = 0; i < bas; i++)
	if (inf->sp[i])
	    break;

    if (inf->ecode == 0 && i <= bas && inf->sp[i]) {
	/*  Only the repetition at the bottom jump to code after testing
	 * all possibilities */
	distance = inf->len - inf->sr[i];
	inf->cod[inf->sr[i] + 3] = distance & 0xff;
	inf->cod[inf->sr[i] + 4] = (distance & 0xff00) >> 8;

	/*  Generate jumps to the previous special repetition */
	for (++i; i <= bas; i++) {
	    if (inf->sp[i]) {
		distance = inf->sj[i] - inf->sr[i];
		inf->cod[inf->sr[i] + 3] = distance & 0xff;
		inf->cod[inf->sr[i] + 4] = (distance & 0xff00) >> 8;
	    }
	}
    }

    return (inf->ecode);
}

static int
rec_off_spc(re_inf *inf)
{
    /* The jump address before the three bytes instruction */
    inf->sr[inf->bas] = inf->len - 3;
    /*  Don't know yet where to go after done with the special
     * repetition, just reserve two bytes for the jump address. */
    return (rec_byte_byte(inf, 0, 0));
}

#ifdef DEBUG
static void
redump(re_cod *code)
{
    int i, j, k;
    unsigned char *cod = code->cod, *stl;

    if (cod[0] & RE_NOSUB)
	printf("Nosub\n");
    if (cod[0] & RE_NEWLINE)
	printf("Newline\n");
    ++cod;
    if (cod[0] != 0xff)
	printf("%d backrefs\n", cod[0] + 1);
    ++cod;
    for (;;) {
	switch (*cod++) {
	    case Re_Open:
		printf("Open");
		break;
	    case Re_Close:
		printf("Close");
		break;
	    case Re_Update:
		printf("Update (%d)", (int)*cod++);
		break;
	    case Re_Alt:
		printf("Alt");
		i = cod[0] | cod[1];
		cod += 2;
		printf(" %d", i);
		break;
	    case Re_AltNext:
		printf("Alt-next");
		i = cod[0] | cod[1];
		cod += 2;
		printf(" %d", i);
		break;
	    case Re_AltDone:
		printf("Alt-done");
		break;
	    case Re_AnyTimes:
		printf("-> Anytimes %d", (int)*cod++);
		i = cod[0] | (cod[1] << 8);
		cod += 2;
		printf(" /%d", i);
		break;
	    case Re_AnyEatAnyTimes:
		printf("Any-eat-anytimes");
		break;
	    case Re_AnyAnyTimes:
		printf("-> Any-anytimes %d", (int)*cod++);
		printf(" (%d)", (int)*cod++);
		i = cod[0] | (cod[1] << 8);
		cod += 2;
		printf(" /%d", i);
		break;
	    case Re_AnyEatMaybe:
		printf("Any-eat-maybe");
		break;
	    case Re_AnyMaybe:
		printf("-> Any-maybe %d", (int)*cod++);
		printf(" (%d)", (int)*cod++);
		i = cod[0] | (cod[1] << 8);
		cod += 2;
		printf(" /%d", i);
		break;
	    case Re_AnyAtLeast:
		printf("-> Any-atleast %d", (int)*cod++);
		printf(" (%d)", (int)*cod++);
		i = cod[0] | (cod[1] << 8);
		cod += 2;
		printf(" /%d", i);
		break;
	    case Re_AnyEatAtLeast:
		printf("Any-eat-atleast");
		break;
	    case Re_Maybe:
		printf("-> Maybe %d", (int)*cod++);
		i = cod[0] | (cod[1] << 8);
		cod += 2;
		printf(" /%d", i);
		break;
	    case Re_AtLeast:
		printf("-> Atleast %d", (int)*cod++);
		i = cod[0] | (cod[1] << 8);
		cod += 2;
		printf(" /%d", i);
		break;
	    case Re_Exact:
		printf("-> Exact ");
		i = *cod++;
		printf("%d", i);
		printf(" %d", (int)*cod++);
		i = cod[0] | (cod[1] << 8);
		cod += 2;
		printf(" /%d", i);
		break;
	    case Re_Min:
		printf("-> Min ");
		i = *cod++;
		printf("%d", i);
		printf(" %d", (int)*cod++);
		i = cod[0] | (cod[1] << 8);
		cod += 2;
		printf(" /%d", i);
		break;
	    case Re_Max:
		printf("-> Max ");
		i = *cod++;
		printf("%d", i);
		printf(" %d", (int)*cod++);
		i = cod[0] | (cod[1] << 8);
		cod += 2;
		printf(" /%d", i);
		break;
	    case Re_MinMax:
		printf("-> Min-max ");
		i = *cod++;
		printf("%d ", i);
		i = *cod++;
		printf("%d", i);
		printf(" %d", (int)*cod++);
		i = cod[0] | (cod[1] << 8);
		cod += 2;
		printf(" /%d", i);
		break;
	    case Re_RepJump:
		printf("<- Rep-jump %d ", (int)*cod++);
		i = *cod++;
		printf("%d", i);
		break;
	    case Re_RepLongJump:
		printf("<- Rep-long-jump %d ", (int)*cod++);
		i = cod[0] | (cod[1] << 8);
		printf("%d", i);
		break;
	    case Re_Any:
		printf("Any");
		break;
	    case Re_Odigit:
		printf("Odigit");
		break;
	    case Re_OdigitNot:
		printf("Odigit-not");
		break;
	    case Re_Digit:
		printf("Digit");
		break;
	    case Re_DigitNot:
		printf("Digit-not");
		break;
	    case Re_Xdigit:
		printf("Xdigit");
		break;
	    case Re_XdigitNot:
		printf("Xdigit-not");
		break;
	    case Re_Space:
		printf("Space");
		break;
	    case Re_SpaceNot:
		printf("Space-not");
		break;
	    case Re_Tab:
		printf("Tab");
		break;
	    case Re_Newline:
		printf("Newline");
		break;
	    case Re_Lower:
		printf("Lower");
		break;
	    case Re_Upper:
		printf("Upper");
		break;
	    case Re_Alnum:
		printf("Alnum");
		break;
	    case Re_AlnumNot:
		printf("Alnum-not");
		break;
	    case Re_Control:
		printf("Control");
		break;
	    case Re_ControlNot:
		printf("Control-not");
		break;
	    case Re_Bol:
		printf("Bol");
		break;
	    case Re_Eol:
		printf("Eol");
		break;
	    case Re_Bow:
		printf("Bow");
		break;
	    case Re_Eow:
		printf("Eow");
		break;
	    case Re_Range:
		printf("Range ");
		goto range;
	    case Re_RangeNot:
		printf("Range-not ");
range:
		for (i = 0; i < 256; i += 32) {
		    for (j = k = 0; j < 32; j++)
			k |= (*cod++ & 1) << (31 - j);
		    printf("%x ", k);
		}
		break;
	    case Re_Literal:
		printf("Literal %c", *cod++);
		break;
	    case Re_LiteralNot:
		printf("Literal-not %c", *cod++);
		break;
	    case Re_SearchLiteral:
		printf("Search-literal %c", *cod++);
		break;
	    case Re_CaseLiteral:
		printf("Case-literal %c", *cod++);
		putchar(*cod++);
		break;
	    case Re_CaseLiteralNot:
		printf("Case-literal-not %c", *cod++);
		putchar(*cod++);
		break;
	    case Re_SearchCaseLiteral:
		printf("Search-case-literal %c", *cod++);
		putchar(*cod++);
		break;
	    case Re_String:
		printf("String ");
		goto string;
	    case Re_SearchString:
		printf("Search-string ");
		goto string;
	    case Re_CaseString:
		printf("Case-string ");
		goto string;
	    case Re_SearchCaseString:
		printf("Search-case-string ");
string:
		i = *cod++;
		if (i & 0x80)
		    i = (i & 0x7f) | (*cod++ << 7);
		for (j = 0; j < i; j++)
		    putchar(*cod++);
		break;
	    case Re_StringList:
		printf("String-list");
		goto string_list;
	    case Re_CaseStringList:
		printf("Case-string-list");
string_list:
		j = *cod++;
		cod += 2;
		stl = cod + j;
		for (i = 0; i < j; i++) {
		    k = *cod++;
		    putchar(i ? ',' : ' ');
		    fwrite(stl, k, 1, stdout);
		    stl += k;
		}
		cod = stl;
		break;
	    case Re_LargeStringList:
		printf("Large-string-list");
large_string_list:
		i = cod[0] | (cod[1] << 8);
		stl = cod + i - 1;
		for (i = 0, cod += 514; cod < stl; i++) {
		    k = *cod++;
		    putchar(i ? ',' : ' ');
		    fwrite(cod, k, 1, stdout);
		    cod += k;
		}
		cod = stl;
		break;
	    case Re_LargeCaseStringList:
		printf("Large-case-string-list");
		goto large_string_list;
	    case Re_Backref:
		printf("Backref %d", (int)*cod++);
		break;
	    case Re_DoneIf:
		printf("Done-if");
		break;
	    case Re_MaybeDone:
		printf("Maybe-done");
		break;
	    case Re_Done:
		printf("Done\n");
		return;
	}
	putchar('\n');
    }
}
#endif
