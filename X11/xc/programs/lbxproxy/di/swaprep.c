/* $Xorg: swaprep.c,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */
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

void
SwapConnSetup(pConnSetup, pConnSetupT)
    xConnSetup *pConnSetup,
               *pConnSetupT;
{
    cpswapl(pConnSetup->release, pConnSetupT->release);
    cpswapl(pConnSetup->ridBase, pConnSetupT->ridBase);
    cpswapl(pConnSetup->ridMask, pConnSetupT->ridMask);
    cpswapl(pConnSetup->motionBufferSize, pConnSetupT->motionBufferSize);
    cpswaps(pConnSetup->nbytesVendor, pConnSetupT->nbytesVendor);
    cpswaps(pConnSetup->maxRequestSize, pConnSetupT->maxRequestSize);
    pConnSetupT->minKeyCode = pConnSetup->minKeyCode;
    pConnSetupT->maxKeyCode = pConnSetup->maxKeyCode;
    pConnSetupT->numRoots = pConnSetup->numRoots;
    pConnSetupT->numFormats = pConnSetup->numFormats;
    pConnSetupT->imageByteOrder = pConnSetup->imageByteOrder;
    pConnSetupT->bitmapBitOrder = pConnSetup->bitmapBitOrder;
    pConnSetupT->bitmapScanlineUnit = pConnSetup->bitmapScanlineUnit;
    pConnSetupT->bitmapScanlinePad = pConnSetup->bitmapScanlinePad;
}

void
SwapWinRoot(pRoot, pRootT)
    xWindowRoot *pRoot,
               *pRootT;
{
    cpswapl(pRoot->windowId, pRootT->windowId);
    cpswapl(pRoot->defaultColormap, pRootT->defaultColormap);
    cpswapl(pRoot->whitePixel, pRootT->whitePixel);
    cpswapl(pRoot->blackPixel, pRootT->blackPixel);
    cpswapl(pRoot->currentInputMask, pRootT->currentInputMask);
    cpswaps(pRoot->pixWidth, pRootT->pixWidth);
    cpswaps(pRoot->pixHeight, pRootT->pixHeight);
    cpswaps(pRoot->mmWidth, pRootT->mmWidth);
    cpswaps(pRoot->mmHeight, pRootT->mmHeight);
    cpswaps(pRoot->minInstalledMaps, pRootT->minInstalledMaps);
    cpswaps(pRoot->maxInstalledMaps, pRootT->maxInstalledMaps);
    cpswapl(pRoot->rootVisualID, pRootT->rootVisualID);
    pRootT->backingStore = pRoot->backingStore;
    pRootT->saveUnders = pRoot->saveUnders;
    pRootT->rootDepth = pRoot->rootDepth;
    pRootT->nDepths = pRoot->nDepths;
}

void
SwapVisual(pVis, pVisT)
    xVisualType *pVis,
               *pVisT;
{
    cpswapl(pVis->visualID, pVisT->visualID);
    pVisT->class = pVis->class;
    pVisT->bitsPerRGB = pVis->bitsPerRGB;
    cpswaps(pVis->colormapEntries, pVisT->colormapEntries);
    cpswapl(pVis->redMask, pVisT->redMask);
    cpswapl(pVis->greenMask, pVisT->greenMask);
    cpswapl(pVis->blueMask, pVisT->blueMask);
}

void
WriteSConnSetupPrefix(pClient, pcsp)
    ClientPtr		pClient;
    xConnSetupPrefix	*pcsp;
{
    xConnSetupPrefix	cspT;

    cspT.success = pcsp->success;
    cspT.lengthReason = pcsp->lengthReason;
    cpswaps(pcsp->majorVersion, cspT.majorVersion);
    cpswaps(pcsp->minorVersion, cspT.minorVersion);
    cpswaps(pcsp->length, cspT.length);
    (void)WriteToClient(pClient, sizeof(cspT), (char *) &cspT);
}

void
WriteSConnectionInfo(pClient, size, pInfo)
    ClientPtr   pClient;
    unsigned long size;
    char       *pInfo;
{
    int         i,
                j,
                k;
    xDepth     *pDepth;
    int         numDepths;
    char       *pInfoT,
               *pInfoTBase;
    xConnSetup *pConnSetup = (xConnSetup *) pInfo;

    pInfoT = pInfoTBase = (char *) ALLOCATE_LOCAL(size);
    if (!pInfoTBase) {
	pClient->noClientException = -1;
	return;
    }
    SwapConnSetup(pConnSetup, (xConnSetup *) pInfoT);
    pInfo += sizeof(xConnSetup);
    pInfoT += sizeof(xConnSetup);

    /* Copy the vendor string */
    i = (pConnSetup->nbytesVendor + 3) & ~3;
    memmove(pInfoT, pInfo, i);
    pInfo += i;
    pInfoT += i;

    /* The Pixmap formats don't need to be swapped, just copied. */
    i = sizeof(xPixmapFormat) * pConnSetup->numFormats;
    memmove(pInfoT, pInfo, i);
    pInfo += i;
    pInfoT += i;

    for (i = 0; i < pConnSetup->numRoots; i++) {
	SwapWinRoot((xWindowRoot *) pInfo, (xWindowRoot *) pInfoT);
	numDepths = ((xWindowRoot *) pInfo)->nDepths;
	pInfo += sizeof(xWindowRoot);
	pInfoT += sizeof(xWindowRoot);
	pDepth = (xDepth *) pInfo;
	for (j = 0; j < numDepths; j++, pDepth = (xDepth *) pInfo) {
	    ((xDepth *) pInfoT)->depth = ((xDepth *) pInfo)->depth;
	    cpswaps(((xDepth *) pInfo)->nVisuals, ((xDepth *) pInfoT)->nVisuals);
	    pInfo += sizeof(xDepth);
	    pInfoT += sizeof(xDepth);
	    for (k = 0; k < pDepth->nVisuals; k++) {
		SwapVisual((xVisualType *) pInfo, (xVisualType *) pInfoT);
		pInfo += sizeof(xVisualType);
		pInfoT += sizeof(xVisualType);
	    }
	}
    }
    (void) WriteToClient(pClient, (int) size, (char *) pInfoTBase);
    DEALLOCATE_LOCAL(pInfoTBase);
}

void
SwapGetPropertyReply(rep)
    xGetPropertyReply *rep;
{
    char        n;

    swaps(&rep->sequenceNumber, n);
    swapl(&rep->length, n);
    swapl(&rep->propertyType, n);
    swapl(&rep->bytesAfter, n);
    swapl(&rep->nItems, n);
}

void
SwapInternAtomReply(rep)
    xInternAtomReply *rep;
{
    char        n;

    swaps(&rep->sequenceNumber, n);
    swapl(&rep->length, n);
    swapl(&rep->atom, n);
}

void
SwapGetAtomNameReply(rep)
    xGetAtomNameReply *rep;
{
    char        n;

    swaps(&rep->sequenceNumber, n);
    swapl(&rep->length, n);
    swaps(&rep->nameLength, n);
}

void
SwapLookupColorReply(rep)
    xLookupColorReply *rep;
{
    char        n;

    swaps(&rep->sequenceNumber, n);
    swapl(&rep->length, n);
    swaps(&rep->exactRed, n);
    swaps(&rep->exactGreen, n);
    swaps(&rep->exactBlue, n);
    swaps(&rep->screenRed, n);
    swaps(&rep->screenGreen, n);
    swaps(&rep->screenBlue, n);
}

void
SwapAllocColorReply(rep)
    xAllocColorReply *rep;
{
    char        n;

    swaps(&rep->sequenceNumber, n);
    swapl(&rep->length, n);
    swaps(&rep->red, n);
    swaps(&rep->green, n);
    swaps(&rep->blue, n);
    swapl(&rep->pixel, n);
}

void
SwapAllocNamedColorReply(rep)
    xAllocNamedColorReply *rep;
{
    char        n;

    swaps(&rep->sequenceNumber, n);
    swapl(&rep->length, n);
    swaps(&rep->exactRed, n);
    swaps(&rep->exactGreen, n);
    swaps(&rep->exactBlue, n);
    swaps(&rep->screenRed, n);
    swaps(&rep->screenGreen, n);
    swaps(&rep->screenBlue, n);
    swapl(&rep->pixel, n);
}

void
SwapModmapReply(rep)
    xGetModifierMappingReply *rep;
{
    char        n;

    swaps(&rep->sequenceNumber, n);
    swapl(&rep->length, n);
}

void
SwapKeymapReply(rep)
    xGetKeyboardMappingReply *rep;
{
    char        n;

    swaps(&rep->sequenceNumber, n);
    swapl(&rep->length, n);
}

void
SwapGetImageReply(rep)
    xGetImageReply *rep;
{
    char n;

    swaps(&rep->sequenceNumber, n);
    swapl(&rep->length, n);
    swapl(&rep->visual, n);
}

void
SwapQueryExtensionReply(rep)
    xQueryExtensionReply	*rep;
{
    char n;

    swaps(&rep->sequenceNumber, n);
    swapl(&rep->length, n);
}

static void
SwapCharInfo(pInfo)
    xCharInfo  *pInfo;
{
    register char n;

    swaps(&pInfo->leftSideBearing, n);
    swaps(&pInfo->rightSideBearing, n);
    swaps(&pInfo->characterWidth, n);
    swaps(&pInfo->ascent, n);
    swaps(&pInfo->descent, n);
    swaps(&pInfo->attributes, n);
}

static void
SwapFontInfo(pr)
    xQueryFontReply *pr;
{
    register char n;

    swaps(&pr->minCharOrByte2, n);
    swaps(&pr->maxCharOrByte2, n);
    swaps(&pr->defaultChar, n);
    swaps(&pr->nFontProps, n);
    swaps(&pr->fontAscent, n);
    swaps(&pr->fontDescent, n);
    SwapCharInfo(&pr->minBounds);
    SwapCharInfo(&pr->maxBounds);
    swapl(&pr->nCharInfos, n);
}

static void
SwapLbxFontInfo(pr)
    xLbxFontInfo *pr;
{
    register char n;

    swaps(&pr->minCharOrByte2, n);
    swaps(&pr->maxCharOrByte2, n);
    swaps(&pr->defaultChar, n);
    swaps(&pr->nFontProps, n);
    swaps(&pr->fontAscent, n);
    swaps(&pr->fontDescent, n);
    SwapCharInfo(&pr->minBounds);
    SwapCharInfo(&pr->maxBounds);
    swapl(&pr->nCharInfos, n);
}

/* native flag specifies whether its usable and we're trying
 * to swap it for external use
 */
void
SwapFont(pr, native)
    xQueryFontReply *pr;
    Bool	native;
{
    unsigned    i;
    xCharInfo  *pxci;
    unsigned    nchars,
                nprops;
    char       *pby;
    register char n;

    swaps(&pr->sequenceNumber, n);
    swapl(&pr->length, n);
    if (!native)
	SwapFontInfo(pr);
    nchars = pr->nCharInfos;
    nprops = pr->nFontProps;
    if (native)
	SwapFontInfo(pr);
    pby = (char *) &pr[1];
    /*
     * Font properties are an atom and either an int32 or a CARD32, so they
     * are always 2 4 byte values
     */
    for (i = 0; i < nprops; i++) {
	swapl(pby, n);
	pby += 4;
	swapl(pby, n);
	pby += 4;
    }
    pxci = (xCharInfo *) pby;
    for (i = 0; i < nchars; i++, pxci++)
	SwapCharInfo(pxci);
}

void
LbxSwapFontInfo(pr, compressed)
    xLbxFontInfo *pr;
    Bool	compressed;
{
    unsigned    i;
    xCharInfo  *pxci;
    unsigned    nchars,
                nprops;
    char       *pby;
    register char n;

    SwapLbxFontInfo(pr);
    nchars = pr->nCharInfos;
    nprops = pr->nFontProps;
    pby = (char *) &pr[1];
    /*
     * Font properties are an atom and either an int32 or a CARD32, so they
     * are always 2 4 byte values
     */
    for (i = 0; i < nprops; i++) {
	swapl(pby, n);
	pby += 4;
	swapl(pby, n);
	pby += 4;
    }
    pxci = (xCharInfo *) pby;
    if (!compressed) {
	for (i = 0; i < nchars; i++, pxci++)
	    SwapCharInfo(pxci);
    } else {
	SwapLongs((CARD32 *) pxci, nchars);
    }
}
