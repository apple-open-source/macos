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
#pragma once

#import <Foundation/Foundation.h>

#import <simd/simd.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@interface DDBridgeVertexAttributeFormat : NSObject

@property (nonatomic, readonly) int32_t semantic;
@property (nonatomic, readonly) int32_t format;
@property (nonatomic, readonly) int32_t layoutIndex;
@property (nonatomic, readonly) int32_t offset;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSemantic:(int32_t)semantic format:(int32_t)format layoutIndex:(int32_t)layoutIndex offset:(int32_t)offset NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeVertexLayout : NSObject

@property (nonatomic, readonly) int32_t bufferIndex;
@property (nonatomic, readonly) int32_t bufferOffset;
@property (nonatomic, readonly) int32_t bufferStride;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithBufferIndex:(int32_t)bufferIndex bufferOffset:(int32_t)bufferOffset bufferStride:(int32_t)bufferStride NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeAddMeshRequest : NSObject

@property (nonatomic, readonly) int32_t indexCapacity;
@property (nonatomic, readonly) int32_t indexType;
@property (nonatomic, readonly) int32_t vertexBufferCount;
@property (nonatomic, readonly) int32_t vertexCapacity;
@property (nonatomic, readonly, nullable) NSArray<DDBridgeVertexAttributeFormat*>* vertexAttributes;
@property (nonatomic, readonly, nullable) NSArray<DDBridgeVertexLayout*>* vertexLayouts;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndexCapacity:(int32_t)indexCapacity
    indexType:(int32_t)indexType
    vertexBufferCount:(int32_t)vertexBufferCount
    vertexCapacity:(int32_t)vertexCapacity
    vertexAttributes:(nullable NSArray<DDBridgeVertexAttributeFormat*>*)vertexAttributes
    vertexLayouts:(nullable NSArray<DDBridgeVertexLayout*>*)vertexLayouts NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeMeshPart : NSObject

@property (nonatomic, readonly) unsigned long indexOffset;
@property (nonatomic, readonly) unsigned long indexCount;
@property (nonatomic, readonly) unsigned long topology;
@property (nonatomic, readonly) unsigned long materialIndex;
@property (nonatomic, readonly) simd_float3 boundsMin;
@property (nonatomic, readonly) simd_float3 boundsMax;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndexOffset:(unsigned long)indexOffset indexCount:(unsigned long)indexCount topology:(unsigned long)topology materialIndex:(unsigned long)materialIndex boundsMin:(simd_float3)boundsMin boundsMax:(simd_float3)boundsMax NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeSetPart : NSObject

@property (nonatomic, readonly) long partIndex;
@property (nonatomic, readonly, strong) DDBridgeMeshPart *part;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndex:(long)index part:(DDBridgeMeshPart*)part NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeSetRenderFlags : NSObject

@property (nonatomic, readonly) long partIndex;
@property (nonatomic, readonly) uint64_t renderFlags;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithIndex:(long)index renderFlags:(uint64_t)renderFlags NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeReplaceVertices : NSObject

@property (nonatomic, readonly) long bufferIndex;

@property (nonatomic, readonly, strong) NSData* buffer;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithBufferIndex:(long)bufferIndex buffer:(NSData*)buffer NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeChainedFloat4x4 : NSObject

@property (nonatomic) simd_float4x4 transform;
@property (nonatomic, nullable) DDBridgeChainedFloat4x4 *next;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithTransform:(simd_float4x4)transform NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeUpdateMesh : NSObject

@property (nonatomic, readonly) long partCount;
@property (nonatomic, readonly) NSArray<DDBridgeSetPart*> *parts;
@property (nonatomic, readonly) NSArray<DDBridgeSetRenderFlags*> *renderFlags;
@property (nonatomic, readonly) NSArray<DDBridgeReplaceVertices*> *vertices;
@property (nonatomic, readonly, nullable) NSData *indices;
@property (nonatomic, readonly) simd_float4x4 transform;
@property (nonatomic, readonly, nullable) DDBridgeChainedFloat4x4 *instanceTransforms;
@property (nonatomic, readonly) NSArray<NSUUID *> *materialIds;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPartCount:(long)partCount
    parts:(NSArray<DDBridgeSetPart *> *)WebSetPart
    renderFlags:(NSArray<DDBridgeSetRenderFlags *> *)renderFlags
    vertices:(NSArray<DDBridgeReplaceVertices *> *)vertices
    indices:(nullable NSData *)indices
    transform:(simd_float4x4)transform
    instanceTransforms:(nullable DDBridgeChainedFloat4x4 *)instanceTransforms
    materialIds:(NSArray<NSUUID *> *)materialIds NS_DESIGNATED_INITIALIZER;

@end

enum class DDBridgeSemantic {
    kColor,
    kVector,
    kScalar,
    kUnknown
};

@interface DDBridgeImageAsset : NSObject

@property (nonatomic, nullable, strong, readonly) NSData *data;
@property (nonatomic, readonly) NSUInteger width;
@property (nonatomic, readonly) NSUInteger height;
@property (nonatomic, readonly) NSUInteger bytesPerPixel;
@property (nonatomic, readonly) DDBridgeSemantic semantic;
@property (nonatomic, readonly, strong) NSString *path;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithData:(nullable NSData *)data width:(NSUInteger)width height:(NSUInteger)height bytesPerPixel:(NSUInteger)bytesPerPixel semantic:(DDBridgeSemantic)semantic path:(NSString *)path NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeUpdateTextureRequest : NSObject

@property (nonatomic, readonly, strong, nullable) DDBridgeImageAsset *imageAsset;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithImageAsset:(nullable DDBridgeImageAsset *)imageAsset NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeAddTextureRequest : NSObject

@property (nonatomic, readonly, strong, nullable) DDBridgeImageAsset *imageAsset;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithImageAsset:(nullable DDBridgeImageAsset *)imageAsset NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeEdge : NSObject

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

enum class DDBridgeDataType {
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

@interface DDBridgePrimvar : NSObject

@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) NSString *referencedGeomPropName;
@property (nonatomic, readonly) NSUInteger attributeFormat;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithName:(NSString *)name referencedGeomPropName:(NSString *)referencedGeomPropName attributeFormat:(NSUInteger)attributeFormat NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeInputOutput : NSObject

@property (nonatomic, readonly) DDBridgeDataType type;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithType:(DDBridgeDataType)dataType name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

enum class DDBridgeConstant {
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

enum class DDBridgeNodeType {
    kBuiltin,
    kConstant,
    kArguments,
    kResults
};

@interface DDValueString : NSObject

@property (nonatomic, readonly) NSNumber *number;
@property (nonatomic, readonly) NSString *string;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNumber:(NSNumber *)number;
- (instancetype)initWithString:(NSString *)string;

@end

@interface DDBridgeConstantContainer : NSObject

@property (nonatomic, readonly) DDBridgeConstant constant;
@property (nonatomic, readonly, strong) NSArray<DDValueString *> *constantValues;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithConstant:(DDBridgeConstant)constant constantValues:(NSArray<DDValueString *> *)constantValues name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeBuiltin : NSObject

@property (nonatomic, readonly) NSString *definition;
@property (nonatomic, readonly) NSString *name;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDefinition:(NSString *)definition name:(NSString *)name NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeNode : NSObject

@property (nonatomic, readonly) DDBridgeNodeType bridgeNodeType;
@property (nonatomic, readonly, strong) DDBridgeBuiltin *builtin;
@property (nonatomic, readonly) DDBridgeConstantContainer *constant;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithBridgeNodeType:(DDBridgeNodeType)bridgeNodeType builtin:(DDBridgeBuiltin *)builtin constant:(DDBridgeConstantContainer *)constant NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeMaterialGraph : NSObject

@property (nonatomic, strong, readonly) NSArray<DDBridgeNode *> *nodes;
@property (nonatomic, strong, readonly) NSArray<DDBridgeEdge *> *edges;
@property (nonatomic, strong, readonly) NSArray<DDBridgeInputOutput *> *inputs;
@property (nonatomic, strong, readonly) NSArray<DDBridgeInputOutput *> *outputs;
@property (nonatomic, strong, readonly) NSArray<DDBridgePrimvar *> *primvars;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNodes:(NSArray<DDBridgeNode *> *)nodes edges:(NSArray<DDBridgeEdge *> *)edges inputs:(NSArray<DDBridgeInputOutput *> *)inputs outputs:(NSArray<DDBridgeInputOutput *> *)outputs primvars:(NSArray<DDBridgePrimvar *> *)primvars NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeUpdateMaterialRequest : NSObject

@property (nonatomic, strong, readonly) DDBridgeMaterialGraph *material;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithMaterial:(DDBridgeMaterialGraph *)material NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeAddMaterialRequest : NSObject

@property (nonatomic, strong, readonly) DDBridgeMaterialGraph *material;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithMaterial:(DDBridgeMaterialGraph *)material NS_DESIGNATED_INITIALIZER;

@end

@interface DDBridgeReceiver : NSObject

- (void)renderWithTexture:(id<MTLTexture>)texture;
- (bool)addMesh:(DDBridgeAddMeshRequest *)descriptor identifier:(NSUUID*)identifier;
- (void)updateMesh:(DDBridgeUpdateMesh *)descriptor identifier:(NSUUID*)identifier;
- (bool)addTexture:(DDBridgeAddTextureRequest *)descriptor identifier:(NSUUID*)identifier;
- (void)updateTexture:(DDBridgeUpdateTextureRequest *)descriptor identifier:(NSUUID*)identifier;
- (bool)addMaterial:(DDBridgeAddMaterialRequest *)descriptor identifier:(NSUUID*)identifier;
- (void)updateMaterial:(DDBridgeUpdateMaterialRequest *)descriptor identifier:(NSUUID*)identifier;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDevice:(id<MTLDevice>)device NS_DESIGNATED_INITIALIZER;

@end

NS_HEADER_AUDIT_END(nullability, sendability)
