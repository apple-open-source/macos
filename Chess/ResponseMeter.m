//#import <AppKit/AppKit.h>

// own interface
#import "ResponseMeter.h"

// PS
#import <AppKit/NSGraphics.h>

// portability layer
#import "gnuglue.h"		// response_time, elapsed_time


@implementation ResponseMeter

- (void)displayFilled
{
	NSRect f = [self bounds];

    [self lockFocus];
    PSgsave();

    PSsetgray( NSWhite );
    PSrectfill(0.0, 0.0, f.size.width, f.size.height);

    PSsetlinewidth( (float)2.0 );
    PSsetgray( NSBlack );
    PSrectstroke(0.0, 0.0, f.size.width, f.size.height);

    PSgrestore();
    [[self window] flushWindow];
    [self unlockFocus];

    return;
}

- (void)drawRect: (NSRect)f
{
    int res_time;

    PSgsave();
    PSsetgray( (float)0.5 );
    PSrectfill(0.0, 0.0, f.size.width, f.size.height);

    if( res_time = response_time() ) {
	float ratio = elapsed_time() / (float)res_time;
	PSsetgray( NSWhite );
	PSrectfill( (float)0.0, (float)0.0, 
			(float)( f.size.width * ratio ), f.size.height );
    }

    PSsetlinewidth( (float)2.0 );
    PSsetgray( NSBlack );
    PSrectstroke(0.0, 0.0, f.size.width, f.size.height);
    PSgrestore();

    return;
}

@end
