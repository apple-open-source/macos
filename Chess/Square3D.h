#import <AppKit/NSCell.h>

@class NSView;

@interface Square3D : NSCell
{
    float  background;
    int  row;
    int  color;
    int  pieceType;
    NSRect  location;
    BOOL  moving;
}

- (void) setBackground: (float)b;
- (float) background;
- (void) setRow: (int)r;
- (int) row;
- (int) colorVal;
- (void) setPieceType: (int)t color: (int)c;
- (int) pieceType;
- (void) setLocation: (NSRect)r;
- (NSRect) location;
- (void) setMoving: (BOOL)flag;
- (BOOL) isMoving;

- (void) drawWithFrame: (NSRect)cellFrame inView: (NSView *)v;
- (void) drawBackground: (NSRect)f inView: (NSView *)v;
- (void) highlight: (const NSRect *) f inView: (NSView *)v;

- (void) drawInteriorWithFrame: (NSRect)cellFrame inView: (NSView *)v;

@end
