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

/* $XdotOrg: app/xedit/lisp/private.h,v 1.3 2004/12/04 00:43:13 kuhn Exp $ */
/* $XFree86: xc/programs/xedit/lisp/private.h,v 1.41 2003/05/27 22:27:04 tsi Exp $ */

#ifndef Lisp_private_h
#define Lisp_private_h

#include <X11/Xos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(X_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE X_POSIX_C_SOURCE
#include <setjmp.h>
#undef _POSIX_C_SOURCE
#else
#include <setjmp.h>
#endif
#include <unistd.h>
#include <sys/time.h>
#include "lisp/internal.h"

#include "lisp/core.h"
#ifdef DEBUGGER
#include "lisp/debugger.h"
#endif
#include "lisp/helper.h"
#include "lisp/string.h"
#include "lisp/struct.h"

/*
 * Defines
 */
#define	STRTBLSZ		23
#define MULTIPLE_VALUES_LIMIT	127
#define MAX_STACK_DEPTH		16384

#define FEATURES							\
    (lisp__data.features->data.atom->a_object ?				\
	(LispObj *)lisp__data.features->data.atom->property->value :	\
	NIL)
#define PACK	lisp__data.packlist
#define PACKAGE	lisp__data.package->data.atom->property->value
#define MOD	lisp__data.modlist
#define COD	lisp__data.codlist
#define RUN	lisp__data.runlist
#define RES	lisp__data.reslist
#define DBG	lisp__data.dbglist
#define BRK	lisp__data.brklist
#define PRO	lisp__data.prolist

#define SINPUT	lisp__data.input
#define SOUTPUT	lisp__data.output
#define STANDARD_INPUT						\
    lisp__data.standard_input->data.atom->property->value
#define STANDARD_OUTPUT						\
    lisp__data.standard_output->data.atom->property->value
#define STANDARDSTREAM(file, desc, flags)			\
	LispNewStandardStream(file, desc, flags)

/*
 * Types
 */
typedef struct _LispStream LispStream;
typedef struct _LispBlock LispBlock;
typedef struct _LispOpaque LispOpaque;
typedef struct _LispModule LispModule;
typedef struct _LispProperty LispProperty;
typedef struct _LispObjList LispObjList;
typedef struct _LispStringHash LispStringHash;
typedef struct _LispCharInfo LispCharInfo;


/* Normal function/macro arguments */
typedef struct _LispNormalArgs {
    int num_symbols;
    LispObj **symbols;		/* symbol names */
} LispNormalArgs;

/* &optional function/macro arguments */
typedef struct _LispOptionalArgs {
    int num_symbols;
    LispObj **symbols;		/* symbol names */
    LispObj **defaults;		/* default values, when unspecifed */
    LispObj **sforms;		/* T if variable specified, NIL otherwise */
} LispOptionalArgs;

/* &key function/macro arguments */
typedef struct _LispKeyArgs {
    int num_symbols;
    LispObj **symbols;		/* symbol names */
    LispObj **defaults;		/* default values */
    LispObj **sforms;		/* T if variable specified, NIL otherwise */
    LispObj **keys;		/* key names, for special keywords */
} LispKeyArgs;

/* &aux function/macro arguments */
typedef struct _LispAuxArgs {
    int num_symbols;
    LispObj **symbols;		/* symbol names */
    LispObj **initials;		/* initial values */
} LispAuxArgs;

/* characters in the field description have the format:
 *	'.'	normals has a list of normal arguments
 *	'o'	optionals has a list of &optional arguments
 *	'k'	keys has a list of &key arguments
 *	'r'	rest is a valid pointer to a &rest symbol
 *	'a'	auxs has a list of &aux arguments
 */
typedef struct _LispArgList {
    LispNormalArgs normals;
    LispOptionalArgs optionals;
    LispKeyArgs keys;
    LispObj *rest;
    LispAuxArgs auxs;
    int num_arguments;
    char *description;
} LispArgList;

typedef enum _LispDocType_t {
    LispDocVariable,
    LispDocFunction,
    LispDocStructure,
    LispDocType,
    LispDocSetf
} LispDocType_t;

struct _LispProperty {
    /* may be used by multiple packages */
    unsigned int refcount;

    /* package where the property was created */
    LispPackage *package;

    /* value of variable attached to symbol */
    LispObj *value;

    union {
	/* function attached to symbol */
	LispObj *function;
	/* builtin function attached to symbol*/
	LispBuiltin *builtin;
    } fun;
    /* function/macro argument list description */
    LispArgList *alist;

    /* symbol properties list */
    LispObj *properties;

    /* setf method */
    LispObj *setf;
    /* setf argument list description */
    LispArgList *salist;

    /* structure information */
    struct {
	LispObj *definition;
#define STRUCT_NAME		-3
#define STRUCT_CHECK		-2
#define STRUCT_CONSTRUCTOR	-1
	int function;		/* if >= 0, it is a structure field index */
    } structure;
};

struct _LispAtom {
    /* hint: dynamically binded variable */
    unsigned int dyn : 1;

    /* Property has useful data in value field */
    unsigned int a_object : 1;
    /* Property has useful data in fun.function field */
    unsigned int a_function : 1;
    /* Property has useful data in fun.builtin field */
    unsigned int a_builtin : 1;
    /* Property has useful data in fun.function field */
    unsigned int a_compiled : 1;
    /* Property has useful data in properties field */
    unsigned int a_property : 1;
    /* Property has useful data in setf field */
    unsigned int a_defsetf : 1;
    /* Property has useful data in defstruct field */
    unsigned int a_defstruct : 1;

    /* Symbol is extern */
    unsigned int ext : 1;

    /* Symbol must be quoted with '|' to be allow reading back */
    unsigned int unreadable : 1;

    /* Symbol value may need special handling when changed */
    unsigned int watch : 1;

    /* Symbol value is constant, cannot be changed */
    unsigned int constant : 1;

    char *string;
    LispObj *object;		/* backpointer to object ATOM */
    int offset;			/* in the environment list */
    LispObj *package;		/* package home of symbol */
    LispObj *function;		/* symbol function */
    LispObj *name;		/* symbol string */
    LispProperty *property;
    struct _LispAtom *next;

    LispObj *documentation[5];
};

struct _LispObjList {
    LispObj **pairs;		/* name0 ... nameN */
    int length;			/* number of objects */
    int space;			/* space allocated in field pairs */
};

struct _LispPackage {
    LispObjList glb;		/* global symbols in package */
    LispObjList use;		/* inherited packages */
    LispAtom *atoms[STRTBLSZ];	/* atoms in this package */
};

struct _LispOpaque {
    int type;
    char *desc;
    LispOpaque *next;
};

/* These strings are never released, they are used to avoid
 * the need of strcmp() on two symbol names, just compare pointers */
struct _LispStringHash {
    char *string;
    LispStringHash *next;
};

typedef enum _LispBlockType {
    LispBlockNone,	/* no block */
    LispBlockTag,	/* may become "invisible" */
    LispBlockCatch,	/* can be used to jump across function calls */
    LispBlockClosure,	/* hides blocks of type LispBlockTag bellow it */
    LispBlockProtect,	/* used by unwind-protect */
    LispBlockBody	/* used by tagbody and go */
} LispBlockType;

struct _LispBlock {
    LispBlockType type;
    LispObj *tag;
    jmp_buf jmp;
    int stack;
    int protect;
    int block_level;
#ifdef DEBUGGER
    int debug_level;
    int debug_step;
#endif
};

struct _LispModule {
    LispModule *next;
    void *handle;
    LispModuleData *data;
};

typedef struct _LispUngetInfo {
    char buffer[16];
    int offset;
} LispUngetInfo;

struct _LispMac {
    /* stack for builtin function arguments */
    struct {
	LispObj **values;
	int base;		/* base of arguments to function */
	int length;
	int space;
    } stack;

    /* environment */
    struct {
	LispObj **values;
	Atom_id *names;
	int lex;		/* until where variables are visible */
	int head;		/* top of environment */
	int length;		/* number of used pairs */
	int space;		/* number of objects in pairs */
    } env;

    struct {
	LispObj **values;
	int count;
    } returns;

    struct {
	LispObj **objects;
	int length;
	int space;
    } protect;

    LispObj *package;		/* package object */
    LispPackage *pack;		/* pointer to lisp__data.package->data.package.package */

    /* fast access to the KEYWORD package */
    LispObj *keyword;
    LispPackage *key;

    /* the LISP package */
    LispObj *lisp;

    /* only used if the package was changed, but an error generated
     * before returning to the toplevel */
    LispObj *savepackage;
    LispPackage *savepack;

    struct {
	int block_level;
	int block_size;
	LispObj *block_ret;
	LispBlock **block;
    } block;

    sigjmp_buf jmp;

    struct {
	unsigned int expandbits : 3;	/* code doesn't look like reusing cells
					 * so try to have a larger number of
					 * free cells */
	unsigned int immutablebits : 1;	/* need to reset immutable bits */
	unsigned int timebits : 1;	/* update gctime counter */
	unsigned int count;
	long gctime;
	int average;			/* of cells freed after gc calls */
    } gc;

    LispStringHash *strings[STRTBLSZ];
    LispOpaque *opqs[STRTBLSZ];
    int opaque;

    LispObj *standard_input, *input, *input_list;
    LispObj *standard_output, *output, *output_list;
    LispObj *error_stream;
    LispUngetInfo **unget;
    int iunget, nunget;
    int eof;

    int interactive;
    int errexit;

    struct {
	int index;
	int level;
	int space;
	void **mem;
    } mem;		/* memory from Lisp*Alloc, to be release in error */
    LispModule *module;
    LispObj *modules;
    char *prompt;

    LispObj *features;

    LispObj *modlist;		/* module list */
    LispObj *packlist;		/* list of packages */
    LispObj *codlist;		/* current code */
    LispObj *runlist[3];	/* +, ++, and +++ */
    LispObj *reslist[3];	/* *, **, and *** */
#ifdef DEBUGGER
    LispObj *dbglist;		/* debug information */
    LispObj *brklist;		/* breakpoints information */
#endif
    LispObj *prolist;		/* protect objects list */

#ifdef SIGNALRETURNSINT
    int (*sigint)(int);
    int (*sigfpe)(int);
#else
    void (*sigint)(int);
    void (*sigfpe)(int);
#endif

    int destroyed;		/* reached LispDestroy, used by unwind-protect */
    int running;		/* there is somewhere to siglongjmp */

    int ignore_errors;		/* inside a ignore-errors block */
    LispObj *error_condition;	/* actually, a string */

    int debugging;		/* debugger enabled? */
#ifdef DEBUGGER
    int debug_level;		/* almost always the same as lisp__data.level */
    int debug_step;		/* control for stoping and printing output */
    int debug_break;		/* next breakpoint number */
    LispDebugState debug;
#endif
};

struct _LispCharInfo {
    char **names;
};


/*
 * Prototypes
 */
void LispUseArgList(LispArgList*);
void LispFreeArgList(LispArgList*);
LispArgList *LispCheckArguments(LispFunType, LispObj*, char*, int);
LispObj *LispListProtectedArguments(LispArgList*);

LispObj *LispGetDoc(LispObj*);
LispObj *LispGetVar(LispObj*);
#ifdef DEBUGGER
void *LispGetVarAddr(LispObj*);	/* used by debugger */
#endif
LispObj *LispAddVar(LispObj*, LispObj*);
LispObj *LispSetVar(LispObj*, LispObj*);
void LispUnsetVar(LispObj*);

	/* only used at initialization time */
LispObj *LispNewStandardStream(LispFile*, LispObj*, int);

	/* create a new package */
LispObj *LispNewPackage(LispObj*, LispObj*);
	/* add package to use-list of current, and imports all extern symbols */
void LispUsePackage(LispObj*);
	/* make symbol extern in the current package */
void LispExportSymbol(LispObj*);
	/* imports symbol to current package */
void LispImportSymbol(LispObj*);

	/* always returns the same string */
char *LispGetAtomString(char*, int);

/* destructive fast reverse, note that don't receive a LispMac* argument */
LispObj *LispReverse(LispObj *list);

char *LispIntToOpaqueType(int);

/* (print) */
void LispPrint(LispObj*, LispObj*, int);

LispBlock *LispBeginBlock(LispObj*, LispBlockType);
#define BLOCKJUMP(block)				\
    lisp__data.stack.length = (block)->stack;		\
    lisp__data.protect.length = (block)->protect;	\
    longjmp((block)->jmp, 1)
void LispEndBlock(LispBlock*);
	/* if unwind-protect active, jump to cleanup code, else do nothing */
void LispBlockUnwind(LispBlock*);

void LispUpdateResults(LispObj*, LispObj*);
void LispTopLevel(void);

#define STRHASH(string)		LispDoHashString(string)
int LispDoHashString(char*);
LispAtom *LispDoGetAtom(char *str, int);
	/* get value from atom's property list */
LispObj *LispGetAtomProperty(LispAtom*, LispObj*);
	/* put value in atom's property list */
LispObj *LispPutAtomProperty(LispAtom*, LispObj*, LispObj*);
	/* remove value from atom's property list */
LispObj *LispRemAtomProperty(LispAtom*, LispObj*);
	/* replace atom's property list */
LispObj *LispReplaceAtomPropertyList(LispAtom*, LispObj*);

	/* returns function associated with symbol */
LispObj *LispSymbolFunction(LispObj*);
	/* returns symbol string name */
LispObj *LispSymbolName(LispObj*);

	/* define byte compiled function, or replace definition */
void LispSetAtomCompiledProperty(LispAtom*, LispObj*);
	/* remove byte compiled function property */
void LispRemAtomCompiledProperty(LispAtom*);
	/* define function, or replace function definition */
void LispSetAtomFunctionProperty(LispAtom*, LispObj*, LispArgList*);
	/* remove function property */
void LispRemAtomFunctionProperty(LispAtom*);
	/* define builtin, or replace builtin definition */
void LispSetAtomBuiltinProperty(LispAtom*, LispBuiltin*, LispArgList*);
	/* remove builtin property */
void LispRemAtomBuiltinProperty(LispAtom*);
	/* define setf macro, or replace current definition */
void LispSetAtomSetfProperty(LispAtom*, LispObj*, LispArgList*);
	/* remove setf macro */
void LispRemAtomSetfProperty(LispAtom*);
	/* create or change structure property */
void LispSetAtomStructProperty(LispAtom*, LispObj*, int);
	/* remove structure property */
void LispRemAtomStructProperty(LispAtom*);

void LispProclaimSpecial(LispObj*, LispObj*, LispObj*);
void LispDefconstant(LispObj*, LispObj*, LispObj*);

void LispAddDocumentation(LispObj*, LispObj*, LispDocType_t);
void LispRemDocumentation(LispObj*, LispDocType_t);
LispObj *LispGetDocumentation(LispObj*, LispDocType_t);

/* increases storage for functions returning multiple values */
void LispMoreReturns(void);

/* increases storage for temporarily protected data */
void LispMoreProtects(void);

/* Initialization */
extern int LispArgList_t;
extern LispCharInfo LispChars[256];

/* This function will return if the interpreter cannot be stopped */
extern void LispSignal(int);

void LispDisableInterrupts(void);
void LispEnableInterrupts(void);
#define DISABLE_INTERRUPTS()	LispDisableInterrupts()
#define ENABLE_INTERRUPTS()	LispEnableInterrupts()

/* Value returned by LispBegin, used everywhere in the code.
 * Only one interpreter instance allowed. */
extern LispMac lisp__data;

#endif /* Lisp_private_h */
