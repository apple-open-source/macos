/*
	File:		MBCFloatingBoardWindow.mm
	Contains:	The board window for the floating board
	Version:	1.0
	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.
*/

#import "MBCFloatingBoardWindow.h"

@implementation MBCFloatingBoardWindow

//
// Adapted from RoundTransparentWindow sample code
//
- (id)initWithContentRect:(NSRect)contentRect styleMask:(unsigned int)aStyle 
				  backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag 
{
	//
    // Call NSWindow's version of this function, but pass in the all-important
	// value of NSBorderlessWindowMask for the styleMask so that the window 
	// doesn't have a title bar
	//
    NSWindow* result = 
		[super initWithContentRect: contentRect 
			   styleMask: NSBorderlessWindowMask 
			   backing: bufferingType defer: flag];
	//
    // Set the background color to clear so that (along with the setOpaque 
	// call below) we can see through the parts of the window that we're not		// drawing into.
	//
    [result setBackgroundColor: [NSColor clearColor]];
	//
    // Let's start with no transparency for all drawing into the window
	//
    [result setAlphaValue:0.999];
	//
    // but let's turn off opaqueness so that we can see through the parts of 
	// the window that we're not drawing into
	//
    [result setOpaque:NO];
	//
    // and while we're at it, make sure the window has a shadow, which will
	// automatically be the shape of our custom content.
	//
    [result setHasShadow: YES];
    return result;
}

//
// Custom windows that use the NSBorderlessWindowMask can't become key by 
// default.  Therefore, controls in such windows won't ever be enabled by 
// default.  Thus, we override this method to change that.
//
- (BOOL) canBecomeKeyWindow
{
    return YES;
}

@end
