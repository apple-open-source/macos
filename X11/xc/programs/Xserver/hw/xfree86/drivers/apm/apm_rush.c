/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/apm/apm_rush.c,v 1.12 2003/02/12 21:46:42 tsi Exp $ */
/*
 * Copyright Loïc Grenié 1999
 */

#include "apm.h"
#include "xaalocal.h"

extern DriverRec APM;

static Bool		RushDestroyPixmap(PixmapPtr);
static __inline__ void	__xf86UnlockPixmap(ApmPtr, PixmapLinkPtr);
static int		xf86RushLockPixmap(int, PixmapPtr);
static void		xf86RushUnlockPixmap(int, PixmapPtr);
static void		xf86RushUnlockAllPixmaps(void);

static Bool RushDestroyPixmap(PixmapPtr pPix)
{
    APMDECL(xf86Screens[pPix->drawable.pScreen->myNum]);
    ApmPixmapPtr pPriv = APM_GET_PIXMAP_PRIVATE(pPix);

    if (pPriv->num)
	pApm->RushY[pPriv->num - 1] = 0;
    return (*pApm->DestroyPixmap)(pPix);
}

static PixmapPtr RushCreatePixmap(ScreenPtr pScreen, int w, int h, int depth)
{
    APMDECL(xf86Screens[pScreen->myNum]);
    PixmapPtr pPix = (*pApm->CreatePixmap)(pScreen, w, h, depth);
    ApmPixmapPtr pPriv = APM_GET_PIXMAP_PRIVATE(pPix);

    bzero(pPriv, sizeof(*pPriv));
    return pPix;
}

static int
xf86RushLockPixmap(int scrnIndex, PixmapPtr pix)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    APMDECL(pScrn);
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);
    ApmPixmapPtr pApmPriv = APM_GET_PIXMAP_PRIVATE(pix);
    XAAPixmapPtr pXAAPriv = XAA_GET_PIXMAP_PRIVATE(pix);
    FBAreaPtr area = pXAAPriv->offscreenArea;
    int	p2, width = (pScrn->displayWidth * pScrn->bitsPerPixel) / 8;

    if (pScrn->drv != &APM || pApm->Chipset != AT3D)
	return 0;
    pApm->apmLock = TRUE;
    pApmPriv->num = 0;
    if (area && pApm->pixelStride) {
	if (area->RemoveAreaCallback) {
	    (*area->RemoveAreaCallback)(area);
	    xf86FreeOffscreenArea(area);
	    area = NULL;
	}
	else
	    return 0;
    }
    if (area) {
	/*
	 * 1) Make it unmovable so that XAA won't know we're playing
	 *    with the cache.
	 * 2) Play musical chairs if needed.
	 */
	pApmPriv->MoveAreaCallback	= area->MoveAreaCallback;
	area->MoveAreaCallback		= NULL;
	pApmPriv->RemoveAreaCallback	= area->RemoveAreaCallback;
	area->RemoveAreaCallback	= NULL;
	pApmPriv->devPriv		= area->devPrivate.ptr;
    }
    else {
	pApmPriv->MoveAreaCallback	= NULL;
	pApmPriv->RemoveAreaCallback	= NULL;
	pApmPriv->devPriv		= NULL;
    }
    if (pApm->pixelStride || !area ||
	(((pix->drawable.x + pScrn->displayWidth * pix->drawable.y) *
					    pScrn->bitsPerPixel) & 32767)) {
	int		p1, i;

	/*
	 * Not aligned on a 4KB boundary, need to move it around.
	 */
	if (area)
	    xf86FreeOffscreenArea(area);
	if (pApm->pixelStride) {
	    area = xf86AllocateLinearOffscreenArea(pScrn->pScreen,
			    ((pix->drawable.width * pix->drawable.height *
			    pix->drawable.depth) / pScrn->bitsPerPixel) +
				4095,
			    pScrn->displayWidth, NULL, NULL, NULL);
	    if (!area) {
		xf86PurgeUnlockedOffscreenAreas(pScrn->pScreen);
		area = xf86AllocateLinearOffscreenArea(pScrn->pScreen,
			    ((pix->drawable.width * pix->drawable.height *
			    pix->drawable.depth) / pScrn->bitsPerPixel) +
				4095,
			    pScrn->displayWidth, NULL, NULL, NULL);
	    }
	    if (area) {
		if (!pApmPriv->devPriv) {
		    PixmapLinkPtr pLink;
		    ScreenPtr pScreen = pScrn->pScreen;
		    PixmapPtr pScreenPix;

		    pLink = xalloc(sizeof(PixmapLink));
		    if (!pLink) {
			xf86FreeOffscreenArea(area);
			return 0;
		    }
		    pXAAPriv->flags |= OFFSCREEN;
		    pix->devKind = pApm->pixelStride;
		    pScreenPix = (*pScreen->GetScreenPixmap)(pScreen);
		    pix->devPrivate.ptr = pScreenPix->devPrivate.ptr;
		    pLink->next = infoRec->OffscreenPixmaps;
		    pLink->pPix = pix;
		    infoRec->OffscreenPixmaps = pLink;
		}
		for (i = 0; i < 7; i++)
		    if (!pApm->RushY[i])
			break;
		pApmPriv->num = i + 1;
		pix->drawable.y = area->box.y1 + (i+1)*pApm->CurrentLayout.Scanlines;
		pApm->RushY[i] = area->box.y1;
		pix->drawable.x = (32768 - (((area->box.x1 + pScrn->displayWidth * area->box.y1) * pScrn->bitsPerPixel) & 32767)) / pApm->CurrentLayout.bitsPerPixel;
		if (pix->drawable.x == 32768 / pApm->CurrentLayout.bitsPerPixel)
		    pix->drawable.x = 0;
	    }
	}
	else {
	    p2 = 1;
	    while (!(p2 & width))
		p2 *= 2;
	    p1 = 4096 / p2 - 1;
	    switch(pScrn->bitsPerPixel) {
	    case 16:
		p2 /= 2;
		break;
	    case 32:
		p2 /= 4;
		break;
	    }
	    area = xf86AllocateOffscreenArea(pScrn->pScreen,
		    (pix->drawable.width * pix->drawable.bitsPerPixel) /
				    pScrn->bitsPerPixel,
		    pix->drawable.height + p1,
		    p2, NULL, NULL, pApmPriv->devPriv);
	    if (!area) {
		xf86PurgeUnlockedOffscreenAreas(pScrn->pScreen);
		area = xf86AllocateOffscreenArea(pScrn->pScreen,
			(pix->drawable.width * pix->drawable.bitsPerPixel) /
					pScrn->bitsPerPixel,
			pix->drawable.height + p1,
			p2, NULL, NULL, pApmPriv->devPriv);
	    }
	    if (area) {
		int	devKind = (pScrn->bitsPerPixel * pScrn->displayWidth) / 8;
		int	off = devKind * p1, h;
		int	goal = (-area->box.x1 * (pScrn->bitsPerPixel >> 3) - area->box.y1 * devKind) & 4095;

		if (!pApmPriv->devPriv) {
		    PixmapLinkPtr pLink;
		    ScreenPtr pScreen = pScrn->pScreen;
		    PixmapPtr pScreenPix;

		    pLink = xalloc(sizeof(PixmapLink));
		    if (!pLink) {
			xf86FreeOffscreenArea(area);
			return 0;
		    }
		    pXAAPriv->flags |= OFFSCREEN;
		    pix->devKind = pApm->CurrentLayout.bytesPerScanline;
		    pScreenPix = (*pScreen->GetScreenPixmap)(pScreen);
		    pix->devPrivate.ptr = pScreenPix->devPrivate.ptr;
		    pLink->next = infoRec->OffscreenPixmaps;
		    pLink->pPix = pix;
		    infoRec->OffscreenPixmaps = pLink;
		}
		pix->drawable.x = area->box.x1;
		for (h = p1; h >= 0; h--, off -= devKind)
		    if ((off & 4095) == goal)
			break;
		for (i = 0; i < 7; i++)
		    if (!pApm->RushY[i])
			break;
		pApmPriv->num = i + 1;
		pix->drawable.y = area->box.y1 + h + (i+1)*pApm->CurrentLayout.Scanlines;
		pApm->RushY[i] = area->box.y1 + h;
	    }
	}
	if (!area && (pXAAPriv->flags & OFFSCREEN)) {
	    /*
	     * Failed, return the old one
	     */
	    switch(pScrn->bitsPerPixel) {
		case 24:
		case  8:	p2 = 4; break;
		case 16:	p2 = 2; break;
		case 32:	p2 = 1; break;
		default:	p2 = 0; break;
	    }
	    pXAAPriv->offscreenArea =
		    area = xf86AllocateOffscreenArea(pScrn->pScreen,
				pix->drawable.width, pix->drawable.height,
				p2,
				pApmPriv->MoveAreaCallback,
				pApmPriv->RemoveAreaCallback,
				pApmPriv->devPriv);
	    /* The allocate can not fail: we just removed the old one. */
	    pix->drawable.x = area->box.x1;
	    pix->drawable.y = area->box.y1;
	}
	if (!area)
	    return 0;
	pXAAPriv->offscreenArea = area;
    }
    return pApm->LinAddress +
	    ((pix->drawable.x + pScrn->displayWidth *
	    (pix->drawable.y % pApm->CurrentLayout.Scanlines)) * pApm->CurrentLayout.bitsPerPixel) / 8;
}

static __inline__ void
__xf86UnlockPixmap(ApmPtr pApm, PixmapLinkPtr pLink)
{
    PixmapPtr pix = pLink->pPix;
    ApmPixmapPtr pApmPriv = APM_GET_PIXMAP_PRIVATE(pix);
    XAAPixmapPtr pXAAPriv = XAA_GET_PIXMAP_PRIVATE(pix);
    FBAreaPtr area = pXAAPriv->offscreenArea;
    int i;

    if (!area)
	area = pLink->area;
    if ((pXAAPriv->flags & OFFSCREEN) && !area->MoveAreaCallback && !area->RemoveAreaCallback) {
	area->MoveAreaCallback		= pApmPriv->MoveAreaCallback;
	area->RemoveAreaCallback	= pApmPriv->RemoveAreaCallback;
	area->devPrivate.ptr		= pApmPriv->devPriv;
    }
    i = pApmPriv->num;
    if (i) {
	pApm->RushY[i - 1] = 0;
	pix->drawable.y %= pApm->CurrentLayout.Scanlines;
    }
}

static void
xf86RushUnlockPixmap(int scrnIndex, PixmapPtr pix)
{
    APMDECL(xf86Screens[scrnIndex]);
    PixmapLinkPtr pLink = GET_XAAINFORECPTR_FROM_SCREEN(xf86Screens[scrnIndex]->pScreen)->OffscreenPixmaps;

    if (xf86Screens[scrnIndex]->drv != &APM || pApm->Chipset != AT3D)
	return;
    if (pApm->apmLock) {
	/*
	 * This is just an attempt, because Daryll is tampering with MY
	 * registers.
	 */
	if (!pApm->noLinear) {
	    CARD8	db;

	    db = RDXB(0xDB);
	    WRXB(0xDB, (db & 0xF4) |  0x0A);
	    ApmWriteSeq(0x1B, 0x20);
	    ApmWriteSeq(0x1C, 0x2F);
	}
	else {
	    CARD8	db;

	    db = RDXB_IOP(0xDB);
	    WRXB_IOP(0xDB, (db & 0xF4) |  0x0A);
	    wrinx(pApm->xport, 0x1B, 0x20);
	    wrinx(pApm->xport, 0x1C, 0x2F);
	}
	pApm->apmLock = FALSE;
    }
    while (pLink && pLink->pPix != pix)
	pLink = pLink->next;
    if (pLink)
	__xf86UnlockPixmap(pApm, pLink);
}

static void
xf86RushUnlockAllPixmaps()
{
    int		scrnIndex;

    for (scrnIndex = 0; scrnIndex < screenInfo.numScreens; scrnIndex++) {
	ScrnInfoPtr	pScrn = xf86Screens[scrnIndex];
	APMDECL(pScrn);
	PixmapLinkPtr pLink = GET_XAAINFORECPTR_FROM_SCREEN(pScrn->pScreen)->OffscreenPixmaps;

	if (pScrn->drv != &APM || pApm->Chipset != AT3D)
	    continue;
	while(pLink) {
	    __xf86UnlockPixmap(pApm, pLink);	
	    pLink = pLink->next;
	}    
    }
}

/*

Copyright (c) 1998 Daryll Strauss

*/

#define NEED_REPLIES
#define NEED_EVENTS
#include "X.h"
#include "Xproto.h"
#include "misc.h"
#include "dixstruct.h"
#include "extnsionst.h"
#include "scrnintstr.h"
#define _XF86RUSH_SERVER_
#include "xf86rushstr.h"

static unsigned char RushReqCode = 0;
static int RushErrorBase;

static DISPATCH_PROC(ProcXF86RushDispatch);
static DISPATCH_PROC(ProcXF86RushQueryVersion);
static DISPATCH_PROC(ProcXF86RushLockPixmap);
static DISPATCH_PROC(ProcXF86RushUnlockPixmap);
static DISPATCH_PROC(ProcXF86RushUnlockAllPixmaps);
static DISPATCH_PROC(ProcXF86RushSetCopyMode);
#if 0
static DISPATCH_PROC(ProcXF86RushSetPixelStride);
#endif
static DISPATCH_PROC(ProcXF86RushOverlayPixmap);
static DISPATCH_PROC(ProcXF86RushStatusRegOffset);
static DISPATCH_PROC(ProcXF86RushAT3DEnableRegs);
static DISPATCH_PROC(ProcXF86RushAT3DDisableRegs);

static int rush_ext_generation = -1;

static DISPATCH_PROC(SProcXF86RushDispatch);

static void XF86RushResetProc(ExtensionEntry* extEntry);

void
XFree86RushExtensionInit(ScreenPtr pScreen)
{
    ExtensionEntry* extEntry;

#ifdef PANORAMIX
    if (!noPanoramiXExtension)
	return;
#endif
    if (rush_ext_generation == serverGeneration) {
	if (xf86Screens[pScreen->myNum]->drv == &APM &&
		APMPTR(xf86Screens[pScreen->myNum])->Chipset == AT3D) {
	    pScreen->CreatePixmap	= RushCreatePixmap;
	    pScreen->DestroyPixmap	= RushDestroyPixmap;
	}
	return;
    }
    rush_ext_generation = serverGeneration;
    if ((extEntry = AddExtension(XF86RUSHNAME,
				XF86RushNumberEvents,
				XF86RushNumberErrors,
				ProcXF86RushDispatch,
				SProcXF86RushDispatch,
				XF86RushResetProc,
				StandardMinorOpcode))) {
	RushReqCode = (unsigned char)extEntry->base;
	RushErrorBase = extEntry->errorBase;
	if (xf86Screens[pScreen->myNum]->drv == &APM &&
		APMPTR(xf86Screens[pScreen->myNum])->Chipset == AT3D) {
	    pScreen->CreatePixmap	= RushCreatePixmap;
	    pScreen->DestroyPixmap	= RushDestroyPixmap;
	}
    }
    else {
	pScreen->CreatePixmap	= APMPTR(xf86Screens[pScreen->myNum])->CreatePixmap;
	pScreen->DestroyPixmap	= APMPTR(xf86Screens[pScreen->myNum])->DestroyPixmap;
    }
}

/*ARGSUSED*/
static void
XF86RushResetProc (ExtensionEntry *extEntry)
{
}

static int
ProcXF86RushQueryVersion(register ClientPtr client)
{
    xXF86RushQueryVersionReply rep;
    register int n;

    REQUEST_SIZE_MATCH(xXF86RushQueryVersionReq);
    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.majorVersion = XF86RUSH_MAJOR_VERSION;
    rep.minorVersion = XF86RUSH_MINOR_VERSION;
    if (client->swapped) {
    	swaps(&rep.sequenceNumber, n);
    	swapl(&rep.length, n);
    }
    WriteToClient(client, sz_xXF86RushQueryVersionReply, (char *)&rep);
    return (client->noClientException);
}

static int
ProcXF86RushLockPixmap(register ClientPtr client)
{
  REQUEST(xXF86RushLockPixmapReq);
  xXF86RushLockPixmapReply rep;
  PixmapPtr pix;

  if (stuff->screen > screenInfo.numScreens)
    return BadValue;

  REQUEST_SIZE_MATCH(xXF86RushLockPixmapReq);
  rep.type = X_Reply;
  rep.length = 0;
  rep.sequenceNumber = client->sequence;
  pix = (PixmapPtr)SecurityLookupIDByType(client,
					  stuff->pixmap, RT_PIXMAP, 
					  SecurityReadAccess);
  rep.addr = xf86RushLockPixmap(stuff->screen, pix);

  WriteToClient(client, SIZEOF(xXF86RushLockPixmapReply), (char*)&rep);
  return client->noClientException;
}

static int
ProcXF86RushUnlockPixmap(register ClientPtr client)
{
  REQUEST(xXF86RushUnlockPixmapReq);
  PixmapPtr pix;

  if (stuff->screen > screenInfo.numScreens)
    return BadValue;

  REQUEST_SIZE_MATCH(xXF86RushUnlockPixmapReq);
  pix = (PixmapPtr)SecurityLookupIDByType(client,
					  stuff->pixmap, RT_PIXMAP, 
					  SecurityReadAccess);
  xf86RushUnlockPixmap(stuff->screen, pix);
  return client->noClientException;
}

static int
ProcXF86RushUnlockAllPixmaps(register ClientPtr client)
{

  REQUEST_SIZE_MATCH(xXF86RushUnlockAllPixmapsReq);
  xf86RushUnlockAllPixmaps();
  return client->noClientException;
}

static int
ProcXF86RushSetCopyMode(register ClientPtr client)
{
  REQUEST(xXF86RushSetCopyModeReq);

  REQUEST_SIZE_MATCH(xXF86RushSetCopyModeReq);
  APMPTR(xf86Screens[stuff->screen])->CopyMode = stuff->CopyMode;
  return client->noClientException;
}

#if 0
static int
ProcXF86RushSetPixelStride(register ClientPtr client)
{
  REQUEST(xXF86RushSetPixelStrideReq);

  REQUEST_SIZE_MATCH(xXF86RushSetPixelStrideReq);
  APMPTR(xf86Screens[stuff->screen])->pixelStride = stuff->PixelStride;
  return client->noClientException;
}
#endif

int
ProcXF86RushDispatch (register ClientPtr client)
{
    REQUEST(xReq);

    if (!LocalClient(client))
	return RushErrorBase + XF86RushClientNotLocal;

    switch (stuff->data)
    {
    case X_XF86RushQueryVersion:
	return ProcXF86RushQueryVersion(client);
    case X_XF86RushLockPixmap:
        return ProcXF86RushLockPixmap(client);
    case X_XF86RushUnlockPixmap:
        return ProcXF86RushUnlockPixmap(client);
    case X_XF86RushUnlockAllPixmaps:
        return ProcXF86RushUnlockAllPixmaps(client);
    case X_XF86RushSetCopyMode:
        return ProcXF86RushSetCopyMode(client);
#if 0
    case X_XF86RushSetPixelStride:
        return ProcXF86RushSetPixelStride(client);
#endif
    case X_XF86RushOverlayPixmap:
	return ProcXF86RushOverlayPixmap(client);
    case X_XF86RushStatusRegOffset:
	return ProcXF86RushStatusRegOffset(client);
    case X_XF86RushAT3DEnableRegs:
	return ProcXF86RushAT3DEnableRegs(client);
    case X_XF86RushAT3DDisableRegs:
	return ProcXF86RushAT3DDisableRegs(client);
    default:
	return BadRequest;
    }
}

int
SProcXF86RushDispatch (register ClientPtr client)
{
    return RushErrorBase + XF86RushClientNotLocal;
}

#include "xvdix.h"
/*
 * The one below is just a subtle modification of ProcXvShmPutImage
 */

static int 
ProcXF86RushOverlayPixmap(ClientPtr client)
{
    DrawablePtr		pDraw;
    ScrnInfoPtr		pScrn;
    PixmapPtr		pPixmap;
    XvPortPtr		pPort;
    XvImagePtr		pImage = NULL;
    GCPtr		pGC;
    int			status, i;
    unsigned char	*offset;
    ApmPtr		pApm;
    ApmPixmapPtr	pPriv;
    REQUEST(xXF86RushOverlayPixmapReq);

    REQUEST_SIZE_MATCH(xXF86RushOverlayPixmapReq);

    VALIDATE_DRAWABLE_AND_GC(stuff->drawable, pDraw, pGC, client);

    pScrn = xf86Screens[pDraw->pScreen->myNum];
    pApm = APMPTR(pScrn);
    if (pScrn->drv != &APM || pApm->Chipset != AT3D)
	return (_XvBadPort);

    if(!(pPort = LOOKUP_PORT(stuff->port, client) )) {
	client->errorValue = stuff->port;
	return (_XvBadPort);
    }

    if (pPort->id != stuff->port) {
	if ((status = (*pPort->pAdaptor->ddAllocatePort)(stuff->port, pPort, &pPort)) != Success) {
	    client->errorValue = stuff->port;
	    return (status);
	}
    }

    if (!(pPort->pAdaptor->type & XvImageMask) ||
	!(pPort->pAdaptor->type & XvInputMask)) {
	client->errorValue = stuff->port;
	return (BadMatch);
    }

    status = XVCALL(diMatchPort)(pPort, pDraw);
    if (status != Success)
	return status;

    pPixmap = (PixmapPtr)SecurityLookupIDByType(client,
					stuff->pixmap, RT_PIXMAP, 
					SecurityReadAccess);
    if (!pPixmap) {
	client->errorValue = stuff->pixmap;
	return (BadPixmap);
    }
    status = XVCALL(diMatchPort)(pPort, (DrawablePtr)pPixmap);
    if (status != Success)
	return status;
    pPriv = APM_GET_PIXMAP_PRIVATE(pPixmap);
    pApm = APMPTR(pScrn);
    if (pPriv->num == 0) {
	client->errorValue = stuff->pixmap;
	return (BadMatch);
    }
    offset = (unsigned char *)pApm->FbBase +
	    pApm->RushY[pPriv->num - 1] * pApm->CurrentLayout.bytesPerScanline +
	    pPixmap->drawable.x * pScrn->bitsPerPixel / 8;

    for(i = 0; i < pPort->pAdaptor->nImages; i++) {
	if(pPort->pAdaptor->pImages[i].id == stuff->id) {
	    pImage = &(pPort->pAdaptor->pImages[i]);
	    break;
	}
    }

    if(!pImage)
	return BadMatch;

    pApm->PutImageStride = pPixmap->devKind;
    status = XVCALL(diPutImage)(client, pDraw, pPort, pGC,
				stuff->src_x, stuff->src_y,
				stuff->src_w, stuff->src_h,
				stuff->drw_x, stuff->drw_y,
				stuff->drw_w, stuff->drw_h,
				pImage, offset, TRUE,
				pPixmap->drawable.width,
				pPixmap->drawable.height);
    pApm->PutImageStride = 0;

    return status;
}

static int
ProcXF86RushStatusRegOffset(ClientPtr client)
{
    int		scrnIndex;
    ScrnInfoPtr	pScrn;
    ApmPtr	pApm;
    REQUEST(xXF86RushStatusRegOffsetReq);
    xXF86RushStatusRegOffsetReply rep;
    register int n;

    REQUEST_SIZE_MATCH(xXF86RushStatusRegOffsetReq);
    scrnIndex = stuff->screen;
    if (scrnIndex < 0 || scrnIndex > screenInfo.numScreens)
	return BadValue;
    pScrn = xf86Screens[scrnIndex];
    pApm = APMPTR(pScrn);
    if (pScrn->drv != &APM || pApm->Chipset != AT3D)
	return BadMatch;
    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.offset = 0xFFEDF4;
    if (client->swapped) {
    	swapl(&rep.offset, n);
    }
    WriteToClient(client, sz_xXF86RushStatusRegOffsetReply, (char *)&rep);

    return client->noClientException;
}

static int
ProcXF86RushAT3DEnableRegs(ClientPtr client)
{
    u32		db, tmp;
    int		scrnIndex;
    ScrnInfoPtr	pScrn;
    ApmPtr	pApm;
    REQUEST(xXF86RushAT3DEnableRegsReq);

    REQUEST_SIZE_MATCH(xXF86RushAT3DEnableRegsReq);
    scrnIndex = stuff->screen;
    if (scrnIndex < 0 || scrnIndex > screenInfo.numScreens)
	return BadValue;
    pScrn = xf86Screens[scrnIndex];
    pApm = APMPTR(pScrn);
    if (pScrn->drv != &APM || pApm->Chipset != AT3D)
	return BadMatch;
    pApm->Rush = 0x04;
    if (!pApm->noLinear) {
	db = RDXL(0xDB);
	WRXL(0xDB, db | 0x04);
	WRXB(0x110, 0x03);
	tmp = RDXB(0x1F0);
	WRXB(0x1F0, tmp | 0xD0);
	tmp = RDXB(0x1F1);
	WRXB(0x1F1, (tmp & ~0xC0) | 0x10);
	tmp = RDXB(0x1F2);
	WRXB(0x1F2, tmp | 0x10);
    }
    else {
	db = RDXL(0xDB);
	WRXL_IOP(0xDB, db | 0x04);
	WRXB_IOP(0x110, 0x03);
	tmp = RDXB_IOP(0x1F0);
	WRXB_IOP(0x1F0, tmp | 0xD0);
	tmp = RDXB_IOP(0x1F1);
	WRXB_IOP(0x1F1, (tmp & ~0xC0) | 0x10);
	tmp = RDXB_IOP(0x1F2);
	WRXB_IOP(0x1F2, tmp | 0x10);
    }

    return client->noClientException;
}

static int
ProcXF86RushAT3DDisableRegs(ClientPtr client)
{
    u32		db, tmp;
    int		scrnIndex;
    ScrnInfoPtr	pScrn;
    ApmPtr	pApm;
    REQUEST(xXF86RushAT3DDisableRegsReq);

    REQUEST_SIZE_MATCH(xXF86RushAT3DDisableRegsReq);
    scrnIndex = stuff->screen;
    if (scrnIndex < 0 || scrnIndex > screenInfo.numScreens)
	return BadValue;
    pScrn = xf86Screens[scrnIndex];
    pApm = APMPTR(pScrn);
    if (pScrn->drv != &APM || pApm->Chipset != AT3D)
	return BadMatch;
    if (!pApm->noLinear) {
	tmp = RDXB(0x1F2);
	WRXB(0x1F2, tmp & ~0x10);
	tmp = RDXB(0x1F0);
	WRXB(0x1F0, tmp & ~0xD0);
	WRXB(0x110, 0);
	pApm->Rush = 0x00;
	db = RDXL(0xDB);
	WRXL(0xDB, db & ~0x04);
    }
    else {
	tmp = RDXB_IOP(0x1F2);
	WRXB_IOP(0x1F2, tmp & ~0x10);
	tmp = RDXB_IOP(0x1F0);
	WRXB_IOP(0x1F0, tmp & ~0xD0);
	WRXB_IOP(0x110, 0);
	pApm->Rush = 0x00;
	db = RDXL_IOP(0xDB);
	WRXL_IOP(0xDB, db & ~0x04);
    }

    return client->noClientException;
}
