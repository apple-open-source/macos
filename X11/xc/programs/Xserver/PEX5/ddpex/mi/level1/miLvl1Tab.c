/* $Xorg: miLvl1Tab.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */

/***********************************************************

Copyright 1989, 1990, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Copyright 1989, 1990, 1991 by Sun Microsystems, Inc.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Sun Microsystems,
and The Open Group, not be used in advertising or publicity 
pertaining to distribution of the software without specific, written 
prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/* Level I Rendering routine tables */

#include "mipex.h"
#include "miRender.h"

/* Rendering procedures for ExecuteOCTable */
extern	ddpex2rtn	miRenderPolyLine(),
			miRenderFillArea(),
			miRenderText(),
			miRenderMarker(),
			miRenderTriStrip();

RendTableType	RenderPrimitiveTable[] = {
    miRenderPolyLine,		/* 0 Polylines */
    miRenderFillArea,		/* 1 Fill Areas */
    miRenderText,		/* 2 Text */
    miRenderMarker,		/* 3 Markers */
    miRenderTriStrip,		/* 4 Triangle Strips */
};

/* Picking procedures for ExecuteOCTable */
extern	ddpex2rtn	miPick1PolyLine(),
			miPick1FillArea(),
			miPick1Text(),
			miPick1Marker(),
			miPick1TriStrip();

RendTableType	PickPrimitiveTable[] = {
    miPick1PolyLine,		/* 0 Polylines */
    miPick1FillArea,		/* 1 Fill Areas */
    miPick1Text,		/* 2 Text */
    miPick1Marker,		/* 3 Markers */
    miPick1TriStrip,		/* 4 Triangle Strips */
};

