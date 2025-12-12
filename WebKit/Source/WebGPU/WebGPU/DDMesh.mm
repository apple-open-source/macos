/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "DDMesh.h"

#import "APIConversions.h"
#import "Instance.h"
#import "TextureView.h"

#import <wtf/CheckedArithmetic.h>
#import <wtf/MathExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/spi/cocoa/IOSurfaceSPI.h>

#if ENABLE(WEBGPU_SWIFT)
#import "WebGPUSwiftInternal.h"
#endif

namespace WebGPU {

#if ENABLE(WEBGPU_SWIFT)
static NSArray<DDBridgeVertexAttributeFormat*>* convertAttributes(const Vector<WGPUDDVertexAttributeFormat>& vertexAttributes)
{
    NSMutableArray<DDBridgeVertexAttributeFormat*>* result = [NSMutableArray array];
    for (auto& a : vertexAttributes)
        [result addObject:[[DDBridgeVertexAttributeFormat alloc] initWithSemantic:a.semantic format:a.format layoutIndex:a.layoutIndex offset:a.offset]];

    return result;
}
static NSArray<DDBridgeVertexLayout*>* convertLayouts(const Vector<WGPUDDVertexLayout>& layouts)
{
    NSMutableArray<DDBridgeVertexLayout*>* result = [NSMutableArray array];
    for (auto& a : layouts)
        [result addObject:[[DDBridgeVertexLayout alloc] initWithBufferIndex:a.bufferIndex bufferOffset:a.bufferOffset bufferStride:a.bufferStride]];

    return result;
}

static DDBridgeAddMeshRequest* convertDescriptor(const WGPUDDMeshDescriptor& descriptor)
{
    DDBridgeAddMeshRequest* result = [[DDBridgeAddMeshRequest alloc] initWithIndexCapacity:descriptor.indexCapacity
        indexType:descriptor.indexType
        vertexBufferCount:descriptor.vertexBufferCount
        vertexCapacity:descriptor.vertexCapacity
        vertexAttributes:convertAttributes(descriptor.vertexAttributes)
        vertexLayouts:convertLayouts(descriptor.vertexLayouts)
    ];

    return result;
}
#endif

Ref<DDMesh> Instance::createModelBacking(const WGPUDDCreateMeshDescriptor& descriptor)
{
#if ENABLE(WEBGPU_SWIFT)
    return DDMesh::create(descriptor, *this);
#else
    UNUSED_PARAM(descriptor);
    return DDMesh::createInvalid(*this);
#endif
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(DDMesh);

DDMesh::DDMesh(const WGPUDDCreateMeshDescriptor& descriptor, Instance& instance)
    : m_instance(instance)
    , m_descriptor(descriptor)
{
    id<MTLDevice> device = instance.device();
    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:descriptor.width height:descriptor.height mipmapped:NO];
    m_textures = [NSMutableArray array];
    for (RetainPtr ioSurface : descriptor.ioSurfaces)
        [m_textures addObject:[device newTextureWithDescriptor:textureDescriptor iosurface:ioSurface.get() plane:0]];

#if ENABLE(WEBGPU_SWIFT)
    m_ddReceiver = [[DDBridgeReceiver alloc] initWithDevice:instance.device()];
#endif
    m_ddMeshIdentifier = [[NSUUID alloc] init];
}

DDMesh::DDMesh(Instance& instance)
    : m_instance(instance)
{
}

bool DDMesh::isValid() const
{
    return true;
}

id<MTLTexture> DDMesh::texture() const
{
    ++m_currentTexture;
    if (m_currentTexture >= m_textures.count)
        m_currentTexture = 0;

    return m_textures.count ? m_textures[m_currentTexture] : nil;
}

DDMesh::~DDMesh() = default;


#if ENABLE(WEBGPU_SWIFT)
static DDBridgeChainedFloat4x4 *convertFloats(const Vector<simd_float4x4>& fs)
{
    auto count = fs.size();
    if (!count)
        return nil;

    DDBridgeChainedFloat4x4 *result = [[DDBridgeChainedFloat4x4 alloc] initWithTransform:fs[0]];
    DDBridgeChainedFloat4x4 *current = result;

    for (uint32_t i = 1; i < count; ++i) {
        DDBridgeChainedFloat4x4 *next = [[DDBridgeChainedFloat4x4 alloc] initWithTransform:fs[i]];
        current.next = next;
    }

    return result;
}

static DDBridgeMeshPart *convertPart(const WGPUDDMeshPart& part)
{
    return [[DDBridgeMeshPart alloc] initWithIndexOffset:part.indexOffset indexCount:part.indexCount topology:part.topology materialIndex:part.materialIndex boundsMin:part.boundsMin boundsMax:part.boundsMax];
}

static NSArray<DDBridgeSetPart*> *convertParts(const Vector<KeyValuePair<int32_t, WGPUDDMeshPart>>& parts)
{
    NSMutableArray<DDBridgeSetPart*>* result = [NSMutableArray array];
    for (auto& kvp : parts) {
        DDBridgeSetPart* part = [[DDBridgeSetPart alloc] initWithIndex:kvp.key part:convertPart(kvp.value)];
        [result addObject:part];
    }
    return result;
}

static NSArray<NSUUID *> *convertMaterialIDs(const Vector<String>& ids)
{
    NSMutableArray<NSUUID *>* result = [NSMutableArray array];
    for (auto& i : ids)
        [result addObject:[[NSUUID alloc] initWithUUIDString:i.createNSString().get()]];
    return result;
}

static NSData* convertUint8s(const Vector<uint8_t>& data)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return [[NSData alloc] initWithBytes:data.span().data() length:data.sizeInBytes()];
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

static NSArray<DDBridgeSetRenderFlags*> *convertRenderFlags(const Vector<KeyValuePair<int32_t, uint64_t>>& renderFlags)
{
    NSMutableArray<DDBridgeSetRenderFlags*>* result = [NSMutableArray array];
    for (auto& r : renderFlags)
        [result addObject:[[DDBridgeSetRenderFlags alloc] initWithIndex:r.key renderFlags:r.value]];
    return result;
}

static NSArray<DDBridgeReplaceVertices*> *convertVertices(const Vector<WGPUDDReplaceVertices>& vertices)
{
    NSMutableArray<DDBridgeReplaceVertices*>* result = [NSMutableArray array];
    for (auto& v : vertices)
        [result addObject:[[DDBridgeReplaceVertices alloc] initWithBufferIndex:v.bufferIndex buffer:convertUint8s(v.buffer)]];

    return result;
}

static DDBridgeUpdateMesh *convertDescriptor(WGPUDDUpdateMeshDescriptor& descriptor)
{
    DDBridgeUpdateMesh *result = [[DDBridgeUpdateMesh alloc] initWithPartCount:descriptor.partCount
        parts:convertParts(descriptor.parts)
        renderFlags:convertRenderFlags(descriptor.renderFlags)
        vertices:convertVertices(descriptor.vertices)
        indices:convertUint8s(descriptor.indices)
        transform:descriptor.transform
        instanceTransforms:convertFloats(descriptor.instanceTransforms4x4)
        materialIds:convertMaterialIDs(descriptor.materialIds)];

    return result;
}

static DDBridgeSemantic convert(WGPUDDSemantic semantic)
{
    switch (semantic) {
    case WGPUDDSemantic::Color:
        return DDBridgeSemantic::kColor;
    case WGPUDDSemantic::Vector:
        return DDBridgeSemantic::kVector;
    case WGPUDDSemantic::Scalar:
        return DDBridgeSemantic::kScalar;
    case WGPUDDSemantic::Unknown:
    default:
        return DDBridgeSemantic::kUnknown;
    }
}

static DDBridgeImageAsset *convertDescriptor(WGPUDDImageAsset& imageAsset)
{
    return [[DDBridgeImageAsset alloc] initWithData:[NSData dataWithBytes:imageAsset.data.span().data() length:imageAsset.data.sizeInBytes()] width:imageAsset.width height:imageAsset.height bytesPerPixel:imageAsset.bytesPerPixel semantic:convert(imageAsset.semantic) path:imageAsset.path.createNSString().get()];
}

static NSArray<DDBridgePrimvar *> *convert(const Vector<WGPUDDPrimvar>& primvars)
{
    NSMutableArray<DDBridgePrimvar *> *result = [NSMutableArray array];
    for (const auto& p : primvars)
        [result addObject:[[DDBridgePrimvar alloc] initWithName:p.name.createNSString().get() referencedGeomPropName:p.referencedGeomPropName.createNSString().get() attributeFormat:p.attributeFormat]];

    return result;
}

static DDBridgeDataType convert(WGPUDDDataType type)
{
    switch (type) {
    case WGPUDDDataType::kBool:
        return DDBridgeDataType::kBool;
    case WGPUDDDataType::kInt:
        return DDBridgeDataType::kInt;
    case WGPUDDDataType::kInt2:
        return DDBridgeDataType::kInt2;
    case WGPUDDDataType::kInt3:
        return DDBridgeDataType::kInt3;
    case WGPUDDDataType::kInt4:
        return DDBridgeDataType::kInt4;
    case WGPUDDDataType::kFloat:
        return DDBridgeDataType::kFloat;
    case WGPUDDDataType::kColor3f:
        return DDBridgeDataType::kColor3f;
    case WGPUDDDataType::kColor3h:
        return DDBridgeDataType::kColor3h;
    case WGPUDDDataType::kColor4f:
        return DDBridgeDataType::kColor4f;
    case WGPUDDDataType::kColor4h:
        return DDBridgeDataType::kColor4h;
    case WGPUDDDataType::kFloat2:
        return DDBridgeDataType::kFloat2;
    case WGPUDDDataType::kFloat3:
        return DDBridgeDataType::kFloat3;
    case WGPUDDDataType::kFloat4:
        return DDBridgeDataType::kFloat4;
    case WGPUDDDataType::kHalf:
        return DDBridgeDataType::kHalf;
    case WGPUDDDataType::kHalf2:
        return DDBridgeDataType::kHalf2;
    case WGPUDDDataType::kHalf3:
        return DDBridgeDataType::kHalf3;
    case WGPUDDDataType::kHalf4:
        return DDBridgeDataType::kHalf4;
    case WGPUDDDataType::kMatrix2f:
        return DDBridgeDataType::kMatrix2f;
    case WGPUDDDataType::kMatrix3f:
        return DDBridgeDataType::kMatrix3f;
    case WGPUDDDataType::kMatrix4f:
        return DDBridgeDataType::kMatrix4f;
    case WGPUDDDataType::kSurfaceShader:
        return DDBridgeDataType::kSurfaceShader;
    case WGPUDDDataType::kGeometryModifier:
        return DDBridgeDataType::kGeometryModifier;
    case WGPUDDDataType::kString:
        return DDBridgeDataType::kString;
    case WGPUDDDataType::kToken:
        return DDBridgeDataType::kToken;
    case WGPUDDDataType::kAsset:
        return DDBridgeDataType::kAsset;
    }
}

static NSArray<DDBridgeInputOutput *> *convert(const Vector<WGPUDDInputOutput>& inputOutputs)
{
    NSMutableArray<DDBridgeInputOutput *> *result = [NSMutableArray array];
    for (const auto& io : inputOutputs)
        [result addObject:[[DDBridgeInputOutput alloc] initWithType:convert(io.type) name:io.name.createNSString().get()]];

    return result;
}

static NSArray<DDBridgeEdge *> *convert(const Vector<WGPUDDEdge>& edges)
{
    NSMutableArray<DDBridgeEdge *> *result = [NSMutableArray array];
    for (const auto& e : edges) {
        [result addObject:[[DDBridgeEdge alloc] initWithUpstreamNodeIndex:e.upstreamNodeIndex
            downstreamNodeIndex:e.downstreamNodeIndex
            upstreamOutputName:e.upstreamOutputName.createNSString().get()
            downstreamInputName:e.downstreamInputName.createNSString().get()]];
    }

    return result;
}

static DDBridgeNodeType convert(WGPUDDNodeType bridgeNodeType)
{
    switch (bridgeNodeType) {
    case WGPUDDNodeType::Builtin:
        return DDBridgeNodeType::kBuiltin;
    case WGPUDDNodeType::Constant:
        return DDBridgeNodeType::kConstant;
    case WGPUDDNodeType::Arguments:
        return DDBridgeNodeType::kArguments;
    case WGPUDDNodeType::Results:
        return DDBridgeNodeType::kResults;
    }
}

static DDBridgeConstant convert(const WGPUDDConstant constant)
{
    switch (constant) {
    case WGPUDDConstant::kBool:
        return DDBridgeConstant::kBool;
    case WGPUDDConstant::kUchar:
        return DDBridgeConstant::kUchar;
    case WGPUDDConstant::kInt:
        return DDBridgeConstant::kInt;
    case WGPUDDConstant::kUint:
        return DDBridgeConstant::kUint;
    case WGPUDDConstant::kHalf:
        return DDBridgeConstant::kHalf;
    case WGPUDDConstant::kFloat:
        return DDBridgeConstant::kFloat;
    case WGPUDDConstant::kTimecode:
        return DDBridgeConstant::kTimecode;
    case WGPUDDConstant::kString:
        return DDBridgeConstant::kString;
    case WGPUDDConstant::kToken:
        return DDBridgeConstant::kToken;
    case WGPUDDConstant::kAsset:
        return DDBridgeConstant::kAsset;
    case WGPUDDConstant::kMatrix2f:
        return DDBridgeConstant::kMatrix2f;
    case WGPUDDConstant::kMatrix3f:
        return DDBridgeConstant::kMatrix3f;
    case WGPUDDConstant::kMatrix4f:
        return DDBridgeConstant::kMatrix4f;
    case WGPUDDConstant::kQuatf:
        return DDBridgeConstant::kQuatf;
    case WGPUDDConstant::kQuath:
        return DDBridgeConstant::kQuath;
    case WGPUDDConstant::kFloat2:
        return DDBridgeConstant::kFloat2;
    case WGPUDDConstant::kHalf2:
        return DDBridgeConstant::kHalf2;
    case WGPUDDConstant::kInt2:
        return DDBridgeConstant::kInt2;
    case WGPUDDConstant::kFloat3:
        return DDBridgeConstant::kFloat3;
    case WGPUDDConstant::kHalf3:
        return DDBridgeConstant::kHalf3;
    case WGPUDDConstant::kInt3:
        return DDBridgeConstant::kInt3;
    case WGPUDDConstant::kFloat4:
        return DDBridgeConstant::kFloat4;
    case WGPUDDConstant::kHalf4:
        return DDBridgeConstant::kHalf4;
    case WGPUDDConstant::kInt4:
        return DDBridgeConstant::kInt4;

    // semantic:
    case WGPUDDConstant::kPoint3f:
        return DDBridgeConstant::kPoint3f;
    case WGPUDDConstant::kPoint3h:
        return DDBridgeConstant::kPoint3h;
    case WGPUDDConstant::kNormal3f:
        return DDBridgeConstant::kNormal3f;
    case WGPUDDConstant::kNormal3h:
        return DDBridgeConstant::kNormal3h;
    case WGPUDDConstant::kVector3f:
        return DDBridgeConstant::kVector3f;
    case WGPUDDConstant::kVector3h:
        return DDBridgeConstant::kVector3h;
    case WGPUDDConstant::kColor3f:
        return DDBridgeConstant::kColor3f;
    case WGPUDDConstant::kColor3h:
        return DDBridgeConstant::kColor3h;
    case WGPUDDConstant::kColor4f:
        return DDBridgeConstant::kColor4f;
    case WGPUDDConstant::kColor4h:
        return DDBridgeConstant::kColor4h;
    case WGPUDDConstant::kTexCoord2h:
        return DDBridgeConstant::kTexCoord2h;
    case WGPUDDConstant::kTexCoord2f:
        return DDBridgeConstant::kTexCoord2f;
    case WGPUDDConstant::kTexCoord3h:
        return DDBridgeConstant::kTexCoord3h;
    case WGPUDDConstant::kTexCoord3f:
        return DDBridgeConstant::kTexCoord3f;
    }
}

static DDBridgeBuiltin *convert(const WGPUDDBuiltin& builtin)
{
    return [[DDBridgeBuiltin alloc] initWithDefinition:builtin.definition.createNSString().get() name:builtin.name.createNSString().get()];
}

static NSArray<DDValueString *> *convert(const Vector<Variant<String, double>>& constantValues)
{
    NSMutableArray<DDValueString *> *result = [NSMutableArray array];
    for (auto& c : constantValues) {
        [result addObject:WTF::switchOn(c, [&](const String& s) -> DDValueString * {
            return [[DDValueString alloc] initWithString:s.createNSString().get()];
        }, [&] (double d) -> DDValueString * {
            return [[DDValueString alloc] initWithNumber:[[NSNumber alloc] initWithDouble:d]];
        })];
    }

    return result;
}

static DDBridgeConstantContainer *convert(const WGPUDDConstantContainer& constant)
{
    return [[DDBridgeConstantContainer alloc] initWithConstant:convert(constant.constant) constantValues:convert(constant.constantValues) name:constant.name.createNSString().get()];
}

static NSArray<DDBridgeNode *> *convert(const Vector<WGPUDDNode>& nodes)
{
    NSMutableArray<DDBridgeNode *> *result = [NSMutableArray array];
    for (auto& node : nodes)
        [result addObject:[[DDBridgeNode alloc] initWithBridgeNodeType:convert(node.bridgeNodeType) builtin:convert(node.builtin) constant:convert(node.constant)]];
    return result;
}

static DDBridgeMaterialGraph *convertDescriptor(WGPUDDMaterialGraph& material)
{
    return [[DDBridgeMaterialGraph alloc] initWithNodes:convert(material.nodes) edges:convert(material.edges) inputs:convert(material.inputs) outputs:convert(material.outputs) primvars:convert(material.primvars)];
}

static DDBridgeAddTextureRequest *convertDescriptor(WGPUDDTextureDescriptor& descriptor)
{
    DDBridgeAddTextureRequest *result = [[DDBridgeAddTextureRequest alloc] initWithImageAsset:convertDescriptor(descriptor.imageAsset)];

    return result;
}
static DDBridgeUpdateTextureRequest *convertDescriptor(WGPUDDUpdateTextureDescriptor& descriptor)
{
    DDBridgeUpdateTextureRequest *result = [[DDBridgeUpdateTextureRequest alloc] initWithImageAsset:convertDescriptor(descriptor.imageAsset)];

    return result;
}

static DDBridgeAddMaterialRequest *convertDescriptor(WGPUDDMaterialDescriptor& descriptor)
{
    DDBridgeAddMaterialRequest *result = [[DDBridgeAddMaterialRequest alloc] initWithMaterial:convertDescriptor(descriptor.materialGraph)];

    return result;
}
static DDBridgeUpdateMaterialRequest *convertDescriptor(WGPUDDUpdateMaterialDescriptor& descriptor)
{
    DDBridgeUpdateMaterialRequest *result = [[DDBridgeUpdateMaterialRequest alloc] initWithMaterial:convertDescriptor(descriptor.materialGraph)];

    return result;
}
#endif

void DDMesh::render() const
{
#if ENABLE(WEBGPU_SWIFT)
    if (id<MTLTexture> modelBacking = texture())
        [m_ddReceiver renderWithTexture:modelBacking];
#endif
}

void DDMesh::update(WGPUDDUpdateMeshDescriptor* desc)
{
#if ENABLE(WEBGPU_SWIFT)
    if (desc) {
        auto& descriptor = *desc;
        NSString *identifierString = descriptor.identifier.createNSString().get();
        [m_ddReceiver updateMesh:convertDescriptor(*desc) identifier:[[NSUUID alloc] initWithUUIDString:identifierString]];
        render();
    }
#else
    UNUSED_PARAM(desc);
#endif
}

void DDMesh::addMesh(WGPUDDMeshDescriptor* desc)
{
    if (!desc)
        return;

#if ENABLE(WEBGPU_SWIFT)
    auto& descriptor = *desc;
    NSString *identifierString = descriptor.identifier.createNSString().get();
    [m_ddReceiver addMesh:convertDescriptor(descriptor) identifier:[[NSUUID alloc] initWithUUIDString:identifierString]];
#endif
}

void DDMesh::addTexture(WGPUDDTextureDescriptor* desc)
{
    if (!desc)
        return;

#if ENABLE(WEBGPU_SWIFT)
    auto& descriptor = *desc;
    NSString *identifierString = descriptor.imageAsset.identifier.createNSString().get();
    [m_ddReceiver addTexture:convertDescriptor(descriptor) identifier:[[NSUUID alloc] initWithUUIDString:identifierString]];
#endif
}

void DDMesh::updateTexture(WGPUDDUpdateTextureDescriptor* desc)
{
    if (!desc)
        return;

#if ENABLE(WEBGPU_SWIFT)
    auto& descriptor = *desc;
    NSString *identifierString = descriptor.imageAsset.identifier.createNSString().get();
    [m_ddReceiver updateTexture:convertDescriptor(descriptor) identifier:[[NSUUID alloc] initWithUUIDString:identifierString]];
#endif
}

void DDMesh::addMaterial(WGPUDDMaterialDescriptor* desc)
{
    if (!desc)
        return;

#if ENABLE(WEBGPU_SWIFT)
    auto& descriptor = *desc;
    NSString *identifierString = descriptor.materialGraph.identifier.createNSString().get();
    DDBridgeAddMaterialRequest *convertedDecsriptor = convertDescriptor(descriptor);
    [m_ddReceiver addMaterial:convertedDecsriptor identifier:[[NSUUID alloc] initWithUUIDString:identifierString]];
#endif
}

void DDMesh::updateMaterial(WGPUDDUpdateMaterialDescriptor* desc)
{
    if (!desc)
        return;

#if ENABLE(WEBGPU_SWIFT)
    auto& descriptor = *desc;
    DDBridgeUpdateMaterialRequest *convertedDecsriptor = convertDescriptor(descriptor);
    [m_ddReceiver updateMaterial:convertedDecsriptor identifier:[[NSUUID alloc] initWithUUIDString:descriptor.materialGraph.identifier.createNSString().get()]];
#endif
}

}

#pragma mark WGPU Stubs

void wgpuDDMeshReference(WGPUDDMesh mesh)
{
    WebGPU::fromAPI(mesh).ref();
}

void wgpuDDMeshRelease(WGPUDDMesh mesh)
{
    WebGPU::fromAPI(mesh).deref();
}

WGPU_EXPORT void wgpuDDMeshUpdate(WGPUDDMesh mesh, WGPUDDUpdateMeshDescriptor* desc)
{
    WebGPU::protectedFromAPI(mesh)->update(desc);
}

WGPU_EXPORT void wgpuDDMeshAdd(WGPUDDMesh mesh, WGPUDDMeshDescriptor* desc)
{
    WebGPU::protectedFromAPI(mesh)->addMesh(desc);
}

WGPU_EXPORT void wgpuDDMeshRender(WGPUDDMesh mesh)
{
    WebGPU::protectedFromAPI(mesh)->render();
}

WGPU_EXPORT void wgpuDDTextureUpdate(WGPUDDMesh mesh, WGPUDDUpdateTextureDescriptor* desc)
{
    WebGPU::protectedFromAPI(mesh)->updateTexture(desc);
}

WGPU_EXPORT void wgpuDDTextureAdd(WGPUDDMesh mesh, WGPUDDTextureDescriptor* desc)
{
    WebGPU::protectedFromAPI(mesh)->addTexture(desc);
}

WGPU_EXPORT void wgpuDDMaterialUpdate(WGPUDDMesh mesh, WGPUDDUpdateMaterialDescriptor* desc)
{
    WebGPU::protectedFromAPI(mesh)->updateMaterial(desc);
}

WGPU_EXPORT void wgpuDDMaterialAdd(WGPUDDMesh mesh, WGPUDDMaterialDescriptor* desc)
{
    WebGPU::protectedFromAPI(mesh)->addMaterial(desc);
}
