#import <AppKit/AppKit.h>

@interface Clock : NSView
{
    int		seconds;
    NSImage	*background;
}

- (void)setSeconds: (int)s;
- (int)seconds;

@end
