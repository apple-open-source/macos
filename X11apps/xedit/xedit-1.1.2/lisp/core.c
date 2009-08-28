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

/* $XFree86: xc/programs/xedit/lisp/core.c,v 1.71tsi Exp $ */

#include "lisp/io.h"
#include "lisp/core.h"
#include "lisp/format.h"
#include "lisp/helper.h"
#include "lisp/package.h"
#include "lisp/private.h"
#include "lisp/write.h"

/*
 * Types
 */
typedef struct _SeqInfo {
    LispType type;
    union {
	LispObj *list;
	LispObj **vector;
	unsigned char *string;
    } data;
} SeqInfo;

#define SETSEQ(seq, object)						\
    switch (seq.type = XOBJECT_TYPE(object)) {				\
	case LispString_t:						\
	    seq.data.string = (unsigned char*)THESTR(object);		\
	    break;							\
	case LispCons_t:						\
	    seq.data.list = object;					\
	    break;							\
	default:							\
	    seq.data.list = object->data.array.list;			\
	    break;							\
    }

#ifdef __UNIXOS2__
# define finite(x) isfinite(x)
#endif

#ifdef NEED_SETENV
extern int setenv(const char *name, const char *value, int overwrite);
extern void unsetenv(const char *name);
#endif

/*
 * Prototypes
 */
#define NONE		0

#define	REMOVE		1
#define	SUBSTITUTE	2
#define DELETE		3
#define	NSUBSTITUTE	4

#define ASSOC		1
#define MEMBER		2

#define FIND		1
#define POSITION	2

#define	IF		1
#define	IFNOT		2

#define UNION		1
#define INTERSECTION	2
#define SETDIFFERENCE	3
#define SETEXCLUSIVEOR	4
#define SUBSETP		5
#define NSETDIFFERENCE	6
#define NINTERSECTION	7
#define NUNION		8
#define NSETEXCLUSIVEOR	9

#define COPY_LIST	1
#define COPY_ALIST	2
#define COPY_TREE	3

#define EVERY		1
#define SOME		2
#define NOTEVERY	3
#define NOTANY		4

/* Call directly LispObjectCompare() if possible */
#define FCODE(predicate)					\
    predicate == Oeql ? FEQL :					\
	predicate == Oequal ? FEQUAL :				\
	    predicate == Oeq ? FEQ :				\
		predicate == Oequalp ? FEQUALP : 0
#define FCOMPARE(predicate, left, right, code)			\
    code == FEQ ? left == right :				\
	code ? LispObjectCompare(left, right, code) != NIL :	\
	       APPLY2(predicate, left, right) != NIL

#define FUNCTION_CHECK(predicate)				\
    if (FUNCTIONP(predicate))					\
	predicate = (predicate)->data.atom->object

#define CHECK_TEST_0()						\
    if (test != UNSPEC && test_not != UNSPEC)			\
	LispDestroy("%s: specify either :TEST or :TEST-NOT",	\
		    STRFUN(builtin))

#define CHECK_TEST()						\
    CHECK_TEST_0();						\
    if (test_not == UNSPEC) {					\
	if (test == UNSPEC)					\
	    lambda = Oeql;					\
	else							\
	    lambda = test;					\
	expect = 1;						\
    }								\
    else {							\
	lambda = test_not;					\
	expect = 0;						\
    }								\
    FUNCTION_CHECK(lambda);					\
    code = FCODE(lambda)


static LispObj *LispAdjoin(LispBuiltin*,
			   LispObj*, LispObj*, LispObj*, LispObj*, LispObj*);
static LispObj *LispAssocOrMember(LispBuiltin*, int, int);
static LispObj *LispEverySomeAnyNot(LispBuiltin*, int);
static LispObj *LispFindOrPosition(LispBuiltin*, int, int);
static LispObj *LispDeleteOrRemoveDuplicates(LispBuiltin*, int);
static LispObj *LispDeleteRemoveXSubstitute(LispBuiltin*, int, int);
static LispObj *LispListSet(LispBuiltin*, int);
static LispObj *LispMapc(LispBuiltin*, int);
static LispObj *LispMapl(LispBuiltin*, int);
static LispObj *LispMapnconc(LispObj*);
extern LispObj *LispRunSetf(LispArgList*, LispObj*, LispObj*, LispObj*);
extern LispObj *LispRunSetfMacro(LispAtom*, LispObj*, LispObj*);
static LispObj *LispMergeSort(LispObj*, LispObj*, LispObj*, int);
static LispObj *LispXReverse(LispBuiltin*, int);
static LispObj *LispCopyList(LispBuiltin*, LispObj*, int);
static LispObj *LispValuesList(LispBuiltin*, int);
static LispObj *LispTreeEqual(LispObj*, LispObj*, LispObj*, int);
static LispDocType_t LispDocumentationType(LispBuiltin*, LispObj*);

extern void LispSetAtomObjectProperty(LispAtom*, LispObj*);

/*
 * Initialization
 */
LispObj *Oeq, *Oeql, *Oequal, *Oequalp, *Omake_array,
	*Kinitial_contents, *Osetf, *Ootherwise, *Oquote;
LispObj *Ogensym_counter;

Atom_id Svariable, Sstructure, Stype, Ssetf;

/*
 * Implementation
 */
void
LispCoreInit(void)
{
    Oeq			= STATIC_ATOM("EQ");
    Oeql		= STATIC_ATOM("EQL");
    Oequal		= STATIC_ATOM("EQUAL");
    Oequalp		= STATIC_ATOM("EQUALP");
    Omake_array		= STATIC_ATOM("MAKE-ARRAY");
    Kinitial_contents	= KEYWORD("INITIAL-CONTENTS");
    Osetf		= STATIC_ATOM("SETF");
    Ootherwise		= STATIC_ATOM("OTHERWISE");
    LispExportSymbol(Ootherwise);
    Oquote		= STATIC_ATOM("QUOTE");
    LispExportSymbol(Oquote);

    Svariable		= GETATOMID("VARIABLE");
    Sstructure		= GETATOMID("STRUCTURE");
    Stype		= GETATOMID("TYPE");

    /* Create as a constant so that only the C code should change the value */
    Ogensym_counter	= STATIC_ATOM("*GENSYM-COUNTER*");
    LispDefconstant(Ogensym_counter, FIXNUM(0), NIL);
    LispExportSymbol(Ogensym_counter);

    Ssetf	= ATOMID(Osetf);
}

LispObj *
Lisp_Acons(LispBuiltin *builtin)
/*
 acons key datum alist
 */
{
    LispObj *key, *datum, *alist;

    alist = ARGUMENT(2);
    datum = ARGUMENT(1);
    key = ARGUMENT(0);

    return (CONS(CONS(key, datum), alist));
}

static LispObj *
LispAdjoin(LispBuiltin*builtin, LispObj *item, LispObj *list,
	   LispObj *key, LispObj *test, LispObj *test_not)
{
    GC_ENTER();
    int code, expect, value;
    LispObj *lambda, *compare, *object;

    CHECK_LIST(list);
    CHECK_TEST();

    if (key != UNSPEC) {
	item = APPLY1(key, item);
	/* Result is not guaranteed to be gc protected */
	GC_PROTECT(item);
    }

    /* Check if item is not already in place */
    for (object = list; CONSP(object); object = CDR(object)) {
	compare = CAR(object);
	if (key != UNSPEC) {
	    compare = APPLY1(key, compare);
	    GC_PROTECT(compare);
	    value = FCOMPARE(lambda, item, compare, code);
	    /* Unprotect compare... */
	    --lisp__data.protect.length;
	}
	else
	    value = FCOMPARE(lambda, item, compare, code);

	if (value == expect) {
	    /* Item is already in list */
	    GC_LEAVE();

	    return (list);
	}
    }
    GC_LEAVE();

    return (CONS(item, list));
}

LispObj *
Lisp_Adjoin(LispBuiltin *builtin)
/*
 adjoin item list &key key test test-not
 */
{
    LispObj *item, *list, *key, *test, *test_not;

    test_not = ARGUMENT(4);
    test = ARGUMENT(3);
    key = ARGUMENT(2);
    list = ARGUMENT(1);
    item = ARGUMENT(0);

    return (LispAdjoin(builtin, item, list, key, test, test_not));
}

LispObj *
Lisp_Append(LispBuiltin *builtin)
/*
 append &rest lists
 */
{
    GC_ENTER();
    LispObj *result, *cons, *list;

    LispObj *lists;

    lists = ARGUMENT(0);

    /* no arguments */
    if (!CONSP(lists))
	return (NIL);

    /* skip initial nil lists */
    for (; CONSP(CDR(lists)) && CAR(lists) == NIL; lists = CDR(lists))
	;

    /* last argument is not copied (even if it is the single argument) */
    if (!CONSP(CDR(lists)))
	return (CAR(lists));

    /* make sure result is a list */
    list = CAR(lists);
    CHECK_CONS(list);
    result = cons = CONS(CAR(list), NIL);
    GC_PROTECT(result);
    for (list = CDR(list); CONSP(list); list = CDR(list)) {
	RPLACD(cons, CONS(CAR(list), NIL));
	cons = CDR(cons);
    }
    lists = CDR(lists);

    /* copy intermediate lists */
    for (; CONSP(CDR(lists)); lists = CDR(lists)) {
	list = CAR(lists);
	if (list == NIL)
	    continue;
	/* intermediate elements must be lists */
	CHECK_CONS(list);
	for (; CONSP(list); list = CDR(list)) {
	    RPLACD(cons, CONS(CAR(list), NIL));
	    cons = CDR(cons);
	}
    }

    /* add last element */
    RPLACD(cons, CAR(lists));

    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_Aref(LispBuiltin *builtin)
/*
 aref array &rest subscripts
 */
{
    long c, count, idx, seq;
    LispObj *obj, *dim;

    LispObj *array, *subscripts;

    subscripts = ARGUMENT(1);
    array = ARGUMENT(0);

    /* accept strings also */
    if (STRINGP(array) && CONSP(subscripts) && CDR(subscripts) == NIL) {
	long offset, length = STRLEN(array);

	CHECK_INDEX(CAR(subscripts));
	offset = FIXNUM_VALUE(CAR(subscripts));

	if (offset >= length)
	    LispDestroy("%s: index %ld too large for sequence length %ld",
			STRFUN(builtin), offset, length);

	return (SCHAR(THESTR(array)[offset]));
    }

    CHECK_ARRAY(array);

    for (count = 0, dim = subscripts, obj = array->data.array.dim; CONSP(dim);
	 count++, dim = CDR(dim), obj = CDR(obj)) {
	if (count >= array->data.array.rank)
	    LispDestroy("%s: too many subscripts %s",
			STRFUN(builtin), STROBJ(subscripts));
	if (!INDEXP(CAR(dim)) ||
	    FIXNUM_VALUE(CAR(dim)) >= FIXNUM_VALUE(CAR(obj)))
	    LispDestroy("%s: %s is out of range or a bad index",
			STRFUN(builtin), STROBJ(CAR(dim)));
    }
    if (count < array->data.array.rank)
	LispDestroy("%s: too few subscripts %s",
		    STRFUN(builtin), STROBJ(subscripts));

    for (count = seq = 0, dim = subscripts; CONSP(dim); dim = CDR(dim), seq++) {
	for (idx = 0, obj = array->data.array.dim; idx < seq;
	     obj = CDR(obj), ++idx)
	    ;
	for (c = 1, obj = CDR(obj); obj != NIL; obj = CDR(obj))
	    c *= FIXNUM_VALUE(CAR(obj));
	count += c * FIXNUM_VALUE(CAR(dim));
    }

    for (array = array->data.array.list; count > 0; array = CDR(array), count--)
	;

    return (CAR(array));
}

static LispObj *
LispAssocOrMember(LispBuiltin *builtin, int function, int comparison)
/*
 assoc item list &key test test-not key
 assoc-if predicate list &key key
 assoc-if-not predicate list &key key
 member item list &key test test-not key
 member-if predicate list &key key
 member-if-not predicate list &key key
 */
{
    int code = 0, expect, value;
    LispObj *lambda, *result, *compare;

    LispObj *item, *list, *test, *test_not, *key;

    if (comparison == NONE) {
	key = ARGUMENT(4);
	test_not = ARGUMENT(3);
	test = ARGUMENT(2);
	list = ARGUMENT(1);
	item = ARGUMENT(0);
	lambda = NIL;
    }
    else {
	key = ARGUMENT(2);
	list = ARGUMENT(1);
	lambda = ARGUMENT(0);
	test = test_not = UNSPEC;
	item = NIL;
    }

    if (list == NIL)
	return (NIL);
    CHECK_CONS(list);

    /* Resolve compare function, and expected result of comparison */
    if (comparison == NONE) {
	CHECK_TEST();
    }
    else
	expect = comparison == IFNOT ? 0 : 1;

    result = NIL;
    for (; CONSP(list); list = CDR(list)) {
	compare = CAR(list);
	if (function == ASSOC) {
	    if (!CONSP(compare))
		continue;
	    compare = CAR(compare);
	}
	if (key != UNSPEC)
	    compare = APPLY1(key, compare);

	if (comparison == NONE)
	    value = FCOMPARE(lambda, item, compare, code);
	else
	    value = APPLY1(lambda, compare) != NIL;
	if (value == expect) {
	    result = list;
	    if (function == ASSOC)
		result = CAR(result);
	    break;
	}
    }
    if (function == MEMBER) {
	CHECK_LIST(list);
    }

    return (result);
}

LispObj *
Lisp_Assoc(LispBuiltin *builtin)
/*
 assoc item list &key test test-not key
 */
{
    return (LispAssocOrMember(builtin, ASSOC, NONE));
}

LispObj *
Lisp_AssocIf(LispBuiltin *builtin)
/*
 assoc-if predicate list &key key
 */
{
    return (LispAssocOrMember(builtin, ASSOC, IF));
}

LispObj *
Lisp_AssocIfNot(LispBuiltin *builtin)
/*
 assoc-if-not predicate list &key key
 */
{
    return (LispAssocOrMember(builtin, ASSOC, IFNOT));
}

LispObj *
Lisp_And(LispBuiltin *builtin)
/*
 and &rest args
 */
{
    LispObj *result = T, *args;

    args = ARGUMENT(0);

    for (; CONSP(args); args = CDR(args)) {
	result = EVAL(CAR(args));
	if (result == NIL)
	    break;
    }

    return (result);
}

LispObj *
Lisp_Apply(LispBuiltin *builtin)
/*
 apply function arg &rest more-args
 */
{
    GC_ENTER();
    LispObj *result, *arguments;

    LispObj *function, *arg, *more_args;

    more_args = ARGUMENT(2);
    arg = ARGUMENT(1);
    function = ARGUMENT(0);

    if (more_args == NIL) {
	CHECK_LIST(arg);
	arguments = arg;
	for (; CONSP(arg); arg = CDR(arg))
	    ;
	CHECK_LIST(arg);
    }
    else {
	LispObj *cons;

	CHECK_CONS(more_args);
	arguments = cons = CONS(arg, NIL);
	GC_PROTECT(arguments);
	for (arg = CDR(more_args);
	     CONSP(arg);
	     more_args = arg, arg = CDR(arg)) {
	    RPLACD(cons, CONS(CAR(more_args), NIL));
	    cons = CDR(cons);
	}
	more_args = CAR(more_args);
	if (more_args != NIL) {
	    for (arg = more_args; CONSP(arg); arg = CDR(arg))
		;
	    CHECK_LIST(arg);
	    RPLACD(cons, more_args);
	}
    }

    result = APPLY(function, arguments);
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_Atom(LispBuiltin *builtin)
/*
 atom object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (CONSP(object) ? NIL : T);
}

LispObj *
Lisp_Block(LispBuiltin *builtin)
/*
 block name &rest body
 */
{
    int did_jump, *pdid_jump = &did_jump;
    LispObj *res, **pres = &res;
    LispBlock *block;

    LispObj *name, *body;

    body = ARGUMENT(1);
    name = ARGUMENT(0);

    if (!SYMBOLP(name) && name != NIL && name != T)
	LispDestroy("%s: %s cannot name a block",
		    STRFUN(builtin), STROBJ(name));

    *pres = NIL;
    *pdid_jump = 1;
    block = LispBeginBlock(name, LispBlockTag);
    if (setjmp(block->jmp) == 0) {
	for (; CONSP(body); body = CDR(body))
	    res = EVAL(CAR(body));
	*pdid_jump = 0;
    }
    LispEndBlock(block);
    if (*pdid_jump)
	*pres = lisp__data.block.block_ret;

    return (res);
}

LispObj *
Lisp_Boundp(LispBuiltin *builtin)
/*
 boundp symbol
 */
{
    LispAtom *atom;

    LispObj *symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);

    atom = symbol->data.atom;
    if (atom->package == lisp__data.keyword ||
	(atom->a_object && atom->property->value != UNBOUND))
	return (T);

    return (NIL);
}

LispObj *
Lisp_Butlast(LispBuiltin *builtin)
/*
 butlast list &optional count
 */
{
    GC_ENTER();
    long length, count;
    LispObj *result, *cons, *list, *ocount;

    ocount = ARGUMENT(1);
    list = ARGUMENT(0);

    CHECK_LIST(list);
    if (ocount == UNSPEC)
	count = 1;
    else {
	CHECK_INDEX(ocount);
	count = FIXNUM_VALUE(ocount);
    }
    length = LispLength(list);

    if (count == 0)
	return (list);
    else if (count >= length)
	return (NIL);

    length -= count + 1;
    result = cons = CONS(CAR(list), NIL);
    GC_PROTECT(result);
    for (list = CDR(list); length > 0; list = CDR(list), length--) {
	RPLACD(cons, CONS(CAR(list), NIL));
	cons = CDR(cons);
    }
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_Nbutlast(LispBuiltin *builtin)
/*
 nbutlast list &optional count
 */
{
    long length, count;
    LispObj *result, *list, *ocount;

    ocount = ARGUMENT(1);
    list = ARGUMENT(0);

    CHECK_LIST(list);
    if (ocount == UNSPEC)
	count = 1;
    else {
	CHECK_INDEX(ocount);
	count = FIXNUM_VALUE(ocount);
    }
    length = LispLength(list);

    if (count == 0)
	return (list);
    else if (count >= length)
	return (NIL);

    length -= count + 1;
    result = list;
    for (; length > 0; list = CDR(list), length--)
	;
    RPLACD(list, NIL);

    return (result);
}

LispObj *
Lisp_Car(LispBuiltin *builtin)
/*
 car list
 */
{
    LispObj *list, *result = NULL;

    list = ARGUMENT(0);

    if (list == NIL)
	result = NIL;
    else {
	CHECK_CONS(list);
	result = CAR(list);
    }

    return (result);
}

LispObj *
Lisp_Case(LispBuiltin *builtin)
/*
 case keyform &rest body
 */
{
    LispObj *result, *code, *keyform, *body, *form;

    body = ARGUMENT(1);
    keyform = ARGUMENT(0);

    result = NIL;
    keyform = EVAL(keyform);

    for (; CONSP(body); body = CDR(body)) {
	code = CAR(body);
	CHECK_CONS(code);

	form = CAR(code);
	if (form == T || form == Ootherwise) {
	    if (CONSP(CDR(body)))
		LispDestroy("%s: %s must be the last clause",
			    STRFUN(builtin), STROBJ(CAR(code)));
	    result = CDR(code);
	    break;
	}
	else if (CONSP(form)) {
	    for (; CONSP(form); form = CDR(form))
		if (XEQL(keyform, CAR(form)) == T) {
		    result = CDR(code);
		    break;
		}
	    if (CONSP(form))	/* if found match */
		break;
	}
	else if (XEQL(keyform, form) == T) {
	    result = CDR(code);
	    break;
	}
    }

    for (body = result; CONSP(body); body = CDR(body))
	result = EVAL(CAR(body));

    return (result);
}

LispObj *
Lisp_Catch(LispBuiltin *builtin)
/*
 catch tag &rest body
 */
{
    int did_jump, *pdid_jump = &did_jump;
    LispObj *res, **pres = &res;
    LispBlock *block;

    LispObj *tag, *body;

    body = ARGUMENT(1);
    tag = ARGUMENT(0);

    *pres = NIL;
    *pdid_jump = 1;
    block = LispBeginBlock(tag, LispBlockCatch);
    if (setjmp(block->jmp) == 0) {
	for (; CONSP(body); body = CDR(body))
	    res = EVAL(CAR(body));
	*pdid_jump = 0;
    }
    LispEndBlock(block);
    if (*pdid_jump)
	*pres = lisp__data.block.block_ret;

    return (res);
}

LispObj *
Lisp_Coerce(LispBuiltin *builtin)
/*
 coerce object result-type
 */
{
    LispObj *object, *result_type;

    result_type = ARGUMENT(1);
    object = ARGUMENT(0);

    return (LispCoerce(builtin, object, result_type));
}

LispObj *
Lisp_Cdr(LispBuiltin *builtin)
/*
 cdr list
 */
{
    LispObj *list, *result = NULL;

    list = ARGUMENT(0);

    if (list == NIL)
	result = NIL;
    else {
	CHECK_CONS(list);
	result = CDR(list);
    }

    return (result);
}

LispObj *
Lisp_C_r(LispBuiltin *builtin)
/*
 c[ad]{2,4}r list
 */
{
    char *desc;

    LispObj *list, *result = NULL;

    list = ARGUMENT(0);

    result = list;
    desc = STRFUN(builtin);
    while (desc[1] != 'R')
	++desc;
    while (*desc != 'C') {
	if (result == NIL)
	    break;
	CHECK_CONS(result);
	result = *desc == 'A' ? CAR(result) : CDR(result);
	--desc;
    }

    return (result);
}

LispObj *
Lisp_Cond(LispBuiltin *builtin)
/*
 cond &rest body
 */
{
    LispObj *result, *code, *body;

    body = ARGUMENT(0);

    result = NIL;
    for (; CONSP(body); body = CDR(body)) {
	code = CAR(body);

	CHECK_CONS(code);
	result = EVAL(CAR(code));
	if (result == NIL)
	    continue;
	for (code = CDR(code); CONSP(code); code = CDR(code))
	    result = EVAL(CAR(code));
	break;
    }

    return (result);
}

static LispObj *
LispCopyList(LispBuiltin *builtin, LispObj *list, int function)
{
    GC_ENTER();
    LispObj *result, *cons;

    if (list == NIL)
	return (list);
    CHECK_CONS(list);

    result = cons = CONS(NIL, NIL);
    GC_PROTECT(result);
    if (CONSP(CAR(list))) {
	switch (function) {
	    case COPY_LIST:
		RPLACA(result, CAR(list));
		break;
	    case COPY_ALIST:
		RPLACA(result, CONS(CAR(CAR(list)), CDR(CAR(list))));
		break;
	    case COPY_TREE:
		RPLACA(result, LispCopyList(builtin, CAR(list), COPY_TREE));
		break;
	}
    }
    else
	RPLACA(result, CAR(list));

    for (list = CDR(list); CONSP(list); list = CDR(list)) {
	CDR(cons) = CONS(NIL, NIL);
	cons = CDR(cons);
	if (CONSP(CAR(list))) {
	    switch (function) {
		case COPY_LIST:
		    RPLACA(cons, CAR(list));
		    break;
		case COPY_ALIST:
		    RPLACA(cons, CONS(CAR(CAR(list)), CDR(CAR(list))));
		    break;
		case COPY_TREE:
		    RPLACA(cons, LispCopyList(builtin, CAR(list), COPY_TREE));
		    break;
	    }
	}
	else
	    RPLACA(cons, CAR(list));
    }
    /* in case list is dotted */
    RPLACD(cons, list);
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_CopyAlist(LispBuiltin *builtin)
/*
 copy-alist list
 */
{
    LispObj *list;

    list = ARGUMENT(0);

    return (LispCopyList(builtin, list, COPY_ALIST));
}

LispObj *
Lisp_CopyList(LispBuiltin *builtin)
/*
 copy-list list
 */
{
    LispObj *list;

    list = ARGUMENT(0);

    return (LispCopyList(builtin, list, COPY_LIST));
}

LispObj *
Lisp_CopyTree(LispBuiltin *builtin)
/*
 copy-tree list
 */
{
    LispObj *list;

    list = ARGUMENT(0);

    return (LispCopyList(builtin, list, COPY_TREE));
}

LispObj *
Lisp_Cons(LispBuiltin *builtin)
/*
 cons car cdr
 */
{
    LispObj *car, *cdr;

    cdr = ARGUMENT(1);
    car = ARGUMENT(0);

    return (CONS(car, cdr));
}

LispObj *
Lisp_Consp(LispBuiltin *builtin)
/*
 consp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (CONSP(object) ? T : NIL);
}

LispObj *
Lisp_Constantp(LispBuiltin *builtin)
/*
 constantp form &optional environment
 */
{
    LispObj *form;

    form = ARGUMENT(0);

    /* not all self-evaluating objects are considered constants */
    if (!POINTERP(form) ||
	NUMBERP(form) ||
	XQUOTEP(form) ||
	(XCONSP(form) && CAR(form) == Oquote) ||
	(XSYMBOLP(form) && form->data.atom->constant) ||
	XSTRINGP(form) ||
	XARRAYP(form))
	return (T);

    return (NIL);
}

LispObj *
Lisp_Defconstant(LispBuiltin *builtin)
/*
 defconstant name initial-value &optional documentation
 */
{
    LispObj *name, *initial_value, *documentation;

    documentation = ARGUMENT(2);
    initial_value = ARGUMENT(1);
    name = ARGUMENT(0);

    CHECK_SYMBOL(name);
    if (documentation != UNSPEC) {
	CHECK_STRING(documentation);
    }
    else
	documentation = NIL;
    LispDefconstant(name, EVAL(initial_value), documentation);

    return (name);
}

LispObj *
Lisp_Defmacro(LispBuiltin *builtin)
/*
 defmacro name lambda-list &rest body
 */
{
    LispArgList *alist;

    LispObj *lambda, *name, *lambda_list, *body;

    body = ARGUMENT(2);
    lambda_list = ARGUMENT(1);
    name = ARGUMENT(0);

    CHECK_SYMBOL(name);
    alist = LispCheckArguments(LispMacro, lambda_list, ATOMID(name)->value, 0);

    if (CONSP(body) && STRINGP(CAR(body))) {
	LispAddDocumentation(name, CAR(body), LispDocFunction);
	body = CDR(body);
    }

    lambda_list = LispListProtectedArguments(alist);
    lambda = LispNewLambda(name, body, lambda_list, LispMacro);

    if (name->data.atom->a_builtin || name->data.atom->a_compiled) {
	if (name->data.atom->a_builtin) {
	    ERROR_CHECK_SPECIAL_FORM(name->data.atom);
	}
	/* redefining these may cause surprises if bytecode
	 * compiled functions references them */
	LispWarning("%s: %s is being redefined", STRFUN(builtin),
		    ATOMID(name)->value);

	LispRemAtomBuiltinProperty(name->data.atom);
    }

    LispSetAtomFunctionProperty(name->data.atom, lambda, alist);
    LispUseArgList(alist);

    return (name);
}

LispObj *
Lisp_Defun(LispBuiltin *builtin)
/*
 defun name lambda-list &rest body
 */
{
    LispArgList *alist;

    LispObj *lambda, *name, *lambda_list, *body;

    body = ARGUMENT(2);
    lambda_list = ARGUMENT(1);
    name = ARGUMENT(0);

    CHECK_SYMBOL(name);
    alist = LispCheckArguments(LispFunction, lambda_list, ATOMID(name)->value, 0);

    if (CONSP(body) && STRINGP(CAR(body))) {
	LispAddDocumentation(name, CAR(body), LispDocFunction);
	body = CDR(body);
    }

    lambda_list = LispListProtectedArguments(alist);
    lambda = LispNewLambda(name, body, lambda_list, LispFunction);

    if (name->data.atom->a_builtin || name->data.atom->a_compiled) {
	if (name->data.atom->a_builtin) {
	    ERROR_CHECK_SPECIAL_FORM(name->data.atom);
	}
	/* redefining these may cause surprises if bytecode
	 * compiled functions references them */
	LispWarning("%s: %s is being redefined", STRFUN(builtin),
		    ATOMID(name)->value);

	LispRemAtomBuiltinProperty(name->data.atom);
    }
    LispSetAtomFunctionProperty(name->data.atom, lambda, alist);
    LispUseArgList(alist);

    return (name);
}

LispObj *
Lisp_Defsetf(LispBuiltin *builtin)
/*
 defsetf function lambda-list &rest body
 */
{
    LispArgList *alist;
    LispObj *obj;
    LispObj *lambda, *function, *lambda_list, *store, *body;

    body = ARGUMENT(2);
    lambda_list = ARGUMENT(1);
    function = ARGUMENT(0);

    CHECK_SYMBOL(function);

    if (body == NIL || (CONSP(body) && STRINGP(CAR(body)))) {
	if (!SYMBOLP(lambda_list))
	    LispDestroy("%s: syntax error %s %s",
			STRFUN(builtin), STROBJ(function), STROBJ(lambda_list));
	if (body != NIL)
	    LispAddDocumentation(function, CAR(body), LispDocSetf);

	LispSetAtomSetfProperty(function->data.atom, lambda_list, NULL);

	return (function);
    }

    alist = LispCheckArguments(LispSetf, lambda_list, ATOMID(function)->value, 0);

    store = CAR(body);
    if (!CONSP(store))
	LispDestroy("%s: %s is a bad store value",
		    STRFUN(builtin), STROBJ(store));
    for (obj = store; CONSP(obj); obj = CDR(obj)) {
	CHECK_SYMBOL(CAR(obj));
    }

    body = CDR(body);
    if (CONSP(body) && STRINGP(CAR(body))) {
	LispAddDocumentation(function, CAR(body), LispDocSetf);
	body = CDR(body);
    }

    lambda = LispNewLambda(function, body, store, LispSetf);
    LispSetAtomSetfProperty(function->data.atom, lambda, alist);
    LispUseArgList(alist);

    return (function);
}

LispObj *
Lisp_Defparameter(LispBuiltin *builtin)
/*
 defparameter name initial-value &optional documentation
 */
{
    LispObj *name, *initial_value, *documentation;

    documentation = ARGUMENT(2);
    initial_value = ARGUMENT(1);
    name = ARGUMENT(0);

    CHECK_SYMBOL(name);
    if (documentation != UNSPEC) {
	CHECK_STRING(documentation);
    }
    else
	documentation = NIL;

    LispProclaimSpecial(name, EVAL(initial_value), documentation);

    return (name);
}

LispObj *
Lisp_Defvar(LispBuiltin *builtin)
/*
 defvar name &optional initial-value documentation
 */
{
    LispObj *name, *initial_value, *documentation;

    documentation = ARGUMENT(2);
    initial_value = ARGUMENT(1);
    name = ARGUMENT(0);

    CHECK_SYMBOL(name);
    if (documentation != UNSPEC) {
	CHECK_STRING(documentation);
    }
    else
	documentation = NIL;

    LispProclaimSpecial(name,
			initial_value != UNSPEC ? EVAL(initial_value) : NULL,
			documentation);

    return (name);
}

LispObj *
Lisp_Delete(LispBuiltin *builtin)
/*
 delete item sequence &key from-end test test-not start end count key
 */
{
    return (LispDeleteRemoveXSubstitute(builtin, DELETE, NONE));
}

LispObj *
Lisp_DeleteIf(LispBuiltin *builtin)
/*
 delete-if predicate sequence &key from-end start end count key
 */
{
    return (LispDeleteRemoveXSubstitute(builtin, DELETE, IF));
}

LispObj *
Lisp_DeleteIfNot(LispBuiltin *builtin)
/*
 delete-if-not predicate sequence &key from-end start end count key
 */
{
    return (LispDeleteRemoveXSubstitute(builtin, DELETE, IFNOT));
}

LispObj *
Lisp_DeleteDuplicates(LispBuiltin *builtin)
/*
 delete-duplicates sequence &key from-end test test-not start end key
 */
{
    return (LispDeleteOrRemoveDuplicates(builtin, DELETE));
}

LispObj *
Lisp_Do(LispBuiltin *builtin)
/*
 do init test &rest body
 */
{
    return (LispDo(builtin, 0));
}

LispObj *
Lisp_DoP(LispBuiltin *builtin)
/*
 do* init test &rest body
 */
{
    return (LispDo(builtin, 1));
}

static LispDocType_t
LispDocumentationType(LispBuiltin *builtin, LispObj *type)
{
    Atom_id atom;
    LispDocType_t doc_type = LispDocVariable;

    CHECK_SYMBOL(type);
    atom = ATOMID(type);

    if (atom == Svariable)
	doc_type = LispDocVariable;
    else if (atom == Sfunction)
	doc_type = LispDocFunction;
    else if (atom == Sstructure)
	doc_type = LispDocStructure;
    else if (atom == Stype)
	doc_type = LispDocType;
    else if (atom == Ssetf)
	doc_type = LispDocSetf;
    else {
	LispDestroy("%s: unknown documentation type %s",
		    STRFUN(builtin), STROBJ(type));
	/*NOTREACHED*/
    }

    return (doc_type);
}

LispObj *
Lisp_Documentation(LispBuiltin *builtin)
/*
 documentation symbol type
 */
{
    LispObj *symbol, *type;

    type = ARGUMENT(1);
    symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);
    /* type is checked in LispDocumentationType() */

    return (LispGetDocumentation(symbol, LispDocumentationType(builtin, type)));
}

LispObj *
Lisp_DoList(LispBuiltin *builtin)
{
    return (LispDoListTimes(builtin, 0));
}

LispObj *
Lisp_DoTimes(LispBuiltin *builtin)
{
    return (LispDoListTimes(builtin, 1));
}

LispObj *
Lisp_Elt(LispBuiltin *builtin)
/*
 elt sequence index
 svref sequence index
 */
{
    long offset, length;
    LispObj *result, *sequence, *oindex;

    oindex = ARGUMENT(1);
    sequence = ARGUMENT(0);

    length = LispLength(sequence);

    CHECK_INDEX(oindex);
    offset = FIXNUM_VALUE(oindex);

    if (offset >= length)
	LispDestroy("%s: index %ld too large for sequence length %ld",
		    STRFUN(builtin), offset, length);

    if (STRINGP(sequence))
	result = SCHAR(THESTR(sequence)[offset]);
    else {
	if (ARRAYP(sequence))
	    sequence = sequence->data.array.list;

	for (; offset > 0; offset--, sequence = CDR(sequence))
	    ;
	result = CAR(sequence);
    }

    return (result);
}

LispObj *
Lisp_Endp(LispBuiltin *builtin)
/*
 endp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    if (object == NIL)
	return (T);
    CHECK_CONS(object);

    return (NIL);
}

LispObj *
Lisp_Eq(LispBuiltin *builtin)
/*
 eq left right
 */
{
    LispObj *left, *right;

    right = ARGUMENT(1);
    left = ARGUMENT(0);

    return (XEQ(left, right));
}

LispObj *
Lisp_Eql(LispBuiltin *builtin)
/*
 eql left right
 */
{
    LispObj *left, *right;

    right = ARGUMENT(1);
    left = ARGUMENT(0);

    return (XEQL(left, right));
}

LispObj *
Lisp_Equal(LispBuiltin *builtin)
/*
 equal left right
 */
{
    LispObj *left, *right;

    right = ARGUMENT(1);
    left = ARGUMENT(0);

    return (XEQUAL(left, right));
}

LispObj *
Lisp_Equalp(LispBuiltin *builtin)
/*
 equalp left right
 */
{
    LispObj *left, *right;

    right = ARGUMENT(1);
    left = ARGUMENT(0);

    return (XEQUALP(left, right));
}

LispObj *
Lisp_Error(LispBuiltin *builtin)
/*
 error control-string &rest arguments
 */
{
    LispObj *string, *arglist;

    LispObj *control_string, *arguments;

    arguments = ARGUMENT(1);
    control_string = ARGUMENT(0);

    arglist = CONS(NIL, CONS(control_string, arguments));
    GC_PROTECT(arglist);
    string = APPLY(Oformat, arglist);
    LispDestroy("%s", THESTR(string));
    /*NOTREACHED*/

    /* No need to call GC_ENTER() and GC_LEAVE() macros */
    return (NIL);
}

LispObj *
Lisp_Eval(LispBuiltin *builtin)
/*
 eval form
 */
{
    int lex;
    LispObj *form, *result;

    form = ARGUMENT(0);

    /* make sure eval form will not access local variables */
    lex = lisp__data.env.lex;
    lisp__data.env.lex = lisp__data.env.length;
    result = EVAL(form);
    lisp__data.env.lex = lex;

    return (result);
}

static LispObj *
LispEverySomeAnyNot(LispBuiltin *builtin, int function)
/*
 every predicate sequence &rest more-sequences
 some predicate sequence &rest more-sequences
 notevery predicate sequence &rest more-sequences
 notany predicate sequence &rest more-sequences
 */
{
    GC_ENTER();
    long i, j, length, count;
    LispObj *result, *list, *item, *arguments, *acons, *value;
    SeqInfo stk[8], *seqs;

    LispObj *predicate, *sequence, *more_sequences;

    more_sequences = ARGUMENT(2);
    sequence = ARGUMENT(1);
    predicate = ARGUMENT(0);

    count = 1;
    length = LispLength(sequence);
    for (list = more_sequences; CONSP(list); list = CDR(list), count++) {
	i = LispLength(CAR(list));
	if (i < length)
	    length = i;
    }

    result = function == EVERY || function == NOTANY ? T : NIL;

    /* if at least one sequence has length zero */
    if (length == 0)
	return (result);

    if (count > sizeof(stk) / sizeof(stk[0]))
	seqs = LispMalloc(count * sizeof(SeqInfo));
    else
	seqs = &stk[0];

    /* build information about sequences */
    SETSEQ(seqs[0], sequence);
    for (i = 1, list = more_sequences; CONSP(list); list = CDR(list), i++) {
	item = CAR(list);
	SETSEQ(seqs[i], item);
    }

    /* prepare argument list */
    arguments = acons = CONS(NIL, NIL);
    GC_PROTECT(arguments);
    for (i = 1; i < count; i++) {
	RPLACD(acons, CONS(NIL, NIL));
	acons = CDR(acons);
    }

    /* loop applying predicate in sequence elements */
    for (i = 0; i < length; i++) {

	/* build argument list */
	for (acons = arguments, j = 0; j < count; acons = CDR(acons), j++) {
	    if (seqs[j].type == LispString_t)
		item = SCHAR(*seqs[j].data.string++);
	    else {
		item = CAR(seqs[j].data.list);
		seqs[j].data.list = CDR(seqs[j].data.list);
	    }
	    RPLACA(acons, item);
	}

	/* apply predicate */
	value = APPLY(predicate, arguments);

	/* check if needs to terminate loop */
	if (value == NIL) {
	    if (function == EVERY) {
		result = NIL;
		break;
	    }
	    if (function == NOTEVERY) {
		result = T;
		break;
	    }
	}
	else {
	    if (function == SOME) {
		result = value;
		break;
	    }
	    if (function == NOTANY) {
		result = NIL;
		break;
	    }
	}
    }

    GC_LEAVE();
    if (seqs != &stk[0])
	LispFree(seqs);

    return (result);
}

LispObj *
Lisp_Every(LispBuiltin *builtin)
/*
 every predicate sequence &rest more-sequences
 */
{
    return (LispEverySomeAnyNot(builtin, EVERY));
}

LispObj *
Lisp_Some(LispBuiltin *builtin)
/*
 some predicate sequence &rest more-sequences
 */
{
    return (LispEverySomeAnyNot(builtin, SOME));
}

LispObj *
Lisp_Notevery(LispBuiltin *builtin)
/*
 notevery predicate sequence &rest more-sequences
 */
{
    return (LispEverySomeAnyNot(builtin, NOTEVERY));
}

LispObj *
Lisp_Notany(LispBuiltin *builtin)
/*
 notany predicate sequence &rest more-sequences
 */
{
    return (LispEverySomeAnyNot(builtin, NOTANY));
}

LispObj *
Lisp_Fboundp(LispBuiltin *builtin)
/*
 fboundp symbol
 */
{
    LispAtom *atom;

    LispObj *symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);

    atom = symbol->data.atom;
    if (atom->a_function || atom->a_builtin || atom->a_compiled)
	return (T);

    return (NIL);
}

LispObj *
Lisp_Find(LispBuiltin *builtin)
/*
 find item sequence &key from-end test test-not start end key
 */
{
    return (LispFindOrPosition(builtin, FIND, NONE));
}

LispObj *
Lisp_FindIf(LispBuiltin *builtin)
/*
 find-if predicate sequence &key from-end start end key
 */
{
    return (LispFindOrPosition(builtin, FIND, IF));
}

LispObj *
Lisp_FindIfNot(LispBuiltin *builtin)
/*
 find-if-not predicate sequence &key from-end start end key
 */
{
    return (LispFindOrPosition(builtin, FIND, IFNOT));
}

LispObj *
Lisp_Fill(LispBuiltin *builtin)
/*
 fill sequence item &key start end
 */
{
    long i, start, end, length;

    LispObj *sequence, *item, *ostart, *oend;

    oend = ARGUMENT(3);
    ostart = ARGUMENT(2);
    item = ARGUMENT(1);
    sequence = ARGUMENT(0);

    LispCheckSequenceStartEnd(builtin, sequence, ostart, oend,
			      &start, &end, &length);

    if (STRINGP(sequence)) {
	int ch;
	char *string = THESTR(sequence);

	CHECK_STRING_WRITABLE(sequence);
	CHECK_SCHAR(item);
	ch = SCHAR_VALUE(item);
	for (i = start; i < end; i++)
	    string[i] = ch;
    }
    else {
	LispObj *list;

	if (CONSP(sequence))
	    list = sequence;
	else
	    list = sequence->data.array.list;

	for (i = 0; i < start; i++, list = CDR(list))
	    ;
	for (; i < end; i++, list = CDR(list))
	    RPLACA(list, item);
    }

    return (sequence);
}

LispObj *
Lisp_Fmakunbound(LispBuiltin *builtin)
/*
 fmkaunbound symbol
 */
{
    LispObj *symbol;

    symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);
    if (symbol->data.atom->a_function)
	LispRemAtomFunctionProperty(symbol->data.atom);
    else if (symbol->data.atom->a_builtin)
	LispRemAtomBuiltinProperty(symbol->data.atom);
    else if (symbol->data.atom->a_compiled)
	LispRemAtomCompiledProperty(symbol->data.atom);

    return (symbol);
}

LispObj *
Lisp_Funcall(LispBuiltin *builtin)
/*
 funcall function &rest arguments
 */
{
    LispObj *result;

    LispObj *function, *arguments;

    arguments = ARGUMENT(1);
    function = ARGUMENT(0);

    result = APPLY(function, arguments);

    return (result);
}

LispObj *
Lisp_Functionp(LispBuiltin *builtin)
/*
 functionp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (FUNCTIONP(object) || LAMBDAP(object) ? T : NIL);
}

LispObj *
Lisp_Get(LispBuiltin *builtin)
/*
 get symbol indicator &optional default
 */
{
    LispObj *result;

    LispObj *symbol, *indicator, *defalt;

    defalt = ARGUMENT(2);
    indicator = ARGUMENT(1);
    symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);

    result = LispGetAtomProperty(symbol->data.atom, indicator);

    if (result != NIL)
	result = CAR(result);
    else
	result = defalt == UNSPEC ? NIL : defalt;

    return (result);
}

/*
 * ext::getenv
 */
LispObj *
Lisp_Getenv(LispBuiltin *builtin)
/*
 getenv name
 */
{
    char *value;

    LispObj *name;

    name = ARGUMENT(0);

    CHECK_STRING(name);
    value = getenv(THESTR(name));

    return (value ? STRING(value) : NIL);
}

LispObj *
Lisp_Gc(LispBuiltin *builtin)
/*
 gc &optional car cdr
 */
{
    LispObj *car, *cdr;

    cdr = ARGUMENT(1);
    car = ARGUMENT(0);

    LispGC(car, cdr);

    return (NIL);
}

LispObj *
Lisp_Gensym(LispBuiltin *builtin)
/*
 gensym &optional arg
 */
{
    char *preffix = "G", name[132];
    long counter = LONGINT_VALUE(Ogensym_counter->data.atom->property->value);
    LispObj *symbol;

    LispObj *arg;

    arg = ARGUMENT(0);
    if (arg != UNSPEC) {
	if (STRINGP(arg))
	    preffix = THESTR(arg);
	else {
	    CHECK_INDEX(arg);
	    counter = FIXNUM_VALUE(arg);
	}
    }
    snprintf(name, sizeof(name), "%s%ld", preffix, counter);
    if (strlen(name) >= 128)
	LispDestroy("%s: name %s too long", STRFUN(builtin), name);
    Ogensym_counter->data.atom->property->value = INTEGER(counter + 1);

    symbol = UNINTERNED_ATOM(name);
    symbol->data.atom->unreadable = !LispCheckAtomString(name);

    return (symbol);
}

LispObj *
Lisp_Go(LispBuiltin *builtin)
/*
 go tag
 */
{
    unsigned blevel = lisp__data.block.block_level;

    LispObj *tag;

    tag = ARGUMENT(0);

    while (blevel) {
	LispBlock *block = lisp__data.block.block[--blevel];

	if (block->type == LispBlockClosure)
	    /* if reached a function call */
	    break;
	if (block->type == LispBlockBody) {
	    lisp__data.block.block_ret = tag;
	    LispBlockUnwind(block);
	    BLOCKJUMP(block);
	}
     }

    LispDestroy("%s: no visible tagbody for %s",
		STRFUN(builtin), STROBJ(tag));
    /*NOTREACHED*/
    return (NIL);
}

LispObj *
Lisp_If(LispBuiltin *builtin)
/*
 if test then &optional else
 */
{
    LispObj *result, *test, *then, *oelse;

    oelse = ARGUMENT(2);
    then = ARGUMENT(1);
    test = ARGUMENT(0);

    test = EVAL(test);
    if (test != NIL)
	result = EVAL(then);
    else if (oelse != UNSPEC)
	result = EVAL(oelse);
    else
	result = NIL;

    return (result);
}

LispObj *
Lisp_IgnoreErrors(LispBuiltin *builtin)
/*
 ignore-erros &rest body
 */
{
    LispObj *result;
    int i, jumped;
    LispBlock *block;

    /* interpreter state */
    GC_ENTER();
    int stack, lex, length;

    /* memory allocation */
    int mem_level;
    void **mem;

    LispObj *body;

    body = ARGUMENT(0);

    /* Save environment information */
    stack = lisp__data.stack.length;
    lex = lisp__data.env.lex;
    length = lisp__data.env.length;

    /* Save memory allocation information */
    mem_level = lisp__data.mem.level;
    mem = LispMalloc(mem_level * sizeof(void*));
    memcpy(mem, lisp__data.mem.mem, mem_level * sizeof(void*));

    ++lisp__data.ignore_errors;
    result = NIL;
    jumped = 1;
    block = LispBeginBlock(NIL, LispBlockProtect);
    if (setjmp(block->jmp) == 0) {
	for (; CONSP(body); body = CDR(body))
	    result = EVAL(CAR(body));
	jumped = 0;
    }
    LispEndBlock(block);
    if (!lisp__data.destroyed && jumped)
	result = lisp__data.block.block_ret;

    if (lisp__data.destroyed) {
	/* Restore environment */
	lisp__data.stack.length = stack;
	lisp__data.env.lex = lex;
	lisp__data.env.head = lisp__data.env.length = length;
	GC_LEAVE();

	/* Check for possible leaks due to ignoring errors */
	for (i = 0; i < mem_level; i++) {
	    if (lisp__data.mem.mem[i] && mem[i] != lisp__data.mem.mem[i])
		LispFree(lisp__data.mem.mem[i]);
	}
	for (; i < lisp__data.mem.level; i++) {
	    if (lisp__data.mem.mem[i])
		LispFree(lisp__data.mem.mem[i]);
	}

	lisp__data.destroyed = 0;
	result = NIL;
	RETURN_COUNT = 1;
	RETURN(0) = lisp__data.error_condition;
    }
    LispFree(mem);
    --lisp__data.ignore_errors;

    return (result);
}

LispObj *
Lisp_Intersection(LispBuiltin *builtin)
/*
 intersection list1 list2 &key test test-not key
 */
{
    return (LispListSet(builtin, INTERSECTION));
}

LispObj *
Lisp_Nintersection(LispBuiltin *builtin)
/*
 nintersection list1 list2 &key test test-not key
 */
{
    return (LispListSet(builtin, NINTERSECTION));
}

LispObj *
Lisp_Keywordp(LispBuiltin *builtin)
/*
 keywordp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (KEYWORDP(object) ? T : NIL);
}

LispObj *
Lisp_Lambda(LispBuiltin *builtin)
/*
 lambda lambda-list &rest body
 */
{
    GC_ENTER();
    LispObj *name;
    LispArgList *alist;

    LispObj *lambda, *lambda_list, *body;

    body = ARGUMENT(1);
    lambda_list = ARGUMENT(0);

    alist = LispCheckArguments(LispLambda, lambda_list, Snil->value, 0);

    name = OPAQUE(alist, LispArgList_t);
    lambda_list = LispListProtectedArguments(alist);
    GC_PROTECT(name);
    GC_PROTECT(lambda_list);
    lambda = LispNewLambda(name, body, lambda_list, LispLambda);
    LispUseArgList(alist);
    GC_LEAVE();

    return (lambda);
}

LispObj *
Lisp_Last(LispBuiltin *builtin)
/*
 last list &optional count
 */
{
    long count, length;
    LispObj *list, *ocount;

    ocount = ARGUMENT(1);
    list = ARGUMENT(0);

    if (!CONSP(list))
	return (list);

    length = LispLength(list);

    if (ocount == UNSPEC)
	count = 1;
    else {
	CHECK_INDEX(ocount);
	count = FIXNUM_VALUE(ocount);
    }

    if (count >= length)
	return (list);

    length -= count;
    for (; length > 0; length--)
	list = CDR(list);

    return (list);
}

LispObj *
Lisp_Length(LispBuiltin *builtin)
/*
 length sequence
 */
{
    LispObj *sequence;

    sequence = ARGUMENT(0);

    return (FIXNUM(LispLength(sequence)));
}

LispObj *
Lisp_Let(LispBuiltin *builtin)
/*
 let init &rest body
 */
{
    GC_ENTER();
    int head = lisp__data.env.length;
    LispObj *init, *body, *pair, *result, *list, *cons = NIL;

    body = ARGUMENT(1);
    init = ARGUMENT(0);

    CHECK_LIST(init);
    for (list = NIL; CONSP(init); init = CDR(init)) {
	LispObj *symbol, *value;

	pair = CAR(init);
	if (SYMBOLP(pair)) {
	    symbol = pair;
	    value = NIL;
	}
	else {
	    CHECK_CONS(pair);
	    symbol = CAR(pair);
	    CHECK_SYMBOL(symbol);
	    pair = CDR(pair);
	    if (CONSP(pair)) {
		value = CAR(pair);
		if (CDR(pair) != NIL)
		    LispDestroy("%s: too much arguments to initialize %s",
				STRFUN(builtin), STROBJ(symbol));
		value = EVAL(value);
	    }
	    else
		value = NIL;
	}
	pair = CONS(symbol, value);
	if (list == NIL) {
	    list = cons = CONS(pair, NIL);
	    GC_PROTECT(list);
	}
	else {
	    RPLACD(cons, CONS(pair, NIL));
	    cons = CDR(cons);
	}
    }
    /* Add variables */
    for (; CONSP(list); list = CDR(list)) {
	pair = CAR(list);
	CHECK_CONSTANT(CAR(pair));
	LispAddVar(CAR(pair), CDR(pair));
	++lisp__data.env.head;
    }
    /* Values of symbols are now protected */
    GC_LEAVE();

    /* execute body */
    for (result = NIL; CONSP(body); body = CDR(body))
	result = EVAL(CAR(body));

    lisp__data.env.head = lisp__data.env.length = head;

    return (result);
}

LispObj *
Lisp_LetP(LispBuiltin *builtin)
/*
 let* init &rest body
 */
{
    int head = lisp__data.env.length;
    LispObj *init, *body, *pair, *result;

    body = ARGUMENT(1);
    init = ARGUMENT(0);

    CHECK_LIST(init);
    for (; CONSP(init); init = CDR(init)) {
	LispObj *symbol, *value;

	pair = CAR(init);
	if (SYMBOLP(pair)) {
	    symbol = pair;
	    value = NIL;
	}
	else {
	    CHECK_CONS(pair);
	    symbol = CAR(pair);
	    CHECK_SYMBOL(symbol);
	    pair = CDR(pair);
	    if (CONSP(pair)) {
		value = CAR(pair);
		if (CDR(pair) != NIL)
		    LispDestroy("%s: too much arguments to initialize %s",
				STRFUN(builtin), STROBJ(symbol));
		value = EVAL(value);
	    }
	    else
		value = NIL;
	}

	CHECK_CONSTANT(symbol);
	LispAddVar(symbol, value);
	++lisp__data.env.head;
    }

    /* execute body */
    for (result = NIL; CONSP(body); body = CDR(body))
	result = EVAL(CAR(body));

    lisp__data.env.head = lisp__data.env.length = head;

    return (result);
}

LispObj *
Lisp_List(LispBuiltin *builtin)
/*
 list &rest args
 */
{
    LispObj *args;

    args = ARGUMENT(0);

    return (args);
}

LispObj *
Lisp_ListP(LispBuiltin *builtin)
/*
 list* object &rest more-objects
 */
{
    GC_ENTER();
    LispObj *result, *cons;

    LispObj *object, *more_objects;

    more_objects = ARGUMENT(1);
    object = ARGUMENT(0);

    if (!CONSP(more_objects))
	return (object);

    result = cons = CONS(object, CAR(more_objects));
    GC_PROTECT(result);
    for (more_objects = CDR(more_objects); CONSP(more_objects);
	 more_objects = CDR(more_objects)) {
	object = CAR(more_objects);
	RPLACD(cons, CONS(CDR(cons), object));
	cons = CDR(cons);
    }
    GC_LEAVE();

    return (result);
}

/* "classic" list-length */
LispObj *
Lisp_ListLength(LispBuiltin *builtin)
/*
 list-length list
 */
{
    long length;
    LispObj *fast, *slow;

    LispObj *list;

    list = ARGUMENT(0);

    CHECK_LIST(list);
    for (fast = slow = list, length = 0;
	 CONSP(slow);
	 slow = CDR(slow), length += 2) {
	if (fast == NIL)
	    break;
	CHECK_CONS(fast);
	fast = CDR(fast);
	if (fast == NIL) {
	    ++length;
	    break;
	}
	CHECK_CONS(fast);
	fast = CDR(fast);
	if (slow == fast)
	    /* circular list */
	    return (NIL);
    }

    return (FIXNUM(length));
}

LispObj *
Lisp_Listp(LispBuiltin *builtin)
/*
 listp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (object == NIL || CONSP(object) ? T : NIL);
}

static LispObj *
LispListSet(LispBuiltin *builtin, int function)
/*
 intersection list1 list2 &key test test-not key
 nintersection list1 list2 &key test test-not key
 set-difference list1 list2 &key test test-not key
 nset-difference list1 list2 &key test test-not key
 set-exclusive-or list1 list2 &key test test-not key
 nset-exclusive-or list1 list2 &key test test-not key
 subsetp list1 list2 &key test test-not key
 union list1 list2 &key test test-not key
 nunion list1 list2 &key test test-not key
 */
{
    GC_ENTER();
    int code, expect, value, inplace, check_list2,
	intersection, setdifference, xunion, setexclusiveor;
    LispObj *lambda, *result, *cmp, *cmp1, *cmp2,
	    *item, *clist1, *clist2, *cons, *cdr;

    LispObj *list1, *list2, *test, *test_not, *key;

    key = ARGUMENT(4);
    test_not = ARGUMENT(3);
    test = ARGUMENT(2);
    list2 = ARGUMENT(1);
    list1 = ARGUMENT(0);

    /* Check if arguments are valid lists */
    CHECK_LIST(list1);
    CHECK_LIST(list2);

    setdifference = intersection = xunion = setexclusiveor = inplace = 0;
    switch (function) {
	case NSETDIFFERENCE:
	    inplace = 1;
	case SETDIFFERENCE:
	    setdifference = 1;
	    break;
	case NINTERSECTION:
	    inplace = 1;
	case INTERSECTION:
	    intersection = 1;
	    break;
	case NUNION:
	    inplace = 1;
	case UNION:
	    xunion = 1;
	    break;
	case NSETEXCLUSIVEOR:
	    inplace = 1;
	case SETEXCLUSIVEOR:
	    setexclusiveor = 1;
	    break;
    }

    /* Check for fast return */
    if (list1 == NIL)
	return (setdifference || intersection ?
		NIL : function == SUBSETP ? T : list2);
    if (list2 == NIL)
	return (intersection || xunion || function == SUBSETP ? NIL : list1);

    CHECK_TEST();
    clist1 = cdr = NIL;

    /* Make a copy of list2 with the key predicate applied */
    if (key != UNSPEC) {
	result = cons = CONS(APPLY1(key, CAR(list2)), NIL);
	GC_PROTECT(result);
	for (cmp2 = CDR(list2); CONSP(cmp2); cmp2 = CDR(cmp2)) {
	    item = APPLY1(key, CAR(cmp2));
	    RPLACD(cons, CONS(APPLY1(key, CAR(cmp2)), NIL));
	    cons = CDR(cons);
	}
	/* check if list2 is a proper list */
	CHECK_LIST(cmp2);
	clist2 = result;
	check_list2 = 0;
    }
    else {
	clist2 = list2;
	check_list2 = 1;
    }
    result = cons = NIL;

    /* Compare elements of lists
     * Logic:
     *	   UNION
     *		1) Walk list1 and if CAR(list1) not in list2, add it to result
     *		2) Add list2 to result
     *	   INTERSECTION
     *		1) Walk list1 and if CAR(list1) in list2, add it to result
     *	   SET-DIFFERENCE
     *		1) Walk list1 and if CAR(list1) not in list2, add it to result
     *	   SET-EXCLUSIVE-OR
     *		1) Walk list1 and if CAR(list1) not in list2, add it to result
     *		2) Walk list2 and if CAR(list2) not in list1, add it to result
     *	   SUBSETP
     *		1) Walk list1 and if CAR(list1) not in list2, return NIL
     *		2) Return T
     */
    value = 0;
    for (cmp1 = list1; CONSP(cmp1); cmp1 = CDR(cmp1)) {
	item = CAR(cmp1);

	/* Apply key predicate if required */
	if (key != UNSPEC) {
	    cmp = APPLY1(key, item);
	    if (setexclusiveor) {
		if (clist1 == NIL) {
		    clist1 = cdr = CONS(cmp, NIL);
		    GC_PROTECT(clist1);
		}
		else {
		    RPLACD(cdr, CONS(cmp, NIL));
		    cdr = CDR(cdr);
		}
	    }
	}
	else
	    cmp = item;

	/* Compare against list2 */
	for (cmp2 = clist2; CONSP(cmp2); cmp2 = CDR(cmp2)) {
	    value = FCOMPARE(lambda, cmp, CAR(cmp2), code);
	    if (value == expect)
		break;
	}
	if (check_list2 && value != expect) {
	    /* check if list2 is a proper list */
	    CHECK_LIST(cmp2);
	    check_list2 = 0;
	}

	if (function == SUBSETP) {
	    /* Element of list1 not in list2? */
	    if (value != expect) {
		GC_LEAVE();

		return (NIL);
	    }
	}
	/* If need to add item to result */
	else if (((setdifference || xunion || setexclusiveor) &&
		  value != expect) ||
		 (intersection && value == expect)) {
	    if (inplace) {
		if (result == NIL)
		    result = cons = cmp1;
		else {
		    if (setexclusiveor) {
			/* don't remove elements yet, will need
			 * to check agains't list2 later */
			for (cmp2 = cons; CDR(cmp2) != cmp1; cmp2 = CDR(cmp2))
			    ;
			if (cmp2 != cons) {
			    RPLACD(cmp2, list1);
			    list1 = cmp2;
			}
		    }
		    RPLACD(cons, cmp1);
		    cons = cmp1;
		}
	    }
	    else {
		if (result == NIL) {
		    result = cons = CONS(item, NIL);
		    GC_PROTECT(result);
		}
		else {
		    RPLACD(cons, CONS(item, NIL));
		    cons = CDR(cons);
		}
	    }
	}
    }
    /* check if list1 is a proper list */
    CHECK_LIST(cmp1);

    if (function == SUBSETP) {
	GC_LEAVE();

	return (T);
    }
    else if (xunion) {
	/* Add list2 to tail of result */
	if (result == NIL)
	    result = list2;
	else
	    RPLACD(cons, list2);
    }
    else if (setexclusiveor) {
	LispObj *result2, *cons2;

	result2 = cons2 = NIL;
	for (cmp2 = list2; CONSP(cmp2); cmp2 = CDR(cmp2)) {
	    item = CAR(cmp2);

	    if (key != UNSPEC) {
		cmp = CAR(clist2);
		/* XXX changing clist2 */
		clist2 = CDR(clist2);
		cmp1 = clist1;
	    }
	    else {
		cmp = item;
		cmp1 = list1;
	    }

	    /* Compare against list1 */
	    for (; CONSP(cmp1); cmp1 = CDR(cmp1)) {
		value = FCOMPARE(lambda, cmp, CAR(cmp1), code);
		if (value == expect)
		    break;
	    }

	    if (value != expect) {
		if (inplace) {
		    if (result2 == NIL)
			result2 = cons2 = cmp2;
		    else {
			RPLACD(cons2, cmp2);
			cons2 = cmp2;
		    }
		}
		else {
		    if (result == NIL) {
			result = cons = CONS(item, NIL);
			GC_PROTECT(result);
		    }
		    else {
			RPLACD(cons, CONS(item, NIL));
			cons = CDR(cons);
		    }
		}
	    }
	}
	if (inplace) {
	    if (CONSP(cons2))
		RPLACD(cons2, NIL);
	    if (result == NIL)
		result = result2;
	    else
		RPLACD(cons, result2);
	}
    }
    else if ((function == NSETDIFFERENCE || function == NINTERSECTION) &&
	     CONSP(cons))
	RPLACD(cons, NIL);

    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_Loop(LispBuiltin *builtin)
/*
 loop &rest body
 */
{
    LispObj *code, *result;
    LispBlock *block;

    LispObj *body;

    body = ARGUMENT(0);

    result = NIL;
    block = LispBeginBlock(NIL, LispBlockTag);
    if (setjmp(block->jmp) == 0) {
	for (;;)
	    for (code = body; CONSP(code); code = CDR(code))
		(void)EVAL(CAR(code));
    }
    LispEndBlock(block);
    result = lisp__data.block.block_ret;

    return (result);
}

/* XXX This function is broken, needs a review
 (being delayed until true array/vectors be implemented) */
LispObj *
Lisp_MakeArray(LispBuiltin *builtin)
/*
 make-array dimensions &key element-type initial-element initial-contents
			    adjustable fill-pointer displaced-to
			    displaced-index-offset
 */
{
    long rank = 0, count = 1, offset, zero, c;
    LispObj *obj, *dim, *array;
    LispType type;

    LispObj *dimensions, *element_type, *initial_element, *initial_contents,
	    *displaced_to, *displaced_index_offset;

    dim = array = NIL;
    type = LispNil_t;

    displaced_index_offset = ARGUMENT(7);
    displaced_to = ARGUMENT(6);
    initial_contents = ARGUMENT(3);
    initial_element = ARGUMENT(2);
    element_type = ARGUMENT(1);
    dimensions = ARGUMENT(0);

    if (INDEXP(dimensions)) {
	dim = CONS(dimensions, NIL);
	rank = 1;
	count = FIXNUM_VALUE(dimensions);
    }
    else if (CONSP(dimensions)) {
	dim = dimensions;

	for (rank = 0; CONSP(dim); rank++, dim = CDR(dim)) {
	    obj = CAR(dim);
	    CHECK_INDEX(obj);
	    count *= FIXNUM_VALUE(obj);
	}
	dim = dimensions;
    }
    else if (dimensions == NIL) {
	dim = NIL;
	rank = count = 0;
    }
    else
	LispDestroy("%s: %s is a bad array dimension",
		    STRFUN(builtin), STROBJ(dimensions));

    /* check element-type */
    if (element_type != UNSPEC) {
	if (element_type == T)
	    type = LispNil_t;
	else if (!SYMBOLP(element_type))
	    LispDestroy("%s: unsupported element type %s",
			STRFUN(builtin), STROBJ(element_type));
	else {
	    Atom_id atom = ATOMID(element_type);

	    if (atom == Satom)
		type = LispAtom_t;
	    else if (atom == Sinteger)
		type = LispInteger_t;
	    else if (atom == Scharacter)
		type = LispSChar_t;
	    else if (atom == Sstring)
		type = LispString_t;
	    else if (atom == Slist)
		type = LispCons_t;
	    else if (atom == Sopaque)
		type = LispOpaque_t;
	    else
		LispDestroy("%s: unsupported element type %s",
			    STRFUN(builtin), ATOMID(element_type)->value);
	}
    }

    /* check initial-contents */
    if (rank) {
	CHECK_LIST(initial_contents);
    }

    /* check displaced-to */
    if (displaced_to != UNSPEC) {
	CHECK_ARRAY(displaced_to);
    }

    /* check displaced-index-offset */
    offset = -1;
    if (displaced_index_offset != UNSPEC) {
	CHECK_INDEX(displaced_index_offset);
	offset = FIXNUM_VALUE(displaced_index_offset);
    }

    c = 0;
    if (initial_element != UNSPEC)
	++c;
    if (initial_contents != UNSPEC)
	++c;
    if (displaced_to != UNSPEC || offset >= 0)
	++c;
    if (c > 1)
	LispDestroy("%s: more than one initialization specified",
		    STRFUN(builtin));
    if (initial_element == UNSPEC)
	initial_element = NIL;

    zero = count == 0;
    if (displaced_to != UNSPEC) {
	CHECK_ARRAY(displaced_to);
	if (offset < 0)
	    offset = 0;
	for (c = 1, obj = displaced_to->data.array.dim; obj != NIL;
	     obj = CDR(obj))
	    c *= FIXNUM_VALUE(CAR(obj));
	if (c < count + offset)
	    LispDestroy("%s: array-total-size + displaced-index-offset "
			"exceeds total size", STRFUN(builtin));
	for (c = 0, array = displaced_to->data.array.list; c < offset; c++)
	    array = CDR(array);
    }
    else if (initial_contents != UNSPEC) {
	CHECK_CONS(initial_contents);
	if (rank == 0)
	    array = initial_contents;
	else if (rank == 1) {
	    for (array = initial_contents, c = 0; c < count;
		 array = CDR(array), c++)
		if (!CONSP(array))
		    LispDestroy("%s: bad argument or size %s",
				STRFUN(builtin), STROBJ(array));
	    if (array != NIL)
		LispDestroy("%s: bad argument or size %s",
			    STRFUN(builtin), STROBJ(array));
	    array = initial_contents;
	}
	else {
	    LispObj *err = NIL;
	    /* check if list matches */
	    int i, j, k, *dims, *loop;

	    /* create iteration variables */
	    dims = LispMalloc(sizeof(int) * rank);
	    loop = LispCalloc(1, sizeof(int) * (rank - 1));
	    for (i = 0, obj = dim; CONSP(obj); i++, obj = CDR(obj))
		dims[i] = FIXNUM_VALUE(CAR(obj));

	    /* check if list matches specified dimensions */
	    while (loop[0] < dims[0]) {
		for (obj = initial_contents, i = 0; i < rank - 1; i++) {
		    for (j = 0; j < loop[i]; j++)
			obj = CDR(obj);
		    err = obj;
		    if (!CONSP(obj = CAR(obj)))
			goto make_array_error;
		    err = obj;
		}
		--i;
		for (;;) {
		    ++loop[i];
		    if (i && loop[i] >= dims[i])
			loop[i] = 0;
		    else
			break;
		    --i;
		}
		for (k = 0; k < dims[rank - 1]; obj = CDR(obj), k++) {
		    if (!CONSP(obj))
			goto make_array_error;
		}
		if (obj == NIL)
		    continue;
make_array_error:
		LispFree(dims);
		LispFree(loop);
		LispDestroy("%s: bad argument or size %s",
			    STRFUN(builtin), STROBJ(err));
	    }

	    /* list is correct, use it to fill initial values */

	    /* reset loop */
	    memset(loop, 0, sizeof(int) * (rank - 1));

	    GCDisable();
	    /* fill array with supplied values */
	    array = NIL;
	    while (loop[0] < dims[0]) {
		for (obj = initial_contents, i = 0; i < rank - 1; i++) {
		    for (j = 0; j < loop[i]; j++)
			obj = CDR(obj);
		    obj = CAR(obj);
		}
		--i;
		for (;;) {
		    ++loop[i];
		    if (i && loop[i] >= dims[i])
			loop[i] = 0;
		    else
			break;
		    --i;
		}
		for (k = 0; k < dims[rank - 1]; obj = CDR(obj), k++) {
		    if (array == NIL)
			array = CONS(CAR(obj), NIL);
		    else {
			RPLACD(array, CONS(CAR(array), CDR(array)));
			RPLACA(array, CAR(obj));
		    }
		}
	    }
	    LispFree(dims);
	    LispFree(loop);
	    array = LispReverse(array);
	    GCEnable();
	}
    }
    else {
	GCDisable();
	/* allocate array */
	if (count) {
	    --count;
	    array = CONS(initial_element, NIL);
	    while (count) {
		RPLACD(array, CONS(CAR(array), CDR(array)));
		RPLACA(array, initial_element);
		count--;
	    }
	}
	GCEnable();
    }

    obj = LispNew(array, dim);
    obj->type = LispArray_t;
    obj->data.array.list = array;
    obj->data.array.dim = dim;
    obj->data.array.rank = rank;
    obj->data.array.type = type;
    obj->data.array.zero = zero;

    return (obj);
}

LispObj *
Lisp_MakeList(LispBuiltin *builtin)
/*
 make-list size &key initial-element
 */
{
    GC_ENTER();
    long count;
    LispObj *result, *cons;

    LispObj *size, *initial_element;

    initial_element = ARGUMENT(1);
    size = ARGUMENT(0);

    CHECK_INDEX(size);
    count = FIXNUM_VALUE(size);

    if (count == 0)
	return (NIL);
    if (initial_element == UNSPEC)
	initial_element = NIL;

    result = cons = CONS(initial_element, NIL);
    GC_PROTECT(result);
    for (; count > 1; count--) {
	RPLACD(cons, CONS(initial_element, NIL));
	cons = CDR(cons);
    }
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_MakeSymbol(LispBuiltin *builtin)
/*
 make-symbol name
 */
{
    LispObj *name, *symbol;

    name = ARGUMENT(0);
    CHECK_STRING(name);

    symbol = UNINTERNED_ATOM(THESTR(name));
    symbol->data.atom->unreadable = !LispCheckAtomString(THESTR(name));

    return (symbol);
}

LispObj *
Lisp_Makunbound(LispBuiltin *builtin)
/*
 makunbound symbol
 */
{
    LispObj *symbol;

    symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);
    LispUnsetVar(symbol);

    return (symbol);
}

LispObj *
Lisp_Mapc(LispBuiltin *builtin)
/*
 mapc function list &rest more-lists
 */
{
    return (LispMapc(builtin, 0));
}

LispObj *
Lisp_Mapcar(LispBuiltin *builtin)
/*
 mapcar function list &rest more-lists
 */
{
    return (LispMapc(builtin, 1));
}

/* Like nconc but ignore non list arguments */
LispObj *
LispMapnconc(LispObj *list)
{
    LispObj *result = NIL;

    if (CONSP(list)) {
	LispObj *cons, *head, *tail;

	cons = NIL;
	for (; CONSP(CDR(list)); list = CDR(list)) {
	    head = CAR(list);
	    if (CONSP(head)) {
		for (tail = head; CONSP(CDR(tail)); tail = CDR(tail))
		    ;
		if (cons != NIL)
		    RPLACD(cons, head);
		else
		    result = head;
		cons = tail;
	    }
	}
	head = CAR(list);
	if (CONSP(head)) {
	    if (cons != NIL)
		RPLACD(cons, head);
	    else
		result = head;
	}
    }

    return (result);
}

LispObj *
Lisp_Mapcan(LispBuiltin *builtin)
/*
 mapcan function list &rest more-lists
 */
{
    return (LispMapnconc(LispMapc(builtin, 1)));
}

static LispObj *
LispMapc(LispBuiltin *builtin, int mapcar)
{
    GC_ENTER();
    long i, offset, count, length;
    LispObj *result = NIL, *cons, *arguments, *acons, *rest, *alist, *value;
    LispObj *stk[8], **cdrs;

    LispObj *function, *list, *more_lists;

    more_lists = ARGUMENT(2);
    list = ARGUMENT(1);
    function = ARGUMENT(0);

    /* Result will be no longer than this */
    for (length = 0, alist = list; CONSP(alist); length++, alist = CDR(alist))
	;

    /* If first argument is not a list... */
    if (length == 0)
	return (NIL);

    /* At least one argument will be passed to function, count how many
     * extra arguments will be used, and calculate result length. */
    count = 0;
    for (rest = more_lists; CONSP(rest); rest = CDR(rest), count++) {

	/* Check if extra list is really a list, and if it is smaller
	 * than the first list */
	for (i = 0, alist = CAR(rest);
	     i < length && CONSP(alist);
	     i++, alist = CDR(alist))
	    ;

	/* If it is not a true list */
	if (i == 0)
	    return (NIL);

	/* If it is smaller than the currently calculated result length */
	if (i < length)
	    length = i;
    }

    if (mapcar) {
	/* Initialize gc protected object cells for resulting list */
	result = cons = CONS(NIL, NIL);
	GC_PROTECT(result);
    }
    else
	result = cons = list;

    if (count >= sizeof(stk) / sizeof(stk[0]))
	cdrs = LispMalloc(count * sizeof(LispObj*));
    else
	cdrs = &stk[0];
    for (i = 0, rest = more_lists; i < count; i++, rest = CDR(rest))
	cdrs[i] = CAR(rest);

    /* Initialize gc protected object cells for argument list */
    arguments = acons = CONS(NIL, NIL);
    GC_PROTECT(arguments);

    /* Allocate space for extra arguments */
    for (i = 0; i < count; i++) {
	RPLACD(acons, CONS(NIL, NIL));
	acons = CDR(acons);
    }

    /* For every element of the list that will be used */
    for (offset = 0;; list = CDR(list)) {
	acons = arguments;

	/* Add first argument */
	RPLACA(acons, CAR(list));
	acons = CDR(acons);

	/* For every extra list argument */
	for (i = 0; i < count; i++) {
	    alist = cdrs[i];
	    cdrs[i] = CDR(cdrs[i]);

	    /* Add element to argument list */
	    RPLACA(acons, CAR(alist));
	    acons = CDR(acons);
	}

	value = APPLY(function, arguments);

	if (mapcar) {
	    /* Store result */
	    RPLACA(cons, value);

	    /* Allocate new result cell */
	    if (++offset < length) {
		RPLACD(cons, CONS(NIL, NIL));
		cons = CDR(cons);
	    }
	    else
		break;
	}
	else if (++offset >= length)
	    break;
    }

    /* Unprotect argument and result list */
    GC_LEAVE();
    if (cdrs != &stk[0])
	LispFree(cdrs);

    return (result);
}

static LispObj *
LispMapl(LispBuiltin *builtin, int maplist)
{
    GC_ENTER();
    long i, offset, count, length;
    LispObj *result = NIL, *cons, *arguments, *acons, *rest, *alist, *value;
    LispObj *stk[8], **cdrs;

    LispObj *function, *list, *more_lists;

    more_lists = ARGUMENT(2);
    list = ARGUMENT(1);
    function = ARGUMENT(0);

    /* count is the number of lists, length is the length of the result */
    for (length = 0, alist = list; CONSP(alist); length++, alist = CDR(alist))
	;

    /* first argument is not a list */
    if (length == 0)
	return (NIL);

    /* check remaining arguments */
    for (count = 0, rest = more_lists; CONSP(rest); rest = CDR(rest), count++) {
	for (i = 0, alist = CAR(rest);
	     i < length && CONSP(alist);
	     i++, alist = CDR(alist))
	    ;
	/* argument is not a list */
	if (i == 0)
	    return (NIL);
	/* result will have the length of the smallest list */
	if (i < length)
	    length = i;
    }

    /* result will be a list */
    if (maplist) {
	result = cons = CONS(NIL, NIL);
	GC_PROTECT(result);
    }
    else
	result = cons = list;

    if (count >= sizeof(stk) / sizeof(stk[0]))
	cdrs = LispMalloc(count * sizeof(LispObj*));
    else
	cdrs = &stk[0];
    for (i = 0, rest = more_lists; i < count; i++, rest = CDR(rest))
	cdrs[i] = CAR(rest);

    /* initialize argument list */
    arguments = acons = CONS(NIL, NIL);
    GC_PROTECT(arguments);
    for (i = 0; i < count; i++) {
	RPLACD(acons, CONS(NIL, NIL));
	acons = CDR(acons);
    }

    /* for every used list element */
    for (offset = 0;; list = CDR(list)) {
	acons = arguments;

	/* first argument */
	RPLACA(acons, list);
	acons = CDR(acons);

	/* for every extra list */
	for (i = 0; i < count; i++) {
	    RPLACA(acons, cdrs[i]);
	    cdrs[i] = CDR(cdrs[i]);
	    acons = CDR(acons);
	}

	value = APPLY(function, arguments);

	if (maplist) {
	    /* store result */
	    RPLACA(cons, value);

	    /* allocate new cell */
	    if (++offset < length) {
		RPLACD(cons, CONS(NIL, NIL));
		cons = CDR(cons);
	    }
	    else
		break;
	}
	else if (++offset >= length)
	    break;
    }

    GC_LEAVE();
    if (cdrs != &stk[0])
	LispFree(cdrs);

    return (result);
}

LispObj *
Lisp_Mapl(LispBuiltin *builtin)
/*
 mapl function list &rest more-lists
 */
{
    return (LispMapl(builtin, 0));
}

LispObj *
Lisp_Maplist(LispBuiltin *builtin)
/*
 maplist function list &rest more-lists
 */
{
    return (LispMapl(builtin, 1));
}

LispObj *
Lisp_Mapcon(LispBuiltin *builtin)
/*
 mapcon function list &rest more-lists
 */
{
    return (LispMapnconc(LispMapl(builtin, 1)));
}

LispObj *
Lisp_Member(LispBuiltin *builtin)
/*
 member item list &key test test-not key
 */
{
    int code, expect;
    LispObj *compare, *lambda;
    LispObj *item, *list, *test, *test_not, *key;

    key = ARGUMENT(4);
    test_not = ARGUMENT(3);
    test = ARGUMENT(2);
    list = ARGUMENT(1);
    item = ARGUMENT(0);

    if (list == NIL)
	return (NIL);
    CHECK_CONS(list);

    CHECK_TEST();
    if (key == UNSPEC) {
	if (code == FEQ) {
	    for (; CONSP(list); list = CDR(list))
		if (item == CAR(list))
		    return (list);
	}
	else {
	    for (; CONSP(list); list = CDR(list))
		if ((FCOMPARE(lambda, item, CAR(list), code)) == expect)
		    return (list);
	}
    }
    else {
	if (code == FEQ) {
	    for (; CONSP(list); list = CDR(list))
		if (item == APPLY1(key, CAR(list)))
		    return (list);
	}
	else {
	    for (; CONSP(list); list = CDR(list)) {
		compare = APPLY1(key, CAR(list));
		if ((FCOMPARE(lambda, item, compare, code)) == expect)
		    return (list);
	    }
	}
    }
    /* check if is a proper list */
    CHECK_LIST(list);

    return (NIL);
}

LispObj *
Lisp_MemberIf(LispBuiltin *builtin)
/*
 member-if predicate list &key key
 */
{
    return (LispAssocOrMember(builtin, MEMBER, IF));
}

LispObj *
Lisp_MemberIfNot(LispBuiltin *builtin)
/*
 member-if-not predicate list &key key
 */
{
    return (LispAssocOrMember(builtin, MEMBER, IFNOT));
}

LispObj *
Lisp_MultipleValueBind(LispBuiltin *builtin)
/*
 multiple-value-bind symbols values &rest body
 */
{
    int i, head = lisp__data.env.length;
    LispObj *result, *symbol, *value;

    LispObj *symbols, *values, *body;

    body = ARGUMENT(2);
    values = ARGUMENT(1);
    symbols = ARGUMENT(0);

    result = EVAL(values);
    for (i = -1; CONSP(symbols); symbols = CDR(symbols), i++) {
	symbol = CAR(symbols);
	CHECK_SYMBOL(symbol);
	CHECK_CONSTANT(symbol);
	if (i >= 0 && i < RETURN_COUNT)
	    value = RETURN(i);
	else if (i < 0)
	    value = result;
	else
	    value = NIL;
	LispAddVar(symbol, value);
	++lisp__data.env.head;
    }

    /* Execute code with binded variables (if any) */
    for (result = NIL; CONSP(body); body = CDR(body))
	result = EVAL(CAR(body));

    lisp__data.env.head = lisp__data.env.length = head;

    return (result);
}

LispObj *
Lisp_MultipleValueCall(LispBuiltin *builtin)
/*
 multiple-value-call function &rest form
 */
{
    GC_ENTER();
    int i;
    LispObj *arguments, *cons, *result;

    LispObj *function, *form;

    form = ARGUMENT(1);
    function = ARGUMENT(0);

    /* build argument list */
    arguments = cons = NIL;
    for (; CONSP(form); form = CDR(form)) {
	RETURN_COUNT = 0;
	result = EVAL(CAR(form));
	if (RETURN_COUNT >= 0) {
	    if (arguments == NIL) {
		arguments = cons = CONS(result, NIL);
		GC_PROTECT(arguments);
	    }
	    else {
		RPLACD(cons, CONS(result, NIL));
		cons = CDR(cons);
	    }
	    for (i = 0; i < RETURN_COUNT; i++) {
		RPLACD(cons, CONS(RETURN(i), NIL));
		cons = CDR(cons);
	    }
	}
    }

    /* apply function */
    if (POINTERP(function) && !XSYMBOLP(function) && !XFUNCTIONP(function)) {
	function = EVAL(function);
	GC_PROTECT(function);
    }
    result = APPLY(function, arguments);
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_MultipleValueProg1(LispBuiltin *builtin)
/*
 multiple-value-prog1 first-form &rest form
 */
{
    GC_ENTER();
    int i, count;
    LispObj *values, *cons;

    LispObj *first_form, *form;

    form = ARGUMENT(1);
    first_form = ARGUMENT(0);

    values = EVAL(first_form);
    if (!CONSP(form))
	return (values);

    cons = NIL;
    count = RETURN_COUNT;
    if (count < 0)
	values = NIL;
    else if (count == 0) {
	GC_PROTECT(values);
    }
    else {
	values = cons = CONS(values, NIL);
	GC_PROTECT(values);
	for (i = 0; i < count; i++) {
	    RPLACD(cons, CONS(RETURN(i), NIL));
	    cons = CDR(cons);
	}
    }

    for (; CONSP(form); form = CDR(form))
	EVAL(CAR(form));

    RETURN_COUNT = count;
    if (count > 0) {
	for (i = 0, cons = CDR(values); CONSP(cons); cons = CDR(cons), i++)
	    RETURN(i) = CAR(cons);
	values = CAR(values);
    }
    GC_LEAVE();

    return (values);
}

LispObj *
Lisp_MultipleValueList(LispBuiltin *builtin)
/*
 multiple-value-list form
 */
{
    int i;
    GC_ENTER();
    LispObj *form, *result, *cons;

    form = ARGUMENT(0);

    result = EVAL(form);

    if (RETURN_COUNT < 0)
	return (NIL);

    result = cons = CONS(result, NIL);
    GC_PROTECT(result);
    for (i = 0; i < RETURN_COUNT; i++) {
	RPLACD(cons, CONS(RETURN(i), NIL));
	cons = CDR(cons);
    }
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_MultipleValueSetq(LispBuiltin *builtin)
/*
 multiple-value-setq symbols form
 */
{
    int i;
    LispObj *result, *symbol, *value;

    LispObj *symbols, *form;

    form = ARGUMENT(1);
    symbols = ARGUMENT(0);

    CHECK_LIST(symbols);
    result = EVAL(form);
    if (CONSP(symbols)) {
	symbol = CAR(symbols);
	CHECK_SYMBOL(symbol);
	CHECK_CONSTANT(symbol);
	LispSetVar(symbol, result);
	symbols = CDR(symbols);
    }
    for (i = 0; CONSP(symbols); symbols = CDR(symbols), i++) {
	symbol = CAR(symbols);
	CHECK_SYMBOL(symbol);
	CHECK_CONSTANT(symbol);
	if (i < RETURN_COUNT && RETURN_COUNT > 0)
	    value = RETURN(i);
	else
	    value = NIL;
	LispSetVar(symbol, value);
    }

    return (result);
}

LispObj *
Lisp_Nconc(LispBuiltin *builtin)
/*
 nconc &rest lists
 */
{
    LispObj *list, *lists, *cons, *head, *tail;

    lists = ARGUMENT(0);

    /* skip any initial empty lists */
    for (; CONSP(lists); lists = CDR(lists))
	if (CAR(lists) != NIL)
	    break;

    /* don't check if a proper list */
    if (!CONSP(lists))
	return (lists);

    /* setup to concatenate lists */
    list = CAR(lists);
    CHECK_CONS(list);
    for (cons = list; CONSP(CDR(cons)); cons = CDR(cons))
	;

    /* if only two lists */
    lists = CDR(lists);
    if (!CONSP(lists)) {
	RPLACD(cons, lists);

	return (list);
    }

    /* concatenate */
    for (; CONSP(CDR(lists)); lists = CDR(lists)) {
	head = CAR(lists);
	if (head == NIL)
	    continue;
	CHECK_CONS(head);
	for (tail = head; CONSP(CDR(tail)); tail = CDR(tail))
	    ;
	RPLACD(cons, head);
	cons = tail;
    }
    /* add last list */
    RPLACD(cons, CAR(lists));

    return (list);
}

LispObj *
Lisp_Nreverse(LispBuiltin *builtin)
/*
 nreverse sequence
 */
{
    return (LispXReverse(builtin, 1));
}

LispObj *
Lisp_NsetDifference(LispBuiltin *builtin)
/*
 nset-difference list1 list2 &key test test-not key
 */
{
    return (LispListSet(builtin, NSETDIFFERENCE));
}

LispObj *
Lisp_Nsubstitute(LispBuiltin *builtin)
/*
 nsubstitute newitem olditem sequence &key from-end test test-not start end count key
 */
{
    return (LispDeleteRemoveXSubstitute(builtin, NSUBSTITUTE, NONE));
}

LispObj *
Lisp_NsubstituteIf(LispBuiltin *builtin)
/*
 nsubstitute-if newitem test sequence &key from-end start end count key
 */
{
    return (LispDeleteRemoveXSubstitute(builtin, NSUBSTITUTE, IF));
}

LispObj *
Lisp_NsubstituteIfNot(LispBuiltin *builtin)
/*
 nsubstitute-if-not newitem test sequence &key from-end start end count key
 */
{
    return (LispDeleteRemoveXSubstitute(builtin, NSUBSTITUTE, IFNOT));
}

LispObj *
Lisp_Nth(LispBuiltin *builtin)
/*
 nth index list
 */
{
    long position;
    LispObj *oindex, *list;

    list = ARGUMENT(1);
    oindex = ARGUMENT(0);

    CHECK_INDEX(oindex);
    position = FIXNUM_VALUE(oindex);

    if (list == NIL)
	return (NIL);

    CHECK_CONS(list);
    for (; position > 0; position--) {
	if (!CONSP(list))
	    return (NIL);
	list = CDR(list);
    }

    return (CONSP(list) ? CAR(list) : NIL);
}

LispObj *
Lisp_Nthcdr(LispBuiltin *builtin)
/*
 nthcdr index list
 */
{
    long position;
    LispObj *oindex, *list;

    list = ARGUMENT(1);
    oindex = ARGUMENT(0);

    CHECK_INDEX(oindex);
    position = FIXNUM_VALUE(oindex);

    if (list == NIL)
	return (NIL);
    CHECK_CONS(list);

    for (; position > 0; position--) {
	if (!CONSP(list))
	    return (NIL);
	list = CDR(list);
    }

    return (list);
}

LispObj *
Lisp_NthValue(LispBuiltin *builtin)
/*
 nth-value index form
 */
{
    long i;
    LispObj *oindex, *form, *result;

    form = ARGUMENT(1);
    oindex = ARGUMENT(0);

    oindex = EVAL(oindex);
    CHECK_INDEX(oindex);
    i = FIXNUM_VALUE(oindex) - 1;

    result = EVAL(form);
    if (RETURN_COUNT < 0 || i >= RETURN_COUNT)
	result = NIL;
    else if (i >= 0)
	result = RETURN(i);

    return (result);
}

LispObj *
Lisp_Null(LispBuiltin *builtin)
/*
 null list
 */
{
    LispObj *list;

    list = ARGUMENT(0);

    return (list == NIL ? T : NIL);
}

LispObj *
Lisp_Or(LispBuiltin *builtin)
/*
 or &rest args
 */
{
    LispObj *result = NIL, *args;

    args = ARGUMENT(0);

    for (; CONSP(args); args = CDR(args)) {
	result = EVAL(CAR(args));
	if (result != NIL)
	    break;
    }

    return (result);
}

LispObj *
Lisp_Pairlis(LispBuiltin *builtin)
/*
 pairlis key data &optional alist
 */
{
    LispObj *result, *cons;

    LispObj *key, *data, *alist;

    alist = ARGUMENT(2);
    data = ARGUMENT(1);
    key = ARGUMENT(0);

    if (CONSP(key) && CONSP(data)) {
	GC_ENTER();

	result = cons = CONS(CONS(CAR(key), CAR(data)), NIL);
	GC_PROTECT(result);
	key = CDR(key);
	data = CDR(data);
	for (; CONSP(key) && CONSP(data); key = CDR(key), data = CDR(data)) {
	    RPLACD(cons, CONS(CONS(CAR(key), CAR(data)), NIL));
	    cons = CDR(cons);
	}
	if (CONSP(key) || CONSP(data))
	    LispDestroy("%s: different length lists", STRFUN(builtin));
	GC_LEAVE();
	if (alist != UNSPEC)
	    RPLACD(cons, alist);
    }
    else
	result = alist == UNSPEC ? NIL : alist;

    return (result);
}

static LispObj *
LispFindOrPosition(LispBuiltin *builtin,
		   int function, int comparison)
/*
 find item sequence &key from-end test test-not start end key
 find-if predicate sequence &key from-end start end key
 find-if-not predicate sequence &key from-end start end key
 position item sequence &key from-end test test-not start end key
 position-if predicate sequence &key from-end start end key
 position-if-not predicate sequence &key from-end start end key
 */
{
    int code = 0, istring, expect, value;
    char *string = NULL;
    long offset = -1, start, end, length, i = comparison == NONE ? 7 : 5;
    LispObj *cmp, *element, **objects = NULL;

    LispObj *item, *predicate, *sequence, *from_end,
	    *test, *test_not, *ostart, *oend, *key;

    key = ARGUMENT(i);		--i;
    oend = ARGUMENT(i);		--i;
    ostart = ARGUMENT(i);	--i;
    if (comparison == NONE) {
	test_not = ARGUMENT(i);	--i;
	test = ARGUMENT(i);	--i;
    }
    else
	test_not = test = UNSPEC;
    from_end = ARGUMENT(i);	--i;
    if (from_end == UNSPEC)
	from_end = NIL;
    sequence = ARGUMENT(i);	--i;
    if (comparison == NONE) {
	item = ARGUMENT(i);
	predicate = Oeql;
    }
    else {
	predicate = ARGUMENT(i);
	item = NIL;
    }

    LispCheckSequenceStartEnd(builtin, sequence, ostart, oend,
			      &start, &end, &length);

    if (sequence == NIL)
	return (NIL);

    /* Cannot specify both :test and :test-not */
    if (test != UNSPEC && test_not != UNSPEC)
	LispDestroy("%s: specify either :TEST or :TEST-NOT", STRFUN(builtin));

    expect = 1;
    if (comparison == NONE) {
	if (test != UNSPEC)
	    predicate = test;
	else if (test_not != UNSPEC) {
	    predicate = test_not;
	    expect = 0;
	}
	FUNCTION_CHECK(predicate);
	code = FCODE(predicate);
    }

    cmp = element = NIL;
    istring = STRINGP(sequence);
    if (istring)
	string = THESTR(sequence);
    else {
	if (!CONSP(sequence))
	    sequence = sequence->data.array.list;
	for (i = 0; i < start; i++)
	    sequence = CDR(sequence);
    }

    if ((length = end - start) == 0)
	return (NIL);

    if (from_end != NIL && !istring) {
	objects = LispMalloc(sizeof(LispObj*) * length);
	for (i = length - 1; i >= 0; i--, sequence = CDR(sequence))
	    objects[i] = CAR(sequence);
    }

    for (i = 0; i < length; i++) {
	if (istring)
	    element = SCHAR(string[from_end == NIL ? i + start : end - i - 1]);
	else
	    element = from_end == NIL ? CAR(sequence) : objects[i];

	if (key != UNSPEC)
	    cmp = APPLY1(key, element);
	else
	    cmp = element;

	/* Update list */
	if (!istring && from_end == NIL)
	    sequence = CDR(sequence);

	if (comparison == NONE)
	    value = FCOMPARE(predicate, item, cmp, code);
	else
	    value = APPLY1(predicate, cmp) != NIL;

	if ((!value &&
	     (comparison == IFNOT ||
	      (comparison == NONE && !expect))) ||
	    (value &&
	     (comparison == IF ||
	      (comparison == NONE && expect)))) {
	    offset = from_end == NIL ? i + start : end - i - 1;
	    break;
	}
    }

    if (from_end != NIL && !istring)
	LispFree(objects);

    return (offset == -1 ? NIL : function == FIND ? element : FIXNUM(offset));
}

LispObj *
Lisp_Pop(LispBuiltin *builtin)
/*
 pop place
 */
{
    LispObj *result, *value;

    LispObj *place;

    place = ARGUMENT(0);

    if (SYMBOLP(place)) {
	result = LispGetVar(place);
	if (result == NULL)
	    LispDestroy("EVAL: the variable %s is unbound", STROBJ(place));
	CHECK_CONSTANT(place);
	if (result != NIL) {
	    CHECK_CONS(result);
	    value = CDR(result);
	    result = CAR(result);
	}
	else
	    value = NIL;
	LispSetVar(place, value);
    }
    else {
	GC_ENTER();
	LispObj quote;

	result = EVAL(place);
	if (result != NIL) {
	    CHECK_CONS(result);
	    value = CDR(result);
	    GC_PROTECT(value);
	    result = CAR(result);
	}
	else
	    value = NIL;
	quote.type = LispQuote_t;
	quote.data.quote = value;
	APPLY2(Osetf, place, &quote);
	GC_LEAVE();
    }

    return (result);
}

LispObj *
Lisp_Position(LispBuiltin *builtin)
/*
 position item sequence &key from-end test test-not start end key
 */
{
    return (LispFindOrPosition(builtin, POSITION, NONE));
}

LispObj *
Lisp_PositionIf(LispBuiltin *builtin)
/*
 position-if predicate sequence &key from-end start end key
 */
{
    return (LispFindOrPosition(builtin, POSITION, IF));
}

LispObj *
Lisp_PositionIfNot(LispBuiltin *builtin)
/*
 position-if-not predicate sequence &key from-end start end key
 */
{
    return (LispFindOrPosition(builtin, POSITION, IFNOT));
}

LispObj *
Lisp_Proclaim(LispBuiltin *builtin)
/*
 proclaim declaration
 */
{
    LispObj *arguments, *object;
    char *operation;

    LispObj *declaration;

    declaration = ARGUMENT(0);

    CHECK_CONS(declaration);

    arguments = declaration;
    object = CAR(arguments);
    CHECK_SYMBOL(object);

    operation = ATOMID(object)->value;
    if (strcmp(operation, "SPECIAL") == 0) {
	for (arguments = CDR(arguments); CONSP(arguments);
	     arguments = CDR(arguments)) {
	    object = CAR(arguments);
	    CHECK_SYMBOL(object);
	    LispProclaimSpecial(object, NULL, NIL);
	}
    }
    else if (strcmp(operation, "TYPE") == 0) {
	/* XXX no type checking yet, but should be added */
    }
    /* else do nothing */

    return (NIL);
}

LispObj *
Lisp_Prog1(LispBuiltin *builtin)
/*
 prog1 first &rest body
 */
{
    GC_ENTER();
    LispObj *result;

    LispObj *first, *body;

    body = ARGUMENT(1);
    first = ARGUMENT(0);

    result = EVAL(first);

    GC_PROTECT(result);
    for (; CONSP(body); body = CDR(body))
	(void)EVAL(CAR(body));
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_Prog2(LispBuiltin *builtin)
/*
 prog2 first second &rest body
 */
{
    GC_ENTER();
    LispObj *result;

    LispObj *first, *second, *body;

    body = ARGUMENT(2);
    second = ARGUMENT(1);
    first = ARGUMENT(0);

    (void)EVAL(first);
    result = EVAL(second);
    GC_PROTECT(result);
    for (; CONSP(body); body = CDR(body))
	(void)EVAL(CAR(body));
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_Progn(LispBuiltin *builtin)
/*
 progn &rest body
 */
{
    LispObj *result = NIL;

    LispObj *body;

    body = ARGUMENT(0);

    for (; CONSP(body); body = CDR(body))
	result = EVAL(CAR(body));

    return (result);
}

/*
 *  This does what I believe is the expected behaviour (or at least
 * acceptable for the the interpreter), if the code being executed
 * ever tries to change/bind a progv symbol, the symbol state will
 * be restored when exiting the progv block, so, code like:
 *	(progv '(*x*) '(1) (defvar *x* 10))
 * when exiting the block, will have *x* unbound, and not a dynamic
 * symbol; if it was already bound, will have the old value.
 *  Symbols already dynamic can be freely changed, even unbounded in
 * the progv block.
 */
LispObj *
Lisp_Progv(LispBuiltin *builtin)
/*
 progv symbols values &rest body
 */
{
    GC_ENTER();
    int head = lisp__data.env.length, i, count, ostk[32], *offsets;
    LispObj *result, *list, *symbol, *value;
    int jumped;
    char fstk[32], *flags;
    LispBlock *block;
    LispAtom *atom;

    LispObj *symbols, *values, *body;

    /* Possible states */
#define DYNAMIC_SYMBOL		1
#define GLOBAL_SYMBOL		2
#define UNBOUND_SYMBOL		3

    body = ARGUMENT(2);
    values = ARGUMENT(1);
    symbols = ARGUMENT(0);

    /* get symbol names */
    symbols = EVAL(symbols);
    GC_PROTECT(symbols);

    /* get symbol values */
    values = EVAL(values);
    GC_PROTECT(values);

    /* count/check symbols and allocate space to remember symbol state */
    for (count = 0, list = symbols; CONSP(list); count++, list = CDR(list)) {
	symbol = CAR(list);
	CHECK_SYMBOL(symbol);
	CHECK_CONSTANT(symbol);
    }
    if (count > sizeof(fstk)) {
	flags = LispMalloc(count);
	offsets = LispMalloc(count * sizeof(int));
    }
    else {
	flags = &fstk[0];
	offsets = &ostk[0];
    }

    /* store flags and save old value if required */
    for (i = 0, list = symbols; i < count; i++, list = CDR(list)) {
	atom = CAR(list)->data.atom;
	if (atom->dyn)
	    flags[i] = DYNAMIC_SYMBOL;
	else if (atom->a_object) {
	    flags[i] = GLOBAL_SYMBOL;
	    offsets[i] = lisp__data.protect.length;
	    GC_PROTECT(atom->property->value);
	}
	else
	    flags[i] = UNBOUND_SYMBOL;
    }

    /* bind the symbols */
    for (i = 0, list = symbols; i < count; i++, list = CDR(list)) {
	symbol = CAR(list);
	atom = symbol->data.atom;
	if (CONSP(values)) {
	    value = CAR(values);
	    values = CDR(values);
	}
	else
	    value = NIL;
	if (flags[i] != DYNAMIC_SYMBOL) {
	    if (!atom->a_object)
		LispSetAtomObjectProperty(atom, value);
	    else
		SETVALUE(atom, value);
	}
	else
	    LispAddVar(symbol, value);
    }
    /* bind dynamic symbols */
    lisp__data.env.head = lisp__data.env.length;

    jumped = 0;
    result = NIL;
    block = LispBeginBlock(NIL, LispBlockProtect);
    if (setjmp(block->jmp) == 0) {
	for (; CONSP(body); body = CDR(body))
	    result = EVAL(CAR(body));
    }

    /* restore symbols */
    for (i = 0, list = symbols; i < count; i++, list = CDR(list)) {
	symbol = CAR(list);
	atom = symbol->data.atom;
	if (flags[i] != DYNAMIC_SYMBOL) {
	    if (flags[i] == UNBOUND_SYMBOL)
		LispUnsetVar(symbol);
	    else {
		/* restore global symbol value */
		LispSetAtomObjectProperty(atom, lisp__data.protect.objects
					  [offsets[i]]);
		atom->dyn = 0;
	    }
	}
    }
    /* unbind dynamic symbols */
    lisp__data.env.head = lisp__data.env.length = head;
    GC_LEAVE();

    if (count > sizeof(fstk)) {
	LispFree(flags);
	LispFree(offsets);
    }

    LispEndBlock(block);
    if (!lisp__data.destroyed) {
	if (jumped)
	    result = lisp__data.block.block_ret;
    }
    else {
	/* check if there is an unwind-protect block */
	LispBlockUnwind(NULL);

	/* no unwind-protect block, return to the toplevel */
	LispDestroy(".");
    }

    return (result);
}

LispObj *
Lisp_Provide(LispBuiltin *builtin)
/*
 provide module
 */
{
    LispObj *module, *obj;

    module = ARGUMENT(0);

    CHECK_STRING(module);
    for (obj = MOD; obj != NIL; obj = CDR(obj)) {
	if (STRLEN(CAR(obj)) == STRLEN(module) &&
	    memcmp(THESTR(CAR(obj)), THESTR(module), STRLEN(module)) == 0)
	    return (module);
    }

    if (MOD == NIL)
	MOD = CONS(module, NIL);
    else {
	RPLACD(MOD, CONS(CAR(MOD), CDR(MOD)));
	RPLACA(MOD, module);
    }

    LispSetVar(lisp__data.modules, MOD);

    return (MOD);
}

LispObj *
Lisp_Push(LispBuiltin *builtin)
/*
 push item place
 */
{
    LispObj *result, *list;

    LispObj *item, *place;

    place = ARGUMENT(1);
    item = ARGUMENT(0);

    item = EVAL(item);

    if (SYMBOLP(place)) {
	list = LispGetVar(place);
	if (list == NULL)
	    LispDestroy("EVAL: the variable %s is unbound", STROBJ(place));
	CHECK_CONSTANT(place);
	LispSetVar(place, result = CONS(item, list));
    }
    else {
	GC_ENTER();
	LispObj quote;

	list = EVAL(place);
	result = CONS(item, list);
	GC_PROTECT(result);
	quote.type = LispQuote_t;
	quote.data.quote = result;
	APPLY2(Osetf, place, &quote);
	GC_LEAVE();
    }

    return (result);
}

LispObj *
Lisp_Pushnew(LispBuiltin *builtin)
/*
 pushnew item place &key key test test-not
 */
{
    GC_ENTER();
    LispObj *result, *list;

    LispObj *item, *place, *key, *test, *test_not;

    test_not = ARGUMENT(4);
    test = ARGUMENT(3);
    key = ARGUMENT(2);
    place = ARGUMENT(1);
    item = ARGUMENT(0);

    /* Evaluate place */
    if (SYMBOLP(place)) {
	list = LispGetVar(place);
	if (list == NULL)
	    LispDestroy("EVAL: the variable %s is unbound", STROBJ(place));
	/* Do error checking now. */
	CHECK_CONSTANT(place);
    }
    else
	/* It is possible that list is not gc protected? */
	list = EVAL(place);

    item = EVAL(item);
    GC_PROTECT(item);
    if (key != UNSPEC) {
	key = EVAL(key);
	GC_PROTECT(key);
    }
    if (test != UNSPEC) {
	test = EVAL(test);
	GC_PROTECT(test);
    }
    else if (test_not != UNSPEC) {
	test_not = EVAL(test_not);
	GC_PROTECT(test_not);
    }

    result = LispAdjoin(builtin, item, list, key, test, test_not);

    /* Item already in list */
    if (result == list) {
	GC_LEAVE();

	return (result);
    }

    if (SYMBOLP(place)) {
	CHECK_CONSTANT(place);
	LispSetVar(place, result);
    }
    else {
	LispObj quote;

	GC_PROTECT(result);
	quote.type = LispQuote_t;
	quote.data.quote = result;
	APPLY2(Osetf, place, &quote);
    }
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_Quit(LispBuiltin *builtin)
/*
 quit &optional status
 */
{
    int status = 0;
    LispObj *ostatus;

    ostatus = ARGUMENT(0);

    if (FIXNUMP(ostatus))
	status = (int)FIXNUM_VALUE(ostatus);
    else if (ostatus != UNSPEC)
	LispDestroy("%s: bad exit status argument %s",
		    STRFUN(builtin), STROBJ(ostatus));

    exit(status);
}

LispObj *
Lisp_Quote(LispBuiltin *builtin)
/*
 quote object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (object);
}

LispObj *
Lisp_Replace(LispBuiltin *builtin)
/*
 replace sequence1 sequence2 &key start1 end1 start2 end2
 */
{
    long length, length1, length2, start1, end1, start2, end2;
    LispObj *sequence1, *sequence2, *ostart1, *oend1, *ostart2, *oend2;

    oend2 = ARGUMENT(5);
    ostart2 = ARGUMENT(4);
    oend1 = ARGUMENT(3);
    ostart1 = ARGUMENT(2);
    sequence2 = ARGUMENT(1);
    sequence1 = ARGUMENT(0);

    LispCheckSequenceStartEnd(builtin, sequence1, ostart1, oend1,
			      &start1, &end1, &length1);
    LispCheckSequenceStartEnd(builtin, sequence2, ostart2, oend2,
			      &start2, &end2, &length2);

    if (start1 == end1 || start2 == end2)
	return (sequence1);

    length = end1 - start1;
    if (length > end2 - start2)
	length = end2 - start2;

    if (STRINGP(sequence1)) {
	CHECK_STRING_WRITABLE(sequence1);
	if (!STRINGP(sequence2))
	    LispDestroy("%s: cannot store %s in %s",
			STRFUN(builtin), STROBJ(sequence2), THESTR(sequence1));

	memmove(THESTR(sequence1) + start1, THESTR(sequence2) + start2, length);
    }
    else {
	int i;
	LispObj *from, *to;

	if (ARRAYP(sequence1))
	    sequence1 = sequence1->data.array.list;
	if (ARRAYP(sequence2))
	    sequence2 = sequence2->data.array.list;

	/* adjust pointers */
	for (i = 0, from = sequence2; i < start2; i++, from = CDR(from))
	    ;
	for (i = 0, to = sequence1; i < start1; i++, to = CDR(to))
	    ;

	/* copy data */
	for (i = 0; i < length; i++, from = CDR(from), to = CDR(to))
	    RPLACA(to, CAR(from));
    }

    return (sequence1);
}

static LispObj *
LispDeleteOrRemoveDuplicates(LispBuiltin *builtin, int function)
/*
 delete-duplicates sequence &key from-end test test-not start end key
 remove-duplicates sequence &key from-end test test-not start end key
 */
{
    GC_ENTER();
    int code, expect, value = 0;
    long i, j, start, end, length, count;
    LispObj *lambda, *result, *cons, *compare;

    LispObj *sequence, *from_end, *test, *test_not, *ostart, *oend, *key;

    key = ARGUMENT(6);
    oend = ARGUMENT(5);
    ostart = ARGUMENT(4);
    test_not = ARGUMENT(3);
    test = ARGUMENT(2);
    from_end = ARGUMENT(1);
    if (from_end == UNSPEC)
	from_end = NIL;
    sequence = ARGUMENT(0);

    LispCheckSequenceStartEnd(builtin, sequence, ostart, oend,
			      &start, &end, &length);

    /* Check if need to do something */
    if (start == end)
	return (sequence);

    CHECK_TEST();

    /* Initialize */
    count = 0;

    result = cons = NIL;
    if (STRINGP(sequence)) {
	char *ptr, *string, *buffer = LispMalloc(length + 1);

	/* Use same code, update start/end offsets */
	if (from_end != NIL) {
	    i = length - start;
	    start = length - end;
	    end = i;
	}

	if (from_end == NIL)
	    string = THESTR(sequence);
	else {
	    /* Make a reversed copy of the sequence */
	    string = LispMalloc(length + 1);
	    for (ptr = THESTR(sequence) + length - 1, i = 0; i < length; i++)
		string[i] = *ptr--;
	    string[i] = '\0';
	}

	ptr = buffer;
	/* Copy leading bytes */
	for (i = 0; i < start; i++)
	    *ptr++ = string[i];

	compare = SCHAR(string[i]);
	if (key != UNSPEC)
	    compare = APPLY1(key, compare);
	result = cons = CONS(compare, NIL);
	GC_PROTECT(result);
	for (++i; i < end; i++) {
	    compare = SCHAR(string[i]);
	    if (key != UNSPEC)
		compare = APPLY1(key, compare);
	    RPLACD(cons, CONS(compare, NIL));
	    cons = CDR(cons);
	}

	for (i = start; i < end; i++, result = CDR(result)) {
	    compare = CAR(result);
	    for (j = i + 1, cons = CDR(result); j < end; j++, cons = CDR(cons)) {
		value = FCOMPARE(lambda, compare, CAR(cons), code);
		if (value == expect)
		    break;
	    }
	    if (value != expect)
		*ptr++ = string[i];
	    else
		++count;
	}

	if (count) {
	    /* Copy ending bytes */
	    for (; i <= length; i++)   /* Also copy the ending nul */
		*ptr++ = string[i];

	    if (from_end == NIL)
		ptr = buffer;
	    else {
		for (i = 0, ptr = buffer + strlen(buffer);
		     ptr > buffer;
		     i++)
		    string[i] = *--ptr;
		string[i] = '\0';
		ptr = string;
		LispFree(buffer);
	    }
	    if (function == REMOVE)
		result = STRING2(ptr);
	    else {
		CHECK_STRING_WRITABLE(sequence);
		result = sequence;
		free(THESTR(result));
		THESTR(result) = ptr;
		LispMused(ptr);
	    }
	}
	else {
	    result = sequence;
	    if (from_end != NIL)
		LispFree(string);
	}
    }
    else {
	long xlength = end - start;
	LispObj *list, *object, **kobjects = NULL, **xobjects;
	LispObj **objects = LispMalloc(sizeof(LispObj*) * xlength);

	if (!CONSP(sequence))
	    object = sequence->data.array.list;
	else
	    object = sequence;
	list = object;

	for (i = 0; i < start; i++)
	    object = CDR(object);

	/* Put data in a vector */
	if (from_end == NIL) {
	    for (i = 0; i < xlength; i++, object = CDR(object))
		objects[i] = CAR(object);
	}
	else {
	    for (i = xlength - 1; i >= 0; i--, object = CDR(object))
		objects[i] = CAR(object);
	}

	/* Apply key predicate if required */
	if (key != UNSPEC) {
	    kobjects = LispMalloc(sizeof(LispObj*) * xlength);
	    for (i = 0; i < xlength; i++) {
		kobjects[i] = APPLY1(key, objects[i]);
		GC_PROTECT(kobjects[i]);
	    }
	    xobjects = kobjects;
	}
	else
	    xobjects = objects;

	/* Check if needs to remove something */
	for (i = 0; i < xlength; i++) {
	    compare = xobjects[i];
	    for (j = i + 1; j < xlength; j++) {
		value = FCOMPARE(lambda, compare, xobjects[j], code);
		if (value == expect) {
		    objects[i] = NULL;
		    ++count;
		    break;
		}
	    }
	}

	if (count) {
	    /* Create/set result list */
	    object = list;

	    if (start) {
		/* Skip first elements of resulting list */
		if (function == REMOVE) {
		    result = cons = CONS(CAR(object), NIL);
		    GC_PROTECT(result);
		    for (i = 1, object = CDR(object);
			 i < start;
			 i++, object = CDR(object)) {
			RPLACD(cons, CONS(CAR(object), NIL));
			cons = CDR(cons);
		    }
		}
		else {
		    result = cons = object;
		    for (i = 1; i < start; i++, cons = CDR(cons))
			;
		}
	    }
	    else if (function == DELETE)
		result = list;

	    /* Skip initial removed elements */
	    if (function == REMOVE) {
		for (i = 0; objects[i] == NULL && i < xlength; i++)
		    ;
	    }
	    else
		i = 0;

	    if (i < xlength) {
		int xstart, xlimit, xinc;

		if (from_end == NIL) {
		    xstart = i;
		    xlimit = xlength;
		    xinc = 1;
		}
		else {
		    xstart = xlength - 1;
		    xlimit = i - 1;
		    xinc = -1;
		}

		if (function == REMOVE) {
		    for (i = xstart; i != xlimit; i += xinc) {
			if (objects[i] != NULL) {
			    if (result == NIL) {
				result = cons = CONS(objects[i], NIL);
				GC_PROTECT(result);
			    }
			    else {
				RPLACD(cons, CONS(objects[i], NIL));
				cons = CDR(cons);
			    }
			}
		    }
		}
		else {
		    /* Delete duplicates */
		    for (i = xstart; i != xlimit; i += xinc) {
			if (objects[i] == NULL) {
			    if (cons == NIL) {
				if (CONSP(CDR(result))) {
				    RPLACA(result, CADR(result));
				    RPLACD(result, CDDR(result));
				}
				else {
				    RPLACA(result, CDR(result));
				    RPLACD(result, NIL);
				}
			    }
			    else {
				if (CONSP(CDR(cons)))
				    RPLACD(cons, CDDR(cons));
				else
				    RPLACD(cons, NIL);
			    }
			}
			else {
			    if (cons == NIL)
				cons = result;
			    else
				cons = CDR(cons);
			}
		    }
		}
	    }
	    if (end < length && function == REMOVE) {
		for (i = start; i < end; i++, object = CDR(object))
		    ;
		if (result == NIL) {
		    result = cons = CONS(CAR(object), NIL);
		    GC_PROTECT(result);
		    ++i;
		    object = CDR(object);
		}
		for (; i < length; i++, object = CDR(object)) {
		    RPLACD(cons, CONS(CAR(object), NIL));
		    cons = CDR(cons);
		}
	    }
	}
	else
	    result = sequence;
	LispFree(objects);
	if (key != UNSPEC)
	    LispFree(kobjects);

	if (count && !CONSP(sequence)) {
	    if (function == REMOVE)
		result = VECTOR(result);
	    else {
		length = FIXNUM_VALUE(CAR(sequence->data.array.dim)) - count;
		CAR(sequence->data.array.dim) = FIXNUM(length);
		result = sequence;
	    }
	}
    }
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_RemoveDuplicates(LispBuiltin *builtin)
/*
 remove-duplicates sequence &key from-end test test-not start end key
 */
{
    return (LispDeleteOrRemoveDuplicates(builtin, REMOVE));
}

static LispObj *
LispDeleteRemoveXSubstitute(LispBuiltin *builtin,
			    int function, int comparison)
/*
 delete item sequence &key from-end test test-not start end count key
 delete-if predicate sequence &key from-end start end count key
 delete-if-not predicate sequence &key from-end start end count key
 remove item sequence &key from-end test test-not start end count key
 remove-if predicate sequence &key from-end start end count key
 remove-if-not predicate sequence &key from-end start end count key
 substitute newitem olditem sequence &key from-end test test-not start end count key
 substitute-if newitem test sequence &key from-end start end count key
 substitute-if-not newitem test sequence &key from-end start end count key
 nsubstitute newitem olditem sequence &key from-end test test-not start end count key
 nsubstitute-if newitem test sequence &key from-end start end count key
 nsubstitute-if-not newitem test sequence &key from-end start end count key
 */
{
    GC_ENTER();
    int code, expect, value, inplace, substitute;
    long i, j, start, end, length, copy, count, xstart, xend, xinc, xlength;

    LispObj *result, *compare;

    LispObj *item, *newitem, *lambda, *sequence, *from_end,
	    *test, *test_not, *ostart, *oend, *ocount, *key;

    substitute = function == SUBSTITUTE || function == NSUBSTITUTE;
    if (!substitute)
	i = comparison == NONE ? 8 : 6;
    else /* substitute */
	i = comparison == NONE ? 9 : 7;

    /* Get function arguments */
    key = ARGUMENT(i);			--i;
    ocount = ARGUMENT(i);		--i;
    oend = ARGUMENT(i);			--i;
    ostart = ARGUMENT(i);		--i;
    if (comparison == NONE) {
	test_not = ARGUMENT(i);		--i;
	test = ARGUMENT(i);		--i;
    }
    else
	test_not = test = UNSPEC;
    from_end = ARGUMENT(i);		--i;
    if (from_end == UNSPEC)
	from_end = NIL;
    sequence = ARGUMENT(i);		--i;
    if (comparison != NONE) {
	lambda = ARGUMENT(i);	--i;
	if (substitute)
	    newitem = ARGUMENT(0);
	else
	    newitem = NIL;
	item = NIL;
    }
    else {
	lambda = NIL;
	if (substitute) {
	    item = ARGUMENT(1);
	    newitem = ARGUMENT(0);
	}
	else {
	    item = ARGUMENT(0);
	    newitem = NIL;
	}
    }

    /* Check if argument is a valid sequence, and if start/end
     * are correctly specified. */
    LispCheckSequenceStartEnd(builtin, sequence, ostart, oend,
			      &start, &end, &length);

    /* Check count argument */
    if (ocount == UNSPEC) {
	count = length;
	/* Doesn't matter, but left to right should be slightly faster */
	from_end = NIL;
    }
    else {
	CHECK_INDEX(ocount);
	count = FIXNUM_VALUE(ocount);
    }

    /* Check if need to do something */
    if (start == end || count == 0)
	return (sequence);

    CHECK_TEST_0();

    /* Resolve comparison function, and expected result of comparison */
    if (comparison == NONE) {
	if (test_not == UNSPEC) {
	    if (test == UNSPEC)
		lambda = Oeql;
	    else
		lambda = test;
	    expect = 1;
	}
	else {
	    lambda = test_not;
	    expect = 0;
	}
	FUNCTION_CHECK(lambda);
    }
    else
	expect = comparison == IFNOT ? 0 : 1;

    /* Check for fast path to comparison function */
    code = FCODE(lambda);

    /* Initialize for loop */
    copy = count;
    result = sequence;
    inplace = function == DELETE || function == NSUBSTITUTE;
    xlength = end - start;

    /* String is easier */
    if (STRINGP(sequence)) {
	char *buffer, *string;

	if (comparison == NONE) {
	    CHECK_SCHAR(item);
	}
	if (substitute) {
	    CHECK_SCHAR(newitem);
	}

	if (from_end == NIL) {
	    xstart = start;
	    xend = end;
	    xinc = 1;
	}
	else {
	    xstart = end - 1;
	    xend = start - 1;
	    xinc = -1;
	}

	string = THESTR(sequence);
	buffer = LispMalloc(length + 1);

	/* Copy leading bytes, if any */
	for (i = 0; i < start; i++)
	    buffer[i] = string[i];

	for (j = xstart; i != xend && count > 0; i += xinc) {
	    compare = SCHAR(string[i]);
	    if (key != UNSPEC) {
		compare = APPLY1(key, compare);
		/* Value returned by the key predicate may not be protected */
		GC_PROTECT(compare);
		if (comparison == NONE)
		    value = FCOMPARE(lambda, item, compare, code);
		else
		    value = APPLY1(lambda, compare) != NIL;
		/* Unprotect value returned by the key predicate */
		GC_LEAVE();
	    }
	    else {
		if (comparison == NONE)
		    value = FCOMPARE(lambda, item, compare, code);
		else
		    value = APPLY1(lambda, compare) != NIL;
	    }

	    if (value != expect) {
		buffer[j] = string[i];
		j += xinc;
	    }
	    else {
		if (substitute) {
		    buffer[j] = SCHAR_VALUE(newitem);
		    j += xinc;
		}
		else
		    --count;
	    }
	}

	if (count != copy && from_end != NIL)
	    memmove(buffer + start, buffer + copy - count, count);

	/* Copy remaining bytes, if any */
	for (; i < length; i++, j++)
	    buffer[j] = string[i];
	buffer[j] = '\0';

	xlength = length - (copy - count);
	if (inplace) {
	    CHECK_STRING_WRITABLE(sequence);
	    /* result is a pointer to sequence */
	    LispFree(THESTR(sequence));
	    LispMused(buffer);
	    THESTR(sequence) = buffer;
	    STRLEN(sequence) = xlength;
	}
	else
	    result = LSTRING2(buffer, xlength);
    }

    /* If inplace, need to update CAR and CDR of sequence */
    else {
	LispObj *list, *object;
	LispObj **objects = LispMalloc(sizeof(LispObj*) * xlength);

	if (!CONSP(sequence))
	    list = sequence->data.array.list;
	else
	    list = sequence;

	/* Put data in a vector */
	for (i = 0, object = list; i < start; i++)
	    object = CDR(object);

	for (i = 0; i < xlength; i++, object = CDR(object))
	    objects[i] = CAR(object);

	if (from_end == NIL) {
	    xstart = 0;
	    xend = xlength;
	    xinc = 1;
	}
	else {
	    xstart = xlength - 1;
	    xend = -1;
	    xinc = -1;
	}

	/* Check if needs to remove something */
	for (i = xstart; i != xend && count > 0; i += xinc) {
	    compare = objects[i];
	    if (key != UNSPEC) {
		compare = APPLY1(key, compare);
		GC_PROTECT(compare);
		if (comparison == NONE)
		    value = FCOMPARE(lambda, item, compare, code);
		else
		    value = APPLY1(lambda, compare) != NIL;
		GC_LEAVE();
	    }
	    else {
		if (comparison == NONE)
		    value = FCOMPARE(lambda, item, compare, code);
		else
		    value = APPLY1(lambda, compare) != NIL;
	    }
	    if (value == expect) {
		if (substitute)
		    objects[i] = newitem;
		else
		    objects[i] = NULL;
		--count;
	    }
	}

	if (copy != count) {
	    LispObj *cons = NIL;

	    i = 0;
	    object = list;
	    if (inplace) {
		/* While result is NIL, skip initial elements of sequence */
		result = start ? list : NIL;

		/* Skip initial elements, if any */
		for (; i < start; i++, cons = object, object = CDR(object))
		    ;
	    }
	    /* Copy initial elements, if any */
	    else {
		result = NIL;
		if (start) {
		    result = cons = CONS(CAR(list), NIL);
		    GC_PROTECT(result);
		    for (++i, object = CDR(list);
			 i < start;
			 i++, object = CDR(object)) {
			RPLACD(cons, CONS(CAR(object), NIL));
		 	cons = CDR(cons);
		    }
		}
	    }

	    /* Skip initial removed elements, if any */
	    for (i = 0; i < xlength && objects[i] == NULL; i++)
		;

	    for (i = 0; i < xlength; i++, object = CDR(object)) {
		if (objects[i]) {
		    if (inplace) {
			if (result == NIL)
			    result = cons = object;
			else {
			    RPLACD(cons, object);
			    cons = CDR(cons);
			}
			if (function == NSUBSTITUTE)
			    RPLACA(cons, objects[i]);
		    }
		    else {
			if (result == NIL) {
			    result = cons = CONS(objects[i], NIL);
			    GC_PROTECT(result);
			}
			else {
			    RPLACD(cons, CONS(objects[i], NIL));
			    cons = CDR(cons);
			}
		    }
		}
	    }

	    if (inplace) {
		if (result == NIL)
		    result = object;
		else
		    RPLACD(cons, object);

		if (!CONSP(sequence)) {
		    result = sequence;
		    CAR(result)->data.array.dim =
			FIXNUM(length - (copy - count));
		}
	    }
	    else if (end < length) {
		i = end;
		/* Copy ending elements, if any */
		if (result == NIL) {
		    result = cons = CONS(CAR(object), NIL);
		    GC_PROTECT(result);
		    object = CDR(object);
		    i++;
		}
		for (; i < length; i++, object = CDR(object)) {
		    RPLACD(cons, CONS(CAR(object), NIL));
		    cons = CDR(cons);
		}
	    }
	}

	/* Release comparison vector */
	LispFree(objects);
    }

    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_Remove(LispBuiltin *builtin)
/*
 remove item sequence &key from-end test test-not start end count key
 */
{
    return (LispDeleteRemoveXSubstitute(builtin, REMOVE, NONE));
}

LispObj *
Lisp_RemoveIf(LispBuiltin *builtin)
/*
 remove-if predicate sequence &key from-end start end count key
 */
{
    return (LispDeleteRemoveXSubstitute(builtin, REMOVE, IF));
}

LispObj *
Lisp_RemoveIfNot(LispBuiltin *builtin)
/*
 remove-if-not predicate sequence &key from-end start end count key
 */
{
    return (LispDeleteRemoveXSubstitute(builtin, REMOVE, IFNOT));
}

LispObj *
Lisp_Remprop(LispBuiltin *builtin)
/*
 remprop symbol indicator
 */
{
    LispObj *symbol, *indicator;

    indicator = ARGUMENT(1);
    symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);

    return (LispRemAtomProperty(symbol->data.atom, indicator));
}

LispObj *
Lisp_Return(LispBuiltin *builtin)
/*
 return &optional result
 */
{
    unsigned blevel = lisp__data.block.block_level;

    LispObj *result;

    result = ARGUMENT(0);

    while (blevel) {
	LispBlock *block = lisp__data.block.block[--blevel];

	if (block->type == LispBlockClosure)
	    /* if reached a function call */
	    break;
	if (block->type == LispBlockTag && block->tag == NIL) {
	    lisp__data.block.block_ret = result == UNSPEC ? NIL : EVAL(result);
	    LispBlockUnwind(block);
	    BLOCKJUMP(block);
	}
    }
    LispDestroy("%s: no visible NIL block", STRFUN(builtin));

    /*NOTREACHED*/
    return (NIL);
}

LispObj *
Lisp_ReturnFrom(LispBuiltin *builtin)
/*
 return-from name &optional result
 */
{
    unsigned blevel = lisp__data.block.block_level;

    LispObj *name, *result;

    result = ARGUMENT(1);
    name = ARGUMENT(0);

    if (name != NIL && name != T && !SYMBOLP(name))
	LispDestroy("%s: %s is not a valid block name",
		    STRFUN(builtin), STROBJ(name));

    while (blevel) {
	LispBlock *block = lisp__data.block.block[--blevel];

	if (name == block->tag &&
	    (block->type == LispBlockTag || block->type == LispBlockClosure)) {
	    lisp__data.block.block_ret = result == UNSPEC ? NIL : EVAL(result);
	    LispBlockUnwind(block);
	    BLOCKJUMP(block);
	}
	if (block->type == LispBlockClosure)
	    /* can use return-from only in the current function */
	    break;
    }
    LispDestroy("%s: no visible block named %s",
		STRFUN(builtin), STROBJ(name));

    /*NOTREACHED*/
    return (NIL);
}

static LispObj *
LispXReverse(LispBuiltin *builtin, int inplace)
/*
 nreverse sequence
 reverse sequence
 */
{
    long length;
    LispObj *list, *result = NIL;

    LispObj *sequence;

    sequence = ARGUMENT(0);

    /* Do error checking for arrays and object type. */
    length = LispLength(sequence);
    if (length <= 1)
	return (sequence);

    switch (XOBJECT_TYPE(sequence)) {
	case LispString_t: {
	    long i;
	    char *from, *to;

	    from = THESTR(sequence) + length - 1;
	    if (inplace) {
		char temp;

		CHECK_STRING_WRITABLE(sequence);
		to = THESTR(sequence);
		for (i = 0; i < length / 2; i++) {
		    temp = to[i];
		    to[i] = from[-i];
		    from[-i] = temp;
		}
		result = sequence;
	    }
	    else {
		to = LispMalloc(length + 1);
		to[length] = '\0';
		for (i = 0; i < length; i++)
		    to[i] = from[-i];
		result = STRING2(to);
	    }
	}   return (result);
	case LispCons_t:
	    if (inplace) {
		long i, j;
		LispObj *temp;

		/* For large lists this can be very slow, but for small
		 * amounts of data, this avoid allocating a buffer to
		 * to store the CAR of the sequence. This is only done
		 * to not destroy the contents of a variable.
		 */
		for (i = 0, list = sequence;
		     i < (length + 1) / 2;
		     i++, list = CDR(list))
		    ;
		length /= 2;
		for (i = 0; i < length; i++, list = CDR(list)) {
		    for (j = length - i - 1, result = sequence;
			 j > 0;
			 j--, result = CDR(result))
			;
		    temp = CAR(list);
		    RPLACA(list, CAR(result));
		    RPLACA(result, temp);
		}
		return (sequence);
	    }
	    list = sequence;
	    break;
	case LispArray_t:
	    if (inplace) {
		sequence->data.array.list =
		    LispReverse(sequence->data.array.list);
		return (sequence);
	    }
	    list = sequence->data.array.list;
	    break;
	default:	/* LispNil_t */
	    return (result);
    }

    {
	GC_ENTER();
	LispObj *cons;

	result = cons = CONS(CAR(list), NIL);
	GC_PROTECT(result);
	for (list = CDR(list); CONSP(list); list = CDR(list)) {
	    RPLACD(cons, CONS(CAR(list), NIL));
	    cons = CDR(cons);
	}
	result = LispReverse(result);

	GC_LEAVE();
    }

    if (ARRAYP(sequence)) {
	list = result;

	result = LispNew(list, NIL);
	result->type = LispArray_t;
	result->data.array.list = list;
	result->data.array.dim = sequence->data.array.dim;
	result->data.array.rank = sequence->data.array.rank;
	result->data.array.type = sequence->data.array.type;
	result->data.array.zero = sequence->data.array.zero;
    }

    return (result);
}

LispObj *
Lisp_Reverse(LispBuiltin *builtin)
/*
 reverse sequence
 */
{
    return (LispXReverse(builtin, 0));
}

LispObj *
Lisp_Rplaca(LispBuiltin *builtin)
/*
 rplaca place value
 */
{
    LispObj *place, *value;

    value = ARGUMENT(1);
    place = ARGUMENT(0);

    CHECK_CONS(place);
    RPLACA(place, value);

    return (place);
}

LispObj *
Lisp_Rplacd(LispBuiltin *builtin)
/*
 rplacd place value
 */
{
    LispObj *place, *value;

    value = ARGUMENT(1);
    place = ARGUMENT(0);

    CHECK_CONS(place);
    RPLACD(place, value);

    return (place);
}

LispObj *
Lisp_Search(LispBuiltin *builtin)
/*
 search sequence1 sequence2 &key from-end test test-not key start1 start2 end1 end2
 */
{
    int code = 0, expect, value;
    long start1, start2, end1, end2, length1, length2, off1, off2, offset = -1;
    LispObj *cmp1, *cmp2, *list1 = NIL, *lambda;
    SeqInfo seq1, seq2;

    LispObj *sequence1, *sequence2, *from_end, *test, *test_not,
	    *key, *ostart1, *ostart2, *oend1, *oend2;

    oend2 = ARGUMENT(9);
    oend1 = ARGUMENT(8);
    ostart2 = ARGUMENT(7);
    ostart1 = ARGUMENT(6);
    key = ARGUMENT(5);
    test_not = ARGUMENT(4);
    test = ARGUMENT(3);
    from_end = ARGUMENT(2);
    sequence2 = ARGUMENT(1);
    sequence1 = ARGUMENT(0);

    LispCheckSequenceStartEnd(builtin, sequence1, ostart1, oend1,
			      &start1, &end1, &length1);
    LispCheckSequenceStartEnd(builtin, sequence2, ostart2, oend2,
			      &start2, &end2, &length2);

    /* Check for special conditions */
    if (start1 == end1)
	return (FIXNUM(end2));
    else if (start2 == end2)
	return (start1 == end1 ? FIXNUM(start2) : NIL);

    CHECK_TEST();

    if (from_end == UNSPEC)
	from_end = NIL;

    SETSEQ(seq1, sequence1);
    SETSEQ(seq2, sequence2);

    length1 = end1 - start1;
    length2 = end2 - start2;

    /* update start of sequences */
    if (start1) {
	if (seq1.type == LispString_t)
	    seq1.data.string += start1;
	else {
	    for (cmp1 = seq1.data.list; start1; cmp1 = CDR(cmp1), --start1)
		;
	    seq1.data.list = cmp1;
	}
	end1 = length1;
    }
    if (start2) {
	if (seq2.type == LispString_t)
	    seq2.data.string += start2;
	else {
	    for (cmp2 = seq2.data.list; start2; cmp2 = CDR(cmp2), --start2)
		;
	    seq2.data.list = cmp2;
	}
	end2 = length2;
    }

    /* easier case */
    if (from_end == NIL) {
	LispObj *list2 = NIL;

	/* while a match is possible */
	while (end2 - start2 >= length1) {

	    /* prepare to search */
	    off1 = 0;
	    off2 = start2;
	    if (seq1.type != LispString_t)
		list1 = seq1.data.list;
	    if (seq2.type != LispString_t)
		list2 = seq2.data.list;

	    /* for every element that must match in sequence1 */
	    while (off1 < length1) {
		if (seq1.type == LispString_t)
		    cmp1 = SCHAR(seq1.data.string[off1]);
		else
		    cmp1 = CAR(list1);
		if (seq2.type == LispString_t)
		    cmp2 = SCHAR(seq2.data.string[off2]);
		else
		    cmp2 = CAR(list2);
		if (key != UNSPEC) {
		    cmp1 = APPLY1(key, cmp1);
		    cmp2 = APPLY1(key, cmp2);
		}

		/* compare elements */
		value = FCOMPARE(lambda, cmp1, cmp2, code);
		if (value != expect)
		    break;

		/* update offsets/sequence pointers */
		++off1;
		++off2;
		if (seq1.type != LispString_t)
		    list1 = CDR(list1);
		if (seq2.type != LispString_t)
		    list2 = CDR(list2);
	    }

	    /* if everything matched */
	    if (off1 == end1) {
		offset = off2 - length1;
		break;
	    }

	    /* update offset/sequence2 pointer */
	    ++start2;
	    if (seq2.type != LispString_t)
		seq2.data.list = CDR(seq2.data.list);
	}
    }
    else {
	/* allocate vector if required, only list2 requires it.
	 * list1 can be traversed forward */
	if (seq2.type != LispString_t) {
	    cmp2 = seq2.data.list;
	    seq2.data.vector = LispMalloc(sizeof(LispObj*) * length2);
	    for (off2 = 0; off2 < end2; off2++, cmp2 = CDR(cmp2))
		seq2.data.vector[off2] = CAR(cmp2);
	}

	/* while a match is possible */
	while (end2 >= length1) {

	    /* prepare to search */
	    off1 = 0;
	    off2 = end2 - length1;
	    if (seq1.type != LispString_t)
		list1 = seq1.data.list;

	    /* for every element that must match in sequence1 */
	    while (off1 < end1) {
		if (seq1.type == LispString_t)
		    cmp1 = SCHAR(seq1.data.string[off1]);
		else
		    cmp1 = CAR(list1);
		if (seq2.type == LispString_t)
		    cmp2 = SCHAR(seq2.data.string[off2]);
		else
		    cmp2 = seq2.data.vector[off2];
		if (key != UNSPEC) {
		    cmp1 = APPLY1(key, cmp1);
		    cmp2 = APPLY1(key, cmp2);
		}

		/* Compare elements */
		value = FCOMPARE(lambda, cmp1, cmp2, code);
		if (value != expect)
		    break;

		/* Update offsets */
		++off1;
		++off2;
		if (seq1.type != LispString_t)
		    list1 = CDR(list1);
	    }

	    /* If all elements matched */
	    if (off1 == end1) {
		offset = off2 - length1;
		break;
	    }

	    /* Update offset */
	    --end2;
	}

	if (seq2.type != LispString_t)
	    LispFree(seq2.data.vector);
    }

    return (offset == -1 ? NIL : FIXNUM(offset));
}

/*
 * ext::getenv
 */
LispObj *
Lisp_Setenv(LispBuiltin *builtin)
/*
 setenv name value &optional overwrite
 */
{
    char *name, *value;

    LispObj *oname, *ovalue, *overwrite;

    overwrite = ARGUMENT(2);
    ovalue = ARGUMENT(1);
    oname = ARGUMENT(0);

    CHECK_STRING(oname);
    name = THESTR(oname);

    CHECK_STRING(ovalue);
    value = THESTR(ovalue);

    setenv(name, value, overwrite != UNSPEC && overwrite != NIL);
    value = getenv(name);

    return (value ? STRING(value) : NIL);
}

LispObj *
Lisp_Set(LispBuiltin *builtin)
/*
 set symbol value
 */
{
    LispAtom *atom;
    LispObj *symbol, *value;

    value = ARGUMENT(1);
    symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);
    atom = symbol->data.atom;
    if (atom->dyn)
	LispSetVar(symbol, value);
    else if (atom->watch || !atom->a_object)
	LispSetAtomObjectProperty(atom, value);
    else {
	CHECK_CONSTANT(symbol);
	SETVALUE(atom, value);
    }

    return (value);
}

LispObj *
Lisp_SetDifference(LispBuiltin *builtin)
/*
 set-difference list1 list2 &key test test-not key
 */
{
    return (LispListSet(builtin, SETDIFFERENCE));
}

LispObj *
Lisp_SetExclusiveOr(LispBuiltin *builtin)
/*
 set-exclusive-or list1 list2 &key test test-not key
 */
{
    return (LispListSet(builtin, SETEXCLUSIVEOR));
}

LispObj *
Lisp_NsetExclusiveOr(LispBuiltin *builtin)
/*
 nset-exclusive-or list1 list2 &key test test-not key
 */
{
    return (LispListSet(builtin, NSETEXCLUSIVEOR));
}

LispObj *
Lisp_SetQ(LispBuiltin *builtin)
/*
 setq &rest form
 */
{
    LispObj *result, *variable, *form;

    form = ARGUMENT(0);

    result = NIL;
    for (; CONSP(form); form = CDR(form)) {
	variable = CAR(form);
	CHECK_SYMBOL(variable);
	CHECK_CONSTANT(variable);
	form = CDR(form);
	if (!CONSP(form))
	    LispDestroy("%s: odd number of arguments", STRFUN(builtin));
	result = EVAL(CAR(form));
	LispSetVar(variable, result);
    }

    return (result);
}

LispObj *
Lisp_Psetq(LispBuiltin *builtin)
/*
 psetq &rest form
 */
{
    GC_ENTER();
    int base = gc__protect;
    LispObj *value, *symbol, *list, *form;

    form = ARGUMENT(0);

    /* parallel setq, first pass evaluate values and basic error checking */
    for (list = form; CONSP(list); list = CDR(list)) {
	symbol = CAR(list);
	CHECK_SYMBOL(symbol);
	list = CDR(list);
	if (!CONSP(list))
	    LispDestroy("%s: odd number of arguments", STRFUN(builtin));
	value = EVAL(CAR(list));
	GC_PROTECT(value);
    }

    /* second pass, assign values */
    for (; CONSP(form); form = CDDR(form)) {
	symbol = CAR(form);
	CHECK_CONSTANT(symbol);
	LispSetVar(symbol, lisp__data.protect.objects[base++]);
    }
    GC_LEAVE();

    return (NIL);
}

LispObj *
Lisp_Setf(LispBuiltin *builtin)
/*
 setf &rest form
 */
{
    LispAtom *atom;
    LispObj *setf, *place, *value, *result = NIL, *data;

    LispObj *form;

    form = ARGUMENT(0);

    for (; CONSP(form); form = CDR(form)) {
	place = CAR(form);
	form = CDR(form);
	if (!CONSP(form))
	    LispDestroy("%s: odd number of arguments", STRFUN(builtin));
	value = CAR(form);

	if (!POINTERP(place))
	    goto invalid_place;
	if (XSYMBOLP(place)) {
	    CHECK_CONSTANT(place);
	    result = EVAL(value);
	    (void)LispSetVar(place, result);
	}
	else if (XCONSP(place)) {
	    /* it really should not be required to protect any object
	     * evaluated here, but is done for safety in case one of
	     * the evaluated forms returns data not gc protected, what
	     * could cause surprises if the object is garbage collected
	     * before finishing setf. */
	    GC_ENTER();

	    setf = CAR(place);
	    if (!SYMBOLP(setf))
		goto invalid_place;
	    if (!CONSP(CDR(place)))
		goto invalid_place;

	    value = EVAL(value);
	    GC_PROTECT(value);

	    atom = setf->data.atom;
	    if (atom->a_defsetf == 0) {
		if (atom->a_defstruct &&
		    atom->property->structure.function >= 0) {
		    /* Use a default setf method for the structure field, as
		     * if this definition have been done
		     *	(defsetf THE-STRUCT-FIELD (struct) (value)
		     *	 `(lisp::struct-store 'THE-STRUCT-FIELD ,struct ,value))
		     */
		    place = CDR(place);
		    data = CAR(place);
		    if (CONSP(CDR(place)))
			goto invalid_place;
		    data = EVAL(data);
		    GC_PROTECT(data);
		    result = APPLY3(Ostruct_store, setf, data, value);
		    GC_LEAVE();
		    continue;
		}
		/* Must also expand macros */
		else if (atom->a_function &&
			 atom->property->fun.function->funtype == LispMacro) {
		    result = LispRunSetfMacro(atom, CDR(place), value);
		    continue;
		}
		goto invalid_place;
	    }

	    place = CDR(place);
	    setf = setf->data.atom->property->setf;
	    if (SYMBOLP(setf)) {
		LispObj *arguments, *cons;

		if (!CONSP(CDR(place))) {
		    arguments = EVAL(CAR(place));
		    GC_PROTECT(arguments);
		    result = APPLY2(setf, arguments, value);
		}
		else if (!CONSP(CDDR(place))) {
		    arguments = EVAL(CAR(place));
		    GC_PROTECT(arguments);
		    cons = EVAL(CADR(place));
		    GC_PROTECT(cons);
		    result = APPLY3(setf, arguments, cons, value);
		}
		else {
		    arguments = cons = CONS(EVAL(CAR(place)), NIL);
		    GC_PROTECT(arguments);
		    for (place = CDR(place); CONSP(place); place = CDR(place)) {
			RPLACD(cons, CONS(EVAL(CAR(place)), NIL));
			cons = CDR(cons);
		    }
		    RPLACD(cons, CONS(value, NIL));
		    result = APPLY(setf, arguments);
		}
	    }
	    else
		result = LispRunSetf(atom->property->salist, setf, place, value);
	    GC_LEAVE();
	}
	else
	    goto invalid_place;
    }

    return (result);
invalid_place:
    LispDestroy("%s: %s is an invalid place", STRFUN(builtin), STROBJ(place));
    /*NOTREACHED*/
    return (NIL);
}

LispObj *
Lisp_Psetf(LispBuiltin *builtin)
/*
 psetf &rest form
 */
{
    int base;
    GC_ENTER();
    LispAtom *atom;
    LispObj *setf, *place = NIL, *value, *data;

    LispObj *form;

    form = ARGUMENT(0);

    /* parallel setf, first pass evaluate values and basic error checking */
    base = gc__protect;
    for (setf = form; CONSP(setf); setf = CDR(setf)) {
	if (!POINTERP(CAR(setf)))
	    goto invalid_place;
	setf = CDR(setf);
	if (!CONSP(setf))
	    LispDestroy("%s: odd number of arguments", STRFUN(builtin));
	value = EVAL(CAR(setf));
	GC_PROTECT(value);
    }

    /* second pass, assign values */
    for (; CONSP(form); form = CDDR(form)) {
	place = CAR(form);
	value = lisp__data.protect.objects[base++];

	if (XSYMBOLP(place)) {
	    CHECK_CONSTANT(place);
	    (void)LispSetVar(place, value);
	}
	else if (XCONSP(place)) {
	    LispObj *arguments, *cons;
	    int xbase = lisp__data.protect.length;

	    setf = CAR(place);
	    if (!SYMBOLP(setf))
		goto invalid_place;
	    if (!CONSP(CDR(place)))
		goto invalid_place;

	    atom = setf->data.atom;
	    if (atom->a_defsetf == 0) {
		if (atom->a_defstruct &&
		    atom->property->structure.function >= 0) {
		    place = CDR(place);
		    data = CAR(place);
		    if (CONSP(CDR(place)))
			goto invalid_place;
		    data = EVAL(data);
		    GC_PROTECT(data);
		    (void)APPLY3(Ostruct_store, setf, data, value);
		    lisp__data.protect.length = xbase;
		    continue;
		}
		else if (atom->a_function &&
			 atom->property->fun.function->funtype == LispMacro) {
		    (void)LispRunSetfMacro(atom, CDR(place), value);
		    lisp__data.protect.length = xbase;
		    continue;
		}
		goto invalid_place;
	    }

	    place = CDR(place);
	    setf = setf->data.atom->property->setf;
	    if (SYMBOLP(setf)) {
		if (!CONSP(CDR(place))) {
		    arguments = EVAL(CAR(place));
		    GC_PROTECT(arguments);
		    (void)APPLY2(setf, arguments, value);
		}
		else if (!CONSP(CDDR(place))) {
		    arguments = EVAL(CAR(place));
		    GC_PROTECT(arguments);
		    cons = EVAL(CADR(place));
		    GC_PROTECT(cons);
		    (void)APPLY3(setf, arguments, cons, value);
		}
		else {
		    arguments = cons = CONS(EVAL(CAR(place)), NIL);
		    GC_PROTECT(arguments);
		    for (place = CDR(place); CONSP(place); place = CDR(place)) {
			RPLACD(cons, CONS(EVAL(CAR(place)), NIL));
			cons = CDR(cons);
		    }
		    RPLACD(cons, CONS(value, NIL));
		    (void)APPLY(setf, arguments);
		}
		lisp__data.protect.length = xbase;
	    }
	    else
		(void)LispRunSetf(atom->property->salist, setf, place, value);
	}
	else
	    goto invalid_place;
    }
    GC_LEAVE();

    return (NIL);
invalid_place:
    LispDestroy("%s: %s is an invalid place", STRFUN(builtin), STROBJ(place));
    /*NOTREACHED*/
    return (NIL);
}

LispObj *
Lisp_Sleep(LispBuiltin *builtin)
/*
 sleep seconds
 */
{
    long sec, msec;
    double value, dsec;

    LispObj *seconds;

    seconds = ARGUMENT(0);

    value = -1.0;
    switch (OBJECT_TYPE(seconds)) {
	case LispFixnum_t:
	    value = FIXNUM_VALUE(seconds);
	    break;
	case LispDFloat_t:
	    value = DFLOAT_VALUE(seconds);
	    break;
	default:
	    break;
    }

    if (value < 0.0 || value > MOST_POSITIVE_FIXNUM)
	LispDestroy("%s: %s is not a positive fixnum",
		    STRFUN(builtin), STROBJ(seconds));

    msec = modf(value, &dsec) * 1e6;
    sec = dsec;

    if (sec)
	sleep(sec);
    if (msec)
	usleep(msec);

    return (NIL);
}

/*
 *   This function is called recursively, but the contents of "list2" are
 * kept gc protected until it returns to LispSort. This is required partly
 * because the "gc protection logic" protects an object, not the contents
 * of the c pointer.
 */
static LispObj *
LispMergeSort(LispObj *list, LispObj *predicate, LispObj *key, int code)
{
    int protect;
    LispObj *list1, *list2, *left, *right, *result, *cons;

    /* Check if list length is larger than 1 */
    if (!CONSP(list) || !CONSP(CDR(list)))
	return (list);

    list1 = list2 = list;
    for (;;) {
	list = CDR(list);
	if (!CONSP(list))
	    break;
	list = CDR(list);
	if (!CONSP(list))
	    break;
	list2 = CDR(list2);
    }
    cons = list2;
    list2 = CDR(list2);
    RPLACD(cons, NIL);

    protect = 0;
    if (lisp__data.protect.length + 2 >= lisp__data.protect.space)
	LispMoreProtects();
    lisp__data.protect.objects[lisp__data.protect.length++] = list2;
    list1 = LispMergeSort(list1, predicate, key, code);
    list2 = LispMergeSort(list2, predicate, key, code);

    left = CAR(list1);
    right = CAR(list2);
    if (key != UNSPEC) {
	protect = lisp__data.protect.length;
	left = APPLY1(key, left);
	lisp__data.protect.objects[protect] = left;
	right = APPLY1(key, right);
	lisp__data.protect.objects[protect + 1] = right;
    }

    result = NIL;
    for (;;) {
	if ((FCOMPARE(predicate, left, right, code)) == 0 &&
	    (FCOMPARE(predicate, right, left, code)) == 1) {
	    /* right is "smaller" */
	    if (result == NIL)
		result = list2;
	    else
		RPLACD(cons, list2);
	    cons = list2;
	    list2 = CDR(list2);
	    if (!CONSP(list2)) {
		RPLACD(cons, list1);
		break;
	    }
	    right = CAR(list2);
	    if (key != UNSPEC) {
		right = APPLY1(key, right);
		lisp__data.protect.objects[protect + 1] = right;
	    }
	}
	else {
	    /* left is "smaller" */
	    if (result == NIL)
		result = list1;
	    else
		RPLACD(cons, list1);
	    cons = list1;
	    list1 = CDR(list1);
	    if (!CONSP(list1)) {
		RPLACD(cons, list2);
		break;
	    }
	    left = CAR(list1);
	    if (key != UNSPEC) {
		left = APPLY1(key, left);
		lisp__data.protect.objects[protect] = left;
	    }
	}
    }
    if (key != UNSPEC)
	lisp__data.protect.length = protect;

    return (result);
}

/* XXX The first version made a copy of the list and then adjusted
 *     the CARs of the list. To minimize GC time now it is now doing
 *     the sort inplace. So, instead of writing just (sort variable)
 *     now it is required to write (setq variable (sort variable))
 *     if the variable should always keep all elements.
 */
LispObj *
Lisp_Sort(LispBuiltin *builtin)
/*
 sort sequence predicate &key key
 */
{
    GC_ENTER();
    int istring, code;
    long length;
    char *string;

    LispObj *list, *work, *cons = NULL;

    LispObj *sequence, *predicate, *key;

    key = ARGUMENT(2);
    predicate = ARGUMENT(1);
    sequence = ARGUMENT(0);

    length = LispLength(sequence);
    if (length < 2)
	return (sequence);

    list = sequence;
    istring = XSTRINGP(sequence);
    if (istring) {
	CHECK_STRING_WRITABLE(sequence);
	/* Convert string to list */
	string = THESTR(sequence);
	work = cons = CONS(SCHAR(string[0]), NIL);
	GC_PROTECT(work);
	for (++string; *string; ++string) {
	    RPLACD(cons, CONS(SCHAR(*string), NIL));
	    cons = CDR(cons);
	}
    }
    else if (ARRAYP(list))
	work = list->data.array.list;
    else
	work = list;

    FUNCTION_CHECK(predicate);
    code = FCODE(predicate);
    work = LispMergeSort(work, predicate, key, code);

    if (istring) {
	/* Convert list to string */
	string = THESTR(sequence);
	for (; CONSP(work); ++string, work = CDR(work))
	    *string = SCHAR_VALUE(CAR(work));
    }
    else if (ARRAYP(list))
	list->data.array.list = work;
    else
	sequence = work;
    GC_LEAVE();

    return (sequence);
}

LispObj *
Lisp_Subseq(LispBuiltin *builtin)
/*
 subseq sequence start &optional end
 */
{
    long start, end, length, seqlength;

    LispObj *sequence, *ostart, *oend, *result;

    oend = ARGUMENT(2);
    ostart = ARGUMENT(1);
    sequence = ARGUMENT(0);

    LispCheckSequenceStartEnd(builtin, sequence, ostart, oend,
			      &start, &end, &length);

    seqlength = end - start;

    if (sequence == NIL)
	result = NIL;
    else if (XSTRINGP(sequence)) {
	char *string = LispMalloc(seqlength + 1);

	memcpy(string, THESTR(sequence) + start, seqlength);
	string[seqlength] = '\0';
	result = STRING2(string);
    }
    else {
	GC_ENTER();
	LispObj *object;

	if (end > start) {
	    /* list or array */
	    int count;
	    LispObj *cons;

	    if (ARRAYP(sequence))
		object = sequence->data.array.list;
	    else
		object = sequence;
	    /* goto first element to copy */
	    for (count = 0; count < start; count++, object = CDR(object))
		;
	    result = cons = CONS(CAR(object), NIL);
	    GC_PROTECT(result);
	    for (++count, object = CDR(object); count < end; count++,
		 object = CDR(object)) {
		RPLACD(cons, CONS(CAR(object), NIL));
		cons = CDR(cons);
	    }
	}
	else
	    result = NIL;

	if (ARRAYP(sequence)) {
	    object = LispNew(NIL, NIL);
	    GC_PROTECT(object);
	    object->type = LispArray_t;
	    object->data.array.list = result;
	    object->data.array.dim = CONS(FIXNUM(seqlength), NIL);
	    object->data.array.rank = 1;
	    object->data.array.type = sequence->data.array.type;
	    object->data.array.zero = length == 0;
	    result = object;
	}
	GC_LEAVE();
    }

    return (result);
}

LispObj *
Lisp_Subsetp(LispBuiltin *builtin)
/*
 subsetp list1 list2 &key test test-not key
 */
{
    return (LispListSet(builtin, SUBSETP));
}


LispObj *
Lisp_Substitute(LispBuiltin *builtin)
/*
 substitute newitem olditem sequence &key from-end test test-not start end count key
 */
{
    return (LispDeleteRemoveXSubstitute(builtin, SUBSTITUTE, NONE));
}

LispObj *
Lisp_SubstituteIf(LispBuiltin *builtin)
/*
 substitute-if newitem test sequence &key from-end start end count key
 */
{
    return (LispDeleteRemoveXSubstitute(builtin, SUBSTITUTE, IF));
}

LispObj *
Lisp_SubstituteIfNot(LispBuiltin *builtin)
/*
 substitute-if-not newitem test sequence &key from-end start end count key
 */
{
    return (LispDeleteRemoveXSubstitute(builtin, SUBSTITUTE, IFNOT));
}

LispObj *
Lisp_Symbolp(LispBuiltin *builtin)
/*
 symbolp object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (SYMBOLP(object) ? T : NIL);
}

LispObj *
Lisp_SymbolFunction(LispBuiltin *builtin)
/*
 symbol-function symbol
 */
{
    LispObj *symbol;

    symbol = ARGUMENT(0);
    CHECK_SYMBOL(symbol);

    return (LispSymbolFunction(symbol));
}

LispObj *
Lisp_SymbolName(LispBuiltin *builtin)
/*
 symbol-name symbol
 */
{
    LispObj *symbol;

    symbol = ARGUMENT(0);
    CHECK_SYMBOL(symbol);

    return (LispSymbolName(symbol));
}

LispObj *
Lisp_SymbolPackage(LispBuiltin *builtin)
/*
 symbol-package symbol
 */
{
    LispObj *symbol;

    symbol = ARGUMENT(0);
    CHECK_SYMBOL(symbol);

    symbol = symbol->data.atom->package;

    return (symbol ? symbol : NIL);
}

LispObj *
Lisp_SymbolPlist(LispBuiltin *builtin)
/*
 symbol-plist symbol
 */
{
    LispObj *symbol;

    symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);

    return (symbol->data.atom->a_property ?
	    symbol->data.atom->property->properties : NIL);
}

LispObj *
Lisp_SymbolValue(LispBuiltin *builtin)
/*
 symbol-value symbol
 */
{
    LispAtom *atom;
    LispObj *symbol;

    symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);
    atom = symbol->data.atom;
    if (!atom->a_object || atom->property->value == UNBOUND) {
	if (atom->package == lisp__data.keyword)
	    return (symbol);
	LispDestroy("%s: the symbol %s has no value",
		    STRFUN(builtin), STROBJ(symbol));
    }

    return (atom->dyn ? LispGetVar(symbol) : atom->property->value);
}

LispObj *
Lisp_Tagbody(LispBuiltin *builtin)
/*
 tagbody &rest body
 */
{
    GC_ENTER();
    int stack, lex, length;
    LispObj *list, *body, *ptr, *tag, *labels, *map, **p_body;
    LispBlock *block;

    body = ARGUMENT(0);

    /* Save environment information */
    stack = lisp__data.stack.length;
    lex = lisp__data.env.lex;
    length = lisp__data.env.length;

    /* Since the body may be large, and the code may iterate several
     * thousand times, it is not a bad idea to avoid checking all
     * elements of the body to verify if it is a tag. */
    for (labels = map = NIL, ptr = body; CONSP(ptr); ptr = CDR(ptr)) {
	tag = CAR(ptr);
	switch (OBJECT_TYPE(tag)) {
	    case LispNil_t:
	    case LispAtom_t:
	    case LispFixnum_t:
		/* Don't allow duplicated labels */
		for (list = labels; CONSP(list); list = CDDR(list)) {
		    if (CAR(list) == tag)
			LispDestroy("%s: tag %s specified more than once",
				    STRFUN(builtin), STROBJ(tag));
		}
		if (labels == NIL) {
		    labels = CONS(tag, CONS(NIL, NIL));
		    map = CDR(labels);
		    GC_PROTECT(labels);
		}
		else {
		    RPLACD(map, CONS(tag, CONS(NIL, NIL)));
		    map = CDDR(map);
		}
		break;
	    case LispCons_t:
		/* Restart point for tag */
		if (map != NIL && CAR(map) == NIL)
		    RPLACA(map, ptr);
		break;
	    default:
		break;
	}
    }
    /* Check for consecutive labels without code between them */
    for (ptr = labels; CONSP(ptr); ptr = CDDR(ptr)) {
	if (CADR(ptr) == NIL) {
	    for (map = CDDR(ptr); CONSP(map); map = CDDR(map)) {
		if (CADR(map) != NIL) {
		    RPLACA(CDR(ptr), CADR(map));
		    break;
		}
	    }
	}
    }

    /* Initialize */
    list = body;
    p_body = &body;
    block = LispBeginBlock(NIL, LispBlockBody);

    /* Loop */
    if (setjmp(block->jmp) != 0) {
	/* Restore environment */
	lisp__data.stack.length = stack;
	lisp__data.env.lex = lex;
	lisp__data.env.head = lisp__data.env.length = length;

	tag = lisp__data.block.block_ret;
	for (ptr = labels; CONSP(ptr); ptr = CDDR(ptr)) {
	    map = CAR(ptr);
	    if (map == tag)
		break;
	}

	if (!CONSP(ptr))
	    LispDestroy("%s: no such tag %s", STRFUN(builtin), STROBJ(tag));

	*p_body = CADR(ptr);
    }

    /* Execute code */
    for (; CONSP(body); body = CDR(body)) {
	LispObj *form = CAR(body);

	if (CONSP(form))
	    EVAL(form);
    }
    /* If got here, (go) not called, else, labels will be candidate to gc
     * when GC_LEAVE() be called by the code in the bottom of the stack. */
    GC_LEAVE();

    /* Finished */
    LispEndBlock(block);

    /* Always return NIL */
    return (NIL);
}

LispObj *
Lisp_The(LispBuiltin *builtin)
/*
 the value-type form
 */
{
    LispObj *value_type, *form;

    form = ARGUMENT(1);
    value_type = ARGUMENT(0);

    form = EVAL(form);

    return (LispCoerce(builtin, form, value_type));
}

LispObj *
Lisp_Throw(LispBuiltin *builtin)
/*
 throw tag result
 */
{
    unsigned blevel = lisp__data.block.block_level;

    LispObj *tag, *result;

    result = ARGUMENT(1);
    tag = ARGUMENT(0);

    tag = EVAL(tag);

    if (blevel == 0)
	LispDestroy("%s: not within a block", STRFUN(builtin));

    while (blevel) {
	LispBlock *block = lisp__data.block.block[--blevel];

	if (block->type == LispBlockCatch && tag == block->tag) {
	    lisp__data.block.block_ret = EVAL(result);
	    LispBlockUnwind(block);
	    BLOCKJUMP(block);
	}
    }
    LispDestroy("%s: %s is not a valid tag", STRFUN(builtin), STROBJ(tag));

    /*NOTREACHED*/
    return (NIL);
}

static LispObj *
LispTreeEqual(LispObj *left, LispObj *right, LispObj *test, int expect)
{
    LispObj *cmp_left, *cmp_right;

    if ((OBJECT_TYPE(left)) ^ (OBJECT_TYPE(right)))
	return (NIL);
    if (CONSP(left)) {
	for (; CONSP(left) && CONSP(right);
	     left = CDR(left), right = CDR(right)) {
	    cmp_left = CAR(left);
	    cmp_right = CAR(right);
	    if ((OBJECT_TYPE(cmp_left)) ^ (OBJECT_TYPE(cmp_right)))
		return (NIL);
	    if (CONSP(cmp_left)) {
		if (LispTreeEqual(cmp_left, cmp_right, test, expect) == NIL)
		    return (NIL);
	    }
	    else {
		if (POINTERP(cmp_left) &&
		    (XQUOTEP(cmp_left) || XBACKQUOTEP(cmp_left))) {
		    cmp_left = cmp_left->data.quote;
		    cmp_right = cmp_right->data.quote;
		}
		else if (COMMAP(cmp_left)) {
		    cmp_left = cmp_left->data.comma.eval;
		    cmp_right = cmp_right->data.comma.eval;
		}
		if ((APPLY2(test, cmp_left, cmp_right) != NIL) != expect)
		    return (NIL);
	    }
	}
	if ((OBJECT_TYPE(left)) ^ (OBJECT_TYPE(right)))
	    return (NIL);
    }

    if (POINTERP(left) && (XQUOTEP(left) || XBACKQUOTEP(left))) {
	left = left->data.quote;
	right = right->data.quote;
    }
    else if (COMMAP(left)) {
	left = left->data.comma.eval;
	right = right->data.comma.eval;
    }

    return ((APPLY2(test, left, right) != NIL) == expect ? T : NIL);
}

LispObj *
Lisp_TreeEqual(LispBuiltin *builtin)
/*
 tree-equal tree-1 tree-2 &key test test-not
 */
{
    int expect;
    LispObj *compare;

    LispObj *tree_1, *tree_2, *test, *test_not;

    test_not = ARGUMENT(3);
    test = ARGUMENT(2);
    tree_2 = ARGUMENT(1);
    tree_1 = ARGUMENT(0);

    CHECK_TEST_0();
    if (test_not != UNSPEC) {
	expect = 0;
	compare = test_not;
    }
    else {
	if (test == UNSPEC)
	    test = Oeql;
	expect = 1;
	compare = test;
    }

    return (LispTreeEqual(tree_1, tree_2, compare, expect));
}

LispObj *
Lisp_Typep(LispBuiltin *builtin)
/*
 typep object type
 */
{
    LispObj *result = NULL;

    LispObj *object, *type;

    type = ARGUMENT(1);
    object = ARGUMENT(0);

    if (SYMBOLP(type)) {
	Atom_id atom = ATOMID(type);

	if (OBJECT_TYPE(object) == LispStruct_t)
	    result = ATOMID(CAR(object->data.struc.def)) == atom ? T : NIL;
	else if (type->data.atom->a_defstruct &&
		 type->data.atom->property->structure.function == STRUCT_NAME)
	    result = NIL;
	else if (atom == Snil)
	    result = object == NIL ? T : NIL;
	else if (atom == St)
	    result = object == T ? T : NIL;
	else if (atom == Satom)
	    result = !CONSP(object) ? T : NIL;
	else if (atom == Ssymbol)
	    result = SYMBOLP(object) || object == NIL || object == T ? T : NIL;
	else if (atom == Sinteger)
	    result = INTEGERP(object) ? T : NIL;
	else if (atom == Srational)
	    result = RATIONALP(object) ? T : NIL;
	else if (atom == Scons || atom == Slist)
	    result = CONSP(object) ? T : NIL;
	else if (atom == Sstring)
	    result = STRINGP(object) ? T : NIL;
	else if (atom == Scharacter)
	    result = SCHARP(object) ? T : NIL;
	else if (atom == Scomplex)
	    result = COMPLEXP(object) ? T : NIL;
	else if (atom == Svector || atom == Sarray)
	    result = ARRAYP(object) ? T : NIL;
	else if (atom == Skeyword)
	    result = KEYWORDP(object) ? T : NIL;
	else if (atom == Sfunction)
	    result = LAMBDAP(object) ? T : NIL;
	else if (atom == Spathname)
	    result = PATHNAMEP(object) ? T : NIL;
	else if (atom == Sopaque)
	    result = OPAQUEP(object) ? T : NIL;
    }
    else if (CONSP(type)) {
	if (OBJECT_TYPE(object) == LispStruct_t &&
	    SYMBOLP(CAR(type)) && ATOMID(CAR(type)) == Sstruct &&
	    SYMBOLP(CAR(CDR(type))) && CDR(CDR(type)) == NIL) {
	    result = ATOMID(CAR(object->data.struc.def)) ==
		     ATOMID(CAR(CDR(type))) ? T : NIL;
	}
    }
    else if (type == NIL)
	result = object == NIL ? T : NIL;
    else if (type == T)
	result = object == T ? T : NIL;
    if (result == NULL)
	LispDestroy("%s: bad type specification %s",
		    STRFUN(builtin), STROBJ(type));

    return (result);
}

LispObj *
Lisp_Union(LispBuiltin *builtin)
/*
 union list1 list2 &key test test-not key
 */
{
    return (LispListSet(builtin, UNION));
}

LispObj *
Lisp_Nunion(LispBuiltin *builtin)
/*
 nunion list1 list2 &key test test-not key
 */
{
    return (LispListSet(builtin, NUNION));
}

LispObj *
Lisp_Unless(LispBuiltin *builtin)
/*
 unless test &rest body
 */
{
    LispObj *result, *test, *body;

    body = ARGUMENT(1);
    test = ARGUMENT(0);

    result = NIL;
    test = EVAL(test);
    RETURN_COUNT = 0;
    if (test == NIL) {
	for (; CONSP(body); body = CDR(body))
	    result = EVAL(CAR(body));
    }

    return (result);
}

/*
 * ext::until
 */
LispObj *
Lisp_Until(LispBuiltin *builtin)
/*
 until test &rest body
 */
{
    LispObj *result, *test, *body, *prog;

    body = ARGUMENT(1);
    test = ARGUMENT(0);

    result = NIL;
    for (;;) {
	if ((result = EVAL(test)) == NIL) {
	    for (prog = body; CONSP(prog); prog = CDR(prog))
		(void)EVAL(CAR(prog));
	}
	else
	    break;
    }

    return (result);
}

LispObj *
Lisp_UnwindProtect(LispBuiltin *builtin)
/*
 unwind-protect protect &rest cleanup
 */
{
    LispObj *result, **presult = &result;
    int did_jump, *pdid_jump = &did_jump, destroyed;
    LispBlock *block;

    LispObj *protect, *cleanup, **pcleanup = &cleanup;

    cleanup = ARGUMENT(1);
    protect = ARGUMENT(0);

    /* run protected code */
    *presult = NIL;
    *pdid_jump = 1;
    block = LispBeginBlock(NIL, LispBlockProtect);
    if (setjmp(block->jmp) == 0) {
	*presult = EVAL(protect);
	*pdid_jump = 0;
    }
    LispEndBlock(block);
    if (!lisp__data.destroyed && *pdid_jump)
	*presult = lisp__data.block.block_ret;

    destroyed = lisp__data.destroyed;
    lisp__data.destroyed = 0;

    /* run cleanup, unprotected code */
    if (CONSP(*pcleanup))
	for (; CONSP(cleanup); cleanup = CDR(cleanup))
	    (void)EVAL(CAR(cleanup));

    if (destroyed) {
	/* in case there is another unwind-protect */
	LispBlockUnwind(NULL);
	/* if not, just return to the toplevel */
	lisp__data.destroyed = 1;
	LispDestroy(".");
    }

    return (result);
}

static LispObj *
LispValuesList(LispBuiltin *builtin, int check_list)
{
    long i, count;
    LispObj *result;

    LispObj *list;

    list = ARGUMENT(0);

    count = LispLength(list) - 1;

    if (count >= 0) {
	result = CAR(list);
	if ((RETURN_CHECK(count)) != count)
	    LispDestroy("%s: too many values", STRFUN(builtin));
	RETURN_COUNT = count;
	for (i = 0, list = CDR(list); count && CONSP(list);
	     count--, i++, list = CDR(list))
	    RETURN(i) = CAR(list);
	if (check_list) {
	    CHECK_LIST(list);
	}
    }
    else {
	RETURN_COUNT = -1;
	result = NIL;
    }

    return (result);
}

LispObj *
Lisp_Values(LispBuiltin *builtin)
/*
 values &rest objects
 */
{
    return (LispValuesList(builtin, 0));
}

LispObj *
Lisp_ValuesList(LispBuiltin *builtin)
/*
 values-list list
 */
{
    return (LispValuesList(builtin, 1));
}

LispObj *
Lisp_Vector(LispBuiltin *builtin)
/*
 vector &rest objects
 */
{
    LispObj *objects;

    objects = ARGUMENT(0);

    return (VECTOR(objects));
}

LispObj *
Lisp_When(LispBuiltin *builtin)
/*
 when test &rest body
 */
{
    LispObj *result, *test, *body;

    body = ARGUMENT(1);
    test = ARGUMENT(0);

    result = NIL;
    test = EVAL(test);
    RETURN_COUNT = 0;
    if (test != NIL) {
	for (; CONSP(body); body = CDR(body))
	    result = EVAL(CAR(body));
    }

    return (result);
}

/*
 * ext::while
 */
LispObj *
Lisp_While(LispBuiltin *builtin)
/*
 while test &rest body
 */
{
    LispObj *test, *body, *prog;

    body = ARGUMENT(1);
    test = ARGUMENT(0);

    for (;;) {
	if (EVAL(test) != NIL) {
	    for (prog = body; CONSP(prog); prog = CDR(prog))
		(void)EVAL(CAR(prog));
	}
	else
	    break;
    }

    return (NIL);
}

/*
 * ext::unsetenv
 */
LispObj *
Lisp_Unsetenv(LispBuiltin *builtin)
/*
 unsetenv name
 */
{
    char *name;

    LispObj *oname;

    oname = ARGUMENT(0);

    CHECK_STRING(oname);
    name = THESTR(oname);

    unsetenv(name);

    return (NIL);
}

LispObj *
Lisp_XeditEltStore(LispBuiltin *builtin)
/*
 lisp::elt-store sequence index value
 */
{
    int length, offset;

    LispObj *sequence, *oindex, *value;

    value = ARGUMENT(2);
    oindex = ARGUMENT(1);
    sequence = ARGUMENT(0);

    CHECK_INDEX(oindex);
    offset = FIXNUM_VALUE(oindex);
    length = LispLength(sequence);

    if (offset >= length)
	LispDestroy("%s: index %d too large for sequence length %d",
		    STRFUN(builtin), offset, length);

    if (STRINGP(sequence)) {
	int ch;

	CHECK_STRING_WRITABLE(sequence);
	CHECK_SCHAR(value);
	ch = SCHAR_VALUE(value);
	if (ch < 0 || ch > 255)
	    LispDestroy("%s: cannot represent character %d",
			STRFUN(builtin), ch);
	THESTR(sequence)[offset] = ch;
    }
    else {
	if (ARRAYP(sequence))
	    sequence = sequence->data.array.list;

	for (; offset > 0; offset--, sequence = CDR(sequence))
	    ;
	RPLACA(sequence, value);
    }

    return (value);
}

LispObj *
Lisp_XeditPut(LispBuiltin *builtin)
/*
 lisp::put symbol indicator value
 */
{
    LispObj *symbol, *indicator, *value;

    value = ARGUMENT(2);
    indicator = ARGUMENT(1);
    symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);

    return (CAR(LispPutAtomProperty(symbol->data.atom, indicator, value)));
}

LispObj *
Lisp_XeditSetSymbolPlist(LispBuiltin *builtin)
/*
 lisp::set-symbol-plist symbol list
 */
{
    LispObj *symbol, *list;

    list = ARGUMENT(1);
    symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);

    return (LispReplaceAtomPropertyList(symbol->data.atom, list));
}

LispObj *
Lisp_XeditVectorStore(LispBuiltin *builtin)
/*
 lisp::vector-store array &rest values
 */
{
    LispObj *value, *list, *object;
    long rank, count, sequence, offset, accum;

    LispObj *array, *values;

    values = ARGUMENT(1);
    array = ARGUMENT(0);

    /* check for errors */
    for (rank = 0, list = values;
	 CONSP(list) && CONSP(CDR(list));
	 list = CDR(list), rank++) {
	CHECK_INDEX(CAR(values));
    }

    if (rank == 0)
	LispDestroy("%s: too few subscripts", STRFUN(builtin));
    value = CAR(list);

    if (STRINGP(array) && rank == 1) {
	long ch;
	long length = STRLEN(array);
	long offset = FIXNUM_VALUE(CAR(values));

	CHECK_SCHAR(value);
	CHECK_STRING_WRITABLE(array);
	ch = SCHAR_VALUE(value);
	if (offset >= length)
	    LispDestroy("%s: index %ld too large for sequence length %ld",
			STRFUN(builtin), offset, length);

	if (ch < 0 || ch > 255)
	    LispDestroy("%s: cannot represent character %ld",
			STRFUN(builtin), ch);
	THESTR(array)[offset] = ch;

	return (value);
    }

    CHECK_ARRAY(array);
    if (rank != array->data.array.rank)
	LispDestroy("%s: too %s subscripts", STRFUN(builtin),
		    rank < array->data.array.rank ? "few" : "many");

    for (list = values, object = array->data.array.dim;
	 CONSP(CDR(list));
	 list = CDR(list), object = CDR(object)) {
	if (FIXNUM_VALUE(CAR(list)) >= FIXNUM_VALUE(CAR(object)))
	    LispDestroy("%s: %ld is out of range, index %ld",
			STRFUN(builtin),
			FIXNUM_VALUE(CAR(list)),
			FIXNUM_VALUE(CAR(object)));
    }

    for (count = sequence = 0, list = values;
	 CONSP(CDR(list));
	 list = CDR(list), sequence++) {
	for (offset = 0, object = array->data.array.dim;
	     offset < sequence; object = CDR(object), offset++)
	    ;
	for (accum = 1, object = CDR(object); CONSP(object);
	     object = CDR(object))
	    accum *= FIXNUM_VALUE(CAR(object));
	count += accum * FIXNUM_VALUE(CAR(list));
    }

    for (array = array->data.array.list; count > 0; array = CDR(array), count--)
	;

    RPLACA(array, value);

    return (value);
}

LispObj *
Lisp_XeditDocumentationStore(LispBuiltin *builtin)
/*
 lisp::documentation-store symbol type string
 */
{
    LispDocType_t doc_type;

    LispObj *symbol, *type, *string;

    string = ARGUMENT(2);
    type = ARGUMENT(1);
    symbol = ARGUMENT(0);

    CHECK_SYMBOL(symbol);

    /* type is checked in LispDocumentationType() */
    doc_type = LispDocumentationType(builtin, type);

    if (string == NIL)
	/* allow explicitly releasing memory used for documentation */
	LispRemDocumentation(symbol, doc_type);
    else {
	CHECK_STRING(string);
	LispAddDocumentation(symbol, string, doc_type);
    }

    return (string);
}
