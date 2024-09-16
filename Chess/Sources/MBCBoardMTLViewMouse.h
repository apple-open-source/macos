/*
    File:        MBCBoardMTLViewMouse.h
    Contains:    Mouse handling for OpenGL chess board view
    Copyright:    © 2002-2010 by Apple Inc., all rights reserved.

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
    of Apple Inc. may be used to endorse or promote products
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

#import "MBCBoardMTLView.h"

struct MBCPosition;

@interface MBCBoardMTLView (Mouse)

/*!
 @abstract approximateBoundsOfSquare:
 @param square Square code on the board
 @discussion Will convert the given board square from world coordinates to screen space coordinates as a NSRect
 */
- (NSRect)approximateBoundsOfSquare:(MBCSquare)square;

/*!
 @abstract mouseToPosition:
 @param mouse Screen position for the mouse
 @discussion Will convert the given mouse screen position to coordinates in the world
 */
- (MBCPosition)mouseToPosition:(NSPoint)mouse;

/*!
 @abstract mouseDown:
 @param event The event for mouseDown
 @discussion Called when initially click mouse
 */
- (void)mouseDown:(NSEvent *)event;

/*!
 @abstract mouseMoved:
 @param event The event for mouseMoved
 @discussion Called when move the mouse
 */
- (void)mouseMoved:(NSEvent *)event;

/*!
 @abstract mouseUp:
 @param event The event for mouseUp
 @discussion Called when release mouse click
 */
- (void)mouseUp:(NSEvent *)event;

/*!
 @abstract dragAndRedraw:forceRedraw:
 @param force
 @discussion Called to handle mouse drag while clicking.  Depending upon which region is
 clicked will either do piece selection/moving or rotate the board (via camera movement).
 */
- (void)dragAndRedraw:(NSEvent *)event forceRedraw:(BOOL)force;

@end
