/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
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

/* $XFree86: xc/programs/xedit/lisp/helper.c,v 1.51 2003/09/17 22:28:08 paulo Exp $ */

#include "lisp/helper.h"
#include "lisp/pathname.h"
#include "lisp/package.h"
#include "lisp/read.h"
#include "lisp/stream.h"
#include "lisp/write.h"
#include "lisp/hash.h"
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>

/*
 * Prototypes
 */
static LispObj *LispReallyDo(LispBuiltin*, int);
static LispObj *LispReallyDoListTimes(LispBuiltin*, int);

/* in math.c */
extern LispObj *LispFloatCoerce(LispBuiltin*, LispObj*);

/*
 * Implementation
 */
LispObj *
LispObjectCompare(LispObj *left, LispObj *right, int function)
{
    LispType ltype, rtype;
    LispObj *result = left == right ? T : NIL;

    /* If left and right are the same object, or if function is EQ */
    if (result == T || function == FEQ)
	return (result);

    ltype = OBJECT_TYPE(left);
    rtype = OBJECT_TYPE(right);

    /* Equalp requires that numeric objects be compared by value, and
     * strings or characters comparison be case insenstive */
    if (function == FEQUALP) {
	switch (ltype) {
	    case LispFixnum_t:
	    case LispInteger_t:
	    case LispBignum_t:
	    case LispDFloat_t:
	    case LispRatio_t:
	    case LispBigratio_t:
	    case LispComplex_t:
		switch (rtype) {
		    case LispFixnum_t:
		    case LispInteger_t:
		    case LispBignum_t:
		    case LispDFloat_t:
		    case LispRatio_t:
		    case LispBigratio_t:
		    case LispComplex_t:
			result = APPLY2(Oequal_, left, right);
			break;
		    default:
			break;
		}
		goto compare_done;
	    case LispSChar_t:
		if (rtype == LispSChar_t &&
		    toupper(SCHAR_VALUE(left)) == toupper(SCHAR_VALUE(right)))
		    result = T;
		goto compare_done;
	    case LispString_t:
		if (rtype == LispString_t && STRLEN(left) == STRLEN(right)) {
		    long i = STRLEN(left);
		    char *sl = THESTR(left), *sr = THESTR(right);

		    for (--i; i >= 0; i--)
			if (toupper(sl[i]) != toupper(sr[i]))
			    break;
		    if (i < 0)
			result = T;
		}
		goto compare_done;
	    case LispArray_t:
		if (rtype == LispArray_t &&
		    left->data.array.type == right->data.array.type &&
		    left->data.array.rank == right->data.array.rank &&
		    LispObjectCompare(left->data.array.dim,
				      right->data.array.dim,
				      FEQUAL) != NIL) {
		    LispObj *llist = left->data.array.list,
		    	    *rlist = right->data.array.list;

		    for (; CONSP(llist); llist = CDR(llist), rlist = CDR(rlist))
			if (LispObjectCompare(CAR(llist), CAR(rlist),
					      FEQUALP) == NIL)
			    break;
		    if (!CONSP(llist))
			result = T;
		}
		goto compare_done;
	    case LispStruct_t:
		if (rtype == LispStruct_t &&
		    left->data.struc.def == right->data.struc.def) {
		    LispObj *lfield = left->data.struc.fields,
		    	    *rfield = right->data.struc.fields;

		    for (; CONSP(lfield);
			 lfield = CDR(lfield), rfield = CDR(rfield)) {
			if (LispObjectCompare(CAR(lfield), CAR(rfield),
					      FEQUALP) != T)
			    break;
		    }
		    if (!CONSP(lfield))
			result = T;
		}
		goto compare_done;
	    case LispHashTable_t:
		if (rtype == LispHashTable_t &&
		    left->data.hash.table->count ==
		    right->data.hash.table->count &&
		    left->data.hash.test == right->data.hash.test) {
		    unsigned long i;
		    LispObj *test = left->data.hash.test;
		    LispHashEntry *lentry = left->data.hash.table->entries,
				  *llast = lentry +
					   left->data.hash.table->num_entries,
				  *rentry = right->data.hash.table->entries;

		    for (; lentry < llast; lentry++, rentry++) {
			if (lentry->count != rentry->count)
			    break;
			for (i = 0; i < lentry->count; i++) {
			    if (APPLY2(test,
				       lentry->keys[i],
				       rentry->keys[i]) == NIL ||
				LispObjectCompare(lentry->values[i],
						  rentry->values[i],
						  FEQUALP) == NIL)
				break;
			}
			if (i < lentry->count)
			    break;
		    }
		    if (lentry == llast)
			result = T;
		}
		goto compare_done;
	    default:
		break;
	}
    }

    /* Function is EQL or EQUAL, or EQUALP on arguments with the same rules */
    if (ltype == rtype) {
	switch (ltype) {
	    case LispFixnum_t:
	    case LispSChar_t:
		if (FIXNUM_VALUE(left) == FIXNUM_VALUE(right))
		    result = T;
		break;
	    case LispInteger_t:
		if (INT_VALUE(left) == INT_VALUE(right))
		    result = T;
		break;
	    case LispDFloat_t:
		if (DFLOAT_VALUE(left) == DFLOAT_VALUE(right))
		    result = T;
		break;
	    case LispRatio_t:
		if (left->data.ratio.numerator ==
		    right->data.ratio.numerator &&
		    left->data.ratio.denominator ==
		    right->data.ratio.denominator)
		    result = T;
		break;
	    case LispComplex_t:
		if (LispObjectCompare(left->data.complex.real,
				      right->data.complex.real,
				      function) == T &&
		    LispObjectCompare(left->data.complex.imag,
				      right->data.complex.imag,
				      function) == T)
		    result = T;
		break;
	    case LispBignum_t:
		if (mpi_cmp(left->data.mp.integer, right->data.mp.integer) == 0)
		    result = T;
		break;
	    case LispBigratio_t:
		if (mpr_cmp(left->data.mp.ratio, right->data.mp.ratio) == 0)
		    result = T;
		break;
	    default:
		break;
	}

	/* Next types must be the same object for EQL */
	if (function == FEQL)
	    goto compare_done;

	switch (ltype) {
	    case LispString_t:
		if (STRLEN(left) == STRLEN(right) &&
		    memcmp(THESTR(left), THESTR(right), STRLEN(left)) == 0)
		    result = T;
		break;
	    case LispCons_t:
		if (LispObjectCompare(CAR(left), CAR(right), function) == T &&
		    LispObjectCompare(CDR(left), CDR(right), function) == T)
		    result = T;
		break;
	    case LispQuote_t:
	    case LispBackquote_t:
	    case LispPathname_t:
		result = LispObjectCompare(left->data.pathname,
					   right->data.pathname, function);
		break;
	    case LispLambda_t:
		result = LispObjectCompare(left->data.lambda.name,
					   right->data.lambda.name,
					   function);
		break;
	    case LispOpaque_t:
		if (left->data.opaque.data == right->data.opaque.data)
		    result = T;
		break;
	    case LispRegex_t:
		/* If the regexs are guaranteed to generate the same matches */
		if (left->data.regex.options == right->data.regex.options)
		    result = LispObjectCompare(left->data.regex.pattern,
					       right->data.regex.pattern,
					       function);
		break;
	    default:
		break;
	}
    }

compare_done:
    return (result);
}

void
LispCheckSequenceStartEnd(LispBuiltin *builtin,
			  LispObj *sequence, LispObj *start, LispObj *end,
			  long *pstart, long *pend, long *plength)
{
    /* Calculate length of sequence and check it's type */
    *plength = LispLength(sequence);

    /* Check start argument */
    if (start == UNSPEC || start == NIL)
	*pstart = 0;
    else {
	CHECK_INDEX(start);
	*pstart = FIXNUM_VALUE(start);
    }

    /* Check end argument */
    if (end == UNSPEC || end == NIL)
	*pend = *plength;
    else {
	CHECK_INDEX(end);
	*pend = FIXNUM_VALUE(end);
    }

    /* Check start argument */
    if (*pstart > *pend)
	LispDestroy("%s: :START %ld is larger than :END %ld",
		    STRFUN(builtin), *pstart, *pend);

    /* Check end argument */
    if (*pend > *plength)
	LispDestroy("%s: :END %ld is larger then sequence length %ld",
		    STRFUN(builtin), *pend, *plength);
}

long
LispLength(LispObj *sequence)
{
    long length;

    if (sequence == NIL)
	return (0);
    switch (OBJECT_TYPE(sequence)) {
	case LispString_t:
	    length = STRLEN(sequence);
	    break;
	case LispArray_t:
	    if (sequence->data.array.rank != 1)
		goto not_a_sequence;
	    sequence = sequence->data.array.list;
	    /*FALLTROUGH*/
	case LispCons_t:
	    for (length = 0;
		 CONSP(sequence);
		 length++, sequence = CDR(sequence))
		;
	    break;
	default:
not_a_sequence:
	    LispDestroy("LENGTH: %s is not a sequence", STROBJ(sequence));
	    /*NOTREACHED*/
	    length = 0;
    }

    return (length);
}

LispObj *
LispCharacterCoerce(LispBuiltin *builtin, LispObj *object)
{
    if (SCHARP(object))
	return (object);
    else if (STRINGP(object) && STRLEN(object) == 1)
	return (SCHAR(THESTR(object)[0]));
    else if (SYMBOLP(object) && ATOMID(object)[1] == '\0')
	return (SCHAR(ATOMID(object)[0]));
    else if (INDEXP(object)) {
	int c = FIXNUM_VALUE(object);

	if (c <= 0xff)
	    return (SCHAR(c));
    }
    else if (object == T)
	return (SCHAR('T'));

    LispDestroy("%s: cannot convert %s to character",
		STRFUN(builtin), STROBJ(object));
    /*NOTREACHED*/
    return (NIL);
}

LispObj *
LispStringCoerce(LispBuiltin *builtin, LispObj *object)
{
    if (STRINGP(object))
	return (object);
    else if (SYMBOLP(object))
	return (LispSymbolName(object));
    else if (SCHARP(object)) {
	char string[1];

	string[0] = SCHAR_VALUE(object);
	return (LSTRING(string, 1));
    }
    else if (object == NIL)
	return (LSTRING(Snil, 3));
    else if (object == T)
	return (LSTRING(St, 1));
    else
	LispDestroy("%s: cannot convert %s to string",
		    STRFUN(builtin), STROBJ(object));
    /*NOTREACHED*/
    return (NIL);
}

LispObj *
LispCoerce(LispBuiltin *builtin,
	   LispObj *object, LispObj *result_type)
{
    LispObj *result = NIL;
    LispType type = LispNil_t;

    if (result_type == NIL)
	/* not even NIL can be converted to NIL? */
	LispDestroy("%s: cannot convert %s to NIL",
		    STRFUN(builtin), STROBJ(object));

    else if (result_type == T)
	/* no conversion */
	return (object);

    else if (!SYMBOLP(result_type))
	/* only know about simple types */
	LispDestroy("%s: bad argument %s",
		    STRFUN(builtin), STROBJ(result_type));

    else {
	/* check all known types */

	Atom_id atom = ATOMID(result_type);

	if (atom == Satom) {
	    if (CONSP(object))
		goto coerce_fail;
	    return (object);
	}
	/* only convert ATOM to SYMBOL */

	if (atom == Sfloat)
	    type = LispDFloat_t;
	else if (atom == Sinteger)
	    type = LispInteger_t;
	else if (atom == Scons || atom == Slist) {
	    if (object == NIL)
		return (object);
	    type = LispCons_t;
	}
	else if (atom == Sstring)
	    type = LispString_t;
	else if (atom == Scharacter)
	    type = LispSChar_t;
	else if (atom == Scomplex)
	    type = LispComplex_t;
	else if (atom == Svector || atom == Sarray)
	    type = LispArray_t;
	else if (atom == Sopaque)
	    type = LispOpaque_t;
	else if (atom == Srational)
	    type = LispRatio_t;
	else if (atom == Spathname)
	    type = LispPathname_t;
	else
	    LispDestroy("%s: invalid type specification %s",
			STRFUN(builtin), ATOMID(result_type));
    }

    if (OBJECT_TYPE(object) == LispOpaque_t) {
	switch (type) {
	    case LispAtom_t:
		result = ATOM(object->data.opaque.data);
		break;
	    case LispString_t:
		result = STRING(object->data.opaque.data);
		break;
	    case LispSChar_t:
		result = SCHAR((unsigned long)object->data.opaque.data);
		break;
	    case LispDFloat_t:
		result = DFLOAT((double)((long)object->data.opaque.data));
		break;
	    case LispInteger_t:
		result = INTEGER(((long)object->data.opaque.data));
		break;
	    case LispOpaque_t:
		result = OPAQUE(object->data.opaque.data, 0);
		break;
	    default:
		goto coerce_fail;
		break;
	}
    }

    else if (OBJECT_TYPE(object) != type) {
	switch (type) {
	    case LispInteger_t:
		if (INTEGERP(object))
		    result = object;
		else if (DFLOATP(object)) {
		    if ((long)DFLOAT_VALUE(object) == DFLOAT_VALUE(object))
			result = INTEGER((long)DFLOAT_VALUE(object));
		    else {
			mpi *integer = LispMalloc(sizeof(mpi));

			mpi_init(integer);
			mpi_setd(integer, DFLOAT_VALUE(object));
			if (mpi_getd(integer) != DFLOAT_VALUE(object)) {
			    mpi_clear(integer);
			    LispFree(integer);
			    goto coerce_fail;
			}
			result = BIGNUM(integer);
		    }
		}
		else
		    goto coerce_fail;
		break;
	    case LispRatio_t:
		if (DFLOATP(object)) {
		    mpr *ratio = LispMalloc(sizeof(mpr));

		    mpr_init(ratio);
		    mpr_setd(ratio, DFLOAT_VALUE(object));
		    if (mpr_fiti(ratio)) {
			result = RATIO(mpi_geti(mpr_num(ratio)),
				       mpi_geti(mpr_den(ratio)));
			mpr_clear(ratio);
			LispFree(ratio);
		    }
		    else
			result = BIGRATIO(ratio);
		}
		else if (RATIONALP(object))
		    result = object;
		else
		    goto coerce_fail;
		break;
	    case LispDFloat_t:
		result = LispFloatCoerce(builtin, object);
	    	break;
	    case LispComplex_t:
		if (NUMBERP(object))
		    result = object;
		else
		    goto coerce_fail;
		break;
	    case LispString_t:
		if (object == NIL)
		    result = STRING("");
		else
		    result = LispStringCoerce(builtin, object);
		break;
	    case LispSChar_t:
		result = LispCharacterCoerce(builtin, object);
		break;
	    case LispArray_t:
		if (LISTP(object))
		    result = VECTOR(object);
		else
		    goto coerce_fail;
		break;
	    case LispCons_t:
		if (ARRAYP(object) && object->data.array.rank == 1)
		    result = object->data.array.list;
		else
		    goto coerce_fail;
		break;
	    case LispPathname_t:
		result = APPLY1(Oparse_namestring, object);
		break;
	    default:
		goto coerce_fail;
	}
    }
    else
	result = object;

    return (result);

coerce_fail:
    LispDestroy("%s: cannot convert %s to %s",
		STRFUN(builtin), STROBJ(object), ATOMID(result_type));
    /* NOTREACHED */
    return (NIL);
}

static LispObj *
LispReallyDo(LispBuiltin *builtin, int refs)
/*
 do init test &rest body
 do* init test &rest body
 */
{
    GC_ENTER();
    int stack, lex, head;
    LispObj *list, *symbol, *value, *values, *cons;

    LispObj *init, *test, *body;

    body = ARGUMENT(2);
    test = ARGUMENT(1);
    init = ARGUMENT(0);

    if (!CONSP(test))
	LispDestroy("%s: end test condition must be a list, not %s",
		    STRFUN(builtin), STROBJ(init));

    CHECK_LIST(init);

    /* Save state */
    stack = lisp__data.stack.length;
    lex = lisp__data.env.lex;
    head = lisp__data.env.length;

    values = cons = NIL;
    for (list = init; CONSP(list); list = CDR(list)) {
	symbol = CAR(list);
	if (!SYMBOLP(symbol)) {
	    CHECK_CONS(symbol);
	    value = CDR(symbol);
	    symbol = CAR(symbol);
	    CHECK_SYMBOL(symbol);
	    CHECK_CONS(value);
	    value = EVAL(CAR(value));
	}
	else
	    value = NIL;

	CHECK_CONSTANT(symbol);

	LispAddVar(symbol, value);

	/* Bind variable now */
	if (refs) {
	    ++lisp__data.env.head;
	}
	else {
	    if (values == NIL) {
		values = cons = CONS(NIL, NIL);
		GC_PROTECT(values);
	    }
	    else {
		RPLACD(cons, CONS(NIL, NIL));
		cons = CDR(cons);
	    }
	}
    }
    if (!refs)
	lisp__data.env.head = lisp__data.env.length;

    for (;;) {
	if (EVAL(CAR(test)) != NIL)
	    break;

	/* TODO Run this code in an implicit tagbody */
	for (list = body; CONSP(list); list = CDR(list))
	    (void)EVAL(CAR(list));

	/* Error checking already done in the initialization */
	for (list = init, cons = values; CONSP(list); list = CDR(list)) {
	    symbol = CAR(list);
	    if (CONSP(symbol)) {
		value = CDDR(symbol);
		symbol = CAR(symbol);
		if (CONSP(value))
		    value = EVAL(CAR(value));
		else
		    value = NIL;
	    }
	    else
		value = NIL;

	    if (refs)
		LispSetVar(symbol, value);
	    else {
		RPLACA(cons, value);
		cons = CDR(cons);
	    }
	}
	if (!refs) {
	    for (list = init, cons = values;
		 CONSP(list);
		 list = CDR(list), cons = CDR(cons)) {
		symbol = CAR(list);
		if (CONSP(symbol)) {
		    if (CONSP(CDR(symbol)))
			LispSetVar(CAR(symbol), CAR(cons));
		}
	    }
	}
    }

    if (CONSP(CDR(test)))
	value = EVAL(CADR(test));
    else
	value = NIL;

    /* Restore state */
    lisp__data.stack.length = stack;
    lisp__data.env.lex = lex;
    lisp__data.env.head = lisp__data.env.length = head;
    GC_LEAVE();

    return (value);
}

LispObj *
LispDo(LispBuiltin *builtin, int refs)
/*
 do init test &rest body
 do* init test &rest body
 */
{
    int jumped;
    LispObj *result;
    LispBlock *block;

    jumped = 1;
    result = NIL;
    block = LispBeginBlock(NIL, LispBlockTag);
    if (setjmp(block->jmp) == 0) {
	result = LispReallyDo(builtin, refs);
	jumped = 0;
    }
    LispEndBlock(block);
    if (jumped)
	result = lisp__data.block.block_ret;

    return (result);
}

static LispObj *
LispReallyDoListTimes(LispBuiltin *builtin, int times)
/*
 dolist init &rest body
 dotimes init &rest body
 */
{
    GC_ENTER();
    int head = lisp__data.env.length;
    long count = 0, end = 0;
    LispObj *symbol, *value = NIL, *result = NIL, *init, *body, *object;

    body = ARGUMENT(1);
    init = ARGUMENT(0);

    /* Parse arguments */
    CHECK_CONS(init);
    symbol = CAR(init);
    CHECK_SYMBOL(symbol);
    init = CDR(init);

    if (init == NIL) {
	if (times)
	    LispDestroy("%s: NIL is not a number", STRFUN(builtin));
    }
    else {
	CHECK_CONS(init);
	value = CAR(init);
	init = CDR(init);
	if (init != NIL) {
	    CHECK_CONS(init);
	    result = CAR(init);
	}

	value = EVAL(value);

	if (times) {
	    CHECK_INDEX(value);
	    end = FIXNUM_VALUE(value);
	}
	else {
	    CHECK_LIST(value);
	    /* Protect iteration control from gc */
	    GC_PROTECT(value);
	}
    }

    /* The variable is only bound inside the loop, so it is safe to optimize
     * it out if there is no code to execute. But the result form may reference
     * the bound variable. */
    if (!CONSP(body)) {
	if (times)
	    count = end;
	else
	    value = NIL;
    }

    /* Initialize counter */
    CHECK_CONSTANT(symbol);
    if (times)
	LispAddVar(symbol, FIXNUM(count));
    else
	LispAddVar(symbol, CONSP(value) ? CAR(value) : value);
    ++lisp__data.env.head;

    if (!CONSP(body) || (times && count >= end) || (!times && !CONSP(value)))
	goto loop_done;

    /* Execute iterations */
    for (;;) {
	for (object = body; CONSP(object); object = CDR(object))
	    (void)EVAL(CAR(object));

	/* Update symbols and check exit condition */
	if (times) {
	    ++count;
	    LispSetVar(symbol, FIXNUM(count));
	    if (count >= end)
		break;
	}
	else {
	    value = CDR(value);
	    if (!CONSP(value)) {
		LispSetVar(symbol, NIL);
		break;
	    }
	    LispSetVar(symbol, CAR(value));
	}
    }

loop_done:
    result = EVAL(result);
    lisp__data.env.head = lisp__data.env.length = head;
    GC_LEAVE();

    return (result);
}

LispObj *
LispDoListTimes(LispBuiltin *builtin, int times)
/*
 dolist init &rest body
 dotimes init &rest body
 */
{
    int did_jump, *pdid_jump = &did_jump;
    LispObj *result, **presult = &result;
    LispBlock *block;

    *presult = NIL;
    *pdid_jump = 1;
    block = LispBeginBlock(NIL, LispBlockTag);
    if (setjmp(block->jmp) == 0) {
	result = LispReallyDoListTimes(builtin, times);
	did_jump = 0;
    }
    LispEndBlock(block);
    if (did_jump)
	result = lisp__data.block.block_ret;

    return (result);
}

LispObj *
LispLoadFile(LispObj *filename, int verbose, int print, int ifdoesnotexist)
{
    LispObj *stream, *cod, *obj, *result;
    int ch;

    LispObj *savepackage;
    LispPackage *savepack;

    if (verbose)
	LispMessage("; Loading %s", THESTR(filename));

    if (ifdoesnotexist) {
	GC_ENTER();
	result = CONS(filename, CONS(Kif_does_not_exist, CONS(Kerror, NIL)));
	GC_PROTECT(result);
	stream = APPLY(Oopen, result);
	GC_LEAVE();
    }
    else
	stream = APPLY1(Oopen, filename);

    if (stream == NIL)
	return (NIL);

    result = NIL;
    LispPushInput(stream);
    ch = LispGet();
    if (ch != '#')
	LispUnget(ch);
    else if ((ch = LispGet()) == '!') {
	for (;;) {
	    ch = LispGet();
	    if (ch == '\n' || ch == EOF)
		break;
	}
    }
    else {
	LispUnget(ch);
	LispUnget('#');
    }

    /* Save package environment */
    savepackage = PACKAGE;
    savepack = lisp__data.pack;

    cod = COD;

    /*CONSTCOND*/
    while (1) {
	if ((obj = LispRead()) != NULL) {
	    result = EVAL(obj);
	    COD = cod;
	    if (print) {
		int i;

		if (RETURN_COUNT >= 0)
		    LispPrint(result, NIL, 1);
		for (i = 0; i < RETURN_COUNT; i++)
		    LispPrint(RETURN(i), NIL, 1);
	    }
	}
	if (lisp__data.eof)
	    break;
    }
    LispPopInput(stream);

    /* Restore package environment */
    PACKAGE = savepackage;
    lisp__data.pack = savepack;

    APPLY1(Oclose, stream);

    return (T);
}

void
LispGetStringArgs(LispBuiltin *builtin,
		  char **string1, char **string2,
		  long *start1, long *end1, long *start2, long *end2)
{
    long length1, length2;
    LispObj *ostring1, *ostring2, *ostart1, *oend1, *ostart2, *oend2;

    oend2 = ARGUMENT(5);
    ostart2 = ARGUMENT(4);
    oend1 = ARGUMENT(3);
    ostart1 = ARGUMENT(2);
    ostring2 = ARGUMENT(1);
    ostring1 = ARGUMENT(0);

    CHECK_STRING(ostring1);
    *string1 = THESTR(ostring1);
    length1 = STRLEN(ostring1);

    CHECK_STRING(ostring2);
    *string2 = THESTR(ostring2);
    length2 = STRLEN(ostring2);

    if (ostart1 == UNSPEC)
	*start1 = 0;
    else {
	CHECK_INDEX(ostart1);
	*start1 = FIXNUM_VALUE(ostart1);
    }
    if (oend1 == UNSPEC)
	*end1 = length1;
    else {
	CHECK_INDEX(oend1);
	*end1 = FIXNUM_VALUE(oend1);
    }

    if (ostart2 == UNSPEC)
	*start2 = 0;
    else {
	CHECK_INDEX(ostart2);
	*start2 = FIXNUM_VALUE(ostart2);
    }

    if (oend2 == UNSPEC)
	*end2 = length2;
    else {
	CHECK_INDEX(oend2);
	*end2 = FIXNUM_VALUE(oend2);
    }

    if (*start1 > *end1)
	LispDestroy("%s: :START1 %ld larger than :END1 %ld",
		    STRFUN(builtin), *start1, *end1);
    if (*start2 > *end2)
	LispDestroy("%s: :START2 %ld larger than :END2 %ld",
		    STRFUN(builtin), *start2, *end2);
    if (*end1 > length1)
	LispDestroy("%s: :END1 %ld larger than string length %ld",
		    STRFUN(builtin), *end1, length1);
    if (*end2 > length2)
	LispDestroy("%s: :END2 %ld larger than string length %ld",
		    STRFUN(builtin), *end2, length2);
}

LispObj *
LispPathnameField(int field, int string)
{
    int offset = field;
    LispObj *pathname, *result, *object;

    pathname = ARGUMENT(0);

    if (!PATHNAMEP(pathname))
	pathname = APPLY1(Oparse_namestring, pathname);

    result = pathname->data.pathname;
    while (offset) {
	result = CDR(result);
	--offset;
    }
    object = result;
    result = CAR(result);

    if (string) {
	if (!STRINGP(result)) {
	    if (result == NIL)
		result = STRING("");
	    else if (field == PATH_DIRECTORY) {
		char *name = THESTR(CAR(pathname->data.pathname)), *ptr;

		ptr = strrchr(name, PATH_SEP);
		if (ptr) {
		    int length = ptr - name + 1;
		    char data[PATH_MAX];

		    if (length > PATH_MAX - 1)
			length = PATH_MAX - 1;
		    strncpy(data, name, length);
		    data[length] = '\0';
		    result = STRING(data);
		}
		else
		    result = STRING("");
	    }
	    else
		result = Kunspecific;
	}
	else if (field == PATH_NAME) {
	    object = CAR(CDR(object));
	    if (STRINGP(object)) {
		int length;
		char name[PATH_MAX + 1];

		strcpy(name, THESTR(result));
		length = STRLEN(result);
		if (length + 1 < sizeof(name)) {
		    name[length++] = PATH_TYPESEP;
		    name[length] = '\0';
		}
		if (STRLEN(object) + length < sizeof(name))
		    strcpy(name + length, THESTR(object));
		/* else LispDestroy ... */
		result = STRING(name);
	    }
	}
    }

    return (result);
}

LispObj *
LispProbeFile(LispBuiltin *builtin, int probe)
{
    GC_ENTER();
    LispObj *result;
    char *name = NULL, resolved[PATH_MAX + 1];
    struct stat st;

    LispObj *pathname;

    pathname = ARGUMENT(0);

    if (!POINTERP(pathname))
	goto bad_pathname;

    if (XSTRINGP(pathname))
	name = THESTR(pathname);
    else if (XPATHNAMEP(pathname))
	name = THESTR(CAR(pathname->data.pathname));
    else if (STREAMP(pathname) && pathname->data.stream.type == LispStreamFile)
	name = THESTR(CAR(pathname->data.stream.pathname->data.pathname));

#ifndef __UNIXOS2__
    if (realpath(name, &resolved[0]) == NULL ||
	stat(resolved, &st)) {
#else
    if ((name == NULL) || stat(resolved, &st)) {
#endif
	if (probe)
	    return (NIL);
	LispDestroy("%s: realpath(\"%s\"): %s",
		    STRFUN(builtin), name, strerror(errno));
    }

    if (S_ISDIR(st.st_mode)) {
	int length = strlen(resolved);

	if (!length || resolved[length - 1] != PATH_SEP) {
	    resolved[length++] = PATH_SEP;
	    resolved[length] = '\0';
	}
    }

    result = STRING(resolved);
    GC_PROTECT(result);
    result = APPLY1(Oparse_namestring, result);
    GC_LEAVE();

    return (result);

bad_pathname:
    LispDestroy("%s: bad pathname %s", STRFUN(builtin), STROBJ(pathname));
    /*NOTREACHED*/
    return (NIL);
}

LispObj *
LispWriteString_(LispBuiltin *builtin, int newline)
/*
 write-line string &optional output-stream &key start end
 write-string string &optional output-stream &key start end
 */
{
    char *text;
    long start, end, length;

    LispObj *string, *output_stream, *ostart, *oend;

    oend = ARGUMENT(3);
    ostart = ARGUMENT(2);
    output_stream = ARGUMENT(1);
    string = ARGUMENT(0);

    CHECK_STRING(string);
    LispCheckSequenceStartEnd(builtin, string, ostart, oend,
			      &start, &end, &length);
    if (output_stream == UNSPEC)
	output_stream = NIL;
    text = THESTR(string);
    if (end > start)
	LispWriteStr(output_stream, text + start, end - start);
    if (newline)
	LispWriteChar(output_stream, '\n');

    return (string);
}
