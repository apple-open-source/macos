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

/* $XFree86: xc/programs/xedit/lisp/lisp.c,v 1.87tsi Exp $ */

#include <stdlib.h>
#include <string.h>
#ifdef sun
#include <strings.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/wait.h>

#ifndef X_NOT_POSIX
#include <unistd.h>	/* for sysconf(), and getpagesize() */
#endif

#if defined(linux)
#define HAS_GETPAGESIZE
#define HAS_SC_PAGESIZE	/* _SC_PAGESIZE may be an enum for Linux */
#endif

#if defined(CSRG_BASED)
#define HAS_GETPAGESIZE
#endif

#if defined(sun)
#define HAS_GETPAGESIZE
#endif

#if defined(QNX4)
#define HAS_GETPAGESIZE
#endif

#if defined(__QNXNTO__)
#define HAS_SC_PAGESIZE
#endif

#include "lisp/bytecode.h"

#include "lisp/read.h"
#include "lisp/format.h"
#include "lisp/math.h"
#include "lisp/hash.h"
#include "lisp/package.h"
#include "lisp/pathname.h"
#include "lisp/regex.h"
#include "lisp/require.h"
#include "lisp/stream.h"
#include "lisp/struct.h"
#include "lisp/time.h"
#include "lisp/write.h"
#include <math.h>

typedef struct {
    LispObj **objects;
    LispObj *freeobj;
    int nsegs;
    int nobjs;
    int nfree;
} LispObjSeg;

/*
 * Prototypes
 */
static void Lisp__GC(LispObj*, LispObj*);
static LispObj *Lisp__New(LispObj*, LispObj*);

/* run a user function, to be called only by LispEval */
static LispObj *LispRunFunMac(LispObj*, LispObj*, int, int);

/* expands and executes a setf method, to be called only by Lisp_Setf */
LispObj *LispRunSetf(LispArgList*, LispObj*, LispObj*, LispObj*);
LispObj *LispRunSetfMacro(LispAtom*, LispObj*, LispObj*);

/* increases storage size for environment */
void LispMoreEnvironment(void);

/* increases storage size for stack of builtin arguments */
void LispMoreStack(void);

/* increases storage size for global variables */
void LispMoreGlobals(LispPackage*);

#ifdef __GNUC__
static INLINE LispObj *LispDoGetVar(LispObj*);
#endif
static INLINE void LispDoAddVar(LispObj*, LispObj*);

/* Helper for importing symbol(s) functions,
 * Search for the specified object in the current package */
static INLINE LispObj *LispGetVarPack(LispObj*);

/* create environment for function call */
static int LispMakeEnvironment(LispArgList*, LispObj*, LispObj*, int, int);

	/* if not already in keyword package, move atom to keyword package */
static LispObj *LispCheckKeyword(LispObj*);

	/* builtin backquote parsing */
static LispObj *LispEvalBackquoteObject(LispObj*, int, int);
	/* used also by the bytecode compiler */
LispObj *LispEvalBackquote(LispObj*, int);

	/* create or change object property */
void LispSetAtomObjectProperty(LispAtom*, LispObj*);
	/* remove object property */
static void LispRemAtomObjectProperty(LispAtom*);

	/* allocates a new LispProperty for the given atom */
static void LispAllocAtomProperty(LispAtom*);
	/* Increment reference count of atom property */
static void LispIncrementAtomReference(LispAtom*);
	/* Decrement reference count of atom property */
static void LispDecrementAtomReference(LispAtom*);
	/* Removes all atom properties */
static void LispRemAtomAllProperties(LispAtom*);

static LispObj *LispAtomPropertyFunction(LispAtom*, LispObj*, int);

static INLINE void LispCheckMemLevel(void);

void LispAllocSeg(LispObjSeg*, int);
static INLINE void LispMark(LispObj*);

/* functions, macros, setf methods, and structure definitions */
static INLINE void LispProt(LispObj*);

static LispObj *LispCheckNeedProtect(LispObj*);

static
#ifdef SIGNALRETURNSINT
int
#else
void
#endif
LispSignalHandler(int);

/*
 * Initialization
 */
LispMac lisp__data;

static LispObj lispunbound = {LispNil_t};
LispObj *UNBOUND = &lispunbound;

static volatile int lisp__disable_int;
static volatile int lisp__interrupted;

LispObj *Okey, *Orest, *Ooptional, *Oaux, *Olambda;

Atom_id Snil, St;
Atom_id Saux, Skey, Soptional, Srest;
Atom_id Satom, Ssymbol, Sinteger, Scharacter, Sstring, Slist,
	Scons, Svector, Sarray, Sstruct, Skeyword, Sfunction, Spathname,
	Srational, Sfloat, Scomplex, Sopaque, Sdefault;

LispObj *Oformat, *Kunspecific;
LispObj *Oexpand_setf_method;

static LispProperty noproperty;
LispProperty *NOPROPERTY = &noproperty;
static int segsize, minfree;
int pagesize, gcpro;

static LispObjSeg objseg = {NULL, NIL};
static LispObjSeg atomseg = {NULL, NIL};

int LispArgList_t;

LispFile *Stdout, *Stdin, *Stderr;

static LispBuiltin lispbuiltins[] = {
    {LispFunction, Lisp_Mul, "* &rest numbers"},
    {LispFunction, Lisp_Plus, "+ &rest numbers"},
    {LispFunction, Lisp_Minus, "- number &rest more-numbers"},
    {LispFunction, Lisp_Div, "/ number &rest more-numbers"},
    {LispFunction, Lisp_OnePlus, "1+ number"},
    {LispFunction, Lisp_OneMinus, "1- number"},
    {LispFunction, Lisp_Less, "< number &rest more-numbers"},
    {LispFunction, Lisp_LessEqual, "<= number &rest more-numbers"},
    {LispFunction, Lisp_Equal_, "= number &rest more-numbers"},
    {LispFunction, Lisp_Greater, "> number &rest more-numbers"},
    {LispFunction, Lisp_GreaterEqual, ">= number &rest more-numbers"},
    {LispFunction, Lisp_NotEqual, "/= number &rest more-numbers"},
    {LispFunction, Lisp_Max, "max number &rest more-numbers"},
    {LispFunction, Lisp_Min, "min number &rest more-numbers"},
    {LispFunction, Lisp_Abs, "abs number"},
    {LispFunction, Lisp_Acons, "acons key datum alist"},
    {LispFunction, Lisp_Adjoin, "adjoin item list &key key test test-not"},
    {LispFunction, Lisp_AlphaCharP, "alpha-char-p char"},
    {LispMacro, Lisp_And, "and &rest args", 1, 0, Com_And},
    {LispFunction, Lisp_Append, "append &rest lists"},
    {LispFunction, Lisp_Apply, "apply function arg &rest more-args", 1},
    {LispFunction, Lisp_Aref, "aref array &rest subscripts"},
    {LispFunction, Lisp_Assoc, "assoc item list &key test test-not key"},
    {LispFunction, Lisp_AssocIf, "assoc-if predicate list &key key"},
    {LispFunction, Lisp_AssocIfNot, "assoc-if-not predicate list &key key"},
    {LispFunction, Lisp_Atom, "atom object"},
    {LispMacro, Lisp_Block, "block name &rest body", 1, 0, Com_Block},
    {LispFunction, Lisp_BothCaseP, "both-case-p character"},
    {LispFunction, Lisp_Boundp, "boundp symbol"},
    {LispFunction, Lisp_Butlast, "butlast list &optional count"},
    {LispFunction, Lisp_Nbutlast, "nbutlast list &optional count"},
    {LispFunction, Lisp_Car, "car list", 0, 0, Com_C_r},
    {LispFunction, Lisp_Car, "first list", 0, 0, Com_C_r},
    {LispMacro, Lisp_Case, "case keyform &rest body"},
    {LispMacro, Lisp_Catch, "catch tag &rest body", 1},
    {LispFunction, Lisp_Cdr, "cdr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_Cdr, "rest list", 0, 0, Com_C_r},
    {LispFunction, Lisp_Ceiling, "ceiling number &optional divisor", 1},
    {LispFunction, Lisp_Fceiling, "fceiling number &optional divisor", 1},
    {LispFunction, Lisp_Char, "char string index"},
    {LispFunction, Lisp_Char, "schar simple-string index"},
    {LispFunction, Lisp_CharLess, "char< character &rest more-characters"},
    {LispFunction, Lisp_CharLessEqual, "char<= character &rest more-characters"},
    {LispFunction, Lisp_CharEqual_, "char= character &rest more-characters"},
    {LispFunction, Lisp_CharGreater, "char> character &rest more-characters"},
    {LispFunction, Lisp_CharGreaterEqual, "char>= character &rest more-characters"},
    {LispFunction, Lisp_CharNotEqual_, "char/= character &rest more-characters"},
    {LispFunction, Lisp_CharLessp, "char-lessp character &rest more-characters"},
    {LispFunction, Lisp_CharNotGreaterp, "char-not-greaterp character &rest more-characters"},
    {LispFunction, Lisp_CharEqual, "char-equal character &rest more-characters"},
    {LispFunction, Lisp_CharGreaterp, "char-greaterp character &rest more-characters"},
    {LispFunction, Lisp_CharNotLessp, "char-not-lessp character &rest more-characters"},
    {LispFunction, Lisp_CharNotEqual, "char-not-equal character &rest more-characters"},
    {LispFunction, Lisp_CharDowncase, "char-downcase character"},
    {LispFunction, Lisp_CharInt, "char-code character"},
    {LispFunction, Lisp_CharInt, "char-int character"},
    {LispFunction, Lisp_CharUpcase, "char-upcase character"},
    {LispFunction, Lisp_Character, "character object"},
    {LispFunction, Lisp_Characterp, "characterp object"},
    {LispFunction, Lisp_Clrhash, "clrhash hash-table"},
    {LispFunction, Lisp_IntChar, "code-char integer"},
    {LispFunction, Lisp_Coerce, "coerce object result-type"},
    {LispFunction, Lisp_Compile, "compile name &optional definition", 1},
    {LispFunction, Lisp_Complex, "complex realpart &optional imagpart"},
    {LispMacro, Lisp_Cond, "cond &rest body", 0, 0, Com_Cond},
    {LispFunction, Lisp_Cons, "cons car cdr", 0, 0, Com_Cons},
    {LispFunction, Lisp_Consp, "consp object", 0, 0, Com_Consp},
    {LispFunction, Lisp_Constantp, "constantp form &optional environment"},
    {LispFunction, Lisp_Conjugate, "conjugate number"},
    {LispFunction, Lisp_Complexp, "complexp object"},
    {LispFunction, Lisp_CopyAlist, "copy-alist list"},
    {LispFunction, Lisp_CopyList, "copy-list list"},
    {LispFunction, Lisp_CopyTree, "copy-tree list"},
    {LispFunction, Lisp_Close, "close stream &key abort"},
    {LispFunction, Lisp_C_r, "caar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cadr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cdar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cddr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "caaar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "caadr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cadar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "caddr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cdaar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cdadr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cddar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cdddr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "caaaar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "caaadr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "caadar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "caaddr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cadaar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cadadr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "caddar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cadddr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cdaaar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cdaadr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cdadar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cdaddr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cddaar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cddadr list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cdddar list", 0, 0, Com_C_r},
    {LispFunction, Lisp_C_r, "cddddr list", 0, 0, Com_C_r},
    {LispMacro, Lisp_Decf, "decf place &optional delta"},
    {LispMacro, Lisp_Defconstant, "defconstant name initial-value &optional documentation"},
    {LispMacro, Lisp_Defmacro, "defmacro name lambda-list &rest body"},
    {LispMacro, Lisp_Defstruct, "defstruct name &rest description"},
    {LispMacro, Lisp_Defun, "defun name lambda-list &rest body"},
    {LispMacro, Lisp_Defsetf, "defsetf function lambda-list &rest body"},
    {LispMacro, Lisp_Defparameter, "defparameter name initial-value &optional documentation"},
    {LispMacro, Lisp_Defvar, "defvar name &optional initial-value documentation"},
    {LispFunction, Lisp_Delete, "delete item sequence &key from-end test test-not start end count key"},
    {LispFunction, Lisp_DeleteDuplicates, "delete-duplicates sequence &key from-end test test-not start end key"},
    {LispFunction, Lisp_DeleteIf, "delete-if predicate sequence &key from-end start end count key"},
    {LispFunction, Lisp_DeleteIfNot, "delete-if-not predicate sequence &key from-end start end count key"},
    {LispFunction, Lisp_DeleteFile, "delete-file filename"},
    {LispFunction, Lisp_Denominator, "denominator rational"},
    {LispFunction, Lisp_DigitChar, "digit-char weight &optional radix"},
    {LispFunction, Lisp_DigitCharP, "digit-char-p character &optional radix"},
    {LispFunction, Lisp_Directory, "directory pathname &key all if-cannot-read"},
    {LispFunction, Lisp_DirectoryNamestring, "directory-namestring pathname"},
    {LispFunction, Lisp_Disassemble, "disassemble function"},
    {LispMacro, Lisp_Do, "do init test &rest body"},
    {LispMacro, Lisp_DoP, "do* init test &rest body"},
    {LispFunction, Lisp_Documentation, "documentation symbol type"},
    {LispMacro, Lisp_DoList, "dolist init &rest body", 0, 0, Com_Dolist},
    {LispMacro, Lisp_DoTimes, "dotimes init &rest body"},
    {LispMacro, Lisp_DoAllSymbols, "do-all-symbols init &rest body"},
    {LispMacro, Lisp_DoExternalSymbols, "do-external-symbols init &rest body"},
    {LispMacro, Lisp_DoSymbols, "do-symbols init &rest body"},
    {LispFunction, Lisp_Elt, "elt sequence index"},
    {LispFunction, Lisp_Endp, "endp object"},
    {LispFunction, Lisp_EnoughNamestring, "enough-namestring pathname &optional defaults"},
    {LispFunction, Lisp_Eq, "eq left right", 0, 0, Com_Eq},
    {LispFunction, Lisp_Eql, "eql left right", 0, 0, Com_Eq},
    {LispFunction, Lisp_Equal, "equal left right", 0, 0, Com_Eq},
    {LispFunction, Lisp_Equalp, "equalp left right", 0, 0, Com_Eq},
    {LispFunction, Lisp_Error, "error control-string &rest arguments"},
    {LispFunction, Lisp_Evenp, "evenp integer"},
    {LispFunction, Lisp_Export, "export symbols &optional package"},
    {LispFunction, Lisp_Eval, "eval form"},
    {LispFunction, Lisp_Every, "every predicate sequence &rest more-sequences"},
    {LispFunction, Lisp_Some, "some predicate sequence &rest more-sequences"},
    {LispFunction, Lisp_Notevery, "notevery predicate sequence &rest more-sequences"},
    {LispFunction, Lisp_Notany, "notany predicate sequence &rest more-sequences"},
    {LispFunction, Lisp_Fboundp, "fboundp symbol"},
    {LispFunction, Lisp_Find, "find item sequence &key from-end test test-not start end key"},
    {LispFunction, Lisp_FindIf, "find-if predicate sequence &key from-end start end key"},
    {LispFunction, Lisp_FindIfNot, "find-if-not predicate sequence &key from-end start end key"},
    {LispFunction, Lisp_FileNamestring, "file-namestring pathname"},
    {LispFunction, Lisp_Fill, "fill sequence item &key start end"},
    {LispFunction, Lisp_FindAllSymbols, "find-all-symbols string-or-symbol"},
    {LispFunction, Lisp_FindSymbol, "find-symbol string &optional package", 1},
    {LispFunction, Lisp_FindPackage, "find-package name"},
    {LispFunction, Lisp_Float, "float number &optional other"},
    {LispFunction, Lisp_Floatp, "floatp object"},
    {LispFunction, Lisp_Floor, "floor number &optional divisor", 1},
    {LispFunction, Lisp_Ffloor, "ffloor number &optional divisor", 1},
    {LispFunction, Lisp_Fmakunbound, "fmakunbound symbol"},
    {LispFunction, Lisp_Format, "format destination control-string &rest arguments"},
    {LispFunction, Lisp_FreshLine, "fresh-line &optional output-stream"},
    {LispFunction, Lisp_Funcall, "funcall function &rest arguments", 1},
    {LispFunction, Lisp_Functionp, "functionp object"},
    {LispFunction, Lisp_Gc, "gc &optional car cdr"},
    {LispFunction, Lisp_Gcd, "gcd &rest integers"},
    {LispFunction, Lisp_Gensym, "gensym &optional arg"},
    {LispFunction, Lisp_Get, "get symbol indicator &optional default"},
    {LispFunction, Lisp_Gethash, "gethash key hash-table &optional default", 1},
    {LispMacro, Lisp_Go, "go tag", 0, 0, Com_Go},
    {LispFunction, Lisp_GraphicCharP, "graphic-char-p char"},
    {LispFunction, Lisp_HashTableP, "hash-table-p object"},
    {LispFunction, Lisp_HashTableCount, "hash-table-count hash-table"},
    {LispFunction, Lisp_HashTableRehashSize, "hash-table-rehash-size hash-table"},
    {LispFunction, Lisp_HashTableRehashThreshold, "hash-table-rehash-threshold hash-table"},
    {LispFunction, Lisp_HashTableSize, "hash-table-size hash-table"},
    {LispFunction, Lisp_HashTableTest, "hash-table-test hash-table"},
    {LispFunction, Lisp_HostNamestring, "host-namestring pathname"},
    {LispMacro, Lisp_If, "if test then &optional else", 0, 0, Com_If},
    {LispMacro, Lisp_IgnoreErrors, "ignore-errors &rest body", 1},
    {LispFunction, Lisp_Imagpart, "imagpart number"},
    {LispMacro, Lisp_InPackage, "in-package name"},
    {LispMacro, Lisp_Incf, "incf place &optional delta"},
    {LispFunction, Lisp_Import, "import symbols &optional package"},
    {LispFunction, Lisp_InputStreamP, "input-stream-p stream"},
    {LispFunction, Lisp_IntChar, "int-char integer"},
    {LispFunction, Lisp_Integerp, "integerp object"},
    {LispFunction, Lisp_Intern, "intern string &optional package", 1},
    {LispFunction, Lisp_Intersection, "intersection list1 list2 &key test test-not key"},
    {LispFunction, Lisp_Nintersection, "nintersection list1 list2 &key test test-not key"},
    {LispFunction, Lisp_Isqrt, "isqrt natural"},
    {LispFunction, Lisp_Keywordp, "keywordp object"},
    {LispFunction, Lisp_Last, "last list &optional count", 0, 0, Com_Last},
    {LispMacro, Lisp_Lambda, "lambda lambda-list &rest body"},
    {LispFunction, Lisp_Lcm, "lcm &rest integers"},
    {LispFunction, Lisp_Length, "length sequence", 0, 0, Com_Length},
    {LispMacro, Lisp_Let, "let init &rest body", 1, 0, Com_Let},
    {LispMacro, Lisp_LetP, "let* init &rest body", 1, 0, Com_Letx},
    {LispFunction, Lisp_ListP, "list* object &rest more-objects"},
    {LispFunction, Lisp_ListAllPackages, "list-all-packages"},
    {LispFunction, Lisp_List, "list &rest args"},
    {LispFunction, Lisp_ListLength, "list-length list"},
    {LispFunction, Lisp_Listp, "listp object", 0, 0, Com_Listp},
    {LispFunction, Lisp_Listen, "listen &optional input-stream"},
    {LispFunction, Lisp_Load, "load filename &key verbose print if-does-not-exist"},
    {LispFunction, Lisp_Logand, "logand &rest integers"},
    {LispFunction, Lisp_Logeqv, "logeqv &rest integers"},
    {LispFunction, Lisp_Logior, "logior &rest integers"},
    {LispFunction, Lisp_Lognot, "lognot integer"},
    {LispFunction, Lisp_Logxor, "logxor &rest integers"},
    {LispMacro, Lisp_Loop, "loop &rest body", 0, 0, Com_Loop},
    {LispFunction, Lisp_LowerCaseP, "lower-case-p character"},
    {LispFunction, Lisp_MakeArray, "make-array dimensions &key element-type initial-element initial-contents adjustable fill-pointer displaced-to displaced-index-offset"},
    {LispFunction, Lisp_MakeHashTable, "make-hash-table &key test size rehash-size rehash-threshold initial-contents"},
    {LispFunction, Lisp_MakeList, "make-list size &key initial-element"},
    {LispFunction, Lisp_MakePackage, "make-package package-name &key nicknames use"},
    {LispFunction, Lisp_MakePathname, "make-pathname &key host device directory name type version defaults"},
    {LispFunction, Lisp_MakeString, "make-string size &key initial-element element-type"},
    {LispFunction, Lisp_MakeSymbol, "make-symbol name"},
    {LispFunction, Lisp_MakeStringInputStream, "make-string-input-stream string &optional start end"},
    {LispFunction, Lisp_MakeStringOutputStream, "make-string-output-stream &key element-type"},
    {LispFunction, Lisp_GetOutputStreamString, "get-output-stream-string string-output-stream"},
    {LispFunction, Lisp_Makunbound, "makunbound symbol"},
    {LispFunction, Lisp_Mapc, "mapc function list &rest more-lists"},
    {LispFunction, Lisp_Mapcar, "mapcar function list &rest more-lists"},
    {LispFunction, Lisp_Mapcan, "mapcan function list &rest more-lists"},
    {LispFunction, Lisp_Maphash, "maphash function hash-table"},
    {LispFunction, Lisp_Mapl, "mapl function list &rest more-lists"},
    {LispFunction, Lisp_Maplist, "maplist function list &rest more-lists"},
    {LispFunction, Lisp_Mapcon, "mapcon function list &rest more-lists"},
    {LispFunction, Lisp_Member, "member item list &key test test-not key"},
    {LispFunction, Lisp_MemberIf, "member-if predicate list &key key"},
    {LispFunction, Lisp_MemberIfNot, "member-if-not predicate list &key key"},
    {LispFunction, Lisp_Minusp, "minusp number"},
    {LispFunction, Lisp_Mod, "mod number divisor"},
    {LispMacro, Lisp_MultipleValueBind, "multiple-value-bind symbols values &rest body"},
    {LispMacro, Lisp_MultipleValueCall, "multiple-value-call function &rest form", 1},
    {LispMacro, Lisp_MultipleValueProg1, "multiple-value-prog1 first-form &rest form", 1},
    {LispMacro, Lisp_MultipleValueList, "multiple-value-list form"},
    {LispMacro, Lisp_MultipleValueSetq, "multiple-value-setq symbols form"},
    {LispFunction, Lisp_Nconc, "nconc &rest lists"},
    {LispFunction, Lisp_Nreverse, "nreverse sequence"},
    {LispFunction, Lisp_NsetDifference, "nset-difference list1 list2 &key test test-not key"},
    {LispFunction, Lisp_Nsubstitute, "nsubstitute newitem olditem sequence &key from-end test test-not start end count key"},
    {LispFunction, Lisp_NsubstituteIf, "nsubstitute-if newitem test sequence &key from-end start end count key"},
    {LispFunction, Lisp_NsubstituteIfNot, "nsubstitute-if-not newitem test sequence &key from-end start end count key"},
    {LispFunction, Lisp_Nth, "nth index list"},
    {LispFunction, Lisp_Nthcdr, "nthcdr index list", 0, 0, Com_Nthcdr},
    {LispMacro, Lisp_NthValue, "nth-value index form"},
    {LispFunction, Lisp_Numerator, "numerator rational"},
    {LispFunction, Lisp_Namestring, "namestring pathname"},
    {LispFunction, Lisp_Null, "not arg", 0, 0, Com_Null},
    {LispFunction, Lisp_Null, "null list", 0, 0, Com_Null},
    {LispFunction, Lisp_Numberp, "numberp object", 0, 0, Com_Numberp},
    {LispFunction, Lisp_Oddp, "oddp integer"},
    {LispFunction, Lisp_Open, "open filename &key direction element-type if-exists if-does-not-exist external-format"},
    {LispFunction, Lisp_OpenStreamP, "open-stream-p stream"},
    {LispMacro, Lisp_Or, "or &rest args", 1, 0, Com_Or},
    {LispFunction, Lisp_OutputStreamP, "output-stream-p stream"},
    {LispFunction, Lisp_Packagep, "packagep object"},
    {LispFunction, Lisp_PackageName, "package-name package"},
    {LispFunction, Lisp_PackageNicknames, "package-nicknames package"},
    {LispFunction, Lisp_PackageUseList, "package-use-list package"},
    {LispFunction, Lisp_PackageUsedByList, "package-used-by-list package"},
    {LispFunction, Lisp_Pairlis, "pairlis key data &optional alist"},
    {LispFunction, Lisp_ParseInteger, "parse-integer string &key start end radix junk-allowed", 1},
    {LispFunction, Lisp_ParseNamestring, "parse-namestring object &optional host defaults &key start end junk-allowed", 1},
    {LispFunction, Lisp_PathnameHost, "pathname-host pathname"},
    {LispFunction, Lisp_PathnameDevice, "pathname-device pathname"},
    {LispFunction, Lisp_PathnameDirectory, "pathname-directory pathname"},
    {LispFunction, Lisp_PathnameName, "pathname-name pathname"},
    {LispFunction, Lisp_PathnameType, "pathname-type pathname"},
    {LispFunction, Lisp_PathnameVersion, "pathname-version pathname"},
    {LispFunction, Lisp_Pathnamep, "pathnamep object"},
    {LispFunction, Lisp_Plusp, "plusp number"},
    {LispMacro, Lisp_Pop, "pop place"},
    {LispFunction, Lisp_Position, "position item sequence &key from-end test test-not start end key"},
    {LispFunction, Lisp_PositionIf, "position-if predicate sequence &key from-end start end key"},
    {LispFunction, Lisp_PositionIfNot, "position-if-not predicate sequence &key from-end start end key"},
    {LispFunction, Lisp_Prin1, "prin1 object &optional output-stream"},
    {LispFunction, Lisp_Princ, "princ object &optional output-stream"},
    {LispFunction, Lisp_Print, "print object &optional output-stream"},
    {LispFunction, Lisp_ProbeFile, "probe-file pathname"},
    {LispFunction, Lisp_Proclaim, "proclaim declaration"},
    {LispMacro, Lisp_Prog1, "prog1 first &rest body"},
    {LispMacro, Lisp_Prog2, "prog2 first second &rest body"},
    {LispMacro, Lisp_Progn, "progn &rest body", 1, 0, Com_Progn},
    {LispMacro, Lisp_Progv, "progv symbols values &rest body", 1},
    {LispFunction, Lisp_Provide, "provide module"},
    {LispMacro, Lisp_Push, "push item place"},
    {LispMacro, Lisp_Pushnew, "pushnew item place &key key test test-not"},
    {LispFunction, Lisp_Quit, "quit &optional status"},
    {LispMacro, Lisp_Quote, "quote object"},
    {LispFunction, Lisp_Rational, "rational number"},
    {LispFunction, Lisp_Rationalp, "rationalp object"},
    {LispFunction, Lisp_Read, "read &optional input-stream eof-error-p eof-value recursive-p"},
    {LispFunction, Lisp_ReadChar, "read-char &optional input-stream eof-error-p eof-value recursive-p"},
    {LispFunction, Lisp_ReadCharNoHang, "read-char-no-hang &optional input-stream eof-error-p eof-value recursive-p"},
    {LispFunction, Lisp_ReadLine, "read-line &optional input-stream eof-error-p eof-value recursive-p", 1},
    {LispFunction, Lisp_Realpart, "realpart number"},
    {LispFunction, Lisp_Replace, "replace sequence1 sequence2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_ReadFromString, "read-from-string string &optional eof-error-p eof-value &key start end preserve-whitespace", 1},
    {LispFunction, Lisp_Require, "require module &optional pathname"},
    {LispFunction, Lisp_Rem, "rem number divisor"},
    {LispFunction, Lisp_Remhash, "remhash key hash-table"},
    {LispFunction, Lisp_Remove, "remove item sequence &key from-end test test-not start end count key"},
    {LispFunction, Lisp_RemoveDuplicates, "remove-duplicates sequence &key from-end test test-not start end key"},
    {LispFunction, Lisp_RemoveIf, "remove-if predicate sequence &key from-end start end count key"},
    {LispFunction, Lisp_RemoveIfNot, "remove-if-not predicate sequence &key from-end start end count key"},
    {LispFunction, Lisp_Remprop, "remprop symbol indicator"},
    {LispFunction, Lisp_RenameFile, "rename-file filename new-name", 1},
    {LispMacro, Lisp_Return, "return &optional result", 1, 0, Com_Return},
    {LispMacro, Lisp_ReturnFrom, "return-from name &optional result", 1, 0, Com_ReturnFrom},
    {LispFunction, Lisp_Reverse, "reverse sequence"},
    {LispFunction, Lisp_Round, "round number &optional divisor", 1},
    {LispFunction, Lisp_Fround, "fround number &optional divisor", 1},
    {LispFunction, Lisp_Rplaca, "rplaca place value", 0, 0, Com_Rplac_},
    {LispFunction, Lisp_Rplacd, "rplacd place value", 0, 0, Com_Rplac_},
    {LispFunction, Lisp_Search, "search sequence1 sequence2 &key from-end test test-not key start1 start2 end1 end2"},
    {LispFunction, Lisp_Set, "set symbol value"},
    {LispFunction, Lisp_SetDifference, "set-difference list1 list2 &key test test-not key"},
    {LispFunction, Lisp_SetExclusiveOr, "set-exclusive-or list1 list2 &key test test-not key"},
    {LispFunction, Lisp_NsetExclusiveOr, "nset-exclusive-or list1 list2 &key test test-not key"},
    {LispMacro, Lisp_Setf, "setf &rest form"},
    {LispMacro, Lisp_Psetf, "psetf &rest form"},
    {LispMacro, Lisp_SetQ, "setq &rest form", 0, 0, Com_Setq},
    {LispMacro, Lisp_Psetq, "psetq &rest form"},
    {LispFunction, Lisp_Sleep, "sleep seconds"},
    {LispFunction, Lisp_Sort, "sort sequence predicate &key key"},
    {LispFunction, Lisp_Sqrt, "sqrt number"},
    {LispFunction, Lisp_Elt, "svref sequence index"},
    {LispFunction, Lisp_Sort, "stable-sort sequence predicate &key key"},
    {LispFunction, Lisp_Streamp, "streamp object"},
    {LispFunction, Lisp_String, "string object"},
    {LispFunction, Lisp_Stringp, "stringp object"},
    {LispFunction, Lisp_StringEqual_, "string= string1 string2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_StringLess, "string< string1 string2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_StringGreater, "string> string1 string2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_StringLessEqual, "string<= string1 string2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_StringGreaterEqual, "string>= string1 string2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_StringNotEqual_, "string/= string1 string2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_StringConcat, "string-concat &rest strings"},
    {LispFunction, Lisp_StringEqual, "string-equal string1 string2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_StringGreaterp, "string-greaterp string1 string2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_StringNotEqual, "string-not-equal string1 string2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_StringNotGreaterp, "string-not-greaterp string1 string2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_StringNotLessp, "string-not-lessp string1 string2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_StringLessp, "string-lessp string1 string2 &key start1 end1 start2 end2"},
    {LispFunction, Lisp_StringTrim, "string-trim character-bag string"},
    {LispFunction, Lisp_StringLeftTrim, "string-left-trim character-bag string"},
    {LispFunction, Lisp_StringRightTrim, "string-right-trim character-bag string"},
    {LispFunction, Lisp_StringUpcase, "string-upcase string &key start end"},
    {LispFunction, Lisp_NstringUpcase, "nstring-upcase string &key start end"},
    {LispFunction, Lisp_StringDowncase, "string-downcase string &key start end"},
    {LispFunction, Lisp_NstringDowncase, "nstring-downcase string &key start end"},
    {LispFunction, Lisp_StringCapitalize, "string-capitalize string &key start end"},
    {LispFunction, Lisp_NstringCapitalize, "nstring-capitalize string &key start end"},
    {LispFunction, Lisp_Subseq, "subseq sequence start &optional end"},
    {LispFunction, Lisp_Subsetp, "subsetp list1 list2 &key test test-not key"},
    {LispFunction, Lisp_Substitute, "substitute newitem olditem sequence &key from-end test test-not start end count key"},
    {LispFunction, Lisp_SubstituteIf, "substitute-if newitem test sequence &key from-end start end count key"},
    {LispFunction, Lisp_SubstituteIfNot, "substitute-if-not newitem test sequence &key from-end start end count key"},
    {LispFunction, Lisp_SymbolFunction, "symbol-function symbol"},
    {LispFunction, Lisp_SymbolName, "symbol-name symbol"},
    {LispFunction, Lisp_Symbolp, "symbolp object"},
    {LispFunction, Lisp_SymbolPlist, "symbol-plist symbol"},
    {LispFunction, Lisp_SymbolPackage, "symbol-package symbol"},
    {LispFunction, Lisp_SymbolValue, "symbol-value symbol"},
    {LispMacro, Lisp_Tagbody, "tagbody &rest body", 0, 0, Com_Tagbody},
    {LispFunction, Lisp_Terpri, "terpri &optional output-stream"},
    {LispFunction, Lisp_Typep, "typep object type"},
    {LispMacro, Lisp_The, "the value-type form"},
    {LispMacro, Lisp_Throw, "throw tag result", 1},
    {LispMacro, Lisp_Time, "time form"},
    {LispFunction, Lisp_Truename, "truename pathname"},
    {LispFunction, Lisp_TreeEqual, "tree-equal tree-1 tree-2 &key test test-not"},
    {LispFunction, Lisp_Truncate, "truncate number &optional divisor", 1},
    {LispFunction, Lisp_Ftruncate, "ftruncate number &optional divisor", 1},
    {LispFunction, Lisp_Unexport, "unexport symbols &optional package"},
    {LispFunction, Lisp_Union, "union list1 list2 &key test test-not key"},
    {LispFunction, Lisp_Nunion, "nunion list1 list2 &key test test-not key"},
    {LispMacro, Lisp_Unless, "unless test &rest body", 1, 0, Com_Unless},
    {LispFunction, Lisp_UserHomedirPathname, "user-homedir-pathname &optional host"},
    {LispMacro, Lisp_UnwindProtect, "unwind-protect protect &rest cleanup"},
    {LispFunction, Lisp_UpperCaseP, "upper-case-p character"},
    {LispFunction, Lisp_Values, "values &rest objects", 1},
    {LispFunction, Lisp_ValuesList, "values-list list", 1},
    {LispFunction, Lisp_Vector, "vector &rest objects"},
    {LispMacro, Lisp_When, "when test &rest body", 1, 0, Com_When},
    {LispFunction, Lisp_Write, " write object &key case circle escape length level lines pretty readably right-margin stream"},
    {LispFunction, Lisp_WriteChar, "write-char string &optional output-stream"},
    {LispFunction, Lisp_WriteLine, "write-line string &optional output-stream &key start end"},
    {LispFunction, Lisp_WriteString, "write-string string &optional output-stream &key start end"},
    {LispFunction, Lisp_XeditCharStore, "lisp::char-store string index value", 0, 1},
    {LispFunction, Lisp_XeditEltStore, "lisp::elt-store sequence index value", 0, 1},
    {LispFunction, Lisp_XeditMakeStruct, "lisp::make-struct atom &rest init", 0, 1},
    {LispFunction, Lisp_XeditPut, " lisp::put symbol indicator value", 0, 1},
    {LispFunction, Lisp_XeditPuthash, "lisp::puthash key hash-table value", 0, 1},
    {LispFunction, Lisp_XeditSetSymbolPlist, "lisp::set-symbol-plist symbol list", 0, 1},
    {LispFunction, Lisp_XeditStructAccess, "lisp::struct-access atom struct", 0, 1},
    {LispFunction, Lisp_XeditStructType, "lisp::struct-type atom struct", 0, 1},
    {LispFunction, Lisp_XeditStructStore, "lisp::struct-store atom struct value", 0, 1},
    {LispFunction, Lisp_XeditVectorStore, "lisp::vector-store array &rest values", 0, 1},
    {LispFunction, Lisp_XeditDocumentationStore, "lisp::documentation-store symbol type string", 0, 1},
    {LispFunction, Lisp_Zerop, "zerop number"},
};

static LispBuiltin extbuiltins[] = {
    {LispFunction, Lisp_Getenv, "getenv name"},
    {LispFunction, Lisp_MakePipe, "make-pipe command-line &key direction element-type external-format"},
    {LispFunction, Lisp_PipeBroken, "pipe-broken pipe-stream"},
    {LispFunction, Lisp_PipeErrorStream, "pipe-error-stream pipe-stream"},
    {LispFunction, Lisp_PipeInputDescriptor, "pipe-input-descriptor pipe-stream"},
    {LispFunction, Lisp_PipeErrorDescriptor, "pipe-error-descriptor pipe-stream"},
    {LispFunction, Lisp_Recomp, "re-comp pattern &key nospec icase nosub newline"},
    {LispFunction, Lisp_Reexec, "re-exec regex string &key count start end notbol noteol"},
    {LispFunction, Lisp_Rep, "re-p object"},
    {LispFunction, Lisp_Setenv, "setenv name value &optional overwrite"},
    {LispFunction, Lisp_Unsetenv, "unsetenv name"},
    {LispFunction, Lisp_NstringTrim, "nstring-trim character-bag string"},
    {LispFunction, Lisp_NstringLeftTrim, "nstring-left-trim character-bag string"},
    {LispFunction, Lisp_NstringRightTrim, "nstring-right-trim character-bag string"},
    {LispMacro, Lisp_Until, "until test &rest body", 0, 0, Com_Until},
    {LispMacro, Lisp_While, "while test &rest body", 0, 0, Com_While},
};

/* byte code function argument list for functions that don't change it's
 * &REST argument list. */
extern LispObj x_cons[8];

/*
 * Implementation
 */
static int
LispGetPageSize(void)
{
    static int pagesize = -1;

    if (pagesize != -1)
	return pagesize;

    /* Try each supported method in the preferred order */

#if defined(_SC_PAGESIZE) || defined(HAS_SC_PAGESIZE)
    pagesize = sysconf(_SC_PAGESIZE);
#endif

#ifdef _SC_PAGE_SIZE
    if (pagesize == -1)
	pagesize = sysconf(_SC_PAGE_SIZE);
#endif

#ifdef HAS_GETPAGESIZE
    if (pagesize == -1)
	pagesize = getpagesize();
#endif

#ifdef PAGE_SIZE
    if (pagesize == -1)
	pagesize = PAGE_SIZE;
#endif

    if (pagesize < sizeof(LispObj) * 16)
	pagesize = sizeof(LispObj) * 16;	/* need a reasonable sane size */

    return pagesize;
}

void
LispDestroy(char *fmt, ...)
{
    static char Error[] = "*** ";

    if (!lisp__data.destroyed) {
	char string[128];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(string, sizeof(string), fmt, ap);
	va_end(ap);

	if (!lisp__data.ignore_errors) {
	    if (Stderr->column)
		LispFputc(Stderr, '\n');
	    LispFputs(Stderr, Error);
	    LispFputs(Stderr, string);
	    LispFputc(Stderr, '\n');
	    LispFflush(Stderr);
	}
	else
	    lisp__data.error_condition = STRING(string);

#ifdef DEBUGGER
	if (lisp__data.debugging) {
	    LispDebugger(LispDebugCallWatch, NIL, NIL);
	    LispDebugger(LispDebugCallFatal, NIL, NIL);
	}
#endif

	lisp__data.destroyed = 1;
	LispBlockUnwind(NULL);
	if (lisp__data.errexit)
	    exit(1);
    }

#ifdef DEBUGGER
    if (lisp__data.debugging) {
	/* when stack variables could be changed, this must be also changed! */
	lisp__data.debug_level = -1;
	lisp__data.debug = LispDebugUnspec;
    }
#endif

    while (lisp__data.mem.level) {
	--lisp__data.mem.level;
	if (lisp__data.mem.mem[lisp__data.mem.level])
	    free(lisp__data.mem.mem[lisp__data.mem.level]);
    }
    lisp__data.mem.index = 0;

    /* If the package was changed and an error happened */
    PACKAGE = lisp__data.savepackage;
    lisp__data.pack = lisp__data.savepack;

    LispTopLevel();

    if (!lisp__data.running) {
	static char Fatal[] = "*** Fatal: nowhere to longjmp.\n";

	LispFputs(Stderr, Fatal);
	LispFflush(Stderr);
	abort();
    }

    siglongjmp(lisp__data.jmp, 1);
}

void
LispContinuable(char *fmt, ...)
{
    va_list ap;
    char string[128];
    static char Error[] = "*** Error: ";

    if (Stderr->column)
	LispFputc(Stderr, '\n');
    LispFputs(Stderr, Error);
    va_start(ap, fmt);
    vsnprintf(string, sizeof(string), fmt, ap);
    va_end(ap);
    LispFputs(Stderr, string);
    LispFputc(Stderr, '\n');
    LispFputs(Stderr, "Type 'continue' if you want to proceed: ");
    LispFflush(Stderr);

    /* NOTE: does not check if stdin is a tty */
    if (LispFgets(Stdin, string, sizeof(string)) &&
	strcmp(string, "continue\n") == 0)
	return;

    LispDestroy("aborted on continuable error");
}

void
LispMessage(char *fmt, ...)
{
    va_list ap;
    char string[128];

    if (Stderr->column)
	LispFputc(Stderr, '\n');
    va_start(ap, fmt);
    vsnprintf(string, sizeof(string), fmt, ap);
    va_end(ap);
    LispFputs(Stderr, string);
    LispFputc(Stderr, '\n');
    LispFflush(Stderr);
}

void
LispWarning(char *fmt, ...)
{
    va_list ap;
    char string[128];
    static char Warning[] = "*** Warning: ";

    if (Stderr->column)
	LispFputc(Stderr, '\n');
    LispFputs(Stderr, Warning);
    va_start(ap, fmt);
    vsnprintf(string, sizeof(string), fmt, ap);
    va_end(ap);
    LispFputs(Stderr, string);
    LispFputc(Stderr, '\n');
    LispFflush(Stderr);
}

void
LispTopLevel(void)
{
    int count;

    COD = NIL;
#ifdef DEBUGGER
    if (lisp__data.debugging) {
	DBG = NIL;
	if (lisp__data.debug == LispDebugFinish)
	    lisp__data.debug = LispDebugUnspec;
	lisp__data.debug_level = -1;
	lisp__data.debug_step = 0;
    }
#endif
    gcpro = 0;
    lisp__data.block.block_level = 0;
    if (lisp__data.block.block_size) {
	while (lisp__data.block.block_size)
	    free(lisp__data.block.block[--lisp__data.block.block_size]);
	free(lisp__data.block.block);
	lisp__data.block.block = NULL;
    }

    lisp__data.destroyed = lisp__data.ignore_errors = 0;

    if (CONSP(lisp__data.input_list)) {
	LispUngetInfo **info, *unget = lisp__data.unget[0];

	while (CONSP(lisp__data.input_list))
	    lisp__data.input_list = CDR(lisp__data.input_list);
	SINPUT = lisp__data.input_list;
	while (lisp__data.nunget > 1)
	    free(lisp__data.unget[--lisp__data.nunget]);
	if ((info = realloc(lisp__data.unget, sizeof(LispUngetInfo*))) != NULL)
	    lisp__data.unget = info;
	lisp__data.unget[0] = unget;
	lisp__data.iunget = 0;
	lisp__data.eof = 0;
    }

    for (count = 0; lisp__data.mem.level;) {
	--lisp__data.mem.level;
	if (lisp__data.mem.mem[lisp__data.mem.level]) {
	    ++count;
#if 0
	    printf("LEAK: %p\n", lisp__data.mem.mem[lisp__data.mem.level]);
#endif
	}
    }
    lisp__data.mem.index = 0;
    if (count)
	LispWarning("%d raw memory pointer(s) left. Probably a leak.", count);

    lisp__data.stack.base = lisp__data.stack.length =
	lisp__data.env.lex = lisp__data.env.length = lisp__data.env.head = 0;
    RETURN_COUNT = 0;
    lisp__data.protect.length = 0;

    lisp__data.savepackage = PACKAGE;
    lisp__data.savepack = lisp__data.pack;

    lisp__disable_int = lisp__interrupted = 0;
}

void
LispGC(LispObj *car, LispObj *cdr)
{
    Lisp__GC(car, cdr);
}

static void
Lisp__GC(LispObj *car, LispObj *cdr)
{
    register LispObj *entry, *last, *freeobj, **pentry, **eentry;
    register int nfree;
    unsigned i, j;
    LispAtom *atom;
    struct timeval start, end;
#ifdef DEBUG
    long sec, msec;
    int count = objseg.nfree;
#else
    long msec;
#endif

    if (gcpro)
	return;

    DISABLE_INTERRUPTS();

    nfree = 0;
    freeobj = NIL;

    ++lisp__data.gc.count;

#ifdef DEBUG
    gettimeofday(&start, NULL);
#else
    if (lisp__data.gc.timebits)
	gettimeofday(&start, NULL);
#endif

    /*  Need to measure timings again to check if it is not better/faster
     * to just mark these fields as any other data, as the interface was
     * changed to properly handle circular lists in the function body itself.
     */
    if (lisp__data.gc.immutablebits) {
	for (j = 0; j < objseg.nsegs; j++) {
	    for (entry = objseg.objects[j], last = entry + segsize;
		 entry < last; entry++)
		entry->prot = 0;
	}
    }

    /* Protect all packages */
    for (entry = PACK; CONSP(entry); entry = CDR(entry)) {
	LispObj *package = CAR(entry);
	LispPackage *pack = package->data.package.package;

	/* Protect cons cell */
	entry->mark = 1;

	/* Protect the package cell */
	package->mark = 1;

	/* Protect package name */
	package->data.package.name->mark = 1;

	/* Protect package nicknames */
	LispMark(package->data.package.nicknames);

	/* Protect global symbols */
	for (pentry = pack->glb.pairs, eentry = pentry + pack->glb.length;
	    pentry < eentry; pentry++)
	    LispMark((*pentry)->data.atom->property->value);

	/* Traverse atom list, protecting properties, and function/structure
	 * definitions if lisp__data.gc.immutablebits set */
	for (i = 0; i < STRTBLSZ; i++) {
	    atom = pack->atoms[i];
	    while (atom) {
		if (atom->property != NOPROPERTY) {
		    if (atom->a_property)
			LispMark(atom->property->properties);
		    if (lisp__data.gc.immutablebits) {
			if (atom->a_function || atom->a_compiled)
			    LispProt(atom->property->fun.function);
			if (atom->a_defsetf)
			    LispProt(atom->property->setf);
			if (atom->a_defstruct)
			    LispProt(atom->property->structure.definition);
		    }
		}
		atom = atom->next;
	    }
	}
    }

    /* protect environment */
    for (pentry = lisp__data.env.values,
	 eentry = pentry + lisp__data.env.length;
	 pentry < eentry; pentry++)
	LispMark(*pentry);

    /* protect multiple return values */
    for (pentry = lisp__data.returns.values,
	 eentry = pentry + lisp__data.returns.count;
	 pentry < eentry; pentry++)
	LispMark(*pentry);

    /* protect stack of arguments to builtin functions */
    for (pentry = lisp__data.stack.values,
	 eentry = pentry + lisp__data.stack.length;
	 pentry < eentry; pentry++)
	LispMark(*pentry);

    /* protect temporary data used by builtin functions */
    for (pentry = lisp__data.protect.objects,
	 eentry = pentry + lisp__data.protect.length;
	 pentry < eentry; pentry++)
	LispMark(*pentry);

    for (i = 0; i < sizeof(x_cons) / sizeof(x_cons[0]); i++)
	x_cons[i].mark = 0;

    LispMark(COD);
#ifdef DEBUGGER
    LispMark(DBG);
    LispMark(BRK);
#endif
    LispMark(PRO);
    LispMark(lisp__data.input_list);
    LispMark(lisp__data.output_list);
    LispMark(car);
    LispMark(cdr);

    for (j = 0; j < objseg.nsegs; j++) {
	for (entry = objseg.objects[j], last = entry + segsize;
	     entry < last; entry++) {
	    if (entry->prot)
		continue;
	    else if (entry->mark)
		entry->mark = 0;
	    else {
		switch (XOBJECT_TYPE(entry)) {
		    case LispString_t:
			free(THESTR(entry));
			entry->type = LispCons_t;
			break;
		    case LispStream_t:
			switch (entry->data.stream.type) {
			    case LispStreamString:
				free(SSTREAMP(entry)->string);
				free(SSTREAMP(entry));
				break;
			    case LispStreamFile:
				if (FSTREAMP(entry))
				    LispFclose(FSTREAMP(entry));
				break;
			    case LispStreamPipe:
				/* XXX may need special handling if child hangs */
				if (PSTREAMP(entry)) {
				    if (IPSTREAMP(entry))
					LispFclose(IPSTREAMP(entry));
				    if (OPSTREAMP(entry))
					LispFclose(OPSTREAMP(entry));
				    /* don't bother with error stream, will also
				     * freed in this GC call, maybe just out
				     * of order */
				    if (PIDPSTREAMP(entry) > 0) {
					kill(PIDPSTREAMP(entry), SIGTERM);
					waitpid(PIDPSTREAMP(entry), NULL, 0);
				    }
				    free(PSTREAMP(entry));
				}
				break;
			    default:
				break;
			}
			entry->type = LispCons_t;
			break;
		    case LispBignum_t:
			mpi_clear(entry->data.mp.integer);
			free(entry->data.mp.integer);
			entry->type = LispCons_t;
			break;
		    case LispBigratio_t:
			mpr_clear(entry->data.mp.ratio);
			free(entry->data.mp.ratio);
			entry->type = LispCons_t;
			break;
		    case LispLambda_t:
			if (!SYMBOLP(entry->data.lambda.name))
			    LispFreeArgList((LispArgList*)
				entry->data.lambda.name->data.opaque.data);
			entry->type = LispCons_t;
			break;
		    case LispRegex_t:
			refree(entry->data.regex.regex);
			free(entry->data.regex.regex);
			entry->type = LispCons_t;
			break;
		    case LispBytecode_t:
			free(entry->data.bytecode.bytecode->code);
			free(entry->data.bytecode.bytecode);
			entry->type = LispCons_t;
			break;
		    case LispHashTable_t:
			LispFreeHashTable(entry->data.hash.table);
			entry->type = LispCons_t;
			break;
		    case LispCons_t:
			break;
		    default:
			entry->type = LispCons_t;
			break;
		}
		CDR(entry) = freeobj;
		freeobj = entry;
		++nfree;
	    }
	}
    }

    objseg.nfree = nfree;
    objseg.freeobj = freeobj;

    lisp__data.gc.immutablebits = 0;

#ifdef DEBUG
    gettimeofday(&end, NULL);
    sec = end.tv_sec - start.tv_sec;
    msec = end.tv_usec - start.tv_usec;
    if (msec < 0) {
	--sec;
	msec += 1000000;
    }
    LispMessage("gc: "
		"%ld sec, %ld msec, "
		"%d recovered, %d free, %d protected, %d total",
		sec, msec,
		objseg.nfree - count, objseg.nfree,
		objseg.nobjs - objseg.nfree, objseg.nobjs);
#else
    if (lisp__data.gc.timebits) {
	gettimeofday(&end, NULL);
	if ((msec = end.tv_usec - start.tv_usec) < 0)
	    msec += 1000000;
	lisp__data.gc.gctime += msec;
    }
#endif

    ENABLE_INTERRUPTS();
}

static INLINE void
LispCheckMemLevel(void)
{
    int i;

    /* Check for a free slot before the end. */
    for (i = lisp__data.mem.index; i < lisp__data.mem.level; i++)
	if (lisp__data.mem.mem[i] == NULL) {
	    lisp__data.mem.index = i;
	    return;
	}

    /* Check for a free slot in the beginning */
    for (i = 0; i < lisp__data.mem.index; i++)
	if (lisp__data.mem.mem[i] == NULL) {
	    lisp__data.mem.index = i;
	    return;
	}

    lisp__data.mem.index = lisp__data.mem.level;
    ++lisp__data.mem.level;
    if (lisp__data.mem.index < lisp__data.mem.space)
	/* There is free space to store pointer. */
	return;
    else {
	void **ptr = (void**)realloc(lisp__data.mem.mem,
				     (lisp__data.mem.space + 16) *
				     sizeof(void*));

	if (ptr == NULL)
	    LispDestroy("out of memory");
	lisp__data.mem.mem = ptr;
	lisp__data.mem.space += 16;
    }
}

void
LispMused(void *pointer)
{
    int i;

    DISABLE_INTERRUPTS();
    for (i = lisp__data.mem.index; i >= 0; i--)
	if (lisp__data.mem.mem[i] == pointer) {
	    lisp__data.mem.mem[i] = NULL;
	    lisp__data.mem.index = i;
	    goto mused_done;
	}

    for (i = lisp__data.mem.level - 1; i > lisp__data.mem.index; i--)
	if (lisp__data.mem.mem[i] == pointer) {
	    lisp__data.mem.mem[i] = NULL;
	    lisp__data.mem.index = i;
	    break;
	}

mused_done:
    ENABLE_INTERRUPTS();
}

void *
LispMalloc(size_t size)
{
    void *pointer;

    DISABLE_INTERRUPTS();
    LispCheckMemLevel();
    if ((pointer = malloc(size)) == NULL)
	LispDestroy("out of memory, couldn't allocate %lu bytes",
		    (unsigned long)size);

    lisp__data.mem.mem[lisp__data.mem.index] = pointer;
    ENABLE_INTERRUPTS();

    return (pointer);
}

void *
LispCalloc(size_t nmemb, size_t size)
{
    void *pointer;

    DISABLE_INTERRUPTS();
    LispCheckMemLevel();
    if ((pointer = calloc(nmemb, size)) == NULL)
	LispDestroy("out of memory, couldn't allocate %lu bytes",
		    (unsigned long)size);

    lisp__data.mem.mem[lisp__data.mem.index] = pointer;
    ENABLE_INTERRUPTS();

    return (pointer);
}

void *
LispRealloc(void *pointer, size_t size)
{
    void *ptr;
    int i;

    DISABLE_INTERRUPTS();
    if (pointer != NULL) {
	for (i = lisp__data.mem.index; i >= 0; i--)
	    if (lisp__data.mem.mem[i] == pointer)
		goto index_found;

	for (i = lisp__data.mem.index + 1; i < lisp__data.mem.level; i++)
	    if (lisp__data.mem.mem[i] == pointer)
		goto index_found;

    }
    LispCheckMemLevel();
    i = lisp__data.mem.index;

index_found:
    if ((ptr = realloc(pointer, size)) == NULL)
	LispDestroy("out of memory, couldn't realloc");

    lisp__data.mem.mem[i] = ptr;
    ENABLE_INTERRUPTS();

    return (ptr);
}

char *
LispStrdup(char *str)
{
    char *ptr = LispMalloc(strlen(str) + 1);

    strcpy(ptr, str);

    return (ptr);
}

void
LispFree(void *pointer)
{
    int i;

    DISABLE_INTERRUPTS();
    for (i = lisp__data.mem.index; i >= 0; i--)
	if (lisp__data.mem.mem[i] == pointer) {
	    lisp__data.mem.mem[i] = NULL;
	    lisp__data.mem.index = i;
	    goto free_done;
	}

    for (i = lisp__data.mem.level - 1; i > lisp__data.mem.index; i--)
	if (lisp__data.mem.mem[i] == pointer) {
	    lisp__data.mem.mem[i] = NULL;
	    lisp__data.mem.index = i;
	    break;
	}

free_done:
    free(pointer);
    ENABLE_INTERRUPTS();
}

LispObj *
LispSetVariable(LispObj *var, LispObj *val, char *fname, int eval)
{
    if (!SYMBOLP(var))
	LispDestroy("%s: %s is not a symbol", fname, STROBJ(var));
    if (eval)
	val = EVAL(val);

    return (LispSetVar(var, val));
}

int
LispRegisterOpaqueType(char *desc)
{
    LispOpaque *opaque;
    int ii = STRHASH(desc);

    for (opaque = lisp__data.opqs[ii]; opaque; opaque = opaque->next)
	if (strcmp(opaque->desc, desc) == 0)
	    return (opaque->type);
    opaque = (LispOpaque*)LispMalloc(sizeof(LispOpaque));
    opaque->desc = LispStrdup(desc);
    opaque->next = lisp__data.opqs[ii];
    lisp__data.opqs[ii] = opaque;
    LispMused(opaque->desc);
    LispMused(opaque);

    return (opaque->type = ++lisp__data.opaque);
}

char *
LispIntToOpaqueType(int type)
{
    int i;
    LispOpaque *opaque;

    if (type) {
	for (i = 0; i < STRTBLSZ; i++) {
	    opaque = lisp__data.opqs[i];
	    while (opaque) {
		if (opaque->type == type)
		    return (opaque->desc);
		opaque = opaque->next;
	    }
	}
	LispDestroy("Opaque type %d not registered", type);
    }

    return (Snil);
}

int
LispDoHashString(char *string)
{
    char *pp;
    int ii, count;

    for (pp = string, ii = count = 0; *pp && count < 32; pp++, count++)
	ii = (ii << 1) ^ *pp;
    if (ii < 0)
	ii = -ii;

    return (ii % STRTBLSZ);
}

char *
LispGetAtomString(char *string, int perm)
{
    LispStringHash *entry;
    int ii = STRHASH(string);

    for (entry = lisp__data.strings[ii]; entry != NULL; entry = entry->next)
	if (strcmp(entry->string, string) == 0)
	    return (entry->string);

    entry = (LispStringHash*)LispCalloc(1, sizeof(LispStringHash));
    if (perm)
	entry->string = string;
    else
	entry->string = LispStrdup(string);
    LispMused(entry);
    if (!perm)
	LispMused(entry->string);
    entry->next = lisp__data.strings[ii];
    lisp__data.strings[ii] = entry;

    return (entry->string);
}

LispAtom *
LispDoGetAtom(char *str, int perm)
{
    LispAtom *atom;
    int ii = STRHASH(str);

    for (atom = lisp__data.pack->atoms[ii]; atom; atom = atom->next)
	if (strcmp(atom->string, str) == 0)
	    return (atom);

    atom = (LispAtom*)LispCalloc(1, sizeof(LispAtom));
    atom->string = LispGetAtomString(str, perm);
    LispMused(atom);
    atom->next = lisp__data.pack->atoms[ii];
    lisp__data.pack->atoms[ii] = atom;
    atom->property = NOPROPERTY;

    return (atom);
}

static void
LispAllocAtomProperty(LispAtom *atom)
{
    LispProperty *property;

    if (atom->property != NOPROPERTY)
	LispDestroy("internal error at ALLOC-ATOM-PROPERTY");

    property = LispCalloc(1, sizeof(LispProperty));
    LispMused(property);
    atom->property = property;
    property->package = lisp__data.pack;
    if (atom->package == NULL)
	atom->package = PACKAGE;

    LispIncrementAtomReference(atom);
}

static void
LispIncrementAtomReference(LispAtom *atom)
{
    if (atom->property != NOPROPERTY)
	/* if atom->property is NOPROPERTY, this is an unbound symbol */
	++atom->property->refcount;
}

/* Assumes atom property is not NOPROPERTY */
static void
LispDecrementAtomReference(LispAtom *atom)
{
    if (atom->property == NOPROPERTY)
	/* if atom->property is NOPROPERTY, this is an unbound symbol */
	return;

    if (atom->property->refcount <= 0)
	LispDestroy("internal error at DECREMENT-ATOM-REFERENCE");

    --atom->property->refcount;

    if (atom->property->refcount == 0) {
	LispRemAtomAllProperties(atom);
	free(atom->property);
	atom->property = NOPROPERTY;
    }
}

static void
LispRemAtomAllProperties(LispAtom *atom)
{
    if (atom->property != NOPROPERTY) {
	if (atom->a_object)
	    LispRemAtomObjectProperty(atom);
	if (atom->a_function) {
	    lisp__data.gc.immutablebits = 1;
	    LispRemAtomFunctionProperty(atom);
	}
	else if (atom->a_compiled) {
	    lisp__data.gc.immutablebits = 1;
	    LispRemAtomCompiledProperty(atom);
	}
	else if (atom->a_builtin) {
	    lisp__data.gc.immutablebits = 1;
	    LispRemAtomBuiltinProperty(atom);
	}
	if (atom->a_defsetf) {
	    lisp__data.gc.immutablebits = 1;
	    LispRemAtomSetfProperty(atom);
	}
	if (atom->a_defstruct) {
	    lisp__data.gc.immutablebits = 1;
	    LispRemAtomStructProperty(atom);
	}
    }
}

void
LispSetAtomObjectProperty(LispAtom *atom, LispObj *object)
{
    if (atom->property == NOPROPERTY)
	LispAllocAtomProperty(atom);
    else if (atom->watch) {
	if (atom->object == lisp__data.package) {
	    if (!PACKAGEP(object))
		LispDestroy("Symbol %s must be a package, not %s",
			    ATOMID(lisp__data.package), STROBJ(object));
	    lisp__data.pack = object->data.package.package;
	}
    }

    atom->a_object = 1;
    SETVALUE(atom, object);
}

static void
LispRemAtomObjectProperty(LispAtom *atom)
{
    if (atom->a_object) {
	atom->a_object = 0;
	atom->property->value = NULL;
    }
}

void
LispSetAtomCompiledProperty(LispAtom *atom, LispObj *bytecode)
{
    if (atom->property == NOPROPERTY)
	LispAllocAtomProperty(atom);

    lisp__data.gc.immutablebits = 1;
    if (atom->a_builtin) {
	atom->a_builtin = 0;
	LispFreeArgList(atom->property->alist);
    }
    else
	atom->a_function = 0;
    atom->a_compiled = 1;
    atom->property->fun.function = bytecode;
}

void
LispRemAtomCompiledProperty(LispAtom *atom)
{
    if (atom->a_compiled) {
	lisp__data.gc.immutablebits = 1;
	atom->property->fun.function = NULL;
	atom->a_compiled = 0;
	LispFreeArgList(atom->property->alist);
	atom->property->alist = NULL;
    }
}

void
LispSetAtomFunctionProperty(LispAtom *atom, LispObj *function,
			    LispArgList *alist)
{
    if (atom->property == NOPROPERTY)
	LispAllocAtomProperty(atom);

    lisp__data.gc.immutablebits = 1;
    if (atom->a_function == 0 && atom->a_builtin == 0 && atom->a_compiled == 0)
	atom->a_function = 1;
    else {
	if (atom->a_builtin) {
	    atom->a_builtin = 0;
	    LispFreeArgList(atom->property->alist);
	}
	else
	    atom->a_compiled = 0;
	atom->a_function = 1;
    }

    atom->property->fun.function = function;
    atom->property->alist = alist;
}

void
LispRemAtomFunctionProperty(LispAtom *atom)
{
    if (atom->a_function) {
	lisp__data.gc.immutablebits = 1;
	atom->property->fun.function = NULL;
	atom->a_function = 0;
	LispFreeArgList(atom->property->alist);
	atom->property->alist = NULL;
    }
}

void
LispSetAtomBuiltinProperty(LispAtom *atom, LispBuiltin *builtin,
			   LispArgList *alist)
{
    if (atom->property == NOPROPERTY)
	LispAllocAtomProperty(atom);

    lisp__data.gc.immutablebits = 1;
    if (atom->a_builtin == 0 && atom->a_function == 0)
	atom->a_builtin = 1;
    else {
	if (atom->a_function) {
	    atom->a_function = 0;
	    LispFreeArgList(atom->property->alist);
	}
    }

    atom->property->fun.builtin = builtin;
    atom->property->alist = alist;
}

void
LispRemAtomBuiltinProperty(LispAtom *atom)
{
    if (atom->a_builtin) {
	lisp__data.gc.immutablebits = 1;
	atom->property->fun.function = NULL;
	atom->a_builtin = 0;
	LispFreeArgList(atom->property->alist);
	atom->property->alist = NULL;
    }
}

void
LispSetAtomSetfProperty(LispAtom *atom, LispObj *setf, LispArgList *alist)
{
    if (atom->property == NOPROPERTY)
	LispAllocAtomProperty(atom);

    lisp__data.gc.immutablebits = 1;
    if (atom->a_defsetf)
	LispFreeArgList(atom->property->salist);

    atom->a_defsetf = 1;
    atom->property->setf = setf;
    atom->property->salist = alist;
}

void
LispRemAtomSetfProperty(LispAtom *atom)
{
    if (atom->a_defsetf) {
	lisp__data.gc.immutablebits = 1;
	atom->property->setf = NULL;
	atom->a_defsetf = 0;
	LispFreeArgList(atom->property->salist);
	atom->property->salist = NULL;
    }
}

void
LispSetAtomStructProperty(LispAtom *atom, LispObj *def, int fun)
{
    if (fun > 0xff)
	/* Not suported by the bytecode compiler... */
	LispDestroy("SET-ATOM-STRUCT-PROPERTY: "
		    "more than 256 fields not supported");

    if (atom->property == NOPROPERTY)
	LispAllocAtomProperty(atom);

    lisp__data.gc.immutablebits = 1;
    atom->a_defstruct = 1;
    atom->property->structure.definition = def;
    atom->property->structure.function = fun;
}

void
LispRemAtomStructProperty(LispAtom *atom)
{
    if (atom->a_defstruct) {
	lisp__data.gc.immutablebits = 1;
	atom->property->structure.definition = NULL;
	atom->a_defstruct = 0;
    }
}

LispAtom *
LispGetAtom(char *str)
{
    return (LispDoGetAtom(str, 0));
}

LispAtom *
LispGetPermAtom(char *str)
{
    return (LispDoGetAtom(str, 1));
}

#define GET_PROPERTY	0
#define ADD_PROPERTY	1
#define REM_PROPERTY	2
static LispObj *
LispAtomPropertyFunction(LispAtom *atom, LispObj *key, int function)
{
    LispObj *list = NIL, *result = NIL;

    if (function == ADD_PROPERTY) {
	if (atom->property == NOPROPERTY)
	    LispAllocAtomProperty(atom);
	if (atom->property->properties == NULL) {
	    atom->a_property = 1;
	    atom->property->properties = NIL;
	}
    }

    if (atom->a_property) {
	LispObj *base;

	for (base = list = atom->property->properties;
	     CONSP(list);
	     list = CDR(list)) {
	    if (key == CAR(list)) {
		result = CDR(list);
		break;
	    }
	    base = list;
	    list = CDR(list);
	    if (!CONSP(list))
		LispDestroy("%s: %s has an odd property list length",
			    STROBJ(atom->object),
			    function == REM_PROPERTY ? "REMPROP" : "GET");
	}
	if (CONSP(list) && function == REM_PROPERTY) {
	    if (!CONSP(CDR(list)))
		LispDestroy("REMPROP: %s has an odd property list length",
			    STROBJ(atom->object));
	    if (base == list)
		atom->property->properties = CDDR(list);
	    else
		RPLACD(CDR(base), CDDR(list));
	}
    }

    if (!CONSP(list)) {
	if (function == ADD_PROPERTY) {
	    atom->property->properties =
		CONS(key, CONS(NIL, atom->property->properties));
	    result = CDR(atom->property->properties);
	}
    }
    else if (function == REM_PROPERTY)
	result = T;

    return (result);
}

LispObj *
LispGetAtomProperty(LispAtom *atom, LispObj *key)
{
    return (LispAtomPropertyFunction(atom, key, GET_PROPERTY));
}

LispObj *
LispPutAtomProperty(LispAtom *atom, LispObj *key, LispObj *value)
{
    LispObj *result = LispAtomPropertyFunction(atom, key, ADD_PROPERTY);

    RPLACA(result, value);

    return (result);
}

LispObj *
LispRemAtomProperty(LispAtom *atom, LispObj *key)
{
    return (LispAtomPropertyFunction(atom, key, REM_PROPERTY));
}

LispObj *
LispReplaceAtomPropertyList(LispAtom *atom, LispObj *list)
{
    if (atom->property == NOPROPERTY)
	LispAllocAtomProperty(atom);
    if (atom->property->properties == NULL)
	atom->a_property = 1;
    atom->property->properties = list;

    return (list);
}
#undef GET_PROPERTY
#undef ADD_PROPERTY
#undef REM_PROPERTY


/* Used to make sure that when defining a function like:
 *	(defun my-function (... &key key1 key2 key3 ...)
 * key1, key2, and key3 will be in the keyword package
 */
static LispObj *
LispCheckKeyword(LispObj *keyword)
{
    if (KEYWORDP(keyword))
	return (keyword);

    return (KEYWORD(ATOMID(keyword)));
}

void
LispUseArgList(LispArgList *alist)
{
    if (alist->normals.num_symbols)
	LispMused(alist->normals.symbols);
    if (alist->optionals.num_symbols) {
	LispMused(alist->optionals.symbols);
	LispMused(alist->optionals.defaults);
	LispMused(alist->optionals.sforms);
    }
    if (alist->keys.num_symbols) {
	LispMused(alist->keys.symbols);
	LispMused(alist->keys.defaults);
	LispMused(alist->keys.sforms);
	LispMused(alist->keys.keys);
    }
    if (alist->auxs.num_symbols) {
	LispMused(alist->auxs.symbols);
	LispMused(alist->auxs.initials);
    }
    LispMused(alist);
}

void
LispFreeArgList(LispArgList *alist)
{
    if (alist->normals.num_symbols)
	LispFree(alist->normals.symbols);
    if (alist->optionals.num_symbols) {
	LispFree(alist->optionals.symbols);
	LispFree(alist->optionals.defaults);
	LispFree(alist->optionals.sforms);
    }
    if (alist->keys.num_symbols) {
	LispFree(alist->keys.symbols);
	LispFree(alist->keys.defaults);
	LispFree(alist->keys.sforms);
	LispFree(alist->keys.keys);
    }
    if (alist->auxs.num_symbols) {
	LispFree(alist->auxs.symbols);
	LispFree(alist->auxs.initials);
    }
    LispFree(alist);
}

static LispObj *
LispCheckNeedProtect(LispObj *object)
{
    if (object) {
	switch (OBJECT_TYPE(object)) {
	    case LispNil_t:
	    case LispAtom_t:
	    case LispFunction_t:
	    case LispFixnum_t:
	    case LispSChar_t:
		return (NULL);
	    default:
		return (object);
	}
    }
    return (NULL);
}

LispObj *
LispListProtectedArguments(LispArgList *alist)
{
    int i;
    GC_ENTER();
    LispObj *arguments, *cons, *obj, *prev;

    arguments = cons = prev = NIL;
    for (i = 0; i < alist->optionals.num_symbols; i++) {
	if ((obj = LispCheckNeedProtect(alist->optionals.defaults[i])) != NULL) {
	    if (arguments == NIL) {
		arguments = cons = prev = CONS(obj, NIL);
		GC_PROTECT(arguments);
	    }
	    else {
		RPLACD(cons, CONS(obj, NIL));
		prev = cons;
		cons = CDR(cons);
	    }
	}
    }
    for (i = 0; i < alist->keys.num_symbols; i++) {
	if ((obj = LispCheckNeedProtect(alist->keys.defaults[i])) != NULL) {
	    if (arguments == NIL) {
		arguments = cons = prev = CONS(obj, NIL);
		GC_PROTECT(arguments);
	    }
	    else {
		RPLACD(cons, CONS(obj, NIL));
		prev = cons;
		cons = CDR(cons);
	    }
	}
    }
    for (i = 0; i < alist->auxs.num_symbols; i++) {
	if ((obj = LispCheckNeedProtect(alist->auxs.initials[i])) != NULL) {
	    if (arguments == NIL) {
		arguments = cons = prev = CONS(obj, NIL);
		GC_PROTECT(arguments);
	    }
	    else {
		RPLACD(cons, CONS(obj, NIL));
		prev = cons;
		cons = CDR(cons);
	    }
	}
    }
    GC_LEAVE();

    /* Don't add a NIL cell at the end, to save some space */
    if (arguments != NIL) {
	if (arguments == cons)
	    arguments = CAR(cons);
	else
	    CDR(prev) = CAR(cons);
    }

    return (arguments);
}

LispArgList *
LispCheckArguments(LispFunType type, LispObj *list, char *name, int builtin)
{
    static char *types[4] = {"LAMBDA-LIST", "FUNCTION", "MACRO", "SETF-METHOD"};
    static char *fnames[4] = {"LAMBDA", "DEFUN", "DEFMACRO", "DEFSETF"};
#define IKEY		0
#define IOPTIONAL	1
#define IREST		2
#define IAUX		3
    static char *keys[4] = {"&KEY", "&OPTIONAL", "&REST", "&AUX"};
    int rest, optional, key, aux, count;
    LispArgList *alist;
    LispObj *spec, *sform, *defval, *default_value;
    char description[8], *desc;

/* If LispRealloc fails, the previous memory will be released
 * in LispTopLevel, unless LispMused was called on the pointer */
#define REALLOC_OBJECTS(pointer, count)		\
    pointer = LispRealloc(pointer, (count) * sizeof(LispObj*))

    alist = LispCalloc(1, sizeof(LispArgList));
    if (!CONSP(list)) {
	if (list != NIL)
	    LispDestroy("%s %s: %s cannot be a %s argument list",
			fnames[type], name, STROBJ(list), types[type]);
	alist->description = GETATOMID("");

	return (alist);
    }

    default_value = builtin ? UNSPEC : NIL;

    description[0] = '\0';
    desc = description;
    rest = optional = key = aux = 0;
    for (; CONSP(list); list = CDR(list)) {
	spec = CAR(list);

	if (CONSP(spec)) {
	    if (builtin)
		LispDestroy("builtin function argument cannot have default value");
	    if (aux) {
		if (!SYMBOLP(CAR(spec)) ||
		    (CDR(spec) != NIL && CDDR(spec) != NIL))
		    LispDestroy("%s %s: bad &AUX argument %s",
				fnames[type], name, STROBJ(spec));
		defval = CDR(spec) != NIL ? CADR(spec) : NIL;
		count = alist->auxs.num_symbols;
		REALLOC_OBJECTS(alist->auxs.symbols, count + 1);
		REALLOC_OBJECTS(alist->auxs.initials, count + 1);
		alist->auxs.symbols[count] = CAR(spec);
		alist->auxs.initials[count] = defval;
		++alist->auxs.num_symbols;
		if (count == 0)
		    *desc++ = 'a';
		++alist->num_arguments;
	    }
	    else if (rest)
		LispDestroy("%s %s: syntax error parsing %s",
			    fnames[type], name, keys[IREST]);
	    else if (key) {
		LispObj *akey = CAR(spec);

		defval = default_value;
		sform = NULL;
		if (CONSP(akey)) {
		    /* check for special case, as in:
		     *	(defun a (&key ((key name) 'default-value)) name)
		     *	(a 'key 'test)	=> TEST
		     *	(a)		=> DEFAULT-VALUE
		     */
		    if (!SYMBOLP(CAR(akey)) || !CONSP(CDR(akey)) ||
			!SYMBOLP(CADR(akey)) || CDDR(akey) != NIL ||
			(CDR(spec) != NIL && CDDR(spec) != NIL))
			LispDestroy("%s %s: bad special &KEY %s",
				    fnames[type], name, STROBJ(spec));
		    if (CDR(spec) != NIL)
			defval = CADR(spec);
		    spec = CADR(akey);
		    akey = CAR(akey);
		}
		else {
		    akey = NULL;

		    if (!SYMBOLP(CAR(spec)))
			LispDestroy("%s %s: %s cannot be a %s argument name",
				    fnames[type], name,
				    STROBJ(CAR(spec)), types[type]);
		    /* check if default value provided, and optionally a `svar' */
		    else if (CDR(spec) != NIL && (!CONSP(CDR(spec)) ||
			      (CDDR(spec) != NIL &&
			       (!SYMBOLP(CAR(CDDR(spec))) ||
				CDR(CDDR(spec)) != NIL))))
			LispDestroy("%s %s: bad argument specification %s",
				    fnames[type], name, STROBJ(spec));
		    if (CONSP(CDR(spec))) {
			defval = CADR(spec);
			if (CONSP(CDDR(spec)))
			    sform = CAR(CDDR(spec));
		    }
		    /* Add to keyword package, and set the keyword in the
		     * argument list, so that a function argument keyword
		     * will reference the same object, and make comparison
		     * simpler. */
		    spec = LispCheckKeyword(CAR(spec));
		}

		count = alist->keys.num_symbols;
		REALLOC_OBJECTS(alist->keys.keys, count + 1);
		REALLOC_OBJECTS(alist->keys.defaults, count + 1);
		REALLOC_OBJECTS(alist->keys.sforms, count + 1);
		REALLOC_OBJECTS(alist->keys.symbols, count + 1);
		alist->keys.symbols[count] = spec;
		alist->keys.defaults[count] = defval;
		alist->keys.sforms[count] = sform;
		alist->keys.keys[count] = akey;
		++alist->keys.num_symbols;
		if (count == 0)
		    *desc++ = 'k';
		alist->num_arguments += 1 + (sform != NULL);
	    }
	    else if (optional) {
		defval = default_value;
		sform = NULL;

		if (!SYMBOLP(CAR(spec)))
		    LispDestroy("%s %s: %s cannot be a %s argument name",
				fnames[type], name,
				STROBJ(CAR(spec)), types[type]);
		/* check if default value provided, and optionally a `svar' */
		else if (CDR(spec) != NIL && (!CONSP(CDR(spec)) ||
			  (CDDR(spec) != NIL &&
			   (!SYMBOLP(CAR(CDDR(spec))) ||
			    CDR(CDDR(spec)) != NIL))))
		    LispDestroy("%s %s: bad argument specification %s",
				fnames[type], name, STROBJ(spec));
		if (CONSP(CDR(spec))) {
		    defval = CADR(spec);
		    if (CONSP(CDDR(spec)))
			sform = CAR(CDDR(spec));
		}
		spec = CAR(spec);

		count = alist->optionals.num_symbols;
		REALLOC_OBJECTS(alist->optionals.symbols, count + 1);
		REALLOC_OBJECTS(alist->optionals.defaults, count + 1);
		REALLOC_OBJECTS(alist->optionals.sforms, count + 1);
		alist->optionals.symbols[count] = spec;
		alist->optionals.defaults[count] = defval;
		alist->optionals.sforms[count] = sform;
		++alist->optionals.num_symbols;
		if (count == 0)
		    *desc++ = 'o';
		alist->num_arguments += 1 + (sform != NULL);
	    }

	    /* Normal arguments cannot have default value */
	    else
		LispDestroy("%s %s: syntax error parsing %s",
			    fnames[type], name, STROBJ(spec));
	}

	/* spec must be an atom, excluding keywords */
	else if (!SYMBOLP(spec) || KEYWORDP(spec))
	    LispDestroy("%s %s: %s cannot be a %s argument",
			fnames[type], name, STROBJ(spec), types[type]);
	else {
	    Atom_id atom = ATOMID(spec);

	    if (atom[0] == '&') {
		if (atom == Srest) {
		    if (rest || aux || CDR(list) == NIL || !SYMBOLP(CADR(list))
			/* only &aux allowed after &rest */
			|| (CDDR(list) != NIL && !SYMBOLP(CAR(CDDR(list))) &&
			    ATOMID(CAR(CDDR(list))) != Saux))
			LispDestroy("%s %s: syntax error parsing %s",
				    fnames[type], name, ATOMID(spec));
		    if (key)
			LispDestroy("%s %s: %s not allowed after %s",
				    fnames[type], name, keys[IREST], keys[IKEY]);
		    rest = 1;
		    continue;
		}

		else if (atom == Skey) {
		    if (rest || aux)
			LispDestroy("%s %s: %s not allowed after %s",
				    fnames[type], name, ATOMID(spec),
				    rest ? keys[IREST] : keys[IAUX]);
		    key = 1;
		    continue;
		}

		else if (atom == Soptional) {
		    if (rest || optional || aux || key)
			LispDestroy("%s %s: %s not allowed after %s",
				    fnames[type], name, ATOMID(spec),
				    rest ? keys[IREST] :
					optional ?
					keys[IOPTIONAL] :
					    aux ? keys[IAUX] : keys[IKEY]);
		    optional = 1;
		    continue;
		}

		else if (atom == Saux) {
		    /* &AUX must be the last keyword parameter */
		    if (aux)
			LispDestroy("%s %s: syntax error parsing %s",
				    fnames[type], name, ATOMID(spec));
		    else if (builtin)
			LispDestroy("builtin function cannot have &AUX arguments");
		    aux = 1;
		    continue;
		}

		/* Untill more lambda-list keywords supported, don't allow
		 * argument names starting with the '&' character */
		else
		    LispDestroy("%s %s: %s not allowed/implemented",
				fnames[type], name, ATOMID(spec));
	    }

	    /* Add argument to alist */
	    if (aux) {
		count = alist->auxs.num_symbols;
		REALLOC_OBJECTS(alist->auxs.symbols, count + 1);
		REALLOC_OBJECTS(alist->auxs.initials, count + 1);
		alist->auxs.symbols[count] = spec;
		alist->auxs.initials[count] = default_value;
		++alist->auxs.num_symbols;
		if (count == 0)
		    *desc++ = 'a';
		++alist->num_arguments;
	    }
	    else if (rest) {
		alist->rest = spec;
		*desc++ = 'r';
		++alist->num_arguments;
	    }
	    else if (key) {
		/* Add to keyword package, and set the keyword in the
		 * argument list, so that a function argument keyword
		 * will reference the same object, and make comparison
		 * simpler. */
		spec = LispCheckKeyword(spec);
		count = alist->keys.num_symbols;
		REALLOC_OBJECTS(alist->keys.keys, count + 1);
		REALLOC_OBJECTS(alist->keys.defaults, count + 1);
		REALLOC_OBJECTS(alist->keys.sforms, count + 1);
		REALLOC_OBJECTS(alist->keys.symbols, count + 1);
		alist->keys.symbols[count] = spec;
		alist->keys.defaults[count] = default_value;
		alist->keys.sforms[count] = NULL;
		alist->keys.keys[count] = NULL;
		++alist->keys.num_symbols;
		if (count == 0)
		    *desc++ = 'k';
		++alist->num_arguments;
	    }
	    else if (optional) {
		count = alist->optionals.num_symbols;
		REALLOC_OBJECTS(alist->optionals.symbols, count + 1);
		REALLOC_OBJECTS(alist->optionals.defaults, count + 1);
		REALLOC_OBJECTS(alist->optionals.sforms, count + 1);
		alist->optionals.symbols[count] = spec;
		alist->optionals.defaults[count] = default_value;
		alist->optionals.sforms[count] = NULL;
		++alist->optionals.num_symbols;
		if (count == 0)
		    *desc++ = 'o';
		++alist->num_arguments;
	    }
	    else {
		count = alist->normals.num_symbols;
		REALLOC_OBJECTS(alist->normals.symbols, count + 1);
		alist->normals.symbols[count] = spec;
		++alist->normals.num_symbols;
		if (count == 0)
		    *desc++ = '.';
		++alist->num_arguments;
	    }
	}
    }

    /* Check for dotted argument list */
    if (list != NIL)
	LispDestroy("%s %s: %s cannot end %s arguments",
		    fnames[type], name, STROBJ(list), types[type]);

    *desc = '\0';
    alist->description = LispGetAtomString(description, 0);

    return (alist);
}

void
LispAddBuiltinFunction(LispBuiltin *builtin)
{
    static LispObj stream;
    static LispString string;
    static int first = 1;
    LispObj *name, *obj, *list, *cons, *code;
    LispAtom *atom;
    LispArgList *alist;
    int length = lisp__data.protect.length;

    if (first) {
	stream.type = LispStream_t;
	stream.data.stream.source.string = &string;
	stream.data.stream.pathname = NIL;
	stream.data.stream.type = LispStreamString;
	stream.data.stream.readable = 1;
	stream.data.stream.writable = 0;
	string.output = 0;
	first = 0;
    }
    string.string = builtin->declaration;
    string.length = strlen(builtin->declaration);
    string.input = 0;

    code = COD;
    LispPushInput(&stream);
    name = LispRead();
    list = cons = CONS(name, NIL);
    if (length + 1 >= lisp__data.protect.space)
	LispMoreProtects();
    lisp__data.protect.objects[lisp__data.protect.length++] = list;
    while ((obj = LispRead()) != NULL) {
	RPLACD(cons, CONS(obj, NIL));
	cons = CDR(cons);
    }
    LispPopInput(&stream);

    atom = name->data.atom;
    alist = LispCheckArguments(builtin->type, CDR(list), atom->string, 1);
    builtin->symbol = CAR(list);
    LispSetAtomBuiltinProperty(atom, builtin, alist);
    LispUseArgList(alist);

    /* Make function a extern symbol, unless told to not do so */
    if (!builtin->internal)
	LispExportSymbol(name);

    lisp__data.protect.length = length;
    COD = code;			/* LispRead protect data in COD */
}

void
LispAllocSeg(LispObjSeg *seg, int cellcount)
{
    unsigned int i;
    LispObj **list, *obj;

    DISABLE_INTERRUPTS();
    while (seg->nfree < cellcount) {
	if ((obj = (LispObj*)calloc(1, sizeof(LispObj) * segsize)) == NULL) {
	    ENABLE_INTERRUPTS();
	    LispDestroy("out of memory");
	}
	if ((list = (LispObj**)realloc(seg->objects,
	    sizeof(LispObj*) * (seg->nsegs + 1))) == NULL) {
	    free(obj);
	    ENABLE_INTERRUPTS();
	    LispDestroy("out of memory");
	}
	seg->objects = list;
	seg->objects[seg->nsegs] = obj;

	seg->nfree += segsize;
	seg->nobjs += segsize;
	for (i = 1; i < segsize; i++, obj++) {
	    /* Objects of type cons are the most used, save some time
	     * by not setting it's type in LispNewCons. */
	    obj->type = LispCons_t;
	    CDR(obj) = obj + 1;
	}
	obj->type = LispCons_t;
	CDR(obj) = seg->freeobj;
	seg->freeobj = seg->objects[seg->nsegs];
	++seg->nsegs;
    }
#ifdef DEBUG
    LispMessage("gc: %d cell(s) allocated at %d segment(s)",
		seg->nobjs, seg->nsegs);
#endif
    ENABLE_INTERRUPTS();
}

static INLINE void
LispMark(register LispObj *object)
{
mark_again:
    switch (OBJECT_TYPE(object)) {
	case LispNil_t:
	case LispAtom_t:
	case LispFixnum_t:
	case LispSChar_t:
	case LispFunction_t:
	    return;
	case LispLambda_t:
	    if (OPAQUEP(object->data.lambda.name))
		object->data.lambda.name->mark = 1;
	    object->mark = 1;
	    LispMark(object->data.lambda.data);
	    object = object->data.lambda.code;
	    goto mark_cons;
	case LispQuote_t:
	case LispBackquote_t:
	case LispFunctionQuote_t:
	    object->mark = 1;
	    object = object->data.quote;
	    goto mark_again;
	case LispPathname_t:
	    object->mark = 1;
	    object = object->data.pathname;
	    goto mark_again;
	case LispComma_t:
	    object->mark = 1;
	    object = object->data.comma.eval;
	    goto mark_again;
	case LispComplex_t:
	    if (POINTERP(object->data.complex.real))
		object->data.complex.real->mark = 1;
	    if (POINTERP(object->data.complex.imag))
		object->data.complex.imag->mark = 1;
	    break;
	case LispCons_t:
mark_cons:
	    for (; CONSP(object) && !object->mark; object = CDR(object)) {
		object->mark = 1;
		switch (OBJECT_TYPE(CAR(object))) {
		    case LispNil_t:
		    case LispAtom_t:
		    case LispFixnum_t:
		    case LispSChar_t:
		    case LispPackage_t:		/* protected in gc */
			break;
		    case LispInteger_t:
		    case LispDFloat_t:
		    case LispString_t:
		    case LispRatio_t:
		    case LispOpaque_t:
		    case LispBignum_t:
		    case LispBigratio_t:
			CAR(object)->mark = 1;
			break;
		    default:
			LispMark(CAR(object));
			break;
		}
	    }
	    if (POINTERP(object) && !object->mark)
		goto mark_again;
	    return;
	case LispArray_t:
	    LispMark(object->data.array.list);
	    object->mark = 1;
	    object = object->data.array.dim;
	    goto mark_cons;
	case LispStruct_t:
	    object->mark = 1;
	    object = object->data.struc.fields;
	    goto mark_cons;
	case LispStream_t:
mark_stream:
	    LispMark(object->data.stream.pathname);
	    if (object->data.stream.type == LispStreamPipe) {
		object->mark = 1;
		object = object->data.stream.source.program->errorp;
		goto mark_stream;
	    }
	    break;
	case LispRegex_t:
	    object->data.regex.pattern->mark = 1;
	    break;
	case LispBytecode_t:
	    object->mark = 1;
	    object = object->data.bytecode.code;
	    goto mark_again;
	case LispHashTable_t: {
	    unsigned long i;
	    LispHashEntry *entry = object->data.hash.table->entries,
			  *last = entry + object->data.hash.table->num_entries;

	    if (object->mark)
		return;
	    object->mark = 1;
	    for (; entry < last; entry++) {
		for (i = 0; i < entry->count; i++) {
		    switch (OBJECT_TYPE(entry->keys[i])) {
			case LispNil_t:
			case LispAtom_t:
			case LispFixnum_t:
			case LispSChar_t:
			case LispFunction_t:
			case LispPackage_t:
			    break;
			case LispInteger_t:
			case LispDFloat_t:
			case LispString_t:
			case LispRatio_t:
			case LispOpaque_t:
			case LispBignum_t:
			case LispBigratio_t:
			    entry->keys[i]->mark = 1;
			    break;
			default:
			    LispMark(entry->keys[i]);
			    break;
		    }
		    switch (OBJECT_TYPE(entry->values[i])) {
			case LispNil_t:
			case LispAtom_t:
			case LispFixnum_t:
			case LispSChar_t:
			case LispFunction_t:
			case LispPackage_t:
			    break;
			case LispInteger_t:
			case LispDFloat_t:
			case LispString_t:
			case LispRatio_t:
			case LispOpaque_t:
			case LispBignum_t:
			case LispBigratio_t:
			    entry->values[i]->mark = 1;
			    break;
			default:
			    LispMark(entry->values[i]);
			    break;
		    }
		}
	    }
	}   return;
	default:
	    break;
    }
    object->mark = 1;
}

static INLINE void
LispProt(register LispObj *object)
{
prot_again:
    switch (OBJECT_TYPE(object)) {
	case LispNil_t:
	case LispAtom_t:
	case LispFixnum_t:
	case LispSChar_t:
	case LispFunction_t:
	    return;
	case LispLambda_t:
	    if (OPAQUEP(object->data.lambda.name))
		object->data.lambda.name->prot = 1;
	    object->prot = 1;
	    LispProt(object->data.lambda.data);
	    object = object->data.lambda.code;
	    goto prot_cons;
	case LispQuote_t:
	case LispBackquote_t:
	case LispFunctionQuote_t:
	    object->prot = 1;
	    object = object->data.quote;
	    goto prot_again;
	case LispPathname_t:
	    object->prot = 1;
	    object = object->data.pathname;
	    goto prot_again;
	case LispComma_t:
	    object->prot = 1;
	    object = object->data.comma.eval;
	    goto prot_again;
	case LispComplex_t:
	    if (POINTERP(object->data.complex.real))
		object->data.complex.real->prot = 1;
	    if (POINTERP(object->data.complex.imag))
		object->data.complex.imag->prot = 1;
	    break;
	case LispCons_t:
prot_cons:
	    for (; CONSP(object) && !object->prot; object = CDR(object)) {
		object->prot = 1;
		switch (OBJECT_TYPE(CAR(object))) {
		    case LispNil_t:
		    case LispAtom_t:
		    case LispFixnum_t:
		    case LispSChar_t:
		    case LispFunction_t:
		    case LispPackage_t:		/* protected in gc */
			break;
		    case LispInteger_t:
		    case LispDFloat_t:
		    case LispString_t:
		    case LispRatio_t:
		    case LispOpaque_t:
		    case LispBignum_t:
		    case LispBigratio_t:
			CAR(object)->prot = 1;
			break;
		    default:
			LispProt(CAR(object));
			break;
		}
	    }
	    if (POINTERP(object) && !object->prot)
		goto prot_again;
	    return;
	case LispArray_t:
	    LispProt(object->data.array.list);
	    object->prot = 1;
	    object = object->data.array.dim;
	    goto prot_cons;
	case LispStruct_t:
	    object->prot = 1;
	    object = object->data.struc.fields;
	    goto prot_cons;
	case LispStream_t:
prot_stream:
	    LispProt(object->data.stream.pathname);
	    if (object->data.stream.type == LispStreamPipe) {
		object->prot = 1;
		object = object->data.stream.source.program->errorp;
		goto prot_stream;
	    }
	    break;
	case LispRegex_t:
	    object->data.regex.pattern->prot = 1;
	    break;
	case LispBytecode_t:
	    object->prot = 1;
	    object = object->data.bytecode.code;
	    goto prot_again;
	case LispHashTable_t: {
	    unsigned long i;
	    LispHashEntry *entry = object->data.hash.table->entries,
			  *last = entry + object->data.hash.table->num_entries;

	    if (object->prot)
		return;
	    object->prot = 1;
	    for (; entry < last; entry++) {
		for (i = 0; i < entry->count; i++) {
		    switch (OBJECT_TYPE(entry->keys[i])) {
			case LispNil_t:
			case LispAtom_t:
			case LispFixnum_t:
			case LispSChar_t:
			case LispFunction_t:
			case LispPackage_t:
			    break;
			case LispInteger_t:
			case LispDFloat_t:
			case LispString_t:
			case LispRatio_t:
			case LispOpaque_t:
			case LispBignum_t:
			case LispBigratio_t:
			    entry->keys[i]->prot = 1;
			    break;
			default:
			    LispProt(entry->keys[i]);
			    break;
		    }
		    switch (OBJECT_TYPE(entry->values[i])) {
			case LispNil_t:
			case LispAtom_t:
			case LispFixnum_t:
			case LispSChar_t:
			case LispFunction_t:
			case LispPackage_t:
			    break;
			case LispInteger_t:
			case LispDFloat_t:
			case LispString_t:
			case LispRatio_t:
			case LispOpaque_t:
			case LispBignum_t:
			case LispBigratio_t:
			    entry->values[i]->prot = 1;
			    break;
			default:
			    LispProt(entry->values[i]);
			    break;
		    }
		}
	    }
	}   return;
	default:
	    break;
    }
    object->prot = 1;
}

void
LispProtect(LispObj *key, LispObj *list)
{
    PRO = CONS(CONS(key, list), PRO);
}

void
LispUProtect(LispObj *key, LispObj *list)
{
    LispObj *prev, *obj;

    for (prev = obj = PRO; obj != NIL; prev = obj, obj = CDR(obj))
	if (CAR(CAR(obj)) == key && CDR(CAR(obj)) == list) {
	    if (obj == PRO)
		PRO = CDR(PRO);
	    else
		CDR(prev) = CDR(obj);
	    return;
	}

    LispDestroy("no match for %s, at UPROTECT", STROBJ(key));
}

static LispObj *
Lisp__New(LispObj *car, LispObj *cdr)
{
    int cellcount;
    LispObj *obj;

    Lisp__GC(car, cdr);
#if 0
    lisp__data.gc.average = (objseg.nfree + lisp__data.gc.average) >> 1;
    if (lisp__data.gc.average < minfree) {
	if (lisp__data.gc.expandbits < 6)
	    ++lisp__data.gc.expandbits;
    }
    else if (lisp__data.gc.expandbits)
	--lisp__data.gc.expandbits;
    /* For 32 bit computers, where sizeof(LispObj) == 16,
     * minfree is set to 1024, and expandbits limited to 6,
     * the maximum extra memory requested here should be 1Mb
     */
    cellcount = minfree << lisp__data.gc.expandbits;
#else
    /* Try to keep at least 3 times more free cells than the de number
     * of used cells in the freelist, to amenize the cost of the gc time,
     * in the, currently, very simple gc strategy code. */
    cellcount = (objseg.nobjs - objseg.nfree) * 3;
    cellcount = cellcount + (minfree - (cellcount % minfree));
#endif

    if (objseg.freeobj == NIL || objseg.nfree < cellcount)
	LispAllocSeg(&objseg, cellcount);

    obj = objseg.freeobj;
    objseg.freeobj = CDR(obj);
    --objseg.nfree;
    
    return (obj);
}

LispObj *
LispNew(LispObj *car, LispObj *cdr)
{
    LispObj *obj = objseg.freeobj;

    if (obj == NIL)
	obj = Lisp__New(car, cdr);
    else {
	objseg.freeobj = CDR(obj);
	--objseg.nfree;
    }
    
    return (obj);
}

LispObj *
LispNewAtom(char *str, int intern)
{
    LispObj *object;
    LispAtom *atom = LispDoGetAtom(str, 0);

    if (atom->object) {
	if (intern && atom->package == NULL)
	    atom->package = PACKAGE;

	return (atom->object);
    }

    if (atomseg.freeobj == NIL)
	LispAllocSeg(&atomseg, pagesize);
    object = atomseg.freeobj;
    atomseg.freeobj = CDR(object);
    --atomseg.nfree;

    object->type = LispAtom_t;
    object->data.atom = atom;
    atom->object = object;
    if (intern)
	atom->package = PACKAGE;

    return (object);
}

LispObj *
LispNewStaticAtom(char *str)
{
    LispObj *object;
    LispAtom *atom = LispDoGetAtom(str, 1);

    object = LispNewSymbol(atom);

    return (object);
}

LispObj *
LispNewSymbol(LispAtom *atom)
{
    if (atom->object) {
	if (atom->package == NULL)
	    atom->package = PACKAGE;

	return (atom->object);
    }
    else {
	LispObj *symbol;

	if (atomseg.freeobj == NIL)
	    LispAllocSeg(&atomseg, pagesize);
	symbol = atomseg.freeobj;
	atomseg.freeobj = CDR(symbol);
	--atomseg.nfree;

	symbol->type = LispAtom_t;
	symbol->data.atom = atom;
	atom->object = symbol;
	atom->package = PACKAGE;

	return (symbol);
    }
}

/* function representation is created on demand and never released,
 * even if the function is undefined and never defined again */
LispObj *
LispNewFunction(LispObj *symbol)
{
    LispObj *function;

    if (symbol->data.atom->function)
	return (symbol->data.atom->function);

    if (symbol->data.atom->package == NULL)
	symbol->data.atom->package = PACKAGE;

    if (atomseg.freeobj == NIL)
	LispAllocSeg(&atomseg, pagesize);
    function = atomseg.freeobj;
    atomseg.freeobj = CDR(function);
    --atomseg.nfree;

    function->type = LispFunction_t;
    function->data.atom = symbol->data.atom;
    symbol->data.atom->function = function;

    return (function);
}

/* symbol name representation is created on demand and never released */
LispObj *
LispSymbolName(LispObj *symbol)
{
    LispObj *name;
    LispAtom *atom = symbol->data.atom;

    if (atom->name)
	return (atom->name);

    if (atomseg.freeobj == NIL)
	LispAllocSeg(&atomseg, pagesize);
    name = atomseg.freeobj;
    atomseg.freeobj = CDR(name);
    --atomseg.nfree;

    name->type = LispString_t;
    THESTR(name) = atom->string;
    STRLEN(name) = strlen(atom->string);
    name->data.string.writable = 0;
    atom->name = name;

    return (name);
}

LispObj *
LispNewFunctionQuote(LispObj *object)
{
    LispObj *quote = LispNew(object, NIL);

    quote->type = LispFunctionQuote_t;
    quote->data.quote = object;

    return (quote);
}

LispObj *
LispNewDFloat(double value)
{
    LispObj *dfloat = objseg.freeobj;

    if (dfloat == NIL)
	dfloat = Lisp__New(NIL, NIL);
    else {
	objseg.freeobj = CDR(dfloat);
	--objseg.nfree;
    }
    dfloat->type = LispDFloat_t;
    dfloat->data.dfloat = value;

    return (dfloat);
}

LispObj *
LispNewString(char *str, long length, int alloced)
{
    char *cstring;
    LispObj *string = objseg.freeobj;

    if (string == NIL)
	string = Lisp__New(NIL, NIL);
    else {
	objseg.freeobj = CDR(string);
	--objseg.nfree;
    }
    if (alloced)
	cstring = str;
    else {
	cstring = LispMalloc(length + 1);
	memcpy(cstring, str, length);
	cstring[length] = '\0';
    }
    LispMused(cstring);
    string->type = LispString_t;
    THESTR(string) = cstring;
    STRLEN(string) = length;
    string->data.string.writable = 1;

    return (string);
}

LispObj *
LispNewComplex(LispObj *realpart, LispObj *imagpart)
{
    LispObj *complexp = objseg.freeobj;

    if (complexp == NIL)
	complexp = Lisp__New(realpart, imagpart);
    else {
	objseg.freeobj = CDR(complexp);
	--objseg.nfree;
    }
    complexp->type = LispComplex_t;
    complexp->data.complex.real = realpart;
    complexp->data.complex.imag = imagpart;

    return (complexp);
}

LispObj *
LispNewInteger(long integer)
{
    if (integer > MOST_POSITIVE_FIXNUM || integer < MOST_NEGATIVE_FIXNUM) {
	LispObj *object = objseg.freeobj;

	if (object == NIL)
	    object = Lisp__New(NIL, NIL);
	else {
	    objseg.freeobj = CDR(object);
	    --objseg.nfree;
	}
	object->type = LispInteger_t;
	object->data.integer = integer;

	return (object);
    }
    return (FIXNUM(integer));
}

LispObj *
LispNewRatio(long num, long den)
{
    LispObj *ratio = objseg.freeobj;

    if (ratio == NIL)
	ratio = Lisp__New(NIL, NIL);
    else {
	objseg.freeobj = CDR(ratio);
	--objseg.nfree;
    }
    ratio->type = LispRatio_t;
    ratio->data.ratio.numerator = num;
    ratio->data.ratio.denominator = den;

    return (ratio);
}

LispObj *
LispNewVector(LispObj *objects)
{
    GC_ENTER();
    long count;
    LispObj *array, *dimension;

    for (count = 0, array = objects; CONSP(array); count++, array = CDR(array))
	;

    GC_PROTECT(objects);
    dimension = CONS(FIXNUM(count), NIL);
    array = LispNew(objects, dimension);
    array->type = LispArray_t;
    array->data.array.list = objects;
    array->data.array.dim = dimension;
    array->data.array.rank = 1;
    array->data.array.type = LispNil_t;
    array->data.array.zero = count == 0;
    GC_LEAVE();

    return (array);
}

LispObj *
LispNewQuote(LispObj *object)
{
    LispObj *quote = LispNew(object, NIL);

    quote->type = LispQuote_t;
    quote->data.quote = object;

    return (quote);
}

LispObj *
LispNewBackquote(LispObj *object)
{
    LispObj *backquote = LispNew(object, NIL);

    backquote->type = LispBackquote_t;
    backquote->data.quote = object;

    return (backquote);
}

LispObj *
LispNewComma(LispObj *object, int atlist)
{
    LispObj *comma = LispNew(object, NIL);

    comma->type = LispComma_t;
    comma->data.comma.eval = object;
    comma->data.comma.atlist = atlist;

    return (comma);
}

LispObj *
LispNewCons(LispObj *car, LispObj *cdr)
{
    LispObj *cons = objseg.freeobj;

    if (cons == NIL)
	cons = Lisp__New(car, cdr);
    else {
	objseg.freeobj = CDR(cons);
	--objseg.nfree;
    }
    CAR(cons) = car;
    CDR(cons) = cdr;

    return (cons);
}

LispObj *
LispNewLambda(LispObj *name, LispObj *code, LispObj *data, LispFunType type)
{
    LispObj *fun = LispNew(data, code);

    fun->type = LispLambda_t;
    fun->funtype = type;
    fun->data.lambda.name = name;
    fun->data.lambda.code = code;
    fun->data.lambda.data = data;

    return (fun);
}

LispObj *
LispNewStruct(LispObj *fields, LispObj *def)
{
    LispObj *struc = LispNew(fields, def);

    struc->type = LispStruct_t;
    struc->data.struc.fields = fields;
    struc->data.struc.def = def;

    return (struc);
}

LispObj *
LispNewOpaque(void *data, int type)
{
    LispObj *opaque = LispNew(NIL, NIL);

    opaque->type = LispOpaque_t;
    opaque->data.opaque.data = data;
    opaque->data.opaque.type = type;

    return (opaque);
}

/* string argument must be static, or allocated */
LispObj *
LispNewKeyword(char *string)
{
    LispObj *keyword;

    if (PACKAGE != lisp__data.keyword) {
	LispObj *savepackage;
	LispPackage *savepack;

	/* Save package environment */
	savepackage = PACKAGE;
	savepack = lisp__data.pack;

	/* Change package environment */
	PACKAGE = lisp__data.keyword;
	lisp__data.pack = lisp__data.key;

	/* Create symbol in keyword package */
	keyword = LispNewStaticAtom(string);

	/* Restore package environment */
	PACKAGE = savepackage;
	lisp__data.pack = savepack;
    }
    else
	/* Just create symbol in keyword package */
	keyword = LispNewStaticAtom(string);

    /* Export keyword symbol */
    LispExportSymbol(keyword);

    /* All keywords are constants */
    keyword->data.atom->constant = 1;

    /* XXX maybe should bound the keyword to itself, but that would
     * require allocating a LispProperty structure for every keyword */

    return (keyword);
}

LispObj *
LispNewPathname(LispObj *obj)
{
    LispObj *path = LispNew(obj, NIL);

    path->type = LispPathname_t;
    path->data.pathname = obj;

    return (path);
}

LispObj *
LispNewStringStream(char *string, int flags, long length, int alloced)
{
    LispObj *stream = LispNew(NIL, NIL);

    SSTREAMP(stream) = LispCalloc(1, sizeof(LispString));
    if (alloced)
	SSTREAMP(stream)->string = string;
    else {
	SSTREAMP(stream)->string = LispMalloc(length + 1);
	memcpy(SSTREAMP(stream)->string, string, length);
	SSTREAMP(stream)->string[length] = '\0';
    }

    stream->type = LispStream_t;

    SSTREAMP(stream)->length = length;
    LispMused(SSTREAMP(stream));
    LispMused(SSTREAMP(stream)->string);
    stream->data.stream.type = LispStreamString;
    stream->data.stream.readable = (flags & STREAM_READ) != 0;
    stream->data.stream.writable = (flags & STREAM_WRITE) != 0;
    SSTREAMP(stream)->space = length + 1;

    stream->data.stream.pathname = NIL;

    return (stream);
}

LispObj *
LispNewFileStream(LispFile *file, LispObj *path, int flags)
{
    LispObj *stream = LispNew(NIL, NIL);

    stream->type = LispStream_t;
    FSTREAMP(stream) = file;
    stream->data.stream.pathname = path;
    stream->data.stream.type = LispStreamFile;
    stream->data.stream.readable = (flags & STREAM_READ) != 0;
    stream->data.stream.writable = (flags & STREAM_WRITE) != 0;

    return (stream);
}

LispObj *
LispNewPipeStream(LispPipe *program, LispObj *path, int flags)
{
    LispObj *stream = LispNew(NIL, NIL);

    stream->type = LispStream_t;
    PSTREAMP(stream) = program;
    stream->data.stream.pathname = path;
    stream->data.stream.type = LispStreamPipe;
    stream->data.stream.readable = (flags & STREAM_READ) != 0;
    stream->data.stream.writable = (flags & STREAM_WRITE) != 0;

    return (stream);
}

LispObj *
LispNewStandardStream(LispFile *file, LispObj *description, int flags)
{
    LispObj *stream = LispNew(NIL, NIL);

    stream->type = LispStream_t;
    FSTREAMP(stream) = file;
    stream->data.stream.pathname = description;
    stream->data.stream.type = LispStreamStandard;
    stream->data.stream.readable = (flags & STREAM_READ) != 0;
    stream->data.stream.writable = (flags & STREAM_WRITE) != 0;

    return (stream);
}

LispObj *
LispNewBignum(mpi *bignum)
{
    LispObj *integer = LispNew(NIL, NIL);

    integer->type = LispBignum_t;
    integer->data.mp.integer = bignum;
    LispMused(bignum->digs);
    LispMused(bignum);

    return (integer);
}

LispObj *
LispNewBigratio(mpr *bigratio)
{
    LispObj *ratio = LispNew(NIL, NIL);

    ratio->type = LispBigratio_t;
    ratio->data.mp.ratio = bigratio;
    LispMused(mpr_num(bigratio)->digs);
    LispMused(mpr_den(bigratio)->digs);
    LispMused(bigratio);

    return (ratio);
}

/* name must be of type LispString_t */
LispObj *
LispNewPackage(LispObj *name, LispObj *nicknames)
{
    LispObj *package = LispNew(name, nicknames);
    LispPackage *pack = LispCalloc(1, sizeof(LispPackage));

    package->type = LispPackage_t;
    package->data.package.name = name;
    package->data.package.nicknames = nicknames;
    package->data.package.package = pack;

    LispMused(pack);

    return (package);
}

LispObj *
LispSymbolFunction(LispObj *symbol)
{
    LispAtom *atom = symbol->data.atom;

    if ((atom->a_builtin &&
	 atom->property->fun.builtin->type == LispFunction) ||
	(atom->a_function &&
	 atom->property->fun.function->funtype == LispFunction) ||
	(atom->a_defstruct &&
	 atom->property->structure.function != STRUCT_NAME) ||
	/* XXX currently bytecode is only generated for functions */
	atom->a_compiled)
	symbol = FUNCTION(symbol);
    else
	LispDestroy("SYMBOL-FUNCTION: %s is not a function", STROBJ(symbol));

    return (symbol);
}


static INLINE LispObj *
LispGetVarPack(LispObj *symbol)
{
    int ii;
    char *string;
    LispAtom *atom;

    string = ATOMID(symbol);
    ii = STRHASH(string);

    atom = lisp__data.pack->atoms[ii];
    while (atom) {
	if (strcmp(atom->string, string) == 0)
	    return (atom->object);

	atom = atom->next;
    }

    /* Symbol not found, just import it */
    return (NULL);
}

/* package must be of type LispPackage_t */
void
LispUsePackage(LispObj *package)
{
    unsigned i;
    LispAtom *atom;
    LispPackage *pack;
    LispObj **pentry, **eentry;

    /* Already using its own symbols... */
    if (package == PACKAGE)
	return;

    /* Check if package not already in use-package list */
    for (pentry = lisp__data.pack->use.pairs,
	 eentry = pentry + lisp__data.pack->use.length;
	 pentry < eentry; pentry++)
	if (*pentry == package)
	return;

    /* Remember this package is in the use-package list */
    if (lisp__data.pack->use.length + 1 >= lisp__data.pack->use.space) {
	LispObj **pairs = realloc(lisp__data.pack->use.pairs,
				  (lisp__data.pack->use.space + 1) *
				  sizeof(LispObj*));

	if (pairs == NULL)
	    LispDestroy("out of memory");

	lisp__data.pack->use.pairs = pairs;
	++lisp__data.pack->use.space;
    }
    lisp__data.pack->use.pairs[lisp__data.pack->use.length++] = package;

    /* Import all extern symbols from package */
    pack = package->data.package.package;

    /* Traverse atom list, searching for extern symbols */
    for (i = 0; i < STRTBLSZ; i++) {
	atom = pack->atoms[i];
	while (atom) {
	    if (atom->ext)
		LispImportSymbol(atom->object);
	    atom = atom->next;
	}
    }
}

/* symbol must be of type LispAtom_t */
void
LispImportSymbol(LispObj *symbol)
{
    int increment;
    LispAtom *atom;
    LispObj *current;

    current = LispGetVarPack(symbol);
    if (current == NULL || current->data.atom->property == NOPROPERTY) {
	/* No conflicts */

	if (symbol->data.atom->a_object) {
	    /* If it is a bounded variable */
	    if (lisp__data.pack->glb.length + 1 >= lisp__data.pack->glb.space)
		LispMoreGlobals(lisp__data.pack);
	    lisp__data.pack->glb.pairs[lisp__data.pack->glb.length++] = symbol;
	}

	/* Create copy of atom in current package */
	atom = LispDoGetAtom(ATOMID(symbol), 0);
	/*   Need to create a copy because if anything new is atached to the
	 * property, the current package is the owner, not the previous one. */

	/* And reference the same properties */
	atom->property = symbol->data.atom->property;

	increment = 1;
    }
    else if (current->data.atom->property != symbol->data.atom->property) {
	/* Symbol already exists in the current package,
	 * but does not reference the same variable */
	LispContinuable("Symbol %s already defined in package %s. Redefine?",
			ATOMID(symbol), THESTR(PACKAGE->data.package.name));

	atom = current->data.atom;

	/* Continued from error, redefine variable */
	LispDecrementAtomReference(atom);
	atom->property = symbol->data.atom->property;
	
	atom->a_object = atom->a_function = atom->a_builtin =
	    atom->a_property = atom->a_defsetf = atom->a_defstruct = 0;

	increment = 1;
    }
    else {
	/* Symbol is already available in the current package, just update */
	atom = current->data.atom;

	increment = 0;
    }

    /* If importing an important system variable */
    atom->watch = symbol->data.atom->watch;

    /* Update constant flag */
    atom->constant = symbol->data.atom->constant;

    /* Set home-package and unique-atom associated with symbol */
    atom->package = symbol->data.atom->package;
    atom->object = symbol->data.atom->object;

    if (symbol->data.atom->a_object)
	atom->a_object = 1;
    if (symbol->data.atom->a_function)
	atom->a_function = 1;
    else if (symbol->data.atom->a_builtin)
	atom->a_builtin = 1;
    else if (symbol->data.atom->a_compiled)
	atom->a_compiled = 1;
    if (symbol->data.atom->a_property)
	atom->a_property = 1;
    if (symbol->data.atom->a_defsetf)
	atom->a_defsetf = 1;
    if (symbol->data.atom->a_defstruct)
	atom->a_defstruct = 1;

    if (increment)
	/* Increase reference count, more than one package using the symbol */
	LispIncrementAtomReference(symbol->data.atom);
}

/* symbol must be of type LispAtom_t */
void
LispExportSymbol(LispObj *symbol)
{
    /* This does not automatically export symbols to another package using
     * the symbols of the current package */
    symbol->data.atom->ext = 1;
}

#ifdef __GNUC__
LispObj *
LispGetVar(LispObj *atom)
{
    return (LispDoGetVar(atom));
}

static INLINE LispObj *
LispDoGetVar(LispObj *atom)
#else
#define LispDoGetVar LispGetVar
LispObj *
LispGetVar(LispObj *atom)
#endif
{
    LispAtom *name;
    int i, base, offset;
    Atom_id id;

    name = atom->data.atom;
    if (name->constant && name->package == lisp__data.keyword)
	return (atom);

    /* XXX offset should be stored elsewhere, it is unique, like the string
     * pointer. Unless a multi-thread interface is implemented (where
     * multiple stacks would be required, the offset value should be
     * stored with the string, so that a few cpu cicles could be saved
     * by initializing the value to -1, and only searching for the symbol
     * binding if it is not -1, and if no binding is found, because the
     * lexical scope was left, reset offset to -1. */
    offset = name->offset;
    id = name->string;
    base = lisp__data.env.lex;
    i = lisp__data.env.head - 1;

    if (offset <= i && (offset >= base || name->dyn) &&
	lisp__data.env.names[offset] == id)
	return (lisp__data.env.values[offset]);

    for (; i >= base; i--)
	if (lisp__data.env.names[i] == id) {
	    name->offset = i;

	    return (lisp__data.env.values[i]);
	}

    if (name->dyn) {
	/* Keep searching as maybe a rebound dynamic variable */
	for (; i >= 0; i--)
	    if (lisp__data.env.names[i] == id) {
		name->offset = i;

	    return (lisp__data.env.values[i]);
	}

	if (name->a_object) {
	    /* Check for a symbol defined as special, but not yet bound. */
	    if (name->property->value == UNBOUND)
		return (NULL);

	    return (name->property->value);
	}
    }

    return (name->a_object ? name->property->value : NULL);
}

#ifdef DEBUGGER
/* Same code as LispDoGetVar, but returns the address of the pointer to
 * the object value. Used only by the debugger */
void *
LispGetVarAddr(LispObj *atom)
{
    LispAtom *name;
    int i, base;
    Atom_id id;

    name = atom->data.atom;
    if (name->constant && name->package == lisp__data.keyword)
	return (&atom);

    id = name->string;

    i = lisp__data.env.head - 1;
    for (base = lisp__data.env.lex; i >= base; i--)
	if (lisp__data.env.names[i] == id)
	    return (&(lisp__data.env.values[i]));

    if (name->dyn) {
	for (; i >= 0; i--)
	    if (lisp__data.env.names[i] == id)
		return (&(lisp__data.env.values[i]));

	if (name->a_object) {
	    /* Check for a symbol defined as special, but not yet bound */
	    if (name->property->value == UNBOUND)
		return (NULL);

	    return (&(name->property->value));
	}
    }

    return (name->a_object ? &(name->property->value) : NULL);
}
#endif

/* Only removes global variables. To be called by makunbound
 * Local variables are unbounded once their block is closed anyway.
 */
void
LispUnsetVar(LispObj *atom)
{
    LispAtom *name = atom->data.atom;

    if (name->package) {
	int i;
	LispPackage *pack = name->package->data.package.package;

	for (i = pack->glb.length - 1; i > 0; i--)
	    if (pack->glb.pairs[i] == atom) {
		LispRemAtomObjectProperty(name);
		--pack->glb.length;
		if (i < pack->glb.length)
		    memmove(pack->glb.pairs + i, pack->glb.pairs + i + 1,
			    sizeof(LispObj*) * (pack->glb.length - i));

		/* unset hint about dynamically binded variable */
		if (name->dyn)
		    name->dyn = 0;
		break;
	    }
    }
}

LispObj *
LispAddVar(LispObj *atom, LispObj *obj)
{
    if (lisp__data.env.length >= lisp__data.env.space)
	LispMoreEnvironment();

    LispDoAddVar(atom, obj);

    return (obj);
}

static INLINE void
LispDoAddVar(LispObj *symbol, LispObj *value)
{
    LispAtom *atom = symbol->data.atom;

    atom->offset = lisp__data.env.length;
    lisp__data.env.values[lisp__data.env.length] = value;
    lisp__data.env.names[lisp__data.env.length++] = atom->string;
}

LispObj *
LispSetVar(LispObj *atom, LispObj *obj)
{
    LispPackage *pack;
    LispAtom *name;
    int i, base, offset;
    Atom_id id;

    name = atom->data.atom;
    offset = name->offset;
    id = name->string;
    base = lisp__data.env.lex;
    i = lisp__data.env.head - 1;

    if (offset <= i && (offset >= base || name->dyn) &&
	lisp__data.env.names[offset] == id)
	return (lisp__data.env.values[offset] = obj);

    for (; i >= base; i--)
	if (lisp__data.env.names[i] == id) {
	    name->offset = i;

	    return (lisp__data.env.values[i] = obj);
	}

    if (name->dyn) {
	for (; i >= 0; i--)
	    if (lisp__data.env.names[i] == id)
		return (lisp__data.env.values[i] = obj);

	if (name->watch) {
	    LispSetAtomObjectProperty(name, obj);

	    return (obj);
	}

	return (SETVALUE(name, obj));
    }

    if (name->a_object) {
	if (name->watch) {
	    LispSetAtomObjectProperty(name, obj);

	    return (obj);
	}

	return (SETVALUE(name, obj));
    }

    LispSetAtomObjectProperty(name, obj);

    pack = name->package->data.package.package;
    if (pack->glb.length >= pack->glb.space)
	LispMoreGlobals(pack);

    pack->glb.pairs[pack->glb.length++] = atom;

    return (obj);
}

void
LispProclaimSpecial(LispObj *atom, LispObj *value, LispObj *doc)
{
    int i = 0, dyn, glb;
    LispAtom *name;
    LispPackage *pack;

    glb = 0;
    name = atom->data.atom;
    pack = name->package->data.package.package;
    dyn = name->dyn;

    if (!dyn) {
	/* Note: don't check if a local variable already is using the symbol */
	for (i = pack->glb.length - 1; i >= 0; i--)
	    if (pack->glb.pairs[i] == atom) {
		glb = 1;
		break;
	    }
    }

    if (dyn) {
	if (name->property->value == UNBOUND && value)
	    /* if variable was just made special, but not bounded */
	    LispSetAtomObjectProperty(name, value);
    }
    else if (glb)
	/* Already a global variable, but not marked as special.
	 * Set hint about dynamically binded variable. */
	name->dyn = 1;
    else {
	/* create new special variable */
	LispSetAtomObjectProperty(name, value ? value : UNBOUND);

	if (pack->glb.length >= pack->glb.space)
	    LispMoreGlobals(pack);

	pack->glb.pairs[pack->glb.length] = atom;
	++pack->glb.length;
	/* set hint about possibly dynamically binded variable */
	name->dyn = 1;
    }

    if (doc != NIL)
	LispAddDocumentation(atom, doc, LispDocVariable);
}

void
LispDefconstant(LispObj *atom, LispObj *value, LispObj *doc)
{
    int i;
    LispAtom *name = atom->data.atom;
    LispPackage *pack = name->package->data.package.package;

    /* Unset hint about dynamically binded variable, if set. */
    name->dyn = 0;

    /* Check if variable is bounded as a global variable */
    for (i = pack->glb.length - 1; i >= 0; i--)
	if (pack->glb.pairs[i] == atom)
	    break;

    if (i < 0) {
	/* Not a global variable */
	if (pack->glb.length >= pack->glb.space)
	    LispMoreGlobals(pack);

	pack->glb.pairs[pack->glb.length] = atom;
	++pack->glb.length;
    }

    /* If already a constant variable */
    if (name->constant && name->a_object && name->property->value != value)
	LispWarning("constant %s is being redefined", STROBJ(atom));
    else
	name->constant = 1;

    /* Set constant value */
    LispSetAtomObjectProperty(name, value);

    if (doc != NIL)
	LispAddDocumentation(atom, doc, LispDocVariable);
}

void
LispAddDocumentation(LispObj *symbol, LispObj *documentation, LispDocType_t type)
{
    int length;
    char *string;
    LispAtom *atom;
    LispObj *object;

    if (!SYMBOLP(symbol) || !STRINGP(documentation))
	LispDestroy("DOCUMENTATION: invalid argument");

    atom = symbol->data.atom;
    if (atom->documentation[type])
	LispRemDocumentation(symbol, type);

    /* allocate documentation in atomseg */
    if (atomseg.freeobj == NIL)
	LispAllocSeg(&atomseg, pagesize);
    length = STRLEN(documentation);
    string = LispMalloc(length);
    memcpy(string, THESTR(documentation), length);
    string[length] = '\0';
    object = atomseg.freeobj;
    atomseg.freeobj = CDR(object);
    --atomseg.nfree;

    object->type = LispString_t;
    THESTR(object) = string;
    STRLEN(object) = length;
    object->data.string.writable = 0;
    atom->documentation[type] = object;
    LispMused(string);
}

void
LispRemDocumentation(LispObj *symbol, LispDocType_t type)
{
    LispAtom *atom;

    if (!SYMBOLP(symbol))
	LispDestroy("DOCUMENTATION: invalid argument");

    atom = symbol->data.atom;
    if (atom->documentation[type]) {
	/* reclaim object to atomseg */
	free(THESTR(atom->documentation[type]));
	CDR(atom->documentation[type]) = atomseg.freeobj;
	atomseg.freeobj = atom->documentation[type];
	atom->documentation[type] = NULL;
	++atomseg.nfree;
    }
}

LispObj *
LispGetDocumentation(LispObj *symbol, LispDocType_t type)
{
    LispAtom *atom;

    if (!SYMBOLP(symbol))
	LispDestroy("DOCUMENTATION: invalid argument");

    atom = symbol->data.atom;

    return (atom->documentation[type] ? atom->documentation[type] : NIL);
}

LispObj *
LispReverse(LispObj *list)
{
    LispObj *tmp, *res = NIL;

    while (list != NIL) {
	tmp = CDR(list);
	CDR(list) = res;
	res = list;
	list = tmp;
    }

    return (res);
}

LispBlock *
LispBeginBlock(LispObj *tag, LispBlockType type)
{
    LispBlock *block;
    unsigned blevel = lisp__data.block.block_level + 1;

    if (blevel > lisp__data.block.block_size) {
	LispBlock **blk;

	if (blevel > MAX_STACK_DEPTH)
	    LispDestroy("stack overflow");

	DISABLE_INTERRUPTS();
	blk = realloc(lisp__data.block.block, sizeof(LispBlock*) * (blevel + 1));

	block = NULL;
	if (blk == NULL || (block = malloc(sizeof(LispBlock))) == NULL) {
	    ENABLE_INTERRUPTS();
	    LispDestroy("out of memory");
	}
	lisp__data.block.block = blk;
	lisp__data.block.block[lisp__data.block.block_size] = block;
	lisp__data.block.block_size = blevel;
	ENABLE_INTERRUPTS();
    }
    block = lisp__data.block.block[lisp__data.block.block_level];
    if (type == LispBlockCatch && !CONSTANTP(tag)) {
	tag = EVAL(tag);
	lisp__data.protect.objects[lisp__data.protect.length++] = tag;
    }
    block->type = type;
    block->tag = tag;
    block->stack = lisp__data.stack.length;
    block->protect = lisp__data.protect.length;
    block->block_level = lisp__data.block.block_level;

    lisp__data.block.block_level = blevel;

#ifdef DEBUGGER
    if (lisp__data.debugging) {
	block->debug_level = lisp__data.debug_level;
	block->debug_step = lisp__data.debug_step;
    }
#endif

    return (block);
}

void
LispEndBlock(LispBlock *block)
{
    lisp__data.protect.length = block->protect;
    lisp__data.block.block_level = block->block_level;

#ifdef DEBUGGER
    if (lisp__data.debugging) {
	if (lisp__data.debug_level >= block->debug_level) {
	    while (lisp__data.debug_level > block->debug_level) {
		DBG = CDR(DBG);
		--lisp__data.debug_level;
	    }
	}
	lisp__data.debug_step = block->debug_step;
    }
#endif
}

void
LispBlockUnwind(LispBlock *block)
{
    LispBlock *unwind;
    int blevel = lisp__data.block.block_level;

    while (blevel > 0) {
	unwind = lisp__data.block.block[--blevel];
	if (unwind->type == LispBlockProtect) {
	    BLOCKJUMP(unwind);
	}
	if (unwind == block)
	    /* jump above unwind block */
	    break;
    }
}

static LispObj *
LispEvalBackquoteObject(LispObj *argument, int list, int quote)
{
    LispObj *result = argument, *object;

    if (!POINTERP(argument))
	return (argument);

    else if (XCOMMAP(argument)) {
	/* argument may need to be evaluated */

	int atlist;

	if (!list && argument->data.comma.atlist)
	    /* cannot append, not in a list */
	    LispDestroy("EVAL: ,@ only allowed on lists");

	--quote;
	if (quote < 0)
	    LispDestroy("EVAL: comma outside of backquote");

	result = object = argument->data.comma.eval;
	atlist = COMMAP(object) && object->data.comma.atlist;

	if (POINTERP(result) && (XCOMMAP(result) || XBACKQUOTEP(result)))
	    /* nested commas, reduce 1 level, or backquote,
	     * don't call LispEval or quote argument will be reset */
	    result = LispEvalBackquoteObject(object, 0, quote);

	else if (quote == 0)
	   /* just evaluate it */
	    result = EVAL(result);

	if (quote != 0)
	    result = result == object ? argument : COMMA(result, atlist);
    }

    else if (XBACKQUOTEP(argument)) {
	object = argument->data.quote;

	result = LispEvalBackquote(object, quote + 1);
	if (quote)
	    result = result == object ? argument : BACKQUOTE(result);
    }

    else if (XQUOTEP(argument) && POINTERP(argument->data.quote) &&
	     (XCOMMAP(argument->data.quote) ||
	      XBACKQUOTEP(argument->data.quote) ||
	      XCONSP(argument->data.quote))) {
	/* ensures `',sym to be the same as `(quote ,sym) */
	object = argument->data.quote;

	result = LispEvalBackquote(argument->data.quote, quote);
	result = result == object ? argument : QUOTE(result);
    }

    return (result);
}

LispObj *
LispEvalBackquote(LispObj *argument, int quote)
{
    int protect;
    LispObj *result, *object, *cons, *cdr;

    if (!CONSP(argument))
	return (LispEvalBackquoteObject(argument, 0, quote));

    result = cdr = NIL;
    protect = lisp__data.protect.length;

    /* always generate a new list for the result, even if nothing
     * is evaluated. It is not expected to use backqoutes when
     * not required. */

    /* reserve a GC protected slot for the result */
    if (protect + 1 >= lisp__data.protect.space)
	LispMoreProtects();
    lisp__data.protect.objects[lisp__data.protect.length++] = NIL;

    for (cons = argument; ; cons = CDR(cons)) {
	/* if false, last argument, and if cons is not NIL, a dotted list */
	int list = CONSP(cons), insert;

	if (list)
	    object = CAR(cons);
	else
	    object = cons;

	if (COMMAP(object))
	    /* need to insert list elements in result, not just cons it? */
	    insert = object->data.comma.atlist;
	else
	    insert = 0;

	/* evaluate object, if required */
	if (CONSP(object))
	    object = LispEvalBackquote(object, quote);
	else
	    object = LispEvalBackquoteObject(object, insert, quote);

	if (result == NIL) {
	    /* if starting result list */
	    if (!insert) {
		if (list)
		    result = cdr = CONS(object, NIL);
		else
		    result = cdr = object;
		/* gc protect result */
		lisp__data.protect.objects[protect] = result;
	    }
	    else {
		if (!CONSP(object)) {
		    result = cdr = object;
		    /* gc protect result */
		    lisp__data.protect.objects[protect] = result;
		}
		else {
		    result = cdr = CONS(CAR(object), NIL);
		    /* gc protect result */
		    lisp__data.protect.objects[protect] = result;

		    /* add remaining elements to result */
		    for (object = CDR(object);
			 CONSP(object);
			 object = CDR(object)) {
			RPLACD(cdr, CONS(CAR(object), NIL));
			cdr = CDR(cdr);
		    }
		    if (object != NIL) {
			/* object was a dotted list */
			RPLACD(cdr, object);
			cdr = CDR(cdr);
		    }
		}
	    }
	}
	else {
	    if (!CONSP(cdr))
		LispDestroy("EVAL: cannot append to %s", STROBJ(cdr));

	    if (!insert) {
		if (list) {
		    RPLACD(cdr, CONS(object, NIL));
		    cdr = CDR(cdr);
		}
		else {
		    RPLACD(cdr, object);
		    cdr = object;
		}
	    }
	    else {
		if (!CONSP(object)) {
		    RPLACD(cdr, object);
		    /* if object is NIL, it is a empty list appended, not
		     * creating a dotted list. */
		    if (object != NIL)
			cdr = object;
		}
		else {
		    for (; CONSP(object); object = CDR(object)) {
			RPLACD(cdr, CONS(CAR(object), NIL));
			cdr = CDR(cdr);
		    }
		    if (object != NIL) {
			/* object was a dotted list */
			RPLACD(cdr, object);
			cdr = CDR(cdr);
		    }
		}
	    }
	}

	/* if last argument list element processed */
	if (!list)
	    break;
    }

    lisp__data.protect.length = protect;

    return (result);
}

void
LispMoreEnvironment(void)
{
    Atom_id *names;
    LispObj **values;

    DISABLE_INTERRUPTS();
    names = realloc(lisp__data.env.names,
		    (lisp__data.env.space + 256) * sizeof(Atom_id));
    if (names != NULL) {
	values = realloc(lisp__data.env.values,
			 (lisp__data.env.space + 256) * sizeof(LispObj*));
	if (values != NULL) {
	    lisp__data.env.names = names;
	    lisp__data.env.values = values;
	    lisp__data.env.space += 256;
	    ENABLE_INTERRUPTS();
	    return;
	}
	else
	    free(names);
    }
    ENABLE_INTERRUPTS();
    LispDestroy("out of memory");
}

void
LispMoreStack(void)
{
    LispObj **values;

    DISABLE_INTERRUPTS();
    values = realloc(lisp__data.stack.values,
		     (lisp__data.stack.space + 256) * sizeof(LispObj*));
    if (values == NULL) {
	ENABLE_INTERRUPTS();
	LispDestroy("out of memory");
    }
    lisp__data.stack.values = values;
    lisp__data.stack.space += 256;
    ENABLE_INTERRUPTS();
}

void
LispMoreGlobals(LispPackage *pack)
{
    LispObj **pairs;

    DISABLE_INTERRUPTS();
    pairs = realloc(pack->glb.pairs,
		    (pack->glb.space + 256) * sizeof(LispObj*));
    if (pairs == NULL) {
	ENABLE_INTERRUPTS();
	LispDestroy("out of memory");
    }
    pack->glb.pairs = pairs;
    pack->glb.space += 256;
    ENABLE_INTERRUPTS();
}

void
LispMoreProtects(void)
{
    LispObj **objects;

    DISABLE_INTERRUPTS();
    objects = realloc(lisp__data.protect.objects,
		      (lisp__data.protect.space + 256) * sizeof(LispObj*));
    if (objects == NULL) {
	ENABLE_INTERRUPTS();
	LispDestroy("out of memory");
    }
    lisp__data.protect.objects = objects;
    lisp__data.protect.space += 256;
    ENABLE_INTERRUPTS();
}

static int
LispMakeEnvironment(LispArgList *alist, LispObj *values,
		    LispObj *name, int eval, int builtin)
{
    char *desc;
    int i, count, base;
    LispObj **symbols, **defaults, **sforms;

#define BUILTIN_ARGUMENT(value)				\
    lisp__data.stack.values[lisp__data.stack.length++] = value

/* If the index value is from register variables, this
 * can save some cpu time. Useful for normal arguments
 * that are the most common, and thus the ones that
 * consume more time in LispMakeEnvironment. */
#define BUILTIN_NO_EVAL_ARGUMENT(index, value)		\
    lisp__data.stack.values[index] = value

#define NORMAL_ARGUMENT(symbol, value)			\
    LispDoAddVar(symbol, value)

    if (builtin) {
	base = lisp__data.stack.length;
	if (base + alist->num_arguments > lisp__data.stack.space) {
	    do
		LispMoreStack();
	    while (base + alist->num_arguments > lisp__data.stack.space);
	}
    }
    else {
	base = lisp__data.env.length;
	if (base + alist->num_arguments > lisp__data.env.space) {
	    do
		LispMoreEnvironment();
	    while (base + alist->num_arguments > lisp__data.env.space);
	}
    }

    desc = alist->description;
    switch (*desc++) {
	case '.':
	    goto normal_label;
	case 'o':
	    goto optional_label;
	case 'k':
	    goto key_label;
	case 'r':
	    goto rest_label;
	case 'a':
	    goto aux_label;
	default:
	    goto done_label;
    }


    /* Code below is done in several almost identical loops, to avoid
     * checking the value of the arguments eval and builtin too much times */


    /* Normal arguments */
normal_label:
    i = 0;
    count = alist->normals.num_symbols;
    if (builtin) {
	if (eval) {
	    for (; i < count && CONSP(values); i++, values = CDR(values)) {
		BUILTIN_ARGUMENT(EVAL(CAR(values)));
	    }
	}
	else {
	    for (; i < count && CONSP(values); i++, values = CDR(values)) {
		BUILTIN_NO_EVAL_ARGUMENT(base + i, CAR(values));
	    }
	    /* macro BUILTIN_NO_EVAL_ARGUMENT does not update
	     * lisp__data.stack.length, as there is no risk of GC while
	     * adding the arguments. */
	    lisp__data.stack.length += i;
	}
    }
    else {
	symbols = alist->normals.symbols;
	if (eval) {
	    for (; i < count && CONSP(values); i++, values = CDR(values)) {
		NORMAL_ARGUMENT(symbols[i], EVAL(CAR(values)));
	    }
	}
	else {
	    for (; i < count && CONSP(values); i++, values = CDR(values)) {
		NORMAL_ARGUMENT(symbols[i], CAR(values));
	    }
	}
    }
    if (i < count)
	LispDestroy("%s: too few arguments", STROBJ(name));

    switch (*desc++) {
	case 'o':
	    goto optional_label;
	case 'k':
	    goto key_label;
	case 'r':
	    goto rest_label;
	case 'a':
	    goto aux_label;
	default:
	    goto done_label;
    }

    /* &OPTIONAL */
optional_label:
    i = 0;
    count = alist->optionals.num_symbols;
    defaults = alist->optionals.defaults;
    sforms = alist->optionals.sforms;
    if (builtin) {
	if (eval) {
	    for (; i < count && CONSP(values); i++, values = CDR(values))
		BUILTIN_ARGUMENT(EVAL(CAR(values)));
	    for (; i < count; i++)
		BUILTIN_ARGUMENT(UNSPEC);
	}
	else {
	    for (; i < count && CONSP(values); i++, values = CDR(values))
		BUILTIN_ARGUMENT(CAR(values));
	    for (; i < count; i++)
		BUILTIN_ARGUMENT(UNSPEC);
	}
    }
    else {
	symbols = alist->optionals.symbols;
	if (eval) {
	    for (; i < count && CONSP(values); i++, values = CDR(values)) {
		NORMAL_ARGUMENT(symbols[i], EVAL(CAR(values)));
		if (sforms[i]) {
		    NORMAL_ARGUMENT(sforms[i], T);
		}
	    }
	}
	else {
	    for (; i < count && CONSP(values); i++, values = CDR(values)) {
		NORMAL_ARGUMENT(symbols[i], CAR(values));
		if (sforms[i]) {
		    NORMAL_ARGUMENT(sforms[i], T);
		}
	    }
	}

	/* default arguments are evaluated for macros */
	for (; i < count; i++) {
	    if (!CONSTANTP(defaults[i])) {
		int head = lisp__data.env.head;
		int lex = lisp__data.env.lex;

		lisp__data.env.lex = base;
		lisp__data.env.head = lisp__data.env.length;
		NORMAL_ARGUMENT(symbols[i], EVAL(defaults[i]));
		lisp__data.env.head = head;
		lisp__data.env.lex = lex;
	    }
	    else {
		NORMAL_ARGUMENT(symbols[i], defaults[i]);
	    }
	    if (sforms[i]) {
		NORMAL_ARGUMENT(sforms[i], NIL);
	    }
	}
    }
    switch (*desc++) {
	case 'k':
	    goto key_label;
	case 'r':
	    goto rest_label;
	case 'a':
	    goto aux_label;
	default:
	    goto done_label;
    }

    /* &KEY */
key_label:
    {
	int argc, nused;
	LispObj *val, *karg, **keys;

	/* Count number of remaining arguments */
	for (karg = values, argc = 0; CONSP(karg); karg = CDR(karg), argc++) {
	    karg = CDR(karg);
	    if (!CONSP(karg))
		LispDestroy("%s: &KEY needs arguments as pairs",
			    STROBJ(name));
	}


	/* OPTIMIZATION:
	 * Builtin functions require that the keyword be in the keyword package.
	 * User functions don't need the arguments being pushed in the stack
	 * in the declared order (bytecode expects it...).
	 * XXX Error checking should be done elsewhere, code may be looping
	 * and doing error check here may consume too much cpu time.
	 * XXX Would also be good to already have the arguments specified in
	 * the correct order.
	 */


	nused = 0;
	val = NIL;
	count = alist->keys.num_symbols;
	symbols = alist->keys.symbols;
	defaults = alist->keys.defaults;
	sforms = alist->keys.sforms;
	if (builtin) {

	    /* Arguments must be created in the declared order */
	    i = 0;
	    if (eval) {
		for (; i < count; i++) {
		    for (karg = values; CONSP(karg); karg = CDDR(karg)) {
			/* This is only true if both point to the
			 * same symbol in the keyword package. */
			if (symbols[i] == CAR(karg)) {
			    if (karg == values)
				values = CDDR(values);
			    ++nused;
			    BUILTIN_ARGUMENT(EVAL(CADR(karg)));
			    goto keyword_builtin_eval_used_label;
			}
		    }
		    BUILTIN_ARGUMENT(UNSPEC);
keyword_builtin_eval_used_label:;
		}
	    }
	    else {
		for (; i < count; i++) {
		    for (karg = values; CONSP(karg); karg = CDDR(karg)) {
			if (symbols[i] == CAR(karg)) {
			    if (karg == values)
				values = CDDR(values);
			    ++nused;
			    BUILTIN_ARGUMENT(CADR(karg));
			    goto keyword_builtin_used_label;
			}
		    }
		    BUILTIN_ARGUMENT(UNSPEC);
keyword_builtin_used_label:;
		}
	    }

	    if (argc != nused) {
		/* Argument(s) may be incorrectly specified, or specified
		 * twice (what is not an error). */
		for (karg = values; CONSP(karg); karg = CDDR(karg)) {
		    val = CAR(karg);
		    if (KEYWORDP(val)) {
			for (i = 0; i < count; i++)
			    if (symbols[i] == val)
				break;
		    }
		    else
			/* Just make the error test true */
			i = count;

		    if (i == count)
			goto invalid_keyword_label;
		}
	    }
	}

#if 0
	else {
	    /* The base offset of the atom in the stack, to check for
	     * keywords specified twice. */
	    LispObj *symbol;
	    int offset = lisp__data.env.length;

	    keys = alist->keys.keys;
	    for (karg = values; CONSP(karg); karg = CDDR(karg)) {
		symbol = CAR(karg);
		if (SYMBOLP(symbol)) {
		    /* Must be a keyword, but even if it is a keyword, may
		     * be a typo, so assume it is correct. If it is not
		     * in the argument list, it is an error. */
		    for (i = 0; i < count; i++) {
			if (!keys[i] && symbols[i] == symbol) {
			    LispAtom *atom = symbol->data.atom;

			    /* Symbol found in the argument list. */
			    if (atom->offset >= offset &&
				atom->offset < offset + nused &&
				lisp__data.env.names[atom->offset] ==
				atom->string)
				/* Specified more than once... */
				goto keyword_duplicated_label;
			    break;
			}
		    }
		}
		else {
		    Atom_id id;

		    if (!QUOTEP(symbol) || !SYMBOLP(val = symbol->data.quote)) {
			/* Bad argument. */
			val = symbol;
			goto invalid_keyword_label;
		    }

		    id = ATOMID(val);
		    for (i = 0; i < count; i++) {
			if (keys[i] && ATOMID(keys[i]) == id) {
			    LispAtom *atom = val->data.atom;

			    /* Symbol found in the argument list. */
			    if (atom->offset >= offset &&
				atom->offset < offset + nused &&
				lisp__data.env.names[atom->offset] ==
				atom->string)
				/* Specified more than once... */
				goto keyword_duplicated_label;
			    break;
			}
		    }
		}
		if (i == count) {
		    /* Argument specification not found. */
		    val = symbol;
		    goto invalid_keyword_label;
		}
		++nused;
		if (eval) {
		    NORMAL_ARGUMENT(symbols[i], EVAL(CADR(karg)));
		}
		else {
		    NORMAL_ARGUMENT(symbols[i], CADR(karg));
		}
		if (sforms[i]) {
		    NORMAL_ARGUMENT(sforms[i], T);
		}
keyword_duplicated_label:;
	    }

	    /* Add variables that were not specified in the function call. */
	    if (nused < count) {
		int j;

		for (i = 0; i < count; i++) {
		    Atom_id id = ATOMID(symbols[i]);

		    for (j = offset + nused - 1; j >= offset; j--) {
			if (lisp__data.env.names[j] == id)
			    break;
		    }

		    if (j < offset) {
			/* Argument not specified. Use default value */

			/* default arguments are evaluated for macros */
			if (!CONSTANTP(defaults[i])) {
			    int head = lisp__data.env.head;
			    int lex = lisp__data.env.lex;

			    lisp__data.env.lex = base;
			    lisp__data.env.head = lisp__data.env.length;
			    NORMAL_ARGUMENT(symbols[i], EVAL(defaults[i]));
			    lisp__data.env.head = head;
			    lisp__data.env.lex = lex;
			}
			else {
			    NORMAL_ARGUMENT(symbols[i], defaults[i]);
			}
			if (sforms[i]) {
			    NORMAL_ARGUMENT(sforms[i], NIL);
			}
		    }
		}
	    }
	}
#else
	else {
	    int varset;

	    sforms = alist->keys.sforms;
	    keys = alist->keys.keys;

	    /* Add variables */
	    for (i = 0; i < alist->keys.num_symbols; i++) {
		val = defaults[i];
		varset = 0;
		if (keys[i]) {
		    Atom_id atom = ATOMID(keys[i]);

		    /* Special keyword specification, need to compare ATOMID
		     * and keyword specification must be a quoted object */
		    for (karg = values; CONSP(karg); karg = CDR(karg)) {
			val = CAR(karg);
		 	if (QUOTEP(val) && atom == ATOMID(val->data.quote)) {
			    val = CADR(karg);
			    varset = 1;
			    ++nused;
			    break;
			}
			karg = CDR(karg);
		    }
		}

		else {
		    /* Normal keyword specification, can compare object pointers,
		     * as they point to the same object in the keyword package */
		    for (karg = values; CONSP(karg); karg = CDR(karg)) {
			/* Don't check if argument is a valid keyword or
			 * special quoted keyword */
			if (symbols[i] == CAR(karg)) {
			    val = CADR(karg);
			    varset = 1;
			    ++nused;
			    break;
			}
			karg = CDR(karg);
		    }
		}

		/* Add the variable to environment */
		if (varset) {
		    NORMAL_ARGUMENT(symbols[i], eval ? EVAL(val) : val);
		    if (sforms[i]) {
			NORMAL_ARGUMENT(sforms[i], T);
		    }
		}
		else {
		    /* default arguments are evaluated for macros */
		    if (!CONSTANTP(val)) {
			int head = lisp__data.env.head;
			int lex = lisp__data.env.lex;

			lisp__data.env.lex = base;
			lisp__data.env.head = lisp__data.env.length;
			NORMAL_ARGUMENT(symbols[i], EVAL(val));
			lisp__data.env.head = head;
			lisp__data.env.lex = lex;
		    }
		    else {
			NORMAL_ARGUMENT(symbols[i], val);
		    }
		    if (sforms[i]) {
			NORMAL_ARGUMENT(sforms[i], NIL);
		    }
		}
	    }

	    if (argc != nused) {
		/* Argument(s) may be incorrectly specified, or specified
		 * twice (what is not an error). */
		for (karg = values; CONSP(karg); karg = CDDR(karg)) {
		    val = CAR(karg);
		    if (KEYWORDP(val)) {
			for (i = 0; i < count; i++)
			    if (symbols[i] == val)
				break;
		    }
		    else if (QUOTEP(val) && SYMBOLP(val->data.quote)) {
			Atom_id atom = ATOMID(val->data.quote);

			for (i = 0; i < count; i++)
			    if (ATOMID(keys[i]) == atom)
				break;
		    }
		    else
			/* Just make the error test true */
			i = count;

		    if (i == count)
			goto invalid_keyword_label;
		}
	    }
	}
#endif
	goto check_aux_label;

invalid_keyword_label:
	{
	    /* If not in argument specification list... */
	    char function_name[36];

	    strcpy(function_name, STROBJ(name));
	    LispDestroy("%s: %s is an invalid keyword",
			function_name, STROBJ(val));
	}
    }

check_aux_label:
    if (*desc == 'a') {
	/* &KEY uses all remaining arguments */
	values = NIL;
	goto aux_label;
    }
    goto finished_label;

    /* &REST */
rest_label:
    if (!CONSP(values)) {
	if (builtin) {
	    BUILTIN_ARGUMENT(values);
	}
	else {
	    NORMAL_ARGUMENT(alist->rest, values);
	}
	values = NIL;
    }
    /* always allocate a new list, don't know if it will be retained */
    else if (eval) {
	LispObj *cons;

	cons = CONS(EVAL(CAR(values)), NIL);
	if (builtin) {
	    BUILTIN_ARGUMENT(cons);
	}
	else {
	    NORMAL_ARGUMENT(alist->rest, cons);
	}
	values = CDR(values);
	for (; CONSP(values); values = CDR(values)) {
	    RPLACD(cons, CONS(EVAL(CAR(values)), NIL));
	    cons = CDR(cons);
	}
    }
    else {
	LispObj *cons;

	cons = CONS(CAR(values), NIL);
	if (builtin) {
	    BUILTIN_ARGUMENT(cons);
	}
	else {
	    NORMAL_ARGUMENT(alist->rest, cons);
	}
	values = CDR(values);
	for (; CONSP(values); values = CDR(values)) {
	    RPLACD(cons, CONS(CAR(values), NIL));
	    cons = CDR(cons);
	}
    }
    if (*desc != 'a')
	goto finished_label;

    /* &AUX */
aux_label:
    i = 0;
    count = alist->auxs.num_symbols;
    defaults = alist->auxs.initials;
    symbols = alist->auxs.symbols;
    {
	int lex = lisp__data.env.lex;

	lisp__data.env.lex = base;
	lisp__data.env.head = lisp__data.env.length;
	for (; i < count; i++) {
	    NORMAL_ARGUMENT(symbols[i], EVAL(defaults[i]));
	    ++lisp__data.env.head;
	}
	lisp__data.env.lex = lex;
    }

done_label:
    if (CONSP(values))
	LispDestroy("%s: too many arguments", STROBJ(name));

finished_label:
    if (builtin)
	lisp__data.stack.base = base;
    else {
	lisp__data.env.head = lisp__data.env.length;
    }
#undef BULTIN_ARGUMENT
#undef NORMAL_ARGUMENT
#undef BUILTIN_NO_EVAL_ARGUMENT

    return (base);
}

LispObj *
LispFuncall(LispObj *function, LispObj *arguments, int eval)
{
    LispAtom *atom;
    LispArgList *alist;
    LispBuiltin *builtin;
    LispObj *lambda, *result;
    int macro, base;

#ifdef DEBUGGER
    if (lisp__data.debugging)
	LispDebugger(LispDebugCallBegin, function, arguments);
#endif

    switch (OBJECT_TYPE(function)) {
	case LispFunction_t:
	    function = function->data.atom->object;
	case LispAtom_t:
	    atom = function->data.atom;
	    if (atom->a_builtin) {
		builtin = atom->property->fun.builtin;

		if (eval)
		    eval = builtin->type != LispMacro;
		base = LispMakeEnvironment(atom->property->alist,
					   arguments, function, eval, 1);
		if (builtin->multiple_values) {
		    RETURN_COUNT = 0;
		    result = builtin->function(builtin);
		}
		else {
		    result = builtin->function(builtin);
		    RETURN_COUNT = 0;
		}
		lisp__data.stack.base = lisp__data.stack.length = base;
	    }
	    else if (atom->a_compiled) {
		int lex = lisp__data.env.lex;
		lambda = atom->property->fun.function;
		alist = atom->property->alist;

		base = LispMakeEnvironment(alist, arguments, function, eval, 0);
		lisp__data.env.lex = base;
		result = LispExecuteBytecode(lambda);
		lisp__data.env.lex = lex;
		lisp__data.env.head = lisp__data.env.length = base;
	    }
	    else if (atom->a_function) {
		lambda = atom->property->fun.function;
		macro = lambda->funtype == LispMacro;
		alist = atom->property->alist;

		lambda = lambda->data.lambda.code;
		if (eval)
		    eval = !macro;
		base = LispMakeEnvironment(alist, arguments, function, eval, 0);
		result = LispRunFunMac(function, lambda, macro, base);
	    }
	    else if (atom->a_defstruct &&
		     atom->property->structure.function != STRUCT_NAME) {
		LispObj cons;

		if (atom->property->structure.function == STRUCT_CONSTRUCTOR)
		    atom = Omake_struct->data.atom;
		else if (atom->property->structure.function == STRUCT_CHECK)
		    atom = Ostruct_type->data.atom;
		else
		    atom = Ostruct_access->data.atom;
		builtin = atom->property->fun.builtin;

		cons.type = LispCons_t;
		cons.data.cons.cdr = arguments;
		if (eval) {
		    LispObj quote;

		    quote.type = LispQuote_t;
		    quote.data.quote = function;
		    cons.data.cons.car = &quote;
		    base = LispMakeEnvironment(atom->property->alist,
					       &cons, function, 1, 1);
		}
		else {
		    cons.data.cons.car = function;
		    base = LispMakeEnvironment(atom->property->alist,
					       &cons, function, 0, 1);
		}
		result = builtin->function(builtin);
		RETURN_COUNT = 0;
		lisp__data.stack.length = base;
	    }
	    else {
		LispDestroy("EVAL: the function %s is not defined",
			    STROBJ(function));
		/*NOTREACHED*/
		result = NIL;
	    }
	    break;
	case LispLambda_t:
	    lambda = function->data.lambda.code;
	    alist = (LispArgList*)function->data.lambda.name->data.opaque.data;
	    base = LispMakeEnvironment(alist, arguments, function, eval, 0);
	    result = LispRunFunMac(function, lambda, 0, base);
	    break;
	case LispCons_t:
	    if (CAR(function) == Olambda) {
		function = EVAL(function);
		if (LAMBDAP(function)) {
		    GC_ENTER();

		    GC_PROTECT(function);
		    lambda = function->data.lambda.code;
		    alist = (LispArgList*)function->data.lambda.name->data.opaque.data;
		    base = LispMakeEnvironment(alist, arguments, NIL, eval, 0);
		    result = LispRunFunMac(NIL, lambda, 0, base);
		    GC_LEAVE();
		    break;
		}
	    }
	default:
	    LispDestroy("EVAL: %s is invalid as a function",
			STROBJ(function));
	    /*NOTREACHED*/
	    result = NIL;
	    break;
    }

#ifdef DEBUGGER
    if (lisp__data.debugging)
	LispDebugger(LispDebugCallEnd, function, result);
#endif

    return (result);
}

LispObj *
LispEval(LispObj *object)
{
    LispObj *result;

    switch (OBJECT_TYPE(object)) {
	case LispAtom_t:
	    if ((result = LispDoGetVar(object)) == NULL)
		LispDestroy("EVAL: the variable %s is unbound", STROBJ(object));
	    break;
	case LispCons_t:
	    result = LispFuncall(CAR(object), CDR(object), 1);
	    break;
	case LispQuote_t:
	    result = object->data.quote;
	    break;
	case LispFunctionQuote_t:
	    result = object->data.quote;
	    if (SYMBOLP(result))
		result = LispSymbolFunction(result);
	    else if (CONSP(result) && CAR(result) == Olambda)
		result = EVAL(result);
	    else
		LispDestroy("FUNCTION: %s is not a function", STROBJ(result));
	    break;
	case LispBackquote_t:
	    result = LispEvalBackquote(object->data.quote, 1);
	    break;
	case LispComma_t:
	    LispDestroy("EVAL: comma outside of backquote");
	default:
	    result = object;
	    break;
    }

    return (result);
}

LispObj *
LispApply1(LispObj *function, LispObj *argument)
{
    LispObj arguments;

    arguments.type = LispCons_t;
    arguments.data.cons.car = argument;
    arguments.data.cons.cdr = NIL;

    return (LispFuncall(function, &arguments, 0));
}

LispObj *
LispApply2(LispObj *function, LispObj *argument1, LispObj *argument2)
{
    LispObj arguments, cdr;

    arguments.type = cdr.type = LispCons_t;
    arguments.data.cons.car = argument1;
    arguments.data.cons.cdr = &cdr;
    cdr.data.cons.car = argument2;
    cdr.data.cons.cdr = NIL;

    return (LispFuncall(function, &arguments, 0));
}

LispObj *
LispApply3(LispObj *function, LispObj *arg1, LispObj *arg2, LispObj *arg3)
{
    LispObj arguments, car, cdr;

    arguments.type = car.type = cdr.type = LispCons_t;
    arguments.data.cons.car = arg1;
    arguments.data.cons.cdr = &car;
    car.data.cons.car = arg2;
    car.data.cons.cdr = &cdr;
    cdr.data.cons.car = arg3;
    cdr.data.cons.cdr = NIL;

    return (LispFuncall(function, &arguments, 0));
}

static LispObj *
LispRunFunMac(LispObj *name, LispObj *code, int macro, int base)
{
    LispObj *result = NIL;

    if (!macro) {
	int lex = lisp__data.env.lex;
	int did_jump = 1;
	LispBlock *block;

	block = LispBeginBlock(name, LispBlockClosure);
	lisp__data.env.lex = base;
	if (setjmp(block->jmp) == 0) {
	    for (; CONSP(code); code = CDR(code))
		result = EVAL(CAR(code));
	    did_jump = 0;
	}
	LispEndBlock(block);
	if (did_jump)
	    result = lisp__data.block.block_ret;
	lisp__data.env.lex = lex;
	lisp__data.env.head = lisp__data.env.length = base;
    }
    else {
	GC_ENTER();

	for (; CONSP(code); code = CDR(code))
	    result = EVAL(CAR(code));
	/* FIXME this does not work if macro has &aux variables,
	 * but there are several other missing features, like
	 * destructuring and more lambda list keywords still missing.
	 * TODO later.
	 */
	lisp__data.env.head = lisp__data.env.length = base;

	GC_PROTECT(result);
	result = EVAL(result);
	GC_LEAVE();
    }

    return (result);
}

LispObj *
LispRunSetf(LispArgList *alist, LispObj *setf, LispObj *place, LispObj *value)
{
    GC_ENTER();
    LispObj *store, *code, *expression, *result, quote;
    int base;

    code = setf->data.lambda.code;
    store = setf->data.lambda.data;

    quote.type = LispQuote_t;
    quote.data.quote = value;
    LispDoAddVar(CAR(store), &quote);
    ++lisp__data.env.head;
    base = LispMakeEnvironment(alist, place, Oexpand_setf_method, 0, 0);

    /* build expansion macro */
    expression = NIL;
    for (; CONSP(code); code = CDR(code))
	expression = EVAL(CAR(code));

    /* Minus 1 to pop the added variable */
    lisp__data.env.head = lisp__data.env.length = base - 1;

    /* protect expansion, and executes it */
    GC_PROTECT(expression);
    result = EVAL(expression);
    GC_LEAVE();

    return (result);
}

LispObj *
LispRunSetfMacro(LispAtom *atom, LispObj *arguments, LispObj *value)
{
    int base;
    GC_ENTER();
    LispObj *place, *body, *result, quote;

    place = NIL;
    base = LispMakeEnvironment(atom->property->alist,
			       arguments, atom->object, 0, 0);
    body = atom->property->fun.function->data.lambda.code;

    /* expand macro body */
    for (; CONSP(body); body = CDR(body))
	place = EVAL(CAR(body));

    /* protect expansion */
    GC_PROTECT(place);

    /* restore environment */
    lisp__data.env.head = lisp__data.env.length = base;

    /* value is already evaluated */
    quote.type = LispQuote_t;
    quote.data.quote = value;

    /* call setf again */
    result = APPLY2(Osetf, place, &quote);

    GC_LEAVE();

    return (result);
}

char *
LispStrObj(LispObj *object)
{
    static int first = 1;
    static char buffer[34];
    static LispObj stream;
    static LispString string;

    if (first) {
	stream.type = LispStream_t;
	stream.data.stream.source.string = &string;
	stream.data.stream.pathname = NIL;
	stream.data.stream.type = LispStreamString;
	stream.data.stream.readable = 0;
	stream.data.stream.writable = 1;

	string.string = buffer;
	string.fixed = 1;
	string.space = sizeof(buffer) - 1;
	first = 0;
    }

    string.length = string.output = 0;

    LispWriteObject(&stream, object);

    /* make sure string is nul terminated */
    string.string[string.length] = '\0';
    if (string.length >= 32) {
	if (buffer[0] == '(')
	    strcpy(buffer + 27, "...)");
	else
	    strcpy(buffer + 28, "...");
    }

    return (buffer);
}

void
LispPrint(LispObj *object, LispObj *stream, int newline)
{
    if (stream != NIL && !STREAMP(stream)) {
	LispDestroy("PRINT: %s is not a stream", STROBJ(stream));
    }
    if (newline && LispGetColumn(stream))
	LispWriteChar(stream, '\n');
    LispWriteObject(stream, object);
    if (stream == NIL || (stream->data.stream.type == LispStreamStandard &&
	stream->data.stream.source.file == Stdout))
	LispFflush(Stdout);
}

void
LispUpdateResults(LispObj *cod, LispObj *res)
{
    LispSetVar(RUN[2], LispGetVar(RUN[1]));
    LispSetVar(RUN[1], LispGetVar(RUN[0]));
    LispSetVar(RUN[0], cod);

    LispSetVar(RES[2], LispGetVar(RES[1]));
    LispSetVar(RES[1], LispGetVar(RES[0]));
    LispSetVar(RES[0], res);
}

#ifdef SIGNALRETURNSINT
int
#else
void
#endif
LispSignalHandler(int signum)
{
    LispSignal(signum);
#ifdef SIGNALRETURNSINT
    return (0);
#endif
}

void
LispSignal(int signum)
{
    char *errstr;
    char buffer[32];

    if (lisp__disable_int) {
	lisp__interrupted = signum;
	return;
    }
    switch (signum) {
	case SIGINT:
	    errstr = "interrupted";
	    break;
	case SIGFPE:
	    errstr = "floating point exception";
	    break;
	default:
	    sprintf(buffer, "signal %d received", signum);
	    errstr = buffer;
	    break;
    }
    LispDestroy(errstr);
}

void
LispDisableInterrupts(void)
{
    ++lisp__disable_int;
}

void
LispEnableInterrupts(void)
{
    --lisp__disable_int;
    if (lisp__disable_int <= 0 && lisp__interrupted)
	LispSignal(lisp__interrupted);
}

void
LispMachine(void)
{
    LispObj *cod, *obj;

    lisp__data.sigint = signal(SIGINT, LispSignalHandler);
    lisp__data.sigfpe = signal(SIGFPE, LispSignalHandler);

    /*CONSTCOND*/
    while (1) {
	if (sigsetjmp(lisp__data.jmp, 1) == 0) {
	    lisp__data.running = 1;
	    if (lisp__data.interactive && lisp__data.prompt) {
		LispFputs(Stdout, lisp__data.prompt);
		LispFflush(Stdout);
	    }
	    if ((cod = LispRead()) != NULL) {
		obj = EVAL(cod);
		if (lisp__data.interactive) {
		    if (RETURN_COUNT >= 0)
			LispPrint(obj, NIL, 1);
		    if (RETURN_COUNT > 0) {
			int i;

			for (i = 0; i < RETURN_COUNT; i++)
			    LispPrint(RETURN(i), NIL, 1);
		    }
		    LispUpdateResults(cod, obj);
		    if (LispGetColumn(NIL))
			LispWriteChar(NIL, '\n');
		}
	    }
	    LispTopLevel();
	}
	if (lisp__data.eof)
	    break;
    }

    signal(SIGINT, lisp__data.sigint);
    signal(SIGFPE, lisp__data.sigfpe);

    lisp__data.running = 0;
}

void *
LispExecute(char *str)
{
    static LispObj stream;
    static LispString string;
    static int first = 1;

    int running = lisp__data.running;
    LispObj *result, *cod, *obj, **presult = &result;

    if (str == NULL || *str == '\0')
	return (NIL);

    *presult = NIL;

    if (first) {
	stream.type = LispStream_t;
	stream.data.stream.source.string = &string;
	stream.data.stream.pathname = NIL;
	stream.data.stream.type = LispStreamString;
	stream.data.stream.readable = 1;
	stream.data.stream.writable = 0;
	string.output = 0;
	first = 0;
    }
    string.string = str;
    string.length = strlen(str);
    string.input = 0;

    LispPushInput(&stream);
    if (!running) {
	lisp__data.running = 1;
	if (sigsetjmp(lisp__data.jmp, 1) != 0)
	    return (NULL);
    }

    cod = COD;
    /*CONSTCOND*/
    while (1) {
	if ((obj = LispRead()) != NULL) {
	    result = EVAL(obj);
	    COD = cod;
	}
	if (lisp__data.eof)
	    break;
    }
    LispPopInput(&stream);

    lisp__data.running = running;

    return (result);
}

void
LispBegin(void)
{
    int i;
    LispAtom *atom;
    char results[4];
    LispObj *object, *path, *ext;

    pagesize = LispGetPageSize();
    segsize = pagesize / sizeof(LispObj);

    /* Initialize memory management */
    lisp__data.mem.mem = (void**)calloc(lisp__data.mem.space = 16,
					sizeof(void*));
    lisp__data.mem.index = lisp__data.mem.level = 0;

    /* Allow LispGetVar to check ATOMID() of unbound symbols */
    UNBOUND->data.atom = (LispAtom*)LispCalloc(1, sizeof(LispAtom));
    LispMused(UNBOUND->data.atom);
    noproperty.value = UNBOUND;

    if (Stdin == NULL)
	Stdin = LispFdopen(0, FILE_READ);
    if (Stdout == NULL)
	Stdout = LispFdopen(1, FILE_WRITE | FILE_BUFFERED);
    if (Stderr == NULL)
	Stderr = LispFdopen(2, FILE_WRITE);

    /* minimum number of free cells after GC
     * if sizeof(LispObj) == 16, than a minfree of 1024 would try to keep
     * at least 16Kb of free cells.
     */
    minfree = 1024;

    MOD = COD = PRO = NIL;
#ifdef DEBUGGER
    DBG = BRK = NIL;
#endif

    /* allocate initial object cells */
    LispAllocSeg(&objseg, minfree);
    LispAllocSeg(&atomseg, pagesize);
    lisp__data.gc.average = segsize;

    /* Don't allow gc in initialization */
    GCDisable();

    /* Initialize package system, the current package is LISP. Order of
     * initialization is very important here */
    lisp__data.lisp = LispNewPackage(STRING("LISP"),
				     CONS(STRING("COMMON-LISP"), NIL));

    /* Make LISP package the current one */
    lisp__data.pack = lisp__data.savepack =
	lisp__data.lisp->data.package.package;

    /* Allocate space in LISP package */
    LispMoreGlobals(lisp__data.pack);

    /* Allocate  space for multiple value return values */
    lisp__data.returns.values = malloc(MULTIPLE_VALUES_LIMIT *
				       (sizeof(LispObj*)));

    /*  Create the first atom, do it "by hand" because macro "PACKAGE"
     * cannot yet be used. */
    atom = LispGetPermAtom("*PACKAGE*");
    lisp__data.package = atomseg.freeobj;
    atomseg.freeobj = CDR(atomseg.freeobj);
    --atomseg.nfree;
    lisp__data.package->type = LispAtom_t;
    lisp__data.package->data.atom = atom;
    atom->object = lisp__data.package;
    atom->package = lisp__data.lisp;

    /* Set package list, to be used by (gc) and (list-all-packages) */
    PACK = CONS(lisp__data.lisp, NIL);

    /* Make *PACKAGE* a special variable */
    LispProclaimSpecial(lisp__data.package, lisp__data.lisp, NIL);

	/* Value of macro "PACKAGE" is now properly available */

    /* Changing *PACKAGE* is like calling (in-package) */
    lisp__data.package->data.atom->watch = 1;

    /* And available to other packages */
    LispExportSymbol(lisp__data.package);

    /* Initialize stacks */
    LispMoreEnvironment();
    LispMoreStack();

    /* Create the KEYWORD package */
    Skeyword = GETATOMID("KEYWORD");
    object = LispNewPackage(STRING(Skeyword),
			    CONS(STRING(""), NIL));

    /* Update list of packages */
    PACK = CONS(object, PACK);

    /* Allow easy access to the keyword package */
    lisp__data.keyword = object;
    lisp__data.key = object->data.package.package;

    /* Initialize some static important symbols */
    Olambda		= STATIC_ATOM("LAMBDA");
    LispExportSymbol(Olambda);
    Okey		= STATIC_ATOM("&KEY");
    LispExportSymbol(Okey);
    Orest		= STATIC_ATOM("&REST");
    LispExportSymbol(Orest);
    Ooptional		= STATIC_ATOM("&OPTIONAL");
    LispExportSymbol(Ooptional);
    Oaux		= STATIC_ATOM("&AUX");
    LispExportSymbol(Oaux);
    Kunspecific		= KEYWORD("UNSPECIFIC");
    Oformat		= STATIC_ATOM("FORMAT");
    Oexpand_setf_method	= STATIC_ATOM("EXPAND-SETF-METHOD");

    Omake_struct	= STATIC_ATOM("MAKE-STRUCT");
    Ostruct_access	= STATIC_ATOM("STRUCT-ACCESS");
    Ostruct_store	= STATIC_ATOM("STRUCT-STORE");
    Ostruct_type	= STATIC_ATOM("STRUCT-TYPE");
    Smake_struct	= ATOMID(Omake_struct);
    Sstruct_access	= ATOMID(Ostruct_access);
    Sstruct_store	= ATOMID(Ostruct_store);
    Sstruct_type	= ATOMID(Ostruct_type);

    /* Initialize some static atom ids */
    Snil		= GETATOMID("NIL");
    St			= GETATOMID("T");
    Saux		= ATOMID(Oaux);
    Skey		= ATOMID(Okey);
    Soptional		= ATOMID(Ooptional);
    Srest		= ATOMID(Orest);
    Sand		= GETATOMID("AND");
    Sor			= GETATOMID("OR");
    Snot		= GETATOMID("NOT");
    Satom		= GETATOMID("ATOM");
    Ssymbol		= GETATOMID("SYMBOL");
    Sinteger		= GETATOMID("INTEGER");
    Scharacter		= GETATOMID("CHARACTER");
    Sstring		= GETATOMID("STRING");
    Slist		= GETATOMID("LIST");
    Scons		= GETATOMID("CONS");
    Svector		= GETATOMID("VECTOR");
    Sarray		= GETATOMID("ARRAY");
    Sstruct		= GETATOMID("STRUCT");
    Sfunction		= GETATOMID("FUNCTION");
    Spathname		= GETATOMID("PATHNAME");
    Srational		= GETATOMID("RATIONAL");
    Sfloat		= GETATOMID("FLOAT");
    Scomplex		= GETATOMID("COMPLEX");
    Sopaque		= GETATOMID("OPAQUE");
    Sdefault		= GETATOMID("DEFAULT");

    LispArgList_t	= LispRegisterOpaqueType("LispArgList*");

    lisp__data.unget = malloc(sizeof(LispUngetInfo*));
    lisp__data.unget[0] = calloc(1, sizeof(LispUngetInfo));
    lisp__data.nunget = 1;

    lisp__data.standard_input = ATOM2("*STANDARD-INPUT*");
    SINPUT = STANDARDSTREAM(Stdin, lisp__data.standard_input, STREAM_READ);
    lisp__data.interactive = 1;
    LispProclaimSpecial(lisp__data.standard_input,
			lisp__data.input_list = SINPUT, NIL);
    LispExportSymbol(lisp__data.standard_input);

    lisp__data.standard_output = ATOM2("*STANDARD-OUTPUT*");
    SOUTPUT = STANDARDSTREAM(Stdout, lisp__data.standard_output, STREAM_WRITE);
    LispProclaimSpecial(lisp__data.standard_output,
			lisp__data.output_list = SOUTPUT, NIL);
    LispExportSymbol(lisp__data.standard_output);

    object = ATOM2("*STANDARD-ERROR*");
    lisp__data.error_stream = STANDARDSTREAM(Stderr, object, STREAM_WRITE);
    LispProclaimSpecial(object, lisp__data.error_stream, NIL);
    LispExportSymbol(object);

    lisp__data.modules = ATOM2("*MODULES*");
    LispProclaimSpecial(lisp__data.modules, MOD, NIL);
    LispExportSymbol(lisp__data.modules);

    object = CONS(KEYWORD("UNIX"), CONS(KEYWORD("XEDIT"), NIL));
    lisp__data.features = ATOM2("*FEATURES*");
    LispProclaimSpecial(lisp__data.features, object, NIL);
    LispExportSymbol(lisp__data.features);

    object = ATOM2("MULTIPLE-VALUES-LIMIT");
    LispDefconstant(object, FIXNUM(MULTIPLE_VALUES_LIMIT + 1), NIL);
    LispExportSymbol(object);

    /* Reenable gc */
    GCEnable();

    LispBytecodeInit();
    LispPackageInit();
    LispCoreInit();
    LispMathInit();
    LispPathnameInit();
    LispStreamInit();
    LispRegexInit();
    LispWriteInit();

    lisp__data.prompt = isatty(0) ? "> " : NULL;

    lisp__data.errexit = !lisp__data.interactive;

    if (lisp__data.interactive) {
	/* add +, ++, +++, *, **, and *** */
	for (i = 0; i < 3; i++) {
	    results[i] = '+';
	    results[i + 1] = '\0';
	    RUN[i] = ATOM(results);
	    LispSetVar(RUN[i], NIL);
	    LispExportSymbol(RUN[i]);
	}
	for (i = 0; i < 3; i++) {
	    results[i] = '*';
	    results[i + 1] = '\0';
	    RES[i] = ATOM(results);
	    LispSetVar(RES[i], NIL);
	    LispExportSymbol(RES[i]);
	}
    }
    else
	RUN[0] = RUN[1] = RUN[2] = RES[0] = RES[1] = RES[2] = NIL;

    /* Add LISP builtin functions */
    for (i = 0; i < sizeof(lispbuiltins) / sizeof(lispbuiltins[0]); i++)
	LispAddBuiltinFunction(&lispbuiltins[i]);

    EXECUTE("(require \"lisp\")");

    object = ATOM2("*DEFAULT-PATHNAME-DEFAULTS*");
#ifdef LISPDIR
    {
	int length;
	char *pathname = LISPDIR;

	length = strlen(pathname);
	if (length && pathname[length - 1] != '/') {
	    pathname = LispMalloc(length + 2);

	    strcpy(pathname, LISPDIR);
	    strcpy(pathname + length, "/");
	    path = LSTRING2(pathname, length + 1);
	}
	else
	    path = LSTRING(pathname, length);
    }
#else
    path = STRING("");
#endif
    GCDisable();
    LispProclaimSpecial(object, APPLY1(Oparse_namestring, path), NIL);
    LispExportSymbol(object);
    GCEnable();

    /* Create and make EXT the current package */
    PACKAGE = ext = LispNewPackage(STRING("EXT"), NIL);
    lisp__data.pack = lisp__data.savepack = PACKAGE->data.package.package;

    /* Update list of packages */
    PACK = CONS(ext, PACK);

    /* Import LISP external symbols in EXT package */
    LispUsePackage(lisp__data.lisp);

    /* Add EXT non standard builtin functions */
    for (i = 0; i < sizeof(extbuiltins) / sizeof(extbuiltins[0]); i++)
	LispAddBuiltinFunction(&extbuiltins[i]);

    /* Create and make USER the current package */
    GCDisable();
    PACKAGE = LispNewPackage(STRING("USER"),
			     CONS(STRING("COMMON-LISP-USER"), NIL));
    GCEnable();
    lisp__data.pack = lisp__data.savepack = PACKAGE->data.package.package;

    /* Update list of packages */
    PACK = CONS(PACKAGE, PACK);

    /* USER package inherits all LISP external symbols */
    LispUsePackage(lisp__data.lisp);
    /* And all EXT external symbols */
    LispUsePackage(ext);

    LispTopLevel();
}

void
LispEnd()
{
    /* XXX needs to free all used memory, not just close file descriptors */
}

void
LispSetPrompt(char *prompt)
{
    lisp__data.prompt = prompt;
}

void
LispSetInteractive(int interactive)
{
    lisp__data.interactive = !!interactive;
}

void
LispSetExitOnError(int errexit)
{
    lisp__data.errexit = !!errexit;
}

void
LispDebug(int enable)
{
    lisp__data.debugging = !!enable;

#ifdef DEBUGGER
    /* assumes we are at the toplevel */
    DBG = BRK = NIL;
    lisp__data.debug_level = -1;
    lisp__data.debug_step = 0;
#endif
}
