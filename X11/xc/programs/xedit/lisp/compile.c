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

/* $XFree86: xc/programs/xedit/lisp/compile.c,v 1.14 2003/01/30 02:46:25 paulo Exp $ */

#define VARIABLE_USED		0x0001
#define VARIABLE_ARGUMENT	0x0002

/*
 * Prototypes
 */
static void ComPredicate(LispCom*, LispBuiltin*, LispBytePredicate);
static void ComReturnFrom(LispCom*, LispBuiltin*, int);

static int ComConstantp(LispCom*, LispObj*);
static void ComAddVariable(LispCom*, LispObj*, LispObj*);
static int ComGetVariable(LispCom*, LispObj*);
static void ComVariableSetFlag(LispCom*, LispAtom*, int);
#define COM_VARIABLE_USED(atom)				\
    ComVariableSetFlag(com, atom, VARIABLE_USED)
#define COM_VARIABLE_ARGUMENT(atom)			\
	ComVariableSetFlag(com, atom, VARIABLE_ARGUMENT)

static int FindIndex(void*, void**, int);
static int compare(const void*, const void*);
static int BuildTablePointer(void*, void***, int*);

static void ComLabel(LispCom*, LispObj*);
static void ComPush(LispCom*, LispObj*, LispObj*, int, int, int);
static int ComCall(LispCom*, LispArgList*, LispObj*, LispObj*, int, int, int);
static void ComFuncall(LispCom*, LispObj*, LispObj*, int);
static void ComProgn(LispCom*, LispObj*);
static void ComEval(LispCom*, LispObj*);

static void ComRecursiveCall(LispCom*, LispArgList*, LispObj*, LispObj*);
static void ComInlineCall(LispCom*, LispArgList*, LispObj*, LispObj*, LispObj*);

static void ComMacroBackquote(LispCom*, LispObj*);
static void ComMacroCall(LispCom*, LispArgList*, LispObj*, LispObj*, LispObj*);
static LispObj *ComMacroExpandBackquote(LispCom*, LispObj*);
static LispObj *ComMacroExpand(LispCom*, LispObj*);
static LispObj *ComMacroExpandFuncall(LispCom*, LispObj*, LispObj*);
static LispObj *ComMacroExpandEval(LispCom*, LispObj*);

/*
 * Implementation
 */
void
Com_And(LispCom *com, LispBuiltin *builtin)
/*
 and &rest args
 */
{
    LispObj *args;

    args = ARGUMENT(0);

    if (CONSP(args)) {
	/* Evaluate first argument */
	ComEval(com, CAR(args));
	args = CDR(args);

	/* If more than one argument, create jump list */
	if (CONSP(args)) {
	    CodeTree *tree = NULL, *group;

	    group = NEW_TREE(CodeTreeJumpIf);
	    group->code = XBC_JUMPNIL;

	    for (; CONSP(args); args = CDR(args)) {
		ComEval(com, CAR(args));
		tree = NEW_TREE(CodeTreeJumpIf);
		tree->code = XBC_JUMPNIL;
		group->group = tree;
		group = tree;
	    }
	    /*  Finish form the last CodeTree code is changed to sign the
	     * end of the AND list */
	    group->code = XBC_NOOP;
	    if (group)
		group->group = tree;
	}
    }
    else
	/* Identity of AND is T */
	com_Bytecode(com, XBC_T);
}

void
Com_Block(LispCom *com, LispBuiltin *builtin)
/*
 block name &rest body
 */
{

    LispObj *name, *body;

    body = ARGUMENT(1);
    name = ARGUMENT(0);

    if (name != NIL && name != T && !SYMBOLP(name))
	LispDestroy("%s: %s cannot name a block",
		    STRFUN(builtin), STROBJ(name));
    if (CONSP(body)) {
	CompileIniBlock(com, LispBlockTag, name);
	ComProgn(com, body);
	CompileFiniBlock(com);
    }
    else
	/* Just load NIL without starting an empty block */
	com_Bytecode(com, XBC_NIL);
}

void
Com_C_r(LispCom *com, LispBuiltin *builtin)
/*
 c[ad]{1,4}r list
 */
{
    LispObj *list;
    char *desc;

    list = ARGUMENT(0);

    desc = STRFUN(builtin);
    if (*desc == 'F')		/* FIRST */
	desc = "CAR";
    else if (*desc == 'R')	/* REST */
	desc = "CDR";

    /* Check if it is a list of constants */
    while (desc[1] != 'R')
	desc++;
    ComEval(com, list);
    while (*desc != 'C') {
	com_Bytecode(com, *desc == 'A' ? XBC_CAR : XBC_CDR);
	--desc;
    }
}

void
Com_Cond(LispCom *com, LispBuiltin *builtin)
/*
 cond &rest body
 */
{
    int count;
    LispObj *code, *body;
    CodeTree *group, *tree;

    body = ARGUMENT(0);

    count = 0;
    group = NULL;
    if (CONSP(body)) {
	for (; CONSP(body); body = CDR(body)) {
	    code = CAR(body);
	    CHECK_CONS(code);
	    ++count;
	    ComEval(com, CAR(code));
	    tree = NEW_TREE(CodeTreeCond);
	    if (group)
		group->group = tree;
	    tree->code = XBC_JUMPNIL;
	    group = tree;
	    /* The code to execute if the test is true */
	    ComProgn(com, CDR(code));
	    /* Add a node signaling the end of the PROGN code */
	    tree = NEW_TREE(CodeTreeCond);
	    tree->code = XBC_JUMPT;
	    if (group)
		group->group = tree;
	    group = tree;
	}
    }
    if (!count)
	com_Bytecode(com, XBC_NIL);
    else
	/* Where to jump after T progn */
	group->code = XBC_NOOP;
}

void
Com_Cons(LispCom *com, LispBuiltin *builtin)
/*
 cons car cdr
 */
{
    LispObj *car, *cdr;

    cdr = ARGUMENT(1);
    car = ARGUMENT(0);

    if (ComConstantp(com, car) && ComConstantp(com, cdr))
	com_BytecodeCons(com, XBC_CCONS, car, cdr);
    else {
	++com->stack.cpstack;
	if (com->stack.pstack < com->stack.cpstack)
	    com->stack.pstack = com->stack.cpstack;
	ComEval(com, car);
	com_Bytecode(com, XBC_CSTAR);
	ComEval(com, cdr);
	com_Bytecode(com, XBC_CFINI);
	--com->stack.cpstack;
    }
}

void
Com_Consp(LispCom *com, LispBuiltin *builtin)
/*
 consp object
 */
{
    ComPredicate(com, builtin, XBP_CONSP);
}

void
Com_Dolist(LispCom *com, LispBuiltin *builtin)
/*
 dolist init &rest body
 */
{
    int unbound, item;
    LispObj *symbol, *list, *result;
    LispObj *init, *body;
    CodeTree *group, *tree;

    body = ARGUMENT(1);
    init = ARGUMENT(0);

    CHECK_CONS(init);
    symbol = CAR(init);
    CHECK_SYMBOL(symbol);
    CHECK_CONSTANT(symbol);
    init = CDR(init);
    if (CONSP(init)) {
	list = CAR(init);
	init = CDR(init);
    }
    else
	list = NIL;
    if (CONSP(init)) {
	result = CAR(init);
	if (CONSP(CDR(init)))
	    LispDestroy("%s: too many arguments %s",
			STRFUN(builtin), STROBJ(CDR(init)));
    }
    else
	result = NIL;

    /*	Generate code for the body of the form.
     *	The generated code uses two objects unavailable to user code,
     * in the format:
     *	(block NIL
     *	    (let ((? list) (item NIL))
     *		(tagbody
     *		    .			    ; the DOT object as a label
     *		    (when (consp list)
     *			(setq item (car ?))
     *			@body		    ; code to be executed
     *			(setq ? (cdr ?))
     *			(go .)
     *		    )
     *		)
     *		(setq item nil)
     *		result
     *	    )
     *	)
     */

    /* XXX All of the logic below should be simplified at some time
     * by adding more opcodes for compound operations ... */

    /* Relative offsets the locally added variables will have at run time */
    unbound = lisp__data.env.length - lisp__data.env.lex;
    item = unbound + 1;

    /* Start BLOCK NIL */
    FORM_ENTER();
    CompileIniBlock(com, LispBlockTag, NIL);

    /* Add the <?> variable */
    ComPush(com, UNBOUND, list, 1, 0, 0);
    /* Add the <item> variable */
    ComPush(com, symbol, NIL, 0, 0, 0);
    /* Stack length is increased */
    CompileStackEnter(com, 2, 0);
    /* Bind variables */
    com_Bind(com, 2);
    com->block->bind += 2;
    lisp__data.env.head += 2;

    /* Remember that iteration variable is used even if it not referenced */
    COM_VARIABLE_USED(symbol->data.atom);

    /* Initialize the TAGBODY */
    FORM_ENTER();
    CompileIniBlock(com, LispBlockBody, NIL);

    /* Create the <.> label */
    ComLabel(com, DOT);

    /* Load <?> variable */
    com_BytecodeShort(com, XBC_LOAD, unbound);
    /* Check if <?> is a list */
    com_BytecodeChar(com, XBC_PRED, XBP_CONSP);

    /* Start WHEN block */
    group = NEW_TREE(CodeTreeJumpIf);
    group->code = XBC_JUMPNIL;
    /* Load <?> again */
    com_BytecodeShort(com, XBC_LOAD, unbound);
    /* Get CAR of <?> */
    com_Bytecode(com, XBC_CAR);
    /* Store it in <item> */
    com_BytecodeShort(com, XBC_SET, item);
    /* Execute @BODY */
    ComProgn(com, body);

    /* Load <?> again */
    com_BytecodeShort(com, XBC_LOAD, unbound);
    /* Get CDR of <?> */
    com_Bytecode(com, XBC_CDR);
    /* Change value of <?> */
    com_BytecodeShort(com, XBC_SET, unbound);

    /* GO back to <.> */
    tree = NEW_TREE(CodeTreeGo);
    tree->data.object = DOT;

    /* Finish WHEN block */
    tree = NEW_TREE(CodeTreeJumpIf);
    tree->code = XBC_NOOP;
    group->group = tree;

    /* Finish the TAGBODY */
    CompileFiniBlock(com);
    FORM_LEAVE();

    /* Set <item> to NIL, in case result references it...
     * Loaded value is NIL as the CONSP predicate */
    com_BytecodeShort(com, XBC_SET, item);

    /* Evaluate <result> */
    ComEval(com, result);

    /* Unbind variables */
    lisp__data.env.head -= 2;
    lisp__data.env.length -= 2;
    com->block->bind -= 2;
    com_Unbind(com, 2);
    /* Stack length is reduced. */
    CompileStackLeave(com, 2, 0);

    /* Finish BLOCK NIL */
    CompileFiniBlock(com);
    FORM_LEAVE();
}

void
Com_Eq(LispCom *com, LispBuiltin *builtin)
/*
 eq left right
 eql left right
 equal left right
 equalp left right
 */
{
    LispObj *left, *right;
    LispByteOpcode code;
    char *name;

    right = ARGUMENT(1);
    left = ARGUMENT(0);

    CompileStackEnter(com, 1, 1);
    /* Just like preparing to call a builtin function */
    ComEval(com, left);
    com_Bytecode(com, XBC_PUSH);
    /* The second argument is now loaded */
    ComEval(com, right);

    /* Compare arguments and restore builtin stack */
    name = STRFUN(builtin);
    switch (name[3]) {
	case 'L':
	    code = XBC_EQL;
	    break;
	case 'U':
	    code = name[5] == 'P' ? XBC_EQUALP : XBC_EQUAL;
	    break;
	default:
	    code = XBC_EQ;
	    break;
    }
    com_Bytecode(com, code);

    CompileStackLeave(com, 1, 1);
}

void
Com_Go(LispCom *com, LispBuiltin *builtin)
/*
 go tag
 */
{
    int bind;
    LispObj *tag;
    CodeTree *tree;
    CodeBlock *block;

    tag = ARGUMENT(0);

    block = com->block;
    bind = block->bind;

    while (block) {
	if (block->type == LispBlockClosure || block->type == LispBlockBody)
	    break;
	block = block->prev;
	if (block)
	    bind += block->bind;
    }

    if (!block || block->type != LispBlockBody)
	LispDestroy("%s called not within a block", STRFUN(builtin));

    /* Unbind any local variables */
    com_Unbind(com, bind);
    tree = NEW_TREE(CodeTreeGo);
    tree->data.object = tag;
}

void
Com_If(LispCom *com, LispBuiltin *builtin)
/*
 if test then &optional else
 */
{
    CodeTree *group, *tree;
    LispObj *test, *then, *oelse;

    oelse = ARGUMENT(2);
    then = ARGUMENT(1);
    test = ARGUMENT(0);

    /* Build code to execute test */
    ComEval(com, test);

    /* Add jump node to use if test is NIL */
    group = NEW_TREE(CodeTreeJumpIf);
    group->code = XBC_JUMPNIL;

    /* Build T code */
    ComEval(com, then);

    if (oelse != UNSPEC) {
	/* Remember start of NIL code */
	tree = NEW_TREE(CodeTreeJump);
	tree->code = XBC_JUMP;
	group->group = tree;
	group = tree;
	/* Build NIL code */
	ComEval(com, oelse);
    }

    /* Remember jump of T code */
    tree = NEW_TREE(CodeTreeJumpIf);
    tree->code = XBC_NOOP;
    group->group = tree;
}

void
Com_Last(LispCom *com, LispBuiltin *builtin)
/*
 last list &optional count
 */
{
    LispObj *list, *count;

    count = ARGUMENT(1);
    list = ARGUMENT(0);

    ComEval(com, list);
    CompileStackEnter(com, 1, 1);
    com_Bytecode(com, XBC_PUSH);
    if (count == UNSPEC)
	count = FIXNUM(1);
    ComEval(com, count);
    CompileStackLeave(com, 1, 1);
    com_Bytecode(com, XBC_LAST);
}

void
Com_Length(LispCom *com, LispBuiltin *builtin)
/*
 length sequence
 */
{
    LispObj *sequence;

    sequence = ARGUMENT(0);

    ComEval(com, sequence);
    com_Bytecode(com, XBC_LENGTH);
}

void
Com_Let(LispCom *com, LispBuiltin *builtin)
/*
 let init &rest body
 */
{
    int count;
    LispObj *symbol, *value, *pair;

    LispObj *init, *body;

    body = ARGUMENT(1);
    init = ARGUMENT(0);

    if (init == NIL) {
	/* If no local variables */
	ComProgn(com, body);
	return;
    }
    CHECK_CONS(init);

    /* Could optimize if the body is empty and the
     * init form is known to have no side effects */

    for (count = 0; CONSP(init); init = CDR(init), count++) {
	pair = CAR(init);
	if (CONSP(pair)) {
	    symbol = CAR(pair);
	    pair = CDR(pair);
	    if (CONSP(pair)) {
		value = CAR(pair);
		if (CDR(pair) != NIL)
		    LispDestroy("%s: too much arguments to initialize %s",
				STRFUN(builtin), STROBJ(symbol));
	    }
	    else
		value = NIL;
	}
	else {
	    symbol = pair;
	    value = NIL;
	}
	CHECK_SYMBOL(symbol);
	CHECK_CONSTANT(symbol);

	/* Add the variable */
	ComPush(com, symbol, value, 1, 0, 0);
    }

    /* Stack length is increased */
    CompileStackEnter(com, count, 0);
    /* Bind the added variables */
    com_Bind(com, count);
    com->block->bind += count;
    lisp__data.env.head += count;
    /* Generate code for the body of the form */
    ComProgn(com, body);
    /* Unbind the added variables */
    lisp__data.env.head -= count;
    lisp__data.env.length -= count;
    com->block->bind -= count;
    com_Unbind(com, count);
    /* Stack length is reduced. */
    CompileStackLeave(com, count, 0);
}

void
Com_Letx(LispCom *com, LispBuiltin *builtin)
/*
 let* init &rest body
 */
{
    int count;
    LispObj *symbol, *value, *pair;

    LispObj *init, *body;

    body = ARGUMENT(1);
    init = ARGUMENT(0);

    if (init == NIL) {
	/* If no local variables */
	ComProgn(com, body);
	return;
    }
    CHECK_CONS(body);

    /* Could optimize if the body is empty and the
     * init form is known to have no side effects */

    for (count = 0; CONSP(init); init = CDR(init), count++) {
	pair = CAR(init);
	if (CONSP(pair)) {
	    symbol = CAR(pair);
	    pair = CDR(pair);
	    if (CONSP(pair)) {
		value = CAR(pair);
		if (CDR(pair) != NIL)
		    LispDestroy("%s: too much arguments to initialize %s",
				STRFUN(builtin), STROBJ(symbol));
	    }
	    else
		value = NIL;
	}
	else {
	    symbol = pair;
	    value = NIL;
	}
	CHECK_SYMBOL(symbol);
	CHECK_CONSTANT(symbol);

	/* LET* is identical to &AUX arguments, just bind the symbol */
	ComPush(com, symbol, value, 1, 0, 0);
	/* Every added variable is binded */
	com_Bind(com, 1);
	/* Must be binded at compile time also */
	++lisp__data.env.head;
	++com->block->bind;
    }

    /* Generate code for the body of the form */
    CompileStackEnter(com, count, 0);
    ComProgn(com, body);
    com_Unbind(com, count);
    com->block->bind -= count;
    lisp__data.env.head -= count;
    lisp__data.env.length -= count;
    CompileStackLeave(com, count, 0);
}

void
Com_Listp(LispCom *com, LispBuiltin *builtin)
/*
 listp object
 */
{
    ComPredicate(com, builtin, XBP_LISTP);
}

void
Com_Loop(LispCom *com, LispBuiltin *builtin)
/*
 loop &rest body
 */
{
    CodeTree *tree, *group;
    LispObj *body;

    body = ARGUMENT(0);

    /* Start NIL block */
    CompileIniBlock(com, LispBlockTag, NIL);

    /* Insert node to mark LOOP start */
    tree = NEW_TREE(CodeTreeJump);
    tree->code = XBC_NOOP;

    /* Execute @BODY */
    if (CONSP(body))
	ComProgn(com, body);
    else
	/* XXX bytecode.c code require that blocks have at least one opcode */
	com_Bytecode(com, XBC_NIL);

    /* Insert node to jump of start of LOOP */
    group = NEW_TREE(CodeTreeJump);
    group->code = XBC_JUMP;
    group->group = tree;

    /* Finish NIL block */
    CompileFiniBlock(com);
}

void
Com_Nthcdr(LispCom *com, LispBuiltin *builtin)
/*
 nthcdr index list
 */
{
    LispObj *oindex, *list;

    list = ARGUMENT(1);
    oindex = ARGUMENT(0);

    ComEval(com, oindex);
    CompileStackEnter(com, 1, 1);
    com_Bytecode(com, XBC_PUSH);
    ComEval(com, list);
    CompileStackLeave(com, 1, 1);
    com_Bytecode(com, XBC_NTHCDR);
}

void
Com_Null(LispCom *com, LispBuiltin *builtin)
/*
 null list
 */
{
    LispObj *list;

    list = ARGUMENT(0);

    if (list == NIL)
	com_Bytecode(com, XBC_T);
    else if (ComConstantp(com, list))
	com_Bytecode(com, XBC_NIL);
    else {
	ComEval(com, list);
	com_Bytecode(com, XBC_INV);
    }
}

void
Com_Numberp(LispCom *com, LispBuiltin *builtin)
/*
 numberp object
 */
{
    ComPredicate(com, builtin, XBP_NUMBERP);
}

void
Com_Or(LispCom *com, LispBuiltin *builtin)
/*
 or &rest args
 */
{
    LispObj *args;

    args = ARGUMENT(0);

    if (CONSP(args)) {
	/* Evaluate first argument */
	ComEval(com, CAR(args));
	args = CDR(args);

	/* If more than one argument, create jump list */
	if (CONSP(args)) {
	    CodeTree *tree = NULL, *group;

	    group = NEW_TREE(CodeTreeJumpIf);
	    group->code = XBC_JUMPT;

	    for (; CONSP(args); args = CDR(args)) {
		ComEval(com, CAR(args));
		tree = NEW_TREE(CodeTreeJumpIf);
		tree->code = XBC_JUMPT;
		group->group = tree;
		group = tree;
	    }
	    /*  Finish form the last CodeTree code is changed to sign the
	     * end of the AND list */
	    group->code = XBC_NOOP;
	    group->group = tree;
	}
    }
    else
	/* Identity of OR is NIL */
	com_Bytecode(com, XBC_NIL);
}

void
Com_Progn(LispCom *com, LispBuiltin *builtin)
/*
 progn &rest body
 */
{
    LispObj *body;

    body = ARGUMENT(0);

    ComProgn(com, body);
}

void
Com_Return(LispCom *com, LispBuiltin *builtin)
/*
 return &optional result
 */
{
    ComReturnFrom(com, builtin, 0);
}

void
Com_ReturnFrom(LispCom *com, LispBuiltin *builtin)
/*
 return-from name &optional result
 */
{
    ComReturnFrom(com, builtin, 1);
}

void
Com_Rplac_(LispCom *com, LispBuiltin *builtin)
/*
 rplac[ad] place value
 */
{
    LispObj *place, *value;

    value = ARGUMENT(1);
    place = ARGUMENT(0);

    CompileStackEnter(com, 1, 1);
    ComEval(com, place);
    com_Bytecode(com, XBC_PUSH);
    ComEval(com, value);
    com_Bytecode(com, STRFUN(builtin)[5] == 'A' ? XBC_RPLACA : XBC_RPLACD);
    CompileStackLeave(com, 1, 1);
}

void
Com_Setq(LispCom *com, LispBuiltin *builtin)
/*
 setq &rest form
 */
{
    int offset;
    LispObj *form, *symbol, *value;

    form = ARGUMENT(0);

    for (; CONSP(form); form = CDR(form)) {
	symbol = CAR(form);
	CHECK_SYMBOL(symbol);
	CHECK_CONSTANT(symbol);
	form = CDR(form);
	if (!CONSP(form))
	    LispDestroy("%s: odd number of arguments", STRFUN(builtin));
	value = CAR(form);
	/* Generate code to load value */
	ComEval(com, value);
	offset = ComGetVariable(com, symbol);
	if (offset >= 0)
	    com_Set(com, offset);
	else
	    com_SetSym(com, symbol->data.atom);
    }
}

void
Com_Tagbody(LispCom *com, LispBuiltin *builtin)
/*
 tagbody &rest body
 */
{
    LispObj *body;

    body = ARGUMENT(0);

    if (CONSP(body)) {
	CompileIniBlock(com, LispBlockBody, NIL);
	ComProgn(com, body);
	/* Tagbody returns NIL */
	com_Bytecode(com, XBC_NIL);
	CompileFiniBlock(com);
    }
    else
	/* Tagbody always returns NIL */
	com_Bytecode(com, XBC_NIL);
}

void
Com_Unless(LispCom *com, LispBuiltin *builtin)
/*
 unless test &rest body
 */
{
    CodeTree *group, *tree;
    LispObj *test, *body;

    body = ARGUMENT(1);
    test = ARGUMENT(0);

    /* Generate code to evaluate test */
    ComEval(com, test);
    /* Add node after test */
    group = NEW_TREE(CodeTreeJumpIf);
    group->code = XBC_JUMPT;
    /* Generate NIL code */
    ComProgn(com, body);
    /* Insert node to know where to jump if test is T */
    tree = NEW_TREE(CodeTreeJumpIf);
    tree->code = XBC_NOOP;
    group->group = tree;
}

void
Com_Until(LispCom *com, LispBuiltin *builtin)
/*
 until test &rest body
 */
{
    CodeTree *tree, *group, *ltree, *lgroup;
    LispObj *test, *body;

    body = ARGUMENT(1);
    test = ARGUMENT(0);

    /* Insert node to mark LOOP start */
    ltree = NEW_TREE(CodeTreeJump);
    ltree->code = XBC_NOOP;

    /* Build code for test */
    ComEval(com, test);
    group = NEW_TREE(CodeTreeJumpIf);
    group->code = XBC_JUMPT;

    /* Execute @BODY */
    ComProgn(com, body);

    /* Insert node to jump to test again */
    lgroup = NEW_TREE(CodeTreeJump);
    lgroup->code = XBC_JUMP;
    lgroup->group = ltree;

    /* Insert node to know where to jump if test is T */
    tree = NEW_TREE(CodeTreeJumpIf);
    tree->code = XBC_NOOP;
    group->group = tree;
}

void
Com_When(LispCom *com, LispBuiltin *builtin)
/*
 when test &rest body
 */
{
    CodeTree *group, *tree;
    LispObj *test, *body;

    body = ARGUMENT(1);
    test = ARGUMENT(0);

    /* Generate code to evaluate test */
    ComEval(com, test);
    /* Add node after test */
    group = NEW_TREE(CodeTreeJumpIf);
    group->code = XBC_JUMPNIL;
    /* Generate T code */
    ComProgn(com, body);
    /* Insert node to know where to jump if test is NIL */
    tree = NEW_TREE(CodeTreeJumpIf);
    tree->code = XBC_NOOP;
    group->group = tree;
}

void
Com_While(LispCom *com, LispBuiltin *builtin)
/*
 while test &rest body
 */
{
    CodeTree *tree, *group, *ltree, *lgroup;
    LispObj *test, *body;

    body = ARGUMENT(1);
    test = ARGUMENT(0);

    /* Insert node to mark LOOP start */
    ltree = NEW_TREE(CodeTreeJump);
    ltree->code = XBC_NOOP;

    /* Build code for test */
    ComEval(com, test);
    group = NEW_TREE(CodeTreeJumpIf);
    group->code = XBC_JUMPNIL;

    /* Execute @BODY */
    ComProgn(com, body);

    /* Insert node to jump to test again */
    lgroup = NEW_TREE(CodeTreeJump);
    lgroup->code = XBC_JUMP;
    lgroup->group = ltree;

    /* Insert node to know where to jump if test is NIL */
    tree = NEW_TREE(CodeTreeJumpIf);
    tree->code = XBC_NOOP;
    group->group = tree;
}


/***********************************************************************
 * Com_XXX helper functions
 ***********************************************************************/
static void
ComPredicate(LispCom *com, LispBuiltin *builtin, LispBytePredicate predicate)
{
    LispObj *object;

    object = ARGUMENT(0);

    if (ComConstantp(com, object)) {
	switch (predicate) {
	    case XBP_CONSP:
		com_Bytecode(com, CONSP(object) ? XBC_T : XBC_NIL);
		break;
	    case XBP_LISTP:
		com_Bytecode(com, CONSP(object) || object == NIL ?
			     XBC_T : XBC_NIL);
		break;
	    case XBP_NUMBERP:
		com_Bytecode(com, NUMBERP(object) ? XBC_T : XBC_NIL);
		break;
	}
    }
    else {
	ComEval(com, object);
	com_BytecodeChar(com, XBC_PRED, predicate);
    }
}

/* XXX Could receive an argument telling if is the last statement in the
 * block(s), i.e. if a jump opcode should be generated or just the
 * evaluation of the returned value. Probably this is better done in
 * an optimization step. */
static void
ComReturnFrom(LispCom *com, LispBuiltin *builtin, int from)
{
    int bind;
    CodeTree *tree;
    LispObj *name, *result;
    CodeBlock *block = com->block;

    if (from) {
	result = ARGUMENT(1);
	name = ARGUMENT(0);
    }
    else {
	result = ARGUMENT(0);
	name = NIL;
    }
    if (result == UNSPEC)
	result = NIL;

    bind = block->bind;
    while (block) {
	if (block->type == LispBlockClosure)
	    /* A function call */
	    break;
	else if (block->type == LispBlockTag && block->tag == name)
	    break;
	block = block->prev;
	if (block)
	    bind += block->bind;
    }

    if (!block || block->tag != name)
	LispDestroy("%s: no visible %s block", STRFUN(builtin), STROBJ(name));

    /* Generate code to load result */
    ComEval(com, result);

    /* Check for added variables that the jump is skiping the unbind opcode */
    com_Unbind(com, bind);

    tree = NEW_TREE(CodeTreeReturn);
    tree->data.block = block;
}

/***********************************************************************
 * Helper functions
 ***********************************************************************/
static int
ComConstantp(LispCom *com, LispObj *object)
{
    switch (OBJECT_TYPE(object)) {
	case LispAtom_t:
	    /* Keywords are guaranteed to evaluate to itself */
	    if (object->data.atom->package == lisp__data.keyword)
		break;
	    return (0);

	    /* Function call */
	case LispCons_t:

	    /* Need macro expansion, these are special abstract objects */
	case LispQuote_t:
	case LispBackquote_t:
	case LispComma_t:
	case LispFunctionQuote_t:
	    return (0);

	    /* Anything else is a literal constant */
	default:
	    break;
    }

    return (1);
}

static int
FindIndex(void *item, void **table, int length)
{
    long cmp;
    int left, right, i;

    left = 0;
    right = length - 1;
    while (left <= right) {
	i = (left + right) >> 1;
	cmp = (char*)item - (char*)table[i];
	if (cmp == 0)
	    return (i);
	else if (cmp < 0)
	    right = i - 1;
	else
	    left = i + 1;
    }

    return (-1);
}

static int
compare(const void *left, const void *right)
{
    long cmp = *(char**)left - *(char**)right;

    return (cmp < 0 ? -1 : 1);
}

static int
BuildTablePointer(void *pointer, void ***pointers, int *num_pointers)
{
    int i;

    if ((i = FindIndex(pointer, *pointers, *num_pointers)) < 0) {
	*pointers = LispRealloc(*pointers,
				sizeof(void*) * (*num_pointers + 1));
	(*pointers)[*num_pointers] = pointer;
	if (++*num_pointers > 1)
	    qsort(*pointers, *num_pointers, sizeof(void*), compare);
	i = FindIndex(pointer, *pointers, *num_pointers);
    }

    return (i);
}

static void
ComAddVariable(LispCom *com, LispObj *symbol, LispObj *value)
{
    LispAtom *atom = symbol->data.atom;

    if (atom && atom->string && !com->macro) {
	int i, length = com->block->variables.length;

	i = BuildTablePointer(atom, (void***)&com->block->variables.symbols,
			      &com->block->variables.length);

	if (com->block->variables.length != length) {
	    com->block->variables.flags =
		LispRealloc(com->block->variables.flags,
			    com->block->variables.length * sizeof(int));

	    /* Variable was inserted in the middle of the list */
	    if (i < length)
		memmove(com->block->variables.flags + i + 1,
			com->block->variables.flags + i,
			(length - i) * sizeof(int));

	    com->block->variables.flags[i] = 0;
	}
    }

    LispAddVar(symbol, value);
}

static int
ComGetVariable(LispCom *com, LispObj *symbol)
{
    LispAtom *name;
    int i, base, offset;
    Atom_id id;

    name = symbol->data.atom;
    if (name->constant) {
	if (name->package == lisp__data.keyword)
	    /*	Just load <symbol> from the byte stream, keywords are
	     * guaranteed to evaluate to itself. */
	    return (SYMBOL_KEYWORD);
	return (SYMBOL_CONSTANT);
    }

    offset = name->offset;
    id = name->string;
    base = lisp__data.env.lex;
    i = lisp__data.env.head - 1;

    /* If variable is local */
    if (offset <= i && offset >= com->lex && lisp__data.env.names[offset] == id) {
	COM_VARIABLE_USED(name);
	/* Relative offset */
	return (offset - base);
    }

    /* name->offset may have been changed in a macro expansion */
    for (; i >= com->lex; i--)
	if (lisp__data.env.names[i] == id) {
	    name->offset = i;
	    COM_VARIABLE_USED(name);
	    return (i - base);
	}

    if (!name->a_object) {
	++com->warnings;
	LispWarning("variable %s is neither declared nor bound",
		    name->string);
    }

    /* Not found, resolve <symbol> at run time */
    return (SYMBOL_UNBOUND);
}

static void
ComVariableSetFlag(LispCom *com, LispAtom *atom, int flag)
{
    int i;
    CodeBlock *block = com->block;

    while (block) {
	i = FindIndex(atom, (void**)block->variables.symbols,
		      block->variables.length);
	if (i >= 0) {
	    block->variables.flags[i] |= flag;
	    /*  Descend block list if an argument to function being called
	     * has the same name as a bound variable in the current function.
	     */
	    if ((flag & VARIABLE_ARGUMENT) ||
		!(block->variables.flags[i] & VARIABLE_ARGUMENT))
		break;
	}
	block = block->prev;
    }
}

/***********************************************************************
 * Bytecode compiler functions
 ***********************************************************************/
static void
ComLabel(LispCom *com, LispObj *label)
{
    int i;
    CodeTree *tree;

    for (i = 0; i < com->block->tagbody.length; i++)
	if (label == com->block->tagbody.labels[i])
	    LispDestroy("TAGBODY: tag %s specified more than once",
			STROBJ(label));

    if (com->block->tagbody.length >= com->block->tagbody.space) {
	com->block->tagbody.labels =
	    LispRealloc(com->block->tagbody.labels,
			sizeof(LispObj*) * (com->block->tagbody.space + 8));
	/*  Reserve space, will be used at link time when
	 * resolving GO jumps. */
	com->block->tagbody.codes =
	    LispRealloc(com->block->tagbody.codes,
			sizeof(CodeTree*) * (com->block->tagbody.space + 8));
	com->block->tagbody.space += 8;
    }

    com->block->tagbody.labels[com->block->tagbody.length++] = label;
    tree = NEW_TREE(CodeTreeLabel);
    tree->data.object = label;
}

static void
ComPush(LispCom *com, LispObj *symbol, LispObj *value,
	int eval, int builtin, int compile)
{
    /*  If <compile> is set, it is pushing an argument to one of
     * Com_XXX functions. */
    if (compile) {
	if (builtin)
	    lisp__data.stack.values[lisp__data.stack.length++] = value;
	else
	    ComAddVariable(com, symbol, value);
	return;
    }

    /*  If <com->macro> is set, it is expanding a macro, just add the local
     * variable <symbol> bounded to <value>, so that it will be available
     * when calling the interpreter to expand the macro. */
    else if (com->macro) {
	ComAddVariable(com, symbol, value);
	return;
    }

    /*  If <eval> is set, it must generate the opcodes to evaluate <value>.
     * If <value> is a constant, just generate the opcodes to load it. */
    else if (eval && !ComConstantp(com, value)) {
	switch (OBJECT_TYPE(value)) {
	    case LispAtom_t: {
		int offset = ComGetVariable(com, value);

		if (offset >= 0) {
		    /* Load <value> from user stack at the relative offset */
		    if (builtin)
			com_LoadPush(com, offset);
		    else
			com_LoadLet(com, offset, symbol->data.atom);
		}
		/* ComConstantp() does not return true for this, as the
		 * current value must be computed. */
		else if (offset == SYMBOL_CONSTANT) {
		    value = value->data.atom->property->value;
		    if (builtin)
			com_LoadConPush(com, value);
		    else
			com_LoadConLet(com, value, symbol->data.atom);
		}
		else {
		    /* Load value bound to <value> at run time */
		    if (builtin)
			com_LoadSymPush(com, value->data.atom);
		    else
			com_LoadSymLet(com, value->data.atom,
				       symbol->data.atom);
		}
	    }	break;

	    default:
		/* Generate code to evaluate <value> */
		ComEval(com, value);
		if (builtin)
		    com_Bytecode(com, XBC_PUSH);
		else
		    com_Let(com, symbol->data.atom);
		break;
	}

	/*  Remember <symbol> will be bound, <value> only matters for
	 * the Com_XXX  functions */
	if (builtin)
	    lisp__data.stack.values[lisp__data.stack.length++] = value;
	else
	    ComAddVariable(com, symbol, value);
	return;
    }

    if (builtin) {
	/* Load <value> as a constant in builtin stack */
	com_LoadConPush(com, value);
	lisp__data.stack.values[lisp__data.stack.length++] = value;
    }
    else {
	/* Load <value> as a constant in stack */
	com_LoadConLet(com, value, symbol->data.atom);
	/* Remember <symbol> will be bound */
	ComAddVariable(com, symbol, value);
    }
}

/*  This function does almost the same job as LispMakeEnvironment, but
 * it is not optimized for speed, as it is not building argument lists
 * to user code, but to Com_XXX functions, or helping in generating the
 * opcodes to load arguments at bytecode run time. */
static int
ComCall(LispCom *com, LispArgList *alist,
	LispObj *name, LispObj *values,
	int eval, int builtin, int compile)
{
    char *desc;
    int i, count, base;
    LispObj **symbols, **defaults, **sforms;

    if (builtin) {
	base = lisp__data.stack.length;
	/* This should never be executed, but make the check for safety */
	if (base + alist->num_arguments > lisp__data.stack.space) {
	    do
		LispMoreStack();
	    while (base + alist->num_arguments > lisp__data.stack.space);
	}
    }
    else
	base = lisp__data.env.length;

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


    /* Normal arguments */
normal_label:
    i = 0;
    symbols = alist->normals.symbols;
    count = alist->normals.num_symbols;
    for (; i < count && CONSP(values); i++, values = CDR(values)) {
	ComPush(com, symbols[i], CAR(values), eval, builtin, compile);
	if (!builtin && !com->macro)
	    COM_VARIABLE_ARGUMENT(symbols[i]->data.atom);
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
    symbols = alist->optionals.symbols;
    defaults = alist->optionals.defaults;
    sforms = alist->optionals.sforms;
    for (; i < count && CONSP(values); i++, values = CDR(values)) {
	ComPush(com, symbols[i], CAR(values), eval, builtin, compile);
	if (!builtin && !com->macro)
	    COM_VARIABLE_ARGUMENT(symbols[i]->data.atom);
	if (sforms[i]) {
	    ComPush(com, sforms[i], T, 0, builtin, compile);
	    if (!builtin && !com->macro)
		COM_VARIABLE_ARGUMENT(sforms[i]->data.atom);
	}
    }
    for (; i < count; i++) {
	if (!builtin) {
	    int lex = com->lex;
	    int head = lisp__data.env.head;

	    com->lex = base;
	    lisp__data.env.head = lisp__data.env.length;
	    /* default arguments are evaluated for macros */
	    ComPush(com, symbols[i], defaults[i], 1, 0, compile);
	    if (!com->macro)
		COM_VARIABLE_ARGUMENT(symbols[i]->data.atom);
	    lisp__data.env.head = head;
	    com->lex = lex;
	}
	else
	    ComPush(com, symbols[i], defaults[i], eval, 1, compile);
	if (sforms[i]) {
	    ComPush(com, sforms[i], NIL, 0, builtin, compile);
	    if (!builtin && !com->macro)
		COM_VARIABLE_ARGUMENT(sforms[i]->data.atom);
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
	int varset;
	LispObj *val, *karg, **keys;

	count = alist->keys.num_symbols;
	symbols = alist->keys.symbols;
	defaults = alist->keys.defaults;
	sforms = alist->keys.sforms;
	keys = alist->keys.keys;

	/* Check if arguments are correctly specified */
	for (karg = values; CONSP(karg); karg = CDR(karg)) {
	    val = CAR(karg);
	    if (KEYWORDP(val)) {
		for (i = 0; i < alist->keys.num_symbols; i++)
		    if (!keys[i] && symbols[i] == val)
			break;
	    }

	    else if (!builtin &&
		     QUOTEP(val) && SYMBOLP(val->data.quote)) {
		for (i = 0; i < alist->keys.num_symbols; i++)
		    if (keys[i] && ATOMID(keys[i]) == ATOMID(val->data.quote))
			break;
	    }

	    else
		/* Just make the error test true */
		i = alist->keys.num_symbols;

	    if (i == alist->keys.num_symbols) {
		/* If not in argument specification list... */
		char function_name[36];

		strcpy(function_name, STROBJ(name));
		LispDestroy("%s: invalid keyword %s",
			    function_name, STROBJ(val));
	    }

	    karg = CDR(karg);
	    if (!CONSP(karg))
		LispDestroy("%s: &KEY needs arguments as pairs",
			    STROBJ(name));
	}

	/* Add variables */
	for (i = 0; i < alist->keys.num_symbols; i++) {
	    val = defaults[i];
	    varset = 0;
	    if (!builtin && keys[i]) {
		Atom_id atom = ATOMID(keys[i]);

		/* Special keyword specification, need to compare ATOMID
		 * and keyword specification must be a quoted object */
		for (karg = values; CONSP(karg); karg = CDR(karg)) {
		    val = CAR(karg);
		    if (QUOTEP(val) && atom == ATOMID(val->data.quote)) {
			val = CADR(karg);
			varset = 1;
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
			break;
		    }
		    karg = CDR(karg);
		}
	    }

	    /* Add the variable to environment */
	    if (varset) {
		ComPush(com, symbols[i], val, eval, builtin, compile);
		if (sforms[i])
		    ComPush(com, sforms[i], T, 0, builtin, compile);
	    }
	    else {
		/* default arguments are evaluated for macros */
		if (!builtin) {
		    int lex = com->lex;
		    int head = lisp__data.env.head;

		    com->lex = base;
		    lisp__data.env.head = lisp__data.env.length;
		    ComPush(com, symbols[i], val, eval, 0, compile);
		    lisp__data.env.head = head;
		    com->lex = lex;
		}
		else
		    ComPush(com, symbols[i], val, eval, builtin, compile);
		if (sforms[i])
		    ComPush(com, sforms[i], NIL, 0, builtin, compile);
	    }
	    if (!builtin && !com->macro) {
		COM_VARIABLE_ARGUMENT(symbols[i]->data.atom);
		if (sforms[i])
		    COM_VARIABLE_ARGUMENT(sforms[i]->data.atom);
	    }
	}
    }

    if (*desc == 'a') {
	/* &KEY uses all remaining arguments */
	values = NIL;
	goto aux_label;
    }
    goto finished_label;


    /* &REST */
rest_label:
    if (!eval || !CONSP(values) || (compile && !builtin))
	ComPush(com, alist->rest, values, eval, builtin, compile);
    else {
	char *string;
	LispObj *list, *car = NIL;
	int count, constantp;

	/* Count number of arguments and check if it is a list of constants */
	for (count = 0, constantp = 1, list = values;
	     CONSP(list);
	     list = CDR(list), count++) {
	    car = CAR(list);
	    if (!ComConstantp(com, car))
		constantp = 0;
	}

	string = builtin ? ATOMID(name) : NULL;
	/* XXX FIXME should have a flag indicating if function call
	 * change the &REST arguments even if it is a constant list
	 * (or if the returned value may be changed). */
	if (string && (count < MAX_BCONS || constantp) &&
	    strcmp(string, "LIST") &&
	    strcmp(string, "APPLY") &&	/* XXX depends on function argument */
	    strcmp(string, "VECTOR") &&
	    /* Append does not copy the last/single list */
	    (strcmp(string, "APPEND") || !CONSP(car))) {
	    if (constantp) {
		/* If the builtin function changes the &REST parameters, must
		 * define a Com_XXX function for it. */
		ComPush(com, alist->rest, values, 0, builtin, compile);
	    }
	    else {
		CompileStackEnter(com, count - 1, 1);
		for (; CONSP(CDR(values)); values = CDR(values)) {
		    /* Evaluate this argument */
		    ComEval(com, CAR(values));
		    /* Save result in builtin stack */
		    com_Bytecode(com, XBC_PUSH);
		}
		CompileStackLeave(com, count - 1, 1);
		/* The last argument is not saved in the stack */
		ComEval(com, CAR(values));
		values = NIL;
		com_Bytecode(com, XBC_BCONS + (count - 1));
	    }
	}
	else {
	    /* Allocate a fresh list of cons */

	    /* Generate code to load object */
	    ComEval(com, CAR(values));

	    com->stack.cpstack += 2;
	    if (com->stack.pstack < com->stack.cpstack)
		com->stack.pstack = com->stack.cpstack;
	    /* Start building a gc protected list, with the loaded value */
	    com_Bytecode(com, XBC_LSTAR);

	    for (values = CDR(values); CONSP(values); values = CDR(values)) {
		/* Generate code to load object */
		ComEval(com, CAR(values));

		/* Add loaded value to gc protected list */
		com_Bytecode(com, XBC_LCONS);
	    }

	    /* Finish gc protected list */
	    com_Bytecode(com, XBC_LFINI);

	    /* Push loaded value */
	    if (builtin)
		com_Bytecode(com, XBC_PUSH);
	    else {
		com_Let(com, alist->rest->data.atom);

		/* Remember this symbol will be bound */
		ComAddVariable(com, alist->rest, values);
	    }
	    com->stack.cpstack -= 2;
	}
    }
    if (!builtin && !com->macro)
	COM_VARIABLE_ARGUMENT(alist->rest->data.atom);
    if (*desc != 'a')
	goto finished_label;


    /* &AUX */
aux_label:
    i = 0;
    count = alist->auxs.num_symbols;
    symbols = alist->auxs.symbols;
    defaults = alist->auxs.initials;
    if (!builtin && !compile) {
	int lex = com->lex;

	com->lex = base;
	lisp__data.env.head = lisp__data.env.length;
	for (; i < count; i++) {
	    ComPush(com, symbols[i], defaults[i], 1, 0, 0);
	    if (!com->macro)
		COM_VARIABLE_ARGUMENT(symbols[i]->data.atom);
	    ++lisp__data.env.head;
	}
	com->lex = lex;
    }
    else {
	for (; i < count; i++) {
	    ComPush(com, symbols[i], defaults[i], eval, builtin, compile);
	    if (!builtin && !com->macro)
		COM_VARIABLE_ARGUMENT(symbols[i]->data.atom);
	}
    }

done_label:
    if (CONSP(values))
	LispDestroy("%s: too many arguments", STROBJ(name));

finished_label:
    if (builtin)
	lisp__data.stack.base = base;
    else
	lisp__data.env.head = lisp__data.env.length;

    return (base);
}

static void
ComFuncall(LispCom *com, LispObj *function, LispObj *arguments, int eval)
{
    int base, compile;
    LispAtom *atom;
    LispArgList *alist;
    LispBuiltin *builtin;
    LispObj *lambda;

    switch (OBJECT_TYPE(function)) {
	case LispFunction_t:
	    function = function->data.atom->object;
	case LispAtom_t:
	    atom = function->data.atom;
	    alist = atom->property->alist;

	    if (atom->a_builtin) {
		builtin = atom->property->fun.builtin;
		compile = builtin->compile != NULL;

		/*  If one of:
		 * 	o expanding a macro
		 *	o calling a builtin special form
		 *	o builtin function is a macro
		 * don't evaluate arguments. */
		if (com->macro || compile || builtin->type == LispMacro)
		    eval = 0;

		if (!com->macro && builtin->type == LispMacro) {
		    /* Set flag of variable used, in case variable is only
		     * used as a builtin macro argument. */
		    LispObj *obj;

		    for (obj = arguments; CONSP(obj); obj = CDR(obj)) {
			if (SYMBOLP(CAR(obj)))
			    COM_VARIABLE_USED(CAR(obj)->data.atom);
		    }
		}

		FORM_ENTER();
		if (!compile && !com->macro)
		    CompileStackEnter(com, alist->num_arguments, 1);

		/* Build argument list in the interpreter stacks */
		base = ComCall(com, alist, function, arguments,
			       eval, 1, compile);

		/* If <compile> is set, it is a special form */
		if (compile)
		    builtin->compile(com, builtin);

		/* Else, generate opcodes to call builtin function */
		else {
		    com_Call(com, alist->num_arguments, builtin);
		    CompileStackLeave(com, alist->num_arguments, 1);
		}
		lisp__data.stack.base = lisp__data.stack.length = base;
		FORM_LEAVE();
	    }
	    else if (atom->a_function) {
		int macro;

		lambda = atom->property->fun.function;
		macro = lambda->funtype == LispMacro;

		/* If <macro> is set, expand macro */
		if (macro)
		    ComMacroCall(com, alist, function, lambda, arguments);

		else {
		    if (com->toplevel->type == LispBlockClosure &&
			com->toplevel->tag == function)
			ComRecursiveCall(com, alist, function, arguments);
		    else {
#if 0
			ComInlineCall(com, alist, function, arguments,
				      lambda->data.lambda.code);
#else
			com_Funcall(com, function, arguments);
#endif
		    }
		}
	    }
	    else if (atom->a_defstruct &&
		     atom->property->structure.function != STRUCT_NAME &&
		     atom->property->structure.function != STRUCT_CONSTRUCTOR) {
		LispObj *definition = atom->property->structure.definition;

		if (!CONSP(arguments) || CONSP(CDR(arguments)))
		    LispDestroy("%s: too %s arguments", atom->string,
				CONSP(arguments) ? "many" : "few");

		ComEval(com, CAR(arguments));
		if (atom->property->structure.function == STRUCT_CHECK)
		    com_Structp(com, definition);
		else
		    com_Struct(com,
			       atom->property->structure.function, definition);
	    }
	    else if (atom->a_compiled) {
		FORM_ENTER();
		CompileStackEnter(com, alist->num_arguments, 0);

		/* Build argument list in the interpreter stacks */
		base = ComCall(com, alist, function, arguments, 1, 0, 0);
		com_Bytecall(com, alist->num_arguments,
			     atom->property->fun.function);
		CompileStackLeave(com, alist->num_arguments, 0);
		lisp__data.env.head = lisp__data.env.length = base;
		FORM_LEAVE();
	    }
	    else {
		/* Not yet defined function/macro. */
		++com->warnings;
		LispWarning("call to undefined function %s", atom->string);
		com_Funcall(com, function, arguments);
	    }
	    break;

	case LispLambda_t:
	    lambda = function->data.lambda.code;
	    alist = (LispArgList*)function->data.lambda.name->data.opaque.data;
	    ComInlineCall(com, alist, NIL, arguments, lambda->data.lambda.code);
	    break;

	case LispCons_t:
	    if (CAR(function) == Olambda) {
		function = EVAL(function);
		if (LAMBDAP(function)) {
		    GC_ENTER();

		    GC_PROTECT(function);
		    lambda = function->data.lambda.code;
		    alist = (LispArgList*)function->data.lambda.name->data.opaque.data;
		    ComInlineCall(com, alist, NIL, arguments, lambda->data.lambda.code);
		    GC_LEAVE();
		    break;
		}
	    }

	default:
	    /*  XXX If bytecode objects are made available, should
	     * handle it here. */
	    LispDestroy("EVAL: %s is invalid as a function",
			STROBJ(function));
	    /*NOTREACHED*/
	    break;
    }
}

/* Generate opcodes for an implicit PROGN */
static void
ComProgn(LispCom *com, LispObj *code)
{
    if (CONSP(code)) {
	for (; CONSP(code); code = CDR(code))
	    ComEval(com, CAR(code));
    }
    else
	/* If no code to execute, empty PROGN returns NIL */
	com_Bytecode(com, XBC_NIL);
}

/* Generate opcodes to evaluate <object>. */
static void
ComEval(LispCom *com, LispObj *object)
{
    int offset;
    LispObj *form;

    switch (OBJECT_TYPE(object)) {
	case LispAtom_t:
	    if (IN_TAGBODY())
		ComLabel(com, object);
	    else {
		offset = ComGetVariable(com, object);
		if (offset >= 0)
		    /* Load from user stack at relative offset */
		    com_Load(com, offset);
		else if (offset == SYMBOL_KEYWORD)
		    com_LoadCon(com, object);
		else if (offset == SYMBOL_CONSTANT)
		    /* Symbol defined as constant, just load it's value */
		    com_LoadCon(com, LispGetVar(object));
		else
		    /* Load value bound to symbol at run time */
		    com_LoadSym(com, object->data.atom);
	    }
	    break;

	case LispCons_t: {
	    /* Macro expansion may be done in the object form */
	    form = com->form;
	    com->form = object;
	    ComFuncall(com, CAR(object), CDR(object), 1);
	    com->form = form;
	}   break;

	case LispQuote_t:
	    com_LoadCon(com, object->data.quote);
	    break;

	case LispBackquote_t:
	    /* Macro expansion is stored in the current value of com->form */
	    ComMacroBackquote(com, object);
	    break;

	case LispComma_t:
	    LispDestroy("EVAL: comma outside of backquote");
	    break;

	case LispFunctionQuote_t:
	    object = object->data.quote;
	    if (SYMBOLP(object))
		object = LispSymbolFunction(object);
	    else if (CONSP(object) && CAR(object) == Olambda) {
		/* object will only be associated with bytecode later,
		 * so, make sure it is protected until compilation finishes */
		object = EVAL(object);
		RPLACD(com->plist, CONS(CAR(com->plist), CDR(com->plist)));
		RPLACA(com->plist, object);
	    }
	    else
		LispDestroy("FUNCTION: %s is not a function", STROBJ(object));
	    com_LoadCon(com, object);
	    break;

	case LispFixnum_t:
	    if (IN_TAGBODY()) {
		ComLabel(com, object);
		break;
	    }
	    /*FALLTROUGH*/

	default:
	    /* Constant object */
	    com_LoadCon(com, object);
	    break;
    }
}

/***********************************************************************
 * Lambda expansion helper functions
 ***********************************************************************/
static void
ComRecursiveCall(LispCom *com, LispArgList *alist,
		 LispObj *name, LispObj *arguments)
{
    int base, lex;

    /* Save state */
    lex = lisp__data.env.lex;

    FORM_ENTER();

    /* Generate code to push function arguments in the stack */
    base = ComCall(com, alist, name, arguments, 1, 0, 0);

    /* Stack will grow this amount */
    CompileStackEnter(com, alist->num_arguments, 0);

#if 0
    /* Make the variables available at run time */
    com_Bind(com, alist->num_arguments);
    com->block->bind += alist->num_arguments;
#endif

    com_BytecodeChar(com, XBC_LETREC, alist->num_arguments);

#if 0
    /* The variables are now unbound */
    com_Unbind(com, alist->num_arguments);
    com->block->bind -= alist->num_arguments;
#endif

    /* Stack length is reduced */
    CompileStackLeave(com, alist->num_arguments, 0);
    FORM_LEAVE();

    /* Restore state */
    lisp__data.env.lex = lex;
    lisp__data.env.head = lisp__data.env.length = base;
}

static void
ComInlineCall(LispCom *com, LispArgList *alist,
	      LispObj *name, LispObj *arguments, LispObj *lambda)
{
    int base, lex;

    /* Save state */
    lex = lisp__data.env.lex;

    FORM_ENTER();
    /* Start the inline function block */
    CompileIniBlock(com, LispBlockClosure, name);

    /* Generate code to push function arguments in the stack */
    base = ComCall(com, alist, name, arguments, 1, 0, 0);

    /* Stack will grow this amount */
    CompileStackEnter(com, alist->num_arguments, 0);

    /* Make the variables available at run time */
    com_Bind(com, alist->num_arguments);
    com->block->bind += alist->num_arguments;

    /* Expand the lambda list */
    ComProgn(com, lambda);

    /* The variables are now unbound */
    com_Unbind(com, alist->num_arguments);
    com->block->bind -= alist->num_arguments;

    /* Stack length is reduced */
    CompileStackLeave(com, alist->num_arguments, 0);

    /* Finish the inline function block */
    CompileFiniBlock(com);
    FORM_LEAVE();

    /* Restore state */
    lisp__data.env.lex = lex;
    lisp__data.env.head = lisp__data.env.length = base;
}

/***********************************************************************
 * Macro expansion helper functions.
 ***********************************************************************/
static LispObj *
ComMacroExpandBackquote(LispCom *com, LispObj *object)
{
    return (LispEvalBackquote(object->data.quote, 1));
}

static LispObj *
ComMacroExpandFuncall(LispCom *com, LispObj *function, LispObj *arguments)
{
    return (LispFuncall(function, arguments, 1));
}

static LispObj *
ComMacroExpandEval(LispCom *com, LispObj *object)
{
    LispObj *result;

    switch (OBJECT_TYPE(object)) {
	case LispAtom_t:
	    result = LispGetVar(object);

	    /* Macro expansion requires bounded symbols */
	    if (result == NULL)
		LispDestroy("EVAL: the variable %s is unbound",
			    STROBJ(object));
	    break;

	case LispCons_t:
	    result = ComMacroExpandFuncall(com, CAR(object), CDR(object));
	    break;

	case LispQuote_t:
	    result = object->data.quote;
	    break;

	case LispBackquote_t:
	    result = ComMacroExpandBackquote(com, object);
	    break;

	case LispComma_t:
	    LispDestroy("EVAL: comma outside of backquote");

	case LispFunctionQuote_t:
	    result = EVAL(object);
	    break;

	default:
	    result = object;
	    break;
    }

    return (result);
}

static LispObj *
ComMacroExpand(LispCom *com, LispObj *lambda)
{
    LispObj *result, **presult = &result, **plambda;
    int jumped, *pjumped = &jumped, backquote, *pbackquote = &backquote;
    LispBlock *block;

    int interpreter_lex, interpreter_head, interpreter_base;

    /* Save interpreter state */
    interpreter_base = lisp__data.stack.length;
    interpreter_head = lisp__data.env.length;
    interpreter_lex = lisp__data.env.lex;

    /* Use the variables */
    plambda = &lambda;
    *presult = NIL;
    *pjumped = 1;
    *pbackquote = !CONSP(lambda);

    block = LispBeginBlock(NIL, LispBlockProtect);
    if (setjmp(block->jmp) == 0) {
	if (!backquote) {
	    for (; CONSP(lambda); lambda = CDR(lambda))
		result = ComMacroExpandEval(com, CAR(lambda));
	}
	else
	    result = ComMacroExpandBackquote(com, lambda);

	*pjumped = 0;
    }
    LispEndBlock(block);

    /* If tried to jump out of the macro expansion block */
    if (!lisp__data.destroyed && jumped)
	LispDestroy("*** EVAL: bad jump in macro expansion");

    /* Macro expansion did something wrong */
    if (lisp__data.destroyed) {
	LispMessage("*** EVAL: aborting macro expansion");
	LispDestroy(".");
    }

    /* Restore interpreter state */
    lisp__data.env.lex = interpreter_lex;
    lisp__data.stack.length = interpreter_base;
    lisp__data.env.head = lisp__data.env.length = interpreter_head;

    return (result);
}

static void
ComMacroCall(LispCom *com, LispArgList *alist,
	     LispObj *name, LispObj *lambda, LispObj *arguments)
{
    int base;
    LispObj *body;

    ++com->macro;
    base = ComCall(com, alist, name, arguments, 0, 0, 0);
    body = lambda->data.lambda.code;
    body = ComMacroExpand(com, body);
    --com->macro;
    lisp__data.env.head = lisp__data.env.length = base;

    /* Macro is expanded, store the result */
    CAR(com->form) = body;
    ComEval(com, body);
}

static void
ComMacroBackquote(LispCom *com, LispObj *lambda)
{
    LispObj *body;

    ++com->macro;
    body = ComMacroExpand(com, lambda);
    --com->macro;

    /* Macro is expanded, store the result */
    CAR(com->form) = body;

    com_LoadCon(com, body);
}
