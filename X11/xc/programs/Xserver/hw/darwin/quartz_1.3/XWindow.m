/*
 * NSWindow subclass for Mac OS X rootless X server
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/XWindow.m,v 1.1 2002/03/28 02:21:20 torrey Exp $ */

#import "XWindow.h"


@implementation XWindow

// XWindow MUST NOT autodisplay! Autodisplay can cause a deadlock.
// event thread - autodisplay: locks view hierarchy, then window
// X Server thread - window resize: locks window, then view hierarchy
// Deadlock occurs if each thread gets one lock and waits for the other.

// XWindow MUST defer! Otherwise an assertion fails in
// NSViewHierarchyLock sometimes.

- (id)initWithContentRect:(NSRect)aRect
                   isRoot:(BOOL)isRoot
{
    int style;
    NSRect viewRect = {{0, 0}, {aRect.size.width, aRect.size.height}};
    style = NSBorderlessWindowMask;

    self = [super initWithContentRect: aRect
                styleMask: style
                backing: NSBackingStoreBuffered
                defer: YES];
    if (! self) return NULL;

    [self setBackgroundColor:[NSColor clearColor]];  // erase transparent
    [self setAlphaValue:1.0];       // draw opaque
    [self setOpaque:YES];           // changed when window is shaped

    [self useOptimizedDrawing:YES]; // Has no overlapping sub-views
    [self setAutodisplay:NO];       // MUST NOT autodisplay! See comment above
    [self disableFlushWindow];      // We do all the flushing manually
    [self setHasShadow: !isRoot];   // All windows have shadows except root

    // [self setAcceptsMouseMovedEvents:YES]; // MUST be AFTER orderFront?

    mView = [[XView alloc] initWithFrame: viewRect];
    [self setContentView:mView];
    [self setInitialFirstResponder:mView];

    return self;
}

- (void)dealloc
{
    [mView release];
    [super dealloc];
}

- (char *)bits
{
    return [mView bits];
}

- (void)getBits:(char **)bits
       rowBytes:(int *)rowBytes
          depth:(int *)depth
   bitsPerPixel:(int *)bpp
{
    [mView getBits:bits rowBytes:rowBytes depth:depth bitsPerPixel:bpp];
}


// rects are X-flip and LOCAL coords
- (void)refreshRects:(fakeBoxRec *)rectList count:(int)count;
{
    [mView refreshRects:rectList count:count];
}


// Deferred windows don't handle mouse moved events very well.
- (void)orderWindow:(NSWindowOrderingMode)place
         relativeTo:(int)otherWindowNumber
{
    [super orderWindow:place relativeTo:otherWindowNumber];
    [self setAcceptsMouseMovedEvents:YES];
}

- (void)sendEvent:(NSEvent *)anEvent
{
    [super sendEvent:anEvent];
    [self setAcceptsMouseMovedEvents:YES];
}

// XWindow may be frameless, and frameless windows default to
// NO key and NO main.
// update: we *don't* want main or key status after all
- (BOOL)canBecomeMainWindow
{
    return NO;
}

- (BOOL)canBecomeKeyWindow
{
    return NO;
}

@end
