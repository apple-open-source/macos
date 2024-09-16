/*
    File:        MBCMetalRenderer.h
    Contains:    Manages the Metal rendering for chess board.
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
#import "MBCBoardEnums.h"

NS_ASSUME_NONNULL_BEGIN

@protocol MTLDevice;
@class MBCArrowInstance;
@class MBCBoardMTLView;
@class MBCBoardDecalInstance;
@class MBCMetalCamera;
@class MBCMetalMaterials;
@class MBCPieceInstance;

@interface MBCMetalRenderer : NSObject

/*!
 @abstract The default MTLDevice for rendering Chess games
 */
@property (nonatomic, class, strong, readonly) id<MTLDevice> defaultMTLDevice;

/*!
 @abstract The camera for Metal scene
 */
@property (nonatomic, strong, readonly) MBCMetalCamera *camera;

/*!
 @abstract initWithDevice:mtkView:
 @param device The default Metal device for rendering
 @param mtkView The MBCBoardMTLView (MTKView) used to render Metal content for chess
 @discussion Creates a new renderer used to render all Metal content for chess
*/
- (instancetype)initWithDevice:(id<MTLDevice>)device mtkView:(MBCBoardMTLView *)mtkView;

/*!
 @abstract drawableSizeWillChange:
 @param size Current MTKView size in pixels.
 @discussion Will adjust the size of drawable texture content based on new screen size
*/
- (void)drawableSizeWillChange:(CGSize)size;

/*!
 @abstract drawSceneToView
 @discussion Will execute the render commands needed to render content for current frame
*/
- (void)drawSceneToView;

/*!
 @abstract readPixel:
 @param position Current position of mouse in pixels
 @discussion Will use the main depth attachment to sample the distance from camera.
*/
- (float)readPixel:(vector_float2)position;

/*!
 @abstract cameraDidRotateAboutYAxis
 @discussion Called after the value of the camera's azimuth angle is updated.
 */
- (void)cameraDidRotateAboutYAxis;

/*!
 @abstract setWhitePieceInstances:blackPieceInstances:
 @param whiteInstances Multidimensional array where each element is an array of MBCPieceInstances
 @param blackInstances Multidimensional array where each element is an array of MBCPieceInstances
 @param transparentInstance Nullable instance that uses transparency
 @discussion Updates the renderable chess piece instances with data for white and black pieces. Each parameter is a
 multidimensional array where each element is an array of MBCPieceInstance for a given type of piece.  
*/
- (void)setWhitePieceInstances:(NSArray *)whiteInstances 
           blackPieceInstances:(NSArray *)blackInstances
           transparentInstance:(MBCPieceInstance * _Nullable)transparentInstance;


/*!
 @abstract setHintMoveInstance:lastMoveInstance:
 @param hintInstance Data needed to render the hint arrow instance
 @param lastMoveInstance Data needed to render the last move arrow instance
 @discussion Updates the renderable board arrows to indicate hint move and/or last move
*/
- (void)setHintMoveInstance:(MBCArrowInstance *_Nullable)hintInstance
           lastMoveInstance:(MBCArrowInstance *_Nullable)lastMoveInstance;

/*!
 @abstract setLabelInstances:
 @param labelInstances Array of MBCBoardDecalInstance objects for current frame in hand piece counts
 @discussion The Crazy House game variant stores captured pieces on side of board. Will draw labels
 for each piece count if a piece's count is > 1 in hand.
 */
- (void)setLabelInstances:(NSArray *)labelInstances;

/*!
 @abstract setPieceSelectionInstance:
 @param instance Instance encapsulating data needed to render the piece selection
 @discussion Called to update the position and visibility data for the piece selection graphic displayed under selected piece.
 */
- (void)setPieceSelectionInstance:(MBCBoardDecalInstance * _Nullable)instance;

/*!
 @abstract loadMaterialsForNewStyle:
 @param newStyle The name of the style to use for the board and pieces
 @discussion Loads the MBCDrawStyle instances that define the materials used to render the board and pieces in the scene.
*/
- (void)loadMaterialsForNewStyle:(NSString *)newStyle;

@end

NS_ASSUME_NONNULL_END
