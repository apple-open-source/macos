/* xttmodule.c -*- Mode: C; tab-width:4; c-basic-offset: 4; -*-
   Copyright (c) 1999 X-TrueType Server Project, All rights reserved.
  
===Notice
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   Release ID: X-TrueType Server Version 1.2 [Aoi MATSUBARA Release 2]

Notice===
*/

#include "xttversion.h"

static char const * const releaseID =
    _XTT_RELEASE_NAME;


/*
 * Copyright (C) 1998 The XFree86 Project, Inc.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 */

#include "misc.h"

#include "fontmod.h"
#include "xf86Module.h"

static MODULESETUPPROTO(xttSetup);
MODULETEARDOWNPROTO(xttTearDown);
    /*
     * this is the module init function that is executed when loading
     * libtype1 as a module. Its name has to be ModuleInit.
     * With this we initialize the function and variable pointers used
     * in generic parts of XFree86
     */

static XF86ModuleVersionInfo VersRec =
{
	"xtt",
	_XTT_VENDOR_NAME,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XF86_VERSION_CURRENT,
	_XTT_V_MAJOR,
	_XTT_V_MINOR,
	_XTT_V_REVISION,
	ABI_CLASS_FONT,			/* Font module */
	ABI_FONT_VERSION,
	MOD_CLASS_FONT,
	{0,0,0,0}       /* signature, to be patched into the file by a tool */
};


XF86ModuleData xttModuleData = {
    &VersRec, 
    xttSetup,
#ifdef CCONV_USE_SYMBOLIC_ENTRY_POINT
    xttTearDown
#else
    NULL
#endif
};

extern void XTrueTypeRegisterFontFileFunctions(void);

FontModule xttModule = {
    XTrueTypeRegisterFontFileFunctions,
    "xtt",
    NULL
};

static pointer
xttSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    xttModule.module = module;
    LoadFont(&xttModule);

    /* Need a non-NULL return */
    return (pointer)1;
}

#ifdef CCONV_USE_SYMBOLIC_ENTRY_POINT
extern char *entryName;
void
xttTearDown(pointer opts)
{
	if(entryName)
		xfree(entryName);
    entryName = NULL;
}
#endif

