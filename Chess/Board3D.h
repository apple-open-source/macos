#import <AppKit/NSControl.h>

@class NSImage;
@class Square3D;

@interface Board3D : NSControl
{
    NSImage 	*_background;
    NSImage 	*_pieces;
    NSImage	*backBitmap;
    Square3D	*square[8][8];
    id 		printImage;
}

- (void) setBackgroundBitmap: (NSImage *)bitmap;
- (NSImage *) backgroundBitmap;
- (void)setPiecesBitmap: (NSImage *) bitmap;
- (NSImage *) piecesBitmap;

- (void) setupPieces;
- (void) layoutBoard: (short *)p color: (short *)c;
- (void) placePiece:  (short)p at: (int)row : (int)col color: (short)c;

- (void) slidePieceFrom: (int)row1 : (int)col1 to: (int)row2 : (int)col2;

- (int) pieceAt: (int)row : (int)col;
- (int) colorAt: (int)row : (int)col;

- (void) highlightSquareAt: (int)row : (int) col;
- (void) unhighlightSquareAt: (int)row : (int) col;
- (void) flashSquareAt: (int)row : (int) col;

- (void) drawRows: (int)row from: (int)col;

@end
