/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/apm/apm_video.c,v 1.12 2003/11/10 18:22:17 tsi Exp $ */

#if PSZ != 24
#include "dixstruct.h"
#include "fourcc.h"

/*
 * Ported from mga_video.c by Loïc Grenié
 */

#ifndef OFF_DELAY
#define OFF_DELAY	200
#endif

static XF86VideoAdaptorPtr A(SetupImageVideo)(ScreenPtr);

static void	A(StopVideo)(ScrnInfoPtr, pointer, Bool);
static int	A(SetPortAttribute)(ScrnInfoPtr, Atom, INT32, pointer);
#ifndef IOP_ACCESS
static int	ApmGetPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);
static void	ApmQueryBestSize(ScrnInfoPtr, Bool, short, short, short,
				    short, unsigned int *, unsigned int *,
				    pointer);
static int	ApmQueryImageAttributes(ScrnInfoPtr, int,
					    unsigned short *, unsigned short *,
					    int *, int *);
#endif
static int	A(ReputImage)(ScrnInfoPtr, short, short, RegionPtr, pointer);
static int	A(PutImage)(ScrnInfoPtr, short, short, short, short, short,
				short, short, short, int, unsigned char*,
				short, short, Bool, RegionPtr, pointer);

static void	A(ResetVideo)(ScrnInfoPtr);
static void	A(XvMoveCB)(FBAreaPtr, FBAreaPtr);
static void	A(XvRemoveCB)(FBAreaPtr);

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

void A(InitVideo)(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors;
    XF86VideoAdaptorPtr newAdaptor;
    APMDECL(pScrn);
    int num_adaptors;
    Bool freeAdaptors = FALSE;

    num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);

    if (pApm->Chipset >= AT24) {
	if ((newAdaptor = A(SetupImageVideo)(pScreen))) {

	   newAdaptors = xalloc((num_adaptors + 1) *
				   sizeof(XF86VideoAdaptorPtr*));
	   if(newAdaptors) {
		if(num_adaptors)
		    memcpy(newAdaptors, adaptors, num_adaptors *
					sizeof(XF86VideoAdaptorPtr));
		newAdaptors[num_adaptors] = newAdaptor;
		adaptors = newAdaptors;
		num_adaptors++;
		freeAdaptors = TRUE;
	   }
	}
    }

    if(num_adaptors)
        xf86XVScreenInit(pScreen, adaptors, num_adaptors);

    if(freeAdaptors)
	xfree(adaptors);
}

#ifndef APM_VIDEO_DEFINES
#define APM_VIDEO_DEFINES

static Atom xvBrightness, xvContrast;

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[1] =
{
    {
	0,
	"XV_IMAGE",
	1024, 1024,
	{1, 1}
    }
};

#define NUM_FORMATS 24

static XF86VideoFormatRec Formats[NUM_FORMATS] =
{
    { 8, PseudoColor},
    {15, PseudoColor},
    {16, PseudoColor},
    {24, PseudoColor},
    {32, PseudoColor},
    { 8, DirectColor},
    {15, DirectColor},
    {16, DirectColor},
    {24, DirectColor},
    {32, DirectColor},
    { 8,   TrueColor},
    {15,   TrueColor},
    {16,   TrueColor},
    {24,   TrueColor},
    {32,   TrueColor}
};

#define NUM_ATTRIBUTES 2

static XF86AttributeRec Attributes[NUM_ATTRIBUTES] =
{
    {XvSettable | XvGettable, -128, 127, "XV_BRIGHTNESS"},
    {XvSettable | XvGettable, 0, 255, "XV_CONTRAST"}
};

#define NUM_IMAGES 9
typedef char c8;

static XF86ImageRec Images[NUM_IMAGES] =
{
   {
	0x35315652,
        XvRGB,
	LSBFirst,
	{'R','V','1','5',
	  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	16,
	XvPacked,
	1,
	15, 0x001F, 0x03E0, 0x7C00,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	{'R','V','B',0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
   {
	0x36315652,
        XvRGB,
	LSBFirst,
	{'R','V','1','6',
	  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	16,
	XvPacked,
	1,
	16, 0x001F, 0x07E0, 0xF800,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	{'R','V','B',0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
   {
	0x32335652,
        XvRGB,
	LSBFirst,
	{'R','V','3','2',
	  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	32,
	XvPacked,
	1,
	24, 0x0000FF, 0x00FF00, 0xFF0000,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	{'R','V','B',0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
   XVIMAGE_YUY2,
   {
	0x59595959,
        XvYUV,
	LSBFirst,
	{0x00,0x00,0x00,0x00,
	  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	8,
	XvPacked,
	1,
	0, 0, 0, 0,
	8, 0, 0,
	1, 1, 1,
	1, 1, 1,
	{'Y','Y','Y','Y',
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
   {
	0x32315659,
        XvYUV,
	LSBFirst,
	{'Y','V','1','2',
	  0x00,0x00,0x00,0x10,(c8)0x80,0x00,0x00,(c8)0xAA,0x00,0x38,(c8)0x9B,0x71},
	12,
	XvPlanar,
	3,
	0, 0, 0, 0 ,
	8, 8, 8,
	1, 2, 2,
	1, 2, 2,
	{'Y','V','U',
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
   {
	0x59565955,
        XvYUV,
	LSBFirst,
	{'U','Y','V','Y',
	  0x00,0x00,0x00,0x10,(c8)0x80,0x00,0x00,(c8)0xAA,0x00,0x38,(c8)0x9B,0x71},
	16,
	XvPlanar,
	1,
	0, 0, 0, 0,
	8, 8, 8,
	1, 2, 2,
	1, 1, 1,
	{'U','Y','V','Y',
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
   {
	0x55595659,
        XvYUV,
	LSBFirst,
	{'Y','V','Y','U',
	  0x00,0x00,0x00,0x10,(c8)0x80,0x00,0x00,(c8)0xAA,0x00,0x38,(c8)0x9B,0x71},
	16,
	XvPlanar,
	1,
	0, 0, 0, 0,
	8, 8, 8,
	1, 2, 2,
	1, 1, 1,
	{'Y','V','Y','U',
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
   {
	0x59555956,
        XvYUV,
	LSBFirst,
	{'V','Y','U','Y',
	  0x00,0x00,0x00,0x10,(c8)0x80,0x00,0x00,(c8)0xAA,0x00,0x38,(c8)0x9B,0x71},
	16,
	XvPlanar,
	1,
	0, 0, 0, 0,
	8, 8, 8,
	1, 2, 2,
	1, 1, 1,
	{'V','Y','U','Y',
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   }
};

typedef struct {
   Bool		on;
   unsigned char	brightness;
   unsigned char	contrast;
   unsigned short	reg, val;
   ApmPtr	pApm;
   int		x1, x10, y1, drw_x, drw_y, Bpp, Bps;
   FBAreaPtr	area;
   RegionRec	clip;
   int		xnum, xden, ynum, yden;
   CARD32	scalex, scaley;
   CARD32	data;
} ApmPortPrivRec, *ApmPortPrivPtr;
#endif


static void
A(ResetVideo)(ScrnInfoPtr pScrn)
{
    APMDECL(pScrn);

    A(WaitForFifo)(pApm, 2);
    ((ApmPortPrivPtr)pApm->adaptor->pPortPrivates[0].ptr)->on = 0;
    ((ApmPortPrivPtr)pApm->adaptor->pPortPrivates[1].ptr)->on = 0;
    WRXW(0x82, 0);
    WRXW(0x92, 0);
}


static XF86VideoAdaptorPtr
A(SetupImageVideo)(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    APMDECL(pScrn);
    XF86VideoAdaptorPtr adapt;
    ApmPortPrivPtr pPriv;

    if(!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
			    2 * sizeof(ApmPortPrivRec) +
			    2 * sizeof(DevUnion))))
	return NULL;

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = VIDEO_OVERLAID_IMAGES;
    adapt->name = "Alliance Pro Motion video engine";
    adapt->nEncodings = 1;
    adapt->pEncodings = DummyEncoding;
    adapt->nFormats = NUM_FORMATS;
    adapt->pFormats = Formats;
    adapt->nPorts = 2;
    adapt->pPortPrivates = (DevUnion*)(&adapt[1]);
    pPriv = (ApmPortPrivPtr)(&adapt->pPortPrivates[2]);
    pPriv->pApm = pApm;
    pPriv[1].pApm = pApm;
    pPriv->reg = 0x82;
    pPriv[1].reg = 0x92;
    adapt->pPortPrivates[0].ptr = (pointer)(pPriv);
    adapt->pPortPrivates[1].ptr = (pointer)(pPriv + 1);
    adapt->nAttributes = NUM_ATTRIBUTES;
    adapt->pAttributes = Attributes;
    adapt->nImages = NUM_IMAGES;
    adapt->pImages = Images;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = A(StopVideo);
    adapt->SetPortAttribute = A(SetPortAttribute);
    adapt->GetPortAttribute = ApmGetPortAttribute;
    adapt->QueryBestSize = ApmQueryBestSize;
    adapt->PutImage = A(PutImage);
    adapt->ReputImage = A(ReputImage);
    adapt->QueryImageAttributes = ApmQueryImageAttributes;

    pPriv->brightness = 0;
    pPriv->contrast = 128;
    pPriv[1].brightness = 0;
    pPriv[1].contrast = 128;

    /* gotta uninit this someplace */
    REGION_NULL(pScreen, &pPriv->clip);
    REGION_NULL(pScreen, &(pPriv + 1)->clip);

    pApm->adaptor = adapt;

    xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
    xvContrast   = MAKE_ATOM("XV_CONTRAST");

    A(ResetVideo)(pScrn);

    return adapt;
}

#ifndef IOP_ACCESS
/* ApmClipVideo -

   Takes the dst box in standard X BoxRec form (top and left
   edges inclusive, bottom and right exclusive).  The new dst
   box is returned.  The source boundaries are given (x1, y1
   inclusive, x2, y2 exclusive) and returned are the new source
   boundaries in 16.16 fixed point.

  extents is the extents of the clip region
*/

static void
ApmClipVideo(BoxPtr dst, INT32 *x1, INT32 *x2, INT32 *y1, INT32 *y2,
	      BoxPtr extents, INT32 width, INT32 height,
	      CARD32 *scalex, CARD32 *scaley, INT32 mask)
{
    INT32 vscale, hscale;
    int diff;

    if (dst->x2 - dst->x1 < *x2 - *x1)
	dst->x2 = dst->x1 + *x2 - *x1;

    if (dst->y2 - dst->y1 < *y2 - *y1)
	dst->y2 = dst->y1 + *y2 - *y1;

    *x1 <<= 12; *x2 <<= 16;
    *y1 <<= 12; *y2 <<= 16;

    hscale = (*x2 - *x1) / (dst->x2 - dst->x1);
    vscale = (*y2 - *y1) / (dst->y2 - dst->y1);

    diff = extents->x1 - dst->x1;
    if(diff > 0) {
	dst->x1 = extents->x1;
	*x1 += diff * hscale;
    }
    diff = dst->x2 - extents->x2;
    if(diff > 0) {
	dst->x2 = extents->x2;
	*x2 -= diff * hscale;
    }
    diff = extents->y1 - dst->y1;
    if(diff > 0) {
	dst->y1 = extents->y1;
	*y1 += diff * vscale;
    }
    diff = dst->y2 - extents->y2;
    if(diff > 0) {
	dst->y2 = extents->y2;
	*y2 -= diff * vscale;
    }

    if (*x2 - *x1 == 0x10000 * (dst->x2 - dst->x1))	/* Shrinking */
	*scalex = 0;
    else
	*scalex = ((*x2 - *x1) / (dst->x2 - dst->x1)) >> 4;
    if (*y2 - *y1 == 0x10000 * (dst->y2 - dst->y1))	/* Shrinking */
	*scaley = 0;
    else
	*scaley = ((*y2 - *y1) / (dst->y2 - dst->y1)) >> 4;
}
#endif

static void
A(StopVideo)(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
    ApmPortPrivPtr pPriv = (ApmPortPrivPtr)data;
    APMDECL(pScrn);

    REGION_EMPTY(pScrn->pScreen, &pPriv->clip);

    pPriv->on = 0;
    A(WaitForFifo)(pApm, 1);
    WRXB(pPriv->reg, 0);
}

static int
A(SetPortAttribute)(ScrnInfoPtr pScrn, Atom attribute, INT32 value,
		      pointer data)
{
  ApmPortPrivPtr pPriv = (ApmPortPrivPtr)data;
  /*APMDECL(pScrn);*/

  if(attribute == xvBrightness) {
	if((value < -128) || (value > 127))
	   return BadValue;
	pPriv->brightness = value;
	/* TODO : enable */
  } else if(attribute == xvContrast) {
	if((value < 0) || (value > 255))
	   return BadValue;
	pPriv->contrast = value;
	/* TODO : enable */
  }

  return Success;
}

#ifndef IOP_ACCESS
static int
ApmGetPortAttribute(ScrnInfoPtr pScrn, Atom attribute, INT32 *value,
		      pointer data)
{
  ApmPortPrivPtr pPriv = (ApmPortPrivPtr)data;

  if(attribute == xvBrightness) {
	*value = pPriv->brightness;
  } else
  if(attribute == xvContrast) {
	*value = pPriv->contrast;
  }

  return Success;
}

static void
ApmQueryBestSize(ScrnInfoPtr pScrn, Bool motion, short vid_w, short vid_h,
		  short drw_w, short drw_h,
		  unsigned int *p_w, unsigned int *p_h, pointer data)
{
    APMDECL(pScrn);
    unsigned short	round = ~pApm->CurrentLayout.mask32;

    *p_w = drw_w & round;
    *p_h = drw_h & round;
}
#endif

static void A(XvMoveCB)(FBAreaPtr area1, FBAreaPtr area2)
{
    ApmPortPrivPtr	pPriv = (ApmPortPrivPtr)area1->devPrivate.ptr;
    ApmPtr		pApm = pPriv->pApm;

    pPriv->on = 0;
    A(WaitForFifo)(pApm, 1);
    WRXB(pPriv->reg, 0);	/* Stop video for this port */
    pPriv->area = area2;
}

static void A(XvRemoveCB)(FBAreaPtr area)
{
    ApmPortPrivPtr	pPriv = (ApmPortPrivPtr)area->devPrivate.ptr;
    ApmPtr		pApm = pPriv->pApm;

    pPriv->on = 0;
    A(WaitForFifo)(pApm, 1);
    WRXB(pPriv->reg, 0);	/* Stop video for this port */
    pPriv->area = NULL;
}

static int
A(ReputImage)(ScrnInfoPtr pScrn, short drw_x, short drw_y,
		RegionPtr clipBoxes, pointer pdata)
{
    ScreenPtr		pScreen = pScrn->pScreen;
    APMDECL(pScrn);
    ApmPortPrivPtr	pPriv = pdata, pPriv0, pPriv1;
    register int	fx, fy;
    CARD32	mask;
    RegionRec	Union;
    RegionPtr	reg0;
    int		nrects, CurrY, tile;
    int		X1, X2, Y1, y2, xmax, ymax;
    BoxPtr	rects;
    Bool	didit = 0;

    mask = pApm->CurrentLayout.mask32;
    fx = pScrn->frameX0 & ~mask;
    fy = pScrn->frameY0 + 1;
    REGION_COPY(pScreen, &pPriv->clip, clipBoxes);
    pPriv->x1 += drw_x - pPriv->drw_x;
    pPriv->x10 = ((pPriv->x1 + mask) & ~mask) - fx;
    pPriv->y1 += drw_y - pPriv->drw_y;
    pPriv->drw_x = drw_x;
    pPriv->drw_y = drw_y;
    A(WaitForFifo)(pApm, 2);
    WRXW(pPriv->reg + 0x06, 0xFFF - ((pPriv->scalex * pPriv->x10) & 0xFFF));
    WRXW(pPriv->reg + 0x0A, 0xFFF - ((pPriv->scaley * pPriv->y1) & 0xFFF));
    pPriv0 = (ApmPortPrivPtr)pApm->adaptor->pPortPrivates[0].ptr;
    pPriv1 = (ApmPortPrivPtr)pApm->adaptor->pPortPrivates[1].ptr;
    reg0 = &pPriv0->clip;
    bzero(&Union, sizeof Union);
    REGION_EMPTY(pScreen, &Union);
    REGION_NULL(pScreen, &Union);
    REGION_UNION(pScreen, &Union, reg0, &pPriv1->clip);
    nrects = REGION_NUM_RECTS(&Union);
    rects = REGION_RECTS(&Union);
    tile = 0x200;
    xmax = pScrn->frameX1 - pScrn->frameX0 + 1;
    ymax = pScrn->frameY1 - pScrn->frameY0;
    CurrY = -1;
    goto BEGIN_LOOP_1;
    do {
	rects++;
BEGIN_LOOP_1:
	X1 = ((rects->x1 + mask) & ~mask) - fx;
	if (X1 < 0)
	    X1 = 0;
	X2 = (rects->x2 & ~mask) - fx;
	if (X2 > xmax)
	    X2 = xmax;
	y2 = rects->y2 - fy;
    } while ((X2 <= X1 || y2 < -1) && --nrects > 0);
    Y1 = rects->y1 - fy;

    while (!(STATUS() & 0x800));
    while (STATUS() & 0x800);
    while (nrects-- > 0) {
	CARD32	reg, data;
	int	x1, x2, y1;

	x1 = X1;
	x2 = X2;
	y1 = Y1;
	if (y1 < -1) y1 = -1;
	if (y1 > ymax)
	    break;
	didit = 1;
	if (y1 > CurrY) {
	    A(WaitForFifo)(pApm, 3);
	    WRXL(tile + 0x00, 0xFFF0011);
	    WRXL(tile + 0x04, y1 << 16);
	    WRXL(tile + 0x08, 0);
	    tile += 16;
	}
	if (RECT_IN_REGION(pScreen, reg0, rects)) {
	    pPriv = pPriv0;
	    reg = (x1 << 16) | 1;
	}
	else {
	    pPriv = pPriv1;
	    reg = (x1 << 16) | 2;
	}
	CurrY = y2;
	if (nrects <= 0)
	    goto BEGIN_LOOP_2;
	do {
	    rects++;
BEGIN_LOOP_2:
	    X1 = ((rects->x1 + mask) & ~mask) - fx;
	    if (X1 < 0)
		X1 = 0;
	    X2 = (rects->x2 & ~mask) - fx;
	    if (X2 > xmax)
		X2 = xmax;
	} while (X2 <= X1 && --nrects > 0);
	Y1 = rects->y1 - fy;
	y2 = rects->y2 - fy;
	data = pPriv->data + (((x1 - pPriv->x10)
				* pPriv->xden) / pPriv->xnum) * pPriv->Bpp +
	    (((y1 - pPriv->y1 + fy) * pPriv->yden) / pPriv->ynum) * pPriv->Bps;
	A(WaitForFifo)(pApm, 4);
	if (!nrects || tile == 0x2B0 || y1 < Y1) {
	    WRXL(tile   , 0x10 | reg);
	}
	else {
	    WRXL(tile   , reg);
	}
	WRXL(tile + 0x04, x2 | (CurrY << 16));
	WRXL(tile + 0x08, (((x2-x1)*pPriv->xden+pPriv->xnum-1) / pPriv->xnum) |
				(data << 16));
	WRXB(tile + 0x0C, data >> 16);
	tile += 16;
	if (tile == 0x2C0) {
	    tile = 0x200;
	    break;
	}
    }
    REGION_UNINIT(pScreen, &Union);

    if (didit) {
	A(WaitForFifo)(pApm, 1);
	WRXW(0x8E, tile - 0x200);
    }

    if (didit ^ ((pPriv0->val | pPriv1->val) & 1)) {
	if (didit) {
	    pPriv0->val |= 1;
	    pPriv1->val |= 1;
	}
	else {
	    pPriv0->val &= 0xFFFE;
	    pPriv1->val &= 0xFFFE;
	}
	if (pPriv0->on) {
	    A(WaitForFifo)(pApm, 1);
	    WRXW(0x82, pPriv0->val);
	}
	if (pPriv1->on) {
	    A(WaitForFifo)(pApm, 1);
	    WRXW(0x92, pPriv1->val);
	}
    }

    return Success;
}

static int
A(PutImage)(ScrnInfoPtr pScrn, short src_x, short src_y,
	      short drw_x, short drw_y, short src_w, short src_h,
	      short drw_w, short drw_h, int id, unsigned char* buf,
	      short width, short height, Bool sync, RegionPtr clipBoxes,
	      pointer data)
{
    ApmPortPrivPtr	pPriv = (ApmPortPrivPtr)data;
    ScreenPtr	pScreen = pScrn->pScreen;
    APMDECL(pScrn);
    INT32	x1, x2, y1, y2;
    unsigned char	*dst_start;
    int		pitch, Bpp, new_h, offset = 0, offset2 = 0, offset3 = 0;
    CARD32	mask;
    FBAreaPtr	area;
    int		srcPitch, dstPitch, srcPitch2 = 0;
    int		top, left, npixels, nlines;
    BoxRec	dstBox;
    CARD32	scalex, scaley, scale;
    CARD32	tmp;
    Bool	offscreen;

    offscreen = (buf < (unsigned char *)pApm->FbBase ||
		    buf > (unsigned char *)pApm->FbBase + 0x400000);

    if(drw_w > 16384) drw_w = 16384;

    /* Clip */
    x1 = src_x;
    x2 = src_x + src_w;
    y1 = src_y;
    y2 = src_y + src_h;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;

    mask = pApm->CurrentLayout.mask32;

    ApmClipVideo(&dstBox, &x1, &x2, &y1, &y2,
		REGION_EXTENTS(pScreen, clipBoxes), width, height,
		&scalex, &scaley, mask);

    pPriv->drw_x = drw_x;
    pPriv->drw_y = drw_y;
    pPriv->xnum = drw_w;
    if (scalex)
	pPriv->xden = src_w;
    else
	pPriv->xden = drw_w;	/* If image is larger than window */
    pPriv->ynum = drw_h;
    if (scaley)
	pPriv->yden = src_h;
    else
	pPriv->yden = drw_h;
    if((x1 - x2 >= 0xFFFF) || (y1 - y2 >= 0xFFFF))
     return Success;

    Bpp = pScrn->bitsPerPixel >> 3;
    pitch = Bpp * pScrn->displayWidth;

    switch(id) {
    case 0x32315659:
	dstPitch = ((width << 1) + 3) & ~3;
	srcPitch = (width + 3) & ~3;
	offset2 = srcPitch * height;
	srcPitch2 = ((width >> 1) + 3) & ~3;
	offset = srcPitch2 * (height >> 1);
	offset3 = offset + offset2;
	new_h = (2 * offset2 + pitch - 1) / pitch;
	break;
    case 0x59595959:
	srcPitch = width;
	dstPitch = (srcPitch + 3) & ~3;
	offset = dstPitch * height;
	new_h = (offset + pitch - 1) / pitch;
	break;
    case 0x32335652:
	srcPitch = (width << 2);
	dstPitch = (srcPitch + 3) & ~3;
	offset = dstPitch * height;
	new_h = (offset + pitch - 1) / pitch;
	break;
    default:
	if (pApm->PutImageStride)
	    srcPitch = pApm->PutImageStride;
	else
	    srcPitch = (width << 1);
	dstPitch = (srcPitch + 3) & ~3;
	offset = dstPitch * height;
	new_h = (offset + pitch - 1) / pitch;
	break;
    }

    area = pPriv->area;

    /* Allocate offscreen memory */
    if (offscreen && (!area || ((area->box.y2 - area->box.y1) < new_h))) {
	Bool nukeMem = FALSE;
	int max_w, max_h;

	xf86QueryLargestOffscreenArea(pScreen, &max_w, &max_h, 0,
			  FAVOR_WIDTH_THEN_AREA, PRIORITY_LOW);
	if (max_w == pScrn->displayWidth && max_h >= new_h) {
	    area = xf86AllocateOffscreenArea(pScreen,
					pScrn->displayWidth, new_h,
					4, A(XvMoveCB), A(XvRemoveCB), pPriv);
	    if (area) {
		if (pPriv->area)
		    xf86FreeOffscreenArea(pPriv->area);
	    }
	    else
		area = pPriv->area;	/* Should not happen */
	}
	if(!area) {
	    if(!(area = xf86AllocateOffscreenArea(pScreen,
					pScrn->displayWidth, new_h, 4,
					A(XvMoveCB), A(XvRemoveCB), pPriv)))
	    {
	       nukeMem = TRUE;
	    }
	} else {
	    if(!xf86ResizeOffscreenArea(area, pScrn->displayWidth, new_h)) {
	       xf86FreeOffscreenArea(area);
	       pPriv->area = area = NULL;
	       nukeMem = TRUE;
	    }
	}
	if(nukeMem) {
	    xf86QueryLargestOffscreenArea(pScreen, &max_w, &max_h, 0,
			    FAVOR_WIDTH_THEN_AREA, PRIORITY_EXTREME);

	    if((max_w < pScrn->displayWidth) || (max_h < new_h))
		return BadAlloc;

	    xf86PurgeUnlockedOffscreenAreas(pScreen);

	    area = xf86AllocateOffscreenArea(pScreen,
					pScrn->displayWidth, new_h, 4,
					A(XvMoveCB), A(XvRemoveCB), pPriv);
	}

	pPriv->area = area;
    }

    /* copy data */
    pPriv->x1 = dstBox.x1 /*drw_x*/;
    pPriv->y1 = dstBox.y1 /*drw_y*/;
    top = y1 >> 16;
    left = (x1 >> 16) & ~1;
    npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;

    switch(id) {
    case 0x59595959:
	pPriv->Bpp = 1;
	break;
    default:
	pPriv->Bpp = 2;
	left <<= 1;
	break;
    case 0x32335652:
	pPriv->Bpp = 4;
	left <<= 2;
	break;
    }
    pPriv->Bps = pPriv->Bpp * pPriv->xden;
    if (offscreen) {
	offset = (area->box.y1 * pitch) + (top * dstPitch);
	dst_start = ((unsigned char *)pApm->FbBase) +
						(pPriv->data = offset + left);
	switch(id) {
	case 0x32315659:
	    top &= ~1;
	    tmp = ((top >> 1) * srcPitch2) + (left >> 2);
	    offset2 += tmp;
	    offset3 += tmp;
	    nlines = ((((y2 + 0xffff) >> 16) + 1) & ~1) - top;
	    xf86XVCopyYUV12ToPacked(buf + (top * srcPitch) + (left >> 1),
				    buf + offset2, buf + offset3, dst_start,
				    srcPitch, srcPitch2, dstPitch,
				    nlines, npixels);
	    break;
	default:
	    if (id == 0x32335652)
		npixels <<= 1;
	    else if (id == 0x59595959)
		npixels >>= 1;
	    buf += (top * srcPitch) + left;
	    nlines = ((y2 + 0xffff) >> 16) - top;
	    if (offscreen)
		xf86XVCopyPacked(buf, dst_start, srcPitch, dstPitch,
				 nlines, npixels);
	    break;
	}
    }
    else
	pPriv->data = buf - (unsigned char *)pApm->FbBase;
    pPriv->on = 1;
    A(WaitForFifo)(pApm, 3);
    WRXW(pPriv->reg + 0x02, dstPitch >> 2);
    WRXW(pPriv->reg + 0x04, scalex);
    WRXW(pPriv->reg + 0x08, scaley);
    pPriv->scalex = scalex;
    pPriv->scaley = scaley;
    if (scalex && scaley)
	scale = 0x0E00;
    else if (scalex)
	scale = 0x0600;
    else if (scaley)
	scale = 0x0A00;
    else
	scale = 0;
    switch(id) {
    case 0x59595959:
	pPriv->val = 0x017B | scale;
	break;
    case 0x32335652:
	pPriv->val = 0x002F | (scale & 0xF7FF);/*Smoothing filter doesn't work*/
	break;
    case 0x36315652:
	pPriv->val = 0x002B | (scale & 0xF7FF);
	break;
    case 0x35315652:
	pPriv->val = 0x0029 | (scale & 0xF7FF);
	break;
    case 0x59555956:
	pPriv->val = 0x013B | scale;
	break;
    case 0x55595659:
	pPriv->val = 0x014B | scale;
	break;
    case 0x32315659:
    case 0x59565955:
    default:
	pPriv->val = 0x016B | scale;
	break;
    }

    (void) A(ReputImage)(pScrn, drw_x, drw_y, clipBoxes, data);

    A(WaitForFifo)(pApm, 1);
    WRXW(pPriv->reg, pPriv->val);

    return Success;
}

#ifndef IOP_ACCESS
static int
ApmQueryImageAttributes(ScrnInfoPtr pScrn, int id,
			  unsigned short *w, unsigned short *h,
			  int *pitches, int *offsets)
{
    int size, tmp;

    if(*w > 1024) *w = 1024;
    if(*h > 1024) *h = 1024;

    *w = (*w + 1) & ~1;
    if(offsets) offsets[0] = 0;

    switch(id) {
    case 0x32315659:
	*h = (*h + 1) & ~1;
	size = (*w + 3) & ~3;
	if(pitches) pitches[0] = size;
	size *= *h;
	if(offsets) offsets[1] = size;
	tmp = ((*w >> 1) + 3) & ~3;
	if(pitches) pitches[1] = pitches[2] = tmp;
	tmp *= (*h >> 1);
	size += tmp;
	if(offsets) offsets[2] = size;
	size += tmp;
	break;
    case 0x59565955:
    case 0x55595659:
    case 0x59555956:
    case 0x32595559:
	size = *w << 1;
	goto common;
    case 0x59595959:
    default:
	size = *w;
common:
	if (pitches)
	    pitches[0] = size;
	size *= *h;
	break;
    }

    return size;
}
#endif
#endif
