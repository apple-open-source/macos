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


	$RCSfile: Square3D.m,v $
	Chess
	
	Copyright (c) 2000-2001 Apple Computer. All rights reserved.
*/

#import <AppKit/NSView.h>
#import <AppKit/NSImage.h>
#import <Foundation/NSGeometry.h>	// NSMakeSize

// own interface
#import "Square3D.h"

// game board
#import "Board3D.h"

// portability layer
#import "gnuglue.h"			// floor_value

// piece size
#define PIECE_WIDTH_3D		(float)54.0	// 55.0?
#define PIECE_HEIGHT_3D		(float)95.0


// Square3D implementations

@implementation Square3D

/*
  This class represents one square on a chess board.  It has a color
  and may contain a piece.
*/

- (void) setBackground: (float)b
{
    background = b; 
    return;
}

- (float) background
{
    return background;
}

- (void) setRow: (int)r
{
    row = r;
    return;
}

- (int) row
{
    return row;
}

- (int) colorVal
{
    return color;
}

- (void) setPieceType: (int)t color: (int) c
{
    pieceType = t;
    color = c;
    return;
}

- (int) pieceType
{
    return pieceType;
}

- (void) setLocation: (NSRect)r
{
    location = r;
    return;
}

- (NSRect) location
{
    return location;
}

- (void) setMoving: (BOOL)flag
{
    moving = flag; 
    return;
}

- (BOOL) isMoving
{
    return moving;
}

- (void) drawWithFrame: (NSRect)f inView: (NSView *)v
{
    [self drawInteriorWithFrame:f inView:v];
    return;
}

- (void) drawBackground: (NSRect)f inView: (NSView *)v
{
    return;
}

- (void) highlight: (const NSRect *)f inView: (NSView *)v
{
    return;
}

/*
Pieces in the big bitmap each are 54 wide 95 high and seperated
by a 1 pixel wide line, with no line at the bottom.  They are ordered
king, queen, bishop, rook, knight, pawn -- white then black.
*/

- (void) drawInteriorWithFrame: (NSRect)r inView: (NSView *)v
/*
  Draw the chess piece.
*/
{
    int col;
    float shrinkFactor;
    NSRect f;
    NSPoint p;
    id bitmap;

    /* Composite the piece icon in the center of the rect. */
    switch( pieceType ){
	case PAWN:	col = 5;	break;
	case ROOK:	col = 3;	break;
	case KNIGHT:	col = 4;	break;
	case BISHOP:	col = 2;	break;
	case QUEEN:	col = 1;	break;
	case KING:	col = 0;	break;
	default:	return;
    }

    shrinkFactor = (0.65 + ((8 - row) * 0.03125));
    f.origin.x = (col + (color == BLACK ? 6 : 0) )* 56 + 1;
    f.origin.y = row * 96;
    f.size = NSMakeSize( PIECE_WIDTH_3D, PIECE_HEIGHT_3D );
    p.x = location.origin.x
	    + (location.size.width - (PIECE_WIDTH_3D * shrinkFactor)) / 2
	    + (5 * shrinkFactor);
    p.y = location.origin.y + (8 * shrinkFactor);

    p.x = (float) floor_value( (double)p.x );
    p.y = (float) floor_value( (double)p.y );

    bitmap = [(Board3D *)v piecesBitmap];	// ??
    [v lockFocus];
    [bitmap compositeToPoint: p fromRect: f operation: NSCompositeSourceOver];
    [v unlockFocus];

    return;
}

@end
