/*
 IMPORTANT: This Apple software is supplied to you by Apple Computer,
 Inc. ("Apple") in consideration of your agreement to the following terms,
 and your use, installation, modification or redistribution of this Apple
 software constitutes acceptance of these terms.  If you do not agree with
 these terms, please do not use, install, modify or redistribute this Apple
 software.
 
 In consideration of your agreement to abide by the following terms, and
 subject to these terms, Apple grants you a personal, non-exclusive
 license, under Apple’s copyrights in this original Apple software (the
 "Apple Software"), to use, reproduce, modify and redistribute the Apple
 Software, with or without modifications, in source and/or binary forms;
 provided that if you redistribute the Apple Software in its entirety and
 without modifications, you must retain this notice and the following text
 and disclaimers in all such redistributions of the Apple Software.
 Neither the name, trademarks, service marks or logos of Apple Computer,
 Inc. may be used to endorse or promote products derived from the Apple
 Software without specific prior written permission from Apple. Except as
 expressly stated in this notice, no other rights or licenses, express or
 implied, are granted by Apple herein, including but not limited to any
 patent rights that may be infringed by your derivative works or by other
 works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES
 NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE
 IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION
 ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND
 WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT
 LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY
 OF SUCH DAMAGE.


	$RCSfile: Board3D.m,v $
	Chess
	
	Copyright (c) 2000-2001 Apple Computer. All rights reserved.
*/

#import <AppKit/NSImage.h>
#import <AppKit/NSPrintInfo.h>
#import <AppKit/NSBitmapImageRep.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSGraphics.h>		// NSBeep
#import <Foundation/NSBundle.h>
#import <Foundation/NSGeometry.h>	// NSMakeSize, NSZeroPoint
#import <Foundation/NSUtilities.h>	// MIN, MAX, ABS
#import <Foundation/NSException.h>

#include <math.h>

// own interface
#import "Board3D.h"

// messaging objects
#import "Square3D.h"
#import "Chess.h"			// NSApp

// portability layer
#import "gnuglue.h"			// floor_value, sleep_microsecs, ...

// piece size
#define PIECE_WIDTH_3D		(float)55.0
#define PIECE_HEIGHT_3D		(float)95.0

// back store size
#define BACK_STORE_WIDTH	(float)80.0
#define BACK_STORE_HEIGHT	(float)110.0

/*  Each set of points describes the vertical lines along the board. */
struct NXLine {
  NSPoint a, b;
};

#define BASE_X 89

static struct NXLine vertical[] = {
  {{BASE_X,122},    {BASE_X+61, 467}},
  {{BASE_X+71,122}, {BASE_X+111,467}},
  {{BASE_X+135,122},{BASE_X+162,467}},
  {{BASE_X+200,122},{BASE_X+212,467}},
  {{BASE_X+265,122},{BASE_X+265,467}},
  {{BASE_X+330,122},{BASE_X+316,467}},
  {{BASE_X+393,122},{BASE_X+367,467}},
  {{BASE_X+459,122},{BASE_X+419,467}},
  {{BASE_X+524,122},{BASE_X+469,467}}
};

/* Each coordinate describes the y value of each line of the board. */
#define BASE_Y  132

static float horizontal[] = {
  BASE_Y, BASE_Y+54, BASE_Y+106, BASE_Y+153,
  BASE_Y+196, BASE_Y+237, BASE_Y+277, BASE_Y+312, BASE_Y+345
};


// private functions

static void squareOrigin( int r, int c, float *x, float *y )
{
    float dx, m, b;

    dx = (vertical[c].a.x -  vertical[c].b.x);
    m  = (vertical[c].a.y -  vertical[c].b.y) / dx;
    b  =  vertical[c].b.y - (vertical[c].b.x  * m);
    *x = (dx) ? ((horizontal[r] - b) / m) : vertical[c].a.x;
    *y = horizontal[r];
    return;
}

static void squareBounds( int r, int c, NSPoint *p1, NSPoint *p2, NSPoint *p3, NSPoint *p4 )
/*
   (p2)----(p4)
    |        |
    |        |
   (p1)----(p3)
*/
{
    float dx, m, b;

    dx = (vertical[c].a.x -  vertical[c].b.x);
    m  = (vertical[c].a.y -  vertical[c].b.y) / dx;
    b  =  vertical[c].b.y - (vertical[c].b.x  * m);
    p1->x = (dx) ? ((horizontal[r] - b) / m) : vertical[c].a.x;
    p1->x = (float) floor_value( (double)p1->x );
    p1->y = (float) floor_value( (double)horizontal[r] );

    p2->x = (dx) ? ((horizontal[r+1] - b) / m) : vertical[c].a.x;
    p2->x = (float) floor_value( (double)p2->x );
    p2->y = (float) floor_value( (double)horizontal[r+1] );

    dx = (vertical[c+1].a.x -  vertical[c+1].b.x);
    m  = (vertical[c+1].a.y -  vertical[c+1].b.y) / dx;
    b  =  vertical[c+1].b.y - (vertical[c+1].b.x  * m);
    p3->x = (dx) ? ((horizontal[r] - b) / m) : vertical[c+1].a.x;
    p3->x = (float) floor_value( (double)p3->x );
    p3->y = (float) floor_value( (double)horizontal[r] );

    p4->x = (dx) ? ((horizontal[r+1] - b) / m) : vertical[c+1].a.x;
    p4->x = (float) floor_value( (double)p4->x );
    p4->y = (float) floor_value( (double)horizontal[r+1] );

    return;
}

static float check_point( struct NXLine *l, NSPoint *p )
{
    float dx  = l->a.x - l->b.x;
    float dy  = l->a.y - l->b.y;
    float dx1 = p->x   - l->a.x;
    float dy1 = p->y   - l->a.y;
    return( dx*dy1 - dy*dx1 );
}

static void convert_point( NSPoint *p, int *r, int *c )
{
    int i;
    for( i = 0; i < 8; i++ ) {
        if( p->y >= horizontal[i] && p->y <= horizontal[i+1] ) {
            *r = i;
            break;
        }
    }
    for( i = 0; i < 8; i++ ) {
        float m1 = check_point( &vertical[i], p );
        float m2 = check_point( &vertical[i+1], p );
        if( m1 > 0 && m2 < 0 ) {
            *c = i;
            break;
        }
    }
    return;
}

// Board3D implementations

@implementation Board3D

- (id)initWithFrame: (NSRect)f
{
    self = [super initWithFrame: f];
    if( self ) {
        NSBundle  *bundle;
        NSString  *path1, *path2;
        NSSize  size;
        int r, c;

        [self allocateGState];
        bundle = [NSBundle mainBundle];
        path1 = [bundle pathForImageResource: @"3d_board"];
        _background = [[NSImage alloc] initWithContentsOfFile: path1];
        path2 = [bundle pathForImageResource: @"3d_pieces"];
        _pieces = [[NSImage alloc] initWithContentsOfFile: path2];
        size = NSMakeSize( BACK_STORE_WIDTH, BACK_STORE_HEIGHT );
        backBitmap = [[NSImage alloc] initWithSize: size];

        for( r = 0; r < 8; r++ ) {
            for( c = 0; c < 8; c++ )
                square[r][c] = [[Square3D alloc] init];
        }
        [self setupPieces];
        return self;
    }
    return nil;
}

- (void)setBackgroundBitmap: (NSImage *) bitmap
{
    if( _background )
        [_background release];
    _background = [bitmap retain];
    return;
}

- (id) backgroundBitmap
{
    return _background;
}

- (void)setPiecesBitmap: (NSImage *) bitmap
{
    if( _pieces )
        [_pieces release];
    _pieces = [bitmap retain];
    return;
}

- (NSImage *)piecesBitmap
{
    return _pieces;
}

- (void)setupPieces
{
    short  *pieces = default_pieces();
    short  *colors = default_colors();
    [self layoutBoard: pieces color: colors]; 
    return;
}

- (void)layoutBoard: (short *)p color: (short *)c
{
    int  sq;

    for( sq = 0; sq < SQUARE_COUNT; sq++ ) {
        int  row = sq / 8;
        int  col = sq % 8;
        [self placePiece: p[sq] at: row: col color: c[sq]];
    }
}

- (void)placePiece:  (short)p at: (int)row : (int)col color: (short)c
{
    int  col2;
    float  m, b, dx, x;
    NSRect  loc;
    Square3D  *theSquare = square[row][col];

    [theSquare setPieceType: p color: c];
    [theSquare setRow: row];
    dx = (vertical[col].a.x -  vertical[col].b.x);
    m  = (vertical[col].a.y -  vertical[col].b.y) / dx;
    b  =  vertical[col].b.y - (vertical[col].b.x  * m);
    x  = (dx) ? ((horizontal[row] - b) / m) : vertical[col].a.x;
    loc.origin.x = x;
    loc.origin.y = horizontal[row];

    col2 = col + 1;
    dx = (vertical[col2].a.x -  vertical[col2].b.x);
    m  = (vertical[col2].a.y -  vertical[col2].b.y) / dx;
    b  =  vertical[col2].b.y - (vertical[col2].b.x  * m);
    x  = (dx) ? ((horizontal[row] - b) / m) : vertical[col2].a.x;
    loc.size.width  = x - loc.origin.x;
    loc.size.height = 99999;

    [theSquare setLocation: loc];
    return;
}

- (void)slidePieceFrom: (int)row1 : (int)col1 to: (int)row2 : (int)col2
{
    Square3D  *theSquare;
    int  pieceType, color;
    NSRect  oldLocation;
    NSPoint  backP, endP, roundedBackP;
    int  controlGState;
    float  incX, incY;
    int  increments, i;

    theSquare = square[row1][col1];
    pieceType = [theSquare pieceType];
    if( ! pieceType )
        return;
    color = [theSquare colorVal];
    oldLocation = [theSquare location];

    squareOrigin( row2, col2, &endP.x, &endP.y );

    /* Remove piece and then save background */
    [theSquare setPieceType: NO_PIECE color: NEUTRAL];
    [self drawRect: [self frame]];

    squareOrigin( row1, col1, &backP.x, &backP.y );
    controlGState = [self gState];

    [backBitmap lockFocus];
    PSgsave();
    PScomposite( roundedBackP.x = floor(backP.x), roundedBackP.y = floor(backP.y), 
				 BACK_STORE_WIDTH, BACK_STORE_HEIGHT, controlGState, 
				 (float)0.0, (float)0.0, NSCompositeCopy );
    PSgrestore();
    [backBitmap unlockFocus];   

    [self lockFocus];
    [theSquare setPieceType: pieceType color: color];
    [theSquare drawInteriorWithFrame: [self frame] inView: self];
    [theSquare setMoving: YES];
    [[self window] flushWindow];

    incX = endP.x - backP.x;
    incY = endP.y - backP.y;
    increments = (int) MAX( ABS(incX), ABS(incY) ) / 7;	// was 5 gcr
    incX = incX / increments;
    incY = incY / increments;

    for( i = 0; i < increments; i++ ) {
        int  dr, dc;
        NSRect  newLocation;

        /* Restore old background */
        [self lockFocus];
        [backBitmap compositeToPoint: roundedBackP operation: NSCompositeCopy];
        [self unlockFocus];   

        backP.x += incX;
        backP.y += incY;
        convert_point( &backP, &dr, &dc );

        /* Save new background */
        [backBitmap lockFocus];
        PSgsave();
		PScomposite( roundedBackP.x = floor(backP.x), roundedBackP.y = floor(backP.y), 
					 BACK_STORE_WIDTH, BACK_STORE_HEIGHT, controlGState, 
					 (float)0.0, (float)0.0, NSCompositeCopy );
        PSgrestore();
        [backBitmap unlockFocus];     

        /* Draw piece at new location. */
        [theSquare setRow: dr];
        newLocation.origin = backP;
        newLocation.size   = NSMakeSize( PIECE_WIDTH_3D, PIECE_HEIGHT_3D );
        [theSquare setLocation: newLocation];
        [theSquare drawInteriorWithFrame: [self frame] inView: self];
        [[self window] flushWindow];
    }

    [theSquare setMoving: NO];
    [self unlockFocus];
    return;
}

- (int) pieceAt: (int)row : (int)col
{
    if( row >= 0 && col >= 0 ) {
        Square3D  *theSquare = square[row][col];
        return [theSquare pieceType];
    }
    return (int)NO_PIECE;
}

- (int) colorAt: (int)row : (int)col
{
    if( row >= 0 && col >= 0 ) {
        Square3D  *theSquare = square[row][col];
        return [theSquare colorVal];
    }
    return (int)NEUTRAL;
}

- (void) highlightSquareAt: (int)row : (int)col
{
    NSPoint  p1, p2, p3, p4;
    int  idx;

    squareBounds( row, col, &p1, &p2, &p3, &p4 );
    [self lockFocus];

    PSgsave();
    PSsetlinewidth( (float)3.0 );
    PSmoveto( p1.x, p1.y );
    PSlineto( p2.x, p2.y );
    PSlineto( p4.x, p4.y );
    PSlineto( p3.x, p3.y );
    PSlineto( p1.x, p1.y );
    PSclosepath();

    /* flash 2 times */
    for( idx = 1; idx <= 3; idx++ ) {
        float  color = NSWhite;
        //	float  color = NSWhite - [self colorAt: row : col];	// ??
        PSsetgray( color );
        PSgsave();
        PSstroke();
        PSgrestore();
        if( [self pieceAt: row : col]
            || (row > 0 && [self pieceAt: row-1 : col]) )
            [self drawRows: row from: col];
        [[self window] flushWindow];
        if( ! [square[row][col] isMoving] )
            sleep_microsecs( (unsigned)15000 );
    }
    PSgrestore();
    [self unhighlightSquareAt: row : col];

    [self unlockFocus];
    return;
}

- (void) unhighlightSquareAt: (int)row : (int)col
{
    NSPoint p1, p2, p3, p4, to;
    NSRect backR;

    squareBounds( row, col, &p1, &p2, &p3, &p4 );
    p1.x = p1.x - 3;
    p1.y = p1.y - 3;
    p2.x = p2.x - 3;
    p2.y = p2.y + 3;
    p3.x = p3.x + 3;
    p3.y = p3.y - 3;
    p4.x = p4.x + 3;
    p4.y = p4.y + 3;

    to.x = MIN( p1.x, p2.x );
    to.y = p1.y;

    backR.origin = to;
    backR.size.width  = MAX( p3.x, p4.x ) - to.x;
    backR.size.height = p2.y - p1.y;

    [self lockFocus];
    PSgsave();
    PSsetlinewidth( (float)3.0 );
    PSsetgray( NSWhite );
    PSnewpath();
    PSmoveto( p1.x, p1.y );
    PSlineto( p2.x, p2.y );
    PSlineto( p4.x, p4.y );
    PSlineto( p3.x, p3.y );
    PSlineto( p1.x, p1.y );
    PSclosepath();
    PSclip();

    [_background compositeToPoint:to fromRect:backR operation:NSCompositeCopy];
    PSgrestore();
    if( [self pieceAt: row : col] || (row > 0 && [self pieceAt: row-1 : col]) )
        [self drawRows: row from: col];
    [[self window] flushWindow];

    [self unlockFocus];
    return;
}

- (void) flashSquareAt: (int)row : (int)col
{
    NSPoint p1, p2, p3, p4;

    squareBounds( row, col, &p1, &p2, &p3, &p4 );
    [self lockFocus];

    PSgsave();
    PSsetlinewidth( (float)3.0 );
    PSsetgray( NSWhite );
    PSnewpath();
    PSmoveto( p1.x, p1.y );
    PSlineto( p2.x, p2.y );
    PSlineto( p4.x, p4.y );
    PSlineto( p3.x, p3.y );
    PSlineto( p1.x, p1.y );
    PSclosepath();
    PSstroke();
    PSgrestore();

    if( [self pieceAt: row : col] || (row > 0 && [self pieceAt: row-1 : col]) )
        [self drawRows: row from: col];

    [self unlockFocus];
    return;
}

- (void) drawRows: (int)row from: (int)col
{
    while( row >= 0 ) {
        Square3D  *theSquare = square[row][col];
        if( [self pieceAt: row : col] && ! [theSquare isMoving] )
            [theSquare drawInteriorWithFrame: [self frame] inView: self];
        row--;
    }
    return;
}

- (void) print: (id)sender
{
    NSPrintInfo	*pi = [NSPrintInfo sharedPrintInfo];
    NSSize	ps  = [pi paperSize];
    NSSize	fs  = [self frame].size;
    float	hm  = (ps.width  - fs.width)  / 2.0;
    float	vm  = (ps.height - fs.height) / 2.0;

    [pi setLeftMargin: hm];
    [pi setRightMargin: hm];
    [pi setTopMargin: vm];
    [pi setBottomMargin: vm];

    [self lockFocus];
    printImage = [[NSBitmapImageRep alloc] initWithFocusedViewRect: [self bounds]];
    [self unlockFocus];

    [super print: sender];
    [printImage release];
    printImage = nil;
    return;
}

- (void) drawRect: (NSRect)f
{
    if( ! printImage ) {
        int  r, c;
        NSPoint  p = NSZeroPoint;

        PSgsave();
        [_background compositeToPoint: p operation: NSCompositeCopy];
        for( r = 7; r >= 0; r-- ) {
            for( c = 7; c >= 0; c-- ) {
                Square3D  *theSquare = square[r][c];
                [theSquare drawWithFrame: f inView: self];
            }
        }
        PSgrestore();
    }
    else {
        [printImage draw];
    }
    return;
}

- (void) mouseDown: (NSEvent *)event
{
    NSException	 *exception = nil;

    if ( [NSApp bothsides] ) {
        NSBeep();
    }
    else if( [NSApp finished] ) {
        [NSApp finishedAlert];
    }
    else if ([self isEnabled]) {
	    NSPoint  pickedP, backP, roundedBackP;
	    Square3D  *theSquare;
	    int  t, clr;
	    NSRect  oldLocation;
	    float  x, y;
	    int  controlGState;
	    int  r2, c2;
	    int  r = -1, c = -1;
	    int  hi_r = -1, hi_c = -1;

            pickedP = [event locationInWindow];
            pickedP = [self convertPoint: pickedP fromView: nil];
            backP   = pickedP;

            convert_point( &pickedP, &r, &c );
            if( r == -1 || c == -1 )
                return;

            theSquare = square[r][c];
            t   = [theSquare pieceType];
            clr = [theSquare colorVal];
            oldLocation = [theSquare location];

            [self lockFocus];
            PSgsave();

            if( t ) {
                [theSquare setPieceType: 0 color: 0];
                [self drawRect: [self frame]];
                [self flashSquareAt: r : c];
                hi_r = r;
                hi_c = c;

                /* Save background */
                squareOrigin( r, c, &x, &y );
                backP.x = x;
                backP.y = y;
				controlGState = [self gState];

                [backBitmap lockFocus];
                PSgsave();
                PScomposite( roundedBackP.x = floor(backP.x), roundedBackP.y = floor(backP.y), 
							 BACK_STORE_WIDTH, BACK_STORE_HEIGHT,
                             controlGState, (float)0.0, (float)0.0, NSCompositeCopy );
                PSgrestore();
                [backBitmap unlockFocus];    

                [theSquare setPieceType: t color: clr];
                [theSquare drawInteriorWithFrame: [self frame] inView: self];
                [theSquare setMoving: YES];
                [[self window] flushWindow];

                pickedP.x = (float) floor_value( (double)(pickedP.x - x) );
                pickedP.y = (float) floor_value( (double)(pickedP.y - y) );
            }

            r2 = 0;
            c2 = 0;
            [[self window] setAcceptsMouseMovedEvents: YES];

            NS_DURING
                while( [event type] != NSLeftMouseUp ) {
                    NSPoint  p, centerP;
                    NSRect  newLocation;
                    unsigned int mask = (NSLeftMouseUpMask | NSLeftMouseDraggedMask);
                    event = [[self window] nextEventMatchingMask: mask];
                    if( ! t )
                        continue;

                    p = [event locationInWindow];
                    p = [self convertPoint: p fromView: nil];

                    /* Restore old background */
			        [self lockFocus];
                    [backBitmap compositeToPoint: roundedBackP operation: NSCompositeCopy];
                    [self unlockFocus];   

                    backP.x = p.x - pickedP.x;
                    backP.y = p.y - pickedP.y;

                    /* Unhighlight square */
                    centerP.y = backP.y + PIECE_HEIGHT_3D / 4.0;
                    centerP.x = backP.x + PIECE_WIDTH_3D  / 2.0;
                    convert_point( &centerP, &r2, &c2 );

                    if( r2 != hi_r || c2 != hi_c ) {
                        if( hi_r != -1 && hi_c != -1 )
                            [self unhighlightSquareAt: hi_r : hi_c];
                        hi_r = r2;
                        hi_c = c2;
                        [self flashSquareAt: r2 : c2];
                    }

                    /* Save new background */
                    [backBitmap lockFocus];
                    PSgsave();
					PScomposite( roundedBackP.x = floor(backP.x), roundedBackP.y = floor(backP.y), 
							 BACK_STORE_WIDTH, BACK_STORE_HEIGHT,
                             controlGState, (float)0.0, (float)0.0, NSCompositeCopy );
                    PSgrestore();
                    [backBitmap unlockFocus];    

                    /* Draw piece at new location. */
                    [theSquare setRow: r2];
                    newLocation.origin.x = p.x - pickedP.x;
                    newLocation.origin.y = p.y - pickedP.y;
                    newLocation.size.width  = PIECE_WIDTH_3D;
                    newLocation.size.height = PIECE_HEIGHT_3D;
                    [theSquare setLocation: newLocation];
                    [theSquare drawInteriorWithFrame: [self frame] inView: self];
                    [self setNeedsDisplay:YES];   // THIS WAS A PROBLEM!
                    [[self window] flushWindow];

                }
                NS_HANDLER
                    exception = localException;
                NS_ENDHANDLER

                if( t ) {
                    [theSquare setMoving: NO];
                    if( r2 != r || c2 != c ) {
                        if( ! [NSApp makeMoveFrom: r : c to: r2 : c2] ) {
                            [theSquare setLocation: oldLocation];
                            [theSquare setPieceType: t color: clr];
                            [theSquare setRow: r];
                        }
                    }
                    else {
                        [theSquare setLocation: oldLocation];
                        [theSquare setPieceType: t color: clr];
                        [theSquare setRow: r];
                    }
                    [self display];
                    [[self window] flushWindow];
               }

                PSgrestore();
                [self unlockFocus];
        }

            if( exception )
                [exception raise];
            return;
}

@end

// Local Variables:
// tab-width: 8
// End:
