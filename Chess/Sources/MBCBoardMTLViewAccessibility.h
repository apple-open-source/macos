/*
  File:        MBCBoardMTLViewAccessibility.h
  Contains:    Accessibility navigation for chess board
  Copyright:    © 2003-2024 by Apple Inc., all rights reserved.
  
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

#import <Foundation/Foundation.h>
#import "MBCBoardMTLView.h"

NS_ASSUME_NONNULL_BEGIN

/*!
 @abstract MBCBoardMTLAccessibilityProxy is used to provide accessibility features for each square on the board.
 */
@interface MBCBoardMTLAccessibilityProxy : NSObject {
    MBCBoardMTLView *_view;
    MBCSquare _square;
}

/*!
 @abstract proxyWithView:square:
 @param view The main view for rendering
 @param square The square that the MBCBoardMTLAccessibilityProxy represents
 @discussion Returns an instance of the MBCBoardMTLAccessibilityProxy
 */
+ (id)proxyWithView:(MBCBoardMTLView *)view square:(MBCSquare)square;

/*!
 @abstract initWithView:square:
 @param view The main view for rendering
 @param square The square that the MBCBoardMTLAccessibilityProxy represents
 @discussion The default initializer
 */
- (id)initWithView:(MBCBoardMTLView *)view square:(MBCSquare)square;

@end

/*!
 @abstract A category to provide accessibility features for the main view
 */
@interface MBCBoardMTLView (Accessibility)

/*!
 @abstract describeSquare:
 @param sqaure The square to be described as a string
 @discussion Will return a string that describes the type and color of piece as well as position on board.
 */
- (NSString *)describeSquare:(MBCSquare)square;

/*!
 @abstract selectSquare:
 @param square The square to select
 @discussion Will be called if accessibilityPerformAction is called with NSAccessibilityPressAction on
 the MBCBoardMTLAccessibilityProxy associated with a square
 */
- (void)selectSquare:(MBCSquare)square;

@end

NS_ASSUME_NONNULL_END
