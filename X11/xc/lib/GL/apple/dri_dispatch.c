/* dri_dispatch.c
   $Id: dri_dispatch.c,v 1.13 2003/07/23 17:58:02 jharper Exp $

   Copyright (c) 2002 Apple Computer, Inc. All rights reserved.

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation files
   (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL THE THE ABOVE LISTED COPYRIGHT
   HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name(s) of the above
   copyright holders shall not be used in advertising or otherwise to
   promote the sale, use or other dealings in this Software without
   prior written authorization. */

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLContext.h>

#include "glapi.h"
#include "glapitable.h"
#include "glxclient.h"

#include <Xlibint.h>
#include <stdio.h>

#ifdef __GNUC__
# define GCC_VERSION (__GNUC__ * 1000 + __GNUC_MINOR__)
#else
# define GCC_VERSION 0
#endif

#if GCC_VERSION < 3000
# define __builtin_expect(a, b) a
#endif

static int indirect_noop (void)
{
    return 0;
}

/* Macro used for gl functions that exist in OpenGL.framework. We'll
   use the existing stub for the initial dispatch, but need this
   function to handle indirect contexts. */
#define DEFUN_LOCAL_1(ret, return, gn, proto, args)		\
static ret indirect__ ## gn proto				\
{								\
    struct _glapi_table *disp;					\
								\
    disp = _glapi_Dispatch;					\
    if (__builtin_expect (disp == NULL, 0))			\
	disp = _glapi_get_dispatch ();				\
								\
    return (*disp->gn) args;					\
}

/* Macro used for gl functions that don't exist in OpenGL.framework.
   We drop them on the floor in direct rendering mode, but pass them
   over the wire normally for indirect contexts. */
#define DEFUN_EXTERN_1(ret, return, gn, proto, args)		\
ret gl ## gn proto						\
{								\
    __GLXcontext *gc;						\
    struct _glapi_table *disp;					\
								\
    gc = __glXGetCurrentContext ();				\
    if (!gc->isDirect) {					\
	disp = _glapi_Dispatch;					\
	if (__builtin_expect (disp == NULL, 0))			\
	    disp = _glapi_get_dispatch ();			\
								\
	return (*disp->gn) args;				\
    } else {							\
	int a; return a = 0;					\
    }								\
}

/* Macro for functions that already exist, but with a different name. */
#define DEFUN_ALIAS_1(ret, return, gn, on, proto, args)	\
ret gl ## gn proto					\
{							\
    return gl ## on args;				\
}

#define DEFUN_LOCAL(r, gn, p, a) \
    DEFUN_LOCAL_1 (r, return, gn, p, a)
#define DEFUN_LOCAL_VOID(gn, p, a) \
    DEFUN_LOCAL_1 (void, , gn, p, a)

#define DEFUN_EXTERN(r, gn, p, a) \
    DEFUN_EXTERN_1 (r, return, gn, p, a)
#define DEFUN_EXTERN_VOID(gn, p, a) \
    DEFUN_EXTERN_1 (void, , gn, p, a)

#define DEFUN_ALIAS(r, gn, on, p, a) \
    DEFUN_ALIAS_1 (r, return, gn, on, p, a)
#define DEFUN_ALIAS_VOID(gn, on, p, a) \
    DEFUN_ALIAS_1 (void, , gn, on, p, a)

#include "dri_dispatch.h"

__private_extern__ const CGLContextObj
XAppleDRIGetIndirectContext (void)
{
    static CGLContextObj ctx;

    void **t;
    int i;

    if (ctx != NULL)
	return ctx;

    /* initialize gl */
    CGLSetOption (kCGLGOResetLibrary, 0);

    /* create an empty "context" for dispatching purposes. add some slop
       in case the dispatch table grows in future updates. */
    ctx = Xcalloc (1, sizeof (struct _CGLContextObject) + 1024);

    /* fill it with no-op vectors */
    t = (void **) &ctx->disp;
    for (i = 0; i < (int) (sizeof (ctx->disp) / sizeof (t[0])); i++)
	t[i] = &indirect_noop;

    /* then install the functions we actually support */
    INDIRECT_DISPATCH_INIT (((void **) (&ctx->disp)), indirect__);

    return ctx;
}
