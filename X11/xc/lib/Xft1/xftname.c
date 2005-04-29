/*
 * $XFree86: xc/lib/Xft1/xftname.c,v 1.4 2003/03/26 20:43:51 tsi Exp $
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

#include "xftint.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

XftPattern *
XftNameParse (const char *name)
{
    return FcNameParse ((const FcChar8 *) name);
}


Bool
XftNameUnparse (XftPattern *pat, char *dest, int len)
{
    FcChar8 *name = FcNameUnparse (pat);
    if (!name)
	return FcFalse;
    if (strlen ((char *) name) > len - 1)
    {
	free (name);
	return FcFalse;
    }
    strcpy (dest, (char *) name);
    free (name);
    return FcTrue;
}

Bool
XftNameConstant (char *string, int *result)
{
    return FcNameConstant ((FcChar8 *) string, result);
}

