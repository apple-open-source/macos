/*
    File:        MBCMetalRenderer.mm
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

#import "MBCMetalRenderer.h"
#import "MBCMathUtilities.h"
#import "MBCBoardMTLView.h"
#import "MBCMetalCamera.h"
#import "MBCMetalMaterials.h"
#import "MBCRenderable.h"
#import "MBCShaderTypes.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#define MOVE_MAIN_LIGHT_WITH_BOARD 1

/*!
 @typedef MBCPBRShaderOption
 @abstract Specifiy options to control features for PBR pipeline state creation
 @constant MBCPBRShaderOptionUseAlphaBlend  Used to enable alpha blending for pipeline state
 @constant MBCPBRShaderOptionSampleIrradiance  Used to enable irradiance map sampling for pipeline state
 to add additional diffuse lighting to models
 @constant MBCPBRShaderOptionSampleReflection  Used to enable reflection map sampling for pipeline state
 @constant MBCPBRShaderOptionSamplePBRMaps  Used to enable material map sampling in fragment shader.
 Parameters that are sampled will be normals, ambient occlusion and roughness
 @constant MBCPBRShaderOptionUseMSAA  Used to enable multi-sample antialiasing for pipeline state
 @constant MBCPBRShaderOptionUseBoardLighting  Used to apply special board lighting adjustments, such as
 to enable uniform shadows on vertical faces instead of sampling from the shadow map.
 */
typedef NS_OPTIONS(NSUInteger, MBCPBRShaderOption) {
    MBCPBRShaderOptionUseAlphaBlend                 = 1 << 0,
    MBCPBRShaderOptionSampleIrradiance              = 1 << 1,
    MBCPBRShaderOptionSampleReflection              = 1 << 2,
    MBCPBRShaderOptionSamplePBRMaps                 = 1 << 3,
    MBCPBRShaderOptionUseMSAA                       = 1 << 4,
    MBCPBRShaderOptionUseBoardLighting              = 1 << 5,
};

const vector_float3 kDirectionalLightPos = simd_make_float3(-60.f, 200.f, 0.f);

const NSUInteger kShadowTextureDimension = 2048;

const float kLightRed     = 255.f / 255.f;
const float kLightGreen   = 255.f / 255.f;
const float kLightBlue    = 255.f / 255.f;
const vector_float3 kLightColor = simd_make_float3(kLightRed, kLightGreen, kLightBlue);
const float kLightIntensity = 2.f;

/*! 
 Spotlight Parameters
 */
const vector_float3 kSpotlightPosition = simd_make_float3(0.f, 200.f, 0.f);
const vector_float3 kSpotlightColor = simd_make_float3(1.f, 1.f, 1.f);
const vector_float3 kSpotlightConeDirection = simd_normalize(simd_make_float3(0.f, -1.f, 0.f));

/*!
 Umbra - Outer cone angle, no light outside of this angle
 Penumbra - Inner cone where spotlight is full intensity
 */
const float umbraDegrees = 38.f;
const float umbraDelta = 20.f;
const float kSpotlightUmbraAngle = umbraDegrees * (M_PI / 180.f);
const float kSpotlightPenumbraAngle = (umbraDegrees - umbraDelta) * (M_PI / 180.f);

/*! 
 Max reach of the spotlight's lighting capability from light's position
 */
const float kSpotlightMaxDistanceFalloff = 250.f;

typedef struct {
    vector_float3 position;
    vector_float3 color;
    vector_float3 specularColor;
    float diffuseIntensity;
} MBCLightConfiguration;

const MBCLightConfiguration lightConfigurations[] = {
    { kDirectionalLightPos, kLightColor, kLightColor, kLightIntensity },        // Main
    { kSpotlightPosition, kSpotlightColor, kSpotlightColor, kLightIntensity },  // Spot
    { { -60.f, 5.f, 0.f }, kLightColor, { 0.1f, 0.1f, 0.1f }, 1.f },            // Left fill
    { { 50.f, 5.f, 60.f }, kLightColor, { 0.1f, 0.1f, 0.1f }, 1.f }             // Front right fill
};

/*!
 @abstract Blur amount for MPSImageGausianBlur filter used to blur reflection map
 */
const float kReflectionBlurSigma = 3.f;

@interface MBCMetalRenderer () {
    /*!
     @abstract Unique identifier for this renderer. Each window will have its own renderer instance.
     */
    NSString *_rendererID;
    
    /*!
     @abstract Name of the material style being used, one of "Wood", "Marble", or "Metal"
     */
    NSString *_currentMaterialStyle;
    
    /*!
     @abstract Default Metal device
     */
    id<MTLDevice> _device;
    
    /*!
     @abstract The MTLCommandQueue command queue for generating command buffers for submitting draw commands
     */
    id<MTLCommandQueue> _commandQueue;
    
    /*!
     @abstract The default Metal library with Metal shaders for rendering
     */
    id<MTLLibrary> _library;
    
    /*!
     @abstract Depth stencil state for depth attachment of render pipeline
     */
    id<MTLDepthStencilState> _depthStencilState;
    
    /*!
     @abstract The main MTKView for Metal rendering
     */
    __weak MBCBoardMTLView *_mtkView;
    
    /*!
     @abstract Metal buffer used to pass light data to renderer.
     */
    id<MTLBuffer> _lightDataBuffer;
    
    /*!
     @abstract All light positions that are updated when camera rotates around Y axis.
     */
    vector_float3 _lightPositions[MBC_TOTAL_LIGHT_COUNT];
    
    /*!
     @abstract The current frame buffer index, where have a max of 3 in flight
     */
    NSInteger _frameDataBufferIndex;
    
    /*!
     @abstract Semaphore used to limit rendering to max of 3 in flight frames
     */
    dispatch_semaphore_t _inFlightSemaphore;
    
    /*!
     @abstract Buffers used to send common uniform data to GPU for all renderable objects in forward pass
     */
    NSArray<id<MTLBuffer>> * _perFrameDataBuffers;
    
    /*!
     @abstract Buffers used to send common uniform data to GPU for all renderable objects in reflection pass
     */
    NSArray<id<MTLBuffer>> * _perFrameReflectionDataBuffers;
    
    /*!
     @abstract Pipeline state used to render board  using PBR shading and samples the piece reflection map.
     Does not use texture maps for normals or PBR parameters (roughness, ambient occlusion)
     */
    id<MTLRenderPipelineState> _pbrPipelineStateNoPBRMaps;
    
    /*!
     @abstract Pipeline state used to render chess pieces using PBR shading without sampling the
     reflection map. Skips the sampling of the reflection map via use of function constant.
     */
    id<MTLRenderPipelineState> _pbrNoReflectionPipelineState;
    
    /*!
     @abstract  Pipeline state used to render transparent piece using PBR shading with alpha blending
     */
    id<MTLRenderPipelineState> _pbrPipelineStateAlphaBlend;
    
    /*!
     @abstract Pipeline state used to render pieces during reflection map generation
     */
    id<MTLRenderPipelineState> _reflectionPipelineState;
    
    /*!
     @abstract Render pass descritpor for the reflection map generation pass
     */
    MTLRenderPassDescriptor *_reflectionRenderPassDescriptor;
    
    /*!
     @abstract Depth attachment for the reflection map generation pass
     */
    id<MTLTexture> _reflectionDepthTexture;
    
    /*!
     @abstract The non-blurred output of the reflection map pass
     */
    id<MTLTexture> _reflectionTexturePreBlur;
    
    /*!
     @abstract Blurred reflection map used for board reflections in the forward pass
     */
    id<MTLTexture> _reflectionTexture;
    
    /*!
     @abstract Filter used to blur the reflection map
     */
    MPSImageGaussianBlur *_blurFilter;
    
    /*!
     @abstract Metal objects needed for shadow pass
     */
    id <MTLRenderPipelineState> _shadowPipelineState;
    id <MTLDepthStencilState> _shadowDepthStencilState;
    MTLRenderPassDescriptor *_shadowRenderPassDescriptor;
    
    /*!
     @abstract The render target for the shadow pass, which will be used for shadowing in forward pass
     where the lighting calculations are performed.
     */
    id <MTLTexture> _shadowMap;

    /*!
     @abstract Matrices used for rendering shadows.  For shadow pass, the position of the light is used
     as the position for an orthographic camera used to render depth map from lights view angle
     */
    matrix_float4x4 _shadowProjectionMatrix;
    matrix_float4x4 _shadowViewProjectionMatrix;

    /*!
     @abstract Used to convert the position of vertex in forward pass to a uv coordinate used
     to sample from the shadow map to determine if fragment is in shadowed area.
     */
    matrix_float4x4 _shadowViewProjectionUVTransformMatrix;
    
    /*!
     @abstract Used to cast shadows under each chess piece by rendering disks in shadow map generation pass
     */
    MBCShadowCasterRenderable *_shadowCaster;
    
    /*!
     @abstract Surface below board to receive spotlight and board shadows
     */
    MBCGroundPlane *_groundPlane;
    
    /*!
     @abstract Renderable instance that contains data needed for rendering board
     */
    MBCBoardRenderable *_boardRenderable;
    
    /*!
     @abstract Renderable instances containing data needed for rendering chess pieces
     */
    NSArray<MBCPieceRenderable *> *_whitePieceRenderables;
    NSArray<MBCPieceRenderable *> *_blackPieceRenderables;
    
    /*!
     @abstract Renderable piece instance that must be drawn with transparency
     */
    MBCPieceRenderable *_transparentPieceRenderable;
    
    /*!
     @abstract All the piece renderables, provides convenience for iterating over all pieces for applying common methods
     */
    NSMutableArray<MBCRenderable *> *_pieceRenderables;
    
    /*!
     @abstract Renderable instance that contains data needed to render hint and/or last move arrow
     */
    MBCArrowRenderable *_arrowRenderable;
    
    /*!
     @abstract Renderable instance that contains data needed to render the in-hand piece count labels for Crazy House.
     */
    MBCBoardDecalRenderable *_labelRenderable;
    
    /*!
     @abstract Draws selection texture under chess piece when the piece is selected for potential move.
     */
    MBCBoardDecalRenderable *_selectionRenderable;
    
    /*!
     @abstract The current raster sample count used for multi sampling textures.
     */
    NSUInteger _rasterSampleCount;
}
@end

@implementation MBCMetalRenderer

+ (id<MTLDevice>)defaultMTLDevice {
    static id<MTLDevice> sDefaultDevice;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sDefaultDevice = MTLCreateSystemDefaultDevice();
    });
    return sDefaultDevice;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device mtkView:(MBCBoardMTLView *)mtkView {
    self = [super init];
    if (self) {
        _rendererID = [NSUUID UUID].UUIDString;
        _device = device;
        _mtkView = mtkView;
        
        vector_float2 simdSize = simd_make_float2(CGRectGetWidth(mtkView.bounds), CGRectGetHeight(mtkView.bounds));
        _camera = [[MBCMetalCamera alloc] initWithSize:simdSize];
        _inFlightSemaphore = dispatch_semaphore_create(kMaxFramesInFlight);
        
        _blurFilter = [[MPSImageGaussianBlur alloc] initWithDevice:_device sigma:kReflectionBlurSigma];
        
        _rasterSampleCount = MBCGetSupportedGPUSampleCount(device);
        _mtkView.sampleCount = _rasterSampleCount;
        
        [self initializeMetal];
        [self loadAssets];
    }
    return self;
}

- (void)initializeMetal {
    _commandQueue = [_device newCommandQueue];
    _library = [_device newDefaultLibrary];
    
    _mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    
    _mtkView.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
    if ([_device supportsFamily:MTLGPUFamilyApple2]) {
        // Apple Silicon Macs support Shared storage mode.
        _mtkView.depthStencilStorageMode = MTLStorageModeShared;
    } else {
        _mtkView.depthStencilStorageMode = MTLStorageModeManaged;
    }

    _mtkView.depthStencilAttachmentTextureUsage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
    
    {
        // Per frame uniform data
        NSMutableArray<id<MTLBuffer>> *buffers = [NSMutableArray arrayWithCapacity:kMaxFramesInFlight];
        NSMutableArray<id<MTLBuffer>> *reflectionBuffers = [NSMutableArray arrayWithCapacity:kMaxFramesInFlight];
        for (int i = 0; i < kMaxFramesInFlight; ++i) {
            id<MTLBuffer> buffer = [_device newBufferWithLength:sizeof(MBCFrameData) options:MTLResourceStorageModeShared];
            buffer.label = @"Per Frame Data";
            [buffers addObject:buffer];
            
            buffer = [_device newBufferWithLength:sizeof(MBCFrameData) options:MTLResourceStorageModeShared];
            buffer.label = @"Per Frame Reflection Data";
            [reflectionBuffers addObject:buffer];
        }
        _perFrameDataBuffers = [[NSArray alloc] initWithArray:buffers];
        _perFrameReflectionDataBuffers = [[NSArray alloc] initWithArray:reflectionBuffers];
    }
    
    {
        // Main Directional light
        _lightDataBuffer = [_device newBufferWithLength:sizeof(MBCLightData) * MBC_TOTAL_LIGHT_COUNT options:MTLResourceStorageModeShared];
        _lightDataBuffer.label = @"Light Buffer";
        
        MBCLightData* lightData = (MBCLightData *)(_lightDataBuffer.contents);
        
        MBCLightData &mainLight = lightData[0];
        MBCLightConfiguration mainLightConfig = lightConfigurations[0];
        mainLight.position = mainLightConfig.position;
        mainLight.light_color = mainLightConfig.color;
        mainLight.specular_color = mainLightConfig.specularColor;
        mainLight.attenuation = simd_make_float3(1.f, 1.f, 1.f);
        mainLight.light_intensity = mainLightConfig.diffuseIntensity;
        mainLight.normalized_direction = vector_normalize(mainLightConfig.position);
        mainLight.direction_is_position = true;
        _lightPositions[MBC_MAIN_LIGHT_INDEX] = mainLight.position;
        
        // Spotlight
        MBCLightData &spotlight = lightData[1];
        MBCLightConfiguration spotlightConfig = lightConfigurations[MBC_SPOT_LIGHT_INDEX];
        spotlight.position = spotlightConfig.position;
        spotlight.light_color = spotlightConfig.color;
        spotlight.specular_color = spotlightConfig.specularColor;
        spotlight.attenuation = simd_make_float3(1.f, 1.f, 1.f);
        spotlight.light_intensity = 1.f;
        spotlight.normalized_direction = simd_normalize(spotlightConfig.position);
        spotlight.spot_cone_direction = kSpotlightConeDirection;
        spotlight.spot_umbra_angle = kSpotlightUmbraAngle;
        spotlight.spot_penumbra_angle = kSpotlightPenumbraAngle;
        spotlight.spot_max_distance_falloff = kSpotlightMaxDistanceFalloff;
        spotlight.direction_is_position = false;
        _lightPositions[MBC_SPOT_LIGHT_INDEX] = spotlight.position;
        
        // Fill Lights
        for (int fillIndex = MBC_FILL_LIGHT_START_INDEX; fillIndex < MBC_TOTAL_LIGHT_COUNT; ++fillIndex) {
            MBCLightConfiguration fillConfig = lightConfigurations[fillIndex];
            MBCLightData &fillLightFront = lightData[fillIndex];
            fillLightFront.position = fillConfig.position;
            fillLightFront.light_color = fillConfig.color;
            fillLightFront.specular_color = fillConfig.specularColor;
            fillLightFront.attenuation = simd_make_float3(1.f, 1.f, 1.f);
            fillLightFront.light_intensity = fillConfig.diffuseIntensity;
            fillLightFront.normalized_direction = vector_normalize(fillLightFront.position);
            fillLightFront.direction_is_position = false;
            _lightPositions[fillIndex] = fillLightFront.position;
        }
    }
    
    {
        // Forward Pass Depth Stencil
        MTLDepthStencilDescriptor *depthStencilDesc = [[MTLDepthStencilDescriptor alloc] init];
        depthStencilDesc.depthCompareFunction = MTLCompareFunctionLess;
        depthStencilDesc.label = @"Depth Stencil";
        depthStencilDesc.depthWriteEnabled = YES;
        _depthStencilState = [_device newDepthStencilStateWithDescriptor:depthStencilDesc];
    }
    
    {
        // PBR pipeline states for rendering board and pieces using PBR texture maps
        MBCPBRShaderOption options = MBCPBRShaderOptionSampleReflection | MBCPBRShaderOptionSampleIrradiance | MBCPBRShaderOptionUseMSAA | MBCPBRShaderOptionUseBoardLighting;
        _pbrPipelineStateNoPBRMaps = [self defaultPBRPipelineStateWithDevice:_device
                                                                     library:_library
                                                                     mtkView:_mtkView
                                                                     options:options];
        
        options = MBCPBRShaderOptionSamplePBRMaps | MBCPBRShaderOptionSampleIrradiance | MBCPBRShaderOptionUseMSAA;
        _pbrNoReflectionPipelineState = [self defaultPBRPipelineStateWithDevice:_device
                                                                        library:_library
                                                                        mtkView:_mtkView
                                                                        options:options];
        
        options = MBCPBRShaderOptionUseAlphaBlend | MBCPBRShaderOptionSamplePBRMaps | MBCPBRShaderOptionSampleIrradiance | MBCPBRShaderOptionUseMSAA;
        _pbrPipelineStateAlphaBlend = [self defaultPBRPipelineStateWithDevice:_device
                                                                      library:_library
                                                                      mtkView:_mtkView
                                                                      options:options];
    }
    
    {
        // Reflection Pipeline State to generate reflection Map
        MBCPBRShaderOption options = MBCPBRShaderOptionSamplePBRMaps | MBCPBRShaderOptionSampleIrradiance;
        _reflectionPipelineState = [self defaultPBRPipelineStateWithDevice:_device
                                                                   library:_library
                                                                   mtkView:_mtkView 
                                                                   options:options];
        _reflectionRenderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];
    }
    
    {
        // Shadow Render Pipeline State
        NSError *error = nil;
        id <MTLFunction> shadowVertexFunction = [_library newFunctionWithName:@"vsChessShadows"];

        MTLRenderPipelineDescriptor *renderPipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        renderPipelineDescriptor.label = @"Shadow Pipeline";
        renderPipelineDescriptor.vertexDescriptor = nil;
        renderPipelineDescriptor.vertexFunction = shadowVertexFunction;
        renderPipelineDescriptor.fragmentFunction = nil;
        renderPipelineDescriptor.depthAttachmentPixelFormat = _mtkView.depthStencilPixelFormat;

        _shadowPipelineState = [_device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor
                                                                       error:&error];
        
        // Shadow Depth Stencil
        MTLDepthStencilDescriptor *depthStencilDesc = [[MTLDepthStencilDescriptor alloc] init];
        depthStencilDesc.depthCompareFunction = MTLCompareFunctionLessEqual;
        depthStencilDesc.label = @"Shadow Depth Stencil";
        depthStencilDesc.depthWriteEnabled = YES;
        _shadowDepthStencilState = [_device newDepthStencilStateWithDescriptor:depthStencilDesc];
        
        // Shadow Texture
        MTLTextureDescriptor *shadowTextureDesc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:_mtkView.depthStencilPixelFormat
                                                               width:kShadowTextureDimension
                                                              height:kShadowTextureDimension
                                                           mipmapped:NO];

        shadowTextureDesc.resourceOptions = MTLResourceStorageModePrivate;
        shadowTextureDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;

        _shadowMap = [_device newTextureWithDescriptor:shadowTextureDesc];
        _shadowMap.label = @"Shadow Map";
        
        // Shadow Render Pass Descriptor
        _shadowRenderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];
        _shadowRenderPassDescriptor.depthAttachment.texture = _shadowMap;
        _shadowRenderPassDescriptor.depthAttachment.loadAction = MTLLoadActionClear;
        _shadowRenderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
        _shadowRenderPassDescriptor.depthAttachment.clearDepth = 1.0;
        
        // Create orthographic camera for generating shadow map, based on size of board.
        // Parameter order is: left, right, bottom, top, nearZ, farZ
        const float centerToEdgeDistance = kBoardRadius + kBorderWidthMTL;
        const float centerToCornerDistance = sqrt(pow(centerToEdgeDistance, 2) * 2);
        
        const float kBoardHalfSize = centerToCornerDistance + 1;
        _shadowProjectionMatrix = matrix_ortho_right_hand(-kBoardHalfSize, kBoardHalfSize, -kBoardHalfSize, kBoardHalfSize, -kBoardHalfSize, kBoardHalfSize);
        
        [self updateShadowMatricesForViewChange];
    }
}

/*!
 @abstract defaultPBRPipelineStateWithDevice:library:mtkView:options:
 @param device The default MTLDevice from renderer
 @param library The default MTLLibrary from the renderer
 @param mtkView The MTKView for displaying Metal content
 @param options Set of options to determine whether or not to enable shader features
 @discussion Class method to create single instance of PBR pipeline state for board and piece renderables.
 */
- (id<MTLRenderPipelineState>)defaultPBRPipelineStateWithDevice:(id<MTLDevice>)device
                                                        library:(id<MTLLibrary>)library
                                                        mtkView:(MTKView *)mtkView
                                                        options:(MBCPBRShaderOption)options {
    NSError *error = nil;
    BOOL usePBRMaps = (options & MBCPBRShaderOptionSamplePBRMaps) != 0;
    BOOL sampleReflection = (options & MBCPBRShaderOptionSampleReflection) != 0;
    BOOL alphaBlend = (options & MBCPBRShaderOptionUseAlphaBlend) != 0;
    BOOL sampleIrradiance = (options & MBCPBRShaderOptionSampleIrradiance) != 0;
    BOOL useMSAA = (options & MBCPBRShaderOptionUseMSAA) != 0;
    BOOL useBoardLighting = (options & MBCPBRShaderOptionUseBoardLighting) != 0;
    
    MTLFunctionConstantValues* functionConstants = [[MTLFunctionConstantValues alloc] init];
    [functionConstants setConstantValue:&usePBRMaps type:MTLDataTypeBool atIndex:MBCFunctionConstantIndexUseMaterialMaps];
    [functionConstants setConstantValue:&sampleReflection type:MTLDataTypeBool atIndex:MBCFunctionConstantIndexSampleReflectionMap];
    [functionConstants setConstantValue:&sampleIrradiance type:MTLDataTypeBool atIndex:MBCFunctionConstantIndexSampleIrradianceMap];
    [functionConstants setConstantValue:&useBoardLighting type:MTLDataTypeBool atIndex:MBCFunctionConstantIndexUseBoardLighting];
    
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vsChess"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fsChess" constantValues:functionConstants error:&error];
    NSAssert(error == nil, @"Error creating fsChess fragment function");
    
    MTLRenderPipelineDescriptor *renderPipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    renderPipelineDescriptor.label = @"Color Pipeline State";
    renderPipelineDescriptor.vertexDescriptor = [MBCRenderable defaultVertexDescriptor];
    renderPipelineDescriptor.vertexFunction = vertexFunction;
    renderPipelineDescriptor.fragmentFunction = fragmentFunction;
    renderPipelineDescriptor.depthAttachmentPixelFormat = mtkView.depthStencilPixelFormat;
    renderPipelineDescriptor.rasterSampleCount = useMSAA ? _rasterSampleCount : 1;
    renderPipelineDescriptor.colorAttachments[0].pixelFormat = mtkView.colorPixelFormat;
    
    if (alphaBlend) {
        renderPipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
        renderPipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        renderPipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        renderPipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        renderPipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        renderPipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        renderPipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    }
    
    id<MTLRenderPipelineState> renderPipelineState = [device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor error:&error];
    NSAssert(error == nil, @"Error creating color render pipeline state");
    
    return renderPipelineState;
}

- (void)loadAssets {
    // Piece Shadows
    _shadowCaster = [[MBCShadowCasterRenderable alloc] initWithDebugName:@"Shadow Caster"];
    
    // Ground Plane below board
    _groundPlane = [[MBCGroundPlane alloc] initWithDebugName:@"Ground Plane"];
    
    // Board
    _boardRenderable = [[MBCBoardRenderable alloc] initWithDebugName:@"Board"];
    
    // Pieces
    // Black Pieces
    // Order based on MBCPieceCode, where the first item at 0 is EMPTY.
    NSMutableArray *pieceRenderables = [NSMutableArray arrayWithCapacity:7];
    [pieceRenderables addObject:[NSNull null]];
    [pieceRenderables addObject:[[MBCPieceRenderable alloc] initWithPiece:KING maxInstanceCount:1 debugName:@"King Black"]];
    [pieceRenderables addObject:[[MBCPieceRenderable alloc] initWithPiece:QUEEN maxInstanceCount:1 debugName:@"Queen Black"]];
    [pieceRenderables addObject:[[MBCPieceRenderable alloc] initWithPiece:BISHOP maxInstanceCount:2 debugName:@"Bishop Black"]];
    [pieceRenderables addObject:[[MBCPieceRenderable alloc] initWithPiece:KNIGHT maxInstanceCount:2 debugName:@"Knight Black"]];
    [pieceRenderables addObject:[[MBCPieceRenderable alloc] initWithPiece:ROOK maxInstanceCount:2 debugName:@"Rook Black"]];
    [pieceRenderables addObject:[[MBCPieceRenderable alloc] initWithPiece:PAWN maxInstanceCount:8 debugName:@"Pawn Black"]];
    _blackPieceRenderables = [NSArray arrayWithArray:pieceRenderables];
    
    // White Pieces
    pieceRenderables[KING] = [[MBCPieceRenderable alloc] initWithPiece:KING maxInstanceCount:1 debugName:@"King White"];
    pieceRenderables[QUEEN] = [[MBCPieceRenderable alloc] initWithPiece:QUEEN maxInstanceCount:1 debugName:@"Queen White"];
    pieceRenderables[BISHOP] = [[MBCPieceRenderable alloc] initWithPiece:BISHOP maxInstanceCount:2 debugName:@"Bishop White"];
    pieceRenderables[KNIGHT] = [[MBCPieceRenderable alloc] initWithPiece:KNIGHT maxInstanceCount:2 debugName:@"Knight White"];
    pieceRenderables[ROOK] = [[MBCPieceRenderable alloc] initWithPiece:ROOK maxInstanceCount:2 debugName:@"Rook White"];
    pieceRenderables[PAWN] = [[MBCPieceRenderable alloc] initWithPiece:PAWN maxInstanceCount:8 debugName:@"Pawn White"];
    _whitePieceRenderables = [NSArray arrayWithArray:pieceRenderables];
    
    // Initialize the renderables, start with 1 as PieceCode 0 is EMPTY
    for (int i = 1; i < 7; ++i) {
        [_blackPieceRenderables[i] initializeMetalWithDevice:_device library:_library mtkView:_mtkView];
        [_whitePieceRenderables[i] initializeMetalWithDevice:_device library:_library mtkView:_mtkView];
    }
    
    [_shadowCaster initializeMetalWithDevice:_device library:_library mtkView:_mtkView];
    
    [_groundPlane initializeMetalWithDevice:_device library:_library mtkView:_mtkView];
    
    [_boardRenderable initializeMetalWithDevice:_device library:_library mtkView:_mtkView];
    
    // Move Arrows
    _arrowRenderable = [[MBCArrowRenderable alloc] initWithDebugName:@"Move Arrow"];
    [_arrowRenderable initializeMetalWithDevice:_device library:_library mtkView:_mtkView];
    
    // Board Labels for in-hand piece counts
    _labelRenderable = [[MBCBoardDecalRenderable alloc] initWithTextureName:@"DigitGrid" 
                                                          normalTextureName:@"DigitGridNormals"
                                                                  debugName:@"Label"];
    [_labelRenderable initializeMetalWithDevice:_device library:_library mtkView:_mtkView];
    
    // Texture drawn under currently selected piece
    _selectionRenderable = [[MBCBoardDecalRenderable alloc] initWithTextureName:@"PieceSelection"
                                                              normalTextureName:nil
                                                                      debugName:@"Piece Selection"];
    [_selectionRenderable initializeMetalWithDevice:_device library:_library mtkView:_mtkView];
}

- (void)loadMaterialsForNewStyle:(NSString *)newStyle {
    
    if (_currentMaterialStyle && [_currentMaterialStyle isEqualToString:newStyle]) {
        return;
    }
    
    NSString *oldStyle = [_currentMaterialStyle copy];
    _currentMaterialStyle = [newStyle copy];
    
    MBCMetalMaterials *materials = [MBCMetalMaterials shared];
    
    [materials loadMaterialsForRendererID:_rendererID newStyle:newStyle];
    
    _boardRenderable.drawStyle = [materials boardMaterialWithStyle:newStyle];
    
    _whitePieceRenderables[KING].drawStyle = [materials materialForPiece:White(KING) style:newStyle];
    _blackPieceRenderables[KING].drawStyle = [materials materialForPiece:Black(KING) style:newStyle];
    
    _whitePieceRenderables[QUEEN].drawStyle = [materials materialForPiece:White(QUEEN) style:newStyle];
    _blackPieceRenderables[QUEEN].drawStyle = [materials materialForPiece:Black(QUEEN) style:newStyle];
    
    _whitePieceRenderables[BISHOP].drawStyle = [materials materialForPiece:White(BISHOP) style:newStyle];
    _blackPieceRenderables[BISHOP].drawStyle = [materials materialForPiece:Black(BISHOP) style:newStyle];
    
    _whitePieceRenderables[KNIGHT].drawStyle = [materials materialForPiece:White(KNIGHT) style:newStyle];
    _blackPieceRenderables[KNIGHT].drawStyle = [materials materialForPiece:Black(KNIGHT) style:newStyle];
    
    _whitePieceRenderables[ROOK].drawStyle = [materials materialForPiece:White(ROOK) style:newStyle];
    _blackPieceRenderables[ROOK].drawStyle = [materials materialForPiece:Black(ROOK) style:newStyle];
    
    _whitePieceRenderables[PAWN].drawStyle = [materials materialForPiece:White(PAWN) style:newStyle];
    _blackPieceRenderables[PAWN].drawStyle = [materials materialForPiece:Black(PAWN) style:newStyle];
    
    // Release usage of old style.
    if (oldStyle) {
        [materials releaseUsageForStyle:oldStyle rendererID:_rendererID];
    }
}

- (void)drawableSizeWillChange:(CGSize)size {
    vector_float2 simdSize = simd_make_float2(size.width, size.height);
    [_camera updateSize:simdSize];
    
    [self updateReflectionTexturesForSize:size];
}

/*!
 @abstract updateReflectionTexturesForSize:
 @param size The new size of drawable that will determine size of reflection textures.
 @discussion Create textures for rendering the reflection map. Using half width and height
 for textures since do not need a sharp image for the reflections.
 */
- (void)updateReflectionTexturesForSize:(CGSize)size {
    // Reflection color map textures
    MTLTextureDescriptor *descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:_mtkView.colorPixelFormat
                                                           width:size.width * 0.5f
                                                          height:size.height * 0.5f
                                                       mipmapped:NO];
    descriptor.resourceOptions = MTLResourceStorageModePrivate;
    descriptor.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
    _reflectionTexture = [_device newTextureWithDescriptor:descriptor];
    
    descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    _reflectionTexturePreBlur = [_device newTextureWithDescriptor:descriptor];
    
    // Reflection depth texture
    descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:_mtkView.depthStencilPixelFormat
                                                           width:size.width * 0.5f
                                                          height:size.height * 0.5f
                                                       mipmapped:NO];
    descriptor.resourceOptions = MTLResourceStorageModeMemoryless;
    descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    _reflectionDepthTexture = [_device newTextureWithDescriptor:descriptor];
    
    // Update the reflection render pass descriptor with new, resized textures
    _reflectionRenderPassDescriptor.colorAttachments[0].texture = _reflectionTexturePreBlur;
    _reflectionRenderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    _reflectionRenderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.f, 0.f, 0.f, 0.f);
    _reflectionRenderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

    _reflectionRenderPassDescriptor.depthAttachment.texture = _reflectionDepthTexture;
}

- (void)updateSceneState {
    _frameDataBufferIndex = (_frameDataBufferIndex + 1) % kMaxFramesInFlight;
    
    // Forward Pass Buffers
    MBCFrameData *frameData = (MBCFrameData *)_perFrameDataBuffers[_frameDataBufferIndex].contents;
    
    frameData->view_matrix = _camera.viewMatrix;
    frameData->projection_matrix = _camera.projectionMatrix;
    frameData->view_projection_matrix = _camera.viewProjectionMatrix;
    frameData->camera_position = _camera.position;
    frameData->viewport_size = simd_make_uint2(_camera.viewport.z, _camera.viewport.w);
    frameData->shadow_vp_matrix = _shadowViewProjectionMatrix;
    frameData->shadow_vp_texture_matrix = _shadowViewProjectionUVTransformMatrix;
    frameData->light_count = MBC_TOTAL_LIGHT_COUNT;
    frameData->main_light_specular_intensity = cos(_camera.elevation * kDegrees2Radians);
    
    for (int i = 0; i < MBC_TOTAL_LIGHT_COUNT; ++i) {
        frameData->light_positions[i] = _lightPositions[i];
    }
    
    // Refelection Pass Buffers
    frameData = (MBCFrameData *)_perFrameReflectionDataBuffers[_frameDataBufferIndex].contents;
    
    frameData->view_matrix = _camera.reflectionViewMatrix;
    frameData->projection_matrix = _camera.projectionMatrix;
    frameData->view_projection_matrix = _camera.reflectionViewProjectionMatrix;
    frameData->camera_position = _camera.position;
    frameData->viewport_size = simd_make_uint2(_camera.viewport.z, _camera.viewport.w);
    frameData->shadow_vp_matrix = _shadowViewProjectionMatrix;
    frameData->shadow_vp_texture_matrix = _shadowViewProjectionUVTransformMatrix;
    
    frameData->light_count = MBC_TOTAL_LIGHT_COUNT;
    frameData->main_light_specular_intensity = cos(_camera.elevation * kDegrees2Radians);
    
    for (int i = 0; i < MBC_TOTAL_LIGHT_COUNT; ++i) {
        frameData->light_positions[i] = _lightPositions[i];
    }

    // Update all renderables for frame, start with 1 as PieceCode 0 is EMPTY
    for (int i = 1; i < 7; ++i) {
        [_blackPieceRenderables[i] updateForFrame:_frameDataBufferIndex device:_device];
        [_whitePieceRenderables[i] updateForFrame:_frameDataBufferIndex device:_device];
    }
    [_shadowCaster updateForFrame:_frameDataBufferIndex device:_device];
    [_groundPlane updateForFrame:_frameDataBufferIndex device:_device];
    [_boardRenderable updateForFrame:_frameDataBufferIndex device:_device];
    [_arrowRenderable updateForFrame:_frameDataBufferIndex device:_device];
    [_labelRenderable updateForFrame:_frameDataBufferIndex device:_device];
    [_selectionRenderable updateForFrame:_frameDataBufferIndex device:_device];
}

- (void)drawShadows:(id<MTLCommandBuffer>)commandBuffer {
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:_shadowRenderPassDescriptor];
    renderEncoder.label = @"Shadow Map Pass";

    [renderEncoder setRenderPipelineState:_shadowPipelineState];
    [renderEncoder setDepthStencilState:_shadowDepthStencilState];
    
    [renderEncoder setCullMode:MTLCullModeBack];
    
    // Set the bias to reduce shadow artifacting
    [renderEncoder setDepthBias:0.015 slopeScale:7 clamp:0.02];

    [renderEncoder setVertexBuffer:_perFrameDataBuffers[_frameDataBufferIndex]
                      offset:0
                     atIndex:MBCBufferIndexFrameData];

    [self drawOpaqueSceneMeshes:renderEncoder renderPassType:MBCRenderPassTypeShadow];

    [renderEncoder endEncoding];
}

- (void)drawPieces:(id<MTLRenderCommandEncoder>)renderEncoder
    renderPassType:(MBCRenderPassType)type {
    // Black pieces, start with 1 as PieceCode 0 is EMPTY
    for (int i = 1; i < 7; ++i) {
        // Do not need to rebind shared material and base
        // color texture onto ecoder for each piece.
        BOOL isFirstBlack = (i == 1);
        [_blackPieceRenderables[i] drawMeshes:renderEncoder renderPassType:MBCRenderPassTypeReflection bindMaterial:isFirstBlack bindBaseColor:isFirstBlack];
    }
    
    // White pieces, start with 1 as PieceCode 0 is EMPTY
    for (int i = 1; i < 7; ++i) {
        // Do not need to rebind shared material and base
        // color texture onto encoder for each piece.
        BOOL isFirstWhite = (i == 1);
        [_whitePieceRenderables[i] drawMeshes:renderEncoder renderPassType:MBCRenderPassTypeReflection bindMaterial:NO bindBaseColor:isFirstWhite];
    }
}

- (void)drawReflectionMap:(id<MTLCommandBuffer>)commandBuffer {
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:_reflectionRenderPassDescriptor];
    renderEncoder.label = @"Reflection Map";
    
    [renderEncoder setDepthStencilState:_depthStencilState];
    [renderEncoder setRenderPipelineState:_reflectionPipelineState];
    [renderEncoder setCullMode:MTLCullModeBack];
    
    // Set the common vertex and fragment uniforms
    [renderEncoder setVertexBuffer:_perFrameReflectionDataBuffers[_frameDataBufferIndex]
                            offset:0
                           atIndex:MBCBufferIndexFrameData];
    
    [renderEncoder setFragmentBuffer:_perFrameReflectionDataBuffers[_frameDataBufferIndex]
                              offset:0
                             atIndex:MBCBufferIndexFrameData];
    
    // Set the common light data for fragment shader
    [renderEncoder setFragmentBuffer:_lightDataBuffer
                              offset:0
                             atIndex:MBCBufferIndexLightsData];
    
    // Set the shadow map for forward render pass to calculate shadows
    [renderEncoder setFragmentTexture:_shadowMap
                              atIndex:MBCTextureIndexShadow];
    
    // Irradiance map for additional diffuse lighting
    [renderEncoder setFragmentTexture:[MBCMetalMaterials shared].irradianceMap
                              atIndex:MBCTextureIndexIrradianceMap];
    
    // Change the winding since have a right handed projection matrix. Using right hand because
    // OpenGL is right handed, and thus game position data is currently based on right hand use.
    [renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    
    [self drawPieces:renderEncoder renderPassType:MBCRenderPassTypeReflection];
    
    [renderEncoder endEncoding];
    
    // Blur the resulting reflection map texture and store in texture for forward pass.
    [_blurFilter encodeToCommandBuffer:commandBuffer
                         sourceTexture:_reflectionTexturePreBlur
                    destinationTexture:_reflectionTexture];
    
}

- (void)drawOpaqueSceneMeshes:(id<MTLRenderCommandEncoder>)renderEncoder renderPassType:(MBCRenderPassType)type {
    // Change the winding since have a right handed projection matrix. Using right hand because
    // OpenGL is right handed, and thus game position data is currently based on right hand use.
    [renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    
    if (type == MBCRenderPassTypeForward) {
        // PBR pipeline opaque objects without sampling reflection map
        [renderEncoder setRenderPipelineState:_pbrNoReflectionPipelineState];
    }
    
    [self drawPieces:renderEncoder renderPassType:type];
    
    if (type == MBCRenderPassTypeForward) {
        // PBR pipeline opaque objects with reflection sampling
        [renderEncoder setRenderPipelineState:_pbrPipelineStateNoPBRMaps];
    }
    [_boardRenderable drawMeshes:renderEncoder renderPassType:type bindMaterial:YES bindBaseColor:YES];
    
    if (type == MBCRenderPassTypeShadow) {
        [_shadowCaster drawMeshes:renderEncoder renderPassType:type bindMaterial:YES bindBaseColor:YES];
    }
}

- (void)drawTransparentSceneMeshes:(id<MTLRenderCommandEncoder>)renderEncoder {
    // Change the winding since have a right handed projection matrix. Using right hand because
    // OpenGL is right handed, and thus game position data is currently based on right hand use.
    [renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    
    // PBR pipeline with alpha blend for transparent rendering
    [renderEncoder setRenderPipelineState:_pbrPipelineStateAlphaBlend];
    
    [_transparentPieceRenderable drawMeshes:renderEncoder
                             renderPassType:MBCRenderPassTypeForwardAlphaBlend
                               bindMaterial:YES
                              bindBaseColor:YES];
}

- (void)drawArrows:(id<MTLRenderCommandEncoder>)renderEncoder {
    [renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [_arrowRenderable drawArrowMeshes:renderEncoder renderPassType:MBCRenderPassTypeForward];
}

- (void)drawLabels:(id<MTLRenderCommandEncoder>)renderEncoder {
    [renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [_labelRenderable drawDecalMeshes:renderEncoder renderPassType:MBCRenderPassTypeForward];
}

- (void)drawSelection:(id<MTLRenderCommandEncoder>)renderEncoder {
    [renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [_selectionRenderable drawDecalMeshes:renderEncoder renderPassType:MBCRenderPassTypeForward];
}

- (void)drawGroundPlane:(id<MTLRenderCommandEncoder>)renderEncoder {
    [renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [_groundPlane drawGroundMesh:renderEncoder renderPassType:MBCRenderPassTypeForward];
}

- (void)drawSceneToView {
    // Ensure only kMaxFramesInFlight are getting processed by any stage in the Metal pipeline
    dispatch_semaphore_wait(_inFlightSemaphore, DISPATCH_TIME_FOREVER);
    
    // Update scene data that will be passed to GPU for this frame.
    [self updateSceneState];
    
    id <MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    commandBuffer.label = @"Chess Command Buffer";
    [self drawShadows:commandBuffer];
    
    [self drawReflectionMap:commandBuffer];
    
    dispatch_semaphore_t semaphore = _inFlightSemaphore;
    [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        dispatch_semaphore_signal(semaphore);
    }];
    
    MTLRenderPassDescriptor *renderPassDescriptor = _mtkView.currentRenderPassDescriptor;
    if (renderPassDescriptor) {
        // Storing depth attachment so can read values for projecting / unprojecting mouse clicks
        // between screen space and world space coordinates
        renderPassDescriptor.depthAttachment.storeAction = MTLStoreActionStore;
        
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        renderEncoder.label = @"Forward Pass Meshes";
        
        [renderEncoder setCullMode:MTLCullModeBack];
        
        [renderEncoder setDepthStencilState:_depthStencilState];
        
        // Set the common vertex and fragment uniforms
        [renderEncoder setVertexBuffer:_perFrameDataBuffers[_frameDataBufferIndex]
                                offset:0
                               atIndex:MBCBufferIndexFrameData];
        
        [renderEncoder setFragmentBuffer:_perFrameDataBuffers[_frameDataBufferIndex]
                                  offset:0
                                 atIndex:MBCBufferIndexFrameData];
        
        // Set the common light data for fragment shader
        [renderEncoder setFragmentBuffer:_lightDataBuffer
                                  offset:0
                                 atIndex:MBCBufferIndexLightsData];
        
        // Set the shadow map for forward render pass to calculate shadows
        [renderEncoder setFragmentTexture:_shadowMap
                                  atIndex:MBCTextureIndexShadow];
        
        // Set the reflection map for forward render pass to calculate board reflections
        [renderEncoder setFragmentTexture:_reflectionTexture
                                  atIndex:MBCTextureIndexReflection];
        
        // Irradiance map for additional diffuse lighting
        [renderEncoder setFragmentTexture:[MBCMetalMaterials shared].irradianceMap
                                  atIndex:MBCTextureIndexIrradianceMap];
        
        [self drawGroundPlane:renderEncoder];
        
        [self drawOpaqueSceneMeshes:renderEncoder renderPassType:MBCRenderPassTypeForward];
        
        if (_transparentPieceRenderable) {
            [self drawTransparentSceneMeshes:renderEncoder];
        }
        
        // Drawing the next three based on their increasing Y position values. They
        // require alpha blending and thus render order matters. The Y position
        // values are defined in MBCBoardCommon.h
        [self drawLabels:renderEncoder];
        
        [self drawArrows:renderEncoder];
        
        [self drawSelection:renderEncoder];
        
        [renderEncoder endEncoding];
        
        id<MTLDrawable> currentDrawable = _mtkView.currentDrawable;
        [commandBuffer addScheduledHandler:^(id<MTLCommandBuffer> _Nonnull commandBuffer) {
            [currentDrawable present];
        }];
    }

    // Finalize rendering here & push the command buffer to the GPU
    [commandBuffer commit];
}

- (float)readPixel:(vector_float2)position {
    if(!_mtkView.depthStencilTexture) {
        return 1.0;
    }

    CGFloat height = _mtkView.drawableSize.height;
    CGFloat width = _mtkView.drawableSize.width;
    
    NSUInteger pixelX = position.x;
    NSUInteger pixelY = height - position.y;
    
    if (pixelX >= width || pixelY >= height) {
        // Region will be out of bounds if either are true since width, height
        // of the region are both 1.
        return 1.0;
    }

    MTLRegion region = MTLRegionMake2D(pixelX, pixelY, 1, 1);
    
    NSAssert1(_mtkView.depthStencilTexture.storageMode != MTLStorageModePrivate,
              @"Invalid depthStencilTexture storage mode %@", @(_mtkView.depthStencilTexture.storageMode));
    
    // For multisample textures, the getBytes method below consecutively positions each sample
    // within a pixel in memory and treats the pixels as part of one row. Depth stencil pixel format
    // is MTLPixelFormatDepth32Float (4 bytes).
    const float bytesPerPixel = 4;
    NSUInteger bytesPerRow = _mtkView.depthStencilTexture.width * bytesPerPixel * _rasterSampleCount;
    float depth = 0;
    [_mtkView.depthStencilTexture getBytes:&depth
                               bytesPerRow:bytesPerRow
                                fromRegion:region
                               mipmapLevel:0];
    return depth;
}

- (void)updateShadowMatricesForViewChange {
    // Shadows are based on the main directional light.
    vector_float4 lightPosition = simd_make_float4(_lightPositions[MBC_MAIN_LIGHT_INDEX], 0.f);
    vector_float4 lightWorldDirection = lightPosition;
    const float lightPositionScale = 10.f;
    
    matrix_float4x4 shadowViewMatrix = matrix_look_at_right_hand(lightWorldDirection.xyz / lightPositionScale, (vector_float3){ 0.f, 0.f, 0.f }, kAxisUp);
    _shadowViewProjectionMatrix = matrix_multiply(_shadowProjectionMatrix, shadowViewMatrix);
    
    // When calculating texture coordinates to sample from shadow map, flip the y/t coordinate and
    // convert from the [-1, 1] range of clip coordinates to [0, 1] range used for texture sampling
    matrix_float4x4 shadowScale = matrix4x4_scale(0.5f, -0.5f, 1.0);        // [-1.0, 1.0] -> [-0.5, 0.5]
    matrix_float4x4 shadowTranslate = matrix4x4_translation(0.5, 0.5, 0);   // [-0.5, 0.5] -> [0.0, 1.0]
    matrix_float4x4 shadowTransform = matrix_multiply(shadowTranslate, shadowScale);

    // This matrix is used to generate the uv coordinates to sample shadow map. Will multiply the vertex
    // position by its model matrix, followed by shadow_vp_matrix to put position in clip space for
    // shadow.  Multiplying by shadowTransform will then give the [0, 1] range for texture sample.
    _shadowViewProjectionUVTransformMatrix = matrix_multiply(shadowTransform, _shadowViewProjectionMatrix);
}

- (void)cameraDidRotateAboutYAxis {
    // Update the position of the scene lights so they remain same position relative
    // to camera. The lights will need to rotate opposite of the camera rotation.
    matrix_float4x4 lightRotation = matrix4x4_rotation(-(_camera.azimuth - 180.f) * kDegrees2Radians, kAxisUp);
    for (int i = 0; i < MBC_TOTAL_LIGHT_COUNT; ++i) {
        if (i == MBC_MAIN_LIGHT_INDEX && !MOVE_MAIN_LIGHT_WITH_BOARD) {
            // Moving main light is off, don't change position.
            continue;
        }
        vector_float3 originalPosition = lightConfigurations[i].position;
        vector_float3 position = simd_mul(lightRotation, simd_make_float4(originalPosition, 1.f)).xyz;
        _lightPositions[i] = position;
    }
    
    if (MOVE_MAIN_LIGHT_WITH_BOARD) {
        [self updateShadowMatricesForViewChange];
    }
}

- (void)setWhitePieceInstances:(NSArray *)whiteInstances
           blackPieceInstances:(NSArray *)blackInstances
           transparentInstance:(MBCPieceInstance *)transparentInstance {
    
    _whitePieceRenderables[KING].instances = whiteInstances[KING];
    _blackPieceRenderables[KING].instances = blackInstances[KING];
    
    _whitePieceRenderables[QUEEN].instances = whiteInstances[QUEEN];
    _blackPieceRenderables[QUEEN].instances = blackInstances[QUEEN];
    
    _whitePieceRenderables[BISHOP].instances = whiteInstances[BISHOP];
    _blackPieceRenderables[BISHOP].instances = blackInstances[BISHOP];
    
    _whitePieceRenderables[KNIGHT].instances = whiteInstances[KNIGHT];
    _blackPieceRenderables[KNIGHT].instances = blackInstances[KNIGHT];
    
    _whitePieceRenderables[ROOK].instances = whiteInstances[ROOK];
    _blackPieceRenderables[ROOK].instances = blackInstances[ROOK];
    
    _whitePieceRenderables[PAWN].instances = whiteInstances[PAWN];
    _blackPieceRenderables[PAWN].instances = blackInstances[PAWN];
    
    NSMutableArray *instances = [NSMutableArray arrayWithCapacity:32];
    for (int i = 1; i <= 6; ++i) {
        [instances addObjectsFromArray:whiteInstances[i]];
        [instances addObjectsFromArray:blackInstances[i]];
    }
    _shadowCaster.pieceInstances = instances;
    
    if (transparentInstance) {
        if (transparentInstance.color == kWhitePiece) {
            _transparentPieceRenderable = _whitePieceRenderables[transparentInstance.type];
        } else {
            _transparentPieceRenderable = _blackPieceRenderables[transparentInstance.type];
        }
    } else {
        _transparentPieceRenderable = nil;
    }
}

- (void)setHintMoveInstance:(MBCArrowInstance *)hintInstance
           lastMoveInstance:(MBCArrowInstance *)lastMoveInstance {
    [_arrowRenderable setHintInstance:hintInstance];
    [_arrowRenderable setLastMoveInstance:lastMoveInstance];
}

- (void)setLabelInstances:(NSArray *)labelInstances {
    NSMutableArray *labelsToRender = [NSMutableArray arrayWithCapacity:labelInstances.count];
    for (MBCBoardDecalInstance *instance in labelInstances) {
        if (instance.isVisible) {
            [labelsToRender addObject:instance];
        }
    }
    [_labelRenderable setInstances:labelsToRender];
}

- (void)setPieceSelectionInstance:(MBCBoardDecalInstance *)instance {
    // Draw the piece selection graphic if visible
    NSArray *instances = instance.isVisible ? @[instance] : @[];
    [_selectionRenderable setInstances:instances];
}

@end
