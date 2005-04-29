/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1998 Go Watanabe, All rights reserved.
   Copyright (c) 1998 Takuya SHIOZAKI, All rights reserved.
   Copyright (c) 1998 X-TrueType Server Project, All rights reserved.

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

   Major Release ID: X-TrueType Server Version 1.3 [Aoi MATSUBARA Release 3]

Notice===
*/
/* $XFree86: xc/extras/X-TrueType/xttprop.c,v 1.6 2003/11/06 18:37:54 tsi Exp $ */

#include "xttversion.h"

#if 0
static char const * const releaseID =
    _XTT_RELEASE_NAME;
#endif

/* THIS FILE IS BASED ON Speedo Font lib spinfo.c */

/*------------------------- original copyright ------------------------*/
/*
 * Copyright 1990, 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices or Digital
 * not be used in advertising or publicity pertaining to distribution of
 * the software without specific, written prior permission.
 *
 * NETWORK COMPUTING DEVICES AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES OR DIGITAL BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Dave Lemke, Network Computing Devices, Inc
 */

/*
Copyright (c) 1987  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/

#ifndef FONTMODULE
#include <X11/Xos.h>
#endif
#include <X11/X.h>
#include "fntfilst.h"
#include "xttcommon.h"

/* percentage of pointsize used to specify ascent & descent */
#define STRETCH_FACTOR  120

enum scaleType {
    none, atom, truncate_atom, pixel_size, point_size, resolution_x,
    resolution_y, average_width
};

typedef struct _fontProp {
    char       *name;
    long        atom;
    enum scaleType type;
} fontProp;

static fontProp fontNamePropTable[] = {
    { "FOUNDRY", 0, atom },
    { "FAMILY_NAME", 0, atom },
    { "WEIGHT_NAME", 0, atom },
    { "SLANT", 0, atom },
    { "SETWIDTH_NAME", 0, atom },
    { "ADD_STYLE_NAME", 0, atom },
    { "PIXEL_SIZE", 0, pixel_size },
    { "POINT_SIZE", 0, point_size },
    { "RESOLUTION_X", 0, resolution_x },
    { "RESOLUTION_Y", 0, resolution_y },
    { "SPACING", 0, atom },
    { "AVERAGE_WIDTH", 0, average_width },
    { "CHARSET_REGISTRY", 0, atom },
    { "CHARSET_ENCODING", 0, truncate_atom },
};

/* Warning: following array is closely related to the sequence of
   defines after it. */

static fontProp extraProps[] = {
    { "FONT", 0, none },
    { "COPYRIGHT", 0, none },
    { "RAW_PIXEL_SIZE", 0, none },
    { "RAW_POINT_SIZE", 0, none },
    { "RAW_ASCENT", 0, none },
    { "RAW_DESCENT", 0, none },
    { "RAW_AVERAGE_WIDTH", 0, none },
};

/* this is a bit kludgy */
#define FONTPROP    0
#define COPYRIGHTPROP   1
#define RAWPIXELPROP    2
#define RAWPOINTPROP    3
#define RAWASCENTPROP   4
#define RAWDESCENTPROP  5
#define RAWWIDTHPROP    6

#define NNAMEPROPS (sizeof(fontNamePropTable) / sizeof(fontProp))
#define NEXTRAPROPS (sizeof(extraProps) / sizeof(fontProp))

#define NPROPS  (NNAMEPROPS + NEXTRAPROPS)

void
freetype_make_standard_props(void)
{
    int         i;
    fontProp   *t;
    i = sizeof(fontNamePropTable) / sizeof(fontProp);
    for (t = fontNamePropTable; i; i--, t++)
        t->atom = MakeAtom(t->name, (unsigned) strlen(t->name), True);
    i = sizeof(extraProps) / sizeof(fontProp);
    for (t = extraProps; i; i--, t++)
        t->atom = MakeAtom(t->name, (unsigned) strlen(t->name), True);
}

void
freetype_compute_props(FontInfoPtr pinfo,
                       FontScalablePtr vals,
                       int raw_width,
                       int raw_ascent,
                       int raw_descent,
                       char *fontname,
                       char *copyright)
{
    FontPropPtr pp;
    int         i, nprops;
    fontProp    *fpt;
    char        *is_str;
    char        *ptr1 = NULL, *ptr2, *ptr3;
    /*    FontScalableRec tmpvals;*/

    nprops = pinfo->nprops = NPROPS;
    pinfo->isStringProp = (char *) xalloc(sizeof(char) * nprops);
    pinfo->props = (FontPropPtr) xalloc(sizeof(FontPropRec) * nprops);
    if (!pinfo->isStringProp || !pinfo->props) {
        xfree(pinfo->isStringProp);
        pinfo->isStringProp = (char *) 0;
        xfree(pinfo->props);
        pinfo->props = (FontPropPtr) 0;
        return;
    }
    memset(pinfo->isStringProp, 0, (sizeof(char) * nprops));

    ptr2 = fontname;
    for (i = NNAMEPROPS, pp = pinfo->props, fpt = fontNamePropTable,
             is_str = pinfo->isStringProp;   i;
         i--, pp++, fpt++, is_str++) {

        if (*ptr2) {
            ptr1 = ptr2 + 1;
            if (!(ptr2 = strchr(ptr1, '-'))) ptr2 = strchr(ptr1, '\0');
        }

        pp->name = fpt->atom;
        switch (fpt->type) {
        case atom:
            *is_str = True;
            pp->value = MakeAtom(ptr1, ptr2 - ptr1, True);
            break;
        case truncate_atom:
            *is_str = True;
            for (ptr3 = ptr1; *ptr3; ptr3++)
                if (*ptr3 == '[')
                    break;
            pp->value = MakeAtom(ptr1, ptr3 - ptr1, True);
            break;
        case pixel_size:
            pp->value = (int)(vals->pixel_matrix[3] +
                              (vals->pixel_matrix[3] > 0 ? .5 : -.5));
            break;
        case point_size:
            pp->value = (int)(vals->point_matrix[3] * 10.0 +
                              (vals->point_matrix[3] > 0 ? .5 : -.5));
            break;
        case resolution_x:
            pp->value = vals->x;
            break;
        case resolution_y:
            pp->value = vals->y;
            break;
        case average_width:
            pp->value = vals->width;
            break;
	default:
	    break;
        }
    }

    for (i=0, fpt = extraProps; i < NEXTRAPROPS; i++, is_str++, pp++, fpt++){
        pp->name = fpt->atom;
        switch (i) {
        case FONTPROP:
            *is_str = True;
            pp->value = MakeAtom(fontname, strlen(fontname), True);
            break;
        case COPYRIGHTPROP:
            *is_str = True;
            pp->value = MakeAtom(copyright,
                                 strlen(copyright), True);
            break;
        case RAWPIXELPROP:
            *is_str = False;
            pp->value = 1000;
            break;
        case RAWPOINTPROP:
            *is_str = False;
            pp->value = (long)(72270.0 / (double)vals->y + .5);
            break;
        case RAWASCENTPROP:
            *is_str = False;
            pp->value = raw_ascent;
            break;
        case RAWDESCENTPROP:
            *is_str = False;
            pp->value = raw_descent;
            break;
        case RAWWIDTHPROP:
            *is_str = False;
            pp->value = raw_width;
            break;
        }
    }
}

/* end of file */
