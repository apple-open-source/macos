#import <AppKit/NSControl.h>

@class Square;
@class NSImage;

@interface Board : NSControl
{
    NSImage	*backBitmap;
    Square	*square[8][8];
    id 		printImage;
}

- (void) setupPieces;
- (void) layoutBoard: (short *)p color: (short *)c;
- (void) placePiece:  (short)p at: (int)row : (int)col color: (short)c;

- (void) slidePieceFrom: (int)row1 : (int)col1 to: (int)row2 : (int)col2;

- (int) pieceAt: (int)row : (int)col;
//- (int) colorAt: (int)row : (int)col;

- (void) highlightSquareAt: (int)row : (int)col;
- (void) unhighlightSquareAt: (int)row : (int) col;
- (void) flashSquareAt: (int)row : (int)col;

@end
