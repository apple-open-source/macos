/*
    File:        MBCMetalCamera.h
    Contains:    Manages the Metal camera to provide view and projection matrices.
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
#import <simd/simd.h>

#import "MBCBoardCommon.h"

NS_ASSUME_NONNULL_BEGIN

@class MBCBoardMTLView;

const float kDegrees2Radians = M_PI / 180.0f;

@interface MBCMetalCamera : NSObject

/*!
 @abstract Camera position in world space
 */
@property (nonatomic, readonly) vector_float3 position;

/*!
 @abstract The horizontal angle of the camera about the vertical (Y) axis of the board. Updated as drag the board to change viewing angle.
 */
@property (nonatomic) float azimuth;

/*!
 @abstract The vertical angle of the camera relative to the horizontal plane of the board in Degrees.
 */
@property (nonatomic) float elevation;

/*
 @abstract The viewport origin and size in pixels (x, y, width, height)
*/
@property (nonatomic, readonly) vector_float4 viewport;

/*!
 @abstract The current projection matrix based on size of the MTKView.
*/
@property (nonatomic, readonly) matrix_float4x4 projectionMatrix;

/*!
 @abstract The current view matrix based on position of camera.
*/
@property (nonatomic, readonly) matrix_float4x4 viewMatrix;

/*!
 @abstract Compute and store the view projection matrix when either of them change.
 Used for the conversion of positions from world to screen coordinates
 */
@property (nonatomic, readonly) matrix_float4x4 viewProjectionMatrix;

/*!
 @abstract The current view matrix for reflection map generation based on position of camera positioned below board.
*/
@property (nonatomic, readonly) matrix_float4x4 reflectionViewMatrix;

/*!
 @abstract Compute and store the view projection matrix when either projection or reflection view matrix changes.
 Used for the conversion of positions from world to screen coordinates for computing reflection map
*/
@property (nonatomic, readonly) matrix_float4x4 reflectionViewProjectionMatrix;

/*!
 @abstract initWithSize:
 @param size Current MTKView size in pixels.
 @discussion Creates a new camera for updating the view and projection matrices for Metal rendering.
*/
- (instancetype)initWithSize:(vector_float2)size;

/*!
 @abstract updateSize:
 @param size New size of MTKView in pixels.
 @discussion Updates the size of the view, which the camera will used to generate new projection matrix.
*/
- (void)updateSize:(vector_float2)size;

/*!
 @abstract The following three methods encapsulate conversion of position coordinates between world and screen coordinate spaces.
 */

/*!
 @abstract projectPositionFromModelToScreen:
 @param inPosition World position
 @discussion Converts world position in 3D to point on screen
 */
- (NSPoint)projectPositionFromModelToScreen:(MBCPosition)inPosition;

/*!
 @abstract unProjectPositionFromScreenToModel:fromView:
 @param position The screen position to convert to world coordinates
 @param view The instance of the MTLView for renderer
 @discussion Unprojects a screen position from screen to world coordinates.
 */
- (MBCPosition)unProjectPositionFromScreenToModel:(vector_float2)position fromView:(MBCBoardMTLView *)view;

/*!
 @abstract unProjectPositionFromScreenToModel:fromView:knownY:
 @param position The screen position to convert to world coordinates
 @param knownY The known y position in in world space
 @discussion Unprojects a screen position from screen to world coordinates for a constant y world value.
 */
- (MBCPosition)unProjectPositionFromScreenToModel:(vector_float2)position knownY:(float)knownY;

@end

NS_ASSUME_NONNULL_END
