/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright (c) 2002 Apple Computer, Inc.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/lib/GL/dri/dri_glx.c,v 1.10 2001/08/27 17:40:57 dawes Exp $ */

/*
 * Authors:
 *   Kevin E. Martin <kevin@precisioninsight.com>
 *   Brian Paul <brian@precisioninsight.com>
 *
 */

#ifdef GLX_DIRECT_RENDERING

#include <unistd.h>
#include <Xlibint.h>
#include <extensions/Xext.h>
#include <extensions/extutil.h>
#include "glxclient.h"
#include "appledri.h"
#include <stdio.h>
#include "dri_glx.h"
#include <sys/types.h>
#include <stdarg.h>


static void driDestroyDisplay(Display *dpy, void *private)
{
    __DRIdisplayPrivate *pdpyp = (__DRIdisplayPrivate *)private;

    if (pdpyp) {
	Xfree(pdpyp->createScreen);
	Xfree(pdpyp);
    }
}


void *driCreateDisplay(Display *dpy, __DRIdisplay *pdisp)
{
    const int numScreens = ScreenCount(dpy);
    __DRIdisplayPrivate *pdpyp;
    int eventBase, errorBase;
    int major, minor, patch;

    /* Initialize these fields to NULL in case we fail.
     * If we don't do this we may later get segfaults trying to free random
     * addresses when the display is closed.
     */
    pdisp->private = NULL;
    pdisp->destroyDisplay = NULL;
    pdisp->createScreen = NULL;

    if (!XAppleDRIQueryExtension(dpy, &eventBase, &errorBase)) {
	return NULL;
    }

    if (!XAppleDRIQueryVersion(dpy, &major, &minor, &patch)) {
	return NULL;
    }

    pdpyp = (__DRIdisplayPrivate *)Xmalloc(sizeof(__DRIdisplayPrivate));
    if (!pdpyp) {
	return NULL;
    }

    pdpyp->driMajor = major;
    pdpyp->driMinor = minor;
    pdpyp->driPatch = patch;

    pdisp->destroyDisplay = driDestroyDisplay;

    /* allocate array of pointers to createScreen funcs */
    pdisp->createScreen = (CreateScreenFunc *) Xmalloc(numScreens * sizeof(void *));
    if (!pdisp->createScreen)
       return NULL;

    /* we'll statically bind to the __driCreateScreen function */
    {
       int i;
       for (i = 0; i < numScreens; i++) {
          pdisp->createScreen[i] = __driCreateScreen;
       }
    }

    return (void *)pdpyp;
}


/*
** Here we'll query the DRI driver for each screen and let each
** driver register its GL extension functions.  We only have to
** do this once.  But it MUST be done before we create any contexts
** (i.e. before any dispatch tables are created) and before
** glXGetProcAddressARB() returns.
**
** Currently called by glXGetProcAddress(), __glXInitialize(), and
** __glXNewIndirectAPI().
*/
void
__glXRegisterExtensions(void)
{
   static GLboolean alreadyCalled = GL_FALSE;

   if (alreadyCalled) {
      return;
   }

   __driRegisterExtensions ();

   alreadyCalled = GL_TRUE;
}


#endif /* GLX_DIRECT_RENDERING */
