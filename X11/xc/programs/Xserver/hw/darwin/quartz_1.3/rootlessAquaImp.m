/*
 * Rootless implementation for Mac OS X Aqua environment
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/rootlessAquaImp.m,v 1.1 2002/03/28 02:21:20 torrey Exp $ */

#include "rootlessAquaImp.h"
#include "XWindow.h"
#include "fakeBoxRec.h"
#include "quartzCommon.h"
#include "pseudoramiX.h"

extern void ErrorF(const char *, ...);

typedef struct {
    XWindow *window;
} AquaWindowRec;


#define WINREC(rw) ((AquaWindowRec *)rw)


// Multihead note: When rootless mode uses PseudoramiX, the
// X server only sees one screen; only PseudoramiX itself knows
// about all of the screens.

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
                    int *rowBytes, unsigned long *bps, unsigned long *spp,
                    int *bpp)
{
    *bps = 8;
    *spp = 3;
    *bpp = 32;

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

void *AquaNewWindow(void *upperw, int x, int y, int w, int h, int isRoot)
{
    AquaWindowRec *winRec = (AquaWindowRec *)malloc(sizeof(AquaWindowRec));
    NSRect frame = NSMakeRect(x, NSHeight([[NSScreen mainScreen] frame]) -
                              y - h, w, h);

    winRec->window = [[XWindow alloc] initWithContentRect:frame isRoot:isRoot];

    if (upperw) {
        AquaWindowRec *upperRec = WINREC(upperw);
        int uppernum = [upperRec->window windowNumber];
        [winRec->window orderWindow:NSWindowBelow relativeTo:uppernum];
    } else {
        [winRec->window orderFront:nil];
    }

    // fixme hide root for now
    if (isRoot) [winRec->window orderOut:nil];

    return winRec;
}

void AquaDestroyWindow(void *rw)
{
    AquaWindowRec *winRec = WINREC(rw);

    [winRec->window release];
}

void AquaMoveWindow(void *rw, int x, int y)
{
    AquaWindowRec *winRec = WINREC(rw);
    NSPoint topLeft = NSMakePoint(x, NSHeight([[NSScreen mainScreen] frame]) -
                                  y);

    [winRec->window setFrameTopLeftPoint:topLeft];
}

void AquaStartResizeWindow(void *rw, int x, int y, int w, int h)
{
    AquaWindowRec *winRec = WINREC(rw);
    NSRect frame = NSMakeRect(x, NSHeight([[NSScreen mainScreen] frame]) -
                              y - h, w, h);

    [winRec->window setFrame:frame display:NO];
}

void AquaFinishResizeWindow(void *rw, int x, int y, int w, int h)
{
    // refresh everything? fixme yes for testing
    fakeBoxRec box = {0, 0, w, h};
    AquaWindowRec *winRec = WINREC(rw);

    [winRec->window refreshRects:&box count:1];
}

void AquaUpdateRects(void *rw, fakeBoxRec *rects, int count)
{
    AquaWindowRec *winRec = WINREC(rw);

    [winRec->window refreshRects:rects count:count];
}

// fixme is this upperw or lowerw?
void AquaRestackWindow(void *rw, void *upperw)
{
    AquaWindowRec *winRec = WINREC(rw);

    if (upperw) {
        AquaWindowRec *upperRec = WINREC(upperw);
        int uppernum = [upperRec->window windowNumber];
        [winRec->window orderWindow:NSWindowBelow relativeTo:uppernum];
    } else {
        [winRec->window orderFront:nil];
    }
    // [winRec->window setAcceptsMouseMovedEvents:YES];
    // fixme prefer to orderFront whenever possible - pass upperw, not lowerw
}

// rects are the areas not part of the new shape
void AquaReshapeWindow(void *rw, fakeBoxRec *rects, int count)
{
    AquaWindowRec *winRec = WINREC(rw);

    // make transparent if window is now shaped
    // transparent windows never go back to opaque
    if (count > 0) {
        [winRec->window setOpaque:NO];
    }

    [[winRec->window contentView] reshapeRects:rects count:count];

    if (! [winRec->window isOpaque]) {
        // force update of window shadow
        [winRec->window setHasShadow:NO];
        [winRec->window setHasShadow:YES];
    }
}

void AquaGetPixmap(void *rw, char **bits,
                   int *rowBytes, int *depth, int *bpp)
{
    AquaWindowRec *winRec = WINREC(rw);

    [winRec->window getBits:bits rowBytes:rowBytes depth:depth
                    bitsPerPixel:bpp];
}
