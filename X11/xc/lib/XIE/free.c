/* $Xorg: free.c,v 1.4 2001/02/09 02:03:41 xorgcvs Exp $ */

/*

Copyright 1993, 1998  The Open Group

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
/* $XFree86: xc/lib/XIE/free.c,v 1.5 2001/12/14 19:54:33 dawes Exp $ */

#include "XIElibint.h"

#define CHECK_AND_FREE(_ptr) if (_ptr) Xfree (_ptr)


void
XieFreeTechniques (XieTechnique *techs, unsigned int count)
{
    unsigned i;

    if (techs)
    {
	for (i = 0; i < count; i++)
	    CHECK_AND_FREE (techs[i].name);

	Xfree ((char *) techs);
    }
}


void
XieFreePhotofloGraph (XiePhotoElement *elements, unsigned int count)
{
    /*
     * NOTE: We do not free the technique parameters here.
     * Most of the technique parameters should be freed by the
     * client using Xfree (exception: EncodeJPEGBaseline and
     * EncodeJPEGLossless, see functions below).  This is so
     * the client can reuse technique parameters between photoflos.
     */

    unsigned i;

    if (!elements)
	return;

    for (i = 0; i < count; i++)
    {
	switch (elements[i].elemType)
	{
	case xieElemConvolve:
	    CHECK_AND_FREE ((char *) elements[i].data.Convolve.kernel);
	    break;
	case xieElemPasteUp:
	    CHECK_AND_FREE ((char *) elements[i].data.PasteUp.tiles);
	    break;
	default:
	    break;
	}
    }

    Xfree ((char *) elements);
}


void
XieFreeEncodeJPEGBaseline (XieEncodeJPEGBaselineParam *param)
{
    if (param)
    {
	CHECK_AND_FREE ((char *) param->q_table);
	CHECK_AND_FREE ((char *) param->ac_table);
	CHECK_AND_FREE ((char *) param->dc_table);
	Xfree ((char *) param);
    }
}


void
XieFreeEncodeJPEGLossless (XieEncodeJPEGLosslessParam *param)
{
    if (param)
    {
	CHECK_AND_FREE ((char *) param->table);
	Xfree ((char *) param);
    }
}


void
XieFreePasteUpTiles (XiePhotoElement *element)
{
    XieTile *tiles= element->data.PasteUp.tiles;

    if (tiles)
    {
	Xfree (tiles);
	element->data.PasteUp.tiles=NULL;
    }
}
