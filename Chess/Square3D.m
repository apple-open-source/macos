//#import <AppKit/AppKit.h>

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
