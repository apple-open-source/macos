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

/* $XFree86: xc/programs/xedit/lisp/require.c,v 1.18 2003/04/27 18:17:34 tsi Exp $ */

#include "lisp/require.h"

/*
 * Implementation
 */
LispObj *
Lisp_Load(LispBuiltin *builtin)
/*
 load filename &key verbose print if-does-not-exist
 */
{
    LispObj *filename, *verbose, *print, *if_does_not_exist;

    if_does_not_exist = ARGUMENT(3);
    print = ARGUMENT(2);
    verbose = ARGUMENT(1);
    filename = ARGUMENT(0);

    if (PATHNAMEP(filename))
	filename = CAR(filename->data.pathname);
    else {
	CHECK_STRING(filename);
    }

    return (LispLoadFile(filename,
			 verbose != UNSPEC && verbose != NIL,
			 print != UNSPEC && print != NIL,
			 if_does_not_exist != UNSPEC &&
			 if_does_not_exist != NIL));
}

LispObj *
Lisp_Require(LispBuiltin *builtin)
/*
 require module &optional pathname
 */
{
    char filename[1024], *ext;
    int len;

    LispObj *obj, *module, *pathname;

    pathname = ARGUMENT(1);
    module = ARGUMENT(0);

    CHECK_STRING(module);
    if (pathname != UNSPEC) {
	if (PATHNAMEP(pathname))
	    pathname = CAR(pathname->data.pathname);
	else {
	    CHECK_STRING(pathname);
	}
    }
    else
	pathname = module;

    for (obj = MOD; CONSP(obj); obj = CDR(obj)) {
	if (strcmp(THESTR(CAR(obj)), THESTR(module)) == 0)
	    return (module);
    }

    if (THESTR(pathname)[0] != '/') {
#ifdef LISPDIR
	snprintf(filename, sizeof(filename), "%s", LISPDIR);
#else
	getcwd(filename, sizeof(filename));
#endif
    }
    else
	filename[0] = '\0';
    *(filename + sizeof(filename) - 5) = '\0';	/* make sure there is place for ext */
    len = strlen(filename);
    if (!len || filename[len - 1] != '/') {
	strcat(filename, "/");
	++len;
    }

    snprintf(filename + len, sizeof(filename) - len - 5, "%s", THESTR(pathname));

    ext = filename + strlen(filename);

#ifdef SHARED_MODULES
    strcpy(ext, ".so");
    if (access(filename, R_OK) == 0) {
	LispModule *lisp_module;
	char data[64];
	int len;

	if (lisp__data.module == NULL) {
	    /* export our own symbols */
	    if (dlopen(NULL, RTLD_LAZY | RTLD_GLOBAL) == NULL)
		LispDestroy("%s: ", STRFUN(builtin), dlerror());
	}

	lisp_module = (LispModule*)LispMalloc(sizeof(LispModule));
	if ((lisp_module->handle =
	     dlopen(filename, RTLD_LAZY | RTLD_GLOBAL)) == NULL)
	    LispDestroy("%s: dlopen: %s", STRFUN(builtin), dlerror());
	snprintf(data, sizeof(data), "%sLispModuleData", THESTR(module));
	if ((lisp_module->data =
	     (LispModuleData*)dlsym(lisp_module->handle, data)) == NULL) {
	    dlclose(lisp_module->handle);
	    LispDestroy("%s: cannot find LispModuleData for %s",
			STRFUN(builtin), STROBJ(module));
	}
	LispMused(lisp_module);
	lisp_module->next = lisp__data.module;
	lisp__data.module = lisp_module;
	if (lisp_module->data->load)
	    (lisp_module->data->load)();

	if (MOD == NIL)
	    MOD = CONS(module, NIL);
	else {
	    RPLACD(MOD, CONS(CAR(MOD), CDR(MOD)));
	    RPLACA(MOD, module);
	}
	LispSetVar(lisp__data.modules, MOD);

	return (module);
    }
#endif

    strcpy(ext, ".lsp");
    (void)LispLoadFile(STRING(filename), 0, 0, 0);

    return (module);
}
