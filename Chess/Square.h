#import <AppKit/AppKit.h>

@interface Square : NSCell
{
    float	background;
    NSString	*icon;
    int		pieceType;
}

- (void) setBackground: (float)b;
- (float) background;
- (void) setImage: (NSImage *)i;
- (NSImage *) image;
- (NSString *)imageName;
- (void) setPieceType: (int)t;
- (int) pieceType;

- (void) drawWithFrame: (NSRect)cellFrame inView: (NSView *)v;
- (void) drawBackground: (NSRect)f inView: (NSView *)v;
- (void) highlight: (NSRect)f inView: (NSView *)v;

- (void) drawInteriorWithFrame: (NSRect)cellFrame inView: (NSView *)v;

@end
