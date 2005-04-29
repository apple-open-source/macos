/* $XFree86: xc/programs/Xserver/hw/xfree86/loader/dlloader.c,v 1.14 2004/02/13 23:58:44 dawes Exp $ */

/*
 * Copyright (c) 1997 The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "Xos.h"
#include "os.h"

#include "sym.h"
#include "loader.h"
#include "dlloader.h"

#ifdef DL_LAZY
#define DLOPEN_LAZY DL_LAZY
#else
#ifdef RTLD_LAZY
#define DLOPEN_LAZY RTLD_LAZY
#else
#ifdef __FreeBSD__
#define DLOPEN_LAZY 1
#else
#define DLOPEN_LAZY 0
#endif
#endif
#endif
#ifdef LD_GLOBAL
#define DLOPEN_GLOBAL LD_GLOBAL
#else
#ifdef RTLD_GLOBAL
#define DLOPEN_GLOBAL RTLD_GLOBAL
#else
#define DLOPEN_GLOBAL 0
#endif
#endif

#define DLOPEN_FLAGS ( DLOPEN_LAZY | DLOPEN_GLOBAL )

#if defined(CSRG_BASED) && !defined(__ELF__)
#define NEED_UNDERSCORE_FOR_DLLSYM
#endif

/*
 * This structure contains all of the information about a module
 * that has been loaded.
 */
typedef struct {
    int handle;
    void *dlhandle;
} DLModuleRec, *DLModulePtr;

/* 
 * a list of loaded modules XXX can be improved
 */
typedef struct DLModuleList {
    DLModulePtr module;
    struct DLModuleList *next;
} DLModuleList;

DLModuleList *dlModuleList = NULL;

/*
 * Search a symbol in the module list
 */
void *
DLFindSymbol(const char *name)
{
    DLModuleList *l;
    void *p;

#ifdef NEED_UNDERSCORE_FOR_DLLSYM
    char *n;

    n = xf86loadermalloc(strlen(name) + 2);
    sprintf(n, "_%s", name);
#endif

    (void)dlerror();		/* Clear out any previous error */
    for (l = dlModuleList; l != NULL; l = l->next) {
#ifdef NEED_UNDERSCORE_FOR_DLLSYM
	p = dlsym(l->module->dlhandle, n);
#else
	p = dlsym(l->module->dlhandle, name);
#endif
	if (dlerror() == NULL) {
#ifdef NEED_UNDERSCORE_FOR_DLLSYM
	    xf86loaderfree(n);
#endif
	    return p;
	}
    }
#ifdef NEED_UNDERSCORE_FOR_DLLSYM
    xf86loaderfree(n);
#endif

    return NULL;
}

/*
 * public interface
 */
void *
DLLoadModule(loaderPtr modrec, int fd, LOOKUP ** ppLookup)
{
    DLModulePtr dlfile;
    DLModuleList *l;

    if ((dlfile = xf86loadercalloc(1, sizeof(DLModuleRec))) == NULL) {
	ErrorF("Unable  to allocate DLModuleRec\n");
	return NULL;
    }
    dlfile->handle = modrec->handle;
    dlfile->dlhandle = dlopen(modrec->name, DLOPEN_FLAGS);
    if (dlfile->dlhandle == NULL) {
	ErrorF("dlopen: %s\n", dlerror());
	xf86loaderfree(dlfile);
	return NULL;
    }
    /* Add it to the module list */
    l = xf86loadermalloc(sizeof(DLModuleList));
    l->module = dlfile;
    l->next = dlModuleList;
    dlModuleList = l;
    *ppLookup = NULL;

    return (void *)dlfile;
}

void
DLResolveSymbols(void *mod)
{
    return;
}

int
DLCheckForUnresolved(void *mod)
{
    return 0;
}

void
DLUnloadModule(void *modptr)
{
    DLModulePtr dlfile = (DLModulePtr) modptr;
    DLModuleList *l, *p;

    /*  remove it from dlModuleList */
    if (dlModuleList->module == modptr) {
	l = dlModuleList;
	dlModuleList = l->next;
	xf86loaderfree(l);
    } else {
	p = dlModuleList;
	for (l = dlModuleList->next; l != NULL; l = l->next) {
	    if (l->module == modptr) {
		p->next = l->next;
		xf86loaderfree(l);
		break;
	    }
	    p = l;
	}
    }
    dlclose(dlfile->dlhandle);
    xf86loaderfree(modptr);
}
