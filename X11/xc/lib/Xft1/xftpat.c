/*
 * $XFree86: xc/lib/Xft1/xftpat.c,v 1.3 2002/06/07 23:44:23 keithp Exp $
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include "xftint.h"
#include <fontconfig/fcprivate.h>

XftPattern *
XftPatternCreate (void)
{
    return FcPatternCreate ();
}

void
XftValueDestroy (XftValue v)
{
    FcValueDestroy (v);
}

void
XftPatternDestroy (XftPattern *p)
{
    FcPatternDestroy (p);
}


Bool
XftPatternAdd (XftPattern *p, const char *object, XftValue value, Bool append)
{
    return FcPatternAdd (p, object, value, append);
}

Bool
XftPatternDel (XftPattern *p, const char *object)
{
    return FcPatternDel (p, object);
}

Bool
XftPatternAddInteger (XftPattern *p, const char *object, int i)
{
    XftValue	v;

    v.type = XftTypeInteger;
    v.u.i = i;
    return XftPatternAdd (p, object, v, True);
}

Bool
XftPatternAddDouble (XftPattern *p, const char *object, double d)
{
    XftValue	v;

    v.type = XftTypeDouble;
    v.u.d = d;
    return XftPatternAdd (p, object, v, True);
}


Bool
XftPatternAddString (XftPattern *p, const char *object, const char *s)
{
    XftValue	v;

    v.type = XftTypeString;
    v.u.s = (FcChar8 *) s;
    return XftPatternAdd (p, object, v, True);
}

Bool
XftPatternAddMatrix (XftPattern *p, const char *object, const XftMatrix *s)
{
    XftValue	v;

    v.type = XftTypeMatrix;
    v.u.m = (XftMatrix *) s;
    return XftPatternAdd (p, object, v, True);
}


Bool
XftPatternAddBool (XftPattern *p, const char *object, Bool b)
{
    XftValue	v;

    v.type = XftTypeBool;
    v.u.b = b;
    return XftPatternAdd (p, object, v, True);
}

XftResult
XftPatternGet (XftPattern *p, const char *object, int id, XftValue *v)
{
    return FcPatternGet (p, object, id, v);
}

XftResult
XftPatternGetInteger (XftPattern *p, const char *object, int id, int *i)
{
    XftValue	v;
    XftResult	r;

    r = XftPatternGet (p, object, id, &v);
    if (r != XftResultMatch)
	return r;
    switch (v.type) {
    case XftTypeDouble:
	*i = (int) v.u.d;
	break;
    case XftTypeInteger:
	*i = v.u.i;
	break;
    default:
        return XftResultTypeMismatch;
    }
    return XftResultMatch;
}

XftResult
XftPatternGetDouble (XftPattern *p, const char *object, int id, double *d)
{
    XftValue	v;
    XftResult	r;

    r = XftPatternGet (p, object, id, &v);
    if (r != XftResultMatch)
	return r;
    switch (v.type) {
    case XftTypeDouble:
	*d = v.u.d;
	break;
    case XftTypeInteger:
	*d = (double) v.u.i;
	break;
    default:
        return XftResultTypeMismatch;
    }
    return XftResultMatch;
}

XftResult
XftPatternGetString (XftPattern *p, const char *object, int id, char **s)
{
    XftValue	v;
    XftResult	r;

    r = XftPatternGet (p, object, id, &v);
    if (r != XftResultMatch)
	return r;
    if (v.type != XftTypeString)
        return XftResultTypeMismatch;
    *s = (char *) v.u.s;
    return XftResultMatch;
}

XftResult
XftPatternGetMatrix (XftPattern *p, const char *object, int id, XftMatrix **m)
{
    XftValue	v;
    XftResult	r;

    r = XftPatternGet (p, object, id, &v);
    if (r != XftResultMatch)
	return r;
    if (v.type != XftTypeMatrix)
        return XftResultTypeMismatch;
    *m = (XftMatrix *) v.u.m;
    return XftResultMatch;
}


XftResult
XftPatternGetBool (XftPattern *p, const char *object, int id, Bool *b)
{
    XftValue	v;
    XftResult	r;

    r = XftPatternGet (p, object, id, &v);
    if (r != XftResultMatch)
	return r;
    if (v.type != XftTypeBool)
        return XftResultTypeMismatch;
    *b = v.u.b;
    return XftResultMatch;
}

XftPatternElt *
XftPatternFind (XftPattern *p, const char *object, FcBool insert)
{
    if (insert)
	return FcPatternInsertElt (p, object);
    else
	return FcPatternFindElt (p, object);
}

    
XftPattern *
XftPatternDuplicate (XftPattern *orig)
{
    return FcPatternDuplicate (orig);
}

XftPattern *
XftPatternVaBuild (XftPattern *orig, va_list va)
{
    XftPattern	*ret;
    
    FcPatternVapBuild (ret, orig, va);
    return ret;
}

XftPattern *
XftPatternBuild (XftPattern *orig, ...)
{
    va_list	va;
    
    va_start (va, orig);
    FcPatternVapBuild (orig, orig, va);
    va_end (va);
    return orig;
}
