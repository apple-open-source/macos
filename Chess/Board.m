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


	$RCSfile: Board.m,v $
	Chess
	
	Copyright (c) 2000-2001 Apple Computer. All rights reserved.
*/

#import <AppKit/NSImage.h>
#import <AppKit/NSPrintInfo.h>
#import <AppKit/NSBitmapImageRep.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSGraphics.h>		// NSBeep
#import <Foundation/NSString.h>
#import <Foundation/NSGeometry.h>	// NSZeroPoint
#import <Foundation/NSUtilities.h>	// MAX, ABS
#import <Foundation/NSException.h>

#include <math.h>

// own interface
#import "Board.h"

// messaging objects
#import "Square.h"
#import "Chess.h"			// NSApp

// portability layer
#import "gnuglue.h"			// floor_value

// square colors
#define BLACK_SQUARE_COLOR  (0.5)
#define WHITE_SQUARE_COLOR  (5.0 / 6.0)

// private functions

static NSString  *whitePiece( p )
short  p;
{
    switch( p ) {
	case PAWN:	return @"white_pawn";
	case ROOK:	return @"white_rook";
	case KNIGHT:	return @"white_knight";
	case BISHOP:	return @"white_bishop";
	case KING:	return @"white_king";
	case QUEEN:	return @"white_queen";
	default:	break;
    }
    return nil;
}

static NSString  *blackPiece( p )
short  p;
{
    switch( p ) {
	case PAWN:	return @"black_pawn";
	case ROOK:	return @"black_rook";
	case KNIGHT:	return @"black_knight";
	case BISHOP:	return @"black_bishop";
	case KING:	return @"black_king";
	case QUEEN:	return @"black_queen";
	default:	break;
    }
    return nil;
}

// Board implementations

@implementation Board

- (id)initWithFrame: (NSRect) f
{
    self = [super initWithFrame: f];
    if( self ) {
	int r, c;
	NSSize size;

	[self allocateGState];
	size.width  = f.size.width  / 8.0;
	size.height = f.size.height / 8.0;
	backBitmap = [[NSImage alloc] initWithSize: size];

	for( r = 0; r < 8; r++ ) {
            for( c = 0; c < 8; c++ ) {
		Square  *aSquare;
		BOOL  even;
		float  bk;
		aSquare = [[Square alloc] init];
		even = ( ! ((r + c) % 2) );
		bk = ( even ) ? BLACK_SQUARE_COLOR : WHITE_SQUARE_COLOR;
		[aSquare setBackground: bk];
		square[r][c] = aSquare;
	    }
	}
	[self setupPieces];
	return self;
    }
    return nil;
}

- (void) setupPieces
{
    short  *pieces = default_pieces();
    short  *colors = default_colors();
    [self layoutBoard: pieces color: colors];
    return;
}

- (void) layoutBoard: (short *)p color: (short *)c
{
    int  sq;
    for( sq = 0; sq < SQUARE_COUNT; sq++ ) {
	int  row = sq / 8;
	int  col = sq % 8;
	[self placePiece: p[sq] at: row: col color: c[sq]];
    }
    return;
}

- (void) placePiece:  (short)p at: (int)row : (int)col color: (short)c
{
    Square    *theSquare = square[row][col];
    NSString  *piece = ( c == WHITE ) ? whitePiece( p ) : blackPiece( p );
    NSImage   *image = ( piece ) ? [NSImage imageNamed: piece] : nil;
    [theSquare setImage: image];
    return;
}

- (void) slidePieceFrom: (int)row1 : (int)col1 to: (int)row2 : (int)col2
{
    Square  *theSquare;
    NSString *icon;
    int  controlGState;
    NSRect  pieceRect;
    NSPoint  backP, endP, winP, roundedBackP;
    float  incX, incY;
    int  i, increments;

    theSquare = square[row1][col1];
    //icon = [[theSquare image] name];
    icon = [theSquare imageName];
    if( [icon isEqual: @""] )
	icon = nil;		// ?
    if( ! icon )
	return;
    controlGState = [self gState];

    pieceRect.size.width  = (float)floor_value( (double)([self frame].size.width  / 8.0) );
    pieceRect.size.height = (float)floor_value( (double)([self frame].size.height / 8.0) );

    backP.x = ( (float)col1 * pieceRect.size.width  );
    backP.y = ( (float)row1 * pieceRect.size.height );
    endP.x  = ( (float)col2 * pieceRect.size.width  );
    endP.y  = ( (float)row2 * pieceRect.size.height );

    [self lockFocus];
    PSgsave();
    
    /* Draw over the piece we are moving. */
    pieceRect.origin.x = col1 * pieceRect.size.width;
    pieceRect.origin.y = row1 * pieceRect.size.height;
    [theSquare drawBackground: pieceRect inView: self];

    /* Save background */ 
    [backBitmap lockFocus];
    PSgsave();
	roundedBackP.x = floor(backP.x); 
	roundedBackP.y = floor(backP.y);
	winP = [[self superview] convertPoint: roundedBackP fromView: self];
    PScomposite( winP.x, winP.y, pieceRect.size.width, pieceRect.size.height,
	controlGState, (float)0.0, (float)0.0, NSCompositeCopy );
    PSgrestore();
    [backBitmap unlockFocus];

    incX = endP.x - backP.x;
    incY = endP.y - backP.y;
    increments = (int) MAX( ABS(incX), ABS(incY) ) / 7;
    incX = incX / increments;
    incY = incY / increments;

    for( i = 0; i < increments; i++ ){

	/* Restore old background */
	[self lockFocus];
	[backBitmap compositeToPoint: backP operation: NSCompositeCopy];
	[self unlockFocus];
	[[self window] flushWindow];

	/* Save new background */
	backP.x += incX;
	backP.y += incY;

	[backBitmap lockFocus];
	PSgsave();
	roundedBackP.x = floor(backP.x); 
	roundedBackP.y = floor(backP.y);
	pieceRect.origin = roundedBackP;
	winP = [[self superview] convertPoint: roundedBackP fromView: self];
	PScomposite( winP.x, winP.y, pieceRect.size.width,
			pieceRect.size.height, controlGState, (float)0.0,
			(float)0.0, NSCompositeCopy );
	PSgrestore();
	[backBitmap unlockFocus];

	/* Draw piece at new location. */
	[theSquare drawInteriorWithFrame: pieceRect inView: self];
	[[self window] flushWindow];
	PSsetgray( NSBlack );
	PSsetlinewidth( (float)2.0 );
	PSclippath();
	PSstroke();
	[[self window] flushWindow];
    }
 
    PSgrestore();
    [self unlockFocus];
    return;
}

- (int) pieceAt: (int)row : (int)col
{
    if( row >= 0 && col >= 0 ) {
	Square  *theSquare = square[row][col];
	return( [theSquare pieceType] );
    }
    return (int)NO_PIECE;
}

- (void) highlightSquareAt: (int)row : (int)col
{
    NSRect  cr;
    Square  *theSquare = square[row][col];

    [self lockFocus];
    cr.size.width  = [self frame].size.width  / 8.0;
    cr.size.height = [self frame].size.height / 8.0;
    cr.origin.x = col * cr.size.width;
    cr.origin.y = row * cr.size.height;

    [theSquare highlight: cr inView: self];

    PSsetgray( NSBlack );
    PSsetlinewidth( (float)2.0 );
    PSclippath();
    PSstroke();
    [[self window] flushWindow];

    [self unlockFocus];
    return;
}
    
- (void) unhighlightSquareAt: (int)row : (int)col
{
    return;
}
    
- (void) flashSquareAt: (int)row : (int)col
{
    return;
}

- (void) print: (id)sender
{
    NSPrintInfo	*pi = [NSPrintInfo sharedPrintInfo];
    NSSize ps = [pi paperSize];
    NSSize fs = [self frame].size;
    float hm = (ps.width  - fs.width)  / 2.0;
    float vm = (ps.height - fs.height) / 2.0;

    [pi setLeftMargin:   hm];
    [pi setRightMargin:  hm];
    [pi setTopMargin:    vm];
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
	int r, c;
	NSRect cr;

	PSgsave();
	cr.size.width  = f.size.width  / 8.0;
	cr.size.height = f.size.height / 8.0;
	for( r = 0; r < 8; r++ ) {
	    cr.origin.y = r * cr.size.height;
	    for( c = 0; c < 8; c++ ) {
		Square  *theSquare = square[r][c];
		cr.origin.x = c * cr.size.width;
		[theSquare drawWithFrame: cr inView: self];
	    }
	}
	PSsetgray( NSBlack );
	PSsetlinewidth( (float)2.0 );
	PSclippath();
	PSstroke();
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
	[[self window] setAcceptsMouseMovedEvents: YES];

      NS_DURING
	while( [event type] != NSLeftMouseUp ) {
	    unsigned int mask = (NSLeftMouseUpMask | NSLeftMouseDraggedMask);
	    event = [[self window] nextEventMatchingMask: mask];
	}
      NS_HANDLER
	exception = localException;
      NS_ENDHANDLER
    }

    else {
	NSPoint  pickedP, backP, roundedBackP, winP;
	NSRect  pieceRect, backR;
	int  r, c;
	Square  *theSquare;
	int  controlGState;
	NSString  *icon;
        NSPoint  p;

	pickedP = [event locationInWindow];
	pickedP = [self convertPoint: pickedP fromView: nil];
	backP = pickedP;
	//pieceRect.origin = NSZeroPoint;
	pieceRect.size.width  = [self frame].size.width  / 8.0;
	pieceRect.size.height = [self frame].size.height / 8.0;
	r = floor_value( (double)(pickedP.y / pieceRect.size.height) ); 
	c = floor_value( (double)(pickedP.x / pieceRect.size.width)  );
	pieceRect.size.width =(float)floor_value((double)pieceRect.size.width);
	pieceRect.size.height=(float)floor_value((double)pieceRect.size.height); 
	backR.origin = NSZeroPoint;
	backR.size   = pieceRect.size;
	pickedP.x = pickedP.x - (((float)c) * pieceRect.size.width);
	pickedP.y = pickedP.y - (((float)r) * pieceRect.size.height);
	theSquare = square[r][c];
	//icon = [[theSquare image] name];
        icon = [theSquare imageName];
	if( [icon isEqual: @""] )
	    icon = nil;		// ?
	if( icon && [self isEnabled] ) {
	    controlGState = [self gState];
	    if( ! controlGState ) {
		[self allocateGState];
		controlGState = [self gState];
	    }
	    [self lockFocus];
	    PSgsave();

	    /* Draw over the piece we are moving. */
	    pieceRect.origin.x = c * pieceRect.size.width;
	    pieceRect.origin.y = r * pieceRect.size.height;
	    [theSquare drawBackground: pieceRect inView: self];

	    /* Save background */ 
	    [backBitmap lockFocus];
	    PSgsave();
		roundedBackP.x = floor(backP.x); 
		roundedBackP.y = floor(backP.y);
		winP = [[self superview] convertPoint: roundedBackP fromView: self];
	    PScomposite( winP.x, winP.y, backR.size.width, backR.size.height,
	      controlGState, (float)0.0, (float)0.0, NSCompositeCopy );
	    PSgrestore();
	    [backBitmap unlockFocus];
	} 
    
	[[self window] setAcceptsMouseMovedEvents: YES];

      NS_DURING
	while ([event type] != NSLeftMouseUp) {
	    unsigned int mask = (NSLeftMouseUpMask | NSLeftMouseDraggedMask);
	    event = [[self window] nextEventMatchingMask: mask];

	    p = [event locationInWindow];
	    p = [self convertPoint: p fromView: nil];

	    if( icon && [self isEnabled] ) {
		/* Restore old background */
		[self lockFocus];
		[backBitmap compositeToPoint:roundedBackP operation:NSCompositeCopy];
		[self unlockFocus];

		/* Save new background */
                backP.x = pieceRect.origin.x = p.x - pickedP.x;
		backP.y = pieceRect.origin.y = p.y - pickedP.y;
                
		[backBitmap lockFocus];
		PSgsave();
		roundedBackP.x = floor(backP.x); 
		roundedBackP.y = floor(backP.y);
		winP = [[self superview] convertPoint: roundedBackP fromView: self];
		PScomposite( winP.x, winP.y, backR.size.width,
					 backR.size.height, controlGState, (float)0.0,
					 (float)0.0, NSCompositeCopy );
		PSgrestore();
		[backBitmap unlockFocus];

		/* Draw piece at new location. */
		[theSquare drawInteriorWithFrame: pieceRect inView: self];

		PSsetgray( NSBlack );
		PSsetlinewidth( (float)2.0 );
		PSclippath();
		PSstroke();
		[[self window] flushWindow];
	    }
	}
      NS_HANDLER
	exception = localException;
      NS_ENDHANDLER

	if( icon && [self isEnabled] ) {
	    NSSize  frame = [self frame].size;
	    int  r2 = floor_value( (double)(p.y / (frame.height / 8.0)) ); 
	    int  c2 = floor_value( (double)(p.x / (frame.width  / 8.0)) );
	    if( r2 != r || c2 != c ) {
		if( ! [NSApp makeMoveFrom: r : c to: r2 : c2] ) {
		    PSWait();
		}
            }
            [self display]; 
	    PSgrestore();
	    [self unlockFocus];
	}
    }

    if( exception )
	[exception raise];
    return;
}

@end

// Local Variables:
// tab-width: 8
// End:
