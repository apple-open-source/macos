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

/* $XFree86: xc/programs/xedit/lisp/pathname.h,v 1.4tsi Exp $ */

#ifndef Lisp_pathname_h
#define Lisp_pathname_h

#include "lisp/internal.h"

#define	PATH_SEP	'/'
#define	PATH_TYPESEP	'.'

#ifndef PATH_MAX
#define PATH_MAX	4096
#endif

#ifndef NAME_MAX
#define NAME_MAX	256
#endif


#define PATH_STRING	0
#define PATH_HOST	1
#define PATH_DEVICE	2
#define PATH_DIRECTORY	3
#define PATH_NAME	4
#define PATH_TYPE	5
#define PATH_VERSION	6

void LispPathnameInit(void);

LispObj *Lisp_Directory(LispBuiltin*);
LispObj *Lisp_Namestring(LispBuiltin*);
LispObj *Lisp_FileNamestring(LispBuiltin*);
LispObj *Lisp_DirectoryNamestring(LispBuiltin*);
LispObj *Lisp_EnoughNamestring(LispBuiltin*);
LispObj *Lisp_HostNamestring(LispBuiltin*);
LispObj *Lisp_MakePathname(LispBuiltin*);
LispObj *Lisp_Pathnamep(LispBuiltin*);
LispObj *Lisp_ParseNamestring(LispBuiltin*);
LispObj *Lisp_PathnameHost(LispBuiltin*);
LispObj *Lisp_PathnameDevice(LispBuiltin*);
LispObj *Lisp_PathnameDirectory(LispBuiltin*);
LispObj *Lisp_PathnameName(LispBuiltin*);
LispObj *Lisp_PathnameType(LispBuiltin*);
LispObj *Lisp_PathnameVersion(LispBuiltin*);
LispObj *Lisp_Truename(LispBuiltin*);
LispObj *Lisp_ProbeFile(LispBuiltin*);
LispObj *Lisp_UserHomedirPathname(LispBuiltin*);

#endif /* Lisp_pathname_h */
