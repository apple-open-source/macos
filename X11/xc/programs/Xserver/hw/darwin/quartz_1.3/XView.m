/*
 * NSView subclass for Mac OS X rootless X server
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/XView.m,v 1.1 2002/03/28 02:21:20 torrey Exp $ */

#include <ApplicationServices/ApplicationServices.h>

#import "XView.h"
#include "fakeBoxRec.h"

static const void *infobytes(void *info)
{
    return info;
}


static unsigned long *shapeBits = NULL;
static int shapeWidth = 0;
static int shapeHeight = 0;

static void reallocShapeBits(NSSize minSize)
{
    if (shapeWidth < minSize.width  ||  shapeHeight < minSize.height) {
        shapeWidth = minSize.width;
        shapeHeight = minSize.height;
        if (shapeBits) free(shapeBits);
        shapeBits = (unsigned long *) malloc(4 * shapeWidth * shapeHeight);
    }
}


@implementation XView

- (id)initWithFrame:(NSRect)aRect
{
    self = [super initWithFrame:aRect];
    if (!self) return nil;

    mShaped = NO;
    mBitsPerSample = 8;
    mSamplesPerPixel = 3;
    mDepth = mBitsPerSample * mSamplesPerPixel;
    mBitsPerPixel = 32;
    mBits = nil;
    [self allocBitsForSize:aRect.size];

    return self;
}

- (void)dealloc
{
    if (mBits) free(mBits);
    [super dealloc];
}

- (void)drawRect:(NSRect)aRect
{
    // Never draw here.
}

- (BOOL)isFlipped
{
    return NO;
}

- (BOOL)isOpaque
{
    return !mShaped;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent
{
    return YES;
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)theEvent
{
    return YES;
}


- (void)mouseDown:(NSEvent *)anEvent
{
    // Only X is allowed to restack windows.
    [NSApp preventWindowOrdering];
    [[self nextResponder] mouseDown:anEvent];
}

- (void)mouseUp:(NSEvent *)anEvent
{
    // Bring app to front if necessary
    // Don't bring app to front in mouseDown; mousedown-mouseup is too
    // long and X gets what looks like a mouse drag.
    if (! [NSApp isActive]) {
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp arrangeInFront:nil]; // fixme only bring some windows forward?
    }

    [[self nextResponder] mouseDown:anEvent];
}


// Reallocate bits.
// setFrame goes through here too.
- (void)setFrameSize:(NSSize)newSize
{
    [self allocBitsForSize:newSize];
    [super setFrameSize:newSize];
}

- (void)allocBitsForSize:(NSSize)newSize
{
    if (mBits) free(mBits);
    mBytesPerRow = newSize.width * mBitsPerPixel / 8;
    mBits = calloc(mBytesPerRow * newSize.height, 1);
}

- (char *)bits
{
    return mBits;
}

- (void)getBits:(char **)bits
       rowBytes:(int *)rowBytes
          depth:(int *)depth
   bitsPerPixel:(int *)bpp
{
    *bits = mBits;
    *rowBytes = mBytesPerRow;
    *depth = mDepth;
    *bpp = mBitsPerPixel;
}

- (void)refreshRects:(fakeBoxRec *)rectList count:(int)count
{
    if (!mShaped) {
        [self lockFocus];
        [self copyToScreen:rectList count:count fromTemp:NO];
    } else {
        [self copyToShapeBits:rectList count:count];
        [self lockFocus];
        [self copyToScreen:rectList count:count fromTemp:YES];
    }
    [[NSGraphicsContext currentContext] flushGraphics];
    [self unlockFocus];
}

// eraseRects are OUTSIDE the new shape
- (void)reshapeRects:(fakeBoxRec *)eraseRects count:(int)count
{
    fakeBoxRec bounds = {0, 0, [self frame].size.width,
                         [self frame].size.height};

    if (count == 0  &&  !mShaped) {
        [self refreshRects:&bounds count:1];
    } else {
        // View is shaped, or used to be shaped.
        // Shaped windows never become unshaped.
        // (Mac OS X 10.0.4 does not allow transparent windows
        // to become opaque.)

        mShaped = YES;

        // Magic. 10.0.4 and 10.1 both require the alpha channel to be
        // cleared explicitly. 10.0.4 additionally requires the view to
        // be unlocked between this and the drawing code below.
        [self lockFocus];
        [[NSColor clearColor] set];
        NSRectFill([self frame]);
        [self unlockFocus];

        // copy everything from X11 to temp
        // erase eraseRects from temp
        // copy everything from temp to screen
        [self lockFocus];
        [self copyToShapeBits:&bounds count:1];
        [self eraseFromShapeBits:eraseRects count:count];
        [self copyToScreen:&bounds count:1 fromTemp:YES];
        [[NSGraphicsContext currentContext] flushGraphics];
        [self unlockFocus];
    }
}


- (void)eraseFromShapeBits:(fakeBoxRec *)rectList count:(int)count
{
    unsigned long *dst = NULL; // don't assign yet
    int dstWidth = 0; // don't assign yet
    fakeBoxRec *r;
    fakeBoxRec *end;

    assert(mBitsPerPixel == 32);
    reallocShapeBits([self frame].size);
    dst = shapeBits;
    dstWidth = shapeWidth;

    for (r = rectList, end = rectList + count; r < end; r++) {
        unsigned long *dstline = dst + dstWidth*r->y1 + r->x1;
        int h = r->y2 - r->y1;

        while (h--) {
            unsigned long *dstp = dstline;
            int w = r->x2 - r->x1;

            while (w--) {
                *dstp++ = 0x00000000;
            }
            dstline += dstWidth;
        }
    }
}

// assumes X11 bits and temp bits are 32-bit
- (void)copyToShapeBits:(fakeBoxRec *)rectList count:(int)count
{
    unsigned long *src = (unsigned long *) mBits;
    unsigned long *dst = NULL; // don't assign yet
    int srcWidth = mBytesPerRow / 4;
    int dstWidth = 0; // don't assign yet
    fakeBoxRec *r;
    fakeBoxRec *end;

    assert(mBitsPerPixel == 32);
    reallocShapeBits([self frame].size);
    dst = shapeBits;
    dstWidth = shapeWidth;

    for (r = rectList, end = rectList + count; r < end; r++) {
        unsigned long *srcline = src + srcWidth*r->y1 + r->x1;
        unsigned long *dstline = dst + dstWidth*r->y1 + r->x1;
        int h = r->y2 - r->y1;

        while (h--) {
            unsigned long *srcp = srcline;
            unsigned long *dstp = dstline;
            int w = r->x2 - r->x1;

            while (w--) {
                *dstp++ = *srcp++ | 0xff000000;
            }
            srcline += srcWidth;
            dstline += dstWidth;
        }
    }
}


// Copy boxes to the screen from the per-window pixmaps where X draws.
// rectList is in local, X-flipped coordinates.
- (void)copyToScreen:(fakeBoxRec *)rectList count:(int)count
            fromTemp:(BOOL)copyFromTemp
{
    unsigned char *offsetbits;
    fakeBoxRec *r;
    fakeBoxRec *end;
    NSRect bounds;
    char *srcBits;
    int bytesPerRow;
    int bitsPerPixel;
    int bitsPerSample;
    CGImageAlphaInfo alpha;
    CGContextRef destCtx = (CGContextRef)[[NSGraphicsContext currentContext]
                                                graphicsPort];
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    // fixme colorspace leaks?
    const CGDataProviderDirectAccessCallbacks cb = {
        infobytes, NULL, NULL, NULL
    };

    if (copyFromTemp) {
        // shapeBits assumed to be 32-bit
        srcBits = (char *)shapeBits;
        bytesPerRow = 4 * shapeWidth;
        bitsPerPixel = 32;
        bitsPerSample = 8;
        alpha = kCGImageAlphaPremultipliedFirst; // premultiplied ARGB
    } else {
        srcBits = mBits;
        bytesPerRow = mBytesPerRow;
        bitsPerPixel = mBitsPerPixel;
        bitsPerSample = mBitsPerSample;
        alpha = kCGImageAlphaNoneSkipFirst; // xRGB
    }

    bounds = [self frame];
    bounds.origin.x = bounds.origin.y = 0;

    for (r = rectList, end = rectList + count; r < end; r++) {
        NSRect nsr = {{r->x1, r->y1}, {r->x2-r->x1, r->y2-r->y1}};
        CGRect destRect;
        CGDataProviderRef dataProviderRef;
        CGImageRef imageRef;

        // Clip to window
        // (bounds origin is (0,0) so it can be used in either flip)
        // fixme is this necessary with pixmap-per-window?
        nsr = NSIntersectionRect(nsr, bounds);

        // Disallow empty rects
        if (nsr.size.width <= 0  ||  nsr.size.height <= 0) continue;

        offsetbits = srcBits + (int)(nsr.origin.y * bytesPerRow +
                                     nsr.origin.x * bitsPerPixel/8);

        // Flip r to Cocoa-flipped
        nsr.origin.y = bounds.size.height - nsr.origin.y - nsr.size.height;
        destRect = CGRectMake(nsr.origin.x, nsr.origin.y,
                              nsr.size.width, nsr.size.height);

        dataProviderRef = CGDataProviderCreateDirectAccess(offsetbits,
                                destRect.size.height * bytesPerRow, &cb);

        imageRef = CGImageCreate(destRect.size.width, destRect.size.height,
                                 bitsPerSample, bitsPerPixel, bytesPerRow,
                                 colorSpace, alpha, dataProviderRef, NULL,
                                 1, kCGRenderingIntentDefault);

        CGContextDrawImage(destCtx, destRect, imageRef);
        CGImageRelease(imageRef);
        CGDataProviderRelease(dataProviderRef);
    }
}


@end
