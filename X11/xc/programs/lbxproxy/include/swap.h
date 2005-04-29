/*
 * $Xorg: swap.h,v 1.3 2000/08/17 19:53:58 cpqbld Exp $
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
/* $XFree86: xc/programs/lbxproxy/include/swap.h,v 1.3 2003/11/17 22:20:49 dawes Exp $ */

#ifndef _SWAP_H_
#define _SWAP_H_

extern void SwapConnSetup(
    xConnSetup * /*pConnSetup*/,
    xConnSetup * /*pConnSetupT*/
);

extern void SwapWinRoot(
    xWindowRoot * /*pRoot*/,
    xWindowRoot * /*pRootT*/
);

extern void SwapVisual(
    xVisualType * /*pVis*/,
    xVisualType * /*pVisT*/
);

extern void WriteSConnSetupPrefix(
    ClientPtr          /* pClient */,
    xConnSetupPrefix * /* pcsp */
);

extern void WriteSConnectionInfo(
    ClientPtr /*pClient*/,
    unsigned long /*size*/,
    char * /*pInfo*/
);

extern void SwapGetPropertyReply(
    xGetPropertyReply * /*rep*/
);

extern void SwapInternAtomReply(
    xInternAtomReply * /*rep*/
);

extern void SwapGetAtomNameReply(
    xGetAtomNameReply * /*rep*/
);

extern void SwapLookupColorReply(
    xLookupColorReply * /*rep*/
);

extern void SwapAllocColorReply(
    xAllocColorReply * /*rep*/
);

extern void SwapAllocNamedColorReply(
    xAllocNamedColorReply * /*rep*/
);

extern void SwapModmapReply(
    xGetModifierMappingReply * /*rep*/
);

extern void SwapKeymapReply(
    xGetKeyboardMappingReply * /*rep*/
);

extern void SwapGetImageReply(
    xGetImageReply * /*rep*/
);

extern void SwapQueryExtensionReply(
    xQueryExtensionReply * /*rep*/
);

extern void SwapFont(
    xQueryFontReply * /*pr*/,
    Bool /*native*/
);

extern void LbxSwapFontInfo(
    xLbxFontInfo * /*pr*/,
    Bool /*compressed*/
);

extern void SwapLongs(
    CARD32 * /*list*/,
    unsigned long /*count*/
);

extern void SwapShorts(
    short * /*list*/,
    unsigned long /*count*/
);

extern void SwapConnClientPrefix(
    xConnClientPrefix * /*pCCP*/
);

extern void SwapNewClient(
    xLbxNewClientReq * /*r*/
);

extern void SwapCloseClient(
    xLbxCloseClientReq * /*r*/
);

extern void SwapModifySequence(
    xLbxModifySequenceReq * /*r*/
);

extern void SwapIncrementPixel(
    xLbxIncrementPixelReq * /*r*/
);

extern void SwapGetModifierMapping(
    xLbxGetModifierMappingReq * /*r*/
);

extern void SwapGetKeyboardMapping(
    xLbxGetKeyboardMappingReq * /*r*/
);

extern void SwapQueryFont(
    xLbxQueryFontReq * /*r*/
);

extern void SwapChangeProperty(
    xLbxChangePropertyReq * /*r*/
);

extern void SwapGetProperty(
    xLbxGetPropertyReq * /*r*/
);

extern void SwapGetImage(
    xLbxGetImageReq * /*r*/
);

extern void SwapInternAtoms(
    xLbxInternAtomsReq * /* r */
);

extern void SwapInvalidateTag(
    xLbxInvalidateTagReq * /*r*/
);

extern void SwapTagData(
    xLbxTagDataReq * /*r*/
);

extern void SwapQueryExtension(
    xLbxQueryExtensionReq * /*r*/
);

extern void SwapLbxConnSetupPrefix(
    xLbxConnSetupPrefix * /*csp*/
);

extern void SwapAllocColor(
    xLbxAllocColorReq * /* r */
);

extern void SwapGrabCmap(
    xLbxGrabCmapReq * /* r */
);

extern void SwapReleaseCmap(
    xLbxReleaseCmapReq * /* r */
);

#endif				/* _SWAP_H_ */
