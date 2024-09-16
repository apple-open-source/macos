/*
    File:        MBCRenderable.h
    Contains:    Renderable chess objects to be rendered by Metal
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

#import "MBCBoardEnums.h"
#import <Foundation/Foundation.h>
#import <simd/simd.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 @abstract The number of samples to use for texture mult isampling. Used to configure MTKView
 as well as MTLRenderPipelineStates that need to use multi sampling.
 */
#define RASTER_SAMPLE_COUNT 2

static const int kMaxFramesInFlight = 3;

@class MBCArrowInstance;
@class MBCDrawStyle;
@class MBCPieceInstance;
@class MBCBoardDecalInstance;
@class MTKView;
@class MTLVertexDescriptor;
@protocol MTLBuffer;
@protocol MTLDevice;
@protocol MTLLibrary;
@protocol MTLRenderCommandEncoder;
@protocol MTLRenderPipelineState;

/*!
 @abstract Utility function to get raster sample count supported by GPU.
 */
extern NSUInteger MBCGetSupportedGPUSampleCount(id<MTLDevice> device);

/*!
 @abstract Class used to encapsulate rendering data for game objects that are rendered using Metal.
 */
@interface MBCRenderable : NSObject {
    /*!
     @abstract An array of data buffers for sending instance data to GPU, where the number is kMaxFramesInFlight.
     */
    NSMutableArray<id<MTLBuffer>> * _perFrameDataBuffers;
    
    /*!
     @abstract The current frame index, used to get a buffer from the per frame data buffer array
     */
    NSInteger _inFlightFrame;
    
    /*!
     @abstract The count of opaque instances for current frame
     */
    NSInteger _opaqueInstanceCount;
    
    /*!
     @abstract The count of transparent instances for current frame
     */
    NSInteger _alphaBlendInstanceCount;
    
    /*!
     @abstract The model matrix for the renderable.
     Declaring this ivar to be used in subclasses for the modelMatrix property
     */
    matrix_float4x4 _modelMatrix;
    
    /*!
     @abstract Used to name the debug group for currently drawn renderable
     */
    NSString *_debugName;
}

/*!
 @abstract The model matrix to convert mesh vertex positions to world space coordinates
 */
@property (nonatomic, readonly) matrix_float4x4 modelMatrix;

/*!
 @abstract MBCDrawStyle encapsulates the material information needed to render instances of the renderable.
 */
@property (nonatomic, strong) MBCDrawStyle *drawStyle;

/*!
 @abstract initWithDebugName:
 @param debugName String to use for debug group during render pass
 @discussion Will init a new instance of MBCRenderable for rendering with given string for debug group name
 */
- (instancetype)initWithDebugName:(NSString *)debugName;

/*!
 @abstract initializeMetalWithDevice:library:mtkView:
 @param device Default MTLDevice
 @param library The default Metal library containing chess shaders
 @param mtkView The MTKView instance for the renderer
 @discussion Called after init to set up the Metal objects used by renderable.
 */
- (void)initializeMetalWithDevice:(id<MTLDevice>)device library:(id<MTLLibrary>)library mtkView:(MTKView *)mtkView;

/*!
 @abstract updateForFrame:device:
 @param inFlightFrame The inFlightFrame number used by the renderer for current pass
 @param device The default MTLDevice from the renderer used to make new MTLBuffers when needed
 @discussion Called for renderable to prepare current frame's MTLBuffer data for shaders in current pass.
 */
- (void)updateForFrame:(NSInteger)inFlightFrame device:(id<MTLDevice>)device;

/*!
 @abstract drawMeshes:renderPassType:
 @param renderEncoder The current MTLRenderEncoder for encoding rendering commands in current frame
 @param type The current type of render pass to draw meshes (forward opaque, forward transparent, shadow, etc)
 @param bindMaterial If YES, bind the simple material to renderEncoder. No if reusing same resource
 from the previously drawn renderable.
 @param bindBaseColor If YES, bind the baseColor texture to renderEncoder. No if reusing the same texture
 from the previously drawn renderable.
 @discussion Called to encode the drawing commands for the renderable
 */
- (void)drawMeshes:(nonnull id<MTLRenderCommandEncoder>)renderEncoder
    renderPassType:(MBCRenderPassType)type 
      bindMaterial:(BOOL)bindMaterial
     bindBaseColor:(BOOL)bindBaseColor;

/*!
 @abstract defaultVertexDescriptor
 @discussion Class method to create single instance of MTLVertexDescriptor board and piece renderables.
 */
+ (MTLVertexDescriptor *)defaultVertexDescriptor;

@end

#pragma mark - Shadow Caster

/*!
 @abstract Manages the rendering of objects that only cast shadows
 */
@interface MBCShadowCasterRenderable : MBCRenderable

/*!
 @abstract Stores the instance data for each instance of piece on the board
 */
@property (nonatomic, strong) NSArray<MBCPieceInstance *> *pieceInstances;

@end

#pragma mark - Ground Plane

/*!
 @abstract Manages the rendering data for the ground plane below board
 */
@interface MBCGroundPlane : MBCRenderable

/*!
 @abstract drawGroundMesh:renderPassType:
 @param renderEncoder The current MTLRenderEncoder for encoding rendering commands in current frame
 @param type The current type of render pass to draw meshes (forward opaque, forward transparent, shadow, etc)
 @discussion Called to encode the drawing commands for the renderable
 */
- (void)drawGroundMesh:(id<MTLRenderCommandEncoder>)renderEncoder
        renderPassType:(MBCRenderPassType)type;

@end

#pragma mark - Chess Board

/*!
 @abstract Manages the rendering data for the chess board
 */
@interface MBCBoardRenderable : MBCRenderable

@end

#pragma mark - Chess Piece

/*!
 @abstract Manages the rendering data for the chess pieces
 */
@interface MBCPieceRenderable : MBCRenderable

/*!
 @abstract Store the type and color of piece that is represented
 */
@property (nonatomic, readonly) MBCPiece piece;

/*!
 @abstract Stores the instance data for each instance of this piece type to draw
 */
@property (nonatomic, strong) NSArray<MBCPieceInstance *> *instances;

/*!
 @abstract initWithPiece:maxInstanceCount:debugName:
 @param piece The type and color for the piece
 @param count Maximum number of instances for this type of piece
 @param debugName String used for debug group when rendering instance of MBCPieceRenderable
 @discussion Default initializer for the piece renderable objects
 */
- (instancetype)initWithPiece:(MBCPiece)piece maxInstanceCount:(NSUInteger)count debugName:(NSString *)debugName;

@end

#pragma mark - Move Arrow

/*!
 @abstract Manages the rendering data for the arrow illustrating piece moves between two squares
 */
@interface MBCArrowRenderable : MBCRenderable

/*!
 @abstract Will update the hint arrow instance to be rendered when it is active. Pass nil to turn off.
 */
- (void)setHintInstance:(MBCArrowInstance *_Nullable)instance;

/*!
 @abstract Will update the last move arrow instance to be rendered when it is active. Pass nil to turn off.
 */
- (void)setLastMoveInstance:(MBCArrowInstance *_Nullable)instance;

/*!
 @abstract drawArrowMeshes:renderPassType:
 @param renderEncoder The current MTLRenderEncoder for encoding rendering commands in current frame
 @param type The current type of render pass to draw meshes (forward opaque, forward transparent, shadow, etc)
 @discussion Called to encode the drawing commands for the renderable
 */
- (void)drawArrowMeshes:(id<MTLRenderCommandEncoder>)renderEncoder
         renderPassType:(MBCRenderPassType)type;

@end

#pragma mark - MBCBoardLabelRenderable

/*!
 @abstract Manages the rendering data for the decal graphics drawn on board surface
 */
@interface MBCBoardDecalRenderable : MBCRenderable

/*!
 @abstract initWithTextureName:debugName:
 @param textureName Name of texture from asset catalog  to use for the decal
 @param normalTextureName Name of texture from asset catalog  to use for the decal normals
 @param debugName String to display in metal frame capture
 @discussion Default initializer for instances of renderable for drawing graphics on board surface
 */
- (instancetype)initWithTextureName:(NSString *)textureName 
                  normalTextureName:(nullable NSString *)normalTextureName
                          debugName:(NSString *)debugName;

/*!
 @abstract setInstances:
 @param instances Array of MBCBoardDecalInstance objects for each Crazy House in hand piece count.
 */
- (void)setInstances:(NSArray<MBCBoardDecalInstance *> *)instances;

/*!
 @abstract drawDecalMeshes:renderPassType:
 @param renderEncoder The current MTLRenderEncoder for encoding rendering commands in current frame
 @param type The current type of render pass to draw meshes (forward opaque, forward transparent, shadow, etc)
 @discussion Called to encode the drawing commands for the renderable
 */
- (void)drawDecalMeshes:(id<MTLRenderCommandEncoder>)renderEncoder
         renderPassType:(MBCRenderPassType)type;

@end

NS_ASSUME_NONNULL_END
