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

/* $XFree86: xc/programs/xedit/lisp/package.c,v 1.21 2003/04/27 18:17:34 tsi Exp $ */

#include "lisp/package.h"
#include "lisp/private.h"

/*
 * Prototypes
 */
static int LispDoSymbol(LispObj*, LispAtom*, int, int);
static LispObj *LispReallyDoSymbols(LispBuiltin*, int, int);
static LispObj *LispDoSymbols(LispBuiltin*, int, int);
static LispObj *LispFindSymbol(LispBuiltin*, int);
static LispObj *LispFindPackageOrDie(LispBuiltin*, LispObj*);
static void LispDoExport(LispBuiltin*, LispObj*, LispObj*, int);
static void LispDoImport(LispBuiltin*, LispObj*);

/*
 * Initialization
 */
extern LispProperty *NOPROPERTY;
static LispObj *Kinternal, *Kexternal, *Kinherited;

/*
 * Implementation
 */
void
LispPackageInit(void)
{
    Kinternal	= KEYWORD("INTERNAL");
    Kexternal	= KEYWORD("EXTERNAL");
    Kinherited	= KEYWORD("INHERITED");
}

LispObj *
LispFindPackageFromString(char *string)
{
    LispObj *list, *package, *nick;

    for (list = PACK; CONSP(list); list = CDR(list)) {
	package = CAR(list);
	if (strcmp(THESTR(package->data.package.name), string) == 0)
	    return (package);
	for (nick = package->data.package.nicknames;
	     CONSP(nick); nick = CDR(nick))
	    if (strcmp(THESTR(CAR(nick)), string) == 0)
		return (package);
    }

    return (NIL);
}

LispObj *
LispFindPackage(LispObj *name)
{
    char *string = NULL;

    if (PACKAGEP(name))
	return (name);

    if (SYMBOLP(name))
	string = ATOMID(name);
    else if (STRINGP(name))
	string = THESTR(name);
    else
	LispDestroy("FIND-PACKAGE: %s is not a string or symbol", STROBJ(name));

    return (LispFindPackageFromString(string));
}

int
LispCheckAtomString(char *string)
{
    char *ptr;

    if (*string == '\0')
	return (0);

    for (ptr = string; *ptr; ptr++) {
	if (islower(*ptr) || strchr("\"\\;#()`'|:", *ptr) ||
	    ((ptr == string || ptr[1] == '\0') && strchr(".,@", *ptr)))
	    return (0);
    }

    return (1);
}

/*   This function is used to avoid some namespace polution caused by the
 * way builtin functions are created, all function name arguments enter
 * the current package, but most of them do not have a property */
static int
LispDoSymbol(LispObj *package, LispAtom *atom, int if_extern, int all_packages)
{
    int dosymbol;

    /* condition 1: atom package is current package */
    dosymbol = !all_packages || atom->package == package;
    if (dosymbol) {
	/* condition 2: intern and extern symbols or symbol is extern */
	dosymbol = !if_extern || atom->ext;
	if (dosymbol) {
	    /* condition 3: atom has properties or is in
	     * the current package */
	    dosymbol = atom->property != NOPROPERTY ||
		       package == lisp__data.keyword ||
		       package == PACKAGE;
	}
    }

    return (dosymbol);
}

static LispObj *
LispFindPackageOrDie(LispBuiltin *builtin, LispObj *name)
{
    LispObj *package;

    package = LispFindPackage(name);

    if (package == NIL)
	LispDestroy("%s: package %s is not available",
		    STRFUN(builtin), STROBJ(name));

    return (package);
}

/* package must be of type LispPackage_t, symbol type is checked
   bypass lisp.c:LispExportSymbol() */
static void
LispDoExport(LispBuiltin *builtin,
	     LispObj *package, LispObj *symbol, int export)
{
    CHECK_SYMBOL(symbol);
    if (!export) {
	if (package == lisp__data.keyword ||
	    symbol->data.atom->package == lisp__data.keyword)
	    LispDestroy("%s: symbol %s cannot be unexported",
			STRFUN(builtin), STROBJ(symbol));
    }

    if (package == PACKAGE)
	symbol->data.atom->ext = export ? 1 : 0;
    else {
	int i;
	char *string;
	LispAtom *atom;
	LispPackage *pack;

	string = ATOMID(symbol);
	pack = package->data.package.package;
	i = STRHASH(string);
	atom = pack->atoms[i];
	while (atom) {
	    if (strcmp(atom->string, string) == 0) {
		atom->ext = export ? 1 : 0;
		return;
	    }

	    atom = atom->next;
	}

	LispDestroy("%s: the symbol %s is not available in package %s",
		    STRFUN(builtin), STROBJ(symbol),
		    THESTR(package->data.package.name));
    }
}

static void
LispDoImport(LispBuiltin *builtin, LispObj *symbol)
{
    CHECK_SYMBOL(symbol);
    LispImportSymbol(symbol);
}

static LispObj *
LispReallyDoSymbols(LispBuiltin *builtin, int only_externs, int all_symbols)
{
    int i, head = lisp__data.env.length;
    LispPackage *pack = NULL;
    LispAtom *atom, *next_atom;
    LispObj *variable, *package = NULL, *list, *code, *result_form;

    LispObj *init, *body;

    body = ARGUMENT(1);
    init = ARGUMENT(0);

    /* Prepare for loop */
    CHECK_CONS(init);
    variable = CAR(init);
    CHECK_SYMBOL(variable);

    if (!all_symbols) {
	/* if all_symbols, a package name is not specified in the init form */

	init = CDR(init);
	if (!CONSP(init))
	    LispDestroy("%s: missing package name", STRFUN(builtin));

	/* Evaluate package specification */
	package = EVAL(CAR(init));
	if (!PACKAGEP(package))
	    package = LispFindPackageOrDie(builtin, package);

	pack = package->data.package.package;
    }

    result_form = NIL;

    init = CDR(init);
    if (CONSP(init))
	result_form = init;

    /* Initialize iteration variable */
    CHECK_CONSTANT(variable);
    LispAddVar(variable, NIL);
    ++lisp__data.env.head;

    for (list = PACK; CONSP(list); list = CDR(list)) {
	if (all_symbols) {
	    package = CAR(list);
	    pack = package->data.package.package;
	}

	/* Traverse the symbol list, executing body */
	for (i = 0; i < STRTBLSZ; i++) {
	    atom = pack->atoms[i];
	    while (atom) {
		/* Save pointer to next atom. If variable is removed,
		 * predicatable result is only guaranteed if the bound
		 * variable is removed. */
		next_atom = atom->next;

		if (LispDoSymbol(package, atom, only_externs, all_symbols)) {
		    LispSetVar(variable, atom->object);
		    for (code = body; CONSP(code); code = CDR(code))
			EVAL(CAR(code));
		}

		atom = next_atom;
	    }
	}

	if (!all_symbols)
	    break;
    }

    /* Variable is still bound */
    for (code = result_form; CONSP(code); code = CDR(code))
	EVAL(CAR(code));

    lisp__data.env.head = lisp__data.env.length = head;

    return (NIL);
}

static LispObj *
LispDoSymbols(LispBuiltin *builtin, int only_externs, int all_symbols)
{
    int did_jump, *pdid_jump = &did_jump;
    LispObj *result, **presult = &result;
    LispBlock *block;

    *presult = NIL;
    *pdid_jump = 1;
    block = LispBeginBlock(NIL, LispBlockTag);
    if (setjmp(block->jmp) == 0) {
	*presult = LispReallyDoSymbols(builtin, only_externs, all_symbols);
	*pdid_jump = 0;
    }
    LispEndBlock(block);
    if (*pdid_jump)
	*presult = lisp__data.block.block_ret;

    return (*presult);
}

LispObj *
LispFindSymbol(LispBuiltin *builtin, int intern)
{
    int i;
    char *ptr;
    LispAtom *atom;
    LispObj *symbol;
    LispPackage *pack;

    LispObj *string, *package;

    package = ARGUMENT(1);
    string = ARGUMENT(0);

    CHECK_STRING(string);
    if (package != UNSPEC)
	package = LispFindPackageOrDie(builtin, package);
    else
	package = PACKAGE;

    /* If got here, package is a LispPackage_t */
    pack = package->data.package.package;

    /* Search symbol in specified package */
    ptr = THESTR(string);

    RETURN_COUNT = 1;

    symbol = NULL;
    /* Fix for current behaviour where NIL and T aren't symbols... */
    if (STRLEN(string) == 3 && memcmp(ptr, "NIL", 3) == 0)
	symbol = NIL;
    else if (STRLEN(string) == 1 && ptr[0] == 'T')
	symbol = T;
    if (symbol) {
	RETURN(0) = NIL;
	return (symbol);
    }

    i = STRHASH(ptr);
    atom = pack->atoms[i];
    while (atom) {
	if (strcmp(atom->string, ptr) == 0) {
	    symbol = atom->object;
	    break;
	}
	atom = atom->next;
    }

    if (symbol == NULL || symbol->data.atom->package == NULL) {
	RETURN(0) = NIL;
	if (intern) {
	    /* symbol does not exist in the specified package, create a new
	     * internal symbol */

	    if (package == PACKAGE)
		symbol = ATOM(ptr);
	    else {
		LispPackage *savepack;
		LispObj *savepackage;

		/* Save package environment */
		savepackage = PACKAGE;
		savepack = lisp__data.pack;

		/* Change package environment */
		PACKAGE = package;
		lisp__data.pack = package->data.package.package;

		symbol = ATOM(ptr);

		/* Restore package environment */
		PACKAGE = savepackage;
		lisp__data.pack = savepack;
	    }

	    symbol->data.atom->unreadable = !LispCheckAtomString(ptr);
	    /* If symbol being create in the keyword package, make it external */
	    if (package == lisp__data.keyword)
		symbol->data.atom->ext = symbol->data.atom->constant = 1;
	}
	else
	    symbol = NIL;
    }
    else {
	if (symbol->data.atom->package == package)
	    RETURN(0) = symbol->data.atom->ext ? Kexternal : Kinternal;
	else
	    RETURN(0) = Kinherited;
    }

    return (symbol);
}


LispObj *
Lisp_DoAllSymbols(LispBuiltin *builtin)
/*
 do-all-symbols init &rest body
 */
{
    return (LispDoSymbols(builtin, 0, 1));
}

LispObj *
Lisp_DoExternalSymbols(LispBuiltin *builtin)
/*
 do-external-symbols init &rest body
 */
{
    return (LispDoSymbols(builtin, 1, 0));
}

LispObj *
Lisp_DoSymbols(LispBuiltin *builtin)
/*
 do-symbols init &rest body
 */
{
    return (LispDoSymbols(builtin, 0, 0));
}

LispObj *
Lisp_FindAllSymbols(LispBuiltin *builtin)
/*
 find-all-symbols string-or-symbol
 */
{
    GC_ENTER();
    char *string = NULL;
    LispAtom *atom;
    LispPackage *pack;
    LispObj *list, *package, *result;
    int i;

    LispObj *string_or_symbol;

    string_or_symbol = ARGUMENT(0);

    if (STRINGP(string_or_symbol))
	string = THESTR(string_or_symbol);
    else if (SYMBOLP(string_or_symbol))
	string = ATOMID(string_or_symbol);
    else
	LispDestroy("%s: %s is not a string or symbol",
		    STRFUN(builtin), STROBJ(string_or_symbol));

    result = NIL;
    i = STRHASH(string);

    /* Traverse all packages, searching for symbols matching specified string */
    for (list = PACK; CONSP(list); list = CDR(list)) {
	package = CAR(list);
	pack = package->data.package.package;

	atom = pack->atoms[i];
	while (atom) {
	    if (strcmp(atom->string, string) == 0 &&
		LispDoSymbol(package, atom, 0, 1)) {
		/* Return only one pointer to a matching symbol */

		if (result == NIL) {
		    result = CONS(atom->object, NIL);
		    GC_PROTECT(result);
		}
		else {
		    /* Put symbols defined first in the
		     * beginning of the result list */
		    RPLACD(result, CONS(CAR(result), CDR(result)));
		    RPLACA(result, atom->object);
		}
	    }
	    atom = atom->next;
	}
    }
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_FindSymbol(LispBuiltin *builtin)
/*
 find-symbol string &optional package
 */
{
    return (LispFindSymbol(builtin, 0));
}

LispObj *
Lisp_FindPackage(LispBuiltin *builtin)
/*
 find-package name
 */
{
    LispObj *name;

    name = ARGUMENT(0);

    return (LispFindPackage(name));
}

LispObj *
Lisp_Export(LispBuiltin *builtin)
/*
 export symbols &optional package
 */
{
    LispObj *list;

    LispObj *symbols, *package;

    package = ARGUMENT(1);
    symbols = ARGUMENT(0);

    /* If specified, make sure package is available */
    if (package != UNSPEC)
	package = LispFindPackageOrDie(builtin, package);
    else
	package = PACKAGE;

    /* Export symbols */
    if (CONSP(symbols)) {
	for (list = symbols; CONSP(list); list = CDR(list))
	    LispDoExport(builtin, package, CAR(list), 1);
    }
    else
	LispDoExport(builtin, package, symbols, 1);

    return (T);
}

LispObj *
Lisp_Import(LispBuiltin *builtin)
/*
 import symbols &optional package
 */
{
    int restore_package;
    LispPackage *savepack = NULL;
    LispObj *list, *savepackage = NULL;

    LispObj *symbols, *package;

    package = ARGUMENT(1);
    symbols = ARGUMENT(0);

    /* If specified, make sure package is available */
    if (package != UNSPEC)
	package = LispFindPackageOrDie(builtin, package);
    else
	package = PACKAGE;

    restore_package = package != PACKAGE;
    if (restore_package) {
	/* Save package environment */
	savepackage = PACKAGE;
	savepack = lisp__data.pack;

	/* Change package environment */
	PACKAGE = package;
	lisp__data.pack = package->data.package.package;
    }

    /* Export symbols */
    if (CONSP(symbols)) {
	for (list = symbols; CONSP(list); list = CDR(list))
	    LispDoImport(builtin, CAR(list));
    }
    else
	LispDoImport(builtin, symbols);

    if (restore_package) {
	/* Restore package environment */
	PACKAGE = savepackage;
	lisp__data.pack = savepack;
    }

    return (T);
}

LispObj *
Lisp_InPackage(LispBuiltin *builtin)
/*
 in-package name
 */
{
    LispObj *package;

    LispObj *name;

    name = ARGUMENT(0);

    package = LispFindPackageOrDie(builtin, name);

    /* Update pointer to package symbol table */
    lisp__data.pack = package->data.package.package;
    PACKAGE = package;

    return (package);
}

LispObj *
Lisp_Intern(LispBuiltin *builtin)
/*
 intern string &optional package
 */
{
    return (LispFindSymbol(builtin, 1));
}

LispObj *
Lisp_ListAllPackages(LispBuiltin *builtin)
/*
 list-all-packages
 */
{
    /*   Maybe this should be read-only or a copy of the package list.
     *   But, if properly implemented, it should be possible to (rplaca)
     * this variable from lisp code with no problems. Don't do it at home. */

    return (PACK);
}

LispObj *
Lisp_MakePackage(LispBuiltin *builtin)
/*
 make-package package-name &key nicknames use
 */
{
    GC_ENTER();
    LispObj *list, *package, *nicks, *cons, *savepackage;

    LispObj *package_name, *nicknames, *use;

    use = ARGUMENT(2);
    nicknames = ARGUMENT(1);
    package_name = ARGUMENT(0);

    /* Check if package already exists */
    package = LispFindPackage(package_name);
    if (package != NIL)
	/* FIXME: this should be a correctable error */
	LispDestroy("%s: package %s already defined",
		    STRFUN(builtin), STROBJ(package_name));

    /* Error checks done, package_name is either a symbol or string */
    if (!XSTRINGP(package_name))
	package_name = STRING(ATOMID(package_name));

    GC_PROTECT(package_name);

    /* Check nicknames */
    nicks = cons = NIL;
    for (list = nicknames; CONSP(list); list = CDR(list)) {
	package = LispFindPackage(CAR(list));
	if (package != NIL)
	    /* FIXME: this should be a correctable error */
	    LispDestroy("%s: nickname %s matches package %s",
			STRFUN(builtin), STROBJ(CAR(list)),
			THESTR(package->data.package.name));
	/* Store all nicknames as strings */
	package = CAR(list);
	if (!XSTRINGP(package))
	    package = STRING(ATOMID(package));
	if (nicks == NIL) {
	    nicks = cons = CONS(package, NIL);
	    GC_PROTECT(nicks);
	}
	else {
	    RPLACD(cons, CONS(package, NIL));
	    cons = CDR(cons);
	}
    }

    /* Check use list */
    for (list = use; CONSP(list); list = CDR(list))
	(void)LispFindPackageOrDie(builtin, CAR(list));

    /* No errors, create new package */
    package = LispNewPackage(package_name, nicks);

    /* Update list of packages */
    PACK = CONS(package, PACK);

    /* No need for gc protection anymore */
    GC_LEAVE();

    /* Import symbols from use list */
    savepackage = PACKAGE;

    /* Update pointer to package symbol table */
    lisp__data.pack = package->data.package.package;
    PACKAGE = package;

    if (use != UNSPEC) {
	for (list = use; CONSP(list); list = CDR(list))
	    LispUsePackage(LispFindPackage(CAR(list)));
    }
    else
	LispUsePackage(lisp__data.lisp);

    /* Restore pointer to package symbol table */
    lisp__data.pack = savepackage->data.package.package;
    PACKAGE = savepackage;

    return (package);
}

LispObj *
Lisp_Packagep(LispBuiltin *builtin)
/*
 packagep object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (PACKAGEP(object) ? T : NIL);
}

LispObj *
Lisp_PackageName(LispBuiltin *builtin)
/*
 package-name package
 */
{
    LispObj *package;

    package = ARGUMENT(0);

    package = LispFindPackageOrDie(builtin, package);

    return (package->data.package.name);
}

LispObj *
Lisp_PackageNicknames(LispBuiltin *builtin)
/*
 package-nicknames package
 */
{
    LispObj *package;

    package = ARGUMENT(0);

    package = LispFindPackageOrDie(builtin, package);

    return (package->data.package.nicknames);
}

LispObj *
Lisp_PackageUseList(LispBuiltin *builtin)
/*
 package-use-list package
 */
{
    /*  If the variable returned by this function is expected to be changeable,
     * need to change the layout of the LispPackage structure. */

    LispPackage *pack;
    LispObj *package, *use, *cons;

    package = ARGUMENT(0);

    package = LispFindPackageOrDie(builtin, package);

    use = cons = NIL;
    pack = package->data.package.package;

    if (pack->use.length) {
	GC_ENTER();
	int i = pack->use.length - 1;

	use = cons = CONS(pack->use.pairs[i], NIL);
	GC_PROTECT(use);
	for (--i; i >= 0; i--) {
	    RPLACD(cons, CONS(pack->use.pairs[i], NIL));
	    cons = CDR(cons);
	}
	GC_LEAVE();
    }

    return (use);
}

LispObj *
Lisp_PackageUsedByList(LispBuiltin *builtin)
/*
 package-used-by-list package
 */
{
    GC_ENTER();
    int i;
    LispPackage *pack;
    LispObj *package, *other, *used, *cons, *list;

    package = ARGUMENT(0);

    package = LispFindPackageOrDie(builtin, package);

    used = cons = NIL;

    for (list = PACK; CONSP(list); list = CDR(list)) {
	other = CAR(list);
	if (package == other)
	    /* Surely package uses itself */
	    continue;

	pack = other->data.package.package;

	for (i = 0; i < pack->use.length; i++) {
	    if (pack->use.pairs[i] == package) {
		if (used == NIL) {
		    used = cons = CONS(other, NIL);
		    GC_PROTECT(used);
		}
		else {
		    RPLACD(cons, CONS(other, NIL));
		    cons = CDR(cons);
		}
	    }
	}
    }

    GC_LEAVE();

    return (used);
}

LispObj *
Lisp_Unexport(LispBuiltin *builtin)
/*
 unexport symbols &optional package
 */
{
    LispObj *list;

    LispObj *symbols, *package;

    package = ARGUMENT(1);
    symbols = ARGUMENT(0);

    /* If specified, make sure package is available */
    if (package != UNSPEC)
	package = LispFindPackageOrDie(builtin, package);
    else
	package = PACKAGE;

    /* Export symbols */
    if (CONSP(symbols)) {
	for (list = symbols; CONSP(list); list = CDR(list))
	    LispDoExport(builtin, package, CAR(list), 0);
    }
    else
	LispDoExport(builtin, package, symbols, 0);

    return (T);
}
