/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>

#import <simd/simd.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@interface WebDDVertexAttributeFormat : NSObject

@property (nonatomic, readonly) int32_t semantic;
@property (nonatomic, readonly) int32_t format;
@property (nonatomic, readonly) int32_t layoutIndex;
@property (nonatomic, readonly) int32_t offset;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSemantic:(int32_t)semantic format:(int32_t)format layoutIndex:(int32_t)layoutIndex offset:(int32_t)offset NS_DESIGNATED_INITIALIZER;

@end

@interface WebDDVertexLayout : NSObject

@property (nonatomic, readonly) int32_t bufferIndex;
@property (nonatomic, readonly) int32_t bufferOffset;
@property (nonatomic, readonly) int32_t bufferStride;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithBufferIndex:(int32_t)bufferIndex bufferOffset:(int32_t)bufferOffset bufferStride:(int32_t)bufferStride NS_DESIGNATED_INITIALIZER;

@end

@interface WebAddMeshRequest : NSObject

@property (nonatomic, readonly) int32_t indexCapacity;
@property (nonatomic, readonly) int32_t indexType;
@property (nonatomic, readonly) int32_t vertexBufferCount;
@property (nonatomic, readonly) int32_t vertexCapacity;
@property (nonatomic, readonly, strong) NSArray<WebDDVertexAttributeFormat*> *vertexAttributes;
@property (nonatomic, readonly, strong) NSArray<WebDDVertexLayout*> *vertexLayouts;
@property (nonatomic, readonly, strong) NSString *identifier;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithIndexCapacity:(int32_t)indexCapacity indexType:(int32_t)indexType vertexBufferCount:(int32_t)vertexBufferCount vertexCapacity:(int32_t)vertexCapacity vertexAttributes:(NSArray<WebDDVertexAttributeFormat*> *)vertexAttributes vertexLayouts:(NSArray<WebDDVertexLayout*> *)vertexLayouts identifier:(NSUUID *)identifier NS_DESIGNATED_INITIALIZER;

@end

@interface WebDDMeshPart : NSObject

@property (nonatomic, readonly) unsigned long indexOffset;
@property (nonatomic, readonly) unsigned long indexCount;
@property (nonatomic, readonly) unsigned long topology;
@property (nonatomic, readonly) unsigned long materialIndex;
@property (nonatomic, readonly) simd_float3 boundsMin;
@property (nonatomic, readonly) simd_float3 boundsMax;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndexOffset:(unsigned long)indexOffset indexCount:(unsigned long)indexCount topology:(unsigned long)topology materialIndex:(unsigned long)materialIndex boundsMin:(simd_float3)boundsMin boundsMax:(simd_float3)boundsMax NS_DESIGNATED_INITIALIZER;

@end

@interface WebSetPart : NSObject

@property (nonatomic, readonly) long partIndex;
@property (nonatomic, strong, readonly) WebDDMeshPart *part;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndex:(long)index part:(WebDDMeshPart*)part NS_DESIGNATED_INITIALIZER;

@end

@interface WebSetRenderFlags : NSObject

@property (nonatomic, readonly) long partIndex;
@property (nonatomic, readonly) uint64_t renderFlags;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndex:(long)index renderFlags:(uint64_t)renderFlags NS_DESIGNATED_INITIALIZER;

@end

@interface WebReplaceVertices : NSObject

@property (nonatomic, readonly) long bufferIndex;

@property (nonatomic, strong, readonly) NSData* buffer;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithBufferIndex:(long)bufferIndex buffer:(NSData*)buffer NS_DESIGNATED_INITIALIZER;

@end

@interface WebChainedFloat4x4 : NSObject

@property (nonatomic, readonly) simd_float4x4 transform;
@property (nonatomic, nullable, strong) WebChainedFloat4x4 *next;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithTransform:(simd_float4x4)transform NS_DESIGNATED_INITIALIZER;

@end

@interface WebUpdateMeshRequest : NSObject

@property (nonatomic, readonly) long partCount;
@property (nonatomic, nullable, strong, readonly) NSArray<WebSetPart *> *parts;
@property (nonatomic, nullable, strong, readonly) NSArray<WebSetRenderFlags *> *renderFlags;
@property (nonatomic, nullable, strong, readonly) NSArray<WebReplaceVertices *> *vertices;
@property (nonatomic, nullable, strong, readonly) NSData *indices;
@property (nonatomic, readonly) simd_float4x4 transform;
@property (nonatomic, nullable, strong) WebChainedFloat4x4 *instanceTransforms;
@property (nonatomic, nullable, strong, readonly) NSArray<NSUUID *> *materialIds;
@property (nonatomic, readonly, strong) NSString *identifier;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPartCount:(long)partCount
    parts:(nullable NSArray<WebSetPart *> *)WebSetPart
    renderFlags:(nullable NSArray<WebSetRenderFlags *> *)renderFlags
    vertices:(nullable NSArray<WebReplaceVertices *> *)vertices
    indices:(nullable NSData *)indices
    transform:(simd_float4x4)transform
    instanceTransforms:(nullable WebChainedFloat4x4 *)instanceTransforms
    materialIds:(nullable NSArray<NSUUID *> *)materialIds identifier:(NSUUID *)identifier NS_DESIGNATED_INITIALIZER;

@end

enum class WebDDSemantic {
    kColor,
    kVector,
    kScalar,
    kUnknown
};

@interface WebDDImageAsset : NSObject

@property (nonatomic, strong, readonly) NSString *path;
@property (nonatomic, strong, readonly) NSString *utType;
@property (nonatomic, nullable, strong, readonly) NSData *data;
@property (nonatomic, readonly) WebDDSemantic semantic;
@property (nonatomic, strong, readonly) NSString *identifier;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPath:(NSString *)path utType:(NSString *)utType data:(nullable NSData *)data semantic:(WebDDSemantic)semantic identifier:(NSUUID *)identifier NS_DESIGNATED_INITIALIZER;

@end

@interface WebDDUpdateTextureRequest : NSObject

@property (nonatomic, readonly, strong) WebDDImageAsset *imageAsset;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithImageAsset:(WebDDImageAsset *)imageAsset NS_DESIGNATED_INITIALIZER;

@end

@interface WebDDAddTextureRequest : NSObject

@property (nonatomic, readonly, strong) WebDDImageAsset *imageAsset;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithImageAsset:(WebDDImageAsset *)imageAsset NS_DESIGNATED_INITIALIZER;

@end

@interface WebDDEdge : NSObject

@property (nonatomic, readonly) long upstreamNodeIndex;
@property (nonatomic, readonly) long downstreamNodeIndex;
@property (nonatomic, readonly) NSString *upstreamOutputName;
@property (nonatomic, readonly) NSString *downstreamInputName;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithUpstreamNodeIndex:(long)upstreamNodeIndex
    downstreamNodeIndex:(long)downstreamNodeIndex
    upstreamOutputName:(NSString *)upstreamOutputName
    downstreamInputName:(NSString *)downstreamInputName NS_DESIGNATED_INITIALIZER;

@end

enum class WebDDDataType {
    kBool,
    kInt,
    kInt2,
    kInt3,
    kInt4,
    kFloat,
    kColor3f,
    kColor3h,
    kColor4f,
    kColor4h,
    kFloat2,
    kFloat3,
    kFloat4,
    kHalf,
    kHalf2,
    kHalf3,
    kHalf4,
    kMatrix2f,
    kMatrix3f,
    kMatrix4f,
    kSurfaceShader,
    kGeometryModifier,
    kString,
    kToken,
    kAsset
};

@interface WebDDPrimvar : NSObject

@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) NSString *referencedGeomPropName;
@property (nonatomic, readonly) NSUInteger attributeFormat;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithName:(NSString *)name referencedGeomPropName:(NSString *)referencedGeomPropName attributeFormat:(NSUInteger)attributeFormat NS_DESIGNATED_INITIALIZER;

@end

@interface WebDDInputOutput : NSObject

@property (nonatomic, readonly) WebDDDataType type;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithType:(WebDDDataType)dataType name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

enum class WebDDConstant {
    kBool,
    kUchar,
    kInt,
    kUint,
    kHalf,
    kFloat,
    kTimecode,
    kString,
    kToken,
    kAsset,
    kMatrix2f,
    kMatrix3f,
    kMatrix4f,
    kQuatf,
    kQuath,
    kFloat2,
    kHalf2,
    kInt2,
    kFloat3,
    kHalf3,
    kInt3,
    kFloat4,
    kHalf4,
    kInt4,

    // semantic types
    kPoint3f,
    kPoint3h,
    kNormal3f,
    kNormal3h,
    kVector3f,
    kVector3h,
    kColor3f,
    kColor3h,
    kColor4f,
    kColor4h,
    kTexCoord2h,
    kTexCoord2f,
    kTexCoord3h,
    kTexCoord3f
};

enum class WebDDNodeType {
    kBuiltin,
    kConstant,
    kArguments,
    kResults
};

@interface WebDDConstantContainer : NSObject

@property (nonatomic, readonly) WebDDConstant constant;
@property (nonatomic, readonly, strong) NSArray<NSValue *> *constantValues;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithConstant:(WebDDConstant)constant constantValues:(NSArray<NSValue *> *)constantValues name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end


@interface WebDDBuiltin : NSObject

@property (nonatomic, readonly) NSString *definition;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDefinition:(NSString *)definition name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

@interface WebDDNode : NSObject

@property (nonatomic, readonly) WebDDNodeType bridgeNodeType;
@property (nonatomic, nullable, readonly, strong) WebDDBuiltin *builtin;
@property (nonatomic, nullable, readonly) WebDDConstantContainer *constant;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithBridgeNodeType:(WebDDNodeType)bridgeNodeType builtin:(nullable WebDDBuiltin *)builtin constant:(nullable WebDDConstantContainer *)constant NS_DESIGNATED_INITIALIZER;

@end


@interface WebDDMaterialGraph : NSObject

@property (nonatomic, strong, readonly) NSArray<WebDDNode *> *nodes;
@property (nonatomic, strong, readonly) NSArray<WebDDEdge *> *edges;
@property (nonatomic, strong, readonly) NSArray<WebDDInputOutput *> *inputs;
@property (nonatomic, strong, readonly) NSArray<WebDDInputOutput *> *outputs;
@property (nonatomic, strong, readonly) NSArray<WebDDPrimvar *> *primvars;
@property (nonatomic, strong, readonly) NSString *identifier;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNodes:(NSArray<WebDDNode *> *)nodes edges:(NSArray<WebDDEdge *> *)edges inputs:(NSArray<WebDDInputOutput *> *)inputs outputs:(NSArray<WebDDInputOutput *> *)outputs primvars:(NSArray<WebDDPrimvar *> *)primvars identifier:(NSUUID *)identifier NS_DESIGNATED_INITIALIZER;

@end

@interface WebDDAddMaterialRequest : NSObject

@property (nonatomic, strong, readonly) WebDDMaterialGraph *material;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithMaterial:(WebDDMaterialGraph *)material NS_DESIGNATED_INITIALIZER;

@end

@interface WebDDUpdateMaterialRequest : NSObject

@property (nonatomic, strong, readonly) WebDDMaterialGraph *material;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithMaterial:(WebDDMaterialGraph *)material NS_DESIGNATED_INITIALIZER;

@end

@interface WebUSDModelLoader : NSObject

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (void)loadModelFrom:(NSURL *)url;
- (void)update:(double)deltaTime;
- (void)requestCompleted:(NSObject *)request;
- (void)setCallbacksWithModelAddedCallback:(void (^)(WebAddMeshRequest *))addRequest modelUpdatedCallback:(void (^)(WebUpdateMeshRequest *))modelUpdatedCallback textureAddedCallback:(void (^)(WebDDAddTextureRequest *))textureAddedCallback textureUpdatedCallback:(void (^)(WebDDUpdateTextureRequest *))textureUpdatedCallback materialAddedCallback:(void (^)(WebDDAddMaterialRequest *))materialAddedCallback materialUpdatedCallback:(void (^)(WebDDUpdateMaterialRequest *))materialUpdatedCallback;

@end

NS_HEADER_AUDIT_END(nullability, sendability)
