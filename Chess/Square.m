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


	$RCSfile: Square.m,v $
	Chess
	
	Copyright (c) 2000-2001 Apple Computer. All rights reserved.
*/

#import "Square.h"

// portability layer
#import "gnuglue.h"			// sleep_microsecs, floor_value


@implementation Square

/*
  This class represents one square on a chess board.  It has a color
  and may contain a piece.
*/

- (void) dealloc
{
    [icon release];
    [super dealloc];
    return;
}

- (void) setBackground: (float)b
{
    background = b; 
    return;
}

- (float) background
{
    return background;
}

- (void) setImage: (NSImage *)i
{
    [icon release];
    icon = [[i name] retain];
    return;
}

- (NSImage *) image
{
    return [NSImage imageNamed: icon];
}

- (NSString *)imageName
{
    if (icon) return icon;
    else return @"";
}
- (void) setPieceType: (int)t
{
    pieceType = t;
    return;
}

- (int) pieceType
{
    return pieceType;
}

- (void) drawWithFrame: (NSRect)f inView: (NSView *)v
{
    PSsetgray( background );
    PSrectfill( f.origin.x, f.origin.y, f.size.width, f.size.height );
    [self drawInteriorWithFrame:f inView:v];
    return;
}

- (void) drawBackground: (NSRect)f inView: (NSView *)v
{
    PSsetgray( background );
    PSrectfill( f.origin.x, f.origin.y, f.size.width, f.size.height );
    return;
}

- (void) highlight: (NSRect)f inView: (NSView *)v
{
    f = NSInsetRect(f, (float)1.0 , (float)1.0);
    PSgsave();
    PSsetlinewidth( (float)2.0 );
    PSsetgray( NSWhite );
    PSrectstroke ( f.origin.x, f.origin.y, f.size.width, f.size.height );
    PSWait();
    sleep_microsecs( (unsigned)15000 );

    PSsetgray( NSBlack );
    PSrectstroke ( f.origin.x, f.origin.y, f.size.width, f.size.height );
    PSWait();
    sleep_microsecs( (unsigned)15000 );

    PSsetgray( NSWhite );
    PSrectstroke ( f.origin.x, f.origin.y, f.size.width, f.size.height );
    PSWait();
    sleep_microsecs( (unsigned)15000 );

    PSsetgray( background );
    PSrectstroke ( f.origin.x, f.origin.y, f.size.width, f.size.height );
    PSWait();

    PSgrestore();
    return;
}

- (void) drawInteriorWithFrame: (NSRect)r inView: (NSView *)v
/*
  Draw the chess piece.
*/
{
    NSPoint p;
    NSSize  s;
    NSImage *bitmap;

    if( !icon )
	return;

    /* Composite the piece icon in the center of the rect. */
    bitmap = [NSImage imageNamed: icon];
    if ( ! bitmap ) {
	NSString  *path = [[NSBundle mainBundle] pathForImageResource: icon];
	bitmap = [[NSImage alloc] initWithContentsOfFile: path];
	[bitmap setName: icon];
    }

    s = [bitmap size];
    p.x = (float) floor_value( (double)(((r.size.width  - s.width)  / 2.0) + r.origin.x) );
    p.y = (float) floor_value( (double)(((r.size.height - s.height) / 2.0) + r.origin.y) );
    [v lockFocus];
    [bitmap compositeToPoint: p operation: NSCompositeSourceOver];
    [v unlockFocus];
    return;
}

@end
