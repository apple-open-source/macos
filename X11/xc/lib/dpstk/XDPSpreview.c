/* 
 * XDPSpreview.c
 *
 * (c) Copyright 1990-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */
/* $XFree86: xc/lib/dpstk/XDPSpreview.c,v 1.2 2000/06/07 22:03:01 tsi Exp $ */

#include <X11/Xlib.h>
#include <DPS/dpsXclient.h>
#include <DPS/XDPSlib.h>
#include <DPS/psops.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NeXT
#include <unistd.h>
#endif

#include <DPS/dpsXshare.h>
#include <DPS/dpsXpreview.h>
#include "XDPSpwraps.h"
#include "dpsXcommonI.h"
#include <math.h>
#include <X11/Xos.h>

#if defined(hpux) || defined(AIXV3)
#define SELECT_TYPE int *
#else
#define SELECT_TYPE fd_set *
#endif

#define BEGINDOCUMENTLEN 15	/* Length of "%%BeginDocument" */
#define BEGINBINARYLEN 14	/* Length of "%%BeginBinary:" */

static int ParseFileForBBox(FILE *file, XRectangle *bb);
static void FillPixmapWithGray(
    Screen *screen,
    Drawable dest,
    XRectangle *bbox,
    int xOffset, int yOffset,
    double pixelsPerPoint,
    Bool createMask);

static XDPSRewindFunction rewindFunction = XDPSFileRewindFunc;
static DPSPointer rewindClientData = NULL;
static XDPSGetsFunction getsFunction = XDPSFileGetsFunc;
static DPSPointer getsClientData = NULL;

int XDPSSetFileFunctions(
    XDPSRewindFunction rewindFunc,
    DPSPointer rewindData,
    XDPSGetsFunction getsFunc,
    DPSPointer getsData)
{
    if (rewindFunc != NULL) {
	rewindFunction = rewindFunc;
	rewindClientData = rewindData;
    }
    if (getsFunc != NULL) {
	getsFunction = getsFunc;
	getsClientData = getsData;
    }
    return 0;
}

/* ARGSUSED */

void XDPSFileRewindFunc(FILE *f, DPSPointer data)
{
    rewind(f);
}

/* ARGSUSED */

char *XDPSFileGetsFunc(char *buf, int n, FILE *f, DPSPointer data)
{
    return fgets(buf, n, f);
}

void XDPSEmbeddedEPSFRewindFunc(FILE *f, DPSPointer data)
{
    XDPSPosition *p = (XDPSPosition *) data;

    p->nestingLevel = 0;
    p->continuedLine = False;
    p->binaryCount = 0;

    if (fseek(f, p->startPos, SEEK_SET) != 0) {
       (void) fseek(f, 0L, SEEK_END);	/* Go to the end */
    }
}

static Bool imaging = False;

char *XDPSEmbeddedGetsFunc(char *buf, int n, FILE *f, DPSPointer data)
{
    XDPSPosition *p = (XDPSPosition *) data;
    int count;
    unsigned len;

    if (fgets(buf, n, f) == NULL) {
	if (imaging) p->startPos = -1;
	return NULL;
    }

    /* If previous call didn't get a whole line, we're somewhere in the
       middle, so don't check for comments.  Also, if we're in the middle of
       binary data, don't look for comments either. */

    len = strlen(buf);

    if (p->binaryCount != 0) {
	if (len > p->binaryCount) p->binaryCount = 0;
	else p->binaryCount -= len;

    } else if (!p->continuedLine) {
	if (strncmp(buf, "%%BeginDocument", BEGINDOCUMENTLEN) == 0) {
	    p->nestingLevel++;

	} else if (strncmp(buf, "%%BeginBinary:", BEGINBINARYLEN) == 0) {
	    count = sscanf(buf, "%%%%BeginBinary: %lu", &p->binaryCount);
	    if (count != 1) p->binaryCount = 0;	/* Malformed comment */

	} else if (strcmp(buf, "%%EndDocument\n") == 0) {
	    if (p->nestingLevel == 0) {
		if (imaging) p->startPos = ftell(f);
		return NULL;
	    }
	    else p->nestingLevel--;
	}
    }

    if ((int)len == n-1 && buf[n-1] != '\n') p->continuedLine = True;
    else p->continuedLine = False;

    return buf;
}

int XDPSCreatePixmapForEPSF(
    DPSContext context,
    Screen *screen,
    FILE *epsf,
    int depth,
    double pixelsPerPoint,
    Pixmap *pixmap,
    XRectangle *pixelSize,
    XRectangle *bbox)
{
    Pixmap p;
    int width, height;
    XRectangle bb;

    if (screen == NULL || depth <= 0 ||
	pixelsPerPoint <= 0) {
	return dps_status_illegal_value;
    }

    if (context == NULL) {
	context = XDPSGetSharedContext(DisplayOfScreen(screen));
    }

    (*rewindFunction)(epsf, rewindClientData);

    if (ParseFileForBBox(epsf, &bb) == dps_status_failure) {
	return dps_status_failure;
    }

    width = ceil(bb.width * pixelsPerPoint);
    height = ceil(bb.height * pixelsPerPoint);
    if (width <= 0 || height <= 0) return dps_status_failure;

    p = XCreatePixmap(DisplayOfScreen(screen), RootWindowOfScreen(screen),
		      width, height, depth);

    if (pixmap != NULL) *pixmap = p;
    if (pixelSize != NULL) {
       pixelSize->x = pixelSize->y = 0;
       pixelSize->width = width;
       pixelSize->height = height;
    }
    if (bbox != NULL) *bbox = bb;

    if (context != NULL) return dps_status_success;
    else return dps_status_no_extension;
}

static int ParseFileForBBox(FILE *file, XRectangle *bb)
{
#define BBOXLEN 14		/* Length of "%%BoundingBox:" */
#define BUFLEN 256
#define ATENDLEN 8		/* Length of "(atend)" plus one byte for \0 */
    char buf[BUFLEN];
    char buf2[ATENDLEN];
    Bool atend = False;	/* Found a %%BoundingBox: (atend) */
    float x, y, r, t;
    int n;
    int nestingLevel = 0;
    unsigned long binaryCount = 0;
    Bool continuedLine = False;
    unsigned len;

    while (1) {
       if ((*getsFunction)(buf, BUFLEN, file, getsClientData) == NULL) {
	  return dps_status_failure;
       }

       len = strlen(buf);

       /* If in binary data or continued line, ignore everything */

       if (binaryCount != 0) {
	   if (len > binaryCount) binaryCount = 0;
	   else binaryCount -= len;

       } else if (!continuedLine) {
	   if (strncmp(buf, "%%BeginBinary:", BEGINBINARYLEN) == 0) {
		n = sscanf(buf, "%%%%BeginBinary: %lu", &binaryCount);
		if (n != 1) binaryCount = 0;	/* Malformed comment */

	   } else if (strncmp(buf, "%%BeginDocument", BEGINDOCUMENTLEN) == 0) {
		nestingLevel++;

	   } else if (strcmp(buf, "%%EndDocument\n") == 0) {
		nestingLevel--;

	   /* Only check for bounding box comments at nesting level 0 */

	   } else if (nestingLevel == 0) {

		/* If we haven't already hit an (atend), the end of the
		   comments is a good place to stop looking for the bbox */

		if (!atend && (strcmp(buf, "%%EndComments\n") == 0 ||
			       strcmp(buf, "%%EndProlog\n") == 0)) {
		    return dps_status_failure;
		}

		if (strncmp(buf, "%%BoundingBox:", BBOXLEN) == 0) {
		    n = sscanf(buf, "%%%%BoundingBox: %f %f %f %f",
			       &x, &y, &r, &t);

		    if (n != 4) {
			n = sscanf(buf, "%%%%BoundingBox: %7s", buf2);

			if (n == 1 && strcmp(buf2, "(atend)") == 0) {
			    atend = True;
			} else return dps_status_failure;

		    } else {
			bb->x = (int) x;
			bb->y = (int) y;
			bb->width = r - bb->x;
			if ((float)((int) r) != r) bb->width++;
			bb->height = t - bb->y;
			if ((float)((int) t) != t) bb->height++;
			return dps_status_success;
		    }
		}
	   }
       }

       /* See if this line fills the buffer */
       if (len == BUFLEN-1 && buf[BUFLEN-1] != '\n') continuedLine = True;
     }

#undef ATENDLEN
#undef BUFLEN
#undef BBOXLEN
}

#define mmPerPoint (25.4/72.0)

double XDPSPixelsPerPoint(Screen *screen)
{
    return (float) WidthOfScreen(screen) * mmPerPoint /
	    (float) WidthMMOfScreen(screen);
}

static int timeStart = 200, maxDoubles = 3;

void XDPSSetImagingTimeout(int timeout, int max)
{
    timeStart = timeout;
    maxDoubles = max;
}

typedef struct _StatusInfo {
    DPSContext ctxt;
    DPSPointer cookie;
    Bool *doneFlag;
    unsigned long startReqNum, endReqNum;
    XDPSStatusProc oldProc;
    struct _StatusInfo *next, *prev;
} StatusInfo;

static StatusInfo *StatusList;

static void SetUpStatusVariables(
    DPSContext context,
    DPSPointer cookie,
    Bool *doneFlag,
    unsigned long startReq,
    XDPSStatusProc oldProc)
{
    StatusInfo *info = (StatusInfo *) malloc(sizeof(StatusInfo));

    info->ctxt = context;
    info->cookie = cookie;
    info->doneFlag = doneFlag;
    info->startReqNum = startReq;
    info->endReqNum = 0xFFFFFFFF;
    info->oldProc = oldProc;
    if (StatusList != NULL) StatusList->prev = info;
    info->next = StatusList;
    info->prev = NULL;
    StatusList = info;
}

static void SetEndReqNum(
    DPSContext context,
    unsigned long endReq)
{
    StatusInfo *info = StatusList;

    while (info != NULL && info->ctxt != context) info = info->next;
    if (info != NULL) info->endReqNum = endReq;
}

static void HandlePreviewStatus(
    DPSContext context,
    int status)
{
    unsigned long serial;
    Display *dpy;
    StatusInfo *info = StatusList;

    while (info != NULL && info->ctxt != context) info = info->next;
    if (info == NULL) return;

    (void) XDPSXIDFromContext(&dpy, context);
    serial = LastKnownRequestProcessed(dpy);

    /* This event is from before our imaging; send to old status proc. */
    if (serial < info->startReqNum) {
	(*info->oldProc) (context, status);
	return;
    }

    /* This event is from during our imaging; ignore it */
    if (serial < info->endReqNum) return;

    /* This event is juuuuust right. */
    if (status == PSFROZEN) *info->doneFlag = True;
}

static int FinishUp(
    DPSContext context,
    DPSPointer cookie)
{
    static char restorebuf[] =
	    "\n$Adobe$DPS$Lib$Dict /EPSFsave get restore grestore\n";
    StatusInfo *info = StatusList;
    int err;

    /* Check the results of the imaging:  Get the error status and restore the
       context */

    _DPSPCheckForError(context, &err);

    /* Can't do this is a wrap because of restore semantics */
    DPSWritePostScript(context, restorebuf, strlen(restorebuf));

    (void) XDPSPopContextParameters(cookie);
    
    /* See if we have an info record and delete it if so */
    while (info != NULL && info->ctxt != context) info = info->next;
    if (info != NULL) {
	if (info == StatusList) StatusList = info->next;
	else info->prev->next = info->next;
	if (info->next != NULL) info->next->prev = info->prev;
	XDPSRegisterStatusProc(context, info->oldProc);
	free(info);
    }

    if (err) return dps_status_postscript_error;
    else return dps_status_success;
}

int XDPSCheckImagingResults(
    DPSContext context,
    Screen *screen)
{
    StatusInfo *info = StatusList;
    int status;

    if (context == NULL) {
        context = XDPSGetSharedContext(DisplayOfScreen(screen));
	if (context == NULL) return dps_status_no_extension;
    }

    while (info != NULL && info->ctxt != context) info = info->next;
    if (info == NULL) return dps_status_illegal_value;

    status = XDPSGetContextStatus(context);
    if (status != PSFROZEN) return dps_status_imaging_incomplete;

    XDPSUnfreezeContext(context);
    return FinishUp(context, info->cookie);
}

static void msleep(int ms)
{
    struct timeval incr;

    incr.tv_sec = ms / 1000;
    incr.tv_usec = (ms % 1000) * 1000;
    (void) select (0, (SELECT_TYPE) NULL, (SELECT_TYPE) NULL,
		   (SELECT_TYPE) NULL, &incr);
}

int XDPSImageFileIntoDrawable(
    DPSContext context,
    Screen *screen,
    Drawable dest,
    FILE *file,
    int drawableHeight,
    int drawableDepth,
    XRectangle *bbox,
    int xOffset, int yOffset,
    double pixelsPerPoint,
    Bool clear, Bool createMask,
    Bool waitForCompletion,
    Bool *doneFlag)
{
#define BUFSIZE 256
#define EXECLEN 6
    char buf[BUFSIZE];
    static char eobuf[] = "\n$Adobe$DPS$Lib$Dict /execSuccess true put\n\
stop\n\
Magic end of data line )))))))))) 99#2 2#99 <xyz> // 7gsad,32h4ghNmndFgj2\n";
    XDPSStandardColormap maskMap;
    XDPSStandardColormap rgbMap;
    unsigned int flags = 0;
    int status;
    Bool inited;
    DPSPointer cookie;
    int doublings;
    int ms;
    XDPSStatusProc oldProc;
    unsigned long startReqNum = 0, endReqNum;

    if (screen == NULL || dest == None || 
	drawableHeight <= 0 || drawableDepth <= 0 ||
	pixelsPerPoint <= 0) {
	return dps_status_illegal_value;
    }

    if (context == NULL) {
        context = XDPSGetSharedContext(DisplayOfScreen(screen));
	if (context == NULL) {
	    FillPixmapWithGray(screen, dest, bbox, xOffset, yOffset,
			       pixelsPerPoint,
			       createMask);
	    return dps_status_no_extension;
	}
    }	

    (*rewindFunction)(file, rewindClientData);

    if (!waitForCompletion) {
	DPSWaitContext(context);
	/* Any status events before this point go to old handler */
	startReqNum = NextRequest(DisplayOfScreen(screen));
    }

    status = _XDPSTestComponentInitialized(context,
					   dps_init_bit_preview, &inited);
    if (status != dps_status_success) return status;
    if (!inited) {
	(void) _XDPSSetComponentInitialized(context, dps_init_bit_preview);
	_DPSPDefineExecFunction(context);
    }

    if (createMask) {
	if (drawableDepth != 1) return dps_status_illegal_value;
	maskMap.colormap = None;
	maskMap.red_max = 1;
	maskMap.red_mult = -1;
	maskMap.base_pixel = 1;
	rgbMap.colormap = None;
	rgbMap.red_max = rgbMap.green_max = rgbMap.blue_max = 
		rgbMap.red_mult = rgbMap.green_mult = rgbMap.blue_mult =
		rgbMap.base_pixel = 0;
	flags = XDPSContextGrayMap | XDPSContextRGBMap;
    }

    status = XDPSPushContextParameters(context, screen, drawableDepth,
				     dest, drawableHeight,
				     &rgbMap, &maskMap,
				     flags | XDPSContextScreenDepth |
					      XDPSContextDrawable, &cookie);

    if (status != dps_status_success) return status;

    _DPSPSetMatrix(context, xOffset, yOffset, pixelsPerPoint);

    if (clear) _DPSPClearArea(context, (int) bbox->x, (int) bbox->y,
			      (int) bbox->width, (int) bbox->height);

    if (createMask) _DPSPSetMaskTransfer(context);

    /* Prepare to read PostScript code */
    _DPSPSaveBeforeExec(context, !waitForCompletion);
    DPSWritePostScript(context, "\nexec\n", EXECLEN);

    imaging = True;
    while ((*getsFunction)(buf, BUFSIZE, file, getsClientData) != NULL) {
	DPSWritePostScript(context, buf, strlen(buf));
    }
    imaging = False;

    /* This marks the end of the data stream */
    DPSWritePostScript(context, eobuf, strlen(eobuf));

    if (!waitForCompletion) {
	*doneFlag = False;
	oldProc = XDPSRegisterStatusProc(context, HandlePreviewStatus);
	SetUpStatusVariables(context, cookie, doneFlag, startReqNum, oldProc);
	XDPSSetStatusMask(context, 0, 0, PSFROZENMASK);

	ms = timeStart;

	/* Check for done until we run out of time */
	doublings = 0;
	while (1) {
	    if (XDPSGetContextStatus(context) == PSFROZEN) {
		waitForCompletion = True;
		XDPSUnfreezeContext(context);
		break;
	    }
	    if (doublings >= maxDoubles) break;

	    /* Wait a while */
	    msleep(ms);
	    ms *= 2;
	    doublings++;
	}
    }

    /* If previous decided imaging is done, it changed waitForCompletion */

    if (waitForCompletion) return FinishUp(context, cookie);
    else {
	endReqNum = NextRequest(DisplayOfScreen(screen)) - 1;
	SetEndReqNum(context, endReqNum);
	return dps_status_imaging_incomplete;
    }
#undef EXECLEN
#undef BUFSIZE
}

static void FillPixmapWithGray(
    Screen *screen,
    Drawable dest,
    XRectangle *bbox,
    int xOffset, int yOffset,
    double pixelsPerPoint,
    Bool createMask)
{
    int width, height, x, y;
    GC gc;
    XGCValues v;
    static char grayBits[] = {0x01, 0x02};
    Pixmap grayStipple;
    Display *dpy = DisplayOfScreen(screen);

    width = ceil(bbox->width * pixelsPerPoint);
    height = ceil(bbox->height * pixelsPerPoint);
    x = (bbox->x + xOffset) * pixelsPerPoint;
    y = (bbox->y + yOffset) * pixelsPerPoint;

    if (createMask) {
	v.foreground = 1;
	v.function = GXcopy;

	gc = XCreateGC(dpy, dest, GCForeground | GCFunction, &v);
	XFillRectangle(dpy, dest, gc, x, y, width, height);
	XFreeGC(dpy, gc);
	return;
    }

    grayStipple = XCreateBitmapFromData(dpy, dest, grayBits, 2, 2);

    v.foreground = BlackPixelOfScreen(screen);
    v.background = WhitePixelOfScreen(screen);
    v.function = GXcopy;
    v.stipple = grayStipple;
    v.fill_style = FillOpaqueStippled;
    gc = XCreateGC(dpy, dest, GCForeground | GCBackground | GCFunction |
		              GCStipple | GCFillStyle, &v);
    XFillRectangle(dpy, dest, gc, x, y, width, height);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, grayStipple);
}
