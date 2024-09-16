/*
    File:        MBCRenderable.mm
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

#import "MBCRenderable.h"
#import "MBCMathUtilities.h"
#import "MBCBoardMTLView.h"
#import "MBCDrawStyle.h"
#import "MBCMeshLoader.h"
#import "MBCMetalMaterials.h"
#import "MBCShaderTypes.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

/*!
 @abstract If a piece's alpha drops below this value, then it will not cast a shadow.
 */
const float kShadowInclusionAlphaThreshold = 0.5f;

/*!
 @abstract Controls how fast the animated move arrows animate each frame.
 */
const float kArrowTextureSpeed = 2.f;

const MBCSimpleVertex kSimpleQuadVertices[] = {
    { { -1.0f,  -1.0f, } },
    { { -1.0f,   1.0f, } },
    { {  1.0f,  -1.0f, } },

    { {  1.0f,  -1.0f, } },
    { { -1.0f,   1.0f, } },
    { {  1.0f,   1.0f, } },
};

/*!
 @abstract At most will have 26 labels, 16 edge notation (1 - 8, A - H) plus possibly
 5 white in hand pieces and 5 black in hand pieces during Crazy House game variant.
 */
const size_t kMaxLabelInstances = 26;

NSUInteger MBCGetSupportedGPUSampleCount(id<MTLDevice> device) {
    if ([device supportsTextureSampleCount:RASTER_SAMPLE_COUNT]) {
        return RASTER_SAMPLE_COUNT;
    }
    return 1;
}

#pragma mark - MBCRenderable Base Class

@interface MBCRenderable () {
@protected
    NSArray<MBCMesh *> *_meshes;
}

- (NSArray<MBCMesh *> *)loadMeshesWithDescriptor:(MDLVertexDescriptor *)descriptor
                                          device:(id<MTLDevice>)device
                                             url:(NSURL *)url
                                           error:(NSError **)error;
@end

@implementation MBCRenderable

@synthesize modelMatrix = _modelMatrix;

- (instancetype)initWithDebugName:(NSString *)debugName {
    self = [super init];
    if (self) {
        _modelMatrix = matrix_identity_float4x4;
        _debugName = [NSString stringWithFormat:@"Draw %@", debugName];
    }
    return self;
}

- (void)initializeMetalWithDevice:(id<MTLDevice>)device library:(id<MTLLibrary>)library mtkView:(MTKView *)mtkView {
    [self createPerFrameBuffersWithDevice:device];
    if ([self urlForMeshResource]) {
        [self loadMeshesWithDevice:device];
    }
}

+ (MTLVertexDescriptor *)defaultVertexDescriptor {
    // The vertex descriptor defines the layout of verties that are passed into the vertex
    // shader for the board and piece models. See `VertexIn` in MBCShaders.metal.
    
    static dispatch_once_t onceToken;
    static MTLVertexDescriptor *sVertexDescriptor;
    dispatch_once(&onceToken, ^{
        const size_t sizeOfFloat = sizeof(float);
        const size_t sizeOfFloat2 = sizeOfFloat * 2;
        const size_t sizeOfFloat3 = sizeOfFloat * 3;
        // Used to track the offset in bytes between consecutive items in the generics buffer.
        // Also used to set the overall stride of the buffer.
        size_t genericsOffset = 0;
        
        sVertexDescriptor = [[MTLVertexDescriptor alloc] init];
        
        sVertexDescriptor.attributes[MBCVertexAttributePosition].format = MTLVertexFormatFloat3;
        sVertexDescriptor.attributes[MBCVertexAttributePosition].offset = 0;
        sVertexDescriptor.attributes[MBCVertexAttributePosition].bufferIndex = MBCBufferIndexMeshPositions;

        // Texture coordinates.
        sVertexDescriptor.attributes[MBCVertexAttributeTexcoord].format = MTLVertexFormatFloat2;
        sVertexDescriptor.attributes[MBCVertexAttributeTexcoord].offset = genericsOffset;
        sVertexDescriptor.attributes[MBCVertexAttributeTexcoord].bufferIndex = MBCBufferIndexMeshGenerics;
        genericsOffset += sizeOfFloat2;

        // Normals.
        sVertexDescriptor.attributes[MBCVertexAttributeNormal].format = MTLVertexFormatFloat3;
        sVertexDescriptor.attributes[MBCVertexAttributeNormal].offset = genericsOffset;
        sVertexDescriptor.attributes[MBCVertexAttributeNormal].bufferIndex = MBCBufferIndexMeshGenerics;
        genericsOffset += sizeOfFloat3;

        // Tangents
        sVertexDescriptor.attributes[MBCVertexAttributeTangent].format = MTLVertexFormatFloat3;
        sVertexDescriptor.attributes[MBCVertexAttributeTangent].offset = genericsOffset;
        sVertexDescriptor.attributes[MBCVertexAttributeTangent].bufferIndex = MBCBufferIndexMeshGenerics;
        genericsOffset += sizeOfFloat3;

        // Bitangents
        sVertexDescriptor.attributes[MBCVertexAttributeBitangent].format = MTLVertexFormatFloat3;
        sVertexDescriptor.attributes[MBCVertexAttributeBitangent].offset = genericsOffset;
        sVertexDescriptor.attributes[MBCVertexAttributeBitangent].bufferIndex = MBCBufferIndexMeshGenerics;
        genericsOffset += sizeOfFloat3;

        // Position Buffer Layout
        sVertexDescriptor.layouts[MBCBufferIndexMeshPositions].stride = sizeOfFloat3;
        sVertexDescriptor.layouts[MBCBufferIndexMeshPositions].stepRate = 1;
        sVertexDescriptor.layouts[MBCBufferIndexMeshPositions].stepFunction = MTLVertexStepFunctionPerVertex;

        // Generic Attribute Buffer Layout
        sVertexDescriptor.layouts[MBCBufferIndexMeshGenerics].stride = genericsOffset;
        sVertexDescriptor.layouts[MBCBufferIndexMeshGenerics].stepRate = 1;
        sVertexDescriptor.layouts[MBCBufferIndexMeshGenerics].stepFunction = MTLVertexStepFunctionPerVertex;
    });
    
    return sVertexDescriptor;
}

- (void)createPerFrameBuffersWithDevice:(id<MTLDevice>)device {
    NSMutableArray<id<MTLBuffer>> *perFrameBuffers = [NSMutableArray arrayWithCapacity:kMaxFramesInFlight];
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        id<MTLBuffer> buffer = [device newBufferWithLength:sizeof(MBCRenderableData) options:MTLResourceStorageModeShared];
        buffer.label = @"Per Frame Renderable Data";
        [perFrameBuffers addObject:buffer];
    }
    _perFrameDataBuffers = perFrameBuffers;
}

- (NSURL *)urlForMeshResource {
    // Subclasses will provide path to resource for their meshes.
    return nil;
}

- (NSArray<MBCMesh *> *)loadMeshesWithDescriptor:(MDLVertexDescriptor *)descriptor device:(id<MTLDevice>)device url:(NSURL *)url error:(NSError **)error {
    NSArray<MBCMesh *> *meshes = [MBCMesh newMeshesFromURL:url
                                   modelIOVertexDescriptor:descriptor
                                               metalDevice:device
                                                     error:error];
    NSAssert2(meshes, @"Could not create meshes from model file %@: %@", url.absoluteString, *error);
    
    return meshes;
}

- (void)loadMeshesWithDevice:(id<MTLDevice>)device {
    // Create a ModelIO vertexDescriptor so that the format/layout of the ModelIO mesh vertices
    // can be made to match Metal render pipeline's vertex descriptor layout
    MDLVertexDescriptor *modelIOVertexDescriptor = MTKModelIOVertexDescriptorFromMetal([MBCRenderable defaultVertexDescriptor]);
    
    // Indicate how each Metal vertex descriptor attribute maps to each ModelIO attribute
    modelIOVertexDescriptor.attributes[MBCVertexAttributePosition].name  = MDLVertexAttributePosition;
    modelIOVertexDescriptor.attributes[MBCVertexAttributeTexcoord].name  = MDLVertexAttributeTextureCoordinate;
    modelIOVertexDescriptor.attributes[MBCVertexAttributeNormal].name    = MDLVertexAttributeNormal;
    modelIOVertexDescriptor.attributes[MBCVertexAttributeTangent].name   = MDLVertexAttributeTangent;
    modelIOVertexDescriptor.attributes[MBCVertexAttributeBitangent].name = MDLVertexAttributeBitangent;
    
    NSURL *modelFileURL = [self urlForMeshResource];
    NSAssert1(modelFileURL, @"Could not find model (%@) file in bundle", modelFileURL.absoluteString);
    
    NSError *error = nil;
    NSArray<MBCMesh *> *loadedMeshes = [self loadMeshesWithDescriptor:modelIOVertexDescriptor
                                                                device:device
                                                                   url:modelFileURL
                                                                 error:&error];
    NSAssert2(loadedMeshes, @"Could not create meshes from model file %@: %@", modelFileURL.absoluteString, error);
    
    _meshes = loadedMeshes;
}

- (void)updateForFrame:(NSInteger)inFlightFrame device:(nonnull id<MTLDevice>)device {
    _inFlightFrame = inFlightFrame;
    _opaqueInstanceCount = 1;
    _alphaBlendInstanceCount = 0;
    
    MBCRenderableData *frameData = (MBCRenderableData *)_perFrameDataBuffers[inFlightFrame].contents;
    
    frameData->model_matrix = _modelMatrix;
    frameData->alpha = 1.f;
}

- (BOOL)shouldRenderForRenderPassType:(MBCRenderPassType)type {
    
    BOOL haveOpaqueInstance = _opaqueInstanceCount > 0;
    BOOL haveAlphaBlendInstance = _alphaBlendInstanceCount > 0;
    BOOL haveAnyInstance = haveOpaqueInstance || haveAlphaBlendInstance;
    
    return (type == MBCRenderPassTypeShadow && haveAnyInstance) ||
           (type == MBCRenderPassTypeReflection && haveAnyInstance) ||
           (type == MBCRenderPassTypeForward && haveOpaqueInstance) ||
           (type == MBCRenderPassTypeForwardAlphaBlend && haveAlphaBlendInstance);
    
    return NO;
}

- (BOOL)includeTransparentObjectsInMapGeneration {
    return YES;
}

- (void)bindPBRBaseColor:(id<MTLRenderCommandEncoder>)renderEncoder {
    // Set any textures that are read/sampled in the render pipeline
    [renderEncoder setFragmentTexture:_drawStyle.baseColorTexture
                              atIndex:MBCTextureIndexBaseColor];
}

- (void)bindPBRMaterial:(id<MTLRenderCommandEncoder>)renderEncoder {
    // Set bytes for the simple material used for PBR shading
    MBCSimpleMaterial material = _drawStyle.materialForPBR;
    [renderEncoder setFragmentBytes:&material
                             length:sizeof(MBCSimpleMaterial)
                            atIndex:MBCBufferIndexMaterialData];
}

- (void)drawMeshes:(nonnull id<MTLRenderCommandEncoder>)renderEncoder
    renderPassType:(MBCRenderPassType)type
      bindMaterial:(BOOL)bindMaterial
     bindBaseColor:(BOOL)bindBaseColor {
    
    // Bind shared PBR resources before possibly returning early
    // for subsequent renderables that need the resource bound.
    if (type != MBCRenderPassTypeShadow) {
        if (bindBaseColor) {
            [self bindPBRBaseColor:renderEncoder];
        }
        if (bindMaterial) {
            [self bindPBRMaterial:renderEncoder];
        }
    }
    
    // Will not continue if this object isn't rendered in current pass.
    if (![self shouldRenderForRenderPassType:type]) {
        return;
    }
    
    [renderEncoder pushDebugGroup:_debugName];
    
    NSInteger instanceCount = _opaqueInstanceCount;
    
    // Offset into the renderable instance buffer. Transparent instance
    // is located at the end of the instances array.
    NSUInteger bufferOffset = 0;
    
    if (type == MBCRenderPassTypeForwardAlphaBlend) {
        // Only drawing the alpha blended instances
        instanceCount = _alphaBlendInstanceCount;
        
        // Change offset to the first non opaque instance, which are found after opaque instance(s)
        bufferOffset = _opaqueInstanceCount * sizeof(MBCRenderableData);
    } else if (type == MBCRenderPassTypeShadow || type == MBCRenderPassTypeReflection) {
        if ([self includeTransparentObjectsInMapGeneration]) {
            // Drawing all objects to the shadow or reflection maps
            instanceCount += _alphaBlendInstanceCount;
        }
    }
    
    if (instanceCount == 0) {
        // Do not proceed if have no instances, will cause an error.
        return;
    }
    
    // Set instance data per frame vertex buffer
    [renderEncoder setVertexBuffer:_perFrameDataBuffers[_inFlightFrame]
                            offset:bufferOffset
                           atIndex:MBCBufferIndexRenderableData];
    
    // Bind renderable PBR resources that are not shared with other renderables
    if (type != MBCRenderPassTypeShadow) {
        if (_drawStyle.normalMapTexture) {
            [renderEncoder setFragmentTexture:_drawStyle.normalMapTexture
                                      atIndex:MBCTextureIndexNormal];
        }
        
        if (_drawStyle.roughnessAmbientOcclusionTexture) {
            [renderEncoder setFragmentTexture:_drawStyle.roughnessAmbientOcclusionTexture
                                      atIndex:MBCTextureIndexRoughnessAO];
        }
    }
    
    for (__unsafe_unretained MBCMesh *mesh in _meshes) {
        __unsafe_unretained MTKMesh *metalKitMesh = mesh.metalKitMesh;

        // Set mesh's vertex buffers
        for (NSUInteger bufferIndex = 0; bufferIndex < metalKitMesh.vertexBuffers.count; bufferIndex++) {
            __unsafe_unretained MTKMeshBuffer *vertexBuffer = metalKitMesh.vertexBuffers[bufferIndex];
            if ((NSNull*)vertexBuffer != [NSNull null]) {
                [renderEncoder setVertexBuffer:vertexBuffer.buffer
                                        offset:vertexBuffer.offset
                                       atIndex:bufferIndex];
            }
        }

        // Draw each submesh of the mesh
        for (__unsafe_unretained MBCSubmesh *submesh in mesh.submeshes) {
            MTKSubmesh *metalKitSubmesh = submesh.metalKitSubmmesh;

            [renderEncoder drawIndexedPrimitives:metalKitSubmesh.primitiveType
                                      indexCount:metalKitSubmesh.indexCount
                                       indexType:metalKitSubmesh.indexType
                                     indexBuffer:metalKitSubmesh.indexBuffer.buffer
                               indexBufferOffset:metalKitSubmesh.indexBuffer.offset
                                   instanceCount:instanceCount];
        }
    }
    
    [renderEncoder popDebugGroup];
}

@end

#pragma mark - MBCPieceRenderable

@implementation MBCPieceRenderable {
    /*!
     @abstract The maximum number of instances that can be drawn for this MBCPieceRenderable instance's piece type.
     */
    NSUInteger _maxInstanceCount;
    
    /*!
     @abstract Array of piece instance data needed to render each instance of this piece type on board.  For example, piece
     of type white pawn will initially have 8 instances.
     */
    NSArray<MBCPieceInstance *> *_instances;
    
    /*!
     @abstract Will store new per frame buffers if the number of instances changes.  For example, if piece promotion occurs
     then could have more than the starting number of pieces of the type.
     */
    NSMutableArray<id<MTLBuffer>> *_resizedBufferPool;
}

- (instancetype)initWithPiece:(MBCPiece)piece maxInstanceCount:(NSUInteger)count debugName:(NSString *)debugName {
    self = [super initWithDebugName:debugName];
    if (self) {
        _piece = piece;
        _maxInstanceCount = count;
        _resizedBufferPool = [NSMutableArray arrayWithCapacity:kMaxFramesInFlight];
    }
    return self;
}

- (NSMutableArray<id<MTLBuffer>> *)perFrameBuffersWithDevice:(id<MTLDevice>)device {
    NSMutableArray<id<MTLBuffer>> *perFrameBuffers = [NSMutableArray arrayWithCapacity:kMaxFramesInFlight];
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        id<MTLBuffer> buffer = [device newBufferWithLength:sizeof(MBCRenderableData) * _maxInstanceCount
                                                   options:MTLResourceStorageModeShared];
        buffer.label = @"Piece Renderable Data";
        [perFrameBuffers addObject:buffer];
    }
    return perFrameBuffers;
}

- (void)createPerFrameBuffersWithDevice:(id<MTLDevice>)device {
    _perFrameDataBuffers = [self perFrameBuffersWithDevice:device];
}

- (NSURL *)urlForMeshResource {
    MBCPieceCode pieceType = Piece(_piece);
    
    const char *meshNames[7] = { "", "king", "queen", "bishop", "knight", "rook", "pawn" };
    
    const char *name = meshNames[pieceType];
    
    if (strlen(name) > 0) {
        NSString *resourcePath = [NSString stringWithFormat:@"Meshes/%s.usdc", name];
        return [[NSBundle mainBundle] URLForResource:resourcePath withExtension:nil];
    }
    
    return nil;
}

- (void)updateForFrame:(NSInteger)inFlightFrame device:(nonnull id<MTLDevice>)device {
    _inFlightFrame = inFlightFrame;
    
    if (_instances.count > _maxInstanceCount) {
        // Added another instance, need to make new instance buffers for
        // the larger number of instances.
        _maxInstanceCount = _instances.count;
        
        // Store in a pool to not release previous buffers if they are in use.
        _resizedBufferPool = [NSMutableArray arrayWithArray:[self perFrameBuffersWithDevice:device]];
    }
    
    _opaqueInstanceCount = _instances.count;
    _alphaBlendInstanceCount = 0;
    
    if (_resizedBufferPool.count > 0) {
        // Need to replace old buffer with one of the new buffers if they have been created.
        _perFrameDataBuffers[_inFlightFrame] = [_resizedBufferPool lastObject];
        [_resizedBufferPool removeLastObject];
    }
    MBCRenderableData *frameData = (MBCRenderableData *)_perFrameDataBuffers[_inFlightFrame].contents;
    
    for (int i = 0; i < _instances.count; ++i) {
        MBCRenderableData &instanceFrameData = frameData[i];
        
        MBCPieceInstance *instance = _instances[i];
        BOOL isBlack = instance.color == kBlackPiece;

        matrix_float4x4 modelMatrix = matrix_identity_float4x4;
        
        // Scale
        if (instance.scale < 1.f) {
            matrix_float4x4 scale = matrix4x4_scale(instance.scale);
            modelMatrix = simd_mul(scale, modelMatrix);
        }
        
        // Rotate
        if (instance.type == KNIGHT && isBlack) {
            matrix_float4x4 rotateY180 = matrix4x4_rotation(M_PI, kAxisUp);
            modelMatrix = simd_mul(rotateY180, modelMatrix);
        }
        
        // Translate
        vector_float4 position = instance.position;
        matrix_float4x4 translate = matrix4x4_translation(position.x, position.y, position.z);
        modelMatrix = simd_mul(translate, modelMatrix);
        
        // Set the instance data in the MTLBuffer contents
        instanceFrameData.model_matrix = modelMatrix;
        instanceFrameData.alpha = instance.alpha;
        
        // If have transparent index, then will be last instance in array. At most one transparent
        // piece may exists at a time (happens when a dragged piece overlaps another piece)
        if (i == _instances.count - 1 && instance.alpha < 1.f) {
            _opaqueInstanceCount = _instances.count - 1;
            _alphaBlendInstanceCount = 1;
        }
    }
}

- (BOOL)includeTransparentObjectsInMapGeneration {
    // Will not draw the shadow if piece alpha is less than threshold.
    return [_instances lastObject].alpha > kShadowInclusionAlphaThreshold;
}

@end
 
#pragma mark - MBCShadowRenderable

@implementation MBCShadowCasterRenderable

- (NSURL *)urlForMeshResource {
    return [[NSBundle mainBundle] URLForResource:@"Meshes/shadowDisk.usdc" withExtension:nil];
}

- (void)createPerFrameBuffersWithDevice:(id<MTLDevice>)device {
    static const size_t kShadowInstances = 32;
    
    NSMutableArray<id<MTLBuffer>> *perFrameBuffers = [NSMutableArray arrayWithCapacity:kMaxFramesInFlight];
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        id<MTLBuffer> buffer = [device newBufferWithLength:sizeof(MBCRenderableData) * kShadowInstances
                                                   options:MTLResourceStorageModeShared];
        buffer.label = @"Shadow Renderable Data";
        [perFrameBuffers addObject:buffer];
    }
    _perFrameDataBuffers = perFrameBuffers;
}

- (void)updateForFrame:(NSInteger)inFlightFrame device:(nonnull id<MTLDevice>)device {
    _inFlightFrame = inFlightFrame;
    
    
    _opaqueInstanceCount = _pieceInstances.count;
    _alphaBlendInstanceCount = 0;
    
    MBCRenderableData *frameData = (MBCRenderableData *)_perFrameDataBuffers[_inFlightFrame].contents;
    
    for (int i = 0; i < _pieceInstances.count; ++i) {
        MBCRenderableData &instanceFrameData = frameData[i];
        
        MBCPieceInstance *instance = _pieceInstances[i];
        
        // Scale
        float radius = instance.pieceBaseRadius;
        matrix_float4x4 modelMatrix = matrix4x4_scale(radius, radius, radius);
        
        // Rotate
        matrix_float4x4 rotate = matrix4x4_rotation(M_PI_2, kAxisRight);
        modelMatrix = simd_mul(rotate, modelMatrix);
        
        // Translate
        vector_float4 position = instance.position;
        matrix_float4x4 translate = matrix4x4_translation(position.x, position.y, position.z);
        modelMatrix = simd_mul(translate, modelMatrix);
        
        // Set the instance data in the MTLBuffer contents
        instanceFrameData.model_matrix = modelMatrix;
        instanceFrameData.alpha = instance.alpha;
    }
}

@end

#pragma mark - MBCGroundPlane

@implementation MBCGroundPlane {
    id<MTLRenderPipelineState> _renderPipelineState;
    id<MTLBuffer> _vertexBuffer;
    id<MTLTexture> _texture;
}

- (void)initializeMetalWithDevice:(id<MTLDevice>)device library:(id<MTLLibrary>)library mtkView:(MTKView *)mtkView {
    [super initializeMetalWithDevice:device library:library mtkView:mtkView];
    
    [self createPipelineStateWithDevice:device
                                library:library
                                mtkView:mtkView];
    
    // Only need a simple 2D quad for drawing the label.
    _vertexBuffer = [device newBufferWithBytes:kSimpleQuadVertices
                                        length:sizeof(kSimpleQuadVertices)
                                       options:0];
    _vertexBuffer.label = @"Ground Vertices";
    
    // Create a MetalKit texture loader to load material textures from the asset catalog
    MTKTextureLoader *textureLoader = [[MTKTextureLoader alloc] initWithDevice:device];
    
    // Load the textures with shader read using private storage
    NSDictionary *textureLoaderOptions = @{
        MTKTextureLoaderOptionTextureUsage : @(MTLTextureUsageShaderRead),
        MTKTextureLoaderOptionTextureStorageMode : @(MTLStorageModePrivate)
    };
    
    NSError *error;
    _texture = [textureLoader newTextureWithName:@"BoardShadow"
                                     scaleFactor:1.0
                                          bundle:nil
                                         options:textureLoaderOptions
                                           error:&error];
    
    // Move the plane below the board
    matrix_float4x4 translate = matrix4x4_translation(0.f, -kBoardHeight, 0.f);
    
    // Ground plane is 4x the size of the board
    const float scaleFactor = 4 * (kBoardRadius + kBorderWidthMTL);
    matrix_float4x4 matrix = matrix4x4_scale(scaleFactor, scaleFactor, 1.f);
    matrix = matrix_multiply(matrix4x4_rotation(M_PI_2, kAxisRight), matrix);
    matrix = matrix_multiply(translate, matrix);
    _modelMatrix = matrix;
}

- (void)createPipelineStateWithDevice:(id<MTLDevice>)device library:(id<MTLLibrary>)library mtkView:(MTKView *)mtkView {
    NSError *error = nil;
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vsChessGround"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fsChessGround"];
    
    MTLRenderPipelineDescriptor *renderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
    renderPipelineDescriptor.label = @"Ground Pipeline State";
    renderPipelineDescriptor.vertexDescriptor = nil;
    renderPipelineDescriptor.vertexFunction = vertexFunction;
    renderPipelineDescriptor.fragmentFunction = fragmentFunction;
    renderPipelineDescriptor.depthAttachmentPixelFormat = mtkView.depthStencilPixelFormat;
    renderPipelineDescriptor.rasterSampleCount = MBCGetSupportedGPUSampleCount(device);
    renderPipelineDescriptor.colorAttachments[0].pixelFormat = mtkView.colorPixelFormat;
    
    _renderPipelineState = [device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor error:&error];
    NSAssert(error == nil, @"Error creating ground plane color render pipeline state");
}

- (void)updateForFrame:(NSInteger)inFlightFrame device:(id<MTLDevice>)device {
    _inFlightFrame = inFlightFrame;

    _opaqueInstanceCount = 1;
    
    MBCRenderableData *frameData = (MBCRenderableData *)_perFrameDataBuffers[inFlightFrame].contents;
    frameData->model_matrix = _modelMatrix;
    frameData->alpha = 1.f;
}

- (void)drawGroundMesh:(id<MTLRenderCommandEncoder>)renderEncoder renderPassType:(MBCRenderPassType)type {
    [renderEncoder pushDebugGroup:_debugName];
    
    if (type != MBCRenderPassTypeShadow) {
        [renderEncoder setRenderPipelineState:_renderPipelineState];
    }
    
    [renderEncoder setVertexBuffer:_vertexBuffer
                            offset:0
                           atIndex:MBCBufferIndexMeshPositions];
    
    [renderEncoder setVertexBuffer:_perFrameDataBuffers[_inFlightFrame]
                            offset:0
                           atIndex:MBCBufferIndexRenderableData];
    
    [renderEncoder setFragmentTexture:[MBCMetalMaterials shared].groundTexture
                              atIndex:MBCTextureIndexBaseColor];

    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                      vertexStart:0
                      vertexCount:6
                    instanceCount:1];
    
    [renderEncoder popDebugGroup];
}

@end

#pragma mark - MBCBoardRenderable

@implementation MBCBoardRenderable

- (NSURL *)urlForMeshResource {
    return [[NSBundle mainBundle] URLForResource:@"Meshes/board.usdc" withExtension:nil];
}

@end

#pragma mark - MBCArrowRenderable

@implementation MBCArrowRenderable {
    /*!
     @abstract Data needed to draw the hint move arrow if not nil.
     */
    MBCArrowInstance *_hintInstance;
    
    /*!
     @abstract Data needed to draw the last move arrow if not nil.
     */
    MBCArrowInstance *_moveInstance;
    
    /*!
     @abstract The render pipeline state to render the hint move arrow(s)
     */
    id<MTLRenderPipelineState> _hintRenderPipelineState;
    
    /*!
     @abstract The render pipeline states to render last move arrows. Using different
     blending factors for white and black so have two different pipeline states.
     */
    id<MTLRenderPipelineState> _lastBlackMoveRenderPipelineState;
    id<MTLRenderPipelineState> _lastWhiteMoveRenderPipelineState;
    
    /*!
     @abstract The metal buffers containing the simple quad vertices for rendering arrows.
     */
    id<MTLBuffer> _hintVertexBuffer;
    id<MTLBuffer> _moveVertexBuffer;
    
    /*!
     @abstract Buffer containing indices into vertex buffer for defining the plane's triangles
     */
    id<MTLBuffer> _indexBuffer;
    
    /*!
     @abstract Used to track time changes between frames for animating arrow texture.
     */
    CFAbsoluteTime _lastFrameTime;
    
    /*!
     @abstract Used to control the animation effect of the texture in the fragment shader.
     */
    float _animationOffsetHint;
    float _animationOffsetMove;
    
    /*!
     @abstract The textures to be sampled for arrows.
     */
    id<MTLTexture> _hintArrowTexture;
    id<MTLTexture> _lastMoveArrowTexture;
}

- (instancetype)initWithDebugName:(NSString *)debugName {
    self = [super initWithDebugName:debugName];
    if (self) {
        _lastFrameTime = CACurrentMediaTime();
        _animationOffsetHint = 0.f;
        _animationOffsetMove = 0.f;
    }
    return self;
}

- (void)initializeMetalWithDevice:(id<MTLDevice>)device library:(id<MTLLibrary>)library mtkView:(MTKView *)mtkView {
    [super initializeMetalWithDevice:device library:library mtkView:mtkView];
    
    [self createPipelineStateWithDevice:device
                                library:library
                                mtkView:mtkView];
    
    // Create buffers to store vertex position and uv data. The values are calculated
    // in the MBCArrowInstance class for both vertex position and uv coordinates
    _hintVertexBuffer = [device newBufferWithLength:sizeof(MBCArrowVertex) * 8 options:0];
    _hintVertexBuffer.label = @"Move Arrow Vertices";
    
    _moveVertexBuffer = [device newBufferWithLength:sizeof(MBCArrowVertex) * 8 options:0];
    _moveVertexBuffer.label = @"Move Arrow Vertices";
    
    // Vertex indices for the 6 triangles that make up the mesh geometry
    const uint32_t indices[] = {
        0, 3, 2,
        0, 1, 3,
        2, 5, 4,
        2, 3, 5,
        4, 7, 6,
        4, 5, 7
    };
    
    _indexBuffer = [device newBufferWithBytes:indices
                                       length:sizeof(indices)
                                      options:0];
    _indexBuffer.label = @"Arrow Triangle Indices";
}

- (void)createPipelineStateWithDevice:(id<MTLDevice>)device library:(id<MTLLibrary>)library mtkView:(MTKView *)mtkView {
    NSError *error = nil;
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vsChessArrow"];
    id<MTLFunction> fragmentFunctionHint = [library newFunctionWithName:@"fsChessHintArrow"];
    id<MTLFunction> fragmentFunctionLastMove = [library newFunctionWithName:@"fsChessLastMoveArrow"];
    
    MTLRenderPipelineDescriptor *renderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
    renderPipelineDescriptor.label = @"Arrow Pipeline State";
    renderPipelineDescriptor.vertexDescriptor = nil;
    renderPipelineDescriptor.vertexFunction = vertexFunction;
    renderPipelineDescriptor.fragmentFunction = fragmentFunctionHint;
    renderPipelineDescriptor.depthAttachmentPixelFormat = mtkView.depthStencilPixelFormat;
    renderPipelineDescriptor.rasterSampleCount = MBCGetSupportedGPUSampleCount(device);
    renderPipelineDescriptor.colorAttachments[0].pixelFormat = mtkView.colorPixelFormat;
    
    // Enable alpha blending for the transparent pixels of arrow texture.
    renderPipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    renderPipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    renderPipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    renderPipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    renderPipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
    renderPipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
    
    _hintRenderPipelineState = [device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor error:&error];
    NSAssert(error == nil, @"Error creating color render pipeline state");
    
    renderPipelineDescriptor.fragmentFunction = fragmentFunctionLastMove;
    
    _lastWhiteMoveRenderPipelineState = [device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor error:&error];
    
    renderPipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    renderPipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    renderPipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    renderPipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    
    _lastBlackMoveRenderPipelineState = [device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor error:&error];
}

- (void)createPerFrameBuffersWithDevice:(id<MTLDevice>)device {
    static const size_t kMaxArrowInstances = 2;
    
    NSMutableArray<id<MTLBuffer>> *perFrameBuffers = [NSMutableArray arrayWithCapacity:kMaxFramesInFlight];
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        id<MTLBuffer> buffer = [device newBufferWithLength:sizeof(MBCArrowRenderableData) * kMaxArrowInstances
                                                   options:MTLResourceStorageModeShared];
        buffer.label = @"Arrow Renderable Data";
        [perFrameBuffers addObject:buffer];
    }
    _perFrameDataBuffers = perFrameBuffers;
}

- (void)setHintInstance:(MBCArrowInstance *)instance {
    _hintInstance = instance;
    _animationOffsetHint = 0.f;
    if (_hintInstance) {
        [_hintInstance updateMTLVertexBuffer:_hintVertexBuffer];
    }
}

- (void)setLastMoveInstance:(MBCArrowInstance *)instance {
    _moveInstance = instance;
    _animationOffsetMove = 0.f;
    if (_moveInstance) {
        [_moveInstance updateMTLVertexBuffer:_moveVertexBuffer];
    }
}

- (void)updateForFrame:(NSInteger)inFlightFrame device:(id<MTLDevice>)device {
    
    if (!_hintInstance && !_moveInstance) {
        return;
    }
    
    _inFlightFrame = inFlightFrame;
    
    // Determine the time change in order to apply the texture animation
    CFAbsoluteTime current = CACurrentMediaTime();
    CFAbsoluteTime deltaTime = current - _lastFrameTime;
    _lastFrameTime = current;
    
    MBCArrowRenderableData *frameData = (MBCArrowRenderableData *)_perFrameDataBuffers[_inFlightFrame].contents;
    
    if (_hintInstance) {
        _animationOffsetHint = (_animationOffsetHint + deltaTime * kArrowTextureSpeed);
        _animationOffsetHint = _animationOffsetHint > 2 * M_PI ? 0.f : _animationOffsetHint;
        
        float forShader = _animationOffsetHint;
        
        MBCArrowRenderableData &hintData = frameData[0];
        hintData.model_matrix = _hintInstance.modelMatrix;
        hintData.color = _hintInstance.color;
        hintData.animation_offset = forShader;
        hintData.length = _hintInstance.length;
    }
    
    if (_moveInstance) {
        _animationOffsetMove = (_animationOffsetMove + deltaTime * kArrowTextureSpeed);
        _animationOffsetMove = _animationOffsetMove > 2 * M_PI ? 0.f : _animationOffsetMove;
        
        float forShader = _animationOffsetMove;
        
        MBCArrowRenderableData &moveData = frameData[1];
        moveData.model_matrix = _moveInstance.modelMatrix;
        moveData.color = _moveInstance.color;
        moveData.animation_offset = forShader;
        moveData.length = _moveInstance.length;
    }
}

- (void)drawArrowMeshes:(id<MTLRenderCommandEncoder>)renderEncoder
    renderPassType:(MBCRenderPassType)type {
    
    if (!_hintInstance && !_moveInstance) {
        return;
    }
    
    [renderEncoder pushDebugGroup:_debugName];
    
    [renderEncoder setVertexBuffer:_indexBuffer
                            offset:0
                           atIndex:MBCBufferIndexTriangleIndices];
    
    [renderEncoder setVertexBuffer:_perFrameDataBuffers[_inFlightFrame]
                            offset:0
                           atIndex:MBCBufferIndexRenderableData];
    
    // There are 6 triangles in the arrow plane, 3 indices per triangle
    const size_t vertexCount = 3 * 6;
    if (_hintInstance) {
        // Draw the hint arrow
        [renderEncoder setRenderPipelineState:_hintRenderPipelineState];
        
        [renderEncoder setVertexBuffer:_hintVertexBuffer
                                offset:0
                               atIndex:MBCBufferIndexMeshPositions];
        
        [renderEncoder setFragmentTexture:MBCMetalMaterials.shared.hintArrowTexture
                                  atIndex:MBCTextureIndexBaseColor];
        
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:vertexCount
                        instanceCount:1
                         baseInstance:0];
    }
    
    if (_moveInstance) {
        // Draw the last move arrow
        if (_moveInstance.isBlackSideMove) {
            [renderEncoder setRenderPipelineState:_lastBlackMoveRenderPipelineState];
        } else {
            [renderEncoder setRenderPipelineState:_lastWhiteMoveRenderPipelineState];
        }
        
        [renderEncoder setVertexBuffer:_moveVertexBuffer
                                offset:0
                               atIndex:MBCBufferIndexMeshPositions];
        
        [renderEncoder setFragmentTexture:MBCMetalMaterials.shared.lastMoveArrowTexture
                                  atIndex:MBCTextureIndexBaseColor];
        
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:vertexCount
                        instanceCount:1
                         baseInstance:1];
    }
    
    [renderEncoder popDebugGroup];
}

@end

#pragma mark - MBCBoardDecalRenderable

@implementation MBCBoardDecalRenderable {
    NSString *_textureName;
    NSString *_normalTextureName;
    id<MTLRenderPipelineState> _renderPipelineState;
    id<MTLBuffer> _vertexBuffer;
    id<MTLTexture> _texture;
    id<MTLTexture> _normalTexture;
    NSArray<MBCBoardDecalInstance *> *_instances;
    CFAbsoluteTime _lastFrameTime;
    float _totalTime;
}

- (instancetype)initWithTextureName:(NSString *)textureName normalTextureName:(NSString *)normalTextureName debugName:(NSString *)debugName {
    self = [super initWithDebugName:debugName];
    if (self) {
        _textureName = textureName;
        _texture = nil;
        _normalTextureName = normalTextureName;
        _normalTexture = nil;
        _lastFrameTime = CACurrentMediaTime();
        _totalTime = 0.f;
    }
    return self;
}

- (void)initializeMetalWithDevice:(id<MTLDevice>)device library:(id<MTLLibrary>)library mtkView:(MTKView *)mtkView {
    [super initializeMetalWithDevice:device library:library mtkView:mtkView];
    
    [self createPipelineStateWithDevice:device
                                library:library
                                mtkView:mtkView];
    
    // Only need a simple 2D quad for drawing the label.
    _vertexBuffer = [device newBufferWithBytes:kSimpleQuadVertices
                                        length:sizeof(kSimpleQuadVertices)
                                       options:0];
    _vertexBuffer.label = @"Label Vertices";
    
    if ([_textureName isEqualToString:@"DigitGrid"]) {
        _texture = MBCMetalMaterials.shared.edgeNotationTexture;
        _normalTexture = MBCMetalMaterials.shared.edgeNotationNormalTexture;
    } else if ([_textureName isEqualToString:@"PieceSelection"]) {
        _texture = MBCMetalMaterials.shared.pieceSelectionTexture;
    }
}

- (void)createPipelineStateWithDevice:(id<MTLDevice>)device library:(id<MTLLibrary>)library mtkView:(MTKView *)mtkView {
    NSError *error = nil;
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vsChessDecal"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fsChessDecal"];
    
    MTLRenderPipelineDescriptor *renderPipelineDescriptor = [MTLRenderPipelineDescriptor new];
    renderPipelineDescriptor.label = @"Label Pipeline State";
    renderPipelineDescriptor.vertexDescriptor = nil;
    renderPipelineDescriptor.vertexFunction = vertexFunction;
    renderPipelineDescriptor.fragmentFunction = fragmentFunction;
    renderPipelineDescriptor.depthAttachmentPixelFormat = mtkView.depthStencilPixelFormat;
    renderPipelineDescriptor.rasterSampleCount = MBCGetSupportedGPUSampleCount(device);
    renderPipelineDescriptor.colorAttachments[0].pixelFormat = mtkView.colorPixelFormat;
    
    // Enable alpha blending for the transparent pixels of label texture.
    renderPipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    renderPipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    renderPipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    renderPipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
    renderPipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    renderPipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    renderPipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    
    _renderPipelineState = [device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor error:&error];
    NSAssert(error == nil, @"Error creating color render pipeline state");
}

- (void)createPerFrameBuffersWithDevice:(id<MTLDevice>)device {
    NSMutableArray<id<MTLBuffer>> *perFrameBuffers = [NSMutableArray arrayWithCapacity:kMaxFramesInFlight];
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        id<MTLBuffer> buffer = [device newBufferWithLength:sizeof(MBCDecalRenderableData) * kMaxLabelInstances
                                                   options:MTLResourceStorageModeShared];
        buffer.label = [NSString stringWithFormat:@"%@ Decal", _debugName];
        [perFrameBuffers addObject:buffer];
    }
    _perFrameDataBuffers = perFrameBuffers;
}

- (void)setInstances:(NSArray *)instances {
    _instances = instances;
    if (_instances.count > 0) {
        // Reset to start time
        _totalTime = 0.f;
    }
}

- (void)updateForFrame:(NSInteger)inFlightFrame device:(id<MTLDevice>)device {
    
    if (_instances.count == 0) {
        return;
    }
    
    _inFlightFrame = inFlightFrame;
    
    // Determine the time change in order to apply the texture animation
    CFAbsoluteTime currentTime = CACurrentMediaTime();
    CFAbsoluteTime deltaTime = currentTime - _lastFrameTime;
    _lastFrameTime = currentTime;
    _totalTime += deltaTime;
    
    MBCDecalRenderableData *frameData = (MBCDecalRenderableData *)_perFrameDataBuffers[_inFlightFrame].contents;
    
    for (int i = 0; i < _instances.count; ++i) {
        MBCDecalRenderableData &decalData = frameData[i];
        MBCBoardDecalInstance *instance = _instances[i];
        
        decalData.model_matrix = instance.modelMatrix;
        decalData.uv_origin = instance.uvOrigin;
        decalData.uv_scale = instance.uvScale;
        if (instance.animateScale) {
            decalData.quad_vertex_scale = instance.quadVertexScale * (0.95 + 0.05f * sinf(_totalTime * 4.f));
        } else {
            decalData.quad_vertex_scale = instance.quadVertexScale;
        }
        decalData.color = instance.color;
    }
}

- (void)drawDecalMeshes:(id<MTLRenderCommandEncoder>)renderEncoder
         renderPassType:(MBCRenderPassType)type {
    
    if (_instances.count == 0) {
        return;
    }
    
    [renderEncoder pushDebugGroup:_debugName];
    
    [renderEncoder setRenderPipelineState:_renderPipelineState];
    
    [renderEncoder setVertexBuffer:_vertexBuffer
                            offset:0
                           atIndex:MBCBufferIndexMeshPositions];
    
    [renderEncoder setVertexBuffer:_perFrameDataBuffers[_inFlightFrame]
                            offset:0
                           atIndex:MBCBufferIndexRenderableData];
    
    [renderEncoder setFragmentTexture:_texture
                              atIndex:MBCTextureIndexBaseColor];
    
    [renderEncoder setFragmentTexture:_normalTexture
                              atIndex:MBCTextureIndexNormal];

    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                      vertexStart:0
                      vertexCount:6
                    instanceCount:_instances.count];
    
    [renderEncoder popDebugGroup];
}

@end
