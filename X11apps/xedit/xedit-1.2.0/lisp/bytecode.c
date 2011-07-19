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

/* $XFree86: xc/programs/xedit/lisp/bytecode.c,v 1.17 2003/05/27 22:27:01 tsi Exp $ */


/*
somethings TODO:

 o Write code for allowing storing the bytecode on disk. Basically
   write a section of the bytecode with the package name of the symbol
   pointers, and after that, the symbols used. At load time just put
   the pointers in the bytecode. Constants can be stored as the string
   representation. Probably just storing the gc protected code as a
   string is enough to rebuild it.

 o Write code to store tags of BLOCK/CATCH and setjump buffer stacks,
   and, only keep track of this if non byte-compiled code is called,
   as after byte-compilation RETURN and THROW are just jumps.

 o Remove not reliable "optmization code" code from Com_XXX functions
   and do it later, removing dead code, tests with a constant argument,
   etc, in the "link time". Frequently joining sequential opcodes to a
   compound version.

 o Write an optimizer to do code transformation.

 o Write code to know when variables can be changed in place, this
   can save a huge time in loop counters.

 o Write code for fast garbage collection of objects that can be
   safely collected.

 o Cleanup of interpreted code. Having bytecode mean that the interpreter
   now is better having a clean and small code. If speed is important,
   it should be byte compiled.

 o Limit the stacks length. So that instead of using an index, use the
   pointer where an object value should be read/stored as the stack address
   would not change during the program execution.

 o Optimize jump to jump. Common in code like:
	(IF test
	    (GO somewhere)
	    (GO elsewhere)
	)
	(GO there)
   that generates a bytecode like:
	<code to evaluate test>
	JUMPNIL :NIL-RESULT
	:T-RESULT
	    JUMP :SOMEWHERE
	JUMP :END-OF-IF			;; <- this is not required, or even
	:NIL-RESULT			;;    better, notice the jump after
	    JUMP :ELSEWHERE		;;    the if and transform it into
	:END-OF-IF			;;    a JUMP :THERE (assuming there
	JUMP :THERE			;;    (was no jump in the T code).

 o Optimize variables that are known to not change it's value, i.e. pseudo
   constants. Loading the value of a constant should be faster than loading
   the current value of a variable; the constant table could fit in the
   processor cache line and needs less calculation to find the object address.

 o Fix some known problems, like when calling return or return-from while
   building the argument list to a builtin function, or inline of recursive
   functions.
 */


#include "lisp/bytecode.h"
#include "lisp/write.h"

#define	SYMBOL_KEYWORD	-1	/* A keyword, load as constant */
#define	SYMBOL_CONSTANT	-2	/* Defined as constant at compile time */
#define	SYMBOL_UNBOUND	-3	/* Not a local variable */

#define NEW_TREE(type)		CompileNewTree(com, type)

/* If in tagbody, ignore anything that is not code */
#define	IN_TAGBODY()		(com->block->type == LispBlockBody && \
				 com->level == com->tagbody)
#define	FORM_ENTER()		++com->level
#define	FORM_LEAVE()		--com->level

#define COMPILE_FAILURE(message)			\
    LispMessage("COMPILE: %s", message);		\
    longjmp(com->jmp, 1)

/*
 * Types
 */
typedef struct _CodeTree CodeTree;
typedef struct _CodeBlock CodeBlock;

typedef enum {
    CodeTreeBytecode,
    CodeTreeLabel,
    CodeTreeGo,
    CodeTreeJump,
    CodeTreeJumpIf,
    CodeTreeCond,
    CodeTreeBlock,
    CodeTreeReturn
} CodeTreeType;

struct _CodeTree {
    CodeTreeType type;

    /* Resolved when linking, may be adjusted while optimizing */
    long offset;

    LispByteOpcode code;

    union {
	signed char signed_char;
	signed short signed_short;
	signed int signed_int;
	LispAtom *atom;
	LispObj *object;
	CodeTree *tree;
	CodeBlock *block;
	struct {
	    unsigned char num_arguments;
	    LispBuiltin *builtin;
	    signed short offset;	/* Used if opcode is XBC_CALL_SET */
	} builtin;
	struct {
	    unsigned char num_arguments;
	    LispObj *name;
	    LispObj *lambda;
	} call;
	struct {
	    unsigned char num_arguments;
	    LispObj *code;
	} bytecall;
	struct {
	    short offset;
	    LispAtom *name;
	} let;
	struct {
	    LispAtom *symbol;
	    LispAtom *name;
	} let_sym;
	struct {
	    LispObj *object;
	    LispAtom *name;
	} let_con;
	struct {
	    signed short load;
	    signed short set;
	} load_set;
	struct {
	    LispObj *object;
	    signed short offset;
	} load_con_set;
	struct {
	    LispObj *car;
	    LispObj *cdr;
	} cons;
	struct {
	    short offset;
	    LispObj *definition;
	} struc;
    } data;

    CodeTree *next;
    CodeTree *group;
    CodeBlock *block;
};

struct _CodeBlock {
    LispBlockType type;
    LispObj *tag;

    struct {
	LispObj **labels;
	CodeTree **codes;	/* Filled at link time */
	int length;
	int space;
    } tagbody;

    struct {
	LispAtom **symbols;	/* Identifiers of variables in a block */
	int *flags;		/* Information about usage of the variable */
	int length;
    } variables;

    int bind;			/* Used in case of RETURN from LET */
    int level;			/* Nesting level block was created */

    CodeTree *tree, *tail;
    CodeBlock *prev;		/* Linked list as a stack */
    CodeTree *parent;		/* Back reference */
};

struct _LispCom {
    unsigned char *bytecode;	/* Bytecode generated so far */
    long length;

    CodeBlock *block, *toplevel;

    int tagbody;		/* Inside a tagbody block? */
    int level;			/* Nesting level */
    int macro;			/* Expanding a macro? */

    int lex;

    int warnings;

    LispObj *form, *plist;

    jmp_buf jmp;		/* Used if compilation cannot be finished */

    struct {
	int cstack;	/* Current number of objects in forms evaluation */
	int cbstack;
	int cpstack;
	int stack;	/* max number of objects will be loaded in stack */
	int bstack;
	int pstack;
    } stack;

    struct {
	/* Constant table */
	LispObj **constants;
	int num_constants;
	/* Symbol table */
	LispAtom **symbols;
	int num_symbols;
	/* Builtin table */
	LispBuiltin **builtins;
	int num_builtins;
	/* Bytecode table */
	LispObj **bytecodes;
	int num_bytecodes;
    } table;
};

/*
 * Prototypes
 */
static LispObj *MakeBytecodeObject(LispCom*, LispObj*, LispObj*);

static CodeTree *CompileNewTree(LispCom*, CodeTreeType);
static void CompileFreeState(LispCom*);
static void CompileFreeBlock(CodeBlock*);
static void CompileFreeTree(CodeTree*);

static void CompileIniBlock(LispCom*, LispBlockType, LispObj*);
static void CompileFiniBlock(LispCom*);

static void com_BytecodeChar(LispCom*, LispByteOpcode, char);
static void com_BytecodeShort(LispCom*, LispByteOpcode, short);
static void com_BytecodeObject(LispCom*, LispByteOpcode, LispObj*);
static void com_BytecodeCons(LispCom*, LispByteOpcode, LispObj*, LispObj*);

static void com_BytecodeAtom(LispCom*, LispByteOpcode, LispAtom*);

static void com_Bytecode(LispCom*, LispByteOpcode);

static void com_Load(LispCom*, short);
static void com_LoadLet(LispCom*, short, LispAtom*);
static void com_LoadPush(LispCom*, short);

static void com_Let(LispCom*, LispAtom*);

static void com_Bind(LispCom*, short);
static void com_Unbind(LispCom*, short);

static void com_LoadSym(LispCom*, LispAtom*);
static void com_LoadSymLet(LispCom*, LispAtom*, LispAtom*);
static void com_LoadSymPush(LispCom*, LispAtom*);

static void com_LoadCon(LispCom*, LispObj*);
static void com_LoadConLet(LispCom*, LispObj*, LispAtom*);
static void com_LoadConPush(LispCom*, LispObj*);

static void com_Set(LispCom*, short);
static void com_SetSym(LispCom*, LispAtom*);

static void com_Struct(LispCom*, short, LispObj*);
static void com_Structp(LispCom*, LispObj*);

static void com_Call(LispCom*, unsigned char, LispBuiltin*);
static void com_Bytecall(LispCom*, unsigned char, LispObj*);
static void com_Funcall(LispCom*, LispObj*, LispObj*);

static void CompileStackEnter(LispCom*, int, int);
static void CompileStackLeave(LispCom*, int, int);

static void LinkBytecode(LispCom*);

static LispObj *ExecuteBytecode(unsigned char*);


/* Defined in lisp.c */
void LispMoreStack(void);
void LispMoreEnvironment(void);
void LispMoreGlobals(LispPackage*);
LispObj *LispEvalBackquote(LispObj*, int);
void LispSetAtomObjectProperty(LispAtom*, LispObj*);

/*
 * Initialization
 */
extern int pagesize;

LispObj x_cons[8];
static LispObj *cons, *cons1, *cons2, *cons3, *cons4, *cons5, *cons6, *cons7;

/*
 * Implementation
 */
#include "lisp/compile.c"

void
LispBytecodeInit(void)
{
    cons = &x_cons[7];
    cons->type = LispCons_t;
    CDR(cons) = NIL;
    cons1 = &x_cons[6];
    cons1->type = LispCons_t;
    CDR(cons1) = cons;
    cons2 = &x_cons[5];
    cons2->type = LispCons_t;
    CDR(cons2) = cons1;
    cons3 = &x_cons[4];
    cons3->type = LispCons_t;
    CDR(cons3) = cons2;
    cons4 = &x_cons[3];
    cons4->type = LispCons_t;
    CDR(cons4) = cons3;
    cons5 = &x_cons[2];
    cons5->type = LispCons_t;
    CDR(cons5) = cons4;
    cons6 = &x_cons[1];
    cons6->type = LispCons_t;
    CDR(cons6) = cons5;
    cons7 = &x_cons[0];
    cons7->type = LispCons_t;
    CDR(cons7) = cons6;
}

LispObj *
Lisp_Compile(LispBuiltin *builtin)
/*
 compile name &optional definition
 */
{
    GC_ENTER();
    LispObj *result, *warnings_p, *failure_p;

    LispObj *name, *definition;

    definition = ARGUMENT(1);
    name = ARGUMENT(0);

    result = name;
    warnings_p = NIL;
    failure_p = T;

    if (name != NIL) {
	LispAtom *atom;

	CHECK_SYMBOL(name);
	atom = name->data.atom;
	if (atom->a_builtin || atom->a_compiled)
	    goto finished_compilation;
	else if (atom->a_function) {
	    LispCom com;
	    int failed;
	    int lex = 0, base;
	    LispArgList *alist;
	    LispObj *lambda, *form, *arguments;

	    lambda = atom->property->fun.function;
	    if (definition != UNSPEC || lambda->funtype != LispFunction)
		/* XXX TODO replace definition etc. */
		goto finished_compilation;
	    alist = atom->property->alist;

	    memset(&com, 0, sizeof(LispCom));
	    com.toplevel = com.block = LispCalloc(1, sizeof(CodeBlock));
	    com.block->type = LispBlockClosure;
	    com.block->tag = name;

	    /*  Create a fake argument list to avoid yet another flag
	     * for ComCall. The value does not matter, just the fact
	     * that the symbol will be bound or not in the implicit
	     * PROGN of the function body. */
	    base = alist->num_arguments - alist->auxs.num_symbols;
	    if (base) {
		LispObj *argument;
		int i, sforms;

		for (i = sforms = 0; i < alist->optionals.num_symbols; i++)
		    if (alist->optionals.sforms[i])
			++sforms;

		arguments = form = NIL;
		i = sforms +
		    alist->normals.num_symbols + alist->optionals.num_symbols;

		if (i) {
		    arguments = form = CONS(NIL, NIL);
		    GC_PROTECT(arguments);
		    for (--i; i > 0; i--) {
			RPLACD(form, CONS(NIL, NIL));
			form = CDR(form);
		    }
		}

		for (i = 0; i < alist->keys.num_symbols; i++) {
		    /* key symbol */
		    if (alist->keys.keys[i])
			argument = QUOTE(alist->keys.keys[i]);
		    else
			argument = alist->keys.symbols[i];

		    /* add key */
		    if (arguments == NIL) {
			arguments = form = CONS(argument, NIL);
			GC_PROTECT(arguments);
		    }
		    else {
			RPLACD(form, CONS(argument, NIL));
			form = CDR(form);
		    }

		    /* add value */
		    RPLACD(form, CONS(NIL, NIL));
		    form = CDR(form);

		    if (alist->keys.sforms[i]) {
			RPLACD(form, CONS(NIL, NIL));
			form = CDR(form);
		    }
		}

		if (alist->rest) {
		    if (arguments == NIL) {
			arguments = form = CONS(NIL, NIL);
			GC_PROTECT(arguments);
		    }
		    else {
			RPLACD(form, CONS(NIL, NIL));
			form = CDR(form);
		    }
		}
	    }
	    else
		arguments = NIL;

	    form = CONS(lambda->data.lambda.code, NIL);
	    GC_PROTECT(form);
	    com.form = form;
	    com.plist = CONS(NIL, NIL);
	    GC_PROTECT(com.plist);

	    failed = 1;
	    if (setjmp(com.jmp) == 0) {
		/* Save interpreter state */
		lex = com.lex = lisp__data.env.lex;
		base = ComCall(&com, alist, name, arguments, 1, 0, 1);

		/* Generate code tree */
		lisp__data.env.lex = base;
		ComProgn(&com, CAR(form));
		failed = 0;
	    }

	    /* Restore interpreter state */
	    lisp__data.env.lex = lex;
	    lisp__data.env.head = lisp__data.env.length = base;

	    if (!failed) {
		failure_p = NIL;
		result = MakeBytecodeObject(&com, name,
					    lambda->data.lambda.data);
		LispSetAtomCompiledProperty(atom, result);
		result = name;
	    }
	    if (com.warnings)
		warnings_p = FIXNUM(com.warnings);
	    goto finished_compilation;
	}
	else
	    goto undefined_function;
    }

undefined_function:
    LispDestroy("%s: the function %s is undefined",
		STRFUN(builtin), STROBJ(name));

finished_compilation:
    RETURN(0) = warnings_p;
    RETURN(1) = failure_p;
    RETURN_COUNT = 2;
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_Disassemble(LispBuiltin *builtin)
/*
 disassemble function
 */
{
    int macro;
    char buffer[128];
    LispAtom *atom;
    LispArgList *alist;
    LispBuiltin *xbuiltin;
    LispObj *name, *lambda, *bytecode;

    LispObj *function;

    function = ARGUMENT(0);

    macro = 0;
    alist = NULL;
    xbuiltin = NULL;
    name = bytecode = NULL;

    switch (OBJECT_TYPE(function)) {
	case LispFunction_t:
	    function = function->data.atom->object;
	case LispAtom_t:
	    name = function;
	    atom = function->data.atom;
	    alist = atom->property->alist;
	    if (atom->a_builtin) {
		xbuiltin = atom->property->fun.builtin;
		macro = xbuiltin->type == LispMacro;
	    }
	    else if (atom->a_compiled)
		bytecode = atom->property->fun.function;
	    else if (atom->a_function) {
		lambda = atom->property->fun.function;
		macro = lambda->funtype == LispMacro;
	    }
	    else if (atom->a_defstruct &&
		     atom->property->structure.function != STRUCT_NAME) {
		if (atom->property->structure.function == STRUCT_CONSTRUCTOR)
		    atom = Omake_struct->data.atom;
		else if (atom->property->structure.function == STRUCT_CHECK)
		    atom = Ostruct_type->data.atom;
		else
		    atom = Ostruct_access->data.atom;
		xbuiltin = atom->property->fun.builtin;
	    }
	    else
		LispDestroy("%s: the function %s is not defined",
			    STRFUN(builtin), STROBJ(function));
	    break;
	case LispBytecode_t:
	    name = Olambda;
	    bytecode = function;
	    break;
	case LispLambda_t:
	    name = Olambda;
	    alist = (LispArgList*)function->data.lambda.name->data.opaque.data;
	    break;
	case LispCons_t:
	    if (CAR(function) == Olambda) {
		function = EVAL(function);
		if (OBJECT_TYPE(function) == LispLambda_t) {
		    name = Olambda;
		    alist = (LispArgList*)
			function->data.lambda.name->data.opaque.data;
		    break;
		}
	    }
	default:
	    LispDestroy("%s: %s is not a function",
			STRFUN(builtin), STROBJ(function));
	    break;
    }

    if (xbuiltin) {
	LispWriteStr(NIL, "Builtin ", 8);
	if (macro)
	    LispWriteStr(NIL, "macro ", 6);
	else
	    LispWriteStr(NIL, "function ", 9);
    }
    else if (macro)
	LispWriteStr(NIL, "Macro ", 6);
    else
	LispWriteStr(NIL, "Function ", 9);
    LispWriteObject(NIL, name);
    LispWriteStr(NIL, ":\n", 2);

    if (alist) {
	int i;

	sprintf(buffer, "%d required argument%s",
		alist->normals.num_symbols,
		alist->normals.num_symbols != 1 ? "s" : "");
	LispWriteStr(NIL, buffer, strlen(buffer));
	for (i = 0; i < alist->normals.num_symbols; i++) {
	    LispWriteChar(NIL, i ? ',' : ':');
	    LispWriteChar(NIL, ' ');
	    LispWriteStr(NIL, ATOMID(alist->normals.symbols[i])->value,
			 ATOMID(alist->normals.symbols[i])->length);
	}
	LispWriteChar(NIL, '\n');

	sprintf(buffer, "%d optional argument%s",
		alist->optionals.num_symbols,
		alist->optionals.num_symbols != 1 ? "s" : "");
	LispWriteStr(NIL, buffer, strlen(buffer));
	for (i = 0; i < alist->optionals.num_symbols; i++) {
	    LispWriteChar(NIL, i ? ',' : ':');
	    LispWriteChar(NIL, ' ');
	    LispWriteStr(NIL, ATOMID(alist->optionals.symbols[i])->value,
			 ATOMID(alist->optionals.symbols[i])->length);
	}
	LispWriteChar(NIL, '\n');

	sprintf(buffer, "%d keyword parameter%s",
		alist->keys.num_symbols,
		alist->keys.num_symbols != 1 ? "s" : "");
	LispWriteStr(NIL, buffer, strlen(buffer));
	for (i = 0; i < alist->keys.num_symbols; i++) {
	    LispWriteChar(NIL, i ? ',' : ':');
	    LispWriteChar(NIL, ' ');
	    LispWriteObject(NIL, alist->keys.symbols[i]);
	}
	LispWriteChar(NIL, '\n');

	if (alist->rest) {
	    LispWriteStr(NIL, "Rest argument: ", 15);
	    LispWriteStr(NIL, ATOMID(alist->rest)->value,
			 ATOMID(alist->rest)->length);
	    LispWriteChar(NIL, '\n');
	}
	else
	    LispWriteStr(NIL, "No rest argument\n", 17);
    }

    if (bytecode) {
	Atom_id id;
	char *ptr;
	int *offsets[4];
	int i, done, j, sym0, sym1, con0, con1, bui0, byt0, strd, strf;
	LispObj **constants;
	LispAtom **symbols;
	LispBuiltin **builtins;
	LispObj **names;
	short stack, num_constants, num_symbols, num_builtins, num_bytecodes;
	unsigned char *base, *stream = bytecode->data.bytecode.bytecode->code;

	LispWriteStr(NIL, "\nBytecode header:\n", 18);

	/* Header information */
	stack = *(short*)stream;
	stream += sizeof(short);
	sprintf(buffer, "%d element%s used in the stack\n",
		stack, stack != 1 ? "s" : "");
	LispWriteStr(NIL, buffer, strlen(buffer));
	stack = *(short*)stream;
	stream += sizeof(short);
	sprintf(buffer, "%d element%s used in the builtin stack\n",
		stack, stack != 1 ? "s" : "");
	LispWriteStr(NIL, buffer, strlen(buffer));
	stack = *(short*)stream;
	stream += sizeof(short);
	sprintf(buffer, "%d element%s used in the protected stack\n",
		stack, stack != 1 ? "s" : "");
	LispWriteStr(NIL, buffer, strlen(buffer));

	num_constants = *(short*)stream;
	stream += sizeof(short);
	num_symbols = *(short*)stream;
	stream += sizeof(short);
	num_builtins = *(short*)stream;
	stream += sizeof(short);
	num_bytecodes = *(short*)stream;
	stream += sizeof(short);

	constants = (LispObj**)stream;
	stream += num_constants * sizeof(LispObj*);
	symbols = (LispAtom**)stream;
	stream += num_symbols * sizeof(LispAtom*);
	builtins = (LispBuiltin**)stream;
	stream += num_builtins * sizeof(LispBuiltin*);
	stream += num_bytecodes * sizeof(unsigned char*);
	names = (LispObj**)stream;
	stream += num_bytecodes * sizeof(LispObj*);

	for (i = 0; i < num_constants; i++) {
	    sprintf(buffer, "Constant %d = %s\n", i, STROBJ(constants[i]));
	    LispWriteStr(NIL, buffer, strlen(buffer));
	}

/* Macro XSTRING avoids some noisy in the output, if it were defined as
 * #define XSTRING(object) object ? STROBJ(object) : #<UNBOUND>
 *	and called as XSTRING(atom->object)
 * it would also print the package name were the symbol was first defined,
 * but for local variables, only the symbol string is important. */
#define XSTRING(key)		key ? key->value : "#<UNBOUND>"

	for (i = 0; i < num_symbols; i++) {
	    sprintf(buffer, "Symbol %d = %s\n",
		    i, XSTRING(symbols[i]->key));
	    LispWriteStr(NIL, buffer, strlen(buffer));
	}
	for (i = 0; i < num_builtins; i++) {
	    sprintf(buffer, "Builtin %d = %s\n",
		    i, STROBJ(builtins[i]->symbol));
	    LispWriteStr(NIL, buffer, strlen(buffer));
	}
	for (i = 0; i < num_bytecodes; i++) {
	    sprintf(buffer, "Bytecode %d = %s\n",
		    i, STROBJ(names[i]));
	    LispWriteStr(NIL, buffer, strlen(buffer));
	}

	/*  Make readability slightly easier printing the names of local
	 * variables where it's offset is known, i.e. function arguments. */
	if (alist) {
	    if (alist->num_arguments == 0)
		LispWriteStr(NIL, "\nNo initial stack\n", 18);
	    else {
		int len1, len2;

		j = 0;
		LispWriteStr(NIL, "\nInitial stack:\n", 16);

		for (i = 0; i < alist->normals.num_symbols; i++, j++) {
		    sprintf(buffer, "%d = ", j);
		    LispWriteStr(NIL, buffer, strlen(buffer));
		    id = alist->normals.symbols[i]->data.atom->key;
		    LispWriteStr(NIL, id->value, id->length);
		    LispWriteChar(NIL, '\n');
		}

		for (i = 0; i < alist->optionals.num_symbols; i++, j++) {
		    sprintf(buffer, "%d = ", j);
		    LispWriteStr(NIL, buffer, strlen(buffer));
		    id = alist->optionals.symbols[i]->data.atom->key;
		    LispWriteStr(NIL, id->value, id->length);
		    LispWriteChar(NIL, '\n');
		    if (alist->optionals.sforms[i]) {
			sprintf(buffer, "%d = ", j);
			len1 = strlen(buffer);
			LispWriteStr(NIL, buffer, len1);
			id = alist->optionals.sforms[i]->data.atom->key;
			len2 = id->length;
			LispWriteStr(NIL, id->value, len2);
			LispWriteChars(NIL, ' ', 28 - (len1 + len2));
			LispWriteStr(NIL, ";  sform\n", 9);
			j++;
		    }
		}

		for (i = 0; i < alist->keys.num_symbols; i++, j++) {
		    sprintf(buffer, "%d = ", j);
		    len1 = strlen(buffer);
		    LispWriteStr(NIL, buffer, len1);
		    if (alist->keys.keys[i]) {
			id = alist->keys.keys[i]->data.atom->key;
			len2 = id->length;
			LispWriteStr(NIL, id->value, id->length);
			LispWriteChars(NIL, ' ', 28 - (len1 + len2));
			LispWriteStr(NIL, ";  special key", 14);
		    }
		    else {
			id = alist->keys.symbols[i]->data.atom->key;
			LispWriteStr(NIL, id->value, id->length);
		    }
		    LispWriteChar(NIL, '\n');
		    if (alist->keys.sforms[i]) {
			sprintf(buffer, "%d = ", j);
			len1 = strlen(buffer);
			LispWriteStr(NIL, buffer, len1);
			id = alist->keys.sforms[i]->data.atom->key;
			len2 = id->length;
			LispWriteStr(NIL, id->value, len2);
			LispWriteChars(NIL, ' ', 28 - (len1 + len2));
			LispWriteStr(NIL, ";  sform\n", 9);
			j++;
		    }
		}

		if (alist->rest) {
		    sprintf(buffer, "%d = ", j);
		    len1 = strlen(buffer);
		    LispWriteStr(NIL, buffer, len1);
		    id = alist->rest->data.atom->key;
		    len2 = id->length;
		    LispWriteStr(NIL, id->value, len2);
		    LispWriteChar(NIL, '\n');
		    j++;
		}

		for (i = 0; i < alist->auxs.num_symbols; i++, j++) {
		    sprintf(buffer, "%d = ", j);
		    len1 = strlen(buffer);
		    LispWriteStr(NIL, buffer, len1);
		    id = alist->auxs.symbols[i]->data.atom->key;
		    len2 = id->length;
		    LispWriteStr(NIL, id->value, len2);
		    LispWriteChars(NIL, ' ', 28 - (len1 + len2));
		    LispWriteStr(NIL, ";  aux\n", 7);
		}
	    }
	}

	LispWriteStr(NIL, "\nBytecode stream:\n", 18);

	base = stream;
	for (done = j = 0; !done; j = 0) {
	    sym0 = sym1 = con0 = con1 = bui0 = byt0 = strd = strf = -1;
	    sprintf(buffer, "%4ld  ", (long)(stream - base));
	    ptr = buffer + strlen(buffer);
	    switch (*stream++) {
		case XBC_NOOP:	strcpy(ptr, "NOOP");	break;
		case XBC_PRED:
		    strcpy(ptr, "PRED:");
		    ptr += strlen(ptr);
		    goto predicate;
		case XBC_INV:	strcpy(ptr, "INV");	break;
		case XBC_NIL:	strcpy(ptr, "NIL");	break;
		case XBC_T:	strcpy(ptr, "T");	break;
		case XBC_CAR:	strcpy(ptr, "CAR");	break;
		case XBC_CDR:	strcpy(ptr, "CDR");	break;
		case XBC_RPLACA:strcpy(ptr, "RPLACA");	break;
		case XBC_RPLACD:strcpy(ptr, "RPLACD");	break;
		case XBC_EQ:	strcpy(ptr, "EQ");	break;
		case XBC_EQL:	strcpy(ptr, "EQL");	break;
		case XBC_EQUAL:	strcpy(ptr, "EQUAL");	break;
		case XBC_EQUALP:strcpy(ptr, "EQUALP");	break;
		case XBC_LENGTH:strcpy(ptr, "LENGTH");	break;
		case XBC_LAST:	strcpy(ptr, "LAST");	break;
		case XBC_NTHCDR:strcpy(ptr, "NTHCDR");	break;
		case XBC_PUSH:	strcpy(ptr, "PUSH");	break;
		case XBC_CAR_PUSH:
		    strcpy(ptr, "CAR&PUSH");
		    break;
		case XBC_CDR_PUSH:
		    strcpy(ptr, "CDR&PUSH");
		    break;
		case XBC_PUSH_NIL:
		    strcpy(ptr, "PUSH NIL");
		    break;
		case XBC_PUSH_UNSPEC:
		    strcpy(ptr, "PUSH #<UNSPEC>");
		    break;
		case XBC_PUSH_T:
		    strcpy(ptr, "PUSH T");
		    break;
		case XBC_PUSH_NIL_N:
		    strcpy(ptr, "PUSH NIL ");
		    ptr += strlen(ptr);
		    sprintf(ptr, "%d", (int)(*stream++));
		    break;
		case XBC_PUSH_UNSPEC_N:
		    strcpy(ptr, "PUSH #<UNSPEC> ");
		    ptr += strlen(ptr);
		    sprintf(ptr, "%d", (int)(*stream++));
		    break;
		case XBC_LET:
		    strcpy(ptr, "LET");
/* update sym0 */
symbol:
		    offsets[j++] = &sym0;
/* update <offsets> - print [byte] */
offset:
		    ptr += strlen(ptr);
		    i = *stream++;
		    *(offsets[j - 1]) = i;
		    sprintf(ptr, " [%d]", i);
		    break;
		case XBC_LETX:
		    strcpy(ptr, "LET*");
		    goto symbol;
		case XBC_LET_NIL:
		    strcpy(ptr, "LET NIL");
		    goto symbol;
		case XBC_LETX_NIL:
		    strcpy(ptr, "LET* NIL");
		    goto symbol;
		case XBC_LETBIND:
		    strcpy(ptr, "LETBIND");
/* print byte */
value:
		    ptr += strlen(ptr);
		    sprintf(ptr, " %d", (int)(*stream++));
		    break;
		case XBC_UNLET:strcpy(ptr, "UNLET");	goto value;
		case XBC_LOAD:
		    strcpy(ptr, "LOAD");
/* print (byte) */
reference:
		    ptr += strlen(ptr);
		    i = *stream++;
		    sprintf(ptr, " (%d)", i);
		    break;
		case XBC_LOAD_CAR:
		    strcpy(ptr, "LOAD&CAR");
		    goto reference;
		case XBC_LOAD_CDR:
		    strcpy(ptr, "LOAD&CDR");
		    goto reference;
		case XBC_LOAD_CAR_STORE:
		    strcpy(ptr, "LOAD&CAR&STORE");
		    goto reference;
		case XBC_LOAD_CDR_STORE:
		    strcpy(ptr, "LOAD&CDR&STORE");
		    goto reference;
		case XBC_LOAD_LET:
		    strcpy(ptr, "LOAD&LET");
load_let:
		    offsets[j++] = &sym0;
		    i = *stream++;
		    ptr += strlen(ptr);
		    sprintf(ptr, " (%d)", i);
		    goto offset;
		case XBC_LOAD_LETX:
		    strcpy(ptr, "LOAD&LET*");
		    goto load_let;
		case XBC_STRUCT:
		    strcpy(ptr, "STRUCT");
		    offsets[j++] = &strf;
		    offsets[j++] = &strd;
/* update <offsets> - print [byte] - update <offsets> - print [byte] */
offset_offset:
		    ptr += strlen(ptr);
		    i = *stream++;
		    *(offsets[j - 2]) = i;
		    sprintf(ptr, " [%d]", i);
		    goto offset;
		case XBC_LOAD_PUSH:
		    strcpy(ptr, "LOAD&PUSH");
		    goto reference;
		case XBC_LOADCON:
		    strcpy(ptr, "LOADCON");
constant:
		    offsets[j++] = &con0;
		    goto offset;
		case XBC_LOADCON_SET:
		    strcpy(ptr, "LOADCON&SET");
		    offsets[j++] = &con0;
/* update <offsets> - print [byte] - print (byte) */
offset_reference:
		    i = *stream++;
		    *(offsets[j - 1]) = i;
		    ptr += strlen(ptr);
		    sprintf(ptr, " [%d]", i);
		    goto reference;
		case XBC_STRUCTP:
		    strcpy(ptr, "STRUCTP");
		    offsets[j++] = &strd;
		    goto offset;
		case XBC_LOADCON_LET:
		    strcpy(ptr, "LOADCON&LET");
loadcon_let:
		    offsets[j++] = &con0;
		    offsets[j++] = &sym0;
		    goto offset_offset;
		case XBC_LOADCON_LETX:
		    strcpy(ptr, "LOADCON&LET*");
		    goto loadcon_let;
		case XBC_LOADCON_PUSH:
		    strcpy(ptr, "LOADCON&PUSH");
		    goto constant;
		case XBC_LOADSYM:
		    strcpy(ptr, "LOADSYM");
		    goto symbol;
		case XBC_LOADSYM_LET:
		    strcpy(ptr, "LOADSYM&LET");
loadsym_let:
		    offsets[j++] = &sym0;
		    offsets[j++] = &sym1;
		    goto offset_offset;
		case XBC_LOADSYM_LETX:
		    strcpy(ptr, "LOADSYM&LET*");
		    goto loadsym_let;
		case XBC_LOADSYM_PUSH:
		    strcpy(ptr, "LOADSYM&PUSH");
		    goto symbol;
		case XBC_LOAD_SET:
		    strcpy(ptr, "LOAD&SET");
/* print (byte) - print (byte) */
reference_reference:
		    ptr += strlen(ptr);
		    i = *stream++;
		    sprintf(ptr, " (%d)", i);
		    goto reference;
		case XBC_LOAD_CAR_SET:
		    strcpy(ptr, "LOAD&CAR&SET");
		    goto reference_reference;
		case XBC_LOAD_CDR_SET:
		    strcpy(ptr, "LOAD&CDR&SET");
		    goto reference_reference;
		case XBC_CAR_SET:
		    strcpy(ptr, "CAR&SET");
		    goto reference;
		case XBC_CDR_SET:
		    strcpy(ptr, "CDR&SET");
		    goto reference;
		case XBC_SET:
		    strcpy(ptr, "SET");
		    goto reference;
		case XBC_SETSYM:
		    strcpy(ptr, "SETSYM");
		    goto symbol;
		case XBC_SET_NIL:
		    strcpy(ptr, "SET NIL");
		    goto reference;
		case XBC_CALL:
		    strcpy(ptr, "CALL");
		    ptr += strlen(ptr);
		    sprintf(ptr, " %d", (int)(*stream++));
		    offsets[j++] = &bui0;
		    goto offset;
		case XBC_CALL_SET:
		    strcpy(ptr, "CALL&SET");
		    ptr += strlen(ptr);
		    sprintf(ptr, " %d", (int)(*stream++));
		    offsets[j++] = &bui0;
		    goto offset_reference;
		case XBC_BYTECALL:
		    strcpy(ptr, "BYTECALL");
		    ptr += strlen(ptr);
		    sprintf(ptr, " %d", (int)(*stream++));
		    offsets[j++] = &byt0;
		    goto offset;
		case XBC_FUNCALL:
		    strcpy(ptr, "FUNCALL");
constant_constant:
		    offsets[j++] = &con0;
		    offsets[j++] = &con1;
		    goto offset_offset;
		case XBC_CCONS:
		    strcpy(ptr, "CCONS");
		    goto constant_constant;
		case XBC_CSTAR:	strcpy(ptr, "CSTAR");	break;
		case XBC_CFINI:	strcpy(ptr, "CFINI");	break;
		case XBC_LSTAR:	strcpy(ptr, "LSTAR");	break;
		case XBC_LCONS:	strcpy(ptr, "LCONS");	break;
		case XBC_LFINI:	strcpy(ptr, "LFINI");	break;
		case XBC_BCONS:	strcpy(ptr, "BCONS");	break;
		case XBC_BCONS1:	case XBC_BCONS2:	case XBC_BCONS3:
		case XBC_BCONS4:	case XBC_BCONS5:	case XBC_BCONS6:
		case XBC_BCONS7:
		    strcpy(ptr, "BCONS");
		    ptr += strlen(ptr);
		    sprintf(ptr, "%d", (int)(stream[-1] - XBC_BCONS));
		    break;
		case XBC_JUMP:
		    strcpy(ptr, "JUMP");
integer:
		    ptr += strlen(ptr);
		    sprintf(ptr, " %d", *(signed short*)stream);
		    stream += sizeof(short);
		    break;
		case XBC_JUMPT:
		    strcpy(ptr, "JUMPT");
		    goto integer;
		case XBC_JUMPNIL:
		    strcpy(ptr, "JUMPNIL");
		    goto integer;
		case XBC_LETREC:
		    strcpy(ptr, "LETREC");
		    ptr += strlen(ptr);
		    sprintf(ptr, " %d", (int)*stream++);
		    break;
		case XBC_RETURN:
		    strcpy(ptr, "RETURN");
		    done = 1;
		    break;
	    }
	    i = ptr - buffer + strlen(ptr);
	    LispWriteStr(NIL, buffer, i);
	    if (j) {

		/* Pad */
		LispWriteChars(NIL, ' ', 28 - i);
		LispWriteChar(NIL, ';');

		ptr = buffer;

		/* Structure */
		if (strf >= 0) {
		    /* strd is valid if strf set */
		    LispObj *fields = constants[strd];

		    for (; strf >= 0; strf--)
			fields = CDR(fields);		    
		    strcpy(ptr, "  ");	    ptr += 2;
		    strcpy(ptr, CAR(fields)->data.atom->key->value);
		    ptr += strlen(ptr);
		}
		if (strd >= 0) {
		    strcpy(ptr, "  ");		ptr += 2;
		    strcpy(ptr, STROBJ(CAR(constants[strd])));
		    ptr += strlen(ptr);
		}

		/* Constants */
		if (con0 >= 0) {
		    strcpy(ptr, "  ");	ptr += 2;
		    strcpy(ptr, STROBJ(constants[con0]));
		    ptr += strlen(ptr);
		    if (con1 >= 0) {
			strcpy(ptr, "  ");	ptr += 2;
			strcpy(ptr, STROBJ(constants[con1]));
			ptr += strlen(ptr);
		    }
		}

		/* Builtin */
		if (bui0 >= 0) {
		    strcpy(ptr, "  ");	ptr += 2;
		    strcpy(ptr, STROBJ(builtins[bui0]->symbol));
		    ptr += strlen(ptr);
		}

		/* Bytecode */
		if (byt0 >= 0) {
		    strcpy(ptr, "  ");	ptr += 2;
		    strcpy(ptr, STROBJ(names[byt0]));
		    ptr += strlen(ptr);
		}

		/* Symbols */
		if (sym0 >= 0) {
		    strcpy(ptr, "  ");	ptr += 2;
		    strcpy(ptr, XSTRING(symbols[sym0]->key));
		    ptr += strlen(ptr);
		    if (sym1 >= 0) {
			strcpy(ptr, "  ");	ptr += 2;
			strcpy(ptr, XSTRING(symbols[sym1]->key));
			ptr += strlen(ptr);
		    }
		}

		i = ptr - buffer;
		LispWriteStr(NIL, buffer, i);
	    }
	    LispWriteChar(NIL, '\n');
	    continue;
predicate:
	    switch (*stream++) {
		case XBP_CONSP:     strcpy(ptr, "CONSP");   break;
		case XBP_LISTP:     strcpy(ptr, "LISTP");   break;
		case XBP_NUMBERP:   strcpy(ptr, "NUMBERP"); break;
	    }
	    LispWriteStr(NIL, buffer, ptr - buffer + strlen(ptr));
	    LispWriteChar(NIL, '\n');
	}
#undef XSTRING
    }

    return (function);
}



LispObj *
LispCompileForm(LispObj *form)
{
    GC_ENTER();
    int failed;
    LispCom com;

    if (!CONSP(form))
	/* Incorrect call or NIL */
	return (form);

    memset(&com, 0, sizeof(LispCom));

    com.toplevel = com.block = LispCalloc(1, sizeof(CodeBlock));
    com.block->type = LispBlockNone;
    com.lex = lisp__data.env.lex;

    com.plist = CONS(NIL, NIL);
    GC_PROTECT(com.plist);

    failed = 1;
    if (setjmp(com.jmp) == 0) {
	for (; CONSP(form); form = CDR(form)) {
	    com.form = form;
	    ComEval(&com, CAR(form));
	}
	failed = 0;
    }
    GC_LEAVE();

    return (failed ? NIL : MakeBytecodeObject(&com, NIL, NIL));
}

LispObj *
LispExecuteBytecode(LispObj *object)
{
    if (!BYTECODEP(object))
	return (EVAL(object));

    return (ExecuteBytecode(object->data.bytecode.bytecode->code));
}

static LispObj *
MakeBytecodeObject(LispCom *com, LispObj *name, LispObj *plist)
{
    LispObj *object;
    LispBytecode *bytecode;

    GC_ENTER();
    unsigned char *stream;
    short i, num_constants;
    LispObj **constants, *code, *cons, *prev;

    /* Resolve dependencies, optimize and create byte stream */
    LinkBytecode(com);

    object = LispNew(NIL, NIL);
    GC_PROTECT(object);
    bytecode = LispMalloc(sizeof(LispBytecode));
    bytecode->code = com->bytecode;
    bytecode->length = com->length;


    stream = bytecode->code;

    /* Skip stack information */
    stream += sizeof(short) * 3;

    /* Get information */
    num_constants = *(short*)stream;
    stream += sizeof(short) * 4;
    constants = (LispObj**)stream;

    GC_PROTECT(plist);
    code = cons = prev = NIL;
    for (i = 0; i < num_constants; i++) {
	if (POINTERP(constants[i]) && !XSYMBOLP(constants[i])) {
	    if (code == NIL) {
		code = cons = prev = CONS(constants[i], NIL);
		GC_PROTECT(code);
	    }
	    else {
		RPLACD(cons, CONS(constants[i], NIL));
		prev = cons;
		cons = CDR(cons);
	    }
	}
    }

    /* Protect this in case the function is redefined */
    for (i = 0; i < com->table.num_bytecodes; i++) {	
	if (code == NIL) {
	    code = cons = prev = CONS(com->table.bytecodes[i], NIL);
	    GC_PROTECT(code);
	}
	else {
	    RPLACD(cons, CONS(com->table.bytecodes[i], NIL));
	    prev = cons;
	    cons = CDR(cons);
	}
    }

    /* Free everything, but the LispCom structure and the generated bytecode */
    CompileFreeState(com);
    
    /* Allocate the minimum required number of cons cells to protect objects */
    if (!CONSP(code))
	code = plist;
    else if (CONSP(plist)) {
	if (code == cons)
	    RPLACD(code, plist);
	else
	    RPLACD(cons, plist);
    }
    else {
	if (code == cons)
	    code = CAR(code);
	else
	    CDR(prev) = CAR(cons);
    }

    object->data.bytecode.bytecode = bytecode;
    /* Byte code references this object, so it cannot be garbage collected */
    object->data.bytecode.code = code;
    object->data.bytecode.name = name;
    object->type = LispBytecode_t;

    LispMused(bytecode);
    LispMused(bytecode->code);
    GC_LEAVE();

    return (object);
}

static void
CompileFreeTree(CodeTree *tree)
{
    if (tree->type == CodeTreeBlock)
	CompileFreeBlock(tree->data.block);
    LispFree(tree);
}

static void
CompileFreeBlock(CodeBlock *block)
{
    CodeTree *tree = block->tree, *next;

    while (tree) {
	next = tree->next;
	CompileFreeTree(tree);
	tree = next;
    }
    if (block->type == LispBlockBody) {
	LispFree(block->tagbody.labels);
	LispFree(block->tagbody.codes);
    }
    LispFree(block->variables.symbols);
    LispFree(block->variables.flags);
    LispFree(block);
}

static void
CompileFreeState(LispCom *com)
{
    CompileFreeBlock(com->block);
    LispFree(com->table.constants);
    LispFree(com->table.symbols);
    LispFree(com->table.builtins);
    LispFree(com->table.bytecodes);
}

/* XXX Put a breakpoint here when changing the macro expansion code.
 *     No opcodes should be generated during macro expansion. */
static CodeTree *
CompileNewTree(LispCom *com, CodeTreeType type)
{
    CodeTree *tree = LispMalloc(sizeof(CodeTree));

    tree->type = type;
    tree->next = NULL;
    tree->block = com->block;
    if (com->block->tree == NULL)
	com->block->tree = tree;
    else
	com->block->tail->next = tree;
    com->block->tail = tree;

    return (tree);
}

static void
CompileIniBlock(LispCom *com, LispBlockType type, LispObj *tag)
{
    CodeTree *tree = NEW_TREE(CodeTreeBlock);
    CodeBlock *block = LispCalloc(1, sizeof(CodeBlock));

    tree->data.block = block;

    block->type = type;
    block->tag = tag;
    block->prev = com->block;
    block->parent = tree;
    block->level = com->level;
    com->block = block;

    if (type == LispBlockBody)
	com->tagbody = com->level;
}

static void
CompileFiniBlock(LispCom *com)
{
    com->block = com->block->prev;
    if (com->block && com->block->type == LispBlockBody)
	com->tagbody = com->block->level;
}

static void
com_BytecodeChar(LispCom *com, LispByteOpcode code, char value)
{
    CodeTree *tree = NEW_TREE(CodeTreeBytecode);

    tree->code = code;
    tree->data.signed_char = value;
}

static void
com_BytecodeShort(LispCom *com, LispByteOpcode code, short value)
{
    CodeTree *tree = NEW_TREE(CodeTreeBytecode);

    tree->code = code;
    tree->data.signed_short = value;
}

static void
com_BytecodeAtom(LispCom *com, LispByteOpcode code, LispAtom *atom)
{
    CodeTree *tree = NEW_TREE(CodeTreeBytecode);

    tree->code = code;
    tree->data.atom = atom;
}

static void
com_BytecodeObject(LispCom *com, LispByteOpcode code, LispObj *object)
{
    CodeTree *tree = NEW_TREE(CodeTreeBytecode);

    tree->code = code;
    tree->data.object = object;
}

static void
com_BytecodeCons(LispCom *com, LispByteOpcode code, LispObj *car, LispObj *cdr)
{
    CodeTree *tree = NEW_TREE(CodeTreeBytecode);

    tree->code = code;
    tree->data.cons.car = car;
    tree->data.cons.cdr = cdr;
}

static void
com_Bytecode(LispCom *com, LispByteOpcode code)
{
    CodeTree *tree = NEW_TREE(CodeTreeBytecode);

    tree->code = code;
}

static void
com_Load(LispCom *com, short offset)
{
    com_BytecodeShort(com, XBC_LOAD, offset);
}

static void
com_LoadLet(LispCom *com, short offset, LispAtom *name)
{
    CodeTree *tree = NEW_TREE(CodeTreeBytecode);

    tree->code = XBC_LOAD_LET;
    tree->data.let.offset = offset;
    tree->data.let.name = name;
}

static void
com_LoadPush(LispCom *com, short offset)
{
    com_BytecodeShort(com, XBC_LOAD_PUSH, offset);
}

static void
com_Let(LispCom *com, LispAtom *name)
{
    com_BytecodeAtom(com, XBC_LET, name);
}

static void
com_Bind(LispCom *com, short count)
{
    if (count)
	com_BytecodeShort(com, XBC_LETBIND, count);
}

static void
com_Unbind(LispCom *com, short count)
{
    if (count)
	com_BytecodeShort(com, XBC_UNLET, count);
}

static void
com_LoadSym(LispCom *com, LispAtom *atom)
{
    com_BytecodeAtom(com, XBC_LOADSYM, atom);
}

static void
com_LoadSymLet(LispCom *com, LispAtom *symbol, LispAtom *name)
{
    CodeTree *tree = NEW_TREE(CodeTreeBytecode);

    tree->code = XBC_LOADSYM_LET;
    tree->data.let_sym.symbol = symbol;
    tree->data.let_sym.name = name;
}

static void
com_LoadSymPush(LispCom *com, LispAtom *name)
{
    com_BytecodeAtom(com, XBC_LOADSYM_PUSH, name);
}

static void
com_LoadCon(LispCom *com, LispObj *constant)
{
    if (constant == NIL)
	com_Bytecode(com, XBC_NIL);
    else if (constant == T)
	com_Bytecode(com, XBC_T);
    else if (constant == UNSPEC) {
	COMPILE_FAILURE("internal error: loading #<UNSPEC>");
    }
    else
	com_BytecodeObject(com, XBC_LOADCON, constant);
}

static void
com_LoadConLet(LispCom *com, LispObj *constant, LispAtom *name)
{
    if (constant == NIL)
	com_BytecodeAtom(com, XBC_LET_NIL, name);
    else {
	CodeTree *tree = NEW_TREE(CodeTreeBytecode);

	tree->code = XBC_LOADCON_LET;
	tree->data.let_con.object = constant;
	tree->data.let_con.name = name;
    }
}

static void
com_LoadConPush(LispCom *com, LispObj *constant)
{
    if (constant == NIL)
	com_Bytecode(com, XBC_PUSH_NIL);
    else if (constant == T)
	com_Bytecode(com, XBC_PUSH_T);
    else if (constant == UNSPEC)
	com_Bytecode(com, XBC_PUSH_UNSPEC);
    else
	com_BytecodeObject(com, XBC_LOADCON_PUSH, constant);
}

static void
com_Set(LispCom *com, short offset)
{
    com_BytecodeShort(com, XBC_SET, offset);
}

static void
com_SetSym(LispCom *com, LispAtom *symbol)
{
    com_BytecodeAtom(com, XBC_SETSYM, symbol);
}

static void
com_Struct(LispCom *com, short offset, LispObj *definition)
{
    CodeTree *tree = NEW_TREE(CodeTreeBytecode);

    tree->code = XBC_STRUCT;
    tree->data.struc.offset = offset;
    tree->data.struc.definition = definition;
}

static void
com_Structp(LispCom *com, LispObj *definition)
{
    com_BytecodeObject(com, XBC_STRUCTP, definition);
}

static void
com_Call(LispCom *com, unsigned char num_arguments, LispBuiltin *builtin)
{
    CodeTree *tree = NEW_TREE(CodeTreeBytecode);

    tree->code = XBC_CALL;
    tree->data.builtin.num_arguments = num_arguments;
    tree->data.builtin.builtin = builtin;
}

static void
com_Bytecall(LispCom *com, unsigned char num_arguments, LispObj *code)
{
    CodeTree *tree = NEW_TREE(CodeTreeBytecode);

    tree->code = XBC_BYTECALL;
    tree->data.bytecall.num_arguments = num_arguments;
    tree->data.bytecall.code = code;
}

static void
com_Funcall(LispCom *com, LispObj *function, LispObj *arguments)
{
    com_BytecodeCons(com, XBC_FUNCALL, function, arguments);
}

static void
CompileStackEnter(LispCom *com, int count, int builtin)
{
    if (!com->macro) {
	if (builtin) {
	    com->stack.cbstack += count;
	    if (com->stack.bstack < com->stack.cbstack)
		com->stack.bstack = com->stack.cbstack;
	}
	else {
	    com->stack.cstack += count;
	    if (com->stack.stack < com->stack.cstack)
		com->stack.stack = com->stack.cstack;
	}
    }
}

static void
CompileStackLeave(LispCom *com, int count, int builtin)
{
    if (!com->macro) {
	if (builtin)
	    com->stack.cbstack -= count;
	else
	    com->stack.cstack -= count;
    }
}

static void
LinkWarnUnused(LispCom *com, CodeBlock *block)
{
    int i;
    CodeTree *tree;

    for (tree = block->tree; tree; tree = tree->next) {
	if (tree->type == CodeTreeBlock)
	    LinkWarnUnused(com, tree->data.block);
    }

    for (i = 0; i < block->variables.length; i++)
	if (!(block->variables.flags[i] & (VARIABLE_USED | VARIABLE_ARGUMENT))) {
	    ++com->warnings;
	    LispWarning("the variable %s is unused",
			block->variables.symbols[i]->key->value);
	}
}

#define	INTERNAL_ERROR_STRING "COMPILE: internal error #%d"
#define	INTERNAL_ERROR(value) LispDestroy(INTERNAL_ERROR_STRING, value)
static long
LinkBuildOffsets(LispCom *com, CodeTree *tree, long offset)
{
    for (; tree; tree = tree->next) {
	tree->offset = offset;
	switch (tree->type) {
	    case CodeTreeBytecode:
		switch (tree->code) {
		    case XBC_NOOP:
			INTERNAL_ERROR(__LINE__);
			break;

		    /* byte */
		    case XBC_BCONS:
		    case XBC_BCONS1:
		    case XBC_BCONS2:
		    case XBC_BCONS3:
		    case XBC_BCONS4:
		    case XBC_BCONS5:
		    case XBC_BCONS6:
		    case XBC_BCONS7:
		    case XBC_INV:
		    case XBC_NIL:
		    case XBC_T:
		    case XBC_PUSH:
		    case XBC_CAR_PUSH:
		    case XBC_CDR_PUSH:
		    case XBC_PUSH_NIL:
		    case XBC_PUSH_UNSPEC:
		    case XBC_PUSH_T:
		    case XBC_LSTAR:
		    case XBC_LCONS:
		    case XBC_LFINI:
		    case XBC_RETURN:
		    case XBC_CSTAR:
		    case XBC_CFINI:
		    case XBC_CAR:
		    case XBC_CDR:
		    case XBC_RPLACA:
		    case XBC_RPLACD:
		    case XBC_EQ:
		    case XBC_EQL:
		    case XBC_EQUAL:
		    case XBC_EQUALP:
		    case XBC_LENGTH:
		    case XBC_LAST:
		    case XBC_NTHCDR:
			++offset;
			break;

		    /* byte + byte */
		    case XBC_PUSH_NIL_N:
		    case XBC_PUSH_UNSPEC_N:
		    case XBC_PRED:
		    case XBC_LETREC:
		    case XBC_LOAD_PUSH:
		    case XBC_CAR_SET:
		    case XBC_CDR_SET:
		    case XBC_SET:
		    case XBC_SET_NIL:
		    case XBC_LETBIND:
		    case XBC_UNLET:
		    case XBC_LOAD:
		    case XBC_LOAD_CAR:
		    case XBC_LOAD_CDR:
		    case XBC_LOAD_CAR_STORE:
		    case XBC_LOAD_CDR_STORE:
		    case XBC_LET:
		    case XBC_LETX:
		    case XBC_LET_NIL:
		    case XBC_LETX_NIL:
		    case XBC_STRUCTP:
		    case XBC_SETSYM:
		    case XBC_LOADCON_PUSH:
		    case XBC_LOADSYM_PUSH:
		    case XBC_LOADCON:
		    case XBC_LOADSYM:
			offset += 2;
			break;

		    /* byte + byte + byte */
		    case XBC_CALL:
		    case XBC_BYTECALL:
		    case XBC_LOAD_SET:
		    case XBC_LOAD_CAR_SET:
		    case XBC_LOAD_CDR_SET:
		    case XBC_LOADCON_SET:
		    case XBC_LOAD_LET:
		    case XBC_LOAD_LETX:
		    case XBC_STRUCT:
		    case XBC_LOADCON_LET:
		    case XBC_LOADCON_LETX:
		    case XBC_LOADSYM_LET:
		    case XBC_LOADSYM_LETX:
		    case XBC_CCONS:
		    case XBC_FUNCALL:
			offset += 3;
			break;

		    /* byte + short */
		    case XBC_JUMP:
		    case XBC_JUMPT:
		    case XBC_JUMPNIL:
			/* XXX this is likely a jump to random address here */
			INTERNAL_ERROR(__LINE__);
			offset += sizeof(short) + 1;
			break;

		    /* byte + byte + byte + byte */
		    case XBC_CALL_SET:
			offset += 4;
			break;
		}
		break;
	    case CodeTreeLabel:
		/* Labels are not loaded */
		break;
	    case CodeTreeJump:
	    case CodeTreeJumpIf:
	    case CodeTreeCond:
		/* If not the point where the conditional block finishes */
		if (tree->code != XBC_NOOP)
		    /* Reserve space for the jump opcode */
		    offset += sizeof(short) + 1;
		break;
	    case CodeTreeGo:
	    case CodeTreeReturn:
		/* Reserve space for the jump opcode */
		offset += sizeof(short) + 1;
		break;
	    case CodeTreeBlock:
		offset = LinkBuildOffsets(com, tree->data.block->tree, offset);
		break;
	}
    }

    return (offset);
}

static void
LinkDoOptimize_0(LispCom *com, CodeBlock *block)
{
    CodeTree *tree, *prev, *next;

    /*  Remove redundant or join opcodes that can be joined. Do it here
     * because some of these are hard to detect earlier, and/or would
     * require a lot of duplicated code or more time. */
    tree = prev = block->tree;
    while (tree) {
	next = tree->next;

	/* LET -> LET* */
	if (next &&
	    next->type == CodeTreeBytecode &&
	    next->code == XBC_LETBIND &&
	    next->data.signed_short == 1) {
	    switch (tree->code) {
		case XBC_LET:
		    tree->code = XBC_LETX;
		    goto remove_next_label;
		case XBC_LET_NIL:
		    tree->code = XBC_LETX_NIL;
		    goto remove_next_label;
		case XBC_LOAD_LET:
		    tree->code = XBC_LOAD_LETX;
		    goto remove_next_label;
		case XBC_LOADCON_LET:
		    tree->code = XBC_LOADCON_LETX;
		    goto remove_next_label;
		case XBC_LOADSYM_LET:
		    tree->code = XBC_LOADSYM_LETX;
		    goto remove_next_label;
		default:
		    break;
	    }
	}

	switch (tree->type) {
	    case CodeTreeBytecode:
		switch (tree->code) {
		    case XBC_LOADCON:
			if (next && next->type == CodeTreeBytecode) {
			    switch (next->code) {
				case XBC_LET:
				    next->code = XBC_LOADCON_LET;
				    next->data.let_con.name =
					next->data.atom;
				    next->data.let_con.object =
					tree->data.object;
				    goto remove_label;
				case XBC_PUSH:
				    next->code = XBC_LOADCON_PUSH;
				    next->data.object = tree->data.object;
				    goto remove_label;
				case XBC_CAR:
				    if (tree->data.object != NIL) {
					if (!CONSP(tree->data.object))
					    LispDestroy("CAR: %s is not a list",
						        STROBJ(
							tree->data.object));
					next->code = XBC_LOADCON;
					next->data.object =
					    CAR(tree->data.object);
				    }
				    goto remove_label;
				case XBC_CDR:
				    if (tree->data.object != NIL) {
					if (!CONSP(tree->data.object))
					    LispDestroy("CAR: %s is not a list",
						        STROBJ(
							tree->data.object));
					next->code = XBC_LOADCON;
					next->data.object =
					    CDR(tree->data.object);
				    }
				    goto remove_label;
				case XBC_SET:
				    next->code = XBC_LOADCON_SET;
				    next->data.load_con_set.offset =
					next->data.signed_short;
				    next->data.load_con_set.object =
					tree->data.object;
				    goto remove_label;
				default:
				    break;
			    }
			}
			break;
		    case XBC_LOADSYM:
			if (next && next->type == CodeTreeBytecode) {
			    switch (next->code) {
				case XBC_LET:
				    next->code = XBC_LOADSYM_LET;
				    next->data.let_sym.name =
					next->data.atom;
				    next->data.let_sym.symbol =
					tree->data.atom;
				    goto remove_label;
				case XBC_PUSH:
				    next->code = XBC_LOADSYM_PUSH;
				    next->data.atom = tree->data.atom;
				    goto remove_label;
				default:
				    break;
			    }
			}
			break;
		    case XBC_LOAD:
			if (next && next->type == CodeTreeBytecode) {
			    switch (next->code) {
				case XBC_SET:
				    next->code = XBC_LOAD_SET;
				    next->data.load_set.set =
					next->data.signed_short;
				    next->data.load_set.load =
					tree->data.signed_short;
				    goto remove_label;
				/* TODO add XBC_LOAD_SETSYM */
				case XBC_CAR:
				    next->code = XBC_LOAD_CAR;
				    next->data.signed_short =
					tree->data.signed_short;
				    goto remove_label;
				case XBC_CDR:
				    next->code = XBC_LOAD_CDR;
				    next->data.signed_short =
					tree->data.signed_short;
				    goto remove_label;
				case XBC_PUSH:
				    tree->code = XBC_LOAD_PUSH;
				    goto remove_next_label;
				case XBC_LET:
				    next->code = XBC_LOAD_LET;
				    next->data.let.name = next->data.atom;
				    next->data.let.offset =
					tree->data.signed_short;
				    goto remove_label;
				default:
				    break;
			    }
			}
			break;
		    case XBC_LOAD_CAR:
			if (next && next->type == CodeTreeBytecode &&
			    next->code == XBC_SET) {
			    if (next->data.signed_short ==
				tree->data.signed_short)
				next->code = XBC_LOAD_CAR_STORE;
			    else {
				next->code = XBC_LOAD_CAR_SET;
				next->data.load_set.set =
				    next->data.signed_short;
				next->data.load_set.load =
				    tree->data.signed_short;
			    }
			    goto remove_label;
			}
			break;
		    case XBC_LOAD_CDR:
			if (next && next->type == CodeTreeBytecode &&
			    next->code == XBC_SET) {
			    if (next->data.signed_short ==
				tree->data.signed_short)
				next->code = XBC_LOAD_CDR_STORE;
			    else {
				next->code = XBC_LOAD_CDR_SET;
				next->data.load_set.set =
				    next->data.signed_short;
				next->data.load_set.load =
				    tree->data.signed_short;
			    }
			    goto remove_label;
			}
			break;
		    case XBC_CALL:
			if (next && next->type == CodeTreeBytecode) {
			    switch (next->code) {
				case XBC_SET:
				    next->code = XBC_CALL_SET;
				    next->data.builtin.offset =
					next->data.signed_short;
				    next->data.builtin.num_arguments =
					tree->data.builtin.num_arguments;
				    next->data.builtin.builtin =
					tree->data.builtin.builtin;
				    goto remove_label;
				/* TODO add XBC_CALL_SETSYM */
				default:
				    break;
			    }
			}
			break;
		    case XBC_CAR:
			if (next && next->type == CodeTreeBytecode) {
			    switch (next->code) {
				case XBC_SET:
				    next->code = XBC_CAR_SET;
				    goto remove_label;
				/* TODO add XBC_CAR_SETSYM */
				case XBC_PUSH:
				    next->code = XBC_CAR_PUSH;
				    goto remove_label;
				default:
				    break;
			    }
			}
			break;
		    case XBC_CDR:
			if (next && next->type == CodeTreeBytecode) {
			    switch (next->code) {
				case XBC_SET:
				    next->code = XBC_CDR_SET;
				    goto remove_label;
				/* TODO add XBC_CDR_SETSYM */
				case XBC_PUSH:
				    next->code = XBC_CDR_PUSH;
				    goto remove_label;
				default:
				    break;
			    }
			}
			break;
		    case XBC_NIL:
			if (next && next->type == CodeTreeBytecode) {
			    switch (next->code) {
				case XBC_SET:
				    next->code = XBC_SET_NIL;
				    goto remove_label;
				/* TODO add XBC_SETSYM_NIL */
				default:
				    break;
			    }
			}
			break;
		    case XBC_PUSH_NIL:
			if (next && next->type == CodeTreeBytecode &&
			    next->code == XBC_PUSH_NIL) {
			    next->code = XBC_PUSH_NIL_N;
			    next->data.signed_char = 2;
			    goto remove_label;
			}
			break;
		    case XBC_PUSH_NIL_N:
			if (next && next->type == CodeTreeBytecode &&
			    next->code == XBC_PUSH_NIL) {
			    next->code = XBC_PUSH_NIL_N;
			    next->data.signed_char = tree->data.signed_char + 1;
			    goto remove_label;
			}
			break;
		    case XBC_PUSH_UNSPEC:
			if (next && next->type == CodeTreeBytecode &&
			    next->code == XBC_PUSH_UNSPEC) {
			    next->code = XBC_PUSH_UNSPEC_N;
			    next->data.signed_char = 2;
			    goto remove_label;
			}
			break;
		    case XBC_PUSH_UNSPEC_N:
			if (next && next->type == CodeTreeBytecode &&
			    next->code == XBC_PUSH_UNSPEC) {
			    next->code = XBC_PUSH_UNSPEC_N;
			    next->data.signed_char = tree->data.signed_char + 1;
			    goto remove_label;
			}
			break;
		    default:
			break;
		}
		break;
	    case CodeTreeBlock:
		LinkDoOptimize_0(com, tree->data.block);
		break;
	    default:
		break;
	}
	goto update_label;
remove_label:
	if (tree == block->tree) {
	    block->tree = prev = next;
	    if (tree == block->tail)
		block->tail = tree;
	}
	else
	    prev->next = next;
	CompileFreeTree(tree);
	tree = next;
	continue;
remove_next_label:
	tree->next = next->next;
	CompileFreeTree(next);
	continue;
update_label:
	prev = tree;
	tree = tree->next;
    }
}

static void
LinkOptimize_0(LispCom *com)
{
    /* Recursive */
    LinkDoOptimize_0(com, com->block);
}

static void
LinkResolveLabels(LispCom *com, CodeBlock *block)
{
    int i;
    CodeTree *tree = block->tree;

    for (; tree; tree = tree->next) {
	if (tree->type == CodeTreeBlock)
	    LinkResolveLabels(com, tree->data.block);
	else if (tree->type == CodeTreeLabel) {
	    for (i = 0; i < block->tagbody.length; i++)
		if (tree->data.object == block->tagbody.labels[i]) {
		    block->tagbody.codes[i] = tree;
		    break;
		}
	}
    }
}

static void
LinkResolveJumps(LispCom *com, CodeBlock *block)
{
    int i;
    CodeBlock *body = block;
    CodeTree *ptr, *tree = block->tree;

    /* Check if there is a tagbody. Error checking already done */
    while (body && body->type != LispBlockBody)
	body = body->prev;

    for (; tree; tree = tree->next) {
	switch (tree->type) {
	    case CodeTreeBytecode:
	    case CodeTreeLabel:
		break;

	    case CodeTreeBlock:
		LinkResolveJumps(com, tree->data.block);
		break;

	    case CodeTreeGo:
		for (i = 0; i < body->tagbody.length; i++)
		    if (tree->data.object == body->tagbody.labels[i])
			break;
		if (i == body->tagbody.length)
		    LispDestroy("COMPILE: no visible tag %s to GO",
				STROBJ(tree->data.object));
		/* Now the jump code is known */
		tree->data.tree = body->tagbody.codes[i];
		break;

	    case CodeTreeCond:
		if (tree->code == XBC_JUMPNIL)
		    /* If test is NIL, go to next test */
		    tree->data.tree = tree->group->next;
		else if (tree->code == XBC_JUMPT) {
		    /* After executing code, test was T */
		    for (ptr = tree->group;
			 ptr->code != XBC_NOOP;
			 ptr = ptr->group)
			;
		    tree->data.tree = ptr;
		}
		break;

	    case CodeTreeJumpIf:
		if (tree->code != XBC_NOOP) {
		    for (ptr = tree->group;
			 ptr->code != XBC_NOOP;
			 ptr = ptr->group) {
			if (ptr->type == CodeTreeJump) {
			    /* ELSE code of IF */
			    ptr = ptr->next;
			    /* Skip inconditional jump node */
			    break;
			}
		    }
		    tree->data.tree = ptr;
		}
		break;

	    case CodeTreeJump:
		if (tree->code != XBC_NOOP)
		    tree->data.tree = tree->group;
		break;

	    case CodeTreeReturn:
		/* One bytecode is guaranteed to exist in the code tree */
		if (tree->data.block->parent == NULL)
		    /* Returning from the function or toplevel form */
		    tree->data.tree = tree->data.block->tail;
		else {
		    for (;;) {
			ptr = tree->data.block->parent->next;
			if (ptr) {
			    tree->data.tree = ptr;
			    break;
			}
			else
			    /* Move one BLOCK up */
			    tree->data.block = tree->data.block->prev;
		    }
		}
		break;
	}
    }
}

static long
LinkPad(long offset, long adjust, int preffix, int datalen)
{
    /* If byte or aligned data */
    if (datalen <= preffix || ((offset + adjust + preffix) % datalen) == 0)
	return (adjust);

    return (adjust + (datalen - ((offset + adjust + preffix) % datalen)));
}

static long
LinkFixupOffsets(LispCom *com, CodeTree *tree, long adjust)
{
    for (; tree; tree = tree->next) {
	switch (tree->type) {
	    case CodeTreeBytecode:
		switch (tree->code) {
		    /* byte + short */
		    case XBC_JUMP:
		    case XBC_JUMPT:
		    case XBC_JUMPNIL:
			adjust = LinkPad(tree->offset, adjust, 1,
					 sizeof(short));
			/*FALLTROUGH*/
		    default:
			tree->offset += adjust;
			break;
		}
		break;
	    case CodeTreeLabel:
		/* Labels are not loaded, just adjust offset */
		tree->offset += adjust;
		break;
	    case CodeTreeJump:
	    case CodeTreeCond:
	    case CodeTreeJumpIf:
		/* If an opcode will be generated. */
		if (tree->code != XBC_NOOP)
		    adjust = LinkPad(tree->offset, adjust, 1, sizeof(short));
		tree->offset += adjust;
		break;
	    case CodeTreeGo:
	    case CodeTreeReturn:
		adjust = LinkPad(tree->offset, adjust, 1, sizeof(short));
		tree->offset += adjust;
		break;
	    case CodeTreeBlock:
		adjust = LinkFixupOffsets(com, tree->data.block->tree, adjust);
		break;
	}
    }

    return (adjust);
}

static void
LinkSkipPadding(LispCom *com, CodeTree *tree)
{
    int found;
    CodeTree *ptr;

    /* Recurse to adjust forward jumps or jumps to the start of the block */
    for (ptr = tree; ptr; ptr = ptr->next) {
	if (ptr->type == CodeTreeBlock) {
	    LinkSkipPadding(com, ptr->data.block->tree);
	    ptr->offset = ptr->data.block->tree->offset;
	}
    }

    /* Adjust the nodes offsets */
    for (; tree; tree = tree->next) {
	switch (tree->type) {
	    case CodeTreeBytecode:
	    case CodeTreeBlock:
	    case CodeTreeGo:
	    case CodeTreeReturn:
		break;
	    case CodeTreeJump:
	    case CodeTreeCond:
	    case CodeTreeJumpIf:
		if (tree->code != XBC_NOOP)
		    /* If code will be generated */
		    break;
	    case CodeTreeLabel:
		/* This should be done in reversed order, but to avoid
		 * the requirement of a prev pointer, do the job in a
		 * harder way here. */
		for (found = 0, ptr = tree->next; ptr; ptr = ptr->next) {
		    switch (ptr->type) {
			case CodeTreeBytecode:
			case CodeTreeBlock:
			case CodeTreeGo:
			case CodeTreeReturn:
			    found = 1;
			    break;
			case CodeTreeJump:
			case CodeTreeCond:
			case CodeTreeJumpIf:
			    if (ptr->code != XBC_NOOP)
				found = 1;
			    break;
			case CodeTreeLabel:
			    break;
		    }
		    if (found)
			break;
		}
		if (found)
		    tree->offset = ptr->offset;
		break;
	}
    }
}

static void
LinkCalculateJump(LispCom *com, CodeTree *tree, LispByteOpcode code)
{
    long jumpto, offset, distance;

    tree->type = CodeTreeBytecode;
    /* After the opcode */
    offset = tree->offset + 1;
    jumpto = tree->data.tree->offset;
    /* Effective distance */
    distance = jumpto - offset;
    tree->code = code;
    if (distance < -32768 || distance > 32767) {
	COMPILE_FAILURE("jump too long");
    }
    tree->data.signed_int = distance;
}

static void
LinkFixupJumps(LispCom *com, CodeTree *tree)
{
    for (; tree; tree = tree->next) {
	switch (tree->type) {
	    case CodeTreeBytecode:
	    case CodeTreeLabel:
		break;
	    case CodeTreeCond:
		if (tree->code == XBC_JUMPNIL)
		    /* Go to next test if NIL */
		    LinkCalculateJump(com, tree, XBC_JUMPNIL);
		else if (tree->code == XBC_JUMPT)
		    /* After executing T code */
		    LinkCalculateJump(com, tree, XBC_JUMP);
		break;
	    case CodeTreeJumpIf:
		if (tree->code != XBC_NOOP)
		    LinkCalculateJump(com, tree, tree->code);
		break;
	    case CodeTreeGo:
		/* Inconditional jump */
		LinkCalculateJump(com, tree, XBC_JUMP);
		break;
	    case CodeTreeReturn:
		/* Inconditional jump */
		if (tree->data.tree != tree)
		    /* If need to skip something */
		    LinkCalculateJump(com, tree, XBC_JUMP);
		break;
	    case CodeTreeBlock:
		LinkFixupJumps(com, tree->data.block->tree);
		break;
	    case CodeTreeJump:
		if (tree->code != XBC_NOOP)
		    LinkCalculateJump(com, tree, tree->code);
	}
    }
}

static void
LinkBuildTableSymbol(LispCom *com, LispAtom *symbol)
{
    if (BuildTablePointer(symbol, (void***)&com->table.symbols,
			  &com->table.num_symbols) > 0xff) {
	COMPILE_FAILURE("more than 256 symbols");
    }
}

static void
LinkBuildTableConstant(LispCom *com, LispObj *constant)
{
    if (BuildTablePointer(constant, (void***)&com->table.constants,
			  &com->table.num_constants) > 0xff) {
	COMPILE_FAILURE("more than 256 constants");
    }
}

static void
LinkBuildTableBuiltin(LispCom *com, LispBuiltin *builtin)
{
    if (BuildTablePointer(builtin, (void***)&com->table.builtins,
			  &com->table.num_builtins) > 0xff) {
	COMPILE_FAILURE("more than 256 functions");
    }
}

static void
LinkBuildTableBytecode(LispCom *com, LispObj *bytecode)
{
    if (BuildTablePointer(bytecode, (void***)&com->table.bytecodes,
			  &com->table.num_bytecodes) > 0xff) {
	COMPILE_FAILURE("more than 256 bytecode functions");
    }
}

static void
LinkBuildTables(LispCom *com, CodeBlock *block)
{
    CodeTree *tree;

    for (tree = block->tree; tree; tree = tree->next) {
	switch (tree->type) {
	    case CodeTreeBytecode:
		switch (tree->code) {
		    case XBC_LET:
		    case XBC_LETX:
		    case XBC_LET_NIL:
		    case XBC_LETX_NIL:
		    case XBC_SETSYM:
		    case XBC_LOADSYM:
		    case XBC_LOADSYM_PUSH:
			LinkBuildTableSymbol(com, tree->data.atom);
			break;
		    case XBC_STRUCTP:
		    case XBC_LOADCON:
		    case XBC_LOADCON_PUSH:
			LinkBuildTableConstant(com, tree->data.object);
			break;
		    case XBC_LOADCON_SET:
			LinkBuildTableConstant(com, tree->data.load_con_set.object);
			break;
		    case XBC_CALL:
		    case XBC_CALL_SET:
			LinkBuildTableBuiltin(com, tree->data.builtin.builtin);
			break;
		    case XBC_BYTECALL:
			LinkBuildTableBytecode(com, tree->data.bytecall.code);
			break;
		    case XBC_LOAD_LET:
		    case XBC_LOAD_LETX:
			LinkBuildTableSymbol(com, tree->data.let.name);
			break;
		    case XBC_STRUCT:
			LinkBuildTableConstant(com, tree->data.struc.definition);
			break;
		    case XBC_LOADSYM_LET:
		    case XBC_LOADSYM_LETX:
			LinkBuildTableSymbol(com, tree->data.let_sym.symbol);
			LinkBuildTableSymbol(com, tree->data.let_sym.name);
			break;
		    case XBC_LOADCON_LET:
		    case XBC_LOADCON_LETX:
			LinkBuildTableConstant(com, tree->data.let_con.object);
			LinkBuildTableSymbol(com, tree->data.let_con.name);
			break;
		    case XBC_CCONS:
		    case XBC_FUNCALL:
			LinkBuildTableConstant(com, tree->data.cons.car);
			LinkBuildTableConstant(com, tree->data.cons.cdr);
			break;
		    default:
			break;
		}
		break;
	    case CodeTreeBlock:
		LinkBuildTables(com, tree->data.block);
		break;
	    default:
		break;
	}
    }
}

static long
LinkEmmitBytecode(LispCom *com, CodeTree *tree,
		  unsigned char *bytecode, long offset)
{
    short i;

    for (; tree; tree = tree->next) {
	/* Fill padding */
	while (offset < tree->offset)
	    bytecode[offset++] = XBC_NOOP;

	switch (tree->type) {
	    case CodeTreeBytecode:
		bytecode[offset++] = tree->code;
		switch (tree->code) {
		    /* Noop should not enter the CodeTree */
		    case XBC_NOOP:
			INTERNAL_ERROR(__LINE__);
			break;

		    /* byte */
		    case XBC_BCONS:
		    case XBC_BCONS1:
		    case XBC_BCONS2:
		    case XBC_BCONS3:
		    case XBC_BCONS4:
		    case XBC_BCONS5:
		    case XBC_BCONS6:
		    case XBC_BCONS7:
		    case XBC_INV:
		    case XBC_NIL:
		    case XBC_T:
		    case XBC_PUSH_NIL:
		    case XBC_PUSH_UNSPEC:
		    case XBC_PUSH_T:
		    case XBC_CAR_PUSH:
		    case XBC_CDR_PUSH:
		    case XBC_PUSH:
		    case XBC_LSTAR:
		    case XBC_LCONS:
		    case XBC_LFINI:
		    case XBC_RETURN:
		    case XBC_CSTAR:
		    case XBC_CFINI:
		    case XBC_CAR:
		    case XBC_CDR:
		    case XBC_RPLACA:
		    case XBC_RPLACD:
		    case XBC_EQ:
		    case XBC_EQL:
		    case XBC_EQUAL:
		    case XBC_EQUALP:
		    case XBC_LENGTH:
		    case XBC_LAST:
		    case XBC_NTHCDR:
			break;

		    /* byte + byte */
		    case XBC_LETREC:
		    case XBC_PRED:
		    case XBC_PUSH_NIL_N:
		    case XBC_PUSH_UNSPEC_N:
			bytecode[offset++] = tree->data.signed_char;
			break;

		    /* byte + byte */
		    case XBC_CAR_SET:
		    case XBC_CDR_SET:
		    case XBC_SET:
		    case XBC_SET_NIL:
		    case XBC_LETBIND:
		    case XBC_UNLET:
		    case XBC_LOAD_PUSH:
		    case XBC_LOAD:
		    case XBC_LOAD_CAR:
		    case XBC_LOAD_CDR:
		    case XBC_LOAD_CAR_STORE:
		    case XBC_LOAD_CDR_STORE:
			bytecode[offset++] = tree->data.signed_short;
			break;

		    /* byte + byte + byte */
		    case XBC_LOAD_SET:
		    case XBC_LOAD_CAR_SET:
		    case XBC_LOAD_CDR_SET:
			bytecode[offset++] = tree->data.load_set.load;
			bytecode[offset++] = tree->data.load_set.set;
			break;

		    /* byte + short */
		    case XBC_JUMP:
		    case XBC_JUMPT:
		    case XBC_JUMPNIL:
			*(short*)(bytecode + offset) = tree->data.signed_int;
			offset += sizeof(short);
			break;

		    /* byte + byte */
		    case XBC_LET:
		    case XBC_LETX:
		    case XBC_LET_NIL:
		    case XBC_LETX_NIL:
		    case XBC_SETSYM:
		    case XBC_LOADSYM:
		    case XBC_LOADSYM_PUSH:
			i = FindIndex(tree->data.atom,
				      (void**)com->table.symbols,
				      com->table.num_symbols);
			bytecode[offset++] = i;
			break;

		    /* byte + byte */
		    case XBC_STRUCTP:
		    case XBC_LOADCON:
		    case XBC_LOADCON_PUSH:
			i = FindIndex(tree->data.object,
				      (void**)com->table.constants,
				      com->table.num_constants);
			bytecode[offset++] = i;
			break;

		    /* byte + byte + byte */
		    case XBC_LOADCON_SET:
			i = FindIndex(tree->data.load_con_set.object,
				      (void**)com->table.constants,
				      com->table.num_constants);
			bytecode[offset++] = i;
			bytecode[offset++] = tree->data.load_con_set.offset;
			break;

		    /* byte + byte + byte */
		    case XBC_CALL:
			bytecode[offset++] = tree->data.builtin.num_arguments;
			i = FindIndex(tree->data.builtin.builtin,
				      (void**)com->table.builtins,
				      com->table.num_builtins);
			bytecode[offset++] = i;
			break;

		    /* byte + byte + byte */
		    case XBC_BYTECALL:
			bytecode[offset++] = tree->data.bytecall.num_arguments;
			i = FindIndex(tree->data.bytecall.code,
				      (void**)com->table.bytecodes,
				      com->table.num_bytecodes);
			bytecode[offset++] = i;
			break;

		    /* byte + byte + byte + byte */
		    case XBC_CALL_SET:
			bytecode[offset++] = tree->data.builtin.num_arguments;
			i = FindIndex(tree->data.builtin.builtin,
				      (void**)com->table.builtins,
				      com->table.num_builtins);
			bytecode[offset++] = i;
			bytecode[offset++] = tree->data.builtin.offset;
			break;

		    /* byte + byte + byte */
		    case XBC_LOAD_LET:
		    case XBC_LOAD_LETX:
			bytecode[offset++] = tree->data.let.offset;
			i = FindIndex(tree->data.let.name,
				      (void**)com->table.symbols,
				      com->table.num_symbols);
			bytecode[offset++] = i;
			break;

		    /* byte + byte + byte */
		    case XBC_STRUCT:
			bytecode[offset++] = tree->data.struc.offset;
			i = FindIndex(tree->data.struc.definition,
				      (void**)com->table.constants,
				      com->table.num_constants);
			bytecode[offset++] = i;
			break;

		    /* byte + byte + byte */
		    case XBC_LOADSYM_LET:
		    case XBC_LOADSYM_LETX:
			i = FindIndex(tree->data.let_sym.symbol,
				      (void**)com->table.symbols,
				      com->table.num_symbols);
			bytecode[offset++] = i;
			i = FindIndex(tree->data.let_sym.name,
				      (void**)com->table.symbols,
				      com->table.num_symbols);
			bytecode[offset++] = i;
			break;

		    /* byte + byte + byte */
		    case XBC_LOADCON_LET:
		    case XBC_LOADCON_LETX:
			i = FindIndex(tree->data.let_con.object,
				      (void**)com->table.constants,
				      com->table.num_constants);
			bytecode[offset++] = i;
			i = FindIndex(tree->data.let_con.name,
				      (void**)com->table.symbols,
				      com->table.num_symbols);
			bytecode[offset++] = i;
			break;

		    /* byte + byte + byte */
		    case XBC_CCONS:
		    case XBC_FUNCALL:
			i = FindIndex(tree->data.cons.car,
				      (void**)com->table.constants,
				      com->table.num_constants);
			bytecode[offset++] = i;
			i = FindIndex(tree->data.cons.cdr,
				      (void**)com->table.constants,
				      com->table.num_constants);
			bytecode[offset++] = i;
			break;
		}
		break;
	    case CodeTreeLabel:
		/* Labels are not loaded */
		break;
	    case CodeTreeCond:
	    case CodeTreeJump:
	    case CodeTreeJumpIf:
		if (tree->code != XBC_NOOP)
		    INTERNAL_ERROR(__LINE__);
		break;
	    case CodeTreeGo:
		INTERNAL_ERROR(__LINE__);
		break;
	    case CodeTreeReturn:
		if (tree->data.tree != tree)
		    INTERNAL_ERROR(__LINE__);
		break;
	    case CodeTreeBlock:
		offset = LinkEmmitBytecode(com, tree->data.block->tree,
					   bytecode, offset);
		break;
	}
    }

    return (offset);
}

static void
LinkBytecode(LispCom *com)
{
    long offset, count;
    unsigned char **codes;
    LispObj **names;

    /* Close bytecode */
    com_Bytecode(com, XBC_RETURN);

    /* The only usage of this information for now, and still may generate
     * false positives because arguments to unamed functions are not being
     * parsed as well as arguments to yet undefined function/maros.
     * XXX should also add declaim/declare to let the code specify that
     * the argument is unused */
    LinkWarnUnused(com, com->block);

    /* First level optimization */
    LinkOptimize_0(com);

    /* Resolve tagbody labels */
    LinkResolveLabels(com, com->block);

    /* Resolve any pending jumps */
    LinkResolveJumps(com, com->block);

    /* Calculate unpadded offsets */
    LinkBuildOffsets(com, com->block->tree, 0);

    /* Do padding for aligned memory reads */
    LinkFixupOffsets(com, com->block->tree, 0);

    /* Jumps normally are to a node that does not generate code,
     * and due to padding, the jump may go to a address with a
     * XBC_NOOP, so adjust the jump to the next useful opcode. */
    LinkSkipPadding(com, com->block->tree);

    /* Now addresses are known */
    LinkFixupJumps(com, com->block->tree);

    /* Build symbol, constant and builtin tables */
    LinkBuildTables(com, com->block);

    /* Stack info */
    com->length = sizeof(short) * 3;
    /* Tables info */
    com->length += sizeof(short) * 4;
    com->length += com->table.num_constants * sizeof(LispObj*);
    com->length += com->table.num_symbols * sizeof(LispAtom*);
    com->length += com->table.num_builtins * sizeof(LispBuiltin*);
    com->length += com->table.num_bytecodes * sizeof(unsigned char*);
    com->length += com->table.num_bytecodes * sizeof(LispObj*);

    /* Allocate space for the bytecode stream */
    com->length += com->block->tail->offset + 1;
    com->bytecode = LispMalloc(com->length);

    /* Add header */
    offset = 0;
    *(short*)(com->bytecode + offset) = com->stack.stack;
    offset += sizeof(short);
    *(short*)(com->bytecode + offset) = com->stack.bstack;
    offset += sizeof(short);
    *(short*)(com->bytecode + offset) = com->stack.pstack;
    offset += sizeof(short);

    *(short*)(com->bytecode + offset) = com->table.num_constants;
    offset += sizeof(short);
    *(short*)(com->bytecode + offset) = com->table.num_symbols;
    offset += sizeof(short);
    *(short*)(com->bytecode + offset) = com->table.num_builtins;
    offset += sizeof(short);
    *(short*)(com->bytecode + offset) = com->table.num_bytecodes;
    offset += sizeof(short);

    count = sizeof(LispObj*) * com->table.num_constants;
    memcpy(com->bytecode + offset, com->table.constants, count);
    offset += count;
    count = sizeof(LispAtom*) * com->table.num_symbols;
    memcpy(com->bytecode + offset, com->table.symbols, count);
    offset += count;
    count = sizeof(LispBuiltin*) * com->table.num_builtins;
    memcpy(com->bytecode + offset, com->table.builtins, count);
    offset += count;

    /* Store bytecode information */
    for (count = 0, codes = (unsigned char**)(com->bytecode + offset);
	 count < com->table.num_bytecodes; count++, codes++)
	*codes = com->table.bytecodes[count]->data.bytecode.bytecode->code;
    offset += com->table.num_bytecodes * sizeof(unsigned char*);
    /* Store names, only useful for disassemble but may also be used
     * to check if a function was redefined, and the bytecode is referencing
     * the older version, the current version can be checked looking at
     * <name>->data.atom */
    for (count = 0, names = (LispObj**)(com->bytecode + offset);
	 count < com->table.num_bytecodes; count++, names++)
	*names = com->table.bytecodes[count]->data.bytecode.name;
    offset += com->table.num_bytecodes * sizeof(LispObj*);

    /* Generate it */
    LinkEmmitBytecode(com, com->block->tree, com->bytecode + offset, 0);
}

static LispObj *
ExecuteBytecode(register unsigned char *stream)
{
    register LispObj *reg0;
    register LispAtom *atom;
    register short offset;
    LispObj *reg1;
    LispBuiltin *builtin;
    LispObj *lambda;
    LispObj *arguments;
    unsigned char *bytecode;

    LispObj **constants;
    LispAtom **symbols;
    LispBuiltin **builtins;
    unsigned char **codes;
    short num_constants, num_symbols, num_builtins, num_codes;

    int lex, len;

    /* To control gc protected slots */
    int phead, pbase;

    long fixnum = 0;

#if defined(__GNUC__) && !defined(ANSI_SOURCE)
#define ALLOW_GOTO_ADDRESS
#endif

#ifdef ALLOW_GOTO_ADDRESS
#define JUMP_ADDRESS(label)	&&label
    static const void *opcode_labels[] = {
	JUMP_ADDRESS(XBC_NOOP),
	JUMP_ADDRESS(XBC_INV),
	JUMP_ADDRESS(XBC_NIL),
	JUMP_ADDRESS(XBC_T),
	JUMP_ADDRESS(XBC_PRED),
	JUMP_ADDRESS(XBC_CAR),
	JUMP_ADDRESS(XBC_CDR),
	JUMP_ADDRESS(XBC_CAR_SET),
	JUMP_ADDRESS(XBC_CDR_SET),
	JUMP_ADDRESS(XBC_RPLACA),
	JUMP_ADDRESS(XBC_RPLACD),
	JUMP_ADDRESS(XBC_EQ),
	JUMP_ADDRESS(XBC_EQL),
	JUMP_ADDRESS(XBC_EQUAL),
	JUMP_ADDRESS(XBC_EQUALP),
	JUMP_ADDRESS(XBC_LENGTH),
	JUMP_ADDRESS(XBC_LAST),
	JUMP_ADDRESS(XBC_NTHCDR),
	JUMP_ADDRESS(XBC_CAR_PUSH),
	JUMP_ADDRESS(XBC_CDR_PUSH),
	JUMP_ADDRESS(XBC_PUSH),
	JUMP_ADDRESS(XBC_PUSH_NIL),
	JUMP_ADDRESS(XBC_PUSH_UNSPEC),
	JUMP_ADDRESS(XBC_PUSH_T),
	JUMP_ADDRESS(XBC_PUSH_NIL_N),
	JUMP_ADDRESS(XBC_PUSH_UNSPEC_N),
	JUMP_ADDRESS(XBC_LET),
	JUMP_ADDRESS(XBC_LETX),
	JUMP_ADDRESS(XBC_LET_NIL),
	JUMP_ADDRESS(XBC_LETX_NIL),
	JUMP_ADDRESS(XBC_LETBIND),
	JUMP_ADDRESS(XBC_UNLET),
	JUMP_ADDRESS(XBC_LOAD),
	JUMP_ADDRESS(XBC_LOAD_LET),
	JUMP_ADDRESS(XBC_LOAD_LETX),
	JUMP_ADDRESS(XBC_LOAD_PUSH),
	JUMP_ADDRESS(XBC_LOADCON),
	JUMP_ADDRESS(XBC_LOADCON_LET),
	JUMP_ADDRESS(XBC_LOADCON_LETX),
	JUMP_ADDRESS(XBC_LOADCON_PUSH),
	JUMP_ADDRESS(XBC_LOAD_CAR),
	JUMP_ADDRESS(XBC_LOAD_CDR),
	JUMP_ADDRESS(XBC_LOAD_CAR_STORE),
	JUMP_ADDRESS(XBC_LOAD_CDR_STORE),
	JUMP_ADDRESS(XBC_LOADCON_SET),
	JUMP_ADDRESS(XBC_LOADSYM),
	JUMP_ADDRESS(XBC_LOADSYM_LET),
	JUMP_ADDRESS(XBC_LOADSYM_LETX),
	JUMP_ADDRESS(XBC_LOADSYM_PUSH),
	JUMP_ADDRESS(XBC_LOAD_SET),
	JUMP_ADDRESS(XBC_LOAD_CAR_SET),
	JUMP_ADDRESS(XBC_LOAD_CDR_SET),
	JUMP_ADDRESS(XBC_SET),
	JUMP_ADDRESS(XBC_SETSYM),
	JUMP_ADDRESS(XBC_SET_NIL),
	JUMP_ADDRESS(XBC_CALL),
	JUMP_ADDRESS(XBC_CALL_SET),
	JUMP_ADDRESS(XBC_BYTECALL),
	JUMP_ADDRESS(XBC_FUNCALL),
	JUMP_ADDRESS(XBC_LETREC),
	JUMP_ADDRESS(XBC_BCONS),
	JUMP_ADDRESS(XBC_BCONS1),
	JUMP_ADDRESS(XBC_BCONS2),
	JUMP_ADDRESS(XBC_BCONS3),
	JUMP_ADDRESS(XBC_BCONS4),
	JUMP_ADDRESS(XBC_BCONS5),
	JUMP_ADDRESS(XBC_BCONS6),
	JUMP_ADDRESS(XBC_BCONS7),
	JUMP_ADDRESS(XBC_CCONS),
	JUMP_ADDRESS(XBC_CSTAR),
	JUMP_ADDRESS(XBC_CFINI),
	JUMP_ADDRESS(XBC_LSTAR),
	JUMP_ADDRESS(XBC_LCONS),
	JUMP_ADDRESS(XBC_LFINI),
	JUMP_ADDRESS(XBC_JUMP),
	JUMP_ADDRESS(XBC_JUMPT),
	JUMP_ADDRESS(XBC_JUMPNIL),
	JUMP_ADDRESS(XBC_STRUCT),
	JUMP_ADDRESS(XBC_STRUCTP),
	JUMP_ADDRESS(XBC_RETURN)
    };
    static const void *predicate_opcode_labels[] = {
	JUMP_ADDRESS(XBP_CONSP),
	JUMP_ADDRESS(XBP_LISTP),
	JUMP_ADDRESS(XBP_NUMBERP)
    };
#endif

    reg0 = NIL;

    bytecode = stream;
    pbase = lisp__data.protect.length;

    /* stack */
    offset = *(short*)stream;
    stream += sizeof(short);
    if (lisp__data.env.length + offset > lisp__data.env.space) {
	do
	    LispMoreEnvironment();
	while (lisp__data.env.length + offset >= lisp__data.env.space);
    }
    /* builtin stack */
    offset = *(short*)stream;
    stream += sizeof(short);
    if (lisp__data.stack.length + offset >= lisp__data.stack.space) {
	do
	    LispMoreStack();
	while (lisp__data.stack.length + offset >= lisp__data.stack.space);
    }
    /* protect stack */
    phead = *(short*)stream;
    stream += sizeof(short);
    if (lisp__data.protect.length + phead > lisp__data.protect.space) {
	do
	    LispMoreProtects();
	while (lisp__data.protect.length + phead >= lisp__data.protect.space);
    }

    num_constants = *(short*)stream;
    stream += sizeof(short);
    num_symbols = *(short*)stream;
    stream += sizeof(short);
    num_builtins = *(short*)stream;
    stream += sizeof(short);
    num_codes = *(short*)stream;
    stream += sizeof(short);

    constants = (LispObj**)stream;
    stream += num_constants * sizeof(LispObj*);
    symbols = (LispAtom**)stream;
    stream += num_symbols * sizeof(LispAtom*);
    builtins = (LispBuiltin**)stream;
    stream += num_builtins * sizeof(LispBuiltin*);
    codes = (unsigned char**)stream;
    stream += num_codes * (sizeof(unsigned char*) + sizeof(LispObj*));

    for (; phead > 0; phead--)
	lisp__data.protect.objects[lisp__data.protect.length++] = NIL;
    phead = pbase;

#ifdef ALLOW_GOTO_ADDRESS
#define OPCODE_LABEL(label)	label
#define NEXT_OPCODE()		goto *opcode_labels[*stream++]
#define GOTO_PREDICATE()	goto *predicate_opcode_labels[*stream++]
#else
#define OPCODE_LABEL(label)	case label
#define NEXT_OPCODE()		goto next_opcode
#define GOTO_PREDICATE()	goto predicate_label
    for (;;) {
next_opcode:
	switch (*stream++) {
#endif	/* ALLOW_GOTO_ADDRESS */

OPCODE_LABEL(XBC_NOOP):
	NEXT_OPCODE();

OPCODE_LABEL(XBC_PRED):
	GOTO_PREDICATE();

OPCODE_LABEL(XBC_INV):
	reg0 = reg0 == NIL ? T : NIL;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_NIL):
	reg0 = NIL;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_T):
	reg0 = T;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_CAR):
car:
	if (reg0 != NIL) {
	    if (!CONSP(reg0))
		LispDestroy("CAR: %s is not a list", STROBJ(reg0));
	    reg0 = CAR(reg0);
	}
	NEXT_OPCODE();

OPCODE_LABEL(XBC_CDR):
cdr:
	if (reg0 != NIL) {
	    if (!CONSP(reg0))
		LispDestroy("CDR: %s is not a list", STROBJ(reg0));
	    reg0 = CDR(reg0);
	}
	NEXT_OPCODE();

OPCODE_LABEL(XBC_RPLACA):
	reg1 = lisp__data.stack.values[--lisp__data.stack.length];
	if (!CONSP(reg1))
	    LispDestroy("RPLACA: %s is not a cons", STROBJ(reg1));
	RPLACA(reg1, reg0);
	reg0 = reg1;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_RPLACD):
	reg1 = lisp__data.stack.values[--lisp__data.stack.length];
	if (!CONSP(reg1))
	    LispDestroy("RPLACD: %s is not a cons", STROBJ(reg1));
	RPLACD(reg1, reg0);
	reg0 = reg1;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_BCONS):
	CAR(cons) = reg0;
	lisp__data.stack.values[lisp__data.stack.length++] = cons;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_BCONS1):
	offset = lisp__data.stack.length - 1;
	CAR(cons) = reg0;
	CAR(cons1) = lisp__data.stack.values[offset];
	lisp__data.stack.values[offset] = cons1;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_BCONS2):
	offset = lisp__data.stack.length;
	CAR(cons) = reg0;
	CAR(cons1) = lisp__data.stack.values[--offset];
	CAR(cons2) = lisp__data.stack.values[--offset];
	lisp__data.stack.values[offset] = cons2;
	lisp__data.stack.length = offset + 1;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_BCONS3):
	offset = lisp__data.stack.length;
	CAR(cons) = reg0;
	CAR(cons1) = lisp__data.stack.values[--offset];
	CAR(cons2) = lisp__data.stack.values[--offset];
	CAR(cons3) = lisp__data.stack.values[--offset];
	lisp__data.stack.values[offset] = cons3;
	lisp__data.stack.length = offset + 1;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_BCONS4):
	offset = lisp__data.stack.length;
	CAR(cons) = reg0;
	CAR(cons1) = lisp__data.stack.values[--offset];
	CAR(cons2) = lisp__data.stack.values[--offset];
	CAR(cons3) = lisp__data.stack.values[--offset];
	CAR(cons4) = lisp__data.stack.values[--offset];
	lisp__data.stack.values[offset] = cons4;
	lisp__data.stack.length = offset + 1;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_BCONS5):
	offset = lisp__data.stack.length;
	CAR(cons) = reg0;
	CAR(cons1) = lisp__data.stack.values[--offset];
	CAR(cons2) = lisp__data.stack.values[--offset];
	CAR(cons3) = lisp__data.stack.values[--offset];
	CAR(cons4) = lisp__data.stack.values[--offset];
	CAR(cons5) = lisp__data.stack.values[--offset];
	lisp__data.stack.values[offset] = cons5;
	lisp__data.stack.length = offset + 1;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_BCONS6):
	offset = lisp__data.stack.length;
	CAR(cons) = reg0;
	CAR(cons1) = lisp__data.stack.values[--offset];
	CAR(cons2) = lisp__data.stack.values[--offset];
	CAR(cons3) = lisp__data.stack.values[--offset];
	CAR(cons4) = lisp__data.stack.values[--offset];
	CAR(cons5) = lisp__data.stack.values[--offset];
	CAR(cons6) = lisp__data.stack.values[--offset];
	lisp__data.stack.values[offset] = cons6;
	lisp__data.stack.length = offset + 1;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_BCONS7):
	offset = lisp__data.stack.length;
	CAR(cons) = reg0;
	CAR(cons1) = lisp__data.stack.values[--offset];
	CAR(cons2) = lisp__data.stack.values[--offset];
	CAR(cons3) = lisp__data.stack.values[--offset];
	CAR(cons4) = lisp__data.stack.values[--offset];
	CAR(cons5) = lisp__data.stack.values[--offset];
	CAR(cons6) = lisp__data.stack.values[--offset];
	CAR(cons7) = lisp__data.stack.values[--offset];
	lisp__data.stack.values[offset] = cons7;
	lisp__data.stack.length = offset + 1;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_EQ):
	reg0 = reg0 == lisp__data.stack.values[--lisp__data.stack.length] ? T : NIL;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_EQL):
	reg1 = lisp__data.stack.values[--lisp__data.stack.length];
	reg0 = XEQL(reg1, reg0);
	NEXT_OPCODE();

OPCODE_LABEL(XBC_EQUAL):
	reg1 = lisp__data.stack.values[--lisp__data.stack.length];
	reg0 = XEQUAL(reg1, reg0);
	NEXT_OPCODE();

OPCODE_LABEL(XBC_EQUALP):
	reg1 = lisp__data.stack.values[--lisp__data.stack.length];
	reg0 = XEQUALP(reg1, reg0);
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LENGTH):
	reg0 = FIXNUM(LispLength(reg0));
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LAST):
    {
	long length;

	reg1 = lisp__data.stack.values[--lisp__data.stack.length];
	if (CONSP(reg1)) {
	    if (reg0 != NIL) {
		if (!FIXNUMP(reg0) || (fixnum = FIXNUM_VALUE(reg0)) < 0)
		    LispDestroy("LAST: %s is not a positive fixnum",
				STROBJ(reg0));
	    }
	    else
		fixnum = 1;
	    reg0 = reg1;
	    for (reg0 = reg1, length = 0;
		 CONSP(reg0);
		 reg0 = CDR(reg0), length++)
		;
	    for (length -= fixnum, reg0 = reg1; length > 0; length--)
		reg0 = CDR(reg0);
	}
	else
	    reg0 = reg1;
    }	NEXT_OPCODE();

OPCODE_LABEL(XBC_NTHCDR):
	reg1 = lisp__data.stack.values[--lisp__data.stack.length];
	if (!FIXNUMP(reg1) || (fixnum = FIXNUM_VALUE(reg1)) < 0)
	    LispDestroy("NTHCDR: %s is not a positive fixnum",
			STROBJ(reg1));
	if (reg0 != NIL) {
	    if (!CONSP(reg0))
		LispDestroy("NTHCDR: %s is not a list", STROBJ(reg0));
	    for (; fixnum > 0; fixnum--) {
		if (!CONSP(reg0))
		    break;
		reg0 = CDR(reg0);
	    }
	}
	NEXT_OPCODE();

	/* Push to builtin stack */
OPCODE_LABEL(XBC_CAR_PUSH):
	if (reg0 != NIL) {
	    if (!CONSP(reg0))
		LispDestroy("CAR: %s is not a list", STROBJ(reg0));
	    reg0 = CAR(reg0);
	}
	goto push_builtin;

OPCODE_LABEL(XBC_CDR_PUSH):
	if (reg0 != NIL) {
	    if (!CONSP(reg0))
		LispDestroy("CDR: %s is not a list", STROBJ(reg0));
	    reg0 = CDR(reg0);
	}
	/*FALLTROUGH*/

OPCODE_LABEL(XBC_PUSH):
push_builtin:
	lisp__data.stack.values[lisp__data.stack.length++] = reg0;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_PUSH_NIL):
	lisp__data.stack.values[lisp__data.stack.length++] = NIL;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_PUSH_UNSPEC):
	lisp__data.stack.values[lisp__data.stack.length++] = UNSPEC;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_PUSH_T):
	lisp__data.stack.values[lisp__data.stack.length++] = T;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_PUSH_NIL_N):
	for (offset = *stream++; offset > 0; offset--)
	    lisp__data.stack.values[lisp__data.stack.length++] = NIL;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_PUSH_UNSPEC_N):
	for (offset = *stream++; offset > 0; offset--)
	    lisp__data.stack.values[lisp__data.stack.length++] = UNSPEC;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LET):
let_argument:
	/*  The global object value is not changed, so it does not
	 * matter if it is a constant symbol. An error would be
	 * generated if it was declared as constant at the time of
	 * bytecode generation. Check can be done looking at the
	 * atom->constant field. */
	atom = symbols[*stream++];
	atom->offset = lisp__data.env.length;
	lisp__data.env.names[lisp__data.env.length] = atom->key;
	lisp__data.env.values[lisp__data.env.length++] = reg0;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LETX):
letx_argument:
	atom = symbols[*stream++];
	atom->offset = lisp__data.env.length;
	lisp__data.env.names[lisp__data.env.length] = atom->key;
	lisp__data.env.values[lisp__data.env.length++] = reg0;
	lisp__data.env.head++;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LET_NIL):
	atom = symbols[*stream++];
	atom->offset = lisp__data.env.length;
	lisp__data.env.names[lisp__data.env.length] = atom->key;
	lisp__data.env.values[lisp__data.env.length++] = NIL;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LETX_NIL):
	atom = symbols[*stream++];
	atom->offset = lisp__data.env.length;
	lisp__data.env.names[lisp__data.env.length] = atom->key;
	lisp__data.env.values[lisp__data.env.length++] = NIL;
	lisp__data.env.head++;
	NEXT_OPCODE();

	/* Bind locally added variables to a block */
OPCODE_LABEL(XBC_LETBIND):
	offset = *stream++;
	lisp__data.env.head += offset;
	NEXT_OPCODE();

	/* Unbind locally added variables to a block */
OPCODE_LABEL(XBC_UNLET):
	offset = *stream++;
	lisp__data.env.head -= offset;
	lisp__data.env.length -= offset;
	NEXT_OPCODE();

	/* Load value from stack */
OPCODE_LABEL(XBC_LOAD):
	offset = *stream++;
	reg0 = lisp__data.env.values[lisp__data.env.lex + offset];
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LOAD_CAR):
	offset = *stream++;
	reg0 = lisp__data.env.values[lisp__data.env.lex + offset];
	goto car;

OPCODE_LABEL(XBC_LOAD_CDR):
	offset = *stream++;
	reg0 = lisp__data.env.values[lisp__data.env.lex + offset];
	goto cdr;

OPCODE_LABEL(XBC_LOAD_CAR_STORE):
	offset = *stream++;
	reg0 = lisp__data.env.values[lisp__data.env.lex + offset];
	if (reg0 != NIL) {
	    if (!CONSP(reg0))
		LispDestroy("CAR: %s is not a list", STROBJ(reg0));
	    reg0 = CAR(reg0);
	    lisp__data.env.values[lisp__data.env.lex + offset] = reg0;
	}
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LOAD_CDR_STORE):
	offset = *stream++;
	reg0 = lisp__data.env.values[lisp__data.env.lex + offset];
	if (reg0 != NIL) {
	    if (!CONSP(reg0))
		LispDestroy("CDR: %s is not a list", STROBJ(reg0));
	    reg0 = CDR(reg0);
	    lisp__data.env.values[lisp__data.env.lex + offset] = reg0;
	}
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LOAD_LET):
	offset = *stream++;
	reg0 = lisp__data.env.values[lisp__data.env.lex + offset];
	goto let_argument;

OPCODE_LABEL(XBC_LOAD_LETX):
	offset = *stream++;
	reg0 = lisp__data.env.values[lisp__data.env.lex + offset];
	goto letx_argument;

OPCODE_LABEL(XBC_LOAD_PUSH):
	offset = *stream++;
	reg0 = lisp__data.env.values[lisp__data.env.lex + offset];
	lisp__data.stack.values[lisp__data.stack.length++] = reg0;
	NEXT_OPCODE();

	/* Load pointer to constant */
OPCODE_LABEL(XBC_LOADCON):
	reg0 = constants[*stream++];
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LOADCON_LET):
	reg0 = constants[*stream++];
	goto let_argument;

OPCODE_LABEL(XBC_LOADCON_LETX):
	reg0 = constants[*stream++];
	goto letx_argument;

OPCODE_LABEL(XBC_LOADCON_PUSH):
	reg0 = constants[*stream++];
	lisp__data.stack.values[lisp__data.stack.length++] = reg0;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LOADCON_SET):
	reg0 = constants[*stream++];
	offset = *stream++;
	lisp__data.env.values[lisp__data.env.lex + offset] = reg0;
	NEXT_OPCODE();

	/* Change value of local variable */
OPCODE_LABEL(XBC_CAR_SET):
car_set:
	if (reg0 != NIL) {
	    if (!CONSP(reg0))
		LispDestroy("CAR: %s is not a list", STROBJ(reg0));
	    reg0 = CAR(reg0);
	}
	goto set_local_variable;

OPCODE_LABEL(XBC_CDR_SET):
cdr_set:
	if (reg0 != NIL) {
	    if (!CONSP(reg0))
		LispDestroy("CDR: %s is not a list", STROBJ(reg0));
	    reg0 = CDR(reg0);
	}
	goto set_local_variable;

OPCODE_LABEL(XBC_LOAD_CAR_SET):
	offset = *stream++;
	reg0 = lisp__data.env.values[lisp__data.env.lex + offset];
	goto car_set;

OPCODE_LABEL(XBC_LOAD_CDR_SET):
	offset = *stream++;
	reg0 = lisp__data.env.values[lisp__data.env.lex + offset];
	goto cdr_set;

OPCODE_LABEL(XBC_LOAD_SET):
	offset = *stream++;
	reg0 = lisp__data.env.values[lisp__data.env.lex + offset];
	/*FALLTROUGH*/

OPCODE_LABEL(XBC_SET):
set_local_variable:
	offset = *stream++;
	lisp__data.env.values[lisp__data.env.lex + offset] = reg0;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_SET_NIL):
	offset = *stream++;
	lisp__data.env.values[lisp__data.env.lex + offset] = NIL;
	NEXT_OPCODE();

	/* Change value of a global/special variable */
OPCODE_LABEL(XBC_SETSYM):
	    atom = symbols[*stream++];
	    if (atom->dyn) {
		/*  atom->dyn and atom->constant are exclusive, no
		 * need to check if variable declared as constant. */
		if (atom->offset < lisp__data.env.head &&
		    lisp__data.env.names[atom->offset] == atom->key)
		    lisp__data.env.values[atom->offset] = reg0;
		else {
		    if (atom->watch)
			LispSetAtomObjectProperty(atom, reg0);
		    else
			SETVALUE(atom, reg0);
		}
	    }
	    else if (atom->a_object) {
		if (atom->constant)
		    LispDestroy("EVAL: %s is a constant",
				STROBJ(atom->object));
		else if (atom->watch)
		    LispSetAtomObjectProperty(atom, reg0);
		else
		    SETVALUE(atom, reg0);
	    }
	    else {
		/* Create new global variable */
		LispPackage *pack;

		LispWarning("the variable %s was not declared",
			    atom->key->value);
		LispSetAtomObjectProperty(atom, reg0);
		pack = atom->package->data.package.package;
		if (pack->glb.length >= pack->glb.space)
		    LispMoreGlobals(pack);
		pack->glb.pairs[pack->glb.length++] = atom->object;
	    }
	    NEXT_OPCODE();

/* Resolve symbol value at runtime */
#define LOAD_SYMBOL_VALUE()					    \
    atom = symbols[*stream++];					    \
    if (atom->dyn) {						    \
	if (atom->offset < lisp__data.env.head &&		    \
	    lisp__data.env.names[atom->offset] == atom->key)	    \
	    reg0 = lisp__data.env.values[atom->offset];		    \
	else {							    \
	    reg0 = atom->property->value;			    \
	    if (reg0 == UNBOUND)				    \
		LispDestroy("EVAL: the symbol %s is unbound",  \
			    STROBJ(atom->object));		    \
	}							    \
    }								    \
    else {							    \
	if (atom->a_object)					    \
	    reg0 = atom->property->value;			    \
	else							    \
	    LispDestroy("EVAL: the symbol %s is unbound",	    \
			STROBJ(atom->object));			    \
    }

OPCODE_LABEL(XBC_LOADSYM):
	LOAD_SYMBOL_VALUE();
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LOADSYM_LET):
	LOAD_SYMBOL_VALUE();
	goto let_argument;

OPCODE_LABEL(XBC_LOADSYM_LETX):
	LOAD_SYMBOL_VALUE();
	goto letx_argument;

OPCODE_LABEL(XBC_LOADSYM_PUSH):
	LOAD_SYMBOL_VALUE();
	lisp__data.stack.values[lisp__data.stack.length++] = reg0;
	NEXT_OPCODE();

	    /* Builtin function */
OPCODE_LABEL(XBC_CALL):
	offset = *stream++;
	lisp__data.stack.base = lisp__data.stack.length - offset;
	builtin = builtins[*stream++];
	if (builtin->multiple_values) {
	    RETURN_COUNT = 0;
	    reg0 = builtin->function(builtin);
	}
	else {
	    reg0 = builtin->function(builtin);
	    RETURN_COUNT = 0;
	}
	lisp__data.stack.length -= offset;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_CALL_SET):
	offset = *stream++;
	lisp__data.stack.base = lisp__data.stack.length - offset;
	builtin = builtins[*stream++];
	if (builtin->multiple_values) {
	    RETURN_COUNT = 0;
	    reg0 = builtin->function(builtin);
	}
	else {
	    reg0 = builtin->function(builtin);
	    RETURN_COUNT = 0;
	}
	lisp__data.stack.length -= offset;
	offset = *stream++;
	lisp__data.env.values[lisp__data.env.lex + offset] = reg0;
	NEXT_OPCODE();

	/* Bytecode call */
OPCODE_LABEL(XBC_BYTECALL):
	lex = lisp__data.env.lex;
	offset = *stream++;
	lisp__data.env.head = lisp__data.env.length;
	len = lisp__data.env.lex = lisp__data.env.length - offset;
	reg0 = ExecuteBytecode(codes[*stream++]);
	lisp__data.env.length = lisp__data.env.head = len;
	lisp__data.env.lex = lex;
	NEXT_OPCODE();

	/* Unimplemented function/macro call */
OPCODE_LABEL(XBC_FUNCALL):
	lambda = constants[*stream++];
	arguments = constants[*stream++];
	reg0 = LispFuncall(lambda, arguments, 1);
	NEXT_OPCODE();

OPCODE_LABEL(XBC_JUMP):
	stream += *(signed short*)stream;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_JUMPT):
	if (reg0 != NIL)
	    stream += *(signed short*)stream;
	else
	    /* skip jump relative offset */
	    stream += sizeof(signed short);
	NEXT_OPCODE();

OPCODE_LABEL(XBC_JUMPNIL):
	if (reg0 == NIL)
	    stream += *(signed short*)stream;
	else
	    /* skip jump relative offset */
	    stream += sizeof(signed short);
	NEXT_OPCODE();

	/* Build CONS of two constant arguments */
OPCODE_LABEL(XBC_CCONS):
	reg0 = constants[*stream++];
	reg1 = constants[*stream++];
	reg0 = CONS(reg0, reg1);
	NEXT_OPCODE();

	/* Start CONS */
OPCODE_LABEL(XBC_CSTAR):
	/* This the CAR of the CONS */
	lisp__data.protect.objects[phead++] = reg0;
	NEXT_OPCODE();

	/* Finish CONS */
OPCODE_LABEL(XBC_CFINI):
	reg0 = CONS(lisp__data.protect.objects[--phead], reg0);
	NEXT_OPCODE();

	/* Start building list */
OPCODE_LABEL(XBC_LSTAR):
	reg1 = CONS(reg0, NIL);
	/* Start of list stored here */
	lisp__data.protect.objects[phead++] = reg1;
	/* Tail of list stored here */
	lisp__data.protect.objects[phead++] = reg1;
	NEXT_OPCODE();

	/* Add to list */
OPCODE_LABEL(XBC_LCONS):
	reg1 = lisp__data.protect.objects[phead - 2];
	RPLACD(reg1, CONS(reg0, NIL));
	 lisp__data.protect.objects[phead - 2] = CDR(reg1);
	NEXT_OPCODE();

	/* Finish list */
OPCODE_LABEL(XBC_LFINI):
	phead -= 2;
	reg0 = lisp__data.protect.objects[phead + 1];
	NEXT_OPCODE();

OPCODE_LABEL(XBC_STRUCT):
	offset = *stream++;
	reg1 = constants[*stream++];
	if (!STRUCTP(reg0) || reg0->data.struc.def != reg1) {
	    char *name = ATOMID(CAR(reg1))->value;

	    for (reg1 = CDR(reg1); offset; offset--)
		reg1 = CDR(reg1);
	    LispDestroy("%s-%s: %s is not a %s",
			name, ATOMID(CAR(reg1))->value, STROBJ(reg0), name);
	}
	for (reg0 = reg0->data.struc.fields; offset; offset--)
	    reg0 = CDR(reg0);
	reg0 = CAR(reg0);
	NEXT_OPCODE();

OPCODE_LABEL(XBC_STRUCTP):
	reg1 = constants[*stream++];
	reg0 = STRUCTP(reg0) && reg0->data.struc.def == reg1 ? T : NIL;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_LETREC):
	/* XXX could/should optimize, shouldn't need to parse
	 * the bytecode header again */
	lex = lisp__data.env.lex;
	offset = *stream++;
	lisp__data.env.head = lisp__data.env.length;
	len = lisp__data.env.lex = lisp__data.env.length - offset;
	reg0 = ExecuteBytecode(bytecode);
	lisp__data.env.length = lisp__data.env.head = len;
	lisp__data.env.lex = lex;
	NEXT_OPCODE();

OPCODE_LABEL(XBC_RETURN):
	lisp__data.protect.length = pbase;
	return (reg0);

#ifndef ALLOW_GOTO_ADDRESS
	}	/* end of switch */

predicate_label:
	switch (*stream++) {
#endif

OPCODE_LABEL(XBP_CONSP):
	reg0 = CONSP(reg0) ? T : NIL;
	NEXT_OPCODE();

OPCODE_LABEL(XBP_LISTP):
	reg0 = LISTP(reg0) ? T : NIL;
	NEXT_OPCODE();

OPCODE_LABEL(XBP_NUMBERP):
	reg0 = NUMBERP(reg0) ? T : NIL;
	NEXT_OPCODE();

#ifndef ALLOW_GOTO_ADDRESS
	}	/* end of switch */
    }
#endif

    /*NOTREACHED*/
    return (reg0);
}
