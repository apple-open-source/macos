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

#include "config.h"
#include "DDMeshImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "DDMaterialDescriptor.h"
#include "DDMeshDescriptor.h"
#include "DDTextureDescriptor.h"
#include "DDUpdateMaterialDescriptor.h"
#include "DDUpdateMeshDescriptor.h"
#include "DDUpdateTextureDescriptor.h"
#include "ModelConvertToBackingContext.h"
#include <WebGPU/WebGPUExt.h>
#include <wtf/TZoneMallocInlines.h>
#include <WebCore/IOSurface.h>

namespace WebCore::DDModel {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DDMeshImpl);

DDMeshImpl::DDMeshImpl(WebGPU::WebGPUPtr<WGPUDDMesh>&& ddMesh, Vector<UniqueRef<WebCore::IOSurface>>&& renderBuffers, ConvertToBackingContext& convertToBackingContext)
    : m_convertToBackingContext(convertToBackingContext)
    , m_backing(WTFMove(ddMesh))
    , m_renderBuffers(WTFMove(renderBuffers))
{
}

DDMeshImpl::~DDMeshImpl() = default;

void DDMeshImpl::setLabelInternal(const String&)
{
    // FIXME: Implement this.
}

#if PLATFORM(COCOA)
static Vector<KeyValuePair<int32_t, WGPUDDMeshPart>> convertToBacking(const Vector<KeyValuePair<int32_t, DDMeshPart>>& parts)
{
    Vector<KeyValuePair<int32_t, WGPUDDMeshPart>> result;
    for (auto& p : parts) {
        result.append(KeyValuePair(WTFMove(p.key), WGPUDDMeshPart {
            .indexOffset = p.value.indexOffset,
            .indexCount = p.value.indexCount,
            .topology = p.value.topology,
            .materialIndex = p.value.materialIndex,
            .boundsMin = WTFMove(p.value.boundsMin),
            .boundsMax = WTFMove(p.value.boundsMax)
        }));
    }
    return result;
}
static Vector<WGPUDDReplaceVertices> convertToBacking(const Vector<DDReplaceVertices>& vertices)
{
    Vector<WGPUDDReplaceVertices> result;
    for (auto& p : vertices) {
        result.append(WGPUDDReplaceVertices {
            .bufferIndex = p.bufferIndex,
            .buffer = WTFMove(p.buffer)
        });
    }
    return result;
}

static Vector<simd_float4x4> toVector(const Vector<DDFloat4x4>& input)
{
    Vector<simd_float4x4> result;
    result.reserveCapacity(input.size());
    for (auto& float4x4 : input)
        result.append(float4x4);

    return result;
}

static Vector<WGPUDDVertexAttributeFormat> convertToBacking(const Vector<DDModel::DDVertexAttributeFormat>& descriptor)
{
    Vector<WGPUDDVertexAttributeFormat> result;
    for (auto& d : descriptor) {
        result.append(WGPUDDVertexAttributeFormat {
            .semantic = d.semantic,
            .format = d.format,
            .layoutIndex = d.layoutIndex,
            .offset = d.offset
        });
    }
    return result;
}

static Vector<WGPUDDVertexLayout> convertToBacking(const Vector<DDModel::DDVertexLayout>& descriptor)
{
    Vector<WGPUDDVertexLayout> result;
    for (auto& d : descriptor) {
        result.append(WGPUDDVertexLayout {
            .bufferIndex = d.bufferIndex,
            .bufferOffset = d.bufferOffset,
            .bufferStride = d.bufferStride,
        });
    }
    return result;
}

void DDMeshImpl::addMesh(const DDModel::DDMeshDescriptor& descriptor)
{
    WGPUDDMeshDescriptor backingDescriptor {
        .indexCapacity = descriptor.indexCapacity,
        .indexType = descriptor.indexType,
        .vertexBufferCount = descriptor.vertexBufferCount,
        .vertexCapacity = descriptor.vertexCapacity,
        .vertexAttributes = convertToBacking(descriptor.vertexAttributes),
        .vertexLayouts = convertToBacking(descriptor.vertexLayouts),
        .identifier = descriptor.identifier
    };

    wgpuDDMeshAdd(m_backing.get(), &backingDescriptor);
}

void DDMeshImpl::update(const DDUpdateMeshDescriptor& descriptor)
{
    WGPUDDUpdateMeshDescriptor backingDescriptor {
        .partCount = descriptor.partCount,
        .parts = convertToBacking(descriptor.parts),
        .renderFlags = WTFMove(descriptor.renderFlags),
        .vertices = convertToBacking(descriptor.vertices),
        .indices = WTFMove(descriptor.indices),
        .transform = WTFMove(descriptor.transform),
        .instanceTransforms4x4 = toVector(descriptor.instanceTransforms4x4),
        .materialIds = WTFMove(descriptor.materialIds),
        .identifier = descriptor.identifier
    };

    wgpuDDMeshUpdate(m_backing.get(), &backingDescriptor);
}

static WGPUDDSemantic convert(DDSemantic semantic)
{
    switch (semantic) {
    case DDSemantic::Color:
        return WGPUDDSemantic::Color;
    case DDSemantic::Vector:
        return WGPUDDSemantic::Vector;
    case DDSemantic::Scalar:
        return WGPUDDSemantic::Scalar;
    case DDSemantic::Unknown:
        return WGPUDDSemantic::Unknown;
    }
}

void DDMeshImpl::addTexture(const DDModel::DDTextureDescriptor& descriptor)
{
    WGPUDDTextureDescriptor backingDescriptor {
        .imageAsset = WGPUDDImageAsset {
            .data = descriptor.imageAsset.data,
            .width = descriptor.imageAsset.width,
            .height = descriptor.imageAsset.height,
            .bytesPerPixel = descriptor.imageAsset.bytesPerPixel,
            .semantic = convert(descriptor.imageAsset.semantic),
            .path = descriptor.imageAsset.path,
            .identifier = descriptor.imageAsset.identifier
        }
    };

    wgpuDDTextureAdd(m_backing.get(), &backingDescriptor);
}

void DDMeshImpl::updateTexture(const DDUpdateTextureDescriptor& descriptor)
{
    WGPUDDUpdateTextureDescriptor backingDescriptor {
        .imageAsset = WGPUDDImageAsset {
            .data = descriptor.imageAsset.data,
            .width = descriptor.imageAsset.width,
            .height = descriptor.imageAsset.height,
            .bytesPerPixel = descriptor.imageAsset.bytesPerPixel,
            .semantic = convert(descriptor.imageAsset.semantic),
            .path = descriptor.imageAsset.path,
            .identifier = descriptor.imageAsset.identifier
        }
    };

    wgpuDDTextureUpdate(m_backing.get(), &backingDescriptor);
}

static WGPUDDConstant convert(const DDModel::DDConstant constant)
{
    switch (constant) {
    case DDModel::DDConstant::kBool:
        return WGPUDDConstant::kBool;
    case DDModel::DDConstant::kUchar:
        return WGPUDDConstant::kUchar;
    case DDModel::DDConstant::kInt:
        return WGPUDDConstant::kInt;
    case DDModel::DDConstant::kUint:
        return WGPUDDConstant::kUint;
    case DDModel::DDConstant::kHalf:
        return WGPUDDConstant::kHalf;
    case DDModel::DDConstant::kFloat:
        return WGPUDDConstant::kFloat;
    case DDModel::DDConstant::kTimecode:
        return WGPUDDConstant::kTimecode;
    case DDModel::DDConstant::kString:
        return WGPUDDConstant::kString;
    case DDModel::DDConstant::kToken:
        return WGPUDDConstant::kToken;
    case DDModel::DDConstant::kAsset:
        return WGPUDDConstant::kAsset;
    case DDModel::DDConstant::kMatrix2f:
        return WGPUDDConstant::kMatrix2f;
    case DDModel::DDConstant::kMatrix3f:
        return WGPUDDConstant::kMatrix3f;
    case DDModel::DDConstant::kMatrix4f:
        return WGPUDDConstant::kMatrix4f;
    case DDModel::DDConstant::kQuatf:
        return WGPUDDConstant::kQuatf;
    case DDModel::DDConstant::kQuath:
        return WGPUDDConstant::kQuath;
    case DDModel::DDConstant::kFloat2:
        return WGPUDDConstant::kFloat2;
    case DDModel::DDConstant::kHalf2:
        return WGPUDDConstant::kHalf2;
    case DDModel::DDConstant::kInt2:
        return WGPUDDConstant::kInt2;
    case DDModel::DDConstant::kFloat3:
        return WGPUDDConstant::kFloat3;
    case DDModel::DDConstant::kHalf3:
        return WGPUDDConstant::kHalf3;
    case DDModel::DDConstant::kInt3:
        return WGPUDDConstant::kInt3;
    case DDModel::DDConstant::kFloat4:
        return WGPUDDConstant::kFloat4;
    case DDModel::DDConstant::kHalf4:
        return WGPUDDConstant::kHalf4;
    case DDModel::DDConstant::kInt4:
        return WGPUDDConstant::kInt4;

    // semantic:
    case DDModel::DDConstant::kPoint3f:
        return WGPUDDConstant::kPoint3f;
    case DDModel::DDConstant::kPoint3h:
        return WGPUDDConstant::kPoint3h;
    case DDModel::DDConstant::kNormal3f:
        return WGPUDDConstant::kNormal3f;
    case DDModel::DDConstant::kNormal3h:
        return WGPUDDConstant::kNormal3h;
    case DDModel::DDConstant::kVector3f:
        return WGPUDDConstant::kVector3f;
    case DDModel::DDConstant::kVector3h:
        return WGPUDDConstant::kVector3h;
    case DDModel::DDConstant::kColor3f:
        return WGPUDDConstant::kColor3f;
    case DDModel::DDConstant::kColor3h:
        return WGPUDDConstant::kColor3h;
    case DDModel::DDConstant::kColor4f:
        return WGPUDDConstant::kColor4f;
    case DDModel::DDConstant::kColor4h:
        return WGPUDDConstant::kColor4h;
    case DDModel::DDConstant::kTexCoord2h:
        return WGPUDDConstant::kTexCoord2h;
    case DDModel::DDConstant::kTexCoord2f:
        return WGPUDDConstant::kTexCoord2f;
    case DDModel::DDConstant::kTexCoord3h:
        return WGPUDDConstant::kTexCoord3h;
    case DDModel::DDConstant::kTexCoord3f:
        return WGPUDDConstant::kTexCoord3f;
    }
}

static WGPUDDConstantContainer convert(const DDModel::DDConstantContainer& container)
{
    return WGPUDDConstantContainer {
        .constant = convert(container.constant),
        .constantValues = container.constantValues,
        .name = container.name
    };
}

static WGPUDDNodeType convert(DDNodeType type)
{
    switch (type) {
    case DDModel::DDNodeType::Builtin:
        return WGPUDDNodeType::Builtin;
    case DDModel::DDNodeType::Constant:
        return WGPUDDNodeType::Constant;
    case DDModel::DDNodeType::Arguments:
        return WGPUDDNodeType::Arguments;
    case DDModel::DDNodeType::Results:
        return WGPUDDNodeType::Results;
    }
}

static WGPUDDNode convert(const DDModel::DDNode& input)
{
    return WGPUDDNode {
        .bridgeNodeType = convert(input.bridgeNodeType),
        .builtin = WGPUDDBuiltin {
            .definition = input.builtin.definition,
            .name = input.builtin.name
        },
        .constant = convert(input.constant)
    };
}
static WGPUDDEdge convert(const DDModel::DDEdge& input)
{
    return WGPUDDEdge {
        .upstreamNodeIndex = input.upstreamNodeIndex,
        .downstreamNodeIndex = input.downstreamNodeIndex,
        .upstreamOutputName = input.upstreamOutputName,
        .downstreamInputName = input.downstreamInputName
    };
}

static WGPUDDDataType convert(DDModel::DDDataType input)
{
    switch (input) {
    case DDModel::DDDataType::kBool:
        return WGPUDDDataType::kBool;
    case DDModel::DDDataType::kInt:
        return WGPUDDDataType::kInt;
    case DDModel::DDDataType::kInt2:
        return WGPUDDDataType::kInt2;
    case DDModel::DDDataType::kInt3:
        return WGPUDDDataType::kInt3;
    case DDModel::DDDataType::kInt4:
        return WGPUDDDataType::kInt4;
    case DDModel::DDDataType::kFloat:
        return WGPUDDDataType::kFloat;
    case DDModel::DDDataType::kColor3f:
        return WGPUDDDataType::kColor3f;
    case DDModel::DDDataType::kColor3h:
        return WGPUDDDataType::kColor3h;
    case DDModel::DDDataType::kColor4f:
        return WGPUDDDataType::kColor4f;
    case DDModel::DDDataType::kColor4h:
        return WGPUDDDataType::kColor4h;
    case DDModel::DDDataType::kFloat2:
        return WGPUDDDataType::kFloat2;
    case DDModel::DDDataType::kFloat3:
        return WGPUDDDataType::kFloat3;
    case DDModel::DDDataType::kFloat4:
        return WGPUDDDataType::kFloat4;
    case DDModel::DDDataType::kHalf:
        return WGPUDDDataType::kHalf;
    case DDModel::DDDataType::kHalf2:
        return WGPUDDDataType::kHalf2;
    case DDModel::DDDataType::kHalf3:
        return WGPUDDDataType::kHalf3;
    case DDModel::DDDataType::kHalf4:
        return WGPUDDDataType::kHalf4;
    case DDModel::DDDataType::kMatrix2f:
        return WGPUDDDataType::kMatrix2f;
    case DDModel::DDDataType::kMatrix3f:
        return WGPUDDDataType::kMatrix3f;
    case DDModel::DDDataType::kMatrix4f:
        return WGPUDDDataType::kMatrix4f;
    case DDModel::DDDataType::kSurfaceShader:
        return WGPUDDDataType::kSurfaceShader;
    case DDModel::DDDataType::kGeometryModifier:
        return WGPUDDDataType::kGeometryModifier;
    case DDModel::DDDataType::kString:
        return WGPUDDDataType::kString;
    case DDModel::DDDataType::kToken:
        return WGPUDDDataType::kToken;
    case DDModel::DDDataType::kAsset:
        return WGPUDDDataType::kAsset;
    }
}

static WGPUDDInputOutput convert(const DDModel::DDInputOutput& input)
{
    return WGPUDDInputOutput {
        .type = convert(input.type),
        .name = input.name
    };
}
static WGPUDDPrimvar convert(const DDModel::DDPrimvar& input)
{
    return WGPUDDPrimvar {
        .name = input.name,
        .referencedGeomPropName = input.referencedGeomPropName,
        .attributeFormat = input.attributeFormat
    };
}

template <typename T, typename U>
Vector<T> convertArray(const Vector<U>& input)
{
    Vector<T> result;
    result.reserveCapacity(input.size());
    for (const auto& i : input)
        result.append(convert(i));

    return result;
}

void DDMeshImpl::addMaterial(const DDModel::DDMaterialDescriptor& descriptor)
{
    WGPUDDMaterialDescriptor backingDescriptor {
        .materialGraph = WGPUDDMaterialGraph {
            .nodes = convertArray<WGPUDDNode>(descriptor.materialGraph.nodes),
            .edges = convertArray<WGPUDDEdge>(descriptor.materialGraph.edges),
            .inputs = convertArray<WGPUDDInputOutput>(descriptor.materialGraph.inputs),
            .outputs = convertArray<WGPUDDInputOutput>(descriptor.materialGraph.outputs),
            .primvars = convertArray<WGPUDDPrimvar>(descriptor.materialGraph.primvars),
            .identifier = descriptor.materialGraph.identifier
        }
    };

    wgpuDDMaterialAdd(m_backing.get(), &backingDescriptor);
}

void DDMeshImpl::updateMaterial(const DDUpdateMaterialDescriptor& descriptor)
{
    WGPUDDUpdateMaterialDescriptor backingDescriptor {
        .materialGraph = WGPUDDMaterialGraph {
            .nodes = convertArray<WGPUDDNode>(descriptor.materialGraph.nodes),
            .edges = convertArray<WGPUDDEdge>(descriptor.materialGraph.edges),
            .inputs = convertArray<WGPUDDInputOutput>(descriptor.materialGraph.inputs),
            .outputs = convertArray<WGPUDDInputOutput>(descriptor.materialGraph.outputs),
            .primvars = convertArray<WGPUDDPrimvar>(descriptor.materialGraph.primvars),
            .identifier = descriptor.materialGraph.identifier
        }
    };

    wgpuDDMaterialUpdate(m_backing.get(), &backingDescriptor);
}

void DDMeshImpl::render()
{
    wgpuDDMeshRender(m_backing.get());
}
#endif

Vector<MachSendRight> DDMeshImpl::ioSurfaceHandles()
{
    return m_renderBuffers.map([](const auto& renderBuffer) {
        return renderBuffer->createSendRight();
    });
}

}

#endif // HAVE(WEBGPU_IMPLEMENTATION)
