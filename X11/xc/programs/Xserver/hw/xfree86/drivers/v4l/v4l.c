/*
 *  video4linux Xv Driver 
 *  based on Michael Schimek's permedia 2 driver.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/v4l/v4l.c,v 1.34 2003/12/31 06:08:53 dawes Exp $ */

#include "videodev.h"
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xf86fbman.h"
#include "xf86xv.h"
#include "Xv.h"
#include "regionstr.h"
#include "dgaproc.h"
#include "xf86str.h"

#include <asm/ioctl.h>		/* _IORW(xxx) #defines are here */

#if 0
# define DEBUG(x) (x)
#else
# define DEBUG(x)
#endif

static void     V4LIdentify(int flags);
static Bool     V4LProbe(DriverPtr drv, int flags);
static const OptionInfoRec * V4LAvailableOptions(int chipid, int busid);

DriverRec V4L = {
        40000,
        "v4l",
        V4LIdentify, /* Identify*/
        V4LProbe, /* Probe */
	V4LAvailableOptions,
        NULL,
        0
};    


#ifdef XFree86LOADER

static MODULESETUPPROTO(v4lSetup);

static XF86ModuleVersionInfo v4lVersRec =
{
        "v4l",
        MODULEVENDORSTRING,
        MODINFOSTRING1,
        MODINFOSTRING2,
        XF86_VERSION_CURRENT,
        0, 0, 1,
        ABI_CLASS_VIDEODRV,
        ABI_VIDEODRV_VERSION,
        MOD_CLASS_NONE,
        {0,0,0,0}
};

XF86ModuleData v4lModuleData = { &v4lVersRec, v4lSetup, NULL };

static pointer
v4lSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
        const char *osname;
	static Bool setupDone = FALSE;

	if (setupDone) {
	    if (errmaj)
		*errmaj = LDR_ONCEONLY;
	    return NULL;             
	}
	
	setupDone = TRUE;

        /* Check that we're being loaded on a Linux system */
        LoaderGetOS(&osname, NULL, NULL, NULL);
        if (!osname || strcmp(osname, "linux") != 0) {
                if (errmaj)
                        *errmaj = LDR_BADOS;
                if (errmin)
                        *errmin = 0;
                return NULL;
        } else {
                /* OK */

	        xf86AddDriver (&V4L, module, 0);   
	  
                return (pointer)1;
        }
}

#else

#include <fcntl.h>
#include <sys/ioctl.h>

#endif

#define VIDEO_OFF     0  /* really off */
#define VIDEO_RGB     1  /* rgb overlay (directly to fb) */
#define VIDEO_YUV     2  /* yuv overlay (to offscreen memory + hw scaling) */
#define VIDEO_RECLIP  3  /* temporarly off, window clipping changes */

typedef struct _PortPrivRec {
    ScrnInfoPtr                 pScrn;
    FBAreaPtr			pFBArea[2];
    int				VideoOn;
    Bool			StreamOn;

    /* file handle */
    int 			nr;
    struct video_capability     cap;

    /* RGB overlay */
    struct video_buffer		rgb_fbuf;
    struct video_window		rgb_win;
    int                         rgbpalette;
    int                         rgbdepth;

    /* attributes */
    struct video_picture	pict;
    struct video_audio          audio;

    XF86VideoEncodingPtr        enc;
    int                         *input;
    int                         *norm;
    int                         nenc,cenc;

    /* yuv to offscreen */
    XF86OffscreenImagePtr       format;   /* list */
    int                         nformat;  /* # if list entries */
    XF86OffscreenImagePtr       myfmt;    /* which one is YUY2 (packed) */
    int                         yuv_format;

    int                         yuv_width,yuv_height;
    XF86SurfacePtr              surface;
    struct video_buffer		yuv_fbuf;
    struct video_window		yuv_win;
} PortPrivRec, *PortPrivPtr;

#define XV_ENCODING	"XV_ENCODING"
#define XV_BRIGHTNESS  	"XV_BRIGHTNESS"
#define XV_CONTRAST 	"XV_CONTRAST"
#define XV_SATURATION  	"XV_SATURATION"
#define XV_HUE		"XV_HUE"

#define XV_FREQ		"XV_FREQ"
#define XV_MUTE		"XV_MUTE"
#define XV_VOLUME      	"XV_VOLUME"

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvEncoding, xvBrightness, xvContrast, xvSaturation, xvHue;
static Atom xvFreq, xvMute, xvVolume;

static XF86VideoFormatRec
InputVideoFormats[] = {
    { 15, TrueColor },
    { 16, TrueColor },
    { 24, TrueColor },
    { 32, TrueColor },
};

#define V4L_ATTR (sizeof(Attributes) / sizeof(XF86AttributeRec))

static const XF86AttributeRec Attributes[] = {
    {XvSettable | XvGettable, -1000,    1000, XV_ENCODING},
    {XvSettable | XvGettable, -1000,    1000, XV_BRIGHTNESS},
    {XvSettable | XvGettable, -1000,    1000, XV_CONTRAST},
    {XvSettable | XvGettable, -1000,    1000, XV_SATURATION},
    {XvSettable | XvGettable, -1000,    1000, XV_HUE},
};
static const XF86AttributeRec VolumeAttr = 
    {XvSettable | XvGettable, -1000,    1000, XV_VOLUME};
static const XF86AttributeRec MuteAttr = 
    {XvSettable | XvGettable,     0,       1, XV_MUTE};
static const XF86AttributeRec FreqAttr = 
    {XvSettable | XvGettable,     0, 16*1000, XV_FREQ};


#define MAX_V4L_DEVICES 4
#define V4L_FD   (v4l_devices[pPPriv->nr].fd)
#define V4L_REF  (v4l_devices[pPPriv->nr].useCount)
#define V4L_NAME (v4l_devices[pPPriv->nr].devName)

static struct V4L_DEVICE {
    int  fd;
    int  useCount;
    char devName[16];
} v4l_devices[MAX_V4L_DEVICES] = {
    { -1 },
    { -1 },
    { -1 },
    { -1 },
};

/* ---------------------------------------------------------------------- */
/* forward decl */

static void V4lQueryBestSize(ScrnInfoPtr pScrn, Bool motion,
		 short vid_w, short vid_h, short drw_w, short drw_h,
		 unsigned int *p_w, unsigned int *p_h, pointer data);

/* ---------------------------------------------------------------------- */

static int V4lOpenDevice(PortPrivPtr pPPriv, ScrnInfoPtr pScrn)
{
    static int first = 1;

    if (-1 == V4L_FD) {
	V4L_FD = open(V4L_NAME, O_RDWR, 0);

	pPPriv->rgb_fbuf.width        = pScrn->virtualX;
	pPPriv->rgb_fbuf.height       = pScrn->virtualY;
	pPPriv->rgb_fbuf.depth        = pScrn->bitsPerPixel;
	pPPriv->rgb_fbuf.bytesperline = pScrn->displayWidth * ((pScrn->bitsPerPixel + 7)/8);
	pPPriv->rgb_fbuf.base         = (pointer)(pScrn->memPhysBase + pScrn->fbOffset);
	if (first) {
	    first = 0;
	    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2,
			 "v4l: memPhysBase=0x%lx\n", pScrn->memPhysBase);
	}

	switch (pScrn->bitsPerPixel) {
	case 16:
	    if (pScrn->weight.green == 5) {
		pPPriv->rgbpalette = VIDEO_PALETTE_RGB555;
		pPPriv->rgbdepth   = 16;
	    } else {
		pPPriv->rgbpalette = VIDEO_PALETTE_RGB565;
		pPPriv->rgbdepth   = 16;
	    }
	    break;
	case 24:
	    pPPriv->rgbpalette = VIDEO_PALETTE_RGB24;
	    pPPriv->rgbdepth   = 24;
	    break;
	case 32:
	    pPPriv->rgbpalette = VIDEO_PALETTE_RGB32;
	    pPPriv->rgbdepth   = 32;
	    break;
	}
    }

    if (-1 == V4L_FD)
	return errno;
    
    V4L_REF++;
    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2,
			 "Xv/open: refcount=%d\n",V4L_REF));

    return 0;
}

static void V4lCloseDevice(PortPrivPtr pPPriv, ScrnInfoPtr pScrn)
{
    V4L_REF--;
    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2,
			 "Xv/close: refcount=%d\n",V4L_REF));
    if (0 == V4L_REF && -1 != V4L_FD) {
	close(V4L_FD);
	V4L_FD = -1;
    }
}

static int
V4lPutVideo(ScrnInfoPtr pScrn,
    short vid_x, short vid_y, short drw_x, short drw_y,
    short vid_w, short vid_h, short drw_w, short drw_h,
    RegionPtr clipBoxes, pointer data)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data;
    struct video_clip *clip;
    BoxPtr pBox;
    RegionRec newReg;
    BoxRec    newBox;
    unsigned int i,dx,dy,dw,dh;
    int width,height;
    int one=1;

    /* Open a file handle to the device */
    if (VIDEO_OFF == pPPriv->VideoOn) {
	if (V4lOpenDevice(pPPriv, pScrn))
	    return Success;
    }

    if (0 != pPPriv->yuv_format) {
	DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "Xv/PV yuv\n"));
	width  = pPPriv->enc[pPPriv->cenc].width;
        height = pPPriv->enc[pPPriv->cenc].height/2; /* no interlace */
	if (drw_w < width)
	    width = drw_w;
	if (drw_h < height)
	    height = drw_h;
	if ((height != pPPriv->yuv_height) || (width != pPPriv->yuv_width)) {
	    /* new size -- free old surface */
	    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "  surface resize\n"));
	    if (pPPriv->surface) {
		pPPriv->VideoOn = VIDEO_OFF;
		pPPriv->myfmt->stop(pPPriv->surface);
		pPPriv->myfmt->free_surface(pPPriv->surface);
		xfree(pPPriv->surface);
		pPPriv->surface    = NULL;
	    }
	    pPPriv->yuv_width  = width;
	    pPPriv->yuv_height = height;
	}
	if (!pPPriv->surface) {
	    /* allocate + setup offscreen surface */
	    if (NULL == (pPPriv->surface = xalloc(sizeof(XF86SurfaceRec))))
		return FALSE;
	    if (Success != pPPriv->myfmt->alloc_surface
		(pScrn,pPPriv->myfmt->image->id,
		 pPPriv->yuv_width,pPPriv->yuv_height,pPPriv->surface)) {
		xfree(pPPriv->surface);
		pPPriv->surface = NULL;
		goto fallback_to_rgb;
	    }
	    pPPriv->yuv_fbuf.width        = pPPriv->surface->width;
	    pPPriv->yuv_fbuf.height       = pPPriv->surface->height;
	    pPPriv->yuv_fbuf.depth        = 16;
	    pPPriv->yuv_fbuf.bytesperline = pPPriv->surface->pitches[0];
	    pPPriv->yuv_fbuf.base         =
		(pointer)(pScrn->memPhysBase + pPPriv->surface->offsets[0]);
	    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "  surface: %p+%d = %p, %dx%d, pitch %d\n",
				 pScrn->memPhysBase,pPPriv->surface->offsets[0],
				 pScrn->memPhysBase+pPPriv->surface->offsets[0],
				 pPPriv->surface->width,pPPriv->surface->height,
				 pPPriv->surface->pitches[0]));
	    pPPriv->yuv_win.width         = pPPriv->surface->width;
	    pPPriv->yuv_win.height        = pPPriv->surface->height;
	}

	/* program driver */
	if (-1 == ioctl(V4L_FD,VIDIOCSFBUF,&(pPPriv->yuv_fbuf)))
	    perror("ioctl VIDIOCSFBUF");
	if (-1 == ioctl(V4L_FD,VIDIOCGPICT,&pPPriv->pict))
	    perror("ioctl VIDIOCGPICT");
	pPPriv->pict.palette = pPPriv->yuv_format;
	pPPriv->pict.depth   = 16;
	if (-1 == ioctl(V4L_FD,VIDIOCSPICT,&pPPriv->pict))
	    perror("ioctl VIDIOCSPICT");
	if (-1 == ioctl(V4L_FD,VIDIOCSWIN,&(pPPriv->yuv_win)))
	    perror("ioctl VIDIOCSWIN");
	if (-1 == ioctl(V4L_FD, VIDIOCCAPTURE, &one))
	    perror("ioctl VIDIOCCAPTURE(1)");

	if (0 == (pPPriv->myfmt->flags & VIDEO_INVERT_CLIPLIST)) {
	    /* invert cliplist */
	    newBox.x1 = drw_x;
	    newBox.y1 = drw_y;
	    newBox.x2 = drw_x + drw_w;
	    newBox.y2 = drw_y + drw_h;

	    if (pPPriv->myfmt->flags & VIDEO_CLIP_TO_VIEWPORT) {
		/* trim to the viewport */
		if(newBox.x1 < pScrn->frameX0)
		    newBox.x1 = pScrn->frameX0;
		if(newBox.x2 > pScrn->frameX1)
		    newBox.x2 = pScrn->frameX1;
		
		if(newBox.y1 < pScrn->frameY0)
		    newBox.y1 = pScrn->frameY0;
		if(newBox.y2 > pScrn->frameY1)
		    newBox.y2 = pScrn->frameY1;
	    }

	    REGION_INIT(pScrn->pScreen, &newReg, &newBox, 1);
	    REGION_SUBTRACT(pScrn->pScreen, &newReg, &newReg, clipBoxes);
	    clipBoxes = &newReg;
	}
	
	/* start overlay */
	DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2,
			     "over: - %d,%d -> %d,%d  (%dx%d) (yuv=%dx%d)\n",
			     drw_x, drw_y,
			     drw_x+drw_w, drw_y+drw_h,
			     drw_w, drw_h,
			     pPPriv->surface->width,pPPriv->surface->height));
	pPPriv->myfmt->display(pPPriv->surface,
			       0, 0, drw_x, drw_y,
			       pPPriv->surface->width,
			       pPPriv->surface->height,
			       drw_w, drw_h,
			       clipBoxes);
	if (0 == (pPPriv->myfmt->flags & VIDEO_INVERT_CLIPLIST)) {
	    REGION_UNINIT(pScrn->pScreen, &newReg); 
	}
	pPPriv->VideoOn = VIDEO_YUV;
	return Success;
    }

 fallback_to_rgb:
    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "Xv/PV rgb\n"));
    /* FIXME: vid-* is ignored for now, not supported by v4l */

    dw = (drw_w < pPPriv->enc[pPPriv->cenc].width) ?
	drw_w : pPPriv->enc[pPPriv->cenc].width;
    dh = (drw_h < pPPriv->enc[pPPriv->cenc].height) ?
	drw_h : pPPriv->enc[pPPriv->cenc].height;
    /* if the window is too big, center the video */
    dx = drw_x + (drw_w - dw)/2;
    dy = drw_y + (drw_h - dh)/2;
    /* bttv prefeares aligned addresses */
    dx &= ~3;
    if (dx < drw_x) dx += 4;
    if (dx+dw > drw_x+drw_w) dw -= 4;

    /* window */
    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "  win: %dx%d+%d+%d\n",
		drw_w,drw_h,drw_x,drw_y));
    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "  use: %dx%d+%d+%d\n",
		dw,dh,dx,dy));
    pPPriv->rgb_win.x      = dx;
    pPPriv->rgb_win.y      = dy;
    pPPriv->rgb_win.width  = dw;
    pPPriv->rgb_win.height = dh;
    pPPriv->rgb_win.flags  = 0;
 
    /* clipping */
    if (pPPriv->rgb_win.clips) {
	xfree(pPPriv->rgb_win.clips);
	pPPriv->rgb_win.clips = NULL;
    }
    pPPriv->rgb_win.clipcount = REGION_NUM_RECTS(clipBoxes);
    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2,"  clip: have #%d\n",
		pPPriv->rgb_win.clipcount));
    if (0 != pPPriv->rgb_win.clipcount) {
	pPPriv->rgb_win.clips = xalloc(pPPriv->rgb_win.clipcount*sizeof(struct video_clip));
	if (NULL != pPPriv->rgb_win.clips) {
	    memset(pPPriv->rgb_win.clips,0,pPPriv->rgb_win.clipcount*sizeof(struct video_clip));
	    pBox = REGION_RECTS(clipBoxes);
	    clip = pPPriv->rgb_win.clips;
	    for (i = 0; i < REGION_NUM_RECTS(clipBoxes); i++, pBox++, clip++) {
		clip->x	 = pBox->x1 - dx;
		clip->y      = pBox->y1 - dy;
		clip->width  = pBox->x2 - pBox->x1;
		clip->height = pBox->y2 - pBox->y1;
	    }
	}
    }

    /* start */
    if (-1 == ioctl(V4L_FD,VIDIOCSFBUF,&(pPPriv->rgb_fbuf)))
	perror("ioctl VIDIOCSFBUF");
    if (-1 == ioctl(V4L_FD,VIDIOCGPICT,&pPPriv->pict))
	perror("ioctl VIDIOCGPICT");
    pPPriv->pict.palette = pPPriv->rgbpalette;
    pPPriv->pict.depth   = pPPriv->rgbdepth;
    if (-1 == ioctl(V4L_FD,VIDIOCSPICT,&pPPriv->pict))
	perror("ioctl VIDIOCSPICT");
    if (-1 == ioctl(V4L_FD,VIDIOCSWIN,&(pPPriv->rgb_win)))
	perror("ioctl VIDIOCSWIN");
    if (-1 == ioctl(V4L_FD, VIDIOCCAPTURE, &one))
	perror("ioctl VIDIOCCAPTURE(1)");
    pPPriv->VideoOn = VIDEO_RGB;

    return Success;
}

static int
V4lPutStill(ScrnInfoPtr pScrn,
    short vid_x, short vid_y, short drw_x, short drw_y,
    short vid_w, short vid_h, short drw_w, short drw_h,
    RegionPtr clipBoxes, pointer data)
{
#if 0
    PortPrivPtr pPPriv = (PortPrivPtr) data;  
#endif

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "Xv/PS\n"));

    /* FIXME */
    return Success;
}

static void
V4lStopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data;  
    int zero=0;

    if (VIDEO_OFF == pPPriv->VideoOn) {
	DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2,
	      "Xv/StopVideo called with video already off\n"));
	return;
    }
    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "Xv/StopVideo shutdown=%d\n",shutdown));

    if (!shutdown) {
	/* just reclipping, we have to stop DMA transfers to the visible screen */
	if (VIDEO_RGB == pPPriv->VideoOn) {
	    if (-1 == ioctl(V4L_FD, VIDIOCCAPTURE, &zero))
		perror("ioctl VIDIOCCAPTURE(0)");
	    pPPriv->VideoOn = VIDEO_RECLIP;
	}
    } else {
	/* video stop - turn off and free everything */
	if (VIDEO_YUV == pPPriv->VideoOn) {
	    pPPriv->myfmt->stop(pPPriv->surface);
	    pPPriv->myfmt->free_surface(pPPriv->surface);
	    xfree(pPPriv->surface);
	    pPPriv->surface = NULL;
	}
	if (-1 == ioctl(V4L_FD, VIDIOCCAPTURE, &zero))
	    perror("ioctl VIDIOCCAPTURE(0)");
	
	V4lCloseDevice(pPPriv,pScrn);
	pPPriv->VideoOn = VIDEO_OFF;
    }
}

/* v4l uses range 0 - 65535; Xv uses -1000 - 1000 */
static int
v4l_to_xv(int val) {
    val = val * 2000 / 65536 - 1000;
    if (val < -1000) val = -1000;
    if (val >  1000) val =  1000;
    return val;
}
static int
xv_to_v4l(int val) {
    val = val * 65536 / 2000 + 32768;
    if (val <    -0) val =     0;
    if (val > 65535) val = 65535;
    return val;
}

static int
V4lSetPortAttribute(ScrnInfoPtr pScrn,
    Atom attribute, INT32 value, pointer data)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data; 
    struct video_channel chan;
    int ret = Success;

    if (V4lOpenDevice(pPPriv, pScrn))
	return Success;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "Xv/SPA %d, %d\n",
			 attribute, value));

    if (-1 == V4L_FD) {
	ret = Success;
    } else if (attribute == xvEncoding) {
	if (value >= 0 && value < pPPriv->nenc) {
	    pPPriv->cenc = value;
	    chan.channel = pPPriv->input[value];
	    chan.norm    = pPPriv->norm[value];
	    if (-1 == ioctl(V4L_FD,VIDIOCSCHAN,&chan))
		perror("ioctl VIDIOCSCHAN");
	} else {
	    ret = BadValue;
	}
    } else if (attribute == xvBrightness ||
               attribute == xvContrast   ||
               attribute == xvSaturation ||
               attribute == xvHue) {
	ioctl(V4L_FD,VIDIOCGPICT,&pPPriv->pict);
	if (attribute == xvBrightness) pPPriv->pict.brightness = xv_to_v4l(value);
	if (attribute == xvContrast)   pPPriv->pict.contrast   = xv_to_v4l(value);
	if (attribute == xvSaturation) pPPriv->pict.colour     = xv_to_v4l(value);
	if (attribute == xvHue)        pPPriv->pict.hue        = xv_to_v4l(value);
	if (-1 == ioctl(V4L_FD,VIDIOCSPICT,&pPPriv->pict))
	    perror("ioctl VIDIOCSPICT");
    } else if (attribute == xvMute ||
	       attribute == xvVolume) {
	ioctl(V4L_FD,VIDIOCGAUDIO,&pPPriv->audio);
	if (attribute == xvMute) {
	    if (value)
		pPPriv->audio.flags |= VIDEO_AUDIO_MUTE;
	    else
		pPPriv->audio.flags &= ~VIDEO_AUDIO_MUTE;
	} else if (attribute == xvVolume) {
	    if (pPPriv->audio.flags & VIDEO_AUDIO_VOLUME)
		pPPriv->audio.volume = xv_to_v4l(value);
	} else {
	    ret = BadValue;
	}
	if (ret != BadValue)
	    if (-1 == ioctl(V4L_FD,VIDIOCSAUDIO,&pPPriv->audio))
		perror("ioctl VIDIOCSAUDIO");
    } else if (attribute == xvFreq) {
	if (-1 == ioctl(V4L_FD,VIDIOCSFREQ,&value))
	    perror("ioctl VIDIOCSFREQ");
    } else if (0 != pPPriv->yuv_format &&
	       pPPriv->myfmt->setAttribute) {
	/* not mine -> pass to yuv scaler driver */
	ret = pPPriv->myfmt->setAttribute(pScrn, attribute, value);
    } else {
	ret = BadValue;
    }

    V4lCloseDevice(pPPriv,pScrn);
    return ret;
}

static int
V4lGetPortAttribute(ScrnInfoPtr pScrn, 
    Atom attribute, INT32 *value, pointer data)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data;
    int ret = Success;

    if (V4lOpenDevice(pPPriv, pScrn))
	return Success;

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "Xv/GPA %d\n",
			 attribute));

    if (-1 == V4L_FD) {
	ret = Success;
    } else if (attribute == xvEncoding) {
	*value = pPPriv->cenc;
    } else if (attribute == xvBrightness ||
               attribute == xvContrast   ||
               attribute == xvSaturation ||
               attribute == xvHue) {
	ioctl(V4L_FD,VIDIOCGPICT,&pPPriv->pict);
	if (attribute == xvBrightness) *value = v4l_to_xv(pPPriv->pict.brightness);
	if (attribute == xvContrast)   *value = v4l_to_xv(pPPriv->pict.contrast);
	if (attribute == xvSaturation) *value = v4l_to_xv(pPPriv->pict.colour);
	if (attribute == xvHue)        *value = v4l_to_xv(pPPriv->pict.hue);
    } else if (attribute == xvMute ||
	       attribute == xvVolume) {
	ioctl(V4L_FD,VIDIOCGAUDIO,&pPPriv->audio);
	if (attribute == xvMute) {
	    *value = (pPPriv->audio.flags & VIDEO_AUDIO_MUTE) ? 1 : 0;
	} else if (attribute == xvVolume) {
	    if (pPPriv->audio.flags & VIDEO_AUDIO_VOLUME)
		*value = v4l_to_xv(pPPriv->audio.volume);
	} else {
	    ret = BadValue;
	}
    } else if (attribute == xvFreq) {
	ioctl(V4L_FD,VIDIOCGFREQ,value);
    } else if (0 != pPPriv->yuv_format &&
	       pPPriv->myfmt->getAttribute) {
	/* not mine -> pass to yuv scaler driver */
	ret = pPPriv->myfmt->getAttribute(pScrn, attribute, value);
    } else {
	ret = BadValue;
    }

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "Xv/GPA %d, %d\n",
	attribute, *value));

    V4lCloseDevice(pPPriv,pScrn);
    return ret;
}

static void
V4lQueryBestSize(ScrnInfoPtr pScrn, Bool motion,
    short vid_w, short vid_h, short drw_w, short drw_h,
    unsigned int *p_w, unsigned int *p_h, pointer data)
{
    PortPrivPtr pPPriv = (PortPrivPtr) data;
    int maxx = pPPriv->enc[pPPriv->cenc].width;
    int maxy = pPPriv->enc[pPPriv->cenc].height;

    if (0 != pPPriv->yuv_format) {
	*p_w = pPPriv->myfmt->max_width;
	*p_h = pPPriv->myfmt->max_height;
    } else {
	*p_w = (drw_w < maxx) ? drw_w : maxx;
	*p_h = (drw_h < maxy) ? drw_h : maxy;
    }

    DEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "Xv/BS %d %dx%d %dx%d\n",
			 pPPriv->cenc,drw_w,drw_h,*p_w,*p_h));
}

static const OptionInfoRec *
V4LAvailableOptions(int chipid, int busid)
{
    return NULL;
}

static void
V4LIdentify(int flags)
{
    xf86Msg(X_INFO, "v4l driver for Video4Linux\n");
}        

static char*
fixname(char *str)
{
    int s,d;
    for (s=0, d=0;; s++) {
	if (str[s] == '-')
	    continue;
	str[d++] = tolower(str[s]);
	if (0 == str[s])
	    break;
    }
    return str;
}

static int
v4l_add_enc(XF86VideoEncodingPtr enc, int i,
	    char *norm, char *input, int width, int height, int n, int d)
{
    enc[i].id     = i;
    enc[i].name   = xalloc(strlen(norm)+strlen(input)+2);
    if (NULL == enc[i].name)
	return -1;
    enc[i].width  = width;
    enc[i].height = height;
    enc[i].rate.numerator   = n;
    enc[i].rate.denominator = d;
    sprintf(enc[i].name,"%s-%s",norm,fixname(input));
    return 0;
}

static void
V4LBuildEncodings(PortPrivPtr p, int fd, int channels)
{
    static struct video_channel     channel;
    int i,entries,have_bttv,bttv_ver;

#define BTTV_VERSION _IOR('v' , BASE_VIDIOCPRIVATE+6, int)
    have_bttv = 0;
    if (-1 != ioctl(fd,BTTV_VERSION,&bttv_ver))
	have_bttv = 1;
	
    entries = (have_bttv ? 7 : 3) * channels;
    p->enc = xalloc(sizeof(XF86VideoEncodingRec) * entries);
    if (NULL == p->enc)
	goto fail;
    memset(p->enc,0,sizeof(XF86VideoEncodingRec) * entries);
    p->norm = xalloc(sizeof(int) * entries);
    if (NULL == p->norm)
	goto fail;
    memset(p->norm,0,sizeof(int) * entries);
    p->input = xalloc(sizeof(int) * entries);
    if (NULL == p->input)
	goto fail;
    memset(p->input,0,sizeof(int) * entries);

    p->nenc = 0;
    for (i = 0; i < channels; i++) {
	channel.channel = i;
	if (-1 == ioctl(fd,VIDIOCGCHAN,&channel)) {
	    perror("ioctl VIDIOCGCHAN");
	    continue;
	}
	
	v4l_add_enc(p->enc, p->nenc,"PAL", channel.name, 768,576, 1,50);
	p->norm[p->nenc]  = VIDEO_MODE_PAL;
	p->input[p->nenc] = i;
	p->nenc++;
	
	v4l_add_enc(p->enc,p->nenc,"NTSC", channel.name, 640,480, 1001,60000);
	p->norm[p->nenc]  = VIDEO_MODE_NTSC;
	p->input[p->nenc] = i;
	p->nenc++;
	
	v4l_add_enc(p->enc,p->nenc,"SECAM",channel.name, 768,576, 1,50);
	p->norm[p->nenc]  = VIDEO_MODE_SECAM;
	p->input[p->nenc] = i;
	p->nenc++;

	if (have_bttv) {
	    /* workaround for a v4l design flaw:  The v4l API knows just pal,
	       ntsc and secam.  But there are a few more norms (pal versions
	       with a different timings used in south america for example).
	       The bttv driver can handle these too. */
	    if (0 != v4l_add_enc(p->enc,p->nenc,"PAL-Nc",channel.name,
				 640, 576, 1,50))
		goto fail;
	    p->norm[p->nenc]  = 3;
	    p->input[p->nenc] = i;
	    p->nenc++;

	    if (0 != v4l_add_enc(p->enc,p->nenc,"PAL-M",channel.name,
				 640, 576, 1,50))
		goto fail;
	    p->norm[p->nenc]  = 4;
	    p->input[p->nenc] = i;
	    p->nenc++;

	    if (0 != v4l_add_enc(p->enc, p->nenc,"PAL-N", channel.name,
				 768,576, 1,50))
		goto fail;
	    p->norm[p->nenc]  = 5;
	    p->input[p->nenc] = i;
	    p->nenc++;
	    
	    if (0 != v4l_add_enc(p->enc,p->nenc,"NTSC-JP", channel.name,
				 640,480, 1001,60000))
		goto fail;
	    p->norm[p->nenc]  = 6;
	    p->input[p->nenc] = i;
	    p->nenc++;
	}
    }
    return;
    
 fail:
    if (p->input)
	xfree(p->input);
    p->input = NULL;
    if (p->norm)
	xfree(p->norm);
    p->norm = NULL;
    if (p->enc)
	xfree(p->enc);
    p->enc = NULL;
    p->nenc = 0;
}

/* add a attribute a list */
static void
v4l_add_attr(XF86AttributeRec **list, int *count,
	     const XF86AttributeRec *attr)
{
    XF86AttributeRec *oldlist = *list;
    int i;

    for (i = 0; i < *count; i++) {
	if (0 == strcmp((*list)[i].name,attr->name)) {
	    DEBUG(xf86Msg(X_INFO, "v4l: skip dup attr %s\n",attr->name));
	    return;
	}
    }
    
    DEBUG(xf86Msg(X_INFO, "v4l: add attr %s\n",attr->name));
    *list = xalloc((*count + 1) * sizeof(XF86AttributeRec));
    if (NULL == *list) {
	*count = 0;
	return;
    }
    if (*count)
	memcpy(*list, oldlist, *count * sizeof(XF86AttributeRec));
    memcpy(*list + *count, attr, sizeof(XF86AttributeRec));
    (*count)++;
}

/* setup yuv overlay + hw scaling: look if we find some common video
   format which both v4l driver and the X-Server can handle */
static void v4l_check_yuv(ScrnInfoPtr pScrn, PortPrivPtr pPPriv,
			  char *dev, int fd)
{
    static const struct {
	unsigned int  v4l_palette;
	unsigned int  v4l_depth;
	unsigned int  xv_id;
	unsigned int  xv_format;
    } yuvlist[] = {
	{ VIDEO_PALETTE_YUV422, 16, 0x32595559, XvPacked },
	{ VIDEO_PALETTE_UYVY,   16, 0x59565955, XvPacked },
	{ 0 /* end of list */ },
    };
    ScreenPtr pScreen = screenInfo.screens[pScrn->scrnIndex];
    int fmt,i;

    pPPriv->format = xf86XVQueryOffscreenImages(pScreen,&pPPriv->nformat);
    for (fmt = 0; yuvlist[fmt].v4l_palette != 0; fmt++) {
	/* check v4l ... */
	ioctl(fd,VIDIOCGPICT,&pPPriv->pict);
	pPPriv->pict.palette = yuvlist[fmt].v4l_palette;
	pPPriv->pict.depth   = yuvlist[fmt].v4l_depth;
	if (-1 == ioctl(fd,VIDIOCSPICT,&pPPriv->pict))
	    continue;
	ioctl(fd,VIDIOCGPICT,&pPPriv->pict);
	if (pPPriv->pict.palette != yuvlist[fmt].v4l_palette)
	    continue;
	/* ... works, check available offscreen image formats now ... */
	for (i = 0; i < pPPriv->nformat; i++) {
	    if (pPPriv->format[i].image->id     == yuvlist[fmt].xv_id &&
		pPPriv->format[i].image->format == yuvlist[fmt].xv_format) {
		/* ... match found, good. */
		pPPriv->yuv_format = yuvlist[fmt].v4l_palette;
		pPPriv->myfmt = pPPriv->format+i;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "v4l[%s]: using hw video scaling [%4.4s].\n",
			   dev,(char*)&(pPPriv->format[i].image->id));
		return;
	    }
	}
    }
}

static int
V4LInit(ScrnInfoPtr pScrn, XF86VideoAdaptorPtr **adaptors)
{
    PortPrivPtr pPPriv;
    DevUnion *Private;
    XF86VideoAdaptorPtr *VAR = NULL;
    char dev[18];
    int  fd,i,j,d;

    DEBUG(xf86Msg(X_INFO, "v4l: init start\n"));

    for (i = 0, d = 0; d < MAX_V4L_DEVICES; d++) {
	sprintf(dev, "/dev/video%d", d);
	fd = open(dev, O_RDWR, 0);
	if (fd == -1) {
	    sprintf(dev, "/dev/v4l/video%d", d);
	    fd = open(dev, O_RDWR, 0);
	    if (fd == -1)
		break;
	}
	DEBUG(xf86Msg(X_INFO,  "v4l: %s open ok\n",dev));

	/* our private data */
	pPPriv = xalloc(sizeof(PortPrivRec));
	if (!pPPriv)
	    return FALSE;
	memset(pPPriv,0,sizeof(PortPrivRec));
	pPPriv->nr = d;

	/* check device */
	if (-1 == ioctl(fd,VIDIOCGCAP,&pPPriv->cap) ||
	    0 == (pPPriv->cap.type & VID_TYPE_OVERLAY)) {
	    DEBUG(xf86Msg(X_INFO,  "v4l: %s: no overlay support\n",dev));
	    xfree(pPPriv);
	    close(fd);
	    continue;
	}
	strncpy(V4L_NAME, dev, 16);
	V4LBuildEncodings(pPPriv,fd,pPPriv->cap.channels);
	if (NULL == pPPriv->enc)
	    return FALSE;
	v4l_check_yuv(pScrn,pPPriv,dev,fd);
	
	/* alloc VideoAdaptorRec */
	VAR = xrealloc(VAR,sizeof(XF86VideoAdaptorPtr)*(i+1));
	VAR[i] = xalloc(sizeof(XF86VideoAdaptorRec));
	if (!VAR[i])
	    return FALSE;
	memset(VAR[i],0,sizeof(XF86VideoAdaptorRec));


	/* build attribute list */
	for (j = 0; j < V4L_ATTR; j++) {
	    /* video attributes */
	    v4l_add_attr(&VAR[i]->pAttributes, &VAR[i]->nAttributes,
			 &Attributes[j]);
	}
	if (0 == ioctl(fd,VIDIOCGAUDIO,&pPPriv->audio)) {
	    /* audio attributes */
	    if (pPPriv->audio.flags & VIDEO_AUDIO_VOLUME)
		v4l_add_attr(&VAR[i]->pAttributes, &VAR[i]->nAttributes,
			     &VolumeAttr);
	    if (pPPriv->audio.flags & VIDEO_AUDIO_MUTABLE)
		v4l_add_attr(&VAR[i]->pAttributes, &VAR[i]->nAttributes,
			     &MuteAttr);
	}
	if (pPPriv->cap.type & VID_TYPE_TUNER) {
	    /* tuner attributes */
	    v4l_add_attr(&VAR[i]->pAttributes, &VAR[i]->nAttributes,
			 &FreqAttr);
	}
	if (0 != pPPriv->yuv_format) {
	    /* pass throuth scaler attributes */
	    for (j = 0; j < pPPriv->myfmt->num_attributes; j++) {
		v4l_add_attr(&VAR[i]->pAttributes, &VAR[i]->nAttributes,
			     pPPriv->myfmt->attributes+j);
	    }
	}
	
	
	/* hook in private data */
	Private = xalloc(sizeof(DevUnion));
	if (!Private)
	    return FALSE;
	memset(Private,0,sizeof(DevUnion));
	Private->ptr = (pointer)pPPriv;
	VAR[i]->pPortPrivates = Private;
	VAR[i]->nPorts = 1;

	/* init VideoAdaptorRec */
	VAR[i]->type  = XvInputMask | XvWindowMask | XvVideoMask;
	VAR[i]->name  = "video4linux";
	VAR[i]->flags = VIDEO_INVERT_CLIPLIST;

	VAR[i]->PutVideo = V4lPutVideo;
	VAR[i]->PutStill = V4lPutStill;
	VAR[i]->StopVideo = V4lStopVideo;
	VAR[i]->SetPortAttribute = V4lSetPortAttribute;
	VAR[i]->GetPortAttribute = V4lGetPortAttribute;
	VAR[i]->QueryBestSize = V4lQueryBestSize;

	VAR[i]->nEncodings = pPPriv->nenc;
	VAR[i]->pEncodings = pPPriv->enc;
	VAR[i]->nFormats =
		sizeof(InputVideoFormats) / sizeof(InputVideoFormats[0]);
	VAR[i]->pFormats = InputVideoFormats;

	if (fd != -1)
	    close(fd);
	i++;
    }

    xvEncoding   = MAKE_ATOM(XV_ENCODING);
    xvHue        = MAKE_ATOM(XV_HUE);
    xvSaturation = MAKE_ATOM(XV_SATURATION);
    xvBrightness = MAKE_ATOM(XV_BRIGHTNESS);
    xvContrast   = MAKE_ATOM(XV_CONTRAST);

    xvFreq       = MAKE_ATOM(XV_FREQ);
    xvMute       = MAKE_ATOM(XV_MUTE);
    xvVolume     = MAKE_ATOM(XV_VOLUME);

    DEBUG(xf86Msg(X_INFO, "v4l: init done, %d device(s) found\n",i));

    *adaptors = VAR;
    return i;
}

static Bool
V4LProbe(DriverPtr drv, int flags)
{
    if (flags & PROBE_DETECT)
	return TRUE;

    xf86XVRegisterGenericAdaptorDriver(V4LInit);
    drv->refCount++;
    return TRUE;
}
