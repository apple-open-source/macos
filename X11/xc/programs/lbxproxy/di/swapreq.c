/* $Xorg: swapreq.c,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */
/*

Copyright 1998  The Open Group

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

*/
/*
 *
 * Copyright 1994 Network Computing Devices, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name Network Computing Devices, Inc. not be
 * used in advertising or publicity pertaining to distribution of this
 * software without specific, written prior permission.
 *
 * THIS SOFTWARE IS PROVIDED `AS-IS'.  NETWORK COMPUTING DEVICES, INC.,
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT
 * LIMITATION ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, OR NONINFRINGEMENT.  IN NO EVENT SHALL NETWORK
 * COMPUTING DEVICES, INC., BE LIABLE FOR ANY DAMAGES WHATSOEVER, INCLUDING
 * SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS OF USE, DATA,
 * OR PROFITS, EVEN IF ADVISED OF THE POSSIBILITY THEREOF, AND REGARDLESS OF
 * WHETHER IN AN ACTION IN CONTRACT, TORT OR NEGLIGENCE, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include	<stdio.h>
#include	"misc.h"
#include	"assert.h"
#include	"lbx.h"
#include	"swap.h"

/* Byte swap a list of longs */
void
SwapLongs(list, count)
    register CARD32 *list;
    register unsigned long count;
{
    register char n;

    while (count >= 8) {
	swapl(list + 0, n);
	swapl(list + 1, n);
	swapl(list + 2, n);
	swapl(list + 3, n);
	swapl(list + 4, n);
	swapl(list + 5, n);
	swapl(list + 6, n);
	swapl(list + 7, n);
	list += 8;
	count -= 8;
    }
    if (count != 0) {
	do {
	    swapl(list, n);
	    list++;
	} while (--count != 0);
    }
}

/* Byte swap a list of shorts */
void
SwapShorts(list, count)
    register short *list;
    register unsigned long count;
{
    register char n;

    while (count >= 16) {
	swaps(list + 0, n);
	swaps(list + 1, n);
	swaps(list + 2, n);
	swaps(list + 3, n);
	swaps(list + 4, n);
	swaps(list + 5, n);
	swaps(list + 6, n);
	swaps(list + 7, n);
	swaps(list + 8, n);
	swaps(list + 9, n);
	swaps(list + 10, n);
	swaps(list + 11, n);
	swaps(list + 12, n);
	swaps(list + 13, n);
	swaps(list + 14, n);
	swaps(list + 15, n);
	list += 16;
	count -= 16;
    }
    if (count != 0) {
	do {
	    swaps(list, n);
	    list++;
	} while (--count != 0);
    }
}

void
SwapConnClientPrefix(pCCP)
    xConnClientPrefix *pCCP;
{
    register char n;

    swaps(&pCCP->majorVersion, n);
    swaps(&pCCP->minorVersion, n);
    swaps(&pCCP->nbytesAuthProto, n);
    swaps(&pCCP->nbytesAuthString, n);
}

void
SwapNewClient(r)
    xLbxNewClientReq *r;
{
    char        n;

    swapl(&r->client, n);
}

void
SwapCloseClient(r)
    xLbxCloseClientReq *r;
{
    char        n;

    swapl(&r->client, n);
}

void
SwapModifySequence(r)
    xLbxModifySequenceReq *r;
{
    char        n;

    swapl(&r->adjust, n);
}

void
SwapIncrementPixel(r)
    xLbxIncrementPixelReq *r;
{
    char        n;

    swapl(&r->cmap, n);
    swapl(&r->pixel, n);
}

/* ARGSUSED */
void
SwapGetModifierMapping(r)
    xLbxGetModifierMappingReq *r;
{
}

/* ARGSUSED */
void
SwapGetKeyboardMapping(r)
    xLbxGetKeyboardMappingReq *r;
{
}

void
SwapQueryFont(r)
    xLbxQueryFontReq *r;
{
    char        n;

    swapl(&r->fid, n);
}

void
SwapChangeProperty(r)
    xLbxChangePropertyReq *r;
{
    char        n;

    swapl(&r->window, n);
    swapl(&r->property, n);
    swapl(&r->type, n);
    swapl(&r->nUnits, n);
}

void
SwapGetProperty(r)
    xLbxGetPropertyReq *r;
{
    char        n;

    swapl(&r->window, n);
    swapl(&r->property, n);
    swapl(&r->type, n);
    swapl(&r->longOffset, n);
    swapl(&r->longLength, n);
}

void
SwapGetImage (r)
    xLbxGetImageReq *r;
{
    char        n;

    swapl(&r->drawable, n);
    swaps(&r->x, n);
    swaps(&r->y, n);
    swaps(&r->width, n);
    swaps(&r->height, n);
    swapl(&r->planeMask, n);
}


void
SwapInternAtoms(r)
    xLbxInternAtomsReq *r;
{
    char n;
    char *ptr;
    char lenbuf[2];
    CARD16 len;
    int i;

    ptr = (char *) r + sz_xLbxInternAtomsReq;
    for (i = 0; i < r->num; i++)
    {
	lenbuf[0] = ptr[0];
	lenbuf[1] = ptr[1];
	len = *((CARD16 *) lenbuf);
	swaps (ptr, n);
	ptr += (len + 2);
    }

    swaps(&r->num, n);
}


void
SwapInvalidateTag(r)
    xLbxInvalidateTagReq *r;
{
    char        n;

    swapl(&r->tag, n);
}

void
SwapTagData(r)
    xLbxTagDataReq *r;
{
    char        n;

    swapl(&r->real_length, n);
    swapl(&r->tag, n);
}

void
SwapQueryExtension(r)
    xLbxQueryExtensionReq *r;
{
    char        n;

    swapl(&r->nbytes, n);
}

void
SwapLbxConnSetupPrefix(csp)
    xLbxConnSetupPrefix *csp;
{
    char        n;

    swaps(&csp->majorVersion, n);
    swaps(&csp->minorVersion, n);
    swapl(&csp->tag, n);
}

void
SwapAllocColor(r)
    xLbxAllocColorReq *r;
{
    char        n;

    swapl(&r->cmap, n);
    swapl(&r->pixel, n);
    swaps(&r->red, n);
    swaps(&r->green, n);
    swaps(&r->blue, n);
}


void
SwapGrabCmap(r)
    xLbxGrabCmapReq *r;
{
    char        n;

    swapl(&r->cmap, n);
}

void
SwapReleaseCmap(r)
    xLbxReleaseCmapReq *r;
{
    char        n;

    swapl(&r->cmap, n);
}

