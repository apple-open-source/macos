/*
 * NSWindow subclass for Mac OS X rootless X server
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/XWindow.h,v 1.1 2002/03/28 02:21:20 torrey Exp $ */

#import <Cocoa/Cocoa.h>
#import "XView.h"

#include "fakeBoxRec.h"

@interface XWindow : NSWindow
{
    XView *mView;
}

- (id)initWithContentRect:(NSRect)aRect
                   isRoot:(BOOL)isRoot;
- (void)dealloc;

- (char *)bits;
- (void)getBits:(char **)bits
       rowBytes:(int *)rowBytes
          depth:(int *)depth
   bitsPerPixel:(int *)bpp;

- (void)refreshRects:(fakeBoxRec *)rectList
               count:(int)count;

- (void)orderWindow:(NSWindowOrderingMode)place
         relativeTo:(int)otherWindowNumber;

- (void)sendEvent:(NSEvent *)anEvent;
- (BOOL)canBecomeMainWindow;
- (BOOL)canBecomeKeyWindow;

@end
