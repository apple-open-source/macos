/*
 * Rootless implementation for Mac OS X Aqua environment
 */
/*
 * Copyright (c) 2001 Greg Parker. All Rights Reserved.
 * Copyright (c) 2002 Torrey T. Lyons. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz/rootlessAquaImp.m,v 1.6 2003/01/24 00:11:39 torrey Exp $ */

#include "rootlessAquaImp.h"
#include "fakeBoxRec.h"
#include "quartzCommon.h"
#include "aquaCommon.h"
#include "pseudoramiX.h"
#import <Cocoa/Cocoa.h>
#include <ApplicationServices/ApplicationServices.h>
#import "XView.h"

extern void ErrorF(const char *, ...);


/*
 * AquaDisplayCount
 *  Return the number of displays.
 *  Multihead note: When rootless mode uses PseudoramiX, the
 *  X server only sees one screen; only PseudoramiX itself knows
 *  about all of the screens.
 */
int AquaDisplayCount()
{
    aquaNumScreens = [[NSScreen screens] count];

    if (noPseudoramiXExtension) {
        return aquaNumScreens;
    } else {
        return 1; // only PseudoramiX knows about the rest
    }
}


void AquaScreenInit(int index, int *x, int *y, int *width, int *height,
                    int *rowBytes, int *bps, int *spp, int *bpp)
{
    *spp = 3;
    *bps = CGDisplayBitsPerSample(kCGDirectMainDisplay);
    *bpp = CGDisplayBitsPerPixel(kCGDirectMainDisplay);

    if (noPseudoramiXExtension) {
        NSScreen *screen = [[NSScreen screens] objectAtIndex:index];
        NSRect frame = [screen frame];

        // set x, y so (0,0) is top left of main screen
        *x = NSMinX(frame);
        *y = NSHeight([[NSScreen mainScreen] frame]) - NSHeight(frame) -
            NSMinY(frame);

        *width =  NSWidth(frame);
        *height = NSHeight(frame);
        *rowBytes = (*width) * (*bpp) / 8;

        // Shift the usable part of main screen down to avoid the menu bar.
        if (NSEqualRects(frame, [[NSScreen mainScreen] frame])) {
            *y      += aquaMenuBarHeight;
            *height -= aquaMenuBarHeight;
        }

    } else {
        int i;
        NSRect unionRect = NSMakeRect(0, 0, 0, 0);
        NSArray *screens = [NSScreen screens];

        // Get the union of all screens (minus the menu bar on main screen)
        for (i = 0; i < [screens count]; i++) {
            NSScreen *screen = [screens objectAtIndex:i];
            NSRect frame = [screen frame];
            frame.origin.y = [[NSScreen mainScreen] frame].size.height -
                             frame.size.height - frame.origin.y;
            if (NSEqualRects([screen frame], [[NSScreen mainScreen] frame])) {
                frame.origin.y    += aquaMenuBarHeight;
                frame.size.height -= aquaMenuBarHeight;
            }
            unionRect = NSUnionRect(unionRect, frame);
        }

        // Use unionRect as the screen size for the X server.
        *x = unionRect.origin.x;
        *y = unionRect.origin.y;
        *width = unionRect.size.width;
        *height = unionRect.size.height;
        *rowBytes = (*width) * (*bpp) / 8;

        // Tell PseudoramiX about the real screens.
        // InitOutput() will move the big screen to (0,0),
        // so compensate for that here.
        for (i = 0; i < [screens count]; i++) {
            NSScreen *screen = [screens objectAtIndex:i];
            NSRect frame = [screen frame];
            int j;

            // Skip this screen if it's a mirrored copy of an earlier screen.
            for (j = 0; j < i; j++) {
                if (NSEqualRects(frame, [[screens objectAtIndex:j] frame])) {
                    ErrorF("PseudoramiX screen %d is a mirror of screen %d.\n",
                           i, j);
                    break;
                }
            }
            if (j < i) continue; // this screen is a mirrored copy

            frame.origin.y = [[NSScreen mainScreen] frame].size.height -
                             frame.size.height - frame.origin.y;

            if (NSEqualRects([screen frame], [[NSScreen mainScreen] frame])) {
                frame.origin.y    += aquaMenuBarHeight;
                frame.size.height -= aquaMenuBarHeight;
            }

            ErrorF("PseudoramiX screen %d added: %dx%d @ (%d,%d).\n", i,
                   (int)frame.size.width, (int)frame.size.height,
                   (int)frame.origin.x, (int)frame.origin.y);

            frame.origin.x -= unionRect.origin.x;
            frame.origin.y -= unionRect.origin.y;

            ErrorF("PseudoramiX screen %d placed at X11 coordinate (%d,%d).\n",
                   i, (int)frame.origin.x, (int)frame.origin.y);

            PseudoramiXAddScreen(frame.origin.x, frame.origin.y,
                                 frame.size.width, frame.size.height);
        }
    }
}


/*
 * AquaNewWindow
 *  Create a new on-screen window.
 *  Rootless windows must not autodisplay! Autodisplay can cause a deadlock.
 *    Event thread - autodisplay: locks view hierarchy, then window
 *    X Server thread - window resize: locks window, then view hierarchy
 *  Deadlock occurs if each thread gets one lock and waits for the other.
*/
void *AquaNewWindow(void *upperw, int x, int y, int w, int h, int isRoot)
{
    AquaWindowRec *winRec = (AquaWindowRec *)malloc(sizeof(AquaWindowRec));
    NSRect frame = NSMakeRect(x, NSHeight([[NSScreen mainScreen] frame]) -
                              y - h, w, h);
    NSWindow *theWindow;
    XView *theView;

    // Create an NSWindow for the new X11 window
    theWindow = [[NSWindow alloc] initWithContentRect:frame
                                  styleMask:NSBorderlessWindowMask
                                  backing:NSBackingStoreBuffered
                                  defer:YES];
    if (!theWindow) return NULL;

    [theWindow setBackgroundColor:[NSColor clearColor]];  // erase transparent
    [theWindow setAlphaValue:1.0];       // draw opaque
    [theWindow setOpaque:YES];           // changed when window is shaped

    [theWindow useOptimizedDrawing:YES]; // Has no overlapping sub-views
    [theWindow setAutodisplay:NO];       // See comment above
    [theWindow disableFlushWindow];      // We do all the flushing manually
    [theWindow setHasShadow:!isRoot];    // All windows have shadows except root
    [theWindow setReleasedWhenClosed:YES]; // Default, but we want to be sure

    theView = [[XView alloc] initWithFrame:frame];
    [theWindow setContentView:theView];
    [theWindow setInitialFirstResponder:theView];

    if (upperw) {
        AquaWindowRec *upperRec = AQUA_WINREC(upperw);
        int uppernum = [upperRec->window windowNumber];
        [theWindow orderWindow:NSWindowBelow relativeTo:uppernum];
    } else {
        if (!isRoot) {
            [theWindow orderFront:nil];
            winRec->port = NULL;
        }
    }

    [theWindow setAcceptsMouseMovedEvents:YES];

    winRec->window = theWindow;
    winRec->view = theView;

    if (!isRoot) {
        winRec->rootGWorld = NULL;
        [theView lockFocus];
        // Fill the window with white to make sure alpha channel is set
        NSEraseRect(frame);
        winRec->port = [theView qdPort];
        winRec->context = [[NSGraphicsContext currentContext] graphicsPort];
        // CreateCGContextForPort(winRec->port, &winRec->context);
        [theView unlockFocus];
    } else {
        // Allocate the offscreen graphics world for root window drawing
        GWorldPtr rootGWorld;
        Rect globalRect;

        SetRect(&globalRect, x, y, x+w, y+h);
        if (NewGWorld(&rootGWorld, 0, &globalRect, NULL, NULL, 0))
            return NULL;
        winRec->rootGWorld = rootGWorld;
    }

    return winRec;
}


void AquaDestroyWindow(void *rw)
{
    AquaWindowRec *winRec = AQUA_WINREC(rw);

    [winRec->window orderOut:nil];
    [winRec->window close];
    [winRec->view release];
    if (winRec->rootGWorld) {
        DisposeGWorld(winRec->rootGWorld);
    }
    free(rw);
}


void AquaMoveWindow(void *rw, int x, int y)
{
    AquaWindowRec *winRec = AQUA_WINREC(rw);
    NSPoint topLeft = NSMakePoint(x, NSHeight([[NSScreen mainScreen] frame]) -
                                  y);

    [winRec->window setFrameTopLeftPoint:topLeft];
}


/*
 * AquaStartResizeWindow
 *  Resize the on screen window.
 */
void AquaStartResizeWindow(void *rw, int x, int y, int w, int h)
{
    AquaWindowRec *winRec = AQUA_WINREC(rw);
    NSRect frame = NSMakeRect(x, NSHeight([[NSScreen mainScreen] frame]) -
                              y - h, w, h);

    [winRec->window setFrame:frame display:NO];
}


void AquaFinishResizeWindow(void *rw, int x, int y, int w, int h)
{
    // refresh everything? fixme yes for testing
    fakeBoxRec box = {0, 0, w, h};
    AquaUpdateRects(rw, &box, 1);
}


/*
 * AquaUpdateRects
 *  Flush rectangular regions from a window's backing buffer
 *  (or PixMap for the root window) to the screen.
 */
void AquaUpdateRects(void *rw, fakeBoxRec *fakeRects, int count)
{
    AquaWindowRec *winRec = AQUA_WINREC(rw);
    fakeBoxRec *rects, *end;
    static RgnHandle rgn = NULL;
    static RgnHandle box = NULL;

    if (!rgn) rgn = NewRgn();
    if (!box) box = NewRgn();

    if (winRec->rootGWorld) {
        // FIXME: Draw from the root PixMap to the normally
        // invisible root window.
    } else {
        for (rects = fakeRects, end = fakeRects+count; rects < end; rects++) {
            Rect qdrect;
            qdrect.left = rects->x1;
            qdrect.top = rects->y1;
            qdrect.right = rects->x2;
            qdrect.bottom = rects->y2;

            RectRgn(box, &qdrect);
            UnionRgn(rgn, box, rgn);
        }

        QDFlushPortBuffer(winRec->port, rgn);
    }

    SetEmptyRgn(rgn);
    SetEmptyRgn(box);
}


/*
 * AquaRestackWindow
 *  Change the window order. Put the window behind upperw or on top if
 *  upperw is NULL.
 */
void AquaRestackWindow(void *rw, void *upperw)
{
    AquaWindowRec *winRec = AQUA_WINREC(rw);

    if (upperw) {
        AquaWindowRec *upperRec = AQUA_WINREC(upperw);
        int uppernum = [upperRec->window windowNumber];
        [winRec->window orderWindow:NSWindowBelow relativeTo:uppernum];
    } else {
        [winRec->window makeKeyAndOrderFront:nil];
    }
}


/*
 * AquaReshapeWindow
 *  Set the shape of a window. The rectangles are the areas that are
 *  not part of the new shape.
 */
void AquaReshapeWindow(void *rw, fakeBoxRec *fakeRects, int count)
{
    AquaWindowRec *winRec = AQUA_WINREC(rw);
    NSRect frame = [winRec->view frame];
    int winHeight = NSHeight(frame);

    [winRec->view lockFocus];

    // If window is currently shaped we need to undo the previous shape
    if (![winRec->window isOpaque]) {
        [[NSColor whiteColor] set];
        NSRectFillUsingOperation(frame, NSCompositeDestinationAtop);
    }

    if (count > 0) {
        fakeBoxRec *rects, *end;

        // Make transparent if window is now shaped.
        [winRec->window setOpaque:NO];

        // Clear the areas outside the window shape
        [[NSColor clearColor] set];
        for (rects = fakeRects, end = fakeRects+count; rects < end; rects++) {
            int rectHeight = rects->y2 - rects->y1;
            NSRectFill( NSMakeRect(rects->x1,
                                   winHeight - rects->y1 - rectHeight,
                                   rects->x2 - rects->x1, rectHeight) );
        }
        [[NSGraphicsContext currentContext] flushGraphics];

        // force update of window shadow
        [winRec->window setHasShadow:NO];
        [winRec->window setHasShadow:YES];

    } else {
        fakeBoxRec bounds = {0, 0, NSWidth(frame), winHeight};

        [winRec->window setOpaque:YES];
        AquaUpdateRects(rw, &bounds, 1);
    }

    [winRec->view unlockFocus];
}


/* AquaStartDrawing
 *  When a window's buffer is not being drawn to, the CoreGraphics
 *  window server may compress or move it. Call this routine
 *  to lock down the buffer during direct drawing. It returns
 *  a pointer to the backing buffer and its depth, etc.
 */
void AquaStartDrawing(void *rw, char **bits,
                      int *rowBytes, int *depth, int *bpp)
{
    AquaWindowRec *winRec = AQUA_WINREC(rw);
    PixMapHandle pix;

    if (! winRec->rootGWorld) {
        [winRec->view lockFocus];
        winRec->port = [winRec->view qdPort];
        LockPortBits(winRec->port);
        [winRec->view unlockFocus];
        pix = GetPortPixMap(winRec->port);
    } else {
        pix = GetGWorldPixMap(winRec->rootGWorld);
        LockPixels(pix);
    }

    *bits = GetPixBaseAddr(pix);
    *rowBytes = GetPixRowBytes(pix) & 0x3fff; // fixme is mask needed?
    *depth = (**pix).cmpCount * (**pix).cmpSize; // fixme use GetPixDepth(pix)?
    *bpp = (**pix).pixelSize;
}


/*
 * AquaStopDrawing
 *  When direct access to a window's buffer is no longer needed, this
 *  routine should be called to allow CoreGraphics to compress or
 *  move it.
 */
void AquaStopDrawing(void *rw)
{
    AquaWindowRec *winRec = AQUA_WINREC(rw);

    if (! winRec->rootGWorld) {
        UnlockPortBits(winRec->port);
    } else {
        PixMapHandle pix = GetGWorldPixMap(winRec->rootGWorld);
        UnlockPixels(pix);
    }
}
