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

/* $XFree86: xc/programs/xedit/lisp/internal.h,v 1.49 2002/12/11 04:44:27 paulo Exp $ */

#ifndef Lisp_internal_h
#define Lisp_internal_h

#include <stdio.h>
#include "lisp.h"

#include "mp.h"
#include "re.h"

/*
 * Defines
 */
typedef struct _LispMac LispMac;

#define STREAM_READ		0x01
#define STREAM_WRITE		0x02
#define STREAM_BINARY		0x20

#define RPLACA(cons, object)	(CAR(cons) = object)
#define RPLACD(cons, object)	(CDR(cons) = object)

#define	CAR(list)		((list)->data.cons.car)
#define	CAAR(list)		((list)->data.cons.car->data.cons.car)
#define	CADR(list)		((list)->data.cons.cdr->data.cons.car)
#define CDR(list)		((list)->data.cons.cdr)
#define CDAR(list)		((list)->data.cons.car->data.cons.cdr)
#define CDDR(list)		((list)->data.cons.cdr->data.cons.cdr)
#define CONS(car, cdr)		LispNewCons(car, cdr)
#define EVAL(list)		LispEval(list)
#define APPLY(fun, args)	LispFuncall(fun, args, 0)
#define APPLY1(fun, arg)	LispApply1(fun, arg)
#define APPLY2(fun, arg1, arg2)	LispApply2(fun, arg1, arg2)
#define APPLY3(f, a1, a2, a3)	LispApply3(f, a1, a2, a3)
#define EXECUTE(string)		LispExecute(string)
#define SYMBOL(atom)		LispNewSymbol(atom)
#define ATOM(string)		LispNewAtom(string, 1)
#define UNINTERNED_ATOM(string)	LispNewAtom(string, 0)
#define FUNCTION(symbol)	LispNewFunction(symbol)
#define FUNCTION_QUOTE(symbol)	LispNewFunctionQuote(symbol)

	/* atom string is a static variable */
#define ATOM2(string)		LispNewSymbol(LispGetPermAtom(string))

	/* make a gc never released variable with a static string argument */
#define STATIC_ATOM(string)	LispNewStaticAtom(string)

#define STRING(str)		LispNewString(str, strlen(str), 0)
#define LSTRING(str, size)	LispNewString(str, size, 0)

	/* string must be from the LispXXX allocation functions,
	 * and LispMused not yet called on it */
#define STRING2(str)		LispNewString(str, strlen(str), 1)
#define LSTRING2(str, size)	LispNewString(str, size, 1)

#define VECTOR(objects)		LispNewVector(objects)

	/* STRINGSTREAM2 and LSTRINGSTREAM2 require that the
	 * string be allocated from the LispXXX allocation functions,
	 * and LispMused not yet called on it */
#define STRINGSTREAM(str, flag)			\
	LispNewStringStream(str, flag, strlen(str), 0)
#define STRINGSTREAM2(str, flag)		\
	LispNewStringStream(str, flag, strlen(str), 1)
#define LSTRINGSTREAM(str, flag, length)	\
	LispNewStringStream(str, flag, length, 0)
#define LSTRINGSTREAM2(str, flag, length)	\
	LispNewStringStream(str, flag, length, 1)

#define FILESTREAM(file, path, flag)	\
	LispNewFileStream(file, path, flag)
#define PIPESTREAM(file, path, flag)	\
	LispNewPipeStream(file, path, flag)

#define CHECKO(obj, typ)						\
	((obj)->type == LispOpaque_t && 				\
	 ((obj)->data.opaque.type == typ || (obj)->data.opaque.type == 0))
#define PROTECT(key, list)	LispProtect(key, list)
#define UPROTECT(key, list)	LispUProtect(key, list)

/* create a new unique static atom string */
#define GETATOMID(string)	LispGetAtomString(string, 1)

#define	GCDisable()		++gcpro
#define	GCEnable()		--gcpro


/* pointer to something unique to all atoms with the same print representation */
#define ATOMID(object)		(object)->data.atom->string



#define NIL_BIT			0x01
#define FIXNUM_BIT		0x02
#define FIXNUM_MASK		0x03
#define SCHAR_BIT		0x04
#define SCHAR_MASK		0x05
#define BIT_COUNT		4
#define BIT_MASK		0x0f
#define POINTERP(object)						\
    (((unsigned long)(object) & NIL_BIT) == 0)

#define MOST_POSITIVE_FIXNUM	((1L << (sizeof(long) * 8 - 5)) - 1)
#define MOST_NEGATIVE_FIXNUM	(-1L << (sizeof(long) * 8 - 5))

#define SCHAR(value)							\
    ((LispObj*)(((long)(value) << BIT_COUNT) | SCHAR_MASK))
#define SCHAR_VALUE(object)	FIXNUM_VALUE(object)
#define SCHARP(object)							\
    (((unsigned long)(object) & BIT_MASK) == SCHAR_MASK)
#define CHECK_SCHAR(object)						\
    if (!SCHARP(object))						\
	LispDestroy("%s: %s is not a character",			\
		    STRFUN(builtin), STROBJ(object))

#define XOBJECT_TYPE(object)	((object)->type)
#define OBJECT_TYPE(object)						\
    (POINTERP(object) ? XOBJECT_TYPE(object) : (long)(object) & BIT_MASK)


#define NIL			(LispObj*)0x00000001
#define T			(LispObj*)0x00000011
#define DOT			(LispObj*)0x00000021
/* unmatched ')' */
#define	EOLIST			(LispObj*)0x00000031
#define READLABEL_MASK		0x00000041
/* unspecified argument */
#define UNSPEC			(LispObj*)0x00000051
#define INVALIDP(object)						\
    ((object) == NULL || (object) == EOLIST || (object) == DOT)


/* cons */
#define XCONSP(object)		((object)->type == LispCons_t)
#define CONSP(object)		(POINTERP(object) && XCONSP(object))
#define CHECK_CONS(object)						\
    if (!CONSP(object))							\
	LispDestroy("%s: %s is not of type cons",			\
		    STRFUN(builtin), STROBJ(object))
#define LISTP(object)		(object == NIL || CONSP(object))
#define CHECK_LIST(object)						\
    if (!LISTP(object))							\
	LispDestroy("%s: %s is not a list",				\
		    STRFUN(builtin), STROBJ(object))

/* fixnum */
#define FIXNUM(value)							\
    ((LispObj*)(((long)(value) << BIT_COUNT) | FIXNUM_MASK))
#define FIXNUM_VALUE(object)	((long)(object) >> BIT_COUNT)
#define FIXNUMP(object)							\
    (((unsigned long)(object) & BIT_MASK) == FIXNUM_MASK)
#define CHECK_FIXNUM(object)						\
    if (!FIXNUMP(object))						\
	LispDestroy("%s: %s is not a fixnum",				\
		    STRFUN(builtin), STROBJ(object))
#define INDEXP(object)							\
    (FIXNUMP(object) && FIXNUM_VALUE(object) >= 0)
#define CHECK_INDEX(object)						\
    if (!INDEXP(object))						\
	LispDestroy("%s: %s is not a positive fixnum",			\
		    STRFUN(builtin), STROBJ(object))


/* long int integer */
#define XINTP(object)		((object)->type == LispInteger_t)
#define INTP(objet)		(POINTERP(object) && XINTP(object))
#define INT_VALUE(object)	(object)->data.integer


/* values that fit in a machine long int but not in a fixnum */
#define LONGINTP(object)						\
    (POINTERP(object) ? XINTP(object) : FIXNUMP(object))
#define LONGINT_VALUE(object)						\
    (POINTERP(object) ? INT_VALUE(object) : FIXNUM_VALUE(object))
#define CHECK_LONGINT(object)						\
    if (!LONGINTP(object))						\
	LispDestroy("%s: %s is not an integer",				\
		    STRFUN(builtin), STROBJ(object))


/* bignum */
#define XBIGNUMP(object)	((object)->type == LispBignum_t)
#define BIGNUMP(object)		(POINTERP(object) && XBIGNUMP(object))
#define BIGNUM(object)		LispNewBignum(object)


/* generic integer */
#define INTEGER(integer)	LispNewInteger(integer)
#define INTEGERP(object)						\
    (POINTERP(object) ? XINTP(object) || XBIGNUMP(object) : FIXNUMP(object))
#define CHECK_INTEGER(object)						\
    if (!INTEGERP(object))						\
	LispDestroy("%s: %s is not an integer",				\
		    STRFUN(builtin), STROBJ(object))


/* ratio */
#define XRATIOP(object)		((object)->type == LispRatio_t)
#define RATIOP(object)		(POINTERP(object) && XRATIOP(object))
#define RATIO(num, den)		LispNewRatio(num, den)


/* bigratio */
#define XBIGRATIOP(object)	((object)->type == LispBigratio_t)
#define BIGRATIOP(object)	(POINTERP(object) && XBIGRATIOP(object))
#define BIGRATIO(ratio)		LispNewBigratio(ratio)


/* generic rational */
#define RATIONALP(object)						\
    (POINTERP(object) ? XINTP(object) || XRATIOP(object) ||		\
			XBIGNUMP(object) || XBIGRATIOP(object) :	\
			FIXNUMP(object))


/* double float */
#define XDFLOATP(object)	((object)->type == LispDFloat_t)
#define DFLOATP(object)		(POINTERP(object) && XDFLOATP(object))
#define DFLOAT_VALUE(object)	(object)->data.dfloat
#define CHECK_DFLOAT(object)						\
    if (!DFLOATP(object))						\
	LispDestroy("%s: %s is not a float number",			\
		    STRFUN(builtin), STROBJ(object))
#define DFLOAT(value)		LispNewDFloat(value)


/* generic float - currently only double float supported */
#define FLOATP(object)		DFLOATP(object)


/* real number */
#define REALP(object)							\
    (POINTERP(object) ? XINTP(object) || XDFLOATP(object) ||		\
			XRATIOP(object) || XBIGNUMP(object) ||		\
			XBIGRATIOP(object) :				\
			FIXNUMP(object))
#define CHECK_REAL(object)						\
    if (!REALP(object))							\
	LispDestroy("%s: %s is not a real number",			\
		    STRFUN(builtin), STROBJ(object))


/* complex */
#define XCOMPLEXP(object)	((object)->type == LispComplex_t)
#define COMPLEXP(object)	(POINTERP(object) && XCOMPLEXP(object))
#define COMPLEX(real, imag)	LispNewComplex(real, imag)


/* generic number */
#define NUMBERP(object)							\
    (POINTERP(object) ? XINTP(object) || XDFLOATP(object) ||		\
			XRATIOP(object) || XBIGNUMP(object) ||		\
			XBIGRATIOP(object) || XCOMPLEXP(object) :	\
			FIXNUMP(object))
#define CHECK_NUMBER(object)						\
    if (!NUMBERP(object))						\
	LispDestroy("%s: %s is not a number",				\
		    STRFUN(builtin), STROBJ(object))


/* symbol */
#define XSYMBOLP(object)	((object)->type == LispAtom_t)
#define SYMBOLP(object)		(POINTERP(object) && XSYMBOLP(object))
#define CHECK_SYMBOL(object)						\
    if (!SYMBOLP(object))						\
	LispDestroy("%s: %s is not a symbol",				\
		    STRFUN(builtin), STROBJ(object))


/* keyword */
#define XKEYWORDP(object)						\
    ((object)->data.atom->package == lisp__data.keyword)
#define KEYWORDP(object)						\
    (POINTERP(object) && XSYMBOLP(object) && XKEYWORDP(object))
#define KEYWORD(string)		LispNewKeyword(string)
#define CHECK_KEYWORD(object)						\
    if (!KEYWORDP(object))						\
	LispDestroy("%s: %s is not a keyword",				\
		    STRFUN(builtin), STROBJ(object))
#define CHECK_CONSTANT(object)						\
    if ((object)->data.atom->constant)					\
	LispDestroy("%s: %s is a constant",				\
		    STRFUN(builtin), STROBJ(object))

#define SETVALUE(atom, object)	((atom)->property->value = object)


/* function */
#define XFUNCTIONP(object)	((object)->type == LispFunction_t)
#define FUNCTIONP(object)	(POINTERP(object) && XFUNCTIONP(object))


/* lambda */
#define XLAMBDAP(object)	((object)->type == LispLambda_t)
#define LAMBDAP(object)		(POINTERP(object) && XLAMBDAP(object))


/* string - currently only simple 8 bit characters */
#define XSTRINGP(object)	((object)->type == LispString_t)
#define STRINGP(object)		(POINTERP(object) && XSTRINGP(object))
#define THESTR(object)		(object)->data.string.string
#define STRLEN(object)		(object)->data.string.length
#define CHECK_STRING(object)						\
    if (!STRINGP(object))						\
	LispDestroy("%s: %s is not a string",				\
		    STRFUN(builtin), STROBJ(object))
#define CHECK_STRING_WRITABLE(object)					\
    if (!object->data.string.writable)					\
	LispDestroy("%s: %s is readonly",				\
		    STRFUN(builtin), STROBJ(object))


/* array/vector */
#define XARRAYP(object)		((object)->type == LispArray_t)
#define ARRAYP(object)		(POINTERP(object) && XARRAYP(object))
#define CHECK_ARRAY(object)						\
    if (!ARRAYP(object))						\
	LispDestroy("%s: %s is not an array",				\
		    STRFUN(builtin), STROBJ(object))


/* quote */
#define XQUOTEP(object)		((object)->type == LispQuote_t)
#define QUOTEP(object)		(POINTERP(object) && XQUOTEP(object))
#define QUOTE(object)		LispNewQuote(object)

#define XBACKQUOTEP(object)	((object)->type == LispBackquote_t)
#define BACKQUOTEP(object)	(POINTERP(object) && XBACKQUOTEP(object))
#define BACKQUOTE(object)	LispNewBackquote(object)

#define XCOMMAP(object)		((object)->type == LispComma_t)
#define COMMAP(object)		(POINTERP(object) && XCOMMAP(object))
#define COMMA(object, at)	LispNewComma(object, at)


/* package */
#define XPACKAGEP(object)	((object)->type == LispPackage_t)
#define PACKAGEP(object)	(POINTERP(object) && XPACKAGEP(object))


/* pathname */
#define XPATHNAMEP(object)	((object)->type == LispPathname_t)
#define PATHNAMEP(object)	(POINTERP(object) && XPATHNAMEP(object))
#define PATHNAME(object)	LispNewPathname(object)
#define CHECK_PATHNAME(object)						\
    if (!PATHNAMEP(object))						\
	LispDestroy("%s: %s is not a pathname",				\
		    STRFUN(builtin), STROBJ(object))


/* stream */
#define XSTREAMP(object)	((object)->type == LispStream_t)
#define STREAMP(object)		(POINTERP(object) && XSTREAMP(object))
#define CHECK_STREAM(object)						\
    if (!STREAMP(object))						\
	LispDestroy("%s: %s is not a stream",				\
		    STRFUN(builtin), STROBJ(object))


/* hastable */
#define XHASHTABLEP(object)	((object)->type == LispHashTable_t)
#define HASHTABLEP(object)	(POINTERP(object) && XHASHTABLEP(object))
#define CHECK_HASHTABLE(object)						\
    if (!HASHTABLEP(object))						\
	LispDestroy("%s: %s is not a hash-table",			\
		    STRFUN(builtin), STROBJ(object))


/* regex */
#define XREGEXP(object)		((object)->type == LispRegex_t)
#define REGEXP(object)		(POINTERP(object) && XREGEXP(object))
#define CHECK_REGEX(object)						\
    if (!REGEXP(object))						\
	LispDestroy("%s: %s is not a regexp",				\
		    STRFUN(builtin), STROBJ(object))


/* bytecode */
#define XBYTECODEP(object)	((object)->type == LispBytecode_t)
#define BYTECODEP(object)	(POINTERP(object) && XBYTECODEP(object))


/* opaque */
#define XOPAQUEP(object)	((object)->type == LispOpaque_t)
#define OPAQUEP(object)		(POINTERP(object) && XOPAQUEP(object))
#define OPAQUE(data, type)	LispNewOpaque((void*)((long)data), type)



#define SSTREAMP(str)		((str)->data.stream.source.string)

#define FSTREAMP(str)		((str)->data.stream.source.file)

#define PSTREAMP(str)		((str)->data.stream.source.program)
#define PIDPSTREAMP(str)	((str)->data.stream.source.program->pid)
#define IPSTREAMP(str)		((str)->data.stream.source.program->input)
#define OPSTREAMP(str)		((str)->data.stream.source.program->output)
#define EPSTREAMP(str)		\
	FSTREAMP((str)->data.stream.source.program->errorp)

#define LispFileno(file)	((file)->descriptor)

#define STRFUN(builtin)		ATOMID(builtin->symbol)
#define STROBJ(obj)		LispStrObj(obj)

/* fetch builtin function/macro argument value
 */
#define ARGUMENT(index)							\
	lisp__data.stack.values[lisp__data.stack.base + (index)]

#define RETURN(index)	lisp__data.returns.values[(index)]
#define RETURN_COUNT	lisp__data.returns.count
#define RETURN_CHECK(value)						\
    value < MULTIPLE_VALUES_LIMIT ?					\
	value : MULTIPLE_VALUES_LIMIT

#define GC_ENTER()		int gc__protect = lisp__data.protect.length

#define GC_PROTECT(object)						\
    if (lisp__data.protect.length >= lisp__data.protect.space)		\
	LispMoreProtects();						\
    lisp__data.protect.objects[lisp__data.protect.length++] = object

#define GC_LEAVE()		lisp__data.protect.length = gc__protect


#define ERROR_CHECK_SPECIAL_FORM(atom)					\
    if (atom->property->fun.builtin->compile)				\
	LispDestroy("%s: the special form %s cannot be redefined",	\
		    STRFUN(builtin), atom->string)



#define CONSTANTP(object)						\
    (!POINTERP(object) ||						\
     XOBJECT_TYPE(object) < LispAtom_t ||				\
     (XSYMBOLP(object) && XKEYWORDP(object)))

/* slightly faster test, since keywords are very uncommon as eval arguments */
#define NCONSTANTP(object)						\
    (OBJECT_TYPE(object) >= LispAtom_t)


/*
 * Types
 */
typedef struct _LispObj LispObj;
typedef struct _LispAtom LispAtom;
typedef struct _LispBuiltin LispBuiltin;
typedef struct _LispModuleData LispModuleData;
typedef struct _LispFile LispFile;
typedef struct _LispString LispString;
typedef struct _LispPackage LispPackage;
typedef struct _LispBytecode LispBytecode;
typedef struct _LispHashTable LispHashTable;

/* Bytecode compiler data */
typedef struct _LispCom LispCom;

typedef char *Atom_id;

typedef enum _LispType {
    /* objects encoded in the LispObj pointer */
    LispNil_t = 1,
    LispFixnum_t = 3,
    LispSChar_t = 5,

    /* objects that have a structure */
    LispInteger_t = 16,
    LispDFloat_t,
    LispString_t,
    LispRatio_t,
    LispOpaque_t,

    /* simple access for marking */
    LispBignum_t,
    LispBigratio_t,

    LispAtom_t,
    LispFunction_t,
    LispFunctionQuote_t,

    LispLambda_t,

    LispComplex_t,
    LispCons_t,
    LispQuote_t,
    LispArray_t,
    LispStruct_t,
    LispStream_t,
    LispBackquote_t,
    LispComma_t,
    LispPathname_t,
    LispPackage_t,
    LispRegex_t,
    LispBytecode_t,
    LispHashTable_t
} LispType;

typedef enum _LispFunType {
    LispLambda,
    LispFunction,
    LispMacro,
    LispSetf
} LispFunType;

typedef enum _LispStreamType {
    LispStreamString,
    LispStreamFile,
    LispStreamStandard,
    LispStreamPipe
} LispStreamType;

typedef struct {
    int pid;			/* process id of program */
    LispFile *input;		/* if READABLE: stdout of program */
    LispFile *output;		/* if WRITABLE: stdin of program */
    LispObj *errorp;		/* ALWAYS (ONLY) READABLE: stderr of program */
} LispPipe;

struct _LispObj {
    LispType type : 6;
    unsigned int mark : 1;	/* gc protected */
    unsigned int prot: 1;	/* protection for constant/unamed variables */
    LispFunType funtype : 4;	/* this is subject to change in the future */
    union {
	LispAtom *atom;
	struct {
	    char *string;
	    long length;
	    int writable : 1;
	} string;
	long integer;
	double dfloat;
	LispObj *quote;
	LispObj *pathname;	/* don't use quote generic name,
				 * to avoid confusing code */
	struct {
	    long numerator;
	    long denominator;
	} ratio;
	union {
	    mpi *integer;
	    mpr *ratio;
	} mp;
	struct {
	    LispObj *real;
	    LispObj *imag;
	} complex;
	struct {
	    LispObj *car;
	    LispObj *cdr;
	} cons;
	struct {
	    LispObj *name;
	    LispObj *code;
	    LispObj *data;		/* extra data to protect */
	} lambda;
	struct {
	    LispObj *list;		/* stored as a linear list */
	    LispObj *dim;		/* dimensions of array */
	    unsigned int rank : 8;	/* i.e. array-rank-limit => 256 */
	    unsigned int type : 7;	/* converted to LispType, if not
					 * Lisp_Nil_t only accepts given
					 * type in array fields */
	    unsigned int zero : 1;	/* at least one of the dimensions
					 * is zero */
	} array;
	struct {
	    LispObj *fields;	/* structure fields */
	    LispObj *def;	/* structure definition */
	} struc;
	struct {
	    union {
		LispFile *file;
		LispPipe *program;
		LispString *string;
	    } source;
	    LispObj *pathname;
	    LispStreamType type : 6;
	    int readable : 1;
	    int writable : 1;
	} stream;
	struct {
	    void *data;
	    int type;
	} opaque;
	struct {
	    LispObj *eval;
	    int atlist;
	} comma;
	struct {
	    LispObj *name;
	    LispObj *nicknames;
	    LispPackage *package;
	} package;
	struct {
	    re_cod *regex;
	    LispObj *pattern;		/* regex string */
	    int options;		/* regex compile flags */
	} regex;
	struct {
	    LispBytecode *bytecode;
	    LispObj *code;		/* object used to generate bytecode */
	    LispObj *name;		/* name of function, or NIL */
	} bytecode;
	struct {
	    LispHashTable *table;
	    LispObj *test;
	} hash;
    } data;
};

typedef	LispObj *(*LispFunPtr)(LispBuiltin*);
typedef void (*LispComPtr)(LispCom*, LispBuiltin*);

struct _LispBuiltin {
    /* these fields must be set */
    LispFunType type;
    LispFunPtr function;
    char *declaration;

    /* this field is optional, set if the function returns multiple values */
    int multiple_values;

    /* this field is also optional, set if the function should not be exported */
    int internal;

    /* this optional field points to a function of the bytecode compiler */
    LispComPtr compile;

    /* this field is set at runtime */
    LispObj *symbol;
};

typedef int (*LispLoadModule)(void);
typedef int (*LispUnloadModule)(void);

#define LISP_MODULE_VERSION		1
struct _LispModuleData {
    int version;
    LispLoadModule load;
    LispUnloadModule unload;
};

/*
 * Prototypes
 */
LispObj *LispEval(LispObj*);
LispObj *LispFuncall(LispObj*, LispObj*, int);
LispObj *LispApply1(LispObj*, LispObj*);
LispObj *LispApply2(LispObj*, LispObj*, LispObj*);
LispObj *LispApply3(LispObj*, LispObj*, LispObj*, LispObj*);

LispObj *LispNew(LispObj*, LispObj*);
LispObj *LispNewSymbol(LispAtom*);
LispObj *LispNewAtom(char*, int);
LispObj *LispNewFunction(LispObj*);
LispObj *LispNewFunctionQuote(LispObj*);
LispObj *LispNewStaticAtom(char*);
LispObj *LispNewDFloat(double);
LispObj *LispNewString(char*, long, int);
LispObj *LispNewInteger(long);
LispObj *LispNewRatio(long, long);
LispObj *LispNewVector(LispObj*);
LispObj *LispNewQuote(LispObj*);
LispObj *LispNewBackquote(LispObj*);
LispObj *LispNewComma(LispObj*, int);
LispObj *LispNewCons(LispObj*, LispObj*);
LispObj *LispNewLambda(LispObj*, LispObj*, LispObj*, LispFunType);
LispObj *LispNewStruct(LispObj*, LispObj*);
LispObj *LispNewComplex(LispObj*, LispObj*);
LispObj *LispNewOpaque(void*, int);
LispObj *LispNewKeyword(char*);
LispObj *LispNewPathname(LispObj*);
LispObj *LispNewStringStream(char*, int, long, int);
LispObj *LispNewFileStream(LispFile*, LispObj*, int);
LispObj *LispNewPipeStream(LispPipe*, LispObj*, int);
LispObj *LispNewBignum(mpi*);
LispObj *LispNewBigratio(mpr*);

LispAtom *LispGetAtom(char*);

/* This function does not allocate a copy of it's argument, but the argument
 * itself. The argument string should never change. */
LispAtom *LispGetPermAtom(char*);

void *LispMalloc(size_t);
void *LispCalloc(size_t, size_t);
void *LispRealloc(void*, size_t);
char *LispStrdup(char*);
void LispFree(void*);
/* LispMused means memory is now safe from LispDestroy, and should not be
 * freed in case of an error */
void LispMused(void*);

void LispGC(LispObj*, LispObj*);

char *LispStrObj(LispObj*);

#ifdef __GNUC__
#define PRINTF_FORMAT	__attribute__ ((format (printf, 1, 2)))
#else
#define PRINTF_FORMAT	/**/
#endif
void LispDestroy(char *fmt, ...) PRINTF_FORMAT;
	/* continuable error */
void LispContinuable(char *fmt, ...) PRINTF_FORMAT;
void LispMessage(char *fmt, ...) PRINTF_FORMAT;
void LispWarning(char *fmt, ...) PRINTF_FORMAT;
#undef PRINTF_FORMAT

LispObj *LispSetVariable(LispObj*, LispObj*, char*, int);

int LispRegisterOpaqueType(char*);

int LispPrintString(LispObj*, char*);

void LispProtect(LispObj*, LispObj*);
void LispUProtect(LispObj*, LispObj*);

/* this function should be called when a module is loaded, and is called
 * when loading the interpreter */
void LispAddBuiltinFunction(LispBuiltin*);

/*
 * Initialization
 */
extern LispObj *UNBOUND;
extern int gcpro;

extern LispObj *Okey, *Orest, *Ooptional, *Oaux, *Olambda;
extern Atom_id Snil, St, Skey, Srest, Soptional, Saux;
extern Atom_id Sand, Sor, Snot;
extern Atom_id Satom, Ssymbol, Sinteger, Scharacter, Sstring, Slist,
	       Scons, Svector, Sarray, Sstruct, Skeyword, Sfunction, Spathname,
	       Srational, Sfloat, Scomplex, Sopaque, Sdefault;

extern LispObj *Ocomplex, *Oformat, *Kunspecific;

extern LispObj *Omake_array, *Kinitial_contents, *Osetf;
extern Atom_id Svariable, Sstructure, Stype, Ssetf;

extern Atom_id Smake_struct, Sstruct_access, Sstruct_store, Sstruct_type;
extern LispObj *Omake_struct, *Ostruct_access, *Ostruct_store, *Ostruct_type;

extern LispObj *Oparse_namestring, *Kerror, *Kabsolute, *Krelative, *Oopen,
	       *Oclose, *Kif_does_not_exist;

extern LispObj *Oequal_;

extern LispFile *Stdout, *Stdin, *Stderr;

#endif /* Lisp_internal_h */
