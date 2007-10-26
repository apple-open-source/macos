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

/* $XFree86: xc/programs/xedit/lisp/bytecode.h,v 1.5tsi Exp $ */

#include "lisp/private.h"

#ifndef Lisp_Bytecode_h
#define Lisp_Bytecode_h

/* Number of arguments for &REST functions with no side effects,
 * i.e. does not need to allocate new cells to build argument list.
 * If this value is changed, it is also required to add more
 * XBC_BCONS? opcodes and change ExecuteBytecode() */
#define MAX_BCONS	8

typedef enum {
    XBP_CONSP,
    XBP_LISTP,
    XBP_NUMBERP
} LispBytePredicate;

typedef enum {
    XBC_NOOP,

    XBC_INV,		/* If NIL loaded, change to T else to NIL */
    XBC_NIL,		/* Load NIL */
    XBC_T,		/* Load T */

    XBC_PRED,		/* Preffix for predicate test */
	/* Followed by byte indicating test */

    XBC_CAR,		/* Set loaded value to it's car */
    XBC_CDR,		/* Set loaded value to it's cdr */

    XBC_CAR_SET,	/* Set local variable to CAR or CDR of loaded value */
    XBC_CDR_SET,
	/* Offset of local variable follows */

    /* Sets C[AD]R of top of builtin stack with
     * loaded value and pop builtin stack */
    XBC_RPLACA,
    XBC_RPLACD,

    /* Push only one argument in builtin stack, and call directly
     * LispObjectCompare without the need of pushing the second arguument */
    XBC_EQ,
    XBC_EQL,
    XBC_EQUAL,
    XBC_EQUALP,

    XBC_LENGTH,
    XBC_LAST,
    XBC_NTHCDR,

    XBC_CAR_PUSH,	/* Pushes CAR or CDR of loaded value to builtin stack */
    XBC_CDR_PUSH,

    XBC_PUSH,		/* Push argument in builtin stack */
    XBC_PUSH_NIL,	/* Pushes NIL in builtin stack */
    XBC_PUSH_UNSPEC,	/* Pushes #<UNSPEC> in builtin stack */
    XBC_PUSH_T,		/* Pushes T in builtin stack */
    XBC_PUSH_NIL_N,	/* Pushes NIL in the builtin stack N times */
    XBC_PUSH_UNSPEC_N,	/* Pushes #<UNSPEC> in the builtin stack N times */


	/* The builtin stack base is saved when entering the bytecode
	 * interpreter, and the bytecode does not reenter from builtin
	 * functions, yet, so there is no need, for XBC_BSAVE and
	 * XBC_BREST instructions */

    XBC_LET,		/* Push loaded value to stack */
    XBC_LETX,		/* Push loaded value to stack and bind */
	/*  Next byte(s) are the symbol offset */
    XBC_LET_NIL,	/* Push loaded value to stack */
    XBC_LETX_NIL,	/* Push loaded value to stack and bind */
	/*  Next byte(s) are the symbol offset */

    XBC_LETBIND,	/* Bind locally added variables */
	/* Followed by number of symbols to bind */

    XBC_UNLET,		/* Unbind locally binded variables */
	/* Followed by number of symbols to unbind */

    XBC_LOAD,		/* Load argument already from the stack */
	/* Followed by  offset follows the opcode */
    XBC_LOAD_LET,	/* Load argument and push */
    XBC_LOAD_LETX,	/* Load argument,  push and bind */
	/* Followed by a short and the atom to be bound */
    XBC_LOAD_PUSH,

    XBC_LOADCON,	/* Load a literal constant */
	/* Next bytes are the constant object */
    XBC_LOADCON_LET,	/* Load a literal constant and push */
    XBC_LOADCON_LETX,	/* Load a literal constant, push and bind */
	/* Followed by object constant and symbol to be bound */
    XBC_LOADCON_PUSH,

    /* Load CAR or CDR of local variable */
    XBC_LOAD_CAR,
    XBC_LOAD_CDR,

    /* Change local variable value to it's CAR or CDR */
    XBC_LOAD_CAR_STORE,
    XBC_LOAD_CDR_STORE,

    XBC_LOADCON_SET,
	/* Followed by constant offset and local variable offset */

    XBC_LOADSYM,	/* Load object symbol value */
	/* The object atom pointer follows de opcode */
    XBC_LOADSYM_LET,	/* Load object symbol value and push */
    XBC_LOADSYM_LETX,	/* Load object symbol value, push and bind */
	/* The symbol atom name and bounded atom name to be bound follows */
    XBC_LOADSYM_PUSH,

    XBC_LOAD_SET,	/* Set value of local variable to the value of another */
	/* Followed by two shorts */
    XBC_LOAD_CAR_SET,	/* Like LOAD_SET, but apply CAR or CDR in the value */
    XBC_LOAD_CDR_SET,

    XBC_SET,		/* Change value of local variable */
	/* A short integer with relative offset follows opcode */
    XBC_SETSYM,		/* Change value of symbol */
	/* The atom symbol pointer follows opcode */

    XBC_SET_NIL,	/* Like XBC_SET but sets the local variable to NIL */

    XBC_CALL,		/* Call builtin function */
	/* 1 byte follows telling how many arguments to use */
	/* LispBuiltin pointer follows opcode */

    XBC_CALL_SET,
	/* Like BCALL, but also followed by an short index of local variable */

    XBC_BYTECALL,	/* Call bytecode */
	/* 1 byte for number of arguments */
	/* 1 byte for index in bytecode table */

    XBC_FUNCALL,
	/* Opcode followed by function and arguments objects, to
	 * be evaluated at run time, as it was not resolved at
	 * bytecode generation time (or is not implemented in
	 * the bytecode compiler) */


    XBC_LETREC,		/* Recursive function call */
	/* 1 byte follows telling how many arguments the funtion receives */

    /* Helper for math functions. Uses a preallocated CONS,
     * setting it's CAR to the loaded value, and in the same step
     * pushes the CONS to the builtin stack */
    XBC_BCONS,
    /* Like BCONS but it is a list of 2 arguments, first argument
     * is saved on the stack, replace with list of 2 arguments */
    XBC_BCONS1,
    /* Like BCONS1 but it is a list of 3 arguments, first arguments
     * are saved on the stack, replace with list of first stack
     * argument with list or 3 arguments, and pop stack  */
    XBC_BCONS2,
    XBC_BCONS3,
    XBC_BCONS4,
    XBC_BCONS5,
    XBC_BCONS6,
    XBC_BCONS7,

    /* Build a CONS */
    XBC_CCONS,		/* Make CONS of two constants */
	/* Constants follow opcode */
    XBC_CSTAR,		/* Save the CAR of the CONS */
    XBC_CFINI,		/* Loaded value is the CDR */

    /* These are to help in interactively building lists */
    XBC_LSTAR,		/* Start building a list in the gc protected stack */
    XBC_LCONS,		/* Add loaded object to list */
    XBC_LFINI,		/* List is finished */

    /* Inconditional jumps */
    XBC_JUMP,		/* Jump relative to following signed int */

    /* Conditional jumps, if true */
    XBC_JUMPT,		/* Jump relative to following signed int */

    /* Conditional jumps, if false */
    XBC_JUMPNIL,	/* Jump relative to following signed int */

    /* Structure field access and type check */
    XBC_STRUCT,
    XBC_STRUCTP,

    XBC_RETURN		/* Resume bytecode execution */
} LispByteOpcode;


struct _LispBytecode {
    unsigned char *code;	/* Bytecode data */
    long length;		/* length of bytecode stream */
};

/*
 * Prototypes
 */
void LispBytecodeInit(void);

LispObj *Lisp_Compile(LispBuiltin*);
LispObj *Lisp_Disassemble(LispBuiltin*);

LispObj *LispCompileForm(LispObj*);
LispObj *LispExecuteBytecode(LispObj*);

void Com_And(LispCom*, LispBuiltin*);
void Com_Block(LispCom*, LispBuiltin*);
void Com_C_r(LispCom*, LispBuiltin*);
void Com_Cond(LispCom*, LispBuiltin*);
void Com_Cons(LispCom*, LispBuiltin*);
void Com_Consp(LispCom*, LispBuiltin*);
void Com_Dolist(LispCom*, LispBuiltin*);
void Com_Eq(LispCom*, LispBuiltin*);
void Com_Go(LispCom*, LispBuiltin*);
void Com_If(LispCom*, LispBuiltin*);
void Com_Last(LispCom*, LispBuiltin*);
void Com_Length(LispCom*, LispBuiltin*);
void Com_Let(LispCom*, LispBuiltin*);
void Com_Letx(LispCom*, LispBuiltin*);
void Com_Listp(LispCom*, LispBuiltin*);
void Com_Loop(LispCom*, LispBuiltin*);
void Com_Nthcdr(LispCom*, LispBuiltin*);
void Com_Null(LispCom*, LispBuiltin*);
void Com_Numberp(LispCom*, LispBuiltin*);
void Com_Or(LispCom*, LispBuiltin*);
void Com_Progn(LispCom*, LispBuiltin*);
void Com_Return(LispCom*, LispBuiltin*);
void Com_ReturnFrom(LispCom*, LispBuiltin*);
void Com_Rplac_(LispCom*, LispBuiltin*);
void Com_Setq(LispCom*, LispBuiltin*);
void Com_Tagbody(LispCom*, LispBuiltin*);
void Com_Unless(LispCom*, LispBuiltin*);
void Com_Until(LispCom*, LispBuiltin*);
void Com_When(LispCom*, LispBuiltin*);
void Com_While(LispCom*, LispBuiltin*);

#endif /* Lisp_Bytecode_h */
