/*
	File:		MBCFloatingBoardWindow.mm
	Contains:	The board window for the floating board
	Copyright:	© 2002-2003 Apple Computer, Inc. All rights reserved.

	IMPORTANT: This Apple software is supplied to you by Apple Computer,
	Inc.  ("Apple") in consideration of your agreement to the following
	terms, and your use, installation, modification or redistribution of
	this Apple software constitutes acceptance of these terms.  If you do
	not agree with these terms, please do not use, install, modify or
	redistribute this Apple software.
	
	In consideration of your agreement to abide by the following terms,
	and subject to these terms, Apple grants you a personal, non-exclusive
	license, under Apple's copyrights in this original Apple software (the
	"Apple Software"), to use, reproduce, modify and redistribute the
	Apple Software, with or without modifications, in source and/or binary
	forms; provided that if you redistribute the Apple Software in its
	entirety and without modifications, you must retain this notice and
	the following text and disclaimers in all such redistributions of the
	Apple Software.  Neither the name, trademarks, service marks or logos
	of Apple Computer, Inc. may be used to endorse or promote products
	derived from the Apple Software without specific prior written
	permission from Apple.  Except as expressly stated in this notice, no
	other rights or licenses, express or implied, are granted by Apple
	herein, including but not limited to any patent rights that may be
	infringed by your derivative works or by other works in which the
	Apple Software may be incorporated.
	
	The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
	MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
	THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS
	USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
	
	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT,
	INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
	REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE,
	HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING
	NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
