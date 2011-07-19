/*

Copyright 1990, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/


#ifndef _RootWindoP_h
#define _RootWindoP_h

#include "RootWin.h"
/* include superclass private header file */
#include <X11/CoreP.h>

typedef struct {
    int empty;
} RootWindowClassPart;

typedef struct _RootWindowClassRec {
    CoreClassPart	core_class;
    RootWindowClassPart	root_class;
} RootWindowClassRec;

extern RootWindowClassRec rootClassRec;

typedef struct {
    /* resources */
    char* resource;
    /* private state */
} RootWindowPart;

typedef struct _RootWindowRec {
    CorePart	core;
    RootWindowPart	root;
} RootWindowRec;

#endif /* _RootWindoP_h */
