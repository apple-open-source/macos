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

/* $XFree86: xc/programs/xedit/lisp/struct.c,v 1.22tsi Exp $ */

#include "lisp/struct.h"

/*
 * Prototypes
 */
static LispObj *LispStructAccessOrStore(LispBuiltin*, int);

/*
 * Initialization
 */
LispObj *Omake_struct, *Ostruct_access, *Ostruct_store, *Ostruct_type;

Atom_id Smake_struct, Sstruct_access, Sstruct_store, Sstruct_type;

/*
 * Implementation
 */
LispObj *
Lisp_Defstruct(LispBuiltin *builtin)
/*
 defstruct name &rest description
 */
{
    int intern;
    LispAtom *atom;
    int i, size, length, slength;
    char *name, *strname, *sname;
    LispObj *list, *cons, *object, *definition, *documentation;

    LispObj *oname, *description;

    description = ARGUMENT(1);
    oname = ARGUMENT(0);

    CHECK_SYMBOL(oname);

    strname = ATOMID(oname);
    length = strlen(strname);

	    /* MAKE- */
    size = length + 6;
    name = LispMalloc(size);

    sprintf(name, "MAKE-%s", strname);
    atom = (object = ATOM(name))->data.atom;

    if (atom->a_builtin)
	LispDestroy("%s: %s cannot be a structure name",
		    STRFUN(builtin), STROBJ(oname));

    intern = !atom->ext;

    if (CONSP(description) && STRINGP(CAR(description))) {
	documentation = CAR(description);
	description = CDR(description);
    }
    else
	documentation = NIL;

    /* get structure fields and default values */
    for (list = description; CONSP(list); list = CDR(list)) {
	object = CAR(list);

	cons = list;
	if (CONSP(object)) {
	    if ((CONSP(CDR(object)) && CDR(CDR(object)) != NIL) ||
		(!CONSP(CDR(object)) && CDR(object) != NIL))
	    LispDestroy("%s: bad initialization %s",
			STRFUN(builtin), STROBJ(object));
	    cons = object;
	    object = CAR(object);
	}
	if (!SYMBOLP(object) || strcmp(ATOMID(object), "P") == 0)
	    /* p is invalid as a field name due to `type'-p */
	    LispDestroy("%s: %s cannot be a field for %s",
			STRFUN(builtin), STROBJ(object), ATOMID(oname));

	if (!KEYWORDP(object))
	    CAR(cons) = KEYWORD(ATOMID(object));

	/* check for repeated field names */
	for (object = description; object != list; object = CDR(object)) {
	    LispObj *left = CAR(object), *right = CAR(list);

	    if (CONSP(left))
		left = CAR(left);
	    if (CONSP(right))
		right = CAR(right);

	    if (ATOMID(left) == ATOMID(right))
		LispDestroy("%s: only one slot named %s allowed",
			    STRFUN(builtin), STROBJ(left));
	}
    }

    /* atom should not have been modified */
    definition = CONS(oname, description);
    LispSetAtomStructProperty(atom, definition, STRUCT_CONSTRUCTOR);
    if (!intern)
	LispExportSymbol(object);

    atom = oname->data.atom;
    if (atom->a_defstruct)
	LispWarning("%s: structure %s is being redefined",
		    STRFUN(builtin), strname);
    LispSetAtomStructProperty(atom, definition, STRUCT_NAME);

    sprintf(name, "%s-P", strname);
    atom = (object = ATOM(name))->data.atom;
    LispSetAtomStructProperty(atom, definition, STRUCT_CHECK);
    if (!intern)
	LispExportSymbol(object);

    for (i = 0, list = description; CONSP(list); i++, list = CDR(list)) {
	if (CONSP(CAR(list)))
	    sname = ATOMID(CAR(CAR(list)));
	else
	    sname = ATOMID(CAR(list));
	slength = strlen(sname);
	if (length + slength + 2 > size) {
	    size = length + slength + 2;
	    name = LispRealloc(name, size);
	}
	sprintf(name, "%s-%s", strname, sname);
	atom = (object = ATOM(name))->data.atom;
	LispSetAtomStructProperty(atom, definition, i);
	if (!intern)
	    LispExportSymbol(object);
    }

    LispFree(name);

    if (documentation != NIL)
	LispAddDocumentation(oname, documentation, LispDocStructure);

    return (oname);
}

/* helper functions
 *	DONT explicitly call them. Non standard functions.
 */
LispObj *
Lisp_XeditMakeStruct(LispBuiltin *builtin)
/*
 lisp::make-struct atom &rest init
 */
{
    int nfld, ncvt, length = lisp__data.protect.length;
    LispAtom *atom = NULL;

    LispObj *definition, *object, *field, *fields, *value = NIL, *cons, *list;
    LispObj *struc, *init;

    init = ARGUMENT(1);
    struc = ARGUMENT(0);

    field = cons = NIL;
    if (!POINTERP(struc) ||
	!(XSYMBOLP(struc) || XFUNCTIONP(struc)) ||
	(atom = struc->data.atom)->a_defstruct == 0 ||
	 atom->property->structure.function != STRUCT_CONSTRUCTOR)
	LispDestroy("%s: invalid constructor %s",
		    STRFUN(builtin), STROBJ(struc));
    definition = atom->property->structure.definition;

    ncvt = nfld = 0;
    fields = NIL;

    /* check for errors in argument list */
    for (list = init, nfld = 0; CONSP(list); list = CDR(list)) {
	CHECK_KEYWORD(CAR(list));
	if (!CONSP(CDR(list)))
	    LispDestroy("%s: values must be provided as pairs",
			ATOMID(struc));
	nfld++;
	list = CDR(list);
    }

    /* create structure, CAR(definition) is structure name */
    for (list = CDR(definition); CONSP(list); list = CDR(list)) {
	Atom_id id;
	LispObj *defvalue = NIL;

	++nfld;
	field = CAR(list);
	if (CONSP(field)) {
	    /* if default value provided */
	    if (CONSP(CDR(field)))
		defvalue = CAR(CDR(field));
	    field = CAR(field);
	}
	id = ATOMID(field);

	for (object = init; CONSP(object); object = CDR(object)) {
	    /* field is a keyword, test above checked it */
	    field = CAR(object);
	    if (id == ATOMID(field)) {
		/* value provided */
		value = CAR(CDR(object));
		ncvt++;
		break;
	    }
	    object = CDR(object);
	}

	/* if no initialization given */
	if (!CONSP(object)) {
	    /* if default value in structure definition */
	    if (defvalue != NIL)
		value = EVAL(defvalue);
	    else
		value = NIL;
	}

	if (fields == NIL) {
	    fields = cons = CONS(value, NIL);
	    if (length + 1 >= lisp__data.protect.space)
		LispMoreProtects();
	    lisp__data.protect.objects[lisp__data.protect.length++] = fields;
	}
	else {
	    RPLACD(cons, CONS(value, NIL));
	    cons = CDR(cons);
	}
    }

    /* if not enough arguments were converted, need to check because
     * it is acceptable to set a field more than once, but in that case,
     * only the first value will be used. */
    if (nfld > ncvt) {
	for (list = init; CONSP(list); list = CDR(list)) {
	    Atom_id id = ATOMID(CAR(list));

	    for (object = CDR(definition); CONSP(object);
		 object = CDR(object)) {
		field = CAR(object);
		if (CONSP(field))
		    field = CAR(field);
		if (ATOMID(field) == id)
		    break;
	    }
	    if (!CONSP(object))
		LispDestroy("%s: %s is not a field for %s",
			    ATOMID(struc), STROBJ(CAR(list)),
			    ATOMID(CAR(definition)));
	    list = CDR(list);
	}
    }

    lisp__data.protect.length = length;

    return (STRUCT(fields, definition));
}

static LispObj *
LispStructAccessOrStore(LispBuiltin *builtin, int store)
/*
 lisp::struct-access atom struct
 lisp::struct-store atom struct value
 */
{
    long offset;
    LispAtom *atom;
    LispObj *definition, *list;

    LispObj *name, *struc, *value = NIL;

    if (store)
	value = ARGUMENT(2);
    struc = ARGUMENT(1);
    name = ARGUMENT(0);

    if (!POINTERP(name) ||
	!(XSYMBOLP(name) || XFUNCTIONP(name)) ||
	(atom = name->data.atom)->a_defstruct == 0 ||
	(offset = atom->property->structure.function) < 0) {
	LispDestroy("%s: invalid argument %s",
		    STRFUN(builtin), STROBJ(name));
	/*NOTREACHED*/
	offset = 0;
	atom = NULL;
    }
    definition = atom->property->structure.definition;

    /* check if the object is of the required type */
    if (!STRUCTP(struc) || struc->data.struc.def != definition)
	LispDestroy("%s: %s is not a %s",
		    ATOMID(name), STROBJ(struc), ATOMID(CAR(definition)));

    for (list = struc->data.struc.fields; offset; list = CDR(list), offset--)
	;

    return (store ? RPLACA(list, value) : CAR(list));
}

LispObj *
Lisp_XeditStructAccess(LispBuiltin *builtin)
/*
 lisp::struct-access atom struct
 */
{
    return (LispStructAccessOrStore(builtin, 0));
}

LispObj *
Lisp_XeditStructStore(LispBuiltin *builtin)
/*
 lisp::struct-store atom struct value
 */
{
    return (LispStructAccessOrStore(builtin, 1));
}

LispObj *
Lisp_XeditStructType(LispBuiltin *builtin)
/*
 lisp::struct-type atom struct
 */
{
    LispAtom *atom = NULL;

    LispObj *definition, *struc, *name;

    struc = ARGUMENT(1);
    name = ARGUMENT(0);

    if (!POINTERP(name) ||
	!(XSYMBOLP(name) || XFUNCTIONP(name)) ||
	(atom = name->data.atom)->a_defstruct == 0 ||
	(atom->property->structure.function != STRUCT_CHECK))
	LispDestroy("%s: invalid argument %s",
		    STRFUN(builtin), STROBJ(name));
    definition = atom->property->structure.definition;

    /* check if the object is of the required type */
    if (STRUCTP(struc) && struc->data.struc.def == definition)
	return (T);

    return (NIL);
}
