/*
 * NSView subclass for Mac OS X rootless X server
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/XView.h,v 1.1 2002/03/28 02:21:20 torrey Exp $ */

#import <Cocoa/Cocoa.h>

#include <drivers/event_status_driver.h>
#include "fakeBoxRec.h"

@interface XView : NSView
{
    char *mBits;
    int mBytesPerRow;
    int mBitsPerSample;
    int mSamplesPerPixel;
    int mBitsPerPixel;
    int mDepth;
    BOOL mShaped;
}

- (id)initWithFrame:(NSRect)aRect;
- (void)dealloc;

- (void)drawRect:(NSRect)aRect;
- (BOOL)isFlipped;
- (BOOL)acceptsFirstResponder;
- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent;
- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)theEvent;

- (void)mouseDown:(NSEvent *)anEvent;

- (void)setFrameSize:(NSSize)newSize;

- (void)allocBitsForSize:(NSSize)newSize;
- (char *)bits;
- (void)getBits:(char **)bits
       rowBytes:(int *)rowBytes
          depth:(int *)depth
   bitsPerPixel:(int *)bpp;

- (void)refreshRects:(fakeBoxRec *)rectList count:(int)count;
- (void)reshapeRects:(fakeBoxRec *)eraseRects count:(int)count;

- (void)copyToScreen:(fakeBoxRec *)rectList count:(int)count
            fromTemp:(BOOL)copyFromTemp;
- (void)copyToShapeBits:(fakeBoxRec *)rectList count:(int)count;
- (void)eraseFromShapeBits:(fakeBoxRec *)rectList count:(int)count;

@end
