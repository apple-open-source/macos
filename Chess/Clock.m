// own interface
#import "Clock.h"

// private functions

static void renderHand( float col, float len, int minute )
{
    PSgsave();
    PSsetgray( col );
    PStranslate( (float)31.0, (float)32.0 );
    PSrotate( (float)( -6.0 * minute ) );
    PSmoveto( (float)0.0, (float)0.0 );
    PSrlineto( (float)0.0, len );
    PSstroke();
    PSgrestore();
}

// Clock implementations

@implementation Clock

- (id)initWithFrame: (NSRect)theFrame
{
    NSRect f;
    f.origin = theFrame.origin;
    f.size.width  = (float)64.0;
    f.size.height = (float)64.0;

    self = [super initWithFrame: f];
    if( self ) {
	background = [[NSImage imageNamed: @"clock"] copy];
	return self;
    }
    return nil;
}

- (void)setSeconds: (int)s
{
    seconds = s; 
}

- (int)seconds
{
    return seconds;
}

- (void)drawRect: (NSRect)rects
{
    NSPoint p = NSZeroPoint;
    PSgsave();
    [background compositeToPoint: p operation: NSCompositeCopy];
    renderHand( (float)0.333, (float)20.0, (int)(seconds % 60)   );
    renderHand( (float)0.0,   (float)20.0, (int)(seconds / 60)   );
    renderHand( (float)0.0,   (float)16.0, (int)(seconds / 3600) );
    PSgrestore();
    return;
}

@end
